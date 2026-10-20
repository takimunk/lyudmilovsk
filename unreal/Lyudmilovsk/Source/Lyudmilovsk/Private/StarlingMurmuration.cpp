#include "StarlingMurmuration.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Murmuration/StarlingFlock.h"
#include "Murmuration/StarlingMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float SimDt = 1.f / 60.f;
	constexpr float PoseAmp = 0.7f;

	/** Sim metres (three.js axes, Y up) -> actor space centimetres (Z up): (x, y, z) -> (x, z, y) * 100. */
	FORCEINLINE FVector ToLocal(float X, float Y, float Z) { return FVector(X, Z, Y) * 100.0; }
	FORCEINLINE FVector ToLocalDir(float X, float Y, float Z) { return FVector(X, Z, Y); }

	/**
	 * Bird transform in actor space: beak along the velocity, banked by Bank about it (the
	 * original's writeMatrix). The mesh has its beak on +Y, wings on X, up on Z.
	 */
	FTransform BirdTransform(const FVector& At, float VX, float VY, float VZ, float Bank, float Scale, float Thick)
	{
		const float S = FMath::Sqrt(VX * VX + VY * VY + VZ * VZ) + 1e-6f;
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
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetCastShadow(false);
		ISM->SetCanEverAffectNavigation(false);
		ISM->bDisableCollision = true;
		ISM->SetMobility(EComponentMobility::Movable);
		// Scene 01's outside sun lights channel 1 only
		ISM->LightingChannels.bChannel1 = true;
		PoseComponents.Add(ISM);
	}
	PeregrineComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Peregrine"));
	PeregrineComponent->SetupAttachment(RootComponent);
	PeregrineComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PeregrineComponent->SetCastShadow(false);
	PeregrineComponent->SetCanEverAffectNavigation(false);
	PeregrineComponent->SetMobility(EComponentMobility::Movable);
	PeregrineComponent->LightingChannels.bChannel1 = true;
	PeregrineComponent->SetVisibility(false);
}

AStarlingMurmuration::~AStarlingMurmuration() = default;

void AStarlingMurmuration::BeginPlay()
{
	Super::BeginPlay();
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
	if (PoseMeshes.Num() == NumPoses && StarlingMaterial)
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
	PoseTransforms.SetNum(NumPoses);
	Flock = MakePimpl<FStarlingFlock>(BirdCount, Params, FVector2f(AreaHalfSize), (uint32)FMath::Rand() | 1u);
	SampleGround();
	Accumulator = 0.f;
	PeregrineClock = 0.f;
	PeregrineUntil = -1.f;
	PeregrineNext = FMath::FRandRange(PeregrineInterval.X, PeregrineInterval.Y) * 0.5f;
}

/** Heights of the ground under the roaming area, from line traces (sea level = the actor's Z). */
void AStarlingMurmuration::SampleGround()
{
	UWorld* World = GetWorld();
	if (!World || !Flock)
	{
		return;
	}
	const float Cell = 5.f;
	const float X0 = -AreaHalfSize.X - 60.f, Z0 = -AreaHalfSize.Y - 60.f;
	const int32 NX = FMath::CeilToInt(2.f * (AreaHalfSize.X + 60.f) / Cell) + 1;
	const int32 NZ = FMath::CeilToInt(2.f * (AreaHalfSize.Y + 60.f) / Cell) + 1;
	const FTransform& T = GetActorTransform();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(StarlingGround), false, this);
	TArray<float> Heights;
	Heights.SetNumZeroed(NX * NZ);
	int32 Land = 0;
	for (int32 IZ = 0; IZ < NZ; IZ++)
	{
		for (int32 IX = 0; IX < NX; IX++)
		{
			const float X = X0 + IX * Cell, Z = Z0 + IZ * Cell;
			const FVector From = T.TransformPositionNoScale(ToLocal(X, 300.f, Z));
			const FVector To = T.TransformPositionNoScale(ToLocal(X, -20.f, Z));
			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Q))
			{
				const float H = (float)T.InverseTransformPositionNoScale(Hit.ImpactPoint).Z / 100.f;
				Heights[IZ * NX + IX] = H;
				Land += H > 0.5f;
			}
		}
	}
	Flock->SetGround(X0, Z0, Cell, NX, NZ, MoveTemp(Heights));
	UE_LOG(LogTemp, Log, TEXT("StarlingMurmuration %s: %d birds, ground %dx%d, %d land cells"), *GetName(), BirdCount, NX, NZ, Land);
}

void AStarlingMurmuration::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Flock || Flock->Count != BirdCount)
	{
		StartFlock();
	}
	Flock->P = Params;

	if (bPeregrine)
	{
		PeregrineClock += DeltaSeconds;
		if (!Flock->IsPredatorOn() && PeregrineClock >= PeregrineNext)
		{
			Flock->SetPredator(true);
			PeregrineUntil = PeregrineClock + PeregrineHunt;
		}
		else if (Flock->IsPredatorOn() && PeregrineClock >= PeregrineUntil)
		{
			Flock->SetPredator(false);
			PeregrineNext = PeregrineClock + FMath::FRandRange(PeregrineInterval.X, PeregrineInterval.Y);
		}
	}
	else if (Flock->IsPredatorOn())
	{
		Flock->SetPredator(false);
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
	for (int32 I = 0; I < F.Count; I++)
	{
		const FVector At = ToLocal(F.Px[I] + F.Vx[I] * Ahead, F.Py[I] + F.Vy[I] * Ahead, F.Pz[I] + F.Vz[I] * Ahead);
		float Grow = 1.f, Thick = 1.f;
		if (bHaveEye)
		{
			Grow = FMath::Max(1.f, (float)FVector::Dist(At, Eye) / 100.f * MinScale);
			Thick = FMath::Lerp(1.f, 2.5f, FMath::Clamp((Grow - 1.f) * 0.5f, 0.f, 1.f));
		}
		const float Beat = FMath::Sin(F.FlapPhase[I] + Ahead * 2.f * PI * 12.5f) * F.FlapAmp[I] / PoseAmp;
		PoseTransforms[PoseOf(Beat, NumPoses)].Add(BirdTransform(At, F.Vx[I], F.Vy[I], F.Vz[I], F.Bank[I], Grow, Thick));
	}

	const FTransform Hidden(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
	for (int32 K = 0; K < NumPoses; K++)
	{
		UInstancedStaticMeshComponent* ISM = PoseComponents[K];
		TArray<FTransform>& Arr = PoseTransforms[K];
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

void AStarlingMurmuration::DebugRun(float Seconds)
{
	if (!Flock || Flock->Count != BirdCount)
	{
		StartFlock();
	}
	Flock->P = Params;
	const int32 N = FMath::RoundToInt(Seconds / SimDt);
	for (int32 S = 1; S <= N; S++)
	{
		if (bPeregrine)
		{
			PeregrineClock += SimDt;
			if (!Flock->IsPredatorOn() && PeregrineClock >= PeregrineNext)
			{
				Flock->SetPredator(true);
				PeregrineUntil = PeregrineClock + PeregrineHunt;
			}
			else if (Flock->IsPredatorOn() && PeregrineClock >= PeregrineUntil)
			{
				Flock->SetPredator(false);
				PeregrineNext = PeregrineClock + FMath::FRandRange(PeregrineInterval.X, PeregrineInterval.Y);
			}
		}
		Flock->Step(SimDt);
		if (S % 300 == 0)
		{
			LogStats();
		}
	}
	UpdateInstances(0.f);
}

void AStarlingMurmuration::LogStats() const
{
	// extent of the middle 90% of the flock per axis (sim metres), and its centre in the world
	TArray<float> Ax[3] = { Flock->Px, Flock->Py, Flock->Pz };
	FVector Lo, Hi;
	for (int32 A = 0; A < 3; A++)
	{
		Ax[A].Sort();
		Lo[A] = Ax[A][Flock->Count / 20];
		Hi[A] = Ax[A][Flock->Count - 1 - Flock->Count / 20];
	}
	const FVector Mid = (Lo + Hi) * 0.5;
	// mean nearest-neighbour distance over a sample of birds
	double NN = 0.0;
	int32 NS = 0;
	for (int32 I = 0; I < Flock->Count; I += FMath::Max(1, Flock->Count / 200), NS++)
	{
		float Best = 1e9f;
		for (int32 J = 0; J < Flock->Count; J++)
		{
			if (J != I)
			{
				Best = FMath::Min(Best, FVector3f(Flock->Px[I] - Flock->Px[J], Flock->Py[I] - Flock->Py[J], Flock->Pz[I] - Flock->Pz[J]).SizeSquared());
			}
		}
		NN += FMath::Sqrt(Best);
	}
	UE_LOG(LogTemp, Display, TEXT("StarlingMurmuration %s t=%.0f centre %s extent 5-95%% x %.0f alt %.0f..%.0f z %.0f m, nn %.2f m, falcon %d, %s"), *GetName(),
		Flock->SimTime, *GetActorTransform().TransformPositionNoScale(ToLocal(Mid.X, Mid.Y, Mid.Z)).ToCompactString(),
		Hi.X - Lo.X, Lo.Y, Hi.Y, Hi.Z - Lo.Z, NS ? NN / NS : 0.0, Flock->IsPredatorOn(), *Flock->Describe());
	}
