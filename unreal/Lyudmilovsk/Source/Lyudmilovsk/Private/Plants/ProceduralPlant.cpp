#include "Plants/ProceduralPlant.h"
#include "Winter/LyudWinter.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HeroReachComponent.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Plants/FallenFruit.h"
#include "Plants/GameCalendar.h"
#include "Plants/PlantHerb.h"
#include "Plants/PlantMeshes.h"
#include "Plants/TreeBakeCache.h"
#include "StaticMeshResources.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Scene 01's outside sun lights channel 1 only; channel 0 keeps the default lights too. */
	void OutdoorLighting(UPrimitiveComponent* C)
	{
		C->LightingChannels.bChannel0 = true;
		C->LightingChannels.bChannel1 = true;
		C->SetCanEverAffectNavigation(false);
	}

	UInstancedStaticMeshComponent* MakeInstanced(AActor* Owner, FName Name, USceneComponent* Parent)
	{
		UInstancedStaticMeshComponent* ISM = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		ISM->SetupAttachment(Parent);
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->bDisableCollision = true;
		ISM->SetMobility(EComponentMobility::Movable);
		OutdoorLighting(ISM);
		return ISM;
	}

	void SetInstances(UInstancedStaticMeshComponent* ISM, const TArray<FTransform>& T)
	{
		ISM->ClearInstances();
		if (!T.IsEmpty())
		{
			ISM->AddInstances(T, false, false, false);
		}
	}

	int32 NumTriangles(const UStaticMesh* Mesh)
	{
		const FStaticMeshRenderData* RD = Mesh ? Mesh->GetRenderData() : nullptr;
		return RD && RD->LODResources.Num() ? RD->LODResources[0].GetNumTriangles() : 0;
	}
}

AProceduralPlant::AProceduralPlant()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Basic(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BarkMat(TEXT("/Game/Plants/M_PlantBark.M_PlantBark"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LeafMat(TEXT("/Game/Plants/M_PlantLeaf.M_PlantLeaf"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> NeedleMat(TEXT("/Game/Plants/M_PlantNeedle.M_PlantNeedle"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FruitMat(TEXT("/Game/Plants/M_PlantFruit.M_PlantFruit"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	CrownEllipsoid = Sphere.Object;
	CrownCone = Cone.Object;
	BaseMaterial = Basic.Object;
	BarkMaterialAsset = BarkMat.Object ? BarkMat.Object : Basic.Object;
	LeafMaterialAsset = LeafMat.Object ? LeafMat.Object : Basic.Object;
	NeedleMaterialAsset = NeedleMat.Object ? NeedleMat.Object : LeafMaterialAsset;
	FruitMaterialAsset = FruitMat.Object ? FruitMat.Object : Basic.Object;

	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Wood = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wood"));
	Wood->SetupAttachment(Body);
	Wood->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Wood->SetMobility(EComponentMobility::Movable);
	OutdoorLighting(Wood);
	for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
	{
		LeafComponents.Add(MakeInstanced(this, *FString::Printf(TEXT("Leaves%d"), K), Body));
		LeafComponents[K]->NumCustomDataFloats = 1;
	}
	FruitComponent = MakeInstanced(this, TEXT("Fruit"), Body);
	FruitComponent->SetStaticMesh(Sphere.Object);
	FruitComponent->NumCustomDataFloats = FPlantSkeleton::FInstances::FruitDataFloats;
	BloomComponent = MakeInstanced(this, TEXT("Bloom"), Body);
	BloomComponent->NumCustomDataFloats = 1;
	BloomComponent->SetCastShadow(false);
	for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
	{
		UInstancedStaticMeshComponent* Litter = MakeInstanced(this, *FString::Printf(TEXT("Litter%d"), K), RootComponent);
		Litter->SetCastShadow(false);
		LitterComponents.Add(Litter);
		UInstancedStaticMeshComponent* Air = MakeInstanced(this, *FString::Printf(TEXT("Falling%d"), K), RootComponent);
		Air->SetCastShadow(false);
		FallingComponents.Add(Air);
	}

	FarTree = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FarTree"));
	FarTree->SetupAttachment(Body);
	FarTree->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FarTree->SetMobility(EComponentMobility::Movable);
	FarTree->SetVisibility(false);
	OutdoorLighting(FarTree);
	SummaryTrunk = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SummaryTrunk"));
	SummaryCrown = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SummaryCrown"));
	for (UStaticMeshComponent* C : { SummaryTrunk.Get(), SummaryCrown.Get() })
	{
		C->SetupAttachment(RootComponent);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetMobility(EComponentMobility::Movable);
		C->SetVisibility(false);
		OutdoorLighting(C);
	}
	SummaryTrunk->SetStaticMesh(Cylinder.Object);
	Herb = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Herb"));
	Herb->SetupAttachment(Body);
	PotMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pot"));
	PotMesh->SetupAttachment(RootComponent);
	for (UStaticMeshComponent* C : { Herb.Get(), PotMesh.Get() })
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetMobility(EComponentMobility::Movable);
		C->SetVisibility(false);
		OutdoorLighting(C);
	}
	SummaryCrown->SetStaticMesh(Sphere.Object);

	TrunkCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("TrunkCollision"));
	TrunkCollision->SetupAttachment(RootComponent);
	TrunkCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	TrunkCollision->SetCanEverAffectNavigation(false);
}

AProceduralPlant::~AProceduralPlant() = default;

TArray<FName> AProceduralPlant::GetSpeciesNames() const
{
	return PlantSpeciesLibrary::Names();
}

const FPlantSpecies& AProceduralPlant::GetSpecies() const
{
	const FPlantSpecies* S = PlantSpeciesLibrary::Find(Species);
	return S ? *S : PlantSpeciesLibrary::All()[0];
}

bool AProceduralPlant::IsHerb() const
{
	return GetSpecies().Herb.Form != EPlantHerbForm::None;
}

EPlantPot AProceduralPlant::PotKind() const
{
	if (Pot != EPlantPot::Auto) return Pot;
	// outdoors a plant grows in the ground unless it is given a pot
	const FPlantSpecies& S = GetSpecies();
	return !S.bIndoor ? EPlantPot::None : S.Pot != EPlantPot::Auto ? S.Pot : EPlantPot::Terracotta;
}

float AProceduralPlant::PotSize() const
{
	if (PotDiameter > 0.f) return PotDiameter;
	const FPlantSpecies& S = GetSpecies();
	return S.PotDiameter > 0.f ? S.PotDiameter * 100.f : FMath::Clamp(S.Herb.Spread * 60.f, 12.f, 40.f);
}

void AProceduralPlant::ApplyPot()
{
	const EPlantPot Kind = PotKind();
	const float D = PotSize();
	if (Kind == BuiltPot && D == BuiltPotSize && (Kind == EPlantPot::None || PotStaticMesh))
	{
		return;
	}
	BuiltPot = Kind;
	BuiltPotSize = D;
	if (Kind == EPlantPot::None)
	{
		PotMesh->SetVisibility(false);
		Body->SetRelativeLocation(FVector::ZeroVector);
		return;
	}
	if (!PotMaterial)
	{
		PotMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		SoilMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		SoilMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.035f, 0.025f, 0.018f));
		SoilMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.95f);
	}
	const FLinearColor Colour = Kind == EPlantPot::Ceramic ? FLinearColor(0.025f, 0.09f, 0.06f) : Kind == EPlantPot::White ? FLinearColor(0.75f, 0.75f, 0.72f)
		: FLinearColor(0.33f, 0.11f, 0.05f);
	PotMaterial->SetVectorParameterValue(TEXT("Color"), Colour);
	PotMaterial->SetScalarParameterValue(TEXT("Roughness"), Kind == EPlantPot::Ceramic ? 0.25f : Kind == EPlantPot::White ? 0.5f : 0.9f);
	PotStaticMesh = PlantHerbs::BuildPot(this, D);
	PotMesh->SetStaticMesh(PotStaticMesh);
	PotMesh->SetMaterial(0, PotMaterial);
	PotMesh->SetMaterial(1, SoilMaterial);
	PotMesh->SetMaterial(2, SoilMaterial);
	PotMesh->SetVisibility(true);
	Body->SetRelativeLocation(FVector(0.f, 0.f, PlantHerbs::PotSoilZ(D)));
}

float AProceduralPlant::GetAge() const
{
	if (DevAge >= 0.f)
	{
		return DevAge;
	}
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return EditorPreviewAge >= 0.f ? EditorPreviewAge : AgeAtStart;
	}
	if (bDemoGrowth)
	{
		const float F = FMath::Clamp(((float)UGameCalendar::GameSeconds(World) - DemoDelay) / FMath::Max(DemoSeconds, 0.1f), 0.f, 1.f);
		return FMath::Lerp(AgeAtStart, FMath::Max(DemoTargetAge, AgeAtStart), F);
	}
	const UGameCalendar* Calendar = World->GetSubsystem<UGameCalendar>();
	const FDateTime Start = Calendar ? Calendar->StartDate : GetDefault<UGameCalendar>()->StartDate;
	return AgeAtStart + (float)UGameCalendar::YearsBetween(Start, UGameCalendar::NowIn(World));
}

FDateTime AProceduralPlant::CurrentDate() const
{
	const UWorld* World = GetWorld();
	return UGameCalendar::NowIn(World) + FTimespan::FromDays(DemoDaysPerSecond * UGameCalendar::GameSeconds(World));
}

float AProceduralPlant::CurrentDay() const
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
	return UGameCalendar::DayOfYear(CurrentDate());
}

FPlantSummary AProceduralPlant::GetSummary() const
{
	return PlantModel::Summary(GetSpecies(), (uint32)Seed, GetAge(), CurrentDay());
}

bool AProceduralPlant::WantsSummary() const
{
	if (IsHerb())
	{
		return false;
	}
	if (bForceSummary || GetSpecies().bPreferBake)
	{
		return true;
	}
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->PlayerCameraManager)
	{
		return false;
	}
	return FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), GetActorLocation()) > SummaryDistance;
}

void AProceduralPlant::EnsureMeshes()
{
	if (!CrownMaterial)
	{
		WoodMaterial = UMaterialInstanceDynamic::Create(BarkMaterialAsset, this);
		Wood->SetMaterial(0, WoodMaterial);
		SummaryTrunk->SetMaterial(0, WoodMaterial);
		LeafMaterials.Reset();
		for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
		{
			LeafMaterials.Add(UMaterialInstanceDynamic::Create(LeafMaterialAsset, this));
		}
		FruitMaterial = UMaterialInstanceDynamic::Create(FruitMaterialAsset, this);
		FruitComponent->SetMaterial(0, FruitMaterial);
		BloomMaterial = UMaterialInstanceDynamic::Create(LeafMaterialAsset, this);
		BloomComponent->SetMaterial(0, BloomMaterial);
		LitterMaterials.Reset();
		for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
		{
			LitterMaterials.Add(UMaterialInstanceDynamic::Create(LeafMaterialAsset, this));
			LitterComponents[K]->SetMaterial(0, LitterMaterials[K]);
			FallingComponents[K]->SetMaterial(0, LeafMaterials[K]);
		}
		CrownMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		CrownMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.85f);
		SummaryCrown->SetMaterial(0, CrownMaterial);
	}
	if (BuiltSpecies != Species || BuiltSeed != Seed || !Skeleton.IsBuilt() || !LeafMesh)
	{
		const FPlantSpecies& S = GetSpecies();
		Skeleton.Build(S, (uint32)Seed);
		UTexture2D* Albedo = nullptr;
		UTexture2D* Normal = nullptr;
		if (SharedBarkAlbedo && SharedBarkNormal)
		{
			Albedo = SharedBarkAlbedo;
			Normal = SharedBarkNormal;
		}
		else
		{
			PlantMeshes::BuildBarkTextures(this, S, (uint32)Seed, Albedo, Normal);
		}
		BarkAlbedo = Albedo;
		BarkNormal = Normal;
		LeafDetail = SharedLeafDetail ? SharedLeafDetail.Get() : PlantMeshes::BuildLeafDetail(this, S.Leaf);
		LeafMesh = PlantMeshes::BuildLeafCluster(this, S);
		SummaryCrown->SetStaticMesh(S.bConicalCrown && CrownCone ? CrownCone.Get() : CrownEllipsoid.Get());
		SingleLeafMesh = PlantMeshes::BuildSingleLeaf(this, S);
		if (!FruitDetail)
		{
			FruitDetail = PlantMeshes::BuildFruitDetail(this);
		}
		if (S.FruitSize > 0.f)
		{
			FruitMesh = PlantMeshes::BuildFruit(this, S);
			FruitComponent->SetStaticMesh(FruitMesh);
		}
		if (S.Blossom.Form != EPlantBlossomForm::None)
		{
			BloomMesh = PlantMeshes::BuildBlossom(this, S);
			BloomAtlas = PlantMeshes::BuildBlossomAtlas(this, S.Blossom);
			BloomComponent->SetStaticMesh(BloomMesh);
			// a flower is a pixel or two beyond ~1000 of its own sizes: not drawn past that
			const FPlantBlossom& Bl = S.Blossom;
			const float Extent = Bl.Size * 100.f * (Bl.Form == EPlantBlossomForm::Blossom ? Bl.Spread : 1.f);
			BloomComponent->SetCullDistances(0, FMath::Clamp(Extent * 1000.f, 3000.f, 9000.f));
			BloomMaterial->SetTextureParameterValue(TEXT("LeafDetail"), BloomAtlas);
			// flowers swing with their twigs and flutter a little, like leaves
			BloomMaterial->SetScalarParameterValue(TEXT("SwayAmp"), S.Sway);
			BloomMaterial->SetScalarParameterValue(TEXT("Flutter"), 1.f);
			BloomMaterial->SetScalarParameterValue(TEXT("Bend"), S.bIndoor ? 0.f : S.WindBend);
			BloomMaterial->SetScalarParameterValue(TEXT("FootFromActor"), 1.f);
		}
		else
		{
			BloomMesh = nullptr;
			BloomComponent->SetStaticMesh(nullptr);
		}
		FruitMaterial->SetTextureParameterValue(TEXT("FruitDetail"), FruitDetail);
		FruitMaterial->SetVectorParameterValue(TEXT("GroundUnripe"), S.FruitUnripeColour);
		FruitMaterial->SetVectorParameterValue(TEXT("GroundRipe"), S.FruitGroundColour);
		FruitMaterial->SetVectorParameterValue(TEXT("BlushColour"), S.FruitColour);
		GroundGrid.Reset();
		Falling.Reset();

		// needle cards are masked by their texture's alpha; blades are cut out in the geometry
		for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
		{
			LeafMaterials[K] = UMaterialInstanceDynamic::Create(S.Leaf.Form == EPlantLeafForm::Needle ? NeedleMaterialAsset.Get() : LeafMaterialAsset.Get(), this);
			FallingComponents[K]->SetMaterial(0, LeafMaterials[K]);
		}
		WoodMaterial->SetTextureParameterValue(TEXT("BarkAlbedo"), BarkAlbedo);
		WoodMaterial->SetTextureParameterValue(TEXT("BarkNormal"), BarkNormal);
		WoodMaterial->SetVectorParameterValue(TEXT("Color"), S.Bark.RidgeColour); // when the plant materials are missing
		WoodMaterial->SetVectorParameterValue(TEXT("TwigColour"), S.Bark.TwigColour);
		WoodMaterial->SetScalarParameterValue(TEXT("SwayAmp"), S.bIndoor ? 0.f : S.Sway);
		WoodMaterial->SetScalarParameterValue(TEXT("Bend"), S.bIndoor ? 0.f : S.WindBend);
		for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
		{
			// leaves on the tree swing with their twigs and flutter; the litter lies still
			LeafMaterials[K]->SetScalarParameterValue(TEXT("SwayAmp"), S.bIndoor ? 0.f : S.Sway);
			LeafMaterials[K]->SetScalarParameterValue(TEXT("Flutter"), S.bIndoor ? 0.f : 1.f);
			// its instances are the tree's leaf clusters: the tree's foot is the actor's place, and the whole crown leans with the wind
			LeafMaterials[K]->SetScalarParameterValue(TEXT("Bend"), S.bIndoor ? 0.f : S.WindBend);
			LeafMaterials[K]->SetScalarParameterValue(TEXT("FootFromActor"), 1.f);
			LitterMaterials[K]->SetScalarParameterValue(TEXT("SwayAmp"), 0.f);
			LitterMaterials[K]->SetScalarParameterValue(TEXT("Flutter"), 0.f);
		}
		WoodMaterial->SetVectorParameterValue(TEXT("BaseRidge"), S.Bark.BaseRidgeColour);
		WoodMaterial->SetVectorParameterValue(TEXT("BaseFurrow"), S.Bark.BaseFurrowColour);
		for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
		{
			LeafMaterials[K]->SetTextureParameterValue(TEXT("LeafDetail"), LeafDetail);
			LeafComponents[K]->SetStaticMesh(LeafMesh);
			LeafComponents[K]->SetMaterial(0, LeafMaterials[K]);
			LitterMaterials[K]->SetTextureParameterValue(TEXT("LeafDetail"), LeafDetail);
			LitterComponents[K]->SetStaticMesh(SingleLeafMesh);
			FallingComponents[K]->SetStaticMesh(SingleLeafMesh);
		}
		BuiltSpecies = Species;
		BuiltSeed = Seed;
		WoodAge = -1.f;
	}
}

void AProceduralPlant::ApplyHerb(float Age, float Day)
{
	const FPlantSpecies& S = GetSpecies();
	if (BuiltSpecies != Species || BuiltSeed != Seed || HerbMaterials.Num() != PlantHerbs::NumSlots)
	{
		// the tree's parts stay empty; its textures and materials are made once for the species
		for (UInstancedStaticMeshComponent* L : LeafComponents) SetInstances(L, {});
		for (UInstancedStaticMeshComponent* L : LitterComponents) SetInstances(L, {});
		for (UInstancedStaticMeshComponent* L : FallingComponents) SetInstances(L, {});
		SetInstances(FruitComponent, {});
		SetInstances(BloomComponent, {});
		Falling.Reset();
		PendingDrops.Reset();
		Skeleton = FPlantSkeleton();
		LeafMesh = nullptr;
		HerbLeafTexture = PlantHerbs::BuildLeafTexture(this, S);
		HerbFlowerAtlas = S.Herb.Flower != EPlantFlowerForm::None ? PlantHerbs::BuildFlowerAtlas(this, S) : nullptr;
		HerbMaterials.Reset();
		for (int32 K = 0; K < PlantHerbs::NumSlots; K++)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(LeafMaterialAsset, this);
			// a herb outdoors stirs in the wind a little (the leaf material's own flutter); indoors it is still
			M->SetScalarParameterValue(TEXT("SwayAmp"), 0.f);
			M->SetScalarParameterValue(TEXT("Flutter"), S.bIndoor || S.Herb.Form == EPlantHerbForm::Column ? 0.f : 0.25f);
			HerbMaterials.Add(M);
		}
		HerbMaterials[PlantHerbs::Leaves]->SetTextureParameterValue(TEXT("LeafDetail"), HerbLeafTexture ? HerbLeafTexture.Get() : PlantMeshes::BuildLeafDetail(this, S.Leaf));
		HerbMaterials[PlantHerbs::Stems]->SetVectorParameterValue(TEXT("Color"), S.Herb.StemColour);
		if (HerbFlowerAtlas)
		{
			HerbMaterials[PlantHerbs::Flowers]->SetTextureParameterValue(TEXT("LeafDetail"), HerbFlowerAtlas);
		}
		for (int32 K = 0; K < PlantHerbs::NumSlots; K++)
		{
			Herb->SetMaterial(K, HerbMaterials[K]);
		}
		// beds and pots are small: gone beyond the distance they would be a few pixels
		// a hedge is part of the street and stays
		const float Cull = S.CrownBox.X > 0.f ? 0.f : S.bIndoor ? 4000.f : 9000.f;
		Herb->SetCullDistance(Cull);
		Herb->LDMaxDrawDistance = Cull;
		Herb->SetCastShadow(true);
		BuiltSpecies = Species;
		BuiltSeed = Seed;
		HerbKey = 0;
	}
	Wood->SetVisibility(false);
	FarTree->SetVisibility(false);
	FruitComponent->SetVisibility(false);
	BloomComponent->SetVisibility(false);
	for (UInstancedStaticMeshComponent* L : LeafComponents) L->SetVisibility(false);
	SetActorHiddenInGame(false);

	const PlantHerbs::FState St = PlantHerbs::State(S, (uint32)Seed, Age, Day);
	// frost browns the leaves; a patterned blade keeps its pattern under the browning
	const FLinearColor Frost(0.12f, 0.07f, 0.03f);
	const FLinearColor Leaf = PlantHerbs::LeafTextureCarriesColour(S) ? FLinearColor::White : St.LeafColour;
	HerbMaterials[PlantHerbs::Leaves]->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(Leaf, Frost, St.Wither));
	HerbMaterials[PlantHerbs::Stems]->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(S.Herb.StemColour, Frost, St.Wither));
	// a hedge's flower section is its shaded inside
	HerbMaterials[PlantHerbs::Flowers]->SetVectorParameterValue(TEXT("Color"), S.CrownBox.X > 0.f ? St.LeafColour * 0.3f
		: FMath::Lerp(FLinearColor::White, Frost * 2.f, St.Wither));
	if (St.Key != HerbKey || !HerbMesh)
	{
		const double T0 = FPlatformTime::Seconds();
		PlantHerbs::FHerbGeometry G;
		float Height = 0.f, Radius = 0.f;
		PlantHerbs::Build(S, (uint32)Seed, St, G, Height, Radius);
		HerbMesh = PlantHerbs::BuildMesh(this, HerbMesh, *FString::Printf(TEXT("SM_Herb_%s"), *S.Name.ToString()), G);
		if (Herb->GetStaticMesh() != HerbMesh)
		{
			Herb->SetStaticMesh(HerbMesh);
		}
		Herb->MarkRenderStateDirty();
		HerbBuildMs = (float)((FPlatformTime::Seconds() - T0) * 1000.0);
		HerbKey = St.Key;
		LastHeight = Height;
		LastReach = Radius;
		LastLeaves = G.Tris[PlantHerbs::Leaves].Num();
		LastLitter = 0;
		LastFruit = 0;
	}
	Herb->SetVisibility(St.bAlive);
	// a pot and a hedge are solid; a herb in a bed is walked through
	const bool bPot = PotKind() != EPlantPot::None;
	const bool bHedge = S.CrownBox.X > 0.f;
	const float D = bHedge ? 100.f * FMath::Min(S.CrownBox.X, S.CrownBox.Y) * FMath::Clamp(LastReach / (0.5f * S.CrownBox.Y), 0.3f, 1.f) : PotSize();
	TrunkCollision->SetCollisionEnabled(bPot || bHedge ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (bPot || bHedge)
	{
		const float HalfH = FMath::Max(0.5f * ((bPot ? PlantHerbs::PotSoilZ(D) : 0.f) + LastHeight * 100.f), 0.5f * D);
		TrunkCollision->SetCapsuleSize(0.5f * D, HalfH);
		TrunkCollision->SetRelativeLocation(FVector(0, 0, HalfH));
	}
	ShownAge = Age;
	ShownDay = Day;
	bShownSummary = false;
}

void AProceduralPlant::Apply(float Age, float Day, bool bSummary, bool bWood)
{
	ApplyPot();
	if (IsHerb())
	{
		ApplyHerb(Age, Day);
		return;
	}
	Herb->SetVisibility(false);
	EnsureMeshes();
	const FPlantSpecies& S = GetSpecies();
	const FPlantSeason Season = PlantModel::Season(S, (uint32)Seed, Day);
	const FPlantSummary Sum = PlantModel::Summary(S, (uint32)Seed, Age, Day);
	// far away the tree is its bake (the forests' kind): real branches and, further off, cards of its
	// own leaves; until the bake is ready it stays in full detail
	const FTreeBaked* Far = nullptr;
	if (bSummary)
	{
		if (UTreeBakeCache* Cache = GetWorld() ? GetWorld()->GetSubsystem<UTreeBakeCache>() : nullptr)
		{
			const float BakeAge = FMath::Max(1.f, FMath::RoundToFloat(Age));
			Far = Cache->Request(Species, (uint32)Seed, BakeAge, Day, 5.f, FVector(15.0, 55.0, 150.0));
			if (!Far && !GetWorld()->IsGameWorld())
			{
				Cache->BakeAllNow();
				Far = Cache->Request(Species, (uint32)Seed, BakeAge, Day, 5.f, FVector(15.0, 55.0, 150.0));
			}
		}
		if (!Far || !Far->Mesh)
		{
			Far = nullptr;
			bSummary = false;
		}
	}

	WoodMaterial->SetScalarParameterValue(TEXT("Snow"), ULyudWinterSubsystem::SnowOnBranches(GetWorld(), Season.Snow));
	// a dead tree's wood greys as it stands
	WoodMaterial->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(FLinearColor(0.82f, 0.82f, 0.86f, 1.f), FLinearColor::White, Sum.Vigour > 0.f ? 1.f : 0.f));
	// past its snag years it is gone
	const bool bGone = PlantModel::IsGone(S, (uint32)Seed, Age);
	SetActorHiddenInGame(bGone);
	TrunkCollision->SetCollisionEnabled(bGone ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
	{
		LeafMaterials[K]->SetVectorParameterValue(TEXT("Color"), Season.Colour[K]);
		LeafComponents[K]->SetVisibility(!bSummary);
	}
	FruitMaterial->SetVectorParameterValue(TEXT("Color"), Season.FruitColour);
	FruitMaterial->SetScalarParameterValue(TEXT("Ripeness"), Season.FruitRipeness);
	const FLinearColor Brown(0.1f, 0.06f, 0.025f);
	for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
	{
		LitterMaterials[K]->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(Season.Colour[K], Brown, 0.35f));
		LitterComponents[K]->SetVisibility(!bSummary);
	}
	Wood->SetVisibility(!bSummary);
	FruitComponent->SetVisibility(!bSummary);
	BloomComponent->SetVisibility(!bSummary && BloomMesh != nullptr);
	BloomMaterial->SetVectorParameterValue(TEXT("Color"), Season.BloomTint);
	// in bloom the thin twigs take on the colour of the flowers (as a baked tree's do): a haze of catkins that the few drawn ones do not give
	{
		const float Mix = S.Blossom.Form != EPlantBlossomForm::None ? FMath::Min(1.f, S.Blossom.FarTint * Season.Bloom * 0.8f) : 0.f;
		WoodMaterial->SetVectorParameterValue(TEXT("TwigColour"), FMath::Lerp(S.Bark.TwigColour, Season.BloomLook, Mix));
	}
	SummaryTrunk->SetVisibility(false);
	SummaryCrown->SetVisibility(false);
	FarTree->SetVisibility(bSummary);

	if (bSummary)
	{
		// level 0: no branches at all, just the summary's height, width and colour
		for (UInstancedStaticMeshComponent* L : LeafComponents) SetInstances(L, {});
		for (UInstancedStaticMeshComponent* L : LitterComponents) SetInstances(L, {});
		SetInstances(FruitComponent, {});
		SetInstances(BloomComponent, {});
		LastBloom = 0;
		if (Far->Version != FarVersion || FarTree->GetStaticMesh() != Far->Mesh)
		{
			FarTree->SetStaticMesh(Far->Mesh);
			FarTree->SetMaterial(0, Far->Wood);
			FarTree->SetMaterial(1, Far->Leaf);
			FarTree->SetMaterial(2, Far->Card);
			FarTree->SetMaterial(3, Far->Bloom);
			FarVersion = Far->Version;
		}
	}
	else
	{
		// in play, leaves that let go since the last re-shape fly down as falling leaves
		const bool bGame = GetWorld() && GetWorld()->IsGameWorld();
		const bool bSincePrev = bGame && Day > LastSeasonDay && Day - LastSeasonDay < 10.f && FMath::Abs(Age - ShownAge) < 1.f;
		FPlantSkeleton::FInstances Out;
		Skeleton.Evaluate(Age, Season, Out, bSincePrev ? &LastSeason : nullptr);
		LastSeason = Season;
		LastSeasonDay = Day;
		int32 NumLeaves = 0;
		for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
		{
			SetInstances(LeafComponents[K], Out.Leaves[K]);
			// how far each cluster swings with its twig (M_PlantLeaf's per-instance data)
			for (int32 L = 0; L < Out.LeafSway[K].Num(); L++)
			{
				LeafComponents[K]->SetCustomDataValue(L, 0, Out.LeafSway[K][L], false);
			}
			LeafComponents[K]->MarkRenderStateDirty();
			NumLeaves += Out.Leaves[K].Num();
		}
		SetInstances(BloomComponent, Out.Blossoms);
		for (int32 B = 0; B < Out.BlossomSway.Num(); B++)
		{
			BloomComponent->SetCustomDataValue(B, 0, Out.BlossomSway[B], false);
		}
		BloomComponent->MarkRenderStateDirty();
		LastBloom = Out.Blossoms.Num();
		UpdateLitter(Out, Season);
		if (bGame)
		{
			SpawnFallingLeaves(Out);
			SyncFallenFruit(Out);
		}
		else
		{
			HangingKeys = Out.FruitIds;
			HangingXf = Out.Fruit;
			HangingData = Out.FruitData;
			DrawHangingFruit();
		}
		LastLeaves = NumLeaves;
		LastFruit = HangingKeys.Num();
		LastHeight = Out.Height;
		LastReach = Out.Reach;
		if (bWood || WoodAge < 0.f)
		{
			UTexture2D* Marks = nullptr;
			WoodMesh = PlantMeshes::BuildWood(this, WoodMesh, Out.Tubes, S.Bark, Age, (uint32)Seed, &Marks);
			if (Marks)
			{
				ScarMask = Marks;
				WoodMaterial->SetTextureParameterValue(TEXT("ScarMask"), ScarMask);
			}
			if (Wood->GetStaticMesh() != WoodMesh)
			{
				Wood->SetStaticMesh(WoodMesh);
			}
			Wood->MarkRenderStateDirty();
			WoodAge = Age;
		}
	}
	// a hedge stops you at its leaves, a tree at its trunk, a potted tree at its pot
	float R = FMath::Max(Sum.TrunkRadius * 100.f, 2.f);
	if (S.CrownBox.X > 0.f) R = FMath::Max(R, 45.f * FMath::Min(S.CrownBox.X, S.CrownBox.Y) * FMath::Clamp(Sum.Height / FMath::Max(S.CrownBox.Z, 0.1f), 0.2f, 1.f));
	if (PotKind() != EPlantPot::None) R = FMath::Max(R, 0.5f * PotSize());
	const float HalfH = FMath::Clamp(Sum.Height * 50.f, R, 150.f);
	TrunkCollision->SetCapsuleSize(R, HalfH);
	TrunkCollision->SetRelativeLocation(FVector(0, 0, HalfH));

	ShownAge = Age;
	ShownDay = Day;
	bShownSummary = bSummary;
}

void AProceduralPlant::DrawHangingFruit()
{
	SetInstances(FruitComponent, HangingXf);
	constexpr int32 N = FPlantSkeleton::FInstances::FruitDataFloats;
	for (int32 F = 0; F < HangingXf.Num(); F++)
	{
		FruitComponent->SetCustomData(F, TArrayView<const float>(&HangingData[F * N], N), false);
	}
	FruitComponent->MarkRenderStateDirty();
}

void AProceduralPlant::SyncFallenFruit(const FPlantSkeleton::FInstances& Out)
{
	constexpr int32 N = FPlantSkeleton::FInstances::FruitDataFloats;
	const FPlantSpecies& S = GetSpecies();
	const int32 Year = CurrentDate().GetYear() * 100000;
	const FTransform& Xf = GetActorTransform();
	TSet<int32> WasHanging(HangingKeys);
	TArray<int32> Keys;
	TArray<FTransform> Hung;
	TArray<float> Data;
	for (int32 F = 0; F < Out.FruitIds.Num(); F++)
	{
		Keys.Add(Year + Out.FruitIds[F]);
		Hung.Add(Out.Fruit[F]);
		Data.Append(&Out.FruitData[F * N], N);
	}
	TSet<int32> Want;
	TArray<FPendingFruit> StillPending;
	for (const FPlantSkeleton::FInstances::FFallen& F : Out.FallenFruit)
	{
		const int32 Key = Year + F.Id;
		if (PickedFruit.Contains(Key))
		{
			continue;
		}
		Want.Add(Key);
		const TWeakObjectPtr<AFallenFruit>* Have = GroundFruit.Find(Key);
		if (Have && Have->IsValid())
		{
			continue;
		}
		if (WasHanging.Contains(Key))
		{
			// its day has come while we watch: it stays on its twig until its turn to fall, then that very
			// fruit lets go (DropFruit)
			StillPending.Add({ Key, F });
			Keys.Add(Key);
			Hung.Add(F.Hung);
			Data.Append(F.Data, N);
			continue;
		}
		// fell before we came: it lies in the grass under its twig
		FVector At = Xf.TransformPosition(FVector(F.At) * 100.0);
		const FVector Scatter((PlantModel::Hash01(Seed, F.Id, 0, 31) - 0.5f) * 120.f, (PlantModel::Hash01(Seed, F.Id, 0, 32) - 0.5f) * 120.f, 0.f);
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(FruitGround), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, At + Scatter, At + Scatter - FVector(0, 0, 3000), ECC_Visibility, Q))
		{
			At = Hit.ImpactPoint + FVector(0, 0, S.FruitSize * 50.f);
		}
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AFallenFruit* Fruit = GetWorld()->SpawnActor<AFallenFruit>(At, F.Hung.Rotator(), P);
		if (!Fruit) continue;
		Fruit->Tree = this;
		Fruit->Key = Key;
		Fruit->Init(FruitMesh, FruitMaterial, F.Hung.GetScale3D().X * 100.f, F.Data, N);
		GroundFruit.Add(Key, Fruit);
	}
	// rotted (it sinks into the grass), or the calendar went back
	for (auto It = GroundFruit.CreateIterator(); It; ++It)
	{
		if (!Want.Contains(It.Key()))
		{
			if (AFallenFruit* F = It.Value().Get()) F->Rot();
			It.RemoveCurrent();
		}
	}
	// the ripe ones fall one by one over the time until the next re-shape
	const double Now = GetWorld()->GetTimeSeconds();
	const double Interval = LastFruitTime < 0.0 ? 1.0 : FMath::Clamp(Now - LastFruitTime, 0.2, 6.0 * 3600.0);
	LastFruitTime = Now;
	PendingFruit = MoveTemp(StillPending);
	FruitReleaseRate = PendingFruit.Num() / (float)Interval;
	HangingKeys = MoveTemp(Keys);
	HangingXf = MoveTemp(Hung);
	HangingData = MoveTemp(Data);
	DrawHangingFruit();
}

void AProceduralPlant::DropFruit(const FPendingFruit& P)
{
	constexpr int32 N = FPlantSkeleton::FInstances::FruitDataFloats;
	const int32 At = HangingKeys.IndexOfByKey(P.Key);
	if (At == INDEX_NONE)
	{
		return;
	}
	// take that fruit off the tree and let the same one fall: same place, size, turn and skin
	const FTransform World = HangingXf[At] * GetActorTransform();
	HangingKeys.RemoveAt(At);
	HangingXf.RemoveAt(At);
	HangingData.RemoveAt(At * N, N);
	DrawHangingFruit();
	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFallenFruit* Fruit = GetWorld()->SpawnActor<AFallenFruit>(World.GetLocation(), World.Rotator(), Sp);
	if (!Fruit)
	{
		return;
	}
	Fruit->Tree = this;
	Fruit->Key = P.Key;
	Fruit->Init(FruitMesh, FruitMaterial, World.GetScale3D().X * 100.f, P.F.Data, N);
	GroundFruit.Add(P.Key, Fruit);
	LastFruit = HangingKeys.Num();
}

float AProceduralPlant::GroundAt(float X, float Y)
{
	if (GroundGrid.IsEmpty())
	{
		// one trace per metre around the trunk, as far as the crown and its litter reach
		const FPlantSpecies& S = GetSpecies();
		const float R = 50.f * FMath::Max(S.CrownWidth * S.MaxHeight, 2.f * S.Branch1MaxLength) + 400.f;
		GroundN = FMath::Clamp(2 * FMath::CeilToInt(R / 100.f) + 1, 3, 61);
		GroundCell = 2.f * R / (GroundN - 1);
		GroundGrid.Init(0.f, GroundN * GroundN);
		UWorld* World = GetWorld();
		if (World)
		{
			const FTransform& Xf = GetActorTransform();
			FCollisionQueryParams Q(SCENE_QUERY_STAT(PlantLitterGround), false, this);
			for (int32 J = 0; J < GroundN; J++)
			{
				for (int32 I = 0; I < GroundN; I++)
				{
					const FVector Local(-R + I * GroundCell, -R + J * GroundCell, 0.f);
					const FVector At = Xf.TransformPosition(Local);
					FHitResult Hit;
					if (World->LineTraceSingleByObjectType(Hit, At + FVector(0, 0, 1500), At - FVector(0, 0, 3000), FCollisionObjectQueryParams(ECC_WorldStatic), Q))
					{
						GroundGrid[J * GroundN + I] = (float)Xf.InverseTransformPosition(Hit.ImpactPoint).Z;
					}
				}
			}
		}
	}
	const float R = 0.5f * GroundCell * (GroundN - 1);
	const float Fx = FMath::Clamp((X + R) / GroundCell, 0.f, GroundN - 1.001f), Fy = FMath::Clamp((Y + R) / GroundCell, 0.f, GroundN - 1.001f);
	const int32 I = FMath::FloorToInt(Fx), J = FMath::FloorToInt(Fy);
	const float A = Fx - I, B = Fy - J;
	auto G = [&](int32 Ii, int32 Jj) { return GroundGrid[Jj * GroundN + Ii]; };
	return FMath::Lerp(FMath::Lerp(G(I, J), G(I + 1, J), A), FMath::Lerp(G(I, J + 1), G(I + 1, J + 1), A), B);
}

void AProceduralPlant::UpdateLitter(const FPlantSkeleton::FInstances& Out, const FPlantSeason& Season)
{
	// no more than a few thousand leaves on the ground per tree: thin evenly beyond that
	constexpr int32 MaxLitter = 5000;
	const int32 Stride = FMath::Max(1, FMath::DivideAndRoundUp(Out.Litter.Num(), MaxLitter));
	TArray<FTransform> Lying[FPlantSeason::NumBuckets];
	for (int32 I = 0; I < Out.Litter.Num(); I += Stride)
	{
		const FPlantSkeleton::FInstances::FLitter& L = Out.Litter[I];
		const float X = L.At.X * 100.f, Y = L.At.Y * 100.f;
		const FRotator Rot(FMath::Sin(L.Yaw) * 12.f, L.Yaw, FMath::Cos(L.Yaw * 1.7f) * 12.f);
		Lying[L.Bucket].Emplace(Rot, FVector(X, Y, GroundAt(X, Y) + 1.5f), FVector(L.Size));
	}
	LastLitter = 0;
	for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
	{
		SetInstances(LitterComponents[K], Lying[K]);
		LastLitter += Lying[K].Num();
		bLandedChanged |= !Landed[K].IsEmpty();
		Landed[K].Reset();
	}
	bLitterDirty = false;
}

void AProceduralPlant::SpawnFallingLeaves(const FPlantSkeleton::FInstances& Out)
{
	// the leaves that let go between two re-shapes take to the air evenly over the time until the
	// next one: a single leaf now and then at the real calendar's pace, a flurry when it is sped up
	const double Now = GetWorld()->GetTimeSeconds();
	const double Interval = LastDropTime < 0.0 ? 1.0 : FMath::Clamp(Now - LastDropTime, 0.2, 6.0 * 3600.0);
	LastDropTime = Now;
	PendingDrops.Append(Out.Dropped);
	ReleaseRate = PendingDrops.Num() / (float)Interval;
}

void AProceduralPlant::TickFallingLeaves(float DeltaSeconds)
{
	constexpr int32 MaxFalling = 400;
	ReleaseAcc += ReleaseRate * DeltaSeconds;
	while (ReleaseAcc >= 1.f && !PendingDrops.IsEmpty())
	{
		ReleaseAcc -= 1.f;
		const FPlantSkeleton::FInstances::FDrop D = PendingDrops.Pop(EAllowShrinking::No);
		if (Falling.Num() >= MaxFalling)
		{
			continue; // too many in the air: this one is already in the litter
		}
		FFallingLeaf& F = Falling.AddDefaulted_GetRef();
		F.P = D.From.GetLocation();
		F.Rot = D.From.Rotator();
		F.Spin = FRotator(FMath::FRandRange(-240.f, 240.f), FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(-300.f, 300.f));
		F.Target = FVector2D(D.To.At.X * 100.f, D.To.At.Y * 100.f);
		F.GroundZ = GroundAt(F.Target.X, F.Target.Y) + 1.5f;
		F.Speed = FMath::FRandRange(60.f, 120.f);
		F.Phase = FMath::FRandRange(0.f, 2.f * PI);
		F.Size = D.To.Size;
		F.Bucket = D.To.Bucket;
	}
	if (PendingDrops.IsEmpty())
	{
		ReleaseAcc = 0.f;
	}
	if (Falling.IsEmpty() && !bLandedChanged)
	{
		return;
	}
	bLandedChanged = false;
	TArray<FTransform> Air[FPlantSeason::NumBuckets];
	for (int32 I = Falling.Num() - 1; I >= 0; I--)
	{
		FFallingLeaf& F = Falling[I];
		// a leaf rocks and sideslips down, drifting toward where it will lie
		const float Left = FMath::Max(F.P.Z - F.GroundZ, 0.f);
		const FVector2D ToTarget = (F.Target - FVector2D(F.P.X, F.P.Y)) / FMath::Max(Left / F.Speed, 0.3f);
		const FVector2D Flutter(FMath::Cos(F.Phase * 1.7f), FMath::Sin(F.Phase * 1.3f));
		F.P.X += (ToTarget.X + Flutter.X * 45.f) * DeltaSeconds;
		F.P.Y += (ToTarget.Y + Flutter.Y * 45.f) * DeltaSeconds;
		F.P.Z -= F.Speed * (0.7f + 0.4f * FMath::Sin(F.Phase * 2.1f)) * DeltaSeconds;
		F.Phase += DeltaSeconds * 2.f * PI * 0.8f;
		F.Rot += F.Spin * DeltaSeconds;
		if (F.P.Z <= F.GroundZ)
		{
			// it lies down where the litter will have it
			Landed[F.Bucket].Emplace(FRotator(FMath::Sin(F.Rot.Yaw) * 12.f, F.Rot.Yaw, 0.f), FVector(F.P.X, F.P.Y, F.GroundZ), FVector(F.Size));
			Falling.RemoveAtSwap(I);
			continue;
		}
		Air[F.Bucket].Emplace(F.Rot, F.P, FVector(F.Size));
	}
	for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
	{
		Air[K].Append(Landed[K]);
		SetInstances(FallingComponents[K], Air[K]);
	}
}

void AProceduralPlant::SetDevTime(float Age, float Day)
{
	DevAge = Age;
	DevDay = Day;
	Refresh();
}

void AProceduralPlant::Refresh()
{
	Apply(GetAge(), CurrentDay(), WantsSummary(), true);
}

FString AProceduralPlant::DescribeState() const
{
	const FDateTime Date = FDateTime(CurrentDate().GetYear(), 1, 1) + FTimespan::FromDays(CurrentDay() - 1.f);
	const FPlantSpecies& S = GetSpecies();
	const float Life = PlantModel::Lifespan(S, (uint32)Seed);
	const float Age = GetAge();
	const TCHAR* Stage = PlantModel::IsGone(S, (uint32)Seed, Age) ? TEXT("ui.plants.stage.gone") : Age >= Life ? TEXT("ui.plants.stage.dead")
		: PlantModel::Vigour(S, (uint32)Seed, Age) < 1.f ? TEXT("ui.plants.stage.declining") : TEXT("ui.plants.stage.alive");
	auto One = [](float V) { return FString::Printf(TEXT("%.1f"), V); };
	return LyudText::Get(TEXT("ui.plants.state"), {
		PlantSpeciesLibrary::DisplayName(S).ToString(), Seed, One(Age), FMath::RoundToInt(Life), LyudText::Get(Stage).ToString(),
		Date.ToString(TEXT("%d.%m %H:%M")), One(LastHeight), One(LastReach * 2.f), LastLeaves, LastLitter, Falling.Num(),
		LastFruit, GroundFruit.Num(), FruitPicked }).ToString();
}

void AProceduralPlant::ClearFallenFruit()
{
	for (const TPair<int32, TWeakObjectPtr<AFallenFruit>>& F : GroundFruit)
	{
		if (AFallenFruit* A = F.Value.Get()) A->Destroy();
	}
	GroundFruit.Reset();
	HangingKeys.Reset();
	HangingXf.Reset();
	HangingData.Reset();
	PendingFruit.Reset();
	FruitReleaseRate = FruitReleaseAcc = 0.f;
}

AFallenFruit* AProceduralPlant::AimedFruit() const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || !PC->PlayerCameraManager || GroundFruit.IsEmpty())
	{
		return nullptr;
	}
	const FVector From = PC->PlayerCameraManager->GetCameraLocation();
	const FVector To = From + PC->PlayerCameraManager->GetCameraRotation().Vector() * (PickDistance + 100.f);
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(FruitPick), false, Pawn);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Q))
	{
		return nullptr;
	}
	AFallenFruit* Fruit = Cast<AFallenFruit>(Hit.GetActor());
	if (!Fruit || Fruit->Tree.Get() != this || FVector::Dist(Pawn->GetActorLocation(), Fruit->GetActorLocation()) > PickDistance)
	{
		return nullptr;
	}
	return Fruit;
}

void AProceduralPlant::OnInteract()
{
	const FName Item = GetSpecies().FruitItem;
	AFallenFruit* Fruit = Item.IsNone() ? nullptr : AimedFruit();
	UHeroReachComponent* Reach = UHeroReachComponent::GetForPlayer(this);
	if (!Fruit || !Reach || Reach->IsBusy())
	{
		return;
	}
	// it is the hero's now: the tree forgets it (and won't lay it out again after a load)
	PickedFruit.AddUnique(Fruit->Key);
	GroundFruit.Remove(Fruit->Key);
	Fruit->Body->SetSimulatePhysics(false);
	Fruit->Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FReachItem Grab;
	Grab.Actor = Fruit;
	Grab.Center = Fruit->GetActorLocation();
	Grab.Radius = Fruit->Body->GetScaledSphereRadius();
	Grab.Front = Fruit->GetActorForwardVector();
	// the hand reaches down, the fingers close on it, it goes into the pocket (the reach destroys it there)
	TWeakObjectPtr<AProceduralPlant> Self(this);
	if (!Reach->Take(Grab, [Self, Item]()
		{
			if (!Self.IsValid()) return;
			Self->FruitPicked++;
			if (ULyudInventory* Inv = ULyudInventory::Get(Self.Get()))
			{
				Inv->Add(Item);
			}
		}))
	{
		// the hand could not take it: it stays on the ground
		PickedFruit.Remove(Fruit->Key);
		GroundFruit.Add(Fruit->Key, Fruit);
		Fruit->Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Fruit->Body->SetSimulatePhysics(true);
	}
}

void AProceduralPlant::OnGameLoaded_Implementation()
{
	// the picked list came back with the save: lay the ground out again from it
	ClearFallenFruit();
	Falling.Reset();
	PendingDrops.Reset();
	LastSeasonDay = -1000.f;
	Apply(GetAge(), CurrentDay(), WantsSummary());
}

void AProceduralPlant::ShowAt(float Age, float DayOfYear, bool bSummary)
{
	Apply(Age, DayOfYear, bSummary);
}

void AProceduralPlant::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Apply(GetAge(), CurrentDay(), bForceSummary);
}

void AProceduralPlant::BeginPlay()
{
	Super::BeginPlay();
	SinceUpdate = SinceWood = 0.f;
	if (IsHerb())
	{
		// a herb changes by the day: its state is checked twice a second, the mesh rebuilt only when it changes
		UpdateInterval = FMath::Max(UpdateInterval, 0.5f);
	}
	if (bSnapToGround)
	{
		FHitResult Hit;
		const FVector At = GetActorLocation();
		FCollisionQueryParams Q(SCENE_QUERY_STAT(PlantGround), false, this);
		// a pot or a herb looks just under itself (a sill, a table, a bed), not on the floor above
		const bool bSmall = IsHerb() || PotKind() != EPlantPot::None;
		if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, bSmall ? 30 : 500), At - FVector(0, 0, bSmall ? 300 : 3000), ECC_Visibility, Q))
		{
			// the trunk goes into the soil a little; a pot stands on it
			SetActorLocation(Hit.ImpactPoint - FVector(0, 0, PotKind() != EPlantPot::None ? 0 : 3));
		}
	}
	if (GetSpecies().FruitSize > 0.f)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			EnableInput(PC);
			if (InputComponent)
			{
				// several actors listen for E (doors, seats, people); none may swallow it
				InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AProceduralPlant::OnInteract).bConsumeInput = false;
			}
		}
	}
	Apply(GetAge(), CurrentDay(), WantsSummary());
}

void AProceduralPlant::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearFallenFruit();
	Super::EndPlay(Reason);
}

void AProceduralPlant::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickFallingLeaves(DeltaSeconds);
	// "[E] pick up the apple" while one lies in reach and in view
	if (!GroundFruit.IsEmpty() || bPickPrompt)
	{
		const bool bAimed = !GetSpecies().FruitItem.IsNone() && AimedFruit() != nullptr;
		if (bAimed != bPickPrompt)
		{
			if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
			{
				const FLyudItemDef* Def = ULyudInventory::FindDef(GetSpecies().FruitItem);
				UI->SetPrompt(this, bAimed && Def ? LyudText::Get(TEXT("ui.pick"), { FStringFormatArg(ULyudInventory::ItemName(Def->Id).ToString()) }) : FText::GetEmpty());
			}
			bPickPrompt = bAimed;
		}
	}
	FruitReleaseAcc += FruitReleaseRate * DeltaSeconds;
	while (FruitReleaseAcc >= 1.f && !PendingFruit.IsEmpty())
	{
		FruitReleaseAcc -= 1.f;
		const FPendingFruit P = PendingFruit[0];
		PendingFruit.RemoveAt(0);
		DropFruit(P);
	}
	SinceUpdate += DeltaSeconds;
	SinceWood += DeltaSeconds;
	if (SinceUpdate < UpdateInterval)
	{
		return;
	}
	SinceUpdate = 0.f;
	const float Age = GetAge(), Day = CurrentDay();
	bool bSummary = WantsSummary();
	bool bNewBake = false;
	if (bSummary)
	{
		// far, but its bake is not ready yet: stay as it is rather than re-shape in full every time; and when a newer bake
		// (the season has moved on: blossom, leaves) is ready, take it over at once instead of at the next re-shape
		UTreeBakeCache* Cache = GetWorld()->GetSubsystem<UTreeBakeCache>();
		const FTreeBaked* Far = Cache ? Cache->Request(Species, (uint32)Seed, FMath::Max(1.f, FMath::RoundToFloat(Age)), Day, 5.f, FVector(15.0, 55.0, 150.0)) : nullptr;
		bNewBake = bShownSummary && Far && Far->Mesh && Far->Version != FarVersion;
		if (!bShownSummary) bSummary = Far && Far->Mesh;
	}
	// a tree grows a few mm a day: re-shape only when the change is visible; the wood less often
	const bool bChanged = bSummary != bShownSummary || FMath::Abs(Age - ShownAge) > 0.01f || FMath::Abs(Day - ShownDay) > 0.25f || bLitterDirty || bNewBake;
	const bool bWoodDue = !bSummary && SinceWood >= WoodUpdateInterval && FMath::Abs(Age - WoodAge) > 0.001f;
	if (bChanged || bWoodDue)
	{
		Apply(Age, Day, bSummary, bWoodDue);
		if (bWoodDue)
		{
			SinceWood = 0.f;
		}
	}
}

FString AProceduralPlant::DebugReport(float Age, float DayOfYear)
{
	if (IsHerb())
	{
		HerbKey = 0;
		Apply(Age, DayOfYear, false);
		const PlantHerbs::FState St = PlantHerbs::State(GetSpecies(), (uint32)Seed, Age, DayOfYear);
		return FString::Printf(TEXT("%s seed %d age %.1f day %.0f: herb %d tris (%d leaf), h %.2f r %.2f, maturity %.2f bloom %.2f present %.2f alive %d | mesh %.2f ms"),
			*GetSpecies().Name.ToString(), Seed, Age, DayOfYear, NumTriangles(HerbMesh), LastLeaves, LastHeight, LastReach, St.Maturity, St.Bloom, St.Present,
			St.bAlive ? 1 : 0, HerbBuildMs);
	}
	EnsureMeshes();
	const FPlantSpecies& S = GetSpecies();
	const FPlantSeason Season = PlantModel::Season(S, (uint32)Seed, DayOfYear);
	const FPlantSummary Sum = PlantModel::Summary(S, (uint32)Seed, Age, DayOfYear);
	FPlantSkeleton::FInstances Out;
	const double T0 = FPlatformTime::Seconds();
	Skeleton.Evaluate(Age, Season, Out);
	const double T1 = FPlatformTime::Seconds();
	WoodMesh = PlantMeshes::BuildWood(this, WoodMesh, Out.Tubes, S.Bark, Age, (uint32)Seed);
	Wood->SetStaticMesh(WoodMesh);
	WoodAge = Age;
	const double T2 = FPlatformTime::Seconds();
	const int32 Leaves = Out.Leaves[0].Num() + Out.Leaves[1].Num() + Out.Leaves[2].Num();
	const FStaticMeshRenderData* RD = WoodMesh ? WoodMesh->GetRenderData() : nullptr;
	const int32 WoodUVs = RD && RD->LODResources.Num() ? RD->LODResources[0].GetNumTexCoords() : 0;
	return FString::Printf(TEXT("%s seed %d age %.1f day %.0f: wood UVs %d, mask %s: %d/%d branches, %d rings, wood %d tris, %d leaf clusters x %d tris, %d fruit, %d bloom (%.2f out, %d tris each), drawn h %.2f reach %.2f | summary h %.2f crown r %.2f leaves %.2f | evaluate %.2f ms, wood mesh %.1f ms"),
		*S.Name.ToString(), Seed, Age, DayOfYear, WoodUVs, ScarMask ? TEXT("yes") : TEXT("no"), Out.LiveBranches, Skeleton.GetBranches().Num(), Out.NumRings(), NumTriangles(WoodMesh),
		Leaves, NumTriangles(LeafMesh), Out.Fruit.Num(), Out.Blossoms.Num(), Season.Bloom, NumTriangles(BloomMesh), Out.Height, Out.Reach, Sum.Height, Sum.CrownRadius, Sum.LeafAmount,
		(T1 - T0) * 1000.0, (T2 - T1) * 1000.0);
}
