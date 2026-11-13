#include "Plants/ProceduralGrass.h"
#include "Winter/LyudWinter.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "CoreGlobals.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Plants/FallenFruit.h"
#include "Plants/GameCalendar.h"
#include "Plants/PlantHerb.h"
#include "Plants/PlantMeshes.h"
#include "Plants/PlantSpecies.h"
#include "Plants/ProceduralPlant.h"
#include "Terrain/LyudTerrain.h"
#include "StaticMeshResources.h"
#include "UObject/ConstructorHelpers.h"

static TAutoConsoleVariable<float> CVarGrassDensity(TEXT("lyud.GrassDensity"), 1.f,
	TEXT("Multiplies every grass patch's tuft density (graphics levels: Foliage; a change lays the patches out again). 0.5 for a slow machine."), ECVF_Scalability);

namespace
{
	/** A patch checks its season and its density this often, seconds. */
	constexpr float GrassPatchTickSeconds = 2.f;

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

	/** A herb patch draws its herb in this many seeds (one component and mesh each), so neighbours are not twins. */
	constexpr int32 HerbSeedCount = 2;

	uint32 HerbSeedOf(int32 PatchSeed, int32 Variant)
	{
		return (uint32)PatchSeed * 7919u + 1u + (uint32)Variant * 104729u;
	}

	/** A herb outdoors that a patch can draw: not a house plant, a hedge or a seaweed. */
	bool IsPatchHerb(const FPlantSpecies& S)
	{
		return S.Herb.Form != EPlantHerbForm::None && S.Herb.Form != EPlantHerbForm::Alga && !S.bIndoor && S.CrownBox.X <= 0.f;
	}

	UHierarchicalInstancedStaticMeshComponent* MakeHerbComp(AActor* Owner, int32 Index)
	{
		// made at run time in a game world only (never saved): see AProceduralGrass::Rebuild
		UHierarchicalInstancedStaticMeshComponent* C = NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner, *FString::Printf(TEXT("Herbs%d"), Index), RF_Transient);
		C->SetupAttachment(Owner->GetRootComponent());
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(false);
		C->bAffectDynamicIndirectLighting = false;
		C->bAffectDistanceFieldLighting = false;
		C->bReceivesDecals = false;
		C->bUseAsOccluder = false;
		// the outdoor sun lights channel 1 only
		C->LightingChannels.bChannel0 = true;
		C->LightingChannels.bChannel1 = true;
		C->RegisterComponent();
		return C;
	}

	UInstancedStaticMeshComponent* MakeTufts(AActor* Owner, FName Name, USceneComponent* Parent)
	{
		// transient: the tufts are laid out again at BeginPlay (and in the editor on load), never saved in the level
		UInstancedStaticMeshComponent* C = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name, true);
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
	PrimaryActorTick.TickInterval = GrassPatchTickSeconds;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	NearTufts = MakeTufts(this, TEXT("NearTufts"), RootComponent);
	FarTufts = MakeTufts(this, TEXT("FarTufts"), RootComponent);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Grass(TEXT("/Game/Plants/M_PlantGrass.M_PlantGrass"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FloorPlant(TEXT("/Game/ForestFloor/M_FloorPlant.M_FloorPlant"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Basic(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	MaterialAsset = Grass.Object ? Grass.Object : Basic.Object;
	HerbMaterialAsset = FloorPlant.Object ? FloorPlant.Object : Basic.Object;
}

TArray<FName> AProceduralGrass::GetSpeciesNames() const
{
	TArray<FName> Out = GrassSpeciesLibrary::Names();
	for (const FPlantSpecies& S : PlantSpeciesLibrary::All())
	{
		if (IsPatchHerb(S)) Out.Add(S.Name);
	}
	return Out;
}

const FPlantSpecies* AProceduralGrass::GetHerb() const
{
	if (GrassSpeciesLibrary::Find(Species))
	{
		return nullptr;
	}
	const FPlantSpecies* S = PlantSpeciesLibrary::Find(Species);
	return S && IsPatchHerb(*S) ? S : nullptr;
}

float AProceduralGrass::HerbDrawDistance(const FPlantSpecies& S) const
{
	// a knee-high herb is a few pixels at 25 m; fireweed's spikes still read at 60 m
	return HerbDistance > 0.f ? HerbDistance : FMath::Clamp(S.Herb.Height * 4500.f, 2500.f, 7000.f);
}

bool AProceduralGrass::HerbNearPlayer(const FPlantSpecies& S) const
{
	const UWorld* W = GetWorld();
	const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->PlayerCameraManager)
	{
		return false;
	}
	const FVector L = GetActorTransform().InverseTransformPosition(PC->PlayerCameraManager->GetCameraLocation());
	const double Margin = HerbDrawDistance(S) + 15000.0;
	return FMath::Abs(L.X) < HalfExtent.X + Margin && FMath::Abs(L.Y) < HalfExtent.Y + Margin;
}

void AProceduralGrass::EnsureHerbAssets(const FPlantSpecies& S)
{
	if (HerbComps.Num() != HerbSeedCount)
	{
		HerbComps.Reset();
		for (int32 V = 0; V < HerbSeedCount; V++) HerbComps.Add(MakeHerbComp(this, V));
	}
	if (BuiltSpecies == Species && HerbMaterials.Num() == PlantHerbs::NumSlots)
	{
		return;
	}
	// the species' three materials, shared by its seeds (as AForestFloor's herbs)
	HerbMaterials.Reset();
	for (int32 Slot = 0; Slot < PlantHerbs::NumSlots; Slot++)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(HerbMaterialAsset, this);
		// tall herbs wave, low mats barely stir
		M->SetScalarParameterValue(TEXT("SwayAmp"), S.Herb.Height > 0.5f ? 9.f : 2.5f);
		M->SetScalarParameterValue(TEXT("HeightRef"), FMath::Max(S.Herb.Height * 100.f, 10.f));
		HerbMaterials.Add(M);
	}
	UTexture2D* LeafTexture = PlantHerbs::BuildLeafTexture(this, S);
	HerbMaterials[PlantHerbs::Leaves]->SetTextureParameterValue(TEXT("LeafDetail"), LeafTexture ? LeafTexture : PlantMeshes::BuildLeafDetail(this, S.Leaf));
	if (S.Herb.Flower != EPlantFlowerForm::None)
	{
		HerbMaterials[PlantHerbs::Flowers]->SetTextureParameterValue(TEXT("LeafDetail"), PlantHerbs::BuildFlowerAtlas(this, S));
	}
	HerbMaterials[PlantHerbs::Stems]->SetVectorParameterValue(TEXT("Color"), S.Herb.StemColour);
	const float Draw = HerbDrawDistance(S);
	for (UHierarchicalInstancedStaticMeshComponent* C : HerbComps)
	{
		for (int32 Slot = 0; Slot < PlantHerbs::NumSlots; Slot++) C->SetMaterial(Slot, HerbMaterials[Slot]);
		// the end fades the plants down into the ground (M_FloorPlant's PerInstanceFadeAmount), not a pop
		C->SetCullDistances((int32)(Draw * 0.8f), (int32)Draw);
	}
	HerbMeshes.Reset();
	HerbMeshes.SetNum(HerbSeedCount);
	for (uint32& Key : HerbKeys) Key = 0;
	BuiltSpecies = Species;
	ShownDay = -1000.f;
}

void AProceduralGrass::ApplyHerbSeason(const FPlantSpecies& S, float Day)
{
	if (HerbComps.Num() != HerbSeedCount || HerbMaterials.Num() != PlantHerbs::NumSlots || HerbMeshes.Num() != HerbSeedCount)
	{
		return;
	}
	const FLinearColor Frost(0.12f, 0.07f, 0.03f);
	// low evergreen mats (lingonberry, mountain avens) are under the snow in winter; taller herbs are dead and gone by then
	const bool bUnderSnow = S.Herb.Height < 0.5f && ULyudWinterSubsystem::SnowOnGround(GetWorld(), PlantModel::SnowCover(Day)) > 0.3f;
	bHerbAlive = false;
	for (int32 V = 0; V < HerbSeedCount; V++)
	{
		const uint32 VSeed = HerbSeedOf(Seed, V);
		const float Age = HerbAge * (1.f + V);
		const PlantHerbs::FState St = PlantHerbs::State(S, VSeed, Age, Day);
		if (V == 0)
		{
			// frost browns the leaves; a patterned blade keeps its pattern under the browning
			const FLinearColor Leaf = PlantHerbs::LeafTextureCarriesColour(S) ? FLinearColor::White : St.LeafColour;
			HerbMaterials[PlantHerbs::Leaves]->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(Leaf, Frost, St.Wither));
			HerbMaterials[PlantHerbs::Stems]->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(S.Herb.StemColour, Frost, St.Wither));
			HerbMaterials[PlantHerbs::Flowers]->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(FLinearColor::White, Frost * 2.f, St.Wither));
		}
		if (St.Key != HerbKeys[V] || !HerbMeshes[V])
		{
			PlantHerbs::FHerbGeometry G0, G1;
			float H = 0.f, R = 0.f;
			PlantHerbs::Build(S, VSeed, St, G0, H, R);
			const FPlantSpecies Lite = PlantHerbs::LiteSpecies(S);
			PlantHerbs::Build(Lite, VSeed, PlantHerbs::State(Lite, VSeed, Age, Day), G1, H, R);
			// LOD 1 from ~10 m for a knee-high herb, ~18 m for a tall one (as the forest floor's)
			const float Screen = FMath::Clamp(S.Herb.Height * 100.f / (S.Herb.Height > 0.5f ? 1800.f : 1000.f), 0.005f, 0.5f);
			// a new mesh, never over the one drawn (PlantHerbs::BuildMeshLods); the old one goes with the last reference
			HerbMeshes[V] = PlantHerbs::BuildMeshLods(this, *FString::Printf(TEXT("SM_HerbPatch_%s_%d"), *S.Name.ToString(), V), G0, G1, Screen);
			HerbComps[V]->SetStaticMesh(HerbMeshes[V]);
			HerbKeys[V] = St.Key;
			if (V == 0) TuftTriangles = G0.NumTriangles();
		}
		HerbComps[V]->SetVisibility(St.bAlive && !bUnderSnow);
		bHerbAlive |= St.bAlive;
	}
	ShownDay = Day;
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
	if (GetHerb())
	{
		if (!bHerbBuilt)
		{
			// a herb patch is laid out only when the player first comes near (Tick)
			return FString::Printf(TEXT("%s (herb): not built yet (the player has not been near)"), *Species.ToString());
		}
		int32 Drawn = 0;
		for (const UHierarchicalInstancedStaticMeshComponent* C : HerbComps) Drawn += C ? C->GetInstanceCount() : 0;
		return FString::Printf(TEXT("%s (herb): %d plants, %d triangles each near, %d instances in %d seeds%s"), *Species.ToString(), Tufts, TuftTriangles,
			Drawn, HerbComps.Num(), bHerbAlive ? TEXT("") : TEXT(", none above ground now"));
	}
	return FString::Printf(TEXT("%s: %d tufts, %d triangles each, %d drawn near / %d far"), *Species.ToString(), Tufts, TuftTriangles,
		NearTufts ? NearTufts->GetInstanceCount() : 0, FarTufts ? FarTufts->GetInstanceCount() : 0);
}

void AProceduralGrass::EnsureAssets()
{
	if (const FPlantSpecies* Herb = GetHerb())
	{
		EnsureHerbAssets(*Herb);
		return;
	}
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
	for (UInstancedStaticMeshComponent* C : { NearTufts.Get(), FarTufts.Get() })
	{
		C->SetStaticMesh(TuftMesh);
		C->SetMaterial(0, Material);
	}
	BuiltSpecies = Species;
	ShownDay = -1000.f;
}

void AProceduralGrass::ApplySeason(float Day)
{
	if (const FPlantSpecies* Herb = GetHerb())
	{
		ApplyHerbSeason(*Herb, Day);
		return;
	}
	if (!Material)
	{
		return;
	}
	const FGrassSpecies& G = GetSpecies();
	using PlantModel::SmoothStep;
	// green from spring to the autumn turn, then straw standing through the winter
	const float Green = SmoothStep(G.GreenUpStart, G.GreenUpFull, Day) * (1.f - SmoothStep(G.YellowStart, G.YellowFull, Day));
	const float Snow = ULyudWinterSubsystem::SnowOnGround(GetWorld(), PlantModel::SnowCover(Day));
	// new growth in spring, then the snow flattens it
	const float Grow = Day < 200.f ? SmoothStep(G.GreenUpStart, G.GreenUpFull + 25.f, Day) : 1.f - SmoothStep(310.f, 345.f, Day);
	Material->SetScalarParameterValue(TEXT("Dryness"), 1.f - Green);
	Material->SetScalarParameterValue(TEXT("Snow"), Snow);
	Material->SetScalarParameterValue(TEXT("HeightScale"), FMath::Lerp(G.WinterHeight, 1.f, Grow));
	ShownDay = Day;
}

void AProceduralGrass::Scatter()
{
	BuiltDensity = CVarGrassDensity.GetValueOnGameThread();
	NearTufts->ClearInstances();
	FarTufts->ClearInstances();
	for (UHierarchicalInstancedStaticMeshComponent* C : HerbComps)
	{
		if (C) C->ClearInstances();
	}
	Tufts = 0;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FGrassSpecies& G = GetSpecies();
	// a herb patch: the herb's plants instead of tufts, by the patch's own density and habitat
	const FPlantSpecies* Herb = GetHerb();
	if (Herb && HerbComps.Num() != HerbSeedCount)
	{
		return;
	}
	const EGrassHabitat Habitat = Herb ? HerbHabitat : G.Habitat;
	const float HeightVariation = Herb ? 0.15f : G.HeightVariation;
	const float Density = FMath::Max((Herb ? HerbDensity : G.Density) * DensityScale * CVarGrassDensity.GetValueOnGameThread(), 0.01f);
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

	const ULyudTerrainMask* Mask = bUseTerrainMask ? ULyudTerrainMask::Get() : nullptr;
	TArray<FTransform> Near, Far;
	TArray<float> NearDry, FarDry;
	TArray<FTransform> HerbXfs[HerbSeedCount];
	const int32 X0 = FMath::FloorToInt(Reach.Min.X / Cell), X1 = FMath::CeilToInt(Reach.Max.X / Cell);
	const int32 Y0 = FMath::FloorToInt(Reach.Min.Y / Cell), Y1 = FMath::CeilToInt(Reach.Max.Y / Cell);
	const int32 MaxTufts = 400000;
	int32 Index = 0;
	for (int32 Cy = Y0; Cy < Y1 && Near.Num() < MaxTufts && Index < MaxTufts; Cy++)
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
			if (Hash01(U, (uint32)Cx, (uint32)Cy, 3) > 0.55f + 0.45f * Clump)
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
			if (Mask)
			{
				// the terrain says how much grass of this kind grows here; each tuft is kept with that chance
				const FVector At(X, Y, Z);
				float Allowed = 0.f;
				switch (Habitat)
				{
				case EGrassHabitat::Dune: Allowed = Mask->GetDuneGrassDensity(At); break;
				case EGrassHabitat::Wet:
					// a herb of the wet ground still wants soil: not the snow, cinder or bare rock that may lie wet too
					Allowed = Mask->GetWetness(At) * (Herb ? PlantModel::SmoothStep(0.05f, 0.3f, Mask->GetGrassDensity(At)) : 1.f);
					break;
				default: Allowed = Mask->GetGrassDensity(At) * (1.f - 0.6f * Mask->GetWetness(At)); break;
				}
				if (Hash01(U, (uint32)Cx, (uint32)Cy, 8) >= Allowed)
				{
					continue;
				}
			}
			const float Yaw = Hash01(U, (uint32)Cx, (uint32)Cy, 4) * 360.f;
			const float Wide = 0.8f + 0.45f * Hash01(U, (uint32)Cx, (uint32)Cy, 5);
			const float Tall = 1.f + HeightVariation * (Hash01(U, (uint32)Cx, (uint32)Cy, 6) * 2.f - 1.f);
			if (Herb)
			{
				const float Girth = 0.85f + 0.3f * Hash01(U, (uint32)Cx, (uint32)Cy, 5);
				HerbXfs[FMath::Min((int32)(Hash01(U, (uint32)Cx, (uint32)Cy, 9) * HerbSeedCount), HerbSeedCount - 1)].Add(
					FTransform(FRotator(0.f, Yaw, 0.f), FVector(X, Y, Z), FVector(Girth, Girth, Tall)));
				Index++;
				continue;
			}
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
	auto Fill = [](UInstancedStaticMeshComponent* C, const TArray<FTransform>& T, const TArray<float>& Dry)
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
	if (Herb)
	{
		Tufts = 0;
		for (int32 V = 0; V < HerbSeedCount; V++)
		{
			if (HerbXfs[V].IsEmpty()) continue;
			HerbComps[V]->AddInstances(HerbXfs[V], false, true, false);
			Tufts += HerbXfs[V].Num();
		}
	}
}

void AProceduralGrass::Rebuild()
{
	if (const FPlantSpecies* Herb = GetHerb())
	{
		// herb patches only in a game world: their components are made at run time and never saved with the level
		const UWorld* World = GetWorld();
		if (!World || !World->IsGameWorld())
		{
			return;
		}
		EnsureAssets();
		// the meshes first, then the plants on them
		ApplyHerbSeason(*Herb, CurrentDay());
		Scatter();
		bHerbBuilt = true;
		return;
	}
	EnsureAssets();
	// the end of each ring fades the tufts down into the ground (the material's PerInstanceFadeAmount), not a pop
	const float Scale = GetSpecies().CullScale;
	NearTufts->SetCullDistances((int32)(NearDistance * Scale * 0.8f), (int32)(NearDistance * Scale));
	FarTufts->SetCullDistances((int32)(FarDistance * Scale * 0.8f), (int32)(FarDistance * Scale));
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
	// a herb patch is built by Tick when the player comes near: the North's ~60 patches cost nothing at load or far away
	if (!GetHerb())
	{
		Rebuild();
	}
}

void AProceduralGrass::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Day = CurrentDay();
	// the graphics level changed the density: the tufts are laid out again, one patch a frame across the world (the patches
	// tick on the same beat, and all of them in one frame would stall it); a patch waiting for its frame ticks every frame
	static uint64 GrassRebuildFrame = 0;
	bool bWaiting = false;
	if (const FPlantSpecies* Herb = GetHerb())
	{
		// far from the player a herb patch does nothing (ShownDay goes stale; it catches up when he comes near); near ones
		// are built and re-seasoned one a frame, the same turn as the density rebuild
		if (HerbNearPlayer(*Herb))
		{
			const bool bLayout = !bHerbBuilt || CVarGrassDensity.GetValueOnGameThread() != BuiltDensity;
			bWaiting = bLayout || FMath::Abs(Day - ShownDay) > 0.1f;
			if (bWaiting && GrassRebuildFrame != GFrameCounter)
			{
				GrassRebuildFrame = GFrameCounter;
				if (bLayout)
				{
					Rebuild();
				}
				else
				{
					ApplySeason(Day);
				}
				bWaiting = false;
			}
		}
	}
	else
	{
		if (FMath::Abs(Day - ShownDay) > 0.1f)
		{
			ApplySeason(Day);
		}
		bWaiting = BuiltDensity >= 0.f && CVarGrassDensity.GetValueOnGameThread() != BuiltDensity;
		if (bWaiting && GrassRebuildFrame != GFrameCounter)
		{
			GrassRebuildFrame = GFrameCounter;
			Rebuild();
			bWaiting = false;
		}
	}
	const float Interval = bWaiting ? 0.f : GrassPatchTickSeconds;
	if (GetActorTickInterval() != Interval)
	{
		SetActorTickInterval(Interval);
	}
}
