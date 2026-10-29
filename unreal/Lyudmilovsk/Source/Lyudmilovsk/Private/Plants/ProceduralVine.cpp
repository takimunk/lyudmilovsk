#include "Plants/ProceduralVine.h"
#include "Winter/LyudWinter.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Plants/GameCalendar.h"
#include "Plants/PlantMeshes.h"
#include "Plants/PlantModel.h"
#include "StaticMeshAttributes.h"
#include "SceneTypes.h"
#include "StaticMeshResources.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogVine, Log, All);

namespace
{
	using PlantModel::Hash01;

	TAutoConsoleVariable<int32> CVarVines(TEXT("lyud.Vines"), 1, TEXT("Climbing vines on the houses: 1 shown, 0 hidden (A/B benchmarks)."));

	/** Roughly normal, mean 0, sd 1, from a seed and keys. */
	float Gauss(uint32 Seed, uint32 A, uint32 B, uint32 C)
	{
		return (Hash01(Seed, A, B, C) + Hash01(Seed, A, B, C + 7919u) + Hash01(Seed, A, B, C + 104729u) - 1.5f) * 2.f;
	}

	FVector OnPlane(const FVector& V, const FVector& N)
	{
		return V - N * FVector::DotProduct(V, N);
	}

	/** Scene 01's outside sun lights channel 1 only; channel 0 keeps the default lights too (as AProceduralPlant). */
	void OutdoorLighting(UPrimitiveComponent* C)
	{
		C->LightingChannels.bChannel0 = true;
		C->LightingChannels.bChannel1 = true;
		C->SetCanEverAffectNavigation(false);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetMobility(EComponentMobility::Movable);
		// a vine is still once grown: its leaves' flutter (WPO) must not re-render the cached shadow pages every frame
		C->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Static;
	}

	// --- the leaf: a compound leaf of Leaflets leaflets fanned at the end of a long stalk, standing off the wall

	struct FLeafMesh
	{
		TArray<FVector3f> Pos, Nrm, Tan;
		TArray<FVector2f> UV;
		TArray<FIntVector3> Tris;

		int32 Add(const FVector3f& P, const FVector3f& N, const FVector3f& T, const FVector2f& U)
		{
			Nrm.Add(N); Tan.Add(T); UV.Add(U);
			return Pos.Add(P);
		}
	};

	/** One leaflet (m): base at Base, midrib along M, facing Bn; rows of three across (fewer far away). */
	void AddLeaflet(FLeafMesh& D, const FPlantLeafShape& L, float Length, const FVector3f& Base, const FVector3f& M, const FVector3f& Bn, int32 Lod)
	{
		const FVector3f Side = FVector3f::CrossProduct(Bn, M).GetSafeNormal();
		const float W = L.Width * Length;
		const float A = FMath::Clamp(L.WidestAt, 0.05f, 0.95f), Al = 2.2f * A, Be = 2.2f * (1.f - A);
		const float Norm = FMath::Pow(A, Al) * FMath::Pow(1.f - A, Be);
		TArray<float> Ts;
		if (Lod == 0) Ts = { 0.f, 0.18f, 0.4f, A, 0.84f, 1.f };
		else Ts = { 0.f, A, 1.f };
		const bool bMid = Lod < 2;
		const int32 Start = D.Pos.Num(), PerRow = bMid ? 3 : 2;
		for (int32 I = 0; I < Ts.Num(); I++)
		{
			const float T = Ts[I];
			const float F = FMath::Pow(T, Al) * FMath::Pow(1.f - T, Be) / Norm;
			const float HalfW = 0.5f * W * F;
			const FVector3f C = Base + M * (T * Length) - Bn * (0.12f * Length * T * T);
			D.Add((C - Side * HalfW) * 100.f, Bn, Side, FVector2f(0.5f - HalfW / W, 1.f - T));
			if (bMid) D.Add((C + Bn * (L.Fold * HalfW)) * 100.f, Bn, Side, FVector2f(0.5f, 1.f - T));
			D.Add((C + Side * HalfW) * 100.f, Bn, Side, FVector2f(0.5f + HalfW / W, 1.f - T));
			if (I == 0) continue;
			const int32 P0 = Start + PerRow * (I - 1), P1 = Start + PerRow * I;
			for (int32 K = 0; K + 1 < PerRow; K++)
			{
				D.Tris.Add(FIntVector3(P0 + K, P1 + K, P1 + K + 1));
				D.Tris.Add(FIntVector3(P0 + K, P1 + K + 1, P0 + K + 1));
			}
		}
	}

	/**
	 * The vine's leaf cluster in instance space (cm): Z out of the wall, Y down the wall. Each leaf's stalk rises
	 * out of the wall and bends down; its blade faces out and a little up, the leaflets fanned around a point.
	 */
	void BuildCluster(FLeafMesh& D, const FVineSpecies& V, int32 Lod)
	{
		const FPlantLeafShape& L = V.Plant.Leaf;
		const int32 Count = FMath::Clamp(L.PerCluster, 1, 4);
		const int32 Leaflets = FMath::Clamp(V.Leaflets, 1, 7);
		for (int32 Li = 0; Li < Count; Li++)
		{
			const float Scale = Li == 0 ? 1.f : 0.78f;
			const float Yaw = FMath::DegreesToRadians(Li == 0 ? -20.f : 35.f + 25.f * (Li - 1));
			const FVector3f Off = Li == 0 ? FVector3f::ZeroVector : FVector3f(0.035f * Li, 0.05f, 0.f);
			const float Lp = V.PetioleLength * (Li == 0 ? 1.f : 0.7f);
			const FQuat4f Turn(FVector3f::UnitZ(), Yaw);
			const FVector3f Tip = Off + Turn.RotateVector(FVector3f(0.f, 0.4f * Lp, 0.9f * Lp));
			if (Lod == 0)
			{
				// the stalk: a thin strip from the stem out to the blade
				const FVector3f Along = (Tip - Off).GetSafeNormal(), Across = Turn.RotateVector(FVector3f(0.0015f, 0.f, 0.f));
				const FVector3f N = FVector3f::CrossProduct(Across, Along).GetSafeNormal();
				const int32 B = D.Add((Off - Across) * 100.f, N, Along, FVector2f(0.5f, 1.f));
				D.Add((Off + Across) * 100.f, N, Along, FVector2f(0.5f, 1.f));
				D.Add((Tip + Across) * 100.f, N, Along, FVector2f(0.5f, 1.f));
				D.Add((Tip - Across) * 100.f, N, Along, FVector2f(0.5f, 1.f));
				D.Tris.Add(FIntVector3(B, B + 2, B + 1));
				D.Tris.Add(FIntVector3(B, B + 3, B + 2));
			}
			// the blade plane faces out and a little up the wall (-Y)
			const FVector3f Bn = Turn.RotateVector(FVector3f(0.f, -0.35f, 1.f).GetSafeNormal());
			const FVector3f Ax = Turn.RotateVector(FVector3f::XAxisVector);
			const FVector3f Ay = FVector3f::CrossProduct(Bn, Ax).GetSafeNormal();
			for (int32 K = 0; K < Leaflets; K++)
			{
				const float Mid = (Leaflets - 1) * 0.5f;
				const float Phi = Leaflets > 1 ? FMath::DegreesToRadians(72.f) * (K - Mid) / FMath::Max(Mid, 1.f) : 0.f;
				const FVector3f M = (Ay * FMath::Cos(Phi) + Ax * FMath::Sin(Phi)).GetSafeNormal();
				const float Len = L.Length * Scale * (Leaflets > 1 ? 1.f - 0.38f * FMath::Square((K - Mid) / FMath::Max(Mid, 1.f)) : 1.f);
				AddLeaflet(D, L, Len, Tip + M * 0.006f, M, Bn, Lod);
			}
		}
		// every triangle faces the blade's way (Unreal's front face: (P2 - P0) x (P1 - P0))
		for (FIntVector3& T : D.Tris)
		{
			const FVector3f Fn = FVector3f::CrossProduct(D.Pos[T.Z] - D.Pos[T.X], D.Pos[T.Y] - D.Pos[T.X]);
			if (FVector3f::DotProduct(Fn, D.Nrm[T.X]) < 0.f) Swap(T.Y, T.Z);
		}
	}

	void ToDescription(const FLeafMesh& D, FMeshDescription& MD)
	{
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> VTan = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> VSign = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> VUV = Attr.GetVertexInstanceUVs();
		TVertexInstanceAttributesRef<FVector4f> VCol = Attr.GetVertexInstanceColors();
		const FPolygonGroupID PG = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[PG] = TEXT("Leaf");
		TArray<FVertexInstanceID> Inst;
		for (int32 I = 0; I < D.Pos.Num(); I++)
		{
			const FVertexID V = MD.CreateVertex();
			VPos[V] = D.Pos[I];
			const FVertexInstanceID VI = MD.CreateVertexInstance(V);
			const FVector3f N = D.Nrm[I].GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector);
			VNrm[VI] = N;
			VTan[VI] = (D.Tan[I] - N * FVector3f::DotProduct(D.Tan[I], N)).GetSafeNormal(UE_SMALL_NUMBER, FVector3f::XAxisVector);
			VSign[VI] = 1.f;
			VUV.Set(VI, 0, D.UV[I]);
			VCol[VI] = FVector4f(0.f, 0.f, 0.f, 1.f);
			Inst.Add(VI);
		}
		for (const FIntVector3& T : D.Tris) MD.CreateTriangle(PG, { Inst[T.X], Inst[T.Y], Inst[T.Z] });
	}

	UStaticMesh* BuildVineLeaves(UObject* Outer, const FVineSpecies& V)
	{
		FMeshDescription Lods[3];
		for (int32 L = 0; L < 3; L++)
		{
			FLeafMesh D;
			BuildCluster(D, V, L);
			ToDescription(D, Lods[L]);
		}
		// never saved and never copied into a PIE world (see PlantMeshes)
		const FName Name = *FString::Printf(TEXT("SM_VineLeaves_%s"), *V.Name.ToString());
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, MakeUniqueObjectName(Outer, UStaticMesh::StaticClass(), Name), RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Leaf")));
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Mesh->BuildFromMeshDescriptions({ &Lods[0], &Lods[1], &Lods[2] }, Params);
		if (FStaticMeshRenderData* RD = Mesh->GetRenderData())
		{
			// a cluster is ~0.25 m: full leaflets within ~4 m, two-triangle rows to ~14 m, flat diamonds beyond (no shadow)
			RD->ScreenSize[0] = 1.f;
			RD->ScreenSize[1] = 0.06f;
			RD->ScreenSize[2] = 0.018f;
			if (RD->LODResources.Num() > 2)
			{
				for (FStaticMeshSection& Section : RD->LODResources[2].Sections) Section.bCastShadow = false;
			}
		}
		return Mesh;
	}

	void ForEachVine(UWorld* World, TFunctionRef<void(AProceduralVine*)> Fn)
	{
		if (!World) return;
		for (TActorIterator<AProceduralVine> It(World); It; ++It) Fn(*It);
	}

	UWorld* ConsoleWorld()
	{
		for (const FWorldContext& C : GEngine->GetWorldContexts())
		{
			if (C.World() && C.World()->IsGameWorld()) return C.World();
		}
		return nullptr;
	}

	FAutoConsoleCommand CmdShowAt(TEXT("Vine.ShowAt"), TEXT("Vine.ShowAt <age> <day of year>: hold every vine at an age and day (no arguments: back to the calendar)."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const float Age = Args.Num() > 0 ? FCString::Atof(*Args[0]) : -1.f;
			const float Day = Args.Num() > 1 ? FCString::Atof(*Args[1]) : -1.f;
			ForEachVine(ConsoleWorld(), [&](AProceduralVine* V) { V->DevAge = Age; V->DevDay = Day; });
		}));

	FAutoConsoleCommand CmdReport(TEXT("Vine.Report"), TEXT("Logs every vine's state."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			ForEachVine(ConsoleWorld(), [](AProceduralVine* V) { UE_LOG(LogVine, Warning, TEXT("Vine %s: %s"), *V->GetName(), *V->DescribeState()); });
		}));
}

AProceduralVine::AProceduralVine()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Basic(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BarkMat(TEXT("/Game/Plants/M_PlantBark.M_PlantBark"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LeafMat(TEXT("/Game/Plants/M_PlantLeaf.M_PlantLeaf"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	BaseMaterial = Basic.Object;
	BarkMaterialAsset = BarkMat.Object ? BarkMat.Object : Basic.Object;
	LeafMaterialAsset = LeafMat.Object ? LeafMat.Object : Basic.Object;

	// the parts stay where they were built (world space): the actor is only the root's place
	Wood = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wood"));
	Wood->SetupAttachment(RootComponent);
	OutdoorLighting(Wood);
	for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
	{
		UHierarchicalInstancedStaticMeshComponent* L = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(*FString::Printf(TEXT("Leaves%d"), K));
		L->SetupAttachment(RootComponent);
		L->bDisableCollision = true;
		L->NumCustomDataFloats = 1;
		L->SetWorldPositionOffsetDisableDistance(2500); // the leaves flutter near the camera only
		OutdoorLighting(L);
		LeafComponents.Add(L);
	}
	Berries = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Berries"));
	Berries->SetupAttachment(RootComponent);
	Berries->SetStaticMesh(Sphere.Object);
	Berries->bDisableCollision = true;
	Berries->SetCastShadow(false);
	OutdoorLighting(Berries);
}

AProceduralVine::~AProceduralVine() = default;

TArray<FName> AProceduralVine::GetSpeciesNames() const
{
	return VineSpeciesLibrary::Names();
}

const FVineSpecies& AProceduralVine::GetSpecies() const
{
	const FVineSpecies* S = VineSpeciesLibrary::Find(Species);
	return S ? *S : VineSpeciesLibrary::All()[0];
}

float AProceduralVine::GrowthAt(const FVineSpecies& S, float Age, float DayOfYear)
{
	// whole seasons up to this year's 1 January, plus this season's share by the day
	const float YearFrac = FMath::Clamp((DayOfYear - 1.f) / 365.2425f, 0.f, 1.f);
	const float Season = FMath::Clamp((DayOfYear - S.GrowStartDay) / FMath::Max(S.GrowEndDay - S.GrowStartDay, 1.f), 0.f, 1.f);
	return FMath::Max(0.f, Age - YearFrac + Season);
}

float AProceduralVine::GetAge() const
{
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return AgeAtStart;
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

float AProceduralVine::CurrentDay() const
{
	return UGameCalendar::DayOfYear(UGameCalendar::NowIn(GetWorld()));
}

float AProceduralVine::GetGrowth() const
{
	float Age, Day;
	CurrentTarget(Age, Day);
	return TargetGrowth(Age, Day);
}

void AProceduralVine::CurrentTarget(float& OutAge, float& OutDay) const
{
	OutAge = DevAge >= 0.f ? DevAge : GetAge();
	OutDay = DevDay >= 0.f ? DevDay : CurrentDay();
}

float AProceduralVine::TargetGrowth(float Age, float Day) const
{
	// a time-lapse grows straight through the years; the leaves keep the calendar's day
	const bool bDemo = bDemoGrowth && DevAge < 0.f && GetWorld() && GetWorld()->IsGameWorld();
	return bDemo ? Age : GrowthAt(GetSpecies(), Age, Day);
}

// --- growth

void AProceduralVine::ResetGrowth()
{
	Nodes.Reset();
	Shoots.Reset();
	Tips.Reset();
	Taken.Reset();
	Slippery.Reset();
	SimulatedTo = -1.f;
	SimSeconds = 0.0;
	SimTraces = 0;
	TraceCount = 0;
	Wraps = Slides = Hangs = 0;
	ShownG = WoodG = -1.f;
	ShownDay = -1000.f;
}

bool AProceduralVine::TraceWall(const FVector& From, const FVector& To, FHitResult& Hit) const
{
	FCollisionQueryParams Q(SCENE_QUERY_STAT(VineGrow), true, this);
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	for (int32 Try = 0; Try < 4; Try++)
	{
		TraceCount++;
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, From, To, Objects, Q))
		{
			return false;
		}
		const UPrimitiveComponent* C = Hit.GetComponent();
		bool bSlippery = C && Slippery.Contains(C);
		if (C && !bSlippery)
		{
			// glass is too smooth for the adhesive pads: a translucent surface is never held
			const UMaterialInterface* M = C->GetMaterial(0);
			if (M && M->GetBlendMode() != BLEND_Opaque && M->GetBlendMode() != BLEND_Masked)
			{
				Slippery.Add(C);
				bSlippery = true;
			}
		}
		if (!bSlippery)
		{
			return true;
		}
		Q.AddIgnoredComponent(C);
	}
	return false;
}

bool AProceduralVine::Crowded(const FVector& P, int32 Shoot, int32 ParentShoot) const
{
	const int32* Owner = Taken.Find(CellOf(P, GetSpecies().Spacing * 100.f));
	return Owner && *Owner != Shoot && *Owner != ParentShoot;
}

void AProceduralVine::Occupy(const FVector& P, int32 Shoot)
{
	Taken.FindOrAdd(CellOf(P, GetSpecies().Spacing * 100.f), Shoot);
}

AProceduralVine::EMove AProceduralVine::MoveOnSurface(const FVector& P, const FVector& N, const FVector& D, float StepCm, FVector& OutP, FVector& OutN, FVector& OutD, bool& bOutWrapped) const
{
	constexpr float Off = 3.f;
	const float Depth = 8.f + StepCm * 0.3f;
	bOutWrapped = false;
	const FVector A = P + N * Off;
	const FVector B = A + D * StepCm;
	FHitResult Hit;
	// something in the way (an inner corner, a sill, a cornice's underside): carry on over it
	if (TraceWall(A, B + D * 2.f, Hit) && FVector::DotProduct(Hit.ImpactNormal, D) < -0.2f)
	{
		OutP = Hit.ImpactPoint;
		OutN = Hit.ImpactNormal;
		FVector Nd = OnPlane(D, OutN);
		OutD = Nd.SizeSquared() > 0.04f ? Nd.GetSafeNormal() : OnPlane(N, OutN).GetSafeNormal(UE_SMALL_NUMBER, N);
		return EMove::Ok;
	}
	// the same surface, or one bending gently away
	if (TraceWall(B, B - N * (Off + Depth), Hit) && FVector::DotProduct(Hit.ImpactNormal, N) > 0.3f)
	{
		OutP = Hit.ImpactPoint;
		OutN = Hit.ImpactNormal;
		OutD = OnPlane(D, OutN).GetSafeNormal(UE_SMALL_NUMBER, D);
		return EMove::Ok;
	}
	// nothing under the next step. If the wall goes on a little further, this is the edge of a hole (a window,
	// a door): the shoot will slide along the edge instead of crossing it
	for (const float Dist : { 35.f, 70.f, 120.f, 180.f, 260.f })
	{
		const FVector E = A + D * Dist;
		if (TraceWall(E, E - N * (Off + Depth), Hit) && FVector::DotProduct(Hit.ImpactNormal, N) > 0.8f && FMath::Abs(FVector::DotProduct(Hit.ImpactPoint - P, N)) < 6.f)
		{
			return EMove::Opening;
		}
	}
	// an outer corner: the face round it, looking back toward where the shoot came from
	const FVector W = B - N * (Off + 4.f);
	if (TraceWall(W, W - D * (StepCm * 1.5f), Hit) && FVector::DotProduct(Hit.ImpactNormal, D) > 0.4f)
	{
		OutP = Hit.ImpactPoint;
		OutN = Hit.ImpactNormal;
		// round the corner and on, keeping whatever climb it had
		const FVector Want = -N + FVector::UpVector * FMath::Max(0.f, (float)D.Z);
		OutD = OnPlane(Want, OutN).GetSafeNormal(UE_SMALL_NUMBER, -N);
		bOutWrapped = true;
		return EMove::Ok;
	}
	return EMove::Edge;
}

void AProceduralVine::StartGrowth()
{
	ResetGrowth();
	const FVineSpecies& S = GetSpecies();
	BuiltSpecies = Species;
	BuiltSeed = Seed;
	BuiltAt = GetActorTransform();
	const FVector At = GetActorLocation();
	const FVector Out = GetActorForwardVector();
	// the wall in front of the root (its +X faces out of it), half a metre up, then down to the ground at its foot
	RootN = Out;
	RootP = At;
	FHitResult Hit;
	if (TraceWall(At + Out * 60.f + FVector(0, 0, 50.f), At - Out * 120.f + FVector(0, 0, 50.f), Hit))
	{
		RootN = OnPlane(Hit.ImpactNormal, FVector::UpVector).GetSafeNormal(UE_SMALL_NUMBER, Out);
		RootP = Hit.ImpactPoint;
		FHitResult Ground;
		RootP.Z = TraceWall(RootP + RootN * 6.f, RootP + RootN * 6.f - FVector(0, 0, 300.f), Ground) ? Ground.ImpactPoint.Z + 2.f : At.Z;
	}
	FNode& Root = Nodes.AddDefaulted_GetRef();
	Root.P = RootP;
	Root.N = RootN;
	Root.T = 0.f;
	const int32 N = FMath::Max(S.RootShoots, 1);
	for (int32 I = 0; I < N; I++)
	{
		FShoot& Sh = Shoots.AddDefaulted_GetRef();
		Sh.Order = 0;
		Sh.Nodes.Add(0);
		FTip T;
		T.Shoot = Shoots.Num() - 1;
		T.Node = 0;
		const float Fan = N > 1 ? (I - 0.5f * (N - 1)) / (0.5f * (N - 1)) : 0.f;
		T.Dir = FVector::UpVector.RotateAngleAxisRad(FMath::DegreesToRadians(32.f * Fan + 12.f * Gauss(Seed, I, 1, 3)), RootN);
		T.T = 0.04f * I;
		T.MaxLen = S.MaxShootLength.X * (0.75f + 0.25f * Hash01(Seed, I, 2, 3));
		T.NextBranch = S.BranchSpacing * (0.4f + Hash01(Seed, I, 3, 3));
		Tips.HeapPush(T, [](const FTip& A, const FTip& B) { return A.T < B.T; });
		Occupy(RootP, T.Shoot);
	}
}

void AProceduralVine::StepTip(FTip Tip)
{
	const FVineSpecies& S = GetSpecies();
	const auto Push = [this](const FTip& T) { Tips.HeapPush(T, [](const FTip& A, const FTip& B) { return A.T < B.T; }); };
	if (Nodes.Num() >= MaxNodes || Tip.Len >= Tip.MaxLen)
	{
		return;
	}
	const FNode C = Nodes[Tip.Node];
	const FShoot& Sh = Shoots[Tip.Shoot];
	const int32 Order = Sh.Order;
	const int32 ParentShoot = Sh.Parent;
	const float H = (C.P.Z - RootP.Z) / 100.f;
	if (H > S.MaxHeight)
	{
		return;
	}
	const float Speed = S.Speed * FMath::Pow(S.SpeedFalloff, (float)Order) * FMath::Clamp(1.f - FMath::Square(H / S.MaxHeight), 0.15f, 1.f);
	const float StepM = S.Step, StepCm = S.Step * 100.f;
	const float Dt = StepM / FMath::Max(Speed, 0.05f);
	const int32 Key = Nodes.Num();

	FNode Next;
	Next.Shoot = Tip.Shoot;
	Next.Parent = Tip.Node;
	Next.T = Tip.T + Dt;
	FVector NewDir = Tip.Dir;
	if (Tip.bFree)
	{
		// hanging in the air: out and, more and more, down
		NewDir = (Tip.Dir + FVector(0, 0, -S.FreeDroop * StepM)).GetSafeNormal();
		NewDir = NewDir.RotateAngleAxisRad(0.25f * Gauss(Seed, Key, 5, 1), FVector::UpVector);
		FHitResult Hit;
		if (TraceWall(C.P, C.P + NewDir * StepCm, Hit))
		{
			return;
		}
		Next.P = C.P + NewDir * StepCm;
		Next.N = C.N;
		Next.bFree = true;
		Tip.FreeLen += StepM;
	}
	else
	{
		FVector D = OnPlane(Tip.Dir, C.N).GetSafeNormal(UE_SMALL_NUMBER, OnPlane(FVector::UpVector, C.N).GetSafeNormal(UE_SMALL_NUMBER, Tip.Dir));
		// toward the light: up the wall
		const FVector Up = OnPlane(FVector::UpVector, C.N);
		if (Up.Size() > 0.15f)
		{
			const FVector U = Up.GetSafeNormal();
			const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(D, U), -1.f, 1.f));
			const float Pull = S.UpPull * (Order == 0 ? 1.f : S.SideUpPull) * StepM;
			const FVector Axis = FVector::CrossProduct(D, U);
			if (Angle > 1e-3f && Axis.SizeSquared() > 1e-6f)
			{
				D = D.RotateAngleAxisRad(FMath::Min(Angle, Pull), Axis.GetSafeNormal());
			}
		}
		// and wandering
		D = D.RotateAngleAxisRad(S.Wobble * FMath::Sqrt(StepM) * Gauss(Seed, Key, 6, 1), C.N);
		// kept within its reach of the root along the wall
		const FVector FromRoot = OnPlane(C.P - RootP, FVector::UpVector);
		if (FromRoot.Size() > S.MaxReach * 100.f && FVector::DotProduct(D, FromRoot) > 0.f)
		{
			return;
		}
		// wall already covered by another shoot: try to turn aside, give up when boxed in
		const bool bYoung = Tip.Len < 3.f * StepM;
		const float Sign = Hash01(Seed, Key, 7, 1) < 0.5f ? 1.f : -1.f;
		bool bFound = false;
		for (const float Deg : { 0.f, 25.f, -25.f, 50.f, -50.f, 80.f, -80.f })
		{
			const FVector Dc = D.RotateAngleAxisRad(FMath::DegreesToRadians(Deg * Sign), C.N);
			if (!Crowded(C.P + Dc * StepCm, Tip.Shoot, bYoung ? ParentShoot : INDEX_NONE))
			{
				D = Dc;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			return;
		}
		FVector P2, N2, D2;
		bool bWrapped = false;
		EMove Move = MoveOnSurface(C.P, C.N, D, StepCm, P2, N2, D2, bWrapped);
		if (Move == EMove::Opening)
		{
			// the edge of a window or door: run along it, the way that climbs more
			if (++Tip.Deflects > 60)
			{
				return;
			}
			TArray<FVector, TInlineAllocator<6>> Tries;
			for (const float Deg : { 70.f, 100.f, 130.f })
			{
				const FVector L = D.RotateAngleAxisRad(FMath::DegreesToRadians(Deg), C.N), R = D.RotateAngleAxisRad(FMath::DegreesToRadians(-Deg), C.N);
				const bool bLeftFirst = L.Z > R.Z + 0.05f || (FMath::Abs(L.Z - R.Z) <= 0.05f && Sign > 0.f);
				Tries.Add(bLeftFirst ? L : R);
				Tries.Add(bLeftFirst ? R : L);
			}
			Move = EMove::Edge;
			for (const FVector& Dt2 : Tries)
			{
				if (Crowded(C.P + Dt2 * StepCm, Tip.Shoot, bYoung ? ParentShoot : INDEX_NONE))
				{
					continue;
				}
				if (MoveOnSurface(C.P, C.N, Dt2, StepCm, P2, N2, D2, bWrapped) == EMove::Ok)
				{
					Move = EMove::Ok;
					Slides++;
					break;
				}
			}
			if (Move != EMove::Ok)
			{
				return;
			}
		}
		if (Move == EMove::Edge)
		{
			// nothing to hold (a roof edge, the top of a window): it hangs free for a while
			if (H < 1.f || S.FreeLength <= 0.f)
			{
				return;
			}
			Hangs++;
			Next.P = C.P + C.N * 4.f + D * StepCm;
			Next.N = C.N;
			Next.bFree = true;
			Tip.bFree = true;
			NewDir = (D + C.N * 0.6f).GetSafeNormal();
		}
		else
		{
			// not along the ground: a shoot that comes down to it, or out onto it at the root, stops there
			if (N2.Z > 0.6f && (P2.Z - RootP.Z) < 60.f)
			{
				return;
			}
			Wraps += bWrapped ? 1 : 0;
			// a long run over level ground (a ledge top, a roof): it would rather hang over the edge
			Tip.FlatRun = N2.Z > 0.6f ? Tip.FlatRun + StepM : 0.f;
			if (Tip.FlatRun > 1.2f && H > 2.f)
			{
				return;
			}
			Next.P = P2;
			Next.N = N2;
			NewDir = D2;
			Occupy(P2, Tip.Shoot);
		}
	}
	const int32 NewNode = Nodes.Add(Next);
	Shoots[Tip.Shoot].Nodes.Add(NewNode);
	Tip.Node = NewNode;
	Tip.Dir = NewDir;
	Tip.T = Next.T;
	Tip.Len += StepM;
	if (Tip.bFree && Tip.FreeLen >= S.FreeLength)
	{
		return;
	}
	// side shoots from buds along the stem, breaking a season or so later
	if (!Tip.bFree && Tip.Len >= Tip.NextBranch)
	{
		const float Hb = Hash01(Seed, NewNode, 8, 1);
		Tip.NextBranch += S.BranchSpacing * (0.5f + Hash01(Seed, NewNode, 9, 1));
		if (Order < S.MaxOrder && Hb < S.BranchChance)
		{
			const float Side = (Tip.Branches++ % 2 == 0 ? 1.f : -1.f) * (Hash01(Seed, Tip.Shoot, 10, 1) < 0.5f ? 1.f : -1.f);
			// most leave at BranchAngle; some turn right down to fill the wall below
			const float Deg = Hash01(Seed, NewNode, 11, 1) < 0.2f ? 125.f : S.BranchAngle + 15.f * Gauss(Seed, NewNode, 12, 1);
			FShoot& B = Shoots.AddDefaulted_GetRef();
			B.Order = Order + 1;
			B.Parent = Tip.Shoot;
			B.Nodes.Add(NewNode);
			FTip T;
			T.Shoot = Shoots.Num() - 1;
			T.Node = NewNode;
			T.Dir = NewDir.RotateAngleAxisRad(FMath::DegreesToRadians(Deg) * Side, Next.N);
			T.T = Next.T + S.BudDelay * (0.6f + 0.8f * Hash01(Seed, NewNode, 13, 1));
			T.MaxLen = (B.Order == 1 ? S.MaxShootLength.Y : S.MaxShootLength.Z) * (0.5f + 0.5f * Hash01(Seed, NewNode, 14, 1));
			T.NextBranch = S.BranchSpacing * (0.4f + Hash01(Seed, NewNode, 15, 1));
			Push(T);
		}
	}
	Push(Tip);
}

bool AProceduralVine::SimulateUntil(float G, double BudgetSeconds)
{
	const double T0 = FPlatformTime::Seconds();
	const int32 Traces0 = TraceCount;
	bool bDone = true;
	while (!Tips.IsEmpty() && Tips.HeapTop().T <= G)
	{
		if (FPlatformTime::Seconds() - T0 > BudgetSeconds)
		{
			bDone = false;
			break;
		}
		FTip Tip;
		Tips.HeapPop(Tip, [](const FTip& A, const FTip& B) { return A.T < B.T; }, EAllowShrinking::No);
		StepTip(Tip);
	}
	SimSeconds += FPlatformTime::Seconds() - T0;
	SimTraces += TraceCount - Traces0;
	if (bDone)
	{
		SimulatedTo = G;
	}
	return bDone;
}

void AProceduralVine::GrowNow(float Age)
{
	if (BuiltSpecies != Species || BuiltSeed != Seed || Nodes.IsEmpty() || !BuiltAt.Equals(GetActorTransform(), 0.1f))
	{
		StartGrowth();
	}
	float Unused, Day;
	CurrentTarget(Unused, Day);
	const float G = GrowthAt(GetSpecies(), Age, Day);
	SimulateUntil(G, 1e9);
	Apply(G, Day, true);
}

void AProceduralVine::Regrow()
{
	Nodes.Reset();
	BuiltSpecies = NAME_None;
}

// --- drawing

void AProceduralVine::EnsureMeshes()
{
	const FVineSpecies& S = GetSpecies();
	if (!WoodMaterial)
	{
		WoodMaterial = UMaterialInstanceDynamic::Create(BarkMaterialAsset, this);
		Wood->SetMaterial(0, WoodMaterial);
		LeafMaterials.Reset();
		for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
		{
			LeafMaterials.Add(UMaterialInstanceDynamic::Create(LeafMaterialAsset, this));
			LeafComponents[K]->SetMaterial(0, LeafMaterials[K]);
		}
		BerryMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		BerryMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.25f);
		Berries->SetMaterial(0, BerryMaterial);
	}
	if (!LeafMesh || BuiltSpecies != Species)
	{
		UTexture2D* Albedo = nullptr;
		UTexture2D* Normal = nullptr;
		PlantMeshes::BuildBarkTextures(this, S.Plant, (uint32)Seed, Albedo, Normal);
		BarkAlbedo = Albedo;
		BarkNormal = Normal;
		LeafDetail = PlantMeshes::BuildLeafDetail(this, S.Plant.Leaf);
		LeafMesh = BuildVineLeaves(this, S);
		WoodMaterial->SetTextureParameterValue(TEXT("BarkAlbedo"), BarkAlbedo);
		WoodMaterial->SetTextureParameterValue(TEXT("BarkNormal"), BarkNormal);
		WoodMaterial->SetVectorParameterValue(TEXT("Color"), S.Plant.Bark.RidgeColour);
		WoodMaterial->SetVectorParameterValue(TEXT("TwigColour"), S.Plant.Bark.TwigColour);
		WoodMaterial->SetVectorParameterValue(TEXT("BaseRidge"), S.Plant.Bark.BaseRidgeColour);
		WoodMaterial->SetVectorParameterValue(TEXT("BaseFurrow"), S.Plant.Bark.BaseFurrowColour);
		// the stems are glued to the wall: they never sway
		WoodMaterial->SetScalarParameterValue(TEXT("SwayAmp"), 0.f);
		for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
		{
			LeafMaterials[K]->SetTextureParameterValue(TEXT("LeafDetail"), LeafDetail);
			LeafMaterials[K]->SetScalarParameterValue(TEXT("SwayAmp"), S.Plant.Sway);
			LeafMaterials[K]->SetScalarParameterValue(TEXT("Flutter"), 1.f);
			LeafComponents[K]->SetStaticMesh(LeafMesh);
		}
	}
	for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
	{
		LeafComponents[K]->SetCullDistances(0, (int32)LeafCullDistance);
	}
	Berries->SetCullDistances(0, (int32)FMath::Min(LeafCullDistance, 6000.f));
	Wood->SetCachedMaxDrawDistance(FarWood);
	Wood->LDMaxDrawDistance = FarWood;
}

void AProceduralVine::Apply(float G, float Day, bool bWood)
{
	EnsureMeshes();
	const FVineSpecies& S = GetSpecies();
	const FPlantSeason Season = PlantModel::Season(S.Plant, (uint32)Seed, Day);
	const FTransform& Xf = GetActorTransform();
	const int32 Num = Nodes.Num();

	// the pipe model: a stem is as thick as the stem it carries, and older wood is thicker still
	TArray<float> Down, R;
	Down.SetNumZeroed(Num);
	R.SetNumZeroed(Num);
	for (int32 I = Num - 1; I > 0; I--)
	{
		const FNode& Nd = Nodes[I];
		if (Nd.T <= G && Nd.Parent != INDEX_NONE)
		{
			Down[Nd.Parent] += Down[I] + FVector::Dist(Nd.P, Nodes[Nd.Parent].P) / 100.f;
		}
	}
	int32 Live = 0;
	for (int32 I = 0; I < Num; I++)
	{
		R[I] = FMath::Clamp(S.MinRadius + S.RadiusPerSqrtLength * FMath::Sqrt(Down[I]) + S.RingWidth * FMath::Max(0.f, G - Nodes[I].T), S.MinRadius, S.MaxRadius);
		Live += Nodes[I].T <= G ? 1 : 0;
	}
	// a stem's axis sits its own radius off the wall
	auto DrawAt = [&](int32 I) { const FNode& Nd = Nodes[I]; return Nd.bFree ? Nd.P : Nd.P + Nd.N * ((R[I] + S.StickGap) * 100.f); };

	if (bWood || WoodG < 0.f)
	{
		TArray<FPlantSkeleton::FTube> Tubes;
		for (int32 Si = 0; Si < Shoots.Num(); Si++)
		{
			const FShoot& Sh = Shoots[Si];
			FPlantSkeleton::FTube Tube;
			Tube.Order = 1;
			Tube.Key = Si;
			for (int32 K = 0; K < Sh.Nodes.Num(); K++)
			{
				const int32 I = Sh.Nodes[K];
				const FNode& Nd = Nodes[I];
				if (Nd.T <= G)
				{
					Tube.P.Add(FVector3f(Xf.InverseTransformPosition(DrawAt(I)) / 100.0));
					Tube.R.Add(R[I]);
					continue;
				}
				// the growing tip, part of the way to its next step
				if (K > 0)
				{
					const int32 Prev = Sh.Nodes[K - 1];
					const float F = FMath::Clamp((G - FMath::Max(Nodes[Prev].T, 0.f)) / FMath::Max(Nd.T - Nodes[Prev].T, 1e-4f), 0.f, 1.f);
					if (F > 0.05f)
					{
						Tube.P.Add(FVector3f(Xf.InverseTransformPosition(FMath::Lerp(DrawAt(Prev), DrawAt(I), (double)F)) / 100.0));
						Tube.R.Add(S.MinRadius);
					}
				}
				break;
			}
			if (Tube.P.Num() >= 2)
			{
				Tubes.Add(MoveTemp(Tube));
			}
		}
		if (!Tubes.IsEmpty())
		{
			WoodMesh = PlantMeshes::BuildWood(this, WoodMesh, Tubes, S.Plant.Bark, G, (uint32)Seed);
			if (Wood->GetStaticMesh() != WoodMesh)
			{
				Wood->SetStaticMesh(WoodMesh);
			}
			Wood->MarkRenderStateDirty();
		}
		Wood->SetVisibility(!Tubes.IsEmpty());
		WoodG = G;
	}
	WoodMaterial->SetScalarParameterValue(TEXT("Snow"), ULyudWinterSubsystem::SnowOnBranches(GetWorld(), Season.Snow));
	WoodMaterial->SetVectorParameterValue(TEXT("Tint"), FLinearColor::White);

	// leaves on every other step or so, facing out of the wall, their tips hanging down it
	TArray<FTransform> Leaves[FPlantSeason::NumBuckets];
	TArray<float> Sway[FPlantSeason::NumBuckets];
	TArray<FTransform> BerryXf;
	const float FruitSize = S.Plant.FruitSize * Season.FruitScale;
	for (int32 I = 1; I < Num && Season.LeafScale > 0.f; I++)
	{
		const FNode& Nd = Nodes[I];
		if (Nd.T > G || Hash01(Seed, I, 21) > S.LeafChance)
		{
			continue;
		}
		const int32 B = FMath::Min((int32)(Hash01(Seed, I, 22) * FPlantSeason::NumBuckets), FPlantSeason::NumBuckets - 1);
		if (Hash01(Seed, I, 23) < Season.Fallen[B])
		{
			continue;
		}
		// the newest leaves at a shoot's tip are still small
		const float Young = FMath::Clamp((G - Nd.T) / 0.12f, 0.4f, 1.f);
		const float Size = Season.LeafScale * Young * (0.8f + 0.45f * Hash01(Seed, I, 24));
		const FVector Out = Nd.N;
		FVector DownWall = OnPlane(FVector(0, 0, -1), Out);
		if (DownWall.SizeSquared() < 0.04f)
		{
			DownWall = OnPlane(FVector(1, 0, 0), Out);
		}
		DownWall = DownWall.GetSafeNormal().RotateAngleAxisRad(FMath::DegreesToRadians(55.f) * Gauss(Seed, I, 25, 1), Out);
		const FVector Z = (Out + FVector(0, 0, 0.15f) * Hash01(Seed, I, 26)).GetSafeNormal();
		const FQuat Q = FRotationMatrix::MakeFromZY(Z, DownWall).ToQuat();
		const FVector At = DrawAt(I) + OnPlane(FVector(Gauss(Seed, I, 27, 1), Gauss(Seed, I, 28, 1), Gauss(Seed, I, 29, 1)), Out) * 3.0;
		Leaves[B].Add(FTransform(Q, At, FVector(Size)));
		Sway[B].Add(Nd.bFree ? 1.f : 0.3f);
	}
	// berries: clusters on side shoots a year old or more, hanging below the stem
	if (FruitSize > 0.f)
	{
		const int32 PerCluster = FMath::Max(S.Plant.FruitCluster, 1);
		for (int32 I = 1; I < Num; I++)
		{
			const FNode& Nd = Nodes[I];
			if (Nd.T > G - 1.f || Nd.bFree || Shoots[Nd.Shoot].Order == 0 || Hash01(Seed, I, 31) > S.Plant.FruitChance * 0.12f)
			{
				continue;
			}
			// birds take them one by one through the winter
			if (Season.Day > S.Plant.FruitRipeDay + 40.f || Season.Day < S.Plant.FruitSetDay)
			{
				if (Hash01(Seed, I, 32) < 0.5f) continue;
			}
			const FVector Base = DrawAt(I) + Nd.N * 3.0 - FVector(0, 0, 4.0);
			for (int32 K = 0; K < PerCluster; K++)
			{
				const FVector Off(Gauss(Seed, I, 33, K) * 1.4, Gauss(Seed, I, 34, K) * 1.4, -FMath::Abs(Gauss(Seed, I, 35, K)) * 2.5);
				BerryXf.Add(FTransform(FQuat::Identity, Base + OnPlane(Off, Nd.N) + Nd.N * (Hash01(Seed, I, 36, K) * 1.5), FVector(FruitSize * (0.85f + 0.3f * Hash01(Seed, I, 37, K)))));
			}
		}
	}
	int32 NumLeaves = 0;
	for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
	{
		UHierarchicalInstancedStaticMeshComponent* L = LeafComponents[K];
		LeafMaterials[K]->SetVectorParameterValue(TEXT("Color"), Season.Colour[K]);
		L->ClearInstances();
		if (!Leaves[K].IsEmpty())
		{
			L->AddInstances(Leaves[K], false, true, false);
			for (int32 J = 0; J < Sway[K].Num(); J++)
			{
				L->SetCustomDataValue(J, 0, Sway[K][J], false);
			}
		}
		L->MarkRenderStateDirty();
		NumLeaves += Leaves[K].Num();
	}
	BerryMaterial->SetVectorParameterValue(TEXT("Color"), Season.FruitColour);
	Berries->ClearInstances();
	if (!BerryXf.IsEmpty())
	{
		Berries->AddInstances(BerryXf, false, true, false);
	}
	LastLeaves = NumLeaves;
	LastBerries = BerryXf.Num();
	LastLiveNodes = Live;
	LastArea = Taken.Num() * FMath::Square(S.Spacing);
	ShownG = G;
	ShownDay = Day;
}

FString AProceduralVine::DescribeState() const
{
	float Age, Day;
	CurrentTarget(Age, Day);
	return FString::Printf(TEXT("%s age %.2f day %.1f growth %.2f (simulated to %.2f): %d/%d steps, %d shoots, %d tips left, %d leaf clusters, %d berries, ~%.1f m2 of wall, wraps %d slides %d hangs %d; sim %.1f ms, %d traces"),
		*Species.ToString(), Age, Day, ShownG, SimulatedTo, LastLiveNodes, Nodes.Num(), Shoots.Num(), Tips.Num(), LastLeaves, LastBerries, LastArea, Wraps, Slides, Hangs, SimSeconds * 1000.0, SimTraces);
}

void AProceduralVine::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (GetWorld() && !GetWorld()->IsGameWorld() && !HasAnyFlags(RF_ClassDefaultObject))
	{
		Regrow();
		GrowNow(AgeAtStart);
	}
}

void AProceduralVine::BeginPlay()
{
	Super::BeginPlay();
	Regrow();
	SinceUpdate = 0.f;
}

void AProceduralVine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// lyud.Vines hides and shows them (only on a change: lyud.Academ hides them with their house too)
	const bool bShow = CVarVines.GetValueOnGameThread() != 0;
	if (bShow != bShownByCVar)
	{
		SetActorHiddenInGame(!bShow);
		bShownByCVar = bShow;
	}
	if (!bShow)
	{
		return;
	}
	if (BuiltSpecies != Species || BuiltSeed != Seed || Nodes.IsEmpty() || !BuiltAt.Equals(GetActorTransform(), 0.1f))
	{
		StartGrowth();
	}
	float Age, Day;
	CurrentTarget(Age, Day);
	const float G = TargetGrowth(Age, Day);
	if (SimulatedTo < G)
	{
		// a few milliseconds of wall-tracing a frame until it has caught up with its age
		const bool bGame = GetWorld()->IsGameWorld();
		if (!SimulateUntil(G, bGame ? TraceBudgetMs / 1000.0 : 1e9))
		{
			return;
		}
	}
	SinceUpdate += DeltaSeconds;
	// the vine grows a few cm a day: re-shape only when that shows; leaves when the day moves on
	const bool bWood = FMath::Abs(G - WoodG) > 0.004f && (SinceUpdate > 0.5f || WoodG < 0.f);
	if (bWood || FMath::Abs(Day - ShownDay) > 0.5f || ShownG < 0.f)
	{
		Apply(G, Day, bWood);
		SinceUpdate = 0.f;
	}
}
