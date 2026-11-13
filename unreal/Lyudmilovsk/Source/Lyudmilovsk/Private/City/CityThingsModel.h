// The things to do inside the town's buildings (docs/city_interiors.md), in plain C++ (no Unreal types) so the same
// rules run in the game (ULyudCityThings) and in the cloud (tools/citysim/run.sh).
//
// Data: Content/City/interiors.json (tools/city/interiors.py): things[] (a chair, a shelf, a counter... of a plot, in
// Unreal cm), acts{} (what E does there), shops{} (stock tables). A thing without an act gets its kind's default act.
// What this file decides: which thing the hero looks at (a grid by position, never a pass over all things), whether its
// act may run now (open hours and days, money, an item, staff on shift, the daily limit), and what running it yields
// (minutes, price, items, a line of text, a quest event, a pose, a sound, a shop, a stamp of a collection). What it remembers
// (the page each thing is on, the counts per day, the uses, the plots visited today, the stamps collected) goes into the
// save as a flat string map.
#pragma once

#include "../People/MiniJson.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace LyudCity
{
	struct FVec
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
	};

	/** The weather as the lines ask it (bits of FWhen::Weather; "weather": "rain" in the data). */
	enum EWeatherBit : uint8_t
	{
		WeatherRain = 1,
		WeatherSnow = 2,
		WeatherFog = 4,
		WeatherCold = 8,
		WeatherWarm = 16,
		WeatherClear = 32,
		WeatherWet = 64,
		WeatherDry = 128,
	};

	/** A weather word of the data as its bit; 0 for an unknown word. */
	inline uint8_t WeatherBit(const std::string& Word)
	{
		static const char* const Words[8] = { "rain", "snow", "fog", "cold", "warm", "clear", "wet", "dry" };
		for (int I = 0; I < 8; ++I)
		{
			if (Word == Words[I])
			{
				return (uint8_t)(1u << I);
			}
		}
		return 0;
	}

	/**
	 * The weather words of the moment from the weather's short summary (ULyudWeatherSubsystem::GetSky; the same thresholds as
	 * the townsfolk's talk, TownBarks WeatherWords): rain (> 0.2 mm/h, not snow), snow, fog, cold (< 2 C), warm (> 22 C),
	 * clear (few clouds, dry, no fog), wet ground, dry (nothing falling).
	 */
	inline uint8_t WeatherBits(float Cloud, float RainMmH, float Snow, float Fog, float TempC, float Wet)
	{
		uint8_t Bits = 0;
		Bits |= RainMmH > 0.2f && Snow < 0.5f ? WeatherRain : 0;
		Bits |= RainMmH > 0.1f && Snow >= 0.5f ? WeatherSnow : 0;
		Bits |= Fog > 0.45f ? WeatherFog : 0;
		Bits |= TempC < 2.f ? WeatherCold : 0;
		Bits |= TempC > 22.f ? WeatherWarm : 0;
		Bits |= Cloud < 0.35f && RainMmH < 0.05f && Fog <= 0.45f ? WeatherClear : 0;
		Bits |= Wet > 0.4f ? WeatherWet : 0;
		Bits |= RainMmH <= 0.1f ? WeatherDry : 0;
		return Bits;
	}

	/** The stamps collected: set -> pieces (docs/city_interiors.md "Collections"). */
	using FStamps = std::map<std::string, std::set<std::string>>;

	/** "set/piece" split at the first slash; false when there is none. */
	inline bool SplitStamp(const std::string& Stamp, std::string& Set, std::string& Piece)
	{
		const size_t Slash = Stamp.find('/');
		if (Slash == std::string::npos || Slash == 0 || Slash + 1 >= Stamp.size())
		{
			return false;
		}
		Set = Stamp.substr(0, Slash);
		Piece = Stamp.substr(Slash + 1);
		return true;
	}

	/** The game's date as the rules need it. */
	struct FWhen
	{
		/** Whole days since some fixed day (the daily limits reset when it changes). */
		long long Day = 0;
		/** 0..24. */
		float Hour = 12.f;
		/** Monday = 0 .. Sunday = 6. */
		int Weekday = 0;
		/** The calendar date as month * 100 + day (1231 = 31 December); 0: unknown (date conditions then hold). */
		int MonthDay = 0;
		/** Whole days since the game began (UGameCalendar::StartDate): 0 on the first day. */
		long long Since = 0;
		/** A holiday today (ULyudFestivalSubsystem::IsFestivalToday: not the weekly market), and the ids of every festival on
		 *  now (ActiveFestivals, the market included). Filled only when an act runs (it allocates). */
		bool bFestival = false;
		std::vector<std::string> Festivals;
		/** EWeatherBit words of the weather now; 0: unknown (every weather condition then fails). */
		uint8_t Weather = 0;
	};

	/** Is Hour inside [Open, Close)? Close below Open runs past midnight; Close - Open >= 24 is always. */
	inline bool InHours(float Hour, float Open, float Close)
	{
		if (Close - Open >= 24.f || (Open <= 0.f && Close >= 24.f))
		{
			return true;
		}
		const float O = std::fmod(Open + 48.f, 24.f);
		const float C = std::fmod(Close + 48.f, 24.f);
		if (O == C)
		{
			return true;
		}
		return O < C ? (Hour >= O && Hour < C) : (Hour >= O || Hour < C);
	}

	/** "MM-DD" (or "M-D") as month * 100 + day; -1 when it is not a date. */
	inline int ParseMonthDay(const std::string& Text)
	{
		const size_t Dash = Text.find('-');
		if (Dash == std::string::npos || Dash == 0 || Dash + 1 >= Text.size())
		{
			return -1;
		}
		const int M = std::atoi(Text.substr(0, Dash).c_str());
		const int D = std::atoi(Text.substr(Dash + 1).c_str());
		return M >= 1 && M <= 12 && D >= 1 && D <= 31 ? M * 100 + D : -1;
	}

	/** A line's festival condition: none given, "any" holiday, one of the ids in Festival, "none" (no holiday today). */
	enum class EFestivalCond : uint8_t
	{
		None,
		Any,
		Ids,
		NotToday,
	};

	/**
	 * One line of an act and when it may be shown. interiors.json writes a plain key when the line has no conditions, else
	 * {"k": key, "h": [from, to), "d": "mon-fri", "date": ["12-20", "01-10"] (or one "MM-DD"), "after": days since the game
	 * began, "before": days since the game began it stops, "once": true (shown once at a thing), "every": [n, r] (days since the
	 * game began % n == r), "festival": "any" | "none" | id | [ids], "weather": word |
	 * [words] (any of them), "have": "set/piece", "count": ["set", n] (at least n stamps of the set), "collect": "set/piece"
	 * (not a condition: the stamp the hero gets when the line is shown)}. Every condition given must hold.
	 */
	struct FLine
	{
		std::string Key;
		float HourFrom = -1.f, HourTo = -1.f;
		uint8_t Days = 0x7F;
		int DateFrom = -1, DateTo = -1;
		int After = 0;
		/** Shown only before this many days since the game began (0: no bound): with After, a notice's days on the board. */
		int Before = 0;
		/** Shown once at a thing, then skipped there (the model remembers it; saved). */
		bool bOnce = false;
		int Every = 0, EveryAt = 0;
		EFestivalCond Festival = EFestivalCond::None;
		std::vector<std::string> FestivalIds;
		/** Any of these EWeatherBit words (0: no weather condition). */
		uint8_t Weather = 0;
		/** "set/piece" the hero must have (empty: none). */
		std::string Have;
		/** At least CountMin stamps of CountSet (CountMin 0: no condition). */
		std::string CountSet;
		int CountMin = 0;
		/** The stamp shown lines give ("set/piece", empty: none). */
		std::string Collect;
		/** "read": the id of a text (FReadText) the hero must have read to the end (empty: no condition). */
		std::string ReadText;
		/** "did": an act's target (what the quests hear as "do") the hero has done at least once; "!target": not yet (empty: no condition). */
		std::string Did;
		/** "heard": the id of a staff talk topic (FTopic) the hero has asked about; "!id": not yet (empty: no condition). */
		std::string Heard;

		bool IsFestive() const { return Festival == EFestivalCond::Any || Festival == EFestivalCond::Ids; }

		/** "did"/"heard" against a set: Name in it, or "!name": not in it (a null set holds nothing). */
		static bool HasName(const std::set<std::string>* Set, const std::string& Name)
		{
			if (!Name.empty() && Name[0] == '!')
			{
				return !Set || Set->count(Name.substr(1)) == 0;
			}
			return Set && Set->count(Name) > 0;
		}

		/** Do the conditions hold? Stamps: the collections (null: none collected, so "have" and "count" fail); ReadMap: the
		 *  texts read to the end (text id -> plot; null: none read, so "read" fails); DoneSet: the targets done ("did"; null:
		 *  nothing done); HeardSet: the topics asked about ("heard"; null: none). */
		bool Holds(const FWhen& W, const FStamps* Stamps = nullptr, const std::map<std::string, std::string>* ReadMap = nullptr,
			const std::set<std::string>* DoneSet = nullptr, const std::set<std::string>* HeardSet = nullptr) const
		{
			if (!ReadText.empty() && (!ReadMap || ReadMap->count(ReadText) == 0))
			{
				return false;
			}
			if (!Did.empty() && !HasName(DoneSet, Did))
			{
				return false;
			}
			if (!Heard.empty() && !HasName(HeardSet, Heard))
			{
				return false;
			}
			if (HourFrom >= 0.f && !InHours(W.Hour, HourFrom, HourTo))
			{
				return false;
			}
			if (Days != 0x7F && !(Days & (1u << (W.Weekday % 7))))
			{
				return false;
			}
			if (DateFrom > 0 && W.MonthDay > 0)
			{
				const bool bIn = DateFrom <= DateTo ? (W.MonthDay >= DateFrom && W.MonthDay <= DateTo) : (W.MonthDay >= DateFrom || W.MonthDay <= DateTo);
				if (!bIn)
				{
					return false;
				}
			}
			if (After > 0 && W.Since < After)
			{
				return false;
			}
			if (Before > 0 && W.Since >= Before)
			{
				return false;
			}
			if (Every > 1 && (((W.Since % Every) + Every) % Every) != EveryAt)
			{
				return false;
			}
			if (Festival == EFestivalCond::Any && !W.bFestival)
			{
				return false;
			}
			if (Festival == EFestivalCond::NotToday && W.bFestival)
			{
				return false;
			}
			if (Festival == EFestivalCond::Ids && std::find_first_of(W.Festivals.begin(), W.Festivals.end(), FestivalIds.begin(), FestivalIds.end()) == W.Festivals.end())
			{
				return false;
			}
			if (Weather != 0 && (W.Weather & Weather) == 0)
			{
				return false;
			}
			if (!Have.empty() || CountMin > 0)
			{
				std::string Set, Piece;
				if (!Have.empty())
				{
					SplitStamp(Have, Set, Piece);
					const auto It = Stamps ? Stamps->find(Set) : FStamps::const_iterator();
					if (!Stamps || It == Stamps->end() || It->second.count(Piece) == 0)
					{
						return false;
					}
				}
				if (CountMin > 0)
				{
					const auto It = Stamps ? Stamps->find(CountSet) : FStamps::const_iterator();
					if (!Stamps || It == Stamps->end() || (int)It->second.size() < CountMin)
					{
						return false;
					}
				}
			}
			return true;
		}
	};

	/** A collection: a set of stamps the hero gathers (films seen, shelves read, kiosks visited), its title key and pieces. */
	struct FCollection
	{
		std::string Id;
		std::string Title;
		std::vector<std::string> Pieces;
	};

	/** What an act is (the defaults of a kind are made the same way). */
	struct FAct
	{
		std::string Id;
		std::string Prompt;
		std::string Title;
		std::vector<FLine> Lines;
		/** "rotate": "daily": one line a game day, the next one on the next day it is read (notice boards, wall newspapers); else
		 *  the next line on every use (a shelf's pages). */
		bool bDaily = false;
		int Minutes = 0;
		/** The hour the act ends at (sleep till 7): the minutes from now to the next such hour; -1: use Minutes. */
		int Until = -1;
		int Price = 0;
		std::vector<std::pair<std::string, int>> Give;
		std::string NeedsItem;
		std::string NeedsStaff;
		std::string Pose = "stand";
		std::string Sound;
		std::string Event;
		std::string Target;
		int Daily = 0;
		std::string Shop;
		std::string Screen;
		bool bFade = false;
		/** Usable whatever the plot's hours (seats, beds, doors: if the hero is in, he may sit down). */
		bool bAnyHour = false;
		/** The stamp every run gives ("set/piece", empty: none). */
		std::string Collect;
		/** Index into FModel::Games: the act plays a mini-game instead of showing its lines (-1: none). */
		int Game = -1;
		/** "read": the texts (indices into FModel::Texts) the reading view opens instead of the lines: one per use in turn, or
		 *  (bReadDaily) one a game day. Empty: the act shows its lines. */
		std::vector<int> Texts;
		bool bReadDaily = false;
		/** "sleep": lying down, the hero chooses the hour he wakes at (a home's or a hotel's bed) instead of the act's minutes. */
		bool bSleep = false;
		/** "topics": what the staff here can be asked about (indices into FModel::Topics), besides the act itself. */
		std::vector<int> Topics;
	};

	/**
	 * A thing the staff at a counter can be asked about (docs/city_interiors.md "Staff talk"; "topics" of interiors.json): the
	 * chooser's entry, when it is offered (the line conditions), whether it goes once heard, the answer (every line that holds
	 * now, in order: by the hour, the weather, a holiday, what the hero did, read or collected), the stamp hearing it the first
	 * time gives. Asking reports the quest event "ask" with the topic's id.
	 */
	struct FTopic
	{
		std::string Id;
		std::string Title;
		/** The conditions under which the topic is offered (its Key unused). */
		FLine When;
		bool bOnce = false;
		std::vector<FLine> Lines;
		std::string Collect;
	};

	/** What asking a topic yields (FModel::AskTopic). */
	struct FTopicAnswer
	{
		int Topic = -1;
		/** The answer's line keys, in order (empty: nothing to say now). */
		std::vector<std::string> Lines;
		/** Heard for the first time now (the chooser's "new" mark goes). */
		bool bFirst = false;
		std::vector<std::string> NewStamps;
		std::string Event = "ask";
		std::string Target;
	};

	/**
	 * A room's sound (docs/city_interiors.md "Room tones"; "tones" of interiors.json): a quiet loop (a soundscape pool's id and
	 * its asset, loaded once) at a volume, and now and then a one-shot of a pool (cups, pages) at a volume, every EveryMin..EveryMax
	 * seconds. While the plot is shut its Closed tone plays instead (-1: silence).
	 */
	struct FTone
	{
		std::string Id;
		std::string Loop;
		std::string Asset;
		float Volume = 0.3f;
		std::vector<std::string> Shots;
		float EveryMin = 12.f, EveryMax = 30.f;
		float ShotVolume = 0.5f;
		int Closed = -1;
	};

	/**
	 * A text the reading view shows ("texts" of interiors.json; docs/city_interiors.md "The reading page"): a title, pages
	 * (each a list of alternatives: the lines whose conditions hold now, one of them by the days since the game began: a
	 * newspaper's second page changes every day), what it is (book, notice, ledger, newspaper: the view's look), the game
	 * minutes a page takes, the stamp reading it to the end gives.
	 */
	struct FReadText
	{
		std::string Id;
		std::string Title;
		std::string Kind = "book";
		int Minutes = 2;
		std::string Collect;
		std::vector<std::vector<FLine>> Pages;
	};

	/** A page turned in the reading view (FModel::TurnPage). */
	struct FPageTurn
	{
		int Page = 0;
		/** The page changed (a turn past the first or last page does nothing). */
		bool bMoved = false;
		/** The last page was reached for the first time: the text is read (stamp, plot remembered). */
		bool bFinished = false;
		std::vector<std::string> NewStamps;
	};

	// --- mini-games (docs/city_interiors.md "Mini-games"): every rule and every die here, the game only draws and polls keys

	/** A small generator (splitmix64): the games' dice, the same numbers in the game and in citysim for one seed. */
	struct FRng
	{
		uint64_t State = 0;
		uint64_t Next()
		{
			State += 0x9E3779B97F4A7C15ull;
			uint64_t Z = State;
			Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
			Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
			return Z ^ (Z >> 31);
		}
		/** [0, 1). */
		double Unit() { return (double)(Next() >> 11) * (1.0 / 9007199254740992.0); }
		/** 0 .. N - 1 (0 when N <= 0). */
		int Below(int N) { return N > 0 ? (int)(Next() % (uint64_t)N) : 0; }
	};

	/** FNV-1a of a text (a thing id into a game's seed). */
	inline uint64_t HashText(const std::string& Text)
	{
		uint64_t H = 1469598103934665603ull;
		for (const char C : Text)
		{
			H ^= (uint64_t)(unsigned char)C;
			H *= 1099511628211ull;
		}
		return H;
	}

	/** aim: a marker runs to and fro, E in the zone (darts, a cue shot, a free throw); rally: E when the ball arrives, the
	 *  window shrinking (table tennis, badminton); duel: three turns of three moves against a regular (chess, dominoes). */
	enum class EGameType : uint8_t
	{
		None,
		Aim,
		Rally,
		Duel,
	};

	inline EGameType GameTypeOf(const std::string& Word)
	{
		return Word == "aim" ? EGameType::Aim : Word == "rally" ? EGameType::Rally : Word == "duel" ? EGameType::Duel : EGameType::None;
	}

	enum class EGameResult : uint8_t
	{
		None,
		Win,
		Lose,
		Draw,
	};

	inline const char* GameResultName(EGameResult R)
	{
		return R == EGameResult::Win ? "win" : R == EGameResult::Lose ? "lose" : R == EGameResult::Draw ? "draw" : "none";
	}

	/** What just happened in a game: the panel's line (city.game.beat.<name>) and the sound. */
	enum class EGameBeat : uint8_t
	{
		None,
		Bull,
		Hit,
		Miss,
		Early,
		Late,
		OppMiss,
		TurnWon,
		TurnLost,
		TurnEven,
	};

	inline const char* GameBeatName(EGameBeat B)
	{
		switch (B)
		{
		case EGameBeat::Bull: return "bull";
		case EGameBeat::Hit: return "hit";
		case EGameBeat::Miss: return "miss";
		case EGameBeat::Early: return "early";
		case EGameBeat::Late: return "late";
		case EGameBeat::OppMiss: return "opp_miss";
		case EGameBeat::TurnWon: return "turn_won";
		case EGameBeat::TurnLost: return "turn_lost";
		case EGameBeat::TurnEven: return "turn_even";
		default: return "none";
		}
	}

	/** Duel moves go round like rock, paper, scissors: move I beats move (I + 2) % 3 (a push catches a slow plan, a plan
	 *  outflanks a wall, a wall stops a push). */
	inline bool MoveBeats(int Mine, int Theirs)
	{
		return Mine >= 0 && Mine < 3 && Theirs >= 0 && Theirs < 3 && Theirs == (Mine + 2) % 3;
	}

	/** One of a duel's three moves: its name, how many points it wins a turn, the opponent's look that betrays it. */
	struct FGameMove
	{
		std::string Key;
		std::string Tell;
		int Power = 1;
	};

	/** The "game" of an act (interiors.json writes every field). */
	struct FGameDef
	{
		EGameType Type = EGameType::None;
		std::string Title;
		/** The key line of the panel (E — throw); empty: the type's own. */
		std::string Keys;
		/** aim: throws; duel: turns (rally plays to Need). */
		int Rounds = 5;
		/** aim alone: points to win (a bull 2, the zone 1); rally: points of the match (with an opponent) or hits in a row (alone). */
		int Need = 5;
		/** aim: to-and-fro runs of the marker a second; rally: ball flights a second at the serve. */
		double Speed = 0.6;
		/** aim: the zone's width (part of the bar); rally: the first window round the ball's arrival (s). */
		double Zone = 0.2;
		/** rally: the window after every hit (x), and the ball's speed-up (x). */
		double Shrink = 0.88;
		double Pace = 1.05;
		/** The regulars who play it ("chess"): one of them is the opponent; empty: alone. */
		std::string Vs;
		std::vector<FGameMove> Moves;
		/** The lines on a win, a loss, a draw ([0] [1] [2]); they turn per thing like a shelf's pages. */
		FAct Results[3];
		/** The stamp a win gives ("chess_wins/{opponent}": the regular beaten). */
		std::string Collect;
		std::vector<std::pair<std::string, int>> Prize;
		/** Games a day (0: no limit); after them the act runs as a plain act (its own lines). */
		int Daily = 0;
		std::string SoundHit, SoundMiss, SoundEnd;
	};

	/** A regular of the town's clubs who plays against the hero: a real resident (npc.<key>.name), a style and words. */
	struct FRegular
	{
		std::string Id;
		std::string Name;
		std::string Style;
		float Skill = 0.5f;
		std::vector<std::string> Plays;
		/** Where they are found (empty: anywhere their game is). */
		std::vector<std::string> Plots;
		/** What they say at the start, when the hero wins, when he loses (keys; may be empty). */
		std::string Hello, SaysWin, SaysLose;
		/** How often they pick each duel move (from the style), and how often their look tells the truth. */
		double Lean[3] = { 1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0 };
		double Honesty = 0.7;
	};

	/** A game being played (the game keeps one; citysim many). Plain data: FModel::TickGame / PressGame move it on. */
	struct FGameState
	{
		int Thing = -1;
		int Act = -1;
		int Def = -1;
		/** Index into FModel::Regulars (-1: alone). */
		int Opponent = -1;
		uint64_t Seed = 0;
		FRng Rng;
		/** Seconds since the start, and the pause during which no press is taken (after a throw, a point, a turn). */
		double Clock = 0.0;
		double Wait = 0.0;
		/** aim: throws made; duel: turns played; rally: points played. */
		int Round = 0;
		int Score = 0;
		int OppScore = 0;
		EGameBeat Beat = EGameBeat::None;
		/** Counts every beat: the game plays a sound and rewrites the panel when it changes. */
		int BeatSerial = 0;
		bool bOver = false;
		EGameResult Result = EGameResult::None;
		// aim: the marker (0..1), the zone's middle, the marker's phase, when the throw began
		double Marker = 0.0;
		double ZoneAt = 0.5;
		double Phase = 0.0;
		double RoundFrom = 0.0;
		// rally: the window (s), a flight's time (s), when the ball was last hit, when it arrives at the hero; going away
		double Window = 0.4;
		double Flight = 1.0;
		double HitAt = 0.0;
		double Arrive = 0.0;
		int Hits = 0;
		bool bAway = false;
		bool bOppMisses = false;
		// duel: the opponent's move this turn, the move his look betrays, the hero's last move and the opponent's last
		int OppMove = -1;
		int Tell = -1;
		int HeroMove = -1;
		int LastOpp = -1;
	};

	/** What a finished game gives (FModel::FinishGame). */
	struct FGameEnd
	{
		EGameResult Result = EGameResult::None;
		/** The result line key (its {0} is the opponent's name), the opponent's name key and their words (may be empty). */
		std::string Line;
		std::string Opponent;
		std::string Says;
		std::vector<std::string> NewStamps;
		std::vector<std::pair<std::string, int>> Prize;
		std::string Target;
	};

	/** "{time}" in a line: the game's clock ("16:40"), for the wall clocks. */
	inline std::string FillTime(const std::string& Text, float Hour)
	{
		const size_t At = Text.find("{time}");
		if (At == std::string::npos)
		{
			return Text;
		}
		const int Minutes = ((int)std::floor(Hour * 60.f + 0.001f) % (24 * 60) + 24 * 60) % (24 * 60);
		char Buf[8];
		std::snprintf(Buf, sizeof(Buf), "%02d:%02d", Minutes / 60, Minutes % 60);
		std::string Out = Text;
		for (size_t P = At; P != std::string::npos; P = Out.find("{time}", P + 5))
		{
			Out.replace(P, 6, Buf);
		}
		return Out;
	}

	/** A storey link's stop (docs/city_interiors.md "Lifts and storey links"): the caption key (empty: a single "to") and the
	 *  door thing it takes the hero to (an index into FModel::Things). */
	struct FFloor
	{
		std::string Key;
		int To = -1;
	};

	struct FThing
	{
		std::string Id;
		std::string Plot;
		std::string Kind;
		FVec At;
		float Yaw = 0.f;
		std::map<std::string, std::string> Props;
		/** Index into FModel::Acts (the named act, or the kind's default). */
		int Act = -1;
		/** The act it does instead while its plot is closed (the chairs on the tables; -1: none, it is just refused). */
		int ClosedAct = -1;
		/** The stamp using it gives ("kiosks/P-KIOSK1", empty: none). */
		std::string Collect;
		/** Open hours [Open, Close) (Close < Open: past midnight) and the weekdays (bit 0 = Monday). */
		bool bHours = false;
		float Open = 0.f, Close = 24.f;
		uint8_t Days = 0x7F;
		/** Topics of this thing's own (a rule's "topics"), offered after its act's (indices into FModel::Topics). */
		std::vector<int> Topics;
		/** A lift or a stair's door: where it takes the hero (one stop: at once; more: a chooser); empty: not a link. */
		std::vector<FFloor> Floors;
		/** No way back is asked of its stops (a slide, a hatch). */
		bool bOneWay = false;
		/** A sitting thing that is not a seat: the seat spot of its building the hero sits on for it ("seat": the cushion's point
		 *  (cm), the way he faces, the cushion's height over the floor (m)); false: none (a free seat thing by it, or standing). */
		bool bSeatSpot = false;
		FVec SeatAt;
		float SeatYaw = 0.f;
		float SeatHeightM = 0.45f;

		std::string Prop(const std::string& Key, const std::string& Default = std::string()) const
		{
			const auto It = Props.find(Key);
			return It == Props.end() ? Default : It->second;
		}
		float PropF(const std::string& Key, float Default) const
		{
			const auto It = Props.find(Key);
			if (It == Props.end() || It->second.empty())
			{
				return Default;
			}
			char* End = nullptr;
			const double V = std::strtod(It->second.c_str(), &End);
			return End == It->second.c_str() ? Default : (float)V;
		}
	};

	struct FGood
	{
		std::string Item;
		int Price = 0;
		int Count = 1;
	};

	/** What the rules ask the game about the hero and the town. */
	struct FAsk
	{
		int Money = 0;
		/** Developer mode (interiors.open_all_hours): open hours, days and staff are not asked. */
		bool bOpenAll = false;
		std::function<int(const std::string& /*Item*/)> ItemCount;
		std::function<bool(const std::string& /*Plot*/, const std::string& /*Role*/)> StaffOnShift;
	};

	enum class EVerdict : uint8_t
	{
		Ok,
		Closed,
		NoMoney,
		NeedsItem,
		NoStaff,
		DailyDone,
		/** A townsperson holds the seat or bed (ClaimThing). */
		Taken,
		Unknown,
	};

	inline const char* VerdictName(EVerdict V)
	{
		switch (V)
		{
		case EVerdict::Ok: return "ok";
		case EVerdict::Closed: return "closed";
		case EVerdict::NoMoney: return "no_money";
		case EVerdict::NeedsItem: return "needs_item";
		case EVerdict::NoStaff: return "no_staff";
		case EVerdict::DailyDone: return "daily_done";
		case EVerdict::Taken: return "taken";
		default: return "unknown";
		}
	}

	/** The string key that tells the hero why an act was refused (city.act.<verdict>). */
	inline std::string RefusalKey(EVerdict V)
	{
		return V == EVerdict::Ok ? std::string() : std::string("city.act.") + VerdictName(V);
	}

	/** What running an act did; the game carries it out. */
	struct FOutcome
	{
		EVerdict Verdict = EVerdict::Unknown;
		int Thing = -1;
		int Act = -1;
		/** The line key shown now (empty: none), and the page title key (may be empty). */
		std::string Line;
		std::string Title;
		/** How many lines the act has (the page shows "next" while there are more to read this time). */
		int NumLines = 0;
		int Minutes = 0;
		int Price = 0;
		bool bFade = false;
		std::vector<std::pair<std::string, int>> Give;
		std::string Event;
		std::string Target;
		std::string Pose;
		std::string Sound;
		std::string Shop;
		std::string Screen;
		/** The first use of anything in this plot today: report "visit" with the plot id. */
		bool bFirstVisitToday = false;
		/** The plot is closed and the thing did its closed act (no visit, no stamp). */
		bool bClosed = false;
		/** Stamps got for the first time by this run ("set/piece"). */
		std::vector<std::string> NewStamps;
		/** The act's mini-game starts (index into FModel::Games; -1: none, the line shows as usual). */
		int Game = -1;
		/** The reading view opens on this text (index into FModel::Texts; -1: none): no line, no act minutes (each page has its own). */
		int Text = -1;
		/** A bed of a "sleep" act: the hero chooses the hour he wakes at (FModel::SleepDefaultHour, SleepStep). */
		bool bSleep = false;
		std::string Plot;
		/** For the refusal line: the price or the item needed. */
		int NeedMoney = 0;
		std::string NeedItem;
	};

	/** Parses a weekday list: "", "daily", "mon-sun", "tue-sun", "fri-mon", "mon,wed,fri", "sat-sun". Bit 0 = Monday. */
	inline uint8_t ParseDays(const std::string& Text)
	{
		static const char* const Names[7] = { "mon", "tue", "wed", "thu", "fri", "sat", "sun" };
		if (Text.empty() || Text == "daily" || Text == "all" || Text == "every")
		{
			return 0x7F;
		}
		auto DayOf = [](const std::string& W) -> int
		{
			for (int I = 0; I < 7; ++I)
			{
				if (W.compare(0, 3, Names[I]) == 0)
				{
					return I;
				}
			}
			return -1;
		};
		uint8_t Mask = 0;
		size_t Start = 0;
		while (Start <= Text.size())
		{
			size_t Comma = Text.find(',', Start);
			if (Comma == std::string::npos)
			{
				Comma = Text.size();
			}
			const std::string Part = Text.substr(Start, Comma - Start);
			const size_t Dash = Part.find('-');
			if (Dash == std::string::npos)
			{
				const int D = DayOf(Part);
				if (D >= 0)
				{
					Mask = (uint8_t)(Mask | (1u << D));
				}
			}
			else
			{
				const int A = DayOf(Part.substr(0, Dash));
				const int B = DayOf(Part.substr(Dash + 1));
				if (A >= 0 && B >= 0)
				{
					for (int D = A, Steps = 0; Steps < 7; D = (D + 1) % 7, ++Steps)
					{
						Mask = (uint8_t)(Mask | (1u << D));
						if (D == B)
						{
							break;
						}
					}
				}
			}
			Start = Comma + 1;
		}
		return Mask ? Mask : (uint8_t)0x7F;
	}

	/** A seat's or bed's surface over its own floor (m): the "height" prop when it is plausible, else the usual one. Some
	 *  exports gave the height over the building's base (6..33 m): sitting there would put the seat's frame metres under the
	 *  floor, and standing up drops the hero through it. */
	constexpr float MinSeatHeight = 0.05f;
	constexpr float MaxSeatHeight = 1.2f;

	/** The bytes of a UTF-8 character starting with Lead. */
	inline int Utf8Len(unsigned char Lead)
	{
		return Lead < 0x80 ? 1 : (Lead >> 5) == 0x6 ? 2 : (Lead >> 4) == 0xE ? 3 : (Lead >> 3) == 0x1E ? 4 : 1;
	}

	/**
	 * A line cut into subtitles for a screen (a film, a play, a TV programme): at the ends of sentences (". ", "! ", "? ",
	 * "… ", a closing quote after them, and the Japanese "。！？" with a closing bracket after them), a piece shorter than
	 * 10 bytes joined to its neighbour, then the shortest neighbours joined until there are at most MaxParts. UTF-8 in and out.
	 */
	inline std::vector<std::string> Subtitles(const std::string& Text, int MaxParts)
	{
		std::vector<std::string> Parts;
		// Japanese runs on without a space
		auto Joiner = [](const std::string& Prev) -> std::string
		{
			const size_t N = Prev.size();
			const bool bCjk = N >= 3 && ((unsigned char)Prev[N - 3] == 0xE3 || (unsigned char)Prev[N - 3] == 0xEF);
			return bCjk ? std::string() : std::string(" ");
		};
		auto IsCloser = [&Text](size_t I, size_t& Len) -> bool
		{
			const unsigned char C = (unsigned char)Text[I];
			if (C == '"' || C == ')' || C == '\'')
			{
				Len = 1;
				return true;
			}
			// » (C2 BB), ” (E2 80 9D), 」 (E3 80 8D), 』 (E3 80 8F)
			if (C == 0xC2 && I + 1 < Text.size() && (unsigned char)Text[I + 1] == 0xBB)
			{
				Len = 2;
				return true;
			}
			if (I + 2 < Text.size() && ((C == 0xE2 && (unsigned char)Text[I + 1] == 0x80 && (unsigned char)Text[I + 2] == 0x9D)
				|| (C == 0xE3 && (unsigned char)Text[I + 1] == 0x80 && ((unsigned char)Text[I + 2] == 0x8D || (unsigned char)Text[I + 2] == 0x8F))))
			{
				Len = 3;
				return true;
			}
			return false;
		};
		std::string Cur;
		size_t I = 0;
		while (I < Text.size())
		{
			const unsigned char C = (unsigned char)Text[I];
			const int Len = Utf8Len(C);
			Cur.append(Text, I, (size_t)Len);
			I += (size_t)Len;
			const bool bLatinEnd = C == '.' || C == '!' || C == '?';
			const bool bEllipsis = Len == 3 && C == 0xE2 && (unsigned char)Text[I - 2] == 0x80 && (unsigned char)Text[I - 1] == 0xA6;
			const bool bCjkEnd = Len == 3 && ((C == 0xE3 && (unsigned char)Text[I - 2] == 0x80 && (unsigned char)Text[I - 1] == 0x82)
				|| (C == 0xEF && (unsigned char)Text[I - 2] == 0xBC && ((unsigned char)Text[I - 1] == 0x81 || (unsigned char)Text[I - 1] == 0x9F)));
			if (!bLatinEnd && !bEllipsis && !bCjkEnd)
			{
				continue;
			}
			// more of the same mark ("?!", "...") and closing quotes stay with the sentence
			size_t CloserLen = 0;
			while (I < Text.size() && (Text[I] == '.' || Text[I] == '!' || Text[I] == '?' || IsCloser(I, CloserLen)))
			{
				const size_t Step = (Text[I] == '.' || Text[I] == '!' || Text[I] == '?') ? 1 : CloserLen;
				Cur.append(Text, I, Step);
				I += Step;
			}
			if (bCjkEnd || I >= Text.size() || Text[I] == ' ')
			{
				while (I < Text.size() && Text[I] == ' ')
				{
					++I;
				}
				Parts.push_back(Cur);
				Cur.clear();
			}
		}
		if (!Cur.empty())
		{
			Parts.push_back(Cur);
		}
		// a piece too short to stand alone ("No.", "Me too") goes with its neighbour
		for (size_t P = 0; P < Parts.size() && Parts.size() > 1;)
		{
			if (Parts[P].size() >= 10)
			{
				++P;
				continue;
			}
			if (P > 0)
			{
				Parts[P - 1] += Joiner(Parts[P - 1]) + Parts[P];
			}
			else
			{
				Parts[1] = Parts[0] + Joiner(Parts[0]) + Parts[1];
			}
			Parts.erase(Parts.begin() + (long)P);
		}
		const size_t Cap = (size_t)(MaxParts > 0 ? MaxParts : 1);
		while (Parts.size() > Cap)
		{
			size_t Best = 0;
			for (size_t P = 1; P + 1 < Parts.size(); ++P)
			{
				if (Parts[P].size() + Parts[P + 1].size() < Parts[Best].size() + Parts[Best + 1].size())
				{
					Best = P;
				}
			}
			Parts[Best] += Joiner(Parts[Best]) + Parts[Best + 1];
			Parts.erase(Parts.begin() + (long)Best + 1);
		}
		return Parts;
	}

	class FModel
	{
	public:
		std::vector<FThing> Things;
		std::vector<FAct> Acts;
		std::map<std::string, std::vector<FGood>> Shops;
		/** The collections of the data ("collections" of interiors.json), in their order. */
		std::vector<FCollection> Collections;
		/** The acts' mini-games (FAct::Game) and the regulars who play them ("regulars" of interiors.json, in id order). */
		std::vector<FGameDef> Games;
		std::vector<FRegular> Regulars;
		/** The texts of the reading view ("texts" of interiors.json), in the file's order. */
		std::vector<FReadText> Texts;
		/** The staff talk's topics ("topics" of interiors.json) and the room tones ("tones"), in the file's order. */
		std::vector<FTopic> Topics;
		std::vector<FTone> Tones;
		/** The most topics a chooser offers at once. */
		static constexpr int MaxTopics = 8;
		/** The most lines an answer has. */
		static constexpr int MaxAnswerLines = 6;

		/** Grid cell size (cm): reach is a few metres, so a pick reads 3x3 cells. */
		static constexpr double CellCm = 400.0;

		/** Reads interiors.json; false (and Error) when it is not JSON. Replaces everything; the saved state is kept. */
		bool Load(const std::string& Text, std::string& Error)
		{
			Things.clear();
			Acts.clear();
			Shops.clear();
			Collections.clear();
			Games.clear();
			Regulars.clear();
			Texts.clear();
			TextIndex.clear();
			Topics.clear();
			TopicIndex.clear();
			Tones.clear();
			ToneIndex.clear();
			PlotTone.clear();
			ActIndex.clear();
			ThingIndex.clear();
			Grid.clear();
			PlotThings.clear();
			HeldBy.clear();
			LyudJson::FValue Root;
			if (!LyudJson::Parse(Text, Root, Error))
			{
				return false;
			}
			if (Root.Type != LyudJson::FValue::Object)
			{
				Error = "interiors: the root is not an object";
				return false;
			}
			for (const auto& Field : Root["texts"].Fields)
			{
				FReadText T = ReadTextDef(Field.first, Field.second);
				if (!T.Pages.empty() && TextIndex.count(T.Id) == 0)
				{
					TextIndex[T.Id] = (int)Texts.size();
					Texts.push_back(std::move(T));
				}
			}
			for (const auto& Field : Root["topics"].Fields)
			{
				FTopic T = ReadTopic(Field.first, Field.second);
				if (!T.Lines.empty() && TopicIndex.count(T.Id) == 0)
				{
					TopicIndex[T.Id] = (int)Topics.size();
					Topics.push_back(std::move(T));
				}
			}
			ReadTones(Root["tones"], Root["plot_tones"]);
			for (const auto& Field : Root["acts"].Fields)
			{
				FAct A = ReadAct(Field.first, Field.second);
				ReadTopicList(Field.second["topics"], A.Topics);
				// "read": {"texts": [ids], "rotate": "use" | "daily"}: an id the file does not have is left out
				const LyudJson::FValue& Read = Field.second["read"];
				for (const LyudJson::FValue& Id : Read["texts"].Items)
				{
					const int Found = FindText(Id.S());
					if (Found >= 0)
					{
						A.Texts.push_back(Found);
					}
				}
				A.bReadDaily = Read["rotate"].S() == "daily";
				const LyudJson::FValue& GameField = Field.second["game"];
				if (GameField.Type == LyudJson::FValue::Object)
				{
					FGameDef Def = ReadGame(GameField);
					if (Def.Type != EGameType::None)
					{
						A.Game = (int)Games.size();
						Games.push_back(std::move(Def));
					}
				}
				ActIndex[A.Id] = (int)Acts.size();
				Acts.push_back(std::move(A));
			}
			for (const auto& Field : Root["regulars"].Fields)
			{
				Regulars.push_back(ReadRegular(Field.first, Field.second));
			}
			for (const auto& Field : Root["shops"].Fields)
			{
				std::vector<FGood>& Goods = Shops[Field.first];
				const LyudJson::FValue& List = Field.second.Type == LyudJson::FValue::Object ? Field.second["goods"] : Field.second;
				for (const LyudJson::FValue& G : List.Items)
				{
					FGood Good;
					Good.Item = G["item"].S();
					Good.Price = G["price"].I(0);
					Good.Count = G["count"].I(1);
					if (!Good.Item.empty())
					{
						Goods.push_back(Good);
					}
				}
			}
			for (const auto& Field : Root["collections"].Fields)
			{
				FCollection C;
				C.Id = Field.first;
				C.Title = Field.second["title"].S();
				for (const LyudJson::FValue& P : Field.second["pieces"].Items)
				{
					if (P.Type == LyudJson::FValue::String && !P.Str.empty())
					{
						C.Pieces.push_back(P.Str);
					}
				}
				Collections.push_back(std::move(C));
			}
			for (const LyudJson::FValue& T : Root["things"].Items)
			{
				FThing Th;
				Th.Id = T["id"].S();
				if (Th.Id.empty() || ThingIndex.count(Th.Id))
				{
					continue;
				}
				Th.Plot = T["plot"].S();
				if (Th.Plot.empty())
				{
					const size_t Slash = Th.Id.find('/');
					Th.Plot = Slash == std::string::npos ? std::string() : Th.Id.substr(0, Slash);
				}
				Th.Kind = T["kind"].S("board");
				const LyudJson::FValue& Ue = T["ue"];
				Th.At.X = Ue[0].Num;
				Th.At.Y = Ue[1].Num;
				Th.At.Z = Ue[2].Num;
				Th.Yaw = T["yaw"].F(0.f);
				for (const auto& P : T["props"].Fields)
				{
					Th.Props[P.first] = ScalarText(P.second);
				}
				const LyudJson::FValue& Hours = T["hours"];
				if (Hours.Type == LyudJson::FValue::Array && Hours.Size() >= 2)
				{
					Th.bHours = true;
					Th.Open = Hours[0].F(0.f);
					Th.Close = Hours[1].F(24.f);
				}
				Th.Days = ParseDays(T["days"].S());
				const std::string ActId = T["act"].S();
				const auto Found = ActIndex.find(ActId);
				Th.Act = ActId.empty() || Found == ActIndex.end() ? DefaultAct(Th) : Found->second;
				const auto Closed = ActIndex.find(T["closed"].S());
				Th.ClosedAct = Closed == ActIndex.end() ? -1 : Closed->second;
				Th.Collect = T["collect"].S();
				ReadTopicList(T["topics"], Th.Topics);
				Th.bOneWay = T["oneway"].Type == LyudJson::FValue::Bool ? T["oneway"].B : T["oneway"].I(0) != 0;
				const LyudJson::FValue& Spot = T["seat"];
				if (Spot.Type == LyudJson::FValue::Array && Spot.Size() >= 5)
				{
					Th.bSeatSpot = true;
					Th.SeatAt.X = Spot[0].Num;
					Th.SeatAt.Y = Spot[1].Num;
					Th.SeatAt.Z = Spot[2].Num;
					Th.SeatYaw = Spot[3].F(0.f);
					const float H = Spot[4].F(0.45f);
					Th.SeatHeightM = H >= MinSeatHeight && H <= MaxSeatHeight ? H : 0.45f;
				}
				if (T["floors"].Type == LyudJson::FValue::Array)
				{
					LiftPending.push_back(std::make_pair((int)Things.size(), &T["floors"]));
				}
				ThingIndex[Th.Id] = (int)Things.size();
				Things.push_back(std::move(Th));
			}
			// the storey links, once every thing is known: "to" is a thing id ("<plot>/<id>"), or an id in the door's own plot
			for (const std::pair<int, const LyudJson::FValue*>& Pending : LiftPending)
			{
				FThing& Door = Things[(size_t)Pending.first];
				for (const LyudJson::FValue& F : Pending.second->Items)
				{
					FFloor Stop;
					Stop.Key = F["k"].S();
					Stop.To = FindLiftTarget(Door.Plot, F["to"].S());
					if (Stop.To >= 0 && Stop.To != Pending.first && (int)Door.Floors.size() < MaxFloors)
					{
						Door.Floors.push_back(Stop);
					}
				}
			}
			LiftPending.clear();
			BuildGrid();
			HeldBy.assign(Things.size(), 0);
			for (int I = 0; I < (int)Things.size(); ++I)
			{
				PlotThings[Things[(size_t)I].Plot].push_back(I);
			}
			return true;
		}

		int FindThing(const std::string& Id) const
		{
			const auto It = ThingIndex.find(Id);
			return It == ThingIndex.end() ? -1 : It->second;
		}
		int FindAct(const std::string& Id) const
		{
			const auto It = ActIndex.find(Id);
			return It == ActIndex.end() ? -1 : It->second;
		}
		const std::vector<FGood>* FindShop(const std::string& Id) const
		{
			const auto It = Shops.find(Id);
			return It == Shops.end() ? nullptr : &It->second;
		}
		const FAct* ActOf(int Thing) const
		{
			return Thing >= 0 && Thing < (int)Things.size() && Things[Thing].Act >= 0 ? &Acts[Things[Thing].Act] : nullptr;
		}

		/** The box round all things (cm); empty model: false. */
		bool Bounds(FVec& Min, FVec& Max) const
		{
			if (Things.empty())
			{
				return false;
			}
			Min = BoxMin;
			Max = BoxMax;
			return true;
		}
		/** Is P within Margin (cm) of the box round all things? (Outside: the game does nothing at all.) */
		bool InBounds(const FVec& P, double Margin) const
		{
			return !Things.empty() && P.X >= BoxMin.X - Margin && P.X <= BoxMax.X + Margin && P.Y >= BoxMin.Y - Margin
				&& P.Y <= BoxMax.Y + Margin && P.Z >= BoxMin.Z - Margin && P.Z <= BoxMax.Z + Margin;
		}

		/**
		 * The things in view from Eye along Dir (unit) within Reach (cm), best first, into Out (at most MaxOut); returns how
		 * many. A thing must be in front, close to the line of sight (more slack near and for big things) and on about the
		 * eye's floor. Reads only the grid cells round the eye; allocates nothing.
		 */
		int Pick(const FVec& Eye, const FVec& Dir, double Reach, int* Out, int MaxOut) const
		{
			if (Things.empty() || MaxOut <= 0)
			{
				return 0;
			}
			const int Cells = (int)std::ceil(Reach / CellCm);
			const long long CX = CellOf(Eye.X), CY = CellOf(Eye.Y);
			int Num = 0;
			double Scores[8];
			const int Cap = MaxOut < 8 ? MaxOut : 8;
			for (long long GX = CX - Cells; GX <= CX + Cells; ++GX)
			{
				for (long long GY = CY - Cells; GY <= CY + Cells; ++GY)
				{
					const auto Cell = Grid.find(Key(GX, GY));
					if (Cell == Grid.end())
					{
						continue;
					}
					for (const int I : Cell->second)
					{
						double Score = 0.0;
						if (!Score1(Things[I], Eye, Dir, Reach, Score))
						{
							continue;
						}
						// insert into the short sorted list
						int Slot = Num < Cap ? Num : Cap;
						while (Slot > 0 && Scores[Slot - 1] > Score)
						{
							if (Slot < Cap)
							{
								Scores[Slot] = Scores[Slot - 1];
								Out[Slot] = Out[Slot - 1];
							}
							--Slot;
						}
						if (Slot < Cap)
						{
							Scores[Slot] = Score;
							Out[Slot] = I;
							if (Num < Cap)
							{
								++Num;
							}
						}
					}
				}
			}
			return Num;
		}

		/** The N nearest things to P (console lists; allocates). Pairs of (thing, distance cm). */
		std::vector<std::pair<int, double>> Nearest(const FVec& P, int N) const
		{
			std::vector<std::pair<int, double>> All;
			All.reserve(Things.size());
			for (int I = 0; I < (int)Things.size(); ++I)
			{
				All.emplace_back(I, Dist(Things[I].At, P));
			}
			const size_t Keep = std::min(All.size(), (size_t)(N > 0 ? N : 0));
			std::partial_sort(All.begin(), All.begin() + (long)Keep, All.end(),
				[](const std::pair<int, double>& A, const std::pair<int, double>& B) { return A.second < B.second; });
			All.resize(Keep);
			return All;
		}

		/** Game minutes from Hour (0..24) to the next o'clock Until (0..23); a whole day when it is that hour already. */
		static int MinutesUntil(float Hour, int Until)
		{
			int M = (int)std::lround((Until - Hour) * 60.f) % (24 * 60);
			return M <= 0 ? M + 24 * 60 : M;
		}

		/** Is Thing's plot shut now (its open hours and days; developer mode's open-all: never)? */
		bool IsShut(int Thing, const FWhen& When, bool bOpenAll) const
		{
			if (Thing < 0 || Thing >= (int)Things.size() || bOpenAll)
			{
				return false;
			}
			const FThing& T = Things[Thing];
			return T.bHours && (!(T.Days & (1u << (When.Weekday % 7))) || !InHours(When.Hour, T.Open, T.Close));
		}

		/** The act Thing does now: its closed act while its plot is shut (if it has one), else its own; -1 none. */
		int ActIndexNow(int Thing, const FWhen& When, bool bOpenAll) const
		{
			if (Thing < 0 || Thing >= (int)Things.size())
			{
				return -1;
			}
			const FThing& T = Things[Thing];
			return T.ClosedAct >= 0 && IsShut(Thing, When, bOpenAll) ? T.ClosedAct : T.Act;
		}
		const FAct* ActNow(int Thing, const FWhen& When, bool bOpenAll) const
		{
			const int I = ActIndexNow(Thing, When, bOpenAll);
			return I >= 0 ? &Acts[(size_t)I] : nullptr;
		}

		/** May Thing's act (or its closed act) run now? No side effects (the prompt may ask this). */
		EVerdict Check(int Thing, const FWhen& When, const FAsk& Ask) const
		{
			const int Index = ActIndexNow(Thing, When, Ask.bOpenAll);
			if (Index < 0)
			{
				return EVerdict::Unknown;
			}
			const FAct* A = &Acts[(size_t)Index];
			const FThing& T = Things[Thing];
			if (Index != T.Act)
			{
				return EVerdict::Ok;     // the closed act: only words, whenever
			}
			if (!A->bAnyHour && IsShut(Thing, When, Ask.bOpenAll))
			{
				return EVerdict::Closed;
			}
			if (!Ask.bOpenAll && !A->NeedsStaff.empty() && !(Ask.StaffOnShift && Ask.StaffOnShift(T.Plot, A->NeedsStaff)))
			{
				return EVerdict::NoStaff;
			}
			if ((A->Pose == "sit" || A->Pose == "lie") && Holder(Thing) != 0)
			{
				return EVerdict::Taken;
			}
			if (A->Daily > 0 && TodayCount(A->Id, When.Day) >= A->Daily)
			{
				return EVerdict::DailyDone;
			}
			if (!A->NeedsItem.empty() && !(Ask.ItemCount && Ask.ItemCount(A->NeedsItem) > 0))
			{
				return EVerdict::NeedsItem;
			}
			if (A->Price > 0 && Ask.Money < A->Price)
			{
				return EVerdict::NoMoney;
			}
			return EVerdict::Ok;
		}

		/** Runs Thing's act: checks it, and when it may run, counts it, turns its page and says what to do. */
		FOutcome Run(int Thing, const FWhen& When, const FAsk& Ask)
		{
			FOutcome O;
			O.Thing = Thing;
			O.Verdict = Check(Thing, When, Ask);
			const int Index = ActIndexNow(Thing, When, Ask.bOpenAll);
			if (Index < 0)
			{
				return O;
			}
			const FAct* A = &Acts[(size_t)Index];
			const FThing& T = Things[Thing];
			O.Act = Index;
			O.Plot = T.Plot;
			O.NeedMoney = A->Price;
			O.NeedItem = A->NeedsItem;
			O.bClosed = Index != T.Act;
			if (O.Verdict != EVerdict::Ok)
			{
				return O;
			}
			RollDay(When.Day);
			Today[A->Id] += 1;
			Uses[A->Id] += 1;
			if (!T.Plot.empty() && !O.bClosed)
			{
				long long& Last = Visited[T.Plot];
				O.bFirstVisitToday = Last != When.Day + 1;     // stored +1: 0 means never
				Last = When.Day + 1;
			}
			O.Title = A->Title;
			// a mini-game instead of the lines, while today's games last (then the act runs plainly: its lines tell of a game)
			const FGameDef* Def = !O.bClosed && A->Game >= 0 && A->Game < (int)Games.size() ? &Games[(size_t)A->Game] : nullptr;
			if (Def && (Def->Daily <= 0 || TodayCount(GameCountKey(A->Id), When.Day) < Def->Daily))
			{
				Today[GameCountKey(A->Id)] += 1;
				O.Game = A->Game;
			}
			// the reading view instead of the lines: a text of the act's (in turn per use, or one a day)
			if (!O.bClosed && O.Game < 0 && !A->Texts.empty())
			{
				O.Text = PickText(PageIdOf(Thing, Index), *A, When);
			}
			const int Holding = O.Game >= 0 || O.Text >= 0 ? 0 : CountUsable(PageIdOf(Thing, Index), *A, When);
			// a daily board shows today's one; a shelf the pages that may be read now, one after another
			O.NumLines = A->bDaily ? (Holding > 0 ? 1 : 0) : Holding;
			if (Holding > 0)
			{
				O.Line = TakeLine(Thing, *A, When, &O.NewStamps);
			}
			if (!O.bClosed)
			{
				if (!IsLineStamp(A->Collect))
				{
					GiveStamp(A->Collect, &O.NewStamps);
				}
				GiveStamp(T.Collect, &O.NewStamps);
			}
			O.Minutes = A->Until >= 0 ? MinutesUntil(When.Hour, A->Until) : A->Minutes;
			if (O.Text >= 0)
			{
				O.Minutes = 0;     // the pages take their own minutes as they are read
			}
			O.bSleep = A->bSleep && !O.bClosed;
			O.Price = A->Price;
			O.bFade = A->bFade;
			O.Give = A->Give;
			O.Event = A->Event.empty() ? std::string("do") : A->Event;
			O.Target = A->Target.empty() ? A->Id : A->Target;
			if (!O.bClosed)
			{
				Done.insert(O.Target);     // what the hero did ("did" lines: the staff know he saw the film)
			}
			O.Pose = A->Pose;
			O.Sound = A->Sound;
			O.Shop = A->Shop;
			O.Screen = A->Screen;
			return O;
		}

		/** The next page of Thing (the page panel's "next"): its line key, and the page turned (a daily act: today's line
		 *  again). Empty: no line may be shown now. A line's first-time stamps go into NewStamps (when given). */
		std::string NextLine(int Thing, const FWhen& When, std::vector<std::string>* NewStamps = nullptr, bool bOpenAll = false)
		{
			const int Index = ActIndexNow(Thing, When, bOpenAll);
			const FAct* A = Index >= 0 ? &Acts[(size_t)Index] : nullptr;
			if (!A || CountUsable(PageIdOf(Thing, Index), *A, When) == 0)
			{
				return std::string();
			}
			return TakeLine(Thing, *A, When, NewStamps);
		}

		/** How many of an act's lines may be shown now (Stamps: the collections the "have"/"count" lines ask; null none). */
		static int CountHolding(const FAct& A, const FWhen& When, const FStamps* InStamps = nullptr)
		{
			int N = 0;
			for (const FLine& L : A.Lines)
			{
				N += L.Holds(When, InStamps) ? 1 : 0;
			}
			return N;
		}

		/** The page a thing's act keeps (the thing's id; its closed act: "<id>#closed"). */
		std::string PageIdOf(int Thing, int Act) const
		{
			const FThing& T = Things[(size_t)Thing];
			return Act >= 0 && Act == T.ClosedAct ? T.Id + "#closed" : T.Id;
		}
		/** May this line be shown at this page now: its conditions hold and, a "once" line, it was not shown there yet. */
		bool Usable(const std::string& PageId, const FLine& L, const FWhen& When) const
		{
			return HoldsNow(L, When) && !(L.bOnce && OnceShown.count(PageId + "|" + L.Key));
		}
		/** Do a line's conditions hold now, the hero's stamps, texts read, deeds done and topics heard included? */
		bool HoldsNow(const FLine& L, const FWhen& When) const
		{
			return L.Holds(When, &Stamps, &ReadAt, &Done, &Heard);
		}

		// --- the staff talk (docs/city_interiors.md "Staff talk"): topics a counter's staff can be asked about besides the act

		int FindTopic(const std::string& Id) const
		{
			const auto It = TopicIndex.find(Id);
			return It == TopicIndex.end() ? -1 : It->second;
		}
		const FTopic* TopicAt(int Topic) const { return Topic >= 0 && Topic < (int)Topics.size() ? &Topics[(size_t)Topic] : nullptr; }
		/** Does Thing have topics at all (its act's or its own)? No allocation (the prompt may ask). */
		bool HasTopics(int Thing) const
		{
			const FAct* A = ActOf(Thing);
			return Thing >= 0 && Thing < (int)Things.size() && ((A && !A->Topics.empty()) || !Things[(size_t)Thing].Topics.empty());
		}
		/** Is the topic offered now: its conditions hold, a "once" topic not heard yet, and some answer line holds? */
		bool TopicOffered(int Topic, const FWhen& When) const
		{
			const FTopic* T = TopicAt(Topic);
			if (!T || !HoldsNow(T->When, When) || (T->bOnce && Heard.count(T->Id)))
			{
				return false;
			}
			const std::string Page = "topic:" + T->Id;
			for (const FLine& L : T->Lines)
			{
				if (Usable(Page, L, When))
				{
					return true;
				}
			}
			return false;
		}
		/** The topics Thing offers now (its act's, then its own; each once; at most MaxTopics), into Out; how many. Nothing while
		 *  its plot is shut and it only says its closed words. Allocates (asked when the chooser opens). */
		int TopicsNow(int Thing, const FWhen& When, std::vector<int>& Out, bool bOpenAll = false) const
		{
			Out.clear();
			const FAct* A = ActOf(Thing);
			if (!A || ActIndexNow(Thing, When, bOpenAll) != Things[(size_t)Thing].Act)
			{
				return 0;
			}
			auto Add = [this, &Out, &When](const std::vector<int>& From)
			{
				for (const int Topic : From)
				{
					if ((int)Out.size() < MaxTopics && std::find(Out.begin(), Out.end(), Topic) == Out.end() && TopicOffered(Topic, When))
					{
						Out.push_back(Topic);
					}
				}
			};
			Add(A->Topics);
			Add(Things[(size_t)Thing].Topics);
			return (int)Out.size();
		}
		/** The role whose staff answer at Thing: its act's needs_staff, or "*" (anybody on shift at the plot). */
		std::string TalkRole(int Thing) const
		{
			const FAct* A = ActOf(Thing);
			return A && !A->NeedsStaff.empty() ? A->NeedsStaff : std::string("*");
		}
		/** May the hero talk at Thing now: the act would run (Check is ok, not its closed words), somebody of TalkRole is on
		 *  shift there (not asked in developer mode) and some topic is offered. The topics go into Out; how many (0: no talk). */
		int TalkReady(int Thing, const FWhen& When, const FAsk& Ask, std::vector<int>& Out) const
		{
			Out.clear();
			if (!HasTopics(Thing))
			{
				return 0;
			}
			// the staff still talk when the act itself is refused for the hero's own reasons (done today, no money, no item)
			const EVerdict Why = Check(Thing, When, Ask);
			if (Why != EVerdict::Ok && Why != EVerdict::DailyDone && Why != EVerdict::NoMoney && Why != EVerdict::NeedsItem)
			{
				return 0;
			}
			if (!Ask.bOpenAll && !(Ask.StaffOnShift && Ask.StaffOnShift(Things[(size_t)Thing].Plot, TalkRole(Thing))))
			{
				return 0;
			}
			return TopicsNow(Thing, When, Out, Ask.bOpenAll);
		}
		bool IsHeard(const std::string& TopicId) const { return Heard.count(TopicId) > 0; }
		const std::set<std::string>& GetHeard() const { return Heard; }
		/** The hero asks about Topic: every answer line that may be shown now (a "once" line then no more), heard (the first time:
		 *  its stamp), the quest event "ask"/<topic id>. */
		FTopicAnswer AskTopic(int Topic, const FWhen& When)
		{
			FTopicAnswer Out;
			const FTopic* T = TopicAt(Topic);
			if (!T)
			{
				return Out;
			}
			Out.Topic = Topic;
			Out.Target = T->Id;
			const std::string Page = "topic:" + T->Id;
			for (const FLine& L : T->Lines)
			{
				if ((int)Out.Lines.size() < MaxAnswerLines && Usable(Page, L, When))
				{
					Out.Lines.push_back(L.Key);
					if (L.bOnce)
					{
						OnceShown.insert(Page + "|" + L.Key);
					}
					GiveStamp(L.Collect, &Out.NewStamps);
				}
			}
			Out.bFirst = Heard.insert(T->Id).second;
			if (Out.bFirst)
			{
				GiveStamp(T->Collect, &Out.NewStamps);
			}
			return Out;
		}
		/** Developer mode: every topic unheard again (and its "once" lines shown again). */
		void ForgetTopics()
		{
			Heard.clear();
			for (auto It = OnceShown.begin(); It != OnceShown.end();)
			{
				It = It->compare(0, 6, "topic:") == 0 ? OnceShown.erase(It) : std::next(It);
			}
		}
		/** The targets the hero has done ("did" lines). */
		bool HasDone(const std::string& Target) const { return Done.count(Target) > 0; }

		// --- lifts and storey links (docs/city_interiors.md "Lifts and storey links")

		static constexpr int MaxFloors = 9;
		/** How far in front of the arrival door the hero stands (cm), and how far under or over the door's point its floor
		 *  may be (cm): the door's point is somewhere between the floor and the lintel. */
		static constexpr double LiftStep = 60.0;
		static constexpr double LiftFloorBelow = 250.0;
		static constexpr double LiftFloorAbove = 50.0;

		bool IsLift(int Thing) const { return Thing >= 0 && Thing < (int)Things.size() && !Things[(size_t)Thing].Floors.empty(); }
		/** A link's target by its data text: "<plot>/<id>" or "<id>" in Plot; -1 none. */
		int FindLiftTarget(const std::string& Plot, const std::string& To) const
		{
			if (To.empty())
			{
				return -1;
			}
			const int Local = Plot.empty() ? -1 : FindThing(Plot + "/" + To);
			return Local >= 0 ? Local : FindThing(To);
		}
		/** Where the hero may be put arriving at Dest (feet, z = the door's point; the game finds the floor under each and
		 *  tests his capsule there), the first that fits wins: 0.6 m straight in front, 1 m, then to the sides, then 1.4 m.
		 *  He faces away from the door (its yaw). How many went into Out. */
		int LiftSpots(int Dest, FVec* Out, int Max, float& OutYaw) const
		{
			if (Dest < 0 || Dest >= (int)Things.size() || !Out || Max <= 0)
			{
				return 0;
			}
			static constexpr double Spots[][2] = { { LiftStep, 0.0 }, { 100.0, 0.0 }, { LiftStep, 50.0 }, { LiftStep, -50.0 },
				{ 30.0, 75.0 }, { 30.0, -75.0 }, { 140.0, 0.0 } };
			const FThing& T = Things[(size_t)Dest];
			const double R = T.Yaw * 3.14159265358979323846 / 180.0;
			const double Fx = std::cos(R), Fy = std::sin(R);
			int N = 0;
			for (const auto& S : Spots)
			{
				if (N >= Max)
				{
					break;
				}
				FVec P;
				P.X = T.At.X + Fx * S[0] - Fy * S[1];
				P.Y = T.At.Y + Fy * S[0] + Fx * S[1];
				P.Z = T.At.Z;
				Out[N++] = P;
			}
			OutYaw = T.Yaw;
			return N;
		}
		/** Is a floor found under an arrival spot believable for a door whose point is at DoorZ? */
		static bool LiftFloorOk(double DoorZ, double FloorZ)
		{
			return FloorZ >= DoorZ - LiftFloorBelow && FloorZ <= DoorZ + LiftFloorAbove;
		}
		/** Can the hero come back from Dest to Door through the links (directly, or stop by stop: the roof's door to the street
		 *  floor's, that one to the ground)? Allocates (checks and the data tools only). */
		bool LiftReturns(int Door, int Dest) const
		{
			if (!IsLift(Dest) || Door < 0 || Door >= (int)Things.size())
			{
				return false;
			}
			std::vector<int> Todo(1, Dest);
			std::set<int> Seen;
			Seen.insert(Dest);
			while (!Todo.empty())
			{
				const int At = Todo.back();
				Todo.pop_back();
				for (const FFloor& F : Things[(size_t)At].Floors)
				{
					if (F.To == Door)
					{
						return true;
					}
					if (Seen.insert(F.To).second)
					{
						Todo.push_back(F.To);
					}
				}
			}
			return false;
		}

		// --- room tones (docs/city_interiors.md "Room tones")

		int FindTone(const std::string& Id) const
		{
			const auto It = ToneIndex.find(Id);
			return It == ToneIndex.end() ? -1 : It->second;
		}
		const FTone* ToneAt(int Tone) const { return Tone >= 0 && Tone < (int)Tones.size() ? &Tones[(size_t)Tone] : nullptr; }
		/** A plot's tone (-1 none). */
		int ToneOfPlot(const std::string& Plot) const
		{
			const auto It = PlotTone.find(Plot);
			return It == PlotTone.end() ? -1 : It->second;
		}
		/** The tone heard at Thing now: its plot's, or while the plot is shut that tone's closed one (-1: silence). */
		int ToneNow(int Thing, const FWhen& When, bool bOpenAll) const
		{
			if (Thing < 0 || Thing >= (int)Things.size())
			{
				return -1;
			}
			const int Tone = ToneOfPlot(Things[(size_t)Thing].Plot);
			const FTone* T = ToneAt(Tone);
			if (!T)
			{
				return -1;
			}
			return IsShut(Thing, When, bOpenAll) ? T->Closed : Tone;
		}
		/** Seconds until a tone's next one-shot (EveryMin..EveryMax; -1: it has none). */
		double NextShotIn(int Tone, FRng& Rng) const
		{
			const FTone* T = ToneAt(Tone);
			if (!T || T->Shots.empty())
			{
				return -1.0;
			}
			return (double)T->EveryMin + Rng.Unit() * (double)std::max(0.f, T->EveryMax - T->EveryMin);
		}
		/** Which of a tone's one-shots plays (empty: none). */
		const std::string& PickShot(int Tone, FRng& Rng) const
		{
			static const std::string None;
			const FTone* T = ToneAt(Tone);
			return T && !T->Shots.empty() ? T->Shots[(size_t)Rng.Below((int)T->Shots.size())] : None;
		}
		/** How many of an act's lines may be shown at this page now. */
		int CountUsable(const std::string& PageId, const FAct& A, const FWhen& When) const
		{
			int N = 0;
			for (const FLine& L : A.Lines)
			{
				N += Usable(PageId, L, When) ? 1 : 0;
			}
			return N;
		}

		// --- the townsfolk's use of the things (docs/city_interiors.md "API for townsfolk")

		/** Who holds Thing (0: nobody; the game's holder ids are never 0). */
		uint64_t Holder(int Thing) const
		{
			return Thing >= 0 && Thing < (int)HeldBy.size() ? HeldBy[(size_t)Thing] : 0;
		}
		/** Who takes Thing: true when it was free or already theirs. */
		bool Claim(int Thing, uint64_t Who)
		{
			if (Who == 0 || Thing < 0 || Thing >= (int)HeldBy.size() || (HeldBy[(size_t)Thing] != 0 && HeldBy[(size_t)Thing] != Who))
			{
				return false;
			}
			HeldBy[(size_t)Thing] = Who;
			return true;
		}
		/** Who lets Thing go (nothing when somebody else holds it). */
		void Release(int Thing, uint64_t Who)
		{
			if (Thing >= 0 && Thing < (int)HeldBy.size() && HeldBy[(size_t)Thing] == Who)
			{
				HeldBy[(size_t)Thing] = 0;
			}
		}
		/** A new game or a level: nobody holds anything. */
		void ReleaseAll() { std::fill(HeldBy.begin(), HeldBy.end(), (uint64_t)0); }

		/**
		 * The nearest thing of Kind (empty: any) to P within Radius (cm), on about P's floor (|dz| <= MaxDz), that nobody holds
		 * and that is not Except (the hero's seat); -1 none. Reads only the grid cells round P (4 m cells), allocates nothing.
		 */
		int FindFree(const std::string& Kind, const FVec& P, double Radius, double MaxDz, int Except = -1) const
		{
			if (Things.empty() || Radius <= 0.0)
			{
				return -1;
			}
			const int Cells = (int)std::ceil(Radius / CellCm);
			const long long CX = CellOf(P.X), CY = CellOf(P.Y);
			int Best = -1;
			double BestD = Radius * Radius;
			for (long long GX = CX - Cells; GX <= CX + Cells; ++GX)
			{
				for (long long GY = CY - Cells; GY <= CY + Cells; ++GY)
				{
					const auto Cell = Grid.find(Key(GX, GY));
					if (Cell == Grid.end())
					{
						continue;
					}
					for (const int I : Cell->second)
					{
						const FThing& T = Things[(size_t)I];
						if (I == Except || HeldBy[(size_t)I] != 0 || (!Kind.empty() && T.Kind != Kind) || std::fabs(T.At.Z - P.Z) > MaxDz)
						{
							continue;
						}
						const double D2 = (T.At.X - P.X) * (T.At.X - P.X) + (T.At.Y - P.Y) * (T.At.Y - P.Y);
						if (D2 <= BestD)
						{
							BestD = D2;
							Best = I;
						}
					}
				}
			}
			return Best;
		}

		/** The things of a plot (indices, in the file's order); null for a plot without things. Built once at load. */
		const std::vector<int>* ThingsOfPlot(const std::string& Plot) const
		{
			const auto It = PlotThings.find(Plot);
			return It == PlotThings.end() ? nullptr : &It->second;
		}

		// --- collections

		const FCollection* FindCollection(const std::string& Set) const
		{
			for (const FCollection& C : Collections)
			{
				if (C.Id == Set)
				{
					return &C;
				}
			}
			return nullptr;
		}
		/** Is "set/piece" a piece of a collection of the data? */
		bool IsPiece(const std::string& Stamp) const
		{
			std::string Set, Piece;
			const FCollection* C = SplitStamp(Stamp, Set, Piece) ? FindCollection(Set) : nullptr;
			return C && std::find(C->Pieces.begin(), C->Pieces.end(), Piece) != C->Pieces.end();
		}
		bool HasStamp(const std::string& Stamp) const
		{
			std::string Set, Piece;
			if (!SplitStamp(Stamp, Set, Piece))
			{
				return false;
			}
			const auto It = Stamps.find(Set);
			return It != Stamps.end() && It->second.count(Piece) > 0;
		}
		/** How many pieces of Set the hero has (only pieces the data still declares). */
		int CountStamps(const std::string& Set) const
		{
			const FCollection* C = FindCollection(Set);
			const auto It = Stamps.find(Set);
			if (!C || It == Stamps.end())
			{
				return 0;
			}
			int N = 0;
			for (const std::string& P : C->Pieces)
			{
				N += It->second.count(P) ? 1 : 0;
			}
			return N;
		}
		const FStamps& GetStamps() const { return Stamps; }
		/** Gives "set/piece" (a piece the data declares); true when it is new. */
		bool GiveStamp(const std::string& Stamp, std::vector<std::string>* NewStamps = nullptr)
		{
			std::string Set, Piece;
			if (Stamp.empty() || !IsPiece(Stamp) || !SplitStamp(Stamp, Set, Piece) || !Stamps[Set].insert(Piece).second)
			{
				return false;
			}
			if (NewStamps)
			{
				NewStamps->push_back(Stamp);
			}
			return true;
		}
		/** Developer mode: every piece of Set ("*": of every set); false for an unknown set. */
		bool GrantSet(const std::string& Set)
		{
			bool bAny = false;
			for (const FCollection& C : Collections)
			{
				if (Set == "*" || C.Id == Set)
				{
					bAny = true;
					Stamps[C.Id].insert(C.Pieces.begin(), C.Pieces.end());
				}
			}
			return bAny;
		}
		/** Developer mode: Set ("*": every set) starts over. */
		void ClearSet(const std::string& Set)
		{
			if (Set == "*")
			{
				Stamps.clear();
			}
			else
			{
				Stamps.erase(Set);
			}
		}

		/** A seat's or bed's surface over its floor, metres (see MinSeatHeight). */
		static float SeatHeight(const FThing& T, bool bLie)
		{
			const float Usual = bLie ? 0.5f : 0.45f;
			const float H = T.PropF("height", Usual);
			return H >= MinSeatHeight && H <= MaxSeatHeight ? H : Usual;
		}

		/**
		 * The thing nearest P within Radius (cm) on about P's floor (|dz| <= MaxDz), from the grid only (no allocation); -1
		 * none. The game asks it a few times a second for the plot the hero is in (a "visit" when it changes).
		 */
		int NearestThing(const FVec& P, double Radius, double MaxDz) const
		{
			if (Things.empty())
			{
				return -1;
			}
			const int Cells = (int)std::ceil(Radius / CellCm);
			const long long CX = CellOf(P.X), CY = CellOf(P.Y);
			int Best = -1;
			double BestD = Radius;
			for (long long GX = CX - Cells; GX <= CX + Cells; ++GX)
			{
				for (long long GY = CY - Cells; GY <= CY + Cells; ++GY)
				{
					const auto Cell = Grid.find(Key(GX, GY));
					if (Cell == Grid.end())
					{
						continue;
					}
					for (const int I : Cell->second)
					{
						const FVec& At = Things[I].At;
						if (std::fabs(At.Z - P.Z) > MaxDz)
						{
							continue;
						}
						const double D = std::sqrt((At.X - P.X) * (At.X - P.X) + (At.Y - P.Y) * (At.Y - P.Y));
						if (D <= BestD)
						{
							BestD = D;
							Best = I;
						}
					}
				}
			}
			return Best;
		}

		/** Is this act a screen show (a film, a play, the TV: the page in the screen style, its line as subtitles)? */
		static bool IsScreen(const FAct& A) { return !A.Screen.empty(); }
		/** Does the hero sit on the thing itself (a seat, a bed, or anything with a seat height: the cinema's hall seats)? */
		static bool IsSeatThing(const FThing& T) { return T.Kind == "seat" || T.Kind == "bed" || T.Props.count("height") > 0; }

		/**
		 * The screen a hero seated on Seat watches: the nearest thing of the same plot within Radius (cm) whose act is a screen
		 * show and that is not a seat itself, in front of the seat (within 70 degrees of its yaw) and on about its floor; -1
		 * none. Asked once when he sits down (a pass over all things).
		 */
		int ScreenFrom(int Seat, double Radius) const
		{
			if (Seat < 0 || Seat >= (int)Things.size())
			{
				return -1;
			}
			const FThing& S = Things[Seat];
			const double R = S.Yaw * 3.14159265358979 / 180.0;
			const double FX = std::cos(R), FY = std::sin(R);
			int Best = -1;
			double BestD = Radius;
			for (int I = 0; I < (int)Things.size(); ++I)
			{
				const FThing& T = Things[I];
				const FAct* A = ActOf(I);
				if (I == Seat || T.Plot != S.Plot || !A || !IsScreen(*A) || IsSeatThing(T) || std::fabs(T.At.Z - S.At.Z) > 450.0)
				{
					continue;
				}
				const double DX = T.At.X - S.At.X, DY = T.At.Y - S.At.Y;
				const double D = std::sqrt(DX * DX + DY * DY);
				if (D > BestD || D < 1.0 || (DX * FX + DY * FY) / D < 0.34)
				{
					continue;
				}
				BestD = D;
				Best = I;
			}
			return Best;
		}

		/** Must the hero sit down first to watch Thing's show? A sitting screen act on a thing that is not a seat, in a plot
		 *  that has a seat within Radius (cm) to watch it from. */
		bool NeedsSeatFirst(int Thing, double Radius) const
		{
			const FAct* A = ActOf(Thing);
			if (!A || !IsScreen(*A) || A->Pose != "sit" || IsSeatThing(Things[Thing]))
			{
				return false;
			}
			const FThing& T = Things[Thing];
			for (int I = 0; I < (int)Things.size(); ++I)
			{
				const FThing& S = Things[I];
				if (I != Thing && S.Plot == T.Plot && S.Kind == "seat" && std::fabs(S.At.Z - T.At.Z) < 450.0
					&& std::sqrt((S.At.X - T.At.X) * (S.At.X - T.At.X) + (S.At.Y - T.At.Y) * (S.At.Y - T.At.Y)) <= Radius)
				{
					return true;
				}
			}
			return false;
		}

		/** How far (cm) from a thing that is not a seat, and how far over or under it, a free seat is taken for its sitting act. */
		static constexpr double SeatByRadius = 160.0;
		static constexpr double SeatByFloor = 150.0;
		/**
		 * Where the hero sits for Thing's act with "pose": "sit": the thing itself when it is one to sit on (IsSeatThing), else
		 * the nearest free seat of its plot by it (a chess table's chair, a piano's stool; never Except, the hero's own); with none,
		 * the thing itself when it has a seat spot (bSeatSpot: a barber's chair, a school desk's bench; Sit takes SeatFrame),
		 * else -1: he uses it standing (its point is the middle of a table or a machine, he would sit in it). Reads the grid round
		 * the thing only; no allocation.
		 */
		int SeatFor(int Thing, int Except = -1) const
		{
			if (Thing < 0 || Thing >= (int)Things.size())
			{
				return -1;
			}
			const FThing& T = Things[(size_t)Thing];
			if (IsSeatThing(T))
			{
				return Thing;
			}
			const int Seat = FindFree("seat", T.At, SeatByRadius, SeatByFloor, Except);
			if (Seat >= 0 && Things[(size_t)Seat].Plot == T.Plot)
			{
				return Seat;
			}
			return T.bSeatSpot ? Thing : -1;
		}

		/** Where the hero sits on Seat (one SeatFor gave, or a bed): its seat spot when it is not a seat itself and has one, else
		 *  the thing's own point, yaw and SeatHeight. At is the cushion's point (cm), HeightM over its floor. */
		void SeatFrame(int Seat, bool bLie, FVec& At, float& Yaw, float& HeightM) const
		{
			const FThing& T = Things[(size_t)Seat];
			const bool bSpot = !bLie && T.bSeatSpot && !IsSeatThing(T);
			At = bSpot ? T.SeatAt : T.At;
			Yaw = bSpot ? T.SeatYaw : T.Yaw;
			HeightM = bSpot ? T.SeatHeightM : SeatHeight(T, bLie);
		}

		// --- sitting and lying (docs/city_interiors.md "Sitting and lying"): what the game decides from the data and its traces

		/**
		 * The floor under a seat or bed whose surface is HeightCm over it, its cushion at AtZ: a trace down from just over the
		 * cushion met something at HitZ (bHit). A hit up near the cushion is the seat's own blocking box (a bench, a bed): the floor
		 * is its height below. A hit far from where the floor should be (a stairwell under a gallery bench, a hole in the mesh, a
		 * table's top) is not believed: the height from the data then. No hit: the height from the data.
		 */
		static double SeatFloor(double AtZ, double HeightCm, bool bHit, double HitZ)
		{
			const double Expected = AtZ - HeightCm;
			if (!bHit)
			{
				return Expected;
			}
			const double Floor = HitZ > AtZ - 0.5 * HeightCm ? HitZ - HeightCm : HitZ;
			return std::fabs(Floor - Expected) > SeatFloorSlack ? Expected : Floor;
		}
		/** How far (cm) a traced floor may be from the data's (the seat's height under its cushion) and still be believed. */
		static constexpr double SeatFloorSlack = 60.0;

		/** Half the cone (deg) the seated hero may look round within, about the seat's yaw: the thing's "cone" prop (30..170), else
		 *  110 (he turns his head and shoulders, not his back to the table). */
		static float LookCone(const FThing& T)
		{
			const float C = T.PropF("cone", 110.f);
			return C < 30.f ? 30.f : (C > 170.f ? 170.f : C);
		}
		/** Is Yaw within Half degrees of Center (any turns of 360)? */
		static bool InCone(float Yaw, float Center, float Half)
		{
			float D = std::fmod(Yaw - Center, 360.f);
			D = D > 180.f ? D - 360.f : (D < -180.f ? D + 360.f : D);
			return std::fabs(D) <= Half + 0.001f;
		}
		/** Yaw brought into the cone of Half degrees about Center (the nearest edge when outside). */
		static float ClampToCone(float Yaw, float Center, float Half)
		{
			float D = std::fmod(Yaw - Center, 360.f);
			D = D > 180.f ? D - 360.f : (D < -180.f ? D + 360.f : D);
			D = D > Half ? Half : (D < -Half ? -Half : D);
			return Center + D;
		}

		/**
		 * The window a hero lying on Bed looks toward: the nearest thing of its plot with "topic": "view" (the window views of the
		 * data) within Radius (cm) of it, on about its floor (it may be up to 2.5 m over the bed's surface: a view board's point is
		 * at eye height); -1 none (he looks at the ceiling). A pass over the plot's things only (the table built at load).
		 */
		int WindowFrom(int Bed, double Radius) const
		{
			if (Bed < 0 || Bed >= (int)Things.size())
			{
				return -1;
			}
			const FThing& B = Things[(size_t)Bed];
			const std::vector<int>* Of = ThingsOfPlot(B.Plot);
			int Best = -1;
			double BestD = Radius;
			for (size_t K = 0; Of && K < Of->size(); ++K)
			{
				const int I = (*Of)[K];
				const FThing& T = Things[(size_t)I];
				if (I == Bed || T.Prop("topic") != "view" || T.At.Z < B.At.Z - 100.0 || T.At.Z > B.At.Z + 250.0)
				{
					continue;
				}
				const double D = std::sqrt((T.At.X - B.At.X) * (T.At.X - B.At.X) + (T.At.Y - B.At.Y) * (T.At.Y - B.At.Y));
				if (D <= BestD)
				{
					BestD = D;
					Best = I;
				}
			}
			return Best;
		}

		/** The hour a "sleep" bed offers first: its act's "until" (sleep till 7), else now plus its minutes, to the next whole hour;
		 *  never the hour it is now (that would be a whole day). */
		static int SleepDefaultHour(const FAct& A, float Hour)
		{
			int H = A.Until >= 0 ? A.Until : (int)std::ceil(Hour + (float)(A.Minutes > 0 ? A.Minutes : 60) / 60.f - 0.001f);
			H = ((H % 24) + 24) % 24;
			return H == (int)std::floor(Hour) % 24 ? (H + 1) % 24 : H;
		}
		/** The hour chooser one step on (Delta +1 later, -1 earlier), going round the clock past the hour it is now. */
		static int SleepStep(int Chosen, int Delta, float Hour)
		{
			const int Now = ((int)std::floor(Hour) % 24 + 24) % 24;
			int H = (((Chosen + Delta) % 24) + 24) % 24;
			if (H == Now)
			{
				H = (((H + (Delta >= 0 ? 1 : -1)) % 24) + 24) % 24;
			}
			return H;
		}

		// --- the reading view (docs/city_interiors.md "The reading page"): a text's pages, the page each text was left on, the texts
		// read to the end (and where), saved with the rest

		int FindText(const std::string& Id) const
		{
			const auto It = TextIndex.find(Id);
			return It == TextIndex.end() ? -1 : It->second;
		}
		const FReadText* TextAt(int Text) const { return Text >= 0 && Text < (int)Texts.size() ? &Texts[(size_t)Text] : nullptr; }

		/** The pages of Text that may be shown now, each as its line key: of a page's alternatives those whose conditions hold, one
		 *  of them by the days since the game began; a page with none holding is left out. Allocates (asked when the view opens). */
		void TextPages(int Text, const FWhen& When, std::vector<std::string>& Out) const
		{
			Out.clear();
			const FReadText* T = TextAt(Text);
			if (!T)
			{
				return;
			}
			int Holding[16];
			for (const std::vector<FLine>& Page : T->Pages)
			{
				int N = 0;
				for (int I = 0; I < (int)Page.size() && N < 16; ++I)
				{
					if (HoldsNow(Page[(size_t)I], When))
					{
						Holding[N] = I;
						++N;
					}
				}
				if (N > 0)
				{
					const long long Pick = ((When.Since % N) + N) % N;
					Out.push_back(Page[(size_t)Holding[(size_t)Pick]].Key);
				}
			}
		}

		/** The page the view opens Text on (NumPages of them now): where it was left; the last one of a text read to the end starts
		 *  it over. */
		int OpenPage(int Text, int NumPages) const
		{
			const FReadText* T = TextAt(Text);
			if (!T || NumPages <= 0)
			{
				return 0;
			}
			const auto It = BookAt.find(T->Id);
			const int Left = It == BookAt.end() ? 0 : It->second;
			if (Left >= NumPages - 1 && ReadAt.count(T->Id) && NumPages > 1)
			{
				return 0;
			}
			return Left < 0 ? 0 : (Left > NumPages - 1 ? NumPages - 1 : Left);
		}

		/** The view turns from Page by Delta (-1, 0 at opening, +1) of NumPages: the page is remembered; reaching the last one the
		 *  first time reads the text (its stamp, the plot it was read in). */
		FPageTurn TurnPage(int Text, int Page, int Delta, int NumPages, const std::string& Plot)
		{
			FPageTurn Out;
			const FReadText* T = TextAt(Text);
			if (!T || NumPages <= 0)
			{
				return Out;
			}
			int To = Page + Delta;
			To = To < 0 ? 0 : (To > NumPages - 1 ? NumPages - 1 : To);
			Out.Page = To;
			Out.bMoved = To != Page;
			BookAt[T->Id] = To;
			if (To == NumPages - 1 && ReadAt.count(T->Id) == 0)
			{
				ReadAt[T->Id] = Plot;
				Out.bFinished = true;
				GiveStamp(T->Collect, &Out.NewStamps);
			}
			return Out;
		}

		bool IsRead(const std::string& TextId) const { return ReadAt.count(TextId) > 0; }
		/** The texts read to the end: text id -> the plot (where). */
		const std::map<std::string, std::string>& GetReadAt() const { return ReadAt; }
		int CountRead() const
		{
			int N = 0;
			for (const FReadText& T : Texts)
			{
				N += ReadAt.count(T.Id) ? 1 : 0;
			}
			return N;
		}
		bool AllRead() const { return !Texts.empty() && CountRead() == (int)Texts.size(); }
		/** Developer mode: every text read (at Plot, its stamps given) or none (the pages left on forgotten too). */
		void MarkAllRead(bool bRead, const std::string& Plot)
		{
			if (!bRead)
			{
				ReadAt.clear();
				BookAt.clear();
				return;
			}
			for (const FReadText& T : Texts)
			{
				if (ReadAt.emplace(T.Id, Plot).second)
				{
					GiveStamp(T.Collect);
				}
			}
		}

		// --- mini-games (docs/city_interiors.md "Mini-games"): the game calls StartGame, then TickGame every frame and PressGame
		// on a key, draws GameBar, and calls FinishGame once bOver and the clock is past Wait. Nothing here allocates per frame.

		/** Pauses (s): before the first throw, serve or turn; after a throw; after a point; after a turn; before the result. */
		static constexpr double GameStartPause = 1.2;
		static constexpr double AimPause = 0.9;
		static constexpr double RallyPause = 1.0;
		static constexpr double DuelPause = 1.4;
		static constexpr double EndPause = 1.3;
		/** An aim throw not made in this long (s) is a miss. */
		static constexpr double AimRoundLimit = 8.0;
		/** The narrowest rally window and the shortest flight (s); a press in the first part of a flight is not taken. */
		static constexpr double MinWindow = 0.08;
		static constexpr double MinFlight = 0.35;
		static constexpr double RallyDeaf = 0.4;
		/** A tick moves a game's clock on at most this much (s): a hitch must not skip a whole window. */
		static constexpr double MaxGameStep = 0.25;

		const FGameDef* GameDef(int Def) const { return Def >= 0 && Def < (int)Games.size() ? &Games[(size_t)Def] : nullptr; }
		/** The game of Thing's own act (null: none). */
		const FGameDef* GameOfThing(int Thing) const
		{
			const FAct* A = ActOf(Thing);
			return A ? GameDef(A->Game) : nullptr;
		}
		const FRegular* RegularOf(const FGameState& G) const
		{
			return G.Opponent >= 0 && G.Opponent < (int)Regulars.size() ? &Regulars[(size_t)G.Opponent] : nullptr;
		}
		int FindRegular(const std::string& Id) const
		{
			for (int I = 0; I < (int)Regulars.size(); ++I)
			{
				if (Regulars[(size_t)I].Id == Id)
				{
					return I;
				}
			}
			return -1;
		}
		/** Today's games of an act are counted under this key (saved with the other counts). */
		static std::string GameCountKey(const std::string& ActId) { return "#g:" + ActId; }

		/**
		 * The regulars who may sit down against the hero at Plot for Def, into Out (at most Cap): those who play its game and
		 * are found at Plot (or anywhere), else everybody who plays it. Returns how many (0: Def is played alone).
		 */
		int Opponents(const FGameDef& Def, const std::string& Plot, int* Out, int Cap) const
		{
			int N = 0;
			if (Def.Vs.empty())
			{
				return 0;
			}
			for (int Pass = 0; Pass < 2 && N == 0; ++Pass)
			{
				for (int I = 0; I < (int)Regulars.size() && N < Cap; ++I)
				{
					const FRegular& R = Regulars[(size_t)I];
					const bool bPlays = std::find(R.Plays.begin(), R.Plays.end(), Def.Vs) != R.Plays.end();
					const bool bHere = R.Plots.empty() || std::find(R.Plots.begin(), R.Plots.end(), Plot) != R.Plots.end();
					if (bPlays && (Pass == 1 || bHere))
					{
						Out[N++] = I;
					}
				}
			}
			return N;
		}

		/** A game of Thing's act from this seed (the opponent and every die come from it); false when its act has no game. */
		bool StartGameSeeded(int Thing, uint64_t Seed, FGameState& G) const
		{
			const FAct* A = ActOf(Thing);
			const FGameDef* D = A ? GameDef(A->Game) : nullptr;
			G = FGameState();
			if (!D)
			{
				return false;
			}
			G.Thing = Thing;
			G.Act = Things[(size_t)Thing].Act;
			G.Def = A->Game;
			G.Seed = Seed;
			G.Rng.State = Seed;
			int Pool[32];
			const int N = Opponents(*D, Things[(size_t)Thing].Plot, Pool, 32);
			G.Opponent = N > 0 ? Pool[G.Rng.Below(N)] : -1;
			BeginRound(*D, G, GameStartPause);
			return true;
		}

		/** A game of Thing's act now: the seed from the thing, the day and how often its act has run (every game its own). */
		bool StartGame(int Thing, const FWhen& When, FGameState& G) const
		{
			const FAct* A = ActOf(Thing);
			if (!A)
			{
				G = FGameState();
				return false;
			}
			const uint64_t Seed = HashText(Things[(size_t)Thing].Id) ^ ((uint64_t)When.Day * 0x9E3779B97F4A7C15ull)
				^ ((uint64_t)TotalUses(A->Id) * 0xC2B2AE3D27D4EB4Full);
			return StartGameSeeded(Thing, Seed, G);
		}

		/** The game's clock moves on Dt seconds: the aim marker runs (a throw not made in time is a miss), a rally's ball flies
		 *  (not returned in its window: the point is lost; the opponent's miss: won). */
		void TickGame(FGameState& G, double Dt) const
		{
			const FGameDef* D = GameDef(G.Def);
			if (!D)
			{
				return;
			}
			G.Clock += Dt > 0.0 ? std::min(Dt, MaxGameStep) : 0.0;
			if (G.bOver || G.Clock < G.Wait)
			{
				return;
			}
			if (D->Type == EGameType::Aim)
			{
				G.Marker = MarkerAt(*D, G, G.Clock);
				if (G.Clock - G.RoundFrom > AimRoundLimit)
				{
					Throw(*D, G, false);
				}
			}
			else if (D->Type == EGameType::Rally)
			{
				if (!G.bAway && G.Clock > G.Arrive + G.Window * 0.5)
				{
					RallyPoint(*D, G, false, EGameBeat::Late);
				}
				else if (G.bAway && G.Clock >= G.HitAt + G.Flight)
				{
					if (G.bOppMisses)
					{
						RallyPoint(*D, G, true, EGameBeat::OppMiss);
					}
					else
					{
						// returned: the ball comes back as fast as it went
						G.bAway = false;
						G.HitAt += G.Flight;
						G.Arrive = G.HitAt + G.Flight;
					}
				}
			}
		}

		/** A key: E (Choice -1) throws (aim) or returns the ball (rally); a duel takes the move 0..2. Ignored in a pause. */
		void PressGame(FGameState& G, int Choice) const
		{
			const FGameDef* D = GameDef(G.Def);
			if (!D || G.bOver || G.Clock < G.Wait)
			{
				return;
			}
			if (D->Type == EGameType::Aim && Choice < 0)
			{
				G.Marker = MarkerAt(*D, G, G.Clock);
				Throw(*D, G, true);
			}
			else if (D->Type == EGameType::Rally && Choice < 0 && !G.bAway)
			{
				if (G.Clock < G.HitAt + RallyDeaf * G.Flight)
				{
					return;     // the ball has only just left the other bat: a press this early is not taken
				}
				if (std::fabs(G.Clock - G.Arrive) > G.Window * 0.5)
				{
					RallyPoint(*D, G, false, EGameBeat::Early);
					return;
				}
				++G.Hits;
				SetBeat(G, EGameBeat::Hit);
				G.HitAt = G.Clock;
				G.bAway = true;
				G.Window = std::max(MinWindow, G.Window * D->Shrink);
				G.Flight = std::max(MinFlight, G.Flight / std::max(1.0, D->Pace));
				if (const FRegular* R = RegularOf(G))
				{
					const double Back = std::min(0.96, std::max(0.25, 0.55 + 0.4 * (double)R->Skill - 0.05 * G.Hits));
					G.bOppMisses = G.Rng.Unit() >= Back;
				}
				else
				{
					G.bOppMisses = false;     // the wall returns everything
					G.Score = G.Hits;
					if (G.Hits >= D->Need)
					{
						EndGame(*D, G);
					}
				}
			}
			else if (D->Type == EGameType::Duel && Choice >= 0 && Choice < 3)
			{
				G.HeroMove = Choice;
				G.LastOpp = G.OppMove;
				if (MoveBeats(Choice, G.OppMove))
				{
					G.Score += MovePower(*D, Choice);
					SetBeat(G, EGameBeat::TurnWon);
				}
				else if (MoveBeats(G.OppMove, Choice))
				{
					G.OppScore += MovePower(*D, G.OppMove);
					SetBeat(G, EGameBeat::TurnLost);
				}
				else
				{
					SetBeat(G, EGameBeat::TurnEven);
				}
				++G.Round;
				if (G.Round >= std::max(1, D->Rounds))
				{
					EndGame(*D, G);
				}
				else
				{
					BeginRound(*D, G, DuelPause);
				}
			}
		}

		/** Developer mode: the game is won at once. */
		void WinGame(FGameState& G) const
		{
			const FGameDef* D = GameDef(G.Def);
			if (!D || G.bOver)
			{
				return;
			}
			G.Score = std::max(G.Score, std::max(D->Need, G.OppScore + 1));
			G.Hits = std::max(G.Hits, D->Need);
			G.bOver = true;
			G.Result = EGameResult::Win;
			G.Wait = G.Clock;
		}

		/** What the panel's bar shows: aim: the marker, the zone and its middle (the bull); rally: the ball (0 at the other
		 *  bat, 1 at the hero's) and the window as a stretch of its path before the hero. Duel: false (no bar). */
		bool GameBar(const FGameState& G, float& Marker, float& ZoneFrom, float& ZoneTo, float& InnerFrom, float& InnerTo) const
		{
			const FGameDef* D = GameDef(G.Def);
			Marker = ZoneFrom = ZoneTo = InnerFrom = InnerTo = 0.f;
			if (!D || D->Type == EGameType::Duel)
			{
				return false;
			}
			if (D->Type == EGameType::Aim)
			{
				Marker = (float)G.Marker;
				ZoneFrom = (float)std::max(0.0, G.ZoneAt - D->Zone * 0.5);
				ZoneTo = (float)std::min(1.0, G.ZoneAt + D->Zone * 0.5);
				InnerFrom = (float)std::max(0.0, G.ZoneAt - D->Zone / 6.0);
				InnerTo = (float)std::min(1.0, G.ZoneAt + D->Zone / 6.0);
				return true;
			}
			const double Along = G.Flight > 0.0 ? (G.Clock - G.HitAt) / G.Flight : 0.0;
			const double Pos = G.Clock < G.Wait ? 0.0 : (G.bAway ? 1.0 - Along : Along);
			Marker = (float)std::min(1.0, std::max(0.0, Pos));
			ZoneFrom = (float)std::max(0.0, 1.0 - (G.Window * 0.5) / std::max(0.01, G.Flight));
			ZoneTo = 1.f;
			InnerFrom = InnerTo = 1.f;
			return true;
		}

		/** The game is over (bOver): its result line turns (per thing and result), a win gives the stamp and the prize. Once. */
		FGameEnd FinishGame(FGameState& G, const FWhen& When)
		{
			FGameEnd E;
			const FGameDef* D = GameDef(G.Def);
			if (!D || !G.bOver || G.Thing < 0 || G.Thing >= (int)Things.size())
			{
				return E;
			}
			E.Result = G.Result;
			const int Slot = G.Result == EGameResult::Win ? 0 : G.Result == EGameResult::Lose ? 1 : 2;
			const FAct& Lines = D->Results[Slot].Lines.empty() ? D->Results[1] : D->Results[Slot];
			const std::string Page = Things[(size_t)G.Thing].Id + "#" + GameResultName(G.Result);
			if (CountUsable(Page, Lines, When) > 0)
			{
				E.Line = TakeLineAt(Page, Lines, When, &E.NewStamps);
			}
			const FRegular* R = RegularOf(G);
			if (R)
			{
				E.Opponent = R->Name;
				E.Says = G.Result == EGameResult::Win ? R->SaysWin : G.Result == EGameResult::Lose ? R->SaysLose : std::string();
			}
			if (G.Result == EGameResult::Win)
			{
				std::string Stamp = D->Collect;
				const size_t At = Stamp.find("{opponent}");
				if (At != std::string::npos)
				{
					Stamp = R ? Stamp.replace(At, 10, R->Id) : std::string();
				}
				GiveStamp(Stamp, &E.NewStamps);
				E.Prize = D->Prize;
			}
			const FAct* A = G.Act >= 0 && G.Act < (int)Acts.size() ? &Acts[(size_t)G.Act] : nullptr;
			E.Target = A ? (A->Target.empty() ? A->Id : A->Target) : std::string();
			G.Def = -1;     // given once
			return E;
		}

		/** A light switch's reach (cm): the thing's "switch" prop, "room" 7 m, "hall" 15 m or metres (1..30); a "what": "switch"
		 *  without it: its room; 0: not a switch. */
		static double SwitchRadiusCm(const FThing& T)
		{
			const std::string S = T.Prop("switch");
			if (S.empty())
			{
				return T.Prop("what") == "switch" ? 700.0 : 0.0;
			}
			if (S == "room")
			{
				return 700.0;
			}
			if (S == "hall")
			{
				return 1500.0;
			}
			char* End = nullptr;
			const double M = std::strtod(S.c_str(), &End);
			return End != S.c_str() && M >= 1.0 && M <= 30.0 ? M * 100.0 : 700.0;
		}

		int TodayCount(const std::string& ActId, long long Day) const
		{
			if (Day != CountDay)
			{
				return 0;
			}
			const auto It = Today.find(ActId);
			return It == Today.end() ? 0 : It->second;
		}
		int TotalUses(const std::string& ActId) const
		{
			const auto It = Uses.find(ActId);
			return It == Uses.end() ? 0 : It->second;
		}
		int LineIndex(const std::string& ThingId) const
		{
			const auto It = LineOf.find(ThingId);
			return It == LineOf.end() ? 0 : It->second;
		}
		/** Developer mode: today's counts start over. */
		void ResetToday() { Today.clear(); }
		/** A new game: nothing used, nothing read. */
		void ResetState()
		{
			Today.clear();
			Uses.clear();
			LineOf.clear();
			ReadDay.clear();
			Visited.clear();
			Stamps.clear();
			OnceShown.clear();
			BookAt.clear();
			ReadAt.clear();
			Heard.clear();
			Done.clear();
			CountDay = 0;
		}

		/** The state as a flat map for the save: "day", "t:<act>", "u:<act>", "l:<thing>", "d:<thing>", "v:<plot>",
		 *  "c:<set>/<piece>" (a stamp), "o:<page>|<line>" (a once line shown), "b:<text>" (the page it was left on), "r:<text>"
		 *  (read to the end; the value is the plot), "h:<topic>" (a staff talk topic heard), "w:<target>" (done: "did" lines). */
		void SaveState(std::map<std::string, std::string>& Out) const
		{
			Out.clear();
			Out["day"] = std::to_string(CountDay);
			for (const auto& P : Today)
			{
				Out["t:" + P.first] = std::to_string(P.second);
			}
			for (const auto& P : Uses)
			{
				Out["u:" + P.first] = std::to_string(P.second);
			}
			for (const auto& P : LineOf)
			{
				if (P.second != 0)
				{
					Out["l:" + P.first] = std::to_string(P.second);
				}
			}
			for (const auto& P : ReadDay)
			{
				Out["d:" + P.first] = std::to_string(P.second);
			}
			for (const auto& P : Visited)
			{
				Out["v:" + P.first] = std::to_string(P.second);
			}
			for (const auto& P : Stamps)
			{
				for (const std::string& Piece : P.second)
				{
					Out["c:" + P.first + "/" + Piece] = "1";
				}
			}
			for (const std::string& Shown : OnceShown)
			{
				Out["o:" + Shown] = "1";
			}
			for (const auto& P : BookAt)
			{
				Out["b:" + P.first] = std::to_string(P.second);
			}
			for (const auto& P : ReadAt)
			{
				Out["r:" + P.first] = P.second;
			}
			for (const std::string& T : Heard)
			{
				Out["h:" + T] = "1";
			}
			for (const std::string& T : Done)
			{
				Out["w:" + T] = "1";
			}
		}
		void LoadState(const std::map<std::string, std::string>& In)
		{
			ResetState();
			for (const auto& P : In)
			{
				const long long V = std::strtoll(P.second.c_str(), nullptr, 10);
				if (P.first == "day")
				{
					CountDay = V;
				}
				else if (P.first.size() > 2 && P.first[1] == ':')
				{
					const std::string Name = P.first.substr(2);
					switch (P.first[0])
					{
					case 't': Today[Name] = (int)V; break;
					case 'u': Uses[Name] = (int)V; break;
					case 'l': LineOf[Name] = (int)V; break;
					case 'd': ReadDay[Name] = V; break;
					case 'v': Visited[Name] = V; break;
					case 'o': OnceShown.insert(Name); break;
					case 'b': BookAt[Name] = (int)V; break;
					case 'r': ReadAt[Name] = P.second; break;
					case 'h': Heard.insert(Name); break;
					case 'w': Done.insert(Name); break;
					case 'c':
					{
						// kept even when the data no longer declares it (a piece may come back with the content)
						std::string Set, Piece;
						if (SplitStamp(Name, Set, Piece))
						{
							Stamps[Set].insert(Piece);
						}
						break;
					}
					default: break;
					}
				}
			}
			// a save from before "w:": the acts used stand for what was done (a staff copy "buy_x@seller" or a seated one is "buy_x")
			for (const auto& P : Uses)
			{
				std::string Target = P.first.substr(0, P.first.find('@'));
				if (Target.size() > 7 && Target.compare(Target.size() - 7, 7, "_seated") == 0)
				{
					Target.resize(Target.size() - 7);
				}
				Done.insert(Target);
			}
		}

	private:
		std::unordered_map<std::string, int> ActIndex;
		std::unordered_map<std::string, int> ThingIndex;
		std::unordered_map<std::string, int> TextIndex;
		std::unordered_map<long long, std::vector<int>> Grid;
		std::unordered_map<std::string, std::vector<int>> PlotThings;
		/** Who holds each thing (0 nobody): a townsperson sitting on it (not saved). */
		std::vector<uint64_t> HeldBy;
		FVec BoxMin, BoxMax;
		/** The game day Today counts (a new day clears it). */
		long long CountDay = 0;
		std::map<std::string, int> Today;
		std::map<std::string, int> Uses;
		std::map<std::string, int> LineOf;
		/** A daily act's thing: the game day (+1) its line was last shown. */
		std::map<std::string, long long> ReadDay;
		std::map<std::string, long long> Visited;
		FStamps Stamps;
		/** "once" lines shown: "<page id>|<line key>". */
		std::set<std::string> OnceShown;
		/** The reading view: the page each text was left on, and the texts read to the end (text id -> the plot). */
		std::map<std::string, int> BookAt;
		std::map<std::string, std::string> ReadAt;
		/** The staff talk's topics heard (ids), and the targets the hero has done (acts' targets: "did"). */
		std::set<std::string> Heard;
		/** Load only: things whose "floors" wait for every thing to be read. */
		std::vector<std::pair<int, const LyudJson::FValue*>> LiftPending;
		std::set<std::string> Done;
		std::unordered_map<std::string, int> TopicIndex;
		std::unordered_map<std::string, int> ToneIndex;
		/** plot id -> index into Tones. */
		std::unordered_map<std::string, int> PlotTone;

		static double Dist(const FVec& A, const FVec& B)
		{
			const double DX = A.X - B.X, DY = A.Y - B.Y, DZ = A.Z - B.Z;
			return std::sqrt(DX * DX + DY * DY + DZ * DZ);
		}
		static long long CellOf(double V) { return (long long)std::floor(V / CellCm); }
		static long long Key(long long GX, long long GY) { return (GX + (1LL << 30)) * (1LL << 31) + (GY + (1LL << 30)); }

		static std::string ScalarText(const LyudJson::FValue& V)
		{
			switch (V.Type)
			{
			case LyudJson::FValue::String: return V.Str;
			case LyudJson::FValue::Bool: return V.B ? "1" : "0";
			case LyudJson::FValue::Number:
			{
				char Buf[32];
				std::snprintf(Buf, sizeof(Buf), "%g", V.Num);
				return Buf;
			}
			default: return std::string();
			}
		}

		void RollDay(long long Day)
		{
			if (Day != CountDay)
			{
				Today.clear();
				CountDay = Day;
			}
		}

		/** The first line at or after From (going round) that may be shown now; -1 none. */
		int FirstHolding(const std::string& PageId, const FAct& A, int From, const FWhen& When) const
		{
			const int N = (int)A.Lines.size();
			for (int Step = 0; Step < N; ++Step)
			{
				const int I = (((From + Step) % N) + N) % N;
				if (Usable(PageId, A.Lines[(size_t)I], When))
				{
					return I;
				}
			}
			return -1;
		}

		/** Which of an act's texts the view opens: one a game day (bReadDaily: by the days since the game began), else the next in
		 *  turn at every use (kept per thing as "<page id>#text" with the pages). */
		int PickText(const std::string& PageId, const FAct& A, const FWhen& When)
		{
			const int N = (int)A.Texts.size();
			if (N == 0)
			{
				return -1;
			}
			if (A.bReadDaily)
			{
				return A.Texts[(size_t)(((When.Since % N) + N) % N)];
			}
			int& Turn = LineOf[PageId + "#text"];
			const int Pick = ((Turn % N) + N) % N;
			Turn = (Pick + 1) % N;
			return A.Texts[(size_t)Pick];
		}

		/** A text of "texts": {"title", "kind", "minutes", "collect", "pages": [page]}, a page a key, a line {"k", conditions} or a
		 *  list of them (the alternatives). */
		static FReadText ReadTextDef(const std::string& Id, const LyudJson::FValue& V)
		{
			FReadText T;
			T.Id = Id;
			T.Title = V["title"].S();
			T.Kind = V["kind"].S("book");
			T.Minutes = std::max(0, std::min(30, V["minutes"].I(2)));
			T.Collect = V["collect"].S();
			auto One = [](const LyudJson::FValue& E, std::vector<FLine>& Into)
			{
				FLine Line;
				if (E.Type == LyudJson::FValue::String)
				{
					Line.Key = E.Str;
				}
				else if (E.Type == LyudJson::FValue::Object)
				{
					ReadLine(E, Line);
				}
				if (!Line.Key.empty())
				{
					Into.push_back(Line);
				}
			};
			for (const LyudJson::FValue& P : V["pages"].Items)
			{
				std::vector<FLine> Page;
				if (P.Type == LyudJson::FValue::Array)
				{
					for (const LyudJson::FValue& E : P.Items)
					{
						One(E, Page);
					}
				}
				else
				{
					One(P, Page);
				}
				if (!Page.empty())
				{
					T.Pages.push_back(std::move(Page));
				}
			}
			return T;
		}

		/** An act's stamp "set/{line}": every line it shows gives a piece of its own (the line's key). */
		static bool IsLineStamp(const std::string& Collect)
		{
			return Collect.size() > 7 && Collect.compare(Collect.size() - 7, 7, "/{line}") == 0;
		}

		/** A festival line of a daily act that holds now (it takes the day); -1 none. */
		int FestiveLine(const std::string& PageId, const FAct& A, const FWhen& When) const
		{
			for (size_t I = 0; I < A.Lines.size(); ++I)
			{
				if (A.Lines[I].IsFestive() && Usable(PageId, A.Lines[I], When))
				{
					return (int)I;
				}
			}
			return -1;
		}

		/** Shows a line of Thing's act and turns the page: the next one that may be shown now; a daily act keeps today's line
		 *  and goes on to the next one on the next day it is read (on a holiday its festival notice takes the day and the run
		 *  goes on after it, nothing skipped). The closed act keeps a page of its own. A line's stamp is given. Call only when
		 *  CountHolding > 0. */
		std::string TakeLine(int Thing, const FAct& A, const FWhen& When, std::vector<std::string>* NewStamps)
		{
			return TakeLineAt(PageIdOf(Thing, (int)(&A - Acts.data())), A, When, NewStamps);
		}

		/** TakeLine on the page Id (a game's result lines keep "<thing>#win" and so on). */
		std::string TakeLineAt(const std::string& Id, const FAct& A, const FWhen& When, std::vector<std::string>* NewStamps)
		{
			const int N = (int)A.Lines.size();
			const int Cur = LineIndex(Id);
			int Pos = -1;
			if (A.bDaily)
			{
				long long& Read = ReadDay[Id];
				const int Festive = FestiveLine(Id, A, When);
				if (Festive >= 0)
				{
					Pos = Festive;
					if (Read != 0)
					{
						Read = When.Day + 1;     // the run goes on tomorrow after the line shown last
					}
				}
				else
				{
					Pos = FirstHolding(Id, A, Read != 0 && Read != When.Day + 1 ? Cur + 1 : Cur, When);
					Read = When.Day + 1;
					LineOf[Id] = Pos < 0 ? Cur : Pos;
				}
			}
			else
			{
				Pos = FirstHolding(Id, A, Cur, When);
				LineOf[Id] = Pos < 0 ? Cur : (Pos + 1) % N;
			}
			if (Pos < 0)
			{
				return std::string();
			}
			if (A.Lines[(size_t)Pos].bOnce)
			{
				OnceShown.insert(Id + "|" + A.Lines[(size_t)Pos].Key);
			}
			GiveStamp(A.Lines[(size_t)Pos].Collect, NewStamps);
			if (IsLineStamp(A.Collect))
			{
				// "films/{line}": the line shown is the piece (a film of the programme, a record of the jukebox)
				GiveStamp(A.Collect.substr(0, A.Collect.size() - 6) + A.Lines[(size_t)Pos].Key, NewStamps);
			}
			return A.Lines[(size_t)Pos].Key;
		}

		// --- the games' rules

		static void SetBeat(FGameState& G, EGameBeat B)
		{
			G.Beat = B;
			++G.BeatSerial;
		}

		static int MovePower(const FGameDef& D, int Move)
		{
			return Move >= 0 && Move < (int)D.Moves.size() ? std::max(1, D.Moves[(size_t)Move].Power) : 1;
		}

		/** The aim marker at time T: to and fro over the bar, Speed runs a second, from the throw's own phase. */
		static double MarkerAt(const FGameDef& D, const FGameState& G, double T)
		{
			const double X = std::max(0.0, T - G.RoundFrom) * D.Speed + G.Phase;
			const double F = X - std::floor(X);
			return 1.0 - std::fabs(2.0 * F - 1.0);
		}

		/** The next throw, serve or turn, after a pause. */
		void BeginRound(const FGameDef& D, FGameState& G, double Pause) const
		{
			G.Wait = G.Clock + Pause;
			if (D.Type == EGameType::Aim)
			{
				const double Z = std::min(0.95, std::max(0.02, D.Zone));
				G.ZoneAt = Z * 0.5 + G.Rng.Unit() * (1.0 - Z);
				G.Phase = G.Rng.Unit();
				G.RoundFrom = G.Wait;
				G.Marker = MarkerAt(D, G, G.Wait);
			}
			else if (D.Type == EGameType::Rally)
			{
				// a serve: the ball leaves the other bat (or the wall) when the pause ends
				G.bAway = false;
				G.bOppMisses = false;
				G.Hits = 0;
				G.Flight = 1.0 / std::min(5.0, std::max(0.1, D.Speed));
				G.Window = std::max(MinWindow, D.Zone);
				G.HitAt = G.Wait;
				G.Arrive = G.HitAt + G.Flight;
			}
			else if (D.Type == EGameType::Duel)
			{
				// the opponent's move by their style, and the look that may betray it (the honest ones show it more often)
				static const double Even[3] = { 1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0 };
				const FRegular* R = RegularOf(G);
				const double* Lean = R ? R->Lean : Even;
				double Roll = G.Rng.Unit() * (Lean[0] + Lean[1] + Lean[2]);
				int Move = 0;
				while (Move < 2 && Roll >= Lean[Move])
				{
					Roll -= Lean[Move];
					++Move;
				}
				G.OppMove = Move;
				const bool bHonest = G.Rng.Unit() < (R ? R->Honesty : 0.6);
				G.Tell = bHonest ? Move : (Move + 1 + G.Rng.Below(2)) % 3;
			}
		}

		/** An aim throw (bPressed false: not made in time): a bull 2, the zone 1; the opponent throws too. */
		void Throw(const FGameDef& D, FGameState& G, bool bPressed) const
		{
			int Points = 0;
			if (bPressed)
			{
				const double Off = std::fabs(G.Marker - G.ZoneAt);
				Points = Off <= D.Zone / 6.0 ? 2 : Off <= D.Zone * 0.5 ? 1 : 0;
			}
			G.Score += Points;
			SetBeat(G, Points == 2 ? EGameBeat::Bull : Points == 1 ? EGameBeat::Hit : EGameBeat::Miss);
			if (const FRegular* R = RegularOf(G))
			{
				const double Roll = G.Rng.Unit();
				G.OppScore += Roll < 0.3 * (double)R->Skill ? 2 : Roll < 0.25 + 0.6 * (double)R->Skill ? 1 : 0;
			}
			++G.Round;
			if (G.Round >= std::max(1, D.Rounds))
			{
				EndGame(D, G);
			}
			else
			{
				BeginRound(D, G, AimPause);
			}
		}

		/** A rally's point is over: with an opponent it is counted (first to Need); alone the rally was the game. */
		void RallyPoint(const FGameDef& D, FGameState& G, bool bWon, EGameBeat B) const
		{
			SetBeat(G, B);
			++G.Round;
			if (!RegularOf(G))
			{
				G.Score = G.Hits;
				EndGame(D, G);
				return;
			}
			if (bWon)
			{
				++G.Score;
			}
			else
			{
				++G.OppScore;
			}
			if (G.Score >= D.Need || G.OppScore >= D.Need)
			{
				EndGame(D, G);
			}
			else
			{
				BeginRound(D, G, RallyPause);
			}
		}

		/** Over: against somebody the higher score wins (equal: a draw); alone Need wins and half of it is a draw (close). */
		void EndGame(const FGameDef& D, FGameState& G) const
		{
			G.bOver = true;
			G.Wait = G.Clock + EndPause;
			if (RegularOf(G) || D.Type == EGameType::Duel)
			{
				G.Result = G.Score > G.OppScore ? EGameResult::Win : G.Score < G.OppScore ? EGameResult::Lose : EGameResult::Draw;
				return;
			}
			const int Got = D.Type == EGameType::Rally ? G.Hits : G.Score;
			G.Result = Got >= D.Need ? EGameResult::Win : Got * 2 >= D.Need ? EGameResult::Draw : EGameResult::Lose;
		}

		static void ReadLines(const LyudJson::FValue& List, FAct& Into)
		{
			for (const LyudJson::FValue& L : List.Items)
			{
				FLine Line;
				if (L.Type == LyudJson::FValue::String)
				{
					Line.Key = L.Str;
				}
				else if (L.Type == LyudJson::FValue::Object)
				{
					ReadLine(L, Line);
				}
				if (!Line.Key.empty())
				{
					Into.Lines.push_back(Line);
				}
			}
		}

		/** An act's "game" (see FGameDef; interiors.py writes every field). */
		static FGameDef ReadGame(const LyudJson::FValue& V)
		{
			FGameDef D;
			D.Type = GameTypeOf(V["type"].S());
			D.Title = V["title"].S();
			D.Keys = V["keys"].S();
			D.Rounds = std::max(1, V["rounds"].I(D.Type == EGameType::Duel ? 3 : 5));
			D.Need = std::max(1, V["need"].I(D.Type == EGameType::Rally ? 3 : 5));
			D.Speed = std::min(5.0, std::max(0.1, (double)V["speed"].F(D.Type == EGameType::Rally ? 0.9f : 0.6f)));
			D.Zone = std::min(1.5, std::max(0.02, (double)V["zone"].F(D.Type == EGameType::Rally ? 0.45f : 0.2f)));
			D.Shrink = std::min(1.0, std::max(0.5, (double)V["shrink"].F(0.88f)));
			D.Pace = std::min(1.5, std::max(1.0, (double)V["pace"].F(1.05f)));
			D.Vs = V["vs"].S();
			for (const LyudJson::FValue& M : V["moves"].Items)
			{
				FGameMove Move;
				Move.Key = M["k"].S();
				Move.Tell = M["tell"].S();
				Move.Power = std::max(1, M["power"].I(1));
				D.Moves.push_back(Move);
			}
			if (D.Type == EGameType::Duel)
			{
				D.Moves.resize(3);     // a duel always has three moves (an empty key shows nothing)
			}
			static const char* const Names[3] = { "win", "lose", "draw" };
			for (int I = 0; I < 3; ++I)
			{
				D.Results[I].Id = Names[I];
				D.Results[I].Target = Names[I];
				ReadLines(V[Names[I]], D.Results[I]);
			}
			D.Collect = V["collect"].S();
			for (const LyudJson::FValue& P : V["prize"].Items)
			{
				const std::string Item = P.Type == LyudJson::FValue::Array ? P[0].S() : P.S();
				const int Count = P.Type == LyudJson::FValue::Array ? P[1].I(1) : 1;
				if (!Item.empty() && Count > 0)
				{
					D.Prize.emplace_back(Item, Count);
				}
			}
			D.Daily = std::max(0, V["daily"].I(0));
			const LyudJson::FValue& Sounds = V["sounds"];
			D.SoundHit = Sounds["hit"].S();
			D.SoundMiss = Sounds["miss"].S();
			D.SoundEnd = Sounds["end"].S();
			return D;
		}

		/** A regular: their style gives how they pick duel moves and how readable their look is (the better, the less). */
		static FRegular ReadRegular(const std::string& Id, const LyudJson::FValue& V)
		{
			FRegular R;
			R.Id = Id;
			R.Name = V["name"].S();
			R.Style = V["style"].S("steady");
			R.Skill = std::min(1.f, std::max(0.f, V["skill"].F(0.5f)));
			for (const LyudJson::FValue& P : V["plays"].Items)
			{
				R.Plays.push_back(P.S());
			}
			for (const LyudJson::FValue& P : V["plots"].Items)
			{
				R.Plots.push_back(P.S());
			}
			R.Hello = V["hello"].S();
			R.SaysWin = V["win"].S();
			R.SaysLose = V["lose"].S();
			const int Fav = R.Style == "bold" ? 0 : R.Style == "careful" ? 1 : R.Style == "tricky" ? 2 : -1;
			for (int I = 0; I < 3; ++I)
			{
				R.Lean[I] = Fav < 0 ? 1.0 / 3.0 : (I == Fav ? 0.6 : 0.2);
			}
			R.Honesty = std::min(0.85, std::max(0.35, 0.9 - 0.5 * (double)R.Skill - (R.Style == "tricky" ? 0.15 : 0.0)));
			return R;
		}

		/** How far off the line of sight a thing of this kind may be and still be looked at (cm, at the eye). */
		static double Slack(const FThing& T)
		{
			const std::string& K = T.Kind;
			if (K == "bed" || K == "counter" || K == "shelf" || K == "screen" || K == "sport" || K == "play")
			{
				return 45.0;
			}
			if (K == "take")
			{
				return 18.0;
			}
			return 30.0;
		}

		static bool Score1(const FThing& T, const FVec& Eye, const FVec& Dir, double Reach, double& Score)
		{
			const FVec D{ T.At.X - Eye.X, T.At.Y - Eye.Y, T.At.Z - Eye.Z };
			const double Len = std::sqrt(D.X * D.X + D.Y * D.Y + D.Z * D.Z);
			if (Len > Reach || std::fabs(D.Z) > 220.0)
			{
				return false;
			}
			const double Along = D.X * Dir.X + D.Y * Dir.Y + D.Z * Dir.Z;
			if (Along <= 1.0 && Len > 30.0)
			{
				return false;     // behind or beside the eye
			}
			const double LX = D.X - Along * Dir.X, LY = D.Y - Along * Dir.Y, LZ = D.Z - Along * Dir.Z;
			const double Lateral = std::sqrt(LX * LX + LY * LY + LZ * LZ);
			const double Allowed = Slack(T) + 0.22 * (Along > 0.0 ? Along : 0.0);
			if (Lateral > Allowed)
			{
				return false;
			}
			Score = Lateral / Allowed + 0.35 * Len / Reach;
			return true;
		}

		void BuildGrid()
		{
			Grid.clear();
			for (int I = 0; I < (int)Things.size(); ++I)
			{
				const FVec& P = Things[I].At;
				Grid[Key(CellOf(P.X), CellOf(P.Y))].push_back(I);
				if (I == 0)
				{
					BoxMin = BoxMax = P;
				}
				BoxMin.X = std::min(BoxMin.X, P.X);
				BoxMin.Y = std::min(BoxMin.Y, P.Y);
				BoxMin.Z = std::min(BoxMin.Z, P.Z);
				BoxMax.X = std::max(BoxMax.X, P.X);
				BoxMax.Y = std::max(BoxMax.Y, P.Y);
				BoxMax.Z = std::max(BoxMax.Z, P.Z);
			}
		}

		/** A line with its conditions: {"k", "h", "d", "date", "after", "every"} (see FLine). */
		static void ReadLine(const LyudJson::FValue& L, FLine& Line)
		{
			Line.Key = L["k"].S();
			const LyudJson::FValue& H = L["h"];
			if (H.Type == LyudJson::FValue::Array && H.Size() >= 2)
			{
				Line.HourFrom = H[0].F(0.f);
				Line.HourTo = H[1].F(24.f);
			}
			if (L["d"].Type == LyudJson::FValue::String)
			{
				Line.Days = ParseDays(L["d"].S());
			}
			const LyudJson::FValue& Date = L["date"];
			if (Date.Type == LyudJson::FValue::String)
			{
				Line.DateFrom = Line.DateTo = ParseMonthDay(Date.Str);
			}
			else if (Date.Type == LyudJson::FValue::Array && Date.Size() >= 2)
			{
				Line.DateFrom = ParseMonthDay(Date[0].S());
				Line.DateTo = ParseMonthDay(Date[1].S());
				if (Line.DateFrom < 0 || Line.DateTo < 0)
				{
					Line.DateFrom = Line.DateTo = -1;
				}
			}
			Line.After = L["after"].I(0);
			Line.Before = L["before"].I(0);
			Line.bOnce = L["once"].Type == LyudJson::FValue::Bool ? L["once"].B : L["once"].I(0) != 0;
			const LyudJson::FValue& Every = L["every"];
			if (Every.Type == LyudJson::FValue::Array && Every.Size() >= 1)
			{
				Line.Every = Every[0].I(0);
				Line.EveryAt = Every[1].I(0);
			}
			else
			{
				Line.Every = Every.I(0);
			}
			if (Line.Every > 1)
			{
				Line.EveryAt = ((Line.EveryAt % Line.Every) + Line.Every) % Line.Every;
			}
			const LyudJson::FValue& Fest = L["festival"];
			if (Fest.Type == LyudJson::FValue::String)
			{
				Line.Festival = Fest.Str == "any" ? EFestivalCond::Any : Fest.Str == "none" ? EFestivalCond::NotToday : EFestivalCond::Ids;
				if (Line.Festival == EFestivalCond::Ids)
				{
					Line.FestivalIds.push_back(Fest.Str);
				}
			}
			else if (Fest.Type == LyudJson::FValue::Array && Fest.Size() > 0)
			{
				Line.Festival = EFestivalCond::Ids;
				for (const LyudJson::FValue& F : Fest.Items)
				{
					Line.FestivalIds.push_back(F.S());
				}
			}
			const LyudJson::FValue& Wx = L["weather"];
			if (Wx.Type == LyudJson::FValue::String)
			{
				Line.Weather = WeatherBit(Wx.Str);
			}
			for (const LyudJson::FValue& Word : Wx.Items)
			{
				Line.Weather = (uint8_t)(Line.Weather | WeatherBit(Word.S()));
			}
			if (Wx.Type != LyudJson::FValue::Null && Line.Weather == 0)
			{
				Line.Days = 0;     // only unknown words: never shown (not "no condition": a line for the snow must not show in July)
			}
			Line.Have = L["have"].S();
			const LyudJson::FValue& Count = L["count"];
			if (Count.Type == LyudJson::FValue::Array && Count.Size() >= 2)
			{
				Line.CountSet = Count[0].S();
				Line.CountMin = Count[1].I(0);
			}
			Line.Collect = L["collect"].S();
			Line.ReadText = L["read"].S();
			Line.Did = L["did"].S();
			Line.Heard = L["heard"].S();
		}

		/** A topic of "topics": {"k": title key, "lines": [line], <line conditions: when it is offered>, "once": true (offered until
		 *  heard), "collect": "set/piece" (heard the first time)}. */
		static FTopic ReadTopic(const std::string& Id, const LyudJson::FValue& V)
		{
			FTopic T;
			T.Id = Id;
			T.Title = V["k"].S();
			ReadLine(V, T.When);
			T.When.Key.clear();
			T.When.bOnce = false;
			T.When.Collect.clear();
			T.bOnce = V["once"].Type == LyudJson::FValue::Bool ? V["once"].B : V["once"].I(0) != 0;
			T.Collect = V["collect"].S();
			for (const LyudJson::FValue& L : V["lines"].Items)
			{
				FLine Line;
				if (L.Type == LyudJson::FValue::String)
				{
					Line.Key = L.Str;
				}
				else if (L.Type == LyudJson::FValue::Object)
				{
					ReadLine(L, Line);
				}
				if (!Line.Key.empty())
				{
					T.Lines.push_back(Line);
				}
			}
			return T;
		}

		/** A list of topic ids (an act's or a thing's "topics"): those the file has, each once. */
		void ReadTopicList(const LyudJson::FValue& List, std::vector<int>& Into) const
		{
			for (const LyudJson::FValue& Id : List.Items)
			{
				const int Found = FindTopic(Id.S());
				if (Found >= 0 && std::find(Into.begin(), Into.end(), Found) == Into.end())
				{
					Into.push_back(Found);
				}
			}
		}

		/** "tones": {"<id>": {"loop", "asset", "volume", "shots": [ids], "every": [min, max] s, "shot_volume", "closed": tone id}},
		 *  "plot_tones": {"<plot>": "<tone id>"}. Volumes are clamped to 0..1, the pause between one-shots to 2..600 s. */
		void ReadTones(const LyudJson::FValue& Defs, const LyudJson::FValue& ByPlot)
		{
			std::vector<std::string> ClosedIds;
			for (const auto& Field : Defs.Fields)
			{
				FTone T;
				T.Id = Field.first;
				const LyudJson::FValue& V = Field.second;
				T.Loop = V["loop"].S();
				T.Asset = V["asset"].S();
				T.Volume = std::min(1.f, std::max(0.f, V["volume"].F(0.3f)));
				for (const LyudJson::FValue& S : V["shots"].Items)
				{
					if (!S.S().empty())
					{
						T.Shots.push_back(S.S());
					}
				}
				const LyudJson::FValue& Every = V["every"];
				T.EveryMin = std::min(600.f, std::max(2.f, Every[0].F(12.f)));
				T.EveryMax = std::min(600.f, std::max(T.EveryMin, Every[1].F(T.EveryMin)));
				T.ShotVolume = std::min(1.f, std::max(0.f, V["shot_volume"].F(0.5f)));
				if (ToneIndex.count(T.Id) == 0)
				{
					ToneIndex[T.Id] = (int)Tones.size();
					ClosedIds.push_back(V["closed"].S());
					Tones.push_back(std::move(T));
				}
			}
			for (size_t I = 0; I < Tones.size(); ++I)
			{
				const int C = ClosedIds[I].empty() ? -1 : FindTone(ClosedIds[I]);
				Tones[I].Closed = C == (int)I ? -1 : C;
			}
			for (const auto& Field : ByPlot.Fields)
			{
				const int Tone = FindTone(Field.second.S());
				if (Tone >= 0)
				{
					PlotTone[Field.first] = Tone;
				}
			}
		}

		static FAct ReadAct(const std::string& Id, const LyudJson::FValue& V)
		{
			FAct A;
			A.Id = Id;
			A.Prompt = V["prompt"].S("city.act.use.prompt");
			A.Title = V["title"].S();
			for (const LyudJson::FValue& L : V["lines"].Items)
			{
				FLine Line;
				if (L.Type == LyudJson::FValue::String)
				{
					Line.Key = L.Str;
				}
				else if (L.Type == LyudJson::FValue::Object)
				{
					ReadLine(L, Line);
				}
				if (!Line.Key.empty())
				{
					A.Lines.push_back(Line);
				}
			}
			A.bDaily = V["rotate"].S() == "daily";
			A.Minutes = V["minutes"].I(0);
			A.Until = V["until"].I(-1);
			if (A.Until > 23)
			{
				A.Until = -1;
			}
			A.Price = V["price"].I(0);
			for (const LyudJson::FValue& G : V["give"].Items)
			{
				// [["item", n]], ["item"] or [{"item": .., "count": ..}]
				std::string Item;
				int Count = 1;
				if (G.Type == LyudJson::FValue::Array)
				{
					Item = G[0].S();
					Count = G[1].I(1);
				}
				else if (G.Type == LyudJson::FValue::Object)
				{
					Item = G["item"].S();
					Count = G["count"].I(1);
				}
				else
				{
					Item = G.S();
				}
				if (!Item.empty() && Count > 0)
				{
					A.Give.emplace_back(Item, Count);
				}
			}
			A.NeedsItem = V["needs_item"].S();
			A.NeedsStaff = V["needs_staff"].S();
			A.Pose = V["pose"].S("stand");
			if (A.Pose.empty())
			{
				A.Pose = "stand";
			}
			A.Sound = V["sound"].S();
			A.Event = V["event"].S();
			A.Target = V["target"].S();
			A.Daily = V["daily"].I(0);
			A.Shop = V["shop"].S();
			A.Screen = V["screen"].S();
			A.bFade = V["fade"].Type == LyudJson::FValue::Bool ? V["fade"].B : V["fade"].I(0) != 0;
			A.bAnyHour = V["any_hour"].Type == LyudJson::FValue::Bool ? V["any_hour"].B : (A.Pose == "sit" || A.Pose == "lie");
			A.Collect = V["collect"].S();
			A.bSleep = V["sleep"].Type == LyudJson::FValue::Bool ? V["sleep"].B : V["sleep"].I(0) != 0;
			return A;
		}

		/** The default act of a thing's kind (made once per kind, per item for "take", per role for "counter"). */
		int DefaultAct(const FThing& T)
		{
			std::string Id = "_" + T.Kind;
			const std::string Item = T.Prop("item");
			const std::string Role = T.Prop("role");
			if (T.Kind == "take" && !Item.empty())
			{
				Id += ":" + Item;
			}
			if (T.Kind == "counter" && !Role.empty())
			{
				Id += ":" + Role;
			}
			const auto Found = ActIndex.find(Id);
			if (Found != ActIndex.end())
			{
				return Found->second;
			}
			FAct A;
			A.Id = Id;
			const std::string& K = T.Kind;
			auto Lines = [&A](const std::string& Base, int N)
			{
				for (int I = 1; I <= N; ++I)
				{
					FLine Line;
					Line.Key = Base + "." + std::to_string(I);
					A.Lines.push_back(Line);
				}
			};
			if (K == "seat")
			{
				A.Prompt = "city.act.sit.prompt";
				A.Pose = "sit";
				A.Event = "sit";
				A.bAnyHour = true;
			}
			else if (K == "bed")
			{
				A.Prompt = "city.act.bed.prompt";
				A.Pose = "lie";
				A.Minutes = (int)(T.PropF("rest_hours", 2.f) * 60.f);
				A.bFade = true;
				A.Event = "rest";
				A.bAnyHour = true;
				Lines("city.act.bed", 2);
			}
			else if (K == "shelf")
			{
				A.Prompt = "city.act.shelf.prompt";
				A.Minutes = 10;
				A.Sound = "diary_page";
				A.Event = "read";
				A.Target = T.Prop("topic");
				Lines("city.act.shelf", 3);
			}
			else if (K == "board")
			{
				A.Prompt = "city.act.board.prompt";
				A.Event = "look";
				A.Target = T.Prop("topic");
				Lines("city.act.board", 3);
			}
			else if (K == "counter")
			{
				A.Prompt = "city.act.counter.prompt";
				A.NeedsStaff = Role;
				A.Event = "talk";
				A.Target = Role;
				Lines("city.act.counter", 2);
			}
			else if (K == "machine")
			{
				A.Prompt = "city.act.machine.prompt";
				A.Minutes = 5;
				A.Event = "machine";
				A.Target = T.Prop("what");
				Lines("city.act.machine", 2);
			}
			else if (K == "play")
			{
				A.Prompt = "city.act.play.prompt";
				A.Minutes = 20;
				A.Event = "play";
				A.Target = T.Prop("what");
				Lines("city.act.play", 2);
			}
			else if (K == "sport")
			{
				A.Prompt = "city.act.sport.prompt";
				A.Minutes = 15;
				A.Event = "sport";
				A.Target = T.Prop("what");
				Lines("city.act.sport", 2);
			}
			else if (K == "craft")
			{
				A.Prompt = "city.act.craft.prompt";
				A.Minutes = 30;
				A.Event = "craft";
				A.Target = T.Prop("what");
				Lines("city.act.craft", 2);
			}
			else if (K == "screen")
			{
				A.Prompt = "city.act.screen.prompt";
				A.Minutes = 30;
				A.bFade = true;
				A.Event = "watch";
				A.Target = T.Prop("what");
				Lines("city.act.screen", 2);
			}
			else if (K == "take")
			{
				A.Prompt = "city.act.take.prompt";
				A.Daily = 1;
				A.Event = "take";
				A.Target = Item;
				if (!Item.empty())
				{
					A.Give.emplace_back(Item, 1);
				}
				Lines("city.act.take", 1);
			}
			else if (K == "door")
			{
				A.Prompt = "city.act.door.prompt";
				A.Event = "look";
				A.Target = T.Prop("to");
				A.bAnyHour = true;
				Lines("city.act.door", 1);
			}
			else
			{
				A.Prompt = "city.act.use.prompt";
				Lines("city.act.board", 3);
			}
			if (A.Target.empty())
			{
				A.Target = K;
			}
			const int Index = (int)Acts.size();
			ActIndex[Id] = Index;
			Acts.push_back(std::move(A));
			return Index;
		}
	};
}
