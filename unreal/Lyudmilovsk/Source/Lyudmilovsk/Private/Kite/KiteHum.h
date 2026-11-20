// The kite line's voice, in plain C++ (no Unreal types; tools/kitesim/run.sh checks it outside the engine).
//
// A taut thin line in the wind sings: the air shedding off it in vortices at f = 0.2 v / d (the aeolian tone; a 2 mm
// braided line in 8 m/s sings near 800 Hz) with a weaker octave, wavering as the gusts and the line's own swing change the air
// over it; under it, the sail's flutter, a soft low thrum of filtered noise that grows with the air. A slack line or a
// kite on the ground is silent. The level follows Hum (0..1, LyudKite::Hum), the pitch the apparent air.
#pragma once

#include <cmath>
#include <cstdint>

namespace LyudKiteHum
{
	struct FVoice
	{
		float Rate = 48000.f;
		float Phase = 0.f, Phase2 = 0.f;
		float Freq = 600.f;         // eased tone, Hz
		float Level = 0.f;          // eased level
		float Lp = 0.f, Lp2 = 0.f;  // the thrum's filter
		float Wobble = 0.f, WobbleT = 0.f;
		uint32_t Rng = 22695477u;
		float Rms = 0.f;

		void Init(float SampleRate, uint32_t Seed)
		{
			Rate = SampleRate > 1000.f ? SampleRate : 48000.f;
			Rng = Seed * 747796405u + 2891336453u;
		}

		float Noise()
		{
			Rng = Rng * 1664525u + 1013904223u;
			return float((Rng >> 9) & 0x7FFFFF) / float(0x7FFFFF) * 2.f - 1.f;
		}

		/** Adds Num mono samples to Out. Hum 0..1, Air m/s over the line, Gain the settings' level. */
		void Render(float* Out, int Num, float Hum, float Air, float Gain)
		{
			const float TwoPi = 6.2831853f;
			const float WantF = 0.2f * (Air < 1.f ? 1.f : Air) / 0.002f;
			const float Want = (Hum < 0.f ? 0.f : (Hum > 1.f ? 1.f : Hum)) * Gain;
			const float EaseF = 1.f - std::exp(-1.f / (0.25f * Rate));
			const float EaseL = 1.f - std::exp(-1.f / (0.08f * Rate));
			const float Thrum = 1.f - std::exp(-TwoPi * 90.f / Rate);
			double Sum = 0.0;
			for (int I = 0; I < Num; ++I)
			{
				// the gusts' waver: a slow random walk of the tone (a few per cent) and of its loudness
				WobbleT += 1.f / Rate;
				if (WobbleT > 0.05f)
				{
					WobbleT = 0.f;
					Wobble = Wobble * 0.92f + Noise() * 0.08f;
				}
				Freq += (WantF * (1.f + 0.04f * Wobble) - Freq) * EaseF;
				Level += (Want - Level) * EaseL;
				if (Level < 1e-5f && Want <= 0.f)
				{
					continue;
				}
				Phase += Freq / Rate;
				Phase -= std::floor(Phase);
				Phase2 += 2.f * Freq / Rate;
				Phase2 -= std::floor(Phase2);
				const float Swell = 0.75f + 0.25f * Wobble;
				const float Tone = std::sin(TwoPi * Phase) * 0.5f + std::sin(TwoPi * Phase2) * 0.14f;
				Lp += (Noise() - Lp) * Thrum;
				Lp2 += (Lp - Lp2) * Thrum;
				const float S = Level * (Tone * 0.22f * Swell * Level + Lp2 * 0.9f);
				Out[I] += S;
				Sum += double(S) * S;
			}
			Rms = Num > 0 ? float(std::sqrt(Sum / Num)) : 0.f;
		}
	};
}
