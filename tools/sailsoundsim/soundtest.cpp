// Offline checks of LyudSail::Sound (Private/Sailing/SailSound.h). tools/sailsoundsim/run.sh builds and runs it.
#include "SailSound.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <sys/stat.h>

using namespace LyudSail::Sound;

static int Fails = 0;

static void Check(bool Ok, const char* What)
{
	std::printf("%s %s\n", Ok ? "ok  " : "FAIL", What);
	if (!Ok)
	{
		++Fails;
	}
}

static bool Sane(const FLoopMix& M)
{
	for (int I = 0; I < NumLoops; ++I)
	{
		if (!(M.Gain[I] >= 0.f && M.Gain[I] <= 1.f) || !(M.Pitch[I] >= 0.7f && M.Pitch[I] <= 1.45f))
		{
			return false;
		}
	}
	return true;
}

static bool HasSidecar(const char* Pool)
{
	struct stat St;
	const std::string P = std::string("audio/sfx/sailing/") + Pool + "_01.json";
	return stat(P.c_str(), &St) == 0;
}

int main()
{
	// every state in a sweep gives sane gains and pitches, and nothing jumps between neighbouring speeds
	bool AllSane = true, Steady = true;
	for (int Pl = 0; Pl < 2; ++Pl)
	{
		for (int Li = 0; Li <= 10; ++Li)
		{
			for (int Wi = 0; Wi <= 8; ++Wi)
			{
				FLoopMix Prev;
				for (int Vi = 0; Vi <= 140; ++Vi)
				{
					FTelemetry T;
					T.SpeedMs = Vi * 0.05f;
					T.Luff = Li * 0.1f;
					T.WindMs = Wi * 2.f;
					T.HeelDeg = 15.f;
					T.bPlaning = Pl == 1;
					const FLoopMix M = MixLoops(T);
					AllSane &= Sane(M);
					if (Vi > 0)
					{
						for (int I = 0; I < NumLoops; ++I)
						{
							Steady &= std::fabs(M.Gain[I] - Prev.Gain[I]) < 0.12f;
						}
					}
					Prev = M;
				}
			}
		}
	}
	Check(AllSane, "gains 0..1, pitches 0.7..1.45 everywhere");
	Check(Steady, "no gain jumps more than 0.12 per 0.05 m/s");

	FTelemetry Rest;
	FLoopMix M = MixLoops(Rest);
	float Sum = 0.f;
	for (int I = 0; I < NumLoops; ++I)
	{
		Sum += M.Gain[I];
	}
	Check(Sum < 1e-4f, "a boat at rest with the sail drawing and no wind is silent");

	FTelemetry Ghost;
	Ghost.SpeedMs = 0.6f;
	Ghost.WindMs = 2.f;
	M = MixLoops(Ghost);
	Check(M.Gain[HullGhost] > 0.5f && M.Gain[HullPlane] == 0.f, "ghosting: the trickle, no planing hiss");

	FTelemetry Cruise;
	Cruise.SpeedMs = 2.2f;
	Cruise.WindMs = 5.f;
	M = MixLoops(Cruise);
	Check(M.Gain[HullChuckle] > 0.8f && M.Gain[HullGhost] < 0.05f, "displacement: the chuckle");

	FTelemetry Plane;
	Plane.SpeedMs = 5.f;
	Plane.WindMs = 10.f;
	Plane.bPlaning = true;
	M = MixLoops(Plane);
	Check(M.Gain[HullPlane] > 0.85f && M.Gain[HullPlane] > M.Gain[HullChuckle] && M.Gain[HullHum] > 0.2f,
		"planing: the hiss over the chuckle, the hum");
	Check(M.Gain[Leech] > 0.5f, "planing with the sail drawing: the leech buzz");

	FTelemetry Flog;
	Flog.Luff = 1.f;
	Flog.WindMs = 12.f;
	M = MixLoops(Flog);
	Check(M.Gain[LuffHard] > 0.9f && M.Gain[LuffLight] < 0.05f, "luffing in a blow: the hard flog");
	Flog.WindMs = 2.f;
	Flog.Luff = 0.4f;
	M = MixLoops(Flog);
	Check(M.Gain[LuffLight] > 0.6f && M.Gain[LuffHard] < 0.05f, "a little luff in light air: the light flutter");

	FTelemetry Swamp;
	Swamp.WaterAboard = 1.f;
	M = MixLoops(Swamp);
	Check(M.Gain[CockpitSlosh] > 0.9f && M.Pitch[CockpitSlosh] < 1.f, "swamped: the slosh, slower");

	// events: a tack (luff up, then the sail draws within ~0.3 s) fills once; a sheet eased in slowly doesn't
	const float Dt = 1.f / 30.f;
	FEventState S;
	FTelemetry T;
	T.WindMs = 6.f;
	int Fills = 0;
	for (int F = 0; F < 90; ++F)
	{
		const float Tm = F * Dt;
		T.Luff = Tm < 1.f ? 0.9f : (Tm < 1.3f ? 0.9f - (Tm - 1.f) * 3.f : 0.f);
		Fills += (StepEvents(S, T, Dt, 0.5f, 0.5f, 0.99f) & EvFill) ? 1 : 0;
	}
	Check(Fills == 1, "a sail snapping full: one whump");
	FEventState S2;
	Fills = 0;
	for (int F = 0; F < 150; ++F)
	{
		const float Tm = F * Dt;
		T.Luff = Tm < 1.f ? 0.9f : (0.9f - (Tm - 1.f) * 0.4f);
		Fills += (StepEvents(S2, T, Dt, 0.5f, 0.5f, 0.99f) & EvFill) ? 1 : 0;
	}
	Check(Fills == 0, "a sheet eased in slowly: no whump");

	// creaks and slaps come at about their rates (a fixed LCG for the randoms)
	uint32_t Rng = 12345u;
	auto U = [&Rng]() {
		Rng = Rng * 1664525u + 1013904223u;
		return float(Rng >> 8) / float(1u << 24);
	};
	FEventState S3;
	FTelemetry Hard;
	Hard.SheetLoad = 1.f;
	Hard.SpeedMs = 3.5f;
	Hard.bPlaning = true;
	Hard.WindMs = 9.f;
	Hard.HeelDeg = 25.f;
	Hard.WaveM = 0.6f;
	int Creaks = 0, Large = 0, Small = 0, Spray = 0;
	for (int F = 0; F < 30 * 600; ++F)
	{
		const float R1 = U(), R2 = U(), R3 = U();
		const unsigned E = StepEvents(S3, Hard, Dt, R1, R2, R3);
		Creaks += (E & (EvRopeCreak | EvMastCreak)) ? 1 : 0;
		Large += (E & EvWaveSlapLarge) ? 1 : 0;
		Small += (E & EvWaveSlapSmall) ? 1 : 0;
		Spray += (E & EvSpray) ? 1 : 0;
	}
	std::printf("     10 min hard on the wind: %d creaks, %d large slaps, %d small, %d sprays\n", Creaks, Large, Small, Spray);
	Check(Creaks > 60 && Creaks < 400, "creaks every few seconds under load, never crowding");
	Check(Large > 200 && Small < Large && Spray == Large, "planing in a chop: large slaps with spray, few clops");

	// the outboard: equal power, in tune at the anchors, continuous in rpm
	bool Power = true, Cont = true, Tune = true;
	FOutboardMix P = MixOutboard(800.f);
	for (int Ri = 0; Ri <= 440; ++Ri)
	{
		const float R = 800.f + Ri * 10.f;
		const FOutboardMix O = MixOutboard(R);
		float E = 0.f;
		for (int I = 0; I < 4; ++I)
		{
			E += O.Gain[I] * O.Gain[I];
			Cont &= std::fabs(O.Gain[I] - P.Gain[I]) < 0.05f;
			if (O.Gain[I] > 0.05f && R >= 1000.f && R <= 4800.f)
			{
				// every loop that is heard is pitched to the wanted rpm
				Tune &= std::fabs(OutboardRpm()[I] * O.Pitch[I] - R) / R < 0.03f;
			}
		}
		Power &= std::fabs(E - 1.f) < 1e-3f;
		P = O;
	}
	Check(Power, "outboard crossfade keeps equal power");
	Check(Cont, "outboard gains continuous in rpm");
	Check(Tune, "every audible outboard loop pitched to the wanted rpm");
	Check(MixOutboard(0.f).Gain[0] == 0.f, "engine off: silent");

	// the boat park and the flags
	Check(HalyardRate(1.f) == 0.f && HalyardRate(4.f) > HalyardRate(3.f) && HalyardRate(30.f) <= 6.f,
		"halyard slaps: none in a calm, more with wind, capped");
	const FHalyardMix H3 = MixHalyards(3.f), H9 = MixHalyards(9.f);
	Check(H3.Light > 0.9f && H3.Fresh < 0.05f && H9.Fresh > 0.9f && H9.Light < 0.05f, "halyards: light loop at 3, fresh at 9");
	const FFlagMix F3 = MixFlags(3.f), F7 = MixFlags(7.f), F12 = MixFlags(12.f);
	Check(F3.Light > F3.Mid && F7.Mid > F7.Light && F7.Mid > F7.Strong && F12.Strong > 0.9f, "flags by strength");
	Check(MixFlags(0.f).Light == 0.f && MixFlags(0.f).Windsock == 0.f, "flags silent in a calm");

	// every pool the header names exists on disk
	bool Pools = true;
	for (int I = 0; I < NumLoops; ++I)
	{
		Pools &= HasSidecar(LoopPool(I));
	}
	for (int I = 0; I < NumEvents; ++I)
	{
		Pools &= HasSidecar(EventPool(1u << I));
	}
	const char* Others[] = {"sail_outboard_idle", "sail_outboard_low", "sail_outboard_mid", "sail_outboard_high",
		"sail_halyard_slap", "sail_halyard_light", "sail_halyard_fresh", "sail_flag_light", "sail_flag_mid",
		"sail_flag_strong", "sail_windsock"};
	for (const char* O : Others)
	{
		Pools &= HasSidecar(O);
	}
	Check(Pools, "every pool named here has its files in audio/sfx/sailing");

	std::printf("%d failure(s)\n", Fails);
	return Fails ? 1 : 0;
}
