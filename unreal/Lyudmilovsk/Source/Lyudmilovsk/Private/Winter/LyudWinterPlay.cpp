#include "Winter/LyudWinterPlay.h"

#include "WinterPlayModel.h"
#include "WinterPlayState.h"

#include "Audio/LyudSoundscape.h"
#include "Camera/PlayerCameraManager.h"
#include "City/CityKit.h"
#include "City/LyudCityThings.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dev/LyudDev.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Festival/LyudFestival.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NpcDialogue.h"
#include "People/PeopleLife.h"
#include "Plants/GameCalendar.h"
#include "Sea/LyudSwim.h"
#include "Sound/SoundBase.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "UI/LyudUIStyle.h"
#include "Weather/LyudWeather.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Winter/LyudWinter.h"
#include "World/LyudLadder.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudWinterPlay, Log, All);

using namespace LyudWinterPlay;


namespace LyudWinterPlayImpl
{
	FString Str(const std::string& S) { return UTF8_TO_TCHAR(S.c_str()); }
	std::string Std(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }
	/** Unreal metres (the model, the json) to world cm and back. */
	FVector Cm(const V3& M) { return FVector(M.X * 100.0, M.Y * 100.0, M.Z * 100.0); }
	V3 Metres(const FVector& C) { return V3(C.X / 100.0, C.Y / 100.0, C.Z / 100.0); }

	TAutoConsoleVariable<int32> CVarWinterPlay(TEXT("lyud.WinterPlay"), 1, TEXT("Winter play (skating, the banya, snowballs, snowmen): 0 off."));

	int32 CityIce()
	{
		static IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.City.Ice"));
		return V ? V->GetInt() : -1;
	}

	CityKit::FSkin Skin(uint8 R, uint8 G, uint8 B, float Rough, CityKit::EPattern Pattern = CityKit::EPattern::None, float Tile = 0.f)
	{
		CityKit::FSkin S;
		S.Color = CityKit::Srgb(R, G, B);
		S.Rough = Rough;
		S.Pattern = Pattern;
		S.Tile = Tile;
		return S;
	}
}
using namespace LyudWinterPlayImpl;

// ------------------------------------------------------------------ the fall's view

bool ULyudIceFallCamera::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);
	if (Phase < 0.f)
	{
		return false;
	}
	// down in a third of a second (tipped, low, the view toward the ice), lying a moment, then up again over GetupS
	const float Down = FMath::Clamp(Phase / 0.35f, 0.f, 1.f);
	const float Up = FMath::Clamp((Phase - FallS) / FMath::Max(0.1f, GetupS), 0.f, 1.f);
	const float A = FMath::SmoothStep(0.f, 1.f, Down) * (1.f - FMath::SmoothStep(0.f, 1.f, Up));
	InOutPOV.Rotation.Roll += Side * 24.f * A;
	InOutPOV.Rotation.Pitch -= 18.f * A;
	InOutPOV.Location.Z -= 95.f * A;
	return false;
}

// ------------------------------------------------------------------ the HUD

class SLyudWinterHud : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudWinterHud) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ULyudWinterPlay>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		using namespace LyudUI;
		TWeakObjectPtr<ULyudWinterPlay> W = InArgs._Owner;
		SetVisibility(EVisibility::HitTestInvisible);
		ColdText = LyudText::Get(TEXT("winter.hud.cold"));
		auto Bar = [W](bool bHeat)
		{
			return SNew(SBox).WidthOverride(220.f).HeightOverride(8.f)
				.Visibility_Lambda([W, bHeat]() { return W.IsValid() && (bHeat ? (W->GetHeat() > 0.02f || W->IsInSteamRoom()) : W->IsColdShown()) ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SProgressBar)
					.Percent_Lambda([W, bHeat]() { return W.IsValid() ? TOptional<float>(FMath::Clamp(bHeat ? W->GetHeat() / 1.12f : W->GetCold() / 1.3f, 0.f, 1.f)) : TOptional<float>(0.f); })
					.FillColorAndOpacity(bHeat ? FLinearColor(0.95f, 0.42f, 0.16f, 0.9f) : FLinearColor(0.45f, 0.72f, 0.98f, 0.9f))
				];
		};
		auto Label = [this, W](bool bHeat)
		{
			return SNew(STextBlock).Font(Font(12))
				.ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.7f))
				.Visibility_Lambda([W, bHeat]() { return W.IsValid() && (bHeat ? (W->GetHeat() > 0.02f || W->IsInSteamRoom()) : W->IsColdShown()) ? EVisibility::Visible : EVisibility::Collapsed; })
				.Text_Lambda([this, W, bHeat]()
				{
					// asked for every frame the HUD shows: the words are looked up only when the rounds change
					if (!W.IsValid())
					{
						return FText();
					}
					if (!bHeat)
					{
						return ColdText;
					}
					const int32 Rounds = W->GetRounds();
					if (Rounds != HeatRounds)
					{
						HeatRounds = Rounds;
						HeatText = LyudText::Get(TEXT("winter.hud.heat"), { FString::FromInt(Rounds) });
					}
					return HeatText;
				})
				.ColorAndOpacity(FLinearColor(0.92f, 0.9f, 0.86f, 0.95f));
		};
		ChildSlot
		[
			SNew(SOverlay)
			// the steam room's haze (thicker after a ladle) and a snowball in the face
			+ SOverlay::Slot()
			[
				SNew(SColorBlock).Color_Lambda([W]()
				{
					const float A = W.IsValid() && W->IsInSteamRoom() ? 0.08f + 0.34f * W->GetSteam() : 0.f;
					return FLinearColor(0.93f, 0.9f, 0.86f, A);
				})
			]
			+ SOverlay::Slot()
			[
				SNew(SColorBlock).Color_Lambda([W]() { return FLinearColor(0.97f, 0.98f, 1.f, W.IsValid() ? 0.7f * W->GetSplat() : 0.f); })
			]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(28, 0, 0, 120))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)[Label(true)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[Bar(true)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)[Label(false)]
				+ SVerticalBox::Slot().AutoHeight()[Bar(false)]
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0, 90, 0, 0))
			[
				SNew(STextBlock).Font(Font(15)).Justification(ETextJustify::Center)
				.ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.75f))
				.Text_Lambda([W]() { return W.IsValid() ? W->GetScoreLine() : FText::GetEmpty(); })
				.ColorAndOpacity(FLinearColor(0.95f, 0.95f, 0.92f, 1.f))
			]
		];
	}

private:
	/** The labels' words (the heat's with the rounds it was made for). */
	FText ColdText, HeatText;
	int32 HeatRounds = -1;
};

// ------------------------------------------------------------------ life

ULyudWinterPlay::ULyudWinterPlay() = default;
ULyudWinterPlay::~ULyudWinterPlay() = default;

ULyudWinterPlay* ULyudWinterPlay::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudWinterPlay>() : nullptr;
}

void ULyudWinterPlay::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	M = MakePimpl<FWinterPlayState>();
	bLoaded = LoadData();
	if (!bLoaded)
	{
		return;
	}
	M->Room.Reset(M->P);
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("WinterPlay"), this);
		Saves->OnNewGame.AddUObject(this, &ULyudWinterPlay::StartOver);
	}
}

bool ULyudWinterPlay::LoadData()
{
	const FString Path = FPaths::ProjectContentDir() / TEXT("City/winter_play.json");
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogLyudWinterPlay, Display, TEXT("WinterPlay: no %s: no skating, banya or snowballs"), *Path);
		return false;
	}
	std::string Error;
	if (!M->P.Load(Std(Text), Error))
	{
		UE_LOG(LogLyudWinterPlay, Warning, TEXT("WinterPlay: %s: %s"), *Path, *Str(Error));
		return false;
	}
	return true;
}

void ULyudWinterPlay::StartOver()
{
	if (!M)
	{
		return;
	}
	M->Save = FSaved();
	M->Body.Reset();
	M->Fight.End();
	M->Balls.clear();
	SkatedM = 0.f;
	bFirstGlide = bLongGlide = false;
	HideSnowmen();
}

void ULyudWinterPlay::Deinitialize()
{
	ShowHud(false);
	if (UWorld* World = GetWorld())
	{
		if (AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)))
		{
			if (bOnIce)
			{
				LeaveIce(Hero);
			}
			if (bSpeedHeld || bLandScaled)
			{
				Hero->SetSpeedScale(1.f);
			}
			Hero->SetAnimSpeedScale(1.f);
		}
		if (ULyudWinterSubsystem* Winter = ULyudWinterSubsystem::Get(World))
		{
			Winter->SetIceHole(TEXT("walrus"), FVector::ZeroVector, false);
		}
	}
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, FText());
	}
	Super::Deinitialize();
}

void ULyudWinterPlay::OnGameSaving_Implementation()
{
	if (!M || !bLoaded)
	{
		return;
	}
	M->Save.ThawHours = M->IceState.ThawHours;
	M->Save.BestGlideM = M->Skater.BestGlideM;
	M->Save.SkatedM = SkatedM;
	M->Save.Rounds = M->Body.Rounds;      // today's (RefreshFacts starts them again on a new day)
	Saved = Str(M->Save.Save());
}

void ULyudWinterPlay::OnGameLoaded_Implementation()
{
	if (!M || !bLoaded)
	{
		return;
	}
	FSaved In;
	if (!Saved.IsEmpty() && In.Load(Std(Saved)))
	{
		M->Save = In;
	}
	else
	{
		M->Save = FSaved();
	}
	M->IceState.ThawHours = M->Save.ThawHours;
	M->Skater.BestGlideM = M->Save.BestGlideM;
	SkatedM = (float)M->Save.SkatedM;
	bFirstGlide = SkatedM > 50.f;
	bLongGlide = M->Skater.BestGlideM >= 25.0;
	M->Body.Reset();
	M->Body.Rounds = FMath::Max(0, M->Save.Rounds);
	M->Fight.End();
	M->Balls.clear();
	Rolling = -1;
	HideSnowmen();
	for (int32 I = 0; I < (int32)M->Save.Snowmen.size(); I++)
	{
		ShowSnowman(I);
	}
	IceTimer = 0.f;
	// a save made on the steam room's shelf puts the hero back inside the banya, whose door never opens (he goes in and out
	// through the fade) and whose walls are solid: out onto the porch, as when he leaves
	UWorld* World = GetWorld();
	AHeroCharacter* Hero = World ? Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
	if (Hero && !Hero->IsSeated())
	{
		const FParams& P = M->P;
		const FRotator Turn(0.f, (float)P.BanyaYaw, 0.f);
		const FVector Local = Turn.UnrotateVector(Hero->GetActorLocation() - Cm(P.BanyaAt));
		if (FMath::Abs(Local.X) < P.BanyaSize[0] * 50.0 && FMath::Abs(Local.Y) < P.BanyaSize[1] * 50.0 && FMath::Abs(Local.Z) < 500.0)
		{
			FVector Out = BanyaPoint(P.Door) + Turn.Vector() * 130.0;
			const float Half = Hero->GetCapsuleComponent() ? Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WinterPlayLoadedOut), false, Hero);
			if (World->LineTraceSingleByChannel(Hit, Out + FVector(0.0, 0.0, 300.0), Out - FVector(0.0, 0.0, 600.0), ECC_Visibility, Q))
			{
				// the porch may not be built yet (the first tick builds it): a little over the sand then
				Out.Z = Hit.ImpactPoint.Z + (bBuilt ? 0.0 : 20.0);
			}
			Hero->TeleportTo(Out + FVector(0.0, 0.0, Half + 2.0), Turn);
		}
	}
}

// ------------------------------------------------------------------ small helpers

USoundBase* ULyudWinterPlay::Sound(const TCHAR* Path)
{
	if (TObjectPtr<USoundBase>* S = SoundCache.Find(Path))
	{
		return S->Get();
	}
	USoundBase* S = LoadObject<USoundBase>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	SoundCache.Add(Path, S);
	return S;
}

void ULyudWinterPlay::PlaySfx(FName Pool, const FVector& At, float Volume)
{
	if (ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this))
	{
		Scape->PlayEffect(Pool, At, Volume);
	}
}

void ULyudWinterPlay::Say(const FString& Key, const TArray<FString>& Args)
{
	if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
	{
		TArray<FStringFormatArg> A;
		for (const FString& S : Args)
		{
			A.Add(FStringFormatArg(S));
		}
		Screen->Toast(LyudText::Get(Key, A));
	}
}

void ULyudWinterPlay::SayOne(const FString& Prefix, int32 Count)
{
	Say(FString::Printf(TEXT("%s.%d"), *Prefix, 1 + (int32)(M->Rng.Next() % (uint32)FMath::Max(1, Count))));
}

void ULyudWinterPlay::Stamp(const FString& Piece)
{
	if (!M->P.HasPiece(Std(Piece)))
	{
		return;
	}
	if (ULyudCityThings* City = ULyudCityThings::Get(this))
	{
		City->GiveStamp(Str(M->P.StampSet) + TEXT("/") + Piece);
	}
}

void ULyudWinterPlay::Event(const FString& What)
{
	ULyudQuests::Notify(this, TEXT("winter"), What);
}

void ULyudWinterPlay::SetPrompt(const FText& Text)
{
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (!UI)
	{
		return;
	}
	const bool bWant = !Text.IsEmpty();
	if (bWant == bPrompt && (!bWant || Text.EqualTo(PromptShown)))
	{
		return;
	}
	bPrompt = bWant;
	PromptShown = Text;
	UI->SetPrompt(this, Text);
}

void ULyudWinterPlay::SetPromptKey(const TCHAR* Key, int32 Arg)
{
	// a held prompt is asked for every frame: its words are looked up (and formatted) only when the key or its number changes
	if (bPrompt && PromptKey == Key && PromptArg == Arg)
	{
		return;
	}
	SetPrompt(Arg != MIN_int32 ? LyudText::Get(Key, { FString::FromInt(Arg) }) : LyudText::Get(Key));
	PromptKey = bPrompt ? Key : nullptr;
	PromptArg = Arg;
}

UStaticMeshComponent* ULyudWinterPlay::MakePart(UStaticMesh* Mesh, const FString& Name, bool bCollide, const FTransform& Xf, float CullCm, bool bMoving)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Holder, *Name);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	if (PropMaterial)
	{
		C->SetMaterial(0, PropMaterial);
	}
	C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	C->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	C->SetCanEverAffectNavigation(false);
	C->SetCachedMaxDrawDistance(CullCm);
	C->LightingChannels.bChannel1 = true;
	if (bMoving)
	{
		// moved while they fly or roll: out of the distance fields and Lumen (perf checklist), no shadow of their own
		C->bAffectDistanceFieldLighting = false;
		C->bAffectDynamicIndirectLighting = false;
		C->SetCastShadow(false);
	}
	C->SetupAttachment(Holder->GetRootComponent());
	C->SetWorldTransform(Xf);
	C->RegisterComponent();
	Holder->AddInstanceComponent(C);
	Parts.Add(C);
	return C;
}

FVector ULyudWinterPlay::BanyaPoint(const double* Local) const
{
	const FParams& P = M->P;
	const FVector L(Local[0] * 100.0, Local[1] * 100.0, Local[2] * 100.0);
	return Cm(P.BanyaAt) + FRotator(0.f, (float)P.BanyaYaw, 0.f).RotateVector(L);
}

bool ULyudWinterPlay::BanyaOpenNow() const
{
	return BanyaOpen(M->P, Month, Day, Weekday, Hour);
}

float ULyudWinterPlay::GetHeat() const { return M ? (float)M->Body.Heat : 0.f; }
float ULyudWinterPlay::GetCold() const { return M ? (float)M->Body.Cold : 0.f; }
float ULyudWinterPlay::GetSteam() const { return M ? (float)M->Room.Steam : 0.f; }
int32 ULyudWinterPlay::GetRounds() const { return M ? M->Body.Rounds : 0; }
bool ULyudWinterPlay::IsColdShown() const { return M && (M->Body.Cold > 0.02 || M->Body.SwimS > 0.0); }
FText ULyudWinterPlay::GetScoreLine() const { return FightLineT > 0.f ? FightLine : FText::GetEmpty(); }

int32 ULyudWinterPlay::RinkReport(float& OutCm) const
{
	OutCm = 0.f;
	if (!M || !bLoaded || !M->Ice.bFrozen)
	{
		return 0;
	}
	OutCm = (float)M->Ice.Cm;
	if (M->Ice.Slush >= 0.5)
	{
		return 3;
	}
	return M->Ice.bOpen ? 1 : 2;
}

FString ULyudWinterPlay::Report() const
{
	if (!M || !bLoaded)
	{
		return TEXT("WinterPlay: not loaded");
	}
	const FIce& I = M->Ice;
	return FString::Printf(TEXT("WinterPlay: %04d-%02d-%02d %.2fh wd %d | ice %s %.1f cm slush %.2f open %d cracks %d | air %.1f C sea %.1f C melt %.2f fdd %.0f | ")
		TEXT("skating %d v %.2f down %d glide best %.1f m skated %.0f m | banya open %d steam %d heat %.2f cold %.2f rounds %d room %.0f C steam %.2f | hole %d | ")
		TEXT("fight %d kids %d (%d:%d) balls %d | snowmen %d rolling %d r %.2f"),
		Year, Month, Day, Hour, Weekday, I.bFrozen ? TEXT("yes") : TEXT("no"), I.Cm, I.Slush, I.bOpen ? 1 : 0, (int32)M->Cracks.size(),
		AirC, SeaC, Melt, Fdd, bOnIce ? 1 : 0, M->Skater.Speed(), M->Skater.bDown ? 1 : 0, M->Skater.BestGlideM, SkatedM,
		BanyaOpenNow() ? 1 : 0, bInSteam ? 1 : 0, M->Body.Heat, M->Body.Cold, M->Body.Rounds, M->Room.C, M->Room.Steam, bHoleOpen ? 1 : 0,
		M->Fight.bOn ? 1 : 0, (int32)M->Fight.Kids.size(), M->Fight.HeroHits, M->Fight.KidHits, (int32)M->Balls.size(),
		(int32)M->Save.Snowmen.size(), Rolling, RollR);
}

void ULyudWinterPlay::GiveKit()
{
	if (ULyudInventory* Inv = ULyudInventory::Get(this))
	{
		for (const TCHAR* Id : { TEXT("skates"), TEXT("birch_venik"), TEXT("banya_hat"), TEXT("banya_towel"), TEXT("carrot"), TEXT("coal") })
		{
			if (Inv->Count(Id) == 0)
			{
				Inv->Add(Id, 1);
			}
		}
	}
}

void ULyudWinterPlay::HeatUp()
{
	if (M)
	{
		M->Body.Heat = M->P.RoundHot + 0.15;
		M->Body.bHot = true;
		M->Body.SinceSteam = 0.0;
	}
}

// ------------------------------------------------------------------ the tick

void ULyudWinterPlay::Tick(float DeltaTime)
{
	if (!bLoaded || !M || CVarWinterPlay.GetValueOnGameThread() == 0)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Dt = FMath::Min(DeltaTime, 0.1f);
	WorldTime = World->GetTimeSeconds();
	if (!bBuilt)
	{
		Build();
		RefreshFacts(true);
	}
	Timer -= Dt;
	if (Timer <= 0.f)
	{
		Timer = 2.f;
		RefreshFacts(false);
	}
	CondTimer -= Dt;
	if (CondTimer <= 0.f)
	{
		CondTimer = 10.f;
		SendConditions();
	}
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);

	// a fade's dark half: do what waited for it, then fade back in
	if (Pending != 0)
	{
		PendingT -= Dt;
		if (PendingT <= 0.f)
		{
			const int32 What = Pending;
			Pending = 0;
			if (Hero)
			{
				if (What == 1)
				{
					EnterBanya(Hero);
				}
				else if (What == 2 || What == 3)
				{
					ExitBanya(What == 3);
				}
			}
			if (PC && PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.6f, FLinearColor::Black, false, false);
			}
		}
	}
	FightLineT = FMath::Max(0.f, FightLineT - Dt);
	Splat = FMath::Max(0.f, Splat - Dt * 0.7f);
	if (!Hero)
	{
		return;
	}
	TickRink(Hero, Dt);
	TickBanya(Hero, Dt);
	TickBody(Hero, Dt);
	TickHole(Dt);
	TickSnow(Hero, Dt);
	// the HUD only while something of it shows
	const bool bHud = bInSteam || M->Body.Heat > 0.02 || IsColdShown() || Splat > 0.f || FightLineT > 0.f;
	ShowHud(bHud);
}

void ULyudWinterPlay::RefreshFacts(bool bForce)
{
	UWorld* World = GetWorld();
	const FDateTime Now = UGameCalendar::NowIn(World);
	Month = Now.GetMonth();
	Day = Now.GetDay();
	Year = Now.GetYear();
	Weekday = (int32)Now.GetDayOfWeek();
	Hour = Now.GetHour() + Now.GetMinute() / 60.0 + Now.GetSecond() / 3600.0;
	const double Hours = Now.GetTicks() / (double)ETimespan::TicksPerHour;
	double Passed = M->LastFactsHours < 0.0 ? 0.0 : FMath::Clamp(Hours - M->LastFactsHours, 0.0, 48.0);
	M->LastFactsHours = Hours;
	if (const ULyudWeatherSubsystem* Wx = ULyudWeatherSubsystem::Get(this))
	{
		const FLyudWeatherState St = Wx->GetState();
		AirC = St.TempC;
		SeaC = St.SeaTempC;
		Melt = St.Melt;
		Fdd = St.FreezingDegreeDays;
		SnowCm = St.SnowDepthCm;
	}
	// the pond's ice and its cracks (a new set only when the winter or the thickness band changes)
	FIceIn In;
	In.Month = Month;
	In.Day = Day;
	In.FreezingDegreeDays = Fdd;
	In.TempC = AirC;
	In.Melt = Melt;
	In.Force = ForcedIceCm >= 0.f ? 1 : CityIce();
	FIce I = M->IceState.Step(M->P, In, Passed);
	if (ForcedIceCm >= 0.f)
	{
		I.Cm = ForcedIceCm;
		I.Slush = 0.0;
		I.bOpen = I.Cm >= M->P.OpenCm;
	}
	M->Ice = I;
	const int32 Winter = WinterOf(Year, Month);
	const std::vector<FCrack> Cr = I.bFrozen ? Cracks(M->P, Winter, I.Cm) : std::vector<FCrack>();
	if (bForce || Winter != CracksWinter || (int32)Cr.size() != CracksCount)
	{
		CracksWinter = Winter;
		CracksCount = (int32)Cr.size();
		M->Cracks = Cr;
		BuildCracks();
	}
	if (CrackPart)
	{
		CrackPart->SetVisibility(I.bFrozen && !M->Cracks.empty());
	}
	// a new day: the banya's rounds start again
	const long long DayIdx = Now.GetTicks() / ETimespan::TicksPerDay;
	if (M->Save.RoundsDay != DayIdx)
	{
		M->Save.RoundsDay = DayIdx;
		M->Body.Rounds = 0;
	}
	// the thaw works on the snowmen
	HourTimer += (float)Passed;
	if (HourTimer >= 0.25f)
	{
		bool bChanged = false;
		for (int32 K = (int32)M->Save.Snowmen.size() - 1; K >= 0; K--)
		{
			FSnowman& Sm = M->Save.Snowmen[(size_t)K];
			const int32 Was = Sm.Balls;
			const double Slump = Sm.Slump;
			if (!MeltSnowman(Sm, M->P, HourTimer, AirC, Melt))
			{
				M->Save.Snowmen.erase(M->Save.Snowmen.begin() + K);
				bChanged = true;
			}
			else if (Sm.Balls != Was || Sm.Slump - Slump > 0.05)
			{
				bChanged = true;
			}
		}
		HourTimer = 0.f;
		if (bChanged)
		{
			// (also while a ball is being rolled: a snowman gone from the list shifts the others' parts)
			HideSnowmen();
			for (int32 K = 0; K < (int32)M->Save.Snowmen.size(); K++)
			{
				ShowSnowman(K);
			}
		}
	}
	// the banya's lights: on while it is heated and he is near; the window glows after dark
	if (Holder && Lights.Num())
	{
		const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
		const bool bNear = Pawn && FVector::Dist(Pawn->GetActorLocation(), Cm(M->P.BanyaAt)) < 6000.0;
		const bool bOn = BanyaOpenNow() && bNear;
		for (UPointLightComponent* L : Lights)
		{
			if (L && L->IsVisible() != bOn)
			{
				L->SetVisibility(bOn);
			}
		}
		if (GlowMid)
		{
			GlowMid->SetScalarParameterValue(TEXT("Glow"), BanyaOpenNow() && (Hour < 8.5 || Hour > 16.0) ? 1.f : 0.f);
		}
	}
}

void ULyudWinterPlay::SendConditions()
{
	UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	if (!People)
	{
		return;
	}
	bool bBusy = false;
	if (M->Ice.bOpen)
	{
		// the first snow's day, a holiday, or the school's winter break at a weekend: skaters all day
		if (const ULyudFestivalSubsystem* Fest = ULyudFestivalSubsystem::Get(this))
		{
			for (const FLyudFestivalNow& F : Fest->ActiveFestivals())
			{
				bBusy |= F.Id == TEXT("first_snow");
			}
			bBusy |= Fest->IsDayOff();
		}
		const int32 K = DateKey(Month, Day);
		bBusy |= (K >= 1229 || K <= 110) && Weekday >= 5;
	}
	People->SetCondition(TEXT("rink"), M->Ice.bOpen);
	People->SetCondition(TEXT("rink_busy"), bBusy);
	People->SetCondition(TEXT("walrus"), WalrusHour(M->P, Month, Day, Weekday, Hour));
	People->SetCondition(TEXT("banya"), BanyaOpenNow());
}

// ------------------------------------------------------------------ the rink

bool ULyudWinterPlay::HeroOnPondIce(const AHeroCharacter* Hero) const
{
	if (!M->Ice.bFrozen)
	{
		return false;
	}
	const FVector L = Hero->GetActorLocation();
	const float Half = Hero->GetCapsuleComponent() ? Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	const double FeetZ = (L.Z - Half) / 100.0;
	const UCharacterMovementComponent* Move = Hero->GetCharacterMovement();
	return OnPond(M->P, L.X / 100.0, L.Y / 100.0, 0.15) && FMath::Abs(FeetZ - (M->P.RinkAt.Z + 0.01)) < 0.45
		&& Move && Move->IsMovingOnGround() && !Hero->IsSeated();
}

void ULyudWinterPlay::EnterIce(AHeroCharacter* Hero)
{
	bOnIce = true;
	UCharacterMovementComponent* Move = Hero->GetCharacterMovement();
	if (!bMoveSaved)
	{
		SavedFriction = Move->GroundFriction;
		SavedBraking = Move->BrakingDecelerationWalking;
		SavedAccel = Move->MaxAcceleration;
		SavedBrakingFactor = Move->BrakingFrictionFactor;
		bMoveSaved = true;
	}
	// the model moves him: the character keeps whatever velocity it is given (no friction, no braking, no acceleration of its own)
	Move->GroundFriction = 0.f;
	Move->BrakingDecelerationWalking = 0.f;
	Move->BrakingFrictionFactor = 0.f;
	Move->MaxAcceleration = 1.f;
	const FVector V = Hero->GetVelocity() / 100.0;
	M->Skater.Vel = V2(V.X, V.Y);
	M->Skater.bDown = false;
	M->Skater.bStroking = false;
	if (bLandScaled)
	{
		Hero->SetSpeedScale(1.f);
		bLandScaled = false;
	}
	bLastFeet = false;
}

void ULyudWinterPlay::LeaveIce(AHeroCharacter* Hero)
{
	bOnIce = false;
	UCharacterMovementComponent* Move = Hero->GetCharacterMovement();
	if (bMoveSaved && Move)
	{
		Move->GroundFriction = SavedFriction;
		Move->BrakingDecelerationWalking = SavedBraking;
		Move->MaxAcceleration = SavedAccel;
		Move->BrakingFrictionFactor = SavedBrakingFactor;
		bMoveSaved = false;
	}
	if (M->Skater.bDown || bWasDown)
	{
		Hero->SetCrouchToggled(false);
		if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
		{
			PC->SetIgnoreMoveInput(false);
		}
		bWasDown = false;
	}
	M->Skater.bDown = false;
	M->Skater.Vel = V2();
	if (FallCam)
	{
		FallCam->Phase = -1.f;
	}
	Hero->SetAnimSpeedScale(1.f);
	if (Glide && Glide->IsPlaying())
	{
		Glide->FadeOut(0.3f, 0.f);
	}
	GlideVolume = 0.f;
}

void ULyudWinterPlay::ShowBlades(AHeroCharacter* Hero, bool bShow)
{
	if (!BladeL || !BladeR)
	{
		return;
	}
	if (bShow != bBladesShown)
	{
		bBladesShown = bShow;
		BladeL->SetVisibility(bShow);
		BladeR->SetVisibility(bShow);
	}
	if (!bShow)
	{
		return;
	}
	// under each shoe, along the foot (the ankle to the ball of the foot), on the sole
	USkeletalMeshComponent* Body = Hero->GetMesh();
	if (!Body)
	{
		return;
	}
	for (int32 Side = 0; Side < 2; Side++)
	{
		const FName FootB = Side ? FName(TEXT("foot_r")) : FName(TEXT("foot_l"));
		const FName BallB = Side ? FName(TEXT("ball_r")) : FName(TEXT("ball_l"));
		if (Body->GetBoneIndex(FootB) == INDEX_NONE || Body->GetBoneIndex(BallB) == INDEX_NONE)
		{
			continue;
		}
		const FVector Foot = Body->GetBoneLocation(FootB), Ball = Body->GetBoneLocation(BallB);
		FVector Along = Ball - Foot;
		Along.Z = 0.0;
		const FVector Dir = Along.GetSafeNormal(UE_SMALL_NUMBER, Hero->GetActorForwardVector());
		const FVector At = (Foot + Ball) * 0.5 + Dir * 3.0 - FVector(0.0, 0.0, 0.0);
		const FVector Pos(At.X, At.Y, FMath::Min(Foot.Z - 8.5, Ball.Z - 4.5));
		(Side ? BladeR : BladeL)->SetWorldLocationAndRotation(Pos, Dir.Rotation());
	}
}

void ULyudWinterPlay::TickRink(AHeroCharacter* Hero, float Dt)
{
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	const bool bWorn = Inv && Inv->IsWorn(TEXT("skates"));
	ShowBlades(Hero, bWorn);
	const ULyudSwimComponent* Swim = Hero->FindComponentByClass<ULyudSwimComponent>();
	const bool bSwimming = Swim && Swim->IsSwimming();
	const bool bOn = HeroOnPondIce(Hero);
	if (bOn && !bOnIce)
	{
		EnterIce(Hero);
	}
	else if (!bOn && bOnIce && !M->Skater.bDown)
	{
		LeaveIce(Hero);
	}
	// skates on land: he totters
	const bool bTotter = bWorn && !bOnIce && !bSwimming && !Hero->IsSeated();
	if (bTotter != bLandScaled && !bSpeedHeld)
	{
		bLandScaled = bTotter;
		Hero->SetSpeedScale(bTotter ? (float)M->P.LandScale : 1.f);
		if (bTotter)
		{
			Hero->SetAnimSpeedScale(1.4f);
		}
		else if (!bOnIce)
		{
			Hero->SetAnimSpeedScale(1.f);
		}
	}
	if (!bOnIce)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Hero->GetController());
	UCharacterMovementComponent* Move = Hero->GetCharacterMovement();
	const FVector L = Hero->GetActorLocation();
	const float Half = Hero->GetCapsuleComponent() ? Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	const FVector Feet = L - FVector(0.0, 0.0, Half);
	FSkater& K = M->Skater;
	// a kerb or somebody in the way stopped him: the model takes the body's real speed
	const FVector Real = Hero->GetVelocity() / 100.0;
	const double RealV = FVector(Real.X, Real.Y, 0.0).Size();
	if (!K.bDown && K.Speed() > 0.8 && RealV < K.Speed() * 0.55)
	{
		K.Vel = V2(Real.X, Real.Y);
	}
	FSkateIn In;
	In.Dt = Dt;
	In.bSkates = bWorn;
	In.bShift = PC && (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift));
	const FVector Wish = (PC && PC->IsMoveInputIgnored()) ? FVector::ZeroVector : Hero->GetLastMovementInputVector();
	In.Wish = V2(Wish.X, Wish.Y);
	if (bLastFeet)
	{
		const V2 A = ToPond(M->P, LastFeet.X / 100.0, LastFeet.Y / 100.0), B = ToPond(M->P, Feet.X / 100.0, Feet.Y / 100.0);
		In.bCrack = (B - A).Len() > 0.01 && CrossesCrack(M->Cracks, A, B);
	}
	LastFeet = Feet;
	bLastFeet = true;
	const ESkateEvent Ev = StepSkate(K, In, M->P);
	Move->Velocity.X = K.Vel.X * 100.0;
	Move->Velocity.Y = K.Vel.Y * 100.0;
	switch (Ev)
	{
	case ESkateEvent::Stroke:
		if (bWorn) PlaySfx(TEXT("skate_stroke"), Feet, 0.5f + 0.1f * (float)K.Speed());
		break;
	case ESkateEvent::Crack:
		PlaySfx(TEXT("ice_crack"), Feet, 0.8f);
		break;
	case ESkateEvent::Stop:
		PlaySfx(TEXT("skate_stop"), Feet, 0.9f);
		break;
	case ESkateEvent::Fall:
	{
		PlaySfx(TEXT("ice_crack"), Feet, 0.7f);
		PlaySfx(TEXT("ice_fall"), Feet, 1.f);
		Hero->SetCrouchToggled(true);
		if (PC)
		{
			PC->SetIgnoreMoveInput(true);
			if (!FallCam && PC->PlayerCameraManager)
			{
				FallCam = Cast<ULyudIceFallCamera>(PC->PlayerCameraManager->AddNewCameraModifier(ULyudIceFallCamera::StaticClass()));
			}
		}
		if (FallCam)
		{
			FallCam->FallS = (float)M->P.FallS;
			FallCam->GetupS = (float)M->P.GetupS;
			FallCam->Side = (M->Rng.Next() & 1) ? 1.f : -1.f;
			FallCam->Phase = 0.f;
		}
		bWasDown = true;
		SayOne(TEXT("winter.skate.fall"), 5);
		Stamp(TEXT("fell_on_ice"));
		Event(TEXT("fall"));
		break;
	}
	case ESkateEvent::GetUp:
		Hero->SetCrouchToggled(false);
		if (PC)
		{
			PC->SetIgnoreMoveInput(false);
		}
		bWasDown = false;
		if (FallCam)
		{
			FallCam->Phase = -1.f;
		}
		break;
	default:
		break;
	}
	if (FallCam && K.bDown)
	{
		FallCam->Phase += Dt;
	}
	// the legs: slow long strides while pushing, still while gliding
	const double V = K.Speed();
	Hero->SetAnimSpeedScale(!bWorn ? 0.8f : (K.bDown ? 0.f : (K.bStroking ? 0.32f : (V > 0.6 ? 0.f : 1.f))));
	// the blades' hiss on the ice
	if (bWorn && !K.bDown && V > 0.4)
	{
		if (!Glide && Holder)
		{
			Glide = NewObject<UAudioComponent>(Holder, TEXT("SkateGlide"));
			Glide->bAutoActivate = false;
			Glide->bAutoDestroy = false;
			Glide->SetSound(Sound(TEXT("/Game/Audio/Winter/S_skate_glide_01.S_skate_glide_01")));
			Glide->RegisterComponent();
		}
		if (Glide && Glide->Sound)
		{
			GlideVolume = FMath::FInterpTo(GlideVolume, FMath::Clamp((float)V / (float)M->P.MaxSpeed, 0.1f, 1.f) * 0.6f, Dt, 6.f);
			Glide->SetWorldLocation(Feet);
			if (!Glide->IsPlaying())
			{
				Glide->Play();
			}
			Glide->SetVolumeMultiplier(GlideVolume);
			Glide->SetPitchMultiplier(0.85f + 0.06f * (float)V);
		}
	}
	else if (Glide && Glide->IsPlaying())
	{
		GlideVolume = FMath::FInterpTo(GlideVolume, 0.f, Dt, 8.f);
		Glide->SetVolumeMultiplier(GlideVolume);
		if (GlideVolume < 0.02f)
		{
			Glide->Stop();
		}
	}
	// how far he has skated, his longest glide: stamps
	if (bWorn && !K.bDown)
	{
		SkatedM += (float)(V * Dt);
		if (!bFirstGlide && SkatedM > 50.f)
		{
			bFirstGlide = true;
			Stamp(TEXT("first_glide"));
			Say(TEXT("winter.skate.first"));
			Event(TEXT("skate"));
		}
		if (!bLongGlide && K.BestGlideM >= 25.0)
		{
			bLongGlide = true;
			Stamp(TEXT("long_glide"));
			Say(TEXT("winter.skate.long_glide"));
			Event(TEXT("long_glide"));
		}
	}
}

// ------------------------------------------------------------------ the banya

void ULyudWinterPlay::TickBanya(AHeroCharacter* Hero, float Dt)
{
	FRoom& Room = M->Room;
	Room.Step(M->P, Dt);
	APlayerController* PC = Cast<APlayerController>(Hero->GetController());
	ULyudInventory* Inv = ULyudInventory::Get(this);
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	VenikT = FMath::Max(0.f, VenikT - Dt);
	if (bInSteam)
	{
		const bool bVenik = bVenikThisVisit || (Inv && Inv->Count(TEXT("birch_venik")) > 0);
		SetPromptKey(bVenik ? TEXT("winter.banya.prompt_steam_venik") : TEXT("winter.banya.prompt_steam"));
		if (PC && Pending == 0 && (!UI || !UI->IsOpen()))
		{
			if (ANpcTalkDirector::UsePressed(PC))
			{
				Room.Ladle(M->P);
				PlaySfx(TEXT("banya_ladle"), BanyaPoint(M->P.Stove) + FVector(0.0, 0.0, 95.0), 1.f);
				if (Room.Steam > 0.75)
				{
					SayOne(TEXT("winter.banya.steam"), 3);
				}
			}
			if (PC->WasInputKeyJustPressed(EKeys::G) && VenikT <= 0.f)
			{
				if (!bVenikThisVisit && Inv && Inv->Remove(TEXT("birch_venik"), 1))
				{
					bVenikThisVisit = true;
					Stamp(TEXT("venik"));
					Event(TEXT("venik"));
				}
				if (bVenikThisVisit)
				{
					M->Body.Heat += M->P.VenikHeat;
					VenikT = (float)M->P.VenikS;
					PlaySfx(TEXT("venik_swish"), Hero->GetActorLocation(), 1.f);
				}
				else
				{
					Say(TEXT("winter.banya.no_venik"));
				}
			}
		}
		if (bExitAsked && Pending == 0)
		{
			bExitAsked = false;
			Pending = bFaint ? 3 : 2;
			PendingT = 0.4f;
			if (PC && PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.35f, FLinearColor::Black, false, true);
			}
		}
		return;
	}
	// at the door, outside
	const FVector Door = BanyaPoint(M->P.Door);
	const FVector Front = FRotator(0.f, (float)M->P.BanyaYaw, 0.f).Vector();
	const FVector Stand = Door + Front * 80.0;
	const FVector L = Hero->GetActorLocation();
	bNearDoor = FVector::Dist2D(L, Stand) < 170.0 && FMath::Abs(L.Z - Stand.Z - 90.0) < 150.0;
	if (!bNearDoor)
	{
		return;
	}
	// this near the door the snow's code leaves the prompt alone (TickSnowman), so the door's prompt, or the steam room's
	// he has just come out of, is taken down here when it no longer holds
	if (Hero->IsSeated() || Pending != 0 || (UI && UI->IsOpen()))
	{
		if (Rolling < 0)
		{
			SetPrompt(FText());
		}
		return;
	}
	FVector Eye;
	FRotator View;
	if (PC)
	{
		PC->GetPlayerViewPoint(Eye, View);
		if (FVector::DotProduct(View.Vector(), (Door + FVector(0.0, 0.0, 120.0) - Eye).GetSafeNormal()) < 0.4)
		{
			if (Rolling < 0)
			{
				SetPrompt(FText());
			}
			return;
		}
	}
	if (!BanyaOpenNow())
	{
		SetPromptKey(TEXT("winter.banya.closed"));
		return;
	}
	SetPromptKey(TEXT("winter.banya.prompt_door"), M->P.BanyaPrice);
	if (!PC)
	{
		return;
	}
	if (PC->WasInputKeyJustPressed(EKeys::T) && UI)
	{
		TArray<FLyudShopGood> Goods;
		for (const FParams::FGood& G : M->P.BanyaShop)
		{
			FLyudShopGood Gd;
			Gd.Item = FName(*Str(G.Item));
			Gd.Price = G.Price;
			Gd.Count = G.Count;
			Goods.Add(Gd);
		}
		UI->OpenShop(LyudText::Get(TEXT("winter.banya.shop")), Goods);
		return;
	}
	if (ANpcTalkDirector::UsePressed(PC))
	{
		if (!Inv || !Inv->Spend(M->P.BanyaPrice))
		{
			Say(TEXT("winter.banya.no_money"), { FString::FromInt(M->P.BanyaPrice) });
			return;
		}
		PlaySfx(TEXT("banya_door"), Door, 1.f);
		Pending = 1;
		PendingT = 0.4f;
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.35f, FLinearColor::Black, false, true);
		}
	}
}

void ULyudWinterPlay::EnterBanya(AHeroCharacter* Hero)
{
	if (!ShelfSeat)
	{
		return;
	}
	const FParams& P = M->P;
	if (Hero->IsSeated())
	{
		Hero->StandUp();
	}
	if (bOnIce)
	{
		LeaveIce(Hero);
	}
	// up on the upper shelf, facing the stove; the move keys stay on the shelf, Space asks to go out
	Hero->SitOn(ShelfSeat, FTransform::Identity, (float)P.Shelf[2] * 100.f);
	Hero->SeatMove = [](const FVector2D&) {};
	TWeakObjectPtr<ULyudWinterPlay> Self(this);
	Hero->SeatLeave = [Self]()
	{
		if (Self.IsValid())
		{
			Self->bExitAsked = true;
		}
		return false;
	};
	if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
	{
		PC->SetControlRotation(FRotator(-8.f, (float)P.BanyaYaw, 0.f));
	}
	M->Room.Reset(P);
	bInSteam = true;
	bVenikThisVisit = false;
	bFaint = false;
	bExitAsked = false;
	SayOne(TEXT("winter.banya.enter"), 3);
	Stamp(TEXT("steam"));
	Event(TEXT("banya"));
}

void ULyudWinterPlay::ExitBanya(bool bFainted)
{
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (!Hero)
	{
		return;
	}
	const FParams& P = M->P;
	if (bInSteam)
	{
		Hero->SeatLeave = nullptr;
		Hero->SeatMove = nullptr;
		if (Hero->IsSeated())
		{
			Hero->StandUp();
		}
		bInSteam = false;
		// out onto the porch, facing the sea
		const FVector Front = FRotator(0.f, (float)P.BanyaYaw, 0.f).Vector();
		FVector Out = BanyaPoint(P.Door) + Front * 130.0;
		const float Half = Hero->GetCapsuleComponent() ? Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WinterPlayOut), false, Hero);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Out + FVector(0.0, 0.0, 300.0), Out - FVector(0.0, 0.0, 600.0), ECC_Visibility, Q))
		{
			Out.Z = Hit.ImpactPoint.Z;
		}
		Hero->TeleportTo(Out + FVector(0.0, 0.0, Half + 2.0), FRotator(0.f, (float)P.BanyaYaw, 0.f));
		if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
		{
			PC->SetControlRotation(FRotator(0.f, (float)P.BanyaYaw, 0.f));
		}
		PlaySfx(TEXT("banya_door"), BanyaPoint(P.Door), 1.f);
		if (bFainted)
		{
			Say(TEXT("winter.banya.faint"));
		}
		else if (M->Body.bHot)
		{
			Say(SnowCm >= (float)P.ManMinCm ? TEXT("winter.banya.out_snow") : TEXT("winter.banya.out_sea"));
		}
		// the stoker's coal for a snowman, once a day when there is snow
		const int32 DayKey = Year * 1000 + Month * 40 + Day;
		if (CoalDay != DayKey && SnowCm >= (float)P.ManMinCm)
		{
			CoalDay = DayKey;
			if (ULyudInventory* Inv = ULyudInventory::Get(this))
			{
				if (Inv->Count(TEXT("coal")) == 0)
				{
					Inv->Add(TEXT("coal"), 1);
					Say(TEXT("winter.banya.coal"));
				}
			}
		}
		bFaint = false;
		return;
	}
	if (bFainted)
	{
		// hauled out of the cold water by the walruses onto the pier's deck
		const float Half = Hero->GetCapsuleComponent() ? Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
		FVector Deck = Cm(P.DeckStand);
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WinterPlayDeck), false, Hero);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Deck + FVector(0.0, 0.0, 500.0), Deck - FVector(0.0, 0.0, 800.0), ECC_Visibility, Q))
		{
			Deck.Z = Hit.ImpactPoint.Z;
		}
		Hero->TeleportTo(Deck + FVector(0.0, 0.0, Half + 2.0), FRotator(0.f, 90.f, 0.f));
		M->Body.Cold = 0.75;
		M->Body.TowelT = 40.0;
		Say(TEXT("winter.walrus.hauled"));
	}
}

void ULyudWinterPlay::TickBody(AHeroCharacter* Hero, float Dt)
{
	FBody& B = M->Body;
	const FParams& P = M->P;
	const ULyudSwimComponent* Swim = Hero->FindComponentByClass<ULyudSwimComponent>();
	const bool bSwimming = Swim && Swim->IsSwimming();
	const FVector L = Hero->GetActorLocation();
	ECool Cool = ECool::Air;
	if (!bInSteam)
	{
		if (bSwimming)
		{
			Cool = ECool::Water;
		}
		else if (Hero->GetCrouchAlpha() > 0.6f && !bOnIce)
		{
			const ULyudWinterSubsystem* Winter = ULyudWinterSubsystem::Get(this);
			const float Half = Hero->GetCapsuleComponent() ? Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
			if (Winter && Winter->GetSnowDepthAt(L - FVector(0.0, 0.0, Half)) >= (float)P.ManMinCm)
			{
				Cool = ECool::Snow;
			}
		}
	}
	// leaving the water with a towel: a rub-down
	if (M->bWasSwimming && !bSwimming && (B.Cold > 0.15 || B.Heat > 0.0))
	{
		const ULyudInventory* Inv = ULyudInventory::Get(this);
		if (Inv && Inv->Count(TEXT("banya_towel")) > 0)
		{
			B.TowelT = 45.0;
			Say(TEXT("winter.walrus.towel"));
		}
	}
	M->bWasSwimming = bSwimming;
	if (B.Heat <= 0.0 && B.Cold <= 0.0 && !bInSteam && !(bSwimming && SeaC <= P.ColdSeaC))
	{
		return;
	}
	if (Cool == ECool::Snow && B.bHot && !bSnowSaid)
	{
		bSnowSaid = true;
		SayOne(TEXT("winter.banya.snow"), 3);
	}
	const EBodyEvent Ev = StepBody(B, M->Room, P, bInSteam ? EWhere::Steam : EWhere::Outside, ULyudInventory::Get(this) && ULyudInventory::Get(this)->IsWorn(TEXT("banya_hat")),
		Cool, AirC, SeaC, Dt);
	switch (Ev)
	{
	case EBodyEvent::TooHot:
		Say(TEXT("winter.banya.too_hot"));
		break;
	case EBodyEvent::MustLeave:
		if (bInSteam && !bExitAsked && Pending == 0)
		{
			bFaint = true;
			bExitAsked = true;
		}
		break;
	case EBodyEvent::Round:
	{
		bSnowSaid = false;
		const bool bHole = bHoleOpen && FVector::Dist2D(L, Cm(P.HoleAt)) < 600.0;
		Say(TEXT("winter.banya.round"), { FString::FromInt(B.Rounds) });
		if (B.LastRoundBy == ECool::Snow)
		{
			Stamp(TEXT("snow_roll"));
		}
		else if (bHole)
		{
			Stamp(TEXT("ice_hole"));
		}
		Event(TEXT("round"));
		if (B.Rounds == P.RoundsFull)
		{
			Stamp(TEXT("three_rounds"));
			Say(TEXT("winter.banya.full"));
			Event(TEXT("banya_full"));
		}
		break;
	}
	case EBodyEvent::SwimCounted:
	{
		// a winter swim: New Year's morning, or with the walruses' club on a Sunday
		if (IsNewYearMorning(P, Month, Day, Hour))
		{
			const long long Winter = WinterOf(Year, Month);
			if (M->Save.SwamNewYear != Winter)
			{
				M->Save.SwamNewYear = Winter;
				Stamp(TEXT("new_year_swim"));
				Say(TEXT("winter.walrus.new_year"));
				Event(TEXT("new_year_swim"));
			}
		}
		else if (WalrusHour(P, Month, Day, Weekday, Hour))
		{
			int32 Near = 0;
			if (const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this))
			{
				const int32 Act = People->FindActivity(TEXT("walrus_swim"));
				for (int32 Id = 0; Act >= 0 && Id < People->NumPeople(); Id++)
				{
					int32 A = -1;
					bool bDoing = false;
					EPersonPose Pose = EPersonPose::Hidden;
					FVector At;
					if (People->GetActivityState(Id, A, bDoing, Pose, At) && A == Act && FVector::Dist2D(At, L) < P.ClubRadiusM * 100.0)
					{
						Near++;
					}
				}
			}
			if (Near >= P.ClubMin)
			{
				Stamp(TEXT("walrus_club"));
				Say(TEXT("winter.walrus.club"));
				Event(TEXT("walrus_club"));
			}
			else
			{
				Say(TEXT("winter.walrus.counted"));
			}
		}
		else
		{
			Say(TEXT("winter.walrus.counted"));
		}
		Event(TEXT("swim"));
		break;
	}
	case EBodyEvent::Cold:
		Say(TEXT("winter.walrus.cold"));
		break;
	case EBodyEvent::ColdOut:
		if (Pending == 0)
		{
			Pending = 3;
			PendingT = 0.5f;
			bInSteam = false;
			if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
			{
				if (PC->PlayerCameraManager)
				{
					PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.45f, FLinearColor::Black, false, true);
				}
			}
		}
		break;
	default:
		break;
	}
}

void ULyudWinterPlay::TickHole(float Dt)
{
	(void)Dt;
	ULyudWinterSubsystem* Winter = ULyudWinterSubsystem::Get(this);
	float Top = 0.f;
	const FVector Hole = Cm(M->P.HoleAt);
	const bool bIce = Winter && Winter->GetIceTop(Top) && Winter->GetIceAt(Hole) > 0.5f;
	if (bIce != bHoleOpen)
	{
		bHoleOpen = bIce;
		if (Winter)
		{
			Winter->SetIceHole(TEXT("walrus"), Hole, bIce);
		}
		for (UStaticMeshComponent* C : HoleParts)
		{
			if (C)
			{
				C->SetVisibility(bIce);
			}
		}
	}
	if (bIce && HoleParts.Num() && HoleParts[0])
	{
		// the frame and the water ride the ice's top (the tide moves it)
		USceneComponent* Root = HoleParts[0]->GetAttachParent();
		if (Root && FMath::Abs(Root->GetComponentLocation().Z - Top) > 1.5)
		{
			Root->SetWorldLocation(FVector(Hole.X, Hole.Y, Top));
		}
	}
}

// ------------------------------------------------------------------ snow

float ULyudWinterPlay::PacksAt(const FVector& Feet) const
{
	const ULyudWinterSubsystem* Winter = ULyudWinterSubsystem::Get(this);
	if (!Winter)
	{
		return 0.f;
	}
	return (float)Packs(M->P, Winter->GetSnowDepthAt(Feet), AirC, Melt);
}

void ULyudWinterPlay::TickSnow(AHeroCharacter* Hero, float Dt)
{
	ThrowCooldown = FMath::Max(0.f, ThrowCooldown - Dt);
	APlayerController* PC = Cast<APlayerController>(Hero->GetController());
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	const float Half = Hero->GetCapsuleComponent() ? Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	const FVector Feet = Hero->GetActorLocation() - FVector(0.0, 0.0, Half);
	const bool bFree = PC && !bInSteam && Pending == 0 && (!UI || !UI->IsOpen()) && !Hero->IsSeated() && !M->Skater.bDown;
	// a scoop under way: down for the snow, then the throw
	if (ScoopT >= 0.f)
	{
		ScoopT += Dt;
		if (ScoopT >= (float)M->P.ScoopS)
		{
			ScoopT = -1.f;
			Hero->SetReachCrouch(0.f);
			ThrowFromHero(Hero);
		}
	}
	else if (bFree && PC->WasInputKeyJustPressed(EKeys::G) && ThrowCooldown <= 0.f)
	{
		// G is the bird feed's throw while feed is in the hand
		const FLyudItemDef* Def = Inv ? ULyudInventory::FindDef(Inv->EquippedItem()) : nullptr;
		const bool bFeed = Def && Def->Kind == ELyudItemKind::Feed;
		const UCharacterMovementComponent* Move = Hero->GetCharacterMovement();
		if (!bFeed && Move && Move->IsMovingOnGround() && Rolling < 0)
		{
			if (PacksAt(Feet) > 0.05f)
			{
				ScoopT = 0.f;
				Hero->SetReachCrouch(0.85f);
				PlaySfx(TEXT("snow_scoop"), Feet, 0.8f);
			}
			else if (SnowCm >= 1.f && FightLineT <= 0.f)
			{
				const ULyudWinterSubsystem* Winter = ULyudWinterSubsystem::Get(this);
				const float Depth = Winter ? Winter->GetSnowDepthAt(Feet) : 0.f;
				if (Depth >= 1.f)
				{
					Say(AirC < M->P.PackTemp[0] ? TEXT("winter.snow.too_cold") : (Depth < M->P.PackCm ? TEXT("winter.snow.too_thin") : TEXT("winter.snow.no_pack")));
					ThrowCooldown = 6.f;
				}
			}
		}
	}
	TickBalls(Hero, Dt);
	TickFight(Hero, Dt);
	TickSnowman(Hero, Dt);
}

void ULyudWinterPlay::ThrowFromHero(AHeroCharacter* Hero)
{
	APlayerController* PC = Cast<APlayerController>(Hero->GetController());
	if (!PC)
	{
		return;
	}
	FVector Eye;
	FRotator View;
	PC->GetPlayerViewPoint(Eye, View);
	FRotator Aim = View;
	Aim.Pitch = FMath::Clamp(Aim.Pitch + (float)M->P.UpDeg, -60.f, 80.f);
	const FVector Dir = Aim.Vector();
	const FVector Right = FRotationMatrix(View).GetUnitAxis(EAxis::Y);
	const FVector From = Eye + View.Vector() * 45.0 + Right * 22.0 - FVector(0.0, 0.0, 14.0);
	Throw(From, Dir * M->P.BallSpeed * 100.0 + Hero->GetVelocity() * 0.5, -1);
	PlaySfx(TEXT("snowball_throw"), From, 0.8f);
	ThrowCooldown = (float)M->P.CooldownS;
	M->Fight.HeroThrown++;
	Event(TEXT("snowball"));
}

void ULyudWinterPlay::Throw(const FVector& From, const FVector& Vel, int32 Owner)
{
	if (M->Balls.size() >= 16)
	{
		return;
	}
	FBall B;
	B.Pos = Metres(From);
	B.Vel = V3(Vel.X / 100.0, Vel.Y / 100.0, Vel.Z / 100.0);
	B.Owner = Owner;
	M->Balls.push_back(B);
}

void ULyudWinterPlay::Burst(const FVector& At)
{
	PlaySfx(TEXT("snowball_hit"), At, 0.8f);
	// a puff: a few crumbs of snow flying out for a moment (pooled)
	int32 Made = 0;
	for (int32 I = 0; I < PuffPool.Num() && Made < 6; I++)
	{
		bool bBusy = false;
		for (const FPuff& F : Puffs)
		{
			bBusy |= F.Part == I;
		}
		if (bBusy || !PuffPool[I])
		{
			continue;
		}
		FPuff F;
		F.Part = I;
		F.Vel = FVector(M->Rng.Range(-1.0, 1.0), M->Rng.Range(-1.0, 1.0), M->Rng.Range(0.3, 1.4)) * 180.0;
		PuffPool[I]->SetWorldLocation(At);
		PuffPool[I]->SetWorldScale3D(FVector(1.f));
		PuffPool[I]->SetVisibility(true);
		Puffs.Add(F);
		Made++;
	}
}

void ULyudWinterPlay::TickBalls(AHeroCharacter* Hero, float Dt)
{
	// crumbs of the bursts
	for (int32 I = Puffs.Num() - 1; I >= 0; I--)
	{
		FPuff& F = Puffs[I];
		F.Age += Dt;
		UStaticMeshComponent* C = PuffPool.IsValidIndex(F.Part) ? PuffPool[F.Part].Get() : nullptr;
		if (!C || F.Age > 0.45f)
		{
			if (C)
			{
				C->SetVisibility(false);
			}
			Puffs.RemoveAtSwap(I);
			continue;
		}
		F.Vel.Z -= 980.0 * Dt;
		C->SetWorldLocation(C->GetComponentLocation() + F.Vel * Dt);
		C->SetWorldScale3D(FVector(FMath::Max(0.05f, 1.f - F.Age / 0.45f)));
	}
	if (M->Balls.empty())
	{
		for (UStaticMeshComponent* C : BallPool)
		{
			if (C && C->IsVisible())
			{
				C->SetVisibility(false);
			}
		}
		return;
	}
	UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	const FVector HeroAt = Hero->GetActorLocation();
	const FParams& P = M->P;
	for (FBall& B : M->Balls)
	{
		const V3 Prev = B.Pos;
		StepBall(B, P, Dt);
		const FVector A = Cm(Prev), Bc = Cm(B.Pos);
		// what it hits: the hero (a child's ball), a child (his), else the world
		if (B.Owner >= 0)
		{
			if (PassDistance(Prev, B.Pos, Metres(HeroAt + FVector(0.0, 0.0, 30.0))) < 0.5)
			{
				B.bDone = true;
				Splat = 1.f;
				M->Fight.KidHits++;
				Burst(HeroAt + FVector(0.0, 0.0, 60.0));
				if (M->Fight.KidHits % 3 == 1)
				{
					SayOne(TEXT("winter.fight.hit_me"), 4);
				}
				continue;
			}
		}
		else if (People)
		{
			for (int32 Kid : Kids)
			{
				int32 Act = -1;
				bool bDoing = false;
				EPersonPose Pose = EPersonPose::Hidden;
				FVector At;
				if (!People->GetActivityState(Kid, Act, bDoing, Pose, At) || Pose == EPersonPose::Hidden || FVector::DistSquared(At, Bc) > 9.0e4)
				{
					continue;
				}
				if (PassDistance(Prev, B.Pos, Metres(At + FVector(0.0, 0.0, 90.0))) < P.HitM)
				{
					B.bDone = true;
					M->Fight.HeroHits++;
					Burst(At + FVector(0.0, 0.0, 100.0));
					People->ReactManner(Kid, TEXT("clap"), 1.2f);
					if (M->Fight.Join(P, Kid, WorldTime))
					{
						SayOne(TEXT("winter.fight.join"), 3);
					}
					break;
				}
			}
			if (B.bDone)
			{
				continue;
			}
		}
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WinterPlayBall), false, B.Owner < 0 ? static_cast<AActor*>(Hero) : nullptr);
		if (GetWorld()->LineTraceSingleByChannel(Hit, A, Bc, ECC_Visibility, Q))
		{
			B.bDone = true;
			Burst(Hit.ImpactPoint);
			// a ball of his landing near children at play draws them in
			if (B.Owner < 0 && People)
			{
				for (int32 Kid : Kids)
				{
					int32 Act = -1;
					bool bDoing = false;
					EPersonPose Pose = EPersonPose::Hidden;
					FVector At;
					if (People->GetActivityState(Kid, Act, bDoing, Pose, At) && Pose != EPersonPose::Hidden && Pose != EPersonPose::Sit
						&& Pose != EPersonPose::Sleep && FVector::Dist(At, Hit.ImpactPoint) < P.JoinM * 100.0 * 0.5)
					{
						if (M->Fight.Join(P, Kid, WorldTime))
						{
							SayOne(TEXT("winter.fight.join"), 3);
						}
					}
				}
			}
		}
	}
	M->Balls.erase(std::remove_if(M->Balls.begin(), M->Balls.end(), [](const FBall& B) { return B.bDone; }), M->Balls.end());
	for (int32 I = 0; I < BallPool.Num(); I++)
	{
		UStaticMeshComponent* C = BallPool[I];
		if (!C)
		{
			continue;
		}
		if (I < (int32)M->Balls.size())
		{
			C->SetWorldLocation(Cm(M->Balls[(size_t)I].Pos));
			if (!C->IsVisible())
			{
				C->SetVisibility(true);
			}
		}
		else if (C->IsVisible())
		{
			C->SetVisibility(false);
		}
	}
}

void ULyudWinterPlay::StartFightNow()
{
	UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (!People || !Hero)
	{
		return;
	}
	if (!bKidsRead)
	{
		TickFight(Hero, 0.f);
	}
	// the nearest children wherever they are
	TArray<TPair<double, int32>> Near;
	for (int32 Kid : Kids)
	{
		int32 Act = -1;
		bool bDoing = false;
		EPersonPose Pose = EPersonPose::Hidden;
		FVector At;
		if (People->GetActivityState(Kid, Act, bDoing, Pose, At) && Pose != EPersonPose::Hidden)
		{
			Near.Add({ FVector::Dist(At, Hero->GetActorLocation()), Kid });
		}
	}
	Near.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key < B.Key; });
	for (int32 I = 0; I < Near.Num() && I < M->P.MaxKids; I++)
	{
		M->Fight.Join(M->P, Near[I].Value, WorldTime);
	}
	UE_LOG(LogLyudWinterPlay, Display, TEXT("WinterPlay: a snowball fight with %d children (nearest %.0f m)"), (int32)M->Fight.Kids.size(),
		Near.Num() ? Near[0].Key / 100.0 : -1.0);
}

void ULyudWinterPlay::TickFight(AHeroCharacter* Hero, float Dt)
{
	(void)Dt;
	UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	if (!People)
	{
		return;
	}
	if (!bKidsRead && People->IsRunning() && People->NumPeople() > 0)
	{
		bKidsRead = true;
		for (int32 Id = 0; Id < People->NumPeople(); Id++)
		{
			FPersonInfo Info;
			if (People->GetPerson(Id, Info) && Info.Age >= M->P.KidAges[0] && Info.Age <= M->P.KidAges[1])
			{
				Kids.Add(Id);
			}
		}
	}
	FFight& F = M->Fight;
	if (!F.bOn)
	{
		return;
	}
	const FParams& P = M->P;
	const FVector HeroAt = Hero->GetActorLocation();
	const FVector HeroVel = Hero->GetVelocity();
	// children who wandered off or went indoors drop out
	for (int32 I = (int32)F.Kids.size() - 1; I >= 0; I--)
	{
		const int32 Id = F.Kids[(size_t)I].Id;
		int32 Act = -1;
		bool bDoing = false;
		EPersonPose Pose = EPersonPose::Hidden;
		FVector At;
		if (!People->GetActivityState(Id, Act, bDoing, Pose, At) || Pose == EPersonPose::Hidden || FVector::Dist(At, HeroAt) > P.LeaveM * 100.0)
		{
			F.Drop(Id);
		}
	}
	const bool bSnow = PacksAt(HeroAt - FVector(0.0, 0.0, 90.0)) > 0.02f || SnowCm >= (float)P.PackCm;
	if (F.Over(WorldTime) || !bSnow)
	{
		FightLine = LyudText::Get(TEXT("winter.fight.score"), { FString::FromInt(F.HeroHits), FString::FromInt(F.KidHits) });
		FightLineT = 7.f;
		if (F.HeroHits >= 3)
		{
			Stamp(TEXT("snowball_fight"));
		}
		Event(TEXT("snowball_fight"));
		F.End();
		return;
	}
	for (int32 Id : F.Due(P, WorldTime))
	{
		int32 Act = -1;
		bool bDoing = false;
		EPersonPose Pose = EPersonPose::Hidden;
		FVector At;
		if (!People->GetActivityState(Id, Act, bDoing, Pose, At) || FVector::Dist(At, HeroAt) > P.FightRadius * 100.0)
		{
			continue;
		}
		const V3 From = Metres(At + FVector(0.0, 0.0, 115.0));
		const V3 Target = Metres(HeroAt + FVector(0.0, 0.0, 20.0));
		const double Dist = (Target - From).Len();
		V3 Vel;
		Aim(From, Target, Metres(HeroVel), P.KidSpeed, P.Gravity, FFight::ErrorFor(P, Dist), &F.Rng, Vel);
		Throw(Cm(From), Cm(Vel), Id);
		People->ReactManner(Id, TEXT("point"), 0.7f);
		PlaySfx(TEXT("snowball_throw"), Cm(From), 0.6f);
	}
}

void ULyudWinterPlay::TickSnowman(AHeroCharacter* Hero, float Dt)
{
	APlayerController* PC = Cast<APlayerController>(Hero->GetController());
	ULyudInventory* Inv = ULyudInventory::Get(this);
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	const ULyudWinterSubsystem* Winter = ULyudWinterSubsystem::Get(this);
	const FParams& P = M->P;
	if (!PC || !Winter || bInSteam || bOnIce || Pending != 0 || (UI && UI->IsOpen()) || Hero->IsSeated())
	{
		if (Rolling >= 0 && (bInSteam || bOnIce || Hero->IsSeated()))
		{
			Rolling = -1;
			if (RollPart) RollPart->SetVisibility(false);
			if (bSpeedHeld) { Hero->SetSpeedScale(1.f); bSpeedHeld = false; }
		}
		if (!bInSteam && !bNearDoor)
		{
			SetPrompt(FText());
		}
		return;
	}
	const float Half = Hero->GetCapsuleComponent() ? Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	const FVector Feet = Hero->GetActorLocation() - FVector(0.0, 0.0, Half);
	const FVector Fwd = FRotator(0.f, Hero->GetActorRotation().Yaw, 0.f).Vector();
	const bool bUse = ANpcTalkDirector::UsePressed(PC);
	auto Ground = [Winter, this](const FVector& At)
	{
		float Z = 0.f;
		if (Winter->GetSnowSurfaceAt(At, Z))
		{
			return (double)Z;
		}
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WinterPlaySnow), false);
		if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0.0, 0.0, 200.0), At - FVector(0.0, 0.0, 400.0), ECC_Visibility, Q))
		{
			return Hit.ImpactPoint.Z;
		}
		return At.Z;
	};
	// the snowman nearest his feet (its foot within reach), and whether a rolled ball could go on it
	int32 Near = -1;
	double NearD = 1e18;
	for (int32 K = 0; K < (int32)M->Save.Snowmen.size(); K++)
	{
		const FSnowman& Sm = M->Save.Snowmen[(size_t)K];
		const FVector At = Cm(Sm.At);
		const double D = FVector::Dist2D(At, Rolling >= 0 ? RollAt : Feet) - Sm.R[0] * 100.0;
		if (D < NearD)
		{
			NearD = D;
			Near = K;
		}
	}
	if (Rolling >= 0)
	{
		// the ball goes ahead of him, growing as it rolls over the snow
		const FVector Want = Feet + Fwd * (45.0 + RollR * 100.0);
		const FVector Moved = Want - RollAt;
		const double Dist = FVector(Moved.X, Moved.Y, 0.0).Size() / 100.0;
		const float Depth = Winter->GetSnowDepthAt(Want);
		if (Dist > 0.001 && Depth >= (float)P.PackCm && PacksAt(Want) > 0.02f)
		{
			RollR = (float)GrowBall(P, RollR, Dist, Depth);
		}
		const FVector Axis = FVector::CrossProduct(FVector::UpVector, Moved.GetSafeNormal2D());
		if (RollPart && Dist > 0.0005)
		{
			const FQuat Spin(Axis.IsNearlyZero() ? FVector::RightVector : Axis.GetSafeNormal(), (float)(Dist / FMath::Max(0.05f, RollR)));
			RollPart->SetWorldRotation(Spin * RollPart->GetComponentQuat());
		}
		RollAt = FVector(Want.X, Want.Y, Ground(Want) + RollR * 100.0 * 0.92);
		if (RollPart)
		{
			RollPart->SetWorldLocation(RollAt);
			RollPart->SetWorldScale3D(FVector(RollR));
		}
		const bool bOnto = Near >= 0 && NearD < RollR * 100.0 + 45.0 && M->Save.Snowmen[(size_t)Near].Balls > 0 && M->Save.Snowmen[(size_t)Near].Balls < 3;
		const bool bFits = bOnto && CanLift(P, RollR, M->Save.Snowmen[(size_t)Near].R[M->Save.Snowmen[(size_t)Near].Balls - 1]);
		SetPromptKey(bOnto ? (bFits ? TEXT("winter.snowman.prompt_lift") : TEXT("winter.snowman.prompt_too_big")) : TEXT("winter.snowman.prompt_leave"),
			FMath::RoundToInt(RollR * 200.f));
		if (!bUse)
		{
			return;
		}
		if (bOnto && !bFits)
		{
			Say(TEXT("winter.snowman.too_big"));
			return;
		}
		if (bOnto)
		{
			FSnowman& Sm = M->Save.Snowmen[(size_t)Near];
			Sm.R[Sm.Balls++] = RollR;
			PlaySfx(TEXT("snow_pat"), RollAt, 1.f);
			if (Sm.Done())
			{
				Stamp(TEXT("snowman"));
				Say(TEXT("winter.snowman.done"));
				Event(TEXT("snowman"));
			}
			ShowSnowman(Near);
		}
		else
		{
			if ((int32)M->Save.Snowmen.size() >= P.MaxSaved)
			{
				M->Save.Snowmen.erase(M->Save.Snowmen.begin());
			}
			FSnowman Sm;
			Sm.At = Metres(FVector(RollAt.X, RollAt.Y, Ground(RollAt)));
			Sm.Yaw = Hero->GetActorRotation().Yaw + 180.0;
			Sm.R[0] = RollR;
			Sm.Balls = 1;
			M->Save.Snowmen.push_back(Sm);
			PlaySfx(TEXT("snow_pat"), RollAt, 0.8f);
			HideSnowmen();
			for (int32 K = 0; K < (int32)M->Save.Snowmen.size(); K++)
			{
				ShowSnowman(K);
			}
		}
		Rolling = -1;
		if (RollPart)
		{
			RollPart->SetVisibility(false);
		}
		if (bSpeedHeld)
		{
			Hero->SetSpeedScale(1.f);
			bSpeedHeld = false;
		}
		return;
	}
	if (bNearDoor)
	{
		return;
	}
	// a snowman to dress: twig arms (always to hand), a carrot nose, coal eyes
	if (Near >= 0 && NearD < 110.0 && M->Save.Snowmen[(size_t)Near].Done() && !M->Save.Snowmen[(size_t)Near].Dressed())
	{
		FSnowman& Sm = M->Save.Snowmen[(size_t)Near];
		const bool bCarrot = Inv && Inv->Count(TEXT("carrot")) > 0, bCoal = Inv && Inv->Count(TEXT("coal")) > 0;
		const TCHAR* Key = !Sm.bArms ? TEXT("winter.snowman.prompt_arms") : (!Sm.bNose && bCarrot ? TEXT("winter.snowman.prompt_nose")
			: (Sm.Eyes < 2 && bCoal ? TEXT("winter.snowman.prompt_eyes") : nullptr));
		if (Key)
		{
			SetPromptKey(Key);
			if (bUse)
			{
				if (!Sm.bArms) Sm.bArms = true;
				else if (!Sm.bNose && Inv->Remove(TEXT("carrot"), 1)) Sm.bNose = true;
				else if (Sm.Eyes < 2 && Inv->Remove(TEXT("coal"), 1)) Sm.Eyes = 2;
				PlaySfx(TEXT("snow_pat"), Cm(Sm.At) + FVector(0.0, 0.0, 120.0), 0.6f);
				if (Sm.Dressed())
				{
					Say(TEXT("winter.snowman.dressed"));
					Event(TEXT("snowman_dressed"));
				}
				ShowSnowman(Near);
			}
			return;
		}
		SetPromptKey(!Sm.bNose ? TEXT("winter.snowman.want_carrot") : TEXT("winter.snowman.want_coal"));
		return;
	}
	// crouched on snow that packs: start a ball
	if (Hero->GetCrouchAlpha() > 0.6f && Winter->GetSnowDepthAt(Feet) >= (float)P.ManMinCm && PacksAt(Feet) > 0.05f)
	{
		SetPromptKey(TEXT("winter.snowman.prompt_start"));
		if (bUse)
		{
			Rolling = 1;
			RollR = (float)P.StartR;
			RollAt = Feet + Fwd * (45.0 + RollR * 100.0);
			RollAt.Z = Ground(RollAt) + RollR * 100.0;
			if (RollPart)
			{
				RollPart->SetWorldLocation(RollAt);
				RollPart->SetWorldScale3D(FVector(RollR));
				RollPart->SetVisibility(true);
			}
			if (!bLandScaled)
			{
				Hero->SetSpeedScale(0.5f);
				bSpeedHeld = true;
			}
			Hero->SetCrouchToggled(false);
			PlaySfx(TEXT("snow_pat"), RollAt, 0.7f);
		}
		return;
	}
	SetPrompt(FText());
}

// ------------------------------------------------------------------ the HUD

void ULyudWinterPlay::ShowHud(bool bShow)
{
	if (!GEngine || !GEngine->GameViewport)
	{
		HudWidget.Reset();
		return;
	}
	if (bShow && !HudWidget)
	{
		HudWidget = SNew(SLyudWinterHud).Owner(this);
		GEngine->GameViewport->AddViewportWidgetContent(HudWidget.ToSharedRef(), 56);
	}
	else if (!bShow && HudWidget)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(HudWidget.ToSharedRef());
		HudWidget.Reset();
	}
}

// ------------------------------------------------------------------ console and developer mode

namespace
{
	ULyudWinterPlay* WinterPlayOf(UWorld* World) { return World ? World->GetSubsystem<ULyudWinterPlay>() : nullptr; }

	FAutoConsoleCommandWithWorldAndArgs GWinterPlayReport(TEXT("lyud.WinterPlay.Report"), TEXT("Winter play's state to the log (docs/winter_play.md)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* W)
		{
			if (ULyudWinterPlay* S = WinterPlayOf(W)) UE_LOG(LogLyudWinterPlay, Display, TEXT("%s"), *S->Report());
		}));
	FAutoConsoleCommandWithWorldAndArgs GWinterPlayIce(TEXT("lyud.WinterPlay.Ice"), TEXT("lyud.WinterPlay.Ice <cm>: the rink's ice forced this thick (-1: the weather's)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			if (ULyudWinterPlay* S = WinterPlayOf(W)) S->ForceIce(Args.Num() ? FCString::Atof(*Args[0]) : 18.f);
		}));
	FAutoConsoleCommandWithWorldAndArgs GWinterPlayKit(TEXT("lyud.WinterPlay.Kit"), TEXT("Skates, a venik, the banya hat and towel, a carrot and coal into the pocket."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* W)
		{
			if (ULyudWinterPlay* S = WinterPlayOf(W)) S->GiveKit();
		}));
	FAutoConsoleCommandWithWorldAndArgs GWinterPlayFight(TEXT("lyud.WinterPlay.Fight"), TEXT("A snowball fight with the nearest children now."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* W)
		{
			if (ULyudWinterPlay* S = WinterPlayOf(W)) S->StartFightNow();
		}));

	LyudDev::FAutoAction GDevKit(TEXT("winter.kit"), TEXT("winter"), TEXT("dev.action.winter.kit"),
		[](UWorld* W) { if (ULyudWinterPlay* S = WinterPlayOf(W)) S->GiveKit(); });
	LyudDev::FAutoAction GDevIce(TEXT("winter.rink"), TEXT("winter"), TEXT("dev.action.winter.rink"),
		[](UWorld* W) { if (ULyudWinterPlay* S = WinterPlayOf(W)) { float Cm = 0.f; S->ForceIce(S->RinkReport(Cm) == 1 ? -1.f : 18.f); } },
		[](UWorld* W) { float Cm = 0.f; const ULyudWinterPlay* S = WinterPlayOf(W); return S && S->RinkReport(Cm) == 1; });
	LyudDev::FAutoAction GDevFight(TEXT("winter.fight"), TEXT("winter"), TEXT("dev.action.winter.fight"),
		[](UWorld* W) { if (ULyudWinterPlay* S = WinterPlayOf(W)) S->StartFightNow(); });
	LyudDev::FAutoAction GDevHeat(TEXT("winter.heat"), TEXT("winter"), TEXT("dev.action.winter.heat"),
		[](UWorld* W) { if (ULyudWinterPlay* S = WinterPlayOf(W)) S->HeatUp(); });
	LyudDev::FAutoAction GDevReport(TEXT("winter.report"), TEXT("winter"), TEXT("dev.action.winter.report"),
		[](UWorld* W) { if (ULyudWinterPlay* S = WinterPlayOf(W)) UE_LOG(LogLyudWinterPlay, Display, TEXT("%s"), *S->Report()); });
}
