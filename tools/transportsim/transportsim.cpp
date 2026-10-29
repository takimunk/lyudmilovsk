// The cable-car metro outside the engine: behaviour checks and a busy day (tools/transportsim/run.sh).
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Transport/CablewaySim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

using namespace LyudTransport;
using LT_V3 = LyudTransport::V3;

static int Fails = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); Fails++; } } while (0)

static std::string ReadFile(const char* P)
{
	std::ifstream F(P);
	std::stringstream S;
	S << F.rdbuf();
	return S.str();
}

static bool Load(FNetwork& N)
{
	if (!N.Load(ReadFile("unreal/Lyudmilovsk/Content/Transport/network_built.json")))
	{
		std::printf("load: %s\n", N.LastError.c_str());
		return false;
	}
	return true;
}

// runs till the rider is done (or the time is up); returns seconds
static double RunTrip(FNetwork& N, int RI, double MaxS, bool bPlayer = false)
{
	const double T0 = N.Time;
	while (N.Time - T0 < MaxS)
	{
		N.Step(0.25f);
		if (bPlayer && N.Riders[RI].State == ERiderState::Waiting && N.Riders[RI].Cabin >= 0)
		{
			const FCabin& C = N.Cabins[N.Riders[RI].Cabin];
			const FLeg& G = N.Riders[RI].Legs[N.Riders[RI].LegI];
			if (C.State == ECabinState::Dwelling && C.AtStop == G.BoardStop && C.Door > 0.9f)
			{
				N.Cabins[N.Riders[RI].Cabin].bPlayerAboard = true;
				N.PlayerBoard(N.Riders[RI].Cabin, G.To);
			}
		}
		if (N.Riders[RI].State == ERiderState::Done)
		{
			if (bPlayer)
			{
				for (FCabin& C : N.Cabins) C.bPlayerAboard = false;
			}
			return N.Time - T0;
		}
	}
	return -1.0;
}

static int Test()
{
	FNetwork N;
	if (!Load(N))
	{
		return 1;
	}
	std::printf("%zu stations, %zu lines, %zu cabins\n", N.Stations.size(), N.Lines.size(), N.Cabins.size());
	const int Aton = N.FindStation("aton"), Academ = N.FindStation("academ"), Zenit = N.FindStation("zenit"),
		Polar = N.FindStation("polar"), Horizon = N.FindStation("horizon"), Mayak = N.FindStation("mayak");
	CHECK(Aton >= 0 && Academ >= 0 && Horizon >= 0, "stations");

	// 1. nobody asks: nothing runs
	for (int I = 0; I < 2400; I++) N.Step(0.5f);
	float Run = 0.f;
	for (const FCabin& C : N.Cabins) Run += C.Odometer;
	CHECK(Run == 0.f, "cabins ran %.0f m with nobody asking", Run);

	// 2. the player at Aton (a cabin waits there): in, to the Academgorodok
	N.Reset();
	int RI = N.Travel(PlayerId, Aton, Academ);
	CHECK(RI >= 0, "player trip");
	bool bCalled = false;
	float Eta = N.Eta(Aton, N.FindLine("sun"), -1, bCalled);
	std::printf("player Aton->Academ: eta %.0f s (called %d)\n", Eta, bCalled);
	double T = RunTrip(N, RI, 900, true);
	std::printf("  done in %.0f s\n", T);
	CHECK(T > 0 && T < 240, "Aton->Academ took %.0f s", T);

	// 3. from the Academgorodok back up (cabin comes from wherever), then the cabins go home
	RI = N.Travel(PlayerId, Academ, Aton);
	T = RunTrip(N, RI, 1200, true);
	std::printf("player Academ->Aton: %.0f s\n", T);
	CHECK(T > 0 && T < 600, "Academ->Aton took %.0f s", T);
	for (int I = 0; I < 1200; I++) N.Step(0.5f);
	int Home = 0, Idle = 0;
	for (const FCabin& C : N.Cabins) { Home += C.State == ECabinState::Depot; Idle += C.State == ECabinState::Idle; }
	std::printf("after 10 min quiet: %d in depot, %d idle\n%s", Home, Idle, N.Report().c_str());
	CHECK(Idle <= (int)N.Lines.size(), "%d cabins idle out of the depot", Idle);

	// 4. an NPC across two lines: Aton -> Horizon (change at Zenit)
	RI = N.Travel(7, Aton, Horizon);
	CHECK(RI >= 0 && N.Riders[RI].Legs.size() == 2, "two legs");
	T = RunTrip(N, RI, 2400);
	std::printf("npc Aton->Horizon (change at %s): %.0f s\n", RI >= 0 && N.Riders[RI].Legs.size() > 1 ? N.Stations[N.Riders[RI].Legs[0].To].Id.c_str() : "?", T);
	CHECK(T > 0, "Aton->Horizon never got there");

	// 5. a crowd: 40 trips in 20 minutes, everybody gets there, nobody runs into anybody
	N.Reset();
	unsigned Seed = 7;
	auto Rnd = [&Seed]() { Seed = Seed * 1103515245u + 12345u; return (Seed >> 16) & 0x7fff; };
	std::vector<int> Trips;
	double MinGap = 1e9;
	for (int I = 0; I < 4800 * 2; I++)
	{
		if (I % 60 == 0 && Trips.size() < 40)
		{
			int A = Rnd() % N.Stations.size(), B = Rnd() % N.Stations.size();
			if (A != B)
			{
				Trips.push_back(N.Travel(100 + (int)Trips.size(), A, B));
			}
		}
		N.Step(0.25f);
		for (int A = 0; A < (int)N.Cabins.size(); A++)
			for (int B = 0; B < (int)N.Cabins.size(); B++)
			{
				const FCabin& Ca = N.Cabins[A], & Cb = N.Cabins[B];
				if (A == B || Ca.Line != Cb.Line || Ca.State != ECabinState::Moving || Cb.State == ECabinState::Depot || Cb.State == ECabinState::Idle)
					continue;
				const float G = N.Lines[Ca.Line].Ahead(Ca.S, Cb.S);
				if (G < MinGap) { MinGap = G; if (G < 8) std::printf("  close: %s(%s) %s(%s) %.1f t=%.0f\n", Ca.Id.c_str(), CabinStateName(Ca.State), Cb.Id.c_str(), CabinStateName(Cb.State), G, N.Time); }
			}
	}
	int Done = 0, Fail = 0;
	double Wait = 0;
	for (int R : Trips)
	{
		if (R < 0) { Fail++; continue; }
		Done += N.Riders[R].State == ERiderState::Done;
	}
	std::printf("crowd: %d of %zu trips done (%d unplannable), closest cabins %.1f m\n%s", Done, Trips.size(), Fail, MinGap, N.Report().c_str());
	CHECK(Done + Fail == (int)Trips.size(), "trips left unfinished");
	CHECK(MinGap > 8.0, "cabins came within %.1f m", MinGap);
	(void)Wait; (void)Polar; (void)Zenit; (void)Mayak;

	// 6. the rope sags under a cabin crossing a span and sways in the wind; the cabin swings when it speeds up
	N.Reset();
	RI = N.Travel(PlayerId, Aton, Academ);
	float MaxSag = 0.f, MaxSway = 0.f, MaxPitch = 0.f, MaxA = 0.f, MaxRoll = 0.f;
	LT_V3 Prev[2];
	int Seen = 0;
	const LT_V3 Wind(0.f, 12.f, 0.f);
	for (int I = 0; I < 1600; I++)
	{
		N.Step(0.05f);
		N.StepMotion(0.05f, Wind);
		const FRider& R = N.Riders[RI];
		if (R.State == ERiderState::Waiting && R.Cabin >= 0 && N.Cabins[R.Cabin].State == ECabinState::Dwelling)
		{
			N.PlayerBoard(R.Cabin, Academ);
		}
		const int C = R.Cabin >= 0 ? R.Cabin : 0;
		const LT_V3 O = N.RopeOffset(N.Cabins[C].Line, N.Cabins[C].S);
		MaxSag = std::max(MaxSag, -O.Z);
		MaxSway = std::max(MaxSway, std::sqrt(O.X * O.X + O.Y * O.Y));
		MaxPitch = std::max(MaxPitch, std::fabs(N.Cabins[C].Pitch));
		MaxRoll = std::max(MaxRoll, std::fabs(N.Cabins[C].Roll));
		// the ride is smooth: the grip's acceleration (second differences of where it is) stays a gondola's, no kinks
		const LT_V3 G3 = N.CabinAt(C);
		if (N.Cabins[C].State == ECabinState::Moving && R.State == ERiderState::Riding)
		{
			if (Seen >= 2)
			{
				const LT_V3 A = (G3 - Prev[1] * 2.f + Prev[0]) * (1.f / (0.05f * 0.05f));
				const float Am = std::sqrt(A.X * A.X + A.Y * A.Y + A.Z * A.Z);
				if (Am > 3.f && std::getenv("TR_DEBUG_JOLT"))
				{
					std::printf("  jolt %.2f at s=%.1f v=%.2f acc=%.2f t=%.2f\n", Am, N.Cabins[C].S, N.Cabins[C].Speed, N.Cabins[C].Acc, N.Time);
				}
				MaxA = std::max(MaxA, Am);
			}
			Prev[0] = Prev[1];
			Prev[1] = G3;
			Seen++;
		}
		else
		{
			Seen = 0;
		}
	}
	std::printf("rope under the cabin: sag up to %.2f m, sway %.2f m in 12 m/s; cabin pitch up to %.1f deg, roll %.1f deg; the ride's acceleration up to %.2f m/s2\n",
		MaxSag, MaxSway, MaxPitch * 57.3f, MaxRoll * 57.3f, MaxA);
	CHECK(MaxA < 4.f, "a jolt in the ride: %.2f m/s2", MaxA);      // ~3.5 at most, slowly round the end wheel
	CHECK(MaxSag > 0.2f && MaxSag < 4.f, "sag %.2f", MaxSag);
	CHECK(MaxSway > 0.1f && MaxSway < 5.f, "sway %.2f", MaxSway);
	CHECK(MaxPitch > 0.002f && MaxPitch < 0.2f, "pitch %.3f", MaxPitch);
	std::printf(Fails ? "%d FAILED\n" : "all ok\n", Fails);
	return Fails ? 1 : 0;
}

int main(int Argc, char** Argv)
{
	const std::string Cmd = Argc > 1 ? Argv[1] : "test";
	if (Cmd == "test")
	{
		return Test();
	}
	std::printf("usage: run.sh test\n");
	return 2;
}
