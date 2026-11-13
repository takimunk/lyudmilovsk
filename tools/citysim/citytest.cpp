// The things to do inside the town's buildings, outside the engine: behaviour checks of CityThingsModel.h on a small
// made-up fixture (tools/citysim/fixture.json), then on the real Content/City/interiors.json when it has been built
// (python3 tools/city/interiors.py).
//   tools/citysim/run.sh            the checks (exit code 1 on a failure)
//   tools/citysim/run.sh <file>     the checks on another interiors file instead of the real one
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/City/CityThingsModel.h"

#include <cmath>
#include <cstdio>
#include <fstream>
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
	CHECK(M.NextLine(Shelf) == "city.read.birds.2" && M.NextLine(Shelf) == "city.read.birds.3", "the page panel's next turns on");

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
}

int main(int Argc, char** Argv)
{
	FixtureChecks();
	RealChecks(Argc > 1 ? Argv[1] : "unreal/Lyudmilovsk/Content/City/interiors.json");
	std::printf("%d passed, %d failed\n", Passed, Failures);
	return Failures ? 1 : 0;
}
