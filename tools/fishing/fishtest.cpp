// Fishing model checks outside the engine (docs/fishing.md): tools/fishing/run.sh [-v]
// The bite weights are made the way ULyudEcosystem::GetBiteWeights makes them (the web's numbers taken as usual),
// times the school factor ULyudFishing applies; then bots fish: a good angler, a greedy one (always winding), a lazy
// one (never winding), one who strikes at every twitch.
#include "FishingModel.h"
#include "FishModel.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace LyudFishing;

static int Fails = 0;
static void Check(bool bOk, const char* What)
{
	std::printf("%s %s\n", bOk ? "ok  " : "FAIL", What);
	if (!bOk) Fails++;
}

struct FSpot { const char* Name; float Sand, Stone, Weed, Depth; };

// as ULyudFishing::BiteWeights: the fish model's species weights mapped to the catchable table
static std::vector<float> Weights(const FSpot& S, int Month, float Daylight)
{
	std::vector<float> W(NumCatchable(), 0.f);
	LyudFish::FClock Clock;
	Clock.Month = Month;
	Clock.Daylight = Daylight;
	for (int I = 0; I < LyudFish::NumSpecies(); I++)
	{
		const LyudFish::FSpecies& Sp = LyudFish::Species(I);
		if (Sp.Way == LyudFish::EWay::Crawler || Sp.Way == LyudFish::EWay::Swarm || Sp.bBreathes) continue;
		const int C = FindCatchable(Sp.Id);
		if (C < 0) continue;
		const float Wt = LyudFish::Suitability(I, S.Sand, S.Stone, S.Weed, S.Depth) * LyudFish::Activity(I, Clock) * Sp.Abundance;
		W[C] = Wt * std::sqrt(0.5f * float(Sp.GroupMin + Sp.GroupMax));
	}
	return W;
}

enum class EBot { Good, Greedy, Lazy, Jumpy };

struct FResult
{
	int Casts = 0, Bites = 0, Hooked = 0, Caught = 0, Snapped = 0, Escaped = 0, Gone = 0, Missed = 0, Stolen = 0;
	float Seconds = 0.f, Grams = 0.f;
	std::map<std::string, int> Species;
	float BiggestG = 0.f;
	std::string Biggest;
};

static FResult Fish(const std::vector<float>& W, const char* BaitItem, EBot Bot, float Minutes, uint32_t Seed, bool bVerbose = false)
{
	FSession S(Seed);
	FRand R(Seed * 7 + 3);
	S.SetBait(FindBait(BaitItem));
	S.Rebait();
	S.SetBiteWeights(W);
	FResult Res;
	const float Dt = 1.f / 30.f;
	float React = -1.f;
	bool bHold = false;
	float CastCharge = R.Range(0.6f, 1.f);
	const bool bLure = S.GetBait() >= 0 && Bait(S.GetBait()).bLure;
	for (float T = 0.f; T < Minutes * 60.f; T += Dt)
	{
		FInput In;
		switch (S.State())
		{
		case EState::Idle:
			if (!S.IsBaited() && !bLure) S.Rebait();
			if (bLure && S.GetBait() < 0) { S.SetBait(FindBait(BaitItem)); }
			In.bButton = S.Time() > 0.5f;       // a moment, then a new cast
			CastCharge = R.Range(0.5f, 1.f);
			break;
		case EState::Charging:
			In.bButton = S.Charge() < CastCharge;
			break;
		case EState::Waiting:
			In.bButton = bLure;      // the spoon is wound in steadily
			if (!bLure && !S.IsBaited()) In.bReelIn = true;   // a bare hook: in and bait it
			if (Bot == EBot::Jumpy && !bLure && R.U() < 0.002f) In.bButton = true;
			break;
		case EState::Nibbling:
			In.bButton = bLure || (Bot == EBot::Jumpy);
			break;
		case EState::Taking:
			if (React < 0.f) React = R.Range(0.18f, 0.45f);
			React -= Dt;
			In.bButton = bLure ? true : (React <= 0.f && Bot != EBot::Lazy) || (Bot == EBot::Lazy && React < -0.1f);
			break;
		case EState::Fighting:
			if (Bot == EBot::Greedy) In.bButton = true;
			else if (Bot == EBot::Lazy) In.bButton = R.U() < 0.15f;
			else
			{
				// a good angler: wind while the rod is not bent too far, let it run
				if (bHold) bHold = S.Tension() < 0.8f;
				else bHold = S.Tension() < 0.5f && !S.IsRunning();
				In.bButton = bHold;
			}
			break;
		default:
			break;
		}
		if (S.State() != EState::Taking) React = -1.f;
		S.Step(Dt, In);
		for (EEvent E : S.Events())
		{
			switch (E)
			{
			case EEvent::Cast: Res.Casts++; break;
			case EEvent::Take: Res.Bites++; break;
			case EEvent::Hooked: Res.Hooked++; break;
			case EEvent::Snapped: Res.Snapped++; break;
			case EEvent::Escaped: Res.Escaped++; break;
			case EEvent::Gone: Res.Gone++; break;
			case EEvent::Missed: Res.Missed++; break;
			case EEvent::BaitStolen: Res.Stolen++; break;
			case EEvent::Caught:
			{
				Res.Caught++;
				const FFish& F = S.Fish();
				Res.Grams += F.Grams;
				Res.Species[Catchable(F.Species).Id]++;
				if (F.Grams > Res.BiggestG) { Res.BiggestG = F.Grams; Res.Biggest = Catchable(F.Species).Id; }
				if (bVerbose) std::printf("    %6.1f s  %-20s %5.1f cm %7.0f g  %d rub\n", T, Catchable(F.Species).Id, F.Length * 100.f, F.Grams, PriceOf(F.Species, F.Grams));
				break;
			}
			default: break;
			}
		}
	}
	Res.Seconds = Minutes * 60.f;
	return Res;
}

static void Print(const char* Title, const FResult& R)
{
	std::printf("  %-34s casts %3d bites %3d hooked %3d caught %3d (%.1f/h, %.2f kg) snapped %d escaped %d gone %d missed %d stolen %d; biggest %s %.0f g\n",
		Title, R.Casts, R.Bites, R.Hooked, R.Caught, R.Caught * 3600.f / R.Seconds, R.Grams / 1000.f, R.Snapped, R.Escaped, R.Gone, R.Missed, R.Stolen,
		R.Biggest.c_str(), R.BiggestG);
	std::string L = "      ";
	for (auto& P : R.Species) L += P.first + " " + std::to_string(P.second) + "  ";
	std::printf("%s\n", L.c_str());
}

// one fish on the line, fought by a bot: landed?
static bool FightOne(int Species, float Scale, EBot Bot, uint32_t Seed, float& OutSecs, int& OutWhy)
{
	FSession S(Seed);
	std::vector<float> W(NumCatchable(), 0.f);
	W[Species] = 1.f;
	S.SetBiteWeights(W);
	S.SetBait(FindBait("bait_worm"));
	S.Rebait();
	const float Dt = 1.f / 30.f;
	bool bHold = false;
	float T = 0.f;
	// cast far, wait for the take, strike
	for (int Guard = 0; Guard < 30 * 600 && S.State() != EState::Fighting; Guard++)
	{
		FInput In;
		In.bButton = (S.State() == EState::Idle && S.Time() > 0.2f) || (S.State() == EState::Charging && S.Charge() < 1.f) || (S.State() == EState::Taking);
		if (S.State() == EState::Waiting && !S.IsBaited()) In.bReelIn = true;
		if (S.State() == EState::Idle) S.Rebait();
		S.Step(Dt, In);
	}
	(void)Scale;
	for (; T < 600.f; T += Dt)
	{
		FInput In;
		if (Bot == EBot::Greedy) In.bButton = true;
		else if (Bot == EBot::Lazy) In.bButton = false;
		else
		{
			if (bHold) bHold = S.Tension() < 0.8f;
			else bHold = S.Tension() < 0.5f && !S.IsRunning();
			In.bButton = bHold;
		}
		S.Step(Dt, In);
		for (EEvent E : S.Events())
		{
			if (E == EEvent::Caught) { OutSecs = T; OutWhy = 0; return true; }
			if (E == EEvent::Snapped) { OutSecs = T; OutWhy = 1; return false; }
			if (E == EEvent::Escaped) { OutSecs = T; OutWhy = 2; return false; }
		}
	}
	OutWhy = 3;
	return false;
}

int main(int Argc, char** Argv)
{
	const bool bVerbose = Argc > 1 && !std::strcmp(Argv[1], "-v");
	std::printf("== tables\n");
	Check(NumCatchable() > 20, "a table of catchable fish");
	for (int I = 0; I < NumCatchable(); I++)
	{
		if (LyudFish::FindSpecies(Catchable(I).Id) < 0) { std::printf("    %s not in the fish model\n", Catchable(I).Id); Check(false, "every catchable fish swims in the fish model"); }
	}
	Check(ItemOf("RoundGoby") == "fish_roundgoby", "item ids");
	Check(BaitLiking(FindCatchable("Cod"), FindBait("lure_spoon")) > BaitLiking(FindCatchable("Cod"), FindBait("bait_maggot")), "cod: spoon over maggots");
	Check(BaitLiking(FindCatchable("Roach"), FindBait("lure_spoon")) < 0.05f, "roach never take a spoon");
	Check(BaitLiking(FindCatchable("Flounder"), FindBait("bait_worm")) >= 0.9f, "flounder take worms");
	{
		FRand R(5);
		float Sum = 0.f, Max = 0.f;
		const int C = FindCatchable("Perch");
		for (int I = 0; I < 2000; I++) { FFish F = RollFish(C, R); Sum += F.Grams; Max = std::max(Max, F.Grams); }
		std::printf("    perch: mean %.0f g, biggest %.0f g\n", Sum / 2000.f, Max);
		Check(Sum / 2000.f > 90.f && Sum / 2000.f < 260.f && Max < 1500.f, "perch weigh like perch (a 22 cm perch ~140 g)");
		FFish Cod = RollFish(FindCatchable("Cod"), R);
		std::printf("    a cod: %.0f cm %.0f g pull %.2f endurance %.0f s\n", Cod.Length * 100.f, Cod.Grams, Cod.Pull, Cod.Endurance);
		Check(PriceOf(FindCatchable("Cod"), 1200.f) > PriceOf(FindCatchable("RoundGoby"), 50.f), "a cod sells for more than a goby");
		Check(PriceOf(FindCatchable("SandGoby"), 5.f) == 0, "nobody buys a sand goby");
	}

	const FSpot Spots[] = {
		{ "sandy beach 2 m", 1.f, 0.05f, 0.1f, 2.f },
		{ "stones and weed 3 m", 0.2f, 0.9f, 0.7f, 3.f },
		{ "pier head 6 m", 0.6f, 0.5f, 0.3f, 6.f },
	};
	std::printf("== an hour at each spot (good angler)\n");
	for (const FSpot& Sp : Spots)
	{
		for (int Month : { 5, 8, 10 })
		{
			const std::vector<float> W = Weights(Sp, Month, 1.f);
			for (const char* B : { "bait_worm", "bait_maggot", "lure_spoon" })
			{
				char Title[160];
				std::snprintf(Title, sizeof Title, "%s, month %d, %s", Sp.Name, Month, B);
				FResult R = Fish(W, B, EBot::Good, 60.f, 11u + Month, bVerbose && Month == 8);
				Print(Title, R);
			}
		}
	}
	{
		const std::vector<float> W = Weights(Spots[0], 8, 1.f);
		FResult Worm = Fish(W, "bait_worm", EBot::Good, 120.f, 3);
		FResult Bare = Fish(W, "nothing", EBot::Good, 120.f, 3);
		Check(Worm.Caught >= 8 && Worm.Caught <= 120, "a summer's two hours on the beach with worms: a fair catch (8..120)");
		Check(Bare.Caught < Worm.Caught / 4 + 1, "a bare hook catches next to nothing");
		const std::vector<float> Night = Weights(Spots[1], 10, 0.f);
		FResult N = Fish(Night, "bait_worm", EBot::Good, 120.f, 4);
		Print("stones, October night, worm", N);
		Check(N.Species.count("Cod") || N.Species.count("Eelpout") || N.Species.count("Eel"), "an autumn night on the stones brings cod, eelpout or eel");
		FResult Sp = Fish(Weights(Spots[2], 5, 1.f), "lure_spoon", EBot::Good, 120.f, 5);
		Check(Sp.Species.count("Garfish") > 0, "a spoon off the pier in May catches garfish");
		FResult Jumpy = Fish(W, "bait_worm", EBot::Jumpy, 120.f, 3);
		Print("jumpy striker, beach August", Jumpy);
		Check(Jumpy.Caught < Worm.Caught, "striking at every twitch catches less");
		FResult Lazy = Fish(W, "bait_worm", EBot::Lazy, 120.f, 3);
		Print("lazy (late strike, no winding)", Lazy);
		Check(Lazy.Caught < Worm.Caught, "the lazy angler catches less");
	}

	std::printf("== fights (good / greedy / lazy), 200 fish each\n");
	struct FCase { const char* Id; };
	for (const char* Id : { "RoundGoby", "Perch", "Flounder", "Cod", "SeaTrout", "Salmon" })
	{
		const int C = FindCatchable(Id);
		int Landed[3] = { 0, 0, 0 }, Snaps[3] = { 0, 0, 0 };
		float Secs[3] = { 0, 0, 0 };
		for (int B = 0; B < 3; B++)
		{
			for (int I = 0; I < 200; I++)
			{
				float S = 0.f;
				int Why = 0;
				if (FightOne(C, 1.f, B == 0 ? EBot::Good : (B == 1 ? EBot::Greedy : EBot::Lazy), 100u + I * 13u, S, Why)) { Landed[B]++; Secs[B] += S; }
				if (Why == 1) Snaps[B]++;
			}
		}
		std::printf("  %-10s good %3d%% (%.0f s)  greedy %3d%% (snapped %d)  lazy %3d%%\n", Id, Landed[0] / 2, Landed[0] ? Secs[0] / Landed[0] : 0.f,
			Landed[1] / 2, Snaps[1], Landed[2] / 2);
		if (!std::strcmp(Id, "RoundGoby")) Check(Landed[0] > 180 && Landed[1] > 180, "a goby comes in whatever you do");
		if (!std::strcmp(Id, "Perch")) Check(Landed[0] > 160, "a good angler lands most perch");
		if (!std::strcmp(Id, "Cod")) Check(Landed[0] > 120 && Landed[0] > Landed[1], "cod: a good angler lands most, the greedy one breaks more");
		if (!std::strcmp(Id, "Salmon")) Check(Landed[0] > 40 && Snaps[1] > 120, "salmon: hard but possible; winding through its runs breaks the line");
		if (!std::strcmp(Id, "SeaTrout")) Check(Landed[2] < Landed[0], "sea trout: letting it do as it likes loses it");
	}

	std::printf("== cost\n");
	{
		FSession S(9);
		S.SetBiteWeights(Weights(Spots[0], 8, 1.f));
		S.SetBait(0);
		S.Rebait();
		const auto T0 = std::chrono::high_resolution_clock::now();
		FInput In;
		for (int I = 0; I < 1000000; I++) { In.bButton = (I / 40) % 3 == 0; S.Step(1.f / 60.f, In); }
		const double Us = std::chrono::duration<double, std::micro>(std::chrono::high_resolution_clock::now() - T0).count() / 1e6;
		std::printf("    a step %.3f us\n", Us);
		Check(Us < 2.0, "a step costs nothing (< 2 us)");
	}
	std::printf(Fails ? "FAILED %d\n" : "ALL OK\n", Fails);
	return Fails ? 1 : 0;
}
