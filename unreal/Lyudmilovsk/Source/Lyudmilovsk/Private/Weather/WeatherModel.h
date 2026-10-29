// The weather engine, in plain C++ (no Unreal types) so it can be run and checked outside the engine
// (tools/weather/run.sh). ULyudWeatherSubsystem wraps it; docs/weather.md explains the rules.
//
// It is still a function of the calendar, not a step-by-step simulation, so a save, a jump of the clock or a second
// machine see the same weather, and any moment can be forecast. What it adds over noise is a story with laws:
//   - synoptic episodes: a seeded Markov chain of weather types, played out from a fixed epoch (FChain), that follows
//     the life of the Atlantic cyclones over the Baltic: a high, then cirrus thickening ahead of a warm front, rain,
//     the mild damp warm sector, the cold front's squall and veer, showers and clearing behind it; occluded drizzly
//     lows, Scandinavian/Russian winter highs with hard frost, summer thundery lows. How often each comes and how deep
//     the cyclones are follows the month (stormy autumn, dry sunny May, thundery July).
//   - air masses: temperature and dew point from the month's climate, the episode's air mass, the day's sun
//     (damped by cloud and wind) and the sea; the cloud base from the dew-point spread (125 m per degree), the
//     freezing level from the lapse rate.
//   - clouds in three layers (low, middle, high), each with a genus, cover, base, top and thickness, following the
//     front's passage (Ci -> Cs -> As -> Ns ahead of a warm front, Cb along a cold front, Cu growing on summer
//     afternoons and over the warm sea in autumn), lenticulars in strong stable flow aloft.
//   - precipitation type from the air and the freezing level: drizzle, rain, showers, thunderstorms, hail, graupel,
//     sleet, snow, snow showers, freezing rain where a warm front slides over frozen ground.
//   - fog by its cause: radiation (clear calm nights), sea/advection (warm moist air over a cold sea), steam
//     (arctic air over open water) and frontal mist under rain.
//   - what the past leaves (FSeason, integrated hour by hour from 1 September): ground wetness, the snow pack (water,
//     depth, density, melt), frost in the ground, the sea's temperature and the sea ice from its freezing degree-days.
//   - the sky beyond the weather: the geomagnetic Kp (solar cycle 25, 27-day recurrent storms, rare big storms) and
//     from it the aurora at our latitude; halos, rainbows, noctilucent clouds and waterspouts as chances 0..1.
//
// Frame and units as the wind's (WindModel.h): local hours since 2000-01-01, compass bearings, m/s at 10 m, C, mm/h of
// water, metres above sea level.
#pragma once

#include <cstdint>
#include <vector>

namespace LyudWeather
{
	struct FWindParams;

	/** Synoptic weather types (the episode the area is in). Keep in step with ELyudWeatherRegime. */
	enum class ERegime : uint8_t
	{
		High,         // anticyclone: settled, light winds, clear or a flat inversion deck in winter
		WarmFront,    // a cyclone's warm front approaching and passing: cirrus to steady rain, wind backing and freshening
		WarmSector,   // mild damp air behind it: low cloud, drizzle, sea fog in spring; humid in summer
		ColdFront,    // the squall line: heavy rain or snow, gusts, the wind veering NW, pressure jumping up
		Showery,      // polar maritime air behind the front: Cu and Cb, showers, bright spells, cold
		Occluded,     // a slow occluded low overhead: grey, long rain or drizzle, wind changing
		ArcticHigh,   // a cold continental high (winter): hard frost, dry NE-E wind, clear or snow grains
		ThunderyLow,  // a summer thundery low: hot humid air, Cb, thunderstorms, hail
		Count
	};

	enum class EPrecip : uint8_t
	{
		None, Drizzle, Rain, Showers, Thunderstorm, Hail, Graupel, Sleet, Snow, SnowShowers, FreezingDrizzle, FreezingRain, Count
	};

	/** Cloud genera (WMO) plus the special ones the game draws. Keep in step with ELyudCloudType. */
	enum class ECloud : uint8_t
	{
		None,
		Cirrus, Cirrocumulus, Cirrostratus,                  // high, ice, 6-11 km
		Altocumulus, Altostratus, AltocumulusLenticular,     // middle, 2-6 km
		Nimbostratus,                                        // the rain deck, low base, thick
		Stratocumulus, Stratus,                              // low layers
		Cumulus, CumulusCongestus, Cumulonimbus,             // convective, base from the dew-point spread
		Count
	};

	enum class EFog : uint8_t { None, Radiation, Advection, Steam, Frontal, Count };

	struct FCloudLayer
	{
		ECloud Type = ECloud::None;
		float Cover = 0.f;    // 0..1 of the sky this layer covers
		float BaseM = 0.f;    // base, m above sea level
		float TopM = 0.f;     // top
		float Density = 0.f;  // 0 thin (sun shows through) .. 1 dark, opaque
		float Sheet = 0.5f;   // 0 heaped or fibrous cells .. 1 a flat continuous sheet (changes smoothly; Type is its name)
	};

	/** The weather at one moment, the same over the whole map (local effects are for whoever draws them). */
	struct FWeatherState
	{
		double Hours = 0.0;

		// the synoptic episode
		ERegime Regime = ERegime::High;
		ERegime NextRegime = ERegime::High;
		float RegimeProgress = 0.f;     // 0 at the episode's start .. 1 at its end
		float HoursToNext = 0.f;        // until the next episode starts
		float Intensity = 0.5f;         // 0..1 how deep this cyclone is (or how strong the high)
		float PressureHPa = 1013.f;
		float PressureTendency = 0.f;   // hPa per 3 h

		// the air
		float TempC = 10.f;             // at 2 m
		float DewPointC = 6.f;
		float Humidity = 0.75f;         // relative 0..1
		float FreezingLevelM = 2000.f;  // 0 C level above sea level (0 when it is freezing at the ground)
		float Inversion = 0.f;          // 0..1 how stable the low air is (cold air trapped under warm)
		float Convection = 0.f;         // 0..1 how strongly the air rises in thermals (Cu growth)

		// the wind of the weather (ULyudWindSubsystem adds the day's rhythm, the sea breeze and gusts)
		float WindFromDeg = 235.f;
		float WindSpeed = 6.f;          // m/s at 10 m over the sea, before the day's rhythm
		float WindGustiness = 0.3f;     // extra gust spread the weather adds (fronts, showers)
		float UpperFromDeg = 260.f;     // ~5 km: where the clouds drift from
		float UpperSpeed = 20.f;        // m/s

		// clouds
		FCloudLayer Low, Mid, High;
		float CloudCover = 0.f;         // total 0..1 (what you see from the ground)
		float Lenticular = 0.f;         // 0..1 lens clouds (strong stable moist flow aloft)
		float ShowerCells = 0.f;        // 0..1 share of the sky in shower/Cb cells (for patchy rain over the map)

		// falling water
		EPrecip Precip = EPrecip::None;
		float PrecipRate = 0.f;         // mm/h of water
		float SnowFraction = 0.f;       // 0 rain .. 1 snow
		float SnowfallCmPerHour = 0.f;  // fresh snow depth rate on level ground
		float Thunder = 0.f;            // 0..1 (strikes per minute ~ 2 x this)
		float Hail = 0.f;               // 0..1 hail or graupel share and size

		// fog
		EFog FogType = EFog::None;
		float Fog = 0.f;                // 0..1
		float VisibilityM = 30000.f;    // horizontal visibility (fog, rain, snow)
		float FogTopM = 0.f;            // how deep the fog is (a shallow ground fog is a few metres)

		// what the past leaves on the ground and the sea (FSeason)
		float Wet = 0.f;                // 0 dry .. 1 soaked ground / puddles
		float SnowDepthCm = 0.f;        // undisturbed level snow (drifts and paths are the winter thread's)
		float SnowWaterMm = 0.f;        // its water equivalent
		float SnowDensity = 100.f;      // kg/m3: 60 fresh fluff .. 400 old wet
		float SnowAgeDays = 0.f;        // since the last fresh snowfall (whiteness)
		float Melt = 0.f;               // 0..1 how fast the snow is melting now (slush, drips)
		float Frost = 0.f;              // 0..1 hoarfrost/rime on surfaces now
		float Glaze = 0.f;              // 0..1 ice on surfaces from freezing rain
		float GroundFrostCm = 0.f;      // frozen soil depth
		float SeaTempC = 10.f;          // the sea's surface off the coast
		float FreezingDegreeDays = 0.f; // this winter's sum of (-T) over freezing days
		float SeaIceCm = 0.f;           // thickness of the level sea ice (0 = open water)
		float SeaIceCover = 0.f;        // 0..1 how much of the open sea is frozen over
		float SeaSurgeM = 0.f;          // storm surge on top of the tide: low pressure (1 cm/hPa) and onshore wind setup, m

		// the sky beyond the weather
		float Kp = 1.f;                 // planetary geomagnetic index 0..9
		float Aurora = 0.f;             // 0..1 how bright the aurora would be at our latitude if dark and clear
		float Halo = 0.f;               // 0..1 chance of a 22 degree halo (thin Cs over the sun or moon)
		float Rainbow = 0.f;            // 0..1 showers with sun (the sky draws it opposite a low sun)
		float Noctilucent = 0.f;        // 0..1 noctilucent clouds (June-July nights)
		float Waterspout = 0.f;         // 0..1 (late summer: cold air over the warm sea under Cb)
	};

	/** The climate the engine plays out. Defaults: the Kaliningrad coast with somewhat snowier winters for the game. */
	struct FClimate
	{
		uint32_t Seed = 750;
		double LatitudeDeg = 54.96, LongitudeDeg = 20.40;
		/** Mean temperature: yearly mean, half swing and the coldest day of the year (Jan ~ -2.5, Jul ~ 18). */
		double MeanTempC = 8.6, YearSwingC = 9.8, ColdestDay = 22.0;
		/** Day-night half swing on a clear calm day, C (more in summer). */
		double DaySwingC = 4.5;
		/** How much more it rains or snows than the climate (1 = the coast; ~800 mm a year). */
		double Wetness = 1.0;
		/** How hard the winters are: scales the cold highs' frost and how often they come (1 = some cold spells a winter, ice on the sea in most). */
		double WinterSeverity = 1.0;
		/** The sea's surface temperature: yearly mean, half swing and the coldest day (it lags the air by ~6 weeks). */
		double SeaMeanC = 9.0, SeaSwingC = 8.5, SeaColdestDay = 60.0;
		/** The solar cycle 25 maximum (fractional year) and how active it is: drives the aurora's storms. */
		double SolarMaxYear = 2024.9, SolarActivity = 1.0;
	};

	/** One episode of the chain. */
	struct FEpisode
	{
		double Start = 0.0, End = 0.0; // hours
		ERegime Regime = ERegime::High;
		float Intensity = 0.5f;        // depth of the low / strength of the high
		float Bias = 0.f;              // -1..1 the episode's own twist (warmer/colder air, a direction swing)
		uint32_t Key = 0;              // a hash for its own noises
	};

	/**
	 * The chain of episodes, played out from an epoch with the climate's seed. Built lazily as far as asked and kept,
	 * so lookups are a binary search. Deterministic: the same seed always gives the same weather on the same date.
	 */
	class FChain
	{
	public:
		explicit FChain(const FClimate& C);
		const FEpisode& At(double Hours, int* OutIndex = nullptr);
		const FEpisode& Index(int I) { Extend(I + 1); return Episodes[I]; }
		uint32_t Seed() const { return Climate.Seed; }
		const FClimate& GetClimate() const { return Climate; }
	private:
		void Extend(int Count);
		void ExtendTo(double Hours);
		FClimate Climate;
		std::vector<FEpisode> Episodes;
	};

	/** The weather that needs no history (the season's ground and sea come from FSeason). */
	FWeatherState Instant(double Hours, FChain& Chain);

	/**
	 * What the past leaves, integrated hour by hour from the 1 September before Hours (a winter's snow and sea ice
	 * start there). Cheap to advance; ULyudWeatherSubsystem keeps one and steps it with the clock.
	 */
	class FSeason
	{
	public:
		/** Brings the integration to Hours (forward from where it is, or from 1 September again if Hours went back). */
		void AdvanceTo(double Hours, FChain& Chain);
		/** Copies the season's state into S (and the wet ground's last hours, which needs finer steps). */
		void Apply(FWeatherState& S, FChain& Chain) const;
		double At() const { return Now; }
	private:
		void Reset(double Hours, FChain& Chain);
		void Step(double Hours, double Dt, FChain& Chain);
		double Now = -1e12;
		double SeasonStart = 0.0;
		/** What the season shows; Apply eases from the hour before (Prev) to the last step's (Cur), an hour late but never in steps. */
		struct FOut
		{
			double SnowWater = 0.0, SnowDepth = 0.0, SnowAge = 30.0, Melt = 0.0;
			double GroundFrost = 0.0, Fdd = 0.0, SeaTemp = 10.0, SeaIce = 0.0, IceCover = 0.0, Glaze = 0.0, Wet = 0.0;
		};
		FOut Cur, Prev;
	};

	/** A + (B - A) * K for every continuous field (the named kinds, types and regimes switch at K = 0.5). */
	FWeatherState LerpState(const FWeatherState& A, const FWeatherState& B, float K);

	/** Everything at Hours: the instant weather plus the season (advanced as needed). */
	FWeatherState Full(double Hours, FChain& Chain, FSeason& Season);

	/** The synoptic wind for the wind model: bearing it comes from and speed m/s (no day rhythm). */
	void SynopticWind(double Hours, FChain& Chain, float& OutFromDeg, float& OutSpeed);

	/** The climate everything plays (ULyudWeatherSubsystem sets it from its settings; the wind model asks the chain). */
	void SetClimate(const FClimate& C);
	FClimate GetClimate();
	/** Debug: the synoptic weather (and the wind that follows it) is read this many hours away from the clock, so an
	 *  episode of another day can be played now; the season (snow, ice) stays the clock's. 0 = off. */
	void SetHourShift(double Hours);
	double GetHourShift();
	/** The chain of the current climate, one per thread, rebuilt when SetClimate changes it. */
	FChain& SharedChain();

	const char* RegimeName(ERegime R);
	const char* PrecipName(EPrecip P);
	const char* CloudName(ECloud C);
	const char* FogName(EFog F);
	/** Parses a regime name (case-insensitive, e.g. "coldfront"); Count if unknown. */
	ERegime ParseRegime(const char* Name);

	/** The sun's elevation, degrees (simple formula; ALyudSky has the precise ephemeris). */
	double SunElevationDeg(double Hours, const FClimate& C);

	/** Hours since 2000 -> calendar year (fractional) and 1-based day of the year. */
	double YearOf(double Hours);
}
