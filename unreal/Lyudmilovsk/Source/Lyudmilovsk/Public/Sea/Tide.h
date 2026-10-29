#pragma once

#include "CoreMinimal.h"

/**
 * The tide on the coast: an equilibrium-like semidiurnal tide from the real sun and moon (LyudEphemeris),
 * the moon's pull plus 0.46 of the sun's, each cos(2 x hour angle), felt LagHours late. The two add up at
 * new and full moon (springs) and partly cancel at the quarters (neaps). High water is always the sea's
 * datum (the map's SEA_Z): the tide only goes down from it, NeapLowM at neaps, SpringLowM at springs.
 * Pure functions, no engine state (docs/tides.md).
 */
namespace LyudTide
{
	/** Ratio of the sun's tidal pull to the moon's. */
	constexpr double SunShare = 0.46;
	/** How late the water answers the sky, hours. */
	constexpr double LagHours = 1.0;
	/** Low water under the datum at neaps and at springs, metres (negative). Mirrored in tools/world_map.py. */
	constexpr double NeapLowM = -2.0;
	constexpr double SpringLowM = -4.5;

	struct FState
	{
		/** Water level over the datum, m (0 = high water of springs, negative below). */
		double LevelM = 0.0;
		/** This tide's low water and its range, m. */
		double LowM = 0.0;
		double RangeM = 0.0;
		/** 0 at neaps .. 1 at springs. */
		double Spring = 0.0;
		/** -1 low water .. 1 high water: where in the cycle the water is. */
		double Phase = 0.0;
		/** Rising (flood) or falling (ebb). */
		bool bRising = false;
	};

	/** The tide at a UTC moment at a longitude. */
	LYUDMILOVSK_API FState At(const FDateTime& Utc, double LongitudeDeg);
	/** The highest level the water reached in the Hours before Utc, m over the datum (where the sand is still wet). */
	LYUDMILOVSK_API double RecentHighM(const FDateTime& Utc, double LongitudeDeg, double Hours);
}
