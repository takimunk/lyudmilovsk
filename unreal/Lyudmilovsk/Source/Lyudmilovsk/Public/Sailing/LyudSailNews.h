// What the yacht school tells the town (docs/yacht_school.md "Town: festivals, paper, radio, talk"): the races of a day for
// the paper's sailing column and the radio's results, and the morning's facts for the radio's sailors' forecast.
//
// The town's code (ULyudTownPaper, ULyudMusic) calls the two free functions below; they answer nothing until the yacht
// school's engine (ULyudSailing) registers its providers with LyudSetSailNewsProviders, so the town builds and runs whether
// or not the school is in the game. Plain structs, no reflection: nothing here is saved or shown to Blueprints.
#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class UObject;

/** One class's race on a day (LyudSail::SchoolNewsOfDay, one item per class sailed). */
struct FLyudSailNewsItem
{
	/** The regatta: sail_opening, sail_closing, optimist_day, cup_lyudmilovsk, wednesday, sunday (content.json races.events ids). */
	FName Event;
	/** optimist, cadet, luch. */
	FName Class;
	/** The first three of the day's series (fewer when fewer finished): townsfolk keys (townsfolk.json "key", shown as
	 *  npc.<key>.name), "hero" for the hero, NAME_None for a sailor slot no townsperson is bound to. */
	TArray<FName> Podium;
	/** The mean wind of the racing, m/s. */
	float WindMs = 0.f;
	/** Boats that started (the largest race of the day in this class). */
	int32 Boats = 0;
	/** Capsizes in this class over the day's races. */
	int32 Capsizes = 0;
	/** A general recall at any start of the day in this class. */
	bool bRecall = false;
	/** The event's day: 0 the first (the cup has days 0 and 1). */
	int32 Day = 0;
	/** The hero's place in the day's series of this class, 0 when the hero did not race. */
	int32 HeroPlace = 0;
	/** No race of this class could be sailed that day (wind, storm, dark). */
	bool bAbandoned = false;
};

/** The morning's facts for the radio's sailors' forecast (LyudSail::SchoolForecastForSailors and the season). */
struct FLyudSailMorning
{
	/** LyudSail::ESeasonState: 0 laid up, 1 fitting out, 2 open, 3 laying up; -1 unknown. */
	int32 Season = -1;
	/** The call of the day: 0 nothing to say, 1 sail, 2 reef (luches reef, the youngest may stay ashore), 3 race, 4 shore
	 *  (a shore lesson: too little or too much wind), 5 storm (the cone), 6 closed (thunder, fog, ice, low water). */
	int32 Call = 0;
	/** A shore or closed day's reason: calm, wind, gust, thunder, fog, dark, ice, low_water (NAME_None: none). */
	FName Reason;
	/** Today's regatta (as FLyudSailNewsItem::Event), NAME_None when none. */
	FName Event;
	/** Its warning signal, "HH:MM" (empty when no regatta). */
	FString EventTime;
	/** The day's sailing wind: mean and gusts, m/s, and where it blows from (compass degrees, 270 = from the west). */
	float WindMs = 0.f;
	float GustMs = 0.f;
	float FromDeg = 0.f;
	/** How the wind goes through the sailing hours: 0 steady, 1 veers (turns clockwise), 2 backs, 3 dies away, 4 freshens. */
	int32 Shift = 0;
	/** The wind may rise past a class's recall limit during the sessions (the coaches may call the boats in early). */
	bool bRecallRisk = false;
	/** The best hour to be out, "HH:MM" (empty: none). */
	FString BestHour;
	/** Low water while the boats go out, "HH:MM", when the trolleys must be rolled far over the flats (empty: no worry). */
	FString LowWater;
};

using FLyudSailNewsProvider = TFunction<void(const UObject* WorldContext, int32 DayNumber, TArray<FLyudSailNewsItem>& Out)>;
using FLyudSailMorningProvider = TFunction<bool(const UObject* WorldContext, int32 DayNumber, FLyudSailMorning& Out)>;

/** The races of a calendar day (DayNumber = days since 1970-01-01 of the game's local date: LyudFest::DaysFromCivil,
 *  LyudSail::SchoolDayNumber), one item per class sailed; empty without the school or on a day without racing. Asked for
 *  yesterday by the morning paper and for today and yesterday by the radio. */
LYUDMILOVSK_API void LyudSailNewsOfDay(const UObject* WorldContext, int32 DayNumber, TArray<FLyudSailNewsItem>& Out);

/** The morning's facts of a calendar day for the sailors' forecast; false (Out untouched) without the school. */
LYUDMILOVSK_API bool LyudSailMorningOf(const UObject* WorldContext, int32 DayNumber, FLyudSailMorning& Out);

/** The yacht school's engine registers its answers once (e.g. in ULyudSailing::Initialize) and clears them with empty
 *  functions in Deinitialize. One provider of each at a time: a second call replaces the first. */
LYUDMILOVSK_API void LyudSetSailNewsProviders(FLyudSailNewsProvider News, FLyudSailMorningProvider Morning);
