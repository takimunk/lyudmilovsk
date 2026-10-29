// The procedural architecture builder (LyudMeshKit.h).
#include "LyudMeshKit.h"

#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"

FVector2f FLyudMeshKit::PlanarUV(const FVector& P, const FVector& N, FVector& OutTangent)
{
	if (FMath::Abs(N.Z) > 0.7f)
	{
		OutTangent = FVector::XAxisVector;
		return FVector2f(P.X / 100.f, -P.Y / 100.f);   // V down -Y: the binormal sign is -1 on every face
	}
	// a wall: U along it, V down the height, so a pattern with an up (ribs, panels, tiles) stands upright
	OutTangent = FVector::CrossProduct(FVector::UpVector, N).GetSafeNormal();
	if (OutTangent.IsNearlyZero())
	{
		OutTangent = FVector::XAxisVector;
	}
	return FVector2f(FVector::DotProduct(P, OutTangent) / 100.f, -P.Z / 100.f);
}

void FLyudMeshKit::Quad(FName Mat, const FVector& A, const FVector& B, const FVector& C, const FVector& D)
{
	const FVector N = FVector::CrossProduct(B - A, D - A).GetSafeNormal();
	if (N.IsNearlyZero())
	{
		return;
	}
	FSection& S = Sec(Mat);
	const int32 Base = S.Pos.Num();
	for (const FVector& P : { A, B, C, D })
	{
		FVector T;
		S.UV.Add(PlanarUV(P, N, T));
		S.Pos.Add(FVector3f(P));
		S.Nrm.Add(FVector3f(N));
		S.Tan.Add(FVector3f(T));
	}
	S.Idx.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
}

void FLyudMeshKit::Tri(FName Mat, const FVector& A, const FVector& B, const FVector& C)
{
	const FVector N = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
	if (N.IsNearlyZero())
	{
		return;
	}
	FSection& S = Sec(Mat);
	const int32 Base = S.Pos.Num();
	for (const FVector& P : { A, B, C })
	{
		FVector T;
		S.UV.Add(PlanarUV(P, N, T));
		S.Pos.Add(FVector3f(P));
		S.Nrm.Add(FVector3f(N));
		S.Tan.Add(FVector3f(T));
	}
	S.Idx.Append({ Base, Base + 1, Base + 2 });
}

void FLyudMeshKit::Box(FName Mat, const FTransform& T, const FVector& H, bool bCollide, FName Top)
{
	if (H.X <= 0.01f || H.Y <= 0.01f || H.Z <= 0.01f)
	{
		return;
	}
	auto P = [&](float X, float Y, float Z) { return T.TransformPosition(FVector(X * H.X, Y * H.Y, Z * H.Z)); };
	const FName TopMat = Top.IsNone() ? Mat : Top;
	Quad(TopMat, P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1));
	Quad(Mat, P(-1, 1, -1), P(1, 1, -1), P(1, -1, -1), P(-1, -1, -1));
	Quad(Mat, P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), P(1, -1, 1));
	Quad(Mat, P(-1, 1, -1), P(-1, -1, -1), P(-1, -1, 1), P(-1, 1, 1));
	Quad(Mat, P(1, 1, -1), P(-1, 1, -1), P(-1, 1, 1), P(1, 1, 1));
	Quad(Mat, P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1));
	if (bCollide)
	{
		CollideBox(T, H);
	}
}

void FLyudMeshKit::BoxIn(FName Mat, const FTransform& F, const FVector& Centre, const FVector& Size, bool bCollide, FName Top, const FRotator& R)
{
	const FTransform Local(R, Centre);
	Box(Mat, Local * F, Size * 0.5f, bCollide, Top);
}

void FLyudMeshKit::Prism(FName Mat, const FTransform& F, const TArray<FVector2D>& Poly, float Z0, float Z1, bool bCollide, FName Top)
{
	const int32 N = Poly.Num();
	if (N < 3 || Z1 - Z0 < 0.01f)
	{
		return;
	}
	auto P = [&](int32 I, float Z) { return F.TransformPosition(FVector(Poly[(I + N) % N].X, Poly[(I + N) % N].Y, Z)); };
	const FName TopMat = Top.IsNone() ? Mat : Top;
	for (int32 I = 1; I + 1 < N; I++)
	{
		Tri(TopMat, P(0, Z1), P(I, Z1), P(I + 1, Z1));
		Tri(Mat, P(0, Z0), P(I + 1, Z0), P(I, Z0));
	}
	for (int32 I = 0; I < N; I++)
	{
		Quad(Mat, P(I, Z0), P(I + 1, Z0), P(I + 1, Z1), P(I, Z1));
	}
	if (bCollide)
	{
		TArray<FVector> Pts;
		for (int32 I = 0; I < N; I++)
		{
			Pts.Add(P(I, Z0));
			Pts.Add(P(I, Z1));
		}
		CollideConvex(Pts);
	}
}

void FLyudMeshKit::SlabWithHole(FName Mat, const FTransform& F, float X0, float X1, float Y0, float Y1, const TArray<FVector2D>& Hole,
	float Z0, float Z1, bool bCollide, FName Top)
{
	// strips across X cut at the hole's corners: in each strip the hole spans Y lo(x)..hi(x) (linear, it is convex), so
	// the slab there is two trapezoids, each a convex prism
	auto Lo = [&](float X, bool bHi, bool& bIn) -> float
	{
		float Best = bHi ? -1e9f : 1e9f;
		bIn = false;
		for (int32 I = 0; I < Hole.Num(); I++)
		{
			const FVector2D A = Hole[I], B = Hole[(I + 1) % Hole.Num()];
			if ((A.X - X) * (B.X - X) > 0.f || FMath::IsNearlyEqual(A.X, B.X, 0.01f))
			{
				if (FMath::IsNearlyEqual(A.X, X, 0.01f))
				{
					bIn = true;
					Best = bHi ? FMath::Max(Best, A.Y) : FMath::Min(Best, A.Y);
				}
				continue;
			}
			const float T = (X - A.X) / (B.X - A.X);
			const float Y = A.Y + (B.Y - A.Y) * T;
			bIn = true;
			Best = bHi ? FMath::Max(Best, Y) : FMath::Min(Best, Y);
		}
		return Best;
	};
	TArray<float> Cuts = { X0, X1 };
	for (const FVector2D& P : Hole)
	{
		if (P.X > X0 && P.X < X1)
		{
			Cuts.Add(P.X);
		}
	}
	Cuts.Sort();
	auto Piece = [&](const TArray<FVector2D>& Poly)
	{
		// drop degenerate corners
		TArray<FVector2D> Q;
		for (const FVector2D& P : Poly)
		{
			if (!Q.Num() || FVector2D::DistSquared(Q.Last(), P) > 0.25f)
			{
				Q.Add(P);
			}
		}
		if (Q.Num() > 2 && FVector2D::DistSquared(Q[0], Q.Last()) < 0.25f)
		{
			Q.Pop();
		}
		if (Q.Num() >= 3)
		{
			Prism(Mat, F, Q, Z0, Z1, bCollide, Top);
		}
	};
	for (int32 I = 0; I + 1 < Cuts.Num(); I++)
	{
		const float A = Cuts[I], B = Cuts[I + 1];
		if (B - A < 0.5f)
		{
			continue;
		}
		const float M = (A + B) * 0.5f;
		bool bInA, bInB, bInM, D0, D1;
		Lo(M, false, bInM);
		if (!bInM || Hole.Num() < 3)
		{
			Piece({ FVector2D(A, Y0), FVector2D(B, Y0), FVector2D(B, Y1), FVector2D(A, Y1) });
			continue;
		}
		// the hole's edges at the strip's two sides, nudged inside so a vertex on a cut reads its own strip
		const float LA = FMath::Clamp(Lo(A + 0.02f, false, bInA), Y0, Y1), HA = FMath::Clamp(Lo(A + 0.02f, true, D0), Y0, Y1);
		const float LB = FMath::Clamp(Lo(B - 0.02f, false, bInB), Y0, Y1), HB = FMath::Clamp(Lo(B - 0.02f, true, D1), Y0, Y1);
		Piece({ FVector2D(A, Y0), FVector2D(B, Y0), FVector2D(B, LB), FVector2D(A, LA) });
		Piece({ FVector2D(A, HA), FVector2D(B, HB), FVector2D(B, Y1), FVector2D(A, Y1) });
	}
}

void FLyudMeshKit::Rod(FName Mat, const FVector& A, const FVector& B, float R, int32 N, bool bCaps, bool bCollide)
{
	const FVector D = B - A;
	const float L = D.Size();
	if (L < 0.5f)
	{
		return;
	}
	const FVector Z = D / L;
	const FVector X = FVector::CrossProduct(FMath::Abs(Z.Z) < 0.9f ? FVector::UpVector : FVector::XAxisVector, Z).GetSafeNormal();
	const FVector Y = FVector::CrossProduct(Z, X);
	FSection& S = Sec(Mat);
	const int32 Base = S.Pos.Num();
	for (int32 I = 0; I <= N; I++)
	{
		const float Ang = 2.f * PI * I / N;
		const FVector Nr = X * FMath::Cos(Ang) + Y * FMath::Sin(Ang);
		for (int32 K = 0; K < 2; K++)
		{
			S.Pos.Add(FVector3f((K ? B : A) + Nr * R));
			S.Nrm.Add(FVector3f(Nr));
			S.Tan.Add(FVector3f(Z));
			S.UV.Add(FVector2f(K * L / 100.f, Ang * R / 100.f));
		}
	}
	for (int32 I = 0; I < N; I++)
	{
		const int32 V = Base + I * 2;
		S.Idx.Append({ V, V + 2, V + 3, V, V + 3, V + 1 });
	}
	if (bCaps)
	{
		for (int32 K = 0; K < 2; K++)
		{
			const FVector C = K ? B : A;
			const FVector Nc = K ? Z : -Z;
			const int32 Cb = S.Pos.Num();
			S.Pos.Add(FVector3f(C)); S.Nrm.Add(FVector3f(Nc)); S.Tan.Add(FVector3f(X)); S.UV.Add(FVector2f(0.f, 0.f));
			for (int32 I = 0; I <= N; I++)
			{
				const float Ang = 2.f * PI * I / N;
				const FVector P = C + (X * FMath::Cos(Ang) + Y * FMath::Sin(Ang)) * R;
				S.Pos.Add(FVector3f(P)); S.Nrm.Add(FVector3f(Nc)); S.Tan.Add(FVector3f(X));
				S.UV.Add(FVector2f(FMath::Cos(Ang) * R / 100.f, FMath::Sin(Ang) * R / 100.f));
			}
			for (int32 I = 0; I < N; I++)
			{
				if (K)
				{
					S.Idx.Append({ Cb, Cb + 1 + I, Cb + 2 + I });
				}
				else
				{
					S.Idx.Append({ Cb, Cb + 2 + I, Cb + 1 + I });
				}
			}
		}
	}
	if (bCollide)
	{
		const FQuat Q = FRotationMatrix::MakeFromZ(Z).ToQuat();
		CollideBox(FTransform(Q, (A + B) * 0.5f), FVector(R * 0.8f, R * 0.8f, L * 0.5f));
	}
}

void FLyudMeshKit::Lathe(const FTransform& F, const TArray<FVector2D>& Profile, const TArray<FName>& SegMats, int32 N, bool bCapTop, bool bCollide)
{
	// smooth round, flat along the profile's segments (a crisp edge where a band starts); V in metres along the profile
	float Run = 0.f;
	for (int32 K = 0; K + 1 < Profile.Num(); K++)
	{
		const FVector2D P0 = Profile[K], P1 = Profile[K + 1];
		const FName Mat = SegMats.IsValidIndex(K) ? SegMats[K] : (SegMats.Num() ? SegMats.Last() : FName(TEXT("enamel")));
		const FVector2D Dir = (P1 - P0).GetSafeNormal();
		const FVector2D Out(Dir.Y, -Dir.X);              // outward normal in (r, z)
		const float SegLen = FVector2D::Distance(P0, P1);
		FSection& S = Sec(Mat);
		const int32 Base = S.Pos.Num();
		const float Circ = 2.f * PI * FMath::Max(P0.X, P1.X);
		for (int32 I = 0; I <= N; I++)
		{
			const float Ang = 2.f * PI * I / N;
			const FVector Rad(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
			const FVector Nr = F.TransformVectorNoScale(Rad * Out.X + FVector(0, 0, Out.Y));
			const FVector Tg = F.TransformVectorNoScale(FVector(-Rad.Y, Rad.X, 0.f));
			for (int32 E = 0; E < 2; E++)
			{
				const FVector2D P = E ? P1 : P0;
				S.Pos.Add(FVector3f(F.TransformPosition(Rad * P.X + FVector(0, 0, P.Y))));
				S.Nrm.Add(FVector3f(Nr));
				S.Tan.Add(FVector3f(Tg));
				S.UV.Add(FVector2f(Circ * I / N / 100.f, -(Run + (E ? SegLen : 0.f)) / 100.f));
			}
		}
		for (int32 I = 0; I < N; I++)
		{
			const int32 V = Base + I * 2;
			S.Idx.Append({ V, V + 2, V + 3, V, V + 3, V + 1 });
		}
		Run += SegLen;
		if (bCollide && SegLen > 1.f)
		{
			const float R = FMath::Min(P0.X, P1.X) * 0.92f;
			CollideBox(FTransform(F.GetRotation(), F.TransformPosition(FVector(0, 0, (P0.Y + P1.Y) * 0.5f))), FVector(R * 0.7f, R * 0.7f, FMath::Abs(P1.Y - P0.Y) * 0.5f));
		}
	}
	if (bCapTop && Profile.Num())
	{
		const FVector2D P = Profile.Last();
		const FName Mat = SegMats.Num() ? SegMats.Last() : FName(TEXT("enamel"));
		FSection& S = Sec(Mat);
		const int32 Cb = S.Pos.Num();
		const FVector Up = F.TransformVectorNoScale(FVector::UpVector);
		S.Pos.Add(FVector3f(F.TransformPosition(FVector(0, 0, P.Y)))); S.Nrm.Add(FVector3f(Up)); S.Tan.Add(FVector3f(F.TransformVectorNoScale(FVector::XAxisVector))); S.UV.Add(FVector2f(0, 0));
		for (int32 I = 0; I <= N; I++)
		{
			const float Ang = 2.f * PI * I / N;
			S.Pos.Add(FVector3f(F.TransformPosition(FVector(FMath::Cos(Ang) * P.X, FMath::Sin(Ang) * P.X, P.Y))));
			S.Nrm.Add(FVector3f(Up)); S.Tan.Add(FVector3f(F.TransformVectorNoScale(FVector::XAxisVector)));
			S.UV.Add(FVector2f(FMath::Cos(Ang) * P.X / 100.f, FMath::Sin(Ang) * P.X / 100.f));
		}
		for (int32 I = 0; I < N; I++)
		{
			S.Idx.Append({ Cb, Cb + 1 + I, Cb + 2 + I });
		}
	}
}

void FLyudMeshKit::Wall(FName Mat, const FTransform& F, float Len, float T, float Z0, float Z1, const TArray<FOpening>& Openings, bool bCollide)
{
	// split the wall into vertical bands at the openings' sides; each band is solid but for the openings in it
	TArray<float> Us = { 0.f, Len };
	for (const FOpening& O : Openings)
	{
		Us.Add(FMath::Clamp(O.U0, 0.f, Len));
		Us.Add(FMath::Clamp(O.U1, 0.f, Len));
	}
	Us.Sort();
	for (int32 I = 0; I + 1 < Us.Num(); I++)
	{
		const float A = Us[I], B = Us[I + 1];
		if (B - A < 0.5f)
		{
			continue;
		}
		const float M = (A + B) * 0.5f;
		TArray<FVector2D> Gaps;     // z ranges cut out in this band
		for (const FOpening& O : Openings)
		{
			if (M > O.U0 && M < O.U1)
			{
				Gaps.Add(FVector2D(FMath::Max(O.Z0, Z0), FMath::Min(O.Z1, Z1)));
			}
		}
		Gaps.Sort([](const FVector2D& L, const FVector2D& R) { return L.X < R.X; });
		float Z = Z0;
		auto Solid = [&](float Za, float Zb)
		{
			if (Zb - Za > 0.5f)
			{
				BoxIn(Mat, F, FVector(M, 0.f, (Za + Zb) * 0.5f), FVector(B - A, T, Zb - Za), bCollide);
			}
		};
		for (const FVector2D& G : Gaps)
		{
			Solid(Z, G.X);
			Z = FMath::Max(Z, G.Y);
		}
		Solid(Z, Z1);
	}
}

void FLyudMeshKit::CollideBox(const FTransform& T, const FVector& H)
{
	FKBoxElem E(H.X * 2.f, H.Y * 2.f, H.Z * 2.f);
	E.Center = T.GetLocation();
	E.Rotation = T.Rotator();
	Geom.BoxElems.Add(E);
}

void FLyudMeshKit::CollideConvex(const TArray<FVector>& Pts)
{
	// an upright prism of a convex polygon is a box when it has four corners at right angles; otherwise a convex hull
	FKConvexElem E;
	E.VertexData = Pts;
	E.UpdateElemBox();
	Geom.ConvexElems.Add(E);
}

int32 FLyudMeshKit::NumTriangles() const
{
	int32 N = 0;
	for (const auto& P : Sections)
	{
		N += P.Value.Idx.Num() / 3;
	}
	return N;
}

UStaticMesh* FLyudMeshKit::Build(UObject* Outer, FName Name, TArray<FName>& OutSlots) const
{
	FMeshDescription MD;
	FStaticMeshAttributes Attr(MD);
	Attr.Register();
	TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> VTan = Attr.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> VSign = Attr.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector2f> VUV = Attr.GetVertexInstanceUVs();
	VUV.SetNumChannels(1);
	UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, MakeUniqueObjectName(Outer, UStaticMesh::StaticClass(), Name), RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
	OutSlots.Reset();
	for (const auto& P : Sections)
	{
		const FSection& S = P.Value;
		if (!S.Idx.Num())
		{
			continue;
		}
		const FPolygonGroupID PG = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[PG] = P.Key;
		Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, P.Key));
		OutSlots.Add(P.Key);
		TArray<FVertexInstanceID> Inst;
		Inst.Reserve(S.Pos.Num());
		for (int32 I = 0; I < S.Pos.Num(); I++)
		{
			const FVertexID V = MD.CreateVertex();
			VPos[V] = S.Pos[I];
			const FVertexInstanceID VI = MD.CreateVertexInstance(V);
			VNrm[VI] = S.Nrm[I];
			VTan[VI] = S.Tan[I];
			VSign[VI] = -1.f;
			VUV.Set(VI, 0, S.UV[I]);
			Inst.Add(VI);
		}
		for (int32 I = 0; I + 2 < S.Idx.Num(); I += 3)
		{
			// the kit lists corners counterclockwise seen from the front (right-handed); Unreal's front faces wind
			// the other way round in its left-handed frame
			MD.CreateTriangle(PG, { Inst[S.Idx[I]], Inst[S.Idx[I + 2]], Inst[S.Idx[I + 1]] });
		}
	}
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bMarkPackageDirty = false;
	Params.bBuildSimpleCollision = false;
	Params.bCommitMeshDescription = false;
	Params.bFastBuild = true;
	Mesh->BuildFromMeshDescriptions({ &MD }, Params);
	if (Geom.GetElementCount() > 0)
	{
		// a body of its own (as the sea's water volume does): boxes and convex pieces, never the triangles
		UBodySetup* Body = NewObject<UBodySetup>(Mesh, NAME_None, RF_Transient);
		Body->BodySetupGuid = FGuid::NewGuid();
		Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		Body->bGenerateMirroredCollision = false;
		Body->AggGeom = Geom;
		Body->CreatePhysicsMeshes();
		Mesh->SetBodySetup(Body);
	}
	return Mesh;
}
