// The things to do inside the town's buildings, outside the engine: behaviour checks of CityThingsModel.h on a small
// made-up fixture (tools/citysim/fixture.json), then on the real Content/City/interiors.json when it has been built
// (python3 tools/city/interiors.py).
//   tools/citysim/run.sh            the checks (exit code 1 on a failure)
//   tools/citysim/run.sh <file>     the checks on another interiors file instead of the real one
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/City/CityThingsModel.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <string>

using namespace LyudCity;

static int Failures = 0;
static int Passed = 0;

#define CHECK(Cond, ...) do { if (Cond) { std::printf("  ok   "); Passed++; } else { std::printf("  FAIL "); Failures++; } std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

static bool ReadFile(const std::string& Path, std::string& Out)
{
	std::ifstream F(Path, std::ios::binary);
	if (!F)
	{
		return false;
	}
	std::stringstream SS;
	SS << F.rdbuf();
	Out = SS.str();
	return true;
}

static FWhen At(int Weekday, float Hour, long long Day = 100)
{
	FWhen W;
	W.Weekday = Weekday;
	W.Hour = Hour;
	W.Day = Day;
	return W;
}

static FVec V(double X, double Y, double Z)
{
	FVec P;
	P.X = X;
	P.Y = Y;
	P.Z = Z;
	return P;
}

static FVec Unit(const FVec& P)
{
	const double L = std::sqrt(P.X * P.X + P.Y * P.Y + P.Z * P.Z);
	return V(P.X / L, P.Y / L, P.Z / L);
}

static void FixtureChecks()
{
	std::printf("fixture (tools/citysim/fixture.json)\n");
	std::string Text, Error;
	CHECK(ReadFile("tools/citysim/fixture.json", Text), "the fixture is there");
	FModel M;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	CHECK(M.Things.size() == 9, "9 things (a duplicate id dropped): %d", (int)M.Things.size());
	CHECK(M.Shops.size() == 2 && M.FindShop("library") && M.FindShop("library")->size() == 2, "2 shops, the library's with 2 goods");
	CHECK(M.FindShop("kiosk") && (*M.FindShop("kiosk"))[0].Count == 1 && M.FindShop("library")->at(1).Count == 1,
		"shop goods: count defaults to 1, the {goods: []} form reads");
	CHECK(M.FindShop("nope") == nullptr, "an unknown shop is null");

	// defaults
	const int Chair = M.FindThing("P-LIBRARY/chair_1");
	const int Bed = M.FindThing("P-HOTEL/bed_1");
	const int Bag = M.FindThing("P-GYM/bag");
	const int Odd = M.FindThing("P-ODD/thing");
	const int Leaflets = M.FindThing("P-LIBRARY/leaflets");
	CHECK(Chair >= 0 && M.ActOf(Chair)->Pose == "sit" && M.ActOf(Chair)->Prompt == "city.act.sit.prompt", "a seat without an act sits");
	CHECK(Bed >= 0 && M.ActOf(Bed)->Pose == "lie" && M.ActOf(Bed)->bFade && M.ActOf(Bed)->Minutes == 120, "a bed rests 2 h with a fade");
	CHECK(Bag >= 0 && M.ActOf(Bag)->Id == "_sport" && M.ActOf(Bag)->Target == "punching_bag", "a sport thing gets _sport, target = what");
	CHECK(Odd >= 0 && M.ActOf(Odd) && !M.ActOf(Odd)->Lines.empty() && M.Things[Odd].Plot == "P-ODD",
		"an unknown kind and act still does something; the plot comes from the id");
	CHECK(Leaflets >= 0 && M.ActOf(Leaflets)->Daily == 1 && M.ActOf(Leaflets)->Give.size() == 1 && M.ActOf(Leaflets)->Give[0].first == "leaflet",
		"a take gives its item once a day");
	CHECK(M.FindAct("_seat") >= 0 && M.FindAct("_seat") == M.Things[Chair].Act, "default acts are shared per kind");

	// days and hours
	CHECK(ParseDays("tue-sun") == 0x7E && ParseDays("") == 0x7F && ParseDays("mon,wed,fri") == 0x15 && ParseDays("fri-mon") == 0x71,
		"weekday lists: ranges, lists, wrapping");
	CHECK(InHours(19.9f, 10.f, 20.f) && !InHours(20.f, 10.f, 20.f) && InHours(1.f, 18.f, 2.f) && !InHours(3.f, 18.f, 2.f) && InHours(5.f, 0.f, 24.f),
		"hours: [open, close), past midnight, all day");

	FAsk Ask;
	Ask.Money = 100;
	std::set<std::string> Staff = { "P-LIBRARY/librarian" };
	std::map<std::string, int> Pocket;
	Ask.ItemCount = [&Pocket](const std::string& Item) { const auto It = Pocket.find(Item); return It == Pocket.end() ? 0 : It->second; };
	Ask.StaffOnShift = [&Staff](const std::string& Plot, const std::string& Role) { return Staff.count(Plot + "/" + Role) > 0; };

	const int Shelf = M.FindThing("P-LIBRARY/shelf_birds");
	CHECK(M.Check(Shelf, At(0, 12.f), Ask) == EVerdict::Closed, "the library is shut on Mondays");
	CHECK(M.Check(Shelf, At(2, 9.f), Ask) == EVerdict::Closed && M.Check(Shelf, At(2, 12.f), Ask) == EVerdict::Ok, "and before 10, open at noon");
	CHECK(M.Check(Chair, At(0, 3.f), Ask) == EVerdict::Ok, "a seat whatever the hour");

	// lines cycle, the first use of a plot today is a visit
	FOutcome O1 = M.Run(Shelf, At(2, 12.f), Ask);
	FOutcome O2 = M.Run(Shelf, At(2, 12.5f), Ask);
	FOutcome O3 = M.Run(Shelf, At(2, 13.f), Ask);
	FOutcome O4 = M.Run(Shelf, At(2, 13.5f), Ask);
	CHECK(O1.Line == "city.read.birds.1" && O2.Line == "city.read.birds.2" && O3.Line == "city.read.birds.3" && O4.Line == "city.read.birds.1",
		"a shelf's pages in order, wrapping round");
	CHECK(O1.Minutes == 20 && O1.Event == "read" && O1.Target == "birds" && O1.Sound == "page_turn" && O1.NumLines == 3,
		"what reading yields: 20 min, read/birds, page_turn");
	CHECK(O1.bFirstVisitToday && !O2.bFirstVisitToday && O1.Plot == "P-LIBRARY", "the first use in a plot today is a visit, the next is not");
	FOutcome Sit = M.Run(Chair, At(2, 14.f), Ask);
	CHECK(Sit.Verdict == EVerdict::Ok && !Sit.bFirstVisitToday && Sit.Event == "sit" && Sit.Target == "seat", "sitting: no second visit");
	FOutcome Tomorrow = M.Run(Chair, At(3, 11.f, 101), Ask);
	CHECK(Tomorrow.bFirstVisitToday, "a new day: a visit again");
	CHECK(M.NextLine(Shelf, At(3, 12.f, 101)) == "city.read.birds.2" && M.NextLine(Shelf, At(3, 12.f, 101)) == "city.read.birds.3", "the page panel's next turns on");

	// a shop needs the librarian
	const int Desk = M.FindThing("P-LIBRARY/desk");
	FOutcome Shop = M.Run(Desk, At(3, 12.f, 101), Ask);
	CHECK(Shop.Verdict == EVerdict::Ok && Shop.Shop == "library" && Shop.Title == "city.shop.library" && Shop.Event == "do" && Shop.Target == "library_desk",
		"the desk opens the library's shop (default event do, target = act id)");
	Staff.clear();
	FOutcome NoStaff = M.Run(Desk, At(3, 12.f, 101), Ask);
	CHECK(NoStaff.Verdict == EVerdict::NoStaff && RefusalKey(NoStaff.Verdict) == "city.act.no_staff", "nobody at the desk: no_staff");

	// money, items, daily limits
	Staff.insert("P-CINEMA/cashier");
	const int Ticket = M.FindThing("P-CINEMA/ticket");
	const int Film = M.FindThing("P-CINEMA/screen");
	CHECK(M.Check(Film, At(4, 20.f, 102), Ask) == EVerdict::NeedsItem, "no ticket, no film");
	Ask.Money = 20;
	CHECK(M.Check(Ticket, At(4, 20.f, 102), Ask) == EVerdict::NoMoney, "20 roubles don't buy a 30-rouble ticket");
	Ask.Money = 100;
	CHECK(M.Check(Ticket, At(4, 12.f, 102), Ask) == EVerdict::Closed && M.Check(Ticket, At(4, 1.f, 102), Ask) == EVerdict::Ok,
		"the cinema's till: open from 18 to 2 at night");
	FOutcome T1 = M.Run(Ticket, At(4, 20.f, 102), Ask);
	CHECK(T1.Verdict == EVerdict::Ok && T1.Price == 30 && T1.Give.size() == 1 && T1.Give[0].first == "cinema_ticket" && T1.Give[0].second == 1,
		"a ticket: 30 roubles, one cinema_ticket");
	M.Run(Ticket, At(4, 20.f, 102), Ask);
	CHECK(M.Check(Ticket, At(4, 21.f, 102), Ask) == EVerdict::DailyDone && M.TodayCount("cinema_ticket", 102) == 2, "two a day, then daily_done");
	CHECK(M.Check(Ticket, At(5, 20.f, 103), Ask) == EVerdict::Ok, "the next day it sells again");
	Pocket["cinema_ticket"] = 1;
	FOutcome F = M.Run(Film, At(4, 21.f, 102), Ask);
	CHECK(F.Verdict == EVerdict::Ok && F.bFade && F.Minutes == 90 && F.Screen == "film_stills", "with a ticket the film runs: 90 min, a fade, the stills");
	M.ResetToday();
	CHECK(M.Check(Ticket, At(4, 21.f, 102), Ask) == EVerdict::Ok && M.TotalUses("cinema_ticket") == 2, "reset today: sells again, the uses stay");

	// save and load
	std::map<std::string, std::string> Saved;
	M.SaveState(Saved);
	FModel M2;
	M2.Load(Text, Error);
	M2.LoadState(Saved);
	CHECK(M2.LineIndex("P-LIBRARY/shelf_birds") == M.LineIndex("P-LIBRARY/shelf_birds") && M2.TotalUses("read_birds") == 4
		&& M2.TotalUses("cinema_ticket") == 2, "the state survives a save and a load (pages, uses)");
	const std::string Expected = "city.read.birds." + std::to_string(M.LineIndex("P-LIBRARY/shelf_birds") + 1);
	FOutcome Again = M2.Run(Shelf, At(3, 12.f, 101), Ask);
	CHECK(Again.Line == Expected && !Again.bFirstVisitToday, "after the load the shelf goes on where it was (%s), the visit remembered", Expected.c_str());
	M2.Run(Ticket, At(4, 20.f, 102), Ask);
	M2.Run(Ticket, At(4, 20.f, 102), Ask);
	std::map<std::string, std::string> Saved2;
	M2.SaveState(Saved2);
	FModel M3;
	M3.Load(Text, Error);
	M3.LoadState(Saved2);
	CHECK(M3.Check(Ticket, At(4, 20.f, 102), Ask) == EVerdict::DailyDone, "today's counts are saved too");
	M3.ResetState();
	CHECK(M3.TotalUses("read_birds") == 0 && M3.LineIndex("P-LIBRARY/shelf_birds") == 0, "a new game forgets it all");

	// picking
	int Out[4];
	const FVec Eye = V(1000, 1800, 160);
	int N = M.Pick(Eye, Unit(V(0, 1, 0)), 260, Out, 4);
	CHECK(N >= 1 && Out[0] == Shelf, "looking at the shelf from 2 m picks the shelf (%d found)", N);
	N = M.Pick(Eye, Unit(V(0, -1, 0)), 260, Out, 4);
	CHECK(N == 0, "turned away: nothing");
	N = M.Pick(V(1000, 1800, 160), Unit(V(0, 1, 0)), 150, Out, 4);
	CHECK(N == 0, "out of reach: nothing");
	N = M.Pick(V(1200, 1850, 160), Unit(V(0.0, 1.0, -0.65)), 260, Out, 4);
	CHECK(N >= 1 && Out[0] == Chair, "looking down at the chair picks it over the shelf beside it");
	N = M.Pick(V(1000, 2000, 900), Unit(V(0, 0, -1)), 1000, Out, 4);
	CHECK(N == 0, "a thing on another floor (2 m+ below) is not picked");
	FVec Mn, Mx;
	CHECK(M.Bounds(Mn, Mx) && M.InBounds(V(1000, 2000, 150), 0) && !M.InBounds(V(50000, 0, 0), 3000), "the town box");
	const auto Near = M.Nearest(V(1000, 2000, 150), 5);
	CHECK(Near.size() == 5 && Near[0].first == Shelf && Near[0].second < 1.0 && Near[1].second <= Near[2].second, "the nearest five, sorted");
}

static void UntilChecks()
{
	std::printf("until (sleep till an hour)\n");
	const std::string Text = R"({"things": [
  {"id": "P-H/bed", "plot": "P-H", "kind": "bed", "ue": [0, 0, 50], "yaw": 0, "props": {}, "act": "sleep_night", "hours": [0, 24], "days": "mon-sun"},
  {"id": "P-H/cot", "plot": "P-H", "kind": "bed", "ue": [500, 0, 50], "yaw": 0, "props": {}, "act": "nap", "hours": [0, 24], "days": "mon-sun"}],
 "acts": {"sleep_night": {"prompt": "p", "minutes": 480, "until": 7, "pose": "lie", "fade": true, "any_hour": true},
          "nap": {"prompt": "p", "minutes": 60, "until": -1, "pose": "lie", "fade": true, "any_hour": true}},
 "shops": {}})";
	FModel M;
	std::string Error;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	FAsk Ask;
	const int Bed = M.FindThing("P-H/bed"), Cot = M.FindThing("P-H/cot");
	CHECK(Bed >= 0 && Cot >= 0 && M.Acts[M.Things[Bed].Act].Until == 7 && M.Acts[M.Things[Cot].Act].Until == -1, "until read, -1 kept");
	CHECK(M.Run(Bed, At(2, 23.f), Ask).Minutes == 480 && M.Run(Bed, At(3, 1.5f), Ask).Minutes == 330 && M.Run(Bed, At(3, 6.5f), Ask).Minutes == 30,
		"to bed at 23:00 sleeps 480 min, at 1:30 330, at 6:30 30");
	CHECK(M.Run(Bed, At(3, 14.f), Ask).Minutes == 17 * 60 && M.Run(Cot, At(3, 14.f), Ask).Minutes == 60, "at 14:00 till 7 tomorrow; without until the minutes");
}


static FWhen AtDate(int Weekday, float Hour, long long Day, int MonthDay, long long Since)
{
	FWhen W = At(Weekday, Hour, Day);
	W.MonthDay = MonthDay;
	W.Since = Since;
	return W;
}

static void WhenChecks()
{
	std::printf("lines by the day and the hour, rotation, seats, screens, the plot round the hero\n");
	const std::string Text = R"({"things": [
  {"id": "P-A/scope", "plot": "P-A", "kind": "machine", "ue": [0, 0, 150], "yaw": 0, "props": {"what": "telescope"}, "act": "scope"},
  {"id": "P-A/board", "plot": "P-A", "kind": "board", "ue": [400, 0, 150], "yaw": 0, "props": {}, "act": "board"},
  {"id": "P-A/board2", "plot": "P-A", "kind": "board", "ue": [800, 0, 150], "yaw": 0, "props": {}, "act": "board"},
  {"id": "P-A/calendar", "plot": "P-A", "kind": "board", "ue": [1200, 0, 150], "yaw": 0, "props": {}, "act": "calendar"},
  {"id": "P-A/sofa", "plot": "P-A", "kind": "seat", "ue": [2000, 0, 45], "yaw": 0, "props": {"height": 32.85}},
  {"id": "P-A/pod", "plot": "P-A", "kind": "seat", "ue": [2000, 300, 10], "yaw": 0, "props": {"height": 0.07}},
  {"id": "P-A/tv", "plot": "P-A", "kind": "screen", "ue": [2500, 0, 120], "yaw": 180, "props": {"what": "tv"}, "act": "tv"},
  {"id": "P-A/hall_seat", "plot": "P-A", "kind": "screen", "ue": [2000, -600, 45], "yaw": 0, "props": {"height": 0.45, "what": "tv"}, "act": "tv"},
  {"id": "P-B/tv", "plot": "P-B", "kind": "screen", "ue": [9000, 0, 120], "yaw": 180, "props": {"what": "tv"}, "act": "tv"},
  {"id": "P-B/bed", "plot": "P-B", "kind": "bed", "ue": [9000, 500, 50], "yaw": 0, "props": {"height": 9.7}}],
 "acts": {
  "scope": {"prompt": "p", "lines": [{"k": "day.1", "h": [6, 21]}, {"k": "day.2", "h": [6, 21]}, {"k": "night.1", "h": [21, 6]}, "any.1"]},
  "board": {"prompt": "p", "rotate": "daily", "lines": ["n.1", "n.2", "n.3", {"k": "n.weekend", "d": "sat-sun"}]},
  "calendar": {"prompt": "p", "lines": [{"k": "c.newyear", "date": ["12-30", "01-02"]}, {"k": "c.may", "date": "05-09"},
               {"k": "c.later", "after": 10}, {"k": "c.even", "every": [2, 0]}, {"k": "c.odd", "every": [2, 1]}, {"k": "c.bad", "date": ["13-01", "x"]}]},
  "tv": {"prompt": "p", "lines": ["tv.1"], "minutes": 20, "pose": "sit", "screen": "tv"}},
 "shops": {}})";
	FModel M;
	std::string Error;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	FAsk Ask;
	const int Scope = M.FindThing("P-A/scope"), Board = M.FindThing("P-A/board"), Board2 = M.FindThing("P-A/board2");
	const int Cal = M.FindThing("P-A/calendar");
	CHECK(M.ActOf(Scope)->Lines.size() == 4 && M.ActOf(Scope)->Lines[0].HourFrom == 6.f && M.ActOf(Scope)->Lines[3].HourFrom < 0.f,
		"lines with conditions and plain keys read side by side");
	// day and night
	const FOutcome D1 = M.Run(Scope, At(2, 12.f), Ask);
	const FOutcome D2 = M.Run(Scope, At(2, 12.f), Ask);
	const FOutcome D3 = M.Run(Scope, At(2, 12.f), Ask);
	CHECK(D1.Line == "day.1" && D2.Line == "day.2" && D3.Line == "any.1" && D1.NumLines == 3, "by day: the day lines and the plain one, in turn (%s %s %s)",
		D1.Line.c_str(), D2.Line.c_str(), D3.Line.c_str());
	const FOutcome N1 = M.Run(Scope, At(2, 23.f), Ask);
	const FOutcome N2 = M.Run(Scope, At(2, 2.f), Ask);
	CHECK(N1.Line == "night.1" && N2.Line == "any.1" && N1.NumLines == 2, "at night: only the night line and the plain one (%s %s)", N1.Line.c_str(), N2.Line.c_str());
	CHECK(M.NextLine(Scope, At(2, 23.5f)) == "night.1", "the page's next keeps to the night lines");
	// daily boards: one a day, the next on the next day it is read, the same all day long
	const FOutcome B1 = M.Run(Board, At(2, 10.f, 200), Ask);
	const FOutcome B1b = M.Run(Board, At(2, 18.f, 200), Ask);
	CHECK(B1.Line == "n.1" && B1b.Line == "n.1" && B1.NumLines == 1, "a daily board: n.1 all day, one line a time");
	CHECK(M.NextLine(Board, At(2, 18.f, 200)) == "n.1", "the page's next on a daily board stays on today's");
	const FOutcome B2 = M.Run(Board, At(3, 10.f, 201), Ask);
	const FOutcome B3 = M.Run(Board, At(1, 10.f, 207), Ask);
	CHECK(B2.Line == "n.2" && B3.Line == "n.3", "the next day the next notice, a week later the one after (not skipped): %s %s", B2.Line.c_str(), B3.Line.c_str());
	const FOutcome B4 = M.Run(Board, At(5, 10.f, 208), Ask);
	const FOutcome B5 = M.Run(Board, At(6, 10.f, 209), Ask);
	CHECK(B4.Line == "n.weekend" && B5.Line == "n.1", "the weekend notice on a Saturday, then round again (%s %s)", B4.Line.c_str(), B5.Line.c_str());
	CHECK(M.Run(Board2, At(6, 10.f, 209), Ask).Line == "n.1", "each board has its own page");
	// calendar dates, days since the game began
	auto CalLines = [&M, Cal](const FWhen& W)
	{
		std::set<std::string> Seen;
		for (int K = 0; K < 8; ++K)
		{
			Seen.insert(M.NextLine(Cal, W));
		}
		std::string All;
		for (const std::string& L : Seen)
		{
			All += L + " ";
		}
		return All;
	};
	// c.bad has a date that is not one: no condition, always there
	const std::string C1 = CalLines(AtDate(0, 12.f, 300, 1231, 3)), C2 = CalLines(AtDate(0, 12.f, 301, 102, 12)), C3 = CalLines(AtDate(0, 12.f, 302, 509, 4));
	CHECK(C1 == "c.bad c.newyear c.odd ", "31 December, day 3: the New Year line and the odd day (%s)", C1.c_str());
	CHECK(C2 == "c.bad c.even c.later c.newyear ", "2 January, day 12: the range wraps the year, 'after 10' holds, an even day (%s)", C2.c_str());
	CHECK(C3 == "c.bad c.even c.may ", "9 May, day 4: the one-day date (%s)", C3.c_str());
	CHECK(M.ActOf(Cal)->Lines[5].DateFrom == -1 && ParseMonthDay("12-31") == 1231 && ParseMonthDay("13-01") == -1 && ParseMonthDay("x") == -1,
		"dates: MM-DD parsed, a bad one is no condition");
	// state of the daily boards survives a save
	std::map<std::string, std::string> Saved;
	M.SaveState(Saved);
	FModel M2;
	M2.Load(Text, Error);
	M2.LoadState(Saved);
	CHECK(M2.Run(Board, At(6, 20.f, 209), Ask).Line == "n.1" && M2.Run(Board, At(0, 9.f, 210), Ask).Line == "n.2",
		"after a load the board keeps today's notice, the next day the next");
	// seats: an absurd height is not believed
	const int Sofa = M.FindThing("P-A/sofa"), Pod = M.FindThing("P-A/pod"), Bed = M.FindThing("P-B/bed");
	CHECK(FModel::SeatHeight(M.Things[Sofa], false) == 0.45f && FModel::SeatHeight(M.Things[Pod], false) == 0.07f
		&& FModel::SeatHeight(M.Things[Bed], true) == 0.5f, "seat heights: 32.85 m -> 0.45, a floor pod keeps 0.07, a 9.7 m bed -> 0.5");
	// screens
	const int Tv = M.FindThing("P-A/tv"), Hall = M.FindThing("P-A/hall_seat"), TvB = M.FindThing("P-B/tv");
	CHECK(M.ScreenFrom(Sofa, 2500.0) == Tv, "from the sofa (facing +x) the TV in front is the screen");
	CHECK(M.ScreenFrom(Pod, 2500.0) == Tv && M.ScreenFrom(Sofa, 300.0) == -1, "within reach only");
	CHECK(M.ScreenFrom(Bed, 2500.0) == -1, "another plot's TV is not watched from here");
	CHECK(M.NeedsSeatFirst(Tv, 2500.0) && !M.NeedsSeatFirst(Hall, 2500.0) && !M.NeedsSeatFirst(TvB, 2500.0),
		"a TV with a sofa near: sit first; a hall seat is the seat; a TV with no seat near plays standing");
	// the plot round the hero
	CHECK(M.NearestThing(V(380, 50, 150), 400.0, 250.0) == Board && M.NearestThing(V(380, 50, 900), 400.0, 250.0) == -1
		&& M.NearestThing(V(5000, 5000, 150), 400.0, 250.0) == -1, "the nearest thing round the hero: on his floor, within reach");
	// developer mode: open all hours
	FModel M3;
	std::string Text3;
	ReadFile("tools/citysim/fixture.json", Text3);
	M3.Load(Text3, Error);
	FAsk Open;
	Open.Money = 100;
	Open.bOpenAll = true;
	const int Shelf = M3.FindThing("P-LIBRARY/shelf_birds"), Desk = M3.FindThing("P-LIBRARY/desk");
	CHECK(M3.Check(Shelf, At(0, 3.f), Open) == EVerdict::Ok && M3.Check(Desk, At(0, 3.f), Open) == EVerdict::Ok
		&& M3.Check(Shelf, At(0, 3.f), Ask) == EVerdict::Closed, "open all hours: no hours, no days, no staff asked");
	// subtitles
	const auto S1 = Subtitles("The Man Who Waited for the Lighthouse. He waited forty years, and all that time the lighthouse waited for him. The whole hall cried at the end. Me too.", 5);
	CHECK(S1.size() == 3 && S1[0] == "The Man Who Waited for the Lighthouse." && S1[2] == "The whole hall cried at the end. Me too.",
		"subtitles: by sentences, a short one joined (%d: %s | %s)", (int)S1.size(), S1.empty() ? "" : S1[0].c_str(), S1.size() > 2 ? S1[2].c_str() : "");
	const auto S2 = Subtitles(u8"«Лекция: Почему мы любим солнце?» Вход свободный. Вопросы из зала приветствуются; ответы не гарантированы.", 5);
	CHECK(S2.size() == 3 && S2[0] == u8"«Лекция: Почему мы любим солнце?»", "Russian with closing guillemets (%d: %s)", (int)S2.size(), S2.empty() ? "" : S2[0].c_str());
	const auto S3 = Subtitles(u8"灯台を待った男。四十年待った。その間、灯台も彼を待っていた。最後は会場中が泣いた。僕も。", 5);
	CHECK(S3.size() == 4 && S3[0] == u8"灯台を待った男。" && S3[3] == u8"最後は会場中が泣いた。僕も。", "Japanese: 。 ends a subtitle, short ones joined with no space (%d: %s)",
		(int)S3.size(), S3.empty() ? "" : S3[0].c_str());
	const auto S4 = Subtitles("One. Two two two. Three three three. Four four four. Five five five. Six six six. Seven seven seven.", 3);
	const auto S5 = Subtitles("No full stop at all", 5);
	CHECK(S4.size() == 3 && S5.size() == 1 && S5[0] == "No full stop at all" && Subtitles("", 5).empty(), "at most MaxParts; one sentence stays whole; nothing is nothing");
}

static void MoreChecks()
{
	std::printf("festivals, weather, collections, the closed state, the townsfolk's seats\n");
	const std::string Text = R"({"things": [
  {"id": "P-C/board", "plot": "P-C", "kind": "board", "ue": [0, 0, 150], "yaw": 0, "props": {}, "act": "club_board"},
  {"id": "P-C/talk", "plot": "P-C", "kind": "counter", "ue": [400, 0, 110], "yaw": 0, "props": {}, "act": "talk"},
  {"id": "P-C/jukebox", "plot": "P-C", "kind": "machine", "ue": [800, 0, 110], "yaw": 0, "props": {}, "act": "jukebox",
   "hours": [10, 22], "days": "mon-sun"},
  {"id": "P-C/chair", "plot": "P-C", "kind": "seat", "ue": [1200, 0, 45], "yaw": 0, "props": {"height": 0.45}, "act": "sit",
   "hours": [10, 22], "days": "mon-sun", "closed": "_closed:chairs"},
  {"id": "P-C/chair2", "plot": "P-C", "kind": "seat", "ue": [1300, 0, 45], "yaw": 0, "props": {"height": 0.45}, "act": "sit",
   "hours": [10, 22], "days": "mon-sun"},
  {"id": "P-C/upstairs_chair", "plot": "P-C", "kind": "seat", "ue": [1250, 0, 445], "yaw": 0, "props": {}, "act": "sit"},
  {"id": "P-C/shelf", "plot": "P-C", "kind": "shelf", "ue": [1600, 0, 130], "yaw": 0, "props": {}, "act": "read_birds",
   "hours": [10, 22], "days": "mon-sun", "closed": "no_such_act"},
  {"id": "P-C/news", "plot": "P-C", "kind": "board", "ue": [2800, 0, 150], "yaw": 0, "props": {}, "act": "notices"},
  {"id": "P-C/news2", "plot": "P-C", "kind": "board", "ue": [3200, 0, 150], "yaw": 0, "props": {}, "act": "notices"},
  {"id": "P-C/film", "plot": "P-C", "kind": "screen", "ue": [2400, 0, 300], "yaw": 0, "props": {}, "act": "film"},
  {"id": "P-K1/counter", "plot": "P-K1", "kind": "counter", "ue": [9000, 0, 110], "yaw": 0, "props": {}, "act": "talk", "collect": "kiosks/P-K1"},
  {"id": "P-K2/counter", "plot": "P-K2", "kind": "counter", "ue": [9500, 0, 110], "yaw": 0, "props": {}, "act": "talk", "collect": "kiosks/P-K2"}],
 "acts": {
  "club_board": {"prompt": "p", "rotate": "daily", "lines": ["n.1", "n.2", "n.3", {"k": "fest.any", "festival": "any"},
                 {"k": "fest.ny", "festival": "new_year"}]},
  "talk": {"prompt": "p", "lines": ["t.1", {"k": "t.rain", "weather": "rain"}, {"k": "t.coldsnow", "weather": ["cold", "snow"]},
           {"k": "t.bad", "weather": "hail_of_frogs"}, {"k": "t.calm", "festival": "none"},
           {"k": "t.fan", "count": ["records", 2]}, {"k": "t.song2", "have": "records/2"}]},
  "jukebox": {"prompt": "p", "lines": [{"k": "j.1", "collect": "records/1"}, {"k": "j.2", "collect": "records/2"}, {"k": "j.3", "collect": "records/3"}]},
  "sit": {"prompt": "p", "pose": "sit", "any_hour": true},
  "_closed:chairs": {"prompt": "look", "lines": ["closed.chairs"], "any_hour": true, "event": "look", "target": "closed"},
  "read_birds": {"prompt": "p", "lines": ["b.1"], "minutes": 10, "collect": "shelves/birds"},
  "film": {"prompt": "p", "lines": ["f.a", {"k": "f.b", "h": [19, 23]}], "collect": "films/{line}"},
  "notices": {"prompt": "p", "rotate": "daily", "lines": [{"k": "w.1", "before": 3}, {"k": "w.2", "after": 3, "before": 6}, {"k": "w.3", "after": 6},
              {"k": "news.once", "once": true}, "plain"]}},
 "collections": {"records": {"title": "c.records", "pieces": ["1", "2", "3"]},
                 "shelves": {"title": "c.shelves", "pieces": ["birds", "sea"]},
                 "kiosks": {"title": "c.kiosks", "pieces": ["P-K1", "P-K2"]},
                 "films": {"title": "c.films", "pieces": ["f.a", "f.b"]}},
 "shops": {}})";
	FModel M;
	std::string Error;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	FAsk Ask;
	Ask.Money = 100;
	const int Board = M.FindThing("P-C/board"), Talk = M.FindThing("P-C/talk"), Juke = M.FindThing("P-C/jukebox");
	const int Chair = M.FindThing("P-C/chair"), Chair2 = M.FindThing("P-C/chair2"), Up = M.FindThing("P-C/upstairs_chair");
	const int Shelf = M.FindThing("P-C/shelf"), K1 = M.FindThing("P-K1/counter"), K2 = M.FindThing("P-K2/counter");
	auto Seen = [&M](int Thing, const FWhen& W)
	{
		std::set<std::string> Keys;
		for (int K = 0; K < 10; ++K)
		{
			Keys.insert(M.NextLine(Thing, W));
		}
		std::string All;
		for (const std::string& L : Keys)
		{
			All += L + " ";
		}
		return All;
	};
	// festivals: a daily board's notice takes the holiday, the run goes on after it
	FWhen Plain = At(2, 12.f, 400);
	FWhen Fest = At(3, 12.f, 401);
	Fest.bFestival = true;
	Fest.Festivals = { "new_year" };
	const FOutcome B1 = M.Run(Board, Plain, Ask);
	const FOutcome B2 = M.Run(Board, Fest, Ask);
	const FOutcome B2b = M.Run(Board, Fest, Ask);
	FWhen Next = At(4, 12.f, 402);
	const FOutcome B3 = M.Run(Board, Next, Ask);
	CHECK(B1.Line == "n.1" && B2.Line == "fest.any" && B2b.Line == "fest.any" && B3.Line == "n.2",
		"a daily board: n.1, on the holiday its notice all day, then n.2 (nothing skipped): %s %s %s", B1.Line.c_str(), B2.Line.c_str(), B3.Line.c_str());
	FWhen Market = At(5, 12.f, 403);
	Market.Festivals = { "weekend_market" };
	CHECK(Seen(Talk, Plain).find("t.calm") != std::string::npos && Seen(Talk, Fest).find("t.calm") == std::string::npos,
		"\"festival\": \"none\" holds only on a day with no holiday");
	CHECK(M.FindAct("club_board") >= 0 && M.Acts[(size_t)M.FindAct("club_board")].Lines[4].Holds(Fest) && !M.Acts[(size_t)M.FindAct("club_board")].Lines[4].Holds(Market)
		&& !M.Acts[(size_t)M.FindAct("club_board")].Lines[3].Holds(Market), "a festival id holds on its day; the market is no holiday for \"any\"");
	// weather
	CHECK(WeatherBits(0.9f, 2.f, 0.f, 0.f, 12.f, 0.8f) == (WeatherRain | WeatherWet) && WeatherBits(0.1f, 0.f, 0.f, 0.f, 25.f, 0.f) == (WeatherWarm | WeatherClear | WeatherDry)
		&& WeatherBits(1.f, 1.f, 1.f, 0.6f, -3.f, 0.f) == (WeatherSnow | WeatherFog | WeatherCold) && WeatherBit("fog") == WeatherFog && WeatherBit("frogs") == 0,
		"weather words from the sky's summary");
	FWhen Rain = Plain;
	Rain.Weather = WeatherRain | WeatherWet;
	FWhen Snow = Plain;
	Snow.Weather = WeatherSnow | WeatherCold;
	const std::string SRain = Seen(Talk, Rain), SDry = Seen(Talk, Plain), SSnow = Seen(Talk, Snow);
	CHECK(SRain.find("t.rain") != std::string::npos && SDry.find("t.rain") == std::string::npos && SSnow.find("t.coldsnow") != std::string::npos
		&& SRain.find("t.coldsnow") == std::string::npos, "weather lines: rain only in the rain, [cold, snow] any of them (%s)", SRain.c_str());
	CHECK(SRain.find("t.bad") == std::string::npos && SSnow.find("t.bad") == std::string::npos, "a line with an unknown weather word never shows");
	// collections: a line's stamp, an act's stamp, a thing's stamp; once each
	CHECK(M.Collections.size() == 4 && M.FindCollection("records") && M.FindCollection("records")->Pieces.size() == 3, "collections read");
	CHECK(SDry.find("t.fan") == std::string::npos && SDry.find("t.song2") == std::string::npos, "no records heard: no fan's lines");
	const FOutcome J1 = M.Run(Juke, Plain, Ask);
	const FOutcome J2 = M.Run(Juke, Plain, Ask);
	CHECK(J1.NewStamps.size() == 1 && J1.NewStamps[0] == "records/1" && J2.NewStamps.size() == 1 && J2.NewStamps[0] == "records/2"
		&& M.CountStamps("records") == 2, "the jukebox's records: a stamp per record heard (2 of 3)");
	M.Run(Juke, Plain, Ask);
	const FOutcome J4 = M.Run(Juke, Plain, Ask);
	CHECK(J4.NewStamps.empty() && M.CountStamps("records") == 3, "a record heard again gives nothing new");
	CHECK(Seen(Talk, Plain).find("t.fan t.song2") != std::string::npos, "\"count\" and \"have\" lines show once the stamps are there");
	const FOutcome R1 = M.Run(Shelf, Plain, Ask);
	const FOutcome R2 = M.Run(Shelf, Plain, Ask);
	CHECK(R1.NewStamps.size() == 1 && R1.NewStamps[0] == "shelves/birds" && R2.NewStamps.empty() && M.CountStamps("shelves") == 1, "an act's stamp, once");
	const FOutcome Kv = M.Run(K1, Plain, Ask);
	CHECK(Kv.NewStamps.size() == 1 && Kv.NewStamps[0] == "kiosks/P-K1" && M.HasStamp("kiosks/P-K1") && !M.HasStamp("kiosks/P-K2"), "a thing's stamp: kiosks visited");
	const int Film = M.FindThing("P-C/film");
	const FOutcome F1 = M.Run(Film, Plain, Ask);
	const FOutcome F2 = M.Run(Film, Plain, Ask);
	const FOutcome F3 = M.Run(Film, At(2, 20.f, 400), Ask);
	CHECK(F1.NewStamps.size() == 1 && F1.NewStamps[0] == "films/f.a" && F2.NewStamps.empty() && F3.NewStamps.size() == 1 && F3.NewStamps[0] == "films/f.b"
		&& M.CountStamps("films") == 2, "\"films/{line}\": every film shown is a piece (the evening one in the evening)");
	CHECK(!M.GiveStamp("records/9") && !M.GiveStamp("nope/1") && !M.GiveStamp("records"), "pieces the data does not declare are not given");
	std::map<std::string, std::string> Saved;
	M.SaveState(Saved);
	FModel M2;
	M2.Load(Text, Error);
	M2.LoadState(Saved);
	CHECK(Saved.count("c:records/2") && M2.CountStamps("records") == 3 && M2.CountStamps("kiosks") == 1, "stamps survive a save and a load");
	M2.GrantSet("kiosks");
	CHECK(M2.CountStamps("kiosks") == 2 && M2.Run(K2, Plain, Ask).NewStamps.empty(), "developer mode: a set granted");
	M2.ClearSet("records");
	CHECK(M2.CountStamps("records") == 0 && M2.CountStamps("kiosks") == 2, "developer mode: a set cleared");
	M2.ClearSet("*");
	M2.GrantSet("*");
	CHECK(M2.CountStamps("records") == 3 && M2.CountStamps("shelves") == 2 && M2.CountStamps("films") == 2 && !M2.GrantSet("nope"), "every set at once; an unknown set is refused");
	M2.ResetState();
	CHECK(M2.CountStamps("kiosks") == 0, "a new game forgets the stamps");
	// before / once: dated notices replace each other; a once line shows once at each thing, also after a load
	auto Day = [](long long Since) { FWhen W = At((int)(Since % 7), 12.f, 500 + Since); W.Since = Since; return W; };
	const int News = M.FindThing("P-C/news"), News2 = M.FindThing("P-C/news2");
	std::string Got;
	for (long long D = 0; D < 9; ++D)
	{
		Got += M.Run(News, Day(D), Ask).Line + " ";
	}
	CHECK(Got == "w.1 news.once plain w.2 plain w.2 w.3 plain w.3 ", "before/after: w.1 till day 3, w.2 days 3..5, w.3 from 6; the once line once (%s)", Got.c_str());
	CHECK(M.Run(News2, Day(1), Ask).Line == "w.1" && M.Run(News2, Day(2), Ask).Line == "news.once", "once is per thing: the other board shows it too");
	std::map<std::string, std::string> SavedOnce;
	M.SaveState(SavedOnce);
	FModel M4;
	M4.Load(Text, Error);
	M4.LoadState(SavedOnce);
	std::set<std::string> After4;
	for (long long D = 7; D < 13; ++D)
	{
		After4.insert(M4.Run(News2, Day(D), Ask).Line);
	}
	CHECK(SavedOnce.count("o:P-C/news2|news.once") && After4.count("news.once") == 0 && After4.count("w.3"), "a once line shown stays shown after a load");
	// the closed state: the chairs on the tables, by data, instead of nothing
	FWhen Night = At(2, 23.f, 404);
	CHECK(M.ActIndexNow(Chair, Night, false) == M.FindAct("_closed:chairs") && M.ActIndexNow(Chair, Plain, false) == M.Things[(size_t)Chair].Act
		&& M.ActIndexNow(Chair, Night, true) == M.Things[(size_t)Chair].Act, "a chair with a closed act: that act at night, its own by day and with open-all");
	const FOutcome C1 = M.Run(Chair, Night, Ask);
	CHECK(C1.Verdict == EVerdict::Ok && C1.bClosed && C1.Line == "closed.chairs" && C1.Pose == "stand" && !C1.bFirstVisitToday,
		"at night the chair says the chairs are on the tables (no sitting, no visit)");
	CHECK(M.Run(Chair2, Night, Ask).Pose == "sit", "a chair without one still sits at any hour");
	CHECK(M.Things[(size_t)Shelf].ClosedAct == -1 && M.Check(Shelf, Night, Ask) == EVerdict::Closed, "an unknown closed act is none: the shelf is just refused");
	CHECK(M.Run(Chair, Plain, Ask).Pose == "sit" && M.LineIndex("P-C/chair#closed") == 0, "by day it sits again; the closed act keeps its own page");
	// the townsfolk: the nearest free seat, claims, the hero refused a taken seat
	const FVec Near = V(1150, 0, 0);
	CHECK(M.FindFree("seat", Near, 500.0, 200.0) == Chair && M.FindFree("seat", V(1150, 0, 400), 500.0, 200.0) == Up, "the nearest free seat on the floor asked");
	CHECK(M.FindFree("seat", Near, 30.0, 200.0) == -1 && M.FindFree("", V(0, 0, 100), 100.0, 200.0) == Board, "within the radius only; any kind");
	CHECK(M.Claim(Chair, 7) && M.Claim(Chair, 7) && !M.Claim(Chair, 8) && M.Holder(Chair) == 7, "a seat claimed: again by the same one, not by another");
	CHECK(M.FindFree("seat", Near, 500.0, 200.0) == Chair2 && M.FindFree("seat", Near, 500.0, 200.0, Chair2) == -1, "a held seat is not free; the hero's is skipped");
	CHECK(M.Check(Chair, Plain, Ask) == EVerdict::Taken && RefusalKey(EVerdict::Taken) == "city.act.taken" && M.Check(Talk, Plain, Ask) == EVerdict::Ok,
		"the hero can't sit on a held seat (city.act.taken)");
	M.Release(Chair, 8);
	CHECK(M.Holder(Chair) == 7, "another can't release it");
	M.Release(Chair, 7);
	CHECK(M.Holder(Chair) == 0 && M.Check(Chair, Plain, Ask) == EVerdict::Ok && !M.Claim(Chair, 0), "released: free again; holder 0 is nobody");
	M.Claim(Chair2, 9);
	M.ReleaseAll();
	CHECK(M.Holder(Chair2) == 0, "release all");
	const std::vector<int>* Of = M.ThingsOfPlot("P-C");
	CHECK(Of && Of->size() == 10 && M.ThingsOfPlot("P-K1")->size() == 1 && !M.ThingsOfPlot("P-NONE"), "the things of a plot, built at load");
}

static void RealChecks(const std::string& Path)
{
	std::string Text, Error;
	if (!ReadFile(Path, Text))
	{
		std::printf("note: %s is not built yet (python3 tools/city/interiors.py); only the fixture was checked\n", Path.c_str());
		return;
	}
	std::printf("real data (%s)\n", Path.c_str());
	FModel M;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	CHECK(!M.Things.empty(), "%d things, %d acts, %d shops", (int)M.Things.size(), (int)M.Acts.size(), (int)M.Shops.size());
	int NoAct = 0, BadShop = 0, NoPrompt = 0, NoPlot = 0;
	std::map<std::string, int> Kinds;
	for (int I = 0; I < (int)M.Things.size(); ++I)
	{
		const FAct* A = M.ActOf(I);
		NoAct += A ? 0 : 1;
		BadShop += A && !A->Shop.empty() && !M.FindShop(A->Shop) ? 1 : 0;
		NoPrompt += A && A->Prompt.empty() ? 1 : 0;
		NoPlot += M.Things[I].Plot.empty() ? 1 : 0;
		Kinds[M.Things[I].Kind]++;
	}
	CHECK(NoAct == 0, "every thing does something (%d without)", NoAct);
	CHECK(BadShop == 0, "every act's shop is in the shops table (%d missing)", BadShop);
	CHECK(NoPrompt == 0 && NoPlot == 0, "prompts and plots everywhere (%d, %d missing)", NoPrompt, NoPlot);
	std::string KindList;
	for (const auto& K : Kinds)
	{
		KindList += K.first + "=" + std::to_string(K.second) + " ";
	}
	std::printf("  kinds: %s\n", KindList.c_str());
	// every thing can be picked from 1.5 m in front of it, looking at it
	int Unpickable = 0;
	std::string First;
	for (int I = 0; I < (int)M.Things.size(); ++I)
	{
		const FThing& T = M.Things[I];
		const double R = T.Yaw * 3.14159265358979 / 180.0;
		const FVec Eye = V(T.At.X + std::cos(R) * 150.0, T.At.Y + std::sin(R) * 150.0, T.At.Z + 40.0);
		int Out[4];
		const int N = M.Pick(Eye, Unit(V(T.At.X - Eye.X, T.At.Y - Eye.Y, T.At.Z - Eye.Z)), 260.0, Out, 4);
		bool bFound = false;
		for (int K = 0; K < N; ++K)
		{
			bFound = bFound || Out[K] == I;
		}
		if (!bFound)
		{
			Unpickable++;
			if (First.empty())
			{
				First = T.Id;
			}
		}
	}
	CHECK(Unpickable == 0, "every thing is among the picks looking at it from 1.5 m in front (%d not, first %s)", Unpickable, First.c_str());
	FAsk Ask;
	Ask.Money = 100000;
	Ask.ItemCount = [](const std::string&) { return 1; };
	Ask.StaffOnShift = [](const std::string&, const std::string&) { return true; };
	int Ran = 0;
	for (int I = 0; I < (int)M.Things.size(); ++I)
	{
		Ran += M.Run(I, At(2, 12.f), Ask).Verdict == EVerdict::Ok ? 1 : 0;
	}
	CHECK(Ran > 0, "at noon on a Wednesday with everything allowed %d of %d things run", Ran, (int)M.Things.size());
	// seat heights as written (the game falls back to the usual one, but the data should not need it)
	int BadHeight = 0;
	std::string FirstBad;
	for (const FThing& T : M.Things)
	{
		const float H = T.PropF("height", 0.45f);
		if (H < MinSeatHeight || H > MaxSeatHeight)
		{
			BadHeight++;
			FirstBad = FirstBad.empty() ? T.Id : FirstBad;
		}
	}
	CHECK(BadHeight == 0, "every seat and bed height is over its own floor, %.2f..%.1f m (%d not, first %s)", MinSeatHeight, MaxSeatHeight, BadHeight, FirstBad.c_str());
	int Daily = 0, Conditional = 0, Screens = 0, EmptyNow = 0;
	for (const FAct& A : M.Acts)
	{
		Daily += A.bDaily ? 1 : 0;
		Screens += FModel::IsScreen(A) ? 1 : 0;
		for (const FLine& L : A.Lines)
		{
			Conditional += (L.HourFrom >= 0.f || L.Days != 0x7F || L.DateFrom > 0 || L.After > 0 || L.Every > 1) ? 1 : 0;
		}
		// an act with lines must have one to show at any hour of any day it may run (else the page would be empty)
		if (!A.Lines.empty())
		{
			for (int H = 0; H < 24; ++H)
			{
				if (FModel::CountHolding(A, AtDate(H % 7, (float)H + 0.5f, 100, 615, 30)) == 0)
				{
					EmptyNow++;
					break;
				}
			}
		}
	}
	std::printf("  daily acts %d, lines with conditions %d, screen acts %d\n", Daily, Conditional, Screens);
	CHECK(EmptyNow == 0, "every act with lines has a line for every hour (%d have a gap)", EmptyNow);
}

int main(int Argc, char** Argv)
{
	FixtureChecks();
	UntilChecks();
	WhenChecks();
	MoreChecks();
	RealChecks(Argc > 1 ? Argv[1] : "unreal/Lyudmilovsk/Content/City/interiors.json");
	std::printf("%d passed, %d failed\n", Passed, Failures);
	return Failures ? 1 : 0;
}
