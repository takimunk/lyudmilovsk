// The day's sunset and sunrise, in plain C++ (no Unreal types) so it can be run and checked outside the engine
// (tools/sky/sunset/run.sh). ULyudSunset wraps it; docs/sun.md ("Sunsets") explains the rules.
//
// Every evening (and every morning) has its own character, a pure function of the date and a seed: pale golden, orange,
// pink, red, deep crimson, or a dull grey strip, now and then (~5%) a spectacular one. The weather then makes the most
// of it or spoils it: clear dry air gives a clean golden-orange, broken high and middle cloud the most vivid pink and red
// undersides, an overcast a dull grey with only a weak strip, the air after rain clean saturated colours, haze and mist a
// deep red low-contrast glow. It all blends in only while the sun is near the horizon (Band(): -9..11 degrees) and is
// exactly neutral (all scales 1, tints white) outside it.
#pragma once

#include <cstdint>

namespace LyudWeather { struct FWeatherState; }

namespace LyudSunset
{
	enum class EKind : uint8_t { Golden, Orange, Pink, Red, Crimson, Grey, Count };
	const char* KindName(EKind K);

	struct FRGB
	{
		float R = 1.f, G = 1.f, B = 1.f;
	};

	/** The character of one sunset or sunrise (before the weather). */
	struct FDayProfile
	{
		EKind Kind = EKind::Orange;
		bool bSunrise = false;
		bool bSpectacular = false;
		float Hue = 0.3f;        // 0 pale golden .. 0.25 orange .. 0.5 pink .. 0.75 red .. 1 crimson
		float Intensity = 1.f;   // how strong the colours are, 0..~1.7
		float Aerosol = 0.3f;    // 0 clean air .. 1 thick haze (Mie density, a redder, lower-contrast sun)
		float Ozone = 0.3f;      // 0..1 the blue-violet of the twilight sky after the sun has gone (ozone's Chappuis band)
		float CloudGlow = 1.f;   // how brightly the clouds catch the low light, 0..2
		float Afterglow = 0.5f;  // 0..1 how long the clouds keep glowing after sunset
		float Dull = 0.f;        // 0..1 a grey, washed-out evening whatever the weather
	};

	/** What the weather does to it, each 0..1 and continuous (no jumps when the weather changes). */
	struct FAir
	{
		float Overcast = 0.f;    // a closed low or middle deck
		float Broken = 0.f;      // clouds with gaps: high cirrus, altocumulus, heaped low cloud (what catches the light)
		float Clean = 0.f;       // washed air after rain, a fresh breeze
		float Haze = 0.f;        // humid air, mist, poor visibility, an inversion
		float Rain = 0.f;        // falling now
	};

	/** What the renderer gets: scales and tints on the sky, the light and the clouds. Neutral by default. */
	struct FLook
	{
		float Weight = 0.f;      // the band weight now (0 = neutral)
		float MieScale = 1.f;    // the atmosphere's Mie scattering
		float MieGAdd = 0.f;     // its anisotropy
		float MieAbsScale = 1.f; // Mie absorption (a duller, smoky haze)
		float MieHeightScale = 1.f; // the haze's scale height (deeper haze: more reddening near the horizon)
		float RayleighScale = 1.f;
		float OzoneScale = 1.f;  // other absorption (ozone): the blue hour's violet
		FRGB SkyTint;            // the sky's luminance (not the haze over the land)
		FRGB LightTint;          // the sunlight's colour (the land, the haze it lights, the clouds)
		FRGB CloudTint;          // on top for the clouds: their glow colour and strength
		float AureoleScale = 1.f;
		float DiskScale = 1.f;
		FRGB DiskTint;           // the photosphere's own colour on top of the air's: a hazy evening's deep red, a clean one's gold
		float ExposureEV = 0.f;  // stops added to the outdoor exposure (negative: the twilight's colour kept, not opened to cream)
	};

	/** Weight of the sunset look by the sun's altitude: 0 above 11 or below -9 degrees, 1 from -5 to 4. */
	float Band(float SunAltDeg);

	/** The profile of the sunset (bSunrise false) or sunrise of the local day DayIndex (days since 2000-01-01). Month 1..12
	 * leans the odds (hazy summers, clean pink winters). ForcedKind >= 0 picks the kind (0..5; 6 = a spectacular one);
	 * the rest of the profile still varies with the day. */
	FDayProfile Profile(uint32_t Seed, int64_t DayIndex, bool bSunrise, int Month, int ForcedKind = -1);

	/** The weather's share (ForceClean / ForceHaze >= 0 override those two, for tests). */
	FAir AirOf(const LyudWeather::FWeatherState& W, float ForceClean = -1.f, float ForceHaze = -1.f);

	/** The look at this sun altitude. Strength scales every change from neutral (1 = as designed, 0 = off). */
	FLook Look(const FDayProfile& P, const FAir& A, float SunAltDeg, float Strength = 1.f);
}
