// The people's life outside the engine: behaviour checks and a day on the real map (CSV for render_people.py).
//   tools/peoplesim/run.sh test
//   tools/peoplesim/run.sh day [start_hour hours]
//   tools/peoplesim/run.sh trace <key> [start_hour hours]
//   tools/peoplesim/run.sh summary [start_hour hours [weekday]]
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/PeopleSim.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Railway/RailRun.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>

using namespace LyudPeople;

static const char* DataDir = "unreal/Lyudmilovsk/Content/PeopleData";
static const char* AnimalsDir = "unreal/Lyudmilovsk/Content/AnimalsData";
static const char* RailBuilt = "unreal/Lyudmilovsk/Content/Railway/railway_built.json";
static int Failures = 0;

#define CHECK(Cond, ...) do { if (Cond) { std::printf("  ok   "); } else { std::printf("  FAIL "); Failures++; } std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

static std::string ReadFile(const std::string& Path)
{
	std::ifstream F(Path, std::ios::binary);
	std::stringstream SS;
	SS << F.rdbuf();
	return SS.str();
}

/** The sea railway's timetable on the sim's clock (seconds since a Monday midnight: whole days, as the game's). */
static LyudRail::FRun Rail;
static bool bRail = false;
/** The railway's clock minus the sim's (the game's life clock counts from the start date: negative before it). */
static double RailShift = 0.0;

static void AttachRail(FWorld& W)
{
	if (!bRail)
	{
		LyudRail::FRunParams P;
		std::string Err;
		if (!LyudRail::LoadParams(ReadFile(RailBuilt), P, Err))
		{
			std::printf("  (no railway timetable: %s; the train rides hidden for %.0f s)\n", Err.c_str(), W.Opt.TrainSeconds);
			return;
		}
		Rail.Build(P);
		bRail = true;
	}
	auto Dir = [](int A, int B) { return Rail.Params().Stops[B].S > Rail.Params().Stops[A].S ? 1 : -1; };
	W.TrainNext = [Dir](const std::string& From, const std::string& To, double Time, double& Doors)
	{
		const int A = Rail.FindStop(From), B = Rail.FindStop(To);
		const double When = A < 0 || B < 0 ? -1.0 : Rail.NextDoorsOpen(A, Dir(A, B), Time + RailShift);
		Doors = When - RailShift;
		return When >= 0.0;
	};
	W.TrainRide = [Dir](const std::string& From, const std::string& To)
	{
		const int A = Rail.FindStop(From), B = Rail.FindStop(To);
		return A < 0 || B < 0 ? -1.0 : Rail.RideSeconds(A, B, Dir(A, B));
	};
}

/** The game's clock: a new game starts on Tuesday 20 October 2026 at 16:00 (UGameCalendar::StartDate). */
struct FClock
{
	double Seconds = 0.0;       // since Monday 00:00 of that week
	void Set(FWorld& W) const
	{
		const double H = Seconds / 3600.0;
		W.Hour = (float)std::fmod(H, 24.0);
		W.Weekday = (int)std::fmod(std::floor(H / 24.0), 7.0);
		W.Time = Seconds;
		W.Sunrise = 7.75f;
		W.Sunset = 17.25f;
		W.bSunUp = W.Hour > W.Sunrise && W.Hour < W.Sunset;
		W.SunYaw = W.Hour < 12.f ? 0.f : 30.f;     // map X is west: the sun sets over the sea
	}
	static FClock At(int Weekday, float Hour) { FClock C; C.Seconds = (Weekday * 24.0 + Hour) * 3600.0; return C; }
};

/** A small made-up town: eight houses of flats, the library, the club, a bench, the square, a lookout, two stations, town. */
static std::string TestPlaces()
{
	const int Houses = 8;     // four flats each: room for every household of people.json (25 now) and a few more
	std::string S = "{\"stairs\": [";
	for (int H = 0; H < Houses; H++)
	{
		char B[512];
		std::snprintf(B, sizeof B, "%s{\"id\": \"h%d.s0\", \"house\": \"H%d\", \"street\": [%d, 0, 0], \"floors\": [{\"floor\": 0, \"path\": [[%d, 2, 0], [%d, 4, 0]]}, {\"floor\": 1, \"path\": [[%d, 2, 0], [%d, 4, 3], [%d, 4, 3.8]]}]}",
			H ? ", " : "", H, H, H * 40, H * 40, H * 40, H * 40, H * 40, H * 40);
		S += B;
	}
	S += "], \"flats\": [";
	bool bFirst = true;
	for (int H = 0; H < Houses; H++)
	{
		for (int F = 0; F < 2; F++)
		{
			for (int Side = 0; Side < 2; Side++)
			{
				const float X = H * 40 + (Side ? 3.f : -3.f), Z = F * 3.8f;
				char B[2048];
				std::snprintf(B, sizeof B,
					"%s{\"id\": \"H%d-%d\", \"house\": \"H%d\", \"stair\": \"h%d.s0\", \"floor\": %d, \"number\": %d, \"door\": [%.1f, 4, %.1f],"
					" \"rooms\": [{\"kind\": \"bedroom\", \"center\": [%.1f, 8, %.1f], \"path\": [[%.1f, 6, %.1f], [%.1f, 8, %.1f]]},"
					" {\"kind\": \"kitchen\", \"center\": [%.1f, 12, %.1f], \"path\": [[%.1f, 6, %.1f], [%.1f, 12, %.1f]]},"
					" {\"kind\": \"living\", \"center\": [%.1f, 16, %.1f], \"path\": [[%.1f, 6, %.1f], [%.1f, 16, %.1f]]}],"
					" \"slots\": [{\"kind\": \"bed\", \"room\": 0, \"pos\": [%.1f, 9, %.1f], \"yaw\": 90, \"capacity\": 2, \"approach\": [%.1f, 8, %.1f], \"height\": 0.5},"
					" {\"kind\": \"bed\", \"room\": 0, \"pos\": [%.1f, 9, %.1f], \"yaw\": 90, \"approach\": [%.1f, 8, %.1f], \"height\": 0.5},"
					" {\"kind\": \"kitchen_seat\", \"room\": 1, \"pos\": [%.1f, 12.5, %.1f], \"yaw\": 0, \"approach\": [%.1f, 12, %.1f]},"
					" {\"kind\": \"kitchen_seat\", \"room\": 1, \"pos\": [%.1f, 13, %.1f], \"yaw\": 0, \"approach\": [%.1f, 12, %.1f]},"
					" {\"kind\": \"sofa\", \"room\": 2, \"pos\": [%.1f, 17, %.1f], \"yaw\": 0, \"capacity\": 2, \"approach\": [%.1f, 16, %.1f]},"
					" {\"kind\": \"desk_seat\", \"room\": 2, \"pos\": [%.1f, 15, %.1f], \"yaw\": 0, \"approach\": [%.1f, 16, %.1f]}]}",
					bFirst ? "" : ", ", H, F * 2 + Side + 1, H, H, F, F * 2 + Side + 1, X, Z,
					X, Z, X, Z, X, Z, X, Z, X, Z, X, Z, X, Z, X, Z, X, Z,
					X - 1, Z, X, Z, X + 1, Z, X, Z, X, Z, X, Z, X + 1, Z, X, Z, X, Z, X, Z, X - 1, Z, X, Z);
				S += B;
				bFirst = false;
			}
		}
	}
	S += "], \"places\": ["
		"{\"id\": \"library\", \"kind\": \"library\", \"door\": [20, -5, 0], \"path\": [[20, -8, 0]], \"slots\": ["
		"{\"kind\": \"read_seat\", \"pos\": [19, -10, 0], \"yaw\": 0, \"approach\": [19, -9, 0]}, {\"kind\": \"read_seat\", \"pos\": [21, -10, 0], \"yaw\": 180, \"approach\": [21, -9, 0]},"
		"{\"kind\": \"read_seat\", \"pos\": [19, -12, 0], \"yaw\": 0, \"approach\": [19, -11, 0]}], \"staff\": {\"kind\": \"staff_seat\", \"pos\": [20, -7, 0], \"yaw\": 90, \"approach\": [20, -7.5, 0]}},"
		"{\"id\": \"bench.0\", \"kind\": \"bench\", \"pos\": [10, -20, 0], \"slots\": [{\"kind\": \"bench\", \"pos\": [9.5, -20, 0], \"yaw\": 270, \"approach\": [9.5, -19.4, 0]}, {\"kind\": \"bench\", \"pos\": [10.5, -20, 0], \"yaw\": 270, \"approach\": [10.5, -19.4, 0]}]},"
		"{\"id\": \"square\", \"kind\": \"square\", \"pos\": [20, -25, 0]},"
		"{\"id\": \"lookout\", \"kind\": \"lookout\", \"pos\": [60, -60, 0], \"yaw\": 30},"
		"{\"id\": \"promenade.0\", \"kind\": \"promenade_bench\", \"pos\": [-40, -60, 0]},"
		"{\"id\": \"station_lower\", \"kind\": \"station_lower\", \"pos\": [0, -80, 0]},"
		"{\"id\": \"station_upper\", \"kind\": \"station_upper\", \"pos\": [100, -80, 0]},"
		"{\"id\": \"town\", \"kind\": \"town_square\", \"pos\": [-100, 0, 0]},"
		"{\"id\": \"lab2_gate\", \"kind\": \"lab\", \"pos\": [-60, 20, 0], \"slots\": [{\"kind\": \"stand\", \"pos\": [-60, 20, 0], \"capacity\": 24}]},"
		"{\"id\": \"kiosk\", \"kind\": \"shop\", \"pos\": [30, -15, 0], \"slots\": [{\"kind\": \"counter\", \"pos\": [30, -15, 0], \"yaw\": 90},"
		" {\"kind\": \"stand\", \"pos\": [29, -13.6, 0], \"yaw\": 270}, {\"kind\": \"stand\", \"pos\": [31, -13.6, 0], \"yaw\": 270}]},"
		"{\"id\": \"beach.0\", \"kind\": \"beach\", \"pos\": [-50, -70, 0]},"
		"{\"id\": \"rail_beach\", \"kind\": \"rail_stop\", \"stop\": \"beach\", \"pos\": [-60, -90, 0], \"way\": [[-62, -100, 0], [-64, -100, 1.5]],"
		" \"slots\": [{\"kind\": \"board\", \"pos\": [-70, -99, 1.5], \"yaw\": 90}, {\"kind\": \"board\", \"pos\": [-75, -99, 1.5], \"yaw\": 90}]},"
		"{\"id\": \"rail_island\", \"kind\": \"rail_stop\", \"stop\": \"island\", \"sea_island\": 1, \"pos\": [-2000, -95, 2], \"way\": [[-2002, -100, 2], [-2004, -100, 1.5]],"
		" \"slots\": [{\"kind\": \"board\", \"pos\": [-2010, -99, 1.5], \"yaw\": 90}, {\"kind\": \"board\", \"pos\": [-2015, -99, 1.5], \"yaw\": 90}]},"
		"{\"id\": \"island_0\", \"kind\": \"island_spot\", \"sea_island\": 1, \"pos\": [-2010, -130, 3]},"
		"{\"id\": \"island_1\", \"kind\": \"island_spot\", \"sea_island\": 1, \"pos\": [-2060, -120, 4]},"
		"{\"id\": \"island_2\", \"kind\": \"island_spot\", \"sea_island\": 1, \"pos\": [-1980, -150, 1]}"
		"], \"club\": {\"entry_path\": [[80, -40, 0], [80, -45, 0], [82, -50, 0]], \"seats\": [";
	for (int I = 0; I <= 13; I++)
	{
		char B[256];
		std::snprintf(B, sizeof B, "%s{\"index\": %d, \"pos\": [84, %d, 0], \"yaw\": 0, \"approach\": [82, %d, 0], \"path\": [[82, %d, 0]], \"height\": 0.47}",
			I ? ", " : "", I, -52 - I * 2, -52 - I * 2, -52 - I * 2);
		S += B;
	}
	S += "], \"vending\": [{\"kind\": \"vending\", \"pos\": [79, -47, 0], \"yaw\": 180}], \"booth\": {\"owner\": {\"kind\": \"booth\", \"pos\": [83, -46, 0], \"yaw\": 180, \"approach\": [82, -46, 0]}},"
		" \"hall_chair\": {\"kind\": \"chair\", \"pos\": [81, -47, 0], \"approach\": [81, -46.5, 0]}}}";
	return S;
}

static bool LoadTest(FWorld& W)
{
	if (!W.LoadPlaces(TestPlaces()) || !W.LoadActivities(ReadFile(std::string(DataDir) + "/activities.json"))
		|| !W.LoadPeople(ReadFile(std::string(DataDir) + "/people.json")))
	{
		std::printf("load failed: %s\n", W.LastError.c_str());
		return false;
	}
	W.AssignHomes();
	AttachRail(W);
	return true;
}

struct FWatch
{
	std::map<std::string, std::map<std::string, float>> ActMinutes;     // person -> activity -> minutes doing it
	std::map<std::string, int> Meals;
	std::map<std::string, float> HungerHigh;     // minutes with hunger > 0.95
	int Overbooked = 0;
	int Talks = 0;
	int Rides = 0;
	int TrainRides = 0;
	std::map<std::string, int> TrainBy;     // person -> train rides
	double LongestWait = 0.0;                // s on a platform
	std::map<std::string, double> GoingSince; // person -> when they set off (the longest way)
	double LongestWay = 0.0;
	std::string LongestWayWho;
	int NoWay = 0;
	std::set<std::string> Seen;
};

static void Run(FWorld& W, FClock& C, float Hours, float Dt, FWatch& Watch, const std::function<void()>& Each = nullptr)
{
	for (double T = 0.0; T < Hours * 3600.0; T += Dt)
	{
		C.Seconds += Dt;
		C.Set(W);
		W.Step(Dt);
		for (const FEvent& E : W.Events)
		{
			const FPerson& P = W.People[E.Person];
			if (E.Kind == EEvent::TalkStart)
			{
				Watch.Talks++;
			}
			if (E.Kind == EEvent::Ride)
			{
				Watch.Rides++;
				if (P.bTrain)
				{
					Watch.TrainRides++;
					Watch.TrainBy[P.Key]++;
				}
			}
			// a meal: at home, in the club, at the diner's booths or at the North's camp table (snacks and the staff's bites
			// at work aren't meals)
			const std::string& Act = P.Activity >= 0 ? W.Activities[P.Activity].Id : std::string();
			if (E.Kind == EEvent::Arrive && (Act == "eat_home" || Act == "eat_club" || Act == "eat_diner" || Act == "eat_camp"))
			{
				Watch.Meals[P.Key]++;
			}
		}
		W.Events.clear();
		for (const FPerson& P : W.People)
		{
			if (P.Activity >= 0 && (P.State == EState::Doing || P.State == EState::Away))
			{
				Watch.ActMinutes[P.Key][W.Activities[P.Activity].Id] += Dt / 60.f;
			}
			if (P.N[(int)ENeed::Hunger] > 0.95f && P.Pose != EPose::Sleep)
			{
				Watch.HungerHigh[P.Key] += Dt / 60.f;
			}
			if (P.Why.rfind("no way", 0) == 0)
			{
				Watch.NoWay++;
			}
			if (P.State == EState::Waiting)
			{
				Watch.LongestWait = std::max(Watch.LongestWait, W.Time - P.StateStart);
			}
			// the whole way to an activity: walking, waiting for the train, riding (a stuck walker never gets there)
			const bool bOnWay = P.State == EState::Going || P.State == EState::Waiting || P.State == EState::Riding;
			if (!bOnWay)
			{
				Watch.GoingSince.erase(P.Key);
			}
			else if (!Watch.GoingSince.count(P.Key))
			{
				Watch.GoingSince[P.Key] = W.Time;
			}
			else if (W.Time - Watch.GoingSince[P.Key] > Watch.LongestWay)
			{
				Watch.LongestWay = W.Time - Watch.GoingSince[P.Key];
				Watch.LongestWayWho = P.Key;
			}
		}
		for (const FSlot& S : W.Slots)
		{
			if ((int)S.Users.size() > S.Capacity)
			{
				if (Watch.Overbooked++ < 3)
				{
					std::printf("        overbooked %s %s/%s:", W.Places[S.Place].Id.c_str(), S.Kind.c_str(), S.Tag.c_str());
					for (int U : S.Users) std::printf(" %s(%s)", W.People[U].Key.c_str(), W.People[U].Activity >= 0 ? W.Activities[W.People[U].Activity].Id.c_str() : "-");
					std::printf("\n");
				}
			}
		}
		if (Each)
		{
			Each();
		}
	}
}

static const FPerson& Who(const FWorld& W, const char* Key) { return W.People[W.FindPerson(Key)]; }

static std::string ActOf(const FWorld& W, const char* Key)
{
	const FPerson& P = Who(W, Key);
	return P.Activity >= 0 ? W.Activities[P.Activity].Id : std::string("-");
}

static void Checks(FWorld& W, bool bReal)
{
	// homes: every household a flat of its own, everybody a bed (a sofa at worst) in it
	std::map<int, std::set<std::string>> FlatHouseholds;
	bool bAllHomes = true, bBeds = true;
	for (const FPerson& P : W.People)
	{
		bAllHomes &= P.Home >= 0;
		if (P.Home >= 0)
		{
			FlatHouseholds[P.Home].insert(P.Household);
			bBeds &= P.BedSlot >= 0 && W.Slots[P.BedSlot].Place == P.Home;
		}
	}
	bool bOneHousehold = true;
	for (const auto& F : FlatHouseholds)
	{
		bOneHousehold &= F.second.size() == 1;
	}
	CHECK(bAllHomes, "everybody has a home (%d people, %d flats taken)", (int)W.People.size(), (int)FlatHouseholds.size());
	CHECK(bOneHousehold, "one household to a flat");
	CHECK(bBeds, "everybody has a bed (or a sofa) at home");
	CHECK(W.People[W.FindPerson("kid_girl")].Home == W.People[W.FindPerson("girl_a")].Home, "Sonya lives with her mother Vera");

	// a new game on Tuesday at 16:00: the club as it is
	FClock C = FClock::At(1, 16.f);
	C.Set(W);
	W.NewGame();
	int AtSeat = 0;
	for (const FPerson& P : W.People)
	{
		if (P.Slot >= 0 && W.Slots[P.Slot].Kind == "club_seat" && W.Slots[P.Slot].Tag == P.ClubSeat && P.State == EState::Doing)
		{
			AtSeat++;
		}
	}
	CHECK(AtSeat == 11, "new game: the 11 regulars at their own stations (%d)", AtSeat);
	CHECK(ActOf(W, "owner") == "work_booth" && Who(W, "owner").Pose == EPose::Work, "new game: the owner at his booth (%s)", ActOf(W, "owner").c_str());
	CHECK(ActOf(W, "clerk") == "work_clerk" && W.Slots[Who(W, "clerk").Slot].Tag == "9", "new game: the clerk at station 9 at work");

	FWatch Watch;
	// two days on
	std::map<std::string, std::string> At3, AtWed10, AtWed12, AtWed2030;
	int Pairs = 0;
	int OnIslandAt3 = 0;
	Run(W, C, 56.f, 0.5f, Watch, [&]()
	{
		auto Snap = [&](std::map<std::string, std::string>& M)
		{
			if (!M.empty())
			{
				return;
			}
			for (const FPerson& P : W.People)
			{
				M[P.Key] = (P.Activity >= 0 ? W.Activities[P.Activity].Id : std::string("-")) + "/" + StateName(P.State);
			}
		};
		const int Day = W.Weekday;
		if (Day == 2 && W.Hour >= 3.f && W.Hour < 3.01f) Snap(At3);
		if (Day == 2 && W.Hour >= 10.f && W.Hour < 10.01f) Snap(AtWed10);
		if (Day == 2 && W.Hour >= 12.f && W.Hour < 12.01f) Snap(AtWed12);
		if (Day == 2 && W.Hour >= 3.f && W.Hour < 3.01f)
		{
			for (const FPerson& P : W.People)
			{
				OnIslandAt3 += P.Island == 2 ? 1 : 0;
			}
		}
		if (Day == 2 && W.Hour >= 20.5f && W.Hour < 20.51f) Snap(AtWed2030);
		for (const FPerson& P : W.People)
		{
			Pairs += P.Partner >= 0 ? 1 : 0;
		}
		if (std::getenv("PEOPLE_DEBUG") && Day == 2 && W.Hour >= 20.4f && W.Hour < 20.4007f)
		{
			for (const FPerson& P : W.People) std::printf("        %s\n        %s\n", W.Describe(P.Id).c_str(), W.Scores(P.Id).c_str());
		}
	});
	int Asleep = 0;
	for (const auto& KV : At3)
	{
		Asleep += KV.second == "sleep/doing" ? 1 : 0;
	}
	CHECK(Asleep == (int)W.People.size(), "Wednesday 03:00: everybody asleep in their bed (%d of %d)", Asleep, (int)W.People.size());
	if (Asleep != (int)W.People.size())
	{
		for (const auto& KV : At3) std::printf("        %s %s\n", KV.first.c_str(), KV.second.c_str());
	}
	CHECK(AtWed10["kid_boy"] == "school/away" && AtWed10["kid_girl"] == "school/away", "Wednesday 10:00: the children at school (%s, %s)",
		AtWed10["kid_boy"].c_str(), AtWed10["kid_girl"].c_str());
	CHECK(AtWed12["clerk"] == "work_clerk/doing", "Wednesday 12:00: the clerk at work in the club (%s)", AtWed12["clerk"].c_str());
	CHECK(AtWed12["girl_b"] == "work_library/doing", "Wednesday 12:00: Olya at the library's desk (%s)", AtWed12["girl_b"].c_str());
	CHECK(AtWed12["owner"] == "work_booth/doing", "Wednesday 12:00: the owner at the booth (%s)", AtWed12["owner"].c_str());
	CHECK(AtWed2030["owner"] == "work_booth/doing" || AtWed2030["owner"].rfind("eat_club", 0) == 0 || AtWed2030["owner"].rfind("snack_club", 0) == 0,
		"Wednesday 20:30: the owner still at the booth, or eating in the club (%s)", AtWed2030["owner"].c_str());
	bool bFed = true, bNotStarved = true;
	for (const FPerson& P : W.People)
	{
		const int Meals = Watch.Meals[P.Key];
		const float Away = Watch.ActMinutes[P.Key]["work_away"] + Watch.ActMinutes[P.Key]["school"] + Watch.ActMinutes[P.Key]["town"]
			+ Watch.ActMinutes[P.Key]["work_lab"] + Watch.ActMinutes[P.Key]["work_sea"];
		if (Meals < 3 && Away < 600.f)
		{
			bFed = false;
			std::printf("        %s ate %d times\n", P.Key.c_str(), Meals);
		}
		if (Watch.HungerHigh[P.Key] > 120.f)
		{
			bNotStarved = false;
			std::printf("        %s very hungry for %.0f min\n", P.Key.c_str(), Watch.HungerHigh[P.Key]);
		}
	}
	CHECK(bFed, "everybody eats (at home, in the club or at the diner) at least 3 times in 2 days");
	CHECK(bNotStarved, "nobody awake stays very hungry for more than 2 hours");
	CHECK(Watch.Overbooked == 0, "no slot ever holds more people than it has room for (%d)", Watch.Overbooked);
	CHECK(Watch.Talks > 0, "people talk to each other (%d conversations)", Watch.Talks);
	std::map<std::string, float> Total;
	for (const auto& P : Watch.ActMinutes)
	{
		for (const auto& A : P.second)
		{
			Total[A.first] += A.second;
		}
	}
	CHECK(Total["play_club"] > 600.f, "people play in the club (%.0f person-minutes)", Total["play_club"]);
	CHECK(Total["read_library"] + Total["read_home"] + Total["homework"] > 60.f, "people read (library %.0f, home %.0f, homework %.0f min)",
		Total["read_library"], Total["read_home"], Total["homework"]);
	CHECK(Total["walk"] + Total["sit_bench"] > 60.f, "people go out into nature (walks %.0f, benches %.0f min)", Total["walk"], Total["sit_bench"]);
	CHECK(Total["sun_watch"] > 0.f, "somebody watches the sun go down (%.0f min)", Total["sun_watch"]);
	CHECK(Total["chat"] > 0.f, "people come out to chat (%.0f min)", Total["chat"]);
	CHECK(Watch.NoWay == 0, "every route found (%d failures)", Watch.NoWay);
	// the Academgorodok's residents (people.json body): work at Laboratory No. 2, the shop, the island by train
	if (W.FindPerson("zotov_lev") >= 0)
	{
		int AtLab = 0, Lab = 0;
		for (const FPerson& P : W.People)
		{
			bool bLab = false;
			for (const FDuty& D : P.Duties)
			{
				bLab |= D.Activity == "work_lab" && (D.Days >> 2) & 1 && D.From <= 11.5f && D.To >= 12.5f;
			}
			Lab += bLab ? 1 : 0;
			AtLab += bLab && AtWed12[P.Key] == "work_lab/away" ? 1 : 0;
		}
		CHECK(AtLab == Lab, "Wednesday 12:00: Laboratory No. 2's staff at work (%d of %d)", AtLab, Lab);
		CHECK(AtWed12["orlova_tanya"] == "work_shop/doing" || AtWed12["orlova_tanya"].rfind("eat_", 0) == 0,
			"Wednesday 12:00: Tanya behind the kiosk's counter (%s)", AtWed12["orlova_tanya"].c_str());
		CHECK(AtWed10["lange_pyotr"] == "work_sea/away", "Wednesday 10:00: Pyotr at sea (%s)", AtWed10["lange_pyotr"].c_str());
		CHECK(W.People[W.FindPerson("sato_yuki")].Home == W.People[W.FindPerson("sato_kei")].Home
			&& W.People[W.FindPerson("sato_mila")].Home == W.People[W.FindPerson("sato_kei")].Home, "the Satos live together");
		CHECK(Total["shop"] > 0.f, "people buy at the kiosk (%.0f person-minutes)", Total["shop"]);
		CHECK(Watch.TrainRides >= 4 && Total["island_walk"] + Total["paint_island"] + Total["birdwatch"] + Total["fish_shore"] > 60.f,
			"people take the train to the island and back (%d rides by %d people; island walks %.0f, painting %.0f, birds %.0f min)",
			Watch.TrainRides, (int)Watch.TrainBy.size(), Total["island_walk"], Total["paint_island"], Total["birdwatch"]);
		CHECK(OnIslandAt3 == 0, "Wednesday 03:00: nobody left on the island (%d)", OnIslandAt3);
		CHECK(Watch.LongestWait < 3700.0, "nobody waits on a platform longer than the hour between trains (%.0f min)", Watch.LongestWait / 60.0);
	}
	CHECK(Watch.LongestWay < 2.5 * 3600.0, "nobody stuck on the way (the longest way %.0f min, %s)", Watch.LongestWay / 60.0, Watch.LongestWayWho.c_str());
	if (bReal)
	{
		CHECK(Watch.Rides > 0, "people ride the cable car between the club and the valley (%d rides)", Watch.Rides);
	}
	std::printf("  minutes by activity over 56 h:");
	for (const auto& A : Total)
	{
		std::printf(" %s %.0f", A.first.c_str(), A.second);
	}
	std::printf("\n");

	// a clock jump: everybody where they'd be (a night: in bed)
	C = FClock::At(3, 2.5f);
	C.Set(W);
	W.Resettle();
	int Beds = 0;
	for (const FPerson& P : W.People)
	{
		Beds += P.Activity >= 0 && W.Activities[P.Activity].Id == "sleep" && P.Slot == P.BedSlot ? 1 : 0;
	}
	CHECK(Beds >= (int)W.People.size() - 1, "a jump to Thursday 02:30: in bed straight away (%d), the owner on his way home or in bed", Beds);
	// an order: Stas to the library
	C = FClock::At(5, 15.f);
	C.Set(W);
	W.Resettle();
	CHECK(W.Order(W.FindPerson("guy_a"), "read_library"), "an order: Stas goes to the library");
	FWatch W2;
	Run(W, C, 0.6f, 0.5f, W2);
	CHECK(ActOf(W, "guy_a") == "read_library" && Who(W, "guy_a").Pose == EPose::Read, "... and reads there (%s, %s)", ActOf(W, "guy_a").c_str(),
		PoseName(Who(W, "guy_a").Pose));

	// late for work: the clerk sets off from home at 10:10 on Wednesday (work from 10:00) and runs outdoors
	C = FClock::At(2, 10.17f);
	C.Set(W);
	W.Resettle();
	W.Order(W.FindPerson("clerk"), "rest_home");
	W.Step(1.f);
	{
		FPerson& Clerk = W.People[W.FindPerson("clerk")];
		Clerk.State = EState::Idle;
		Clerk.Pos = W.Places[Clerk.Home].Portal;
		Clerk.Slot = -1;
		Clerk.bIndoors = false;
	}
	W.Order(W.FindPerson("clerk"), "work_clerk");
	float MaxOut = 0.f;
	FWatch W3;
	Run(W, C, 0.05f, 0.5f, W3, [&]()
	{
		const FPerson& P = Who(W, "clerk");
		if (P.State == EState::Going && !P.bIndoors)
		{
			MaxOut = std::max(MaxOut, P.Speed);
		}
	});
	CHECK(MaxOut > 2.2f, "late for work: the clerk runs outdoors (%.2f m/s)", MaxOut);
	// rain: people on their way outdoors hurry
	W.bRain = true;
	W.Order(W.FindPerson("guy_a"), "sit_bench");
	MaxOut = 0.f;
	Run(W, C, 0.05f, 0.5f, W3, [&]()
	{
		const FPerson& P = Who(W, "guy_a");
		if (P.State == EState::Going && !P.bIndoors)
		{
			MaxOut = std::max(MaxOut, P.Speed);
		}
	});
	W.bRain = false;
	CHECK(MaxOut > 2.2f, "rain: Stas hurries to the bench (%.2f m/s)", MaxOut);}

/** The game's case: the life's clock before the start date (negative sim seconds), two people on the island ordered
 *  home: they must wait on the platform for the timetable's train and ride it (never the hidden fallback), and follow the
 *  timetable when the railway's clock is moved while they wait. */
static void IslandOrder(FWorld& W)
{
	if (!bRail)
	{
		return;
	}
	FClock C = FClock::At(1, 10.f + 41.f / 60.f);
	const double Shift = 200000.0;      // sim seconds = clock - Shift (negative, as in the game on its first day)
	RailShift = Shift;
	auto Set = [&]() { C.Set(W); W.Time = C.Seconds - Shift; };
	Set();
	W.Resettle();
	const char* Keys[] = { "sorokin_oleg", "nishimura_aya" };
	for (const char* K : Keys)
	{
		W.Order(W.FindPerson(K), "island_walk");
	}
	// out there by the train first, then a minute of what they came for
	bool bOnIsland = false;
	for (int I = 0; I < 3 * 3600 && !bOnIsland; I++)
	{
		C.Seconds += 1;
		Set();
		W.Step(1.f);
		bOnIsland = true;
		for (const char* K : Keys)
		{
			bOnIsland &= Who(W, K).Island == 2 && Who(W, K).State == EState::Doing;
		}
	}
	for (int I = 0; I < 60; I++) { C.Seconds += 1; Set(); W.Step(1.f); }
	for (const char* K : Keys)
	{
		W.Order(W.FindPerson(K), "read_home");
	}
	int Waited = 0, Timed = 0, Fallback = 0, Followed = 0;
	std::set<std::string> Home;
	bool bMoved = false;
	std::map<std::string, EState> Last;
	for (int I = 0; I < 2 * 3600; I++)
	{
		C.Seconds += 1;
		Set();
		W.Step(1.f);
		for (const char* K : Keys)
		{
			const FPerson& P = Who(W, K);
			// the railway's clock moved 10 minutes on while the first of them waits: the doors come 10 minutes sooner
			if (P.State == EState::Waiting && !bMoved && P.DoorsAt - W.Time > 900.0)
			{
				const double Was = P.DoorsAt;
				bMoved = true;
				RailShift += 600.0;
				for (int J = 0; J < 12; J++) { C.Seconds += 1; Set(); W.Step(1.f); }
				Followed += std::fabs((Was - P.DoorsAt) - 600.0) < 1.0 ? 1 : 0;
			}
			if (P.State == Last[K])
			{
				continue;
			}
			Waited += P.State == EState::Waiting ? 1 : 0;
			if (P.State == EState::Riding && P.bTrain)
			{
				const bool bTimedRide = Last[K] == EState::Waiting && P.RideUntil > P.DoorsAt + 60.0 && P.RideUntil < P.DoorsAt + 300.0;
				(bTimedRide ? Timed : Fallback)++;
			}
			if (P.State == EState::Doing && P.Island == 0)
			{
				Home.insert(K);
			}
			Last[K] = P.State;
		}
	}
	RailShift = 0.0;
	CHECK(bOnIsland, "the island case: Oleg and Aya taken to the island by the train, the life's clock before the start date (%.0f s)", W.Time);
	CHECK(Waited == 2 && Timed == 2 && Fallback == 0, "ordered home from the island: they wait on the platform (%d) and ride the timetable's train (%d timed, %d hidden fallbacks)",
		Waited, Timed, Fallback);
	CHECK(Followed == 1, "waiting, they follow the timetable when the railway's clock moves (%d)", Followed);
	CHECK(Home.size() == 2, "... and get back to the valley (%d of 2)", (int)Home.size());
}

// winter: stretches of snowy path near the valley's outdoor places; residents clear them with the pusher
static void Snow(FWorld& W)
{
	struct FJob { V3 A, B; int Owner = -1; float Done = 0.f; };
	std::vector<FJob> Jobs;
	for (const FPlace& P : W.Places)
	{
		if (!P.bIndoor && P.Island == 0 && (P.Kind == "bench" || P.Kind == "square" || P.Kind == "promenade") && Jobs.size() < 14)
		{
			FJob J;
			J.A = P.Portal;
			J.B = P.Portal + V3(9.f, 0.f, 0.f);
			Jobs.push_back(J);
		}
	}
	int Double = 0;
	W.SnowJobs = [&](const V3& Near, int Max, std::vector<FWorld::FSnowJob>& Out)
	{
		for (int I = 0; I < (int)Jobs.size() && (int)Out.size() < Max; I++)
		{
			if (Jobs[I].Owner < 0 && Jobs[I].Done < 1.f)
			{
				FWorld::FSnowJob J;
				J.Job = I;
				J.From = Jobs[I].A;
				J.To = Jobs[I].B;
				J.Cm = 10.f;
				Out.push_back(J);
			}
		}
	};
	W.SnowProgress = [&](int Person, int Job, float Progress) -> bool
	{
		FJob& J = Jobs[Job];
		if (Progress == 0.f)
		{
			if (J.Owner >= 0 && J.Owner != Person)
			{
				Double++;
				return false;
			}
			J.Owner = Person;
			return true;
		}
		if (J.Owner != Person)
		{
			Double++;
		}
		if (Progress < 0.f || Progress >= 1.f)
		{
			J.Owner = -1;
		}
		J.Done = std::max(J.Done, Progress);
		return true;
	};
	FClock C = FClock::At(5, 8.f);     // a Saturday morning after a night's snow
	C.Set(W);
	W.Resettle();
	FWatch Watch;
	std::set<std::string> Shovellers;
	float MaxOff = 0.f;
	Run(W, C, 8.f, 1.f, Watch, [&]()
	{
		if ((int)W.Time % 5 == 0)
		{
			W.UpdateSnowSlots(Jobs.empty() ? V3() : Jobs[0].A);
		}
		for (const FPerson& P : W.People)
		{
			if (P.Pose == EPose::Shovel && P.SweepT >= 0.f && P.Slot >= 0)
			{
				Shovellers.insert(P.Key);
				// on the line of the stretch they clear
				const FSlot& Sl = W.Slots[P.Slot];
				const V3 D = Sl.SnowTo - Sl.Pos;
				const float T = std::max(0.f, std::min(1.f, ((P.Pos - Sl.Pos).X * D.X + (P.Pos - Sl.Pos).Y * D.Y) / std::max(D.Len2D() * D.Len2D(), 1e-3f)));
				MaxOff = std::max(MaxOff, (P.Pos - (Sl.Pos + D * T)).Len2D());
			}
		}
	});
	int Done = 0;
	for (const FJob& J : Jobs)
	{
		Done += J.Done >= 1.f;
	}
	CHECK(!Jobs.empty() && !Shovellers.empty(), "winter: residents come out to clear the snow (%d of them, %d stretches)", (int)Shovellers.size(), (int)Jobs.size());
	CHECK(Done * 2 >= (int)Jobs.size(), "winter: most stretches cleared in a morning (%d of %d)", Done, (int)Jobs.size());
	CHECK(Double == 0, "winter: nobody clears a stretch somebody else has (%d clashes)", Double);
	CHECK(MaxOff < 0.3f, "winter: they push along the stretch (%.2f m off it at most)", MaxOff);
	CHECK(Watch.Overbooked == 0, "winter: no slot overbooked (%d)", Watch.Overbooked);
	W.SnowJobs = nullptr;
	W.SnowProgress = nullptr;
}


/** Bicycles (docs/bikes.md): a bike in each of five entrances, owned by a household up that stair; two days. */
/** The North (docs/north.md "People"): the ranger sleeps in his hut, keeps the gate by day, walks his round to the camp
 *  and the hot spring, never gets stuck on the 2 km between the town and the North; the campers sleep in their tents and
 *  sit by the fire with him some evenings; on Saturday morning he walks down to the Gastronom and is back for his round. */
static void NorthLife(FWorld& W)
{
	const int R = W.FindPerson("ranger");
	const int Hut = W.FindPlace("north_hut");
	if (R < 0 || Hut < 0)
	{
		return;
	}
	std::printf("the North:\n");
	const char* Campers[] = { "melnikova_anya", "ito_ren", "savin_gleb", "okada_kiyo" };
	const FPlace& HutP = W.Places[Hut];
	const V3 Camp = W.Places[W.FindPlace("north_campfire")].Portal;
	const V3 Spring = W.Places[W.FindPlace("north_spring")].Portal;
	CHECK(W.People[R].Home == Hut && W.People[R].BedSlot >= 0 && W.Slots[W.People[R].BedSlot].Place == Hut && HutP.Island == 3,
		"the ranger lives in the hut (home %s, island %d)", W.People[R].Home >= 0 ? W.Places[W.People[R].Home].Id.c_str() : "-", HutP.Island);
	bool bTents = true;
	for (const char* K : Campers)
	{
		const FPerson& P = Who(W, K);
		bTents &= P.Home >= 0 && W.Places[P.Home].Id.rfind("north_tent_", 0) == 0 && P.BedSlot >= 0 && W.Slots[P.BedSlot].Place == P.Home;
	}
	CHECK(bTents, "every camper sleeps in a tent of their own");
	const int Trail = W.TrailOf(3);
	CHECK(Trail >= 0 && W.Places[Trail].Island == 0 && W.Places[Trail].WayLen > 100.f, "the trail from the valley's walk grid to the North (%.0f m)",
		Trail >= 0 ? W.Places[Trail].WayLen : 0.f);
	bool bOwnHomes = true;
	for (const FPerson& P : W.People)
	{
		const bool bNorth = P.Key == "ranger" || std::find_if(std::begin(Campers), std::end(Campers), [&](const char* K) { return P.Key == K; }) != std::end(Campers);
		bOwnHomes &= bNorth || P.Home < 0 || !W.Places[P.Home].bPrivate;
	}
	CHECK(bOwnHomes, "nobody else is given the hut or a tent");

	// a weekday: from Tuesday 21:00 (Resettle counts tiredness from this morning's waking: an earlier start would make a
	// morning of the next day look sleepless) to Wednesday midnight
	FClock C = FClock::At(1, 21.f);
	C.Set(W);
	W.Resettle();
	FWatch Watch;
	std::string At3, At10;
	float Gate = 0.f, Round = 0.f, FireTogether = 0.f, CampersAtFire = 0.f;
	float NearCamp = 1e9f, NearSpring = 1e9f;
	int CampersInTentAt3 = 0;
	bool bLeftNorth = false;
	double Way0 = -1.0, LongestWay = 0.0;
	Run(W, C, 27.f, 0.5f, Watch, [&]()
	{
		const FPerson& P = W.People[R];
		const std::string Act = P.Activity >= 0 ? W.Activities[P.Activity].Id : std::string("-");
		const std::string Now = Act + "/" + StateName(P.State) + "@" + (P.Place >= 0 ? W.Places[P.Place].Id : std::string("-"));
		if (W.Hour >= 3.f && W.Hour < 3.01f && At3.empty())
		{
			At3 = Now + (P.Slot == P.BedSlot ? "" : " (not in his bed)");
			for (const char* K : Campers)
			{
				const FPerson& Q = Who(W, K);
				CampersInTentAt3 += Q.Slot == Q.BedSlot && Q.State == EState::Doing ? 1 : 0;
			}
		}
		if (W.Hour >= 10.f && W.Hour < 10.01f && At10.empty()) At10 = Now;
		Gate += Act == "watch_gate" && P.State == EState::Doing ? 0.5f / 60.f : 0.f;
		if (Act == "patrol_north")
		{
			Round += 0.5f / 60.f;
			NearCamp = std::min(NearCamp, (P.Pos - Camp).Len2D());
			NearSpring = std::min(NearSpring, (P.Pos - Spring).Len2D());
		}
		bLeftNorth |= P.Island != 3 && P.State == EState::Doing;
		const bool bOnWay = P.State == EState::Going || P.State == EState::Riding || P.State == EState::Waiting;
		if (!bOnWay)
		{
			Way0 = -1.0;
		}
		else if (Way0 < 0.0)
		{
			Way0 = W.Time;
		}
		else
		{
			LongestWay = std::max(LongestWay, W.Time - Way0);
		}
		int AtFire = 0;
		for (const char* K : Campers)
		{
			const FPerson& Q = Who(W, K);
			AtFire += Q.Activity >= 0 && W.Activities[Q.Activity].Id == "campfire" && Q.State == EState::Doing ? 1 : 0;
		}
		CampersAtFire += AtFire * 0.5f / 60.f;
		FireTogether += AtFire > 0 && Act == "campfire" && P.State == EState::Doing ? 0.5f / 60.f : 0.f;
	});
	CHECK(At3.rfind("sleep/doing@north_hut", 0) == 0 && At3.find("not in") == std::string::npos, "Wednesday 03:00: the ranger asleep in his bunk (%s)", At3.c_str());
	CHECK(CampersInTentAt3 == 4, "Wednesday 03:00: the campers asleep in their tents (%d of 4)", CampersInTentAt3);
	CHECK(At10.rfind("watch_gate/", 0) == 0 || At10.rfind("stove_tea", 0) == 0 || At10.rfind("eat_", 0) == 0,
		"Wednesday 10:00: the ranger at the gate, or on a short break (%s)", At10.c_str());
	CHECK(Gate > 180.f, "the ranger keeps the gate in the morning (%.0f min)", Gate);
	CHECK(Round > 90.f && NearCamp < 40.f && NearSpring < 25.f, "his round reaches the camp and the hot spring (%.0f min, %.0f m and %.0f m from them)",
		Round, NearCamp, NearSpring);
	CHECK(!bLeftNorth, "on a weekday he stays in the North");
	CHECK(LongestWay < 45.0 * 60.0, "the ranger never stuck on a way (the longest %.0f min)", LongestWay / 60.0);
	CHECK(CampersAtFire > 120.f, "the campers sit by the fire in the evening (%.0f person-minutes)", CampersAtFire);
	CHECK(FireTogether > 0.f, "the ranger by the fire with the campers (%.0f min)", FireTogether);
	CHECK(Watch.NoWay == 0, "every route found on the weekday (%d failures)", Watch.NoWay);

	// Saturday: down to the Gastronom in the morning, back up for his round
	C = FClock::At(5, 6.5f);
	C.Set(W);
	W.Resettle();
	bool bShop = false, bBack = false;
	double ShopAt = -1.0;
	FWatch W2;
	Run(W, C, 7.f, 0.5f, W2, [&]()
	{
		const FPerson& P = W.People[R];
		const std::string Act = P.Activity >= 0 ? W.Activities[P.Activity].Id : std::string();
		if (Act == "supplies_town" && P.State == EState::Doing && !bShop)
		{
			bShop = true;
			ShopAt = W.Hour;
		}
		bBack |= bShop && Act == "patrol_north" && P.State == EState::Doing && P.Island == 3;
	});
	CHECK(bShop, "Saturday: the ranger walks down to the Gastronom for supplies (at %02d:%02d)", (int)ShopAt, (int)(std::fmod(ShopAt, 1.0) * 60.0));
	CHECK(bBack, "... and is back in the North for his round");
	CHECK(W2.NoWay == 0, "every route found on Saturday (%d failures)", W2.NoWay);
}

static void Bikes(FWorld& W)
{
	std::printf("bicycles:\n");
	W.NewGame();
	int N = 0;
	for (const FWorld::FLobby& L : W.Lobbies)
	{
		if (N < 5)
		{
			W.AddBike("bike_" + L.Stair, L.Stair, L.Lobby, 0.f);
			N++;
		}
	}
	W.AssignBikes();
	int Owned = 0;
	for (const FWorld::FBike& B : W.Bikes)
	{
		Owned += B.Owner.empty() ? 0 : 1;
		std::printf("        %s owner %s\n", B.Id.c_str(), B.Owner.empty() ? "-" : B.Owner.c_str());
	}
	CHECK(Owned >= 3, "entrance bikes have owners (%d of %d)", Owned, (int)W.Bikes.size());
	FClock C;
	C.Seconds = (1 * 24 + 6) * 3600.0;       // Tuesday 06:00
	C.Set(W);
	W.Resettle();
	FWatch Watch;
	std::map<std::string, int> Rides;
	int Takes = 0, Parks = 0, ParksHome = 0, HiddenWithBike = 0, LongStops = 0, Ridden = 0;
	double RideM = 0.0;
	std::map<int, double> StopSince;
	Run(W, C, 40.f, 1.f, Watch, [&]()
	{
		for (const FPerson& P : W.People)
		{
			if (P.Bike >= 0 && P.BikeMode > 0 && (P.Pose == EPose::Hidden || P.State == EState::Away))
			{
				HiddenWithBike++;
			}
			if (P.BikeAct != EBikeAct::None)
			{
				if (!StopSince.count(P.Id))
				{
					StopSince[P.Id] = W.Time;
					if (P.BikeAct == EBikeAct::Take || P.BikeAct == EBikeAct::TakeMount)
					{
						Takes++;
						Rides[P.Key]++;
					}
					if (P.BikeAct == EBikeAct::Park)
					{
						Parks++;
						ParksHome += (W.Bikes[P.Bike].Pos - W.Bikes[P.Bike].Home).Len() < 0.05f ? 1 : 0;
					}
				}
				else if (W.Time - StopSince[P.Id] > 10.0)
				{
					LongStops++;
				}
			}
			else
			{
				StopSince.erase(P.Id);
			}
			if (P.Bike >= 0 && P.BikeMode == 2 && P.State == EState::Going)
			{
				Ridden++;
				RideM += P.Speed;
			}
		}
	});
	for (const auto& R : Rides)
	{
		std::printf("        %s took their bike %d times\n", R.first.c_str(), R.second);
	}
	CHECK(Takes >= 4, "residents take their bikes (%d times, %.1f km ridden)", Takes, RideM / 1000.0);
	CHECK(Parks >= Takes - (int)W.Bikes.size(), "and park them again (%d, %d back in their entrance)", Parks, ParksHome);
	CHECK(ParksHome >= 1, "a bike goes back home into its entrance");
	CHECK(HiddenWithBike == 0, "nobody away or hidden with a bike in hand (%d s)", HiddenWithBike);
	CHECK(LongStops == 0, "no stop with a bike lasts over 10 s (%d)", LongStops);
	int Riders = 0;
	for (const FWorld::FBike& B : W.Bikes)
	{
		Riders += B.Rider >= 0 && W.People[B.Rider].BikeMode == 0 && W.People[B.Rider].State != EState::Going ? 1 : 0;
	}
	CHECK(Riders == 0, "no bike reserved by someone who isn't on the way to it (%d)", Riders);
}

static int Test()
{
	std::printf("a made-up town:\n");
	{
		FWorld W;
		if (!LoadTest(W))
		{
			return 1;
		}
		Checks(W, false);
	}
	std::ifstream Real(std::string(DataDir) + "/people_places.json");
	if (Real)
	{
		std::printf("the real map (the story's people; the town: run.sh town):\n");
		FWorld W;
		W.bLoadTown = false;
		if (!W.Load(DataDir, AnimalsDir))
		{
			std::printf("load failed: %s\n", W.LastError.c_str());
			return 1;
		}
		W.AssignHomes();
		AttachRail(W);
		int Cut = 0, Valley = 0, Spur = 0, Sea = 0, North = 0;
		for (const FPlace& P : W.Places)
		{
			(P.Island < 0 ? Cut : (P.Island == 0 ? Valley : (P.Island == 1 ? Spur : (P.Island == 2 ? Sea : North))))++;
		}
		CHECK(Cut == 0, "every place reachable on foot, by the cable car or the train (valley %d, spur %d, the sea island %d, the North %d, cut off %d)",
			Valley, Spur, Sea, North, Cut);
		for (const FPlace& P : W.Places)
		{
			if (P.Island < 0)
			{
				std::printf("        cut off: %s at %.1f %.1f\n", P.Id.c_str(), P.Portal.X, P.Portal.Y);
			}
		}
		const auto T0 = std::chrono::high_resolution_clock::now();
		Checks(W, true);
		IslandOrder(W);
		Snow(W);
		Bikes(W);
		NorthLife(W);
		const double S = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - T0).count();
		std::printf("  the real map's checks took %.2f s\n", S);
	}
	std::printf(Failures ? "%d FAILED\n" : "all ok\n", Failures);
	return Failures ? 1 : 0;
}


/** The town's life (docs/town_life.md): the townsfolk of townsfolk.json in the town's places, a day and a night. */
static int Town()
{
	FWorld W;
	if (!W.Load(DataDir, AnimalsDir))
	{
		std::printf("load failed: %s\n", W.LastError.c_str());
		return 1;
	}
	W.AssignHomes();
	AttachRail(W);
	int Town = 0, Homed = 0, Wrong = 0;
	for (const FPerson& P : W.People)
	{
		if (!P.bTown)
		{
			continue;
		}
		Town++;
		Homed += P.Home >= 0 ? 1 : 0;
		Wrong += P.Home >= 0 && W.Places[P.Home].Id != P.HomeId ? 1 : 0;
	}
	std::printf("the town: %d townsfolk, %zu places, %zu slots\n", Town, W.Places.size(), W.Slots.size());
	CHECK(Town > 100, "townsfolk loaded (%d)", Town);
	CHECK(Homed == Town && Wrong == 0, "every townsperson lives in their own flat (%d of %d, %d elsewhere)", Homed, Town, Wrong);
	// Wednesday 04:00 to Thursday 16:00
	FClock C = FClock::At(2, 4.f);
	C.Set(W);
	W.Resettle();
	std::map<std::string, double> Minutes;      // activity -> person-minutes (townsfolk)
	std::map<std::string, int> Meals;
	std::map<std::string, float> Hungry;
	int Lit21 = -1, Lit03 = -1, Lit12 = -1, Occupied = 0, Asleep03 = 0, Awake03 = 0, AwakeNightType03 = 0;
	int Waiters12 = -1, Sellers10 = -1, Pupils10 = -1, PupilsOff = -1, AtSquare = -1, Overbooked = 0;
	double Longest = 0.0;
	std::string LongestWho;
	std::map<int, double> Since;
	for (const FPlace& Pl : W.Places)
	{
		Occupied += !Pl.Windows.empty() && !Pl.Residents.empty() ? 1 : 0;
	}
	std::vector<int> Changed, Staff;
	double Ms = 0.0;
	long Steps = 0;
	const float Dt = 1.f;
	bool bGather = false, bDayOff = false;
	for (double T = 0.0; T < 36.0 * 3600.0; T += Dt)
	{
		C.Seconds += Dt;
		C.Set(W);
		const float H = W.Hour;
		const int Day = W.Weekday;
		// a holiday on the square Wednesday 18-22 (as ULyudFestivalSubsystem would set it)
		if (!bGather && Day == 2 && H >= 17.9f)
		{
			W.SetGathering("test_fest", "S-SQUARE", 18.f, 22.f, 1.6f, EGatherMode::Mingle, false, V3());
			bGather = true;
		}
		if (bGather && Day == 2 && H >= 22.f)
		{
			W.ClearGathering("test_fest");
		}
		// Thursday is a day off: no school
		if (!bDayOff && Day == 3 && H >= 6.f)
		{
			W.bDayOff = true;
			bDayOff = true;
		}
		const auto T0 = std::chrono::high_resolution_clock::now();
		W.Step(Dt);
		Ms += std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - T0).count();
		Steps++;
		for (const FEvent& E : W.Events)
		{
			const FPerson& P = W.People[E.Person];
			const std::string Act = P.Activity >= 0 ? W.Activities[P.Activity].Id : std::string();
			if (P.bTown && E.Kind == EEvent::Arrive && (Act == "eat_home" || Act == "eat_out" || Act == "eat_diner" || Act == "eat_club"))
			{
				Meals[P.Key]++;
			}
		}
		W.Events.clear();
		const int Sec = (int)std::lround(std::fmod(C.Seconds, 86400.0));
		const bool bTick = Sec % 60 == 0;
		if (bTick)
		{
			for (const FPerson& P : W.People)
			{
				if (!P.bTown)
				{
					continue;
				}
				if (P.Activity >= 0 && (P.State == EState::Doing || P.State == EState::Away))
				{
					Minutes[W.Activities[P.Activity].Id] += 1.0;
				}
				if (P.N[(int)ENeed::Hunger] > 0.95f && P.Pose != EPose::Sleep)
				{
					Hungry[P.Key] += 1.f;
				}
				const bool bWay = P.State == EState::Going || P.State == EState::Waiting || P.State == EState::Riding;
				if (!bWay)
				{
					Since.erase(P.Id);
				}
				else if (!Since.count(P.Id))
				{
					Since[P.Id] = W.Time;
				}
				else if (W.Time - Since[P.Id] > Longest)
				{
					Longest = W.Time - Since[P.Id];
					LongestWho = P.Key;
				}
			}
			for (const FSlot& S : W.Slots)
			{
				Overbooked += (int)S.Users.size() > S.Capacity ? 1 : 0;
			}
		}
		W.UpdateLights(Changed);
		auto LitNow = [&]() { int N = 0; for (size_t I = 0; I < W.FlatLit.size(); I++) N += W.FlatLit[I] == 1 ? 1 : 0; return N; };
		if (Day == 2 && Sec == 21 * 3600) Lit21 = LitNow();
		if (Day == 2 && Sec == 12 * 3600) Lit12 = LitNow();
		if (Day == 2 && Sec == 12 * 3600) { W.StaffOnShift("restaurant", "waiter", Staff); Waiters12 = (int)Staff.size(); }
		if (Day == 2 && Sec == 10 * 3600) { W.StaffOnShift("P-MARKET", "seller", Staff); Sellers10 = (int)Staff.size(); W.StaffOnShift("P-SCHOOL", "pupil", Staff); Pupils10 = (int)Staff.size(); }
		if (Day == 3 && Sec == 10 * 3600) { W.StaffOnShift("P-SCHOOL", "pupil", Staff); PupilsOff = (int)Staff.size(); }
		if (Day == 2 && Sec == 20 * 3600)
		{
			const int Sq = W.FindPlace("S-SQUARE");
			AtSquare = 0;
			for (const FPerson& P : W.People) AtSquare += P.Place == Sq && P.State == EState::Doing ? 1 : 0;
		}
		if (Day == 3 && Sec == 3 * 3600)
		{
			Lit03 = LitNow();
			for (const FPerson& P : W.People)
			{
				if (!P.bTown) continue;
				const bool bSleep = P.Pose == EPose::Sleep && P.Place == P.Home;
				(bSleep ? Asleep03 : Awake03)++;
				AwakeNightType03 += !bSleep && (P.Chronotype == "night" || P.Chronotype == "owl") ? 1 : 0;
			}
		}
	}
	int Fed = 0, Starving = 0;
	for (const FPerson& P : W.People)
	{
		if (P.bTown)
		{
			Fed += Meals[P.Key] >= 2 ? 1 : 0;
			Starving += Hungry[P.Key] > 180.f ? 1 : 0;
		}
	}
	std::printf("  minutes by activity (townsfolk, 36 h):");
	for (const auto& M : Minutes) std::printf(" %s %.0f", M.first.c_str(), M.second);
	std::printf("\n");
	CHECK(Asleep03 > Town * 0.8, "Thursday 03:00: most asleep in their own beds (%d asleep, %d up)", Asleep03, Awake03);
	CHECK(AwakeNightType03 > 0, "... but the night owls are up (%d)", AwakeNightType03);
	CHECK(Minutes["watch_sunrise"] > 0, "early risers watch the sunrise (%.0f person-minutes)", Minutes["watch_sunrise"]);
	CHECK(Minutes["night_walk"] > 0, "somebody walks at night (%.0f person-minutes)", Minutes["night_walk"]);
	CHECK(Minutes["sit_bench"] > 300, "people sit on the benches (%.0f person-minutes)", Minutes["sit_bench"]);
	CHECK(Minutes["work_town"] > 3000 && Minutes["work_office"] > 1000, "the town's shops, cafés and offices are staffed (%.0f / %.0f person-minutes)", Minutes["work_town"], Minutes["work_office"]);
	CHECK(Minutes["eat_out"] + Minutes["coffee"] > 200, "people eat out and have coffee (%.0f + %.0f)", Minutes["eat_out"], Minutes["coffee"]);
	CHECK(Waiters12 > 0, "Wednesday 12:00: waiters on shift at the restaurant (%d)", Waiters12);
	CHECK(Sellers10 > 0, "Wednesday 10:00: sellers at the market (%d)", Sellers10);
	CHECK(Pupils10 > 10, "Wednesday 10:00: pupils at school (%d)", Pupils10);
	CHECK(PupilsOff == 0, "Thursday (a day off) 10:00: nobody at school (%d)", PupilsOff);
	CHECK(AtSquare > 40, "a holiday on the square at 20:00 draws the town (%d there)", AtSquare);
	CHECK(Lit12 == 0, "windows dark at noon (%d lit)", Lit12);
	CHECK(Lit21 > Occupied * 0.33, "windows lit at 21:00, a holiday on the square still on (%d of %d lived-in flats with windows)", Lit21, Occupied);
	CHECK(Lit03 >= 0 && Lit03 < Occupied * 0.2, "few windows lit at 03:00 (%d)", Lit03);
	CHECK(Fed > Town * 0.9, "townsfolk eat at least twice in 36 h (%d of %d)", Fed, Town);
	CHECK(Starving < Town * 0.03, "few stay very hungry for 3 hours (%d)", Starving);
	CHECK(Overbooked == 0, "no slot overbooked (%d)", Overbooked);
	CHECK(Longest < 100 * 60, "nobody on the way for more than 100 min (the island by train included) (%.0f min, %s)", Longest / 60.0, LongestWho.c_str());
	CHECK(Ms / Steps < 2.0, "a step of the whole town costs %.3f ms (%zu people)", Ms / Steps, W.People.size());
	{
		// a load at Friday 02:00, as the game does it: the townsfolk placed over a few steps, the windows mostly dark
		W.Opt.SettleBatch = 120;
		FClock::At(4, 2.f).Set(W);
		auto T0 = std::chrono::steady_clock::now();
		W.Resettle();
		const double ResettleMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
		double WorstMs = 0.0;
		int SettleSteps = 0;
		for (; SettleSteps < 60 && W.Settling(); SettleSteps++)
		{
			T0 = std::chrono::steady_clock::now();
			W.Step(0.25f);
			WorstMs = std::max(WorstMs, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count());
		}
		W.FlatLit.assign(W.Places.size(), -1);
		W.UpdateLights(Changed);
		int LitLoad = 0, Unplaced = 0;
		for (size_t I = 0; I < W.FlatLit.size(); I++) LitLoad += W.FlatLit[I] == 1 ? 1 : 0;
		for (const FPerson& P : W.People) Unplaced += P.bTown && P.Activity < 0 && P.State == EState::Idle ? 1 : 0;
		CHECK(!W.Settling() && Unplaced < Town * 0.05, "a load places the townsfolk over %d steps (%d unplaced; Resettle %.1f ms, worst step %.1f ms)", SettleSteps, Unplaced, ResettleMs, WorstMs);
		CHECK(LitLoad < Occupied * 0.2, "after a load at 02:00 few windows are lit (%d of %d)", LitLoad, Occupied);
	}
	{
		// a festival night: Saturday's dance on the square till 02:30, everybody up for it but the small children
		W.Opt.SettleBatch = 0;
		FClock N = FClock::At(5, 19.f);
		N.Set(W);
		W.Resettle();
		W.SetGathering("test_dance", "S-SQUARE", 21.f, 2.5f, 1.6f, EGatherMode::Mingle, false, V3());
		W.SetStayUp(26.5f);
		const double End = N.Seconds + 6.5 * 3600.0;   // 01:30
		while (N.Seconds < End)
		{
			N.Seconds += 1.0;
			N.Set(W);
			W.Step(1.f);
		}
		const int Sq = W.FindPlace("S-SQUARE");
		int Dancing = 0, KidsUp = 0;
		for (const FPerson& P : W.People)
		{
			if (!P.bTown) continue;
			Dancing += P.Place == Sq && P.State == EState::Doing && P.Age >= 12 ? 1 : 0;
			KidsUp += P.Age < 12 && P.Activity >= 0 && W.Activities[P.Activity].Pose != EPose::Sleep ? 1 : 0;
		}
		W.SetStayUp(0.f);
		W.ClearGathering("test_dance");
		CHECK(Dancing > 40, "a festival night: at 01:30 the square still dances (%d)", Dancing);
		CHECK(KidsUp < 10, "... but the small children are in bed (%d up)", KidsUp);
	}
	std::printf(Failures ? "%d FAILED\n" : "all ok\n", Failures);
	return Failures ? 1 : 0;
}

static int Day(int Argc, char** Argv, const char* TraceKey, bool bSummary = false)
{
	const float Start = Argc > 0 ? (float)std::atof(Argv[0]) : (bSummary ? 4.f : 16.f);
	const float Hours = Argc > 1 ? (float)std::atof(Argv[1]) : 24.f;
	const int Weekday = Argc > 2 ? std::atoi(Argv[2]) % 7 : (bSummary ? 2 : 1);
	FWorld W;
	std::ifstream Real(std::string(DataDir) + "/people_places.json");
	if (Real ? !W.Load(DataDir, AnimalsDir) : !LoadTest(W))
	{
		std::printf("load failed: %s\n", W.LastError.c_str());
		return 1;
	}
	W.AssignHomes();
	AttachRail(W);
	FClock C = FClock::At(Weekday, Start);
	C.Set(W);
	if (Start == 16.f && Weekday == 1)
	{
		W.NewGame();
	}
	else
	{
		W.Resettle();
	}
	for (const FPerson& P : W.People)
	{
		if (!bSummary)
		{
			std::printf("%-9s %2d %-16s home %s\n", P.Key.c_str(), P.Age, P.Occupation.c_str(), P.Home >= 0 ? W.Places[P.Home].Id.c_str() : "-");
		}
	}
	// the summary: per person their day as blocks of what they did (10 minutes or more), the ways in between left out
	struct FBlock { float From; std::string What; double Secs; };
	std::vector<std::vector<FBlock>> Blocks(W.People.size());
	std::vector<int> Trains(W.People.size(), 0);
	FILE* F = std::fopen("Saved/PeopleSim/day.csv", "w");
	std::fprintf(F, "hour,key,activity,state,pose,x,y,z\n");
	FILE* A = std::fopen("Saved/PeopleSim/acts.txt", "w");
	const int Trace = TraceKey ? W.FindPerson(TraceKey) : -1;
	std::vector<std::string> Last(W.People.size());
	double Ms = 0.0;
	long Steps = 0;
	float NextRow = 0.f;
	const float Dt = 0.5f;
	for (double T = 0.0; T < Hours * 3600.0; T += Dt)
	{
		C.Seconds += Dt;
		C.Set(W);
		const auto T0 = std::chrono::high_resolution_clock::now();
		W.Step(Dt);
		Ms += std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - T0).count();
		Steps++;
		for (const FEvent& E : W.Events)
		{
			if (E.Kind == EEvent::Ride && W.People[E.Person].bTrain)
			{
				Trains[E.Person]++;
				const FPerson& P = W.People[E.Person];
				Blocks[E.Person].push_back({ W.Hour, std::string("train ") + (P.RailFrom >= 0 ? W.Places[P.RailFrom].Stop : "?") + ">"
					+ (P.RailTo >= 0 ? W.Places[P.RailTo].Stop : "?"), 600.0 });
			}
		}
		W.Events.clear();
		for (const FPerson& P : W.People)
		{
			if (P.Activity >= 0 && (P.State == EState::Doing || P.State == EState::Away || P.State == EState::Settling))
			{
				const std::string What = W.Activities[P.Activity].Id;
				std::vector<FBlock>& B = Blocks[P.Id];
				if (B.empty() || B.back().What != What)
				{
					if (!B.empty() && B.back().Secs < 600.0 && B.back().What.rfind("train", 0) != 0)
					{
						B.pop_back();       // a short one (a few minutes at the kiosk counts: shop)
					}
					if (B.empty() || B.back().What != What)
					{
						B.push_back({ W.Hour, What, 0.0 });
					}
				}
				B.back().Secs += Dt;
				if (What == "shop" && B.back().Secs < 600.0)
				{
					B.back().Secs = 600.0;
				}
			}
		}
		for (const FPerson& P : W.People)
		{
			const std::string Now = (P.Activity >= 0 ? W.Activities[P.Activity].Id : std::string("-")) + " " + StateName(P.State);
			if (Now != Last[P.Id])
			{
				Last[P.Id] = Now;
				static const char* DayNames[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
				std::fprintf(A, "%s %02d:%02d %-9s %s%s\n", DayNames[W.Weekday % 7], (int)W.Hour, (int)(std::fmod(W.Hour, 1.f) * 60.f),
					P.Key.c_str(), Now.c_str(), P.State == EState::Going ? (" -> " + (P.Place >= 0 ? W.Places[P.Place].Id : std::string())).c_str() : "");
				if (P.Id == Trace)
				{
					std::printf("%02d:%02d %s\n", (int)W.Hour, (int)(std::fmod(W.Hour, 1.f) * 60.f), W.Describe(P.Id).c_str());
				}
			}
		}
		if (T >= NextRow)
		{
			NextRow += 30.f;
			for (const FPerson& P : W.People)
			{
				std::fprintf(F, "%.4f,%s,%s,%s,%s,%.2f,%.2f,%.2f\n", W.Hour, P.Key.c_str(), P.Activity >= 0 ? W.Activities[P.Activity].Id.c_str() : "-",
					StateName(P.State), PoseName(P.Pose), P.Pos.X, P.Pos.Y, P.Pos.Z);
			}
		}
	}
	std::fclose(F);
	std::fclose(A);
	if (bSummary)
	{
		static const char* DayNames[] = { "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" };
		std::printf("%s %02d:00, %.0f h (the residents of the Academgorodok; the club's people: run.sh day)\n", DayNames[Weekday], (int)Start, Hours);
		for (const FPerson& P : W.People)
		{
			if (P.Body == P.Mesh)
			{
				continue;
			}
			std::string Line;
			for (const FBlock& B : Blocks[P.Id])
			{
				if (B.Secs < 600.0)
				{
					continue;
				}
				char Buf[96];
				std::snprintf(Buf, sizeof Buf, "%s%02d:%02d %s", Line.empty() ? "" : ", ", (int)B.From, (int)(std::fmod(B.From, 1.f) * 60.f), B.What.c_str());
				Line += Buf;
			}
			std::printf("%-14s %2d %-15s %s\n", P.Key.c_str(), P.Age, P.Occupation.c_str(), Line.c_str());
		}
	}
	std::printf("%ld steps, %.4f ms a step for %d people\n", Steps, Ms / Steps, (int)W.People.size());
	return 0;
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "test";
	if (Mode == "test")
	{
		return Test();
	}
	if (Mode == "town")
	{
		return Town();
	}
	if (Mode == "day")
	{
		return Day(Argc - 2, Argv + 2, nullptr);
	}
	if (Mode == "summary")
	{
		return Day(Argc - 2, Argv + 2, nullptr, true);
	}
	if (Mode == "trace" && Argc > 2)
	{
		return Day(Argc - 3, Argv + 3, Argv[2]);
	}
	std::printf("usage: run.sh test | day [start_hour hours] | trace <key> [start_hour hours]\n");
	return 1;
}
