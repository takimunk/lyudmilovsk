// Deeper checks of the coastal food web (Private/Sea/EcoModel.cpp) with what sits on it: the fish round the diver
// (FishModel.cpp), the bite weights the fishing reads (ULyudEcosystem::GetBiteWeights, mirrored here) and the catches it
// reports back (ReportCatch, mirrored). tools/underwater/run.sh ecosys [-v]
//
// The web's shape (every prey a node, everything fed from the plants), extreme time steps (a century at once, days
// going backwards, the year's end, a leap year's 366th day), heavy fishing of every catchable fish and the recovery,
// save/load of every field with the old saves' ids, the bite weights over every month, place and light, the fish
// crowd over long runs in every month, and the time budgets.
#include "EcoModel.h"
#include "FishModel.h"
#include "FishingModel.h"

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace LyudEco;

static int Fails = 0;
static bool bVerbose = false;
static void Check(bool Ok, const char* What, double Value = 0.0)
{
	std::printf("%s %s (%.4g)\n", Ok ? "ok  " : "FAIL", What, Value);
	if (!Ok) ++Fails;
}

/** Every node finite, within the model's clamp (1e-3..20 of its base), here 0..1, the young finite and >= 0; how many are not. */
static int BadNodes(const FModel& M, bool bPrint = false)
{
	int Bad = 0;
	for (int I = 0; I < NumNodes(); ++I)
	{
		const float B = M.Biomass(I), R = M.Relative(I), F = M.Here(I), Y = M.YoungToCome(I);
		const bool bBad = !std::isfinite(B) || !std::isfinite(Y) || !std::isfinite(F) || R < 1e-3f * 0.999f || R > 20.f * 1.001f
			|| F < 0.f || F > 1.f || Y < 0.f;
		if (bBad && bPrint) std::printf("  bad %s: rel %g here %g young %g\n", Node(I).Id, R, F, Y);
		Bad += bBad;
	}
	return Bad;
}

static float WorstRelDiff(const FModel& A, const FModel& B)
{
	float W = 0.f;
	for (int I = 0; I < NumNodes(); ++I)
	{
		W = std::max(W, std::fabs(A.Biomass(I) / B.Biomass(I) - 1.f));
		W = std::max(W, std::fabs(A.Here(I) - B.Here(I)));
	}
	return W;
}

static double MsSince(std::chrono::steady_clock::time_point T0)
{
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
}

/** Runs Fn in a child process with a time limit: true if it returned in time (a hang is a fail, not a stuck suite). */
template <typename FnT>
static bool ReturnsWithin(unsigned Seconds, FnT Fn)
{
	std::fflush(stdout);
	const pid_t Pid = fork();
	if (Pid < 0) return true;
	if (Pid == 0)
	{
		alarm(Seconds);
		Fn();
		_exit(0);
	}
	int Status = 0;
	waitpid(Pid, &Status, 0);
	return WIFEXITED(Status) && WEXITSTATUS(Status) == 0;
}

// ---- mirrors of ULyudEcosystem's plain logic (LyudEcosystem.cpp): keep in step with it

/** Refresh(): a fish species' numbers against the usual, clamped as the fish see them. */
static float FishNow(const FModel& M, int FishSpecies)
{
	const int N = Find(LyudFish::Species(FishSpecies).Id);
	return N >= 0 ? std::min(std::max(M.Relative(N), 0.05f), 4.f) : 1.f;
}

/** GetBiteWeights(): the fish that take a bait here now by weight (index: LyudFish species; not normalised). */
static std::vector<float> BiteWeights(const FModel& M, float XM, float YM, float Sand, float Stone, float Weed, float Depth, int Month,
	float Daylight)
{
	std::vector<float> W(LyudFish::NumSpecies(), 0.f);
	LyudFish::FClock Clock;
	Clock.Month = Month;
	Clock.Daylight = Daylight;
	for (int S = 0; S < LyudFish::NumSpecies(); ++S)
	{
		const LyudFish::FSpecies& Sp = LyudFish::Species(S);
		if (Sp.Way == LyudFish::EWay::Crawler || Sp.Way == LyudFish::EWay::Swarm || Sp.bBreathes) continue;
		const int N = Find(Sp.Id);
		const float Abund = FishNow(M, S) * (N >= 0 ? M.Local(N, XM, YM) : 1.f);
		const float V = Abund * LyudFish::Suitability(S, Sand, Stone, Weed, Depth) * LyudFish::Activity(S, Clock) * Sp.Abundance;
		if (V > 0.f) W[S] = V;
	}
	return W;
}

/** ReportCatch(): a fish taken out of the web and its spot (Grams < 0: a typical one). */
static void ReportCatch(FModel& M, const char* Id, float XM, float YM, float Grams)
{
	const int N = Find(Id);
	if (N < 0 || !std::isfinite(Grams)) return;
	const int S = LyudFish::FindSpecies(Id);
	if (Grams < 0.f)
	{
		const float L = S >= 0 ? LyudFish::Species(S).Length : 0.2f;
		Grams = 10.f * std::pow(L / 0.1f, 3.f);
	}
	M.Remove(N, Grams);
	const bool bSettled = S >= 0 && LyudFish::IsSettled(LyudFish::Species(S).Way);
	M.TakeLocal(N, XM, YM, bSettled ? 0.25f : 0.08f);
}

/** EcoDayOfYear(): 0-based day plus the hour (a leap year's 31 December is day 365.x). */
static float EcoDay(int DayOfYear1, float Hours) { return float(DayOfYear1 - 1) + Hours / 24.f; }

/** Sync()'s catch-up: a jump of Days from the day the web was stepped to (at most three years lived through). */
static void CatchUp(FModel& M, float SteppedToDay, double Days)
{
	constexpr double MaxDays = 3.0 * 365.0;
	const float Live = float(std::min(Days, MaxDays));
	// a longer jump lives the last three years before now, so the web ends in today's season
	const float NowDay = float(std::fmod(double(SteppedToDay) + Days, 365.0));
	M.Step(Days > MaxDays ? NowDay - Live : SteppedToDay, Live);
}

/** A bed like the one off the club (as underwatertest.cpp's, the reef's edge a steep 1 m ramp: a real bed has no cliff), with the food web's numbers for the fish. */
struct FEcoEnv : LyudFish::FEnv
{
	float Level = 0.f;
	const FModel* Web = nullptr;
	float Force = -1.f;   // >= 0: every species at this abundance
	bool Bed(float X, float Y, float& Z) const override
	{
		if (X < 5.f) return false;
		Z = -std::min(0.04f * (X - 5.f) + 0.3f, 15.f) + 1.5f * std::min(std::max(10.5f - std::fabs(X - 120.f), 0.f), 1.f) + 0.2f * std::sin(Y * 0.3f);
		return Z < Level - 0.1f;
	}
	float SurfaceZ() const override { return Level; }
	void Habitat(float X, float Y, float& Sand, float& Stone, float& Weed) const override
	{
		Stone = std::fabs(X - 120.f) < 12.f ? 1.f : (std::sin(X * 0.1f) * std::sin(Y * 0.13f) > 0.6f ? 0.6f : 0.f);
		Weed = (X > 40.f && X < 100.f && std::sin(Y * 0.05f) > -0.2f) ? 0.9f : 0.f;
		Sand = 1.f - Stone;
	}
	LyudFish::FV Current(const LyudFish::FV&) const override { return LyudFish::FV(0.05f, 0.2f, 0.f); }
	float Abundance(int Species, float X, float Y) const override
	{
		if (Force >= 0.f) return Force;
		if (!Web) return 1.f;
		const int N = Find(LyudFish::Species(Species).Id);
		return FishNow(*Web, Species) * (N >= 0 ? Web->Local(N, X, Y) : 1.f);
	}
};

static void WebShape()
{
	const int N = NumNodes();
	// unique ids, every prey a node, the table's diet shares sum to 1 (a typo shows as 0.9 or 1.1)
	int Dups = 0, Unknown = 0, BadSums = 0;
	for (int I = 0; I < N; ++I)
	{
		for (int J = I + 1; J < N; ++J) Dups += std::strcmp(Node(I).Id, Node(J).Id) == 0;
		float Sum = 0.f;
		for (const FDietItem& D : Node(I).Diet)
		{
			Sum += D.Share;
			if (Find(D.Prey) < 0) { std::printf("  %s eats unknown %s\n", Node(I).Id, D.Prey); ++Unknown; }
			if (D.Share <= 0.f) { std::printf("  %s: share of %s %g\n", Node(I).Id, D.Prey, D.Share); ++BadSums; }
		}
		// a prey listed twice is a typo too
		for (size_t A = 0; A < Node(I).Diet.size(); ++A)
			for (size_t B = A + 1; B < Node(I).Diet.size(); ++B)
				if (std::strcmp(Node(I).Diet[A].Prey, Node(I).Diet[B].Prey) == 0) { std::printf("  %s lists %s twice\n", Node(I).Id, Node(I).Diet[A].Prey); ++BadSums; }
		// the model normalises each row, so a row a little over 1 only shifts the shares; far off is a lost or doubled item
		if (Node(I).Kind == EKind::Consumer && std::fabs(Sum - 1.f) > 0.011f) std::printf("  %s: diet sums to %.3f (normalised)\n", Node(I).Id, Sum);
		if (Node(I).Kind == EKind::Consumer && std::fabs(Sum - 1.f) > 0.15f) ++BadSums;
		if (Node(I).Kind != EKind::Consumer && !Node(I).Diet.empty()) { std::printf("  %s is not a consumer but eats\n", Node(I).Id); ++BadSums; }
	}
	Check(Dups == 0, "every node id is unique (duplicates)", Dups);
	Check(Unknown == 0, "every diet item is a node (unknown prey)", Unknown);
	Check(BadSums == 0, "every consumer's diet shares are positive, listed once and sum to 1 within 15% (bad rows)", BadSums);

	// the model's diet matrix: rows of consumers sum to 1, of the rest to 0, no negative share
	{
		FModel M;
		float Worst = 0.f;
		for (int J = 0; J < N; ++J)
		{
			float Sum = 0.f;
			for (int I = 0; I < N; ++I)
			{
				Sum += M.DietShare(J, I);
				if (M.DietShare(J, I) < 0.f) Worst = 1.f;
			}
			Worst = std::max(Worst, std::fabs(Sum - (Node(J).Kind == EKind::Consumer ? 1.f : 0.f)));
		}
		Check(Worst < 1e-5f, "the diet matrix: each consumer's row sums to 1, the plants' and the detritus' to 0", Worst);
	}

	// everything is fed from the plants: a consumer is reachable once one of its prey is; the detritus once a plant is
	{
		std::vector<char> Fed(N, 0);
		for (int I = 0; I < N; ++I) Fed[I] = Node(I).Kind == EKind::Producer;
		for (bool bMore = true; bMore;)
		{
			bMore = false;
			for (int I = 0; I < N; ++I)
			{
				if (Fed[I]) continue;
				bool bNow = false;
				if (Node(I).Kind == EKind::Detritus)
					for (int P = 0; P < N; ++P) bNow |= Fed[P] && Node(P).Kind == EKind::Producer;
				for (const FDietItem& D : Node(I).Diet)
					if (const int P = Find(D.Prey); P >= 0 && Fed[P]) bNow = true;
				if (bNow) { Fed[I] = 1; bMore = true; }
			}
		}
		int Unfed = 0;
		for (int I = 0; I < N; ++I)
			if (!Fed[I]) { std::printf("  %s is not fed from the plants\n", Node(I).Id); ++Unfed; }
		Check(Unfed == 0, "every group is reachable from the primary producers (unfed)", Unfed);
		// and nothing only eats itself (cannibalism is a share, never the whole)
		int Self = 0;
		FModel M;
		for (int I = 0; I < N; ++I) Self += M.DietShare(I, I) > 0.5f;
		Check(Self == 0, "no group lives mostly on itself", Self);
	}

	// the plankton and the bed's small animals are all somebody's food (a fish nothing eats is a top fish: listed)
	{
		FModel M;
		int Uneaten = 0, TopFish = 0;
		for (int I = 0; I < N; ++I)
		{
			if (Node(I).Kind != EKind::Consumer || Node(I).bWarm) continue;
			float By = 0.f;
			for (int J = 0; J < N; ++J) if (J != I) By += M.DietShare(J, I);
			if (By > 0.f) continue;
			const int S = LyudFish::FindSpecies(Node(I).Id);
			const bool bFish = S >= 0 && LyudFish::Species(S).Way != LyudFish::EWay::Crawler && LyudFish::Species(S).Way != LyudFish::EWay::Swarm;
			if (bFish) { if (bVerbose) std::printf("  nothing in the web eats the fish %s\n", Node(I).Id); ++TopFish; }
			else { std::printf("  nothing eats %s\n", Node(I).Id); ++Uneaten; }
		}
		std::printf("  fish nothing in the web eats: %d\n", TopFish);
		Check(Uneaten == 0, "every invertebrate group is somebody's food (uneaten)", Uneaten);
	}

	// the fish and the web meet by name, and agree on the months the migrants are here
	{
		int Missing = 0, Months = 0;
		for (int S = 0; S < LyudFish::NumSpecies(); ++S)
		{
			const LyudFish::FSpecies& Sp = LyudFish::Species(S);
			const int I = Find(Sp.Id);
			if (I < 0) { std::printf("  fish %s has no node\n", Sp.Id); ++Missing; continue; }
			if (Node(I).Months != Sp.Months) { std::printf("  %s: web months %03x, fish months %03x\n", Sp.Id, Node(I).Months, Sp.Months); ++Months; }
		}
		Check(Missing == 0, "every fish species is a node of the web (missing)", Missing);
		Check(Months == 0, "the fish and the web agree on the months each is here (disagreeing)", Months);
	}

	// every catchable fish is a fish species and a node, and can be on the bite list (not a crawler, swarm or air breather)
	{
		int Bad = 0;
		for (int C = 0; C < LyudFishing::NumCatchable(); ++C)
		{
			const char* Id = LyudFishing::Catchable(C).Id;
			const int S = LyudFish::FindSpecies(Id);
			const bool bOk = S >= 0 && Find(Id) >= 0 && LyudFish::Species(S).Way != LyudFish::EWay::Crawler
				&& LyudFish::Species(S).Way != LyudFish::EWay::Swarm && !LyudFish::Species(S).bBreathes;
			if (!bOk) { std::printf("  catchable %s cannot bite\n", Id); ++Bad; }
		}
		Check(Bad == 0, "every catchable fish is a fish species and a web node that can take a bait (bad)", Bad);
	}
}

static void ExtremeSteps(const FModel& Warm)
{
	// a century of daily steps, then a century in one call: finite and inside the clamp all the way
	{
		FModel M = Warm;
		int Bad = 0, Worst = 0;
		const auto T0 = std::chrono::steady_clock::now();
		for (int Y = 0; Y < 100; ++Y)
		{
			for (int D = 0; D < 365; ++D) M.Step(float(D), 1.f);
			const int B = BadNodes(M);
			Bad += B;
			Worst = std::max(Worst, B);
		}
		const double Ms = MsSince(T0);
		BadNodes(M, true);
		Check(Bad == 0, "a hundred years of daily steps: every node finite, >= 0, inside its clamp (bad node-years)", Bad);
		Check(Ms < 15000.0, "a hundred years of daily steps take under 15 s (ms)", Ms);
		FModel One = Warm;
		One.Step(0.f, 100.f * 365.f);
		Check(BadNodes(One, true) == 0, "a hundred years in one call: every node finite and inside its clamp", BadNodes(One));
		// the same century either way ends near the same web (the step is quarter days inside either way)
		const float Diff = WorstRelDiff(M, One);
		Check(Diff < 0.02f, "a century day by day and at once end at the same web (worst difference)", Diff);
	}
	// step size: a month in game hours (a sync every hour), in 60-day warm-up slices and at once agree
	{
		FModel A = Warm, B = Warm, C = Warm;
		for (int H = 0; H < 30 * 24; ++H) A.Step(100.f + H / 24.f, 1.f / 24.f);
		B.Step(100.f, 30.f);
		for (int K = 0; K < 30; ++K) C.Step(100.f + K * 1.f, 1.f);
		// (the plankton turn over in days: their quarter-day Euler steps differ from hourly ones by some %; the rest must not)
		float D1 = 0.f, DPl = 0.f;
		for (int I = 0; I < NumNodes(); ++I)
		{
			const float D = std::fabs(A.Biomass(I) / B.Biomass(I) - 1.f);
			if (Node(I).PB > 20.f) DPl = std::max(DPl, D);
			else D1 = std::max(D1, D);
		}
		const float D2 = WorstRelDiff(B, C);
		Check(D1 < 0.04f, "a month stepped hour by hour and at once agree within 4% but for the plankton (worst)", D1);
		Check(DPl < 0.15f, "... and the plankton within 15% (worst)", DPl);
		Check(D2 < 1e-4f, "a month stepped day by day and at once agree (whole quarter-day steps) (worst)", D2);
	}
	// the year's end: across New Year's in one call and in two agree; a leap year's day 365.x is New Year's day 0.x
	{
		FModel A = Warm, B = Warm;
		A.Step(364.5f, 1.f);
		B.Step(364.5f, 0.5f);
		B.Step(0.f, 0.5f);
		Check(WorstRelDiff(A, B) < 1e-5f, "stepping across New Year's at once or in two halves agrees", WorstRelDiff(A, B));
		FModel L = Warm, NY = Warm;
		L.Step(EcoDay(366, 12.f), 1.f);   // 31 December of a leap year, noon
		NY.Step(0.5f, 1.f);
		Check(WorstRelDiff(L, NY) == 0.f, "a leap year's 31 December steps as the 1st of January (366th day wraps)", WorstRelDiff(L, NY));
		FModel Neg = Warm, Far = Warm, Plain = Warm;
		Neg.Step(100.f - 10.f * 365.f, 3.f);
		Far.Step(100.f + 1000.f * 365.f, 3.f);
		Plain.Step(100.f, 3.f);
		const float D = std::max(WorstRelDiff(Neg, Plain), WorstRelDiff(Far, Plain));
		Check(D < 1e-4f, "days far before 0 or years ahead step as their day of the year (worst)", D);
	}
	// the clock going back and forth (loads of older saves, a console jump): the web stays finite
	{
		FModel M = Warm;
		uint32_t R = 99;
		int Bad = 0;
		for (int K = 0; K < 3000; ++K)
		{
			R = R * 1664525u + 1013904223u;
			const float Day = float(int(R >> 8) % 2000 - 1000) * 0.73f;
			const float Len = float((R >> 4) % 97) * 0.11f;
			M.Step(Day, Len);
			M.Step(Day, -Len);   // a negative span is nothing
			M.Step(Day, 0.f);
			Bad += BadNodes(M) > 0;
		}
		Check(Bad == 0, "days jumping backwards and forwards, zero and negative spans: the web stays finite (bad steps)", Bad);
		FModel Z = Warm;
		Z.Step(50.f, -30.f);
		Z.Step(50.f, std::nanf(""));
		Check(WorstRelDiff(Z, Warm) == 0.f, "a negative or NaN span changes nothing", WorstRelDiff(Z, Warm));
	}
	// a span the float cannot count down (an infinite or a billion days from a broken caller) still returns
	{
		FModel M = Warm;
		const bool bInf = ReturnsWithin(5, [&M] { M.Step(10.f, INFINITY); });
		Check(bInf, "Step with an infinite span returns (no endless loop)", bInf);
		const bool bHuge = ReturnsWithin(5, [&M] { M.Step(10.f, 1e9f); });
		Check(bHuge, "Step with a billion-day span returns within 5 s", bHuge);
	}
	// the catch-up after a long jump (ULyudEcosystem::Sync, mirrored): ends in today's season, not the one it left
	{
		FModel A = Warm, Ref = Warm;
		// stepped to the 1st of January; now is five and a half years later, early July
		CatchUp(A, 0.f, 5.0 * 365.0 + 185.0);
		Ref.Step(185.f - 3.f * 365.f, 3.f * 365.f);
		const int Eider = Find("Eider"), Sprat = Find("Sprat");
		Check(A.Here(Eider) < 0.1f && A.Here(Sprat) < 0.1f, "after a five-year jump to July the eiders and sprat are away (eider share)", A.Here(Eider));
		Check(WorstRelDiff(A, Ref) < 1e-4f, "a jump past three years lives the three years before now", WorstRelDiff(A, Ref));
		FModel B = Warm, C = Warm;
		CatchUp(B, 40.f, 20.5);
		C.Step(40.f, 20.5f);
		Check(WorstRelDiff(B, C) == 0.f, "a short catch-up steps on from where the web was", WorstRelDiff(B, C));
	}
}

static void HeavyFishing(const FModel& Warm)
{
	// every catchable fish taken as hard as an army of anglers could (ReportCatch, mirrored): 5000 a day each, ten years
	FModel M = Warm;
	uint32_t R = 7;
	int Bad = 0;
	const auto T0 = std::chrono::steady_clock::now();
	for (int Day = 0; Day < 3650; ++Day)
	{
		for (int C = 0; C < LyudFishing::NumCatchable(); ++C)
			for (int K = 0; K < 50; ++K)
			{
				R = R * 1664525u + 1013904223u;
				const float X = float(int(R >> 12) % 4000 - 2000), Y = float(int(R >> 4) % 400);
				// a hundred fish a cast's worth (50 reports of 100 typical ones), plus a single typical one at the spot
				ReportCatch(M, LyudFishing::Catchable(C).Id, X, Y, 100.f * 10.f * std::pow(LyudFish::Species(LyudFish::FindSpecies(LyudFishing::Catchable(C).Id)).Length / 0.1f, 3.f));
				ReportCatch(M, LyudFishing::Catchable(C).Id, X, Y, -1.f);
			}
		M.Step(float(Day % 365), 1.f);
		Bad += BadNodes(M) > 0;
	}
	const double Ms = MsSince(T0);
	BadNodes(M, true);
	Check(Bad == 0, "ten years of every catchable fish fished hard: every node finite and >= its floor (bad days)", Bad);
	float Lo = 1e9f;
	const char* LoId = "";
	for (int C = 0; C < LyudFishing::NumCatchable(); ++C)
	{
		const int I = Find(LyudFishing::Catchable(C).Id);
		if (M.Relative(I) < Lo) { Lo = M.Relative(I); LoId = Node(I).Id; }
	}
	std::printf("  fished hardest down: %s at %.4f of its base (%.0f ms)\n", LoId, Lo, Ms);
	Check(Lo < 0.2f, "hard fishing takes the fish down (lowest catchable, of base)", Lo);
	// the spots: thinned but never below nothing, and the fished spots' records healed and gone a year after the fishing stops
	float LocalLo = 1.f, LocalHi = 0.f;
	{
		std::vector<std::string> K;
		std::vector<float> V;
		M.Save(K, V);
		for (size_t I = 0; I < K.size(); ++I)
			if (K[I].find(',') != std::string::npos) { LocalLo = std::min(LocalLo, V[I]); LocalHi = std::max(LocalHi, V[I]); }
	}
	Check(LocalLo >= 0.f && LocalHi <= 1.f, "the local thinning stays within 0..1 under heavy fishing (lowest)", LocalLo);
	// the fishing stops: everything comes back (thirty years)
	FModel Rec = M;
	for (int Y = 0; Y < 30; ++Y) Rec.Step(0.f, 365.f);
	float RecLo = 1e9f;
	const char* RecId = "";
	for (int C = 0; C < LyudFishing::NumCatchable(); ++C)
	{
		const int I = Find(LyudFishing::Catchable(C).Id);
		if (Rec.Relative(I) < RecLo) { RecLo = Rec.Relative(I); RecId = Node(I).Id; }
	}
	std::printf("  thirty years after: slowest back %s at %.2f of its base\n", RecId, RecLo);
	Check(RecLo > 0.3f, "thirty years after the fishing stops every catchable fish is back over 30% of its base (lowest)", RecLo);
	std::vector<std::string> K;
	std::vector<float> V;
	Rec.Save(K, V);
	int Cells = 0;
	for (const std::string& Key : K) Cells += Key.find(',') != std::string::npos;
	Check(Cells == 0, "the fished spots have all healed and are forgotten (local records left)", Cells);
}

static void LocalThinning(const FModel& Warm)
{
	FModel M = Warm;
	const int Cod = Find("Cod"), Goby = Find("RoundGoby");
	for (int K = 0; K < 200; ++K) M.TakeLocal(Goby, 5.f, 5.f, 0.25f);
	M.TakeLocal(Cod, 5.f, 5.f, 2.f);    // more than all of them
	M.TakeLocal(Cod, 90.f, 5.f, -1.f);  // a negative take
	const float G = M.Local(Goby, 5.f, 5.f), C = M.Local(Cod, 5.f, 5.f), Neg = M.Local(Cod, 90.f, 5.f);
	Check(G >= 0.f && G < 1e-6f && C == 0.f && Neg == 1.f, "a spot fished bare stays at 0, a take over 1 or below 0 is clamped (gobies)", G);
	// either side of 0 and of a 40 m cell's edge are other cells
	M.TakeLocal(Cod, -0.5f, -0.5f, 0.5f);
	Check(M.Local(Cod, 0.5f, 0.5f) == 0.f && M.Local(Cod, -0.5f, -0.5f) == 0.5f && M.Local(Cod, -39.5f, -0.5f) == 0.5f
		&& M.Local(Cod, -40.5f, -0.5f) == 1.f, "cells split at 0 and every 40 m (negative coordinates too)", M.Local(Cod, -40.5f, -0.5f));
	// a bare spot comes back: the roamers within a fortnight, the bottom fish within three months
	M.Step(10.f, 14.f);
	const float C14 = M.Local(Cod, 5.f, 5.f);
	M.Step(24.f, 90.f);
	const float G104 = M.Local(Goby, 5.f, 5.f);
	Check(C14 > 0.95f, "a spot with every cod taken has them back in a fortnight", C14);
	Check(G104 > 0.95f, "a spot with every goby taken has them back in three months", G104);
	// many spots at once: a step stays cheap
	FModel Many = Warm;
	for (int K = 0; K < 3000; ++K) Many.TakeLocal(Goby, float(K % 60) * 40.f, float(K / 60) * 40.f, 0.5f);
	const auto T0 = std::chrono::steady_clock::now();
	for (int H = 0; H < 24; ++H) Many.Step(100.f + H / 24.f, 1.f / 24.f);
	const double Us = MsSince(T0) * 1000.0 / 24.0;
	Check(Us < 1000.0, "an hour's step with 3000 thinned spots takes under 1 ms (us)", Us);
}

static void SaveLoad(const FModel& Warm)
{
	// a web with every kind of field: off its base, migrants half in, young to come, thinned spots (negative cells,
	// two species in a cell)
	FModel S = Warm;
	S.Step(150.f, 40.f);   // June: the young pools are full, the sprat leaving
	S.SetBiomass(Find("Cod"), S.Biomass(Find("Cod")) * 0.3f);
	S.TakeLocal(Find("Perch"), 120.f, -35.f, 0.5f);
	S.TakeLocal(Find("RoundGoby"), 120.f, -35.f, 0.3f);
	S.TakeLocal(Find("Flounder"), -4000.f, 7000.f, 0.9f);
	S.TakeLocal(Find("Herring"), -1.f, -1.f, 0.2f);
	std::vector<std::string> K;
	std::vector<float> V;
	S.Save(K, V);
	int Rel = 0, Here = 0, Young = 0, Cells = 0;
	for (const std::string& Key : K)
	{
		Rel += Key.find("@rel") != std::string::npos;
		Here += Key.find("@here") != std::string::npos;
		Young += Key.find("@young") != std::string::npos;
		Cells += Key.find(',') != std::string::npos;
	}
	Check(Rel == NumNodes() && Here == NumNodes() && Young > 10 && Cells == 4, "a save holds every node's biomass and share here, the young, every thinned spot (cells)", Cells);
	FModel L;
	L.Reset(190.f);
	L.Load(K, V, 190.f);
	float Worst = 0.f;
	for (int I = 0; I < NumNodes(); ++I)
	{
		Worst = std::max(Worst, std::fabs(L.Biomass(I) / S.Biomass(I) - 1.f) + std::fabs(L.Here(I) - S.Here(I)));
		Worst = std::max(Worst, std::fabs(L.YoungToCome(I) - S.YoungToCome(I)) / std::max(S.YoungToCome(I), 1e-6f));
	}
	const bool bCells = L.Local(Find("Perch"), 121.f, -36.f) == S.Local(Find("Perch"), 121.f, -36.f)
		&& L.Local(Find("RoundGoby"), 121.f, -36.f) == S.Local(Find("RoundGoby"), 121.f, -36.f)
		&& L.Local(Find("Flounder"), -3999.f, 7001.f) == S.Local(Find("Flounder"), -3999.f, 7001.f)
		&& L.Local(Find("Herring"), -2.f, -2.f) == S.Local(Find("Herring"), -2.f, -2.f);
	Check(Worst < 1e-5f, "load gives back every node's biomass, share here and young (worst relative)", Worst);
	Check(bCells, "load gives back every thinned spot, negative cells and shared cells too", bCells);
	// saved again it is the same save; a year on the two webs are still the same
	{
		std::vector<std::string> K2;
		std::vector<float> V2;
		L.Save(K2, V2);
		std::map<std::string, float> A, B;
		for (size_t I = 0; I < K.size(); ++I) A[K[I]] = V[I];
		for (size_t I = 0; I < K2.size(); ++I) B[K2[I]] = V2[I];
		float D = A.size() == B.size() ? 0.f : 1.f;
		for (const auto& E : A)
		{
			auto It = B.find(E.first);
			D = std::max(D, It == B.end() ? 1.f : std::fabs(It->second - E.second) / std::max(std::fabs(E.second), 1e-6f));
		}
		Check(D < 1e-5f, "a loaded web saves the same keys and values (worst)", D);
		FModel S2 = S, L2 = L;
		S2.Step(190.f, 365.f);
		L2.Step(190.f, 365.f);
		Check(WorstRelDiff(S2, L2) < 1e-3f, "a year on, the saved and the loaded web are still the same (worst)", WorstRelDiff(S2, L2));
	}
	// the old saves: FishBirds and SeaDucks (renamed Cormorant and Eider), plain g/m2 (no @), no young kept
	{
		FModel O;
		O.Reset(200.f);
		const int Co = Find("Cormorant"), Ei = Find("Eider"), Cod = Find("Cod"), Her = Find("Herring");
		std::vector<std::string> OK = { "FishBirds@rel", "SeaDucks@rel", "SeaDucks@here", "Cod", "Herring@rel" };
		std::vector<float> OV = { 0.5f, 2.f, 0.25f, Node(Cod).B * 0.4f, 1.2f };
		O.Load(OK, OV, 200.f);
		const bool bAlias = std::fabs(O.Relative(Co) - 0.5f) < 1e-5f && std::fabs(O.Relative(Ei) - 2.f) < 1e-5f && O.Here(Ei) == 0.25f;
		Check(bAlias, "an old save's FishBirds and SeaDucks load as Cormorant and Eider", O.Relative(Co));
		Check(std::fabs(O.Relative(Cod) - 0.4f) < 1e-5f, "an old save's plain g/m2 value loads as biomass (cod, of base)", O.Relative(Cod));
		Check(O.YoungToCome(Her) > 0.f && std::isfinite(O.YoungToCome(Her)), "an old save without the young gets them grown for its day (herring young)", O.YoungToCome(Her));
		// a newer save's young win over the old-save estimate, and the alias of a key with @young works
		FModel Y;
		Y.Reset(200.f);
		Y.Load({ "Herring@young", "Herring@rel" }, { 0.125f, 1.f }, 200.f);
		Check(std::fabs(Y.YoungToCome(Her) - 0.125f * Node(Her).B) < 1e-5f, "a save's young of the year load as saved (herring)", Y.YoungToCome(Her) / Node(Her).B);
	}
	// nonsense in a save (unknown ids, broken keys, NaN, out-of-range values, fewer values than keys) loads what it can
	{
		FModel G;
		G.Reset(10.f);
		std::vector<std::string> GK = { "Kraken@rel", "Cod@", "@rel", "", "Cod@abc", "Cod@5", "Cod@rel", "Perch@rel", "Perch@here",
			"Pike@here", "Sprat@young", "Eel@1,2", "Eel@3,4", "Zoo@rel" };
		std::vector<float> GV = { 3.f, 5.f, 2.f, 1.f, 0.5f, 0.5f, std::nanf(""), 1e9f, -4.f, 7.f, -3.f, 2.f, -1.f };
		G.Load(GK, GV, 10.f);
		const int Pe = Find("Perch"), Pi = Find("Pike"), Sp = Find("Sprat"), Eel = Find("Eel"), Zoo = Find("Zoo");
		const bool bOk = BadNodes(G, true) == 0 && G.Relative(Find("Cod")) == 1.f && G.Relative(Pe) == 20.f && G.Here(Pe) == 0.f
			&& G.Here(Pi) == 1.f && G.YoungToCome(Sp) == 0.f && G.Local(Eel, 41.f, 81.f) == 1.f && G.Local(Eel, 121.f, 161.f) == 0.f
			&& G.Relative(Zoo) == 1.f;
		Check(bOk, "a broken save loads safely: unknown ids and NaN skipped, values clamped, keys without values ignored", G.Relative(Pe));
	}
}

static void BiteWeightChecks(const FModel& Warm)
{
	// over every month, day and night, every kind of bed and depth, a web from fished-out to teeming: finite, >= 0, zero
	// for a fish away that month, and every catchable fish bites somewhere sometime
	FModel Low = Warm, High = Warm;
	for (int I = 0; I < NumNodes(); ++I)
	{
		Low.SetBiomass(I, 0.f);
		High.SetBiomass(I, 1e9f);
	}
	Low.TakeLocal(Find("Cod"), 0.f, 0.f, 1.f);
	const FModel* Webs[] = { &Low, &Warm, &High };
	std::vector<float> Best(LyudFish::NumSpecies(), 0.f);
	int Bad = 0, Away = 0, Excluded = 0, Grid = 0;
	for (const FModel* W : Webs)
		for (int Mo = 1; Mo <= 12; ++Mo)
			for (float Light : { 0.f, 0.5f, 1.f })
				for (float Depth : { 0.f, 0.3f, 1.f, 2.5f, 5.f, 9.f, 15.f, 25.f, 40.f })
					for (int Bed = 0; Bed < 5; ++Bed)
					{
						const float Sand = Bed == 0 ? 1.f : Bed == 3 ? 0.5f : 0.f, Stone = Bed == 1 ? 1.f : Bed == 3 ? 0.5f : 0.f;
						const float Weed = Bed == 2 ? 1.f : Bed == 4 ? 0.3f : 0.f;
						const std::vector<float> B = BiteWeights(*W, 0.f, 0.f, Sand, Stone, Weed, Depth, Mo, Light);
						++Grid;
						for (int S = 0; S < LyudFish::NumSpecies(); ++S)
						{
							const LyudFish::FSpecies& Sp = LyudFish::Species(S);
							Bad += !std::isfinite(B[S]) || B[S] < 0.f;
							Away += B[S] > 0.f && !((Sp.Months >> (Mo - 1)) & 1u);
							// the web says it is away (its share here 0 in mid-month) but it would bite
							const int N = Find(Sp.Id);
							Away += B[S] > 0.f && N >= 0 && !((Node(N).Months >> (Mo - 1)) & 1u);
							Excluded += B[S] > 0.f && (Sp.Way == LyudFish::EWay::Crawler || Sp.Way == LyudFish::EWay::Swarm || Sp.bBreathes);
							Best[S] = std::max(Best[S], B[S]);
						}
					}
	Check(Bad == 0, "bite weights are finite and >= 0 for every month, light, bed and depth, fished out or teeming (bad)", Bad);
	Check(Away == 0, "no bite weight for a fish away that month (by the fish's or the web's months)", Away);
	Check(Excluded == 0, "no bite weight for shrimp, mysids, seals or birds", Excluded);
	int Never = 0;
	for (int C = 0; C < LyudFishing::NumCatchable(); ++C)
	{
		const int S = LyudFish::FindSpecies(LyudFishing::Catchable(C).Id);
		if (S < 0 || Best[S] <= 0.f) { std::printf("  %s never bites\n", LyudFishing::Catchable(C).Id); ++Never; }
	}
	Check(Never == 0, "every catchable fish has a bite weight somewhere, some month (never)", Never);
	// a spot fished bare has no bite weight for that fish (the cod at 0,0 above), the next cell does
	{
		const int Cod = LyudFish::FindSpecies("Cod");
		const float Here = BiteWeights(Low, 0.f, 0.f, 0.f, 1.f, 0.f, 10.f, 1, 0.f)[Cod];
		const float Next = BiteWeights(Low, 50.f, 0.f, 0.f, 1.f, 0.f, 10.f, 1, 0.f)[Cod];
		Check(Here == 0.f && Next > 0.f, "a spot fished bare gives that fish no bite weight, the next cell does", Next);
	}
	// the fishing takes them as they come (UpdateWeights: catchable index): a session's bite rate is finite and positive
	// and only fish with a weight bite
	{
		int BadRate = 0, Wrong = 0, Bites = 0;
		const int Worm = LyudFishing::FindBait("bait_worm");
		for (int Mo = 1; Mo <= 12; ++Mo)
		{
			const std::vector<float> B = BiteWeights(Warm, 0.f, 0.f, 0.6f, 0.4f, 0.3f, 4.f, Mo, 0.5f);
			std::vector<float> W(LyudFishing::NumCatchable(), 0.f);
			for (int C = 0; C < LyudFishing::NumCatchable(); ++C)
			{
				const int S = LyudFish::FindSpecies(LyudFishing::Catchable(C).Id);
				if (S >= 0) W[C] = B[S];
			}
			LyudFishing::FSession Ses(uint32_t(Mo * 31 + 1));
			Ses.SetBait(Worm);
			Ses.Rebait();
			Ses.SetBiteWeights(W);
			const float Rate = Ses.BitesPerMinute();
			BadRate += !std::isfinite(Rate) || Rate <= 0.f;
			// cast and wait for the bites: whoever is at the bait has a weight here
			LyudFishing::FInput In;
			In.bButton = true;
			for (int T = 0; T < 30; ++T) Ses.Step(1.f / 30.f, In);
			In.bButton = false;
			for (int T = 0; T < 120 && Ses.State() != LyudFishing::EState::Flying; ++T) Ses.Step(1.f / 30.f, In);
			Ses.SetLanded(true, 12.f);
			for (int T = 0; T < 30 * 600; ++T)
			{
				Ses.Step(1.f / 30.f, In);
				const int F = Ses.BitingFish();
				if (F >= 0)
				{
					++Bites;
					Wrong += W[F] <= 0.f;
					Ses.Rebait();
				}
			}
		}
		Check(BadRate == 0, "the session's bite rate from the web's weights is finite and positive every month (bad months)", BadRate);
		Check(Bites > 0 && Wrong == 0, "only fish with a bite weight come to the bait (bites seen)", Bites);
	}
}

static void FishCrowd(const FModel& Warm)
{
	using LyudFish::FV;
	// all year round, day and night, at the water's edge, over the weed, on the reef and offshore: the fish stay finite,
	// in the water, near the eye, their numbers bounded; the web gives their numbers (and a teeming and a bare sea)
	FEcoEnv Env;
	Env.Web = &Warm;
	LyudFish::FSim Sim;
	FModel DietWeb;
	for (int S = 0; S < LyudFish::NumSpecies(); ++S)
		for (int P = 0; P < LyudFish::NumSpecies(); ++P)
		{
			const int J = Find(LyudFish::Species(S).Id), I = Find(LyudFish::Species(P).Id);
			if (J >= 0 && I >= 0) Sim.SetDiet(S, P, DietWeb.DietShare(J, I));
		}
	const FV Spots[] = { FV(8.f, 0.f, -0.2f), FV(70.f, 10.f, -1.5f), FV(120.f, -20.f, -3.f), FV(300.f, 40.f, -8.f) };
	std::vector<int> Seen(LyudFish::NumSpecies(), 0);
	long Frames = 0;
	int NonFinite = 0, OutOfWater = 0, Far = 0, MaxCount = 0, Jumps = 0;
	std::map<uint32_t, FV> Was;
	double WorstMs = 0.0, SumMs = 0.0;
	long Steps = 0;
	for (int Mo = 1; Mo <= 12; ++Mo)
	{
		for (float Force : { -1.f, 4.f, 0.f })
		{
			Env.Force = Force;
			for (const FV& Eye : Spots)
			{
				Sim.Clear();
				Was.clear();
				LyudFish::FClock Clock;
				Clock.Month = Mo;
				const int N = Force < 0.f ? 900 : 300;
				for (int T = 0; T < N; ++T)
				{
					Clock.Daylight = 0.5f + 0.5f * std::sin(T * 0.01f + Mo);
					// a hitch now and then (a load, a breakpoint), and a paused frame
					const float Dt = T % 97 == 13 ? 0.5f : T % 89 == 7 ? 0.f : 1.f / 30.f;
					const auto T0 = std::chrono::steady_clock::now();
					Sim.Step(Dt, Env, Eye, Clock);
					const double Ms = MsSince(T0);
					if (T > 60) { WorstMs = std::max(WorstMs, Ms); SumMs += Ms; ++Steps; }
					MaxCount = std::max(MaxCount, int(Sim.Fish().size()));
					// a bottom fish moves no further in a step than its burst and the current carry it (no teleports)
					const float Step = std::min(std::max(Dt, 0.f), 0.1f);
					std::map<uint32_t, FV> Now;
					for (const LyudFish::FFish& F : Sim.Fish())
					{
						if (F.Group >= 0 || F.bHidden) continue;
						Now[F.Seed] = F.Pos;
						auto It = Was.find(F.Seed);
						if (It == Was.end()) continue;
						const float Moved = (F.Pos - It->second).Len2D();
						if (Moved > (LyudFish::Species(F.Species).Burst * 1.6f + 0.6f) * Step + 0.02f)
						{
							if (bVerbose && Jumps < 10) std::printf("  jump: %s month %d moved %.3f m in %.3f s\n", LyudFish::Species(F.Species).Id, Mo, Moved, Step);
							++Jumps;
						}
					}
					Was.swap(Now);
					for (const LyudFish::FFish& F : Sim.Fish())
					{
						++Frames;
						const bool bFin = std::isfinite(F.Pos.X) && std::isfinite(F.Pos.Y) && std::isfinite(F.Pos.Z) && std::isfinite(F.Vel.X)
							&& std::isfinite(F.Vel.Y) && std::isfinite(F.Vel.Z) && std::isfinite(F.Yaw) && std::isfinite(F.Pitch)
							&& std::isfinite(F.Phase) && std::isfinite(F.Beat) && std::isfinite(F.Bend);
						if (!bFin) { ++NonFinite; continue; }
						if ((F.Pos - Eye).Len2D() > Sim.Radius * 1.6f + 10.f) ++Far;
						if (F.bHidden) continue;
						float Bz;
						if (!Env.Bed(F.Pos.X, F.Pos.Y, Bz) || F.Pos.Z < Bz - 0.05f || F.Pos.Z > Env.Level + 0.01f)
						{
							if (bVerbose && OutOfWater < 10) std::printf("  out of water: %s month %d at %.1f %.1f %.2f\n", LyudFish::Species(F.Species).Id, Mo, F.Pos.X, F.Pos.Y, F.Pos.Z);
							++OutOfWater;
						}
					}
				}
				for (int S = 0; S < LyudFish::NumSpecies(); ++S) Seen[S] += Sim.Count(S);
			}
		}
	}
	// (the porpoise, the Baltic's rarest, is a glimpse a year: not in a few minutes a month)
	int Kinds = 0, Common = 0;
	for (int S = 0; S < LyudFish::NumSpecies(); ++S)
	{
		Common += LyudFish::Species(S).Abundance >= 0.001f;
		Kinds += Seen[S] > 0 && LyudFish::Species(S).Abundance >= 0.001f;
		if (!Seen[S]) std::printf("  never seen all year: %s\n", LyudFish::Species(S).Id);
	}
	std::printf("  %ld fish-frames, most fish at once %d, %d of %d species seen; mean step %.3f ms, worst %.3f ms\n", Frames, MaxCount,
		Kinds, LyudFish::NumSpecies(), Steps ? SumMs / Steps : 0.0, WorstMs);
	Check(NonFinite == 0, "a year of fish in every month, light, place and abundance: positions and motion finite (bad fish-frames)", NonFinite);
	Check(OutOfWater == 0, "no fish under the bed or out of the water in any month, hitches included (fish-frames)", OutOfWater);
	Check(Jumps == 0, "no bottom fish moves faster than its burst and the current in a step (fish-frames)", Jumps);
	Check(Far == 0, "no fish strays far from the eye (fish-frames beyond 1.6 radius + 10 m)", Far);
	Check(MaxCount < 4000, "the crowd stays bounded at four times the usual numbers (most at once)", MaxCount);
	Check(Kinds == Common, "every species but the rarest shows up some time in the year (kinds)", Kinds);
	Check(SumMs / std::max(Steps, 1L) < 0.6, "a fish step averages under 0.6 ms, teeming seas included (ms)", SumMs / std::max(Steps, 1L));
	// the diver swims off far (a teleport, a boat trip): the old fish go, the new spot fills
	{
		Env.Force = -1.f;
		Sim.Clear();
		LyudFish::FClock Clock;
		Clock.Month = 8;
		FV Eye(70.f, 0.f, -1.5f);
		for (int T = 0; T < 300; ++T) Sim.Step(1.f / 30.f, Env, Eye, Clock);
		Eye = FV(250.f, 2000.f, -6.f);
		int Left = 0;
		for (int T = 0; T < 90; ++T) Sim.Step(1.f / 30.f, Env, Eye, Clock);
		for (const LyudFish::FFish& F : Sim.Fish()) Left += (F.Pos - Eye).Len2D() > Sim.Radius * 1.6f + 10.f;
		Check(Left == 0 && !Sim.Fish().empty(), "three seconds after a 2 km jump no fish of the old spot is left and the new one has fish (left)", Left);
	}
	// the thinned spot shows in the water: every bottom fish taken from a cell, fewer settle there
	{
		FModel Thin = Warm;
		FEcoEnv E2;
		E2.Web = &Thin;
		LyudFish::FClock Clock;
		Clock.Month = 7;
		const FV Eye(120.f, 0.f, -3.f);
		LyudFish::FSim A, B;
		for (int T = 0; T < 120; ++T) A.Step(1.f / 30.f, E2, Eye, Clock);
		const int Goby = LyudFish::FindSpecies("RoundGoby");
		const int Before = A.Count(Goby);
		for (float X = 80.f; X < 170.f; X += 20.f)
			for (float Y = -40.f; Y < 40.f; Y += 20.f) Thin.TakeLocal(Find("RoundGoby"), X, Y, 1.f);
		for (int T = 0; T < 120; ++T) B.Step(1.f / 30.f, E2, Eye, Clock);
		Check(Before > 0 && B.Count(Goby) < Before / 4 + 1, "a reef fished bare of gobies shows few gobies to the diver (after/before)", double(B.Count(Goby)) / std::max(Before, 1));
	}
}

int main(int Argc, char** Argv)
{
	bVerbose = Argc > 1 && std::strcmp(Argv[1], "-v") == 0;
	WebShape();
	// the web as a game sees it: the base warmed through six years of seasons
	FModel Warm;
	Warm.Reset(293.f - 6.f * 365.f);
	Warm.Step(293.f - 6.f * 365.f, 6.f * 365.f);
	ExtremeSteps(Warm);
	HeavyFishing(Warm);
	LocalThinning(Warm);
	SaveLoad(Warm);
	BiteWeightChecks(Warm);
	FishCrowd(Warm);
	std::printf(Fails ? "%d FAILED\n" : "all ok\n", Fails);
	return Fails ? 1 : 0;
}
