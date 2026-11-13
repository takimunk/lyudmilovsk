// tools/plantsim/run.sh render: a picture of a species without Unreal, to look at its shape, its colours and its season.
// Trees and shrubs from the skeleton (PlantSkeleton::Evaluate): the wood as tubes, the leaf clusters as
// PlantMeshes::LeafClusterData lays them out (simple pinnate, palmate or needle blades), blossom, catkins, cones and fruit
// as small bodies; herbs from PlantHerbs::Build (their real mesh: leaves, stems, flowers in their atlas colours). Flat
// shaded under a sun and the sky, no shadows and no textures, on a gravel disc with a 2 m pole (red and white every 0.5 m).
// Panels side by side at one scale, so ages and seasons compare. Written as an uncompressed PNG (no libraries).
//   run.sh render <out.png> <Name>:<age>:<day>[:<seed>] ...
#include "Plants/PlantHerb.h"
#include "Plants/PlantModel.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
	struct FTri
	{
		FVector3f A, B, C;
		FLinearColor Col;
		/** Leaves and petals: lit from either side, a little light through them. */
		bool bThin = false;
	};

	struct FScene
	{
		std::vector<FTri> Tris;
		float Height = 0.f, Reach = 0.f;
		void Add(const FVector3f& A, const FVector3f& B, const FVector3f& C, const FLinearColor& Col, bool bThin)
		{
			Tris.push_back({ A, B, C, Col, bThin });
			for (const FVector3f* P : { &A, &B, &C })
			{
				Height = FMath::Max(Height, P->Z);
				Reach = FMath::Max(Reach, FVector2f(P->X, P->Y).Size());
			}
		}
	};

	FVector3f ToF(const FVector& V) { return FVector3f((float)V.X, (float)V.Y, (float)V.Z); }
	/** A point of an instance's local mesh (cm) to tree space (m). */
	FVector3f Xf(const FTransform& T, const FVector3f& LocalCm) { return ToF(T.TransformPosition(FVector(LocalCm.X, LocalCm.Y, LocalCm.Z))) / 100.f; }

	FVector3f AnyPerp(const FVector3f& D)
	{
		return FVector3f::CrossProduct(D, FMath::Abs(D.Z) < 0.9f ? FVector3f(0.f, 0.f, 1.f) : FVector3f(1.f, 0.f, 0.f)).GetSafeNormal();
	}

	/** A tube through P (m) with radii R. */
	void Tube(FScene& Sc, const TArray<FVector3f>& P, const TArray<float>& R, const FLinearColor& Col, int32 Sides)
	{
		for (int32 I = 0; I + 1 < P.Num(); I++)
		{
			const FVector3f D = (P[I + 1] - P[I]).GetSafeNormal(UE_SMALL_NUMBER, FVector3f(0.f, 0.f, 1.f));
			const FVector3f X = AnyPerp(D), Y = FVector3f::CrossProduct(D, X);
			for (int32 S = 0; S < Sides; S++)
			{
				const float A0 = 2.f * PI * S / Sides, A1 = 2.f * PI * (S + 1) / Sides;
				const FVector3f R0 = X * FMath::Cos(A0) + Y * FMath::Sin(A0), R1 = X * FMath::Cos(A1) + Y * FMath::Sin(A1);
				const FVector3f Q0 = P[I] + R0 * R[I], Q1 = P[I] + R1 * R[I], Q2 = P[I + 1] + R0 * R[I + 1], Q3 = P[I + 1] + R1 * R[I + 1];
				Sc.Add(Q0, Q2, Q1, Col, false);
				Sc.Add(Q1, Q2, Q3, Col, false);
			}
		}
	}

	/** A spindle along Dir from Base (catkins, cones, fruit): Len long, Radius wide. */
	void Spindle(FScene& Sc, const FVector3f& Base, const FVector3f& Dir, float Len, float Radius, const FLinearColor& Col)
	{
		TArray<FVector3f> P;
		TArray<float> R;
		const float Ts[5] = { 0.f, 0.2f, 0.5f, 0.8f, 1.f }, Rs[5] = { 0.3f, 0.85f, 1.f, 0.8f, 0.05f };
		for (int32 I = 0; I < 5; I++) { P.Add(Base + Dir * (Len * Ts[I])); R.Add(Radius * Rs[I]); }
		Tube(Sc, P, R, Col, 5);
	}

	/** One leaf blade in cluster space (m) as BuildBlade lays it: along +Y, across X, from the origin of Rot/At. */
	void Blade(FScene& Sc, const FPlantLeafShape& L, float Length, const FQuat& Rot, const FVector3f& At, const FTransform& Inst, const FLinearColor& Col)
	{
		auto Put = [&](const FVector3f& P) { return Xf(Inst, (ToF(Rot.RotateVector(FVector(P.X, P.Y, P.Z))) + At) * 100.f); };
		const float W = L.Width * Length;
		if (L.Form == EPlantLeafForm::Palmate)
		{
			const int32 N = 24, Lobes = FMath::Max(L.Lobes, 1);
			const float Spread = 0.72f * PI, Spacing = Lobes > 1 ? 2.f * Spread / (Lobes - 1) : 2.f * Spread, Mid = (Lobes - 1) * 0.5f;
			const FVector3f C = Put(FVector3f(0.f, 0.f, 0.f));
			FVector3f Prev;
			for (int32 I = 0; I <= N; I++)
			{
				const float Phi = -0.86f * PI + 1.72f * PI * I / N;
				const int32 K = FMath::Clamp(FMath::RoundToInt((Phi + Spread) / Spacing), 0, Lobes - 1);
				const float LobeLen = Mid > 0.f ? 1.f - 0.45f * FMath::Pow(FMath::Abs(K - Mid) / Mid, 1.5f) : 1.f;
				const float Cc = FMath::Clamp(1.f - FMath::Abs(Phi - (-Spread + K * Spacing)) / (Spacing * 0.5f), 0.f, 1.f);
				const float R = Length * LobeLen * ((1.f - L.LobeDepth) + L.LobeDepth * FMath::Pow(Cc, 1.4f));
				const FVector3f P = Put(FVector3f(FMath::Sin(Phi) * R * L.Width * 0.85f, FMath::Cos(Phi) * R, -0.12f * Length * FMath::Square(R / Length)));
				if (I > 0) Sc.Add(C, Prev, P, Col, true);
				Prev = P;
			}
			return;
		}
		if (L.Form == EPlantLeafForm::Needle)
		{
			const FVector3f A = Put(FVector3f(-0.5f * W, 0.f, 0.f)), B = Put(FVector3f(0.5f * W, 0.f, 0.f)), T = Put(FVector3f(0.f, Length, -0.09f * Length));
			Sc.Add(A, B, T, Col, true);
			return;
		}
		const int32 N = 8;
		const float A = FMath::Clamp(L.WidestAt, 0.05f, 0.95f), Al = 2.2f * A, Be = 2.2f * (1.f - A);
		const float Norm = FMath::Pow(A, Al) * FMath::Pow(1.f - A, Be);
		FVector3f PL, PC, PR;
		for (int32 I = 0; I <= N; I++)
		{
			const float T = (float)I / N;
			float F = FMath::Pow(T, Al) * FMath::Pow(1.f - T, Be) / Norm;
			if (L.Lobes > 0) F *= (1.f - L.LobeDepth) + L.LobeDepth * FMath::Sqrt(FMath::Abs(FMath::Sin(PI * L.Lobes * T)));
			const float Hw = 0.5f * W * F, Y = T * Length, Z = -0.15f * Length * T * T;
			const FVector3f QL = Put(FVector3f(-Hw, Y, Z)), QC = Put(FVector3f(0.f, Y, Z + L.Fold * Hw)), QR = Put(FVector3f(Hw, Y, Z));
			if (I > 0)
			{
				Sc.Add(PL, QL, QC, Col, true); Sc.Add(PL, QC, PC, Col, true);
				Sc.Add(PC, QC, QR, Col, true); Sc.Add(PC, QR, PR, Col, true);
			}
			PL = QL; PC = QC; PR = QR;
		}
	}

	/** A leaf cluster instance (PlantMeshes LeafClusterData). */
	void Cluster(FScene& Sc, const FPlantSpecies& S, const FTransform& Inst, const FLinearColor& Col)
	{
		const FPlantLeafShape& L = S.Leaf;
		const int32 Count = FMath::Max(L.PerCluster, 1);
		constexpr float GoldenAngle = 2.39996323f;
		if (L.Form == EPlantLeafForm::Needle)
		{
			// a spray of needle cards; a card shows a fan of needles: drawn as three needles
			const int32 Cards = FMath::Clamp(L.Cards, 1, 8);
			for (int32 I = 0; I < Cards; I++)
			{
				const float Phi = I * GoldenAngle * 2.f + PlantModel::Hash01(77, I, 1) * 0.5f;
				const float Theta = FMath::DegreesToRadians(25.f + 70.f * PlantModel::Hash01(77, I, 2));
				const FVector3f Dir(FMath::Cos(Phi) * FMath::Sin(Theta), FMath::Sin(Phi) * FMath::Sin(Theta), FMath::Cos(Theta));
				const FVector3f Side = FVector3f::CrossProduct(Dir, FVector3f(0.f, 0.f, 1.f)).GetSafeNormal(UE_SMALL_NUMBER, FVector3f(1.f, 0.f, 0.f));
				const float Length = L.Length * (0.85f + 0.3f * PlantModel::Hash01(77, I, 4)), Half = 0.5f * Length * L.Width;
				const FVector3f Root(0.f, 0.f, S.LeafClusterSize * 0.25f * (PlantModel::Hash01(77, I, 3) - 0.3f));
				for (int32 K = -1; K <= 1; K++)
				{
					const FVector3f Tip = Root + (Dir * Length + Side * (Half * 0.8f * K));
					const FVector3f W = Side * (Half * 0.12f);
					Sc.Add(Xf(Inst, (Root - W) * 100.f), Xf(Inst, (Root + W) * 100.f), Xf(Inst, Tip * 100.f), Col, true);
				}
			}
			return;
		}
		for (int32 I = 0; I < Count; I++)
		{
			const float Phi = I * GoldenAngle + PlantModel::Hash01(77, I, 1) * 0.5f;
			const FVector3f Out(FMath::Cos(Phi), FMath::Sin(Phi), 0.f);
			const FVector3f Node = Out * (S.LeafClusterSize * 0.1f * (0.5f + PlantModel::Hash01(77, I, 2))) + FVector3f(0.f, 0.f, ((I + 0.5f) / Count - 0.5f) * S.LeafClusterSize * 0.25f);
			const float Length = L.Length * (0.8f + 0.4f * PlantModel::Hash01(77, I, 3));
			const FVector3f Stalk = Out * (L.Petiole * Length) - FVector3f(0.f, 0.f, 0.25f * L.Petiole * Length);
			const FRotator Rot(-(8.f + 22.f * PlantModel::Hash01(77, I, 4)), FMath::RadiansToDegrees(Phi) - 90.f, (PlantModel::Hash01(77, I, 5) - 0.5f) * 40.f);
			Blade(Sc, L, Length, Rot.Quaternion(), Node + Stalk, Inst, Col);
		}
	}

	void BuildTree(FScene& Sc, const FPlantSpecies& S, uint32 Seed, float Age, float Day)
	{
		FPlantSkeleton Skeleton;
		Skeleton.Build(S, Seed);
		const FPlantSeason Season = PlantModel::Season(S, Seed, Day);
		FPlantSkeleton::FInstances Out;
		Skeleton.Evaluate(Age, Season, Out);
		const FLinearColor Bark = S.Bark.Pattern == EPlantBarkPattern::Birch ? FLinearColor(0.7f, 0.68f, 0.62f) : S.Bark.RidgeColour * 0.7f + S.Bark.FurrowColour * 0.3f;
		for (const FPlantSkeleton::FTube& T : Out.Tubes)
		{
			const bool bTwig = S.Bark.TwigRadius > 0.f && T.R.Num() > 0 && T.R[0] < S.Bark.TwigRadius;
			Tube(Sc, T.P, T.R, bTwig ? S.Bark.TwigColour : Bark, T.R.Num() > 0 && T.R[0] > 0.05f ? 8 : 4);
		}
		const size_t Before = Sc.Tris.size();
		for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
			for (const FTransform& Inst : Out.Leaves[K]) Cluster(Sc, S, Inst, Season.Colour[K]);
		if (getenv("PLANTSIM_DEBUG"))
		{
			double Area = 0.0;
			for (size_t I = Before; I < Sc.Tris.size(); I++) Area += 0.5 * FVector3f::CrossProduct(Sc.Tris[I].B - Sc.Tris[I].A, Sc.Tris[I].C - Sc.Tris[I].A).Size();
			std::printf("leaf triangles %d, area %.2f m2\n", (int)(Sc.Tris.size() - Before), Area);
		}
		const FPlantBlossom& B = S.Blossom;
		const FLinearColor BloomCol = B.FreshColour * Season.BloomTint;
		for (const FTransform& Inst : Out.Blossoms)
		{
			const int32 N = FMath::Max(B.PerCluster, 1);
			for (int32 I = 0; I < N; I++)
			{
				const float Phi = I * 2.39996323f;
				if (B.Form == EPlantBlossomForm::Blossom)
				{
					const float Rad = N > 1 ? 0.5f * B.Spread * B.Size * FMath::Sqrt((I + 0.5f) / N) : 0.f;
					const FVector3f C(Rad * FMath::Cos(Phi), Rad * FMath::Sin(Phi), B.Size * 0.9f);
					const float R = 0.5f * B.Size;
					for (int32 P = 0; P < 6; P++)
					{
						const float A0 = 2.f * PI * P / 6.f, A1 = 2.f * PI * (P + 1) / 6.f;
						Sc.Add(Xf(Inst, C * 100.f), Xf(Inst, (C + FVector3f(FMath::Cos(A0), FMath::Sin(A0), 0.f) * R) * 100.f),
						       Xf(Inst, (C + FVector3f(FMath::Cos(A1), FMath::Sin(A1), 0.f) * R) * 100.f), BloomCol, true);
					}
					continue;
				}
				const bool bCatkin = B.Form == EPlantBlossomForm::Catkin;
				const float Tilt = bCatkin ? (N > 1 ? 0.12f + 0.28f * PlantModel::Hash01(78, I, 2) : 0.f)
				                           : (N > 1 ? FMath::DegreesToRadians(8.f + 50.f * FMath::Sqrt((I + 0.5f) / N)) : 0.f);
				const FVector3f Dir(FMath::Sin(Tilt) * FMath::Cos(Phi), FMath::Sin(Tilt) * FMath::Sin(Phi), (bCatkin ? -1.f : 1.f) * FMath::Cos(Tilt));
				const FVector3f Base = Xf(Inst, FVector3f(0.f, 0.f, 0.f));
				const FVector3f Tip = Xf(Inst, Dir * (B.Size * 100.f));
				const float Len = FVector3f::Dist(Base, Tip);
				Spindle(Sc, Base, (Tip - Base).GetSafeNormal(), Len, 0.5f * Len * B.Thickness, BloomCol);
			}
		}
		const FLinearColor FruitCol = Season.FruitColour;
		for (const FTransform& F : Out.Fruit)
		{
			const FVector3f C = Xf(F, FVector3f(0.f, 0.f, 0.f));
			const float D = (float)F.GetScale3D().X;
			Spindle(Sc, C - FVector3f(0.f, 0.f, 0.5f * D * S.FruitAspect), FVector3f(0.f, 0.f, 1.f), D * S.FruitAspect, 0.5f * D, FruitCol);
		}
	}

	void BuildHerb(FScene& Sc, const FPlantSpecies& S, uint32 Seed, float Age, float Day)
	{
		const PlantHerbs::FState St = PlantHerbs::State(S, Seed, Age, Day);
		PlantHerbs::FHerbGeometry G;
		float H = 0.f, R = 0.f;
		PlantHerbs::Build(S, Seed, St, G, H, R);
		const FPlantHerb& Hb = S.Herb;
		FLinearColor Leaf = St.LeafColour;
		if (St.Wither > 0.f) Leaf = FMath::Lerp(Leaf, S.AutumnLeafColourLate, St.Wither);
		for (int32 Slot = 0; Slot < PlantHerbs::NumSlots; Slot++)
		{
			for (const FIntVector3& T : G.Tris[Slot])
			{
				FLinearColor Col = Slot == PlantHerbs::Leaves ? Leaf : Hb.StemColour;
				if (Slot == PlantHerbs::Flowers)
				{
					// the atlas: a column per colour variant, the petals above v 0.62, the centre below 0.72
					const FVector2f Uv = G.UV[T.X];
					const int32 V = FMath::Clamp((int32)(Uv.X / 0.25f), 0, 3);
					Col = Uv.Y > 0.7f ? Hb.CentreColour : Hb.FlowerColours.Num() > 0 ? Hb.FlowerColours[V % Hb.FlowerColours.Num()] : FLinearColor(0.8f, 0.8f, 0.8f);
				}
				Sc.Add(G.Pos[T.X] / 100.f, G.Pos[T.Y] / 100.f, G.Pos[T.Z] / 100.f, Col, Slot != PlantHerbs::Stems);
			}
		}
	}

	struct FImage
	{
		int32 W = 0, H = 0;
		std::vector<float> Rgb, Depth;
		void Init(int32 InW, int32 InH)
		{
			W = InW; H = InH;
			Rgb.assign((size_t)W * H * 3, 0.f);
			Depth.assign((size_t)W * H, 1e30f);
		}
	};

	struct FCamera
	{
		FVector3f Eye, Fwd, Right, Upv;
		float Focal = 1.f;
		float Cx = 0.f, Cy = 0.f;
		/** Screen x, y and depth. */
		FVector3f Project(const FVector3f& P) const
		{
			const FVector3f D = P - Eye;
			const float Z = FVector3f::DotProduct(D, Fwd);
			return FVector3f(Cx + Focal * FVector3f::DotProduct(D, Right) / Z, Cy - Focal * FVector3f::DotProduct(D, Upv) / Z, Z);
		}
	};

	long GThinPixels = 0;

	void Raster(FImage& Im, const FCamera& Cam, int32 X0, int32 X1, const FTri& T, const FVector3f& Sun)
	{
		// in cm: a leaf's triangles are a few mm across, below GetSafeNormal's tolerance in m
		FVector3f N = FVector3f::CrossProduct((T.B - T.A) * 100.f, (T.C - T.A) * 100.f).GetSafeNormal();
		if (N.IsNearlyZero()) return;
		const FVector3f Centre = (T.A + T.B + T.C) / 3.f;
		const FVector3f ToEye = (Cam.Eye - Centre).GetSafeNormal();
		if (FVector3f::DotProduct(N, ToEye) < 0.f) N = -N;
		const float Ns = FVector3f::DotProduct(N, Sun);
		float Light;
		if (T.bThin)
		{
			// a leaf: the sun on its face, or through it (dimmer) when lit from behind; the sky from above
			Light = 0.32f + 0.12f * N.Z + (Ns > 0.f ? 1.0f * Ns : 0.35f * -Ns);
		}
		else
		{
			Light = 0.3f + 0.12f * N.Z + 1.0f * FMath::Max(Ns, 0.f);
		}
		// the exposure: the species' colours are albedos, dark as leaves are
		const FLinearColor C = T.Col * (1.9f * Light);
		const FVector3f A = Cam.Project(T.A), B = Cam.Project(T.B), Cc = Cam.Project(T.C);
		if (A.Z <= 0.01f || B.Z <= 0.01f || Cc.Z <= 0.01f) return;
		const int32 MinX = FMath::Max(X0, (int32)std::floor(FMath::Min(A.X, FMath::Min(B.X, Cc.X))));
		const int32 MaxX = FMath::Min(X1 - 1, (int32)std::ceil(FMath::Max(A.X, FMath::Max(B.X, Cc.X))));
		const int32 MinY = FMath::Max(0, (int32)std::floor(FMath::Min(A.Y, FMath::Min(B.Y, Cc.Y))));
		const int32 MaxY = FMath::Min(Im.H - 1, (int32)std::ceil(FMath::Max(A.Y, FMath::Max(B.Y, Cc.Y))));
		const float Area = (B.X - A.X) * (Cc.Y - A.Y) - (B.Y - A.Y) * (Cc.X - A.X);
		if (FMath::Abs(Area) < 1e-9f) return;
		for (int32 Y = MinY; Y <= MaxY; Y++)
		{
			for (int32 X = MinX; X <= MaxX; X++)
			{
				const float Px = X + 0.5f, Py = Y + 0.5f;
				const float W0 = ((B.X - Px) * (Cc.Y - Py) - (B.Y - Py) * (Cc.X - Px)) / Area;
				const float W1 = ((Cc.X - Px) * (A.Y - Py) - (Cc.Y - Py) * (A.X - Px)) / Area;
				const float W2 = 1.f - W0 - W1;
				if (W0 < 0.f || W1 < 0.f || W2 < 0.f) continue;
				const float Z = W0 * A.Z + W1 * B.Z + W2 * Cc.Z;
				const size_t I = (size_t)Y * Im.W + X;
				if (Z >= Im.Depth[I]) continue;
				Im.Depth[I] = Z;
				if (T.bThin) GThinPixels++;
				Im.Rgb[3 * I] = C.R; Im.Rgb[3 * I + 1] = C.G; Im.Rgb[3 * I + 2] = C.B;
			}
		}
	}

	uint32 Crc(const unsigned char* P, size_t N, uint32 C = 0xffffffffu)
	{
		static uint32 Table[256];
		static bool bTable = false;
		if (!bTable)
		{
			for (uint32 I = 0; I < 256; I++)
			{
				uint32 V = I;
				for (int32 K = 0; K < 8; K++) V = V & 1 ? 0xedb88320u ^ (V >> 1) : V >> 1;
				Table[I] = V;
			}
			bTable = true;
		}
		for (size_t I = 0; I < N; I++) C = Table[(C ^ P[I]) & 0xff] ^ (C >> 8);
		return C;
	}

	/** An 8-bit RGB PNG, its deflate stream in stored blocks. */
	bool WritePng(const char* Path, int32 W, int32 H, const std::vector<unsigned char>& Rgb)
	{
		std::vector<unsigned char> Raw;
		for (int32 Y = 0; Y < H; Y++)
		{
			Raw.push_back(0);
			Raw.insert(Raw.end(), Rgb.begin() + (size_t)Y * W * 3, Rgb.begin() + (size_t)(Y + 1) * W * 3);
		}
		std::vector<unsigned char> Z = { 0x78, 0x01 };
		uint32 A = 1, B = 0;
		for (unsigned char C : Raw) { A = (A + C) % 65521u; B = (B + A) % 65521u; }
		for (size_t Off = 0; Off < Raw.size() || Off == 0; Off += 65535)
		{
			const size_t Len = std::min<size_t>(65535, Raw.size() - Off);
			Z.push_back(Off + Len >= Raw.size() ? 1 : 0);
			Z.push_back(Len & 0xff); Z.push_back(Len >> 8); Z.push_back(~Len & 0xff); Z.push_back((~Len >> 8) & 0xff);
			Z.insert(Z.end(), Raw.begin() + Off, Raw.begin() + Off + Len);
			if (Len == 0) break;
		}
		const uint32 Adler = (B << 16) | A;
		for (int32 S = 24; S >= 0; S -= 8) Z.push_back((Adler >> S) & 0xff);
		FILE* F = std::fopen(Path, "wb");
		if (!F) return false;
		auto Be32 = [&](uint32 V) { unsigned char Q[4] = { (unsigned char)(V >> 24), (unsigned char)(V >> 16), (unsigned char)(V >> 8), (unsigned char)V }; std::fwrite(Q, 1, 4, F); };
		auto Chunk = [&](const char* Type, const std::vector<unsigned char>& Data)
		{
			Be32((uint32)Data.size());
			std::vector<unsigned char> TD(Type, Type + 4);
			TD.insert(TD.end(), Data.begin(), Data.end());
			std::fwrite(TD.data(), 1, TD.size(), F);
			Be32(Crc(TD.data(), TD.size()) ^ 0xffffffffu);
		};
		const unsigned char Sig[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
		std::fwrite(Sig, 1, 8, F);
		std::vector<unsigned char> Hdr = { (unsigned char)(W >> 24), (unsigned char)(W >> 16), (unsigned char)(W >> 8), (unsigned char)W,
			(unsigned char)(H >> 24), (unsigned char)(H >> 16), (unsigned char)(H >> 8), (unsigned char)H, 8, 2, 0, 0, 0 };
		Chunk("IHDR", Hdr);
		Chunk("IDAT", Z);
		Chunk("IEND", {});
		std::fclose(F);
		return true;
	}
}

int RenderMain(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::printf("usage: run.sh render <out.png> <Name>:<age>:<day>[:<seed>] ...\n");
		return 2;
	}
	std::vector<FScene> Scenes;
	for (int I = 2; I < Argc; I++)
	{
		std::string A = Argv[I];
		float Age = 20.f, Day = 200.f, SeedF = 1.f;
		const size_t C1 = A.find(':');
		if (C1 != std::string::npos)
		{
			std::sscanf(A.c_str() + C1 + 1, "%f:%f:%f", &Age, &Day, &SeedF);
			A = A.substr(0, C1);
		}
		const FPlantSpecies* S = PlantSpeciesLibrary::Find(FName(A.c_str()));
		if (!S)
		{
			std::printf("no species %s\n", A.c_str());
			return 1;
		}
		FScene Sc;
		if (S->Herb.Form == EPlantHerbForm::None) BuildTree(Sc, *S, (uint32)SeedF, Age, Day);
		else BuildHerb(Sc, *S, (uint32)SeedF, Age, Day);
		std::printf("%s age %.0f day %.0f seed %u: h %.2f m reach %.2f m, %d triangles drawn\n", A.c_str(), Age, Day, (uint32)SeedF, Sc.Height, Sc.Reach, (int)Sc.Tris.size());
		Scenes.push_back(std::move(Sc));
	}
	// one scale for every panel: the tallest plant (and the 2 m pole) fills 80% of the height
	float Top = 2.f, Wide = 0.5f;
	for (const FScene& Sc : Scenes) { Top = FMath::Max(Top, Sc.Height); Wide = FMath::Max(Wide, Sc.Reach); }
	constexpr int32 SS = 2, PanelH = 720;
	const int32 PanelW = FMath::Clamp((int32)(PanelH * (2.f * Wide + 1.2f) / (1.25f * Top)), 260, 900);
	FImage Im;
	Im.Init(PanelW * (int32)Scenes.size() * SS, PanelH * SS);
	const FVector3f Sun = FVector3f(-0.45f, -0.6f, 0.66f).GetSafeNormal();
	for (size_t K = 0; K < Scenes.size(); K++)
	{
		const int32 X0 = (int32)K * PanelW * SS, X1 = X0 + PanelW * SS;
		// sky over a gravel disc
		for (int32 Y = 0; Y < Im.H; Y++)
			for (int32 X = X0; X < X1; X++)
			{
				const float T = (float)Y / Im.H;
				const size_t I = (size_t)Y * Im.W + X;
				Im.Rgb[3 * I] = 0.32f + 0.25f * T; Im.Rgb[3 * I + 1] = 0.45f + 0.22f * T; Im.Rgb[3 * I + 2] = 0.7f + 0.1f * T;
			}
		FCamera Cam;
		const float Fov = FMath::DegreesToRadians(28.f);
		const float Dist = 0.62f * Top / FMath::Sin(0.5f * Fov) + Wide;
		const FVector3f Target(0.f, 0.f, 0.47f * Top);
		Cam.Fwd = FVector3f(0.25f, 1.f, -0.38f).GetSafeNormal();
		Cam.Eye = Target - Cam.Fwd * Dist;
		Cam.Right = FVector3f::CrossProduct(Cam.Fwd, FVector3f(0.f, 0.f, 1.f)).GetSafeNormal() * -1.f;
		Cam.Upv = FVector3f::CrossProduct(Cam.Right, Cam.Fwd) * -1.f;
		Cam.Focal = 0.5f * Im.H / std::tan(0.5f * Fov);
		Cam.Cx = X0 + 0.5f * PanelW * SS;
		Cam.Cy = 0.5f * Im.H;
		FScene Ground;
		const float Gr = 1.2f * Wide + 1.6f + 0.3f * Top;
		for (int32 S = 0; S < 48; S++)
		{
			const float A0 = 2.f * PI * S / 48.f, A1 = 2.f * PI * (S + 1) / 48.f;
			Ground.Add(FVector3f(0.f, 0.f, 0.f), FVector3f(FMath::Cos(A0), FMath::Sin(A0), 0.f) * Gr, FVector3f(FMath::Cos(A1), FMath::Sin(A1), 0.f) * Gr, FLinearColor(0.2f, 0.19f, 0.16f), false);
		}
		// the pole: 2 m, red and white every half metre, beside the plant
		const FVector3f PoleAt = Cam.Right * -(Wide + 0.35f);
		for (int32 B = 0; B < 4; B++)
		{
			TArray<FVector3f> P = { PoleAt + FVector3f(0.f, 0.f, 0.5f * B), PoleAt + FVector3f(0.f, 0.f, 0.5f * (B + 1)) };
			TArray<float> R = { 0.025f, 0.025f };
			Tube(Ground, P, R, B % 2 ? FLinearColor(0.7f, 0.7f, 0.7f) : FLinearColor(0.6f, 0.05f, 0.03f), 6);
		}
		for (const FTri& T : Ground.Tris) Raster(Im, Cam, X0, X1, T, Sun);
		GThinPixels = 0;
		for (const FTri& T : Scenes[K].Tris) Raster(Im, Cam, X0, X1, T, Sun);
		if (getenv("PLANTSIM_DEBUG")) std::printf("panel %d: %ld leaf pixels\n", (int)K, GThinPixels);
	}
	// down-sample and encode
	const int32 W = Im.W / SS, H = Im.H / SS;
	std::vector<unsigned char> Out((size_t)W * H * 3);
	for (int32 Y = 0; Y < H; Y++)
		for (int32 X = 0; X < W; X++)
		{
			float C[3] = {};
			for (int32 Sy = 0; Sy < SS; Sy++)
				for (int32 Sx = 0; Sx < SS; Sx++)
					for (int32 Ch = 0; Ch < 3; Ch++) C[Ch] += Im.Rgb[3 * ((size_t)(Y * SS + Sy) * Im.W + X * SS + Sx) + Ch];
			const FColor F = FLinearColor(C[0] / (SS * SS), C[1] / (SS * SS), C[2] / (SS * SS)).ToFColor(true);
			unsigned char* P = &Out[3 * ((size_t)Y * W + X)];
			P[0] = F.R; P[1] = F.G; P[2] = F.B;
		}
	// a dark line between panels
	for (size_t K = 1; K < Scenes.size(); K++)
		for (int32 Y = 0; Y < H; Y++)
		{
			unsigned char* P = &Out[3 * ((size_t)Y * W + K * PanelW)];
			P[0] = P[1] = P[2] = 30;
		}
	if (!WritePng(Argv[1], W, H, Out))
	{
		std::printf("cannot write %s\n", Argv[1]);
		return 1;
	}
	std::printf("wrote %s (%d x %d)\n", Argv[1], W, H);
	return 0;
}
