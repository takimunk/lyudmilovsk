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

/** Sitting and lying (docs/city_interiors.md "Sitting and lying"): the floor under a seat, the look cone, the window from a bed,
 *  the sleep chooser's hours. */
static void SeatChecks()
{
	std::printf("sitting and lying\n");
	// the floor under a cushion at z 145 that is 45 cm high: 100 expected
	CHECK(FModel::SeatFloor(145.0, 45.0, false, 0.0) == 100.0, "no trace hit: the floor is the seat's height under the cushion");
	CHECK(FModel::SeatFloor(145.0, 45.0, true, 98.0) == 98.0, "a chair (no blocking box): the traced floor 2 cm lower is believed");
	CHECK(FModel::SeatFloor(145.0, 45.0, true, 141.0) == 96.0, "a bench's blocking box met near the cushion: the floor is its height below the box");
	CHECK(FModel::SeatFloor(145.0, 45.0, true, -250.0) == 100.0 && FModel::SeatFloor(145.0, 45.0, true, 30.0) == 100.0,
		"a stairwell or a hole under a gallery bench (3.5 m, 70 cm down): the data's floor, feet not through it");
	FThing Chair;
	CHECK(FModel::LookCone(Chair) == 110.f, "the look cone while seated: 110 degrees either way by default");
	Chair.Props["cone"] = "10";
	const float Narrow = FModel::LookCone(Chair);
	Chair.Props["cone"] = "400";
	CHECK(Narrow == 30.f && FModel::LookCone(Chair) == 170.f, "a thing's cone is kept to 30..170 (%.0f, %.0f)", Narrow, FModel::LookCone(Chair));
	CHECK(FModel::InCone(170.f, -170.f, 30.f) && !FModel::InCone(0.f, 180.f, 110.f) && FModel::InCone(-60.f, 40.f, 110.f),
		"in the cone across the 180 seam, not behind the back");
	CHECK(std::fabs(FModel::ClampToCone(0.f, 180.f, 110.f) - 70.f) < 0.01f && std::fabs(FModel::ClampToCone(200.f, 180.f, 110.f) - 200.f) < 0.01f,
		"a look behind the back comes to the cone's nearest edge (%.1f)", FModel::ClampToCone(0.f, 180.f, 110.f));

	const std::string Text = R"({"things": [
  {"id": "P-H/bed", "plot": "P-H", "kind": "bed", "ue": [0, 0, 50], "yaw": 0, "props": {"height": 0.5}, "act": "sleep_home"},
  {"id": "P-H/window", "plot": "P-H", "kind": "board", "ue": [300, 100, 150], "yaw": 180, "props": {"topic": "view"}},
  {"id": "P-H/window_far", "plot": "P-H", "kind": "board", "ue": [900, 0, 150], "yaw": 180, "props": {"topic": "view"}},
  {"id": "P-H/window_up", "plot": "P-H", "kind": "board", "ue": [100, 0, 450], "yaw": 180, "props": {"topic": "view"}},
  {"id": "P-N/window", "plot": "P-N", "kind": "board", "ue": [50, 0, 150], "yaw": 180, "props": {"topic": "view"}},
  {"id": "P-H/cot", "plot": "P-H", "kind": "bed", "ue": [2000, 0, 50], "yaw": 0, "act": "nap"}],
 "acts": {"sleep_home": {"prompt": "p", "minutes": 480, "until": 7, "pose": "lie", "fade": true, "sleep": true, "lines": ["slept"]},
          "nap": {"prompt": "p", "minutes": 90, "pose": "lie", "fade": true}}})";
	FModel M;
	std::string Error;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	const int Bed = M.FindThing("P-H/bed"), Cot = M.FindThing("P-H/cot");
	CHECK(M.WindowFrom(Bed, 600.0) == M.FindThing("P-H/window"), "lying, he looks toward the window of his room (not one far off, upstairs or next door)");
	CHECK(M.WindowFrom(Cot, 600.0) == -1, "no window near: the ceiling");
	FAsk Ask;
	const FOutcome O = M.Run(Bed, At(2, 22.5f), Ask);
	CHECK(O.Verdict == EVerdict::Ok && O.bSleep && O.Pose == "lie" && O.Line == "slept", "a sleep bed: he lies down and chooses the hour");
	CHECK(!M.Run(Cot, At(2, 22.5f), Ask).bSleep, "a nap bed does not ask");
	const FAct& Sleep = *M.ActOf(Bed);
	const FAct& Nap = *M.ActOf(Cot);
	CHECK(FModel::SleepDefaultHour(Sleep, 22.5f) == 7 && FModel::SleepDefaultHour(Nap, 13.2f) == 15 && FModel::SleepDefaultHour(Sleep, 7.2f) == 8,
		"the hour offered: the act's until (7), else now and its minutes (13:12 + 1.5 h: 15), never the hour it is now");
	CHECK(FModel::SleepStep(7, 1, 22.5f) == 8 && FModel::SleepStep(23, 1, 22.5f) == 0 && FModel::SleepStep(23, -1, 22.5f) == 21
		&& FModel::SleepStep(0, -1, 0.5f) == 23, "the chooser goes round the clock and skips the hour it is now");
	CHECK(FModel::MinutesUntil(22.5f, 7) == 510 && FModel::MinutesUntil(22.5f, 23) == 30, "sleeping till 7 from 22:30 is 510 minutes, till 23 half an hour");

	// where he sits for a sitting act on a thing that is not a seat: the chess table's chair, never the middle of the table
	const std::string Tables = R"({"things": [
  {"id": "P-G/chess", "plot": "P-G", "kind": "play", "ue": [0, 0, 78], "yaw": 0, "props": {"what": "chess"}, "act": "chess"},
  {"id": "P-G/chair_a", "plot": "P-G", "kind": "seat", "ue": [75, 0, 45], "yaw": 180, "props": {"height": 0.45}},
  {"id": "P-G/chair_b", "plot": "P-G", "kind": "seat", "ue": [-120, 0, 45], "yaw": 0, "props": {"height": 0.45}},
  {"id": "P-G/chair_up", "plot": "P-G", "kind": "seat", "ue": [20, 0, 345], "yaw": 0, "props": {"height": 0.45}},
  {"id": "P-G/lone_chess", "plot": "P-G", "kind": "play", "ue": [3000, 0, 78], "yaw": 0, "act": "chess"},
  {"id": "P-G/next_door_chair", "plot": "P-N", "kind": "seat", "ue": [3060, 0, 45], "yaw": 0, "props": {"height": 0.45}},
  {"id": "P-G/barber", "plot": "P-G", "kind": "machine", "ue": [6000, 0, 60], "yaw": 0, "act": "haircut"},
  {"id": "P-G/barber_spot", "plot": "P-G", "kind": "machine", "ue": [9000, 0, 60], "yaw": 0, "act": "haircut", "seat": [9010, 40, 45, 90, 0.5]},
  {"id": "P-G/piano", "plot": "P-G", "kind": "play", "ue": [12000, 0, 90], "yaw": 0, "act": "chess", "seat": [12000, -60, 45, 180, 9]},
  {"id": "P-G/piano_stool", "plot": "P-G", "kind": "seat", "ue": [12080, 0, 45], "yaw": 180, "props": {"height": 0.45}}],
 "acts": {"chess": {"prompt": "p", "pose": "sit"}, "haircut": {"prompt": "p", "pose": "sit"}}})";
	FModel T;
	CHECK(T.Load(Tables, Error), "it loads (%s)", Error.c_str());
	const int Chess = T.FindThing("P-G/chess"), ChairA = T.FindThing("P-G/chair_a"), ChairB = T.FindThing("P-G/chair_b");
	const int Lone = T.FindThing("P-G/lone_chess"), Barber = T.FindThing("P-G/barber");
	CHECK(T.SeatFor(ChairA) == ChairA && T.SeatFor(Chess) == ChairA, "a seat is sat on itself; a chess table on its nearest chair (not upstairs)");
	CHECK(T.SeatFor(Chess, ChairA) == ChairB && T.Claim(ChairA, 5) && T.SeatFor(Chess) == ChairB, "the hero's own or a held chair is skipped: the other one");
	CHECK(T.Claim(ChairB, 6) && T.SeatFor(Chess) == -1, "both chairs held: the game is played standing");
	CHECK(T.SeatFor(Lone) == -1 && T.SeatFor(Barber) == -1 && T.SeatFor(-1) == -1,
		"a table or a machine with no chair of its plot by it and no seat spot: used standing");
	const int BarberSpot = T.FindThing("P-G/barber_spot"), Piano = T.FindThing("P-G/piano"), Stool = T.FindThing("P-G/piano_stool");
	CHECK(T.SeatFor(BarberSpot) == BarberSpot && T.Things[(size_t)BarberSpot].bSeatSpot && !T.Things[(size_t)Barber].bSeatSpot,
		"a machine with a seat spot (\"seat\" at generation) is sat at");
	FVec SpotAt;
	float SpotYaw = 0.f, SpotH = 0.f;
	T.SeatFrame(BarberSpot, false, SpotAt, SpotYaw, SpotH);
	CHECK(SpotAt.X == 9010.0 && SpotAt.Y == 40.0 && SpotAt.Z == 45.0 && SpotYaw == 90.f && SpotH == 0.5f,
		"on its spot: the cushion's point, the spot's yaw and height (%.0f %.0f %.0f, %.0f, %.2f)", SpotAt.X, SpotAt.Y, SpotAt.Z, SpotYaw, SpotH);
	T.SeatFrame(ChairA, false, SpotAt, SpotYaw, SpotH);
	CHECK(SpotAt.X == 75.0 && SpotYaw == 180.f && SpotH == 0.45f, "a seat thing: its own point, yaw and height");
	CHECK(T.SeatFor(Piano) == Stool && T.Claim(Stool, 7) && T.SeatFor(Piano) == Piano, "a free seat thing first; held, the spot");
	T.SeatFrame(Piano, false, SpotAt, SpotYaw, SpotH);
	CHECK(SpotH == 0.45f && SpotYaw == 180.f, "a spot's height out of 0.05..1.2 m is the usual 0.45");
}

/** The reading view (docs/city_interiors.md "The reading page"): texts, the page each is left on, read to the end, the "read" line
 *  condition, a newspaper whose second page changes daily, the save. */
static void ReadingChecks()
{
	std::printf("the reading view\n");
	const std::string Text = R"({"things": [
  {"id": "P-L/shelf", "plot": "P-L", "kind": "shelf", "ue": [0, 0, 130], "yaw": 0, "props": {"topic": "birds"}, "act": "read_guide"},
  {"id": "P-L/stand", "plot": "P-L", "kind": "take", "ue": [400, 0, 100], "yaw": 0, "props": {"item": "daily_paper"}, "act": "take_paper"},
  {"id": "P-L/board", "plot": "P-L", "kind": "board", "ue": [800, 0, 150], "yaw": 0, "props": {}, "act": "notices"},
  {"id": "P-K/shelf", "plot": "P-K", "kind": "shelf", "ue": [5000, 0, 130], "yaw": 0, "props": {}, "act": "two_books"}],
 "texts": {
  "guide": {"title": "t.guide", "kind": "book", "minutes": 3, "collect": "reading/guide", "pages": ["g.1", "g.2", "g.3"]},
  "paper": {"title": "t.paper", "kind": "newspaper", "minutes": 2, "pages": [["p.1"], ["p.2a", "p.2b", "p.2c"], [{"k": "p.night", "h": [21, 6]}], "p.3"]},
  "one": {"title": "t.one", "pages": ["o.1"]},
  "empty": {"title": "t.empty", "pages": []}},
 "acts": {"read_guide": {"prompt": "p", "minutes": 15, "lines": ["old.1"], "read": {"texts": ["guide", "nope"]}, "collect": "shelves/birds"},
          "take_paper": {"prompt": "p", "daily": 1, "give": [["daily_paper", 1]], "lines": ["took"], "read": {"texts": ["paper"]}},
          "two_books": {"prompt": "p", "read": {"texts": ["guide", "one"]}},
          "daily_two": {"prompt": "p", "read": {"texts": ["guide", "one"], "rotate": "daily"}},
          "notices": {"prompt": "p", "lines": ["n.always", {"k": "n.after", "read": "guide"}]}},
 "collections": {"reading": {"title": "c", "pieces": ["guide"]}, "shelves": {"title": "s", "pieces": ["birds"]}}})";
	FModel M;
	std::string Error;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	CHECK(M.Texts.size() == 3 && M.FindText("empty") < 0 && M.FindText("guide") >= 0, "texts read, one with no pages left out (%d)", (int)M.Texts.size());
	const int Shelf = M.FindThing("P-L/shelf"), Stand = M.FindThing("P-L/stand"), Board = M.FindThing("P-L/board"), Two = M.FindThing("P-K/shelf");
	const int Guide = M.FindText("guide"), Paper = M.FindText("paper"), One = M.FindText("one");
	CHECK(M.ActOf(Shelf)->Texts.size() == 1 && M.TextAt(Guide)->Minutes == 3 && M.TextAt(One)->Minutes == 2 && M.TextAt(One)->Kind == "book",
		"an act's unknown text is left out; minutes and kind default to 2 and book");
	FAsk Ask;
	Ask.ItemCount = [](const std::string&) { return 0; };
	const FOutcome O = M.Run(Shelf, At(2, 12.f, 300), Ask);
	CHECK(O.Verdict == EVerdict::Ok && O.Text == Guide && O.Line.empty() && O.Minutes == 0 && O.Target == "read_guide",
		"a reading act opens its text: no line, no act minutes (the pages take theirs)");
	CHECK(O.NewStamps.size() == 1 && O.NewStamps[0] == "shelves/birds", "the act's own stamp still comes (the shelf topics)");
	CHECK(M.LineIndex("P-L/shelf") == 0, "the old lines' page does not turn");

	std::vector<std::string> Pages;
	M.TextPages(Guide, At(2, 12.f, 300), Pages);
	CHECK(Pages.size() == 3 && Pages[0] == "g.1" && Pages[2] == "g.3", "three pages of keys");
	CHECK(M.OpenPage(Guide, 3) == 0, "opened the first time: the first page");
	FPageTurn T0 = M.TurnPage(Guide, 0, 0, 3, "P-L");
	FPageTurn T1 = M.TurnPage(Guide, 0, 1, 3, "P-L");
	CHECK(!T0.bMoved && !T0.bFinished && T1.bMoved && T1.Page == 1 && !T1.bFinished, "a turn on: page 2, not finished");
	FPageTurn TBack = M.TurnPage(Guide, 0, -1, 3, "P-L");
	CHECK(!TBack.bMoved && TBack.Page == 0, "back from the first page: nothing");
	M.TurnPage(Guide, 0, 1, 3, "P-L");
	CHECK(M.OpenPage(Guide, 3) == 1, "closed on page 2: opened again on page 2");
	CHECK(M.Run(Board, At(2, 12.f, 300), Ask).NumLines == 1, "a line with read: guide does not show before it is read");
	FPageTurn T2 = M.TurnPage(Guide, 1, 1, 3, "P-L");
	CHECK(T2.bFinished && T2.Page == 2 && M.IsRead("guide") && M.GetReadAt().at("guide") == "P-L" && T2.NewStamps.size() == 1
		&& T2.NewStamps[0] == "reading/guide", "the last page: read, in P-L, the stamp reading/guide");
	CHECK(!M.TurnPage(Guide, 1, 1, 3, "P-K").bFinished && M.GetReadAt().at("guide") == "P-L", "read once: the second time no stamp, the place stays");
	CHECK(M.OpenPage(Guide, 3) == 0, "a text read and left on its last page opens at the start again");
	CHECK(M.Run(Board, At(2, 12.f, 300), Ask).NumLines == 2, "the read: guide line shows now");
	CHECK(M.CountRead() == 1 && !M.AllRead(), "1 of 3 read");

	// the newspaper: the second page changes every day, a page that holds only at night
	std::string Day0, Day1, Day3;
	M.TextPages(Paper, At(2, 12.f, 300), Pages);
	CHECK(Pages.size() == 3, "by day the paper has 3 pages (the night page left out): %d", (int)Pages.size());
	Day0 = Pages[1];
	FWhen W1 = At(3, 12.f, 301);
	W1.Since = 1;
	M.TextPages(Paper, W1, Pages);
	Day1 = Pages[1];
	FWhen W3 = At(5, 12.f, 303);
	W3.Since = 3;
	M.TextPages(Paper, W3, Pages);
	Day3 = Pages[1];
	CHECK(Day0 == "p.2a" && Day1 == "p.2b" && Day3 == "p.2a", "page two by the day: %s, %s, then round again: %s", Day0.c_str(), Day1.c_str(), Day3.c_str());
	M.TextPages(Paper, At(2, 23.f, 300), Pages);
	CHECK(Pages.size() == 4 && Pages[2] == "p.night", "at night the night page is there too");
	const FOutcome P1 = M.Run(Stand, At(2, 12.f, 300), Ask);
	CHECK(P1.Text == Paper && P1.Give.size() == 1, "taking the paper gives it and opens today's issue");
	CHECK(M.Run(Stand, At(2, 13.f, 300), Ask).Verdict == EVerdict::DailyDone, "one paper a day");

	// two texts in turn per use; one a day
	CHECK(M.Run(Two, At(2, 12.f, 300), Ask).Text == Guide && M.Run(Two, At(2, 12.f, 300), Ask).Text == One && M.Run(Two, At(2, 12.f, 300), Ask).Text == Guide,
		"two texts on a shelf: one, the other, the first again");
	FAct Daily = M.Acts[(size_t)M.FindAct("daily_two")];
	CHECK(Daily.bReadDaily && Daily.Texts.size() == 2, "rotate daily read");
	CHECK(M.OpenPage(One, 1) == 0 && M.TurnPage(One, 0, 0, 1, "P-K").bFinished, "a one-page text is read as soon as it is opened");

	// the save
	std::map<std::string, std::string> Saved;
	M.TurnPage(Paper, 0, 1, 3, "P-L");
	M.SaveState(Saved);
	CHECK(Saved.count("r:guide") && Saved["r:guide"] == "P-L" && Saved.count("b:paper") && Saved["b:paper"] == "1" && Saved.count("l:P-K/shelf#text"),
		"saved: read (with the place), the page left on, the turn of the texts");
	FModel M2;
	M2.Load(Text, Error);
	M2.LoadState(Saved);
	CHECK(M2.IsRead("guide") && M2.IsRead("one") && M2.OpenPage(M2.FindText("paper"), 3) == 1 && M2.HasStamp("reading/guide"), "loaded: the same");
	M2.MarkAllRead(true, "");
	CHECK(M2.AllRead() && M2.GetReadAt().at("paper").empty(), "developer: every text read");
	M2.MarkAllRead(false, "");
	CHECK(M2.CountRead() == 0 && M2.OpenPage(M2.FindText("paper"), 3) == 0, "and cleared (the pages too)");
	M2.ResetState();
	CHECK(M2.CountRead() == 0, "a new game forgets what was read");
}

static void TalkChecks()
{
	std::printf("the staff talk and the room tones\n");
	const std::string Text = R"({"things": [
  {"id": "P-K/counter", "plot": "P-K", "kind": "counter", "ue": [0, 0, 110], "yaw": 0, "props": {}, "act": "buy_paper",
   "hours": [8, 20], "days": "daily", "closed": "kiosk_shut"},
  {"id": "P-K/plain", "plot": "P-K", "kind": "counter", "ue": [400, 0, 110], "yaw": 0, "props": {}, "act": "plain", "topics": ["k.own", "k.weather", "nope"]},
  {"id": "P-C/screen", "plot": "P-C", "kind": "screen", "ue": [3000, 0, 300], "yaw": 0, "props": {}, "act": "film"},
  {"id": "P-Q/door", "plot": "P-Q", "kind": "board", "ue": [6000, 0, 150], "yaw": 0, "props": {}, "act": "plain"}],
 "acts": {"buy_paper": {"prompt": "p", "needs_staff": "seller", "lines": ["hello"], "topics": ["k.weather", "k.film", "k.secret", "k.once", "k.night", "k.weather"]},
          "plain": {"prompt": "p", "lines": ["x"]},
          "kiosk_shut": {"prompt": "p", "lines": ["shut"]},
          "film": {"prompt": "p", "lines": ["f"], "target": "film_today"}},
 "topics": {
  "k.weather": {"k": "t.weather", "lines": ["w.1", {"k": "w.rain", "weather": "rain"}, {"k": "w.first", "once": 1}], "collect": "staff/kiosk"},
  "k.film": {"k": "t.film", "lines": [{"k": "f.before", "did": "!film_today"}, {"k": "f.after", "did": "film_today"}]},
  "k.secret": {"k": "t.secret", "heard": "k.weather", "lines": ["s.1"]},
  "k.once": {"k": "t.once", "once": 1, "lines": ["o.1"], "collect": "staff/once"},
  "k.night": {"k": "t.night", "h": [18, 20], "lines": ["n.1"]},
  "k.own": {"k": "t.own", "lines": ["own.1"]},
  "k.empty": {"k": "t.empty", "lines": []}},
 "tones": {
  "shop": {"loop": "hum", "asset": "/Game/A/S_hum_01.S_hum_01", "volume": 3.0, "shots": ["a", "b"], "every": [10, 20], "shot_volume": 0.5, "closed": "quiet"},
  "quiet": {"loop": "hum", "volume": 0.05},
  "hall": {"loop": "far", "every": [0, 1]}},
 "plot_tones": {"P-K": "shop", "P-C": "hall", "P-Q": "nope"},
 "collections": {"staff": {"title": "c", "pieces": ["kiosk", "once"]}}})";
	FModel M;
	std::string Error;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	CHECK(M.Topics.size() == 6 && M.FindTopic("k.empty") < 0, "6 topics, one with no lines left out (%d)", (int)M.Topics.size());
	const int Counter = M.FindThing("P-K/counter"), Plain = M.FindThing("P-K/plain"), Screen = M.FindThing("P-C/screen"), Door = M.FindThing("P-Q/door");
	CHECK(M.ActOf(Counter)->Topics.size() == 5 && M.Things[(size_t)Plain].Topics.size() == 2, "unknown and repeated topic ids are dropped");
	CHECK(M.HasTopics(Counter) && M.HasTopics(Plain) && !M.HasTopics(Screen), "who has topics");

	bool bOnShift = true;
	FAsk Ask;
	Ask.ItemCount = [](const std::string&) { return 0; };
	Ask.StaffOnShift = [&bOnShift](const std::string&, const std::string& Role) { return bOnShift && (Role == "seller" || Role == "*"); };
	std::vector<int> Offer;
	const FWhen Noon = At(2, 12.f, 400);
	CHECK(M.TalkReady(Counter, Noon, Ask, Offer) == 3, "at noon: weather, film and the once topic (secret before weather, night by day not): %d", (int)Offer.size());
	CHECK(M.TopicAt(Offer[0])->Id == "k.weather" && M.TopicAt(Offer[1])->Id == "k.film", "in the act's order");
	CHECK(M.TalkReady(Counter, At(2, 19.f, 400), Ask, Offer) == 4, "at 19 the evening topic too");
	CHECK(M.TalkReady(Counter, At(2, 22.f, 400), Ask, Offer) == 0 && M.TopicsNow(Counter, At(2, 22.f, 400), Offer) == 0,
		"shut (its closed words): no talk");
	CHECK(M.TalkReady(Counter, At(2, 22.f, 400), FAsk{0, true, Ask.ItemCount, nullptr}, Offer) == 3, "developer mode opens all hours and needs nobody");
	bOnShift = false;
	CHECK(M.TalkReady(Counter, Noon, Ask, Offer) == 0, "nobody on shift: no talk");
	bOnShift = true;
	CHECK(M.TalkRole(Counter) == "seller" && M.TalkRole(Plain) == "*", "the speaker's role: needs_staff or anybody");
	CHECK(M.TalkReady(Plain, Noon, Ask, Offer) == 2 && M.TopicAt(Offer[0])->Id == "k.own", "a thing's own topics");

	const int Weather = M.FindTopic("k.weather"), Film = M.FindTopic("k.film"), Secret = M.FindTopic("k.secret"), Once = M.FindTopic("k.once");
	FTopicAnswer A1 = M.AskTopic(Weather, Noon);
	CHECK(A1.bFirst && A1.Lines.size() == 2 && A1.Lines[0] == "w.1" && A1.Lines[1] == "w.first" && A1.Event == "ask" && A1.Target == "k.weather",
		"asked: the plain line and the once line (no rain: weather unknown), event ask/k.weather (%d lines)", (int)A1.Lines.size());
	CHECK(A1.NewStamps.size() == 1 && A1.NewStamps[0] == "staff/kiosk" && M.IsHeard("k.weather"), "heard; the first time the stamp staff/kiosk");
	FWhen Rain = Noon;
	Rain.Weather = WeatherRain;
	FTopicAnswer A2 = M.AskTopic(Weather, Rain);
	CHECK(!A2.bFirst && A2.NewStamps.empty() && A2.Lines.size() == 2 && A2.Lines[1] == "w.rain", "asked again in the rain: the rain line, no once line, no stamp");
	CHECK(M.TopicOffered(Secret, Noon) && M.TalkReady(Counter, Noon, Ask, Offer) == 4, "the secret topic opens once weather was heard");
	CHECK(M.AskTopic(Film, Noon).Lines[0] == "f.before", "film not seen: the before line");
	M.Run(Screen, Noon, Ask);
	CHECK(M.HasDone("film_today") && M.AskTopic(Film, Noon).Lines[0] == "f.after", "after the film: the after line (did)");
	FTopicAnswer A3 = M.AskTopic(Once, Noon);
	CHECK(A3.bFirst && A3.NewStamps.size() == 1 && !M.TopicOffered(Once, Noon), "a once topic is gone once heard");

	std::map<std::string, std::string> Saved;
	M.SaveState(Saved);
	CHECK(Saved.count("h:k.weather") && Saved.count("h:k.once") && Saved.count("w:film_today"), "saved: heard topics and what was done");
	FModel M2;
	M2.Load(Text, Error);
	M2.LoadState(Saved);
	CHECK(M2.IsHeard("k.weather") && M2.IsHeard("k.once") && M2.HasDone("film_today") && M2.GetHeard().size() == 3, "loaded: the same (%d heard)",
		(int)M2.GetHeard().size());
	CHECK(M2.AskTopic(M2.FindTopic("k.weather"), Noon).Lines.size() == 1, "the once line stays shown after a load");
	M2.ForgetTopics();
	CHECK(M2.GetHeard().empty() && M2.TopicOffered(M2.FindTopic("k.once"), Noon) && M2.AskTopic(M2.FindTopic("k.weather"), Noon).Lines.size() == 2,
		"developer: forgotten, once topics and lines back");
	M2.ResetState();
	CHECK(M2.GetHeard().empty() && !M2.HasDone("film_today"), "a new game forgets them");

	// room tones
	const int Shop = M.FindTone("shop"), Quiet = M.FindTone("quiet"), Hall = M.FindTone("hall");
	CHECK(M.Tones.size() == 3 && M.ToneAt(Shop)->Volume == 1.f && M.ToneAt(Shop)->Closed == Quiet && M.ToneAt(Shop)->Asset == "/Game/A/S_hum_01.S_hum_01",
		"3 tones; volume clamped to 1; the closed tone resolved");
	CHECK(M.ToneOfPlot("P-K") == Shop && M.ToneOfPlot("P-Q") < 0 && M.ToneOfPlot("P-X") < 0, "plots to tones, an unknown tone dropped");
	CHECK(M.ToneNow(Counter, Noon, false) == Shop && M.ToneNow(Counter, At(2, 22.f, 400), false) == Quiet
		&& M.ToneNow(Counter, At(2, 22.f, 400), true) == Shop && M.ToneNow(Door, Noon, false) < 0, "open: the shop tone, shut: the quiet one, no tone: silence");
	FRng Rng;
	Rng.State = 7;
	bool bInRange = true;
	std::set<std::string> Picked;
	for (int I = 0; I < 200; ++I)
	{
		const double S = M.NextShotIn(Shop, Rng);
		bInRange = bInRange && S >= 10.0 && S <= 20.0;
		Picked.insert(M.PickShot(Shop, Rng));
	}
	CHECK(bInRange && Picked.size() == 2, "one-shots every 10..20 s, both ids come up");
	CHECK(M.NextShotIn(Quiet, Rng) < 0.0 && M.PickShot(Quiet, Rng).empty() && M.ToneAt(Hall)->EveryMin >= 2.f, "no shots: none; every is clamped to 2 s or more");
}

static void LiftChecks()
{
	std::printf("lifts and storey links\n");
	const std::string Text = R"({"things": [
  {"id": "P-U/lift_0", "plot": "P-U", "kind": "door", "ue": [0, 0, 100], "yaw": 0, "props": {"to": "lift_1"},
   "floors": [{"k": "f.street", "to": "lift_1"}, {"k": "f.roof", "to": "P-U/lift_2"}, {"k": "f.nope", "to": "nowhere"}, {"k": "f.self", "to": "lift_0"}]},
  {"id": "P-U/lift_1", "plot": "P-U", "kind": "door", "ue": [0, 0, 2500], "yaw": 90, "props": {}, "floors": [{"to": "lift_0"}]},
  {"id": "P-U/lift_2", "plot": "P-U", "kind": "door", "ue": [0, 0, 5300], "yaw": 180, "props": {}, "floors": [{"to": "lift_1"}]},
  {"id": "P-U/hatch", "plot": "P-U", "kind": "door", "ue": [500, 0, 5300], "yaw": 0, "props": {}, "oneway": true, "floors": [{"to": "P-X/yard"}]},
  {"id": "P-X/yard", "plot": "P-X", "kind": "board", "ue": [9000, 0, 100], "yaw": 0, "props": {}},
  {"id": "P-U/plain", "plot": "P-U", "kind": "door", "ue": [800, 0, 100], "yaw": 0, "props": {"to": "staff"}}],
 "acts": {}})";
	FModel M;
	std::string Error;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	const int L0 = M.FindThing("P-U/lift_0"), L1 = M.FindThing("P-U/lift_1"), L2 = M.FindThing("P-U/lift_2"), Hatch = M.FindThing("P-U/hatch"),
		Yard = M.FindThing("P-X/yard"), Plain = M.FindThing("P-U/plain");
	const FThing& T0 = M.Things[(size_t)L0];
	CHECK(T0.Floors.size() == 2 && T0.Floors[0].To == L1 && T0.Floors[0].Key == "f.street" && T0.Floors[1].To == L2,
		"stops by local and full id; unknown and self stops dropped (%d)", (int)T0.Floors.size());
	CHECK(M.IsLift(L0) && M.IsLift(L1) && !M.IsLift(Plain) && M.Things[(size_t)Hatch].Floors[0].To == Yard && M.Things[(size_t)Hatch].bOneWay,
		"a caption-only door is no lift; a one-way hatch to another plot");
	CHECK(M.FindLiftTarget("P-U", "lift_2") == L2 && M.FindLiftTarget("P-U", "P-X/yard") == Yard && M.FindLiftTarget("P-U", "") < 0,
		"targets: own plot first, then a full id");
	CHECK(M.LiftReturns(L0, L1) && M.LiftReturns(L1, L0) && M.LiftReturns(L0, L2) && !M.LiftReturns(Hatch, Yard), "ways back: direct, via the plot's lift; none from a yard");
	FVec Spots[8];
	float Yaw = 0.f;
	const int N = M.LiftSpots(L1, Spots, 8, Yaw);
	CHECK(N == 7 && Yaw == 90.f && std::fabs(Spots[0].X) < 1e-6 && std::fabs(Spots[0].Y - 60.0) < 1e-6 && Spots[0].Z == 2500.0,
		"arrival: 0.6 m in front of the door (yaw 90: +Y), facing away, 7 spots (%.1f, %.1f)", Spots[0].X, Spots[0].Y);
	CHECK(std::fabs(Spots[1].Y - 100.0) < 1e-6 && std::fabs(Spots[2].X + 50.0) < 1e-6 && std::fabs(Spots[3].X - 50.0) < 1e-6, "then 1 m, then to the sides");
	CHECK(M.LiftSpots(L1, Spots, 2, Yaw) == 2 && M.LiftSpots(-1, Spots, 8, Yaw) == 0, "at most Max; none for no thing");
	CHECK(FModel::LiftFloorOk(2500.0, 2400.0) && FModel::LiftFloorOk(2500.0, 2530.0) && !FModel::LiftFloorOk(2500.0, 2000.0)
		&& !FModel::LiftFloorOk(2500.0, 2600.0), "a floor 1 m under the door's point is believed, 5 m under or 1 m over is not");
}

static void RealGameChecks(const FModel& M);

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
	// seat spots (docs "pose sit"): sitting things that are not seats, how many sit on a spot, on a seat thing, or stand
	int OnSpot = 0, OnSeat = 0, Standing = 0, FarSpot = 0;
	for (int I = 0; I < (int)M.Things.size(); ++I)
	{
		const FAct* A = M.ActOf(I);
		const FThing& T = M.Things[(size_t)I];
		if (!A || A->Pose != "sit" || !A->Screen.empty() || FModel::IsSeatThing(T))
		{
			continue;
		}
		const int Seat = M.SeatFor(I);
		OnSeat += Seat >= 0 && Seat != I ? 1 : 0;
		OnSpot += Seat == I ? 1 : 0;
		Standing += Seat < 0 ? 1 : 0;
		const double DX = T.SeatAt.X - T.At.X, DY = T.SeatAt.Y - T.At.Y;
		FarSpot += T.bSeatSpot && (DX * DX + DY * DY > 160.0 * 160.0 || T.SeatAt.Z - T.At.Z > 100.0 || T.At.Z - T.SeatAt.Z > 160.0) ? 1 : 0;
	}
	CHECK(OnSpot > 0 && FarSpot == 0, "sitting things that are not seats: %d on a seat thing, %d on a seat spot (%d off their thing), %d standing",
		OnSeat, OnSpot, FarSpot, Standing);
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
					EmptyNow++; if (getenv("CITYSIM_GAPS")) std::printf("  gap: %s at %d\n", A.Id.c_str(), H);
					break;
				}
			}
		}
	}
	std::printf("  daily acts %d, lines with conditions %d, screen acts %d\n", Daily, Conditional, Screens);
	CHECK(EmptyNow == 0, "every act with lines has a line for every hour (%d have a gap)", EmptyNow);

	// the staff talk: every thing with topics talks at some hour of its open day (developer mode: nobody needed), every
	// topic is offered somewhere, and every offered topic answers with at least one line
	FAsk Dev = Ask;
	Dev.bOpenAll = true;
	int WithTopics = 0, Silent = 0, NoAnswer = 0;
	std::string FirstSilent;
	std::set<int> Reached;
	std::vector<int> Offer;
	std::vector<std::pair<int, FWhen>> FirstAsk;
	for (int I = 0; I < (int)M.Things.size(); ++I)
	{
		if (!M.HasTopics(I))
		{
			continue;
		}
		WithTopics++;
		bool bTalks = false;
		for (int H = 0; H < 24; ++H)
		{
			FWhen W = AtDate(H % 7, (float)H + 0.5f, 100, 615, 30);
			static const std::vector<std::string> TownTalk{"cup"};
			W.Rumours = H == 12 ? &TownTalk : nullptr; // at noon the town talks of the hero's cup (docs/town_rumours.md)
			if (M.TalkReady(I, W, Dev, Offer) > 0)
			{
				bTalks = true;
				for (const int Topic : Offer)
				{
					if (Reached.insert(Topic).second)
					{
						FirstAsk.push_back(std::make_pair(Topic, W));
					}
				}
			}
		}
		if (!bTalks)
		{
			Silent++;
			FirstSilent = FirstSilent.empty() ? M.Things[(size_t)I].Id : FirstSilent;
		}
	}
	for (const auto& Ask1 : FirstAsk)
	{
		NoAnswer += M.AskTopic(Ask1.first, Ask1.second).Lines.empty() ? 1 : 0;
	}
	M.ResetState();
	int NoTitle = 0;
	for (const FTopic& T : M.Topics)
	{
		NoTitle += T.Title.empty() || T.Lines.empty() ? 1 : 0;
	}
	CHECK(Silent == 0, "%d things with topics, each talks at some hour (%d do not, first %s)", WithTopics, Silent, FirstSilent.c_str());
	CHECK(NoTitle == 0 && NoAnswer == 0, "%d topics, each with a title and lines; every offered one answers (%d, %d not)", (int)M.Topics.size(), NoTitle, NoAnswer);
	// six wait on a fresh save: two older ones, and the four photo quests' "I have the print" (photo.city.done,
	// repair.grandpa.done, keeper.pier.done, cog.cl.maru.done: each heard after the topic that gives its quest)
	// + 2: the telescopes' topics that wait for a stamp of the sky log (docs/telescopes.md)
	// + 1: the beach's topic that waits for a stamp of the shore's finds (docs/beach.md)
	// + 1: the canteen cook's "taste it" waits for the stamp cook/fine (docs/kitchen.md)
	// + the topics of one holiday (docs/festivals.md "Science Day, the Old New Year, Neptune Day"): offered on its day only
	// (holidaychecks.h asks them on their days)
	int Festive = 0;
	for (int I = 0; I < (int)M.Topics.size(); ++I)
	{
		Festive += M.Topics[(size_t)I].When.IsFestive() && !Reached.count(I) ? 1 : 0;
	}
	CHECK((int)Reached.size() + 6 + 4 + Festive >= (int)M.Topics.size(), "%d of %d topics are offered on a fresh save at some hour (the rest wait for heard, did, read or count; %d for their holiday)",
		(int)Reached.size(), (int)M.Topics.size(), Festive);
	// the room tones: every tone a plot names is there, a closed tone is a tone, volumes in 0..1, every min <= max
	int BadTone = 0, Loud = 0;
	for (const FTone& T : M.Tones)
	{
		BadTone += (T.Closed >= (int)M.Tones.size() || T.EveryMin > T.EveryMax || (T.Loop.empty() && T.Shots.empty())) ? 1 : 0;
		Loud += (T.Volume < 0.f || T.Volume > 1.f || T.ShotVolume < 0.f || T.ShotVolume > 1.f) ? 1 : 0;
	}
	CHECK(!M.Tones.empty() && BadTone == 0 && Loud == 0, "%d room tones, each sound (%d bad, %d out of 0..1)", (int)M.Tones.size(), BadTone, Loud);
	// the rooms in the rain (docs/indoor_weather.md): the keeper's radio in the lighthouse, a full cafe; dry: as ever
	{
		int Pier = -1, Cafe = -1, BadAlt = 0;
		for (int I = 0; I < (int)M.Things.size(); ++I)
		{
			Pier = Pier < 0 && M.Things[(size_t)I].Plot == "P-PIER" ? I : Pier;
			Cafe = Cafe < 0 && M.Things[(size_t)I].Plot == "P-CAFE" ? I : Cafe;
		}
		for (const FTone& T : M.Tones)
		{
			for (const auto& Alt : T.ByWeather)
			{
				BadAlt += Alt.second < 0 || Alt.second >= (int)M.Tones.size() || Alt.first == 0 ? 1 : 0;
			}
			BadAlt += T.Windows < 0.f || T.Windows > 1.f ? 1 : 0;
		}
		FWhen Wet;
		Wet.Hour = 23.f;
		Wet.Weather = WeatherRain | WeatherWet | WeatherCold;
		FWhen Dry = Wet;
		Dry.Weather = WeatherDry | WeatherClear;
		const int Room = M.FindTone("lighthouse_room"), Storm = M.FindTone("lighthouse_storm");
		CHECK(Pier >= 0 && M.ToneNow(Pier, Dry, true) == Room && M.ToneNow(Pier, Wet, true) == Storm && M.ToneAt(Storm)->Windows > 0.9f,
			"the lighthouse: its room dry, the keeper's radio in the rain, the rain on every pane");
		Wet.Hour = Dry.Hour = 13.f;
		CHECK(Cafe >= 0 && M.ToneNow(Cafe, Dry, true) == M.FindTone("cafe_room") && M.ToneNow(Cafe, Wet, true) == M.FindTone("cafe_full"),
			"the cafe: its room dry, full in the rain");
		CHECK(BadAlt == 0, "every weather tone is a tone, every window share in 0..1 (%d bad)", BadAlt);
	}
	int Lifts = 0, NoWayBack = 0;
	std::string FirstOneWay;
	for (int I = 0; I < (int)M.Things.size(); ++I)
	{
		if (!M.IsLift(I))
		{
			continue;
		}
		Lifts++;
		for (const FFloor& F : M.Things[(size_t)I].Floors)
		{
			if (!M.Things[(size_t)I].bOneWay && !M.LiftReturns(I, F.To))
			{
				NoWayBack++;
				FirstOneWay = FirstOneWay.empty() ? M.Things[(size_t)I].Id : FirstOneWay;
			}
		}
	}
	CHECK(NoWayBack == 0, "%d lift doors, every stop has a way back unless one-way (%d not, first %s)", Lifts, NoWayBack, FirstOneWay.c_str());
	RealGameChecks(M);
}

// --- mini-games (docs/city_interiors.md "Mini-games")

/** Ticks a game in 1/60 s steps until Done holds or Seconds pass; true when Done held. */
template <typename FDone>
static bool RunUntil(const FModel& M, FGameState& G, double Seconds, FDone Done)
{
	for (int I = 0; I < (int)(Seconds * 60.0); ++I)
	{
		if (Done())
		{
			return true;
		}
		M.TickGame(G, 1.0 / 60.0);
	}
	return Done();
}

/** Plays a game to its end: aim presses at the zone's middle (bAim) or never; rally presses at the ball's arrival (or never);
 *  duel picks the move that beats the opponent's (bRead) or always move 0. Returns false if it did not end in Seconds. */
static bool Play(const FModel& M, FGameState& G, bool bGood, double Seconds = 240.0)
{
	const FGameDef* D = M.GameDef(G.Def);
	for (int I = 0; I < (int)(Seconds * 60.0) && !G.bOver; ++I)
	{
		M.TickGame(G, 1.0 / 60.0);
		if (G.bOver || G.Clock < G.Wait)
		{
			continue;
		}
		if (D->Type == EGameType::Aim && bGood && std::fabs(G.Marker - G.ZoneAt) < D->Zone / 12.0)
		{
			M.PressGame(G, -1);
		}
		else if (D->Type == EGameType::Rally && bGood && !G.bAway && std::fabs(G.Clock - G.Arrive) < 1.0 / 60.0)
		{
			M.PressGame(G, -1);
		}
		else if (D->Type == EGameType::Duel)
		{
			M.PressGame(G, bGood ? (G.OppMove + 1) % 3 : 0);
		}
	}
	return G.bOver;
}

static void GameChecks()
{
	std::printf("mini-games\n");
	const std::string Text = R"({"things": [
  {"id": "P-CLUB/darts", "plot": "P-CLUB", "kind": "play", "ue": [0, 0, 150], "yaw": 0, "props": {"what": "darts"}, "act": "darts", "hours": [10, 22], "days": "mon-sun"},
  {"id": "P-CLUB/hoop", "plot": "P-CLUB", "kind": "sport", "ue": [600, 0, 150], "yaw": 0, "props": {"what": "basketball"}, "act": "hoop", "hours": [0, 24], "days": "mon-sun"},
  {"id": "P-CLUB/tt", "plot": "P-CLUB", "kind": "play", "ue": [1200, 0, 100], "yaw": 0, "props": {"what": "table_tennis"}, "act": "tt", "hours": [0, 24], "days": "mon-sun"},
  {"id": "P-CLUB/wall", "plot": "P-CLUB", "kind": "play", "ue": [1800, 0, 100], "yaw": 0, "props": {"what": "table_tennis"}, "act": "wall", "hours": [0, 24], "days": "mon-sun"},
  {"id": "P-CLUB/chess", "plot": "P-CLUB", "kind": "play", "ue": [2400, 0, 80], "yaw": 0, "props": {"what": "chess"}, "act": "chess", "hours": [0, 24], "days": "mon-sun"},
  {"id": "P-PARK/chess", "plot": "P-PARK", "kind": "play", "ue": [9000, 0, 80], "yaw": 0, "props": {"what": "chess"}, "act": "chess", "hours": [0, 24], "days": "mon-sun"},
  {"id": "P-CLUB/switch", "plot": "P-CLUB", "kind": "machine", "ue": [3000, 0, 140], "yaw": 0, "props": {"what": "switch", "switch": "hall"}, "act": "sw", "hours": [0, 24], "days": "mon-sun"},
  {"id": "P-CLUB/switch2", "plot": "P-CLUB", "kind": "machine", "ue": [3300, 0, 140], "yaw": 0, "props": {"what": "switch"}, "act": "sw", "hours": [0, 24], "days": "mon-sun"}],
 "acts": {
  "darts": {"prompt": "p", "lines": ["city.do.darts.1"], "minutes": 10, "event": "do", "target": "darts", "pose": "stand",
   "game": {"type": "aim", "rounds": 5, "need": 6, "speed": 0.7, "zone": 0.2, "vs": "darts", "win": ["d.win.1", "d.win.2"], "lose": ["d.lose"], "draw": ["d.draw"],
    "collect": "darts_wins/{opponent}", "prize": [["postcard", 1]], "daily": 2, "sounds": {"hit": "rest_clink", "miss": "", "end": "rest_bell"}}},
  "hoop": {"prompt": "p", "lines": ["city.do.basketball.1"], "minutes": 15,
   "game": {"type": "aim", "rounds": 5, "need": 6, "speed": 0.55, "zone": 0.22, "win": ["h.win"], "lose": ["h.lose"], "draw": ["h.draw"]}},
  "tt": {"prompt": "p", "lines": ["x"], "minutes": 20,
   "game": {"type": "rally", "need": 3, "speed": 0.9, "zone": 0.5, "shrink": 0.88, "pace": 1.05, "vs": "table_tennis", "win": ["t.win"], "lose": ["t.lose"], "draw": ["t.draw"]}},
  "wall": {"prompt": "p", "lines": ["x"], "minutes": 20,
   "game": {"type": "rally", "need": 6, "speed": 0.9, "zone": 0.5, "win": ["w.win"], "lose": ["w.lose"], "draw": ["w.draw"]}},
  "chess": {"prompt": "p", "lines": ["x"], "minutes": 40, "pose": "sit",
   "game": {"type": "duel", "rounds": 3, "vs": "chess", "moves": [{"k": "m1", "power": 2, "tell": "t1"}, {"k": "m2", "power": 1, "tell": "t2"}, {"k": "m3", "power": 2, "tell": "t3"}],
    "win": ["c.win"], "lose": ["c.lose"], "draw": ["c.draw"], "collect": "chess_wins/{opponent}"}},
  "sw": {"prompt": "p", "any_hour": true}
 },
 "collections": {"chess_wins": {"title": "t", "pieces": ["ann", "bob", "cid"]}, "darts_wins": {"title": "t", "pieces": ["dan"]}},
 "regulars": {
  "ann": {"name": "npc.ann.name", "style": "bold", "skill": 0.6, "plays": ["chess"], "plots": ["P-CLUB"], "hello": "a.hi", "win": "a.w", "lose": "a.l"},
  "bob": {"name": "npc.bob.name", "style": "careful", "skill": 0.3, "plays": ["chess", "table_tennis"], "plots": ["P-CLUB"]},
  "cid": {"name": "npc.cid.name", "style": "tricky", "skill": 0.9, "plays": ["chess"], "plots": ["P-ELSEWHERE"]},
  "dan": {"name": "npc.dan.name", "style": "steady", "skill": 0.5, "plays": ["darts"]}
 }})";
	FModel M;
	std::string Error;
	CHECK(M.Load(Text, Error), "a games file loads (%s)", Error.c_str());
	CHECK(M.Games.size() == 5 && M.Regulars.size() == 4, "five games, four regulars (%d, %d)", (int)M.Games.size(), (int)M.Regulars.size());
	const int Darts = M.FindThing("P-CLUB/darts"), Hoop = M.FindThing("P-CLUB/hoop"), Tt = M.FindThing("P-CLUB/tt"), Wall = M.FindThing("P-CLUB/wall");
	const int Chess = M.FindThing("P-CLUB/chess"), ParkChess = M.FindThing("P-PARK/chess");
	const FGameDef* DD = M.GameOfThing(Darts);
	CHECK(DD && DD->Type == EGameType::Aim && DD->Rounds == 5 && DD->Need == 6 && DD->Vs == "darts" && DD->Daily == 2 && DD->Prize.size() == 1
		&& DD->Results[0].Lines.size() == 2 && DD->SoundHit == "rest_clink", "the darts' game reads: aim, 5 throws, vs darts, 2 a day, a prize, 2 win lines");
	const FGameDef* CD = M.GameOfThing(Chess);
	CHECK(CD && CD->Type == EGameType::Duel && CD->Moves.size() == 3 && CD->Moves[0].Power == 2 && CD->Moves[2].Tell == "t3", "the chess duel: three moves with powers and tells");
	const FRegular& Ann = M.Regulars[(size_t)M.FindRegular("ann")];
	CHECK(Ann.Lean[0] > Ann.Lean[1] && Ann.Lean[0] > Ann.Lean[2] && M.Regulars[(size_t)M.FindRegular("cid")].Honesty < Ann.Honesty,
		"a bold regular leans to the first move; a skilled tricky one is harder to read");
	CHECK(MoveBeats(0, 2) && MoveBeats(1, 0) && MoveBeats(2, 1) && !MoveBeats(0, 1) && !MoveBeats(1, 1), "the moves go round like rock, paper, scissors");

	// Run: the game starts instead of the lines, while today's games last
	FAsk Ask;
	Ask.Money = 100;
	FOutcome R1 = M.Run(Darts, At(2, 12.f, 50), Ask);
	FOutcome R2 = M.Run(Darts, At(2, 12.5f, 50), Ask);
	FOutcome R3 = M.Run(Darts, At(2, 13.f, 50), Ask);
	CHECK(R1.Verdict == EVerdict::Ok && R1.Game == M.Acts[(size_t)M.Things[Darts].Act].Game && R1.Game >= 0 && R1.Line.empty() && R2.Game >= 0,
		"the darts start their game (no line shown)");
	CHECK(R3.Game < 0 && R3.Line == "city.do.darts.1", "two games a day, then the act runs plainly (its line)");
	FOutcome R4 = M.Run(Darts, At(3, 12.f, 51), Ask);
	CHECK(R4.Game >= 0, "the next day: a game again");
	M.ResetToday();
	CHECK(M.Run(Darts, At(3, 12.f, 51), Ask).Game >= 0, "reset today: games again");
	CHECK(M.Run(Darts, At(3, 23.f, 51), Ask).Verdict == EVerdict::Closed, "the club is shut at 23: no game");

	// opponents: the plot's regulars first, else anybody who plays it
	int Pool[8];
	const int NClub = M.Opponents(*CD, "P-CLUB", Pool, 8);
	CHECK(NClub == 2 && M.Regulars[(size_t)Pool[0]].Id == "ann" && M.Regulars[(size_t)Pool[1]].Id == "bob", "at the club: ann and bob play chess (%d)", NClub);
	CHECK(M.Opponents(*CD, "P-PARK", Pool, 8) == 3, "in the park nobody is at home: all three chess players may come");
	CHECK(M.Opponents(*M.GameOfThing(Hoop), "P-CLUB", Pool, 8) == 0, "free throws are alone");

	// determinism: the same seed and the same presses, the same game
	FGameState A1, A2, A3;
	M.StartGameSeeded(Darts, 12345, A1);
	M.StartGameSeeded(Darts, 12345, A2);
	M.StartGameSeeded(Darts, 99999, A3);
	CHECK(A1.Opponent == A2.Opponent && A1.ZoneAt == A2.ZoneAt && A1.Phase == A2.Phase && (A1.ZoneAt != A3.ZoneAt || A1.Phase != A3.Phase),
		"a seed fixes the opponent and the zone; another seed moves them");
	Play(M, A1, true);
	Play(M, A2, true);
	CHECK(A1.bOver && A2.bOver && A1.Score == A2.Score && A1.OppScore == A2.OppScore && A1.Result == A2.Result, "played the same way, the same result (%d:%d %s)",
		A1.Score, A1.OppScore, GameResultName(A1.Result));
	CHECK(A1.Score == 10 && A1.Round == 5, "five throws at the zone's middle: five bulls, 10 points");

	// aim: a press far from the zone misses, a throw not made in time is a miss
	FGameState Aim;
	M.StartGameSeeded(Hoop, 7, Aim);
	CHECK(Aim.Opponent < 0 && Aim.Clock < Aim.Wait, "free throws: alone, a pause before the first");
	M.PressGame(Aim, -1);
	CHECK(Aim.Round == 0 && Aim.BeatSerial == 0, "a press in the pause is not taken");
	const double Zone = M.GameOfThing(Hoop)->Zone;
	RunUntil(M, Aim, 10.0, [&]() { return Aim.Clock >= Aim.Wait && std::fabs(Aim.Marker - Aim.ZoneAt) > Zone; });
	M.PressGame(Aim, -1);
	CHECK(Aim.Round == 1 && Aim.Score == 0 && Aim.Beat == EGameBeat::Miss, "a press outside the zone: a miss");
	RunUntil(M, Aim, 10.0, [&]() { return Aim.Clock >= Aim.Wait && std::fabs(Aim.Marker - Aim.ZoneAt) > Zone / 6.0 + 0.01 && std::fabs(Aim.Marker - Aim.ZoneAt) < Zone * 0.5 - 0.01; });
	M.PressGame(Aim, -1);
	CHECK(Aim.Round == 2 && Aim.Score == 1 && Aim.Beat == EGameBeat::Hit, "in the zone but off its middle: 1 point");
	const int Before = Aim.Round;
	RunUntil(M, Aim, 30.0, [&]() { return Aim.Round > Before; });
	CHECK(Aim.Round == Before + 1 && Aim.Beat == EGameBeat::Miss && Aim.Clock < Aim.Wait + FModel::AimRoundLimit + FModel::AimPause + 1.0,
		"no throw for %.0f s: a miss", FModel::AimRoundLimit);
	float Mk, Z0, Z1, I0, I1;
	CHECK(M.GameBar(Aim, Mk, Z0, Z1, I0, I1) && Z0 < I0 && I1 < Z1 && Z1 - Z0 > 0.2f && Mk >= 0.f && Mk <= 1.f, "the bar: the zone round its bull, the marker on the bar");
	Play(M, Aim, false);
	CHECK(Aim.bOver && Aim.Result == EGameResult::Lose && Aim.Score == 1, "alone, 1 point of 6: lost");
	FGameState Close;
	M.StartGameSeeded(Hoop, 8, Close);
	for (int K = 0; K < 2; ++K)
	{
		RunUntil(M, Close, 10.0, [&]() { return Close.Clock >= Close.Wait && std::fabs(Close.Marker - Close.ZoneAt) < Zone / 12.0; });
		M.PressGame(Close, -1);
	}
	Play(M, Close, false);
	CHECK(Close.Result == EGameResult::Draw && Close.Score == 4, "alone, 4 of 6 (half or more): a draw, \"close\" (%d points, %s)", Close.Score, GameResultName(Close.Result));
	FGameState Good;
	M.StartGameSeeded(Hoop, 9, Good);
	Play(M, Good, true);
	CHECK(Good.Result == EGameResult::Win && Good.Score == 10, "alone, five bulls: won");

	// rally: the window round the arrival, early and late lose the point, the wall returns everything
	FGameState W;
	M.StartGameSeeded(Wall, 3, W);
	CHECK(W.Opponent < 0 && !W.bAway && std::fabs(W.Arrive - W.Wait - 1.0 / 0.9) < 1e-6, "against the wall: the first ball comes 1/speed s after the pause");
	RunUntil(M, W, 10.0, [&]() { return W.Clock >= W.HitAt + 0.1 * W.Flight; });
	M.PressGame(W, -1);
	CHECK(W.BeatSerial == 0 && !W.bOver, "a press just after the ball left is not taken");
	RunUntil(M, W, 10.0, [&]() { return W.Clock >= W.Arrive - 1.0 / 120.0; });
	const double Win0 = W.Window, Fl0 = W.Flight;
	M.PressGame(W, -1);
	CHECK(W.Hits == 1 && W.bAway && W.Beat == EGameBeat::Hit && W.Window < Win0 && W.Flight < Fl0, "at the arrival: returned; the window shrinks, the ball speeds up");
	RunUntil(M, W, 10.0, [&]() { return !W.bAway; });
	CHECK(!W.bAway && W.Arrive > W.Clock, "the wall sends it back");
	RunUntil(M, W, 10.0, [&]() { return W.Clock >= W.HitAt + 0.5 * W.Flight; });
	if (W.Clock < W.Arrive - W.Window * 0.5)
	{
		M.PressGame(W, -1);
	}
	CHECK(W.bOver && W.Beat == EGameBeat::Early && W.Hits == 1 && W.Result == EGameResult::Lose, "too early: the rally (the game alone) is over, 1 of 6: lost");
	FGameState Late;
	M.StartGameSeeded(Wall, 4, Late);
	RunUntil(M, Late, 10.0, [&]() { return Late.bOver; });
	CHECK(Late.bOver && Late.Beat == EGameBeat::Late, "no press at all: too late");
	FGameState WGood;
	M.StartGameSeeded(Wall, 5, WGood);
	Play(M, WGood, true);
	CHECK(WGood.Result == EGameResult::Win && WGood.Hits == 6, "six returns against the wall: won (%d)", WGood.Hits);
	float RB, RZ0, RZ1, RI0, RI1;
	FGameState WBar;
	M.StartGameSeeded(Wall, 6, WBar);
	RunUntil(M, WBar, 10.0, [&]() { return WBar.Clock >= WBar.HitAt + 0.5 * WBar.Flight; });
	CHECK(M.GameBar(WBar, RB, RZ0, RZ1, RI0, RI1) && std::fabs(RB - 0.5f) < 0.05f && RZ0 > 0.6f && RZ1 == 1.f, "the rally bar: the ball halfway, the window before the hero");
	FGameState TtGame;
	M.StartGameSeeded(Tt, 11, TtGame);
	const bool bTtOver = Play(M, TtGame, true);
	CHECK(bTtOver && TtGame.Opponent >= 0 && M.Regulars[(size_t)TtGame.Opponent].Id == "bob" && (TtGame.Score == 3 || TtGame.OppScore == 3),
		"table tennis against bob ends at 3 (%d:%d)", TtGame.Score, TtGame.OppScore);
	FGameState TtBad;
	M.StartGameSeeded(Tt, 11, TtBad);
	Play(M, TtBad, false);
	CHECK(TtBad.Result == EGameResult::Lose && TtBad.OppScore == 3 && TtBad.Score == 0, "never returning: 0:3");

	// duel: the move that beats the opponent's wins the turn; reading the tell pays
	FGameState Du;
	M.StartGameSeeded(Chess, 21, Du);
	CHECK(Du.Opponent >= 0 && Du.OppMove >= 0 && Du.Tell >= 0, "a chess duel: an opponent of the club, a move and a tell");
	const int Opp0 = Du.OppMove;
	RunUntil(M, Du, 5.0, [&]() { return Du.Clock >= Du.Wait; });
	M.PressGame(Du, (Opp0 + 1) % 3);
	CHECK(Du.Score == CD->Moves[(size_t)((Opp0 + 1) % 3)].Power && Du.Beat == EGameBeat::TurnWon && Du.Round == 1 && Du.LastOpp == Opp0, "the move that beats theirs wins its power");
	RunUntil(M, Du, 5.0, [&]() { return Du.Clock >= Du.Wait; });
	const int Opp1 = Du.OppMove;
	M.PressGame(Du, (Opp1 + 2) % 3);
	CHECK(Du.OppScore == CD->Moves[(size_t)Opp1].Power && Du.Beat == EGameBeat::TurnLost, "the move theirs beats loses theirs");
	RunUntil(M, Du, 5.0, [&]() { return Du.Clock >= Du.Wait; });
	M.PressGame(Du, Du.OppMove);
	CHECK(Du.Beat == EGameBeat::TurnEven && Du.bOver && Du.Round == 3, "the same move: nobody; three turns and it is over");
	int Wins = 0, Reads = 0, Blind = 0;
	for (uint64_t Seed = 1; Seed <= 300; ++Seed)
	{
		FGameState S1, S2;
		M.StartGameSeeded(Chess, Seed, S1);
		M.StartGameSeeded(Chess, Seed, S2);
		Play(M, S1, true);
		Wins += S1.Result == EGameResult::Win ? 1 : 0;
		// reading the tell (the move that beats the one the look betrays) against always the first move
		for (int T = 0; T < 3 && !S2.bOver; ++T)
		{
			RunUntil(M, S2, 5.0, [&]() { return S2.Clock >= S2.Wait; });
			M.PressGame(S2, (S2.Tell + 1) % 3);
		}
		Reads += S2.Result == EGameResult::Win ? 1 : 0;
		FGameState S3;
		M.StartGameSeeded(Chess, Seed, S3);
		Play(M, S3, false);
		Blind += S3.Result == EGameResult::Win ? 1 : 0;
	}
	CHECK(Wins == 300 && Reads > Blind + 40, "knowing the move always wins; reading the tell wins more than playing blind (%d vs %d of 300)", Reads, Blind);

	// the end: result lines per thing, the stamp of the regular beaten, the prize, once
	FGameState Fin;
	M.StartGameSeeded(Chess, 21, Fin);
	M.WinGame(Fin);
	const std::string Beaten = M.Regulars[(size_t)Fin.Opponent].Id;
	FGameEnd E1 = M.FinishGame(Fin, At(2, 12.f, 60));
	CHECK(E1.Result == EGameResult::Win && E1.Line == "c.win" && E1.NewStamps.size() == 1 && E1.NewStamps[0] == "chess_wins/" + Beaten
		&& M.HasStamp("chess_wins/" + Beaten) && E1.Target == "chess" && !E1.Opponent.empty(), "a won duel: the win line, the stamp chess_wins/%s", Beaten.c_str());
	FGameEnd E2 = M.FinishGame(Fin, At(2, 12.f, 60));
	CHECK(E2.Result == EGameResult::None && E2.Line.empty(), "a finished game gives nothing twice");
	FGameState Fin2;
	M.StartGameSeeded(Chess, 21, Fin2);
	M.WinGame(Fin2);
	CHECK(M.FinishGame(Fin2, At(2, 12.f, 60)).NewStamps.empty() && M.CountStamps("chess_wins") == 1, "the same regular beaten again: no new stamp");
	FGameState DGame;
	M.StartGameSeeded(Darts, 5, DGame);
	M.WinGame(DGame);
	FGameEnd DE = M.FinishGame(DGame, At(2, 12.f, 60));
	CHECK(DE.Prize.size() == 1 && DE.Prize[0].first == "postcard" && DE.Line == "d.win.1" && M.HasStamp("darts_wins/dan"), "a won darts game: the prize and the stamp");
	FGameState DGame2;
	M.StartGameSeeded(Darts, 6, DGame2);
	M.WinGame(DGame2);
	CHECK(M.FinishGame(DGame2, At(2, 12.f, 60)).Line == "d.win.2", "the next win: the next win line");
	FGameState Lost;
	M.StartGameSeeded(Darts, 6, Lost);
	Play(M, Lost, false);
	FGameEnd LE = M.FinishGame(Lost, At(2, 12.f, 60));
	CHECK(LE.Result == EGameResult::Lose && LE.Line == "d.lose" && LE.Prize.empty(), "a lost game: the lose line, no prize");
	FGameState Park;
	M.StartGameSeeded(ParkChess, 1, Park);
	CHECK(Park.Opponent >= 0, "the park's table finds a player from elsewhere");
	FGameState None;
	CHECK(!M.StartGameSeeded(M.FindThing("P-CLUB/switch"), 1, None) && None.Def < 0, "a thing without a game starts none");

	// the started game of Run's seed differs per run
	FGameState G1, G2;
	M.StartGame(Darts, At(2, 12.f, 70), G1);
	M.Run(Darts, At(2, 12.f, 70), Ask);
	M.StartGame(Darts, At(2, 12.f, 70), G2);
	CHECK(G1.Seed != G2.Seed, "every game of a thing its own seed (the day and the runs)");

	// save: today's games are counted with the rest
	std::map<std::string, std::string> Saved;
	M.SaveState(Saved);
	CHECK(Saved.count("t:#g:darts") == 1 && Saved.count("c:chess_wins/" + Beaten) == 1, "the save keeps today's games and the regulars beaten");

	// light switches and clocks
	CHECK(FModel::SwitchRadiusCm(M.Things[(size_t)M.FindThing("P-CLUB/switch")]) == 1500.0 && FModel::SwitchRadiusCm(M.Things[(size_t)M.FindThing("P-CLUB/switch2")]) == 700.0
		&& FModel::SwitchRadiusCm(M.Things[(size_t)Darts]) == 0.0, "a switch's reach: hall 15 m, a bare switch its room (7 m), darts none");
	CHECK(FillTime("На часах {time}.", 16.6667f) == "На часах 16:40." && FillTime("{time} / {time}", 0.f) == "00:00 / 00:00" && FillTime("no clock", 3.f) == "no clock"
		&& FillTime("{time}", 23.999f) == "23:59", "{time}: the game's clock, every one of them");
}

/** Every game of the real data plays to its end, well and badly, and has words for every result at every hour. */
static void RealGameChecks(const FModel& M)
{
	int Bad = 0, NoEnd = 0, NoOpp = 0, NoLines = 0, Played = 0;
	std::string First;
	for (int I = 0; I < (int)M.Things.size(); ++I)
	{
		const FGameDef* D = M.GameOfThing(I);
		if (!D)
		{
			continue;
		}
		++Played;
		for (int R = 0; R < 3; ++R)
		{
			for (int H = 0; H < 24; ++H)
			{
				if (FModel::CountHolding(D->Results[R], At(H % 7, (float)H + 0.5f)) == 0)
				{
					++NoLines;
					First = First.empty() ? M.Things[(size_t)I].Id : First;
					break;
				}
			}
		}
		int Pool[32];
		if (!D->Vs.empty() && M.Opponents(*D, M.Things[(size_t)I].Plot, Pool, 32) == 0)
		{
			++NoOpp;
		}
		for (int Good = 0; Good < 2; ++Good)
		{
			FGameState G;
			if (!M.StartGameSeeded(I, 1000 + (uint64_t)I, G))
			{
				++Bad;
				continue;
			}
			if (D->Type == EGameType::Duel || Good)
			{
				NoEnd += Play(M, G, Good != 0) ? 0 : 1;
			}
			else
			{
				// no press at all: an aim or a rally ends by itself
				RunUntil(M, G, 200.0, [&]() { return G.bOver; });
				NoEnd += G.bOver ? 0 : 1;
			}
			if (Good && D->Type != EGameType::Rally && G.Result == EGameResult::Lose)
			{
				++Bad;
				First = First.empty() ? M.Things[(size_t)I].Id : First;
			}
		}
	}
	std::printf("  games on %d things, %d regulars\n", Played, (int)M.Regulars.size());
	CHECK(NoEnd == 0, "every game ends, played well or not at all (%d do not)", NoEnd);
	CHECK(Bad == 0, "playing perfectly never loses an aim or a duel (%d do, first %s)", Bad, First.c_str());
	CHECK(NoOpp == 0, "every game against somebody finds a regular where it stands (%d do not)", NoOpp);
	CHECK(NoLines == 0, "every game has a win, a lose and a draw line at every hour (%d gaps, first %s)", NoLines, First.c_str());
}

static void StayChecks()
{
	std::printf("stays (a sanatorium's voucher: stay lines, needs_stay, the curfew, the save)\n");
	const std::string Text = R"({"things": [
  {"id": "P-S/desk", "plot": "P-S", "kind": "counter", "ue": [0, 0, 100], "yaw": 0, "props": {}, "act": "voucher"},
  {"id": "P-S/board", "plot": "P-S", "kind": "board", "ue": [400, 0, 150], "yaw": 0, "props": {}, "act": "regime"},
  {"id": "P-S/lamp", "plot": "P-S", "kind": "machine", "ue": [800, 0, 120], "yaw": 0, "props": {}, "act": "quartz"},
  {"id": "P-S/bed", "plot": "P-S", "kind": "bed", "ue": [1200, 0, 50], "yaw": 0, "props": {}, "act": "sleep"},
  {"id": "P-S/bell", "plot": "P-S", "kind": "counter", "ue": [1600, 0, 100], "yaw": 0, "props": {}, "act": "bell"},
  {"id": "P-S/window", "plot": "P-S", "kind": "counter", "ue": [2000, 0, 100], "yaw": 0, "props": {}, "act": "extend"},
  {"id": "P-T/board", "plot": "P-T", "kind": "board", "ue": [9000, 0, 150], "yaw": 0, "props": {}, "act": "regime"}],
 "acts": {
  "voucher": {"prompt": "p", "price": 60, "lines": ["v.1"], "stay": {"id": "san", "days": 3}},
  "regime": {"prompt": "p", "lines": [{"k": "r.guest", "stay": "!san"}, {"k": "r.mine", "stay": "san"}]},
  "quartz": {"prompt": "p", "lines": [{"k": "q.1", "collect": "cure/quartz"}], "needs_stay": "san", "no_stay": "_closed:q.no"},
  "_closed:q.no": {"prompt": "p", "lines": ["q.no"], "event": "look", "target": "closed", "any_hour": true},
  "_closed:nurse": {"prompt": "p", "lines": ["nurse.1"], "event": "look", "target": "closed", "any_hour": true},
  "sleep": {"prompt": "p", "minutes": 480, "until": 7, "pose": "lie", "any_hour": true, "lines": ["s.1"]},
  "bell": {"prompt": "p", "lines": ["b.1"]},
  "extend": {"prompt": "p", "price": 40, "daily": 1, "lines": ["e.1"], "stay": {"id": "san", "days": 4}, "needs_stay": "san", "no_stay": "voucher"}},
 "collections": {"cure": {"title": "t", "pieces": ["quartz"]}},
 "stays": {"san": {"plot": "P-S", "curfew": [23, 6], "act": "_closed:nurse", "allow": ["bell"]}},
 "shops": {}})";
	FModel M;
	std::string Error;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	FAsk Ask;
	Ask.Money = 100;
	const int Desk = M.FindThing("P-S/desk"), Board = M.FindThing("P-S/board"), Lamp = M.FindThing("P-S/lamp");
	const int Bed = M.FindThing("P-S/bed"), Bell = M.FindThing("P-S/bell"), Other = M.FindThing("P-T/board");
	CHECK(M.Stays.size() == 1 && M.Stays[0].Plot == "P-S" && M.Stays[0].CurfewAct == M.FindAct("_closed:nurse") && M.Stays[0].Allow.count("bell"),
		"the stay reads: plot, curfew act, allowed acts");
	CHECK(M.Acts[M.Things[Desk].Act].StayDays == 3 && M.Acts[M.Things[Lamp].Act].NoStayAct == M.FindAct("_closed:q.no"), "stay and no_stay read");
	// no voucher
	CHECK(M.Run(Board, AtDate(0, 10.f, 100, 0, 5), Ask).Line == "r.guest", "without a voucher the board shows the guest's line");
	const FOutcome NoV = M.Run(Lamp, AtDate(0, 10.f, 100, 0, 5), Ask);
	CHECK(NoV.Verdict == EVerdict::Ok && NoV.bClosed && NoV.Line == "q.no" && NoV.NewStamps.empty(), "the lamp without a voucher: words only, no stamp");
	CHECK(M.Run(Bell, AtDate(0, 23.5f, 100, 0, 5), Ask).Line == "b.1" && M.Run(Board, AtDate(0, 23.5f, 100, 0, 5), Ask).Line == "r.guest",
		"no curfew without a voucher");
	// buy one on day 5: days 5, 6, 7
	const FOutcome Buy = M.Run(Desk, AtDate(0, 10.f, 100, 0, 5), Ask);
	CHECK(Buy.Verdict == EVerdict::Ok && Buy.Price == 60 && M.OnStay("san", 5) && M.OnStay("san", 7) && !M.OnStay("san", 8) && M.StayDaysLeft("san", 5) == 3,
		"a voucher for 3 days: on it days 5..7 (%d left)", M.StayDaysLeft("san", 5));
	CHECK(M.Run(Board, AtDate(0, 10.f, 100, 0, 5), Ask).Line == "r.mine", "with it the board shows his own line");
	const FOutcome Q = M.Run(Lamp, AtDate(0, 10.f, 100, 0, 6), Ask);
	CHECK(Q.Verdict == EVerdict::Ok && !Q.bClosed && Q.Line == "q.1" && Q.NewStamps.size() == 1, "the lamp with it: the procedure and its stamp");
	// the curfew
	const FOutcome Cf = M.Run(Board, AtDate(1, 23.5f, 101, 0, 6), Ask);
	CHECK(Cf.bClosed && Cf.Line == "nurse.1", "after 23 on a voucher: the duty nurse's words at the board");
	CHECK(M.Run(Board, AtDate(1, 5.f, 101, 0, 6), Ask).Line == "nurse.1" && M.Run(Board, AtDate(1, 6.f, 101, 0, 6), Ask).Line == "r.mine",
		"till 6, then the day again");
	CHECK(!M.Run(Bed, AtDate(1, 23.5f, 101, 0, 6), Ask).bClosed && M.Run(Bell, AtDate(1, 23.5f, 101, 0, 6), Ask).Line == "b.1",
		"the bed and the allowed bell keep their acts in the curfew");
	CHECK(!M.Run(Other, AtDate(1, 23.5f, 101, 0, 6), Ask).bClosed, "another plot keeps no curfew");
	FAsk All = Ask;
	All.bOpenAll = true;
	CHECK(!M.Run(Board, AtDate(1, 23.5f, 101, 0, 6), All).bClosed, "developer mode's open-all lifts the curfew");
	// it ends; a second voucher adds days
	CHECK(M.Run(Lamp, AtDate(3, 10.f, 103, 0, 8), Ask).Line == "q.no" && M.Run(Board, AtDate(3, 23.5f, 103, 0, 8), Ask).Line == "r.guest",
		"day 8: the voucher is over, no procedures, no curfew");
	M.StartStay("san", 3, 6);
	CHECK(M.StayDaysLeft("san", 6) == 5 && M.OnStay("san", 10) && !M.OnStay("san", 11), "a voucher bought on one adds its days (%d left)",
		M.StayDaysLeft("san", 6));
	// a real act for those without the stay: the window sells the voucher, then extends it
	FModel M3;
	M3.Load(Text, Error);
	const int Window = M3.FindThing("P-S/window");
	CHECK(M3.Check(Window, AtDate(0, 10.f, 100, 0, 5), Ask) == EVerdict::Ok, "the window without a voucher: its no_stay act may run");
	const FOutcome Sold = M3.Run(Window, AtDate(0, 10.f, 100, 0, 5), Ask);
	CHECK(Sold.Verdict == EVerdict::Ok && !Sold.bClosed && Sold.Price == 60 && Sold.Line == "v.1" && M3.StayDaysLeft("san", 5) == 3,
		"a real no_stay act runs in full: 60 roubles, 3 days");
	const FOutcome Ext = M3.Run(Window, AtDate(0, 11.f, 100, 0, 5), Ask);
	CHECK(Ext.Price == 40 && Ext.Line == "e.1" && M3.StayDaysLeft("san", 5) == 7, "on it the window extends it by 4: 7 days (%d)", M3.StayDaysLeft("san", 5));
	FAsk Poor = Ask;
	Poor.Money = 10;
	FModel M4;
	M4.Load(Text, Error);
	CHECK(M4.Check(Window, AtDate(0, 10.f, 100, 0, 5), Poor) == EVerdict::NoMoney && M4.Run(Window, AtDate(0, 10.f, 100, 0, 5), Poor).NeedMoney == 60,
		"the voucher's own price is asked (no money: refused, 60 needed)");
	CHECK(M3.Check(Window, AtDate(0, 12.f, 100, 0, 5), Ask) == EVerdict::DailyDone, "the extension's own daily limit");
	// the save
	std::map<std::string, std::string> Saved;
	M.SaveState(Saved);
	FModel M2;
	M2.Load(Text, Error);
	M2.LoadState(Saved);
	CHECK(Saved.count("s:san") && M2.OnStay("san", 10) && !M2.OnStay("san", 11), "the stay is saved (s:san = %s)", Saved.count("s:san") ? Saved["s:san"].c_str() : "-");
	M2.StartStay("san", 0, 6);
	CHECK(!M2.OnStay("san", 6) && M2.GetStays().empty(), "0 days ends it");
}

/** The sanatorium's course of cure on the real data (tools/city/content/x_p_sanatorium.json, docs/city_interiors.md "Stays"):
 *  the voucher at the cloakroom, the nine stamps at their hours, the curfew, the extension, the discharge topic. */
static void CourseChecks(const std::string& Path)
{
	std::string Text, Error;
	if (!ReadFile(Path, Text))
	{
		return;
	}
	std::printf("the sanatorium's course of cure (real data)\n");
	FModel M;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	const int Window = M.FindThing("P-SANATORIUM/cloakroom"), Pump = M.FindThing("P-SANATORIUM/pump_room");
	const int Ex = M.FindThing("P-SANATORIUM/lobby_exercise"), Quartz = M.FindThing("P-SANATORIUM/treat_quartz");
	const int Oxy = M.FindThing("P-SANATORIUM/oxygen_bar"), Walk = M.FindThing("P-SANATORIUM/treat_chart");
	const int Couch = M.FindThing("P-SANATORIUM/couch_a"), Night = M.FindThing("P-SANATORIUM/couch_b");
	const int Dance = M.FindThing("P-SANATORIUM/lobby_dance"), Board = M.FindThing("P-SANATORIUM/regime_board");
	const int Book = M.FindThing("P-SANATORIUM/treat_cabinet"), Table = M.FindThing("P-SANATORIUM/dining_1_b");
	const int Bell = M.FindThing("P-SANATORIUM/night_bell"), Desk = M.FindThing("P-SANATORIUM/treat_desk");
	if (!(Window >= 0 && Pump >= 0 && Ex >= 0 && Quartz >= 0 && Oxy >= 0 && Walk >= 0 && Couch >= 0 && Night >= 0 && Dance >= 0 && Board >= 0
		&& Book >= 0 && Table >= 0 && Bell >= 0 && Desk >= 0))
	{
		CHECK(false, "the sanatorium's things are there");
		return;
	}
	CHECK(M.FindStayDef("san") && M.FindStayDef("san")->CurfewAct >= 0, "the stay san with its curfew");
	FAsk Ask;
	Ask.Money = 1000;
	Ask.StaffOnShift = [](const std::string&, const std::string&) { return true; };
	// Monday (weekday 0) is day 10 since the game began
	auto Day = [](int Since, float Hour) { FWhen W = AtDate((int)(Since - 10) % 7, Hour, 1000 + Since, 0, Since); W.Weather = 0x40; return W; };
	const FOutcome Paid = M.Run(Night, Day(10, 22.f), Ask);
	CHECK(Paid.Verdict == EVerdict::Ok && Paid.Price > 0 && Paid.bSleep, "without a voucher the couch is the paid night (%d roubles)", Paid.Price);
	M.Run(Quartz, Day(10, 10.f), Ask);
	CHECK(M.CountStamps("san_course") == 0, "no voucher: the quartz lamp gives no course stamp");
	const FOutcome V = M.Run(Window, Day(10, 10.f), Ask);
	CHECK(V.Verdict == EVerdict::Ok && V.Target == "san_voucher" && V.Price == 90 && M.StayDaysLeft("san", 10) == 3,
		"the cloakroom window sells a 3-day voucher (%s, %d roubles, %d days)", V.Target.c_str(), V.Price, M.StayDaysLeft("san", 10));
	const FOutcome V2 = M.Run(Window, Day(10, 10.5f), Ask);
	CHECK(V2.Target == "san_extend" && V2.Price == 100 && M.StayDaysLeft("san", 10) == 7, "on it the window extends it to a week (%d days)",
		M.StayDaysLeft("san", 10));
	CHECK(M.Run(Board, Day(10, 10.f), Ask).Line == "city.san.regime.my.2", "the regime board shows the guest's own day");
	// day 11 (Tuesday): the whole course
	std::vector<std::string> Got;
	auto Take = [&Got](const std::vector<std::string>& Stamps) { for (const std::string& S : Stamps) { if (S.rfind("san_course/", 0) == 0) Got.push_back(S); } };
	FOutcome E = M.Run(Ex, Day(11, 7.5f), Ask);
	FGameState G;
	bool bWon = false;
	if (E.Game >= 0 && M.StartGame(Ex, Day(11, 7.5f), G) && Play(M, G, true))
	{
		const FGameEnd End = M.FinishGame(G, Day(11, 7.6f));
		bWon = End.Result == EGameResult::Win;
		Take(End.NewStamps);
	}
	CHECK(bWon && M.HasStamp("san_course/exercise"), "the morning exercises: a won rally stamps the book");
	Take(M.Run(Pump, Day(11, 8.f), Ask).NewStamps);
	Take(M.Run(Pump, Day(11, 13.f), Ask).NewStamps);
	const FOutcome Off = M.Run(Pump, Day(11, 15.f), Ask);
	CHECK(Off.Line == "city.san.water.late2" && Off.NewStamps.empty(), "water off the hour: no drop in the book");
	Take(M.Run(Pump, Day(11, 18.f), Ask).NewStamps);
	CHECK(M.HasStamp("san_course/water_8") && M.HasStamp("san_course/water_13") && M.HasStamp("san_course/water_18"), "three springs at 8, 13 and 18");
	Take(M.Run(Quartz, Day(11, 10.f), Ask).NewStamps);
	Take(M.Run(Oxy, Day(11, 11.f), Ask).NewStamps);
	Take(M.Run(Walk, Day(11, 11.5f), Ask).NewStamps);
	const FOutcome Q0 = M.Run(Couch, Day(11, 12.f), Ask);
	CHECK(Q0.Line == "city.san.quietbed.off" && Q0.NewStamps.empty(), "lying down before quiet hour: no stamp");
	Take(M.Run(Couch, Day(11, 14.5f), Ask).NewStamps);
	const FOutcome D0 = M.Run(Dance, Day(11, 17.f), Ask);
	CHECK(D0.Line == "city.san.evening.course.notyet" && D0.NewStamps.empty(), "the lobby before seven: no dance yet");
	Take(M.Run(Dance, Day(11, 20.f), Ask).NewStamps);
	CHECK(M.CountStamps("san_course") == 9 && Got.size() == 9, "a full book in one day: %d stamps (%d new)", M.CountStamps("san_course"), (int)Got.size());
	// the book, the doctor
	const FOutcome B = M.Run(Book, Day(11, 16.f), Ask);
	CHECK(B.Text >= 0 && M.Texts[(size_t)B.Text].Id == "san_course_book", "the card index opens the health-resort book");
	const int Discharge = M.FindTopic("sand.discharge");
	CHECK(Discharge >= 0 && M.TopicOffered(Discharge, Day(11, 16.f)), "with nine stamps the doctor offers the discharge");
	// the curfew
	const FOutcome Cf = M.Run(Board, Day(11, 23.5f), Ask);
	CHECK(Cf.bClosed && Cf.Line.rfind("city.san.curfew.", 0) == 0, "after 23 the duty nurse sends him to bed (%s)", Cf.Line.c_str());
	CHECK(!M.Run(Bell, Day(11, 23.5f), Ask).bClosed, "the night bell still answers");
	const FOutcome Bed = M.Run(Night, Day(11, 23.5f), Ask);
	CHECK(Bed.Verdict == EVerdict::Ok && Bed.Price == 0 && Bed.bSleep, "on the voucher the couch's night is free and he chooses the hour");
	// the stories at the window table: one a day, in order
	const FOutcome T1 = M.Run(Table, Day(11, 9.f), Ask), T1b = M.Run(Table, Day(11, 13.f), Ask), T2 = M.Run(Table, Day(12, 9.f), Ask);
	CHECK(T1.Line == "city.san.tg.1" && T1b.Line == "city.san.tg.1" && T2.Line == "city.san.tg.2", "the window table's story: a day a part (%s %s %s)",
		T1.Line.c_str(), T1b.Line.c_str(), T2.Line.c_str());
	std::string Last;
	for (int D = 13; D < 22; ++D)
	{
		Last = M.Run(Table, Day(D, 13.f), Ask).Line;
	}
	CHECK(Last == "city.san.tg.after", "after the nine parts the table stays as they left it (%s)", Last.c_str());
	// the voucher ends on day 17 (10 + 7)
	CHECK(M.OnStay("san", 16) && !M.OnStay("san", 17) && !M.Run(Board, Day(17, 23.5f), Ask).bClosed, "a week later the voucher is over: no curfew");
	(void)Desk;
}

// The sanatorium's procedures as scenes (docs/sanatorium_procedures.md): played with scripted keys, a careful hand gets the good
// result and its collection piece, a careless one the poor words; the guest at it speaks by name; the doctor's numbers get
// better over the course; without the voucher the treatment rooms only say their words.
enum class EProcPlay { Good, Poor, Away };

static FProcEnd PlayProc(FModel& M, int Thing, const FWhen& When, const FAsk& Ask, EProcPlay How, const std::vector<FProcCompanion>& With,
	std::vector<std::string>* Lines = nullptr)
{
	FProcState G;
	if (!M.StartProc(Thing, When, With, G))
	{
		return FProcEnd();
	}
	const FProcDef* D = M.ProcDef(G.Def);
	const double Dt = 1.0 / 60.0;
	int LastStep = -1;
	for (int Frame = 0; Frame < 60 * 400 && !G.bOver; ++Frame)
	{
		const FProcStep& S = D->Steps[(size_t)G.Step];
		if (G.Step != LastStep)
		{
			LastStep = G.Step;
			if (Lines && !G.Line.empty())
			{
				Lines->push_back(G.Line);
			}
		}
		const double T = G.Clock - G.StepFrom;
		const bool bGood = How == EProcPlay::Good;
		bool bDown = false;
		if (!G.bSettled)
		{
			switch (S.Do)
			{
			case EProcStep::Pick:
				M.PressProc(G, 1);
				break;
			case EProcStep::Pour:
				// the careful hand lets go in the middle of the mark; the careless one holds on over the brim
				bDown = bGood ? G.Level < (S.MarkLo + S.MarkHi) * 0.5 : true;
				break;
			case EProcStep::Sip:
			{
				const double K = (T - FModel::ProcLead) / S.Every;
				if (bGood && K > -0.01 && std::fabs(K - std::floor(K + 0.5)) * S.Every < Dt * 0.6)
				{
					M.PressProc(G, -1);
				}
				else if (!bGood && Frame == 1)
				{
					M.PressProc(G, -1);     // one press long before the clock, then he forgets
				}
				break;
			}
			case EProcStep::Breathe:
				if (bGood && T >= FModel::ProcLead)
				{
					bDown = std::fmod(T - FModel::ProcLead, S.In + S.Out) < S.In;
				}
				break;
			case EProcStep::Tap:
				if (bGood && Frame % 6 == 0)
				{
					M.PressProc(G, -1);
				}
				break;
			default:
				if (T > 1.0)
				{
					M.PressProc(G, -1);     // E past the line
				}
				break;
			}
		}
		M.TickProc(G, Dt, bDown);
		if (How == EProcPlay::Away && G.Step >= 1)
		{
			return FProcEnd();     // walked off: the game drops the state, nothing is given
		}
	}
	if (!G.bOver)
	{
		return FProcEnd();
	}
	return M.FinishProc(G, When, Ask);
}

static void ProcChecks(const std::string& Path)
{
	std::string Text, Error;
	if (!ReadFile(Path, Text))
	{
		return;
	}
	std::printf("the sanatorium's procedures as scenes (real data)\n");
	FModel M;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	const char* Ids[] = { "pump_room", "inhale_1", "hydro_mud", "hydro_charcot", "massage_table", "treat_checkup", "lobby_dance",
		"lounge_projector", "cloakroom" };
	int T[9];
	bool bAll = true;
	for (int I = 0; I < 9; ++I)
	{
		T[I] = M.FindThing(std::string("P-SANATORIUM/") + Ids[I]);
		if (T[I] < 0)
		{
			std::printf("  (no P-SANATORIUM/%s)\n", Ids[I]);
			bAll = false;
		}
	}
	if (!bAll)
	{
		CHECK(false, "the treatment rooms' things are there");
		return;
	}
	const int Pump = T[0], Inhale = T[1], Mud = T[2], Charcot = T[3], Massage = T[4], Doctor = T[5], Dance = T[6], Projector = T[7], Window = T[8];
	FAsk Ask;
	Ask.Money = 1000;
	Ask.StaffOnShift = [](const std::string&, const std::string&) { return true; };
	auto Day = [](int Since, float Hour) { FWhen W = AtDate((int)(Since - 10) % 7, Hour, 1000 + Since, 0, Since); W.Weather = 0x40; return W; };
	const std::vector<FProcCompanion> Nobody;
	const std::vector<FProcCompanion> Gromov = { { "gromov", "npc.t_gromov_pyotr.name" } };
	// no voucher: the plain act (the words for a passer-by), no scene
	CHECK(M.ProcNow(Pump, Day(10, 8.f), Ask) < 0, "no voucher: the pump room's cup is no scene");
	const FOutcome NoStay = M.Run(Inhale, Day(10, 10.f), Ask);
	CHECK(M.ProcNow(Inhale, Day(10, 10.f), Ask) < 0 && NoStay.Line.rfind("city.san.p.inh.nostay.", 0) == 0,
		"no voucher: the inhaler says its words (%s)", NoStay.Line.c_str());
	M.Run(Window, Day(10, 9.f), Ask);
	M.Run(Window, Day(10, 9.5f), Ask);
	CHECK(M.OnStay("san", 10), "on a voucher now");
	// every procedure has its scene on the voucher at its hours
	const int Procs[] = { Pump, Inhale, Mud, Charcot, Massage, Doctor };
	int Offered = 0;
	for (const int P : Procs)
	{
		Offered += M.ProcNow(P, Day(10, 10.f), Ask) >= 0 ? 1 : 0;
	}
	CHECK(Offered == 6, "the six procedures play as scenes on the voucher (%d)", Offered);
	CHECK(M.ProcNow(Dance, Day(10, 17.f), Ask) < 0 && M.ProcNow(Dance, Day(10, 20.f), Ask) >= 0, "the waltz is a scene only in the evening");
	// the pump room: the cup picked, filled to the mark, sipped on the clock's ticks
	std::vector<std::string> Seen;
	const FProcEnd PumpGood = PlayProc(M, Pump, Day(10, 13.f), Ask, EProcPlay::Good, Nobody, &Seen);
	CHECK(PumpGood.Result == 0 && PumpGood.Line == "city.san.p.pump.good.1", "a careful hand at the spring: the good words (%d %s)", PumpGood.Result,
		PumpGood.Line.c_str());
	CHECK(std::find(Seen.begin(), Seen.end(), "city.san.p.pump.pour.2") != Seen.end(), "the second cup picked: its own pouring words");
	CHECK(PumpGood.Run.Verdict == EVerdict::Ok && M.HasStamp("san_course/water_13"), "the act runs after the scene: the 13 o'clock drop in the book");
	const FProcEnd PumpPoor = PlayProc(M, Pump, Day(10, 18.f), Ask, EProcPlay::Poor, Nobody);
	CHECK(PumpPoor.Result == 2 && PumpPoor.Line == "city.san.p.pump.poor.1", "spilt and gulped: the poor words (%d)", PumpPoor.Result);
	// the inhaler: poor first (no stamp), good next day with Gromov in the next chair
	const FProcEnd InhPoor = PlayProc(M, Inhale, Day(10, 10.f), Ask, EProcPlay::Poor, Nobody);
	CHECK(InhPoor.Result == 2 && !M.HasStamp("san_proc/inhale"), "breathing out of time: poor, no piece for the collection");
	Seen.clear();
	const FProcEnd InhGood = PlayProc(M, Inhale, Day(11, 10.f), Ask, EProcPlay::Good, Gromov, &Seen);
	CHECK(InhGood.Result == 0 && M.HasStamp("san_proc/inhale"), "breathing with the ring: good, the piece san_proc/inhale");
	CHECK(std::find(Seen.begin(), Seen.end(), "city.san.p.inh.guest.gromov") != Seen.end(), "Gromov in the next chair says his own word");
	CHECK(std::find(Seen.begin(), Seen.end(), "city.san.p.inh.herb.tue") != Seen.end(), "Tuesday's herb in the inhaler");
	Seen.clear();
	PlayProc(M, Inhale, Day(12, 10.f), Ask, EProcPlay::Good, Nobody, &Seen);
	bool bGuest = false;
	for (const std::string& L : Seen)
	{
		bGuest |= L.rfind("city.san.p.inh.guest.", 0) == 0;
	}
	CHECK(!bGuest, "alone in the room: no guest's word");
	// walked off: nothing counted, nothing run
	const int Before = M.ProcCount("san_inhale");
	PlayProc(M, Inhale, Day(13, 10.f), Ask, EProcPlay::Away, Nobody);
	CHECK(M.ProcCount("san_inhale") == Before, "walked off mid-scene: not counted");
	// mud, Charcot, massage: the good result's pieces; the cuts' minutes come off the act's
	const FProcEnd MudGood = PlayProc(M, Mud, Day(11, 11.f), Ask, EProcPlay::Good, Gromov);
	const FProcEnd ChGood = PlayProc(M, Charcot, Day(11, 12.f), Ask, EProcPlay::Good, Nobody);
	const FProcEnd MasGood = PlayProc(M, Massage, Day(11, 15.f), Ask, EProcPlay::Good, Nobody);
	CHECK(MudGood.Result == 0 && ChGood.Result == 0 && MasGood.Result == 0, "mud, Charcot shower and massage done well (%d %d %d)", MudGood.Result,
		ChGood.Result, MasGood.Result);
	CHECK(M.HasStamp("san_proc/mud") && M.HasStamp("san_proc/charcot") && M.HasStamp("san_proc/massage"), "their pieces of the collection");
	CHECK(MudGood.Run.Minutes < M.Acts[(size_t)M.Things[(size_t)Mud].Act].Minutes, "the mud's cuts pass part of its half hour (%d left)",
		MudGood.Run.Minutes);
	// the doctor: the numbers on the card get better over the course
	const FProcEnd Doc1 = PlayProc(M, Doctor, Day(11, 10.f), Ask, EProcPlay::Good, Nobody);
	CHECK(Doc1.Vitals.Valid() && Doc1.Args.size() == 10, "the doctor's card: weight, pressure, pulse, lungs (%.1f kg %d/%d %d %d ml)",
		Doc1.Vitals.Weight, Doc1.Vitals.Sys, Doc1.Vitals.Dia, Doc1.Vitals.Pulse, Doc1.Vitals.Lungs);
	for (int D = 12; D < 16; ++D)
	{
		PlayProc(M, Pump, Day(D, 8.f), Ask, EProcPlay::Good, Nobody);
		PlayProc(M, Inhale, Day(D, 10.f), Ask, EProcPlay::Good, Nobody);
		PlayProc(M, Massage, Day(D, 15.f), Ask, EProcPlay::Good, Nobody);
	}
	const FProcEnd Doc2 = PlayProc(M, Doctor, Day(16, 10.f), Ask, EProcPlay::Good, Nobody);
	CHECK(Doc2.Vitals.Valid() && Doc2.Vitals.Sys < Doc1.Vitals.Sys && Doc2.Vitals.Pulse < Doc1.Vitals.Pulse && Doc2.Vitals.Lungs > Doc1.Vitals.Lungs,
		"five days on: pressure %d -> %d, pulse %d -> %d, lungs %d -> %d", Doc1.Vitals.Sys, Doc2.Vitals.Sys, Doc1.Vitals.Pulse, Doc2.Vitals.Pulse,
		Doc1.Vitals.Lungs, Doc2.Vitals.Lungs);
	CHECK(Doc2.First.Sys == Doc1.Vitals.Sys && M.FirstVitals().Sys == Doc1.Vitals.Sys, "the card keeps the first numbers to compare");
	CHECK(M.HasStamp("san_proc/checkup"), "the checkup's piece");
	CHECK(M.Collections.size() > 0 && M.CountStamps("san_proc") == 5, "the procedures' collection complete (%d of 5)", M.CountStamps("san_proc"));
	// saved and loaded: the counts and the first numbers stay
	FModel M2;
	std::map<std::string, std::string> Saved;
	M.SaveState(Saved);
	M2.Load(Text, Error);
	M2.LoadState(Saved);
	CHECK(M2.ProcCount("san_inhale") == M.ProcCount("san_inhale") && M2.FirstVitals().Sys == Doc1.Vitals.Sys, "saved: %d inhalations, first pressure %d",
		M2.ProcCount("san_inhale"), M2.FirstVitals().Sys);
	// the waltz in the evening with a guest; the film night in the lounge on its evenings
	Seen.clear();
	const std::vector<FProcCompanion> Sonya = { { "sonya", "npc.t_mironova_sonya.name" } };
	const FProcEnd Waltz = PlayProc(M, Dance, Day(13, 20.f), Ask, EProcPlay::Good, Sonya, &Seen);
	CHECK(Waltz.Result == 0 && std::find(Seen.begin(), Seen.end(), "city.san.p.dance.guest.sonya") != Seen.end(), "the waltz with Sonya (%d)", Waltz.Result);
	const FOutcome FilmWed = M.Run(Projector, Day(12, 21.f), Ask), FilmMon = M.Run(Projector, Day(10, 21.f), Ask);
	CHECK(!FilmWed.Screen.empty() && FilmMon.Screen.empty(), "the lounge's film night: Wednesday's yes, Monday's no (%s / %s)", FilmWed.Line.c_str(),
		FilmMon.Line.c_str());
}

// The small jobs on the real data (docs/city_interiors.md "Small jobs"): each job pays once a day by its result inside its
// hours and days, gives its page of the work record book, and outside them the thing only says its old words, unpaid.
static void JobChecks(const std::string& Path)
{
	std::string Text, Error;
	if (!ReadFile(Path, Text))
	{
		return;
	}
	std::printf("the small jobs (real data)\n");
	FModel M;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	const int Board = M.FindThing("P-T2H2/hall_duty_board"), Youth = M.FindThing("P-UNITE/game_rules");
	const int Papers = M.FindThing("P-KIOSK1/price_board"), Kiosk = M.FindThing("P-KIOSK1/counter");
	const int Boxes = M.FindThing("P-MARKET/market_scales_2"), Scales = M.FindThing("P-MARKET/market_scales");
	const int Flags = M.FindThing("P-LIFEGUARD1/flag_halyard"), Whistle = M.FindThing("P-LIFEGUARD1/whistle");
	const int Watch = M.FindThing("P-PIER/keeper_clockwork"), Scoops = M.FindThing("P-CAFE/cake_vitrine");
	const int Door = M.FindThing("P-CLUB2/ticket_stubs");
	if (!(Board >= 0 && Youth >= 0 && Papers >= 0 && Kiosk >= 0 && Boxes >= 0 && Scales >= 0 && Flags >= 0 && Whistle >= 0 && Watch >= 0
		&& Scoops >= 0 && Door >= 0))
	{
		CHECK(false, "the job things are there");
		return;
	}
	FAsk Ask;
	Ask.Money = 0;     // a job needs no money
	Ask.StaffOnShift = [](const std::string&, const std::string&) { return true; };
	// Monday (weekday 0) is day 10 since the game began; weather bits: rain 1, snow 2, fog 4, cold 8, clear 32, wet 64, dry 128
	auto Day = [](int Since, float Hour, int Wx) { FWhen W = AtDate((int)(Since - 10) % 7, Hour, 2000 + Since, 0, Since); W.Weather = (uint8_t)Wx; return W; };
	const int Dry = 128 | 32, Rain = 1 | 64, Fog = 4 | 128, ColdDry = 8 | 128;
	int Earned = 0;
	// the boards
	const FOutcome B = M.Run(Board, Day(10, 12.f, Dry), Ask);
	CHECK(B.Text >= 0 && M.Texts[(size_t)B.Text].Id == "jobs_board_cm" && B.Target == "job_board", "the house committee's board opens \"Help wanted\"");
	const FOutcome Y = M.Run(Youth, Day(10, 12.f, Dry), Ask);
	CHECK(Y.Text >= 0 && M.Texts[(size_t)Y.Text].Id == "jobs_board_youth" && Y.Target == "job_board", "so does the games room's");
	// the kiosk's morning papers: Wednesday to Sunday, 8 to 10
	const FOutcome K = M.Run(Papers, Day(12, 8.5f, Dry), Ask);
	CHECK(K.Verdict == EVerdict::Ok && !K.bClosed && K.Pay == 25 && M.HasStamp("work_book/kiosk"), "the morning papers pay 25 on a dry morning (%d, %s)", K.Pay, K.Line.c_str());
	Earned += K.Pay;
	CHECK(M.Run(Papers, Day(12, 9.f, Dry), Ask).Verdict == EVerdict::DailyDone, "once a day");
	const FOutcome KR = M.Run(Papers, Day(13, 8.5f, Rain), Ask);
	CHECK(KR.Pay == 35 && KR.Line == "city.job.k1.rain", "in the rain more (%d, %s)", KR.Pay, KR.Line.c_str());
	const FOutcome KC = M.Run(Papers, Day(14, 8.5f, ColdDry), Ask);
	CHECK(KC.Pay == 30 && KC.Line == "city.job.k1.cold", "in the cold a little more (%d, %s)", KC.Pay, KC.Line.c_str());
	const FOutcome KD = M.Run(Papers, Day(15, 8.5f, 0), Ask);
	CHECK(KD.Pay == 25 && KD.Line == "city.job.k1.drizzle", "unknown weather: the drizzle line (%s)", KD.Line.c_str());
	const FOutcome KL = M.Run(Papers, Day(16, 12.f, Dry), Ask);
	CHECK(KL.bClosed && KL.Pay == 0 && KL.Line.rfind("city.story.kiosk1.prices", 0) == 0, "at noon the board shows its prices again, unpaid (%s)", KL.Line.c_str());
	CHECK(M.Run(Papers, Day(17, 8.5f, Dry), Ask).bClosed, "not on Mondays (Yerlan's day off)");
	const int KioskTalk = M.FindTopic("job.kiosk");
	CHECK(KioskTalk >= 0 && M.HasTopics(Kiosk), "the kiosk's seller can be asked about the job");
	// the Saturday market: crates 7-10 (no seller before seven), the scales 7-13
	CHECK(M.Run(Scales, Day(14, 9.f, Dry), Ask).bClosed && M.Run(Boxes, Day(14, 7.f, Dry), Ask).Pay == 0, "on a Friday the scales and the crates wait for Saturday");
	const FOutcome Bx = M.Run(Boxes, Day(15, 7.5f, Dry), Ask);
	CHECK(Bx.Pay == 20 && !Bx.bClosed && M.HasStamp("work_book/market"), "Saturday at half past seven: the crates pay 20 (%s)", Bx.Line.c_str());
	Earned += Bx.Pay;
	const FOutcome Sc = M.Run(Scales, Day(15, 8.f, Dry), Ask);
	FGameState G;
	FGameEnd End;
	if (Sc.Game >= 0 && M.StartGame(Scales, Day(15, 8.f, Dry), G) && Play(M, G, true))
	{
		End = M.FinishGame(G, Day(15, 8.2f, Dry));
	}
	CHECK(End.Result == EGameResult::Win && End.Pay >= 20 + 3 * 7, "weighing well: a won game pays its line and 3 a point (%d)", End.Pay);
	Earned += End.Pay;
	const FOutcome Sc2 = M.Run(Scales, Day(15, 9.f, Dry), Ask);
	CHECK(Sc2.Game < 0 && Sc2.Pay == 0 && Sc2.Line == "city.job.mk.weigh.after", "a second go the same day: words, no game, no pay");
	// lifeguard tower 1: the flag by the weather, the watch
	const FOutcome FF = M.Run(Flags, Day(12, 11.f, Fog), Ask);
	CHECK(FF.Line == "city.job.lg.flag.fog" && FF.Pay == 10, "fog: the red flag (%s)", FF.Line.c_str());
	CHECK(M.Run(Flags, Day(13, 11.f, Rain), Ask).Line == "city.job.lg.flag.rain", "rain: yellow");
	CHECK(M.Run(Flags, Day(14, 11.f, ColdDry), Ask).Line == "city.job.lg.flag.cold", "cold water: yellow, short");
	CHECK(M.Run(Flags, Day(15, 11.f, Dry), Ask).Line == "city.job.lg.flag.green", "a calm dry day: green");
	CHECK(M.Run(Flags, Day(16, 11.f, 64), Ask).Line == "city.job.lg.flag.drizzle", "a drizzle: the careful yellow");
	CHECK(M.Run(Flags, Day(17, 11.f, Dry), Ask).bClosed, "Monday: no helper on the tower");
	CHECK(M.HasTopics(Flags), "Eitan can be asked which flag");
	const FOutcome Wt = M.Run(Whistle, Day(12, 12.f, Dry), Ask);
	FGameState G2;
	FGameEnd End2;
	if (Wt.Game >= 0 && M.StartGame(Whistle, Day(12, 12.f, Dry), G2) && Play(M, G2, false))
	{
		End2 = M.FinishGame(G2, Day(12, 13.f, Dry));
	}
	CHECK(End2.Result != EGameResult::None && End2.Result != EGameResult::Win && End2.Pay >= 8 && M.HasStamp("work_book/lifeguard"),
		"a poor watch still pays a little and stamps the book (%d)", End2.Pay);
	Earned += End2.Pay;
	// the lighthouse: the lamp, an entry an hour, six a day
	int Entries = 0, Night = 0;
	const FOutcome L0 = M.Run(Watch, Day(12, 18.5f, Dry), Ask);
	CHECK(L0.Line == "city.job.px.lamp" && L0.Pay == 15 && M.HasStamp("work_book/lighthouse"), "at dusk he lights the lamp (%s)", L0.Line.c_str());
	Night += L0.Pay;
	CHECK(M.Run(Watch, Day(12, 20.5f, Fog), Ask).Line == "city.job.px.eve.fog", "a foggy hour: the bell in the log");
	for (int H = 21; H < 24; ++H)
	{
		const FOutcome L = M.Run(Watch, Day(12, (float)H + 0.5f, Dry), Ask);
		Entries += L.Pay == 10 ? 1 : 0;
		Night += L.Pay;
	}
	CHECK(Entries == 3, "an entry an hour pays 10 (%d of 3)", Entries);
	M.Run(Watch, Day(12, 23.9f, Dry), Ask);
	CHECK(M.Run(Watch, Day(12, 23.95f, Dry), Ask).Verdict == EVerdict::DailyDone, "six entries a day at most");
	const FOutcome L1 = M.Run(Watch, Day(13, 1.5f, Dry), Ask);
	CHECK(L1.Line.rfind("city.job.px.night.", 0) == 0 && L1.Pay == 10, "after midnight Arkady's watch (%s)", L1.Line.c_str());
	const FOutcome L2 = M.Run(Watch, Day(13, 6.f, Dry), Ask);
	CHECK(L2.Line == "city.job.px.dawn" && L2.Pay == 15, "at dawn the light goes out and the night is paid (%s)", L2.Line.c_str());
	const FOutcome L3 = M.Run(Watch, Day(13, 12.f, Dry), Ask);
	CHECK(L3.bClosed && L3.Pay == 0 && L3.Line.rfind("city.px.clockwork.", 0) == 0, "by day the clockwork is only the clockwork (%s)", L3.Line.c_str());
	Earned += Night;
	// «Tarelka»: the scoops, then the cake case's menu
	const FOutcome Ic = M.Run(Scoops, Day(12, 10.f, Dry), Ask);
	FGameState G3;
	FGameEnd End3;
	if (Ic.Game >= 0 && M.StartGame(Scoops, Day(12, 10.f, Dry), G3) && Play(M, G3, true))
	{
		End3 = M.FinishGame(G3, Day(12, 10.3f, Dry));
	}
	CHECK(End3.Result == EGameResult::Win && End3.Pay >= 20 && M.HasStamp("work_book/cafe"), "round scoops pay (%d)", End3.Pay);
	Earned += End3.Pay;
	const FOutcome Ic2 = M.Run(Scoops, Day(12, 11.f, Dry), Ask);
	CHECK(Ic2.Game < 0 && Ic2.Pay == 0 && Ic2.Text >= 0 && M.Texts[(size_t)Ic2.Text].Id == "tarelka_menu", "after the shift the case shows its menu again");
	// «Rassvet»: the door before the evening show
	CHECK(M.Run(Door, Day(11, 18.f, Dry), Ask).bClosed, "Tuesday: no door job");
	CHECK(M.Run(Door, Day(12, 15.f, Dry), Ask).bClosed, "three o'clock: too early");
	const FOutcome Dr = M.Run(Door, Day(12, 18.f, Dry), Ask);
	FGameState G4;
	FGameEnd End4;
	if (Dr.Game >= 0 && M.StartGame(Door, Day(12, 18.f, Dry), G4) && Play(M, G4, true))
	{
		End4 = M.FinishGame(G4, Day(12, 18.5f, Dry));
	}
	CHECK(End4.Result == EGameResult::Win && End4.Pay >= 20 + 2 * 8 && M.HasStamp("work_book/cinema"), "every ticket torn: paid (%d)", End4.Pay);
	Earned += End4.Pay;
	CHECK(M.CountStamps("work_book") == 6, "the work record book is full: %d of 6", M.CountStamps("work_book"));
	CHECK(Earned > 150 && Earned < 600, "a week of odd jobs earns %d roubles", Earned);
}

#include "cupchecks.h"
#include "homechecks.h"
#include "rewardchecks.h"
#include "rentalchecks.h"
#include "holidaychecks.h"

// The photo club's contest and the photo quests on the real data (docs/photo.md, tools/city/content/photo.json): the table
// with the box of prints and the wall of winners hand over to the camera's code ("open"), the lab's board reads the guide and
// the contest's sheet in turn, the sheet's notice follows the calendar (the month's theme, the reminder, the jury's day, the
// results), and each quest's "I have the print" topic waits for the topic that gives the quest.
static void PhotoContestChecks(const std::string& Path)
{
	std::string Text, Error;
	if (!ReadFile(Path, Text))
	{
		return;
	}
	std::printf("the photo club's contest and the photo quests (real data)\n");
	FModel M;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	const int Box = M.FindThing("P-CLUB3/contest_box"), Wall = M.FindThing("P-CLUB3/drying_line"), Board = M.FindThing("P-CLUB3/photo_board");
	const int Bench = M.FindThing("P-CLUB3/darkroom_bench"), Cloak = M.FindThing("P-CLUB3/cloak");
	const int Repair = M.FindThing("W1-U04/repair_counter"), Keeper = M.FindThing("P-PIER/tower_desk");
	if (!(Box >= 0 && Wall >= 0 && Board >= 0 && Bench >= 0 && Cloak >= 0 && Repair >= 0 && Keeper >= 0))
	{
		CHECK(false, "the contest's things and the quests' counters are there");
		return;
	}
	FAsk Ask;
	Ask.Money = 0;
	Ask.StaffOnShift = [](const std::string&, const std::string&) { return true; };
	// a Wednesday afternoon, the club open (12-23)
	const FWhen Noon = AtDate(2, 15.f, 300, 115, 87);
	const FOutcome B = M.Run(Box, Noon, Ask);
	CHECK(B.Verdict == EVerdict::Ok && !B.bClosed && B.Open == "photo_contest" && B.Event == "do" && B.Target == "photo_contest" && B.Text < 0 && B.Price == 0,
		"the table with the box of prints opens the contest's screen (%s, %s %s)", B.Open.c_str(), B.Event.c_str(), B.Target.c_str());
	const FOutcome W = M.Run(Wall, Noon, Ask);
	CHECK(W.Verdict == EVerdict::Ok && !W.bClosed && W.Open == "photo_wall" && W.Target == "photo_wall" && W.Text < 0,
		"the lab's end wall opens the wall of winners (%s)", W.Open.c_str());
	CHECK(M.Run(Bench, Noon, Ask).Open == "photo_darkroom", "the darkroom bench still opens the darkroom");
	CHECK(M.Run(Box, AtDate(2, 9.f, 300, 115, 87), Ask).Open.empty(), "in the morning the club is shut: no contest screen");
	// the board: the guide and the contest's sheet in turn
	std::set<std::string> Read;
	for (int Use = 0; Use < 2; ++Use)
	{
		const FOutcome R = M.Run(Board, Noon, Ask);
		if (R.Text >= 0)
		{
			Read.insert(M.Texts[(size_t)R.Text].Id);
		}
	}
	CHECK(Read.count("cog_photo_guide") == 1 && Read.count("notices_photo") == 1, "the photo board reads the lab's guide and the contest's sheet in turn (%d texts)", (int)Read.size());
	const int Sheet = M.FindText("notices_photo");
	CHECK(Sheet >= 0, "the contest's sheet is a text");
	if (Sheet >= 0)
	{
		// the sheet through a year: every day one notice of the contest and the rules; the theme by the month
		static const char* const Themes[4] = { "sea", "birds", "work", "festival" };
		static const int MonthDays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
		int Bad = 0, Days = 0;
		std::string FirstBad;
		for (int Mo = 1; Mo <= 12; ++Mo)
		{
			for (int Dy = 1; Dy <= MonthDays[Mo - 1]; ++Dy)
			{
				std::vector<std::string> Pages;
				M.TextPages(Sheet, AtDate(0, 15.f, 400 + Days, Mo * 100 + Dy, 100 + Days), Pages);
				const std::string Want = Dy <= 19 ? std::string("city.note.club.contest_") + Themes[(Mo - 1) % 4]
					: Dy <= 25 ? "city.note.club.contest_soon" : Dy == 26 ? "city.note.club.contest_jury" : "city.note.club.contest_done";
				if (!(Pages.size() == 2 && Pages[0] == Want && Pages[1] == "city.note.photo.rules"))
				{
					++Bad;
					if (FirstBad.empty())
					{
						FirstBad = std::to_string(Mo * 100 + Dy) + ": " + (Pages.empty() ? std::string("nothing") : Pages[0]) + " for " + Want;
					}
				}
				++Days;
			}
		}
		CHECK(Bad == 0, "the sheet through %d days: the month's theme to the 19th, the reminder to the 25th, the jury on the 26th, then the results (%d wrong, first %s)",
			Days, Bad, FirstBad.c_str());
	}
	// the town's club boards carry the same notices
	const int ClubBoards = M.FindText("notices_club");
	if (ClubBoards >= 0)
	{
		std::vector<std::string> Jan, Feb26;
		M.TextPages(ClubBoards, AtDate(0, 15.f, 500, 101, 200), Jan);
		M.TextPages(ClubBoards, AtDate(0, 15.f, 557, 226, 257), Feb26);
		CHECK(std::find(Jan.begin(), Jan.end(), "city.note.club.contest_sea") != Jan.end()
			&& std::find(Feb26.begin(), Feb26.end(), "city.note.club.contest_jury") != Feb26.end()
			&& std::find(Feb26.begin(), Feb26.end(), "city.note.club.contest_birds") == Feb26.end(),
			"the club boards: the sea on 1 January, the jury's day on 26 February");
	}
	// the three quests' topics: the one that gives the quest is offered at once, the print can be shown only after it
	struct FQuestTalk { int Thing; const char* Topic; FWhen When; };
	const FQuestTalk Talks[3] = { { Repair, "repair.grandpa", AtDate(2, 11.f, 300, 115, 87) }, { Keeper, "keeper.pier", AtDate(2, 19.f, 300, 115, 87) },
		{ Cloak, "cog.cl.maru", AtDate(2, 15.f, 300, 115, 87) } };
	for (const FQuestTalk& Q : Talks)
	{
		const int Give = M.FindTopic(Q.Topic), Done = M.FindTopic(std::string(Q.Topic) + ".done");
		std::vector<int> Now;
		M.TopicsNow(Q.Thing, Q.When, Now);
		const bool bGiveFirst = Give >= 0 && Done >= 0 && std::find(Now.begin(), Now.end(), Give) != Now.end() && std::find(Now.begin(), Now.end(), Done) == Now.end();
		const int Lines = Give >= 0 ? (int)M.AskTopic(Give, Q.When).Lines.size() : 0;
		Now.clear();
		M.TopicsNow(Q.Thing, Q.When, Now);
		const bool bDoneAfter = Done >= 0 && std::find(Now.begin(), Now.end(), Done) != Now.end();
		CHECK(bGiveFirst && Lines == 3 && bDoneAfter, "%s: asked first (3 lines, %d), then \"I have the print\" is offered (%d, %d)", Q.Topic, Lines, bGiveFirst ? 1 : 0, bDoneAfter ? 1 : 0);
	}
	M.ResetState();
}

// windows that open, the mailboxes and the taps (backlog item 31, docs/city_interiors.md "Small things that answer a touch")
static void TouchChecks(const std::string& Path)
{
	std::string Text, Error, Ru;
	if (!ReadFile(Path, Text))
	{
		return;
	}
	std::printf("small things that answer a touch (real data)\n");
	FModel M;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	ReadFile("unreal/Lyudmilovsk/Content/Localization/city/strings_ru.json", Ru);
	auto Has = [&Ru](const std::string& Key) { return Ru.find("\"" + Key + "\"") != std::string::npos; };
	// every window, vent and balcony door a building declares opens; every switch, mailbox and tap does its act
	int Windows = 0, Shut = 0, Mail = 0, NoMail = 0, Taps = 0, NoTap = 0;
	int First = -1, Vent = -1, Plain = -1;
	for (int I = 0; I < (int)M.Things.size(); ++I)
	{
		const FThing& T = M.Things[(size_t)I];
		const std::string What = T.Prop("what");
		const FAct* A = M.ActOf(I);
		if (What == "window" || What == "vent" || What == "balcony_door")
		{
			Windows++;
			Shut += M.WindowShare(I) > 0.f ? 0 : 1;
			Vent = What == "vent" && Vent < 0 ? I : Vent;
			First = What == "window" && First < 0 ? I : First;
		}
		if (What == "mailbox")
		{
			Mail++;
			NoMail += A && !A->Lines.empty() ? 0 : 1;
		}
		if (What == "tap")
		{
			Taps++;
			NoTap += A && !A->Lines.empty() ? 0 : 1;
		}
		Plain = Plain < 0 && M.WindowShare(I) <= 0.f && What.empty() ? I : Plain;
	}
	CHECK(Windows >= 6 && Shut == 0, "%d windows, vents and balcony doors in the houses, each opens (%d do not)", Windows, Shut);
	CHECK(Mail >= 4 && NoMail == 0 && Taps >= 5 && NoTap == 0, "%d banks of mailboxes and %d taps, each with its lines (%d, %d without)", Mail, Taps, NoMail, NoTap);
	if (First < 0 || Vent < 0 || Plain < 0)
	{
		CHECK(false, "a window, a vent and a plain thing to try");
		return;
	}
	// open, the air comes in; a second, narrower one adds nothing; shut the wide one: the vent's air; a plain thing never opens
	CHECK(!M.IsWindowOpen(First) && M.OpenAir() == 0.f, "a fresh visit: every window shut, no open air");
	const bool bOpen = M.ToggleWindow(First);
	CHECK(bOpen && M.IsWindowOpen(First) && std::fabs(M.OpenAir() - M.WindowShare(First)) < 1e-4f, "opened: the open air is its width (%.2f)", M.OpenAir());
	M.ToggleWindow(Vent);
	CHECK(std::fabs(M.OpenAir() - std::max(M.WindowShare(First), M.WindowShare(Vent))) < 1e-4f, "two open: the wider one counts (%.2f)", M.OpenAir());
	const bool bShut = !M.ToggleWindow(First);
	CHECK(bShut && !M.IsWindowOpen(First) && std::fabs(M.OpenAir() - M.WindowShare(Vent)) < 1e-4f, "the window shut again: only the vent's air (%.2f)", M.OpenAir());
	CHECK(!M.ToggleWindow(Plain) && !M.IsWindowOpen(Plain), "a thing that is no window never opens (%s)", M.Things[(size_t)Plain].Id.c_str());
	M.ShutWindows();
	CHECK(M.OpenAir() == 0.f && !M.IsWindowOpen(Vent), "walking into another plot shuts them all");
	M.ToggleWindow(First);
	M.ResetState();
	CHECK(M.OpenAir() == 0.f, "starting over shuts them too");
	// the words as it opens and shuts: by the weather first, then the hour; every key in the tables
	FWhen W;
	int Missing = 0;
	std::set<std::string> Keys;
	const uint8_t Weathers[] = { 0, WeatherRain, WeatherSnow, WeatherFog, WeatherCold, WeatherWarm, WeatherClear, WeatherRain | WeatherCold };
	for (const uint8_t Bits : Weathers)
	{
		for (const float Hour : { 3.f, 12.f, 22.5f })
		{
			W.Weather = Bits;
			W.Hour = Hour;
			for (const bool bO : { true, false })
			{
				const std::string Key = FModel::WindowNote(bO, W);
				Keys.insert(Key);
				Missing += Has(Key) ? 0 : 1;
			}
		}
	}
	W.Weather = WeatherRain | WeatherCold;
	CHECK(FModel::WindowNote(true, W) == "city.window.open.rain", "rain and cold: the rain is told (%s)", FModel::WindowNote(true, W).c_str());
	W.Weather = WeatherClear;
	W.Hour = 23.f;
	CHECK(FModel::WindowNote(false, W) == "city.window.shut.night", "a clear night: the night (%s)", FModel::WindowNote(false, W).c_str());
	CHECK(Keys.size() == 14 && Missing == 0, "%d words for opening and shutting, every one in the tables (%d missing)", (int)Keys.size(), Missing);
	CHECK(Has("city.act.window.shut_prompt") && Has("city.act.window_look.prompt"), "the prompts: shut it, open and look out");
}

int main(int Argc, char** Argv)
{
	FixtureChecks();
	UntilChecks();
	WhenChecks();
	MoreChecks();
	GameChecks();
	SeatChecks();
	ReadingChecks();
	TalkChecks();
	LiftChecks();
	StayChecks();
	RealChecks(Argc > 1 ? Argv[1] : "unreal/Lyudmilovsk/Content/City/interiors.json");
	CourseChecks(Argc > 1 ? Argv[1] : "unreal/Lyudmilovsk/Content/City/interiors.json");
	ProcChecks(Argc > 1 ? Argv[1] : "unreal/Lyudmilovsk/Content/City/interiors.json");
	JobChecks(Argc > 1 ? Argv[1] : "unreal/Lyudmilovsk/Content/City/interiors.json");
	CupChecks(Argc > 1 ? Argv[1] : "unreal/Lyudmilovsk/Content/City/interiors.json");
	TouchChecks(Argc > 1 ? Argv[1] : "unreal/Lyudmilovsk/Content/City/interiors.json");
	RewardChecks(Argc > 1 ? Argv[1] : "unreal/Lyudmilovsk/Content/City/interiors.json");
	PhotoContestChecks(Argc > 1 ? Argv[1] : "unreal/Lyudmilovsk/Content/City/interiors.json");
	RentalChecks(Argc > 1 ? Argv[1] : "unreal/Lyudmilovsk/Content/City/interiors.json");
	HomeTouchChecks(Argc > 1 ? Argv[1] : "unreal/Lyudmilovsk/Content/City/interiors.json");
	RoomTouchChecks(Argc > 1 ? Argv[1] : "unreal/Lyudmilovsk/Content/City/interiors.json");
	HolidayChecks(Argc > 1 ? Argv[1] : "unreal/Lyudmilovsk/Content/City/interiors.json");
	std::printf("%d passed, %d failed\n", Passed, Failures);
	return Failures ? 1 : 0;
}
