#include "South/LyudLighthouse.h"

#include "City/SLyudCityPage.h"
#include "Dev/LyudDev.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Food/LyudHandUse.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "South/LighthouseModel.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NpcDialogue.h"
#include "Plants/GameCalendar.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sky/Ephemeris.h"
#include "South/LyudSouthSpot.h"
#include "TimerManager.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "Weather/LyudWeather.h"

int32 ALyudLighthouse::Forced = -1;
float ALyudLighthouse::ForcedVisibilityM = -1.f;

namespace LyudLighthouseLocal
{
	namespace LH = LyudLighthouse;

	const TCHAR* LhLensMesh = TEXT("/Game/South/Meshes/SM_South_Lighthouse_lens.SM_South_Lighthouse_lens");
	const TCHAR* LhBeamMesh = TEXT("/Game/South/Meshes/SM_South_Lighthouse_beam.SM_South_Lighthouse_beam");
	const TCHAR* LhTowerMesh = TEXT("SM_South_Lighthouse");
	const FName LhGlassSlot(TEXT("M_South_LanternGlass"));
	const FName LhLensSlot(TEXT("M_South_LensGlass"));
	const FName LhGlow(TEXT("Glow"));
	const FName LhStrength(TEXT("Strength"));
	const FName LhQuest(TEXT("lighthouse_paraffin"));
	/** The tower is open to visitors these hours (before the errand is done). */
	constexpr float LhOpenFrom = 8.f, LhOpenTo = 21.f;
	/** The fade of a climb: out, the move, back in (s). */
	constexpr float LhFadeOut = 0.35f, LhFadeIn = 0.45f;
	/** The spot light at full: candela, cone (deg). */
	constexpr float LhSpotCandela = 2.2e6f;
	constexpr float LhSpotOuterDeg = 3.6f, LhSpotInnerDeg = 1.2f;
	/** The days the log goes back, and its readings' hours. */
	constexpr int32 LhLogDays = 7;
	constexpr int32 LhReadingHours[3] = { 8, 14, 20 };
	/** The coast's clock is UTC+2 at 54.96 N, 20.40 E (as the people's sun: PeopleLife's PeopleSunTimes). */
	constexpr double LhLat = 54.96, LhLon = 20.40;

	/** Sunrise and sunset of a day, local hours. */
	void LhSunTimes(const FDateTime& Day, float& Rise, float& Set)
	{
		Rise = 7.f;
		Set = 17.f;
		const FDateTime Midnight(Day.GetYear(), Day.GetMonth(), Day.GetDay());
		double Prev = -90.0;
		for (int32 M = 0; M <= 24 * 60; M += 4)
		{
			const FDateTime Utc = Midnight + FTimespan::FromMinutes(M) - FTimespan::FromHours(2);
			const double Alt = LyudEphemeris::Sun(Utc, LhLat, LhLon).AltitudeDeg;
			if (Prev < -0.83 && Alt >= -0.83)
			{
				Rise = M / 60.f;
			}
			if (Prev >= -0.83 && Alt < -0.83 && M > 0)
			{
				Set = M / 60.f;
			}
			Prev = Alt;
		}
	}

	float LhDayHour(const FDateTime& T)
	{
		return T.GetHour() + T.GetMinute() / 60.f + T.GetSecond() / 3600.f;
	}

	AHeroCharacter* LhHeroOf(const UWorld* World, APlayerController*& PC)
	{
		PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	}

	/** Of the lighthouse's spots in reach the nearest has the prompt (as the coast's seats do). */
	uint64 LhGFrame = 0;
	float LhGBestDist = 0.f;
	TWeakObjectPtr<const AActor> LhGBest;
	TWeakObjectPtr<const AActor> LhGWinner;

	bool LhClaimNearest(const AActor* Spot, float Dist)
	{
		if (LhGFrame != GFrameCounter)
		{
			LhGWinner = LhGFrame + 1 == GFrameCounter ? LhGBest : nullptr;
			LhGBest = nullptr;
			LhGFrame = GFrameCounter;
		}
		if (!LhGBest.IsValid() || Dist < LhGBestDist)
		{
			LhGBest = Spot;
			LhGBestDist = Dist;
		}
		return LhGWinner.Get() == Spot;
	}

	/** How near the hero's feet are to a spot (in plan, cm) and whether at its level. */
	bool LhNear(const AHeroCharacter* H, const FVector& Here, float Reach, float& Dist)
	{
		const float Half = H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector Feet = H->GetActorLocation() - FVector(0.0, 0.0, Half);
		Dist = float(FVector::Dist2D(Feet, Here));
		return Dist < Reach && FMath::Abs(Feet.Z - Here.Z) < 160.0;
	}

	FVector LhVec(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Field, A) && A->Num() == 3)
		{
			return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
		}
		return FVector::ZeroVector;
	}

	FString LhClock(float Hour)
	{
		Hour = FMath::Fmod(Hour + 24.f, 24.f);
		const int32 H = FMath::FloorToInt(Hour);
		const int32 M = FMath::Clamp(FMath::RoundToInt((Hour - H) * 60.f), 0, 59);
		return FString::Printf(TEXT("%d:%02d"), H, M);
	}

	LH::FReading LhReadingOf(const FLyudWeatherState& S, float Hour)
	{
		LH::FReading R;
		R.Hour = Hour;
		R.WindFromDeg = S.WindFromDeg;
		R.WindMs = S.WindSpeed;
		R.PressureHPa = S.PressureHPa;
		R.TendencyHPa3h = S.PressureTendency;
		R.TempC = S.TempC;
		R.Cloud = S.CloudCover;
		R.PrecipMmH = S.PrecipRate;
		R.SnowFraction = S.SnowFraction;
		R.Thunder = S.Thunder;
		R.Fog = S.Fog;
		R.VisibilityM = S.VisibilityM;
		R.SeaIceCover = S.SeaIceCover;
		return R;
	}

	FAutoConsoleCommand GForceCmd(TEXT("lyud.Lighthouse.Force"),
		TEXT("lyud.Lighthouse.Force on|off|auto: the Mayak lighthouse's lamp burns / is out / follows the sun and the fog"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const FString A = Args.Num() ? Args[0].ToLower() : FString(TEXT("auto"));
			ALyudLighthouse::Forced = A == TEXT("on") || A == TEXT("1") ? 1 : (A == TEXT("off") || A == TEXT("0") ? 0 : -1);
			UE_LOG(LogTemp, Display, TEXT("LIGHTHOUSE force %d"), ALyudLighthouse::Forced);
		}));

	FAutoConsoleCommand GVisCmd(TEXT("lyud.Lighthouse.Vis"),
		TEXT("lyud.Lighthouse.Vis <m>|-1: the visibility the lighthouse's light sees (fog tests); -1 = the weather's"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			ALyudLighthouse::ForcedVisibilityM = Args.Num() ? FCString::Atof(*Args[0]) : -1.f;
			UE_LOG(LogTemp, Display, TEXT("LIGHTHOUSE visibility %.0f"), ALyudLighthouse::ForcedVisibilityM);
		}));

	// F7 panel, group "lighthouse": the lamp on (else back to the sun), a thick fog for the beam and the horn (else the weather's)
	LyudDev::FAutoAction GDevLamp(TEXT("lighthouse.lamp"), TEXT("lighthouse"), TEXT("dev.action.lighthouse.lamp"),
		[](UWorld*) { ALyudLighthouse::Forced = ALyudLighthouse::Forced == 1 ? -1 : 1; },
		[](UWorld*) { return ALyudLighthouse::Forced == 1; });
	LyudDev::FAutoAction GDevFog(TEXT("lighthouse.fog"), TEXT("lighthouse"), TEXT("dev.action.lighthouse.fog"),
		[](UWorld*) { ALyudLighthouse::ForcedVisibilityM = ALyudLighthouse::ForcedVisibilityM >= 0.f ? -1.f : 400.f; },
		[](UWorld*) { return ALyudLighthouse::ForcedVisibilityM >= 0.f; });

	FAutoConsoleCommandWithWorldAndArgs GReportCmd(TEXT("lyud.Lighthouse.Report"), TEXT("lyud.Lighthouse.Report: logs a LIGHTHOUSE line"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			const ULyudLighthouseSubsystem* S = ULyudLighthouseSubsystem::Get(World);
			const ALyudLighthouse* L = S ? S->GetLight() : nullptr;
			UE_LOG(LogTemp, Display, TEXT("LIGHTHOUSE %s"), L ? *L->Report() : TEXT("none in this level"));
		}));

	FAutoConsoleCommandWithWorldAndArgs GClimbCmd(TEXT("lyud.Lighthouse.Climb"),
		TEXT("lyud.Lighthouse.Climb up|down: the hero goes up onto the gallery / down to the door (from anywhere, locked or not)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = nullptr;
			AHeroCharacter* H = LhHeroOf(World, PC);
			const ULyudLighthouseSubsystem* S = ULyudLighthouseSubsystem::Get(World);
			const bool bUp = Args.Num() == 0 || Args[0].ToLower() != TEXT("down");
			ALyudLighthouseStair* St = S ? S->GetStair(bUp) : nullptr;
			const bool bOk = H && St && St->Go(H, true);
			UE_LOG(LogTemp, Display, TEXT("LIGHTHOUSE climb %s: %s"), bUp ? TEXT("up") : TEXT("down"), bOk ? TEXT("ok") : TEXT("FAILED"));
		}));

	FAutoConsoleCommandWithWorldAndArgs GGotoCmd(TEXT("lyud.Lighthouse.Goto"),
		TEXT("lyud.Lighthouse.Goto base|gallery|log: puts the hero at the tower's door, on the gallery or at the weather log"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = nullptr;
			AHeroCharacter* H = LhHeroOf(World, PC);
			const ULyudLighthouseSubsystem* S = ULyudLighthouseSubsystem::Get(World);
			if (!H || !S)
			{
				return;
			}
			const FString Where = Args.Num() ? Args[0].ToLower() : FString(TEXT("base"));
			const AActor* At = Where == TEXT("log") ? static_cast<const AActor*>(S->GetLog()) : static_cast<const AActor*>(S->GetStair(Where != TEXT("gallery")));
			if (!At)
			{
				return;
			}
			const float Half = H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			H->TeleportTo(At->GetActorLocation() + FVector(0.0, 0.0, Half + 5.0), At->GetActorRotation(), false, true);
			PC->SetControlRotation(At->GetActorRotation());
			UE_LOG(LogTemp, Display, TEXT("LIGHTHOUSE goto %s at %s"), *Where, *At->GetActorLocation().ToCompactString());
		}));

	FAutoConsoleCommandWithWorldAndArgs GLogCmd(TEXT("lyud.Lighthouse.Log"),
		TEXT("lyud.Lighthouse.Log [days back]: prints a page of the keeper's weather log; 'open' opens it on screen"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const ULyudLighthouseSubsystem* S = ULyudLighthouseSubsystem::Get(World);
			ALyudLighthouseLog* L = S ? S->GetLog() : nullptr;
			if (!L)
			{
				UE_LOG(LogTemp, Display, TEXT("LIGHTHOUSE log: none in this level"));
				return;
			}
			if (Args.Num() && Args[0] == TEXT("open"))
			{
				L->Use();
				return;
			}
			FText Title, Body;
			L->PageText(Args.Num() ? FCString::Atoi(*Args[0]) : 0, Title, Body);
			UE_LOG(LogTemp, Display, TEXT("LIGHTHOUSE log: %s\n%s"), *Title.ToString(), *Body.ToString());
		}));
}

using namespace LyudLighthouseLocal;

// ---------------------------------------------------------------------------------------------------------------------
// the light

ALyudLighthouse::ALyudLighthouse()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Turn = CreateDefaultSubobject<USceneComponent>(TEXT("Turn"));
	Turn->SetupAttachment(RootComponent);
	Turn->SetMobility(EComponentMobility::Movable);

	auto Part = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Turn);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		// a moving mesh stays out of Lumen and the distance fields (docs/perf_checklist.md)
		C->bAffectDistanceFieldLighting = false;
		C->bAffectDynamicIndirectLighting = false;
		C->bVisibleInRayTracing = false;
		C->SetGenerateOverlapEvents(false);
		C->bReceivesDecals = false;
		return C;
	};
	Lens = Part(TEXT("Lens"));
	Beam = Part(TEXT("Beam"));
	Beam->SetVisibility(false);

	Spot = CreateDefaultSubobject<USpotLightComponent>(TEXT("Spot"));
	Spot->SetupAttachment(Turn);
	Spot->SetMobility(EComponentMobility::Movable);
	Spot->SetRelativeRotation(FRotator(-1.5f, 0.f, 0.f));
	Spot->SetIntensityUnits(ELightUnits::Candelas);
	Spot->SetIntensity(0.f);
	Spot->SetInnerConeAngle(LhSpotInnerDeg);
	Spot->SetOuterConeAngle(LhSpotOuterDeg);
	Spot->SetAttenuationRadius(LH::SpotReachM * 100.f);
	Spot->SetLightColor(FLinearColor(1.f, 0.88f, 0.68f));
	// one light, out of Lumen: no indirect light, no shadows, no volumetric fog (the cards are the beam)
	Spot->SetCastShadows(false);
	Spot->SetIndirectLightingIntensity(0.f);
	Spot->SetVolumetricScatteringIntensity(0.f);
	Spot->SetAffectTranslucentLighting(false);
	Spot->bAffectsWorld = true;
	Spot->SetVisibility(false);
}

void ALyudLighthouse::BeginPlay()
{
	Super::BeginPlay();
	if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, LhLensMesh))
	{
		Lens->SetStaticMesh(M);
		const int32 Slot = Lens->GetMaterialIndex(LhLensSlot);
		LensMID = Lens->CreateDynamicMaterialInstance(Slot >= 0 ? Slot : 0);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("LIGHTHOUSE: no lens mesh %s (import_south.sh)"), LhLensMesh);
	}
	if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, LhBeamMesh))
	{
		Beam->SetStaticMesh(M);
		BeamMID = Beam->CreateDynamicMaterialInstance(0);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("LIGHTHOUSE: no beam mesh %s (import_south.sh)"), LhBeamMesh);
	}
	SampleSky();
	Level = bLampTime ? 1.f : 0.f;      // a game loaded at night finds it burning
	ShowParts(Level > 0.f);
	SetActorTickInterval(Level > 0.f ? 0.f : 1.f);
}

void ALyudLighthouse::BindTower(UStaticMeshComponent* Tower)
{
	if (!Tower)
	{
		return;
	}
	const int32 Slot = Tower->GetMaterialIndex(LhGlassSlot);
	if (Slot < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("LIGHTHOUSE: the tower mesh has no %s slot (rebuild lighthouse.glb, import_south.sh)"), *LhGlassSlot.ToString());
		return;
	}
	GlassMID = Tower->CreateDynamicMaterialInstance(Slot);
	SetGlass = -1.f;
}

void ALyudLighthouse::SampleSky()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FDateTime Now = UGameCalendar::NowIn(World);
	const int32 Day = Now.GetYear() * 400 + Now.GetDayOfYear();
	if (Day != SunDay)
	{
		SunDay = Day;
		LhSunTimes(Now, Sunrise, Sunset);
	}
	VisibilityM = 30000.f;
	Rain = 0.f;
	if (const ULyudWeatherSubsystem* W = ULyudWeatherSubsystem::Get(World))
	{
		const FLyudWeatherState S = W->GetState();
		VisibilityM = S.VisibilityM;
		Rain = W->GetSky().Rain;
	}
	if (ForcedVisibilityM >= 0.f)
	{
		VisibilityM = ForcedVisibilityM;
	}
	bLampTime = Forced >= 0 ? Forced == 1 : LH::LampTime(LhDayHour(Now), Sunrise, Sunset, VisibilityM);
}

void ALyudLighthouse::ShowParts(bool bShow)
{
	if (bShow == bShown)
	{
		return;
	}
	bShown = bShow;
	Beam->SetVisibility(bShow);
	Spot->SetVisibility(bShow);
}

void ALyudLighthouse::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double T = World->GetTimeSeconds();
	if (T >= NextSample)
	{
		NextSample = T + 0.5;
		SampleSky();
	}
	const float Dt = GetActorTickInterval() > 0.f ? GetActorTickInterval() : DeltaTime;
	Level = LH::StepLevel(Level, bLampTime, Dt);
	const bool bLit = Level > 0.f;
	ShowParts(bLit);
	// by day: a look once a second and nothing turns (the keeper stops the clockwork)
	SetActorTickInterval(bLit || bLampTime ? 0.f : 1.f);

	const LH::FBeam B = LH::BeamFor(VisibilityM, Rain, Level);
	const float TurnDeg = LH::TurnDeg(T);
	if (bLit)
	{
		Turn->SetRelativeRotation(FRotator(0.f, TurnDeg, 0.f));
	}
	// the cards: their strength and length only when they change
	if (BeamMID && FMath::Abs(B.Card - SetBeam) > 0.005f)
	{
		SetBeam = B.Card;
		BeamMID->SetScalarParameterValue(LhStrength, B.Card);
	}
	if (FMath::Abs(B.LengthM - SetLength) > 1.f)
	{
		SetLength = B.LengthM;
		Beam->SetRelativeScale3D(FVector(B.LengthM));
	}
	if (FMath::Abs(B.Spot - SetSpot) > 0.01f)
	{
		SetSpot = B.Spot;
		Spot->SetIntensity(LhSpotCandela * B.Spot);
		Spot->SetAttenuationRadius(FMath::Min(LH::SpotReachM, FMath::Max(80.f, VisibilityM)) * 100.f);
	}
	// the glass: the steady glow and the flash when a panel faces the camera
	float Glass = 0.f;
	if (bLit)
	{
		const APlayerController* PC = World->GetFirstPlayerController();
		const FVector Cam = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : GetActorLocation();
		const FVector To = Cam - GetActorLocation();
		const float ViewYaw = FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X));
		Glass = LH::GlassGlow(LH::OffBeam(TurnDeg, ViewYaw), Level);
	}
	if (GlassMID && FMath::Abs(Glass - SetGlass) > 0.01f)
	{
		SetGlass = Glass;
		GlassMID->SetScalarParameterValue(LhGlow, Glass);
	}
	if (LensMID && FMath::Abs(Level - SetLens) > 0.01f)
	{
		SetLens = Level;
		LensMID->SetScalarParameterValue(LhGlow, Level);
	}
}

FString ALyudLighthouse::Report() const
{
	const LH::FBeam B = LH::BeamFor(VisibilityM, Rain, Level);
	return FString::Printf(TEXT("at %s level %.2f lamp_time %d (sun %.2f..%.2f, lit %.2f out %.2f) vis %.0f m card %.2f length %.0f m spot %.2f glass %d lens %d beam %d horn %.2f"),
		*GetActorLocation().ToCompactString(), Level, bLampTime ? 1 : 0, Sunrise, Sunset, LH::LitAt(Sunset), LH::OutAt(Sunrise),
		VisibilityM, B.Card, B.LengthM, B.Spot, GlassMID ? 1 : 0, Lens->GetStaticMesh() ? 1 : 0, Beam->GetStaticMesh() ? 1 : 0,
		LH::FogHorn(VisibilityM));
}

// ---------------------------------------------------------------------------------------------------------------------
// the ways up and down

ALyudLighthouseStair::ALyudLighthouseStair()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Tags.Add(TEXT("South"));
}

void ALyudLighthouseStair::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetPrompt(nullptr);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ALyudLighthouseStair::SetAwake(bool bInAwake, bool bInNear)
{
	Super::SetAwake(bInAwake, bInNear);
	SetActorTickEnabled(bNear);
	if (!bNear)
	{
		SetPrompt(nullptr);
	}
}

void ALyudLighthouseStair::SetPrompt(const TCHAR* Key)
{
	if (Key == Prompted)
	{
		return;
	}
	Prompted = Key;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, Key ? LyudText::Get(Key) : FText::GetEmpty());
	}
}

bool ALyudLighthouseStair::IsOpen() const
{
	if (!bUp)
	{
		return true;        // nobody is locked up on the gallery
	}
	const ULyudQuests* Q = ULyudQuests::Get(this);
	if (Q && Q->State(LhQuest) == ELyudQuestState::Done)
	{
		return true;
	}
	const float H = LhDayHour(UGameCalendar::NowIn(GetWorld()));
	return H >= LhOpenFrom && H < LhOpenTo;
}

bool ALyudLighthouseStair::Go(AHeroCharacter* H, bool bForce)
{
	UWorld* World = GetWorld();
	if (!H || !World || ArriveAt > 0.0 || H->IsSeated() || H->IsOnBike())
	{
		return false;
	}
	if (!bForce && !IsOpen())
	{
		if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
		{
			Screen->Toast(LyudText::Get(TEXT("lh.tower.locked")));
		}
		UE_LOG(LogTemp, Display, TEXT("LIGHTHOUSE stair: locked"));
		return false;
	}
	if (ULyudHandUse* Hand = ULyudHandUse::Get(this))
	{
		Hand->PutAway();
	}
	Climber = H;
	SetPrompt(nullptr);
	APlayerController* PC = Cast<APlayerController>(H->GetController());
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, LhFadeOut, FLinearColor::Black, false, true);
	}
	ArriveAt = World->GetTimeSeconds() + LhFadeOut;
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateUObject(this, &ALyudLighthouseStair::Arrive), LhFadeOut + 0.05f, false);
	return true;
}

void ALyudLighthouseStair::Arrive()
{
	ArriveAt = 0.0;
	AHeroCharacter* H = Climber.Get();
	Climber.Reset();
	if (!H)
	{
		// the climber is gone (a level change, a load): never leave the screen black
		APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		if (PC && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, LhFadeIn, FLinearColor::Black, false, false);
		}
		return;
	}
	const float Half = H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	H->TeleportTo(To + FVector(0.0, 0.0, Half + 3.0), FRotator(0.f, ToYaw, 0.f), false, true);
	if (APlayerController* PC = Cast<APlayerController>(H->GetController()))
	{
		PC->SetControlRotation(FRotator(bUp ? -4.f : 0.f, ToYaw, 0.f));
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, LhFadeIn, FLinearColor::Black, false, false);
		}
	}
	if (bUp)
	{
		ULyudQuests::Notify(this, TEXT("reach"), TEXT("lighthouse_gallery"));
	}
	UE_LOG(LogTemp, Display, TEXT("LIGHTHOUSE stair %s: at %s"), bUp ? TEXT("up") : TEXT("down"), *To.ToCompactString());
}

void ALyudLighthouseStair::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	APlayerController* PC = nullptr;
	AHeroCharacter* H = LhHeroOf(GetWorld(), PC);
	if (!H || ArriveAt > 0.0)
	{
		SetPrompt(nullptr);
		return;
	}
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	const bool bScreen = UI && UI->IsOpen();
	float Dist = 0.f;
	const bool bMine = LhNear(H, GetActorLocation(), Reach, Dist) && !bScreen && !H->IsSeated() && !H->IsOnBike() && LhClaimNearest(this, Dist);
	SetPrompt(!bMine ? nullptr : (!bUp ? TEXT("ui.lh.down") : (IsOpen() ? TEXT("ui.lh.up") : TEXT("ui.lh.locked"))));
	if (bMine && ANpcTalkDirector::UsePressed(PC))
	{
		Go(H);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// the weather log

ALyudLighthouseLog::ALyudLighthouseLog()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Tags.Add(TEXT("South"));
}

void ALyudLighthouseLog::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetPrompt(nullptr);
	ClosePage();
	Super::EndPlay(EndPlayReason);
}

void ALyudLighthouseLog::SetAwake(bool bInAwake, bool bInNear)
{
	Super::SetAwake(bInAwake, bInNear);
	SetActorTickEnabled(bNear);
	if (!bNear)
	{
		SetPrompt(nullptr);
		ClosePage();
	}
}

void ALyudLighthouseLog::SetPrompt(const TCHAR* Key)
{
	if (Key == Prompted)
	{
		return;
	}
	Prompted = Key;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, Key ? LyudText::Get(Key) : FText::GetEmpty());
	}
}

void ALyudLighthouseLog::Use()
{
	if (PageIndex + 1 < LhLogDays)
	{
		ShowPage(PageIndex + 1);
	}
	else
	{
		ClosePage();
	}
}

void ALyudLighthouseLog::PageText(int32 DaysBack, FText& Title, FText& Body) const
{
	const UWorld* World = GetWorld();
	const ULyudWeatherSubsystem* W = ULyudWeatherSubsystem::Get(World);
	const FDateTime Now = W ? W->WeatherTime() : UGameCalendar::NowIn(World);
	const FDateTime Today(Now.GetYear(), Now.GetMonth(), Now.GetDay());
	const FDateTime Day = Today - FTimespan::FromDays(FMath::Max(DaysBack, 0));
	const float NowH = DaysBack > 0 ? 24.f : LhDayHour(Now);
	Title = LyudText::Get(FString::Printf(TEXT("lh.log.date.%d"), Day.GetMonth()), { FStringFormatArg(Day.GetDay()) });

	TArray<FString> Lines;
	if (DaysBack == 0)
	{
		Lines.Add(LyudText::Get(TEXT("lh.log.head")).ToString());
	}
	std::vector<LH::FReading> Readings;
	for (int32 H : LhReadingHours)
	{
		if (H > NowH)
		{
			break;
		}
		const FLyudWeatherState S = W ? W->GetStateAt(Day + FTimespan::FromHours(H)) : FLyudWeatherState();
		const LH::FReading R = LhReadingOf(S, float(H));
		Readings.push_back(R);
		const int32 Tend = LH::Tendency(R.TendencyHPa3h);
		Lines.Add(LyudText::Get(TEXT("lh.log.reading"), {
			FStringFormatArg(FString::Printf(TEXT("%02d:00"), H)),
			FStringFormatArg(LyudText::Get(FString::Printf(TEXT("lh.log.sky.%d"), LH::SkyCode(R))).ToString()),
			FStringFormatArg(LyudText::Get(FString::Printf(TEXT("lh.log.wind.%d"), LH::Compass8(R.WindFromDeg))).ToString()),
			FStringFormatArg(FMath::RoundToInt(R.WindMs)),
			FStringFormatArg(FMath::RoundToInt(R.PressureHPa)),
			FStringFormatArg(LyudText::Get(Tend < 0 ? TEXT("lh.log.tend.fall") : (Tend > 0 ? TEXT("lh.log.tend.rise") : TEXT("lh.log.tend.steady"))).ToString()),
			FStringFormatArg(FMath::RoundToInt(R.TempC)),
			FStringFormatArg(LyudText::Get(FString::Printf(TEXT("lh.log.vis.%d"), LH::VisClass(R.VisibilityM))).ToString()) }).ToString());
	}
	if (Readings.empty())
	{
		Lines.Add(LyudText::Get(TEXT("lh.log.no_reading")).ToString());
	}
	// the lamp: put out this morning, lit this evening (today only what is past)
	float Rise = 8.f, Set = 17.f;
	LhSunTimes(Day, Rise, Set);
	const float Out = LH::OutAt(Rise), Lit = LH::LitAt(Set);
	if (NowH >= Lit)
	{
		Lines.Add(LyudText::Get(TEXT("lh.log.lamp"), { FStringFormatArg(LhClock(Out)), FStringFormatArg(LhClock(Lit)) }).ToString());
	}
	else if (NowH >= Out)
	{
		Lines.Add(LyudText::Get(TEXT("lh.log.lamp_out"), { FStringFormatArg(LhClock(Out)) }).ToString());
	}
	// the horn's hours, sampled every two hours of the part of the day that is past
	std::vector<float> Vis;
	for (int32 H = 1; H < 24 && float(H) <= NowH; H += 2)
	{
		const float V = W ? W->GetStateAt(Day + FTimespan::FromHours(H)).VisibilityM : 30000.f;
		Vis.push_back(V);
		Vis.push_back(V);
	}
	const float Horn = LH::HornHours(Vis);
	if (Horn > 0.f)
	{
		Lines.Add(LyudText::Get(TEXT("lh.log.horn"), { FStringFormatArg(FMath::RoundToInt(Horn)) }).ToString());
	}
	// the keeper's word on a finished day
	if (DaysBack > 0)
	{
		const std::string Key = LH::Remark(Readings, Horn, uint32(Day.GetYear() * 400 + Day.GetDayOfYear()) * 2654435761u >> 7);
		if (!Key.empty())
		{
			Lines.Add(LyudText::Get(TEXT("lh.log.remark.") + FString(UTF8_TO_TCHAR(Key.c_str()))).ToString());
		}
	}
	Body = FText::FromString(FString::Join(Lines, TEXT("\n")));
}

void ALyudLighthouseLog::ShowPage(int32 Index)
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport)
	{
		return;
	}
	if (!Page)
	{
		Page = SNew(SLyudCityPage);
		Viewport->AddViewportWidgetContent(Page.ToSharedRef(), 61);
		Page->SetScreen(false);
	}
	FText Title, Body;
	PageText(Index, Title, Body);
	Page->SetPage(Title, Body, LyudText::Get(Index + 1 < LhLogDays ? TEXT("city.page.next") : TEXT("city.page.close")));
	if (Index == 0)
	{
		ULyudQuests::Notify(this, TEXT("read"), TEXT("lighthouse_log"));
		UE_LOG(LogTemp, Display, TEXT("LIGHTHOUSE log: reading"));
	}
	PageIndex = Index;
}

void ALyudLighthouseLog::ClosePage()
{
	PageIndex = -1;
	if (!Page)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(Page.ToSharedRef());
	}
	Page.Reset();
}

void ALyudLighthouseLog::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	APlayerController* PC = nullptr;
	AHeroCharacter* H = LhHeroOf(GetWorld(), PC);
	if (!H)
	{
		SetPrompt(nullptr);
		ClosePage();
		return;
	}
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	const bool bScreen = UI && UI->IsOpen();
	float Dist = 0.f;
	const bool bClose = LhNear(H, GetActorLocation(), Reach, Dist);
	if (PageIndex >= 0)
	{
		if (!bClose || bScreen)
		{
			ClosePage();
			return;
		}
		SetPrompt(nullptr);
		if (ANpcTalkDirector::UsePressed(PC, true))
		{
			Use();
		}
		return;
	}
	const bool bMine = bClose && !bScreen && !H->IsSeated() && !H->IsOnBike() && LhClaimNearest(this, Dist);
	SetPrompt(bMine ? TEXT("ui.lh.log") : nullptr);
	if (bMine && ANpcTalkDirector::UsePressed(PC))
	{
		Use();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// the spawner

ULyudLighthouseSubsystem* ULyudLighthouseSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<ULyudLighthouseSubsystem>() : nullptr;
}

bool ULyudLighthouseSubsystem::GetHornLocation(FVector& Out) const
{
	Out = Horn;
	return bHorn;
}

void ULyudLighthouseSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// only where the Aton coast is placed (import_south.py's seats and viewer): a dev level gets nothing
	if (!InWorld.IsGameWorld() || !TActorIterator<ALyudSouthSpot>(&InWorld))
	{
		return;
	}
	Spawn(InWorld);
}

void ULyudLighthouseSubsystem::Spawn(UWorld& World)
{
	FString Text;
	TSharedPtr<FJsonObject> Root;
	const FString Path = FPaths::ProjectContentDir() / TEXT("South/lighthouse.json");
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("LIGHTHOUSE: no %s (python3 tools/lighthouse/lighthouse.py)"), *Path);
		return;
	}
	const FVector Lamp = LhVec(Root, TEXT("lamp"));
	Horn = Lamp;
	bHorn = true;
	ALyudLighthouse* L = World.SpawnActor<ALyudLighthouse>(ALyudLighthouse::StaticClass(), FTransform(Lamp));
	Light = L;
	// the placed tower (import_south.py place: tagged South, SM_South_Lighthouse): its lantern glass
	UStaticMeshComponent* Tower = nullptr;
	double Best = 1e18;
	for (TActorIterator<AStaticMeshActor> It(&World); It; ++It)
	{
		UStaticMeshComponent* C = It->GetStaticMeshComponent();
		const UStaticMesh* M = C ? C->GetStaticMesh() : nullptr;
		if (!M || !It->Tags.Contains(TEXT("South")) || !M->GetName().StartsWith(LhTowerMesh) || M->GetName().Contains(TEXT("_lens"))
			|| M->GetName().Contains(TEXT("_beam")))
		{
			continue;
		}
		const double D = FVector::DistSquared2D(It->GetActorLocation(), Lamp);
		if (D < Best)
		{
			Best = D;
			Tower = C;
		}
	}
	if (L)
	{
		L->BindTower(Tower);
	}
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Root->TryGetArrayField(TEXT("stairs"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
			if (!O.IsValid())
			{
				continue;
			}
			const FVector At = LhVec(O, TEXT("at"));
			const FRotator Rot(0.0, O->GetNumberField(TEXT("yaw")), 0.0);
			ALyudLighthouseStair* S = World.SpawnActorDeferred<ALyudLighthouseStair>(ALyudLighthouseStair::StaticClass(), FTransform(Rot, At),
				nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!S)
			{
				continue;
			}
			S->bUp = O->GetStringField(TEXT("id")) == TEXT("up");
			S->To = LhVec(O, TEXT("to"));
			S->ToYaw = (float)O->GetNumberField(TEXT("to_yaw"));
			S->Reach = O->HasField(TEXT("reach")) ? (float)O->GetNumberField(TEXT("reach")) : 120.f;
			S->FinishSpawning(FTransform(Rot, At));
			(S->bUp ? Up : Down) = S;
		}
	}
	const TSharedPtr<FJsonObject>* LogObj = nullptr;
	if (Root->TryGetObjectField(TEXT("log"), LogObj))
	{
		const FVector At = LhVec(*LogObj, TEXT("at"));
		const FRotator Rot(0.0, (*LogObj)->GetNumberField(TEXT("yaw")), 0.0);
		Log = World.SpawnActor<ALyudLighthouseLog>(ALyudLighthouseLog::StaticClass(), FTransform(Rot, At));
	}
	// the gallery's view over the sea: the coast's own viewer spot (E holds the binoculars' view), no stand on the narrow deck
	const TSharedPtr<FJsonObject>* ViewObj = nullptr;
	bool bView = false;
	if (Root->TryGetObjectField(TEXT("view"), ViewObj))
	{
		const FVector At = LhVec(*ViewObj, TEXT("at"));
		const FRotator Rot(0.0, (*ViewObj)->GetNumberField(TEXT("yaw")), 0.0);
		ALyudSouthSpot* V = World.SpawnActorDeferred<ALyudSouthSpot>(ALyudSouthSpot::StaticClass(), FTransform(Rot, At), nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (V)
		{
			V->Kind = TEXT("view");
			V->Reach = 90.f;
			V->FinishSpawning(FTransform(Rot, At));
			bView = true;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("LIGHTHOUSE: light at %s, tower glass %s, stairs %d/%d, log %d, gallery view %d"), *Lamp.ToCompactString(),
		Tower ? TEXT("bound") : TEXT("NOT FOUND"), Up.IsValid() ? 1 : 0, Down.IsValid() ? 1 : 0, Log.IsValid() ? 1 : 0, bView ? 1 : 0);
}
