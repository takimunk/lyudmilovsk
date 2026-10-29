// The rare sky, in plain C++ (no Unreal types) so it can be run and checked outside the engine (tools/sky/rare/run.sh).
// ALyudRareSky wraps it and draws it; docs/rare_sky.md explains it.
//
// The weather engine (WeatherModel.h) decides how likely each phenomenon is (the geomagnetic Kp and the aurora's
// strength at our latitude, the chances of a halo, a rainbow, noctilucent clouds, thunder). This turns those chances
// into what is actually in the sky at a given minute, as a pure function of the local hour, so a save, a jump of the
// clock or a second machine see the same sky:
//   - the aurora: where the oval stands (its equatorward edge from Kp, nearer at magnetic midnight), and its night as
//     a string of substorms: a quiet arc that brightens and creeps south (growth), a sudden breakup with rays, folds
//     and fast motion (expansion, ~10 min), then fading pulsating patches (recovery);
//   - halos and sun dogs: thin cirrostratus drifts over in patches, so the halo comes and goes for tens of minutes;
//   - rainbows: a shower opposite a low sun, a few to twenty minutes each; Europa makes faint moonbows;
//   - noctilucent clouds: summer twilight, the engine's chance;
//   - flares of our red dwarf (a flare star): a white-hot spot on its face for minutes, the daylight a little
//     brighter and whiter while it lasts. Small flares come every few days, big ones a few times a year.
//
// Frame and units as the weather's: local hours since 2000-01-01, compass bearings, km.
#pragma once

#include <cstdint>

namespace LyudRareSky
{
	/** What the weather engine and the sky give for this moment. */
	struct FInputs
	{
		double Hours = 0.0;          // local hours since 2000-01-01
		uint32_t Seed = 2660;
		// the weather engine's (FWeatherState)
		float Kp = 1.f;              // 0..9
		float Aurora = 0.f;          // 0..1 how bright the aurora would be here if dark and clear
		float Halo = 0.f;            // 0..1 chance of a halo (thin Cs over the sun or the moon)
		float Rainbow = 0.f;         // 0..1 showers with sun
		float Noctilucent = 0.f;     // 0..1
		float CloudCover = 0.f;      // 0..1 total
		float HighCover = 0.f;       // 0..1 of the high (ice) layer
		float HighDensity = 0.f;     // 0 thin .. 1 opaque
		float PrecipRate = 0.f;      // mm/h
		float ShowerCells = 0.f;     // 0..1
		// the sky's
		float SunAltDeg = 0.f;
		float MoonAltDeg = 0.f;
		float MoonPhase = 0.f;       // 0 new .. 1 full (lit fraction)
		// tests and the console: a substorm whose breakup was this many minutes ago (below -1e8: none)
		float ForceOnsetMinutes = -1e9f;
	};

	enum class EAuroraPhase : uint8_t { None, Quiet, Growth, Breakup, Recovery };

	struct FAurora
	{
		EAuroraPhase Phase = EAuroraPhase::None;
		float Brightness = 0.f;      // 0..~1.5 the brightest arc (before darkness and cloud)
		float Visible = 0.f;         // Brightness x the sky's darkness (what reaches the eye over clear sky)
		float EdgeKm = 2000.f;       // ground distance toward the magnetic pole from us to the main arc's foot (negative: south of us)
		float SpreadKm = 150.f;      // how far poleward the other arcs and the diffuse glow reach beyond it
		float Activity = 0.f;        // 0 still, smooth .. 1 breakup: rays, folds, fast motion
		float Pulsate = 0.f;         // 0..1 the recovery's pulsating patches
		float Red = 0.3f;            // the 630 nm red's share over the green (high, diffuse, storms; SAR arcs)
		float Fringe = 0.f;          // 0..1 the pink-purple lower border of a vigorous display
		float LowerKm = 105.f;       // the curtains' lower border altitude
		float MinutesFromOnset = 0.f;// since the current substorm's breakup (negative: before it)
		float Seed = 0.f;            // the current substorm's own pattern
	};

	struct FHalo
	{
		float Strength = 0.f;        // 0..1 the 22-degree ring (ice crystals in the veil)
		float SunDogs = 0.f;         // 0..1 the parhelia (plate crystals; best with a low sun)
		float Pillar = 0.f;          // 0..1 the light pillar over a low sun
		float Ring46 = 0.f;          // 0..1 the faint 46-degree ring
		bool bMoon = false;          // round Europa (night) instead of the sun
	};

	struct FRainbow
	{
		float Strength = 0.f;        // 0..1 the primary bow (the secondary is ~0.4 of it)
		float Rain = 0.f;            // 0..1 how much of the antisolar sky has rain in it (where the bow can stand)
		bool bMoon = false;          // a moonbow
	};

	struct FFlare
	{
		float Strength = 0.f;        // 0..1 now (0 = none)
		float Peak = 0.f;            // 0..1 this flare's peak (0.2 small .. 1 a superflare)
		float Lat = 0.f, Lon = 0.f;  // where on the star's face, rad (lon from the face turned to us)
		float Minutes = 0.f;         // since it began
	};

	struct FState
	{
		FAurora Aurora;
		FHalo Halo;
		FRainbow Rainbow;
		float Noctilucent = 0.f;     // 0..1 how bright the noctilucent clouds are now (twilight, before cloud)
		FFlare Flare;
	};

	/** Our observer: the geomagnetic latitude (corrected, deg) and the bearing of the geomagnetic pole. */
	struct FSite
	{
		double MagLatDeg = 52.0;
		double PoleBearingDeg = 344.0;
		/** Local hour of magnetic midnight (the oval reaches farthest south then). */
		double MagMidnightHour = 23.7;
	};

	FState Evaluate(const FInputs& In, const FSite& Site = FSite());

	/** The oval's equatorward edge, corrected geomagnetic latitude, at a Kp and a magnetic local time (hours, 0 = midnight). */
	double OvalEdgeLat(double Kp, double MltHours);

	/** For tools and tests: the substorm onsets (local hours) of the magnetic night around Hours. */
	int SubstormOnsets(double Hours, uint32_t Seed, float Kp, double* Out, int Max);

	/** The flares (start hour, peak 0..1) that begin in [From, To). */
	int Flares(double From, double To, uint32_t Seed, double* OutStart, float* OutPeak, int Max);

	const char* PhaseName(EAuroraPhase P);
}
