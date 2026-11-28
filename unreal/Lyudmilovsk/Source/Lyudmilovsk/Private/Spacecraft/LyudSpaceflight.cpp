#include "Spacecraft/LyudSpacecraft.h"

#include "Spacecraft/FlightModel.h"
#include "Audio/LyudSoundscape.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dev/LyudDev.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Europa/LyudEuropa.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NpcDialogue.h"
#include "Plants/GameCalendar.h"
#include "Sea/LyudSea.h"
#include "Sea/LyudSwim.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sky/Ephemeris.h"
#include "Sky/LyudRareSky.h"
#include "Sky/LyudSky.h"
#include "Sound/SoundBase.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "UI/LyudUIStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "World/LyudLadder.h"
#include "World/LyudPrefetch.h"

#include <string>
#include <vector>

/**
 * The flight's director (docs/spacecraft.md, tools/spacecraft/ENGINE_NOTES.md). Everything about the flight is decided
 * by LyudFlight::FFlight (Private/Spacecraft/FlightModel.h); this file feeds it four facts and a dt and draws, plays
 * and moves what it says. The cabin's own look is ALyudSpacecraft (LyudSpacecraftCabin.cpp).
 */

namespace LyudFlightImpl
{
	TAutoConsoleVariable<int32> CVarFlight(TEXT("lyud.Flight"), 1, TEXT("The craft on Europa and the flight home (0: not spawned; read at play start)."));
	TAutoConsoleVariable<float> CVarFlightFast(TEXT("lyud.Flight.Fast"), 1.f, TEXT("The flight model runs this many times faster (tests; 20 = the flight in 45 s)."));
	TAutoConsoleVariable<float> CVarFlightShake(TEXT("lyud.Flight.Shake"), 1.f, TEXT("Scales the view's shake in the cabin (0: none)."));
	TAutoConsoleVariable<float> CVarFlightDetail(TEXT("lyud.Flight.Detail"), 1.f,
		TEXT("0..1: the planet's fine layers through the porthole (billows, wind streaks, glitter) and its air shell (0: off)."), ECVF_Scalability);

	const TCHAR* EuropaMeshes = TEXT("/Game/Europa/Meshes/");
	const TCHAR* CraftMeshes = TEXT("/Game/Spacecraft/Meshes/");
	const TCHAR* CraftFolder = TEXT("/Game/Spacecraft");
	const TCHAR* SoundFolder = TEXT("/Game/Audio/Spacecraft");
	/** M_SC_PlanetNear draws its exact sphere this much inside the mesh (unreal/scripts/import_spacecraft.py PLANET_FIT). */
	constexpr float PlanetFit = 0.9985f;

	/** Every pool of tools/spacecraft/SOUND_NOTES.md (loaded with the craft, let go after the flight). */
	const TCHAR* const Pools[] = {
		TEXT("sc_cabin_hum"), TEXT("sc_fan"), TEXT("sc_gyro_whine"), TEXT("sc_gyro_spinup"), TEXT("sc_air_hiss"), TEXT("sc_radio_static"),
		TEXT("sc_beacon"), TEXT("sc_toggle"), TEXT("sc_cover"), TEXT("sc_button"), TEXT("sc_relay"), TEXT("sc_buzzer"), TEXT("sc_chime"),
		TEXT("sc_lamp_tick"), TEXT("sc_hatch_close"), TEXT("sc_hatch_open"), TEXT("sc_wheel"), TEXT("sc_belt"), TEXT("sc_seat"),
		TEXT("sc_ignition"), TEXT("sc_engine"), TEXT("sc_cutoff"), TEXT("sc_thruster"), TEXT("sc_sep_bang"), TEXT("sc_creak"),
		TEXT("sc_rattle"), TEXT("sc_entry_roar"), TEXT("sc_entry_roar_hi"), TEXT("sc_plasma_crackle"), TEXT("sc_wind"), TEXT("sc_drogue"),
		TEXT("sc_chute_open"), TEXT("sc_chute_creak"), TEXT("sc_jets"), TEXT("sc_splash"), TEXT("sc_afloat"), TEXT("sc_drip"),
		TEXT("sc_needle_down"), TEXT("sc_needle_up"), TEXT("sc_needle_skip"), TEXT("sc_vinyl_crackle"), TEXT("sc_platter_start"),
		TEXT("sc_runout"), TEXT("sc_thermos_open"), TEXT("sc_sip"), TEXT("sc_page"),
	};

	/** The loops: one attached audio component each while it is heard. */
	enum ELoop
	{
		LoopHum, LoopHiss, LoopGyro, LoopStatic, LoopBeacon, LoopEngine, LoopRattle, LoopRoar, LoopRoarHi, LoopWind, LoopChuteCreak,
		LoopAfloat, LoopCrackle, LoopRunout, LoopCount
	};
	struct FLoopDef
	{
		const TCHAR* Pool;
		/** ALyudSpacecraft::ActionPoint id the loop sits at. */
		const TCHAR* Point;
		/** Volume per second up and down. */
		float Attack, Release;
	};
	const FLoopDef LoopDefs[LoopCount] = {
		{ TEXT("sc_cabin_hum"), TEXT("panel"), 0.25f, 0.5f },
		{ TEXT("sc_air_hiss"), TEXT("fan_0"), 0.3f, 0.5f },
		{ TEXT("sc_gyro_whine"), TEXT("panel"), 0.6f, 0.08f },
		{ TEXT("sc_radio_static"), TEXT("speaker"), 0.4f, 0.6f },
		{ TEXT("sc_beacon"), TEXT("speaker"), 0.3f, 0.6f },
		{ TEXT("sc_engine"), TEXT("jets"), 1.2f, 11.f },
		{ TEXT("sc_rattle"), TEXT("panel"), 1.5f, 1.6f },
		{ TEXT("sc_entry_roar"), TEXT("hull_0"), 0.8f, 0.8f },
		{ TEXT("sc_entry_roar_hi"), TEXT("port_big"), 0.8f, 0.8f },
		{ TEXT("sc_wind"), TEXT("hull_1"), 1.2f, 0.2f },
		{ TEXT("sc_chute_creak"), TEXT("hull_2"), 0.3f, 0.6f },
		{ TEXT("sc_afloat"), TEXT("hull_3"), 0.33f, 0.5f },
		{ TEXT("sc_vinyl_crackle"), TEXT("player"), 2.f, 3.f },
		{ TEXT("sc_runout"), TEXT("player"), 2.f, 4.f },
	};

	FString ScStr(const std::string& In) { return FString(UTF8_TO_TCHAR(In.c_str())); }
	std::string ScStd(const FString& In) { return std::string(TCHAR_TO_UTF8(*In)); }
	/** A vector of the model (right-handed: X forward, Y left, Z up) in Unreal's axes. */
	FVector ScUe(const LyudFlight::FVec& V) { return FVector(V.X, -V.Y, V.Z); }
	LyudFlight::FVec ScModel(const FVector& V) { return LyudFlight::V3(V.X, -V.Y, V.Z); }

	TSharedPtr<FJsonObject> ScReadJson(const FString& Rel)
	{
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / Rel)))
		{
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root);
		}
		return Root;
	}

	FVector ScVec(const TSharedPtr<FJsonObject>& O, const FString& Field, const FVector& Default)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O && O->TryGetArrayField(Field, A) && A->Num() >= 3)
		{
			return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
		}
		return Default;
	}

	double ScNum(const TSharedPtr<FJsonObject>& O, const FString& Field, double Default)
	{
		double V = Default;
		if (O)
		{
			O->TryGetNumberField(Field, V);
		}
		return V;
	}

	TSharedPtr<FJsonObject> ScObj(const TSharedPtr<FJsonObject>& O, const FString& Field)
	{
		const TSharedPtr<FJsonObject>* Out = nullptr;
		if (O && O->TryGetObjectField(Field, Out))
		{
			return *Out;
		}
		return nullptr;
	}

	UStaticMesh* ScMesh(const TCHAR* Folder, const TCHAR* Name)
	{
		const FString Path = FString(Folder) + Name + TEXT(".") + Name;
		UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path);
		if (!M)
		{
			UE_LOG(LogTemp, Warning, TEXT("Spacecraft: missing %s (unreal/scripts/import_spacecraft.sh)"), *Path);
		}
		return M;
	}

	UStaticMeshComponent* ScMakeMesh(AActor* Owner, const TCHAR* Name, USceneComponent* Parent)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner, Name);
		C->SetupAttachment(Parent);
		C->SetMobility(EComponentMobility::Movable);
		C->RegisterComponent();
		return C;
	}

	/** Moves every frame: out of the distance field and the dynamic GI, no shadow. */
	void ScQuiet(UPrimitiveComponent* C)
	{
		C->bAffectDistanceFieldLighting = false;
		C->bAffectDynamicIndirectLighting = false;
		C->SetCastShadow(false);
	}

	float ScSmooth01(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	float ScSmoothStep(float A, float B, float X)
	{
		return ScSmooth01((X - A) / FMath::Max(B - A, 1e-6f));
	}

	ACharacter* ScPlayerHero(const UWorld* World, APlayerController** OutPC = nullptr)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (OutPC)
		{
			*OutPC = PC;
		}
		return PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	}

	void ScToast(const UObject* Ctx, const FString& Key)
	{
		if (Key.IsEmpty())
		{
			return;
		}
		if (ULyudGameScreen* Screen = ULyudGameScreen::Get(Ctx))
		{
			Screen->Toast(LyudText::Get(Key));
		}
	}

	ALyudSky* ScFindSky(UWorld* World)
	{
		TActorIterator<ALyudSky> It(World);
		return It ? *It : nullptr;
	}

	ULyudSpaceflightSubsystem* ScFlightOf(UWorld* World)
	{
		return World ? World->GetSubsystem<ULyudSpaceflightSubsystem>() : nullptr;
	}

	/** One record of the craft's rack (Content/Spacecraft/records.json, tools/spacecraft/records_strings.py). */
	struct FRec
	{
		FString Id;
		FString Short;
		int32 Cell = 0;
		double Seconds = 240.0;
		TArray<FString> Fallback;
	};

	/** A one-shot waiting for its moment (a relay after its switch, the sip after the thermos). */
	struct FLater
	{
		FName Pool;
		FName Point;
		float In = 0.f;
		float Volume = 1.f;
	};

	/** The hero going through the hatch: the feet from one cabin-frame point to another. */
	struct FMove
	{
		bool bOn = false;
		FVector FromLocal = FVector::ZeroVector;
		FVector ToLocal = FVector::ZeroVector;
		float T = 0.f;
		float Seconds = 1.f;
		/** 0 into the cabin, 1 out onto the stage's platform, 2 out into the sea. */
		int32 Then = 0;
	};
}

using namespace LyudFlightImpl;

// ------------------------------------------------------------------ the director's runtime state

struct ULyudSpaceflightSubsystem::FImpl
{
	LyudFlight::FFlight Flight;
	LyudFlight::FState St;

	// ---- spacecraft.json
	float StageOffsetCm = 6000.f;
	float RealAscentAltCm = 3.0e5f;
	float BlendSeconds = 12.f;
	float PlanetDrawCm = 4.5e6f;
	float EuropaDrawCm = 4.0e6f;
	float StarsRadiusCm = 1.0e7f;
	float PlanetRadiusKm = 2000.f;
	float AirRadius = 1.045f;
	double IslandLatDeg = 55.0148, IslandLonDeg = 20.40735, IslandExtentKm = 25.6;
	double SplashEastKm = 0.0, SplashNorthKm = 0.0;
	float MinDepthCm = 300.f, NudgeCm = 2000.f;
	float BobCm = 6.f, RollDeg = 3.5f, RollPeriod = 5.5f, DriftCmS = 4.f;

	// ---- the stage on Europa
	bool bPadLevelled = false;
	FTransform PadSeat = FTransform::Identity;
	/** The flight second at which the real climb over the real ice reaches RealAscentAltCm (found once from the profile). */
	double SwapT = 20.0;
	bool bSiteHiddenByFlight = false;
	bool bSitePlanetHidden = false;
	bool bObserverMine = false;

	// ---- loading
	bool bBuilt = false;
	bool bPoolsLoaded = false;
	bool bPendingRestore = false;
	bool bWantStand = false;

	// ---- the sky round the cabin
	FQuat StarBase = FQuat::Identity;
	FQuat Fix = FQuat::Identity;
	FVector SunWorld = FVector::UpVector;
	FVector PlanetWorld = FVector::UpVector;
	FVector LastPlanetSun = FVector::ZeroVector;
	float SkyTimer = 0.f;
	FLinearColor SunLight = FLinearColor(12.7f, 10.5f, 8.6f);
	float SkyGain = 2.2f;
	FVector PlanetSunRH = FVector(0.3, 0.2, 0.9);
	bool bRigShown = false;
	TWeakObjectPtr<ALyudSky> Sky;

	// ---- the hero
	FMove Move;
	FName Look;                     // big, left, right or none
	float LookTurn = 0.f;           // seconds the view is still being turned to the porthole
	FVector EyeLocal = FVector::ZeroVector;
	bool bEyeInit = false;
	float SavedGravity = -1.f;
	bool bWeightlessWalk = false;
	int32 BookPage = -1;
	float NapFade = 0.f;
	FBox DryBox = FBox(ForceInit);
	float DryTimer = 0.f;
	bool bHold = false;

	// ---- afloat
	FVector AfloatAt = FVector::ZeroVector;
	float AfloatYaw = 0.f;
	float AfloatZ = 0.f;
	bool bAfloatInit = false;
	double AfloatClock = 0.0;
	float Wet = 0.f;
	bool bChuteShown = false;

	// ---- sounds
	float LoopVol[LoopCount] = {};
	float LoopSet[LoopCount] = {};
	float LoopPitchSet[LoopCount] = {};
	TArray<FLater> Later;
	float GyroSpin = 0.f;
	float EngineHold = 0.f;
	float Duck = 1.f;
	float CreakIn = 3.f, PuffIn = 2.f, DripIn = 3.f;
	float DripFor = 0.f;
	double ShakeClock = 0.0;

	// ---- the record player
	TArray<FRec> Recs;
	FString HouseCue = TEXT("europa_1");
	int32 MusicIndex = -1;
	float MusicIn = -1.f;
	bool bMusicOn = false;
	bool bRunout = false;
	float ArmShown = -1.f;
	int32 CellShown = -2;
	bool bSpinShown = false;

	// ---- the subtitle
	FString SubtitleKey;
	float SubtitleFor = 0.f;
	float SubtitleAlpha = 0.f;
	TSharedPtr<SLyudFlightSubtitle> SubtitleWidget;

	// ---- what was shown (set only on change)
	int32 ShownInside = -1, ShownOutside = -1, ShownStage = -1, ShownCabinLight = -1;
	bool bWasRealWorld = false;
	LyudFlight::EPhase LastPhase = LyudFlight::EPhase::Parked;

	// ---- more of the same
	/** Turns the model's planet on the pad (35 degrees up, dead ahead) onto the real one of the site's sky; eased off
	 *  with the climb. */
	FQuat PlanetOff = FQuat::Identity;
	float SeawardYaw = 303.6f;
	float PadTimer = 0.f;
	float EyeAlpha = 0.f;
	float CrackleIn = 0.5f;
	float LastJets = 0.f;
	float SkyDim = 1.f;
	long long DayFed = -1;
	float LastIslandSize = -1.f, LastHaze = -1.f, LastDetail = -1.f, LastGain = -1.f;
	FVector LastStarSun = FVector::ZeroVector;
	int32 PanelLit = -1;
	int32 FireLit = -1;
	float HeroFarCm = 4000.f;
	float StandGravity = 0.f;
	bool bNapFadeOn = false;
	uint32 LampMask = 0;
	bool bLampsKnown = false;
	double MusicPos = 0.0;

	void LoadData(ULyudSpaceflightSubsystem& O);
	void GiveRecords();
	bool EnsureBuilt(ULyudSpaceflightSubsystem& O, bool bForce);
	void LevelPad(ULyudSpaceflightSubsystem& O);
	void PlaceOnStage(ULyudSpaceflightSubsystem& O);
	void SetActive(ULyudSpaceflightSubsystem& O, bool bOn);
	void Show(ULyudSpaceflightSubsystem& O, bool bInside, bool bOutside, bool bStage);

	// the hero
	FVector FeetOf(const ACharacter& Hero) const;
	void SetFeet(ACharacter& Hero, const FVector& Feet) const;
	void SetAboard(ULyudSpaceflightSubsystem& O, bool bOn);
	void Sit(ULyudSpaceflightSubsystem& O, bool bSnap);
	void Stand(ULyudSpaceflightSubsystem& O);
	void StartMove(ULyudSpaceflightSubsystem& O, const FVector& ToWorld, float Seconds, int32 Then);
	void TickMove(ULyudSpaceflightSubsystem& O, ACharacter& Hero, float Dt);
	void IntoTheSea(ULyudSpaceflightSubsystem& O, ACharacter& Hero);
	void TickHero(ULyudSpaceflightSubsystem& O, ACharacter& Hero, APlayerController& PC, float Dt);
	void TickCamera(ULyudSpaceflightSubsystem& O, APlayerController& PC, float Dt);
	void UpdateAction(ULyudSpaceflightSubsystem& O, ACharacter& Hero, APlayerController& PC);
	void DoAction(ULyudSpaceflightSubsystem& O, const FString& Id);
	void SetHatch(ULyudSpaceflightSubsystem& O, bool bOpen, bool bSnap);
	void SetWheel(ULyudSpaceflightSubsystem& O, bool bSealed, bool bSnap);

	// the flight
	void Feed(ULyudSpaceflightSubsystem& O, const ACharacter* Hero);
	void OnLaunch(ULyudSpaceflightSubsystem& O);
	void PlaceCabin(ULyudSpaceflightSubsystem& O, float Dt);
	void EnterRealWorld(ULyudSpaceflightSubsystem& O);
	void CraftBack(ULyudSpaceflightSubsystem& O);
	void Restore(ULyudSpaceflightSubsystem& O);
	void TickSky(ULyudSpaceflightSubsystem& O, float Dt);
	void ShowRig(ULyudSpaceflightSubsystem& O, bool bShow);
	void TakeObserver(ULyudSpaceflightSubsystem& O, bool bMine);
	FVector DirWorld(const ULyudSpaceflightSubsystem& O, const FVector& RightHanded) const;
	void TickExterior(ULyudSpaceflightSubsystem& O, float Dt);

	// sounds, words
	void Snd(ULyudSpaceflightSubsystem& O, const TCHAR* Pool, const TCHAR* Point, float Volume = 1.f, float In = 0.f);
	void HandleEvent(ULyudSpaceflightSubsystem& O, const LyudFlight::FEvent& E);
	void TickSounds(ULyudSpaceflightSubsystem& O, float Dt);
	void StopSounds(ULyudSpaceflightSubsystem& O, float Fade);
	void TickRecord(ULyudSpaceflightSubsystem& O, float Dt);
	void StartMusic(ULyudSpaceflightSubsystem& O, bool bToast);
	void StopMusic(ULyudSpaceflightSubsystem& O);
	void Say(ULyudSpaceflightSubsystem& O, const FString& SubtitleKeyIn, const FString& VoiceKey, const FVector& Where, float Volume, bool bSubtitle);
	void ShowSubtitle(ULyudSpaceflightSubsystem& O, bool bShow);
};

// ------------------------------------------------------------------ the subtitle (the hero's lines, the automatic voice, the book)

class SLyudFlightSubtitle : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudFlightSubtitle) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ULyudSpaceflightSubsystem>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		using namespace LyudUI;
		TWeakObjectPtr<ULyudSpaceflightSubsystem> W = InArgs._Owner;
		SetVisibility(EVisibility::HitTestInvisible);
		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0, 0, 0, 132))
			[
				SNew(SBox).WidthOverride(760.f)
				[
					SNew(STextBlock).Font(Font(18)).Justification(ETextJustify::Center).AutoWrapText(true)
					.ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.7f))
					.Text_Lambda([W]() { return W.IsValid() && W->Impl && !W->Impl->SubtitleKey.IsEmpty() ? LyudText::Get(W->Impl->SubtitleKey) : FText::GetEmpty(); })
					.ColorAndOpacity_Lambda([W]() { return FSlateColor(Bright.CopyWithNewOpacity(W.IsValid() && W->Impl ? W->Impl->SubtitleAlpha : 0.f)); })
				]
			]
		];
	}
};

// ------------------------------------------------------------------ the view in the cabin

bool ULyudFlightCamera::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);
	if (!bOn)
	{
		return false;
	}
	InOutPOV.Location = FMath::Lerp(InOutPOV.Location, EyeAt, FMath::Clamp(EyeAlpha, 0.f, 1.f)) + Offset;
	InOutPOV.Rotation.Pitch += Shake.Pitch;
	InOutPOV.Rotation.Yaw += Shake.Yaw;
	InOutPOV.Rotation.Roll += Shake.Roll;
	InOutPOV.FOV += FovAdd;
	return false;
}

// ------------------------------------------------------------------ the landing stage

ALyudSpacecraftStage::ALyudSpacecraftStage()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Ladder = CreateDefaultSubobject<ULyudLadderComponent>(TEXT("Ladder"));
	Ladder->SetupAttachment(RootComponent);
}

void ALyudSpacecraftStage::Build()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	const TSharedPtr<FJsonObject> Kit = ScReadJson(TEXT("Spacecraft/kit.json"));
	const TSharedPtr<FJsonObject> J = ScObj(Kit, TEXT("stage"));
	if (!J)
	{
		UE_LOG(LogTemp, Warning, TEXT("Spacecraft: no stage in Content/Spacecraft/kit.json, using built-in measures"));
	}
	CabinSeatLocal = ScVec(J, TEXT("cabin_seat"), CabinSeatLocal);
	Mesh = ScMakeMesh(this, TEXT("Stage"), GetRootComponent());
	Mesh->SetStaticMesh(ScMesh(CraftMeshes, TEXT("SM_SC_Stage")));
	// stands still on the ice in the star's light (channel 1) and the planet's (channel 2), as Europa's own things
	Mesh->SetLightingChannels(false, true, true);
	// the ladder up to the hatch's platform: the player's own to climb (ULyudLadderSubsystem: E takes hold, W/S, Space)
	const TSharedPtr<FJsonObject> L = ScObj(J, TEXT("ladder"));
	const FVector At = ScVec(L, TEXT("at"), FVector(-205.f, 68.f, 0.f));
	Ladder->SetRelativeLocationAndRotation(At, FRotator(0.f, float(ScNum(L, TEXT("yaw"), 90.0)), 0.f));
	Ladder->RungSpacing = float(ScNum(L, TEXT("rung_spacing"), 30.0));
	Ladder->FirstRungZ = float(ScNum(L, TEXT("first_rung_z"), 25.0));
	Ladder->RungCount = FMath::Max(1, int32(ScNum(L, TEXT("rung_count"), 9.0)));
	Ladder->HalfWidth = float(ScNum(L, TEXT("half_width"), 22.5));
	Ladder->RungRadius = float(ScNum(L, TEXT("rung_radius"), 1.6));
	Ladder->TopExit = ScVec(L, TEXT("top_exit"), FVector(-45.f, 0.f, 295.f));
	Ladder->GripVolume = 0.3f;
	Ladder->bInteractive = true;
	FootLocal = At + FRotator(0.f, float(ScNum(L, TEXT("yaw"), 90.0)), 0.f).RotateVector(FVector(60.f, 0.f, 0.f));
	TopLocal = ScVec(L, TEXT("top_exit_stage"), TopLocal);
}

void ALyudSpacecraftStage::SetShown(bool bInShown)
{
	if (bShown == bInShown)
	{
		return;
	}
	bShown = bInShown;
	SetActorHiddenInGame(!bShown);
	SetActorEnableCollision(bShown);
}

FTransform ALyudSpacecraftStage::CabinSeat() const
{
	return FTransform(GetActorQuat(), GetActorTransform().TransformPosition(CabinSeatLocal));
}

FVector ALyudSpacecraftStage::LadderFoot() const
{
	return GetActorTransform().TransformPosition(FootLocal);
}

FVector ALyudSpacecraftStage::LadderTop() const
{
	return GetActorTransform().TransformPosition(TopLocal);
}

// ------------------------------------------------------------------ data, loading, the stage on the ice

namespace
{
	/** The flight's spoken lines load in the background with the craft (LyudText::LoadVoice then finds them in memory). */
	void WarmVoices(const UObject* Ctx)
	{
		static const TCHAR* const Hero[] = { TEXT("liftoff"), TEXT("weightless"), TEXT("europa_behind"), TEXT("planet_grows"), TEXT("ocean"),
			TEXT("island"), TEXT("fire"), TEXT("chute"), TEXT("home"), TEXT("afloat") };
		static const TCHAR* const Auto[] = { TEXT("ready"), TEXT("launch"), TEXT("cutoff"), TEXT("weightless"), TEXT("burn"), TEXT("half"),
			TEXT("approach"), TEXT("sep"), TEXT("entry"), TEXT("blackout"), TEXT("chute"), TEXT("splash"), TEXT("home") };
		for (const TCHAR* Id : Hero)
		{
			LyudText::WarmVoice(Ctx, FString::Printf(TEXT("line.hero.flight_%s.1"), Id));
		}
		for (const TCHAR* Id : Auto)
		{
			LyudText::WarmVoice(Ctx, FString::Printf(TEXT("line.flightauto.%s.1"), Id));
		}
	}
}

void ULyudSpaceflightSubsystem::FImpl::LoadData(ULyudSpaceflightSubsystem& O)
{
	const TSharedPtr<FJsonObject> J = ScReadJson(TEXT("Spacecraft/spacecraft.json"));
	if (!J)
	{
		UE_LOG(LogTemp, Warning, TEXT("Spaceflight: no Content/Spacecraft/spacecraft.json (tools/spacecraft/site.py), using built-in numbers"));
	}
	O.SplashAt = ScVec(J, TEXT("splash_cm"), FVector(64000.0, 10790.0, -4300.0));
	O.SplashHeadingDeg = float(ScNum(J, TEXT("splash_yaw_deg"), 125.0));
	// out to sea: straight away from the town (compass bearings: yaw = bearing + 90 in this world, as splash_yaw_deg)
	SeawardYaw = float(ScNum(J, TEXT("town_bearing_deg"), 33.6)) + 90.f + 180.f;
	MinDepthCm = float(ScNum(J, TEXT("min_depth_play_m"), 3.0)) * 100.f;
	NudgeCm = float(ScNum(J, TEXT("nudge_step_m"), 20.0)) * 100.f;
	const TSharedPtr<FJsonObject> StageJ = ScObj(J, TEXT("stage"));
	StageOffsetCm = float(ScNum(StageJ, TEXT("offset_m"), 60.0)) * 100.f;
	const TSharedPtr<FJsonObject> SpotJ = ScObj(J, TEXT("flight_spot"));
	RealAscentAltCm = float(ScNum(SpotJ, TEXT("real_ascent_alt_cm"), 3.0e5));
	BlendSeconds = FMath::Max(1.f, float(ScNum(SpotJ, TEXT("blend_seconds"), 12.0)));
	const TSharedPtr<FJsonObject> DrawJ = ScObj(J, TEXT("draw"));
	PlanetDrawCm = float(ScNum(DrawJ, TEXT("planet_cm"), 4.5e6));
	EuropaDrawCm = float(ScNum(DrawJ, TEXT("europa_cm"), 4.0e6));
	StarsRadiusCm = float(ScNum(DrawJ, TEXT("stars_radius_cm"), 1.0e7));
	PlanetRadiusKm = float(ScNum(DrawJ, TEXT("planet_radius_km"), 2000.0));
	AirRadius = float(ScNum(DrawJ, TEXT("air_radius"), 1.045));
	const TSharedPtr<FJsonObject> IslandJ = ScObj(J, TEXT("island"));
	IslandLatDeg = ScNum(IslandJ, TEXT("centre_lat_deg"), IslandLatDeg);
	IslandLonDeg = ScNum(IslandJ, TEXT("centre_lon_deg"), IslandLonDeg);
	IslandExtentKm = ScNum(IslandJ, TEXT("extent_km"), IslandExtentKm);
	SplashEastKm = ScNum(IslandJ, TEXT("splash_east_km"), 0.0);
	SplashNorthKm = ScNum(IslandJ, TEXT("splash_north_km"), 0.0);
	const TSharedPtr<FJsonObject> AfloatJ = ScObj(J, TEXT("afloat"));
	BobCm = float(ScNum(AfloatJ, TEXT("bob_cm"), 6.0));
	RollDeg = float(ScNum(AfloatJ, TEXT("roll_deg"), 3.5));
	RollPeriod = FMath::Max(1.f, float(ScNum(AfloatJ, TEXT("roll_period_s"), 5.5)));
	DriftCmS = float(ScNum(AfloatJ, TEXT("drift_cm_s"), 4.0));
	HeroFarCm = float(ScNum(AfloatJ, TEXT("hero_far_cm"), 4000.0));
	GiveRecords();
	// the second of the climb at which the real ice is left behind for the impostors (the model's own height)
	SwapT = 20.0;
	for (double T = 0.0; T < Flight.P.C.TAscentEnd; T += 0.25)
	{
		LyudFlight::FState Probe;
		Flight.P.Eval(T, Flight.SunF, Probe);
		if (Probe.AltEuropaKm * 1.0e5 >= double(RealAscentAltCm))
		{
			SwapT = T;
			break;
		}
	}
}

void ULyudSpaceflightSubsystem::FImpl::GiveRecords()
{
	Recs.Reset();
	const TSharedPtr<FJsonObject> J = ScReadJson(TEXT("Spacecraft/records.json"));
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (J)
	{
		J->TryGetStringField(TEXT("house_cue"), HouseCue);
	}
	if (J && J->TryGetArrayField(TEXT("records"), List))
	{
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> R = V->AsObject();
			if (!R)
			{
				continue;
			}
			FRec Rec;
			R->TryGetStringField(TEXT("id"), Rec.Id);
			R->TryGetStringField(TEXT("short"), Rec.Short);
			Rec.Cell = int32(ScNum(R, TEXT("cell"), double(Recs.Num())));
			Rec.Seconds = ScNum(R, TEXT("seconds"), 240.0);
			const TArray<TSharedPtr<FJsonValue>>* Cues = nullptr;
			if (R->TryGetArrayField(TEXT("fallback"), Cues))
			{
				for (const TSharedPtr<FJsonValue>& C : *Cues)
				{
					Rec.Fallback.Add(C->AsString());
				}
			}
			if (!Rec.Id.IsEmpty())
			{
				Recs.Add(MoveTemp(Rec));
			}
		}
	}
	if (Recs.Num() == 0)
	{
		// the record that is always on board
		FRec House;
		House.Id = TEXT("record_sc_house");
		House.Short = TEXT("house");
		House.Seconds = 214.0;
		Recs.Add(MoveTemp(House));
		UE_LOG(LogTemp, Warning, TEXT("Spaceflight: no Content/Spacecraft/records.json (tools/spacecraft/records_strings.py): only the house record"));
	}
	std::vector<LyudFlight::FRecord> Rack;
	Rack.reserve(size_t(Recs.Num()));
	for (const FRec& R : Recs)
	{
		LyudFlight::FRecord M;
		M.Id = ScStd(R.Id);
		M.LengthSec = R.Seconds;
		Rack.push_back(M);
	}
	Flight.SetRecords(Rack);
}

bool ULyudSpaceflightSubsystem::FImpl::EnsureBuilt(ULyudSpaceflightSubsystem& O, bool bForce)
{
	if (bBuilt)
	{
		return true;
	}
	if (!O.Cabin || !O.Stage || !O.SkyRig)
	{
		return false;
	}
	if (!bForce)
	{
		// both asked on every call: the meshes and the sounds load side by side, in the background
		const bool bMeshes = ULyudPrefetch::FolderReady(&O, CraftFolder);
		const bool bSounds = ULyudPrefetch::FolderReady(&O, SoundFolder);
		if (!bMeshes || !bSounds)
		{
			return false;
		}
	}
	bBuilt = true;
	O.Stage->Build();
	O.Cabin->Build();
	ShownInside = ShownOutside = ShownStage = ShownCabinLight = -1;
	CellShown = -2;
	ArmShown = -1.f;
	// what the hands left as it was
	O.Cabin->PlayActionAnim(TEXT("hatch"), O.bHatchOpen, true);
	O.Cabin->PlayActionAnim(TEXT("wheel"), O.bWheelSealed, true);
	O.Cabin->PlayActionAnim(TEXT("cover"), O.bCoverUp, true);
	O.Cabin->PlayActionAnim(TEXT("chute_cover"), O.bChuteCoverUp, true);
	O.Cabin->PlayActionAnim(TEXT("buckle"), O.bBuckled, true);

	if (ULyudSoundscapeSubsystem* SS = ULyudSoundscapeSubsystem::Get(&O))
	{
		for (const TCHAR* Pool : Pools)
		{
			SS->LoadPool(TEXT("Spacecraft"), FName(Pool));
		}
		bPoolsLoaded = true;
	}
	WarmVoices(&O);
	if (O.Loops.Num() != LoopCount)
	{
		O.Loops.SetNum(LoopCount);
	}

	// the sky round the cabin (made once: the spheres are Europa's own 1 m ball)
	if (!O.PlanetMesh)
	{
		USceneComponent* Root = O.SkyRig->GetRootComponent();
		UStaticMesh* Ball = ScMesh(EuropaMeshes, TEXT("SM_EU_Planet"));
		auto Sphere = [&O, Root, Ball](const TCHAR* Name)
		{
			UStaticMeshComponent* C = ScMakeMesh(O.SkyRig.Get(), Name, Root);
			C->SetStaticMesh(Ball);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ScQuiet(C);
			C->bNeverDistanceCull = true;
			C->SetVisibility(false);
			return C;
		};
		O.PlanetMesh = Sphere(TEXT("FlightPlanet"));
		O.AirMesh = Sphere(TEXT("FlightPlanetAir"));
		O.EuropaMesh = Sphere(TEXT("FlightEuropa"));
		O.StarMesh = Sphere(TEXT("FlightStars"));
		if (UMaterialInterface* Near = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Spacecraft/Materials/M_SC_PlanetNear.M_SC_PlanetNear")))
		{
			O.PlanetMID = UMaterialInstanceDynamic::Create(Near, &O);
			O.PlanetMesh->SetMaterial(0, O.PlanetMID);
			O.PlanetMID->SetScalarParameterValue(TEXT("PlanetRadiusKm"), PlanetRadiusKm);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Spacecraft: missing /Game/Spacecraft/Materials/M_SC_PlanetNear (unreal/scripts/import_spacecraft.sh): the planet keeps the ball's own material"));
		}
		if (UMaterialInterface* Air = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Spacecraft/Materials/M_SC_PlanetAir.M_SC_PlanetAir")))
		{
			O.AirMID = UMaterialInstanceDynamic::Create(Air, &O);
			O.AirMesh->SetMaterial(0, O.AirMID);
			O.AirMID->SetVectorParameterValue(TEXT("Air"), FLinearColor(AirRadius, 1.f, 0.f, 0.f));
		}
		if (UMaterialInterface* Ice = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Europa/Materials/MI_EU_ice_crust.MI_EU_ice_crust")))
		{
			// Europa astern: its own ice, lit by the star (channel 1 while the sky is in its space view)
			O.EuropaMesh->SetMaterial(0, Ice);
		}
		O.EuropaMesh->SetLightingChannels(false, true, false);
		if (UMaterialInterface* Space = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Europa/Materials/M_EU_Space.M_EU_Space")))
		{
			O.StarMID = UMaterialInstanceDynamic::Create(Space, &O);
			O.StarMesh->SetMaterial(0, O.StarMID);
		}
		O.StarMesh->SetWorldScale3D(FVector(StarsRadiusCm / 100.f));
	}
	if (!O.FireLight)
	{
		auto Lamp = [&O](const TCHAR* Name, const TCHAR* Point, float Radius)
		{
			UPointLightComponent* L = NewObject<UPointLightComponent>(O.Cabin.Get(), Name);
			L->SetupAttachment(O.Cabin->GetRootComponent());
			L->SetMobility(EComponentMobility::Movable);
			L->SetIntensityUnits(ELightUnits::Candelas);
			L->SetIntensity(0.f);
			L->SetAttenuationRadius(Radius);
			L->SetCastShadows(false);
			L->RegisterComponent();
			L->SetWorldLocation(O.Cabin->ActionPoint(FName(Point)));
			L->SetVisibility(false);
			return L;
		};
		O.FireLight = Lamp(TEXT("FireLight"), TEXT("port_big"), 420.f);
		O.PanelLight = Lamp(TEXT("PanelLight"), TEXT("panel"), 160.f);
		O.PanelLight->SetLightColor(FLinearColor(0.75f, 1.f, 0.8f));
	}
	else
	{
		// the cabin was built again: its points may have moved with a new kit
		O.FireLight->SetWorldLocation(O.Cabin->ActionPoint(TEXT("port_big")));
		O.PanelLight->SetWorldLocation(O.Cabin->ActionPoint(TEXT("panel")));
	}
	PanelLit = FireLit = -1;
	UE_LOG(LogTemp, Display, TEXT("FLIGHT built (%s)"), bForce ? TEXT("at once") : TEXT("prefetched"));
	return true;
}

void ULyudSpaceflightSubsystem::FImpl::LevelPad(ULyudSpaceflightSubsystem& O)
{
	const ULyudEuropaSubsystem* Europa = ULyudEuropaSubsystem::Get(&O);
	ALyudEuropaSite* Site = Europa ? Europa->GetSite() : nullptr;
	UWorld* World = O.GetWorld();
	if (bPadLevelled || !Site || !Site->HasGround() || !World || !O.Stage)
	{
		return;
	}
	// the four foot pads on the ice as the game has it: the stage stands level on their mean (its legs take up the rest)
	const TSharedPtr<FJsonObject> StageJ = ScObj(ScReadJson(TEXT("Spacecraft/kit.json")), TEXT("stage"));
	const TArray<TSharedPtr<FJsonValue>>* Pads = nullptr;
	TArray<FVector, TInlineAllocator<4>> Feet;
	if (StageJ && StageJ->TryGetArrayField(TEXT("pads"), Pads))
	{
		for (const TSharedPtr<FJsonValue>& P : *Pads)
		{
			const TArray<TSharedPtr<FJsonValue>>& A = P->AsArray();
			if (A.Num() >= 2)
			{
				Feet.Add(FVector(A[0]->AsNumber(), A[1]->AsNumber(), 0.0));
			}
		}
	}
	if (Feet.Num() == 0)
	{
		Feet = { FVector(215.67, -215.67, 0.0), FVector(-215.67, -215.67, 0.0), FVector(-215.67, 215.67, 0.0), FVector(215.67, 215.67, 0.0) };
	}
	const FTransform StageT = O.Stage->GetActorTransform();
	double Sum = 0.0, Low = 1.0e12, High = -1.0e12;
	int32 N = 0;
	for (const FVector& Foot : Feet)
	{
		const FVector At = StageT.TransformPosition(Foot);
		TArray<FHitResult> Hits;
		World->LineTraceMultiByObjectType(Hits, FVector(At.X, At.Y, At.Z + 3000.0), FVector(At.X, At.Y, At.Z - 3000.0), FCollisionObjectQueryParams(ECC_WorldStatic));
		double Best = -1.0e12;
		for (const FHitResult& H : Hits)
		{
			if (H.GetActor() == Site)
			{
				Best = FMath::Max(Best, double(H.ImpactPoint.Z));
			}
		}
		if (Best > -1.0e11)
		{
			Sum += Best;
			Low = FMath::Min(Low, Best);
			High = FMath::Max(High, Best);
			++N;
		}
	}
	if (N == 0)
	{
		return;
	}
	bPadLevelled = true;
	FVector At = O.Stage->GetActorLocation();
	At.Z = Sum / N;
	O.Stage->SetActorLocation(At);
	PadSeat = O.Stage->CabinSeat();
	O.FlightSpot = PadSeat.GetLocation() + FVector(0.0, 0.0, double(RealAscentAltCm));
	if (Flight.CraftOnEuropa())
	{
		PlaceOnStage(O);
	}
	UE_LOG(LogTemp, Display, TEXT("FLIGHT stage: %d pads on the ice, %.0f..%.0f cm, stands at %s"), N, Low, High, *At.ToString());
}

void ULyudSpaceflightSubsystem::FImpl::PlaceOnStage(ULyudSpaceflightSubsystem& O)
{
	if (O.Cabin && O.Stage)
	{
		PadSeat = O.Stage->CabinSeat();
		O.Cabin->SetActorLocationAndRotation(PadSeat.GetLocation(), O.PadRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void ULyudSpaceflightSubsystem::FImpl::Show(ULyudSpaceflightSubsystem& O, bool bInside, bool bOutside, bool bStage)
{
	if (!bBuilt)
	{
		return;
	}
	if (ShownInside != int32(bInside))
	{
		ShownInside = int32(bInside);
		O.Cabin->SetShownInside(bInside);
	}
	if (ShownOutside != int32(bOutside))
	{
		ShownOutside = int32(bOutside);
		O.Cabin->SetShownOutside(bOutside);
		// it is solid wherever it can be touched: on the ice while the hero is there, aboard, afloat
		O.Cabin->SetActorEnableCollision(bOutside);
	}
	if (ShownStage != int32(bStage))
	{
		ShownStage = int32(bStage);
		O.Stage->SetShown(bStage);
	}
}

void ULyudSpaceflightSubsystem::FImpl::SetActive(ULyudSpaceflightSubsystem& O, bool bOn)
{
	if (O.bActive == bOn)
	{
		return;
	}
	O.bActive = bOn;
	UE_LOG(LogTemp, Display, TEXT("FLIGHT %s"), bOn ? TEXT("awake") : TEXT("asleep"));
	if (bOn)
	{
		return;
	}
	// the hero is elsewhere and the craft stands cold on Europa: nothing is drawn, heard or held
	StopSounds(O, 0.5f);
	StopMusic(O);
	ShowRig(O, false);
	TakeObserver(O, false);
	Show(O, false, false, false);
	ShowSubtitle(O, false);
	SubtitleFor = SubtitleAlpha = 0.f;
	if (O.Camera)
	{
		O.Camera->bOn = false;
	}
	if (!O.ShownPrompt.IsEmpty())
	{
		O.Action.Reset();
		O.ShownPrompt.Reset();
		if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(&O))
		{
			UI->SetPrompt(&O, FText::GetEmpty());
		}
	}
	if (ULyudSoundscapeSubsystem* SS = ULyudSoundscapeSubsystem::Get(&O))
	{
		if (bHold)
		{
			bHold = false;
			SS->SetMusicHold(&O, false);
		}
		if (bPoolsLoaded)
		{
			bPoolsLoaded = false;
			for (const TCHAR* Pool : Pools)
			{
				SS->UnloadPool(FName(Pool));
			}
		}
	}
	if (bBuilt)
	{
		bBuilt = false;
		O.Cabin->Unbuild();
		ULyudPrefetch::ReleaseFolder(&O, CraftFolder);
		ULyudPrefetch::ReleaseFolder(&O, SoundFolder);
	}
}

// ------------------------------------------------------------------ the hero in the craft

FVector ULyudSpaceflightSubsystem::FImpl::FeetOf(const ACharacter& Hero) const
{
	return Hero.GetActorLocation() - FVector(0, 0, Hero.GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

void ULyudSpaceflightSubsystem::FImpl::SetFeet(ACharacter& Hero, const FVector& Feet) const
{
	Hero.SetActorLocation(Feet + FVector(0, 0, Hero.GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
}

void ULyudSpaceflightSubsystem::FImpl::SetAboard(ULyudSpaceflightSubsystem& O, bool bOn)
{
	if (O.bAboard == bOn)
	{
		return;
	}
	O.bAboard = bOn;
	if (!bOn)
	{
		O.bSeated = false;
		O.bBuckled = false;
		Look = NAME_None;
		if (O.bBookOpen)
		{
			O.bBookOpen = false;
			SubtitleFor = 0.f;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("FLIGHT hero %s"), bOn ? TEXT("aboard") : TEXT("out of the cabin"));
}

void ULyudSpaceflightSubsystem::FImpl::Sit(ULyudSpaceflightSubsystem& O, bool bSnap)
{
	APlayerController* PC = nullptr;
	AHeroCharacter* H = Cast<AHeroCharacter>(ScPlayerHero(O.GetWorld(), &PC));
	if (!H || !O.Cabin || !bBuilt)
	{
		return;
	}
	// his own couch: the seat's frame on the cabin (origin on the floor under the pelvis, X the way the couch faces)
	const FTransform CabinT = O.Cabin->GetActorTransform();
	const FTransform Pelvis = O.Cabin->SeatPelvis().GetRelativeTransform(CabinT);
	const FVector P = Pelvis.GetLocation();
	const FTransform SeatLocal(FRotator(0.f, float(Pelvis.Rotator().Yaw), 0.f), FVector(P.X, P.Y, 0.0));
	H->SitOn(O.Cabin->GetRootComponent(), SeatLocal, float(P.Z));
	if (!H->IsSeated())
	{
		return;
	}
	H->SetStandUpOffset(SeatLocal.InverseTransformPosition(CabinT.InverseTransformPosition(O.Cabin->StandPoint())));
	// the couch does not slide; Space asks to get up (answered in the tick: the belt and the load have a say)
	TWeakObjectPtr<ULyudSpaceflightSubsystem> W(&O);
	H->SeatMove = [](const FVector2D&) {};
	H->SeatLeave = [W]() -> bool
	{
		if (W.IsValid() && W->Impl)
		{
			W->Impl->bWantStand = true;
		}
		return false;
	};
	SetAboard(O, true);
	O.bSeated = true;
	Look = NAME_None;
	if (bSnap)
	{
		bEyeInit = false;
		EyeAlpha = 1.f;
	}
	if (PC)
	{
		PC->SetControlRotation(O.Cabin->SeatEye().Rotator());
	}
	if (!bSnap)
	{
		Snd(O, TEXT("sc_seat"), TEXT("couch"), 0.8f);
	}
	UE_LOG(LogTemp, Display, TEXT("FLIGHT hero seated"));
}

void ULyudSpaceflightSubsystem::FImpl::Stand(ULyudSpaceflightSubsystem& O)
{
	AHeroCharacter* H = Cast<AHeroCharacter>(ScPlayerHero(O.GetWorld()));
	if (!H || !O.bSeated)
	{
		return;
	}
	O.bSeated = false;
	O.bBuckled = false;
	Look = NAME_None;
	if (H->IsSeated())
	{
		H->StandUp();
	}
	Snd(O, TEXT("sc_seat"), TEXT("couch"), 0.5f);
	UE_LOG(LogTemp, Display, TEXT("FLIGHT hero up from the couch"));
}

void ULyudSpaceflightSubsystem::FImpl::StartMove(ULyudSpaceflightSubsystem& O, const FVector& ToWorld, float Seconds, int32 Then)
{
	APlayerController* PC = nullptr;
	ACharacter* Hero = ScPlayerHero(O.GetWorld(), &PC);
	if (!Hero || !PC || !O.Cabin || Move.bOn)
	{
		return;
	}
	const FTransform CabinT = O.Cabin->GetActorTransform();
	Move.bOn = true;
	Move.FromLocal = CabinT.InverseTransformPosition(FeetOf(*Hero));
	Move.ToLocal = CabinT.InverseTransformPosition(ToWorld);
	Move.T = 0.f;
	Move.Seconds = FMath::Max(0.2f, Seconds);
	Move.Then = Then;
	if (ALyudSea* Sea = ALyudSea::Get(&O))
	{
		Sea->SetPlayerScripted(&O, true);
	}
	Hero->SetActorEnableCollision(false);
	UCharacterMovementComponent* Mv = Hero->GetCharacterMovement();
	Mv->StopMovementImmediately();
	Mv->SetMovementMode(MOVE_None);
	PC->SetIgnoreMoveInput(true);
}

void ULyudSpaceflightSubsystem::FImpl::TickMove(ULyudSpaceflightSubsystem& O, ACharacter& Hero, float Dt)
{
	Move.T += Dt;
	const float A = ScSmooth01(Move.T / Move.Seconds);
	// over the hatch's sill: the feet lift as he goes through, in the cabin's own frame (it may be rocking on the sea)
	FVector L = FMath::Lerp(Move.FromLocal, Move.ToLocal, A);
	L.Z += 42.f * FMath::Sin(PI * A);
	SetFeet(Hero, O.Cabin->GetActorTransform().TransformPosition(L));
	if (Move.T < Move.Seconds)
	{
		return;
	}
	Move.bOn = false;
	Hero.SetActorEnableCollision(true);
	Hero.GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	if (APlayerController* PC = Cast<APlayerController>(Hero.GetController()))
	{
		PC->SetIgnoreMoveInput(false);
	}
	if (ALyudSea* Sea = ALyudSea::Get(&O))
	{
		Sea->SetPlayerScripted(&O, false);
	}
	if (Move.Then == 0)
	{
		SetAboard(O, true);
	}
	else
	{
		SetAboard(O, false);
		if (Move.Then == 2)
		{
			IntoTheSea(O, Hero);
		}
	}
}

void ULyudSpaceflightSubsystem::FImpl::IntoTheSea(ULyudSpaceflightSubsystem& O, ACharacter& Hero)
{
	// out of the hatch, half a metre over the water: the sea takes him from here (ULyudSwimComponent)
	Snd(O, TEXT("sc_drip"), TEXT("hatch"), 0.8f);
	UE_LOG(LogTemp, Display, TEXT("FLIGHT hero out into the sea at %s"), *Hero.GetActorLocation().ToString());
}

void ULyudSpaceflightSubsystem::FImpl::TickHero(ULyudSpaceflightSubsystem& O, ACharacter& Hero, APlayerController& PC, float Dt)
{
	if (!bBuilt)
	{
		return;
	}
	if (Move.bOn)
	{
		TickMove(O, Hero, Dt);
		return;
	}
	const AHeroCharacter* H = Cast<AHeroCharacter>(&Hero);
	// taken off the couch by something else (a dev teleport stands him up)
	if (O.bSeated && (!H || !H->IsSeated()))
	{
		O.bSeated = false;
		O.bBuckled = false;
		Look = NAME_None;
	}
	if (!O.bSeated)
	{
		bool bIn = O.Cabin->IsInside(Hero.GetActorLocation());
		if (Flight.Flying() && O.bAboard)
		{
			// in flight nothing but a teleport takes him out: a drift to the shell's edge is still aboard
			bIn = FVector::DistSquared(Hero.GetActorLocation(), O.Cabin->GetActorLocation()) < FMath::Square(1000.0);
		}
		if (bIn != O.bAboard)
		{
			SetAboard(O, bIn);
		}
	}
	if (bWantStand)
	{
		bWantStand = false;
		if (O.bSeated)
		{
			DoAction(O, TEXT("stand"));
		}
	}
	// the couch takes him back before the air (the model lets nobody stand from the approach on)
	if (O.bAboard && !O.bSeated && Flight.Flying() && St.T > Flight.P.C.TApproach - 12.0)
	{
		Sit(O, false);
		if (O.bSeated && !O.bBuckled)
		{
			O.bBuckled = true;
			O.Cabin->PlayActionAnim(TEXT("buckle"), true, false);
			Snd(O, TEXT("sc_belt"), TEXT("couch"), 0.8f, 0.6f);
		}
	}
	// on his feet in flight: his weight is the model's (nothing in the coast, a third in the burn)
	const bool bFloat = O.bAboard && !O.bSeated && Flight.Flying();
	UCharacterMovementComponent* Mv = Hero.GetCharacterMovement();
	if (bFloat != bWeightlessWalk)
	{
		bWeightlessWalk = bFloat;
		if (bFloat)
		{
			SavedGravity = Mv->GravityScale;
		}
		else if (SavedGravity >= 0.f)
		{
			Mv->GravityScale = SavedGravity;
			SavedGravity = -1.f;
		}
	}
	if (bFloat)
	{
		Mv->GravityScale = FMath::Max(0.015f, float(St.GLoad));
		if (St.GLoad < 0.05)
		{
			// he drifts, and the air slows him
			Mv->Velocity *= FMath::Clamp(1.f - 0.6f * Dt, 0.f, 1.f);
		}
	}
	// the capsule on the sea is dry inside: the sea leaves that box alone
	DryTimer -= Dt;
	if (DryTimer <= 0.f)
	{
		DryTimer = 0.25f;
		if (ALyudSea* Sea = ALyudSea::Get(&O))
		{
			const bool bWantDry = O.bRealWorld && (O.bAboard || FVector::DistSquared(Hero.GetActorLocation(), O.Cabin->GetActorLocation()) < FMath::Square(600.0));
			if (bWantDry)
			{
				DryBox = FBox(FVector(-128.0, -128.0, -20.0), FVector(128.0, 128.0, 204.0)).TransformBy(O.Cabin->GetActorTransform());
				Sea->SetDryZone(&O, DryBox);
			}
			else if (DryBox.IsValid)
			{
				DryBox = FBox(ForceInit);
				Sea->SetDryZone(&O, DryBox);
			}
		}
	}
}

void ULyudSpaceflightSubsystem::FImpl::TickCamera(ULyudSpaceflightSubsystem& O, APlayerController& PC, float Dt)
{
	if (!O.Camera && PC.PlayerCameraManager)
	{
		O.Camera = Cast<ULyudFlightCamera>(PC.PlayerCameraManager->AddNewCameraModifier(ULyudFlightCamera::StaticClass()));
	}
	ULyudFlightCamera* Cam = O.Camera;
	if (!Cam)
	{
		return;
	}
	const bool bSeatView = O.bSeated && bBuilt;
	EyeAlpha = FMath::FInterpConstantTo(EyeAlpha, bSeatView ? 1.f : 0.f, Dt, 2.5f);
	Cam->bOn = bBuilt && O.bAboard && (EyeAlpha > 0.f || Flight.Flying());
	// the nap: the eyes close (and open again whatever else happens)
	NapFade = FMath::FInterpConstantTo(NapFade, Cam->bOn && Flight.Napping() ? 0.88f : 0.f, Dt, 0.6f);
	if (PC.PlayerCameraManager && (NapFade > 0.f || bNapFadeOn))
	{
		PC.PlayerCameraManager->SetManualCameraFade(NapFade, FLinearColor::Black, false);
		bNapFadeOn = NapFade > 0.f;
	}
	if (!Cam->bOn)
	{
		bEyeInit = false;
		return;
	}
	const FTransform CabinT = O.Cabin->GetActorTransform();
	if (bSeatView)
	{
		const FTransform Want = Look.IsNone() ? O.Cabin->SeatEye() : O.Cabin->PortholeView(Look);
		const FVector WantLocal = CabinT.InverseTransformPosition(Want.GetLocation());
		EyeLocal = bEyeInit ? FMath::VInterpTo(EyeLocal, WantLocal, Dt, 5.f) : WantLocal;
		bEyeInit = true;
		if (LookTurn > 0.f)
		{
			LookTurn -= Dt;
			PC.SetControlRotation(FMath::RInterpTo(PC.GetControlRotation(), Want.Rotator(), Dt, 6.f));
		}
	}
	Cam->EyeAlpha = EyeAlpha;
	Cam->EyeAt = CabinT.TransformPosition(EyeLocal);
	// the shake (the model's 0..1) and the load: pressed into the couch, the view narrowing
	const float K = FMath::Clamp(CVarFlightShake.GetValueOnGameThread(), 0.f, 3.f) * float(St.Shake);
	ShakeClock += Dt;
	const float A = float(FMath::Fmod(ShakeClock, 1000.0));
	Cam->Shake = FRotator(K * 0.55f * FMath::Sin(A * 31.f + 1.3f * FMath::Sin(A * 7.3f)), K * 0.4f * FMath::Sin(A * 27.f + 0.7f),
		K * 0.5f * FMath::Sin(A * 23.f + 2.1f * FMath::Sin(A * 5.1f)));
	const FVector Jitter(K * 0.5f * FMath::Sin(A * 37.f), K * 0.5f * FMath::Sin(A * 41.f + 1.f), K * 0.7f * FMath::Sin(A * 29.f + 2.f));
	const float Over = FMath::Clamp(float(St.GLoad) - 1.f, 0.f, 3.5f);
	const FVector Down = CabinT.TransformVectorNoScale(ScUe(St.G)).GetSafeNormal();
	Cam->Offset = Jitter + Down * (O.bSeated ? Over * 1.6f : 0.f);
	Cam->FovAdd = -Over * 1.4f;
}

void ULyudSpaceflightSubsystem::FImpl::SetHatch(ULyudSpaceflightSubsystem& O, bool bOpen, bool bSnap)
{
	O.bHatchOpen = bOpen;
	if (bBuilt)
	{
		O.Cabin->PlayActionAnim(TEXT("hatch"), bOpen, bSnap);
		if (!bSnap)
		{
			Snd(O, bOpen ? TEXT("sc_hatch_open") : TEXT("sc_hatch_close"), TEXT("hatch"));
		}
	}
}

void ULyudSpaceflightSubsystem::FImpl::SetWheel(ULyudSpaceflightSubsystem& O, bool bSealed, bool bSnap)
{
	O.bWheelSealed = bSealed;
	if (bBuilt)
	{
		O.Cabin->PlayActionAnim(TEXT("wheel"), bSealed, bSnap);
		if (!bSnap)
		{
			Snd(O, TEXT("sc_wheel"), TEXT("hatch"));
			ScToast(&O, bSealed ? TEXT("ui.flight.toast.sealed") : TEXT("ui.flight.toast.unsealed"));
		}
	}
}

void ULyudSpaceflightSubsystem::FImpl::UpdateAction(ULyudSpaceflightSubsystem& O, ACharacter& Hero, APlayerController& PC)
{
	FString Now, Key;
	const bool bNearHatch = bBuilt && FVector::DistSquared(Hero.GetActorLocation(), O.Cabin->HatchExit()) < FMath::Square(500.0);
	if (bBuilt && !Move.bOn && (O.bAboard || bNearHatch))
	{
		const AHeroCharacter* H = Cast<AHeroCharacter>(&Hero);
		const FVector Eye = PC.PlayerCameraManager ? PC.PlayerCameraManager->GetCameraLocation() : (H ? H->EyeLocation() : Hero.GetPawnViewLocation());
		const FVector Dir = PC.GetControlRotation().Vector();
		const FString Raw = O.Cabin->ActionAt(Eye, Dir, &O.ActionWhere);
		const bool bAfloat = Flight.CapsuleAfloat();
		if (Flight.Napping())
		{
			// asleep: E wakes him, whatever he would be looking at
			Now = Key = TEXT("wake");
		}
		else if (Raw == TEXT("hatch") || Raw == TEXT("wheel"))
		{
			if (O.bSeated)
			{
				// out of reach from the couch
			}
			else if (O.bHatchOpen)
			{
				if (Raw == TEXT("wheel"))
				{
					Now = TEXT("hatch");
					Key = TEXT("hatch_close");
				}
				else if (!O.bAboard)
				{
					Now = Key = TEXT("enter");
				}
				else if (bAfloat)
				{
					Now = Key = TEXT("swim_out");
				}
				else
				{
					Now = Key = TEXT("exit");
				}
			}
			else if (O.bWheelSealed)
			{
				Now = TEXT("wheel");
				Key = TEXT("wheel_unseal");
			}
			else if (Raw == TEXT("wheel"))
			{
				Now = TEXT("wheel");
				Key = TEXT("wheel_seal");
			}
			else
			{
				Now = TEXT("hatch");
				Key = bAfloat ? TEXT("hatch_open_sea") : TEXT("hatch_open");
			}
		}
		else if (!O.bAboard || Raw.IsEmpty())
		{
			// from outside only the hatch answers
		}
		else if (Raw == TEXT("sit"))
		{
			if (!O.bSeated)
			{
				Now = Key = TEXT("sit");
			}
		}
		else if (Raw == TEXT("buckle"))
		{
			if (O.bSeated)
			{
				Now = TEXT("buckle");
				Key = O.bBuckled ? TEXT("unbuckle") : TEXT("buckle");
			}
		}
		else if (Raw == TEXT("sw_light"))
		{
			Now = Raw;
			Key = St.I.bCabinLight ? TEXT("light_off") : TEXT("light_on");
		}
		else if (Raw == TEXT("sw_radio"))
		{
			Now = Raw;
			Key = St.I.bRadio ? TEXT("radio_off") : TEXT("radio_on");
		}
		else if (Raw == TEXT("sw_crt"))
		{
			Now = Raw;
			Key = TEXT("crt_mode");
		}
		else if (Raw.StartsWith(TEXT("sw_")))
		{
			for (int32 i = 0; i < LyudFlight::SwitchCount; ++i)
			{
				const FString Name(UTF8_TO_TCHAR(LyudFlight::SwitchName(i)));
				if (Raw.Mid(3) == Name)
				{
					Now = Raw;
					Key = TEXT("sw.") + Name + (St.Switches[i] ? TEXT("_off") : TEXT("_on"));
					break;
				}
			}
		}
		else if (Raw == TEXT("cover") || (Raw == TEXT("launch") && !O.bCoverUp))
		{
			Now = TEXT("cover");
			Key = O.bCoverUp ? TEXT("cover_down") : TEXT("cover_up");
		}
		else if (Raw == TEXT("launch"))
		{
			Now = Key = TEXT("launch");
		}
		else if (Raw.StartsWith(TEXT("port_")))
		{
			if (O.bSeated)
			{
				if (Look == FName(*Raw.Mid(5)))
				{
					Now = Key = TEXT("look_back");
				}
				else
				{
					Now = Raw;
					Key = TEXT("look_") + Raw.Mid(5);
				}
			}
		}
		else if (Raw == TEXT("player"))
		{
			Now = Raw;
			Key = St.Record.Index < 0 ? TEXT("record_put") : TEXT("record_next");
		}
		else if (Raw == TEXT("needle"))
		{
			Now = Raw;
			Key = St.Record.bPlaying ? TEXT("needle_up") : TEXT("record_put");
		}
		else if (Raw == TEXT("thermos") || Raw == TEXT("book"))
		{
			Now = Key = Raw;
		}
		else if (Raw == TEXT("nap"))
		{
			if (O.bSeated)
			{
				Now = Key = Raw;
			}
		}
		else if (Raw == TEXT("chute_cover") || (Raw == TEXT("chute_handle") && !O.bChuteCoverUp))
		{
			Now = Key = TEXT("chute_cover");
		}
		else if (Raw == TEXT("chute_handle"))
		{
			Now = Key = Raw;
		}
		// looking at nothing of the cabin's: what the director adds by itself
		if (Now.IsEmpty() && O.bAboard)
		{
			if (O.bBookOpen)
			{
				Now = Key = TEXT("book_close");
			}
			else if (O.bSeated && !Look.IsNone())
			{
				Now = Key = TEXT("look_back");
			}
			else if (O.bSeated && PC.GetControlRotation().GetNormalized().Pitch < -50.f)
			{
				// down at his own lap: the belt, then getting up
				Now = Key = O.bBuckled ? TEXT("unbuckle") : TEXT("stand");
			}
		}
	}
	O.Action = Now;
	if (Key != O.ShownPrompt)
	{
		O.ShownPrompt = Key;
		if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(&O))
		{
			UI->SetPrompt(&O, Key.IsEmpty() ? FText::GetEmpty() : LyudText::Get(TEXT("ui.flight.") + Key));
		}
	}
}

void ULyudSpaceflightSubsystem::FImpl::DoAction(ULyudSpaceflightSubsystem& O, const FString& Id)
{
	if (!bBuilt || Move.bOn || Id.IsEmpty())
	{
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("FLIGHT use %s (%s, %.1f s)"), *Id, UTF8_TO_TCHAR(LyudFlight::PhaseName(Flight.Phase())), Flight.Time());
	// the model's refusals are string keys: shown as they are
	auto Refused = [&O](const std::string& Why)
	{
		if (Why.empty())
		{
			return false;
		}
		ScToast(&O, ScStr(Why));
		return true;
	};
	const bool bAfloat = Flight.CapsuleAfloat();
	if (Id == TEXT("wake"))
	{
		Flight.EndNap();
	}
	else if (Id == TEXT("hatch"))
	{
		if (O.bHatchOpen)
		{
			SetHatch(O, false, false);
		}
		else if (O.bWheelSealed)
		{
			DoAction(O, TEXT("wheel"));
		}
		else if (!Refused(Flight.CanOpenHatch()))
		{
			SetHatch(O, true, false);
		}
	}
	else if (Id == TEXT("wheel"))
	{
		if (!O.bHatchOpen && !(O.bWheelSealed && Refused(Flight.CanOpenHatch())))
		{
			SetWheel(O, !O.bWheelSealed, false);
		}
	}
	else if (Id == TEXT("enter"))
	{
		if (O.bHatchOpen && !O.bAboard)
		{
			StartMove(O, O.Cabin->HatchInside(), 1.6f, 0);
		}
	}
	else if (Id == TEXT("exit") || Id == TEXT("swim_out"))
	{
		if (O.bHatchOpen && O.bAboard && !O.bSeated && !Refused(bAfloat ? Flight.CanSwimOut() : Flight.CanOpenHatch()))
		{
			StartMove(O, O.Cabin->HatchExit(), 1.6f, bAfloat ? 2 : 1);
		}
	}
	else if (Id == TEXT("sit"))
	{
		if (O.bAboard && !O.bSeated)
		{
			Sit(O, false);
		}
	}
	else if (Id == TEXT("buckle") || Id == TEXT("unbuckle"))
	{
		if (O.bSeated && !(O.bBuckled && Refused(Flight.CanUnbuckle())) && !(Id == TEXT("unbuckle") && !O.bBuckled))
		{
			O.bBuckled = !O.bBuckled;
			O.Cabin->PlayActionAnim(TEXT("buckle"), O.bBuckled, false);
			Snd(O, TEXT("sc_belt"), TEXT("couch"), 0.8f);
		}
	}
	else if (Id == TEXT("stand"))
	{
		if (O.bSeated && O.bBuckled)
		{
			// the belt first; Space again gets him up
			DoAction(O, TEXT("unbuckle"));
		}
		else if (O.bSeated && !Refused(Flight.CanStand()))
		{
			Stand(O);
		}
	}
	else if (Id == TEXT("sw_light"))
	{
		Flight.ToggleLight();
	}
	else if (Id == TEXT("sw_radio"))
	{
		Flight.ToggleRadio();
	}
	else if (Id == TEXT("sw_crt"))
	{
		Flight.NextCrtMode();
	}
	else if (Id.StartsWith(TEXT("sw_")))
	{
		for (int32 i = 0; i < LyudFlight::SwitchCount; ++i)
		{
			if (Id.Mid(3) == UTF8_TO_TCHAR(LyudFlight::SwitchName(i)))
			{
				// the click, the relay and the buzzer come back as the model's events
				Feed(O, ScPlayerHero(O.GetWorld()));
				Refused(Flight.Toggle(i));
				break;
			}
		}
	}
	else if (Id == TEXT("cover"))
	{
		O.bCoverUp = !O.bCoverUp;
		O.Cabin->PlayActionAnim(TEXT("cover"), O.bCoverUp, false);
		Snd(O, TEXT("sc_cover"), TEXT("cover"));
	}
	else if (Id == TEXT("launch"))
	{
		if (!O.bCoverUp)
		{
			DoAction(O, TEXT("cover"));
			return;
		}
		O.Cabin->PlayActionAnim(TEXT("launch"), true, false);
		Snd(O, TEXT("sc_button"), TEXT("launch"));
		Feed(O, ScPlayerHero(O.GetWorld()));
		if (!Refused(Flight.Launch()))
		{
			OnLaunch(O);
		}
	}
	else if (Id.StartsWith(TEXT("port_")))
	{
		if (O.bSeated)
		{
			const FName Which(*Id.Mid(5));
			Look = Look == Which ? FName() : Which;
			LookTurn = 0.7f;
		}
	}
	else if (Id == TEXT("look_back"))
	{
		Look = NAME_None;
		LookTurn = 0.7f;
	}
	else if (Id == TEXT("player"))
	{
		Refused(Flight.RecordNext());
	}
	else if (Id == TEXT("needle"))
	{
		if (Flight.State().Record.bPlaying)
		{
			Flight.RecordStop();
		}
		else
		{
			Refused(Flight.State().Record.Index < 0 ? Flight.RecordNext() : Flight.RecordNeedleDown());
		}
	}
	else if (Id == TEXT("thermos"))
	{
		O.Cabin->PlayActionAnim(TEXT("thermos"), true, false);
		Snd(O, TEXT("sc_thermos_open"), TEXT("thermos"));
		Snd(O, TEXT("sc_sip"), TEXT("thermos"), 1.f, 1.4f);
		ScToast(&O, TEXT("ui.flight.toast.tea"));
	}
	else if (Id == TEXT("book"))
	{
		// the log: one entry at a time under the view, E on it turns the page, the last page is blank
		BookPage = O.bBookOpen ? BookPage + 1 : 1;
		O.bBookOpen = BookPage <= 6;
		O.Cabin->PlayActionAnim(TEXT("book"), O.bBookOpen, false);
		Snd(O, TEXT("sc_page"), TEXT("book"));
		if (O.bBookOpen)
		{
			SubtitleKey = BookPage <= 5 ? FString::Printf(TEXT("flight.book.%d"), BookPage) : FString(TEXT("flight.book.blank"));
			SubtitleFor = 600.f;
			ShowSubtitle(O, true);
		}
		else
		{
			SubtitleFor = 0.f;
		}
	}
	else if (Id == TEXT("book_close"))
	{
		if (O.bBookOpen)
		{
			O.bBookOpen = false;
			O.Cabin->PlayActionAnim(TEXT("book"), false, false);
			Snd(O, TEXT("sc_page"), TEXT("book"), 0.7f);
			SubtitleFor = 0.f;
		}
	}
	else if (Id == TEXT("nap"))
	{
		if (!O.bSeated)
		{
			ScToast(&O, TEXT("ui.flight.no.not_seated"));
		}
		else
		{
			Feed(O, ScPlayerHero(O.GetWorld()));
			Refused(Flight.StartNap());
		}
	}
	else if (Id == TEXT("chute_cover"))
	{
		O.bChuteCoverUp = !O.bChuteCoverUp;
		O.Cabin->PlayActionAnim(TEXT("chute_cover"), O.bChuteCoverUp, false);
		Snd(O, TEXT("sc_cover"), TEXT("chute_cover"));
	}
	else if (Id == TEXT("chute_handle"))
	{
		if (!O.bChuteCoverUp)
		{
			DoAction(O, TEXT("chute_cover"));
			return;
		}
		// it never does anything: the automatics have it
		O.Cabin->PlayActionAnim(TEXT("chute_handle"), true, false);
		Refused(Flight.PullChuteHandle());
	}
}

// ------------------------------------------------------------------ the flight: where the cabin is, the sky round it

namespace
{
	/** A model vector as it is (right-handed), for FImpl::DirWorld. */
	FVector Rh(const LyudFlight::FVec& V)
	{
		return FVector(V.X, V.Y, V.Z);
	}
}

void ULyudSpaceflightSubsystem::FImpl::Feed(ULyudSpaceflightSubsystem& O, const ACharacter* Hero)
{
	Flight.bSeated = O.bSeated;
	Flight.bBuckled = O.bBuckled;
	Flight.bHatchShut = !O.bHatchOpen;
	Flight.bWheelSealed = O.bWheelSealed;
	Flight.HeroDistM = Hero && O.Cabin ? FVector::Dist(Hero->GetActorLocation(), O.Cabin->GetActorLocation()) * 0.01 : 0.0;
	// the day: the capsule is not taken from under the hero at midnight (the model's "next game day" rule waits until
	// he has left it)
	const long long Today = (long long)(UGameCalendar::NowIn(O.GetWorld()).GetDate().GetTicks() / ETimespan::TicksPerDay);
	if (DayFed < 0 || !(Flight.CapsuleAfloat() && Flight.HeroDistM < 300.0))
	{
		DayFed = Today;
	}
	Flight.GameDay = DayFed;
	Flight.Fast = double(FMath::Clamp(CVarFlightFast.GetValueOnGameThread(), 0.05f, 200.f));
	if (Flight.CraftOnEuropa())
	{
		// the star as the site's sky has it, in the flight frame (kept by the model from the launch on)
		const ULyudEuropaSubsystem* Europa = ULyudEuropaSubsystem::Get(&O);
		if (const ALyudEuropaSite* Site = Europa ? Europa->GetSite() : nullptr)
		{
			const FVector S = O.PadRotation.Quaternion().UnrotateVector(Site->GetSky().Sun).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
			Flight.SunF = ScModel(S);
		}
	}
}

void ULyudSpaceflightSubsystem::FImpl::OnLaunch(ULyudSpaceflightSubsystem& O)
{
	Look = NAME_None;
	if (O.bBookOpen)
	{
		DoAction(O, TEXT("book_close"));
	}
	ULyudQuests::Notify(&O, TEXT("flight"), TEXT("launched"));
	UE_LOG(LogTemp, Display, TEXT("FLIGHT launched (swap to the impostors at %.2f s, flight %d)"), SwapT, Flight.FlightsFlown() + 1);
}

FVector ULyudSpaceflightSubsystem::FImpl::DirWorld(const ULyudSpaceflightSubsystem& O, const FVector& RightHanded) const
{
	return O.PadRotation.Quaternion().RotateVector(Fix.RotateVector(FVector(RightHanded.X, -RightHanded.Y, RightHanded.Z)));
}

void ULyudSpaceflightSubsystem::FImpl::PlaceCabin(ULyudSpaceflightSubsystem& O, float Dt)
{
	const bool bFlying = Flight.Flying();
	if (!O.Cabin || (!bFlying && !Flight.CapsuleAfloat()))
	{
		// on its stage: put there once (PlaceOnStage)
		return;
	}
	if (!O.bRealWorld)
	{
		// the real climb straight up over the real ice, then it hangs there: the universe turns round it (TickSky)
		const double Up = St.T >= SwapT ? double(RealAscentAltCm) : FMath::Min(St.AltEuropaKm * 1.0e5, double(RealAscentAltCm));
		O.Cabin->SetActorLocationAndRotation(PadSeat.GetLocation() + FVector(0.0, 0.0, Up), O.PadRotation, false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}
	// the real world: under the canopy down a slope along its heading onto the splash point, then on the real waves
	const ALyudSea* Sea = ALyudSea::Get(&O);
	const float SeaZ = Sea ? Sea->SeaLevel() : float(O.SplashAt.Z);
	const float Touch = float(Flight.P.C.TTouch);
	const float Wave = bFlying ? ScSmoothStep(Touch, Touch + 2.5f, float(St.T)) : 1.f;
	AfloatClock += Dt;
	const float Clock = float(FMath::Fmod(AfloatClock, 3600.0));
	const float Yaw = AfloatYaw + float(St.ChuteYawDeg);
	FVector At = AfloatAt - FRotator(0.f, AfloatYaw, 0.f).Vector() * (St.AltSeaM * 100.0 * 0.22);
	At += FVector(FMath::Sin(Clock * 0.021f), FMath::Cos(Clock * 0.017f), 0.f) * (DriftCmS * 25.f * Wave);
	float Z = SeaZ + float(St.AltSeaM) * 100.f - O.Cabin->WaterlineZ();
	float Pitch = float(St.SwingPitchDeg) * (1.f - Wave);
	float Roll = float(St.SwingRollDeg) * (1.f - Wave);
	if (Wave > 0.f)
	{
		Z += Wave * 0.5f * BobCm * FMath::Sin(Clock * 2.f * PI / 3.1f);
		Roll += Wave * RollDeg * FMath::Sin(Clock * 2.f * PI / RollPeriod);
		Pitch += Wave * 0.4f * RollDeg * FMath::Sin(Clock * 2.f * PI / (RollPeriod * 1.37f) + 1.f);
		if (Sea)
		{
			const FLyudSeaSample S = Sea->SampleSurface(FVector(At.X, At.Y, SeaZ));
			if (S.bIsSea)
			{
				Z += Wave * 0.8f * (S.SurfaceZ - SeaZ);
				const FVector N = FRotator(0.f, Yaw, 0.f).UnrotateVector(S.Normal);
				Pitch += Wave * FMath::Clamp(-0.6f * FMath::RadiansToDegrees(float(FMath::Atan2(N.X, N.Z))), -8.f, 8.f);
				Roll += Wave * FMath::Clamp(0.6f * FMath::RadiansToDegrees(float(FMath::Atan2(N.Y, N.Z))), -8.f, 8.f);
			}
		}
	}
	AfloatZ = Z;
	O.Cabin->SetActorLocationAndRotation(FVector(At.X, At.Y, Z), FRotator(Pitch, Yaw, Roll), false, nullptr, ETeleportType::TeleportPhysics);
}

void ULyudSpaceflightSubsystem::FImpl::EnterRealWorld(ULyudSpaceflightSubsystem& O)
{
	O.bRealWorld = true;
	ShowRig(O, false);
	if (!bAfloatInit)
	{
		bAfloatInit = true;
		AfloatAt = O.SplashAt;
		AfloatYaw = O.SplashHeadingDeg;
		AfloatClock = 0.0;
		// the bed as the game has it now (the tide, a terrain rebuilt since site.py ran): out to sea until it is deep enough
		if (const ALyudSea* Sea = ALyudSea::Get(&O))
		{
			const FVector Seaward = FRotator(0.f, SeawardYaw, 0.f).Vector();
			int32 Steps = 0;
			while (Steps < 15 && Sea->DepthAt(AfloatAt) < MinDepthCm)
			{
				AfloatAt += Seaward * NudgeCm;
				++Steps;
			}
			AfloatAt.Z = Sea->SeaLevel();
			UE_LOG(LogTemp, Display, TEXT("FLIGHT splash point %s (depth %.0f cm, moved %d steps out)"), *AfloatAt.ToString(), Sea->DepthAt(AfloatAt), Steps);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("FLIGHT real world at %.1f s, %.0f m over the sea"), St.T, St.AltSeaM);
}

void ULyudSpaceflightSubsystem::FImpl::CraftBack(ULyudSpaceflightSubsystem& O)
{
	// the capsule has gone from the sea (or a dev reset): the cabin stands cold on its stage again
	O.bRealWorld = false;
	bAfloatInit = false;
	AfloatClock = 0.0;
	Wet = 0.f;
	O.bHatchOpen = false;
	O.bWheelSealed = false;
	O.bCoverUp = false;
	O.bChuteCoverUp = false;
	O.bBookOpen = false;
	Look = NAME_None;
	if (bBuilt)
	{
		O.Cabin->PlayActionAnim(TEXT("hatch"), false, true);
		O.Cabin->PlayActionAnim(TEXT("wheel"), false, true);
		O.Cabin->PlayActionAnim(TEXT("cover"), false, true);
		O.Cabin->PlayActionAnim(TEXT("chute_cover"), false, true);
		O.Cabin->PlayActionAnim(TEXT("buckle"), false, true);
		O.Cabin->PlayActionAnim(TEXT("book"), false, true);
		O.Cabin->SetChute(0, 0.f, FVector2D::ZeroVector, 0.f);
		O.Cabin->SetJets(0.f);
	}
	bChuteShown = false;
	LastJets = 0.f;
	StopMusic(O);
	PlaceOnStage(O);
	UE_LOG(LogTemp, Display, TEXT("FLIGHT craft back on Europa"));
}

void ULyudSpaceflightSubsystem::FImpl::Restore(ULyudSpaceflightSubsystem& O)
{
	// a loaded game: the model's own string, then where the hero was
	const bool bSave = !O.SavedFlight.IsEmpty() && Flight.Load(ScStd(O.SavedFlight));
	if (!bSave)
	{
		Flight.DevReset();
	}
	GiveRecords();
	if (O.bRealWorld || Move.bOn)
	{
		Move.bOn = false;
		CraftBack(O);
	}
	St = Flight.State();
	O.bHatchOpen = bSave && O.bSavedHatchOpen;
	O.bWheelSealed = bSave && O.bSavedWheelSealed;
	if (O.bSeated)
	{
		// still held by the couch from before the load: let go first
		if (AHeroCharacter* Seated = Cast<AHeroCharacter>(ScPlayerHero(O.GetWorld())); Seated && Seated->IsSeated())
		{
			Seated->StandUp();
		}
	}
	O.bAboard = O.bSeated = O.bBuckled = false;
	DayFed = -1;
	const bool bFlying = Flight.Flying();
	const bool bAfloat = Flight.CapsuleAfloat();
	const int32 Aboard = bSave ? O.SavedAboard : 0;
	if (bFlying || bAfloat || Aboard > 0)
	{
		EnsureBuilt(O, true);
	}
	if (bBuilt)
	{
		O.Cabin->PlayActionAnim(TEXT("hatch"), O.bHatchOpen, true);
		O.Cabin->PlayActionAnim(TEXT("wheel"), O.bWheelSealed, true);
	}
	const ULyudEuropaSubsystem* Europa = ULyudEuropaSubsystem::Get(&O);
	if (const ALyudEuropaSite* Site = Europa ? Europa->GetSite() : nullptr)
	{
		if (const ALyudSky* SkyNow = ScFindSky(O.GetWorld()))
		{
			StarBase = Site->SkyAt(SkyNow->ShownUtc()).StarTurn;
		}
	}
	if (St.bRealWorld && (bFlying || bAfloat))
	{
		O.bRealWorld = true;
		bAfloatInit = !O.SavedAfloatAt.IsNearlyZero();
		AfloatAt = O.SavedAfloatAt;
		AfloatYaw = O.SavedAfloatYaw;
		if (!bAfloatInit)
		{
			O.bRealWorld = false;
			EnterRealWorld(O);
		}
	}
	else
	{
		PlaceOnStage(O);
	}
	PlaceCabin(O, 0.f);
	if (bBuilt && (Aboard >= 2 || bFlying))
	{
		// in flight he is on his couch whatever the save says (nobody is left adrift in the cabin by a load)
		Sit(O, true);
		O.bBuckled = O.bSeated && (Aboard == 3 || bFlying);
		O.Cabin->PlayActionAnim(TEXT("buckle"), O.bBuckled, true);
	}
	else if (Aboard == 1)
	{
		SetAboard(O, true);
	}
	LastPhase = Flight.Phase();
	UE_LOG(LogTemp, Display, TEXT("FLIGHT restored: %s at %.1f s, hero aboard %d, hatch open %d"), UTF8_TO_TCHAR(LyudFlight::PhaseName(Flight.Phase())), Flight.Time(), Aboard, O.bHatchOpen ? 1 : 0);
}

void ULyudSpaceflightSubsystem::FImpl::ShowRig(ULyudSpaceflightSubsystem& O, bool bShow)
{
	if (bRigShown == bShow)
	{
		return;
	}
	bRigShown = bShow;
	if (O.PlanetMesh)
	{
		O.PlanetMesh->SetVisibility(bShow);
		O.AirMesh->SetVisibility(bShow && O.AirMID != nullptr);
		O.StarMesh->SetVisibility(bShow && O.StarMID != nullptr);
		if (!bShow)
		{
			O.EuropaMesh->SetVisibility(false);
		}
	}
	// one planet and one star sphere at a time: the site's own give way while the cabin's are up
	const ULyudEuropaSubsystem* Europa = ULyudEuropaSubsystem::Get(&O);
	if (const ALyudEuropaSite* Site = Europa ? Europa->GetSite() : nullptr)
	{
		if (Site->Planet)
		{
			Site->Planet->SetVisibility(!bShow);
		}
		if (Site->PlanetAir && Site->PlanetAir->GetMaterial(0) != nullptr)
		{
			Site->PlanetAir->SetVisibility(!bShow);
		}
		if (Site->Stars && Site->Stars->GetMaterial(0) != nullptr)
		{
			Site->Stars->SetVisibility(!bShow);
		}
	}
	LastPlanetSun = LastStarSun = FVector::ZeroVector;
	LastIslandSize = LastHaze = LastDetail = LastGain = -1.f;
	SkyTimer = 0.f;
}

void ULyudSpaceflightSubsystem::FImpl::TakeObserver(ULyudSpaceflightSubsystem& O, bool bMine)
{
	if (bObserverMine == bMine)
	{
		return;
	}
	bObserverMine = bMine;
	if (bMine)
	{
		// the sky's star and planet where the flight has them (its sun light comes in through the portholes from there)
		TWeakObjectPtr<ULyudSpaceflightSubsystem> W(&O);
		for (TActorIterator<ALyudSky> It(O.GetWorld()); It; ++It)
		{
			It->SetSpaceObserver([W](const FDateTime&, FVector& OutSun, FVector& OutPlanet)
			{
				if (W.IsValid() && W->Impl)
				{
					OutSun = W->Impl->SunWorld;
					OutPlanet = W->Impl->PlanetWorld;
				}
			});
		}
	}
	else if (ULyudEuropaSubsystem* Europa = ULyudEuropaSubsystem::Get(&O))
	{
		Europa->InstallSpaceObserver();
	}
}

void ULyudSpaceflightSubsystem::FImpl::TickSky(ULyudSpaceflightSubsystem& O, float Dt)
{
	const bool bFlying = Flight.Flying();
	const bool bWant = bBuilt && O.PlanetMesh && (O.bAboard || bFlying) && !O.bRealWorld && !Flight.CapsuleAfloat();
	if (bWant != bRigShown)
	{
		ShowRig(O, bWant);
	}
	if (!bWant)
	{
		return;
	}
	const ULyudEuropaSubsystem* Europa = ULyudEuropaSubsystem::Get(&O);
	const ALyudEuropaSite* Site = Europa ? Europa->GetSite() : nullptr;
	const FQuat PadQ = O.PadRotation.Quaternion();
	// the model's flight frame as the cabin sees it (identity on the pad). While the real ice is under the cabin the
	// universe stays the site's own and the cabin is drawn upright (the model's small tilt in the climb is not shown);
	// after the swap the universe is eased onto the model's and from then on turns round the upright cabin exactly
	const FQuat Qm = FRotationMatrix::MakeFromXZ(ScUe(St.FrameX), ScUe(St.FrameZ)).ToQuat();
	const float W = bFlying ? ScSmooth01(float((St.T - SwapT) / double(BlendSeconds))) : 0.f;
	const FQuat Turn = FQuat::Slerp(FQuat::Identity, Qm, W);
	Fix = Turn * Qm.Inverse();
	const FVector Cab = O.Cabin->GetActorLocation();
	if (!bFlying && Site)
	{
		// parked: the site's sky is the truth; its star sphere's turn and its planet's place are kept from the launch on
		if (Site->Stars)
		{
			StarBase = Site->Stars->GetComponentQuat();
		}
		PlanetOff = FQuat::FindBetweenNormals(DirWorld(O, Rh(St.PlanetDir)).GetSafeNormal(), Site->PlanetDir.GetSafeNormal());
	}
	const FQuat Off = FQuat::Slerp(PlanetOff, FQuat::Identity, bFlying ? ScSmooth01(float(St.T / (SwapT + double(BlendSeconds)))) : 0.f);
	SunWorld = DirWorld(O, Rh(St.SunDir)).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	PlanetWorld = Off.RotateVector(DirWorld(O, Rh(St.PlanetDir))).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);

	// the light by the hour, twice a second
	SkyTimer -= Dt;
	const bool bSlow = SkyTimer <= 0.f;
	if (bSlow)
	{
		SkyTimer = 0.5f;
		if (!Sky.IsValid())
		{
			Sky = ScFindSky(O.GetWorld());
		}
		if (const ALyudSky* S = Sky.Get())
		{
			SunLight = S->SunColor * (S->SunIlluminance / PI);
			// where the sun stands over the town now: the planet's day side (east, north, up at the island)
			PlanetSunRH = LyudEphemeris::Sun(S->ShownUtc(), S->LatitudeDeg, S->LongitudeDeg).EastNorthUp();
			if (!bFlying && Site)
			{
				// the eye as the site has it (ALyudEuropaSite::UpdateLook); kept through the flight
				const float SunAlt = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(float(Site->GetSky().Sun.Z), -1.f, 1.f)));
				SkyDim = FMath::Pow(2.f, -0.85f * S->NightExposureEV * (1.f - FMath::SmoothStep(-8.f, 8.f, SunAlt)));
			}
		}
	}

	// the planet: the splash point's texel along the model's island axis, the pole toward its north axis
	const float SizeEase = ScSmoothStep(26.f, 70.f, float(St.PlanetDiaDeg));
	const float IslandSize = float(IslandExtentKm) * FMath::Pow(1000.f / float(IslandExtentKm), 1.f - SizeEase);
	const double Stretch = double(IslandSize) / IslandExtentKm / double(PlanetRadiusKm);
	const double LatS = FMath::DegreesToRadians(IslandLatDeg) + SplashNorthKm * Stretch;
	const double LonS = FMath::DegreesToRadians(IslandLonDeg) + SplashEastKm * Stretch / FMath::Max(0.2, FMath::Cos(FMath::DegreesToRadians(IslandLatDeg)));
	const FVector Iw = Off.RotateVector(DirWorld(O, Rh(St.PlanetIslandAxis))).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const FVector Nw = Off.RotateVector(DirWorld(O, Rh(St.PlanetNorthAxis)));
	const FVector Tw = (Nw - Iw * FVector::DotProduct(Nw, Iw)).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FVector Zw = (Iw * FMath::Sin(LatS) + Tw * FMath::Cos(LatS)).GetSafeNormal();
	const FVector Mw = (Iw - Zw * FVector::DotProduct(Iw, Zw)).GetSafeNormal(UE_SMALL_NUMBER, Tw);
	const FVector Xw = Mw * FMath::Cos(LonS) + FVector::CrossProduct(Zw, Mw) * FMath::Sin(LonS);
	const FQuat PlanetRot = FRotationMatrix::MakeFromXZ(Xw, Zw).ToQuat();
	const float Half = FMath::DegreesToRadians(FMath::Clamp(float(St.PlanetDiaDeg) * 0.5f, 1.f, 88.f));
	const float R = PlanetDrawCm * FMath::Sin(Half) / PlanetFit;
	const FVector PlanetAt = Cab + PlanetWorld * double(PlanetDrawCm);
	O.PlanetMesh->SetWorldLocationAndRotation(PlanetAt, PlanetRot);
	O.PlanetMesh->SetWorldScale3D(FVector(R / 100.f));
	const float Detail = FMath::Clamp(CVarFlightDetail.GetValueOnGameThread(), 0.f, 1.f);
	const bool bAir = O.AirMID != nullptr && Detail > 0.f;
	if (O.AirMesh->IsVisible() != bAir)
	{
		O.AirMesh->SetVisibility(bAir);
	}
	if (bAir)
	{
		O.AirMesh->SetWorldLocationAndRotation(PlanetAt, PlanetRot);
		O.AirMesh->SetWorldScale3D(FVector(R * AirRadius / 100.f));
	}
	// its sun (the mesh's east at the island is up x north in these axes)
	const FVector PlanetSun = (FVector::CrossProduct(Iw, Tw) * PlanetSunRH.X + Tw * PlanetSunRH.Y + Iw * PlanetSunRH.Z).GetSafeNormal(UE_SMALL_NUMBER, Iw);
	const bool bSunMoved = !PlanetSun.Equals(LastPlanetSun, 2.0e-4);
	const float Gain = 2.2f * SkyDim;
	const float Haze = FMath::Lerp(0.3f, 1.f, ScSmoothStep(2.f, 30.f, float(St.RangeKm)));
	if (O.PlanetMID)
	{
		if (bSunMoved)
		{
			O.PlanetMID->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(float(PlanetSun.X), float(PlanetSun.Y), float(PlanetSun.Z), 0.f));
		}
		if (bSlow)
		{
			O.PlanetMID->SetVectorParameterValue(TEXT("SunLight"), SunLight);
			O.PlanetMID->SetScalarParameterValue(TEXT("CloudShift"), float(FMath::Fmod(St.T * 2.0e-5, 1.0)));
		}
		if (!FMath::IsNearlyEqual(Gain, LastGain, 0.005f))
		{
			O.PlanetMID->SetScalarParameterValue(TEXT("Gain"), Gain);
		}
		if (!FMath::IsNearlyEqual(IslandSize, LastIslandSize, 0.05f))
		{
			LastIslandSize = IslandSize;
			O.PlanetMID->SetScalarParameterValue(TEXT("IslandSizeKm"), IslandSize);
			O.PlanetMID->SetScalarParameterValue(TEXT("IslandLat"), float(IslandLatDeg));
			O.PlanetMID->SetScalarParameterValue(TEXT("IslandLon"), float(IslandLonDeg));
		}
		if (!FMath::IsNearlyEqual(Haze, LastHaze, 0.005f))
		{
			LastHaze = Haze;
			O.PlanetMID->SetScalarParameterValue(TEXT("Haze"), Haze);
		}
		if (!FMath::IsNearlyEqual(Detail, LastDetail, 0.005f))
		{
			LastDetail = Detail;
			O.PlanetMID->SetScalarParameterValue(TEXT("Detail"), Detail);
		}
	}
	if (O.AirMID && bAir)
	{
		if (bSunMoved)
		{
			O.AirMID->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(float(PlanetSun.X), float(PlanetSun.Y), float(PlanetSun.Z), 0.f));
		}
		if (bSlow)
		{
			O.AirMID->SetVectorParameterValue(TEXT("SunLight"), SunLight);
		}
		if (!FMath::IsNearlyEqual(Gain, LastGain, 0.005f))
		{
			O.AirMID->SetScalarParameterValue(TEXT("Gain"), Gain);
		}
	}
	if (bSunMoved)
	{
		LastPlanetSun = PlanetSun;
	}
	LastGain = Gain;

	// Europa astern, once the real ice has been left
	const bool bEuropa = bSiteHiddenByFlight && St.EuropaVisible > 0.01;
	if (O.EuropaMesh->IsVisible() != bEuropa)
	{
		O.EuropaMesh->SetVisibility(bEuropa);
	}
	if (bEuropa)
	{
		const float EHalf = FMath::DegreesToRadians(FMath::Clamp(float(St.EuropaDiaDeg) * 0.5f, 0.3f, 88.f));
		O.EuropaMesh->SetWorldLocationAndRotation(Cab + DirWorld(O, Rh(St.EuropaDir)) * double(EuropaDrawCm), PadQ * Turn);
		O.EuropaMesh->SetWorldScale3D(FVector(EuropaDrawCm * FMath::Sin(EHalf) / 100.f));
	}

	// the stars: fixed in the flight frame
	O.StarMesh->SetWorldLocationAndRotation(Cab, PadQ * Turn * PadQ.Inverse() * StarBase);
	if (O.StarMID)
	{
		if (!SunWorld.Equals(LastStarSun, 2.0e-4))
		{
			LastStarSun = SunWorld;
			const float SunHalf = FMath::DegreesToRadians((Sky.IsValid() ? Sky->SunSizeNowDeg() : 0.5f) * 0.5f);
			O.StarMID->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(float(SunWorld.X), float(SunWorld.Y), float(SunWorld.Z), SunHalf));
		}
		if (bSlow)
		{
			O.StarMID->SetVectorParameterValue(TEXT("Look"), FLinearColor(Site ? Site->StarGain : 1.f, Site && !bSiteHiddenByFlight ? Site->StarTwinkle : 0.f, 0.f, 0.f));
		}
	}
}

void ULyudSpaceflightSubsystem::FImpl::TickExterior(ULyudSpaceflightSubsystem& O, float Dt)
{
	if (!bBuilt || (!O.Cabin->IsShownInside() && !O.Cabin->IsShownOutside()))
	{
		return;
	}
	const bool bFlying = Flight.Flying();
	const bool bAfloat = Flight.CapsuleAfloat();
	const LyudFlight::FConfig& C = Flight.P.C;
	Wet = FMath::FInterpConstantTo(Wet, (bFlying && St.T >= C.TTouch) || bAfloat ? 1.f : 0.f, Dt, 0.7f);
	FLyudCraftExterior E;
	E.Plasma = float(St.Plasma);
	E.PlasmaKelvin = float(St.PlasmaK);
	E.Heat = float(St.I.Hull01);
	E.Scorch = float(St.Scorch);
	// the hoar of Europa on the shell: gone in the first minute of the climb
	E.Frost = bFlying ? 0.3f * (1.f - ScSmoothStep(5.f, 60.f, float(St.T))) : (bAfloat ? 0.f : 0.3f);
	E.Wet = Wet;
	E.Fog = float(St.Whiteout);
	FVector Sun = SunWorld;
	if (O.bRealWorld && Sky.IsValid())
	{
		Sun = Sky->SunDirection();
	}
	else if (!bRigShown)
	{
		const ULyudEuropaSubsystem* Europa = ULyudEuropaSubsystem::Get(&O);
		if (const ALyudEuropaSite* Site = Europa ? Europa->GetSite() : nullptr)
		{
			Sun = Site->GetSky().Sun;
		}
	}
	E.SunLocal = O.Cabin->GetActorQuat().UnrotateVector(Sun);
	E.Time = float(St.T);
	O.Cabin->SetExterior(E);

	// the canopy (a last "none" once it is in the water) and the landing jets
	const int32 Chute = bFlying ? int32(St.Chute) : 0;
	if (Chute != 0 || bChuteShown)
	{
		O.Cabin->SetChute(Chute, float(St.ChuteInflation), FVector2D(-0.5f * float(St.SwingPitchDeg), -0.5f * float(St.SwingRollDeg)), float(St.ChuteYawDeg));
		bChuteShown = Chute != 0;
	}
	const float Jets = bFlying ? float(Flight.P.JetsWindow(St.T)) : 0.f;
	if (Jets > 0.f || LastJets > 0.f)
	{
		LastJets = Jets;
		O.Cabin->SetJets(Jets);
	}

	// the cabin's lamps as a whole (a nap dims them whatever the switch says)
	const int32 LampsOn = Flight.Napping() ? 0 : 1;
	if (LampsOn != ShownCabinLight)
	{
		ShownCabinLight = LampsOn;
		O.Cabin->SetCabinLight(LampsOn != 0);
	}
	// the plasma's flicker inside, and the panel's own glow
	const int32 Fire = St.Plasma > 0.002 && O.Cabin->IsShownInside() ? 1 : 0;
	if (O.FireLight)
	{
		if (Fire != FireLit)
		{
			FireLit = Fire;
			O.FireLight->SetVisibility(Fire != 0);
		}
		if (Fire != 0)
		{
			const float A = float(FMath::Fmod(ShakeClock, 1000.0));
			const float Flicker = 0.78f + 0.14f * FMath::Sin(A * 19.f + 1.7f * FMath::Sin(A * 6.1f)) + 0.08f * FMath::Sin(A * 43.f);
			const float Hot = FMath::Clamp((float(St.PlasmaK) - 1800.f) / 5200.f, 0.f, 1.f);
			O.FireLight->SetIntensity(70.f * float(St.Plasma) * Flicker);
			O.FireLight->SetLightColor(FMath::Lerp(FLinearColor(1.f, 0.42f, 0.14f), FLinearColor(1.f, 0.86f, 0.74f), Hot));
		}
	}
	const int32 Panel = St.I.bPowered && O.Cabin->IsShownInside() ? 1 : 0;
	if (O.PanelLight && Panel != PanelLit)
	{
		PanelLit = Panel;
		O.PanelLight->SetVisibility(Panel != 0);
		O.PanelLight->SetIntensity(Panel != 0 ? 1.4f : 0.f);
	}
}

// ------------------------------------------------------------------ sounds (tools/spacecraft/SOUND_NOTES.md), words

void ULyudSpaceflightSubsystem::FImpl::Snd(ULyudSpaceflightSubsystem& O, const TCHAR* Pool, const TCHAR* Point, float Volume, float In)
{
	if (!bBuilt)
	{
		return;
	}
	if (In > 0.f)
	{
		FLater L;
		L.Pool = FName(Pool);
		L.Point = FName(Point);
		L.In = In;
		L.Volume = Volume;
		Later.Add(L);
		return;
	}
	if (ULyudSoundscapeSubsystem* SS = ULyudSoundscapeSubsystem::Get(&O))
	{
		// from outside on the ice there is no air to carry it: felt through the hands on the hatch, no more
		SS->PlayEffect(FName(Pool), O.Cabin->ActionPoint(FName(Point)), O.bAboard || O.bRealWorld ? Volume : Volume * 0.3f);
	}
}

void ULyudSpaceflightSubsystem::FImpl::HandleEvent(ULyudSpaceflightSubsystem& O, const LyudFlight::FEvent& E)
{
	const FString Id = ScStr(E.Id);
	const FString Arg = ScStr(E.Arg);
	const float Soft = E.bAsleep ? 0.35f : 1.f;
	UE_LOG(LogTemp, Display, TEXT("FLIGHT event %d %s %s at %.1f s"), E.Kind, *Id, *Arg, E.T);
	if (E.Kind == LyudFlight::CueSay)
	{
		// the hero's own line: his voice inside his head, and its subtitle
		const ACharacter* Hero = ScPlayerHero(O.GetWorld());
		Say(O, Id, Id, Hero ? Hero->GetPawnViewLocation() : O.Cabin->GetActorLocation(), 1.f, true);
		return;
	}
	if (E.Kind == LyudFlight::CueAuto)
	{
		// the automatic voice from the speaker grille: auto.<id>
		const FString Short = Id.StartsWith(TEXT("auto.")) ? Id.Mid(5) : Id;
		Snd(O, TEXT("sc_chime"), TEXT("speaker"), 0.45f * Soft);
		Say(O, TEXT("flight.auto.") + Short, FString::Printf(TEXT("line.flightauto.%s.1"), *Short), O.Cabin->ActionPoint(TEXT("speaker")), 0.9f * Soft, !E.bAsleep);
		return;
	}
	if (Id == TEXT("switch_on") || Id == TEXT("switch_off"))
	{
		const bool bOn = Id == TEXT("switch_on");
		Snd(O, TEXT("sc_toggle"), *(TEXT("sw_") + Arg));
		Snd(O, TEXT("sc_relay"), TEXT("panel"), 0.7f, 0.06f);
		if (bOn && Arg == TEXT("gyro"))
		{
			// the whine comes in under the spin-up's end
			Snd(O, TEXT("sc_gyro_spinup"), TEXT("panel"), 0.6f);
			GyroSpin = 3.8f;
		}
	}
	else if (Id == TEXT("switch_click"))
	{
		Snd(O, TEXT("sc_toggle"), *(TEXT("sw_") + Arg), 0.9f);
	}
	else if (Id == TEXT("buzzer"))
	{
		Snd(O, TEXT("sc_buzzer"), TEXT("panel"), 0.8f);
	}
	else if (Id == TEXT("launch"))
	{
		Snd(O, TEXT("sc_relay"), TEXT("panel"), 0.9f, 0.07f);
	}
	else if (Id == TEXT("liftoff"))
	{
		Snd(O, TEXT("sc_ignition"), TEXT("jets"), 1.f);
		EngineHold = 2.7f;
	}
	else if (Id == TEXT("stage_left"))
	{
		Snd(O, TEXT("sc_creak"), TEXT("hull_0"), 0.6f);
	}
	else if (Id == TEXT("engine_cutoff"))
	{
		Snd(O, TEXT("sc_cutoff"), TEXT("jets"), 0.9f * Soft);
	}
	else if (Id == TEXT("correction_burn_on"))
	{
		Snd(O, TEXT("sc_ignition"), TEXT("jets"), 0.5f * Soft);
		EngineHold = 0.4f;
	}
	else if (Id == TEXT("correction_burn_off"))
	{
		Snd(O, TEXT("sc_cutoff"), TEXT("jets"), 0.6f * Soft);
	}
	else if (Id == TEXT("turn_start") || Id == TEXT("approach_start"))
	{
		Snd(O, TEXT("sc_thruster"), TEXT("hull_1"), 0.7f * Soft);
	}
	else if (Id == TEXT("module_sep"))
	{
		Snd(O, TEXT("sc_sep_bang"), TEXT("hull_2"), 1.f);
		Snd(O, TEXT("sc_creak"), TEXT("hull_0"), 0.7f, 0.5f);
		Duck = 0.5f;
	}
	else if (Id == TEXT("entry_interface") || Id == TEXT("blackout_start") || Id == TEXT("blackout_end"))
	{
		Snd(O, TEXT("sc_lamp_tick"), TEXT("panel"), 0.6f);
	}
	else if (Id == TEXT("peak_g"))
	{
		Snd(O, TEXT("sc_creak"), TEXT("hull_3"), 0.9f);
	}
	else if (Id == TEXT("drogue"))
	{
		Snd(O, TEXT("sc_drogue"), TEXT("hull_0"), 1.f);
		Duck = 0.5f;
	}
	else if (Id == TEXT("main_chute"))
	{
		Snd(O, TEXT("sc_chute_open"), TEXT("hull_0"), 1.f);
		Snd(O, TEXT("sc_creak"), TEXT("hull_1"), 0.7f, 0.7f);
		Duck = 0.5f;
	}
	else if (Id == TEXT("jets"))
	{
		Snd(O, TEXT("sc_jets"), TEXT("jets"), 1.f);
	}
	else if (Id == TEXT("splash"))
	{
		Snd(O, TEXT("sc_splash"), TEXT("hull_3"), 1.f);
		Duck = 0.5f;
		DripFor = 60.f;
		DripIn = 2.f;
	}
	else if (Id == TEXT("afloat"))
	{
		// home: the quest event, and once the toast that needs no quest
		ULyudQuests::Notify(&O, TEXT("flight"), TEXT("landed"));
		if (!O.bLandedToastShown)
		{
			O.bLandedToastShown = true;
			ScToast(&O, TEXT("ui.flight.toast.landed"));
		}
		UE_LOG(LogTemp, Display, TEXT("FLIGHT landed (flight %d) at %s"), Flight.FlightsFlown(), *O.Cabin->GetActorLocation().ToString());
	}
	else if (Id == TEXT("nap_start"))
	{
		Snd(O, TEXT("sc_seat"), TEXT("couch"), 0.4f);
		Look = NAME_None;
	}
	else if (Id == TEXT("nap_end"))
	{
		Snd(O, TEXT("sc_seat"), TEXT("couch"), 0.6f);
		const int32 Minutes = FMath::Max(1, FMath::RoundToInt(float(Flight.NapSleptSec() / 60.0)));
		if (ULyudGameScreen* Screen = ULyudGameScreen::Get(&O))
		{
			Screen->Toast(LyudText::Get(TEXT("ui.flight.toast.slept"), { FStringFormatArg(FText::AsNumber(Minutes).ToString()) }));
		}
	}
	else if (Id == TEXT("record_start"))
	{
		if (!bSpinShown)
		{
			Snd(O, TEXT("sc_platter_start"), TEXT("player"), 0.6f * Soft);
		}
		Snd(O, TEXT("sc_needle_down"), TEXT("player"), 0.8f * Soft);
		StopMusic(O);
		bRunout = false;
		// the music a moment after the needle
		MusicIn = 0.15f;
	}
	else if (Id == TEXT("record_stop"))
	{
		Snd(O, TEXT("sc_needle_up"), TEXT("player"), 0.8f * Soft);
		StopMusic(O);
		bRunout = false;
	}
	else if (Id == TEXT("record_side_end"))
	{
		StopMusic(O);
		bRunout = true;
	}
	else if (Id == TEXT("needle_skip"))
	{
		Snd(O, TEXT("sc_needle_skip"), TEXT("player"), 0.8f * Soft);
		// the music jumps to where the model's needle landed
		MusicPos = -1000.0;
	}
}

void ULyudSpaceflightSubsystem::FImpl::TickSounds(ULyudSpaceflightSubsystem& O, float Dt)
{
	ULyudSoundscapeSubsystem* SS = ULyudSoundscapeSubsystem::Get(&O);
	if (!SS || !bBuilt || O.Loops.Num() != LoopCount)
	{
		return;
	}
	for (int32 i = Later.Num() - 1; i >= 0; --i)
	{
		Later[i].In -= Dt;
		if (Later[i].In <= 0.f)
		{
			const FLater L = Later[i];
			Later.RemoveAtSwap(i);
			SS->PlayEffect(L.Pool, O.Cabin->ActionPoint(L.Point), O.bAboard || O.bRealWorld ? L.Volume : L.Volume * 0.3f);
		}
	}
	const LyudFlight::FConfig& C = Flight.P.C;
	const bool bFlying = Flight.Flying();
	const bool bAfloat = Flight.CapsuleAfloat();
	const bool bWet = bAfloat || (bFlying && St.T >= C.TTouch);
	const float P = float(St.Plasma);
	const float G = float(St.GLoad);
	const float Speed01 = FMath::Clamp(float(St.SpeedKms) * 1000.f / 200.f, 0.f, 1.f);
	const bool bCoasting = St.Phase == LyudFlight::EPhase::Coast || St.Phase == LyudFlight::EPhase::Approach;
	const bool bEntry = St.Phase == LyudFlight::EPhase::Entry;
	EngineHold = FMath::Max(0.f, EngineHold - Dt);
	GyroSpin = FMath::Max(0.f, GyroSpin - Dt);
	// the loudest moments duck the beds (-6 dB, back over a second and a half)
	Duck = FMath::FInterpConstantTo(Duck, 1.f, Dt, 0.35f);
	const bool bEngine = (St.bEngine || St.bBurn) && EngineHold <= 0.f;

	float Want[LoopCount] = {};
	float Pitch[LoopCount];
	for (float& V : Pitch)
	{
		V = 1.f;
	}
	if (St.I.bPowered && !(bAfloat && O.bHatchOpen))
	{
		Want[LoopHum] = bEngine ? 0.2f : 0.35f;
	}
	Want[LoopHiss] = St.I.bPowered && St.Switches[LyudFlight::SwAir] ? 0.25f : 0.f;
	Want[LoopGyro] = St.I.bPowered && St.Switches[LyudFlight::SwGyro] && GyroSpin <= 0.f && !bWet ? 0.12f : 0.f;
	if (St.I.bRadio)
	{
		Want[LoopStatic] = St.bBlackout ? 0.4f : (bCoasting ? 0.f : 0.3f);
		Want[LoopBeacon] = bCoasting && !St.bBlackout ? 0.3f : 0.f;
	}
	if (bEngine)
	{
		Want[LoopEngine] = St.bEngine ? 0.9f : 0.6f;
		Pitch[LoopEngine] = FMath::Clamp(1.f + 0.03f * (G - 1.6f), 0.95f, 1.08f);
	}
	Want[LoopRattle] = !bFlying ? 0.f : (St.bEngine ? 0.25f * G / 1.6f : (bEntry ? 0.2f + 0.6f * FMath::Clamp(G / 4.2f, 0.f, 1.f) : 0.3f * float(St.Shake)));
	Want[LoopRoar] = 0.9f * ScSmoothStep(0.f, 0.35f, P) * (1.f - 0.5f * ScSmoothStep(0.55f, 0.9f, P));
	Want[LoopRoarHi] = ScSmoothStep(0.45f, 0.9f, P);
	Pitch[LoopRoar] = Pitch[LoopRoarHi] = 0.9f + 0.15f * P;
	if (bFlying && !bWet && St.Chute == LyudFlight::EChute::Drogue)
	{
		Want[LoopWind] = 0.8f * FMath::Max(Speed01, 0.3f);
		Pitch[LoopWind] = 0.85f + 0.25f * Speed01;
	}
	else if (bFlying && !bWet && St.Chute == LyudFlight::EChute::Main)
	{
		Want[LoopWind] = 0.2f;
		Pitch[LoopWind] = 0.85f;
		Want[LoopChuteCreak] = 0.6f;
	}
	Want[LoopAfloat] = bWet ? 0.5f : 0.f;
	Want[LoopCrackle] = bMusicOn ? 0.18f : 0.f;
	Want[LoopRunout] = bRunout ? 0.3f : 0.f;

	// the hull between: from outside only a little comes through
	const float Hull = O.bAboard ? 1.f : (O.bRealWorld ? 0.5f : 0.15f);
	const float Bus = SS->MasterVolume * SS->EffectsVolume;
	for (int32 i = 0; i < LoopCount; ++i)
	{
		const FLoopDef& Def = LoopDefs[i];
		const float Target = Want[i] * Hull * (i == LoopCrackle || i == LoopRunout ? 1.f : Duck);
		LoopVol[i] = FMath::FInterpConstantTo(LoopVol[i], Target, Dt, Target > LoopVol[i] ? Def.Attack : Def.Release);
		UAudioComponent* A = O.Loops[i];
		if (LoopVol[i] <= 0.003f)
		{
			if (A)
			{
				ULyudSoundscapeSubsystem::ReleaseLoop(A, 0.1f);
				O.Loops[i] = nullptr;
			}
			// a pool that was missing is asked again the next time the loop is wanted
			LoopPitchSet[i] = 0.f;
			continue;
		}
		if (!A && LoopPitchSet[i] >= 0.f)
		{
			A = SS->AttachLoop(FName(Def.Pool), O.Cabin->GetRootComponent(), LoopVol[i]);
			O.Loops[i] = A;
			LoopSet[i] = LoopVol[i];
			LoopPitchSet[i] = A ? 1.f : -1.f;
			if (A)
			{
				A->SetWorldLocation(O.Cabin->ActionPoint(FName(Def.Point)));
			}
		}
		if (!A)
		{
			continue;
		}
		if (FMath::Abs(LoopVol[i] - LoopSet[i]) > 0.004f)
		{
			LoopSet[i] = LoopVol[i];
			A->SetVolumeMultiplier(LoopVol[i] * Bus);
		}
		if (FMath::Abs(Pitch[i] - LoopPitchSet[i]) > 0.004f)
		{
			LoopPitchSet[i] = Pitch[i];
			A->SetPitchMultiplier(Pitch[i]);
		}
	}

	// the things that happen now and then
	static const TCHAR* const HullPoints[4] = { TEXT("hull_0"), TEXT("hull_1"), TEXT("hull_2"), TEXT("hull_3") };
	static const TCHAR* const Ports[3] = { TEXT("port_big"), TEXT("port_left"), TEXT("port_right") };
	if (bFlying && G > 1.2f && (St.bEngine || bEntry))
	{
		CreakIn -= Dt;
		if (CreakIn <= 0.f)
		{
			Snd(O, TEXT("sc_creak"), HullPoints[FMath::RandRange(0, 3)], FMath::FRandRange(0.5f, 0.85f));
			CreakIn = bEntry ? FMath::Lerp(2.5f, 0.6f, FMath::Clamp(G / 4.2f, 0.f, 1.f)) * FMath::FRandRange(0.8f, 1.25f) : FMath::FRandRange(3.f, 8.f);
		}
	}
	if (P > 0.03f)
	{
		CrackleIn -= Dt;
		if (CrackleIn <= 0.f)
		{
			Snd(O, TEXT("sc_plasma_crackle"), Ports[FMath::RandRange(0, 2)], 0.4f + 0.6f * P);
			// a Poisson stream, 6 p^2 a second
			CrackleIn = FMath::Min(4.f, -FMath::Loge(FMath::Max(FMath::FRand(), 0.001f)) / FMath::Max(6.f * P * P, 0.05f));
		}
	}
	if (bCoasting && !St.bBurn)
	{
		PuffIn -= Dt;
		if (PuffIn <= 0.f)
		{
			Snd(O, TEXT("sc_thruster"), HullPoints[FMath::RandRange(0, 3)], Flight.Napping() ? 0.2f : 0.5f);
			PuffIn = FMath::FRandRange(7.f, 24.f);
		}
	}
	if (DripFor > 0.f && bWet)
	{
		DripFor -= Dt;
		DripIn -= Dt;
		if (DripIn <= 0.f)
		{
			Snd(O, TEXT("sc_drip"), HullPoints[FMath::RandRange(0, 3)], FMath::FRandRange(0.5f, 0.9f));
			DripIn = FMath::FRandRange(2.f, 9.f);
		}
	}
	// a lamp lighting ticks (the rule's own state: a blinking lamp ticks once)
	uint32 Mask = 0;
	for (int32 i = 0; i < LyudFlight::LampCount; ++i)
	{
		if (St.Lamps[i].bOn)
		{
			Mask |= 1u << i;
		}
	}
	if (bLampsKnown && (Mask & ~LampMask) != 0 && !St.I.bLampTest && O.bAboard)
	{
		Snd(O, TEXT("sc_lamp_tick"), TEXT("panel"), 0.4f);
	}
	LampMask = Mask;
	bLampsKnown = true;

	// the game's own music keeps out of the cabin in flight and from under a record
	const bool bWantHold = bMusicOn || (O.bAboard && bFlying);
	if (bWantHold != bHold)
	{
		bHold = bWantHold;
		SS->SetMusicHold(&O, bHold);
	}
}

void ULyudSpaceflightSubsystem::FImpl::StopSounds(ULyudSpaceflightSubsystem& O, float Fade)
{
	for (int32 i = 0; i < O.Loops.Num(); ++i)
	{
		if (O.Loops[i])
		{
			ULyudSoundscapeSubsystem::ReleaseLoop(O.Loops[i], Fade);
			O.Loops[i] = nullptr;
		}
		if (i < LoopCount)
		{
			LoopVol[i] = LoopSet[i] = LoopPitchSet[i] = 0.f;
		}
	}
	Later.Reset();
	bLampsKnown = false;
}

void ULyudSpaceflightSubsystem::FImpl::StartMusic(ULyudSpaceflightSubsystem& O, bool bToast)
{
	const LyudFlight::FRecordState& R = St.Record;
	if (O.RecordMusic)
	{
		ULyudSoundscapeSubsystem::ReleaseLoop(O.RecordMusic, 0.1f);
		O.RecordMusic = nullptr;
	}
	bMusicOn = false;
	if (!Recs.IsValidIndex(R.Index))
	{
		return;
	}
	const FRec& Rec = Recs[R.Index];
	auto Load = [](const FString& Folder, const FString& Name) -> USoundBase*
	{
		const FString Package = Folder / Name;
		return FPackageName::DoesPackageExist(Package) ? LoadObject<USoundBase>(nullptr, *(Package + TEXT(".") + Name)) : nullptr;
	};
	// the record itself if it was fetched and imported (tools/spacecraft/fetch_records.py), else one of the game's own cues
	USoundBase* Sound = Load(TEXT("/Game/Audio/Music"), TEXT("MU_SC_") + Rec.Short);
	if (!Sound)
	{
		Sound = Load(TEXT("/Game/Spacecraft/Music"), Rec.Id);
	}
	const bool bOwn = Sound != nullptr;
	for (int32 i = 0; !Sound && i < Rec.Fallback.Num(); ++i)
	{
		Sound = Load(TEXT("/Game/Audio/Music"), TEXT("MU_") + Rec.Fallback[i]);
	}
	if (!Sound)
	{
		Sound = Load(TEXT("/Game/Audio/Music"), TEXT("MU_") + HouseCue);
	}
	if (!Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("Spacecraft: no sound for the record %s (nor a cue to stand in)"), *Rec.Id);
		return;
	}
	O.RecordSound = Sound;
	O.RecordMusic = UGameplayStatics::CreateSound2D(&O, Sound, 1.f, 1.f, 0.f, nullptr, false, false);
	if (!O.RecordMusic)
	{
		return;
	}
	const float Length = Sound->GetDuration();
	const float From = Length > 1.f && Length < 36000.f ? FMath::Fmod(float(R.PosSec), Length) : float(R.PosSec);
	O.RecordMusic->SetVolumeMultiplier(0.01f);
	O.RecordMusic->Play(From);
	bMusicOn = true;
	MusicIndex = R.Index;
	MusicPos = R.PosSec;
	if (bToast && O.bAboard && !Flight.Napping())
	{
		if (ULyudGameScreen* Screen = ULyudGameScreen::Get(&O))
		{
			Screen->Toast(bOwn
				? LyudText::Get(TEXT("ui.flight.toast.record"), { FStringFormatArg(LyudText::Get(TEXT("flight.rec.") + Rec.Short + TEXT(".title")).ToString()) })
				: LyudText::Get(TEXT("ui.flight.toast.house_record")));
		}
	}
	UE_LOG(LogTemp, Display, TEXT("FLIGHT record %s plays %s from %.1f s"), *Rec.Id, *Sound->GetName(), From);
}

void ULyudSpaceflightSubsystem::FImpl::StopMusic(ULyudSpaceflightSubsystem& O)
{
	if (O.RecordMusic)
	{
		ULyudSoundscapeSubsystem::ReleaseLoop(O.RecordMusic, 0.15f);
		O.RecordMusic = nullptr;
	}
	O.RecordSound = nullptr;
	bMusicOn = false;
	MusicIn = -1.f;
}

void ULyudSpaceflightSubsystem::FImpl::TickRecord(ULyudSpaceflightSubsystem& O, float Dt)
{
	if (!bBuilt)
	{
		return;
	}
	const LyudFlight::FRecordState& R = St.Record;
	// the platter, the label, the arm creeping across the side
	const int32 Cell = Recs.IsValidIndex(R.Index) ? Recs[R.Index].Cell : -1;
	const float Arm = R.bPlaying ? 1.f + float(FMath::Clamp(R.PosSec / FMath::Max(1.0, R.LengthSec), 0.0, 1.0)) : 0.f;
	if (Cell != CellShown || R.bPlaying != bSpinShown || FMath::Abs(Arm - ArmShown) > 0.002f)
	{
		CellShown = Cell;
		bSpinShown = R.bPlaying;
		ArmShown = Arm;
		O.Cabin->SetRecord(Cell, R.bPlaying, Arm);
	}
	if (MusicIn > 0.f)
	{
		MusicIn -= Dt;
		if (MusicIn <= 0.f && R.bPlaying)
		{
			StartMusic(O, true);
		}
	}
	else if (R.bPlaying && !bMusicOn && MusicIn < 0.f && Flight.Fast < 1.01)
	{
		// a record that was turning when the game was saved, or one the sounds were dropped under: again, silently
		StartMusic(O, false);
		MusicIn = 0.f;
	}
	if (!bMusicOn)
	{
		return;
	}
	if (!R.bPlaying || !O.RecordMusic)
	{
		StopMusic(O);
		return;
	}
	const ULyudSoundscapeSubsystem* SS = ULyudSoundscapeSubsystem::Get(&O);
	float Near = 1.f;
	if (!O.bAboard)
	{
		// from outside: through the open hatch, a few metres
		const ACharacter* Hero = ScPlayerHero(O.GetWorld());
		const float Far = Hero ? float(FVector::Dist(Hero->GetActorLocation(), O.Cabin->GetActorLocation())) : 1.0e6f;
		Near = (O.bHatchOpen ? 0.5f : 0.12f) * FMath::Clamp(1.f - Far / 1500.f, 0.f, 1.f);
	}
	const float Level = FMath::Min(1.f, (SS ? SS->MasterVolume * SS->MusicVolume * 1.5f : 0.6f)) * Near;
	const float Clock = float(FMath::Fmod(ShakeClock, 1000.0));
	O.RecordMusic->SetVolumeMultiplier(FMath::Max(Level, 0.001f));
	// the platter drags under load and flutters with the shake
	O.RecordMusic->SetPitchMultiplier(float(R.Pitch) * (1.f + 0.006f * float(R.Wow) * FMath::Sin(Clock * 3.6f)));
	MusicPos += double(Dt) * R.Pitch;
	if (Flight.Fast < 1.01 && FMath::Abs(MusicPos - R.PosSec) > 0.35)
	{
		// a needle skip, or a drift: to where the model's needle is
		const float Length = O.RecordSound ? O.RecordSound->GetDuration() : 0.f;
		O.RecordMusic->Play(Length > 1.f && Length < 36000.f ? FMath::Fmod(float(R.PosSec), Length) : float(R.PosSec));
		MusicPos = R.PosSec;
	}
}

void ULyudSpaceflightSubsystem::FImpl::Say(ULyudSpaceflightSubsystem& O, const FString& SubtitleKeyIn, const FString& VoiceKey, const FVector& Where, float Volume, bool bSubtitle)
{
	if (!O.bAboard)
	{
		return;
	}
	USoundBase* Voice = LyudText::LoadVoice(VoiceKey);
	if (Voice)
	{
		if (ULyudSoundscapeSubsystem* SS = ULyudSoundscapeSubsystem::Get(&O))
		{
			SS->PlayVoiceAt(Voice, Where, Volume);
		}
	}
	if (bSubtitle)
	{
		// a line takes the log's page off the screen
		if (O.bBookOpen)
		{
			O.bBookOpen = false;
			if (bBuilt)
			{
				O.Cabin->PlayActionAnim(TEXT("book"), false, false);
			}
		}
		SubtitleKey = SubtitleKeyIn;
		SubtitleFor = Voice ? FMath::Max(3.5f, Voice->GetDuration() + 1.f) : 5.f;
		ShowSubtitle(O, true);
	}
	UE_LOG(LogTemp, Display, TEXT("FLIGHT line %s (voice %d)"), *VoiceKey, Voice ? 1 : 0);
}

void ULyudSpaceflightSubsystem::FImpl::ShowSubtitle(ULyudSpaceflightSubsystem& O, bool bShow)
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return;
	}
	if (bShow && !SubtitleWidget)
	{
		SubtitleWidget = SNew(SLyudFlightSubtitle).Owner(&O);
		GEngine->GameViewport->AddViewportWidgetContent(SubtitleWidget.ToSharedRef(), 59);
	}
	else if (!bShow && SubtitleWidget)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(SubtitleWidget.ToSharedRef());
		SubtitleWidget.Reset();
	}
}

// ------------------------------------------------------------------ the subsystem

ULyudSpaceflightSubsystem* ULyudSpaceflightSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudSpaceflightSubsystem>() : nullptr;
}

bool ULyudSpaceflightSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULyudSpaceflightSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (CVarFlight.GetValueOnGameThread() <= 0)
	{
		return;
	}
	Impl = new FImpl();
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("Spaceflight"), this);
	}
	TrySpawn();
}

void ULyudSpaceflightSubsystem::Deinitialize()
{
	if (Impl)
	{
		Impl->ShowSubtitle(*this, false);
		Impl->StopSounds(*this, 0.f);
		if (RecordMusic)
		{
			ULyudSoundscapeSubsystem::ReleaseLoop(RecordMusic, 0.f);
			RecordMusic = nullptr;
		}
		if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(this))
		{
			Saves->UnregisterSaveObject(TEXT("Spaceflight"));
		}
		delete Impl;
		Impl = nullptr;
	}
	Super::Deinitialize();
}

bool ULyudSpaceflightSubsystem::TrySpawn()
{
	UWorld* World = GetWorld();
	const ULyudEuropaSubsystem* Europa = ULyudEuropaSubsystem::Get(this);
	const ALyudEuropaSite* Site = Europa ? Europa->GetSite() : nullptr;
	// the worlds where the hatch to Europa has its far site (Lvl_Scene01 and its test levels)
	if (bSpawned || !World || !Impl || !Site)
	{
		return false;
	}
	bSpawned = true;
	Impl->LoadData(*this);
	Impl->Sky = ScFindSky(World);
	// some 60 m from Europa's hatch toward the planet's azimuth, the big porthole that way (the model's flight frame)
	const FVector Planet = Impl->Sky.IsValid() ? Site->SkyAt(Impl->Sky->ShownUtc()).Planet : Site->PlanetDir;
	const FVector Azimuth = FVector(Planet.X, Planet.Y, 0.0).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	PadRotation = FRotator(0.f, float(Azimuth.Rotation().Yaw), 0.f);
	const FVector At = Europa->GetSiteOrigin() + Azimuth * double(Impl->StageOffsetCm);
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.Name = TEXT("SpacecraftStage");
	Stage = World->SpawnActor<ALyudSpacecraftStage>(At, PadRotation, P);
	P.Name = TEXT("Spacecraft");
	Cabin = World->SpawnActor<ALyudSpacecraft>(At + FVector(0.0, 0.0, 275.0), PadRotation, P);
	P.Name = TEXT("SpacecraftSky");
	SkyRig = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (!Stage || !Cabin || !SkyRig)
	{
		UE_LOG(LogTemp, Warning, TEXT("Spaceflight: could not spawn the craft"));
		return true;
	}
	USceneComponent* RigRoot = NewObject<USceneComponent>(SkyRig.Get(), TEXT("Root"));
	SkyRig->SetRootComponent(RigRoot);
	RigRoot->RegisterComponent();
	// nothing is loaded, drawn or ticked until the hero comes near (FImpl::EnsureBuilt from the airlock on)
	Stage->SetShown(false);
	Cabin->SetActorHiddenInGame(true);
	Cabin->SetActorEnableCollision(false);
	Cabin->SetActorTickEnabled(false);
	Impl->PadSeat = FTransform(PadRotation, At + FVector(0.0, 0.0, 275.0));
	FlightSpot = Impl->PadSeat.GetLocation() + FVector(0.0, 0.0, double(Impl->RealAscentAltCm));
	UE_LOG(LogTemp, Display, TEXT("FLIGHT spawned: stage %s yaw %.1f, splash %s heading %.0f, swap at %.2f s, %d records"),
		*At.ToString(), PadRotation.Yaw, *SplashAt.ToString(), SplashHeadingDeg, Impl->SwapT, Impl->Recs.Num());
	return true;
}

void ULyudSpaceflightSubsystem::Tick(float DeltaTime)
{
	if (!Impl)
	{
		return;
	}
	if (!bSpawned)
	{
		// the Europa subsystem may begin play after this one: asked again once a second
		SpawnWait -= DeltaTime;
		if (SpawnWait > 0.f)
		{
			return;
		}
		SpawnWait = 1.f;
		if (!TrySpawn())
		{
			return;
		}
	}
	FImpl& M = *Impl;
	APlayerController* PC = nullptr;
	ACharacter* Hero = ScPlayerHero(GetWorld(), &PC);
	ULyudEuropaSubsystem* Europa = ULyudEuropaSubsystem::Get(this);
	ALyudEuropaSite* Site = Europa ? Europa->GetSite() : nullptr;
	if (!Hero || !PC || !Site || !Cabin || !Stage || !SkyRig)
	{
		// not the hero any more (a bird's flight): the craft's prompt does not stay on the screen
		if (!ShownPrompt.IsEmpty())
		{
			Action.Reset();
			ShownPrompt.Reset();
			if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
			{
				UI->SetPrompt(this, FText::GetEmpty());
			}
		}
		if (Camera)
		{
			Camera->bOn = false;
		}
		return;
	}
	if (M.bPendingRestore)
	{
		M.bPendingRestore = false;
		M.Restore(*this);
	}
	// awake while the hero is on Europa (or on his way there through the airlock: the craft loads in the background
	// meanwhile), aboard, in flight, or the capsule is on the sea; otherwise nothing below runs
	const bool bOnEuropa = Europa->IsOnEuropa();
	const bool bBusy = bAboard || M.Flight.Flying() || M.Flight.CapsuleAfloat();
	const ALyudAirlock* Airlock = Europa->GetAirlock();
	const bool bWant = bOnEuropa || bBusy || (Airlock && Airlock->IsInside(Hero->GetActorLocation()));
	if (bWant != bActive)
	{
		M.SetActive(*this, bWant);
	}
	if (!bActive || !M.EnsureBuilt(*this, bBusy))
	{
		return;
	}
	if (!M.bPadLevelled && bOnEuropa)
	{
		M.PadTimer -= DeltaTime;
		if (M.PadTimer <= 0.f)
		{
			M.PadTimer = 0.5f;
			M.LevelPad(*this);
		}
	}

	// the model: four facts and a dt in, events and a state out
	M.Feed(*this, Hero);
	M.Flight.Advance(double(DeltaTime));
	{
		const std::vector<LyudFlight::FEvent> Events = M.Flight.TakeEvents();
		for (const LyudFlight::FEvent& E : Events)
		{
			M.HandleEvent(*this, E);
		}
	}
	M.St = M.Flight.State();
	const bool bFlying = M.Flight.Flying();
	const bool bAfloat = M.Flight.CapsuleAfloat();
	if (M.Flight.Phase() != M.LastPhase)
	{
		M.LastPhase = M.Flight.Phase();
		UE_LOG(LogTemp, Display, TEXT("FLIGHT phase %s at %.1f s"), UTF8_TO_TCHAR(LyudFlight::PhaseName(M.LastPhase)), M.St.T);
	}

	// which world the cabin is in (all from the state, so a load or a dev jump lands right)
	const bool bRealNow = M.St.bRealWorld && (bFlying || bAfloat);
	if (bRealNow && !bRealWorld)
	{
		M.EnterRealWorld(*this);
	}
	else if (!bRealNow && bRealWorld)
	{
		M.CraftBack(*this);
	}
	const bool bSpace = bFlying && !bRealNow;
	M.TakeObserver(*this, bSpace);
	const bool bHideSite = bSpace && M.St.T >= M.SwapT;
	if (bHideSite != M.bSiteHiddenByFlight)
	{
		// the real ice is left 3 km below: from here Europa is the impostor astern
		M.bSiteHiddenByFlight = bHideSite;
		if (bHideSite)
		{
			Site->SetShown(false);
		}
		else if (bOnEuropa && !bRealNow)
		{
			Site->SetShown(true);
		}
	}
	M.PlaceCabin(*this, DeltaTime);

	// what is drawn
	const bool bOnPad = M.Flight.CraftOnEuropa();
	const float HatchDist = float(FVector::Dist(Hero->GetActorLocation(), Cabin->GetActorLocation()));
	const bool bInside = bAboard || bFlying || (bOnPad && bOnEuropa && HatchDist < 1100.f) || (bAfloat && HatchDist < M.HeroFarCm);
	const bool bOutside = bAboard || bFlying || bAfloat || (bOnPad && bOnEuropa);
	M.Show(*this, bInside, bOutside, bOnEuropa && !M.bSiteHiddenByFlight && !bRealNow);
	if (bInside || bOutside)
	{
		Cabin->ApplyState(M.St, DeltaTime);
	}
	M.TickExterior(*this, DeltaTime);
	M.TickSky(*this, DeltaTime);
	M.TickHero(*this, *Hero, *PC, DeltaTime);
	M.TickSounds(*this, DeltaTime);
	M.TickRecord(*this, DeltaTime);
	M.TickCamera(*this, *PC, DeltaTime);

	// E: polled, never consumed (it belongs to several things at once); one prompt
	M.UpdateAction(*this, *Hero, *PC);
	const ULyudInventoryUI* ScreenUI = ULyudInventoryUI::Get(this);
	if (ANpcTalkDirector::UsePressed(PC) && !Action.IsEmpty() && !(ScreenUI && ScreenUI->IsOpen()))
	{
		Use();
	}
	if (M.SubtitleFor > 0.f || M.SubtitleAlpha > 0.f)
	{
		M.SubtitleFor -= DeltaTime;
		M.SubtitleAlpha = FMath::FInterpConstantTo(M.SubtitleAlpha, M.SubtitleFor > 0.f ? 1.f : 0.f, DeltaTime, 3.f);
		if (M.SubtitleFor <= 0.f && M.SubtitleAlpha <= 0.f)
		{
			M.ShowSubtitle(*this, false);
		}
	}
}

FString ULyudSpaceflightSubsystem::PhaseId() const
{
	return Impl ? FString(UTF8_TO_TCHAR(LyudFlight::PhaseName(Impl->Flight.Phase()))) : FString(TEXT("parked"));
}

float ULyudSpaceflightSubsystem::FlightSeconds() const
{
	return Impl ? float(Impl->Flight.Time()) : 0.f;
}

bool ULyudSpaceflightSubsystem::IsFlying() const
{
	return Impl && Impl->Flight.Flying();
}

bool ULyudSpaceflightSubsystem::IsCapsuleAfloat() const
{
	return Impl && Impl->Flight.CapsuleAfloat();
}

bool ULyudSpaceflightSubsystem::IsRecordPlaying() const
{
	return Impl && Impl->St.Record.bPlaying;
}

int32 ULyudSpaceflightSubsystem::FlightsFlown() const
{
	return Impl ? int32(Impl->Flight.FlightsFlown()) : 0;
}

void ULyudSpaceflightSubsystem::Use()
{
	if (Impl && !Action.IsEmpty())
	{
		Impl->DoAction(*this, Action);
	}
}

void ULyudSpaceflightSubsystem::Do(const FString& ActionId)
{
	if (!Impl)
	{
		return;
	}
	if (ActionId == TEXT("dev_swim_out"))
	{
		// tests: off the couch, the hatch open, out into the sea
		Impl->DoAction(*this, TEXT("unbuckle"));
		Impl->DoAction(*this, TEXT("stand"));
		if (bWheelSealed)
		{
			Impl->DoAction(*this, TEXT("wheel"));
		}
		if (!bHatchOpen)
		{
			Impl->DoAction(*this, TEXT("hatch"));
		}
		Impl->DoAction(*this, TEXT("swim_out"));
		return;
	}
	Impl->DoAction(*this, ActionId);
}

void ULyudSpaceflightSubsystem::LookThrough(const FString& Which)
{
	if (Impl && bSeated)
	{
		Impl->Look = Which == TEXT("big") || Which == TEXT("left") || Which == TEXT("right") ? FName(*Which) : FName();
		Impl->LookTurn = 0.7f;
	}
}

// ------------------------------------------------------------------ tests and developer mode

void ULyudSpaceflightSubsystem::GoCraft()
{
	ULyudEuropaSubsystem* Europa = ULyudEuropaSubsystem::Get(this);
	ACharacter* Hero = ScPlayerHero(GetWorld());
	if (!Impl || !bSpawned || !Europa || !Hero || !Stage || !Cabin)
	{
		return;
	}
	FImpl& M = *Impl;
	if (M.Flight.Flying() || M.Flight.CapsuleAfloat())
	{
		M.Flight.DevReset();
		M.CraftBack(*this);
	}
	if (AHeroCharacter* H = Cast<AHeroCharacter>(Hero); H && H->IsSeated())
	{
		H->StandUp();
	}
	M.Move.bOn = false;
	M.SetAboard(*this, false);
	// onto Europa first (its sky, its ground, its gravity), then to the ladder's foot
	Europa->GoEuropa();
	M.EnsureBuilt(*this, true);
	M.LevelPad(*this);
	M.PlaceOnStage(*this);
	M.SetFeet(*Hero, Stage->LadderFoot() + FVector(0.0, 0.0, 5.0));
	if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
	{
		PC->SetControlRotation((Cabin->GetActorLocation() - Hero->GetPawnViewLocation()).Rotation());
	}
	UE_LOG(LogTemp, Display, TEXT("FLIGHT go craft: hero at %s"), *Hero->GetActorLocation().ToString());
}

void ULyudSpaceflightSubsystem::GoCabin()
{
	if (!Impl || !bSpawned || !Cabin)
	{
		return;
	}
	FImpl& M = *Impl;
	if (!M.Flight.Flying() && !M.Flight.CapsuleAfloat())
	{
		GoCraft();
		if (!M.bBuilt)
		{
			return;
		}
		M.SetHatch(*this, false, true);
		M.SetWheel(*this, true, true);
	}
	else
	{
		M.EnsureBuilt(*this, true);
	}
	if (!bSeated)
	{
		M.Sit(*this, true);
	}
	if (bSeated && !bBuckled)
	{
		bBuckled = true;
		Cabin->PlayActionAnim(TEXT("buckle"), true, true);
	}
	M.Feed(*this, ScPlayerHero(GetWorld()));
	UE_LOG(LogTemp, Display, TEXT("FLIGHT go cabin: seated %d"), bSeated ? 1 : 0);
}

void ULyudSpaceflightSubsystem::LaunchNow()
{
	if (!Impl || !bSpawned)
	{
		return;
	}
	FImpl& M = *Impl;
	if (M.Flight.Flying() || M.Flight.CapsuleAfloat())
	{
		ResetFlight();
	}
	GoCabin();
	if (!bSeated)
	{
		return;
	}
	// the checklist in its order, the cover, the button
	for (int32 i = 0; i < LyudFlight::SwitchCount; ++i)
	{
		if (!M.Flight.Switch(i))
		{
			const std::string Why = M.Flight.Toggle(i);
			if (!Why.empty())
			{
				UE_LOG(LogTemp, Warning, TEXT("FLIGHT launch: switch %d refused: %s"), i, UTF8_TO_TCHAR(Why.c_str()));
			}
		}
	}
	bCoverUp = true;
	Cabin->PlayActionAnim(TEXT("cover"), true, true);
	const std::string Why = M.Flight.Launch();
	if (Why.empty())
	{
		M.OnLaunch(*this);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FLIGHT launch refused: %s"), UTF8_TO_TCHAR(Why.c_str()));
	}
}

void ULyudSpaceflightSubsystem::JumpToPhase(const FString& Phase)
{
	if (!Impl || !bSpawned)
	{
		return;
	}
	FImpl& M = *Impl;
	const LyudFlight::FConfig& C = M.Flight.P.C;
	double T = -1.0;
	if (Phase == TEXT("parked"))
	{
		ResetFlight();
		return;
	}
	if (Phase == TEXT("prelaunch"))
	{
		if (M.Flight.Flying() || M.Flight.CapsuleAfloat())
		{
			ResetFlight();
		}
		GoCabin();
		return;
	}
	if (Phase == TEXT("ascent"))
	{
		T = 0.5;
	}
	else if (Phase == TEXT("coast"))
	{
		T = C.TAscentEnd;
	}
	else if (Phase == TEXT("approach"))
	{
		T = C.TApproach;
	}
	else if (Phase == TEXT("entry"))
	{
		T = C.TEntry;
	}
	else if (Phase == TEXT("chute"))
	{
		T = C.TChute;
	}
	else if (Phase == TEXT("splash"))
	{
		T = C.TSplashPhase;
	}
	else if (Phase == TEXT("afloat"))
	{
		T = C.TEnd - 1.0;
	}
	else
	{
		// a number: that flight second
		T = FCString::Atod(*Phase);
		if (T <= 0.0 || T >= C.TEnd)
		{
			UE_LOG(LogTemp, Warning, TEXT("FLIGHT no such phase: %s"), *Phase);
			return;
		}
	}
	// seated and buckled on the pad first (the star's place is taken there), then the model's own jump; the worlds
	// follow from the state in the next tick
	GoCabin();
	if (!bSeated)
	{
		return;
	}
	M.Flight.DevJump(T);
	M.Later.Reset();
	M.Look = NAME_None;
	bCoverUp = true;
	UE_LOG(LogTemp, Display, TEXT("FLIGHT jump to %s (%.1f s)"), *Phase, T);
}

void ULyudSpaceflightSubsystem::ResetFlight()
{
	if (!Impl || !bSpawned)
	{
		return;
	}
	FImpl& M = *Impl;
	const bool bAway = M.Flight.Flying() || M.Flight.CapsuleAfloat();
	M.Flight.DevReset();
	M.CraftBack(*this);
	M.St = M.Flight.State();
	if (bAway && bAboard)
	{
		// the cabin went back to Europa from under him: he goes with it, onto the ice by the ladder
		GoCraft();
	}
	UE_LOG(LogTemp, Display, TEXT("FLIGHT reset"));
}

FString ULyudSpaceflightSubsystem::Report() const
{
	if (!Impl || !bSpawned)
	{
		return FString::Printf(TEXT("FLIGHT report: not spawned (lyud.Flight %d)"), CVarFlight.GetValueOnGameThread());
	}
	const FImpl& M = *Impl;
	const LyudFlight::FState& S = M.St;
	const ACharacter* Hero = ScPlayerHero(GetWorld());
	int32 LoopsOn = 0;
	for (const TObjectPtr<UAudioComponent>& A : Loops)
	{
		LoopsOn += A ? 1 : 0;
	}
	int32 Lamps = 0;
	for (int32 i = 0; i < LyudFlight::LampCount; ++i)
	{
		Lamps += S.Lamps[i].bLit ? 1 : 0;
	}
	return FString::Printf(TEXT("FLIGHT report: phase %s t %.1f flying %d real %d afloat %d flights %d | aboard %d seated %d buckled %d hatch_open %d sealed %d cover %d nap %d look '%s' action '%s' | built %d active %d inside %d outside %d stage %d rig %d site_hidden %d observer %d"),
			UTF8_TO_TCHAR(LyudFlight::PhaseName(M.Flight.Phase())), S.T, M.Flight.Flying() ? 1 : 0, bRealWorld ? 1 : 0, M.Flight.CapsuleAfloat() ? 1 : 0, M.Flight.FlightsFlown(),
			bAboard ? 1 : 0, bSeated ? 1 : 0, bBuckled ? 1 : 0, bHatchOpen ? 1 : 0, bWheelSealed ? 1 : 0, bCoverUp ? 1 : 0, M.Flight.Napping() ? 1 : 0, *M.Look.ToString(), *Action,
			M.bBuilt ? 1 : 0, bActive ? 1 : 0, M.ShownInside, M.ShownOutside, M.ShownStage, M.bRigShown ? 1 : 0, M.bSiteHiddenByFlight ? 1 : 0, M.bObserverMine ? 1 : 0)
		+ FString::Printf(TEXT(" | g %.2f shake %.2f plasma %.2f whiteout %.2f chute %d alt_sea %.0f m range %.0f km planet %.1f deg europa %.1f deg | switches %d%d%d%d%d lamps %d power %d"),
			S.GLoad, S.Shake, S.Plasma, S.Whiteout, int32(S.Chute), S.AltSeaM, S.RangeKm, S.PlanetDiaDeg, S.EuropaDiaDeg,
			S.Switches[0] ? 1 : 0, S.Switches[1] ? 1 : 0, S.Switches[2] ? 1 : 0, S.Switches[3] ? 1 : 0, S.Switches[4] ? 1 : 0, Lamps, S.I.bPowered ? 1 : 0)
		+ FString::Printf(TEXT(" | loops %d record %d playing %d pos %.1f music %d | cabin %s rot %s | hero %s | splash %s | sun %s planet %s"),
			LoopsOn, S.Record.Index, S.Record.bPlaying ? 1 : 0, S.Record.PosSec, M.bMusicOn ? 1 : 0,
			Cabin ? *Cabin->GetActorLocation().ToString() : TEXT("-"), Cabin ? *Cabin->GetActorRotation().ToString() : TEXT("-"),
			Hero ? *Hero->GetActorLocation().ToString() : TEXT("none"), *(bRealWorld ? M.AfloatAt : SplashAt).ToString(), *M.SunWorld.ToString(), *M.PlanetWorld.ToString());
}

void ULyudSpaceflightSubsystem::OnGameSaving_Implementation()
{
	if (!Impl || !bSpawned)
	{
		// a world without the craft keeps what was saved before
		return;
	}
	SavedFlight = ScStr(Impl->Flight.Save());
	SavedAboard = bAboard ? (bSeated ? (bBuckled ? 3 : 2) : 1) : 0;
	bSavedHatchOpen = bHatchOpen;
	bSavedWheelSealed = bWheelSealed;
	SavedAfloatAt = bRealWorld ? Impl->AfloatAt : FVector::ZeroVector;
	SavedAfloatYaw = Impl->AfloatYaw;
}

void ULyudSpaceflightSubsystem::OnGameLoaded_Implementation()
{
	if (Impl)
	{
		// done in the next tick: the hero and Europa's site are there by then
		Impl->bPendingRestore = true;
	}
}

namespace
{
	FAutoConsoleCommandWithWorld FlightUseCmd(TEXT("lyud.Flight.Use"), TEXT("What E does at the craft now."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudSpaceflightSubsystem* F = ScFlightOf(W)) { F->Use(); } }));
	FAutoConsoleCommandWithWorld FlightReportCmd(TEXT("lyud.Flight.Report"), TEXT("Logs the flight's state (FLIGHT report ...)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudSpaceflightSubsystem* F = ScFlightOf(W)) { UE_LOG(LogTemp, Display, TEXT("%s"), *F->Report()); } }));
	FAutoConsoleCommandWithWorld FlightGoCraftCmd(TEXT("lyud.Flight.GoCraft"), TEXT("Onto Europa's ice by the craft's ladder."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudSpaceflightSubsystem* F = ScFlightOf(W)) { F->GoCraft(); } }));
	FAutoConsoleCommandWithWorld FlightGoCabinCmd(TEXT("lyud.Flight.GoCabin"), TEXT("Onto the couch in the cabin on Europa (hatch shut and sealed, buckled)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudSpaceflightSubsystem* F = ScFlightOf(W)) { F->GoCabin(); } }));
	FAutoConsoleCommandWithWorld FlightLaunchCmd(TEXT("lyud.Flight.Launch"), TEXT("Seats the hero, runs the checklist and presses the button."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudSpaceflightSubsystem* F = ScFlightOf(W)) { F->LaunchNow(); } }));
	FAutoConsoleCommandWithWorld FlightResetCmd(TEXT("lyud.Flight.Reset"), TEXT("The craft back on Europa, cold."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudSpaceflightSubsystem* F = ScFlightOf(W)) { F->ResetFlight(); } }));
	FAutoConsoleCommandWithWorld FlightSwimOutCmd(TEXT("lyud.Flight.SwimOut"), TEXT("Afloat: off the couch, the hatch open, out into the sea."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudSpaceflightSubsystem* F = ScFlightOf(W)) { F->Do(TEXT("dev_swim_out")); } }));
	FAutoConsoleCommandWithWorldAndArgs FlightPhaseCmd(TEXT("lyud.Flight.Phase"),
		TEXT("lyud.Flight.Phase <parked|prelaunch|ascent|coast|approach|entry|chute|splash|afloat|seconds>: jumps the flight there."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			if (ULyudSpaceflightSubsystem* F = ScFlightOf(W); F && Args.Num() > 0)
			{
				F->JumpToPhase(Args[0]);
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs FlightDoCmd(TEXT("lyud.Flight.Do"), TEXT("lyud.Flight.Do <action id>: one action whatever the hero looks at (hatch, wheel, sit, sw_power, launch ...)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			if (ULyudSpaceflightSubsystem* F = ScFlightOf(W); F && Args.Num() > 0)
			{
				F->Do(Args[0]);
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs FlightLookCmd(TEXT("lyud.Flight.Look"), TEXT("lyud.Flight.Look <big|left|right|back>: leans to a porthole."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			if (ULyudSpaceflightSubsystem* F = ScFlightOf(W))
			{
				F->LookThrough(Args.Num() > 0 ? Args[0] : FString());
			}
		}));
	FAutoConsoleCommandWithWorld FlightRecordCmd(TEXT("lyud.Flight.Record"), TEXT("The next record onto the platter."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudSpaceflightSubsystem* F = ScFlightOf(W)) { F->Do(TEXT("player")); } }));

	// developer mode (F7): the group "flight"
	bool StandInCabin(UWorld* W, FTransform& Out)
	{
		ULyudSpaceflightSubsystem* F = ScFlightOf(W);
		if (!F)
		{
			return false;
		}
		// onto Europa by the craft first (that builds it), then on his feet in the cabin, the hatch as it was
		F->GoCraft();
		if (!F->GetCabin() || !F->GetCabin()->IsBuilt())
		{
			return false;
		}
		Out = FTransform(F->GetCabin()->GetActorRotation(), F->GetCabin()->StandPoint());
		return true;
	}

	LyudDev::FAutoTeleport GFlightCraftTp(TEXT("flight_craft"), TEXT("flight"), TEXT("dev.tp.flight_craft"), [](UWorld* W, FTransform& Out)
	{
		ULyudSpaceflightSubsystem* F = ScFlightOf(W);
		if (!F || !F->GetStage())
		{
			return false;
		}
		F->GoCraft();
		Out = FTransform(F->GetStage()->GetActorRotation(), F->GetStage()->LadderFoot());
		return true;
	});
	LyudDev::FAutoTeleport GFlightCabinTp(TEXT("flight_cabin"), TEXT("flight"), TEXT("dev.tp.flight_cabin"), [](UWorld* W, FTransform& Out)
	{
		return StandInCabin(W, Out);
	});
	LyudDev::FAutoAction GFlightStart(TEXT("flight.start"), TEXT("flight"), TEXT("dev.action.flight.start"), [](UWorld* W)
	{
		if (ULyudSpaceflightSubsystem* F = ScFlightOf(W))
		{
			F->LaunchNow();
		}
	});
	void DevPhase(UWorld* W, const TCHAR* Phase)
	{
		if (ULyudSpaceflightSubsystem* F = ScFlightOf(W))
		{
			F->JumpToPhase(Phase);
		}
	}
	LyudDev::FAutoAction GFlightParked(TEXT("flight.phase_parked"), TEXT("flight"), TEXT("dev.action.flight.phase_parked"), [](UWorld* W) { DevPhase(W, TEXT("parked")); });
	LyudDev::FAutoAction GFlightPrelaunch(TEXT("flight.phase_prelaunch"), TEXT("flight"), TEXT("dev.action.flight.phase_prelaunch"), [](UWorld* W) { DevPhase(W, TEXT("prelaunch")); });
	LyudDev::FAutoAction GFlightAscent(TEXT("flight.phase_ascent"), TEXT("flight"), TEXT("dev.action.flight.phase_ascent"), [](UWorld* W) { DevPhase(W, TEXT("ascent")); });
	LyudDev::FAutoAction GFlightCoast(TEXT("flight.phase_coast"), TEXT("flight"), TEXT("dev.action.flight.phase_coast"), [](UWorld* W) { DevPhase(W, TEXT("coast")); });
	LyudDev::FAutoAction GFlightApproach(TEXT("flight.phase_approach"), TEXT("flight"), TEXT("dev.action.flight.phase_approach"), [](UWorld* W) { DevPhase(W, TEXT("approach")); });
	LyudDev::FAutoAction GFlightEntry(TEXT("flight.phase_entry"), TEXT("flight"), TEXT("dev.action.flight.phase_entry"), [](UWorld* W) { DevPhase(W, TEXT("entry")); });
	LyudDev::FAutoAction GFlightChute(TEXT("flight.phase_chute"), TEXT("flight"), TEXT("dev.action.flight.phase_chute"), [](UWorld* W) { DevPhase(W, TEXT("chute")); });
	LyudDev::FAutoAction GFlightSplash(TEXT("flight.phase_splash"), TEXT("flight"), TEXT("dev.action.flight.phase_splash"), [](UWorld* W) { DevPhase(W, TEXT("splash")); });
	LyudDev::FAutoAction GFlightAfloat(TEXT("flight.phase_afloat"), TEXT("flight"), TEXT("dev.action.flight.phase_afloat"), [](UWorld* W) { DevPhase(W, TEXT("afloat")); });
	LyudDev::FAutoAction GFlightFast(TEXT("flight.fast"), TEXT("flight"), TEXT("dev.action.flight.fast"),
		[](UWorld*)
		{
			// the whole model twenty times faster (the flight in 45 s), and back
			CVarFlightFast->Set(CVarFlightFast.GetValueOnGameThread() > 1.5f ? TEXT("1") : TEXT("20"), ECVF_SetByConsole);
		},
		[](UWorld*) { return CVarFlightFast.GetValueOnGameThread() > 1.5f; });
	LyudDev::FAutoAction GFlightRecords(TEXT("flight.records"), TEXT("flight"), TEXT("dev.action.flight.records"), [](UWorld* W)
	{
		// the rack is the craft's own (eight sleeves, no pocket items): the next one goes onto the platter
		if (ULyudSpaceflightSubsystem* F = ScFlightOf(W))
		{
			F->Do(TEXT("player"));
		}
	});
	LyudDev::FAutoAction GFlightReset(TEXT("flight.reset"), TEXT("flight"), TEXT("dev.action.flight.reset"), [](UWorld* W)
	{
		if (ULyudSpaceflightSubsystem* F = ScFlightOf(W))
		{
			F->ResetFlight();
		}
	});
}
