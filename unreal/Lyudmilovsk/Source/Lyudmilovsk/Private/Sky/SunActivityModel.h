// The red dwarf's activity, in plain C++ (no Unreal types) so it can be run and checked outside the engine
// (tools/sky/sun/run.sh). ULyudSunActivity wraps it, ALyudSky feeds it to the sky material; docs/sun.md explains it.
//
// A pure function of the game clock, so a save, a jump of the clock or a second machine see the same star:
//   - active regions are born at random over the in-game days, in two belts and near the poles (M dwarfs keep spots
//     at high latitudes too), grow in a fraction of their life and decay slowly; they ride the star's spin with a
//     little differential rotation (the equator laps the high latitudes);
//   - each region carries a spot group (a leader, followers, pores; its size from the region's strength) and faculae;
//   - prominences: loop systems and surges standing on the active regions (hours), long quiescent hedgerows at middle
//     and high latitudes (days to weeks); they grow, sway, some erupt (lift off, expand and fade within an hour or so)
//     and the rest fade where they stand;
//   - flares on the active regions: a sudden brightening for minutes (M dwarfs flare many times a day).
// Regions follow the game's days (they turn with the star); prominences and flares run on an activity clock
// TimeScale times faster than the game's, so a few real minutes of looking show them change (docs/sun.md).
//
// Frames: the star's own ("star frame": y = its north, z = the face toward us when its spin is 0, longitudes in it
// turn with the equator) and the view frame (x = east on the disk, y = north, z = toward the viewer), both on the
// unit star. The spin is the angle the star has turned since 2026-01-01 00:00: a feature at star longitude L faces
// us when the spin is L (the sky material's own convention, sky_hlsl.py).
#pragma once

#include <cstdint>

namespace LyudSunActivity
{
	constexpr int MaxRegions = 8;        // what the sky material is given (the most visible ones)
	constexpr int MaxProminences = 16;

	struct FSettings
	{
		double RotationDays = 6.0;       // the equator's period (ALyudSky::SunRotationDays)
		float DiffRotation = 0.06f;      // Omega(lat) = Omega_eq (1 - DiffRotation sin^2 lat): fully convective stars turn almost solidly
		float Activity = 1.f;            // births of regions, prominences and flares x this (0 = a quiet star)
		float TimeScale = 20.f;          // the activity clock over the game clock (prominences, eruptions, flares)
		uint32_t Seed = 3200;
	};

	/** An active region now (star frame). */
	struct FRegion
	{
		float Lat = 0.f, Lon = 0.f;      // rad; Lon in the star frame (turns with the spin)
		float Strength = 0.f;            // 0..1 now (spot sizes, faculae, how often it flares and builds loops)
		float Seed = 0.f;                // 0..1000, the group's own shape
		float ViewZ = 0.f;               // its centre's z in the view frame now (1 = disk centre, 0 = the limb)
		int Id = 0;
	};

	enum class EPromKind : uint8_t { Loop = 0, Hedgerow = 1, Surge = 2 };

	/** A prominence now (view frame), as the sky material draws it. */
	struct FProminence
	{
		float A[3] = { 0, 0, 1 };        // the anchor (the middle of its foot) on the unit star
		float T[3] = { 1, 0, 0 };        // the axis along its foot, tangent to the star at A
		float HalfWidth = 0.05f;         // rad over the star from the middle to each end of the foot
		float Height = 0.1f;             // star radii above the surface at the top
		float Bright = 0.f;              // 0..~1.2 emission (0 = none)
		float Lift = 0.f;                // star radii it has risen as a whole (an eruption)
		float Seed = 0.f;                // 0..1000
		float Life01 = 0.f;              // its age over its life
		float Visibility = 0.f;          // how much of it shows (off the limb, or as a dark filament on the disk)
		EPromKind Kind = EPromKind::Loop;
		bool bErupting = false;
	};

	struct FFlare
	{
		float Lat = 0.f, Lon = 0.f;      // star frame, rad
		float Strength = 0.f;            // 0..1 now
		float Peak = 0.f;                // its peak
		float AgeMin = 0.f;              // activity minutes since the start
		float Face = 0.f;                // 0..1 how much it faces us (0 = behind the star)
		int Region = -1;
	};

	struct FState
	{
		double Spin = 0.0;               // rad, wrapped to 0..2pi
		FRegion Regions[MaxRegions];
		int NumRegions = 0;
		FProminence Proms[MaxProminences];
		int NumProms = 0;
		FFlare Flare;
		/** The daylight's share of a flare now (Strength x Face), 0..1. */
		float FlareLight = 0.f;
		// counts over the whole star (tests, the debug line)
		int AliveRegions = 0, AliveProms = 0, LimbProms = 0, Eruptions = 0;
	};

	/** A flare staged by hand (lyud.Sun.ForceFlare): its start on the activity clock and its peak. */
	struct FForcedFlare
	{
		double StartActivityHours = -1e9;
		float Peak = 0.f;
	};

	/** The game clock in hours since 2026-01-01 00:00 local -> the activity clock in activity hours. */
	double ActivityHours(double GameHours, const FSettings& S);
	/** The star's spin at a game hour, rad (0..2pi). */
	double SpinAt(double GameHours, const FSettings& S);

	/** Everything on the star at GameHours (hours since 2026-01-01 00:00 local). */
	void Evaluate(double GameHours, const FSettings& S, FState& Out, const FForcedFlare* Forced = nullptr);
}
