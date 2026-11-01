// The sea's currents off the Sambian coast, in plain C++ (no Unreal types) so tools/underwater/run.sh test can check
// them outside the engine. Metres, seconds, the world's X/Y (the coast runs along Y, the sea lies toward +X).
#pragma once

#include <cstdint>

namespace LyudSeaCurrent
{
	struct FVec3
	{
		float X = 0.f, Y = 0.f, Z = 0.f;
	};

	/** What drives the water now. */
	struct FDrive
	{
		/** The wind the sea has felt over the last hours (blowing toward), m/s. */
		float WindX = 0.f, WindY = 0.f;
		/** The tide's rate, m/s (positive: rising); and its spring-neap state 0..1. */
		float TideRate = 0.f;
		float Spring = 0.5f;
		/** Seconds (any clock that runs on: the eddies drift with it). */
		double Time = 0.0;
	};

	/** Where in the water. */
	struct FPlace
	{
		double X = 0.0, Y = 0.0;
		/** Still-water depth here now, m. */
		float Depth = 5.f;
		/** Height over the bed, m (the bed's boundary layer slows the water near it). */
		float OverBed = 2.f;
		/** Stones on the bed 0..1 (a reef roughens the flow: eddies behind it). */
		float Rough = 0.f;
	};

	/**
	 * The current at a place, m/s (Z: the small up and down of the eddies). Three parts:
	 * - the longshore current the wind drives (~2% of the wind's alongshore part, strongest in the bar zone a few metres deep),
	 * - the tidal stream: along the coast with the flood and back with the ebb (up to ~0.35 m/s at springs out on the
	 *   bed), and across the shore over the flat, filling and draining it (faster where the water is shallow),
	 * - eddies a few metres across drifting with it (stronger over rough bed),
	 * all slowed toward the bed by a log boundary layer.
	 */
	FVec3 At(const FDrive& D, const FPlace& P);

	/** The mean current at a place (no eddies), m/s: what fish hold against and weed leans with. */
	FVec3 Mean(const FDrive& D, const FPlace& P);

	/** How turbulent the water is there, 0..1 (the eddies' share of the flow). */
	float Turbulence(const FDrive& D, const FPlace& P);
}
