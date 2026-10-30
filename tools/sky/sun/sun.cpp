// The red dwarf's activity outside the engine (tools/sky/sun/run.sh): prints what the model does and checks it.
#include "../../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Sky/SunActivityModel.h"

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace LyudSunActivity;

namespace
{
	// 2026-10-20 12:30 local, in hours since 2026-01-01 00:00 (day 292 of the year, 0-based)
	constexpr double Noon = 292.0 * 24.0 + 12.5;
	int Failures = 0;

	void Check(bool bOk, const char* What, double Value)
	{
		std::printf("%s %-58s %.3f\n", bOk ? "ok  " : "FAIL", What, Value);
		Failures += bOk ? 0 : 1;
	}

	const char* KindName(const FProminence& P)
	{
		return P.Kind == EPromKind::Loop ? "loop" : (P.Kind == EPromKind::Hedgerow ? "hedge" : "surge");
	}

	/** A prominence's identity across frames: its seed. */
	bool Has(const FState& S, float Seed)
	{
		for (int I = 0; I < S.NumProms; ++I)
		{
			if (S.Proms[I].Seed == Seed && S.Proms[I].Visibility > 0.5f && S.Proms[I].A[2] < 0.4f)
			{
				return true;
			}
		}
		return false;
	}
}

int main(int Argc, char** Argv)
{
	const bool bTestOnly = Argc > 1 && std::strcmp(Argv[1], "test") == 0;
	FSettings Set;
	FState S, S2;

	if (!bTestOnly)
	{
		std::printf("Ten real minutes of the limb from 2026-10-20 12:30 (activity clock x%.0f), one line a minute:\n", Set.TimeScale);
		for (int M = 0; M <= 10; ++M)
		{
			Evaluate(Noon + M / 60.0, Set, S);
			std::printf("  +%2d min  regions %d (sent %d)  prominences %2d alive, %d at the limb, %d erupting | flare %.2f face %.2f\n",
				M, S.AliveRegions, S.NumRegions, S.AliveProms, S.LimbProms, S.Eruptions, S.Flare.Strength, S.Flare.Face);
			for (int I = 0; I < S.NumProms; ++I)
			{
				const FProminence& P = S.Proms[I];
				if (P.Visibility < 0.5f || P.A[2] > 0.4f)
				{
					continue;
				}
				std::printf("      %-5s pa %4.0f deg  h %.2f  w %.2f  bright %.2f  life %.2f  lift %.2f%s\n", KindName(P),
					std::atan2(P.A[1], P.A[0]) * 57.2958, P.Height, P.HalfWidth, P.Bright, P.Life01, P.Lift, P.bErupting ? "  ERUPTING" : "");
			}
		}
		std::printf("\nA game day, every 2 h: regions on the visible face (strength, latitude, view z)\n");
		for (int Hr = 0; Hr <= 24; Hr += 2)
		{
			Evaluate(Noon + Hr, Set, S);
			std::printf("  +%2d h  spin %5.1f deg :", Hr, S.Spin * 57.2958);
			for (int I = 0; I < S.NumRegions; ++I)
			{
				std::printf("  %.2f@%+3.0f z%+.2f", S.Regions[I].Strength, S.Regions[I].Lat * 57.2958, S.Regions[I].ViewZ);
			}
			std::printf("\n");
		}
	}

	// --- checks --------------------------------------------------------------------------------------------------
	// determinism: the same hour gives the same star
	Evaluate(Noon + 3.1234, Set, S);
	Evaluate(Noon + 3.1234, Set, S2);
	Check(std::memcmp(&S.Proms, &S2.Proms, sizeof(S.Proms)) == 0 && S.NumProms == S2.NumProms, "deterministic", 1.0);

	// over 30 game days, sampled every 37 min
	double Regions = 0, Limb = 0, NoLimb = 0, Samples = 0, FlareVisible = 0, MaxFlare = 0;
	int Births = 0, Deaths = 0, Erupting = 0;
	FState Prev;
	Evaluate(Noon, Set, Prev);
	for (double T = 0.0; T < 30.0 * 24.0; T += 37.0 / 60.0)
	{
		Evaluate(Noon + T, Set, S);
		Regions += S.AliveRegions;
		Limb += S.LimbProms;
		NoLimb += S.LimbProms == 0 ? 1 : 0;
		Erupting += S.Eruptions > 0 ? 1 : 0;
		FlareVisible += S.FlareLight > 0.05f ? 1 : 0;
		MaxFlare = std::fmax(MaxFlare, S.FlareLight);
		Samples += 1;
	}
	Check(Regions / Samples > 4 && Regions / Samples < 14, "mean active regions on the whole star (4..14)", Regions / Samples);
	Check(Limb / Samples > 2.5 && Limb / Samples < 12, "mean prominences standing on the limb (2.5..12)", Limb / Samples);
	Check(NoLimb / Samples < 0.05, "share of moments with no prominence on the limb (<5%)", NoLimb / Samples);
	Check(Erupting / Samples > 0.03 && Erupting / Samples < 0.6, "share of moments with an eruption going on", Erupting / Samples);
	Check(FlareVisible / Samples > 0.02 && FlareVisible / Samples < 0.3, "share of moments with a visible flare (>5% light)", FlareVisible / Samples);
	Check(MaxFlare > 0.3, "the biggest visible flare in 30 days", MaxFlare);

	// a few real minutes of looking show change: over 5 game minutes something appears or vanishes at the limb
	int Changed = 0, Windows = 0;
	for (double T = 0.0; T < 10.0 * 24.0; T += 1.7)
	{
		Evaluate(Noon + T, Set, S);
		Evaluate(Noon + T + 5.0 / 60.0, Set, S2);
		bool bChange = false;
		for (int I = 0; I < S.NumProms && !bChange; ++I)
		{
			if (S.Proms[I].Visibility > 0.5f && S.Proms[I].A[2] < 0.4f && !Has(S2, S.Proms[I].Seed))
			{
				bChange = true;
			}
		}
		for (int I = 0; I < S2.NumProms && !bChange; ++I)
		{
			if (S2.Proms[I].Visibility > 0.5f && S2.Proms[I].A[2] < 0.4f && !Has(S, S2.Proms[I].Seed))
			{
				bChange = true;
			}
		}
		Changed += bChange ? 1 : 0;
		++Windows;
		(void)Births; (void)Deaths;
	}
	Check(double(Changed) / Windows > 0.6, "5-minute windows where a limb prominence comes or goes (>60%)", double(Changed) / Windows);

	// a quiet star is quiet; a forced flare shows
	FSettings Quiet = Set;
	Quiet.Activity = 0.f;
	Evaluate(Noon, Quiet, S);
	Check(S.AliveRegions == 0 && S.NumProms == 0 && S.FlareLight == 0.f, "Activity 0: nothing on the star", S.AliveProms);
	FForcedFlare F;
	F.StartActivityHours = ActivityHours(Noon, Set) - 3.0 / 60.0;
	F.Peak = 0.8f;
	Evaluate(Noon, Set, S, &F);
	Check(S.Flare.Strength > 0.5f && S.Flare.Face > 0.5f, "a forced flare 3 activity minutes old shows on the face", S.FlareLight);

	std::printf("%s\n", Failures ? "SOME CHECKS FAILED" : "all checks passed");
	return Failures ? 1 : 0;
}
