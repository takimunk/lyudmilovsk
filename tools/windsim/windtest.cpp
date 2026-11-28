// Checks and looks for the local wind (Private/Weather/WindField.cpp) outside the engine: the shifts, the land's lee,
// crests and lift on the real map's relief (Saved/Wind/terrain.bin from terrain.py) and on a small built-in coast,
// the look-ahead, the words and numbers. See run.sh.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "WindField.h"

using namespace LyudWeather;

static int Passed = 0, Failures = 0;
static void Check(bool Ok, const char* What, double Value)
{
	std::printf("%s %-78s %.4g\n", Ok ? "ok  " : "FAIL", What, Value);
	Ok ? Passed++ : Failures++;
}

static const double Pi = 3.14159265358979323846;

/** A small deterministic generator for the checks' random points. */
struct FRand
{
	uint32_t S = 12345;
	explicit FRand(uint32_t Seed) : S(Seed) {}
	double Next()
	{
		S ^= S << 13;
		S ^= S >> 17;
		S ^= S << 5;
		return double(S >> 8) / 16777216.0;
	}
	double In(double A, double B) { return A + (B - A) * Next(); }
};

static FWindParams GameParams()
{
	FWindParams P;
	P.Seed = 921;   // ULyudWindSubsystem's
	return P;
}

/** A wind of the area made by hand: from a bearing at a speed, gusty as a day (0..1) makes it. */
static FWindState Wind(float FromDeg, float Speed, float Day = 0.5f)
{
	FWindState S;
	S.FromDeg = FromDeg;
	S.Speed = Speed;
	TowardOf(FromDeg, S.DirX, S.DirY);
	S.Day = Day;
	S.Gustiness = 0.14f + 0.18f * Day;
	S.Weather = Speed;
	return S;
}

// ---------------------------------------------------------------------------------------------------- the terrains

/** The real map (terrain.py). MapY: world Y = map Y + MapY. */
static bool LoadTerrain(const char* Path, FWindTerrain& T, double& MapY)
{
	FILE* F = std::fopen(Path, "rb");
	if (!F)
	{
		return false;
	}
	char Magic[4];
	int32_t N[2];
	double D[4];
	float Sea;
	bool bOk = std::fread(Magic, 1, 4, F) == 4 && std::memcmp(Magic, "LWT1", 4) == 0 && std::fread(N, 4, 2, F) == 2 &&
		std::fread(D, 8, 4, F) == 4 && std::fread(&Sea, 4, 1, F) == 1 && N[0] > 1 && N[1] > 1 && N[0] < 4096 && N[1] < 4096;
	if (bOk)
	{
		T.NX = N[0];
		T.NY = N[1];
		T.X0 = D[0];
		T.Y0 = D[1];
		T.Cell = D[2];
		MapY = D[3];
		T.SeaZ = Sea;
		const size_t Count = size_t(T.NX) * size_t(T.NY);
		T.Ground.resize(Count);
		T.Cover.resize(Count);
		bOk = std::fread(T.Ground.data(), 4, Count, F) == Count && std::fread(T.Cover.data(), 4, Count, F) == Count;
	}
	std::fclose(F);
	return bOk && T.Valid();
}

// The built-in coast, a cut-down copy of the map's (tools/world_map.py): the sea toward +X, the water line at X 135,
// 25 m of beach, a 46 m wide cliff up to an upland at +18 m. South of Y 0 the upland is bare; north of it an old wood
// (24 m crowns) starts 60 m back from the brow. One 30 m block of a house stands on the bare upland.
namespace Synth
{
	const double ShoreX = 135.0, BeachW = 25.0, CliffW = 46.0, TopZ = 18.0, SeaZ = -43.0;
	const double FootX = ShoreX - BeachW, BrowX = FootX - CliffW;
	const double WoodX = BrowX - 60.0;
	const double HouseX = -300.0, HouseY = -400.0, HouseHalf = 24.0, HouseH = 30.0;

	static double Height(double X)
	{
		const double In = ShoreX - X;
		if (In <= 0.0)
		{
			return SeaZ - 0.3 + std::max(In / 80.0, -14.0);
		}
		if (In <= BeachW)
		{
			return SeaZ - 0.3 + In / BeachW * 1.8;
		}
		if (In >= BeachW + CliffW)
		{
			return TopZ;
		}
		const double T = (In - BeachW) / CliffW;
		return SeaZ + 1.5 + (TopZ - SeaZ - 1.5) * (0.5 * T + 0.5 * T * T * (3.0 - 2.0 * T));
	}

	static void Build(FWindTerrain& T)
	{
		T.Cell = 16.0;
		// (wide enough that the grid's own landward edges are further than the march looks from where the checks stand)
		T.X0 = -3696.0;
		T.Y0 = -3696.0;
		T.NX = 257;     // to X +400
		T.NY = 463;     // to Y +3696
		T.SeaZ = float(SeaZ);
		T.Ground.resize(size_t(T.NX) * T.NY);
		T.Cover.assign(size_t(T.NX) * T.NY, 0.f);
		for (int J = 0; J < T.NY; J++)
		{
			for (int I = 0; I < T.NX; I++)
			{
				const double X = T.X0 + I * T.Cell, Y = T.Y0 + J * T.Cell;
				T.Ground[size_t(J) * T.NX + I] = float(Height(X));
				float C = 0.f;
				if (Y > 0.0 && X < WoodX)
				{
					C = 24.f;
				}
				if (std::fabs(X - HouseX) <= HouseHalf && std::fabs(Y - HouseY) <= HouseHalf)
				{
					C = float(HouseH);
				}
				T.Cover[size_t(J) * T.NX + I] = C;
			}
		}
	}
}

static const float Datum = -43.f;   // the sea's level in the checks: the datum (high water of springs)

static FLocalWind At(const FWindState& S, const FWindTerrain* T, double X, double Y, double Above)
{
	// Above = metres over the ground or the water there
	const double Surface = T && T->Valid() ? std::max(T->GroundAt(X, Y), Datum) : 0.f;
	return LocalWindAt(S, T, X, Y, Surface + Above, Datum, 0.0, 0.0, 0.0);
}

/** The same without gusts (so that a check compares the land's doing, not two patches of the gust field). */
static FLocalWind Steady(FWindState S, const FWindTerrain* T, double X, double Y, double Above)
{
	const float Gustiness = S.Gustiness;
	S.Gustiness = 0.f;
	FLocalWind W = At(S, T, X, Y, Above);
	S.Gustiness = Gustiness;
	// the turbulence does read the gustiness: take it from the gusty call
	W.Turbulence = At(S, T, X, Y, Above).Turbulence;
	return W;
}

static void PrintWind(const char* Name, const FLocalWind& W)
{
	std::printf("       %-34s speed %5.2f from %5.1f  exposure %4.2f shelter %4.2f  up %+5.2f  turb %4.2f  h %5.1f  fetch %6.0f %s\n",
		Name, W.Speed, W.FromDeg, W.Exposure, W.Shelter, W.Updraft, W.Turbulence, W.HeightM, W.FetchM, W.bOverWater ? "water" : "land");
}

/** Where the coast is along a row of the grid: the water line and the cliff's brow and foot (world X; the sea is toward +X). */
struct FCoast
{
	double ShoreX = 0.0, FootX = 0.0, BrowX = 0.0, TopZ = 0.0;
	bool bCliff = false;
};

static FCoast FindCoast(const FWindTerrain& T, double Y)
{
	FCoast C;
	const double XMax = T.X0 + (T.NX - 1) * T.Cell;
	double X = XMax;
	while (X > T.X0 && T.GroundAt(X, Y) < Datum)
	{
		X -= 1.0;
	}
	C.ShoreX = X;
	// the foot: where the ground starts to climb steeper than 1:4; the brow: where it stops
	while (X > T.X0 + 10.0 && T.GroundAt(X - 4.0, Y) - T.GroundAt(X, Y) < 1.0)
	{
		X -= 1.0;
		if (C.ShoreX - X > 120.0)
		{
			return C;
		}
	}
	C.FootX = X;
	while (X > T.X0 + 10.0 && T.GroundAt(X - 4.0, Y) - T.GroundAt(X, Y) > 0.4)
	{
		X -= 1.0;
	}
	C.BrowX = X;
	C.TopZ = T.GroundAt(X, Y);
	C.bCliff = C.TopZ - Datum > 25.0;
	return C;
}

// ---------------------------------------------------------------------------------------------------- the checks

static void TestShifts()
{
	const FWindParams P = GameParams();
	const double Start = HoursSince2000(2026, 7, 1, 0.0);

	// a month, every half minute: the swing's mean, its bounds, the breathing's mean
	double SumVeer = 0.0, SumFactor = 0.0, MaxOver = 0.0, MinFactor = 9.0, MaxFactor = 0.0, MinRange = 99.0, MaxRange = 0.0;
	double MinPeriod = 99.0, MaxPeriod = 0.0, WorstHour = 0.0, HourSum = 0.0, MaxFresh = 0.0, MinLight = 99.0, PeakShare = 0.0;
	int N = 0, HourN = 0, Hours = 0;
	// measured swings against the PeriodMin the model reports, in three bands of it
	double BandTime[3] = { 0, 0, 0 }, BandReported[3] = { 0, 0, 0 };
	int BandTurns[3] = { 0, 0, 0 };
	float PrevVeer = 0.f;
	for (double H = 0.0; H < 30.0 * 24.0; H += 1.0 / 120.0)
	{
		const FWindState R = Regime(Start + H, P);
		const FWindShift Sh = ShiftAt(Start + H, R, P.Seed);
		SumVeer += Sh.VeerDeg;
		SumFactor += Sh.Factor;
		MaxOver = std::max(MaxOver, double(std::fabs(Sh.VeerDeg) - Sh.RangeDeg));
		PeakShare = std::max(PeakShare, double(std::fabs(Sh.VeerDeg) / Sh.RangeDeg));
		MinFactor = std::min(MinFactor, double(Sh.Factor));
		MaxFactor = std::max(MaxFactor, double(Sh.Factor));
		MinRange = std::min(MinRange, double(Sh.RangeDeg));
		MaxRange = std::max(MaxRange, double(Sh.RangeDeg));
		MinPeriod = std::min(MinPeriod, double(Sh.PeriodMin));
		MaxPeriod = std::max(MaxPeriod, double(Sh.PeriodMin));
		if (R.Speed > 10.f && R.Day < 0.3f)
		{
			MaxFresh = std::max(MaxFresh, double(Sh.RangeDeg));
		}
		if (R.Speed < 3.f && R.Day > 0.8f)
		{
			MinLight = std::min(MinLight, double(Sh.RangeDeg));
		}
		const int Band = Sh.PeriodMin > 9.f ? 0 : (Sh.PeriodMin > 6.5f ? 1 : 2);
		BandTime[Band] += 0.5;
		BandReported[Band] += Sh.PeriodMin * 0.5;
		if (N > 0 && (Sh.VeerDeg >= 0.f) != (PrevVeer >= 0.f))
		{
			BandTurns[Band]++;
		}
		PrevVeer = Sh.VeerDeg;
		HourSum += Sh.VeerDeg / Sh.RangeDeg;
		HourN++;
		if (HourN == 120)
		{
			WorstHour += std::fabs(HourSum / HourN);
			Hours++;
			HourSum = 0.0;
			HourN = 0;
		}
		N++;
	}
	Check(std::fabs(SumVeer / N) < 0.15, "shift: a month's mean veer is nothing, degrees", SumVeer / N);
	Check(std::fabs(SumFactor / N - 1.0) < 0.003, "shift: a month's mean factor is 1", SumFactor / N);
	Check(WorstHour / Hours < 0.12, "shift: an hour's mean veer, share of the range (mean over the month)", WorstHour / Hours);
	Check(MaxOver <= 1e-4 && PeakShare > 0.97, "shift: the veer reaches its range and never passes it (peak share)", PeakShare);
	Check(MinRange >= 2.0 && MaxRange <= 18.0, "shift: the range stays in 2..18 degrees (the month's widest)", MaxRange);
	Check(MinFactor > 0.78 && MaxFactor < 1.25, "shift: the bay's breathing stays in 0.78..1.25 (lowest)", MinFactor);
	std::printf("       range %.1f..%.1f deg, factor %.3f..%.3f, period %.1f..%.1f min\n", MinRange, MaxRange, MinFactor, MaxFactor, MinPeriod, MaxPeriod);
	Check(MinPeriod >= 4.0 && MaxPeriod <= 12.0, "shift: PeriodMin stays in 4..12 minutes (shortest)", MinPeriod);
	for (int B = 0; B < 3; B++)
	{
		if (BandTurns[B] < 50)
		{
			continue;
		}
		const double Measured = 2.0 * BandTime[B] / BandTurns[B], Reported = BandReported[B] / BandTime[B];
		char Name[128];
		std::snprintf(Name, sizeof(Name), "shift: measured swing %.1f min against PeriodMin %.1f (%s air), ratio", Measured, Reported,
			B == 0 ? "settled" : (B == 1 ? "mixed" : "stirred"));
		Check(Measured > 4.0 && Measured < 12.0 && std::fabs(Measured / Reported - 1.0) < 0.2, Name, Measured / Reported);
	}

	// the stated ends of the range, on regimes made by hand
	{
		FWindState Fresh = Wind(250.f, 12.f, 0.f), Light = Wind(250.f, 2.f, 1.f), Middling = Wind(250.f, 6.f, 0.6f);
		const float A = ShiftAt(Start, Fresh, 921).RangeDeg, B = ShiftAt(Start, Light, 921).RangeDeg, C = ShiftAt(Start, Middling, 921).RangeDeg;
		Check(A >= 2.f && A <= 5.f, "shift: a steady fresh night wind swings 2..5 degrees", A);
		Check(B >= 15.f && B <= 18.f, "shift: a light stirred sunny afternoon swings 15..18 degrees", B);
		Check(C > A && C < B, "shift: a moderate day lies between", C);
		Check(MaxFresh <= 5.5 || MaxFresh == 0.0, "shift: the month's fresh nights (over 10 m/s) stay under 5.5 degrees", MaxFresh);
		Check(MinLight >= 13.0 || MinLight == 99.0, "shift: the month's light afternoons (under 3 m/s) swing over 13 degrees", MinLight);
		Check(ShiftAt(Start, Light, 921).PeriodMin < ShiftAt(Start, Fresh, 921).PeriodMin, "shift: stirred air swings quicker than settled, min",
			ShiftAt(Start, Light, 921).PeriodMin);
	}

	// smooth in time: a second of the game's clock at a time over three days
	{
		double MaxStep = 0.0, MaxFactorStep = 0.0, MaxBend = 0.0, MaxNow = 0.0;
		const double Sec = 1.0 / 3600.0;
		const double From = HoursSince2000(2026, 7, 10, 0.0);
		float V0 = 0.f, V1 = 0.f, F1 = 1.f, Now1 = 0.f;
		for (int I = 0; I < 3 * 86400; I++)
		{
			const double H = From + I * Sec;
			const FWindState R = Regime(H, P);
			FWindShift Sh;
			Sh = ShiftAt(H, R, P.Seed);
			const float NowFrom = Shifted(R, Sh).FromDeg;
			if (I > 1)
			{
				MaxStep = std::max(MaxStep, double(std::fabs(Sh.VeerDeg - V1)));
				MaxFactorStep = std::max(MaxFactorStep, double(std::fabs(Sh.Factor - F1)));
				MaxBend = std::max(MaxBend, double(std::fabs(Sh.VeerDeg - 2.f * V1 + V0)));
				MaxNow = std::max(MaxNow, std::fabs(double(AngleOff(Now1, NowFrom))));
			}
			V0 = V1;
			V1 = Sh.VeerDeg;
			F1 = Sh.Factor;
			Now1 = NowFrom;
		}
		Check(MaxStep < 0.8, "shift: the veer never moves more than 0.8 degrees in a second", MaxStep);
		Check(MaxBend < 0.02, "shift: nor kinks (second difference per second^2, degrees)", MaxBend);
		Check(MaxFactorStep < 0.004, "shift: the breathing never moves more than 0.4% in a second", MaxFactorStep);
		Check(MaxNow < 0.8, "shift: Now()'s bearing never jumps (degrees in a second)", MaxNow);
	}

	// NextTurnMin against a brute-force scan, with the regime held and with the regime running on
	{
		FRand Rnd(7);
		int Good = 0, GoodLive = 0, Count = 0, PhaseOk = 0;
		double Worst = 0.0, MeanNext = 0.0;
		for (int I = 0; I < 1500; I++)
		{
			const double H = Start + Rnd.In(0.0, 30.0 * 24.0);
			const FWindState R = Regime(H, P);
			const FWindShift Sh = ShiftAt(H, R, P.Seed);
			const bool bVeered = Sh.VeerDeg >= 0.f;
			double Brute = -1.0, Live = -1.0;
			for (double M = 0.005; M < 31.0 && (Brute < 0.0 || Live < 0.0); M += 0.005)
			{
				if (Brute < 0.0 && (ShiftAt(H + M / 60.0, R, P.Seed).VeerDeg >= 0.f) != bVeered)
				{
					Brute = M;
				}
				if (Live < 0.0 && (ShiftAt(H + M / 60.0, Regime(H + M / 60.0, P), P.Seed).VeerDeg >= 0.f) != bVeered)
				{
					Live = M;
				}
			}
			if (Brute < 0.0)
			{
				Brute = 30.0;
			}
			if (Live < 0.0)
			{
				Live = 30.0;
			}
			Count++;
			Good += std::fabs(Brute - Sh.NextTurnMin) < 0.02 ? 1 : 0;
			GoodLive += std::fabs(Live - Sh.NextTurnMin) < 0.25 ? 1 : 0;
			Worst = std::max(Worst, std::fabs(Brute - Sh.NextTurnMin));
			MeanNext += Sh.NextTurnMin;
			PhaseOk += Sh.Phase >= 0.f && Sh.Phase <= 1.f && (Sh.Phase < 0.5f) == bVeered ? 1 : 0;
		}
		Check(Good >= Count * 0.995, "shift: NextTurnMin is the brute-force scan's (regime held), share within 1 s", double(Good) / Count);
		Check(GoodLive >= Count * 0.97, "shift: and the turn really comes then as the regime runs on, share within 15 s", double(GoodLive) / Count);
		Check(PhaseOk == Count, "shift: Phase is 0..1, its first half the veered one", double(PhaseOk) / Count);
		std::printf("       mean time to the next turn %.2f min, worst miss %.3f min\n", MeanNext / Count, Worst);
		// the phase runs forward through a swing
		const double H = Start + 100.0;
		const FWindState R = Regime(H, P);
		const FWindShift A = ShiftAt(H, R, P.Seed);
		const FWindShift B = ShiftAt(H + A.NextTurnMin * 0.5 / 60.0, R, P.Seed);
		Check((A.Phase < 0.5f) == (B.Phase < 0.5f) && B.Phase > A.Phase && std::fabs(B.NextTurnMin - A.NextTurnMin * 0.5f) < 0.01f,
			"shift: half way to the turn the phase is on and the turn half as far", B.Phase - A.Phase);
	}

	// Shifted: bearing and vector agree, the rest is kept
	{
		const FWindState R = Regime(Start + 13.0, P);
		FWindShift Sh;
		Sh.VeerDeg = 12.f;
		Sh.Factor = 1.1f;
		const FWindState S = Shifted(R, Sh);
		Check(std::fabs(AngleOff(R.FromDeg, S.FromDeg) - 12.f) < 1e-3f && std::fabs(AngleOff(S.FromDeg, BearingFrom(S.DirX, S.DirY))) < 0.01f &&
			std::fabs(S.Speed - R.Speed * 1.1f) < 1e-5f && S.Gustiness == R.Gustiness && S.Day == R.Day && S.Hours == R.Hours,
			"Shifted: +12 degrees veers the bearing and the vector alike, 1.1 on the speed", AngleOff(R.FromDeg, S.FromDeg));
		const FWindState N1 = Now(Start + 13.0, P), N2 = Shifted(R, ShiftAt(Start + 13.0, R, P.Seed));
		Check(N1.FromDeg == N2.FromDeg && N1.Speed == N2.Speed && N1.DirX == N2.DirX, "Now() is Regime() with ShiftAt() applied", N1.FromDeg);
	}

	// the year's statistics stand
	{
		const double Y0 = HoursSince2000(2026, 1, 1, 0.0);
		double SumR = 0.0, SumN = 0.0, RX = 0.0, RY = 0.0, NX = 0.0, NY = 0.0;
		std::vector<float> SpeedsR, SpeedsN;
		int Count = 0;
		for (double H = 0.0; H < 365.0 * 24.0; H += 1.0 / 6.0)
		{
			const FWindState R = Regime(Y0 + H, P), S = Now(Y0 + H, P);
			SumR += R.Speed;
			SumN += S.Speed;
			RX += R.DirX * R.Speed;
			RY += R.DirY * R.Speed;
			NX += S.DirX * S.Speed;
			NY += S.DirY * S.Speed;
			SpeedsR.push_back(R.Speed);
			SpeedsN.push_back(S.Speed);
			Count++;
		}
		std::sort(SpeedsR.begin(), SpeedsR.end());
		std::sort(SpeedsN.begin(), SpeedsN.end());
		const double MedR = SpeedsR[SpeedsR.size() / 2], MedN = SpeedsN[SpeedsN.size() / 2];
		const float DirR = BearingFrom(float(RX), float(RY)), DirN = BearingFrom(float(NX), float(NY));
		std::printf("       year 2026: mean %.3f -> %.3f m/s, median %.3f -> %.3f, prevailing from %.2f -> %.2f\n", SumR / Count, SumN / Count, MedR, MedN, DirR, DirN);
		Check(std::fabs(SumN / SumR - 1.0) < 0.003, "year: the mean speed with shifts over the regime's", SumN / SumR);
		Check(std::fabs(MedN / MedR - 1.0) < 0.01, "year: the median speed with shifts over the regime's", MedN / MedR);
		Check(std::fabs(AngleOff(DirR, DirN)) < 0.3, "year: the prevailing direction moves by (degrees)", AngleOff(DirR, DirN));
	}
}

static void TestNoTerrain()
{
	FRand Rnd(3);
	int Bad = 0;
	FWindTerrain Empty;
	for (int I = 0; I < 20000; I++)
	{
		FWindState S = Wind(float(Rnd.In(0.0, 360.0)), float(Rnd.In(0.0, 20.0)), float(Rnd.Next()));
		const double X = Rnd.In(-3000.0, 3000.0), Y = Rnd.In(-3000.0, 6000.0), Z = Rnd.In(0.0, 150.0);
		const double OX = Rnd.In(0.0, 4096.0), OY = Rnd.In(0.0, 4096.0), Sec = Rnd.In(0.0, 10240.0);
		const FLocalWind W = LocalWindAt(S, (I & 1) ? &Empty : nullptr, X, Y, Z, Datum, OX, OY, Sec);
		// ULyudWindSubsystem::GetWindAt as it was
		const FGust G = GustAt(S, X, Y, OX, OY, Sec);
		const float Speed = S.Speed * G.Factor * HeightFactor(float(Z), false);
		const float DX = S.DirX * std::cos(G.Veer) - S.DirY * std::sin(G.Veer), DY = S.DirX * std::sin(G.Veer) + S.DirY * std::cos(G.Veer);
		const bool bSame = W.Speed == Speed && W.VX == DX * Speed && W.VY == DY * Speed && W.VZ == 0.f && W.Updraft == 0.f && W.Exposure == 1.f &&
			W.Shelter == 0.f && W.Gust == G.Factor && W.HeightM == float(Z) && W.SurfaceZ == 0.f && W.FetchM == 0.f && !W.bOverWater &&
			std::fabs(AngleOff(W.FromDeg, S.FromDeg + G.Veer * 57.29578f)) < 1e-3f && W.Turbulence >= 0.f;
		Bad += bSame ? 0 : 1;
	}
	Check(Bad == 0, "no terrain: LocalWindAt is the old speed x gust x land profile, bit for bit (misses)", Bad);
	FWindState S = Wind(250.f, 8.f);
	const FLocalWind A = LocalWindAt(S, nullptr, 10.0, 20.0, 10.0, Datum, 5.0, 6.0, 7.0), B = LocalWindAt(S, nullptr, 10.0, 20.0, 10.0, Datum, 5.0, 6.0, 7.0);
	Check(std::memcmp(&A, &B, sizeof(FLocalWind)) == 0, "no terrain: the same call twice gives the same bits", A.Speed);
	Check(std::fabs(BearingFrom(A.VX, A.VY) - A.FromDeg) < 0.01f, "no terrain: FromDeg is the bearing of (VX, VY)", A.FromDeg);
}

static void TestGrid()
{
	FWindTerrain T;
	T.X0 = 100.0;
	T.Y0 = 200.0;
	T.Cell = 16.0;
	T.NX = 3;
	T.NY = 2;
	T.Ground = { 0.f, 16.f, 32.f, 8.f, 24.f, 40.f };
	Check(T.Valid() && std::fabs(T.GroundAt(108.0, 208.0) - 12.f) < 1e-4f && std::fabs(T.GroundAt(132.0, 200.0) - 32.f) < 1e-4f,
		"grid: GroundAt is bilinear between the cells' centres", T.GroundAt(108.0, 208.0));
	Check(T.GroundAt(100.0 - 16.0, 200.0) == T.SeaZ - 10.f && T.GroundAt(5000.0, 0.0) == T.SeaZ - 10.f && T.CoverAt(108.0, 208.0) == 0.f,
		"grid: a cell outside is the sea bed; no cover grid is no cover", T.GroundAt(5000.0, 0.0));
	Check(std::fabs(T.GroundAt(100.0 - 8.0, 200.0) - (0.5f * (T.SeaZ - 10.f))) < 1e-3f, "grid: within a cell of the edge it eases down to the bed", T.GroundAt(92.0, 200.0));
	Check(T.CellAt(100.0, 200.0) == 0 && T.CellAt(123.9, 215.9) == 4 && T.CellAt(140.1, 200.0) == -1 && T.CellAt(91.9, 200.0) == -1 && T.CellAt(107.9, 207.9) == 0,
		"grid: CellAt is the nearest cell's index, -1 outside", T.CellAt(123.9, 215.9));
	T.Cover = { 0.f, 0.f, 0.f, 0.f, 10.f, 0.f };
	Check(std::fabs(T.CoverAt(116.0, 216.0) - 10.f) < 1e-4f && std::fabs(T.CoverAt(116.0, 208.0) - 5.f) < 1e-4f, "grid: CoverAt is bilinear too", T.CoverAt(116.0, 208.0));
	FWindTerrain Broken = T;
	Broken.Ground.pop_back();
	Check(!Broken.Valid() && Broken.GroundAt(108.0, 208.0) == T.SeaZ - 10.f, "grid: a grid of the wrong size is not valid (and reads as sea)", Broken.GroundAt(108.0, 208.0));
}

/** The largest changes of the wind over one step anywhere along random walks over the terrain. */
struct FSteps
{
	double Speed = 0.0, Up = 0.0, Dir = 0.0, Turb = 0.0, Exposure = 0.0, Fetch = 0.0, Time = 0.0;
	double X = 0.0, Y = 0.0, Z = 0.0, From = 0.0;
};

static FSteps Walks(const FWindTerrain& T, double Step, int Count, double Metres, double XA, double XB, double YA, double YB)
{
	FRand Rnd(11);
	FSteps M;
	for (int Walk = 0; Walk < Count; Walk++)
	{
		const FWindState S = Wind(float(Rnd.In(0.0, 360.0)), 10.f, 0.6f);
		double X = Rnd.In(XA, XB), Y = Rnd.In(YA, YB);
		const double Above = Rnd.Next() < 0.5 ? Rnd.In(1.0, 6.0) : Rnd.In(6.0, 120.0);
		const double Heading = Rnd.In(0.0, 2.0 * Pi);
		const double OX = Rnd.In(0.0, 4096.0), OY = Rnd.In(0.0, 4096.0), Sec = Rnd.In(0.0, 5000.0);
		// a walker keeps a height over the ground; a flyer keeps a level
		const bool bLevel = Walk % 3 == 0;
		const double Level = std::max(T.GroundAt(X, Y), Datum) + Above;
		FLocalWind Prev;
		const int N = int(Metres / Step);
		for (int I = 0; I < N; I++)
		{
			const double Surface = std::max(T.GroundAt(X, Y), Datum);
			// the grid's own landward edges are a 60 m drop to the sea bed in one cell, and a flyer does not fly into a hill
			if (X < XA || Y < YA || Y > YB || (bLevel && Level < Surface + 1.0))
			{
				break;
			}
			const FLocalWind W = LocalWindAt(S, &T, X, Y, bLevel ? Level : Surface + Above, Datum, OX, OY, Sec);
			if (I > 0)
			{
				const double DS = std::fabs(W.Speed - Prev.Speed);
				if (DS > M.Speed)
				{
					M.Speed = DS;
					M.X = X;
					M.Y = Y;
					M.Z = W.HeightM;
					M.From = S.FromDeg;
				}
				M.Up = std::max(M.Up, double(std::fabs(W.Updraft - Prev.Updraft)));
				M.Dir = std::max(M.Dir, double(std::fabs(AngleOff(W.FromDeg, Prev.FromDeg))));
				M.Turb = std::max(M.Turb, double(std::fabs(W.Turbulence - Prev.Turbulence)));
				M.Exposure = std::max(M.Exposure, double(std::fabs(W.Exposure - Prev.Exposure)));
				M.Fetch = std::max(M.Fetch, double(std::fabs(W.FetchM - Prev.FetchM)));
			}
			Prev = W;
			X += std::cos(Heading) * Step;
			Y += std::sin(Heading) * Step;
		}
		// and in time at a point: a sixtieth of a second, the field drifting as the engine drifts it
		const FLocalWind A = LocalWindAt(S, &T, X, Y, Level, Datum, OX, OY, Sec);
		const double Dt = 1.0 / 60.0;
		const FLocalWind B = LocalWindAt(S, &T, X, Y, Level, Datum, OX + S.DirX * S.Speed * Dt, OY + S.DirY * S.Speed * Dt, Sec + Dt);
		M.Time = std::max(M.Time, double(std::fabs(A.Speed - B.Speed)));
	}
	return M;
}

static void TestContinuity(const FWindTerrain& T, const char* Which, double XA, double XB, double YA, double YB)
{
	// the same walks in steps of 25 cm and of 2.5 cm: a jump would be as big in either, a slope ten times smaller
	const FSteps A = Walks(T, 0.25, 400, 300.0, XA, XB, YA, YB), B = Walks(T, 0.025, 400, 300.0, XA, XB, YA, YB);
	char Name[160];
	std::printf("       %s: 400 walks of 300 m in a 10 m/s wind; the steepest speed at (%.0f, %.0f) %.1f m up, wind from %.0f\n", Which, A.X, A.Y, A.Z, A.From);
	std::printf("       largest change in 0.25 m / in 0.025 m: speed %.3f / %.4f m/s, exposure %.3f / %.4f, updraft %.3f / %.4f m/s, bearing %.2f / %.3f deg,\n"
		"       turbulence %.3f / %.4f m/s, fetch %.1f / %.2f m\n", A.Speed, B.Speed, A.Exposure, B.Exposure, A.Up, B.Up, A.Dir, B.Dir, A.Turb, B.Turb, A.Fetch, B.Fetch);
	std::snprintf(Name, sizeof(Name), "continuity (%s): speed changes under 1 m/s in 0.25 m anywhere (a brow's lee is that sharp)", Which);
	Check(A.Speed < 1.0, Name, A.Speed);
	std::snprintf(Name, sizeof(Name), "continuity (%s): exposure under 0.1 in 0.25 m", Which);
	Check(A.Exposure < 0.1, Name, A.Exposure);
	std::snprintf(Name, sizeof(Name), "continuity (%s): updraft under 0.4 m/s in 0.25 m", Which);
	Check(A.Up < 0.4, Name, A.Up);
	std::snprintf(Name, sizeof(Name), "continuity (%s): bearing under 1 degree in 0.25 m", Which);
	Check(A.Dir < 1.0, Name, A.Dir);
	std::snprintf(Name, sizeof(Name), "continuity (%s): turbulence under 0.4 m/s in 0.25 m", Which);
	Check(A.Turb < 0.4, Name, A.Turb);
	// (the fetch is steep by nature: a step out from behind a headland opens kilometres of water)
	std::snprintf(Name, sizeof(Name), "continuity (%s): fetch under 300 m in 0.25 m", Which);
	Check(A.Fetch < 300.0, Name, A.Fetch);
	std::snprintf(Name, sizeof(Name), "continuity (%s): no jumps: ten times finer steps change at most a fifth as much (worst ratio)", Which);
	const double Ratio = std::max(std::max(B.Speed / A.Speed, B.Exposure / A.Exposure), std::max(std::max(B.Up / A.Up, B.Turb / A.Turb), std::max(B.Fetch / A.Fetch, B.Dir / A.Dir)));
	Check(Ratio < 0.2, Name, Ratio);
	std::snprintf(Name, sizeof(Name), "continuity (%s): speed changes under 0.2 m/s in a frame (1/60 s)", Which);
	Check(A.Time < 0.2, Name, A.Time);

	// right across the grid's own lines: a millimetre either side of a cell centre line and of the grid's edge
	FRand Rnd(13);
	double MaxEdge = 0.0;
	for (int I = 0; I < 4000; I++)
	{
		const FWindState S = Wind(float(Rnd.In(0.0, 360.0)), 10.f, 0.6f);
		const int Cx = int(Rnd.In(0, T.NX)), Cy = int(Rnd.In(0, T.NY));
		const double X = T.X0 + Cx * T.Cell, Y = T.Y0 + Rnd.In(-1.0, T.NY) * T.Cell;
		const double Z = Rnd.In(-40.0, 60.0);
		const bool bAlongX = I & 1;
		const double PX = bAlongX ? X : T.X0 + Rnd.In(-1.0, T.NX) * T.Cell, PY = bAlongX ? Y : T.Y0 + Cy * T.Cell;
		const FLocalWind P = LocalWindAt(S, &T, PX - (bAlongX ? 0.001 : 0.0), PY - (bAlongX ? 0.0 : 0.001), Z, Datum, 0, 0, 0);
		const FLocalWind Q = LocalWindAt(S, &T, PX + (bAlongX ? 0.001 : 0.0), PY + (bAlongX ? 0.0 : 0.001), Z, Datum, 0, 0, 0);
		MaxEdge = std::max(MaxEdge, double(std::fabs(P.Speed - Q.Speed) + std::fabs(P.Updraft - Q.Updraft) + std::fabs(P.Turbulence - Q.Turbulence)));
	}
	std::snprintf(Name, sizeof(Name), "continuity (%s): nothing steps across a cell line or the grid's edge (2 mm)", Which);
	Check(MaxEdge < 0.02, Name, MaxEdge);
}

/** The coast's own checks: the same on the built-in coast and on a cliffed stretch of the real map. */
static void TestCoast(const FWindTerrain& T, const char* Which, double Y, const FCoast& C)
{
	char Name[200];
	const float U = 8.f;
	const FWindState Off = Wind(90.f, U), On = Wind(270.f, U);
	const double Beach = 0.5 * (C.ShoreX + C.FootX), Face = 0.5 * (C.FootX + C.BrowX);
	std::printf("       %s, row Y %.0f: water line X %.0f, cliff foot %.0f, brow %.0f at Z %.1f (%.0f m over the sea)\n", Which, Y, C.ShoreX, C.FootX,
		C.BrowX, C.TopZ, C.TopZ - Datum);

	// --- offshore (from the east, blowing out to sea)
	const FLocalWind OffBeach = Steady(Off, &T, Beach, Y, 2.0), OffSea100 = Steady(Off, &T, C.ShoreX + 100.0, Y, 2.0);
	const FLocalWind OffSea400 = Steady(Off, &T, C.ShoreX + 400.0, Y, 2.0), OffSea1500 = Steady(Off, &T, C.ShoreX + 1500.0, Y, 2.0);
	const FLocalWind OffSea1000 = Steady(Off, &T, C.ShoreX + 1000.0, Y, 2.0), OffSea2000 = Steady(Off, &T, C.ShoreX + 2000.0, Y, 2.0);
	const FLocalWind OffOver = Steady(Off, &T, Beach, Y, 30.0);
	std::printf("     offshore wind, %g m/s from 90:\n", U);
	PrintWind("beach under the cliff, 2 m", OffBeach);
	PrintWind("beach, 30 m up", OffOver);
	PrintWind("sea 100 m out, 2 m", OffSea100);
	PrintWind("sea 400 m out, 2 m", OffSea400);
	PrintWind("sea 1000 m out, 2 m", OffSea1000);
	PrintWind("sea 1500 m out, 2 m", OffSea1500);
	PrintWind("sea 2000 m out, 2 m", OffSea2000);
	std::snprintf(Name, sizeof(Name), "%s, offshore: the beach under the cliff is in a lee (exposure)", Which);
	Check(OffBeach.Exposure < 0.4f && OffBeach.Shelter > 0.6f, Name, OffBeach.Exposure);
	std::snprintf(Name, sizeof(Name), "%s, offshore: the sea 1.5 km out is open (exposure)", Which);
	Check(OffSea1500.Exposure > 0.97f && OffSea1500.Shelter < 0.01f && OffSea2000.Exposure > 0.99f, Name, OffSea1500.Exposure);
	std::snprintf(Name, sizeof(Name), "%s, offshore: the calm strip fades going out (speed 100 m < 400 m < 1000 m)", Which);
	Check(OffBeach.Speed < OffSea100.Speed + 0.3f && OffSea100.Speed < OffSea400.Speed && OffSea400.Speed < OffSea1000.Speed && OffSea1000.Speed <= OffSea1500.Speed + 0.01f,
		Name, OffSea400.Speed);
	std::snprintf(Name, sizeof(Name), "%s, offshore: the lee tumbles (beach turbulence over the open sea's)", Which);
	Check(OffBeach.Turbulence > 1.8f * OffSea1500.Turbulence, Name, OffBeach.Turbulence / OffSea1500.Turbulence);
	std::snprintf(Name, sizeof(Name), "%s, offshore: the air sinks behind the brow (updraft 30 m over the beach)", Which);
	Check(OffOver.Updraft < -0.3f && OffOver.Updraft > -1.6f && OffBeach.Updraft > -0.05f, Name, OffOver.Updraft);
	std::snprintf(Name, sizeof(Name), "%s, offshore: the fetch grows going out to sea (at 400 m)", Which);
	Check(OffBeach.FetchM == 0.f && OffSea100.FetchM > 60.f && OffSea100.FetchM < 140.f && OffSea400.FetchM > 300.f && OffSea400.FetchM < 500.f &&
		OffSea1000.FetchM > 800.f && OffSea1000.FetchM < 1200.f && OffSea2000.FetchM > OffSea1000.FetchM, Name, OffSea400.FetchM);

	// --- onshore (from the west, off the sea)
	const FLocalWind OnSea = Steady(On, &T, C.ShoreX + 1500.0, Y, 10.0), OnSea2 = Steady(On, &T, C.ShoreX + 1500.0, Y, 2.0);
	const FLocalWind OnBeach = Steady(On, &T, Beach, Y, 2.0);
	const FLocalWind OnFace = Steady(On, &T, Face, Y, 5.0), OnFace20 = Steady(On, &T, Face, Y, 20.0);
	const FLocalWind OnBrow = Steady(On, &T, C.BrowX + 4.0, Y, 10.0), OnBrowHigh = Steady(On, &T, C.BrowX + 4.0, Y, 250.0);
	const FLocalWind OnBrow3 = Steady(On, &T, C.BrowX, Y, 3.0);
	const FLocalWind OnSeaNear = Steady(On, &T, C.ShoreX + 150.0, Y, 60.0);
	std::printf("     onshore wind, %g m/s from 270:\n", U);
	PrintWind("sea 1500 m out, 10 m", OnSea);
	PrintWind("sea 1500 m out, 2 m", OnSea2);
	PrintWind("beach, 2 m", OnBeach);
	PrintWind("cliff face, 5 m off it", OnFace);
	PrintWind("cliff face, 20 m off it", OnFace20);
	PrintWind("just seaward of the brow, 10 m", OnBrow);
	PrintWind("the brow, 3 m", OnBrow3);
	PrintWind("over the brow, 250 m", OnBrowHigh);
	PrintWind("150 m out to sea, 60 m up", OnSeaNear);
	std::snprintf(Name, sizeof(Name), "%s, onshore: the open sea has the regime's wind at 10 m and the cap's fetch", Which);
	Check(std::fabs(OnSea.Speed - U) < 0.02f && OnSea.FetchM == 3000.f && OnSea.bOverWater && std::fabs(OnSea2.Speed - U * HeightFactor(2.f, true)) < 0.02f, Name, OnSea.Speed);
	std::snprintf(Name, sizeof(Name), "%s, onshore: the beach is open (exposure)", Which);
	Check(OnBeach.Exposure > 0.9f && OnBeach.Shelter < 0.05f && OnBeach.FetchM == 0.f && !OnBeach.bOverWater, Name, OnBeach.Exposure);
	std::snprintf(Name, sizeof(Name), "%s, onshore: ridge lift on the cliff's face, share of the wind", Which);
	Check(OnFace20.Updraft > 0.3f * U && OnFace20.Updraft < 0.95f * U && OnFace.Updraft > 0.15f * U, Name, OnFace20.Updraft / U);
	std::snprintf(Name, sizeof(Name), "%s, onshore: lift at and just above the crest, share of the wind", Which);
	Check(OnBrow.Updraft > 0.15f * U && OnBrow3.Updraft > 0.05f * U, Name, OnBrow.Updraft / U);
	std::snprintf(Name, sizeof(Name), "%s, onshore: the lift fades high above the crest (250 m over 10 m)", Which);
	Check(OnBrowHigh.Updraft < 0.3f * OnBrow.Updraft && OnBrowHigh.Updraft >= 0.f, Name, OnBrowHigh.Updraft / std::max(OnBrow.Updraft, 1e-3f));
	std::snprintf(Name, sizeof(Name), "%s, onshore: no lift 150 m out to sea, nor sink", Which);
	Check(std::fabs(OnSeaNear.Updraft) < 0.05f, Name, OnSeaNear.Updraft);
	std::snprintf(Name, sizeof(Name), "%s, onshore: the wind is squeezed over the brow (exposure at 3 m)", Which);
	Check(OnBrow3.Exposure > 1.12f && OnBrow3.Exposure < 1.45f, Name, OnBrow3.Exposure);
	std::snprintf(Name, sizeof(Name), "%s, onshore: the sea under an onshore wind is not in the cliff's lee", Which);
	Check(Steady(On, &T, C.ShoreX + 60.0, Y, 2.0).Shelter == 0.f, Name, Steady(On, &T, C.ShoreX + 60.0, Y, 2.0).Exposure);

	// --- along the shore: the cliff turns the low air a little, and less higher up
	const FWindState Along = Wind(225.f, U);
	const FLocalWind Slide = Steady(Along, &T, C.FootX - 6.0, Y, 2.0), SlideHigh = Steady(Along, &T, C.FootX - 6.0, Y, 150.0);
	std::printf("     wind from 225 (aslant onto the cliff):\n");
	PrintWind("foot of the cliff, 2 m", Slide);
	PrintWind("the same, 150 m up", SlideHigh);
	const float Bend = AngleOff(225.f, Slide.FromDeg), BendHigh = AngleOff(225.f, SlideHigh.FromDeg);
	std::snprintf(Name, sizeof(Name), "%s: a slanting wind is bent along the cliff at its foot, degrees (- = backed toward the south)", Which);
	Check(Bend < -3.f && Bend > -16.f && std::fabs(BendHigh) < 0.25f * std::fabs(Bend), Name, Bend);
}

static void TestSynthetic(const FWindTerrain& T)
{
	const float U = 8.f;
	const FWindState On = Wind(270.f, U), Off = Wind(90.f, U);
	FCoast C;
	C.ShoreX = Synth::ShoreX;
	C.FootX = Synth::FootX;
	C.BrowX = Synth::BrowX;
	C.TopZ = Synth::TopZ;
	C.bCliff = true;
	TestCoast(T, "built-in coast", -400.0, C);

	// the bare upland far inland against the open sea, both at 10 m
	const FLocalWind Sea = Steady(On, &T, Synth::ShoreX + 1500.0, -800.0, 10.0), Land = Steady(On, &T, -800.0, -800.0, 10.0);
	const FLocalWind Land2 = Steady(On, &T, -800.0, -800.0, 2.0), Sea2 = Steady(On, &T, Synth::ShoreX + 1500.0, -800.0, 2.0);
	std::printf("     onshore, inland:\n");
	PrintWind("bare upland 900 m inland, 10 m", Land);
	PrintWind("bare upland 900 m inland, 2 m", Land2);
	Check(Sea.Speed > Land.Speed * 1.15f && Land.Speed > 0.7f * U, "built-in coast: the wind over open water at 10 m beats the land's at 10 m (ratio)", Sea.Speed / Land.Speed);
	Check(Sea2.Speed / Sea.Speed > Land2.Speed / Land.Speed + 0.1f, "built-in coast: and the land's profile is the fuller (2 m over 10 m, land)", Land2.Speed / Land.Speed);
	// the sea's wind lasts a little way inland, the land's a little way out
	const FLocalWind Edge = Steady(On, &T, Synth::BrowX - 150.0, -800.0, 10.0);
	Check(Edge.Speed < Sea.Speed && Edge.Speed > Land.Speed + 0.1f, "built-in coast: 150 m inland of the brow the wind is between the two", Edge.Speed);
	const FLocalWind OffNear = Steady(Off, &T, Synth::ShoreX + 2500.0, -800.0, 10.0);
	Check(std::fabs(OffNear.Speed - U) < 0.05f, "built-in coast: 2.5 km out an offshore wind has become the sea's", OffNear.Speed);

	// the wood: its lee, the air among the trunks, the air over the crowns
	const double WoodEdgeX = Synth::WoodX, Yw = 600.0;
	// an offshore wind comes through the wood and out over the 60 m of bare ground before the brow
	const FLocalWind LeeNear = Steady(Off, &T, WoodEdgeX + 25.0, Yw, 2.0), Under = Steady(Off, &T, WoodEdgeX - 200.0, Yw, 2.0);
	const FLocalWind Over = Steady(Off, &T, WoodEdgeX - 200.0, Yw, 40.0), Bare = Steady(Off, &T, WoodEdgeX - 200.0, -800.0, 2.0);
	const FLocalWind BareOver = Steady(Off, &T, WoodEdgeX - 200.0, -800.0, 40.0);
	// an onshore wind: the wood is downwind of the bare strip; a wood upwind of a clearing shelters it, a wood downwind does not
	const FLocalWind Windward = Steady(On, &T, WoodEdgeX + 25.0, Yw, 2.0);
	std::printf("     the wood (24 m crowns):\n");
	PrintWind("25 m in its lee, 2 m", LeeNear);
	PrintWind("25 m to windward of it, 2 m", Windward);
	PrintWind("among the trunks, 2 m", Under);
	PrintWind("over the crowns, 40 m", Over);
	PrintWind("bare upland, 2 m", Bare);
	PrintWind("bare upland, 40 m", BareOver);
	Check(LeeNear.Speed < 0.6f * Bare.Speed && LeeNear.Shelter > 0.4f, "built-in coast: the ground behind the wood is slower than bare ground (ratio)", LeeNear.Speed / Bare.Speed);
	Check(LeeNear.Exposure > 0.25f, "built-in coast: but a wood is porous: not the cliff's dead lee (exposure)", LeeNear.Exposure);
	Check(Windward.Shelter < 0.05f, "built-in coast: to windward of the wood there is no shelter", Windward.Shelter);
	Check(Under.Speed < 0.5f * Bare.Speed, "built-in coast: among the trunks it is slower still (ratio to bare)", Under.Speed / Bare.Speed);
	Check(Over.Speed < BareOver.Speed && Over.Speed > 0.4f * BareOver.Speed && Over.Shelter < 0.1f, "built-in coast: over the crowns the wind runs, slower than over bare ground (ratio)",
		Over.Speed / BareOver.Speed);
	Check(Under.Turbulence > Bare.Turbulence, "built-in coast: rough cover stirs the air (turbulence, wood over bare)", Under.Turbulence / Bare.Turbulence);

	// the house: a lee behind it that ends, nothing beside it
	const double Hx = Synth::HouseX, Hy = Synth::HouseY, Edge2 = Synth::HouseHalf;
	const FLocalWind Behind = Steady(On, &T, Hx - Edge2 - 40.0, Hy, 2.0), Far = Steady(On, &T, Hx - Edge2 - 600.0, Hy, 2.0);
	const FLocalWind Beside = Steady(On, &T, Hx - Edge2 - 40.0, Hy + 150.0, 2.0), Front = Steady(On, &T, Hx + Edge2 + 40.0, Hy, 2.0);
	const FLocalWind Above = Steady(On, &T, Hx - Edge2 - 40.0, Hy, 60.0);
	std::printf("     the house (30 m):\n");
	PrintWind("40 m behind it, 2 m", Behind);
	PrintWind("600 m behind it, 2 m", Far);
	PrintWind("150 m to the side, 2 m", Beside);
	PrintWind("40 m before it, 2 m", Front);
	PrintWind("40 m behind it, 60 m up", Above);
	Check(Behind.Speed < 0.6f * Beside.Speed && Behind.Shelter > 0.4f, "built-in coast: the ground behind a house is slower (ratio to beside it)", Behind.Speed / Beside.Speed);
	Check(Far.Shelter == 0.f && Beside.Shelter == 0.f && Front.Shelter == 0.f && Above.Shelter == 0.f, "built-in coast: its lee ends: none 600 m on, beside, before or well above it",
		Far.Shelter);
	Check(Behind.Turbulence > Beside.Turbulence, "built-in coast: and its lee tumbles a little (turbulence ratio)", Behind.Turbulence / Beside.Turbulence);

	// the tide: at low water the flat is land to the wind
	{
		const double X = Synth::ShoreX + 80.0;
		const FLocalWind High = LocalWindAt(Wind(270.f, U, 0.f), &T, X, -400.0, Datum + 2.0, Datum, 0, 0, 0);
		const FLocalWind LowTide = LocalWindAt(Wind(270.f, U, 0.f), &T, X, -400.0, Datum + 2.0, Datum - 4.5f, 0, 0, 0);
		Check(High.bOverWater && !LowTide.bOverWater && LowTide.FetchM == 0.f && High.FetchM > 2000.f && LowTide.SurfaceZ < High.SurfaceZ,
			"built-in coast: at spring low water the bared flat 80 m out is land (surface Z)", LowTide.SurfaceZ);
	}
}

static void TestRealMap(const FWindTerrain& T, double MapY)
{
	// a cliffed stretch south of the club (the upland's sea cliff) and the town's front
	const double Yc = -300.0 + MapY;
	const FCoast C = FindCoast(T, Yc);
	Check(C.bCliff && C.ShoreX > 100.0 && C.ShoreX < 400.0 && C.BrowX < C.FootX && C.FootX < C.ShoreX, "real map: the sea cliff is found at map Y -300 (brow's height over the sea)",
		C.TopZ - Datum);
	if (C.bCliff)
	{
		TestCoast(T, "real map", Yc, C);
	}
	const float U = 8.f;
	const FWindState On = Wind(270.f, U), Off = Wind(90.f, U);
	// the upland's wood begins at the brow there
	const FLocalWind Wood = Steady(On, &T, C.BrowX - 150.0, Yc, 2.0), WoodOver = Steady(On, &T, C.BrowX - 150.0, Yc, 45.0);
	const FLocalWind Sea = Steady(On, &T, C.ShoreX + 1500.0, Yc, 10.0);
	std::printf("     the upland's old wood, onshore wind (cover there %.1f m):\n", T.CoverAt(C.BrowX - 150.0, Yc));
	PrintWind("150 m inland of the brow, 2 m", Wood);
	PrintWind("the same, 45 m up", WoodOver);
	Check(T.CoverAt(C.BrowX - 150.0, Yc) > 10.f && Wood.Speed < 0.3f * U && Wood.Shelter > 0.4f, "real map: under the upland's wood the ground wind is a fraction of the sea's", Wood.Speed / U);
	Check(WoodOver.Speed > 2.f * Wood.Speed && WoodOver.Speed < Sea.Speed * HeightFactor(45.f, true), "real map: over its crowns the wind runs (45 m, m/s)", WoodOver.Speed);

	// the town (map Y ~905): the promenade, a street behind a block
	const double Yt = 905.0 + MapY;
	const FCoast Town = FindCoast(T, Yt);
	std::printf("       the town's front, row Y %.0f: water line X %.0f\n", Yt, Town.ShoreX);
	// the tallest cover within the town, and a point in its lee
	double BestX = 0.0, BestY = 0.0;
	float BestH = 0.f;
	for (double Y = 400.0 + MapY; Y < 1400.0 + MapY; Y += 16.0)
	{
		for (double X = Town.ShoreX - 700.0; X < Town.ShoreX - 30.0; X += 16.0)
		{
			// a block with open ground east of it, so that the lee is its own
			const float H = T.CoverAt(X, Y);
			if (H > BestH && T.CoverAt(X - 48.0, Y) < 1.f && T.CoverAt(X - 64.0, Y) < 1.f)
			{
				BestH = H;
				BestX = X;
				BestY = Y;
			}
		}
	}
	std::printf("       a block %.0f m high at map (%.0f, %.0f)\n", BestH, BestX, BestY - MapY);
	if (BestH > 6.f)
	{
		const FLocalWind Behind = Steady(On, &T, BestX - 48.0, BestY, 2.0), Prom = Steady(On, &T, Town.ShoreX - 12.0, Yt, 2.0);
		const FLocalWind Roof = Steady(On, &T, BestX - 48.0, BestY, BestH * 2.5);
		const FLocalWind OffProm = Steady(Off, &T, Town.ShoreX - 12.0, Yt, 2.0);
		std::printf("     the town, onshore wind:\n");
		PrintWind("the water's edge on the axis, 2 m", Prom);
		PrintWind("48 m behind the block, 2 m", Behind);
		PrintWind("the same, well over its roof", Roof);
		std::printf("     the town, offshore wind:\n");
		PrintWind("the water's edge on the axis, 2 m", OffProm);
		Check(Behind.Shelter > 0.3f && Behind.Speed < 0.75f * Prom.Speed, "real map: the ground behind a town block is slower than the front (ratio)", Behind.Speed / Prom.Speed);
		Check(Roof.Shelter < 0.05f, "real map: well over its roof there is no shelter", Roof.Shelter);
		Check(Prom.Exposure > 0.9f, "real map: the town's front is open to an onshore wind (exposure)", Prom.Exposure);
	}
	else
	{
		std::printf("       (no free-standing block found in the cover grid: the town checks are skipped)\n");
	}
	// far inland, open ground against the sea at 10 m
	{
		// the most open land cell of the town's lowland: no cover within 100 m
		double BX = 0.0, BY = 0.0;
		bool bFound = false;
		for (double Y = 500.0 + MapY; Y < 1300.0 + MapY && !bFound; Y += 32.0)
		{
			for (double X = Town.ShoreX - 900.0; X < Town.ShoreX - 400.0 && !bFound; X += 32.0)
			{
				bool bOpen = true;
				for (double D = 0.0; D <= 200.0; D += 16.0)
				{
					bOpen = bOpen && T.CoverAt(X + D, Y) < 0.5f && T.GroundAt(X + D, Y) <= T.GroundAt(X, Y) + 1.f;
				}
				if (bOpen)
				{
					BX = X;
					BY = Y;
					bFound = true;
				}
			}
		}
		if (bFound)
		{
			const FLocalWind Land = Steady(On, &T, BX, BY, 10.0);
			std::printf("     open ground at map (%.0f, %.0f), %.0f m from the water, onshore wind:\n", BX, BY - MapY, Town.ShoreX - BX);
			PrintWind("10 m", Land);
			Check(Sea.Speed > Land.Speed * 1.05f, "real map: the wind over open water at 10 m beats open land's at 10 m (ratio)", Sea.Speed / Land.Speed);
		}
	}
}

static void TestAhead()
{
	FRand Rnd(5);
	// GustAhead against the engine's own drift: Tick adds Mean x dt to the offset (wrapped) and dt to the clock
	double Worst = 0.0, WorstExact = 0.0;
	for (int I = 0; I < 300; I++)
	{
		const FWindState S = Wind(float(Rnd.In(0.0, 360.0)), float(Rnd.In(1.0, 16.0)), float(Rnd.Next()));
		const double X = Rnd.In(-2000.0, 2000.0), Y = Rnd.In(-2000.0, 5000.0);
		double OX = Rnd.In(0.0, GustPeriod), OY = Rnd.In(0.0, GustPeriod), Sec = Rnd.In(0.0, 9000.0);
		const double In = Rnd.In(0.0, 120.0);
		const float Ahead = GustAhead(S, X, Y, OX, OY, Sec, In);
		const double OX1 = OX + double(S.DirX) * S.Speed * In, OY1 = OY + double(S.DirY) * S.Speed * In;
		WorstExact = std::max(WorstExact, double(std::fabs(Ahead - GustAt(S, X, Y, OX1, OY1, Sec + In).Factor)));
		const int Frames = int(std::ceil(In * 60.0));
		const double Dt = Frames > 0 ? In / Frames : 0.0;
		for (int F = 0; F < Frames; F++)
		{
			OX = std::fmod(OX + double(S.DirX * S.Speed) * Dt + GustPeriod, GustPeriod);
			OY = std::fmod(OY + double(S.DirY * S.Speed) * Dt + GustPeriod, GustPeriod);
			Sec += Dt;
		}
		Worst = std::max(Worst, double(std::fabs(Ahead - GustAt(S, X, Y, OX, OY, Sec).Factor)));
	}
	Check(WorstExact == 0.0, "GustAhead: is GustAt at the later time with the drifted offset, exactly", WorstExact);
	Check(Worst < 2e-3, "GustAhead: and what the engine's frame-by-frame drift arrives at (worst difference)", Worst);
	{
		// and it is what LocalWindAt reports there then, in the open
		const FWindState S = Wind(250.f, 9.f, 0.8f);
		const float Ahead = GustAhead(S, 100.0, 200.0, 30.0, 40.0, 50.0, 25.0);
		const FLocalWind W = LocalWindAt(S, nullptr, 100.0, 200.0, 10.0, Datum, 30.0 + double(S.DirX) * S.Speed * 25.0, 40.0 + double(S.DirY) * S.Speed * 25.0, 75.0);
		Check(W.Gust == Ahead && GustAhead(S, 100.0, 200.0, 30.0, 40.0, 50.0, 0.0) == GustAt(S, 100.0, 200.0, 30.0, 40.0, 50.0).Factor,
			"GustAhead: 25 s ahead is LocalWindAt().Gust 25 s later; 0 s ahead is now", Ahead);
	}

	// NextGust against a fine scan
	int Found = 0, Agree = 0, None = 0, NoneAgree = 0, Lulls = 0, PeakOk = 0, LastOk = 0, Planted = 0, Grazes = 0;
	double WorstIn = 0.0;
	for (int I = 0; I < 400; I++)
	{
		const FWindState S = Wind(float(Rnd.In(0.0, 360.0)), float(Rnd.In(3.0, 14.0)), float(Rnd.Next()));
		const double X = Rnd.In(-2000.0, 2000.0), Y = Rnd.In(-2000.0, 5000.0);
		const double OX = Rnd.In(0.0, GustPeriod), OY = Rnd.In(0.0, GustPeriod), Sec = Rnd.In(0.0, 9000.0);
		const float Thr = 0.25f;
		const FGustEvent E = NextGust(S, X, Y, OX, OY, Sec, 60.0, Thr);
		double First = -1.0, End = -1.0;
		float Peak = 1.f;
		bool bLull = false;
		for (double D = 0.0; D <= 60.0 + 120.0; D += 0.02)
		{
			const float F = GustAhead(S, X, Y, OX, OY, Sec, D);
			if (First < 0.0)
			{
				if (D > 60.0)
				{
					break;
				}
				if (F > 1.f + Thr || F < 1.f - Thr)
				{
					First = D;
					bLull = F < 1.f;
					Peak = F;
				}
			}
			else if (bLull ? F >= 1.f - Thr : F <= 1.f + Thr)
			{
				End = D;
				break;
			}
			else
			{
				Peak = bLull ? std::min(Peak, F) : std::max(Peak, F);
			}
		}
		if (First < 0.0)
		{
			None++;
			NoneAgree += E.InSeconds < 0.f ? 1 : 0;
			continue;
		}
		const bool bSame = E.InSeconds >= 0.f && std::fabs(E.InSeconds - First) < 1.0 && E.bLull == bLull;
		// NextGust looks once a second: a graze of the threshold shorter than that is not worth a word, and it may miss it
		if (!bSame && End >= 0.0 && End - First < 1.0)
		{
			Grazes++;
			continue;
		}
		Found++;
		Planted += First > 10.0 ? 1 : 0;
		Lulls += bLull ? 1 : 0;
		Agree += bSame ? 1 : 0;
		if (bSame)
		{
			WorstIn = std::max(WorstIn, std::fabs(double(E.InSeconds) - First));
			PeakOk += std::fabs(E.Factor - Peak) < 0.03f ? 1 : 0;
			LastOk += End < 0.0 || std::fabs(E.LastsSeconds - (End - First)) < 1.5 ? 1 : 0;
		}
	}
	std::printf("       NextGust over 400 random looks: %d with a gust or lull within a minute (%d lulls, %d more than 10 s off), %d with none, %d grazes under a second missed\n", Found, Lulls, Planted, None, Grazes);
	Check(Found > 50 && Planted > 20 && Agree >= Found * 0.99 && Grazes < 40, "NextGust: finds the fine scan's first gust or lull within a second (share)", double(Agree) / std::max(Found, 1));
	Check(NoneAgree >= None * 0.97 && None > 20, "NextGust: says none when the fine scan finds none (share)", double(NoneAgree) / std::max(None, 1));
	Check(PeakOk >= Agree * 0.95 && LastOk >= Agree * 0.9, "NextGust: its peak is the scan's within 0.03 (share)", double(PeakOk) / std::max(Agree, 1));
	std::printf("       worst timing miss among the agreed %.2f s; how long it lasts agrees within 1.5 s in %.0f%%\n", WorstIn, 100.0 * LastOk / std::max(Agree, 1));
	{
		// calm air has no gusts; the look-ahead is capped at two minutes
		FWindState S = Wind(250.f, 8.f, 0.f);
		S.Gustiness = 0.f;
		Check(NextGust(S, 0, 0, 0, 0, 0, 60.0).InSeconds < 0.f && NextGust(S, 0, 0, 0, 0, 0, 1e9).InSeconds < 0.f, "NextGust: air without gusts has none, however far it looks", -1.0);
		S.Gustiness = 0.4f;
		const FGustEvent E = NextGust(S, 0, 0, 0, 0, 0, 1e9, 0.05f);
		Check(E.InSeconds >= 0.f && E.InSeconds <= 120.f && E.LastsSeconds > 0.f && (E.bLull ? E.Factor < 0.95f : E.Factor > 1.05f), "NextGust: a low threshold in gusty air finds one soon (s)",
			E.InSeconds);
	}
}

static void TestWords()
{
	// WMO's table, m/s: the top of each force and the bottom of the next
	static const float Top[12] = { 0.2f, 1.5f, 3.3f, 5.4f, 7.9f, 10.7f, 13.8f, 17.1f, 20.7f, 24.4f, 28.4f, 32.6f };
	static const float Bottom[12] = { 0.3f, 1.6f, 3.4f, 5.5f, 8.0f, 10.8f, 13.9f, 17.2f, 20.8f, 24.5f, 28.5f, 32.7f };
	int Bad = 0;
	for (int F = 0; F < 12; F++)
	{
		Bad += Beaufort(Top[F]) == F ? 0 : 1;
		Bad += Beaufort(Bottom[F]) == F + 1 ? 0 : 1;
	}
	Bad += Beaufort(0.f) == 0 && Beaufort(-1.f) == 0 && Beaufort(60.f) == 12 ? 0 : 1;
	Check(Bad == 0, "Beaufort: every edge of the WMO table (misses)", Bad);
	Check(!std::strcmp(BeaufortKey(0), "calm") && !std::strcmp(BeaufortKey(4), "moderate_breeze") && !std::strcmp(BeaufortKey(8), "gale") &&
		!std::strcmp(BeaufortKey(12), "hurricane") && !std::strcmp(BeaufortKey(-3), "calm") && !std::strcmp(BeaufortKey(40), "hurricane"),
		"BeaufortKey: calm .. hurricane, clamped", 12);
	Check(!std::strcmp(CompassKey(0.f, 8), "n") && !std::strcmp(CompassKey(44.f, 8), "ne") && !std::strcmp(CompassKey(247.5f, 16), "wsw") &&
		!std::strcmp(CompassKey(247.5f, 4), "w") && !std::strcmp(CompassKey(359.f, 16), "n") && !std::strcmp(CompassKey(-90.f), "w") &&
		!std::strcmp(CompassKey(225.f, 4), "w") && !std::strcmp(CompassKey(100.f, 4), "e") && !std::strcmp(CompassKey(191.f, 16), "s") &&
		!std::strcmp(CompassKey(192.f, 16), "ssw") && !std::strcmp(CompassKey(725.f, 8), "n") && !std::strcmp(CompassKey(170.f, 7), "s"),
		"CompassKey: 4, 8 and 16 points, wrapped", 16);
	Check(std::fabs(ToKnots(10.f) - 19.438f) < 0.01f, "ToKnots: 10 m/s in knots", ToKnots(10.f));

	// the frame: north = +Y, east = -X; a west wind blows toward -X (onto this coast from the sea at +X)
	float X, Y;
	TowardOf(270.f, X, Y);
	bool bFrame = std::fabs(X + 1.f) < 1e-6f && std::fabs(Y) < 1e-6f;
	TowardOf(0.f, X, Y);
	bFrame = bFrame && std::fabs(X) < 1e-6f && std::fabs(Y + 1.f) < 1e-6f;
	TowardOf(90.f, X, Y);
	bFrame = bFrame && std::fabs(X - 1.f) < 1e-6f;
	Check(bFrame, "TowardOf: a west wind blows toward -X, a north wind toward -Y, an east wind out to sea (+X)", X);
	double Worst = 0.0;
	for (float D = 0.f; D < 360.f; D += 7.3f)
	{
		TowardOf(D, X, Y);
		Worst = std::max(Worst, double(std::fabs(AngleOff(D, BearingFrom(X * 5.f, Y * 5.f)))));
	}
	const FWindState R = Regime(HoursSince2000(2026, 10, 20, 16.0), GameParams());
	Check(Worst < 1e-3 && std::fabs(AngleOff(R.FromDeg, BearingFrom(R.DirX, R.DirY))) < 0.01f, "BearingFrom: undoes TowardOf, and agrees with Regime's FromDeg and Dir", Worst);
	Check(AngleOff(0.f, 90.f) == 90.f && AngleOff(350.f, 10.f) == 20.f && AngleOff(10.f, 350.f) == -20.f && AngleOff(90.f, 270.f) == -180.f && AngleOff(45.f, 45.f) == 0.f,
		"AngleOff: heading north, an east wind is 90 on the starboard side", AngleOff(0.f, 90.f));
	float AX, AY;
	Apparent(-5.f, 0.f, 3.f, 0.f, AX, AY);
	Check(AX == -8.f && AY == 0.f, "Apparent: beating into 5 m/s at 3 m/s feels 8", -AX);
}

static void TestOutlook()
{
	const FWindParams P = GameParams();
	FRand Rnd(9);
	int Bad = 0, TurnBad = 0, TrendBad = 0;
	for (int I = 0; I < 200; I++)
	{
		const double H = HoursSince2000(2026, 1, 1, 0.0) + Rnd.In(0.0, 365.0 * 24.0), Span = Rnd.In(0.5, 12.0);
		const FWindOutlook O = Outlook(H, Span, P);
		// by hand: the same ten-minute looks, and six times finer for what lies between them
		const int Looks = int(std::ceil(Span * 6.0 - 1e-9)) + 1;
		double Lo = 1e9, Hi = 0.0, Sum = 0.0, Gust = 0.0, VX = 0.0, VY = 0.0;
		for (int K = 0; K < Looks; K++)
		{
			const FWindState S = Now(H + Span * K / (Looks - 1), P);
			Lo = std::min(Lo, double(S.Speed));
			Hi = std::max(Hi, double(S.Speed));
			Sum += S.Speed;
			Gust = std::max(Gust, double(S.Speed * (1.f + 2.2f * S.Gustiness * 0.8f)));
			VX += S.DirX * S.Speed;
			VY += S.DirY * S.Speed;
		}
		double FineLo = 1e9, FineHi = 0.0, FineSum = 0.0;
		int N = 0;
		for (double A = 0.0; A <= Span; A += 1.0 / 36.0)
		{
			const FWindState S = Now(H + A, P);
			FineLo = std::min(FineLo, double(S.Speed));
			FineHi = std::max(FineHi, double(S.Speed));
			FineSum += S.Speed;
			N++;
		}
		const double Mean = Sum / Looks, FineMean = FineSum / N;
		const bool bOk = std::fabs(O.MinSpeed - Lo) < 1e-4 && std::fabs(O.MaxSpeed - Hi) < 1e-4 && std::fabs(O.MeanSpeed - Mean) < 1e-4 && std::fabs(O.GustSpeed - Gust) < 1e-4 &&
			O.MinSpeed <= O.MeanSpeed && O.MeanSpeed <= O.MaxSpeed && O.GustSpeed >= O.MaxSpeed && O.MinSpeed >= FineLo - 0.03 * FineMean - 0.02 && O.MaxSpeed <= FineHi + 0.03 * FineMean + 0.02 &&
			std::fabs(O.MeanSpeed - FineMean) < 0.08 * FineMean + 0.05 && O.Force == Beaufort(O.MeanSpeed) && O.ForceMax == Beaufort(O.GustSpeed) && O.ForceMax >= O.Force &&
			(Mean < 0.5 || std::fabs(AngleOff(O.FromDeg, BearingFrom(float(VX), float(VY)))) < 0.05f);
		Bad += bOk ? 0 : 1;
		const FWindState A = Regime(H, P), B = Regime(H + Span, P);
		TurnBad += std::fabs(AngleOff(0.f, AngleOff(A.FromDeg, B.FromDeg) - O.TurnDeg)) < 0.5f ? 0 : 1;
		// the trend has the sign of end minus start whenever that is a clear change
		const double Change = (B.Speed - A.Speed) / Span;
		if (std::fabs(B.Speed - A.Speed) > 2.5)
		{
			TrendBad += (O.Trend > 0.f) == (Change > 0.0) ? 0 : 1;
		}
	}
	Check(Bad == 0, "Outlook: min, mean, max, gust, bearing, forces are the ten-minute looks', inside a finer scan's (misses of 200)", Bad);
	Check(TurnBad == 0, "Outlook: TurnDeg is how the regime's bearing ends against how it starts (misses)", TurnBad);
	Check(TrendBad == 0, "Outlook: the trend freshens or eases as the span's ends do when they differ by 2.5 m/s (misses)", TrendBad);
	const FWindOutlook One = Outlook(HoursSince2000(2026, 10, 20, 16.0), 0.0, P);
	const FWindState S = Now(HoursSince2000(2026, 10, 20, 16.0), P);
	Check(One.MinSpeed == S.Speed && One.MaxSpeed == S.Speed && One.Trend == 0.f && One.TurnDeg == 0.f && std::fabs(AngleOff(One.FromDeg, S.FromDeg)) < 0.01f,
		"Outlook: a span of nothing is the wind now (scene start, m/s)", One.MeanSpeed);

	// NextSpell against a minute-by-minute scan
	int SpellBad = 0, Spells = 0, Nones = 0, Zero = 0;
	double WorstSpell = 0.0;
	for (int I = 0; I < 120; I++)
	{
		const double H = HoursSince2000(2026, 1, 1, 0.0) + Rnd.In(0.0, 365.0 * 24.0);
		const float Lo = float(Rnd.In(1.0, 7.0)), Hi = Lo + float(Rnd.In(1.5, 5.0));
		const double Hold = Rnd.In(0.5, 3.0), Within = Rnd.In(6.0, 48.0);
		const bool bDay = I % 3 != 0;
		const double Got = NextSpell(H, P, Lo, Hi, Hold, Within, bDay);
		// by hand: the first minute from which every minute for Hold hours is good
		double Want = -1.0, RunStart = -1.0;
		for (double A = 0.0; A <= Within + Hold + 0.2; A += 1.0 / 60.0)
		{
			const FWindState R = Regime(H + A, P);
			const bool bGood = R.Speed >= Lo && R.Speed <= Hi && (!bDay || R.Day > 0.f);
			if (!bGood)
			{
				RunStart = -1.0;
				continue;
			}
			if (RunStart < 0.0)
			{
				RunStart = A;
			}
			if (A - RunStart >= Hold)
			{
				Want = RunStart;
				break;
			}
		}
		if (Want > Within)
		{
			Want = -1.0;
		}
		// spells that begin right at the look-ahead's end, or last just the hold, can fall either way of a 5 minute step
		const bool bEdge = (Want < 0.0) != (Got < 0.0);
		if (bEdge)
		{
			// accept only when the fine scan nearly has it (a run within ten minutes of long enough, or starting at the limit)
			double Longest = 0.0, Run = -1.0;
			bool bNear = false;
			for (double A = 0.0; A <= Within + Hold + 0.4; A += 1.0 / 60.0)
			{
				const FWindState R = Regime(H + A, P);
				const bool bGood = R.Speed >= Lo && R.Speed <= Hi && (!bDay || R.Day > 0.f);
				if (bGood && Run < 0.0)
				{
					Run = A;
				}
				if (bGood && Run >= 0.0 && Run <= Within + 0.2)
				{
					Longest = std::max(Longest, A - Run);
				}
				if (!bGood)
				{
					Run = -1.0;
				}
			}
			bNear = Longest > Hold - 0.2;
			SpellBad += bNear ? 0 : 1;
			continue;
		}
		if (Want < 0.0)
		{
			Nones++;
			continue;
		}
		Spells++;
		Zero += Want == 0.0 ? 1 : 0;
		WorstSpell = std::max(WorstSpell, std::fabs(Got - Want));
		SpellBad += std::fabs(Got - Want) < 0.12 ? 0 : 1;
	}
	std::printf("       NextSpell over 120 random asks: %d spells (%d already on), %d none; worst miss %.3f h\n", Spells, Zero, Nones, WorstSpell);
	Check(SpellBad == 0 && Spells > 30, "NextSpell: starts where a minute-by-minute scan finds the spell, within 7 minutes (misses)", SpellBad);
	const double H0 = HoursSince2000(2026, 10, 20, 16.0);
	Check(NextSpell(H0, P, 100.f, 200.f) == -1.0, "NextSpell: a wind that never comes is -1", NextSpell(H0, P, 100.f, 200.f));
	const FWindState R0 = Regime(H0, P);
	Check(NextSpell(H0, P, R0.Speed - 2.f, R0.Speed + 2.f, 0.25) == 0.0, "NextSpell: a spell that is on now is 0", NextSpell(H0, P, R0.Speed - 2.f, R0.Speed + 2.f, 0.25));
	const double Night = NextSpell(HoursSince2000(2026, 10, 20, 22.0), P, 0.f, 50.f, 1.0, 48.0, true);
	Check(Night > 6.0 && Night < 13.0 && NextSpell(HoursSince2000(2026, 10, 20, 22.0), P, 0.f, 50.f, 1.0, 48.0, false) == 0.0,
		"NextSpell: by daylight only, any wind at 22:00 waits for the morning (hours)", Night);
}

static double Clock()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static void TestCost(const FWindTerrain& T, const char* Which, double XA, double XB, double YA, double YB)
{
	FRand Rnd(21);
	const int N = 1 << 16;
	std::vector<double> Xs(N), Ys(N), Zs(N);
	for (int I = 0; I < N; I++)
	{
		Xs[I] = Rnd.In(XA, XB);
		Ys[I] = Rnd.In(YA, YB);
		Zs[I] = Rnd.In(-40.0, 60.0);
	}
	const FWindState S = Wind(250.f, 8.f);
	double Sum = 0.0;
	const int Rounds = 16;
	double Bare = 1e9, Land = 1e9, Gust = 1e9;
	// the best of a few rounds: other agents' builds share this machine
	for (int Try = 0; Try < 5; Try++)
	{
		double T0 = Clock();
		for (int R = 0; R < Rounds; R++)
		{
			for (int I = 0; I < N; I++)
			{
				Sum += GustAt(S, Xs[I], Ys[I], 1.0 * R, 2.0, 3.0).Factor;
			}
		}
		Gust = std::min(Gust, (Clock() - T0) / (double(Rounds) * N));
		T0 = Clock();
		for (int R = 0; R < Rounds; R++)
		{
			for (int I = 0; I < N; I++)
			{
				Sum += LocalWindAt(S, nullptr, Xs[I], Ys[I], Zs[I] + 45.0, Datum, 1.0 * R, 2.0, 3.0).Speed;
			}
		}
		Bare = std::min(Bare, (Clock() - T0) / (double(Rounds) * N));
		T0 = Clock();
		for (int R = 0; R < Rounds; R++)
		{
			for (int I = 0; I < N; I++)
			{
				Sum += LocalWindAt(S, &T, Xs[I], Ys[I], Zs[I], Datum, 1.0 * R, 2.0, 3.0).Speed;
			}
		}
		Land = std::min(Land, (Clock() - T0) / (double(Rounds) * N));
	}
	const FWindParams P = GameParams();
	const double H = HoursSince2000(2026, 7, 1, 0.0);
	double T0 = Clock();
	for (int I = 0; I < 20000; I++)
	{
		Sum += ShiftAt(H + I * 0.01, Regime(H + I * 0.01, P), P.Seed).VeerDeg;
	}
	const double Shift = (Clock() - T0) / 20000.0;
	T0 = Clock();
	for (int I = 0; I < 20000; I++)
	{
		Sum += Regime(H + I * 0.01, P).Speed;
	}
	const double Reg = (Clock() - T0) / 20000.0;
	const FWindState S9 = Wind(250.f, 9.f, 0.8f);
	T0 = Clock();
	for (int I = 0; I < 2000; I++)
	{
		Sum += NextGust(S9, Xs[I], Ys[I], 0, 0, 0).InSeconds;
	}
	const double Next = (Clock() - T0) / 2000.0;
	std::printf("       cost per call (%s, random points; sum %.0f): GustAt alone %.0f ns, LocalWindAt without terrain %.0f ns, with terrain %.0f ns;\n"
		"       Regime %.2f us, Regime + ShiftAt %.2f us (once a frame), NextGust %.1f us (on demand)\n",
		Which, Sum, Gust * 1e9, Bare * 1e9, Land * 1e9, Reg * 1e6, Shift * 1e6, Next * 1e6);
	// against GustAt too (the four noises every wind query has always paid for), so that a busy machine does not fail it
	Check(Bare < std::max(1e-6, 1.5 * Gust), "cost: LocalWindAt without a terrain is well under a microsecond (ns)", Bare * 1e9);
	Check(Land < std::max(2e-6, 4.0 * Gust), "cost: LocalWindAt with the terrain is under two microseconds (ns)", Land * 1e9);
	Check(Land - Gust < 1e-6, "cost: the march over the terrain itself, on top of the gust noise (ns)", (Land - Gust) * 1e9);
}

// ---------------------------------------------------------------------------------------------------- the looks

static void WriteMaps(const FWindTerrain& T, double MapY, float FromDeg, float Speed, double Above)
{
	// 8 m a pixel over the grid and 1.6 km of sea west of it; north up, the sea on the left (as on the game's paper map)
	const double Px = 8.0, Sea = 1600.0;
	const double XA = T.X0, XB = T.X0 + (T.NX - 1) * T.Cell + Sea, YA = T.Y0, YB = T.Y0 + (T.NY - 1) * T.Cell;
	const int W = int((XB - XA) / Px), H = int((YB - YA) / Px);
	std::vector<unsigned char> Exposure(size_t(W) * H), Lift(size_t(W) * H * 3);
	const FWindState S = Wind(FromDeg, Speed, 0.f);
	FWindState Still = S;
	Still.Gustiness = 0.f;
	double MaxUp = 0.0, MaxDown = 0.0;
	for (int J = 0; J < H; J++)
	{
		for (int I = 0; I < W; I++)
		{
			const double X = XB - (I + 0.5) * Px, Y = YB - (J + 0.5) * Px;
			const FLocalWind L = At(Still, &T, X, Y, Above);
			// the speed against the open sea's at this height: black dead calm, mid grey open sea, white a crest
			const float Ratio = L.Speed / (Speed * HeightFactor(float(std::max(Above, 0.3)), true));
			Exposure[size_t(J) * W + I] = (unsigned char)(std::min(std::max(Ratio / 1.5f, 0.f), 1.f) * 255.f);
			const float Up = L.Updraft / Speed;
			MaxUp = std::max(MaxUp, double(L.Updraft));
			MaxDown = std::min(MaxDown, double(L.Updraft));
			// red rising, blue sinking, on the land's grey
			const float Shade = T.GroundAt(X, Y) < Datum ? 40.f : 90.f + std::min(std::max((T.GroundAt(X, Y) - Datum) * 0.8f, 0.f), 80.f);
			unsigned char* Pix = &Lift[(size_t(J) * W + I) * 3];
			Pix[0] = (unsigned char)std::min(255.f, Shade + std::max(Up, 0.f) * 400.f);
			Pix[1] = (unsigned char)std::max(0.f, Shade - std::fabs(Up) * 250.f);
			Pix[2] = (unsigned char)std::min(255.f, Shade + std::max(-Up, 0.f) * 1500.f);
		}
	}
	char Path[256];
	std::snprintf(Path, sizeof(Path), "Saved/Wind/exposure_%03d_%g_%gm.pgm", int(FromDeg), double(Speed), Above);
	if (FILE* F = std::fopen(Path, "wb"))
	{
		std::fprintf(F, "P5\n%d %d\n255\n", W, H);
		std::fwrite(Exposure.data(), 1, Exposure.size(), F);
		std::fclose(F);
		std::printf("%s: %d x %d, %g m a pixel, north up, sea on the left; speed over the open sea's at %g m: black 0, grey (170) 1, white 1.5\n", Path, W, H, Px, Above);
	}
	std::snprintf(Path, sizeof(Path), "Saved/Wind/updraft_%03d_%g_%gm.ppm", int(FromDeg), double(Speed), Above);
	if (FILE* F = std::fopen(Path, "wb"))
	{
		std::fprintf(F, "P6\n%d %d\n255\n", W, H);
		std::fwrite(Lift.data(), 1, Lift.size(), F);
		std::fclose(F);
		std::printf("%s: red rising (strongest %.2f m/s), blue sinking (%.2f m/s)\n", Path, MaxUp, MaxDown);
	}
	std::printf("(map Y = world Y - %.1f)\n", MapY);
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "test";
	auto Arg = [&](int I, double Default) { return Argc > I ? std::atof(Argv[I]) : Default; };
	FWindTerrain Real, Built;
	double MapY = 7.9;
	const bool bReal = LoadTerrain("Saved/Wind/terrain.bin", Real, MapY);
	Synth::Build(Built);
	const FWindParams P = GameParams();

	if (Mode == "map")
	{
		if (!bReal)
		{
			std::printf("no Saved/Wind/terrain.bin (python3 tools/windsim/terrain.py): the built-in coast is drawn instead\n");
		}
		WriteMaps(bReal ? Real : Built, bReal ? MapY : 0.0, float(Arg(2, 270.0)), float(Arg(3, 8.0)), Arg(4, 10.0));
		return 0;
	}
	if (Mode == "at")
	{
		// map metres and metres over the ground or the water there; the hour of the calendar (default: the scene's start)
		const double X = Arg(2, 0.0), Y = Arg(3, 0.0) + MapY, Above = Arg(4, 10.0);
		const double H = Argc > 5 ? HoursSince2000(2026, 10, 20, 0.0) + Arg(5, 16.0) : HoursSince2000(2026, 10, 20, 16.0);
		const FWindState R = Regime(H, P);
		const FWindShift Sh = ShiftAt(H, R, P.Seed);
		const FWindState S = Shifted(R, Sh);
		const FWindTerrain* T = bReal ? &Real : nullptr;
		std::printf("regime: %.2f m/s from %.1f (%s), gustiness %.2f, day %.2f\n", R.Speed, R.FromDeg, CompassKey(R.FromDeg, 16), R.Gustiness, R.Day);
		std::printf("shift:  veer %+.1f of %.1f deg, factor %.3f, swing %.1f min, phase %.2f, next turn in %.1f min -> %.2f m/s from %.1f, force %d %s, %.1f kn\n", Sh.VeerDeg, Sh.RangeDeg,
			Sh.Factor, Sh.PeriodMin, Sh.Phase, Sh.NextTurnMin, S.Speed, S.FromDeg, Beaufort(S.Speed), BeaufortKey(Beaufort(S.Speed)), ToKnots(S.Speed));
		if (T)
		{
			std::printf("ground %.1f m, cover %.1f m, cell %d\n", T->GroundAt(X, Y), T->CoverAt(X, Y), T->CellAt(X, Y));
		}
		else
		{
			std::printf("(no terrain.bin: open flat land)\n");
		}
		PrintWind("here", At(S, T, X, Y, Above));
		const FGustEvent E = NextGust(S, X, Y, 0, 0, 0);
		if (E.InSeconds < 0.f)
		{
			std::printf("no gust or lull worth a word within a minute\n");
		}
		else
		{
			std::printf("next %s: in %.0f s, factor %.2f, for %.0f s\n", E.bLull ? "lull" : "gust", E.InSeconds, E.Factor, E.LastsSeconds);
		}
		return 0;
	}
	if (Mode == "line")
	{
		// a cut along X at a map Y: line <mapY> <fromDeg> <speed> [above] [x0 x1 step]
		const double Y = Arg(2, -300.0) + MapY, Above = Arg(5, 2.0);
		const FWindState S = Wind(float(Arg(3, 270.0)), float(Arg(4, 8.0)));
		const FWindTerrain& T = bReal ? Real : Built;
		std::printf("     X   ground cover   speed  from  expos shelt   up    turb  fetch\n");
		for (double X = Arg(6, -400.0); X <= Arg(7, 800.0); X += Arg(8, 20.0))
		{
			const FLocalWind L = Steady(S, &T, X, Y, Above);
			std::printf("%6.0f  %6.1f %5.1f  %6.2f %5.1f  %5.2f %5.2f %+5.2f  %5.2f %6.0f %s\n", X, T.GroundAt(X, Y), T.CoverAt(X, Y), L.Speed, L.FromDeg, L.Exposure, L.Shelter,
				L.Updraft, L.Turbulence, L.FetchM, L.bOverWater ? "w" : "");
		}
		return 0;
	}
	if (Mode == "day")
	{
		const int Y = int(Arg(2, 2026)), M = int(Arg(3, 10)), D = int(Arg(4, 20));
		std::printf("%04d-%02d-%02d (seed %u)\n hour  regime        now           veer/range  factor swing  turn in  force              next 3 h: mean  gust  turn  trend\n", Y, M, D, P.Seed);
		for (int Hour = 0; Hour < 24; Hour++)
		{
			const double H = HoursSince2000(Y, M, D, Hour);
			const FWindState R = Regime(H, P);
			const FWindShift Sh = ShiftAt(H, R, P.Seed);
			const FWindState S = Shifted(R, Sh);
			const FWindOutlook O = Outlook(H, 3.0, P);
			std::printf("%5d  %5.2f %-4s  %5.2f %5.1f  %+5.1f/%4.1f  %5.3f %5.1f  %6.1f  %d %-15s  %14.2f %5.2f %+5.0f %+6.2f\n", Hour, R.Speed, CompassKey(R.FromDeg, 16), S.Speed,
				S.FromDeg, Sh.VeerDeg, Sh.RangeDeg, Sh.Factor, Sh.PeriodMin, Sh.NextTurnMin, Beaufort(S.Speed), BeaufortKey(Beaufort(S.Speed)), O.MeanSpeed, O.GustSpeed, O.TurnDeg,
				O.Trend);
		}
		return 0;
	}
	if (Mode == "spell")
	{
		// the spells of a wind within min..max m/s holding an hour by daylight, over two weeks from a date
		const float Lo = float(Arg(2, 3.0)), Hi = float(Arg(3, 8.0));
		double H = HoursSince2000(int(Arg(4, 2026)), int(Arg(5, 10)), int(Arg(6, 20)), 0.0);
		const double End = H + 14.0 * 24.0, Start = H;
		std::printf("spells of %.1f..%.1f m/s holding an hour by daylight, two weeks from day 0:\n", Lo, Hi);
		while (H < End)
		{
			const double In = NextSpell(H, P, Lo, Hi, 1.0, End - H, true);
			if (In < 0.0)
			{
				break;
			}
			double To = H + In;
			FWindState R = Regime(To, P);
			double Sum = 0.0;
			int N = 0;
			while (R.Speed >= Lo && R.Speed <= Hi && R.Day > 0.f && To < End)
			{
				Sum += R.Speed;
				N++;
				To += 1.0 / 12.0;
				R = Regime(To, P);
			}
			const double A = H + In - Start;
			std::printf("  day %2d %05.2f h  for %4.1f h  mean %.1f m/s from %s\n", int(A / 24.0), std::fmod(A, 24.0), To - H - In, Sum / std::max(N, 1),
				CompassKey(Regime(H + In, P).FromDeg, 16));
			H = To + 1.0 / 12.0;
		}
		return 0;
	}
	if (Mode != "test")
	{
		std::printf("modes: test | map <fromDeg> <speed> [above] | at <mapX> <mapY> [above] [hour] | line <mapY> <fromDeg> <speed> [above] [x0 x1 step] | day y m d | spell min max [y m d]\n");
		return 2;
	}

	TestNoTerrain();
	TestGrid();
	TestShifts();
	TestSynthetic(Built);
	TestContinuity(Built, "built-in coast", -600.0, 700.0, -600.0, 600.0);
	if (bReal)
	{
		std::printf("     the real map: %d x %d cells of %g m (Saved/Wind/terrain.bin), world Y = map Y + %.1f\n", Real.NX, Real.NY, Real.Cell, MapY);
		TestRealMap(Real, MapY);
		TestContinuity(Real, "real map", -1300.0, 1500.0, -900.0 + MapY, 4200.0 + MapY);
	}
	else
	{
		std::printf("     (no Saved/Wind/terrain.bin: python3 or tools/world_map.py is missing; the real map's checks are skipped)\n");
	}
	TestAhead();
	TestWords();
	TestOutlook();
	TestCost(bReal ? Real : Built, bReal ? "real map" : "built-in coast", -1300.0, 1500.0, -900.0, 4200.0);
	std::printf("%d passed, %d failed\n", Passed, Failures);
	return Failures ? 1 : 0;
}
