#include "Plants/PlantHerb.h"

#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "Plants/PlantMeshes.h"
#include "Plants/PlantModel.h"
#include "StaticMeshAttributes.h"

namespace
{
	using PlantModel::Hash01;
	using namespace PlantHerbs;
	const FVector3f Up(0.f, 0.f, 1.f);
	constexpr float Golden = 2.39996323f;

	float Smooth(float E0, float E1, float X)
	{
		const float T = FMath::Clamp((X - E0) / FMath::Max(E1 - E0, 1e-4f), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	/** A horizontal side for a direction (cross with up), or by azimuth when the direction is vertical. */
	FVector3f SideOf(const FVector3f& Dir, float Azimuth)
	{
		FVector3f S = FVector3f::CrossProduct(Dir, Up);
		if (S.SizeSquared() < 1e-6f)
		{
			S = FVector3f(-FMath::Sin(Azimuth), FMath::Cos(Azimuth), 0.f);
		}
		return S.GetSafeNormal();
	}

	FVector3f FromAngles(float RiseDeg, float Azimuth)
	{
		const float R = FMath::DegreesToRadians(RiseDeg);
		return FVector3f(FMath::Sin(R) * FMath::Cos(Azimuth), FMath::Sin(R) * FMath::Sin(Azimuth), FMath::Cos(R));
	}

	/** A tube along P (m) with radii R, Sides around (ring frames by parallel transport); Ribs > 0 fluted (a cactus). */
	void Tube(FHerbGeometry& G, int32 Slot, const TArray<FVector3f>& P, const TArray<float>& R, int32 Sides, int32 Ribs = 0, float RibDepth = 0.f, float UVTile = 0.1f)
	{
		const int32 N = P.Num();
		if (N < 2) return;
		FVector3f Dir = (P[1] - P[0]).GetSafeNormal(UE_SMALL_NUMBER, Up);
		FVector3f Nf = SideOf(Dir, 0.f);
		const int32 Base = G.Pos.Num();
		float Arc = 0.f;
		for (int32 I = 0; I < N; I++)
		{
			if (I > 0)
			{
				Arc += FVector3f::Dist(P[I], P[I - 1]);
				Dir = (P[FMath::Min(I + 1, N - 1)] - P[I - 1]).GetSafeNormal(UE_SMALL_NUMBER, Dir);
				Nf = (Nf - Dir * FVector3f::DotProduct(Nf, Dir)).GetSafeNormal(UE_SMALL_NUMBER, SideOf(Dir, 0.f));
			}
			const FVector3f Bf = FVector3f::CrossProduct(Dir, Nf);
			for (int32 S = 0; S <= Sides; S++)
			{
				const float A = 2.f * PI * S / Sides;
				const FVector3f Rad = Nf * FMath::Cos(A) + Bf * FMath::Sin(A);
				const float Flute = Ribs > 0 ? 1.f - RibDepth * (0.5f - 0.5f * FMath::Cos(Ribs * A)) : 1.f;
				G.Add((P[I] + Rad * (R[I] * Flute)) * 100.f, Rad, Bf * FMath::Cos(A) - Nf * FMath::Sin(A),
				      FVector2f(Ribs > 0 ? (float)S / Sides * Ribs : (float)S / Sides, Arc / UVTile));
			}
		}
		for (int32 I = 0; I + 1 < N; I++)
		{
			for (int32 S = 0; S < Sides; S++)
			{
				const int32 A = Base + I * (Sides + 1) + S, B = A + 1, C = A + Sides + 1, E = C + 1;
				G.Tris[Slot].Add(FIntVector3(A, C, B));
				G.Tris[Slot].Add(FIntVector3(B, C, E));
			}
		}
	}

	/** A straight-ish stalk from A along Dir, L long (m), bent down by Arch (rad) over its length; returns its tip and end direction. */
	FVector3f Stalk(FHerbGeometry& G, const FVector3f& A, const FVector3f& Dir, float Azimuth, float L, float Radius, float Arch, FVector3f* OutDir = nullptr)
	{
		const FVector3f Side = SideOf(Dir, Azimuth);
		const FVector3f N0 = FVector3f::CrossProduct(Side, Dir).GetSafeNormal();
		TArray<FVector3f> P;
		TArray<float> R;
		FVector3f At = A;
		FVector3f D = Dir;
		constexpr int32 Steps = 5;
		P.Add(At); R.Add(Radius);
		for (int32 I = 1; I <= Steps; I++)
		{
			const float T = (I - 0.5f) / Steps, Ang = Arch * FMath::Pow(T, 1.3f);
			D = Dir * FMath::Cos(Ang) - N0 * FMath::Sin(Ang);
			At += D * (L / Steps);
			P.Add(At);
			R.Add(Radius * (1.f - 0.4f * I / Steps));
		}
		Tube(G, Stems, P, R, 4);
		if (OutDir) *OutDir = D;
		return At;
	}

	struct FBlade
	{
		float Length = 0.1f, HalfWidth = 0.02f, WidestAt = 0.4f, Power = 1.f;
		int32 Lobes = 0, Teeth = 0;
		float LobeDepth = 0.f, TeethDepth = 0.f, Fold = 0.1f, Thickness = 0.f;
		/** Radians: bending down from base to tip, and twist. */
		float Arch = 0.f, Twist = 0.f;
		bool bRound = false;
		/** Segments along the blade at most (a hedge's thousands of leaves: 2). */
		int32 MaxSamples = 40;
	};

	/** The half-width profile of a blade, 0..1 along it. */
	float Profile(const FBlade& B, float T)
	{
		const float A = FMath::Clamp(B.WidestAt, 0.05f, 0.95f), Al = 2.2f * A, Be = 2.2f * (1.f - A);
		const float Norm = FMath::Pow(A, Al) * FMath::Pow(1.f - A, Be);
		float F = FMath::Pow(FMath::Max(FMath::Pow(T, Al) * FMath::Pow(1.f - T, Be) / Norm, 0.f), B.Power);
		if (B.Lobes > 0) F *= (1.f - B.LobeDepth) + B.LobeDepth * FMath::Sqrt(FMath::Abs(FMath::Sin(PI * B.Lobes * T)));
		if (B.Teeth > 0 && B.Teeth <= 8) F *= 1.f - B.TeethDepth * FMath::Frac(B.Teeth * T);
		return F;
	}

	/**
	 * A leaf blade from Base along Dir (Side across it; its face looks along Side x Dir, up for a rising leaf):
	 * bending down (Arch), twisting, folded along the midrib, thick if succulent. Round blades: a disc through
	 * Base (a violet's, a geranium's), scalloped by Lobes. UVs: u across (0.5 the midrib), v = 1 at the base.
	 */
	void Blade(FHerbGeometry& G, const FVector3f& Base, const FVector3f& Dir, const FVector3f& Side, const FBlade& B, int32 Slot = Leaves)
	{
		const FVector3f N0 = FVector3f::CrossProduct(Side, Dir).GetSafeNormal();
		const int32 First = G.Pos.Num();
		if (B.bRound)
		{
			// a disc through the base (the stalk's end), cupped a little; Lobes scallops round its edge
			const int32 Rim = 20;
			const int32 C = G.Add((Base + Dir * (0.45f * B.Length) + N0 * (B.Fold * 0.25f * B.Length)) * 100.f, N0, Side, FVector2f(0.5f, 0.275f));
			for (int32 I = 0; I <= Rim; I++)
			{
				const float Phi = -0.47f * PI + 0.94f * PI * I / Rim;
				float R = B.Length * FMath::Cos(Phi);
				if (B.Lobes > 0) R *= 1.f - B.LobeDepth * FMath::Sqrt(FMath::Abs(FMath::Sin(B.Lobes * Phi)));
				const float Fwd = R * FMath::Cos(Phi), Across = R * FMath::Sin(Phi) * (B.HalfWidth * 2.f / B.Length);
				const float Droop = B.Arch * Fwd / B.Length;
				const FVector3f P = Base + Dir * Fwd + Side * Across - N0 * (Droop * 0.3f * Fwd) + N0 * (B.Fold * 0.1f * B.Length);
				G.Add(P * 100.f, (N0 + Dir * (0.2f * Droop)).GetSafeNormal(), Side, FVector2f(0.5f + 0.5f * Across / B.Length, 0.5f - 0.5f * Fwd / B.Length));
				if (I > 0) G.Tris[Slot].Add(FIntVector3(C, C + I, C + I + 1));
			}
			return;
		}
		const int32 Samples0 = FMath::Clamp(6 + B.Lobes * 4 + (B.Teeth > 0 && B.Teeth <= 8 ? B.Teeth * 2 : 0), 4, 40) / (B.Length < 0.03f ? 3 : 1);
		const int32 Samples = FMath::Min(Samples0, B.MaxSamples);
		FVector3f P = Base;
		const int32 Rows = B.Thickness > 0.f ? 2 : 1;
		for (int32 I = 0; I <= Samples; I++)
		{
			const float T = (float)I / Samples;
			const float A = B.Arch * FMath::Pow(FMath::Max(T - 0.5f / Samples, 0.f), 1.3f);
			const FVector3f D = Dir * FMath::Cos(A) - N0 * FMath::Sin(A);
			FVector3f Nn = N0 * FMath::Cos(A) + Dir * FMath::Sin(A);
			FVector3f Sd = Side;
			const float Tw = B.Twist * T;
			Sd = Side * FMath::Cos(Tw) + Nn * FMath::Sin(Tw);
			Nn = Nn * FMath::Cos(Tw) - Side * FMath::Sin(Tw);
			if (I > 0) P += D * (B.Length / Samples);
			const float W = B.HalfWidth * Profile(B, T);
			const float U = W / FMath::Max(2.f * B.HalfWidth, 1e-4f);
			for (int32 Row = 0; Row < Rows; Row++)
			{
				// row 0 the face (a keel up the middle by Fold), row 1 a succulent's underside (a keel down by Thickness)
				const FVector3f Keel = Row == 0 ? Nn * (B.Fold * W) : -Nn * (B.Thickness * 2.f * W);
				const FVector3f Nrm = Row == 0 ? Nn : -Nn;
				G.Add((P - Sd * W) * 100.f, (Nrm - Sd * 0.25f).GetSafeNormal(), D, FVector2f(0.5f - U, 1.f - T));
				G.Add((P + Keel) * 100.f, Nrm, D, FVector2f(0.5f, 1.f - T));
				G.Add((P + Sd * W) * 100.f, (Nrm + Sd * 0.25f).GetSafeNormal(), D, FVector2f(0.5f + U, 1.f - T));
			}
			if (I > 0)
			{
				for (int32 Row = 0; Row < Rows; Row++)
				{
					const int32 P0 = First + 3 * Rows * (I - 1) + 3 * Row, P1 = First + 3 * Rows * I + 3 * Row;
					G.Tris[Slot].Add(FIntVector3(P0, P1, P1 + 1)); G.Tris[Slot].Add(FIntVector3(P0, P1 + 1, P0 + 1));
					G.Tris[Slot].Add(FIntVector3(P0 + 1, P1 + 1, P1 + 2)); G.Tris[Slot].Add(FIntVector3(P0 + 1, P1 + 2, P0 + 2));
				}
			}
		}
	}

	FBlade BladeOf(const FPlantSpecies& S, float Length)
	{
		FBlade B;
		B.Length = Length;
		B.HalfWidth = 0.5f * Length * S.Leaf.Width;
		B.WidestAt = S.Leaf.WidestAt;
		B.Power = S.Herb.ProfilePower;
		B.Lobes = S.Leaf.Lobes;
		B.LobeDepth = S.Leaf.LobeDepth;
		B.Teeth = S.Leaf.Teeth;
		B.TeethDepth = S.Leaf.TeethDepth;
		B.Fold = S.Leaf.Fold;
		B.Thickness = S.Herb.Thickness;
		B.Arch = FMath::DegreesToRadians(S.Herb.Arch);
		B.Twist = FMath::DegreesToRadians(S.Herb.Twist);
		B.bRound = S.Leaf.Form == EPlantLeafForm::Palmate;
		return B;
	}

	/**
	 * One leaf of the species from Base along Dir: its stalk (Petiole), then the blade, or for a compound leaf
	 * (Leaflets) a rachis with pairs of leaflets.
	 */
	void Leaf(FHerbGeometry& G, const FPlantSpecies& S, uint32 Seed, uint32 Key, const FVector3f& Base, const FVector3f& Dir, float Azimuth, float Length)
	{
		const FPlantHerb& H = S.Herb;
		FVector3f At = Base, D = Dir;
		const float Petiole = S.Leaf.Petiole * Length;
		if (Petiole > 0.004f)
		{
			At = Stalk(G, Base, Dir, Azimuth, Petiole, FMath::Max(0.0012f, 0.02f * Length), FMath::DegreesToRadians(H.Arch) * 0.25f, &D);
		}
		FBlade B = BladeOf(S, Length);
		if (H.Leaflets <= 0)
		{
			Blade(G, At, D, SideOf(D, Azimuth), B);
			return;
		}
		// a rachis bending down along the leaf, leaflets in pairs along it, longest in the middle
		const FVector3f Side = SideOf(D, Azimuth);
		const FVector3f N0 = FVector3f::CrossProduct(Side, D).GetSafeNormal();
		TArray<FVector3f> P;
		TArray<float> R;
		constexpr int32 Steps = 8;
		FVector3f Q = At, Dq = D;
		P.Add(Q); R.Add(FMath::Max(0.001f, 0.01f * Length));
		TArray<FVector3f> Dirs, Norms;
		Dirs.Add(D); Norms.Add(N0);
		for (int32 I = 1; I <= Steps; I++)
		{
			const float T = (I - 0.5f) / Steps, Ang = B.Arch * FMath::Pow(T, 1.3f);
			Dq = D * FMath::Cos(Ang) - N0 * FMath::Sin(Ang);
			Q += Dq * (Length / Steps);
			P.Add(Q); R.Add(FMath::Max(0.0008f, 0.01f * Length * (1.f - 0.7f * I / Steps)));
			Dirs.Add(Dq); Norms.Add(N0 * FMath::Cos(Ang) + D * FMath::Sin(Ang));
		}
		Tube(G, Stems, P, R, 3);
		FBlade LB = B;
		LB.Lobes = 0; LB.Teeth = 0; LB.Thickness = 0.f; LB.bRound = false; LB.Twist = 0.f;
		LB.Arch = FMath::DegreesToRadians(25.f);
		LB.Power = 0.6f;
		LB.WidestAt = 0.4f;
		for (int32 K = 0; K < H.Leaflets; K++)
		{
			const float T = 0.12f + 0.85f * (K + 0.5f) / H.Leaflets;
			const float F = T * Steps;
			const int32 I = FMath::Min(FMath::FloorToInt(F), Steps - 1);
			const FVector3f Pt = FMath::Lerp(P[I], P[I + 1], F - I);
			const FVector3f Dr = Dirs[I + 1], Nr = Norms[I + 1];
			const FVector3f Sr = FVector3f::CrossProduct(Dr, Nr).GetSafeNormal();
			const float Len = H.Leaflet.X * Length * FMath::Pow(FMath::Max(FMath::Sin(PI * FMath::Clamp(0.15f + 0.85f * T, 0.f, 1.f)), 0.f), 0.5f) * (0.9f + 0.2f * Hash01(Seed, Key, K, 31));
			LB.Length = Len;
			LB.HalfWidth = 0.5f * Len * H.Leaflet.Y;
			for (int32 Sgn = -1; Sgn <= 1; Sgn += 2)
			{
				const FVector3f Ld = (Sr * (float)Sgn * 0.8f + Dr * 0.55f).GetSafeNormal();
				Blade(G, Pt, Ld, SideOf(Ld, Azimuth), LB);
			}
		}
	}

	// --- flowers: petals in the atlas column of their colour Variant (petal rows v 0.05..0.62, the centre 0.72..0.95)

	struct FFlowerUV { float U0, U1; };

	void Petal(FHerbGeometry& G, const FVector3f& Base, const FVector3f& Dir, const FVector3f& Nrm, float Length, float HalfWidth, float Curl, const FFlowerUV& UV, float Power = 0.7f)
	{
		const FVector3f Side = FVector3f::CrossProduct(Dir, Nrm).GetSafeNormal();
		const int32 First = G.Pos.Num();
		const int32 Samples = Length < 0.025f ? 2 : 4;
		FVector3f P = Base;
		for (int32 I = 0; I <= Samples; I++)
		{
			const float T = (float)I / Samples;
			const float A = Curl * T;
			const FVector3f D = Dir * FMath::Cos(A) - Nrm * FMath::Sin(A);
			const FVector3f Nn = Nrm * FMath::Cos(A) + Dir * FMath::Sin(A);
			if (I > 0) P += D * (Length / Samples);
			const float W = HalfWidth * FMath::Pow(FMath::Max(FMath::Sin(PI * FMath::Clamp(0.12f + 0.88f * T, 0.f, 1.f)), 0.f), Power);
			const float V = 0.62f - 0.57f * T;
			G.Add((P - Side * W) * 100.f, Nn, D, FVector2f(UV.U0, V));
			G.Add((P + Side * W) * 100.f, Nn, D, FVector2f(UV.U1, V));
			if (I > 0)
			{
				const int32 A0 = First + 2 * (I - 1), A1 = First + 2 * I;
				G.Tris[Flowers].Add(FIntVector3(A0, A1, A1 + 1));
				G.Tris[Flowers].Add(FIntVector3(A0, A1 + 1, A0 + 1));
			}
		}
	}

	/** A small dome (a daisy's disc, a flower's centre) of radius R facing Axis. */
	void Disc(FHerbGeometry& G, const FVector3f& P, const FVector3f& Axis, float R, float Height, const FFlowerUV& UV)
	{
		const FVector3f X = SideOf(Axis, 0.f), Y = FVector3f::CrossProduct(Axis, X);
		const float Uc = 0.5f * (UV.U0 + UV.U1);
		const int32 C = G.Add((P + Axis * Height) * 100.f, Axis, X, FVector2f(Uc, 0.84f));
		constexpr int32 Rim = 10;
		for (int32 I = 0; I <= Rim; I++)
		{
			const float A = 2.f * PI * I / Rim;
			const FVector3f Rd = X * FMath::Cos(A) + Y * FMath::Sin(A);
			G.Add((P + Rd * R) * 100.f, (Axis + Rd * 0.6f).GetSafeNormal(), X, FVector2f(Uc + 0.4f * (UV.U1 - Uc) * FMath::Cos(A), 0.84f + 0.1f * FMath::Sin(A)));
			if (I > 0) G.Tris[Flowers].Add(FIntVector3(C, C + I, C + I + 1));
		}
	}

	void Flower(FHerbGeometry& G, const FPlantHerb& H, uint32 Seed, uint32 Key, const FVector3f& P, const FVector3f& Axis, float Size)
	{
		const int32 NumColours = FMath::Max(1, H.FlowerColours.Num());
		const int32 Variant = FMath::Min((int32)(Hash01(Seed, Key, 0, 61) * NumColours), NumColours - 1) % 4;
		const FFlowerUV UV{ Variant * 0.25f + 0.03f, Variant * 0.25f + 0.22f };
		const FVector3f X = SideOf(Axis, Hash01(Seed, Key, 0, 62) * 6.28f), Y = FVector3f::CrossProduct(Axis, X);
		const float Spin = Hash01(Seed, Key, 0, 63) * 6.28f;
		auto Radial = [&](float A) { return X * FMath::Cos(A + Spin) + Y * FMath::Sin(A + Spin); };
		switch (H.Flower)
		{
		case EPlantFlowerForm::Daisy:
		{
			const float Rc = 0.15f * Size;
			Disc(G, P, Axis, Rc, 0.06f * Size, UV);
			const int32 N = FMath::Max(5, H.Petals);
			for (int32 K = 0; K < N; K++)
			{
				const FVector3f D = (Radial(2.f * PI * (K + 0.3f * Hash01(Seed, Key, K, 64)) / N) - Axis * 0.12f).GetSafeNormal();
				Petal(G, P + D * (0.8f * Rc), D, Axis, 0.5f * Size - Rc, FMath::Min(0.13f * Size, 2.6f * Size / N), -0.15f, UV, 0.5f);
			}
			break;
		}
		case EPlantFlowerForm::Trumpet:
			for (int32 K = 0; K < 6; K++)
			{
				const FVector3f Rd = Radial(2.f * PI * K / 6.f);
				const FVector3f D = (Axis * 0.8f + Rd * 0.6f).GetSafeNormal();
				const FVector3f Nrm = FVector3f::CrossProduct(FVector3f::CrossProduct(D, Rd), D).GetSafeNormal();
				Petal(G, P, D, -Nrm, 0.55f * Size, (K % 2 ? 0.13f : 0.16f) * Size, 0.9f, UV, 0.6f);
			}
			break;
		case EPlantFlowerForm::Pompon:
			for (int32 L = 0; L < 5; L++)
			{
				const float E = FMath::DegreesToRadians(8.f + 16.f * L);
				const int32 N = 13 - L * 2;
				for (int32 K = 0; K < N; K++)
				{
					const FVector3f Rd = Radial(2.f * PI * (K + 0.5f * (L % 2)) / N);
					const FVector3f D = (Rd * FMath::Cos(E) + Axis * FMath::Sin(E)).GetSafeNormal();
					Petal(G, P + Axis * (0.06f * Size * L) + Rd * (0.04f * Size), D, (Axis - Rd * 0.3f).GetSafeNormal(), 0.3f * Size * (1.f - 0.12f * L), 0.1f * Size, 0.5f, UV, 0.4f);
				}
			}
			break;
		case EPlantFlowerForm::Five:
		case EPlantFlowerForm::Umbel:
		{
			auto Five = [&](const FVector3f& At, const FVector3f& Ax, float Sz)
			{
				const FVector3f X5 = SideOf(Ax, 0.f), Y5 = FVector3f::CrossProduct(Ax, X5);
				for (int32 K = 0; K < 5; K++)
				{
					const float A = 2.f * PI * K / 5.f + Spin;
					const FVector3f D = (X5 * FMath::Cos(A) + Y5 * FMath::Sin(A) + Ax * 0.2f).GetSafeNormal();
					Petal(G, At, D, Ax, 0.5f * Sz, 0.24f * Sz, -0.25f, UV, 0.45f);
				}
				Disc(G, At, Ax, 0.08f * Sz, 0.04f * Sz, UV);
			};
			if (H.Flower == EPlantFlowerForm::Five)
			{
				Five(P, Axis, Size);
				break;
			}
			// a head of florets on short stalks, a dome
			for (int32 F = 0; F < 11; F++)
			{
				const FVector3f D = (Axis * 0.9f + Radial(F * Golden) * (F == 0 ? 0.f : 0.75f)).GetSafeNormal();
				Five(P + D * (0.28f * Size), D, 0.36f * Size);
			}
			break;
		}
		default:
			break;
		}
	}

	FVector3f Lerp3(const TArray<FVector3f>& P, float T)
	{
		const float F = FMath::Clamp(T, 0.f, 1.f) * (P.Num() - 1);
		const int32 I = FMath::Min(FMath::FloorToInt(F), P.Num() - 2);
		return FMath::Lerp(P[I], P[I + 1], F - I);
	}

	/** A curved stem from Base, Height long, leaning Lean degrees toward Azimuth and curving back up by Lift; its points (m). */
	TArray<FVector3f> StemCurve(const FVector3f& Base, float Length, float LeanDeg, float Azimuth, float Lift, int32 Steps)
	{
		TArray<FVector3f> P;
		P.Add(Base);
		FVector3f D = FromAngles(LeanDeg, Azimuth);
		for (int32 I = 1; I <= Steps; I++)
		{
			P.Add(P.Last() + D * (Length / Steps));
			D = (D + Up * (Lift / Steps)).GetSafeNormal();
		}
		return P;
	}

	void BuildRosette(const FPlantSpecies& S, uint32 Seed, const FState& St, FHerbGeometry& G)
	{
		const FPlantHerb& H = S.Herb;
		const float M = St.Maturity;
		const int32 Count = FMath::Max(1, FMath::RoundToInt(H.Leaves * (0.25f + 0.75f * M)));
		const float Size = (0.35f + 0.65f * M) * St.LeafScale;
		const int32 NumFans = H.Fans > 0 ? FMath::Max(1, FMath::RoundToInt(H.Fans * (0.4f + 0.6f * M))) : 0;
		const int32 PerFan = NumFans > 0 ? FMath::Max(1, Count / NumFans) : Count;
		if (Size > 0.f && St.Present > 0.f)
		{
			for (int32 I = 0; I < Count; I++)
			{
				if (Hash01(Seed, I, 0, 1) >= St.Present) continue;
				float Azimuth, Outer;
				FVector3f Base(0.f, 0.f, 0.f);
				if (NumFans > 0)
				{
					// fans of two-ranked leaves, the oldest outermost
					const int32 Fan = I % NumFans, J = I / NumFans;
					const float Af = Fan * 2.f * PI / NumFans + Hash01(Seed, Fan, 0, 2) * 1.2f;
					Base = FVector3f(FMath::Cos(Af), FMath::Sin(Af), 0.f) * (0.18f * H.Spread * M * (NumFans > 1 ? 1.f : 0.f));
					Azimuth = Af + (J % 2 ? PI : 0.f) + (Hash01(Seed, I, 0, 3) - 0.5f) * 0.5f;
					Outer = PerFan > 1 ? 1.f - (float)FMath::Min(J, PerFan - 1) / (PerFan - 1) : 1.f;
				}
				else
				{
					Azimuth = I * Golden + Hash01(Seed, I, 0, 3) * 0.3f;
					Outer = Count > 1 ? 1.f - (float)I / (Count - 1) : 1.f;
				}
				const float Rise = FMath::Lerp((float)H.Rise.X, (float)H.Rise.Y, Outer) + (Hash01(Seed, I, 0, 4) - 0.5f) * 10.f;
				const FVector3f Dir = FromAngles(Rise, Azimuth);
				const float Length = S.Leaf.Length * Size * (0.6f + 0.4f * Outer) * (0.85f + 0.3f * Hash01(Seed, I, 0, 5));
				Leaf(G, S, Seed, 1000 + I, Base + Dir * 0.005f, Dir, Azimuth, Length);
			}
		}
		// flower scapes from the middle
		if (H.Flower != EPlantFlowerForm::None && H.Flowers > 0 && St.Bloom > 0.f)
		{
			const int32 Scapes = FMath::Max(1, FMath::RoundToInt(H.Flowers * M));
			for (int32 K = 0; K < Scapes; K++)
			{
				if (Hash01(Seed, K, 0, 6) >= St.Bloom) continue;
				const float Az = K * Golden + 0.7f;
				const float Len = (H.Height * M + H.ScapeHeight) * (0.85f + 0.3f * Hash01(Seed, K, 0, 7));
				const TArray<FVector3f> P = StemCurve(FVector3f(0.f, 0.f, 0.f), Len, 8.f + 12.f * Hash01(Seed, K, 0, 8), Az, 0.1f, 5);
				TArray<float> R;
				for (int32 I = 0; I < P.Num(); I++) R.Add(FMath::Max(0.0015f, 0.004f * (1.f - 0.5f * I / P.Num())));
				Tube(G, Stems, P, R, 4);
				const FVector3f Top = P.Last(), Axis = (Up * 0.6f + FromAngles(70.f, Az)).GetSafeNormal();
				Flower(G, H, Seed, 5000 + K, Top, Axis, H.FlowerSize);
				if (H.Flower == EPlantFlowerForm::Trumpet && Hash01(Seed, K, 0, 9) < St.Bloom)
				{
					Flower(G, H, Seed, 6000 + K, Lerp3(P, 0.85f) + FromAngles(90.f, Az + 1.3f) * 0.05f, (Up * 0.4f + FromAngles(90.f, Az + 1.3f)).GetSafeNormal(), H.FlowerSize * 0.9f);
				}
			}
		}
	}

	/** Leaves along a stem (points P), from Bare up, PerNode at a node. */
	void StemLeaves(FHerbGeometry& G, const FPlantSpecies& S, uint32 Seed, uint32 Key, const TArray<FVector3f>& P, int32 Count, float Size, float Present, float Bare, bool bTrailing)
	{
		const FPlantHerb& H = S.Herb;
		for (int32 K = 0; K < Count; K++)
		{
			const float T = Bare + (1.f - Bare) * (K + 0.5f) / Count;
			const FVector3f At = Lerp3(P, T);
			const FVector3f Along = (Lerp3(P, FMath::Min(T + 0.05f, 1.f)) - Lerp3(P, FMath::Max(T - 0.05f, 0.f))).GetSafeNormal(UE_SMALL_NUMBER, Up);
			const float Low = Count > 1 ? 1.f - (float)K / (Count - 1) : 1.f;
			for (int32 J = 0; J < FMath::Max(1, H.PerNode); J++)
			{
				const uint32 LeafKey = Key * 64 + K * 4 + J;
				if (Hash01(Seed, LeafKey, 0, 11) >= Present) continue;
				const float Az = (H.PerNode > 1 ? K * (PI / 2.f) + J * 2.f * PI / H.PerNode : K * Golden) + Hash01(Seed, LeafKey, 0, 12) * 0.4f;
				FVector3f Dir;
				if (bTrailing)
				{
					// leaves of a hanging stem stand out from it and turn to the light
					const FVector3f Out = SideOf(Along, Az) * (J % 2 ? -1.f : 1.f);
					Dir = (Out + Up * 0.6f).GetSafeNormal();
				}
				else
				{
					const float Rise = FMath::Lerp((float)H.Rise.X, (float)H.Rise.Y, Low) + (Hash01(Seed, LeafKey, 0, 13) - 0.5f) * 10.f;
					Dir = FromAngles(Rise, Az);
				}
				const float Len = S.Leaf.Length * Size * (0.6f + 0.4f * Low) * (0.85f + 0.3f * Hash01(Seed, LeafKey, 0, 14));
				Leaf(G, S, Seed, LeafKey, At, Dir, Az, Len);
			}
		}
	}

	void BuildBush(const FPlantSpecies& S, uint32 Seed, const FState& St, FHerbGeometry& G, bool bTrailing)
	{
		const FPlantHerb& H = S.Herb;
		const float M = St.Maturity;
		const float Size = (0.4f + 0.6f * M) * St.LeafScale;
		const int32 NumStems = FMath::Max(1, FMath::RoundToInt(H.Stems * (0.4f + 0.6f * M)));
		const int32 LeavesPer = FMath::Max(1, FMath::RoundToInt(H.Leaves * (0.3f + 0.7f * M)));
		int32 FlowerKey = 0;
		auto Bloom = [&](const FVector3f& At, const FVector3f& Axis, float Scale)
		{
			const uint32 K = 7000 + FlowerKey++;
			if (H.Flower != EPlantFlowerForm::None && St.Bloom > 0.f && Hash01(Seed, K, 0, 15) < St.Bloom)
			{
				Flower(G, H, Seed, K, At, Axis, H.FlowerSize * Scale);
			}
		};
		for (int32 Sn = 0; Sn < NumStems; Sn++)
		{
			const float Az = Sn * Golden + Hash01(Seed, Sn, 0, 16) * 0.8f;
			const FVector3f Base = NumStems > 1 ? FVector3f(FMath::Cos(Az), FMath::Sin(Az), 0.f) * (0.04f * H.Spread * (0.5f + Hash01(Seed, Sn, 0, 17))) : FVector3f::ZeroVector;
			TArray<FVector3f> P;
			float Len;
			if (bTrailing)
			{
				// out over the rim, then hanging: a stem whose direction keeps turning down
				Len = H.Spread * (0.5f + 0.7f * M) * (0.7f + 0.5f * Hash01(Seed, Sn, 0, 18));
				FVector3f D = FromAngles(55.f + 15.f * Hash01(Seed, Sn, 0, 19), Az);
				const FVector3f Sd = SideOf(D, Az), N0 = FVector3f::CrossProduct(Sd, D).GetSafeNormal();
				const FVector3f D0 = D;
				P.Add(Base + FVector3f(0.f, 0.f, 0.02f));
				constexpr int32 Steps = 12;
				for (int32 I = 1; I <= Steps; I++)
				{
					const float A = FMath::DegreesToRadians(115.f) * FMath::Pow((I - 0.5f) / Steps, 1.2f);
					D = D0 * FMath::Cos(A) - N0 * FMath::Sin(A);
					P.Add(P.Last() + D * (Len / Steps));
				}
			}
			else
			{
				Len = H.Height * (0.35f + 0.65f * M) * (0.8f + 0.35f * Hash01(Seed, Sn, 0, 18));
				const float Lean = H.Lean * (NumStems == 1 ? 0.25f : 0.5f + Hash01(Seed, Sn, 0, 19));
				P = StemCurve(Base, Len, Lean, Az, 0.15f, 8);
			}
			TArray<float> R;
			const float R0 = FMath::Max(0.0015f, H.StemRadius * (0.5f + 0.5f * M));
			for (int32 I = 0; I < P.Num(); I++) R.Add(R0 * (1.f - 0.6f * I / (P.Num() - 1)));
			Tube(G, Stems, P, R, R0 > 0.006f ? 6 : 4);
			StemLeaves(G, S, Seed, Sn + 1, P, LeavesPer, Size, St.Present, bTrailing ? 0.08f : H.BareStem, bTrailing);
			if (bTrailing) continue;
			const FVector3f TipDir = (P.Last() - P[P.Num() - 2]).GetSafeNormal();
			Bloom(P.Last(), (TipDir + Up).GetSafeNormal(), 1.f);
			// side branches, shorter higher up, each with its leaves and a flower
			const int32 NumBranches = FMath::RoundToInt(H.Branches * M);
			for (int32 B = 0; B < NumBranches; B++)
			{
				const float T = 0.35f + 0.5f * Hash01(Seed, Sn, B, 20);
				const float BAz = Az + 2.f + B * Golden;
				const TArray<FVector3f> Q = StemCurve(Lerp3(P, T), Len * (0.55f - 0.35f * T) * (0.8f + 0.4f * Hash01(Seed, Sn, B, 21)), 40.f, BAz, 0.6f, 5);
				TArray<float> Rq;
				for (int32 I = 0; I < Q.Num(); I++) Rq.Add(FMath::Max(0.0012f, R0 * 0.6f * (1.f - 0.6f * I / (Q.Num() - 1))));
				Tube(G, Stems, Q, Rq, 4);
				StemLeaves(G, S, Seed, 100 + Sn * 16 + B, Q, FMath::Max(1, LeavesPer / 2), Size * 0.8f, St.Present, 0.2f, false);
				Bloom(Q.Last(), ((Q.Last() - Q[Q.Num() - 2]).GetSafeNormal() + Up).GetSafeNormal(), 0.9f);
			}
			// more flowers on short stalks round the top of the stem (an aster's sprays)
			for (int32 F = 1 + NumBranches; F < H.Flowers; F++)
			{
				const float T = 0.6f + 0.38f * Hash01(Seed, Sn, F, 22);
				const FVector3f Out = FromAngles(55.f, Az + F * Golden);
				const FVector3f Tip = Lerp3(P, T) + Out * (0.07f * H.Height);
				TArray<FVector3f> Q = { Lerp3(P, T), Tip };
				TArray<float> Rq = { FMath::Max(0.001f, R0 * 0.35f), 0.001f };
				Tube(G, Stems, Q, Rq, 3);
				Bloom(Tip, (Out + Up).GetSafeNormal(), 0.85f);
			}
		}
	}

	/**
	 * A clipped hedge's face: the leaves that clipping makes it grow thick at its surface, over the sides and top
	 * of its box (scaled down while it is young), each standing out a little.
	 */
	void HedgeShell(const FPlantSpecies& S, uint32 Seed, const FState& St, FHerbGeometry& G)
	{
		const float M = St.Maturity;
		const FVector3f Half(0.5f * S.CrownBox.X * (0.4f + 0.6f * M), 0.5f * S.CrownBox.Y * (0.4f + 0.6f * M), S.CrownBox.Z * (0.3f + 0.7f * M));
		constexpr float PerM2 = 600.f;
		// the shaded inside, seen through the gaps: a box a little within the leaves
		{
			const FVector3f In = Half - FVector3f(0.07f, 0.07f, 0.07f);
			const FVector3f Lo(-In.X, -In.Y, 0.03f), Hi(In.X, In.Y, In.Z);
			auto Quad = [&](const FVector3f& N, const FVector3f& A, const FVector3f& B, const FVector3f& C, const FVector3f& D)
			{
				const int32 V0 = G.Add(A * 100.f, N, B - A, FVector2f(0.3f, 0.5f));
				G.Add(B * 100.f, N, B - A, FVector2f(0.7f, 0.5f));
				G.Add(C * 100.f, N, B - A, FVector2f(0.7f, 0.6f));
				G.Add(D * 100.f, N, B - A, FVector2f(0.3f, 0.6f));
				// in the flowers' section (a hedge has none): the plant shades it to the leaves' colour in shadow
				G.Tris[Flowers].Add(FIntVector3(V0, V0 + 2, V0 + 1));
				G.Tris[Flowers].Add(FIntVector3(V0, V0 + 3, V0 + 2));
			};
			if (In.X > 0.f && In.Y > 0.f && In.Z > 0.05f && St.Present > 0.3f)
			{
				Quad({ 1, 0, 0 }, { Hi.X, Lo.Y, Lo.Z }, { Hi.X, Hi.Y, Lo.Z }, { Hi.X, Hi.Y, Hi.Z }, { Hi.X, Lo.Y, Hi.Z });
				Quad({ -1, 0, 0 }, { Lo.X, Hi.Y, Lo.Z }, { Lo.X, Lo.Y, Lo.Z }, { Lo.X, Lo.Y, Hi.Z }, { Lo.X, Hi.Y, Hi.Z });
				Quad({ 0, 1, 0 }, { Hi.X, Hi.Y, Lo.Z }, { Lo.X, Hi.Y, Lo.Z }, { Lo.X, Hi.Y, Hi.Z }, { Hi.X, Hi.Y, Hi.Z });
				Quad({ 0, -1, 0 }, { Lo.X, Lo.Y, Lo.Z }, { Hi.X, Lo.Y, Lo.Z }, { Hi.X, Lo.Y, Hi.Z }, { Lo.X, Lo.Y, Hi.Z });
				Quad({ 0, 0, 1 }, { Lo.X, Lo.Y, Hi.Z }, { Hi.X, Lo.Y, Hi.Z }, { Hi.X, Hi.Y, Hi.Z }, { Lo.X, Hi.Y, Hi.Z });
			}
		}
		struct FFace { FVector3f N, U, V, C; float W, H; };
		const FFace Faces[5] = {
			{ { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { Half.X, 0, 0.5f * Half.Z }, Half.Y, 0.5f * Half.Z },
			{ { -1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { -Half.X, 0, 0.5f * Half.Z }, Half.Y, 0.5f * Half.Z },
			{ { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, { 0, Half.Y, 0.5f * Half.Z }, Half.X, 0.5f * Half.Z },
			{ { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, { 0, -Half.Y, 0.5f * Half.Z }, Half.X, 0.5f * Half.Z },
			{ { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, Half.Z }, Half.X, Half.Y },
		};
		FBlade B = BladeOf(S, S.Leaf.Length * St.LeafScale);
		B.Arch = FMath::DegreesToRadians(20.f);
		B.MaxSamples = 2;
		if (B.Length <= 0.f) return;
		uint32 K = 0;
		for (const FFace& F : Faces)
		{
			const int32 Count = FMath::RoundToInt(4.f * F.W * F.H * PerM2);
			for (int32 I = 0; I < Count; I++, K++)
			{
				if (Hash01(Seed, K, 0, 81) >= St.Present) continue;
				// the bottom of the sides is thinner (shaded)
				const float A = Hash01(Seed, K, 0, 82) * 2.f - 1.f, Bv = Hash01(Seed, K, 0, 83) * 2.f - 1.f;
				if (F.N.Z == 0.f && Bv < -0.6f && Hash01(Seed, K, 0, 84) < 0.5f) continue;
				FVector3f At = F.C + F.U * (A * F.W) + F.V * (Bv * F.H) - F.N * (0.06f * Hash01(Seed, K, 0, 85));
				At.Z = FMath::Max(At.Z, 0.04f);
				const float Az = Hash01(Seed, K, 0, 86) * 2.f * PI;
				const FVector3f Tang = F.U * FMath::Cos(Az) + F.V * FMath::Sin(Az);
				const FVector3f Dir = (F.N * 0.55f + Tang + Up * 0.3f).GetSafeNormal();
				B.Length = S.Leaf.Length * St.LeafScale * (0.8f + 0.4f * Hash01(Seed, K, 0, 87));
				B.HalfWidth = 0.5f * B.Length * S.Leaf.Width;
				Blade(G, At, Dir, SideOf(Dir, Az), B);
			}
		}
	}

	void BuildColumn(const FPlantSpecies& S, uint32 Seed, const FState& St, FHerbGeometry& G)
	{
		const FPlantHerb& H = S.Herb;
		const float M = St.Maturity;
		const float Height = H.Height * (0.3f + 0.7f * M), Radius = 0.5f * H.Spread * (0.5f + 0.5f * M);
		const int32 Sides = FMath::Max(3, H.Ribs) * 4;
		auto Column = [&](const TArray<FVector3f>& Axis, float R)
		{
			// a ribbed column with a rounded top
			TArray<FVector3f> P;
			TArray<float> Rr;
			const int32 N = Axis.Num();
			for (int32 I = 0; I < N; I++)
			{
				P.Add(Axis[I]);
				Rr.Add(R);
			}
			const FVector3f Top = Axis.Last(), Dir = (Axis.Last() - Axis[N - 2]).GetSafeNormal();
			for (int32 I = 1; I <= 4; I++)
			{
				const float A = 0.5f * PI * I / 4.f;
				P.Add(Top + Dir * (0.7f * R * FMath::Sin(A)));
				Rr.Add(FMath::Max(R * FMath::Cos(A), 0.002f));
			}
			Tube(G, Leaves, P, Rr, Sides, H.Ribs, 0.25f, 0.06f);
		};
		TArray<FVector3f> Body;
		for (int32 I = 0; I <= 10; I++) Body.Add(FVector3f(0.f, 0.f, (Height - 0.7f * Radius) * I / 10.f));
		Column(Body, Radius);
		const int32 Arms = M > 0.6f ? H.Arms : 0;
		for (int32 A = 0; A < Arms; A++)
		{
			const float Az = A * 2.7f + Hash01(Seed, A, 0, 23) * 1.2f;
			const FVector3f Out(FMath::Cos(Az), FMath::Sin(Az), 0.f);
			const float Z = Height * (0.35f + 0.25f * Hash01(Seed, A, 0, 24));
			const float Ra = Radius * 0.65f;
			TArray<FVector3f> Arm = { FVector3f(0.f, 0.f, Z) + Out * (0.5f * Radius), FVector3f(0.f, 0.f, Z) + Out * (Radius + 0.06f),
				FVector3f(0.f, 0.f, Z + 0.05f) + Out * (Radius + 0.12f) };
			const float Rise = Height * (0.25f + 0.2f * Hash01(Seed, A, 0, 25));
			for (int32 I = 1; I <= 4; I++) Arm.Add(Arm[2] + FVector3f(0.f, 0.f, Rise * I / 4.f));
			Column(Arm, Ra);
		}
	}

	FColor Srgb(const FLinearColor& C) { return FLinearColor(C.R, C.G, C.B, 1.f).ToFColor(true); }
}

FState PlantHerbs::State(const FPlantSpecies& S, uint32 Seed, float Age, float Day)
{
	const FPlantHerb& H = S.Herb;
	FState St;
	const float D = Day - (Hash01(Seed, 0x5EA5u) * 2.f - 1.f) * S.PhenologyJitter;
	St.Maturity = FMath::Clamp(1.f - FMath::Exp(-FMath::Max(Age, 0.f) / FMath::Max(H.MatureYears, 0.05f)), 0.05f, 1.f);
	auto BloomOn = [&](float Dd)
	{
		if (H.Flower == EPlantFlowerForm::None) return 0.f;
		if (H.BloomStart <= 0.f) return 0.8f;
		return Smooth(H.BloomStart, H.BloomStart + 12.f, Dd) * (1.f - Smooth(H.BloomEnd - 12.f, H.BloomEnd, Dd));
	};
	if (S.bIndoor)
	{
		St.Bloom = BloomOn(D);
		St.LeafColour = S.SummerLeafColour;
	}
	else if (H.bAnnual)
	{
		// sown in spring, grown up two months later, browned and gone with the first frost
		St.bAlive = D >= H.SowDay && D < H.FrostDay + 8.f;
		St.Maturity = FMath::Clamp(Smooth(H.SowDay, H.SowDay + 65.f, D), 0.08f, 1.f);
		St.Wither = Smooth(H.FrostDay - 1.f, H.FrostDay + 3.f, D);
		St.Present = 1.f - 0.6f * Smooth(H.FrostDay + 2.f, H.FrostDay + 8.f, D);
		St.Bloom = BloomOn(D) * (1.f - St.Wither) * Smooth(0.45f, 0.75f, St.Maturity);
		St.LeafColour = FMath::Lerp(S.SummerLeafColour, S.AutumnLeafColourLate, St.Wither);
	}
	else
	{
		// a perennial: the season's leaves (they turn and die back like a tree's), flowers in their weeks
		const FPlantSeason Sn = PlantModel::Season(S, Seed, Day);
		St.LeafScale = S.bEvergreen ? 1.f : Sn.LeafScale;
		St.Present = S.bEvergreen ? 1.f : 1.f - Sn.Fallen[1];
		St.bAlive = H.bWoody || (St.LeafScale > 0.f && St.Present > 0.02f);
		St.Bloom = BloomOn(D);
		St.LeafColour = Sn.Colour[1];
	}
	auto Q = [](float V, float Steps) { return (uint32)FMath::RoundToInt(FMath::Clamp(V, 0.f, 1.f) * Steps); };
	St.Key = HashCombineFast(HashCombineFast(Q(St.Maturity, 40.f), Q(St.LeafScale, 20.f)), HashCombineFast(HashCombineFast(Q(St.Present, 20.f), Q(St.Bloom, 20.f)), St.bAlive ? 1u : 0u));
	return St;
}

void PlantHerbs::Build(const FPlantSpecies& S, uint32 Seed, const FState& St, FHerbGeometry& Out, float& OutHeight, float& OutRadius)
{
	if (St.bAlive)
	{
		switch (S.Herb.Form)
		{
		case EPlantHerbForm::Rosette: BuildRosette(S, Seed, St, Out); break;
		case EPlantHerbForm::Bush:
			BuildBush(S, Seed, St, Out, false);
			if (S.CrownBox.X > 0.f) HedgeShell(S, Seed, St, Out);
			break;
		case EPlantHerbForm::Trailing: BuildBush(S, Seed, St, Out, true); break;
		case EPlantHerbForm::Column: BuildColumn(S, Seed, St, Out); break;
		default: break;
		}
	}
	if (S.CrownBox.X > 0.f)
	{
		// clipped: whatever grows out of the box is cut back to its face, a little unevenly
		const FVector3f Half(50.f * S.CrownBox.X, 50.f * S.CrownBox.Y, 100.f * S.CrownBox.Z);
		for (int32 I = 0; I < Out.Pos.Num(); I++)
		{
			FVector3f& P = Out.Pos[I];
			const float In = 3.f * Hash01(Seed, I, 0, 71);
			P.X = FMath::Clamp(P.X, -Half.X + In, Half.X - In);
			P.Y = FMath::Clamp(P.Y, -Half.Y + In, Half.Y - In);
			P.Z = FMath::Min(P.Z, Half.Z - In);
		}
	}
	OutHeight = OutRadius = 0.f;
	for (const FVector3f& P : Out.Pos)
	{
		OutHeight = FMath::Max(OutHeight, P.Z / 100.f);
		OutRadius = FMath::Max(OutRadius, FVector2f(P.X, P.Y).Size() / 100.f);
	}
}

UStaticMesh* PlantHerbs::BuildMesh(UObject* Outer, UStaticMesh* Mesh, FName Name, const FHerbGeometry& G)
{
	static const FName SlotNames[NumSlots] = { TEXT("Leaves"), TEXT("Stems"), TEXT("Flowers") };
	FMeshDescription MD;
	FStaticMeshAttributes Attr(MD);
	Attr.Register();
	TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> VTan = Attr.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> VSign = Attr.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector2f> VUV = Attr.GetVertexInstanceUVs();
	VUV.SetNumChannels(1);
	FPolygonGroupID Groups[NumSlots];
	for (int32 K = 0; K < NumSlots; K++)
	{
		Groups[K] = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[Groups[K]] = SlotNames[K];
	}
	TArray<FVertexInstanceID> Inst;
	Inst.Reserve(G.Pos.Num() + 3);
	for (int32 I = 0; I < G.Pos.Num(); I++)
	{
		const FVertexID V = MD.CreateVertex();
		VPos[V] = G.Pos[I].ContainsNaN() ? FVector3f::ZeroVector : G.Pos[I];
		const FVertexInstanceID VI = MD.CreateVertexInstance(V);
		const FVector3f N = G.Nrm[I].GetSafeNormal(UE_SMALL_NUMBER, Up);
		VNrm[VI] = N;
		FVector3f T = (G.Tan[I] - N * FVector3f::DotProduct(G.Tan[I], N)).GetSafeNormal();
		if (T.IsNearlyZero()) T = SideOf(N, 0.f);
		VTan[VI] = T;
		VSign[VI] = 1.f;
		VUV.Set(VI, 0, G.UV[I]);
		Inst.Add(VI);
	}
	// every section keeps at least one (degenerate) triangle, so the material slots never shift
	const int32 Pad = Inst.Num();
	for (int32 I = 0; I < 3; I++)
	{
		const FVertexID V = MD.CreateVertex();
		VPos[V] = FVector3f::ZeroVector;
		const FVertexInstanceID VI = MD.CreateVertexInstance(V);
		VNrm[VI] = Up; VTan[VI] = FVector3f::XAxisVector; VSign[VI] = 1.f; VUV.Set(VI, 0, FVector2f::ZeroVector);
		Inst.Add(VI);
	}
	for (int32 K = 0; K < NumSlots; K++)
	{
		for (const FIntVector3& T : G.Tris[K]) MD.CreateTriangle(Groups[K], { Inst[T.X], Inst[T.Y], Inst[T.Z] });
		if (G.Tris[K].IsEmpty()) MD.CreateTriangle(Groups[K], { Inst[Pad], Inst[Pad + 1], Inst[Pad + 2] });
	}
	if (!Mesh)
	{
		// never saved, never copied (a PIE copy builds its own at BeginPlay), a fresh name each time
		Mesh = NewObject<UStaticMesh>(Outer, MakeUniqueObjectName(Outer, UStaticMesh::StaticClass(), Name), RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
		for (int32 K = 0; K < NumSlots; K++) Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, SlotNames[K]));
	}
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bMarkPackageDirty = false;
	Params.bBuildSimpleCollision = false;
	Params.bCommitMeshDescription = false;
	Params.bFastBuild = true;
	Mesh->BuildFromMeshDescriptions({ &MD }, Params);
	if (UBodySetup* Body = Mesh->GetBodySetup()) Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	return Mesh;
}

bool PlantHerbs::LeafTextureCarriesColour(const FPlantSpecies& S)
{
	return S.Herb.Variegation != EPlantVariegation::None || S.Herb.Form == EPlantHerbForm::Column;
}

UTexture2D* PlantHerbs::BuildLeafTexture(UObject* Outer, const FPlantSpecies& S)
{
	if (!LeafTextureCarriesColour(S)) return nullptr;
	const FPlantHerb& H = S.Herb;
	constexpr int32 W = 128;
	TArray<FColor> Px;
	Px.SetNumUninitialized(W * W);
	const FLinearColor Green = S.SummerLeafColour, Var = H.VariegationColour;
	for (int32 Y = 0; Y < W; Y++)
	{
		for (int32 X = 0; X < W; X++)
		{
			const float U = (X + 0.5f) / W, V = (Y + 0.5f) / W;
			const float Dx = FMath::Abs(U - 0.5f);
			FLinearColor C = Green;
			if (H.Form == EPlantHerbForm::Column)
			{
				// one rib wide, 6 cm tall: darker in the groove, an areole with a tuft of pale spines on the crest
				const float Groove = 0.75f + 0.25f * FMath::Cos(2.f * PI * U);
				C = Green * Groove;
				const float Ax = FMath::Min(U, 1.f - U), Ay = V - 0.5f;
				const float R = FMath::Sqrt(Ax * Ax + Ay * Ay);
				const float Spine = (R < 0.35f && FMath::Abs(FMath::Sin(8.f * FMath::Atan2(Ay, Ax))) < 0.12f) ? 1.f : 0.f;
				if (R < 0.06f || Spine > 0.f) C = FLinearColor(0.75f, 0.72f, 0.6f);
			}
			else
			{
				float Mask = 0.f;
				switch (H.Variegation)
				{
				case EPlantVariegation::CentralStripe: Mask = 1.f - Smooth(0.1f, 0.16f, Dx); break;
				case EPlantVariegation::Margin: Mask = Smooth(0.36f, 0.42f, Dx); break;
				case EPlantVariegation::Bands:
				{
					const float Wave = FMath::Sin(V * 46.f + FMath::Sin(U * 9.f + V * 3.f) * 1.6f);
					Mask = Smooth(0.2f, 0.6f, Wave) * 0.85f;
					break;
				}
				case EPlantVariegation::Blotches:
				{
					const float N = 0.5f + 0.25f * FMath::Sin(U * 23.f + FMath::Sin(V * 17.f) * 2.f) + 0.25f * FMath::Sin(V * 31.f + U * 7.f);
					Mask = Smooth(0.55f, 0.7f, N) * (1.f - Smooth(0.0f, 0.04f, 0.04f - Dx)) * Smooth(0.42f, 0.3f, Dx);
					break;
				}
				case EPlantVariegation::Stripes: Mask = Smooth(0.12f, 0.16f, Dx) * (1.f - Smooth(0.3f, 0.35f, Dx)); break;
				default: break;
				}
				C = FMath::Lerp(Green, H.Variegation == EPlantVariegation::Bands ? Green * 0.35f : Var, Mask);
				// veins: a paler midrib and faint side veins
				const float Mid = FMath::Exp(-FMath::Square(Dx / 0.012f));
				const float Side = FMath::Exp(-FMath::Square((FMath::Frac((1.f - V - Dx * 1.1f) * 8.f) - 0.5f) / 0.06f)) * FMath::Clamp(1.f - Dx * 2.2f, 0.f, 1.f);
				C = C * (0.9f + 0.18f * FMath::Max(Mid, 0.5f * Side));
			}
			Px[Y * W + X] = Srgb(C);
		}
	}
	return PlantMeshes::MakeTextureFromPixels(Outer, W, W, MoveTemp(Px), true);
}

UTexture2D* PlantHerbs::BuildFlowerAtlas(UObject* Outer, const FPlantSpecies& S)
{
	const FPlantHerb& H = S.Herb;
	constexpr int32 W = 256, Hh = 128;
	TArray<FColor> Px;
	Px.SetNumUninitialized(W * Hh);
	for (int32 Y = 0; Y < Hh; Y++)
	{
		for (int32 X = 0; X < W; X++)
		{
			const float U = (X + 0.5f) / W, V = (Y + 0.5f) / Hh;
			const int32 Variant = FMath::Min(X * 4 / W, 3);
			const FLinearColor Petal = H.FlowerColours.Num() ? H.FlowerColours[Variant % H.FlowerColours.Num()] : FLinearColor(0.8f, 0.8f, 0.8f);
			FLinearColor C;
			if (V < 0.68f)
			{
				// a petal from its base (v 0.62, a touch darker and toward the centre's hue) to its tip (v 0.05), faint veins
				const float T = FMath::Clamp((0.62f - V) / 0.57f, 0.f, 1.f);
				const float Lu = FMath::Frac(U * 4.f);
				const float Vein = 0.06f * FMath::Cos(Lu * 2.f * PI * 6.f);
				C = FMath::Lerp(FMath::Lerp(Petal, H.CentreColour, 0.25f) * 0.75f, Petal * 1.05f, Smooth(0.f, 0.4f, T)) * (1.f + Vein);
			}
			else
			{
				// the centre: its colour, freckled
				const float Dots = FMath::Sin(U * 97.f) * FMath::Sin(V * 83.f);
				C = H.CentreColour * (0.85f + 0.25f * Dots);
			}
			Px[Y * W + X] = Srgb(C);
		}
	}
	return PlantMeshes::MakeTextureFromPixels(Outer, W, Hh, MoveTemp(Px), true);
}

float PlantHerbs::PotSoilZ(float Diameter)
{
	return 0.72f * Diameter;
}

UStaticMesh* PlantHerbs::BuildPot(UObject* Outer, float Diameter)
{
	// a lathe of the pot's profile (r, z) in cm: the base, the wall, the rim's lip, its top, the inner wall down to
	// the soil; section 0 the pot, section 1 the soil's disc
	const float D = Diameter, H = 0.8f * D;
	const TArray<FVector2f> Profile = { { 0.f, 0.f }, { 0.36f * D, 0.f }, { 0.47f * D, 0.86f * H }, { 0.52f * D, 0.87f * H }, { 0.52f * D, H },
		{ 0.46f * D, H }, { 0.45f * D, 0.9f * H } };
	FHerbGeometry G;
	constexpr int32 Sides = 28;
	for (int32 K = 0; K + 1 < Profile.Num(); K++)
	{
		const FVector2f A = Profile[K], B = Profile[K + 1];
		const FVector2f T = (B - A).GetSafeNormal();
		const FVector2f N2(T.Y, -T.X); // outward on the outside, inward-facing on the way back in
		const int32 Base = G.Pos.Num();
		for (int32 S = 0; S <= Sides; S++)
		{
			const float Ang = 2.f * PI * S / Sides;
			const FVector3f Rd(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
			const FVector3f Nrm = (Rd * N2.X + Up * N2.Y).GetSafeNormal(UE_SMALL_NUMBER, Up);
			G.Add(Rd * A.X + Up * A.Y, Nrm, FVector3f(-Rd.Y, Rd.X, 0.f), FVector2f((float)S / Sides, A.Y / H));
			G.Add(Rd * B.X + Up * B.Y, Nrm, FVector3f(-Rd.Y, Rd.X, 0.f), FVector2f((float)S / Sides, B.Y / H));
		}
		for (int32 S = 0; S < Sides; S++)
		{
			const int32 I0 = Base + 2 * S, I1 = I0 + 1, I2 = I0 + 2, I3 = I0 + 3;
			G.Tris[0].Add(FIntVector3(I0, I1, I2));
			G.Tris[0].Add(FIntVector3(I2, I1, I3));
		}
	}
	const int32 C = G.Add(Up * (0.9f * H), Up, FVector3f::XAxisVector, FVector2f(0.5f, 0.5f));
	for (int32 S = 0; S <= Sides; S++)
	{
		const float Ang = 2.f * PI * S / Sides;
		G.Add(FVector3f(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * (0.45f * D) + Up * (0.9f * H), Up, FVector3f::XAxisVector, FVector2f(0.5f + 0.5f * FMath::Cos(Ang), 0.5f + 0.5f * FMath::Sin(Ang)));
		if (S > 0) G.Tris[1].Add(FIntVector3(C, C + S + 1, C + S));
	}
	// positions above are already cm; BuildMesh takes them as they are
	return BuildMesh(Outer, nullptr, TEXT("SM_PlantPot"), G);
}
