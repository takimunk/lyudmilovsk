// The street's day by the weather and the season (Private/People/StreetLifeModel.h, docs/street_life.md), outside the engine.
//   tools/streetsim/run.sh        the checks, then a year of the conditions' days (how many days each scene has)
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/StreetLifeModel.h"

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace LyudStreet;

static int Failures = 0;
#define CHECK(Cond, ...) do { if (!(Cond)) { Failures++; std::printf("FAIL: "); } else std::printf("  ok: "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

static bool On(uint32_t B, ECond C) { return (B & C) != 0; }

// a made-up climate: the day's warmest at 15, the year's at the end of July; rain on the days the die says, from RainFrom
struct FClimate
{
	float Mean = 12.f, Swing = 11.f, Daily = 4.f;
	float RainFrom = -1.f, RainTo = -1.f;
	float Cloud = 0.3f;
	FSky At(int DayOfYear, float H) const
	{
		FSky S;
		S.TempC = Mean + Swing * std::cos((DayOfYear - 205) * 2.f * 3.14159f / 365.f) + Daily * std::cos((H - 15.f) * 3.14159f / 12.f);
		S.Cloud = Cloud;
		S.Rain = (RainFrom >= 0.f && H >= RainFrom && H < RainTo) ? 1.5f : 0.f;
		S.Snow = S.TempC < 0.f ? 1.f : 0.f;
		return S;
	}
};

static int DayOfYear(int M, int D)
{
	static const int Cum[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
	return Cum[M - 1] + D;
}

static uint32_t At(const FClimate& K, int Y, int M, int D, float H, FDayPlan* Out = nullptr)
{
	const int Wd = WeekdayOf(Y, M, D), Doy = DayOfYear(M, D);
	const FDayPlan P = PlanDay(Y, M, D, Wd, [&](float Hh) { return K.At(Doy, Hh); });
	if (Out) *Out = P;
	FNow N;
	N.Year = Y; N.Month = M; N.Day = D; N.Weekday = Wd; N.Hour = H;
	N.Sky = K.At(Doy, H);
	return Conditions(N, P);
}

int main()
{
	std::printf("the street's conditions (StreetLifeModel.h):\n");
	// the calendar
	CHECK(WeekdayOf(2026, 11, 27) == 4, "27 November 2026 is a Friday (%d)", WeekdayOf(2026, 11, 27));
	CHECK(WeekdayOf(2026, 10, 20) == 1, "the game's first day, 20 October 2026, is a Tuesday (%d)", WeekdayOf(2026, 10, 20));
	CHECK(SubbotnikDay(2027) == 17 && WeekdayOf(2027, 4, SubbotnikDay(2027)) == 5, "the subbotnik of 2027: Saturday 17 April (%d)", SubbotnikDay(2027));
	for (int Y = 2026; Y <= 2040; Y++)
	{
		const int D = SubbotnikDay(Y);
		if (WeekdayOf(Y, 4, D) != 5 || D < 15 || D > 21) { CHECK(false, "the subbotnik of %d is the third Saturday (%d April)", Y, D); }
	}
	CHECK(CondIndex("street_hot") == 0 && CondIndex("trunks_white") == 10 && CondIndex("nope") == -1, "condition names and indices agree");

	FClimate Summer;     // ~27 C in a July afternoon
	Summer.Mean = 14.f; Summer.Swing = 10.f;
	FDayPlan P;
	// the barrel and the cart
	uint32_t B = At(Summer, 2027, 7, 24, 14.f, &P);
	CHECK(P.bHotDay && On(B, Hot), "a hot July Saturday at 14: the barrel is out (%.1f C)", P.HotMax);
	CHECK(!On(At(Summer, 2027, 7, 24, 9.5f), Hot) && !On(At(Summer, 2027, 7, 24, 19.2f), Hot), "not before ten nor after seven");
	FClimate Shower = Summer; Shower.RainFrom = 15.f; Shower.RainTo = 15.5f;
	CHECK(On(At(Shower, 2027, 7, 24, 14.f), Hot) && !On(At(Shower, 2027, 7, 24, 15.2f), Hot) && On(At(Shower, 2027, 7, 24, 16.f), Hot),
		"a shower at three: packed up while it lasts, out again after");
	CHECK(!On(At(Summer, 2027, 10, 2, 14.f), Hot), "October: no barrel, whatever the weather");
	FClimate Cool = Summer; Cool.Mean = 6.f;
	CHECK(!On(At(Cool, 2027, 7, 24, 14.f, &P), Hot), "a cool July day (%.1f C): no barrel", P.HotMax);
	FClimate WetNoon = Summer; WetNoon.RainFrom = 13.f; WetNoon.RainTo = 15.f;
	CHECK(!At(WetNoon, 2027, 7, 24, 11.f, &P) || !P.bHotDay, "a hot day with rain at two in the forecast: the barrel stays in");
	// the washing
	FClimate Fine = Summer;
	At(Fine, 2027, 6, 7, 9.f, &P);     // a Monday
	CHECK(P.bHang && P.TakeAt == (P.HotMax > 22.f ? 17.f : 18.f), "a fine wash-day Monday: the washing goes up, in at %.2f", P.TakeAt);
	CHECK(On(At(Fine, 2027, 6, 7, 9.2f), LaundryHang) && On(At(Fine, 2027, 6, 7, 12.f), LaundryOut) && !On(At(Fine, 2027, 6, 7, 12.f), LaundryHang),
		"hung at nine, on the line at noon");
	FClimate Later = Fine; Later.RainFrom = 14.f; Later.RainTo = 16.f;
	At(Later, 2027, 6, 7, 9.f, &P);
	CHECK(P.bHang && std::fabs(P.TakeAt - 13.25f) < 0.01f, "rain at two: taken in at a quarter past one (%.2f)", P.TakeAt);
	CHECK(On(At(Later, 2027, 6, 7, 13.3f), LaundryTake) && On(At(Later, 2027, 6, 7, 13.3f), LaundryOut) && !On(At(Later, 2027, 6, 7, 14.5f), LaundryOut),
		"being taken in at 13:20, gone by the rain");
	FClimate WetMorning = Fine; WetMorning.RainFrom = 8.f; WetMorning.RainTo = 10.f;
	At(WetMorning, 2027, 6, 7, 9.f, &P);
	CHECK(!P.bHang && !On(At(WetMorning, 2027, 6, 7, 12.f), LaundryOut), "a wet morning: no washing out today");
	CHECK(!PlanDay(2027, 6, 8, WeekdayOf(2027, 6, 8), [&](float H) { return Fine.At(DayOfYear(6, 8), H); }).bHang, "Tuesday is not a wash day");
	CHECK(!PlanDay(2027, 12, 6, WeekdayOf(2027, 12, 6), [&](float H) { return Fine.At(DayOfYear(12, 6), H); }).bHang, "nor any day in December");
	// the yards
	CHECK(On(At(Fine, 2027, 6, 8, 11.f), Grannies) && !On(At(Fine, 2027, 6, 8, 14.f), Grannies) && On(At(Fine, 2027, 6, 8, 17.f), Grannies),
		"the grandmothers: late morning and evening, not in the heat of the day");
	CHECK(On(At(Fine, 2027, 6, 8, 19.f), Dominoes) && !On(At(Fine, 2027, 6, 8, 16.f), Dominoes) && !On(At(Fine, 2027, 6, 8, 22.f), Dominoes),
		"dominoes: from half past five to half past nine");
	FClimate Rainy = Fine; Rainy.RainFrom = 0.f; Rainy.RainTo = 24.f;
	CHECK(!On(At(Rainy, 2027, 6, 8, 19.f), Dominoes) && !On(At(Rainy, 2027, 6, 8, 11.f), Grannies), "not in the rain");
	// the seasons
	FClimate Autumn = Fine;
	CHECK(On(At(Autumn, 2027, 10, 12, 9.f), Raking) && On(At(Autumn, 2027, 10, 12, 15.f), LeafPiles) && !On(At(Autumn, 2027, 10, 12, 15.f), Raking),
		"October: raked in the morning, the piles lie till evening");
	CHECK(!On(At(Autumn, 2027, 8, 12, 9.f), Raking) && !On(At(Autumn, 2027, 11, 20, 9.f), Raking), "not in August, not after mid-November");
	const int Sd = SubbotnikDay(2027);
	CHECK(On(At(Fine, 2027, 4, Sd, 10.f), Subbotnik) && !On(At(Fine, 2027, 4, Sd, 14.f), Subbotnik) && !On(At(Fine, 2027, 4, Sd - 7, 10.f), Subbotnik),
		"the subbotnik: that Saturday's morning only");
	CHECK(!On(At(Fine, 2027, 4, Sd, 9.5f), TrunksWhite) && On(At(Fine, 2027, 4, Sd, 10.5f), TrunksWhite) && On(At(Fine, 2027, 8, 1, 12.f), TrunksWhite)
		&& !On(At(Fine, 2027, 11, 5, 12.f), TrunksWhite) && !On(At(Fine, 2027, 4, Sd - 1, 12.f), TrunksWhite), "the trunks white from the subbotnik to October");
	CHECK(On(At(Fine, 2027, 6, 1, 6.f), Sweep) && !On(At(Fine, 2027, 1, 12, 6.f), Sweep), "the dawn sweep in June, not in January");

	// a year of days: every scene comes and goes (none always on, none never)
	std::printf("a year (a fair climate, a shower every fifth afternoon, a wet morning every seventh):\n");
	static const int Len[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	int Days[NumConds] = {};
	for (int M = 1; M <= 12; M++)
		for (int D = 1; D <= Len[M - 1]; D++)
		{
			FClimate K = Fine;
			const int Doy = DayOfYear(M, D);
			if (Doy % 5 == 0) { K.RainFrom = 15.f; K.RainTo = 17.f; }
			if (Doy % 7 == 0) { K.RainFrom = 7.f; K.RainTo = 9.5f; }
			uint32_t Any = 0;
			for (float H = 0.f; H < 24.f; H += 0.25f) Any |= At(K, 2027, M, D, H);
			for (int I = 0; I < NumConds; I++) Days[I] += (Any >> I) & 1;
		}
	for (int I = 0; I < NumConds; I++)
	{
		std::printf("    %-14s %3d days\n", CondName(I), Days[I]);
		if (Days[I] == 0 || Days[I] == 365) CHECK(false, "%s comes and goes over the year (%d days)", CondName(I), Days[I]);
	}
	CHECK(Days[9] == 1, "one subbotnik a year (%d)", Days[9]);
	std::printf(Failures ? "%d FAILED\n" : "all ok\n", Failures);
	return Failures ? 1 : 0;
}
