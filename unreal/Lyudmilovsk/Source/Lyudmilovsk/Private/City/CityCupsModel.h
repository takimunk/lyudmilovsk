#pragma once

// The clubs' weekly tournaments (docs/city_interiors.md "Tournaments"): plain C++ on top of LyudCity::FModel, no engine types,
// so tools/citysim runs a year of them offline. Data: Content/City/interiors.json "tournaments" (tools/city/interiors.py
// writes it from tools/city/content/x_tournaments.json).
//
// A tournament is a knock-out of eight on its weekday at one plot: three rounds (quarter-finals, semi-finals, the final), each
// Step hours long from Start. The field is drawn from the regulars who play its game (the ten of the clubs and the guests of
// x_tournaments.json), seated by a hash of the day, so a day's bracket is the same whenever it is asked. Matches between
// regulars are settled by their skill and a die of the day; the hero (signed up on the bracket board's sheet before the start)
// plays his own matches for real: in his round's hours, at a table of the plot with the tournament's game, his opponent is
// the bracket's. A draw is replayed; a round not played by its end is lost (no-show). The champion's cup is a stamp
// (Collect, "cups/<id>"), shown on the board's cup case and printed in the paper the next morning.
//
// Saved in the model's flat map under "k:" keys (the model's own LoadState skips them): "k:s:<cup>@<day>" signed up,
// "k:r:<cup>@<day>" the hero's results in order ('W' won, 'L' lost), "k:f:<cup>@<day>" that day was a holiday's cup.
//
// A holiday's cup (docs/festivals.md "The new toys"): when the tournament's weekday falls on a holiday (FWhen::bFestival), the
// board is headed with FestTitle and says so, and the champion takes a second stamp (FestCollect) beside the weekly one.
// Only a holiday that is not a public day off: on a day off (IsHoliday) the town is at the festival and there is no cup at all.

#include "CityThingsModel.h"

namespace LyudCity
{
	struct FCupDef
	{
		std::string Id;
		/** Name key (city.cup.<id>.title) and the venue's name key. */
		std::string Title;
		std::string Venue;
		/** The game ("vs" of the tables' game: chess, dominoes, table_tennis) and the plot it is played at. */
		std::string Game;
		std::string Plot;
		/** Monday = 0 .. Sunday = 6; the first round's hour; a round's length (hours). */
		int Weekday = 5;
		float Start = 13.f;
		float Step = 1.5f;
		/** The champion's stamp ("cups/<id>"; empty: none). */
		std::string Collect;
		/** On a holiday: the board's heading (empty: the usual one) and the champion's second stamp (empty: none). */
		std::string FestTitle;
		std::string FestCollect;
		/** The regulars who may be drawn (indices into FModel::Regulars, in id order): bound by FCups::Bind. */
		std::vector<int> Pool;
	};

	/** One line of a generated page: a key and its arguments ('@' before an argument: it is a key itself). */
	struct FCupLine
	{
		std::string Key;
		std::vector<std::string> Args;
	};
	using FCupPage = std::vector<FCupLine>;

	/** A tournament day's bracket as it stands at some moment. */
	struct FBracket
	{
		static constexpr int Seats = 8;
		static constexpr int Matches = 7;
		static constexpr int Rounds = 3;
		static constexpr int HeroSeat = -1;
		int Cup = -1;
		long long Day = -1;
		bool bHero = false;
		/** Who sits at each seat: a regular's index, or HeroSeat. */
		int Seat[Seats] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		/** Each match's two seats (-1: not known yet) and the winner's seat (-1: not played yet). Matches 0..3 the quarter-finals,
		 *  4..5 the semi-finals, 6 the final. */
		int Side[Matches][2] = {};
		int Winner[Matches] = { -1, -1, -1, -1, -1, -1, -1 };
		/** The hero did not come (the match given to the other side). */
		bool bNoShow[Matches] = {};
		/** How far the hero got: -1 not in it; 0..2 out in that round (or playing it, see bHeroAlive); 3 champion. */
		int HeroReached = -1;
		bool bHeroAlive = false;

		static int RoundOf(int Match) { return Match < 4 ? 0 : Match < 6 ? 1 : 2; }
		static int FirstMatch(int Round) { return Round <= 0 ? 0 : Round == 1 ? 4 : 6; }
		/** The match the winner of Match plays next (-1: the final). */
		static int NextMatch(int Match) { return Match < 4 ? 4 + Match / 2 : Match < 6 ? 6 : -1; }
		int Champion() const { return Winner[6] >= 0 ? Seat[Winner[6]] : -2; }
		bool IsDone() const { return Winner[6] >= 0; }
	};

	/** What a match the hero just played did (FCups::Record). */
	enum class ECupNote : uint8_t
	{
		None,
		/** A draw: the match is played again. */
		Replay,
		/** Won: the next round starts at NextHour. */
		Next,
		/** Lost: out of the tournament. */
		Out,
		/** Won the final. */
		Champion,
	};

	struct FCupResult
	{
		ECupNote Note = ECupNote::None;
		int Round = -1;
		float NextHour = 0.f;
		/** The champion won a holiday's cup. */
		bool bFestival = false;
		std::vector<std::string> NewStamps;
	};

	/** What the paper prints the morning after a tournament day. */
	struct FCupNews
	{
		int Cup = -1;
		/** The champion's name key; empty: the hero. */
		std::string Winner;
		/** The runner-up's name key (empty: the hero). */
		std::string RunnerUp;
		/** -1 not in it, 0..2 out in that round, 3 champion. */
		int HeroReached = -1;
	};

	class FCups
	{
	public:
		std::vector<FCupDef> Defs;

		/** Reads "tournaments" of interiors.json (the regulars are bound by Bind). */
		bool Load(const std::string& Text, std::string& Error)
		{
			Defs.clear();
			LyudJson::FValue Root;
			if (!LyudJson::Parse(Text, Root, Error))
			{
				return false;
			}
			for (const auto& Field : Root["tournaments"].Fields)
			{
				const LyudJson::FValue& V = Field.second;
				FCupDef D;
				D.Id = Field.first;
				D.Title = V["title"].S();
				D.Venue = V["venue"].S();
				D.Game = V["game"].S();
				D.Plot = V["plot"].S();
				D.Weekday = std::min(6, std::max(0, V["weekday"].I(5)));
				D.Start = V["start"].F(13.f);
				D.Step = std::max(0.25f, V["step"].F(1.5f));
				D.Collect = V["collect"].S();
				D.FestTitle = V["fest_title"].S();
				D.FestCollect = V["fest_collect"].S();
				if (!D.Id.empty() && !D.Game.empty() && !D.Plot.empty())
				{
					Defs.push_back(std::move(D));
				}
			}
			return true;
		}

		/** The pools from the model's regulars (every one who plays the game). False when a cup has fewer than eight. */
		bool Bind(const FModel& M)
		{
			bool bOk = true;
			for (FCupDef& D : Defs)
			{
				D.Pool.clear();
				for (int I = 0; I < (int)M.Regulars.size(); ++I)
				{
					const std::vector<std::string>& Plays = M.Regulars[(size_t)I].Plays;
					if (std::find(Plays.begin(), Plays.end(), D.Game) != Plays.end())
					{
						D.Pool.push_back(I);
					}
				}
				bOk = bOk && (int)D.Pool.size() >= FBracket::Seats;
			}
			return bOk;
		}

		int Find(const std::string& Id) const
		{
			for (int I = 0; I < (int)Defs.size(); ++I)
			{
				if (Defs[(size_t)I].Id == Id)
				{
					return I;
				}
			}
			return -1;
		}
		const FCupDef* Def(int Cup) const { return Cup >= 0 && Cup < (int)Defs.size() ? &Defs[(size_t)Cup] : nullptr; }
		bool IsUsable(int Cup) const { const FCupDef* D = Def(Cup); return D && (int)D->Pool.size() >= FBracket::Seats; }

		/** Is a day (since the game began) a public holiday? The game asks its festivals (ULyudFestivalSubsystem: a festival's
		 *  day off, Big Sun Day); unset: none. On such a day no cup is played: the town is at the festival, the regulars too
		 *  (tools/togethersim found the Saturday chess cup's players watching the zenith on the square). */
		std::function<bool(long long /*Day*/)> IsHoliday;
		bool IsOff(long long Day) const { return Day >= 0 && IsHoliday && IsHoliday(Day); }
		/** The weekday's tournament day of this week (today, or the coming one) when it falls on a holiday; -1 when not. */
		long long SkippedDay(int Cup, const FWhen& Now) const
		{
			const FCupDef* D = Def(Cup);
			if (!D)
			{
				return -1;
			}
			const int Ahead = ((D->Weekday - Now.Weekday) % 7 + 7) % 7;
			const long long Day = Now.Since + Ahead;
			return IsOff(Day) ? Day : -1;
		}

		/** The weekday of a day (since the game began), from today's. */
		static int WeekdayOf(long long Day, const FWhen& Now)
		{
			return (int)(((Now.Weekday + (Day - Now.Since)) % 7 + 7) % 7);
		}
		/** A round's hours on its day: [from, to). */
		float RoundFrom(int Cup, int Round) const { const FCupDef* D = Def(Cup); return D ? D->Start + D->Step * (float)Round : 0.f; }
		float RoundTo(int Cup, int Round) const { return RoundFrom(Cup, Round + 1); }

		/** The tournament day the sheet signs up for now: today while the first round has not begun, else the next one. */
		long long NextDay(int Cup, const FWhen& Now) const
		{
			const FCupDef* D = Def(Cup);
			if (!D)
			{
				return -1;
			}
			const int Ahead = ((D->Weekday - Now.Weekday) % 7 + 7) % 7;
			long long Day = Now.Since + (Ahead == 0 && Now.Hour >= D->Start ? 7 : Ahead);
			for (int Guard = 0; Guard < 8 && IsOff(Day); ++Guard)
			{
				Day += 7;     // a holiday: no cup that week, the sheet is for the next one
			}
			return Day;
		}
		/** The tournament day whose bracket the board shows: today's from its start, else the last one (-1: none yet). */
		long long ShownDay(int Cup, const FWhen& Now) const
		{
			const FCupDef* D = Def(Cup);
			if (!D)
			{
				return -1;
			}
			const int Back = ((Now.Weekday - D->Weekday) % 7 + 7) % 7;
			long long Day = Now.Since - (Back == 0 && Now.Hour < D->Start ? 7 : Back);
			for (int Guard = 0; Guard < 8 && IsOff(Day); ++Guard)
			{
				Day -= 7;     // a holiday was played by nobody: the last cup before it
			}
			return Day >= 0 ? Day : -1;
		}
		/** Was (or is) that tournament day a holiday's cup: remembered once a match of it was recorded on a holiday, and today
		 *  while Now says a holiday is on. */
		bool IsFestDay(int Cup, long long Day, const FWhen* Now = nullptr) const
		{
			const FCupDef* D = Def(Cup);
			if (!D || (D->FestTitle.empty() && D->FestCollect.empty()) || IsOff(Day))
			{
				return false;
			}
			return (Now && Now->bFestival && Day == Now->Since) || FestDays.count(Key(Cup, Day)) > 0;
		}
		/** The name key the tournament goes by on that day. */
		const std::string& TitleOn(int Cup, long long Day, const FWhen* Now = nullptr) const
		{
			static const std::string None;
			const FCupDef* D = Def(Cup);
			return !D ? None : (IsFestDay(Cup, Day, Now) && !D->FestTitle.empty() ? D->FestTitle : D->Title);
		}
		bool IsSigned(int Cup, long long Day) const { return Def(Cup) && Signed.count(Key(Cup, Day)) > 0; }
		/** The hero writes his name on the sheet (for NextDay). False: signed already (or no such cup). */
		bool SignUp(int Cup, const FWhen& Now, long long* Day = nullptr)
		{
			const long long D = NextDay(Cup, Now);
			if (Day)
			{
				*Day = D;
			}
			return IsUsable(Cup) && D >= 0 && Signed.insert(Key(Cup, D)).second;
		}

		/** The bracket of a tournament day as it stands at Now (a past day: as it ended). */
		FBracket Bracket(const FModel& M, int Cup, long long Day, const FWhen& Now) const
		{
			FBracket B;
			B.Cup = Cup;
			B.Day = Day;
			const FCupDef* D = Def(Cup);
			if (!D || !IsUsable(Cup))
			{
				return B;
			}
			B.bHero = IsSigned(Cup, Day);
			// the field: the pool shuffled by the day, the first eight (seven and the hero), then the seats shuffled
			FRng Rng;
			Rng.State = HashText(D->Id) ^ ((uint64_t)(Day + 1) * 0x9E3779B97F4A7C15ull);
			std::vector<int> Pool = D->Pool;
			for (int I = (int)Pool.size() - 1; I > 0; --I)
			{
				std::swap(Pool[(size_t)I], Pool[(size_t)Rng.Below(I + 1)]);
			}
			const int Take = B.bHero ? FBracket::Seats - 1 : FBracket::Seats;
			for (int I = 0; I < Take; ++I)
			{
				B.Seat[I] = Pool[(size_t)I];
			}
			if (B.bHero)
			{
				B.Seat[Take] = FBracket::HeroSeat;
			}
			for (int I = FBracket::Seats - 1; I > 0; --I)
			{
				std::swap(B.Seat[I], B.Seat[Rng.Below(I + 1)]);
			}
			const std::string Rec = B.bHero ? Record(Cup, Day) : std::string();
			const double NowAt = (double)Now.Since * 24.0 + (double)Now.Hour;
			B.HeroReached = B.bHero ? 0 : -1;
			B.bHeroAlive = B.bHero;
			for (int Mt = 0; Mt < FBracket::Matches; ++Mt)
			{
				const int Round = FBracket::RoundOf(Mt);
				if (Round == 0)
				{
					B.Side[Mt][0] = 2 * Mt;
					B.Side[Mt][1] = 2 * Mt + 1;
				}
				const int A = B.Side[Mt][0], C = B.Side[Mt][1];
				if (A >= 0 && C >= 0)
				{
					const double Ends = (double)Day * 24.0 + (double)RoundTo(Cup, Round);
					const bool bHeroHere = B.Seat[A] == FBracket::HeroSeat || B.Seat[C] == FBracket::HeroSeat;
					int Won = -1;
					if (bHeroHere)
					{
						const int Hero = B.Seat[A] == FBracket::HeroSeat ? A : C;
						const int Other = Hero == A ? C : A;
						if ((int)Rec.size() > Round)
						{
							Won = Rec[(size_t)Round] == 'W' ? Hero : Other;
						}
						else if (NowAt >= Ends)
						{
							Won = Other;
							B.bNoShow[Mt] = true;
						}
						if (Won >= 0)
						{
							B.bHeroAlive = Won == Hero;
							B.HeroReached = Won == Hero ? Round + 1 : Round;
						}
					}
					else if (NowAt >= Ends)
					{
						Won = Die(M, *D, Day, Mt, B.Seat[A], B.Seat[C]) ? A : C;
					}
					B.Winner[Mt] = Won;
					const int Next = FBracket::NextMatch(Mt);
					if (Won >= 0 && Next >= 0)
					{
						B.Side[Next][(Mt - FBracket::FirstMatch(Round)) % 2] = Won;
					}
				}
			}
			if (B.HeroReached == 3)
			{
				B.bHeroAlive = false;
			}
			return B;
		}

		/** The hero's match at Thing now, if there is one: his round's hours of a tournament he is in, at a table of its plot
		 *  with its game. Returns the opponent's regular index (-1: none) and the cup, day and round. */
		int MatchAt(const FModel& M, int Thing, const FWhen& Now, int* OutCup = nullptr, long long* OutDay = nullptr, int* OutRound = nullptr) const
		{
			const FGameDef* G = Thing >= 0 && Thing < (int)M.Things.size() ? M.GameOfThing(Thing) : nullptr;
			if (!G || G->Vs.empty())
			{
				return -1;
			}
			const std::string& Plot = M.Things[(size_t)Thing].Plot;
			for (int Cup = 0; Cup < (int)Defs.size(); ++Cup)
			{
				const FCupDef& D = Defs[(size_t)Cup];
				if (D.Plot != Plot || D.Game != G->Vs || D.Weekday != Now.Weekday || !IsSigned(Cup, Now.Since) || IsOff(Now.Since))
				{
					continue;
				}
				const FBracket B = Bracket(M, Cup, Now.Since, Now);
				if (!B.bHeroAlive)
				{
					continue;
				}
				const int Round = B.HeroReached;
				if (Now.Hour < RoundFrom(Cup, Round) || Now.Hour >= RoundTo(Cup, Round))
				{
					continue;
				}
				for (int Mt = FBracket::FirstMatch(Round); Mt < FBracket::FirstMatch(Round) + (4 >> Round); ++Mt)
				{
					const int A = B.Side[Mt][0], C = B.Side[Mt][1];
					if (A < 0 || C < 0 || B.Winner[Mt] >= 0 || (B.Seat[A] != FBracket::HeroSeat && B.Seat[C] != FBracket::HeroSeat))
					{
						continue;
					}
					if (OutCup) *OutCup = Cup;
					if (OutDay) *OutDay = Now.Since;
					if (OutRound) *OutRound = Round;
					return B.Seat[A] == FBracket::HeroSeat ? B.Seat[C] : B.Seat[A];
				}
			}
			return -1;
		}

		/** The hero's match of Round ended so. A win of the final gives the cup's stamp (into M, listed in the result). */
		FCupResult Record(FModel& M, int Cup, long long Day, int Round, EGameResult Result, bool bFestival = false)
		{
			FCupResult R;
			R.Round = Round;
			if (!Def(Cup))
			{
				return R;
			}
			if (bFestival && !IsOff(Day))
			{
				FestDays.insert(Key(Cup, Day));
			}
			std::string& Rec = Results[Key(Cup, Day)];
			if (Round != (int)Rec.size() || Round >= FBracket::Rounds || Result == EGameResult::None)
			{
				return R;
			}
			if (Result == EGameResult::Draw)
			{
				R.Note = ECupNote::Replay;
				return R;
			}
			Rec.push_back(Result == EGameResult::Win ? 'W' : 'L');
			if (Result != EGameResult::Win)
			{
				R.Note = ECupNote::Out;
			}
			else if (Round + 1 >= FBracket::Rounds)
			{
				R.Note = ECupNote::Champion;
				M.GiveStamp(Defs[(size_t)Cup].Collect, &R.NewStamps);
				if (IsFestDay(Cup, Day) && !Defs[(size_t)Cup].FestCollect.empty())
				{
					R.bFestival = true;
					M.GiveStamp(Defs[(size_t)Cup].FestCollect, &R.NewStamps);
				}
			}
			else
			{
				R.Note = ECupNote::Next;
				R.NextHour = RoundFrom(Cup, Round + 1);
			}
			return R;
		}

		/** The tournaments played on Day (for the next morning's paper). */
		void News(const FModel& M, long long Day, const FWhen& Now, std::vector<FCupNews>& Out) const
		{
			for (int Cup = 0; Cup < (int)Defs.size(); ++Cup)
			{
				if (!IsUsable(Cup) || WeekdayOf(Day, Now) != Defs[(size_t)Cup].Weekday || Day < 0 || IsOff(Day))
				{
					continue;
				}
				FWhen After = Now;
				After.Since = Day + 1;
				After.Hour = 0.f;
				const FBracket B = Bracket(M, Cup, Day, After);
				if (!B.IsDone())
				{
					continue;
				}
				FCupNews N;
				N.Cup = Cup;
				N.Winner = NameOf(M, B.Champion());
				const int Final = B.Side[6][0] == B.Winner[6] ? B.Side[6][1] : B.Side[6][0];
				N.RunnerUp = Final >= 0 ? NameOf(M, B.Seat[Final]) : std::string();
				N.HeroReached = B.HeroReached;
				Out.push_back(std::move(N));
			}
		}

		/** How many cups the hero has won (all tournaments, or one). */
		int CupsWon(int Cup = -1) const
		{
			int N = 0;
			for (const auto& P : Results)
			{
				if (P.second == "WWW" && (Cup < 0 || P.first.compare(0, Defs[(size_t)Cup].Id.size() + 1, Defs[(size_t)Cup].Id + "@") == 0))
				{
					++N;
				}
			}
			return N;
		}

		/** A regular's name key; the hero's word for himself for HeroSeat. */
		static std::string NameOf(const FModel& M, int Regular)
		{
			return Regular == FBracket::HeroSeat ? std::string() : Regular >= 0 && Regular < (int)M.Regulars.size() ? M.Regulars[(size_t)Regular].Name : std::string("city.cup.nobody");
		}

		/**
		 * The bracket board's pages ("cup:<id>" texts): the tournament (when, the rules, the sheet's state), the bracket shown
		 * (today's from its start, else the last), the cup case (the last champions, the hero's cups). Names go as '@' keys, the
		 * hero as @city.cup.you. Empty when the cup is unknown or cannot be drawn.
		 */
		std::vector<FCupPage> Pages(const FModel& M, int Cup, const FWhen& Now) const
		{
			std::vector<FCupPage> Out;
			const FCupDef* D = Def(Cup);
			if (!D || !IsUsable(Cup))
			{
				return Out;
			}
			const auto Who = [&](int Regular) { return Regular == FBracket::HeroSeat ? std::string("@city.cup.you") : "@" + NameOf(M, Regular); };
			const auto Date = [&](long long Day)
			{
				// the day's date: Now.MonthDay moved by the days between (good enough for a board: day.month)
				return DateOf(Now, Day);
			};
			// 1. the tournament
			{
				FCupPage P;
				const long long Next = NextDay(Cup, Now);
				const bool bFestNext = IsFestDay(Cup, Next, &Now);
				P.push_back({ "city.cup.head", { "@" + TitleOn(Cup, Next, &Now) } });
				if (bFestNext)
				{
					P.push_back({ "city.cup.fest", {} });
				}
				P.push_back({ "city.cup.when", { "@city.cup.dow." + std::to_string(D->Weekday), Hm(D->Start), "@" + D->Venue } });
				P.push_back({ "city.cup.rules", { Hm(D->Start), Hm(D->Start + D->Step), Hm(D->Start + 2.f * D->Step) } });
				P.push_back({ "city.cup.rules." + D->Game, {} });
				const long long Skipped = SkippedDay(Cup, Now);
				if (Skipped >= 0)
				{
					P.push_back({ "city.cup.holiday", { Date(Skipped) } });
				}
				P.push_back({ IsSigned(Cup, Next) ? "city.cup.signed" : "city.cup.sheet", { Date(Next), Hm(D->Start) } });
				Out.push_back(std::move(P));
			}
			// 2. the bracket
			const long long Shown = ShownDay(Cup, Now);
			if (Shown >= 0)
			{
				const FBracket B = Bracket(M, Cup, Shown, Now);
				FCupPage P;
				P.push_back({ Shown == Now.Since ? "city.cup.bracket_today" : "city.cup.bracket_last", { Date(Shown) } });
				if (IsFestDay(Cup, Shown, &Now))
				{
					P.push_back({ "city.cup.fest_bracket", { "@" + TitleOn(Cup, Shown, &Now) } });
				}
				for (int Round = 0; Round < FBracket::Rounds; ++Round)
				{
					P.push_back({ "city.cup.round." + std::to_string(Round), { Hm(RoundFrom(Cup, Round)) } });
					for (int Mt = FBracket::FirstMatch(Round); Mt < FBracket::FirstMatch(Round) + (4 >> Round); ++Mt)
					{
						const int A = B.Side[Mt][0], C = B.Side[Mt][1];
						const std::string NA = A >= 0 ? Who(B.Seat[A]) : std::string("@city.cup.tbd");
						const std::string NC = C >= 0 ? Who(B.Seat[C]) : std::string("@city.cup.tbd");
						if (B.Winner[Mt] >= 0)
						{
							P.push_back({ B.bNoShow[Mt] ? "city.cup.m.noshow" : "city.cup.m.won", { NA, NC, Who(B.Seat[B.Winner[Mt]]) } });
						}
						else
						{
							P.push_back({ "city.cup.m.wait", { NA, NC } });
						}
					}
				}
				if (B.IsDone())
				{
					P.push_back({ B.Champion() == FBracket::HeroSeat ? "city.cup.champion_you" : "city.cup.champion", { Who(B.Champion()) } });
				}
				Out.push_back(std::move(P));
			}
			// 3. the cup case: the last six champions, the hero's cups
			{
				FCupPage P;
				P.push_back({ "city.cup.case", { "@" + D->Title } });
				int Lines = 0;
				for (long long Day = Shown; Day >= 0 && Lines < 6; Day -= 7)
				{
					const FBracket B = Bracket(M, Cup, Day, Now);
					if (B.IsDone())
					{
						P.push_back({ "city.cup.case.line", { Date(Day), Who(B.Champion()) } });
						++Lines;
					}
				}
				if (Lines == 0)
				{
					P.push_back({ "city.cup.case.empty", {} });
				}
				const int Mine = CupsWon(Cup);
				if (Mine > 0)
				{
					P.push_back({ "city.cup.case.yours", { std::to_string(Mine) } });
				}
				Out.push_back(std::move(P));
			}
			return Out;
		}

		/** "13:30". */
		static std::string Hm(float Hour)
		{
			const int Minutes = ((int)std::floor(Hour * 60.f + 0.5f) % (24 * 60) + 24 * 60) % (24 * 60);
			char Buf[8];
			std::snprintf(Buf, sizeof(Buf), "%02d:%02d", Minutes / 60, Minutes % 60);
			return Buf;
		}
		/** "21.11": a day's date from today's (MonthDay) by the days between; "" when today's date is unknown. */
		static std::string DateOf(const FWhen& Now, long long Day)
		{
			if (Now.MonthDay <= 0)
			{
				return std::string();
			}
			static const int Len[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
			int Mo = Now.MonthDay / 100, Dd = Now.MonthDay % 100;
			for (long long I = Now.Since; I < Day; ++I)
			{
				if (++Dd > Len[(Mo + 11) % 12]) { Dd = 1; Mo = Mo % 12 + 1; }
			}
			for (long long I = Now.Since; I > Day; --I)
			{
				if (--Dd < 1) { Mo = (Mo + 10) % 12 + 1; Dd = Len[Mo - 1]; }
			}
			char Buf[8];
			std::snprintf(Buf, sizeof(Buf), "%02d.%02d", Dd, Mo);
			return Buf;
		}

		const std::string& Record(int Cup, long long Day) const
		{
			static const std::string None;
			const auto It = Results.find(Key(Cup, Day));
			return It == Results.end() ? None : It->second;
		}

		/** Appends the state to the model's flat map ("k:" keys). */
		void SaveState(std::map<std::string, std::string>& Out) const
		{
			for (const std::string& S : Signed)
			{
				Out["k:s:" + S] = "1";
			}
			for (const auto& P : Results)
			{
				if (!P.second.empty())
				{
					Out["k:r:" + P.first] = P.second;
				}
			}
			for (const std::string& S : FestDays)
			{
				Out["k:f:" + S] = "1";
			}
		}
		void LoadState(const std::map<std::string, std::string>& In)
		{
			Reset();
			for (const auto& P : In)
			{
				if (P.first.compare(0, 4, "k:s:") == 0)
				{
					Signed.insert(P.first.substr(4));
				}
				else if (P.first.compare(0, 4, "k:r:") == 0)
				{
					Results[P.first.substr(4)] = P.second.substr(0, FBracket::Rounds);
				}
				else if (P.first.compare(0, 4, "k:f:") == 0)
				{
					FestDays.insert(P.first.substr(4));
				}
			}
		}
		void Reset()
		{
			Signed.clear();
			Results.clear();
			FestDays.clear();
		}

	private:
		std::set<std::string> Signed;
		std::map<std::string, std::string> Results;
		/** The tournament days played on a holiday ("<cup>@<day>"). */
		std::set<std::string> FestDays;

		std::string Key(int Cup, long long Day) const
		{
			return (Def(Cup) ? Defs[(size_t)Cup].Id : std::string("?")) + "@" + std::to_string(Day);
		}
		/** Does A beat C in a match between regulars? The stronger is likelier, never sure. */
		static bool Die(const FModel& M, const FCupDef& D, long long Day, int Match, int A, int C)
		{
			const double SA = A >= 0 && A < (int)M.Regulars.size() ? M.Regulars[(size_t)A].Skill : 0.5;
			const double SC = C >= 0 && C < (int)M.Regulars.size() ? M.Regulars[(size_t)C].Skill : 0.5;
			const double P = std::min(0.88, std::max(0.12, 0.5 + 1.1 * (SA - SC)));
			FRng Rng;
			Rng.State = HashText(D.Id + "#m") ^ ((uint64_t)(Day + 1) * 0xC2B2AE3D27D4EB4Full) ^ ((uint64_t)(Match + 1) * 0x165667B19E3779F9ull);
			return Rng.Unit() < P;
		}
	};
}
