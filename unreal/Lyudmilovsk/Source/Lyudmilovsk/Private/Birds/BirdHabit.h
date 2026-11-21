// The signature behaviours of each species (docs/bird_habits.md): the one or two things a species alone does, that a patient
// hero with binoculars sees and writes on the bird book's "Habits" page. A wagtail's dashing run after a fly on the wet sand,
// a swallow dipping to drink, a blackbird tossing leaves, a great tit hammering a seed under its foot, a starling mimicking
// on the wire, a heron's stalk and strike, a herring gull's snatch from a bench.
//
// A habit is a short act on top of the bird life (BirdSim.cpp). Three kinds of source:
//   * dice: a calm bird near the hero, in the right place, hour, season and weather, starts it now and then, within the
//     habit's budget for the day (a few times a day, minutes apart);
//   * mapped: something the bird life already does is the habit (a crow cracking a walnut on the paving, a cormorant holding
//     its wings out, a sparrowhawk's attack along the hedge): it is only recognised and announced;
//   * heard: a voice that goes with a posture (the herring gull's long call).
// Habits that move the bird (a run, a sally from the perch and back, a dip to the water) are flown here, on a path checked
// against walls and the ground, and bring the bird back to where the bird life left it.
//
// Nothing here runs for birds far from the camera: FHabits::Consider leaves at once for tiers 2 and 3, and Step walks the
// handful of running habits only.
//
// Plain C++ like BirdSim.cpp (tools/birdsim builds it); FWorld owns one FHabits and calls its hooks.
#pragma once

#include "BirdSim.h"

#include <cstdint>
#include <string>
#include <vector>

namespace LyudBirds
{
	enum class EHabit : uint8_t
	{
		None,
		// the crow family
		RookParliament, RookProbe, CrowNutDrop, CrowRoofSlide, JackdawAllopreen, JackdawChimney, MagpieHideShiny, MagpieTailPull,
		RavenTumble, RavenBow, JayAcornCache, JayAnting, NutcrackerAnvil, NutcrackerStore, CarrionDunk, CarrionStrut,
		// pigeons and the birds of the air
		PigeonBowCoo, PigeonSunbathe, WoodPigeonDisplay, WoodPigeonDrink, SwiftScreamingParty, SwallowDrink, SwallowWirePreen,
		MartinBankFlutter,
		// gulls and the water's birds
		GullSnatch, GullLongCall, BlackHeadedSquabble, CommonGullPaddle, CormorantWings, CormorantDive, SwanBusk, SwanUpend,
		HeronStalk, HeronSunDelta, AlbatrossSeaRun, AlbatrossShear,
		// the garden's small birds
		TitHammer, TitHang, BlueTitAcrobat, GoldfinchCling, ChaffinchSally, BullfinchBudNip, SparrowDustBath, TreeSparrowPuddle,
		// thrushes and the other singers
		StarlingMimic, StarlingGape, ThrushAnvil, FieldfareGuard, BlackbirdLeafToss, BlackbirdWormTug, RobinBob, RobinPounce,
		WagtailDash, WagtailLeap, WaxwingGulp, WaxwingPass,
		// the birds of prey
		SparrowhawkDash, SparrowhawkPluck, PeregrineStoop, PeregrineBob, SeaEagleSnatch, GoldenEagleDance, BuzzardHang,
		Num
	};
	constexpr int NumHabits = (int)EHabit::Num;
	/** "wagtail_dash": the key of its line (bird.habit.<name>), its sketch and its clip. */
	const char* HabitName(EHabit H);
	EHabit FindHabit(const std::string& Name);

	/** The families the bird book's stamps go by (every habit of every species of a family seen = its stamp). */
	enum class EBirdFamily : uint8_t { Crows, Wings, Water, Garden, Singers, Raptors, Num };
	const char* FamilyName(EBirdFamily F);

	/** How a habit moves its bird. */
	enum class EHabitMech : uint8_t
	{
		Still,       // where it sits or stands
		Pair,        // with a neighbour of its kind: it sidles up, both do it
		Run,         // short runs on the ground with stops
		Sally,       // from its place out to a point and back (a flycatcher's sally, a pounce to the ground, a display flight)
		Swoop,       // on the wing: down to a point (the water, the eaves) and on
		Hang,        // on the wing: held on the wind over one spot
		Dance,       // on the wing: a row of dives and climbs
		Mapped,      // the bird life does it; the habit is only announced
	};

	/** When in the day a habit happens. */
	enum class EHabitHours : uint8_t { Day, Morning, Midday, Evening };

	/** Habit needs (flags). */
	constexpr uint32_t NeedDry = 1;            // no rain now and the ground dry
	constexpr uint32_t NeedWet = 2;            // puddles: it rained not long ago
	constexpr uint32_t NeedLowTide = 4;        // the sea below its mean
	constexpr uint32_t NeedFeeder = 8;         // on a feeder's tray
	constexpr uint32_t NeedNoFeeder = 16;
	constexpr uint32_t NeedWater = 32;         // water within reach (the sea's edge, or a water the game named)
	constexpr uint32_t NeedSun = 64;           // no rain, a warm hour
	constexpr uint32_t NeedFrost = 128;        // below zero
	constexpr uint32_t NeedRival = 256;        // another species feeding at its place
	constexpr uint32_t NeedMorsel = 512;       // food in the open the game named (a snack on a bench, a fisherman's catch)
	constexpr uint32_t NeedFeeding = 1024;     // its group is feeding
	constexpr uint32_t NeedResting = 2048;     // its group is resting
	constexpr uint32_t NeedGathering = 4096;   // its group is at the gathering before the roost
	constexpr uint32_t NeedPlayerNear = 8192;  // it has noticed the player, calmly
	constexpr uint32_t NeedGround = 16384;     // standing on the ground (not on a twig over it)

	/** What starts a mapped habit. */
	enum class EHabitFrom : uint8_t { Dice, Corvid, BirdEvent, Voice, Act };

	struct FHabitDef
	{
		EHabit Id = EHabit::None;
		const char* Name = "";
		const char* Species = "";
		EBirdFamily Family = EBirdFamily::Crows;
		EHabitMech Mech = EHabitMech::Still;
		uint32_t Acts = 0;               // the acts it starts from (bits of EAct)
		uint32_t Habitats = 0;           // where (bits of EHabitat); 0 = anywhere
		uint32_t Needs = 0;
		EHabitHours Hours = EHabitHours::Day;
		float DoyFrom = 0.f, DoyTo = 366.f;   // days of the year (From > To wraps over the new year)
		float WindMin = 0.f, WindMax = 99.f;  // m/s
		float LenMin = 3.f, LenMax = 6.f;     // s
		int Beats = 0;                   // moments in it worth a sound or a puff (a strike, a knock, a toss)
		float Rate = 1.f;                // starts a minute for one bird when everything fits
		int PerDay = 4;                  // at most this many near the hero in a day
		float Gap = 600.f;               // s between two of them
		float Room = 0.f;                // m of free space it needs (ahead and to the sides)
		float Reach = 0.f;               // m: how far it goes (run, sally, swoop)
		float Speed = 0.f;               // m/s on its path, 0 = the species' own
		EAct Base = EAct::Perch;         // the act the bird life sees meanwhile
		EHabitFrom From = EHabitFrom::Dice;
		int FromKind = 0;                // the corvid event, bird event, voice or act that starts it
		int FromArg = -1;                // an item kind (corvid), -1 = any
		const char* Clip = "";           // the body's clip to reuse until the habit has its own (docs/bird_habits.md)
	};
	/** The table (BirdHabit.cpp). */
	const std::vector<FHabitDef>& HabitTable();
	const FHabitDef& HabitDef(EHabit H);
	/** The habits of a species (indices into the table). */
	const std::vector<int>& HabitsOf(int Species);
	EBirdFamily FamilyOf(int Species);

	enum class EHabitEvent : uint8_t { Start, Beat, End };

	struct FHabitEvent
	{
		EHabitEvent Kind;
		EHabit Habit;
		int Bird;
		V3 Pos;
		int Arg = -1;                    // Beat: its number; a snatch's beat: the morsel's id
	};

	/** A habit running on a bird. */
	struct FHabitRun
	{
		int Bird = -1;
		EHabit Habit = EHabit::None;
		float T = 0.f, Len = 1.f;
		int Step = 0;                    // phase of its path
		float StepT = 0.f, StepLen = 0.f;
		int Other = -1;                  // the partner (Pair), the rival (guard), the morsel
		bool bSecond = false;            // the partner's side of a Pair
		bool bMoves = false;             // flown here: the bird life's Move keeps off it
		bool bMapped = false;
		int Beat = 0;
		V3 Home, From, To, Exit;
		float HomeYaw = 0.f;
		EAct Mine = EAct::Perch;         // the act this habit set (another one = something else took the bird: the habit ends)
		EAct Was = EAct::Perch;
		V3 MySpot;
		int MySite = -1;
	};

	/** Food lying in the open that a gull may snatch (the game fills them: a resident's snack on a bench, a fisherman's catch). */
	struct FMorsel
	{
		int Id = -1;
		V3 Pos;
		uint8_t Kind = 0;                // 0 a snack, 1 a fish
	};

	class FHabits
	{
	public:
		std::vector<FHabitRun> Active;
		std::vector<FHabitEvent> Events;       // this step's (FWorld::Step clears them when the next one begins, like FCorvids::Events)
		/** Counters: starts since the beginning, starts near the hero today, and when the last one was. */
		int Count[NumHabits] = {};
		int Today[NumHabits] = {};
		double LastStart[NumHabits];
		/** How many times a bird was looked at for a habit (the cost; 0 while every bird is far). */
		long Considered = 0;
		/** At most this many dice habits run at once near the hero. */
		int MaxActive = 4;
		/** Dice habits start within this many metres of the camera (plus a little for a big bird). */
		float ShowDist = 70.f;
		/** Fresh water the game names (the brook's pools, a fountain, puddles after rain): swallows drink there. */
		std::vector<V3> Waters;
		std::vector<FMorsel> Morsels;
		/** 0 dry .. 1 puddles everywhere (rises in rain, dries in a couple of hours). */
		float Wet = 0.f;

		FHabits();

		// --- hooks from FWorld ---
		void Step(FWorld& W, float Dt);
		/** A bird in a habit thinks: true = the habit holds it (the bird life's own acts are skipped this think). */
		bool Busy(FWorld& W, FBird& B, float Dt);
		/** A free bird thinks: true = it started a habit. */
		bool Consider(FWorld& W, FBird& B, float Dt);
		/** The habit flies this bird itself (FWorld::Step leaves Move out). */
		bool Moves(const FBird& B) const { return B.Habit != 0 && B.bHabitMoves; }
		/** The clock jumped: every habit ends where it is. */
		void Resettle(FWorld& W);

		// --- the game and the tests ---
		/** The habit running on a bird (null = none). */
		const FHabitRun* Of(int Bird) const;
		/** Starts a habit on a bird now if it can be done where the bird is (the place, the room, a partner): developer mode
		 *  and the tests. bFree skips the hour, season, weather and budget. */
		bool Start(FWorld& W, FBird& B, EHabit H, bool bFree = true);
		/** Would this habit start on this bird now (everything but the dice)? */
		bool Fits(FWorld& W, const FBird& B, const FHabitDef& D, bool bFree = false) const;
		std::string Report(const FWorld& W) const;

	private:
		int Day = -1;
		float LastHour = -1.f;
		uint32_t Rng = 0x9E3779B9u;      // the habits' own dice: the bird life's streams stay as they were
		size_t Processed = 0;            // FWorld::Events read so far
		void Emit(EHabitEvent K, EHabit H, const FBird& B, int Arg = -1);
		bool Begin(FWorld& W, FBird& B, const FHabitDef& D, bool bMapped, float Len);
		void End(FWorld& W, FHabitRun& R, bool bAbort);
		void Advance(FWorld& W, FHabitRun& R, float Dt);
		bool Plan(FWorld& W, FBird& B, const FHabitDef& D, FHabitRun& R) const;
		bool Clear(const FWorld& W, const V3& A, const V3& B) const;
		bool RoomAt(const FWorld& W, const V3& P, float Yaw, float Room, float* OutYaw) const;
		bool GroundAt(const FWorld& W, const V3& Near, float X, float Y, float& Z) const;
		int Partner(const FWorld& W, const FBird& B, float R, bool bSame) const;
		bool WaterNear(const FWorld& W, const V3& P, float R, uint32_t& Rng, V3& Out) const;
		void MapEvents(FWorld& W);
		void MapActs(FWorld& W, FBird& B);
		int Find(int Bird) const;
	};
}
