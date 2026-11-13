// Offline checks of Public/North/NorthModel.h (tools/north_sim/run.sh model): wake ranges with their margin, mud
// bubbles (timing, where they pop, how often), the camp fire's schedule, the vents' column heights.
#include "NorthModel.h"

#include <cstdio>
#include <cstdlib>

using namespace LyudNorthModel;

static int Fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); Fails++; } } while (0)

int main()
{
	// ranges: wakes inside 300 m, stays awake to 325, sleeps beyond
	const FRanges R;
	CHECK(!NextAwake(false, 310.f, R));
	CHECK(NextAwake(false, 290.f, R));
	CHECK(NextAwake(true, 320.f, R));
	CHECK(!NextAwake(true, 330.f, R));
	CHECK(NextNear(false, 60.f, R) && !NextNear(false, 75.f, R) && NextNear(true, 75.f, R));

	// bubbles: a minute of a pot at 30 Hz pops 15..60 times, never outside 70 % of the pool, scale in 0..1.1
	uint32_t Seed = 12345u;
	FBubble B;
	NewBubble(B, Seed);
	int Pops = 0;
	float MaxScale = 0.f;
	for (int I = 0; I < 30 * 60; I++)
	{
		if (StepBubble(B, 1.f / 30.f, Seed))
		{
			Pops++;
			CHECK(B.U * B.U + B.V * B.V <= 0.7f * 0.7f + 1e-4f);
		}
		const float S = BubbleScale(B);
		CHECK(S >= 0.f && S <= 1.1f);
		MaxScale = S > MaxScale ? S : MaxScale;
	}
	std::printf("bubbles: %d pops a minute, biggest %.2f\n", Pops, MaxScale);
	CHECK(Pops >= 15 && Pops <= 60);

	// the fire: lit at dusk, burns through midnight, out after OutHour, not by day, no flicker at the threshold
	CHECK(!FireScheduled(false, 10.f, 15.f, 2.f, 1.5f));
	CHECK(FireScheduled(false, 1.f, 21.f, 2.f, 1.5f));
	CHECK(FireScheduled(true, 2.5f, 21.f, 2.f, 1.5f));
	CHECK(!FireScheduled(false, 2.5f, 21.f, 2.f, 1.5f));
	CHECK(FireScheduled(true, -10.f, 0.5f, 2.f, 1.5f));
	CHECK(!FireScheduled(true, -10.f, 2.f, 2.f, 1.5f));
	CHECK(!FireScheduled(false, -5.f, 5.f, 2.f, 1.5f));

	// vents: the jet alone 6..15 m over the sizes north.json uses (0.7..1.8)
	for (float S = 0.7f; S <= 1.81f; S += 0.1f)
	{
		const float H = VentColumnM(S);
		CHECK(H >= 6.f && H <= 15.f);
	}
	std::printf("vent column %.1f m (size 0.7) .. %.1f m (size 1.8)\n", VentColumnM(0.7f), VentColumnM(1.8f));
	CHECK(SteamDensity(15.f, 0.6f) > 0.99f && SteamDensity(15.f, 0.6f) < 1.01f);
	CHECK(SteamDensity(-5.f, 1.f) > 2.f);
	CHECK(HotRadiusM(3, 3.2f) > 2.9f && HotRadiusM(3, 3.2f) < 3.2f);

	std::printf(Fails ? "north model: %d FAILED\n" : "north model: ok\n", Fails);
	return Fails ? 1 : 0;
}
