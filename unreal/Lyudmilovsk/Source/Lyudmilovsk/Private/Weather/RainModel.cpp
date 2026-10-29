#include "RainModel.h"
#include "WeatherModel.h"
#include "WindModel.h"

#include <cmath>

// The old short summary, now read from the weather engine (WeatherModel.cpp) with this seed and wetness.
namespace LyudWeather
{
	double DayOfYearOf(double Hours)
	{
		return std::fmod(Hours / 24.0, 365.2425) + 1.0;
	}

	FSkyState Sky(double Hours, const FWindParams& W, const FSkyParams& P)
	{
		(void)W;
		thread_local FChain* Chain = nullptr;
		thread_local FSeason* Season = nullptr;
		FClimate C = GetClimate();
		C.Seed = P.Seed;
		C.Wetness = P.Wetness;
		if (!Chain || Chain->Seed() != C.Seed || Chain->GetClimate().Wetness != C.Wetness)
		{
			delete Chain;
			delete Season;
			Chain = new FChain(C);
			Season = new FSeason();
		}
		const FWeatherState S = Full(Hours, *Chain, *Season);
		FSkyState O;
		O.Cloud = S.CloudCover;
		O.Rain = S.PrecipRate;
		O.Snow = S.SnowFraction;
		O.Thunder = S.Thunder;
		O.Fog = S.Fog;
		O.TempC = S.TempC;
		O.Wet = S.Wet;
		return O;
	}
}
