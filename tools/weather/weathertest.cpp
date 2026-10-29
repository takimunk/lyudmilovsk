// The weather engine outside Unreal (tools/weather/run.sh): its climate per month, the days round a date, the
// winters, and checks against the Kaliningrad coast's climate and the engine's own laws.
#include "WeatherModel.h"
#include "RainModel.h"
#include "WindModel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace LyudWeather;

static int Failures = 0;
static void Check(bool Ok, const char* What, double Value)
{
	std::printf("%s %-64s %.3f\n", Ok ? "  ok " : "FAIL ", What, Value);
	if (!Ok) Failures++;
}

static FClimate ClimateFor(int Argc, char** Argv, int SeedArg)
{
	FClimate C;
	if (Argc > SeedArg) C.Seed = uint32_t(std::atoi(Argv[SeedArg]));
	SetClimate(C);
	return C;
}

struct FMonth
{
	double T = 0, Tmin = 1e9, Tmax = -1e9, Precip = 0, WetH = 0, SnowH = 0, ThunderH = 0, FogH = 0, Cloud = 0, Clear = 0, Overcast = 0, Wind = 0, N = 0;
	double SnowDepth = 0, SnowDays = 0, IceH = 0, AuroraH = 0, LensH = 0, HaloH = 0, RainbowH = 0, NlcH = 0, FrzH = 0, Sst = 0;
	double Regime[8] = {};
};

static std::vector<FMonth> Climate(const FClimate& C, int Years)
{
	std::vector<FMonth> M(12);
	FChain Chain(C);
	FSeason Season;
	const double H0 = HoursSince2000(2026, 9, 1, 0.0);
	for (double H = 0; H < Years * 365.2425 * 24; H += 1.0)
	{
		const FWeatherState S = Full(H0 + H, Chain, Season);
		const int Mo = std::min(11, int((DayOfYearOf(H0 + H) - 1) / 30.44));
		FMonth& X = M[Mo];
		const bool bDark = SunElevationDeg(H0 + H, C) < -12;
		X.T += S.TempC; X.Tmin = std::min(X.Tmin, double(S.TempC)); X.Tmax = std::max(X.Tmax, double(S.TempC));
		X.Precip += S.PrecipRate; X.WetH += S.PrecipRate > 0.1; X.SnowH += S.PrecipRate > 0.1 && S.SnowFraction > 0.5;
		X.ThunderH += S.Thunder > 0.2; X.FogH += S.VisibilityM < 1000; X.Cloud += S.CloudCover; X.Clear += S.CloudCover < 0.2; X.Overcast += S.CloudCover > 0.85;
		X.Wind += S.WindSpeed; X.N++; X.SnowDepth += S.SnowDepthCm; X.SnowDays += S.SnowDepthCm > 1; X.IceH += S.SeaIceCover > 0.5;
		X.AuroraH += S.Aurora > 0.1 && bDark;
		X.LensH += S.Lenticular > 0.3; X.HaloH += S.Halo > 0.4; X.RainbowH += S.Rainbow > 0.3; X.NlcH += S.Noctilucent > 0.3;
		X.FrzH += S.Precip == EPrecip::FreezingRain || S.Precip == EPrecip::FreezingDrizzle;
		X.Sst += S.SeaTempC;
		X.Regime[int(S.Regime)]++;
	}
	return M;
}

static void PrintClimate(const std::vector<FMonth>& M, int Years)
{
	std::printf("mon  T(C)  min   max   mm/mo wet%% snow%% thund fog%% cloud clr%% ovc%% wind snowcm snowd ice%% sst  aurora-h lens-h halo-h rainbow-h nlc-h frz-h | High WF WS CF Sh Oc AH TL %%\n");
	double Year = 0;
	for (int I = 0; I < 12; I++)
	{
		const FMonth& X = M[I];
		const double N = X.N;
		const double Mm = X.Precip / Years;
		Year += Mm;
		std::printf("%3d %5.1f %5.1f %5.1f %6.0f %4.0f %5.0f %5.1f %4.1f %5.2f %4.0f %4.0f %4.1f %6.1f %5.0f %4.0f %4.1f %7.1f %6.1f %6.1f %8.1f %5.1f %5.1f |",
			I + 1, X.T / N, X.Tmin, X.Tmax, Mm, 100 * X.WetH / N, 100 * X.SnowH / N, X.ThunderH / Years, 100 * X.FogH / N, X.Cloud / N,
			100 * X.Clear / N, 100 * X.Overcast / N, X.Wind / N, X.SnowDepth / N, X.SnowDays / 24 / Years, 100 * X.IceH / N, X.Sst / N,
			X.AuroraH / Years, X.LensH / Years, X.HaloH / Years, X.RainbowH / Years, X.NlcH / Years, X.FrzH / Years);
		for (int R = 0; R < 8; R++) std::printf(" %2.0f", 100 * X.Regime[R] / N);
		std::printf("\n");
	}
	std::printf("year precipitation %.0f mm\n", Year);
}

static void PrintDays(const FClimate& C, int Y, int Mo, int D, double Days)
{
	FChain Chain(C);
	FSeason Season;
	const double H0 = HoursSince2000(Y, Mo, D, 0.0);
	FILE* Csv = std::fopen("Saved/Weather/days.csv", "w");
	if (Csv) std::fprintf(Csv, "hours,regime,temp,dew,rh,hpa,wind_from,wind,cloud,low,mid,high,precip,rate,snowfrac,thunder,fog,fogtype,vis,wet,snow_cm,ice_cm,sst,kp,aurora,lens,halo\n");
	for (double H = 0; H < Days * 24; H += 1.0)
	{
		const FWeatherState S = Full(H0 + H, Chain, Season);
		if (int(H) % 3 == 0)
		{
			std::printf("%5.0f d%02d %02d:00 %-11s %3.0f%% %5.1fC td%5.1f %4.0fhPa %+4.1f wind %3.0f %4.1f | cloud %.2f %-5s%.2f@%4.0f %-5s%.2f %-3s%.2f | %-15s %5.2f thund %.2f | fog %-9s %.2f vis %5.0f | wet %.2f snow %4.1f ice %4.1f sst %4.1f kp %.1f lens %.2f halo %.2f\n",
				H, int(H / 24) + D, int(std::fmod(H, 24.0)), RegimeName(S.Regime), 100 * S.RegimeProgress, S.TempC, S.DewPointC, S.PressureHPa, S.PressureTendency,
				S.WindFromDeg, S.WindSpeed, S.CloudCover, CloudName(S.Low.Type), S.Low.Cover, S.Low.BaseM, CloudName(S.Mid.Type), S.Mid.Cover,
				CloudName(S.High.Type), S.High.Cover, PrecipName(S.Precip), S.PrecipRate, S.Thunder, FogName(S.FogType), S.Fog, S.VisibilityM,
				S.Wet, S.SnowDepthCm, S.SeaIceCm, S.SeaTempC, S.Kp, S.Lenticular, S.Halo);
		}
		if (Csv)
		{
			std::fprintf(Csv, "%.0f,%s,%.2f,%.2f,%.3f,%.1f,%.0f,%.2f,%.3f,%s,%s,%s,%s,%.3f,%.2f,%.2f,%.3f,%s,%.0f,%.3f,%.2f,%.2f,%.2f,%.2f,%.3f,%.3f,%.3f\n",
				H, RegimeName(S.Regime), S.TempC, S.DewPointC, S.Humidity, S.PressureHPa, S.WindFromDeg, S.WindSpeed, S.CloudCover,
				CloudName(S.Low.Type), CloudName(S.Mid.Type), CloudName(S.High.Type), PrecipName(S.Precip), S.PrecipRate, S.SnowFraction, S.Thunder,
				S.Fog, FogName(S.FogType), S.VisibilityM, S.Wet, S.SnowDepthCm, S.SeaIceCm, S.SeaTempC, S.Kp, S.Aurora, S.Lenticular, S.Halo);
		}
	}
	if (Csv) std::fclose(Csv);
}

static void Winters(const FClimate& C, int Years, int& IceWinters, int& DeepSnowWinters, bool bPrint)
{
	FChain Chain(C);
	IceWinters = DeepSnowWinters = 0;
	for (int Y = 2026; Y < 2026 + Years; Y++)
	{
		FSeason Season;
		double MaxSnow = 0, MaxIce = 0, MaxFdd = 0, MinT = 99, SnowDays = 0, IceDays = 0;
		const double H0 = HoursSince2000(Y, 9, 1, 0.0), H1 = HoursSince2000(Y + 1, 5, 1, 0.0);
		for (double H = H0; H < H1; H += 1.0)
		{
			const FWeatherState S = Full(H, Chain, Season);
			MaxSnow = std::max(MaxSnow, double(S.SnowDepthCm));
			MaxIce = std::max(MaxIce, double(S.SeaIceCm));
			MaxFdd = std::max(MaxFdd, double(S.FreezingDegreeDays));
			MinT = std::min(MinT, double(S.TempC));
			SnowDays += S.SnowDepthCm > 1 ? 1.0 / 24 : 0;
			IceDays += S.SeaIceCover > 0.5 ? 1.0 / 24 : 0;
		}
		IceWinters += MaxIce > 8;
		DeepSnowWinters += MaxSnow > 20;
		if (bPrint)
		{
			std::printf("winter %d/%02d: min %.1f C, FDD %.0f, snow max %.0f cm on %.0f days, sea ice max %.0f cm, frozen %.0f days\n",
				Y, (Y + 1) % 100, MinT, MaxFdd, MaxSnow, SnowDays, MaxIce, IceDays);
		}
	}
}

static int Test(const FClimate& C)
{
	// the scene's start: 2026-10-20 16:00, a breezy WSW afternoon with broken cloud, dry
	{
		FChain Chain(C);
		FSeason Season;
		const FWeatherState S = Full(HoursSince2000(2026, 10, 20, 16.0), Chain, Season);
		std::printf("scene start: %s %.1f C wind %.0f %.1f cloud %.2f rain %.2f\n", RegimeName(S.Regime), S.TempC, S.WindFromDeg, S.WindSpeed, S.CloudCover, S.PrecipRate);
		Check(S.PrecipRate < 0.05f, "scene start dry", S.PrecipRate);
		Check(S.CloudCover > 0.2f && S.CloudCover < 0.85f, "scene start broken cloud 0.2..0.85", S.CloudCover);
		Check(S.TempC > 4.f && S.TempC < 15.f, "scene start 4..15 C", S.TempC);
	}

	const int Years = 10;
	const std::vector<FMonth> M = Climate(C, Years);
	PrintClimate(M, Years);
	auto Mean = [&](int Mo) { return M[Mo].T / M[Mo].N; };
	double Year = 0, WetH = 0, N = 0, Fog = 0, Thunder = 0, Aurora = 0, Lens = 0, Halo = 0, Rainbow = 0, Nlc = 0;
	for (const FMonth& X : M) { Year += X.Precip / Years; WetH += X.WetH; N += X.N; Fog += X.FogH; Thunder += X.ThunderH / Years; Aurora += X.AuroraH / Years; Lens += X.LensH / Years; Halo += X.HaloH / Years; Rainbow += X.RainbowH / Years; Nlc += X.NlcH / Years; }
	Check(Mean(0) > -5.5 && Mean(0) < 0.0, "January mean -5.5..0 C", Mean(0));
	Check(Mean(6) > 15.5 && Mean(6) < 20.5, "July mean 15.5..20.5 C", Mean(6));
	Check(M[0].Tmin < -12.0, "a hard frost below -12 C in Januaries", M[0].Tmin);
	Check(M[6].Tmax > 27.0, "a hot day over 27 C in Julys", M[6].Tmax);
	Check(Year > 600 && Year < 1000, "precipitation 600..1000 mm a year", Year);
	Check(WetH / N > 0.07 && WetH / N < 0.22, "wet hours 7..22 %", 100 * WetH / N);
	Check(M[4].Precip < M[7].Precip, "May drier than August", M[4].Precip / Years);
	Check(M[4].Clear / M[4].N > M[10].Clear / M[10].N, "May clearer than November", 100 * M[4].Clear / M[4].N);
	Check(M[10].Overcast / M[10].N > 0.35, "November mostly grey (>35 % overcast hours)", 100 * M[10].Overcast / M[10].N);
	Check(M[0].SnowH > M[0].WetH * 0.4, "January: at least 40 % of wet hours snow", M[0].SnowH / std::max(M[0].WetH, 1.0));
	Check(M[6].SnowH == 0, "no snow in July", M[6].SnowH);
	Check(M[6].ThunderH / Years > M[0].ThunderH / Years + 3, "thunder in summer, not winter (Jul h/yr)", M[6].ThunderH / Years);
	Check(Thunder > 15 && Thunder < 150, "thunder hours a year 15..150", Thunder);
	Check(Fog / N > 0.005 && Fog / N < 0.08, "fog (vis < 1 km) 0.5..8 % of hours", 100 * Fog / N);
	Check(Aurora > 3 && Aurora < 80, "dark aurora hours a year 3..80", Aurora);
	Check(Lens > 5 && Lens < 200, "lenticular hours a year 5..200", Lens);
	Check(Halo > 20, "halo hours a year > 20", Halo);
	Check(Rainbow > 10, "rainbow hours a year > 10", Rainbow);
	Check(Nlc > 2 && M[0].NlcH == 0, "noctilucent clouds on summer nights only", Nlc);
	Check(M[1].SnowDepth / M[1].N > 3, "February mean snow cover > 3 cm", M[1].SnowDepth / M[1].N);
	Check(M[6].SnowDepth == 0, "no snow on the ground in July", M[6].SnowDepth);
	Check(M[7].Sst / M[7].N > 15 && M[1].Sst / M[1].N < 3, "sea 15+ C in August, under 3 C in February", M[7].Sst / M[7].N);
	double FrzH = 0; for (const FMonth& X : M) FrzH += X.FrzH / Years;
	Check(FrzH > 1 && FrzH < 60, "freezing rain hours a year 1..60", FrzH);
	double Regimes[8] = {}; for (const FMonth& X : M) for (int R = 0; R < 8; R++) Regimes[R] += X.Regime[R] / N;
	for (int R = 0; R < 8; R++) Check(Regimes[R] > 0.005, RegimeName(ERegime(R)), 100 * Regimes[R]);

	int IceW = 0, SnowW = 0;
	Winters(C, Years, IceW, SnowW, true);
	Check(IceW >= 4 && IceW <= 9, "sea ice (>8 cm) in 4..9 winters of 10", IceW);
	Check(SnowW >= 3, "deep snow (>20 cm level) in 3+ winters of 10", SnowW);

	// laws: continuity and the fronts' story
	{
		FChain Chain(C);
		FSeason Season;
		const double H0 = HoursSince2000(2026, 10, 1, 0.0);
		double MaxDT = 0, MaxDP = 0, MaxDW = 0, MaxDD = 0;
		FWeatherState A = Full(H0, Chain, Season);
		int Veers = 0, Fronts = 0, PressureJumps = 0;
		for (double H = 0.1; H < 90 * 24; H += 0.1)
		{
			const FWeatherState B = Full(H0 + H, Chain, Season);
			MaxDT = std::max(MaxDT, double(std::fabs(B.TempC - A.TempC)));
			MaxDP = std::max(MaxDP, double(std::fabs(B.PressureHPa - A.PressureHPa)));
			MaxDW = std::max(MaxDW, double(std::fabs(B.WindSpeed - A.WindSpeed)));
			double Dd = std::fabs(B.WindFromDeg - A.WindFromDeg); if (Dd > 180) Dd = 360 - Dd;
			if (A.WindSpeed > 2) MaxDD = std::max(MaxDD, Dd);
			if (A.Regime == ERegime::ColdFront && B.Regime == ERegime::Showery)
			{
				Fronts++;
				// over the front's passage: veered to W-NW and the pressure rising
				FSeason Tmp;
				const FWeatherState Before = Full(H0 + H - 4.0, Chain, Tmp);
				const FWeatherState After = Full(H0 + H + 3.0, Chain, Tmp);
				double V = After.WindFromDeg - Before.WindFromDeg; if (V < -180) V += 360; if (V > 180) V -= 360;
				Veers += V > 10;
				PressureJumps += After.PressureHPa > Before.PressureHPa;
			}
			A = B;
		}
		Check(MaxDT < 1.0, "temperature change per 6 min under 1 C", MaxDT);
		Check(MaxDP < 1.5, "pressure change per 6 min under 1.5 hPa", MaxDP);
		Check(MaxDW < 1.0, "weather wind change per 6 min under 1 m/s", MaxDW);
		Check(MaxDD < 10, "weather wind direction change per 6 min under 10 deg", MaxDD);
		Check(Fronts > 3, "cold fronts in 90 autumn days", Fronts);
		Check(Veers >= Fronts * 0.7, "the wind veers at most cold fronts", double(Veers) / std::max(Fronts, 1));
		Check(PressureJumps >= Fronts * 0.7, "pressure rises behind most cold fronts", double(PressureJumps) / std::max(Fronts, 1));
	}
	// deterministic: a fresh chain and a fresh season agree with a long-running one
	{
		FChain A(C), B(C);
		FSeason SA, SB;
		const double H = HoursSince2000(2027, 1, 15, 9.5);
		for (double X = HoursSince2000(2026, 9, 1, 0); X < H; X += 7.0) Full(X, A, SA);
		const FWeatherState P = Full(H, A, SA), Q = Full(H, B, SB);
		Check(std::fabs(P.TempC - Q.TempC) < 1e-4 && std::fabs(P.SnowDepthCm - Q.SnowDepthCm) < 0.2, "same moment, same weather (fresh vs long-run)", std::fabs(P.SnowDepthCm - Q.SnowDepthCm));
	}
	// the wind model follows the chain
	{
		FWindParams W;
		const double H = HoursSince2000(2026, 10, 20, 16.0);
		const FWindState S = Regime(H, W);
		std::printf("wind at the scene start: %.0f deg %.1f m/s\n", S.FromDeg, S.Speed);
		Check(S.FromDeg > 200 && S.FromDeg < 290, "scene start wind from SW..W", S.FromDeg);
		Check(S.Speed > 3.5 && S.Speed < 11, "scene start wind 3.5..11 m/s", S.Speed);
	}
	std::printf("%s (%d failures)\n", Failures ? "FAILED" : "PASSED", Failures);
	return Failures ? 1 : 0;
}

int main(int Argc, char** Argv)
{
	const char* Cmd = Argc > 1 ? Argv[1] : "test";
	if (!std::strcmp(Cmd, "climate"))
	{
		const FClimate C = ClimateFor(Argc, Argv, 2);
		PrintClimate(Climate(C, 10), 10);
		return 0;
	}
	if (!std::strcmp(Cmd, "days") && Argc >= 6)
	{
		const FClimate C = ClimateFor(Argc, Argv, 6);
		PrintDays(C, std::atoi(Argv[2]), std::atoi(Argv[3]), std::atoi(Argv[4]), std::atof(Argv[5]));
		return 0;
	}
	if (!std::strcmp(Cmd, "winters"))
	{
		const FClimate C = ClimateFor(Argc, Argv, 2);
		int A, B;
		Winters(C, 10, A, B, true);
		return 0;
	}
	if (!std::strcmp(Cmd, "seeds"))
	{
		// seeds whose scene start is dry broken cloud with a SW..W breeze
		for (uint32_t Seed = 1; Seed < 4000; Seed++)
		{
			FClimate C;
			C.Seed = Seed;
			FChain Chain(C);
			FSeason Season;
			const double H = HoursSince2000(2026, 10, 20, 16.0);
			const FWeatherState S = Full(H, Chain, Season);
			const FWeatherState T = Full(H + 24, Chain, Season);
			if (S.PrecipRate < 0.02f && S.CloudCover > 0.3f && S.CloudCover < 0.7f && S.WindFromDeg > 225 && S.WindFromDeg < 265 && S.WindSpeed > 5 && S.WindSpeed < 9 && T.PrecipRate > 0.1f)
			{
				std::printf("seed %u: %s %.1f C wind %.0f %.1f cloud %.2f; next day %s %.2f mm/h\n", Seed, RegimeName(S.Regime), S.TempC, S.WindFromDeg, S.WindSpeed, S.CloudCover, PrecipName(T.Precip), T.PrecipRate);
			}
		}
		return 0;
	}
	const FClimate C = ClimateFor(Argc, Argv, 2);
	return Test(C);
}
