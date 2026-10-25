#include "SeaLifeMeshes.h"

#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"

namespace LyudSeaLife
{
	namespace
	{
		struct FBuilder
		{
			TArray<FVector3f> P, N;
			TArray<FVector2f> UV;
			TArray<FLinearColor> C;
			TArray<int32> I;

			int32 V(const FVector3f& Pos, const FVector3f& Nrm, const FVector2f& Uv, const FLinearColor& Col)
			{
				P.Add(Pos);
				N.Add(Nrm.GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector));
				UV.Add(Uv);
				C.Add(Col);
				return P.Num() - 1;
			}

			/** A triangle facing Out: Unreal's front face is where (P2 - P0) x (P1 - P0) points. */
			void Tri(int32 A, int32 B, int32 Cc, const FVector3f& Out)
			{
				const FVector3f F = FVector3f::CrossProduct(P[Cc] - P[A], P[B] - P[A]);
				if (FVector3f::DotProduct(F, Out) >= 0.f)
				{
					I.Append({ A, B, Cc });
				}
				else
				{
					I.Append({ A, Cc, B });
				}
			}

			/** A triangle facing along its vertices' normals. */
			void TriN(int32 A, int32 B, int32 Cc) { Tri(A, B, Cc, N[A] + N[B] + N[Cc]); }

			/** A low-poly ellipsoid (closed), radii R, around Center, rotated by Rot. */
			void Ellipsoid(const FVector3f& Center, const FVector3f& R, const FQuat4f& Rot, int32 Seg, int32 Rings, const FLinearColor& Col, float Bumps = 0.f, uint32 Seed = 0)
			{
				const int32 Base = P.Num();
				for (int32 J = 0; J <= Rings; J++)
				{
					const float Th = PI * J / Rings;
					for (int32 K = 0; K < Seg; K++)
					{
						const float Ph = 2.f * PI * K / Seg;
						FVector3f U(FMath::Sin(Th) * FMath::Cos(Ph), FMath::Sin(Th) * FMath::Sin(Ph), FMath::Cos(Th));
						float Bump = 1.f;
						if (Bumps > 0.f)
						{
							// lumps: a few low harmonics, the same for every vertex at a pole
							const float Px = U.X * 2.1f + Seed * 0.37f, Py = U.Y * 2.3f + Seed * 0.71f, Pz = U.Z * 1.9f;
							Bump = 1.f + Bumps * (0.55f * FMath::Sin(Px * 1.7f + Py) * FMath::Cos(Pz * 1.3f - Px) + 0.3f * FMath::Sin(Py * 3.1f + Pz * 2.7f) + 0.15f * FMath::Sin(Px * 5.3f - Pz * 4.1f));
						}
						const FVector3f Pos = Center + Rot.RotateVector(U * R * Bump);
						const FVector3f Nrm = Rot.RotateVector(FVector3f(U.X / R.X, U.Y / R.Y, U.Z / R.Z));
						V(Pos, Nrm, FVector2f(float(K) / Seg, float(J) / Rings), Col);
					}
				}
				for (int32 J = 0; J < Rings; J++)
				{
					for (int32 K = 0; K < Seg; K++)
					{
						const int32 K1 = (K + 1) % Seg;
						const int32 A = Base + J * Seg + K, B = Base + J * Seg + K1, Cc = Base + (J + 1) * Seg + K, D = Base + (J + 1) * Seg + K1;
						if (J > 0) Tri(A, B, Cc, P[A] - Center);
						if (J < Rings - 1) Tri(B, D, Cc, P[B] - Center);
					}
				}
			}

			/** Smooth normals from the faces for the vertices From.. (a lumpy closed shape). */
			void FaceNormals(int32 From)
			{
				TArray<FVector3f> Acc;
				Acc.Init(FVector3f::ZeroVector, P.Num() - From);
				for (int32 K = 0; K + 2 < I.Num(); K += 3)
				{
					const int32 A = I[K], B = I[K + 1], Cc = I[K + 2];
					if (A < From || B < From || Cc < From) continue;
					// front face normal: (P2 - P0) x (P1 - P0)
					const FVector3f F = FVector3f::CrossProduct(P[Cc] - P[A], P[B] - P[A]);
					Acc[A - From] += F;
					Acc[B - From] += F;
					Acc[Cc - From] += F;
				}
				// the ring seam and the poles have their own copies: weld by position
				TMap<FIntVector, FVector3f> ByPos;
				auto Key = [&](int32 V) { return FIntVector(FMath::RoundToInt(P[V].X * 100.f), FMath::RoundToInt(P[V].Y * 100.f), FMath::RoundToInt(P[V].Z * 100.f)); };
				for (int32 V = From; V < P.Num(); V++) ByPos.FindOrAdd(Key(V)) += Acc[V - From];
				for (int32 V = From; V < P.Num(); V++) N[V] = ByPos[Key(V)].GetSafeNormal(UE_SMALL_NUMBER, N[V]);
			}

			/** A flat ribbon along a polyline, Width wide across Side; R of vertex colour runs Along0..Along1. */
			void Ribbon(const TArray<FVector3f>& Line, const TArray<float>& Widths, const FVector3f& Side, float Along0, float Along1, const FLinearColor& Col)
			{
				const int32 Base = P.Num();
				const int32 Nn = Line.Num();
				for (int32 K = 0; K < Nn; K++)
				{
					const FVector3f T = (Line[FMath::Min(K + 1, Nn - 1)] - Line[FMath::Max(K - 1, 0)]).GetSafeNormal();
					const FVector3f S = (Side - T * FVector3f::DotProduct(Side, T)).GetSafeNormal(UE_SMALL_NUMBER, FVector3f::XAxisVector);
					const FVector3f Nrm = FVector3f::CrossProduct(T, S);
					FLinearColor Cc = Col;
					Cc.R = FMath::Lerp(Along0, Along1, float(K) / (Nn - 1));
					V(Line[K] - S * Widths[K] * 0.5f, Nrm, FVector2f(0.f, float(K) / (Nn - 1)), Cc);
					V(Line[K] + S * Widths[K] * 0.5f, Nrm, FVector2f(1.f, float(K) / (Nn - 1)), Cc);
				}
				for (int32 K = 0; K + 1 < Nn; K++)
				{
					const int32 A = Base + K * 2;
					TriN(A, A + 1, A + 2);
					TriN(A + 1, A + 3, A + 2);
				}
			}
		};

		UStaticMesh* Build(const TCHAR* Name, const FBuilder& D)
		{
			FMeshDescription MD;
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
			Attr.GetPolygonGroupMaterialSlotNames()[PG] = Name;
			TArray<FVertexInstanceID> Inst;
			for (int32 K = 0; K < D.P.Num(); K++)
			{
				const FVertexID Vx = MD.CreateVertex();
				VPos[Vx] = D.P[K];
				const FVertexInstanceID VI = MD.CreateVertexInstance(Vx);
				VNrm[VI] = D.N[K];
				FVector3f Tan = FVector3f::CrossProduct(FVector3f::UpVector, D.N[K]);
				if (Tan.SizeSquared() < 1e-6f) Tan = FVector3f::XAxisVector;
				VTan[VI] = Tan.GetSafeNormal();
				VSign[VI] = 1.f;
				VUV.Set(VI, 0, D.UV[K]);
				VCol[VI] = FVector4f(D.C[K].R, D.C[K].G, D.C[K].B, 1.f);
				Inst.Add(VI);
			}
			for (int32 K = 0; K + 2 < D.I.Num(); K += 3)
			{
				MD.CreateTriangle(PG, { Inst[D.I[K]], Inst[D.I[K + 1]], Inst[D.I[K + 2]] });
			}
			UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), Name, RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
			Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, Name));
			UStaticMesh::FBuildMeshDescriptionsParams Params;
			Params.bMarkPackageDirty = false;
			Params.bBuildSimpleCollision = false;
			Params.bCommitMeshDescription = false;
			Params.bFastBuild = true;
			Mesh->BuildFromMeshDescriptions({ &MD }, Params);
			Mesh->AddToRoot();
			return Mesh;
		}

		/** Deterministic 0..1. */
		float Rnd(uint32& S)
		{
			S = S * 1664525u + 1013904223u;
			return float(S >> 8) / 16777216.f;
		}

		FVector3f Horizontal(float Yaw) { return FVector3f(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.f); }

		/** A forked ribbon branch: the wrack's flat thallus and the red weed's strands. */
		void Branch(FBuilder& B, const FVector3f& Start, const FVector3f& Dir, const FVector3f& Side, float Len, float Width, int32 Depth, float Along0, float AlongPerLen,
			float Fork, float Droop, uint32& S, bool bBladders)
		{
			TArray<FVector3f> Line;
			TArray<float> Widths;
			FVector3f Pt = Start, D = Dir;
			constexpr int32 Steps = 3;
			for (int32 K = 0; K <= Steps; K++)
			{
				Line.Add(Pt);
				Widths.Add(Width * (1.f - 0.25f * K / Steps));
				D = (D + FVector3f(0.f, 0.f, -Droop) + FVector3f(Rnd(S) - 0.5f, Rnd(S) - 0.5f, 0.f) * 0.15f).GetSafeNormal();
				Pt += D * (Len / Steps);
			}
			const float Along1 = Along0 + Len * AlongPerLen;
			B.Ribbon(Line, Widths, Side, Along0, Along1, FLinearColor(0.f, 0.4f + 0.4f * Rnd(S), 0.f));
			const FVector3f Tip = Line.Last();
			if (Depth <= 0)
			{
				return;
			}
			if (bBladders && Rnd(S) < 0.7f)
			{
				// a pair of air bladders either side of the midrib, just below the fork
				const FVector3f At = Line[Steps - 1] * 0.4f + Tip * 0.6f;
				const FVector3f Across = (Side - D * FVector3f::DotProduct(Side, D)).GetSafeNormal();
				const float R = Width * 0.32f;
				const FQuat4f Q = FQuat4f::FindBetweenNormals(FVector3f::UpVector, D);
				B.Ellipsoid(At + Across * Width * 0.22f, FVector3f(R, R, R * 1.5f), Q, 6, 4, FLinearColor(Along1, 0.5f, 1.f));
				B.Ellipsoid(At - Across * Width * 0.22f, FVector3f(R, R, R * 1.5f), Q, 6, 4, FLinearColor(Along1, 0.5f, 1.f));
			}
			// dichotomous: two halves at +-Fork in the blade's plane
			const FVector3f Axis = FVector3f::CrossProduct(D, Side).GetSafeNormal();
			for (int32 K = -1; K <= 1; K += 2)
			{
				const float A = K * Fork * (0.8f + 0.4f * Rnd(S));
				const FVector3f D2 = FQuat4f(Axis, A).RotateVector(D);
				const FVector3f S2 = FQuat4f(D2, (Rnd(S) - 0.5f) * 0.6f).RotateVector(Side);
				Branch(B, Tip, D2, S2, Len * (0.75f + 0.15f * Rnd(S)), Width * 0.85f, Depth - 1, Along1, AlongPerLen, Fork, Droop, S, bBladders);
			}
		}
	}

	UStaticMesh* Eelgrass()
	{
		static UStaticMesh* Mesh = nullptr;
		if (Mesh) return Mesh;
		// a shoot of long ribbon leaves from the buried rhizome, 40-90 cm, bent over by the water
		FBuilder B;
		uint32 S = 101;
		for (int32 Shoot = 0; Shoot < 4; Shoot++)
		{
			const float Yaw = Rnd(S) * 2.f * PI;
			const FVector3f Base = Horizontal(Yaw) * (Rnd(S) * 9.f);
			const int32 Leaves = 3 + int32(Rnd(S) * 3.f);
			for (int32 L = 0; L < Leaves; L++)
			{
				const float H = 40.f + 50.f * Rnd(S);
				const float Lean = Rnd(S) * 2.f * PI;
				const float Bend = 0.25f + 0.45f * Rnd(S);
				TArray<FVector3f> Line;
				TArray<float> W;
				constexpr int32 Steps = 7;
				for (int32 K = 0; K <= Steps; K++)
				{
					const float T = float(K) / Steps;
					Line.Add(Base + FVector3f(0, 0, H * T * (1.f - 0.3f * Bend * T)) + Horizontal(Lean) * (H * Bend * T * T * 0.6f));
					W.Add(0.7f * (K == Steps ? 0.35f : 1.f));
				}
				B.Ribbon(Line, W, Horizontal(Yaw + L * 0.9f + PI * 0.5f), 0.f, 1.f, FLinearColor(0.f, 0.3f + 0.6f * Rnd(S), 0.f));
			}
		}
		return Mesh = Build(TEXT("SM_SeaEelgrass"), B);
	}

	UStaticMesh* Fucus()
	{
		static UStaticMesh* Mesh = nullptr;
		if (Mesh) return Mesh;
		// bladderwrack: flat forked fronds with a midrib and paired air bladders, ~30 cm, from one holdfast
		FBuilder B;
		uint32 S = 202;
		for (int32 F = 0; F < 4; F++)
		{
			const float Yaw = F * 1.57f + Rnd(S) * 0.8f;
			const FVector3f Dir = (FVector3f(0, 0, 1) + Horizontal(Yaw) * (0.35f + 0.3f * Rnd(S))).GetSafeNormal();
			Branch(B, FVector3f(0, 0, 0), Dir, Horizontal(Yaw + PI * 0.5f), 7.f + 2.f * Rnd(S), 2.2f, 3, 0.f, 1.f / 32.f, 0.42f, 0.05f, S, true);
		}
		B.Ellipsoid(FVector3f(0, 0, 0.5f), FVector3f(2.f, 2.f, 1.f), FQuat4f::Identity, 6, 3, FLinearColor(0.f, 0.2f, 0.f));
		return Mesh = Build(TEXT("SM_SeaFucus"), B);
	}

	UStaticMesh* Furcellaria()
	{
		static UStaticMesh* Mesh = nullptr;
		if (Mesh) return Mesh;
		// the red weed: a tuft of thin round strands forking a few times, upright, 15-25 cm
		FBuilder B;
		uint32 S = 303;
		for (int32 F = 0; F < 7; F++)
		{
			const float Yaw = Rnd(S) * 2.f * PI;
			const FVector3f Dir = (FVector3f(0, 0, 1) + Horizontal(Yaw) * (0.2f + 0.3f * Rnd(S))).GetSafeNormal();
			for (int32 Cross = 0; Cross < 2; Cross++)
			{
				uint32 S2 = S + F * 17;   // the same strand twice, crossed, so it reads round from every side
				Branch(B, FVector3f(0, 0, 0), Dir, Horizontal(Yaw + Cross * PI * 0.5f), 5.f + 2.f * Rnd(S2), 0.45f, 3, 0.f, 1.f / 20.f, 0.35f, 0.f, S2, false);
			}
			Rnd(S);
		}
		return Mesh = Build(TEXT("SM_SeaFurcellaria"), B);
	}

	UStaticMesh* Boulder()
	{
		static UStaticMesh* Mesh = nullptr;
		if (Mesh) return Mesh;
		// a rounded glacial erratic, 1 m across, its lower third in the sand
		FBuilder B;
		B.Ellipsoid(FVector3f(0, 0, 25.f), FVector3f(50.f, 45.f, 40.f), FQuat4f::Identity, 24, 14, FLinearColor(0.f, 0.5f, 0.f), 0.28f, 5);
		B.FaceNormals(0);
		return Mesh = Build(TEXT("SM_SeaBoulder"), B);
	}

	UStaticMesh* Mussels()
	{
		static UStaticMesh* Mesh = nullptr;
		if (Mesh) return Mesh;
		// a clump of blue mussels, 3-6 cm each, hinge down, packed together
		FBuilder B;
		uint32 S = 404;
		for (int32 K = 0; K < 14; K++)
		{
			const float A = Rnd(S) * 2.f * PI, R = 9.f * FMath::Sqrt(Rnd(S));
			const float L = 3.f + 3.f * Rnd(S);
			const FQuat4f Q = FQuat4f(FVector3f::UpVector, Rnd(S) * 2.f * PI) * FQuat4f(FVector3f::XAxisVector, 0.6f + Rnd(S) * 0.8f);
			B.Ellipsoid(FVector3f(R * FMath::Cos(A), R * FMath::Sin(A), L * 0.35f), FVector3f(L * 0.5f, L * 0.22f, L * 0.28f), Q, 7, 4, FLinearColor(0.f, 0.2f + 0.5f * Rnd(S), 0.f));
		}
		return Mesh = Build(TEXT("SM_SeaMussels"), B);
	}

	UStaticMesh* Shells()
	{
		static UStaticMesh* Mesh = nullptr;
		if (Mesh) return Mesh;
		// empty cockle and Baltic tellin valves lying on the sand
		FBuilder B;
		uint32 S = 505;
		for (int32 K = 0; K < 9; K++)
		{
			const float A = Rnd(S) * 2.f * PI, R = 22.f * FMath::Sqrt(Rnd(S));
			const float L = 1.2f + 1.4f * Rnd(S);
			const FQuat4f Q = FQuat4f(FVector3f::UpVector, Rnd(S) * 2.f * PI) * FQuat4f(FVector3f::XAxisVector, (Rnd(S) - 0.5f) * 0.5f);
			B.Ellipsoid(FVector3f(R * FMath::Cos(A), R * FMath::Sin(A), 0.1f), FVector3f(L * 0.55f, L * 0.5f, L * 0.18f), Q, 7, 3, FLinearColor(0.f, 0.4f + 0.6f * Rnd(S), 0.f));
		}
		return Mesh = Build(TEXT("SM_SeaShells"), B);
	}

	UStaticMesh* Flounder()
	{
		static UStaticMesh* Mesh = nullptr;
		if (Mesh) return Mesh;
		// a flatfish lying on its blind side: an oval disc fringed by fins, a tail, both eyes on top; 30 cm, head +X
		FBuilder B;
		constexpr int32 Seg = 20;
		const int32 Top = B.V(FVector3f(2.f, 0.f, 2.2f), FVector3f::UpVector, FVector2f(0.5f, 0.5f), FLinearColor(0.4f, 0.5f, 0.f));
		const int32 Bot = B.V(FVector3f(2.f, 0.f, 0.2f), -FVector3f::UpVector, FVector2f(0.5f, 0.5f), FLinearColor(0.4f, 0.5f, 0.f));
		TArray<int32> RimT, RimB;
		for (int32 K = 0; K < Seg; K++)
		{
			const float A = 2.f * PI * K / Seg;
			const float X = 12.f * FMath::Cos(A) + 2.f, Y = 7.5f * FMath::Sin(A);
			const float Along = FMath::Clamp((14.f - X) / 26.f, 0.f, 1.f);
			RimT.Add(B.V(FVector3f(X, Y, 0.9f), FVector3f(FMath::Cos(A) * 0.4f, FMath::Sin(A) * 0.4f, 1.f), FVector2f(0.5f + 0.5f * FMath::Cos(A), 0.5f + 0.5f * FMath::Sin(A)), FLinearColor(Along, 0.5f, 0.f)));
			RimB.Add(B.V(FVector3f(X, Y, 0.3f), FVector3f(FMath::Cos(A) * 0.4f, FMath::Sin(A) * 0.4f, -1.f), FVector2f(0.5f + 0.5f * FMath::Cos(A), 0.5f + 0.5f * FMath::Sin(A)), FLinearColor(Along, 0.5f, 0.f)));
		}
		for (int32 K = 0; K < Seg; K++)
		{
			const int32 K1 = (K + 1) % Seg;
			B.Tri(Top, RimT[K], RimT[K1], FVector3f::UpVector);
			B.Tri(Bot, RimB[K], RimB[K1], -FVector3f::UpVector);
			const FVector3f Out = (B.P[RimT[K]] - FVector3f(2.f, 0.f, 0.6f)).GetSafeNormal();
			B.Tri(RimT[K], RimB[K], RimT[K1], Out);
			B.Tri(RimT[K1], RimB[K], RimB[K1], Out);
		}
		// the tail fan
		const int32 T0 = B.V(FVector3f(-9.5f, 0.f, 0.6f), FVector3f::UpVector, FVector2f(0, 0), FLinearColor(0.85f, 0.5f, 0.f));
		const int32 T1 = B.V(FVector3f(-15.f, 4.f, 0.6f), FVector3f::UpVector, FVector2f(0, 1), FLinearColor(1.f, 0.5f, 0.f));
		const int32 T2 = B.V(FVector3f(-15.f, -4.f, 0.6f), FVector3f::UpVector, FVector2f(1, 1), FLinearColor(1.f, 0.5f, 0.f));
		B.Tri(T0, T1, T2, FVector3f::UpVector);
		// eyes: two bumps on the right-eyed upper side
		B.Ellipsoid(FVector3f(9.f, 1.8f, 1.6f), FVector3f(0.7f, 0.7f, 0.6f), FQuat4f::Identity, 6, 3, FLinearColor(0.f, 0.f, 1.f));
		B.Ellipsoid(FVector3f(8.f, -0.6f, 1.6f), FVector3f(0.7f, 0.7f, 0.6f), FQuat4f::Identity, 6, 3, FLinearColor(0.f, 0.f, 1.f));
		return Mesh = Build(TEXT("SM_SeaFlounder"), B);
	}

	UStaticMesh* Jellyfish()
	{
		static UStaticMesh* Mesh = nullptr;
		if (Mesh) return Mesh;
		// moon jelly: a shallow clear bell 20 cm across with four pink horseshoe rings, a fringe of fine
		// tentacles at the rim and four frilled oral arms hanging under it
		FBuilder B;
		constexpr int32 Seg = 24, Rings = 6;
		const int32 Base = B.P.Num();
		for (int32 J = 0; J <= Rings; J++)
		{
			const float T = float(J) / Rings;            // 0 top .. 1 rim
			const float R = 10.f * FMath::Sin(T * PI * 0.5f);
			const float Z = 4.5f * FMath::Cos(T * PI * 0.5f);
			for (int32 K = 0; K < Seg; K++)
			{
				const float A = 2.f * PI * K / Seg;
				const FVector3f N(FMath::Cos(A) * FMath::Sin(T * PI * 0.5f) / 10.f, FMath::Sin(A) * FMath::Sin(T * PI * 0.5f) / 10.f, FMath::Cos(T * PI * 0.5f) / 4.5f);
				B.V(FVector3f(R * FMath::Cos(A), R * FMath::Sin(A), Z), N, FVector2f(float(K) / Seg, T), FLinearColor(T * T, 0.5f, 0.f));
			}
		}
		for (int32 J = 0; J < Rings; J++)
		{
			for (int32 K = 0; K < Seg; K++)
			{
				const int32 K1 = (K + 1) % Seg;
				const int32 A = Base + J * Seg + K, Bb = Base + J * Seg + K1, C = Base + (J + 1) * Seg + K, D = Base + (J + 1) * Seg + K1;
				if (J > 0) B.TriN(A, Bb, C);
				B.TriN(Bb, D, C);
			}
		}
		// the four rings (gonads), seen through the bell
		for (int32 G = 0; G < 4; G++)
		{
			const float A0 = G * PI * 0.5f + PI * 0.25f;
			B.Ellipsoid(FVector3f(3.6f * FMath::Cos(A0), 3.6f * FMath::Sin(A0), 3.0f), FVector3f(2.0f, 1.0f, 0.5f), FQuat4f(FVector3f::UpVector, A0 + PI * 0.5f), 8, 3, FLinearColor(0.1f, 0.5f, 1.f));
		}
		uint32 S = 606;
		// rim fringe
		for (int32 K = 0; K < 32; K++)
		{
			const float A = 2.f * PI * K / 32;
			const FVector3f At(10.f * FMath::Cos(A), 10.f * FMath::Sin(A), 0.f);
			const float L = 3.f + 2.f * Rnd(S);
			B.Ribbon({ At, At + FVector3f(0, 0, -L * 0.5f) + At * 0.03f, At + FVector3f(0, 0, -L) + At * 0.05f }, { 0.12f, 0.1f, 0.06f }, Horizontal(A + PI * 0.5f), 1.f, 1.f, FLinearColor(1.f, 0.5f, 0.f));
		}
		// oral arms
		for (int32 K = 0; K < 4; K++)
		{
			const float A = K * PI * 0.5f;
			const FVector3f D = Horizontal(A);
			B.Ribbon({ FVector3f(0, 0, 0.5f), D * 1.5f + FVector3f(0, 0, -3.f), D * 2.5f + FVector3f(0, 0, -7.f), D * 3.f + FVector3f(0, 0, -10.f) },
				{ 1.4f, 1.2f, 0.9f, 0.3f }, Horizontal(A + PI * 0.5f), 0.4f, 0.8f, FLinearColor(0.f, 0.8f, 0.f));
		}
		return Mesh = Build(TEXT("SM_SeaJellyfish"), B);
	}

	UStaticMesh* Fish()
	{
		static UStaticMesh* Mesh = nullptr;
		if (Mesh) return Mesh;
		// a sprat: a slim silver spindle 10 cm long, head +X, flattened sideways, with a forked tail
		FBuilder B;
		constexpr int32 Seg = 8;
		const float Prof[][2] = { { 5.f, 0.f }, { 4.4f, 0.45f }, { 3.f, 0.85f }, { 1.f, 1.f }, { -1.5f, 0.85f }, { -3.5f, 0.45f }, { -4.6f, 0.18f } };
		constexpr int32 NP = UE_ARRAY_COUNT(Prof);
		const int32 Base = B.P.Num();
		for (int32 J = 0; J < NP; J++)
		{
			const float Along = (5.f - Prof[J][0]) / 10.f;
			for (int32 K = 0; K < Seg; K++)
			{
				const float A = 2.f * PI * K / Seg;
				const FVector3f N(0.f, FMath::Cos(A), FMath::Sin(A));
				// dark back, silver flanks and belly: G = 0 on the back .. 1 underneath
				B.V(FVector3f(Prof[J][0], Prof[J][1] * 0.55f * FMath::Cos(A), Prof[J][1] * FMath::Sin(A)), N, FVector2f(Along, float(K) / Seg),
					FLinearColor(Along, 0.5f - 0.5f * FMath::Sin(A), 0.f));
			}
		}
		for (int32 J = 0; J + 1 < NP; J++)
		{
			for (int32 K = 0; K < Seg; K++)
			{
				const int32 K1 = (K + 1) % Seg;
				const int32 A = Base + J * Seg + K, Bb = Base + J * Seg + K1, C = Base + (J + 1) * Seg + K, D = Base + (J + 1) * Seg + K1;
				const FVector3f Axis(Prof[J][0], 0.f, 0.f);
				if (J > 0) B.Tri(A, Bb, C, B.P[A] - Axis);
				B.Tri(Bb, D, C, B.P[Bb] - Axis);
			}
		}
		const int32 T0 = B.V(FVector3f(-4.4f, 0.f, 0.f), FVector3f::YAxisVector, FVector2f(1, 0.5f), FLinearColor(0.95f, 0.3f, 0.f));
		const int32 T1 = B.V(FVector3f(-6.2f, 0.f, 1.6f), FVector3f::YAxisVector, FVector2f(1, 0), FLinearColor(1.f, 0.3f, 0.f));
		const int32 T2 = B.V(FVector3f(-5.6f, 0.f, 0.f), FVector3f::YAxisVector, FVector2f(1, 0.5f), FLinearColor(1.f, 0.3f, 0.f));
		const int32 T3 = B.V(FVector3f(-6.2f, 0.f, -1.6f), FVector3f::YAxisVector, FVector2f(1, 1), FLinearColor(1.f, 0.3f, 0.f));
		B.Tri(T0, T1, T2, FVector3f::YAxisVector);
		B.Tri(T0, T2, T3, FVector3f::YAxisVector);
		return Mesh = Build(TEXT("SM_SeaFish"), B);
	}
}
