// The crow family's own life on top of the bird life (docs/corvids.md): rooks, crows, jackdaws, magpies, ravens, jays and
// nutcrackers live like the other birds (BirdSim.cpp: groups, sites, the day, fear), and besides that each one has a small
// mind of its own: needs (hunger, the urge to hoard, company, play), a memory (where it hid things, whose hiding places it
// watched being made) and feelings about the player (a bond from food, a grudge from harm, both learned from the others
// too). From these it picks a task now and then, like the town's people pick what to do next: fetch something it saw,
// hide it, dig up its own store or someone else's, crack a shell by dropping it on the road, steal from another bird, from
// the feeder or from the player's palm, bring the player a present, scold them, follow them, preen its mate, play in the
// wind or with a stick, pull a cat's tail, wait its turn at a dead fish.
//
// The things they carry are items of the world (FItem): acorns, nuts, cones, mussels, crusts, a dead herring, the player's
// thrown feed, bottle caps, foil, rings, buttons, coins, keys, sea glass, sticks. Items lie on the ground, are carried in a
// bill, fall, are hidden in caches, eaten, or taken by the player.
//
// Plain C++ like BirdSim.cpp (tools/birdsim builds it); FWorld owns one FCorvids and calls its hooks.
#pragma once

#include "BirdSim.h"

#include <cstdint>
#include <vector>

namespace LyudBirds
{
	/** The things in the world corvids carry. SM_CorvidItem_<name> in the game (ItemName). */
	enum class EItem : uint8_t
	{
		Acorn, Hazelnut, Walnut, PineCone, Mussel, Crust, Fish, FeedPile,
		BottleCap, Foil, Ring, Button, Coin, Key, SeaGlass, Stick,
		Num
	};
	constexpr int NumItems = (int)EItem::Num;
	const char* ItemName(EItem K);

	/** What an item is to a bird. */
	struct FItemKind
	{
		float Food = 0.f;        // how much a bird's hunger falls when it eats it all (0 = not food)
		bool bHard = false;      // a shell it cracks by dropping it on something hard (walnut, mussel)
		bool bBig = false;       // too heavy to carry far: eaten where it lies, by several birds in turn (the dead fish, thrown feed)
		bool bShiny = false;     // jackdaws, magpies and crows can't leave it alone
		bool bCache = false;     // worth hiding for later
		bool bStick = false;
	};
	const FItemKind& ItemKind(EItem K);

	enum class EItemState : uint8_t
	{
		Ground,      // lying where it is
		Carried,     // in a bird's bill (Holder)
		Falling,     // dropped from the air (a shell to crack, a stick in play)
		Cached,      // hidden in the ground by Owner
		Gone,        // eaten, or forgotten (the slot is reused)
		Taken,       // picked up by the player
	};

	/** Item flags. */
	constexpr uint16_t ItemGift = 1;         // a present a bird left for the player
	constexpr uint16_t ItemPlayer = 2;       // the player threw or dropped it
	constexpr uint16_t ItemCracked = 4;      // the shell is open
	constexpr uint16_t ItemSeen = 8;         // the player watched it being hidden (they may dig it up)
	constexpr uint16_t ItemSeeded = 16;      // put out by the world (acorns under an oak...): goes again when nobody is near
	constexpr uint16_t ItemStolen = 32;      // taken from the player's hand or the feeder

	struct FItem
	{
		int Id = -1;
		EItem Kind = EItem::Acorn;
		EItemState State = EItemState::Gone;
		uint16_t Flags = 0;
		V3 Pos, Vel;
		float Yaw = 0.f;
		int Holder = -1;           // the bird carrying it
		int Owner = -1;            // who hid it, or who brought it as a present
		int Source = -1;           // the site the world put it out at
		float Food = 1.f;          // share left (several birds eat a dead fish or a pile of feed)
		double Since = 0.0;        // when it got into this state
		int Watchers[3] = { -1, -1, -1 };   // birds that saw it being hidden
	};

	/** What a corvid is up to beyond its group's plan. */
	enum class ETask : uint8_t
	{
		None,
		Fetch,       // going for an item it saw
		Harvest,     // pecking out an acorn, a nut, a cone, a mussel where it feeds
		Eat,         // eating what it holds (or a share of a big thing)
		Cache,       // taking what it holds to a hiding place and burying it
		Retrieve,    // digging up its own store
		Pilfer,      // digging up a store it watched another bird make
		Crack,       // dropping a shell on the road from a height until it breaks
		Snatch,      // a grab at the food in the player's hand
		Steal,       // chasing a bird that carries something until it drops it
		Gift,        // bringing the player a present
		Scold,       // calling at the player it holds a grudge against, flying at their head
		Follow,      // keeping near the player who feeds it
		Preen,       // preening its mate
		Play,        // tumbling in the wind over the cliff or a roof, or dropping and catching a stick
		Tease,       // pulling a cat's or a dog's tail
		Feast,       // at a dead fish: waiting its turn round it, darting in
		Num
	};
	const char* TaskName(ETask T);

	/** The crow family's tempers, per species (BirdCorvid.cpp). */
	struct FCorvidKind
	{
		const char* Id = "";
		float Hoard = 0.f;       // urge to hide food in autumn (jays and nutcrackers live by it)
		float Play = 0.f;
		float Steal = 0.f;       // nerve for theft (from birds, the feeder, the hand)
		float Shiny = 0.f;
		float Clever = 0.f;      // watches others hide things and robs them, re-hides its own when watched
		float Social = 0.f;      // company, its mate, learning from the others
		float Gift = 0.f;        // brings presents to someone who feeds it
		float Crack = 0.f;       // drops shells on the road
		float Tease = 0.f;       // pulls tails
		float CacheFar = 60.f;   // m: how far it carries a find to hide it
		uint32_t Harvest = 0;    // items it pecks out where it feeds (bits of EItem)
	};

	/** One corvid's mind (indexed by bird id; only corvids have one). */
	struct FCorvid
	{
		int Kind = -1;               // row of the corvid table, -1 = not a corvid
		ETask Task = ETask::None;
		int Step = 0;
		float TaskTime = 0.f;
		float TaskLen = 0.f;
		int Item = -1;               // the item the task is about
		int Held = -1;               // the item in its bill
		int Other = -1;              // a bird (victim, mate), a beast, a site, depending on the task
		V3 Goal, Anchor;
		int Tries = 0;
		float Cooldown = 0.f;        // s before it thinks of a new task
		// needs, 0..1
		float Hoard = 0.f, Social = 0.f, Play = 0.f;
		// the player
		float Bond = 0.f, Grudge = 0.f;
		double LastGift = -1e9, LastFollow = -1e9, LastScold = -1e9;
		int Mate = -1;
		std::vector<int> Caches;     // items it hid
		std::vector<int> Spied;      // items it watched others hide
	};

	/** Corvid events beyond EEvent (the game's OnCorvid delegate). */
	enum class ECorvidEvent : uint8_t { Pick, Drop, Cache, Dig, Rob, Steal, Snatch, Gift, Scold, Play, Tease, Crack, Eat, Num };
	const char* CorvidEventName(ECorvidEvent E);

	struct FCorvidEvent
	{
		ECorvidEvent Kind;
		int Bird;
		int Item;
		V3 Pos;
	};

	class FCorvids
	{
	public:
		std::vector<FItem> Items;
		std::vector<FCorvid> Minds;
		std::vector<FCorvidEvent> Events;     // this step's (cleared by the caller, like FWorld::Events)
		/** Counters since the start, for the report and the tests. */
		int Count[(int)ECorvidEvent::Num] = {};
		int TaskStarts[(int)ETask::Num] = {};

		/** The corvid table's row for a species (-1 = not a corvid). */
		static int KindOf(const FSpecies& S);
		static const FCorvidKind& Kind(int K);
		bool IsCorvid(int Bird) const { return Bird >= 0 && Bird < (int)Minds.size() && Minds[Bird].Kind >= 0; }
		const FCorvid* Mind(int Bird) const { return IsCorvid(Bird) ? &Minds[Bird] : nullptr; }
		FCorvid* Mind(int Bird) { return IsCorvid(Bird) ? &Minds[Bird] : nullptr; }

		// --- hooks from FWorld ---
		/** Every step after the birds moved: items fall and follow the bills, the world puts things out, minds age. */
		void Step(FWorld& W, float Dt);
		/** A bird thinks; true = the corvid's task handles it now (BirdSim's own acts are skipped this think). */
		bool Think(FWorld& W, FBird& B, float Dt);
		/** A bird landed or settled (FWorld::Settle); true = the task took it over. */
		bool Arrive(FWorld& W, FBird& B);
		/** In the air: where a task flies it (scolding circles, the climb before a drop, play in the wind, a chase for a dropped thing). */
		void FlightGoal(FWorld& W, FBird& B, V3& Goal, float& Speed, bool& bLanding);
		/** It flies off in fright: the task ends, what it carries it may drop. */
		void OnFlush(FWorld& W, FBird& B, const V3& From, int Cause);
		/** The group mustn't send it elsewhere now. */
		bool Busy(int Bird) const;
		/** The player's danger for this bird (a grudge keeps it farther off, a bond lets them closer). */
		float DangerScale(const FBird& B) const;
		/** A teasing magpie isn't put up by the cat it is teasing unless the cat turns on it. */
		float BeastScale(const FBird& B, const FBeast& M) const;

		// --- the game ---
		/** The player threw a handful of feed (or dropped something) at P: an item on the ground. */
		int PlayerDrop(FWorld& W, EItem Kind, const V3& P);
		/** The player picked up item Id (a present, a find, a dug-up store): its owner, if it sees, minds. */
		bool PlayerTake(FWorld& W, int Id);
		/** Items the player can pick up within R of P: lying ones, and stores they watched being made. */
		void PickableNear(const FWorld& W, const V3& P, float R, std::vector<int>& Out) const;
		/** A dead fish washes up on the beach (tests and the game's events; the world does it now and then itself). */
		int WashUp(FWorld& W, const V3& P);
		/** Puts an item on the ground (tests). */
		int Spawn(FWorld& W, EItem Kind, const V3& P, uint16_t Flags = 0);
		/** A stick (or anything) in this corvid's bill for another system's errand (the nests: BirdNest.cpp); -1 if it isn't a corvid. */
		int GiveItem(FWorld& W, FBird& B, EItem Kind);
		/** That errand used the item up (woven into the nest): false if the bird no longer holds it (dropped, stolen). */
		bool UseItem(FWorld& W, FBird& B, int Item);
		/** The world clock jumped: carried things go, tasks end. */
		void Resettle(FWorld& W);

		std::string Report(const FWorld& W) const;

	private:
		float SeedTimer = 0.f;
		float FishTimer = 600.f;
		float SocialTimer = 0.f;
		std::vector<int> FreeItems;
		std::vector<int> Scratch;

		void Sync(FWorld& W);
		int NewItem(FWorld& W, EItem Kind, const V3& P, EItemState State);
		void Remove(FWorld& W, int Item);
		void Emit(ECorvidEvent E, const FBird& B, int Item);
		void Seed(FWorld& W);
		void SocialStep(FWorld& W, float Dt);

		void Choose(FWorld& W, FBird& B, FCorvid& C);
		void Start(FWorld& W, FBird& B, FCorvid& C, ETask T, float Len);
		void End(FWorld& W, FBird& B, FCorvid& C, bool bBackToGroup = true);
		void Pick(FWorld& W, FBird& B, FCorvid& C, int Item);
		void Release(FWorld& W, FBird& B, FCorvid& C, bool bFall);
		void AfterPick(FWorld& W, FBird& B, FCorvid& C);
		void GoTo(FWorld& W, FBird& B, const V3& P, int Site = -1);
		bool Near(const FBird& B, const V3& P, float R) const;
		V3 GroundAt(const FWorld& W, float X, float Y) const;
		bool CacheSpot(FWorld& W, const FBird& B, const FCorvid& C, V3& Out, float MinD) const;
		int PerchNear(const FWorld& W, const V3& P, float MinD, float MaxD, uint32_t& Rng) const;
		int HardGround(const FWorld& W, const V3& P) const;
		bool Watched(FWorld& W, const FBird& B, int* Out) const;
		void HideAt(FWorld& W, FBird& B, FCorvid& C);
		void Learn(FWorld& W, const FBird& From, float Grudge, float Bond);
		bool PlayerSees(FWorld& W, const V3& P, float MaxD) const;
		float Season(const FWorld& W, EItem K) const;
	};
}
