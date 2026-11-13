// What the townsfolk say on their own (docs/town_talk.md), in plain C++ (no Unreal types) so tools/barksim can run it on
// the town's people outside the engine. Reads Content/Dialogue/town_barks.txt: sets of lines and when each is said (who
// says it, what they are doing and where, the hour, the month, the sky, the holiday) and picks what someone says now.
// Only string keys live here; the words are in Content/Localization/barks/strings_<lang>.json (LyudText).
// ULyudTownTalkSubsystem (Public/People/TownTalk.h) shows the lines over the speakers' heads in the game.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace LyudPeople
{
	class FWorld;
	struct FPerson;
}

namespace LyudBarks
{
	/** How the lines of a set are said. */
	enum class EKind : uint8_t
	{
		Say,        // a line said aloud on their own
		Greet,      // to the player walking past (to = hero)
		Talk,       // an exchange of two: A | B | A ...
		Event,      // at a moment of the game (on = rest_serve, ...)
	};

	/** Numbers in ranges: 5-9, 22-4 (over the end of the day or the year), 10 (that one alone). */
	struct FRanges
	{
		struct FRange { float From = 0.f; float To = 0.f; bool bSingle = false; };
		std::vector<FRange> R;
		bool Empty() const { return R.empty(); }
		/** bHours: the end is left out (17-22 is 17:00 to 21:59) and a single number is its hour (6 = 6:00 to 6:59);
		 *  otherwise both ends count (months 10-4: October to April). From over To goes over the end. */
		bool Has(float V, bool bHours) const;
		/** "5-9 22-4 10"; false on a token that isn't a number or a range. */
		bool Parse(const std::string& Text);
	};

	/** One condition: values any of which will do, and values that must not be ('!x'). "any" matches any non-empty value. */
	struct FValues
	{
		std::vector<std::string> Yes;
		std::vector<std::string> No;
		bool Empty() const { return Yes.empty() && No.empty(); }
		/** One of Have is among Yes (or there is no Yes), and none of Have is among No. */
		bool Match(const std::vector<std::string>& Have) const;
		bool Match(const std::string& Have) const;
	};

	/** What a set asks of one speaker. */
	struct FWhoCond
	{
		FValues Job, Sex, Doing, At, Pose, Gather, Chrono, Need;
		FValues Manner;             // what their hands are doing (manners.json: smoke, phone_call, umbrella, ...)
		FValues AgeGroup;           // kid, teen, adult, elder
		FRanges AgeYears;           // 50-120
		bool bNearHero = false;
	};

	/** Someone about to speak: what the sets can ask of them (the game and tools/barksim fill it from the people's life). */
	struct FWho
	{
		std::vector<std::string> Jobs;      // occupation and their duties' roles (waiter, cook, seller, ...)
		int Age = 30;
		bool bFemale = false;
		std::vector<std::string> Doing;     // the activity's id, and "work" (on a duty at its place) or "free"
		std::vector<std::string> At;        // where: outdoors or indoors; there: the place's kind, its id, its building's id
		std::string Pose;                   // stand, walk, talk, look, sit, computer, eat, read, work, shovel
		std::string Gather;                 // the holiday gathering's mode they are at (mingle, watch, sit, fair), "" none
		std::string Chrono;                 // lark, owl, night, usual
		std::string Manner;                 // their manner now (PeopleManners.h), "" none or not drawn; the caller sets it after FillWho
		std::vector<std::string> Needs;     // the needs over 0.75: hunger, tired, social, fun, nature, mind, sun
		bool bHeroNear = false;             // the player is within 10 m
		/** Empties it, keeping the memory (filled again every scan without allocating). */
		void Reset();
	};

	/** The moment: the game's clock, the sky, the holiday. */
	struct FNow
	{
		float Hour = 12.f;                  // 0 .. 24
		int Month = 10;                     // 1 .. 12
		int Weekday = 0;                    // 0 Monday .. 6 Sunday
		std::string Sun;                    // day, low (sunrise, sunset), night, cloudy
		std::vector<std::string> Weather;   // any of rain, snow, fog, cold, warm, clear, wet
		std::string Festival;               // festivals.json id of the festival on now, "" none
		std::string Phase;                  // its phase now, "" none
	};

	/** A [set] of town_barks.txt. */
	struct FSet
	{
		std::string Name;
		EKind Kind = EKind::Say;
		std::string On;                     // Event: which
		FWhoCond A;                         // the speaker (the first of an exchange)
		FWhoCond B;                         // the second of an exchange (job2, age2, sex2, doing2)
		FRanges Hours, Months;
		FValues Weekday, Sun, Weather, Festival, Phase;
		float Weight = 1.f;
		int Conditions = 0;                 // how many conditions it has: the more particular, the more often it wins
		std::vector<std::string> Lines;     // say: keys without "line."
		std::vector<std::vector<std::string>> Talks;    // talk: the lines in turn, A first
		int FileLine = 0;
	};

	class FBook
	{
	public:
		std::vector<FSet> Sets;
		/** Parse: why nothing was read (""), and what was skipped. */
		std::string LastError;
		std::vector<std::string> Warnings;

		/** Reads the text of town_barks.txt; false if it holds no sets. Unknown keys and bad values are Warnings. */
		bool Parse(const std::string& Text);
		/** Every line's string key ("line." + the key), each once, in the file's order. */
		std::vector<std::string> Keys() const;

		bool MatchWho(const FWhoCond& C, const FWho& W) const;
		bool MatchNow(const FSet& S, const FNow& N) const;

		/** A line for someone to say on their own (Say), to the player walking past (Greet) or at a moment (Event, On): its
		 *  key without "line.", null if nothing fits. Lines said lately are left out while others fit (a greeting or an
		 *  event's line rather repeats than says nothing). OutSet: the set's index. */
		const std::string* PickLine(EKind Kind, const FWho& Who, const FNow& Now, const char* On = nullptr, int* OutSet = nullptr);
		/** An exchange between A and B: its keys in turn, A's first unless bSwapped (B starts it). Null if none fits. */
		const std::vector<std::string>* PickTalk(const FWho& A, const FWho& B, const FNow& Now, bool& bSwapped, int* OutSet = nullptr);

		void Seed(uint32_t S) { Rng = S != 0u ? S : 1u; }
		/** Forgets which lines were said lately. */
		void ClearRecent() { RecentN = 0; RecentAt = 0; }
		/** Remembers a line as said (the game: a line shown some other way). */
		void MarkSaid(const std::string& Key) { Remember(Hash(Key)); }

		static const char* AgeGroup(int Age);       // kid (under 12), teen (12-17), adult (18-59), elder (60 and over)
		static const char* WeekdayName(int Weekday);    // mon .. sun
		/** How often a set is picked against the others that fit: its weight, a half more for every condition. */
		static float Chance(const FSet& S) { return S.Weight * (1.f + 0.5f * (float)S.Conditions); }

	private:
		uint32_t Rng = 0x9E3779B9u;
		float Rand01();
		static constexpr int RecentMax = 64;
		uint32_t Recent[RecentMax] = {};
		int RecentN = 0;
		int RecentAt = 0;
		bool IsRecent(uint32_t H) const;
		void Remember(uint32_t H);
		static uint32_t Hash(const std::string& S);
		/** Picks a set among Cand by Chance (removing it from Cand), -1 when none is left. */
		int TakeWeighted();
		// scratch kept between calls (no allocations once warm)
		std::vector<int> Cand;
		std::vector<uint8_t> CandSwap;
		std::vector<int> Fresh;
		bool ParseLine(FSet& S, const std::string& Key, const std::string& Value, int LineNo);
	};

	// --- from the people's life (the same in the game and in tools/barksim) -----------------------------------------

	/** Whether someone can say anything now: up and about where they can be seen (not asleep, away, riding, in a building's
	 *  shell, between activities). Somebody else's actor drawing them (the restaurant's waiter) is the caller's to allow. */
	bool CanSpeak(const LyudPeople::FWorld& W, const LyudPeople::FPerson& P);
	/** Fills Who from someone of the people's life; bHeroNear is left to the caller. */
	void FillWho(const LyudPeople::FWorld& W, const LyudPeople::FPerson& P, FWho& Who);
	/** Someone's who doesn't depend on the moment (job, age, sex, temper): FillWho's part for a speaker the life doesn't
	 *  place (the restaurant's stand-in waiter): Role is added to their jobs. */
	void FillWhoRole(FWho& Who, const char* Role);

	/** The sun for the sets' sun = : night (under -4 deg), cloudy (up, Cloud over 0.75), low (under 10 deg), day. */
	const char* SunState(float AltitudeDeg, float Cloud);
	/** The weather for the sets' weather = (ULyudWeatherSubsystem's sky): rain and snow (mm/h of water, its snow part),
	 *  fog, cold (under 2 C), warm (over 22 C), clear (few clouds, dry), wet (soaked ground). */
	void WeatherWords(float Cloud, float RainMmH, float Snow, float Fog, float TempC, float Wet, std::vector<std::string>& Out);
}
