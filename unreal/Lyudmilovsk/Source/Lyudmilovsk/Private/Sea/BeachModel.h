#pragma once

#include <cstdint>
#include <functional>
#include <vector>

/**
 * The beach at play (docs/beach.md): skipping stones, a sandcastle the tide takes, the swim to the buoy and back, shells and
 * amber on the waterline after a storm. Plain C++, no engine: ULyudBeach (Sea/LyudBeach) feeds it the real sea (ALyudSea: the
 * level, the wet line, the waves) and draws what it says; tools/beachsim/run.sh checks it.
 *
 * Units: metres, seconds, metres per second. Heights are over any one datum (the caller's), the same for sea and sand.
 */
namespace LyudBeach
{
	uint32_t Hash(uint32_t A, uint32_t B = 0x9e3779b9u, uint32_t C = 0x85ebca6bu);
	/** 0 .. 1 from a hash. */
	double Unit(uint32_t H);

	// ------------------------------------------------------------------ stones

	/** A stone counts as flat (worth throwing) from this flatness. */
	constexpr double FlatFrom = 0.6;
	/** The town's record on the lifeguards' board. */
	constexpr int TownRecord = 11;
	/** How flat the stone found at a spot is, 0 a pebble .. 1 a perfect disc: most are not (about three in ten are flat).
	 *  The same spot gives the same stones on a day: Try counts the stones already looked at there. */
	double StoneFlatness(int CellX, int CellY, int Day, int Try);
	inline bool IsFlat(double Flatness) { return Flatness >= FlatFrom; }

	/** The arm's swing while the throw key is held: 0 back .. 1 forward and back again, a triangle wave. */
	constexpr double SwingSeconds = 0.9;
	double SwingAt(double HeldSeconds);
	/** The swing where letting go is best (the arm almost forward), and how wide "good" is each side of it. */
	constexpr double BestSwing = 0.8;
	constexpr double SwingWidth = 0.35;
	/** 0 a fumble .. 1 a clean release. */
	double ReleaseQuality(double Swing);

	struct FThrowIn
	{
		double Quality = 1.0;
		double Flatness = 1.0;
		/** The sea's wave height where the stone lands, m. */
		double WaveM = 0.1;
		/** Depth of the water in front of the hero, m: a stone thrown onto wet sand does not skip. */
		double DepthM = 1.0;
		uint32_t Seed = 0;
	};

	struct FHop
	{
		/** Where it touches the water, m from the hand along the throw, and when, s after the release. */
		double X = 0.0;
		double T = 0.0;
		/** How high it flies before this touch, m. */
		double Height = 0.0;
		/** Its speed when it leaves the water again, m/s (0 at the last touch: it sinks). */
		double SpeedOut = 0.0;
	};

	struct FThrowOut
	{
		/** The touches after which it flew on: the count the children shout. */
		int Skips = 0;
		/** Every touch, the last one is where it sinks. */
		std::vector<FHop> Hops;
		double Distance = 0.0;
		double Seconds = 0.0;
	};

	/** The whole flight at once (the engine plays it back along the real surface). */
	FThrowOut Throw(const FThrowIn& In);
	/** The height of the stone over the water T seconds after the release, m (0 on a touch; negative never). XOut: m along. */
	double StoneAt(const FThrowOut& Flight, double T, double& XOut);

	// ------------------------------------------------------------------ the sandcastle

	/** Work 0 .. 6: a heap (1), a keep (2), towers (3), walls (4), a moat (5), a flag (6). */
	constexpr int CastleStages = 6;
	constexpr double StageSeconds = 4.0;
	/** Sand holds its shape only where it is damp: from a hand over the water up to this far over the hour's wet line. */
	constexpr double DampUnderM = 0.05;
	constexpr double DampOverM = 0.45;

	struct FCastle
	{
		bool bAlive = false;
		double X = 0.0, Y = 0.0, Z = 0.0;
		double Work = 0.0;
		/** 0 fresh .. 1 washed flat. */
		double Worn = 0.0;
		/** The game's day it was begun on (for the diary and the stamp "stood a night"). */
		double BegunDay = 0.0;

		int Stage() const { return Work >= CastleStages ? CastleStages : static_cast<int>(Work); }
		bool HasMoat() const { return Work >= 5.0; }
		bool HasFlag() const { return Work >= 6.0; }
		/** What is left to see: 0 nothing .. 1 whole. */
		double Standing() const { return bAlive ? 1.0 - Worn : 0.0; }
	};

	/** Can a castle be built on sand at GroundZ now. */
	bool CanBuild(double GroundZ, double SeaZ, double WetLineZ);
	/** Holding E for Dt: true when a stage was just finished. */
	bool Build(FCastle& Castle, double Dt);
	/** Children who join in (the town's kids at play on the sand near him) speed it up, each by HelperBoost, at most MaxHelpers. */
	constexpr int MaxHelpers = 2;
	constexpr double HelperBoost = 0.5;
	bool Build(FCastle& Castle, double Dt, int Helpers);
	/** The sea at SeaZ with waves WaveM (the swash runs up some 40% of the wave height) for Dt: the castle wears while the
	 *  water reaches its foot; a moat takes the first run-ups (it must fill first). Rain wears it slowly too.
	 *  True when it was just washed away. */
	bool Wash(FCastle& Castle, double SeaZ, double WaveM, bool bRain, double Dt);
	/** Hours until the water first reaches the foot of a castle at Z, by the level LevelAt(hours from now); -1 when it does
	 *  not within Within hours. */
	double HoursUntilWash(double Z, double WaveM, const std::function<double(double)>& LevelAt, double Within = 14.0);

	// ------------------------------------------------------------------ the buoy swim

	enum class ERace : uint8_t { Idle, Ready, Out, Back, Done, Failed };
	enum class ERaceEvent : uint8_t { None, Started, Turned, Finished, GaveUp };

	/** The buoy is touched within this far, m. */
	constexpr double BuoyTouchM = 3.5;
	/** The lifeguard forgets a swimmer who stays ashore this long after the whistle, s. */
	constexpr double ReadySeconds = 90.0;
	/** A race that takes longer is called off, s per metre of the course. */
	constexpr double SlowestPerM = 3.0;

	struct FRace
	{
		ERace State = ERace::Idle;
		double Seconds = 0.0;
		double ReadyFor = 0.0;
		double OutSeconds = 0.0;
		/** One way, m (set at the whistle). */
		double CourseM = 60.0;
	};

	struct FRaceIn
	{
		/** In the water deep enough to swim. */
		bool bSwimming = false;
		/** Ashore: on his feet on the sand. */
		bool bAshore = true;
		double ToBuoyM = 1000.0;
	};

	/** The lifeguard's whistle: on your marks. */
	void RaceReady(FRace& Race, double CourseM);
	ERaceEvent RaceStep(FRace& Race, const FRaceIn& In, double Dt);
	/** 3 gold, 2 silver, 1 bronze, 0 just a swim: by the mean speed over the course there and back. */
	int Medal(double Seconds, double CourseM);
	/** The lifeguards' own time for a course, s (the record on the board). */
	double LifeguardSeconds(double CourseM);

	// ------------------------------------------------------------------ summer visitors at the buoy

	/** The summer visitors (дачники) who swim the buoy course with the hero in the lifeguards' season: a pace at the start,
	 *  how much of it they lose by the end (a fast starter fades), and a cap's colour (0 .. 5). Words: beach.rival.<Id>. */
	struct FRival
	{
		const char* Id;
		double Speed;
		double Fade;
	};
	constexpr int RivalCount = 6;
	constexpr int MaxRivals = 3;
	const FRival& Rival(int Index);
	/** Visitors swim from mid-June to the end of August, in water of 16 C and warmer. */
	bool RivalSeason(int Month, int Day, double SeaC);
	/** Who swims with him on a day (indices into the rivals, up to MaxRivals; the same all day), the count. */
	int TodaysRivals(int GameDay, int Month, int Day, double SeaC, int* Out);
	/** How far round the course (there and back, 0 .. 2 x CourseM) a visitor has swum T seconds after the whistle. Their pace
	 *  on the day varies a little (GameDay), and falls off linearly to (1 - Fade) by the end. */
	double RivalAlong(int Index, double CourseM, double T, int GameDay);
	/** The visitor's time over the whole course, s. */
	double RivalSeconds(int Index, double CourseM, int GameDay);
	/** The hero's place among the visitors (1 = first): one more than those who were in before him. */
	int RacePlace(double HeroSeconds, const double* Rivals, int Count);

	// ------------------------------------------------------------------ shells and amber

	enum class EFind : uint8_t { Cockle, Mussel, Tellin, Whelk, Amber, AmberFly, Num };
	/** "shell_cockle", "shell_mussel", "shell_tellin", "shell_whelk", "amber", "amber_fly". */
	const char* FindId(EFind Find);
	inline bool IsAmber(EFind Find) { return Find == EFind::Amber || Find == EFind::AmberFly; }

	/** The waterline is cut into cells this long, m; a cell holds at most one find a day. */
	constexpr double FindCellM = 8.0;

	/** 0 a calm day .. 1 the morning after a real storm: from the strongest wind of the last day and a half, m/s
	 *  (nothing under a fresh breeze of 9 m/s, a full storm from 17 m/s). */
	double StormScore(const double* WindMs, int Count);

	struct FFind
	{
		/** The cell along the shore and where in it, m along the shore. */
		int Cell = 0;
		double Along = 0.0;
		/** 0 at the water .. 1 at the wet line: where across the damp strip it lies. */
		double Across = 0.5;
		EFind Kind = EFind::Cockle;
	};

	/** What lies in a cell of the waterline on a day; false when nothing. The same answer all day. */
	bool FindIn(int Cell, int Day, double Storm, FFind& Out);
	/** The cells from CellFrom to CellTo (inclusive), appended to Out. */
	void Finds(int CellFrom, int CellTo, int Day, double Storm, std::vector<FFind>& Out);

	// ------------------------------------------------------------------ selling finds

	/** The pocket item a find becomes: "seashell", "amber_piece", or "amber_fly" for amber with a fly in it. */
	const char* FindItem(EFind Find);
	/** What the souvenir units on the promenade pay for one of a find's items, roubles (a fly in amber is a museum piece);
	 *  0 for anything else. */
	int SellPrice(const char* Item);
	/** How many pieces of amber the seller lets him keep before the quest "The amber shore" is done (she wants to see three). */
	constexpr int KeepForQuest = 3;
	/** The souvenir seller buys between these hours. */
	constexpr double ShopFrom = 10.0, ShopTo = 20.0;

	// ------------------------------------------------------------------ the book

	/** What the hero has done on the beach (ULyudBeachBook keeps it in the save). */
	struct FBook
	{
		int BestSkips = 0;
		int Throws = 0;
		double BestSwim = 0.0;
		int Swims = 0;
		int CastlesBuilt = 0;
		int CastlesWashed = 0;
		int Shells = 0;
		int Ambers = 0;
		/** Bit per EFind found at least once. */
		uint32_t FoundBits = 0;
	};

	struct FNews
	{
		bool bBest = false;
		bool bTownRecord = false;
		bool bFirstOfKind = false;
	};

	FNews NoteThrow(FBook& Book, int Skips);
	FNews NoteSwim(FBook& Book, double Seconds);
	FNews NoteFind(FBook& Book, EFind Find);
}
