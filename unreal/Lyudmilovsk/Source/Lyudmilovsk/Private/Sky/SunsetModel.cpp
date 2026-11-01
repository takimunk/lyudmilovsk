#include "SunsetModel.h"

#include "Weather/WeatherModel.h"

#include <algorithm>
#include <cmath>

namespace LyudSunset
{
	namespace
	{
		float Sat(float X) { return std::min(1.f, std::max(0.f, X)); }
		float Lerp(float A, float B, float T) { return A + (B - A) * T; }
		// UE's FMath::SmoothStep does not run downhill (docs/weather.md): our own
		float Smooth(float A, float B, float X)
		{
			const float T = Sat((X - A) / (B - A));
			return T * T * (3.f - 2.f * T);
		}
		FRGB Mix(const FRGB& A, const FRGB& B, float T) { return {Lerp(A.R, B.R, T), Lerp(A.G, B.G, T), Lerp(A.B, B.B, T)}; }
		float Lum(const FRGB& C) { return 0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B; }
		FRGB Scale(const FRGB& C, float K) { return {C.R * K, C.G * K, C.B * K}; }
		/** Pushes a colour away from (S > 1) or toward (S < 1) its own grey. */
		FRGB Saturate(const FRGB& C, float S)
		{
			const float Y = Lum(C);
			return {std::max(0.f, Y + (C.R - Y) * S), std::max(0.f, Y + (C.G - Y) * S), std::max(0.f, Y + (C.B - Y) * S)};
		}

		uint64_t Mix64(uint64_t X)
		{
			X += 0x9E3779B97F4A7C15ull;
			X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
			X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
			return X ^ (X >> 31);
		}
		struct FRand
		{
			uint64_t S;
			float Next() { S = Mix64(S); return float(S >> 40) / float(1ull << 24); }
			float Range(float A, float B) { return A + (B - A) * Next(); }
		};

		// The palette along the hue: pale golden, orange, pink, red, crimson, as multipliers on the sky's, the light's and
		// the clouds' own colour. Deep on purpose: the tonemapper's shoulder flattens a bright sunset sky, and a tint near
		// white (luminance-normalised, the first try) changed it by a few percent. Only partly compensated where used
		// (Comp), so a red evening is a little darker than a golden one, as it is.
		const FRGB LightHue[5] = {{1.00f, 0.92f, 0.70f}, {1.00f, 0.75f, 0.50f}, {1.00f, 0.66f, 0.74f}, {1.00f, 0.56f, 0.42f}, {1.00f, 0.42f, 0.40f}};
		const FRGB CloudHue[5] = {{1.00f, 0.85f, 0.55f}, {1.00f, 0.66f, 0.40f}, {1.00f, 0.55f, 0.75f}, {1.00f, 0.45f, 0.36f}, {1.00f, 0.32f, 0.38f}};
		const FRGB SkyHue[5] = {{1.00f, 0.93f, 0.72f}, {1.00f, 0.80f, 0.62f}, {1.00f, 0.70f, 0.88f}, {1.00f, 0.62f, 0.55f}, {1.00f, 0.48f, 0.52f}};
		// a dull evening: cool, so it takes the atmosphere's own orange out instead of only fading the tint
		const FRGB Grey = {0.80f, 0.88f, 1.00f};

		/** A tint at weight T, its luminance loss only partly given back (0 none .. 1 all). */
		FRGB Comp(const FRGB& C, float T, float Back)
		{
			const FRGB M = Mix(FRGB(), C, T);
			return Scale(M, std::pow(std::max(Lum(M), 0.05f), -Back));
		}

		FRGB Along(const FRGB (&P)[5], float Hue)
		{
			const float X = Sat(Hue) * 4.f;
			const int I = std::min(3, int(X));
			return Mix(P[I], P[I + 1], X - float(I));
		}

		struct FKindBase { float Hue, Intensity, Aerosol, Ozone, CloudGlow, Afterglow, Dull; };
		const FKindBase Kinds[int(EKind::Count)] = {
			// hue   int   aero  ozone glow  after dull
			{0.04f, 0.70f, 0.15f, 0.20f, 0.70f, 0.30f, 0.00f},   // pale golden: clean dry air, a short warm glow
			{0.27f, 0.95f, 0.35f, 0.30f, 1.00f, 0.50f, 0.00f},   // orange
			{0.50f, 1.00f, 0.22f, 0.75f, 1.25f, 0.75f, 0.00f},   // pink: clean upper air, violet blue hour, long afterglow
			{0.74f, 1.05f, 0.60f, 0.40f, 1.15f, 0.60f, 0.00f},   // red: hazier, deeper
			{0.96f, 1.20f, 0.72f, 0.55f, 1.40f, 0.85f, 0.00f},   // deep crimson: dramatic
			{0.30f, 0.45f, 0.85f, 0.10f, 0.35f, 0.15f, 0.80f},   // dull grey: milky, washed out
		};
	}

	const char* KindName(EKind K)
	{
		static const char* Names[] = {"golden", "orange", "pink", "red", "crimson", "grey"};
		return K < EKind::Count ? Names[int(K)] : "?";
	}

	float Band(float SunAltDeg)
	{
		return Smooth(-9.f, -5.f, SunAltDeg) * Smooth(11.f, 4.f, SunAltDeg);
	}

	FDayProfile Profile(uint32_t Seed, int64_t DayIndex, bool bSunrise, int Month, int ForcedKind)
	{
		FRand R{Mix64((uint64_t(Seed) << 33) ^ (uint64_t(DayIndex) * 2ull + (bSunrise ? 1ull : 0ull)) ^ 0x53554E53ull)};
		R.Next();
		// how often each kind comes: summer evenings hazier (golden, orange, red), winter air cleaner (pink, crimson)
		const float Summer = 0.5f - 0.5f * std::cos(float(std::max(1, std::min(12, Month)) - 1) * 0.5235988f);   // 0 Jan .. 1 Jul
		float W[int(EKind::Count)] = {
			20.f + 8.f * Summer,   // golden
			26.f + 6.f * Summer,   // orange
			20.f - 8.f * Summer,   // pink
			15.f + 3.f * Summer,   // red
			8.f,                   // crimson
			11.f - 4.f * Summer,   // grey
		};
		if (bSunrise)
		{
			// the morning air is cooler and cleaner: fewer red and grey dawns, more pink and golden ones
			W[int(EKind::Golden)] += 6.f;
			W[int(EKind::Pink)] += 6.f;
			W[int(EKind::Red)] -= 4.f;
			W[int(EKind::Grey)] -= 3.f;
		}
		float Sum = 0.f;
		for (float V : W) Sum += V;
		float Pick = R.Next() * Sum;
		int K = 0;
		for (; K < int(EKind::Count) - 1; K++)
		{
			Pick -= W[K];
			if (Pick < 0.f) break;
		}
		const float SpecRoll = R.Next();
		bool bSpectacular = SpecRoll < 0.05f && K != int(EKind::Grey);
		if (ForcedKind >= 0)
		{
			bSpectacular = ForcedKind >= int(EKind::Count);
			K = bSpectacular ? (SpecRoll < 0.5f ? int(EKind::Pink) : int(EKind::Crimson)) : std::min(ForcedKind, int(EKind::Count) - 1);
		}

		const FKindBase& B = Kinds[K];
		FDayProfile P;
		P.Kind = EKind(K);
		P.bSunrise = bSunrise;
		P.bSpectacular = bSpectacular;
		// every evening its own within its kind
		P.Hue = Sat(B.Hue + R.Range(-0.08f, 0.08f));
		P.Intensity = std::max(0.f, B.Intensity * R.Range(0.85f, 1.15f));
		P.Aerosol = Sat(B.Aerosol + R.Range(-0.1f, 0.1f) + (bSunrise ? -0.08f : 0.f));
		P.Ozone = Sat(B.Ozone + R.Range(-0.15f, 0.15f));
		P.CloudGlow = std::max(0.f, B.CloudGlow * R.Range(0.8f, 1.2f));
		P.Afterglow = Sat(B.Afterglow + R.Range(-0.15f, 0.15f));
		P.Dull = Sat(B.Dull + (B.Dull > 0.f ? R.Range(-0.15f, 0.1f) : 0.f));
		if (bSpectacular)
		{
			// the rare one: deep saturated cloud glow, a long afterglow, the violet blue hour after it
			P.Intensity = std::max(P.Intensity, 1.5f) * R.Range(1.f, 1.12f);
			P.CloudGlow = std::max(P.CloudGlow, 1.8f) * R.Range(1.f, 1.15f);
			P.Afterglow = 1.f;
			P.Ozone = std::max(P.Ozone, 0.8f);
			P.Dull = 0.f;
		}
		return P;
	}

	FAir AirOf(const LyudWeather::FWeatherState& W, float ForceClean, float ForceHaze)
	{
		FAir A;
		// a closed deck (like the opacity ALyudSky adapts the exposure to, but heaped cells with gaps between them, Sheet
		// toward 0, close it less: broken altocumulus is the vivid sunset, not an overcast)
		const float LowDeck = W.Low.Cover * Sat(W.Low.Density * 1.2f) * (0.35f + 0.65f * W.Low.Sheet);
		const float MidDeck = 0.7f * W.Mid.Cover * W.Mid.Density * (0.2f + 0.8f * W.Mid.Sheet);
		A.Overcast = Sat(1.f - (1.f - LowDeck) * (1.f - MidDeck));
		// clouds that catch the light: high ones (thin), middle ones with gaps, heaped low ones; none under a closed deck
		const float Catch = W.High.Cover * (1.f - 0.4f * W.High.Density) + 0.8f * W.Mid.Cover * (1.f - 0.6f * W.Mid.Density)
			+ 0.5f * W.Low.Cover * (1.f - W.Low.Sheet) * (1.f - 0.5f * W.Low.Density);
		A.Broken = Sat(Catch) * (1.f - A.Overcast);
		A.Rain = Sat(W.PrecipRate / 1.5f);
		// washed air: wet ground and nothing falling now, or a fresh wind mixing the haze out
		A.Clean = std::max(Sat((W.Wet - 0.15f) / 0.45f) * (1.f - A.Rain), 0.6f * Sat((W.WindSpeed - 7.f) / 7.f));
		const float Humid = Sat((W.Humidity - 0.72f) / 0.23f);
		const float Murk = Sat((22000.f - W.VisibilityM) / 18000.f);
		A.Haze = std::max(std::max(0.6f * Humid, Murk), std::max(W.Fog, 0.6f * W.Inversion)) * (1.f - 0.6f * A.Clean);
		if (ForceClean >= 0.f) A.Clean = Sat(ForceClean);
		if (ForceHaze >= 0.f) A.Haze = Sat(ForceHaze);
		return A;
	}

	FLook Look(const FDayProfile& P, const FAir& A, float SunAltDeg, float Strength)
	{
		FLook L;
		const float Wb = Band(SunAltDeg) * std::max(0.f, Strength);
		L.Weight = Wb;
		if (Wb <= 0.f)
		{
			return L;
		}

		// what the weather makes of the day's character
		const float Dull = Sat(std::max(std::max(P.Dull, 1.2f * A.Overcast - 0.15f), A.Rain));
		const float Aerosol = Sat(P.Aerosol + 0.6f * A.Haze - 0.45f * A.Clean);
		const float Hue = Sat(P.Hue + 0.25f * A.Haze);
		const float Vivid = (1.f + 0.35f * A.Clean - 0.3f * A.Haze) * (1.f - 0.75f * Dull);
		const float Colour = P.Intensity * (1.f - 0.8f * A.Overcast) * (1.f - 0.6f * A.Rain);
		const float Glow = P.CloudGlow * (0.55f + 1.1f * A.Broken) * (1.f - 0.75f * A.Overcast);

		// below the horizon: the clouds glow on for a while (afterglow), the ground light is gone already
		const float Below = Smooth(1.f, -2.f, SunAltDeg) * Smooth(-9.f, -3.f, SunAltDeg);
		const float AfterK = 1.f + 0.6f * P.Afterglow * Below;

		// the air itself (physical levers, the same at every hue)
		L.MieScale = 1.f + Wb * (1.7f * Aerosol - 0.45f);       // clean 0.55x .. thick haze 2.25x
		L.MieGAdd = Wb * (0.06f * Aerosol - 0.02f);
		L.MieAbsScale = 1.f + Wb * (1.5f * Aerosol + 4.f * Dull * Aerosol);
		L.MieHeightScale = 1.f + Wb * 0.9f * Aerosol;
		L.RayleighScale = 1.f + Wb * (0.12f * Hue + 0.08f * Aerosol) * Vivid;
		L.OzoneScale = 1.f + Wb * 1.6f * P.Ozone * std::max(0.f, Vivid);

		// the colour
		// a dull evening greys out whatever its colour would have been
		const float Tint = std::max(Sat(Wb * Colour), 0.75f * Dull * std::min(Wb, 1.f));
		const float V = std::max(0.f, Vivid);
		L.LightTint = Comp(Mix(Saturate(Along(LightHue, Hue), V), Grey, Dull), Tint * 0.85f, 0.5f);
		L.SkyTint = Comp(Mix(Saturate(Along(SkyHue, Hue), V), Grey, Dull), Tint * 0.9f, 0.4f);
		// the clouds: their own glow colour, brighter or dimmer by how much they catch the light; after the sun has set
		// the afterglow keeps the colour (more of the tint) while the light fails: dimmer, not brighter (with the night
		// exposure opening they blew out to cream)
		const FRGB CloudC = Mix(Saturate(Along(CloudHue, Hue), V), Grey, Dull);
		const float CloudLum = std::min(1.5f, std::max(0.3f, (0.75f + 0.25f * std::min(Glow, 2.f)) * (1.f - 0.35f * Dull))) * (1.f - 0.45f * Below);
		const float CloudW = Sat(Wb * std::max(Colour, 0.3f) * (0.45f + 0.55f * std::min(Glow, 1.5f)) * AfterK);
		L.CloudTint = Scale(Comp(CloudC, CloudW, 0.35f), Lerp(1.f, CloudLum, Wb));

		// the disk and its glow: hazy evenings a bigger softer aureole and a dimmer disk
		L.AureoleScale = 1.f + Wb * (0.9f * Aerosol - 0.25f);
		L.DiskScale = 1.f - Wb * (0.35f * Aerosol + 0.25f * Dull);
		// the eye: ALyudSky opens the exposure for the night from 8 degrees up (~3 stops at -4), which blew the glowing
		// twilight sky and the afterglow on the clouds out to cream whatever their colour; held back here while the band
		// lasts (gone by -9, where the night's exposure is wanted)
		const float Dusk = Smooth(1.f, -3.f, SunAltDeg);
		L.ExposureEV = -std::min(Wb, 1.f) * (0.4f + 1.1f * Dusk);
		return L;
	}
}
