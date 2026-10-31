// The birds' breeding year on top of the bird life (docs/bird_nests.md): every species that breeds on this coast pairs up in
// spring, holds a territory or joins its colony, builds its own kind of nest (a rook's stick platform in the rookery, a
// magpie's dome, a chaffinch's lichen cup, a tit's moss pad in a nest box, a swallow's mud cup under the eaves, a sand
// martin's burrow in the clay cliff, a gull's scrape, a swan's reed mound), lays, sits, feeds its chicks and leads the
// fledglings about until they are on their own; some species have a second or third brood, some lose a clutch and lay
// again. The season follows the game calendar: ravens and herons lay in March, most songbirds in late April and May,
// swifts at the end of May, feral pigeons whenever they like.
//
// The timeline of every nest is a pure function of the nest, the year and the day (FNest::Plan): the same date always gives
// the same stage, whatever happened before, so clock jumps, saves and the LOD cost nothing. On top of it the parents of the
// nests near the camera live it: song from a post at dawn, trips for material and back, sitting tight on the eggs, the
// sitter fed by its mate, feeding trips to begging chicks, alarm calls and dives at a player who comes too close, and the
// fledglings (birds of the bird life, from a pool) perched near the nest begging, then following their parents.
//
// Plain C++ like BirdSim.cpp (tools/birdsim builds it); FWorld owns one FNesting and calls its hooks, as it does FCorvids'.
// Nothing here runs every frame for every nest: the stages are worked out once a game minute, behaviour only in the
// thinks of the birds that have a nest.
#pragma once

#include "BirdSim.h"

#include <cstdint>
#include <string>
#include <vector>

namespace LyudBirds
{
	/** The kinds of nest; the game draws SM_Nest_<Species>_<stage> (the species' own) and falls back to the kind's. */
	enum class ENest : uint8_t
	{
		None,
		Cup,         // a woven cup in a fork or a bush (finches, thrushes, the jay)
		Dome,        // a roofed ball of sticks (the magpie)
		Platform,    // a stick platform (crows, rooks, pigeons, herons, raptors, cormorants)
		Cavity,      // a hole in a tree or a nest box (tits, starlings, sparrows, jackdaws): only the hole or the box shows
		Eave,        // a hole under the eaves or in a wall with the bedding sticking out (sparrows, wagtails, swifts)
		MudCup,      // a mud cup on a wall under the eaves (barn swallow)
		Burrow,      // a tunnel in the clay cliff (sand martin)
		Scrape,      // a hollow on the ground lined with a little grass (gulls, the peregrine on a ledge)
		Mound,       // a heap of reed at the water (swan, black-headed gulls in the reed)
		Num
	};
	const char* NestName(ENest K);

	/** What a nest is built of (the thing a builder carries in its bill). */
	enum class EMaterial : uint8_t { None, Sticks, Twigs, Grass, Moss, Mud, Reed, Feathers, Num };
	const char* MaterialName(EMaterial M);

	/** Who does a job: the female, the male, or both in turn. */
	enum class EWho : uint8_t { Female, Male, Both };

	/** One species' breeding: what it builds where, when, and how long each stage takes (BirdNest.cpp, from BWP/Cramp
	 *  and the Kaliningrad/Baltic atlases; days of the year, a non-leap year). */
	struct FBreedKind
	{
		const char* Id = "";
		ENest Nest = ENest::None;
		EMaterial Material = EMaterial::Grass;
		const char* Where = "";        // habitat tastes for the nest site ("Broadleaf 1, Conifer .5")
		float HeightMin = 0.f, HeightMax = 0.f;   // m over the ground in a tree (crowns only)
		float NestRadius = 0.06f;      // m, outer radius of the finished nest
		bool bColonial = false;        // nests together (rookery, heronry, gullery, a martins' cliff)
		float Spacing = 30.f;          // m between neighbouring nests (a territory); within a colony
		float ColonyRadius = 0.f;      // m: a colony's nests lie within this of its centre
		int ColonySize = 1;            // pairs in one colony (more pairs make more colonies)
		int PerSite = 1;               // nests of the species on one site (a rookery tree holds several)
		int LayFrom = 120, LayUntil = 140;   // first clutches are started between these days
		int LayLast = 180;             // no clutch (repeat or second brood) is started after it
		int BuildDays = 5;             // building a new nest (a reused one: a third of it)
		int ClutchMin = 4, ClutchMax = 5;
		float LayInterval = 1.f;       // days between eggs
		int IncubDays = 13;
		int NestlingDays = 14;         // in the nest after hatching (precocial: hours, they leave with the parents)
		int DependDays = 14;           // fed by the parents after leaving the nest
		int Broods = 1;
		EWho Builds = EWho::Both, Sits = EWho::Female;
		bool bPrecocial = false;       // the chicks run or swim after the parents (swan, gulls walk off the scrape)
		bool bReuse = false;           // the nest stands for years and is used again (sticks, mud, burrows, boxes)
		float Survival = 0.97f;        // a nest's chance per day to come through (predators, weather)
		float FeedMinutes = 3.f;       // minutes between one parent's feeding visits to the chicks
		float MaterialRange = 40.f;    // m it goes for material
		float Defend = 0.f;            // 0 slips away, 0.5 alarm calls round the intruder, 1 dives at their head (gulls)
		float SingRate = 0.f;          // songs a minute from the song post in the season (territorial males)
	};
	const std::vector<FBreedKind>& BreedTable();
	/** The table row for a species (null: doesn't breed here: waxwings, the albatross). */
	const FBreedKind* BreedOf(const std::string& SpeciesId);

	/** The stage of a nest on a day. */
	enum class EStage : uint8_t
	{
		None,        // nothing there this year (yet)
		Old,         // last year's nest, empty (or a box nobody uses)
		Build,       // being built (Progress 0..1)
		Eggs,        // laid, sat on (Eggs)
		Chicks,      // in the nest (Chicks, Age days)
		Fledged,     // the young have left; the parents still feed them round about
		Failed,      // robbed or deserted: empty, maybe torn
		Num
	};
	const char* StageName(EStage S);

	/** One breeding attempt in a year (a nest may have two or three: a second brood, a repeat after a loss). */
	struct FAttempt
	{
		float Build = 0.f;          // day building starts
		float Lay = 0.f;            // first egg
		int Clutch = 0;
		float Hatch = 0.f;
		float Fledge = 0.f;
		float Independent = 0.f;
		float Fail = 1e9f;          // day it is lost (1e9 = it isn't)
		int Fledglings = 0;         // young that leave the nest
	};

	/** The state of a nest at one moment, worked out from its plan. */
	struct FNestState
	{
		EStage Stage = EStage::None;
		float Progress = 0.f;       // building, 0..1 (1 also for finished nests of every later stage)
		int Eggs = 0;
		int Chicks = 0;
		float Age = 0.f;            // chicks' days since hatching
		int Attempt = -1;
		bool bPairHere = false;     // the pair belongs at the nest now (building to independence)
	};

	struct FNest
	{
		int Id = -1;
		int Species = -1;
		int Kind = -1;                // row of BreedTable
		int Site = -1;
		V3 Pos;                       // where the cup or the box is (the game snaps it to a branch, a wall, the ground)
		float Yaw = 0.f;
		int Colony = -1;
		int Male = -1, Female = -1;   // the pair (birds); -1 = none (the nest isn't used: an old one stays)
		int FirstYear = 0;            // the year it was built first
		uint32_t Seed = 1;
		bool bPlaced = false;         // the game has put it on real wood or a wall (Pos, Yaw final)
		float DrawScale = 1.f;        // the game's fit (a tree hole to the trunk's girth)
		// the year's plan (cached; PlanYear says for which)
		int PlanYear = -1;
		std::vector<FAttempt> Plan;
		FNestState State;             // now
		float Deserted = -1.f;        // day the player made the pair give it up (-1 none): this attempt fails
		float Disturb = 0.f;          // s the player has been too close lately (decays)
		// the fledglings out of it now (birds from the pool)
		std::vector<int> Young;
	};

	enum class ENestEvent : uint8_t { BuildStart, Material, Lay, Hatch, Feed, Fledge, Fail, Alarm, Dive, Song, Beg, Num };
	const char* NestEventName(ENestEvent E);

	struct FNestEvent
	{
		ENestEvent Kind;
		int Nest;
		int Bird;
		V3 Pos;
	};

	/** What a parent is doing for its nest beyond its group's plan. */
	enum class EDuty : uint8_t { None, Sing, Gather, Carry, Work, Sit, Forage, Bring, Feed, Guard, Dive, Num };
	const char* DutyName(EDuty D);

	class FNesting
	{
	public:
		std::vector<FNest> Nests;
		std::vector<FNestEvent> Events;     // this step's (cleared by Step)
		int Year = 2027;                     // the game's year (the game sets it before Setup and when it changes)
		int Count[(int)ENestEvent::Num] = {};
		int DutyStarts[(int)EDuty::Num] = {};

		/** Places the nests and pairs the birds (FWorld::Populate calls it after the groups are made). */
		void Setup(FWorld& W);
		/** The nest a bird belongs to (-1 none). */
		int NestOf(int Bird) const { return Bird >= 0 && Bird < (int)Role.size() ? Role[Bird].Nest : -1; }
		/** A fledgling of the pool out in the world: the nest it came from (-1 none). */
		int YoungOf(int Bird) const { return Bird >= 0 && Bird < (int)Role.size() ? Role[Bird].YoungOf : -1; }
		/** A downy cygnet or gull chick out of the pool: the game draws it with the chick kit, not the adult's body. */
		bool DownyYoung(int Bird) const;
		/** The young's size against an adult's (the game scales its body); 1 for the rest. */
		float BodyScale(const FWorld& W, int Bird) const;
		/** The state of a nest on any day (pure: no side effects but the cached plan). */
		FNestState StateOn(int Nest, int Year, float Day);
		/** The breeding season changes a species' life: pairs keep near the nest, flocks come apart (0..1 now). */
		float Season(const FWorld& W, int Species) const;

		// --- hooks from FWorld ---
		void Step(FWorld& W, float Dt);
		bool Think(FWorld& W, FBird& B, float Dt);
		bool Arrive(FWorld& W, FBird& B);
		void FlightGoal(FWorld& W, FBird& B, V3& Goal, float& Speed, bool& bLanding);
		void OnFlush(FWorld& W, FBird& B, const V3& From, int Cause);
		bool Busy(int Bird) const;
		/** A sitting bird sits tight: the player can come much nearer than usual before it slips off. */
		float DangerScale(const FBird& B) const;
		/** What a builder carries in its bill now, for the game to draw (None: nothing, or a corvid's stick: the corvid items draw that). */
		EMaterial Carrying(int Bird) const;
		/** A bird's nest duty now (tests, the report). */
		EDuty DutyOf(int Bird) const { return Bird >= 0 && Bird < (int)Role.size() ? Role[Bird].Duty : EDuty::None; }
		/** Fledglings out now / the pool's size. */
		int YoungOut() const { return PoolSize - (int)Pool.size(); }
		static constexpr int PoolSize = 192;
		/** The clock jumped: duties end, fledglings go back to the pool and are made again where they would be. */
		void Resettle(FWorld& W);

		/** Tests and the game: nests within R of P (ids). */
		void Near(const V3& P, float R, std::vector<int>& Out) const;
		std::string Report(const FWorld& W) const;
		/** The breeding table as JSON (the art scripts read it: blender/birds/nests). */
		static std::string TableJson();

	private:
		struct FRole
		{
			int Nest = -1;
			int YoungOf = -1;
			EDuty Duty = EDuty::None;
			float Time = 0.f, Len = 0.f;
			int Step = 0;
			V3 Goal;
			int Carry = -1;            // the corvid item it carries (sticks), -1 none
			EMaterial Mat = EMaterial::None;   // what it carries (drawn by the game unless a corvid item)
			double Next = 0.0;         // W.Time of its next nest trip
			float SongTimer = 0.f;     // s to its next song / alarm call / begging
			int Target = -1;           // the fledgling it is bringing food to
			// a fledgling
			float Hunger = 0.f;
			bool bTied = false;        // keeps to the nest (or its mother) rather than going with the group
			int Index = 0;             // its place in the brood
		};
		std::vector<FRole> Role;
		std::vector<int> Pool;         // fledgling birds not in use (bGone)
		std::vector<V3> ColonyAt;      // per colony: its centre
		std::vector<V3> SavedHome;     // per group: its home outside the season
		std::vector<uint8_t> bHomeMoved;
		float MinuteTimer = 0.f, YoungTimer = 0.f, GuardTimer = 0.f;
		int LastDay = -1;

		const FBreedKind& KindOf(const FNest& N) const;
		void MakePlan(FNest& N, int Year);
		void Update(FWorld& W, FNest& N, float Day, bool bEvents);
		void Emit(ENestEvent E, int Nest, int Bird, const V3& P);
		bool Daytime(const FWorld& W, const FSpecies& S) const;
		void Start(FWorld& W, FBird& B, EDuty D, float Len);
		void End(FWorld& W, FBird& B, bool bBackToGroup = true);
		bool ChooseDuty(FWorld& W, FBird& B, FNest& N);
		int MaterialSite(FWorld& W, const FBird& B, const FNest& N, EMaterial M) const;
		int FoodSite(FWorld& W, const FBird& B, const FNest& N) const;
		V3 NestSeat(const FNest& N, bool bInside) const;
		void Fledge(FWorld& W, FNest& N, int Count);
		void ReleaseYoung(FWorld& W, FNest& N);
		bool YoungThink(FWorld& W, FBird& B, FRole& R, float Dt);
		void Guard(FWorld& W, FNest& N, float Dt);
		int Mate(const FNest& N, int Bird) const { return N.Male == Bird ? N.Female : N.Male; }
		bool Sitter(const FWorld& W, const FNest& N, const FBird& B) const;
		void Carry(FWorld& W, FBird& B, EMaterial M);
		void Drop(FWorld& W, FBird& B);
	};
}
