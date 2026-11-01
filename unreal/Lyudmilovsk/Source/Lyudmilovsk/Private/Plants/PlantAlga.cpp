#include "Plants/PlantHerb.h"

// Seaweeds and sea grass of the Sambian coast's shallow sea (EPlantHerbForm::Alga), built like the herbs: one mesh of the
// three sections (fronds, stalks and holdfast, bladders and receptacles), a pure function of (species, seed, age, day).
// Units in the mesh: cm, the holdfast at the origin, up +Z. The water moves them in their material (M_LyudSeaAlga).

namespace
{
	using namespace PlantHerbs;

	float Hash01(uint32 Seed, uint32 A, uint32 B = 0, uint32 C = 0)
	{
		uint32 H = Seed * 0x9E3779B1u ^ A * 0x85EBCA77u ^ B * 0xC2B2AE3Du ^ C * 0x27D4EB2Fu;
		H ^= H >> 15; H *= 0x2C1B3C6Du; H ^= H >> 12; H *= 0x297A2D39u; H ^= H >> 15;
		return float(H & 0xFFFFFF) / float(0x1000000);
	}
	float Smooth(float E0, float E1, float X)
	{
		const float T = FMath::Clamp((X - E0) / (E1 - E0), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}
	FVector3f Rotate(const FVector3f& V, const FVector3f& Axis, float Rad)
	{
		return FVector3f(FQuat4f(Axis.GetSafeNormal(), Rad).RotateVector(V));
	}
	FVector3f AnyPerp(const FVector3f& D)
	{
		const FVector3f A = FMath::Abs(D.Z) < 0.9f ? FVector3f(0.f, 0.f, 1.f) : FVector3f(1.f, 0.f, 0.f);
		return FVector3f::CrossProduct(D, A).GetSafeNormal();
	}

	/** A frame along a centreline: the point, the direction, the blade's side. */
	struct FFrame
	{
		FVector3f P, D, S;
	};

	/**
	 * A flat strip along the frames: Across vertices across (2, or 3 with a raised midrib or ruffled edges), Half widths
	 * (cm) per frame, V from V0 to V1 along it (the material browns the tips by V). One-sided: the material is two-sided.
	 */
	void Strip(FHerbGeometry& G, int32 Slot, const TArray<FFrame>& F, const TArray<float>& Half, float V0, float V1, float Midrib, float Ruffle, uint32 Seed)
	{
		const int32 N = F.Num();
		const int32 Across = (Midrib > 0.f || Ruffle > 0.f) ? 3 : 2;
		const int32 Base = G.Pos.Num();
		for (int32 I = 0; I < N; I++)
		{
			const float T = N > 1 ? float(I) / (N - 1) : 0.f;
			const FVector3f Nrm = FVector3f::CrossProduct(F[I].D, F[I].S).GetSafeNormal();
			for (int32 K = 0; K < Across; K++)
			{
				const float A = Across == 2 ? (K ? 1.f : -1.f) : float(K - 1);
				FVector3f P = F[I].P + F[I].S * (A * Half[I]);
				if (Across == 3 && K == 1) P += Nrm * (Midrib * Half[I]);
				if (Across == 3 && K != 1 && Ruffle > 0.f)
				{
					// wavy edges: a crinkle along each edge, out of phase between the two
					P += Nrm * (Ruffle * Half[I] * 0.8f * FMath::Sin(T * 23.f + A * 1.7f + Hash01(Seed, 3) * 6.f));
				}
				G.Add(P, Nrm, F[I].D, FVector2f(Across == 2 ? float(K) : 0.5f * K, FMath::Lerp(V0, V1, T)));
			}
		}
		for (int32 I = 0; I + 1 < N; I++)
			for (int32 K = 0; K + 1 < Across; K++)
			{
				const int32 A = Base + I * Across + K, B = A + 1, C = A + Across, D = C + 1;
				G.Tris[Slot].Add(FIntVector3(A, C, B));
				G.Tris[Slot].Add(FIntVector3(B, C, D));
			}
	}

	/** A round stem along the frames, radii per frame (cm), Sides around. */
	void Tube(FHerbGeometry& G, int32 Slot, const TArray<FFrame>& F, const TArray<float>& R, int32 Sides, float V0, float V1)
	{
		const int32 N = F.Num();
		const int32 Base = G.Pos.Num();
		for (int32 I = 0; I < N; I++)
		{
			const FVector3f B = FVector3f::CrossProduct(F[I].D, F[I].S).GetSafeNormal();
			for (int32 K = 0; K <= Sides; K++)
			{
				const float A = 2.f * PI * K / Sides;
				const FVector3f O = F[I].S * FMath::Cos(A) + B * FMath::Sin(A);
				G.Add(F[I].P + O * R[I], O, F[I].D, FVector2f(float(K) / Sides, FMath::Lerp(V0, V1, N > 1 ? float(I) / (N - 1) : 0.f)));
			}
		}
		for (int32 I = 0; I + 1 < N; I++)
			for (int32 K = 0; K < Sides; K++)
			{
				const int32 A = Base + I * (Sides + 1) + K, B = A + 1, C = A + Sides + 1, D = C + 1;
				G.Tris[Slot].Add(FIntVector3(A, B, C));
				G.Tris[Slot].Add(FIntVector3(B, D, C));
			}
	}

	/** A small ellipsoid (a bladder, a receptacle), radii along the frame's S, normal and D (cm). */
	void Blob(FHerbGeometry& G, int32 Slot, const FFrame& F, const FVector3f& Radii)
	{
		const FVector3f Nrm = FVector3f::CrossProduct(F.D, F.S).GetSafeNormal();
		const int32 Rings = 4, Sides = 6;
		const int32 Base = G.Pos.Num();
		for (int32 I = 0; I <= Rings; I++)
		{
			const float Phi = PI * I / Rings;
			for (int32 K = 0; K <= Sides; K++)
			{
				const float A = 2.f * PI * K / Sides;
				const FVector3f U = F.S * (FMath::Sin(Phi) * FMath::Cos(A)) + Nrm * (FMath::Sin(Phi) * FMath::Sin(A)) + F.D * FMath::Cos(Phi);
				const FVector3f P = F.P + F.S * (U.Dot(F.S) * Radii.X) + Nrm * (U.Dot(Nrm) * Radii.Y) + F.D * (U.Dot(F.D) * Radii.Z);
				G.Add(P, U, F.S, FVector2f(float(K) / Sides, float(I) / Rings));
			}
		}
		for (int32 I = 0; I < Rings; I++)
			for (int32 K = 0; K < Sides; K++)
			{
				const int32 A = Base + I * (Sides + 1) + K, B = A + 1, C = A + Sides + 1, D = C + 1;
				G.Tris[Slot].Add(FIntVector3(A, C, B));
				G.Tris[Slot].Add(FIntVector3(B, C, D));
			}
	}

	/**
	 * A centreline of Segs segments, Length cm, from a frame: it bends by Arch radians over its length about its side
	 * (a frond lies over toward its face), sways a little sideways and twists by Twist radians about itself.
	 */
	TArray<FFrame> Line(const FFrame& Start, float Length, int32 Segs, float Arch, float Twist, float Wander, uint32 Seed)
	{
		TArray<FFrame> Out;
		FFrame F = Start;
		Out.Add(F);
		const float Step = Length / Segs;
		for (int32 I = 0; I < Segs; I++)
		{
			const FVector3f Nrm = FVector3f::CrossProduct(F.D, F.S).GetSafeNormal();
			F.D = Rotate(F.D, F.S, Arch / Segs).GetSafeNormal();
			F.D = Rotate(F.D, Nrm, Wander * (Hash01(Seed, I, 11) - 0.5f)).GetSafeNormal();
			F.S = Rotate(F.S, F.D, Twist / Segs);
			F.S = (F.S - F.D * F.S.Dot(F.D)).GetSafeNormal();
			F.P += F.D * Step;
			Out.Add(F);
		}
		return Out;
	}

	struct FCtx
	{
		const FPlantSpecies& S;
		const FPlantAlga& A;
		FHerbGeometry& G;
		uint32 Seed;
		float Height;     // cm, this plant's frond length now
		float Present;    // share of its fronds there now
		bool bReceptacles;
	};

	/** A frond of a forked alga: a segment, then two arms, until Forks; flat with a midrib and bladders, or round. */
	void Fork(FCtx& C, const FFrame& Start, float SegLen, float HalfW, int32 Level, float V0, uint32 Key)
	{
		const FPlantAlga& A = C.A;
		const float VStep = 1.f / (A.Forks + 1);
		const float Arch = (1.f - A.Upright) * 0.5f + 0.15f * (Hash01(C.Seed, Key, 1) - 0.5f);
		const int32 Segs = 3;
		TArray<FFrame> L = Line(Start, SegLen, Segs, Arch, 0.15f * (Hash01(C.Seed, Key, 2) - 0.5f), 0.12f, C.Seed ^ Key);
		const float W1 = HalfW * (Level < A.Forks ? 0.94f : 0.7f);
		TArray<float> Half;
		for (int32 I = 0; I <= Segs; I++) Half.Add(FMath::Lerp(HalfW, W1, float(I) / Segs));
		if (A.Flat > 0.5f)
		{
			Strip(C.G, Leaves, L, Half, V0, V0 + VStep, A.bMidrib ? 0.18f : 0.f, A.Ruffle, C.Seed ^ Key);
			// air bladders in pairs beside the midrib, in the middle of a segment
			if (A.Bladders > 0.f && Level > 0 && Hash01(C.Seed, Key, 3) < A.Bladders)
			{
				const FFrame& M = L[Segs / 2 + 1];
				const float R = 50.f * A.BladderSize * (0.8f + 0.4f * Hash01(C.Seed, Key, 4));
				for (float Side : { -1.f, 1.f })
				{
					FFrame B = M;
					B.P += M.S * (Side * HalfW * 0.45f);
					Blob(C.G, Flowers, B, FVector3f(R * 0.8f, R * 0.75f, R * 1.15f));
				}
			}
		}
		else
		{
			Tube(C.G, Leaves, L, Half, A.Width > 0.003f ? 5 : 3, V0, V0 + VStep);
		}
		if (Level >= A.Forks)
		{
			if (C.bReceptacles && A.Receptacles > 0.f)
			{
				// the swollen warty tips of a ripe wrack
				FFrame T = L.Last();
				T.P += T.D * (HalfW * 0.6f);
				Blob(C.G, Flowers, T, FVector3f(HalfW * 0.9f, HalfW * 0.6f * A.Receptacles + 0.15f, HalfW * (1.2f + 1.2f * A.Receptacles)));
			}
			return;
		}
		// two arms, in the frond's plane (flat) or anywhere round it (round stems); not every arm on a thin bush grows on
		const FVector3f Nrm = FVector3f::CrossProduct(L.Last().D, L.Last().S).GetSafeNormal();
		const float Spread = FMath::DegreesToRadians(A.ForkAngle) * (0.8f + 0.4f * Hash01(C.Seed, Key, 5));
		const float Roll = A.Flat > 0.5f ? 0.f : Hash01(C.Seed, Key, 6) * PI;
		for (int32 Arm = 0; Arm < 2; Arm++)
		{
			if (Level >= 1 && Hash01(C.Seed, Key, 7 + Arm) > C.Present) continue;
			FFrame F = L.Last();
			const FVector3f Axis = Rotate(Nrm, F.D, Roll);
			F.D = Rotate(F.D, Axis, (Arm ? 0.5f : -0.5f) * Spread).GetSafeNormal();
			F.S = (F.S - F.D * F.S.Dot(F.D)).GetSafeNormal();
			Fork(C, F, SegLen * (0.82f + 0.2f * Hash01(C.Seed, Key, 9 + Arm)), W1, Level + 1, V0 + VStep, Key * 2 + Arm + 1);
		}
	}

	/** Where a frond leaves the holdfast: azimuth and its lean off the vertical. */
	FFrame Leave(const FPlantAlga& A, uint32 Seed, int32 I, int32 N, float Radius)
	{
		const float Az = 2.f * PI * (I + 0.6f * Hash01(Seed, I, 31)) / FMath::Max(N, 1);
		const float Lean = FMath::DegreesToRadians(8.f + (1.f - A.Upright) * 65.f) * (0.5f + Hash01(Seed, I, 32));
		const FVector3f Out(FMath::Cos(Az), FMath::Sin(Az), 0.f);
		FFrame F;
		F.P = Out * (Radius * Hash01(Seed, I, 33));
		F.D = (FVector3f(0.f, 0.f, 1.f) * FMath::Cos(Lean) + Out * FMath::Sin(Lean)).GetSafeNormal();
		F.S = FVector3f(-Out.Y, Out.X, 0.f);
		F.S = Rotate(F.S, F.D, Hash01(Seed, I, 34) * PI);
		F.S = (F.S - F.D * F.S.Dot(F.D)).GetSafeNormal();
		return F;
	}

	void Ribbons(FCtx& C)
	{
		const FPlantAlga& A = C.A;
		const int32 Shoots = FMath::Max(1, FMath::RoundToInt(A.Fronds * (0.4f + 0.6f * C.Present)));
		for (int32 Sh = 0; Sh < Shoots; Sh++)
		{
			// a shoot: its sheath at the bed, the leaves fanning a little out of it, the youngest innermost and shortest
			const float Az = Hash01(C.Seed, Sh, 41) * 2.f * PI, Rr = 100.f * A.Clump * FMath::Sqrt(Hash01(C.Seed, Sh, 42));
			const FVector3f At(Rr * FMath::Cos(Az), Rr * FMath::Sin(Az), 0.f);
			const float Shoot = 0.6f + 0.4f * Hash01(C.Seed, Sh, 43);
			for (int32 L = 0; L < A.LeavesPerShoot; L++)
			{
				const float Age = (L + 1.f) / A.LeavesPerShoot;
				const float Len = C.Height * Shoot * (0.45f + 0.55f * Age) * (0.85f + 0.3f * Hash01(C.Seed, Sh, L, 44));
				const float Lean = FMath::DegreesToRadians(4.f + 14.f * Age + (1.f - A.Upright) * 50.f);
				const float La = Az + (L % 2 ? PI : 0.f) + 0.3f * (Hash01(C.Seed, Sh, L, 45) - 0.5f);
				const FVector3f Out(FMath::Cos(La), FMath::Sin(La), 0.f);
				FFrame F;
				F.P = At + FVector3f(0.f, 0.f, 1.5f);
				F.D = (FVector3f(0.f, 0.f, 1.f) * FMath::Cos(Lean) + Out * FMath::Sin(Lean)).GetSafeNormal();
				F.S = FVector3f(-Out.Y, Out.X, 0.f);
				const int32 Segs = FMath::Clamp(FMath::RoundToInt(Len / 9.f), 4, 10);
				const float Arch = (0.25f + 0.6f * Age) * (1.2f - A.Upright);
				TArray<FFrame> Ln = Line(F, Len, Segs, Arch, FMath::DegreesToRadians(A.Twist) * (Hash01(C.Seed, Sh, L, 46) - 0.3f), 0.05f, C.Seed + Sh * 31 + L);
				TArray<float> Half;
				for (int32 I = 0; I <= Segs; I++)
				{
					const float T = float(I) / Segs;
					// a strap leaf: even width, a blunt rounded tip
					Half.Add(50.f * A.Width * (T < 0.92f ? 1.f : FMath::Sqrt(FMath::Max(0.f, 1.f - (T - 0.92f) / 0.08f)) * 0.8f + 0.2f));
				}
				Strip(C.G, Leaves, Ln, Half, 0.f, 1.f, 0.f, 0.f, C.Seed ^ (Sh * 977 + L));
			}
			// the sheath binding the shoot's leaves at the bed
			TArray<FFrame> Sheath = { FFrame{ At, FVector3f(0.f, 0.f, 1.f), FVector3f(1.f, 0.f, 0.f) }, FFrame{ At + FVector3f(0.f, 0.f, 6.f), FVector3f(0.f, 0.f, 1.f), FVector3f(1.f, 0.f, 0.f) } };
			Tube(C.G, Stems, Sheath, { 50.f * A.Width * 0.9f, 50.f * A.Width * 0.7f }, 4, 0.f, 0.1f);
		}
	}

	void Sheets(FCtx& C)
	{
		const FPlantAlga& A = C.A;
		const int32 N = FMath::Max(1, FMath::RoundToInt(A.Fronds * (0.5f + 0.5f * C.Present)));
		for (int32 I = 0; I < N; I++)
		{
			FFrame F = Leave(A, C.Seed, I, N, 100.f * A.Clump);
			const float Len = C.Height * (0.6f + 0.4f * Hash01(C.Seed, I, 51));
			const int32 Segs = 7;
			TArray<FFrame> Ln = Line(F, Len, Segs, (1.f - A.Upright) * 1.2f, 0.6f * (Hash01(C.Seed, I, 52) - 0.5f), 0.15f, C.Seed + I);
			TArray<float> Half;
			const float W = 50.f * A.Width * (0.7f + 0.5f * Hash01(C.Seed, I, 53));
			for (int32 K = 0; K <= Segs; K++)
			{
				const float T = float(K) / Segs;
				// a broad rounded sheet from a narrow base
				Half.Add(W * FMath::Max(0.12f, FMath::Sin(PI * FMath::Clamp(T * 0.85f + 0.08f, 0.f, 1.f))));
			}
			Strip(C.G, Leaves, Ln, Half, 0.f, 1.f, 0.f, A.Ruffle, C.Seed ^ (I * 131));
		}
	}

	void Forked(FCtx& C)
	{
		const FPlantAlga& A = C.A;
		const int32 N = FMath::Max(1, FMath::RoundToInt(A.Fronds * (0.5f + 0.5f * C.Present)));
		const float SegLen = C.Height / (A.Forks + 1.f) * 1.05f;
		for (int32 I = 0; I < N; I++)
		{
			FFrame F = Leave(A, C.Seed, I, N, 100.f * A.Clump);
			Fork(C, F, SegLen * (0.75f + 0.4f * Hash01(C.Seed, I, 61)), 50.f * A.Width, 0, 0.f, uint32(I + 1) * 7919u);
		}
		// the holdfast: a small disc of stems
		TArray<FFrame> Hold = { FFrame{ FVector3f(0.f, 0.f, -0.5f), FVector3f(0.f, 0.f, 1.f), FVector3f(1.f, 0.f, 0.f) },
			FFrame{ FVector3f(0.f, 0.f, 1.5f), FVector3f(0.f, 0.f, 1.f), FVector3f(1.f, 0.f, 0.f) } };
		Tube(C.G, Stems, Hold, { 100.f * A.Clump * 0.4f + 1.f, 100.f * A.Clump * 0.25f + 0.6f }, 6, 0.f, 0.1f);
	}

	void Filaments(FCtx& C)
	{
		// a tuft of fine branched threads: each thread a pair of crossed narrow strips (a round thread too thin to model)
		const FPlantAlga& A = C.A;
		const int32 N = FMath::Max(3, FMath::RoundToInt(A.Fronds * C.Present));
		const float SegLen = C.Height / (A.Forks + 1.f);
		for (int32 I = 0; I < N; I++)
		{
			FFrame F = Leave(A, C.Seed, I, N, 100.f * A.Clump);
			TFunction<void(const FFrame&, float, int32, uint32)> Thread = [&](const FFrame& S, float Len, int32 Level, uint32 Key)
			{
				TArray<FFrame> L = Line(S, Len, 3, (1.f - A.Upright) * 0.8f, 0.4f, 0.35f, C.Seed ^ Key);
				const TArray<float> Half = { 50.f * A.Width, 50.f * A.Width, 50.f * A.Width * 0.8f, 50.f * A.Width * 0.5f };
				const float V0 = float(Level) / (A.Forks + 1), V1 = float(Level + 1) / (A.Forks + 1);
				Strip(C.G, Leaves, L, Half, V0, V1, 0.f, 0.f, Key);
				TArray<FFrame> L2 = L;
				for (FFrame& Fr : L2) Fr.S = FVector3f::CrossProduct(Fr.D, Fr.S).GetSafeNormal();
				Strip(C.G, Leaves, L2, Half, V0, V1, 0.f, 0.f, Key + 1);
				if (Level >= A.Forks) return;
				for (int32 Arm = 0; Arm < 2; Arm++)
				{
					FFrame Nx = L.Last();
					Nx.D = Rotate(Nx.D, Rotate(Nx.S, Nx.D, Hash01(C.Seed, Key, Arm, 71) * PI), (Arm ? 0.5f : -0.5f) * FMath::DegreesToRadians(A.ForkAngle)).GetSafeNormal();
					Nx.S = AnyPerp(Nx.D);
					Thread(Nx, Len * 0.8f, Level + 1, Key * 2 + Arm + 1);
				}
			};
			Thread(F, SegLen * (0.7f + 0.6f * Hash01(C.Seed, I, 72)), 0, uint32(I + 3) * 104729u);
		}
	}

	void Kelp(FCtx& C)
	{
		const FPlantAlga& A = C.A;
		// the holdfast: claw-like branched roots gripping the stone
		for (int32 H = 0; H < 7; H++)
		{
			const float Az = 2.f * PI * H / 7.f + 0.4f * Hash01(C.Seed, H, 81);
			const FVector3f Out(FMath::Cos(Az), FMath::Sin(Az), 0.f);
			FFrame F{ FVector3f(0.f, 0.f, 4.f), (Out * 0.7f - FVector3f(0.f, 0.f, 0.7f)).GetSafeNormal(), FVector3f(-Out.Y, Out.X, 0.f) };
			TArray<FFrame> L = Line(F, 7.f + 4.f * Hash01(C.Seed, H, 82), 3, 0.6f, 0.f, 0.2f, C.Seed + H);
			Tube(C.G, Stems, L, { 1.f, 0.9f, 0.7f, 0.4f }, 4, 0.f, 0.05f);
		}
		// the stalk: stiff, a little bent
		const float StipeLen = C.Height * A.Stipe;
		FFrame F{ FVector3f(0.f, 0.f, 2.f), FVector3f(0.05f, 0.f, 1.f).GetSafeNormal(), FVector3f(0.f, 1.f, 0.f) };
		TArray<FFrame> Stalk = Line(F, StipeLen, 5, 0.25f * (1.f - A.Upright), 0.f, 0.06f, C.Seed);
		TArray<float> R;
		for (int32 I = 0; I <= 5; I++) R.Add(100.f * A.StipeRadius * (1.f - 0.35f * I / 5.f));
		Tube(C.G, Stems, Stalk, R, 6, 0.f, 0.15f);
		// the blade: broad from a wedge at the stalk's top, split into fingers above the palm
		const FFrame Top = Stalk.Last();
		const float BladeLen = C.Height - StipeLen;
		const int32 Fingers = FMath::Max(1, FMath::RoundToInt(A.Fingers * (0.6f + 0.4f * C.Present)));
		const float Palm = 0.22f;
		const float HalfW = 50.f * A.Width;
		TArray<FFrame> P = Line(Top, BladeLen * Palm, 3, 0.15f, 0.f, 0.f, C.Seed + 5);
		Strip(C.G, Leaves, P, { HalfW * 0.15f, HalfW * 0.6f, HalfW * 0.9f, HalfW }, 0.f, Palm, 0.15f, 0.f, C.Seed);
		for (int32 K = 0; K < Fingers; K++)
		{
			const float T = Fingers > 1 ? (K + 0.5f) / Fingers * 2.f - 1.f : 0.f;
			FFrame S = P.Last();
			S.P += S.S * (T * HalfW * (1.f - 1.f / Fingers));
			S.D = Rotate(S.D, FVector3f::CrossProduct(S.D, S.S).GetSafeNormal(), T * 0.25f).GetSafeNormal();
			TArray<FFrame> L = Line(S, BladeLen * (1.f - Palm) * (0.8f + 0.3f * Hash01(C.Seed, K, 91)), 8, (1.f - A.Upright) * 1.6f + 0.2f, 0.5f * (Hash01(C.Seed, K, 92) - 0.5f), 0.08f, C.Seed + K * 17);
			TArray<float> Half;
			const float Wf = HalfW / Fingers * 1.05f;
			for (int32 I = 0; I <= 8; I++) Half.Add(Wf * (I < 7 ? 1.f - 0.1f * I / 7.f : 0.5f));
			Strip(C.G, Leaves, L, Half, Palm, 1.f, 0.f, A.Ruffle, C.Seed ^ (K * 7));
		}
	}

	FLinearColor C3(float R, float G, float B) { return FLinearColor(R, G, B); }

	/** The common setup of an alga: the herb's form, always green under water (no frost), no pot. */
	FPlantSpecies Alga(const TCHAR* Name, EPlantAlgaForm Form, float Height, float Spread)
	{
		FPlantSpecies S;
		S.Name = Name;
		S.Herb.Form = EPlantHerbForm::Alga;
		S.Herb.Alga.Form = Form;
		S.Herb.Height = Height;
		S.Herb.Spread = Spread;
		S.Herb.MatureYears = 1.f;
		S.Herb.Flower = EPlantFlowerForm::None;
		S.bEvergreen = true;
		S.PhenologyJitter = 8.f;
		S.LifeMedian = 20.f;
		S.LifeMax = 40.f;
		return S;
	}
}

PlantHerbs::FState PlantAlgae::State(const FPlantSpecies& S, uint32 Seed, float Age, float Day)
{
	const FPlantAlga& A = S.Herb.Alga;
	FState St;
	const float D = Day - (Hash01(Seed, 0x5EA5u) * 2.f - 1.f) * S.PhenologyJitter;
	St.Maturity = FMath::Clamp(1.f - FMath::Exp(-FMath::Max(Age, 0.f) / FMath::Max(S.Herb.MatureYears, 0.05f)), 0.05f, 1.f);
	// the year: a cosine from the peak day to half a year off it
	const float Year = 0.5f + 0.5f * FMath::Cos(2.f * PI * (D - A.PeakDay) / 365.f);
	St.LeafScale = FMath::Lerp(A.LowScale, 1.f, Year);
	St.Present = FMath::Lerp(FMath::Max(A.LowScale, 0.15f), 1.f, Year);
	St.bAlive = St.LeafScale > 0.05f;
	// tips brown and fray after the peak, through the autumn (eelgrass sheds its old leaves then)
	float Since = D - A.PeakDay;
	if (Since < 0.f) Since += 365.f;
	St.Wither = A.TipBrown * Smooth(10.f, 70.f, Since) * (1.f - Smooth(150.f, 200.f, Since));
	St.LeafColour = S.SummerLeafColour;
	const FPlantHerb& H = S.Herb;
	St.Bloom = (A.Receptacles > 0.f && H.BloomStart > 0.f) ? Smooth(H.BloomStart, H.BloomStart + 15.f, D) * (1.f - Smooth(H.BloomEnd - 15.f, H.BloomEnd, D)) : 0.f;
	auto Q = [](float V, float Steps) { return (uint32)FMath::RoundToInt(FMath::Clamp(V, 0.f, 1.f) * Steps); };
	St.Key = HashCombineFast(HashCombineFast(Q(St.Maturity, 20.f), Q(St.LeafScale, 12.f)), HashCombineFast(Q(St.Present, 12.f), St.Bloom > 0.5f ? 7u : 3u));
	return St;
}

void PlantAlgae::Build(const FPlantSpecies& S, uint32 Seed, const FState& St, FHerbGeometry& Out)
{
	const FPlantAlga& A = S.Herb.Alga;
	FCtx C{ S, A, Out, Seed, 100.f * S.Herb.Height * St.LeafScale * (0.35f + 0.65f * St.Maturity) * (0.8f + 0.4f * Hash01(Seed, 1)), St.Present, St.Bloom > 0.5f };
	switch (A.Form)
	{
	case EPlantAlgaForm::Ribbons: Ribbons(C); break;
	case EPlantAlgaForm::Sheet: Sheets(C); break;
	case EPlantAlgaForm::Forked: Forked(C); break;
	case EPlantAlgaForm::Filaments: Filaments(C); break;
	case EPlantAlgaForm::Kelp: Kelp(C); break;
	}
}

// The sea's plants off the Sambian coast (brackish, ~7 psu: a poor flora of tough survivors), drawn by
// ULyudSeabedComponent (docs/underwater.md).
TArray<FPlantSpecies> PlantHerbs::SeaSpecies()
{
	TArray<FPlantSpecies> Out;
	{
		// Zostera marina, eelgrass (взморник): meadows on sand 1.5-7 m deep, shoots of 3-5 strap leaves 4-8 mm wide and up to
		// a metre long, longest in August; in autumn the old leaves brown from the tips and are shed, washed up in drifts
		FPlantSpecies S = Alga(TEXT("Eelgrass"), EPlantAlgaForm::Ribbons, 0.85f, 0.25f);
		FPlantAlga& A = S.Herb.Alga;
		// a patch of a meadow: shoots a hand apart (the leaves drawn a touch wider than life so they read at a few metres)
		A.Fronds = 9;
		A.LeavesPerShoot = 4;
		A.Width = 0.009f;
		A.Upright = 0.88f;
		A.Twist = 90.f;
		A.Clump = 0.3f;
		A.PeakDay = 225.f;
		A.LowScale = 0.45f;
		A.TipBrown = 0.35f;
		A.Translucent = 0.55f;
		S.SummerLeafColour = C3(0.11f, 0.24f, 0.05f);
		S.AutumnLeafColour = C3(0.2f, 0.16f, 0.06f);
		S.Herb.StemColour = C3(0.12f, 0.12f, 0.06f);
		Out.Add(S);
	}
	{
		// Fucus vesiculosus, bladderwrack (пузырчатый фукус): on stones 0.5-6 m, olive-brown flat fronds 1-2 cm wide forking
		// with a midrib, paired air bladders, warty swollen receptacles at the tips from autumn into spring
		FPlantSpecies S = Alga(TEXT("Bladderwrack"), EPlantAlgaForm::Forked, 0.4f, 0.35f);
		FPlantAlga& A = S.Herb.Alga;
		A.Fronds = 4;
		A.Width = 0.014f;
		A.Forks = 4;
		A.ForkAngle = 38.f;
		A.Flat = 1.f;
		A.bMidrib = true;
		A.Bladders = 0.75f;
		A.BladderSize = 0.011f;
		A.Receptacles = 0.7f;
		A.Ruffle = 0.08f;
		A.Upright = 0.75f;
		A.Clump = 0.02f;
		A.LowScale = 0.9f;
		A.Translucent = 0.3f;
		S.Herb.BloomStart = 260.f;     // receptacles swell from September, shed by May (BloomEnd wraps through the winter)
		S.Herb.BloomEnd = 365.f;
		S.SummerLeafColour = C3(0.16f, 0.12f, 0.03f);
		S.AutumnLeafColour = C3(0.2f, 0.13f, 0.04f);
		S.Herb.StemColour = C3(0.1f, 0.07f, 0.02f);
		S.Herb.CentreColour = C3(0.24f, 0.17f, 0.04f);
		Out.Add(S);
	}
	{
		// Furcellaria lumbricalis, the red weed of the Baltic's stones 4-12 m (фурцеллярия): stiff round dark wine-red stems
		// 2 mm thick, forking many times into a bush 15-25 cm tall; loose-lying mats of it drift on the deeper sand
		FPlantSpecies S = Alga(TEXT("Furcellaria"), EPlantAlgaForm::Forked, 0.22f, 0.2f);
		FPlantAlga& A = S.Herb.Alga;
		A.Fronds = 7;
		A.Width = 0.0022f;
		A.Forks = 5;
		A.ForkAngle = 30.f;
		A.Flat = 0.f;
		A.Upright = 0.85f;
		A.Clump = 0.015f;
		A.LowScale = 0.9f;
		A.Translucent = 0.2f;
		S.SummerLeafColour = C3(0.13f, 0.025f, 0.03f);
		S.AutumnLeafColour = C3(0.12f, 0.03f, 0.03f);
		S.Herb.StemColour = C3(0.09f, 0.02f, 0.02f);
		Out.Add(S);
	}
	{
		// Cladophora glomerata, the green thread weed (кладофора): bright green tufts of branched threads on stones and on
		// the wrack in the first metres, lush in summer, gone in winter
		FPlantSpecies S = Alga(TEXT("Cladophora"), EPlantAlgaForm::Filaments, 0.16f, 0.14f);
		FPlantAlga& A = S.Herb.Alga;
		A.Fronds = 16;
		A.Width = 0.0018f;
		A.Forks = 3;
		A.ForkAngle = 32.f;
		A.Upright = 0.7f;
		A.Clump = 0.03f;
		A.PeakDay = 205.f;
		A.LowScale = 0.25f;
		A.Translucent = 0.7f;
		S.SummerLeafColour = C3(0.12f, 0.3f, 0.04f);
		S.AutumnLeafColour = C3(0.14f, 0.2f, 0.05f);
		S.Herb.StemColour = C3(0.08f, 0.18f, 0.03f);
		Out.Add(S);
	}
	{
		// Ulva lactuca / U. intestinalis, sea lettuce (ульва): thin bright green ruffled sheets on stones and shells in the
		// shallows, summer
		FPlantSpecies S = Alga(TEXT("SeaLettuce"), EPlantAlgaForm::Sheet, 0.18f, 0.2f);
		FPlantAlga& A = S.Herb.Alga;
		A.Fronds = 4;
		A.Width = 0.09f;
		A.Ruffle = 0.6f;
		A.Upright = 0.55f;
		A.Clump = 0.02f;
		A.PeakDay = 200.f;
		A.LowScale = 0.3f;
		A.Translucent = 0.85f;
		S.SummerLeafColour = C3(0.18f, 0.42f, 0.08f);
		S.AutumnLeafColour = C3(0.2f, 0.35f, 0.08f);
		S.Herb.StemColour = C3(0.12f, 0.25f, 0.05f);
		Out.Add(S);
	}
	{
		// Laminaria digitata, oarweed (ламинария): a kelp of the open Baltic's western end, here only a few on the deepest
		// reef stones past 9 m (the brackish water stunts it): a claw holdfast, a stiff stalk and a leathery blade split
		// into fingers, olive brown
		FPlantSpecies S = Alga(TEXT("Oarweed"), EPlantAlgaForm::Kelp, 1.1f, 0.5f);
		FPlantAlga& A = S.Herb.Alga;
		A.Fronds = 1;
		A.Width = 0.3f;
		A.Fingers = 6;
		A.Stipe = 0.35f;
		A.StipeRadius = 0.011f;
		A.Ruffle = 0.25f;
		A.Upright = 0.6f;
		A.LowScale = 0.8f;
		A.PeakDay = 180.f;
		A.Translucent = 0.25f;
		S.SummerLeafColour = C3(0.17f, 0.11f, 0.03f);
		S.AutumnLeafColour = C3(0.17f, 0.11f, 0.03f);
		S.Herb.StemColour = C3(0.1f, 0.07f, 0.02f);
		Out.Add(S);
	}
	return Out;
}
