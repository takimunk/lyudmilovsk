#include "Winter/LyudWinter.h"

#include "Animals/AnimalActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
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
	Trail.SetNumZeroed(TrailN * TrailN);
	Ground.SetNumZeroed(GroundN * GroundN);
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

void ALyudSnowNear::Recentre(const FVector2D& C)
{
	const UWorld* World = GetWorld();
	// the trail: the window's columns and rows coming in are cleared (it is world-locked, wrapping round)
	const int64 OX = int64(FMath::FloorToDouble((Centre.X - HalfCm) / TrailCm)), OY = int64(FMath::FloorToDouble((Centre.Y - HalfCm) / TrailCm));
	const int64 NX = int64(FMath::FloorToDouble((C.X - HalfCm) / TrailCm)), NY = int64(FMath::FloorToDouble((C.Y - HalfCm) / TrailCm));
	if (FMath::Abs(NX - OX) >= TrailN || FMath::Abs(NY - OY) >= TrailN)
	{
		FMemory::Memzero(Trail.GetData(), Trail.Num());
	}
	else
	{
		for (int64 G = NX; G < NX + TrailN; G++)
		{
			if (G < OX || G >= OX + TrailN)
			{
				const int32 Col = int32(((G % TrailN) + TrailN) % TrailN);
				for (int32 R = 0; R < TrailN; R++)
				{
					Trail[R * TrailN + Col] = 0;
				}
			}
		}
		for (int64 G = NY; G < NY + TrailN; G++)
		{
			if (G < OY || G >= OY + TrailN)
			{
				const int32 Row = int32(((G % TrailN) + TrailN) % TrailN);
				FMemory::Memzero(Trail.GetData() + Row * TrailN, TrailN);
			}
		}
	}
	bFullUpload = true;
	Centre = C;
	// the ground's exact heights, 1 m, round the new centre
	GroundOrigin = FVector2D(C.X - (GroundN - 1) * 0.5 * GroundCm, C.Y - (GroundN - 1) * 0.5 * GroundCm);
	const double Z = GetActorLocation().Z;
	float Last = float(Z);
	TArray<float> Tops;
	Tops.SetNumZeroed(GroundN * GroundN);
	for (int32 Y = 0; Y < GroundN; Y++)
	{
		for (int32 X = 0; X < GroundN; X++)
		{
			bool bOk;
			float T;
			const float G = SnowNearGroundAt(World, GroundOrigin.X + X * GroundCm, GroundOrigin.Y + Y * GroundCm, Z, bOk, T);
			Ground[Y * GroundN + X] = bOk ? G : Last - 50.f;
			Tops[Y * GroundN + X] = bOk ? T : Ground[Y * GroundN + X];
			Last = bOk ? G : Last;
		}
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
	Ground = Base;
	TArray<uint8> Bytes;
	Bytes.SetNumUninitialized(Ground.Num() * sizeof(float));
	FMemory::Memcpy(Bytes.GetData(), Ground.GetData(), Bytes.Num());
	SnowNearUpload(GroundRT, 0, 0, GroundN, GroundN, sizeof(float), MoveTemp(Bytes));
	SetActorLocation(FVector(C.X, C.Y, Z));
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
			// a firm floor and a soft rim
			const float K = Amount * (1.f - FMath::SmoothStep(0.65f, 1.f, E));
			const int32 Col = int32(((GX % TrailN) + TrailN) % TrailN), Row = int32(((GY % TrailN) + TrailN) % TrailN);
			uint8& T = Trail[Row * TrailN + Col];
			T = uint8(FMath::Max<int32>(T, FMath::RoundToInt(K * 255.f)));
		}
	}
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
		for (uint8& T : Trail)
		{
			T = uint8(FMath::Max(0, int32(T) - Dec));
		}
		bFullUpload = true;
		FillTimer = 0.f;
	}
	else if (Fall <= 0.05f)
	{
		FillTimer = 0.f;
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
	if (UMaterialParameterCollection* MPC = LoadObject<UMaterialParameterCollection>(nullptr, SnowNearCollectionPath))
	{
		UKismetMaterialLibrary::SetVectorParameterValue(World, MPC, TEXT("NearRect"), FLinearColor(float(C.X), float(C.Y), HalfCm, 600.f));
		UKismetMaterialLibrary::SetVectorParameterValue(World, MPC, TEXT("NearGround"), FLinearColor(float(GroundOrigin.X), float(GroundOrigin.Y), GroundCm, float(GroundN)));
		UKismetMaterialLibrary::SetVectorParameterValue(World, MPC, TEXT("NearTrail"), FLinearColor(TrailCm, TrailCm * TrailN, 0.f, 0.f));
	}
}
