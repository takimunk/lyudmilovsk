#include "Cruise/LyudCruiseScenery.h"

#include "Audio/LyudSoundscape.h"
#include "Birds/BirdActor.h"
#include "Camera/PlayerCameraManager.h"
#include "City/LyudCityThings.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Cruise/LyudCruise.h"
#include "Cruise/LyudCruiseSeascape.h"
#include "Cruise/LyudMotorShip.h"
#include "Cruise/LyudShipWake.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "North/LyudNorthFx.h"
#include "Plants/GameCalendar.h"
#include "Railway/LyudRailTrack.h"
#include "Sea/LyudSea.h"
#include "SeascapeMath.h"
#include "ShipSeaMath.h"
#include "Sky/LyudSky.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Weather/LyudWind.h"
#include "World/LyudPrefetch.h"

namespace
{
TAutoConsoleVariable<int32> CVarCruiseScenery(TEXT("lyud.Cruise.Scenery"), 1,
	TEXT("The cruise's scenery (the visual ship on the roads, the islands passing the stage ship, her wake, the drifting waves, gulls, sounds): on (1) or off (0)."),
	ECVF_Default);

TAutoConsoleVariable<float> CVarCruiseSway(TEXT("lyud.Cruise.Sway"), 0.5f,
	TEXT("The view's sway aboard the motor ship, 0 (none) .. 1 (0.6 degrees of roll in a fresh sea)."), ECVF_Default);
TAutoConsoleVariable<float> CVarCruiseMaxHs(TEXT("lyud.Cruise.MaxHs"), 1.2f,
	TEXT("While the hero is at the cruise's standing ship the sea grows no higher than this significant height, m (she cannot pitch); 0 = no cap."), ECVF_Default);
TAutoConsoleVariable<int32> CVarCruiseTideHold(TEXT("lyud.Cruise.TideHold"), 1,
	TEXT("1: the sea is held at the datum while the hero is at the cruise's standing ship; 0: the tide is left free (she will hang in the air at low water)."), ECVF_Default);

constexpr double StageNearCm = 250000.0;   // 2.5 km round the stage: the sea is the ship's
constexpr double EyeJumpCm = 50000.0;      // the camera moved this far in a frame: a teleport behind a fade

FAutoConsoleCommandWithWorld CmdSceneryReport(TEXT("lyud.Cruise.SceneryReport"), TEXT("Logs the cruise scenery's state."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (const ULyudCruiseScenery* S = ULyudCruiseScenery::Get(World))
		{
			UE_LOG(LogTemp, Display, TEXT("%s"), *S->Report());
		}
	}));

FAutoConsoleCommandWithWorld CmdFeedGulls(TEXT("lyud.Cruise.FeedGulls"), TEXT("The gulls at the stern are thrown food (as the stern rail's act does)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		ULyudCruiseScenery* S = ULyudCruiseScenery::Get(World);
		const ULyudCruise* Cruise = ULyudCruise::Get(World);
		if (S && Cruise)
		{
			FVector At = Cruise->StageTransform().TransformPosition(FVector(0.0, 0.0, 600.0));
			Cruise->GetShipPoint(TEXT("stern_rail"), At);
			S->FeedGulls(At);
		}
	}));

USoundBase* ShipSound(const TCHAR* Id)
{
	return LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/Ship/S_%s_01.S_%s_01"), Id, Id), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

const TCHAR* const GullFolders[] = { TEXT("/Game/Birds/HerringGull"), TEXT("/Game/Birds/BlackHeadedGull"), TEXT("/Game/Birds/CommonGull") };
constexpr int32 NumSternGulls = 6;
constexpr int32 NumColony = 16;
constexpr float ColonyWithinM = 3000.f;
constexpr float FeedSeconds = 10.f;
}

ULyudCruiseScenery* ULyudCruiseScenery::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudCruiseScenery>() : nullptr;
}

bool ULyudShipSwayCamera::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);
	InOutPOV.Rotation.Roll += RollDeg;
	InOutPOV.Rotation.Pitch += PitchDeg;
	return false;
}

bool ULyudCruiseScenery::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId ULyudCruiseScenery::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudCruiseScenery, STATGROUP_Tickables);
}

void ULyudCruiseScenery::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bBegun = true;
}

void ULyudCruiseScenery::Deinitialize()
{
	if (bAboard)
	{
		GoAshore();
	}
	ReleaseSea();
	if (UWorld* World = GetWorld())
	{
		if (ULyudCityThings* Things = World->GetSubsystem<ULyudCityThings>())
		{
			Things->RemoveActHooks(this);
		}
		if (ULyudCruise* Cruise = ULyudCruise::Get(World))
		{
			Cruise->OnCruiseEvent.RemoveAll(this);
		}
	}
	bBegun = false;
	Super::Deinitialize();
}

void ULyudCruiseScenery::Make()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	MotorShip = World->SpawnActor<ALyudMotorShip>(ALyudMotorShip::StaticClass(), FTransform::Identity, P);
	Seascape = World->SpawnActor<ALyudCruiseSeascape>(ALyudCruiseSeascape::StaticClass(), FTransform::Identity, P);
	if (Seascape)
	{
		Seascape->Setup();
		Wake = NewObject<ULyudShipWake>(Seascape, TEXT("StageWake"));
		Wake->SetupAttachment(Seascape->GetRootComponent());
		Wake->RegisterComponent();
		Seascape->AddInstanceComponent(Wake);
		MakeSounds();
		MakeNavLamps();
	}
	if (ULyudCruise* Cruise = ULyudCruise::Get(World))
	{
		Cruise->OnCruiseEvent.AddUObject(this, &ULyudCruiseScenery::OnCruiseEvent);
	}
	bMade = true;
}

void ULyudCruiseScenery::Tick(float DeltaTime)
{
	const ULyudCruise* Cruise = ULyudCruise::Get(this);
	const bool bOn = Cruise && Cruise->IsRunning() && CVarCruiseScenery.GetValueOnGameThread() != 0;
	if (!bOn)
	{
		if (bAboard)
		{
			GoAshore();
		}
		ReleaseSea();
		return;
	}
	if (!bMade)
	{
		Make();
	}
	TickSeaAtStage(*Cruise, DeltaTime);
	if (!bHooked)
	{
		// the stern rail's act: the city's things tell us, we only send the birds (the act goes on as it is)
		if (ULyudCityThings* Things = GetWorld()->GetSubsystem<ULyudCityThings>())
		{
			Things->AddActHook(ULyudCityThings::FActHook::CreateUObject(this, &ULyudCruiseScenery::OnCityAct));
			bHooked = true;
		}
	}
	const FLyudCruiseView& View = Cruise->View();
	if (View.bHeroAboard != bAboard)
	{
		if (View.bHeroAboard)
		{
			ComeAboard();
		}
		else
		{
			GoAshore();
		}
	}
	if (bAboard)
	{
		TickAboard(View, DeltaTime);
	}
}

// --------------------------------------------------------------------------------------------------- the sea at the hull

void ULyudCruiseScenery::TickSeaAtStage(const ULyudCruise& Cruise, float DeltaTime)
{
	if (!Sea.IsValid())
	{
		Sea = ALyudSea::Get(this);
	}
	ALyudSea* TheSea = Sea.Get();
	const FTransform Stage = Cruise.StageTransform();
	const FVector Mid = Stage.TransformPosition(FVector(3100.0, 0.0, 0.0));
	if (TheSea && PatchSea.Get() != TheSea)
	{
		// the hull's box (62 x 10 m) is kept calm, the open sea comes back over 10 m outside it; her lowest deck is
		// 30 cm over the waterline and nothing masks the sea inside a hull. The patch stands for the whole season.
		PatchSea = TheSea;
		const float Yaw = float(Stage.GetRotation().Rotator().Yaw);
		TheSea->SetCalmFootprint(FVector2D(Mid.X, Mid.Y), Yaw, 3100.f, 500.f, 1000.f, 15.f);
		// belt and braces for deck D's cabins (x 12..46 m, where the hull is at its full breadth): no swimming and no
		// underwater look there whatever the surface does. A box in world axes: only for a stage that is not turned
		if (FMath::Abs(FMath::Sin(FMath::DegreesToRadians(2.f * Yaw))) < 0.02f)
		{
			FBox Dry(ForceInit);
			Dry += Stage.TransformPosition(FVector(1200.0, -400.0, -330.0));
			Dry += Stage.TransformPosition(FVector(4600.0, 400.0, 270.0));
			TheSea->SetDryZone(this, Dry);
		}
	}
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APlayerCameraManager* PCM = PC ? PC->PlayerCameraManager.Get() : nullptr;
	if (!PCM)
	{
		return;
	}
	const FVector Eye = PCM->GetCameraLocation();
	const bool bJump = !bEyeSet || FVector::DistSquared(Eye, LastEye) > FMath::Square(EyeJumpCm);
	LastEye = Eye;
	bEyeSet = true;
	const bool bNear = FVector::DistSquared2D(Eye, Mid) < FMath::Square(StageNearCm);
	if (TheSea)
	{
		// the tide: she is a building at the datum, so while he is here the sea stands at the datum. He comes and goes
		// through a fade (the camera jumps 26 km): at once then; if he ever comes by himself, over a minute
		const bool bHold = bNear && CVarCruiseTideHold.GetValueOnGameThread() != 0;
		if (bHold != bSeaHeld)
		{
			bSeaHeld = bHold;
			TheSea->SetTideHold(this, bHold, bJump ? 0.f : 60.f);
		}
		TheSea->SetSeaStateCap(bNear ? FMath::Max(CVarCruiseMaxHs.GetValueOnGameThread(), 0.f) : 0.f);
	}
	// the view sways a little with the sea while he is in her (not with a panel open: the pointer is shown then)
	const APawn* Pawn = PC->GetPawn();
	const bool bInShip = bNear && Pawn && Cruise.IsInsideHull(Pawn->GetActorLocation(), 300.f);
	const float Scale = FMath::Clamp(CVarCruiseSway.GetValueOnGameThread(), 0.f, 1.f);
	const bool bSway = bInShip && Scale > 0.f && !PC->bShowMouseCursor;
	SwayGain = FMath::FInterpConstantTo(SwayGain, bSway ? 1.f : 0.f, DeltaTime, 0.5f);
	if (SwayGain > 0.f && !SwayCam)
	{
		SwayCam = Cast<ULyudShipSwayCamera>(PCM->AddNewCameraModifier(ULyudShipSwayCamera::StaticClass()));
	}
	if (SwayCam)
	{
		double Roll = 0.0, Pitch = 0.0;
		if (SwayGain > 0.f)
		{
			LyudShipSea::Sway(World->GetTimeSeconds(), TheSea ? TheSea->WaveHeight() * 0.01 : 0.5, Scale * SwayGain, Roll, Pitch);
		}
		SwayCam->RollDeg = float(Roll);
		SwayCam->PitchDeg = float(Pitch);
	}
	// her lights from dusk to dawn: under way the masthead light, the sidelights and the stern light; at anchor (the
	// roads, the night's bay) one white light forward
	if (NavLamps.Num() == 5)
	{
		if (!Sky.IsValid())
		{
			TActorIterator<ALyudSky> It(World);
			if (It)
			{
				Sky = *It;
			}
		}
		const FLyudCruiseView& View = Cruise.View();
		const float Alt = Sky.IsValid() ? Sky->SunAltitude() : 20.f;
		const float Dark = bNear && View.Phase != ELyudCruisePhase::Laid ? FMath::Clamp((4.f - Alt) / 8.f, 0.f, 1.f) : 0.f;
		const bool bWay = (View.Phase == ELyudCruisePhase::Underway || View.Phase == ELyudCruisePhase::Circling) && View.SpeedMS > 0.2f;
		const float Code = Dark + (bWay ? 10.f : 0.f);
		if (FMath::Abs(Code - NavCode) > 0.03f)
		{
			NavCode = Code;
			static const FVector ShipCm[5] = { FVector(5140.0, 0.0, 1233.0), FVector(4115.0, -522.0, 1187.0), FVector(4115.0, 522.0, 1187.0),
				FVector(178.0, 0.0, 663.0), FVector(5095.0, 0.0, 1600.0) };
			for (int32 I = 0; I < 5; I++)
			{
				const bool bOn = Dark > 0.1f && (I == 4 ? !bWay : bWay);
				NavLamps[I]->SetVisibility(bOn);
				if (bOn)
				{
					NavLamps[I]->SetWorldLocation(Stage.TransformPosition(ShipCm[I]));
				}
				if (NavMids.IsValidIndex(I) && NavMids[I])
				{
					NavMids[I]->SetScalarParameterValue(TEXT("Intensity"), bOn ? 40.f * Dark : 0.f);
				}
			}
		}
	}
}

void ULyudCruiseScenery::ReleaseSea()
{
	if (ALyudSea* TheSea = PatchSea.Get())
	{
		TheSea->SetTideHold(this, false, 0.f);
		TheSea->SetSeaStateCap(0.f);
		TheSea->ClearCalmFootprint();
		TheSea->SetDryZone(this, FBox(ForceInit));
	}
	PatchSea.Reset();
	bSeaHeld = false;
	bEyeSet = false;
	SwayGain = 0.f;
	if (SwayCam)
	{
		SwayCam->RollDeg = 0.f;
		SwayCam->PitchDeg = 0.f;
	}
	for (UStaticMeshComponent* L : NavLamps)
	{
		if (L)
		{
			L->SetVisibility(false);
		}
	}
	NavCode = -1.f;
}

void ULyudCruiseScenery::MakeNavLamps()
{
	// masthead (white), port (red), starboard (green), stern (white), anchor (white): small unlit balls at the hull's
	// own lamp boxes (blender/city/buildings/_shipkit.py FITTINGS), no light components, on the seascape's actor
	UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Transport/Materials/M_TR_Glow.M_TR_Glow"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Ball || !Seascape)
	{
		return;
	}
	const FLinearColor Colours[5] = { FLinearColor(1.f, 0.95f, 0.85f), FLinearColor(1.f, 0.05f, 0.03f), FLinearColor(0.05f, 1.f, 0.35f),
		FLinearColor(1.f, 0.95f, 0.85f), FLinearColor(1.f, 0.95f, 0.85f) };
	for (int32 I = 0; I < 5; I++)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Seascape, *FString::Printf(TEXT("StageNavLamp%d"), I));
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Ball);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(false);
		C->bAffectDynamicIndirectLighting = false;
		C->bAffectDistanceFieldLighting = false;
		UMaterialInstanceDynamic* M = Glow ? UMaterialInstanceDynamic::Create(Glow, this) : nullptr;
		if (M)
		{
			M->SetVectorParameterValue(TEXT("Color"), Colours[I]);
			M->SetScalarParameterValue(TEXT("Intensity"), 0.f);
			C->SetMaterial(0, M);
		}
		NavMids.Add(M);
		C->SetupAttachment(Seascape->GetRootComponent());
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		C->SetUsingAbsoluteScale(true);
		C->RegisterComponent();
		C->SetWorldScale3D(FVector(0.16f));
		C->SetVisibility(false);
		Seascape->AddInstanceComponent(C);
		NavLamps.Add(C);
	}
}

void ULyudCruiseScenery::ComeAboard()
{
	bAboard = true;
	if (ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this))
	{
		Scape->SetOpenSea(true);
	}
	// what stands still in the world and would show that the ship does too: the North's far volcanoes and far land
	// (never culled, hundreds of metres high), the volcano's plume, the sea railway's bank and masts (7 km from the stage).
	// The seascape shows the cones, the coast and the railway where the chart has them instead.
	Hidden.Reset();
	HideWorld(true);
	// what comes into the world later (the North spawns its plumes when its data is read, which after a load made
	// aboard may be after this) is hidden as it comes: TickAboard looks again
	HideClock = 2.f;
	HideRound = 0;
	BirdClock = 0.f;
	LastHalfHour = -1;
	BellsLeft = 0;
	SmoothSpeed = 0.f;
	if (const ULyudCruise* Cruise = ULyudCruise::Get(this))
	{
		LastPhase = uint8(Cruise->View().Phase);
	}
}

void ULyudCruiseScenery::HideWorld(bool bStaticMeshes)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// only what is shown now is hidden and remembered: GoAshore shows again exactly what this hid
	auto Hide = [this](AActor* A)
	{
		if (A && !A->IsHidden())
		{
			A->SetActorHiddenInGame(true);
			Hidden.Add(A);
		}
	};
	for (TActorIterator<ALyudRailTrack> It(World); It; ++It)
	{
		Hide(*It);
	}
	for (TActorIterator<ALyudNorthPlume> It(World); It; ++It)
	{
		Hide(*It);
	}
	if (bStaticMeshes)
	{
		// the far volcanoes and the far land: import_north.py's backdrop actors, known by their meshes' names
		const FStringView FarPrefix(TEXT("SM_North_Far"));
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			const UStaticMeshComponent* C = It->GetStaticMeshComponent();
			const UStaticMesh* M = C ? C->GetStaticMesh() : nullptr;
			if (M)
			{
				TStringBuilder<NAME_SIZE> MeshName;
				M->GetFName().ToString(MeshName);
				if (MeshName.ToView().StartsWith(FarPrefix))
				{
					Hide(*It);
				}
			}
		}
	}
}

void ULyudCruiseScenery::GoAshore()
{
	bAboard = false;
	for (const TWeakObjectPtr<AActor>& A : Hidden)
	{
		if (A.IsValid())
		{
			A->SetActorHiddenInGame(false);
		}
	}
	Hidden.Reset();
	if (ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this))
	{
		Scape->SetOpenSea(false);
	}
	if (Sea.IsValid())
	{
		// he leaves through a fade: the pattern may jump back to where the town knows it
		Sea->ResetSurfaceDrift();
	}
	if (Seascape)
	{
		Seascape->HideAll();
	}
	StopSounds();
	DropGulls();
	DropColony();
	FeedLeft = 0.f;
}

void ULyudCruiseScenery::TickAboard(const FLyudCruiseView& View, float DeltaTime)
{
	const ULyudCruise* Cruise = ULyudCruise::Get(this);
	if (!Cruise)
	{
		return;
	}
	const FTransform Stage = Cruise->StageTransform();
	if (!Sea.IsValid())
	{
		Sea = ALyudSea::Get(this);
	}
	if (!Sky.IsValid())
	{
		TActorIterator<ALyudSky> It(GetWorld());
		if (It)
		{
			Sky = *It;
		}
	}
	if (Seascape)
	{
		Seascape->SetRealRock(Cruise->IsRealRockShown());      // the landing at the Far Light: the real rock for its copy
		Seascape->Place(View, Stage, DeltaTime);
	}
	// the water streams aft at her speed: the waves' pattern (the sea), the foam (the wake)
	LyudSeascape::FShip Ship;
	Ship.Chart = { View.ChartPosM.X, View.ChartPosM.Y };
	Ship.Heading = View.HeadingRad;
	Ship.StageYaw = FMath::DegreesToRadians(Stage.GetRotation().Rotator().Yaw);
	// a clock jump (sleep) steps the model's speed: the water's follows over a few seconds
	SmoothSpeed = FMath::FInterpTo(SmoothSpeed, View.SpeedMS, DeltaTime, 0.5f);
	const LyudSeascape::FVec2 Water = LyudSeascape::WaterVelocity(Ship, SmoothSpeed);
	if (Sea.IsValid())
	{
		Sea->SetSurfaceDrift(SmoothSpeed > 0.02f ? FVector2D(Water.X, Water.Y) : FVector2D::ZeroVector);
	}
	if (Wake)
	{
		Wake->Feed(Stage, FVector::ZeroVector, FVector(Water.X * 100.0, Water.Y * 100.0, 0.0), DeltaTime);
	}
	// late comers to the world are hidden too: the typed actors every 5 s, the static mesh actors every 15 s
	HideClock -= DeltaTime;
	if (HideClock <= 0.f)
	{
		HideClock = 5.f;
		HideWorld(HideRound++ % 3 == 0);
	}
	// are the birds' assets in memory: asked twice a second, not every frame (ULyudPrefetch::FolderReady builds strings)
	BirdClock -= DeltaTime;
	bAskBirds = BirdClock <= 0.f;
	if (bAskBirds)
	{
		BirdClock = 0.5f;
	}
	UpdateSounds(View, Stage, DeltaTime);
	UpdateGulls(View, Stage, DeltaTime);
	UpdateColony(DeltaTime);
	// the anchor: the chain runs out as she anchors for the night, and comes home when she weighs
	const uint8 Phase = uint8(View.Phase);
	if (Phase != LastPhase)
	{
		const bool bNow = View.Phase == ELyudCruisePhase::Anchored;
		const bool bWas = LastPhase == uint8(ELyudCruisePhase::Anchored);
		if (bNow != bWas)
		{
			PlayAtShip(TEXT("ship_chain"), TEXT("windlass"), FVector(5600.0, 0.0, 400.0), DeckAtten);
		}
		LastPhase = Phase;
	}
}

// --------------------------------------------------------------------------------------------------- sounds

void ULyudCruiseScenery::MakeSounds()
{
	auto Atten = [this](float Full, float Falloff)
	{
		USoundAttenuation* At = NewObject<USoundAttenuation>(this);
		FSoundAttenuationSettings& A = At->Attenuation;
		A.bAttenuate = true;
		A.bSpatialize = true;
		A.AttenuationShape = EAttenuationShape::Sphere;
		A.AttenuationShapeExtents = FVector(Full, 0.f, 0.f);
		A.FalloffDistance = Falloff;
		A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		A.bAttenuateWithLPF = true;
		A.LPFRadiusMin = Full * 2.f;
		A.LPFRadiusMax = Falloff;
		return At;
	};
	// the engine is heard through the whole ship, strongest low and aft; the deck's sounds within a deck's length
	USoundAttenuation* HumAtten = Atten(700.f, 6500.f);
	DeckAtten = Atten(900.f, 9000.f);
	HornAtten = Atten(4000.f, 300000.f);
	BirdAtten = Atten(800.f, 30000.f);
	auto Loop = [this](const TCHAR* Id, const TCHAR* Name, USoundAttenuation* At) -> UAudioComponent*
	{
		USoundBase* S = ShipSound(Id);
		if (!S || !Seascape)
		{
			return nullptr;
		}
		UAudioComponent* A = NewObject<UAudioComponent>(Seascape, Name);
		A->SetSound(S);
		A->bOverrideAttenuation = false;
		A->AttenuationSettings = At;
		A->bAutoActivate = false;
		A->SetupAttachment(Seascape->GetRootComponent());
		A->SetUsingAbsoluteLocation(true);
		A->RegisterComponent();
		Seascape->AddInstanceComponent(A);
		return A;
	};
	Hum = Loop(TEXT("ship_hum"), TEXT("ShipHum"), HumAtten);
	Wash = Loop(TEXT("ship_wash"), TEXT("ShipWash"), DeckAtten);
	Rigging = Loop(TEXT("ship_rigging"), TEXT("ShipRigging"), DeckAtten);
	Creak = Loop(TEXT("ship_creak"), TEXT("ShipCreak"), HumAtten);
	for (const TCHAR* N : { TEXT("ship_horn"), TEXT("ship_horn3"), TEXT("ship_bell"), TEXT("ship_chain"), TEXT("gulls_feed") })
	{
		Sounds.Add(N, ShipSound(N));
	}
}

void ULyudCruiseScenery::PlayAtShip(FName Sound, FName Point, const FVector& FallbackShipCm, USoundAttenuation* Atten, float Volume)
{
	const ULyudCruise* Cruise = ULyudCruise::Get(this);
	USoundBase* S = Sounds.FindRef(Sound);
	if (!Cruise || !S)
	{
		return;
	}
	FVector At = Cruise->StageTransform().TransformPosition(FallbackShipCm);
	Cruise->GetShipPoint(Point, At);
	float Level = Volume;
	if (const ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this))
	{
		Level *= Scape->MasterVolume * Scape->EffectsVolume;
	}
	UGameplayStatics::PlaySoundAtLocation(this, S, At, Level, 1.f, 0.f, Atten);
}

void ULyudCruiseScenery::UpdateSounds(const FLyudCruiseView& View, const FTransform& Stage, float DeltaTime)
{
	float Level = 1.f, Outdoors = 1.f;
	if (const ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this))
	{
		Level = Scape->MasterVolume * Scape->EffectsVolume;
		Outdoors = Scape->GetOutdoors();
	}
	const float K = FMath::Clamp(SmoothSpeed / LyudSeascape::WakeFullSpeed, 0.f, 1.f);
	const bool bAnchored = View.Phase == ELyudCruisePhase::Anchored;
	auto Drive = [](UAudioComponent* A, const FVector& At, float Volume, float Pitch)
	{
		if (!A)
		{
			return;
		}
		if (Volume > 0.01f)
		{
			A->SetWorldLocation(At);
			A->SetVolumeMultiplier(Volume);
			A->SetPitchMultiplier(Pitch);
			if (!A->IsPlaying())
			{
				A->Play();
			}
		}
		else if (A->IsPlaying())
		{
			A->Stop();
		}
	};
	// the diesel: in the engine room, low and aft (the attenuation makes it felt more on the lower deck and aft); the
	// generator alone at anchor in the night, quieter still
	Drive(Hum, Stage.TransformPosition(FVector(1500.0, 0.0, 120.0)), Level * (bAnchored ? 0.12f : 0.3f + 0.7f * K), 0.82f + 0.24f * K);
	// the wash at the stem: outside only, by her speed
	const float Out = FMath::Lerp(0.2f, 1.f, Outdoors);
	Drive(Wash, Stage.TransformPosition(FVector(5900.0, 0.0, 60.0)), Level * Out * K * 0.9f, 0.9f + 0.2f * K);
	// the wind in the rigging: the wind she feels (the true wind less her own way)
	float Apparent = 0.f;
	if (const ULyudWindSubsystem* Wind = ULyudWindSubsystem::Get(this))
	{
		const FVector W = Wind->GetWindAt(Stage.GetLocation(), 12.f, true).Velocity;
		const FVector Way = Stage.GetUnitAxis(EAxis::X) * SmoothSpeed;
		Apparent = float((W - Way).Size2D());
	}
	Drive(Rigging, Stage.TransformPosition(FVector(4300.0, 0.0, 1500.0)), Level * Out * FMath::Clamp((Apparent - 4.f) / 12.f, 0.f, 1.f), 0.85f + 0.3f * FMath::Clamp(Apparent / 20.f, 0.f, 1.f));
	// the joinery works as she rolls: between decks amidships, more in a sea
	const float Hs = Sea.IsValid() ? Sea->WaveHeight() * 0.01f : 0.3f;
	Drive(Creak, Stage.TransformPosition(FVector(3300.0, 0.0, 150.0)), Level * (1.f - 0.6f * Outdoors) * FMath::Clamp(0.15f + 0.5f * Hs, 0.f, 1.f) * (bAnchored ? 0.5f : 1.f), 1.f);

	// the bell: every half hour of the watch by day (07:00-23:00), struck in pairs
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	const int32 Second = Now.GetHour() * 3600 + Now.GetMinute() * 60 + Now.GetSecond();
	const int64 HalfHour = int64(Now.GetTicks() / (ETimespan::TicksPerSecond * 1800));
	if (HalfHour != LastHalfHour)
	{
		const int32 N = LyudSeascape::BellsAt(Second, 20);
		// only a half hour that has just begun (not the first look, not a clock jumped into the middle of one)
		if (LastHalfHour >= 0 && N > 0 && Now.GetHour() >= 7 && Now.GetHour() < 23)
		{
			BellsLeft = N;
			BellsStruck = 0;
			BellClock = 0.f;
		}
		LastHalfHour = HalfHour;
	}
	if (BellsLeft > 0)
	{
		BellClock -= DeltaTime;
		if (BellClock <= 0.f)
		{
			PlayAtShip(TEXT("ship_bell"), TEXT("ship_bell"), FVector(5300.0, 0.0, 520.0), DeckAtten);
			BellsLeft--;
			BellsStruck++;
			// ding-ding, a pause, ding-ding...
			BellClock = BellsStruck % 2 == 0 ? 1.5f : 0.55f;
		}
	}
}

void ULyudCruiseScenery::StopSounds()
{
	for (UAudioComponent* A : { Hum.Get(), Wash.Get(), Rigging.Get(), Creak.Get() })
	{
		if (A && A->IsPlaying())
		{
			A->Stop();
		}
	}
	BellsLeft = 0;
}

void ULyudCruiseScenery::OnCruiseEvent(FName EventId)
{
	const FString Id = EventId.ToString();
	const bool bDeparture = Id == TEXT("departure");
	const bool bArrival = Id == TEXT("arrival");
	if (!bAboard)
	{
		// from the town: three long blasts as she sails, one as she comes to anchor
		if (MotorShip && (bDeparture || bArrival))
		{
			MotorShip->Horn(bDeparture ? 3 : 1);
		}
		return;
	}
	if (bDeparture)
	{
		PlayAtShip(TEXT("ship_horn3"), TEXT("funnel_top"), FVector(2350.0, 0.0, 1340.0), HornAtten);
	}
	else if (bArrival || Id.StartsWith(TEXT("isl_")))
	{
		PlayAtShip(TEXT("ship_horn"), TEXT("funnel_top"), FVector(2350.0, 0.0, 1340.0), HornAtten);
	}
}

// --------------------------------------------------------------------------------------------------- birds

bool ULyudCruiseScenery::OnCityAct(const FCityActInfo& Info, FText& Line)
{
	if (bAboard && Info.Act.StartsWith(TEXT("ship_gull_feed")))
	{
		FeedGulls(Info.At);
	}
	(void)Line;
	return false;      // never taken: the act runs as its data says
}

void ULyudCruiseScenery::FeedGulls(const FVector& From)
{
	FeedLeft = FeedSeconds;
	FeedFrom = From;
	if (Gulls.Num() > 0)
	{
		PlayAtShip(TEXT("gulls_feed"), TEXT("stern_rail"), FVector(0.0, 0.0, 600.0), BirdAtten);
		for (FGull& G : Gulls)
		{
			G.NextCall = FMath::Min(G.NextCall, FMath::FRandRange(0.3f, 2.5f));
		}
	}
}

bool ULyudCruiseScenery::BirdsLoaded(bool bWithCormorant) const
{
	bool bReady = true;
	for (const TCHAR* Folder : GullFolders)
	{
		bReady &= ULyudPrefetch::FolderReady(this, Folder, TEXT("_Hero"));
	}
	if (bWithCormorant)
	{
		bReady &= ULyudPrefetch::FolderReady(this, TEXT("/Game/Birds/Cormorant"), TEXT("_Hero"));
	}
	return bReady;
}

void ULyudCruiseScenery::UpdateGulls(const FLyudCruiseView& View, const FTransform& Stage, float DeltaTime)
{
	// ALyudTrain::UpdateGulls for a ship that stands still: the gulls hang over the stern and the wash by day while
	// she is under way (or while somebody feeds them), peel off and go when she stops or the light fails
	const ULyudCruise* Cruise = ULyudCruise::Get(this);
	FVector Rear = Stage.TransformPosition(FVector(0.0, 0.0, 600.0));
	if (Cruise)
	{
		static const FName SternRail(TEXT("stern_rail"));
		Cruise->GetShipPoint(SternRail, Rear);
	}
	const bool bDay = !Sky.IsValid() || Sky->SunAltitude() > -2.f;
	FeedLeft = FMath::Max(0.f, FeedLeft - DeltaTime);
	const bool bFeeding = FeedLeft > 0.f;
	const bool bWant = bDay && (SmoothSpeed > 0.5f || bFeeding);
	GullAway = bWant ? 0.f : FMath::Min(GullAway + DeltaTime, 30.f);
	if (bWant && Gulls.Num() == 0 && bAskBirds && BirdsLoaded(false))
	{
		static const FName Species[] = { TEXT("HerringGull"), TEXT("BlackHeadedGull"), TEXT("HerringGull"), TEXT("CommonGull"), TEXT("BlackHeadedGull"), TEXT("HerringGull") };
		for (int32 I = 0; I < NumSternGulls; I++)
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			P.bDeferConstruction = true;
			// they come in from astern
			const FVector At = Rear - Stage.GetUnitAxis(EAxis::X) * FMath::FRandRange(9000.f, 16000.f)
				+ FVector(FMath::FRandRange(-4000.f, 4000.f), FMath::FRandRange(-4000.f, 4000.f), FMath::FRandRange(1500.f, 3500.f));
			ABirdActor* B = GetWorld()->SpawnActor<ABirdActor>(ABirdActor::StaticClass(), FTransform(At), P);
			if (B)
			{
				B->Species = Species[I];
				B->InitialState = EBirdState::Glide;
				B->Seed = 7100 + I;
				B->FinishSpawning(FTransform(At));
				FGull G;
				G.Bird = B;
				G.Phase = I * 1.3f;
				G.NextCall = FMath::FRandRange(2.f, 9.f);
				// 2..30 m aft of the rail, 14 m to either side, 5..19 m over it
				G.Offset = FVector(FMath::FRandRange(-2800.f, 0.f), FMath::FRandRange(-1400.f, 1400.f), FMath::FRandRange(500.f, 1900.f));
				Gulls.Add(G);
			}
		}
	}
	if (Gulls.Num() == 0)
	{
		return;
	}
	const FVector Fwd = Stage.GetUnitAxis(EAxis::X).GetSafeNormal2D();
	const FVector Side(-Fwd.Y, Fwd.X, 0.0);
	const float T = GetWorld()->GetTimeSeconds();
	(void)View;
	bool bAllGone = true;
	for (int32 I = 0; I < Gulls.Num(); I++)
	{
		FGull& G = Gulls[I];
		ABirdActor* B = G.Bird.Get();
		if (!B)
		{
			continue;
		}
		const FVector P = B->GetActorLocation();
		FVector Target;
		float Gain = 0.6f, MaxSpeed = 2200.f;
		if (GullAway > 0.f)
		{
			// they peel off astern and climb away, then go
			Target = P - Fwd * 5000.0 + Side * (I % 2 ? 4000.0 : -4000.0) + FVector(0, 0, 2500.0);
		}
		else if (bFeeding)
		{
			// food in the air off the rail: quick passes close under the stern, one after another
			const FVector Crumb = FeedFrom - Fwd * (450.0 + 300.0 * FMath::Sin(T * 1.7f + G.Phase))
				+ Side * 260.0 * FMath::Sin(T * 2.3f + G.Phase * 2.f) + FVector(0, 0, -120.0 + 160.0 * FMath::Sin(T * 2.9f + G.Phase));
			Target = Crumb;
			Gain = 1.5f;
			MaxSpeed = 1500.f;
		}
		else
		{
			// hanging over the stern and the wash: slow figure-eights round a spot behind and above the rail
			Target = Rear - Fwd * (600.0 - G.Offset.X) + Side * G.Offset.Y + FVector(0, 0, G.Offset.Z)
				+ Side * 600.0 * FMath::Sin(T * 0.35f + G.Phase) + Fwd * 400.0 * FMath::Sin(T * 0.7f + G.Phase * 2.f)
				+ FVector(0, 0, 200.0 * FMath::Sin(T * 0.5f + G.Phase));
		}
		const FVector Want = (Target - P) * Gain;
		G.Vel = FMath::VInterpTo(G.Vel, Want.GetClampedToMaxSize(MaxSpeed), DeltaTime, 1.5f);
		const FVector NewP = P + G.Vel * DeltaTime;
		const float Bank = FMath::Clamp(float(FVector::DotProduct((Want - G.Vel), FVector(-G.Vel.Y, G.Vel.X, 0.0).GetSafeNormal())) * 0.02f, -35.f, 35.f);
		// a bird that hangs on the wind faces the way the ship goes, not the way it drifts
		const float Yaw = G.Vel.Size2D() > 250.f ? float(G.Vel.Rotation().Yaw) : float(Fwd.Rotation().Yaw);
		B->SetActorLocationAndRotation(NewP, FRotator(FMath::Clamp(float(-G.Vel.Z * 0.02f), -20.f, 20.f), Yaw, Bank));
		// what the wings feel: the bird's own way plus the ship's through the air
		B->SetVelocity(G.Vel + Fwd * SmoothSpeed * 100.f);
		const bool bFlap = bFeeding || FMath::Frac(T * 0.08f + G.Phase * 0.2f) < 0.22f || G.Vel.Z > 120.f;
		if (bFlap != G.bFlap)
		{
			B->SetState(bFlap ? EBirdState::Fly : EBirdState::Glide, 0.4f);
			G.bFlap = bFlap;
		}
		G.NextCall -= DeltaTime;
		if (G.NextCall <= 0.f && GullAway <= 0.f)
		{
			G.NextCall = bFeeding ? FMath::FRandRange(1.f, 4.f) : FMath::FRandRange(4.f, 14.f);
			TObjectPtr<USoundBase>* Known = GullCalls.Find(B->Species);
			if (!Known)
			{
				const FString Sp = B->Species.ToString();
				const TCHAR* Call = Sp == TEXT("HerringGull") ? TEXT("kyow") : (Sp == TEXT("BlackHeadedGull") ? TEXT("kree") : TEXT("kee_ya"));
				const FString Path = FString::Printf(TEXT("/Game/Birds/%s/Sounds/S_%s_%s.S_%s_%s"), *Sp, *Sp, Call, *Sp, Call);
				Known = &GullCalls.Add(B->Species, LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
			}
			if (USoundBase* Snd = Known->Get())
			{
				UGameplayStatics::PlaySoundAtLocation(this, Snd, NewP, 1.f, FMath::FRandRange(0.92f, 1.08f), 0.f, BirdAtten);
			}
		}
		if (GullAway < 20.f)
		{
			bAllGone = false;
		}
	}
	if (bAllGone)
	{
		DropGulls();
	}
}

void ULyudCruiseScenery::DropGulls()
{
	for (FGull& G : Gulls)
	{
		if (ABirdActor* B = G.Bird.Get())
		{
			B->Destroy();
		}
	}
	Gulls.Reset();
	GullAway = 0.f;
}

void ULyudCruiseScenery::UpdateColony(float DeltaTime)
{
	// Bird Island: gulls and cormorants on their rounds in front of the cliff while the island is near, by day. Their
	// places are worked out from the island's place every frame (the island is what moves), nothing is integrated.
	FVector Origin = FVector::ZeroVector, Face = FVector::ZeroVector;
	float DistM = 0.f;
	const bool bDay = !Sky.IsValid() || Sky->SunAltitude() > -2.f;
	static const FName BirdIsland(TEXT("birds")), ColonySocket(TEXT("colony"));
	const bool bNear = Seascape && bDay && Seascape->GetIslandWorld(BirdIsland, Origin, &DistM) && DistM < ColonyWithinM
		&& Seascape->GetSocketWorld(BirdIsland, ColonySocket, Face);
	if (!bNear)
	{
		if (Colony.Num() > 0 && (!Seascape || !bDay || DistM > ColonyWithinM * 1.15f || DistM <= 0.f))
		{
			DropColony();
		}
		return;
	}
	if (Colony.Num() == 0)
	{
		if (!bAskBirds || !BirdsLoaded(true))
		{
			return;
		}
		static const FName Species[] = { TEXT("HerringGull"), TEXT("CommonGull"), TEXT("Cormorant"), TEXT("HerringGull"), TEXT("BlackHeadedGull"), TEXT("Cormorant"),
			TEXT("HerringGull"), TEXT("CommonGull"), TEXT("HerringGull"), TEXT("Cormorant"), TEXT("BlackHeadedGull"), TEXT("CommonGull"), TEXT("HerringGull"),
			TEXT("Cormorant"), TEXT("HerringGull"), TEXT("CommonGull") };
		FRandomStream R(4512);
		for (int32 I = 0; I < NumColony; I++)
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			P.bDeferConstruction = true;
			ABirdActor* B = GetWorld()->SpawnActor<ABirdActor>(ABirdActor::StaticClass(), FTransform(Face), P);
			if (!B)
			{
				continue;
			}
			const bool bCormorant = Species[I] == TEXT("Cormorant");
			B->Species = Species[I];
			B->InitialState = bCormorant ? EBirdState::Fly : EBirdState::Glide;
			B->Seed = 7200 + I;
			B->FinishSpawning(FTransform(Face));
			FColonyBird C;
			C.Bird = B;
			// gulls wheel high along the face; cormorants fly long low rounds out over the water
			C.Radius = bCormorant ? R.FRandRange(14000.f, 26000.f) : R.FRandRange(5000.f, 16000.f);
			C.Height = bCormorant ? R.FRandRange(300.f, 900.f) : R.FRandRange(1800.f, 7500.f);
			C.Along = R.FRandRange(-18000.f, 18000.f);
			C.Speed = bCormorant ? 1500.f : R.FRandRange(900.f, 1200.f);
			C.Phase = R.FRandRange(0.f, 2.f * PI);
			C.Squash = R.FRandRange(0.35f, 0.7f);
			Colony.Add(C);
		}
	}
	(void)DeltaTime;
	const FVector Out = (Face - Origin).GetSafeNormal2D();
	const FVector Along(-Out.Y, Out.X, 0.0);
	const float T = GetWorld()->GetTimeSeconds();
	for (int32 I = 0; I < Colony.Num(); I++)
	{
		const FColonyBird& C = Colony[I];
		ABirdActor* B = C.Bird.Get();
		if (!B)
		{
			continue;
		}
		const float Dir = I % 2 ? 1.f : -1.f;
		const float Rate = Dir * C.Speed / C.Radius;
		const float A = C.Phase + T * Rate;
		const float Sa = FMath::Sin(A), Ca = FMath::Cos(A);
		// an ellipse flattened against the cliff, touching the face at its inner end
		const FVector At = FVector(Face.X, Face.Y, Origin.Z) + Out * (600.0 + C.Radius * C.Squash * (1.f + Ca)) + Along * (C.Along + C.Radius * Sa)
			+ FVector(0.0, 0.0, C.Height + 250.0 * FMath::Sin(A * 2.f + C.Phase));
		const FVector Vel = (Out * (-C.Radius * C.Squash * Sa) + Along * (C.Radius * Ca)) * Rate + FVector(0.0, 0.0, 500.0 * FMath::Cos(A * 2.f + C.Phase) * Rate);
		B->SetActorLocationAndRotation(At, FRotator(FMath::Clamp(float(-Vel.Z * 0.02f), -15.f, 15.f), float(Vel.Rotation().Yaw), Dir * -18.f));
		B->SetVelocity(Vel);
	}
}

void ULyudCruiseScenery::DropColony()
{
	for (FColonyBird& C : Colony)
	{
		if (ABirdActor* B = C.Bird.Get())
		{
			B->Destroy();
		}
	}
	Colony.Reset();
}

FString ULyudCruiseScenery::Report() const
{
	FString Out = FString::Printf(TEXT("cruise scenery: %s, aboard %d, water speed %.2f m/s, hidden actors %d, stern gulls %d (feeding %.0f s), colony birds %d, wake foam %d"),
		bMade ? TEXT("made") : TEXT("not made"), bAboard ? 1 : 0, SmoothSpeed, Hidden.Num(), Gulls.Num(), FeedLeft, Colony.Num(), Wake ? Wake->NumFoam() : 0);
	if (Sea.IsValid())
	{
		const FVector2D D = Sea->GetSurfaceDrift();
		Out += FString::Printf(TEXT(", sea drift %.2f %.2f m/s"), D.X, D.Y);
	}
	if (MotorShip)
	{
		Out += TEXT("\n") + MotorShip->Report();
	}
	if (Seascape)
	{
		Out += TEXT("\n") + Seascape->Report();
	}
	return Out;
}
