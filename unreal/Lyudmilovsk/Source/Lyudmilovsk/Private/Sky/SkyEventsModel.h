#pragma once

#include <string>
#include <vector>

#include "TelescopeModel.h"

/**
 * What happens in the sky on a given night besides the planets (docs/sky_events.md): the year's meteor showers on their
 * real dates, the real moon's phase and libration, satellites crossing the dusk, and planets standing close together.
 * Plain C++ over the telescopes' model (LyudScope::Sky gives the sun and the planets); tools/skyeventsim/run.sh checks it.
 * Day is Schlyter's day number in UT (LyudScope::DayNumber), as everywhere in LyudScope.
 */
namespace LyudSkyEvents
{
	// ------------------------------------------------------------------ meteor showers

	struct FShower
	{
		/** "quadrantids", "lyrids", "eta_aquariids", "perseids", "orionids", "leonids", "geminids", "ursids". */
		const char* Id;
		/** The sun's ecliptic longitude at the maximum (degrees, of date): the earth is at the same place of its orbit every year. */
		double PeakSunLon;
		/** Meteors an hour at the maximum for a perfect sky with the radiant overhead. */
		double Zhr;
		/** How fast it falls off: the rate is Zhr * 10^(-Slope * degrees of the sun's longitude from the peak). */
		double Slope;
		/** Where they seem to come from. */
		double RadiantRaDeg, RadiantDecDeg;
	};

	const std::vector<FShower>& Showers();
	/** The sun's ecliptic longitude of a sky, degrees 0..360. */
	double SunLongitude(const LyudScope::FSky& S);
	/** A shower's zenithal rate at a moment (0 when it is under 1 an hour). */
	double ZenithRate(const FShower& Shower, double SunLonDeg);

	struct FMeteors
	{
		/** The strongest shower now (-1: only the sporadic ones), where its radiant stands. */
		int Shower = -1;
		double RadiantAltDeg = -90.0, RadiantAzDeg = 0.0;
		/** Meteors an hour somebody watching would count: the shower's and the sporadic background's. */
		double ShowerPerHour = 0.0;
		double SporadicPerHour = 0.0;
		/** The faintest star seen (6.5 in a perfect night; twilight, the moon and haze take from it). */
		double LimitMagnitude = 6.5;
	};

	/** MoonLit 0..1 and MoonAltDeg: the game's moon as the sky shows it; CloudCover 0..1. */
	FMeteors Meteors(const LyudScope::FSky& S, double MoonLit, double MoonAltDeg, double CloudCover);

	// ------------------------------------------------------------------ the real moon

	struct FMoon
	{
		/** Ecliptic longitude and latitude, degrees (geocentric, about 2' good). */
		double LonDeg = 0.0, LatDeg = 0.0;
		/** How far from the sun, 0 new .. 180 full; the lit part 0..1; days since new; waxing. */
		double ElongationDeg = 0.0;
		double Lit = 0.0;
		double AgeDays = 0.0;
		bool bWaxing = true;
		/** The optical libration: how far the face is turned, in longitude (+: the east limb, Mare Crisium's, comes round)
		 *  and in latitude (+: the north pole toward us), degrees. */
		double LibrationLonDeg = 0.0, LibrationLatDeg = 0.0;
	};

	FMoon Moon(double Day);
	/** "new", "waxing_crescent", "first_quarter", "waxing_gibbous", "full", "waning_gibbous", "last_quarter", "waning_crescent". */
	const char* PhaseId(const FMoon& M);
	/** The next new (bFull false) or full moon after Day. */
	double NextPhase(double Day, bool bFull);

	// ------------------------------------------------------------------ satellites

	struct FSatellite
	{
		/** "mir" (the old station, bright), "meteor" (a weather satellite over the poles), "zarya" (a small one). */
		const char* Id;
		double HeightKm, InclinationDeg;
		/** The node and the place on the orbit at Day 0. */
		double NodeDeg, PhaseDeg;
		/** How bright it is overhead in full sun (magnitude). */
		double Magnitude;
	};

	const std::vector<FSatellite>& Satellites();

	struct FSatNow
	{
		double AltDeg = -90.0, AzDeg = 0.0;
		double RangeKm = 0.0;
		bool bSunlit = false;
		/** Above 10 degrees, in the sun, and the sky dark enough (the sun 6 degrees down or more). */
		bool bSeen = false;
		double Magnitude = 99.0;
	};

	FSatNow Where(const FSatellite& Sat, const LyudScope::FSky& S);

	struct FPass
	{
		int Satellite = 0;
		/** Seen from .. to (Day, UT), and the highest it gets. */
		double From = 0.0, To = 0.0, PeakDay = 0.0;
		double PeakAltDeg = 0.0, PeakAzDeg = 0.0;
		double FromAzDeg = 0.0, ToAzDeg = 0.0;
		/** It goes out in the earth's shadow before it sets. */
		bool bFades = false;
	};

	/** The passes seen in [DayFrom, DayTo). */
	std::vector<FPass> Passes(double DayFrom, double DayTo, double LatDeg, double LonDeg);

	// ------------------------------------------------------------------ planets side by side

	struct FConjunction
	{
		LyudScope::EBody A = LyudScope::EBody::Venus, B = LyudScope::EBody::Jupiter;
		/** The night they are closest among those they can be seen on, and how close (degrees). */
		double Day = 0.0;
		double SeparationDeg = 0.0;
		/** How high the lower of the two stands then, and whether it is the evening sky (else before dawn). */
		double AltDeg = 0.0;
		bool bEvening = true;
		/** The first and the last night they are within the limit. */
		double From = 0.0, To = 0.0;
	};

	/** Pairs of Venus, Mars, Jupiter and Saturn within LimitDeg of each other in a dark sky (the sun 6 degrees down, both
	 *  5 degrees up) in [DayFrom, DayTo). */
	std::vector<FConjunction> Conjunctions(double DayFrom, double DayTo, double LatDeg, double LonDeg, double LimitDeg = 3.0);
}
