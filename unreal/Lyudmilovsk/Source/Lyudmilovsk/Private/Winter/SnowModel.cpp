#include "SnowModel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <unordered_map>

namespace LyudWinter
{
	namespace
	{
		constexpr double WinterPi = 3.14159265358979323846;
		inline float Sat(float V) { return V < 0.f ? 0.f : (V > 1.f ? 1.f : V); }
		inline float SmoothStepW(float A, float B, float V)
		{
			const float T = Sat((V - A) / (B - A));
			return T * T * (3.f - 2.f * T);
		}
		/** Shelter (Sx, degrees) -> how much of the level snow a cell keeps once drifted. */
		inline float ShelterGain(float SxDeg)
		{
			return SxDeg >= 0.f ? std::min(1.f + 0.10f * SxDeg, 5.f) : std::max(1.f + 0.06f * SxDeg, 0.15f);
		}
		/** Rounds the deepest drifts off toward MaxM instead of cutting them flat. */
		inline float SoftClamp(float D, float MaxM)
		{
			const float Knee = 0.6f * MaxM, Room = MaxM - Knee;
			return D <= Knee ? D : Knee + Room * (1.f - std::exp(-(D - Knee) / Room));
		}
		// the upwind distances the shelter looks at, m
		const float ShelterRanges[] = { 2.f, 3.f, 4.f, 6.f, 8.f, 11.f, 14.f, 18.f, 23.f, 29.f, 36.f, 45.f, 56.f, 70.f, 85.f, 100.f };
	}

	void Serial(int N, const std::function<void(int)>& Body)
	{
		for (int I = 0; I < N; I++)
		{
			Body(I);
		}
	}

	// ---------------------------------------------------------------------------------------------------------- noise

	float Hash01(int32_t X, int32_t Y, uint32_t Seed)
	{
		uint32_t H = uint32_t(X) * 0x8da6b343u ^ uint32_t(Y) * 0xd8163841u ^ Seed * 0xcb1ab31fu;
		H ^= H >> 13;
		H *= 0x5bd1e995u;
		H ^= H >> 15;
		return float(H & 0xffffffu) / 16777216.f;
	}

	float ValueNoise(double XM, double YM, double ScaleM, uint32_t Seed)
	{
		const float X = float(XM / ScaleM), Y = float(YM / ScaleM);
		const float FX0 = std::floor(X), FY0 = std::floor(Y);
		const int32_t IX = int32_t(FX0), IY = int32_t(FY0);
		float FX = X - FX0, FY = Y - FY0;
		FX = FX * FX * (3.f - 2.f * FX);
		FY = FY * FY * (3.f - 2.f * FY);
		const float A = Hash01(IX, IY, Seed), B = Hash01(IX + 1, IY, Seed);
		const float C = Hash01(IX, IY + 1, Seed), D = Hash01(IX + 1, IY + 1, Seed);
		return (A + (B - A) * FX) + ((C + (D - C) * FX) - (A + (B - A) * FX)) * FY;
	}

	float Fbm(double XM, double YM, double ScaleM, uint32_t Seed)
	{
		return 0.5f * ValueNoise(XM, YM, ScaleM, Seed) + 0.3f * ValueNoise(XM, YM, ScaleM * 0.47, Seed + 1u) +
		       0.2f * ValueNoise(XM, YM, ScaleM * 0.21, Seed + 2u);
	}

	void UpwindDir(float FromDeg, double& OutX, double& OutY)
	{
		const double A = FromDeg * WinterPi / 180.0;
		OutX = -std::sin(A); // east = -X
		OutY = std::cos(A);  // north = +Y
	}

	float AddToRose(float Rose[8], float FromDeg, float SpeedMS, float Weight)
	{
		// loose snow starts to move at ~5 m/s and the load grows with the cube of the excess
		const float Excess = std::max(SpeedMS - 5.f, 0.f);
		const float Power = Weight * Excess * Excess * Excess;
		if (Power <= 0.f)
		{
			return 0.f;
		}
		float S = std::fmod(FromDeg, 360.f);
		if (S < 0.f)
		{
			S += 360.f;
		}
		S /= 45.f;
		const int A = int(std::floor(S)) % 8, B = (A + 1) % 8;
		const float T = S - std::floor(S);
		Rose[A] += Power * (1.f - T);
		Rose[B] += Power * T;
		return Power;
	}

	// ---------------------------------------------------------------------------------------------------------- cover

	void FCover::Init(int InW, int InH, double InX0, double InY0, float InCell)
	{
		W = InW;
		H = InH;
		X0 = InX0;
		Y0 = InY0;
		Cell = InCell;
		Ground.assign(size_t(W) * H, NoGround);
		Top.assign(size_t(W) * H, NoGround);
		Kind.assign(size_t(W) * H, CellNone);
	}

	void FCover::Classify(float SeaZ)
	{
		for (int I = 0; I < W * H; I++)
		{
			const float G = Ground[I];
			if (G == NoGround || G < SeaZ + 30.f)
			{
				Kind[I] = CellNone;
				continue;
			}
			if (Top[I] - G > RoofMinCm)
			{
				Kind[I] = CellRoof;
			}
			else if (Kind[I] != CellForest)
			{
				Kind[I] = CellOpen;
			}
		}
	}

	void FCover::CleanRoofs()
	{
		std::vector<uint8_t> K = Kind;
		for (int Y = 0; Y < H; Y++)
		{
			for (int X = 0; X < W; X++)
			{
				const int I = Index(X, Y);
				if (K[I] != CellRoof)
				{
					continue;
				}
				int N = 0;
				for (int DY = -1; DY <= 1; DY++)
				{
					for (int DX = -1; DX <= 1; DX++)
					{
						const int NX = X + DX, NY = Y + DY;
						if ((DX || DY) && NX >= 0 && NY >= 0 && NX < W && NY < H && K[Index(NX, NY)] == CellRoof)
						{
							N++;
						}
					}
				}
				if (N < 3)
				{
					Kind[I] = CellOpen;
					Top[I] = Ground[I];
				}
			}
		}
	}

	float FCover::GroundAt(double X, double Y) const
	{
		if (!Valid())
		{
			return NoGround;
		}
		const double U = (X - X0) / Cell, V = (Y - Y0) / Cell;
		if (U < -0.5 || V < -0.5 || U > W - 0.5 || V > H - 0.5)
		{
			return NoGround;
		}
		const int IX = std::clamp(int(std::floor(U)), 0, W - 1), IY = std::clamp(int(std::floor(V)), 0, H - 1);
		const int JX = std::min(IX + 1, W - 1), JY = std::min(IY + 1, H - 1);
		const float FX = float(std::clamp(U - IX, 0.0, 1.0)), FY = float(std::clamp(V - IY, 0.0, 1.0));
		float Sum = 0.f, Wt = 0.f;
		const int Ix[4] = { Index(IX, IY), Index(JX, IY), Index(IX, JY), Index(JX, JY) };
		const float Ww[4] = { (1 - FX) * (1 - FY), FX * (1 - FY), (1 - FX) * FY, FX * FY };
		for (int K = 0; K < 4; K++)
		{
			if (Ground[Ix[K]] != NoGround)
			{
				Sum += Ground[Ix[K]] * Ww[K];
				Wt += Ww[K];
			}
		}
		return Wt > 0.f ? Sum / Wt : NoGround;
	}

	uint8_t FCover::KindAt(double X, double Y) const
	{
		const int IX = int(std::lround((X - X0) / Cell)), IY = int(std::lround((Y - Y0) / Cell));
		return IX >= 0 && IY >= 0 && IX < W && IY < H ? Kind[Index(IX, IY)] : CellNone;
	}

	void FCover::MarkForest(double MinX, double MinY, double MaxX, double MaxY)
	{
		const int AX = std::max(0, int(std::ceil((MinX - X0) / Cell))), BX = std::min(W - 1, int(std::floor((MaxX - X0) / Cell)));
		const int AY = std::max(0, int(std::ceil((MinY - Y0) / Cell))), BY = std::min(H - 1, int(std::floor((MaxY - Y0) / Cell)));
		for (int Y = AY; Y <= BY; Y++)
		{
			for (int X = AX; X <= BX; X++)
			{
				uint8_t& K = Kind[Index(X, Y)];
				if (K == CellOpen || K == CellNone)
				{
					K = CellForest;
				}
			}
		}
	}

	uint32_t FCover::Hash() const
	{
		uint32_t H32 = 2166136261u ^ uint32_t(W * 73856093) ^ uint32_t(H * 19349663);
		for (size_t I = 0; I < Ground.size(); I += 7)
		{
			H32 = (H32 ^ uint32_t(int32_t(Ground[I]))) * 16777619u;
			H32 = (H32 ^ uint32_t(int32_t(Top[I]))) * 16777619u;
		}
		return H32;
	}

	bool FCover::Save(const std::string& Path) const
	{
		FILE* F = std::fopen(Path.c_str(), "wb");
		if (!F)
		{
			return false;
		}
		const char Magic[4] = { 'S', 'C', 'V', '1' };
		std::fwrite(Magic, 1, 4, F);
		std::fwrite(&W, sizeof(int), 1, F);
		std::fwrite(&H, sizeof(int), 1, F);
		std::fwrite(&X0, sizeof(double), 1, F);
		std::fwrite(&Y0, sizeof(double), 1, F);
		std::fwrite(&Cell, sizeof(float), 1, F);
		std::fwrite(Ground.data(), sizeof(float), Ground.size(), F);
		std::fwrite(Top.data(), sizeof(float), Top.size(), F);
		std::fwrite(Kind.data(), 1, Kind.size(), F);
		std::fclose(F);
		return true;
	}

	bool FCover::Load(const std::string& Path)
	{
		FILE* F = std::fopen(Path.c_str(), "rb");
		if (!F)
		{
			return false;
		}
		char Magic[4];
		int InW = 0, InH = 0;
		double InX0 = 0, InY0 = 0;
		float InCell = 0;
		bool bOk = std::fread(Magic, 1, 4, F) == 4 && std::memcmp(Magic, "SCV1", 4) == 0 && std::fread(&InW, sizeof(int), 1, F) == 1 &&
		           std::fread(&InH, sizeof(int), 1, F) == 1 && std::fread(&InX0, sizeof(double), 1, F) == 1 &&
		           std::fread(&InY0, sizeof(double), 1, F) == 1 && std::fread(&InCell, sizeof(float), 1, F) == 1 && InW > 0 && InH > 0 &&
		           InW < 100000 && InH < 100000;
		if (bOk)
		{
			Init(InW, InH, InX0, InY0, InCell);
			bOk = std::fread(Ground.data(), sizeof(float), Ground.size(), F) == Ground.size() &&
			      std::fread(Top.data(), sizeof(float), Top.size(), F) == Top.size() && std::fread(Kind.data(), 1, Kind.size(), F) == Kind.size();
		}
		std::fclose(F);
		if (!bOk)
		{
			W = H = 0;
			Ground.clear();
		}
		return bOk;
	}

	// ---------------------------------------------------------------------------------------------------------- drift basis

	void FDriftBasis::Build(const FCover& C, const FParallel& Par)
	{
		W = C.W;
		H = C.H;
		const size_t N = size_t(W) * H;
		for (int D = 0; D < 8; D++)
		{
			Sx[D].assign(N, 0);
			Scoop[D].assign(N, 0);
		}
		SlopeDeg.assign(N, 0);
		const float CellM = C.Cell * 0.01f;
		auto HeightOf = [&C](int I) -> float
		{
			const float G = C.Ground[I];
			if (G == FCover::NoGround)
			{
				return FCover::NoGround;
			}
			return C.Kind[I] == CellForest ? std::max(C.Top[I], G + ForestShelterCm) : C.Top[I];
		};
		double UX[8], UY[8];
		for (int D = 0; D < 8; D++)
		{
			UpwindDir(45.f * D, UX[D], UY[D]);
		}
		Par(H, [&](int Y)
		{
			for (int X = 0; X < W; X++)
			{
				const int I = C.Index(X, Y);
				const uint8_t K = C.Kind[I];
				const float G = C.Ground[I];
				if (G != FCover::NoGround)
				{
					auto GAt = [&](int AX, int AY) { const float V = C.Ground[C.Index(std::clamp(AX, 0, W - 1), std::clamp(AY, 0, H - 1))]; return V == FCover::NoGround ? G : V; };
					const float SX = (GAt(X + 1, Y) - GAt(X - 1, Y)) / (2.f * C.Cell), SY = (GAt(X, Y + 1) - GAt(X, Y - 1)) / (2.f * C.Cell);
					SlopeDeg[I] = uint8_t(std::min(90.0, std::atan(std::sqrt(SX * SX + SY * SY)) * 180.0 / WinterPi));
				}
				if (K != CellOpen && K != CellForest)
				{
					continue;
				}
				const float H0 = G + 50.f;
				for (int D = 0; D < 8; D++)
				{
					float Best = -90.f;
					for (float R : ShelterRanges)
					{
						const int SX = X + int(std::lround(UX[D] * R / CellM)), SY = Y + int(std::lround(UY[D] * R / CellM));
						if (SX < 0 || SY < 0 || SX >= W || SY >= H)
						{
							break;
						}
						const float HS = HeightOf(C.Index(SX, SY));
						if (HS == FCover::NoGround)
						{
							continue;
						}
						Best = std::max(Best, float(std::atan2((HS - H0) * 0.01f, R) * 180.0 / WinterPi));
					}
					Sx[D][I] = int8_t(std::clamp(Best <= -90.f ? 0.f : Best, -90.f, 90.f));
					// a wall just downwind: the wind turns down it and scoops the snow away at its foot
					float Rise = 0.f;
					for (float R : { 2.f, 3.f, 4.f })
					{
						const int SX = X - int(std::lround(UX[D] * R / CellM)), SY = Y - int(std::lround(UY[D] * R / CellM));
						if (SX >= 0 && SY >= 0 && SX < W && SY < H)
						{
							const float HS = HeightOf(C.Index(SX, SY));
							if (HS != FCover::NoGround)
							{
								Rise = std::max(Rise, (HS - H0) * 0.01f);
							}
						}
					}
					Scoop[D][I] = uint8_t(Sat((Rise - 1.5f) / 3.f) * 255.f);
				}
			}
		});
	}

	// ---------------------------------------------------------------------------------------------------------- field

	void FSnowField::Setup(const FCover& C, float InCell, const std::vector<uint8_t>* OpenFine)
	{
		Cell = InCell;
		X0 = C.X0 - C.Cell * 0.5 + Cell * 0.5;
		Y0 = C.Y0 - C.Cell * 0.5 + Cell * 0.5;
		W = int(std::floor(C.W * C.Cell / Cell));
		H = int(std::floor(C.H * C.Cell / Cell));
		Depth.assign(size_t(W) * H, 0.f);
		if (OpenFine && OpenFine->size() == Depth.size())
		{
			Open = *OpenFine;
			return;
		}
		Open.assign(size_t(W) * H, 0);
		for (int Y = 0; Y < H; Y++)
		{
			for (int X = 0; X < W; X++)
			{
				const uint8_t K = C.KindAt(X0 + X * Cell, Y0 + Y * Cell);
				Open[size_t(Y) * W + X] = (K == CellOpen || K == CellForest) ? 1 : 0;
			}
		}
	}

	void FSnowField::ComputeFactor(const FCover& C, const FDriftBasis& B, const FSnowInputs& In, const FParallel& Par, std::vector<float>& Factor)
	{
		const size_t N = size_t(C.W) * C.H;
		std::vector<float> F(N, 0.f);
		float Rose[8];
		float RoseSum = 0.f;
		for (int D = 0; D < 8; D++)
		{
			RoseSum += std::max(In.Rose[D], 0.f);
		}
		for (int D = 0; D < 8; D++)
		{
			Rose[D] = RoseSum > 0.f ? std::max(In.Rose[D], 0.f) / RoseSum : 0.125f;
		}
		Par(C.H, [&](int Y)
		{
			for (int X = 0; X < C.W; X++)
			{
				const int I = C.Index(X, Y);
				const uint8_t K = C.Kind[I];
				if (K != CellOpen && K != CellForest)
				{
					continue;
				}
				float V = 0.f;
				for (int D = 0; D < 8; D++)
				{
					if (Rose[D] > 0.f)
					{
						V += Rose[D] * ShelterGain(B.Sx[D][I]) * (1.f - 0.65f * B.Scoop[D][I] / 255.f);
					}
				}
				F[I] = V;
			}
		});
		// a blur: the wind lays the snow in smooth shapes, not cell by cell
		for (int Pass = 0; Pass < 2; Pass++)
		{
			std::vector<float> G = F;
			Par(C.H, [&](int Y)
			{
				for (int X = 0; X < C.W; X++)
				{
					const int I = C.Index(X, Y);
					const uint8_t K = C.Kind[I];
					if (K != CellOpen && K != CellForest)
					{
						continue;
					}
					float S = 0.f, Wt = 0.f;
					for (int DY = -1; DY <= 1; DY++)
					{
						for (int DX = -1; DX <= 1; DX++)
						{
							const int NX = X + DX, NY = Y + DY;
							if (NX < 0 || NY < 0 || NX >= C.W || NY >= C.H)
							{
								continue;
							}
							const int J = C.Index(NX, NY);
							if (C.Kind[J] == CellOpen || C.Kind[J] == CellForest)
							{
								const float Wk = (DX == 0 && DY == 0) ? 2.f : ((DX == 0 || DY == 0) ? 1.f : 0.6f);
								S += G[J] * Wk;
								Wt += Wk;
							}
						}
					}
					F[I] = Wt > 0.f ? S / Wt : G[I];
				}
			});
		}
		// the snow moved, not made: the drifted factor averages 1 over the open ground
		double Sum = 0.0;
		size_t Count = 0;
		for (size_t I = 0; I < N; I++)
		{
			if (C.Kind[I] == CellOpen || C.Kind[I] == CellForest)
			{
				Sum += F[I];
				Count++;
			}
		}
		const float Mean = Count ? float(Sum / Count) : 1.f;
		const float T = Sat(In.Transport);
		Factor.assign(N, 0.f);
		Par(C.H, [&](int Y)
		{
			for (int X = 0; X < C.W; X++)
			{
				const int I = C.Index(X, Y);
				const uint8_t K = C.Kind[I];
				if (K != CellOpen && K != CellForest)
				{
					continue;
				}
				float V = 1.f + T * (F[I] / std::max(Mean, 0.05f) - 1.f);
				// it slides off steep slopes and the crowns keep some of it up in the trees
				V *= 1.f - SmoothStepW(34.f, 52.f, B.SlopeDeg[I]);
				if (K == CellForest)
				{
					V *= 0.72f;
				}
				Factor[I] = V;
			}
		});
	}

	float FSnowField::FactorAt(const FCover& C, double WX, double WY) const
	{
		if (Factor.empty())
		{
			return 1.f;
		}
		const double U = (WX - C.X0) / C.Cell, V = (WY - C.Y0) / C.Cell;
		const int IX = std::clamp(int(std::floor(U)), 0, C.W - 1), IY = std::clamp(int(std::floor(V)), 0, C.H - 1);
		const int JX = std::min(IX + 1, C.W - 1), JY = std::min(IY + 1, C.H - 1);
		const float FX = float(std::clamp(U - IX, 0.0, 1.0)), FY = float(std::clamp(V - IY, 0.0, 1.0));
		const int Ix[4] = { C.Index(IX, IY), C.Index(JX, IY), C.Index(IX, JY), C.Index(JX, JY) };
		const float Ww[4] = { (1 - FX) * (1 - FY), FX * (1 - FY), (1 - FX) * FY, FX * FY };
		float S = 0.f, Wt = 0.f;
		for (int K = 0; K < 4; K++)
		{
			// only the open neighbours: a drift runs right up to the wall instead of fading into the roof's zero
			if (C.Kind[Ix[K]] == CellOpen || C.Kind[Ix[K]] == CellForest)
			{
				S += Factor[Ix[K]] * Ww[K];
				Wt += Ww[K];
			}
		}
		return Wt > 1e-4f ? S / Wt : 0.f;
	}

	float FSnowField::DriftedM(const FCover& C, const FSnowInputs& In, int X, int Y) const
	{
		const size_t I = size_t(Y) * W + X;
		if (!Open[I] || In.LevelCm <= 0.f)
		{
			return 0.f;
		}
		return SoftClamp(In.LevelCm * 0.01f * FactorAt(C, X0 + X * Cell, Y0 + Y * Cell), In.MaxDepthM);
	}

	void FSnowField::Compose(const FCover& C, const FSnowInputs& In, const FSnowPaths* P, int RX0, int RY0, int RX1, int RY1, const FParallel& Par)
	{
		RX0 = std::max(RX0, 0);
		RY0 = std::max(RY0, 0);
		RX1 = std::min(RX1, W - 1);
		RY1 = std::min(RY1, H - 1);
		if (RX1 < RX0 || RY1 < RY0)
		{
			return;
		}
		const bool bPaths = P && P->Valid() && P->Patch.size() == Depth.size();
		Par(RY1 - RY0 + 1, [&](int Row)
		{
			const int Y = RY0 + Row;
			for (int X = RX0; X <= RX1; X++)
			{
				const size_t I = size_t(Y) * W + X;
				float D = DriftedM(C, In, X, Y);
				if (bPaths && D > 0.f)
				{
					const int Pk = P->Patch[I];
					if (Pk >= 0)
					{
						const FPatch& Pt = P->Patches[Pk];
						const float Along = Pt.bAlongX ? float(X - Pt.MinX + 0.5f) / float(Pt.MaxX - Pt.MinX + 1)
						                               : float(Y - Pt.MinY + 0.5f) / float(Pt.MaxY - Pt.MinY + 1);
						const float OnPath = (Along < Pt.Progress ? Pt.FreshCm : Pt.FreshCm + Pt.FreshBefore) * 0.01f;
						D = std::min(D, OnPath + 0.012f);
					}
					else if (P->Near[I] >= 0)
					{
						const FPatch& Pt = P->Patches[P->Near[I]];
						const float Dist = P->NearDist[I] * 0.1f;
						const float Bump = std::exp(-((Dist - 0.9f) / 0.65f) * ((Dist - 0.9f) / 0.65f));
						D = std::min(D + Bump * Pt.RemovedCm * 0.01f * 0.9f, In.MaxDepthM + 0.25f);
					}
				}
				Depth[I] = D;
			}
		});
		if (!bPaths)
		{
			return;
		}
		// the shovelled walls slump: one smoothing pass round the paths (the ground's 1 m vertices make a cut wall
		// a jagged slab otherwise); a cleared path keeps a packed film of 1-2 cm
		const int SX0 = std::max(RX0 - 1, 0), SY0 = std::max(RY0 - 1, 0), SX1 = std::min(RX1 + 1, W - 1), SY1 = std::min(RY1 + 1, H - 1);
		const int SW = SX1 - SX0 + 1;
		std::vector<float> Copy(size_t(SW) * (SY1 - SY0 + 1));
		for (int Y = SY0; Y <= SY1; Y++)
		{
			std::copy(Depth.begin() + size_t(Y) * W + SX0, Depth.begin() + size_t(Y) * W + SX1 + 1, Copy.begin() + size_t(Y - SY0) * SW);
		}
		auto Cp = [&](int X, int Y) { return Copy[size_t(std::clamp(Y, SY0, SY1) - SY0) * SW + (std::clamp(X, SX0, SX1) - SX0)]; };
		Par(RY1 - RY0 + 1, [&](int Row)
		{
			const int Y = RY0 + Row;
			for (int X = RX0; X <= RX1; X++)
			{
				const size_t I = size_t(Y) * W + X;
				if (!Open[I] || (P->Patch[I] < 0 && (P->Near[I] < 0 || P->NearDist[I] > 15)))
				{
					continue;
				}
				const float V = 0.4f * Cp(X, Y) + 0.1f * (Cp(X - 1, Y) + Cp(X + 1, Y) + Cp(X, Y - 1) + Cp(X, Y + 1)) +
				                0.05f * (Cp(X - 1, Y - 1) + Cp(X + 1, Y - 1) + Cp(X - 1, Y + 1) + Cp(X + 1, Y + 1));
				Depth[I] = P->Patch[I] >= 0 ? std::min(V, Cp(X, Y) + 0.008f) : V;
			}
		});
	}

	float FSnowField::DepthAt(double X, double Y) const
	{
		if (!Valid())
		{
			return 0.f;
		}
		const double U = (X - X0) / Cell, V = (Y - Y0) / Cell;
		if (U < 0.0 || V < 0.0 || U > W - 1 || V > H - 1)
		{
			return 0.f;
		}
		const int IX = std::min(int(U), W - 2), IY = std::min(int(V), H - 2);
		const float FX = float(U - IX), FY = float(V - IY);
		const float A = Depth[size_t(IY) * W + IX], B = Depth[size_t(IY) * W + IX + 1];
		const float Cc = Depth[size_t(IY + 1) * W + IX], Dd = Depth[size_t(IY + 1) * W + IX + 1];
		return (A + (B - A) * FX) * (1 - FY) + (Cc + (Dd - Cc) * FX) * FY;
	}

	// ---------------------------------------------------------------------------------------------------------- paths

	void FSnowPaths::Build(int W, int H, double X0, double Y0, float Cell, const std::vector<uint8_t>& Mask)
	{
		const size_t N = size_t(W) * H;
		Patch.assign(N, -1);
		Near.assign(N, -1);
		NearDist.assign(N, 255);
		Patches.clear();
		if (Mask.size() != N)
		{
			return;
		}
		const int Tile = std::max(1, int(std::lround(PatchM * 100.f / Cell)));
		std::unordered_map<int64_t, int> ByTile;
		std::vector<double> SX, SY;
		for (int Y = 0; Y < H; Y++)
		{
			for (int X = 0; X < W; X++)
			{
				const size_t I = size_t(Y) * W + X;
				if (!Mask[I])
				{
					continue;
				}
				const int64_t Key = int64_t(Y / Tile) * 1000003 + X / Tile;
				auto It = ByTile.find(Key);
				int Pk;
				if (It == ByTile.end())
				{
					Pk = int(Patches.size());
					ByTile.emplace(Key, Pk);
					FPatch Pt;
					Pt.MinX = Pt.MaxX = X;
					Pt.MinY = Pt.MaxY = Y;
					Patches.push_back(Pt);
					SX.push_back(0.0);
					SY.push_back(0.0);
				}
				else
				{
					Pk = It->second;
				}
				FPatch& Pt = Patches[Pk];
				Pt.MinX = std::min(Pt.MinX, X);
				Pt.MaxX = std::max(Pt.MaxX, X);
				Pt.MinY = std::min(Pt.MinY, Y);
				Pt.MaxY = std::max(Pt.MaxY, Y);
				Pt.Cells++;
				SX[Pk] += X;
				SY[Pk] += Y;
				Patch[I] = Pk;
			}
		}
		for (size_t Pk = 0; Pk < Patches.size(); Pk++)
		{
			FPatch& Pt = Patches[Pk];
			Pt.X = X0 + SX[Pk] / Pt.Cells * Cell;
			Pt.Y = Y0 + SY[Pk] / Pt.Cells * Cell;
			Pt.bAlongX = (Pt.MaxX - Pt.MinX) >= (Pt.MaxY - Pt.MinY);
		}
		// the berm cells: the nearest path (a chamfer distance carrying the patch along), within BermReachM
		std::vector<float> Dist(N, 1e9f);
		for (size_t I = 0; I < N; I++)
		{
			if (Patch[I] >= 0)
			{
				Dist[I] = 0.f;
				Near[I] = Patch[I];
			}
		}
		const float Diag = 1.41421356f;
		auto Relax = [&](int X, int Y, int NX, int NY, float Step)
		{
			if (NX < 0 || NY < 0 || NX >= W || NY >= H)
			{
				return;
			}
			const size_t I = size_t(Y) * W + X, J = size_t(NY) * W + NX;
			if (Dist[J] + Step < Dist[I])
			{
				Dist[I] = Dist[J] + Step;
				Near[I] = Near[J];
			}
		};
		for (int Y = 0; Y < H; Y++)
		{
			for (int X = 0; X < W; X++)
			{
				Relax(X, Y, X - 1, Y, 1.f);
				Relax(X, Y, X - 1, Y - 1, Diag);
				Relax(X, Y, X, Y - 1, 1.f);
				Relax(X, Y, X + 1, Y - 1, Diag);
			}
		}
		for (int Y = H - 1; Y >= 0; Y--)
		{
			for (int X = W - 1; X >= 0; X--)
			{
				Relax(X, Y, X + 1, Y, 1.f);
				Relax(X, Y, X + 1, Y + 1, Diag);
				Relax(X, Y, X, Y + 1, 1.f);
				Relax(X, Y, X - 1, Y + 1, Diag);
			}
		}
		const float Reach = BermReachM * 100.f / Cell;
		for (size_t I = 0; I < N; I++)
		{
			if (Patch[I] >= 0 || Dist[I] > Reach)
			{
				Near[I] = Patch[I] >= 0 ? Patch[I] : -1;
				NearDist[I] = Patch[I] >= 0 ? 0 : 255;
				continue;
			}
			// distance from the path's edge (a path cell's centre is half a cell inside it)
			NearDist[I] = uint8_t(std::clamp((Dist[I] - 0.5f) * Cell * 0.1f, 0.f, 254.f));
		}
	}

	int FSnowPaths::PatchAt(double X, double Y, int W, int H, double X0, double Y0, float Cell) const
	{
		const int IX = int(std::lround((X - X0) / Cell)), IY = int(std::lround((Y - Y0) / Cell));
		if (IX < 0 || IY < 0 || IX >= W || IY >= H || Patch.empty())
		{
			return -1;
		}
		return Patch[size_t(IY) * W + IX];
	}

	void StepPaths(FSnowPaths& P, const FSnowInputs& In, float PrevLevelCm, double NowHours, float Hours, double CamX, double CamY,
	               float AutoClearHours, float AutoClearNearM, std::vector<int>& Changed)
	{
		Changed.clear();
		const float Ratio = PrevLevelCm > 0.5f ? std::min(1.f, In.LevelCm / PrevLevelCm) : 1.f;
		for (int Pk = 0; Pk < int(P.Patches.size()); Pk++)
		{
			FPatch& Pt = P.Patches[Pk];
			const float Old = Pt.FreshCm + Pt.FreshBefore, OldRemoved = Pt.RemovedCm;
			if (In.LevelCm < 0.5f)
			{
				Pt.FreshCm = Pt.FreshBefore = Pt.RemovedCm = 0.f;
				Pt.SnowSinceHours = -1.0;
			}
			else
			{
				Pt.FreshCm = std::min((Pt.FreshCm + In.SnowfallCmPerHour * Hours) * Ratio, In.LevelCm * 1.5f);
				Pt.FreshBefore *= Ratio;
				Pt.RemovedCm *= Ratio;
				if (Pt.FreshCm + Pt.FreshBefore > 1.f && Pt.SnowSinceHours < 0.0)
				{
					Pt.SnowSinceHours = NowHours;
				}
				// somebody we don't see clears it: far from the camera, some hours after the snow began to lie
				if (Pt.Claimed < 0 && Pt.Progress >= 1.f && Pt.FreshCm >= 2.f && Pt.SnowSinceHours >= 0.0)
				{
					const double Due = AutoClearHours * (0.6 + 0.8 * Hash01(Pk, 17, 1947u));
					const double Since = NowHours - Pt.SnowSinceHours;
					const double DX = (Pt.X - CamX) * 0.01, DY = (Pt.Y - CamY) * 0.01;
					const bool bFar = DX * DX + DY * DY > double(AutoClearNearM) * AutoClearNearM;
					if (Since > Due && (bFar || Since > Due * 3.0))
					{
						Pt.RemovedCm += Pt.FreshCm;
						Pt.FreshCm = 0.f;
						Pt.SnowSinceHours = -1.0;
					}
				}
			}
			if (std::fabs(Pt.FreshCm + Pt.FreshBefore - Old) > 0.5f || std::fabs(Pt.RemovedCm - OldRemoved) > 0.5f)
			{
				Changed.push_back(Pk);
			}
		}
	}

	void SettlePaths(FSnowPaths& P, const FSnowInputs& In, double NowHours)
	{
		const bool bSnowing = In.SnowfallCmPerHour > 0.05f;
		for (FPatch& Pt : P.Patches)
		{
			Pt.FreshBefore = 0.f;
			Pt.Progress = 1.f;
			Pt.Claimed = -1;
			Pt.FreshCm = bSnowing ? std::min(In.LevelCm, 2.f) : 0.f;
			Pt.RemovedCm = In.LevelCm;
			Pt.SnowSinceHours = Pt.FreshCm > 0.f ? NowHours : -1.0;
		}
	}

	// ---------------------------------------------------------------------------------------------------------- ice

	void DistanceTransform(std::vector<float>& G, int W, int H, const FParallel& Par)
	{
		// Felzenszwalb & Huttenlocher's lower envelope of parabolas, in doubles (the seeds' "infinity" is 1e20)
		auto Line = [](float* F, int N, int Stride, std::vector<double>& D, std::vector<int>& V, std::vector<double>& Z)
		{
			const double Inf = std::numeric_limits<double>::infinity();
			D.resize(N);
			V.resize(N);
			Z.resize(N + 1);
			int K = 0;
			V[0] = 0;
			Z[0] = -Inf;
			Z[1] = Inf;
			for (int Q = 1; Q < N; Q++)
			{
				const double FQ = double(F[size_t(Q) * Stride]) + double(Q) * Q;
				double S = (FQ - (double(F[size_t(V[K]) * Stride]) + double(V[K]) * V[K])) / (2.0 * (Q - V[K]));
				while (S <= Z[K])
				{
					K--;
					S = (FQ - (double(F[size_t(V[K]) * Stride]) + double(V[K]) * V[K])) / (2.0 * (Q - V[K]));
				}
				K++;
				V[K] = Q;
				Z[K] = S;
				Z[K + 1] = Inf;
			}
			K = 0;
			for (int Q = 0; Q < N; Q++)
			{
				while (Z[K + 1] < Q)
				{
					K++;
				}
				const double Dq = double(Q - V[K]);
				D[Q] = Dq * Dq + F[size_t(V[K]) * Stride];
			}
			for (int Q = 0; Q < N; Q++)
			{
				F[size_t(Q) * Stride] = float(std::min(D[Q], 1e20));
			}
		};
		Par(W, [&](int X)
		{
			std::vector<double> D, Z;
			std::vector<int> V;
			Line(G.data() + X, H, W, D, V, Z);
		});
		Par(H, [&](int Y)
		{
			std::vector<double> D, Z;
			std::vector<int> V;
			Line(G.data() + size_t(Y) * W, W, 1, D, V, Z);
		});
	}

	void FIceMask::Init(int InW, int InH, double InX0, double InY0, float InCell)
	{
		W = InW;
		H = InH;
		X0 = InX0;
		Y0 = InY0;
		Cell = InCell;
		Bed.assign(size_t(W) * H, FCover::NoGround);
		Dist.assign(size_t(W) * H, 1e4f);
		BuiltSeaZ = -1.0e9f;
	}

	void FIceMask::ComputeDist(float SeaZ, float ShallowM, const FParallel& Par, std::vector<float>& Out) const
	{
		if (W <= 0)
		{
			return;
		}
		std::vector<float> G(size_t(W) * H);
		const float Limit = SeaZ - ShallowM * 100.f;
		for (size_t I = 0; I < G.size(); I++)
		{
			// the shore (land, the beach, the tidal flat out of the water), the bank and the island are the seeds
			G[I] = (Bed[I] != FCover::NoGround && Bed[I] > Limit) ? 0.f : 1e20f;
		}
		DistanceTransform(G, W, H, Par);
		const float CellM = Cell * 0.01f;
		Out.resize(G.size());
		for (size_t I = 0; I < G.size(); I++)
		{
			Out[I] = G[I] >= 1e19f ? 1e4f : std::sqrt(G[I]) * CellM;
		}
	}

	float FIceMask::DistAt(double X, double Y) const
	{
		if (!Valid())
		{
			return 1e4f;
		}
		const double U = (X - X0) / Cell, V = (Y - Y0) / Cell;
		if (U < 0.0 || V < 0.0 || U > W - 1 || V > H - 1)
		{
			return 1e4f;
		}
		const int IX = std::min(int(U), W - 2), IY = std::min(int(V), H - 2);
		const float FX = float(U - IX), FY = float(V - IY);
		const float A = Dist[size_t(IY) * W + IX], B = Dist[size_t(IY) * W + IX + 1];
		const float C = Dist[size_t(IY + 1) * W + IX], D = Dist[size_t(IY + 1) * W + IX + 1];
		return (A + (B - A) * FX) * (1 - FY) + (C + (D - C) * FX) * FY;
	}

	float FIceMask::IceAt(double X, double Y, const FIceParams& P) const
	{
		if (P.ThicknessCm < 1.f || P.Cover <= 0.f)
		{
			return 0.f;
		}
		float D = DistAt(X, Y);
		if (P.RailX1 > P.RailX0 && X >= P.RailX0 && X <= P.RailX1)
		{
			D = std::min(D, std::max(float(std::fabs(Y - P.RailY) * 0.01) - P.RailHalfWidthM, 0.f));
		}
		const double XM = X * 0.01, YM = Y * 0.01;
		const float Edge = P.OpenStripM + P.StripNoiseM * (2.f * Fbm(XM, YM, 9.0, P.Seed) - 1.f);
		// pancakes float in the first 4 m past the edge (drawn only); the solid ice starts beyond them
		const float Rim = Sat((D - Edge - 4.f) / 1.5f);
		if (Rim <= 0.f)
		{
			return 0.f;
		}
		float Floe = 1.f;
		if (P.Cover < 0.97f)
		{
			const float N = Fbm(XM - P.DriftX * 0.01, YM - P.DriftY * 0.01, 60.0, P.Seed + 7u);
			Floe = Sat((0.2f + 0.6f * P.Cover - N) / 0.03f);
		}
		return Rim * Floe;
	}

	bool FIceMask::Walkable(double X, double Y, const FIceParams& P, float MinCm) const
	{
		return P.ThicknessCm >= MinCm && IceAt(X, Y, P) >= 0.99f;
	}
}
