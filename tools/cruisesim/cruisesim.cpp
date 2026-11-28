// Offline checks of the motor ship's cruise (Private/Cruise/CruiseModel.h) on Content/Cruise/cruise.json and its words
// (Content/Localization/ship). Run: tools/cruisesim/run.sh [test | table | at <YYYY-MM-DD> <HH:MM>]
#include "Cruise/CruiseModel.h"

#include <cstring>
#include <fstream>
#include <set>
#include <sstream>

static int Fails = 0;
static int Checks = 0;
#define CHECK(c, ...) do { ++Checks; if (!(c)) { ++Fails; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

using namespace LyudCruise;

static std::string Read(const std::string& Path)
{
	std::ifstream F(Path, std::ios::binary);
	std::stringstream S;
	S << F.rdbuf();
	return S.str();
}

/** Seconds since 1 January 2026 of a calendar date and time. */
static double TOf(int Y, int M, int D, int Hour = 0, int Minute = 0, int Second = 0)
{
	const int Yy = M <= 2 ? Y - 1 : Y;
	const long long Era = (Yy >= 0 ? Yy : Yy - 399) / 400;
	const long long Yoe = Yy - Era * 400;
	const long long Doy = (153 * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1;
	const long long Doe = Yoe * 365 + Yoe / 4 - Yoe / 100 + Doy;
	const long long Days = Era * 146097 + Doe - 719468;
	return (double)(Days - 20454) * DaySec + Hour * 3600.0 + Minute * 60.0 + Second;
}

static std::string Clock(double Tv, const FModel& M)
{
	const double Of = Tv + M.SailClock;
	const long long Day = FloorDiv(Of, DaySec);
	const double In = Of - (double)Day * DaySec;
	static const char* Names[] = { "Fri", "Sat", "Sun", "Mon" };
	char Buf[32];
	std::snprintf(Buf, sizeof(Buf), "%s %02d:%02d", Names[std::min<long long>(3, std::max<long long>(0, Day))], (int)(In / 3600.0), (int)std::fmod(In / 60.0, 60.0));
	return Buf;
}

static void Table(const FModel& M)
{
	std::printf("route: %.1f km, %zu stops, %zu segments\n", M.Length / 1000.0, M.Stops.size(), M.Segs.size());
	for (size_t I = 0; I < M.Stops.size(); ++I)
	{
		const FStop& S = M.Stops[I];
		std::printf("  stop %2zu %-10s at (%6.0f, %6.0f) r %4.0f dir %+d turn %6.1f deg  s %8.0f..%8.0f %s\n", I,
			S.Island.empty() ? "-" : S.Island.c_str(), S.C.X, S.C.Y, S.R, S.Dir, S.Sweep * 180.0 / Pi, S.S0, S.S0 + S.R * S.Sweep, S.Event.c_str());
	}
	std::printf("legs between fixes (steady speed m/s, knots):\n");
	for (size_t I = 1; I < M.Fixes.size(); ++I)
	{
		const FFix& A = M.Fixes[I - 1];
		const FFix& B = M.Fixes[I];
		const double Vc = M.CruiseSpeed(A, B);
		std::printf("  %s -> %s  %6.1f km  %5.2f m/s  %4.1f kn\n", Clock(A.T, M).c_str(), Clock(B.T, M).c_str(), (B.S - A.S) / 1000.0, Vc, Vc / 0.5144);
	}
	std::printf("programme:\n");
	const double Sail = M.SailTime(M.VoyageAt(TOf(2026, 10, 23, 17)));
	for (const FEvent& E : M.Events)
	{
		const FState St = M.At(Sail + (E.T0 + E.T1) * 0.5);
		std::string Near;
		for (const FIsland& I : M.Islands)
		{
			if (M.InSight(St, I))
			{
				char Buf[64];
				std::snprintf(Buf, sizeof(Buf), " %s %.0f m", I.Id.c_str(), Len(St.Pos - I.At));
				Near += Buf;
			}
		}
		std::printf("  %s - %s  %-16s %-9s at (%6.0f, %6.0f) hdg %4.0f  %4.2f m/s%s\n", Clock(E.T0, M).c_str(), Clock(E.T1, M).c_str(), E.Id.c_str(),
			PhaseName(St.Phase), St.Pos.X, St.Pos.Y, St.Heading * 180.0 / Pi, St.Speed, Near.c_str());
	}
}

static void TestCalendar(const FModel& M)
{
	const FDate A = DateOf(TOf(2026, 10, 20, 16));
	CHECK(A.Year == 2026 && A.Month == 10 && A.Day == 20 && A.Weekday == 1, "20 October 2026 is a Tuesday (got %d-%d-%d wd %d)", A.Year, A.Month, A.Day, A.Weekday);
	const FDate B = DateOf(TOf(2027, 1, 1));
	CHECK(B.Year == 2027 && B.Month == 1 && B.Day == 1 && B.Weekday == 4, "1 January 2027 is a Friday");
	const FDate C = DateOf(TOf(2028, 2, 29, 23, 59));
	CHECK(C.Year == 2028 && C.Month == 2 && C.Day == 29 && C.Weekday == 1, "29 February 2028 is a Tuesday");
	CHECK(DateOf(0.0).MonthDay() == 101 && DateOf(-1.0).MonthDay() == 1231 && DateOf(-1.0).Year == 2025, "the epoch is 1 January 2026");

	// the season's gates
	CHECK(M.At(TOf(2026, 10, 20, 16)).Phase == EPhase::Port, "20 October: on the roads");
	CHECK(M.At(TOf(2026, 10, 23, 15, 59)).Phase == EPhase::Port, "Friday 15:59: still no boarding");
	CHECK(M.At(TOf(2026, 10, 23, 16, 0)).Phase == EPhase::Boarding, "Friday 16:00: boarding");
	CHECK(M.At(TOf(2026, 10, 23, 17, 59, 59)).Phase == EPhase::Boarding, "Friday 17:59:59: boarding");
	CHECK(M.At(TOf(2026, 10, 23, 18, 0)).Phase == EPhase::Underway, "Friday 18:00: sailed");
	CHECK(M.At(TOf(2026, 10, 24, 10, 30)).Phase == EPhase::Circling, "Saturday 10:30: round Bird Island");
	CHECK(M.At(TOf(2026, 10, 24, 17, 0)).Phase == EPhase::Anchored, "Saturday 17:00: at anchor off the Far Light");
	CHECK(M.At(TOf(2026, 10, 24, 16, 29)).Phase == EPhase::Underway && M.At(TOf(2026, 10, 24, 18, 30)).Phase == EPhase::Underway, "the landing is 16:30-18:30");
	CHECK(M.At(TOf(2026, 10, 25, 3, 0)).Phase == EPhase::Anchored, "Sunday 03:00: at anchor");
	CHECK(M.At(TOf(2026, 10, 25, 6, 0)).Phase == EPhase::Underway, "Sunday 06:00: under way again");
	CHECK(M.At(TOf(2026, 10, 25, 18, 0)).Phase == EPhase::Landing, "Sunday 18:00: landing");
	CHECK(M.At(TOf(2026, 10, 25, 19, 59, 59)).Phase == EPhase::Landing, "Sunday 19:59:59: landing");
	CHECK(M.At(TOf(2026, 10, 25, 20, 0)).Phase == EPhase::Port, "Sunday 20:00: over");
	CHECK(M.At(TOf(2026, 10, 23, 16, 0)).Day == 1 && M.At(TOf(2026, 10, 24, 0, 0)).Day == 2 && M.At(TOf(2026, 10, 25, 19, 0)).Day == 3
		&& M.At(TOf(2026, 10, 26, 12)).Day == 0, "the voyage's days are the calendar's");
	CHECK(M.VoyageRuns(M.VoyageAt(TOf(2026, 11, 13, 17))) && M.At(TOf(2026, 11, 14, 12)).Phase != EPhase::Port, "13-15 November: the last voyage of 2026");
	CHECK(M.At(TOf(2026, 11, 15, 23)).Phase == EPhase::Port && M.At(TOf(2026, 11, 16, 0, 0)).Phase == EPhase::Laid, "laid up from 16 November");
	CHECK(!M.VoyageRuns(M.VoyageAt(TOf(2026, 11, 20, 17))) && M.At(TOf(2026, 11, 20, 17)).Phase == EPhase::Laid, "20 November: no voyage");
	CHECK(M.At(TOf(2027, 1, 15, 17)).Phase == EPhase::Laid && M.At(TOf(2027, 4, 14, 23, 59)).Phase == EPhase::Laid, "winter: laid up");
	CHECK(M.At(TOf(2027, 4, 15, 0, 0)).Phase == EPhase::Port, "15 April: on the roads");
	CHECK(!M.VoyageRuns(M.VoyageAt(TOf(2027, 4, 9, 17))), "9 April 2027: before the season");
	CHECK(M.VoyageRuns(M.VoyageAt(TOf(2027, 4, 16, 17))) && M.At(TOf(2027, 4, 16, 17)).Phase == EPhase::Boarding, "16 April 2027: the first voyage");
	CHECK(M.VoyageRuns(M.VoyageAt(TOf(2027, 11, 12, 17))) && !M.VoyageRuns(M.VoyageAt(TOf(2027, 11, 19, 17))), "2027 ends with 12-14 November");
	int Voyages = 0;
	for (long long K = M.VoyageAt(TOf(2027, 1, 1)); K < M.VoyageAt(TOf(2028, 1, 1)); ++K)
	{
		Voyages += M.VoyageRuns(K) ? 1 : 0;
	}
	CHECK(Voyages >= 29 && Voyages <= 31, "about thirty voyages a season (got %d)", Voyages);
	// the next boarding
	CHECK(M.NextBoarding(TOf(2026, 10, 20, 16)) == TOf(2026, 10, 23, 16), "the next boarding after 20 October is Friday the 23rd");
	CHECK(M.NextBoarding(TOf(2026, 10, 23, 16, 0, 1)) == TOf(2026, 10, 30, 16), "a boarding begun is not the next one");
	CHECK(M.NextBoarding(TOf(2026, 11, 14, 12)) == TOf(2027, 4, 16, 16), "after the last voyage the next boarding is in April");
}

static void TestEvents(const FModel& M)
{
	static const char* Ids[] = { "boarding", "departure", "drill", "dinner1", "isl_brothers", "concert1", "cards1", "lights_out", "isl_arch", "wake",
		"exercise", "breakfast2", "isl_birds", "deckgames", "lunch2", "quiet", "bridge_tour", "tea2", "isl_lighthouse", "captains_dinner", "concert2",
		"cards2", "anchor", "breakfast3", "quiz", "isl_seals", "lunch3", "diplomas", "home_in_sight", "arrival", "landing" };
	for (const char* Id : Ids)
	{
		CHECK(M.FindEvent(Id) >= 0, "the programme has no %s", Id);
	}
	CHECK(!M.Events.empty() && M.Events.front().Id == "boarding" && M.Events.back().Id == "landing", "the programme runs from the boarding to the landing");
	std::set<std::string> Seen;
	for (size_t I = 0; I < M.Events.size(); ++I)
	{
		const FEvent& E = M.Events[I];
		CHECK(E.T1 > E.T0, "%s ends before it begins", E.Id.c_str());
		CHECK(E.T0 >= -M.BoardSec() && E.T1 <= M.TotalSec + M.LandingSec, "%s is outside the voyage (%.0f..%.0f)", E.Id.c_str(), E.T0, E.T1);
		CHECK(I == 0 || M.Events[I - 1].T0 <= E.T0, "%s is out of order", E.Id.c_str());
		CHECK(E.Day == 1 + (int)FloorDiv(E.T0 + M.SailClock, DaySec), "%s: its day does not match its time", E.Id.c_str());
		CHECK(Seen.insert(E.Id).second || E.Id == "wake", "%s comes twice", E.Id.c_str());
		CHECK(E.Announce == "cruise.announce." + E.Id && E.Name == "cruise.event." + E.Id + ".name", "%s: keys", E.Id.c_str());
	}
	const double Sail = M.SailTime(M.VoyageAt(TOf(2026, 10, 23, 17)));
	CHECK(M.IsEventOn(Sail - 60.0, "boarding") && !M.IsEventOn(Sail, "boarding") && M.IsEventOn(Sail, "departure"), "boarding ends as the ship sails");
	CHECK(M.IsEventOn(Sail + 4.0 * 3600.0 + 1800.0, "concert1") && M.IsEventOn(Sail + 4.0 * 3600.0 + 1800.0, "cards1"), "Friday 22:30: the concert and the cards");
	CHECK(M.IsEventOn(TOf(2026, 10, 24, 7, 5), "wake") && M.IsEventOn(TOf(2026, 10, 25, 7, 5), "wake") && !M.IsEventOn(TOf(2026, 10, 26, 7, 5), "wake"), "wake on both mornings");
	CHECK(M.EventsOn(TOf(2026, 10, 21, 12)) == 0 && M.EventsOn(TOf(2026, 12, 25, 19, 45)) == 0, "no events without a voyage");
	CHECK(M.IsEventOn(TOf(2026, 10, 25, 0, 30), "anchor") && M.IsEventOn(TOf(2026, 10, 25, 0, 30), "cards2"), "Sunday 00:30");
	double Sec = 0.0;
	int Next = M.NextEvent(Sail + 60.0, Sec);
	CHECK(Next >= 0 && M.Events[(size_t)Next].Id == "drill" && std::fabs(Sec - 29.0 * 60.0) < 1e-6, "after sailing the drill is next, in 29 minutes");
	Next = M.NextEvent(Sail + M.TotalSec + 60.0, Sec);
	CHECK(Next == 0 && std::fabs(Sec - (5.0 * DaySec - 2.0 * 3600.0 - 60.0)) < 1e-6, "during the landing the next event is next Friday's boarding (%.0f)", Sec);
	Next = M.NextEvent(TOf(2026, 12, 1), Sec);
	CHECK(Next == 0 && std::fabs(TOf(2026, 12, 1) + Sec - TOf(2027, 4, 16, 16)) < 1e-6, "in winter the next event is April's first boarding");
	CHECK(M.EventTime(Sail - 3600.0, "isl_birds") == TOf(2026, 10, 24, 10) && M.EventTime(TOf(2026, 10, 24, 12), "isl_birds") == TOf(2026, 10, 31, 10)
		&& M.EventTime(Sail, "no_such") < 0.0, "an event's time: this voyage's while it is ahead, else the next one's");

	// the words
	for (const char* Lang : { "ru", "en", "ja" })
	{
		LyudJson::FValue T;
		std::string Err;
		CHECK(LyudJson::Parse(Read(std::string("unreal/Lyudmilovsk/Content/Localization/ship/strings_") + Lang + ".json"), T, Err), "%s: %s", Lang, Err.c_str());
		std::set<std::string> K;
		for (const auto& KV : T.Fields)
		{
			K.insert(KV.first);
			CHECK(!KV.second.S().empty(), "%s: %s is empty", Lang, KV.first.c_str());
		}
		for (const FEvent& E : M.Events)
		{
			CHECK(K.count(E.Announce) && K.count(E.Name) && K.count("dev.action.cruise.jump_" + E.Id), "%s: no words for %s", Lang, E.Id.c_str());
		}
		for (const FIsland& I : M.Islands)
		{
			CHECK(K.count(I.Name), "%s: no %s", Lang, I.Name.c_str());
		}
		for (const char* Key : { "cruise.speaker.radio", "cruise.speaker.purser", "cruise.speaker.bosun", "cruise.say.board.hold", "cruise.say.board.std",
			"cruise.say.board.lux", "cruise.say.board.late", "cruise.say.ashore", "cruise.say.ashore_over", "cruise.say.ashore_early", "cruise.say.overboard",
			"cruise.say.stowaway", "dev.group.cruise", "dev.action.cruise.board_now", "dev.action.cruise.next_event", "dev.action.cruise.ashore",
			"dev.action.cruise.report", "dev.action.cruise.my_cabin", "dev.action.cruise.ticket_box", "dev.action.cruise.ticket_hold",
			"dev.action.cruise.ticket_lux", "dev.tp.ship_gangway", "dev.tp.ship_saloon", "dev.tp.ship_bridge" })
		{
			CHECK(K.count(Key), "%s: no %s", Lang, Key);
		}
	}
}

static void TestRoute(const FModel& M)
{
	const double Sail = M.SailTime(M.VoyageAt(TOf(2026, 10, 23, 17)));
	// the belt itself
	for (size_t I = 0; I < M.Stops.size(); ++I)
	{
		const FStop& S = M.Stops[I];
		// (the roads: the ship comes back the way it went, a half turn)
		CHECK(S.Kind == EStop::Orbit || S.Sweep < (S.Kind == EStop::Home ? 1.2 * Pi : Pi), "stop %zu turns %.0f degrees: more than half a circle", I, S.Sweep * 180.0 / Pi);
		CHECK(S.Kind != EStop::Orbit || S.Sweep >= 2.0 * Pi * S.Laps, "stop %zu: laps", I);
	}
	for (size_t I = 1; I < M.Segs.size(); ++I)
	{
		FVec2 A, B;
		double Ha = 0.0, Hb = 0.0;
		M.PathAt(M.Segs[I].S0 - 1e-6, A, Ha);
		M.PathAt(M.Segs[I].S0 + 1e-6, B, Hb);
		CHECK(Len(A - B) < 1e-3 && std::fabs(WrapAngle(Ha - Hb)) < 1e-6, "the path breaks at segment %zu (%.4f m, %.6f rad)", I, Len(A - B), WrapAngle(Ha - Hb));
	}
	{
		FVec2 A, B;
		double Ha = 0.0, Hb = 0.0;
		M.PathAt(0.0, A, Ha);
		M.PathAt(M.Length, B, Hb);
		CHECK(Len(A - B) < 1e-3 && std::fabs(WrapAngle(Ha - Hb)) < 1e-6, "the route does not close (%.3f m, %.6f rad)", Len(A - B), WrapAngle(Ha - Hb));
		const FIsland* Home = M.FindIsland("home");
		CHECK(Home && Len(A - Home->At) < 400.0, "the roads are %.0f m from the home anchorage", Home ? Len(A - Home->At) : -1.0);
	}
	for (size_t I = 1; I < M.Fixes.size(); ++I)
	{
		const double Vc = M.CruiseSpeed(M.Fixes[I - 1], M.Fixes[I]);
		CHECK(Vc >= 0.0 && Vc <= 4.5, "the leg to tv %.0f needs %.2f m/s", M.Fixes[I].T, Vc);
		CHECK(M.Fixes[I].S <= M.Fixes[I - 1].S || Vc >= 1.2, "the leg to tv %.0f crawls at %.2f m/s", M.Fixes[I].T, Vc);
	}

	// every second of a week: no jump of the position, the heading, the speed or the turn rate, also across the phases
	const double From = Sail - 2.0 * 3600.0 - 600.0;
	const double To = Sail + M.TotalSec + M.LandingSec + 600.0;
	FState Prev = M.At(From);
	double MaxV = 0.0, MaxA = 0.0, MaxTurn = 0.0, MaxTurnStep = 0.0, MaxRaw = 0.0, MinY = 1e9, MinX = 1e9, MaxX = 0.0, MaxY = 0.0;
	int Breaks = 0;
	std::vector<double> InSightFrom(M.Islands.size(), 1e18), InSightTo(M.Islands.size(), -1e18), Nearest(M.Islands.size(), 1e18);
	for (double T = From + 1.0; T <= To; T += 1.0)
	{
		const FState St = M.At(T);
		const double Step = Len(St.Pos - Prev.Pos);
		const double Raw = std::fabs(WrapAngle(St.Heading - Prev.Heading));
		if (Step > std::max(St.Speed, Prev.Speed) + 0.01 || Raw > 0.02)
		{
			if (++Breaks <= 5)
			{
				std::printf("FAIL a jump at %s: %.3f m, %.5f rad (%s -> %s)\n", Clock(St.Tv, M).c_str(), Step, Raw, PhaseName(Prev.Phase), PhaseName(St.Phase));
			}
		}
		MaxV = std::max(MaxV, St.Speed);
		MaxA = std::max(MaxA, std::fabs(St.Speed - Prev.Speed));
		MaxTurn = std::max(MaxTurn, std::fabs(St.TurnRate));
		MaxTurnStep = std::max(MaxTurnStep, std::fabs(St.TurnRate - Prev.TurnRate));
		MaxRaw = std::max(MaxRaw, Raw);
		MinY = std::min(MinY, St.Pos.Y);
		MinX = std::min(MinX, St.Pos.X);
		MaxX = std::max(MaxX, St.Pos.X);
		MaxY = std::max(MaxY, St.Pos.Y);
		for (size_t I = 0; I < M.Islands.size(); ++I)
		{
			const double D = Len(St.Pos - M.Islands[I].At);
			Nearest[I] = std::min(Nearest[I], D);
			if (M.InSight(St, M.Islands[I]))
			{
				InSightFrom[I] = std::min(InSightFrom[I], St.Tv);
				InSightTo[I] = std::max(InSightTo[I], St.Tv);
			}
		}
		Prev = St;
	}
	CHECK(Breaks == 0, "%d jumps of the position or the heading", Breaks);
	CHECK(MaxV <= 4.5, "the top speed is %.2f m/s", MaxV);
	CHECK(MaxA <= 0.03, "the hardest acceleration is %.4f m/s2", MaxA);
	CHECK(MaxRaw <= 0.012 && MaxTurn <= 0.012, "the fastest turn is %.4f rad/s (smoothed %.4f)", MaxRaw, MaxTurn);
	CHECK(MaxTurnStep <= 0.0006, "the turn rate steps by %.5f rad/s in a second", MaxTurnStep);
	CHECK(MinY >= 1000.0 && MinX >= 1500.0, "the route comes too near the land or the sea railway (min X %.0f, min Y %.0f)", MinX, MinY);
	CHECK(MaxX <= 70000.0 && MaxY <= 60000.0, "the route leaves the chart (max X %.0f, max Y %.0f)", MaxX, MaxY);
	std::printf("route: %.0f km, top speed %.2f m/s, accel %.4f m/s2, turn %.4f rad/s, box X %.0f..%.0f Y %.0f..%.0f\n", M.Length / 1000.0, MaxV, MaxA, MaxRaw,
		MinX, MaxX, MinY, MaxY);

	// before, between and after the voyages the ship lies where the voyage begins and ends
	const FState Lying = M.At(Sail - 3.0 * DaySec);
	const FState Start = M.At(Sail);
	const FState End = M.At(Sail + M.TotalSec + M.LandingSec - 1.0);
	const FState After = M.At(Sail + M.TotalSec + M.LandingSec + 1.0);
	const FState Winter = M.At(TOf(2027, 2, 1));
	CHECK(Len(Lying.Pos - Start.Pos) < 1e-6 && Len(End.Pos - After.Pos) < 1e-3 && Len(After.Pos - Start.Pos) < 1e-3 && Len(Winter.Pos - Start.Pos) < 1e-6,
		"the roads are one place");
	CHECK(std::fabs(WrapAngle(Lying.Heading - Start.Heading)) < 1e-9 && std::fabs(WrapAngle(End.Heading - After.Heading)) < 1e-6
		&& std::fabs(WrapAngle(After.Heading - Start.Heading)) < 1e-6, "the ship lies on the roads with one heading");
	CHECK(Lying.Speed == 0.0 && Start.Speed == 0.0 && After.Speed == 0.0 && After.TurnRate == 0.0, "no way on at the roads");

	// the islands: met at their events, never run over, in sight only round their events
	for (size_t I = 0; I < M.Islands.size(); ++I)
	{
		const FIsland& Isl = M.Islands[I];
		CHECK(Nearest[I] >= Isl.KeepOff, "the route comes within %.0f m of %s (keep off %.0f)", Nearest[I], Isl.Id.c_str(), Isl.KeepOff);
		if (Isl.Id == "home")
		{
			continue;
		}
		const FStop* Stop = nullptr;
		for (const FStop& S : M.Stops)
		{
			if (S.Island == Isl.Id)
			{
				Stop = &S;
			}
		}
		CHECK(Stop != nullptr, "the route does not call at %s", Isl.Id.c_str());
		if (!Stop)
		{
			continue;
		}
		const FEvent& Ev = M.Events[(size_t)M.FindEvent(Stop->Event)];
		const FState Mid = M.At(Sail + (Ev.T0 + Ev.T1) * 0.5);
		const double D = Len(Mid.Pos - Isl.At);
		CHECK(InSightFrom[I] >= Ev.T0 - 5400.0 && InSightTo[I] <= Ev.T1 + 5400.0, "%s is in sight %s..%s, far from its event", Isl.Id.c_str(),
			Clock(InSightFrom[I], M).c_str(), Clock(InSightTo[I], M).c_str());
		CHECK(InSightFrom[I] <= Ev.T0 && InSightTo[I] >= Ev.T1 - 1.0, "%s is not in sight through its event", Isl.Id.c_str());
		if (Stop->Kind == EStop::Pass)
		{
			CHECK(std::fabs(D - Stop->R) < 60.0 && D >= Isl.Radius + 200.0 && D <= Isl.Radius + 600.0, "%s is %.0f m off at the middle of its event", Isl.Id.c_str(), D);
			CHECK(Mid.Phase == EPhase::Underway && Mid.Speed > 1.0 && Mid.Speed < 2.5, "%s is passed at %.2f m/s", Isl.Id.c_str(), Mid.Speed);
		}
		else if (Stop->Kind == EStop::Orbit)
		{
			double Turned = 0.0;
			double Bearing = 0.0;
			for (double Tv = Ev.T0; Tv <= Ev.T1; Tv += 10.0)
			{
				const FState St = M.At(Sail + Tv);
				const FVec2 Off = St.Pos - Isl.At;
				const double Dist = Len(Off);
				CHECK(std::fabs(Dist - Stop->R) < 1.0, "circling %s the ship is %.0f m from it", Isl.Id.c_str(), Dist);
				const double Now = std::atan2(Off.Y, Off.X);
				Turned += Tv > Ev.T0 ? WrapAngle(Now - Bearing) : 0.0;
				Bearing = Now;
				CHECK(Tv >= Ev.T1 || St.Phase == EPhase::Circling, "circling %s: phase %s", Isl.Id.c_str(), PhaseName(St.Phase));
				CHECK(St.Speed > 1.2 && St.Speed < 3.2, "circling %s at %.2f m/s", Isl.Id.c_str(), St.Speed);
			}
			const double Off = Stop->R - Isl.Radius;
			CHECK(Off >= 400.0 && Off <= 600.0, "%s is circled %.0f m off", Isl.Id.c_str(), Off);
			CHECK(std::fabs(Turned) >= 2.0 * Pi * 2.0 - 0.1, "%s is circled %.1f times", Isl.Id.c_str(), std::fabs(Turned) / (2.0 * Pi));
		}
		else if (Stop->bAshore)
		{
			// the landing: she lies still exactly where the seascape's island stands on the real rock (the city plot)
			CHECK(M.AshoreStop >= 0 && M.AshoreIsland == Isl.Id && !M.AshorePlot.empty(), "the landing stop is the \"ashore\" island");
			for (double Tv = Ev.T0 + 60.0; Tv < Ev.T1 - 30.0; Tv += 60.0)
			{
				const FState St = M.At(Sail + Tv);
				CHECK(St.Phase == EPhase::Anchored && St.Speed == 0.0 && St.TurnRate == 0.0, "at the landing the ship does not move (%s)", Clock(Tv, M).c_str());
				CHECK(M.AshoreMismatch(St) < 0.01, "at the landing the chart island is %.3f m from the real rock", M.AshoreMismatch(St));
			}
			CHECK(std::fabs(WrapAngle(Mid.Heading - M.AshoreHeading)) < 1e-9 && std::fabs(Mid.Heading) < 1e-6, "she lies at the landing with her bow to +X (%.5f deg)", Mid.Heading * 180.0 / Pi);
			CHECK(M.AshoreOffset.Y > 200.0 && M.AshoreOffset.Y < 450.0 && std::fabs(M.AshoreOffset.X) < 80.0, "the rock is abeam to starboard, 200..450 m");
			CHECK(D >= Isl.KeepOff && D <= 450.0, "the landing anchorage is %.0f m from the rock", D);
			// the hand-over: a minute before and after she is metres away (the real rock must not be shown then)
			CHECK(M.AshoreMismatch(M.At(Sail + Ev.T0 - 120.0)) > 1.0 && M.AshoreMismatch(M.At(Sail + Ev.T1 + 120.0)) > 1.0, "under way the copy is not on the rock");
			CHECK(M.AshoreMismatch(M.At(Sail + Ev.T0 - 2.0)) < 1.0, "she comes to rest smoothly: %.3f m two seconds before", M.AshoreMismatch(M.At(Sail + Ev.T0 - 2.0)));
			CHECK(M.AshoreMismatch(M.At(TOf(2026, 10, 21, 12))) > 1e6, "no rock in port");
			// the launch
			CHECK(!M.AshoreOpen(Ev.T0 + 60.0) && M.AshoreOpen(Ev.T0 + M.AshoreLeadSec) && M.AshoreOpen(Ev.T1 - M.AshoreLastSec - 1.0) && !M.AshoreOpen(Ev.T1 - M.AshoreLastSec)
				&& !M.AshoreOpen(Ev.T0 - 1.0) && !M.AshoreOpen(Ev.T1 + 86400.0), "the launch's hours");
			CHECK(Clock(M.AshoreLastLaunch(), M) == "Sat 18:20", "the last launch leaves at %s", Clock(M.AshoreLastLaunch(), M).c_str());
			CHECK(M.AshoreWarnSec.size() == 3 && M.AshoreWarnings(Ev.T1 - 1801.0) == 0 && M.AshoreWarnings(Ev.T1 - 1800.0) == 1 && M.AshoreWarnings(Ev.T1 - 1200.0) == 2
				&& M.AshoreWarnings(Ev.T1 - 700.0) == 3 && M.AshoreWarnings(Ev.T1 + 5.0) == 0, "three warnings from 18:00");
			FHero H;
			H.bAboard = true;
			H.Voyage = M.VoyageAt(Sail);
			CHECK(M.MayBeAshore(Sail + Ev.T0 + 10.0, H) && M.MayBeAshore(Sail + Ev.T1 - 601.0, H) && !M.MayBeAshore(Sail + Ev.T1 - 600.0, H) && !M.MayBeAshore(Sail + Ev.T0 - 1.0, H)
				&& !M.MayBeAshore(Sail + Ev.T0 + 10.0 + WeekSec, H) && !M.MayBeAshore(Sail + 100000.0, H), "he may walk the rock only while she lies there, to the last launch");
			H.bAboard = false;
			CHECK(!M.MayBeAshore(Sail + Ev.T0 + 10.0, H), "a hero who is not a passenger is not on the landing");
		}
		else
		{
			for (double Tv = Ev.T0 + 60.0; Tv < Ev.T1 - 60.0; Tv += 600.0)
			{
				const FState St = M.At(Sail + Tv);
				CHECK(St.Phase == EPhase::Anchored && St.Speed == 0.0 && St.TurnRate == 0.0 && Len(St.Pos - Mid.Pos) < 1e-6, "at anchor the ship does not move");
			}
			CHECK(D >= Isl.KeepOff && D <= Isl.Radius, "the anchorage is %.0f m from %s's middle (in its bay: %.0f..%.0f)", D, Isl.Id.c_str(), Isl.KeepOff, Isl.Radius);
		}
	}
	const FIsland* Home = M.FindIsland("home");
	const FState Sight = M.At(Sail + M.Events[(size_t)M.FindEvent("home_in_sight")].T0);
	CHECK(Home && Len(Sight.Pos - Home->At) < 14000.0 && Len(Sight.Pos - Home->At) > 5000.0, "at home_in_sight home is %.1f km off", Home ? Len(Sight.Pos - Home->At) / 1000.0 : 0.0);
}

static void TestHero(const FModel& M)
{
	const double Board = TOf(2026, 10, 23, 16);
	const double Sail = TOf(2026, 10, 23, 18);
	CHECK(M.Tickets.size() == 3, "three tickets");
	const FTicket* Hold = M.FindTicket("cruise_ticket_hold");
	const FTicket* Std = M.FindTicket("cruise_ticket");
	const FTicket* Lux = M.FindTicket("cruise_ticket_lux");
	CHECK(Hold && Std && Lux, "the three ticket items");
	if (!Hold || !Std || !Lux)
	{
		return;
	}
	CHECK(Hold->ClassIndex == 0 && Hold->Cabin == 15 && Hold->Price == 90, "the hold ticket: cabin 15, 90 roubles");
	CHECK(Std->ClassIndex == 1 && Std->Cabin == 7 && Std->Price == 150, "the standard ticket: cabin 7, 150 roubles");
	CHECK(Lux->ClassIndex == 2 && Lux->Cabin == 11 && Lux->Price == 300, "the lux ticket: cabin 11, 300 roubles");
	CHECK(M.TicketOfClass(2) == Lux && M.TicketOfClass(0) == Hold, "a class's ticket");

	FHero H;
	CHECK(!M.CanBoard(Board - 1.0, H, true) && M.CanBoard(Board, H, true) && !M.CanBoard(Board, H, false) && !M.CanBoard(Sail, H, true), "the gate: boarding hours and a ticket");
	CHECK(!M.CanBoard(TOf(2026, 12, 4, 17), H, true), "no boarding in winter");
	M.Board(H, Board + 3600.0, *Lux);
	CHECK(H.bAboard && H.Class == 2 && H.Cabin == 11 && H.Voyage == M.VoyageAt(Sail), "aboard in lux, cabin 11");
	CHECK(!M.CanBoard(Board + 3700.0, H, true), "aboard already");
	CHECK(!M.MustGoAshore(Board + 3600.0, H) && !M.MustGoAshore(Sail + 40000.0, H) && !M.MustGoAshore(TOf(2026, 10, 25, 19, 59, 59), H), "aboard for the whole voyage");
	CHECK(M.MustGoAshore(TOf(2026, 10, 25, 20, 0), H), "put ashore at 20:00 on Sunday");
	CHECK(M.MustGoAshore(TOf(2026, 10, 30, 17), H) && M.MustGoAshore(TOf(2026, 10, 31, 12), H), "a week slept through: it is another voyage");
	CHECK(M.MustGoAshore(TOf(2026, 10, 22, 12), H) && M.MustGoAshore(TOf(2027, 1, 10), H), "the calendar moved back, or into winter");
	// the save
	FHero Loaded;
	CHECK(Loaded.Load(H.Save()) && Loaded.bAboard == H.bAboard && Loaded.Class == H.Class && Loaded.Cabin == H.Cabin && Loaded.Voyage == H.Voyage
		&& Loaded.Cruises == H.Cruises && Loaded.Save() == H.Save(), "the save round trip (%s)", H.Save().c_str());
	CHECK(!Loaded.Load("") && !Loaded.Load("1 2") && !Loaded.Load("x y z"), "a broken save is refused");
	CHECK(Loaded.Load("1 9 -4 12 -2") && Loaded.Class == 2 && Loaded.Cabin == 0 && Loaded.Cruises == 0, "a bad save is clamped");
	// a load in the middle of the voyage, then at its end
	CHECK(!M.MustGoAshore(TOf(2026, 10, 24, 13), H), "loaded on Saturday: still aboard");
	FHero Early = H;
	CHECK(!M.Ashore(Early, Board + 4000.0) && !Early.bAboard && Early.Cruises == 0 && Early.Cabin == 0, "ashore before it sailed: no voyage counted");
	CHECK(M.Ashore(H, TOf(2026, 10, 25, 20, 0)) && !H.bAboard && H.Cruises == 1, "ashore after the voyage: one voyage sailed");
	CHECK(!M.Ashore(H, TOf(2026, 10, 25, 20, 1)) && H.Cruises == 1 && !M.MustGoAshore(TOf(2026, 10, 26), H), "ashore once");

	// waiting for the next event: from the boarding every event is met in order, then the wait ends
	CHECK(M.WaitMinutes(Sail + 300.0) == 25, "Friday 18:05: 25 minutes to the drill (got %d)", M.WaitMinutes(Sail + 300.0));
	CHECK(M.WaitMinutes(TOf(2026, 10, 24, 1, 30)) == 180, "Saturday 01:30: capped at three hours (got %d)", M.WaitMinutes(TOf(2026, 10, 24, 1, 30)));
	CHECK(M.WaitMinutes(TOf(2026, 10, 25, 19, 30)) == 30, "Sunday 19:30: to the landing's end");
	CHECK(M.WaitMinutes(TOf(2026, 10, 21, 12)) == 0 && M.WaitMinutes(TOf(2026, 12, 21, 12)) == 0, "no wait without a voyage");
	double T = Board + 30.0;
	uint64_t Met = M.EventsOn(T);
	int Steps = 0;
	for (; Steps < 200; ++Steps)
	{
		const int Min = M.WaitMinutes(T);
		if (Min <= 0)
		{
			break;
		}
		CHECK(Min <= 180, "a wait of %d minutes", Min);
		const uint64_t Before = M.EventsOn(T);
		T += Min * 60.0;
		const uint64_t Now = M.EventsOn(T);
		Met |= Now;
		CHECK(Min == 180 || Now != Before || M.At(T).Phase == EPhase::Port, "a wait that changes nothing at %s", Clock(M.At(T).Tv, M).c_str());
	}
	CHECK(Steps > 20 && Steps < 60, "the whole voyage is %d waits", Steps);
	CHECK(M.At(T).Phase == EPhase::Port && T <= TOf(2026, 10, 25, 20, 1), "the waits end with the landing");
	for (size_t I = 0; I < M.Events.size(); ++I)
	{
		CHECK(Met >> I & 1ull, "waiting skips %s", M.Events[I].Id.c_str());
	}
}

static void TestFlags(const FModel& M)
{
	std::set<std::string> Names(M.FlagNames.begin(), M.FlagNames.end());
	CHECK(Names.size() == M.FlagNames.size(), "a flag name comes twice");
	for (const char* Name : { "cruise_season", "cruise_boarding", "cruise_landing", "cruise_aboard", "cruise_day1", "cruise_day2", "cruise_day3", "cruise_underway",
		"cruise_anchor", "cruise_ev_concert1", "cruise_ev_isl_birds", "cruise_ev_wake", "cruise_at_birds", "cruise_at_home", "cruise_class_hold", "cruise_class_std",
		"cruise_class_lux", "cruise_can_board", "cruise_has_ticket", "cruise_port", "cruise_voyage", "cruise_circling", "cruise_night", "cruise_done", "cruise_ashore_stop", "cruise_ashore_open" })
	{
		CHECK(Names.count(Name), "no flag %s", Name);
	}
	const auto Index = [&M](const char* Name) { return (size_t)(std::find(M.FlagNames.begin(), M.FlagNames.end(), Name) - M.FlagNames.begin()); };
	std::vector<uint8_t> F, G;
	FHero Ashore, Aboard;
	Aboard.bAboard = true;
	Aboard.Class = 0;
	Aboard.Cabin = 15;
	Aboard.Voyage = M.VoyageAt(TOf(2026, 10, 23, 17));
	for (double T = TOf(2026, 10, 22, 0); T < TOf(2026, 10, 26, 6); T += 300.0)
	{
		M.Flags(T, Aboard, false, F);
		M.Flags(T, Aboard, false, G);
		CHECK(F == G && F.size() == M.FlagNames.size(), "the flags are a pure function of the time");
		const FState St = M.At(T);
		const bool bVoyage = St.Phase != EPhase::Port && St.Phase != EPhase::Laid;
		CHECK(F[Index("cruise_season")] == 1 && F[Index("cruise_voyage")] == (bVoyage ? 1 : 0), "season and voyage");
		CHECK(F[Index("cruise_day1")] + F[Index("cruise_day2")] + F[Index("cruise_day3")] == (bVoyage ? 1 : 0), "one day flag on a voyage, none in port");
		CHECK(F[Index("cruise_boarding")] + F[Index("cruise_landing")] + F[Index("cruise_underway")] + F[Index("cruise_anchor")] + F[Index("cruise_ashore_stop")] == (bVoyage ? 1 : 0),
			"boarding, under way, at anchor, at the landing stop, landing: one of them on a voyage");
		CHECK(F[Index("cruise_ashore_stop")] == (M.AtAshoreStop(St.Tv) && bVoyage ? 1 : 0) && F[Index("cruise_ashore_open")] == (bVoyage && M.AshoreOpen(St.Tv) ? 1 : 0)
			&& (!F[Index("cruise_ashore_open")] || (F[Index("cruise_ashore_stop")] && F[Index("cruise_ev_isl_lighthouse")] && F[Index("cruise_at_lighthouse")])),
			"the landing's flags at %s", Clock(St.Tv, M).c_str());
		CHECK(F[Index("cruise_port")] == (St.Phase == EPhase::Port || St.Phase == EPhase::Boarding || St.Phase == EPhase::Landing ? 1 : 0)
			&& (!F[Index("cruise_port")] || St.Speed == 0.0), "port: lying on the roads");
		CHECK(!F[Index("cruise_circling")] || F[Index("cruise_underway")], "circling is under way");
		CHECK(F[Index("cruise_class_hold")] == 1 && F[Index("cruise_class_std")] == 0 && F[Index("cruise_class_lux")] == 0 && F[Index("cruise_aboard")] == 1, "the class aboard");
		CHECK(F[Index("cruise_can_board")] == 0 && F[Index("cruise_has_ticket")] == 0, "no ticket, no boarding");
		for (const FEvent& E : M.Events)
		{
			CHECK((F[Index(("cruise_ev_" + E.Id).c_str())] != 0) == M.IsEventOn(T, E.Id), "cruise_ev_%s at %s", E.Id.c_str(), Clock(St.Tv, M).c_str());
		}
		for (const FIsland& I : M.Islands)
		{
			CHECK((F[Index(("cruise_at_" + I.Id).c_str())] != 0) == M.InSight(St, I), "cruise_at_%s", I.Id.c_str());
		}
		M.Flags(T, Ashore, true, G);
		CHECK(G[Index("cruise_can_board")] == (St.Phase == EPhase::Boarding ? 1 : 0) && G[Index("cruise_has_ticket")] == 1 && G[Index("cruise_aboard")] == 0
			&& G[Index("cruise_class_hold")] + G[Index("cruise_class_std")] + G[Index("cruise_class_lux")] == 0, "ashore with a ticket");
	}
	M.Flags(TOf(2026, 10, 23, 17), Ashore, false, F);
	CHECK(F[Index("cruise_at_home")] == 1 && F[Index("cruise_ev_boarding")] == 1 && F[Index("cruise_port")] == 1 && F[Index("cruise_night")] == 0, "Friday 17:00");
	M.Flags(TOf(2026, 10, 24, 2), Ashore, false, F);
	CHECK(F[Index("cruise_night")] == 1 && F[Index("cruise_at_home")] == 0, "Saturday 02:00: night at sea");
	M.Flags(TOf(2026, 10, 27, 2), Ashore, false, F);
	CHECK(F[Index("cruise_night")] == 0 && F[Index("cruise_at_home")] == 1 && F[Index("cruise_port")] == 1, "Tuesday 02:00: on the roads");
	M.Flags(TOf(2026, 12, 24, 12), Ashore, true, F);
	int On = 0;
	for (const uint8_t B : F)
	{
		On += B;
	}
	CHECK(On == 1 && F[Index("cruise_has_ticket")] == 1, "in winter only the ticket in the pocket is left (%d flags on)", On);
	Ashore.Cruises = 2;
	M.Flags(TOf(2026, 12, 24, 12), Ashore, false, F);
	CHECK(F[Index("cruise_done")] == 1, "cruise_done after a voyage");
}

int main(int Argc, char** Argv)
{
	const std::string Text = Read("unreal/Lyudmilovsk/Content/Cruise/cruise.json");
	FModel M;
	if (!M.Load(Text))
	{
		std::printf("FAIL load: %s\n", M.LastError.c_str());
		return 1;
	}
	const std::string Mode = Argc > 1 ? Argv[1] : "test";
	if (Mode == "table")
	{
		Table(M);
		return 0;
	}
	if (Mode == "at" && Argc > 3)
	{
		int Y = 2026, Mo = 10, D = 23, H = 18, Mi = 0;
		std::sscanf(Argv[2], "%d-%d-%d", &Y, &Mo, &D);
		std::sscanf(Argv[3], "%d:%d", &H, &Mi);
		const double T = TOf(Y, Mo, D, H, Mi);
		const FState St = M.At(T);
		double Sec = 0.0;
		const int Next = M.NextEvent(T, Sec);
		std::printf("%s day %d tv %.0f at (%.0f, %.0f) heading %.1f deg, %.2f m/s, turn %.5f rad/s; next %s in %.0f min; wait %d min\n", PhaseName(St.Phase), St.Day,
			St.Tv, St.Pos.X, St.Pos.Y, St.Heading * 180.0 / Pi, St.Speed, St.TurnRate, Next >= 0 ? M.Events[(size_t)Next].Id.c_str() : "-", Sec / 60.0, M.WaitMinutes(T));
		return 0;
	}
	TestCalendar(M);
	TestEvents(M);
	TestRoute(M);
	TestHero(M);
	TestFlags(M);
	// determinism: a second model from the same file answers the same
	FModel N;
	CHECK(N.Load(Text) && N.Length == M.Length && N.Fixes.size() == M.Fixes.size() && N.FlagNames == M.FlagNames, "a second load is the same model");
	for (double T = TOf(2026, 10, 23, 15); T < TOf(2026, 10, 25, 21); T += 977.0)
	{
		const FState A = M.At(T);
		const FState B = N.At(T);
		CHECK(A.Pos.X == B.Pos.X && A.Pos.Y == B.Pos.Y && A.Heading == B.Heading && A.Speed == B.Speed && A.TurnRate == B.TurnRate && A.Phase == B.Phase,
			"two models differ at %.0f", T);
	}
	FModel Bad;
	CHECK(!Bad.Load("{") && !Bad.Load("{}") && !Bad.LastError.empty(), "a broken file is refused with a reason");
	std::printf("%s: %d checks, %d failed\n", Fails ? "FAIL" : "ok", Checks, Fails);
	return Fails ? 1 : 0;
}
