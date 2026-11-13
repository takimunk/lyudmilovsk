#include "TopiModel.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace LyudTopi
{
	namespace
	{
		double Clamp01(double T) { return T < 0.0 ? 0.0 : (T > 1.0 ? 1.0 : T); }
		double Smooth(double T)
		{
			T = Clamp01(T);
			return T * T * (3.0 - 2.0 * T);
		}
		double Lerp(double A, double B, double T) { return A + (B - A) * T; }

		double Hash01(int64_t I, int64_t J, uint32_t Seed)
		{
			uint32_t H = uint32_t(I) * 73856093u ^ uint32_t(J) * 19349663u ^ Seed * 83492791u;
			H = (H ^ (H >> 13)) * 0x5bd1e995u;
			H ^= H >> 15;
			H *= 0x27d4eb2du;
			H ^= H >> 16;
			return double(H & 0xffffffu) / double(0xffffff);
		}

		/** Value noise 0..1, smooth. */
		double Noise(double X, double Y, uint32_t Seed)
		{
			const double FX = std::floor(X), FY = std::floor(Y);
			const int64_t I = int64_t(FX), J = int64_t(FY);
			double TX = X - FX, TY = Y - FY;
			TX = TX * TX * TX * (TX * (TX * 6.0 - 15.0) + 10.0);
			TY = TY * TY * TY * (TY * (TY * 6.0 - 15.0) + 10.0);
			const double A = Hash01(I, J, Seed), B = Hash01(I + 1, J, Seed), C = Hash01(I, J + 1, Seed), D = Hash01(I + 1, J + 1, Seed);
			return Lerp(Lerp(A, B, TX), Lerp(C, D, TX), TY);
		}

		/** One family of dunes marching down the wind. U: metres down the wind, V: across it. Length: crest to crest,
		 *  Amp: mean height, Lee: where along the dune the crest stands (the rest is the slip face). Returns the height;
		 *  Windward: 1 on the dune's long back, 0 on its slip face. */
		double DuneLayer(double U, double V, double Length, double Amp, double Lee, uint32_t Seed, double* Windward = nullptr)
		{
			const double Phase = U / Length
				+ 1.4 * (Noise(U / (5.0 * Length), V / (4.5 * Length), Seed) - 0.5)
				+ 0.2 * (Noise(U / (1.7 * Length), V / (1.4 * Length), Seed + 1) - 0.5);
			const double FI = std::floor(Phase);
			const double T = Phase - FI;
			// each crest has its own height, and it swells and dies along its length
			const double A = Amp * (0.45 + 0.65 * Noise(FI * 0.731 + 0.19, V / (2.6 * Length), Seed + 2));
			double P;
			if (T < Lee)
			{
				const double R = T / Lee;
				P = 0.55 * R + 0.45 * R * R * (3.0 - 2.0 * R);
			}
			else
			{
				const double Q = (1.0 - T) / (1.0 - Lee);
				P = std::pow(Q, 1.15);
			}
			if (Windward)
			{
				*Windward = 1.0 - Smooth((T - Lee + 0.06) / 0.06) * (1.0 - Smooth((T - 0.93) / 0.07));
			}
			return A * P;
		}

		/** A dune family's mean height (what is left of it where it is too small to draw). */
		double DuneMean(double Amp) { return Amp * 0.775 * 0.5; }

		struct FPathData
		{
			std::vector<FVec2> Points;
			std::vector<double> Along;
			double Length = 0.0;
		};

		const FPathData& PathData()
		{
			static const FPathData Data = []
			{
				FPathData D;
				const double EndX = 150.0;
				D.Points = {
					{ 283.0, -770.0 }, { 296.0, -812.0 }, { 395.0, -850.0 }, { 448.0, -905.0 }, { 470.0, -985.0 }, { 440.0, -1065.0 },
					{ 300.0, -1140.0 }, { 215.0, -1215.0 }, { 150.0, -1290.0 }, { 118.0, -1350.0 }, { 192.0, -1470.0 }, { 128.0, -1590.0 },
					{ 162.0, -1690.0 },
					{ EndX, FModel::RiverY(EndX) + RiverHalfWidth + 6.0 } };
				D.Along.push_back(0.0);
				for (size_t I = 1; I < D.Points.size(); ++I)
				{
					D.Length += std::hypot(D.Points[I].X - D.Points[I - 1].X, D.Points[I].Y - D.Points[I - 1].Y);
					D.Along.push_back(D.Length);
				}
				return D;
			}();
			return Data;
		}
	}

	double FEdgeTable::At(double X, double Y) const
	{
		if (!Valid())
		{
			return SeaZ + 6.0;
		}
		double GX = (X - X0) / Cell, GY = (Y - Y0) / Cell;
		GX = std::min(std::max(GX, 0.0), double(NX - 1) - 1e-6);
		GY = std::min(std::max(GY, 0.0), double(NY - 1) - 1e-6);
		const int I = int(GX), J = int(GY);
		const double TX = GX - I, TY = GY - J;
		const float* R0 = &Z[size_t(J) * NX + I];
		const float* R1 = R0 + NX;
		return Lerp(Lerp(R0[0], R0[1], TX), Lerp(R1[0], R1[1], TX), TY);
	}

	bool FEdgeTable::Read(const uint8_t* Data, size_t Size)
	{
		struct FHeader
		{
			char Magic[4];
			int32_t Version, NX, NY;
			float X0, Y0, Cell;
		};
		FHeader H;
		if (Size < sizeof(H))
		{
			return false;
		}
		std::memcpy(&H, Data, sizeof(H));
		if (std::memcmp(H.Magic, "TOPE", 4) != 0 || H.Version != 1 || H.NX < 2 || H.NY < 2
			|| Size < sizeof(H) + size_t(H.NX) * size_t(H.NY) * sizeof(float))
		{
			return false;
		}
		NX = H.NX;
		NY = H.NY;
		X0 = H.X0;
		Y0 = H.Y0;
		Cell = H.Cell;
		Z.resize(size_t(NX) * size_t(NY));
		std::memcpy(Z.data(), Data + sizeof(H), Z.size() * sizeof(float));
		return true;
	}

	void FModel::Prepare()
	{
		RidgeTops.clear();
		if (!Edge.Valid())
		{
			return;
		}
		// the highest map ground in each column of the strip the great dune covers, widened and smoothed so the sand
		// that buries the cliff lies at a slope sand can hold
		const int N = Edge.NX;
		std::vector<float> Top(N, -1e9f), Wide(N), Soft(N);
		for (int I = 0; I < N; ++I)
		{
			for (int J = 0; J < Edge.NY; ++J)
			{
				const double Y = Edge.Y0 + J * Edge.Cell;
				if (Y >= FullY - 6.0 && Y <= -800.0)
				{
					Top[I] = std::max(Top[I], Edge.Z[size_t(J) * N + I]);
				}
			}
		}
		const int Grow = int(48.0 / Edge.Cell), Blur = int(66.0 / Edge.Cell);
		for (int I = 0; I < N; ++I)
		{
			float M = Top[I];
			for (int K = std::max(I - Grow, 0); K <= std::min(I + Grow, N - 1); ++K)
			{
				M = std::max(M, Top[K]);
			}
			Wide[I] = M;
		}
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			for (int I = 0; I < N; ++I)
			{
				double Sum = 0.0;
				for (int K = I - Blur; K <= I + Blur; ++K)
				{
					Sum += Wide[std::min(std::max(K, 0), N - 1)];
				}
				Soft[I] = float(Sum / (2 * Blur + 1));
			}
			Wide = Soft;
		}
		RidgeTops = Wide;
	}

	double FModel::RidgeTop(double X) const
	{
		if (RidgeTops.empty())
		{
			return SeaZ + 20.0;
		}
		double G = (X - Edge.X0) / Edge.Cell;
		G = std::min(std::max(G, 0.0), double(RidgeTops.size() - 1) - 1e-6);
		const int I = int(G);
		return Lerp(RidgeTops[I], RidgeTops[I + 1], G - I) + 10.0;
	}

	double FModel::ShoreX(double Y)
	{
		// the map's coast passes (330, -900) running ~0.27 m west per metre south; past the map it swings west for good
		// where the sand begins it has spilled out over the tidal flat, so Topi's own beach lies ~150 m west of the map's
		const double D = FullY - Y;
		const double Spit = 150.0 * Smooth((-775.0 - Y) / 112.0);
		return D <= 0.0 ? 330.0 + 0.27 * D + Spit : 330.0 + 0.27 * D + 0.00022 * D * D + Spit;
	}

	double FModel::Region(double X, double Y)
	{
		const double Start = -775.0 + 22.0 * std::sin(X / 140.0) + 10.0 * std::sin(X / 53.0 + 1.0);
		return Smooth((Start - Y) / 112.0);
	}

	double FModel::Depth(double X, double Y)
	{
		return std::max(0.0, -880.0 - Y);
	}

	double FModel::RiverY(double X)
	{
		return -1840.0 + 60.0 * std::sin(X / 310.0 + 0.7) + 25.0 * std::sin(X / 127.0 + 2.1);
	}

	double FModel::RiverOffset(double X, double Y)
	{
		const double Slope = 60.0 / 310.0 * std::cos(X / 310.0 + 0.7) + 25.0 / 127.0 * std::cos(X / 127.0 + 2.1);
		return (RiverY(X) - Y) / std::sqrt(1.0 + Slope * Slope);
	}

	double FModel::RiverStrength(double X)
	{
		// by the coast (inland ~250 m at the river's Y) the water has sunk into the sand
		const double Inland = ShoreX(RiverY(X)) - X;
		return Smooth((Inland - 150.0) / 190.0);
	}

	FVec2 FModel::RiverFlow(double X, double Y)
	{
		const double Off = std::fabs(RiverOffset(X, Y));
		const double K = RiverStrength(X);
		if (Off >= RiverHalfWidth || K <= 0.0)
		{
			return {};
		}
		const double Slope = 60.0 / 310.0 * std::cos(X / 310.0 + 0.7) + 25.0 / 127.0 * std::cos(X / 127.0 + 2.1);
		const double L = std::sqrt(1.0 + Slope * Slope);
		// fastest in the middle: a strong, smooth stream
		const double Speed = 2.4 * K * (1.0 - 0.75 * (Off / RiverHalfWidth) * (Off / RiverHalfWidth));
		return { Speed / L, Speed * Slope / L };
	}

	double FModel::RiverDepth(double X, double Y) const
	{
		const double K = RiverStrength(X);
		if (K <= 0.0 || std::fabs(RiverOffset(X, Y)) > RiverHalfWidth + 14.0)
		{
			return 0.0;
		}
		return std::max(0.0, RiverZ - Height(X, Y));
	}

	const std::vector<FVec2>& FModel::Path()
	{
		return PathData().Points;
	}

	double FModel::PathLength()
	{
		return PathData().Length;
	}

	double FModel::PathDistance(double X, double Y, double* Along)
	{
		const FPathData& D = PathData();
		double Best = 1e30, BestAlong = 0.0;
		for (size_t I = 1; I < D.Points.size(); ++I)
		{
			const FVec2& A = D.Points[I - 1];
			const FVec2& B = D.Points[I];
			const double EX = B.X - A.X, EY = B.Y - A.Y;
			const double L2 = EX * EX + EY * EY;
			const double T = Clamp01(((X - A.X) * EX + (Y - A.Y) * EY) / L2);
			const double DX = X - (A.X + EX * T), DY = Y - (A.Y + EY * T);
			const double Dist = std::sqrt(DX * DX + DY * DY);
			if (Dist < Best)
			{
				Best = Dist;
				BestAlong = D.Along[I - 1] + T * std::sqrt(L2);
			}
		}
		if (Along)
		{
			*Along = BestAlong;
		}
		return Best;
	}

	FVec2 FModel::PathPoint(double Along)
	{
		const FPathData& D = PathData();
		Along = std::min(std::max(Along, 0.0), D.Length);
		for (size_t I = 1; I < D.Points.size(); ++I)
		{
			if (Along <= D.Along[I] || I + 1 == D.Points.size())
			{
				const double T = (Along - D.Along[I - 1]) / (D.Along[I] - D.Along[I - 1]);
				return { Lerp(D.Points[I - 1].X, D.Points[I].X, T), Lerp(D.Points[I - 1].Y, D.Points[I].Y, T) };
			}
		}
		return D.Points.back();
	}

	double FModel::Pressure(double X, double Y, bool bNight)
	{
		const double In = Depth(X, Y);
		if (In <= 0.0)
		{
			return 0.0;
		}
		if (!bNight)
		{
			return Smooth((In - 70.0) / 260.0);
		}
		const double Off = Smooth((PathDistance(X, Y) - 26.0) / 46.0);
		const FVec2& End = PathData().Points.back();
		// by the river where the path comes out it is calm; far along the bank it is not
		const double Calm = (1.0 - Smooth((std::fabs(RiverOffset(X, Y)) - 55.0) / 40.0)) * (1.0 - Smooth((std::fabs(X - End.X) - 230.0) / 120.0));
		return Smooth((In - 40.0) / 160.0) * Off * (1.0 - Calm);
	}

	FVec2 FModel::WayOut(double X, double Y)
	{
		double Along = 0.0;
		const double Dist = PathDistance(X, Y, &Along);
		FVec2 To{ 0.0, 1.0 };
		if (Dist < 140.0 && Along > 30.0)
		{
			const FVec2 P = PathPoint(Along - 30.0);
			const double DX = P.X - X, DY = P.Y - Y;
			const double L = std::hypot(DX, DY);
			if (L > 1.0)
			{
				To = { DX / L, DY / L };
			}
		}
		return To;
	}

	double FModel::Height(double X, double Y, double Cell) const
	{
		FSample S;
		Sample(X, Y, Cell, S);
		return S.Z;
	}

	void FModel::Sample(double X, double Y, double Cell, FSample& Out) const
	{
		const double K = Region(X, Y);
		const double In = Depth(X, Y);
		const double Inland = ShoreX(Y) - X;
		Out.Region = K;
		Out.Depth = In;
		Out.River = 0.0;
		Out.Grass = 0.0;

		const bool bOverMap = Y > FullY - 60.0;
		const double Map = bOverMap ? Edge.At(X, Y) : 0.0;
		if (K <= 0.0)
		{
			Out.Z = Map - 5.0;
			return;
		}

		// the level the dunes stand on: low by the edge, rising slowly into the deep, in broad swells
		double Base = SeaZ + 7.0 + 9.0 * Smooth(In / 1500.0) + 5.0 * (Noise(X / 900.0, Y / 900.0, 11) - 0.5);

		// the great dune over the end of the map: it has climbed the cliff and the upland and buried them
		double Ridge = 0.0;
		{
			const double Top = RidgeTop(X);
			const double South = std::max(0.0, (FullY + 10.0) - Y);
			// its north face stands at the slope sand holds: it comes up over ~100 m
			Ridge = std::max(0.0, Top - Base) * std::exp(-(South / 130.0) * (South / 130.0)) * Smooth((-790.0 - Y) / 100.0) * Smooth((Inland - 40.0) / 160.0);
		}

		// the river's plain and how much of the river there is here
		const double RiverK = RiverStrength(X);
		const double Off = std::fabs(RiverOffset(X, Y));
		const double Plain = RiverK * (1.0 - Smooth((Off - 38.0) / 115.0));

		// the dunes: the wind blows from the sea (toward -X), so the slip faces look inland
		const double U = -X, V = Y;
		const double PathD = PathDistance(X, Y);
		const double Corridor = 1.0 - 0.5 * (1.0 - Smooth((PathD - 6.0) / 16.0));
		const double FromSea = Smooth((Inland - 30.0) / 190.0);
		const double Env = K * (0.25 + 0.75 * FromSea) * (1.0 - Plain);
		const double Mega = std::max(Smooth((In - 1400.0) / 2500.0), Smooth((-X - 1700.0) / 2500.0)) * Smooth((In - 150.0) / 500.0) * Smooth((Inland - 100.0) / 600.0);

		double Dunes = 0.0;
		double BigWind = 1.0, MedWind = 1.0;
		const auto Layer = [&](double Length, double Amp, double Lee, uint32_t Seed, double Scale, double* Windward, double Turn)
		{
			if (Scale <= 0.0)
			{
				return;
			}
			if (Cell > 0.0 && Length < 3.0 * Cell)
			{
				Dunes += Scale * DuneMean(Amp);
				return;
			}
			const double C = std::cos(Turn), S = std::sin(Turn);
			Dunes += Scale * DuneLayer(U * C + V * S, V * C - U * S, Length, Amp, Lee, Seed, Windward);
		};
		Layer(5200.0, 260.0, 0.84, 71, Mega * Smooth((In - 2500.0) / 5000.0), nullptr, 0.21);
		Layer(1150.0, 75.0, 0.82, 61, Mega, nullptr, -0.12);
		Layer(240.0, 22.0, 0.80, 21, Env, &BigWind, 0.0);
		Layer(78.0, 5.5, 0.80, 31, Env * (0.35 + 0.65 * BigWind) * Corridor, &MedWind, 0.16);
		Layer(19.0, 0.9, 0.78, 41, Env * (0.3 + 0.7 * BigWind * MedWind) * Corridor, nullptr, -0.33);

		double Z = Base + Ridge * (1.0 - Plain) + Dunes;

		// the river: a level plain of hard sand, and the channel in it
		if (Plain > 0.0)
		{
			Z = Lerp(Z, RiverZ + 0.7, Plain * 0.92);
			const double Bank = 1.0 - Smooth((Off - RiverHalfWidth + 2.0) / 12.0);
			const double Mid = std::max(0.0, 1.0 - (Off / RiverHalfWidth) * (Off / RiverHalfWidth));
			Z -= RiverK * (Bank * 1.0 + Mid * RiverDeep);
			Out.River = RiverK * (1.0 - Smooth((Off - RiverHalfWidth - 3.0) / 9.0));
		}

		// the coast: a beach, then the bed
		const double Coast = Smooth((Inland - 25.0) / 70.0);
		if (Coast < 1.0)
		{
			const double Beach = Inland >= 0.0 ? SeaZ - 0.3 + Inland * 0.07 : std::max(SeaZ - 0.3 + Inland * 0.035, SeaZ - 15.0);
			Z = Lerp(Beach, Z, Coast);
		}

		// over the map's own ground the sand comes up from under it
		if (bOverMap && K < 1.0)
		{
			Z = Lerp(Map - 5.0, Z, Smooth(K / 0.7));
		}
		Out.Z = Z;

		// marram: on the fore dunes and round the edge, thinning into the deep; none on the beach, the river plain or under water
		const double Edgeward = 1.0 - Smooth((In - 120.0) / 420.0);
		const double Fore = 1.0 - Smooth((Inland - 160.0) / 260.0);
		Out.Grass = K * std::max(Edgeward, 0.8 * Fore) * Smooth((Inland - 28.0) / 30.0) * (1.0 - Plain)
			* (0.35 + 0.65 * Noise(X / 37.0, Y / 37.0, 91));
	}
}
