#include "WindField.h"

#include <algorithm>
#include <cmath>

namespace LyudWeather
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr double TwoPi = 2.0 * Pi;

		float Sat(float V) { return std::min(std::max(V, 0.f), 1.f); }
		float Smooth(float A, float B, float X)
		{
			const float T = Sat((X - A) / (B - A));
			return T * T * (3.f - 2.f * T);
		}
		double SmoothD(double A, double B, double X)
		{
			const double T = std::min(std::max((X - A) / (B - A), 0.0), 1.0);
			return T * T * (3.0 - 2.0 * T);
		}
		float Wrap360(float Deg)
		{
			Deg = std::fmod(Deg, 360.f);
			return Deg < 0.f ? Deg + 360.f : Deg;
		}
		float Wrap180(float Deg)
		{
			Deg = std::fmod(Deg + 180.f, 360.f);
			return (Deg < 0.f ? Deg + 360.f : Deg) - 180.f;
		}

		// ------------------------------------------------------------ shifts

		/** A seeded phase, 0..2 pi (its own hash: WindModel's is private to it). */
		double SeedPhase(uint32_t Seed, uint32_t K)
		{
			uint32_t H = Seed * 0x9e3779b9u + K * 0x85ebca6bu + 0x27d4eb2fu;
			H = (H ^ (H >> 16)) * 0x7feb352du;
			H = (H ^ (H >> 15)) * 0x846ca68bu;
			H ^= H >> 16;
			return double(H >> 8) * (TwoPi / 16777216.0);
		}

		// Three bands of three sines each, periods in minutes that share no small common multiple: the slow swing of
		// settled air, the quick one of stirred afternoon air, and the bay's breathing. Every sine has a zero mean, so
		// an hour's veer averages out and Regime's statistics stand.
		constexpr double SlowMin[3] = { 10.9, 7.3, 17.9 };
		constexpr double FastMin[3] = { 5.3, 3.7, 6.9 };
		constexpr double BreathMin[3] = { 13.7, 8.9, 5.9 };
		constexpr double BandWeight[3] = { 0.62, 0.33, 0.22 };
		// what a band's mean swing (two sign changes) takes, measured over a year by tools/windsim
		constexpr double SlowSwingMin = 10.4, FastSwingMin = 5.0;

		struct FShiftBasis
		{
			double Phase[9];
			double Fast = 0.0;      // 0 settled air: the slow band only .. 1 stirred: the quick one
			double Norm = 1.0;
			float RangeDeg = 2.f;
			float Breath = 0.05f;
			float PeriodMin = 10.f;
		};

		FShiftBasis MakeBasis(const FWindState& R, uint32_t Seed)
		{
			FShiftBasis B;
			for (uint32_t K = 0; K < 9; K++)
			{
				B.Phase[K] = SeedPhase(Seed, K);
			}
			// a light wind wanders; a fresh gradient wind holds its bearing. Sun and gusty air stir it either way.
			const float Light = 1.f - Smooth(3.f, 10.f, R.Speed);
			const float Gusty = std::min(std::max((R.Gustiness - 0.10f) / 0.22f, 0.f), 1.2f);
			const float Day = Sat(R.Day);
			B.RangeDeg = std::min(2.f + 1.5f * Gusty + 1.5f * Day + Light * (2.f + 5.f * Gusty + 6.5f * Day), 18.f);
			B.Breath = std::min(0.04f + 0.5f * R.Gustiness * (0.6f + 0.4f * Light), 0.22f);
			B.Fast = SmoothD(0.0, 1.0, 0.08 + 0.84 * Day * (0.6 + 0.4 * Light));
			const double Slow = 1.0 - B.Fast;
			B.Norm = 1.0 / std::sqrt(Slow * Slow + B.Fast * B.Fast);
			// the two bands' shares of the swing's energy set how long a swing takes now
			const double FastShare = B.Fast * B.Fast * B.Norm * B.Norm;
			B.PeriodMin = float(1.0 / ((1.0 - FastShare) / SlowSwingMin + FastShare / FastSwingMin));
			return B;
		}

		double Band(double Minutes, const double* Periods, const double* Phase)
		{
			double V = 0.0;
			for (int K = 0; K < 3; K++)
			{
				V += BandWeight[K] * std::sin(TwoPi * Minutes / Periods[K] + Phase[K]);
			}
			return V;
		}

		/** The swing before it is squashed into -1..1: only its sign and zeros matter to the scans. */
		double RawSwing(double Minutes, const FShiftBasis& B)
		{
			return ((1.0 - B.Fast) * Band(Minutes, SlowMin, B.Phase) + B.Fast * Band(Minutes, FastMin, B.Phase + 3)) * B.Norm;
		}

		/** Odd, smooth, -1..1: lets the swing reach its range often without ever passing it. */
		double Squash(double V)
		{
			V = std::min(std::max(V, -1.0), 1.0);
			return V * (1.5 - 0.5 * V * V);
		}

		constexpr double TurnStepMin = 0.1, TurnLimitMin = 30.0;

		/** What one scan step turns each of the swing's six sines by. */
		struct FTurnSteps
		{
			double Cos[6], Sin[6];
			FTurnSteps()
			{
				for (int K = 0; K < 6; K++)
				{
					const double A = TwoPi * TurnStepMin / (K < 3 ? SlowMin[K] : FastMin[K - 3]);
					Cos[K] = std::cos(A);
					Sin[K] = std::sin(A);
				}
			}
		};

		/** Minutes from Minutes to the swing's next (Dir +1) or last (Dir -1) change of sign; TurnLimitMin if none that near.
		 *  Walks in TurnStepMin steps by turning each sine (no sin() per step), then halves the step it found the turn in. */
		double TurnFrom(double Minutes, const FShiftBasis& B, double Dir)
		{
			static const FTurnSteps Steps;
			double S[6], C[6], W[6];
			double V = 0.0;
			for (int K = 0; K < 6; K++)
			{
				const double A = TwoPi * Minutes / (K < 3 ? SlowMin[K] : FastMin[K - 3]) + B.Phase[K];
				S[K] = std::sin(A);
				C[K] = std::cos(A);
				W[K] = BandWeight[K % 3] * (K < 3 ? 1.0 - B.Fast : B.Fast);
				V += W[K] * S[K];
			}
			const bool bPositive = RawSwing(Minutes, B) >= 0.0;
			const int Limit = int(TurnLimitMin / TurnStepMin + 0.5);
			for (int I = 1; I <= Limit; I++)
			{
				V = 0.0;
				for (int K = 0; K < 6; K++)
				{
					const double S1 = S[K] * Steps.Cos[K] + Dir * C[K] * Steps.Sin[K];
					C[K] = C[K] * Steps.Cos[K] - Dir * S[K] * Steps.Sin[K];
					S[K] = S1;
					V += W[K] * S1;
				}
				if ((V >= 0.0) != bPositive)
				{
					// the walk's own sum drifts by rounding only: the exact function has the last word on the bracket
					double Lo = (I - 1) * TurnStepMin, Hi = I * TurnStepMin;
					if ((RawSwing(Minutes + Dir * Hi, B) >= 0.0) == bPositive)
					{
						continue;
					}
					for (int J = 0; J < 9; J++)
					{
						const double Mid = 0.5 * (Lo + Hi);
						if ((RawSwing(Minutes + Dir * Mid, B) >= 0.0) == bPositive)
						{
							Lo = Mid;
						}
						else
						{
							Hi = Mid;
						}
					}
					return 0.5 * (Lo + Hi);
				}
			}
			return TurnLimitMin;
		}

		void ShiftCore(double Hours, const FShiftBasis& B, FWindShift& Out)
		{
			const double Minutes = Hours * 60.0;
			Out.VeerDeg = float(B.RangeDeg * Squash(RawSwing(Minutes, B)));
			Out.Factor = float(1.0 + B.Breath * Squash(Band(Minutes, BreathMin, B.Phase + 6)));
			Out.PeriodMin = B.PeriodMin;
			Out.RangeDeg = B.RangeDeg;
		}

		// ------------------------------------------------------------ the land

		/** Ground and cover at a point in one go. Inside the grid bilinear; within a cell outside it they ease to the
		 *  sea bed, so nothing jumps at the grid's edge. */
		inline void Sample(const FWindTerrain& T, const float* Ground, const float* Cover, double InvCell, double X, double Y,
			float& OutGround, float& OutCover)
		{
			double FX = (X - T.X0) * InvCell, FY = (Y - T.Y0) * InvCell;
			const double MaxX = double(T.NX - 1), MaxY = double(T.NY - 1);
			float Keep = 1.f;
			if (!(FX >= 0.0 && FY >= 0.0 && FX < MaxX && FY < MaxY))
			{
				const double Out = std::max(std::max(-FX, FX - MaxX), std::max(-FY, FY - MaxY));
				if (!(Out < 1.0))
				{
					OutGround = T.SeaZ - 10.f;
					OutCover = 0.f;
					return;
				}
				Keep = float(1.0 - std::max(Out, 0.0));
				FX = std::min(std::max(FX, 0.0), MaxX);
				FY = std::min(std::max(FY, 0.0), MaxY);
			}
			const int IX = std::min(int(FX), T.NX - 2), IY = std::min(int(FY), T.NY - 2);
			const float U = float(FX - IX), V = float(FY - IY);
			const int I = IY * T.NX + IX;
			const float W00 = (1.f - U) * (1.f - V), W10 = U * (1.f - V), W01 = (1.f - U) * V, W11 = U * V;
			float G = Ground[I] * W00 + Ground[I + 1] * W10 + Ground[I + T.NX] * W01 + Ground[I + T.NX + 1] * W11;
			float C = Cover ? Cover[I] * W00 + Cover[I + 1] * W10 + Cover[I + T.NX] * W01 + Cover[I + T.NX + 1] * W11 : 0.f;
			if (Keep < 1.f)
			{
				G = (T.SeaZ - 10.f) + (G - (T.SeaZ - 10.f)) * Keep;
				C *= Keep;
			}
			OutGround = G;
			OutCover = C;
		}

		// The march upwind, 10 m to 3 km in 30 samples: close in each is 1.46 times further than the last; from 100 m
		// to 890 m (as far as the lee of the highest ground here reaches) they stand 44 m apart, no wider than a cliff's
		// face, so that some sample is always on the face of whatever shelters the point and a lee fades evenly with
		// distance instead of in the samples' strides; beyond, where only the fetch is left to find, 1.3 times.
		constexpr int MaxUp = 32;
		constexpr float FetchCap = 3000.f;
		struct FMarch
		{
			int Num = 0;
			int NumNear = 0;            // the first samples, to 46 m: the ground and cover the point stands among
			int NumRelief = 0;          // to 145 m: the relief a lift over the point is measured against
			int NumCrest = 0;           // to 450 m: how far back a drop still squeezes the air over a crest
			float Dist[MaxUp], Step[MaxUp];
			// how much each sample says about the surface the air here has been running over: all of it out to
			// 250 m, nothing past 600 m (a sea wind's profile lasts a few hundred metres inland, and the other way round)
			float Near[MaxUp];
			float NearSum = 10.f;       // with the 10 m under the point itself
			float StepSum = 0.f;        // the cap, as the march itself adds it up (so that open sea reads exactly the cap)
			FMarch()
			{
				double D = 10.0, Last = 0.0;
				while (Num < MaxUp)
				{
					Dist[Num] = float(D);
					Step[Num] = float(D - Last);
					StepSum += Step[Num];
					NumNear += D <= 46.0 ? 1 : 0;
					NumRelief += D <= 145.0 ? 1 : 0;
					NumCrest += D <= 450.0 ? 1 : 0;
					Num++;
					if (D >= FetchCap)
					{
						break;
					}
					Last = D;
					D = D < 850.0 ? D + std::min(0.46 * D, 44.0) : std::min(D * 1.3, double(FetchCap));
					if (Num == MaxUp - 1)
					{
						D = FetchCap;
					}
				}
				for (int I = 0; I < Num; I++)
				{
					const double Half = 0.5 * ((I + 1 < Num ? Dist[I + 1] : Dist[I]) - (I > 0 ? Dist[I - 1] : 0.f));
					Near[I] = float((1.0 - SmoothD(250.0, 600.0, Dist[I])) * Half);
					NearSum += Near[I];
				}
			}
		};
		const FMarch March;
		constexpr float LeeSolid = 0.88f;       // the speed lost in the deepest lee of solid ground
		constexpr float PorousShare = 0.7f;     // a wood or a street of houses lets some of the wind through

		/** How deep in an obstacle's lee: 1 right behind it and under its top, 0 from 14 of its heights on or well above it
		 *  (the wake's upper edge is soft and rises downwind). */
		inline float Lee(float Dist, float Height, float PointHeight)
		{
			const float T = Sat((Dist / Height - 2.f) * (1.f / 12.f));
			return (1.f - T) * (1.f - T) * (1.f - Smooth(-1.5f - 0.25f * Height, 1.5f + 0.15f * Height + 0.1f * Dist, PointHeight - Height));
		}
	}

	// ---------------------------------------------------------------- 1. shifts

	FWindShift ShiftAt(double Hours, const FWindState& Regime, uint32_t Seed)
	{
		const FShiftBasis B = MakeBasis(Regime, Seed);
		FWindShift Out;
		ShiftCore(Hours, B, Out);
		const double Minutes = Hours * 60.0;
		const double Next = TurnFrom(Minutes, B, 1.0);
		const double Last = TurnFrom(Minutes, B, -1.0);
		Out.NextTurnMin = float(Next);
		const double Through = Last / std::max(Last + Next, 1e-6);
		// the first half of a swing is the veered one
		Out.Phase = float(RawSwing(Minutes, B) >= 0.0 ? 0.5 * Through : 0.5 + 0.5 * Through);
		return Out;
	}

	FWindState Shifted(const FWindState& Regime, const FWindShift& Shift)
	{
		FWindState S = Regime;
		const double A = double(Shift.VeerDeg) * Pi / 180.0;
		const double C = std::cos(A), Sn = std::sin(A);
		// a turn by +A about Z is clockwise on the compass in this frame (east = -X)
		S.DirX = float(Regime.DirX * C - Regime.DirY * Sn);
		S.DirY = float(Regime.DirX * Sn + Regime.DirY * C);
		S.FromDeg = Wrap360(Regime.FromDeg + Shift.VeerDeg);
		S.Speed = Regime.Speed * Shift.Factor;
		return S;
	}

	FWindState Now(double Hours, const FWindParams& P)
	{
		const FWindState R = Regime(Hours, P);
		FWindShift Shift;
		ShiftCore(Hours, MakeBasis(R, P.Seed), Shift);
		return Shifted(R, Shift);
	}

	// ---------------------------------------------------------------- 2. the land and the sea

	float FWindTerrain::GroundAt(double X, double Y) const
	{
		if (!Valid())
		{
			return SeaZ - 10.f;
		}
		float G, C;
		Sample(*this, Ground.data(), nullptr, 1.0 / Cell, X, Y, G, C);
		return G;
	}

	float FWindTerrain::CoverAt(double X, double Y) const
	{
		if (!Valid() || Cover.size() != Ground.size())
		{
			return 0.f;
		}
		float G, C;
		Sample(*this, Ground.data(), Cover.data(), 1.0 / Cell, X, Y, G, C);
		return C;
	}

	int FWindTerrain::CellAt(double X, double Y) const
	{
		if (NX <= 0 || NY <= 0)
		{
			return -1;
		}
		const double FX = std::floor((X - X0) / Cell + 0.5), FY = std::floor((Y - Y0) / Cell + 0.5);
		if (!(FX >= 0.0 && FY >= 0.0 && FX < double(NX) && FY < double(NY)))
		{
			return -1;
		}
		return int(FY) * NX + int(FX);
	}

	FLocalWind LocalWindAt(const FWindState& S, const FWindTerrain* Terrain, double X, double Y, double Z, float WaterZ,
		double OffsetX, double OffsetY, double Seconds)
	{
		FLocalWind Out;
		const FGust G = GustAt(S, X, Y, OffsetX, OffsetY, Seconds);
		Out.Gust = G.Factor;

		if (!Terrain || !Terrain->Valid())
		{
			// the engine's old GetWindAt over land: open, flat, the ground at Z = 0
			const float H = float(Z);
			const float C = std::cos(G.Veer), Sn = std::sin(G.Veer);
			const float DX = S.DirX * C - S.DirY * Sn, DY = S.DirX * Sn + S.DirY * C;
			Out.Speed = S.Speed * G.Factor * HeightFactor(H, false);
			Out.VX = DX * Out.Speed;
			Out.VY = DY * Out.Speed;
			Out.FromDeg = Wrap360(S.FromDeg + G.Veer * float(180.0 / Pi));
			Out.Turbulence = S.Speed * S.Gustiness * 0.6f * (1.f - 0.4f * Smooth(20.f, 120.f, H));
			Out.HeightM = H;
			return Out;
		}

		const FWindTerrain& T = *Terrain;
		const float* Ground = T.Ground.data();
		const float* Cover = T.Cover.size() == T.Ground.size() ? T.Cover.data() : nullptr;
		const double InvCell = 1.0 / T.Cell;
		const double DX = S.DirX, DY = S.DirY;

		float G0, C0;
		Sample(T, Ground, Cover, InvCell, X, Y, G0, C0);
		// the water's edge is half a metre of height wide to the wind, so nothing steps as the tide or a walker crosses it
		const float EdgeLo = WaterZ - 0.4f, EdgeHi = WaterZ + 0.4f;
		const float Land0 = Smooth(EdgeLo, EdgeHi, G0);
		const float Surf0 = std::max(G0, WaterZ);
		const float Height = std::max(float(Z) - Surf0, 0.f);

		float SolidLee = 0.f, PorousLee = 0.f;  // the deepest lee found, of ground and of what stands on it
		float Fall = 0.f;                       // how steeply the surface drops away upwind (a crest under the point)
		float Fetch = 0.f, Open = 1.f - Land0;  // open water upwind, and whether the march is still over it
		float Marine = 10.f * (1.f - Land0);
		float CoverNear = 2.f * C0;
		float Top = Surf0, Bottom = Surf0;      // the relief round the point (for how high its lift reaches)
		float Up0 = Surf0, Up1 = Surf0;
		float LastOver = G0 - WaterZ, LastHs = 0.f, LastHp = 0.f;
		const int Num = March.Num;
		for (int I = 0; I < Num; I++)
		{
			const float D = March.Dist[I];
			float Gi, Ci;
			Sample(T, Ground, Cover, InvCell, X - DX * D, Y - DY * D, Gi, Ci);
			const float Si = std::max(Gi, WaterZ);

			// the fetch: the water of each step while no land has come yet; where a step crosses the water's edge, the
			// share of it the two ends' depth and height give (so the fetch grows evenly as the point moves away)
			const float Over = Gi - WaterZ;
			const float Wet = std::max(-LastOver, 0.f) + std::max(-Over, 0.f), Dry = std::max(LastOver, 0.f) + std::max(Over, 0.f);
			Fetch += March.Step[I] * Open * ((Wet + 1e-6f) / (Wet + Dry + 1e-6f));
			// (a far sample stands for a long step: its shore is as much wider, or a step of the point across a steep
			// bank far upwind would open or shut kilometres of water at once)
			const float Landi = Smooth(EdgeLo, EdgeHi + 0.004f * D, Gi);
			Open *= 1.f - Landi;
			Marine += March.Near[I] * (1.f - Landi);
			LastOver = Over;

			// the lee of ground and of cover that stand higher than the surface here. Where the last sample stood
			// lower, the obstacle's edge is between the two: as much nearer as the last one was already up it.
			const float Hs = Si - Surf0;
			if (Hs > 0.5f)
			{
				const float Edge = D - March.Step[I] * Sat(LastHs / Hs);
				SolidLee = std::max(SolidLee, Lee(Edge, Hs, Height) * Smooth(0.5f, 5.f, Hs));
			}
			const float Hp = Hs + Ci;
			if (Ci > 0.5f && Hp > 0.5f)
			{
				// low cover is thin cover (a wood's ragged edge, scrub, a cell half under a roof)
				const float Edge = D - March.Step[I] * Sat(LastHp / Hp);
				PorousLee = std::max(PorousLee, Lee(Edge, Hp, Height) * Smooth(0.5f, 9.f, std::min(Ci, Hp)));
			}
			LastHs = Hs;
			LastHp = Ci > 0.5f ? Hp : Hs;
			if (I < March.NumCrest)
			{
				const float Drop = -Hs;
				if (Drop > 0.f)
				{
					const float A = D / (2.5f * Drop + 20.f);
					Fall = std::max(Fall, Drop / (D + 8.f) / (1.f + A * A));
				}
			}
			if (I < March.NumNear)
			{
				CoverNear += Ci;
				Top = std::max(Top, Si);
			}
			if (I < March.NumRelief)
			{
				Bottom = std::min(Bottom, Si);
			}
			if (I == 0)
			{
				Up0 = Si;
			}
			else if (I == 1)
			{
				Up1 = Si;
			}
		}
		CoverNear /= 2.f + float(March.NumNear);
		Marine /= March.NearSum;
		Fetch = FetchCap * (Fetch / March.StepSum);

		// under the crowns (or between the houses) of the very spot
		if (C0 > 0.5f)
		{
			PorousLee = std::max(PorousLee, Smooth(0.5f, 9.f, C0) * (1.f - Smooth(-1.5f - 0.3f * C0, 1.5f + 0.15f * C0, Height - C0)));
		}
		const float Shelter = std::max(SolidLee, PorousShare * PorousLee);

		// downwind: what the air is about to climb or drop into
		float Gd, Cd;
		Sample(T, Ground, nullptr, InvCell, X + DX * 12.0, Y + DY * 12.0, Gd, Cd);
		const float Down1 = std::max(Gd, WaterZ);
		Sample(T, Ground, nullptr, InvCell, X + DX * 35.0, Y + DY * 35.0, Gd, Cd);
		const float Down2 = std::max(Gd, WaterZ);
		Sample(T, Ground, nullptr, InvCell, X + DX * 90.0, Y + DY * 90.0, Gd, Cd);
		const float Down3 = std::max(Gd, WaterZ);
		Top = std::max(std::max(Top, Down1), std::max(Down2, Down3));
		Bottom = std::min(Bottom, Down1);
		const float Relief = std::max(8.f, Top - Bottom);
		const float Rise1 = (Down1 - Surf0) * (1.f / 12.f), Rise2 = (Down2 - Surf0) * (1.f / 35.f), Rise3 = (Down3 - Surf0) * (1.f / 90.f);
		const float Local = (Down1 - Up0) / (12.f + March.Dist[0]);

		// a crest, a cliff top, a headland: the air is squeezed over it, most near the ground and right at the brow
		const float Crest = Smooth(0.f, 0.5f, Fall) * (1.f - Smooth(0.05f, 0.35f, std::max(Rise1, Rise2)));
		const float SpeedUp = 1.f + 0.42f * Crest / (1.f + Height / (0.7f * std::max(Surf0 - Bottom, 10.f))) * (1.f - Shelter);

		// the wind's profile: the sea's far out, the land's over land and for the first few hundred metres off it;
		// over a wood or roofs the profile starts from near their tops
		const float OverCover = std::max(Height - 0.65f * C0, 0.3f);
		const float LandProfile = HeightFactor(OverCover, false), SeaProfile = HeightFactor(std::max(Height, 0.3f), true);
		const float Profile = LandProfile + (SeaProfile - LandProfile) * Marine;
		// and the land's drag on the whole lower air (the regime's speed is the open sea's at 10 m)
		const float Rough = (0.2f + 0.08f * Smooth(0.f, 10.f, CoverNear)) * (1.f - Marine) / (1.f + Height * (1.f / 150.f));

		Out.Shelter = Shelter;
		Out.Exposure = (1.f - Rough) * SpeedUp * (1.f - LeeSolid * Shelter);
		Out.Speed = S.Speed * G.Factor * Profile * Out.Exposure;

		// ridge lift: the air follows the ground rising under and ahead of it (and still climbs just past a brow),
		// fading above the crest as BirdAir's does; behind a crest it sinks, though never near the ground
		const float OverTop = (float(Z) - Top) / (1.4f * Relief);
		const float Fade = OverTop <= 0.f ? 1.f : 1.f / (1.f + OverTop + 0.5f * OverTop * OverTop);
		// (the ground under the point lifts the air at any height; ground further ahead only the air some way up)
		const float Ahead = std::max(std::max(Rise1 * Smooth(0.f, 4.f, Height), 0.7f * Rise2 * Smooth(0.f, 12.f, Height)), 0.4f * Rise3 * Smooth(0.f, 30.f, Height));
		const float Climb = std::max(std::max(Local, Ahead), 0.5f * (Surf0 - Up1) / March.Dist[1]);
		float Updraft = 0.f;
		if (Climb > 0.f)
		{
			// the air follows a gentle slope; off a steep face it rises at 0.6 of its speed at most
			const float Steep = Climb * (1.f / 0.6f);
			Updraft = Out.Speed * Climb / std::sqrt(1.f + Steep * Steep) * Fade;
		}
		const float Descent = std::max(-Local, 0.f) * (1.f - Sat(Climb * 10.f));
		const float Sink = std::min(0.3f * std::min(Descent, 0.6f) + 0.12f * SolidLee, 0.2f);
		if (Sink > 0.f)
		{
			Updraft -= std::min(S.Speed * G.Factor * Sink, 1.5f) * Smooth(5.f, 20.f, Height) * Fade;
		}
		Out.Updraft = Out.VZ = Updraft;

		// along a cliff or a steep shore the low air slides toward the lower side
		Sample(T, Ground, nullptr, InvCell, X + DX * 15.0 - DY * 30.0, Y + DY * 15.0 + DX * 30.0, Gd, Cd);
		const float Left = std::max(Gd, WaterZ);
		Sample(T, Ground, nullptr, InvCell, X + DX * 15.0 + DY * 30.0, Y + DY * 15.0 - DX * 30.0, Gd, Cd);
		const float Right = std::max(Gd, WaterZ);
		const float Cross = (Left - Right) * (2.5f / 60.f);
		const float Bend = -0.26f * Cross / std::sqrt(1.f + Cross * Cross) / (1.f + Height * (1.f / 25.f));

		const float Turn = G.Veer + Bend;
		const float C = std::cos(Turn), Sn = std::sin(Turn);
		Out.VX = (S.DirX * C - S.DirY * Sn) * Out.Speed;
		Out.VY = (S.DirX * Sn + S.DirY * C) * Out.Speed;
		Out.FromDeg = Wrap360(S.FromDeg + Turn * float(180.0 / Pi));

		// what the mean does not carry: the gusts' own spread (less over open water, more over woods and roofs, less
		// high up) and the tumbling of a lee, a sixth of the open wind or so
		const float Stir = 0.4f + (0.2f + 0.25f * Smooth(0.f, 12.f, CoverNear)) * (1.f - Marine);
		// (deep in a lee the gusts do not reach; the tumbling is strongest toward the wake's edge)
		const float Wake = std::max(SolidLee, 0.6f * PorousShare * PorousLee);
		Out.Turbulence = S.Speed * (S.Gustiness * Stir * (1.f - 0.4f * Smooth(20.f, 120.f, Height)) * (1.f - 0.5f * Shelter) + 0.45f * Wake * (1.f - 0.65f * Wake));

		Out.HeightM = Height;
		Out.SurfaceZ = Surf0;
		Out.FetchM = Fetch;
		Out.bOverWater = G0 < WaterZ;
		return Out;
	}

	// ---------------------------------------------------------------- 3. looking ahead

	float GustAhead(const FWindState& S, double X, double Y, double OffsetX, double OffsetY, double Seconds, double InSeconds)
	{
		// the engine drifts the field by the mean wind (ULyudWindSubsystem::Tick) and runs the gusts' clock in seconds
		const double Run = double(S.Speed) * InSeconds;
		return GustAt(S, X, Y, OffsetX + S.DirX * Run, OffsetY + S.DirY * Run, Seconds + InSeconds).Factor;
	}

	FGustEvent NextGust(const FWindState& S, double X, double Y, double OffsetX, double OffsetY, double Seconds,
		double WithinSeconds, float Threshold)
	{
		FGustEvent Out;
		const double Within = std::min(std::max(WithinSeconds, 0.0), 120.0);
		const float Hi = 1.f + Threshold, Lo = 1.f - Threshold;
		constexpr double Step = 1.0;
		auto Beyond = [Hi, Lo](float F) { return F > Hi || F < Lo; };
		float Prev = GustAhead(S, X, Y, OffsetX, OffsetY, Seconds, 0.0);
		double At = 0.0, Start = -1.0;
		if (Beyond(Prev))
		{
			Start = 0.0;
		}
		while (Start < 0.0 && At < Within)
		{
			const double To = std::min(At + Step, Within);
			const float F = GustAhead(S, X, Y, OffsetX, OffsetY, Seconds, To);
			if (Beyond(F))
			{
				// where between the two samples it crossed the threshold
				const float Edge = F > Hi ? Hi : Lo;
				Start = At + (To - At) * Sat((Edge - Prev) / (F - Prev));
			}
			Prev = F;
			At = To;
		}
		if (Start < 0.0)
		{
			return Out;
		}
		// follow it to its end (two minutes at most) for its peak and how long it lasts
		Out.InSeconds = float(Start);
		Out.bLull = Prev < Lo;
		Out.Factor = Prev;
		double End = Start + 120.0;
		while (At < Start + 120.0)
		{
			const double To = At + Step;
			const float F = GustAhead(S, X, Y, OffsetX, OffsetY, Seconds, To);
			if (Out.bLull ? F >= Lo : F <= Hi)
			{
				const float Edge = Out.bLull ? Lo : Hi;
				End = At + (To - At) * Sat((Edge - Prev) / (F - Prev));
				break;
			}
			Out.Factor = Out.bLull ? std::min(Out.Factor, F) : std::max(Out.Factor, F);
			Prev = F;
			At = To;
		}
		Out.LastsSeconds = float(End - Start);
		return Out;
	}

	// ---------------------------------------------------------------- 4. words and numbers

	int Beaufort(float SpeedMs)
	{
		// WMO: the lowest 10 m speed of forces 1..12, m/s
		static const float From[12] = { 0.3f, 1.6f, 3.4f, 5.5f, 8.0f, 10.8f, 13.9f, 17.2f, 20.8f, 24.5f, 28.5f, 32.7f };
		int Force = 0;
		while (Force < 12 && SpeedMs >= From[Force] - 1e-4f)
		{
			Force++;
		}
		return Force;
	}

	const char* BeaufortKey(int Force)
	{
		static const char* const Keys[13] = { "calm", "light_air", "light_breeze", "gentle_breeze", "moderate_breeze", "fresh_breeze",
			"strong_breeze", "near_gale", "gale", "strong_gale", "storm", "violent_storm", "hurricane" };
		return Keys[std::min(std::max(Force, 0), 12)];
	}

	const char* CompassKey(float Deg, int Points)
	{
		static const char* const Keys[16] = { "n", "nne", "ne", "ene", "e", "ese", "se", "sse", "s", "ssw", "sw", "wsw", "w", "wnw", "nw", "nnw" };
		if (Points != 4 && Points != 16)
		{
			Points = 8;
		}
		const int Index = int(std::floor(Wrap360(Deg) * float(Points) / 360.f + 0.5f)) % Points;
		return Keys[Index * (16 / Points)];
	}

	FWindOutlook Outlook(double Hours, double SpanHours, const FWindParams& P)
	{
		FWindOutlook Out;
		const double Span = std::max(SpanHours, 0.0);
		const int N = std::max(int(std::ceil(Span * 6.0 - 1e-9)), 0) + 1;
		const double Step = N > 1 ? Span / (N - 1) : 0.0;
		double Sum = 0.0, VX = 0.0, VY = 0.0, SumT = 0.0, SumTT = 0.0, SumTS = 0.0;
		float First = 0.f, Last = 0.f, Lowest = 0.f, Highest = 0.f, Gust = 0.f, LastDirX = 0.f, LastDirY = 1.f;
		for (int I = 0; I < N; I++)
		{
			const double At = Step * I;
			const FWindState R = Regime(Hours + At, P);
			FWindShift Shift;
			ShiftCore(Hours + At, MakeBasis(R, P.Seed), Shift);
			const FWindState S = Shifted(R, Shift);
			Lowest = I == 0 ? S.Speed : std::min(Lowest, S.Speed);
			Highest = I == 0 ? S.Speed : std::max(Highest, S.Speed);
			Gust = std::max(Gust, S.Speed * (1.f + 2.2f * S.Gustiness * 0.8f));
			Sum += S.Speed;
			VX += S.DirX * S.Speed;
			VY += S.DirY * S.Speed;
			SumT += At;
			SumTT += At * At;
			SumTS += At * S.Speed;
			// the turn is the weather's own: a swing of a few minutes at either end is not a change of wind
			if (I == 0)
			{
				First = R.FromDeg;
			}
			Last = R.FromDeg;
			LastDirX = S.DirX;
			LastDirY = S.DirY;
		}
		Out.MinSpeed = Lowest;
		Out.MaxSpeed = Highest;
		Out.MeanSpeed = float(Sum / N);
		Out.GustSpeed = Gust;
		// in a dead calm the vectors sum to nothing: then the last bearing stands
		Out.FromDeg = VX * VX + VY * VY > 1e-12 ? BearingFrom(float(VX), float(VY)) : BearingFrom(LastDirX, LastDirY);
		Out.TurnDeg = Wrap180(Last - First);
		const double Den = N * SumTT - SumT * SumT;
		Out.Trend = Den > 1e-9 ? float((N * SumTS - SumT * Sum) / Den) : 0.f;
		Out.Force = Beaufort(Out.MeanSpeed);
		Out.ForceMax = Beaufort(Out.GustSpeed);
		return Out;
	}

	double NextSpell(double Hours, const FWindParams& P, float MinSpeed, float MaxSpeed, double HoldHours, double WithinHours, bool bDaylight)
	{
		// the mean wind is the regime's: a puff of the bay's breathing does not end a spell
		constexpr double Step = 1.0 / 12.0;
		const double Hold = std::max(HoldHours, 0.0);
		double Start = -1.0;
		for (double At = 0.0; At <= WithinHours + Hold + Step; At += Step)
		{
			const FWindState R = Regime(Hours + At, P);
			const bool bGood = R.Speed >= MinSpeed && R.Speed <= MaxSpeed && (!bDaylight || R.Day > 0.f);
			if (!bGood)
			{
				Start = -1.0;
				if (At > WithinHours)
				{
					break;
				}
				continue;
			}
			if (Start < 0.0)
			{
				if (At > WithinHours)
				{
					break;
				}
				Start = At;
			}
			if (At - Start >= Hold - 1e-9)
			{
				return Start;
			}
		}
		return -1.0;
	}

	// ---------------------------------------------------------------- 5. things that move

	float BearingFrom(float VX, float VY)
	{
		// the bearing of the point the air comes from: opposite to where it goes (north +Y, east -X)
		const float D = std::atan2(VX, -VY) * float(180.0 / Pi);
		return D < 0.f ? D + 360.f : (D >= 360.f ? 0.f : D);
	}

	void TowardOf(float FromDeg, float& OutX, float& OutY)
	{
		const float R = FromDeg * float(Pi / 180.0);
		OutX = std::sin(R);
		OutY = -std::cos(R);
	}

	float AngleOff(float HeadingDeg, float FromDeg)
	{
		return Wrap180(FromDeg - HeadingDeg);
	}
}
