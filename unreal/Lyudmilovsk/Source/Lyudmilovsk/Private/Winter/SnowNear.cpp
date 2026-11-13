#include "Winter/LyudWinter.h"

#include "Animals/AnimalActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "MeshDescription.h"
#include "People/PeopleLife.h"
#include "GameFramework/Volume.h"
#include "Plants/ProceduralForest.h"
#include "Plants/ProceduralGrass.h"
#include "Plants/ProceduralPlant.h"
#include "Plants/ProceduralVine.h"
#include "SnowModel.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "StaticMeshAttributes.h"
#include "TextureResource.h"

namespace
{
	const TCHAR* const SnowNearMaterialPath = TEXT("/Game/Winter/M_SnowNear.M_SnowNear");
	const TCHAR* const SnowNearGroundPath = TEXT("/Game/Winter/RT_NearGround.RT_NearGround");
	const TCHAR* const SnowNearTrailPath = TEXT("/Game/Winter/RT_SnowTrail.RT_SnowTrail");
	const TCHAR* const SnowNearCollectionPath = TEXT("/Game/Winter/MPC_LyudWinter.MPC_LyudWinter");

	/** Bytes of a region into a render target (BytesPerTexel each, rows of W). */
	void SnowNearUpload(UTextureRenderTarget2D* RT, int32 X, int32 Y, int32 W, int32 H, int32 BytesPerTexel, TArray<uint8>&& Data)
	{
		if (!RT || W <= 0 || H <= 0)
		{
			return;
		}
		FTextureResource* Res = RT->GetResource();
		ENQUEUE_RENDER_COMMAND(LyudSnowNearUpload)([Res, X, Y, W, H, BytesPerTexel, Data = MoveTemp(Data)](FRHICommandListImmediate& RHICmdList)
		{
			if (Res && Res->TextureRHI)
			{
				RHICmdList.UpdateTexture2D(Res->TextureRHI, 0, FUpdateTextureRegion2D(X, Y, 0, 0, W, H), W * BytesPerTexel, Data.GetData());
			}
		});
	}

	/** What the snow lies on: the landscape, or a low surface over it (a yard's paving, a platform; under 1.8 m). */
	float SnowNearGroundAt(const UWorld* World, double X, double Y, double Z, bool& bOk, float& OutTop)
	{
		OutTop = -1e9f;
		TArray<FHitResult> Hits;
		World->LineTraceMultiByObjectType(Hits, FVector(X, Y, Z + 8000.0), FVector(X, Y, Z - 8000.0), FCollisionObjectQueryParams(ECC_WorldStatic),
			FCollisionQueryParams(SCENE_QUERY_STAT(LyudSnowNear), true));
		Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Distance < B.Distance; });
		double Top = -1e12;
		for (const FHitResult& H : Hits)
		{
			const AActor* A = H.GetActor();
			const UPrimitiveComponent* C = H.GetComponent();
			if (!A || !C || A->IsA<AVolume>() || A->IsA<ALyudSeaIce>() || A->IsA<ANpcBody>() || !C->IsVisible())
			{
				continue;
			}
			const bool bPlant = A->IsA<AProceduralPlant>() || A->IsA<AProceduralForest>() || A->IsA<AProceduralGrass>() || A->IsA<AProceduralVine>();
			if (Top < -1e11 && !bPlant)
			{
				Top = H.ImpactPoint.Z;
			}
			if (A->IsA<ALandscapeProxy>())
			{
				bOk = true;
				const double G = H.ImpactPoint.Z;
				OutTop = float(Top > G ? Top : G);
				return float(G);
			}
		}
		bOk = false;
		return float(Z);
	}
}

ALyudSnowNear::ALyudSnowNear()
{
	PrimaryActorTick.bCanEverTick = false;
	Surface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Surface"));
	RootComponent = Surface;
	Surface->SetMobility(EComponentMobility::Movable);
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// the map's snow decal shades it like the ground round it (the same colour, sparkle and light): no ring
	Surface->SetReceivesDecals(true);
	Surface->LightingChannels.bChannel0 = true;
	Surface->LightingChannels.bChannel1 = true;
	// no shadow of its own: a world-position-offset mesh would redraw the virtual shadow map every frame
	Surface->SetCastShadow(false);
	// it moves with the camera: keep it out of Lumen's scene and the distance fields (no recapture on every recentre)
	Surface->bAffectDynamicIndirectLighting = false;
	Surface->bAffectDistanceFieldLighting = false;
	Trail.SetNumZeroed(TrailN * TrailN);
	Ground.SetNumZeroed(GroundN * GroundN);
	Lowered.SetNumZeroed(GroundN * GroundN);
	RawGround.SetNumZeroed(GroundN * GroundN);
	RawTop.SetNumZeroed(GroundN * GroundN);
	RawOk.SetNumZeroed(GroundN * GroundN);
	TrailUsed.SetNumZeroed(TrailTiles * TrailTiles);
}

int32 ALyudSnowNear::Wrap(double V) const
{
	const int64 I = int64(FMath::FloorToDouble(V / TrailCm));
	return int32(((I % TrailN) + TrailN) % TrailN);
}

void ALyudSnowNear::BuildMesh()
{
	const int32 N = FMath::RoundToInt(2.f * HalfCm / StepCm);
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
	Attr.GetPolygonGroupMaterialSlotNames()[PG] = TEXT("Snow");
	TArray<FVertexInstanceID> Inst;
	Inst.Reserve((N + 1) * (N + 1));
	for (int32 Y = 0; Y <= N; Y++)
	{
		for (int32 X = 0; X <= N; X++)
		{
			const FVertexID V = MD.CreateVertex();
			VPos[V] = FVector3f(-HalfCm + X * StepCm, -HalfCm + Y * StepCm, 0.f);
			const FVertexInstanceID VI = MD.CreateVertexInstance(V);
			VNrm[VI] = FVector3f::UpVector;
			VTan[VI] = FVector3f::XAxisVector;
			VSign[VI] = 1.f;
			VUV.Set(VI, 0, FVector2f(float(X) / N, float(Y) / N));
			Inst.Add(VI);
		}
	}
	for (int32 Y = 0; Y < N; Y++)
	{
		for (int32 X = 0; X < N; X++)
		{
			const int32 A = Y * (N + 1) + X, B = A + 1, C = A + N + 1, D = C + 1;
			MD.CreateTriangle(PG, { Inst[A], Inst[C], Inst[B] });
			MD.CreateTriangle(PG, { Inst[B], Inst[C], Inst[D] });
		}
	}
	UStaticMesh* Mesh = NewObject<UStaticMesh>(this, TEXT("SM_LyudSnowNear"), RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
	Mesh->GetStaticMaterials().Add(FStaticMaterial(LoadObject<UMaterialInterface>(nullptr, SnowNearMaterialPath), TEXT("Snow")));
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bMarkPackageDirty = false;
	Params.bBuildSimpleCollision = false;
	Params.bCommitMeshDescription = false;
	Params.bFastBuild = true;
	Mesh->BuildFromMeshDescriptions({ &MD }, Params);
	// the world position offset puts it anywhere on the ground: keep it from being culled
	Mesh->SetExtendedBounds(FBoxSphereBounds(FBox(FVector(-HalfCm, -HalfCm, -60000.0), FVector(HalfCm, HalfCm, 60000.0))));
	Surface->SetStaticMesh(Mesh);
	GroundRT = LoadObject<UTextureRenderTarget2D>(nullptr, SnowNearGroundPath);
	TrailRT = LoadObject<UTextureRenderTarget2D>(nullptr, SnowNearTrailPath);
	// once here, not a LoadObject every frame in Drive
	NearCollection = LoadObject<UMaterialParameterCollection>(nullptr, SnowNearCollectionPath);
	if (GroundRT)
	{
		GroundRT->ResizeTarget(GroundN, GroundN);
	}
	if (TrailRT)
	{
		TrailRT->ResizeTarget(TrailN, TrailN);
	}
	bFullUpload = true;
}

void ALyudSnowNear::ClearTrailStrip(bool bColumns, int64 First, int64 Num)
{
	// world columns (or rows) First .. First + Num - 1 of the wrapping trail: one or two runs in the store
	int64 Left = Num;
	int32 At = int32(((First % TrailN) + TrailN) % TrailN);
	while (Left > 0)
	{
		const int32 Run = int32(FMath::Min<int64>(Left, TrailN - At));
		bool bHad = false;
		if (bColumns)
		{
			for (int32 Row = 0; Row < TrailN; Row++)
			{
				uint8* Cells = Trail.GetData() + Row * TrailN + At;
				for (int32 K = 0; K < Run; K++)
				{
					bHad |= Cells[K] != 0;
					Cells[K] = 0;
				}
			}
		}
		else
		{
			uint8* Cells = Trail.GetData() + At * TrailN;
			for (int32 K = 0; K < Run * TrailN; K++)
			{
				bHad |= Cells[K] != 0;
			}
			FMemory::Memzero(Cells, Run * TrailN);
		}
		// the texture holds what the store held: only a strip that had prints in it needs sending
		if (bHad && !bFullUpload)
		{
			TArray<uint8> Zeros;
			Zeros.SetNumZeroed(Run * TrailN);
			if (bColumns)
			{
				SnowNearUpload(TrailRT, At, 0, Run, TrailN, 1, MoveTemp(Zeros));
			}
			else
			{
				SnowNearUpload(TrailRT, 0, At, TrailN, Run, 1, MoveTemp(Zeros));
			}
		}
		Left -= Run;
		At = 0;
	}
}

void ALyudSnowNear::TraceCell(int32 Index, double Z)
{
	bool bOk;
	float T;
	const float G = SnowNearGroundAt(GetWorld(), GroundOrigin.X + (Index % GroundN) * GroundCm, GroundOrigin.Y + (Index / GroundN) * GroundCm, Z, bOk, T);
	RawGround[Index] = G;
	RawTop[Index] = T;
	RawOk[Index] = bOk ? 1 : 0;
}

void ALyudSnowNear::ComposeGround(double Z)
{
	// off the landscape: a little under the last ground found, in the window's reading order
	float Last = float(Z);
	TArray<float> Tops;
	Tops.SetNumUninitialized(GroundN * GroundN);
	for (int32 I = 0; I < GroundN * GroundN; I++)
	{
		const bool bOk = RawOk[I] != 0;
		Ground[I] = bOk ? RawGround[I] : Last - 50.f;
		Tops[I] = bOk ? RawTop[I] : Ground[I];
		Last = bOk ? RawGround[I] : Last;
	}
	// what the snow lies on: a low, wide, even surface over the landscape (a yard's paving, a platform) counts, a lone
	// thing standing on the ground (a bench, a stump, a fence's top) does not (it made shelves of snow in the air)
	TArray<float> Base = Ground;
	for (int32 Y = 1; Y + 1 < GroundN; Y++)
	{
		for (int32 X = 1; X + 1 < GroundN; X++)
		{
			const int32 I = Y * GroundN + X;
			const float Lift = Tops[I] - Ground[I];
			if (Lift < 2.f || Lift > 60.f)
			{
				continue;
			}
			int32 Even = 0;
			for (int32 DY = -1; DY <= 1; DY++)
			{
				for (int32 DX = -1; DX <= 1; DX++)
				{
					const int32 J = (Y + DY) * GroundN + X + DX;
					Even += (DX || DY) && Tops[J] - Ground[J] >= 2.f && FMath::Abs(Tops[J] - Tops[I]) < 15.f;
				}
			}
			if (Even >= 7)
			{
				Base[I] = Tops[I];
			}
		}
	}
	Ground = MoveTemp(Base);
}

void ALyudSnowNear::RefreshGround()
{
	if (RefreshLeft <= 0)
	{
		return;
	}
	// the cells kept over a recentre are read again a few a frame (a door, a thing built since): never the whole
	// window in one frame
	const double Z = GetActorLocation().Z;
	const double Until = FPlatformTime::Seconds() + 0.0003;
	bool bChanged = false;
	for (int32 K = 0; K < 24 && RefreshLeft > 0; K++)
	{
		const int32 I = RefreshCursor;
		RefreshCursor = (RefreshCursor + 1) % (GroundN * GroundN);
		RefreshLeft--;
		const float G0 = RawGround[I], T0 = RawTop[I];
		const uint8 Ok0 = RawOk[I];
		TraceCell(I, Z);
		bChanged |= RawGround[I] != G0 || RawTop[I] != T0 || RawOk[I] != Ok0;
		if (FPlatformTime::Seconds() > Until)
		{
			break;
		}
	}
	if (bChanged)
	{
		ComposeGround(Z);
		bGroundDirty = true;
	}
}

void ALyudSnowNear::Recentre(const FVector2D& C)
{
	const UWorld* World = GetWorld();
	// the trail: the window's columns and rows coming in are cleared (it is world-locked, wrapping round)
	const int64 OX = int64(FMath::FloorToDouble((Centre.X - HalfCm) / TrailCm)), OY = int64(FMath::FloorToDouble((Centre.Y - HalfCm) / TrailCm));
	const int64 NX = int64(FMath::FloorToDouble((C.X - HalfCm) / TrailCm)), NY = int64(FMath::FloorToDouble((C.Y - HalfCm) / TrailCm));
	const bool bFar = FMath::Abs(NX - OX) >= TrailN || FMath::Abs(NY - OY) >= TrailN;
	if (bFar)
	{
		FMemory::Memzero(Trail.GetData(), Trail.Num());
		FMemory::Memzero(TrailUsed.GetData(), TrailUsed.Num());
		bFullUpload = true;
	}
	else
	{
		// only the strips that come in, and only if they held prints: not the whole 1200 x 1200 trail every 2 m
		if (NX > OX)
		{
			ClearTrailStrip(true, OX + TrailN, NX - OX);
		}
		else if (NX < OX)
		{
			ClearTrailStrip(true, NX, OX - NX);
		}
		if (NY > OY)
		{
			ClearTrailStrip(false, OY + TrailN, NY - OY);
		}
		else if (NY < OY)
		{
			ClearTrailStrip(false, NY, OY - NY);
		}
	}
	Centre = C;
	// forget the walkers that are gone or far off (they start a fresh track when they come back, as after a jump)
	for (auto It = Walkers.CreateIterator(); It; ++It)
	{
		const AActor* A = It.Key().Get();
		if (!A || FVector2D::DistSquared(FVector2D(A->GetActorLocation()), C) > FMath::Square(2.0 * HalfCm))
		{
			It.RemoveCurrent();
		}
	}
	// the ground's exact heights, 1 m, round the new centre
	// on the landscape's own vertices (its 1 m grid from its origin), so the fine surface can follow its triangles
	// the landscape does not move: found once, not an actor walk every recentre
	if (!bHaveLandOrigin)
	{
		if (TActorIterator<ALandscapeProxy> It(World); It)
		{
			LandOrigin = FVector2D(It->GetActorLocation());
			bHaveLandOrigin = true;
		}
	}
	const FVector2D OldOrigin = GroundOrigin;
	GroundOrigin = FVector2D(C.X - (GroundN - 1) * 0.5 * GroundCm, C.Y - (GroundN - 1) * 0.5 * GroundCm);
	GroundOrigin = LandOrigin + FVector2D(FMath::RoundToDouble((GroundOrigin.X - LandOrigin.X) / GroundCm), FMath::RoundToDouble((GroundOrigin.Y - LandOrigin.Y) / GroundCm)) * GroundCm;
	const double Z = GetActorLocation().Z;
	// the window slides over the ground: the cells it keeps are not traced again here (2 m walked = 2 strips of 48, not
	// all 2304 multi-traces in one frame); RefreshGround reads them again over the next frames
	const double ShiftX = FMath::RoundToDouble((GroundOrigin.X - OldOrigin.X) / GroundCm), ShiftY = FMath::RoundToDouble((GroundOrigin.Y - OldOrigin.Y) / GroundCm);
	const bool bSlide = bHaveRaw && !bFar && FMath::Abs(ShiftX) < GroundN && FMath::Abs(ShiftY) < GroundN;
	if (bSlide)
	{
		const int32 SX = int32(ShiftX), SY = int32(ShiftY);
		const TArray<float> OldGround = RawGround, OldTop = RawTop, OldLowered = Lowered;
		const TArray<uint8> OldOk = RawOk;
		// a vertex keeps its sink if every trail cell it reads (60 cm round it) lies in the old window and the new one:
		// those cells were not cleared and it read the same ones before (40 cm more: a print at the window's very edge
		// spills round the seam to the far side)
		const int32 Reach = FMath::CeilToInt(100.f / TrailCm);
		const int64 LoX = FMath::Max(OX, NX) + Reach, HiX = FMath::Min(OX, NX) + TrailN - Reach;
		const int64 LoY = FMath::Max(OY, NY) + Reach, HiY = FMath::Min(OY, NY) + TrailN - Reach;
		for (int32 Y = 0; Y < GroundN; Y++)
		{
			for (int32 X = 0; X < GroundN; X++)
			{
				const int32 I = Y * GroundN + X;
				const int32 FromX = X + SX, FromY = Y + SY;
				const bool bKept = FromX >= 0 && FromX < GroundN && FromY >= 0 && FromY < GroundN;
				if (bKept)
				{
					const int32 J = FromY * GroundN + FromX;
					RawGround[I] = OldGround[J];
					RawTop[I] = OldTop[J];
					RawOk[I] = OldOk[J];
				}
				else
				{
					TraceCell(I, Z);
				}
				const int64 GX = int64(FMath::FloorToDouble((GroundOrigin.X + X * GroundCm) / TrailCm)), GY = int64(FMath::FloorToDouble((GroundOrigin.Y + Y * GroundCm) / TrailCm));
				if (bKept && GX >= LoX && GX < HiX && GY >= LoY && GY < HiY)
				{
					Lowered[I] = OldLowered[FromY * GroundN + FromX];
				}
				else
				{
					Lowered[I] = LoweredByteAt(X, Y) / 255.f;
				}
			}
		}
	}
	else
	{
		for (int32 I = 0; I < GroundN * GroundN; I++)
		{
			TraceCell(I, Z);
		}
		bHaveRaw = true;
	}
	RefreshLeft = bSlide ? GroundN * GroundN : 0;
	ComposeGround(Z);
	if (!bSlide)
	{
		UpdateLowered(0, 0, GroundN - 1, GroundN - 1);
	}
	UploadGround();
	SetActorLocation(FVector(C.X, C.Y, Z));
}

uint8 ALyudSnowNear::LoweredByteAt(int32 VX, int32 VY) const
{
	// a vertex sinks as far as the deepest print within 60 cm of it (its triangles reach 1 m: the fine surface covers them)
	const int32 R = FMath::CeilToInt(60.f / TrailCm);
	const double WX = GroundOrigin.X + VX * GroundCm, WY = GroundOrigin.Y + VY * GroundCm;
	uint8 M = 0;
	if (FMath::Abs(WX - Centre.X) < HalfCm && FMath::Abs(WY - Centre.Y) < HalfCm)
	{
		const int64 GX = int64(FMath::FloorToDouble(WX / TrailCm)), GY = int64(FMath::FloorToDouble(WY / TrailCm));
		for (int64 TY = GY - R; TY <= GY + R && M < 255; TY++)
		{
			const uint8* Row = Trail.GetData() + int32(((TY % TrailN) + TrailN) % TrailN) * TrailN;
			for (int64 TX = GX - R; TX <= GX + R; TX++)
			{
				M = FMath::Max(M, Row[int32(((TX % TrailN) + TrailN) % TrailN)]);
			}
		}
	}
	return M;
}

void ALyudSnowNear::UpdateLowered(int32 X0, int32 Y0, int32 X1, int32 Y1)
{
	for (int32 VY = FMath::Max(Y0, 0); VY <= FMath::Min(Y1, GroundN - 1); VY++)
	{
		for (int32 VX = FMath::Max(X0, 0); VX <= FMath::Min(X1, GroundN - 1); VX++)
		{
			const float L = LoweredByteAt(VX, VY) / 255.f;
			if (Lowered[VY * GroundN + VX] != L)
			{
				Lowered[VY * GroundN + VX] = L;
				bGroundDirty = true;
			}
		}
	}
}

void ALyudSnowNear::FillPrints(int32 Dec)
{
	// only the tiles that hold prints are faded and sent, not all 1.44 M cells
	int32 NumChanged = 0;
	TArray<int32, TInlineAllocator<64>> Changed;
	for (int32 Tile = 0; Tile < TrailTiles * TrailTiles; Tile++)
	{
		if (!TrailUsed[Tile])
		{
			continue;
		}
		const int32 X0 = (Tile % TrailTiles) * TrailTileN, Y0 = (Tile / TrailTiles) * TrailTileN;
		bool bWas = false, bStays = false;
		for (int32 Y = Y0; Y < Y0 + TrailTileN; Y++)
		{
			uint8* Cells = Trail.GetData() + Y * TrailN + X0;
			for (int32 X = 0; X < TrailTileN; X++)
			{
				const uint8 Old = Cells[X];
				const uint8 New = uint8(FMath::Max(0, int32(Old) - Dec));
				Cells[X] = New;
				bWas |= Old != 0;
				bStays |= New != 0;
			}
		}
		TrailUsed[Tile] = bStays ? 1 : 0;
		if (bWas)
		{
			NumChanged++;
			Changed.Add(Tile);
		}
	}
	if (NumChanged > 96)
	{
		bFullUpload = true;
	}
	else if (!bFullUpload)
	{
		for (const int32 Tile : Changed)
		{
			const int32 X0 = (Tile % TrailTiles) * TrailTileN, Y0 = (Tile / TrailTiles) * TrailTileN;
			TArray<uint8> Bytes;
			Bytes.SetNumUninitialized(TrailTileN * TrailTileN);
			for (int32 Y = 0; Y < TrailTileN; Y++)
			{
				FMemory::Memcpy(Bytes.GetData() + Y * TrailTileN, Trail.GetData() + (Y0 + Y) * TrailN + X0, TrailTileN);
			}
			SnowNearUpload(TrailRT, X0, Y0, TrailTileN, TrailTileN, 1, MoveTemp(Bytes));
		}
	}
	// the vertices' sinks: every cell a vertex reads fell by the same amount, so its deepest did too; the ones at the
	// window's edge read round its seam (or a print spilt round it) and are read again
	const int32 Reach = FMath::CeilToInt(100.f / TrailCm);
	const int64 WX0 = int64(FMath::FloorToDouble((Centre.X - HalfCm) / TrailCm)), WY0 = int64(FMath::FloorToDouble((Centre.Y - HalfCm) / TrailCm));
	for (int32 VY = 0; VY < GroundN; VY++)
	{
		for (int32 VX = 0; VX < GroundN; VX++)
		{
			const int32 I = VY * GroundN + VX;
			const int64 GX = int64(FMath::FloorToDouble((GroundOrigin.X + VX * GroundCm) / TrailCm)), GY = int64(FMath::FloorToDouble((GroundOrigin.Y + VY * GroundCm) / TrailCm));
			const bool bInner = GX - Reach >= WX0 && GX + Reach < WX0 + TrailN && GY - Reach >= WY0 && GY + Reach < WY0 + TrailN;
			const uint8 M = bInner ? uint8(FMath::Max(0, FMath::RoundToInt(Lowered[I] * 255.f) - Dec)) : LoweredByteAt(VX, VY);
			const float L = M / 255.f;
			if (Lowered[I] != L)
			{
				Lowered[I] = L;
				bGroundDirty = true;
			}
		}
	}
}

void ALyudSnowNear::UploadGround()
{
	TArray<uint8> Bytes;
	Bytes.SetNumUninitialized(Ground.Num() * 2 * sizeof(float));
	float* F = reinterpret_cast<float*>(Bytes.GetData());
	for (int32 I = 0; I < Ground.Num(); I++)
	{
		F[I * 2] = Ground[I];
		F[I * 2 + 1] = Lowered[I];
	}
	SnowNearUpload(GroundRT, 0, 0, GroundN, GroundN, 2 * sizeof(float), MoveTemp(Bytes));
	bGroundDirty = false;
}

void ALyudSnowNear::Press(const FVector2D& At, const FVector2D& Dir, float A, float B, float Amount)
{
	const float R = FMath::Max(A, B) + TrailCm;
	const int64 X0 = int64(FMath::FloorToDouble((At.X - R) / TrailCm)), X1 = int64(FMath::FloorToDouble((At.X + R) / TrailCm));
	const int64 Y0 = int64(FMath::FloorToDouble((At.Y - R) / TrailCm)), Y1 = int64(FMath::FloorToDouble((At.Y + R) / TrailCm));
	const FVector2D U = Dir.GetSafeNormal().IsNearlyZero() ? FVector2D(1.0, 0.0) : Dir.GetSafeNormal();
	const FVector2D V(-U.Y, U.X);
	for (int64 GY = Y0; GY <= Y1; GY++)
	{
		for (int64 GX = X0; GX <= X1; GX++)
		{
			const FVector2D P((GX + 0.5) * TrailCm - At.X, (GY + 0.5) * TrailCm - At.Y);
			const float E = FMath::Sqrt(FMath::Square(FVector2D::DotProduct(P, U) / A) + FMath::Square(FVector2D::DotProduct(P, V) / B));
			if (E >= 1.f)
			{
				continue;
			}
			// a bowl: a firm floor easing up into a soft rim
			const float K = Amount * (1.f - FMath::SmoothStep(0.35f, 1.f, E));
			const int32 Col = int32(((GX % TrailN) + TrailN) % TrailN), Row = int32(((GY % TrailN) + TrailN) % TrailN);
			uint8& T = Trail[Row * TrailN + Col];
			T = uint8(FMath::Max<int32>(T, FMath::RoundToInt(K * 255.f)));
			TrailUsed[(Row / TrailTileN) * TrailTiles + Col / TrailTileN] = 1;
		}
	}
	UpdateLowered(FMath::FloorToInt((At.X - R - 60.f - GroundOrigin.X) / GroundCm), FMath::FloorToInt((At.Y - R - 60.f - GroundOrigin.Y) / GroundCm),
		FMath::CeilToInt((At.X + R + 60.f - GroundOrigin.X) / GroundCm), FMath::CeilToInt((At.Y + R + 60.f - GroundOrigin.Y) / GroundCm));
	const int32 CX0 = int32(((X0 % TrailN) + TrailN) % TrailN), CY0 = int32(((Y0 % TrailN) + TrailN) % TrailN);
	const int32 CX1 = CX0 + int32(X1 - X0), CY1 = CY0 + int32(Y1 - Y0);
	if (CX1 >= TrailN || CY1 >= TrailN)
	{
		bFullUpload = true;   // across the window's seam
		return;
	}
	const FIntRect R2(CX0, CY0, CX1 + 1, CY1 + 1);
	Dirty = bDirty ? FIntRect(Dirty.Min.ComponentMin(R2.Min), Dirty.Max.ComponentMax(R2.Max)) : R2;
	bDirty = true;
}

void ALyudSnowNear::Walk(AActor* Who, const FVector& Feet, const FVector& Velocity, float Stride, float Lateral, float A, float B, float SnowM, bool bPlough)
{
	FWalker& W = Walkers.FindOrAdd(Who);
	const FVector2D P(Feet.X, Feet.Y);
	if (W.Last.X > 1e11)
	{
		W.Last = P;
		return;
	}
	const FVector2D D = P - W.Last;
	if (D.Size() > Stride * 4.f)
	{
		W.Last = P;   // a jump (a teleport, a ride): no prints over the gap
		return;
	}
	if (bPlough)
	{
		// deep snow: the legs plough a trench as wide as the body, then the prints in its floor
		if (W.PloughLast.X > 1e11 || FVector2D::Distance(W.PloughLast, P) > Stride * 4.f)
		{
			W.PloughLast = P;
		}
		const FVector2D G = P - W.PloughLast;
		if (G.Size() > 8.f)
		{
			const FVector2D GU = G.GetSafeNormal();
			for (float T = 0.f; T <= G.Size(); T += 8.f)
			{
				Press(W.PloughLast + GU * T, GU, 26.f, 24.f, FMath::Clamp((SnowM - 0.2f) * 1.6f, 0.3f, 0.7f));
			}
			W.PloughLast = P;
		}
	}
	if (D.Size() < Stride)
	{
		return;
	}
	const FVector2D U = D.GetSafeNormal();
	const FVector2D Side(-U.Y, U.X);
	Press(P + Side * (Lateral * W.Side), U, A, B, 1.f);
	W.Side = -W.Side;
	W.Last = P;
	Prints++;
}

float ALyudSnowNear::PressedAt(const FVector& Location) const
{
	return Trail[Wrap(Location.Y) * TrailN + Wrap(Location.X)] / 255.f;
}

void ALyudSnowNear::Drive(ULyudWinterSubsystem& Winter, const FVector& Camera, float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!Surface->GetStaticMesh())
	{
		BuildMesh();
	}
	const FVector2D C(FMath::GridSnap(Camera.X, 200.0), FMath::GridSnap(Camera.Y, 200.0));
	if (FVector2D::Distance(C, Centre) > 1.0)
	{
		Recentre(C);
	}
	else
	{
		RefreshGround();
	}
	// the walkers near the camera: the player, the people's bodies, the cats and dogs
	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		if (ACharacter* Hero = Cast<ACharacter>(PC->GetPawn()))
		{
			const FVector Feet = Hero->GetActorLocation() - FVector(0.f, 0.f, Hero->GetSimpleCollisionHalfHeight());
			const float SnowM = Winter.GetSnowDepthAt(Feet) * 0.01f;
			float Surf;
			if (SnowM > 0.02f && Winter.GetSnowSurfaceAt(Feet, Surf) && Feet.Z < Surf + LyudWinter::RoofMinCm)
			{
				Walk(Hero, Feet, Hero->GetVelocity(), 72.f, 11.f, 15.f, 6.5f, SnowM, SnowM > 0.22f);
			}
		}
	}
	const double R2 = FMath::Square(HalfCm);
	for (TActorIterator<ANpcBody> It(World); It; ++It)
	{
		const FVector L = It->GetActorLocation();
		if (!It->IsHidden() && FVector::DistSquared2D(L, FVector(C, 0.0)) < R2 && Winter.GetSnowDepthAt(L) > 2.f)
		{
			Walk(*It, L, FVector::ZeroVector, 68.f, 10.f, 14.f, 6.f, Winter.GetSnowDepthAt(L) * 0.01f, false);
		}
	}
	for (TActorIterator<AAnimalActor> It(World); It; ++It)
	{
		const FVector L = It->GetActorLocation();
		if (!It->IsHidden() && FVector::DistSquared2D(L, FVector(C, 0.0)) < R2 && Winter.GetSnowDepthAt(L) > 2.f)
		{
			Walk(*It, L, FVector::ZeroVector, 26.f, 5.f, 4.5f, 3.5f, Winter.GetSnowDepthAt(L) * 0.01f, false);
		}
	}
	// fresh snow fills the prints (~5 cm of it hides a print)
	const float Fall = Winter.GetNow().SnowfallCmPerHour;
	FillTimer += DeltaTime;
	if (Fall > 0.05f && FillTimer > 2.f)
	{
		const float GameHours = FillTimer / 3600.f * FMath::Max(1.f, World->GetWorldSettings() ? World->GetWorldSettings()->TimeDilation : 1.f);
		const int32 Dec = FMath::Clamp(FMath::RoundToInt(Fall * GameHours / 5.f * 255.f), 1, 255);
		FillPrints(Dec);
		FillTimer = 0.f;
	}
	else if (Fall <= 0.05f)
	{
		FillTimer = 0.f;
	}
	if (bGroundDirty)
	{
		UploadGround();
	}
	if (bFullUpload)
	{
		SnowNearUpload(TrailRT, 0, 0, TrailN, TrailN, 1, TArray<uint8>(Trail));
		bFullUpload = false;
		bDirty = false;
	}
	else if (bDirty)
	{
		TArray<uint8> Bytes;
		const int32 W = Dirty.Width(), H = Dirty.Height();
		Bytes.SetNumUninitialized(W * H);
		for (int32 Y = 0; Y < H; Y++)
		{
			FMemory::Memcpy(Bytes.GetData() + Y * W, Trail.GetData() + (Dirty.Min.Y + Y) * TrailN + Dirty.Min.X, W);
		}
		SnowNearUpload(TrailRT, Dirty.Min.X, Dirty.Min.Y, W, H, 1, MoveTemp(Bytes));
		bDirty = false;
	}
	if (UMaterialParameterCollection* MPC = NearCollection)
	{
		static const FName NearRectName(TEXT("NearRect")), NearGroundName(TEXT("NearGround")), NearTrailName(TEXT("NearTrail"));
		UKismetMaterialLibrary::SetVectorParameterValue(World, MPC, NearRectName, FLinearColor(float(C.X), float(C.Y), HalfCm, 600.f));
		UKismetMaterialLibrary::SetVectorParameterValue(World, MPC, NearGroundName, FLinearColor(float(GroundOrigin.X), float(GroundOrigin.Y), GroundCm, float(GroundN)));
		UKismetMaterialLibrary::SetVectorParameterValue(World, MPC, NearTrailName, FLinearColor(TrailCm, TrailCm * TrailN, 0.f, 0.f));
	}
}
