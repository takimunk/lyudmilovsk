// The town's holidays, plain C++ (no Unreal types): which festival is on at a moment of the game's calendar and what part
// of its program is running. Shared by the game (ULyudFestivalSubsystem) and tools/festivalsim (run outside the engine).
// The festivals themselves are data: Content/PeopleData/festivals.json (docs/festivals.md).
//
// A festival happens on days given by a rule (a fixed date, the Nth weekday of a month, days from Orthodox Easter, every
// given weekday, or a trigger the game fires, e.g. the first snow), lasts Days days, and has a program of phases. A phase's
// hours count from 00:00 of the festival's first day and may run past 24 (26.5 = 02:30 the next night); a phase with
// day = -1 repeats on every day of the festival. Decorations are up from DecorFrom to DecorTo (the same hours, DecorTo
// counted from the last day).
#pragma once

#include <map>
#include <string>
#include <vector>

namespace LyudFest
{
	struct FDate
	{
		int Y = 2026, M = 1, D = 1;
		bool operator==(const FDate& O) const { return Y == O.Y && M == O.M && D == O.D; }
		bool operator<(const FDate& O) const { return Y != O.Y ? Y < O.Y : (M != O.M ? M < O.M : D < O.D); }
	};

	/** Days since 1970-01-01 (proleptic Gregorian) and back. */
	int DaysFromCivil(const FDate& Date);
	FDate CivilFromDays(int Days);
	inline FDate AddDays(const FDate& Date, int N) { return CivilFromDays(DaysFromCivil(Date) + N); }
	/** 0 Monday .. 6 Sunday. */
	int Weekday(const FDate& Date);
	int DaysInMonth(int Y, int M);
	/** Orthodox (Julian computus) Easter Sunday as a Gregorian date. */
	FDate OrthodoxEaster(int Y);
	/** Western (Gregorian) Easter Sunday. */
	FDate WesternEaster(int Y);
	/** The Nth (1..5; -1 = last) weekday Wd (0 Monday) of month M; false if there is none. */
	bool NthWeekday(int Y, int M, int N, int Wd, FDate& Out);
	/** The Julian Ephemeris Day of the K-th new moon (Phase 0) or full moon (Phase 0.5) after 2000-01-06 (Meeus ch. 49,
	 *  the main periodic terms: a few minutes). */
	double MoonPhaseJDE(int K, double Phase);
	/** The local date (UTC + TzHours) of the first new (Phase 0) or full (0.5) moon on or after From; within 40 days. */
	FDate MoonOnOrAfter(const FDate& From, double Phase, float TzHours);

	/** One part of a festival's program. */
	struct FPhase
	{
		std::string Id;                 // fair, concert, fireworks, lanterns, bonfire, sunrise, dance, ...
		float From = 10.f, To = 18.f;   // hours from 00:00 of the festival's day (Day), may pass 24
		int Day = 0;                    // which day of the festival (0 first); -1 every day
		std::string Venue;              // a spot kind or id (square, fair, beach, pier, promenade, park, sunrise_point)
		std::string Mode = "mingle";    // how people take part: mingle, watch, sit, fair
		float Pull = 1.f;               // how strongly it draws people (0..2)
		std::string Face;               // what watchers face: stage, sea, bonfire, tree, sky, sunrise ("" none)
		std::vector<std::string> Show;  // what the game shows during it: fireworks, sky_lanterns, bonfire, music, stage_lights
	};

	struct FFestival
	{
		std::string Id;                 // strings: festival.<id>.name / .desc
		std::string Rule = "date";      // date, nth_weekday, easter_orthodox, easter_western, weekly, trigger, new_moon, full_moon
		int Month = 1, DayOfMonth = 1;  // date; nth_weekday: Month
		int Nth = 1, Wd = 5;            // nth_weekday: the Nth weekday Wd (0 Monday) of Month
		int Offset = 0;                 // easter_*: days from Easter Sunday; any rule: shifts the found day
		std::vector<int> Weekdays;      // weekly: which weekdays
		std::vector<int> Months;        // weekly (or any): only in these months (empty = all)
		std::string Trigger;            // trigger: the game's event (first_snow)
		int Window = 30;                // new_moon / full_moon: the first such moon in [Month/DayOfMonth, + Window days)
		float Tz = 2.f;                 // new_moon / full_moon: the hours from UTC its date is reckoned in (8: China)
		std::vector<std::string> Menu;  // the restaurant's dishes for it (item ids, Content/Restaurant/menu.json)
		std::vector<std::string> Foods; // what the fair's stalls sell on its day (item ids)
		float Late = -1.f;              // people stay up till this hour of its first day (26 = 02:00 the next night); -1 not
		std::string Origin;             // where in the world it comes from (a note: festival.<id>.origin)
		int Days = 1;
		bool bHoliday = false;          // a public holiday: offices and schools are closed on its days
		std::vector<int> HolidayDays;   // which of its days are days off (empty = all of them, if bHoliday)
		float DecorFrom = 8.f, DecorTo = 24.f;
		std::vector<std::string> Decor; // bunting, lanterns, stage, stalls, tree, flags, candles, birdhouses, wreaths
		std::vector<std::string> DecorVenues;   // where the decorations go (spot kinds or ids); empty = the phases' venues
		int Priority = 0;               // when two festivals want the same venue at once, the higher one has it
		std::vector<FPhase> Phases;
	};

	/** A festival that is on (its decorations are up) at a moment. */
	struct FActive
	{
		int Festival = -1;
		FDate Start;                    // its first day
		float Rel = 0.f;                // hours since 00:00 of its first day
		int Phase = -1;                 // the running phase (-1: between phases, decorations only)
		float PhaseT = 0.f;             // how far through it, 0..1
		std::vector<int> Phases;        // every phase running now (overlapping ones), Phase first
	};

	class FCalendar
	{
	public:
		std::vector<FFestival> Festivals;
		std::string LastError;

		bool Load(const std::string& JsonText);
		int Find(const std::string& Id) const;

		/** Does festival F start on Date (its first day)? */
		bool StartsOn(int F, const FDate& Date) const;
		/** The festivals on at Date, Hour (0..24): decorations up, the phases running. Highest priority first. */
		std::vector<FActive> At(const FDate& Date, float Hour) const;
		/** Is Date a day off (a holiday festival's day)? The festival's id in OutId. */
		bool IsDayOff(const FDate& Date, std::string* OutId = nullptr) const;
		/** The first start of F on or after From (within a few years); false if none. */
		bool Next(int F, const FDate& From, FDate& Out) const;
		/** Every festival that starts in [From, From + Days), in date order: (date, festival). */
		std::vector<std::pair<FDate, int>> Upcoming(const FDate& From, int Days) const;

		/** A trigger festival happens from Date (the game calls it: the first snow of the winter). */
		void Fire(const std::string& Trigger, const FDate& Date);
		/** Trigger -> the days it fired (kept with the save by the game). */
		std::map<std::string, std::vector<FDate>> Fired;
	};
}
