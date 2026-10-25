#include "RainModel.h"
#include "WindModel.h"

#include <algorithm>
#include <cmath>

namespace LyudWeather
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		double Clamp01(double V) { return std::min(std::max(V, 0.0), 1.0); }
		double SmoothStep(double A, double B, double X)
		{
			const double T = Clamp01((X - A) / (B - A));
			return T * T * (3.0 - 2.0 * T);
		}

		/** The parts of the sky that need no history (Wet integrates them over the last hours). */
		FSkyState SkyNow(double Hours, const FWindParams& W, const FSkyParams& P)
		{
			FSkyState S;
			const FWindState Wind = Regime(Hours, W);
			const double Doy = DayOfYearOf(Hours);
			const double HourOfDay = Hours - std::floor(Hours / 24.0) * 24.0;
			const uint32_t Seed = P.Seed;

			// the season: 0 in late winter .. 1 in late summer and autumn, when the Baltic rains most
			const double WetSeason = 0.5 - 0.5 * std::cos(2.0 * Pi * (Doy - 60.0) / 365.25);
			const double Summer = Clamp01(0.5 - 0.5 * std::cos(2.0 * Pi * (Doy - 15.0) / 365.25));

			// cloud: the wind's fronts (its weather speed against the median) and a slower spell of our own
			const double Front = std::log(std::max<double>(Wind.Weather, 0.3) / std::max(W.MedianSpeed, 0.5)) / 0.6;
			const double Own = 0.65 * SmoothNoise1(Hours / 26.0, Seed) + 0.35 * SmoothNoise1(Hours / 7.0, Seed + 1);
			const double CloudN = 0.55 * Front + 0.75 * Own + 0.1 * (WetSeason - 0.5);
			S.Cloud = float(SmoothStep(-0.75, 0.55, CloudN));

			// rain: only under a thick enough deck; showers come and go inside it
			const double Shower = 0.5 + 0.5 * SmoothNoise1(Hours / 1.6, Seed + 2);
			const double Deck = SmoothStep(0.8, 0.99, S.Cloud);
			const double Chance = Deck * SmoothStep(0.62 - 0.1 * WetSeason, 0.95, Shower * P.Wetness);
			if (Chance > 0.0)
			{
				// steady frontal drizzle-to-rain, summer showers heavier
				const double Heavy = 0.5 + 0.5 * SmoothNoise1(Hours / 0.7, Seed + 3);
				S.Rain = float(Chance * (0.5 + 2.0 * Heavy * Heavy + 3.5 * Summer * std::pow(Heavy, 4.0)));
			}

			// temperature: the year, the day (damped under cloud) and a few degrees of weather either way
			const double Year = P.MeanTempC - P.YearSwingC * std::cos(2.0 * Pi * (Doy - P.ColdestDay) / 365.25);
			const double Daily = -std::cos(2.0 * Pi * (HourOfDay - 3.0) / 24.0) * P.DaySwingC * (1.0 - 0.6 * S.Cloud);
			const double Anomaly = 3.0 * SmoothNoise1(Hours / 60.0, Seed + 4);
			S.TempC = float(Year + Daily + Anomaly);
			S.Snow = float(SmoothStep(2.0, -0.5, S.TempC));

			// thunder: heavy warm showers
			S.Thunder = float(SmoothStep(4.0, 10.0, S.Rain) * SmoothStep(13.0, 20.0, S.TempC));

			// mist: calm, cold, clearing nights into the morning (rising after sunset, lifting a few hours after sunrise)
			const double Night = SmoothStep(19.0, 23.0, HourOfDay) + SmoothStep(10.5, 6.0, HourOfDay);
			const double Calm = SmoothStep(3.0, 0.8, Wind.Speed);
			const double MistChance = 0.5 + 0.5 * SmoothNoise1(Hours / 18.0, Seed + 5);
			S.Fog = float(Clamp01(Night * Calm * SmoothStep(0.45, 0.8, MistChance) * (1.0 - 0.5 * Summer)
				+ 0.3 * SmoothStep(0.5, 3.0, S.Rain) * (1.0 - S.Snow)));
			return S;
		}
	}

	double DayOfYearOf(double Hours)
	{
		return std::fmod(Hours / 24.0, 365.2425) + 1.0;
	}

	FSkyState Sky(double Hours, const FWindParams& W, const FSkyParams& P)
	{
		FSkyState S = SkyNow(Hours, W, P);
		// the ground: wetted by the last hours' rain, dried faster in warm windy weather
		double Water = 0.0;
		for (int K = 0; K <= 16; K++)
		{
			const double Ago = K * 0.5;
			const float R = K == 0 ? S.Rain : SkyNow(Hours - Ago, W, P).Rain;
			Water += R * std::exp(-Ago / (3.0 + 4.0 * SmoothStep(15.0, 2.0, S.TempC))) * 0.5;
		}
		S.Wet = float(Clamp01(Water / 3.0));
		return S;
	}
}
