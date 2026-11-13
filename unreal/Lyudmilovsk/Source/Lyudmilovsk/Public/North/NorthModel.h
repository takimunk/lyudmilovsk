// The North's small rules without Unreal types (docs/north.md "Game side"): when a thing wakes and sleeps, how a mud
// bubble swells and pops, when the camp's fire burns, how much steam a vent gives. Plain C++ so tools/north_sim can check
// it offline (tools/north_sim/run.sh model).
#pragma once

#include <cmath>
#include <cstdint>

namespace LyudNorthModel
{
	/** A seeded xorshift: the same bubbles every run for a given pot. */
	inline float Rand01(uint32_t& S)
	{
		S ^= S << 13;
		S ^= S >> 17;
		S ^= S << 5;
		return (S & 0xFFFFFFu) / float(0x1000000);
	}

	inline float Clamp01(float X) { return X < 0.f ? 0.f : (X > 1.f ? 1.f : X); }
	inline float Smooth(float E0, float E1, float X)
	{
		const float T = Clamp01((X - E0) / (E1 - E0));
		return T * T * (3.f - 2.f * T);
	}

	/** Awake (ticks, steams) and near (sounds) with a margin, so a hero standing on the edge does not flicker it. */
	struct FRanges
	{
		float WakeM = 300.f;
		float NearM = 70.f;
		float MarginM = 25.f;
	};
	inline bool NextAwake(bool bAwake, float DistM, const FRanges& R)
	{
		return bAwake ? DistM < R.WakeM + R.MarginM : DistM < R.WakeM;
	}
	inline bool NextNear(bool bNear, float DistM, const FRanges& R)
	{
		return bNear ? DistM < R.NearM + R.MarginM * 0.4f : DistM < R.NearM;
	}

	/** One bubble of a mud pot: rests, swells for Swell s (a dome growing to Peak of its size), plops. */
	struct FBubble
	{
		float T = 0.f;          // seconds into the current phase
		float Rest = 1.f;       // seconds of the flat surface before it rises
		float Swell = 1.5f;     // seconds it swells
		float Peak = 1.f;       // 0..1 of the biggest bubble this pot makes
		float U = 0.f, V = 0.f; // where on the pool, -1..1 of its radius
		bool bSwelling = false;
	};

	/** Picks the next bubble's place and timing (somewhere inside 70% of the pool, so it never pops on the rim). */
	inline void NewBubble(FBubble& B, uint32_t& Seed)
	{
		const float A = Rand01(Seed) * 6.2831853f, Rr = 0.7f * std::sqrt(Rand01(Seed));
		B.U = Rr * std::cos(A);
		B.V = Rr * std::sin(A);
		B.Rest = 0.3f + 2.7f * Rand01(Seed);
		B.Swell = 0.8f + 1.6f * Rand01(Seed);
		B.Peak = 0.45f + 0.55f * Rand01(Seed);
		B.T = 0.f;
		B.bSwelling = false;
	}

	/** Advances a bubble; returns true on the frame it pops (a plop and a little steam). */
	inline bool StepBubble(FBubble& B, float Dt, uint32_t& Seed)
	{
		B.T += Dt;
		if (!B.bSwelling)
		{
			if (B.T >= B.Rest)
			{
				B.bSwelling = true;
				B.T = 0.f;
			}
			return false;
		}
		if (B.T >= B.Swell)
		{
			NewBubble(B, Seed);
			return true;
		}
		return false;
	}

	/** The dome's scale now: 0 flat .. Peak, slow at first, quick just before it bursts; a little wobble. */
	inline float BubbleScale(const FBubble& B)
	{
		if (!B.bSwelling)
		{
			return 0.f;
		}
		const float P = Clamp01(B.T / B.Swell);
		return B.Peak * std::pow(P, 0.6f) * (1.f + 0.06f * std::sin(B.T * 17.f));
	}

	/** The camp's fire by the schedule: from dusk (the sun under LightAlt degrees) until OutHour after midnight. A lit
	 *  fire keeps burning until the sun is a degree over LightAlt again (no flicker at the threshold). */
	inline bool FireScheduled(bool bWasLit, float SunAltDeg, float Hour, float LightAlt, float OutHour)
	{
		const bool bDark = bWasLit ? SunAltDeg < LightAlt + 1.f : SunAltDeg < LightAlt;
		// the evening and the night up to OutHour; never in the small hours after it, nor by day
		const bool bHours = Hour >= 12.f || Hour < OutHour;
		return bDark && bHours;
	}

	/** Puffs a second a vent gives (Size 0.7..1.8), times the air's density (1 normal, up to ~2 cold and damp). */
	inline float VentRate(float Size, float Density) { return (2.5f + 2.5f * Size) * (0.7f + 0.3f * Density); }
	/** The jet's starting speed, cm/s, and the drag that stops it: a column of ~6..15 m before it drifts. */
	inline float VentSpeed(float Size) { return 250.f + 220.f * Size; }
	inline float VentDrag() { return 0.45f; }
	/** How high the jet itself climbs (speed / drag), m: what the docs promise (6..15 m with buoyancy on top). */
	inline float VentColumnM(float Size) { return VentSpeed(Size) / VentDrag() / 100.f; }

	/** The air's density for steam: cold and humid air keeps it visible longer (1 = 15 C, 60 %). */
	inline float SteamDensity(float TempC, float Humidity)
	{
		const float Cold = Clamp01((15.f - TempC) / 20.f);
		const float Damp = Clamp01((Humidity - 0.6f) / 0.4f);
		return 1.f + 0.7f * Cold + 0.4f * Damp;
	}

	/** Hot ground round a fumarole, metres (quests: "don't step there"). Size as north.json gives it. */
	inline float HotRadiusM(int Kind, float Size)
	{
		switch (Kind)
		{
		case 0: return 1.5f + 2.5f * Size;      // vent
		case 1: return 1.5f + 1.5f * Size;      // steaming ground
		case 2: return 0.5f * Size + 0.8f;      // mud pot: its pool and a rim
		default: return 0.8125f * Size + 0.5f;  // spring: 2.6 m pool at size 3.2
		}
	}
}
