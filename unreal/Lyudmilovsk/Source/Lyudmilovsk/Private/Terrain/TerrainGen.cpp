#include "TerrainGen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>

namespace LyudTerrain
{
const char* const LayerNames[NumLayers] = {"Grass", "Meadow", "Dirt", "Mud", "Sand", "Rock", "Forest"};

namespace
{
	inline float Clamp01(float V) { return V < 0.f ? 0.f : (V > 1.f ? 1.f : V); }
	inline float Smooth(float E0, float E1, float V)
	{
		const float T = Clamp01((V - E0) / (E1 - E0));
		return T * T * (3.f - 2.f * T);
	}
	inline float Lerp(float A, float B, float T) { return A + (B - A) * T; }

	/** Perlin gradient noise, -1..1, seeded; std::mt19937 is the same on every platform. */
	struct FNoise
	{
		uint8_t Perm[512];
		explicit FNoise(uint32_t Seed)
		{
			std::mt19937 Rng(Seed);
			uint8_t P[256];
			for (int I = 0; I < 256; ++I) P[I] = uint8_t(I);
			for (int I = 255; I > 0; --I) std::swap(P[I], P[Rng() % uint32_t(I + 1)]);
			for (int I = 0; I < 512; ++I) Perm[I] = P[I & 255];
		}
		static float Grad(int H, float X, float Y)
		{
			switch (H & 7)
			{
			case 0: return X + Y;
			case 1: return X - Y;
			case 2: return -X + Y;
			case 3: return -X - Y;
			case 4: return X;
			case 5: return -X;
			case 6: return Y;
			default: return -Y;
			}
		}
		static float Fade(float T) { return T * T * T * (T * (T * 6.f - 15.f) + 10.f); }
		float Get(float X, float Y) const
		{
			const int XI = int(std::floor(X)), YI = int(std::floor(Y));
			const float XF = X - XI, YF = Y - YI;
			const int A = XI & 255, B = YI & 255;
			const float U = Fade(XF), V = Fade(YF);
			const float N00 = Grad(Perm[Perm[A] + B], XF, YF);
			const float N10 = Grad(Perm[Perm[A + 1] + B], XF - 1.f, YF);
			const float N01 = Grad(Perm[Perm[A] + B + 1], XF, YF - 1.f);
			const float N11 = Grad(Perm[Perm[A + 1] + B + 1], XF - 1.f, YF - 1.f);
			return Lerp(Lerp(N00, N10, U), Lerp(N01, N11, U), V) * 0.85f;
		}
		/** Fractal sum, roughly -1..1; Wave = the largest feature, metres. */
		float Fbm(double X, double Y, float Wave, int Octaves, float Gain = 0.5f) const
		{
			float Sum = 0.f, Amp = 1.f, Norm = 0.f;
			double F = 1.0 / Wave;
			for (int O = 0; O < Octaves; ++O)
			{
				// each octave turned and shifted so the grids don't line up
				const double C = 0.8, S = 0.6;
				const double RX = X * C - Y * S + O * 17.31, RY = X * S + Y * C - O * 9.77;
				Sum += Amp * Get(float(std::fmod(RX * F, 256.0)), float(std::fmod(RY * F, 256.0)));
				Norm += Amp;
				Amp *= Gain;
				F *= 2.03;
				X = RX; Y = RY;
			}
			return Sum / Norm;
		}
	};

	/** Approximate euclidean distance (metres) from every vertex to the nearest vertex where Seed is true. */
	std::vector<float> Distance(int NX, int NY, const std::vector<uint8_t>& IsSeed, float Max)
	{
		std::vector<float> D(size_t(NX) * NY, Max);
		for (size_t I = 0; I < D.size(); ++I) if (IsSeed[I]) D[I] = 0.f;
		const float S2 = 1.41421356f;
		for (int Y = 0; Y < NY; ++Y)
			for (int X = 0; X < NX; ++X)
			{
				float& V = D[size_t(Y) * NX + X];
				if (X > 0) V = std::min(V, D[size_t(Y) * NX + X - 1] + 1.f);
				if (Y > 0)
				{
					V = std::min(V, D[size_t(Y - 1) * NX + X] + 1.f);
					if (X > 0) V = std::min(V, D[size_t(Y - 1) * NX + X - 1] + S2);
					if (X < NX - 1) V = std::min(V, D[size_t(Y - 1) * NX + X + 1] + S2);
				}
			}
		for (int Y = NY - 1; Y >= 0; --Y)
			for (int X = NX - 1; X >= 0; --X)
			{
				float& V = D[size_t(Y) * NX + X];
				if (X < NX - 1) V = std::min(V, D[size_t(Y) * NX + X + 1] + 1.f);
				if (Y < NY - 1)
				{
					V = std::min(V, D[size_t(Y + 1) * NX + X] + 1.f);
					if (X < NX - 1) V = std::min(V, D[size_t(Y + 1) * NX + X + 1] + S2);
					if (X > 0) V = std::min(V, D[size_t(Y + 1) * NX + X - 1] + S2);
				}
			}
		return D;
	}

	/** Separable box blur, radius R vertices, repeated Passes times (three passes ~ gaussian). */
	std::vector<float> Blur(const std::vector<float>& In, int NX, int NY, int R, int Passes)
	{
		std::vector<float> A = In, B(In.size());
		for (int P = 0; P < Passes; ++P)
		{
			for (int Y = 0; Y < NY; ++Y)
			{
				const float* Row = &A[size_t(Y) * NX];
				float Sum = 0.f;
				for (int X = -R; X <= R; ++X) Sum += Row[std::clamp(X, 0, NX - 1)];
				for (int X = 0; X < NX; ++X)
				{
					B[size_t(Y) * NX + X] = Sum / (2 * R + 1);
					Sum += Row[std::min(X + R + 1, NX - 1)] - Row[std::max(X - R, 0)];
				}
			}
			for (int X = 0; X < NX; ++X)
			{
				float Sum = 0.f;
				for (int Y = -R; Y <= R; ++Y) Sum += B[size_t(std::clamp(Y, 0, NY - 1)) * NX + X];
				for (int Y = 0; Y < NY; ++Y)
				{
					A[size_t(Y) * NX + X] = Sum / (2 * R + 1);
					Sum += B[size_t(std::min(Y + R + 1, NY - 1)) * NX + X] - B[size_t(std::max(Y - R, 0)) * NX + X];
				}
			}
		}
		return A;
	}

	/**
	 * Hydraulic erosion by rain drops (after Hans Theobald Beyer, 2015): every drop runs downhill,
	 * picks up soil where it speeds up and drops it where it slows, carving gullies down the slopes
	 * and filling the hollows. Strength scales with Free (0 = untouched). Visits counts the water.
	 */
	void Erode(std::vector<float>& H, int NX, int NY, const std::vector<float>& Free, uint32_t Seed, int Drops,
	           std::vector<float>& Visits)
	{
		const float Inertia = 0.06f, Capacity = 3.5f, MinCapacity = 0.01f, DepositRate = 0.25f, ErodeRate = 0.3f,
		            Evaporate = 0.015f, Gravity = 4.f;
		const int Life = 48, Radius = 2;
		// erosion brush: weights of the cells within Radius
		std::vector<std::pair<int, int>> Off;
		std::vector<float> Wt;
		float WSum = 0.f;
		for (int DY = -Radius; DY <= Radius; ++DY)
			for (int DX = -Radius; DX <= Radius; ++DX)
			{
				const float D = std::sqrt(float(DX * DX + DY * DY));
				if (D <= Radius)
				{
					Off.push_back({DX, DY});
					Wt.push_back(1.f - D / Radius + 0.05f);
					WSum += Wt.back();
				}
			}
		for (float& V : Wt) V /= WSum;

		auto Sample = [&](float X, float Y, float& GX, float& GY) {
			const int CX = int(X), CY = int(Y);
			const float U = X - CX, V = Y - CY;
			const size_t I = size_t(CY) * NX + CX;
			const float H00 = H[I], H10 = H[I + 1], H01 = H[I + NX], H11 = H[I + NX + 1];
			GX = (H10 - H00) * (1 - V) + (H11 - H01) * V;
			GY = (H01 - H00) * (1 - U) + (H11 - H10) * U;
			return H00 * (1 - U) * (1 - V) + H10 * U * (1 - V) + H01 * (1 - U) * V + H11 * U * V;
		};

		std::mt19937 Rng(Seed ^ 0x9e3779b9u);
		for (int D = 0; D < Drops; ++D)
		{
			float X = float(Rng() % uint32_t(NX - 3)) + 1.f + float(Rng() % 1000) / 1000.f;
			float Y = float(Rng() % uint32_t(NY - 3)) + 1.f + float(Rng() % 1000) / 1000.f;
			if (Free[size_t(Y) * NX + size_t(X)] <= 0.02f) continue;
			float DirX = 0, DirY = 0, Speed = 1, Water = 1, Sediment = 0;
			for (int Step = 0; Step < Life; ++Step)
			{
				const int CX = int(X), CY = int(Y);
				const size_t Cell = size_t(CY) * NX + CX;
				const float U = X - CX, V = Y - CY;
				float GX, GY;
				const float Z = Sample(X, Y, GX, GY);
				DirX = DirX * Inertia - GX * (1 - Inertia);
				DirY = DirY * Inertia - GY * (1 - Inertia);
				const float Len = std::sqrt(DirX * DirX + DirY * DirY);
				if (Len < 1e-6f) break;
				DirX /= Len; DirY /= Len;
				X += DirX; Y += DirY;
				if (X < 2 || Y < 2 || X >= NX - 3 || Y >= NY - 3) break;
				Visits[Cell] += Water;
				float NGX, NGY;
				const float DeltaZ = Sample(X, Y, NGX, NGY) - Z;
				const float F = Free[Cell];
				const float Cap = std::max(-DeltaZ * Speed * Water * Capacity, MinCapacity);
				if (Sediment > Cap || DeltaZ > 0)
				{
					// drop soil into the four corners of the cell it leaves
					const float Amount = (DeltaZ > 0 ? std::min(DeltaZ, Sediment) : (Sediment - Cap) * DepositRate) * F;
					Sediment -= Amount;
					H[Cell] += Amount * (1 - U) * (1 - V);
					H[Cell + 1] += Amount * U * (1 - V);
					H[Cell + NX] += Amount * (1 - U) * V;
					H[Cell + NX + 1] += Amount * U * V;
				}
				else
				{
					const float Amount = std::min((Cap - Sediment) * ErodeRate, -DeltaZ) * F;
					for (size_t K = 0; K < Off.size(); ++K)
					{
						const int BX = CX + Off[K].first, BY = CY + Off[K].second;
						if (BX < 0 || BY < 0 || BX >= NX || BY >= NY) continue;
						const size_t BI = size_t(BY) * NX + BX;
						const float Take = std::min(Amount * Wt[K], std::max(H[BI] - Z + 0.5f, 0.f)) * std::min(Free[BI] * 1.5f, 1.f);
						H[BI] -= Take;
						Sediment += Take;
					}
				}
				Speed = std::sqrt(std::max(Speed * Speed + DeltaZ * -Gravity, 0.f));
				Water *= 1 - Evaporate;
			}
		}
	}

	/** Running maximum over a (2R+1) square window (van Herk / Gil-Werman, separable). */
	std::vector<float> Dilate(const std::vector<float>& In, int NX, int NY, int R)
	{
		auto Pass = [R](const float* Src, float* Dst, int Len, int Stride) {
			const int W = 2 * R + 1;
			std::vector<float> G(Len), H(Len);
			for (int I = 0; I < Len; ++I)
			{
				const float V = Src[size_t(I) * Stride];
				G[I] = (I % W == 0) ? V : std::max(G[I - 1], V);
			}
			for (int I = Len - 1; I >= 0; --I)
			{
				const float V = Src[size_t(I) * Stride];
				H[I] = (I % W == W - 1 || I == Len - 1) ? V : std::max(H[I + 1], V);
			}
			for (int I = 0; I < Len; ++I)
			{
				const int A = std::max(I - R, 0), B = std::min(I + R, Len - 1);
				// window [A, B] spans at most two blocks: H from A, G at B
				Dst[size_t(I) * Stride] = (A / W == B / W) ? *std::max_element(&G[A], &G[B] + 1) : std::max(H[A], G[B]);
			}
		};
		std::vector<float> T(In.size()), Out(In.size());
		for (int Y = 0; Y < NY; ++Y) Pass(&In[size_t(Y) * NX], &T[size_t(Y) * NX], NX, 1);
		for (int X = 0; X < NX; ++X) Pass(&T[X], &Out[X], NY, NX);
		return Out;
	}

	/** Distance (metres) from every vertex to the nearest track, up to MaxD. */
	std::vector<float> TrackDistance(const FInputs& In, float MaxD)
	{
		std::vector<float> D(size_t(In.NX) * In.NY, MaxD);
		for (const auto& T : In.Tracks)
			for (size_t S = 0; S + 1 < T.size(); ++S)
			{
				const float AX = float(T[S].first - In.X0), AY = float(T[S].second - In.Y0);
				const float BX = float(T[S + 1].first - In.X0), BY = float(T[S + 1].second - In.Y0);
				const float DX = BX - AX, DY = BY - AY, L2 = std::max(DX * DX + DY * DY, 1e-6f);
				const int X0 = std::max(0, int(std::min(AX, BX) - MaxD)), X1 = std::min(In.NX - 1, int(std::max(AX, BX) + MaxD) + 1);
				const int Y0 = std::max(0, int(std::min(AY, BY) - MaxD)), Y1 = std::min(In.NY - 1, int(std::max(AY, BY) + MaxD) + 1);
				for (int Y = Y0; Y <= Y1; ++Y)
					for (int X = X0; X <= X1; ++X)
					{
						const float T2 = Clamp01(((X - AX) * DX + (Y - AY) * DY) / L2);
						const float EX = X - AX - DX * T2, EY = Y - AY - DY * T2;
						float& V = D[size_t(Y) * In.NX + X];
						V = std::min(V, std::sqrt(EX * EX + EY * EY));
					}
			}
		return D;
	}
}

std::string ReadInputs(const std::string& Dir, FInputs& Out)
{
	const std::string Path = Dir + "/terrain_in.bin";
	FILE* F = std::fopen(Path.c_str(), "rb");
	if (!F) return "cannot open " + Path;
	char Magic[4];
	int32_t Hdr[4];
	uint32_t Seed;
	float Sea;
	int32_t NTracks;
	bool Ok = std::fread(Magic, 4, 1, F) == 1 && std::memcmp(Magic, "LYTR", 4) == 0 && std::fread(Hdr, 4, 4, F) == 4 &&
	          std::fread(&Seed, 4, 1, F) == 1 && std::fread(&Sea, 4, 1, F) == 1 && std::fread(&NTracks, 4, 1, F) == 1;
	if (Ok)
	{
		Out.NX = Hdr[0]; Out.NY = Hdr[1]; Out.X0 = Hdr[2]; Out.Y0 = Hdr[3]; Out.Seed = Seed; Out.SeaZ = Sea;
		Out.Tracks.resize(size_t(std::max(NTracks, 0)));
		for (auto& T : Out.Tracks)
		{
			int32_t N = 0;
			Ok = Ok && std::fread(&N, 4, 1, F) == 1;
			std::vector<float> P(size_t(std::max(N, 0)) * 2);
			Ok = Ok && std::fread(P.data(), 4, P.size(), F) == P.size();
			for (int32_t I = 0; I < N && Ok; ++I) T.push_back({P[2 * I], P[2 * I + 1]});
		}
		const size_t N = size_t(Out.NX) * Out.NY;
		Out.Height.resize(N);
		Ok = Ok && std::fread(Out.Height.data(), 4, N, F) == N;
		for (std::vector<uint8_t>* A : {&Out.Protect, &Out.Kind, &Out.Forest, &Out.Hole, &Out.Cliff, &Out.RaiseOnly})
		{
			A->resize(N);
			Ok = Ok && std::fread(A->data(), 1, N, F) == N;
		}
	}
	std::fclose(F);
	return Ok ? std::string() : "bad or short " + Path;
}

bool WriteHeights(const std::string& Dir, const FOutputs& Out)
{
	FILE* F = std::fopen((Dir + "/terrain_out.bin").c_str(), "wb");
	if (!F) return false;
	const bool Ok = std::fwrite(Out.Height.data(), 4, Out.Height.size(), F) == Out.Height.size();
	std::fclose(F);
	return Ok;
}

namespace
{
	inline uint32_t CellHash(int X, int Y, uint32_t Seed, uint32_t K)
	{
		uint32_t H = uint32_t(X) * 0x8DA6B343u ^ uint32_t(Y) * 0xD8163841u ^ Seed * 0xCB1AB31Fu ^ K * 0x9E3779B9u;
		H ^= H >> 13; H *= 0x5BD1E995u; H ^= H >> 15;
		return H;
	}
	inline float CellRand(int X, int Y, uint32_t Seed, uint32_t K) { return (CellHash(X, Y, Seed, K) & 0xFFFFFFu) / float(0x1000000); }

	/**
	 * Inside the forest only (docs/forest_floor.md): low mossy hummocks 0.2-0.45 m over a few metres, shallow hollows
	 * between them, and every ~20 m on average a windthrow: the pit a fallen tree's roots tore out (~0.45 m deep,
	 * ~1.5 m across) with its mound of soil beside it. Not on tracks, protected ground or the bluff; the 1 m grid
	 * carries this, finer bumps are the ground material's.
	 */
	void ForestFloorRelief(const FInputs& In, const std::vector<float>& TrackD, FOutputs& Out)
	{
		const int NX = In.NX, NY = In.NY;
		const size_t N = size_t(NX) * NY;
		std::vector<float> F(N);
		for (size_t I = 0; I < N; ++I) F[I] = In.Kind[I] == Land ? In.Forest[I] / 255.f : 0.f;
		F = Blur(F, NX, NY, 2, 2);
		const FNoise Bumps(In.Seed * 2654435761u + 101u);
		constexpr float Cell = 20.f;
		int Throws = 0;
		float Highest = 0.f;
		for (int Y = 1; Y < NY - 1; ++Y)
			for (int X = 1; X < NX - 1; ++X)
			{
				const size_t I = size_t(Y) * NX + X;
				if (In.Hole[I] || In.Protect[I] == 255) continue;
				const float GX = (Out.Height[I + 1] - Out.Height[I - 1]) * 0.5f, GY = (Out.Height[I + NX] - Out.Height[I - NX]) * 0.5f;
				const float W = Smooth(0.25f, 0.8f, F[I]) * (1.f - In.Protect[I] / 255.f) * Smooth(2.f, 5.f, TrackD[I])
					* (1.f - Smooth(0.45f, 0.8f, std::sqrt(GX * GX + GY * GY))) * (1.f - In.Cliff[I] / 255.f);
				if (W <= 0.f) continue;
				const double WX = In.X0 + X, WY = In.Y0 + Y;
				// hummocks: only the tops of a noise field rise, so they stand apart with flat floor between
				const float B = Bumps.Fbm(WX, WY, 8.f, 2);
				const float Hummock = Smooth(0.1f, 0.42f, B) * 0.42f;
				const float Hollow = Smooth(0.25f, 0.55f, -Bumps.Fbm(WX + 913.0, WY - 377.0, 9.f, 2)) * 0.14f;
				// windthrows: one at most per 20 m cell, the pit and the mound side by side along the tree's fall
				float Throw = 0.f;
				const int CX = int(std::floor(WX / Cell)), CY = int(std::floor(WY / Cell));
				for (int DY = -1; DY <= 1; ++DY)
					for (int DX = -1; DX <= 1; ++DX)
					{
						const int KX = CX + DX, KY = CY + DY;
						if (CellRand(KX, KY, In.Seed, 1) > 0.3f) continue;
						const float PX = (KX + 0.2f + 0.6f * CellRand(KX, KY, In.Seed, 2)) * Cell, PY = (KY + 0.2f + 0.6f * CellRand(KX, KY, In.Seed, 3)) * Cell;
						const float A = CellRand(KX, KY, In.Seed, 4) * 6.2832f, Size = 0.75f + 0.5f * CellRand(KX, KY, In.Seed, 5);
						const float DirX = std::cos(A), DirY = std::sin(A);
						const float RX = float(WX - PX), RY = float(WY - PY);
						const float U = (RX * DirX + RY * DirY) / Size, V = (-RX * DirY + RY * DirX) / Size;
						const float Pit = std::exp(-((U + 0.9f) * (U + 0.9f) / 0.7f + V * V / 1.1f));
						const float Mound = std::exp(-((U - 1.0f) * (U - 1.0f) / 0.45f + V * V / 0.9f));
						Throw += (Mound * 0.55f - Pit * 0.45f) * Size;
					}
				const float D = std::clamp(Hummock - Hollow + Throw, -0.55f, 0.75f) * W;
				Out.Height[I] += D;
				Highest = std::max(Highest, D);
				Throws += Throw > 0.3f;
			}
		char Buf[160];
		std::snprintf(Buf, sizeof(Buf), "forest floor: hummocks up to %.2f m, ~%d windthrow mound vertices\n", Highest, Throws);
		Out.Report += Buf;
	}
}

void Generate(const FInputs& In, FOutputs& Out)
{
	const int NX = In.NX, NY = In.NY;
	const size_t N = size_t(NX) * NY;
	const FNoise Noise(In.Seed), Noise2(In.Seed * 7919u + 13u), Noise3(In.Seed * 104729u + 7u);
	auto MX = [&](int X) { return In.X0 + X; };
	auto MY = [&](int Y) { return In.Y0 + Y; };

	// how far each vertex is from the beach and the sea: the land calms down toward the shore
	std::vector<uint8_t> NotLand(N);
	for (size_t I = 0; I < N; ++I) NotLand[I] = In.Kind[I] != Land;
	const std::vector<float> ToShore = Distance(NX, NY, NotLand, 1000.f);

	// macro slope (rise per metre) to keep cliffs clean and steer the detail
	std::vector<float> MacroSlope(N, 0.f);
	for (int Y = 1; Y < NY - 1; ++Y)
		for (int X = 1; X < NX - 1; ++X)
		{
			const size_t I = size_t(Y) * NX + X;
			const float GX = (In.Height[I + 1] - In.Height[I - 1]) * 0.5f, GY = (In.Height[I + NX] - In.Height[I - NX]) * 0.5f;
			MacroSlope[I] = std::sqrt(GX * GX + GY * GY);
		}

	// --- natural detail: rolling land, small rises and dips, hummocks, dunes ---
	std::vector<float> Free(N);
	Out.Height.resize(N);
	for (int Y = 0; Y < NY; ++Y)
		for (int X = 0; X < NX; ++X)
		{
			const size_t I = size_t(Y) * NX + X;
			const double WX = MX(X), WY = MY(Y);
			const float P = In.Protect[I] / 255.f;
			float H = In.Height[I];
			if (In.Kind[I] == Land)
			{
				const float Rough = 0.35f + 0.65f * Smooth(-0.35f, 0.45f, Noise2.Fbm(WX, WY, 420.f, 2));
				const float Calm = Smooth(4.f, 45.f, ToShore[I]);               // no bumps right at the beach
				const float Cliff = 1.f - 0.7f * Smooth(0.45f, 0.9f, MacroSlope[I]);
				const float Rolling = Noise.Fbm(WX, WY, 320.f, 4) * 6.f * Cliff;
				const float Mid = Noise.Fbm(WX + 1000.0, WY - 500.0, 60.f, 3) * 3.f * Cliff;
				const float Knoll = Noise2.Fbm(WX + 4000.0, WY + 2500.0, 26.f, 2) * 1.3f * Cliff;
				const float Small = Noise2.Fbm(WX - 300.0, WY + 800.0, 14.f, 2) * 0.8f;
				const float Ridge = 1.f - std::fabs(Noise3.Fbm(WX, WY, 70.f, 3));
				const float Gully = (Ridge * Ridge * Ridge - 0.35f) * 1.2f * Smooth(0.08f, 0.35f, MacroSlope[I]);
				const float Hummock = Noise3.Fbm(WX * 1.7, WY * 1.7, 9.f, 2) * 0.3f * Rough;
				H += ((Rolling + Mid + Knoll + Small) * Rough * Calm - Gully * Calm + Hummock) * (1.f - P);
			}
			else if (In.Kind[I] == Beach)
			{
				// low dunes on the dry upper beach, ridges along the shore
				const float Up = Smooth(0.6f, 1.6f, In.Height[I] - In.SeaZ);
				const float R = 1.f - std::fabs(Noise.Fbm(WX * 2.2, WY * 0.55, 20.f, 2));
				H += (R * R * R * 0.9f + Noise3.Fbm(WX, WY, 6.f, 2) * 0.12f) * Up * (1.f - P);
			}
			else
			{
				H += Noise.Fbm(WX, WY, 30.f, 2) * 0.25f;                        // sand ripples under water
			}
			Out.Height[I] = H;
			Free[I] = In.Kind[I] == Land ? (1.f - P) * Smooth(2.f, 15.f, ToShore[I]) : 0.f;
		}

	// --- the coastal bluff: gullies cut down its face, slump benches, landslide scars with their debris,
	// rills, scree and sand fans at the foot of every gully, the top edge bitten back by the gully heads ---
	std::vector<float> CliffFace(N, 0.f), Fan(N, 0.f), Scar(N, 0.f);
	{
		const float Foot = In.SeaZ + 1.5f;
		const std::vector<float> Top = Dilate(In.Height, NX, NY, 60);
		const std::vector<float> Soft = Blur(In.Height, NX, NY, 4, 2);
		std::vector<uint8_t> FaceSeed(N);
		for (size_t I = 0; I < N; ++I)
		{
			const float C = std::max(In.Cliff[I] / 255.f, Smooth(0.55f, 1.1f, MacroSlope[I]));
			CliffFace[I] = In.Kind[I] == Land ? C * (1.f - In.Protect[I] / 255.f) : 0.f;
			FaceSeed[I] = CliffFace[I] > 0.4f;
		}
		const std::vector<float> ToFace = Distance(NX, NY, FaceSeed, 40.f);
		int Touched = 0;
		float Deepest = 0.f;
		for (int Y = 2; Y < NY - 2; ++Y)
			for (int X = 2; X < NX - 2; ++X)
			{
				const size_t I = size_t(Y) * NX + X;
				if (ToFace[I] >= 25.f || In.Kind[I] == Seabed || In.Hole[I] || In.Protect[I] == 255) continue;
				// local frame: V down the bluff, U along it
				float GX = (Soft[I + 1] - Soft[I - 1]) * 0.5f, GY = (Soft[I + NX] - Soft[I - NX]) * 0.5f;
				const float GL = std::sqrt(GX * GX + GY * GY);
				if (GL < 1e-4f) continue;
				GX /= GL; GY /= GL;
				const double WX = MX(X), WY = MY(Y);
				const double U = -WX * GY + WY * GX, V = WX * GX + WY * GY;
				const float Span = std::max(Top[I] - Foot, 6.f);
				const float TH = (In.Height[I] - Foot) / Span;             // 0 at the foot, 1 at the top edge
				const float Near = 1.f - Smooth(6.f, 25.f, ToFace[I]);      // fades out away from the face
				const float Face = std::max(CliffFace[I], Near * 0.6f);
				// gullies: narrow V valleys running straight down, a few metres deep, their heads biting the edge
				float G = 0.f, A = 1.f, Nrm = 0.f;
				for (int O = 0; O < 3; ++O)
				{
					G += A * Noise3.Get(float(std::fmod(U / (16.0 / (1 << O)), 256.0)), float(std::fmod(V / (90.0 / (1 << O)), 256.0)) + O * 31.7f);
					Nrm += A;
					A *= 0.5f;
				}
				G /= Nrm;
				const float R = 1.f - std::min(std::fabs(G) * 1.8f, 1.f);
				const float Strength = 0.5f + 0.5f * Smooth(-0.3f, 0.4f, Noise2.Get(float(std::fmod(U / 140.0, 256.0)), 3.3f));
				const float Along = Smooth(0.02f, 0.25f, TH) * (1.f - Smooth(1.0f, 1.08f, TH) * (1.f - Near * 0.6f));
				const float GullyDepth = std::pow(R, 3.f) * 8.f * Strength * Along * Face;
				// slump benches: a few bulges and ledges stepping down the face, in stretches of the coast
				const float Slump = Smooth(0.1f, 0.45f, Noise2.Fbm(U, 0.0, 160.f, 2)) * std::sin(TH * 3.14159f * 5.f + Noise.Get(float(std::fmod(U / 40.0, 256.0)), 7.1f) * 2.f)
					* 1.6f * Smooth(0.1f, 0.3f, TH) * (1.f - Smooth(0.85f, 1.f, TH)) * CliffFace[I];
				// landslide scars: the upper face slid away, its debris lies as a lobe at the foot
				const float Slide = Smooth(0.38f, 0.55f, Noise.Fbm(U + 900.0, 0.0, 120.f, 2));
				const float ScarCut = Slide * 3.5f * Smooth(0.5f, 0.85f, TH) * (1.f - Smooth(0.97f, 1.06f, TH)) * Face;
				const float Lobe = Slide * 3.f * Smooth(-0.12f, 0.05f, TH) * (1.f - Smooth(0.08f, 0.35f, TH)) * Near;
				// fans of scree and sand where each gully opens onto the beach
				const float Mouth = std::pow(1.f - std::min(std::fabs(G) * 2.f, 1.f), 3.f) * Strength;
				const float FanH = Mouth * 2.6f * Smooth(-0.15f, 0.02f, TH) * (1.f - Smooth(0.05f, 0.25f, TH)) * Near;
				// fine rills on the bare face
				const float Rill = Noise2.Get(float(std::fmod(U / 2.5, 256.0)), float(std::fmod(V / 18.0, 256.0))) * 0.3f * CliffFace[I];
				const float D = -GullyDepth + Slump - ScarCut + Lobe + FanH + Rill;
				Out.Height[I] += D;
				Fan[I] = Clamp01(std::max(FanH / 1.2f, Lobe / 2.f));
				Scar[I] = Clamp01(ScarCut / 2.f + GullyDepth / 3.f);
				++Touched;
				Deepest = std::max(Deepest, GullyDepth + ScarCut);
			}
		char Buf[160];
		std::snprintf(Buf, sizeof(Buf), "bluff: %d vertices shaped, deepest gully or scar %.1f m\n", Touched, Deepest);
		Out.Report += Buf;
	}

	// --- erosion: gullies down the slopes, silt in the hollows ---
	std::vector<float> Visits(N, 0.f);
	{
		std::vector<float> Before = Out.Height;
		Erode(Out.Height, NX, NY, Free, In.Seed, int(N / 14), Visits);
		float MaxCut = 0.f, MaxFill = 0.f;
		for (size_t I = 0; I < N; ++I)
		{
			const float D = std::clamp(Out.Height[I] - Before[I], -2.5f, 1.2f) * (Free[I] > 0.f ? 1.f : 0.f);
			Out.Height[I] = Before[I] + D;
			MaxCut = std::min(MaxCut, D);
			MaxFill = std::max(MaxFill, D);
		}
		char Buf[160];
		std::snprintf(Buf, sizeof(Buf), "erosion: %d drops, deepest cut %.2f m, highest fill %.2f m\n", int(N / 14), -MaxCut, MaxFill);
		Out.Report += Buf;
	}
	// a light smoothing pass so the drops' brush marks don't read as pits
	{
		const std::vector<float> S = Blur(Out.Height, NX, NY, 1, 1);
		for (size_t I = 0; I < N; ++I) Out.Height[I] = Lerp(Out.Height[I], S[I], 0.35f * Free[I]);
	}

	// --- dirt tracks: a worn trough, cut and fill along the way ---
	const std::vector<float> TrackD = TrackDistance(In, 8.f);
	{
		const std::vector<float> Smoothed = Blur(Out.Height, NX, NY, 3, 3);
		for (size_t I = 0; I < N; ++I)
		{
			if (TrackD[I] >= 8.f || In.Kind[I] != Land) continue;
			const float P = In.Protect[I] / 255.f;
			const float Flat = (1.f - Smooth(2.5f, 7.f, TrackD[I])) * 0.85f;
			const float Trough = (1.f - Smooth(0.5f, 2.6f, TrackD[I])) * 0.16f;
			Out.Height[I] = Lerp(Out.Height[I], Lerp(Smoothed[I], Out.Height[I], P), Flat) - Trough * (1.f - P);
		}
	}
	// --- the forest floor's micro-relief: mossy hummocks and old windthrow pits with their root mounds ---
	ForestFloorRelief(In, TrackD, Out);

	// the portal hole and every protected vertex keep the macro height exactly; over the corridor the
	// ground may rise but not sink
	for (size_t I = 0; I < N; ++I)
	{
		if (In.RaiseOnly[I])
		{
			Out.Height[I] = Lerp(Out.Height[I], std::max(Out.Height[I], In.Height[I]), In.RaiseOnly[I] / 255.f);
		}
		if (In.Protect[I] == 255 || In.Hole[I]) Out.Height[I] = In.Height[I];
	}

	// --- where water gathers ---
	Out.Wetness.assign(N, 0);
	std::vector<float> Wet(N);
	{
		const std::vector<float> V = Blur(Visits, NX, NY, 1, 2);
		for (size_t I = 0; I < N; ++I) Wet[I] = Clamp01(std::log1p(V[I]) / 3.2f);
		for (size_t I = 0; I < N; ++I) Out.Wetness[I] = uint8_t(Wet[I] * 255.f + 0.5f);
	}

	// --- surface layers ---
	for (auto& L : Out.Weights) L.assign(N, 0);
	Out.GrassDensity.assign(N, 0);
	Out.DuneGrass.assign(N, 0);
	Out.ForestFloor.assign(N, 0);
	double Cover[NumLayers] = {};
	for (int Y = 0; Y < NY; ++Y)
		for (int X = 0; X < NX; ++X)
		{
			const size_t I = size_t(Y) * NX + X;
			const double WX = MX(X), WY = MY(Y);
			const int XL = std::max(X - 1, 0), XR = std::min(X + 1, NX - 1), YD = std::max(Y - 1, 0), YU = std::min(Y + 1, NY - 1);
			const float GX = (Out.Height[size_t(Y) * NX + XR] - Out.Height[size_t(Y) * NX + XL]) / float(XR - XL);
			const float GY = (Out.Height[size_t(YU) * NX + X] - Out.Height[size_t(YD) * NX + X]) / float(YU - YD);
			const float Slope = std::atan(std::sqrt(GX * GX + GY * GY)) * 57.2958f;
			const float P = In.Protect[I] / 255.f;
			const float Fine = Noise2.Fbm(WX, WY, 7.f, 2);
			const float Patch = Noise3.Fbm(WX + 300.0, WY, 26.f, 3) + 0.25f * Fine;

			float SandV = In.Kind[I] != Land ? 1.f : std::max((1.f - Smooth(0.f, 3.5f + 2.f * Fine, ToShore[I])) * 0.9f, Fan[I] * 0.75f);
			// bare clay on the bluff wherever it is steep, in fresh scars and gullies; grass holds on the gentler bits
			const float RockV = std::max({Smooth(40.f, 52.f, Slope + Fine * 7.f), CliffFace[I] * Smooth(30.f, 42.f, Slope + Fine * 8.f),
			                              Scar[I] * 0.9f});
			const float Steep = Smooth(33.f, 43.f, Slope + Fine * 6.f);
			const float Bare = Smooth(0.2f, 0.34f, Patch) * (1.f - P);                // bald patches in the grass
			const float Gully = Smooth(0.45f, 0.8f, Wet[I]) * Smooth(6.f, 14.f, Slope);
			const float TrackEdge = 1.f - Smooth(1.1f, 2.6f + Fine * 1.2f, TrackD[I]);
			const float TrackCore = 1.f - Smooth(0.2f, 1.2f, TrackD[I]);
			const float Hollow = Smooth(0.88f, 0.99f, Wet[I]) * (1.f - Smooth(2.f, 6.f, Slope));
			const float DirtV = std::max({Steep, Bare, Gully * 0.45f, TrackEdge * 0.9f});
			const float MudV = std::max(TrackCore * (0.55f + 0.45f * Wet[I]), Hollow * 0.8f) * (1.f - P);
			const float ForestV = In.Forest[I] / 255.f;
			const float MeadowV = Smooth(0.0f, 0.42f, Noise.Fbm(WX - 700.0, WY + 200.0, 70.f, 3) + 0.25f * (1.f - Wet[I]) - 0.1f) * 0.85f;

			float W[NumLayers] = {};
			float Rem = 1.f;
			auto Take = [&](ELayer L, float V) { W[L] = Rem * Clamp01(V); Rem -= W[L]; };
			Take(ELayer::Sand, SandV);
			Take(ELayer::Rock, In.Kind[I] == Land ? RockV : 0.f);
			Take(ELayer::Mud, In.Kind[I] == Land ? MudV : 0.f);
			Take(ELayer::Dirt, In.Kind[I] == Land ? DirtV : 0.f);
			Take(ELayer::Forest, ForestV);
			Take(ELayer::Meadow, MeadowV);
			W[Grass] += Rem;

			// to bytes summing to 255
			int Sum = 0, Big = 0;
			uint8_t B[NumLayers];
			for (int L = 0; L < NumLayers; ++L)
			{
				B[L] = uint8_t(std::lround(W[L] * 255.f));
				Sum += B[L];
				if (W[L] > W[Big]) Big = L;
			}
			B[Big] = uint8_t(std::clamp(int(B[Big]) + 255 - Sum, 0, 255));
			for (int L = 0; L < NumLayers; ++L)
			{
				Out.Weights[L][I] = B[L];
				Cover[L] += B[L];
			}

			const float Clump = 0.6f + 0.4f * Smooth(-0.3f, 0.3f, Noise.Fbm(WX, WY, 3.5f, 2));
			const float G = (W[Grass] + W[ELayer::Meadow] * 0.9f + W[ELayer::Forest] * 0.25f + W[ELayer::Dirt] * 0.06f) * Clump;
			Out.GrassDensity[I] = uint8_t(Clamp01(G) * 255.f + 0.5f);
			const float Z = Out.Height[I] - In.SeaZ;
			const float Dune = W[ELayer::Sand] * Smooth(0.9f, 1.6f, Z) * Smooth(-0.15f, 0.3f, Noise2.Fbm(WX, WY, 6.f, 2));
			Out.DuneGrass[I] = uint8_t(Clamp01(Dune) * 255.f + 0.5f);
			const float Floor = ForestV * (1.f - P) * Smooth(1.5f, 3.5f, TrackD[I]) * (1.f - Clamp01(RockV)) * (1.f - Clamp01(SandV)) * (1.f - Steep);
			Out.ForestFloor[I] = In.Kind[I] == Land ? uint8_t(Clamp01(Floor) * 255.f + 0.5f) : 0;
		}

	float Lo = 1e9f, Hi = -1e9f;
	for (float H : Out.Height) { Lo = std::min(Lo, H); Hi = std::max(Hi, H); }
	char Buf[256];
	std::snprintf(Buf, sizeof(Buf), "heights %.1f..%.1f m on %d x %d, %zu tracks; cover:", Lo, Hi, NX, NY, In.Tracks.size());
	Out.Report += Buf;
	for (int L = 0; L < NumLayers; ++L)
	{
		std::snprintf(Buf, sizeof(Buf), " %s %.1f%%", LayerNames[L], Cover[L] / (255.0 * N) * 100.0);
		Out.Report += Buf;
	}
}
}
