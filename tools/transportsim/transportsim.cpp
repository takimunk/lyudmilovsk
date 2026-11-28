// The cable-car metro outside the engine: behaviour checks and a busy day (tools/transportsim/run.sh).
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Transport/CablewaySim.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Transport/StationLife.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Transport/FrontDoor.h"

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

// the hall's moods (docs/transport.md "Hall moods"): a typical hall's places filled by the picker the game uses
static void HallMoods(const FStationLife& L)
{
	using K = EHallSpot;
	const std::vector<K> Kinds = { K::Seat, K::Seat, K::Seat, K::Seat, K::Seat, K::Seat, K::TableSeat, K::TableSeat, K::Counter,
		K::Queue, K::Queue, K::Rail, K::Rail, K::Window, K::Window, K::Diagram, K::Read, K::StandTable };
	auto Fill = [&](const FHallMood& M, const std::vector<int>& Ages, bool bOut, std::vector<FHallPlace>& P, std::vector<int>& Who)
	{
		P.clear();
		Who.assign(Kinds.size(), -1);
		int Q = 0;
		for (K Kd : Kinds)
		{
			FHallPlace X;
			X.Kind = Kd;
			X.Queue = Kd == K::Queue ? ++Q : 0;
			P.push_back(X);
		}
		for (int I = 0; I < (int)Ages.size(); I++)
		{
			const int J = HallPick(P, "rider_" + std::to_string(I), Ages[I], bOut, M, 77u);
			if (J >= 0)
			{
				P[J].bTaken = true;
				Who[J] = I;
			}
		}
	};
	std::vector<FHallPlace> P;
	std::vector<int> Who;
	// the morning rush: the buffet's counter and its queue
	FHallMood Rush;
	Rush.Hour = 8.f;
	Fill(Rush, std::vector<int>(10, 35), false, P, Who);
	int Queue = 0;
	for (int I = 0; I < (int)P.size(); I++)
	{
		Queue += (P[I].Kind == K::Counter || P[I].Kind == K::Queue) && P[I].bTaken ? 1 : 0;
	}
	CHECK(Queue >= 2, "8:00, ten adults: only %d at the counter and in its queue", Queue);
	// the queue in order: never a second spot without the first
	CHECK(!(P[10].bTaken && !P[9].bTaken), "the queue's second spot taken before its first");
	// the buffet shut: nobody at its counter
	FHallMood Shut = Rush;
	Shut.bBuffet = false;
	Fill(Shut, std::vector<int>(10, 35), false, P, Who);
	CHECK(!P[8].bTaken && !P[9].bTaken, "a queue at a shut buffet");
	// an afternoon with children: they take the windows
	FHallMood Day;
	Day.Hour = 15.f;
	Fill(Day, { 9, 40, 11, 45, 52, 33 }, false, P, Who);
	int KidsAtWindows = 0;
	for (int I = 0; I < (int)P.size(); I++)
	{
		KidsAtWindows += P[I].Kind == K::Window && Who[I] >= 0 && (Who[I] == 0 || Who[I] == 2) ? 1 : 0;
	}
	CHECK(KidsAtWindows == 2, "15:00: %d of 2 children at the windows", KidsAtWindows);
	// the old in the morning: seated, most with the paper
	FHallMood Morning;
	Morning.Hour = 10.f;
	int Seated = 0, Reading = 0;
	Fill(Morning, std::vector<int>(5, 70), false, P, Who);
	for (int I = 0; I < (int)P.size(); I++)
	{
		if (Who[I] >= 0 && (P[I].Kind == K::Seat || P[I].Kind == K::Read))
		{
			Seated++;
			Reading += HallPose(70, P[I].Kind, Morning, LifeHash("rider_" + std::to_string(Who[I]), 53u)) == EHallPose::Read ? 1 : 0;
		}
	}
	CHECK(Seated >= 4, "10:00, five pensioners: only %d seated or at the paper rack", Seated);
	CHECK(Reading >= 2, "10:00: only %d of the old reading", Reading);
	// walking out: never a seat or the counter
	Fill(Rush, std::vector<int>(8, 30), true, P, Who);
	for (int I = 0; I < (int)P.size(); I++)
	{
		CHECK(!(P[I].bTaken && (P[I].Kind == K::Seat || P[I].Kind == K::TableSeat || P[I].Kind == K::Counter || P[I].Kind == K::Queue)),
			"someone walking out sat down or queued");
	}
	// lunch: the cafe's tables eat
	FHallMood Lunch;
	Lunch.Hour = 12.5f;
	int Eat = 0;
	for (int I = 0; I < 20; I++)
	{
		Eat += HallPose(35, K::TableSeat, Lunch, I / 20.f) == EHallPose::Eat ? 1 : 0;
	}
	CHECK(Eat >= 12, "lunch: %d of 20 at the tables eat", Eat);
	// the rain fills the hall with passers-by; the stop with tea drinkers
	int Dry = 0, Wet = 0;
	FHallMood Rain = Day;
	Rain.bRain = true;
	for (uint32_t S = 0; S < 200; S++)
	{
		Dry += HallFiller(Day, S * 2654435761u);
		Wet += HallFiller(Rain, S * 2654435761u);
	}
	CHECK(Wet >= Dry + 400, "the rain brought %.1f more people in on average", (Wet - Dry) / 200.f);
	CHECK(std::string(HallBark(Rain)) == "rain", "no rain words in the rain");
	// the staff: the barista works while someone waits, the attendant talks to the hall at a stop
	CHECK(BaristaPose(2, Rush, 0.2f) == EHallPose::Work, "the barista idle with a queue");
	FHallMood Stop = Rush;
	Stop.bStopped = true;
	CHECK(AttendantPose(5, Stop, 0.1f) == EHallPose::Talk, "the attendant silent at the wind stop");
	// the riders' ages came with the data
	int Kids = 0, Old = 0;
	for (const FLifeRider& R : L.Riders)
	{
		Kids += R.Age < 18 ? 1 : 0;
		Old += R.Age >= 60 ? 1 : 0;
	}
	CHECK(Old > 10, "only %d riders of 60 and over (no ages in station_life.json?)", Old);
	// a child along with a parent goes on the same days (the parent's draw) and from the same hall
	int Families = 0;
	for (int R = 0; R < (int)L.Riders.size(); R++)
	{
		if (L.Riders[R].With.empty())
		{
			continue;
		}
		Families++;
		int Parent = -1;
		for (int Q = 0; Q < (int)L.Riders.size(); Q++)
		{
			Parent = L.Riders[Q].Key == L.Riders[R].With ? Q : Parent;
		}
		CHECK(Parent >= 0 && L.Riders[R].Age < 14, "%s goes along with nobody", L.Riders[R].Key.c_str());
		for (int D = 0; D < 14 && Parent >= 0; D++)
		{
			FStationLife Life = L;
			Life.StartDay(D, D % 7, true);
			int Kid = 0, Par = 0;
			for (const FLifeRun& Run : Life.Today)
			{
				Kid += Life.Trips[Run.Trip].Rider == R ? 1 : 0;
				Par += Life.Trips[Run.Trip].Rider == Parent ? 1 : 0;
			}
			CHECK(Kid == 0 || Par > 0, "%s off to the beach on day %d without %s", L.Riders[R].Key.c_str(), D, L.Riders[R].With.c_str());
			for (const FLifeRun& K : Life.Today)
			{
				for (const FLifeRun& PR : Life.Today)
				{
					const FLifeTrip &KT = Life.Trips[K.Trip], &PT = Life.Trips[PR.Trip];
					CHECK(!(KT.Rider == R && PT.Rider == Parent && KT.At == PT.At) || K.HallMinutes == PR.HallMinutes,
						"%s and %s wait different minutes in the hall", L.Riders[R].Key.c_str(), L.Riders[R].With.c_str());
				}
			}
		}
	}
	CHECK(Families > 0, "no children along with a parent in station_life.json");
	std::printf("hall moods: %d at the counter at 8:00, %d children at the windows, %d of 5 old seated (%d reading), rain +%.1f people; riders %d under 18 (%d along with a parent), %d over 60\n",
		Queue, KidsAtWindows, Seated, Reading, (Wet - Dry) / 200.f, Kids, Families, Old);
}

// the cabins' life (docs/transport.md "Cabin life"): who stands at the glass, who reads, the words aboard
static void CabinLife()
{
	FCabinMood Clear;
	Clear.Hour = 11.f;
	FCabinMood Fog = Clear;
	Fog.bFog = true;
	FCabinMood Night;
	Night.Hour = 22.f;
	auto Share = [](int Age, int Aboard, const FCabinMood& M, ECabinAct A)
	{
		int N = 0;
		for (int I = 0; I < 200; I++)
		{
			N += CabinAct(Age, Aboard, M, (I + 0.5f) / 200.f) == A ? 1 : 0;
		}
		return N / 200.f;
	};
	CHECK(Share(8, 2, Clear, ECabinAct::Window) > 0.8f, "a child not at the glass on a clear day");
	CHECK(Share(8, 2, Fog, ECabinAct::Window) > 0.8f, "a child not at the glass in the fog");
	CHECK(Share(35, 2, Clear, ECabinAct::Window) > Share(35, 2, Fog, ECabinAct::Window) * 2.f, "the fog keeps as many grown-ups at the glass");
	CHECK(Share(35, 2, Night, ECabinAct::Window) < Share(35, 2, Clear, ECabinAct::Window), "as many at the dark glass as at the bay");
	CHECK(Share(70, 2, Clear, ECabinAct::Read) > Share(30, 2, Clear, ECabinAct::Read), "the young read more than the old");
	// a full cabin: at most two at the glass, the children first
	std::vector<ECabinAct> Acts;
	CabinActs({ 9, 7, 30, 16 }, { 0.1f, 0.2f, 0.01f, 0.05f }, Clear, Acts);
	int Glass = 0;
	for (ECabinAct A : Acts)
	{
		Glass += A == ECabinAct::Window ? 1 : 0;
	}
	CHECK(Glass == 2 && Acts[0] == ECabinAct::Window && Acts[1] == ECabinAct::Window, "a full cabin: %d at the glass, the children %d %d", Glass, (int)Acts[0], (int)Acts[1]);
	CHECK(std::string(CabinBark(Fog, false, 0.9f)) == "fog" && std::string(CabinBark(Clear, true, 0.1f)) == "kid", "the words aboard ignore the fog or the child");
	std::printf("cabin life: child at the glass %.0f%% (fog %.0f%%), adult %.0f%% clear / %.0f%% fog / %.0f%% night, old reading %.0f%%\n",
		100.f * Share(8, 2, Clear, ECabinAct::Window), 100.f * Share(8, 2, Fog, ECabinAct::Window), 100.f * Share(35, 2, Clear, ECabinAct::Window),
		100.f * Share(35, 2, Fog, ECabinAct::Window), 100.f * Share(35, 2, Night, ECabinAct::Window), 100.f * Share(70, 2, Clear, ECabinAct::Read));
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
	HallMoods(L);
	CabinLife();
	std::printf(Fails ? "%d FAILED\n" : "all ok\n", Fails);
	return Fails ? 1 : 0;
}


// --- the Aton station as the club's front door (docs/front_door.md): the board, the line, the usual, the lost things --------

static int Board()
{
	using namespace LyudFrontDoor;
	const std::string LifeText = ReadFile("unreal/Lyudmilovsk/Content/Transport/station_life.json");
	FNetwork N;
	FStationLife L;
	if (LifeText.empty() || !Load(N) || !L.Load(LifeText, N))
	{
		std::printf("no network or station life\n");
		return 1;
	}
	const int Aton = N.FindStation("aton"), Sun = N.FindLine("sun");
	CHECK(Aton >= 0 && Sun >= 0, "no aton or sun");
	N.bReuseRiders = true;
	N.Reset();
	L.StartDay(0, 0, false);
	std::vector<FDeparture> Rows;
	// a prediction made ten minutes before the riders go up, and when they really got on
	std::vector<float> Predicted(L.Today.size(), -1.f);
	int Samples = 0, Unsorted = 0, OutOfWindow = 0, Empty = 0, BoardingSeen = 0, BoardingMissed = 0, Planned = 0, Arriving = 0;
	int MaxRows = 0;
	const float Dt = 0.5f;
	for (double T = 5.5 * 3600.0; T < 23.5 * 3600.0; T += Dt)
	{
		const float Hour = (float)(T / 3600.0);
		N.Step(Dt);
		L.Step(N, Hour);
		for (const FEvent& E : N.Events)
		{
			if (E.Kind == EEvent::Done || E.Kind == EEvent::Alight)
			{
				N.ReleaseRider(E.Rider);
			}
		}
		N.Events.clear();
		const int Sec = (int)std::lround(T);
		if (Sec % 30 != 0 || Hour < 6.f)
		{
			continue;
		}
		Departures(N, &L, Aton, Sun, Hour, 6, Rows);
		Samples++;
		MaxRows = std::max(MaxRows, (int)Rows.size());
		Empty += Rows.empty() ? 1 : 0;
		for (size_t I = 0; I < Rows.size(); I++)
		{
			Unsorted += I > 0 && Rows[I].Hour < Rows[I - 1].Hour ? 1 : 0;
			OutOfWindow += Rows[I].Hour < Hour - 0.01f || Rows[I].Hour > Hour + 1.6f ? 1 : 0;
			OutOfWindow += Rows[I].To == Aton || Rows[I].To < 0 ? 1 : 0;
			Planned += Rows[I].Kind == EDepKind::Planned ? 1 : 0;
			Arriving += Rows[I].Kind == EDepKind::Arriving ? 1 : 0;
		}
		// a cabin standing at Aton with its doors open and riders aboard is on the board as boarding
		for (int CI = 0; CI < (int)N.Cabins.size(); CI++)
		{
			const FCabin& C = N.Cabins[(size_t)CI];
			if (C.Line != Sun || C.State == ECabinState::Moving || C.AtStop < 0 || N.Lines[Sun].Stops[C.AtStop].Station != Aton || C.Riders.empty() || C.Door < 0.5f)
			{
				continue;
			}
			bool bOn = false;
			for (const FDeparture& D : Rows)
			{
				bOn |= D.Cabin == CI && D.Kind == EDepKind::Boarding;
			}
			BoardingSeen += bOn ? 1 : 0;
			BoardingMissed += bOn ? 0 : 1;
		}
		for (size_t RI = 0; RI < L.Today.size(); RI++)
		{
			const FLifeRun& R = L.Today[RI];
			const float Ask = R.At + R.HallMinutes / 60.f;
			if (L.Trips[R.Trip].From != Aton || Predicted[RI] >= 0.f || Ask - Hour > 10.f / 60.f || Ask < Hour)
			{
				continue;
			}
			// the planned row nearest their going up
			float Best = -1.f;
			for (const FDeparture& D : Rows)
			{
				if (D.Kind == EDepKind::Planned && (Best < 0.f || std::fabs(D.Hour - Ask) < std::fabs(Best - Ask)))
				{
					Best = D.Hour;
				}
			}
			Predicted[RI] = Best >= 0.f ? Best : -2.f;
		}
	}
	int Compared = 0, Late = 0, NotShown = 0;
	double SumErr = 0.0, MaxErr = 0.0;
	for (size_t RI = 0; RI < L.Today.size(); RI++)
	{
		const FLifeRun& R = L.Today[RI];
		if (L.Trips[R.Trip].From != Aton || R.Wait <= 0.0)
		{
			continue;
		}
		if (Predicted[RI] < -1.f)
		{
			NotShown++;
			continue;
		}
		if (Predicted[RI] < 0.f)
		{
			continue;
		}
		const double Boarded = 5.5 + (R.RideStart + R.Wait) / 3600.0;
		const double Err = std::fabs(Boarded - Predicted[RI]) * 60.0;
		SumErr += Err;
		MaxErr = std::max(MaxErr, Err);
		Late += Err > 5.0 ? 1 : 0;
		Compared++;
	}
	const double Mean = Compared ? SumErr / Compared : 0.0;
	std::printf("Aton's board, Monday: %d looks, up to %d rows, %d planned and %d arriving rows seen; %d riders' departures foretold 10 min ahead "
		"(mean error %.1f min, worst %.1f, %d over 5 min, %d not on the board); boarding shown %d of %d looks\n",
		Samples, MaxRows, Planned, Arriving, Compared, Mean, MaxErr, Late, NotShown, BoardingSeen, BoardingSeen + BoardingMissed);
	CHECK(Empty == 0, "the board was empty %d times", Empty);
	CHECK(Unsorted == 0 && OutOfWindow == 0, "rows out of order %d, out of the hour or to nowhere %d", Unsorted, OutOfWindow);
	CHECK(Planned > 0 && Arriving > 0, "the board shows riders' cabins (%d) and cabins on their way (%d)", Planned, Arriving);
	CHECK(Compared > 20 && Mean < 3.0 && Late * 10 <= Compared, "the board's time ten minutes ahead: mean %.1f min over %d, %d late by over 5",
		Mean, Compared, Late);
	CHECK(NotShown * 10 <= Compared, "riders' departures missing from the board: %d", NotShown);
	CHECK(BoardingMissed == 0 && BoardingSeen > 0, "a cabin boarding at Aton not on the board: %d of %d", BoardingMissed, BoardingSeen + BoardingMissed);
	CHECK(Clock(7.7f) == "07:42" && Clock(24.25f) == "00:15", "the clock: %s %s", Clock(7.7f).c_str(), Clock(24.25f).c_str());

	// the counter's line: the front first, the hero's held spot taken, stepping up
	CHECK(JoinSpot({ -1, -1, -1 }) == 0 && JoinSpot({ 5, -1, -1 }) == 1 && JoinSpot({ -2, -1, -1 }) == 1 && JoinSpot({ 5, 6, 7 }) == -1,
		"joining a line at its first free spot");
	int To = -1;
	CHECK(StepUp({ -1, 5, 6 }, To) == 1 && To == 0, "the second steps up to a free front");
	CHECK(StepUp({ -2, -1, 6 }, To) == 2 && To == 1, "behind the hero they step up to him");
	CHECK(StepUp({ -2, 5, 6 }, To) == -1, "nobody steps past the hero");

	// the usual: three times the same at a counter
	FUsual U;
	U.Add("hall:aton", "coffee");
	U.Add("hall:aton", "coffee");
	U.Add("hall:aton", "pie");
	CHECK(U.Of("hall:aton").empty(), "twice is not the usual yet");
	U.Add("hall:aton", "coffee");
	int Times = 0;
	CHECK(U.Of("hall:aton", &Times) == "coffee" && Times == 3 && U.Visits("hall:aton") == 4 && U.Of("hall:zenit").empty(), "coffee is the usual at Aton");
	FUsual U2;
	U2.Load(U.Save());
	CHECK(U2.Of("hall:aton") == "coffee" && U2.Visits("hall:aton") == 4, "the usual is saved");

	// the lost and found: things come, wait, go; one of a person's at a time; handed back for a gift
	FLostFound F;
	std::vector<float> Hours;
	OwnersAt(L, Aton, F.Owners, Hours);
	CHECK(F.Owners.size() > 20 && Hours.size() == F.Owners.size(), "owners through Aton: %zu", F.Owners.size());
	int Found = 0, MaxShelf = 0;
	for (int D = 0; D < 30; D++)
	{
		const size_t Before = F.Shelf.size();
		F.StartDay(D);
		Found += (int)F.Shelf.size() > (int)Before ? (int)(F.Shelf.size() - Before) : 0;
		MaxShelf = std::max(MaxShelf, (int)F.Shelf.size());
		for (const FLost& X : F.Shelf)
		{
			CHECK(D - X.Day < FLostFound::KeepDays, "a thing on the shelf %d days", D - X.Day);
		}
	}
	CHECK(Found >= 10 && MaxShelf <= FLostFound::MaxShelf, "a month of finds: %d, the shelf holds at most %d", Found, MaxShelf);
	FLost Took;
	if (!F.Shelf.empty())
	{
		CHECK(F.Take(0, Took) && F.Carried.size() == 1, "he takes a thing");
		const std::string Owner = F.Owners[(size_t)Took.Owner];
		CHECK(F.CarriedFor(Owner) == 0 && F.CarriedFor("nobody") == -1, "it is the owner's");
		FLostFound G;
		G.Owners = F.Owners;
		G.Load(F.Save());
		CHECK(G.Carried.size() == 1 && G.Shelf.size() == F.Shelf.size() && G.Day == F.Day, "the lost and found is saved");
		const std::string Gift = F.GiveBack(0);
		bool bKnown = false;
		for (int K = 0; K < NumGifts; K++)
		{
			bKnown |= Gift == Gifts[K];
		}
		CHECK(bKnown && F.Carried.empty() && F.Returned == 1, "handed back for a gift (%s)", Gift.c_str());
		F.StartDay(31);
		for (const FLost& X : F.Shelf)
		{
			CHECK(F.Owners[(size_t)X.Owner] != Owner || X.Day == 31, "the same thing again on the shelf");
		}
	}

	// the travel card: seven stamps, each once
	FTravelCard Card;
	CHECK(!Card.Punch("aton"), "no card, no stamp");
	Card.bHave = true;
	int New = 0;
	for (const FStation& S : N.Stations)
	{
		New += Card.Punch(S.Id) ? 1 : 0;
		New += Card.Punch(S.Id) ? 1 : 0;
	}
	FTravelCard Card2;
	Card2.Load(Card.Save());
	CHECK(New == (int)N.Stations.size() && N.Stations.size() == 7 && Card2.Stamps.size() == 7 && Card2.bHave, "seven stations, seven stamps (%d)", New);

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
	if (Cmd == "board")
	{
		return Board();
	}
	std::printf("usage: run.sh test | town | board\n");
	return 2;
}
