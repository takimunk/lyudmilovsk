// Prints the rare sky's model outside the engine (run.sh): an aurora night minute by minute at a few Kp, the flares
// of a year, how often halos and rainbows show at fixed chances, and checks (no NaNs, determinism, sane ranges).
#include "../../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Sky/RareSkyModel.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

using namespace LyudRareSky;

static double HoursOf(int Y, int M, int D, double H)
{
	// days from 2000-01-01 (proleptic Gregorian)
	auto Days = [](int Y, int M, int D) {
		Y -= M <= 2; const int Era = (Y >= 0 ? Y : Y - 399) / 400; const int Yoe = Y - Era * 400;
		const int Doy = (153 * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1; const int Doe = Yoe * 365 + Yoe / 4 - Yoe / 100 + Doy;
		return Era * 146097 + Doe - 719468; };
	return (Days(Y, M, D) - Days(2000, 1, 1)) * 24.0 + H;
}

static int Fails = 0;
static void Check(bool Ok, const char* What) { if (!Ok) { ++Fails; std::printf("FAIL %s\n", What); } }

int main(int Argc, char** Argv)
{
	const bool bQuiet = Argc > 1 && !std::strcmp(Argv[1], "test");
	FInputs In;
	In.Aurora = 1.f;
	In.SunAltDeg = -30.f;
	const double Night = HoursOf(2026, 10, 29, 18.0);
	for (float Kp : { 3.f, 5.f, 7.f, 9.f })
	{
		In.Kp = Kp;
		double On[8];
		const int N = SubstormOnsets(Night + 6.0, In.Seed, Kp, On, 8);
		if (!bQuiet)
		{
			std::printf("\nKp %.0f: %d substorms at", Kp, N);
			for (int I = 0; I < N; ++I) std::printf(" %05.2f", std::fmod(On[I], 24.0));
			std::printf("\n  time  phase     bright  edge km  spread  act  puls  red  fringe low\n");
		}
		for (int M = 0; M <= 12 * 60; M += 10)
		{
			In.Hours = Night + M / 60.0;
			const FAurora A = Evaluate(In).Aurora;
			Check(std::isfinite(A.Brightness) && std::isfinite(A.EdgeKm) && A.Brightness >= 0.f && A.Brightness <= 1.6f, "aurora range");
			if (!bQuiet && M % 30 == 0)
				std::printf("  %05.2f %-9s %5.2f  %7.0f  %6.0f  %.2f %.2f  %.2f %.2f  %3.0f\n", std::fmod(In.Hours, 24.0), PhaseName(A.Phase),
					A.Brightness, A.EdgeKm, A.SpreadKm, A.Activity, A.Pulsate, A.Red, A.Fringe, A.LowerKm);
		}
	}
	// the same moment gives the same sky
	In.Hours = Night + 5.3; In.Kp = 6.f;
	const FState A1 = Evaluate(In), A2 = Evaluate(In);
	Check(A1.Aurora.Brightness == A2.Aurora.Brightness && A1.Aurora.EdgeKm == A2.Aurora.EdgeKm && A1.Aurora.Phase == A2.Aurora.Phase && A1.Flare.Strength == A2.Flare.Strength, "deterministic");
	// a year of flares
	double S[4096]; float P[4096];
	const int NF = Flares(HoursOf(2026, 1, 1, 0), HoursOf(2027, 1, 1, 0), 2660, S, P, 4096);
	int Big = 0; for (int I = 0; I < NF; ++I) Big += P[I] > 0.5f;
	if (!bQuiet) std::printf("\nflares in 2026: %d (%d big)\n", NF, Big);
	Check(NF > 40 && NF < 160 && Big >= 1 && Big < 20, "flare rate");
	// halos and rainbows at fixed chances: share of daylight hours over a month
	for (float C : { 0.1f, 0.3f, 0.6f })
	{
		FInputs H; H.Halo = C; H.Rainbow = C; H.SunAltDeg = 20.f; H.HighDensity = 0.3f;
		int NH = 0, NR = 0, T = 0;
		for (double X = HoursOf(2026, 6, 1, 0); X < HoursOf(2026, 7, 1, 0); X += 0.05, ++T)
		{
			H.Hours = X; const FState St = Evaluate(H);
			NH += St.Halo.Strength > 0.2f; NR += St.Rainbow.Strength > 0.2f;
		}
		if (!bQuiet) std::printf("chance %.1f: halo %.0f%% of the time, rainbow %.0f%%\n", C, 100.0 * NH / T, 100.0 * NR / T);
		Check(NH > 0 && NH < T, "halo intermittent");
	}
	std::printf(Fails ? "RARESKY %d checks failed\n" : "RARESKY all checks passed\n", Fails);
	return Fails ? 1 : 0;
}
