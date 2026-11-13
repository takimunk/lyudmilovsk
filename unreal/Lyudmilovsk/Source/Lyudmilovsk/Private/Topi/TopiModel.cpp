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

		// Everything below is worked out in Topi's own frame, "local" metres: LX runs along the sand's edge (toward
		// the map's south), LY across it, with the sand's edge at LY -775, the end of the map's landscape at LY -900
		// (FullLY) and Topi going on toward -LY for ever. On the map that is: x = LY + EdgeShift, y = AlongOrigin - LX
		// (a quarter turn: local "north" is the map's west), so the edge lies along the map's east side.
		constexpr double FullLY = -900.0;
		constexpr double EdgeShift = WestX + 900.0 - 0.0;     // LY -900 is the map's X_MIN
		constexpr double AlongOrigin = 1188.0;                // the path starts at local LX 283: map Y 905, the town's main axis

		double LocalRiverY(double LX)
		{
			return -1840.0 + 60.0 * std::sin(LX / 310.0 + 0.7) + 25.0 * std::sin(LX / 127.0 + 2.1);
		}
		double LocalRiverSlope(double LX)
		{
			return 60.0 / 310.0 * std::cos(LX / 310.0 + 0.7) + 25.0 / 127.0 * std::cos(LX / 127.0 + 2.1);
		}

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
					{ 283.0, -770.0 }, { 290.0, -840.0 }, { 300.0, -912.0 }, { 250.0, -1010.0 }, { 268.0, -1120.0 },
					{ 215.0, -1215.0 }, { 150.0, -1290.0 }, { 118.0, -1350.0 }, { 192.0, -1470.0 }, { 128.0, -1590.0 },
					{ 162.0, -1690.0 },
					{ EndX, LocalRiverY(EndX) + RiverHalfWidth + 6.0 } };
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
			return 0.0;
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
		if (std::memcmp(H.Magic, "TOPE", 4) != 0 || H.Version != 2 || H.NX < 2 || H.NY < 2
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

	FVec2 FModel::ToLocal(double X, double Y)
	{
		return { AlongOrigin - Y, X - EdgeShift };
	}

	FVec2 FModel::FromLocal(double LX, double LY)
	{
		return { LY + EdgeShift, AlongOrigin - LX };
	}

	void FModel::Prepare()
	{
		RidgeTops.clear();
		if (!Edge.Valid())
		{
			return;
		}
		// the highest map ground in each row (map Y) of the strip the great dune covers, widened and smoothed along
		// the edge so the sand that buries it lies at a slope sand can hold
		const int N = Edge.NY;
		std::vector<float> Top(N, -1e9f), Wide(N), Soft(N);
		for (int J = 0; J < N; ++J)
		{
			for (int I = 0; I < Edge.NX; ++I)
			{
				const double X = Edge.X0 + I * Edge.Cell;
				if (X >= WestX - 6.0 && X <= WestX + 100.0)
				{
					Top[J] = std::max(Top[J], Edge.Z[size_t(J) * Edge.NX + I]);
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

	double FModel::RidgeTop(double Y) const
	{
		if (RidgeTops.empty())
		{
			return 10.0;
		}
		double G = (Y - Edge.Y0) / Edge.Cell;
		G = std::min(std::max(G, 0.0), double(RidgeTops.size() - 1) - 1e-6);
		const int I = int(G);
		return Lerp(RidgeTops[I], RidgeTops[I + 1], G - I) + 10.0;
	}

	double FModel::Region(double X, double Y)
	{
		const FVec2 L = ToLocal(X, Y);
		const double Start = -775.0 + 22.0 * std::sin(L.X / 140.0) + 10.0 * std::sin(L.X / 53.0 + 1.0);
		return Smooth((Start - L.Y) / 112.0);
	}

	double FModel::Depth(double X, double Y)
	{
		return std::max(0.0, -880.0 - ToLocal(X, Y).Y);
	}

	bool FModel::Near(double X, double Y, double Margin)
	{
		return ToLocal(X, Y).Y < -730.0 + Margin;
	}

	FVec2 FModel::RiverPoint(double S)
	{
		const double LX = PathData().Points.back().X + S;
		return FromLocal(LX, LocalRiverY(LX));
	}

	FVec2 FModel::RiverDirection(double S)
	{
		const double Slope = LocalRiverSlope(PathData().Points.back().X + S);
		const double L = std::sqrt(1.0 + Slope * Slope);
		// local (1, Slope) / L, turned into the map's frame
		return { Slope / L, -1.0 / L };
	}

	double FModel::RiverOffset(double X, double Y)
	{
		const FVec2 L = ToLocal(X, Y);
		const double Slope = LocalRiverSlope(L.X);
		return (LocalRiverY(L.X) - L.Y) / std::sqrt(1.0 + Slope * Slope);
	}

	FVec2 FModel::RiverFlow(double X, double Y)
	{
		const FVec2 L = ToLocal(X, Y);
		const double Slope = LocalRiverSlope(L.X);
		const double Len = std::sqrt(1.0 + Slope * Slope);
		const double Off = std::fabs(LocalRiverY(L.X) - L.Y) / Len;
		if (Off >= RiverHalfWidth)
		{
			return {};
		}
		// fastest in the middle: a strong, smooth stream
		const double Speed = 2.4 * (1.0 - 0.75 * (Off / RiverHalfWidth) * (Off / RiverHalfWidth));
		return { Speed * Slope / Len, -Speed / Len };
	}

	double FModel::RiverDepth(double X, double Y) const
	{
		if (std::fabs(RiverOffset(X, Y)) > RiverHalfWidth + 14.0)
		{
			return 0.0;
		}
		return std::max(0.0, RiverZ - Height(X, Y));
	}

	const std::vector<FVec2>& FModel::Path()
	{
		static const std::vector<FVec2> World = []
		{
			std::vector<FVec2> W;
			for (const FVec2& P : PathData().Points)
			{
				W.push_back(FromLocal(P.X, P.Y));
			}
			return W;
		}();
		return World;
	}

	double FModel::PathLength()
	{
		return PathData().Length;
	}

	namespace
	{
		double LocalPathDistance(double LX, double LY, double* Along)
		{
			const FPathData& D = PathData();
			double Best = 1e30, BestAlong = 0.0;
			for (size_t I = 1; I < D.Points.size(); ++I)
			{
				const FVec2& A = D.Points[I - 1];
				const FVec2& B = D.Points[I];
				const double EX = B.X - A.X, EY = B.Y - A.Y;
				const double L2 = EX * EX + EY * EY;
				const double T = Clamp01(((LX - A.X) * EX + (LY - A.Y) * EY) / L2);
				const double DX = LX - (A.X + EX * T), DY = LY - (A.Y + EY * T);
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

		FVec2 LocalPathPoint(double Along)
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
	}

	double FModel::PathDistance(double X, double Y, double* Along)
	{
		const FVec2 L = ToLocal(X, Y);
		return LocalPathDistance(L.X, L.Y, Along);
	}

	FVec2 FModel::PathPoint(double Along)
	{
		const FVec2 P = LocalPathPoint(Along);
		return FromLocal(P.X, P.Y);
	}

	double FModel::Pressure(double X, double Y, bool bNight)
	{
		const FVec2 L = ToLocal(X, Y);
		const double In = std::max(0.0, -880.0 - L.Y);
		if (In <= 0.0)
		{
			return 0.0;
		}
		if (!bNight)
		{
			return Smooth((In - 70.0) / 260.0);
		}
		const double Off = Smooth((LocalPathDistance(L.X, L.Y, nullptr) - 26.0) / 46.0);
		const FVec2& End = PathData().Points.back();
		// by the river where the path comes out it is calm; far along the bank it is not
		const double Calm = (1.0 - Smooth((std::fabs(RiverOffset(X, Y)) - 55.0) / 40.0)) * (1.0 - Smooth((std::fabs(L.X - End.X) - 230.0) / 120.0));
		return Smooth((In - 40.0) / 160.0) * Off * (1.0 - Calm);
	}

	FVec2 FModel::WayOut(double X, double Y)
	{
		const FVec2 L = ToLocal(X, Y);
		double Along = 0.0;
		const double Dist = LocalPathDistance(L.X, L.Y, &Along);
		// straight back to the map (local "north" is the map's west), or back along the path if he is near it
		FVec2 To{ 1.0, 0.0 };
		if (Dist < 140.0 && Along > 30.0)
		{
			const FVec2 P = PathPoint(Along - 30.0);
			const double DX = P.X - X, DY = P.Y - Y;
			const double Len = std::hypot(DX, DY);
			if (Len > 1.0)
			{
				To = { DX / Len, DY / Len };
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
		const FVec2 L = ToLocal(X, Y);
		const double K = Region(X, Y);
		const double In = std::max(0.0, -880.0 - L.Y);
		Out.Region = K;
		Out.Depth = In;
		Out.River = 0.0;
		Out.Grass = 0.0;

		const bool bOverMap = L.Y > FullLY - 60.0;
		const double Map = bOverMap ? Edge.At(X, Y) : 0.0;
		if (K <= 0.0)
		{
			Out.Z = Map - 5.0;
			return;
		}

		// the level the dunes stand on: well under the map's inland ridge, rising slowly into the deep, in broad swells
		double Base = BaseZ + 9.0 * Smooth(In / 1500.0) + 5.0 * (Noise(L.X / 900.0, L.Y / 900.0, 11) - 0.5);

		// the great dune over the edge of the map: it has climbed the inland ridge from behind and buried its back
		double Great = 0.0;
		{
			const double Top = RidgeTop(Y);
			const double Past = std::max(0.0, (FullLY + 10.0) - L.Y);
			// its face toward the town stands at the slope sand holds: it comes up over ~100 m
			Great = std::max(0.0, Top - Base) * std::exp(-(Past / 130.0) * (Past / 130.0)) * Smooth((-790.0 - L.Y) / 100.0);
		}
		// where the path climbs it the great dune has a saddle, so the way in is a climb and not a wall: there it
		// stands only a little over the map's own ground at this very place, not over the highest ground around
		const double Saddle = 1.0 - Smooth((LocalPathDistance(L.X, L.Y, nullptr) - 25.0) / 90.0);
		if (Saddle > 0.0 && Great > 0.0)
		{
			const double Here = std::max(Edge.At(WestX, Y), std::max(Edge.At(WestX + 30.0, Y), Edge.At(WestX + 60.0, Y))) + 4.5;
			const double Low = Great * std::max(0.0, Here - Base) / std::max(1.0, RidgeTop(Y) - Base);
			Great = Lerp(Great, std::min(Great, Low), Saddle);
		}

		// the river's plain
		const double Off = std::fabs(LocalRiverY(L.X) - L.Y) / std::sqrt(1.0 + LocalRiverSlope(L.X) * LocalRiverSlope(L.X));
		const double Plain = 1.0 - Smooth((Off - 38.0) / 115.0);

		// the dunes: the wind blows from the sea across the town and on (toward the map's -X, local -LY), so the
		// slip faces look away from the town
		const double U = -L.Y, V = L.X;
		const double PathD = LocalPathDistance(L.X, L.Y, nullptr);
		const double Corridor = 1.0 - 0.5 * (1.0 - Smooth((PathD - 6.0) / 16.0));
		const double Env = K * (0.3 + 0.7 * Smooth(In / 220.0)) * (1.0 - Plain);
		const double Mega = Smooth((In - 1400.0) / 2500.0);

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
		Layer(240.0, 31.0, 0.80, 21, Env, &BigWind, 0.0);
		Layer(78.0, 7.5, 0.80, 31, Env * (0.35 + 0.65 * BigWind) * Corridor, &MedWind, 0.16);
		Layer(19.0, 0.9, 0.78, 41, Env * (0.3 + 0.7 * BigWind * MedWind) * Corridor, nullptr, -0.33);

		double Z = Base + Great * (1.0 - Plain) + Dunes;

		// the river: a level plain of hard sand, and the channel in it
		if (Plain > 0.0)
		{
			Z = Lerp(Z, RiverZ + 0.7, Plain * 0.92);
			const double Bank = 1.0 - Smooth((Off - RiverHalfWidth + 2.0) / 12.0);
			const double Mid = std::max(0.0, 1.0 - (Off / RiverHalfWidth) * (Off / RiverHalfWidth));
			Z -= Bank * 1.0 + Mid * RiverDeep;
			Out.River = 1.0 - Smooth((Off - RiverHalfWidth - 3.0) / 9.0);
		}

		// over the map's own ground the sand comes up from under it
		if (bOverMap && K < 1.0)
		{
			Z = Lerp(Map - 5.0, Z, Smooth(K / 0.7));
		}
		Out.Z = Z;

		// marram: thick round the edge; further in it holds the dunes' long backs and crests in tussocky patches
		// (none on the slip faces), thinning out past the river; none on the river's plain
		const double Edgeward = 1.0 - Smooth((In - 120.0) / 420.0);
		const double Patch = Smooth((Noise(L.X / 95.0, L.Y / 80.0, 92) - 0.42) / 0.2);
		const double Inner = 0.55 * Patch * BigWind * (1.0 - Smooth((In - 1300.0) / 900.0));
		Out.Grass = K * std::max(Edgeward, Inner) * (1.0 - Plain) * (0.35 + 0.65 * Noise(L.X / 37.0, L.Y / 37.0, 91));
	}
}
