// The wind where you stand, in plain C++ (no Unreal types; tools/windsim/run.sh runs it outside the engine).
// ULyudWindSubsystem wraps it for the game (GetLocalWind and friends in Public/Weather/LyudWind.h).
//
// WindModel.h gives the area's wind (Regime) and its drifting gusts (GustAt) over an open, flat world. This adds
// what yachts, kites, birds and people on the beach feel on top of that, without changing either of those
// functions (the sea's material mirrors GustAt in HLSL, and the suites are calibrated on Regime):
//
//   1. Shifts: the direction swings a few degrees either way every several minutes and the whole area breathes
//      (a puff, a lull). A function of the calendar hour like the regime, so a save or a second machine agree.
//      This is what a helmsman plays ("the wind has headed us") and what turns a kite's window.
//   2. The land and the sea: over a height grid of the map the wind is slowed in the lee of the cliff, of the
//      forest and of houses, squeezed and sped up over crests and headlands, rises on a windward slope
//      (ridge lift) and sinks and tumbles behind it (rotor, turbulence); over water it runs freer the longer
//      its fetch, over land it is rougher; an offshore wind leaves a calm strip under the cliff.
//   3. Looking ahead: the gust that will reach a point in N seconds (the field drifts downwind, so the answer is
//      exact for the open-air part): dark patches on the water coming at a dinghy, a child bracing a kite.
//   4. Words and numbers people use: Beaufort, compass points, knots, an outlook for the next hours, the next
//      spell of wind good for something (a sailing lesson, a kite afternoon).
//   5. Small helpers for things that move through the air (apparent wind, the angle off the bow).
//
// Frame as WindModel.h: world metres, north = +Y, east = -X, Z up. Speeds m/s. Bearings are compass degrees
// the wind comes FROM. Everything is deterministic for the same inputs and allocation-free per call.
#pragma once

#include "WindModel.h"

#include <cstdint>
#include <vector>

namespace LyudWeather
{
	// ---------------------------------------------------------------- 1. shifts

	/** The slow swing of the direction and the area's breathing at a moment. */
	struct FWindShift
	{
		float VeerDeg = 0.f;        // added to the regime's FromDeg (+ = veered, clockwise on the compass; - = backed)
		float Factor = 1.f;         // on the regime's speed: a puff over the whole bay > 1, a lull < 1 (about 0.8 .. 1.25)
		float PeriodMin = 8.f;      // how long one swing takes now, minutes (shorter in stirred afternoon air)
		float Phase = 0.f;          // 0..1 through the current swing
		float NextTurnMin = 0.f;    // minutes until the veer next changes sign (when a tack will pay)
		float RangeDeg = 0.f;       // how far it swings either way today (2 a steady gradient wind .. 18 a light shifty day)
	};

	/** The shift at a calendar moment for this regime (needs its Speed, Day and Gustiness). Seed = FWindParams::Seed. */
	FWindShift ShiftAt(double Hours, const FWindState& Regime, uint32_t Seed);

	/** The regime with a shift applied: FromDeg, DirX/DirY and Speed change; the rest is kept. */
	FWindState Shifted(const FWindState& Regime, const FWindShift& Shift);

	/** Regime(Hours, P) with ShiftAt applied: the wind a game shows at this moment. */
	FWindState Now(double Hours, const FWindParams& P);

	// ---------------------------------------------------------------- 2. the land and the sea

	/**
	 * The map's relief as the wind sees it: a coarse grid of ground heights and of what stands on the ground.
	 * The game fills it once from the landscape, the forests and the town's houses; windsim fills it from a
	 * small model of the coast (cliff, beach, upland, town terraces). Outside the grid there is open sea.
	 */
	struct FWindTerrain
	{
		double X0 = 0.0, Y0 = 0.0;      // world metres of cell (0, 0)'s centre
		double Cell = 16.0;             // metres
		int NX = 0, NY = 0;
		float SeaZ = -43.f;             // the sea's datum, world metres (the tide's level is passed per query)
		std::vector<float> Ground;      // NX * NY, world Z of the solid ground (the bed under the sea), metres
		std::vector<float> Cover;       // NX * NY or empty: height of what stands there (crowns, roofs), metres

		bool Valid() const { return NX > 1 && NY > 1 && int(Ground.size()) == NX * NY; }
		/** Bilinear; outside the grid the sea bed (SeaZ - 10). */
		float GroundAt(double X, double Y) const;
		/** Bilinear; 0 outside or with no cover grid. */
		float CoverAt(double X, double Y) const;
		/** The cell index of a point, or -1 outside. */
		int CellAt(double X, double Y) const;
	};

	/** The air at a point. */
	struct FLocalWind
	{
		float VX = 0.f, VY = 0.f, VZ = 0.f; // world velocity of the air, m/s (VZ: ridge lift up, lee sink down)
		float Speed = 0.f;                  // horizontal speed, m/s
		float FromDeg = 0.f;                // compass bearing the air here comes from (the land bends it a little)
		float Gust = 1.f;                   // the open-air gust factor here (FGust::Factor)
		float Exposure = 1.f;               // what the land does to the speed: 0 dead lee .. 1 open .. ~1.4 on a crest or a headland
		float Shelter = 0.f;                // 0 open .. 1 deep in a lee (walls, the cliff, the forest upwind)
		float Updraft = 0.f;                // = VZ
		float Turbulence = 0.f;             // rms m/s of the tumbling the mean does not carry (rotor, rough land, gusty air)
		float HeightM = 10.f;               // height over the ground or the water here
		float SurfaceZ = 0.f;               // world Z of the ground or the water under the point, metres
		float FetchM = 0.f;                 // open water upwind of the point, metres (0 over land; capped at 3000)
		bool bOverWater = false;
	};

	/**
	 * The air at world point (X, Y, Z) metres. S is the area's wind (pass Now(), or Regime() for no shifts),
	 * Terrain may be null or not Valid (then: open flat land at Z = 0 unless WaterZ > -1e8 and bFlatIsWater...
	 * see the cpp: without a terrain the result equals GustAt x HeightFactor, Exposure 1, no updraft).
	 * WaterZ = the sea's level now (tide), world metres. OffsetX/OffsetY/Seconds as GustAt.
	 */
	FLocalWind LocalWindAt(const FWindState& S, const FWindTerrain* Terrain, double X, double Y, double Z, float WaterZ,
		double OffsetX, double OffsetY, double Seconds);

	// ---------------------------------------------------------------- 3. looking ahead

	/** The open-air gust factor that will be at (X, Y) in InSeconds, if the regime holds (the field drifts with it). */
	float GustAhead(const FWindState& S, double X, double Y, double OffsetX, double OffsetY, double Seconds, double InSeconds);

	/** The next gust or lull worth a word. */
	struct FGustEvent
	{
		float InSeconds = -1.f;     // < 0: nothing within the look-ahead
		float Factor = 1.f;         // its peak (a gust) or trough (a lull)
		float LastsSeconds = 0.f;   // how long it stays beyond the threshold
		bool bLull = false;
	};
	/** Scans ahead (to WithinSeconds, at most 120) for the first gust over 1 + Threshold or lull under 1 - Threshold. */
	FGustEvent NextGust(const FWindState& S, double X, double Y, double OffsetX, double OffsetY, double Seconds,
		double WithinSeconds = 60.0, float Threshold = 0.25f);

	// ---------------------------------------------------------------- 4. words and numbers

	/** Beaufort force 0..12 of a 10 m wind speed. */
	int Beaufort(float SpeedMs);
	/** A key for the string tables: "calm", "light_air", "light_breeze", "gentle_breeze", "moderate_breeze",
	 *  "fresh_breeze", "strong_breeze", "near_gale", "gale", "strong_gale", "storm", "violent_storm", "hurricane". */
	const char* BeaufortKey(int Force);
	/** Compass point of a bearing: Points 4, 8 or 16 -> "n", "nne", "ne" ... (lower case keys). */
	const char* CompassKey(float Deg, int Points = 8);
	inline float ToKnots(float Ms) { return Ms * 1.943844f; }

	/** What the next hours hold (regime with shifts, sampled every 10 minutes). */
	struct FWindOutlook
	{
		float MinSpeed = 0.f, MaxSpeed = 0.f, MeanSpeed = 0.f;  // m/s at 10 m
		float GustSpeed = 0.f;      // the strongest gust to expect, m/s (max of speed x (1 + 2.2 x gustiness x 0.8))
		float FromDeg = 0.f;        // the mean direction (vector mean)
		float TurnDeg = 0.f;        // how the direction ends against how it starts (+ veers, - backs)
		float Trend = 0.f;          // m/s per hour: freshening > 0, easing < 0
		int Force = 0;              // Beaufort of the mean
		int ForceMax = 0;           // Beaufort of the strongest gust
	};
	FWindOutlook Outlook(double Hours, double SpanHours, const FWindParams& P);

	/**
	 * Hours from Hours until the mean wind stays within [MinSpeed, MaxSpeed] for at least HoldHours (0 = it does now),
	 * or -1 if no such spell starts within WithinHours. bDaylight: only count the stirred day (DayFactor > 0).
	 */
	double NextSpell(double Hours, const FWindParams& P, float MinSpeed, float MaxSpeed, double HoldHours = 1.0,
		double WithinHours = 48.0, bool bDaylight = true);

	// ---------------------------------------------------------------- 5. things that move

	/** The wind felt by something moving at (BodyVX, BodyVY): true wind minus its own motion. */
	inline void Apparent(float TrueVX, float TrueVY, float BodyVX, float BodyVY, float& OutVX, float& OutVY)
	{
		OutVX = TrueVX - BodyVX;
		OutVY = TrueVY - BodyVY;
	}
	/** Compass bearing a world velocity blows FROM, degrees 0..360 (north +Y, east -X). */
	float BearingFrom(float VX, float VY);
	/** World unit vector the wind from this bearing blows toward. */
	void TowardOf(float FromDeg, float& OutX, float& OutY);
	/** The wind's angle off a heading: -180..180, 0 = dead ahead, + = on the starboard (right) side. */
	float AngleOff(float HeadingDeg, float FromDeg);
}
