#include "Plants/PlantMeshes.h"

#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "Engine/Texture2D.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "TextureResource.h"

namespace
{
	using PlantModel::Hash01;
	constexpr float GoldenAngle = 2.39996323f;

	struct FMeshData
	{
		TArray<FVector3f> Pos, Nrm, Tan;
		TArray<FVector2f> UV, UV1;
		/** Vertex colour: on wood, R = rough dark base, G = young twig bark (M_PlantBark). */
		TArray<FVector4f> Col;
		TArray<FIntVector3> Tris;

		int32 Add(const FVector3f& P, const FVector3f& N, const FVector3f& T, const FVector2f& U, const FVector4f& C = FVector4f(0.f, 0.f, 0.f, 1.f))
		{
			Nrm.Add(N); Tan.Add(T); UV.Add(U); UV1.Add(FVector2f::ZeroVector); Col.Add(C);
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
		TVertexInstanceAttributesRef<FVector4f> VCol = Attr.GetVertexInstanceColors();
		VUV.SetNumChannels(2);
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
			VUV.Set(VI, 1, D.UV1[I]);
			VCol[VI] = D.Col[I];
			Inst.Add(VI);
		}
		for (const FIntVector3& T : D.Tris)
		{
			MD.CreateTriangle(PG, { Inst[T.X], Inst[T.Y], Inst[T.Z] });
		}
		if (!Mesh)
		{
			// never saved and never copied: a duplicate (Play In Editor copies the editor's world) would
			// have no mesh description to rebuild from and PIE would wait on it forever; the copy builds
			// its own meshes at BeginPlay
			// a fresh name every time: building over an existing object of the same name would free the render data a live component still draws
			Mesh = NewObject<UStaticMesh>(Outer, MakeUniqueObjectName(Outer, UStaticMesh::StaticClass(), Name), RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
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
	void BuildBlade(const FPlantLeafShape& L, float Length, FMeshData& Out, const FTransform3f& Xf, bool bSimple = false)
	{
		FMeshData B;
		const float W = L.Width * Length;
		if (L.Form == EPlantLeafForm::Palmate)
		{
			const int32 N = bSimple ? 10 : 48;
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
		else if (L.Form == EPlantLeafForm::Needle)
		{
			// a stiff, slightly curved spike: broad at the base, three triangles to the point
			const float HalfW = 0.5f * W;
			const float Y[3] = { 0.f, 0.45f * Length, Length }, Zs[3] = { 0.f, -0.02f * Length, -0.09f * Length }, Hw[3] = { HalfW, 0.7f * HalfW, 0.f };
			for (int32 I = 0; I < 3; I++)
			{
				const float T = Y[I] / Length;
				B.Add(FVector3f(-Hw[I], Y[I], Zs[I]), FVector3f::UpVector, FVector3f::XAxisVector, FVector2f(0.5f - Hw[I] / W, 1.f - T));
				if (I < 2) B.Add(FVector3f(Hw[I], Y[I], Zs[I]), FVector3f::UpVector, FVector3f::XAxisVector, FVector2f(0.5f + Hw[I] / W, 1.f - T));
			}
			B.Tris.Add(FIntVector3(0, 2, 1));
			B.Tris.Add(FIntVector3(1, 2, 3));
			B.Tris.Add(FIntVector3(2, 4, 3));
		}
		else
		{
			const bool bTeeth = L.Teeth > 0 && L.Teeth <= 8;
			// a far-away blade (bSimple) is a few triangles: just its outline's widest part
			const int32 N = bSimple ? 2 : (L.Lobes > 0 ? L.Lobes * 2 + 4 : (bTeeth ? L.Teeth * 4 : 8));
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
	float BarkHeight(const FPlantBark& B, uint32 Seed, float U, float V, float& OutFlake)
	{
		OutFlake = 0.f;
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
			// irregular scales, longer than wide, cracked apart; each a little cupped and lifting at its
			// edges, and some already flaked off, baring the smoother, warmer bark under them
			float Cell = 0.f;
			const float E = Voronoi(Seed + 6, U + Warp * 1.5f, V + Warp, 7, 7, B.TileHeight / B.TileWidth * 0.6f, Cell);
			const float Crack = Smooth(0.f, 0.06f, E);
			if (Cell < B.FlakeChance)
			{
				OutFlake = Crack;
				return FMath::Clamp(0.25f + 0.1f * Fine, 0.f, 1.f) * Crack;
			}
			const float Cup = 0.55f + 0.3f * Cell + 0.15f * (1.f - Smooth(0.06f, 0.3f, E));
			return FMath::Clamp(Cup * Crack + 0.18f * (Fine - 0.5f), 0.f, 1.f);
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

namespace
{
	/**
	 * Birch bark tile (U around, V along; tileTW x TH metres), painted from the tree's seed instead of
	 * sampled from noise: chalk-white peeling in thin horizontal strips, big black torn patches (some
	 * chevron-shaped), long black bands that run around the trunk, clusters of horizontal lenticel
	 * dashes. Black = 0..1 (how black), Hgt = relief 0 (cracked, sunk) .. 1 (raised), Tone = brightness of the white.
	 */
	void PaintBirchTile(const FPlantBark& B, uint32 Seed, int32 W, int32 H, TArray<float>& Hgt, TArray<float>& Black, TArray<float>& Tone)
	{
		// a small tile (dwarf birch) is a smaller bark, not a few huge marks: sizes below are for a 0.5 m tile
		const float Sc = FMath::Min(1.f, B.TileWidth / 0.5f), TW = B.TileWidth / Sc, TH = B.TileHeight / Sc;
		const float Mx = TW / W, My = TH / H; // metres per texel, at full size
		Black.SetNumZeroed(W * H);
		Hgt.SetNumUninitialized(W * H);
		Tone.SetNumUninitialized(W * H);
		auto At = [&](int32 X, int32 Y) { return ((Y % H + H) % H) * W + ((X % W) + W) % W; };

		// peeling strips: streaks stretched around the trunk, and thin papery curls whose edges catch the light
		for (int32 Y = 0; Y < H; Y++)
		{
			for (int32 X = 0; X < W; X++)
			{
				const float U = (X + 0.5f) / W, V = (Y + 0.5f) / H;
				const float Streak = 0.6f * Noise(Seed + 21, U, V, 3, 70) + 0.4f * Noise(Seed + 22, U, V, 6, 200);
				const float Blot = Noise(Seed + 23, U, V, 2, 4);
				const int32 I = At(X, Y);
				Tone[I] = 0.94f + 0.16f * (Streak - 0.5f) + 0.08f * (Blot - 0.5f);
				Hgt[I] = 0.72f + 0.14f * (Streak - 0.5f) + 0.06f * (Noise(Seed + 24, U, V, 24, 48) - 0.5f);
			}
		}
		const int32 Curls = FMath::RoundToInt(TW * TH * 70.f);
		for (int32 K = 0; K < Curls; K++)
		{
			const float Cx = Hash01(Seed, K, 0, 31) * W, Cy = Hash01(Seed, K, 0, 32) * H;
			const float Half = (0.04f + 0.14f * Hash01(Seed, K, 0, 33)) / Mx;
			const float Bend = (Hash01(Seed, K, 0, 34) - 0.5f) * 0.012f / My;
			for (int32 X = -FMath::CeilToInt(Half); X <= FMath::CeilToInt(Half); X++)
			{
				const float T = X / Half;
				const int32 Y = FMath::RoundToInt(Cy + Bend * T * T + X * 0.03f * (Hash01(Seed, K, 0, 35) - 0.5f));
				const float Fall = 1.f - Smooth(0.5f, 1.f, FMath::Abs(T));
				for (int32 Dy = 0; Dy < 2; Dy++)
				{
					const int32 I = At(FMath::RoundToInt(Cx) + X, Y + Dy);
					if (Dy == 0) { Tone[I] *= 1.f - 0.2f * Fall; Hgt[I] -= 0.2f * Fall; }
					else { Tone[I] = FMath::Min(Tone[I] * (1.f + 0.05f * Fall), 1.1f); Hgt[I] += 0.28f * Fall; }
				}
			}
		}

		// one black mark: a torn lens or chevron, Ax / Ay half sizes and Bend (arms droop) in texels
		auto Mark = [&](float Cx, float Cy, float Ax, float Ay, float Bend, float Rough, float Strength, uint32 Salt)
		{
			const int32 Ex = FMath::CeilToInt(Ax * 1.5f), Ey = FMath::CeilToInt(Ay * 1.6f + FMath::Abs(Bend) * 1.5f);
			for (int32 Y = -Ey; Y <= Ey; Y++)
			{
				for (int32 X = -Ex; X <= Ex; X++)
				{
					const float Dx = FMath::Abs((float)X) / Ax;
					const float Dy = (Y - Bend * Dx) / Ay;
					const int32 Px = FMath::RoundToInt(Cx) + X, Py = FMath::RoundToInt(Cy) + Y;
					const float U = (float)(((Px % W) + W) % W) / W, V = (float)(((Py % H) + H) % H) / H;
					// the edge is torn: noise squeezed along the trunk, and ragged rows
					const float Tear = (Noise(Seed + Salt, U, V, 24, 160) - 0.5f) * Rough + (Hash01(Seed, ((Py % H) + H) % H, Salt, 36) - 0.5f) * 0.25f * Rough;
					const float D = (Dx > 1.f ? Dx * Dx : Dx) + FMath::Abs(Dy) + Tear;
					const float V1 = Strength * (1.f - Smooth(0.82f, 1.06f, D));
					if (V1 <= 0.f) continue;
					const int32 I = At(Px, Py);
					Black[I] = FMath::Max(Black[I], V1);
				}
			}
		};
		// big patches: where a limb was, or the bark has split. About 14 per square metre, every kind different
		const int32 Patches = FMath::Max(2, FMath::RoundToInt(TW * TH * 14.f));
		for (int32 K = 0; K < Patches; K++)
		{
			const float Cx = Hash01(Seed, K, 0, 41) * W, Cy = Hash01(Seed, K, 0, 42) * H;
			const float Shape = Hash01(Seed, K, 0, 43);
			const float Size = 0.4f + 1.2f * Hash01(Seed, K, 0, 44) * Hash01(Seed, K, 0, 45);
			if (Shape < 0.2f)
			{
				// a chevron / diamond like the scar of a shed branch: wide, with arms that droop, one side often longer
				const float A = (0.03f + 0.05f * Size) / Mx, Hh = (0.012f + 0.016f * Size) / My;
				Mark(Cx, Cy, A, Hh, (0.8f + 1.2f * Hash01(Seed, K, 0, 46)) * Hh, 0.9f, 1.f, 50 + K);
			}
			else if (Shape < 0.6f)
			{
				// a torn lens, longer than tall
				Mark(Cx, Cy, (0.03f + 0.07f * Size) / Mx, (0.006f + 0.013f * Size) / My, 0.f, 0.9f, 1.f, 90 + K);
			}
			else if (Shape < 0.8f)
			{
				// a band that runs far around the trunk
				Mark(Cx, Cy, (0.08f + 0.15f * Size) / Mx, (0.003f + 0.006f * Size) / My, 0.f, 0.8f, 0.95f, 130 + K);
			}
			else
			{
				// a ragged blotch where the white has split away
				Mark(Cx, Cy, (0.035f + 0.045f * Size) / Mx, (0.02f + 0.025f * Size) / My, 0.f, 1.6f, 1.f, 150 + K);
			}
		}
		// lenticels: short black dashes in clusters (bands of them, bare bark between)
		const int32 Bands = FMath::Max(3, FMath::RoundToInt(TH * 7.f));
		const int32 Dashes = FMath::RoundToInt(TW * TH * 520.f);
		for (int32 K = 0; K < Dashes; K++)
		{
			const int32 Band = FMath::FloorToInt(Hash01(Seed, K, 0, 61) * Bands);
			const float BandY = (Band + 0.15f + 0.7f * Hash01(Seed, Band, 0, 62)) / Bands;
			// a band is dense somewhere around the trunk and thin elsewhere
			const float BandX = Hash01(Seed, Band, 0, 63);
			const float Spread = 0.12f + 0.4f * Hash01(Seed, Band, 0, 64);
			const float Around = BandX + (Hash01(Seed, K, 0, 65) - 0.5f) * 2.f * Spread;
			const float Cy = (BandY + (Hash01(Seed, K, 0, 66) - 0.5f) * 0.012f / TH * 4.f) * H;
			const float Len = 0.012f + 0.05f * FMath::Pow(Hash01(Seed, K, 0, 67), 1.6f);
			const float Thick = 0.0014f + 0.0026f * Hash01(Seed, K, 0, 68);
			Mark(Around * W, Cy, FMath::Max(Len / Mx, 1.5f), FMath::Max(Thick / My, 1.f), 0.f, 0.3f, 0.55f + 0.45f * Hash01(Seed, K, 0, 69), 170 + K % 7);
		}
		// relief: black marks are sunk and split
		for (int32 I = 0; I < W * H; I++)
		{
			const float Bk = Black[I];
			Hgt[I] = FMath::Clamp(FMath::Lerp(Hgt[I], 0.3f + 0.25f * Hgt[I], Bk), 0.f, 1.f);
		}
	}
}

namespace
{
	/** The trunk's and limbs' own patch of the birch-mark mask: where it sits and how many texels per metre. */
	struct FAtlasRect { int32 X = 0, Y = 0, W = 0, H = 0; };

	/** Adds Value (0..1, max-blended) to one texel of a tube's patch; X wraps around the tube. */
	void PaintTexel(TArray<uint8>& Mask, int32 Size, const FAtlasRect& R, int32 X, int32 Y, float Value)
	{
		if (Y < 0 || Y >= R.H || Value <= 0.f) return;
		X = ((X % R.W) + R.W) % R.W;
		uint8& Px = Mask[(R.Y + Y) * Size + R.X + X];
		Px = FMath::Max(Px, (uint8)FMath::RoundToInt(FMath::Clamp(Value, 0.f, 1.f) * 255.f));
	}
}

static UStaticMesh* BuildWoodImpl(UObject* Outer, UStaticMesh* Mesh, const TArray<FPlantSkeleton::FTube>& Tubes, const FPlantBark& Bark, float Age, uint32 Seed, UTexture2D** OutScarMask, FMeshData* OutData)
{
	FMeshData D;
	const float DarkBase = FMath::Min(Bark.DarkBaseRate * Age, Bark.DarkBaseMax);

	// --- the mark mask's atlas: one patch per trunk and limb, sized by its bark area, shelf-packed
	constexpr int32 MaskSize = 1024;
	const bool bMarks = Bark.Scars > 0.f && OutScarMask;
	TArray<FAtlasRect> Rects;
	Rects.SetNum(Tubes.Num());
	TArray<float> Lengths;
	Lengths.SetNumZeroed(Tubes.Num());
	for (int32 Ti = 0; Ti < Tubes.Num(); Ti++)
	{
		for (int32 I = 1; I < Tubes[Ti].P.Num(); I++) Lengths[Ti] += FVector3f::Dist(Tubes[Ti].P[I], Tubes[Ti].P[I - 1]);
	}
	if (bMarks)
	{
		TArray<int32> Order;
		double Area = 0.0;
		float LongestLen = 1.f;
		for (int32 Ti = 0; Ti < Tubes.Num(); Ti++)
		{
			if (Tubes[Ti].Order <= 1 && Tubes[Ti].P.Num() > 1 && Tubes[Ti].R[0] > 0.01f)
			{
				Order.Add(Ti);
				Area += 2.0 * PI * Tubes[Ti].R[0] * Lengths[Ti];
				LongestLen = FMath::Max(LongestLen, Lengths[Ti]);
			}
		}
		Order.Sort([&](int32 A, int32 B) { return Lengths[A] > Lengths[B]; });
		float Density = FMath::Min3(120.f, (float)FMath::Sqrt(0.7 * MaskSize * MaskSize / FMath::Max(Area, 1e-3)), (MaskSize - 8) / LongestLen);
		for (int32 Try = 0; Try < 8; Try++, Density *= 0.85f)
		{
			bool bFits = true;
			int32 X = 2, Y = 2, RowH = 0;
			for (const int32 Ti : Order)
			{
				FAtlasRect& R = Rects[Ti];
				R.W = FMath::Max(4, FMath::CeilToInt(2.f * PI * Tubes[Ti].R[0] * Density));
				R.H = FMath::Max(4, FMath::CeilToInt(Lengths[Ti] * Density));
				if (X + R.W + 2 > MaskSize) { X = 2; Y += RowH + 2; RowH = 0; }
				if (Y + R.H + 2 > MaskSize || R.W + 4 > MaskSize) { bFits = false; break; }
				R.X = X; R.Y = Y;
				X += R.W + 2;
				RowH = FMath::Max(RowH, R.H);
			}
			if (bFits) break;
		}
	}
	TArray<uint8> Mask;
	if (bMarks) Mask.SetNumZeroed(MaskSize * MaskSize);

	for (int32 Ti = 0; Ti < Tubes.Num(); Ti++)
	{
		const FPlantSkeleton::FTube& T = Tubes[Ti];
		const int32 N = T.P.Num();
		if (N < 2) continue;
		const float R0 = T.R[0];
		const int32 Sides = R0 > 0.12f ? 14 : R0 > 0.04f ? 10 : R0 > 0.012f ? 6 : R0 > 0.005f ? 4 : 3;
		// the bark tiles a whole number of times around, scaled down on thin twigs; each branch starts the
		// tile somewhere else, so neighbours never line up
		const float Circ = 2.f * PI * R0;
		const int32 Repeats = FMath::Max(1, FMath::RoundToInt(Circ / Bark.TileWidth));
		const float K = FMath::Max(Circ / (Repeats * Bark.TileWidth), 0.08f);
		const float U0 = PlantModel::Hash01(Seed, T.Key, 0, 61), V0 = PlantModel::Hash01(Seed, T.Key, 0, 62) * 13.f;
		const FAtlasRect& Rect = Rects[Ti];
		const bool bInAtlas = bMarks && Rect.W > 0;
		const float Len = FMath::Max(Lengths[Ti], 1e-3f);
		// ring frames by parallel transport: they turn with the branch but never twist
		TArray<FVector3f> Dirs, Nfs, Bfs;
		TArray<float> Arc;
		FVector3f Dir = (T.P[1] - T.P[0]).GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector);
		FVector3f Nf = AnyPerp(Dir);
		float Sum = 0.f;
		for (int32 I = 0; I < N; I++)
		{
			if (I > 0)
			{
				Sum += FVector3f::Dist(T.P[I], T.P[I - 1]);
				Dir = (T.P[FMath::Min(I + 1, N - 1)] - T.P[I - 1]).GetSafeNormal(UE_SMALL_NUMBER, Dir);
				Nf = (Nf - Dir * FVector3f::DotProduct(Nf, Dir)).GetSafeNormal(UE_SMALL_NUMBER, AnyPerp(Dir));
			}
			Dirs.Add(Dir); Nfs.Add(Nf); Bfs.Add(FVector3f::CrossProduct(Dir, Nf)); Arc.Add(Sum);
		}
		const int32 Base = D.Pos.Num();
		for (int32 I = 0; I < N; I++)
		{
			const float V = V0 + Arc[I] / (Bark.TileHeight * K);
			// twigs swing in the wind, more toward their tips (M_PlantBark's world offset)
			const float Sway = T.Order == 2 ? FMath::Pow(Arc[I] / Len, 1.3f) : 0.f;
			for (int32 S = 0; S <= Sides; S++)
			{
				const float A = 2.f * PI * S / Sides;
				const FVector3f Nrm = Nfs[I] * FMath::Cos(A) + Bfs[I] * FMath::Sin(A);
				const FVector3f Tan = Bfs[I] * FMath::Cos(A) - Nfs[I] * FMath::Sin(A);
				// the dark rough base climbs the trunk unevenly, in tongues; thin twigs keep their young bark
				float Dark = 0.f;
				if (DarkBase > 0.f && T.Order <= 1)
				{
					// black tongues run up the trunk between white ones, with a ragged edge
					const float Edge = DarkBase * (0.35f + 1.1f * Noise(17 + T.Key, (float)S / Sides, T.P[I].Z * 0.15f, 7, 1) * Noise(18 + T.Key, (float)S / Sides, 0.5f, Sides, 1));
					Dark = FMath::Clamp((Edge - T.P[I].Z) / 0.09f, 0.f, 1.f);
				}
				const float Twig = Bark.TwigRadius > 0.f ? FMath::Clamp((Bark.TwigRadius - T.R[I]) / (0.5f * Bark.TwigRadius), 0.f, 1.f) : 0.f;
				const int32 Vi = D.Add((T.P[I] + Nrm * T.R[I]) * 100.f, Nrm, Tan, FVector2f(U0 + (float)S / Sides * Repeats, -V), FVector4f(Dark, Twig, Sway, 1.f));
				if (bInAtlas)
				{
					D.UV1[Vi] = FVector2f((Rect.X + (float)S / Sides * Rect.W) / MaskSize, (Rect.Y + Arc[I] / Len * Rect.H) / MaskSize);
				}
				else if (bMarks)
				{
					D.UV1[Vi] = FVector2f(0.5f / MaskSize, 0.5f / MaskSize); // an unmarked texel
				}
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
		if (!bInAtlas) continue;

		// --- this tube's marks, in metres along it and around it, so they stay put as it grows
		const float PerM_Y = Rect.H / Len;
		auto RingAt = [&](float At) { int32 I = 0; while (I + 1 < N && Arc[I + 1] <= At) I++; return I; };
		auto PerM_X = [&](float At) { return Rect.W / (2.f * PI * FMath::Max(T.R[RingAt(At)], 0.005f)); };
		// black chevrons over every branch (and the eyes of shed ones): a Λ whose arms run down either side
		for (int32 Jk = 0; Jk < T.Junctions.Num(); Jk++)
		{
			const FPlantSkeleton::FTube::FJunction& J = T.Junctions[Jk];
			if (J.R < 0.006f || J.S > Len) continue;
			const int32 Ri = RingAt(J.S);
			const FVector3f Dp = J.Dir - Dirs[Ri] * FVector3f::DotProduct(J.Dir, Dirs[Ri]);
			const float Theta = FMath::Atan2(FVector3f::DotProduct(Dp, Bfs[Ri]), FVector3f::DotProduct(Dp, Nfs[Ri]));
			const float Strength = Bark.Scars * (0.8f + 0.2f * PlantModel::Hash01(Seed, T.Key, Jk, 63));
			const float Arms = 0.7f + 0.6f * PlantModel::Hash01(Seed, T.Key, Jk, 64);
			const float Rc = J.R * (J.bShed ? 1.6f : 2.2f);
			const float Cx = (Theta < 0.f ? Theta + 2.f * PI : Theta) / (2.f * PI) * Rect.W, Cy = J.S * PerM_Y;
			const float Mx = PerM_X(J.S);
			const int32 Ex = FMath::CeilToInt(3.6f * Arms * Rc * Mx), EyLo = FMath::CeilToInt(1.6f * Rc * PerM_Y), EyHi = FMath::CeilToInt(2.2f * Rc * PerM_Y);
			for (int32 Y = -EyLo; Y <= EyHi; Y++)
			{
				for (int32 X = -FMath::Min(Ex, Rect.W / 2); X <= FMath::Min(Ex, Rect.W / 2); X++)
				{
					const float Lx = X / (Mx * Rc), Ly = Y / (PerM_Y * Rc);
					const float Ax = FMath::Abs(Lx) / Arms;
					const float Crest = 1.3f - 0.65f * Ax;
					// a bold black brow, thickest over the branch, filled in underneath toward the branch
					const float Band = 0.55f - 0.08f * Ax;
					float Val = (1.f - Smooth(Band * 0.55f, Band, FMath::Abs(Ly - Crest))) * (1.f - Smooth(2.6f, 3.4f, Ax));
					if (Ly < Crest && Ly > Crest - 1.1f && Ax < 1.6f) Val = FMath::Max(Val, 0.9f * (1.f - Smooth(1.f, 1.6f, Ax)));
					if (J.bShed && FMath::Square(Lx / 1.6f) + FMath::Square(Ly / 0.9f) < 1.f) Val = 1.f;
					PaintTexel(Mask, MaskSize, Rect, FMath::RoundToInt(Cx) + X, FMath::RoundToInt(Cy) + Y, Val * Strength);
				}
			}
		}
	}
	if (D.Tris.IsEmpty())
	{
		// an empty mesh can't be built: one degenerate triangle
		for (int32 I = 0; I < 3; I++) D.Add(FVector3f::ZeroVector, FVector3f::UpVector, FVector3f::XAxisVector, FVector2f::ZeroVector);
		D.Tris.Add(FIntVector3(0, 1, 2));
	}
	if (bMarks)
	{
		TArray<FColor> Px;
		Px.SetNumUninitialized(MaskSize * MaskSize);
		for (int32 I = 0; I < Mask.Num(); I++) Px[I] = FColor(Mask[I], Mask[I], Mask[I], 255);
		// PLANT_MASK_DUMP=<dir>: write each mark mask as a PGM there (for checking it without a renderer)
		const FString Dump = FPlatformMisc::GetEnvironmentVariable(TEXT("PLANT_MASK_DUMP"));
		if (!Dump.IsEmpty())
		{
			TArray<uint8> File;
			const FString Header = FString::Printf(TEXT("P5 %d %d 255\n"), MaskSize, MaskSize);
			File.Append((const uint8*)TCHAR_TO_ANSI(*Header), Header.Len());
			File.Append(Mask);
			FFileHelper::SaveArrayToFile(File, *FString::Printf(TEXT("%s/mask_%u.pgm"), *Dump, Seed));
		}
		*OutScarMask = MakeTexture(Outer, MaskSize, MaskSize, MoveTemp(Px), true, false);
	}
	if (OutData)
	{
		*OutData = MoveTemp(D);
		return nullptr;
	}
	return BuildMesh(Outer, Mesh, TEXT("SM_PlantWood"), D);
}

UStaticMesh* PlantMeshes::BuildWood(UObject* Outer, UStaticMesh* Mesh, const TArray<FPlantSkeleton::FTube>& Tubes, const FPlantBark& Bark, float Age, uint32 Seed, UTexture2D** OutScarMask)
{
	return BuildWoodImpl(Outer, Mesh, Tubes, Bark, Age, Seed, OutScarMask, nullptr);
}

static void LeafClusterData(const FPlantSpecies& S, FMeshData& D, bool bSimple = false)
{
	const FPlantLeafShape& L = S.Leaf;
	const int32 Count = FMath::Max(L.PerCluster, 1);
	if (L.Form == EPlantLeafForm::Needle)
	{
		// a spray of needles as a few crossed cards (a fan of needles painted on each, see BuildLeafDetail), up and
		// out from a short shoot along Z; shaded as one soft ball
		const int32 Cards = FMath::Clamp(L.Cards, 1, 8);
		for (int32 I = 0; I < Cards; I++)
		{
			const float Phi = I * GoldenAngle * 2.f + Hash01(77, I, 1) * 0.5f;
			const float Theta = FMath::DegreesToRadians(25.f + 70.f * Hash01(77, I, 2));
			const FVector3f Dir(FMath::Cos(Phi) * FMath::Sin(Theta), FMath::Sin(Phi) * FMath::Sin(Theta), FMath::Cos(Theta));
			const FVector3f Side = FVector3f::CrossProduct(Dir, FVector3f::UpVector).GetSafeNormal(UE_SMALL_NUMBER, FVector3f::XAxisVector);
			const FVector3f Root(0.f, 0.f, S.LeafClusterSize * 0.25f * (Hash01(77, I, 3) - 0.3f));
			const float Length = L.Length * (0.85f + 0.3f * Hash01(77, I, 4));
			const float Half = 0.5f * Length * L.Width;
			const FVector3f Tip = Root + Dir * Length;
			const int32 Base = D.Pos.Num();
			const FVector3f Pts[4] = { Root - Side * (Half * 0.12f), Root + Side * (Half * 0.12f), Tip + Side * Half, Tip - Side * Half };
			const FVector2f Uvs[4] = { FVector2f(0.44f, 1.f), FVector2f(0.56f, 1.f), FVector2f(1.f, 0.f), FVector2f(0.f, 0.f) };
			for (int32 V = 0; V < 4; V++)
			{
				D.Add(Pts[V] * 100.f, (Pts[V] - FVector3f(0.f, 0.f, 0.f)).GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector), Side, Uvs[V]);
			}
			D.Tris.Add(FIntVector3(Base, Base + 2, Base + 1));
			D.Tris.Add(FIntVector3(Base, Base + 3, Base + 2));
		}
		return;
	}
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
		BuildBlade(L, Length, D, FTransform3f(FRotator3f(Rot), Node + Stalk), bSimple);
	}
}

UStaticMesh* PlantMeshes::BuildLeafCluster(UObject* Outer, const FPlantSpecies& S)
{
	FMeshData D;
	LeafClusterData(S, D);
	return BuildMesh(Outer, nullptr, *FString::Printf(TEXT("SM_Leaves_%s"), *S.Name.ToString()), D);
}

void PlantMeshes::BuildBarkTextures(UObject* Outer, const FPlantSpecies& S, uint32 Seed, UTexture2D*& OutAlbedo, UTexture2D*& OutNormal)
{
	const FPlantBark& B = S.Bark;
	const bool bBirch = B.Pattern == EPlantBarkPattern::Birch;
	const int32 W = bBirch ? 512 : 256, H = bBirch ? 1024 : 512;
	TArray<float> Hgt, BirchBlack, BirchTone;
	Hgt.SetNumUninitialized(W * H);
	TArray<FColor> Albedo, Normal;
	Albedo.SetNumUninitialized(W * H);
	Normal.SetNumUninitialized(W * H);
	const uint32 TexSeed = Seed * 131u + GetTypeHash(S.Name);
	if (bBirch)
	{
		PaintBirchTile(B, TexSeed, W, H, Hgt, BirchBlack, BirchTone);
	}
	for (int32 Y = 0; Y < H; Y++)
	{
		for (int32 X = 0; X < W; X++)
		{
			const float U = (X + 0.5f) / W, V = (Y + 0.5f) / H;
			FLinearColor C;
			if (bBirch)
			{
				// chalk white (no mottling: the marks are all there is), black where a mark is
				C = FMath::Lerp(B.RidgeColour * BirchTone[Y * W + X], B.FurrowColour, FMath::Clamp(BirchBlack[Y * W + X] * 1.15f, 0.f, 1.f));
			}
			else
			{
				float Flake = 0.f;
				const float Hh = BarkHeight(B, TexSeed, U, V, Flake);
				Hgt[Y * W + X] = Hh;
				const float Mottle = 0.8f + 0.4f * Fbm(TexSeed + 20, U, V, 6, 10, 3);
				C = FMath::Lerp(FMath::Lerp(B.FurrowColour, B.RidgeColour, Hh), B.UnderColour, Flake) * Mottle;
			}
			// alpha: the deep fissures of an old, dark trunk base (where the vertex colour asks for it)
			float Unused = 0.f;
			FPlantBark Rough = B;
			Rough.Pattern = EPlantBarkPattern::Blocks;
			// a birch tile is two to four times the size of the other barks': its fissures run at their own scale
			const float Fissure = bBirch ? BarkHeight(Rough, TexSeed + 77, FMath::Frac(2.f * U), FMath::Frac(3.f * V), Unused) : BarkHeight(Rough, TexSeed + 77, U, V, Unused);
			FColor Px = FLinearColor(C.R, C.G, C.B, 1.f).ToFColor(true);
			Px.A = (uint8)FMath::RoundToInt(Fissure * 255.f);
			Albedo[Y * W + X] = Px;
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

UTexture2D* PlantMeshes::BuildFoliageCard(UObject* Outer, const FPlantLeafShape& L)
{
	if (L.Form == EPlantLeafForm::Needle)
	{
		return BuildLeafDetail(Outer, L);
	}
	// a spray of leaves seen from the side: alpha 255 on a leaf, a grey shade, the shapes of this species' blades
	constexpr int32 W = 128;
	TArray<FColor> Img;
	Img.Init(FColor(0, 0, 0, 0), W * W);
	const int32 Leaves = 70;
	for (int32 N = 0; N < Leaves; N++)
	{
		const float Cx = (0.12f + 0.76f * Hash01(41, N, 1)) * W, Cy = (0.1f + 0.85f * Hash01(41, N, 2)) * W;
		const float Ang = (Hash01(41, N, 3) - 0.5f) * 2.4f + (Cx / W - 0.5f) * 1.2f;
		const float Len = (0.16f + 0.1f * Hash01(41, N, 4)) * W, Wid = Len * FMath::Clamp(L.Width, 0.35f, 1.1f) * 0.5f;
		const float Shade = 0.7f + 0.3f * Hash01(41, N, 5);
		const float Ca = FMath::Cos(Ang), Sa = FMath::Sin(Ang);
		for (int32 Y = -(int32)Len; Y <= (int32)Len; Y++)
		{
			for (int32 X = -(int32)Len; X <= (int32)Len; X++)
			{
				// leaf space: along the leaf (u from the base) and across it
				const float U = (X * Sa - Y * Ca) / Len + 0.5f, Vv = (X * Ca + Y * Sa) / Wid;
				if (U < 0.f || U > 1.f) continue;
				const float Half = FMath::Pow(FMath::Sin(PI * FMath::Clamp(U, 0.f, 1.f)), 0.8f);
				if (FMath::Abs(Vv) > Half) continue;
				const int32 Ix = FMath::RoundToInt(Cx) + X, Iy = FMath::RoundToInt(Cy) + Y;
				if (Ix < 0 || Ix >= W || Iy < 0 || Iy >= W) continue;
				const uint8 C = (uint8)FMath::RoundToInt(255.f * Shade * (0.75f + 0.25f * U));
				Img[Iy * W + Ix] = FColor(C, C, C, 255);
			}
		}
	}
	return MakeTexture(Outer, W, W, MoveTemp(Img), true, false);
}

UTexture2D* PlantMeshes::BuildLeafDetail(UObject* Outer, const FPlantLeafShape& L)
{
	if (L.Form == EPlantLeafForm::Needle)
	{
		// a fan of needles from the bottom middle, alpha 255 on a needle: M_PlantNeedle cuts the card out by it
		constexpr int32 NW = 128, NH = 128;
		TArray<FColor> Img;
		Img.Init(FColor(0, 0, 0, 0), NW * NH);
		const int32 Needles = FMath::Clamp(L.PerCluster * 2, 12, 120);
		const int32 Group = FMath::Max(L.Lobes, 1);
		for (int32 N = 0; N < Needles; N++)
		{
			const int32 G = N / Group, M = N % Group;
			const float A = ((Hash01(31, G, 1) - 0.5f) * 2.f) * 0.95f + M * 0.07f;
			const float Len = (0.55f + 0.45f * Hash01(31, G, 2)) * NH * 0.97f;
			const float Bend = (Hash01(31, G, 3) - 0.5f) * 0.5f;
			const float Shade = 0.8f + 0.2f * Hash01(31, G, 4);
			const int32 Steps = FMath::CeilToInt(Len * 1.5f);
			for (int32 K = 0; K <= Steps; K++)
			{
				const float T = (float)K / Steps;
				const float Ang = A + Bend * T * T;
				const float X = NW * 0.5f + FMath::Sin(Ang) * Len * T, Y = NH - 1.f - FMath::Cos(Ang) * Len * T;
				const float Thick = 1.4f * (1.f - 0.7f * T);
				const uint8 V = (uint8)FMath::RoundToInt(255.f * Shade * (0.7f + 0.3f * T));
				for (int32 Dy = -1; Dy <= 1; Dy++)
				{
					for (int32 Dx = -1; Dx <= 1; Dx++)
					{
						if (Dx * Dx + Dy * Dy > Thick * Thick) continue;
						const int32 Ix = FMath::RoundToInt(X) + Dx, Iy = FMath::RoundToInt(Y) + Dy;
						if (Ix < 0 || Ix >= NW || Iy < 0 || Iy >= NH) continue;
						Img[Iy * NW + Ix] = FColor(V, V, V, 255);
					}
				}
			}
		}
		return MakeTexture(Outer, NW, NH, MoveTemp(Img), true, false);
	}
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

UStaticMesh* PlantMeshes::BuildSingleLeaf(UObject* Outer, const FPlantSpecies& S)
{
	FMeshData D;
	FPlantLeafShape L = S.Leaf;
	L.Fold *= 0.5f;
	// centred on the blade, so it spins about its middle as it falls
	BuildBlade(L, L.Length, D, FTransform3f(FVector3f(0.f, -0.5f * L.Length, 0.f)));
	return BuildMesh(Outer, nullptr, *FString::Printf(TEXT("SM_Leaf_%s"), *S.Name.ToString()), D);
}

UStaticMesh* PlantMeshes::BuildFruit(UObject* Outer, const FPlantSpecies& S)
{
	// an apple: 100 units across (scaled like the engine sphere), broad shoulders, a dimple at the
	// stalk and at the eye, a short stalk; UV u around, v from the stalk (0) to the eye (1); the
	// stalk is at v < 0 (the material browns it)
	FMeshData D;
	constexpr int32 Around = 20, Down = 14;
	const bool bApple = S.FruitBlush > 0.f;
	auto Radius = [&](float Th) { return 50.f * FMath::Sin(Th) * (bApple ? 1.f - 0.08f * FMath::Cos(Th) : 1.f); };
	auto Height = [&](float Th)
	{
		float Z = 50.f * FMath::Cos(Th) * (bApple ? 0.88f : 1.15f);
		if (bApple)
		{
			Z -= 9.f * FMath::Exp(-FMath::Square(Th / 0.32f));
			Z += 6.f * FMath::Exp(-FMath::Square((PI - Th) / 0.3f));
		}
		return Z;
	};
	for (int32 J = 0; J <= Down; J++)
	{
		const float Th = PI * J / Down;
		for (int32 I = 0; I <= Around; I++)
		{
			const float A = 2.f * PI * I / Around;
			const FVector3f P(FMath::Cos(A) * Radius(Th), FMath::Sin(A) * Radius(Th), Height(Th));
			D.Add(P, P.GetSafeNormal(), FVector3f(-FMath::Sin(A), FMath::Cos(A), 0.f), FVector2f((float)I / Around, (float)J / Down));
		}
	}
	for (int32 J = 0; J < Down; J++)
	{
		for (int32 I = 0; I < Around; I++)
		{
			const int32 A = J * (Around + 1) + I, B = A + 1, C = A + Around + 1, E = C + 1;
			D.Tris.Add(FIntVector3(A, B, C));
			D.Tris.Add(FIntVector3(B, E, C));
		}
	}
	// smooth normals from the faces, so the dimples shade
	TArray<FVector3f> N;
	N.SetNumZeroed(D.Pos.Num());
	for (FIntVector3& T : D.Tris)
	{
		FVector3f Fn = FVector3f::CrossProduct(D.Pos[T.Z] - D.Pos[T.X], D.Pos[T.Y] - D.Pos[T.X]);
		const FVector3f Mid = (D.Pos[T.X] + D.Pos[T.Y] + D.Pos[T.Z]) / 3.f;
		if (FVector3f::DotProduct(Fn, Mid) < 0.f) { Swap(T.Y, T.Z); Fn = -Fn; }
		N[T.X] += Fn; N[T.Y] += Fn; N[T.Z] += Fn;
	}
	for (int32 I = 0; I < N.Num(); I++) D.Nrm[I] = N[I].GetSafeNormal(UE_SMALL_NUMBER, D.Nrm[I]);
	// the stalk: a thin, slightly bent stick out of the top dimple
	const float Top = Height(0.f);
	const int32 Base = D.Pos.Num();
	for (int32 K = 0; K <= 1; K++)
	{
		for (int32 I = 0; I < 5; I++)
		{
			const float A = 2.f * PI * I / 5.f;
			const FVector3f Nr(FMath::Cos(A), FMath::Sin(A), 0.f);
			const FVector3f C = K == 0 ? FVector3f(0.f, 0.f, Top - 2.f) : FVector3f(5.f, 0.f, Top + (bApple ? 24.f : 8.f));
			D.Add(C + Nr * 1.6f, Nr, FVector3f::UpVector, FVector2f(0.f, -0.5f));
		}
	}
	for (int32 I = 0; I < 5; I++)
	{
		const int32 A = Base + I, B = Base + (I + 1) % 5, C = A + 5, E = B + 5;
		D.Tris.Add(FIntVector3(A, C, B));
		D.Tris.Add(FIntVector3(B, C, E));
	}
	return BuildMesh(Outer, nullptr, *FString::Printf(TEXT("SM_Fruit_%s"), *S.Name.ToString()), D);
}

UTexture2D* PlantMeshes::BuildFruitDetail(UObject* Outer)
{
	// R: soft noise that breaks up the blush and its stripes; G: lenticels, the pale dots on an apple's skin
	// (sRGB like every other plant texture, so the material samples it as a plain colour)
	constexpr int32 W = 128;
	TArray<FColor> Px;
	Px.SetNumUninitialized(W * W);
	for (int32 Y = 0; Y < W; Y++)
	{
		for (int32 X = 0; X < W; X++)
		{
			const float U = (X + 0.5f) / W, V = (Y + 0.5f) / W;
			const float N = Fbm(91, U, V, 4, 4, 4);
			const float Dx = FMath::Frac(U * 24.f) - 0.5f, Dy = FMath::Frac(V * 24.f) - 0.5f;
			const bool bCell = Hash01(92, FMath::FloorToInt(U * 24.f), FMath::FloorToInt(V * 24.f)) < 0.35f;
			const float Dot = bCell ? FMath::Clamp(1.f - FMath::Sqrt(Dx * Dx + Dy * Dy) / 0.18f, 0.f, 1.f) : 0.f;
			Px[Y * W + X] = FColor((uint8)FMath::RoundToInt(N * 255.f), (uint8)FMath::RoundToInt(Dot * 255.f), 0, 255);
		}
	}
	return MakeTexture(Outer, W, W, MoveTemp(Px), true, false);
}

UStaticMesh* PlantMeshes::BuildGrassTuft(UObject* Outer, const FGrassSpecies& G)
{
	FMeshData D;
	constexpr int32 Rows = 5;
	const int32 Count = FMath::Clamp(G.Blades, 3, 40);
	for (int32 B = 0; B < Count; B++)
	{
		// where it stands in the tuft, which way it leans, how long, how far it arches, how it twists
		const float Around = B * GoldenAngle;
		const float R0 = G.TuftRadius * FMath::Sqrt((B + 0.5f) / Count) * (0.6f + 0.4f * Hash01(5, B, 1));
		const float Heading = Around + (Hash01(5, B, 2) - 0.5f) * 1.4f;
		const float Len = G.Height * (0.55f + 0.45f * Hash01(5, B, 3));
		const float Lean0 = FMath::DegreesToRadians(G.Lean * (0.4f + 0.9f * Hash01(5, B, 4)));
		const float Arch = FMath::DegreesToRadians(G.Arch * (0.4f + 0.8f * Hash01(5, B, 5)));
		const float Width = G.BladeWidth * (0.8f + 0.4f * Hash01(5, B, 6));
		const float Twist = (Hash01(5, B, 7) - 0.5f) * 1.6f;
		const float Shade = Hash01(5, B, 8);
		const FVector3f Out(FMath::Cos(Heading), FMath::Sin(Heading), 0.f);
		FVector3f P = FVector3f(FMath::Cos(Around) * R0, FMath::Sin(Around) * R0, 0.f);
		const int32 Base = D.Pos.Num();
		FVector3f Face = FVector3f::ZeroVector;
		for (int32 K = 0; K < Rows; K++)
		{
			const float T = (float)K / (Rows - 1);
			const float A = Lean0 + Arch * FMath::Pow(T, 1.5f);
			const FVector3f Dir(Out.X * FMath::Sin(A), Out.Y * FMath::Sin(A), FMath::Cos(A));
			// across the blade: horizontal and square to its heading, twisting a little along its length
			const FVector3f Flat(-Out.Y, Out.X, 0.f);
			const FVector3f Normal0 = FVector3f::CrossProduct(Flat, Dir).GetSafeNormal();
			const float Tw = Twist * T;
			const FVector3f Side = Flat * FMath::Cos(Tw) + Normal0 * FMath::Sin(Tw);
			const float Half = 0.5f * Width * (K == Rows - 1 ? 0.f : 1.f - FMath::Pow(T, 1.4f));
			Face = FVector3f::CrossProduct(Side, Dir).GetSafeNormal();
			if (FVector3f::DotProduct(Face, Out) < 0.f) Face = -Face;
			// shaded as a soft, upward-looking surface rather than a hard flat strip
			const FVector3f Nrm = (Face + FVector3f::UpVector * 0.8f).GetSafeNormal();
			const FVector4f Col(T, Shade, 0.f, 1.f);
			if (K == Rows - 1)
			{
				D.Add(P * 100.f, Nrm, Side, FVector2f(0.5f, 1.f - T), Col);
			}
			else
			{
				D.Add((P - Side * Half) * 100.f, Nrm, Side, FVector2f(0.5f - Half / Width, 1.f - T), Col);
				D.Add((P + Side * Half) * 100.f, Nrm, Side, FVector2f(0.5f + Half / Width, 1.f - T), Col);
			}
			const FVector3f Next = Dir; // the step to the next row follows this row's direction
			P += Next * (Len / (Rows - 1));
		}
		// strip of quads from the root, then one triangle to the tip; wound to face along Face (either side draws)
		auto Tri = [&](int32 A, int32 B2, int32 C2)
		{
			const FVector3f N = FVector3f::CrossProduct(D.Pos[C2] - D.Pos[A], D.Pos[B2] - D.Pos[A]);
			if (FVector3f::DotProduct(N, D.Nrm[A]) >= 0.f) D.Tris.Add(FIntVector3(A, B2, C2));
			else D.Tris.Add(FIntVector3(A, C2, B2));
		};
		for (int32 K = 0; K + 2 < Rows; K++)
		{
			const int32 L0 = Base + 2 * K, R1 = L0 + 1, L1 = L0 + 2, R2 = L0 + 3;
			Tri(L0, R1, L1);
			Tri(R1, R2, L1);
		}
		const int32 Last = Base + 2 * (Rows - 2);
		Tri(Last, Last + 1, Last + 2);
	}
	return BuildMesh(Outer, nullptr, *FString::Printf(TEXT("SM_Grass_%s"), *G.Name.ToString()), D);
}

namespace
{
	/** The data of one LOD as two sections, wood and leaves, in a mesh description. */
	void FillTreeLod(FMeshDescription& MD, const FMeshData& Wood, const FMeshData& Leaf, const FMeshData& Card)
	{
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> VTan = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> VSign = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> VUV = Attr.GetVertexInstanceUVs();
		TVertexInstanceAttributesRef<FVector4f> VCol = Attr.GetVertexInstanceColors();
		VUV.SetNumChannels(2);
		const FMeshData* Parts[3] = { &Wood, &Leaf, &Card };
		const TCHAR* Slots[3] = { TEXT("Wood"), TEXT("Leaf"), TEXT("Card") };
		for (int32 Part = 0; Part < 3; Part++)
		{
			const FMeshData& D = *Parts[Part];
			const FPolygonGroupID PG = MD.CreatePolygonGroup();
			Attr.GetPolygonGroupMaterialSlotNames()[PG] = Slots[Part];
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
				VUV.Set(VI, 1, D.UV1[I]);
				VCol[VI] = D.Col[I];
				Inst.Add(VI);
			}
			for (const FIntVector3& T : D.Tris)
			{
				MD.CreateTriangle(PG, { Inst[T.X], Inst[T.Y], Inst[T.Z] });
			}
		}
	}
}

PlantMeshes::FTreePrep PlantMeshes::PrepareTreeBake(const FPlantSpecies& S, uint32 Seed, float Age, float Day)
{
	const double T0 = FPlatformTime::Seconds();
	FTreePrep Result;
	FPlantSkeleton Skeleton;
	Skeleton.Build(S, Seed);
	FPlantSkeleton::FInstances Out;
	Skeleton.Evaluate(Age, PlantModel::Season(S, Seed, Day), Out);
	FMeshData Cluster, Simple, CardCluster;
	LeafClusterData(S, Cluster);
	LeafClusterData(S, Simple, true);
	{
		FPlantSpecies SC = S;
		SC.Leaf.Form = EPlantLeafForm::Needle;
		SC.Leaf.Cards = 3;
		SC.Leaf.Width = 0.9f;
		SC.Leaf.Length = S.Leaf.Form == EPlantLeafForm::Needle ? S.Leaf.Length : FMath::Max(S.LeafClusterSize * 1.1f, 0.3f);
		LeafClusterData(SC, CardCluster);
	}

	struct FLod { int32 MaxOrder; int32 Stride; float Scale; };
	// needle clusters are already 6 triangles; a blade cluster is cut to a fifth in every LOD (simple blades) and thinned out.
	// From LOD2 the leaves are cards (the "Card" section): three crossed quads per cluster, each with a spray of real leaves or needles painted in its alpha, so the silhouette stays a tree's
	const bool bNeedle = S.Leaf.Form == EPlantLeafForm::Needle;
	const FLod Lods[4] = { { 3, bNeedle ? 1 : 2, bNeedle ? 1.f : 1.25f }, { 1, bNeedle ? 3 : 6, bNeedle ? 1.7f : 2.2f }, { 1, 6, 2.f }, { 1, 20, 3.6f } };
	TArray<FMeshDescription>& Descriptions = Result.Lods;
	Descriptions.SetNum(4);
	for (int32 L = 0; L < 4; L++)
	{
		TArray<FPlantSkeleton::FTube> Tubes;
		for (const FPlantSkeleton::FTube& T : Out.Tubes)
		{
			if (T.Order <= Lods[L].MaxOrder) Tubes.Add(T);
		}
		FMeshData Wood, Leaf, Card;
		if (!Tubes.IsEmpty())
		{
			BuildWoodImpl(nullptr, nullptr, Tubes, S.Bark, Age, Seed, nullptr, &Wood);
		}
		int32 Count = 0;
		for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
		{
			for (const FTransform& Xf0 : Out.Leaves[K])
			{
				if (Count++ % Lods[L].Stride != 0) continue;
				FTransform Xf = Xf0;
				Xf.SetScale3D(Xf.GetScale3D() * Lods[L].Scale);
				FMeshData& Dst = L >= 2 ? Card : Leaf;
				const int32 Base = Dst.Pos.Num();
				const FMeshData& Src = L >= 2 ? CardCluster : (bNeedle ? Cluster : Simple);
				for (int32 V = 0; V < Src.Pos.Num(); V++)
				{
					Dst.Add(FVector3f(Xf.TransformPosition(FVector(Src.Pos[V]))), FVector3f(Xf.TransformVectorNoScale(FVector(Src.Nrm[V]))),
					         FVector3f(Xf.TransformVectorNoScale(FVector(Src.Tan[V]))), Src.UV[V]);
				}
				for (const FIntVector3& T : Src.Tris) Dst.Tris.Add(FIntVector3(Base + T.X, Base + T.Y, Base + T.Z));
			}
		}
		if (Wood.Tris.IsEmpty() && Leaf.Tris.IsEmpty() && Card.Tris.IsEmpty())
		{
			for (int32 I = 0; I < 3; I++) Wood.Add(FVector3f::ZeroVector, FVector3f::UpVector, FVector3f::XAxisVector, FVector2f::ZeroVector);
			Wood.Tris.Add(FIntVector3(0, 1, 2));
		}
		Result.Triangles[L] = Wood.Tris.Num() + Leaf.Tris.Num() + Card.Tris.Num();
		FillTreeLod(Descriptions[L], Wood, Leaf, Card);
	}
	Result.Radius = FMath::Sqrt(FMath::Square(0.5f * Out.Height) + FMath::Square(Out.Reach));
	Result.Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
	return Result;
}

PlantMeshes::FTreeBake PlantMeshes::FinishTreeBake(UObject* Outer, const FPlantSpecies& S, uint32 Seed, FTreePrep&& Prep, const FVector3f& LodDistances)
{
	FTreeBake Result;
	for (int32 L = 0; L < 4; L++) Result.Triangles[L] = Prep.Triangles[L];
	// the same rules as every runtime-built mesh here: never saved, never copied into a PIE world
	UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, MakeUniqueObjectName(Outer, UStaticMesh::StaticClass(), *FString::Printf(TEXT("SM_Tree_%s_%u"), *S.Name.ToString(), Seed)), RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
	Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Wood")));
	Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Leaf")));
	Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Card")));
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bMarkPackageDirty = false;
	Params.bBuildSimpleCollision = false;
	Params.bCommitMeshDescription = false;
	Params.bFastBuild = true;
	Mesh->BuildFromMeshDescriptions({ &Prep.Lods[0], &Prep.Lods[1], &Prep.Lods[2], &Prep.Lods[3] }, Params);
	Result.Mesh = Mesh;
	Result.Radius = Prep.Radius;
	// the two farthest LODs cast no shadow (the shadow of a tree 60 m away is a few pixels of a shadow map page)
	if (FStaticMeshRenderData* Data = Mesh->GetRenderData())
	{
		for (int32 L = 2; L < Data->LODResources.Num(); L++)
		{
			for (FStaticMeshSection& Section : Data->LODResources[L].Sections) Section.bCastShadow = false;
		}
	}
	// LOD switch by camera distance: a sphere of radius R fills about R / d of a 90 degree view
	if (FStaticMeshRenderData* RD = Mesh->GetRenderData())
	{
		const float R = FMath::Max(Result.Radius, 0.3f);
		RD->ScreenSize[0] = 1.f;
		RD->ScreenSize[1] = FMath::Min(R / LodDistances.X, 0.9f);
		RD->ScreenSize[2] = FMath::Min(R / LodDistances.Y, 0.5f);
		RD->ScreenSize[3] = FMath::Min(R / LodDistances.Z, 0.25f);
	}
	return Result;
}

PlantMeshes::FTreeBake PlantMeshes::BuildTreeBake(UObject* Outer, const FPlantSpecies& S, uint32 Seed, float Age, float Day, const FVector3f& LodDistances)
{
	return FinishTreeBake(Outer, S, Seed, PrepareTreeBake(S, Seed, Age, Day), LodDistances);
}
