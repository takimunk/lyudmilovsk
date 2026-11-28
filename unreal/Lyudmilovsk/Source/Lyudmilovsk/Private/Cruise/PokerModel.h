#pragma once

// Five-card draw at the card table of the motor ship «Полдень» (docs/ship.md "Poker"). Plain C++ (std only, no engine
// types): ULyudShipPoker runs it in the game, tools/pokersim checks it offline (bash tools/pokersim/run.sh test).
//
// The rules: an ante from everybody, five cards each, a betting round (fixed limit: the small bet), everybody still in
// changes 0-3 cards, a second betting round (the big bet), the showdown. At most MaxRaises raises over a bet in a round.
// Table stakes: nobody reaches into a pocket during a hand; a player who cannot cover a bet is all-in and can win from each
// other player no more than he put in himself (the pot is paid out by layers, so chips are always conserved).
//
// Everything is decided by the seed (voyage, day, hand number). The regulars never see the hero's cards: they judge their own
// hand, the pot odds, the raises and how many cards the others changed, each in his own style, and each gives himself away
// in his own way (a "tell": a short line whose kind is honest only as often as the man is). One scripted exception, for the
// quest "catch the card sharp": in the marked-cards mode Альберт, on the hands he deals, draws the best cards left in the
// deck; watching his hands while he deals (and only then) catches him.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace LyudPoker
{
	// ------------------------------------------------------------------------------------------------------------ the cards

	/** A card is 0..51: rank 2..14 (ace high), suit 0..3 (spades, hearts, diamonds, clubs). */
	inline int RankOf(int Card) { return Card / 4 + 2; }
	inline int SuitOf(int Card) { return Card % 4; }
	inline int MakeCard(int Rank, int Suit) { return (Rank - 2) * 4 + Suit; }

	enum ECat : int { HighCard = 0, OnePair, TwoPair, Three, Straight, Flush, FullHouse, Four, StraightFlush, NumCats };

	/** The ids of the hands (strings poker.hand.<id>, events poker_hand, stamps poker/<id>); a royal flush is "royal". */
	inline const char* CatId(int Cat)
	{
		static const char* const Ids[NumCats] = { "high", "pair", "two_pair", "three", "straight", "flush", "full_house", "four", "straight_flush" };
		return Ids[Cat < 0 ? 0 : (Cat >= NumCats ? NumCats - 1 : Cat)];
	}

	/** The worth of five cards: the higher the better, equal only for hands that split the pot. The category in the bits from
	 *  20 up, then the ranks that break ties, most important first (a straight: only its top card; A-2-3-4-5 tops at 5). */
	inline uint32_t Eval5(const int* Cards)
	{
		int Count[15] = { 0 };
		bool bFlush = true;
		for (int I = 0; I < 5; ++I)
		{
			++Count[RankOf(Cards[I])];
			bFlush = bFlush && SuitOf(Cards[I]) == SuitOf(Cards[0]);
		}
		int Order[5] = { 0, 0, 0, 0, 0 };
		int N = 0;
		for (int K = 4; K >= 1; --K)
		{
			for (int R = 14; R >= 2; --R)
			{
				if (Count[R] == K)
				{
					Order[N++] = R;
				}
			}
		}
		int Cat = HighCard;
		if (N == 5)
		{
			int Top = 0;
			if (Order[0] - Order[4] == 4)
			{
				Top = Order[0];
			}
			else if (Order[0] == 14 && Order[1] == 5)
			{
				Top = 5;   // the wheel: A-2-3-4-5
			}
			if (Top > 0)
			{
				return ((uint32_t)(bFlush ? StraightFlush : Straight) << 20) | ((uint32_t)Top << 16);
			}
			Cat = bFlush ? Flush : HighCard;
		}
		else if (N == 4)
		{
			Cat = OnePair;
		}
		else if (N == 3)
		{
			Cat = Count[Order[0]] == 3 ? Three : TwoPair;
		}
		else
		{
			Cat = Count[Order[0]] == 4 ? Four : FullHouse;
		}
		uint32_t V = (uint32_t)Cat;
		for (int I = 0; I < 5; ++I)
		{
			V = (V << 4) | (uint32_t)(I < N ? Order[I] : 0);
		}
		return V;
	}

	inline int CatOf(uint32_t Value) { return (int)(Value >> 20); }
	inline int TopRankOf(uint32_t Value) { return (int)((Value >> 16) & 15u); }
	inline bool IsRoyal(uint32_t Value) { return CatOf(Value) == StraightFlush && TopRankOf(Value) == 14; }
	/** "royal" or the category's id. */
	inline const char* HandId(uint32_t Value) { return IsRoyal(Value) ? "royal" : CatId(CatOf(Value)); }

	// ---------------------------------------------------------------------------------------------------------- the chances

	struct FRng
	{
		uint64_t S = 1;
		uint64_t Next()
		{
			S += 0x9E3779B97F4A7C15ull;
			uint64_t Z = S;
			Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
			Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
			return Z ^ (Z >> 31);
		}
		double Uni() { return (double)(Next() >> 11) * (1.0 / 9007199254740992.0); }
		int Below(int N) { return N <= 1 ? 0 : (int)(Next() % (uint64_t)N); }
	};

	/** The seed of a hand: the voyage (any number that is the same all cruise), its day, the number of the hand. */
	inline uint64_t HandSeed(long long Voyage, int Day, int Hand)
	{
		FRng R;
		R.S = (uint64_t)Voyage * 0x100000001B3ull + (uint64_t)(Day + 7) * 0x9E3779B1ull + (uint64_t)(Hand + 1) * 0x85EBCA6Bull;
		R.Next();
		return R.Next();
	}

	// ----------------------------------------------------------------------------------------- how good a hand is (the AI's eye)

	inline double Clamp01(double V) { return V < 0.0 ? 0.0 : (V > 1.0 ? 1.0 : V); }

	/** The share of hands (as dealt, before the draw) a hand beats, roughly: no pair is the lower half, a pair up to 0.925... */
	inline double PreStrength(uint32_t Value)
	{
		const int Cat = CatOf(Value);
		const int R0 = (int)((Value >> 16) & 15u);
		const int R1 = (int)((Value >> 12) & 15u);
		switch (Cat)
		{
		case HighCard: { const double T = Clamp01((R0 - 5) / 9.0); return 0.50 * T * T; }
		case OnePair: return 0.50 + 0.425 * ((R0 - 2) + (R1 - 2) / 13.0) / 13.0;
		case TwoPair: return 0.925 + 0.047 * (R0 - 3) / 12.0;
		case Three: return 0.972 + 0.0215 * (R0 - 2) / 13.0;
		default: return 0.9935 + 0.0065 * (Cat - Straight) / 5.0;
		}
	}

	/** The same after the draw, among the hands of players who stayed for it (they have improved: a pair is now middling). */
	inline double PostStrength(uint32_t Value)
	{
		const int Cat = CatOf(Value);
		const int R0 = (int)((Value >> 16) & 15u);
		const int R1 = (int)((Value >> 12) & 15u);
		switch (Cat)
		{
		case HighCard: { const double T = Clamp01((R0 - 6) / 8.0); return 0.26 * T * T; }
		case OnePair: return 0.26 + 0.42 * ((R0 - 2) + (R1 - 2) / 13.0) / 13.0;
		case TwoPair: return 0.68 + 0.15 * (R0 - 3) / 11.0;
		case Three: return 0.83 + 0.10 * (R0 - 2) / 12.0;
		case Straight: return 0.93 + 0.025 * (R0 - 5) / 9.0;
		case Flush: return 0.955 + 0.017 * (R0 - 7) / 7.0;
		case FullHouse: return 0.972 + 0.021 * (R0 - 2) / 12.0;
		default: return 0.995;
		}
	}

	/** Four cards of one suit: the index of the odd card, -1 none. */
	inline int FourFlushOdd(const int* Cards)
	{
		for (int Odd = 0; Odd < 5; ++Odd)
		{
			int Suit = -1;
			bool bSame = true;
			for (int I = 0; I < 5 && bSame; ++I)
			{
				if (I == Odd)
				{
					continue;
				}
				if (Suit < 0)
				{
					Suit = SuitOf(Cards[I]);
				}
				bSame = SuitOf(Cards[I]) == Suit;
			}
			if (bSame)
			{
				return Odd;
			}
		}
		return -1;
	}

	/** Four cards in a row open at both ends (5-6-7-8, not J-Q-K-A): the index of the odd card, -1 none. */
	inline int OpenStraightOdd(const int* Cards)
	{
		for (int Odd = 0; Odd < 5; ++Odd)
		{
			int Lo = 15, Hi = 0;
			int Seen = 0;
			bool bDistinct = true;
			for (int I = 0; I < 5; ++I)
			{
				if (I == Odd)
				{
					continue;
				}
				const int R = RankOf(Cards[I]);
				bDistinct = bDistinct && (Seen & (1 << R)) == 0;
				Seen |= 1 << R;
				Lo = std::min(Lo, R);
				Hi = std::max(Hi, R);
			}
			if (bDistinct && Hi - Lo == 3 && Hi < 14)
			{
				return Odd;
			}
		}
		return -1;
	}

	/** The cards to change (a mask of the five, at most three): what a sound player does. */
	inline int SoundDraw(const int* Cards)
	{
		const uint32_t V = Eval5(Cards);
		const int Cat = CatOf(V);
		if (Cat >= Straight)
		{
			return 0;
		}
		int Count[15] = { 0 };
		for (int I = 0; I < 5; ++I)
		{
			++Count[RankOf(Cards[I])];
		}
		int Mask = 0;
		if (Cat >= OnePair)
		{
			for (int I = 0; I < 5; ++I)
			{
				if (Count[RankOf(Cards[I])] == 1)
				{
					Mask |= 1 << I;
				}
			}
			return Mask;
		}
		int Odd = FourFlushOdd(Cards);
		if (Odd < 0)
		{
			Odd = OpenStraightOdd(Cards);
		}
		if (Odd >= 0)
		{
			return 1 << Odd;
		}
		// nothing: the two highest cards stay
		int Order[5] = { 0, 1, 2, 3, 4 };
		std::sort(Order, Order + 5, [Cards](int A, int B) { return Cards[A] > Cards[B]; });
		return (1 << Order[2]) | (1 << Order[3]) | (1 << Order[4]);
	}

	// ---------------------------------------------------------------------------------------------------------- the regulars

	constexpr int NumOpponents = 6;
	enum EOpp : int { Albert = 0, Givi, Vera, Taras, Blum, Lev };

	inline const char* OppId(int Opp)
	{
		static const char* const Ids[NumOpponents] = { "ship_albert", "ship_givi", "ship_vera", "ship_taras", "ship_blum", "ship_lev" };
		return Ids[Opp < 0 ? 0 : (Opp >= NumOpponents ? NumOpponents - 1 : Opp)];
	}

	struct FStyle
	{
		/** How much better than the pot's odds a call must be (above 1: tight, below: loose). */
		double Margin;
		/** How readily a good hand bets and raises instead of calling (0..1). */
		double Aggr;
		/** How often nothing is bet as if it were something. */
		double Bluff;
		/** A bluff is a raise over a bet as readily as a bet into nobody (Альберт: rarely, but big). */
		double BluffRaise;
		/** How wrongly the own hand is judged (Блюм plays by ear). */
		double Noise;
		/** How much the others' raises and pat hands are respected (0..1). */
		double Respect;
		/** How often the tell says what is true. */
		double Honesty;
	};

	inline const FStyle& StyleOf(int Opp)
	{
		static const FStyle Styles[NumOpponents] = {
			/* Альберт: tight-aggressive, bluffs rarely but big, gives away little */ { 2.00, 0.88, 0.025, 1.0, 0.02, 0.8, 0.50 },
			/* Гиви: loose, calls along, talks when weak                            */ { 0.30, 0.28, 0.08, 0.2, 0.08, 0.15, 0.86 },
			/* Вера Игнатьевна: careful, sharp                                      */ { 1.15, 0.55, 0.06, 0.3, 0.03, 0.9, 0.72 },
			/* Тарас: bold, bluffs a lot and badly                                  */ { 0.60, 0.80, 0.26, 0.5, 0.06, 0.3, 0.93 },
			/* Блюм: by ear                                                         */ { 0.80, 0.55, 0.16, 0.5, 0.30, 0.4, 0.62 },
			/* Лев Маркович: the odds, exactly                                      */ { 1.00, 0.62, 0.07, 0.3, 0.00, 1.0, 0.76 },
		};
		return Styles[Opp < 0 ? 0 : (Opp >= NumOpponents ? NumOpponents - 1 : Opp)];
	}

	/** What a tell says of the man: strings poker.tell.<opponent>.<strong|weak|bluff>.<0..TellLines-1>. */
	enum ETell : int { TellStrong = 0, TellWeak, TellBluff, NumTells };
	constexpr int TellLines = 6;
	constexpr int TalkLines = 8;
	inline const char* TellId(int Tell)
	{
		static const char* const Ids[NumTells] = { "strong", "weak", "bluff" };
		return Ids[Tell < 0 ? 0 : (Tell >= NumTells ? NumTells - 1 : Tell)];
	}

	// ------------------------------------------------------------------------------------------------------------ the stakes

	struct FStakes
	{
		bool bRoubles = false;
		int Ante = 1;
		int Small = 2;
		int Big = 4;
		int MaxRaises = 3;
		/** What the hero sits down with (matches: handed out; roubles: paid from his pocket). */
		int BuyIn = 50;
		/** What each regular brings for the night. */
		int Purse = 50;
		/** The table breaks up after this many hands at one sitting. */
		int MaxHands = 24;
		/** Roubles: the most the hero may be down in one evening (further buy-ins are refused). */
		int LossCap = 0;

		static FStakes Matches() { return FStakes(); }
		static FStakes Roubles()
		{
			FStakes S;
			S.bRoubles = true;
			S.BuyIn = 30;
			S.Purse = 40;
			S.LossCap = 60;
			return S;
		}
	};

	/** Game minutes one hand takes. */
	constexpr int MinutesPerHand = 4;

	/** One evening at the roubles table: what the hero is down so far and what is left in each regular's purse. */
	struct FNight
	{
		long long Day = -1;
		int Net = 0;                       // bought in minus cashed out (negative: he is up)
		int Purse[NumOpponents] = { 0, 0, 0, 0, 0, 0 };

		/** The evening of a clock: the small hours belong to the evening before. */
		static long long EveningOf(long long Day, double Hour) { return Hour < 12.0 ? Day - 1 : Day; }

		void Roll(long long InDay, const FStakes& S)
		{
			if (InDay != Day)
			{
				Day = InDay;
				Net = 0;
				for (int& P : Purse)
				{
					P = S.Purse;
				}
			}
		}
		bool CanBuyIn(const FStakes& S) const { return !S.bRoubles || Net + S.BuyIn <= S.LossCap; }
	};

	/** Who sits at the table while the evening's cards are on, by the clock (the same hours the people's life seats their
	 *  bodies there, tools/ship/cruise_people.py): Альберт all evening (never once caught), Тарас till half past midnight,
	 *  Лев Маркович till half past eleven, Вера Игнатьевна till eleven, Блюм after his concert (Day 1: 23:10, Day 2: 23:40),
	 *  and Гиви calls his bets from behind the bar when the saloon has gone quiet and a place is free. At most four; nobody
	 *  at other hours. */
	inline std::vector<int> Lineup(int Day, double Hour, bool bCardsOn, bool bCaught)
	{
		std::vector<int> Out;
		if (!bCardsOn)
		{
			return Out;
		}
		// whole minutes since midnight of the evening's day (the small hours count on: 00:30 is 1470)
		const int M = (int)std::floor((Hour < 12.0 ? Hour + 24.0 : Hour) * 60.0 + 0.5);
		if (!bCaught)
		{
			Out.push_back(Albert);
		}
		if (M < 24 * 60 + 30)
		{
			Out.push_back(Taras);
		}
		if (M < 23 * 60 + 30)
		{
			Out.push_back(Lev);
		}
		if (M < 23 * 60)
		{
			Out.push_back(Vera);
		}
		if (M >= 23 * 60 + (Day <= 1 ? 10 : 40))
		{
			Out.push_back(Blum);
		}
		if (M >= 23 * 60 && (int)Out.size() < 4)
		{
			Out.push_back(Givi);
		}
		if ((int)Out.size() > 4)
		{
			Out.resize(4);
		}
		return Out;
	}

	// ------------------------------------------------------------------------------------------------------------- the table

	enum class EAct : int { Fold = 0, CheckCall, BetRaise };
	enum class EPhase : int { Idle = 0, Dealing, Bet1, Draw, Bet2, Showdown, Over };
	enum class EWatch : int { NotNow = 0, Nothing, Caught };

	enum class EEv : int
	{
		Deal,       // Seat: the dealer; A: the hand's number
		Ante,       // A: the ante
		Check, Bet, Call, Raise, Fold,   // A: chips put in; B: 1 all-in
		Draw,       // A: cards changed
		Tell,       // A: ETell shown; B: the line
		Show,       // A: the category; B: 1 royal
		Win,        // A: chips taken
		Talk,       // B: the line (between hands)
		Suspect,    // Seat: the sharp (his marked draw came out a full house or better)
		Caught,     // Seat: the sharp
		Missed,     // Seat: the sharp (watched an honest deal)
		Left,       // Seat: a regular out of chips gets up
		Gone,       // Seat: a regular gets up because it is his time
		Joined,     // Seat: a regular sits in
	};

	struct FEvent
	{
		EEv Type = EEv::Deal;
		int Seat = 0;
		int A = 0;
		int B = 0;
	};

	struct FSeat
	{
		int Opp = -1;                      // -1: the hero
		int Stack = 0;
		int Bet = 0;                       // this round
		int Total = 0;                     // this hand
		bool bIn = false;                  // dealt in this hand
		bool bFolded = false;
		bool bOut = false;                 // left the table
		bool bActed = false;
		int Cards[5] = { 0, 0, 0, 0, 0 };
		int Drew = -1;
		uint32_t Value = 0;
		int Tell = -1;                     // the last tell shown this hand (ETell), -1 none
		int TellLine = -1, TellTrue = -1, TellPhase = -1;
		int Won = 0;                       // taken from the last pot
		int Put = 0;                       // put into the last pot
		// the sitting's statistics (tools/pokersim: the styles in their bands)
		int Hands = 0, Vpip = 0, Raised = 0, Called = 0, Bluffs = 0;
		bool bVpipHand = false;

		bool Live() const { return bIn && !bFolded; }
		bool CanAct() const { return Live() && Stack > 0; }
	};

	/** What the hand just played came to, for the game's events and stamps. */
	struct FResult
	{
		bool bHeroIn = false;              // he was dealt in
		bool bHeroWon = false;             // he took chips from the pot
		bool bShowdown = false;
		bool bHeroShown = false;           // his cards were shown down
		bool bBluffWin = false;            // everybody folded to him and he held less than a pair
		uint32_t HeroValue = 0;
		int Pot = 0;
		int HeroNet = 0;                   // what the hand changed in his stack
		int BeatenMask = 0;                // regulars (1 << EOpp) whose shown cards he beat
		bool bVoid = false;                // the sharp was caught: the antes went back
	};

	enum class EEnd : int { None = 0, Broke, Empty, MaxHands };

	struct FTable
	{
		FStakes Stakes;
		std::vector<FSeat> Seats;          // 0: the hero
		long long Voyage = 0;
		int Day = 0;
		int HandBase = 0;                  // hands played before this sitting (so no two sittings deal the same cards)
		int HandNo = 0;                    // hands dealt at this sitting
		int Dealer = 0;
		EPhase Phase = EPhase::Idle;
		int Turn = -1;
		int CurrentBet = 0;
		int Bets = 0;                      // the bet and the raises of this round
		int Deck[52];
		int DeckAt = 0;
		FRng Rng;
		std::vector<FEvent> Log;           // of this hand
		FResult Result;
		bool bSharp = false;               // the quest's mode
		bool bMarked = false;              // this hand is dealt by the sharp from a marked deck
		bool bCaught = false;
		int HeroBuyIn = 0;

		// ---- sitting down

		/** Opponents and what each has left for the night (a regular with an empty purse does not sit). */
		void Sit(const FStakes& InStakes, const std::vector<int>& Opps, const int* Purses, long long InVoyage, int InDay, int InHandBase)
		{
			Stakes = InStakes;
			Voyage = InVoyage;
			Day = InDay;
			HandBase = InHandBase;
			HandNo = 0;
			Phase = EPhase::Idle;
			Turn = -1;
			Log.clear();
			Log.reserve(64);
			Result = FResult();
			bMarked = false;
			Seats.clear();
			FSeat Hero;
			Hero.Stack = Stakes.BuyIn;
			HeroBuyIn = Stakes.BuyIn;
			Seats.push_back(Hero);
			for (const int O : Opps)
			{
				FSeat S;
				S.Opp = O;
				S.Stack = Purses ? Purses[O] : Stakes.Purse;
				if (S.Stack > 0)
				{
					Seats.push_back(S);
				}
			}
			FRng R;
			R.S = HandSeed(Voyage, Day, HandBase + 100000);
			Dealer = R.Below((int)Seats.size());
		}

		int NumSeats() const { return (int)Seats.size(); }
		int Pot() const
		{
			int P = 0;
			for (const FSeat& S : Seats)
			{
				P += S.bIn ? S.Total : 0;
			}
			return P;
		}
		int Chips() const
		{
			int C = 0;
			for (const FSeat& S : Seats)
			{
				C += S.Stack + (Phase != EPhase::Idle && Phase != EPhase::Over && S.bIn ? S.Total : 0);
			}
			return C;
		}
		int SeatOf(int Opp) const
		{
			for (int I = 1; I < NumSeats(); ++I)
			{
				if (Seats[(size_t)I].Opp == Opp && !Seats[(size_t)I].bOut)
				{
					return I;
				}
			}
			return -1;
		}
		int Playing() const
		{
			int N = 0;
			for (const FSeat& S : Seats)
			{
				N += !S.bOut && S.Stack > 0 ? 1 : 0;
			}
			return N;
		}
		int LiveCount() const
		{
			int N = 0;
			for (const FSeat& S : Seats)
			{
				N += S.Live() ? 1 : 0;
			}
			return N;
		}
		EEnd Ended() const
		{
			if (Phase != EPhase::Idle && Phase != EPhase::Over)
			{
				return EEnd::None;
			}
			if (Seats.empty() || Seats[0].Stack <= 0)
			{
				return EEnd::Broke;
			}
			if (Playing() < 2)
			{
				return EEnd::Empty;
			}
			return HandNo >= Stakes.MaxHands ? EEnd::MaxHands : EEnd::None;
		}

		// ---- a hand

		bool WaitingHero() const { return (Phase == EPhase::Bet1 || Phase == EPhase::Bet2 || Phase == EPhase::Draw) && Turn == 0; }
		int ToCall(int Seat) const { return std::max(0, CurrentBet - Seats[(size_t)Seat].Bet); }
		int Unit() const { return Phase == EPhase::Bet2 ? Stakes.Big : Stakes.Small; }
		bool CanRaise(int Seat) const { return Bets < 1 + Stakes.MaxRaises && Seats[(size_t)Seat].Stack > ToCall(Seat); }

		/** Deals the next hand (false: the sitting is over). The table then waits in Dealing until the first Step.
		 *  NewLineup (with the purses of the night): who should be at the table now; those whose hour has passed get up,
		 *  newcomers sit in (a regular who lost everything does not come back). */
		bool StartHand(const std::vector<int>* NewLineup = nullptr, const int* Purses = nullptr)
		{
			if (Phase != EPhase::Idle && Phase != EPhase::Over)
			{
				return false;
			}
			Log.clear();
			if (NewLineup)
			{
				Reseat(*NewLineup, Purses);
			}
			if (Ended() != EEnd::None)
			{
				return false;
			}
			Result = FResult();
			Rng.S = HandSeed(Voyage, Day, HandBase + HandNo);
			++HandNo;
			// the deal passes to the left
			do
			{
				Dealer = (Dealer + 1) % NumSeats();
			} while (Seats[(size_t)Dealer].bOut || Seats[(size_t)Dealer].Stack <= 0);
			for (int I = 0; I < 52; ++I)
			{
				Deck[I] = I;
			}
			for (int I = 51; I > 0; --I)
			{
				std::swap(Deck[I], Deck[Rng.Below(I + 1)]);
			}
			DeckAt = 0;
			bMarked = bSharp && !bCaught && Seats[(size_t)Dealer].Opp == Albert && Rng.Uni() < 0.8;
			Log.push_back({ EEv::Deal, Dealer, HandBase + HandNo, 0 });
			for (FSeat& S : Seats)
			{
				S.bIn = !S.bOut && S.Stack > 0;
				S.bFolded = false;
				S.bActed = false;
				S.Bet = 0;
				S.Total = 0;
				S.Drew = -1;
				S.Tell = -1;
				S.TellLine = -1;
				S.TellTrue = -1;
				S.TellPhase = -1;
				S.Won = 0;
				S.Put = 0;
				S.Value = 0;
				S.bVpipHand = false;
				if (S.bIn)
				{
					++S.Hands;
					const int A = std::min(Stakes.Ante, S.Stack);
					S.Stack -= A;
					S.Total = A;
				}
			}
			Log.push_back({ EEv::Ante, -1, Stakes.Ante, 0 });
			for (int C = 0; C < 5; ++C)
			{
				for (int K = 1; K <= NumSeats(); ++K)
				{
					FSeat& S = Seats[(size_t)((Dealer + K) % NumSeats())];
					if (S.bIn)
					{
						S.Cards[C] = Deck[DeckAt++];
					}
				}
			}
			for (FSeat& S : Seats)
			{
				if (S.bIn)
				{
					S.Value = Eval5(S.Cards);
				}
			}
			Result.bHeroIn = Seats[0].bIn;
			Phase = EPhase::Dealing;
			Turn = -1;
			return true;
		}

		/** The hero watches the dealer's hands: it catches Альберт only while he deals a marked hand. Caught: the hand is void
		 *  (the antes go back) and he leaves the table. */
		EWatch WatchHands()
		{
			if (Phase != EPhase::Dealing || Seats[(size_t)Dealer].Opp != Albert)
			{
				return EWatch::NotNow;
			}
			if (!bMarked)
			{
				Log.push_back({ EEv::Missed, Dealer, 0, 0 });
				return EWatch::Nothing;
			}
			for (FSeat& S : Seats)
			{
				if (S.bIn)
				{
					S.Stack += S.Total;
					S.Total = 0;
					S.bIn = false;
				}
			}
			Seats[(size_t)Dealer].bOut = true;
			bCaught = true;
			bMarked = false;
			Result.bVoid = true;
			Log.push_back({ EEv::Caught, Dealer, 0, 0 });
			Phase = EPhase::Over;
			Turn = -1;
			return EWatch::Caught;
		}

		/** One thing happens (a regular's move, the draw of one, the showdown); false when the hero is waited for or the hand
		 *  is over. */
		bool Step()
		{
			switch (Phase)
			{
			case EPhase::Dealing:
				BeginRound(EPhase::Bet1);
				return true;
			case EPhase::Bet1:
			case EPhase::Bet2:
			{
				if (Turn <= 0)
				{
					return false;
				}
				int Tell = -1;
				const EAct A = Decide(Turn, Tell);
				const int Seat = Turn;
				Apply(Seat, A);
				if (Tell >= 0 && A != EAct::Fold)
				{
					ShowTell(Seat, Tell);
				}
				return true;
			}
			case EPhase::Draw:
				if (Turn <= 0)
				{
					return false;
				}
				DoDraw(Turn, DrawOf(Turn));
				return true;
			case EPhase::Showdown:
				Finish(true);
				return true;
			default:
				return false;
			}
		}

		/** The hero's move in a betting round (a raise that is not allowed is a call). */
		bool HeroAct(EAct A)
		{
			if (!(Phase == EPhase::Bet1 || Phase == EPhase::Bet2) || Turn != 0)
			{
				return false;
			}
			Apply(0, A);
			return true;
		}

		/** The hero changes the cards of the mask (at most three; more are cut to the first three). */
		bool HeroDraw(int Mask)
		{
			if (Phase != EPhase::Draw || Turn != 0)
			{
				return false;
			}
			int Kept = 0, Out = 0;
			for (int I = 0; I < 5; ++I)
			{
				if ((Mask & (1 << I)) && Kept < 3)
				{
					Out |= 1 << I;
					++Kept;
				}
			}
			DoDraw(0, Out);
			return true;
		}

		/** The hero gets up in the middle of a hand: his cards are folded, the others play it out at once. */
		void HeroLeave()
		{
			if (Phase == EPhase::Idle || Phase == EPhase::Over)
			{
				return;
			}
			if (Phase == EPhase::Dealing)
			{
				BeginRound(EPhase::Bet1);
			}
			if (Seats[0].Live() && Phase != EPhase::Showdown)
			{
				Seats[0].bFolded = true;
				Log.push_back({ EEv::Fold, 0, 0, 0 });
				if (LiveCount() <= 1)
				{
					Finish(false);
					return;
				}
				if (Turn == 0 && Phase == EPhase::Draw)
				{
					Turn = NextDrawer(0);
					if (Turn < 0)
					{
						BeginRound(EPhase::Bet2);
					}
				}
				else if (Turn == 0)
				{
					Turn = NextSeat(0, true);
					if (Turn < 0)
					{
						EndRound();
					}
				}
			}
			for (int Guard = 0; Phase != EPhase::Over && Guard < 500 && Step(); ++Guard)
			{
			}
		}

		/** The regulars' purses after the sitting (what each has left for the night). */
		void Purses(int* Out) const
		{
			for (const FSeat& S : Seats)
			{
				if (S.Opp >= 0)
				{
					Out[S.Opp] = S.Stack;
				}
			}
		}

	private:
		void Reseat(const std::vector<int>& Now, const int* Purses)
		{
			for (int I = 1; I < NumSeats(); ++I)
			{
				FSeat& S = Seats[(size_t)I];
				if (!S.bOut && std::find(Now.begin(), Now.end(), S.Opp) == Now.end())
				{
					S.bOut = true;
					S.bIn = false;
					Log.push_back({ EEv::Gone, I, 0, 0 });
				}
			}
			for (const int O : Now)
			{
				int Has = -1;
				for (int I = 1; I < NumSeats(); ++I)
				{
					Has = Seats[(size_t)I].Opp == O ? I : Has;
				}
				if (Has > 0)
				{
					FSeat& S = Seats[(size_t)Has];
					if (S.bOut && S.Stack > 0 && !(O == Albert && bCaught))
					{
						S.bOut = false;
						Log.push_back({ EEv::Joined, Has, 0, 0 });
					}
					continue;
				}
				FSeat S;
				S.Opp = O;
				S.Stack = Purses ? Purses[O] : Stakes.Purse;
				if (S.Stack > 0 && !(O == Albert && bCaught))
				{
					Seats.push_back(S);
					Log.push_back({ EEv::Joined, NumSeats() - 1, 0, 0 });
				}
			}
		}

		int NextSeat(int From, bool bNeedsAct) const
		{
			for (int K = 1; K <= NumSeats(); ++K)
			{
				const int I = (From + K) % NumSeats();
				const FSeat& S = Seats[(size_t)I];
				if (bNeedsAct ? (S.CanAct() && (!S.bActed || S.Bet < CurrentBet)) : S.Live())
				{
					return I;
				}
			}
			return -1;
		}

		void BeginRound(EPhase InPhase)
		{
			Phase = InPhase;
			CurrentBet = 0;
			Bets = 0;
			int Able = 0;
			for (FSeat& S : Seats)
			{
				S.Bet = 0;
				S.bActed = false;
				Able += S.CanAct() ? 1 : 0;
			}
			Turn = Able >= 2 ? NextSeat(Dealer, true) : -1;
			if (Turn < 0)
			{
				EndRound();
			}
		}

		void EndRound()
		{
			if (Phase == EPhase::Bet1)
			{
				Phase = EPhase::Draw;
				Turn = NextDrawer(Dealer);
				if (Turn < 0)
				{
					BeginRound(EPhase::Bet2);
				}
			}
			else
			{
				Phase = EPhase::Showdown;
				Turn = -1;
			}
		}

		int NextDrawer(int From) const
		{
			for (int K = 1; K <= NumSeats(); ++K)
			{
				const int I = (From + K) % NumSeats();
				if (Seats[(size_t)I].Live() && Seats[(size_t)I].Drew < 0)
				{
					return I;
				}
			}
			return -1;
		}

		void Apply(int Seat, EAct A)
		{
			FSeat& S = Seats[(size_t)Seat];
			const int Need = ToCall(Seat);
			if (A == EAct::BetRaise && !CanRaise(Seat))
			{
				A = EAct::CheckCall;
			}
			if (A == EAct::Fold && Need == 0)
			{
				A = EAct::CheckCall;   // nobody folds to no bet
			}
			if (A == EAct::Fold)
			{
				S.bFolded = true;
				Log.push_back({ EEv::Fold, Seat, 0, 0 });
			}
			else if (A == EAct::CheckCall)
			{
				const int Pay = std::min(Need, S.Stack);
				S.Stack -= Pay;
				S.Bet += Pay;
				S.Total += Pay;
				if (Pay > 0)
				{
					++S.Called;
					S.bVpipHand = S.bVpipHand || Phase == EPhase::Bet1;
				}
				Log.push_back({ Pay > 0 ? EEv::Call : EEv::Check, Seat, Pay, S.Stack == 0 ? 1 : 0 });
			}
			else
			{
				const int Pay = std::min(Need + Unit(), S.Stack);
				S.Stack -= Pay;
				S.Bet += Pay;
				S.Total += Pay;
				++S.Raised;
				S.bVpipHand = S.bVpipHand || Phase == EPhase::Bet1;
				Log.push_back({ Bets == 0 ? EEv::Bet : EEv::Raise, Seat, Pay, S.Stack == 0 ? 1 : 0 });
				CurrentBet = S.Bet;
				++Bets;
				for (FSeat& O : Seats)
				{
					O.bActed = false;
				}
			}
			S.bActed = true;
			if (LiveCount() <= 1)
			{
				Finish(false);
				return;
			}
			Turn = NextSeat(Seat, true);
			if (Turn < 0)
			{
				EndRound();
			}
		}

		void DoDraw(int Seat, int Mask)
		{
			FSeat& S = Seats[(size_t)Seat];
			int N = 0;
			for (int I = 0; I < 5; ++I)
			{
				N += (Mask >> I) & 1;
			}
			if (bMarked && S.Opp == Albert && N > 0)
			{
				StackDeckFor(S, Mask, N);
			}
			for (int I = 0; I < 5; ++I)
			{
				if ((Mask >> I) & 1)
				{
					S.Cards[I] = Deck[DeckAt++];
				}
			}
			S.Drew = N;
			S.Value = Eval5(S.Cards);
			Log.push_back({ EEv::Draw, Seat, N, 0 });
			Turn = NextDrawer(Seat);
			if (Turn < 0)
			{
				BeginRound(EPhase::Bet2);
			}
		}

		/** The marked deck: the N cards that make the most of the cards he keeps are brought to the top. */
		void StackDeckFor(const FSeat& S, int Mask, int N)
		{
			int Slots[3] = { 0, 0, 0 };
			int K = 0;
			for (int I = 0; I < 5 && K < 3; ++I)
			{
				if ((Mask >> I) & 1)
				{
					Slots[K++] = I;
				}
			}
			int Try[5];
			for (int I = 0; I < 5; ++I)
			{
				Try[I] = S.Cards[I];
			}
			uint32_t Best = 0;
			int Pick[3] = { DeckAt, DeckAt + 1, DeckAt + 2 };
			for (int A = DeckAt; A < 52; ++A)
			{
				Try[Slots[0]] = Deck[A];
				for (int B = N >= 2 ? A + 1 : 51; B < 52; ++B)
				{
					if (N >= 2)
					{
						Try[Slots[1]] = Deck[B];
					}
					for (int C = N >= 3 ? B + 1 : 51; C < 52; ++C)
					{
						if (N >= 3)
						{
							Try[Slots[2]] = Deck[C];
						}
						const uint32_t V = Eval5(Try);
						if (V > Best)
						{
							Best = V;
							Pick[0] = A;
							Pick[1] = B;
							Pick[2] = C;
						}
					}
				}
			}
			// ascending picks: each swap leaves the later ones where they were
			for (int I = 0; I < N; ++I)
			{
				std::swap(Deck[DeckAt + I], Deck[Pick[I]]);
			}
		}

		void ShowTell(int Seat, int True)
		{
			FSeat& S = Seats[(size_t)Seat];
			// a man keeps his face for a round of betting, unless what is behind it changes
			if (S.TellPhase == (int)Phase && S.TellTrue == True)
			{
				return;
			}
			const FStyle& St = StyleOf(S.Opp);
			int Shown = True;
			if (Rng.Uni() >= St.Honesty)
			{
				// a false face: the bluffer and the weak look strong, the strong look weak
				Shown = True == TellStrong ? TellWeak : TellStrong;
			}
			int Line = Rng.Below(TellLines);
			if (Shown == S.Tell && Line == S.TellLine)
			{
				Line = (Line + 1 + Rng.Below(TellLines - 1)) % TellLines;
			}
			S.Tell = Shown;
			S.TellLine = Line;
			S.TellTrue = True;
			S.TellPhase = (int)Phase;
			Log.push_back({ EEv::Tell, Seat, Shown, Line });
		}

		/** A regular's move, from his own cards, the pot and what the others did; OutTell: what he truly is (ETell). */
		EAct Decide(int Seat, int& OutTell)
		{
			FSeat& S = Seats[(size_t)Seat];
			const FStyle& St = StyleOf(S.Opp);
			const bool bSecond = Phase == EPhase::Bet2;
			double P = bSecond ? PostStrength(S.Value) : PreStrength(S.Value);
			if (!bSecond && CatOf(S.Value) == HighCard)
			{
				if (FourFlushOdd(S.Cards) >= 0)
				{
					P = std::max(P, 0.66);
				}
				else if (OpenStraightOdd(S.Cards) >= 0)
				{
					P = std::max(P, 0.62);
				}
			}
			const bool bStrong = P >= (bSecond ? 0.72 : 0.80);
			P = Clamp01(P + St.Noise * (Rng.Uni() + Rng.Uni() - 1.0));
			int Others = 0;
			int Pat = 0;
			for (int I = 0; I < NumSeats(); ++I)
			{
				if (I != Seat && Seats[(size_t)I].Live())
				{
					++Others;
					Pat += bSecond && Seats[(size_t)I].Drew == 0 ? 1 : 0;
				}
			}
			const int Need = ToCall(Seat);
			// a bet says something of the hand behind it, a raise more, a pat hand too: his own hand is weighed against the
			// hands that bet like that, as far as he respects them
			double Floor = 0.0;
			if (Need > 0)
			{
				Floor = Bets <= 1 ? 0.45 : (Bets == 2 ? 0.66 : 0.82);
			}
			if (Pat > 0)
			{
				Floor = std::max(Floor, 0.60);
			}
			Floor *= 0.55 + 0.45 * St.Respect;
			double Win = Clamp01((P - Floor) / (1.0 - Floor));
			for (int I = 1; I < Others; ++I)
			{
				Win *= P;
			}
			const int PotNow = Pot();
			const double BetAt = 0.28 * std::sqrt(St.Margin) + (1.0 - St.Aggr) * 0.25;
			const double U = Rng.Uni();
			const double V = Rng.Uni();
			OutTell = bStrong ? TellStrong : TellWeak;
			if (Need == 0)
			{
				if (Win >= BetAt && CanRaise(Seat))
				{
					// a strong hand is sometimes held back for a raise later
					if (bStrong && Win > 0.8 && U < 0.15 * (1.0 - St.Aggr))
					{
						return EAct::CheckCall;
					}
					return EAct::BetRaise;
				}
				if (!bStrong && CanRaise(Seat) && V < St.Bluff * (Others == 1 ? 2.0 : 1.0))
				{
					OutTell = TellBluff;
					++S.Bluffs;
					return EAct::BetRaise;
				}
				return EAct::CheckCall;
			}
			const double Odds = (double)Need / (double)(PotNow + Need);
			if (Win >= BetAt + 0.20 && CanRaise(Seat) && U < St.Aggr)
			{
				return EAct::BetRaise;
			}
			if (Win >= Odds * St.Margin)
			{
				return EAct::CheckCall;
			}
			if (!bStrong && CanRaise(Seat) && Bets <= 2 && V < St.Bluff * St.BluffRaise)
			{
				OutTell = TellBluff;
				++S.Bluffs;
				return EAct::BetRaise;
			}
			return EAct::Fold;
		}

		int DrawOf(int Seat)
		{
			const FSeat& S = Seats[(size_t)Seat];
			int Mask = SoundDraw(S.Cards);
			// by ear: a kicker kept with the pair now and then
			if (S.Opp == Blum && CatOf(S.Value) == OnePair && Rng.Uni() < 0.3)
			{
				for (int I = 0; I < 5; ++I)
				{
					if ((Mask >> I) & 1)
					{
						Mask &= ~(1 << I);
						break;
					}
				}
			}
			return Mask;
		}

		void Finish(bool bShowdown)
		{
			const int N = NumSeats();
			const int HeroBefore = Seats[0].Stack + Seats[0].Total;
			Result.Pot = Pot();
			Result.bShowdown = bShowdown;
			Result.HeroValue = Seats[0].Value;
			if (bShowdown)
			{
				for (int K = 1; K <= N; ++K)
				{
					const int I = (Dealer + K) % N;
					if (Seats[(size_t)I].Live())
					{
						Log.push_back({ EEv::Show, I, CatOf(Seats[(size_t)I].Value), IsRoyal(Seats[(size_t)I].Value) ? 1 : 0 });
					}
				}
			}
			// the pot by layers: nobody takes from another more than he put in himself
			std::vector<int> Levels;
			for (const FSeat& S : Seats)
			{
				if (S.bIn && S.Total > 0)
				{
					Levels.push_back(S.Total);
				}
			}
			std::sort(Levels.begin(), Levels.end());
			Levels.erase(std::unique(Levels.begin(), Levels.end()), Levels.end());
			int Prev = 0;
			for (const int L : Levels)
			{
				int Layer = 0;
				uint32_t Best = 0;
				int Winners = 0;
				bool bAny = false;
				for (const FSeat& S : Seats)
				{
					if (S.bIn)
					{
						Layer += std::min(S.Total, L) - std::min(S.Total, Prev);
						bAny = bAny || (S.Live() && S.Total >= L);
					}
				}
				for (const FSeat& S : Seats)
				{
					if (S.Live() && (S.Total >= L || !bAny) && S.Value >= Best)
					{
						Winners = S.Value > Best || Winners == 0 ? 1 : Winners + 1;
						Best = S.Value;
					}
				}
				int Left = Layer;
				int Share = Winners > 0 ? Layer / Winners : 0;
				for (int K = 1; K <= N && Winners > 0; ++K)
				{
					FSeat& S = Seats[(size_t)((Dealer + K) % N)];
					if (S.Live() && (S.Total >= L || !bAny) && S.Value == Best)
					{
						// the odd chips to the first winners from the dealer's left
						const int Take = Share + (Left - Share * Winners > 0 ? 1 : 0);
						S.Won += Take;
						Left -= Take;
						--Winners;
						Share = Winners > 0 ? Left / Winners : 0;
					}
				}
				Prev = L;
			}
			for (int I = 0; I < N; ++I)
			{
				FSeat& S = Seats[(size_t)I];
				if (!S.bIn)
				{
					continue;
				}
				S.Stack += S.Won;
				S.Vpip += S.bVpipHand ? 1 : 0;
				if (S.Won > S.Total)
				{
					Log.push_back({ EEv::Win, I, S.Won, 0 });
				}
			}
			const FSeat& Hero = Seats[0];
			Result.bHeroWon = Hero.bIn && Hero.Won > Hero.Total;
			Result.bHeroShown = bShowdown && Hero.Live();
			Result.bBluffWin = !bShowdown && Result.bHeroWon && CatOf(Hero.Value) == HighCard;
			Result.HeroNet = Hero.Stack - HeroBefore;
			if (bShowdown && Result.bHeroWon)
			{
				for (const FSeat& S : Seats)
				{
					if (S.Opp >= 0 && S.Live() && S.Value < Hero.Value)
					{
						Result.BeatenMask |= 1 << S.Opp;
					}
				}
			}
			if (bMarked && bShowdown)
			{
				const int A = SeatOf(Albert);
				if (A > 0 && Seats[(size_t)A].Live() && CatOf(Seats[(size_t)A].Value) >= FullHouse)
				{
					Log.push_back({ EEv::Suspect, A, 0, 0 });
				}
			}
			for (int I = 1; I < N; ++I)
			{
				FSeat& S = Seats[(size_t)I];
				if (S.bIn && S.Stack <= 0 && !S.bOut)
				{
					S.bOut = true;
					Log.push_back({ EEv::Left, I, 0, 0 });
				}
			}
			for (FSeat& S : Seats)
			{
				S.Put = S.bIn ? S.Total : 0;
				S.Total = 0;
				S.Bet = 0;
			}
			// a word between hands, more often than not
			if (Rng.Uni() < 0.7)
			{
				int Talkers[8];
				int NumTalkers = 0;
				for (int I = 1; I < N && NumTalkers < 8; ++I)
				{
					if (!Seats[(size_t)I].bOut)
					{
						Talkers[NumTalkers++] = I;
					}
				}
				if (NumTalkers > 0)
				{
					const int Who = Talkers[Rng.Below(NumTalkers)];
					Log.push_back({ EEv::Talk, Who, 0, Rng.Below(TalkLines) });
				}
			}
			Phase = EPhase::Over;
			Turn = -1;
		}
	};

	// ------------------------------------------------------------------------------------------------------------- the record

	/** What is kept with the save ("ShipPoker"). */
	struct FRecord
	{
		int Hands = 0;
		int PotsWon = 0;
		int BiggestPot = 0;                // roubles table only
		int BeatenMask = 0;                // 1 << EOpp
		int HandsMask = 0;                 // 1 << ECat shown; bit NumCats: a royal flush
		bool bSharp = false;
		bool bCaught = false;
		FNight Night;

		std::string ToString() const
		{
			char Buf[256];
			std::snprintf(Buf, sizeof(Buf), "v1;%d;%d;%d;%d;%d;%d;%d;%lld;%d;%d;%d;%d;%d;%d;%d", Hands, PotsWon, BiggestPot, BeatenMask, HandsMask,
				bSharp ? 1 : 0, bCaught ? 1 : 0, Night.Day, Night.Net, Night.Purse[0], Night.Purse[1], Night.Purse[2], Night.Purse[3],
				Night.Purse[4], Night.Purse[5]);
			return std::string(Buf);
		}

		/** False (and a fresh record) for anything that is not ours. */
		bool FromString(const std::string& In)
		{
			*this = FRecord();
			std::vector<long long> F;
			size_t At = 0;
			if (In.compare(0, 3, "v1;") != 0)
			{
				return false;
			}
			At = 3;
			while (At <= In.size())
			{
				const size_t End = std::min(In.find(';', At), In.size());
				F.push_back(std::strtoll(In.substr(At, End - At).c_str(), nullptr, 10));
				At = End + 1;
			}
			if (F.size() != 15)
			{
				return false;
			}
			Hands = (int)std::max(0ll, F[0]);
			PotsWon = (int)std::max(0ll, F[1]);
			BiggestPot = (int)std::max(0ll, F[2]);
			BeatenMask = (int)F[3] & ((1 << NumOpponents) - 1);
			HandsMask = (int)F[4] & ((1 << (NumCats + 1)) - 1);
			bSharp = F[5] != 0;
			bCaught = F[6] != 0;
			Night.Day = F[7];
			Night.Net = (int)F[8];
			for (int I = 0; I < NumOpponents; ++I)
			{
				Night.Purse[I] = (int)std::max(0ll, F[9 + I]);
			}
			return true;
		}

		/** A hand's result goes into the record. */
		void Note(const FResult& R, bool bRoubles)
		{
			if (R.bVoid || !R.bHeroIn)
			{
				return;
			}
			++Hands;
			PotsWon += R.bHeroWon ? 1 : 0;
			if (bRoubles && R.bHeroWon)
			{
				BiggestPot = std::max(BiggestPot, R.Pot);
			}
			BeatenMask |= R.BeatenMask;
			if (R.bHeroShown)
			{
				HandsMask |= 1 << (IsRoyal(R.HeroValue) ? (int)NumCats : CatOf(R.HeroValue));
			}
		}
	};
}
