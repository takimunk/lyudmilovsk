// The wind over the outdoor map, in plain C++ (no Unreal types) so it can be run and looked at outside
// the engine (tools/sea/run.sh). ULyudWindSubsystem wraps it for the game.
//
// The wind is a function of the local date and hour, not a simulation: the same moment always has the
// same wind, so a saved game, a jump of the clock or a second machine see the same weather. Three
// scales stack up:
//   - the weather: direction and strength wander over a day or two around the Baltic autumn's
//     prevailing south-westerly (smooth noise over time, seeded);
//   - the day: stronger and gustier in the afternoon when the sun stirs the air, calmer at night, and a
//     sea breeze onshore on quiet sunny afternoons (day length from the date and latitude);
//   - gusts: patches of stronger and weaker air tens of metres across, drifting downwind and changing
//     as they go (value noise over the ground and time), with the wind veering a little in each.
//
// Frame: Unreal world metres, north = +Y, east = -X (ALyudSky with NorthYawDeg 0). Speeds in m/s at
// 10 m over open water.
#pragma once

#include <cstdint>

namespace LyudWeather
{
	struct FWindParams
	{
		/** Compass bearing the wind comes from most often (225 = south-west). */
		double PrevailingFromDeg = 235.0;
		/** How far the weather swings the direction either way, degrees. */
		double DirectionSwingDeg = 75.0;
		/** Median wind of the weather, m/s, and its spread (log-normal, so calms and storms both happen). */
		double MedianSpeed = 6.5;
		double SpeedLogSigma = 0.6;
		double MinSpeed = 0.3, MaxSpeed = 20.0;
		/** The wind at night as a share of the afternoon's (the air settles once the sun is down). */
		double NightFactor = 0.6;
		/** Gusts: speed spread as a share of the mean, by day and by night. */
		double GustinessDay = 0.32, GustinessNight = 0.14;
		/** Onshore sea breeze on a calm sunny afternoon, m/s, and the bearing the sea lies at from the coast. */
		double SeaBreeze = 2.2;
		double SeaBearingDeg = 270.0;
		/** Where the world is (day length). */
		double LatitudeDeg = 54.96;
		double LongitudeDeg = 20.40;
		double UtcOffsetHours = 2.0;
		uint32_t Seed = 1977;
		/** Take the weather's direction and strength from the synoptic chain (WeatherModel.h: fronts veer and freshen
		 *  it); false = the old free-running noise of Seed. The day's rhythm, the sea breeze and gusts stay here. */
		bool bSynoptic = true;
	};

	/** The wind of the whole area at one moment, without gusts. */
	struct FWindState
	{
		double Hours = 0.0;          // local hours since 2000-01-01 00:00
		float FromDeg = 0.f;         // compass bearing the mean wind comes from
		float Speed = 0.f;           // mean speed, m/s
		float DirX = 0.f, DirY = 1.f;// world unit vector the wind blows toward
		float Gustiness = 0.f;       // gust spread as a share of the mean
		float Day = 0.f;             // 0 night .. 1 the stirred afternoon air
		float Weather = 0.f;         // the weather's own speed before the day's rhythm, m/s
	};

	/** The gusts at a point: the factor on the mean speed and the veer of the direction, radians. */
	struct FGust
	{
		float Factor = 1.f;
		float Veer = 0.f;
	};

	/** Local date and hour to hours since 2000-01-01 00:00. */
	double HoursSince2000(int Year, int Month, int Day, double HourOfDay);

	/** 0..1 how stirred the air is at this local hour of this day of the year (0 at night, 1 mid-afternoon). */
	float DayFactor(double Hours, const FWindParams& P);

	/** The area's mean wind at a moment. */
	FWindState Regime(double Hours, const FWindParams& P);

	/** The mean wind averaged over the last few hours: what the sea's waves have grown in. */
	FWindState SeaStateWind(double Hours, const FWindParams& P, double OverHours = 3.0);

	/**
	 * Gusts at a point (world metres). Offset = how far the air has drifted (the integral of the mean wind
	 * over time, metres, wrapped to GustPeriod), Seconds = a clock for the gusts' own change. The HLSL in
	 * tools/sea/sea_hlsl.py (GUST_HLSL) computes the same field for materials.
	 */
	FGust GustAt(const FWindState& S, double X, double Y, double OffsetX, double OffsetY, double Seconds);

	/** The gust field repeats every GustPeriod metres, so the drift offset can be wrapped without a seam. */
	constexpr double GustPeriod = 4096.0;

	/** Mean wind at a height over the ground: the log profile over open water (Z0Sea) or land (Z0Land), m/s factor. */
	float HeightFactor(float HeightAboveGroundM, bool bOverWater);

	/** The noises (exposed for the HLSL mirror's test). */
	float ValueNoise3(double X, double Y, double Z, uint32_t Seed, int Period);
	float SmoothNoise1(double T, uint32_t Seed);
}
