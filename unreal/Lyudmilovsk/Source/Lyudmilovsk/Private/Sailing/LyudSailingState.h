// What ULyudSailing (Public/Sailing/LyudSailing.h) keeps between ticks, in plain C++: the school's data and rules
// (SailSchoolModel.h), the fleet's brain (SailFleetModel.h), the hero's log and the engine's own little memories.
// Shared by LyudSailing.cpp, LyudSailingSite.cpp, LyudSailingFleet.cpp, LyudSailingRace.cpp and LyudSailingDev.cpp.
// docs/yacht_school.md "Engine: the school".
#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "People/PeopleLife.h"

#include "SailFleetModel.h"

#include <map>
#include <string>
#include <vector>

class ALyudSea;

namespace LyudSailingImpl
{
	inline FString Str(const std::string& S) { return UTF8_TO_TCHAR(S.c_str()); }
	inline std::string Std(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }
	inline FName Name(const std::string& S) { return FName(UTF8_TO_TCHAR(S.c_str())); }
	inline FVector2D V2(const LyudSail::FSchoolV2& V) { return FVector2D(V.X, V.Y); }
	inline LyudSail::FSchoolV2 MetresOf(const FVector& Cm) { return LyudSail::FSchoolV2(Cm.X * 0.01, Cm.Y * 0.01); }
	inline FVector CmOf(const LyudSail::FSchoolV2& V, double ZCm) { return FVector(V.X * 100.0, V.Y * 100.0, ZCm); }
	inline FVector CmOf(const LyudSail::FSchoolPoint& Pt) { return FVector(Pt.X * 100.0, Pt.Y * 100.0, Pt.Z * 100.0); }
}

/** One hull of the school's fleet (a row of FSchoolParams::BoatSlots that is not the launch) and its actor's place. */
struct FLyudSailHull
{
	int32 Slot = -1;            // index into FSchoolParams::BoatSlots
	int32 BoatClass = 2;
	int32 Unit = -1;            // the brain's unit sailing her now (-1: nobody)
	int32 Shown = -1;           // how she stands ashore: a packed key of place, rig and cover (-1: not placed yet)
	bool bAfloat = false;       // on the water (drawn and stepped by the fleet's tick)
	float FarDt = 0.f;          // a far boat's time not yet stepped
};

/** The regatta of the day as the engine runs it on the water. */
struct FLyudSailRaceRun
{
	int32 Event = -1;           // index into FSchoolParams::Events
	int32 Wanted = 0;           // races planned today
	int32 Sailed = 0;           // races closed today (in front of the player)
	int32 Stage = 0;            // 0 none, 1 fleet going out / sequence / racing, 2 between races, 3 the day's racing is over
	double BetweenS = 0.0;      // seconds left before the next warning signal
	bool bHeroIn = false;       // the hero entered today's regatta
	bool bPrizes = false;       // the fanfare of the prize-giving has sounded
	bool bGathering = false;    // the town was asked to watch
	bool bRecalled = false;     // a general recall at any start today
	int32 Capsizes = 0;
	double WindSum = 0.0;
	int32 HeroPlace = 0;
};

/** The hero's own business with the school. */
struct FLyudSailHero
{
	bool bBoatOut = false;      // his boat is afloat (he is aboard, or she waits for him at the slip, or he left her somewhere)
	bool bAboard = false;
	bool bHired = false;        // this outing is a hire (else a lesson or a race)
	bool bLeftElsewhere = false;    // he stepped out away from the slip: the launch brings her home, the bosun has a word
	bool bNightLeave = false;   // the head coach's leave to sail after dusk
	bool bNightOut = false;     // this outing began after dusk
	bool bNightSaid = false;
	bool bLesson = false;       // a lesson is running (Lesson)
	bool bRecallSaid = false, bAreaWarned = false, bTowAsked = false, bTowed = false, bStartSaid = false, bOcsSaid = false;
	double RecallS = 0.0;       // seconds he has ignored the recall
	double OutsideS = 0.0;      // seconds outside every sailing area
	double NightS = 0.0;        // seconds afloat past the corridor on a night outing
	double AfloatS = 0.0, Metres = 0.0;
	int32 Capsizes = 0;
	double LessonRaceT = -1e9;  // the lesson's own start sequence: seconds from its gun
	bool bLessonCourse = false;
	int32 LessonObjectiveShown = -1;
	double HudIn = 0.0;
	LyudSail::FLessonRun Lesson;
	LyudSail::FRaceCourse LessonCourse;
	bool bHasTarget = false;    // the objective's buoy or landing place
	LyudSail::FSchoolV2 Target;
	bool bPrompt = false;       // the boarding prompt is up
	TArray<FText> HudLines;     // kept between HUD refreshes (nothing allocated per refresh once it has grown)
	// a loaded game: the boat to put back
	bool bRestore = false, bRestoreAboard = false;
	LyudSail::FSchoolV2 RestoreAt;
	double RestoreYaw = 0.0;
};

struct FLyudSailingState
{
	TWeakObjectPtr<ALyudSea> Sea;
	TFuture<void> PolarJob;

	// ---- the data and the rules
	LyudSail::FSchoolParams P;
	LyudSail::FSchoolLog Journal;           // the hero's log (saved)
	LyudSail::FFleetBrain Brain;

	// ---- the facts of the moment (RefreshFacts, every 2 s)
	LyudSail::FSchoolDate Date;
	long long Day = -1;
	double Hour = 12.0;
	bool bDayOff = false;
	bool bFacts = false;
	LyudSail::ESeasonState Season = LyudSail::ESeasonState::LaidUp;
	LyudSail::FSchoolWeather W;             // over the water of the session's area
	LyudSail::FSchoolV2 WindTo = LyudSail::FSchoolV2(-1.0, 0.0);
	double WindFromDeg = 270.0;
	LyudSail::FSchoolNow Now;
	LyudSail::FSchoolLaunch Lp;
	LyudSail::FSchoolV2 LpShown = LyudSail::FSchoolV2(-1e9, 0.0);
	std::vector<LyudSail::FSchoolSessionState> States;     // today's sessions as decided (saved)
	std::map<long long, LyudSail::FSchoolGo> Decisions;    // the decisions themselves (for SchoolMustRecall)
	long long ResolvedDay = -1;             // the last race day closed with SchoolResolveDay (saved)
	long long HeroInDay = -1;               // a loaded game: the day the hero had entered the regatta of
	std::string SignalShown = "?";          // the mast's flags as hoisted, joined
	std::string SignalState;
	int32 ForcedSeason = -1;                // lyud.Sail.Season: -1 the calendar, 0 laid up, 1 open

	// ---- the fleet
	std::vector<FLyudSailHull> Hulls;       // as ULyudSailing::Dinghies
	int32 HeroHull = -1;
	std::vector<int32> UnitHull;            // the brain's units -> Hulls (unit 0 is the hero's)
	std::vector<std::string> UnitCrew;      // the brain's units -> the crew's slot (a cadet's), empty: single-handed
	std::map<std::string, double> Learned;  // a member's skill learned in sessions, by slot (saved)
	long long WaterKey = 0;                 // the session whose boats are on the water (0: none)
	bool bWaterRace = false;
	bool bWaterWatched = false;             // the session has been sailed in front of the player (else it is resolved by numbers)
	bool bHomeCalled = false;               // the boats of WaterKey were sent home
	int32 LaunchNext = 1;                   // the next unit to push off
	double LaunchIn = 0.0;
	std::string DrillSet;                   // the drill the brain was given
	long long StatDone = 0;                 // the last session resolved by numbers
	int32 DevFleet = -1;                    // lyud.Sail.Fleet: at most this many boats go out (-1: all)
	bool bLaunchOut = false;                // the coach's launch is off its mooring
	double LaunchIdleS = 0.0;
	uint32 MarksOut = 0;                    // a bit per mark of the pool on the water (0-15 the session's, 16-23 the hero's lesson's)
	FName LaunchFlags[2];                   // the flags up on the launch's staff (the class flag, then P)
	double LodIn = 0.0, ParkIn = 0.0, SailorsIn = 0.0;
	double WaterS = 0.0;                    // seconds since the session's boats began to go out
	LyudSail::FSchoolV2 LpBrain = LyudSail::FSchoolV2(-1e9, 0.0);     // the launch point the brain was given
	TArray<FLyudSailorNow> Sailors;         // the people's word on who is where (refreshed every 2 s while boats are out)

	FLyudSailRaceRun Race;
	FLyudSailHero Hero;

	// ---- news of the days sailed in front of the player (the provider adds what the log does not hold)
	long long NewsDay = -1;
	int32 NewsCapsizes[LyudSail::SchoolBoats] = { 0, 0, 0 };
	int32 NewsBoats[LyudSail::SchoolBoats] = { 0, 0, 0 };
	bool bNewsRecall = false;
};
