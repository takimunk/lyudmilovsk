// Prints the weather model's climate (tools/audiosim/run.sh weather): per month how often it rains, how much, the
// temperatures, fog and thunder, and the days around the scene's start. Checks it against the Kaliningrad coast's.
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Weather/RainModel.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Weather/WindModel.h"

#include <cstdio>
#include <cstring>

using namespace LyudWeather;

int WeatherMain(int Argc, char** Argv)
{
	FWindParams W;
	W.Seed = 921;   // ULyudWindSubsystem's default (FLyudWindSettings::Seed)
	FSkyParams P;
	const char* Names[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	const int Days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	std::printf("month  rain%%time  mm/month  wetdays  snow%%  thunderh  fog%%  cloud  Tmin  Tmean  Tmax\n");
	double YearMm = 0;
	for (int Y = 2026; Y <= 2028; Y++)
	{
		for (int M = 0; M < 12; M++)
		{
			double RainT = 0, Mm = 0, SnowT = 0, ThunderH = 0, FogT = 0, Cloud = 0, T = 0, TMin = 99, TMax = -99;
			int WetDays = 0, N = 0;
			for (int D = 1; D <= Days[M]; D++)
			{
				double DayMm = 0;
				for (int H = 0; H < 24; H++)
				{
					const FSkyState S = Sky(HoursSince2000(Y, M + 1, D, H + 0.5), W, P);
					RainT += S.Rain > 0.1f;
					Mm += S.Rain;
					DayMm += S.Rain;
					SnowT += S.Rain > 0.1f && S.Snow > 0.5f;
					ThunderH += S.Thunder > 0.3f;
					FogT += S.Fog > 0.5f;
					Cloud += S.Cloud;
					T += S.TempC;
					TMin = S.TempC < TMin ? S.TempC : TMin;
					TMax = S.TempC > TMax ? S.TempC : TMax;
					N++;
				}
				WetDays += DayMm >= 1.0;
			}
			if (Y == 2027)
			{
				YearMm += Mm;
				std::printf("%s    %5.1f     %6.1f    %4d    %5.1f   %5.1f   %5.1f  %.2f  %5.1f %5.1f %5.1f\n", Names[M], 100 * RainT / N, Mm, WetDays,
					100 * SnowT / N, ThunderH, 100 * FogT / N, Cloud / N, TMin, T / N, TMax);
			}
		}
	}
	std::printf("year 2027: %.0f mm (Kaliningrad ~800 mm, Jul-Oct wettest; mean T Jan -1..-2, Jul ~18)\n", YearMm);
	std::printf("\n2026-10-20..23 hourly (the scene starts 20 Oct 16:00):\n");
	for (int D = 20; D <= 23; D++)
	{
		for (int H = 0; H < 24; H += 2)
		{
			const FSkyState S = Sky(HoursSince2000(2026, 10, D, H), W, P);
			const FWindState Wd = Regime(HoursSince2000(2026, 10, D, H), W);
			std::printf("  %02d %02d:00 wind %4.1f cloud %.2f rain %5.2f wet %.2f fog %.2f T %5.1f\n", D, H, Wd.Speed, S.Cloud, S.Rain, S.Wet, S.Fog, S.TempC);
		}
	}
	return 0;
}
