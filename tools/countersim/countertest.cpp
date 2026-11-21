// The counters' rules (Private/People/CounterModel.h): the note the hero pays with, his change, his place in a line.
#include "CounterModel.h"

#include <cstdio>

static int Fails = 0;
#define CHECK(Cond, ...) do { std::printf((Cond) ? "  ok   " : "  FAIL "); std::printf(__VA_ARGS__); std::printf("\n"); Fails += (Cond) ? 0 : 1; } while (0)

int main()
{
	using namespace LyudCounter;
	CHECK(PayWith(7, 100) == 10 && Change(7, 100) == 3, "7 rub. out of 100: a ten, 3 back (%d, %d)", PayWith(7, 100), Change(7, 100));
	CHECK(PayWith(5, 100) == 0 && Change(5, 100) == 0, "5 rub.: a five is exactly it, no change");
	CHECK(PayWith(26, 100) == 50 && Change(26, 100) == 24, "26 rub.: a fifty, 24 back");
	CHECK(PayWith(26, 40) == 0, "26 rub. out of a purse of 40: no fifty in it, counted out exactly");
	CHECK(PayWith(60, 100) == 100 && Change(60, 100) == 40, "60 rub.: a hundred, 40 back");
	CHECK(PayWith(150, 300) == 0, "150 rub.: no single note covers it, counted out");
	CHECK(PayWith(0, 100) == 0 && PayWith(10, 5) == 0, "nothing bought, or more than he has: nothing paid");
	CHECK(PayWith(2, 100) == 3 && Change(2, 100) == 1, "2 rub.: a three-rouble note, 1 back");
	bool bCovers = true;
	for (int T = 1; T <= 100; T++)
	{
		const int N = PayWith(T, 100);
		bCovers &= N == 0 || (N > T && Change(T, 100) == N - T && Change(T, 100) > 0);
	}
	CHECK(bCovers, "every price 1..100: the note covers it and the change is the difference");

	const std::vector<int> Line = { 12, -1, -2, 40 };
	CHECK(HeroSpot(Line) == 2 && Ahead(Line, 2) == 1, "a line 12 . hero 40: he is third, one ahead of him");
	CHECK(HeroSpot({ 3, 4 }) == -1 && Ahead({ -2, 5 }, 0) == 0, "not in the line; at its front nobody is ahead");

	std::printf(Fails ? "%d failed\n" : "all ok\n", Fails);
	return Fails ? 1 : 0;
}
