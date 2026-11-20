// The winter play's things built in code (ULyudWinterPlay, docs/winter_play.md "What is built"): the log banya by the pier,
// the walruses' ice hole and its ladder, the cracks on the pond's ice, the skate blades, the snowballs and the snowmen.
// Everything is CityKit meshes on the one city material (colours in the vertex colour); built once, rebuilt only on a
// change (the cracks when the winter or the thickness band changes). Lengths here are metres in each mesh's own frame.

#include "Winter/LyudWinterPlay.h"

#include "WinterPlayModel.h"

#include "City/CityKit.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "World/LyudLadder.h"

using namespace LyudWinterPlay;

namespace
{
	FVector Cm3(const V3& M) { return FVector(M.X * 100.0, M.Y * 100.0, M.Z * 100.0); }

	CityKit::FSkin Sk(uint8 R, uint8 G, uint8 B, float Rough, CityKit::EPattern Pattern = CityKit::EPattern::None, float Tile = 0.f, float Glow = 0.f)
	{
		CityKit::FSkin S;
		S.Color = CityKit::Srgb(R, G, B);
		S.Rough = Rough;
		S.Pattern = Pattern;
		S.Tile = Tile;
		S.Glow = Glow;
		return S;
	}

	/** A small deterministic jitter (the logs' tones, the cracks' zigzags, the rim's chunks). */
	float Jit(uint32 Seed)
	{
		Seed = (Seed ^ 61u) ^ (Seed >> 16);
		Seed *= 9u;
		Seed ^= Seed >> 4;
		Seed *= 0x27d4eb2du;
		Seed ^= Seed >> 15;
		return (Seed & 0xffffu) / 65535.f;
	}

	/** A log's skin: weathered pine, a little different log to log. */
	CityKit::FSkin LogSkin(int32 I)
	{
		const float J = Jit(1000u + (uint32)I) * 18.f;
		return Sk((uint8)(118 + J), (uint8)(86 + J * 0.8f), (uint8)(56 + J * 0.5f), 0.85f);
	}

	const float LogR = 0.12f;      // a log's radius, m

	/** A run of logs along one axis (bX: along X at Y = Fixed; else along Y at X = Fixed) at height Z, from A to B, broken by
	 *  the gaps (from, to along the run) whose height band (z0, z1) covers Z. */
	void LogRun(CityKit::FMesh& M, bool bX, float Fixed, float A, float B, float Z, const TArray<FVector4f>& Gaps, int32 Seed)
	{
		TArray<FVector2f> Spans;
		Spans.Add(FVector2f(A, B));
		for (const FVector4f& G : Gaps)
		{
			if (Z < G.Z || Z > G.W)
			{
				continue;
			}
			TArray<FVector2f> Next;
			for (const FVector2f& S : Spans)
			{
				if (G.Y <= S.X || G.X >= S.Y)
				{
					Next.Add(S);
					continue;
				}
				if (G.X - S.X > 0.05f) Next.Add(FVector2f(S.X, G.X));
				if (S.Y - G.Y > 0.05f) Next.Add(FVector2f(G.Y, S.Y));
			}
			Spans = Next;
		}
		int32 K = 0;
		for (const FVector2f& S : Spans)
		{
			const FVector P0 = bX ? FVector(S.X, Fixed, Z) : FVector(Fixed, S.X, Z);
			const FVector P1 = bX ? FVector(S.Y, Fixed, Z) : FVector(Fixed, S.Y, Z);
			M.Tube(P0, P1, LogR, LogR * 0.96f, 8, LogSkin(Seed * 7 + K++), true, true);
		}
	}

	/** A gable's triangle of boards (the end of the roof over a wall at X), drawn both ways. */
	void Gable(CityKit::FMesh& M, float X, float HalfW, float Z0, float Z1, const CityKit::FSkin& S)
	{
		for (const float Side : { 1.f, -1.f })
		{
			const FVector Out(Side, 0.f, 0.f);
			const int32 A = M.Vert(FVector(X, -HalfW, Z0), Out, S, FVector2f(-HalfW, Z0));
			const int32 B = M.Vert(FVector(X, HalfW, Z0), Out, S, FVector2f(HalfW, Z0));
			const int32 C = M.Vert(FVector(X, 0.f, Z1), Out, S, FVector2f(0.f, Z1));
			M.Tri(A, B, C, Out);
		}
	}

	/** A quad drawn on both faces (roof boards seen from under the eaves). */
	void Quad2(CityKit::FMesh& M, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const CityKit::FSkin& Top, const CityKit::FSkin& Under)
	{
		M.Quad(A, B, C, D, Top);
		M.Quad(D, C, B, A, Under);
	}
}

void ULyudWinterPlay::Build()
{
	bBuilt = true;
	UWorld* World = GetWorld();
	if (!World || !M)
	{
		return;
	}
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SP.ObjectFlags |= RF_Transient;
	Holder = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SP);
	if (!Holder)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("WinterPlayRoot"));
	Root->SetMobility(EComponentMobility::Movable);
	Holder->SetRootComponent(Root);
	Root->RegisterComponent();
#if WITH_EDITOR
	Holder->SetActorLabel(TEXT("WinterPlay"));
#endif
	PropMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/City/M_CityProp.M_CityProp"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	GlowMid = PropMaterial ? UMaterialInstanceDynamic::Create(PropMaterial, this) : nullptr;
	if (GlowMid)
	{
		GlowMid->SetScalarParameterValue(TEXT("Glow"), 0.f);
	}

	// the banya sits on the sand under the seawall: its floor a log's height over the ground the trace finds
	{
		const FVector At = Cm3(M->P.BanyaAt);
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WinterPlayBanyaGround), false);
		if (World->LineTraceSingleByChannel(Hit, At + FVector(0.0, 0.0, 400.0), At - FVector(0.0, 0.0, 600.0), ECC_WorldStatic, Q))
		{
			M->P.BanyaAt.Z = Hit.ImpactPoint.Z / 100.0 + 0.15;
		}
		else
		{
			M->P.BanyaAt.Z += 0.15;
		}
	}
	BuildBanya();
	BuildHole();
	BuildBallMeshes();
	BuildCracks();
}

void ULyudWinterPlay::BuildBanya()
{
	using namespace CityKit;
	const FParams& P = M->P;
	const float HX = (float)P.BanyaSize[0] * 0.5f, HY = (float)P.BanyaSize[1] * 0.5f, H = (float)P.BanyaSize[2];
	const float DoorY = (float)P.Door[1], DoorW = 0.8f, DoorH = 1.75f;
	const float PartX = (float)(P.SteamC[0] + P.SteamHalf[0]) + 0.06f;       // the partition between the steam room and the dressing room
	const float WinX = (float)P.DressC[0];                                   // the dressing room's window, on the +Y wall

	FMesh B;
	// footing: four thick logs on the sand
	for (const float Y : { -HY + LogR, HY - LogR })
	{
		B.Tube(FVector(-HX - 0.15f, Y, -0.08f), FVector(HX + 0.15f, Y, -0.08f), 0.14f, 0.14f, 8, Sk(84, 66, 48, 0.9f), true, true);
	}
	// the walls: courses of logs, the long walls and the end walls a half log apart (they cross at the corners and stick out)
	TArray<FVector4f> FrontGaps, SideGaps, NoGaps;
	FrontGaps.Add(FVector4f(DoorY - DoorW * 0.5f, DoorY + DoorW * 0.5f, 0.f, DoorH));
	SideGaps.Add(FVector4f(WinX - 0.32f, WinX + 0.32f, 0.95f, 1.5f));
	const int32 Courses = FMath::Max(4, FMath::RoundToInt(H / (LogR * 2.f)));
	for (int32 C = 0; C < Courses; C++)
	{
		const float Zs = LogR + C * LogR * 2.f;          // the side walls' (along X) course
		const float Ze = Zs + LogR;                       // the end walls' (along Y) course, half a log up
		LogRun(B, true, -HY + LogR, -HX - 0.18f, HX + 0.18f, Zs, NoGaps, C * 4 + 0);
		LogRun(B, true, HY - LogR, -HX - 0.18f, HX + 0.18f, Zs, SideGaps, C * 4 + 1);
		if (Ze < H)
		{
			LogRun(B, false, HX - LogR, -HY - 0.18f, HY + 0.18f, Ze, FrontGaps, C * 4 + 2);
			LogRun(B, false, -HX + LogR, -HY - 0.18f, HY + 0.18f, Ze, NoGaps, C * 4 + 3);
		}
	}
	// the door's frame and the door itself (shut: he goes in through the fade), a lintel log over it
	const FSkin Plank = Sk(132, 98, 64, 0.8f, EPattern::Boards, 0.14f);
	const FSkin Dark = Sk(70, 52, 36, 0.9f);
	const float FX = HX - LogR;
	B.Box(FVector(FX + 0.02f, DoorY, DoorH * 0.5f), FVector(0.03f, DoorW * 0.5f - 0.03f, DoorH * 0.5f), Plank);
	for (const float S : { -1.f, 1.f })
	{
		B.Box(FVector(FX, DoorY + S * (DoorW * 0.5f + 0.02f), DoorH * 0.5f), FVector(0.13f, 0.04f, DoorH * 0.5f), Dark);
	}
	B.Bar(FVector(FX + 0.06f, DoorY + DoorW * 0.3f, 0.95f), FVector(FX + 0.06f, DoorY + DoorW * 0.3f, 1.12f), 0.025f, 0.025f, Sk(40, 36, 34, 0.5f));
	// the window's frame (its glass is its own part, on the glowing material)
	B.Box(FVector(WinX, HY - LogR, 0.93f), FVector(0.36f, 0.14f, 0.025f), Dark);
	B.Box(FVector(WinX, HY - LogR, 1.52f), FVector(0.36f, 0.14f, 0.025f), Dark);
	B.Bar(FVector(WinX, HY - 0.02f, 0.95f), FVector(WinX, HY - 0.02f, 1.5f), 0.03f, 0.03f, Dark);
	// the floor (boards), the ceiling, the partition with its inner door
	B.Box(FVector(0.f, 0.f, -0.02f), FVector(HX - 0.2f, HY - 0.2f, 0.02f), Sk(150, 116, 78, 0.75f, EPattern::Boards, 0.12f));
	B.Quad(FVector(-HX + 0.2f, -HY + 0.2f, H - 0.02f), FVector(-HX + 0.2f, HY - 0.2f, H - 0.02f), FVector(HX - 0.2f, HY - 0.2f, H - 0.02f),
		FVector(HX - 0.2f, -HY + 0.2f, H - 0.02f), Sk(146, 112, 76, 0.8f, EPattern::Boards, 0.12f));
	{
		const float Y0 = -HY + 0.2f, Y1 = HY - 0.2f, G0 = DoorY - 0.35f, G1 = DoorY + 0.35f;
		const FSkin Lining = Sk(170, 134, 92, 0.7f, EPattern::Boards, 0.1f);    // linden lining, lighter
		B.Box(FVector(PartX, (Y0 + G0) * 0.5f, H * 0.5f), FVector(0.04f, (G0 - Y0) * 0.5f, H * 0.5f), Lining);
		B.Box(FVector(PartX, (G1 + Y1) * 0.5f, H * 0.5f), FVector(0.04f, (Y1 - G1) * 0.5f, H * 0.5f), Lining);
		B.Box(FVector(PartX, DoorY, (H + 1.7f) * 0.5f), FVector(0.04f, 0.35f, (H - 1.7f) * 0.5f), Lining);
		// the steam room lined with linden boards behind the shelves
		B.Box(FVector(-HX + 0.25f, 0.f, H * 0.5f), FVector(0.02f, HY - 0.25f, H * 0.5f - 0.02f), Lining);
	}
	// the steam room: the upper shelf (the hero's), the lower step, their legs; a bucket and a ladle
	{
		const FSkin Shelf = Sk(186, 150, 104, 0.6f, EPattern::Boards, 0.09f);
		const float SX = (float)P.Shelf[0], SZ = (float)P.Shelf[2];
		const float Back = -HX + 0.27f, Front = SX + 0.35f;
		B.Box(FVector((Back + Front) * 0.5f, 0.f, SZ - 0.025f), FVector((Front - Back) * 0.5f, HY - 0.3f, 0.025f), Shelf);
		B.Box(FVector(Front + 0.2f, 0.f, SZ * 0.5f - 0.025f), FVector(0.2f, HY - 0.3f, 0.025f), Shelf);
		for (const float Y : { -HY + 0.4f, 0.f, HY - 0.4f })
		{
			B.Bar(FVector(Front - 0.05f, Y, 0.f), FVector(Front - 0.05f, Y, SZ - 0.05f), 0.05f, 0.05f, Dark);
			B.Bar(FVector(Front + 0.35f, Y, 0.f), FVector(Front + 0.35f, Y, SZ * 0.5f - 0.05f), 0.05f, 0.05f, Dark);
		}
		const FVector Bucket(Front + 0.55f, -HY + 0.45f, 0.f);
		B.Tube(Bucket, Bucket + FVector(0.f, 0.f, 0.26f), 0.13f, 0.15f, 10, Sk(150, 108, 66, 0.6f), true, false);
		B.Tube(Bucket + FVector(0.f, 0.f, 0.06f), Bucket + FVector(0.f, 0.f, 0.08f), 0.142f, 0.142f, 10, Sk(50, 48, 46, 0.4f), false, false);
		B.Tube(Bucket + FVector(0.f, 0.f, 0.2f), Bucket + FVector(0.f, 0.f, 0.22f), 0.147f, 0.147f, 10, Sk(50, 48, 46, 0.4f), false, false);
		B.Bar(Bucket + FVector(0.f, 0.f, 0.24f), Bucket + FVector(0.32f, 0.12f, 0.36f), 0.025f, 0.02f, Sk(150, 108, 66, 0.6f));
	}
	// the stove: an iron box, the stones heaped on it, the pipe up through the roof; a brick hearth under it
	{
		const FVector S((float)P.Stove[0], (float)P.Stove[1], 0.f);
		const FSkin Iron = Sk(46, 44, 42, 0.45f);
		B.Box(S + FVector(0.f, 0.f, 0.02f), FVector(0.42f, 0.36f, 0.02f), Sk(140, 70, 52, 0.85f, EPattern::Masonry, 0.12f));
		B.Box(S + FVector(0.f, 0.f, 0.4f), FVector(0.3f, 0.24f, 0.36f), Iron);
		B.Box(S + FVector(0.31f, 0.f, 0.26f), FVector(0.015f, 0.12f, 0.1f), Sk(30, 28, 26, 0.4f));      // the firebox door
		B.Box(S + FVector(0.f, 0.f, 0.86f), FVector(0.26f, 0.2f, 0.1f), Sk(60, 58, 56, 0.5f));          // the stones' cage
		for (int32 K = 0; K < 9; K++)
		{
			const FVector At = S + FVector(-0.18f + (K % 3) * 0.18f + Jit(K) * 0.04f, -0.13f + (K / 3) * 0.13f, 0.98f + Jit(K + 40) * 0.04f);
			const uint8 G = (uint8)(78 + Jit(K + 80) * 40.f);
			B.Sphere(At, 0.065f + Jit(K + 20) * 0.025f, 6, Sk(G, (uint8)(G - 4), (uint8)(G - 8), 0.8f));
		}
		B.Tube(S + FVector(-0.15f, 0.1f, 0.96f), S + FVector(-0.15f, 0.1f, H + 1.3f), 0.075f, 0.075f, 10, Iron, false, true);
		B.Tube(S + FVector(-0.15f, 0.1f, H + 1.2f), S + FVector(-0.15f, 0.1f, H + 1.32f), 0.11f, 0.11f, 10, Iron, true, true);
	}
	// the dressing room: a bench along the window wall, a small table, pegs with a towel
	{
		const FSkin Wood = Sk(138, 102, 66, 0.7f);
		const float X0 = PartX + 0.15f, X1 = HX - 0.35f;
		B.Box(FVector((X0 + X1) * 0.5f, HY - 0.45f, 0.44f), FVector((X1 - X0) * 0.5f, 0.2f, 0.03f), Plank);
		for (const float X : { X0 + 0.1f, X1 - 0.1f })
		{
			B.Box(FVector(X, HY - 0.45f, 0.21f), FVector(0.03f, 0.17f, 0.21f), Wood);
		}
		B.Box(FVector(X0 + 0.45f, -HY + 0.55f, 0.7f), FVector(0.3f, 0.3f, 0.025f), Wood);
		B.Bar(FVector(X0 + 0.45f, -HY + 0.55f, 0.f), FVector(X0 + 0.45f, -HY + 0.55f, 0.68f), 0.06f, 0.06f, Wood);
		B.Tube(FVector(X0 + 0.45f, -HY + 0.5f, 0.725f), FVector(X0 + 0.45f, -HY + 0.5f, 0.84f), 0.05f, 0.045f, 8, Sk(200, 196, 186, 0.3f), false, true);   // an enamel mug
		for (int32 K = 0; K < 3; K++)
		{
			const float X = X0 + 0.3f + K * 0.35f;
			B.Bar(FVector(X, HY - 0.26f, 1.6f), FVector(X, HY - 0.4f, 1.62f), 0.025f, 0.025f, Wood);
		}
		B.Box(FVector(X0 + 0.65f, HY - 0.3f, 1.3f), FVector(0.14f, 0.02f, 0.3f), Sk(214, 206, 190, 0.9f));       // a towel on its peg
	}
	// the roof: boards over a gable along X, eaves out over the walls, longer over the porch; the gables' boards; the ridge
	{
		const FSkin Roof = Sk(88, 82, 76, 0.85f, EPattern::Boards, 0.18f), Under = Sk(112, 84, 58, 0.85f, EPattern::Boards, 0.14f);
		const float Eave = 0.45f, X0 = -HX - 0.4f, X1 = HX + 1.25f, Z0 = H + 0.02f, Z1 = H + 1.05f, Y = HY + Eave, Ze = Z0 - Eave * (Z1 - Z0) / HY;
		Quad2(B, FVector(X0, -Y, Ze), FVector(X1, -Y, Ze), FVector(X1, 0.f, Z1), FVector(X0, 0.f, Z1), Roof, Under);
		Quad2(B, FVector(X1, Y, Ze), FVector(X0, Y, Ze), FVector(X0, 0.f, Z1), FVector(X1, 0.f, Z1), Roof, Under);
		Gable(B, HX - LogR, HY, Z0, Z1, Plank);
		Gable(B, -HX + LogR, HY, Z0, Z1, Plank);
		B.Bar(FVector(X0, 0.f, Z1 + 0.03f), FVector(X1, 0.f, Z1 + 0.03f), 0.1f, 0.06f, Sk(66, 60, 56, 0.8f));
		// the porch's two posts under the long eave
		for (const float S : { -1.f, 1.f })
		{
			B.Bar(FVector(HX + 1.05f, S * (HY - 0.15f), 0.f), FVector(HX + 1.05f, S * (HY - 0.15f), Ze + (HY + Eave - (HY - 0.15f)) * (Z1 - Z0) / HY - 0.08f), 0.11f, 0.11f, Dark);
		}
	}
	// the porch: boards on joists before the door, two steps down to the sand; a woodpile along the back wall
	{
		B.Box(FVector(HX + 0.6f, 0.f, -0.03f), FVector(0.55f, HY, 0.03f), Plank);
		B.Box(FVector(HX + 1.32f, 0.f, -0.09f), FVector(0.17f, HY * 0.6f, 0.03f), Plank);
		const FSkin Fire = Sk(150, 118, 84, 0.85f);
		for (int32 R = 0; R < 4; R++)
		{
			for (int32 K = 0; K < 8 - R; K++)
			{
				const float Y = -HY + 0.3f + (K + R * 0.5f) * 0.21f, Z = -0.05f + 0.1f + R * 0.18f;
				B.Tube(FVector(-HX - 0.75f, Y, Z), FVector(-HX - 0.35f, Y, Z), 0.09f, 0.09f, 6, Fire, true, true);
			}
		}
	}
	// the player's collision: the four walls, the porch, the stove's pipe left out (it is inside); the woodpile
	B.Collide(FVector(HX - LogR, 0.f, H * 0.5f), FVector(LogR + 0.02f, HY + 0.2f, H * 0.5f));
	B.Collide(FVector(-HX + LogR, 0.f, H * 0.5f), FVector(LogR + 0.02f, HY + 0.2f, H * 0.5f));
	B.Collide(FVector(0.f, HY - LogR, H * 0.5f), FVector(HX + 0.2f, LogR + 0.02f, H * 0.5f));
	B.Collide(FVector(0.f, -HY + LogR, H * 0.5f), FVector(HX + 0.2f, LogR + 0.02f, H * 0.5f));
	B.Collide(FVector(HX + 0.6f, 0.f, -0.06f), FVector(0.55f, HY, 0.06f));
	B.Collide(FVector(-HX - 0.55f, 0.f, 0.35f), FVector(0.22f, HY - 0.15f, 0.4f));

	const FTransform Xf(FRotator(0.f, (float)P.BanyaYaw, 0.f), Cm3(P.BanyaAt));
	if (UStaticMesh* Mesh = B.Build(this, TEXT("SM_WinterBanya"), true))
	{
		Meshes.Add(Mesh);
		MakePart(Mesh, TEXT("Banya"), true, Xf, 40000.f);
	}
	// the window's glass: lit from inside after dark (the glowing material's Glow)
	{
		FMesh G;
		const FSkin Glass = Sk(255, 196, 120, 0.2f, EPattern::None, 0.f, 1.f);
		G.Quad(FVector(WinX - 0.3f, HY - 0.05f, 0.97f), FVector(WinX + 0.3f, HY - 0.05f, 0.97f), FVector(WinX + 0.3f, HY - 0.05f, 1.49f),
			FVector(WinX - 0.3f, HY - 0.05f, 1.49f), Glass);
		if (UStaticMesh* Mesh = G.Build(this, TEXT("SM_WinterBanyaWindow"), false))
		{
			Meshes.Add(Mesh);
			if (UStaticMeshComponent* C = MakePart(Mesh, TEXT("BanyaWindow"), false, Xf, 40000.f))
			{
				if (GlowMid)
				{
					C->SetMaterial(0, GlowMid);
				}
				C->SetCastShadow(false);
			}
		}
	}
	// the seat on the upper shelf: on the floor under the shelf's front edge, facing the stove (+X)
	ShelfSeat = NewObject<USceneComponent>(Holder, TEXT("BanyaShelfSeat"));
	ShelfSeat->SetMobility(EComponentMobility::Movable);
	ShelfSeat->SetupAttachment(Holder->GetRootComponent());
	{
		const double Front[3] = { P.Shelf[0] + 0.33, P.Shelf[1], 0.0 };
		ShelfSeat->SetWorldLocationAndRotation(BanyaPoint(Front), FRotator(0.f, (float)P.BanyaYaw, 0.f));
	}
	ShelfSeat->RegisterComponent();
	Holder->AddInstanceComponent(ShelfSeat);
	// the lights: the steam room's (warm and dim, by the ceiling), the stove's glow (low, orange), the dressing room's
	auto Light = [this](const FString& Name, const FVector& At, float Lumens, float Radius, const FLinearColor& Color)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(Holder, *Name);
		L->SetMobility(EComponentMobility::Movable);
		L->SetCastShadows(false);
		L->SetIntensityUnits(ELightUnits::Lumens);
		L->SetIntensity(Lumens);
		L->SetAttenuationRadius(Radius);
		L->SetSourceRadius(8.f);
		L->SetLightColor(Color);
		L->SetVisibility(false);
		L->SetupAttachment(Holder->GetRootComponent());
		L->SetWorldLocation(At);
		L->RegisterComponent();
		Holder->AddInstanceComponent(L);
		Lights.Add(L);
	};
	{
		const double SteamLamp[3] = { P.SteamC[0] + 0.3, P.SteamC[1] - 0.8, P.BanyaSize[2] - 0.25 };
		const double StoveGlow[3] = { P.Stove[0] + 0.45, P.Stove[1], 0.3 };
		const double DressLamp[3] = { P.DressC[0], P.DressC[1], P.BanyaSize[2] - 0.2 };
		Light(TEXT("BanyaSteamLight"), BanyaPoint(SteamLamp), 320.f, 420.f, FLinearColor(1.f, 0.72f, 0.45f));
		Light(TEXT("BanyaStoveGlow"), BanyaPoint(StoveGlow), 160.f, 260.f, FLinearColor(1.f, 0.42f, 0.16f));
		Light(TEXT("BanyaDressLight"), BanyaPoint(DressLamp), 420.f, 520.f, FLinearColor(1.f, 0.8f, 0.58f));
	}
}

void ULyudWinterPlay::BuildHole()
{
	using namespace CityKit;
	const FParams& P = M->P;
	UWorld* World = GetWorld();
	const FVector Hole = Cm3(P.HoleAt);
	// the hole's things ride the ice's top (TickHole moves this root with the tide); hidden until the sea is frozen
	USceneComponent* HoleRoot = NewObject<USceneComponent>(Holder, TEXT("IceHoleRoot"));
	HoleRoot->SetMobility(EComponentMobility::Movable);
	HoleRoot->SetupAttachment(Holder->GetRootComponent());
	HoleRoot->SetWorldLocation(Hole);
	HoleRoot->RegisterComponent();
	Holder->AddInstanceComponent(HoleRoot);
	const float Half = 1.5f;     // one 3 m tile of the sea ice
	{
		FMesh R;
		// the rim: broken ice heaped along the cut, chunks a little off square, and a scraped ring of snow round it
		const FSkin Ice = Sk(196, 222, 232, 0.25f), Snow = Sk(236, 240, 244, 0.85f), Board = Sk(110, 84, 58, 0.85f, EPattern::Boards, 0.14f);
		int32 K = 0;
		for (int32 Side = 0; Side < 4; Side++)
		{
			for (int32 I = 0; I < 6; I++, K++)
			{
				const float T = -Half + (I + 0.5f) * (2.f * Half / 6.f) + (Jit(K * 3) - 0.5f) * 0.12f;
				const float O = Half + 0.12f + Jit(K * 3 + 1) * 0.12f;
				const FVector2f Pt = Side == 0 ? FVector2f(T, -O) : Side == 1 ? FVector2f(O, T) : Side == 2 ? FVector2f(-T, O) : FVector2f(-O, -T);
				const FVector Sz(0.12f + Jit(K * 3 + 2) * 0.1f, 0.08f + Jit(K * 5) * 0.08f, 0.03f + Jit(K * 7) * 0.05f);
				R.Box(FVector(Pt.X, Pt.Y, Sz.Z * 0.6f), Sz, Ice, FRotator(Jit(K * 11) * 14.f - 7.f, Jit(K * 13) * 90.f, Jit(K * 17) * 14.f - 7.f));
			}
		}
		R.Disc(FVector(0.f, 0.f, 0.012f), Half + 0.3f, Half + 1.4f, 24, Snow, Half + 1.4f);
		// boards laid along two sides to stand on (the club's), and one across the pier's side under the ladder
		for (const float S : { -1.f, 1.f })
		{
			R.Box(FVector(S * (Half + 0.35f), 0.f, 0.03f), FVector(0.14f, Half + 0.25f, 0.025f), Board);
		}
		// slush in the open water: a few flat floes a little under the ice's top
		for (int32 I = 0; I < 7; I++)
		{
			const FVector At(-1.1f + Jit(200 + I) * 2.2f, -1.1f + Jit(300 + I) * 2.2f, -0.13f);
			R.Box(At, FVector(0.08f + Jit(400 + I) * 0.12f, 0.06f + Jit(500 + I) * 0.1f, 0.012f), Ice, FRotator(0.f, Jit(600 + I) * 180.f, 0.f));
		}
		if (UStaticMesh* Mesh = R.Build(this, TEXT("SM_WinterIceHole"), false))
		{
			Meshes.Add(Mesh);
			if (UStaticMeshComponent* C = MakePart(Mesh, TEXT("IceHole"), false, FTransform(Hole), 25000.f))
			{
				C->AttachToComponent(HoleRoot, FAttachmentTransformRules::KeepWorldTransform);
				C->SetVisibility(false);
				HoleParts.Add(C);
			}
		}
	}
	// the ladder: in the hole at its pier side, its foot on the bed, hooked over the deck's edge; the climb steps off onto
	// the deck (there all year: in summer it is the swimmers' ladder off the pier)
	if (World)
	{
		const FVector Foot = Cm3(P.LadderAt);
		float DeckTop = (float)P.DeckZ * 100.f;
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WinterPlayDeck), false);
		const FVector OnDeck(Foot.X, P.DeckEdgeY * 100.0 - 80.0, DeckTop);
		if (World->LineTraceSingleByChannel(Hit, OnDeck + FVector(0.0, 0.0, 300.0), OnDeck - FVector(0.0, 0.0, 300.0), ECC_WorldStatic, Q))
		{
			DeckTop = Hit.ImpactPoint.Z;
		}
		const float Rise = FMath::Max(120.f, DeckTop - Foot.Z);
		const FTransform LXf(FRotator(0.f, (float)P.LadderYaw, 0.f), Foot);
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SP.bDeferConstruction = true;
		SP.ObjectFlags |= RF_Transient;
		Ladder = World->SpawnActor<ALyudLadder>(ALyudLadder::StaticClass(), LXf, SP);
		if (Ladder)
		{
			ULyudLadderComponent* L = Ladder->Ladder;
			L->RungSpacing = 30.f;
			L->FirstRungZ = 25.f;
			L->RungCount = FMath::Max(3, FMath::FloorToInt((Rise - L->FirstRungZ) / L->RungSpacing) + 1);
			L->TopExit = FVector(-(float)P.LadderTopBack * 100.f, 0.f, Rise);
			L->bInteractive = true;
			Ladder->Material = PropMaterial;
			Ladder->FinishSpawning(LXf);
			// two hooks from the rails' tops over the deck's edge
			FMesh Hk;
			const FSkin Iron = Sk(60, 58, 56, 0.45f);
			const float Top = (L->TopRungZ() + L->FirstRungZ) / 100.f, Back = (float)P.LadderTopBack - 0.5f;
			for (const float S : { -1.f, 1.f })
			{
				const float Y = S * (L->HalfWidth + 1.5f) / 100.f;
				Hk.Bar(FVector(-0.01f, Y, Top - 0.02f), FVector(-Back, Y, Top + 0.06f), 0.03f, 0.02f, Iron);
				Hk.Bar(FVector(-Back, Y, Top + 0.06f), FVector(-Back - 0.08f, Y, Top - 0.04f), 0.025f, 0.02f, Iron);
			}
			if (UStaticMesh* Mesh = Hk.Build(this, TEXT("SM_WinterLadderHooks"), false))
			{
				Meshes.Add(Mesh);
				MakePart(Mesh, TEXT("LadderHooks"), false, LXf, 20000.f);
			}
		}
	}
}

void ULyudWinterPlay::BuildCracks()
{
	using namespace CityKit;
	if (!Holder || !M)
	{
		return;
	}
	const FParams& P = M->P;
	UStaticMesh* Old = CrackPart ? CrackPart->GetStaticMesh() : nullptr;
	if (M->Cracks.empty())
	{
		if (CrackPart)
		{
			CrackPart->SetVisibility(false);
		}
		return;
	}
	// each crack a zigzag of thin dark quads along its segment (within a few cm of it: what the rules test), a pale
	// edge either side, and a short branch or two; flat on the ice a few mm over its top
	FMesh C;
	const FSkin Core = Sk(54, 70, 84, 0.08f), Edge = Sk(214, 230, 238, 0.35f);
	const float Z = 0.016f;
	int32 Seed = CracksWinter * 131 + CracksCount;
	for (const FCrack& Cr : M->Cracks)
	{
		const FVector2D A(Cr.A.X, Cr.A.Y), B(Cr.B.X, Cr.B.Y);
		const FVector2D Dir = (B - A).GetSafeNormal(), Side(-Dir.Y, Dir.X);
		const double Len = FVector2D::Distance(A, B);
		const int32 N = FMath::Clamp(FMath::RoundToInt(Len / 0.9), 3, 18);
		const float W = FMath::Clamp((float)Cr.Width * 0.12f, 0.018f, 0.07f);
		TArray<FVector2D> Pts;
		for (int32 I = 0; I <= N; I++)
		{
			const double T = (double)I / N;
			const double Off = (I == 0 || I == N) ? 0.0 : (Jit((uint32)(Seed++)) - 0.5) * FMath::Min(0.22, Cr.Width * 0.4);
			Pts.Add(A + (B - A) * T + Side * Off);
		}
		auto Strip = [&C, Z](const FVector2D& P0, const FVector2D& P1, float W0, float W1, const FSkin& S, float Dz)
		{
			const FVector2D D = (P1 - P0).GetSafeNormal(), Sd(-D.Y, D.X);
			C.Quad(FVector(P0.X - Sd.X * W0, P0.Y - Sd.Y * W0, Z + Dz), FVector(P1.X - Sd.X * W1, P1.Y - Sd.Y * W1, Z + Dz),
				FVector(P1.X + Sd.X * W1, P1.Y + Sd.Y * W1, Z + Dz), FVector(P0.X + Sd.X * W0, P0.Y + Sd.Y * W0, Z + Dz), S);
		};
		for (int32 I = 0; I < N; I++)
		{
			const float T0 = (float)I / N, T1 = (float)(I + 1) / N;
			// thinner at the ends
			const float W0 = W * FMath::Sin(PI * FMath::Lerp(0.08f, 0.92f, T0)), W1 = W * FMath::Sin(PI * FMath::Lerp(0.08f, 0.92f, T1));
			Strip(Pts[I], Pts[I + 1], W0 * 2.6f, W1 * 2.6f, Edge, 0.f);
			Strip(Pts[I], Pts[I + 1], W0, W1, Core, 0.002f);
			if (Jit((uint32)(Seed++)) < 0.18f && I > 0 && I < N - 1)
			{
				const double Turn = (Jit((uint32)(Seed++)) < 0.5f ? 1.0 : -1.0) * (0.5 + Jit((uint32)(Seed++)) * 0.5);
				const FVector2D Br = (Dir * FMath::Cos(Turn) + Side * FMath::Sin(Turn)).GetSafeNormal();
				const FVector2D E = Pts[I] + Br * (0.4 + Jit((uint32)(Seed++)) * 0.8);
				Strip(Pts[I], E, W0 * 0.6f, 0.004f, Core, 0.002f);
			}
		}
	}
	// the pond's frame to the world: its centre, turned by its yaw
	const FTransform Xf(FRotator(0.f, (float)P.RinkYaw, 0.f), Cm3(P.RinkAt));
	UStaticMesh* Mesh = C.Build(this, *FString::Printf(TEXT("SM_WinterCracks_%d_%d"), CracksWinter, CracksCount), false);
	if (!Mesh)
	{
		return;
	}
	Meshes.Add(Mesh);
	if (!CrackPart)
	{
		CrackPart = MakePart(Mesh, TEXT("PondCracks"), false, Xf, 15000.f);
		if (CrackPart)
		{
			CrackPart->SetCastShadow(false);
		}
	}
	else
	{
		CrackPart->SetStaticMesh(Mesh);
		CrackPart->SetWorldTransform(Xf);
	}
	// the old set let go (perf checklist: free old meshes)
	if (Old && Old != Mesh)
	{
		Meshes.Remove(Old);
		Old->MarkAsGarbage();
	}
}

void ULyudWinterPlay::BuildBallMeshes()
{
	using namespace CityKit;
	const FParams& P = M->P;
	const FSkin Snow = Sk(240, 244, 248, 0.8f), Packed = Sk(226, 232, 238, 0.7f);
	// a snowball, a crumb of a burst, the unit ball (1 m radius: the rolled ball and the snowmen's, scaled), a carrot, a coal
	// eye, a twig arm; the skate blade
	{
		FMesh S;
		S.Sphere(FVector::ZeroVector, (float)P.BallRadius, 10, Snow);
		BallMesh = S.Build(this, TEXT("SM_WinterSnowball"), false);
		Meshes.Add(BallMesh);
	}
	UStaticMesh* Crumb = nullptr;
	{
		FMesh S;
		S.Sphere(FVector::ZeroVector, 0.03f, 6, Snow);
		Crumb = S.Build(this, TEXT("SM_WinterCrumb"), false);
		Meshes.Add(Crumb);
	}
	{
		FMesh S;
		S.Sphere(FVector::ZeroVector, 1.f, 18, Packed);
		// a few lumps on it, so its turning shows as it rolls
		for (int32 K = 0; K < 10; K++)
		{
			const FVector D = FVector(Jit(K) - 0.5f, Jit(K + 30) - 0.5f, Jit(K + 60) - 0.5f).GetSafeNormal();
			S.Sphere(D * 0.93f, 0.16f + Jit(K + 90) * 0.1f, 6, Snow);
		}
		S.Collide(FVector::ZeroVector, FVector(0.72f, 0.72f, 0.8f));
		SnowSphere = S.Build(this, TEXT("SM_WinterSnowSphere"), true);
		Meshes.Add(SnowSphere);
	}
	{
		FMesh S;
		S.Tube(FVector::ZeroVector, FVector(0.16f, 0.f, 0.f), 0.024f, 0.003f, 8, Sk(236, 112, 26, 0.6f), true, true);
		NoseMesh = S.Build(this, TEXT("SM_WinterCarrot"), false);
		Meshes.Add(NoseMesh);
	}
	{
		FMesh S;
		S.Sphere(FVector::ZeroVector, 0.022f, 6, Sk(18, 18, 20, 0.5f));
		EyeMesh = S.Build(this, TEXT("SM_WinterCoal"), false);
		Meshes.Add(EyeMesh);
	}
	{
		FMesh S;
		const FSkin Twig = Sk(78, 58, 40, 0.9f);
		S.Bar(FVector::ZeroVector, FVector(0.f, 0.5f, 0.08f), 0.02f, 0.02f, Twig);
		S.Bar(FVector(0.f, 0.32f, 0.05f), FVector(0.f, 0.44f, 0.2f), 0.012f, 0.012f, Twig);
		S.Bar(FVector(0.f, 0.5f, 0.08f), FVector(0.03f, 0.6f, 0.06f), 0.01f, 0.01f, Twig);
		S.Bar(FVector(0.f, 0.5f, 0.08f), FVector(-0.02f, 0.58f, 0.16f), 0.01f, 0.01f, Twig);
		ArmMesh = S.Build(this, TEXT("SM_WinterTwig"), false);
		Meshes.Add(ArmMesh);
	}
	// the pools: snowballs in flight, the crumbs of their bursts, the ball being rolled (none collide; all move)
	for (int32 I = 0; I < 16; I++)
	{
		if (UStaticMeshComponent* C = MakePart(BallMesh, FString::Printf(TEXT("Snowball%d"), I), false, FTransform::Identity, 9000.f, true))
		{
			C->SetVisibility(false);
			BallPool.Add(C);
		}
	}
	for (int32 I = 0; I < 12; I++)
	{
		if (UStaticMeshComponent* C = MakePart(Crumb, FString::Printf(TEXT("SnowCrumb%d"), I), false, FTransform::Identity, 5000.f, true))
		{
			C->SetVisibility(false);
			PuffPool.Add(C);
		}
	}
	RollPart = MakePart(SnowSphere, TEXT("RolledBall"), false, FTransform::Identity, 12000.f, true);
	if (RollPart)
	{
		RollPart->SetVisibility(false);
		RollPart->SetCastShadow(true);      // big enough to want its shadow; it is out of the distance fields all the same
	}
	// the skates' blades, under the hero's shoes while he wears them on the ice (ShowBlades moves them)
	{
		FMesh S;
		const FSkin Steel = Sk(196, 200, 206, 0.22f), Holder_ = Sk(30, 30, 32, 0.5f);
		S.Box(FVector(0.01f, 0.f, -0.052f), FVector(0.15f, 0.0025f, 0.013f), Steel);
		S.Bar(FVector(0.16f, 0.f, -0.046f), FVector(0.185f, 0.f, -0.03f), 0.005f, 0.012f, Steel);       // the toe's curl
		for (const float X : { -0.1f, 0.1f })
		{
			S.Box(FVector(X, 0.f, -0.022f), FVector(0.025f, 0.012f, 0.02f), Holder_);
		}
		S.Box(FVector(0.f, 0.f, -0.004f), FVector(0.13f, 0.045f, 0.004f), Holder_);      // the plate under the sole
		if (UStaticMesh* Blade = S.Build(this, TEXT("SM_WinterBlade"), false))
		{
			Meshes.Add(Blade);
			BladeL = MakePart(Blade, TEXT("BladeL"), false, FTransform::Identity, 4000.f, true);
			BladeR = MakePart(Blade, TEXT("BladeR"), false, FTransform::Identity, 4000.f, true);
			if (BladeL) BladeL->SetVisibility(false);
			if (BladeR) BladeR->SetVisibility(false);
			bBladesShown = false;
		}
	}
	// the saved snowmen
	for (int32 K = 0; K < (int32)M->Save.Snowmen.size(); K++)
	{
		ShowSnowman(K);
	}
}

void ULyudWinterPlay::ShowSnowman(int32 Index)
{
	if (!M || !Holder || !SnowSphere || Index < 0 || Index >= (int32)M->Save.Snowmen.size())
	{
		return;
	}
	// each snowman has a slice of 8 pooled parts (three balls, the nose, two eyes, two arms), made on first need and moved
	// only when it changes (a ball put on, dressed, the thaw's slump)
	const int32 PerMan = 8;
	while (ManSlices.Num() <= Index)
	{
		const int32 First = ManParts.Num();
		const int32 N = ManSlices.Num();
		for (int32 B = 0; B < 3; B++)
		{
			ManParts.Add(MakePart(SnowSphere, FString::Printf(TEXT("Snowman%d_Ball%d"), N, B), B == 0, FTransform::Identity, 15000.f));
		}
		ManParts.Add(MakePart(NoseMesh, FString::Printf(TEXT("Snowman%d_Nose"), N), false, FTransform::Identity, 5000.f));
		ManParts.Add(MakePart(EyeMesh, FString::Printf(TEXT("Snowman%d_EyeL"), N), false, FTransform::Identity, 4000.f));
		ManParts.Add(MakePart(EyeMesh, FString::Printf(TEXT("Snowman%d_EyeR"), N), false, FTransform::Identity, 4000.f));
		ManParts.Add(MakePart(ArmMesh, FString::Printf(TEXT("Snowman%d_ArmL"), N), false, FTransform::Identity, 6000.f));
		ManParts.Add(MakePart(ArmMesh, FString::Printf(TEXT("Snowman%d_ArmR"), N), false, FTransform::Identity, 6000.f));
		ManSlices.Add(TPair<int32, int32>(First, PerMan));
	}
	const FSnowman& Sm = M->Save.Snowmen[(size_t)Index];
	const int32 First = ManSlices[Index].Key;
	auto Part = [this, First](int32 I) -> UStaticMeshComponent* { return ManParts.IsValidIndex(First + I) ? ManParts[First + I].Get() : nullptr; };
	const FVector Foot = Cm3(Sm.At);
	const FRotator Face(0.f, (float)Sm.Yaw, 0.f);
	const FVector Fwd = Face.Vector(), Right = FRotator(0.f, (float)Sm.Yaw + 90.f, 0.f).Vector();
	// the balls stacked, each sunk a little into the one under it; the thaw squats them
	const float Squash = 1.f - 0.3f * (float)Sm.Slump;
	float Z = 0.f;
	FVector Top = Foot;
	float TopR = 0.f;
	for (int32 B = 0; B < 3; B++)
	{
		UStaticMeshComponent* C = Part(B);
		if (!C)
		{
			continue;
		}
		const float R = (float)Sm.R[B] * 100.f;
		if (B >= Sm.Balls || R <= 1.f)
		{
			C->SetVisibility(false);
			continue;
		}
		Z += B == 0 ? R * 0.9f * Squash : (TopR * 0.78f + R * 0.82f) * Squash;
		const FVector At = Foot + FVector(0.0, 0.0, Z);
		C->SetWorldLocationAndRotation(At, Face);
		C->SetWorldScale3D(FVector(R / 100.f, R / 100.f, R / 100.f * Squash));
		C->SetVisibility(true);
		Top = At;
		TopR = R;
	}
	// dressed only when all three are on: the nose and the eyes on the head's face, the arms in the middle ball
	const bool bDone = Sm.Done();
	const FVector Head = Top;
	const float HR = TopR;
	if (UStaticMeshComponent* Nose = Part(3))
	{
		const bool bShow = bDone && Sm.bNose;
		Nose->SetVisibility(bShow);
		if (bShow)
		{
			Nose->SetWorldLocationAndRotation(Head + Fwd * HR * 0.92f, FRotator(-6.f - 20.f * (float)Sm.Slump, (float)Sm.Yaw, 0.f));
		}
	}
	for (int32 E = 0; E < 2; E++)
	{
		if (UStaticMeshComponent* Eye = Part(4 + E))
		{
			const bool bShow = bDone && Sm.Eyes > E;
			Eye->SetVisibility(bShow);
			if (bShow)
			{
				const FVector Dir = (Fwd * 0.86f + Right * (E ? 0.34f : -0.34f) + FVector(0.0, 0.0, 0.36)).GetSafeNormal();
				Eye->SetWorldLocation(Head + Dir * HR * 0.97f);
			}
		}
	}
	const bool bMid = Sm.Balls >= 2 && Part(1);
	const FVector Mid = bMid ? Part(1)->GetComponentLocation() : Foot;
	const float MR = (float)Sm.R[1] * 100.f;
	for (int32 A = 0; A < 2; A++)
	{
		if (UStaticMeshComponent* Arm = Part(6 + A))
		{
			const bool bShow = bDone && Sm.bArms && bMid;
			Arm->SetVisibility(bShow);
			if (bShow)
			{
				// the twig's +Y out of the ball's side, a little up (droops with the thaw)
				const float Side = A ? 1.f : -1.f;
				const FVector Out = Right * Side;
				const FRotator R = FRotationMatrix::MakeFromYZ(Out, FVector::UpVector).Rotator() + FRotator(0.f, 0.f, Side * (12.f - 30.f * (float)Sm.Slump));
				Arm->SetWorldLocationAndRotation(Mid + Out * MR * 0.8f + FVector(0.0, 0.0, MR * 0.15f), R);
			}
		}
	}
	for (int32 I = 0; I < PerMan; I++)
	{
		if (UStaticMeshComponent* C = Part(I))
		{
			C->SetCastShadow(I < 3);
		}
	}
}

void ULyudWinterPlay::HideSnowmen()
{
	for (UStaticMeshComponent* C : ManParts)
	{
		if (C && C->IsVisible())
		{
			C->SetVisibility(false);
		}
	}
}
