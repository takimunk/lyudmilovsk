// The yacht school's rules outside the engine (docs/yacht_school.md, part B): the season, the go / no-go and the signal
// mast, the timetable, the water and the launch point, courses, mark rounding, the start, scoring, the resolver of unwatched
// races, the hero's lessons and ranks, the save and the news, on the real Content/City/sailing.json
// (python3 tools/sailing/sailing.py).
//   tools/sailschoolsim/run.sh
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Sailing/SailSchoolModel.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace LyudSail;

static int Failures = 0, Passed = 0;
#define CHECK(Cond, ...) do { if (Cond) { Passed++; } else { std::printf("  FAIL "); std::printf(__VA_ARGS__); std::printf("\n"); Failures++; } } while (0)

static bool ReadFile(const std::string& Path, std::string& Out)
{
	std::ifstream F(Path, std::ios::binary);
	if (!F) return false;
	std::stringstream SS;
	SS << F.rdbuf();
	Out = SS.str();
	return true;
}

static FSchoolWeather Fair(double Wind = 5.0, double Tide = -0.5)
{
	FSchoolWeather W;
	W.Wind = Wind; W.Gust = Wind * 1.25; W.TideM = Tide; W.WaterC = 17.0;
	return W;
}

static bool Has(const std::vector<std::string>& V, const std::string& S) { return std::find(V.begin(), V.end(), S) != V.end(); }
static int Count(const std::vector<ERaceEvent>& E, ERaceEvent X) { return (int)std::count(E.begin(), E.end(), X); }
static int CountL(const std::vector<ELessonEvent>& E, ELessonEvent X) { return (int)std::count(E.begin(), E.end(), X); }
static FSchoolV2 Rot(const FSchoolV2& V, double Deg)
{
	const double A = Deg * SchoolPi / 180.0, C = std::cos(A), S = std::sin(A);
	return FSchoolV2(V.X * C - V.Y * S, V.X * S + V.Y * C);
}

// ---------------------------------------------------------------------------------------------------------------------

static void TestData(const FSchoolParams& P)
{
	std::printf("data\n");
	CHECK(P.Areas.size() == 3 && P.Area("corridor") && P.Area("kids") && P.Area("race"), "three water areas: corridor, kids, race");
	CHECK(P.BoatSlots.size() == 22, "22 slots in the park: 8 optimists, 4 cadets, 9 luches, the launch (%zu)", P.BoatSlots.size());
	int N[4] = { 0, 0, 0, 0 };
	bool bHero = false;
	for (const FSchoolBoatSlot& S : P.BoatSlots) { N[S.Class < 0 ? 3 : S.Class]++; bHero = bHero || S.Id == P.HeroBoat; }
	CHECK(N[0] == P.BoatCount[0] && N[1] == P.BoatCount[1] && N[2] == P.BoatCount[2] && N[3] == 1, "the slots match the fleet (%d %d %d + %d)", N[0], N[1], N[2], N[3]);
	CHECK(bHero, "the hero's boat %s has a slot", P.HeroBoat.c_str());
	for (const char* K : { "entrance", "gate", "boathouse_door", "boat_door", "notice_board", "bosun", "wash", "classroom", "classroom_seat", "blackboard",
		"tea_table", "changing", "rigging", "signal_mast", "coach_mast", "windsock", "coach_slip", "slip_head", "slip_foot", "shore_lawn", "shore_pupil",
		"shore_boat", "coach_shore", "knot_bench", "paint_trestle", "podium", "spectators_beach", "spectators_pier", "spectators_promenade" })
	{
		CHECK(P.Place(K) != nullptr, "place %s is in the data", K);
	}
	CHECK(P.Place("classroom_seat") && P.Place("classroom_seat")->Slots.size() == 12, "12 classroom seats");
	CHECK(P.Place("spectators_pier") && P.Place("spectators_pier")->Slots.size() == 16, "16 places along the pier's rail");
	CHECK(std::fabs(P.Boathouse.SizeSea - 16.5) < 0.01 && std::fabs(P.Boathouse.SizeCoast - 9.0) < 0.01, "the boathouse is 16.5 x 9 m");
	CHECK(std::fabs(P.Park.SizeSea - 15.0) < 0.01 && P.Park.SizeCoast > 13.0, "the park is 15 m deep, %.1f along the coast", P.Park.SizeCoast);
	CHECK(P.Boathouse.PlanId == "S-SAIL-SCHOOL" && P.Park.PlanId == "S-SAIL-PARK" && P.Slip.PlanId == "S-SAIL-SLIP", "the contract's plan ids");
	// every boat stands inside the park's fence
	FSchoolArea Fence;
	Fence.Poly = P.Park.Corners;
	bool bIn = true;
	for (const FSchoolBoatSlot& S : P.BoatSlots) bIn = bIn && SchoolInside(Fence, S.At.At()) && SchoolEdgeDistance(Fence, S.At.At()) > 0.5;
	CHECK(bIn, "every boat's slot is inside the fence");
	CHECK(P.Wind[0][1] == 8.0 && P.Wind[1][1] == 10.0 && P.Wind[2][1] == 12.0 && P.Wind[0][0] == 1.5, "the contract's wind limits");
	CHECK(P.Sailors.size() == 20 && P.Lessons.size() == 10 && P.Ranks.size() == 5, "20 sailors, 10 lessons, 5 ranks");
	for (const char* G : { "kids", "juniors", "adults", "race" }) CHECK(P.Group(G) != nullptr, "group %s", G);
	CHECK(P.Group("kids") && P.Group("kids")->Condition == "sail_kids" && P.Group("race")->Condition == "sail_race", "the groups' conditions are the contract's");
	// every phase's place is a place of the school (or the water)
	bool bPlaces = true;
	for (const auto& It : P.Phases) for (const FTimetablePhaseDef& D : It.second) bPlaces = bPlaces && (D.Place == "water" || P.Place(D.Place));
	CHECK(bPlaces, "every phase has a place");
}

static void TestSeason(const FSchoolParams& P)
{
	std::printf("season\n");
	const int Open[5][3] = { { 2026, 4, 25 }, { 2027, 5, 1 }, { 2028, 4, 29 }, { 2029, 4, 28 }, { 2030, 4, 27 } };
	const int Close[5][3] = { { 2026, 11, 1 }, { 2027, 11, 7 }, { 2028, 11, 5 }, { 2029, 11, 4 }, { 2030, 11, 3 } };
	for (int I = 0; I < 5; I++)
	{
		const FSchoolDate O = SeasonOpening(P, Open[I][0]), C = SeasonClosing(P, Close[I][0]);
		CHECK(O == FSchoolDate(Open[I][0], Open[I][1], Open[I][2]) && SchoolWeekday(O) == 5, "%d opens on Saturday %d-%02d (%d-%02d)", Open[I][0], Open[I][1], Open[I][2], O.M, O.D);
		CHECK(C == FSchoolDate(Close[I][0], Close[I][1], Close[I][2]) && SchoolWeekday(C) == 6, "%d closes on Sunday %d-%02d (%d-%02d)", Close[I][0], Close[I][1], Close[I][2], C.M, C.D);
	}
	CHECK(SchoolWeekday(FSchoolDate(2026, 11, 27)) == 4 && SchoolWeekday(FSchoolDate(2000, 1, 1)) == 5, "weekdays: 27 Nov 2026 is a Friday");
	CHECK(SchoolDateOf(SchoolDayNumber(FSchoolDate(2028, 2, 29))) == FSchoolDate(2028, 2, 29), "dates go to day numbers and back");
	CHECK(SeasonStateOn(P, FSchoolDate(2026, 1, 20)) == ESeasonState::LaidUp, "January: laid up");
	CHECK(SeasonStateOn(P, FSchoolDate(2026, 4, 10)) == ESeasonState::LaidUp, "April 10: still laid up");
	CHECK(SeasonStateOn(P, FSchoolDate(2026, 4, 11)) == ESeasonState::FitOut && SeasonStateOn(P, FSchoolDate(2026, 4, 24)) == ESeasonState::FitOut, "the fortnight before the opening: fitting out");
	CHECK(SeasonStateOn(P, FSchoolDate(2026, 4, 25)) == ESeasonState::Open && SeasonStateOn(P, FSchoolDate(2026, 11, 1)) == ESeasonState::Open, "open from the opening day to the closing day");
	CHECK(SeasonStateOn(P, FSchoolDate(2026, 11, 2)) == ESeasonState::LayUp && SeasonStateOn(P, FSchoolDate(2026, 11, 8)) == ESeasonState::LayUp, "the week after: laying up");
	CHECK(SeasonStateOn(P, FSchoolDate(2026, 11, 9)) == ESeasonState::LaidUp && SeasonStateOn(P, FSchoolDate(2026, 11, 27)) == ESeasonState::LaidUp, "then laid up (today, 27 November)");
	CHECK(SeasonOpen(P, FSchoolDate(2026, 7, 1)) && !SeasonOpen(P, FSchoolDate(2026, 7, 1), true) && !SeasonOpen(P, FSchoolDate(2026, 12, 1)), "sea ice closes the water whatever the date");
	CHECK(SeasonIsSummer(P, FSchoolDate(2026, 6, 1)) && SeasonIsSummer(P, FSchoolDate(2026, 8, 31)) && !SeasonIsSummer(P, FSchoolDate(2026, 5, 31)) && !SeasonIsSummer(P, FSchoolDate(2026, 9, 1)), "summer holidays June 1 - August 31");
	const FSchoolDaylight Mid = SchoolDaylight(P, FSchoolDate(2026, 6, 21)), Nov = SchoolDaylight(P, FSchoolDate(2026, 11, 1));
	CHECK(Mid.Sunset > 21.0 && Mid.Sunset < 21.7 && Mid.Sunrise > 3.6 && Mid.Sunrise < 4.4, "midsummer: the sun from %.2f to %.2f", Mid.Sunrise, Mid.Sunset);
	CHECK(Nov.Sunset > 16.6 && Nov.Sunset < 17.3 && Nov.Sunrise > 7.4 && Nov.Sunrise < 8.2, "the closing day: the sun from %.2f to %.2f", Nov.Sunrise, Nov.Sunset);
}

static void TestGo(const FSchoolParams& P)
{
	std::printf("go / no-go\n");
	const FSchoolDate D(2026, 7, 7);
	FSchoolGo G = SchoolDecide(P, D, Fair(0.8));
	CHECK(G.Go == ESchoolGo::Shore && G.Reason == ESchoolReason::Calm, "0.8 m/s: no sailing, calm");
	G = SchoolDecide(P, D, Fair(5.0));
	CHECK(G.Go == ESchoolGo::Sail && G.ClassOk[0] && G.ClassOk[1] && G.ClassOk[2] && !G.bReef && G.Reason == ESchoolReason::None, "5 m/s: everybody sails");
	CHECK(G.ClassArea[0] == "kids" && G.ClassArea[2] == "race" && !G.bTowOut, "optimists in the children's area, luches in the race area");
	G = SchoolDecide(P, D, Fair(9.0));
	CHECK(G.Go == ESchoolGo::Sail && !G.ClassOk[0] && G.ClassReason[0] == ESchoolReason::Wind && G.ClassOk[1] && G.ClassOk[2], "9 m/s: optimists ashore, cadets and luches out");
	G = SchoolDecide(P, D, Fair(9.0), 1);
	CHECK(G.Go == ESchoolGo::Shore && G.Reason == ESchoolReason::Wind, "9 m/s for the children's group alone: a shore lesson, wind");
	G = SchoolDecide(P, D, Fair(11.0));
	CHECK(G.Go == ESchoolGo::Sail && !G.ClassOk[1] && G.ClassOk[2] && G.bReef, "11 m/s: only luches, reefed");
	G = SchoolDecide(P, D, Fair(12.5));
	CHECK(G.Go == ESchoolGo::Shore && G.Reason == ESchoolReason::Wind, "12.5 m/s: nobody");
	G = SchoolDecide(P, D, Fair(15.0));
	CHECK(G.Go == ESchoolGo::Storm, "15 m/s: storm warning");
	FSchoolWeather W = Fair(7.5);
	W.Gust = 11.5;
	G = SchoolDecide(P, D, W);
	CHECK(!G.ClassOk[0] && G.ClassReason[0] == ESchoolReason::Gusts && G.ClassOk[1], "7.5 gusting 11.5: gusts keep the optimists in");
	W = Fair(5.0); W.bThunder = true;
	CHECK(SchoolDecide(P, D, W).Go == ESchoolGo::Storm && SchoolDecide(P, D, W).Reason == ESchoolReason::Thunder, "thunder: storm warning");
	W = Fair(5.0); W.VisibilityM = 200.0;
	CHECK(SchoolDecide(P, D, W).Go == ESchoolGo::Shore && SchoolDecide(P, D, W).Reason == ESchoolReason::Fog, "dense fog: ashore");
	W = Fair(5.0); W.bDaylight = false;
	CHECK(SchoolDecide(P, D, W).Go == ESchoolGo::Closed && SchoolDecide(P, D, W).Reason == ESchoolReason::Dark, "dark: closed");
	W = Fair(5.0); W.bSeaIce = true;
	CHECK(SchoolDecide(P, D, W).Go == ESchoolGo::Closed && SchoolDecide(P, D, W).Reason == ESchoolReason::Ice, "sea ice: closed");
	CHECK(SchoolDecide(P, FSchoolDate(2026, 12, 5), Fair(5.0)).Reason == ESchoolReason::Season, "December: out of season");
	G = SchoolDecide(P, D, Fair(5.0, -4.5));
	CHECK(G.Go == ESchoolGo::Shore && G.Reason == ESchoolReason::LowWater, "spring low water: too far to roll the trolleys (%.0f m)", G.Launch.HaulM);
	G = SchoolDecide(P, D, Fair(4.0, -3.0));
	CHECK(G.ClassOk[0] && G.ClassArea[0] == "race" && G.bTowOut && G.ClassOk[2], "low water, light wind: the children are towed out to the race area");
	G = SchoolDecide(P, D, Fair(7.0, -3.0));
	CHECK(!G.ClassOk[0] && G.ClassReason[0] == ESchoolReason::LowWater && G.ClassOk[2], "low water, 7 m/s: the children stay ashore, the luches go");
	CHECK(SchoolBoardDown(P, 1.2) == 1.0 && SchoolBoardDown(P, 0.7) == 0.5 && SchoolBoardDown(P, 0.3) == 0.0, "the centreboard: down in a metre, half under it, up aground");

	std::printf("recall\n");
	const FSchoolGo Kids = SchoolDecide(P, D, Fair(7.0), 1);
	CHECK(Kids.Go == ESchoolGo::Sail, "the children go out in 7 m/s");
	CHECK(SchoolMustRecall(P, Fair(9.0), Kids) == ESchoolReason::None, "9.0: inside the margin, they stay out");
	CHECK(SchoolMustRecall(P, Fair(9.6), Kids) == ESchoolReason::Wind, "9.6: recalled");
	W = Fair(8.0); W.Gust = 12.8;
	CHECK(SchoolMustRecall(P, W, Kids) == ESchoolReason::Gusts, "8 gusting 12.8: recalled for the gusts");
	const FSchoolGo Jun = SchoolDecide(P, D, Fair(8.0), 6);
	bool Cls[3] = { false, false, false };
	W = Fair(11.6); W.Gust = 13.0;
	CHECK(SchoolMustRecall(P, W, Jun, Cls) == ESchoolReason::None && Cls[1] && !Cls[2] && !Cls[0], "juniors at 11.6: the cadets come in, the luches stay");
	CHECK(SchoolMustRecall(P, Fair(13.6), Jun, Cls) == ESchoolReason::Wind && Cls[1] && Cls[2], "13.6: everybody in");
	W = Fair(5.0); W.bThunder = true;
	CHECK(SchoolMustRecall(P, W, Jun) == ESchoolReason::Thunder, "thunder recalls");
	W = Fair(5.0); W.bDaylight = false;
	CHECK(SchoolMustRecall(P, W, Jun) == ESchoolReason::Dark, "dusk recalls");

	std::printf("the launch\n");
	std::vector<FSchoolBoatState> Boats(4);
	const FSchoolV2 Mid = SchoolCentroid(*P.Area("kids")), Home = SchoolLaunchPoint(P, -0.5).At;
	for (size_t I = 0; I < Boats.size(); I++) { Boats[I].Pos = Mid + FSchoolV2(10.0 * (double)I, 5.0); Boats[I].bKid = true; }
	CHECK(SchoolLaunchTask(P, Boats, Mid, Mid, false, Home).Kind == FSchoolLaunchTask::Station, "nothing wrong: the launch keeps its station");
	Boats[2].bCapsized = true; Boats[2].CapsizedS = 10.0;
	CHECK(SchoolLaunchTask(P, Boats, Mid, Mid, false, Home).Kind == FSchoolLaunchTask::Station, "a child capsized for 10 s: let him try");
	Boats[2].CapsizedS = 25.0;
	FSchoolLaunchTask T = SchoolLaunchTask(P, Boats, Mid, Mid, false, Home);
	CHECK(T.Kind == FSchoolLaunchTask::Right && T.Boat == 2, "25 s: the launch goes to right him");
	T = SchoolLaunchTask(P, Boats, Mid, Mid, true, Home);
	CHECK(T.Kind == FSchoolLaunchTask::Tow && T.Boat == 2, "on a recall the capsized child is towed in");
	Boats[2].bCapsized = false;
	Boats[1].Pos = FSchoolV2(P.Area("race")->Poly[0].X + 2000.0, Mid.Y);
	T = SchoolLaunchTask(P, Boats, Mid, Mid, false, Home);
	CHECK(T.Kind == FSchoolLaunchTask::Shepherd && T.Boat == 1, "a boat far outside the water is shepherded back");
	Boats[1].Pos = Mid;
	T = SchoolLaunchTask(P, Boats, Mid, Mid, true, Home);
	CHECK(T.Kind == FSchoolLaunchTask::Tow && T.Boat >= 0, "on a recall the farthest child is towed home");
}

static void TestSignal(const FSchoolParams& P)
{
	std::printf("the signal mast\n");
	const FSchoolDate Tue(2026, 7, 7), Sun(2026, 7, 12);
	FSchoolSignal S = SchoolSignalAt(P, Tue, 11.0, Fair(5.0));
	CHECK(S.State == "sail" && Has(S.Flags, "flag_school") && Has(S.Flags, "flag_optimist") && !Has(S.Flags, "flag_luch") && S.Horn == 1, "the children's morning: the school's flag and the optimists'");
	S = SchoolSignalAt(P, Tue, 15.0, Fair(11.0));
	CHECK(S.State == "sail" && Has(S.Flags, "flag_luch") && !Has(S.Flags, "flag_cadet"), "juniors in 11 m/s: only the luch flag");
	S = SchoolSignalAt(P, Tue, 11.0, Fair(9.0));
	CHECK(S.State == "shore" && Has(S.Flags, "flag_shore") && S.Reason == ESchoolReason::Wind, "too much wind for the children: the shore-lesson flag");
	S = SchoolSignalAt(P, Tue, 11.0, Fair(0.5));
	CHECK(S.State == "wait" && Has(S.Flags, "flag_ap") && S.Reason == ESchoolReason::Calm, "calm: the postponement pennant");
	S = SchoolSignalAt(P, Tue, 11.0, Fair(16.0));
	CHECK(S.State == "storm" && Has(S.Flags, "storm_cone"), "a gale: the storm cone");
	FSchoolWeather Night = Fair(5.0);
	Night.bDaylight = false;
	S = SchoolSignalAt(P, Tue, 23.5, Night);
	CHECK(S.State == "closed" && S.Flags.empty(), "night: the mast is bare");
	CHECK(SchoolSignalAt(P, Tue, 7.0, Fair(5.0)).State == "closed", "seven in the morning: nobody is in yet");
	CHECK(SchoolSignalAt(P, FSchoolDate(2026, 12, 8), 16.0, Fair(5.0)).State == "closed", "winter: closed");
	CHECK(SchoolSignalAt(P, FSchoolDate(2026, 4, 15), 12.0, Fair(5.0)).State == "fitout", "fitting out: the school's flag alone");
	S = SchoolSignalAt(P, Sun, 12.5, Fair(6.0));
	CHECK(S.State == "race" && Has(S.Flags, "flag_race") && Has(S.Flags, "flag_optimist") && Has(S.Flags, "flag_luch"), "Sunday's regatta: the race pennant and every class");
	const FSchoolNow Now = SchoolSessionAt(P, Tue, 11.0);
	std::vector<FSchoolSessionState> St(1);
	St[0].Key = Now.Key; St[0].Go = ESchoolGo::Sail; St[0].RecallHour = 10.9; St[0].Reason = ESchoolReason::Wind;
	S = SchoolSignalAt(P, Tue, 11.0, Fair(7.0), false, &St);
	CHECK(S.State == "recall" && Has(S.Flags, "flag_recall") && S.Horn == 3, "a recall: the recall flag and three blasts");
}

static void TestTimetable(const FSchoolParams& P)
{
	std::printf("timetable\n");
	// a term week (11-17 May 2026) and a holiday week (6-12 July 2026)
	const int TermN[7] = { 0, 2, 2, 2, 1, 3, 1 }, SummerN[7] = { 2, 3, 3, 3, 2, 3, 1 };
	for (int Week = 0; Week < 2; Week++)
	{
		const FSchoolDate Mon = Week == 0 ? FSchoolDate(2026, 5, 11) : FSchoolDate(2026, 7, 6);
		std::map<std::string, int> Per;
		bool bOverlap = false, bCoach = false, bBoats = false, bPhases = true;
		for (int Dn = 0; Dn < 7; Dn++)
		{
			const FSchoolDate D = SchoolAddDays(Mon, Dn);
			const std::vector<FTimetableSession> Ss = SchoolSessionsOn(P, D);
			CHECK((int)Ss.size() == (Week == 0 ? TermN : SummerN)[Dn], "%s week, weekday %d: %zu sessions (%d wanted)", Week == 0 ? "term" : "summer", Dn, Ss.size(), (Week == 0 ? TermN : SummerN)[Dn]);
			for (size_t I = 0; I < Ss.size(); I++)
			{
				Per[Ss[I].Group]++;
				for (size_t J = I + 1; J < Ss.size(); J++)
				{
					const bool bTime = Ss[I].From < Ss[J].To && Ss[J].From < Ss[I].To;
					bOverlap = bOverlap || bTime;
					bCoach = bCoach || (bTime && Ss[I].Coach == Ss[J].Coach);
					bBoats = bBoats || (bTime && (Ss[I].Classes & Ss[J].Classes));
				}
				double T = Ss[I].From;
				for (const FTimetablePhase& Ph : Ss[I].Phases) { bPhases = bPhases && std::fabs(Ph.From - T) < 1e-6 && Ph.To >= Ph.From; T = Ph.To; }
				bPhases = bPhases && std::fabs(T - Ss[I].To) < 1e-6;
			}
		}
		CHECK(!bOverlap, "%s week: no two sessions at once (one launch)", Week == 0 ? "term" : "summer");
		CHECK(!bCoach && !bBoats, "%s week: no coach and no boat class twice at once", Week == 0 ? "term" : "summer");
		CHECK(bPhases, "%s week: each session's phases follow one another to its end", Week == 0 ? "term" : "summer");
		CHECK(Per["kids"] >= 3 && Per["juniors"] >= 3 && Per["adults"] >= 3 && Per["race"] == 2, "%s week: kids %d, juniors %d, adults %d, race days %d", Week == 0 ? "term" : "summer",
			Per["kids"], Per["juniors"], Per["adults"], Per["race"]);
	}
	CHECK(SchoolSessionsOn(P, FSchoolDate(2026, 5, 12))[0].From == 15.0 && SchoolSessionsOn(P, FSchoolDate(2026, 7, 7))[0].From == 10.0, "the children sail after school in term, in the morning in the holidays");

	// every session planned afloat lies in the day's light, for two seasons
	int Afloat = 0, Bad = 0, Theory = 0, WedRaces = 0, Short = 0;
	for (int Y = 2026; Y <= 2027; Y++)
	{
		for (FSchoolDate D = SeasonOpening(P, Y); SchoolDayNumber(D) <= SchoolDayNumber(SeasonClosing(P, Y)); D = SchoolAddDays(D, 1))
		{
			const FSchoolDaylight L = SchoolDaylight(P, D);
			for (const FTimetableSession& S : SchoolSessionsOn(P, D))
			{
				if (S.bDark) Theory++;
				if (!S.bAfloat) continue;
				Afloat++;
				double Ashore = S.WaterTo;
				for (size_t I = 0; I + 1 < S.Phases.size(); I++) if (S.Phases[I].Id == "water") Ashore = S.Phases[I + 1].To;
				if (S.WaterFrom < L.Sunrise + P.DaylightMargin - 1e-6 || Ashore > L.Sunset - P.DaylightMargin + 1e-6) Bad++;
				if (S.WaterTo - S.WaterFrom < 0.5 - 1e-6) Short++;
				if (S.Event == "wednesday") WedRaces++;
			}
		}
	}
	CHECK(Afloat > 600 && Bad == 0, "%d sessions afloat in two seasons, %d outside the daylight", Afloat, Bad);
	CHECK(Short == 0, "none with under half an hour on the water");
	CHECK(Theory > 20, "%d evenings held ashore because the light fails (autumn)", Theory);
	CHECK(WedRaces >= 20 && WedRaces <= 40, "%d Wednesday evening races in two seasons (the light holds them from May to August)", WedRaces);

	std::printf("days of the calendar\n");
	std::vector<FTimetableSession> Ss = SchoolSessionsOn(P, FSchoolDate(2026, 4, 25));
	CHECK(Ss.size() == 1 && Ss[0].Kind == "race" && Ss[0].Event == "sail_opening" && Ss[0].Group == "race", "the opening day: only the opening regatta");
	Ss = SchoolSessionsOn(P, FSchoolDate(2026, 11, 1));
	CHECK(Ss.size() == 2 && Ss[0].Event == "sail_closing" && Ss[1].Kind == "layup", "the closing day: the closing regatta, then the boats are laid up");
	Ss = SchoolSessionsOn(P, FSchoolDate(2026, 6, 1));
	CHECK(Ss.size() == 1 && Ss[0].Event == "optimist_day" && Ss[0].Classes == 1, "June 1: the children's Optimist regatta, nothing else");
	const FRaceDay Cup1 = RaceDayOn(P, FSchoolDate(2026, 6, 20)), Cup2 = RaceDayOn(P, FSchoolDate(2026, 6, 21));
	CHECK(Cup1.Event == P.Event("cup_lyudmilovsk") && Cup1.Day == 0 && !Cup1.bLast && Cup2.Event == Cup1.Event && Cup2.Day == 1 && Cup2.bLast, "the Lyudmilovsk Cup: Saturday 20 and Sunday 21 June 2026");
	CHECK(RaceDayOn(P, FSchoolDate(2027, 6, 26)).Event == P.Event("cup_lyudmilovsk") && RaceDayOn(P, FSchoolDate(2027, 6, 27)).Day == 1, "in 2027: 26-27 June");
	CHECK(SchoolSessionsOn(P, FSchoolDate(2026, 6, 20))[0].Races == 3, "three races a day in the cup");
	CHECK(RaceDayOn(P, FSchoolDate(2026, 7, 8)).Event == P.Event("wednesday") && RaceDayOn(P, FSchoolDate(2026, 7, 12)).Event == P.Event("sunday"), "Wednesdays and Sundays are race days");
	CHECK(RaceDayOn(P, FSchoolDate(2026, 7, 9)).Event == -1 && RaceDayOn(P, FSchoolDate(2026, 12, 6)).Event == -1, "a Thursday is not; a December Sunday is not");
	Ss = SchoolSessionsOn(P, FSchoolDate(2026, 5, 12), true);
	CHECK(Ss.empty(), "a public day off: no lessons");
	Ss = SchoolSessionsOn(P, FSchoolDate(2026, 7, 12), true);
	CHECK(Ss.size() == 1 && Ss[0].Kind == "race", "a day off that is a Sunday: the regatta goes on");
	CHECK(SchoolSessionsOn(P, FSchoolDate(2026, 12, 3)).size() == 1 && SchoolSessionsOn(P, FSchoolDate(2026, 12, 3))[0].Kind == "theory", "out of season, a Thursday: theory in the classroom");
	CHECK(SchoolSessionsOn(P, FSchoolDate(2026, 12, 4)).empty(), "out of season, a Friday: nobody in");
	Ss = SchoolSessionsOn(P, FSchoolDate(2026, 4, 15));
	CHECK(Ss.size() == 1 && Ss[0].Kind == "fitout" && Ss[0].Phases[0].Place == "paint_trestle", "the fitting-out fortnight: painting outside the boathouse, every day");
	CHECK(SchoolSessionsOn(P, FSchoolDate(2026, 11, 4))[0].Kind == "layup", "the week after closing: laying up");
	Ss = SchoolSessionsOn(P, FSchoolDate(2026, 9, 15));
	CHECK(Ss.size() == 2 && Ss[0].bAfloat && !Ss[1].bAfloat && Ss[1].bDark && Ss[1].Kind == "theory" && Ss[1].Group == "adults", "mid September: the adults' evening is a theory evening");

	std::printf("SessionAt\n");
	const FSchoolDate Tue(2026, 5, 12);
	FSchoolNow N = SchoolSessionAt(P, Tue, 15.05);
	CHECK(N.bOn && N.Group == "kids" && N.Coach == "coach_kids" && N.Phase == "gather" && N.Place == "changing" && N.Activity == "sail_change", "15:03 the children change in the boathouse");
	N = SchoolSessionAt(P, Tue, 15.3);
	CHECK(N.Phase == "rig" && N.Place == "rigging", "15:18 rigging in the park");
	N = SchoolSessionAt(P, Tue, 15.5);
	CHECK(N.Phase == "launch" && N.Place == "slip_head", "15:30 launching down the slip");
	FSchoolWeather W = Fair(4.0);
	N = SchoolSessionAt(P, Tue, 15.8, false, nullptr, &W);
	CHECK(N.Phase == "water" && N.bAfloat && N.Block == 0 && !N.Drill.empty() && N.Place.rfind("water:", 0) == 0 && N.Area == "kids", "15:48 on the water: drill %s in %s", N.Drill.c_str(), N.Place.c_str());
	const FSchoolNow N2 = SchoolSessionAt(P, Tue, 16.6, false, nullptr, &W);
	CHECK(N2.Block == 1 && N2.Drill != N.Drill && P.Drill(N2.Drill), "the second half is another drill (%s)", N2.Drill.c_str());
	CHECK(SchoolSessionAt(P, Tue, 15.8, false, nullptr, &W).Drill == N.Drill, "the same drill whenever it is asked");
	bool bCapsizeCold = false, bCapsizeWarm = false;
	FSchoolWeather Cold = Fair(4.0), Warm = Fair(4.0);
	Cold.WaterC = 11.0; Warm.WaterC = 19.0;
	for (int Dn = 0; Dn < 120; Dn++)
	{
		const FSchoolDate D = SchoolAddDays(FSchoolDate(2026, 5, 1), Dn);
		for (const FTimetableSession& S : SchoolSessionsOn(P, D))
		{
			if (!S.bAfloat || S.Kind != "lesson") continue;
			for (int B = 0; B < P.DrillBlocks; B++)
			{
				bCapsizeCold = bCapsizeCold || SchoolDrillFor(P, S, B, &Cold, D) == "capsize";
				bCapsizeWarm = bCapsizeWarm || SchoolDrillFor(P, S, B, &Warm, D) == "capsize";
			}
		}
	}
	CHECK(!bCapsizeCold && bCapsizeWarm, "the capsize drill only in warm water");
	N = SchoolSessionAt(P, Tue, 17.0);
	CHECK(N.Phase == "return" && N.Place == "slip_foot", "17:00 coming in");
	N = SchoolSessionAt(P, Tue, 17.2);
	CHECK(N.Phase == "derig" && N.Place == "wash", "17:12 hosing the boats down");
	N = SchoolSessionAt(P, Tue, 17.4);
	CHECK(N.Phase == "debrief" && N.Place == "classroom_seat" && N.Activity == "sail_tea", "17:24 tea and the debrief");
	CHECK(!SchoolSessionAt(P, Tue, 17.7).bOn && !SchoolSessionAt(P, Tue, 9.0).bOn, "between sessions nothing is on");
	N = SchoolSessionAt(P, Tue, 19.0);
	CHECK(N.Group == "adults" && N.Coach == "coach_head" && N.Phase == "water" && N.Area == "race", "19:00 the adults are out in the race area");
	std::vector<FSchoolSessionState> St(1);
	St[0].Key = SchoolSessionAt(P, Tue, 15.8).Key; St[0].Go = ESchoolGo::Shore; St[0].Reason = ESchoolReason::Wind;
	N = SchoolSessionAt(P, Tue, 15.8, false, &St);
	CHECK(N.Kind == "shore" && N.Phase == "shore" && N.Place == "shore_pupil" && !N.bAfloat && !N.Topic.empty() && Has(P.ShoreTopics, N.Topic), "decided ashore at the start: a shore lesson (%s)", N.Topic.c_str());
	St[0].Go = ESchoolGo::Sail; St[0].RecallHour = 16.0;
	N = SchoolSessionAt(P, Tue, 15.9, false, &St);
	CHECK(N.Phase == "water", "before the recall they are out");
	N = SchoolSessionAt(P, Tue, 16.1, false, &St);
	CHECK(N.Phase == "return" && N.Place == "slip_foot", "recalled at 16:00: coming in");
	N = SchoolSessionAt(P, Tue, 16.6, false, &St);
	CHECK(N.Phase == "shore" && N.Place == "classroom_seat", "then the rest of the session in the classroom");
	N = SchoolSessionAt(P, FSchoolDate(2026, 7, 12), 12.5);
	CHECK(N.Kind == "race" && N.Phase == "water" && N.Race == 0 && N.Event == "sunday", "Sunday 12:30: the first race");
	CHECK(SchoolSessionAt(P, FSchoolDate(2026, 7, 12), 13.8).Race == 1, "13:48: the second");
	CHECK(SchoolSessionAt(P, FSchoolDate(2026, 7, 12), 14.8).Phase == "prizes" && SchoolSessionAt(P, FSchoolDate(2026, 7, 12), 14.8).Place == "podium", "14:48: the prize-giving");

	std::printf("ConditionsAt\n");
	FSchoolConditions C = SchoolConditionsAt(P, Tue, 15.8);
	CHECK(C.bSeason && C.bKids && !C.bJuniors && !C.bAdults && !C.bRace && !C.bShore, "the children's session: sail_season, sail_kids");
	C = SchoolConditionsAt(P, Tue, 19.0);
	CHECK(C.bAdults && !C.bKids, "the adults' session: sail_adults");
	C = SchoolConditionsAt(P, FSchoolDate(2026, 5, 13), 16.0);
	CHECK(C.bJuniors, "Wednesday afternoon: sail_juniors");
	C = SchoolConditionsAt(P, FSchoolDate(2026, 7, 12), 12.5);
	CHECK(C.bRace && !C.bKids && !C.bAdults && C.bSeason, "Sunday's regatta: sail_race for everyone");
	St[0].Go = ESchoolGo::Shore; St[0].RecallHour = -1.0;
	C = SchoolConditionsAt(P, Tue, 15.8, false, false, &St);
	CHECK(C.bKids && C.bShore, "a shore lesson: sail_kids and sail_shore");
	C = SchoolConditionsAt(P, Tue, 13.0);
	CHECK(C.bSeason && !C.bKids && !C.bShore, "nothing on: only sail_season");
	C = SchoolConditionsAt(P, FSchoolDate(2026, 12, 3), 16.0);
	CHECK(!C.bSeason && C.bShore && !C.bKids, "winter theory: sail_shore, no season");
	C = SchoolConditionsAt(P, FSchoolDate(2026, 4, 15), 12.0);
	CHECK(!C.bSeason && C.bFitOut && C.bShore, "fitting out: ashore, the season not open yet");
	C = SchoolConditionsAt(P, Tue, 15.8, false, true);
	CHECK(!C.bSeason && C.bKids && C.bShore, "late ice: no season, the session ashore");
	C = SchoolConditionsAt(P, FSchoolDate(2026, 9, 15), 19.0);
	CHECK(C.bAdults && C.bShore, "an autumn theory evening: sail_adults and sail_shore");

	std::printf("hire\n");
	ESchoolReason Why = ESchoolReason::None;
	CHECK(SchoolHireOpen(P, FSchoolDate(2026, 7, 6), 12.0, Fair(5.0)), "Monday noon in July: the children have the optimists, a luch can be hired");
	CHECK(!SchoolHireOpen(P, FSchoolDate(2026, 7, 7), 14.5, Fair(5.0), false, &Why), "Tuesday 14:30: the juniors have the luches");
	CHECK(SchoolHireOpen(P, FSchoolDate(2026, 7, 7), 17.0, Fair(5.0)), "17:00: free again");
	CHECK(!SchoolHireOpen(P, FSchoolDate(2026, 7, 12), 12.5, Fair(5.0)), "not during the regatta");
	CHECK(!SchoolHireOpen(P, FSchoolDate(2026, 7, 6), 12.0, Fair(13.0), false, &Why) && Why == ESchoolReason::Wind, "not in 13 m/s");
	CHECK(!SchoolHireOpen(P, FSchoolDate(2026, 7, 6), 21.0, Fair(5.0)) && !SchoolHireOpen(P, FSchoolDate(2026, 7, 6), 9.0, Fair(5.0)), "not late in the evening, not before ten");
	CHECK(!SchoolHireOpen(P, FSchoolDate(2026, 12, 6), 12.0, Fair(5.0)), "not in winter");
}

static void TestWater(const FSchoolParams& P)
{
	std::printf("the water\n");
	const FSchoolV2 PierDir = (P.PierB - P.PierA).Norm();
	for (const FSchoolArea& A : P.Areas)
	{
		double Pier = 1e18, Head = 1e18, Moor = 1e18, Hole = 1e18, Bank = 1e18, Isl = 1e18, MinOff = 1e18;
		const size_t N = A.Poly.size();
		for (size_t I = 0; I < N; I++)
		{
			// the outline sampled every 2 m
			const FSchoolV2 U = A.Poly[I], V = A.Poly[(I + 1) % N];
			const int Steps = std::max(1, (int)((V - U).Len() / 2.0));
			for (int K = 0; K <= Steps; K++)
			{
				const FSchoolV2 Q = U + (V - U) * ((double)K / Steps);
				Pier = std::min(Pier, SchoolSegDist(Q, P.PierA, P.PierB) - P.PierHalfW);
				Head = std::min(Head, (Q - P.PierB).Len());
				Moor = std::min(Moor, SchoolSegDist(Q, P.MooringA, P.MooringB));
				Hole = std::min(Hole, (Q - P.IceHole).Len());
				Bank = std::min(Bank, SchoolSegDist(Q, P.BankA, P.BankB));
				Isl = std::min(Isl, (Q - P.Island).Len() - P.IslandR);
				MinOff = std::min(MinOff, SchoolOffshore(P, Q));
			}
		}
		CHECK(Pier >= P.PierClear && Head >= P.PierHeadClear, "%s: %.1f m from the pier's deck, %.1f from its head", A.Id.c_str(), Pier, Head);
		CHECK(Moor >= P.MooringClear, "%s: %.1f m from the rowing boat's mooring line", A.Id.c_str(), Moor);
		CHECK(Hole >= P.IceHoleClear && SchoolEdgeDistance(A, P.IceHole) < 0.0, "%s: %.0f m from the ice hole", A.Id.c_str(), Hole);
		CHECK(Bank >= P.BankClear && Isl >= P.IslandClear, "%s: %.0f m from the railway's bank, %.0f from its island", A.Id.c_str(), Bank, Isl);
		if (A.Id != "corridor") CHECK(MinOff >= P.SwimLineM, "%s keeps %.0f m off the beach (the bathers' strip is %.0f)", A.Id.c_str(), MinOff, P.SwimLineM);
		// nothing of the area lies north of the pier's south edge
		bool bSouth = true;
		for (const FSchoolV2& V : A.Poly) bSouth = bSouth && (V - P.PierA).Dot(PierDir.Left()) * 0.0 + V.Y < P.PierA.Y - P.PierHalfW;
		CHECK(bSouth, "%s lies wholly south of the pier", A.Id.c_str());
	}
	const FSchoolArea& Cor = *P.Area("corridor");
	const FSchoolArea& Kids = *P.Area("kids");
	const FSchoolArea& Race = *P.Area("race");
	CHECK(SchoolInside(Cor, SchoolLaunchLineAt(P, 5.0)) && SchoolInside(Cor, SchoolLaunchLineAt(P, P.SlipFootD)) && SchoolInside(Cor, SchoolLaunchLineAt(P, 100.0)), "the launch line runs out inside the corridor");
	bool bJoin1 = false, bJoin2 = false;
	double KidsN = -1e18;
	for (const FSchoolV2& V : Kids.Poly) KidsN = std::max(KidsN, V.Y);
	for (double D = 0.0; D < 400.0; D += 2.0)
	{
		const FSchoolV2 Q(SchoolShoreX(P, KidsN - 2.0) + D, KidsN - 2.0);
		bJoin1 = bJoin1 || (SchoolInside(Cor, Q) && SchoolInside(Kids, Q));
		const FSchoolV2 R(SchoolShoreX(P, Kids.Poly[0].Y + 50.0) + D, Kids.Poly[0].Y + 50.0);
		bJoin2 = bJoin2 || (SchoolInside(Kids, R) && SchoolInside(Race, R));
	}
	CHECK(bJoin1 && bJoin2, "the corridor runs into the children's area and that into the race area");
	const FSchoolV2 KMid = SchoolCentroid(Kids), RMid = SchoolCentroid(Race);
	std::string Where;
	CHECK(SchoolInside(Kids, KMid) && SchoolInsideWater(P, KMid, &Where) && Where == "kids", "the children's area's middle is in it");
	CHECK(SchoolOffshore(P, KMid) < SchoolOffshore(P, RMid) && SchoolOffshore(P, RMid) > 400.0, "the children's area is close in, the race area out (%.0f, %.0f m)", SchoolOffshore(P, KMid), SchoolOffshore(P, RMid));
	CHECK(!SchoolInsideWater(P, P.PierB) && !SchoolInsideWater(P, P.IceHole) && !SchoolInsideWater(P, P.Island), "the pier's head, the ice hole and the island are outside");
	FSchoolRng Rng(11u);
	int Bad = 0;
	double Far = 0.0;
	for (int I = 0; I < 300; I++)
	{
		const FSchoolV2 Q(RMid.X + (Rng.U() - 0.5) * 2500.0, RMid.Y + (Rng.U() - 0.5) * 2500.0);
		for (const FSchoolArea& A : P.Areas)
		{
			const FSchoolV2 R = SchoolNearestInside(A, Q, 5.0);     // the corridor is 16 m wide
			if (SchoolEdgeDistance(A, R) < 5.0 - 1e-6) Bad++;
			if (SchoolEdgeDistance(A, Q) >= 5.0 && (R - Q).Len() > 1e-9) Bad++;
		}
		const FSchoolV2 Wq = SchoolNearestWater(P, Q, 5.0);
		if (!SchoolInsideWater(P, Wq)) Bad++;
		if (!SchoolInsideWater(P, Q)) Far = std::max(Far, (Wq - Q).Len());
	}
	CHECK(Bad == 0, "the nearest point inside is inside by the margin, for 300 points (%d wrong)", Bad);
	CHECK(Far > 100.0, "a boat far outside is sent a long way back (%.0f m)", Far);
	CHECK(SchoolAreaAfloat(P, Race, P.TideSpringLow) > 0.99, "the race area is afloat at the lowest spring water");
	CHECK(SchoolAreaAfloat(P, Kids, 0.0) > 0.99 && SchoolAreaAfloat(P, Kids, P.TideSpringLow) < 0.2, "the children's area: all afloat at high water, dry at spring low");

	std::printf("the launch point\n");
	double Last = -1.0;
	bool bMono = true, bDepth = true, bOk = true, bIn = true;
	for (double Tide = 0.0; Tide >= P.TideSpringLow - 1e-9; Tide -= 0.25)
	{
		const FSchoolLaunch L = SchoolLaunchPoint(P, Tide);
		bOk = bOk && L.bOk && L.bGuess;
		bMono = bMono && L.D >= Last - 1e-9;
		Last = L.D;
		bDepth = bDepth && Tide - SchoolBedAtD(P, L.D) >= P.LaunchDepth - 1e-6 && Tide - SchoolBedAtD(P, std::max(0.0, L.D - 2.0)) < P.LaunchDepth + 0.1;
		for (double D = L.D; D < 500.0; D += 1.0) bDepth = bDepth && Tide - SchoolBedAtD(P, D) >= P.BarDepth - 1e-6;
		bIn = bIn && (L.D > 130.0 || SchoolInside(Cor, L.At)) && SchoolInsideWater(P, L.At);
	}
	CHECK(bOk, "a launch point at every tide from high water to spring low");
	CHECK(bMono, "the lower the tide the farther out");
	CHECK(bDepth, "half a metre of water at it, less just inshore, and no bar drier than 0.3 m beyond");
	CHECK(bIn, "always in the school's water");
	const FSchoolLaunch Hi = SchoolLaunchPoint(P, 0.0), Lo = SchoolLaunchPoint(P, P.TideSpringLow), Neap = SchoolLaunchPoint(P, P.TideNeapLow);
	CHECK(Hi.D < 30.0 && Hi.HaulM < 20.0, "high water: launched off the slip's foot (%.0f m out)", Hi.D);
	CHECK(Neap.HaulM > 100.0 && Neap.HaulM < P.MaxHaul, "neap low water: a %.0f m haul over the flat, still done", Neap.HaulM);
	CHECK(Lo.D > 300.0 && Lo.HaulM > P.MaxHaul, "spring low water: %.0f m out, too far", Lo.D);
	const FSchoolLaunch Sm = SchoolLaunchPointSampled(P, [&P](const FSchoolV2& Q) { return -1.5 - SchoolBedAtD(P, (Q - P.LaunchOrigin).Dot(P.LaunchDir)); });
	CHECK(Sm.bOk && !Sm.bGuess && std::fabs(Sm.D - SchoolLaunchPoint(P, -1.5).D) < 1e-6, "the sea's own depth gives the same point as the profile it was made from");
	CHECK(!SchoolLaunchPointSampled(P, [](const FSchoolV2&) { return 0.1; }).bOk, "no water anywhere: no launch point");
}

static void TestCourses(const FSchoolParams& P)
{
	std::printf("courses\n");
	int Sets = 0, NotOk = 0, Outside = 0, Shallow = 0, Skew = 0, WrongEnd = 0, Below = 0, Short = 0, NearPier = 0;
	double MinScale = 1.0;
	const ERaceCourse Kinds[4] = { ERaceCourse::WindwardLeeward, ERaceCourse::Triangle, ERaceCourse::KidsTriangle, ERaceCourse::Figure8 };
	for (int Dir = 0; Dir < 16; Dir++)
	{
		const FSchoolV2 WindTo = SchoolWindTo(Dir * 22.5);
		for (const double Wind : { 2.0, 5.0, 8.0, 11.5 })
		{
			for (int Cls = 0; Cls < SchoolBoats; Cls++)
			{
				if (Wind > P.Wind[Cls][1]) continue;
				for (const ERaceCourse Kind : Kinds)
				{
					for (const double Tide : { 0.0, -2.0 })
					{
						const uint32_t Seed = RaceSeed(FSchoolDate(2026, 6, 1 + Dir), 3, 0, Cls);
						const FRaceCourse C = RaceSetCourse(P, Kind, Cls, WindTo, Wind, 8, Seed, Tide);
						Sets++;
						if (!C.bOk) { NotOk++; continue; }
						const FSchoolArea& A = *P.Area(C.Area);
						std::vector<FSchoolV2> Pts;
						for (const FRaceMark& M : C.Marks) Pts.push_back(M.At);
						if (C.bLine) { Pts.push_back(C.LineBoat); Pts.push_back(C.LinePin); }
						for (const FSchoolV2& Q : Pts)
						{
							if (SchoolEdgeDistance(A, Q) < P.CourseMargin - 1e-6) Outside++;
							if (SchoolDepthAt(P, Q, Tide) < A.MinDepth - 1e-6) Shallow++;
							if (SchoolSegDist(Q, P.PierA, P.PierB) - P.PierHalfW < P.PierClear) NearPier++;
						}
						MinScale = std::min(MinScale, C.Scale);
						if (C.bLine)
						{
							const FSchoolV2 Line = (C.LinePin - C.LineBoat).Norm();
							if (std::fabs(Line.Dot(C.WindTo)) > 1e-6) Skew++;
							// the launch is the starboard end: on the right hand looking upwind
							if ((C.LineBoat - C.LineMid()).Dot(C.Up.Left()) > 0.0) WrongEnd++;
							for (const FRaceMark& M : C.Marks) if ((M.At - C.LineMid()).Dot(C.Up) <= 0.0) Below++;
						}
						if (Kind != ERaceCourse::Figure8 && C.LegM < P.MinLeg - 1e-6) Short++;
					}
				}
			}
		}
	}
	CHECK(NotOk == 0, "a course is set for every wind direction, wind, class and kind (%d of %d failed)", NotOk, Sets);
	CHECK(Outside == 0, "every mark and line end lies %.0f m inside its area (%d outside)", P.CourseMargin, Outside);
	CHECK(Shallow == 0, "with the area's depth of water under it (%d shallow)", Shallow);
	CHECK(NearPier == 0, "none near the pier");
	CHECK(Skew == 0 && WrongEnd == 0 && Below == 0, "the line square to the wind, the launch at the starboard end, the marks to windward of it (%d %d %d)", Skew, WrongEnd, Below);
	CHECK(Short == 0 && MinScale > 0.3, "no beat under %.0f m (the smallest scale %.2f)", P.MinLeg, MinScale);
	// north wind: the windward mark is north of the line, the wing mark on the port hand (west = +X)
	const FRaceCourse N = RaceSetCourse(P, ERaceCourse::Triangle, SchoolLuch, SchoolWindTo(0.0), 6.0, 9, 5u, 0.0);
	CHECK(N.bOk && N.Marks.size() == 3 && N.Marks[0].At.Y > N.LineMid().Y + 100.0 && std::fabs(N.Marks[0].At.X - N.LineMid().X) < 1e-6, "a northerly: mark 1 dead north of the line");
	CHECK(N.Marks[1].At.X > N.Marks[0].At.X + 50.0 && N.Marks[1].At.Y < N.Marks[0].At.Y && N.Marks[1].At.Y > N.Marks[2].At.Y, "mark 2 out to the west (the port side), between 1 and 3");
	CHECK(N.LineBoat.X < N.LinePin.X, "the launch at the east end of the line");
	CHECK(N.Order.size() == 4 && N.Order[0] == 0 && N.Order[1] == 1 && N.Order[2] == 2 && N.Order[3] == 0 && N.LengthM > 3.0 * N.LegM, "the triangle: 1, 2, 3, 1 and home (%.0f m)", N.LengthM);
	const FRaceCourse Wl = RaceSetCourse(P, ERaceCourse::WindwardLeeward, SchoolLuch, SchoolWindTo(270.0), 6.0, 9, 5u, 0.0);
	CHECK(Wl.bOk && Wl.Order.size() == 3 && Wl.Marks[0].At.X > Wl.LineMid().X + 100.0, "a westerly: mark 1 out to sea; windward-leeward 1, 2, 1");
	CHECK(std::fabs(Wl.LegM - 300.0) < 1.0, "a luch's beat in 6 m/s is 300 m (%.0f)", Wl.LegM);
	const FRaceCourse Light = RaceSetCourse(P, ERaceCourse::WindwardLeeward, SchoolLuch, SchoolWindTo(270.0), 2.0, 9, 5u, 0.0);
	const FRaceCourse Fresh = RaceSetCourse(P, ERaceCourse::WindwardLeeward, SchoolLuch, SchoolWindTo(270.0), 11.0, 9, 5u, 0.0);
	CHECK(Light.LegM < Wl.LegM && Fresh.LegM > Wl.LegM, "shorter in light airs (%.0f), longer in a breeze (%.0f)", Light.LegM, Fresh.LegM);
	const FRaceCourse Op = RaceSetCourse(P, ERaceCourse::Triangle, SchoolOptimist, SchoolWindTo(270.0), 6.0, 8, 5u, 0.0);
	CHECK(Op.bOk && Op.Area == "kids" && Op.LegM < Wl.LegM * 0.5, "the optimists' triangle is in the children's area and under half the size (%.0f m)", Op.LegM);
	const FRaceCourse OpLow = RaceSetCourse(P, ERaceCourse::Triangle, SchoolOptimist, SchoolWindTo(270.0), 4.0, 8, 5u, -3.5);
	CHECK(OpLow.bOk && (OpLow.Area == "race" || SchoolDepthAt(P, OpLow.Marks[2].At, -3.5) >= P.Area("kids")->MinDepth), "at low water it moves out to where there is water (%s)", OpLow.Area.c_str());
	const FRaceCourse Same = RaceSetCourse(P, ERaceCourse::Triangle, SchoolLuch, SchoolWindTo(0.0), 6.0, 9, 5u, 0.0);
	CHECK((Same.Marks[0].At - N.Marks[0].At).Len() < 1e-9 && (Same.LinePin - N.LinePin).Len() < 1e-9, "the same day and race give the same course");
	const FRaceCourse F8 = RaceSetCourse(P, ERaceCourse::Figure8, SchoolLuch, SchoolWindTo(0.0), 5.0, 1, 5u, 0.0, "kids", 2);
	CHECK(F8.bOk && !F8.bLine && F8.Marks.size() == 2 && F8.Order.size() == 4 && F8.Side[0] == 1 && F8.Side[1] == -1 && std::fabs((F8.Marks[0].At - F8.Marks[1].At).Len() - P.Figure8Gap) < 1e-6,
		"a figure-eight: two buoys %.0f m apart across the wind, rounded opposite ways", P.Figure8Gap);
	const FRaceCourse Sl = RaceSetCourse(P, ERaceCourse::StartLine, SchoolLuch, SchoolWindTo(0.0), 5.0, 6, 5u, 0.0);
	CHECK(Sl.bOk && Sl.bLine && Sl.Marks.empty() && std::fabs((Sl.LinePin - Sl.LineBoat).Len() - P.PracticeLine) < 1e-6, "a practice line and nothing else");
	const FSchoolDrill* D = P.Drill("tack_whistle");
	CHECK(D && (SchoolDrillStation(*D, N.Middle, N.WindTo) - N.Middle).Dot(N.Up) < -20.0, "the launch lies to leeward of the tacking drill");

	std::printf("the start's signals\n");
	const std::vector<FRaceSignal> Seq = RaceStartSequence(P, SchoolLuch);
	CHECK(Seq.size() == 4 && Seq[0].T == -180.0 && Seq[1].T == -60.0 && Seq[2].T == 0.0 && Seq[3].T == 0.0, "three minutes, one minute, go");
	CHECK(Seq[0].Flag == "flag_luch" && Seq[0].bUp && Seq[0].Horns == 1 && Seq[1].Flag == "flag_p" && Seq[1].bUp && Seq[1].bLong, "the class flag up with a horn, then P with a long one");
	CHECK(!Seq[2].bUp && Seq[2].Flag == "flag_luch" && Seq[2].Horns == 1 && !Seq[3].bUp && Seq[3].Flag == "flag_p", "at the gun both come down");
	CHECK(RaceStartSequence(P, SchoolOptimist)[0].Flag == "flag_optimist", "the optimists start under their own flag");
	CHECK(RaceGeneralRecall(P, 5, 8) && !RaceGeneralRecall(P, 4, 8) && !RaceGeneralRecall(P, 0, 0), "over half the fleet over: a general recall");
}

/** Sails a boat along a polyline at V m/s from time T, feeding the tracker; collects the events. */
static void Drive(const FSchoolParams& P, const FRaceCourse& C, FRaceTracker& Tr, const std::vector<FSchoolV2>& Pts, double& T, double V, std::vector<ERaceEvent>& Seen)
{
	const double Dt = 0.2;
	for (size_t I = 0; I + 1 < Pts.size(); I++)
	{
		const FSchoolV2 D = Pts[I + 1] - Pts[I];
		const double Len = D.Len(), Heading = std::atan2(D.Y, D.X) * 180.0 / SchoolPi;
		const int Steps = std::max(1, (int)std::ceil(Len / (V * Dt)));
		for (int K = I == 0 ? 0 : 1; K <= Steps; K++)
		{
			for (const ERaceEvent E : Tr.Step(P, C, T, Pts[I] + D * ((double)K / Steps), Heading)) Seen.push_back(E);
			T += Dt;
		}
	}
}

/** The three points round the K-th rounding at radius R, in the order that leaves the mark on its side (Flip: the other way). */
static void Around(const FRaceCourse& C, int K, double R, bool bFlip, std::vector<FSchoolV2>& Out)
{
	const FSchoolV2 M = C.Waypoint(K), Ray = FRaceTracker::OutRay(C, K);
	const double A = (C.Side[(size_t)K] > 0) != bFlip ? 70.0 : -70.0;
	Out.push_back(M + Rot(Ray, A) * R);
	Out.push_back(M + Ray * R);
	Out.push_back(M + Rot(Ray, -A) * R);
}

/** A clean lap: from under the line, across it, every mark on its side, home across the line. */
static std::vector<FSchoolV2> GoodPath(const FRaceCourse& C, double R)
{
	std::vector<FSchoolV2> Pts, Tmp;
	const FSchoolV2 Port = C.Up.Left();
	if (C.bLine)
	{
		Around(C, 0, R, false, Tmp);
		const double Lat = (Tmp[0] - C.Waypoint(0)).Dot(Port) > 0.0 ? 1.0 : -1.0;
		const FSchoolV2 X = C.LineMid() + Port * (Lat * (C.LinePin - C.LineBoat).Len() * 0.3);
		Pts.push_back(X - C.Up * 25.0);
		Pts.push_back(X + C.Up * 6.0);
	}
	for (int K = 0; K < (int)C.Order.size(); K++) Around(C, K, R, false, Pts);
	if (C.bLine)
	{
		const double Lat = (Pts.back() - C.Waypoint((int)C.Order.size() - 1)).Dot(Port) > 0.0 ? 1.0 : -1.0;
		const FSchoolV2 X = C.LineMid() + Port * (Lat * (C.LinePin - C.LineBoat).Len() * 0.3);
		Pts.push_back(X + C.Up * 12.0);
		Pts.push_back(X - C.Up * 20.0);
	}
	return Pts;
}

static void TestTracker(const FSchoolParams& P)
{
	std::printf("rounding the marks\n");
	for (int Dir = 0; Dir < 16; Dir += 3)
	{
		for (const ERaceCourse Kind : { ERaceCourse::WindwardLeeward, ERaceCourse::Triangle, ERaceCourse::KidsTriangle })
		{
			const FRaceCourse C = RaceSetCourse(P, Kind, SchoolLuch, SchoolWindTo(Dir * 22.5), 6.0, 8, 9u, 0.0);
			FRaceTracker Tr;
			std::vector<ERaceEvent> Seen;
			double T = -8.0;
			Drive(P, C, Tr, GoodPath(C, 9.0), T, 3.0, Seen);
			CHECK(Tr.Status == ERaceStatus::Finished && Count(Seen, ERaceEvent::Started) == 1 && Count(Seen, ERaceEvent::Rounded) == (int)C.Order.size()
				&& Count(Seen, ERaceEvent::Finished) == 1 && Count(Seen, ERaceEvent::WrongSide) == 0 && Count(Seen, ERaceEvent::Touch) == 0 && Count(Seen, ERaceEvent::Missed) == 0,
				"wind from %.0f, course %d: a clean lap finishes (%d of %zu rounded, status %s)", Dir * 22.5, (int)Kind, Count(Seen, ERaceEvent::Rounded), C.Order.size(), RaceStatusId(Tr.Status));
			CHECK(Tr.FinishT > Tr.StartT && Tr.StartT >= 0.0 && std::fabs(Tr.FinishT - Tr.StartT - C.LengthM / 3.0) < C.LengthM / 3.0 * 0.35 + 8.0 * (double)C.Order.size(), "its time is about the course's length at its speed (%.0f s for %.0f m)", Tr.FinishT - Tr.StartT, C.LengthM);
		}
	}
	// a northerly, by hand: up the east side of mark 1, round its north, down the west side = the mark left to port
	const FRaceCourse C = RaceSetCourse(P, ERaceCourse::WindwardLeeward, SchoolLuch, SchoolWindTo(0.0), 6.0, 8, 9u, 0.0, std::string(), 1);
	const FSchoolV2 M = C.Marks[0].At, S = C.LineMid() + FSchoolV2(-8.0, 0.0);
	CHECK(C.Order.size() == 1, "one beat: mark 1 and home");
	{
		FRaceTracker Tr;
		std::vector<ERaceEvent> Seen;
		double T = -5.0;
		Drive(P, C, Tr, { S + FSchoolV2(0.0, -20.0), S + FSchoolV2(0.0, 10.0), M + FSchoolV2(-10.0, -10.0), M + FSchoolV2(-10.0, 5.0), M + FSchoolV2(0.0, 10.0), M + FSchoolV2(10.0, 5.0),
			M + FSchoolV2(10.0, -10.0), S + FSchoolV2(16.0, 10.0), S + FSchoolV2(16.0, -20.0) }, T, 3.0, Seen);
		CHECK(Count(Seen, ERaceEvent::Rounded) == 1 && Tr.Status == ERaceStatus::Finished, "east about, north about, west about: rounded to port, finished");
		CHECK(Tr.NextMark(C) == -1, "next: the finish");
	}
	{
		// the other way round: the mark left to starboard
		FRaceTracker Tr;
		std::vector<ERaceEvent> Seen;
		double T = -5.0;
		Drive(P, C, Tr, { S + FSchoolV2(16.0, -20.0), S + FSchoolV2(16.0, 10.0), M + FSchoolV2(10.0, -10.0), M + FSchoolV2(10.0, 5.0), M + FSchoolV2(0.0, 10.0), M + FSchoolV2(-10.0, 5.0),
			M + FSchoolV2(-10.0, -10.0), S + FSchoolV2(0.0, 10.0), S + FSchoolV2(0.0, -20.0) }, T, 3.0, Seen);
		CHECK(Count(Seen, ERaceEvent::Rounded) == 0 && Count(Seen, ERaceEvent::WrongSide) == 1, "the wrong way round: not rounded, said so");
		CHECK(Count(Seen, ERaceEvent::FinishRefused) == 1 && Tr.Status == ERaceStatus::Racing && Tr.Close() == ERaceStatus::Dnf, "the finish does not take him: DNF");
		CHECK(Tr.NextMark(C) == 0, "next: still mark 1");
	}
	{
		// wrong way, back the same way (unwound), then properly
		FRaceTracker Tr;
		std::vector<ERaceEvent> Seen;
		double T = -5.0;
		Drive(P, C, Tr, { S + FSchoolV2(16.0, -20.0), S + FSchoolV2(16.0, 10.0), M + FSchoolV2(10.0, 5.0), M + FSchoolV2(0.0, 10.0), M + FSchoolV2(-10.0, 5.0) }, T, 3.0, Seen);
		CHECK(Count(Seen, ERaceEvent::WrongSide) == 1 && Tr.Wind == -1, "wrong way over the top");
		Drive(P, C, Tr, { M + FSchoolV2(-10.0, 5.0), M + FSchoolV2(0.0, 10.0), M + FSchoolV2(10.0, 5.0) }, T, 3.0, Seen);
		CHECK(Count(Seen, ERaceEvent::Rounded) == 0 && Tr.Wind == 0, "back the same way: unwound, not yet rounded");
		Drive(P, C, Tr, { M + FSchoolV2(10.0, 5.0), M + FSchoolV2(10.0, -12.0), M + FSchoolV2(-10.0, -12.0), M + FSchoolV2(-10.0, 5.0), M + FSchoolV2(0.0, 10.0), M + FSchoolV2(10.0, 5.0),
			M + FSchoolV2(10.0, -10.0), S + FSchoolV2(16.0, 10.0), S + FSchoolV2(16.0, -20.0) }, T, 3.0, Seen);
		CHECK(Count(Seen, ERaceEvent::Rounded) == 1 && Tr.Status == ERaceStatus::Finished, "then round it properly and finish");
	}
	{
		// a touch: a turn is owed
		FRaceTracker Tr;
		std::vector<ERaceEvent> Seen;
		double T = -5.0;
		Drive(P, C, Tr, { S + FSchoolV2(0.0, -20.0), S + FSchoolV2(0.0, 10.0), M + FSchoolV2(-10.0, -10.0), M + FSchoolV2(-0.6, 0.3), M + FSchoolV2(0.0, 10.0), M + FSchoolV2(10.0, 5.0),
			M + FSchoolV2(10.0, -10.0) }, T, 3.0, Seen);
		CHECK(Count(Seen, ERaceEvent::Touch) == 1 && Tr.PenaltiesOwed == 1 && Tr.Touches == 1, "brushing the mark: a touch, one turn owed");
		FRaceTracker Dsq = Tr;
		std::vector<ERaceEvent> Seen2;
		double T2 = T;
		Drive(P, C, Dsq, { M + FSchoolV2(10.0, -10.0), S + FSchoolV2(16.0, 10.0), S + FSchoolV2(16.0, -20.0) }, T2, 3.0, Seen2);
		CHECK(Dsq.Status == ERaceStatus::Dsq, "finishing without the turn: DSQ");
		// the turn: a full circle of 4 m
		std::vector<FSchoolV2> Circle;
		const FSchoolV2 Cc = M + FSchoolV2(14.0, -10.0);
		for (int K = 0; K <= 36; K++) Circle.push_back(Cc + Rot(FSchoolV2(-4.0, 0.0), K * 12.5));     // a turn and a quarter
		Drive(P, C, Tr, Circle, T, 2.0, Seen);
		CHECK(Count(Seen, ERaceEvent::PenaltyDone) == 1 && Tr.PenaltiesOwed == 0, "a full turn clears it");
		Drive(P, C, Tr, { Circle.back(), S + FSchoolV2(16.0, 10.0), S + FSchoolV2(16.0, -20.0) }, T, 3.0, Seen);
		CHECK(Tr.Status == ERaceStatus::Finished, "and he finishes");
	}
	{
		// a missed mark on the triangle: by mark 1 on the inside, on round mark 2
		const FRaceCourse Tc = RaceSetCourse(P, ERaceCourse::Triangle, SchoolLuch, SchoolWindTo(0.0), 6.0, 8, 9u, 0.0);
		FRaceTracker Tr;
		std::vector<ERaceEvent> Seen;
		double T = -5.0;
		std::vector<FSchoolV2> Pts = { Tc.LineMid() + FSchoolV2(-8.0, -20.0), Tc.LineMid() + FSchoolV2(-8.0, 10.0), Tc.Marks[0].At + FSchoolV2(6.0, -25.0) };
		Around(Tc, 1, 9.0, false, Pts);
		Drive(P, Tc, Tr, Pts, T, 3.0, Seen);
		CHECK(Count(Seen, ERaceEvent::Missed) == 1 && Count(Seen, ERaceEvent::Rounded) == 0 && Tr.NextMark(Tc) == 0, "cutting inside mark 1 and rounding 2: mark 1 missed");
	}

	std::printf("the start\n");
	{
		// over the line at the gun: OCS until he comes back
		FRaceTracker Tr;
		std::vector<ERaceEvent> Seen;
		double T = -6.0;
		Drive(P, C, Tr, { S + FSchoolV2(0.0, -10.0), S + FSchoolV2(0.0, 20.0) }, T, 3.0, Seen);
		CHECK(Count(Seen, ERaceEvent::Ocs) == 1 && Count(Seen, ERaceEvent::Started) == 0 && Tr.Status == ERaceStatus::Ocs, "over at the gun: OCS, not started");
		FRaceTracker On = Tr;
		std::vector<ERaceEvent> Seen2;
		double T2 = T;
		Drive(P, C, On, { S + FSchoolV2(0.0, 20.0), M + FSchoolV2(-10.0, -10.0), M + FSchoolV2(-10.0, 5.0), M + FSchoolV2(0.0, 10.0), M + FSchoolV2(10.0, 5.0), S + FSchoolV2(16.0, 10.0) }, T2, 3.0, Seen2);
		CHECK(On.Close() == ERaceStatus::Ocs && Count(Seen2, ERaceEvent::Finished) == 0, "sailing on without coming back: scored OCS");
		Drive(P, C, Tr, { S + FSchoolV2(0.0, 20.0), S + FSchoolV2(0.0, -6.0) }, T, 3.0, Seen);
		CHECK(Count(Seen, ERaceEvent::OcsCleared) == 1 && Tr.Status == ERaceStatus::Waiting, "back under the line: cleared");
		Drive(P, C, Tr, { S + FSchoolV2(0.0, -6.0), S + FSchoolV2(0.0, 10.0) }, T, 3.0, Seen);
		CHECK(Count(Seen, ERaceEvent::Started) == 1 && Tr.StartDelay() > 10.0, "then he starts, late (%.0f s)", Tr.StartDelay());
	}
	{
		FRaceTracker Tr;
		std::vector<ERaceEvent> Seen;
		double T = -8.0;
		Drive(P, C, Tr, { S + FSchoolV2(0.0, -26.0), S + FSchoolV2(0.0, 10.0) }, T, 3.0, Seen);
		CHECK(Count(Seen, ERaceEvent::Started) == 1 && Tr.StartDelay() >= 0.0 && Tr.StartDelay() < 2.0, "timed right: across %.1f s after the gun", Tr.StartDelay());
	}
	{
		// round the end of the line is not a start
		FRaceTracker Tr;
		std::vector<ERaceEvent> Seen;
		double T = 1.0;
		const FSchoolV2 Pin = C.LinePin;
		Drive(P, C, Tr, { Pin + FSchoolV2(12.0, -15.0), Pin + FSchoolV2(12.0, 15.0) }, T, 3.0, Seen);
		CHECK(Count(Seen, ERaceEvent::Started) == 0, "passing outside the pin is not a start");
		double T3 = P.StartWindowS + 1.0;
		Drive(P, C, Tr, { Pin + FSchoolV2(12.0, 15.0), Pin + FSchoolV2(12.0, 20.0) }, T3, 3.0, Seen);
		CHECK(Count(Seen, ERaceEvent::Dns) == 1 && Tr.Status == ERaceStatus::Dns, "not started in %.0f s: DNS", P.StartWindowS);
	}
	{
		FRaceTracker Tr;
		CHECK(Tr.Close() == ERaceStatus::Dns, "a boat that never came: DNS");
	}
	{
		// a figure-eight has no line: racing from the first step, a to port, b to starboard
		const FRaceCourse F8 = RaceSetCourse(P, ERaceCourse::Figure8, SchoolLuch, SchoolWindTo(45.0), 5.0, 1, 5u, 0.0, "kids", 2);
		FRaceTracker Tr;
		Tr.ResetFree();
		std::vector<ERaceEvent> Seen;
		double T = 0.0;
		Drive(P, F8, Tr, GoodPath(F8, 8.0), T, 2.5, Seen);
		CHECK(Count(Seen, ERaceEvent::Rounded) == 4 && Tr.Status == ERaceStatus::Finished, "two figure-eights: four roundings, done (%d)", Count(Seen, ERaceEvent::Rounded));
		FRaceTracker Bad;
		Bad.ResetFree();
		std::vector<ERaceEvent> Seen2;
		std::vector<FSchoolV2> Pts;
		for (int K = 0; K < 4; K++) Around(F8, K, 8.0, K % 2 == 1, Pts);     // both buoys the same way: ovals, not eights
		double T2 = 0.0;
		Drive(P, F8, Bad, Pts, T2, 2.5, Seen2);
		CHECK(Bad.Status != ERaceStatus::Finished && Count(Seen2, ERaceEvent::WrongSide) >= 1, "ovals round both the same way are not figure-eights");
	}
}

static void TestScoring(const FSchoolParams& P)
{
	std::printf("scoring\n");
	auto F = [](const char* Slot, ERaceStatus St, double T) { FRaceFinish R; R.Slot = Slot; R.Status = St; R.TimeS = T; return R; };
	std::vector<FRaceFinish> R = { F("c", ERaceStatus::Finished, 630.0), F("a", ERaceStatus::Finished, 600.0), F("e", ERaceStatus::Dnf, 0.0), F("b", ERaceStatus::Finished, 615.0),
		F("d", ERaceStatus::Finished, 615.04), F("f", ERaceStatus::Ocs, 590.0), F("g", ERaceStatus::Dsq, 580.0), F("h", ERaceStatus::Dns, 0.0) };
	RaceScore(R);
	std::map<std::string, double> Pts;
	std::map<std::string, int> Place;
	for (const FRaceFinish& X : R) { Pts[X.Slot] = X.Points; Place[X.Slot] = X.Place; }
	CHECK(R[0].Slot == "a" && Pts["a"] == 1.0 && Place["a"] == 1, "the first home scores 1");
	CHECK(Pts["b"] == 2.5 && Pts["d"] == 2.5 && Place["b"] == 2 && Place["d"] == 2, "a dead heat for second: 2.5 each");
	CHECK(Pts["c"] == 4.0 && Place["c"] == 4, "the next is fourth");
	CHECK(Pts["e"] == 9.0 && Pts["f"] == 9.0 && Pts["g"] == 9.0 && Pts["h"] == 9.0 && Place["f"] == 0, "DNF, OCS, DSQ, DNS: the entries + 1 (9 of 8)");
	std::vector<FRaceFinish> R2 = { F("a", ERaceStatus::Finished, 600.0), F("b", ERaceStatus::Dnf, 0.0) };
	RaceScore(R2, 12);
	CHECK(R2[1].Points == 13.0, "with 12 entered a DNF scores 13");
	CHECK(RaceDiscards(P, 3) == 0 && RaceDiscards(P, 4) == 1 && RaceDiscards(P, 8) == 1 && RaceDiscards(P, 9) == 2 && RaceDiscards(P, 30) == 3, "discards: none under 4 races, 1 from 4, 2 from 9, 3 from 16");
	std::map<std::string, std::vector<double>> Sc;
	Sc["anna"] = { 1.0, 2.0, 9.0, 1.0 };       // net 4 (drops the 9)
	Sc["boris"] = { 2.0, 1.0, 1.0, 3.0 };      // net 4 (drops the 3): 1, 1, 2 beats 1, 1, 2? level -> the last race
	Sc["vera"] = { 3.0, 3.0, 2.0, 2.0 };       // net 7
	Sc["gleb"] = { 4.0, 4.0 };                 // came twice: 9 for the two he missed, net 17
	std::vector<FRaceSeriesRow> Rows = RaceSeriesTable(P, Sc, 4, 9.0);
	CHECK(Rows.size() == 4 && Rows[0].Net == 4.0 && Rows[1].Net == 4.0 && Rows[2].Slot == "vera" && Rows[2].Net == 7.0 && Rows[3].Slot == "gleb" && Rows[3].Net == 17.0 && Rows[3].Total == 26.0,
		"a series of four with one discard: 4, 4, 7, 17");
	CHECK(Rows[0].Slot == "anna" && Rows[0].Place == 1 && Rows[1].Slot == "boris" && Rows[1].Place == 2, "level on points and on kept scores: the last race decides (anna won it)");
	CHECK(Rows[0].Dropped[2] && !Rows[0].Dropped[0] && Rows[0].Total == 13.0, "anna's 9 is the one dropped");
	Sc.clear();
	Sc["anna"] = { 1.0, 3.0, 3.0 };            // 7: best scores 1, 3, 3
	Sc["boris"] = { 2.0, 2.0, 3.0 };           // 7: best scores 2, 2, 3 -> anna's 1 wins
	Rows = RaceSeriesTable(P, Sc, 3, 9.0);
	CHECK(Rows[0].Slot == "anna" && Rows[1].Place == 2, "level on points: the one with the better best score is ahead");
	Sc.clear();
	Sc["anna"] = { 1.5, 1.5 };
	Sc["boris"] = { 1.5, 1.5 };
	Rows = RaceSeriesTable(P, Sc, 2, 9.0);
	CHECK(Rows[0].Place == 1 && Rows[1].Place == 1, "level in everything: they share the place");
}

static void TestResolve(const FSchoolParams& P)
{
	std::printf("races nobody watches\n");
	const std::vector<FRaceEntry> En = RaceEntries(P, SchoolLuch);
	CHECK(En.size() == 8 && RaceEntries(P, SchoolOptimist).size() == 8 && RaceEntries(P, SchoolCadet).size() == 4, "8 luches, 8 optimists, 4 cadets race");
	const std::vector<FRaceFinish> A = RaceResolve(P, 1234u, SchoolLuch, En, 6.0, 7.5, 1200.0), B = RaceResolve(P, 1234u, SchoolLuch, En, 6.0, 7.5, 1200.0);
	bool bSame = A.size() == B.size();
	for (size_t I = 0; I < A.size() && bSame; I++) bSame = A[I].Slot == B[I].Slot && A[I].TimeS == B[I].TimeS && A[I].Status == B[I].Status && A[I].Points == B[I].Points;
	CHECK(bSame, "the same seed gives the same race");
	int Differ = 0;
	for (uint32_t S = 1; S <= 20; S++) if (RaceResolve(P, S, SchoolLuch, En, 6.0, 7.5, 1200.0)[0].Slot != A[0].Slot) Differ++;
	CHECK(Differ > 0, "other days have other winners (%d of 20)", Differ);
	std::map<std::string, int> Wins, Dnf;
	double TMin = 1e9, TMax = 0.0, Sum = 0.0;
	int N = 3000, Finishers = 0;
	for (int I = 0; I < N; I++)
	{
		const std::vector<FRaceFinish> R = RaceResolve(P, SchoolHash((uint32_t)I, 77u), SchoolLuch, En, 6.0, 7.5, 1200.0);
		if (R[0].Status == ERaceStatus::Finished) Wins[R[0].Slot]++;
		for (const FRaceFinish& X : R)
		{
			Sum += X.Points;
			if (X.Status == ERaceStatus::Finished) { TMin = std::min(TMin, X.TimeS); TMax = std::max(TMax, X.TimeS); Finishers++; }
			else Dnf[X.Slot]++;
		}
	}
	// the luches by skill: adult_1 .84, jun_l1 .74, adult_2 .72, jun_l2 .60, adult_3 .58, jun_l3 .46, adult_4 .44, adult_5 .30
	CHECK(Wins["adult_1"] > Wins["adult_2"] && Wins["adult_2"] > Wins["adult_3"] && Wins["adult_3"] > Wins["adult_5"], "the better sailor wins more often (%d, %d, %d, %d of %d)", Wins["adult_1"],
		Wins["adult_2"], Wins["adult_3"], Wins["adult_5"], N);
	CHECK(Wins["adult_1"] > N / 4 && Wins["adult_1"] < N * 7 / 10, "but not always: the best wins %d %% of them", Wins["adult_1"] * 100 / N);
	CHECK(Wins["adult_5"] > 0 && Wins["adult_5"] < N / 20, "the weakest wins now and then (%d)", Wins["adult_5"]);
	CHECK(TMin > 300.0 && TMax < 1800.0, "a 1200 m course in 6 m/s takes a luch %.0f to %.0f minutes", TMin / 60.0, TMax / 60.0);
	CHECK(Finishers > N * 8 * 85 / 100, "most finish (%d %%)", Finishers * 100 / (N * 8));
	int DnfLight = 0, DnfFresh = 0, HeavyLight = 0, HeavyFresh = 0, DnfBest = 0, DnfWorst = 0;
	for (int I = 0; I < 1500; I++)
	{
		const std::vector<FRaceFinish> L = RaceResolve(P, SchoolHash((uint32_t)I, 5u), SchoolLuch, En, 4.0, 5.0, 1200.0), H = RaceResolve(P, SchoolHash((uint32_t)I, 5u), SchoolLuch, En, 11.5, 14.0, 1200.0);
		for (const FRaceFinish& X : L) if (X.Status == ERaceStatus::Dnf) DnfLight++;
		for (const FRaceFinish& X : H) if (X.Status == ERaceStatus::Dnf) { DnfFresh++; DnfBest += X.Slot == "adult_1"; DnfWorst += X.Slot == "adult_5"; }
		// jun_l2 is a light-wind sailor (heavy -0.5), jun_l1 likes a blow (0.4)
		for (size_t K = 0; K < L.size(); K++) { if (L[K].Slot == "jun_l2") HeavyLight += (int)K; if (H[K].Slot == "jun_l2") HeavyFresh += (int)K; }
	}
	CHECK(DnfFresh > DnfLight * 2, "more retire in a blow (%d against %d)", DnfFresh, DnfLight);
	CHECK(HeavyFresh > HeavyLight, "a light-wind sailor does worse in a blow");
	CHECK(DnfWorst > DnfBest, "the weaker retire more often in it (%d against %d)", DnfWorst, DnfBest);
	CHECK(RaceClassSpeed(P, SchoolLuch, 6.0) > RaceClassSpeed(P, SchoolOptimist, 6.0) && RaceClassSpeed(P, SchoolLuch, 2.0) < RaceClassSpeed(P, SchoolLuch, 6.0), "luches are faster than optimists, and everyone in more wind");
	(void)Sum;
}

static std::vector<ELessonEvent> Feed(FLessonRun& Run, const FSchoolParams& P, FLessonTelemetry T, double Seconds)
{
	std::vector<ELessonEvent> All;
	T.Dt = 0.1;
	for (double S = 0.0; S < Seconds - 1e-9; S += T.Dt)
	{
		for (const ELessonEvent E : Run.Step(P, T)) All.push_back(E);
	}
	return All;
}

static FLessonTelemetry Sailing(double Twa, double Speed = 2.2)
{
	FLessonTelemetry T;
	T.TwaDeg = Twa; T.Speed = Speed;
	return T;
}

/** Sails a lesson's course objective cleanly along the tracker's own good path. */
static std::vector<ELessonEvent> SailCourse(FLessonRun& Run, const FSchoolParams& P, bool bStart)
{
	std::vector<ELessonEvent> All;
	const std::vector<FSchoolV2> Pts = GoodPath(Run.Course, 9.0);
	double T = bStart ? -8.0 : 0.0;
	for (size_t I = 0; I + 1 < Pts.size(); I++)
	{
		const FSchoolV2 D = Pts[I + 1] - Pts[I];
		const int Steps = std::max(1, (int)std::ceil(D.Len() / 0.6));
		for (int K = 0; K <= Steps; K++)
		{
			FLessonTelemetry Tel = Sailing(60.0, 3.0);
			Tel.Dt = 0.2; Tel.Pos = Pts[I] + D * ((double)K / Steps); Tel.HeadingDeg = std::atan2(D.Y, D.X) * 180.0 / SchoolPi; Tel.RaceT = T;
			for (const ELessonEvent E : Run.Step(P, Tel)) All.push_back(E);
			T += 0.2;
			if (Run.bComplete || !Run.bCourseSet) return All;
		}
	}
	return All;
}

static void TestHero(const FSchoolParams& P)
{
	std::printf("the hero's path\n");
	FSchoolLog Log;
	const FSchoolDate Day(2026, 7, 6);      // a Monday in July
	const FSchoolWeather W = Fair(5.0);
	CHECK(!LessonAvailable(P, Log, 0, Day, 13.0, W).bOk && LessonAvailable(P, Log, 0, Day, 13.0, W).Why == "not_member", "no lessons before joining");
	CHECK(!SchoolCanJoin(P, Log, P.JoinFee - 1) && SchoolCanJoin(P, Log, P.JoinFee), "joining costs %d", P.JoinFee);
	FSchoolEvents Ev = SchoolJoin(P, Log, Day);
	CHECK(Log.bMember && Has(Ev.Quest, "sail join") && Has(Ev.Stamps, "joined") && Log.Rank == 0, "joined: the event and the stamp");
	CHECK(SchoolJoin(P, Log, Day).Quest.empty() && !SchoolCanJoin(P, Log, 1000), "only once");
	CHECK(!SchoolCanHire(P, Log) && !SchoolCanEnter(P, Log, P.Event("sunday")), "no hire and no races without the certificate");
	CHECK(SchoolNextLesson(P, Log) == 0 && LessonAvailable(P, Log, 0, Day, 13.0, W).bOk, "the first lesson is the shore one");
	CHECK(!LessonAvailable(P, Log, 1, Day, 13.0, W).bOk && LessonAvailable(P, Log, 1, Day, 13.0, W).Why == "order", "lessons in order");
	CHECK(LessonAvailable(P, Log, 0, FSchoolDate(2026, 12, 3), 16.0, W).bOk && !LessonAvailable(P, Log, 0, FSchoolDate(2026, 12, 4), 16.0, W).bOk, "the shore lesson in winter too, on the coach's days");
	CHECK(LessonAvailable(P, Log, 0, Day, 13.0, Fair(13.0)).bOk, "the shore lesson in any wind");

	// 1 shore
	FLessonRun Run;
	Run.Begin(P, 0);
	std::vector<ELessonEvent> E = Run.Notify(P, "rig");
	CHECK(E.empty() && Run.Active == 0, "the objectives go in order: knots first");
	Run.Notify(P, "knot"); Run.Notify(P, "knot");
	E = Run.Notify(P, "knot");
	CHECK(CountL(E, ELessonEvent::ObjectiveDone) == 1 && Run.Active == 1, "three knots tied");
	Run.Notify(P, "parts");
	E = Run.Notify(P, "rig");
	CHECK(Run.bComplete && CountL(E, ELessonEvent::LessonDone) == 1, "parts named, the boat rigged: the shore lesson is done");
	Ev = SchoolCompleteLesson(P, Log, 0);
	CHECK(Has(Ev.Quest, "sail lesson shore") && Has(Ev.Quest, "sail rank yunga") && Log.Rank == 1 && Ev.NewRank == 1, "he is a yunga now");
	CHECK(SchoolCompleteLesson(P, Log, 0).Quest.empty(), "a lesson counts once");

	// 2 first sail: follow the coach, then a beam reach
	CHECK(LessonAvailable(P, Log, 1, Day, 13.0, W).bOk, "the first sail on a fair Monday");
	CHECK(!LessonAvailable(P, Log, 1, Day, 13.0, Fair(7.5)).bOk && LessonAvailable(P, Log, 1, Day, 13.0, Fair(7.5)).Reason == ESchoolReason::Wind, "not in 7.5 m/s: too much for a first sail");
	CHECK(!LessonAvailable(P, Log, 1, Day, 13.0, Fair(13.0)).bOk && LessonAvailable(P, Log, 1, Day, 13.0, Fair(13.0)).Why == "weather", "not when nobody sails");
	CHECK(!LessonAvailable(P, Log, 1, Day, 21.0, W).bOk && LessonAvailable(P, Log, 1, Day, 21.0, W).Why == "hours", "not at nine in the evening");
	CHECK(!LessonAvailable(P, Log, 1, FSchoolDate(2026, 7, 12), 12.5, W).bOk && LessonAvailable(P, Log, 1, FSchoolDate(2026, 7, 12), 12.5, W).Why == "race", "not during the regatta");
	CHECK(LessonAvailable(P, Log, 1, FSchoolDate(2026, 7, 7), 19.5, W).Coach != "coach_head" && LessonAvailable(P, Log, 1, Day, 13.0, W).Coach == "coach_head", "the head coach teaches unless he is out with the adults");
	Run.Begin(P, 1);
	FLessonTelemetry T = Sailing(90.0);
	T.bHasLeader = true; T.Leader = FSchoolV2(60.0, 0.0);
	E = Feed(Run, P, T, 100.0);
	CHECK(Run.Active == 0 && Run.Progress[0] == 0.0, "60 m behind the launch is not following");
	T.Leader = FSchoolV2(12.0, 0.0);
	E = Feed(Run, P, T, 91.0);
	CHECK(Run.Active == 1, "90 s within 25 m of the launch: followed");
	E = Feed(Run, P, Sailing(88.0), 40.0);
	E = Feed(Run, P, Sailing(40.0), 5.0);
	CHECK(Run.Active == 1 && Run.Progress[1] < 1.0, "luffing up breaks the beam reach: the count starts again");
	E = Feed(Run, P, Sailing(-95.0), 61.0);
	CHECK(Run.bComplete, "a beam reach held for a minute (on either tack)");
	Ev = SchoolCompleteLesson(P, Log, 1);
	CHECK(Has(Ev.Stamps, "first_sail"), "the first-sail stamp");

	// 3 tacking: close-hauled, then five tacks; one that hangs in irons does not count
	Run.Begin(P, 2);
	Feed(Run, P, Sailing(45.0), 46.0);
	CHECK(Run.Active == 1, "close-hauled for 45 s");
	int Tacks = 0;
	double Side = 1.0;
	for (int K = 0; K < 3; K++)
	{
		Side = -Side;
		Feed(Run, P, Sailing(Side * -20.0, 1.6), 0.6); Feed(Run, P, Sailing(0.0, 1.3), 0.5); Feed(Run, P, Sailing(Side * 20.0, 1.2), 0.6);
		Tacks += CountL(Feed(Run, P, Sailing(Side * 45.0), 6.0), ELessonEvent::Tack);
	}
	CHECK(Tacks == 3 && Run.Progress[1] == 3.0, "three clean tacks counted");
	Side = -Side;
	E = Feed(Run, P, Sailing(0.0, 0.1), 6.0);
	CHECK(CountL(E, ELessonEvent::Irons) == 1, "head to wind and stopped: in irons");
	E = Feed(Run, P, Sailing(Side * 20.0, 0.2), 1.0);
	E = Feed(Run, P, Sailing(Side * 45.0), 6.0);
	CHECK(CountL(E, ELessonEvent::Tack) == 0 && Run.Progress[1] == 3.0, "the tack that hung in irons does not count");
	for (int K = 0; K < 2; K++)
	{
		Side = -Side;
		Feed(Run, P, Sailing(Side * -15.0, 1.5), 0.5); Feed(Run, P, Sailing(Side * 15.0, 1.2), 0.5);
		Feed(Run, P, Sailing(Side * 45.0), 6.0);
	}
	CHECK(Run.bComplete, "two more: five tacks, the lesson is done");
	SchoolCompleteLesson(P, Log, 2);

	// 4 the points of sail
	Run.Begin(P, 3);
	Feed(Run, P, Sailing(45.0), 31.0); Feed(Run, P, Sailing(90.0), 31.0); Feed(Run, P, Sailing(135.0), 31.0);
	CHECK(Run.Active == 3, "close-hauled, a beam reach, a broad reach");
	Feed(Run, P, Sailing(170.0), 31.0);
	CHECK(Run.bComplete, "and a run: every point of sail");
	SchoolCompleteLesson(P, Log, 3);

	// 5 gybing: two gybes without a capsize, then figure-eights
	Run.Begin(P, 4);
	Feed(Run, P, Sailing(160.0), 4.0);
	Feed(Run, P, Sailing(178.0), 0.5); Feed(Run, P, Sailing(-178.0), 0.5);
	E = Feed(Run, P, Sailing(-160.0), 1.0);
	FLessonTelemetry Cap = Sailing(-160.0, 0.0);
	Cap.bCapsized = true;
	E = Feed(Run, P, Cap, 12.0);
	CHECK(CountL(E, ELessonEvent::Capsized) == 1 && Run.Progress[0] == 0.0, "a gybe that ends upside down does not count");
	E = Feed(Run, P, Sailing(-160.0), 5.0);
	CHECK(CountL(E, ELessonEvent::Righted) == 1, "righted");
	Feed(Run, P, Sailing(-178.0), 0.5); Feed(Run, P, Sailing(178.0), 0.5);
	E = Feed(Run, P, Sailing(160.0), 5.0);
	CHECK(CountL(E, ELessonEvent::Gybe) == 1 && Run.Progress[0] == 1.0, "a clean gybe");
	Feed(Run, P, Sailing(178.0), 0.5); Feed(Run, P, Sailing(-178.0), 0.5);
	Feed(Run, P, Sailing(-160.0), 5.0);
	CHECK(Run.Active == 1, "two gybes");
	const FLessonObjective* Ob = Run.Objective(P);
	CHECK(Ob && Ob->Kind == "course" && Ob->Course == "figure8" && Ob->Laps == 2, "then the figure-eights");
	Run.SetCourse(RaceSetCourse(P, RaceCourseKind(Ob->Course), P.HeroClass, SchoolWindTo(250.0), 5.0, 1, 3u, -0.5, P.Lessons[4].Area, Ob->Laps), false);
	CHECK(Run.Course.bOk && Run.Course.Area == "kids", "laid in the children's area");
	E = SailCourse(Run, P, false);
	CHECK(Run.bComplete, "two figure-eights sailed: the lesson is done");
	SchoolCompleteLesson(P, Log, 4);

	// 6 capsize: warm water only
	FSchoolWeather Cold = W;
	Cold.WaterC = 12.0;
	CHECK(!LessonAvailable(P, Log, 5, Day, 13.0, Cold).bOk && LessonAvailable(P, Log, 5, Day, 13.0, Cold).Why == "cold_water", "the capsize lesson waits for warm water");
	CHECK(LessonAvailable(P, Log, 5, Day, 13.0, W).bOk, "17 degrees will do");
	Run.Begin(P, 5);
	Feed(Run, P, Sailing(90.0), 2.0);
	Feed(Run, P, Cap, 120.0);
	E = Feed(Run, P, Sailing(90.0), 2.0);
	CHECK(Run.Progress[0] == 0.0, "two minutes to right her is too long");
	Feed(Run, P, Cap, 40.0); Feed(Run, P, Sailing(90.0), 2.0);
	Feed(Run, P, Cap, 30.0); Feed(Run, P, Sailing(90.0), 2.0);
	CHECK(Run.bComplete, "righted twice inside 90 s");
	Ev = SchoolCompleteLesson(P, Log, 5);
	CHECK(Has(Ev.Stamps, "capsize"), "the capsize stamp");

	// 7 irons: sternway, then stopping at the buoy
	Run.Begin(P, 6);
	Feed(Run, P, Sailing(5.0, -0.5), 2.0); Feed(Run, P, Sailing(5.0, 0.3), 1.0);
	CHECK(Run.Active == 0, "two seconds astern is not enough");
	Feed(Run, P, Sailing(5.0, -0.5), 3.1);
	CHECK(Run.Active == 1, "three seconds of sternway");
	T = Sailing(10.0, 0.3);
	T.bHasTarget = true; T.Target = FSchoolV2(100.0, 100.0); T.Pos = FSchoolV2(120.0, 100.0);
	Feed(Run, P, T, 5.0);
	CHECK(!Run.bComplete, "stopped, but 20 m from the buoy");
	T.Pos = FSchoolV2(102.0, 101.0);
	Feed(Run, P, T, 3.1);
	CHECK(Run.bComplete, "stopped head to wind at the buoy");
	SchoolCompleteLesson(P, Log, 6);

	// 8 man overboard: two pick-ups
	Run.Begin(P, 7);
	T = Sailing(40.0, 2.5);
	T.bHasTarget = true; T.Target = FSchoolV2(100.0, 100.0); T.Pos = FSchoolV2(100.5, 100.0);
	E = Feed(Run, P, T, 1.0);
	CHECK(Run.Progress[0] == 0.0, "tearing past the buoy at 2.5 m/s is no pick-up");
	T.Speed = 0.6;
	E = Feed(Run, P, T, 1.0);
	CHECK(Run.Progress[0] == 1.0 && CountL(E, ELessonEvent::NextTarget) == 1, "slow alongside: picked up; the coach throws it again");
	E = Feed(Run, P, T, 3.0);
	CHECK(Run.Progress[0] == 1.0, "lying beside it does not count twice");
	T.Target = FSchoolV2(160.0, 100.0);
	Feed(Run, P, T, 1.0);
	T.Pos = FSchoolV2(159.0, 100.0);
	Feed(Run, P, T, 1.0);
	CHECK(Run.bComplete, "the second pick-up");
	SchoolCompleteLesson(P, Log, 7);
	CHECK(Log.Rank == 1 && !SchoolHasCertificate(P, Log), "eight lessons: still a yunga");

	// 9 landing: the small triangle, then onto the slip under control
	Run.Begin(P, 8);
	Ob = Run.Objective(P);
	Run.SetCourse(RaceSetCourse(P, RaceCourseKind(Ob->Course), P.HeroClass, SchoolWindTo(200.0), 5.0, 1, 4u, -0.5, P.Lessons[8].Area, Ob->Laps), false);
	CHECK(Run.Course.bOk && Run.Course.Order.size() == 4, "the small triangle, one lap");
	E = SailCourse(Run, P, false);
	CHECK(Run.Active == 1, "round the triangle");
	const FSchoolLaunch Land = SchoolLaunchPoint(P, W.TideM);
	T = Sailing(100.0, 2.8);
	T.bHasTarget = true; T.Target = Land.At; T.Pos = Land.At + FSchoolV2(2.0, 1.0);
	Feed(Run, P, T, 1.0);
	CHECK(!Run.bComplete, "charging the beach at 2.8 m/s is not a landing");
	T.Speed = 0.8;
	Feed(Run, P, T, 0.2);
	CHECK(Run.bComplete, "easing in at 0.8 m/s is");
	Ev = SchoolCompleteLesson(P, Log, 8);
	CHECK(Ev.bCertificate && Has(Ev.Quest, "sail certificate") && Has(Ev.Quest, "sail rank helm3") && Has(Ev.Stamps, "certificate") && Log.Rank == 2, "nine lessons: рулевой 3-го класса, the certificate");
	CHECK(SchoolCanHire(P, Log) && SchoolCanEnter(P, Log, P.Event("sunday")) && !SchoolCanEnter(P, Log, P.Event("optimist_day")), "he may hire a boat and enter the luch races (not the children's)");
	CHECK(!SchoolCanEnter(P, Log, P.Event("cup_lyudmilovsk"), 10) && SchoolCanEnter(P, Log, P.Event("cup_lyudmilovsk"), 20), "the cup's entry fee is 20");
	CHECK(Has(SchoolHire(P, Log, Day).Quest, "sail hire") && Log.HiredDay == SchoolDayNumber(Day), "a hire is logged");

	// 10 racing: a start within 5 s, then a windward-leeward
	CHECK(LessonAvailable(P, Log, 9, Day, 13.0, W).bOk, "the racing lesson needs the certificate, which he has");
	Run.Begin(P, 9);
	Ob = Run.Objective(P);
	CHECK(Ob && Ob->Kind == "start", "first the start");
	const FRaceCourse Rc = RaceSetCourse(P, ERaceCourse::WindwardLeeward, P.HeroClass, SchoolWindTo(280.0), 5.0, 6, 6u, -0.5, P.Lessons[9].Area, 1);
	Run.SetCourse(Rc, true);
	// late: crosses 20 s after the gun
	{
		const FSchoolV2 X = Rc.LineMid();
		FLessonTelemetry L = Sailing(45.0, 2.0);
		std::vector<ELessonEvent> All;
		for (double Tt = 15.0; Tt < 25.0; Tt += 0.2)
		{
			L.Dt = 0.2; L.RaceT = Tt; L.Pos = X + Rc.Up * ((Tt - 20.0) * 2.0);
			for (const ELessonEvent Le : Run.Step(P, L)) All.push_back(Le);
		}
		CHECK(CountL(All, ELessonEvent::BadStart) == 1 && Run.Active == 0, "twenty seconds late is a bad start");
	}
	Run.RestartStart();
	E = SailCourse(Run, P, true);
	CHECK(Run.Active == 1 && Run.Tracker.StartT < 5.0, "again: across %.1f s after the gun", Run.Tracker.StartT);
	Run.SetCourse(Rc, false);
	E = SailCourse(Run, P, false);
	CHECK(Run.bComplete, "and round the course");
	Ev = SchoolCompleteLesson(P, Log, 9);
	CHECK(Log.Rank == 2 && SchoolNextLesson(P, Log) == -1, "all ten lessons done; helm2 wants races and miles as well");

	std::printf("the log\n");
	Ev = SchoolAddSailing(P, Log, 9.0 * 1852.0, 3.0, 1);
	CHECK(Has(Ev.Quest, "sail capsize") && !Has(Ev.Stamps, "miles_10") && Log.Capsizes == 1 && Log.Wear > 0.0, "nine miles, a capsize, some wear");
	Ev = SchoolAddSailing(P, Log, 12.0 * 1852.0, 4.0, 0);
	CHECK(Has(Ev.Stamps, "miles_10") && Has(Ev.Quest, "sail miles 10") && std::fabs(Log.Miles(P) - 21.0) < 1e-9, "the ten-mile stamp at 21 miles");
	CHECK(Log.Rank == 2, "20 miles but no races yet: still helm3");
	auto F = [](const char* Slot, double T) { FRaceFinish R; R.Slot = Slot; R.TimeS = T; return R; };
	const int Sunday = P.Event("sunday");
	for (int K = 0; K < 5; K++)
	{
		std::vector<FRaceFinish> R = { F("adult_1", 600.0), F("hero", K == 4 ? 590.0 : 640.0 + K), F("adult_2", 630.0), F("adult_3", 650.0) };
		RaceScore(R);
		Ev = SchoolRecordRace(P, Log, SchoolAddDays(FSchoolDate(2026, 7, 12), 7 * (K / 2)), Sunday, SchoolLuch, R, 5.0);
		if (K == 0) CHECK(Has(Ev.Stamps, "first_race") && Has(Ev.Quest, "sail race") && Has(Ev.Quest, "sail podium"), "the first race: a stamp, third place is a podium");
	}
	CHECK(Log.Races == 5 && Log.Wins == 1 && Log.BestPlace == 1 && Log.Podiums == 5, "five races: a win, five podiums");
	CHECK(Has(Ev.Stamps, "race_win") && Has(Ev.Quest, "sail race_win") && Ev.NewRank == 3 && Log.Rank == 3 && Has(Ev.Stamps, "helm2"), "the win's stamp; five races and 20 miles: рулевой 2-го класса");
	const FSchoolSeries* Se = Log.FindSeries("sunday", SchoolLuch);
	CHECK(Se && Se->Races == 5 && Se->Scores.at("hero").size() == 5 && Se->Entries == 4, "the Sunday series holds the five races");
	std::vector<FRaceSeriesRow> Rows = RaceSeriesTable(P, Se->Scores, Se->Races, Se->Entries + 1.0);
	CHECK(Rows[0].Slot == "adult_1" && Rows[0].Net == 4.0 && Rows[1].Slot == "adult_2" && Rows[1].Net == 8.0 && Rows[2].Slot == "hero" && Rows[2].Net == 10.0 && Rows[2].Place == 3,
		"its table: adult_1 leads (1, 1, 1, 1 and a 2 dropped), the hero is third (3, 3, 3, 1 and a 3 dropped)");
	Log.Races = 15; Log.MetresSailed = 61.0 * 1852.0;
	FSchoolEvents Pe;
	SchoolPromote(P, Log, Pe);
	CHECK(Log.Rank == 4 && Has(Pe.Stamps, "helm1"), "fifteen races, sixty miles, three podiums: рулевой 1-го класса");
	Log.Wear = 0.5;
	SchoolMend(P, Log, FSchoolDate(2026, 7, 14));
	CHECK(Log.Wear == 0.5, "the bosun does not mend on a Tuesday");
	SchoolMend(P, Log, FSchoolDate(2026, 7, 13));
	CHECK(Log.Wear == 0.0, "he does on Monday");
	Ev = SchoolNoteRecall(P, Log);
	CHECK(Has(Ev.Stamps, "recall") && SchoolNoteRecall(P, Log).Stamps.empty(), "the recall stamp, once");

	std::printf("save\n");
	const std::string Saved = Log.Save();
	FSchoolLog Back;
	CHECK(Back.Load(Saved) && Back.Save() == Saved, "the log saves and loads the same (%zu bytes)", Saved.size());
	CHECK(Back.bMember && Back.Rank == Log.Rank && Back.LessonsDone == Log.LessonsDone && Back.Races == 15 && Back.Stamps == Log.Stamps && Back.Capsizes == 1
		&& std::fabs(Back.MetresSailed - Log.MetresSailed) < 1.0 && Back.Series.size() == Log.Series.size() && Back.Days.size() == Log.Days.size(), "every field comes back");
	CHECK(Back.FindSeries("sunday", SchoolLuch) && Back.FindSeries("sunday", SchoolLuch)->Scores.at("hero") == Se->Scores.at("hero"), "the series' scores come back");
	FSchoolLog Junk;
	CHECK(!Junk.Load("") && !Junk.Load("v9 1 2 3") && !Junk.Load("v1 1 2") && !Junk.bMember, "a broken save is refused and changes nothing");
	CHECK(FSchoolLog().Save() == "v1 0 -1 0 0 0 0 0 0 0 0 0 0 -1 -1 0 0 0 0", "an empty log is a short string");
}

static FSchoolWeather DayWeather(const FSchoolDate& D)
{
	const uint32_t H = SchoolHash((uint32_t)SchoolDayNumber(D), 4242u);
	FSchoolWeather W = Fair(2.5 + (H % 700) / 100.0, -(double)((H >> 10) % 250) / 100.0);
	if ((H >> 20) % 11 == 0) W.Wind = 0.6;      // a calm day now and then
	if ((H >> 20) % 13 == 0) W.Wind = 15.0;     // a gale
	W.Gust = W.Wind * 1.3;
	return W;
}

static void TestNews(const FSchoolParams& P, bool bPrint)
{
	std::printf("the news\n");
	FSchoolLog Log;
	const FSchoolDate Sun(2026, 7, 12);
	FSchoolEvents Ev;
	std::vector<FSchoolNewsItem> News = SchoolResolveDay(P, Log, Sun, Fair(6.0), SchoolWindTo(270.0), false, &Ev);
	CHECK(News.size() == 3, "a Sunday's regatta: three classes in the news (%zu)", News.size());
	bool bOk = News.size() == 3;
	for (const FSchoolNewsItem& N : News) bOk = bOk && !N.First.empty() && !N.Second.empty() && !N.Third.empty() && N.Races == 2 && N.Event == "sunday" && N.HeroPlace == 0 && N.Key == "sail.news.result" && !N.bAbandoned;
	CHECK(bOk, "each with a winner, a second and a third after two races");
	CHECK(News.size() == 3 && News[0].First != News[0].Second && P.Group("kids") && News[0].Class == SchoolOptimist && News[0].First.rfind("kids_", 0) == 0, "the optimists' winner is one of the children (%s)", News.empty() ? "" : News[0].First.c_str());
	FSchoolLog Log2;
	const std::vector<FSchoolNewsItem> Again = SchoolResolveDay(P, Log2, Sun, Fair(6.0), SchoolWindTo(270.0));
	CHECK(Again.size() == News.size() && Again[2].First == News[2].First && Again[2].Third == News[2].Third && Log2.Save() == Log.Save(), "the same day resolves the same way");
	const std::string Before = Log.Save();
	SchoolResolveDay(P, Log, Sun, Fair(6.0), SchoolWindTo(270.0));
	CHECK(Log.Save() == Before, "resolving a day twice changes nothing");
	const std::vector<FSchoolNewsItem> Read = SchoolNewsOfDay(P, Log, Sun);
	CHECK(Read.size() == 3 && Read[1].First == News[1].First, "the next morning the paper reads the same news from the log");
	CHECK(SchoolNewsOfDay(P, Log, FSchoolDate(2026, 7, 13)).empty(), "no racing on Monday: no news");
	// the hero sailed the first luch race himself: the second is resolved, he is in the day's table
	FSchoolLog Mine;
	std::vector<FRaceFinish> R;
	for (const FRaceEntry& E : RaceEntries(P, SchoolLuch)) { FRaceFinish F; F.Slot = E.Slot; F.TimeS = 900.0 - E.Skill * 100.0; R.push_back(F); }
	FRaceFinish H;
	H.Slot = "hero"; H.TimeS = 700.0;
	R.push_back(H);
	RaceScore(R);
	SchoolRecordRace(P, Mine, Sun, P.Event("sunday"), SchoolLuch, R, 6.0);
	News = SchoolResolveDay(P, Mine, Sun, Fair(6.0), SchoolWindTo(270.0));
	const FSchoolSeries* Se = Mine.FindSeries("sunday", SchoolLuch);
	CHECK(Se && Se->Races == 2 && Se->Scores.at("hero").size() == 2 && Se->Scores.at("hero")[0] == 1.0 && Se->Scores.at("hero")[1] == 10.0, "he won race 1 and missed race 2 (scored 10 of 9)");
	CHECK(News.size() == 3 && News[2].Class == SchoolLuch && News[2].HeroPlace > 0 && News[2].Key != "sail.news.result", "the luch news names his place (%d)", News.size() == 3 ? News[2].HeroPlace : -1);
	// no wind: abandoned
	FSchoolLog Calm;
	News = SchoolResolveDay(P, Calm, Sun, Fair(0.5), SchoolWindTo(270.0));
	CHECK(News.size() == 3 && News[0].bAbandoned && News[0].Reason == ESchoolReason::Calm && News[0].Key == "sail.news.abandoned" && News[0].First.empty(), "a calm Sunday: abandoned, no wind");
	CHECK(!Calm.FindSeries("sunday", SchoolLuch) || Calm.FindSeries("sunday", SchoolLuch)->Races == 0, "nothing is scored");
	// 9 m/s: the optimists stay ashore, the others race
	FSchoolLog Fresh;
	News = SchoolResolveDay(P, Fresh, Sun, Fair(9.0), SchoolWindTo(270.0));
	CHECK(News.size() == 3 && News[0].bAbandoned && News[0].Reason == ESchoolReason::Wind && !News[1].bAbandoned && !News[2].bAbandoned, "9 m/s: no optimist race, the cadets and luches sail");

	// a whole season
	FSchoolLog Season;
	int RaceDays = 0, WithNews = 0, Abandoned = 0, Items = 0, NoWinner = 0;
	std::vector<FSchoolNewsItem> Closing, CupEnd;
	for (FSchoolDate D = SeasonOpening(P, 2026); SchoolDayNumber(D) <= SchoolDayNumber(SeasonClosing(P, 2026)); D = SchoolAddDays(D, 1))
	{
		bool bRace = false;
		for (const FTimetableSession& S : SchoolSessionsOn(P, D)) bRace = bRace || S.Kind == "race";
		const FSchoolWeather W = DayWeather(D);
		const std::vector<FSchoolNewsItem> N = SchoolResolveDay(P, Season, D, W, SchoolWindTo((double)(SchoolHash((uint32_t)SchoolDayNumber(D), 9u) % 360)));
		if (!bRace) { if (!N.empty()) NoWinner += 100; continue; }
		RaceDays++;
		if (!N.empty()) WithNews++;
		for (const FSchoolNewsItem& X : N)
		{
			Items++;
			if (X.bAbandoned) Abandoned++;
			else if (X.First.empty()) NoWinner++;
		}
		if (D == SeasonClosing(P, 2026)) Closing = N;
		if (D == FSchoolDate(2026, 6, 21)) CupEnd = N;
	}
	CHECK(RaceDays > 40 && WithNews == RaceDays, "a season: %d race days, the paper has something on every one (%d)", RaceDays, WithNews);
	CHECK(NoWinner == 0 && Abandoned > 0 && Abandoned < Items / 3, "%d lines of news, %d of them abandoned races, none without a winner", Items, Abandoned);
	int SeriesFinals = 0, CupFinals = 0;
	for (const FSchoolNewsItem& X : Closing) if (X.bSeries && (X.Event == "sunday" || X.Event == "wednesday") && !X.First.empty()) SeriesFinals++;
	for (const FSchoolNewsItem& X : CupEnd) if (X.bSeries && X.Event == "cup_lyudmilovsk" && X.Key == "sail.news.cup") CupFinals++;
	CHECK(SeriesFinals == 4, "on the closing day the season's tables: Sunday's three classes and Wednesday's luches (%d)", SeriesFinals);
	CHECK(CupFinals == 3 || DayWeather(FSchoolDate(2026, 6, 21)).Wind < 1.5 || DayWeather(FSchoolDate(2026, 6, 21)).Wind > 8.0, "on the cup's Sunday the cup's final tables (%d)", CupFinals);
	const FSchoolSeries* Sunday = Season.FindSeries("sunday", SchoolLuch);
	CHECK(Sunday && Sunday->Races >= 20 && Sunday->Scores.size() == 8, "the Sunday championship: %d luch races", Sunday ? Sunday->Races : 0);
	bool bLens = Sunday != nullptr;
	if (Sunday) for (const auto& It : Sunday->Scores) bLens = bLens && (int)It.second.size() == Sunday->Races;
	CHECK(bLens, "every boat has a score for every race");
	if (Sunday)
	{
		const std::vector<FRaceSeriesRow> Rows = RaceSeriesTable(P, Sunday->Scores, Sunday->Races, Sunday->Entries + 1.0);
		bool bOrder = true;
		for (size_t I = 1; I < Rows.size(); I++) bOrder = bOrder && Rows[I].Net >= Rows[I - 1].Net && Rows[I].Place >= Rows[I - 1].Place;
		CHECK(bOrder && Rows[0].Place == 1, "its table is in order");
		size_t Best = Rows.size(), Worst = 0;
		for (size_t I = 0; I < Rows.size(); I++) { if (Rows[I].Slot == "adult_1") Best = I; if (Rows[I].Slot == "adult_5") Worst = I; }
		CHECK(Best < 3 && Worst > 4, "over a season skill tells: adult_1 is %zu, adult_5 is %zu of 8", Best + 1, Worst + 1);
		if (bPrint)
		{
			std::printf("    the Sunday championship, luch, %d races, %d discards\n", Sunday->Races, RaceDiscards(P, Sunday->Races));
			for (const FRaceSeriesRow& Rr : Rows) std::printf("    %d. %-8s net %5.1f total %5.1f\n", Rr.Place, Rr.Slot.c_str(), Rr.Net, Rr.Total);
		}
	}
	FSchoolLog Back;
	CHECK(Back.Load(Season.Save()) && Back.Save() == Season.Save(), "a season's log saves and loads the same (%zu bytes)", Season.Save().size());
	// a new year clears the tables
	SchoolResolveDay(P, Season, FSchoolDate(2027, 5, 2), Fair(5.0), SchoolWindTo(270.0));
	CHECK(Season.Season == 2027 && Season.FindSeries("sunday", SchoolLuch) && Season.FindSeries("sunday", SchoolLuch)->Races == 2 && !Season.FindSeries("cup_lyudmilovsk", SchoolLuch), "the next season starts with clean tables");

	// the cup's stamp: the hero on its podium
	FSchoolLog Cup;
	Cup.bMember = true;
	const int CupEv = P.Event("cup_lyudmilovsk");
	for (int Dn = 0; Dn < 2; Dn++)
	{
		for (int K = 0; K < 3; K++)
		{
			std::vector<FRaceFinish> Rr;
			for (const FRaceEntry& E : RaceEntries(P, SchoolLuch)) { FRaceFinish F; F.Slot = E.Slot; F.TimeS = 900.0 - E.Skill * 100.0; Rr.push_back(F); }
			FRaceFinish Hf;
			Hf.Slot = "hero"; Hf.TimeS = 820.0;     // second behind adult_1 (816)
			Rr.push_back(Hf);
			RaceScore(Rr);
			SchoolRecordRace(P, Cup, SchoolAddDays(FSchoolDate(2026, 6, 20), Dn), CupEv, SchoolLuch, Rr, 6.0);
		}
		FSchoolEvents Ce;
		News = SchoolResolveDay(P, Cup, SchoolAddDays(FSchoolDate(2026, 6, 20), Dn), Fair(6.0), SchoolWindTo(270.0), false, &Ce);
		if (Dn == 0) CHECK(!Has(Ce.Stamps, "cup_lyudmilovsk"), "no cup stamp after the first day");
		else
		{
			CHECK(Has(Ce.Stamps, "cup_lyudmilovsk") && Has(Ce.Quest, "sail cup cup_lyudmilovsk"), "second in the cup after six races: the cup's stamp");
			bool bFinal = false;
			for (const FSchoolNewsItem& X : News) bFinal = bFinal || (X.bSeries && X.Class == SchoolLuch && X.HeroPlace == 2 && X.First == "adult_1" && X.Races == 6);
			CHECK(bFinal, "and the paper has the cup's table with him second");
		}
	}

	std::printf("the forecast and the spectators\n");
	std::vector<std::pair<double, FSchoolWeather>> Hours;
	for (int Hr = 6; Hr <= 21; Hr += 3) Hours.push_back({ (double)Hr, Fair(Hr < 12 ? 3.0 : 5.5) });
	FSchoolForecast F = SchoolForecastForSailors(P, FSchoolDate(2026, 7, 7), Hours);
	CHECK(F.Go == ESchoolGo::Sail && F.Key == "sail.radio.sail" && F.BestHour >= 12.0 && Has(F.Groups, "kids") && Has(F.Groups, "juniors") && Has(F.Groups, "adults") && !F.bRace, "a fair Tuesday: sailing for all three groups, best after noon");
	F = SchoolForecastForSailors(P, Sun, Hours);
	CHECK(F.bRace && F.Event == "sunday" && F.Key == "sail.radio.race", "a fair Sunday: the regatta");
	for (auto& X : Hours) X.second = Fair(10.5);
	F = SchoolForecastForSailors(P, FSchoolDate(2026, 7, 7), Hours);
	CHECK(F.Go == ESchoolGo::Sail && F.bReef && F.Key == "sail.radio.reef" && !Has(F.Groups, "kids") && Has(F.Groups, "adults"), "10.5 m/s: luches reefed, the children ashore");
	for (auto& X : Hours) X.second = Fair(13.0);
	F = SchoolForecastForSailors(P, FSchoolDate(2026, 7, 7), Hours);
	CHECK(F.Go == ESchoolGo::Shore && F.Key == "sail.radio.shore" && F.Reason == ESchoolReason::Wind && F.Groups.empty(), "13 m/s all day: shore lessons");
	Hours[3].second = Fair(17.0);
	F = SchoolForecastForSailors(P, FSchoolDate(2026, 7, 7), Hours);
	CHECK(F.Go == ESchoolGo::Storm && F.Key == "sail.radio.storm" && F.Wind == 17.0, "a gale in the afternoon: storm warning");
	F = SchoolForecastForSailors(P, FSchoolDate(2026, 12, 1), Hours);
	CHECK(F.Go == ESchoolGo::Closed && F.Key == "sail.radio.closed" && F.Reason == ESchoolReason::Season, "December: closed");
	std::string Place;
	CHECK(SchoolSpectatorPull(P, Sun, 12.5, false, &Place) == 0.5 && Place == "spectators_pier", "Sunday's races draw people to the pier");
	CHECK(SchoolSpectatorPull(P, FSchoolDate(2026, 4, 25), 12.5, false, &Place) == 1.2 && Place == "spectators_beach", "the opening day draws the town to the beach");
	CHECK(SchoolSpectatorPull(P, Sun, 10.0) == 0.0 && SchoolSpectatorPull(P, Sun, 14.9) == 0.0 && SchoolSpectatorPull(P, FSchoolDate(2026, 7, 13), 12.5) == 0.0, "nobody before the start, after the finish, or on a Monday");
	CHECK(SchoolSpectatorPull(P, FSchoolDate(2026, 6, 1), 12.5) > 0.8, "the children's regatta on June 1 draws the parents");
}

static void PrintWeek(const FSchoolParams& P, const FSchoolDate& From)
{
	static const char* const Wd[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
	for (int Dn = 0; Dn < 7; Dn++)
	{
		const FSchoolDate D = SchoolAddDays(From, Dn);
		const FSchoolDaylight L = SchoolDaylight(P, D);
		std::printf("%s %d-%02d-%02d (sun %.2f-%.2f)\n", Wd[SchoolWeekday(D)], D.Y, D.M, D.D, L.Sunrise, L.Sunset);
		for (const FTimetableSession& S : SchoolSessionsOn(P, D))
		{
			std::printf("  %5.2f-%5.2f %-8s %-7s %-13s %s", S.From, S.To, S.Group.c_str(), S.Kind.c_str(), S.Coach.c_str(), S.Event.c_str());
			if (S.bAfloat) std::printf(" afloat %.2f-%.2f", S.WaterFrom, S.WaterTo);
			if (S.Races) std::printf(" races %d", S.Races);
			std::printf("\n");
		}
	}
}

int main(int Argc, char** Argv)
{
	std::string Text, Error;
	FSchoolParams P;
	if (!ReadFile("unreal/Lyudmilovsk/Content/City/sailing.json", Text) || !P.Load(Text, Error))
	{
		std::printf("cannot load Content/City/sailing.json: %s (run python3 tools/sailing/sailing.py)\n", Error.c_str());
		return 1;
	}
	if (Argc >= 5 && std::strcmp(Argv[1], "week") == 0)
	{
		PrintWeek(P, FSchoolDate(std::atoi(Argv[2]), std::atoi(Argv[3]), std::atoi(Argv[4])));
		return 0;
	}
	const bool bSeason = Argc >= 2 && std::strcmp(Argv[1], "season") == 0;
	TestData(P);
	TestSeason(P);
	TestGo(P);
	TestSignal(P);
	TestTimetable(P);
	TestWater(P);
	TestCourses(P);
	TestTracker(P);
	TestScoring(P);
	TestResolve(P);
	TestHero(P);
	TestNews(P, bSeason);
	std::printf("%d checks passed, %d failed (bed: %s)\n", Passed, Failures, P.BedSource.c_str());
	return Failures ? 1 : 0;
}
