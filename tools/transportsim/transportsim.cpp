// The cable-car metro outside the engine: behaviour checks and a busy day (tools/transportsim/run.sh).
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Transport/CablewaySim.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Transport/StationLife.h"

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

// --- the stations' day (station_life.json: tools/station_life.py; docs/transport.md "Station life") ----------------------

// One day of the town's trips on the real network from 5:30 to 0:45; Wind(hour) the wind over the line (m/s).
struct FDayResult
{
	int Today = 0, Started = 0, Finished = 0, Cut = 0, Unfinished = 0, DepartsInStop = 0, MaxWaiting = 0, MaxAboard = 0;
	int StuckInHall = 0;
	double MaxWait = 0.0, AvgWait = 0.0;
	int PeakHour = -1, PeakTrips = 0;
};

template <typename FWind>
static FDayResult RunDay(const std::string& LifeText, int Weekday, bool bWarm, float FestivalEnd, FWind Wind, const char* Name)
{
	FNetwork N;
	FDayResult Out;
	if (!Load(N))
	{
		Fails++;
		return Out;
	}
	N.bReuseRiders = true;
	FStationLife L;
	if (!L.Load(LifeText, N))
	{
		std::printf("station_life.json: %s\n", L.LastError.c_str());
		Fails++;
		return Out;
	}
	N.Reset();
	L.StartDay(Weekday, Weekday, bWarm);
	bool bFest = false;
	int ByHour[26] = {};
	const float Dt = 0.5f;
	for (double T = 5.5 * 3600.0; T < 24.75 * 3600.0; T += Dt)
	{
		const float Hour = (float)(T / 3600.0);
		if (FestivalEnd > 0.f && !bFest && Hour >= FestivalEnd)
		{
			L.FestivalEnded(Hour);
			bFest = true;
		}
		L.SetWind(N, Wind(Hour));
		const bool bHeld = N.bWindHold;
		N.Step(Dt);
		L.Step(N, Hour);
		int Waiting = 0;
		for (const FRider& R : N.Riders)
		{
			Waiting += R.State == ERiderState::Waiting && !R.bFree ? 1 : 0;
		}
		Out.MaxWaiting = std::max(Out.MaxWaiting, Waiting);
		for (const FCabin& C : N.Cabins)
		{
			Out.MaxAboard = std::max(Out.MaxAboard, (int)C.Riders.size());
		}
		for (const FEvent& E : N.Events)
		{
			if (E.Kind == EEvent::Depart && bHeld)
			{
				Out.DepartsInStop++;
			}
			if (E.Kind == EEvent::Done || E.Kind == EEvent::Alight)
			{
				N.ReleaseRider(E.Rider);
			}
		}
		N.Events.clear();
	}
	for (const FLifeRun& R : L.Today)
	{
		if (R.Phase == ELifePhase::Riding || R.Phase == ELifePhase::InHall || R.Phase == ELifePhase::Pending)
		{
			Out.Unfinished++;
			if (std::getenv("TR_DEBUG_LIFE"))
			{
				const FLifeTrip& Tr = L.Trips[R.Trip];
				std::printf("  unfinished %s %s -> %s at %.2f phase %d\n", L.Riders[Tr.Rider].Key.c_str(), N.Stations[Tr.From].Id.c_str(),
					N.Stations[Tr.To].Id.c_str(), R.At, (int)R.Phase);
			}
		}
		if (R.Phase != ELifePhase::Skipped)
		{
			ByHour[std::min(25, (int)R.At)]++;
		}
	}
	// at closing (0:15) nobody is left in a hall
	std::vector<int> In;
	for (int S = 0; S < (int)N.Stations.size(); S++)
	{
		L.InHall(S, In);
		Out.StuckInHall += (int)In.size();
	}
	for (int H = 0; H < 26; H++)
	{
		if (ByHour[H] > Out.PeakTrips)
		{
			Out.PeakTrips = ByHour[H];
			Out.PeakHour = H;
		}
	}
	Out.Today = (int)L.Today.size();
	Out.Started = L.Started;
	Out.Finished = L.Finished;
	Out.Cut = L.Cut;
	Out.MaxWait = L.MaxWait;
	Out.AvgWait = L.Finished ? L.SumWait / L.Finished : 0.0;
	std::printf("%-28s %3d trips today, %3d ridden, %d cut; peak %02d:00 (%d trips); wait avg %3.0f s, max %4.0f s; up to %d waiting, "
		"%d aboard; %d unfinished, %d in a hall at closing; wind stops %d, departures during one %d\n",
		Name, Out.Today, Out.Finished, Out.Cut, Out.PeakHour, Out.PeakTrips, Out.AvgWait, Out.MaxWait, Out.MaxWaiting, Out.MaxAboard,
		Out.Unfinished, Out.StuckInHall, L.WindStops, Out.DepartsInStop);
	return Out;
}

static int Town()
{
	const std::string Life = ReadFile("unreal/Lyudmilovsk/Content/Transport/station_life.json");
	if (Life.empty())
	{
		std::printf("no station_life.json (python3 tools/station_life.py)\n");
		return 1;
	}
	FNetwork N;
	if (!Load(N))
	{
		return 1;
	}
	FStationLife L;
	CHECK(L.Load(Life, N), "load: %s", L.LastError.c_str());
	std::printf("%zu riders, %zu trips, %zu staff\n", L.Riders.size(), L.Trips.size(), L.Staff.size());
	// the rota: an attendant in every hall every open hour, a barista 6:30..23:30, every day
	for (int S = 0; S < (int)N.Stations.size(); S++)
	{
		for (int D = 0; D < 7; D++)
		{
			for (float H = 6.f; H < 23.5f; H += 0.25f)
			{
				CHECK(L.OnShift(S, "attendant", D, H) >= 0, "%s: no attendant on day %d at %.2f", N.Stations[S].Id.c_str(), D, H);
				CHECK(H < 6.5f || L.OnShift(S, "barista", D, H) >= 0, "%s: no barista on day %d at %.2f", N.Stations[S].Id.c_str(), D, H);
			}
			CHECK(L.OnShift(S, "attendant", D, 3.f) < 0, "%s: an attendant at 3:00", N.Stations[S].Id.c_str());
		}
	}
	auto Calm = [](float) { return 4.f; };
	// a weekday: the researchers and the out-of-town workers in the morning and the evening
	const FDayResult Mon = RunDay(Life, 0, false, 0.f, Calm, "Monday");
	CHECK(Mon.Unfinished == 0 && Mon.StuckInHall == 0, "Monday: %d unfinished, %d in a hall at closing", Mon.Unfinished, Mon.StuckInHall);
	CHECK(Mon.Finished > 100, "Monday: only %d trips ridden", Mon.Finished);
	CHECK(Mon.PeakHour >= 6 && Mon.PeakHour <= 9 || Mon.PeakHour >= 14 && Mon.PeakHour <= 18, "Monday's peak at %d:00", Mon.PeakHour);
	CHECK(Mon.MaxWait < 15 * 60.0, "Monday: somebody waited %.0f s for a cabin", Mon.MaxWait);
	CHECK(Mon.MaxAboard <= 4, "a cabin with %d aboard", Mon.MaxAboard);
	// Saturday, warm: the market and the beach; a festival ends at 22:30
	const FDayResult Sat = RunDay(Life, 5, true, 22.5f, Calm, "Saturday, warm, festival");
	CHECK(Sat.Unfinished == 0 && Sat.StuckInHall == 0, "Saturday: %d unfinished, %d in a hall at closing", Sat.Unfinished, Sat.StuckInHall);
	CHECK(Sat.Finished > 30, "Saturday: only %d trips ridden", Sat.Finished);
	CHECK(Sat.MaxWait < 20 * 60.0, "Saturday: somebody waited %.0f s", Sat.MaxWait);
	// a gale on a weekday morning (8:00..9:10 over the stop): nothing leaves a station while it blows, everyone gets
	// home in the end
	auto Gale = [](float H) { return H > 8.f && H < 9.1f ? 21.f : (H > 7.8f && H < 9.4f ? 13.f : 5.f); };
	const FDayResult Windy = RunDay(Life, 1, false, 0.f, Gale, "Tuesday, a gale 8:00-9:10");
	CHECK(Windy.DepartsInStop == 0, "%d cabins left a station during the wind stop", Windy.DepartsInStop);
	CHECK(Windy.Unfinished == 0 && Windy.StuckInHall == 0, "the gale day: %d unfinished, %d in a hall at closing", Windy.Unfinished, Windy.StuckInHall);
	CHECK(Windy.MaxWait > Mon.MaxWait, "the gale kept nobody waiting longer (%.0f s vs %.0f s)", Windy.MaxWait, Mon.MaxWait);
	// the stop's own rule: on above 17 m/s, off only after 5 minutes under 12
	{
		FNetwork M;
		Load(M);
		FStationLife W;
		W.SetWind(M, 18.f);
		CHECK(M.bWindHold, "18 m/s: no stop");
		M.Time += 200.0;
		W.SetWind(M, 11.f);
		M.Time += 200.0;
		W.SetWind(M, 11.f);
		CHECK(M.bWindHold, "going again after 200 s of calm");
		M.Time += 200.0;
		W.SetWind(M, 11.f);
		CHECK(!M.bWindHold, "still stopped after 400 s under 12 m/s");
	}
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
	if (Cmd == "town")
	{
		return Town();
	}
	std::printf("usage: run.sh test | town\n");
	return 2;
}
