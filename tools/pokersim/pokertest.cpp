// Offline checks of the ship's poker (Private/Cruise/PokerModel.h, docs/ship.md "Poker") and its strings
// (Content/Localization/poker). Run: bash tools/pokersim/run.sh test | play [seed] [hands] [lang] | stats
#include "Cruise/PokerModel.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>

using namespace LyudPoker;

static int Fails = 0;
static int Checks = 0;
#define CHECK(c, ...) do { ++Checks; if (!(c)) { ++Fails; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

// ------------------------------------------------------------------------------------------------------------ cards as text

/** "As Kd 7h 7c 2s" -> five cards (T for ten). */
static void Parse(const char* Text, int* Out)
{
	static const char* const Ranks = "23456789TJQKA";
	static const char* const Suits = "shdc";
	int N = 0;
	for (const char* P = Text; *P && N < 5; ++P)
	{
		const char* R = std::strchr(Ranks, *P);
		if (R && P[1])
		{
			const char* S = std::strchr(Suits, P[1]);
			Out[N++] = MakeCard((int)(R - Ranks) + 2, S ? (int)(S - Suits) : 0);
			++P;
		}
	}
}

static uint32_t Val(const char* Text)
{
	int C[5];
	Parse(Text, C);
	return Eval5(C);
}

static std::string CardText(int Card)
{
	static const char* const Ranks[13] = { "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A" };
	static const char* const Suits[4] = { "\xE2\x99\xA0", "\xE2\x99\xA5", "\xE2\x99\xA6", "\xE2\x99\xA3" };
	return std::string(Ranks[RankOf(Card) - 2]) + Suits[SuitOf(Card)];
}

static std::string HandText(const int* Cards)
{
	std::string S;
	for (int I = 0; I < 5; ++I)
	{
		S += (I ? " " : "") + CardText(Cards[I]);
	}
	return S;
}

// --------------------------------------------------------------------------------------- a slow evaluator written another way

/** Category and tie-break ranks by sorting and looking at neighbours (the classic if-chain), compared as a list. */
static std::vector<int> SlowEval(const int* Cards)
{
	int R[5];
	for (int I = 0; I < 5; ++I)
	{
		R[I] = RankOf(Cards[I]);
	}
	std::sort(R, R + 5, [](int A, int B) { return A > B; });
	bool bFlush = true;
	for (int I = 1; I < 5; ++I)
	{
		bFlush = bFlush && SuitOf(Cards[I]) == SuitOf(Cards[0]);
	}
	const bool bDistinct = R[0] != R[1] && R[1] != R[2] && R[2] != R[3] && R[3] != R[4];
	const bool bRun = bDistinct && R[0] - R[4] == 4;
	const bool bWheel = R[0] == 14 && R[1] == 5 && R[2] == 4 && R[3] == 3 && R[4] == 2;
	if (bRun || bWheel)
	{
		return { bFlush ? 8 : 4, bWheel ? 5 : R[0] };
	}
	if (R[0] == R[3]) return { 7, R[0], R[4] };
	if (R[1] == R[4]) return { 7, R[1], R[0] };
	if (R[0] == R[2] && R[3] == R[4]) return { 6, R[0], R[3] };
	if (R[0] == R[1] && R[2] == R[4]) return { 6, R[2], R[0] };
	if (bFlush) return { 5, R[0], R[1], R[2], R[3], R[4] };
	if (R[0] == R[2]) return { 3, R[0], R[3], R[4] };
	if (R[1] == R[3]) return { 3, R[1], R[0], R[4] };
	if (R[2] == R[4]) return { 3, R[2], R[0], R[1] };
	if (R[0] == R[1] && R[2] == R[3]) return { 2, R[0], R[2], R[4] };
	if (R[0] == R[1] && R[3] == R[4]) return { 2, R[0], R[3], R[2] };
	if (R[1] == R[2] && R[3] == R[4]) return { 2, R[1], R[3], R[0] };
	if (R[0] == R[1]) return { 1, R[0], R[2], R[3], R[4] };
	if (R[1] == R[2]) return { 1, R[1], R[0], R[3], R[4] };
	if (R[2] == R[3]) return { 1, R[2], R[0], R[1], R[4] };
	if (R[3] == R[4]) return { 1, R[3], R[0], R[1], R[2] };
	return { 0, R[0], R[1], R[2], R[3], R[4] };
}

static int Sign(long long V) { return V > 0 ? 1 : (V < 0 ? -1 : 0); }

static void TestEvaluator()
{
	// the categories
	struct FKnown { const char* Hand; int Cat; };
	const FKnown Known[] = {
		{ "As Ks Qs Js Ts", StraightFlush }, { "5h 4h 3h 2h Ah", StraightFlush }, { "9c 9d 9h 9s 2d", Four },
		{ "Kc Kd Kh 2s 2d", FullHouse }, { "Ah 9h 7h 4h 2h", Flush }, { "9c 8d 7h 6s 5d", Straight },
		{ "Ac 2d 3h 4s 5d", Straight }, { "Ac Kd Qh Js Td", Straight }, { "7c 7d 7h Ks 2d", Three },
		{ "Jc Jd 4h 4s Ad", TwoPair }, { "Tc Td 4h 8s Ad", OnePair }, { "Ac Jd 9h 4s 2d", HighCard },
		{ "Kc Ad 2h 3s 4d", HighCard },   // no straight round the corner
		{ "Qc Kd Ah 2s 3d", HighCard },
	};
	for (const FKnown& K : Known)
	{
		CHECK(CatOf(Val(K.Hand)) == K.Cat, "%s is %s, not %s", K.Hand, CatId(CatOf(Val(K.Hand))), CatId(K.Cat));
	}
	CHECK(IsRoyal(Val("As Ks Qs Js Ts")) && !IsRoyal(Val("Ks Qs Js Ts 9s")) && !IsRoyal(Val("Ac Kd Qh Js Td")), "the royal flush");
	CHECK(std::string(HandId(Val("Ad Kd Qd Jd Td"))) == "royal" && std::string(HandId(Val("9d Kd Qd Jd Td"))) == "straight_flush", "hand ids");

	// the order: each line beats the next (">") or ties with it ("=")
	struct FOrder { const char* A; char Op; const char* B; };
	const FOrder Order[] = {
		{ "As Ks Qs Js Ts", '>', "Ks Qs Js Ts 9s" }, { "6h 5h 4h 3h 2h", '>', "5h 4h 3h 2h Ah" }, { "5h 4h 3h 2h Ah", '>', "Ac Ad Ah As Kd" },
		{ "As Ks Qs Js Ts", '=', "Ah Kh Qh Jh Th" },
		{ "9c 9d 9h 9s 2d", '>', "8c 8d 8h 8s Ad" }, { "9c 9d 9h 9s Ad", '>', "9c 9d 9h 9s Kd" }, { "2c 2d 2h 2s 3d", '>', "Ac Ad Ah Ks Kd" },
		{ "Kc Kd Kh 2s 2d", '>', "Qc Qd Qh As Ad" }, { "Kc Kd Kh 3s 3d", '>', "Kc Kd Kh 2s 2d" }, { "2c 2d 2h 3s 3d", '>', "Ah Kh Qh Jh 9h" },
		{ "Ah 9h 7h 4h 2h", '>', "Kh Qh Jh Th 8h" }, { "Ah 9h 7h 4h 3h", '>', "Ad 9d 7d 4d 2d" }, { "Ah Kh 7h 4h 2h", '>', "Ah Qh Jh Th 9h" },
		{ "Ah 9h 7h 4h 2h", '=', "Ad 9d 7d 4d 2d" }, { "7h 5h 4h 3h 2h", '>', "Ac Kd Qh Js Td" },
		{ "Ac Kd Qh Js Td", '>', "Kc Qd Jh Ts 9d" }, { "6c 5d 4h 3s 2d", '>', "Ac 2d 3h 4s 5d" },   // the wheel is the lowest straight
		{ "Ac 2d 3h 4s 5d", '=', "5c 4d 3h 2s Ad" }, { "Ac 2d 3h 4s 5d", '>', "Ac Ad Ah Ks Qd" }, { "9c 8d 7h 6s 5d", '=', "9d 8h 7s 6c 5h" },
		{ "7c 7d 7h Ks 2d", '>', "6c 6d 6h As Kd" }, { "7c 7d 7h Ks 3d", '>', "7c 7d 7h Ks 2d" }, { "7c 7d 7h As 2d", '>', "7c 7d 7h Ks Qd" },
		{ "2c 2d 2h 4s 3d", '>', "Ac Ad Kh Ks Qd" },
		{ "Jc Jd 4h 4s Ad", '>', "Tc Td 9h 9s Ad" }, { "Jc Jd 5h 5s 2d", '>', "Jc Jd 4h 4s Ad" }, { "Jc Jd 4h 4s Ad", '>', "Jc Jd 4h 4s Kd" },
		{ "Jc Jd 4h 4s Ad", '=', "Jh Js 4c 4d Ah" }, { "3c 3d 2h 2s 4d", '>', "Ac Ad Kh Qs Jd" },
		{ "Tc Td 4h 8s Ad", '>', "9c 9d Ah Ks Qd" }, { "Tc Td Ah 8s 4d", '>', "Tc Td Kh Qs Jd" }, { "Tc Td Ah 8s 4d", '>', "Tc Td Ah 7s 6d" },
		{ "Tc Td Ah 8s 4d", '>', "Tc Td Ah 8s 3d" }, { "Tc Td Ah 8s 4d", '=', "Th Ts Ac 8d 4h" }, { "2c 2d 3h 4s 5d", '>', "Ac Kd Qh Js 9d" },
		{ "Ac Jd 9h 4s 2d", '>', "Kc Qd Jh Ts 8d" }, { "Ac Jd 9h 4s 3d", '>', "Ac Jd 9h 4s 2d" }, { "Ac Jd 9h 5s 2d", '>', "Ac Jd 9h 4s 3d" },
		{ "Ac Qd 9h 4s 2d", '>', "Ac Jd Th 9s 8d" }, { "Ac Jd 9h 4s 2d", '=', "Ad Jh 9s 4c 2h" }, { "7c 5d 4h 3s 2d", '<', "8c 5d 4h 3s 2d" },
	};
	for (const FOrder& O : Order)
	{
		const uint32_t A = Val(O.A), B = Val(O.B);
		const char Got = A > B ? '>' : (A < B ? '<' : '=');
		CHECK(Got == O.Op, "%s %c %s (got %c)", O.A, O.Op, O.B, Got);
	}

	// 100 000 random pairs of hands against the slow evaluator
	FRng R;
	R.S = 20261127;
	int Seen[NumCats] = { 0 };
	int Bad = 0;
	for (int T = 0; T < 100000; ++T)
	{
		int Deck[52];
		for (int I = 0; I < 52; ++I)
		{
			Deck[I] = I;
		}
		for (int I = 0; I < 10; ++I)
		{
			std::swap(Deck[I], Deck[I + R.Below(52 - I)]);
		}
		// every now and then a made-up strong hand, so the rare categories meet each other too
		if (T % 4 == 0)
		{
			const int Rank = 2 + R.Below(9);
			const int Suit = R.Below(4);
			for (int I = 0; I < 5; ++I)
			{
				Deck[I] = MakeCard(T % 8 == 0 ? Rank + I : 2 + R.Below(13), T % 8 == 0 && T % 16 != 0 ? R.Below(4) : Suit);
			}
			if (T % 16 == 0 && Rank == 2)
			{
				Deck[4] = MakeCard(14, Suit);   // a wheel
				for (int I = 0; I < 4; ++I)
				{
					Deck[I] = MakeCard(2 + I, T % 32 == 0 ? Suit : R.Below(4));
				}
			}
			bool bDup = false;
			for (int I = 0; I < 5; ++I)
			{
				for (int J = I + 1; J < 5; ++J)
				{
					bDup = bDup || Deck[I] == Deck[J];
				}
			}
			if (bDup)
			{
				continue;
			}
		}
		if (T % 50 == 1)
		{
			// four of a kind, or three and a pair, against whatever comes
			const int Rank = 2 + R.Below(13);
			const int Other = 2 + (Rank - 2 + 1 + R.Below(12)) % 13;
			for (int I = 0; I < 5; ++I)
			{
				Deck[I] = I < (T % 100 == 1 ? 4 : 3) ? MakeCard(Rank, I) : MakeCard(Other, I - 3);
			}
		}
		const uint32_t A = Eval5(Deck), B = Eval5(Deck + 5);
		const std::vector<int> SA = SlowEval(Deck), SB = SlowEval(Deck + 5);
		++Seen[CatOf(A)];
		const int Fast = Sign((long long)A - (long long)B);
		const int Slow = SA > SB ? 1 : (SA < SB ? -1 : 0);
		if (Fast != Slow || CatOf(A) != SA[0] || CatOf(B) != SB[0])
		{
			if (++Bad <= 5)
			{
				std::printf("  %s (%s) vs %s (%s): fast %d slow %d\n", HandText(Deck).c_str(), CatId(CatOf(A)), HandText(Deck + 5).c_str(), CatId(CatOf(B)), Fast, Slow);
			}
		}
	}
	CHECK(Bad == 0, "%d random pairs judged differently by the slow evaluator", Bad);
	for (int C = 0; C < NumCats; ++C)
	{
		CHECK(Seen[C] > 20, "the random deals had only %d of %s", Seen[C], CatId(C));
	}
	// every 5-card hand of the deck once: the categories' counts are the textbook's
	{
		long long Count[NumCats] = { 0 };
		int C[5];
		for (C[0] = 0; C[0] < 52; ++C[0]) for (C[1] = C[0] + 1; C[1] < 52; ++C[1]) for (C[2] = C[1] + 1; C[2] < 52; ++C[2])
			for (C[3] = C[2] + 1; C[3] < 52; ++C[3]) for (C[4] = C[3] + 1; C[4] < 52; ++C[4])
			{
				++Count[CatOf(Eval5(C))];
			}
		const long long Book[NumCats] = { 1302540, 1098240, 123552, 54912, 10200, 5108, 3744, 624, 40 };
		for (int K = 0; K < NumCats; ++K)
		{
			CHECK(Count[K] == Book[K], "%s: %lld hands in the deck, the book says %lld", CatId(K), Count[K], Book[K]);
		}
	}
	// the draw a sound player makes
	int H[5];
	Parse("Kc Kd 7h 4s 2d", H);
	CHECK(SoundDraw(H) == 0x1C, "a pair keeps the pair (mask %x)", SoundDraw(H));
	Parse("Kc Kd 7h 7s 2d", H);
	CHECK(SoundDraw(H) == 0x10, "two pair change one (mask %x)", SoundDraw(H));
	Parse("Kc Kd Kh 7s 2d", H);
	CHECK(SoundDraw(H) == 0x18, "three change two (mask %x)", SoundDraw(H));
	Parse("9c 8d 7h 6s 5d", H);
	CHECK(SoundDraw(H) == 0, "a straight stands pat");
	Parse("Ah 9h 7h 4h 2c", H);
	CHECK(SoundDraw(H) == 0x10, "four to a flush change the odd card (mask %x)", SoundDraw(H));
	Parse("9c 8d 7h 6s Kd", H);
	CHECK(SoundDraw(H) == 0x10, "four in a row change the odd card (mask %x)", SoundDraw(H));
	Parse("Ac Jd 9h 4s 2d", H);
	CHECK(SoundDraw(H) == 0x1C, "nothing keeps the two highest (mask %x)", SoundDraw(H));
}

// --------------------------------------------------------------------------------------------------------- scripted heroes

enum class EHero { Random, Tight, Station, Solid, Reader };

/** The seat that bet or raised last in this round (-1: nobody). */
static int LastAggressor(const FTable& T)
{
	for (size_t I = T.Log.size(); I-- > 0;)
	{
		const FEvent& E = T.Log[I];
		if (E.Type == EEv::Draw || E.Type == EEv::Deal)
		{
			break;
		}
		if (E.Type == EEv::Bet || E.Type == EEv::Raise)
		{
			return E.Seat;
		}
	}
	return -1;
}

static EAct HeroMove(const FTable& T, EHero Kind, FRng& R)
{
	const FSeat& H = T.Seats[0];
	const bool bSecond = T.Phase == EPhase::Bet2;
	const int Need = T.ToCall(0);
	double P = bSecond ? PostStrength(H.Value) : PreStrength(H.Value);
	if (!bSecond && CatOf(H.Value) == HighCard && (FourFlushOdd(H.Cards) >= 0 || OpenStraightOdd(H.Cards) >= 0))
	{
		P = 0.64;
	}
	switch (Kind)
	{
	case EHero::Random: return (EAct)R.Below(3);
	case EHero::Station: return EAct::CheckCall;
	case EHero::Tight: return P >= 0.88 ? EAct::BetRaise : (Need == 0 ? EAct::CheckCall : EAct::Fold);
	default: break;
	}
	// a sound, plain game
	double CallAt = bSecond ? 0.50 : 0.62;
	double BetAt = bSecond ? 0.72 : 0.82;
	const double RaiseAt = bSecond ? 0.90 : 0.94;
	if (Kind == EHero::Reader)
	{
		// he knows his people: Альберт's face says nothing, the others' mostly the truth
		const int Agg = LastAggressor(T);
		if (Need > 0 && Agg > 0 && StyleOf(T.Seats[(size_t)Agg].Opp).Honesty >= 0.6)
		{
			const int Tell = T.Seats[(size_t)Agg].Tell;
			if (Tell == TellBluff)
			{
				CallAt = 0.0;
				BetAt = 0.0;
				if (P >= (bSecond ? 0.26 : 0.50))
				{
					return EAct::BetRaise;
				}
			}
			else if (Tell == TellStrong)
			{
				CallAt = bSecond ? 0.74 : 0.80;
			}
			else if (Tell == TellWeak)
			{
				CallAt = bSecond ? 0.36 : 0.55;
			}
		}
		if (Need == 0)
		{
			// everybody left looks weak: a thinner bet
			bool bAllWeak = true;
			for (int I = 1; I < T.NumSeats(); ++I)
			{
				const FSeat& S = T.Seats[(size_t)I];
				if (S.Live())
				{
					bAllWeak = bAllWeak && S.Tell == TellWeak && StyleOf(S.Opp).Honesty >= 0.6;
				}
			}
			if (bAllWeak)
			{
				BetAt = bSecond ? 0.55 : 0.70;
			}
		}
	}
	if (Need == 0)
	{
		return P >= BetAt ? EAct::BetRaise : EAct::CheckCall;
	}
	if (P >= RaiseAt)
	{
		return EAct::BetRaise;
	}
	return P >= CallAt ? EAct::CheckCall : EAct::Fold;
}

static int HeroDrawMask(const FTable& T, EHero Kind, FRng& R)
{
	if (Kind == EHero::Random)
	{
		return R.Below(32);
	}
	return SoundDraw(T.Seats[0].Cards);
}

/** Plays the hand dealt to its end; Each (if given) sees the table after every step. */
template <typename FEach>
static void PlayHand(FTable& T, EHero Kind, FRng& R, FEach Each)
{
	int Guard = 0;
	while (T.Phase != EPhase::Over && ++Guard < 2000)
	{
		if (T.WaitingHero())
		{
			if (T.Phase == EPhase::Draw)
			{
				T.HeroDraw(HeroDrawMask(T, Kind, R));
			}
			else
			{
				T.HeroAct(HeroMove(T, Kind, R));
			}
		}
		else
		{
			T.Step();
		}
		Each(T);
	}
	CHECK(Guard < 2000, "a hand that never ends (phase %d)", (int)T.Phase);
}

static std::vector<int> FourOf(FRng& R)
{
	std::vector<int> All = { 0, 1, 2, 3, 4, 5 };
	for (int I = 5; I > 0; --I)
	{
		std::swap(All[(size_t)I], All[(size_t)R.Below(I + 1)]);
	}
	All.resize((size_t)(3 + R.Below(2)));
	return All;
}

// ------------------------------------------------------------------------------------------------------------------ the checks

static void TestConservation()
{
	// 10 000 sittings: no chip appears or vanishes, no stack below zero, nobody wins from another more than he put in
	FRng R;
	R.S = 4242;
	long long Hands = 0, AllIns = 0, Splits = 0, Showdowns = 0;
	int BadChips = 0, Negative = 0, OverWin = 0, Unfinished = 0;
	for (int Sitting = 0; Sitting < 10000; ++Sitting)
	{
		const EHero Kind = (EHero)(Sitting % 3);
		FStakes St = Sitting % 2 ? FStakes::Roubles() : FStakes::Matches();
		int Purses[NumOpponents];
		for (int& P : Purses)
		{
			P = Sitting % 7 == 0 ? 3 + R.Below(12) : St.Purse;   // short stacks: all-ins
		}
		if (Sitting % 5 == 0)
		{
			St.BuyIn = 4 + R.Below(8);
		}
		FTable T;
		T.Sit(St, FourOf(R), Purses, 1 + Sitting / 50, 1 + Sitting % 2, Sitting * 31);
		const int Total = T.Chips();
		while (T.StartHand())
		{
			++Hands;
			PlayHand(T, Kind, R, [&](const FTable& Now)
			{
				BadChips += Now.Chips() != Total ? 1 : 0;
				for (const FSeat& S : Now.Seats)
				{
					Negative += S.Stack < 0 || S.Bet < 0 || S.Total < 0 ? 1 : 0;
				}
			});
			Showdowns += T.Result.bShowdown ? 1 : 0;
			CHECK(T.Phase == EPhase::Over && T.Chips() == Total, "after a hand: phase %d, %d chips of %d", (int)T.Phase, T.Chips(), Total);
			int Winners = 0;
			for (const FSeat& S : T.Seats)
			{
				if (!S.bIn)
				{
					continue;
				}
				int Cap = 0;
				for (const FSeat& O : T.Seats)
				{
					Cap += O.bIn ? std::min(O.Put, S.Put) : 0;
				}
				OverWin += S.Won > Cap ? 1 : 0;
				AllIns += S.Stack == S.Won && S.Put > 0 && S.Live() ? 1 : 0;
				Winners += S.Won > S.Put ? 1 : 0;
			}
			Splits += Winners > 1 ? 1 : 0;
		}
		Unfinished += T.Ended() == EEnd::None ? 1 : 0;
		CHECK(T.HandNo <= St.MaxHands, "a sitting of %d hands", T.HandNo);
	}
	CHECK(BadChips == 0, "chips not conserved at %d steps", BadChips);
	CHECK(Negative == 0, "%d negative stacks or bets", Negative);
	CHECK(OverWin == 0, "%d times somebody won more than he could cover", OverWin);
	CHECK(Unfinished == 0, "%d sittings stopped for no reason", Unfinished);
	CHECK(Hands > 100000 && AllIns > 500 && Splits > 20 && Showdowns > 20000, "coverage: %lld hands, %lld all-ins, %lld split pots, %lld showdowns",
		Hands, AllIns, Splits, Showdowns);
	std::printf("conservation: %lld hands, %lld showdowns, %lld all-in hands, %lld split or side pots\n", Hands, Showdowns, AllIns, Splits);

	// getting up in the middle of a hand, at any moment: his cards are folded, the hand is played out, nothing is lost
	int Left = 0;
	for (int Seed = 0; Seed < 6000; ++Seed)
	{
		FTable T;
		T.Sit(Seed % 2 ? FStakes::Roubles() : FStakes::Matches(), FourOf(R), nullptr, 77, 1, Seed * 7);
		const int Total = T.Chips();
		const int Warm = 1 + R.Below(3);
		for (int H = 0; H < Warm && T.StartHand(); ++H)
		{
			PlayHand(T, EHero::Station, R, [](const FTable&) {});
		}
		if (T.StartHand())
		{
			const int At = R.Below(14);
			const int Before = T.Seats[0].Stack + T.Seats[0].Total;
			for (int Step = 0; Step < At && T.Phase != EPhase::Over; ++Step)
			{
				if (T.WaitingHero())
				{
					if (T.Phase == EPhase::Draw)
					{
						T.HeroDraw(SoundDraw(T.Seats[0].Cards));
					}
					else
					{
						T.HeroAct(EAct::CheckCall);
					}
				}
				else
				{
					T.Step();
				}
			}
			const bool bWasOver = T.Phase == EPhase::Over;
			T.HeroLeave();
			++Left;
			CHECK(T.Phase == EPhase::Over && T.Chips() == Total, "leaving at step %d: phase %d, %d chips of %d", At, (int)T.Phase, T.Chips(), Total);
			CHECK(bWasOver || T.Result.bShowdown || T.Seats[0].Stack <= Before, "leaving at step %d won him chips (%d from %d)", At, T.Seats[0].Stack, Before);
			for (const FSeat& S : T.Seats)
			{
				CHECK(S.Stack >= 0, "leaving: a stack of %d", S.Stack);
			}
		}
	}
	CHECK(Left > 5000, "left %d hands", Left);

	// the all-in rule on a made case: the hero with 3 chips against full stacks can take 3 from each, never more
	int Short = 0;
	for (int Seed = 0; Seed < 4000; ++Seed)
	{
		FTable T;
		FStakes St = FStakes::Matches();
		St.BuyIn = 3;
		T.Sit(St, { Givi, Taras, Lev }, nullptr, 9, 1, Seed * 13);
		T.StartHand();
		PlayHand(T, EHero::Station, R, [](const FTable&) {});
		const FSeat& H = T.Seats[0];
		if (H.Put == 3 && T.Result.bShowdown)
		{
			++Short;
			CHECK(H.Won <= 3 * T.NumSeats(), "all-in for 3 took %d", H.Won);
			if (H.Won > H.Put)
			{
				// and the rest of the pot went to the best of the others, not to him
				CHECK(T.Result.Pot >= H.Won, "pot %d, he took %d", T.Result.Pot, H.Won);
			}
		}
	}
	CHECK(Short > 300, "only %d short all-in showdowns", Short);
}

static void TestNight()
{
	// an evening at the roubles table: he sits again and again while he may; never more than 60 down, never more won than
	// the regulars brought
	FRng R;
	R.S = 99;
	int Worst = 0, Best = 0;
	for (int Night = 0; Night < 3000; ++Night)
	{
		const FStakes St = FStakes::Roubles();
		FNight N;
		N.Roll(1000 + Night, St);
		const EHero Kind = (EHero)(Night % 4);
		int Pocket = 500;
		int Sittings = 0;
		while (N.CanBuyIn(St) && Sittings < 12)
		{
			const std::vector<int> Who = Lineup(1 + Night % 2, 22.0 + Sittings * 0.4, true, false);
			FTable T;
			T.Sit(St, Who, N.Purse, Night, 1, Sittings * 100);
			if (T.NumSeats() < 2)
			{
				break;
			}
			Pocket -= St.BuyIn;
			N.Net += St.BuyIn;
			while (T.StartHand())
			{
				PlayHand(T, Kind, R, [](const FTable&) {});
			}
			Pocket += T.Seats[0].Stack;
			N.Net -= T.Seats[0].Stack;
			T.Purses(N.Purse);
			++Sittings;
		}
		CHECK(N.Net == 500 - Pocket, "the night's count %d, the pocket says %d", N.Net, 500 - Pocket);
		CHECK(N.Net <= St.LossCap, "night %d: %d roubles lost", Night, N.Net);
		CHECK(-N.Net <= NumOpponents * St.Purse, "night %d: %d roubles won from purses of %d", Night, -N.Net, NumOpponents * St.Purse);
		int Left = 0;
		for (const int P : N.Purse)
		{
			CHECK(P >= 0, "a purse of %d", P);
			Left += P;
		}
		CHECK(Left + (-N.Net) == NumOpponents * St.Purse, "night %d: the purses hold %d, he is up %d", Night, Left, -N.Net);
		Worst = std::max(Worst, N.Net);
		Best = std::max(Best, -N.Net);
	}
	CHECK(Worst == 60, "the worst night lost %d (the cap is 60 and a careless hero reaches it)", Worst);
	std::printf("nights: the worst lost %d roubles, the best won %d\n", Worst, Best);
	// the evening rolls over at noon, not at midnight
	CHECK(FNight::EveningOf(10, 23.5) == 10 && FNight::EveningOf(11, 0.5) == 10 && FNight::EveningOf(11, 13.0) == 11, "the small hours belong to the evening before");
	FNight N;
	const FStakes St = FStakes::Roubles();
	N.Roll(5, St);
	N.Net = 31;
	CHECK(!N.CanBuyIn(St), "31 down: a buy-in of 30 would pass the cap");
	N.Net = 30;
	CHECK(N.CanBuyIn(St), "30 down: one more buy-in");
	N.Roll(5, St);
	CHECK(N.Net == 30, "the same evening keeps its count");
	N.Roll(6, St);
	CHECK(N.Net == 0 && N.Purse[0] == St.Purse, "a new evening starts clean");
	CHECK(FStakes::Matches().LossCap == 0 && FNight().CanBuyIn(FStakes::Matches()), "matches are free");
}

static std::string LogText(const FTable& T)
{
	std::string S;
	char Buf[64];
	for (const FEvent& E : T.Log)
	{
		std::snprintf(Buf, sizeof(Buf), "%d:%d:%d:%d;", (int)E.Type, E.Seat, E.A, E.B);
		S += Buf;
	}
	for (const FSeat& P : T.Seats)
	{
		std::snprintf(Buf, sizeof(Buf), "[%d %d]", P.Stack, (int)P.Value);
		S += Buf;
	}
	return S;
}

static std::string RunSitting(long long Voyage, int Day, int Base, EHero Kind, bool bSharp = false)
{
	FTable T;
	T.bSharp = bSharp;
	T.Sit(FStakes::Roubles(), { Albert, Givi, Vera, Taras }, nullptr, Voyage, Day, Base);
	FRng R;
	R.S = 5;
	std::string All;
	while (T.StartHand())
	{
		PlayHand(T, Kind, R, [](const FTable&) {});
		All += LogText(T);
	}
	return All;
}

static void TestDeterminism()
{
	const std::string A = RunSitting(3, 1, 0, EHero::Solid);
	CHECK(A == RunSitting(3, 1, 0, EHero::Solid), "the same seed plays the same evening");
	CHECK(A != RunSitting(4, 1, 0, EHero::Solid), "another voyage, another evening");
	CHECK(A != RunSitting(3, 2, 0, EHero::Solid), "another day, another evening");
	CHECK(A != RunSitting(3, 1, 24, EHero::Solid), "a later sitting deals other cards");
	CHECK(HandSeed(1, 1, 1) != HandSeed(1, 1, 2) && HandSeed(1, 1, 1) != HandSeed(1, 2, 1) && HandSeed(1, 1, 1) != HandSeed(2, 1, 1), "hand seeds");
	// the regulars never look at the hero's cards: with his cards swapped for others (same deck order elsewhere) their
	// moves up to his first move are the same
	int Same = 0, Tried = 0;
	for (int Seed = 0; Seed < 300; ++Seed)
	{
		FTable A1, B1;
		A1.Sit(FStakes::Matches(), { Givi, Vera, Taras, Lev }, nullptr, 50, 1, Seed);
		B1.Sit(FStakes::Matches(), { Givi, Vera, Taras, Lev }, nullptr, 50, 1, Seed);
		A1.StartHand();
		B1.StartHand();
		// B's hero gets the five cards from the bottom of the deck instead
		for (int I = 0; I < 5; ++I)
		{
			B1.Seats[0].Cards[I] = B1.Deck[47 + I];
		}
		B1.Seats[0].Value = Eval5(B1.Seats[0].Cards);
		while (!A1.WaitingHero() && A1.Step()) {}
		while (!B1.WaitingHero() && B1.Step()) {}
		++Tried;
		Same += A1.Log.size() == B1.Log.size() ? 1 : 0;
		for (size_t I = 0; I < std::min(A1.Log.size(), B1.Log.size()); ++I)
		{
			CHECK(A1.Log[I].Type == B1.Log[I].Type && A1.Log[I].A == B1.Log[I].A, "seed %d: a regular's move depends on the hero's cards", Seed);
		}
	}
	CHECK(Same == Tried, "%d of %d openings the same whatever the hero holds", Same, Tried);
}

struct FStat { long long Hands = 0, Vpip = 0, Raised = 0, Called = 0, Bluffs = 0, Net = 0; };

static void Styles(FStat* Out, int Sittings, bool bPrint, EHero Kind = EHero::Solid)
{
	long long HeroHands = 0, HeroNet = 0;
	FRng R;
	R.S = 777;
	for (int Sitting = 0; Sitting < Sittings; ++Sitting)
	{
		FTable T;
		const std::vector<int> Who = FourOf(R);
		T.Sit(FStakes::Matches(), Who, nullptr, 100 + Sitting, 1, Sitting * 37);
		while (T.StartHand())
		{
			PlayHand(T, Kind, R, [](const FTable&) {});
		}
		HeroHands += T.Seats[0].Hands;
		HeroNet += T.Seats[0].Stack - FStakes::Matches().BuyIn;
		for (const FSeat& S : T.Seats)
		{
			if (S.Opp >= 0)
			{
				FStat& O = Out[S.Opp];
				O.Hands += S.Hands;
				O.Vpip += S.Vpip;
				O.Raised += S.Raised;
				O.Called += S.Called;
				O.Bluffs += S.Bluffs;
				O.Net += S.Stack - FStakes::Matches().Purse;
			}
		}
	}
	if (bPrint)
	{
		std::printf("%-12s %8s %7s %7s %8s %9s\n", "regular", "hands", "VPIP", "aggr", "bluffs", "net/100");
		for (int O = 0; O < NumOpponents; ++O)
		{
			const FStat& S = Out[O];
			std::printf("%-12s %8lld %6.1f%% %6.1f%% %7.2f%% %+9.1f\n", OppId(O), S.Hands, 100.0 * S.Vpip / S.Hands,
				100.0 * S.Raised / std::max(1ll, S.Raised + S.Called), 100.0 * S.Bluffs / S.Hands, 100.0 * S.Net / S.Hands);
		}
		std::printf("%-12s %8lld %42.1f\n", "the hero", HeroHands, 100.0 * HeroNet / std::max(1ll, HeroHands));
	}
}

static void TestStyles(bool bPrint)
{
	FStat S[NumOpponents];
	Styles(S, 6000, bPrint);
	auto Vpip = [&S](int O) { return 100.0 * S[O].Vpip / S[O].Hands; };
	auto Aggr = [&S](int O) { return 100.0 * S[O].Raised / std::max(1ll, S[O].Raised + S[O].Called); };
	auto Bluff = [&S](int O) { return 100.0 * S[O].Bluffs / S[O].Hands; };
	// the bands each style is meant to stay in (percent): VPIP low/high, aggression low/high
	struct FBand { int Opp; double V0, V1, A0, A1; };
	const FBand Bands[] = {
		{ Albert, 18, 34, 62, 88 },   // tight, aggressive
		{ Givi, 48, 72, 25, 50 },     // loose, calls along
		{ Vera, 24, 42, 45, 70 },     // careful
		{ Taras, 50, 72, 58, 82 },    // bold
		{ Blum, 40, 60, 45, 70 },     // by ear
		{ Lev, 27, 45, 50, 74 },      // by the odds
	};
	for (const FBand& B : Bands)
	{
		CHECK(S[B.Opp].Hands > 20000, "%s played %lld hands", OppId(B.Opp), S[B.Opp].Hands);
		CHECK(Vpip(B.Opp) >= B.V0 && Vpip(B.Opp) <= B.V1, "%s: VPIP %.1f%% outside %.0f..%.0f", OppId(B.Opp), Vpip(B.Opp), B.V0, B.V1);
		CHECK(Aggr(B.Opp) >= B.A0 && Aggr(B.Opp) <= B.A1, "%s: aggression %.1f%% outside %.0f..%.0f", OppId(B.Opp), Aggr(B.Opp), B.A0, B.A1);
	}
	CHECK(std::min(Vpip(Givi), Vpip(Taras)) > Vpip(Blum) + 4 && Vpip(Blum) > Vpip(Lev) + 4 && Vpip(Lev) > Vpip(Albert) + 4,
		"who plays the most hands: Гиви and Тарас, then Блюм, Лев Маркович, and Альберт the fewest");
	CHECK(Vpip(Vera) < Vpip(Blum), "Вера Игнатьевна is more careful than Блюм");
	CHECK(Aggr(Albert) > Aggr(Vera) && Aggr(Taras) > Aggr(Blum) && Aggr(Givi) < Aggr(Blum), "who pushes: Альберт and Тарас; Гиви calls along");
	CHECK(Bluff(Taras) > 2.0 * Bluff(Vera) && Bluff(Taras) > Bluff(Blum) && Bluff(Blum) > Bluff(Lev), "Тарас bluffs the most, then Блюм");
	CHECK(Bluff(Albert) < 0.8 * Bluff(Vera) && Bluff(Albert) > 0.5, "Альберт bluffs rarely (%.2f%%)", Bluff(Albert));
	CHECK(S[Albert].Net > 0 && S[Vera].Net > 0 && S[Lev].Net > 0 && S[Givi].Net < 0 && S[Taras].Net < 0, "the careful ones win in the long run, Гиви and Тарас pay for the evening");
}

static void TestTells(bool bPrint)
{
	// the tells carry real news: what a regular's face says against what he truly held, for each of them
	FRng R;
	R.S = 31337;
	long long Shown[NumOpponents][NumTells] = {}, True[NumOpponents][NumTells] = {};
	// and a hero who reads them beats one who does not, over the same 5000 deals, on several sets of deals
	int ReaderAhead = 0;
	long long SumRead = 0, SumPlain = 0;
	const int Sets = 6;
	for (int Set = 0; Set < Sets; ++Set)
	{
		long long Net[2] = { 0, 0 };
		for (int Who = 0; Who < 2; ++Who)
		{
			const EHero Kind = Who == 0 ? EHero::Solid : EHero::Reader;
			FRng Pick;
			Pick.S = 1000 + Set;
			int Hands = 0;
			for (int Sitting = 0; Hands < 5000; ++Sitting)
			{
				FTable T;
				T.Sit(FStakes::Matches(), FourOf(Pick), nullptr, 7000 + Set, 1, Sitting * 50);
				int Before = T.Seats[0].Stack;
				while (Hands < 5000 && T.StartHand())
				{
					++Hands;
					PlayHand(T, Kind, R, [](const FTable&) {});
					Net[Who] += T.Seats[0].Stack - Before;
					Before = T.Seats[0].Stack;
					if (Who == 0)
					{
						// the truth of a tell: a shown "bluff"/"weak" bettor should hold worse cards than a shown "strong" one
						for (const FSeat& S : T.Seats)
						{
							if (S.Opp >= 0 && S.Tell >= 0 && S.bIn)
							{
								++Shown[S.Opp][S.Tell];
								True[S.Opp][S.Tell] += CatOf(S.Value) >= TwoPair ? 1 : 0;
							}
						}
					}
				}
			}
		}
		ReaderAhead += Net[1] > Net[0] ? 1 : 0;
		SumPlain += Net[0];
		SumRead += Net[1];
		if (bPrint)
		{
			std::printf("tells set %d: plain hero %+lld, reader %+lld chips over 5000 hands\n", Set, Net[0], Net[1]);
		}
	}
	CHECK(ReaderAhead == Sets, "the hero who reads the tells was ahead in only %d of %d sets of 5000 hands", ReaderAhead, Sets);
	CHECK(SumRead > SumPlain + 1500 * Sets / 6, "reading the tells is worth little: %+lld against %+lld", SumRead, SumPlain);
	std::printf("tells: a hero who reads them %+.1f chips per 100 hands, one who does not %+.1f\n", 100.0 * SumRead / (5000.0 * Sets), 100.0 * SumPlain / (5000.0 * Sets));
	for (int O = 0; O < NumOpponents; ++O)
	{
		const double Strong = (double)True[O][TellStrong] / std::max(1ll, Shown[O][TellStrong]);
		const double Weak = (double)True[O][TellWeak] / std::max(1ll, Shown[O][TellWeak]);
		if (bPrint)
		{
			std::printf("  %-12s looks strong: two pair or better %4.1f%% of the time; looks weak: %4.1f%%; tells %lld/%lld/%lld\n", OppId(O),
				100.0 * Strong, 100.0 * Weak, Shown[O][0], Shown[O][1], Shown[O][2]);
		}
		CHECK(Shown[O][TellStrong] > 200 && Shown[O][TellWeak] > 200, "%s shows too few tells", OppId(O));
		if (O == Albert)
		{
			CHECK(Strong < Weak + 0.12, "Альберт's face should say next to nothing (strong %.2f, weak %.2f)", Strong, Weak);
		}
		else
		{
			CHECK(Strong > Weak + 0.08, "%s: a strong look should mean better cards (strong %.2f, weak %.2f)", OppId(O), Strong, Weak);
		}
	}
	CHECK(Shown[Taras][TellBluff] > 3 * Shown[Vera][TellBluff], "Тарас's bluffs show on his face far more often than Вера Игнатьевна's");
}

static void TestSharp()
{
	// the marked deck: only on Альберт's own deal, his draws come out far too well, and watching catches him only then
	FRng R;
	R.S = 2026;
	long long HisDeals = 0, Marked = 0, Big[2] = { 0, 0 }, Shows[2] = { 0, 0 }, Suspects = 0;
	int CaughtWrong = 0, NotNowWrong = 0, Caught = 0, Nothing = 0;
	for (int Sitting = 0; Sitting < 3000; ++Sitting)
	{
		FTable T;
		T.bSharp = Sitting % 2 == 0;
		T.Sit(FStakes::Matches(), { Albert, Givi, Vera, Lev }, nullptr, 300 + Sitting, 2, Sitting * 41);
		const int Total = T.Chips();
		const int Watch = Sitting % 3;   // 0: never; 1: at every moment of every hand; 2: when he deals
		while (T.StartHand())
		{
			const bool bHis = T.Seats[(size_t)T.Dealer].Opp == Albert;
			const bool bMarkedNow = T.bMarked;
			HisDeals += bHis && T.bSharp ? 1 : 0;
			Marked += bMarkedNow ? 1 : 0;
			CHECK(!bMarkedNow || (bHis && T.bSharp), "a marked hand not dealt by Альберт in the quest's mode");
			if (Watch == 2 && bHis)
			{
				const EWatch W = T.WatchHands();
				CHECK(W == (bMarkedNow ? EWatch::Caught : EWatch::Nothing), "watching his deal: %d (marked %d)", (int)W, bMarkedNow ? 1 : 0);
				Caught += W == EWatch::Caught ? 1 : 0;
				Nothing += W == EWatch::Nothing ? 1 : 0;
				if (W == EWatch::Caught)
				{
					CHECK(T.Phase == EPhase::Over && T.Result.bVoid && T.Chips() == Total && T.SeatOf(Albert) < 0 && T.bCaught, "caught: the hand is void, he is gone");
					CHECK(T.WatchHands() == EWatch::NotNow, "caught twice");
					continue;
				}
			}
			const int A = T.SeatOf(Albert);
			PlayHand(T, EHero::Solid, R, [&](FTable& Now)
			{
				if (Watch == 1 && Now.Phase != EPhase::Dealing && Now.Phase != EPhase::Over)
				{
					// any other moment: nothing to see, nothing changes
					const size_t Before = Now.Log.size();
					const EWatch W = Now.WatchHands();
					CaughtWrong += W == EWatch::Caught ? 1 : 0;
					NotNowWrong += W != EWatch::NotNow || Now.Log.size() != Before ? 1 : 0;
				}
			});
			if (A > 0 && T.Result.bShowdown && T.Seats[(size_t)A].Live())
			{
				++Shows[bMarkedNow ? 1 : 0];
				Big[bMarkedNow ? 1 : 0] += CatOf(T.Seats[(size_t)A].Value) >= FullHouse ? 1 : 0;
			}
			for (const FEvent& E : T.Log)
			{
				Suspects += E.Type == EEv::Suspect ? 1 : 0;
				CHECK(E.Type != EEv::Suspect || bMarkedNow, "a suspicion on an honest hand");
			}
			CHECK(T.Chips() == Total, "chips in the sharp's game");
		}
		CHECK(T.bSharp || (!T.bCaught && T.SeatOf(Albert) != 0), "nobody is caught outside the quest's mode");
	}
	CHECK(CaughtWrong == 0 && NotNowWrong == 0, "watching at a wrong moment: caught %d, not refused %d", CaughtWrong, NotNowWrong);
	CHECK(Caught > 200 && Nothing > 20, "watching his deals caught him %d times, saw nothing %d times", Caught, Nothing);
	CHECK(Marked > 0.7 * HisDeals && Marked < 0.9 * HisDeals, "%lld of his %lld deals marked", Marked, HisDeals);
	const double Honest = (double)Big[0] / std::max(1ll, Shows[0]);
	const double Cheat = (double)Big[1] / std::max(1ll, Shows[1]);
	CHECK(Shows[1] > 300 && Cheat > 0.5 && Honest < 0.05, "full houses and better at his showdowns: %.1f%% honest, %.1f%% marked (%lld)", 100.0 * Honest, 100.0 * Cheat, Shows[1]);
	CHECK(Suspects > 200, "%lld suspicions", Suspects);
	std::printf("sharp: a full house or better at %.1f%% of his marked showdowns, %.1f%% of the honest ones\n", 100.0 * Cheat, 100.0 * Honest);
	// with the mode off the same seeds play as if it never existed
	CHECK(RunSitting(3, 1, 0, EHero::Solid, false) != RunSitting(3, 1, 0, EHero::Solid, true), "the mode changes his deals");
}

static bool Has(const std::vector<int>& V, int O) { return std::find(V.begin(), V.end(), O) != V.end(); }

static void TestLineup()
{
	// who sits when (the hours the people's life seats their bodies at the table)
	for (int Day = 1; Day <= 2; ++Day)
	{
		CHECK(Lineup(Day, 15.0, false, false).empty() && Lineup(Day, 23.0, false, false).empty(), "nobody plays when the cards are not on");
		for (int M = 22 * 60; M < 25 * 60; ++M)
		{
			const double H = M / 60.0;
			const double Hour = H >= 24.0 ? H - 24.0 : H;
			const std::vector<int> L = Lineup(Day, Hour, true, false);
			CHECK(L.size() >= 3 && L.size() <= 4, "day %d %.2f: %zu at the table", Day, H, L.size());
			for (const int O : L)
			{
				CHECK(std::count(L.begin(), L.end(), O) == 1, "the same man twice");
			}
			CHECK(Has(L, Albert), "Альберт sits all evening");
			CHECK(Has(L, Taras) == (M < 1470) && Has(L, Lev) == (M < 1410) && Has(L, Vera) == (M < 1380), "day %d %.2f: Тарас, Лев Маркович, Вера Игнатьевна by their hours", Day, H);
			CHECK(Has(L, Blum) == (M >= (Day == 1 ? 1390 : 1420)), "day %d %.2f: Блюм comes after his concert", Day, H);
			CHECK(!Has(L, Givi) || M >= 1380, "Гиви is busy at the bar before eleven");
			const std::vector<int> C = Lineup(Day, Hour, true, true);
			CHECK(!Has(C, Albert) && C.size() >= 2, "day %d %.2f: once caught he does not come back (%zu left)", Day, H, C.size());
		}
	}
	// a whole evening at the table: people get up at their hour and others sit in between hands, no chip is lost in it
	FRng R;
	R.S = 808;
	int Seen = 0;
	long long Joins = 0, Goes = 0;
	for (int Evening = 0; Evening < 400; ++Evening)
	{
		const int Day = 1 + Evening % 2;
		FStakes St = FStakes::Roubles();
		St.MaxHands = 60;
		FNight N;
		N.Roll(Evening, St);
		double Hour = 22.0;
		FTable T;
		T.Sit(St, Lineup(Day, Hour, true, false), N.Purse, Evening, Day, 0);
		int Met = 0;
		for (;;)
		{
			const int Before = T.Chips();
			const int SeatsBefore = T.NumSeats();
			const std::vector<int> Now = Lineup(Day, Hour >= 24.0 ? Hour - 24.0 : Hour, Hour < 25.0, false);
			if (Now.empty() || !T.StartHand(&Now, N.Purse))
			{
				break;
			}
			int Added = 0;
			for (int I = SeatsBefore; I < T.NumSeats(); ++I)
			{
				Added += T.Seats[(size_t)I].Stack + T.Seats[(size_t)I].Total;
			}
			CHECK(T.Chips() == Before + Added, "evening %d: %d chips before the hand, %d after with %d brought in", Evening, Before, T.Chips(), Added);
			for (const FEvent& E : T.Log)
			{
				Joins += E.Type == EEv::Joined ? 1 : 0;
				Goes += E.Type == EEv::Gone ? 1 : 0;
			}
			for (int I = 1; I < T.NumSeats(); ++I)
			{
				const FSeat& S = T.Seats[(size_t)I];
				CHECK(!S.bIn || Has(Now, S.Opp), "evening %d: %s is dealt in after his hour", Evening, OppId(S.Opp));
				Met |= S.bIn ? 1 << S.Opp : 0;
				for (int J = I + 1; J < T.NumSeats(); ++J)
				{
					CHECK(T.Seats[(size_t)J].Opp != S.Opp, "%s has two seats", OppId(S.Opp));
				}
			}
			PlayHand(T, EHero::Solid, R, [](const FTable&) {});
			Hour += MinutesPerHand / 60.0;
		}
		T.Purses(N.Purse);
		Seen += Met == (1 << NumOpponents) - 1 ? 1 : 0;
	}
	CHECK(Seen > 100 && Joins > 400 && Goes > 400, "all six met in %d evenings of 400; %lld sat in, %lld got up", Seen, Joins, Goes);
}

static void TestRecord()
{
	FRecord A;
	A.Hands = 37;
	A.PotsWon = 9;
	A.BiggestPot = 58;
	A.BeatenMask = 0x2B;
	A.HandsMask = (1 << FullHouse) | (1 << NumCats) | 3;
	A.bSharp = true;
	A.bCaught = false;
	A.Night.Day = 739583;
	A.Night.Net = -17;
	for (int I = 0; I < NumOpponents; ++I)
	{
		A.Night.Purse[I] = 40 - 5 * I;
	}
	FRecord B;
	CHECK(B.FromString(A.ToString()), "the record reads itself: %s", A.ToString().c_str());
	CHECK(B.ToString() == A.ToString() && B.Hands == 37 && B.BiggestPot == 58 && B.BeatenMask == 0x2B && B.bSharp && !B.bCaught && B.Night.Net == -17
		&& B.Night.Day == 739583 && B.Night.Purse[5] == 15 && B.HandsMask == A.HandsMask, "round trip: %s", B.ToString().c_str());
	for (const char* Junk : { "", "v2;1;2", "v1;1;2;3", "hello", "v1;;;;;;;;;;;;;;;;;;;;" })
	{
		FRecord C;
		C.Hands = 5;
		CHECK(!C.FromString(Junk) && C.Hands == 0 && C.ToString() == FRecord().ToString(), "junk \"%s\" gives a fresh record", Junk);
	}
	// a played sitting goes into it
	FRecord Rec;
	FTable T;
	T.Sit(FStakes::Roubles(), { Givi, Vera, Taras, Lev }, nullptr, 12, 1, 0);
	FRng R;
	R.S = 3;
	int Won = 0, Biggest = 0;
	while (T.StartHand())
	{
		PlayHand(T, EHero::Solid, R, [](const FTable&) {});
		Rec.Note(T.Result, true);
		Won += T.Result.bHeroWon ? 1 : 0;
		Biggest = std::max(Biggest, T.Result.bHeroWon ? T.Result.Pot : 0);
	}
	CHECK(Rec.Hands == T.Seats[0].Hands && Rec.PotsWon == Won && Rec.BiggestPot == Biggest, "the record of a sitting: %s", Rec.ToString().c_str());
	FRecord Back;
	CHECK(Back.FromString(Rec.ToString()) && Back.ToString() == Rec.ToString(), "and its round trip");
}

// ----------------------------------------------------------------------------------------------------------------- the words

static std::string ReadFile(const std::string& Path)
{
	std::ifstream F(Path, std::ios::binary);
	std::stringstream S;
	S << F.rdbuf();
	return S.str();
}

/** A flat {"key": "text"} file (the string tables). */
static std::map<std::string, std::string> Strings(const std::string& Lang)
{
	std::map<std::string, std::string> Out;
	const std::string T = ReadFile("unreal/Lyudmilovsk/Content/Localization/poker/strings_" + Lang + ".json");
	size_t At = 0;
	auto Quoted = [&T, &At](std::string& S)
	{
		At = T.find('"', At);
		if (At == std::string::npos)
		{
			return false;
		}
		S.clear();
		for (++At; At < T.size() && T[At] != '"'; ++At)
		{
			if (T[At] == '\\' && At + 1 < T.size())
			{
				++At;
				S += T[At] == 'n' ? '\n' : T[At];
			}
			else
			{
				S += T[At];
			}
		}
		++At;
		return true;
	};
	std::string K, V;
	while (Quoted(K) && Quoted(V))
	{
		Out[K] = V;
	}
	return Out;
}

static std::vector<std::string> NeededKeys()
{
	std::vector<std::string> K = {
		"poker.title", "poker.table.matches", "poker.table.roubles", "poker.choose", "poker.choose.matches", "poker.choose.roubles",
		"poker.choose.keys", "poker.closed", "poker.cap", "poker.no_money", "poker.nobody", "poker.help.0", "poker.help.1", "poker.help.2", "poker.help.3", "poker.help.4", "poker.help.5", "poker.help.6", "poker.choose.cap",
		"poker.title.table", "poker.keys.common", "poker.log.says", "poker.sharp.suspect2", "poker.sharp.caught2", "poker.sharp.clean", "poker.you", "poker.pot", "poker.stack",
		"poker.bet", "poker.dealer", "poker.unit.matches", "poker.unit.roubles", "poker.hand_no",
		"poker.keys.bet", "poker.keys.bet_call", "poker.keys.draw", "poker.keys.next", "poker.keys.wait", "poker.keys.deal_watch", "poker.keys.over",
		"poker.act.check", "poker.act.bet", "poker.act.call", "poker.act.raise", "poker.act.fold", "poker.act.allin", "poker.act.draw", "poker.act.pat",
		"poker.act.you.check", "poker.act.you.bet", "poker.act.you.call", "poker.act.you.raise", "poker.act.you.fold", "poker.act.you.draw", "poker.act.you.pat",
		"poker.log.deal", "poker.log.you_deal", "poker.log.ante", "poker.log.show", "poker.log.you_show", "poker.log.win", "poker.log.you_win", "poker.log.split",
		"poker.log.left", "poker.log.goes", "poker.log.joins", "poker.log.your_turn", "poker.log.your_draw", "poker.mark.out", "poker.mark.fold", "poker.mark.allin", "poker.mark.drew",
		"poker.sharp.suspect", "poker.sharp.caught", "poker.sharp.missed", "poker.sharp.not_now",
		"poker.end.left", "poker.end.broke", "poker.end.empty", "poker.end.hands", "poker.end.clock", "poker.end.up", "poker.end.down", "poker.end.even",
		"poker.end.matches", "poker.hand.royal", "dev.action.poker.open", "dev.action.poker.sharp",
	};
	for (int C = 0; C < NumCats; ++C)
	{
		K.push_back(std::string("poker.hand.") + CatId(C));
	}
	for (int O = 0; O < NumOpponents; ++O)
	{
		K.push_back(std::string("poker.name.") + OppId(O));
		K.push_back(std::string("poker.style.") + OppId(O));
		for (int T = 0; T < NumTells; ++T)
		{
			for (int L = 0; L < TellLines; ++L)
			{
				K.push_back(std::string("poker.tell.") + OppId(O) + "." + TellId(T) + "." + std::to_string(L));
			}
		}
		for (int L = 0; L < TalkLines; ++L)
		{
			K.push_back(std::string("poker.talk.") + OppId(O) + "." + std::to_string(L));
		}
	}
	return K;
}

static int Placeholders(const std::string& S)
{
	int Mask = 0;
	for (size_t I = 0; I + 2 < S.size(); ++I)
	{
		if (S[I] == '{' && S[I + 1] >= '0' && S[I + 1] <= '9' && S[I + 2] == '}')
		{
			Mask |= 1 << (S[I + 1] - '0');
		}
	}
	return Mask;
}

static void TestStrings()
{
	const std::vector<std::string> Keys = NeededKeys();
	const std::map<std::string, std::string> Ru = Strings("ru");
	CHECK(Ru.size() >= Keys.size(), "the Russian table has %zu strings, %zu are needed", Ru.size(), Keys.size());
	for (const char* Lang : { "ru", "en", "ja" })
	{
		const std::map<std::string, std::string> T = Strings(Lang);
		int Missing = 0;
		for (const std::string& K : Keys)
		{
			const auto It = T.find(K);
			if (It == T.end() || It->second.empty())
			{
				if (++Missing <= 8)
				{
					std::printf("  %s: no %s\n", Lang, K.c_str());
				}
				continue;
			}
			const auto R = Ru.find(K);
			CHECK(R == Ru.end() || Placeholders(R->second) == Placeholders(It->second), "%s: %s has other {n} than the Russian", Lang, K.c_str());
		}
		CHECK(Missing == 0, "%s: %d strings missing", Lang, Missing);
		CHECK(T.size() == Ru.size(), "%s has %zu strings, ru %zu", Lang, T.size(), Ru.size());
		for (const auto& KV : T)
		{
			CHECK(KV.first.rfind("poker.", 0) == 0 || KV.first.rfind("dev.action.poker.", 0) == 0, "%s: a key that is not ours: %s", Lang, KV.first.c_str());
			CHECK(Ru.count(KV.first) == 1, "%s: %s is not in the Russian table", Lang, KV.first.c_str());
		}
		// a tell is one short line on the panel; no two lines of a man are the same
		for (int O = 0; O < NumOpponents; ++O)
		{
			std::map<std::string, int> Seen;
			for (const auto& KV : T)
			{
				if (KV.first.rfind(std::string("poker.tell.") + OppId(O), 0) == 0 || KV.first.rfind(std::string("poker.talk.") + OppId(O), 0) == 0)
				{
					CHECK(++Seen[KV.second] == 1, "%s: %s repeats another line", Lang, KV.first.c_str());
					CHECK(KV.second.size() <= (std::strcmp(Lang, "en") == 0 ? 110u : 220u), "%s: %s is too long for the panel (%zu bytes)", Lang, KV.first.c_str(), KV.second.size());
				}
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------- the demo

static std::string Fill(std::string S, const std::vector<std::string>& Args)
{
	for (size_t I = 0; I < Args.size(); ++I)
	{
		const std::string Tag = "{" + std::to_string(I) + "}";
		for (size_t At = S.find(Tag); At != std::string::npos; At = S.find(Tag, At + Args[I].size()))
		{
			S.replace(At, Tag.size(), Args[I]);
		}
	}
	return S;
}

static int Play(long long Seed, int Hands, const std::string& Lang, bool bSharp)
{
	const std::map<std::string, std::string> T = Strings(Lang);
	auto Tx = [&T](const std::string& K) { const auto It = T.find(K); return It == T.end() ? K : It->second; };
	const std::vector<int> Who = Lineup(1, 23.5, true, false);
	FTable Tab;
	Tab.bSharp = bSharp;
	const FStakes St = FStakes::Roubles();
	Tab.Sit(St, Who, nullptr, Seed, 1, 0);
	auto Name = [&](int Seat) { return Seat == 0 ? Tx("poker.you") : Tx(std::string("poker.name.") + OppId(Tab.Seats[(size_t)Seat].Opp)); };
	std::printf("== %s. %s ==\n", Tx("poker.title").c_str(), Tx("poker.table.roubles").c_str());
	for (int I = 1; I < Tab.NumSeats(); ++I)
	{
		std::printf("   %s: %s\n", Name(I).c_str(), Tx(std::string("poker.style.") + OppId(Tab.Seats[(size_t)I].Opp)).c_str());
	}
	FRng R;
	R.S = (uint64_t)Seed;
	for (int Hand = 0; Hand < Hands && Tab.StartHand(); ++Hand)
	{
		size_t Printed = 0;
		auto Print = [&]()
		{
			for (; Printed < Tab.Log.size(); ++Printed)
			{
				const FEvent& E = Tab.Log[Printed];
				const std::string N = E.Seat >= 0 ? Name(E.Seat) : std::string();
				const std::string Amount = std::to_string(E.A);
				const std::string You = E.Seat == 0 ? "you." : "";
				switch (E.Type)
				{
				case EEv::Deal:
					std::printf("\n-- %s --\n%s\n", Fill(Tx("poker.hand_no"), { Amount }).c_str(),
						E.Seat == 0 ? Tx("poker.log.you_deal").c_str() : Fill(Tx("poker.log.deal"), { N }).c_str());
					break;
				case EEv::Ante: std::printf("%s\n", Fill(Tx("poker.log.ante"), { Amount }).c_str()); break;
				case EEv::Check: std::printf("%s\n", Fill(Tx("poker.act." + You + "check"), { N }).c_str()); break;
				case EEv::Bet: std::printf("%s\n", Fill(Tx("poker.act." + You + "bet"), { N, Amount }).c_str()); break;
				case EEv::Call: std::printf("%s\n", Fill(Tx("poker.act." + You + "call"), { N, Amount }).c_str()); break;
				case EEv::Raise: std::printf("%s\n", Fill(Tx("poker.act." + You + "raise"), { N, Amount }).c_str()); break;
				case EEv::Fold: std::printf("%s\n", Fill(Tx("poker.act." + You + "fold"), { N }).c_str()); break;
				case EEv::Draw: std::printf("%s\n", Fill(Tx("poker.act." + You + (E.A ? "draw" : "pat")), { N, Amount }).c_str()); break;
				case EEv::Tell:
					std::printf("      ~ %s\n", Tx(std::string("poker.tell.") + OppId(Tab.Seats[(size_t)E.Seat].Opp) + "." + TellId(E.A) + "." + std::to_string(E.B)).c_str());
					break;
				case EEv::Show:
				{
					const std::string HandName = Tx(std::string("poker.hand.") + HandId(Tab.Seats[(size_t)E.Seat].Value));
					std::printf("%s  [%s]\n", E.Seat == 0 ? Fill(Tx("poker.log.you_show"), { HandName }).c_str() : Fill(Tx("poker.log.show"), { N, HandName }).c_str(),
						HandText(Tab.Seats[(size_t)E.Seat].Cards).c_str());
					break;
				}
				case EEv::Win: std::printf("%s\n", E.Seat == 0 ? Fill(Tx("poker.log.you_win"), { Amount }).c_str() : Fill(Tx("poker.log.win"), { N, Amount }).c_str()); break;
				case EEv::Talk:
					std::printf("      %s: %s\n", N.c_str(), Tx(std::string("poker.talk.") + OppId(Tab.Seats[(size_t)E.Seat].Opp) + "." + std::to_string(E.B)).c_str());
					break;
				case EEv::Suspect: std::printf("   ! %s\n", Tx("poker.sharp.suspect").c_str()); break;
				case EEv::Caught: std::printf("   ! %s\n", Tx("poker.sharp.caught").c_str()); break;
				case EEv::Missed: std::printf("   ! %s\n", Tx("poker.sharp.missed").c_str()); break;
				case EEv::Left: std::printf("%s\n", Fill(Tx("poker.log.left"), { N }).c_str()); break;
				case EEv::Gone: std::printf("%s\n", Fill(Tx("poker.log.goes"), { N }).c_str()); break;
				case EEv::Joined: std::printf("%s\n", Fill(Tx("poker.log.joins"), { N }).c_str()); break;
				}
			}
		};
		Print();
		std::printf("   %s: [%s]\n", Tx("poker.you").c_str(), HandText(Tab.Seats[0].Cards).c_str());
		// in the quest's mode the scripted hero watches the sharp's second deal
		if (bSharp && Tab.Seats[(size_t)Tab.Dealer].Opp == Albert && Hand >= 5)
		{
			Tab.WatchHands();
			Print();
		}
		int Guard = 0;
		while (Tab.Phase != EPhase::Over && ++Guard < 2000)
		{
			if (Tab.WaitingHero())
			{
				if (Tab.Phase == EPhase::Draw)
				{
					Tab.HeroDraw(SoundDraw(Tab.Seats[0].Cards));
					Print();
					std::printf("   %s: [%s]\n", Tx("poker.you").c_str(), HandText(Tab.Seats[0].Cards).c_str());
				}
				else
				{
					Tab.HeroAct(HeroMove(Tab, EHero::Reader, R));
				}
			}
			else
			{
				Tab.Step();
			}
			Print();
		}
		std::printf("   ");
		for (int I = 0; I < Tab.NumSeats(); ++I)
		{
			std::printf("%s %d   ", Name(I).c_str(), Tab.Seats[(size_t)I].Stack);
		}
		std::printf("\n");
	}
	const int Net = Tab.Seats[0].Stack - St.BuyIn;
	std::printf("\n%s\n", Fill(Tx(Net > 0 ? "poker.end.up" : (Net < 0 ? "poker.end.down" : "poker.end.even")), { std::to_string(Net < 0 ? -Net : Net) }).c_str());
	return 0;
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "test";
	if (Mode == "play" || Mode == "sharp")
	{
		return Play(Argc > 2 ? std::atoll(Argv[2]) : 7, Argc > 3 ? std::atoi(Argv[3]) : 4, Argc > 4 ? Argv[4] : "ru", Mode == "sharp");
	}
	if (Mode == "stats")
	{
		FStat S[NumOpponents];
		Styles(S, 6000, true);
		for (const EHero K : { EHero::Station, EHero::Tight, EHero::Random, EHero::Reader })
		{
			FStat S2[NumOpponents];
			Styles(S2, 2000, true, K);
		}
		TestTells(true);
		return 0;
	}
	TestEvaluator();
	TestConservation();
	TestNight();
	TestDeterminism();
	TestStyles(true);
	TestTells(false);
	TestSharp();
	TestLineup();
	TestRecord();
	TestStrings();
	std::printf("pokersim: %d checks, %d failed\n", Checks, Fails);
	return Fails ? 1 : 0;
}
