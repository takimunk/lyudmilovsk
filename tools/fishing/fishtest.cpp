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

static FResult Fish(const std::vector<float>& W, const char* BaitItem, EBot Bot, float Minutes, uint32_t Seed, bool bVerbose = false,
	const char* Rod = "fishing_rod", const FConditions* Cond = nullptr)
{
	FSession S(Seed);
	S.SetTackle(FindTackle(Rod));
	if (Cond) S.SetConditions(*Cond, {});
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
static bool FightOne(int Species, float Scale, EBot Bot, uint32_t Seed, float& OutSecs, int& OutWhy, const char* Rod = "fishing_rod")
{
	FSession S(Seed);
	S.SetTackle(FindTackle(Rod));
	std::vector<float> W(NumCatchable(), 0.f);
	W[Species] = 1.f;
	S.SetBiteWeights(W);
	S.SetBait(FindBait("bait_worm"));
	S.Rebait();
	const float Dt = 1.f / 30.f;
	bool bHold = false;
	float T = 0.f;
	// cast far; once the float is out, that fish is on (waiting for a salmon to find a worm takes too long for a test)
	for (int Guard = 0; Guard < 30 * 60 && S.State() != EState::Waiting; Guard++)
	{
		FInput In;
		In.bButton = (S.State() == EState::Idle && S.Time() > 0.2f) || (S.State() == EState::Charging && S.Charge() < 1.f);
		S.Step(Dt, In);
	}
	S.DebugHook(Species);
	float Look = 0.f;
	(void)Scale;
	for (; T < 600.f; T += Dt)
	{
		FInput In;
		if (Bot == EBot::Greedy) In.bButton = true;
		else if (Bot == EBot::Lazy) In.bButton = false;
		else if (Bot == EBot::Jumpy)
		{
			// a person: looks at the rod every 0.25 s and acts on what was there
			Look -= Dt;
			if (Look <= 0.f)
			{
				Look = 0.25f;
				if (bHold) bHold = S.Tension() < 0.8f;
				else bHold = S.Tension() < 0.5f && !S.IsRunning();
			}
			In.bButton = bHold;
		}
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
		int Gar = 0;
		for (uint32_t Seed : { 5u, 6u, 7u }) Gar += Fish(Weights(Spots[2], 5, 1.f), "lure_spoon", EBot::Good, 120.f, Seed).Species["Garfish"];
		Check(Gar > 0, "a spoon off the pier in May catches garfish");
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

	std::printf("== rods\n");
	Check(NumTackle() == 3 && FindTackle("rod_spinning") > 0 && FindTackle("rod_sea") > 0, "three rods");
	{
		FSession S(1);
		S.SetBait(FindBait("lure_spoon"));
		const float Basic = S.MaxCast();
		S.SetTackle(FindTackle("rod_spinning"));
		const float Spin = S.MaxCast();
		S.SetBait(FindBait("lure_wobbler"));
		const float Wob = S.MaxCast();
		S.SetTackle(FindTackle("rod_sea"));
		S.SetBait(FindBait("bait_worm"));
		const float Sea = S.MaxCast();
		std::printf("    casts: spoon basic %.0f m, spinning %.0f m (wobbler %.0f m), sea rod with a worm %.0f m\n", Basic, Spin, Wob, Sea);
		Check(Spin > Basic + 8.f && Wob < Spin && Sea > 40.f, "the spinning rod casts a spoon further, a wobbler less; the sea rod's sinker goes far out");
	}
	for (const char* Id : { "Cod", "SeaTrout", "Salmon" })
	{
		const int C = FindCatchable(Id);
		int Landed[3] = { 0, 0, 0 }, Snaps[3] = { 0, 0, 0 };
		const char* Rods[3] = { "fishing_rod", "rod_spinning", "rod_sea" };
		for (int K = 0; K < 3; K++)
		{
			for (int I = 0; I < 200; I++)
			{
				float Sec = 0.f;
				int Why = 0;
				if (FightOne(C, 1.f, EBot::Jumpy, 300u + I * 17u, Sec, Why, Rods[K])) Landed[K]++;
				int Why2 = 0;
				FightOne(C, 1.f, EBot::Greedy, 300u + I * 17u, Sec, Why2, Rods[K]);
				if (Why2 == 1) Snaps[K]++;
			}
		}
		std::printf("  %-9s a slow angler landed: basic %3d%%  spinning %3d%%  sea %3d%%;  greedy snapped: %d / %d / %d\n", Id, Landed[0] / 2, Landed[1] / 2, Landed[2] / 2, Snaps[0], Snaps[1], Snaps[2]);
		Check(Landed[1] >= Landed[0] && Snaps[1] <= Snaps[0] && Snaps[2] <= Snaps[1], (std::string(Id) + ": a stronger rod lands more and breaks less").c_str());
	}
	{
		const std::vector<float> W = Weights(Spots[1], 10, 0.f);
		FResult Float = Fish(W, "bait_worm", EBot::Good, 180.f, 21);
		FResult Sea = Fish(W, "bait_worm", EBot::Good, 180.f, 21, false, "rod_sea");
		Print("stones, October night, worm, float rod", Float);
		Print("stones, October night, worm, sea rod", Sea);
		const int FloatBig = Float.Species["Cod"] + Float.Species["Eelpout"] + Float.Species["Eel"];
		const int SeaBig = Sea.Species["Cod"] + Sea.Species["Eelpout"] + Sea.Species["Eel"];
		Check(SeaBig > FloatBig, "the sea bottom rod brings more cod, eelpout and eel than a float");
		const std::vector<float> Day = Weights(Spots[0], 8, 1.f);
		FResult Mag = Fish(Day, "bait_maggot", EBot::Good, 120.f, 22);
		FResult MagSea = Fish(Day, "bait_maggot", EBot::Good, 120.f, 22, false, "rod_sea");
		Check(MagSea.Species["Herring"] + MagSea.Species["Sprat"] + MagSea.Species["Smelt"] <= Mag.Species["Herring"] + Mag.Species["Sprat"] + Mag.Species["Smelt"],
			"small fish high in the water do not find a bait on the bottom");
	}
	std::printf("== new baits\n");
	Check(BaitLiking(FindCatchable("Cod"), FindBait("bait_shrimp")) > BaitLiking(FindCatchable("Cod"), FindBait("bait_worm")), "cod: shrimp over worms");
	Check(BaitLiking(FindCatchable("Roach"), FindBait("bait_dough")) > 0.9f && BaitLiking(FindCatchable("Cod"), FindBait("bait_dough")) < 0.05f, "dough: roach yes, cod no");
	Check(BaitLiking(FindCatchable("SeaTrout"), FindBait("lure_wobbler")) > BaitLiking(FindCatchable("SeaTrout"), FindBait("lure_spoon")), "sea trout: a wobbler over a spoon");
	{
		FResult Dough = Fish(Weights(Spots[1], 8, 1.f), "bait_dough", EBot::Good, 120.f, 31);
		Print("stones and weed, August, dough", Dough);
		int Mixed = 0;
		for (auto& P : Dough.Species) { const int C = FindCatchable(P.first.c_str()); if (C >= 0 && Catchable(C).Diet == EDiet::Mixed) Mixed += P.second; }
		Check(Dough.Caught == 0 || Mixed * 2 >= Dough.Caught, "dough catches mostly roach, vimba, whitefish, perch");
		FResult Wob = Fish(Weights(Spots[2], 5, 1.f), "lure_wobbler", EBot::Good, 120.f, 32, false, "rod_spinning");
		Print("pier head, May, wobbler, spinning rod", Wob);
		Check(Wob.Species.count("Garfish") || Wob.Species.count("SeaTrout") || Wob.Species.count("Perch"), "a wobbler in May takes garfish, sea trout or perch");
	}
	std::printf("== light and weather\n");
	{
		FConditions Grey;
		Grey.SunAltitude = 25.f; Grey.Cloud = 0.8f; Grey.Wind = 4.f;
		FConditions Dusk = Grey; Dusk.SunAltitude = 0.5f;
		FConditions Noon; Noon.SunAltitude = 50.f; Noon.Cloud = 0.f; Noon.Wind = 0.5f;
		FConditions Front = Grey; Front.PressureTendency = -3.f; Front.Rain = 1.f;
		FConditions Gale = Grey; Gale.Wind = 16.f;
		FConditions Storm = Grey; Storm.Thunder = 1.f; Storm.Rain = 12.f;
		FConditions Murky = Grey; Murky.Murk = 2.5f;
		const int Trout = FindCatchable("SeaTrout"), Goby = FindCatchable("RoundGoby"), Roach = FindCatchable("Roach");
		std::printf("    sea trout on a spoon: grey %.2f dusk %.2f noon %.2f front %.2f gale %.2f storm %.2f murky %.2f\n",
			BiteMood(Grey, Trout, true), BiteMood(Dusk, Trout, true), BiteMood(Noon, Trout, true), BiteMood(Front, Trout, true),
			BiteMood(Gale, Trout, true), BiteMood(Storm, Trout, true), BiteMood(Murky, Trout, true));
		Check(BiteMood(Dusk, Trout, true) > 1.4f * BiteMood(Grey, Trout, true), "the evening rise: sea trout keener at dusk");
		Check(BiteMood(Dusk, Goby, false) < BiteMood(Dusk, Trout, true), "gobies care little for the dusk");
		Check(BiteMood(Noon, Roach, false) < 0.85f * BiteMood(Grey, Roach, false), "a bright still noon puts shy roach off");
		Check(BiteMood(Front, Trout, true) > 1.2f * BiteMood(Grey, Trout, true), "a falling glass and soft rain: the hunters feed");
		Check(BiteMood(Gale, Trout, true) < BiteMood(Grey, Trout, true) && BiteMood(Storm, Trout, true) < 0.6f, "a gale and a thunderstorm put them down");
		Check(BiteMood(Murky, Trout, true) < BiteMood(Grey, Trout, true) && BiteMood(Murky, Goby, false) == BiteMood(Grey, Goby, false), "murky water hides a lure, not a worm");
		Check(MoodWhy(Dusk) == EMoodWhy::Twilight && MoodWhy(Front) == EMoodWhy::Falling && MoodWhy(Storm) == EMoodWhy::Storm && MoodWhy(Noon) == EMoodWhy::Bright, "the forecast's reasons");
		Check(Hunger(0.5f) > 1.2f && Hunger(2.f) < 0.8f && Hunger(1.f) == 1.f, "a hungry fish takes more readily, a full one less");
		const std::vector<float> W = Weights(Spots[2], 9, 1.f);
		FResult G = Fish(W, "lure_spoon", EBot::Good, 180.f, 41, false, "rod_spinning", &Grey);
		FResult D = Fish(W, "lure_spoon", EBot::Good, 180.f, 41, false, "rod_spinning", &Dusk);
		std::printf("    pier head, September, spoon: grey day %d bites, dusk %d bites\n", G.Bites, D.Bites);
		Check(D.Bites > G.Bites, "more bites at dusk than on a grey noon");
	}
	std::printf("== the restaurant\n");
	{
		const int Cod = FindCatchable("Cod"), Goby = FindCatchable("RoundGoby");
		Check(RestaurantPrice(Cod, 1500.f, 0.52f) > PriceOf(Cod, 1500.f), "the kitchen pays more for a good cod");
		Check(RestaurantPrice(Cod, 300.f, 0.30f) == 0, "but not for a small one");
		Check(RestaurantPrice(Goby, 60.f, 0.16f) == 0, "nor for a goby");
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
