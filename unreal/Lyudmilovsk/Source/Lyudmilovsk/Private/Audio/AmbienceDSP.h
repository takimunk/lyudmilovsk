// The outdoor world's procedural sound, in plain C++ (no Unreal types) so it can be rendered to WAV and checked outside
// the engine (tools/audiosim/run.sh). ULyudSoundscapeSubsystem feeds it the world's state through a USynthComponent;
// ULyudWindAudioComponent plays the wind voice alone (the bird's flight).
//
// Every voice is synthesised from noise and a few oscillators, shaped by what makes the real sound:
//   - wind: turbulence round the ears and against things (a low rumble and a broad whoosh that rise with the speed,
//     breathe with the gusts), whistling in edges and wires in a strong wind, the rush of the air past a flying head;
//   - leaves: thousands of small flutters (grains of bright noise) whose rate and loudness follow the wind in the
//     crowns, crisper when the leaves are dry, a smooth hiss from pine needles;
//   - surf: each wave breaking (a crash that darkens as it rolls, then the long hiss of the wash running up the sand
//     and back), in sets of bigger and smaller waves at the sea's period, small slaps on a calm day, a roar from afar;
//   - rain: a bed of countless far drops and single near ones (ticks on the ground, plips in puddles, patters on
//     leaves), drumming and muffled on a roof overhead; snow nearly silent;
//   - crickets: field crickets chirping at the rate their temperature sets (Dolbear's law), each in its own place;
//   - thunder: a crack (a train of claps from the channel's kinks) when near, a long rolling rumble that comes later
//     and darker the farther the strike;
//   - the weather's own voices (WeatherVoices.h, AmbienceWeather.cpp): hail, umbrella, windows, blizzard, the hush of fresh snow and fog,
//     drips, iced twigs, sea ice, the lighthouse's fog signal, footsteps on snow, slush, puddles and ice.
// Stereo: directions are given as pans (-1 left .. 1 right of the listener's view), wider sources decorrelated.
#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace LyudAudio
{
	constexpr float Pi = 3.14159265358979f;

	/** xorshift32: cheap, deterministic noise per voice. */
	struct FRng
	{
		uint32_t S = 0x9E3779B9u;
		explicit FRng(uint32_t Seed = 1) : S(Seed * 2654435761u + 0x9E3779B9u) { if (!S) S = 1; }
		uint32_t Next() { S ^= S << 13; S ^= S >> 17; S ^= S << 5; return S; }
		/** -1 .. 1 */
		float White() { return float(int32_t(Next())) * (1.f / 2147483648.f); }
		/** 0 .. 1 */
		float Uniform() { return float(Next() >> 8) * (1.f / 16777216.f); }
	};

	/** Pink noise (Paul Kellet's filter on white). */
	struct FPink
	{
		float B[7] = {};
		float Process(float W);
	};

	/** Brown noise (leaky integrated white). */
	struct FBrown
	{
		float Y = 0.f;
		float Process(float W) { Y = 0.997f * Y + 0.06f * W; return Y; }
	};

	/** Topology-preserving state variable filter: low, band and high pass of one input. */
	struct FSvf
	{
		float Ic1 = 0.f, Ic2 = 0.f;
		float G = 0.f, K = 1.f, A1 = 0.f, A2 = 0.f, A3 = 0.f;
		float Low = 0.f, Band = 0.f, High = 0.f;
		void Set(float CutoffHz, float Q, float SampleRate);
		void Process(float X);
	};

	/** One-pole smoothing toward a target, per sample. */
	struct FSmooth
	{
		float Y = 0.f, Coef = 0.f;
		void Init(float Seconds, float SampleRate, float Start = 0.f);
		float Step(float Target) { Y += Coef * (Target - Y); return Y; }
	};

	/** A smoothly wandering random value -1..1 (cubic between random points at Rate per second). */
	struct FDrift
	{
		FRng R;
		float A = 0.f, B = 0.f, C = 0.f, D = 0.f, T = 0.f;
		explicit FDrift(uint32_t Seed = 1) : R(Seed) { A = R.White(); B = R.White(); C = R.White(); D = R.White(); }
		float Step(float RateHz, float SampleRate);
	};

	/** Equal-power pan of a mono sample into a stereo pair, Pan -1 .. 1. */
	inline void PanTo(float X, float Pan, float Width, float& L, float& R);

	enum EVoice { Wind = 0, Leaves, Surf, Rain, Crickets, Thunder, Snow, Drips, Ice, Signal, Steps, NumVoices };

	/** What the world sounds like here and now; set from the game thread, read by the audio thread. */
	struct FAmbienceParams
	{
		// wind at the ear, m/s (outdoors at head height, gusts included), how gusty, and where it comes from
		float WindSpeed = 0.f;
		float WindGustiness = 0.25f;
		float WindPan = 0.f;
		/** Speed of the air past the head from the listener's own motion, m/s (a flying bird; 0 on foot). */
		float AirSpeed = 0.f;
		/** Leafy crowns around, 0 .. 1 (by the trees near and their leaves in this season), how dry they are, pine needles. */
		float Leaves = 0.f;
		float LeafDry = 0.f;
		float Needles = 0.f;
		/** Height of the breaking waves at the nearest beach, m, their period, s, how far the surf line is, m, and its pan. */
		float SurfHeight = 0.f;
		float SurfPeriod = 6.f;
		float SurfDistance = 1000.f;
		float SurfPan = 0.f;
		/** Rain, mm/h; share of it that falls as snow; 0 open sky .. 1 a roof right overhead. */
		float Rain = 0.f;
		float Snow = 0.f;
		float RainRoof = 0.f;
		/** What falls besides plain rain: share of drizzle (fine drops, a soft mist hiss), hail or graupel 0..1, how
		 *  showery (rain coming in waves with the gusts) 0..1. */
		float Drizzle = 0.f;
		float Hail = 0.f;
		float Showery = 0.f;
		/** An umbrella over the listener 0..1; indoors by windows 0..1 (rain ticks on the glass). */
		float Umbrella = 0.f;
		float Windows = 0.f;
		/** Snow lying: level depth cm and density kg/m3 (fresh fluff hushes the world, old crust doesn't). */
		float SnowDepthCm = 0.f;
		float SnowDensity = 100.f;
		/** Fog 0..1 (dulls the far sounds); drips off the trees around (after rain, in fog) 0..1; thaw 0..1 (eaves
		 *  dripping); glaze on the twigs around 0..1 (they clink in the wind). */
		float Fog = 0.f;
		float Drip = 0.f;
		float Melt = 0.f;
		float Glaze = 0.f;
		/** Sea ice in hearing: cover 0..1, thickness cm, how far its edge is, m, its pan, the air's change C per hour
		 *  (cooling cracks it), the swell under it, m (grinding floes). */
		float SeaIce = 0.f;
		float IceCm = 0.f;
		float IceDistance = 1000.f;
		float IcePan = 0.f;
		float IceTempRate = 0.f;
		float IceSwell = 0.f;
		/** The lighthouse's fog signal: 0 silent .. 1 sounding, how far, m, and its pan. */
		float FogSignal = 0.f;
		float FogSignalDistance = 2000.f;
		float FogSignalPan = 0.f;
		/** The ground under the feet (Footstep): snow there, cm (-1: the level SnowDepthCm; drifts and cleared paths
		 *  differ), wet 0..1, puddles 0..1, standing on ice 0..1. */
		float UnderfootSnowCm = -1.f;
		float Wet = 0.f;
		float Puddles = 0.f;
		float OnIce = 0.f;
		/** Crickets: 0 .. 1 how many sing, and the air temperature that paces them, C. */
		float Crickets = 0.f;
		float TempC = 15.f;
		/** 0 outdoors .. 1 deep indoors: the outside comes through walls muffled and quieter. */
		float Enclosure = 0.f;
		float Master = 1.f;
		/** Per-voice gains (tests, mixing), by EVoice. */
		float Gain[NumVoices] = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
	};

	/** Wind: rumble, whoosh, whistle and the rush of flight. Usable alone (ULyudWindAudioComponent). */
	class FWindVoice
	{
	public:
		void Init(float SampleRate, uint32_t Seed);
		/** Adds Num stereo samples. Speed = wind at the ear m/s, Gustiness 0..0.5, Pan of where it comes from, AirSpeed m/s. */
		void Render(float* L, float* R, int Num, float Speed, float Gustiness, float Pan, float AirSpeed, float Gain);
		/** Level the wind is at now (0..~1.5), for meters and tests. */
		float Level() const { return LastLevel; }

	private:
		float Rate = 48000.f;
		FRng NL, NR, NW;
		FPink PL, PR;
		FBrown BL, BR, BRush;
		FSvf BodyL, BodyR, AirL, AirR, RumbleL, RumbleR, Whistle1, Whistle2, RushLow, RushHigh;
		FDrift Turb1, Turb2, Turb3, WhistleDrift, Buffet;
		FSmooth SSpeed, SGust, SPan, SAir;
		int Counter = 0;
		float LastLevel = 0.f;
	};

}

#include "WeatherVoices.h"

namespace LyudAudio
{
	/** The whole outdoor bed. */
	class FAmbience
	{
	public:
		void Init(float SampleRate, uint32_t Seed = 1);
		/** Adds Num stereo frames (separate channels) for these params. */
		void Render(float* L, float* R, int Num, const FAmbienceParams& P);
		/** A lightning strike DistanceM away (the crack and rumble arrive after the sound's travel), at Pan. */
		void Strike(float DistanceM, float Pan);
		/** A footstep of the listener now (Strength ~1 a walk, ~1.4 a run): its sound comes from the ground in the
		 *  params (snow, slush, puddles, ice, wet or dry earth). */
		void Footstep(float Strength, float Pan) { WeatherV.Footstep(Strength, Pan); }
		/** RMS of each voice over the last render (tests). */
		float VoiceRms[NumVoices] = {};

	private:
		struct FGrain { float Amp = 0.f, Decay = 0.f, Pan = 0.f, Freq = 0.f, Phase = 0.f, Ring = 0.f; FSvf F; int Kind = 0; int Wait = 0; };
		struct FBreak { float T = 0.f, Size = 1.f, Crash = 1.f, Wash = 4.f; FSvf F, Fw; bool bLive = false; };
		struct FCricket { float Pan = 0.f, Freq = 4600.f, Phase = 0.f, Clock = 0.f, RateJit = 1.f, Gain = 1.f; int Pulse = 0; };
		struct FStrike { float Delay = 0.f, Dist = 1000.f, Pan = 0.f, T = 0.f; float Claps[5] = {}; FSvf F, Fc; FDrift Roll{ 3 }; bool bLive = false; };

		float Rate = 48000.f;
		FRng R;
		FDrift ShowerDrift{ 31 };
		FWindVoice WindV;
		FWeatherVoices WeatherV;
		// leaves
		FSvf LeafHp, LeafHp2, NeedleBp;
		FDrift LeafSwell{ 11 };
		std::vector<FGrain> Grains;
		// surf
		std::vector<FBreak> Breaks;
		float NextBreak = 1.f, SetPhase = 0.f;
		FSvf RoarL, RoarR, SlapF;
		FPink RoarPL, RoarPR;
		FDrift RoarDrift{ 21 };
		float SlapEnv = 0.f, SlapPan = 0.f;
		// rain
		FSvf RainBedL, RainBedR, RainLowL, RainLowR, RoofL, RoofR;
		FPink RainPL, RainPR;
		// crickets
		std::vector<FCricket> Crick;
		// thunder
		std::vector<FStrike> Strikes;
		FBrown ThunderB;
		// enclosure (muffling through walls)
		FSvf WallL, WallR;
		FSmooth SEnc, SRain, SLeaves, SCrick, SSurfH, SSurfD, SSurfPan;
		float Time = 0.f;
		std::vector<float> Tmp;

		void RenderLeaves(float* L, float* R, int Num, const FAmbienceParams& P);
		void RenderSurf(float* L, float* R, int Num, const FAmbienceParams& P);
		void RenderRain(float* L, float* R, int Num, const FAmbienceParams& P);
		void RenderCrickets(float* L, float* R, int Num, const FAmbienceParams& P);
		void RenderThunder(float* L, float* R, int Num, const FAmbienceParams& P);
		void AddGrain(int Kind, float Amp, float Pan, float Freq, float DecayS, float Q, int Wait);
	};

	inline void PanTo(float X, float Pan, float Width, float& L, float& R)
	{
		const float P = Pan < -1.f ? -1.f : (Pan > 1.f ? 1.f : Pan);
		const float A = (P * Width + 1.f) * 0.25f * Pi;
		L += X * std::cos(A);
		R += X * std::sin(A);
	}
}
