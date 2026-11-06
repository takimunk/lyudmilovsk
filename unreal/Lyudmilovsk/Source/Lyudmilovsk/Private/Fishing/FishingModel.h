// Fishing with a rod, in plain C++ (no Unreal types): tools/fishing/run.sh runs it outside the engine. docs/fishing.md.
//
// What is here: the tackle (the float rod's baits and the spoon), which fish likes which, how big a caught fish is,
// and one session at the water: the cast's charge, the float waiting, nibbles and the take, the strike, the fight on
// the line (the fish's runs and rests, its strength and tiring, the line's tension, the reel), the landing.
// What takes the bait comes from the food web (ULyudEcosystem::GetBiteWeights): the caller hands the weights in.
// Seconds, metres, grams. Tension 1 = the line's breaking strain.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace LyudFishing
{
	/** What a fish eats, which decides the bait it takes. */
	enum class EDiet : uint8_t
	{
		Plankton,   // herring, sprat, smelt, sticklebacks: small baits (maggots), barely a worm
		Bottom,     // gobies, flounder, eelpout, eel, sculpins: worms on the bottom
		Mixed,      // perch, roach, vimba, whitefish: worms and maggots
		Hunter,     // cod, sea trout, salmon, garfish, turbot, shad: the spoon, worms for cod and turbot
		None,       // pipefish: nothing on a hook
	};

	/** A fish one can catch: its fishing side (the swimming side is LyudFish::FSpecies of the same Id). */
	struct FCatchable
	{
		const char* Id;
		EDiet Diet;
		/** Length of an adult as the fish model has it, m; a catch is mostly 0.6..1.3 of it, rarely up to Max of it. */
		float Length;
		float Max;
		/** Weight = K * L(cm)^3 / 100 g (Fulton's condition: ~1 for a perch, 0.15 for an eel, 2.5 for a lumpsucker). */
		float K;
		/** How it fights for its weight (a sea trout jumps and runs, a flounder is a heavy plate, an eel twists). */
		float Fight;
		/** Nibbles before it takes (0 bold .. 1 shy), and how long the float stays under on the take, s. */
		float Shy;
		float TakeTime;
		/** Roubles a kilo at the Gastronom (and at least MinPrice for one). */
		int PerKilo;
		int MinPrice;
	};

	int NumCatchable();
	const FCatchable& Catchable(int Index);
	int FindCatchable(const char* Id);
	/** The inventory item of a caught fish: "fish_" + the id in lower case ("fish_roundgoby"). */
	std::string ItemOf(const char* Id);

	/** A bait or a lure. Baits go on the hook and are used up (stolen, eaten, the fish caught); the spoon stays (lost with the line). */
	struct FBait
	{
		const char* Item;
		/** Cast and retrieved: bites only while it is wound in (no float). */
		bool bLure;
		/** Liking per diet (Plankton, Bottom, Mixed, Hunter). */
		float Like[4];
	};
	int NumBaits();
	const FBait& Bait(int Index);
	int FindBait(const char* Item);
	/** How much species Catchable index takes this bait (0 never). */
	float BaitLiking(int Fish, int BaitIndex);

	/** A small seeded random (the same seed, the same session: tests). */
	struct FRand
	{
		uint32_t S = 1;
		explicit FRand(uint32_t Seed = 1) : S(Seed ? Seed : 1) {}
		uint32_t Next() { S ^= S << 13; S ^= S >> 17; S ^= S << 5; return S; }
		float U() { return float(Next() & 0xFFFFFF) / float(0x1000000); }
		float Range(float A, float B) { return A + (B - A) * U(); }
		float Normal();
	};

	/** One fish on the hook: what, how long, how heavy, and its fighting numbers. */
	struct FFish
	{
		int Species = -1;
		float Length = 0.f;
		float Grams = 0.f;
		/** Its full pull (tension units) and its stamina, s of hard running. */
		float Pull = 0.f;
		float Endurance = 0.f;
		/** Speed of a run, m/s. */
		float RunSpeed = 0.f;
	};
	/** A fish of a species as it comes on the hook. */
	FFish RollFish(int Species, FRand& R);
	/** The Gastronom's price of a fish of this weight, roubles. */
	int PriceOf(int Species, float Grams);

	/** Things that happen, in order, for the caller's sounds, words and moves. */
	enum class EEvent : uint8_t
	{
		CastStart,   // the rod goes back
		Cast,        // the line is out: Distance is how far (the caller flies the float there)
		Landed,      // the float (or the spoon) is in the water
		Ashore,      // it fell on land: wound back
		Nibble,      // the float twitches
		Take,        // the float goes under (or the spoon is hit): strike now
		Hooked,
		Missed,      // a strike at nothing (or too early: the fish is gone)
		Gone,        // the take was missed: the fish let go
		BaitStolen,  // the hook comes back bare
		Run,         // a hooked fish runs
		Snapped,     // the line broke: the fish and the hook are gone (the spoon too)
		Escaped,     // the hook came out (slack, or it shook its head)
		Landing,     // the fish is at the feet: lifted out
		Caught,
		Retrieved,   // the float (spoon) is back at the rod
	};

	enum class EState : uint8_t
	{
		Idle,       // the rod in the hand, the line wound in
		Charging,   // the button held: the cast's swing back and strength
		Swing,      // the rod comes forward, the line goes out
		Flying,     // the caller flies the float; SetLanded ends it
		Waiting,    // the float on the water (the spoon being wound in)
		Nibbling,   // a fish is at the bait
		Taking,     // the float is under: the strike window
		Fighting,
		Landing,
		Winding,    // the empty line wound back (nothing on it)
	};

	struct FInput
	{
		/** The left button (cast: hold and let go; strike: a press; fight: held = wind the reel, up = the clutch lets line out). */
		bool bButton = false;
		/** The right button: wind the line back in (abort a wait). */
		bool bReelIn = false;
	};

	/**
	 * One rod at the water. Each frame: Step(dt, input). The caller supplies the bite weights where the float lies
	 * (SetBiteWeights: Catchable index -> weight, as ULyudEcosystem::GetBiteWeights gives, refresh every few seconds),
	 * flies the float after Cast and reports where it fell (SetLanded). Events() lists this step's events.
	 */
	class FSession
	{
	public:
		explicit FSession(uint32_t Seed = 1) : R(Seed) {}

		/** The bait on the hook (FindBait index, -1 none: a bare hook catches next to nothing). */
		void SetBait(int BaitIndex) { Bait = BaitIndex; }
		int GetBait() const { return Bait; }
		/** Is there bait on the hook (a lure always; a bait until it is stolen or a fish is caught). */
		bool IsBaited() const { return Bait >= 0 && bBaited; }
		/** The caller put a fresh bait on (and took one from the pocket). */
		void Rebait() { bBaited = Bait >= 0; }
		void Unbait() { bBaited = false; }

		void SetBiteWeights(const std::vector<float>& W) { Weights = W; }
		/** The cast reached Water (or land) at Distance m from the rod tip. */
		void SetLanded(bool bWater, float Distance);

		void Step(float Dt, const FInput& In);
		const std::vector<EEvent>& Events() const { return Ev; }

		EState State() const { return St; }
		/** The cast's charge 0..1 (Charging), the throw's distance it gives, m. */
		float Charge() const { return ChargeT; }
		float CastDistance() const { return Throw; }
		/** How far the float, spoon or fish is from the rod tip, m (on the water's surface). */
		float Distance() const { return Dist; }
		/** The float: 0 riding, ~0.3 a twitch, 1 under. */
		float FloatDip() const { return Dip; }
		/** The line's tension (1: it breaks); the rod bends with it. */
		float Tension() const { return Tens; }
		/** The fish's way off the straight line out, -1..1 (it swims left and right); its depth 0..1. */
		float Side() const { return SideNow; }
		bool IsRunning() const { return bRun; }
		/** The fish on the line (Fighting, Landing) and the one last caught (after Caught). */
		const FFish& Fish() const { return Hook; }
		/** The fish's strength left, 0..1. */
		float Stamina() const { return Stam; }
		/** Seconds in this state. */
		float Time() const { return T; }

		/** Max cast with this tackle, m. */
		float MaxCast() const;
		/** Mean bites a minute here now with this bait (for the prompt and tests). */
		float BitesPerMinute() const;

		/** Checks: a take now on the waiting float (Fish: a Catchable index), or that fish on the line now. */
		void DebugBite(int Fish);
		void DebugHook(int Fish);
		/** The line pulled in to Distance (the hero walked off with it). */
		void SetDistance(float Distance) { Dist = Distance; }
		/** Wind everything in now (the rod put away, a screen opened): no fish, no events. */
		void Reset();

		/** Tuning (tests may change them). */
		struct FTune
		{
			float ChargeTime = 1.1f;     // s to a full swing
			float MinCast = 3.f;
			float FloatCast = 18.f;      // m, a float rig at full swing
			float LureCast = 30.f;
			float SwingTime = 0.35f;
			float BiteEvery = 100.f;     // s: half the bite rate's ceiling (a rich spot with a liked bait: a bite a minute or so)
			float RichWeight = 0.8f;     // the summed weight (x liking) at which bites come at half their ceiling
			float LureEvery = 40.f;      // the same for a spoon, in seconds of winding
			float ReelSpeed = 1.1f;      // m/s wound in, nothing on
			float FightReel = 0.75f;     // m/s against a fish at rest
			float ClutchCap = 0.62f;     // tension the clutch holds when the button is up
			float SnapAfter = 0.22f;     // s over 1 that break the line
			float SlackTension = 0.07f;
			float SlackAfter = 2.2f;     // s of slack before the hook may come out
			float LandAt = 1.8f;         // m: lifted out
			float LineLength = 80.f;     // all the line on the reel
		} Tune;

	private:
		void Go(EState S);
		void Emit(EEvent E) { Ev.push_back(E); }
		float RichNow(bool bLure) const;
		int PickFish();
		void StartBite();
		void Hooked();
		void FightStep(float Dt, bool bReel);

		FRand R;
		EState St = EState::Idle;
		float T = 0.f;
		std::vector<EEvent> Ev;
		std::vector<float> Weights;
		int Bait = -1;
		bool bBaited = false;
		bool bWasDown = false;
		float ChargeT = 0.f, Throw = 0.f, Dist = 0.f, Dip = 0.f, Tens = 0.f, SideNow = 0.f;
		// a bite on its way: the fish, nibbles left, time to the next one
		int BiteFish = -1, Nibbles = 0;
		float NextBite = -1.f, NextNibble = 0.f, NibbleT = 0.f, TakeLeft = 0.f, Calm = 0.f;
		// the fight
		FFish Hook;
		float Stam = 1.f, RunLeft = 0.f, RestLeft = 0.f, OverT = 0.f, SlackT = 0.f, SideGoal = 0.f;
		bool bRun = false;
	};
}
