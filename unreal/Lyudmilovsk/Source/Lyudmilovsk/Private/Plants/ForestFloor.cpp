#include "Plants/ForestFloor.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ForestFloorMeshes.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeProxy.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "Plants/FallenFruit.h"
#include "Plants/GameCalendar.h"
#include "Plants/GrassSpecies.h"
#include "Plants/PlantHerb.h"
#include "Plants/PlantMeshes.h"
#include "Plants/PlantModel.h"
#include "Plants/PlantSpecies.h"
#include "Plants/ProceduralForest.h"
#include "Plants/ProceduralGrass.h"
#include "Plants/ProceduralPlant.h"
#include "Plants/TreeBakeCache.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "Terrain/LyudTerrain.h"
#include "TextureResource.h"
#include "UObject/ConstructorHelpers.h"
#include "Weather/LyudRainFog.h"
#include "Weather/LyudWeather.h"
#include "Weather/LyudWind.h"
#include "Winter/LyudWinter.h"

static TAutoConsoleVariable<int32> CVarFloor(TEXT("lyud.ForestFloor"), 1, TEXT("0 hides the forest floor's ground cover, litter and debris (AForestFloor)."));
static TAutoConsoleVariable<float> CVarFloorDensity(TEXT("lyud.ForestFloor.Density"), 1.f,
	TEXT("Multiplies the forest floor's density (applied at the next layout). 0.5 for a slow machine."));
static TAutoConsoleVariable<int32> CVarFloorShadows(TEXT("lyud.ForestFloor.Shadows"), 1, TEXT("0: no forest floor piece casts a shadow (logs, stumps, boulders, branches, roots do by default)."));
static TAutoConsoleVariable<int32> CVarFloorDebug(TEXT("lyud.ForestFloor.Debug"), 0, TEXT("1: print the forest floor's state on screen."));
static TAutoConsoleVariable<float> CVarFloorWet(TEXT("lyud.ForestFloor.Wet"), -1.f, TEXT("Force the forest floor's wetness 0..1 (negative: the weather's)."));
static TAutoConsoleVariable<float> CVarFloorRime(TEXT("lyud.ForestFloor.Rime"), -1.f, TEXT("Force rime on the litter and the moss 0..1 (negative: the weather's)."));
static TAutoConsoleVariable<int32> CVarFloorGuard(TEXT("lyud.ForestFloor.Guard"), 1,
	TEXT("1: the floor lies only on wild forest ground, 15 m from anything built, among enough trees. Tests: 0 anywhere the terrain mask allows, as before; -1 as before on a ground built without the mask's forest channel (any landscape among any trees)."));
static TAutoConsoleVariable<float> CVarFloorDry(TEXT("lyud.ForestFloor.Dry"), -1.f, TEXT("Force drought on the litter 0..1 (negative: the weather's)."));

namespace
{
	using PlantModel::Hash01;
	using PlantModel::SmoothStep;
	namespace FFM = ForestFloorMeshes;

	constexpr float TreeCell = 1000.f;
	constexpr int32 LitterN = 128;
	constexpr float LitterCell = 150.f;
	/** The floor keeps this far (cm) from anything built: paving, yards, buildings, stations, roofs. */
	constexpr float KeepOffBuilt = 1500.f;
	/** Wild trees it needs about: any piece within SmallTreeR, a log, stump, boulder, branch or root within BigTreeR. */
	constexpr float SmallTreeR = 1200.f, BigTreeR = 1500.f;
	constexpr int32 SmallTrees = 3, BigTrees = 6;
	/** Built things larger than this (the coast's slabs, the rail bank) are left to the ground trace. */
	constexpr float MaxBuiltBox = 30000.f;
	/** Walk grid kinds (tools/animal_map.py, LyudAnimals::EGround). */
	enum : uint8 { WalkBlocked = 0, WalkGrass = 1, WalkPaved = 2, WalkYard = 3, WalkSand = 4, WalkForest = 5 };
	/** A site cell beyond the walk grid (the upland and the hills round the valley): wild land, its trees decide. */
	constexpr uint8 SiteBeyond = 7;
	/** Trees of these species stand only in towns and gardens: their forests are not wild woods. */
	bool IsTownSpecies(FName S)
	{
		const FString N = S.ToString();
		return N.Contains(TEXT("Maple")) || N.Contains(TEXT("Apple")) || N.Contains(TEXT("Poplar")) || N.Contains(TEXT("Lime")) || N.Contains(TEXT("Chestnut"));
	}

	/** Bands of the layout: what is only worth drawing close by is laid out only close by. */
	constexpr float BandReach[3] = { 1800.f, 4200.f, 7000.f };

	/** What the floor needs to know of a tree species. */
	struct FTreeClass
	{
		FName Species;
		bool bConifer = false, bSpruce = false, bPine = false, bBirch = false;
		/** Litter hue (FForestLitter::Hue) and the species' leaf fall. */
		float Hue = 0.3f, DropStart = 288.f, Bare = 318.f;
	};
	TArray<FTreeClass> GClasses;

	int32 ClassOf(FName Species)
	{
		for (int32 I = 0; I < GClasses.Num(); I++) if (GClasses[I].Species == Species) return I;
		const FPlantSpecies* S = PlantSpeciesLibrary::Find(Species);
		FTreeClass C;
		C.Species = Species;
		const FString N = Species.ToString();
		C.bSpruce = N.Contains(TEXT("Spruce"));
		C.bPine = N.Contains(TEXT("Pine"));
		C.bBirch = N.Contains(TEXT("Birch"));
		C.bConifer = S && S->bEvergreen;
		if (S)
		{
			C.DropStart = S->LeafDropStartDay;
			C.Bare = S->BareDay;
		}
		C.Hue = C.bBirch ? 0.05f : N.Contains(TEXT("Aspen")) ? 0.15f : N.Contains(TEXT("Poplar")) ? 0.12f : N.Contains(TEXT("Maple")) ? 0.45f
			: N.Contains(TEXT("Rowan")) ? 0.6f : N.Contains(TEXT("Apple")) ? 0.55f : N.Contains(TEXT("Buckthorn")) ? 0.75f
			: N.Contains(TEXT("Oak")) ? 0.9f : N.Contains(TEXT("Alder")) ? 1.f : 0.3f;
		return GClasses.Add(C);
	}

	float Patch(uint32 Seed, float X, float Y, float ScaleCm)
	{
		// smooth 2D value noise 0..1 on a grid of ScaleCm
		const float U = X / ScaleCm, V = Y / ScaleCm;
		const int32 I = FMath::FloorToInt(U), J = FMath::FloorToInt(V);
		const float Fu = U - I, Fv = V - J;
		auto S = [](float T) { return T * T * (3.f - 2.f * T); };
		auto H = [&](int32 A, int32 B) { return Hash01(Seed, (uint32)A, (uint32)B, 77); };
		return FMath::Lerp(FMath::Lerp(H(I, J), H(I + 1, J), S(Fu)), FMath::Lerp(H(I, J + 1), H(I + 1, J + 1), S(Fu)), S(Fv));
	}

	/** This year's and last year's leaves still on the ground on Day: (amount 0..1, decay 0..1). */
	FVector2f LeafLitter(const FTreeClass& C, float Day)
	{
		const float Tau = 140.f;
		if (Day >= C.DropStart)
		{
			const float Fresh = SmoothStep(C.DropStart, C.Bare, Day);
			const float Old = FMath::Exp(-(Day - C.Bare + 365.f) / Tau) * 0.8f;
			const float Age = FMath::Max(Day - C.Bare, 0.f);
			return FVector2f(FMath::Min(Fresh + Old, 1.f), FMath::Clamp(0.1f * (1.f - Fresh) + Age / 220.f, 0.f, 0.45f));
		}
		const float Since = Day + 365.f - C.Bare;
		return FVector2f(FMath::Exp(-Since / Tau), FMath::Clamp(0.35f + Since / 300.f, 0.f, 1.f));
	}

	enum class EKindType : uint8 { Herb, Grass, Seedling, Debris, Leaf, Falling };
	enum class EWood : uint8 { Pine, Spruce, Birch };
}

/** One kind of thing on the floor: a mesh, its materials, one set of instances. */
struct AForestFloor::FKind
{
	FName Name;
	EKindType Type = EKindType::Debris;
	FName Species;
	uint32 VariantSeed = 1;
	float Age = 20.f;
	int32 Band = 1;
	float Cull = 3000.f;
	bool bShadow = false;
	bool bCollision = false;
	/** Density at most, per m2 (DensityAt says how much of it here). */
	float Density = 0.f;
	int32 Variants = 1;
	int32 Variant = 0;
	int32 Comp = INDEX_NONE;
	UStaticMesh* Mesh = nullptr;
	TArray<UMaterialInstanceDynamic*> MIDs;
	uint32 HerbKey = 0xFFFFFFFF;
	int32 BakeVersion = -1;
	float Top = 10.f;
	FString Lint;
	int32 LintProblems = 0;
	int32 Triangles = 0;
	int32 Instances = 0;
	/** Debris: its geometry (LOD 0, LOD 1 or none). */
	FFM::FGeom Geom, GeomLod1;
	EWood Wood = EWood::Pine;
	/** Pattern in M_FloorDebris: 0 bark, 1 stone, 2 cone, 3 moss. */
	float Pattern = 0.f;
	FLinearColor TintA = FLinearColor::White, TintB = FLinearColor::White;
	/** Lying things follow the ground's slope; standing plants stand up. */
	bool bAlign = false;
	float ScaleMin = 0.8f, ScaleMax = 1.2f;
	float Sink = 0.f;
	/** Logs, stumps, boulders, branches, roots: they need a real wood about them. */
	bool bBig = false;
};

namespace
{
	/** A herb's mesh with a cheap second LOD (fewer, larger leaves) for the floor's distance. */
	void FillHerbMD(FMeshDescription& MD, const PlantHerbs::FHerbGeometry& G)
	{
		static const FName SlotNames[PlantHerbs::NumSlots] = { TEXT("Leaves"), TEXT("Stems"), TEXT("Flowers") };
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> VTan = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> VSign = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> VUV = Attr.GetVertexInstanceUVs();
		VUV.SetNumChannels(1);
		FPolygonGroupID Groups[PlantHerbs::NumSlots];
		for (int32 K = 0; K < PlantHerbs::NumSlots; K++)
		{
			Groups[K] = MD.CreatePolygonGroup();
			Attr.GetPolygonGroupMaterialSlotNames()[Groups[K]] = SlotNames[K];
		}
		TArray<FVertexInstanceID> Inst;
		for (int32 I = 0; I < G.Pos.Num() + 3; I++)
		{
			const bool bPad = I >= G.Pos.Num();
			const FVertexID V = MD.CreateVertex();
			VPos[V] = bPad || G.Pos[I].ContainsNaN() ? FVector3f::ZeroVector : G.Pos[I];
			const FVertexInstanceID VI = MD.CreateVertexInstance(V);
			const FVector3f N = bPad ? FVector3f::UpVector : G.Nrm[I].GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector);
			FVector3f T = bPad ? FVector3f::XAxisVector : (G.Tan[I] - N * FVector3f::DotProduct(G.Tan[I], N)).GetSafeNormal();
			if (T.IsNearlyZero()) T = FVector3f::CrossProduct(FMath::Abs(N.Z) < 0.9f ? FVector3f::UpVector : FVector3f::XAxisVector, N).GetSafeNormal();
			VNrm[VI] = N; VTan[VI] = T; VSign[VI] = 1.f;
			VUV.Set(VI, 0, bPad ? FVector2f::ZeroVector : G.UV[I]);
			Inst.Add(VI);
		}
		const int32 Pad = G.Pos.Num();
		for (int32 K = 0; K < PlantHerbs::NumSlots; K++)
		{
			for (const FIntVector3& T : G.Tris[K]) MD.CreateTriangle(Groups[K], { Inst[T.X], Inst[T.Y], Inst[T.Z] });
			if (G.Tris[K].IsEmpty()) MD.CreateTriangle(Groups[K], { Inst[Pad], Inst[Pad + 1], Inst[Pad + 2] });
		}
	}

	UStaticMesh* BuildHerbLods(UObject* Outer, FName Name, const PlantHerbs::FHerbGeometry& L0, const PlantHerbs::FHerbGeometry& L1, float Screen1)
	{
		FMeshDescription A, B;
		FillHerbMD(A, L0);
		FillHerbMD(B, L1);
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, MakeUniqueObjectName(Outer, UStaticMesh::StaticClass(), Name), RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
		for (const TCHAR* Slot : { TEXT("Leaves"), TEXT("Stems"), TEXT("Flowers") }) Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, Slot));
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Mesh->BuildFromMeshDescriptions({ &A, &B }, Params);
		if (FStaticMeshRenderData* RD = Mesh->GetRenderData())
		{
			RD->ScreenSize[0] = 1.f;
			RD->ScreenSize[1] = Screen1;
		}
		return Mesh;
	}

	/** The lite version of a herb for its LOD 1: a third of the leaves, half again as large; no flowers but the biggest. */
	FPlantSpecies Lite(const FPlantSpecies& S)
	{
		FPlantSpecies L = S;
		FPlantHerb& H = L.Herb;
		H.Leaves = FMath::Max(2, FMath::RoundToInt(H.Leaves * 0.4f));
		H.Stems = FMath::Max(2, FMath::RoundToInt(H.Stems * 0.6f));
		H.Branches = FMath::Min(H.Branches, 1);
		H.Leaflets = H.Leaflets > 0 ? FMath::Max(4, H.Leaflets / 2) : 0;
		H.Leaflet.X *= 1.5;
		H.PerNode = 1;
		H.Flowers = FMath::Min(H.Flowers, 2);
		L.Leaf.Length *= H.Leaflets > 0 ? 1.f : 1.5f;
		L.Leaf.Teeth = 0;
		L.Leaf.Lobes = FMath::Min(L.Leaf.Lobes, 3);
		return L;
	}

	UHierarchicalInstancedStaticMeshComponent* MakeComp(AActor* Owner, int32 Index)
	{
		UHierarchicalInstancedStaticMeshComponent* C = NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner, *FString::Printf(TEXT("Floor%d"), Index), RF_Transient);
		C->SetupAttachment(Owner->GetRootComponent());
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(false);
		C->bAffectDynamicIndirectLighting = false;
		C->bAffectDistanceFieldLighting = false;
		C->bReceivesDecals = false;
		C->bUseAsOccluder = false;
		C->NumCustomDataFloats = 2;
		// the outdoor sun lights channel 1 only
		C->LightingChannels.bChannel0 = true;
		C->LightingChannels.bChannel1 = true;
		C->RegisterComponent();
		return C;
	}

	void UploadHalf(UTextureRenderTarget2D* RT, int32 W, int32 H, TArray<FFloat16>&& Data)
	{
		if (!RT) return;
		FTextureResource* Res = RT->GetResource();
		ENQUEUE_RENDER_COMMAND(LyudLitterUpload)([Res, W, H, Data = MoveTemp(Data)](FRHICommandListImmediate& RHICmdList)
		{
			if (!Res || !Res->TextureRHI) return;
			const FUpdateTextureRegion2D Region(0, 0, 0, 0, W, H);
			RHICmdList.UpdateTexture2D(Res->TextureRHI, 0, Region, W * 4 * sizeof(FFloat16), (const uint8*)Data.GetData());
		});
	}
}

// ------------------------------------------------------------------------------------------------------------ subsystem

bool ULyudForestFloorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer);
}

void ULyudForestFloorSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld())
	{
		return;
	}
	if (TActorIterator<AForestFloor> It(&InWorld); It)
	{
		Floor = *It;
		return;
	}
	if (TActorIterator<AProceduralForest> It(&InWorld); It)
	{
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		P.Name = TEXT("ForestFloor");
		Floor = InWorld.SpawnActor<AForestFloor>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	}
}

AForestFloor* ULyudForestFloorSubsystem::FindFloor(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const ULyudForestFloorSubsystem* S = World ? World->GetSubsystem<ULyudForestFloorSubsystem>() : nullptr;
	return S ? S->GetFloor() : nullptr;
}

// ------------------------------------------------------------------------------------------------------------ actor

AForestFloor::AForestFloor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Plant(TEXT("/Game/ForestFloor/M_FloorPlant.M_FloorPlant"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Grass(TEXT("/Game/ForestFloor/M_FloorGrass.M_FloorGrass"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Debris(TEXT("/Game/ForestFloor/M_FloorDebris.M_FloorDebris"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Falling(TEXT("/Game/ForestFloor/M_FloorFalling.M_FloorFalling"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Basic(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> Mpc(TEXT("/Game/Weather/MPC_LyudWeather.MPC_LyudWeather"));
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> Rt(TEXT("/Game/ForestFloor/RT_Litter.RT_Litter"));
	PlantMaterial = Plant.Object ? Plant.Object : Basic.Object;
	GrassMaterial = Grass.Object ? Grass.Object : Basic.Object;
	DebrisMaterial = Debris.Object ? Debris.Object : Basic.Object;
	FallingMaterial = Falling.Object ? Falling.Object.Get() : PlantMaterial.Get();
	Collection = Mpc.Object;
	LitterRT = Rt.Object;
}

float AForestFloor::CurrentDay() const
{
	return DevDay >= 0.f ? DevDay : UGameCalendar::DayOfYear(UGameCalendar::NowIn(GetWorld()));
}

bool AForestFloor::CameraLocation(FVector& Out) const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (PC && PC->PlayerCameraManager)
	{
		Out = PC->PlayerCameraManager->GetCameraLocation();
		return true;
	}
	return false;
}

void AForestFloor::BeginPlay()
{
	Super::BeginPlay();
	// the landscape's vertices: the floor samples the ground exactly on them
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->FindComponentByClass<ULandscapeHeightfieldCollisionComponent>())
		{
			const FVector L = It->GetActorLocation();
			GridOrigin = FVector2D(FMath::Fmod(L.X, 100.0), FMath::Fmod(L.Y, 100.0));
			break;
		}
	}
	if (LitterRT)
	{
		LitterRT->ResizeTarget(LitterN, LitterN);
	}
}

void AForestFloor::ClearKinds()
{
	for (FKind* K : Kinds) delete K;
	Kinds.Reset();
}

void AForestFloor::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearKinds();
	Super::EndPlay(Reason);
}

void AForestFloor::BeginDestroy()
{
	ClearKinds();
	Super::BeginDestroy();
}

template <typename FFn>
void AForestFloor::ForTreesNear(const FVector& At, float Radius, FFn&& Fn) const
{
	const int32 X0 = FMath::FloorToInt((At.X - Radius) / TreeCell), X1 = FMath::FloorToInt((At.X + Radius) / TreeCell);
	const int32 Y0 = FMath::FloorToInt((At.Y - Radius) / TreeCell), Y1 = FMath::FloorToInt((At.Y + Radius) / TreeCell);
	const float R2 = Radius * Radius;
	for (int32 Y = Y0; Y <= Y1; Y++)
	{
		for (int32 X = X0; X <= X1; X++)
		{
			if (const TArray<FTreeRef>* Trees = TreeGrid.Find(FIntPoint(X, Y)))
			{
				for (const FTreeRef& T : *Trees)
				{
					const float D2 = FMath::Square(T.At.X - (float)At.X) + FMath::Square(T.At.Y - (float)At.Y);
					if (D2 < R2) Fn(T, FMath::Sqrt(D2));
				}
			}
		}
	}
}

void AForestFloor::BuildTreeIndex()
{
	TreeGrid.Reset();
	TreeCount = 0;
	ForestTreeSum = 0;
	auto Add = [&](FName Species, const FVector& At, float Age, bool bWild)
	{
		const FPlantSpecies* S = PlantSpeciesLibrary::Find(Species);
		if (!S || S->Herb.Form != EPlantHerbForm::None) return;
		FTreeRef T;
		T.At = FVector3f(At);
		T.Height = PlantModel::Height(*S, Age) * 100.f;
		T.TrunkR = (S->TrunkRadiusPerHeight * T.Height * 0.01f + S->RingWidth * Age) * 100.f;
		const int32 C = ClassOf(Species);
		T.Class = (uint8)FMath::Min(C, 255);
		T.Crown = FMath::Clamp(T.Height * (GClasses[C].bConifer ? 0.18f : 0.26f), 120.f, 650.f);
		T.bWild = bWild;
		TreeGrid.FindOrAdd(FIntPoint(FMath::FloorToInt(At.X / TreeCell), FMath::FloorToInt(At.Y / TreeCell))).Add(T);
		TreeCount++;
	};
	for (TActorIterator<AProceduralForest> It(GetWorld()); It; ++It)
	{
		ForestTreeSum += It->NumTrees();
		// the wild woods: not the Academgorodok's garden, not the town's trees between the houses
		bool bWild = !It->Tags.Contains(TEXT("Academ"));
		for (const FForestMix& M : It->Mix) bWild &= !IsTownSpecies(M.Species);
		It->ForEachTree([&](FName Species, const FTransform& Xf, float Age) { Add(Species, Xf.GetLocation(), Age, bWild); });
	}
	for (TActorIterator<AProceduralPlant> It(GetWorld()); It; ++It)
	{
		// the live plants a forest swaps in stand where its own tree is; only count those of nobody's forest (the club's pines, the alley)
		if (It->GetOwner() == nullptr && !It->IsHidden()) Add(It->Species, It->GetActorLocation(), It->AgeAtStart, false);
	}
	BuildSites();
}

int32 AForestFloor::WildTreesNear(float X, float Y, float Radius) const
{
	int32 N = 0;
	ForTreesNear(FVector(X, Y, 0.0), Radius, [&](const FTreeRef& T, float) { N += T.bWild ? 1 : 0; });
	return N;
}

void AForestFloor::BuildSites()
{
	SiteForest.Reset();
	SiteClear.Reset();
	SiteKind.Reset();
	BuiltBoxes.Reset();
	SiteNX = SiteNY = 0;
	TArray<uint8> Bin;
	FFileHelper::LoadFileToArray(Bin, *(FPaths::ProjectContentDir() / TEXT("AnimalsData/animal_world.bin")));
	if (Bin.Num() < 24 || FMemory::Memcmp(Bin.GetData(), "AWG1", 4) != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ForestFloor: no walk grid (AnimalsData/animal_world.bin): no forest floor anywhere"));
		return;
	}
	int32 WNX, WNY;
	float X0, Y0, Cell;
	FMemory::Memcpy(&WNX, Bin.GetData() + 4, 4);
	FMemory::Memcpy(&WNY, Bin.GetData() + 8, 4);
	FMemory::Memcpy(&X0, Bin.GetData() + 12, 4);
	FMemory::Memcpy(&Y0, Bin.GetData() + 16, 4);
	FMemory::Memcpy(&Cell, Bin.GetData() + 20, 4);
	const int64 N = int64(WNX) * WNY;
	const int64 KindsAt = 24 + N * 2;
	if (WNX <= 0 || WNY <= 0 || Bin.Num() < KindsAt + N)
	{
		return;
	}
	// the walk grid covers the town's valley; the site grid runs on its lattice over the whole of the wild woods too
	double MinX = X0 * 100.0, MinY = Y0 * 100.0, MaxX = (X0 + (WNX - 1) * Cell) * 100.0, MaxY = (Y0 + (WNY - 1) * Cell) * 100.0;
	for (const TPair<FIntPoint, TArray<FTreeRef>>& P : TreeGrid)
	{
		for (const FTreeRef& T : P.Value)
		{
			if (!T.bWild) continue;
			MinX = FMath::Min(MinX, T.At.X - 5000.0); MaxX = FMath::Max(MaxX, T.At.X + 5000.0);
			MinY = FMath::Min(MinY, T.At.Y - 5000.0); MaxY = FMath::Max(MaxY, T.At.Y + 5000.0);
		}
	}
	SiteCellCm = Cell * 100.f;
	const int32 OX = FMath::CeilToInt((X0 * 100.0 - MinX) / SiteCellCm), OY = FMath::CeilToInt((Y0 * 100.0 - MinY) / SiteCellCm);
	SiteOrigin = FVector2D(X0 * 100.0 - OX * SiteCellCm, Y0 * 100.0 - OY * SiteCellCm);
	SiteNX = FMath::CeilToInt((MaxX - SiteOrigin.X) / SiteCellCm) + 1;
	SiteNY = FMath::CeilToInt((MaxY - SiteOrigin.Y) / SiteCellCm) + 1;
	const int64 SN = int64(SiteNX) * SiteNY;
	SiteKind.Init(SiteBeyond, SN);
	for (int32 Y = 0; Y < WNY; Y++)
		for (int32 X = 0; X < WNX; X++)
			SiteKind[int64(Y + OY) * SiteNX + X + OX] = Bin[KindsAt + int64(Y) * WNX + X] & 7;
	const int16* Zw = (const int16*)(Bin.GetData() + 24);
	// forest ground: the wild woods' cells, the steep ones among them (blocked for walkers, still forest; not the sea),
	// and beyond the walk grid the wild forests' own tiles (AProceduralForest's extent)
	SiteForest.SetNumZeroed(SN);
	for (TActorIterator<AProceduralForest> It(GetWorld()); It; ++It)
	{
		bool bWild = !It->Tags.Contains(TEXT("Academ"));
		for (const FForestMix& M : It->Mix) bWild &= !IsTownSpecies(M.Species);
		if (!bWild) continue;
		const FVector L = It->GetActorLocation();
		const int32 IX0 = FMath::Max(FMath::CeilToInt((L.X - It->HalfExtent.X - SiteOrigin.X) / SiteCellCm), 0), IX1 = FMath::Min(FMath::FloorToInt((L.X + It->HalfExtent.X - SiteOrigin.X) / SiteCellCm), SiteNX - 1);
		const int32 IY0 = FMath::Max(FMath::CeilToInt((L.Y - It->HalfExtent.Y - SiteOrigin.Y) / SiteCellCm), 0), IY1 = FMath::Min(FMath::FloorToInt((L.Y + It->HalfExtent.Y - SiteOrigin.Y) / SiteCellCm), SiteNY - 1);
		for (int32 Y = IY0; Y <= IY1; Y++)
			for (int32 X = IX0; X <= IX1; X++)
				if (SiteKind[int64(Y) * SiteNX + X] == SiteBeyond) SiteForest[int64(Y) * SiteNX + X] = 1;
	}
	const int32 Steep = 3;
	for (int32 Y = 0; Y < SiteNY; Y++)
	{
		for (int32 X = 0; X < SiteNX; X++)
		{
			const int64 I = int64(Y) * SiteNX + X;
			if (SiteKind[I] == WalkForest) { SiteForest[I] = 1; continue; }
			if (SiteKind[I] == SiteBeyond) continue;
			if (SiteKind[I] != WalkBlocked || Zw[int64(Y - OY) * WNX + X - OX] <= -4200) continue;
			for (int32 J = FMath::Max(Y - Steep, 0); J <= FMath::Min(Y + Steep, SiteNY - 1) && !SiteForest[I]; J++)
				for (int32 K = FMath::Max(X - Steep, 0); K <= FMath::Min(X + Steep, SiteNX - 1); K++)
					if (SiteKind[int64(J) * SiteNX + K] == WalkForest) { SiteForest[I] = 1; break; }
		}
	}
	const ALandscapeProxy* Land = nullptr;
	for (TActorIterator<ALandscapeProxy> It(GetWorld()); It && !Land; ++It) Land = *It;
	const int32 NX = SiteNX, NY = SiteNY;
	// what is built: paved and yard cells, and the footprints of the actors nothing may grow on
	TArray<float> D;
	D.Init(1e9f, SN);
	for (int64 I = 0; I < SN; I++) if (SiteKind[I] == WalkPaved || SiteKind[I] == WalkYard) D[I] = 0.f;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const AActor* A = *It;
		if (!(A->Tags.Contains(TEXT("NoTree")) || A->Tags.Contains(TEXT("NoGrass")) || A->Tags.Contains(TEXT("Roof")))) continue;
		TArray<UPrimitiveComponent*> Prims;
		A->GetComponents(Prims);
		for (const UPrimitiveComponent* P : Prims)
		{
			if (!P->IsRegistered()) continue;
			const FBox B = P->Bounds.GetBox();
			if (!B.IsValid || B.GetSize().X > MaxBuiltBox || B.GetSize().Y > MaxBuiltBox) continue;
			if (B.Max.X < SiteOrigin.X || B.Max.Y < SiteOrigin.Y || B.Min.X > SiteOrigin.X + NX * SiteCellCm || B.Min.Y > SiteOrigin.Y + NY * SiteCellCm) continue;
			const int32 IX0 = FMath::Clamp(FMath::FloorToInt((B.Min.X - SiteOrigin.X) / SiteCellCm), 0, NX - 1), IX1 = FMath::Clamp(FMath::CeilToInt((B.Max.X - SiteOrigin.X) / SiteCellCm), 0, NX - 1);
			const int32 IY0 = FMath::Clamp(FMath::FloorToInt((B.Min.Y - SiteOrigin.Y) / SiteCellCm), 0, NY - 1), IY1 = FMath::Clamp(FMath::CeilToInt((B.Max.Y - SiteOrigin.Y) / SiteCellCm), 0, NY - 1);
			// only what stands on the ground (not a rope or a cabin high over the woods)
			const TOptional<float> G = Land ? Land->GetHeightAtLocation(B.GetCenter()) : TOptional<float>();
			if (G.IsSet() && B.Min.Z > G.GetValue() + 800.f) continue;
			for (int32 Y = IY0; Y <= IY1; Y++)
				for (int32 X = IX0; X <= IX1; X++)
					D[int64(Y) * NX + X] = 0.f;
			BuiltBoxes.Add(B);
		}
	}
	// metres to the nearest built cell (two-pass chamfer distance)
	const float Dg = SiteCellCm * UE_SQRT_2;
	for (int32 Y = 0; Y < NY; Y++)
	{
		for (int32 X = 0; X < NX; X++)
		{
			float& V = D[int64(Y) * NX + X];
			if (X > 0) V = FMath::Min(V, D[int64(Y) * NX + X - 1] + SiteCellCm);
			if (Y > 0)
			{
				V = FMath::Min(V, D[int64(Y - 1) * NX + X] + SiteCellCm);
				if (X > 0) V = FMath::Min(V, D[int64(Y - 1) * NX + X - 1] + Dg);
				if (X < NX - 1) V = FMath::Min(V, D[int64(Y - 1) * NX + X + 1] + Dg);
			}
		}
	}
	for (int32 Y = NY - 1; Y >= 0; Y--)
	{
		for (int32 X = NX - 1; X >= 0; X--)
		{
			float& V = D[int64(Y) * NX + X];
			if (X < NX - 1) V = FMath::Min(V, D[int64(Y) * NX + X + 1] + SiteCellCm);
			if (Y < NY - 1)
			{
				V = FMath::Min(V, D[int64(Y + 1) * NX + X] + SiteCellCm);
				if (X < NX - 1) V = FMath::Min(V, D[int64(Y + 1) * NX + X + 1] + Dg);
				if (X > 0) V = FMath::Min(V, D[int64(Y + 1) * NX + X - 1] + Dg);
			}
		}
	}
	SiteClear.SetNumUninitialized(SN);
	int64 Ok = 0;
	for (int64 I = 0; I < SN; I++)
	{
		// a cell's centre may lie a cell's half diagonal off the point asked, a built edge half a cell off the cell that
		// marks it, a piece up to 1 m off the ground's vertices that allowed it: keep that much further away
		SiteClear[I] = (uint8)FMath::Clamp((D[I] - SiteCellCm * 1.21f - 100.f) / 100.f, 0.f, 255.f);
		Ok += SiteForest[I] && SiteClear[I] * 100.f >= KeepOffBuilt ? 1 : 0;
	}
	UE_LOG(LogTemp, Log, TEXT("ForestFloor: sites %d x %d (%.0f cm), %lld forest floor cells (%.1f%%), %d built footprints"),
		SiteNX, SiteNY, SiteCellCm, Ok, 100.0 * Ok / SN, BuiltBoxes.Num());
}

bool AForestFloor::SiteCell(double X, double Y, int32& OutIndex) const
{
	if (SiteNX <= 0) return false;
	const int32 IX = FMath::RoundToInt((X - SiteOrigin.X) / SiteCellCm), IY = FMath::RoundToInt((Y - SiteOrigin.Y) / SiteCellCm);
	if (IX < 0 || IY < 0 || IX >= SiteNX || IY >= SiteNY) return false;
	OutIndex = IY * SiteNX + IX;
	return true;
}

float AForestFloor::SiteAt(FVector Location) const
{
	int32 I;
	return SiteCell(Location.X, Location.Y, I) && SiteForest[I] && SiteClear[I] * 100.f >= KeepOffBuilt ? 1.f : 0.f;
}

float AForestFloor::SiteSoft(double X, double Y) const
{
	int32 I;
	if (!SiteCell(X, Y, I) || !SiteForest[I]) return 0.f;
	return SmoothStep(KeepOffBuilt * 0.004f, KeepOffBuilt * 0.01f, (float)SiteClear[I]);
}

float AForestFloor::SnowDepthAt(const FVector& At) const
{
	const ULyudWinterSubsystem* W = ULyudWinterSubsystem::Get(this);
	return W && W->IsReady() ? W->GetSnowDepthAt(At) : 0.f;
}

FForestLitter AForestFloor::LitterFrom(const FVector& At, float Day) const
{
	FForestLitter L;
	float Needle = 0.f, Spruce = 0.f, Leaf = 0.f, Hue = 0.f, Decay = 0.f, Sum = 0.f;
	ForTreesNear(At, 1600.f, [&](const FTreeRef& T, float D)
	{
		const FTreeClass& C = GClasses[T.Class];
		// leaves and needles fall under the crown and a little beyond it; a young tree drops less
		const float W = FMath::Exp(-FMath::Square(D / (T.Crown * 1.5f))) * FMath::Clamp(T.Height / 600.f, 0.1f, 1.f);
		Sum += W;
		if (C.bConifer)
		{
			Needle += W;
			Spruce += C.bSpruce ? W : 0.f;
			return;
		}
		const FVector2f F = LeafLitter(C, Day);
		Leaf += W * F.X;
		Hue += W * F.X * C.Hue;
		Decay += W * F.X * F.Y;
	});
	L.Canopy = 1.f - FMath::Exp(-Sum * 1.2f);
	L.Needles = 1.f - FMath::Exp(-Needle * 1.4f);
	L.Spruce = Needle > 1e-3f ? Spruce / Needle : 0.f;
	L.Leaves = 1.f - FMath::Exp(-Leaf * 1.6f);
	L.Hue = Leaf > 1e-3f ? Hue / Leaf : 0.f;
	L.Decay = Leaf > 1e-3f ? Decay / Leaf : 0.f;
	return L;
}

FForestLitter AForestFloor::LitterAt(FVector Location) const
{
	return LitterFrom(Location, CurrentDay());
}

// ------------------------------------------------------------------------------------------------------------ kinds

void AForestFloor::EnsureKinds()
{
	if (!Kinds.IsEmpty()) return;
	const float Day = CurrentDay();
	auto New = [&](FName Name, EKindType Type, int32 Band, float Cull, float Density) -> FKind&
	{
		FKind* K = new FKind;
		K->Name = Name;
		K->Type = Type;
		K->Band = Band;
		K->Cull = Cull;
		K->Density = Density;
		Kinds.Add(K);
		return *K;
	};
	// --- ground cover: the plant system's species
	struct FHerbKind { const TCHAR* Species; int32 Band; float Cull; float Density; };
	for (const FHerbKind& H : { FHerbKind{ TEXT("Bilberry"), 1, 3200.f, 3.f }, FHerbKind{ TEXT("Lingonberry"), 1, 2600.f, 4.f },
		FHerbKind{ TEXT("Heather"), 1, 3200.f, 1.6f }, FHerbKind{ TEXT("Bracken"), 1, 4200.f, 0.9f }, FHerbKind{ TEXT("BucklerFern"), 1, 4200.f, 0.3f },
		FHerbKind{ TEXT("WoodSorrel"), 0, 1600.f, 7.f } })
	{
		for (int32 V = 0; V < 3; V++)
		{
			FKind& K = New(*FString::Printf(TEXT("%s_%d"), H.Species, V), EKindType::Herb, H.Band, H.Cull, H.Density / 3.f);
			K.Species = H.Species;
			K.VariantSeed = 1 + V * 7919;
			K.Variants = 3;
			K.Variant = V;
			K.Age = 6.f + 4.f * V;
		}
	}
	for (const TCHAR* G : { TEXT("FeatherMoss"), TEXT("HaircapMoss"), TEXT("ReindeerLichen"), TEXT("WavyHairGrass") })
	{
		const FGrassSpecies* S = GrassSpeciesLibrary::Find(G);
		FKind& K = New(G, EKindType::Grass, 0, FCString::Strcmp(G, TEXT("WavyHairGrass")) == 0 ? 2400.f : 1500.f, S ? S->Density : 10.f);
		K.Species = G;
	}
	for (const TCHAR* Sd : { TEXT("ScotsPine"), TEXT("NorwaySpruce"), TEXT("SilverBirch"), TEXT("Rowan") })
	{
		for (int32 V = 0; V < 2; V++)
		{
			FKind& K = New(*FString::Printf(TEXT("Seedling_%s_%d"), Sd, V), EKindType::Seedling, 1, 4200.f, 0.012f);
			K.Species = Sd;
			K.VariantSeed = 31 + V * 104729;
			K.Age = V == 0 ? 4.f : 8.f;
			K.Variants = 2;
			K.Variant = V;
		}
	}
	// --- fallen leaves of every deciduous species in the woods, and the leaves falling from them
	TSet<FName> Deciduous;
	for (const FTreeClass& C : GClasses) if (!C.bConifer) Deciduous.Add(C.Species);
	for (const FName Sp : Deciduous)
	{
		FKind& K = New(*FString::Printf(TEXT("Leaf_%s"), *Sp.ToString()), EKindType::Leaf, 0, 1300.f, 22.f);
		K.Species = Sp;
		K.bAlign = true;
		K.Top = 1.f;
	}
	if (!Deciduous.IsEmpty())
	{
		FKind& K = New(TEXT("FallingLeaves"), EKindType::Falling, 0, 2500.f, 0.f);
		K.Species = *Deciduous.CreateConstIterator();
		for (const FName Sp : Deciduous) if (Sp.ToString().Contains(TEXT("Birch"))) K.Species = Sp;
	}
	// --- debris built in code
	auto Debris = [&](const TCHAR* Name, int32 Band, float Cull, float Density, float Pattern, const FFM::FGeom& G, int32 Variants, int32 Variant) -> FKind&
	{
		FKind& K = New(Name, EKindType::Debris, Band, Cull, Density / Variants);
		K.Geom = G;
		K.Pattern = Pattern;
		K.Variants = Variants;
		K.Variant = Variant;
		K.bAlign = true;
		return K;
	};
	const FLinearColor Granite(0.3f, 0.27f, 0.25f), GraniteB(0.42f, 0.33f, 0.3f), Gneiss(0.24f, 0.24f, 0.25f), GneissB(0.36f, 0.35f, 0.34f);
	for (int32 V = 0; V < 3; V++)
	{
		FKind& K = Debris(*FString::Printf(TEXT("Stone_%d"), V), 1, 3000.f, 0.07f, 1.f, FFM::Stone(100 + V, 9.f + 5.f * V, 0.6f + 0.1f * V, 2), 3, V);
		K.TintA = V == 1 ? Gneiss : Granite; K.TintB = V == 1 ? GneissB : GraniteB;
		K.ScaleMin = 0.5f; K.ScaleMax = 1.6f;
	}
	for (int32 V = 0; V < 2; V++)
	{
		FKind& K = Debris(*FString::Printf(TEXT("Boulder_%d"), V), 2, 7000.f, 0.0025f, 1.f, FFM::Stone(200 + V, 45.f + 20.f * V, 0.65f, 3), 2, V);
		K.GeomLod1 = FFM::Stone(200 + V, 45.f + 20.f * V, 0.65f, 1);
		K.TintA = V ? Gneiss : Granite; K.TintB = V ? GneissB : GraniteB;
		K.bShadow = true; K.bCollision = true; K.ScaleMin = 0.6f; K.ScaleMax = 1.5f;
	}
	for (int32 V = 0; V < 3; V++)
	{
		// twigs of every tree are dark (a birch is white only on its trunk and old limbs)
		FKind& K = Debris(*FString::Printf(TEXT("Stick_%d"), V), 1, 2600.f, 0.35f, 0.f, FFM::Stick(300 + V, 35.f + 22.f * V, 0.8f + 0.35f * V, V, 5), 3, V);
		K.Wood = V == 1 ? EWood::Spruce : EWood::Pine;
	}
	for (int32 V = 0; V < 2; V++)
	{
		FKind& K = Debris(*FString::Printf(TEXT("Branch_%d"), V), 2, 4500.f, 0.05f, 0.f, FFM::Stick(400 + V, 180.f + 90.f * V, 3.f + V, 4 + V, 7), 2, V);
		K.GeomLod1 = FFM::Stick(400 + V, 180.f + 90.f * V, 3.f + V, 2, 4);
		K.Wood = V ? EWood::Spruce : EWood::Pine; K.bShadow = true;
	}
	for (int32 V = 0; V < 3; V++)
	{
		const EWood W = (EWood)V;
		FKind& K = Debris(*FString::Printf(TEXT("Log_%d"), V), 2, 7000.f, 0.004f, 0.f, FFM::Log(500 + V, 500.f + 150.f * V, 16.f + 5.f * V), 3, V);
		K.Wood = W; K.bShadow = true; K.bCollision = true; K.ScaleMin = 0.75f; K.ScaleMax = 1.25f;
		FKind& S = Debris(*FString::Printf(TEXT("Stump_%d"), V), 2, 6000.f, 0.005f, 0.f, FFM::Stump(600 + V, 17.f + 5.f * V, 30.f + 25.f * V), 3, V);
		S.Wood = W; S.bShadow = true; S.bCollision = true; S.bAlign = false;
	}
	for (int32 V = 0; V < 2; V++)
	{
		FKind& K = Debris(*FString::Printf(TEXT("Roots_%d"), V), 1, 3500.f, 0.f, 0.f, FFM::Roots(700 + V, 20.f, 5 + V), 2, V);
		K.Wood = V ? EWood::Spruce : EWood::Pine; K.bShadow = true; K.bAlign = false;
	}
	{
		FKind& P = Debris(TEXT("PineCone"), 0, 1700.f, 3.f, 2.f, FFM::Cone(800, 5.f, 1.8f, 0.8f), 1, 0);
		P.TintA = FLinearColor(0.16f, 0.09f, 0.045f); P.TintB = FLinearColor(0.26f, 0.17f, 0.09f);
		FKind& S = Debris(TEXT("SpruceCone"), 0, 1700.f, 2.f, 2.f, FFM::Cone(801, 12.f, 2.f, 0.25f), 1, 0);
		S.TintA = FLinearColor(0.13f, 0.07f, 0.035f); S.TintB = FLinearColor(0.24f, 0.13f, 0.06f);
		FKind& B = Debris(TEXT("BarkCurl"), 0, 1600.f, 0.8f, 0.f, FFM::BarkCurl(900, 14.f), 1, 0);
		B.Wood = EWood::Birch;
	}
	for (int32 V = 0; V < 2; V++)
	{
		// low, lumpy pillows of moss over a buried stump or stone, not domes
		FKind& K = Debris(*FString::Printf(TEXT("Cushion_%d"), V), 2, 4500.f, 0.015f, 3.f, FFM::Cushion(1000 + V, 30.f + 18.f * V, 9.f + 4.f * V), 2, V);
		K.TintA = FLinearColor(0.035f, 0.06f, 0.015f); K.TintB = FLinearColor(0.08f, 0.11f, 0.03f);
		K.bShadow = true; K.bAlign = false;
	}
	for (int32 I = 0; I < Kinds.Num(); I++)
	{
		Kinds[I]->Comp = Comps.Add(MakeComp(this, I));
		const FString Nm = Kinds[I]->Name.ToString();
		Kinds[I]->bBig = Kinds[I]->Type == EKindType::Debris && (Kinds[I]->bCollision || Nm.StartsWith(TEXT("Branch_")) || Nm.StartsWith(TEXT("Roots_")));
		BuildKind(*Kinds[I]);
		MaxReach = FMath::Max(MaxReach, Kinds[I]->Cull);
	}
	UpdateSeason(Day, true);
}

void AForestFloor::BuildKind(FKind& K)
{
	UHierarchicalInstancedStaticMeshComponent* C = Comps[K.Comp];
	const bool bShadows = CVarFloorShadows.GetValueOnGameThread() > 0;
	C->SetCastShadow(K.bShadow && bShadows);
	C->SetCullDistances((int32)(K.Cull * 0.8f), (int32)K.Cull);
	if (K.bCollision)
	{
		// they stop walkers (the player, people, animals) and things thrown, nothing else: the winter takes any WorldStatic
		// it traces for what the snow lies on (a log's box would hold a shelf of snow in the air) and the rain's roof map
		// looks along Visibility
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		C->SetCollisionObjectType(ECC_WorldDynamic);
		C->SetCollisionResponseToAllChannels(ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		C->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	}
	switch (K.Type)
	{
	case EKindType::Debris:
	{
		TArray<const FFM::FGeom*> Lods = { &K.Geom };
		if (K.GeomLod1.Tris.Num()) Lods.Add(&K.GeomLod1);
		K.Mesh = FFM::Build(this, *FString::Printf(TEXT("SM_Floor_%s"), *K.Name.ToString()), Lods, { 1.f, 0.05f }, K.bCollision);
		K.Lint = FFM::Lint(K.Geom, K.Pattern == 0.f || K.Pattern == 2.f, K.LintProblems);
		K.Top = K.Geom.Top();
		K.Triangles = K.Geom.Tris.Num();
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(DebrisMaterial, this);
		M->SetScalarParameterValue(TEXT("Pattern"), K.Pattern);
		// moss grows on old logs and stumps, the roots and stones, a little on fallen branches
		const FString N = K.Name.ToString();
		M->SetScalarParameterValue(TEXT("MossAmount"), N.StartsWith(TEXT("Log")) || N.StartsWith(TEXT("Stump")) ? 1.f
			: N.StartsWith(TEXT("Roots")) ? 0.7f : N.StartsWith(TEXT("Boulder")) ? 0.8f : N.StartsWith(TEXT("Stone")) ? 0.4f
			: N.StartsWith(TEXT("Branch")) ? 0.35f : N.StartsWith(TEXT("Stick")) ? 0.15f : 0.f);
		M->SetVectorParameterValue(TEXT("TintA"), K.TintA);
		M->SetVectorParameterValue(TEXT("TintB"), K.TintB);
		if (K.Pattern == 0.f)
		{
			const TCHAR* Sp = K.Wood == EWood::Birch ? TEXT("SilverBirch") : K.Wood == EWood::Spruce ? TEXT("NorwaySpruce") : TEXT("ScotsPine");
			if (const FPlantSpecies* S = PlantSpeciesLibrary::Find(Sp))
			{
				UTexture2D *Albedo = nullptr, *Normal = nullptr;
				PlantMeshes::BuildBarkTextures(this, *S, 7, Albedo, Normal);
				KeepAlive.Add(Albedo);
				KeepAlive.Add(Normal);
				M->SetTextureParameterValue(TEXT("BarkAlbedo"), Albedo);
				M->SetTextureParameterValue(TEXT("BarkNormal"), Normal);
				// a tile of the bark texture spans TileWidth x TileHeight m (the UVs are in metres)
				M->SetVectorParameterValue(TEXT("BarkTile"), FLinearColor(1.f / S->Bark.TileWidth, 1.f / S->Bark.TileHeight, 0.f, 0.f));
				M->SetVectorParameterValue(TEXT("TintA"), S->Bark.FurrowColour);
				M->SetVectorParameterValue(TEXT("TintB"), S->Bark.RidgeColour);
			}
		}
		K.MIDs = { M };
		C->SetStaticMesh(K.Mesh);
		C->SetMaterial(0, M);
		break;
	}
	case EKindType::Grass:
	{
		const FGrassSpecies* G = GrassSpeciesLibrary::Find(K.Species);
		if (!G) break;
		K.Mesh = PlantMeshes::BuildGrassTuft(this, *G);
		K.Top = G->Height * 100.f;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(GrassMaterial, this);
		M->SetVectorParameterValue(TEXT("GreenBase"), G->GreenBase);
		M->SetVectorParameterValue(TEXT("GreenTip"), G->GreenTip);
		M->SetVectorParameterValue(TEXT("DryBase"), G->DryBase);
		M->SetVectorParameterValue(TEXT("DryTip"), G->DryTip);
		M->SetScalarParameterValue(TEXT("SwayAmp"), G->Sway);
		M->SetScalarParameterValue(TEXT("Moss"), G->Name == TEXT("WavyHairGrass") ? 0.f : 1.f);
		K.MIDs = { M };
		C->SetStaticMesh(K.Mesh);
		C->SetMaterial(0, M);
		const FStaticMeshRenderData* RD = K.Mesh->GetRenderData();
		K.Triangles = RD && RD->LODResources.Num() ? RD->LODResources[0].GetNumTriangles() : 0;
		break;
	}
	case EKindType::Herb:
	{
		const FPlantSpecies* S = PlantSpeciesLibrary::Find(K.Species);
		if (!S) break;
		// the species' three materials are shared by its variants
		for (const FKind* O : Kinds)
		{
			if (O != &K && O->Type == EKindType::Herb && O->Species == K.Species && O->MIDs.Num() == PlantHerbs::NumSlots)
			{
				K.MIDs = O->MIDs;
				break;
			}
		}
		if (K.MIDs.IsEmpty())
		{
			for (int32 Slot = 0; Slot < PlantHerbs::NumSlots; Slot++)
			{
				UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(PlantMaterial, this);
				// ferns and bracken wave; low shrubs barely stir
				M->SetScalarParameterValue(TEXT("SwayAmp"), S->Herb.Height > 0.5f ? 9.f : 2.5f);
				M->SetScalarParameterValue(TEXT("HeightRef"), FMath::Max(S->Herb.Height * 100.f, 10.f));
				K.MIDs.Add(M);
			}
			UTexture2D* Leaf = PlantHerbs::BuildLeafTexture(this, *S);
			K.MIDs[PlantHerbs::Leaves]->SetTextureParameterValue(TEXT("LeafDetail"), Leaf ? Leaf : PlantMeshes::BuildLeafDetail(this, S->Leaf));
			if (S->Herb.Flower != EPlantFlowerForm::None)
			{
				K.MIDs[PlantHerbs::Flowers]->SetTextureParameterValue(TEXT("LeafDetail"), PlantHerbs::BuildFlowerAtlas(this, *S));
			}
			K.MIDs[PlantHerbs::Stems]->SetVectorParameterValue(TEXT("Color"), S->Herb.StemColour);
		}
		K.Top = S->Herb.Height * 100.f;
		for (int32 Slot = 0; Slot < PlantHerbs::NumSlots; Slot++) C->SetMaterial(Slot, K.MIDs[Slot]);
		break;
	}
	case EKindType::Seedling:
	{
		if (const FPlantSpecies* S = PlantSpeciesLibrary::Find(K.Species)) K.Top = PlantModel::Height(*S, K.Age) * 100.f;
		break;
	}
	case EKindType::Leaf:
	case EKindType::Falling:
	{
		const FPlantSpecies* S = PlantSpeciesLibrary::Find(K.Species);
		if (!S) break;
		K.Mesh = PlantMeshes::BuildSingleLeaf(this, *S);
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(K.Type == EKindType::Falling ? FallingMaterial : PlantMaterial, this);
		M->SetTextureParameterValue(TEXT("LeafDetail"), PlantMeshes::BuildLeafDetail(this, S->Leaf));
		M->SetScalarParameterValue(TEXT("SwayAmp"), 0.f);
		M->SetScalarParameterValue(TEXT("Loose"), 1.f);
		M->SetVectorParameterValue(TEXT("Color"), S->AutumnLeafColour);
		M->SetVectorParameterValue(TEXT("ColorB"), S->AutumnLeafColourLate);
		K.MIDs = { M };
		C->SetStaticMesh(K.Mesh);
		C->SetMaterial(0, M);
		C->SetCastShadow(false);
		const FStaticMeshRenderData* RD = K.Mesh->GetRenderData();
		K.Triangles = RD && RD->LODResources.Num() ? RD->LODResources[0].GetNumTriangles() : 0;
		break;
	}
	}
	if (K.Mesh) KeepAlive.Add(K.Mesh);
	for (UMaterialInstanceDynamic* M : K.MIDs) KeepAlive.AddUnique(M);
}

void AForestFloor::UpdateSeason(float Day, bool bForce)
{
	const float SnowGround = ULyudWinterSubsystem::SnowOnGround(GetWorld(), PlantModel::SnowCover(Day));
	UTreeBakeCache* Cache = GetWorld() ? GetWorld()->GetSubsystem<UTreeBakeCache>() : nullptr;
	for (int32 I = 0; I < Kinds.Num(); I++)
	{
		FKind& K = *Kinds[I];
		UHierarchicalInstancedStaticMeshComponent* C = Comps[K.Comp];
		if (K.Type == EKindType::Herb)
		{
			const FPlantSpecies* S = PlantSpeciesLibrary::Find(K.Species);
			if (!S) continue;
			const PlantHerbs::FState St = PlantHerbs::State(*S, K.VariantSeed, K.Age, Day);
			const FLinearColor Frost(0.12f, 0.07f, 0.03f);
			if (K.Variant == 0)
			{
				K.MIDs[PlantHerbs::Leaves]->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(St.LeafColour, Frost, St.Wither));
				K.MIDs[PlantHerbs::Flowers]->SetVectorParameterValue(TEXT("Color"), FLinearColor::White);
			}
			if (St.Key != K.HerbKey || bForce || !K.Mesh)
			{
				PlantHerbs::FHerbGeometry G0, G1;
				float H = 0.f, R = 0.f;
				PlantHerbs::Build(*S, K.VariantSeed, St, G0, H, R);
				const FPlantSpecies L = Lite(*S);
				PlantHerbs::Build(L, K.VariantSeed, PlantHerbs::State(L, K.VariantSeed, K.Age, Day), G1, H, R);
				// LOD 1 from ~10 m for a knee-high shrub, ~18 m for a fern
				const float Screen = FMath::Clamp(S->Herb.Height * 100.f / (S->Herb.Height > 0.5f ? 1800.f : 1000.f), 0.005f, 0.5f);
				UStaticMesh* Old = K.Mesh;
				K.Mesh = BuildHerbLods(this, *FString::Printf(TEXT("SM_Floor_%s"), *K.Name.ToString()), G0, G1, Screen);
				KeepAlive.Remove(Old);
				KeepAlive.Add(K.Mesh);
				C->SetStaticMesh(K.Mesh);
				K.HerbKey = St.Key;
				K.Triangles = G0.NumTriangles();
				K.Top = FMath::Max(H * 100.f, 3.f);
			}
			C->SetVisibility(St.bAlive && !bHidden);
		}
		else if (K.Type == EKindType::Grass)
		{
			const FGrassSpecies* G = GrassSpeciesLibrary::Find(K.Species);
			if (!G || K.MIDs.IsEmpty()) continue;
			const float Green = SmoothStep(G->GreenUpStart, G->GreenUpFull, Day) * (1.f - SmoothStep(G->YellowStart, G->YellowFull, Day));
			const float Grow = Day < 200.f ? SmoothStep(G->GreenUpStart, G->GreenUpFull + 25.f, Day) : 1.f - SmoothStep(310.f, 345.f, Day);
			K.MIDs[0]->SetScalarParameterValue(TEXT("Dryness"), 1.f - Green);
			K.MIDs[0]->SetScalarParameterValue(TEXT("Snow"), SnowGround);
			K.MIDs[0]->SetScalarParameterValue(TEXT("HeightScale"), FMath::Lerp(G->WinterHeight, 1.f, Grow));
		}
		else if (K.Type == EKindType::Seedling && Cache)
		{
			const FTreeBaked* B = Cache->Request(K.Species, K.VariantSeed, K.Age, Day, 14.f, FVector(15.0, 55.0, 150.0));
			if (B && B->Mesh && B->Version != K.BakeVersion)
			{
				C->SetStaticMesh(B->Mesh);
				C->SetMaterial(0, B->Wood);
				C->SetMaterial(1, B->Leaf);
				C->SetMaterial(2, B->Card);
				C->SetMaterial(3, B->Bloom);
				K.Mesh = B->Mesh;
				K.Triangles = B->Triangles[0];
				K.BakeVersion = B->Version;
				DirtyKinds.AddUnique(I);
			}
		}
	}
	ShownDay = Day;
}

// ------------------------------------------------------------------------------------------------------------ layout

void AForestFloor::TraceCell(const FIntPoint& C, FCell& Cell)
{
	UWorld* World = GetWorld();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ForestFloorGround), false, this);
	FCollisionObjectQueryParams Obj(ECC_WorldStatic);
	const ULyudTerrainMask* Mask = ULyudTerrainMask::Get();
	const int32 Guard = CVarFloorGuard.GetValueOnGameThread();
	const bool bMask = Mask && Mask->HasForestFloor() && Guard >= 0;
	const bool bGuard = Guard > 0;
	const double X0 = GridOrigin.X + C.X * (double)CellSize, Y0 = GridOrigin.Y + C.Y * (double)CellSize;
	const float Step = CellSize / 8.f;
	for (int32 J = 0; J <= 8; J++)
	{
		for (int32 I = 0; I <= 8; I++)
		{
			const double X = X0 + I * Step, Y = Y0 + J * Step;
			// never in the town, the Academgorodok, a yard, on or by a path or a building: the wild woods' own ground,
			// 15 m clear of anything built, among trees (a lone garden tree or the alley's make no forest floor)
			// (the trees counted a metre closer than a piece asks: it may lie that far off the vertex)
			if (bGuard && (SiteAt(FVector(X, Y, 0.0)) <= 0.f || WildTreesNear(X, Y, SmallTreeR - 100.f) < SmallTrees))
			{
				Cell.Ground[J * 9 + I] = -1e9f;
				Cell.Floor[J * 9 + I] = 0;
				continue;
			}
			FHitResult Hit;
			float Z = -1e9f;
			uint8 F = 0;
			if (World->LineTraceSingleByObjectType(Hit, FVector(X, Y, 50000.0), FVector(X, Y, -20000.0), Obj, Q))
			{
				Z = (float)Hit.ImpactPoint.Z;
				// only the bare ground grows the floor: not a path, a yard, a platform or a trunk standing on it
				const bool bGround = Cast<ULandscapeHeightfieldCollisionComponent>(Hit.GetComponent()) != nullptr;
				const float M = bMask ? Mask->GetForestFloor(Hit.ImpactPoint) : 1.f;
				F = bGround ? (uint8)FMath::Clamp(M * 255.f, 0.f, 255.f) : 0;
			}
			Cell.Ground[J * 9 + I] = Z;
			Cell.Floor[J * 9 + I] = F;
		}
	}
	Cell.bTraced = true;
}

float AForestFloor::GroundAt(const FCell& Cell, const FIntPoint& C, float X, float Y, float& OutNormalZ, float& OutFloor) const
{
	const float Step = CellSize / 8.f;
	const float U = FMath::Clamp((X - (GridOrigin.X + C.X * CellSize)) / Step, 0.f, 7.999f);
	const float V = FMath::Clamp((Y - (GridOrigin.Y + C.Y * CellSize)) / Step, 0.f, 7.999f);
	const int32 I = (int32)U, J = (int32)V;
	const float Fu = U - I, Fv = V - J;
	const float A = Cell.Ground[J * 9 + I], B = Cell.Ground[J * 9 + I + 1], Cc = Cell.Ground[(J + 1) * 9 + I], D = Cell.Ground[(J + 1) * 9 + I + 1];
	if (FMath::Min(FMath::Min(A, B), FMath::Min(Cc, D)) < -1e8f)
	{
		OutFloor = 0.f;
		return 0.f;
	}
	OutFloor = FMath::Min(FMath::Min(Cell.Floor[J * 9 + I], Cell.Floor[J * 9 + I + 1]), FMath::Min(Cell.Floor[(J + 1) * 9 + I], Cell.Floor[(J + 1) * 9 + I + 1])) / 255.f;
	const float Gx = FMath::Lerp(B - A, D - Cc, Fv) / Step, Gy = FMath::Lerp(Cc - A, D - B, Fu) / Step;
	OutNormalZ = 1.f / FMath::Sqrt(1.f + Gx * Gx + Gy * Gy);
	return FMath::Lerp(FMath::Lerp(A, B, Fu), FMath::Lerp(Cc, D, Fu), Fv);
}

void AForestFloor::LayCell(const FIntPoint& C, FCell& Cell, int32 Band)
{
	const float Day = CurrentDay();
	const double X0 = GridOrigin.X + C.X * (double)CellSize, Y0 = GridOrigin.Y + C.Y * (double)CellSize;
	const FVector Mid(X0 + CellSize * 0.5, Y0 + CellSize * 0.5, 0.0);
	// what stands round the cell: shares of pine, spruce, birch, other broadleaf, and the gaps between
	float Pine = 0.f, Spruce = 0.f, Birch = 0.f, Broad = 0.f, Sum = 0.f;
	ForTreesNear(Mid, 1400.f, [&](const FTreeRef& T, float D)
	{
		const FTreeClass& Cl = GClasses[T.Class];
		const float W = FMath::Exp(-FMath::Square(D / (T.Crown * 1.6f + 300.f))) * FMath::Clamp(T.Height / 800.f, 0.15f, 1.f);
		Sum += W;
		if (Cl.bSpruce) Spruce += W; else if (Cl.bConifer) Pine += W; else if (Cl.bBirch) Birch += W; else Broad += W;
	});
	const float Canopy = 1.f - FMath::Exp(-Sum * 0.9f);
	if (Sum > 1e-3f) { Pine /= Sum; Spruce /= Sum; Birch /= Sum; Broad /= Sum; }
	const float Light = 1.f - Canopy * 0.85f;
	const ULyudTerrainMask* Mask = ULyudTerrainMask::Get();
	const float Scale = DensityScale * CVarFloorDensity.GetValueOnGameThread();
	const uint32 CellKey = HashCombineFast(GetTypeHash(C), (uint32)Seed);
	const bool bGuard = CVarFloorGuard.GetValueOnGameThread() > 0;

	auto Place = [&](FKind& K, int32 KindIndex, float X, float Y, float Yaw, float S, float C0, float C1, float Pitch = 0.f, float Roll = 0.f) -> bool
	{
		float Nz = 1.f, F = 0.f;
		const float Z = GroundAt(Cell, C, X, Y, Nz, F);
		if (F <= 0.f || Nz < 0.72f) return false;
		if (K.bBig && bGuard && WildTreesNear(X, Y, BigTreeR) < BigTrees) return false;
		FCellItem It;
		FQuat4f Q = FQuat4f(FRotator3f(Pitch, Yaw, Roll));
		if (K.bAlign)
		{
			// lie along the slope: tilt up by the ground's gradient
			float Nz2 = 1.f, F2 = 0.f;
			const float Zx = GroundAt(Cell, C, X + 30.f, Y, Nz2, F2), Zy = GroundAt(Cell, C, X, Y + 30.f, Nz2, F2);
			const FVector3f N = FVector3f(-(Zx - Z) / 30.f, -(Zy - Z) / 30.f, 1.f).GetSafeNormal();
			Q = FQuat4f::FindBetweenNormals(FVector3f::UpVector, N) * Q;
		}
		It.Xf = FTransform3f(Q, FVector3f(X, Y, Z - K.Sink * S), FVector3f(S));
		It.Custom0 = C0;
		It.Custom1 = C1;
		It.Top = K.Top * S;
		It.Kind = (uint16)KindIndex;
		Cell.Items.Add(It);
		return true;
	};

	for (int32 KI = 0; KI < Kinds.Num(); KI++)
	{
		FKind& K = *Kinds[KI];
		if (K.Band != Band || K.Type == EKindType::Falling) continue;
		const uint32 KS = HashCombineFast(CellKey, GetTypeHash(K.Name));
		if (K.Name.ToString().StartsWith(TEXT("Roots_")))
		{
			// roots round the feet of old trees standing in this cell, a kind per bark
			ForTreesNear(Mid, CellSize * 0.75f, [&](const FTreeRef& T, float D)
			{
				const FTreeClass& Cl = GClasses[T.Class];
				const bool bMine = (Cl.bSpruce ? 1 : 0) == K.Variant && Cl.bConifer;
				if (!bMine || T.TrunkR < 12.f || T.At.X < X0 || T.At.X >= X0 + CellSize || T.At.Y < Y0 || T.At.Y >= Y0 + CellSize) return;
				const float S = T.TrunkR / 20.f;
				Place(K, KI, T.At.X, T.At.Y, Hash01(KS, (uint32)(T.At.X), (uint32)(T.At.Y)) * 360.f, S, Hash01(KS, 1), 0.f);
			});
			continue;
		}
		if (K.Type == EKindType::Leaf)
		{
			// fallen leaves: how many lie here is the litter field's; a leaf of a species with the chance of its share
			bool bNear = false;
			ForTreesNear(Mid, CellSize * 0.71f + 900.f, [&](const FTreeRef& T, float) { bNear |= GClasses[T.Class].Species == K.Species; });
			if (!bNear) continue;
			const int32 N = FMath::RoundToInt(K.Density * 64.f * FMath::Min(Scale, 2.f));
			for (int32 I = 0; I < N; I++)
			{
				const float X = X0 + Hash01(KS, I, 1) * CellSize, Y = Y0 + Hash01(KS, I, 2) * CellSize;
				const FVector At(X, Y, 0.0);
				float Mine = 0.f, All = 0.f, Amount = 0.f, Decay = 0.f;
				ForTreesNear(At, 900.f, [&](const FTreeRef& T, float D)
				{
					const FTreeClass& Cl = GClasses[T.Class];
					if (Cl.bConifer) return;
					const FVector2f Lf = LeafLitter(Cl, Day);
					const float W = FMath::Exp(-FMath::Square(D / (T.Crown * 1.5f))) * Lf.X;
					All += W;
					Amount += W;
					Decay += W * Lf.Y;
					if (Cl.Species == K.Species) Mine += W;
				});
				if (All < 1e-3f) continue;
				const float Cover = 1.f - FMath::Exp(-Amount * 1.6f);
				if (Hash01(KS, I, 3) > Cover * (Mine / All) * 0.9f) continue;
				// fresh leaves lie loose on top (the wind stirs them); old ones are matted flat
				const float D = Decay / All;
				Place(K, KI, X, Y, Hash01(KS, I, 4) * 360.f, 0.8f + 0.5f * Hash01(KS, I, 5), FMath::Clamp(D + 0.2f * (Hash01(KS, I, 6) - 0.5f), 0.f, 1.f),
				      D < 0.3f ? Hash01(KS, I, 7) : 0.f, (Hash01(KS, I, 8) - 0.5f) * 30.f * (1.f - D), (Hash01(KS, I, 9) - 0.5f) * 30.f * (1.f - D));
			}
			continue;
		}
		if (K.Name == TEXT("PineCone") || K.Name == TEXT("SpruceCone") || K.Name == TEXT("BarkCurl"))
		{
			// under their own trees, thickest round the trunk
			const int32 N = FMath::RoundToInt(K.Density * 64.f * Scale);
			for (int32 I = 0; I < N; I++)
			{
				const float X = X0 + Hash01(KS, I, 1) * CellSize, Y = Y0 + Hash01(KS, I, 2) * CellSize;
				float W = 0.f;
				ForTreesNear(FVector(X, Y, 0.0), 700.f, [&](const FTreeRef& T, float D)
				{
					const FTreeClass& Cl = GClasses[T.Class];
					const bool bMine = K.Name == TEXT("PineCone") ? Cl.bPine : K.Name == TEXT("SpruceCone") ? Cl.bSpruce : Cl.bBirch;
					if (bMine && T.Height > 500.f) W += FMath::Exp(-FMath::Square(D / T.Crown));
				});
				if (Hash01(KS, I, 3) > W * 0.6f) continue;
				Place(K, KI, X, Y, Hash01(KS, I, 4) * 360.f, 0.8f + 0.45f * Hash01(KS, I, 5), Hash01(KS, I, 6), 0.f, 0.f, (Hash01(KS, I, 7) - 0.5f) * 40.f);
			}
			continue;
		}
		// everything else: a density per m2 here from the stand, the light and patches over metres
		const float Wet = Mask ? Mask->GetWetness(Mid) : 0.f;
		const FString Nm = K.Species.IsNone() ? K.Name.ToString() : K.Species.ToString();
		auto Want = [&](float X, float Y) -> float
		{
			const uint32 Ps = HashCombineFast((uint32)Seed, GetTypeHash(Nm));
			const float P = Patch(Ps, X, Y, 900.f) * 0.7f + Patch(Ps + 1, X, Y, 300.f) * 0.3f;
			if (Nm == TEXT("Bilberry")) return (Pine + Spruce * 1.1f + Birch * 0.4f) * SmoothStep(0.38f, 0.62f, P) * Canopy;
			if (Nm == TEXT("Lingonberry")) return Pine * (1.f - Spruce) * SmoothStep(0.45f, 0.7f, P) * Canopy;
			if (Nm == TEXT("Heather")) return Pine * Light * SmoothStep(0.5f, 0.72f, Patch(Ps, X, Y, 1500.f)) * (1.f - Wet);
			if (Nm == TEXT("Bracken")) return (Birch + Broad + Pine * 0.4f) * Light * SmoothStep(0.55f, 0.75f, Patch(Ps, X, Y, 2000.f));
			if (Nm == TEXT("BucklerFern")) return (Spruce + Broad * 0.5f) * (0.3f + Wet) * SmoothStep(0.45f, 0.7f, P);
			if (Nm == TEXT("WoodSorrel")) return (Spruce + Broad * 0.3f) * SmoothStep(0.4f, 0.65f, P);
			if (Nm == TEXT("FeatherMoss")) return (Pine * 0.7f + Spruce + Birch * 0.2f) * (0.25f + 0.75f * SmoothStep(0.3f, 0.6f, P)) * Canopy * 0.5f;
			if (Nm == TEXT("HaircapMoss")) return (Spruce + Pine * 0.4f) * SmoothStep(0.5f, 0.7f, P) * (0.3f + Wet) * Canopy * 0.4f;
			if (Nm == TEXT("ReindeerLichen")) return Pine * (1.f - Spruce) * Light * (1.f - Wet) * SmoothStep(0.55f, 0.75f, P) * 0.5f;
			if (Nm == TEXT("WavyHairGrass")) return (Pine + Birch) * Light * SmoothStep(0.45f, 0.7f, P) * 0.25f;
			if (K.Type == EKindType::Seedling)
			{
				const float Share = Nm == TEXT("ScotsPine") ? Pine : Nm == TEXT("NorwaySpruce") ? Spruce : Nm == TEXT("SilverBirch") ? Birch : Broad + Birch * 0.3f;
				return Share * Light * 1.5f;
			}
			if (Nm.StartsWith(TEXT("Cushion"))) return Canopy * (Spruce + Pine * 0.5f) * (0.5f + Wet);
			if (Nm.StartsWith(TEXT("Log_")) || Nm.StartsWith(TEXT("Stump_")) || Nm.StartsWith(TEXT("Branch_")) || Nm.StartsWith(TEXT("Stick_")))
			{
				const EWood Wd = K.Wood;
				const float Share = Wd == EWood::Birch ? Birch + Broad : Wd == EWood::Spruce ? Spruce : Pine;
				return Canopy * (Nm.StartsWith(TEXT("Stick_")) || Nm.StartsWith(TEXT("Branch_")) ? 1.f : Share * 2.f);
			}
			return Canopy; // stones and boulders: all over the woods
		};
		const float Expect = K.Density * 64.f * Scale;
		const int32 N = FMath::FloorToInt(Expect) + (Hash01(KS, 999) < FMath::Frac(Expect) ? 1 : 0);
		for (int32 I = 0; I < N; I++)
		{
			const float X = X0 + Hash01(KS, I, 1) * CellSize, Y = Y0 + Hash01(KS, I, 2) * CellSize;
			if (Hash01(KS, I, 3) > FMath::Clamp(Want(X, Y), 0.f, 1.f)) continue;
			const float S = FMath::Lerp(K.ScaleMin, K.ScaleMax, Hash01(KS, I, 5));
			const bool bLying = K.Type == EKindType::Debris && K.bAlign;
			Place(K, KI, X, Y, Hash01(KS, I, 4) * 360.f, S, Hash01(KS, I, 6), Hash01(KS, I, 7), 0.f, bLying ? 0.f : (Hash01(KS, I, 8) - 0.5f) * 8.f);
		}
	}
	Cell.Bands |= (uint8)(1 << Band);
}

void AForestFloor::CommitKind(int32 Index)
{
	FKind& K = *Kinds[Index];
	UHierarchicalInstancedStaticMeshComponent* C = Comps[K.Comp];
	if (!K.Mesh)
	{
		return;
	}
	const ULyudWinterSubsystem* Winter = ULyudWinterSubsystem::Get(this);
	const bool bSnow = Winter && Winter->IsReady() && Winter->GetNow().LevelCm > 0.5f;
	TArray<FTransform> Xfs;
	TArray<float> Custom;
	const float Reach2 = FMath::Square(K.Cull + Margin);
	if (K.Type == EKindType::Falling)
	{
		// leaves falling under the deciduous crowns round the camera: the material moves them; here only where and how high
		const float Day = CurrentDay();
		const bool bGuard = CVarFloorGuard.GetValueOnGameThread() > 0;
		int32 N = 0;
		ForTreesNear(Centre, K.Cull, [&](const FTreeRef& T, float D)
		{
			const FTreeClass& Cl = GClasses[T.Class];
			if (Cl.bConifer || N >= 600 || (bGuard && !T.bWild)) return;
			const float Rate = SmoothStep(Cl.DropStart - 10.f, Cl.DropStart + 5.f, Day) * (1.f - SmoothStep(Cl.Bare - 5.f, Cl.Bare + 3.f, Day));
			const int32 Count = FMath::RoundToInt(Rate * 14.f);
			for (int32 I = 0; I < Count; I++)
			{
				const uint32 H = (uint32)(T.At.X * 7.f + T.At.Y * 13.f) + I * 7919;
				const float A = Hash01(H, 1) * 2.f * PI, R = FMath::Sqrt(Hash01(H, 2)) * T.Crown;
				const FVector At(T.At.X + FMath::Cos(A) * R, T.At.Y + FMath::Sin(A) * R, T.At.Z);
				if (bGuard && SiteAt(At) <= 0.f) continue;
				Xfs.Add(FTransform(FRotator(0.f, Hash01(H, 3) * 360.f, 0.f), At, FVector(1.f)));
				Custom.Add(T.Height * (0.45f + 0.4f * Hash01(H, 4)));   // the height it falls from
				Custom.Add(Hash01(H, 5));                                // its phase
				N++;
			}
		});
	}
	else
	{
		for (const TPair<FIntPoint, FCell>& P : Cells)
		{
			const FCell& Cell = P.Value;
			if (!(Cell.Bands & (1 << K.Band))) continue;
			for (const FCellItem& It : Cell.Items)
			{
				if (It.Kind != Index) continue;
				const FVector At(It.Xf.GetLocation());
				if (FVector::DistSquared2D(At, Centre) > Reach2) continue;
				// buried: the snow lies deeper than it stands (a log or a stone a little sooner: its last sliver over the
				// snow would read as a white plank, where a real one is a rounded hump of snow)
				if (bSnow && SnowDepthAt(At) > It.Top * (K.Type == EKindType::Debris ? 0.7f : 0.9f)) continue;
				Xfs.Add(FTransform(It.Xf));
				Custom.Add(It.Custom0);
				Custom.Add(It.Custom1);
			}
		}
	}
	C->ClearInstances();
	if (!Xfs.IsEmpty())
	{
		C->AddInstances(Xfs, false, true, false);
		for (int32 I = 0; I < Xfs.Num(); I++)
		{
			C->SetCustomData(I, MakeArrayView(&Custom[I * 2], 2), false);
		}
		C->MarkRenderStateDirty();
	}
	K.Instances = Xfs.Num();
}

void AForestFloor::RebuildAround(FVector Location)
{
	if (TreeCount == 0 || ForestTreeSum < 0) BuildTreeIndex();
	EnsureKinds();
	Centre = Location;
	for (int32 B = 0; B < 3; B++)
	{
		const float R = BandReach[B] + Margin;
		const int32 N = FMath::CeilToInt(R / CellSize);
		const FIntPoint C0(FMath::FloorToInt((Location.X - GridOrigin.X) / CellSize), FMath::FloorToInt((Location.Y - GridOrigin.Y) / CellSize));
		for (int32 Y = -N; Y <= N; Y++)
		{
			for (int32 X = -N; X <= N; X++)
			{
				const FIntPoint C(C0.X + X, C0.Y + Y);
				const FVector Mid(GridOrigin.X + (C.X + 0.5) * CellSize, GridOrigin.Y + (C.Y + 0.5) * CellSize, 0.0);
				if (FVector::DistSquared2D(Mid, Location) > FMath::Square(R + CellSize)) continue;
				FCell& Cell = Cells.FindOrAdd(C);
				if (!Cell.bTraced) TraceCell(C, Cell);
				if (!(Cell.Bands & (1 << B))) LayCell(C, Cell, B);
			}
		}
	}
	UpdateSeason(CurrentDay(), false);
	for (int32 I = 0; I < Kinds.Num(); I++) CommitKind(I);
	DirtyKinds.Reset();
	UpdateLitterField(Location, CurrentDay(), true);
	LaidDay = CurrentDay();
	bStarted = true;
}

FString AForestFloor::AuditMap(const FString& OutFile, FVector Location, float Radius)
{
	if (TreeCount == 0 || ForestTreeSum < 0) BuildTreeIndex();
	EnsureKinds();
	if (SiteNX <= 0) return TEXT("audit: no walk grid");
	const double T0 = FPlatformTime::Seconds();
	const bool bGuard = CVarFloorGuard.GetValueOnGameThread() > 0;
	// 0 nothing, 1 a piece lies in this 2 m cell, 2 a big one (log, stump, boulder, branch, roots)
	TArray<uint8> Occ;
	Occ.SetNumZeroed(int64(SiteNX) * SiteNY);
	int64 Items = 0, Big = 0, FewTrees = 0;
	int32 CellsLaid = 0;
	const int32 N = FMath::CeilToInt(Radius / CellSize);
	const FIntPoint C0(FMath::FloorToInt((Location.X - GridOrigin.X) / CellSize), FMath::FloorToInt((Location.Y - GridOrigin.Y) / CellSize));
	for (int32 Y = -N; Y <= N; Y++)
	{
		for (int32 X = -N; X <= N; X++)
		{
			const FIntPoint C(C0.X + X, C0.Y + Y);
			const FVector Mid(GridOrigin.X + (C.X + 0.5) * CellSize, GridOrigin.Y + (C.Y + 0.5) * CellSize, 0.0);
			if (FVector::DistSquared2D(Mid, Location) > FMath::Square(Radius)) continue;
			int32 Dummy;
			if (!SiteCell(Mid.X, Mid.Y, Dummy)) continue;
			FCell Cell;
			TraceCell(C, Cell);
			bool bAny = false;
			for (uint8 F : Cell.Floor) bAny |= F > 0;
			if (!bAny) continue;
			for (int32 B = 0; B < 3; B++) LayCell(C, Cell, B);
			CellsLaid++;
			for (const FCellItem& It : Cell.Items)
			{
				const FVector3f At = It.Xf.GetLocation();
				int32 I;
				if (!SiteCell(At.X, At.Y, I)) continue;
				const bool bBig = Kinds[It.Kind]->bBig;
				Occ[I] = FMath::Max<uint8>(Occ[I], bBig ? 2 : 1);
				Items++;
				Big += bBig ? 1 : 0;
				FewTrees += WildTreesNear(At.X, At.Y, bBig ? BigTreeR : SmallTreeR) < (bBig ? BigTrees : SmallTrees) ? 1 : 0;
			}
		}
	}
	// every occupied cell checked by brute force on the walk grid itself and the built footprints (not the site field)
	TMap<FIntPoint, TArray<int32>> BoxBuckets;
	const float Bucket = 5000.f;
	for (int32 I = 0; I < BuiltBoxes.Num(); I++)
	{
		const FBox& B = BuiltBoxes[I];
		for (int32 Y = FMath::FloorToInt((B.Min.Y - KeepOffBuilt) / Bucket); Y <= FMath::FloorToInt((B.Max.Y + KeepOffBuilt) / Bucket); Y++)
			for (int32 X = FMath::FloorToInt((B.Min.X - KeepOffBuilt) / Bucket); X <= FMath::FloorToInt((B.Max.X + KeepOffBuilt) / Bucket); X++)
				BoxBuckets.FindOrAdd(FIntPoint(X, Y)).Add(I);
	}
	int64 OutsideForest = 0, NearPaved = 0, NearBuilt = 0, BigBad = 0;
	const int32 R = FMath::CeilToInt(KeepOffBuilt / SiteCellCm);
	for (int32 Y = 0; Y < SiteNY; Y++)
	{
		for (int32 X = 0; X < SiteNX; X++)
		{
			const int64 I = int64(Y) * SiteNX + X;
			if (!Occ[I]) continue;
			const double Wx = SiteOrigin.X + X * SiteCellCm, Wy = SiteOrigin.Y + Y * SiteCellCm;
			// outside the woods: not a forest cell, nor a steep one next to forest cells
			bool bForest = SiteKind[I] == WalkForest || (SiteKind[I] == SiteBeyond && SiteForest[I]);
			for (int32 J = FMath::Max(Y - 3, 0); J <= FMath::Min(Y + 3, SiteNY - 1) && !bForest && SiteKind[I] == WalkBlocked; J++)
				for (int32 K = FMath::Max(X - 3, 0); K <= FMath::Min(X + 3, SiteNX - 1); K++)
					bForest |= SiteKind[int64(J) * SiteNX + K] == WalkForest;
			bool bPaved = false;
			for (int32 J = FMath::Max(Y - R, 0); J <= FMath::Min(Y + R, SiteNY - 1) && !bPaved; J++)
			{
				for (int32 K = FMath::Max(X - R, 0); K <= FMath::Min(X + R, SiteNX - 1); K++)
				{
					const uint8 Kd = SiteKind[int64(J) * SiteNX + K];
					if ((Kd == WalkPaved || Kd == WalkYard) && FMath::Square((J - Y) * SiteCellCm) + FMath::Square((K - X) * SiteCellCm) < FMath::Square(KeepOffBuilt)) { bPaved = true; break; }
				}
			}
			bool bBuilt = false;
			if (const TArray<int32>* L = BoxBuckets.Find(FIntPoint(FMath::FloorToInt(Wx / Bucket), FMath::FloorToInt(Wy / Bucket))))
			{
				for (int32 BI : *L)
				{
					const FBox& B = BuiltBoxes[BI];
					const double Dx = FMath::Max3(B.Min.X - Wx, 0.0, Wx - B.Max.X), Dy = FMath::Max3(B.Min.Y - Wy, 0.0, Wy - B.Max.Y);
					if (Dx * Dx + Dy * Dy < FMath::Square(KeepOffBuilt - SiteCellCm * 0.71f)) { bBuilt = true; break; }
				}
			}
			OutsideForest += bForest ? 0 : 1;
			NearPaved += bPaved ? 1 : 0;
			NearBuilt += bBuilt ? 1 : 0;
			BigBad += Occ[I] == 2 && (!bForest || bPaved || bBuilt) ? 1 : 0;
		}
	}
	if (!OutFile.IsEmpty())
	{
		TArray<uint8> Out;
		Out.Append((const uint8*)"FFA1", 4);
		const float Hdr[3] = { float(SiteOrigin.X / 100.0), float(SiteOrigin.Y / 100.0), SiteCellCm / 100.f };
		Out.Append((const uint8*)&SiteNX, 4);
		Out.Append((const uint8*)&SiteNY, 4);
		Out.Append((const uint8*)Hdr, sizeof(Hdr));
		Out.Append(Occ);
		FFileHelper::SaveArrayToFile(Out, *OutFile);
	}
	return FString::Printf(TEXT("audit guard %d: %d cells laid, %lld pieces (%lld big) in %.1f s; occupied 2 m cells outside the woods %lld, within 15 m of paving/yard %lld, "
		"within 15 m of a building/station/roof %lld, big pieces in such cells %lld; pieces with too few wild trees about %lld"),
		bGuard ? 1 : 0, CellsLaid, Items, Big, FPlatformTime::Seconds() - T0, OutsideForest, NearPaved, NearBuilt, BigBad, FewTrees);
}

int32 AForestFloor::NumInstances() const
{
	int32 N = 0;
	for (const FKind* K : Kinds) N += K->Instances;
	return N;
}

FString AForestFloor::DescribeState() const
{
	FString S = FString::Printf(TEXT("forest floor: %d trees, %d cells, %d instances, day %.1f, wet %.2f rime %.2f dry %.2f rain %.2f snow %.2f fall %.2f\n"),
		TreeCount, Cells.Num(), NumInstances(), ShownDay, Weather[0], Weather[1], Weather[2], Weather[3], Weather[4], Weather[5]);
	int64 Tris = 0;
	for (const FKind* K : Kinds)
	{
		Tris += (int64)K->Instances * K->Triangles;
		S += FString::Printf(TEXT("  %-26s %6d inst x %5d tris, cull %4.0f m%s%s %s\n"), *K->Name.ToString(), K->Instances, K->Triangles, K->Cull / 100.f,
			K->bShadow ? TEXT(", shadow") : TEXT(""), K->bCollision ? TEXT(", collision") : TEXT(""), *K->Lint);
	}
	S += FString::Printf(TEXT("  LOD0 triangles if all drawn: %.2f M"), Tris / 1e6);
	if (const UTreeBakeCache* Cache = GetWorld() ? GetWorld()->GetSubsystem<UTreeBakeCache>() : nullptr)
	{
		S += TEXT("\n  tree bakes: ") + Cache->DescribeStats();
	}
	return S;
}

// ------------------------------------------------------------------------------------------------------------ weather, litter field

void AForestFloor::UpdateWeather(float DeltaSeconds)
{
	float Target[6] = {};
	if (const ULyudRainFogSubsystem* Rain = ULyudRainFogSubsystem::Get(this))
	{
		const FLyudRainFogWeather W = Rain->ReadWeather();
		Target[0] = W.Wet;
		Target[3] = FMath::Clamp(W.RainRate / 4.f, 0.f, 1.f);
	}
	float TempC = 10.f;
	if (const ULyudWeatherSubsystem* Ws = ULyudWeatherSubsystem::Get(this))
	{
		const FLyudWeatherState St = Ws->GetState();
		Target[1] = FMath::Max(St.Frost, St.Glaze);
		TempC = St.TempC;
		if (Target[0] <= 0.f) Target[0] = St.Wet;
	}
	const ULyudWinterSubsystem* Winter = ULyudWinterSubsystem::Get(this);
	Target[4] = Winter ? FMath::Clamp(Winter->GetNow().LevelCm / 10.f, 0.f, 1.f) : 0.f;
	// drought: a dry spell in warm weather pales and crisps the litter
	Target[2] = (1.f - FMath::Clamp(Target[0] * 3.f, 0.f, 1.f)) * SmoothStep(14.f, 24.f, TempC) * (1.f - Target[4]);
	// leaves let go in gusts: the wind at the scene scales the falling (and how far the loose ones skitter)
	const ULyudWindSubsystem* Wind = ULyudWindSubsystem::Get(this);
	Target[5] = Wind ? FMath::Clamp(Wind->GetMeanSpeed() / 12.f, 0.f, 1.f) : 0.3f;
	if (CVarFloorWet.GetValueOnGameThread() >= 0.f) Target[0] = CVarFloorWet.GetValueOnGameThread();
	if (CVarFloorRime.GetValueOnGameThread() >= 0.f) Target[1] = CVarFloorRime.GetValueOnGameThread();
	if (CVarFloorDry.GetValueOnGameThread() >= 0.f) Target[2] = CVarFloorDry.GetValueOnGameThread();
	const float K = DeltaSeconds <= 0.f ? 1.f : 1.f - FMath::Exp(-DeltaSeconds / 3.f);
	for (int32 I = 0; I < 6; I++) Weather[I] += (Target[I] - Weather[I]) * K;
	if (Collection)
	{
		UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("FloorWeather"), FLinearColor(Weather[0], Weather[1], Weather[2], Weather[3]));
		UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("FloorSnow"), FLinearColor(Weather[4], Weather[5], 0.f, 0.f));
	}
}

void AForestFloor::UpdateLitterField(const FVector& Camera, float Day, bool bForce)
{
	const FVector2D Want(FMath::RoundToDouble(Camera.X / (LitterCell * 16.0)) * LitterCell * 16.0, FMath::RoundToDouble(Camera.Y / (LitterCell * 16.0)) * LitterCell * 16.0);
	if (!bForce && Want == LitterCentre && FMath::Abs(Day - LitterDay) < 0.5f)
	{
		return;
	}
	LitterCentre = Want;
	LitterDay = Day;
	const double X0 = Want.X - LitterN * LitterCell * 0.5, Y0 = Want.Y - LitterN * LitterCell * 0.5;
	const bool bGuard = CVarFloorGuard.GetValueOnGameThread() > 0;
	TArray<FFloat16> Data;
	Data.SetNumUninitialized(LitterN * LitterN * 4);
	for (int32 J = 0; J < LitterN; J++)
	{
		for (int32 I = 0; I < LitterN; I++)
		{
			const double X = X0 + (I + 0.5) * LitterCell, Y = Y0 + (J + 0.5) * LitterCell;
			const FForestLitter L = LitterFrom(FVector(X, Y, 0.0), Day);
			// no litter on the ground in the town, the yards, by the paths: it fades in from 6 to 15 m off them
			const float Site = bGuard ? SiteSoft(X, Y) : 1.f;
			const int32 O = (J * LitterN + I) * 4;
			// R needles, G leaves, B their hue, A their decay
			Data[O] = FFloat16(L.Needles * Site);
			Data[O + 1] = FFloat16(L.Leaves * Site);
			Data[O + 2] = FFloat16(L.Hue);
			Data[O + 3] = FFloat16(L.Decay);
		}
	}
	UploadHalf(LitterRT, LitterN, LitterN, MoveTemp(Data));
	if (Collection)
	{
		UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("LitterRect"),
			FLinearColor((float)X0, (float)Y0, 1.f / (LitterN * LitterCell), 1.f));
	}
}

// ------------------------------------------------------------------------------------------------------------ tick

void AForestFloor::ResetLayout()
{
	Cells.Reset();
	ClearKinds();
	for (UHierarchicalInstancedStaticMeshComponent* C : Comps) C->DestroyComponent();
	Comps.Reset();
	KeepAlive.Reset();
	Centre = FVector(1e12);
	LitterCentre = FVector2D(1e12, 1e12);
	bStarted = false;
}

void AForestFloor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const bool bOn = CVarFloor.GetValueOnGameThread() > 0;
	if (bOn == bHidden)
	{
		bHidden = !bOn;
		for (UHierarchicalInstancedStaticMeshComponent* C : Comps) C->SetVisibility(bOn);
		if (Collection) UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("LitterRect"), FLinearColor(0.f, 0.f, 0.f, bOn ? 1.f : 0.f));
		LitterCentre = FVector2D(1e12, 1e12);
	}
	if (!bOn) return;
	const double Now = GetWorld()->GetTimeSeconds();
	// the forests lay their trees out at their BeginPlay (and the bakes come later): index them once they are all there
	if (Now >= NextTreeCheck)
	{
		NextTreeCheck = Now + 5.0;
		int32 Sum = 0;
		for (TActorIterator<AProceduralForest> It(GetWorld()); It; ++It) Sum += It->NumTrees();
		if (Sum != ForestTreeSum)
		{
			BuildTreeIndex();
			ResetLayout();
		}
	}
	const int32 Guard = CVarFloorGuard.GetValueOnGameThread();
	if (Guard != GuardShown)
	{
		if (GuardShown != MIN_int32) ResetLayout();
		GuardShown = Guard;
	}
	if (TreeCount == 0) return;
	EnsureKinds();
	FVector Camera;
	if (!CameraLocation(Camera)) return;
	const float Day = CurrentDay();
	UpdateWeather(DeltaSeconds);
	UpdateLitterField(Camera, Day, false);
	if (Now >= NextSeason)
	{
		NextSeason = Now + 2.0;
		// the herbs' season and the grass's colours when the day has moved; the seedlings' bakes until they come
		bool bWaiting = false;
		for (const FKind* K : Kinds) bWaiting |= K->Type == EKindType::Seedling && K->BakeVersion < 0;
		if (FMath::Abs(Day - ShownDay) > 0.05f || bWaiting) UpdateSeason(Day, false);
		// the fallen leaves and the falling ones follow the season every few days of it
		if (FMath::Abs(Day - LaidDay) > 3.f)
		{
			LaidDay = Day;
			for (TPair<FIntPoint, FCell>& P : Cells)
			{
				P.Value.Items.RemoveAll([&](const FCellItem& It) { return Kinds[It.Kind]->Band == 0; });
				P.Value.Bands &= ~1;
			}
			for (int32 I = 0; I < Kinds.Num(); I++) if (Kinds[I]->Band == 0) DirtyKinds.AddUnique(I);
		}
		const ULyudWinterSubsystem* Winter = ULyudWinterSubsystem::Get(this);
		const float Snow = Winter && Winter->IsReady() ? Winter->GetNow().LevelCm : 0.f;
		if (FMath::Abs(Snow - LastSnowCm) > 2.f)
		{
			LastSnowCm = Snow;
			for (int32 I = 0; I < Kinds.Num(); I++) DirtyKinds.AddUnique(I);
		}
	}
	// a new layout once the camera has moved on
	if (FVector::DistSquared2D(Camera, Centre) > FMath::Square(Relayout))
	{
		if (!bStarted)
		{
			RebuildAround(Camera);
			return;
		}
		Centre = Camera;
		for (int32 I = 0; I < Kinds.Num(); I++) DirtyKinds.AddUnique(I);
		// drop the cells far behind
		const float Drop2 = FMath::Square(MaxReach + Margin + 2.f * CellSize);
		for (auto It = Cells.CreateIterator(); It; ++It)
		{
			const FVector Mid(GridOrigin.X + (It.Key().X + 0.5) * CellSize, GridOrigin.Y + (It.Key().Y + 0.5) * CellSize, 0.0);
			if (FVector::DistSquared2D(Mid, Camera) > Drop2) It.RemoveCurrent();
		}
	}
	// lay out the cells the new layout needs, nearest band first, within the frame's trace budget
	int32 Budget = TracesPerTick;
	bool bPending = false;
	for (int32 B = 0; B < 3 && Budget > 0; B++)
	{
		const float R = BandReach[B] + Margin;
		const int32 N = FMath::CeilToInt(R / CellSize);
		const FIntPoint C0(FMath::FloorToInt((Centre.X - GridOrigin.X) / CellSize), FMath::FloorToInt((Centre.Y - GridOrigin.Y) / CellSize));
		for (int32 Y = -N; Y <= N && Budget > 0; Y++)
		{
			for (int32 X = -N; X <= N && Budget > 0; X++)
			{
				const FIntPoint C(C0.X + X, C0.Y + Y);
				const FVector Mid(GridOrigin.X + (C.X + 0.5) * CellSize, GridOrigin.Y + (C.Y + 0.5) * CellSize, 0.0);
				if (FVector::DistSquared2D(Mid, Centre) > FMath::Square(R + CellSize)) continue;
				FCell& Cell = Cells.FindOrAdd(C);
				if (!Cell.bTraced)
				{
					TraceCell(C, Cell);
					Budget -= 81;
				}
				if (!(Cell.Bands & (1 << B)))
				{
					LayCell(C, Cell, B);
					Budget -= 20;
					bPending = true;
				}
			}
		}
		if (Budget <= 0) bPending = true;
	}
	// then put them on screen, a kind or two a frame
	if (!bPending)
	{
		for (int32 N = 0; N < 2 && !DirtyKinds.IsEmpty(); N++)
		{
			CommitKind(DirtyKinds[0]);
			DirtyKinds.RemoveAt(0);
		}
	}
	if (CVarFloorDebug.GetValueOnGameThread() > 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 0.f, FColor::Green, DescribeState());
	}
}
