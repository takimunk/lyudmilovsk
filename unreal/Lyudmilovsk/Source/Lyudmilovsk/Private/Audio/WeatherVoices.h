// The weather's own voices on top of the outdoor bed (AmbienceDSP.h), plain C++ like it, rendered by FAmbience:
//   - what falls: hail knocking (harder and lower on a roof), the drum of drops on a taut umbrella and the trickle off
//     its rim, rain ticking on the window glass when indoors;
//   - snow: the hiss of snow driven by a strong wind (blizzard, and snow lifted off the ground in a gale), the hush of
//     fresh snow lying (soft snow swallows the highs of everything around), fog dulling the far sounds the same way;
//   - water and ice on things: drips from the trees after rain and in fog, eaves dripping in a thaw (each drip point
//     keeping its own beat), iced twigs clinking in the wind after freezing rain;
//   - sea ice: "singing" chirps (the flexural waves in thin ice are dispersive, so a crack far off arrives as a whistle
//     falling in pitch), booms of thick ice cracking as the night cools it, floes grinding and brash ice clinking in a
//     swell when the sea is only partly frozen;
//   - the lighthouse's fog signal: a diaphone (a deep tone that ends in its grunt), two blasts a minute, from where the
//     lighthouse is, darker the farther, echoing off the coast;
//   - footsteps on the weather's ground: snow crunching (squeaking when it is very cold and dry, a soft "pff" in fresh
//     fluff), slush squelching, puddles splashing, ice clicking, wet and dry earth.
// Everything is deterministic from the seed and the parameters. Included by AmbienceDSP.h (it needs its types first).
#pragma once

#include <cstdint>
#include <vector>

namespace LyudAudio
{
	class FWeatherVoices
	{
	public:
		void Init(float SampleRate, uint32_t Seed);
		/** Adds the voices heard from outside (go through the walls with the rest of the bed) into L/R, each voice's
		 *  energy into Energy[Snow..Signal] (sum of squares). */
		void RenderOutside(float* L, float* R, int Num, const FAmbienceParams& P, double Energy[NumVoices]);
		/** The muffling of the outside by fresh snow and fog (in place, before the walls). */
		void Hush(float* L, float* R, int Num, const FAmbienceParams& P);
		/** Adds the voices right at the listener (footsteps, window ticks) after the walls. */
		void RenderNear(float* L, float* R, int Num, const FAmbienceParams& P, double Energy[NumVoices]);
		/** A footstep now: Strength ~1 a walk, ~1.4 a run, the foot's pan (-1 .. 1, small). */
		void Footstep(float Strength, float Pan);
		/** 0..1 how muffled the outside is now (tests). */
		float Muffle() const { return SHush.Y; }

	private:
		struct FPing
		{
			float Amp = 0.f, Decay = 0.f, Freq = 0.f, Glide = 1.f, Phase = 0.f, Pan = 0.f;
			FSvf F;
			int Kind = 0;      // 0 a sine (a bubble when Glide > 1, a ping of glass or ice), 1 a resonant click of noise
			int Wait = 0;
			bool bNear = false;
			int Voice = 0;
		};
		struct FStep
		{
			bool bLive = false;
			int Kind = 0;
			float T = 0.f, Strength = 1.f, Pan = 0.f, Len = 0.3f;
			FSvf Crunch, Body, Sq1, Sq2, Hi;
		};
		struct FChirp
		{
			bool bLive = false;
			int Kind = 0;      // 0 a singing chirp, 1 a boom
			float Wait = 0.f, T = 0.f, Dur = 0.3f, F0 = 3000.f, F1 = 300.f, Phase = 0.f, Amp = 0.f, Pan = 0.f;
			FSvf F;
		};
		struct FEave { float Clock = 0.f, Period = 1.f, Pan = 0.f, Gain = 1.f; int Kind = 0; };

		float Rate = 48000.f;
		FRng R{ 3 };
		std::vector<FPing> Pings;
		std::vector<FStep> StepPool;
		std::vector<FChirp> Chirps;
		std::vector<FEave> Eaves;
		// blizzard
		FSvf BlizL, BlizR, BlizLowL, BlizLowR;
		FDrift BlizGust{ 41 }, BlizFlutter{ 42 };
		// hush
		FSvf HushL, HushR;
		// sea ice grinding
		FSvf GrindF, BrashF;
		FDrift GrindRate{ 51 }, GrindSwell{ 52 };
		float GrindPhase = 0.f;
		// the fog signal: phase of its tone, its minute's clock, a coast echo
		float SigPhase = 0.f, SigClock = 50.f;
		FSvf SigF;
		std::vector<float> Echo;
		int EchoAt = 0;
		FSmooth SBliz, SHush, SDrip, SMelt, SGlaze, SIce, SSignal, SGrind;

		void AddPing(int Kind, float Amp, float Pan, float Freq, float DecayS, float Q, float Glide, int Wait, bool bNear, int Voice);
		void RenderPings(float* L, float* R, int Num, bool bNear, double Energy[NumVoices]);
		void Spawn(const FAmbienceParams& P, int Num);
		void SpawnStepGrains(int Kind, float Strength, float Pan);
	};
}
