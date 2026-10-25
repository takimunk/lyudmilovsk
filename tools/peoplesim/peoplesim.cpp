// The people's life outside the engine: behaviour checks and a day on the real map (CSV for render_people.py).
//   tools/peoplesim/run.sh test
//   tools/peoplesim/run.sh day [start_hour hours]
//   tools/peoplesim/run.sh trace <key> [start_hour hours]
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/PeopleSim.h"

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
static int Failures = 0;

#define CHECK(Cond, ...) do { if (Cond) { std::printf("  ok   "); } else { std::printf("  FAIL "); Failures++; } std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

static std::string ReadFile(const std::string& Path)
{
	std::ifstream F(Path, std::ios::binary);
	std::stringstream SS;
	SS << F.rdbuf();
	return SS.str();
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

/** A small made-up town: two houses of flats, the library, the club, a bench, the square, a lookout, two stations, town. */
static std::string TestPlaces()
{
	std::string S = "{\"stairs\": [";
	for (int H = 0; H < 3; H++)
	{
		char B[512];
		std::snprintf(B, sizeof B, "%s{\"id\": \"h%d.s0\", \"house\": \"H%d\", \"street\": [%d, 0, 0], \"floors\": [{\"floor\": 0, \"path\": [[%d, 2, 0], [%d, 4, 0]]}, {\"floor\": 1, \"path\": [[%d, 2, 0], [%d, 4, 3], [%d, 4, 3.8]]}]}",
			H ? ", " : "", H, H, H * 40, H * 40, H * 40, H * 40, H * 40, H * 40);
		S += B;
	}
	S += "], \"flats\": [";
	bool bFirst = true;
	for (int H = 0; H < 3; H++)
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
		"{\"id\": \"town\", \"kind\": \"town_square\", \"pos\": [-100, 0, 0]}"
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
			}
			if (E.Kind == EEvent::Arrive && P.Activity >= 0 && (W.Activities[P.Activity].Id == "eat_home" || W.Activities[P.Activity].Id == "eat_club"))
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
		if (Day == 2 && W.Hour >= 20.5f && W.Hour < 20.51f) Snap(AtWed2030);
		for (const FPerson& P : W.People)
		{
			Pairs += P.Partner >= 0 ? 1 : 0;
		}
		if (std::getenv("PEOPLE_DEBUG") && Day == 1 && W.Hour >= 16.75f && W.Hour < 16.7514f)
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
		const float Away = Watch.ActMinutes[P.Key]["work_away"] + Watch.ActMinutes[P.Key]["school"] + Watch.ActMinutes[P.Key]["town"];
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
	CHECK(bFed, "everybody eats (at home or in the club) at least 3 times in 2 days");
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
		std::printf("the real map:\n");
		FWorld W;
		if (!W.Load(DataDir, AnimalsDir))
		{
			std::printf("load failed: %s\n", W.LastError.c_str());
			return 1;
		}
		W.AssignHomes();
		int Cut = 0, Valley = 0, Spur = 0;
		for (const FPlace& P : W.Places)
		{
			(P.Island < 0 ? Cut : (P.Island == 0 ? Valley : Spur))++;
		}
		CHECK(Cut == 0, "every place reachable on foot or by the cable car (valley %d, spur %d, cut off %d)", Valley, Spur, Cut);
		for (const FPlace& P : W.Places)
		{
			if (P.Island < 0)
			{
				std::printf("        cut off: %s at %.1f %.1f\n", P.Id.c_str(), P.Portal.X, P.Portal.Y);
			}
		}
		const auto T0 = std::chrono::high_resolution_clock::now();
		Checks(W, true);
		const double S = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - T0).count();
		std::printf("  the real map's checks took %.2f s\n", S);
	}
	std::printf(Failures ? "%d FAILED\n" : "all ok\n", Failures);
	return Failures ? 1 : 0;
}

static int Day(int Argc, char** Argv, const char* TraceKey)
{
	const float Start = Argc > 0 ? (float)std::atof(Argv[0]) : 16.f;
	const float Hours = Argc > 1 ? (float)std::atof(Argv[1]) : 24.f;
	FWorld W;
	std::ifstream Real(std::string(DataDir) + "/people_places.json");
	if (Real ? !W.Load(DataDir, AnimalsDir) : !LoadTest(W))
	{
		std::printf("load failed: %s\n", W.LastError.c_str());
		return 1;
	}
	W.AssignHomes();
	FClock C = FClock::At(1, Start);
	C.Set(W);
	if (Start == 16.f)
	{
		W.NewGame();
	}
	else
	{
		W.Resettle();
	}
	for (const FPerson& P : W.People)
	{
		std::printf("%-9s %2d %-16s home %s\n", P.Key.c_str(), P.Age, P.Occupation.c_str(), P.Home >= 0 ? W.Places[P.Home].Id.c_str() : "-");
	}
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
		W.Events.clear();
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
	if (Mode == "day")
	{
		return Day(Argc - 2, Argv + 2, nullptr);
	}
	if (Mode == "trace" && Argc > 2)
	{
		return Day(Argc - 3, Argv + 3, Argv[2]);
	}
	std::printf("usage: run.sh test | day [start_hour hours] | trace <key> [start_hour hours]\n");
	return 1;
}
