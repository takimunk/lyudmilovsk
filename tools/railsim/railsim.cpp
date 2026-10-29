// The sea railway's timetable outside the engine (Private/Railway/RailRun.cpp). tools/railsim/run.sh test|table
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Railway/RailRun.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace LyudRail;

static int Fails = 0;
static void Check(bool bOk, const char* What)
{
	std::printf("%s %s\n", bOk ? "ok  " : "FAIL", What);
	Fails += bOk ? 0 : 1;
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "test";
	std::ifstream F("unreal/Lyudmilovsk/Content/Railway/railway_built.json");
	std::stringstream Ss;
	Ss << F.rdbuf();
	FRunParams P;
	std::string Err;
	if (!LoadParams(Ss.str(), P, Err))
	{
		std::printf("cannot read railway_built.json: %s\n", Err.c_str());
		return 2;
	}
	FRun Run;
	Run.Build(P);
	const FRunParams& Q = Run.Params();
	std::printf("one way %.0f s (%.1f min), round trip %.0f s, period %.0f s, %zu stops\n", Run.OneWaySeconds(), Run.OneWaySeconds() / 60,
		Run.CycleSeconds(), Q.Period, Q.Stops.size());
	if (Mode == "table")
	{
		for (double t = 0; t < Q.Period; t += 30)
		{
			const FTrainState S = Run.At(t);
			std::printf("%5.0f s %8.1f m %6.2f m/s dir %+d stop %d doors %d vis %d svc %d\n", t, S.S, S.V, S.Dir, S.Stop, S.bDoorsOpen, S.bVisible, S.bInService);
		}
		return 0;
	}
	// smooth: no jump of more than VMax*dt, no speed over the limits, acceleration bounded
	double LastS = Run.At(0).S, MaxJump = 0, MaxV = 0, MaxA = 0, LastV = 0;
	for (double t = 0.5; t < Q.Period * 2; t += 0.5)
	{
		const FTrainState S = Run.At(t);
		if (S.bInService)
		{
			MaxJump = std::max(MaxJump, std::fabs(S.S - LastS));
			MaxV = std::max(MaxV, std::fabs(S.V));
			MaxA = std::max(MaxA, std::fabs(std::fabs(S.V) - std::fabs(LastV)) / 0.5);
		}
		LastS = S.S;
		LastV = S.V;
	}
	std::printf("  max step %.2f m / 0.5 s, max speed %.2f, max accel %.2f\n", MaxJump, MaxV, MaxA);
	Check(MaxJump < Q.VMax * 0.5 + 0.5, "no jumps while in service");
	Check(MaxV <= Q.VMax + 0.01, "never over VMax");
	Check(MaxA <= 1.2, "acceleration bounded (the last creep onto the mark is the sharpest)");
	// every stop is served both ways, doors open while standing there
	for (int k = 0; k < int(Q.Stops.size()); k++)
	{
		for (int d = -1; d <= 1; d += 2)
		{
			const double T = Run.NextDoorsOpen(k, d, 100.0);
			const FTrainState S = Run.At(T + 1.0);
			char Msg[160];
			std::snprintf(Msg, sizeof Msg, "stop %s dir %+d: doors open at %.0f s, train at %.1f (stop %.1f) doors %d", Q.Stops[k].Id.c_str(), d, T, S.S,
				Q.Stops[k].S, S.bDoorsOpen);
			Check(T >= 100.0 && std::fabs(S.S - Q.Stops[k].S) < 0.5 && S.bDoorsOpen && S.Stop == k && S.Dir == d, Msg);
		}
	}
	const int B = Run.FindStop("beach"), I = Run.FindStop("island");
	const double Ride = Run.RideSeconds(B, I, 1), Back = Run.RideSeconds(I, B, -1);
	std::printf("  beach -> island %.0f s, back %.0f s\n", Ride, Back);
	Check(Ride > 120 && Ride < 600 && Back > 120 && Back < 600, "rides between the halts take minutes");
	// hidden in the tunnel and past the horizon, seen between
	Check(!Run.At(1.0).bVisible, "starts unseen in the tunnel");
	int Dir = 0;
	const double Pass = Run.NextPass(Q.PortalS + 6.0, 0.0, &Dir);
	Check(Pass > 0 && Dir == 1, "the crossing sees an outbound train first");
	Check(Run.At(Pass).bVisible, "seen at the crossing");
	return Fails ? 1 : 0;
}
