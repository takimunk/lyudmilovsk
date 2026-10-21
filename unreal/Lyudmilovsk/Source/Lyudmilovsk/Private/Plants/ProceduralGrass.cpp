#include "Plants/ProceduralGrass.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Plants/FallenFruit.h"
#include "Plants/GameCalendar.h"
#include "Plants/PlantMeshes.h"
#include "Plants/ProceduralPlant.h"
#include "StaticMeshResources.h"
#include "UObject/ConstructorHelpers.h"

static TAutoConsoleVariable<float> CVarGrassDensity(TEXT("lyud.GrassDensity"), 1.f,
	TEXT("Multiplies every grass patch's tuft density (applied when a patch is built). 0.5 for a slow machine."), ECVF_Default);

namespace
{
	using PlantModel::Hash01;

	/** The ground under a world point: its height, and whether grass may grow on it. */
	struct FGround
	{
		bool bHit = false;
		bool bBare = false;
		float Z = 0.f;
		float NormalZ = 1.f;
	};

	constexpr float GroundCell = 200.f;

	UHierarchicalInstancedStaticMeshComponent* MakeTufts(AActor* Owner, FName Name, USceneComponent* Parent)
	{
		// transient: the tufts are laid out again at BeginPlay (and in the editor on load), never saved in the level
		UHierarchicalInstancedStaticMeshComponent* C = Owner->CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(Name, true);
		C->SetupAttachment(Parent);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->bDisableCollision = true;
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(false);
		C->bAffectDynamicIndirectLighting = false;
		C->bAffectDistanceFieldLighting = false;
		C->bReceivesDecals = false;
		C->bUseAsOccluder = false;
		C->NumCustomDataFloats = 1;
		// Scene 01's outside sun lights channel 1 only; channel 0 keeps the default lights too
		C->LightingChannels.bChannel0 = true;
		C->LightingChannels.bChannel1 = true;
		return C;
	}
}

AProceduralGrass::AProceduralGrass()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 2.f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	NearTufts = MakeTufts(this, TEXT("NearTufts"), RootComponent);
	FarTufts = MakeTufts(this, TEXT("FarTufts"), RootComponent);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Grass(TEXT("/Game/Plants/M_PlantGrass.M_PlantGrass"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Basic(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	MaterialAsset = Grass.Object ? Grass.Object : Basic.Object;
}

TArray<FName> AProceduralGrass::GetSpeciesNames() const
{
	return GrassSpeciesLibrary::Names();
}

const FGrassSpecies& AProceduralGrass::GetSpecies() const
{
	const FGrassSpecies* S = GrassSpeciesLibrary::Find(Species);
	return S ? *S : GrassSpeciesLibrary::All()[0];
}

float AProceduralGrass::CurrentDay() const
{
	if (DevDay >= 0.f)
	{
		return DevDay;
	}
	const UWorld* World = GetWorld();
	if ((!World || !World->IsGameWorld()) && EditorPreviewDay >= 0.f)
	{
		return EditorPreviewDay;
	}
	return UGameCalendar::DayOfYear(UGameCalendar::NowIn(World));
}

FString AProceduralGrass::DescribeState() const
{
	return FString::Printf(TEXT("%s: %d tufts, %d triangles each, %d drawn near / %d far"), *Species.ToString(), Tufts, TuftTriangles,
		NearTufts ? NearTufts->GetInstanceCount() : 0, FarTufts ? FarTufts->GetInstanceCount() : 0);
}

void AProceduralGrass::EnsureAssets()
{
	if (BuiltSpecies == Species && TuftMesh && Material)
	{
		return;
	}
	const FGrassSpecies& G = GetSpecies();
	// never saved and never copied (see PlantMeshes::BuildMesh); the PIE copy builds its own at BeginPlay
	TuftMesh = PlantMeshes::BuildGrassTuft(this, G);
	const FStaticMeshRenderData* RD = TuftMesh->GetRenderData();
	TuftTriangles = RD && RD->LODResources.Num() ? RD->LODResources[0].GetNumTriangles() : 0;
	Material = UMaterialInstanceDynamic::Create(MaterialAsset, this);
	Material->SetVectorParameterValue(TEXT("GreenBase"), G.GreenBase);
	Material->SetVectorParameterValue(TEXT("GreenTip"), G.GreenTip);
	Material->SetVectorParameterValue(TEXT("DryBase"), G.DryBase);
	Material->SetVectorParameterValue(TEXT("DryTip"), G.DryTip);
	Material->SetVectorParameterValue(TEXT("Color"), G.GreenTip); // when the grass material is missing
	Material->SetScalarParameterValue(TEXT("SwayAmp"), G.Sway);
	for (UHierarchicalInstancedStaticMeshComponent* C : { NearTufts.Get(), FarTufts.Get() })
	{
		C->SetStaticMesh(TuftMesh);
		C->SetMaterial(0, Material);
	}
	BuiltSpecies = Species;
	ShownDay = -1000.f;
}

void AProceduralGrass::ApplySeason(float Day)
{
	if (!Material)
	{
		return;
	}
	const FGrassSpecies& G = GetSpecies();
	using PlantModel::SmoothStep;
	// green from spring to the autumn turn, then straw standing through the winter
	const float Green = SmoothStep(G.GreenUpStart, G.GreenUpFull, Day) * (1.f - SmoothStep(G.YellowStart, G.YellowFull, Day));
	const float Snow = PlantModel::SnowCover(Day);
	// new growth in spring, then the snow flattens it
	const float Grow = Day < 200.f ? SmoothStep(G.GreenUpStart, G.GreenUpFull + 25.f, Day) : 1.f - SmoothStep(310.f, 345.f, Day);
	Material->SetScalarParameterValue(TEXT("Dryness"), 1.f - Green);
	Material->SetScalarParameterValue(TEXT("Snow"), Snow);
	Material->SetScalarParameterValue(TEXT("HeightScale"), FMath::Lerp(G.WinterHeight, 1.f, Grow));
	ShownDay = Day;
}

void AProceduralGrass::Scatter()
{
	NearTufts->ClearInstances();
	FarTufts->ClearInstances();
	Tufts = 0;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FGrassSpecies& G = GetSpecies();
	const float Density = FMath::Max(G.Density * DensityScale * CVarGrassDensity.GetValueOnGameThread(), 0.01f);
	const float Cell = 100.f / FMath::Sqrt(Density); // cm between tufts, on a grid fixed to the world so neighbouring patches match
	const FTransform Xf = GetActorTransform();
	const FTransform Inv = Xf.Inverse();

	// the patch's reach in the world
	FBox2D Reach(ForceInit);
	for (int32 I = 0; I < 4; I++)
	{
		const FVector Corner = Xf.TransformPosition(FVector((I & 1) ? HalfExtent.X : -HalfExtent.X, (I & 2) ? HalfExtent.Y : -HalfExtent.Y, 0.0));
		Reach += FVector2D(Corner.X, Corner.Y);
	}

	// the ground, sampled every GroundCell on a world grid and interpolated between
	TMap<FIntPoint, FGround> Samples;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(GrassGround), false, this);
	if (bSnapToGround)
	{
		for (TActorIterator<AProceduralPlant> It(World); It; ++It) Q.AddIgnoredActor(*It);
		for (TActorIterator<AFallenFruit> It(World); It; ++It) Q.AddIgnoredActor(*It);
		for (TActorIterator<AProceduralGrass> It(World); It; ++It) Q.AddIgnoredActor(*It);
	}
	auto Sample = [&](int32 Ix, int32 Iy) -> const FGround&
	{
		if (const FGround* Have = Samples.Find(FIntPoint(Ix, Iy)))
		{
			return *Have;
		}
		FGround Gd;
		const float X = Ix * GroundCell, Y = Iy * GroundCell;
		if (!bSnapToGround)
		{
			Gd.bHit = true;
			Gd.Z = (float)Xf.GetLocation().Z;
		}
		else
		{
			FHitResult Hit;
			const double Z0 = Xf.GetLocation().Z;
			if (World->LineTraceSingleByChannel(Hit, FVector(X, Y, Z0 + TraceUp), FVector(X, Y, Z0 - TraceDown), ECC_Visibility, Q))
			{
				Gd.bHit = true;
				Gd.Z = (float)Hit.ImpactPoint.Z;
				Gd.NormalZ = (float)Hit.ImpactNormal.Z;
				Gd.bBare = Hit.GetActor() && !BareGroundTag.IsNone() && Hit.GetActor()->ActorHasTag(BareGroundTag);
			}
		}
		return Samples.Add(FIntPoint(Ix, Iy), Gd);
	};

	TArray<FTransform> Near, Far;
	TArray<float> NearDry, FarDry;
	const int32 X0 = FMath::FloorToInt(Reach.Min.X / Cell), X1 = FMath::CeilToInt(Reach.Max.X / Cell);
	const int32 Y0 = FMath::FloorToInt(Reach.Min.Y / Cell), Y1 = FMath::CeilToInt(Reach.Max.Y / Cell);
	const int32 MaxTufts = 400000;
	int32 Index = 0;
	for (int32 Cy = Y0; Cy < Y1 && Near.Num() < MaxTufts; Cy++)
	{
		for (int32 Cx = X0; Cx < X1; Cx++)
		{
			const uint32 U = (uint32)Seed;
			const float X = (Cx + Hash01(U, (uint32)Cx, (uint32)Cy, 1)) * Cell, Y = (Cy + Hash01(U, (uint32)Cx, (uint32)Cy, 2)) * Cell;
			const FVector Local = Inv.TransformPosition(FVector(X, Y, 0.0));
			if (FMath::Abs(Local.X) > HalfExtent.X || FMath::Abs(Local.Y) > HalfExtent.Y)
			{
				continue;
			}
			// a patchy sward: clumps and thin places on a scale of tens of metres
			const float Clump = 0.5f + 0.25f * FMath::Sin(X * 0.0041f + Seed * 1.7f) + 0.25f * FMath::Sin(Y * 0.0033f + X * 0.0017f + Seed * 0.7f);
			if (Hash01(U, (uint32)Cx, (uint32)Cy, 3) > 0.3f + 0.7f * Clump)
			{
				continue;
			}
			float Z = (float)Xf.GetLocation().Z;
			if (bSnapToGround)
			{
				const int32 Ix = FMath::FloorToInt(X / GroundCell), Iy = FMath::FloorToInt(Y / GroundCell);
				const float Fx = X / GroundCell - Ix, Fy = Y / GroundCell - Iy;
				const FGround* Corners[4] = { &Sample(Ix, Iy), &Sample(Ix + 1, Iy), &Sample(Ix, Iy + 1), &Sample(Ix + 1, Iy + 1) };
				const float Weights[4] = { (1.f - Fx) * (1.f - Fy), Fx * (1.f - Fy), (1.f - Fx) * Fy, Fx * Fy };
				float Sum = 0.f, W = 0.f, MinNz = 1.f;
				bool bAny = false, bBare = false;
				for (int32 K = 0; K < 4; K++)
				{
					if (!Corners[K]->bHit) continue;
					bAny = true;
					bBare |= Corners[K]->bBare;
					MinNz = FMath::Min(MinNz, Corners[K]->NormalZ);
					Sum += Corners[K]->Z * Weights[K];
					W += Weights[K];
				}
				if (!bAny || bBare || MinNz < MinSlopeNormalZ || W < 1e-3f)
				{
					continue;
				}
				Z = Sum / W;
			}
			if (Z < MinWorldZ || Z > MaxWorldZ)
			{
				continue;
			}
			const float Yaw = Hash01(U, (uint32)Cx, (uint32)Cy, 4) * 360.f;
			const float Wide = 0.8f + 0.45f * Hash01(U, (uint32)Cx, (uint32)Cy, 5);
			const float Tall = 1.f + G.HeightVariation * (Hash01(U, (uint32)Cx, (uint32)Cy, 6) * 2.f - 1.f);
			const float Dry = Hash01(U, (uint32)Cx, (uint32)Cy, 7);
			Near.Add(FTransform(FRotator(0.f, Yaw, 0.f), FVector(X, Y, Z), FVector(Wide, Wide, Tall)));
			NearDry.Add(Dry);
			if (Index++ % FMath::Max(FarEvery, 2) == 0)
			{
				Far.Add(FTransform(FRotator(0.f, Yaw, 0.f), FVector(X, Y, Z), FVector(Wide, Wide, Tall) * FarScale));
				FarDry.Add(Dry);
			}
		}
	}
	auto Fill = [](UHierarchicalInstancedStaticMeshComponent* C, const TArray<FTransform>& T, const TArray<float>& Dry)
	{
		if (T.IsEmpty()) return;
		C->AddInstances(T, false, true);
		for (int32 I = 0; I < Dry.Num(); I++)
		{
			C->SetCustomDataValue(I, 0, Dry[I], false);
		}
		C->MarkRenderStateDirty();
	};
	Fill(NearTufts, Near, NearDry);
	Fill(FarTufts, Far, FarDry);
	Tufts = Near.Num();
}

void AProceduralGrass::Rebuild()
{
	EnsureAssets();
	// the end of each ring fades the tufts down into the ground (the material's PerInstanceFadeAmount), not a pop
	NearTufts->SetCullDistances((int32)(NearDistance * 0.8f), (int32)NearDistance);
	FarTufts->SetCullDistances((int32)(FarDistance * 0.8f), (int32)FarDistance);
	Scatter();
	ApplySeason(CurrentDay());
}

void AProceduralGrass::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// in the editor a patch appears when it is loaded or dropped in; later changes are made by Rebuild
	const UWorld* World = GetWorld();
	if (World && !World->IsGameWorld() && bPreviewInEditor && Tufts == 0 && World->WorldType == EWorldType::Editor)
	{
		Rebuild();
	}
}

void AProceduralGrass::BeginPlay()
{
	Super::BeginPlay();
	Rebuild();
}

void AProceduralGrass::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Day = CurrentDay();
	if (FMath::Abs(Day - ShownDay) > 0.1f)
	{
		ApplySeason(Day);
	}
}
