#include "Murmuration/StarlingMesh.h"

#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"

namespace
{
	// Geometry authored as in starling.py: metres, three.js axes (beak +Z, up +Y, wings along X).
	using FP = FVector3f;

	// body stations: z, half-width, half-height, centre-y
	const float BODY[][4] = {
		{ -0.052f, 0.012f, 0.007f, 0.004f },
		{ -0.040f, 0.0165f, 0.012f, 0.002f },
		{ -0.022f, 0.020f, 0.018f, 0.000f },
		{ -0.004f, 0.0235f, 0.021f, -0.001f },
		{ 0.014f, 0.024f, 0.022f, 0.000f },
		{ 0.030f, 0.021f, 0.020f, 0.003f },
		{ 0.043f, 0.0165f, 0.0165f, 0.006f },
		{ 0.055f, 0.0155f, 0.0155f, 0.009f },
		{ 0.066f, 0.0135f, 0.013f, 0.009f },
		{ 0.075f, 0.0085f, 0.008f, 0.007f },
		{ 0.081f, 0.0042f, 0.0042f, 0.0055f },
	};
	const FP BEAK_TIP(0.f, 0.0035f, 0.106f);
	const FP REAR_TIP(0.f, 0.004f, -0.058f);
	constexpr int32 RING = 10;

	// wing stations along the span: x, leading-edge z, trailing-edge z, thickness
	const float WING[][4] = {
		{ 0.012f, 0.030f, -0.034f, 0.007f },
		{ 0.030f, 0.037f, -0.046f, 0.0055f },   // shoulder
		{ 0.075f, 0.040f, -0.050f, 0.0035f },   // wrist, most forward point
		{ 0.120f, 0.018f, -0.050f, 0.0022f },   // mid hand (primaries)
		{ 0.160f, -0.012f, -0.048f, 0.0012f },  // outer primaries
		{ 0.190f, -0.0445f, -0.0465f, 0.0003f }, // pointed tip
	};

	// The wing joint of the flap shader (src/scene/birds.ts): vertices farther than this from the
	// body axis belong to the wing.
	constexpr float SHOULDER = 0.03f;

	struct FShell
	{
		TArray<FP> V;
		TArray<FIntVector3> T;
		int32 Add(const FP& P) { return V.Add(P); }
		void Tri(int32 A, int32 B, int32 C) { T.Add(FIntVector3(A, B, C)); }
		void Poly(const TArray<int32>& Idx)
		{
			for (int32 K = 1; K + 1 < Idx.Num(); K++) Tri(Idx[0], Idx[K], Idx[K + 1]);
		}
	};

	void BuildBody(FShell& S)
	{
		TArray<TArray<int32>> Rings;
		for (const auto& St : BODY)
		{
			TArray<int32>& R = Rings.AddDefaulted_GetRef();
			for (int32 K = 0; K < RING; K++)
			{
				const float T = 2.f * PI * K / RING;
				const float C = FMath::Cos(T), Sn = FMath::Sin(T);
				const float HH = Sn > 0.f ? St[2] : St[2] * 0.9f; // flatter belly, rounder back
				R.Add(S.Add(FP(St[1] * C, St[3] + HH * Sn, St[0])));
			}
		}
		for (int32 R = 0; R + 1 < Rings.Num(); R++)
		{
			for (int32 K = 0; K < RING; K++)
			{
				const int32 J = (K + 1) % RING;
				S.Poly({ Rings[R][K], Rings[R][J], Rings[R + 1][J], Rings[R + 1][K] });
			}
		}
		const int32 Tip = S.Add(BEAK_TIP), Rear = S.Add(REAR_TIP);
		for (int32 K = 0; K < RING; K++)
		{
			const int32 J = (K + 1) % RING;
			S.Tri(Rings.Last()[K], Rings.Last()[J], Tip);
			S.Tri(Rings[0][J], Rings[0][K], Rear);
		}
	}

	/** Sections: vertex rings of equal length; quads between them and caps at both ends. */
	void Loft(FShell& S, const TArray<TArray<FP>>& Sections)
	{
		TArray<TArray<int32>> Rows;
		for (const TArray<FP>& Sec : Sections)
		{
			TArray<int32>& Row = Rows.AddDefaulted_GetRef();
			for (const FP& P : Sec) Row.Add(S.Add(P));
		}
		const int32 N = Sections[0].Num();
		for (int32 R = 0; R + 1 < Rows.Num(); R++)
		{
			for (int32 K = 0; K < N; K++)
			{
				const int32 J = (K + 1) % N;
				S.Poly({ Rows[R][K], Rows[R + 1][K], Rows[R + 1][J], Rows[R][J] });
			}
		}
		S.Poly(Rows[0]);
		TArray<int32> Last = Rows.Last();
		Algo::Reverse(Last);
		S.Poly(Last);
	}

	void BuildTail(FShell& S, float Droop = 0.f)
	{
		// short, square tail: z, half-width, thickness, y
		const float ST[][4] = { { -0.040f, 0.012f, 0.0035f, 0.004f }, { -0.072f, 0.019f, 0.0025f, 0.002f - Droop * 0.5f }, { -0.101f, 0.0235f, 0.0016f, -Droop } };
		TArray<TArray<FP>> Secs;
		for (const auto& T : ST)
		{
			Secs.Add({ FP(-T[1], T[3], T[0]), FP(0.f, T[3] - T[2] * 0.5f, T[0]), FP(T[1], T[3], T[0]), FP(0.f, T[3] + T[2], T[0]) });
		}
		Loft(S, Secs);
	}

	void BuildWing(FShell& S, float Side)
	{
		TArray<TArray<FP>> Secs;
		for (const auto& W : WING)
		{
			const float X = W[0], LE = W[1], TE = W[2], TH = W[3];
			const float Y = 0.005f + 0.035f * (X - 0.012f); // slight dihedral, still flat-ish
			const float C = LE - TE;
			const float Zs[] = { LE, LE - 0.25f * C, LE - 0.6f * C, TE, LE - 0.6f * C, LE - 0.25f * C };
			const float Ys[] = { Y, Y + TH, Y + TH * 0.6f, Y, Y - TH * 0.15f, Y - TH * 0.3f };
			TArray<FP> Sec;
			for (int32 K = 0; K < 6; K++) Sec.Add(FP(X * Side, Ys[K], Zs[K]));
			if (Side < 0.f) Algo::Reverse(Sec);
			Secs.Add(Sec);
		}
		Loft(S, Secs);
	}

	/** Folded wings lie along the upper flank, the primaries crossing over the tail base. */
	void BuildFoldedWing(FShell& S, float Side)
	{
		// z, inner-top (x, y), outer (x, y), lower (x, y)
		const float ST[][7] = {
			{ 0.034f, 0.011f, 0.018f, 0.020f, 0.012f, 0.019f, 0.002f },
			{ 0.010f, 0.010f, 0.022f, 0.026f, 0.012f, 0.024f, -0.006f },
			{ -0.025f, 0.007f, 0.018f, 0.022f, 0.012f, 0.019f, -0.002f },
			{ -0.062f, 0.004f, 0.010f, 0.011f, 0.008f, 0.009f, 0.005f },
			{ -0.082f, 0.002f, 0.007f, 0.004f, 0.0065f, 0.003f, 0.006f },
		};
		TArray<TArray<FP>> Secs;
		for (const auto& T : ST)
		{
			const float Z = T[0];
			const FVector2f A(T[1], T[2]), B(T[3], T[4]), C(T[5], T[6]);
			const FVector2f Inner(A.X * 0.9f, (A.Y + C.Y) / 2.f);
			TArray<FP> Sec = { FP(A.X * Side, A.Y, Z), FP(B.X * Side, B.Y, Z), FP(C.X * Side, C.Y, Z), FP(Inner.X * Side, Inner.Y, Z) };
			if (Side < 0.f) Algo::Reverse(Sec);
			Secs.Add(Sec);
		}
		Loft(S, Secs);
	}

	/** A closed thin prism (n sides) between two points. */
	void Prism(FShell& S, const FP& P0, const FP& P1, float R, int32 N = 3)
	{
		const FP D = (P1 - P0).GetSafeNormal();
		FP Side = FP::CrossProduct(D, FP(0.f, 1.f, 0.f));
		if (Side.SizeSquared() < 1e-6f) Side = FP(1.f, 0.f, 0.f);
		Side.Normalize();
		const FP Up = FP::CrossProduct(Side, D);
		TArray<TArray<FP>> Secs;
		for (const FP& At : { P0, P1 })
		{
			TArray<FP>& Sec = Secs.AddDefaulted_GetRef();
			for (int32 K = 0; K < N; K++)
			{
				const float A = 2.f * PI * K / N;
				Sec.Add(At + Side * (FMath::Cos(A) * R) + Up * (FMath::Sin(A) * R));
			}
		}
		Loft(S, Secs);
	}

	void BuildLegs(TArray<FShell>& Shells)
	{
		for (const float Side : { 1.f, -1.f })
		{
			const float X = 0.007f * Side;
			const FP Top(X, -0.012f, -0.002f), Knee(X, -0.030f, 0.002f), Foot(X, -0.046f, 0.004f);
			Prism(Shells.AddDefaulted_GetRef(), Top, Knee, 0.0016f);
			Prism(Shells.AddDefaulted_GetRef(), Knee, Foot, 0.0016f);
			// toes: forward and back
			for (const FP& D : { FP(0.f, 0.f, 0.018f), FP(0.004f * Side, 0.f, 0.014f), FP(0.f, 0.f, -0.012f) })
			{
				Prism(Shells.AddDefaulted_GetRef(), Foot, Foot + D, 0.001f);
			}
		}
	}

	/** The flap vertex shader of birds.ts, with the folded share at 0. */
	FP Flap(FP P, float Beat, float Amp)
	{
		const float AX = FMath::Abs(P.X);
		if (AX <= SHOULDER) return P;
		const float S = FMath::Sign(P.X);
		const float W = AX - SHOULDER;
		// the upstroke is quicker and bends the outer wing (wrist flex)
		const float Ang = (0.85f * Beat + 0.15f * Amp) * 0.95f;
		const float C = FMath::Cos(Ang), Sn = FMath::Sin(Ang);
		const float TipT = FMath::Clamp((W - 0.06f) / (0.17f - 0.06f), 0.f, 1.f);
		const float Tip = TipT * TipT * (3.f - 2.f * TipT);
		const float Bend = -Beat * 0.35f * Tip;
		P.X = S * (SHOULDER + W * C - W * Tip * 0.1f * Amp);
		P.Y += W * Sn + Bend * W;
		return P;
	}

	/** three.js metres -> Unreal centimetres: (x, y, z) -> (x, z, y) * 100 (a mirror, so windings flip). */
	FVector3f ToUnreal(const FP& P) { return FVector3f(P.X, P.Z, P.Y) * 100.f; }
}

namespace
{
	UStaticMesh* MakeMesh(UObject* Outer, FName Name, const TArray<FShell>& Shells, UMaterialInterface* Material);
}

UStaticMesh* StarlingMesh::Build(UObject* Outer, FName Name, float Beat, float Amp, UMaterialInterface* Material)
{
	TArray<FShell> Shells;
	BuildBody(Shells.AddDefaulted_GetRef());
	BuildTail(Shells.AddDefaulted_GetRef());
	BuildWing(Shells.AddDefaulted_GetRef(), 1.f);
	BuildWing(Shells.AddDefaulted_GetRef(), -1.f);
	for (FShell& S : Shells)
	{
		for (FP& V : S.V) V = Flap(V, Beat, Amp);
	}
	return MakeMesh(Outer, Name, Shells, Material);
}

UStaticMesh* StarlingMesh::BuildPerched(UObject* Outer, FName Name, UMaterialInterface* Material)
{
	// body pitched nose-up 18 degrees, wings folded, legs (starling.py's perched variant)
	TArray<FShell> Shells;
	BuildBody(Shells.AddDefaulted_GetRef());
	BuildTail(Shells.AddDefaulted_GetRef(), 0.006f);
	BuildFoldedWing(Shells.AddDefaulted_GetRef(), 1.f);
	BuildFoldedWing(Shells.AddDefaulted_GetRef(), -1.f);
	const float A = FMath::DegreesToRadians(18.f), C = FMath::Cos(A), Sn = FMath::Sin(A);
	for (FShell& S : Shells)
	{
		for (FP& V : S.V) V = FP(V.X, V.Y * C + V.Z * Sn, -V.Y * Sn + V.Z * C);
	}
	BuildLegs(Shells);
	// stand on the origin: feet at y = 0
	for (FShell& S : Shells)
	{
		for (FP& V : S.V) V.Y += 0.0466f;
	}
	return MakeMesh(Outer, Name, Shells, Material);
}

namespace
{
UStaticMesh* MakeMesh(UObject* Outer, FName Name, const TArray<FShell>& Shells, UMaterialInterface* Material)
{
	TArray<FVector3f> Pos;
	TArray<FIntVector3> Tris;
	for (const FShell& S : Shells)
	{
		const int32 Base = Pos.Num();
		for (const FP& P : S.V) Pos.Add(ToUnreal(P));
		// every part is a closed shell: orient it so that (P2 - P0) x (P1 - P0), Unreal's front
		// face normal, points out (negative signed volume)
		double Vol = 0.0;
		for (const FIntVector3& T : S.T)
		{
			Vol += FVector3d::DotProduct(FVector3d(Pos[Base + T.X]), FVector3d::CrossProduct(FVector3d(Pos[Base + T.Y]), FVector3d(Pos[Base + T.Z])));
		}
		for (const FIntVector3& T : S.T)
		{
			Tris.Add(Vol > 0.0 ? FIntVector3(Base + T.X, Base + T.Z, Base + T.Y) : FIntVector3(Base + T.X, Base + T.Y, Base + T.Z));
		}
	}
	// smooth normals per shell vertex (Blender's shade smooth)
	TArray<FVector3f> Nrm;
	Nrm.SetNumZeroed(Pos.Num());
	for (const FIntVector3& T : Tris)
	{
		const FVector3f N = FVector3f::CrossProduct(Pos[T.Z] - Pos[T.X], Pos[T.Y] - Pos[T.X]);
		Nrm[T.X] += N; Nrm[T.Y] += N; Nrm[T.Z] += N;
	}

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
	Attr.GetPolygonGroupMaterialSlotNames()[PG] = TEXT("Starling");
	TArray<FVertexInstanceID> Inst;
	for (int32 I = 0; I < Pos.Num(); I++)
	{
		const FVertexID V = MD.CreateVertex();
		VPos[V] = Pos[I];
		const FVertexInstanceID VI = MD.CreateVertexInstance(V);
		const FVector3f N = Nrm[I].GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector);
		VNrm[VI] = N;
		VTan[VI] = (FVector3f::YAxisVector - N * N.Y).GetSafeNormal(UE_SMALL_NUMBER, FVector3f::XAxisVector);
		VSign[VI] = 1.f;
		VUV.Set(VI, 0, FVector2f(Pos[I].X / 38.f + 0.5f, Pos[I].Y / 22.f + 0.5f));
		Inst.Add(VI);
	}
	for (const FIntVector3& T : Tris)
	{
		MD.CreateTriangle(PG, { Inst[T.X], Inst[T.Y], Inst[T.Z] });
	}

	UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, Name, RF_Transient);
	Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("Starling")));
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bMarkPackageDirty = false;
	Params.bBuildSimpleCollision = false;
	Params.bCommitMeshDescription = false;
	Params.bFastBuild = true;
	Mesh->BuildFromMeshDescriptions({ &MD }, Params);
	// no collision: nothing to cook
	if (UBodySetup* Body = Mesh->GetBodySetup())
	{
		Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	}
	return Mesh;
}
}
