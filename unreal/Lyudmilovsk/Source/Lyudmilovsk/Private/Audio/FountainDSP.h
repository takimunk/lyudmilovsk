// The sound of a fountain, in plain C++ like the outdoor bed (AmbienceDSP.h), so it can be rendered outside the engine
// (tools/audiosim/run.sh fountain). Mono: ALyudFountain plays it from the basin through a spatialised USynthComponent.
//
// What makes it (water falling into water, a few tens of centimetres):
//   - the wash: countless far drops merge into a broad noise, brightest round 1-3 kHz, breathing as the veil wavers;
//   - bubbles: each drop that hits the surface traps a bubble that rings at its Minnaert frequency (f ~ 3.26 / r Hz,
//     r in metres: 1 mm sings at ~3.3 kHz, 6 mm at ~550 Hz) for a few dozen cycles, its pitch rising as it shrinks;
//   - impacts: the slap of each drop, a tick of bright noise;
//   - a low burble of the jet's column falling on the upper bowl.
#pragma once

#include "AmbienceDSP.h"

namespace LyudAudio
{
	class FFountainVoice
	{
	public:
		void Init(float SampleRate, uint32_t Seed);
		/** Adds Num mono samples. Flow 0 (off) .. 1 (running), eased inside so turning it on or off swells. */
		void Render(float* Out, int Num, float Flow, float Gain);
		/** RMS of the last render (tests). */
		float Rms = 0.f;

	private:
		struct FBubble { float Amp = 0.f, Decay = 0.f, Freq = 0.f, Phase = 0.f, Rise = 1.f; };
		struct FTick { float Amp = 0.f, Decay = 0.f; FSvf F; };
		static constexpr int MaxBubbles = 48;
		static constexpr int MaxTicks = 24;

		float Rate = 48000.f;
		FRng R;
		FPink Pink;
		FBrown Brown;
		FSvf Wash, Hiss, Burble;
		FDrift Waver{ 51 }, Flutter{ 52 }, BurbleDrift{ 53 };
		FSmooth SFlow;
		FBubble Bubbles[MaxBubbles];
		FTick Ticks[MaxTicks];
		int NumBubbles = 0, NumTicks = 0;
	};
}
