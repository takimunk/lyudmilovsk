#include "StarlingMurmuration.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Murmuration/StarlingFlock.h"
#include "Murmuration/StarlingMesh.h"
#include "Plants/GameCalendar.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float SimDt = 1.f / 60.f;
	constexpr float PoseAmp = 0.7f;

	/** Sim metres (three.js axes, Y up) -> actor space centimetres (Z up): (x, y, z) -> (x, z, y) * 100. */
	FORCEINLINE FVector ToLocal(float X, float Y, float Z) { return FVector(X, Z, Y) * 100.0; }
	FORCEINLINE FVector ToLocalDir(float X, float Y, float Z) { return FVector(X, Z, Y); }
	FORCEINLINE FVector3f FromLocal(const FVector& L) { return FVector3f(L.X, L.Z, L.Y) / 100.f; }

	/**
	 * Bird transform in actor space: beak along the velocity, banked by Bank about it (the
	 * original's writeMatrix). The mesh has its beak on +Y, wings on X, up on Z.
	 */
	FTransform BirdTransform(const FVector& At, float VX, float VY, float VZ, float Bank, float Scale, float Thick)
	{
		const float S = FMath::Sqrt(VX * VX + VY * VY + VZ * VZ) + 1e-9f;
		const float FX = VX / S, FY = VY / S, FZ = VZ / S;
		// side axis = up x forward, up0 = forward x side (three.js axes, as in the sim)
		float RX = FZ, RZ = -FX;
		float RL = FMath::Sqrt(RX * RX + RZ * RZ);
		if (RL < 1e-4f) { RX = 1.f; RZ = 0.f; RL = 1.f; }
		RX /= RL; RZ /= RL;
		const float UX = FY * RZ, UY = FZ * RX - FX * RZ, UZ = -FY * RX;
		const float C = FMath::Cos(Bank), Sn = FMath::Sin(Bank);
		const FVector Side = ToLocalDir(RX * C + UX * Sn, UY * Sn, RZ * C + UZ * Sn);
		const FVector Up = ToLocalDir(-RX * Sn + UX * C, UY * C, -RZ * Sn + UZ * C);
		const FVector Fwd = ToLocalDir(FX, FY, FZ);
		const FMatrix M(Side, Fwd, Up, FVector::ZeroVector);
		return FTransform(M.ToQuat(), At, FVector(Scale, Scale, Scale * Thick));
	}

	int32 PoseOf(float Beat, int32 NumPoses)
	{
		return FMath::Clamp(FMath::RoundToInt((Beat * 0.5f + 0.5f) * (NumPoses - 1)), 0, NumPoses - 1);
	}

	/** Hour angle (hours from solar noon) at which the sun stands at Altitude degrees. */
	float HourAngle(float AltitudeDeg, float LatDeg, float DeclRad)
	{
		const float Lat = FMath::DegreesToRadians(LatDeg);
		const float CosH = (FMath::Sin(FMath::DegreesToRadians(AltitudeDeg)) - FMath::Sin(Lat) * FMath::Sin(DeclRad)) / (FMath::Cos(Lat) * FMath::Cos(DeclRad));
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosH, -1.f, 1.f))) / 15.f;
	}

	void ConfigureBirdComponent(UPrimitiveComponent* C)
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetCanEverAffectNavigation(false);
		C->SetMobility(EComponentMobility::Movable);
		// Scene 01's outside sun lights channel 1 only
		C->LightingChannels.bChannel1 = true;
	}
}

AStarlingMurmuration::AStarlingMurmuration()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Basic(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = Basic.Object;

	for (int32 K = 0; K < NumPoses; K++)
	{
		UInstancedStaticMeshComponent* ISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("Pose%d"), K));
		ISM->SetupAttachment(RootComponent);
		ConfigureBirdComponent(ISM);
		ISM->bDisableCollision = true;
		PoseComponents.Add(ISM);
	}
	PerchedComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Perched"));
	PerchedComponent->SetupAttachment(RootComponent);
	ConfigureBirdComponent(PerchedComponent);
	PerchedComponent->bDisableCollision = true;
	PeregrineComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Peregrine"));
	PeregrineComponent->SetupAttachment(RootComponent);
	ConfigureBirdComponent(PeregrineComponent);
	PeregrineComponent->SetVisibility(false);
}

AStarlingMurmuration::~AStarlingMurmuration() = default;

static TAutoConsoleVariable<int32> CVarStarlingsLegacy(TEXT("lyud.Starlings.Legacy"), 0,
	TEXT("1: keep an AStarlingMurmuration placed in Lvl_Scene01. 0 (default): it stands down there, the starlings are the bird "
		 "life's (BirdMurmur.cpp); the actor stays a dev/test flock in other levels."));

void AStarlingMurmuration::BeginPlay()
{
	Super::BeginPlay();
	if (GetWorld() && GetWorld()->GetMapName().Contains(TEXT("Lvl_Scene01")) && CVarStarlingsLegacy.GetValueOnGameThread() == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("StarlingMurmuration %s: stands down in the outdoor map (the bird life's starlings murmurate there)"), *GetName());
		SetActorHiddenInGame(true);
		SetActorTickEnabled(false);
		return;
	}
	StartFlock();
}

void AStarlingMurmuration::EndPlay(const EEndPlayReason::Type Reason)
{
	Flock.Reset();
	Super::EndPlay(Reason);
}

void AStarlingMurmuration::Restart()
{
	StartFlock();
}

void AStarlingMurmuration::EnsureMeshes()
{
	if (PoseMeshes.Num() == NumPoses + 1 && StarlingMaterial)
	{
		return;
	}
	StarlingMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	StarlingMaterial->SetVectorParameterValue(TEXT("Color"), StarlingColor);
	StarlingMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.9f);
	PeregrineMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	PeregrineMaterial->SetVectorParameterValue(TEXT("Color"), PeregrineColor);
	PeregrineMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.9f);
	PoseMeshes.Reset();
	for (int32 K = 0; K < NumPoses; K++)
	{
		// wing beat from full upstroke to full downstroke
		const float Beat = (float)K / (NumPoses - 1) * 2.f - 1.f;
		UStaticMesh* Mesh = StarlingMesh::Build(this, *FString::Printf(TEXT("SM_Starling_Pose%d"), K), Beat * PoseAmp, PoseAmp, StarlingMaterial);
		PoseMeshes.Add(Mesh);
		PoseComponents[K]->SetStaticMesh(Mesh);
		PoseComponents[K]->SetMaterial(0, StarlingMaterial);
	}
	UStaticMesh* Perched = StarlingMesh::BuildPerched(this, TEXT("SM_Starling_Perched"), StarlingMaterial);
	PoseMeshes.Add(Perched);
	PerchedComponent->SetStaticMesh(Perched);
	PerchedComponent->SetMaterial(0, StarlingMaterial);
	PeregrineComponent->SetStaticMesh(PoseMeshes[NumPoses / 2]);
	PeregrineComponent->SetMaterial(0, PeregrineMaterial);
}

void AStarlingMurmuration::StartFlock()
{
	EnsureMeshes();
	for (UInstancedStaticMeshComponent* ISM : PoseComponents)
	{
		ISM->ClearInstances();
	}
	PerchedComponent->ClearInstances();
	PoseTransforms.SetNum(NumPoses);
	Flock = MakePimpl<FStarlingFlock>(BirdCount, Params, FVector2f(AreaHalfSize), (uint32)FMath::Rand() | 1u);
	Flock->GroundProbe = [this](float X, float Z) { return ProbeGround(X, Z); };
	SampleGround();
	Accumulator = 0.f;
	PeregrineClock = 0.f;
	PeregrineUntil = -1.f;
	PeregrineNext = FMath::FRandRange(PeregrineInterval.X, PeregrineInterval.Y) * 0.5f;
	bFeeding = false;
	LureFeet = FVector(1e30);
	UpdateClock(0.f, true);
}

/** Heights of the ground under and around the roaming area, from line traces (sea level = the actor's Z). */
void AStarlingMurmuration::SampleGround()
{
	UWorld* World = GetWorld();
	if (!World || !Flock)
	{
		return;
	}
	// a wide margin, so the roost can be found on land beyond the area
	const float Cell = 6.f, Margin = 150.f;
	const float X0 = -AreaHalfSize.X - Margin, Z0 = -AreaHalfSize.Y - Margin;
	const int32 NX = FMath::CeilToInt(2.f * (AreaHalfSize.X + Margin) / Cell) + 1;
	const int32 NZ = FMath::CeilToInt(2.f * (AreaHalfSize.Y + Margin) / Cell) + 1;
	TArray<float> Heights;
	Heights.SetNumZeroed(NX * NZ);
	int32 Land = 0;
	for (int32 IZ = 0; IZ < NZ; IZ++)
	{
		for (int32 IX = 0; IX < NX; IX++)
		{
			const float H = ProbeGround(X0 + IX * Cell, Z0 + IZ * Cell);
			Heights[IZ * NX + IX] = H;
			Land += H > 0.5f;
		}
	}
	Flock->SetGround(X0, Z0, Cell, NX, NZ, MoveTemp(Heights));
	UE_LOG(LogTemp, Display, TEXT("StarlingMurmuration %s: %d birds, ground %dx%d, %d land cells"), *GetName(), BirdCount, NX, NZ, Land);
}

/** The ground (or sea, 0) under sim (x, z), sim metres; the player is not ground. */
float AStarlingMurmuration::ProbeGround(float X, float Z) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}
	const FTransform& T = GetActorTransform();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(StarlingGround), false, this);
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Q.AddIgnoredActor(Pawn);
	}
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, T.TransformPositionNoScale(ToLocal(X, 300.f, Z)), T.TransformPositionNoScale(ToLocal(X, -20.f, Z)), ECC_Visibility, Q))
	{
		return FMath::Max(0.f, (float)T.InverseTransformPositionNoScale(Hit.ImpactPoint).Z / 100.f);
	}
	return 0.f;
}

/** The time of day for the birds: the game calendar (or ClockOverride, or a debug clock) and the sun's times. */
void AStarlingMurmuration::UpdateClock(float DeltaSeconds, bool bForce)
{
	ClockTimer -= DeltaSeconds;
	if (!Flock || (!bForce && ClockTimer > 0.f))
	{
		return;
	}
	ClockTimer = 0.25f;
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	float Hours = (float)Now.GetTimeOfDay().GetTotalHours();
	if (DebugHours >= 0.f) Hours = DebugHours;
	else if (ClockOverride >= 0.f) Hours = ClockOverride;
	// the sun's declination for the date, and the times it crosses the horizon and -6 degrees
	const float Decl = FMath::DegreesToRadians(23.44f) * FMath::Sin(2.f * PI * (UGameCalendar::DayOfYear(Now) - 81.f) / 365.f);
	const float Day = HourAngle(-0.833f, Latitude, Decl);
	const float Civil = HourAngle(-6.f, Latitude, Decl);
	Flock->SetClock(Hours, SolarNoon - Day, SolarNoon + Day, SolarNoon + Civil);
}

void AStarlingMurmuration::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Flock || Flock->Count != FMath::Clamp(BirdCount, 0, 20000))
	{
		StartFlock();
	}
	Flock->P = Params;

	if (bPeregrine)
	{
		PeregrineClock += DeltaSeconds;
		// the falcon hunts by day only
		const bool bDay = Flock->GetActivity() != FStarlingFlock::EActivity::Roosting;
		if (!Flock->IsPredatorOn() && PeregrineClock >= PeregrineNext && bDay)
		{
			Flock->SetPredator(true);
			PeregrineUntil = PeregrineClock + PeregrineHunt;
		}
		else if (Flock->IsPredatorOn() && (PeregrineClock >= PeregrineUntil || !bDay))
		{
			Flock->SetPredator(false);
			PeregrineNext = PeregrineClock + FMath::FRandRange(PeregrineInterval.X, PeregrineInterval.Y);
		}
	}
	else if (Flock->IsPredatorOn())
	{
		Flock->SetPredator(false);
	}
	UpdateClock(DeltaSeconds, false);
	ViewerTimer -= DeltaSeconds;
	if (ViewerTimer <= 0.f)
	{
		ViewerTimer = 0.1f;
		UpdateViewer(false);
	}

	Accumulator += FMath::Min(DeltaSeconds, 0.1f);
	int32 Steps = 0;
	while (Accumulator >= SimDt && Steps < 4)
	{
		Flock->Step(SimDt);
		Accumulator -= SimDt;
		Steps++;
	}
	Accumulator = FMath::Min(Accumulator, SimDt);
	UpdateInstances(Accumulator);

	if (UE_LOG_ACTIVE(LogTemp, Verbose) && FMath::FloorToInt(Flock->SimTime / 5.f) != FMath::FloorToInt((Flock->SimTime - Steps * SimDt) / 5.f))
	{
		LogStats();
	}
}

// --- the player and hand-feeding ------------------------------------------------------------------

void AStarlingMurmuration::SetFeeding(bool bOn)
{
	bFeeding = bOn;
	LureFeet = FVector(1e30);
	UpdateViewer(true);
}

/** The player's feet, walk and hand, ~10 times a second. */
void AStarlingMurmuration::UpdateViewer(bool bForce)
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!Pawn || !Flock)
	{
		return;
	}
	const FVector Feet = Pawn->GetActorLocation() - FVector(0.f, 0.f, Pawn->GetSimpleCollisionHalfHeight());
	FVector Hand = Feet + FVector(0.f, 0.f, 120.f);
	if (bFeedHand)
	{
		Hand = FeedHand;
	}
	else if (PC && PC->PlayerCameraManager)
	{
		const FRotator View = PC->PlayerCameraManager->GetCameraRotation();
		// the palm held out in front, a little below the eyes
		Hand = PC->PlayerCameraManager->GetCameraLocation() + FRotator(0.f, View.Yaw, 0.f).Vector() * 45.f - FVector(0.f, 0.f, 30.f);
	}
	FeedViewer(Feet, Pawn->GetVelocity(), Hand, bForce);
}

void AStarlingMurmuration::FeedViewer(const FVector& Feet, const FVector& Velocity, const FVector& Hand, bool bForce)
{
	const FTransform& T = GetActorTransform();
	Flock->SetViewer(FromLocal(T.InverseTransformPositionNoScale(Feet)), FromLocal(T.InverseTransformVectorNoScale(Velocity)));
	const FVector3f SimHand = FromLocal(T.InverseTransformPositionNoScale(Hand));
	if (bFeeding)
	{
		// landing spots on the ground round the feet, new ones once the player has moved on
		TArray<FVector3f> Spots;
		if (FVector::Dist2D(Feet, LureFeet) > 300.f)
		{
			LureFeet = Feet;
			Spots = MakeLureSpots(Feet);
		}
		if (bForce || Spots.Num() || Flock->IsLureOn())
		{
			Flock->SetLure(true, SimHand, MoveTemp(Spots));
		}
	}
	else if (Flock->IsLureOn())
	{
		Flock->SetLure(false, SimHand, {});
	}
}

/** Up to 100 spots on open ground 1-7 m round the feet, from line traces (sim frame). */
TArray<FVector3f> AStarlingMurmuration::MakeLureSpots(const FVector& Feet) const
{
	TArray<FVector3f> Spots;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return Spots;
	}
	FCollisionQueryParams Q(SCENE_QUERY_STAT(StarlingSpots), false, this);
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Q.AddIgnoredActor(Pawn);
	}
	const FTransform& T = GetActorTransform();
	for (int32 K = 0; K < 160 && Spots.Num() < 100; K++)
	{
		// a golden-angle spiral, a little jittered, from 1 m out
		const float R = 100.f + 600.f * FMath::Sqrt((K + 0.5f) / 160.f) + FMath::FRandRange(-20.f, 20.f);
		const float A = K * 2.39996f + FMath::FRandRange(-0.2f, 0.2f);
		const FVector At = Feet + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f);
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 150.f), At - FVector(0.f, 0.f, 300.f), ECC_Visibility, Q))
		{
			continue;
		}
		// open, walkable ground near the player's level (not a roof, a wall top, the sea)
		if (Hit.ImpactNormal.Z < 0.75f || FMath::Abs(Hit.ImpactPoint.Z - Feet.Z) > 150.f || Hit.ImpactPoint.Z < T.GetLocation().Z + 50.f)
		{
			continue;
		}
		Spots.Add(FromLocal(T.InverseTransformPositionNoScale(Hit.ImpactPoint)));
	}
	return Spots;
}

// --- drawing ----------------------------------------------------------------------------------

void AStarlingMurmuration::SyncInstances(UInstancedStaticMeshComponent* ISM, TArray<FTransform>& Arr)
{
	static const FTransform Hidden(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
	const int32 Want = Arr.Num();
	const int32 Have = ISM->GetInstanceCount();
	if (Want > Have)
	{
		// grow with some slack; spare instances are parked at zero scale
		TArray<FTransform> Extra;
		Extra.Init(Hidden, Want - Have + FMath::Max(16, Want / 5));
		ISM->AddInstances(Extra, false);
	}
	Arr.SetNum(ISM->GetInstanceCount(), EAllowShrinking::No);
	for (int32 J = Want; J < Arr.Num(); J++)
	{
		Arr[J] = Hidden;
	}
	if (Arr.Num() > 0)
	{
		ISM->BatchUpdateInstancesTransforms(0, Arr, false, true, true);
	}
}

void AStarlingMurmuration::UpdateInstances(float Ahead)
{
	const FStarlingFlock& F = *Flock;
	const FTransform& ActorT = GetActorTransform();

	// birds always cover MinPixels however far they are (90 degree view, 1080 px high), and
	// far ones get a little thicker, so one seen edge-on doesn't vanish
	const UWorld* World = GetWorld();
	FVector Eye = ActorT.GetLocation();
	bool bHaveEye = false;
	if (World && World->ViewLocationsRenderedLastFrame.Num() > 0)
	{
		Eye = ActorT.InverseTransformPositionNoScale(World->ViewLocationsRenderedLastFrame[0]);
		bHaveEye = true;
	}
	const float MinScale = MinPixels * (2.f * FMath::Tan(FMath::DegreesToRadians(45.f)) / 1080.f) / 0.38f; // per metre of distance

	for (TArray<FTransform>& Arr : PoseTransforms)
	{
		Arr.Reset();
	}
	PerchedTransforms.Reset();
	for (int32 I = 0; I < F.Count; I++)
	{
		const uint8 St = F.State[I];
		if (St == FStarlingFlock::Away)
		{
			continue;
		}
		const bool bStill = St == FStarlingFlock::Perched || St == FStarlingFlock::Grounded;
		const float A = bStill ? 0.f : Ahead;
		const FVector At = ToLocal(F.Px[I] + F.Vx[I] * A, F.Py[I] + F.Vy[I] * A, F.Pz[I] + F.Vz[I] * A);
		float Grow = 1.f, Thick = 1.f;
		if (bHaveEye)
		{
			Grow = FMath::Max(1.f, (float)FVector::Dist(At, Eye) / 100.f * MinScale);
			Thick = FMath::Lerp(1.f, 2.5f, FMath::Clamp((Grow - 1.f) * 0.5f, 0.f, 1.f));
		}
		if (bStill || (St == FStarlingFlock::Visiting && F.Fold[I] > 0.9f))
		{
			PerchedTransforms.Add(BirdTransform(At, F.Vx[I], F.Vy[I], F.Vz[I], 0.f, Grow, 1.f));
			continue;
		}
		const float Beat = FMath::Sin(F.FlapPhase[I] + Ahead * 2.f * PI * 12.5f) * F.FlapAmp[I] / PoseAmp;
		PoseTransforms[PoseOf(Beat, NumPoses)].Add(BirdTransform(At, F.Vx[I], F.Vy[I], F.Vz[I], F.Bank[I], Grow, Thick));
	}
	for (int32 K = 0; K < NumPoses; K++)
	{
		SyncInstances(PoseComponents[K], PoseTransforms[K]);
	}
	SyncInstances(PerchedComponent, PerchedTransforms);

	const bool bPred = F.IsPredatorOn();
	PeregrineComponent->SetVisibility(bPred);
	if (bPred)
	{
		const FVector3f& PP = F.PredPos;
		const FVector3f& PV = F.PredVel;
		const float Sp = PV.Length();
		// a stoop folds the wings; otherwise slow, deep beats
		const float Beat = Sp > 22.f ? 0.f : FMath::Sin(F.SimTime * 2.f * PI * 4.5f);
		PeregrineComponent->SetStaticMesh(PoseMeshes[PoseOf(Beat, NumPoses)]);
		const FVector At = ToLocal(PP.X + PV.X * Ahead, PP.Y + PV.Y * Ahead, PP.Z + PV.Z * Ahead);
		PeregrineComponent->SetRelativeTransform(BirdTransform(At, PV.X, PV.Y, PV.Z, 0.f, 2.7f, 1.f));
	}
}

// --- headless runs (pythonscript commandlet) -----------------------------------------------------

void AStarlingMurmuration::DebugRun(float Seconds, float StartHour, float HoursPerSecond, float LogEvery)
{
	if (!Flock || Flock->Count != FMath::Clamp(BirdCount, 0, 20000))
	{
		StartFlock();
	}
	Flock->P = Params;
	DebugHours = StartHour;
	DebugRate = HoursPerSecond;
	UpdateClock(0.f, true);
	const int32 N = FMath::RoundToInt(Seconds / SimDt);
	const int32 Every = FMath::Max(1, FMath::RoundToInt(LogEvery / SimDt));
	for (int32 S = 1; S <= N; S++)
	{
		if (DebugHours >= 0.f) DebugHours = FMath::Fmod(DebugHours + DebugRate * SimDt, 24.f);
		UpdateClock(SimDt, false);
		if (bPeregrine)
		{
			PeregrineClock += SimDt;
			const bool bDay = Flock->GetActivity() != FStarlingFlock::EActivity::Roosting;
			if (!Flock->IsPredatorOn() && PeregrineClock >= PeregrineNext && bDay)
			{
				Flock->SetPredator(true);
				PeregrineUntil = PeregrineClock + PeregrineHunt;
			}
			else if (Flock->IsPredatorOn() && (PeregrineClock >= PeregrineUntil || !bDay))
			{
				Flock->SetPredator(false);
				PeregrineNext = PeregrineClock + FMath::FRandRange(PeregrineInterval.X, PeregrineInterval.Y);
			}
		}
		Flock->Step(SimDt);
		if (S % Every == 0)
		{
			LogStats();
		}
	}
	DebugHours = -1.f;
	UpdateInstances(0.f);
}

void AStarlingMurmuration::DebugFeed(float FeetX, float FeetY, float FeetZ, float Seconds, float WalkAtBirdsAfter, float NoticeRange)
{
	if (!Flock)
	{
		StartFlock();
	}
	const float SavedRange = Params.NoticeRange;
	if (NoticeRange >= 0.f)
	{
		Params.NoticeRange = NoticeRange;
	}
	Flock->P = Params;
	bFeeding = true;
	LureFeet = FVector(1e30);
	FVector At(FeetX, FeetY, FeetZ);
	const FTransform& T = GetActorTransform();
	const int32 N = FMath::RoundToInt((Seconds + 40.f) / SimDt);
	for (int32 S = 1; S <= N; S++)
	{
		const float Time = S * SimDt;
		if (Time > Seconds && bFeeding)
		{
			bFeeding = false;
			UE_LOG(LogTemp, Display, TEXT("StarlingMurmuration feeding: food put away"));
		}
		FVector Vel = FVector::ZeroVector;
		if (WalkAtBirdsAfter >= 0.f && Time > WalkAtBirdsAfter && Time < WalkAtBirdsAfter + 4.f)
		{
			// walk at the birds on the ground at 1.4 m/s
			FVector Sum = FVector::ZeroVector;
			int32 Num = 0;
			for (int32 I = 0; I < Flock->Count; I++)
			{
				if (Flock->State[I] == FStarlingFlock::Grounded) { Sum += T.TransformPositionNoScale(ToLocal(Flock->Px[I], Flock->Py[I], Flock->Pz[I])); Num++; }
			}
			if (Num)
			{
				Vel = ((Sum / Num - At) * FVector(1, 1, 0)).GetSafeNormal() * 140.f;
				At += Vel * SimDt;
			}
		}
		if (S % 6 == 0)
		{
			FeedViewer(At, Vel, At + FVector(0.f, 0.f, 120.f), false);
		}
		UpdateClock(SimDt, false);
		Flock->Step(SimDt);
		if (S % 60 == 0)
		{
			int32 Visiting = 0, Grounded = 0, Near = 0;
			for (int32 I = 0; I < Flock->Count; I++)
			{
				Visiting += Flock->State[I] == FStarlingFlock::Visiting;
				Grounded += Flock->State[I] == FStarlingFlock::Grounded;
				Near += FVector::Dist(T.TransformPositionNoScale(ToLocal(Flock->Px[I], Flock->Py[I], Flock->Pz[I])), At) < Params.NoticeRange * 100.f;
			}
			UE_LOG(LogTemp, Display, TEXT("StarlingMurmuration feeding t=%.0f: %d within notice range, %d flying in, %d on the ground%s"),
				Time, Near, Visiting, Grounded, Vel.IsZero() ? TEXT("") : TEXT(", walking at them"));
		}
	}
	Params.NoticeRange = SavedRange;
	Flock->P = Params;
	UpdateInstances(0.f);
}

void AStarlingMurmuration::LogStats() const
{
	// extent of the middle 90% of the flying birds per axis (sim metres), and their centre in the world
	TArray<float> Ax[3];
	for (int32 I = 0; I < Flock->Count; I++)
	{
		const uint8 St = Flock->State[I];
		if (St == FStarlingFlock::Flying || St == FStarlingFlock::Escaping)
		{
			Ax[0].Add(Flock->Px[I]); Ax[1].Add(Flock->Py[I]); Ax[2].Add(Flock->Pz[I]);
		}
	}
	FString Extent = TEXT("none flying");
	if (Ax[0].Num() > 20)
	{
		FVector Lo, Hi;
		const int32 N = Ax[0].Num();
		for (int32 A = 0; A < 3; A++)
		{
			Ax[A].Sort();
			Lo[A] = Ax[A][N / 20];
			Hi[A] = Ax[A][N - 1 - N / 20];
		}
		const FVector Mid = (Lo + Hi) * 0.5;
		Extent = FString::Printf(TEXT("centre %s extent 5-95%% x %.0f alt %.0f..%.0f z %.0f m"),
			*GetActorTransform().TransformPositionNoScale(ToLocal(Mid.X, Mid.Y, Mid.Z)).ToCompactString(), Hi.X - Lo.X, Lo.Y, Hi.Y, Hi.Z - Lo.Z);
	}
	UE_LOG(LogTemp, Display, TEXT("StarlingMurmuration %s t=%.0f %s, falcon %d, %s"), *GetName(), Flock->SimTime, *Extent, Flock->IsPredatorOn(), *Flock->Describe());
}
