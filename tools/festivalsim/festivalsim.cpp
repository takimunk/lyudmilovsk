// The town's festival calendar outside the engine (Private/Festival/FestivalCalendar.h, docs/festivals.md).
//   festivalsim test                 checks of the date rules and of festivals.json
//   festivalsim year [YYYY]          every festival day of a year with its program
//   festivalsim at YYYY-MM-DD HH.H   what is on at a moment
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Festival/FestivalCalendar.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

using namespace LyudFest;

static int Fails = 0;
#define CHECK(C, ...) do { if (!(C)) { Fails++; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static std::string ReadFile(const char* Path)
{
	std::ifstream F(Path);
	std::stringstream S;
	S << F.rdbuf();
	return S.str();
}

static const char* DayName(int W)
{
	static const char* N[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
	return N[W % 7];
}

static std::string Hours(float H)
{
	char B[16];
	const int M = (int)(H * 60.f + 0.5f);
	std::snprintf(B, sizeof(B), "%02d:%02d", (M / 60) % 24, M % 60);
	return B;
}

static void PrintAt(const FCalendar& C, const FDate& D, float H)
{
	std::string Off;
	const bool bOff = C.IsDayOff(D, &Off);
	std::printf("%04d-%02d-%02d %s %s%s\n", D.Y, D.M, D.D, DayName(Weekday(D)), Hours(H).c_str(), bOff ? (" day off (" + Off + ")").c_str() : "");
	for (const FActive& A : C.At(D, H))
	{
		const FFestival& F = C.Festivals[A.Festival];
		std::printf("  %-15s since %04d-%02d-%02d rel %.2f h", F.Id.c_str(), A.Start.Y, A.Start.M, A.Start.D, A.Rel);
		if (A.Phase >= 0)
		{
			const FPhase& P = F.Phases[A.Phase];
			std::printf("  phase %s at %s (%s, pull %.1f) %.0f%%", P.Id.c_str(), P.Venue.c_str(), P.Mode.c_str(), P.Pull, A.PhaseT * 100.f);
		}
		else
		{
			std::printf("  decorations only");
		}
		std::printf("\n");
	}
}

static int Test(FCalendar& C)
{
	// the date rules
	CHECK(Weekday({ 2026, 10, 20 }) == 1, "2026-10-20 is a Tuesday");
	CHECK(Weekday({ 2026, 11, 6 }) == 4, "2026-11-06 is a Friday");
	CHECK(CivilFromDays(DaysFromCivil({ 2028, 2, 29 })) == (FDate{ 2028, 2, 29 }), "round trip");
	CHECK(OrthodoxEaster(2026) == (FDate{ 2026, 4, 12 }), "Orthodox Easter 2026");
	CHECK(OrthodoxEaster(2027) == (FDate{ 2027, 5, 2 }), "Orthodox Easter 2027");
	CHECK(OrthodoxEaster(2025) == (FDate{ 2025, 4, 20 }), "Orthodox Easter 2025");
	CHECK(WesternEaster(2026) == (FDate{ 2026, 4, 5 }), "Western Easter 2026");
	CHECK(WesternEaster(2027) == (FDate{ 2027, 3, 28 }), "Western Easter 2027");
	FDate N;
	CHECK(NthWeekday(2026, 10, 4, 5, N) && N == (FDate{ 2026, 10, 24 }), "4th Saturday of October 2026");
	CHECK(NthWeekday(2026, 10, -1, 5, N) && N == (FDate{ 2026, 10, 31 }), "last Saturday of October 2026");
	CHECK(!NthWeekday(2026, 2, 5, 0, N), "no 5th Monday in February 2026");

	// the data
	CHECK(C.Festivals.size() >= 10, "festivals.json has %d festivals", (int)C.Festivals.size());
	const int City = C.Find("sea_festival"), Market = C.Find("weekend_market"), NewYear = C.Find("new_year"), Mas = C.Find("maslenitsa");
	const int Mid = C.Find("midsummer"), Snow = C.Find("first_snow");
	CHECK(City >= 0 && Market >= 0 && NewYear >= 0 && Mas >= 0 && Mid >= 0 && Snow >= 0, "the main festivals are there");
	CHECK(C.StartsOn(City, { 2026, 10, 24 }), "the sea festival is 2026-10-24");
	CHECK(C.StartsOn(Mas, { 2026, 2, 22 }), "Maslenitsa's Sunday 2026-02-22 (Easter - 49)");
	CHECK(C.StartsOn(Market, { 2026, 10, 24 }) && C.StartsOn(Market, { 2026, 10, 31 }) && !C.StartsOn(Market, { 2026, 10, 25 }), "the market every Saturday");
	// the first week of the game: the sea festival comes on its fifth day
	bool bCityFirst = false;
	for (const auto& U : C.Upcoming({ 2026, 10, 20 }, 7))
	{
		bCityFirst |= U.second == City;
	}
	CHECK(bCityFirst, "the sea festival is in the game's first week");
	// the sea festival program
	{
		auto A = C.At({ 2026, 10, 24 }, 11.f);
		CHECK(!A.empty() && A[0].Festival == City, "the sea festival is first (priority) at 11:00");
		CHECK(!A.empty() && C.Festivals[City].Phases[A[0].Phase].Id == "fair", "11:00 the fair");
		A = C.At({ 2026, 10, 24 }, 21.7f);
		CHECK(!A.empty() && A[0].Festival == City && C.Festivals[City].Phases[A[0].Phase].Id == "fireworks", "21:42 the fireworks");
		A = C.At({ 2026, 10, 25 }, 0.25f);
		CHECK(!A.empty() && A[0].Festival == City && C.Festivals[City].Phases[A[0].Phase].Id == "dance", "00:15 the next night: still dancing");
		A = C.At({ 2026, 10, 23 }, 20.f);
		CHECK(!A.empty() && A[0].Festival == City && A[0].Phase < 0, "the evening before: decorations up, nothing on yet");
		A = C.At({ 2026, 10, 25 }, 3.f);
		bool bCity = false;
		for (const FActive& X : A) bCity |= X.Festival == City;
		CHECK(!bCity, "03:00 the next night: it is over");
		CHECK(C.IsDayOff({ 2026, 10, 24 }), "the sea festival is a day off");
	}
	// New Year over midnight, days off on the 1st and 2nd
	{
		auto A = C.At({ 2027, 1, 1 }, 0.1f);
		CHECK(!A.empty() && A[0].Festival == NewYear && C.Festivals[NewYear].Phases[A[0].Phase].Id == "fireworks", "00:06 on the 1st: fireworks");
		CHECK(C.IsDayOff({ 2027, 1, 1 }) && C.IsDayOff({ 2027, 1, 2 }) && !C.IsDayOff({ 2027, 1, 4 }), "1-2 January days off");
		A = C.At({ 2026, 12, 25 }, 15.f);
		bool bFair = false;
		for (const FActive& X : A) bFair |= C.Festivals[X.Festival].Id == "winter_fair" && X.Phase >= 0;
		CHECK(bFair, "the winter fair on 25 December");
	}
	// midsummer: people go to see the sunrise
	{
		auto A = C.At({ 2026, 6, 24 }, 4.f);
		CHECK(!A.empty() && A[0].Festival == Mid && C.Festivals[Mid].Phases[A[0].Phase].Id == "sunrise", "04:00 on 24 June: the sunrise");
	}
	// a trigger
	CHECK(!C.StartsOn(Snow, { 2026, 11, 20 }), "no snow yet");
	C.Fire("first_snow", { 2026, 11, 19 });
	CHECK(C.StartsOn(Snow, { 2026, 11, 20 }), "the day after the first snow");
	// every festival happens within a year, every phase is sane
	for (int F = 0; F < (int)C.Festivals.size(); F++)
	{
		const FFestival& Fe = C.Festivals[F];
		FDate D;
		CHECK(Fe.Rule == "trigger" || C.Next(F, { 2026, 10, 20 }, D), "%s happens", Fe.Id.c_str());
		CHECK(!Fe.Phases.empty(), "%s has a program", Fe.Id.c_str());
		for (const FPhase& P : Fe.Phases)
		{
			CHECK(P.To > P.From && P.Pull > 0.f && P.Pull <= 2.f, "%s.%s hours/pull", Fe.Id.c_str(), P.Id.c_str());
			CHECK(P.Mode == "mingle" || P.Mode == "watch" || P.Mode == "sit" || P.Mode == "fair", "%s.%s mode %s", Fe.Id.c_str(), P.Id.c_str(), P.Mode.c_str());
		}
	}
	std::printf(Fails ? "%d FAILED\n" : "all festival checks pass\n", Fails);
	return Fails ? 1 : 0;
}

int main(int Argc, char** Argv)
{
	const std::string Text = ReadFile("unreal/Lyudmilovsk/Content/PeopleData/festivals.json");
	FCalendar C;
	if (!C.Load(Text))
	{
		std::printf("%s\n", C.LastError.c_str());
		return 2;
	}
	const std::string Cmd = Argc > 1 ? Argv[1] : "test";
	if (Cmd == "test")
	{
		return Test(C);
	}
	if (Cmd == "year")
	{
		const int Y = Argc > 2 ? std::atoi(Argv[2]) : 2026;
		for (const auto& U : C.Upcoming({ Y, 1, 1 }, 365))
		{
			const FFestival& F = C.Festivals[U.second];
			if (F.Rule == "weekly")
			{
				continue;
			}
			std::printf("%04d-%02d-%02d %s %-15s %d day(s)%s:", U.first.Y, U.first.M, U.first.D, DayName(Weekday(U.first)), F.Id.c_str(), F.Days, F.bHoliday ? " holiday" : "");
			for (const FPhase& P : F.Phases)
			{
				std::printf(" %s %s-%s@%s", P.Id.c_str(), Hours(P.From).c_str(), Hours(P.To).c_str(), P.Venue.c_str());
			}
			std::printf("\n");
		}
		return 0;
	}
	if (Cmd == "at" && Argc > 3)
	{
		FDate D;
		std::sscanf(Argv[2], "%d-%d-%d", &D.Y, &D.M, &D.D);
		PrintAt(C, D, (float)std::atof(Argv[3]));
		return 0;
	}
	std::printf("festivalsim test | year [YYYY] | at YYYY-MM-DD HH.H\n");
	return 1;
}
