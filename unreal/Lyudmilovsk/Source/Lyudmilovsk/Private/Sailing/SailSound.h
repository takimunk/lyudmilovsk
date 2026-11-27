// The yacht school's sound mixing, in plain C++ (no Unreal types; tools/sailsoundsim/run.sh checks it outside the engine).
//
// The sounds themselves are WAVs made by tools/sailing/sounds.py (audio/sfx/sailing -> /Game/Audio/Sailing/S_<pool>_0N,
// the soundscape's Sailing pools). This header only says, from a boat's telemetry, how loud and at what pitch each loop
// plays and when the one-shots fire, so the engine (ALyudDinghy, the launch, the boat park) drives them consistently:
//
//   MixLoops(T)        -> gain 0..1 and pitch (playback rate) of the dinghy's loops (luff x3, leech, hull x4, slosh)
//   StepEvents(S,T,..) -> which one-shots fire this tick (sail fill, rope/mast creak, wave slaps, spray)
//   MixOutboard(rpm)   -> the launch's four engine loops crossfaded by pitch (these play at 0.65..1.6)
//   HalyardRate(wind), MixHalyards(wind), MixFlags(wind) -> the boat park and the signal mast
//
// Gains are the loop's share before the settings' volume and attenuation; ease them (~0.15 s) in the engine. The boat's
// pitches stay within 0.7..1.45 so the loops never sound sped-up.
#pragma once

#include <cmath>

namespace LyudSail
{
namespace Sound
{
	/** The dinghy's loops, in the order of FLoopMix's arrays; LoopPool names the sound pool of each. */
	enum ELoop : int
	{
		LuffLight,
		LuffMid,
		LuffHard,
		Leech,
		HullGhost,
		HullChuckle,
		HullPlane,
		HullHum,
		CockpitSlosh,
		NumLoops
	};

	inline const char* LoopPool(int Loop)
	{
		static const char* const Names[NumLoops] = {"sail_luff_light", "sail_luff_mid", "sail_luff_hard", "sail_leech",
			"sail_hull_ghost", "sail_hull_chuckle", "sail_hull_plane", "sail_hull_hum", "sail_cockpit_slosh"};
		return (Loop >= 0 && Loop < NumLoops) ? Names[Loop] : "";
	}

	/** What the boat is doing; all of it from the sail model / the dinghy each frame. */
	struct FTelemetry
	{
		float SpeedMs = 0.f;      // speed through the water, m/s
		float Luff = 0.f;         // 0 = the sail full and drawing .. 1 = flogging head to wind / let fly
		float HeelDeg = 0.f;      // heel, degrees (either side)
		float SheetLoad = 0.f;    // mainsheet load 0..1 (1 = the most a sailor can hold)
		bool bPlaning = false;    // the hull is up on the plane
		float WindMs = 0.f;       // apparent wind over the boat, m/s
		float WaveM = 0.3f;       // significant wave height round the boat, m (ALyudSea)
		float WaterAboard = 0.f;  // 0 = dry .. 1 = swamped
	};

	struct FLoopMix
	{
		float Gain[NumLoops] = {};
		float Pitch[NumLoops] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f};
	};

	inline float Clamp(float X, float Lo, float Hi)
	{
		return X < Lo ? Lo : (X > Hi ? Hi : X);
	}

	/** 0 below A, 1 above B, smooth between (A > B fades the other way). */
	inline float Smooth(float A, float B, float X)
	{
		const float U = Clamp((X - A) / (B - A), 0.f, 1.f);
		return U * U * (3.f - 2.f * U);
	}

	/** A triangle weight centred on C reaching zero at C -/+ W (for crossfading neighbours that sum to 1). */
	inline float Tri(float C, float W, float X)
	{
		return Clamp(1.f - std::fabs(X - C) / W, 0.f, 1.f);
	}

	/** How hard the sail flogs, 0..1: the luff, scaled by the wind that drives it. */
	inline float FlogStrength(const FTelemetry& T)
	{
		return Clamp(T.Luff, 0.f, 1.f) * (0.3f + 0.7f * Smooth(2.f, 10.f, T.WindMs));
	}

	inline FLoopMix MixLoops(const FTelemetry& T)
	{
		FLoopMix M;
		const float V = T.SpeedMs > 0.f ? T.SpeedMs : 0.f;
		const float W = T.WindMs > 0.f ? T.WindMs : 0.f;
		const float Heel = Clamp(std::fabs(T.HeelDeg) / 30.f, 0.f, 1.f);

		// the sail luffing: three strengths crossfaded by how hard it flogs; faster flutter in more wind
		const float S = FlogStrength(T);
		const float On = Smooth(0.06f, 0.22f, T.Luff);
		float WL = S <= 0.15f ? 1.f : Tri(0.15f, 0.35f, S);
		float WM = Tri(0.5f, 0.35f, S);
		float WH = S >= 0.85f ? 1.f : Tri(0.85f, 0.35f, S);
		const float Sum = WL + WM + WH;
		WL /= Sum;
		WM /= Sum;
		WH /= Sum;
		const float FlutterPitch = Clamp(0.85f + 0.035f * W, 0.85f, 1.25f);
		M.Gain[LuffLight] = On * std::sqrt(WL) * 0.8f;
		M.Gain[LuffMid] = On * std::sqrt(WM) * 0.9f;
		M.Gain[LuffHard] = On * std::sqrt(WH);
		M.Pitch[LuffLight] = M.Pitch[LuffMid] = M.Pitch[LuffHard] = FlutterPitch;

		// the leech buzzing at speed with the sail drawing
		M.Gain[Leech] = Smooth(1.5f, 4.5f, V) * Smooth(2.f, 7.f, W) * (1.f - Smooth(0.2f, 0.5f, T.Luff)) * 0.7f;
		M.Pitch[Leech] = Clamp(0.9f + 0.06f * V, 0.9f, 1.3f);

		// the hull: ghosting trickle -> displacement chuckle -> planing hiss, plus the foils' hum
		const float Plane = T.bPlaning ? Smooth(2.6f, 4.0f, V) : 0.35f * Smooth(3.2f, 5.5f, V);
		M.Gain[HullGhost] = Smooth(0.05f, 0.6f, V) * (1.f - Smooth(1.1f, 2.0f, V)) * 0.8f;
		M.Gain[HullChuckle] = Smooth(0.7f, 1.6f, V) * (1.f - 0.75f * Plane) * (1.f - Smooth(4.0f, 6.0f, V) * 0.5f) *
			(0.85f + 0.15f * Heel);
		M.Gain[HullPlane] = Plane * (0.9f + 0.1f * Heel);
		M.Gain[HullHum] = Smooth(3.5f, 5.5f, V) * (T.bPlaning ? 0.6f : 0.15f);
		M.Pitch[HullGhost] = Clamp(0.9f + 0.2f * V, 0.9f, 1.2f);
		M.Pitch[HullChuckle] = Clamp(0.8f + 0.12f * V, 0.8f, 1.25f);
		M.Pitch[HullPlane] = Clamp(0.8f + 0.06f * V, 0.85f, 1.3f);
		M.Pitch[HullHum] = Clamp(V / 4.5f, 0.7f, 1.45f);

		// water in the cockpit sloshing, slower the more there is
		const float Wa = Clamp(T.WaterAboard, 0.f, 1.f);
		M.Gain[CockpitSlosh] = Smooth(0.03f, 0.4f, Wa);
		M.Pitch[CockpitSlosh] = 1.1f - 0.25f * Wa;

		for (int I = 0; I < NumLoops; ++I)
		{
			M.Gain[I] = Clamp(M.Gain[I], 0.f, 1.f);
			M.Pitch[I] = Clamp(M.Pitch[I], 0.7f, 1.45f);
		}
		return M;
	}

	/** One-shots, as bits of StepEvents' result; the pool of each in EventPool. */
	enum EEvent : unsigned
	{
		EvFill = 1u << 0,           // sail_fill
		EvRopeCreak = 1u << 1,      // sail_rope_creak
		EvMastCreak = 1u << 2,      // sail_mast_creak
		EvWaveSlapSmall = 1u << 3,  // sail_wave_slap_small
		EvWaveSlapLarge = 1u << 4,  // sail_wave_slap_large
		EvSpray = 1u << 5,          // sail_spray (with a large slap at speed in a breeze)
		EvRudderKnock = 1u << 6     // sail_rudder_knock
	};
	constexpr int NumEvents = 7;

	inline const char* EventPool(unsigned Bit)
	{
		static const char* const Names[NumEvents] = {"sail_fill", "sail_rope_creak", "sail_mast_creak",
			"sail_wave_slap_small", "sail_wave_slap_large", "sail_spray", "sail_rudder_knock"};
		for (int I = 0; I < NumEvents; ++I)
		{
			if (Bit == (1u << I))
			{
				return Names[I];
			}
		}
		return "";
	}

	struct FEventState
	{
		float LuffPeak = 0.f;  // the highest luff since the sail last drew
		float PeakAge = 0.f;   // seconds since the luff was last over 0.5
		float Quiet = 0.f;     // seconds since the last creak (creaks never crowd)
	};

	/** The one-shots that fire in this tick of Dt seconds. R1, R2, R3: the caller's uniform randoms 0..1 (seeded per
	 *  boat, so a replay sounds the same). Gains for them: FillGain, SlapGain. */
	inline unsigned StepEvents(FEventState& S, const FTelemetry& T, float Dt, float R1, float R2, float R3)
	{
		unsigned Out = 0;
		const float V = T.SpeedMs > 0.f ? T.SpeedMs : 0.f;
		// the sail fills: the luff falls from a flog (> 0.5) to drawing (< 0.2) within 0.6 s
		if (T.Luff > 0.5f)
		{
			S.LuffPeak = T.Luff > S.LuffPeak ? T.Luff : S.LuffPeak;
			S.PeakAge = 0.f;
		}
		else
		{
			S.PeakAge += Dt;
			if (S.LuffPeak > 0.5f && T.Luff < 0.2f && S.PeakAge < 0.6f && T.WindMs > 2.f)
			{
				Out |= EvFill;
				S.LuffPeak = 0.f;
			}
			if (S.PeakAge >= 0.6f)
			{
				S.LuffPeak = 0.f;
			}
		}
		// creaks: the sheet over ~0.6 of its load, the mast with heel in a breeze; at least 1.5 s apart
		S.Quiet += Dt;
		const float RopeRate = Clamp((T.SheetLoad - 0.6f) * 0.9f, 0.f, 0.4f);
		const float MastRate = Clamp((std::fabs(T.HeelDeg) - 10.f) / 30.f, 0.f, 1.f) * Smooth(3.f, 9.f, T.WindMs) * 0.25f;
		if (S.Quiet > 1.5f)
		{
			if (R1 < RopeRate * Dt)
			{
				Out |= EvRopeCreak;
				S.Quiet = 0.f;
			}
			else if (R1 > 1.f - MastRate * Dt)
			{
				Out |= EvMastCreak;
				S.Quiet = 0.f;
			}
		}
		// waves on the hull: small clops when slow, the bow dropping into a chop when driving into it fast
		const float Waves = Clamp(T.WaveM / 0.3f, 0.f, 2.5f);
		const float SmallRate = Waves * (0.5f + 0.35f * V) * (1.f - Smooth(2.0f, 3.0f, V));
		const float LargeRate = Smooth(0.25f, 0.8f, T.WaveM) * Smooth(1.0f, 3.0f, V) * (T.bPlaning ? 0.9f : 0.5f);
		if (R2 < SmallRate * Dt)
		{
			Out |= EvWaveSlapSmall;
		}
		else if (R2 > 1.f - LargeRate * Dt)
		{
			Out |= EvWaveSlapLarge;
			if (V > 2.5f && T.WindMs > 6.f)
			{
				Out |= EvSpray;
			}
		}
		// the rudder knocking in a seaway
		if (R3 < Clamp(T.WaveM - 0.2f, 0.f, 1.f) * 0.3f * Dt)
		{
			Out |= EvRudderKnock;
		}
		return Out;
	}

	/** The fill's whump louder in more wind; a slap louder at speed and in bigger waves. */
	inline float FillGain(float WindMs)
	{
		return 0.35f + 0.65f * Smooth(2.f, 9.f, WindMs);
	}

	inline float SlapGain(const FTelemetry& T)
	{
		return Clamp(0.4f + 0.25f * T.SpeedMs / 3.f + 0.35f * T.WaveM / 0.6f, 0.2f, 1.f);
	}

	/** The launch's outboard: loops sail_outboard_{idle,low,mid,high} recorded at these rpm, crossfaded by pitch. An
	 *  engine stands more re-pitching than the water: these loops play at 0.65..1.6. */
	struct FOutboardMix
	{
		float Gain[4] = {};
		float Pitch[4] = {1.f, 1.f, 1.f, 1.f};
	};

	inline const float* OutboardRpm()
	{
		static const float Rpm[4] = {1000.f, 1800.f, 3000.f, 4800.f};
		return Rpm;
	}

	inline FOutboardMix MixOutboard(float Rpm)
	{
		FOutboardMix M;
		const float* A = OutboardRpm();
		for (int I = 0; I < 4; ++I)
		{
			M.Pitch[I] = Clamp(Rpm / A[I], 0.65f, 1.6f);
		}
		if (Rpm <= A[0])
		{
			M.Gain[0] = Rpm > 0.f ? 1.f : 0.f;
			return M;
		}
		if (Rpm >= A[3])
		{
			M.Gain[3] = 1.f;
			return M;
		}
		int I = 0;
		while (I < 2 && Rpm > A[I + 1])
		{
			++I;
		}
		// equal power between the two neighbours, only over the rpm where both can be pitched to it (so they meet in tune)
		const float Lo = A[I] > 0.65f * A[I + 1] ? A[I] : 0.65f * A[I + 1];
		const float Hi = A[I + 1] < 1.6f * A[I] ? A[I + 1] : 1.6f * A[I];
		const float U = Clamp((Rpm - Lo) / (Hi - Lo), 0.f, 1.f);
		M.Gain[I] = std::cos(U * 1.5707963f);
		M.Gain[I + 1] = std::sin(U * 1.5707963f);
		return M;
	}

	/** Slaps a second of one parked boat's halyard on its mast (sail_halyard_slap one-shots), by the wind at the mast. */
	inline float HalyardRate(float WindMs)
	{
		if (WindMs <= 1.5f)
		{
			return 0.f;
		}
		const float R = 0.25f * std::pow(WindMs - 1.5f, 1.3f);
		return R > 6.f ? 6.f : R;
	}

	/** The boat park's loops instead of one-shots: sail_halyard_light vs _fresh (per boat) and the _chorus pair. */
	struct FHalyardMix
	{
		float Light = 0.f;
		float Fresh = 0.f;
	};

	inline FHalyardMix MixHalyards(float WindMs)
	{
		FHalyardMix M;
		const float On = Smooth(1.5f, 3.0f, WindMs);
		const float F = Smooth(4.5f, 6.5f, WindMs);
		M.Light = On * std::cos(F * 1.5707963f);
		M.Fresh = On * std::sin(F * 1.5707963f);
		return M;
	}

	/** Small flags (sail_flag_light/_mid/_strong) and the windsock (sail_windsock) by the wind. */
	struct FFlagMix
	{
		float Light = 0.f;
		float Mid = 0.f;
		float Strong = 0.f;
		float Windsock = 0.f;
		float Pitch = 1.f;
	};

	inline FFlagMix MixFlags(float WindMs)
	{
		FFlagMix M;
		const float On = Smooth(1.0f, 2.5f, WindMs);
		float L = WindMs <= 3.5f ? 1.f : Tri(3.5f, 3.5f, WindMs);
		float Mi = Tri(7.f, 3.5f, WindMs);
		float S = WindMs >= 10.5f ? 1.f : Tri(10.5f, 3.5f, WindMs);
		const float Sum = L + Mi + S;
		M.Light = On * std::sqrt(L / Sum);
		M.Mid = On * std::sqrt(Mi / Sum);
		M.Strong = On * std::sqrt(S / Sum);
		M.Windsock = Smooth(1.5f, 4.f, WindMs) * (1.f - 0.3f * Smooth(10.f, 15.f, WindMs));
		M.Pitch = Clamp(0.9f + 0.02f * WindMs, 0.9f, 1.2f);
		return M;
	}
}
}
