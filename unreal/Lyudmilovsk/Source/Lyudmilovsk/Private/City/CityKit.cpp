#include "CityKit.h"

#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

static TAutoConsoleVariable<FString> CVarCityExportKit(TEXT("lyud.City.ExportKit"), TEXT(""),
	TEXT("A folder: every public-spaces mesh built from now on is also written there as <name>.obj (metres, Blender's axes, vertex "
		 "colours; faces grouped by the material's pattern: usemtl pat<kind>_<tile>, glow) for looking at them outside the game."));

namespace CityKit
{
	/** The mesh as Wavefront OBJ with vertex colours (linear RGB) in Blender's frame (x, -y, z; metres). */
	static void WriteObj(const FMesh& M, const FString& Path)
	{
		TStringBuilder<1 << 16> S;
		S << TEXT("# Lyudmilovsk public spaces kit mesh (CityKit), metres, Blender axes\n");
		for (int32 I = 0; I < M.Pos.Num(); I++)
		{
			const FVector3f P = M.Pos[I] / 100.f;
			S.Appendf(TEXT("v %.4f %.4f %.4f %.4f %.4f %.4f\n"), P.X, -P.Y, P.Z, M.Col[I].X, M.Col[I].Y, M.Col[I].Z);
		}
		for (int32 I = 0; I < M.Pos.Num(); I++) S.Appendf(TEXT("vt %.4f %.4f\n"), M.UV0[I].X, M.UV0[I].Y);
		for (int32 I = 0; I < M.Pos.Num(); I++) S.Appendf(TEXT("vn %.4f %.4f %.4f\n"), M.Nrm[I].X, -M.Nrm[I].Y, M.Nrm[I].Z);
		FString Cur;
		for (int32 I = 0; I + 2 < M.Idx.Num(); I += 3)
		{
			const int32 A = M.Idx[I];
			const FString Mat = M.UV2[A].X > 0.5f ? FString(TEXT("glow")) : FString::Printf(TEXT("pat%d_%.2f_r%.2f"), FMath::RoundToInt(M.UV1[A].X), M.UV1[A].Y, M.Col[A].W);
			if (Mat != Cur)
			{
				S << TEXT("usemtl ") << *Mat << TEXT("\n");
				Cur = Mat;
			}
			// y mirrored: the winding flips with it
			const int32 B = M.Idx[I + 2] + 1, C = M.Idx[I + 1] + 1, Aa = A + 1;
			S.Appendf(TEXT("f %d/%d/%d %d/%d/%d %d/%d/%d\n"), Aa, Aa, Aa, B, B, B, C, C, C);
		}
		FFileHelper::SaveStringToFile(FStringView(S.ToString(), S.Len()), *Path);
	}
}


namespace CityKit
{
	FLinearColor Srgb(uint8 R, uint8 G, uint8 B) { return FLinearColor::FromSRGBColor(FColor(R, G, B)); }

	FSkin Make(const FLinearColor& C, float Rough, EPattern P = EPattern::None, float Tile = 0.f, float Glow = 0.f)
	{
		FSkin S;
		S.Color = C;
		S.Rough = Rough;
		S.Pattern = P;
		S.Tile = Tile;
		S.Glow = Glow;
		return S;
	}
	FSkin Iron() { return Make(Srgb(38, 44, 40), 0.45f); }
	FSkin Timber() { return Make(Srgb(122, 84, 52), 0.7f); }
	FSkin Granite() { return Make(Srgb(128, 120, 116), 0.72f); }
	FSkin Concrete() { return Make(Srgb(168, 164, 156), 0.85f); }
	FSkin Paint(uint8 R, uint8 G, uint8 B, float Rough) { return Make(Srgb(R, G, B), Rough); }
	FSkin Glass() { return Make(Srgb(255, 236, 200), 0.15f, EPattern::None, 0.f, 1.f); }

	int32 FMesh::Vert(const FVector& P, const FVector& N, const FSkin& S, const FVector2f& UV)
	{
		Pos.Add(FVector3f(P * 100.0));
		Nrm.Add(FVector3f(N.GetSafeNormal()));
		Col.Add(FVector4f(S.Color.R, S.Color.G, S.Color.B, S.Rough));
		UV0.Add(UV);
		UV1.Add(FVector2f((float)S.Pattern, S.Tile));
		UV2.Add(FVector2f(S.Glow, 0.f));
		return Pos.Num() - 1;
	}

	void FMesh::Tri(int32 A, int32 B, int32 C, const FVector& Out)
	{
		const FVector3f N = FVector3f::CrossProduct(Pos[B] - Pos[A], Pos[C] - Pos[A]);
		if (FVector3f::DotProduct(N, FVector3f(Out)) < 0.f)
		{
			Swap(B, C);
		}
		Idx.Append({ A, B, C });
	}

	void FMesh::Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FSkin& S)
	{
		FVector N = FVector::CrossProduct(B - A, C - A);
		if (N.SizeSquared() < 1e-12)
		{
			N = FVector::CrossProduct(C - A, D - A);
		}
		N.Normalize();
		// UV0 in metres in the quad's plane
		const FVector U = (B - A).GetSafeNormal(), V = FVector::CrossProduct(N, U);
		auto Uv = [&](const FVector& P) { return FVector2f(FVector::DotProduct(P - A, U), FVector::DotProduct(P - A, V)); };
		const int32 I0 = Vert(A, N, S, Uv(A)), I1 = Vert(B, N, S, Uv(B)), I2 = Vert(C, N, S, Uv(C)), I3 = Vert(D, N, S, Uv(D));
		Tri(I0, I1, I2, N);
		Tri(I0, I2, I3, N);
	}

	void FMesh::QuadUV(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector2f& Ua, const FVector2f& Ub,
		const FVector2f& Uc, const FVector2f& Ud, const FSkin& S)
	{
		FVector N = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
		const int32 I0 = Vert(A, N, S, Ua), I1 = Vert(B, N, S, Ub), I2 = Vert(C, N, S, Uc), I3 = Vert(D, N, S, Ud);
		Tri(I0, I1, I2, N);
		Tri(I0, I2, I3, N);
	}

	void FMesh::Box(const FVector& C, const FVector& H, const FSkin& S, const FRotator& Rot, bool bBottom)
	{
		auto P = [&](float X, float Y, float Z) { return C + Rot.RotateVector(FVector(X * H.X, Y * H.Y, Z * H.Z)); };
		Quad(P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1), S);       // top
		Quad(P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), P(1, -1, 1), S);       // +x
		Quad(P(-1, 1, -1), P(-1, -1, -1), P(-1, -1, 1), P(-1, 1, 1), S);   // -x
		Quad(P(1, 1, -1), P(-1, 1, -1), P(-1, 1, 1), P(1, 1, 1), S);       // +y
		Quad(P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1), S);   // -y
		if (bBottom)
		{
			Quad(P(-1, 1, -1), P(1, 1, -1), P(1, -1, -1), P(-1, -1, -1), S);
		}
	}

	void FMesh::Bar(const FVector& A, const FVector& B, float Size, float Size2, const FSkin& S, const FVector& Up)
	{
		const FVector D = B - A;
		const float L = D.Size();
		if (L < 1e-4f) return;
		const FVector X = D / L;
		FVector Z = Up - X * FVector::DotProduct(Up, X);
		if (Z.SizeSquared() < 1e-6) Z = FVector::CrossProduct(X, FVector::RightVector);
		Z.Normalize();
		const FVector Y = FVector::CrossProduct(Z, X);
		const FMatrix Mx(X, Y, Z, FVector::ZeroVector);
		Box((A + B) * 0.5, FVector(L * 0.5, Size * 0.5, Size2 * 0.5), S, Mx.Rotator(), true);
	}

	void FMesh::Tube(const FVector& A, const FVector& B, float R0, float R1, int32 Sides, const FSkin& S, bool bCapA, bool bCapB)
	{
		const FVector Ax = (B - A).GetSafeNormal();
		const FVector U = FVector::CrossProduct(Ax, FMath::Abs(Ax.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
		const FVector V = FVector::CrossProduct(Ax, U);
		const int32 Base = Pos.Num();
		const float Slope = (R0 - R1) / FMath::Max(1e-3f, (float)(B - A).Size());
		for (int32 I = 0; I <= Sides; I++)
		{
			const float T = 2.f * PI * I / Sides;
			const FVector Rd = U * FMath::Cos(T) + V * FMath::Sin(T);
			const FVector N = (Rd + Ax * Slope).GetSafeNormal();
			Vert(A + Rd * R0, N, S, FVector2f(T, 0.f));
			Vert(B + Rd * R1, N, S, FVector2f(T, 1.f));
		}
		for (int32 I = 0; I < Sides; I++)
		{
			const int32 A0 = Base + I * 2, A1 = A0 + 1, B0 = A0 + 2, B1 = A0 + 3;
			const FVector Out = FVector(Nrm[A0]);
			Tri(A0, B0, B1, Out);
			Tri(A0, B1, A1, Out);
		}
		auto Cap = [&](const FVector& C, float R, const FVector& N)
		{
			if (R <= 0.f) return;
			const int32 Ci = Vert(C, N, S);
			const int32 B2 = Pos.Num();
			for (int32 I = 0; I <= Sides; I++)
			{
				const float T = 2.f * PI * I / Sides;
				Vert(C + (U * FMath::Cos(T) + V * FMath::Sin(T)) * R, N, S);
			}
			for (int32 I = 0; I < Sides; I++) Tri(Ci, B2 + I, B2 + I + 1, N);
		};
		if (bCapA) Cap(A, R0, -Ax);
		if (bCapB) Cap(B, R1, Ax);
	}

	void FMesh::Lathe(const FVector& C, const TArray<FVector2D>& Pr, int32 Sides, TFunctionRef<FSkin(int32, int32)> Skin, float A0, float A1)
	{
		// each span its own vertices (hard edges between spans: the profiles are mostly mouldings)
		for (int32 K = 0; K + 1 < Pr.Num(); K++)
		{
			const FVector2D P0 = Pr[K], P1 = Pr[K + 1];
			if (P0.Equals(P1)) continue;
			const FVector2D T = (P1 - P0).GetSafeNormal();
			const FVector2D N2(T.Y, -T.X);           // outward for a profile going up with r on the left... (r, z): normal (dz, -dr)
			for (int32 I = 0; I < Sides; I++)
			{
				const float Ta = FMath::Lerp(A0, A1, float(I) / Sides), Tb = FMath::Lerp(A0, A1, float(I + 1) / Sides);
				const FSkin S = Skin(K, I);
				const FVector Ra(FMath::Cos(Ta), FMath::Sin(Ta), 0.f), Rb(FMath::Cos(Tb), FMath::Sin(Tb), 0.f);
				const FVector Na = Ra * N2.X + FVector::UpVector * N2.Y, Nb = Rb * N2.X + FVector::UpVector * N2.Y;
				const int32 I0 = Vert(C + Ra * P0.X + FVector(0, 0, P0.Y), Na, S, FVector2f(Ta * P0.X, P0.Y));
				const int32 I1 = Vert(C + Rb * P0.X + FVector(0, 0, P0.Y), Nb, S, FVector2f(Tb * P0.X, P0.Y));
				const int32 I2 = Vert(C + Rb * P1.X + FVector(0, 0, P1.Y), Nb, S, FVector2f(Tb * P1.X, P1.Y));
				const int32 I3 = Vert(C + Ra * P1.X + FVector(0, 0, P1.Y), Na, S, FVector2f(Ta * P1.X, P1.Y));
				const FVector Out = (Na + Nb).GetSafeNormal();
				if (P0.X > 1e-4f || P1.X > 1e-4f)
				{
					Tri(I0, I1, I2, Out);
					Tri(I0, I2, I3, Out);
				}
			}
		}
	}

	void FMesh::Sphere(const FVector& C, float R, int32 Sides, const FSkin& S)
	{
		TArray<FVector2D> Pr;
		const int32 N = FMath::Max(4, Sides / 2);
		for (int32 I = 0; I <= N; I++)
		{
			const float A = -PI / 2 + PI * I / N;
			Pr.Add(FVector2D(R * FMath::Cos(A), R * FMath::Sin(A)));
		}
		Lathe(C, Pr, Sides, [&](int32, int32) { return S; });
	}

	void FMesh::Disc(const FVector& C, float R0, float R1, int32 Sides, const FSkin& S, float Ry)
	{
		const float K = Ry > 0.f ? Ry / R1 : 1.f;
		for (int32 I = 0; I < Sides; I++)
		{
			const float Ta = 2.f * PI * I / Sides, Tb = 2.f * PI * (I + 1) / Sides;
			const FVector Ra(FMath::Cos(Ta), FMath::Sin(Ta) * K, 0.f), Rb(FMath::Cos(Tb), FMath::Sin(Tb) * K, 0.f);
			if (R0 <= 0.f)
			{
				const int32 Ci = Vert(C, FVector::UpVector, S, FVector2f(0, 0));
				const int32 Ia = Vert(C + Ra * R1, FVector::UpVector, S, FVector2f(Ra.X * R1, Ra.Y * R1));
				const int32 Ib = Vert(C + Rb * R1, FVector::UpVector, S, FVector2f(Rb.X * R1, Rb.Y * R1));
				Tri(Ci, Ia, Ib, FVector::UpVector);
			}
			else
			{
				Quad(C + Ra * R0, C + Ra * R1, C + Rb * R1, C + Rb * R0, S);
			}
		}
	}

	UStaticMesh* FMesh::Build(UObject* Outer, FName Name, bool bCollision) const
	{
		const FString Dir = CVarCityExportKit.GetValueOnGameThread();
		if (!Dir.IsEmpty())
		{
			IFileManager::Get().MakeDirectory(*Dir, true);
			WriteObj(*this, Dir / Name.ToString() + TEXT(".obj"));
		}
		FMeshDescription MD;
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> VTan = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> VSign = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> VUV = Attr.GetVertexInstanceUVs();
		TVertexInstanceAttributesRef<FVector4f> VCol = Attr.GetVertexInstanceColors();
		VUV.SetNumChannels(3);
		const FPolygonGroupID PG = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[PG] = Name;
		MD.ReserveNewVertices(Pos.Num());
		MD.ReserveNewVertexInstances(Pos.Num());
		MD.ReserveNewTriangles(Idx.Num() / 3);
		TArray<FVertexInstanceID> Inst;
		Inst.Reserve(Pos.Num());
		for (int32 I = 0; I < Pos.Num(); I++)
		{
			const FVertexID V = MD.CreateVertex();
			VPos[V] = Pos[I];
			const FVertexInstanceID VI = MD.CreateVertexInstance(V);
			const FVector3f N = Nrm[I];
			VNrm[VI] = N;
			VTan[VI] = FVector3f::CrossProduct(FMath::Abs(N.Z) < 0.9f ? FVector3f::UpVector : FVector3f::XAxisVector, N).GetSafeNormal();
			VSign[VI] = -1.f;
			VUV.Set(VI, 0, UV0[I]);
			VUV.Set(VI, 1, UV1[I]);
			VUV.Set(VI, 2, UV2[I]);
			VCol[VI] = Col[I];
			Inst.Add(VI);
		}
		for (int32 I = 0; I + 2 < Idx.Num(); I += 3)
		{
			// Tri() wound them counterclockwise round the outward normal (right-handed); Unreal's front faces go the other way
			MD.CreateTriangle(PG, { Inst[Idx[I]], Inst[Idx[I + 2]], Inst[Idx[I + 1]] });
		}
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, MakeUniqueObjectName(Outer, UStaticMesh::StaticClass(), Name),
			RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, Name));
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = bCollision;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Mesh->BuildFromMeshDescriptions({ &MD }, Params);
		if (bCollision && Colliders.Num() > 0)
		{
			if (UBodySetup* Body = Mesh->GetBodySetup())
			{
				Body->AggGeom.BoxElems.Reset();
				for (const FCollider& Cl : Colliders)
				{
					FKBoxElem B(Cl.Half.X * 200.f, Cl.Half.Y * 200.f, Cl.Half.Z * 200.f);
					B.Center = Cl.C * 100.0;
					B.Rotation = FRotator(0.f, Cl.Yaw, 0.f);
					Body->AggGeom.BoxElems.Add(B);
				}
				Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
				Body->InvalidatePhysicsData();
				Body->CreatePhysicsMeshes();
			}
		}
		else if (UBodySetup* Body = Mesh->GetBodySetup())
		{
			Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		}
		return Mesh;
	}

	// --- the kit ---------------------------------------------------------------------------------------------

	namespace
	{
		FSkin Variant4(int32 V, uint8 Alpha = 0)
		{
			static const uint8 C[4][3] = { { 196, 48, 40 }, { 40, 92, 160 }, { 46, 128, 82 }, { 214, 150, 34 } };
			const int32 K = ((V % 4) + 4) % 4;
			return Paint(C[K][0], C[K][1], C[K][2], 0.6f);
		}
		FSkin White() { return Paint(232, 228, 216, 0.6f); }
		FSkin Green() { return Paint(44, 84, 56, 0.5f); }

		/** The Soviet park bench: two cast-iron side frames, three seat slats and three back slats painted green. */
		void BenchPark(FMesh& M)
		{
			const FSkin Fe = Iron(), Wood = Paint(56, 104, 64, 0.55f);
			for (float Y : { -0.8f, 0.8f })
			{
				M.Bar(FVector(0.2f, Y, 0.f), FVector(0.16f, Y, 0.44f), 0.05f, 0.04f, Fe);              // front leg
				M.Bar(FVector(-0.22f, Y, 0.f), FVector(-0.2f, Y, 0.42f), 0.05f, 0.04f, Fe);            // back leg
				M.Bar(FVector(-0.2f, Y, 0.42f), FVector(-0.32f, Y, 0.86f), 0.05f, 0.04f, Fe);          // back stay
				M.Bar(FVector(-0.24f, Y, 0.42f), FVector(0.2f, Y, 0.42f), 0.04f, 0.05f, Fe);           // seat bearer
				M.Bar(FVector(0.16f, Y, 0.44f), FVector(0.18f, Y, 0.66f), 0.04f, 0.04f, Fe);           // armrest post
				M.Bar(FVector(-0.24f, Y, 0.66f), FVector(0.22f, Y, 0.66f), 0.05f, 0.04f, Fe);          // armrest
			}
			for (int32 I = 0; I < 3; I++)
			{
				const float X = 0.13f - I * 0.14f;
				M.Box(FVector(X, 0.f, 0.455f), FVector(0.055f, 0.92f, 0.018f), Wood, FRotator::ZeroRotator, true);
			}
			for (int32 I = 0; I < 3; I++)
			{
				const float T = 0.25f + I * 0.3f;
				const FVector P = FMath::Lerp(FVector(-0.215f, 0.f, 0.47f), FVector(-0.33f, 0.f, 0.88f), T);
				M.Box(P, FVector(0.018f, 0.92f, 0.055f), Wood, FRotator(-15.f, 0.f, 0.f), true);
			}
			M.Collide(FVector(-0.05f, 0.f, 0.3f), FVector(0.27f, 0.92f, 0.3f));
		}

		/** The promenade bench: two concrete pedestals, a timber seat and back on iron brackets. */
		void BenchPromenade(FMesh& M)
		{
			const FSkin Con = Concrete(), Wood = Timber(), Fe = Iron();
			for (float Y : { -0.72f, 0.72f })
			{
				M.Box(FVector(-0.02f, Y, 0.2f), FVector(0.24f, 0.09f, 0.2f), Con);
				M.Bar(FVector(-0.22f, Y, 0.4f), FVector(-0.34f, Y, 0.85f), 0.05f, 0.03f, Fe);
			}
			for (int32 I = 0; I < 5; I++)
			{
				M.Box(FVector(0.18f - I * 0.095f, 0.f, 0.425f), FVector(0.04f, 0.95f, 0.022f), Wood, FRotator::ZeroRotator, true);
			}
			for (int32 I = 0; I < 3; I++)
			{
				const FVector P = FMath::Lerp(FVector(-0.24f, 0.f, 0.5f), FVector(-0.335f, 0.f, 0.86f), 0.2f + I * 0.33f);
				M.Box(P, FVector(0.02f, 0.95f, 0.05f), Wood, FRotator(-14.f, 0.f, 0.f), true);
			}
			M.Collide(FVector(-0.04f, 0.f, 0.3f), FVector(0.27f, 0.95f, 0.3f));
		}

		/** The tall lamp: a fluted cast pole on a base, an opal globe on top. */
		void Lamp(FMesh& M)
		{
			const FSkin Fe = Iron();
			TArray<FVector2D> Base = { { 0.24, 0.0 }, { 0.24, 0.12 }, { 0.2, 0.16 }, { 0.17, 0.45 }, { 0.12, 0.55 }, { 0.085, 0.62 }, { 0.0, 0.62 } };
			M.Lathe(FVector::ZeroVector, Base, 12, [&](int32, int32) { return Fe; });
			M.Tube(FVector(0, 0, 0.6), FVector(0, 0, 4.3), 0.075f, 0.05f, 10, Fe, false, false);
			TArray<FVector2D> Top = { { 0.05, 4.3 }, { 0.09, 4.36 }, { 0.12, 4.42 }, { 0.11, 4.47 }, { 0.0, 4.47 } };
			M.Lathe(FVector::ZeroVector, Top, 12, [&](int32, int32) { return Fe; });
			M.Sphere(FVector(0, 0, 4.7f), 0.24f, 16, Glass());
			M.Tube(FVector(0, 0, 4.93f), FVector(0, 0, 5.0f), 0.05f, 0.0f, 8, Fe, false, false);
			M.Collide(FVector(0, 0, 2.5f), FVector(0.2f, 0.2f, 2.5f));
		}

		/** The park lamp: a slim pole with a four-sided lantern. */
		void LampPark(FMesh& M)
		{
			const FSkin Fe = Iron();
			M.Tube(FVector(0, 0, 0), FVector(0, 0, 0.3f), 0.1f, 0.08f, 8, Fe, false, true);
			M.Tube(FVector(0, 0, 0.3f), FVector(0, 0, 3.2f), 0.05f, 0.04f, 8, Fe, false, false);
			M.Box(FVector(0, 0, 3.22f), FVector(0.15f, 0.15f, 0.025f), Fe, FRotator::ZeroRotator, true);
			M.Box(FVector(0, 0, 3.47f), FVector(0.13f, 0.13f, 0.23f), Glass());
			for (float X : { -0.13f, 0.13f }) for (float Y : { -0.13f, 0.13f })
				M.Box(FVector(X, Y, 3.47f), FVector(0.015f, 0.015f, 0.24f), Fe);
			TArray<FVector2D> Cap = { { 0.21, 3.7 }, { 0.0, 3.86 } };
			M.Lathe(FVector::ZeroVector, Cap, 4, [&](int32, int32) { return Fe; }, PI / 4, PI / 4 + 2 * PI);
			M.Collide(FVector(0, 0, 1.8f), FVector(0.12f, 0.12f, 1.8f));
		}

		/** A 2 m run of cast-iron railing along X: a post at -1 m, two rails, balusters, a timber handrail. */
		void Railing(FMesh& M)
		{
			const FSkin Fe = Iron(), Wood = Timber();
			M.Box(FVector(-1.f, 0, 0.55f), FVector(0.035f, 0.035f, 0.55f), Fe);
			M.Box(FVector(-1.f, 0, 1.12f), FVector(0.05f, 0.05f, 0.03f), Fe);
			M.Box(FVector(0, 0, 1.06f), FVector(1.0f, 0.045f, 0.03f), Wood, FRotator::ZeroRotator, true);
			M.Box(FVector(0, 0, 0.98f), FVector(1.0f, 0.015f, 0.015f), Fe, FRotator::ZeroRotator, true);
			M.Box(FVector(0, 0, 0.12f), FVector(1.0f, 0.015f, 0.015f), Fe, FRotator::ZeroRotator, true);
			for (int32 I = 0; I < 15; I++)
			{
				const float X = -0.875f + I * 0.125f;
				M.Box(FVector(X, 0, 0.55f), FVector(0.008f, 0.008f, 0.43f), Fe);
			}
			M.Collide(FVector(0, 0, 0.6f), FVector(1.0f, 0.06f, 0.6f));
		}

		void RailingPost(FMesh& M)
		{
			const FSkin Fe = Iron();
			M.Box(FVector(0, 0, 0.5f), FVector(0.025f, 0.025f, 0.5f), Fe);
			M.Box(FVector(0, 0, 0.98f), FVector(0.8f, 0.03f, 0.025f), Fe, FRotator::ZeroRotator, true);
			M.Collide(FVector(0, 0, 0.5f), FVector(0.8f, 0.05f, 0.5f));
		}

		void TreeGrate(FMesh& M)
		{
			const FSkin Fe = Paint(30, 32, 30, 0.6f);
			for (int32 I = 0; I < 4; I++)
			{
				const FRotator R(0.f, 90.f * I, 0.f);
				M.Box(R.RotateVector(FVector(0.62f, 0, 0.012f)), FVector(0.18f, 0.8f, 0.012f), Fe, R);
			}
			for (int32 I = 0; I < 8; I++)
			{
				const float Y = -0.7f + I * 0.2f;
				M.Box(FVector(0, Y, 0.01f), FVector(0.44f, 0.02f, 0.01f), Fe);
			}
			M.Disc(FVector(0, 0, 0.004f), 0.f, 0.46f, 16, Paint(58, 44, 32, 0.95f));
		}

		/** A Soviet concrete urn. */
		void Bin(FMesh& M)
		{
			TArray<FVector2D> P = { { 0.17, 0.0 }, { 0.17, 0.06 }, { 0.1, 0.12 }, { 0.09, 0.3 }, { 0.2, 0.42 }, { 0.26, 0.6 }, { 0.27, 0.72 },
				{ 0.22, 0.72 }, { 0.2, 0.55 } };
			M.Lathe(FVector::ZeroVector, P, 14, [](int32 K, int32) { return K >= 6 ? Paint(60, 56, 50, 0.9f) : Concrete(); });
			M.Disc(FVector(0, 0, 0.56f), 0.f, 0.205f, 14, Paint(40, 36, 32, 0.95f));
			M.Collide(FVector(0, 0, 0.36f), FVector(0.2f, 0.2f, 0.36f));
		}

		void Umbrella(FMesh& M, int32 V)
		{
			M.Tube(FVector(0, 0, -0.3f), FVector(0, 0, 2.3f), 0.022f, 0.02f, 6, White(), false, true);
			const FSkin A = Variant4(V), B = White();
			TArray<FVector2D> C = { { 1.25, 1.98 }, { 1.2, 2.04 }, { 0.6, 2.28 }, { 0.0, 2.4 } };
			M.Lathe(FVector::ZeroVector, C, 16, [&](int32, int32 S) { return S % 2 ? A : B; });
			// the underside, the canopy seen from beneath
			TArray<FVector2D> U = { { 0.0, 2.38 }, { 0.6, 2.26 }, { 1.2, 2.02 } };
			M.Lathe(FVector::ZeroVector, U, 16, [&](int32, int32 S) { FSkin K = S % 2 ? A : B; K.Color *= 0.7f; return K; });
		}

		void Lounger(FMesh& M)
		{
			const FSkin Wood = Paint(196, 176, 140, 0.7f), Fe = Paint(200, 200, 196, 0.4f);
			for (float Y : { -0.3f, 0.3f })
			{
				M.Bar(FVector(-0.95f, Y, 0.3f), FVector(0.45f, Y, 0.3f), 0.03f, 0.04f, Fe);
				M.Bar(FVector(-0.85f, Y, 0.0f), FVector(-0.85f, Y, 0.3f), 0.03f, 0.03f, Fe);
				M.Bar(FVector(0.35f, Y, 0.0f), FVector(0.35f, Y, 0.3f), 0.03f, 0.03f, Fe);
				M.Bar(FVector(0.45f, Y, 0.32f), FVector(0.9f, Y, 0.75f), 0.03f, 0.03f, Fe);
			}
			for (int32 I = 0; I < 12; I++)
			{
				M.Box(FVector(-0.92f + I * 0.115f, 0, 0.33f), FVector(0.045f, 0.33f, 0.012f), Wood, FRotator::ZeroRotator, true);
			}
			for (int32 I = 0; I < 5; I++)
			{
				const FVector P = FMath::Lerp(FVector(0.48f, 0, 0.36f), FVector(0.88f, 0, 0.75f), 0.1f + I * 0.2f);
				M.Box(P, FVector(0.045f, 0.33f, 0.012f), Wood, FRotator(44.f, 0.f, 0.f), true);
			}
		}

		void ChangingBooth(FMesh& M)
		{
			const FSkin Fe = Paint(230, 230, 226, 0.5f);
			for (int32 W = 0; W < 3; W++)
			{
				const FRotator R(0.f, 90.f * W + 90.f, 0.f);
				const FVector C = R.RotateVector(FVector(0.6f, 0, 1.05f));
				for (int32 S = 0; S < 6; S++)
				{
					const FSkin K = S % 2 ? Paint(40, 92, 160, 0.55f) : White();
					M.Box(C + R.RotateVector(FVector(0, -0.5f + S * 0.2f, 0)), FVector(0.02f, 0.1f, 0.85f), K, R, true);
				}
			}
			for (float X : { -0.6f, 0.6f }) for (float Y : { -0.6f, 0.6f })
				M.Bar(FVector(X, Y, 0), FVector(X, Y, 2.0f), 0.04f, 0.04f, Fe);
			M.Collide(FVector(0, 0, 1.0f), FVector(0.62f, 0.62f, 1.0f));
		}

		void Flowerbed(FMesh& M, float R)
		{
			TArray<FVector2D> K = { { R + 0.15, 0.0 }, { R + 0.15, 0.3 }, { R + 0.1, 0.36 }, { R - 0.1, 0.36 }, { R - 0.15, 0.3 } };
			M.Lathe(FVector::ZeroVector, K, FMath::Clamp(FMath::RoundToInt(R * 14.f), 24, 72), [](int32, int32) { return Granite(); });
			M.Disc(FVector(0, 0, 0.3f), 0.f, R - 0.14f, 32, Paint(52, 40, 30, 0.95f));
			M.Collide(FVector(0, 0, 0.18f), FVector(R, R, 0.18f));
		}

		/** The pond's kerb (an oval of granite) and its silty floor; the water is ALyudPublicSpaces's. */
		void Pond(FMesh& M, float Rx, float Ry)
		{
			const int32 N = 72;
			auto P = [&](int32 I, float D, float Z)
			{
				const float A = 2.f * PI * I / N;
				return FVector(FMath::Cos(A) * (Rx + D), FMath::Sin(A) * (Ry + D), Z);
			};
			const FSkin G = Granite();
			for (int32 I = 0; I < N; I++)
			{
				M.Quad(P(I, 0.f, 0.36f), P(I, 0.5f, 0.36f), P(I + 1, 0.5f, 0.36f), P(I + 1, 0.f, 0.36f), G);
				M.Quad(P(I, 0.5f, 0.36f), P(I, 0.5f, -0.1f), P(I + 1, 0.5f, -0.1f), P(I + 1, 0.5f, 0.36f), G);
				M.Quad(P(I, 0.f, -0.1f), P(I, 0.f, 0.36f), P(I + 1, 0.f, 0.36f), P(I + 1, 0.f, -0.1f), G);
			}
			M.Disc(FVector(0, 0, 0.03f), 0.f, Rx + 0.02f, N, Paint(48, 50, 36, 0.95f), Ry + 0.02f);
			for (int32 I = 0; I < 24; I++)
			{
				const float A = 2.f * PI * I / 24;
				M.Collide(FVector(FMath::Cos(A) * (Rx + 0.25f), FMath::Sin(A) * (Ry + 0.25f), 0.2f),
					FVector(0.3f, FMath::Max(Rx, Ry) * PI / 24 + 0.2f, 0.25f), FMath::RadiansToDegrees(FMath::Atan2(FMath::Sin(A) * Rx, FMath::Cos(A) * Ry)));
			}
		}

		/** The square's stage: a platform with steps at the front and a curved concrete shell behind; faces +X. */
		void Stage(FMesh& M)
		{
			const FSkin Con = Concrete(), Boards = Paint(110, 78, 50, 0.65f);
			M.Box(FVector(0, 0, 0.5f), FVector(4.5f, 7.f, 0.5f), Con);
			M.Box(FVector(0.4f, 0, 1.02f), FVector(4.0f, 6.6f, 0.02f), Boards);
			for (int32 I = 0; I < 6; I++)
			{
				M.Box(FVector(4.5f + 0.17f + I * 0.32f, 0, 0.5f - (I + 1) * 0.166f), FVector(0.17f, 3.f, 0.083f + (5 - I) * 0.166f * 0.5f), Granite());
			}
			// the shell: a quarter of an ellipsoid, open to the front
			const int32 NA = 24, NB = 10;
			auto S = [&](int32 I, int32 J, float Grow)
			{
				const float A = PI / 2 + PI * I / NA;            // round the back, from +Y to -Y through -X
				const float B = PI / 2 * J / NB;                 // up from the floor to the crown
				return FVector(-1.f + FMath::Cos(A) * FMath::Cos(B) * (3.6f + Grow), FMath::Sin(A) * FMath::Cos(B) * (6.8f + Grow),
					1.f + FMath::Sin(B) * (5.6f + Grow));
			};
			for (int32 I = 0; I < NA; I++) for (int32 J = 0; J < NB; J++)
			{
				const FSkin K = (J % 3 == 2) ? Paint(196, 190, 176, 0.8f) : Paint(222, 216, 200, 0.8f);
				M.Quad(S(I, J, 0.f), S(I, J + 1, 0.f), S(I + 1, J + 1, 0.f), S(I + 1, J, 0.f), K);
				M.Quad(S(I + 1, J, 0.25f), S(I + 1, J + 1, 0.25f), S(I, J + 1, 0.25f), S(I, J, 0.25f), Con);
			}
			for (int32 I = 0; I < NA; I++)
			{
				M.Quad(S(I, 0, 0.f), S(I + 1, 0, 0.f), S(I + 1, 0, 0.25f), S(I, 0, 0.25f), Con);
			}
			M.Collide(FVector(0, 0, 0.5f), FVector(4.5f, 7.f, 0.5f));
			M.Collide(FVector(-4.4f, 0, 3.5f), FVector(0.3f, 7.f, 2.5f));
		}

		/** A fair stall: a timber counter, four posts and a striped sloping awning; goods on the counter. Faces +X. */
		void Stall(FMesh& M, int32 V)
		{
			const FSkin Wood = Timber(), Dark = Paint(78, 54, 36, 0.75f), A = Variant4(V), B = White();
			M.Box(FVector(0.35f, 0, 0.48f), FVector(0.4f, 1.3f, 0.48f), Wood);
			M.Box(FVector(0.38f, 0, 0.98f), FVector(0.45f, 1.36f, 0.025f), Dark);
			for (float X : { -0.75f, 0.75f }) for (float Y : { -1.3f, 1.3f })
				M.Bar(FVector(X, Y, 0), FVector(X, Y, X > 0 ? 2.3f : 2.6f), 0.06f, 0.06f, Dark);
			M.Box(FVector(-0.75f, 0, 1.0f), FVector(0.02f, 1.3f, 1.0f), Wood);
			for (int32 S = 0; S < 8; S++)
			{
				const float Y = -1.3f + (S + 0.5f) * (2.6f / 8);
				M.Box(FVector(0.05f, Y, 2.48f), FVector(1.12f, 2.6f / 16 + 0.005f, 0.02f), S % 2 ? A : B, FRotator(-11.f, 0.f, 0.f), true);
			}
			// the valance: a scalloped edge along the front
			for (int32 S = 0; S < 8; S++)
			{
				const float Y = -1.3f + (S + 0.5f) * (2.6f / 8);
				M.Box(FVector(1.15f, Y, 2.17f), FVector(0.01f, 2.6f / 16, 0.1f), S % 2 ? A : B);
			}
			// goods: crates and jars in the stall's colours of produce
			FRandomStream R(V * 31 + 7);
			static const uint8 G[6][3] = { { 200, 130, 30 }, { 150, 40, 40 }, { 230, 200, 120 }, { 80, 120, 40 }, { 180, 90, 140 }, { 210, 210, 200 } };
			for (int32 I = 0; I < 6; I++)
			{
				const int32 K = R.RandRange(0, 5);
				const float Y = -1.05f + I * 0.42f;
				M.Box(FVector(0.5f, Y, 1.05f), FVector(0.2f, 0.17f, 0.05f), Paint(150, 110, 70, 0.8f));
				for (int32 J = 0; J < 3; J++)
					M.Sphere(FVector(0.4f + J * 0.1f, Y + R.FRandRange(-0.08f, 0.08f), 1.13f), 0.05f, 6, Paint(G[K][0], G[K][1], G[K][2], 0.5f));
			}
			M.Collide(FVector(0.0f, 0, 1.1f), FVector(0.8f, 1.35f, 1.1f));
		}

		/** A small roundabout carousel: a striped cone roof on a pole, eight seats round a platform. */
		/** The carousel's fixed part: a low ring step round it and the hub's foot (the rest turns: CarouselRotor). */
		void Carousel(FMesh& M)
		{
			const FSkin Red = Paint(190, 40, 40), Gold = Paint(210, 170, 70, 0.35f), Step = Paint(150, 110, 70, 0.7f);
			TArray<FVector2D> Ring = { { 4.75, 0.0 }, { 4.75, 0.18 }, { 4.2, 0.18 }, { 4.2, 0.05 } };
			M.Lathe(FVector::ZeroVector, Ring, 40, [&](int32 K, int32) { return K == 0 ? Red : Step; });
			TArray<FVector2D> Hub = { { 0.45, 0.0 }, { 0.45, 0.3 }, { 0.3, 0.38 } };
			M.Lathe(FVector::ZeroVector, Hub, 16, [&](int32, int32) { return Gold; });
			M.Collide(FVector(0, 0, 0.18f), FVector(4.f, 4.f, 0.18f));
			M.Collide(FVector(0, 0, 2.f), FVector(0.3f, 0.3f, 2.f));
		}

		/** A small Ferris wheel's fixed frame: two A-frame legs either side of the wheel (its plane is XZ), the axle. */
		void WheelFrame(FMesh& M)
		{
			const FSkin Fe = Paint(232, 228, 216, 0.5f), Base = Concrete(), Hub = Paint(196, 48, 40);
			const float H = 9.2f;
			for (float Y : { -1.1f, 1.1f })
			{
				M.Bar(FVector(-3.6f, Y * 1.6f, 0.4f), FVector(0, Y, H), 0.22f, 0.22f, Fe);
				M.Bar(FVector(3.6f, Y * 1.6f, 0.4f), FVector(0, Y, H), 0.22f, 0.22f, Fe);
				M.Bar(FVector(-2.4f, Y * 1.45f, 3.4f), FVector(2.4f, Y * 1.45f, 3.4f), 0.12f, 0.12f, Fe);
				for (float X : { -3.6f, 3.6f }) M.Box(FVector(X, Y * 1.6f, 0.2f), FVector(0.5f, 0.5f, 0.2f), Base);
			}
			M.Tube(FVector(0, -1.35f, H), FVector(0, 1.35f, H), 0.18f, 0.18f, 12, Hub, true, true);
			// the boarding platform under the lowest gondola
			M.Box(FVector(0, 0, 0.22f), FVector(1.6f, 1.6f, 0.22f), Paint(120, 90, 60, 0.7f));
			M.Collide(FVector(-3.6f, 0, 0.6f), FVector(0.6f, 2.0f, 0.6f));
			M.Collide(FVector(3.6f, 0, 0.6f), FVector(0.6f, 2.0f, 0.6f));
			M.Collide(FVector(0, 0, 0.22f), FVector(1.6f, 1.6f, 0.22f));
		}
	}

	void BuildCarouselRotor(FMesh& M)
	{
		const FSkin Red = Paint(190, 40, 40), Wh = Paint(232, 228, 216, 0.6f), Gold = Paint(210, 170, 70, 0.35f);
		TArray<FVector2D> Deck = { { 4.0, 0.05 }, { 4.0, 0.35 }, { 0.3, 0.35 } };
		M.Lathe(FVector::ZeroVector, Deck, 32, [&](int32 K, int32) { return K == 0 ? Red : Paint(150, 110, 70, 0.7f); });
		M.Tube(FVector(0, 0, 0.35f), FVector(0, 0, 4.0f), 0.25f, 0.25f, 12, Gold, false, false);
		TArray<FVector2D> Roof = { { 4.4, 3.6 }, { 4.4, 3.9 }, { 0.0, 5.4 } };
		M.Lathe(FVector::ZeroVector, Roof, 16, [&](int32, int32 S) { return S % 2 ? Red : Wh; });
		TArray<FVector2D> Under = { { 0.0, 5.3 }, { 4.35, 3.85 } };
		M.Lathe(FVector::ZeroVector, Under, 16, [&](int32, int32 S) { FSkin K = S % 2 ? Red : Wh; K.Color *= 0.6f; return K; });
		for (int32 I = 0; I < 8; I++)
		{
			const float A = 2.f * PI * I / 8;
			const FVector P(FMath::Cos(A) * 3.2f, FMath::Sin(A) * 3.2f, 0.f);
			M.Tube(P + FVector(0, 0, 0.35f), P + FVector(0, 0, 3.8f), 0.025f, 0.025f, 6, Gold, false, false);
			const FRotator R(0.f, FMath::RadiansToDegrees(A) + 90.f, 0.f);
			M.Box(P + FVector(0, 0, 1.0f), FVector(0.45f, 0.14f, 0.18f), Variant4(I), R, true);
			M.Box(P + R.RotateVector(FVector(0.4f, 0, 1.35f)), FVector(0.1f, 0.08f, 0.22f), Variant4(I), R, true);
		}
		for (int32 I = 0; I < 16; I++)
		{
			const float A = 2.f * PI * I / 16;
			M.Sphere(FVector(FMath::Cos(A) * 4.42f, FMath::Sin(A) * 4.42f, 3.62f), 0.06f, 6, Glass());
		}
	}

	void BuildWheelRotor(FMesh& M, float R, int32 Gondolas)
	{
		// in the wheel's own frame: the axle along Y at the origin, the rim in the XZ plane
		const FSkin Fe = Paint(232, 228, 216, 0.5f), Blue = Paint(40, 92, 160, 0.55f);
		const int32 N = 48;
		for (float Y : { -0.55f, 0.55f })
		{
			for (int32 I = 0; I < N; I++)
			{
				const float A0 = 2.f * PI * I / N, A1 = 2.f * PI * (I + 1) / N;
				M.Bar(FVector(FMath::Cos(A0) * R, Y, FMath::Sin(A0) * R), FVector(FMath::Cos(A1) * R, Y, FMath::Sin(A1) * R), 0.12f, 0.12f, Blue);
				M.Bar(FVector(FMath::Cos(A0) * R * 0.55f, Y, FMath::Sin(A0) * R * 0.55f), FVector(FMath::Cos(A1) * R * 0.55f, Y, FMath::Sin(A1) * R * 0.55f), 0.07f, 0.07f, Fe);
			}
			for (int32 I = 0; I < Gondolas * 2; I++)
			{
				const float A = 2.f * PI * I / (Gondolas * 2);
				M.Bar(FVector(0, Y, 0), FVector(FMath::Cos(A) * R, Y, FMath::Sin(A) * R), 0.05f, 0.05f, Fe);
			}
		}
		for (int32 I = 0; I < Gondolas; I++)
		{
			const float A = 2.f * PI * I / Gondolas;
			M.Bar(FVector(FMath::Cos(A) * R, -0.6f, FMath::Sin(A) * R), FVector(FMath::Cos(A) * R, 0.6f, FMath::Sin(A) * R), 0.06f, 0.06f, Fe);
		}
		for (int32 I = 0; I < Gondolas * 3; I++)
		{
			const float A = 2.f * PI * (I + 0.5f) / (Gondolas * 3);
			for (float Y : { -0.62f, 0.62f }) M.Sphere(FVector(FMath::Cos(A) * R, Y, FMath::Sin(A) * R), 0.07f, 6, Glass());
		}
	}

	void BuildGondola(FMesh& M, int32 Variant)
	{
		// hangs from its pivot at the origin: a hanger and an open car below
		const FSkin Fe = Paint(232, 228, 216, 0.5f), Car = Variant4(Variant), Seat = Paint(120, 90, 60, 0.7f);
		for (float Y : { -0.5f, 0.5f }) M.Bar(FVector(0, Y, 0), FVector(0, Y * 0.9f, -1.0f), 0.04f, 0.04f, Fe);
		M.Box(FVector(0, 0, -1.25f), FVector(0.65f, 0.55f, 0.04f), Car, FRotator::ZeroRotator, true);
		for (float X : { -0.62f, 0.62f }) M.Box(FVector(X, 0, -1.05f), FVector(0.03f, 0.55f, 0.2f), Car, FRotator::ZeroRotator, true);
		for (float Y : { -0.52f, 0.52f }) M.Box(FVector(0, Y, -1.05f), FVector(0.65f, 0.03f, 0.2f), Car, FRotator::ZeroRotator, true);
		for (float X : { -0.4f, 0.4f }) M.Box(FVector(X, 0, -1.08f), FVector(0.15f, 0.5f, 0.06f), Seat, FRotator::ZeroRotator, true);
		TArray<FVector2D> Canopy = { { 0.75, -0.25 }, { 0.0, -0.05 } };
		M.Lathe(FVector::ZeroVector, Canopy, 8, [&](int32, int32 S) { return S % 2 ? Car : Fe; });
	}

	namespace
	{
		void Swings(FMesh& M)
		{
			const FSkin Fe = Paint(40, 92, 160), Ch = Paint(160, 160, 160, 0.4f), Seat = Paint(214, 150, 34);
			for (float Y : { -1.6f, 1.6f })
			{
				M.Bar(FVector(-0.9f, Y, 0), FVector(0, Y, 2.3f), 0.07f, 0.07f, Fe);
				M.Bar(FVector(0.9f, Y, 0), FVector(0, Y, 2.3f), 0.07f, 0.07f, Fe);
			}
			M.Tube(FVector(0, -1.65f, 2.3f), FVector(0, 1.65f, 2.3f), 0.05f, 0.05f, 8, Fe, true, true);
			for (float Y : { -0.7f, 0.7f })
			{
				for (float D : { -0.22f, 0.22f }) M.Bar(FVector(0, Y + D, 2.28f), FVector(0, Y + D, 0.45f), 0.012f, 0.012f, Ch);
				M.Box(FVector(0, Y, 0.43f), FVector(0.12f, 0.26f, 0.02f), Seat, FRotator::ZeroRotator, true);
			}
			M.Collide(FVector(0, -1.6f, 1.1f), FVector(0.9f, 0.1f, 1.1f));
			M.Collide(FVector(0, 1.6f, 1.1f), FVector(0.9f, 0.1f, 1.1f));
		}

		void Slide(FMesh& M)
		{
			const FSkin Fe = Paint(46, 128, 82), Chute = Paint(200, 200, 200, 0.25f), Step = Paint(214, 150, 34);
			for (float Y : { -0.3f, 0.3f })
			{
				M.Bar(FVector(-1.4f, Y, 0), FVector(-1.1f, Y, 1.9f), 0.05f, 0.05f, Fe);
				M.Bar(FVector(-0.75f, Y, 0), FVector(-0.75f, Y, 1.5f), 0.06f, 0.06f, Fe);
				M.Bar(FVector(-0.75f, Y, 1.5f), FVector(-0.75f, Y, 2.3f), 0.04f, 0.04f, Fe);
			}
			for (int32 I = 0; I < 6; I++) M.Box(FVector(-1.35f + I * 0.05f, 0, 0.25f + I * 0.25f), FVector(0.04f, 0.3f, 0.015f), Step);
			M.Box(FVector(-0.95f, 0, 1.5f), FVector(0.25f, 0.32f, 0.03f), Step);
			const FVector A(-0.7f, 0, 1.5f), B(1.6f, 0, 0.25f);
			M.Bar(A, B, 0.5f, 0.03f, Chute);
			M.Bar(A + FVector(0, 0.26f, 0.1f), B + FVector(0, 0.26f, 0.1f), 0.03f, 0.2f, Chute);
			M.Bar(A + FVector(0, -0.26f, 0.1f), B + FVector(0, -0.26f, 0.1f), 0.03f, 0.2f, Chute);
			M.Bar(B, B + FVector(0.4f, 0, 0), 0.5f, 0.03f, Chute);
			M.Collide(FVector(-0.9f, 0, 1.1f), FVector(0.5f, 0.35f, 1.1f));
		}

		void Climber(FMesh& M)
		{
			const FSkin Fe = Paint(196, 48, 40);
			for (int32 I = 0; I < 4; I++)
			{
				const float Yaw = 45.f * I;
				const FRotator R(0.f, Yaw, 0.f);
				FVector Prev;
				for (int32 J = 0; J <= 10; J++)
				{
					const float A = PI * J / 10;
					const FVector P = R.RotateVector(FVector(FMath::Cos(A) * 1.6f, 0, FMath::Sin(A) * 1.6f));
					if (J > 0) M.Bar(Prev, P, 0.04f, 0.04f, Fe);
					Prev = P;
				}
			}
			for (float Z : { 0.6f, 1.2f })
			{
				const float R = FMath::Sqrt(1.6f * 1.6f - Z * Z);
				FVector Prev;
				for (int32 J = 0; J <= 16; J++)
				{
					const float A = 2.f * PI * J / 16;
					const FVector P(FMath::Cos(A) * R, FMath::Sin(A) * R, Z);
					if (J > 0) M.Bar(Prev, P, 0.035f, 0.035f, Fe);
					Prev = P;
				}
			}
			M.Collide(FVector(0, 0, 0.8f), FVector(1.1f, 1.1f, 0.8f));
		}

		void Seesaw(FMesh& M)
		{
			const FSkin Fe = Paint(40, 92, 160), Wood = Paint(214, 150, 34);
			M.Box(FVector(0, 0, 0.22f), FVector(0.12f, 0.2f, 0.22f), Fe);
			M.Box(FVector(0, 0, 0.5f), FVector(1.7f, 0.13f, 0.03f), Wood, FRotator(-6.f, 90.f, 0.f), true);
			for (float Y : { -1.45f, 1.45f }) M.Bar(FVector(-0.15f, Y, 0.55f - Y * 0.1f), FVector(0.15f, Y, 0.55f - Y * 0.1f), 0.03f, 0.03f, Fe, FVector::UpVector);
			M.Collide(FVector(0, 0, 0.3f), FVector(0.2f, 1.7f, 0.3f));
		}

		void Sandpit(FMesh& M)
		{
			const FSkin Wood = Paint(170, 120, 70, 0.75f), Sand = Paint(214, 196, 150, 0.95f);
			for (int32 I = 0; I < 4; I++)
			{
				const FRotator R(0.f, 90.f * I, 0.f);
				M.Box(R.RotateVector(FVector(1.6f, 0, 0.15f)), FVector(0.1f, 1.7f, 0.15f), Wood, R);
			}
			M.Box(FVector(0, 0, 0.12f), FVector(1.5f, 1.5f, 0.12f), Sand);
		}
	}

	bool PropCollides(const FString& K)
	{
		return !(K == TEXT("umbrella") || K == TEXT("lounger") || K == TEXT("tree_grate") || K == TEXT("sandpit"));
	}

	bool PropGlows(const FString& K)
	{
		return K == TEXT("lamp") || K == TEXT("lamp_park") || K == TEXT("carousel");
	}

	void BuildProp(FMesh& M, const FString& K, int32 V, const TSharedPtr<FJsonObject>& X)
	{
		if (K == TEXT("bench_park")) BenchPark(M);
		else if (K == TEXT("bench_promenade")) BenchPromenade(M);
		else if (K == TEXT("lamp")) Lamp(M);
		else if (K == TEXT("lamp_park")) LampPark(M);
		else if (K == TEXT("railing")) Railing(M);
		else if (K == TEXT("railing_post")) RailingPost(M);
		else if (K == TEXT("tree_grate")) TreeGrate(M);
		else if (K == TEXT("bin")) Bin(M);
		else if (K == TEXT("umbrella")) Umbrella(M, V);
		else if (K == TEXT("lounger")) Lounger(M);
		else if (K == TEXT("changing_booth")) ChangingBooth(M);
		else if (K == TEXT("flowerbed")) Flowerbed(M, X.IsValid() && X->HasField(TEXT("r")) ? (float)X->GetNumberField(TEXT("r")) : 3.f);
		else if (K == TEXT("pond"))
		{
			const TArray<TSharedPtr<FJsonValue>>* R = nullptr;
			float Rx = 12.f, Ry = 18.f;
			if (X.IsValid() && X->TryGetArrayField(TEXT("r"), R) && R->Num() == 2)
			{
				Rx = (*R)[0]->AsNumber();
				Ry = (*R)[1]->AsNumber();
			}
			Pond(M, Rx, Ry);
		}
		else if (K == TEXT("stage")) Stage(M);
		else if (K == TEXT("stall")) Stall(M, V);
		else if (K == TEXT("carousel")) Carousel(M);
		else if (K == TEXT("wheel")) WheelFrame(M);
		else if (K == TEXT("swings")) Swings(M);
		else if (K == TEXT("slide")) Slide(M);
		else if (K == TEXT("climber")) Climber(M);
		else if (K == TEXT("seesaw")) Seesaw(M);
		else if (K == TEXT("sandpit")) Sandpit(M);
	}

	void SkinsOf(const FString& Skin, FSkin& Top, FSkin& Side)
	{
		if (Skin == TEXT("deck")) { Top = Make(Srgb(184, 180, 170), 0.82f, EPattern::Slabs, 1.0f); Side = Make(Srgb(150, 146, 138), 0.85f, EPattern::Masonry, 0.5f); }
		else if (Skin == TEXT("square")) { Top = Make(Srgb(190, 176, 154), 0.8f, EPattern::Slabs, 1.0f); Side = Make(Srgb(160, 150, 136), 0.85f, EPattern::Masonry, 0.5f); }
		else if (Skin == TEXT("granite")) { Top = Make(Srgb(122, 116, 112), 0.7f, EPattern::Setts, 0.2f); Side = Make(Srgb(112, 106, 104), 0.75f, EPattern::Masonry, 0.3f); }
		else if (Skin == TEXT("gravel")) { Top = Make(Srgb(178, 158, 128), 0.95f, EPattern::Gravel, 0.f); Side = Make(Srgb(132, 124, 112), 0.9f); }
		else if (Skin == TEXT("rubber")) { Top = Make(Srgb(152, 64, 54), 0.95f, EPattern::Rubber, 0.f); Side = Make(Srgb(90, 88, 84), 0.9f); }
		else if (Skin == TEXT("basin")) { Top = Make(Srgb(78, 84, 76), 0.6f); Side = Make(Srgb(96, 98, 92), 0.7f); }
		else if (Skin == TEXT("boards")) { Top = Make(Srgb(128, 96, 66), 0.75f, EPattern::Boards, 0.14f); Side = Make(Srgb(96, 72, 50), 0.8f); }
		else { Top = Concrete(); Side = Concrete(); }
	}

	void BuildBlock(FMesh& M, const FString& Skin)
	{
		const FSkin S = Concrete();
		M.Box(FVector::ZeroVector, FVector(0.5f), S, FRotator::ZeroRotator, true);
	}

	void AddBlock(FMesh& M, const FVector& C, float Yaw, const FVector& Size, const FString& Skin, float WorldZ0)
	{
		FSkin Top, Side;
		SkinsOf(Skin, Top, Side);
		const FRotator R(0.f, Yaw, 0.f);
		const FVector H = Size * 0.5;
		auto P = [&](float X, float Y, float Z) { return C + R.RotateVector(FVector(X * H.X, Y * H.Y, Z * H.Z)); };
		// top: metres from the (-x, -y) corner
		M.QuadUV(P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1), FVector2f(0, 0), FVector2f(Size.X, 0), FVector2f(Size.X, Size.Y),
			FVector2f(0, Size.Y), Top);
		// sides: along the face from its left end, world height (courses line up across blocks)
		const float Zb = WorldZ0 - H.Z, Zt = WorldZ0 + H.Z;
		auto Side4 = [&](const FVector& A, const FVector& B, float Len)
		{
			M.QuadUV(A - FVector(0, 0, Size.Z), B - FVector(0, 0, Size.Z), B, A, FVector2f(0, Zb), FVector2f(Len, Zb), FVector2f(Len, Zt), FVector2f(0, Zt), Side);
		};
		Side4(P(1, -1, 1), P(1, 1, 1), Size.Y);
		Side4(P(-1, 1, 1), P(-1, -1, 1), Size.Y);
		Side4(P(1, 1, 1), P(-1, 1, 1), Size.X);
		Side4(P(-1, -1, 1), P(1, -1, 1), Size.X);
	}

	void BuildFountainStone(FMesh& M, const TSharedPtr<FJsonObject>& S)
	{
		auto N = [&S](const TCHAR* K, float D) { return S.IsValid() && S->HasField(K) ? (float)S->GetNumberField(K) : D; };
		const float RimR = N(TEXT("rim_r"), 9.f), RimZ = N(TEXT("rim_z"), 0.46f), RimW = N(TEXT("rim_w"), 0.45f);
		const float LowR = N(TEXT("lower_r"), 8.55f), LowFloor = N(TEXT("lower_floor"), 0.06f);
		const float UpZ = N(TEXT("upper_level"), 2.42f), UpFloor = N(TEXT("upper_floor"), 2.26f), LipR = N(TEXT("lip_r"), 1.45f);
		const float LipZ = N(TEXT("lip_z"), 2.42f), NozZ = N(TEXT("nozzle_z"), 3.45f), StemR = N(TEXT("stem_r"), 0.36f), NeckR = N(TEXT("neck_r"), 0.18f);
		const FSkin Stone = Make(Srgb(214, 206, 192), 0.62f), Inner = Make(Srgb(150, 158, 150), 0.5f), Bronze = Make(Srgb(178, 136, 60), 0.32f);
		const int32 Sides = 96;
		// the basin's wall: a granite seat rim, the outside face down to the paving, the inside down to the floor
		TArray<FVector2D> Rim = { { RimR + 0.06, 0.0 }, { RimR + 0.06, RimZ - 0.08 }, { RimR + 0.1, RimZ - 0.05 }, { RimR + 0.1, RimZ },
			{ RimR - RimW, RimZ }, { RimR - RimW, LowFloor } };
		M.Lathe(FVector::ZeroVector, Rim, Sides, [&](int32 K, int32) { return K >= 4 ? Inner : Stone; });
		M.Disc(FVector(0, 0, LowFloor), StemR, RimR - RimW + 0.01f, Sides, Inner);
		// the centrepiece: a drum, the stem, the upper bowl, the sun
		// a drum in the basin (inside where the veil lands), a fluted stem, the upper bowl's cup and lip
		const float DrumR = FMath::Max(1.15f, LipR * 1.1f), DrumZ = FMath::Max(0.62f, UpZ * 0.27f);
		const float BowlZ = UpZ - 0.32f;
		TArray<FVector2D> Mid = { { DrumR, LowFloor }, { DrumR, DrumZ }, { DrumR - 0.1, DrumZ + 0.08 }, { DrumR * 0.55, DrumZ + 0.16 },
			{ StemR + 0.08, DrumZ + 0.3 }, { StemR, DrumZ + 0.48 }, { StemR * 0.85, (DrumZ + BowlZ) * 0.5 + 0.3 }, { NeckR + 0.06, BowlZ - 0.1 },
			{ NeckR, BowlZ }, { LipR * 0.35, BowlZ + 0.07 }, { LipR - 0.05, UpZ - 0.12 }, { LipR, LipZ }, { LipR - 0.07, LipZ + 0.02 },
			{ LipR - 0.1, UpFloor + 0.1 }, { 0.3, UpFloor } };
		M.Lathe(FVector::ZeroVector, Mid, 48, [&](int32 K, int32) { return K >= 12 ? Inner : Stone; });
		M.Disc(FVector(0, 0, UpFloor), 0.f, 0.32f, 24, Inner);
		M.Tube(FVector(0, 0, UpFloor), FVector(0, 0, UpZ + 0.25f), 0.1f, 0.08f, 12, Bronze, false, false);
		// the sun: a gilded ball with sixteen short rays, the nozzle on its crown
		const float SunZ = (UpZ + NozZ) * 0.5f + 0.05f, SunR = FMath::Max(0.2f, (NozZ - UpZ) * 0.38f);
		M.Sphere(FVector(0, 0, SunZ), SunR, 20, Bronze);
		for (int32 I = 0; I < 16; I++)
		{
			const float A = 2.f * PI * I / 16;
			const FVector D(FMath::Cos(A), 0.f, FMath::Sin(A));
			const FVector Dr = FRotator(0.f, 90.f, 0.f).RotateVector(D);   // the disc of rays faces the sea (+X)
			M.Tube(FVector(0, 0, SunZ) + Dr * SunR * 0.9f, FVector(0, 0, SunZ) + Dr * SunR * (I % 2 ? 1.9f : 1.6f), 0.045f, 0.0f, 6, Bronze, false, false);
		}
		M.Tube(FVector(0, 0, SunZ + SunR * 0.9f), FVector(0, 0, NozZ), 0.05f, 0.03f, 8, Bronze, false, true);
		// collision: the rim as a ring of boxes (sit on it, can't walk in), the centrepiece as a column
		for (int32 I = 0; I < 32; I++)
		{
			const float A = 2.f * PI * (I + 0.5f) / 32;
			M.Collide(FVector(FMath::Cos(A) * (RimR - RimW * 0.5f + 0.05f), FMath::Sin(A) * (RimR - RimW * 0.5f + 0.05f), RimZ * 0.5f),
				FVector(RimW * 0.5f + 0.05f, RimR * PI / 32 + 0.1f, RimZ * 0.5f), FMath::RadiansToDegrees(A));
		}
		M.Collide(FVector(0, 0, 1.2f), FVector(DrumR * 0.95f, DrumR * 0.95f, 1.2f));
		(void)LowR;
	}
}
