// The diary writes itself (docs/diary.md, "Days"): what really happened on each game day, kept as plain facts (the game's
// events by topic, the weather by the hour), and the short entry the hero writes about it in the evening, made from
// templates with slots and variants. Plain C++ (no Unreal types) so tools/daylog runs it outside the engine.
// Data: Content/UI/daylog.json (tools/daylog/build_daylog.py); words: Content/Localization/diary/strings_<lang>.json.
// The game side: ULyudDayLog (Public/UI/LyudDayLog.h).
#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace LyudDayLog
{
	/** An event of the game (ULyudQuests::Notify) that counts for a topic of the day. */
	struct FRule
	{
		std::string Event;
		/** "" or "*": any target; "job_*": by prefix; "~chess": containing; else exactly. */
		std::string Target;
		std::string Topic;
		/** Targets that never count ("job_board": reading the board is not a job). */
		std::vector<std::string> Except;
	};

	/** A topic of the day: what a line of the entry is about. */
	struct FTopic
	{
		std::string Id;
		/** How much it is worth a line (bigger first). */
		int Rank = 10;
		/** How a target becomes a name: item, npc, bird, festival, place, film; "" none (only plain lines). */
		std::string Slot;
		/** The margin doodle it gives: item (its pencil sketch), bird (its drawing), "" none. */
		std::string Doodle;
		/** Counts only a target never met on an earlier day (a place found: the first time only). */
		bool bOnce = false;
		/** Never a line unless nothing better happened (eating). */
		bool bFiller = false;
		/** Variants: diary.day.<id>.one.<i> ({0} a name), .many.<i> ({0} a name, {1} how many), .plain.<i>. */
		int One = 0, Many = 0, Plain = 0;
	};

	struct FData
	{
		std::vector<FRule> Rules;
		std::vector<FTopic> Topics;
		/** Weather codes in the order they win (thunder before rain...) and their variants: diary.day.weather.<code>.<i>. */
		std::vector<std::pair<std::string, int>> Weather;
		int Quiet = 0;      // diary.day.quiet.<i>: a day when nothing much happened
		int Busy = 0;       // diary.day.close.busy.<i>: the last line of a full day
		int Calm = 0;       // diary.day.close.calm.<i>: the last line of an ordinary one (not every day)
		int MaxTopics = 4;  // lines between the weather and the close
		int BusyHits = 7;   // topics' events that make a day full
		int EveningHour = 18;   // today's entry is written from this hour

		bool Load(const std::string& Json, std::string& Error);
		const FTopic* FindTopic(const std::string& Id) const;
		/** The topic an event counts for ("" none). */
		const FTopic* Match(const std::string& Event, const std::string& Target) const;
	};

	/** The weather of one hour, as the game saw it. */
	struct FSky
	{
		std::string Precip;     // "", drizzle, rain, snow, thunder
		float Cloud = 0.f;      // 0..1
		float TempC = 10.f;
		float Fog = 0.f;        // 0..1
		bool bRainbow = false;
		bool bAurora = false;
	};

	struct FHit
	{
		std::string Topic;
		int Count = 0;
		std::vector<std::string> Targets;   // in the order met, each once
	};

	/** What happened on a day. */
	struct FDay
	{
		int Day = 0;        // days since 1970-01-01
		std::vector<FHit> Hits;
		int Hours = 0, RainH = 0, DrizzleH = 0, SnowH = 0, ThunderH = 0, FogH = 0;
		float CloudSum = 0.f, TMin = 99.f, TMax = -99.f;
		bool bRainbow = false, bAurora = false;
		int LastHour = -1;  // the last hour sampled (one sample an hour)

		FHit* Find(const std::string& Topic);
		const FHit* Find(const std::string& Topic) const;
		bool Empty() const { return Hits.empty() && Hours == 0; }
	};

	/** A line of an entry: a key and its args (an arg starting with @ is a key itself, as the journal's). */
	struct FLine
	{
		std::string Key;
		std::vector<std::string> Args;
		/** The plain variant to show when a name's key is missing in the language ("" none). */
		std::string Fallback;
	};

	struct FEntry
	{
		int Day = 0;
		std::string Weather;        // the code its first line is about
		std::vector<FLine> Lines;
		/** The margin doodle: "bird:<Species>", "item:<id>", "weather:<code>". */
		std::string Doodle;
	};

	/** CamelCase -> snake_case (a bird species to its words' key, as ULyudBirdBook::Snake). */
	std::string Snake(const std::string& Camel);
	/** A name key for a target of a slot: "@item.<id>.name"... ("" none). */
	std::string NameArg(const std::string& Slot, const std::string& Target);
	/** The weather code of a day (the first of FData::Weather that the day's hours make true). */
	std::string WeatherOf(const FDay& D);

	class FModel
	{
	public:
		FData Data;
		/** The days with anything in them, oldest first. */
		std::vector<FDay> Days;

		/** An event of the game on a day (true if it counted for a topic). */
		bool Note(int Day, const std::string& Event, const std::string& Target, int N = 1);
		/** The sky at an hour of a day (the first sample of an hour counts). */
		void Sample(int Day, int Hour, const FSky& Sky);

		/**
		 * The entries of every day up to Today (including Today only when Hour >= EveningHour), oldest first. Each day's entry
		 * depends on the day before (no sentence two days running) and on what was met on earlier days (firsts), so they are
		 * written in order every time: the same facts always give the same words.
		 */
		std::vector<FEntry> Write(int Today, int Hour) const;

		void Save(std::map<std::string, std::string>& Out) const;
		void LoadState(const std::map<std::string, std::string>& In);
		void Reset() { Days.clear(); }

	private:
		FDay& DayOf(int Day);
		FEntry Compose(const FDay& D, const FEntry* Before, std::set<std::string>& Ever) const;
	};
}
