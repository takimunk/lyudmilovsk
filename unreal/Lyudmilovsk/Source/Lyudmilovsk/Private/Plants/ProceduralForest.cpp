#include "Plants/ProceduralForest.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "CoreGlobals.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Plants/FallenFruit.h"
#include "Plants/GameCalendar.h"
#include "Plants/PlantMeshes.h"
#include "Plants/PlantModel.h"
#include "Plants/ProceduralGrass.h"
#include "Plants/ProceduralPlant.h"
#include "Plants/TreeBakeCache.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "UObject/ConstructorHelpers.h"

static TAutoConsoleVariable<float> CVarForestLiveMaxSpeed(TEXT("lyud.Forest.LiveMaxSpeed"), -2.f,
	TEXT("No live plant is swapped in while the camera is faster than this, cm/s (below -1: the actor's LiveMaxSpeed; -1 or 0 from the console: no limit)."), ECVF_Default);
static TAutoConsoleVariable<int32> CVarForestMaxLive(TEXT("lyud.Forest.MaxLive"), -1,
	TEXT("Live plants a forest swaps in near the camera at most (below 0: the actor's MaxLive). 0 draws baked trees only."), ECVF_Scalability);
// the graphics levels (Config/DefaultScalability.ini, FoliageQuality) set these; the benchmark's lyud.Bench.Forest* knobs win when set
static TAutoConsoleVariable<float> CVarForestDistanceScale(TEXT("lyud.Forest.DistanceScale"), 1.f,
	TEXT("Forest trees are drawn out to the forest's CullDistance x this (graphics levels: Foliage)."), ECVF_Scalability);
static TAutoConsoleVariable<int32> CVarForestCastShadows(TEXT("lyud.Forest.Shadows"), 1,
	TEXT("Forest trees cast shadows (1) or not (0) (graphics levels: Foliage)."), ECVF_Scalability);
static TAutoConsoleVariable<float> CVarForestWindDistance(TEXT("lyud.Forest.WindDistance"), 0.f,
	TEXT("Forest trees farther than this many metres stand still: no wind in their vertices, and the virtual shadow map keeps their shadows cached (0: wind everywhere; graphics levels: Foliage)."), ECVF_Scalability);

namespace
{
	using PlantModel::Hash01;

	/** A forest looks at the camera, its bakes and its graphics level this often, seconds. */
	constexpr float ForestTickSeconds = 0.25f;

	/** A benchmark knob (Bench/LyudBenchmark.cpp, registered with the module): -1 when unset or absent. */
	float ForestBenchKnob(IConsoleVariable* const& Var)
	{
		return Var ? Var->GetFloat() : -1.f;
	}
}

AProceduralForest::AProceduralForest()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = ForestTickSeconds;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Mix = { FForestMix() };
}

TArray<FName> AProceduralForest::GetSpeciesNames() const
{
	return PlantSpeciesLibrary::Names();
}

float AProceduralForest::CurrentDay() const
{
	return DevDay >= 0.f ? DevDay : UGameCalendar::DayOfYear(UGameCalendar::NowIn(GetWorld()));
}

uint32 AProceduralForest::BakeSeedFor(FName Species, int32 Variant) const
{
	// the same for every forest, so their kinds are shared in the cache
	return (uint32)Variant * 977u + (GetTypeHash(Species) & 0xFFFFu) + 1u;
}

int32 AProceduralForest::NumBaked() const
{
	int32 N = 0;
	for (const FGroup& G : Groups) N += G.Version >= 0 ? G.TreeIds.Num() : 0;
	return N;
}

FString AProceduralForest::DescribeState() const
{
	int32 Drawn = 0;
	for (const FGroup& G : Groups) Drawn += G.Comp.IsValid() ? G.Comp->GetInstanceCount() : 0;
	const UTreeBakeCache* Cache = GetWorld() ? GetWorld()->GetSubsystem<UTreeBakeCache>() : nullptr;
	return FString::Printf(TEXT("%d trees in %d kinds, %d drawn as instances, %d live; %s"), Trees.Num(), Groups.Num(), Drawn, LiveCount, Cache ? *Cache->DescribeStats() : TEXT("no bake cache"));
}

int32 AProceduralForest::GroupFor(FName Species, uint32 BakeSeed, float Age)
{
	for (int32 I = 0; I < Groups.Num(); I++)
	{
		if (Groups[I].Species == Species && Groups[I].BakeSeed == BakeSeed && FMath::IsNearlyEqual(Groups[I].Age, Age)) return I;
	}
	FGroup G;
	G.Species = Species;
	G.BakeSeed = BakeSeed;
	G.Age = Age;
	UInstancedStaticMeshComponent* NewComp = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
	G.Comp = NewComp;
	G.Comp->SetMobility(EComponentMobility::Movable);
	G.Comp->SetupAttachment(RootComponent);
	G.Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	G.Comp->bDisableCollision = true;
	G.Comp->SetCanEverAffectNavigation(false);
	G.Comp->bAffectDistanceFieldLighting = false;
	// trees take Lumen's light but do not bounce it: a forest in the Lumen scene costs more than it adds
	G.Comp->bAffectDynamicIndirectLighting = false;
	G.Comp->bReceivesDecals = false;
	G.Comp->bUseAsOccluder = false;
	// the outdoor sun lights channel 1 only; channel 0 is the club's lights, whose shadow work the forests' huge bounds would join
	G.Comp->LightingChannels.bChannel0 = false;
	G.Comp->LightingChannels.bChannel1 = true;
	// the graphics level's draw distance, shadows and wind (ApplyQuality keeps them up to date)
	const float Draw = GetDrawDistance();
	const int32 Shadows = QualityShadows() ? 1 : 0;
	const int32 Wind = QualityWindDistance();
	G.Comp->SetCullDistances(0, (int32)Draw);
	G.Comp->CastShadow = Shadows != 0;
	G.Comp->WorldPositionOffsetDisableDistance = Wind;
	G.Comp->RegisterComponent();
	if (Groups.IsEmpty())
	{
		// the first kind is built with the level as it is: nothing for ApplyQuality to wait its turn for at the start
		AppliedDraw = Draw;
		AppliedShadows = Shadows;
		AppliedWind = Wind;
	}
	return Groups.Add(MoveTemp(G));
}

float AProceduralForest::GetDrawDistance() const
{
	// looked up once, at the first call (FindConsoleVariable builds a string: never every tick)
	static IConsoleVariable* const Knob = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Bench.ForestCullM"));
	const float BenchM = ForestBenchKnob(Knob);
	return BenchM >= 0.f ? BenchM * 100.f : CullDistance * FMath::Max(0.05f, CVarForestDistanceScale.GetValueOnGameThread());
}

bool AProceduralForest::QualityShadows() const
{
	static IConsoleVariable* const Knob = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Bench.ForestShadows"));
	const float Bench = ForestBenchKnob(Knob);
	return Bench >= 0.f ? Bench != 0.f : CVarForestCastShadows.GetValueOnGameThread() != 0;
}

int32 AProceduralForest::QualityWindDistance() const
{
	static IConsoleVariable* const Knob = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Bench.ForestWPODisableM"));
	const float BenchM = ForestBenchKnob(Knob);
	return FMath::Max(0, FMath::RoundToInt((BenchM >= 0.f ? BenchM : CVarForestWindDistance.GetValueOnGameThread()) * 100.f));
}

bool AProceduralForest::ApplyQuality(bool bForce)
{
	const float Draw = GetDrawDistance();
	const int32 Shadows = QualityShadows() ? 1 : 0;
	const int32 Wind = QualityWindDistance();
	const bool bChanged = Draw != AppliedDraw || Shadows != AppliedShadows || Wind != AppliedWind;
	// a change of level: one forest a frame across the world (each resends its kinds' components, all of them in one frame
	// would stall it); a forest waiting for its frame ticks every frame until then, and does nothing else in those ticks.
	// Forced (the benchmark's knobs), or nothing built yet: now.
	static uint64 ForestQualityFrame = 0;
	const bool bNow = bForce || Groups.IsEmpty() || (bChanged && ForestQualityFrame != GFrameCounter);
	const bool bWaiting = bChanged && !bNow;
	const float Interval = bWaiting ? 0.f : ForestTickSeconds;
	if (GetActorTickInterval() != Interval)
	{
		SetActorTickInterval(Interval);
	}
	if (!bNow || (!bChanged && !bForce))
	{
		return bWaiting;
	}
	if (!bForce && !Groups.IsEmpty())
	{
		ForestQualityFrame = GFrameCounter;
	}
	AppliedDraw = Draw;
	AppliedShadows = Shadows;
	AppliedWind = Wind;
	// each setter marks the render state dirty only when its value changes: once per change of level, never per frame
	for (FGroup& G : Groups)
	{
		if (!G.Comp.IsValid())
		{
			continue;
		}
		G.Comp->SetCullDistances(0, (int32)Draw);
		G.Comp->SetCastShadow(Shadows != 0);
		G.Comp->SetWorldPositionOffsetDisableDistance(Wind);
	}
	return false;
}

void AProceduralForest::AddTree(FName Species, FVector Location, float Age, int32 Variant, float Yaw, float Scale)
{
	FTree T;
	T.Species = Species;
	T.BakeSeed = BakeSeedFor(Species, Variant);
	T.Age = Age;
	T.Xf = FTransform(FRotator(0.f, Yaw, 0.f), Location, FVector(Scale));
	T.Group = GroupFor(Species, T.BakeSeed, Age);
	Groups[T.Group].TreeIds.Add(Trees.Num());
	Groups[T.Group].bInstancesAdded = false;
	Trees.Add(MoveTemp(T));
}

void AProceduralForest::Clear()
{
	for (FTree& T : Trees)
	{
		if (AProceduralPlant* P = T.Live.Get()) P->Destroy();
	}
	LiveCount = 0;
	for (FGroup& G : Groups)
	{
		if (UInstancedStaticMeshComponent* C = G.Comp.Get()) C->DestroyComponent();
	}
	Groups.Reset();
	Trees.Reset();
}

void AProceduralForest::Scatter()
{
	UWorld* World = GetWorld();
	if (!World || Mix.IsEmpty()) return;
	const FTransform Xf = GetActorTransform();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ForestGround), false, this);
	if (bSnapToGround)
	{
		for (TActorIterator<AProceduralPlant> It(World); It; ++It) Q.AddIgnoredActor(*It);
		for (TActorIterator<AFallenFruit> It(World); It; ++It) Q.AddIgnoredActor(*It);
		for (TActorIterator<AProceduralGrass> It(World); It; ++It) Q.AddIgnoredActor(*It);
		for (TActorIterator<AProceduralForest> It(World); It; ++It) Q.AddIgnoredActor(*It);
	}
	const int32 Nx = FMath::Max(1, FMath::CeilToInt(2.f * HalfExtent.X / Spacing)), Ny = FMath::Max(1, FMath::CeilToInt(2.f * HalfExtent.Y / Spacing));
	const float KeepShare = FMath::Clamp((float)Count / (Nx * Ny), 0.f, 1.f);
	TArray<const FPlantSpecies*> Species;
	for (const FForestMix& M : Mix) Species.Add(PlantSpeciesLibrary::Find(M.Species));
	for (int32 Cy = 0; Cy < Ny; Cy++)
	{
		for (int32 Cx = 0; Cx < Nx; Cx++)
		{
			const uint32 U = (uint32)Seed;
			if (Hash01(U, Cx, Cy, 11) > KeepShare) continue;
			const FVector Local(-HalfExtent.X + (Cx + 0.5f + 0.7f * (Hash01(U, Cx, Cy, 12) - 0.5f)) * Spacing, -HalfExtent.Y + (Cy + 0.5f + 0.7f * (Hash01(U, Cx, Cy, 13) - 0.5f)) * Spacing, 0.0);
			FVector P = Xf.TransformPosition(Local);
			if (bSnapToGround)
			{
				FHitResult Hit;
				if (!World->LineTraceSingleByChannel(Hit, P + FVector(0, 0, TraceUp), P - FVector(0, 0, TraceDown), ECC_Visibility, Q)) continue;
				if (Hit.ImpactNormal.Z < MinSlopeNormalZ) continue;
				if (Hit.GetActor() && !BareGroundTag.IsNone() && Hit.GetActor()->ActorHasTag(BareGroundTag)) continue;
				P = Hit.ImpactPoint;
			}
			if (P.Z < MinWorldZ || P.Z > MaxWorldZ) continue;
			// stands: each kind is favoured by its own slow wave over the forest, so species come in patches
			float Total = 0.f;
			TArray<float> Score;
			for (int32 I = 0; I < Mix.Num(); I++)
			{
				const float Wave = 0.5f + 0.5f * FMath::Sin(P.X * (0.0007f + 0.0003f * I) + P.Y * (0.0005f + 0.0002f * I) * (I % 2 ? -1.f : 1.f) + I * 2.1f + Seed);
				Score.Add(Species[I] ? Mix[I].Weight * (0.15f + 0.85f * Wave) : 0.f);
				Total += Score.Last();
			}
			if (Total <= 0.f) continue;
			float Pick = Hash01(U, Cx, Cy, 14) * Total;
			int32 Kind = 0;
			for (; Kind < Mix.Num() - 1 && Pick > Score[Kind]; Kind++) Pick -= Score[Kind];
			const FForestMix& M = Mix[Kind];
			const float Age = Hash01(U, Cx, Cy, 15) < M.YoungShare ? M.YoungAge : M.MatureAge;
			AddTree(M.Species, P, Age, FMath::FloorToInt(Hash01(U, Cx, Cy, 16) * Variants), Hash01(U, Cx, Cy, 17) * 360.f, 0.92f + 0.16f * Hash01(U, Cx, Cy, 18));
		}
	}
}

void AProceduralForest::Rebuild()
{
	const FVector Home = GetActorLocation();
	Clear();
	Scatter();
	(void)Home;
}

void AProceduralForest::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const UWorld* World = GetWorld();
	if (bPreviewInEditor && World && World->WorldType == EWorldType::Editor && Trees.IsEmpty())
	{
		Rebuild();
		BakeAll();
	}
}

void AProceduralForest::BeginPlay()
{
	Super::BeginPlay();
	if (Trees.IsEmpty()) Rebuild();
}

void AProceduralForest::EndPlay(const EEndPlayReason::Type Reason)
{
	Clear();
	Super::EndPlay(Reason);
}

void AProceduralForest::SyncBakes(float Day)
{
	UTreeBakeCache* Cache = GetWorld() ? GetWorld()->GetSubsystem<UTreeBakeCache>() : nullptr;
	if (!Cache) return;
	for (FGroup& G : Groups)
	{
		const FTreeBaked* B = Cache->Request(G.Species, G.BakeSeed, G.Age, Day, RebakeDays, LodDistances * 0.01);
		if (!B || B->Version == G.Version || !B->Mesh || !G.Comp.IsValid()) continue;
		G.Comp->SetStaticMesh(B->Mesh);
		G.Comp->SetMaterial(0, B->Wood);
		G.Comp->SetMaterial(1, B->Leaf);
		G.Comp->SetMaterial(2, B->Card);
		G.Comp->SetMaterial(3, B->Bloom);
		G.Comp->SetCullDistances(0, (int32)GetDrawDistance());
		if (!G.bInstancesAdded)
		{
			TArray<FTransform> Xfs;
			for (const int32 Id : G.TreeIds) Xfs.Add(Trees[Id].Xf);
			G.Comp->ClearInstances();
			const TArray<int32> Indices = G.Comp->AddInstances(Xfs, true, true);
			for (int32 K = 0; K < G.TreeIds.Num(); K++) Trees[G.TreeIds[K]].Instance = Indices.IsValidIndex(K) ? Indices[K] : INDEX_NONE;
			G.bInstancesAdded = true;
		}
		G.Version = B->Version;
		Applied++;
	}
}

void AProceduralForest::BakeAll()
{
	UTreeBakeCache* Cache = GetWorld() ? GetWorld()->GetSubsystem<UTreeBakeCache>() : nullptr;
	if (!Cache) return;
	SyncBakes(CurrentDay()); // asks for every kind
	Cache->BakeAllNow();
	SyncBakes(CurrentDay());
}

void AProceduralForest::SetHidden(FTree& T, bool bHidden)
{
	if (T.Group == INDEX_NONE || T.Instance == INDEX_NONE || !Groups[T.Group].Comp.IsValid()) return;
	FTransform Xf = T.Xf;
	if (bHidden) Xf.SetScale3D(FVector(0.0001));
	Groups[T.Group].Comp->UpdateInstanceTransform(T.Instance, Xf, true, false, true);
}

bool AProceduralForest::Interactive(FName Species) const
{
	const FPlantSpecies* S = PlantSpeciesLibrary::Find(Species);
	return S && !S->FruitItem.IsNone();
}

void AProceduralForest::UpdateLive(const FVector& Camera)
{
	const double Near2 = FMath::Square((double)LiveDistance), Far2 = FMath::Square((double)LiveDistance * 1.3);
	TArray<TPair<double, int32>> Wanted;
	for (int32 I = 0; I < Trees.Num(); I++)
	{
		FTree& T = Trees[I];
		const double D2 = FVector::DistSquared(Camera, T.Xf.GetLocation());
		const double Above = Camera.Z - T.Xf.GetLocation().Z;
		if (AProceduralPlant* P = T.Live.Get())
		{
			if (D2 > Far2 || Above > LiveMaxHeight * 1.5)
			{
				P->Destroy();
				T.Live.Reset();
				LiveCount--;
				SetHidden(T, false);
			}
		}
		else if (D2 < Near2 && Above < LiveMaxHeight && (!bLiveOnlyInteractive || Interactive(T.Species)) && Groups[T.Group].Version >= 0 && T.Instance != INDEX_NONE)
		{
			Wanted.Emplace(D2, I);
		}
	}
	Wanted.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key < B.Key; });
	const int32 MaxNow = CVarForestMaxLive.GetValueOnGameThread() >= 0 ? CVarForestMaxLive.GetValueOnGameThread() : MaxLive;
	if (LiveCount > MaxNow)
	{
		// a lighter graphics level allows fewer live plants: the farthest go back to their bakes now (once per change)
		TArray<TPair<double, int32>> LiveTrees;
		for (int32 I = 0; I < Trees.Num(); I++)
		{
			if (Trees[I].Live.IsValid())
			{
				LiveTrees.Emplace(FVector::DistSquared(Camera, Trees[I].Xf.GetLocation()), I);
			}
		}
		LiveTrees.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key > B.Key; });
		for (int32 K = 0; K < LiveTrees.Num() && LiveCount > MaxNow; K++)
		{
			FTree& T = Trees[LiveTrees[K].Value];
			T.Live->Destroy();
			T.Live.Reset();
			LiveCount--;
			SetHidden(T, false);
		}
	}
	// a camera that is flying or running past does not need a plant built under it
	const double Now = GetWorld()->GetTimeSeconds();
	double Speed = 0.0;
	if (LastCameraTime >= 0.0 && Now > LastCameraTime)
	{
		Speed = FVector::Dist(Camera, LastCamera) / (Now - LastCameraTime);
	}
	LastCamera = Camera;
	LastCameraTime = Now;
	const float Limit = CVarForestLiveMaxSpeed.GetValueOnGameThread() < -1.f ? LiveMaxSpeed : CVarForestLiveMaxSpeed.GetValueOnGameThread();
	if (Limit > 0.f && Speed > Limit)
	{
		return;
	}
	int32 Built = 0;
	UTreeBakeCache* Cache = GetWorld()->GetSubsystem<UTreeBakeCache>();
	// every forest shares the frame: where two or three meet, each built its own plant in the same frame (tens of ms
	// each); the others now wait a frame
	static uint64 LiveFrame = 0;
	static int32 LiveBuiltInFrame = 0;
	if (LiveFrame != GFrameCounter)
	{
		LiveFrame = GFrameCounter;
		LiveBuiltInFrame = 0;
	}
	for (const TPair<double, int32>& W : Wanted)
	{
		// a live plant builds its skeleton, wood and leaves in this tick: a few at most per tick, the nearest first
		if (Built >= LivePerTick || LiveBuiltInFrame >= LivePerTick) break;
		if (LiveCount >= MaxNow) break;
		FTree& T = Trees[W.Value];
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AProceduralPlant* P = GetWorld()->SpawnActorDeferred<AProceduralPlant>(AProceduralPlant::StaticClass(), T.Xf, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!P) continue;
		P->Species = T.Species;
		P->Seed = (int32)T.BakeSeed;
		P->AgeAtStart = T.Age;
		P->bDemoGrowth = false;
		P->bSnapToGround = false;
		UTexture2D *Albedo = nullptr, *Normal = nullptr, *Detail = nullptr;
		if (Cache && Cache->GetSpeciesTextures(T.Species, Albedo, Normal, Detail))
		{
			P->SharedBarkAlbedo = Albedo;
			P->SharedBarkNormal = Normal;
			P->SharedLeafDetail = Detail;
		}
		P->FinishSpawning(T.Xf);
		Built++;
		LiveBuiltInFrame++;
		T.Live = P;
		LiveCount++;
		SetHidden(T, true);
	}
}

void AProceduralForest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld()) return;
	// waiting for its frame to take a new graphics level: only that, every frame, until its turn
	if (ApplyQuality(false)) return;
	SyncBakes(CurrentDay());
	const APlayerController* PC = World->GetFirstPlayerController();
	if (PC && PC->PlayerCameraManager && !Trees.IsEmpty())
	{
		UpdateLive(PC->PlayerCameraManager->GetCameraLocation());
	}
}
