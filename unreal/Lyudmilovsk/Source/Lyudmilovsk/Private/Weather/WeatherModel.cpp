#include "WeatherModel.h"
#include "RainModel.h"
#include "WindModel.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstring>
#include <mutex>

namespace LyudWeather
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr int NR = int(ERegime::Count);

		double Clamp(double V, double A, double B) { return std::min(std::max(V, A), B); }
		double Clamp01(double V) { return Clamp(V, 0.0, 1.0); }
		double Lerp(double A, double B, double T) { return A + (B - A) * T; }
		double SmoothStep(double A, double B, double X)
		{
			const double T = Clamp01((X - A) / (B - A));
			return T * T * (3.0 - 2.0 * T);
		}

		uint32_t HashU(uint32_t X, uint32_t Y, uint32_t Seed)
		{
			uint32_t H = X * 0x8da6b343u ^ Y * 0xd8163841u ^ Seed * 0x165667b1u ^ 0x5bd1e995u;
			H = (H ^ (H >> 15)) * 0x2c1b3c6du;
			H = (H ^ (H >> 12)) * 0x297a2d39u;
			H ^= H >> 15;
			return H;
		}
		double Hash01(uint32_t X, uint32_t Y, uint32_t Seed) { return double(HashU(X, Y, Seed) >> 8) * (1.0 / 16777215.0); }

		/** 1 in mid-winter (the coldest day) .. 0 in mid-summer. */
		double Winterness(double Doy, double Coldest) { return 0.5 + 0.5 * std::cos(2.0 * Pi * (Doy - Coldest) / 365.25); }

		/** Month 0..11 of a day of the year (near enough for climate tables). */
		int MonthOf(double Doy) { return std::min(11, std::max(0, int((Doy - 1.0) / 30.44))); }

		/** The sun's elevation, degrees (simple declination and hour angle; ALyudSky has the precise one). */
		double SunElevation(double Hours, const FClimate& C)
		{
			const double Doy = DayOfYearOf(Hours);
			const double Hod = Hours - std::floor(Hours / 24.0) * 24.0;
			const double Decl = 23.44 * std::sin(2.0 * Pi * (284.0 + Doy) / 365.0) * Pi / 180.0;
			const double Solar = Hod - 2.0 + C.LongitudeDeg / 15.0;
			const double Ha = (Solar - 12.0) * 15.0 * Pi / 180.0;
			const double Lat = C.LatitudeDeg * Pi / 180.0;
			return std::asin(std::sin(Lat) * std::sin(Decl) + std::cos(Lat) * std::cos(Decl) * std::cos(Ha)) * 180.0 / Pi;
		}

		/** Saturation vapour pressure (Magnus), hPa. */
		double Es(double T) { return 6.112 * std::exp(17.62 * T / (243.12 + T)); }

		// ---- the chain's climate: how likely each type follows each, and how often it comes in each month ----

		// base transitions [from][to] (the cyclone's life cycle)
		const double Transition[NR][NR] = {
			//           High  WarmF WarmS ColdF Show  Occl  Arct  Thund
			/* High  */ { 0.15, 0.60, 0.00, 0.00, 0.00, 0.12, 0.15, 0.10 },
			/* WarmF */ { 0.00, 0.00, 0.75, 0.00, 0.00, 0.25, 0.00, 0.00 },
			/* WarmS */ { 0.00, 0.00, 0.00, 0.80, 0.00, 0.00, 0.00, 0.25 },
			/* ColdF */ { 0.15, 0.00, 0.00, 0.00, 0.85, 0.00, 0.00, 0.00 },
			/* Show  */ { 0.45, 0.35, 0.00, 0.00, 0.00, 0.00, 0.20, 0.00 },
			/* Occl  */ { 0.30, 0.20, 0.00, 0.00, 0.50, 0.00, 0.00, 0.00 },
			/* Arct  */ { 0.35, 0.55, 0.00, 0.00, 0.00, 0.00, 0.10, 0.00 },
			/* Thund */ { 0.20, 0.00, 0.00, 0.60, 0.20, 0.00, 0.00, 0.00 },
		};
		// how often a type is chosen in each month (Jan..Dec), on top of the transitions
		const double MonthWeight[NR][12] = {
			/* High  */ { 0.8, 0.9, 1.0, 1.2, 1.6, 1.3, 1.0, 1.1, 1.2, 0.9, 0.7, 0.8 },
			/* WarmF */ { 1.2, 1.1, 1.0, 0.8, 0.6, 0.8, 0.9, 1.0, 1.1, 1.3, 1.4, 1.3 },
			/* WarmS */ { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 },
			/* ColdF */ { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 },
			/* Show  */ { 1.0, 1.0, 1.0, 1.0, 0.9, 1.0, 1.0, 1.0, 1.0, 1.1, 1.1, 1.0 },
			/* Occl  */ { 1.0, 1.0, 0.9, 0.8, 0.7, 0.8, 0.9, 1.0, 1.0, 1.1, 1.2, 1.1 },
			/* Arct  */ { 0.8, 0.75, 0.3, 0.03, 0.0, 0.0, 0.0, 0.0, 0.0, 0.02, 0.2, 0.55 },
			/* Thund */ { 0.0, 0.0, 0.0, 0.0, 0.3, 1.2, 1.6, 1.2, 0.35, 0.0, 0.0, 0.0 },
		};
		// episode lengths, hours
		const double MinHours[NR] = { 24, 8, 6, 4, 12, 14, 48, 10 };
		const double MaxHours[NR] = { 110, 16, 30, 8, 40, 40, 120, 30 };
		// how stormy each month's lows are (deepens Intensity)
		const double Storminess[12] = { 0.75, 0.65, 0.55, 0.4, 0.3, 0.3, 0.35, 0.4, 0.5, 0.65, 0.8, 0.8 };

		// ---- the shared climate ----
		std::mutex ClimateLock;
		FClimate CurrentClimate;
		std::atomic<uint32_t> ClimateVersion{ 1 };
		std::atomic<double> Shift{ 0.0 };
	}

	const char* RegimeName(ERegime R)
	{
		static const char* N[] = { "High", "WarmFront", "WarmSector", "ColdFront", "Showery", "Occluded", "ArcticHigh", "ThunderyLow" };
		return int(R) < NR ? N[int(R)] : "?";
	}
	const char* PrecipName(EPrecip P)
	{
		static const char* N[] = { "None", "Drizzle", "Rain", "Showers", "Thunderstorm", "Hail", "Graupel", "Sleet", "Snow", "SnowShowers", "FreezingDrizzle", "FreezingRain" };
		return int(P) < int(EPrecip::Count) ? N[int(P)] : "?";
	}
	const char* CloudName(ECloud C)
	{
		static const char* N[] = { "None", "Ci", "Cc", "Cs", "Ac", "As", "AcLen", "Ns", "Sc", "St", "Cu", "CuCon", "Cb" };
		return int(C) < int(ECloud::Count) ? N[int(C)] : "?";
	}
	const char* FogName(EFog F)
	{
		static const char* N[] = { "None", "Radiation", "Advection", "Steam", "Frontal" };
		return int(F) < int(EFog::Count) ? N[int(F)] : "?";
	}
	ERegime ParseRegime(const char* Name)
	{
		for (int I = 0; I < NR; I++)
		{
			const char* A = RegimeName(ERegime(I));
			const char* B = Name;
			while (*A && *B && std::tolower(*A) == std::tolower(*B)) { A++; B++; }
			if (!*A && !*B) return ERegime(I);
		}
		return ERegime::Count;
	}

	double SunElevationDeg(double Hours, const FClimate& C) { return SunElevation(Hours, C); }

	double YearOf(double Hours)
	{
		// close enough for the solar cycle: 2000 + days / 365.2425
		return 2000.0 + Hours / (24.0 * 365.2425);
	}

	void SetClimate(const FClimate& C)
	{
		std::lock_guard<std::mutex> L(ClimateLock);
		if (std::memcmp(&C, &CurrentClimate, sizeof(FClimate)) != 0)
		{
			CurrentClimate = C;
			ClimateVersion++;
		}
	}

	FClimate GetClimate()
	{
		std::lock_guard<std::mutex> L(ClimateLock);
		return CurrentClimate;
	}

	void SetHourShift(double Hours) { Shift.store(Hours); }
	double GetHourShift() { return Shift.load(); }

	FChain& SharedChain()
	{
		thread_local FChain* Chain = nullptr;
		thread_local uint32_t Version = 0;
		const uint32_t V = ClimateVersion.load();
		if (!Chain || Version != V)
		{
			delete Chain;
			Chain = new FChain(GetClimate());
			Version = V;
		}
		return *Chain;
	}

	// ------------------------------------------------------------------------------------------------ the chain

	namespace
	{
		/** The chain starts on 2020-01-01 in a high; a few weeks in it has forgotten that. */
		const double Epoch = 20.0 * 365.2425 * 24.0 - 15.0 * 24.0;
	}

	FChain::FChain(const FClimate& C) : Climate(C)
	{
		Episodes.reserve(2048);
	}

	void FChain::Extend(int Count)
	{
		const uint32_t Seed = Climate.Seed;
		if (Episodes.empty())
		{
			FEpisode E;
			E.Start = Epoch;
			E.End = Epoch + 48.0;
			E.Regime = ERegime::High;
			E.Key = HashU(0, 7, Seed);
			Episodes.push_back(E);
		}
		while (int(Episodes.size()) < Count)
		{
			const FEpisode& Prev = Episodes.back();
			const uint32_t I = uint32_t(Episodes.size());
			const double Doy = DayOfYearOf(Prev.End);
			const int Month = MonthOf(Doy);
			// the next type: the life cycle's transitions times the month's climate
			double W[NR];
			double Sum = 0.0;
			for (int R = 0; R < NR; R++)
			{
				double S = MonthWeight[R][Month];
				if (ERegime(R) == ERegime::ArcticHigh) S *= Climate.WinterSeverity;
				// two highs in a row are a blocking spell: rarer the longer the last one was
				W[R] = Transition[int(Prev.Regime)][R] * S;
				Sum += W[R];
			}
			if (Sum <= 0.0)
			{
				W[int(ERegime::High)] = 1.0;
				Sum = 1.0;
			}
			double U = Hash01(I, 1, Seed) * Sum;
			int Next = 0;
			for (; Next < NR - 1; Next++)
			{
				U -= W[Next];
				if (U <= 0.0) break;
			}
			FEpisode E;
			E.Regime = ERegime(Next);
			E.Start = Prev.End;
			const double Storm = Storminess[Month];
			// deep lows in the stormy months; highs stronger in mid-winter
			E.Intensity = float(std::pow(Hash01(I, 2, Seed), 1.6 - Storm));
			E.Bias = float(Hash01(I, 3, Seed) * 2.0 - 1.0);
			E.Key = HashU(I, 4, Seed);
			// a slow, deep low takes longer; a fast small wave is short
			const double L = Hash01(I, 5, Seed);
			double Dur = Lerp(MinHours[Next], MaxHours[Next], L * L * (0.6 + 0.4 * (1.0 - Storm)) + 0.25 * L);
			if (E.Regime == ERegime::High && Month >= 3 && Month <= 5) Dur *= 1.25;  // the dry sunny late spring
			E.End = E.Start + std::max(Dur, MinHours[Next] * 0.8);
			Episodes.push_back(E);
		}
	}

	void FChain::ExtendTo(double Hours)
	{
		if (Episodes.empty()) Extend(1);
		while (Episodes.back().End <= Hours + 240.0)
		{
			Extend(int(Episodes.size()) + 64);
		}
	}

	const FEpisode& FChain::At(double Hours, int* OutIndex)
	{
		ExtendTo(Hours);
		int Lo = 0, Hi = int(Episodes.size()) - 1;
		if (Hours <= Episodes[0].Start)
		{
			Hi = 0;
		}
		while (Lo < Hi)
		{
			const int Mid = (Lo + Hi + 1) / 2;
			if (Episodes[Mid].Start <= Hours) Lo = Mid; else Hi = Mid - 1;
		}
		if (OutIndex) *OutIndex = Lo;
		return Episodes[Lo];
	}

	// ------------------------------------------------------------------------------------------------ profiles

	namespace
	{
		/** What an episode asks of the weather at one point of its course; blended across episodes' edges. */
		struct FProf
		{
			double P = 1013.0;          // pressure hPa
			double TA = 0.0;            // air mass temperature anomaly, C
			double Spread = 4.0;        // dew-point depression of the air mass, C
			double FromX = 0.0, FromY = 0.0, Speed = 5.0, Gust = 0.0;
			double UpX = 0.0, UpY = 0.0, UpSpeed = 20.0;
			double LowCov = 0.0, LowBase = 800.0, LowTop = 1500.0, LowDen = 0.5;
			double MidCov = 0.0, MidBase = 3500.0, MidTop = 4500.0, MidDen = 0.4;
			double HighCov = 0.0, HighBase = 8000.0, HighDen = 0.2;
			double Strat = 0.0;         // stratiform precipitation, mm/h
			double Cells = 0.0;         // share of sky in shower cells
			double CellRate = 0.0;      // mm/h inside a cell
			double Instab = 0.0;        // 0..1+ how readily the air convects
			double Stable = 0.0;        // 0..1 inversion
			double WarmNose = 0.0;      // 0..1 warm layer aloft (freezing rain over frozen ground)
			double Drizzle = 0.0;       // 0..1 how drizzly the low deck is
			double ThunderBoost = 0.0;
			ECloud LowType = ECloud::None, MidType = ECloud::None, HighType = ECloud::None;
		};

		void AddBearing(FProf& F, double FromDeg, double Weight)
		{
			F.FromX += std::sin(FromDeg * Pi / 180.0) * Weight;
			F.FromY += std::cos(FromDeg * Pi / 180.0) * Weight;
		}
		/** From bearing A to B the short way round (veering when it is nearly a reversal). */
		double LerpBearing(double A, double B, double T)
		{
			double D = std::fmod(B - A + 540.0, 360.0) - 180.0;
			if (D < -165.0) D += 360.0;   // a near-reversal veers (both ways round would otherwise flicker)
			return A + D * T;
		}
		/** Turns the profile's wind towards a bearing (A = 0 keep .. 1 there), as ahead of a front. */
		void SteerBearing(FProf& F, double ToDeg, double A, double RefDeg)
		{
			const double Mag = std::sqrt(F.FromX * F.FromX + F.FromY * F.FromY);
			const double Cur = std::atan2(F.FromX, F.FromY) * 180.0 / Pi;
			// the short way, but a near-reversal (judged from the episode's own bearing, so it can't flicker) backs
			const double RefArc = std::fmod(ToDeg - RefDeg + 540.0, 360.0) - 180.0;
			double Arc = std::fmod(ToDeg - Cur + 540.0, 360.0) - 180.0;
			if (RefArc > 140.0 && Arc > 90.0) Arc -= 360.0;
			if (RefArc < -140.0 && Arc < -90.0) Arc += 360.0;
			const double D = (Cur + Arc * A) * Pi / 180.0;
			F.FromX = std::sin(D) * Mag;
			F.FromY = std::cos(D) * Mag;
		}
		void AddUpper(FProf& F, double FromDeg, double Weight)
		{
			F.UpX += std::sin(FromDeg * Pi / 180.0) * Weight;
			F.UpY += std::cos(FromDeg * Pi / 180.0) * Weight;
		}

		struct FCtx
		{
			double Hours, Doy, W, Summer;
			const FClimate* C;
		};

		/** An episode's profile at fraction T of its course (Next = what follows it, for the approach of a front). */
		FProf Profile(const FEpisode& E, ERegime Prev, ERegime Next, double T, const FCtx& X)
		{
			FProf F;
			const double I = E.Intensity, B = E.Bias, W = X.W, S = X.Summer;
			const double Base = 5.0 + 2.5 * W;                // the season's breeze, m/s
			const double Jet = 14.0 + 14.0 * W + 8.0 * I;     // the upper wind
			const double Hours = (E.End - E.Start) * (1.0 - T);
			switch (E.Regime)
			{
			case ERegime::High:
			{
				// behind a front the ridge builds from the north-west; later the wind comes from wherever the centre lies
				const bool bBehind = Prev == ERegime::ColdFront || Prev == ERegime::Showery || Prev == ERegime::Occluded;
				const double Build = bBehind ? SmoothStep(0.0, 0.35, T) : 1.0;
				F.P = Lerp(1012.0, 1020.0 + 12.0 * I, Build);
				F.TA = -2.5 * W + 2.5 * S + 1.5 * B;
				F.Spread = 3.0 + 4.0 * S - 1.5 * W;
				AddBearing(F, LerpBearing(315.0, 90.0 + 120.0 * B, Build), 1.0);
				F.Speed = Base * (0.45 + 0.2 * (1.0 - I));
				AddUpper(F, 280.0 + 40.0 * B, 1.0);
				F.UpSpeed = Jet * 0.6;
				F.Instab = 0.55 * S;
				F.Stable = 0.4 + 0.5 * W;
				F.HighCov = 0.12 * (0.5 + 0.5 * B);
				F.HighType = ECloud::Cirrus;
				// a winter high under its inversion: a flat grey Sc deck for days ('anticyclonic gloom')
				if (W > 0.55 && B > 0.15)
				{
					F.LowCov = 0.92 * SmoothStep(0.55, 0.8, W);
					F.LowType = ECloud::Stratocumulus;
					F.LowBase = 500.0 + 300.0 * I;
					F.LowTop = F.LowBase + 400.0;
					F.LowDen = 0.55;
					F.Drizzle = 0.15;
					F.Spread = 1.5;
				}
				// the next front's cirrus streams in over the last day, then thickens; the wind backs and freshens
				if (Next == ERegime::WarmFront || Next == ERegime::Occluded)
				{
					const double A = SmoothStep(22.0, 0.0, Hours);
					F.HighCov = Lerp(F.HighCov, 0.75, A);
					F.HighType = A > 0.6 ? ECloud::Cirrostratus : (A > 0.25 ? ECloud::Cirrus : F.HighType);
					F.HighDen = Lerp(0.15, 0.3, A);
					F.MidCov = 0.25 * SmoothStep(0.6, 1.0, A);
					F.MidType = ECloud::Altocumulus;
					F.P -= 6.0 * A;
					SteerBearing(F, 165.0, A, 90.0 + 120.0 * B);
					F.Speed = Lerp(F.Speed, Base * 0.9, A);
					F.UpSpeed = Lerp(F.UpSpeed, Jet, A);
				}
				break;
			}
			case ERegime::WarmFront:
			{
				// cirrostratus (halo) -> altostratus -> nimbostratus and steady rain, then the front passes
				F.P = Lerp(1012.0 - 4.0 * I, 1000.0 - 16.0 * I, T);
				F.TA = 1.0 * W + 0.2 * S + Lerp(0.0, 2.0 * W, T) + 1.0 * B;
				F.Spread = Lerp(3.5, 0.4, SmoothStep(0.1, 0.65, T));
				AddBearing(F, Lerp(155.0, 195.0, T) + 15.0 * B, 1.0);
				F.Speed = Base * (1.0 + 0.6 * I) * Lerp(0.85, 1.15, T);
				F.Gust = 0.05;
				AddUpper(F, 240.0 + 15.0 * B, 1.0);
				F.UpSpeed = Jet * 1.1;
				F.Stable = 0.6;
				F.WarmNose = SmoothStep(0.45, 0.8, T);
				F.HighCov = Lerp(0.8, 1.0, SmoothStep(0.0, 0.25, T));
				F.HighType = ECloud::Cirrostratus;
				F.HighDen = Lerp(0.3, 0.5, T);
				F.MidCov = SmoothStep(0.1, 0.45, T);
				F.MidType = ECloud::Altostratus;
				F.MidBase = Lerp(4500.0, 2200.0, T);
				F.MidTop = 5500.0;
				F.MidDen = Lerp(0.35, 0.85, SmoothStep(0.15, 0.6, T));
				F.LowCov = SmoothStep(0.4, 0.7, T);
				F.LowType = ECloud::Nimbostratus;
				F.LowBase = Lerp(900.0, 350.0, T);
				F.LowTop = 4500.0;
				F.LowDen = 0.9;
				F.Strat = SmoothStep(0.45, 0.65, T) * (0.27 + 0.75 * I) * (1.0 - 0.6 * SmoothStep(0.9, 1.0, T));
				break;
			}
			case ERegime::WarmSector:
			{
				F.P = 1003.0 - 10.0 * I;
				F.TA = 3.0 * W + 2.0 * S + 1.5 * B;
				F.Spread = 1.0 + 1.5 * S * (0.5 + 0.5 * B);
				AddBearing(F, 225.0 + 15.0 * B, 1.0);
				F.Speed = Base * (1.1 + 0.5 * I);
				F.Gust = 0.05;
				AddUpper(F, 245.0, 1.0);
				F.UpSpeed = Jet;
				F.Stable = 0.5 * W + 0.3;
				F.Instab = 0.45 * S;
				F.WarmNose = 0.8;
				F.LowCov = Lerp(0.95, 0.55, S * (0.5 + 0.5 * B));
				F.LowType = S > 0.6 && B > 0.0 ? ECloud::Stratocumulus : ECloud::Stratus;
				F.LowBase = Lerp(250.0, 700.0, S);
				F.LowTop = F.LowBase + 700.0;
				F.LowDen = 0.6;
				F.Drizzle = 0.55 * W + 0.15;
				F.MidCov = 0.3;
				F.MidType = ECloud::Altocumulus;
				F.Strat = 0.15 * W;
				break;
			}
			case ERegime::ColdFront:
			{
				// the squall: towering cloud, a burst of heavy rain (snow in winter), gusts, the veer
				const double Core = SmoothStep(0.05, 0.35, T) * (1.0 - SmoothStep(0.65, 1.0, T));
				F.P = Lerp(998.0 - 12.0 * I, 1006.0 - 6.0 * I, SmoothStep(0.3, 1.0, T));
				F.TA = Lerp(3.5 * W + 1.5 * S, -1.0, SmoothStep(0.3, 0.9, T));
				F.Spread = 0.6;
				AddBearing(F, Lerp(235.0, 295.0, SmoothStep(0.25, 0.75, T)), 1.0);
				F.Speed = Base * (1.35 + 0.75 * I) * (0.85 + 0.3 * Core);
				F.Gust = 0.25 * Core + 0.1;
				AddUpper(F, 260.0, 1.0);
				F.UpSpeed = Jet * 1.2;
				F.Instab = 0.9 + 0.3 * S;
				F.LowCov = 0.75 + 0.25 * Core;
				F.LowType = ECloud::Cumulonimbus;
				F.LowBase = 700.0;
				F.LowTop = 6000.0 + 4000.0 * S;
				F.LowDen = 0.95;
				F.MidCov = 0.7;
				F.MidType = ECloud::Altostratus;
				F.MidDen = 0.7;
				F.HighCov = 0.5;
				F.HighType = ECloud::Cirrus;
				F.Strat = Core * (0.7 + 1.5 * I) * (0.6 + 0.6 * S);
				F.Cells = 0.5 * Core;
				F.CellRate = 6.0 + 10.0 * S;
				F.ThunderBoost = 0.25 * S;
				break;
			}
			case ERegime::Showery:
			{
				// cold air behind the front: towers and blue gaps, showers (off the warm sea in autumn and winter)
				const double Settle = SmoothStep(0.4, 1.0, T);
				F.P = Lerp(1006.0, 1016.0, T);
				F.TA = -2.5 + 0.5 * B - 1.5 * W * I;
				F.Spread = 3.5;
				AddBearing(F, Lerp(285.0, 320.0, T) + 20.0 * B, 1.0);
				F.Speed = Base * (1.25 + 0.4 * I) * Lerp(1.0, 0.7, Settle);
				F.Gust = 0.18;
				AddUpper(F, 300.0, 1.0);
				F.UpSpeed = Jet * 0.9;
				F.Instab = 1.0 - 0.5 * Settle;
				F.LowCov = 0.25;
				F.LowType = ECloud::Cumulus;
				F.LowBase = 600.0;
				F.LowTop = 2500.0;
				F.LowDen = 0.6;
				F.MidCov = 0.15;
				F.MidType = ECloud::Altocumulus;
				F.HighCov = 0.2;
				F.HighType = ECloud::Cirrus;
				F.Cells = (0.22 + 0.15 * I) * (1.0 - 0.7 * Settle);
				F.CellRate = 1.0 + 3.6 * S * S;
				F.ThunderBoost = 0.25 * S * S;
				if (Next == ERegime::WarmFront)
				{
					const double A = SmoothStep(20.0, 0.0, Hours);
					F.HighCov = Lerp(F.HighCov, 0.7, A);
					F.HighType = A > 0.5 ? ECloud::Cirrostratus : ECloud::Cirrus;
					F.Cells *= 1.0 - A;
					F.P -= 5.0 * A;
					SteerBearing(F, 170.0, A, 302.0 + 20.0 * B);
				}
				break;
			}
			case ERegime::Occluded:
			{
				F.P = 996.0 - 14.0 * I + 6.0 * T;
				F.TA = 0.5 * W - 1.0 * S + B;
				F.Spread = 0.8;
				AddBearing(F, Lerp(180.0, 320.0, T) + 20.0 * B, 1.0);
				F.Speed = Base * (0.9 + 0.6 * I);
				F.Gust = 0.08;
				AddUpper(F, Lerp(220.0, 300.0, T), 1.0);
				F.UpSpeed = Jet * 0.9;
				F.Stable = 0.4;
				F.Instab = 0.3;
				F.WarmNose = 0.5 * (1.0 - T);
				F.LowCov = 1.0 - 0.4 * SmoothStep(0.75, 1.0, T);
				F.LowType = T < 0.7 ? ECloud::Nimbostratus : ECloud::Stratocumulus;
				F.LowBase = 450.0;
				F.LowTop = 3500.0;
				F.LowDen = 0.85;
				F.MidCov = 0.9;
				F.MidType = ECloud::Altostratus;
				F.MidDen = 0.7;
				F.Strat = (0.25 + 0.6 * I) * (1.0 - SmoothStep(0.6, 0.95, T));
				F.Drizzle = 0.5;
				break;
			}
			case ERegime::ArcticHigh:
			{
				const double Sev = X.C->WinterSeverity;
				F.P = 1028.0 + 12.0 * I;
				F.TA = -(6.0 + 9.0 * I) * Sev * SmoothStep(0.2, 0.8, W) - 2.0 * (1.0 - W) + 1.5 * B;
				F.Spread = 3.0;
				AddBearing(F, 75.0 + 25.0 * B, 1.0);
				F.Speed = Base * (0.7 + 0.4 * I);
				AddUpper(F, 20.0 + 30.0 * B, 1.0);
				F.UpSpeed = Jet * 0.6;
				F.Stable = 0.8;
				F.LowCov = 0.08 + 0.2 * std::max(0.0, -B);
				F.LowType = ECloud::Stratocumulus;
				F.LowBase = 600.0;
				F.LowTop = 1100.0;
				F.LowDen = 0.4;
				F.Cells = 0.04;     // a few snow showers off the open sea
				F.CellRate = 0.6;
				F.Instab = 0.3;
				if (Next == ERegime::WarmFront)
				{
					const double A = SmoothStep(20.0, 0.0, Hours);
					F.HighCov = 0.7 * A;
					F.HighType = ECloud::Cirrostratus;
					F.P -= 8.0 * A;
					SteerBearing(F, 140.0, A, 75.0 + 25.0 * B);
					F.Speed = Lerp(F.Speed, Base, A);
				}
				break;
			}
			case ERegime::ThunderyLow:
			{
				F.P = 1008.0 - 4.0 * I;
				F.TA = 5.0 + 1.5 * B;
				F.Spread = 3.0 - 1.0 * I;
				AddBearing(F, 170.0 + 40.0 * B, 1.0);
				F.Speed = Base * 0.55;
				F.Gust = 0.1;
				AddUpper(F, 200.0 + 30.0 * B, 1.0);
				F.UpSpeed = Jet * 0.8;
				F.Instab = 1.3;
				F.LowCov = 0.3;
				F.LowType = ECloud::CumulusCongestus;
				F.LowBase = 1100.0;
				F.LowTop = 6000.0;
				F.LowDen = 0.7;
				F.MidCov = 0.45;
				F.MidType = ECloud::Altocumulus;   // castellanus in the morning
				F.HighCov = 0.4;
				F.HighType = ECloud::Cirrus;
				F.Cells = 0.25 + 0.25 * I;
				F.CellRate = 8.0 + 18.0 * I;
				F.ThunderBoost = 0.6;
				break;
			}
			default:
				break;
			}
			return F;
		}

		/** How sharp the boundary between two types is, hours (half width of the cross-fade). */
		double EdgeHours(ERegime A, ERegime B)
		{
			// the fronts themselves are sharp; what follows them settles in over hours
			if (B == ERegime::ColdFront) return 1.2;
			if (A == ERegime::WarmFront && B == ERegime::WarmSector) return 1.5;
			if (A == ERegime::ColdFront) return 3.0;
			return 5.0;
		}

		/** A += (B - A) * K; bAEarlier says which side of the boundary A is, so both sides turn the wind the same way. */
		void Mix(FProf& A, const FProf& B, double K, bool bAEarlier, double Sense)
		{
			if (K <= 0.0) return;
			auto L = [K](double& X, double Y) { X += (Y - X) * K; };
			L(A.P, B.P); L(A.TA, B.TA); L(A.Spread, B.Spread);
			// directions turn the short way round, not through the vectors' cancelling sum
			// (the short way, but a near-reversal always veers, as when a low passes to the north)
			auto Turn = [K, bAEarlier, Sense](double& X, double& Y, double BX, double BY)
			{
				const double MA = std::sqrt(X * X + Y * Y), MB = std::sqrt(BX * BX + BY * BY);
				const double DA = std::atan2(X, Y) * 180.0 / Pi, DB = std::atan2(BX, BY) * 180.0 / Pi;
				const double Early = bAEarlier ? DA : DB, Late = bAEarlier ? DB : DA;
				double Arc = std::fmod(Late - Early + 540.0, 360.0) - 180.0;
				if (Sense > 0.0 && Arc < -90.0) Arc += 360.0;
				if (Sense < 0.0 && Arc > 90.0) Arc -= 360.0;
				if (Sense == 0.0 && Arc < -165.0) Arc += 360.0;
				const double Deg = bAEarlier ? Early + Arc * K : Late - Arc * K;
				const double D = Deg * Pi / 180.0;
				const double M = MA + (MB - MA) * K;
				X = std::sin(D) * M;
				Y = std::cos(D) * M;
			};
			Turn(A.FromX, A.FromY, B.FromX, B.FromY);
			auto TurnUp = [K, bAEarlier](double& X, double& Y, double BX, double BY)
			{
				const double MA = std::sqrt(X * X + Y * Y), MB = std::sqrt(BX * BX + BY * BY);
				const double D = (bAEarlier ? LerpBearing(std::atan2(X, Y) * 180.0 / Pi, std::atan2(BX, BY) * 180.0 / Pi, K)
					: LerpBearing(std::atan2(BX, BY) * 180.0 / Pi, std::atan2(X, Y) * 180.0 / Pi, 1.0 - K)) * Pi / 180.0;
				const double M = MA + (MB - MA) * K;
				X = std::sin(D) * M;
				Y = std::cos(D) * M;
			};
			TurnUp(A.UpX, A.UpY, B.UpX, B.UpY);
			L(A.Speed, B.Speed); L(A.Gust, B.Gust); L(A.UpSpeed, B.UpSpeed);
			// a layer's type goes with whichever side has more of it
			if (B.LowCov * K > A.LowCov * (1.0 - K)) A.LowType = B.LowType;
			if (B.MidCov * K > A.MidCov * (1.0 - K)) A.MidType = B.MidType;
			if (B.HighCov * K > A.HighCov * (1.0 - K)) A.HighType = B.HighType;
			L(A.LowCov, B.LowCov); L(A.LowBase, B.LowBase); L(A.LowTop, B.LowTop); L(A.LowDen, B.LowDen);
			L(A.MidCov, B.MidCov); L(A.MidBase, B.MidBase); L(A.MidTop, B.MidTop); L(A.MidDen, B.MidDen);
			L(A.HighCov, B.HighCov); L(A.HighBase, B.HighBase); L(A.HighDen, B.HighDen);
			L(A.Strat, B.Strat); L(A.Cells, B.Cells); L(A.CellRate, B.CellRate); L(A.Instab, B.Instab); L(A.Stable, B.Stable);
			L(A.WarmNose, B.WarmNose); L(A.Drizzle, B.Drizzle); L(A.ThunderBoost, B.ThunderBoost);
		}

		FCtx MakeCtx(double Hours, const FClimate& C)
		{
			FCtx X;
			X.Hours = Hours;
			X.Doy = DayOfYearOf(Hours);
			X.W = Winterness(X.Doy, C.ColdestDay);
			X.Summer = 1.0 - X.W;
			X.C = &C;
			return X;
		}

		/** The blended profile at Hours, with the episode it is in. */
		FProf Blended(double Hours, FChain& Chain, int* OutIndex = nullptr)
		{
			int I = 0;
			const FEpisode E = Chain.At(Hours, &I);
			const FEpisode Prev = I > 0 ? Chain.Index(I - 1) : E;
			const FEpisode Next = Chain.Index(I + 1);
			const FEpisode Next2 = Chain.Index(I + 2);
			const ERegime PP = I > 1 ? Chain.Index(I - 2).Regime : Prev.Regime;
			const FCtx X = MakeCtx(Hours, Chain.GetClimate());
			const double Len = std::max(E.End - E.Start, 0.1);
			const double T = Clamp01((Hours - E.Start) / Len);
			FProf F = Profile(E, Prev.Regime, Next.Regime, T, X);
			const double WL = std::min(EdgeHours(Prev.Regime, E.Regime), 0.45 * std::min(Len, Prev.End - Prev.Start));
			const double WR = std::min(EdgeHours(E.Regime, Next.Regime), 0.45 * std::min(Len, Next.End - Next.Start));
			const double DL = Hours - E.Start, DR = E.End - Hours;
			// which way the wind turns over a boundary is decided once from the two ends (and the weather's sense:
			// it backs ahead of a warm front or an occlusion, veers otherwise), so both sides of it agree
			auto SenseOf = [](const FProf& Early, const FProf& Late, ERegime LateRegime)
			{
				const double A = std::atan2(Early.FromX, Early.FromY) * 180.0 / Pi, B = std::atan2(Late.FromX, Late.FromY) * 180.0 / Pi;
				const double Arc = std::fmod(B - A + 540.0, 360.0) - 180.0;
				if (std::fabs(Arc) < 120.0) return Arc >= 0.0 ? 1.0 : -1.0;
				return LateRegime == ERegime::WarmFront || LateRegime == ERegime::Occluded ? -1.0 : 1.0;
			};
			if (I > 0 && DL < WL)
			{
				const FProf PEnd = Profile(Prev, PP, E.Regime, 1.0, X);
				const FProf EStart = Profile(E, Prev.Regime, Next.Regime, 0.0, X);
				Mix(F, PEnd, 0.5 * (1.0 - SmoothStep(0.0, 1.0, DL / WL)), false, SenseOf(PEnd, EStart, E.Regime));
			}
			if (DR < WR)
			{
				const FProf EEnd = Profile(E, Prev.Regime, Next.Regime, 1.0, X);
				const FProf NStart = Profile(Next, E.Regime, Next2.Regime, 0.0, X);
				Mix(F, NStart, 0.5 * (1.0 - SmoothStep(0.0, 1.0, DR / WR)), true, SenseOf(EEnd, NStart, Next.Regime));
			}
			if (OutIndex) *OutIndex = I;
			return F;
		}

		double PrevailingTempC(double Doy, const FClimate& C)
		{
			return C.MeanTempC - C.YearSwingC * std::cos(2.0 * Pi * (Doy - C.ColdestDay) / 365.25);
		}

		double ClimateSeaC(double Doy, const FClimate& C)
		{
			return std::max(C.SeaMeanC - C.SeaSwingC * std::cos(2.0 * Pi * (Doy - C.SeaColdestDay) / 365.25), 0.5);
		}

		/** The geomagnetic Kp: quiet days, 27-day recurrent coronal-hole storms and CME storms, by the solar cycle. */
		double KpAt(double Hours, const FClimate& C)
		{
			const double Year = YearOf(Hours);
			// cycle 25 (max ~2024.9) and 26 (~2036): activity 0.15 at minimum .. 1 at maximum
			auto Cycle = [](double Y, double Max) { const double D = (Y - Max) / 3.4; return std::exp(-D * D); };
			const double Act = C.SolarActivity * (0.15 + 0.85 * std::max(Cycle(Year, C.SolarMaxYear), Cycle(Year, C.SolarMaxYear + 11.0)));
			const uint32_t Seed = C.Seed ^ 0xA0u;
			double Kp = 1.3 + 0.9 * (0.5 + 0.5 * SmoothNoise1(Hours / 9.0, Seed)) + 0.5 * Act;
			const double Day = std::floor(Hours / 24.0);
			for (int Back = 0; Back <= 3; Back++)
			{
				const double D = Day - Back;
				const uint32_t K = uint32_t(int64_t(D));
				// CME storms: more near the maximum; the rare big ones Kp 8-9
				const double P = 0.025 + 0.07 * Act;
				if (Hash01(K, 11, Seed) < P)
				{
					const double U = Hash01(K, 12, Seed);
					const double Peak = 5.0 + 4.0 * std::pow(U, 2.2 - 0.8 * Act);
					const double Onset = D * 24.0 + 24.0 * Hash01(K, 13, Seed);
					const double Dt = Hours - Onset;
					if (Dt > -3.0)
					{
						const double Rise = SmoothStep(-3.0, 0.0, Dt);
						const double Decay = Dt <= 0.0 ? 1.0 : std::exp(-Dt / (10.0 + 12.0 * U));
						Kp = std::max(Kp, 2.0 + (Peak - 2.0) * Rise * Decay);
					}
				}
			}
			// recurrent coronal-hole streams: every solar rotation, Kp 4-6 for a day or so (more in the declining phase)
			const double Rot = 27.27 * 24.0;
			const double Phase = std::fmod(Hours, Rot) / Rot;
			const uint32_t RotI = uint32_t(int64_t(std::floor(Hours / Rot)));
			const double HolePhase = Hash01(uint32_t(int64_t(std::floor(Year))), 21, Seed);
			const double D = std::fabs(Phase - HolePhase);
			const double Near = std::exp(-std::pow(std::min(D, 1.0 - D) * 27.27 / 0.8, 2.0));
			Kp = std::max(Kp, 2.0 + (2.2 + 1.6 * Hash01(RotI, 22, Seed)) * Near * (0.4 + 0.6 * Act));
			return Clamp(Kp, 0.0, 9.0);
		}
	}

	// ------------------------------------------------------------------------------------------------ the instant

	namespace
	{
		FWeatherState InstantWith(double Hours, FChain& Chain, double SeaTempC, double IceCover, double SnowDepthCm)
		{
			const FClimate& C = Chain.GetClimate();
			const uint32_t Seed = C.Seed;
			FWeatherState S;
			S.Hours = Hours;
			int I = 0;
			const FProf F = Blended(Hours, Chain, &I);
			const FEpisode E = Chain.Index(I);
			const FEpisode N = Chain.Index(I + 1);
			S.Regime = E.Regime;
			S.NextRegime = N.Regime;
			S.RegimeProgress = float(Clamp01((Hours - E.Start) / std::max(E.End - E.Start, 0.1)));
			S.HoursToNext = float(E.End - Hours);
			S.Intensity = E.Intensity;
			S.PressureHPa = float(F.P + 2.0 * SmoothNoise1(Hours / 11.0, Seed + 40));
			{
				const FProf B = Blended(Hours - 3.0, Chain);
				S.PressureTendency = float(F.P - B.P + 2.0 * (SmoothNoise1(Hours / 11.0, Seed + 40) - SmoothNoise1((Hours - 3.0) / 11.0, Seed + 40)));
			}

			const FCtx X = MakeCtx(Hours, C);
			const double Hod = Hours - std::floor(Hours / 24.0) * 24.0;
			const double Sun = SunElevation(Hours, C);
			const double SunUp = SmoothStep(-2.0, 25.0, Sun);

			// wind of the weather
			double FromDeg = std::atan2(F.FromX, F.FromY) * 180.0 / Pi;
			FromDeg += 14.0 * SmoothNoise1(Hours / 7.0, Seed + 41);
			if (FromDeg < 0.0) FromDeg += 360.0;
			S.WindFromDeg = float(std::fmod(FromDeg, 360.0));
			S.WindSpeed = float(Clamp(F.Speed * (1.0 + 0.15 * SmoothNoise1(Hours / 5.0, Seed + 42)), 0.3, 24.0));
			S.WindGustiness = float(F.Gust);
			double UpDeg = std::atan2(F.UpX, F.UpY) * 180.0 / Pi;
			if (UpDeg < 0.0) UpDeg += 360.0;
			S.UpperFromDeg = float(UpDeg);
			S.UpperSpeed = float(F.UpSpeed * (1.0 + 0.12 * SmoothNoise1(Hours / 9.0, Seed + 43)));

			// clouds before the sun's effect on the air (the day's swing needs the cover)
			double LowCov = Clamp01(F.LowCov + 0.12 * SmoothNoise1(Hours / 3.0, Seed + 44) * F.LowCov);
			double MidCov = Clamp01(F.MidCov * (1.0 + 0.25 * SmoothNoise1(Hours / 4.0, Seed + 45)));
			double HighCov = Clamp01(F.HighCov * (1.0 + 0.3 * SmoothNoise1(Hours / 5.0, Seed + 46)));

			// the air: the season, the air mass, the day's swing (damped by cloud and wind), the sea's pull onshore
			const double Air = PrevailingTempC(X.Doy, C) + F.TA + 1.6 * SmoothNoise1(Hours / 90.0, Seed + 47);
			const double Opaque = Clamp01(LowCov * F.LowDen + 0.6 * MidCov * F.MidDen + 0.15 * HighCov);
			const double Swing = C.DaySwingC * (0.65 + 0.7 * X.Summer) * (1.0 - 0.75 * Opaque)
				* (1.0 - 0.5 * SmoothStep(3.0, 12.0, S.WindSpeed));
			// over snow the nights go much colder
			const double SnowCold = SmoothStep(3.0, 20.0, SnowDepthCm) * (1.0 - Opaque) * (1.0 - SmoothStep(2.0, 7.0, S.WindSpeed));
			const double Daily = Swing * std::cos(2.0 * Pi * (Hod - 15.0) / 24.0) - SnowCold * 4.0 * (1.0 - SunUp);
			// onshore wind (from the sea, west) brings the sea's temperature in
			const double Onshore = SmoothStep(0.3, 0.8, std::cos((S.WindFromDeg - 270.0) * Pi / 180.0));
			const double OpenSea = 1.0 - IceCover;
			double T = Air + Daily;
			T = Lerp(T, SeaTempC, 0.25 * Onshore * OpenSea * SmoothStep(1.0, 8.0, S.WindSpeed));
			S.TempC = float(T);

			// moisture: the air mass's dew point stays put while the day swings the temperature, so nights get damp
			double Td = Air - F.Spread - 0.5 * SmoothNoise1(Hours / 6.0, Seed + 48);
			Td = std::min(Td, T);
			S.DewPointC = float(Td);
			S.Humidity = float(Clamp01(Es(Td) / Es(T)));
			S.FreezingLevelM = float(std::max(0.0, T * 150.0 + (F.WarmNose > 0.5 && T < 0.0 ? 900.0 : 0.0)));
			S.Inversion = float(Clamp01(F.Stable + 0.3 * (1.0 - SunUp) * (1.0 - Opaque)));

			// convection: the sun on the land, cold air over the warm sea (day and night)
			const double SeaHeat = OpenSea * SmoothStep(3.0, 12.0, SeaTempC - T);
			const double SunHeat = SmoothStep(15.0, 50.0, Sun) * (0.35 + 0.65 * X.Summer) * std::sin(Pi * Clamp01((Hod - 8.0) / 11.0));
			S.Convection = float(Clamp01(F.Instab * std::max(SunHeat, 0.8 * SeaHeat) * (1.0 - 0.6 * S.Inversion)));

			// the low layer: stratiform from the episode, cumulus grown by convection (base from the dew-point spread)
			const double CuBase = Clamp(125.0 * (T - Td), 250.0, 2600.0);
			double LowBase = F.LowBase, LowTop = F.LowTop, LowDen = F.LowDen;
			ECloud LowType = F.LowType;
			const double Cu = S.Convection * (0.35 + 0.4 * F.Instab);
			if (Cu > 0.05 && (LowType == ECloud::Cumulus || LowType == ECloud::CumulusCongestus || LowCov < Cu))
			{
				LowType = S.Convection > 0.75 && F.Instab > 0.9 ? ECloud::Cumulonimbus
					: (S.Convection > 0.45 ? ECloud::CumulusCongestus : ECloud::Cumulus);
				LowBase = CuBase;
				LowTop = CuBase + 800.0 + 7000.0 * S.Convection * std::min(F.Instab, 1.2);
				LowDen = 0.4 + 0.5 * S.Convection;
				LowCov = std::max(LowCov, Cu);
			}
			if (LowType == ECloud::Stratus || LowType == ECloud::Stratocumulus || LowType == ECloud::Nimbostratus)
			{
				LowBase = std::min(LowBase, std::max(120.0, 125.0 * (T - Td) + 80.0));
			}
			S.Low = { LowCov > 0.02 ? LowType : ECloud::None, float(LowCov), float(LowBase), float(std::max(LowTop, LowBase + 100.0)), float(LowDen) };
			S.Mid = { MidCov > 0.02 ? F.MidType : ECloud::None, float(MidCov), float(F.MidBase), float(std::max(F.MidTop, F.MidBase + 300.0)), float(F.MidDen) };
			S.High = { HighCov > 0.02 ? F.HighType : ECloud::None, float(HighCov), float(F.HighBase), float(F.HighBase + 1500.0), float(F.HighDen) };
			if (S.High.Type == ECloud::Cirrus && S.High.Cover > 0.3 && SmoothNoise1(Hours / 2.0, Seed + 49) > 0.5)
			{
				S.High.Type = ECloud::Cirrocumulus;
			}
			// the cover you see: layers overlap at random
			S.CloudCover = float(1.0 - (1.0 - LowCov) * (1.0 - MidCov) * (1.0 - 0.8 * HighCov));

			// lenticulars: strong stable flow aloft with a moist middle layer; a quarter of such episodes show them
			const double LensOk = (E.Key & 3u) == 0u ? 1.0 : 0.15;
			S.Lenticular = float(SmoothStep(20.0, 34.0, S.UpperSpeed) * SmoothStep(0.35, 0.75, F.Stable) * LensOk
				* (1.0 - 0.8 * LowCov) * SmoothStep(-0.3, 0.6, SmoothNoise1(Hours / 4.0, Seed + 50)));
			if (S.Lenticular > 0.25 && S.Mid.Cover < 0.6)
			{
				S.Mid.Type = ECloud::AltocumulusLenticular;
				S.Mid.Cover = std::max(S.Mid.Cover, float(0.25 * S.Lenticular));
				S.Mid.BaseM = 3000.f;
				S.Mid.TopM = 3600.f;
				S.Mid.Density = 0.5f;
			}

			// precipitation: the episode's steady fall and the shower cells passing over (heavier when warm)
			const double Cells = Clamp01(F.Cells * (0.4 + 0.9 * S.Convection + 0.3 * SeaHeat));
			S.ShowerCells = float(Cells);
			double Conv = 0.0;
			if (Cells > 0.0)
			{
				const double Cell = 0.5 + 0.5 * (0.7 * SmoothNoise1(Hours / 0.55, Seed + 51) + 0.3 * SmoothNoise1(Hours / 0.17, Seed + 52));
				const double Over = (Cell - (1.0 - Cells)) / std::max(Cells, 0.05);
				if (Over > 0.0)
				{
					Conv = F.CellRate * std::pow(Clamp01(Over), 1.5) * (0.5 + 0.7 * std::max(S.Convection, float(SeaHeat)));
				}
			}
			double Strat = F.Strat * (0.75 + 0.45 * (0.5 + 0.5 * SmoothNoise1(Hours / 1.3, Seed + 53)));
			// drizzle out of a low damp deck
			if (F.Drizzle > 0.0 && LowCov > 0.85)
			{
				const double D = 0.5 + 0.5 * SmoothNoise1(Hours / 2.5, Seed + 54);
				Strat += F.Drizzle * 0.35 * SmoothStep(0.55, 0.9, D) * SmoothStep(0.9, 0.98, S.Humidity + 0.05);
			}
			const double Rate = (Strat + Conv) * C.Wetness;
			S.PrecipRate = float(Rate);

			// its kind: snow and sleet by the wet-bulb, freezing rain under a warm nose, hail and graupel in showers
			const double Tw = T - (T - Td) / 3.0;
			double SnowFrac = SmoothStep(2.0, -0.2, Tw);
			const bool bFreezing = T < 0.0 && F.WarmNose > 0.5 && T > -6.0 && E.Regime != ERegime::ArcticHigh;
			if (bFreezing) SnowFrac = SmoothStep(-3.0, -6.0, T);
			S.SnowFraction = float(SnowFrac);
			S.Thunder = float(Clamp01(SmoothStep(0.5, 3.0, Conv) * SmoothStep(9.0, 16.0, T) * (0.8 + F.ThunderBoost)
				+ F.ThunderBoost * SmoothStep(1.0, 5.0, Conv) * SmoothStep(14.0, 20.0, T)));
			S.Hail = float(Clamp01(SmoothStep(6.0, 18.0, Conv) * SmoothStep(14.0, 22.0, T) + 0.6 * SmoothStep(1.0, 4.0, Conv) * SmoothStep(5.0, 1.0, T) * SmoothStep(-1.0, 1.0, T)));
			const double Ratio = 1.0 + 1.0 * SmoothStep(0.0, -10.0, T);   // cm of snow per mm of water
			S.SnowfallCmPerHour = float(Rate * SnowFrac * Ratio);
			if (Rate < 0.03)
			{
				S.Precip = EPrecip::None;
			}
			else
			{
				const bool bConv = Conv > Strat;
				if (bFreezing && SnowFrac < 0.5) S.Precip = Rate < 0.4 ? EPrecip::FreezingDrizzle : EPrecip::FreezingRain;
				else if (SnowFrac > 0.75) S.Precip = bConv ? EPrecip::SnowShowers : EPrecip::Snow;
				else if (SnowFrac > 0.25) S.Precip = EPrecip::Sleet;
				else if (S.Hail > 0.35) S.Precip = T < 6.0 ? EPrecip::Graupel : EPrecip::Hail;
				else if (S.Thunder > 0.3) S.Precip = EPrecip::Thunderstorm;
				else if (bConv) S.Precip = EPrecip::Showers;
				else if (Rate < 0.5 && F.Drizzle > 0.2) S.Precip = EPrecip::Drizzle;
				else S.Precip = EPrecip::Rain;
			}

			// fog by its cause
			const double Calm = SmoothStep(4.5, 1.5, S.WindSpeed);
			const double Spread = T - Td;
			const double Night = 1.0 - SmoothStep(-1.0, 12.0, Sun);
			const double Radiation = SmoothStep(1.8, 0.2, Spread) * Calm * (1.0 - 0.8 * Opaque) * Night;
			const double Advection = SmoothStep(0.5, 3.0, Td - SeaTempC) * Onshore * OpenSea * SmoothStep(1.0, 3.0, S.WindSpeed)
				* SmoothStep(11.0, 6.0, S.WindSpeed) * SmoothStep(0.85, 0.97, S.Humidity);
			const double Steam = OpenSea * SmoothStep(8.0, 16.0, SeaTempC - T) * SmoothStep(12.0, 4.0, S.WindSpeed);
			const double Frontal = SmoothStep(0.8, 3.0, Strat) * SmoothStep(0.93, 1.0, S.Humidity) * 0.45;
			double Fog = Radiation;
			S.FogType = Radiation > 0.05 ? EFog::Radiation : EFog::None;
			S.FogTopM = float(5.0 + 120.0 * Radiation);
			if (Advection > Fog) { Fog = Advection; S.FogType = EFog::Advection; S.FogTopM = float(150.0 + 250.0 * Advection); }
			if (Steam * 0.7 > Fog) { Fog = Steam * 0.7; S.FogType = EFog::Steam; S.FogTopM = float(8.0 + 40.0 * Steam); }
			if (Frontal > Fog) { Fog = Frontal; S.FogType = EFog::Frontal; S.FogTopM = float(LowBase); }
			S.Fog = float(Clamp01(Fog));
			if (S.Fog < 0.03) S.FogType = EFog::None;
			// visibility: the extinctions add up (fog 1 -> 80 m; 1 mm/h of rain ~ 12 km, of snow ~ 1.5 km)
			const double KFog = 3.912 / (30000.0 * std::pow(10.0, -2.57 * S.Fog));
			const double KRain = Rate > 0.0 ? 3.912 / (12000.0 / std::pow(std::max(Rate * (1.0 - SnowFrac), 0.01), 0.6)) : 0.0;
			const double KSnow = Rate > 0.0 && SnowFrac > 0.0 ? 3.912 / (1500.0 / std::pow(std::max(Rate * SnowFrac, 0.01), 0.8)) : 0.0;
			S.VisibilityM = float(Clamp(3.912 / (KFog + KRain + KSnow), 30.0, 50000.0));

			// frost on surfaces: clear calm frozen nights near saturation, rime in freezing fog
			S.Frost = float(Clamp01(SmoothStep(0.5, -3.0, T) * (SmoothStep(2.5, 0.5, Spread) * Calm * (1.0 - Opaque) * Night + S.Fog * SmoothStep(0.0, -2.0, T))));

			// the sky beyond the weather
			S.Kp = float(KpAt(Hours, C));
			S.Aurora = float(SmoothStep(5.3, 8.5, S.Kp) * (0.65 + 0.35 * (0.5 + 0.5 * SmoothNoise1(Hours / 0.4, Seed + 55))));
			S.Halo = float(Clamp01((S.High.Type == ECloud::Cirrostratus ? 0.8f * S.High.Cover : 0.f) * (1.0 - MidCov * F.MidDen * 1.5) * (1.0 - LowCov)));
			S.Rainbow = float(Clamp01(Cells * 5.0 * SmoothStep(0.05, 0.6, Rate + Cells) * (1.0 - LowCov * 0.7) * SmoothStep(1.0, 6.0, Sun) * SmoothStep(44.0, 30.0, Sun)));
			{
				const bool bNlcSeason = X.Doy > 145.0 && X.Doy < 225.0;
				const double Twilight = SmoothStep(-4.0, -7.0, Sun) * SmoothStep(-17.0, -13.0, Sun);
				const uint32_t NightKey = uint32_t(int64_t(std::floor((Hours - 12.0) / 24.0)));
				const double Show = Hash01(NightKey, 31, Seed);
				S.Noctilucent = float(bNlcSeason ? Twilight * SmoothStep(0.65, 0.9, Show) * std::sin(Pi * (X.Doy - 145.0) / 80.0) : 0.0);
			}
			{
				const double Doy = X.Doy;
				const double LateSummer = SmoothStep(180.0, 210.0, Doy) * SmoothStep(290.0, 255.0, Doy);
				S.Waterspout = float(LateSummer * OpenSea * SmoothStep(3.0, 8.0, SeaTempC - T) * Cells * 2.0 * SmoothStep(0.3, 0.8, S.Convection));
			}
			return S;
		}
	}

	FWeatherState Instant(double Hours, FChain& Chain)
	{
		const FClimate& C = Chain.GetClimate();
		return InstantWith(Hours, Chain, ClimateSeaC(DayOfYearOf(Hours), C), 0.0, 0.0);
	}

	void SynopticWind(double Hours, FChain& Chain, float& OutFromDeg, float& OutSpeed)
	{
		Hours += GetHourShift();
		const FProf F = Blended(Hours, Chain);
		const uint32_t Seed = Chain.GetClimate().Seed;
		double FromDeg = std::atan2(F.FromX, F.FromY) * 180.0 / Pi + 14.0 * SmoothNoise1(Hours / 7.0, Seed + 41);
		if (FromDeg < 0.0) FromDeg += 360.0;
		OutFromDeg = float(std::fmod(FromDeg, 360.0));
		OutSpeed = float(Clamp(F.Speed * (1.0 + 0.15 * SmoothNoise1(Hours / 5.0, Seed + 42)), 0.3, 24.0));
	}

	// ------------------------------------------------------------------------------------------------ the season

	namespace
	{
		/** The 1 September at or before Hours (local hours since 2000). */
		double SeasonStartOf(double Hours)
		{
			const int Year = 2000 + int(std::floor(Hours / (24.0 * 365.2425)));
			for (int Y = Year + 1; Y >= Year - 1; Y--)
			{
				const double S = HoursSince2000(Y, 9, 1, 0.0);
				if (S <= Hours) return S;
			}
			return HoursSince2000(Year - 1, 9, 1, 0.0);
		}
	}

	void FSeason::Reset(double Hours)
	{
		SeasonStart = SeasonStartOf(Hours);
		Now = SeasonStart;
		SnowWater = SnowDepth = 0.0;
		SnowAge = 30.0;
		LastMelt = 0.0;
		GroundFrost = Fdd = SeaIce = IceCover = Glaze = Wet = 0.0;
		SeaTemp = 0.0;   // set from the climate on the first step
	}

	void FSeason::Step(double Hours, double Dt, FChain& Chain)
	{
		const FClimate& C = Chain.GetClimate();
		const double Doy = DayOfYearOf(Hours);
		if (SeaTemp == 0.0 && Hours - SeasonStart < Dt + 1e-6)
		{
			SeaTemp = ClimateSeaC(Doy, C);
		}
		const FWeatherState S = InstantWith(Hours, Chain, SeaTemp, IceCover, SnowDepth);
		const double T = S.TempC;
		const double Rate = S.PrecipRate;
		const double SnowW = Rate * S.SnowFraction * Dt;
		const double RainW = Rate * (1.0 - S.SnowFraction) * Dt;

		// snow pack: fresh snow light and fluffy, settling over weeks, melted by warm air, rain and the spring sun
		if (SnowW > 0.0)
		{
			const double Fresh = Clamp(60.0 + 6.0 * T, 50.0, 160.0);
			const double OldD = SnowDepth;
			SnowWater += SnowW;
			SnowDepth += SnowW / Fresh * 100.0;
			if (SnowW / Dt > 0.1) SnowAge = 0.0;
			(void)OldD;
		}
		double Melt = 0.0;
		if (SnowWater > 0.0)
		{
			const double Sun = SunElevation(Hours, C);
			const double Rad = SmoothStep(5.0, 45.0, Sun) * (1.0 - 0.8 * S.CloudCover) * 0.35 * SmoothStep(-4.0, 0.0, T);
			Melt = std::max(0.0, 3.5 / 24.0 * T) + 0.0125 * RainW / std::max(Dt, 1e-3) * std::max(T, 0.0) + Rad;
			Melt = std::min(SnowWater, Melt * Dt);
			const double Rho = SnowDepth > 0.01 ? SnowWater / (SnowDepth / 100.0) : 100.0;
			SnowWater -= Melt;
			// settling: towards 350 kg/m3 over ~3 weeks, quicker when warm and wet
			const double Settle = Dt / (24.0 * (T > -1.0 ? 6.0 : 20.0));
			const double NewRho = std::min(Rho + (380.0 - Rho) * Settle + (Melt > 0.0 ? 4.0 * Dt : 0.0), 450.0);
			SnowDepth = SnowWater > 0.01 ? SnowWater / NewRho * 100.0 : 0.0;
			if (SnowWater <= 0.01) SnowWater = 0.0;
		}
		SnowAge += Dt / 24.0;
		LastMelt = Melt / std::max(Dt, 1e-3);

		// freezing degree-days of this winter (net: a thaw takes some back)
		Fdd = std::max(0.0, Fdd + (T < 0.0 ? -T : -0.5 * T) * Dt / 24.0);
		// frozen soil: grows under bare frozen ground, insulated by snow, thaws from the top
		const double Insul = 1.0 / (1.0 + SnowDepth / 8.0);
		GroundFrost = Clamp(GroundFrost + (T < 0.0 ? 0.06 * -T * Insul / (1.0 + GroundFrost / 15.0) : -0.08 * T) * Dt, 0.0, 120.0);

		// the sea: pulled to its climate over two months, cooled or warmed by the air (faster in wind), frozen at -0.3
		const double Wind = S.WindSpeed;
		if (IceCover < 0.98)
		{
			const double Clim = ClimateSeaC(Doy, C);
			SeaTemp += (Clim - SeaTemp) * Dt / (24.0 * 60.0) + (T - SeaTemp) * Dt / (24.0 * 20.0) * (0.4 + Wind / 8.0);
			SeaTemp = std::max(SeaTemp, -0.3);
		}
		// sea ice: once the water is at freezing, it thickens as the square root of the frost (Stefan), melts in thaw,
		// and wind breaks young ice up
		if (SeaTemp <= -0.25 && T < -1.0)
		{
			SeaIce += 0.045 * (-T - 1.0) / (SeaIce + 2.0) * Dt * 10.0 / 10.0;
		}
		else if (T > 0.0 || SeaTemp > 0.0)
		{
			SeaIce -= (0.04 * std::max(T, 0.0) + 0.02 * std::max(SeaTemp, 0.0)) * Dt;
		}
		if (Wind > 11.0 && SeaIce < 6.0) SeaIce -= 0.05 * (Wind - 11.0) * Dt;
		SeaIce = std::max(SeaIce, 0.0);
		IceCover = SmoothStep(0.5, 6.0, SeaIce);
		if (IceCover > 0.02)
		{
			SeaTemp = std::min(SeaTemp, -0.3 + 1.5 * (1.0 - IceCover));
		}

		// glaze from freezing rain, gone in a thaw or a sunny day
		if (S.Precip == EPrecip::FreezingRain || S.Precip == EPrecip::FreezingDrizzle) Glaze += RainW * 0.6;
		Glaze = Clamp(Glaze - (std::max(T, 0.0) * 0.15 + 0.01) * Dt, 0.0, 1.0);

		// the ground's water: rain and melt in, out to evaporation and soaking (dries in hours on a warm windy day)
		Wet += RainW + Melt * 0.5;
		const double Evap = (0.04 + 0.025 * std::max(T, 0.0) + 0.015 * Wind) * (1.0 - S.Humidity + 0.1) + 0.03;
		Wet = Clamp(Wet - Evap * Dt * (T < 0.0 ? 0.2 : 1.0), 0.0, 6.0);

		Now = Hours + Dt;
	}

	void FSeason::AdvanceTo(double Hours, FChain& Chain)
	{
		const double Start = SeasonStartOf(Hours);
		if (Hours < Now || Start != SeasonStart || Now < -1e11)
		{
			Reset(Hours);
		}
		// hourly steps (the last one partial is left to Apply)
		while (Now + 1.0 <= Hours)
		{
			Step(Now, 1.0, Chain);
		}
	}

	void FSeason::Apply(FWeatherState& S, FChain& Chain) const
	{
		(void)Chain;
		const double Part = Clamp(S.Hours - Now, 0.0, 1.0);
		const double RainNow = S.PrecipRate * (1.0 - S.SnowFraction);
		S.Wet = float(Clamp01((Wet + RainNow * Part) / 2.5));
		S.SnowWaterMm = float(SnowWater);
		S.SnowDepthCm = float(SnowDepth + S.SnowfallCmPerHour * Part);
		S.SnowDensity = float(SnowDepth > 0.1 ? SnowWater / (SnowDepth / 100.0) : 100.0);
		S.SnowAgeDays = float(S.SnowfallCmPerHour > 0.1f ? 0.0 : SnowAge);
		S.Melt = float(Clamp01(LastMelt / 0.5));
		S.Glaze = float(Glaze);
		S.GroundFrostCm = float(GroundFrost);
		S.SeaTempC = float(SeaTemp);
		S.FreezingDegreeDays = float(Fdd);
		S.SeaIceCm = float(SeaIce);
		S.SeaIceCover = float(IceCover);
		if (S.SnowDepthCm > 1.f && S.TempC < 0.5f)
		{
			S.Wet = std::min(S.Wet, 0.2f);
		}
	}

	FWeatherState Full(double Hours, FChain& Chain, FSeason& Season)
	{
		Season.AdvanceTo(Hours, Chain);
		FWeatherState Probe;
		Probe.Hours = Hours;
		Season.Apply(Probe, Chain);
		FWeatherState S = InstantWith(Hours + GetHourShift(), Chain, Probe.SeaTempC, Probe.SeaIceCover, Probe.SnowDepthCm);
		S.Hours = Hours;
		Season.Apply(S, Chain);
		return S;
	}
}
