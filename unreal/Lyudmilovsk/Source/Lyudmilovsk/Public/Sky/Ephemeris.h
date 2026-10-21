#pragma once

#include "CoreMinimal.h"

/** Where a body stands in the local sky: altitude above the horizon and azimuth from north through east, degrees. */
struct FSkyPosition
{
	double AltitudeDeg = 0.0;
	double AzimuthDeg = 0.0;

	/** Unit vector toward the body in local east, north, up. */
	FVector EastNorthUp() const;
};

/**
 * Low-precision sun and moon positions (Paul Schlyter's method, ~0.1 deg for the sun and ~0.3 deg for the
 * moon, good from 1900 to 2100): enough for the light to come from where it really would on that date.
 * Pure functions, no engine state.
 */
namespace LyudEphemeris
{
	/** Days since 2000-01-00 00:00 UT (Schlyter's day number), from a UTC date. */
	LYUDMILOVSK_API double DayNumber(const FDateTime& Utc);
	LYUDMILOVSK_API FSkyPosition Sun(const FDateTime& Utc, double LatitudeDeg, double LongitudeDeg);
	/** Topocentric (parallax corrected). PhaseFraction = lit part of the disk, 0 new moon .. 1 full. */
	LYUDMILOVSK_API FSkyPosition Moon(const FDateTime& Utc, double LatitudeDeg, double LongitudeDeg, double* PhaseFraction = nullptr);
}
