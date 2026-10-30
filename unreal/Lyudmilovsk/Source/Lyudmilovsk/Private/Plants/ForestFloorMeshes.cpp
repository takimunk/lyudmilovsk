#include "ForestFloorMeshes.h"

#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "Plants/PlantModel.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"

using namespace ForestFloorMeshes;

namespace
{
	using PlantModel::Hash01;
	const FVector3f Up(0.f, 0.f, 1.f);

	/** Smooth value noise in 3D, -1..1 (stones, cushions). */
	float Noise3(uint32 Seed, const FVector3f& P)
	{
		const int32 X = FMath::FloorToInt(P.X), Y = FMath::FloorToInt(P.Y), Z = FMath::FloorToInt(P.Z);
		const float Fx = P.X - X, Fy = P.Y - Y, Fz = P.Z - Z;
		auto S = [](float T) { return T * T * (3.f - 2.f * T); };
		auto H = [&](int32 I, int32 J, int32 K) { return Hash01(Seed, (uint32)I * 73856093u ^ (uint32)J * 19349663u, (uint32)K, 91) * 2.f - 1.f; };
		const float A = FMath::Lerp(FMath::Lerp(H(X, Y, Z), H(X + 1, Y, Z), S(Fx)), FMath::Lerp(H(X, Y + 1, Z), H(X + 1, Y + 1, Z), S(Fx)), S(Fy));
		const float B = FMath::Lerp(FMath::Lerp(H(X, Y, Z + 1), H(X + 1, Y, Z + 1), S(Fx)), FMath::Lerp(H(X, Y + 1, Z + 1), H(X + 1, Y + 1, Z + 1), S(Fx)), S(Fy));
		return FMath::Lerp(A, B, S(Fz));
	}

	float Fbm3(uint32 Seed, const FVector3f& P, int32 Octaves)
	{
		float Sum = 0.f, Amp = 1.f, Norm = 0.f;
		FVector3f Q = P;
		for (int32 O = 0; O < Octaves; O++)
		{
			Sum += Amp * Noise3(Seed + O * 101, Q);
			Norm += Amp;
			Amp *= 0.5f;
			Q = Q * 2.03f + FVector3f(5.1f, 1.7f, 3.3f);
		}
		return Sum / Norm;
	}

	FVector3f AnyPerp(const FVector3f& D)
	{
		return FVector3f::CrossProduct(FMath::Abs(D.Z) < 0.9f ? Up : FVector3f(1.f, 0.f, 0.f), D).GetSafeNormal();
	}

	FVector4f Col(float Bare, float Part) { return FVector4f(Bare, Part, 0.f, 1.f); }

	/**
	 * A tube along P (cm) with radii R, ring frames by parallel transport. UVs in metres (U round the girth at that
	 * ring, V along). Caps: a jagged broken end of bare wood at the start and/or the end.
	 */
	void Tube(FGeom& G, const TArray<FVector3f>& P, const TArray<float>& R, int32 Sides, float Part, uint32 Seed, bool bCap0, bool bCap1, float Jag = 0.35f)
	{
		const int32 N = P.Num();
		if (N < 2) return;
		FVector3f Dir = (P[1] - P[0]).GetSafeNormal();
		FVector3f Nf = AnyPerp(Dir);
		TArray<FVector3f> Ds, Ns;
		float Arc = 0.f;
		const int32 Base = G.Pos.Num();
		for (int32 I = 0; I < N; I++)
		{
			if (I > 0)
			{
				Arc += FVector3f::Dist(P[I], P[I - 1]);
				Dir = (P[FMath::Min(I + 1, N - 1)] - P[I - 1]).GetSafeNormal(UE_SMALL_NUMBER, Dir);
				Nf = (Nf - Dir * FVector3f::DotProduct(Nf, Dir)).GetSafeNormal(UE_SMALL_NUMBER, AnyPerp(Dir));
			}
			Ds.Add(Dir); Ns.Add(Nf);
			const FVector3f Bf = FVector3f::CrossProduct(Dir, Nf);
			const float Girth = 2.f * PI * R[I] * 0.01f;
			for (int32 S = 0; S <= Sides; S++)
			{
				const float A = 2.f * PI * S / Sides;
				const FVector3f Rad = Nf * FMath::Cos(A) + Bf * FMath::Sin(A);
				G.Add(P[I] + Rad * R[I], Rad, Bf * FMath::Cos(A) - Nf * FMath::Sin(A), FVector2f(Girth * S / Sides, Arc * 0.01f), Col(0.f, Part));
			}
		}
		for (int32 I = 0; I + 1 < N; I++)
		{
			for (int32 S = 0; S < Sides; S++)
			{
				const int32 A = Base + I * (Sides + 1) + S, B = A + 1, C = A + Sides + 1, E = C + 1;
				G.Tris.Add(FIntVector3(A, C, B));
				G.Tris.Add(FIntVector3(B, C, E));
			}
		}
		// broken ends: a cap of bare wood, the rim and middle splintered in and out along the axis
		auto Cap = [&](int32 I, float Sign)
		{
			const FVector3f Ax = Ds[I] * Sign, Nf0 = Ns[I], Bf0 = FVector3f::CrossProduct(Ds[I], Nf0);
			const int32 C0 = G.Add(P[I] + Ax * (R[I] * Jag * (Hash01(Seed, I, 7, 1) - 0.3f)), Ax, Nf0, FVector2f(0.f, 0.f), Col(1.f, Part));
			for (int32 S = 0; S <= Sides; S++)
			{
				const float A = 2.f * PI * S / Sides;
				const FVector3f Rad = Nf0 * FMath::Cos(A) + Bf0 * FMath::Sin(A);
				const float Splinter = (S % Sides) == 0 ? 0.f : R[I] * Jag * 0.6f * (Hash01(Seed, I * 64 + S, 7, 2) - 0.5f);
				G.Add(P[I] + Rad * (R[I] * 0.97f) + Ax * Splinter, (Ax * 2.f + Rad).GetSafeNormal(), Nf0,
				      FVector2f(FMath::Cos(A) * R[I] * 0.01f, FMath::Sin(A) * R[I] * 0.01f), Col(1.f, Part));
				if (S > 0)
				{
					// Unreal's front faces wind clockwise seen from outside (as the tube's quads above)
					G.Tris.Add(Sign > 0.f ? FIntVector3(C0, C0 + S + 1, C0 + S) : FIntVector3(C0, C0 + S, C0 + S + 1));
				}
			}
		};
		if (bCap0) Cap(0, -1.f);
		if (bCap1) Cap(N - 1, 1.f);
	}

	/** Vertex normals from the faces (after a displacement). */
	void FaceNormals(FGeom& G, int32 First)
	{
		for (int32 I = First; I < G.Nrm.Num(); I++) G.Nrm[I] = FVector3f::ZeroVector;
		for (const FIntVector3& T : G.Tris)
		{
			if (T.X < First) continue;
			const FVector3f N = FVector3f::CrossProduct(G.Pos[T.Z] - G.Pos[T.X], G.Pos[T.Y] - G.Pos[T.X]);
			G.Nrm[T.X] += N; G.Nrm[T.Y] += N; G.Nrm[T.Z] += N;
		}
		for (int32 I = First; I < G.Nrm.Num(); I++)
		{
			G.Nrm[I] = G.Nrm[I].GetSafeNormal(UE_SMALL_NUMBER, Up);
			G.Tan[I] = AnyPerp(G.Nrm[I]);
		}
	}

	/** An icosphere of radius 1, Detail subdivisions, appended to G (positions only; normals later). */
	void Sphere(FGeom& G, int32 Detail, float Part)
	{
		const float T = (1.f + FMath::Sqrt(5.f)) / 2.f;
		TArray<FVector3f> V = { {-1, T, 0}, {1, T, 0}, {-1, -T, 0}, {1, -T, 0}, {0, -1, T}, {0, 1, T}, {0, -1, -T}, {0, 1, -T},
			{T, 0, -1}, {T, 0, 1}, {-T, 0, -1}, {-T, 0, 1} };
		for (FVector3f& P : V) P.Normalize();
		TArray<FIntVector3> F = { {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6},
			{7, 1, 8}, {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9}, {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1} };
		for (int32 D = 0; D < Detail; D++)
		{
			TMap<uint64, int32> Mid;
			auto M = [&](int32 A, int32 B)
			{
				const uint64 K = ((uint64)FMath::Min(A, B) << 32) | (uint64)FMath::Max(A, B);
				if (const int32* Have = Mid.Find(K)) return *Have;
				return Mid.Add(K, V.Add(((V[A] + V[B]) * 0.5f).GetSafeNormal()));
			};
			TArray<FIntVector3> F2;
			for (const FIntVector3& Tr : F)
			{
				const int32 A = M(Tr.X, Tr.Y), B = M(Tr.Y, Tr.Z), C = M(Tr.Z, Tr.X);
				F2.Append({ {Tr.X, A, C}, {Tr.Y, B, A}, {Tr.Z, C, B}, {A, B, C} });
			}
			F = MoveTemp(F2);
		}
		const int32 Base = G.Pos.Num();
		for (const FVector3f& P : V)
		{
			G.Add(P, P, AnyPerp(P), FVector2f(P.X, P.Y) * 0.5f, Col(0.f, Part));
		}
		// the icosahedron's faces wind clockwise seen from outside; Unreal's front faces are the other way round
		for (const FIntVector3& Tr : F) G.Tris.Add(FIntVector3(Base + Tr.X, Base + Tr.Z, Base + Tr.Y));
	}

	/** Box-projected UVs in metres from object-space positions (stones, cushions: their materials go by position, this keeps the lint honest). */
	void BoxUVs(FGeom& G, int32 First)
	{
		for (int32 I = First; I < G.Pos.Num(); I++)
		{
			const FVector3f& P = G.Pos[I];
			const FVector3f N = G.Nrm[I].GetAbs();
			G.UV[I] = N.Z >= N.X && N.Z >= N.Y ? FVector2f(P.X, P.Y) * 0.01f : N.X >= N.Y ? FVector2f(P.Y, P.Z) * 0.01f : FVector2f(P.X, P.Z) * 0.01f;
		}
	}

	/** B = facing the sky. */
	void SkyMask(FGeom& G)
	{
		for (int32 I = 0; I < G.Pos.Num(); I++) G.Col[I].Z = FMath::Clamp(G.Nrm[I].Z, 0.f, 1.f);
	}

	TArray<FVector3f> Bent(const FVector3f& From, const FVector3f& Dir, float Length, int32 Steps, uint32 Seed, uint32 Key, float Bend, float Droop)
	{
		TArray<FVector3f> P;
		FVector3f D = Dir, At = From;
		const FVector3f Side = AnyPerp(Dir);
		P.Add(At);
		for (int32 I = 1; I <= Steps; I++)
		{
			const float Wob = (Hash01(Seed, Key, I, 3) - 0.5f) * Bend;
			D = (D + Side * Wob + FVector3f(0.f, 0.f, -Droop / Steps)).GetSafeNormal();
			At += D * (Length / Steps);
			P.Add(At);
		}
		return P;
	}
}

void FGeom::Append(const FGeom& O)
{
	const int32 Base = Pos.Num();
	Pos.Append(O.Pos); Nrm.Append(O.Nrm); Tan.Append(O.Tan); UV.Append(O.UV); Col.Append(O.Col);
	for (const FIntVector3& T : O.Tris) Tris.Add(FIntVector3(T.X + Base, T.Y + Base, T.Z + Base));
}

float FGeom::Top() const
{
	float Z = 0.f;
	for (const FVector3f& P : Pos) Z = FMath::Max(Z, P.Z);
	return Z;
}

FGeom ForestFloorMeshes::Stone(uint32 Seed, float Radius, float Flatness, int32 Detail)
{
	FGeom G;
	Sphere(G, Detail, Hash01(Seed, 1));
	const FVector3f Stretch(1.f + 0.35f * Hash01(Seed, 2), 1.f - 0.2f * Hash01(Seed, 3), Flatness);
	for (FVector3f& P : G.Pos)
	{
		// a weathered glacial erratic: rounded, a few broad facets, small pits
		const float Lump = 0.16f * Fbm3(Seed, P * 1.3f, 3) + 0.05f * Noise3(Seed + 9, P * 6.f);
		const float Facet = 0.08f * FMath::Max(0.f, FMath::Abs(Noise3(Seed + 5, P * 0.9f)) - 0.4f);
		P = P * (1.f + Lump - Facet) * Stretch * Radius;
		// sunk a third into the ground, the underside flattened
		P.Z = P.Z < -0.2f * Radius * Flatness ? FMath::Lerp(P.Z, -0.25f * Radius * Flatness, 0.6f) : P.Z;
		P.Z += Radius * Flatness * 0.33f;
	}
	FaceNormals(G, 0);
	BoxUVs(G, 0);
	SkyMask(G);
	return G;
}

FGeom ForestFloorMeshes::Stick(uint32 Seed, float Length, float Radius, int32 Forks, int32 Sides)
{
	FGeom G;
	const float Yaw = (Hash01(Seed, 1) - 0.5f) * 0.3f;
	TArray<FVector3f> P = Bent(FVector3f(-Length * 0.5f, 0.f, Radius * 0.8f), FVector3f(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.f), Length, 6, Seed, 1, 0.25f, 0.f);
	// it lies on the ground: every point at its radius over it (a gentle bow between)
	TArray<float> R;
	for (int32 I = 0; I < P.Num(); I++)
	{
		const float T = (float)I / (P.Num() - 1);
		R.Add(Radius * (1.f - 0.45f * T));
		P[I].Z = R.Last() * 0.85f + Radius * 0.6f * FMath::Sin(PI * T) * Hash01(Seed, 4);
	}
	Tube(G, P, R, Sides, Hash01(Seed, 5), Seed, true, true);
	for (int32 F = 0; F < Forks; F++)
	{
		const float T = 0.2f + 0.6f * Hash01(Seed, F, 6);
		const int32 I = FMath::Clamp(FMath::FloorToInt(T * (P.Num() - 1)), 0, P.Num() - 2);
		const FVector3f Along = (P[I + 1] - P[I]).GetSafeNormal();
		const float Side = Hash01(Seed, F, 7) < 0.5f ? -1.f : 1.f;
		const FVector3f Out = (Along * 0.7f + FVector3f::CrossProduct(Up, Along) * Side + Up * (0.2f + 0.5f * Hash01(Seed, F, 8))).GetSafeNormal();
		const float L = Length * (0.12f + 0.2f * Hash01(Seed, F, 9));
		const float R0 = R[I] * 0.55f;
		TArray<FVector3f> Q = Bent(P[I] - Out * (R[I] * 0.3f), Out, L, 3, Seed, 100 + F, 0.4f, 0.15f);
		TArray<float> Rq = { R0, R0 * 0.8f, R0 * 0.6f, R0 * 0.45f };
		Tube(G, Q, Rq, FMath::Max(3, Sides - 2), Hash01(Seed, F, 10), Seed + F, false, true);
	}
	SkyMask(G);
	return G;
}

FGeom ForestFloorMeshes::Log(uint32 Seed, float Length, float Radius)
{
	FGeom G;
	TArray<FVector3f> P;
	TArray<float> R;
	constexpr int32 Steps = 10;
	for (int32 I = 0; I <= Steps; I++)
	{
		const float T = (float)I / Steps;
		// sunk into the floor by a third of its girth, settling more in the middle, tapering to the top end
		R.Add(Radius * (1.f - 0.3f * T) * (1.f + 0.05f * (Hash01(Seed, I, 1) - 0.5f)));
		P.Add(FVector3f((T - 0.5f) * Length, (Hash01(Seed, I, 2) - 0.5f) * Radius * 0.15f, R.Last() * (0.6f - 0.12f * FMath::Sin(PI * T))));
	}
	Tube(G, P, R, 12, Hash01(Seed, 3), Seed, true, true, 0.5f);
	// stubs of its limbs, broken off short
	const int32 Stubs = 2 + (int32)(Hash01(Seed, 4) * 4.f);
	for (int32 K = 0; K < Stubs; K++)
	{
		const float T = 0.3f + 0.65f * Hash01(Seed, K, 5);
		const int32 I = FMath::Clamp(FMath::FloorToInt(T * Steps), 0, Steps - 1);
		const float A = (Hash01(Seed, K, 6) - 0.5f) * 2.4f;  // round the upper half
		const FVector3f Out = FVector3f(0.25f, FMath::Sin(A), FMath::Cos(A)).GetSafeNormal();
		const float Rs = R[I] * (0.18f + 0.15f * Hash01(Seed, K, 7));
		const float L = R[I] * (0.4f + 1.4f * Hash01(Seed, K, 8));
		TArray<FVector3f> Q = { P[I] + Out * (R[I] * 0.3f), P[I] + Out * (R[I] * 0.9f + L) };
		TArray<float> Rq = { Rs, Rs * 0.75f };
		Tube(G, Q, Rq, 6, Hash01(Seed, K, 9), Seed + K, false, true);
	}
	SkyMask(G);
	return G;
}

FGeom ForestFloorMeshes::Stump(uint32 Seed, float Radius, float Height)
{
	FGeom G;
	constexpr int32 Sides = 16, Rings = 7;
	const float Part = Hash01(Seed, 1);
	const int32 Base = G.Pos.Num();
	auto RadiusAt = [&](float Z, float A)
	{
		const float Flare = 0.65f * FMath::Exp(-FMath::Max(Z, 0.f) / (0.3f * Radius));
		const float Lobes = 0.22f * FMath::Max(0.f, FMath::Cos(5.f * A + Hash01(Seed, 2) * 6.f)) * FMath::Exp(-FMath::Max(Z, 0.f) / (0.45f * Radius));
		return Radius * (1.f + Flare + Lobes) * (1.f + 0.04f * (Hash01(Seed, (uint32)(A * 10.f), 3) - 0.5f));
	};
	for (int32 I = 0; I <= Rings; I++)
	{
		const float Z = -12.f + (Height + 12.f) * (float)I / Rings;
		for (int32 S = 0; S <= Sides; S++)
		{
			const float A = 2.f * PI * (S % Sides) / Sides;
			const float Rr = RadiusAt(Z, A);
			const FVector3f D(FMath::Cos(A), FMath::Sin(A), 0.f);
			G.Add(D * Rr + FVector3f(0.f, 0.f, Z), D, FVector3f(-D.Y, D.X, 0.f), FVector2f(2.f * PI * Radius * 0.01f * S / Sides, (Z + 12.f) * 0.01f), Col(0.f, Part));
		}
	}
	for (int32 I = 0; I < Rings; I++)
	{
		for (int32 S = 0; S < Sides; S++)
		{
			const int32 A = Base + I * (Sides + 1) + S, B = A + 1, C = A + Sides + 1, E = C + 1;
			G.Tris.Add(FIntVector3(A, C, B));
			G.Tris.Add(FIntVector3(B, C, E));
		}
	}
	FaceNormals(G, Base);
	// the top: weathered and cracked, sunk in the middle where it rots, a jagged rim of sound wood
	const int32 C0 = G.Add(FVector3f(0.f, 0.f, Height - Radius * 0.12f), Up, FVector3f(1.f, 0.f, 0.f), FVector2f(0.f, 0.f), Col(1.f, Part));
	for (int32 S = 0; S <= Sides; S++)
	{
		const float A = 2.f * PI * (S % Sides) / Sides;
		const float Rr = RadiusAt(Height, A) * 0.98f;
		const float Z = Height + (S % Sides ? Radius * 0.25f * (Hash01(Seed, S, 4) - 0.4f) * Hash01(Seed, 5) : 0.f);
		G.Add(FVector3f(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, FMath::Min(Z, Height)), (Up * 2.f + FVector3f(FMath::Cos(A), FMath::Sin(A), 0.f)).GetSafeNormal(),
		      FVector3f(1.f, 0.f, 0.f), FVector2f(FMath::Cos(A), FMath::Sin(A)) * Rr * 0.01f, Col(1.f, Part));
		if (S > 0) G.Tris.Add(FIntVector3(C0, C0 + S + 1, C0 + S));
	}
	SkyMask(G);
	return G;
}

FGeom ForestFloorMeshes::Cone(uint32 Seed, float Length, float Radius, float Open)
{
	FGeom G;
	constexpr int32 Sides = 12, Rings = 12;
	const float Part = Hash01(Seed, 1);
	for (int32 I = 0; I <= Rings; I++)
	{
		const float T = (float)I / Rings;
		// egg-shaped: widest a third from the base, a blunt tip
		const float Prof = FMath::Pow(FMath::Max(FMath::Sin(PI * FMath::Clamp(0.04f + 0.94f * T, 0.f, 1.f)), 0.f), 0.65f) * (1.f - 0.25f * T);
		for (int32 S = 0; S <= Sides; S++)
		{
			const float A = 2.f * PI * (S % Sides) / Sides;
			// scales on a spiral: each one a lump, standing out when the cone is open
			const float Spiral = FMath::Sin(A * 5.f + T * Rings * 1.7f);
			const float Scale = 1.f + (0.07f + 0.22f * Open) * FMath::Max(Spiral, 0.f) * FMath::Sin(PI * T);
			const float Rr = Radius * Prof * Scale + 0.05f;
			const FVector3f D(0.f, FMath::Cos(A), FMath::Sin(A));
			G.Add(FVector3f(Length * (T - 0.5f), 0.f, 0.f) + D * Rr, D, FVector3f(0.f, -D.Z, D.Y),
			      FVector2f(2.f * PI * Radius * 0.01f * S / Sides, Length * 0.01f * T), Col(0.f, Part));
		}
	}
	for (int32 I = 0; I < Rings; I++)
	{
		for (int32 S = 0; S < Sides; S++)
		{
			const int32 A = I * (Sides + 1) + S, B = A + 1, C = A + Sides + 1, E = C + 1;
			G.Tris.Add(FIntVector3(A, C, B));
			G.Tris.Add(FIntVector3(B, C, E));
		}
	}
	FaceNormals(G, 0);
	// lying on its side, a little nose down
	for (FVector3f& P : G.Pos) P.Z += Radius * 0.8f;
	SkyMask(G);
	return G;
}

FGeom ForestFloorMeshes::BarkCurl(uint32 Seed, float Size)
{
	FGeom G;
	constexpr int32 Nu = 10, Nv = 3;
	const float W = Size, L = Size * (0.5f + 0.4f * Hash01(Seed, 1));
	const float Curl = 0.7f + 0.9f * Hash01(Seed, 2);   // radians rolled up from each edge: an open trough, not a tube
	const float Thick = 0.25f;                          // cm: the paper-thin bark's two faces
	const float Part = Hash01(Seed, 3);
	for (int32 Face = 0; Face < 2; Face++)
	{
		const int32 Base = G.Pos.Num();
		for (int32 J = 0; J <= Nv; J++)
		{
			for (int32 I = 0; I <= Nu; I++)
			{
				const float U = (float)I / Nu * 2.f - 1.f;               // -1 .. 1 across, rolled up at both edges
				const float A = Curl * FMath::Abs(U);
				const float Rr = W * 0.5f / FMath::Max(Curl, 0.1f);
				const float Sg = U < 0.f ? -1.f : 1.f;
				FVector3f P(L * ((float)J / Nv - 0.5f), Sg * Rr * FMath::Sin(A), Rr * (1.f - FMath::Cos(A)));
				const FVector3f N = FVector3f(0.f, -Sg * FMath::Sin(A), FMath::Cos(A)) * (Face == 0 ? -1.f : 1.f);
				P += N * (Face == 0 ? 0.f : Thick);
				// the outer face is the white bark (the material's birch texture); the inner one is bare brown (R)
				G.Add(P + FVector3f(0.f, 0.f, 0.3f), N, FVector3f(1.f, 0.f, 0.f), FVector2f(U * W * 0.005f, (float)J / Nv * L * 0.01f), Col(Face == 0 ? 0.f : 1.f, Part));
			}
		}
		for (int32 J = 0; J < Nv; J++)
		{
			for (int32 I = 0; I < Nu; I++)
			{
				const int32 A = Base + J * (Nu + 1) + I, B = A + 1, C = A + Nu + 1, E = C + 1;
				if (Face == 0) { G.Tris.Add(FIntVector3(A, C, B)); G.Tris.Add(FIntVector3(B, C, E)); }
				else { G.Tris.Add(FIntVector3(A, B, C)); G.Tris.Add(FIntVector3(B, E, C)); }
			}
		}
	}
	SkyMask(G);
	return G;
}

FGeom ForestFloorMeshes::Cushion(uint32 Seed, float Radius, float Height)
{
	FGeom G;
	Sphere(G, 3, Hash01(Seed, 1));
	for (FVector3f& P : G.Pos)
	{
		const float Lump = 0.22f * Fbm3(Seed, P * 1.8f, 3) + 0.06f * Noise3(Seed + 3, P * 7.f);
		FVector3f Q = P * (1.f + Lump);
		Q.X *= Radius; Q.Y *= Radius * (0.75f + 0.3f * Hash01(Seed, 2));
		// a dome: the lower half pressed flat into the ground
		Q.Z = Q.Z > 0.f ? Q.Z * Height : Q.Z * 8.f - 2.f;
		P = Q;
	}
	FaceNormals(G, 0);
	BoxUVs(G, 0);
	SkyMask(G);
	return G;
}

FGeom ForestFloorMeshes::Roots(uint32 Seed, float TrunkR, int32 Count)
{
	FGeom G;
	for (int32 K = 0; K < Count; K++)
	{
		const float A = 2.f * PI * (K + 0.35f * Hash01(Seed, K, 1)) / Count;
		const FVector3f Out(FMath::Cos(A), FMath::Sin(A), 0.f);
		const float L = TrunkR * (2.5f + 3.f * Hash01(Seed, K, 2));
		const float R0 = TrunkR * (0.28f + 0.12f * Hash01(Seed, K, 3));
		TArray<FVector3f> P;
		TArray<float> R;
		constexpr int32 Steps = 7;
		const FVector3f Side(-Out.Y, Out.X, 0.f);
		for (int32 I = 0; I <= Steps; I++)
		{
			const float T = (float)I / Steps;
			R.Add(FMath::Max(R0 * (1.f - 0.85f * T), 1.2f));
			const float Wander = (Hash01(Seed, K * 16 + I, 4) - 0.5f) * TrunkR * 0.35f * T;
			// from inside the trunk, over the ground, diving into it: half buried by its end
			const float Z = R.Last() * (0.35f - 0.95f * T) + TrunkR * 0.25f * (1.f - T) * (1.f - T);
			P.Add(Out * (TrunkR * 0.6f + L * T) + Side * Wander + FVector3f(0.f, 0.f, Z));
		}
		Tube(G, P, R, 7, Hash01(Seed, K, 5), Seed + K, false, false);
	}
	SkyMask(G);
	return G;
}

FString ForestFloorMeshes::Lint(const FGeom& G, bool bCheckUVs, int32& OutProblems)
{
	int32 Degenerate = 0, Stretched = 0, Overlap = 0, Nan = 0;
	TArray<float> Density;
	for (const FIntVector3& T : G.Tris)
	{
		const FVector3f &A = G.Pos[T.X], &B = G.Pos[T.Y], &C = G.Pos[T.Z];
		if (A.ContainsNaN() || B.ContainsNaN() || C.ContainsNaN()) { Nan++; continue; }
		const float Area = 0.5f * FVector3f::CrossProduct(B - A, C - A).Size();
		if (Area < 1e-3f) { Degenerate++; continue; }
		const FVector2f Ua = G.UV[T.X], Ub = G.UV[T.Y], Uc = G.UV[T.Z];
		const float UvArea = 0.5f * FMath::Abs((Ub.X - Ua.X) * (Uc.Y - Ua.Y) - (Uc.X - Ua.X) * (Ub.Y - Ua.Y));
		// UVs are in metres: texel density = sqrt(uv area / area in m2), 1 where nothing is stretched
		Density.Add(FMath::Sqrt(UvArea / (Area * 1e-4f)));
	}
	if (bCheckUVs)
	{
		for (const float D : Density)
		{
			if (D < 0.33f || D > 3.f) Stretched++;
		}
	}
	// faces of different parts in one plane, one over the other (z-fighting): centroid of one inside the other, within 2 mm
	const int32 N = G.Tris.Num();
	for (int32 I = 0; I < N && N < 6000; I++)
	{
		const FIntVector3& T = G.Tris[I];
		const FVector3f A = G.Pos[T.X], Nn = FVector3f::CrossProduct(G.Pos[T.Y] - A, G.Pos[T.Z] - A).GetSafeNormal();
		if (Nn.IsNearlyZero()) continue;
		for (int32 J = 0; J < N; J++)
		{
			const FIntVector3& O = G.Tris[J];
			if (J == I || FMath::Abs(G.Col[O.X].Y - G.Col[T.X].Y) < 1e-4f) continue;
			const FVector3f Oc = (G.Pos[O.X] + G.Pos[O.Y] + G.Pos[O.Z]) / 3.f;
			if (FMath::Abs(FVector3f::DotProduct(Oc - A, Nn)) > 0.2f) continue;
			const FVector3f On = FVector3f::CrossProduct(G.Pos[O.Y] - G.Pos[O.X], G.Pos[O.Z] - G.Pos[O.X]).GetSafeNormal();
			if (FMath::Abs(FVector3f::DotProduct(On, Nn)) < 0.98f) continue;
			// barycentric test in the plane
			const FVector3f V0 = G.Pos[T.Y] - A, V1 = G.Pos[T.Z] - A, V2 = Oc - A;
			const float D00 = V0 | V0, D01 = V0 | V1, D11 = V1 | V1, D20 = V2 | V0, D21 = V2 | V1, Den = D00 * D11 - D01 * D01;
			if (FMath::Abs(Den) < 1e-8f) continue;
			const float V = (D11 * D20 - D01 * D21) / Den, W = (D00 * D21 - D01 * D20) / Den;
			if (V > 0.02f && W > 0.02f && V + W < 0.98f) { Overlap++; break; }
		}
	}
	OutProblems = Degenerate + Stretched + Overlap + Nan;
	return FString::Printf(TEXT("%d tris, %d verts; degenerate %d, uv-stretched %d%s, coplanar overlaps %d, nan %d"), N, G.Pos.Num(), Degenerate, Stretched,
		bCheckUVs ? TEXT("") : TEXT(" (object-space textured)"), Overlap, Nan);
}

UStaticMesh* ForestFloorMeshes::Build(UObject* Outer, FName Name, const TArray<const FGeom*>& Lods, const TArray<float>& ScreenSizes, bool bCollision)
{
	TArray<FMeshDescription> Descs;
	Descs.SetNum(Lods.Num());
	for (int32 L = 0; L < Lods.Num(); L++)
	{
		const FGeom& D = *Lods[L];
		FMeshDescription& MD = Descs[L];
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> VTan = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> VSign = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> VUV = Attr.GetVertexInstanceUVs();
		TVertexInstanceAttributesRef<FVector4f> VCol = Attr.GetVertexInstanceColors();
		VUV.SetNumChannels(1);
		const FPolygonGroupID PG = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[PG] = TEXT("Debris");
		TArray<FVertexInstanceID> Inst;
		Inst.Reserve(D.Pos.Num());
		for (int32 I = 0; I < D.Pos.Num(); I++)
		{
			const FVertexID V = MD.CreateVertex();
			VPos[V] = D.Pos[I].ContainsNaN() ? FVector3f::ZeroVector : D.Pos[I];
			const FVertexInstanceID VI = MD.CreateVertexInstance(V);
			const FVector3f N = D.Nrm[I].GetSafeNormal(UE_SMALL_NUMBER, Up);
			FVector3f T = (D.Tan[I] - N * FVector3f::DotProduct(D.Tan[I], N)).GetSafeNormal();
			if (T.IsNearlyZero()) T = AnyPerp(N);
			VNrm[VI] = N; VTan[VI] = T; VSign[VI] = 1.f;
			VUV.Set(VI, 0, D.UV[I]);
			VCol[VI] = D.Col[I];
			Inst.Add(VI);
		}
		for (const FIntVector3& T : D.Tris) MD.CreateTriangle(PG, { Inst[T.X], Inst[T.Y], Inst[T.Z] });
	}
	UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, MakeUniqueObjectName(Outer, UStaticMesh::StaticClass(), Name), RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
	Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Debris")));
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bMarkPackageDirty = false;
	Params.bBuildSimpleCollision = bCollision;
	Params.bCommitMeshDescription = false;
	Params.bFastBuild = true;
	TArray<const FMeshDescription*> Ptrs;
	for (const FMeshDescription& MD : Descs) Ptrs.Add(&MD);
	Mesh->BuildFromMeshDescriptions(Ptrs, Params);
	if (UBodySetup* Body = Mesh->GetBodySetup())
	{
		Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	}
	if (FStaticMeshRenderData* RD = Mesh->GetRenderData())
	{
		for (int32 L = 0; L < Lods.Num() && L < MAX_STATIC_MESH_LODS; L++)
		{
			RD->ScreenSize[L] = ScreenSizes.IsValidIndex(L) ? ScreenSizes[L] : (L == 0 ? 1.f : 0.1f / L);
		}
	}
	return Mesh;
}
