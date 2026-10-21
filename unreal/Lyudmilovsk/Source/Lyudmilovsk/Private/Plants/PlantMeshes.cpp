#include "Plants/PlantMeshes.h"

#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "TextureResource.h"

namespace
{
	using PlantModel::Hash01;
	constexpr float GoldenAngle = 2.39996323f;

	struct FMeshData
	{
		TArray<FVector3f> Pos, Nrm, Tan;
		TArray<FVector2f> UV;
		TArray<FIntVector3> Tris;

		int32 Add(const FVector3f& P, const FVector3f& N, const FVector3f& T, const FVector2f& U)
		{
			Nrm.Add(N); Tan.Add(T); UV.Add(U);
			return Pos.Add(P);
		}
	};

	UStaticMesh* BuildMesh(UObject* Outer, UStaticMesh* Mesh, FName Name, const FMeshData& D)
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
		const FPolygonGroupID PG = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[PG] = Name;
		MD.ReserveNewVertices(D.Pos.Num());
		MD.ReserveNewVertexInstances(D.Pos.Num());
		MD.ReserveNewTriangles(D.Tris.Num());
		TArray<FVertexInstanceID> Inst;
		Inst.Reserve(D.Pos.Num());
		for (int32 I = 0; I < D.Pos.Num(); I++)
		{
			const FVertexID V = MD.CreateVertex();
			VPos[V] = D.Pos[I];
			const FVertexInstanceID VI = MD.CreateVertexInstance(V);
			const FVector3f N = D.Nrm[I].GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector);
			VNrm[VI] = N;
			FVector3f T = (D.Tan[I] - N * FVector3f::DotProduct(D.Tan[I], N)).GetSafeNormal();
			if (T.IsNearlyZero())
			{
				T = FVector3f::CrossProduct(FMath::Abs(N.Z) < 0.9f ? FVector3f::UpVector : FVector3f::XAxisVector, N).GetSafeNormal();
			}
			VTan[VI] = T;
			VSign[VI] = 1.f;
			VUV.Set(VI, 0, D.UV[I]);
			Inst.Add(VI);
		}
		for (const FIntVector3& T : D.Tris)
		{
			MD.CreateTriangle(PG, { Inst[T.X], Inst[T.Y], Inst[T.Z] });
		}
		if (!Mesh)
		{
			Mesh = NewObject<UStaticMesh>(Outer, Name, RF_Transient);
			Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, Name));
		}
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Mesh->BuildFromMeshDescriptions({ &MD }, Params);
		if (UBodySetup* Body = Mesh->GetBodySetup())
		{
			Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		}
		return Mesh;
	}

	FVector3f AnyPerp(const FVector3f& D)
	{
		return FVector3f::CrossProduct(FMath::Abs(D.Z) < 0.9f ? FVector3f::UpVector : FVector3f::XAxisVector, D).GetSafeNormal();
	}

	// --- leaves

	/** A blade in leaf space (m): base at the origin, midrib along +Y, facing +Z. */
	void BuildBlade(const FPlantLeafShape& L, float Length, FMeshData& Out, const FTransform3f& Xf)
	{
		FMeshData B;
		const float W = L.Width * Length;
		if (L.Form == EPlantLeafForm::Palmate)
		{
			const int32 N = 48;
			const int32 Lobes = FMath::Max(L.Lobes, 1);
			const float Spread = 0.72f * PI, Spacing = Lobes > 1 ? 2.f * Spread / (Lobes - 1) : 2.f * Spread;
			const float Mid = (Lobes - 1) * 0.5f;
			const int32 C = B.Add(FVector3f(0.f, 0.f, L.Fold * Length * 0.3f), FVector3f::UpVector, FVector3f::XAxisVector, FVector2f(0.5f, 0.5f));
			for (int32 I = 0; I <= N; I++)
			{
				const float Phi = -0.86f * PI + 1.72f * PI * I / N;
				const int32 K = FMath::Clamp(FMath::RoundToInt((Phi + Spread) / Spacing), 0, Lobes - 1);
				const float LobeLen = Mid > 0.f ? 1.f - 0.45f * FMath::Pow(FMath::Abs(K - Mid) / Mid, 1.5f) : 1.f;
				const float Cc = FMath::Clamp(1.f - FMath::Abs(Phi - (-Spread + K * Spacing)) / (Spacing * 0.5f), 0.f, 1.f);
				const float R = Length * LobeLen * ((1.f - L.LobeDepth) + L.LobeDepth * FMath::Pow(Cc, 1.4f));
				const FVector3f P(FMath::Sin(Phi) * R * L.Width * 0.85f, FMath::Cos(Phi) * R, -0.12f * Length * FMath::Square(R / Length));
				B.Add(P, FVector3f::UpVector, FVector3f::XAxisVector, FVector2f(0.5f + P.X / (2.2f * Length), 0.5f - P.Y / (2.2f * Length)));
				if (I > 0) B.Tris.Add(FIntVector3(C, C + I, C + I + 1));
			}
		}
		else
		{
			const bool bTeeth = L.Teeth > 0 && L.Teeth <= 8;
			const int32 N = L.Lobes > 0 ? L.Lobes * 4 + 4 : (bTeeth ? L.Teeth * 4 : 8);
			const float A = FMath::Clamp(L.WidestAt, 0.05f, 0.95f), Al = 2.2f * A, Be = 2.2f * (1.f - A);
			const float Norm = FMath::Pow(A, Al) * FMath::Pow(1.f - A, Be);
			for (int32 I = 0; I <= N; I++)
			{
				const float T = (float)I / N;
				float F = FMath::Pow(T, Al) * FMath::Pow(1.f - T, Be) / Norm;
				if (L.Lobes > 0) F *= (1.f - L.LobeDepth) + L.LobeDepth * FMath::Sqrt(FMath::Abs(FMath::Sin(PI * L.Lobes * T)));
				if (bTeeth) F *= 1.f - L.TeethDepth * FMath::Frac(L.Teeth * T);
				const float HalfW = 0.5f * W * F, Y = T * Length, Z = -0.15f * Length * T * T;
				B.Add(FVector3f(-HalfW, Y, Z), FVector3f::UpVector, FVector3f::XAxisVector, FVector2f(0.5f - HalfW / W, 1.f - T));
				B.Add(FVector3f(0.f, Y, Z + L.Fold * HalfW), FVector3f::UpVector, FVector3f::XAxisVector, FVector2f(0.5f, 1.f - T));
				B.Add(FVector3f(HalfW, Y, Z), FVector3f::UpVector, FVector3f::XAxisVector, FVector2f(0.5f + HalfW / W, 1.f - T));
				if (I > 0)
				{
					const int32 P0 = 3 * (I - 1), P1 = 3 * I;
					B.Tris.Add(FIntVector3(P0, P1, P1 + 1)); B.Tris.Add(FIntVector3(P0, P1 + 1, P0 + 1));
					B.Tris.Add(FIntVector3(P0 + 1, P1 + 1, P1 + 2)); B.Tris.Add(FIntVector3(P0 + 1, P1 + 2, P0 + 2));
				}
			}
		}
		// every triangle faces up (Unreal's front face: (P2 - P0) x (P1 - P0)); smooth normals
		TArray<FVector3f> N;
		N.SetNumZeroed(B.Pos.Num());
		for (FIntVector3& T : B.Tris)
		{
			FVector3f Fn = FVector3f::CrossProduct(B.Pos[T.Z] - B.Pos[T.X], B.Pos[T.Y] - B.Pos[T.X]);
			if (Fn.Z < 0.f) { Swap(T.Y, T.Z); Fn = -Fn; }
			N[T.X] += Fn; N[T.Y] += Fn; N[T.Z] += Fn;
		}
		const int32 Base = Out.Pos.Num();
		for (int32 I = 0; I < B.Pos.Num(); I++)
		{
			Out.Add(Xf.TransformPosition(B.Pos[I]) * 100.f, Xf.TransformVectorNoScale(N[I].GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector)),
			        Xf.TransformVectorNoScale(FVector3f::XAxisVector), B.UV[I]);
		}
		for (const FIntVector3& T : B.Tris) Out.Tris.Add(FIntVector3(Base + T.X, Base + T.Y, Base + T.Z));
	}

	// --- textures

	UTexture2D* MakeTexture(UObject* Outer, int32 W, int32 H, TArray<FColor> Pixels, bool bSRGB, bool bNormal)
	{
		UTexture2D* T = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
		T->SRGB = bSRGB;
		T->CompressionSettings = bNormal ? TC_Normalmap : TC_Default;
		T->AddressX = TA_Wrap;
		T->AddressY = TA_Wrap;
		FTexturePlatformData* PD = T->GetPlatformData();
		for (int32 Level = 0;; Level++)
		{
			FTexture2DMipMap* Mip = Level == 0 ? &PD->Mips[0] : new FTexture2DMipMap(W, H);
			if (Level > 0) PD->Mips.Add(Mip);
			void* Data = Mip->BulkData.Lock(LOCK_READ_WRITE);
			if (Level > 0) Data = Mip->BulkData.Realloc((int64)W * H * 4);
			FMemory::Memcpy(Data, Pixels.GetData(), (SIZE_T)W * H * 4);
			Mip->BulkData.Unlock();
			if (W == 1 && H == 1) break;
			// box-filter the next level down (normals renormalised)
			const int32 W2 = FMath::Max(W / 2, 1), H2 = FMath::Max(H / 2, 1);
			TArray<FColor> Next;
			Next.SetNumUninitialized(W2 * H2);
			for (int32 Y = 0; Y < H2; Y++)
			{
				for (int32 X = 0; X < W2; X++)
				{
					FVector4f Sum(0, 0, 0, 0);
					for (int32 K = 0; K < 4; K++)
					{
						const FColor& C = Pixels[FMath::Min(Y * 2 + K / 2, H - 1) * W + FMath::Min(X * 2 + K % 2, W - 1)];
						Sum += FVector4f(C.R, C.G, C.B, C.A);
					}
					Sum *= 0.25f;
					if (bNormal)
					{
						const FVector3f Nn = FVector3f(Sum.X / 127.5f - 1.f, Sum.Y / 127.5f - 1.f, Sum.Z / 127.5f - 1.f).GetSafeNormal();
						Sum = FVector4f((Nn.X + 1.f) * 127.5f, (Nn.Y + 1.f) * 127.5f, (Nn.Z + 1.f) * 127.5f, 255.f);
					}
					Next[Y * W2 + X] = FColor((uint8)FMath::RoundToInt(Sum.X), (uint8)FMath::RoundToInt(Sum.Y), (uint8)FMath::RoundToInt(Sum.Z), (uint8)FMath::RoundToInt(Sum.W));
				}
			}
			Pixels = MoveTemp(Next);
			W = W2;
			H = H2;
		}
		T->UpdateResource();
		return T;
	}

	/** Value noise that tiles with period (Px, Py) lattice cells over the unit square. */
	float Noise(uint32 Seed, float U, float V, int32 Px, int32 Py)
	{
		const float X = U * Px, Y = V * Py;
		const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
		const float Fx = X - X0, Fy = Y - Y0;
		const float Sx = Fx * Fx * (3.f - 2.f * Fx), Sy = Fy * Fy * (3.f - 2.f * Fy);
		auto L = [&](int32 I, int32 J) { return Hash01(Seed, (uint32)(((I % Px) + Px) % Px), (uint32)(((J % Py) + Py) % Py)); };
		return FMath::Lerp(FMath::Lerp(L(X0, Y0), L(X0 + 1, Y0), Sx), FMath::Lerp(L(X0, Y0 + 1), L(X0 + 1, Y0 + 1), Sx), Sy);
	}

	float Fbm(uint32 Seed, float U, float V, int32 Px, int32 Py, int32 Octaves)
	{
		float Sum = 0.f, Amp = 1.f, Total = 0.f;
		for (int32 O = 0; O < Octaves; O++)
		{
			Sum += Amp * Noise(Seed + O * 7919u, U, V, Px << O, Py << O);
			Total += Amp;
			Amp *= 0.5f;
		}
		return Sum / Total;
	}

	/** Distance to the nearest cell edge (F2 - F1) and the nearest cell's id, on a tiling jittered grid. */
	float Voronoi(uint32 Seed, float U, float V, int32 Cx, int32 Cy, float Aspect, float& OutCell)
	{
		const float X = U * Cx, Y = V * Cy;
		const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
		float F1 = 1e9f, F2 = 1e9f;
		for (int32 J = -1; J <= 1; J++)
		{
			for (int32 I = -1; I <= 1; I++)
			{
				const int32 Ci = ((X0 + I) % Cx + Cx) % Cx, Cj = ((Y0 + J) % Cy + Cy) % Cy;
				const float Px = X0 + I + 0.15f + 0.7f * Hash01(Seed, Ci, Cj, 1), Py = Y0 + J + 0.15f + 0.7f * Hash01(Seed, Ci, Cj, 2);
				const float D = FMath::Sqrt(FMath::Square((Px - X) / Cx) + FMath::Square((Py - Y) / Cy * Aspect));
				if (D < F1) { F2 = F1; F1 = D; OutCell = Hash01(Seed, Ci, Cj, 3); }
				else if (D < F2) { F2 = D; }
			}
		}
		return (F2 - F1) * Cx;
	}

	float Smooth(float E0, float E1, float X)
	{
		const float T = FMath::Clamp((X - E0) / (E1 - E0), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	/** Bark relief 0 (furrow bottom) .. 1 (ridge top) at (U around, V along). */
	float BarkHeight(const FPlantBark& B, uint32 Seed, float U, float V)
	{
		const float Warp = (Fbm(Seed + 1, U, V, 4, 2, 3) - 0.5f) * 0.12f;
		const float Fine = Fbm(Seed + 2, U, V, 24, 48, 3);
		switch (B.Pattern)
		{
		case EPlantBarkPattern::Furrows:
		case EPlantBarkPattern::Blocks:
		{
			// ridges: noise stretched along the trunk, so its crests run lengthwise and braid
			const float R = 0.65f * Noise(Seed + 3, U + Warp, V, 9, 2) + 0.35f * Noise(Seed + 4, U + Warp * 2.f, V, 18, 3);
			float H = Smooth(0.32f, 0.62f, R);
			if (B.Pattern == EPlantBarkPattern::Blocks)
			{
				// cracks across the ridges
				const float C = FMath::Abs(Noise(Seed + 5, U + Warp, V + Warp, 5, 9) - 0.5f);
				H *= 0.3f + 0.7f * Smooth(0.015f, 0.07f, C);
			}
			return FMath::Clamp(H * (0.85f + 0.15f * Fine) + 0.08f * (Fine - 0.5f), 0.f, 1.f);
		}
		case EPlantBarkPattern::Plates:
		{
			float Cell = 0.f;
			const float E = Voronoi(Seed + 6, U + Warp, V, 5, 9, B.TileHeight / B.TileWidth, Cell);
			return FMath::Clamp((0.45f + 0.45f * Cell) * Smooth(0.f, 0.12f, E) + 0.15f * (Fine - 0.5f), 0.f, 1.f);
		}
		default:
		{
			const float Grain = Noise(Seed + 7, U + Warp, V, 22, 2);
			// lenticels: short horizontal dashes
			const float Lx = U * 40.f, Ly = V * 70.f;
			const bool bDash = Hash01(Seed + 8, FMath::FloorToInt(Lx) % 40, FMath::FloorToInt(Ly) % 70) < 0.06f && FMath::Abs(FMath::Frac(Ly) - 0.5f) < 0.18f;
			return FMath::Clamp(0.6f + 0.25f * (Grain - 0.5f) + 0.15f * (Fine - 0.5f) - (bDash ? 0.25f : 0.f), 0.f, 1.f);
		}
		}
	}
}

UStaticMesh* PlantMeshes::BuildWood(UObject* Outer, UStaticMesh* Mesh, const TArray<FPlantSkeleton::FTube>& Tubes, const FPlantBark& Bark)
{
	FMeshData D;
	for (const FPlantSkeleton::FTube& T : Tubes)
	{
		const int32 N = T.P.Num();
		if (N < 2) continue;
		const float R0 = T.R[0];
		const int32 Sides = R0 > 0.12f ? 14 : R0 > 0.04f ? 10 : R0 > 0.012f ? 6 : 4;
		// the bark tiles a whole number of times around, scaled down on thin twigs
		const float Circ = 2.f * PI * R0;
		const int32 Repeats = FMath::Max(1, FMath::RoundToInt(Circ / Bark.TileWidth));
		const float K = FMath::Max(Circ / (Repeats * Bark.TileWidth), 0.08f);
		FVector3f Dir = (T.P[1] - T.P[0]).GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector);
		FVector3f Nf = AnyPerp(Dir);
		float V = 0.f;
		const int32 Base = D.Pos.Num();
		for (int32 I = 0; I < N; I++)
		{
			if (I > 0)
			{
				V += FVector3f::Dist(T.P[I], T.P[I - 1]) / (Bark.TileHeight * K);
				Dir = (T.P[FMath::Min(I + 1, N - 1)] - T.P[I - 1]).GetSafeNormal(UE_SMALL_NUMBER, Dir);
				// parallel transport: the ring's frame turns with the branch but never twists
				Nf = (Nf - Dir * FVector3f::DotProduct(Nf, Dir)).GetSafeNormal(UE_SMALL_NUMBER, AnyPerp(Dir));
			}
			const FVector3f Bf = FVector3f::CrossProduct(Dir, Nf);
			for (int32 S = 0; S <= Sides; S++)
			{
				const float A = 2.f * PI * S / Sides;
				const FVector3f Nrm = Nf * FMath::Cos(A) + Bf * FMath::Sin(A);
				const FVector3f Tan = Bf * FMath::Cos(A) - Nf * FMath::Sin(A);
				D.Add((T.P[I] + Nrm * T.R[I]) * 100.f, Nrm, Tan, FVector2f((float)S / Sides * Repeats, -V));
			}
		}
		for (int32 I = 0; I + 1 < N; I++)
		{
			for (int32 S = 0; S < Sides; S++)
			{
				const int32 A = Base + I * (Sides + 1) + S, B = A + 1, C = A + Sides + 1, E = C + 1;
				D.Tris.Add(FIntVector3(A, C, B));
				D.Tris.Add(FIntVector3(B, C, E));
			}
		}
	}
	if (D.Tris.IsEmpty())
	{
		// an empty mesh can't be built: one degenerate triangle
		for (int32 I = 0; I < 3; I++) D.Add(FVector3f::ZeroVector, FVector3f::UpVector, FVector3f::XAxisVector, FVector2f::ZeroVector);
		D.Tris.Add(FIntVector3(0, 1, 2));
	}
	return BuildMesh(Outer, Mesh, TEXT("SM_PlantWood"), D);
}

UStaticMesh* PlantMeshes::BuildLeafCluster(UObject* Outer, const FPlantSpecies& S)
{
	const FPlantLeafShape& L = S.Leaf;
	FMeshData D;
	const int32 Count = FMath::Max(L.PerCluster, 1);
	for (int32 I = 0; I < Count; I++)
	{
		const float Phi = I * GoldenAngle + Hash01(77, I, 1) * 0.5f;
		const FVector3f Out(FMath::Cos(Phi), FMath::Sin(Phi), 0.f);
		const FVector3f Node = Out * (S.LeafClusterSize * 0.1f * (0.5f + Hash01(77, I, 2))) + FVector3f(0.f, 0.f, ((I + 0.5f) / Count - 0.5f) * S.LeafClusterSize * 0.25f);
		const float Length = L.Length * (0.8f + 0.4f * Hash01(77, I, 3));
		// the stalk reaches out and droops a little; the blade carries on from its end
		const FVector3f Stalk = Out * (L.Petiole * Length) - FVector3f(0.f, 0.f, 0.25f * L.Petiole * Length);
		const FVector3f Across = FVector3f(-Out.Y, Out.X, 0.f) * 0.0015f;
		const int32 P0 = D.Add(Node * 100.f - Across * 100.f, FVector3f::UpVector, Out, FVector2f(0.5f, 1.f));
		D.Add(Node * 100.f + Across * 100.f, FVector3f::UpVector, Out, FVector2f(0.5f, 1.f));
		D.Add((Node + Stalk) * 100.f + Across * 100.f, FVector3f::UpVector, Out, FVector2f(0.5f, 1.f));
		D.Add((Node + Stalk) * 100.f - Across * 100.f, FVector3f::UpVector, Out, FVector2f(0.5f, 1.f));
		D.Tris.Add(FIntVector3(P0, P0 + 2, P0 + 1));
		D.Tris.Add(FIntVector3(P0, P0 + 3, P0 + 2));
		const FRotator Rot(-(8.f + 22.f * Hash01(77, I, 4)), FMath::RadiansToDegrees(Phi) - 90.f, (Hash01(77, I, 5) - 0.5f) * 40.f);
		BuildBlade(L, Length, D, FTransform3f(FRotator3f(Rot), Node + Stalk));
	}
	return BuildMesh(Outer, nullptr, *FString::Printf(TEXT("SM_Leaves_%s"), *S.Name.ToString()), D);
}

void PlantMeshes::BuildBarkTextures(UObject* Outer, const FPlantSpecies& S, uint32 Seed, UTexture2D*& OutAlbedo, UTexture2D*& OutNormal)
{
	constexpr int32 W = 256, H = 512;
	const FPlantBark& B = S.Bark;
	TArray<float> Hgt;
	Hgt.SetNumUninitialized(W * H);
	TArray<FColor> Albedo, Normal;
	Albedo.SetNumUninitialized(W * H);
	Normal.SetNumUninitialized(W * H);
	const uint32 TexSeed = Seed * 131u + GetTypeHash(S.Name);
	for (int32 Y = 0; Y < H; Y++)
	{
		for (int32 X = 0; X < W; X++)
		{
			const float U = (X + 0.5f) / W, V = (Y + 0.5f) / H;
			const float Hh = BarkHeight(B, TexSeed, U, V);
			Hgt[Y * W + X] = Hh;
			const float Mottle = 0.8f + 0.4f * Fbm(TexSeed + 20, U, V, 6, 10, 3);
			const FLinearColor C = FMath::Lerp(B.FurrowColour, B.RidgeColour, Hh) * Mottle;
			Albedo[Y * W + X] = FLinearColor(C.R, C.G, C.B, 1.f).ToFColor(true);
		}
	}
	// relief: Depth cm between furrow and ridge
	const float Dx = B.TileWidth / W, Dy = B.TileHeight / H, Relief = B.Depth * 0.01f;
	for (int32 Y = 0; Y < H; Y++)
	{
		for (int32 X = 0; X < W; X++)
		{
			const float Sx = (Hgt[Y * W + (X + 1) % W] - Hgt[Y * W + (X + W - 1) % W]) * Relief / (2.f * Dx);
			const float Sy = (Hgt[((Y + 1) % H) * W + X] - Hgt[((Y + H - 1) % H) * W + X]) * Relief / (2.f * Dy);
			const FVector3f N = FVector3f(-Sx, Sy, 1.f).GetSafeNormal();
			Normal[Y * W + X] = FColor((uint8)FMath::RoundToInt((N.X + 1.f) * 127.5f), (uint8)FMath::RoundToInt((N.Y + 1.f) * 127.5f), (uint8)FMath::RoundToInt((N.Z + 1.f) * 127.5f), 255);
		}
	}
	OutAlbedo = MakeTexture(Outer, W, H, MoveTemp(Albedo), true, false);
	OutNormal = MakeTexture(Outer, W, H, MoveTemp(Normal), false, true);
}

UTexture2D* PlantMeshes::BuildLeafDetail(UObject* Outer, const FPlantLeafShape& L)
{
	constexpr int32 W = 128;
	TArray<FColor> Px;
	Px.SetNumUninitialized(W * W);
	for (int32 Y = 0; Y < W; Y++)
	{
		for (int32 X = 0; X < W; X++)
		{
			const float U = (X + 0.5f) / W, V = (Y + 0.5f) / W;
			float Vein = 0.f;
			if (L.Form == EPlantLeafForm::Palmate)
			{
				// main veins radiate from the stalk into each lobe
				const float Dx = U - 0.5f, Dy = 0.5f - V, R = FMath::Sqrt(Dx * Dx + Dy * Dy);
				const float Phi = FMath::Atan2(Dx, Dy);
				const int32 Lobes = FMath::Max(L.Lobes, 1);
				const float Spacing = Lobes > 1 ? 1.44f * PI / (Lobes - 1) : PI;
				const float K = (Phi + 0.72f * PI) / Spacing;
				const float Dist = FMath::Abs(K - FMath::RoundToFloat(K)) * Spacing * R;
				Vein = FMath::Exp(-FMath::Square(Dist / 0.006f)) * FMath::Clamp(1.f - R * 1.6f, 0.f, 1.f);
			}
			else
			{
				// midrib, and side veins sweeping toward the tip
				const float Dx = FMath::Abs(U - 0.5f), T = 1.f - V;
				const float Mid = FMath::Exp(-FMath::Square(Dx / 0.012f));
				const float Phase = (T - Dx * 1.2f) * 9.f;
				const float Side = FMath::Exp(-FMath::Square((FMath::Frac(Phase) - 0.5f) / 0.06f)) * FMath::Clamp(1.f - Dx * 2.2f, 0.f, 1.f);
				Vein = FMath::Max(Mid, Side * 0.6f);
			}
			const float Mottle = Fbm(5, U, V, 8, 8, 3);
			const float G = FMath::Clamp(0.84f + 0.16f * Vein + 0.08f * (Mottle - 0.5f), 0.f, 1.f);
			const uint8 C = (uint8)FMath::RoundToInt(G * 255.f);
			Px[Y * W + X] = FColor(C, C, C, 255);
		}
	}
	return MakeTexture(Outer, W, W, MoveTemp(Px), true, false);
}
