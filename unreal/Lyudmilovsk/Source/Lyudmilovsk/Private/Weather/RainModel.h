// Clouds, rain, snow, thunder, fog and the air's temperature over the outdoor map, in plain C++ (no Unreal types) so it
// can be run and looked at outside the engine (tools/audiosim/run.sh weather). ULyudWeatherSubsystem wraps it.
//
// Like the wind (WindModel.h) it is a function of the local date and hour, not a simulation, and it leans on the
// wind's weather: the fronts that bring a fresh wind bring the cloud and the rain with them, so a gale is grey and wet
// and a calm is mostly clear. On top of that:
//   - the season: the Kaliningrad coast's climate (mean temperature -2 C in January .. 18 C in July, rain on about a
//     third of the days, wettest from July to October, summer showers and thunder, snow when it is cold enough);
//   - the day: warmer afternoons, cooler nights (less of both under cloud), mist on calm cold clear mornings;
//   - showers: patches of rain an hour or two long inside a cloudy spell, heavier in summer.
#pragma once

#include <cstdint>

namespace LyudWeather
{
	struct FWindParams;

	struct FSkyParams
	{
		/** Picks the sequence of days (the wind has its own seed; this one only decides the rain inside its fronts; 2660 = broken cloud and dry on the scene's start, rain the next days). */
		uint32_t Seed = 2660;
		/** How much more often than the climate it rains (1 = the Baltic coast; 0 = never). */
		double Wetness = 1.0;
		/** Monthly mean temperature: the yearly cosine's mean and half swing, C, and the coldest day of the year. */
		double MeanTempC = 8.0, YearSwingC = 10.0, ColdestDay = 20.0;
		/** Day-night half swing on a clear day, C. */
		double DaySwingC = 4.0;
	};

	/** The sky at one moment, the same over the whole map. */
	struct FSkyState
	{
		float Cloud = 0.f;      // 0 clear .. 1 overcast
		float Rain = 0.f;       // falling water, mm/h (drizzle 0.3, rain 2, a downpour 10+); snow counts as its water
		float Snow = 0.f;       // 0 rain .. 1 all of it falls as snow
		float Thunder = 0.f;    // 0 .. 1 how stormy a shower is (strikes per minute ~ 2 x this)
		float Fog = 0.f;        // 0 .. 1 mist (visibility from ~10 km down to ~200 m)
		float TempC = 0.f;      // air temperature at 2 m
		float Wet = 0.f;        // 0 dry .. 1 soaked ground (follows the rain, dries over hours)
	};

	/** The sky at Hours (local hours since 2000-01-01) under the wind of Wind (its fronts carry the cloud). */
	FSkyState Sky(double Hours, const FWindParams& Wind, const FSkyParams& P);

	/** 1-based day of the year of Hours. */
	double DayOfYearOf(double Hours);
}
