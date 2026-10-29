#include "Sea/LyudSea.h"

#include "Components/BrushComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PhysicsVolume.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "Sea/LyudSeabed.h"
#include "Sea/Tide.h"
#include "Plants/GameCalendar.h"
#include "Sky/LyudSky.h"
#include "StaticMeshAttributes.h"
#include "UObject/ConstructorHelpers.h"
#include "../Weather/WaveModel.h"
#include "Weather/LyudWeather.h"
#include "Weather/LyudWind.h"

namespace
{
	TAutoConsoleVariable<int32> CVarSea(TEXT("lyud.Sea"), 1, TEXT("0: hide the sea (surface, seabed, underwater), for A/B benchmarks."));
	TAutoConsoleVariable<int32> CVarSeaQuality(TEXT("lyud.Sea.Quality"), -1,
		TEXT("Sea detail: 0 phone (64 x ~100 mesh, 20 + 16 wave trains), 1 medium, 2 desktop (128 x ~200, 64 + 48); -1 by platform."));
	TAutoConsoleVariable<int32> CVarSeaReplacePlane(TEXT("lyud.Sea.ReplacePlane"), 1, TEXT("1: in play, swap an old flat sea plane (M_BalticSea) for an ALyudSea."));
	TAutoConsoleVariable<float> CVarSeaDebugFoam(TEXT("lyud.Sea.DebugFoam"), 0.f, TEXT("Make the foam glow by this much, to see where it is."));
	TAutoConsoleVariable<float> CVarSeaCapillary(TEXT("lyud.Sea.Capillary"), -1.f,
		TEXT("The capillary ripples' slope per train (tuning: the sun's glitter; < 0 = the material's own)."));
	TAutoConsoleVariable<int32> CVarSeaDebug(TEXT("lyud.Sea.Debug"), 0, TEXT("1: print the sea state on screen."));
	TAutoConsoleVariable<float> CVarTideOverride(TEXT("lyud.TideOverride"), -999.f,
		TEXT("Pin the tide: water level in metres over the datum (0 = spring high water, -4.5 = spring low); -999 or NaN = follow the sky."));
	TAutoConsoleVariable<float> CVarSeaMeanLevel(TEXT("lyud.Sea.MeanLevel"), -9999.f,
		TEXT("Force the mean sea level's offset, cm (as SetLevelOffset, at once; below -9000: what the game set)."));
	TAutoConsoleVariable<float> CVarSeaLookSeconds(TEXT("lyud.Sea.LookSeconds"), 120.f,
		TEXT("How many seconds the sea's colour takes to follow the weather and the light (0 = at once)."));
	TAutoConsoleVariable<float> CVarTideSpeed(TEXT("lyud.TideSpeed"), 1.f,
		TEXT("The tide's clock runs this many times faster than the sky's (1 = with it; 3600 = an hour a second)."));

	const TCHAR* CollectionPath = TEXT("/Game/Weather/MPC_LyudWeather.MPC_LyudWeather");
	constexpr int32 TableHalf = 64;   // columns per train set in the wave table (geometry 0..63, detail 64..127)

	struct FQuality
	{
		int32 Segments;
		float MinStepCm;
		int32 Geo, Detail;
	};
	const FQuality Qualities[3] = { { 64, 60.f, 20, 16 }, { 96, 40.f, 40, 32 }, { 128, 25.f, 64, 48 } };

	TWeakObjectPtr<ALyudSea> GSea;
}

ALyudSea::ALyudSea()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);

	Surface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Surface"));
	Surface->SetupAttachment(RootComponent);
	Surface->SetMobility(EComponentMobility::Movable);
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Surface->SetCastShadow(false);
	Surface->bAffectDistanceFieldLighting = false;
	Surface->bAffectDynamicIndirectLighting = false;
	Surface->SetGenerateOverlapEvents(false);
	// outdoors: the sun and the moon light channel 1 only (ALyudSky)
	Surface->LightingChannels.bChannel0 = true;
	Surface->LightingChannels.bChannel1 = true;
	Surface->bVisibleInRayTracing = false;

	Underwater = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Underwater"));
	Underwater->SetupAttachment(RootComponent);
	Underwater->bUnbound = true;
	Underwater->Priority = 900.f;
	Underwater->BlendWeight = 0.f;

	Seabed = CreateDefaultSubobject<ULyudSeabedComponent>(TEXT("Seabed"));
	Seabed->SetupAttachment(RootComponent);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SeaMat(TEXT("/Game/Sea/M_LyudSea.M_LyudSea"));
	if (SeaMat.Succeeded())
	{
		SeaMaterial = SeaMat.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnderMat(TEXT("/Game/Sea/PP_LyudUnderwater.PP_LyudUnderwater"));
	if (UnderMat.Succeeded())
	{
		UnderwaterMaterial = UnderMat.Object;
	}
	Field = MakePimpl<LyudWeather::FWaveField>();
}

ALyudSea::~ALyudSea() = default;

ALyudSea* ALyudSea::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	if (GSea.IsValid() && GSea->GetWorld() == World)
	{
		return GSea.Get();
	}
	TActorIterator<ALyudSea> It(World);
	GSea = It ? *It : nullptr;
	return GSea.Get();
}

void ALyudSea::ApplyQuality(int32 Quality)
{
	if (Quality < 0)
	{
#if PLATFORM_ANDROID || PLATFORM_IOS
		Quality = 0;
#else
		Quality = 2;
#endif
	}
	Quality = FMath::Clamp(Quality, 0, 2);
	if (Quality == AppliedQuality && SurfaceMesh)
	{
		return;
	}
	AppliedQuality = Quality;
	const FQuality& Q = Qualities[Quality];
	Segments = Q.Segments;
	MinStepCm = Q.MinStepCm;
	LyudWeather::FWaveParams P;
	P.GeoCount = Q.Geo;
	P.DetailCount = Q.Detail;
	P.FetchKm = FetchKm;
	P.Choppiness = Choppiness;
	Field->Init(P);
	AppliedFetch = FetchKm;
	AppliedChop = Choppiness;
	AppliedWind = -1.f;
	BuildSurfaceMesh();
	UpdateWaves(true);
}

void ALyudSea::BuildSurfaceMesh()
{
	// rings around the camera: a fine core of MinStep, then each ring a segment's width farther out, so
	// the cells stay square and grow with distance like the pixels they cover
	TArray<float> Radii;
	for (float R = MinStepCm; R < OuterRadius;)
	{
		Radii.Add(R);
		R += FMath::Max(MinStepCm, R * 2.f * PI / Segments);
	}
	Radii.Add(OuterRadius);

	FMeshDescription MD;
	FStaticMeshAttributes Attr(MD);
	Attr.Register();
	TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> VTan = Attr.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> VSign = Attr.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector2f> VUV = Attr.GetVertexInstanceUVs();
	VUV.SetNumChannels(1);
	const FPolygonGroupID PG = MD.CreatePolygonGroup();
	Attr.GetPolygonGroupMaterialSlotNames()[PG] = TEXT("Sea");
	TArray<FVertexInstanceID> Inst;
	Inst.Reserve(1 + Radii.Num() * Segments);
	auto Add = [&](const FVector3f& P)
	{
		const FVertexID V = MD.CreateVertex();
		VPos[V] = P;
		const FVertexInstanceID VI = MD.CreateVertexInstance(V);
		VNrm[VI] = FVector3f::UpVector;
		VTan[VI] = FVector3f::XAxisVector;
		VSign[VI] = 1.f;
		VUV.Set(VI, 0, FVector2f(P.X, P.Y) * 0.01f);
		Inst.Add(VI);
	};
	Add(FVector3f::ZeroVector);
	for (float R : Radii)
	{
		for (int32 I = 0; I < Segments; I++)
		{
			const float A = 2.f * PI * I / Segments;
			Add(FVector3f(R * FMath::Cos(A), R * FMath::Sin(A), 0.f));
		}
	}
	// Unreal's front face is the one where (P2 - P0) x (P1 - P0) points out: up here
	for (int32 I = 0; I < Segments; I++)
	{
		const int32 B = 1 + I, C = 1 + (I + 1) % Segments;
		MD.CreateTriangle(PG, { Inst[0], Inst[C], Inst[B] });
	}
	for (int32 J = 0; J + 1 < Radii.Num(); J++)
	{
		for (int32 I = 0; I < Segments; I++)
		{
			const int32 I1 = (I + 1) % Segments;
			const int32 A = 1 + J * Segments + I, B = 1 + J * Segments + I1;
			const int32 C = 1 + (J + 1) * Segments + I, D = 1 + (J + 1) * Segments + I1;
			MD.CreateTriangle(PG, { Inst[A], Inst[B], Inst[C] });
			MD.CreateTriangle(PG, { Inst[B], Inst[D], Inst[C] });
		}
	}
	UStaticMesh* Mesh = NewObject<UStaticMesh>(this, MakeUniqueObjectName(this, UStaticMesh::StaticClass(), TEXT("SM_LyudSeaRings")),
		RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
	Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Sea")));
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bMarkPackageDirty = false;
	Params.bBuildSimpleCollision = false;
	Params.bCommitMeshDescription = false;
	Params.bFastBuild = true;
	Params.bAllowCpuAccess = false;
	Mesh->BuildFromMeshDescriptions({ &MD }, Params);
	// the waves lift and sink it a few metres, never out of these bounds
	Mesh->SetExtendedBounds(FBoxSphereBounds(FBox(FVector(-OuterRadius, -OuterRadius, -3000.0), FVector(OuterRadius, OuterRadius, 3000.0))));
	SurfaceMesh = Mesh;
	Surface->SetStaticMesh(Mesh);
	EnsureMaterial();
	UE_LOG(LogTemp, Log, TEXT("LyudSea: surface %d rings x %d segments, %d vertices"), Radii.Num(), Segments, 1 + Radii.Num() * Segments);
}

void ALyudSea::EnsureMaterial()
{
	if (!SeaMaterial)
	{
		return;
	}
	if (!SeaMID || SeaMID->Parent != SeaMaterial)
	{
		SeaMID = UMaterialInstanceDynamic::Create(SeaMaterial, this);
	}
	Surface->SetMaterial(0, SeaMID);
	if (WaveTable)
	{
		SeaMID->SetTextureParameterValue(TEXT("WaveTable"), WaveTable);
	}
	if (DepthTex && bDepthUploaded)
	{
		SeaMID->SetTextureParameterValue(TEXT("DepthMap"), DepthTex);
	}
	SeaMID->SetScalarParameterValue(TEXT("LandSideX"), LandSideX * 0.01f);
	SeaMID->SetScalarParameterValue(TEXT("OpenSeaDepth"), OpenSeaDepthM);
	SeaMID->SetScalarParameterValue(TEXT("Tide"), TideCm * 0.01f);
	if (UnderwaterMaterial && !UnderMID)
	{
		UnderMID = UMaterialInstanceDynamic::Create(UnderwaterMaterial, this);
		Underwater->Settings.WeightedBlendables.Array.Reset();
		Underwater->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, UnderMID));
	}
}

void ALyudSea::UpdateWaves(bool bForce)
{
	FVector Wind(0.0, 6.0, 0.0);
	if (const ULyudWindSubsystem* W = ULyudWindSubsystem::Get(this))
	{
		Wind = W->GetSeaStateWind();
	}
	Wind *= WindScale;
	const float Speed = Wind.Size2D();
	const FVector2D Dir = FVector2D(Wind).GetSafeNormal();
	if (FetchKm != AppliedFetch || Choppiness != AppliedChop)
	{
		LyudWeather::FWaveParams P = Field->Params;
		P.FetchKm = FetchKm;
		P.Choppiness = Choppiness;
		Field->Params = P;
		AppliedFetch = FetchKm;
		AppliedChop = Choppiness;
		bForce = true;
	}
	if (!bForce && FMath::Abs(Speed - AppliedWind) < 0.02f && FVector2D::DotProduct(Dir, AppliedDir) > 0.9999f)
	{
		return;
	}
	AppliedWind = Speed;
	AppliedDir = Dir;
	Field->SetWind(Speed, Dir.X, Dir.Y);
	UploadWaveTable();
}

void ALyudSea::UploadWaveTable()
{
	if (!WaveTable)
	{
		WaveTable = UTexture2D::CreateTransient(TableHalf * 2, 2, PF_A32B32G32R32F, TEXT("T_LyudWaveTable"));
		WaveTable->SRGB = false;
		WaveTable->Filter = TF_Nearest;
		WaveTable->NeverStream = true;
		WaveTable->AddressX = TA_Clamp;
		WaveTable->AddressY = TA_Clamp;
		WaveTable->UpdateResource();
		if (SeaMID)
		{
			SeaMID->SetTextureParameterValue(TEXT("WaveTable"), WaveTable);
		}
	}
	const int32 W = TableHalf * 2;
	FLinearColor* Data = new FLinearColor[W * 2];
	FMemory::Memzero(Data, sizeof(FLinearColor) * W * 2);
	auto Put = [&](const std::vector<LyudWeather::FWaveTrain>& Set, int32 Col0)
	{
		for (int32 I = 0; I < FMath::Min<int32>(Set.size(), TableHalf); I++)
		{
			const LyudWeather::FWaveTrain& T = Set[I];
			Data[Col0 + I] = FLinearColor(T.DirX, T.DirY, T.K, T.Amp);
			Data[W + Col0 + I] = FLinearColor(T.Omega, T.Phase, T.Lambda, 0.f);
		}
	};
	Put(Field->Geo, 0);
	Put(Field->Detail, TableHalf);
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, W, 2);
	WaveTable->UpdateTextureRegions(0, 1, Region, W * sizeof(FLinearColor), sizeof(FLinearColor), (uint8*)Data,
		[](uint8* Src, const FUpdateTextureRegion2D* Regions) { delete[] (FLinearColor*)Src; delete Regions; });

	if (SeaMID)
	{
		SeaMID->SetVectorParameterValue(TEXT("SeaParams"), FLinearColor(Field->Chop, Field->Hs, (float)Field->Geo.size(), (float)Field->Detail.size()));
	}
}

void ALyudSea::StepDepthMap(int32 Budget)
{
	UWorld* World = GetWorld();
	if (!World || IsDepthReady())
	{
		return;
	}
	TArray<ALandscapeProxy*> Proxies;
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		Proxies.Add(*It);
	}
	if (!bDepthStarted)
	{
		FBox Box(ForceInit);
		for (ALandscapeProxy* P : Proxies)
		{
			Box += P->GetComponentsBoundingBox(true);
		}
		// other ground the sea stands on (the railway's bank near the coast, its island: components tagged LyudSeaBed)
		if (Box.IsValid)
		{
			for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
			{
				if (It->GetWorld() == World && It->ComponentHasTag(TEXT("LyudSeaBed")) && It->IsRegistered())
				{
					Box += It->Bounds.GetBox();
				}
			}
		}
		if (!Box.IsValid)
		{
			return;
		}
		bDepthStarted = true;
		DepthMin = FVector2D(Box.Min) * 0.01;
		const FVector2D Size = FVector2D(Box.Max - Box.Min) * 0.01;
		DepthW = FMath::Clamp(FMath::CeilToInt(Size.X / DepthCellM), 2, 1024);
		DepthH = FMath::Clamp(FMath::CeilToInt(Size.Y / DepthCellM), 2, 1024);
		DepthSize = Size;
		Depth.Init(OpenSeaDepthM, DepthW * DepthH);
		DepthRow = 0;
	}
	const float SeaZ = GetActorLocation().Z;
	const double StartTime = FPlatformTime::Seconds();
	int32 Done = 0;
	while (DepthRow < DepthH && Done < Budget)
	{
		const double Y = DepthMin.Y + (DepthRow + 0.5) * DepthSize.Y / DepthH;
		for (int32 X = 0; X < DepthW; X++)
		{
			const FVector P((DepthMin.X + (X + 0.5) * DepthSize.X / DepthW) * 100.0, Y * 100.0, 0.0);
			bool bOk;
			const float Ground = GroundZ(P.X, P.Y, bOk);
			Depth[DepthRow * DepthW + X] = bOk ? (SeaZ - Ground) * 0.01f : OpenSeaDepthM;
		}
		Done += DepthW;
		DepthRow++;
	}
	if (IsDepthReady())
	{
		UploadDepthMap();
		UE_LOG(LogTemp, Log, TEXT("LyudSea: depth map %dx%d over %.0fx%.0f m read (last slice %.1f ms)"), DepthW, DepthH, DepthSize.X, DepthSize.Y,
			(FPlatformTime::Seconds() - StartTime) * 1000.0);
	}
}

float ALyudSea::GroundZ(double X, double Y, bool& bOk) const
{
	// the ground only (the landscape, and what is tagged LyudSeaBed: the railway's bank and island): a pier, a boulder or a tree is not the bed
	bOk = false;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}
	const float SeaZ = GetActorLocation().Z;
	TArray<FHitResult> Hits;
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LyudSeaGround), true);
	World->LineTraceMultiByObjectType(Hits, FVector(X, Y, SeaZ + 15000.0), FVector(X, Y, SeaZ - 8000.0), Objects, Params);
	for (const FHitResult& H : Hits)
	{
		if (H.GetActor() && (H.GetActor()->IsA<ALandscapeProxy>() || (H.GetComponent() && H.GetComponent()->ComponentHasTag(TEXT("LyudSeaBed")))))
		{
			bOk = true;
			return float(H.ImpactPoint.Z);
		}
	}
	return 0.f;
}

void ALyudSea::FindPools()
{
	// Priority flood (Barnes et al. 2014) from the water that never leaves (deeper than the lowest springs) and
	// the map's edges: each cell's spill level is the lowest rim it would have to climb over to reach the sea.
	// A cell under its spill level is in a hollow the ebb cannot drain; it keeps that much water (PoolW).
	const int32 N = DepthW * DepthH;
	PoolW.Init(0.f, N);
	TArray<float> Fill;
	Fill.SetNumUninitialized(N);
	TArray<uint8> Done;
	Done.Init(0, N);
	struct FCell { float H; int32 I; };
	struct FLower { bool operator()(const FCell& A, const FCell& B) const { return A.H < B.H; } };
	TArray<FCell> Heap;
	const float Always = -float(LyudTide::SpringLowM) + 0.3f;
	for (int32 I = 0; I < N; I++)
	{
		const int32 X = I % DepthW, Y = I / DepthW;
		if (Depth[I] > Always || X == 0 || Y == 0 || X == DepthW - 1 || Y == DepthH - 1)
		{
			Fill[I] = -Depth[I];
			Done[I] = 1;
			Heap.HeapPush({ Fill[I], I }, FLower());
		}
	}
	while (Heap.Num())
	{
		FCell C;
		Heap.HeapPop(C, FLower(), EAllowShrinking::No);
		const int32 X = C.I % DepthW, Y = C.I / DepthW;
		static const int32 DX[4] = { 1, -1, 0, 0 }, DY[4] = { 0, 0, 1, -1 };
		for (int32 K = 0; K < 4; K++)
		{
			const int32 NX = X + DX[K], NY = Y + DY[K];
			if (NX < 0 || NY < 0 || NX >= DepthW || NY >= DepthH)
			{
				continue;
			}
			const int32 J = NY * DepthW + NX;
			if (Done[J])
			{
				continue;
			}
			Done[J] = 1;
			Fill[J] = FMath::Max(-Depth[J], C.H);
			Heap.HeapPush({ Fill[J], J }, FLower());
		}
	}
	// only on the tidal flat: the hollow's water under the datum, not ponds on dry land
	int32 Pools = 0;
	for (int32 I = 0; I < N; I++)
	{
		const float W = Fill[I] + Depth[I];
		if (W > 0.02f && Fill[I] < -0.05f && Depth[I] < Always)
		{
			PoolW[I] = W;
			Pools++;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("LyudSea: %d depth cells hold tide pools at low water"), Pools);
}

void ALyudSea::UploadDepthMap()
{
	FindPools();
	DepthTex = UTexture2D::CreateTransient(DepthW, DepthH, PF_G16R16F, TEXT("T_LyudSeaDepth"));
	DepthTex->SRGB = false;
	DepthTex->Filter = TF_Bilinear;
	DepthTex->NeverStream = true;
	DepthTex->AddressX = TA_Clamp;
	DepthTex->AddressY = TA_Clamp;
	DepthTex->UpdateResource();
	// r = depth under the datum, g = water a hollow keeps at low tide (tools/sea/sea_hlsl.py Lyud.Water)
	FFloat16* Data = new FFloat16[DepthW * DepthH * 2];
	for (int32 I = 0; I < DepthW * DepthH; I++)
	{
		Data[I * 2] = FFloat16(Depth[I]);
		Data[I * 2 + 1] = FFloat16(PoolW[I]);
	}
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, DepthW, DepthH);
	DepthTex->UpdateTextureRegions(0, 1, Region, DepthW * sizeof(FFloat16) * 2, sizeof(FFloat16) * 2, (uint8*)Data,
		[](uint8* Src, const FUpdateTextureRegion2D* Regions) { delete[] (FFloat16*)Src; delete Regions; });
	bDepthUploaded = true;
	if (SeaMID)
	{
		SeaMID->SetTextureParameterValue(TEXT("DepthMap"), DepthTex);
		SeaMID->SetVectorParameterValue(TEXT("DepthRect"), FLinearColor(DepthMin.X, DepthMin.Y, 1.0 / DepthSize.X, 1.0 / DepthSize.Y));
	}
	// the ground draws the pools the ebb leaves (M_Ground: standing water in the hollows over the sea's level)
	for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
	{
		It->SetLandscapeMaterialTextureParameterValue(TEXT("TidePools"), DepthTex);
		It->SetLandscapeMaterialVectorParameterValue(TEXT("PoolRect"), FLinearColor(DepthMin.X, DepthMin.Y, 1.0 / DepthSize.X, 1.0 / DepthSize.Y));
	}
}

float ALyudSea::DepthAtM(double XM, double YM) const
{
	if (XM * 100.0 < LandSideX)
	{
		return -10.f;
	}
	if (!IsDepthReady())
	{
		return OpenSeaDepthM;
	}
	// bilinear between cell centres, the same as the material's sampler
	const double U = (XM - DepthMin.X) / DepthSize.X * DepthW - 0.5, V = (YM - DepthMin.Y) / DepthSize.Y * DepthH - 0.5;
	if (U < -0.5 || V < -0.5 || U > DepthW - 0.5 || V > DepthH - 0.5)
	{
		return OpenSeaDepthM;
	}
	const int32 X0 = FMath::Clamp(FMath::FloorToInt(U), 0, DepthW - 1), Y0 = FMath::Clamp(FMath::FloorToInt(V), 0, DepthH - 1);
	const int32 X1 = FMath::Min(X0 + 1, DepthW - 1), Y1 = FMath::Min(Y0 + 1, DepthH - 1);
	const float FX = FMath::Clamp(float(U - X0), 0.f, 1.f), FY = FMath::Clamp(float(V - Y0), 0.f, 1.f);
	const float A = FMath::Lerp(Depth[Y0 * DepthW + X0], Depth[Y0 * DepthW + X1], FX);
	const float B = FMath::Lerp(Depth[Y1 * DepthW + X0], Depth[Y1 * DepthW + X1], FX);
	return FMath::Lerp(A, B, FY);
}

float ALyudSea::DepthAt(const FVector& Location) const
{
	return WaterDepthAtM(Location.X * 0.01, Location.Y * 0.01) * 100.f;
}

float ALyudSea::DepthAtLevel(const FVector& Location, float WaterZ) const
{
	return DepthAtM(Location.X * 0.01, Location.Y * 0.01) * 100.f + (WaterZ - DatumZ());
}

float ALyudSea::LowWaterZ() const
{
	return DatumZ() + float(LyudTide::SpringLowM) * 100.f;
}

void ALyudSea::UpdateTide(float DeltaSeconds)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Speed = CVarTideSpeed.GetValueOnGameThread();
	if (Speed != 1.f)
	{
		TideExtraHours += double(Speed - 1.f) * DeltaSeconds / 3600.0;
	}
	TideAge += DeltaSeconds;
	if (TideAge < 0.2f && Speed == 1.f)
	{
		return;
	}
	// the mean level eases to its target (winter's lower sea; lyud.Sea.MeanLevel pins it)
	const float MeanPin = CVarSeaMeanLevel.GetValueOnGameThread();
	if (MeanPin > -9000.f)
	{
		MeanOffsetNowCm = MeanOffsetTargetCm = MeanPin;
	}
	else
	{
		MeanOffsetNowCm = MeanOffsetRateCmS > 0.f ? FMath::FInterpConstantTo(MeanOffsetNowCm, MeanOffsetTargetCm, TideAge, MeanOffsetRateCmS) : MeanOffsetTargetCm;
	}
	TideAge = 0.f;
	if (!Sky.IsValid())
	{
		TActorIterator<ALyudSky> It(World);
		Sky = It ? *It : nullptr;
	}
	// the sky's clock (with lyud.SkyHourOffset), so a shot of a night shows that night's tide
	double Lon = 20.40, Utc = 2.0;
	FDateTime Local = UGameCalendar::NowIn(World);
	if (const ALyudSky* S = Sky.Get())
	{
		Local = S->ShownTime();
		Lon = S->LongitudeDeg;
		Utc = S->UtcOffsetHours;
	}
	const FDateTime When = Local - FTimespan::FromHours(Utc) + FTimespan::FromHours(TideExtraHours);
	const LyudTide::FState T = LyudTide::At(When, Lon);
	TideCm = float(T.LevelM) * 100.f + MeanOffsetNowCm;
	TideLowCm = float(T.LowM) * 100.f + MeanOffsetNowCm;
	TideSpringK = float(T.Spring);
	bTideRising = T.bRising;
	WetLineCm = float(LyudTide::RecentHighM(When, Lon, 1.0)) * 100.f + MeanOffsetNowCm;
	const float Pin = CVarTideOverride.GetValueOnGameThread();
	if (!FMath::IsNaN(Pin) && Pin > -900.f)
	{
		TideCm = FMath::Min(Pin, 0.f) * 100.f;
		// as if on the ebb: the sand a little above the pinned water is still wet
		WetLineCm = FMath::Min(TideCm + 120.f, 0.f);
	}
	// the water volume's top rides the tide (moved only by whole centimetres: it is a big box)
	if (WaterVolume && FMath::Abs(VolumeTideCm - TideCm) > 2.f)
	{
		VolumeTideCm = TideCm;
		FVector At = WaterVolume->GetActorLocation();
		At.Z = SeaLevel() - WaterVolumeDepth * 0.5;
		WaterVolume->SetActorLocation(At);
	}
}

void ALyudSea::SetLevelOffset(float Cm, float BlendSeconds)
{
	if (bMeanOffsetSet && FMath::Abs(Cm - MeanOffsetTargetCm) < 0.1f)
	{
		return;
	}
	MeanOffsetTargetCm = Cm;
	MeanOffsetRateCmS = BlendSeconds > 0.f && bMeanOffsetSet ? FMath::Abs(Cm - MeanOffsetNowCm) / BlendSeconds : 0.f;
	if (MeanOffsetRateCmS <= 0.f)
	{
		MeanOffsetNowCm = Cm;
	}
	bMeanOffsetSet = true;
	TideAge = 1e9f;   // apply on the next tick
}

void ALyudSea::UpdateLook(float DeltaSeconds)
{
	// what the water's colour follows (the material's SeaLook): the cloud over it (grey-green, desaturated, darker),
	// the sea state (a storm: dark slate, turbid, more foam), the season (winter colder and greyer, summer brighter)
	// and the night. The light and the sky it mirrors change it by themselves (sunset, moonlight); this is the
	// water's own body colour.
	FLinearColor Want(0.f, 0.f, 0.f, 0.f);
	if (const ULyudWeatherSubsystem* W = ULyudWeatherSubsystem::Get(this))
	{
		const FLyudSkyWeather Sky0 = W->GetSky();
		Want.R = FMath::Clamp(Sky0.Cloud + 0.3f * FMath::Min(Sky0.Rain / 4.f, 1.f), 0.f, 1.f);
		Want.G = FMath::Max(FMath::SmoothStep(1.6f, 3.5f, Field->Hs), 0.6f * Sky0.Thunder);
	}
	else
	{
		Want.G = FMath::SmoothStep(1.6f, 3.5f, Field->Hs);
	}
	FDateTime Local = UGameCalendar::NowIn(GetWorld());
	float SunAlt = 30.f;
	if (const ALyudSky* S = Sky.Get())
	{
		Local = S->ShownTime();
		SunAlt = S->SunAltitude();
	}
	// coldest water late February, warmest early August
	Want.B = FMath::Cos(2.f * PI * float(Local.GetDayOfYear() - 50) / 365.f);
	Want.A = 1.f - FMath::SmoothStep(-6.f, 4.f, SunAlt);
	const float T = CVarSeaLookSeconds.GetValueOnGameThread();
	if (!bLookSet || T <= 0.f)
	{
		LookNow = Want;
		bLookSet = true;
	}
	else
	{
		const float K = 1.f - FMath::Exp(-DeltaSeconds / T);
		LookNow = LookNow + (Want - LookNow) * K;
	}
	if (SeaMID)
	{
		SeaMID->SetVectorParameterValue(TEXT("SeaLook"), LookNow);
	}
}

double ALyudSea::WaveClock() const
{
	return Field->Wrap(Clock);
}

FLyudSeaSample ALyudSea::SampleSurface(const FVector& Location) const
{
	FLyudSeaSample S;
	const float SeaZ = SeaLevel();
	const double X = Location.X * 0.01, Y = Location.Y * 0.01;
	const float D = WaterDepthAtM(X, Y);
	S.Depth = D * 100.f;
	S.SurfaceZ = SeaZ;
	S.bIsSea = D > 0.f && Location.X >= LandSideX;
	if (S.bIsSea)
	{
		const LyudWeather::FSeaSurface W = Field->SurfaceAt(X, Y, WaveClock(), D);
		S.SurfaceZ = SeaZ + W.Height * 100.f;
		S.Normal = FVector(W.Normal[0], W.Normal[1], W.Normal[2]);
		S.Velocity = FVector(W.Velocity[0], W.Velocity[1], W.Velocity[2]) * 100.f;
	}
	S.Immersion = S.SurfaceZ - Location.Z;
	return S;
}

float ALyudSea::WaveHeight() const { return Field->Hs * 100.f; }
float ALyudSea::SeaWindSpeed() const { return Field->WindSpeed; }

bool ALyudSea::ViewLocation(FVector& Out) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	if (World->IsGameWorld())
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			if (PC->PlayerCameraManager)
			{
				Out = PC->PlayerCameraManager->GetCameraLocation();
				return true;
			}
		}
	}
	if (World->ViewLocationsRenderedLastFrame.Num() > 0)
	{
		Out = World->ViewLocationsRenderedLastFrame[0];
		return true;
	}
	return false;
}

void ALyudSea::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	GSea = this;
	AppliedQuality = -2;
	ApplyQuality(CVarSeaQuality.GetValueOnGameThread());
}

void ALyudSea::BeginPlay()
{
	Super::BeginPlay();
	GSea = this;
	if (!SurfaceMesh)
	{
		AppliedQuality = -2;
	}
	ApplyQuality(CVarSeaQuality.GetValueOnGameThread());
	Collection = LoadObject<UMaterialParameterCollection>(nullptr, CollectionPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (bWaterVolume)
	{
		SpawnWaterVolume();
	}
}

void ALyudSea::SpawnWaterVolume()
{
	UWorld* World = GetWorld();
	if (!World || WaterVolume)
	{
		return;
	}
	// a box of water from the land side out to the edge of the sea, sea level down to WaterVolumeDepth;
	// the volume's brush is a simple box body built here (no editor brush in a game)
	const double East = FMath::Max<double>(LandSideX, -OuterRadius), West = OuterRadius;
	const FVector Size(West - East, OuterRadius * 2.0, WaterVolumeDepth);
	const FVector Center((East + West) * 0.5, GetActorLocation().Y, SeaLevel() - WaterVolumeDepth * 0.5);
	FActorSpawnParameters P;
	P.Name = TEXT("LyudSeaWater");
	APhysicsVolume* V = World->SpawnActor<APhysicsVolume>(Center, FRotator::ZeroRotator, P);
	if (!V)
	{
		return;
	}
	V->bWaterVolume = true;
	V->FluidFriction = FluidFriction;
	V->Priority = 10;
	UBrushComponent* B = V->GetBrushComponent();
	UBodySetup* Body = NewObject<UBodySetup>(B, TEXT("SeaWaterBody"), RF_Transient);
	Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	Body->AggGeom.BoxElems.Add(FKBoxElem(Size.X, Size.Y, Size.Z));
	Body->CreatePhysicsMeshes();
	B->BrushBodySetup = Body;
	B->SetMobility(EComponentMobility::Movable);
	B->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	B->SetGenerateOverlapEvents(true);
	B->RecreatePhysicsState();
	B->UpdateBounds();
	WaterVolume = V;
	VolumeTideCm = TideCm;
	UE_LOG(LogTemp, Log, TEXT("LyudSea: water volume %s, %.0f x %.0f x %.0f m"), *Center.ToString(), Size.X * 0.01, Size.Y * 0.01, Size.Z * 0.01);
}

void ALyudSea::KeepSwimmerHeadUp(float DeltaSeconds)
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	ACharacter* C = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	UCharacterMovementComponent* Move = C ? C->GetCharacterMovement() : nullptr;
	USceneComponent* Root = C ? C->GetRootComponent() : nullptr;
	if (!Move || !Root || !PC->PlayerCameraManager || !WaterVolume)
	{
		return;
	}
	// The water volume's flat top knows nothing of the waves: a swimmer lifted on a crest would pop out of
	// it, fall, drop back in. So while the player's body is in the sea and the water is deeper than a
	// standing man's chest, the sea holds them in its water itself; shallower, they find their feet.
	const FVector At = C->GetActorLocation();
	const FLyudSeaSample Here = SampleSurface(At);
	const float Bottom = At.Z - C->GetSimpleCollisionHalfHeight();
	const bool bInSea = Here.bIsSea && Here.Depth > SwimMinDepth && Bottom < Here.SurfaceZ - 10.f;
	if (!bInSea)
	{
		if (bHoldingSwimmer && Swimmer.Get() == C)
		{
			Root->SetShouldUpdatePhysicsVolume(true);
			Root->UpdatePhysicsVolume(true);
		}
		bHoldingSwimmer = false;
		return;
	}
	if (!bHoldingSwimmer || Swimmer.Get() != C)
	{
		bHoldingSwimmer = true;
		Swimmer = C;
		Root->SetShouldUpdatePhysicsVolume(false);
	}
	if (Root->GetPhysicsVolume() != WaterVolume)
	{
		Root->SetPhysicsVolume(WaterVolume, true);
	}
	if (!Move->IsSwimming())
	{
		Move->SetMovementMode(MOVE_Swimming);
	}
	if (SwimEyeClearance <= 0.f)
	{
		return;
	}
	// the eyes ride the waves: a swimming body floats with its head out, whatever put it in the water
	// (a fall, the hand-over from a drowned bird)
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const FLyudSeaSample S = SampleSurface(Eye);
	const float Want = S.SurfaceZ + SwimEyeClearance;
	if (Eye.Z >= Want)
	{
		return;
	}
	const float NewZ = Want - Eye.Z > 40.f ? Want : FMath::FInterpTo(Eye.Z, Want, DeltaSeconds, 12.f);
	C->AddActorWorldOffset(FVector(0.0, 0.0, NewZ - Eye.Z), false, nullptr, ETeleportType::TeleportPhysics);
	FVector V = Move->Velocity;
	V.Z = FMath::Max(V.Z, S.Velocity.Z);
	Move->Velocity = V;
}

bool ALyudSea::IsSwimmable(const FVector& Location) const
{
	const FLyudSeaSample S = SampleSurface(Location);
	return S.bIsSea && S.Immersion > 0.f && Location.Z > SeaLevel() - S.Depth;
}

void ALyudSea::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GSea.Get() == this)
	{
		GSea.Reset();
	}
	if (WaterVolume)
	{
		WaterVolume->Destroy();
		WaterVolume = nullptr;
	}
	Super::EndPlay(Reason);
}

void ALyudSea::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const bool bOn = CVarSea.GetValueOnGameThread() != 0;
	Surface->SetVisibility(bOn);
	Seabed->SetActive(bOn);
	if (!bOn)
	{
		Underwater->BlendWeight = 0.f;
		return;
	}
	ApplyQuality(CVarSeaQuality.GetValueOnGameThread());
	Clock += DeltaSeconds;
	UpdateTide(DeltaSeconds);

	// the ground under the sea: a few dozen rows a frame until it is all read
	StepDepthMap(GetWorld() && GetWorld()->IsGameWorld() ? 8000 : 30000);

	WaveAge += DeltaSeconds;
	if (WaveAge > 0.5f)
	{
		WaveAge = 0.f;
		UpdateWaves(false);
	}
	EnsureMaterial();

	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		KeepSwimmerHeadUp(DeltaSeconds);
	}
	FVector Eye;
	if (ViewLocation(Eye))
	{
		// the rings follow the eye in whole fine cells, so the near water does not swim
		const double Snap = MinStepCm * 2.0;
		Surface->SetWorldLocation(FVector(FMath::GridSnap(Eye.X, Snap), FMath::GridSnap(Eye.Y, Snap), SeaLevel()));
		const FLyudSeaSample S = SampleSurface(Eye);
		bCameraUnder = S.bIsSea && S.Immersion > 2.f;
		Underwater->BlendWeight = bCameraUnder ? 1.f : 0.f;
		if (bCameraUnder && UnderMID)
		{
			// the light coming down through the surface: full by day, a glimmer at night
			if (!Sky.IsValid())
			{
				TActorIterator<ALyudSky> It(GetWorld());
				Sky = It ? *It : nullptr;
			}
			const float Alt = Sky.IsValid() ? Sky->SunAltitude() : 30.f;
			UnderMID->SetScalarParameterValue(TEXT("Light"), 0.03f + 0.97f * FMath::SmoothStep(-4.f, 12.f, Alt));
		}
		if (SeaMID)
		{
			SeaMID->SetScalarParameterValue(TEXT("CameraUnder"), bCameraUnder ? 1.f : 0.f);
		}
	}
	if (SeaMID)
	{
		SeaMID->SetScalarParameterValue(TEXT("DebugFoam"), CVarSeaDebugFoam.GetValueOnGameThread());
		if (CVarSeaCapillary.GetValueOnGameThread() >= 0.f)
		{
			SeaMID->SetVectorParameterValue(TEXT("Glitter"), FLinearColor(CVarSeaCapillary.GetValueOnGameThread(), 0.03f, 0.f, 0.f));
		}
		SeaMID->SetVectorParameterValue(TEXT("SeaClock"), FLinearColor((float)WaveClock(), (float)Segments, MinStepCm * 0.01f, OpenSeaDepthM));
		SeaMID->SetScalarParameterValue(TEXT("Tide"), TideCm * 0.01f);
	}
	UpdateLook(DeltaSeconds);
	if (Collection)
	{
		// the dominant wave for things that sway with the sea (seabed weed, moored boats)
		LyudWeather::FWaveTrain Big;
		for (const LyudWeather::FWaveTrain& T : Field->Geo)
		{
			if (T.Amp > Big.Amp)
			{
				Big = T;
			}
		}
		UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("Sea"), FLinearColor(SeaLevel(), (float)WaveClock(), Field->Hs, Field->PeakLambda));
		// the tide for the ground (wet sand under the wet line): water now, datum, wet line (world cm), springs 0..1
		UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("Tide"), FLinearColor(SeaLevel(), DatumZ(), WetLineZ(), TideSpringK));
		UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("SeaWave"), FLinearColor(Big.DirX, Big.DirY, Big.K, Big.Amp));
		UKismetMaterialLibrary::SetScalarParameterValue(this, Collection, TEXT("SeaWaveOmega"), Big.Omega);
		UKismetMaterialLibrary::SetScalarParameterValue(this, Collection, TEXT("SeaWavePhase"), Big.Phase);
	}
	if (CVarSeaDebug.GetValueOnGameThread() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(0x53454121, 0.f, FColor::Cyan, GetDebugReport());
	}
}

FString ALyudSea::GetDebugReport() const
{
	FString Player;
	if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (const APawn* P = PC->GetPawn())
		{
			const APhysicsVolume* V = P->GetRootComponent() ? P->GetRootComponent()->GetPhysicsVolume() : nullptr;
			const ACharacter* C = Cast<ACharacter>(P);
			Player = FString::Printf(TEXT(", player in %s (water %d), movement %d"), V ? *V->GetName() : TEXT("none"), V ? (int32)V->bWaterVolume : -1,
				C && C->GetCharacterMovement() ? (int32)C->GetCharacterMovement()->MovementMode.GetValue() : -1);
		}
	}
	return Player.IsEmpty() ? GetDebugReportCore() : GetDebugReportCore() + Player;
}

FString ALyudSea::GetDebugReportCore() const
{
	return FString::Printf(TEXT("sea: tide %+.2f m (low %.2f, wet line %+.2f, springs %.2f, %s), wind %.1f m/s, Hs %.2f m, peak %.0f m, chop %.2f, quality %d (%d geo + %d detail trains, %d segments), depth map %d/%d rows, camera %s, mean level %+.0f cm (to %+.0f), look cloud %.2f storm %.2f cold %+.2f night %.2f"),
		TideCm * 0.01f, TideLowCm * 0.01f, WetLineCm * 0.01f, TideSpringK, bTideRising ? TEXT("flood") : TEXT("ebb"),
		Field->WindSpeed, Field->Hs, Field->PeakLambda, Field->Chop, AppliedQuality, (int32)Field->Geo.size(), (int32)Field->Detail.size(), Segments,
		DepthRow, DepthH, bCameraUnder ? TEXT("under water") : TEXT("above water"), MeanOffsetNowCm, MeanOffsetTargetCm,
		LookNow.R, LookNow.G, LookNow.B, LookNow.A);
}

ULyudBuoyancyComponent::ULyudBuoyancyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	Points = { FVector(50, 50, 0), FVector(50, -50, 0), FVector(-50, 50, 0), FVector(-50, -50, 0) };
}

void ULyudBuoyancyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(GetAttachParent());
	const ALyudSea* Sea = ALyudSea::Get(this);
	if (!Body || !Sea || !Body->IsSimulatingPhysics() || Points.Num() == 0)
	{
		return;
	}
	const FTransform Xf = Body->GetComponentTransform();
	// water 1000 kg/m3: a litre under water lifts 9.81 N = 981 kg cm/s2
	const float LiftPerPoint = VolumeLitres * 981.f / Points.Num();
	int32 Under = 0;
	for (const FVector& Local : Points)
	{
		const FVector P = Xf.TransformPosition(Local);
		const FLyudSeaSample S = Sea->SampleSurface(P);
		const FVector V = Body->GetPhysicsLinearVelocityAtPoint(P);
		if (!S.bIsSea || S.Immersion <= -PointHeight * 0.5f)
		{
			Body->AddForceAtLocation(-V * AirDrag, P);
			continue;
		}
		const float Sub = FMath::Clamp(S.Immersion / PointHeight + 0.5f, 0.f, 1.f);
		Under += Sub > 0.5f;
		Body->AddForceAtLocation(FVector::UpVector * LiftPerPoint * Sub + (S.Velocity - V) * WaterDrag * Sub, P);
	}
	Submerged = float(Under) / Points.Num();
}

bool ULyudSeaSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULyudSeaSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!CVarSeaReplacePlane.GetValueOnGameThread() || ALyudSea::Get(&InWorld))
	{
		return;
	}
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		TArray<UStaticMeshComponent*> Comps;
		It->GetComponents(Comps);
		for (UStaticMeshComponent* C : Comps)
		{
			UMaterialInterface* M = C->GetMaterial(0);
			if (!M || !M->GetName().Contains(TEXT("M_BalticSea")))
			{
				continue;
			}
			const FBox Box = C->Bounds.GetBox();
			FActorSpawnParameters P;
			P.Name = TEXT("LyudSea");
			ALyudSea* Sea = InWorld.SpawnActor<ALyudSea>(FVector(0.0, Box.GetCenter().Y, C->GetComponentLocation().Z), FRotator::ZeroRotator, P);
			if (Sea)
			{
				Sea->LandSideX = Box.Min.X;
				C->SetVisibility(false);
				C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				UE_LOG(LogTemp, Log, TEXT("LyudSea: replaced the flat sea %s (z %.0f, land side x %.0f)"), *It->GetName(), C->GetComponentLocation().Z, Box.Min.X);
			}
			return;
		}
	}
}

namespace
{
	/** lyud.Sea.SpawnFloat [x y z]: a 1 m wooden crate (half full of air) dropped on the water 6 m ahead of the camera, or at x y z (cm), tagged LyudFloat. */
	FAutoConsoleCommandWithWorldAndArgs GSpawnFloat(TEXT("lyud.Sea.SpawnFloat"),
		TEXT("Drop a floating 1 m crate on the sea 6 m ahead of the camera (or at x y z cm), tagged LyudFloat, to see buoyancy."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || !World->IsGameWorld())
			{
				return;
			}
			FVector At(0.0);
			if (Args.Num() >= 3)
			{
				At = FVector(FCString::Atod(*Args[0]), FCString::Atod(*Args[1]), FCString::Atod(*Args[2]));
			}
			else if (const APlayerController* PC = World->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
			{
				At = PC->PlayerCameraManager->GetCameraLocation() + PC->PlayerCameraManager->GetCameraRotation().Vector().GetSafeNormal2D() * 600.0;
				if (const ALyudSea* Sea = ALyudSea::Get(World))
				{
					At.Z = Sea->SeaLevel() + 150.0;
				}
			}
			UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(At, FRotator(10.0, 30.0, 5.0));
			if (!A || !Cube)
			{
				return;
			}
			A->Tags.Add(TEXT("LyudFloat"));
			UStaticMeshComponent* C = A->GetStaticMeshComponent();
			C->SetMobility(EComponentMobility::Movable);
			C->SetStaticMesh(Cube);
			C->SetCollisionProfileName(TEXT("PhysicsActor"));
			C->SetSimulatePhysics(true);
			// 1 m3 of wood and air: ~450 kg, so it floats a little under half sunk
			C->SetMassOverrideInKg(NAME_None, 450.f, true);
			C->LightingChannels.bChannel1 = true;
			ULyudBuoyancyComponent* B = NewObject<ULyudBuoyancyComponent>(A, TEXT("Buoyancy"));
			B->VolumeLitres = 1000.f;
			B->PointHeight = 100.f;
			B->WaterDrag = 900.f;
			B->Points = { FVector(40, 40, -10), FVector(40, -40, -10), FVector(-40, 40, -10), FVector(-40, -40, -10),
				FVector(40, 40, 30), FVector(40, -40, 30), FVector(-40, 40, 30), FVector(-40, -40, 30) };
			B->SetupAttachment(C);
			B->RegisterComponent();
			UE_LOG(LogTemp, Log, TEXT("LyudSea: floating crate at %s"), *At.ToString());
		}));
}
