// Checks of the Mayak lighthouse's rules (docs/lighthouse.md): tools/lighthousesim/run.sh test
#include "LighthouseModel.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

using namespace LyudLighthouse;

static int Fails = 0, Checks = 0;

static void Check(bool bOk, const char* What, double A = 0.0, double B = 0.0)
{
	++Checks;
	if (!bOk)
	{
		++Fails;
	}
	std::printf("  %s  %s (%.3f, %.3f)\n", bOk ? "ok  " : "FAIL", What, A, B);
}

static void Test()
{
	// October at 55 N: sunrise ~8.0, sunset ~17.2 (the game's start)
	const float Rise = 8.f, Set = 17.2f;
	Check(!LampTime(12.f, Rise, Set, 30000.f), "no lamp at noon in clear air");
	Check(LampTime(17.1f, Rise, Set, 30000.f), "lit a little before sunset", LitAt(Set));
	Check(LampTime(23.f, Rise, Set, 30000.f) && LampTime(3.f, Rise, Set, 30000.f), "burns all night");
	Check(LampTime(8.1f, Rise, Set, 30000.f) && !LampTime(8.3f, Rise, Set, 30000.f), "put out a little after sunrise", OutAt(Rise));
	Check(LampTime(12.f, Rise, Set, 600.f), "burns by day in thick fog");
	Check(!LampTime(12.f, Rise, Set, 1500.f), "not in mist by day");
	// June: sunrise ~3.7, sunset ~21.9
	Check(!LampTime(21.f, 3.7f, 21.9f, 30000.f) && LampTime(22.f, 3.7f, 21.9f, 30000.f), "a June night starts late");

	float L = 0.f;
	for (int I = 0; I < 10; I++)
	{
		L = StepLevel(L, true, 1.f);
	}
	Check(L > 0.3f && L < 0.5f, "the lamp warms up slowly", L);
	for (int I = 0; I < 40; I++)
	{
		L = StepLevel(L, true, 1.f);
	}
	Check(L == 1.f, "and stays full", L);
	for (int I = 0; I < 15; I++)
	{
		L = StepLevel(L, false, 1.f);
	}
	Check(L == 0.f, "and goes out faster than it warms", L);

	Check(std::fabs(TurnDeg(0.0)) < 1e-3f && std::fabs(TurnDeg(TurnPeriodS / 4.0) - 90.f) < 1e-2f, "the lens turns at a steady rate", TurnDeg(TurnPeriodS / 4.0));
	Check(std::fabs(TurnDeg(TurnPeriodS * 1000.0 + 5.0) - TurnDeg(5.0)) < 0.05f, "the same flash after many turns (no drift)");
	Check(std::fabs(FlashPeriodS - 10.f) < 1e-4f, "a white flash every 10 s (two panels)", FlashPeriodS);

	// the beam and the weather
	const FBeam Clear = BeamFor(30000.f, 0.f, 1.f), Haze = BeamFor(8000.f, 0.f, 1.f), Mist = BeamFor(2000.f, 0.f, 1.f), Fog = BeamFor(300.f, 0.f, 1.f), Thick = BeamFor(80.f, 0.f, 1.f);
	Check(Clear.LengthM == BeamLongM && Haze.LengthM == BeamLongM, "a clear night: the long beam", Clear.LengthM);
	Check(Haze.Card > Clear.Card, "haze shows the beam better than clean air", Haze.Card, Clear.Card);
	Check(Fog.LengthM < 200.f && Fog.LengthM < Mist.LengthM, "fog shortens it", Fog.LengthM, Mist.LengthM);
	Check(Fog.Card < Haze.Card && Thick.Card < Fog.Card, "fog dims it", Fog.Card, Thick.Card);
	Check(Thick.LengthM >= BeamShortM && Thick.Card > 0.f, "even thick fog keeps a glow round the lantern", Thick.LengthM, Thick.Card);
	Check(Fog.Spot < Clear.Spot, "the spot light dims in fog too", Fog.Spot, Clear.Spot);
	Check(BeamFor(30000.f, 1.f, 1.f).Card < Clear.Card, "rain dims the card a little");
	Check(BeamFor(8000.f, 0.f, 0.f).Card == 0.f && BeamFor(8000.f, 0.f, 0.f).Spot == 0.f, "nothing while the lamp is out");

	Check(FogHorn(30000.f) == 0.f && FogHorn(5000.f) == 0.f, "no horn in clear air");
	Check(FogHorn(800.f) == 1.f, "the horn in fog", FogHorn(800.f));
	Check(FogHorn(1800.f) > 0.f && FogHorn(1800.f) < 1.f, "it comes in between", FogHorn(1800.f));

	// the flash as the panel sweeps past
	Check(OffBeam(0.f, 0.f) == 0.f && OffBeam(0.f, 180.f) == 0.f, "both panels flash (0 and 180)");
	Check(std::fabs(OffBeam(10.f, 100.f) - 90.f) < 1e-3f && std::fabs(OffBeam(350.f, 20.f) - 30.f) < 1e-3f, "the angle off the nearer beam", OffBeam(350.f, 20.f));
	Check(GlassGlow(0.f, 1.f) > 0.99f && GlassGlow(10.f, 1.f) < 0.3f && GlassGlow(10.f, 1.f) > 0.2f, "a narrow flash over a steady glow", GlassGlow(10.f, 1.f));
	Check(GlassGlow(0.f, 0.f) == 0.f, "a cold lamp has no glow");
	// a flash seen from one place: how long the glass stays bright (above half) in one sweep
	int Bright = 0;
	for (int I = 0; I < 2000; I++)
	{
		const double T = double(I) * TurnPeriodS / 2000.0;
		Bright += GlassGlow(OffBeam(TurnDeg(T), 37.f), 1.f) > 0.6f ? 1 : 0;
	}
	const double FlashS = Bright * TurnPeriodS / 2000.0 / 2.0;
	Check(FlashS > 0.1 && FlashS < 0.8, "a flash lasts a fraction of a second", FlashS);

	// the log's codes
	Check(Compass8(0.f) == 0 && Compass8(44.f) == 1 && Compass8(91.f) == 2 && Compass8(225.f) == 5 && Compass8(350.f) == 0 && Compass8(-90.f) == 6, "eight points of the compass");
	Check(Beaufort(0.1f) == 0 && Beaufort(6.f) == 4 && Beaufort(18.f) == 8 && Beaufort(40.f) == 12, "Beaufort");
	FReading R;
	R.Cloud = 0.05f;
	Check(SkyCode(R) == 0, "clear");
	R.Cloud = 0.95f;
	Check(SkyCode(R) == 3, "overcast");
	R.PrecipMmH = 2.f;
	Check(SkyCode(R) == 5, "rain");
	R.SnowFraction = 0.9f;
	Check(SkyCode(R) == 8, "snow");
	R.Fog = 0.8f;
	R.VisibilityM = 300.f;
	R.PrecipMmH = 0.f;
	Check(SkyCode(R) == 10, "fog");
	R.Thunder = 0.6f;
	Check(SkyCode(R) == 9, "thunder wins");
	Check(Tendency(-2.f) == -1 && Tendency(0.5f) == 0 && Tendency(1.6f) == 1, "the pressure's way");
	Check(VisClass(100.f) == 0 && VisClass(700.f) == 1 && VisClass(3000.f) == 2 && VisClass(9000.f) == 3 && VisClass(20000.f) == 4, "visibility steps");

	std::vector<float> Hourly(24, 30000.f);
	for (int H = 4; H < 9; H++)
	{
		Hourly[H] = 400.f;
	}
	Check(HornHours(Hourly) == 5.f, "horn hours", HornHours(Hourly));
	std::vector<FReading> Day(3);
	Day[0].WindMs = 21.f;
	Check(Remark(Day, 0.f, 7).rfind("gale.", 0) == 0, "a gale gets its remark");
	Day[0].WindMs = 6.f;
	Check(Remark(Day, 3.f, 7).rfind("fog.", 0) == 0, "fog with the horn gets its remark");
	Check(Remark(Day, 3.f, 7) == Remark(Day, 3.f, 7), "the same day, the same remark");
	std::set<std::string> Seen;
	for (unsigned S = 0; S < 60; S++)
	{
		Seen.insert(Remark(Day, 3.f, S));
	}
	Check(Seen.size() == 3, "fog's three variants all come up", double(Seen.size()));
	Day[1].TempC = -9.f;
	Check(Remark(Day, 0.f, 4).rfind("frost.", 0) == 0, "a hard frost");
	Day[1].TempC = 5.f;
	int Plain = 0, Empty = 0;
	for (unsigned S = 0; S < 300; S++)
	{
		const std::string K = Remark(Day, 0.f, S);
		Plain += K.rfind("plain.", 0) == 0 ? 1 : 0;
		Empty += K.empty() ? 1 : 0;
	}
	Check(Plain > 50 && Empty > 150, "a plain day is mostly blank, now and then a note", Plain, Empty);
}

static void Night()
{
	const float Vis[] = { 30000.f, 8000.f, 2000.f, 600.f, 150.f };
	for (float V : Vis)
	{
		std::printf("visibility %6.0f m:", V);
		for (int H = 0; H < 24; H += 2)
		{
			const bool bOn = LampTime(float(H), 8.f, 17.2f, V);
			const FBeam B = BeamFor(V, 0.f, bOn ? 1.f : 0.f);
			std::printf(" %02d:%s%.2f/%3.0f", H, bOn ? "*" : " ", B.Card, B.LengthM);
		}
		std::printf("  horn %.2f\n", FogHorn(V));
	}
}

int main(int Argc, char** Argv)
{
	const char* Mode = Argc > 1 ? Argv[1] : "test";
	if (!std::strcmp(Mode, "night"))
	{
		Night();
		return 0;
	}
	Test();
	std::printf("lighthousesim: %d checks, %d failed\n", Checks, Fails);
	return Fails ? 1 : 0;
}
