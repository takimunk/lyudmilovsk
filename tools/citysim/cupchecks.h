// The clubs' weekly tournaments (Private/City/CityCupsModel.h) on the real data: a year of brackets without the hero, then
// the hero signing up, playing his rounds for real (a draw replayed, the cup's stamp), a no-show, a loss, the save, the board's
// pages and their words. Included by citytest.cpp (its CHECK, ReadFile).
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/City/CityCupsModel.h"

namespace CupTest
{
	/** A moment of a game day: the game began on Tuesday 20 October 2026. */
	inline FWhen Day(long long Since, float Hour)
	{
		static const int Len[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
		FWhen W;
		W.Since = Since;
		W.Day = Since + 1000;
		W.Hour = Hour;
		W.Weekday = (int)((1 + Since) % 7);
		int Mo = 10, Dd = 20;
		for (long long I = 0; I < Since; ++I)
		{
			if (++Dd > Len[Mo - 1]) { Dd = 1; Mo = Mo % 12 + 1; }
		}
		W.MonthDay = Mo * 100 + Dd;
		return W;
	}
	/** The first day (since the start) on that weekday at or after From. */
	inline long long First(int Weekday, long long From)
	{
		long long D = From;
		while ((1 + D) % 7 != Weekday) ++D;
		return D;
	}
	/** A thing of Plot whose game is Vs (-1: none). */
	inline int Table(const FModel& M, const std::string& Plot, const std::string& Vs, int Skip = -1)
	{
		for (int I = 0; I < (int)M.Things.size(); ++I)
		{
			const FGameDef* G = M.GameOfThing(I);
			if (I != Skip && M.Things[(size_t)I].Plot == Plot && G && G->Vs == Vs)
			{
				return I;
			}
		}
		return -1;
	}
	inline bool BracketSane(const FModel& M, const FCups& C, const FBracket& B, std::string& Why)
	{
		std::set<int> Seen;
		const FCupDef& D = C.Defs[(size_t)B.Cup];
		for (int S = 0; S < FBracket::Seats; ++S)
		{
			const int R = B.Seat[S];
			if (R == FBracket::HeroSeat ? !B.bHero : std::find(D.Pool.begin(), D.Pool.end(), R) == D.Pool.end())
			{
				Why = "seat " + std::to_string(S) + " is nobody of the pool";
				return false;
			}
			if (!Seen.insert(R).second)
			{
				Why = "seat " + std::to_string(S) + " twice";
				return false;
			}
		}
		for (int Mt = 0; Mt < FBracket::Matches; ++Mt)
		{
			if (B.Winner[Mt] >= 0 && B.Winner[Mt] != B.Side[Mt][0] && B.Winner[Mt] != B.Side[Mt][1])
			{
				Why = "match " + std::to_string(Mt) + " won by a seat not in it";
				return false;
			}
		}
		(void)M;
		return true;
	}
}

static void CupChecks(const std::string& Path)
{
	using namespace CupTest;
	std::string Text, CupText, Error;
	const std::string CupPath = Path.substr(0, Path.find_last_of('/') + 1) + "tournaments.json";
	if (!ReadFile(Path, Text) || !ReadFile(CupPath, CupText))
	{
		std::printf("note: %s or tournaments.json is not built yet: no tournament checks\n", Path.c_str());
		return;
	}
	std::printf("tournaments (%s)\n", CupPath.c_str());
	FModel M;
	FCups C;
	CHECK(M.Load(Text, Error) && C.Load(CupText, Error), "the interiors and the tournaments load (%s)", Error.c_str());
	CHECK(C.Defs.size() == 3 && C.Bind(M), "three tournaments, each with eight regulars or more (%d)", (int)C.Defs.size());
	for (const FCupDef& D : C.Defs)
	{
		std::printf("       %s: %s at %s, weekday %d from %.1f, %d in the pool\n", D.Id.c_str(), D.Game.c_str(), D.Plot.c_str(), D.Weekday, D.Start, (int)D.Pool.size());
	}
	// every cup: a table of its game at its plot, and a board on some thing that reads its bracket text
	for (int Cup = 0; Cup < (int)C.Defs.size(); ++Cup)
	{
		const FCupDef& D = C.Defs[(size_t)Cup];
		int Boards = 0;
		for (int I = 0; I < (int)M.Things.size(); ++I)
		{
			const FAct* A = M.ActOf(I);
			for (int Tx : A ? A->Texts : std::vector<int>())
			{
				Boards += Tx >= 0 && Tx < (int)M.Texts.size() && M.Texts[(size_t)Tx].Gen == "cup:" + D.Id ? 1 : 0;
			}
		}
		CHECK(Table(M, D.Plot, D.Game) >= 0 && Boards >= 1, "%s: a %s table at %s, %d bracket board(s)", D.Id.c_str(), D.Game.c_str(), D.Plot.c_str(), Boards);
	}

	// 1. a year without the hero: sane brackets, the same when asked again, the stronger win more often, many champions
	for (int Cup = 0; Cup < (int)C.Defs.size(); ++Cup)
	{
		const FCupDef& D = C.Defs[(size_t)Cup];
		std::map<int, int> Champs;
		int Bad = 0, Undone = 0, Unstable = 0, Hero = 0;
		double PoolSkill = 0.0, ChampSkill = 0.0;
		for (int R : D.Pool) PoolSkill += M.Regulars[(size_t)R].Skill;
		PoolSkill /= (double)D.Pool.size();
		std::string Why;
		for (long long Dd = First(D.Weekday, 0), N = 0; N < 52; Dd += 7, ++N)
		{
			const FWhen After = Day(Dd + 1, 9.f);
			const FBracket B = C.Bracket(M, Cup, Dd, After);
			const FBracket B2 = C.Bracket(M, Cup, Dd, After);
			Bad += BracketSane(M, C, B, Why) ? 0 : 1;
			Undone += B.IsDone() ? 0 : 1;
			Hero += B.bHero ? 1 : 0;
			Unstable += std::equal(std::begin(B.Seat), std::end(B.Seat), std::begin(B2.Seat)) && std::equal(std::begin(B.Winner), std::end(B.Winner), std::begin(B2.Winner)) ? 0 : 1;
			if (B.IsDone())
			{
				Champs[B.Champion()]++;
				ChampSkill += M.Regulars[(size_t)B.Champion()].Skill;
			}
		}
		ChampSkill /= 52.0;
		CHECK(Bad == 0 && Undone == 0 && Hero == 0 && Unstable == 0, "%s: 52 weeks, every bracket of eight of the pool, played out, the same when asked again (bad %d %s, undone %d, unstable %d)",
			D.Id.c_str(), Bad, Why.c_str(), Undone, Unstable);
		CHECK((int)Champs.size() >= 4 && ChampSkill > PoolSkill, "%s: %d different champions; the champions' skill %.2f over the pool's %.2f",
			D.Id.c_str(), (int)Champs.size(), ChampSkill, PoolSkill);
		// before the start nothing is played, mid-way the first round only
		const long long Dd = First(D.Weekday, 7);
		const FBracket Early = C.Bracket(M, Cup, Dd, Day(Dd, D.Start - 0.5f));
		const FBracket Mid = C.Bracket(M, Cup, Dd, Day(Dd, D.Start + D.Step + 0.1f));
		int EarlyWon = 0, MidWon = 0;
		for (int Mt = 0; Mt < FBracket::Matches; ++Mt)
		{
			EarlyWon += Early.Winner[Mt] >= 0 ? 1 : 0;
			MidWon += Mid.Winner[Mt] >= 0 ? 1 : 0;
		}
		CHECK(EarlyWon == 0 && MidWon == 4 && Mid.Side[4][0] >= 0 && Mid.Side[5][1] >= 0, "%s: before the start nothing is played; after the first round its four, the semi-finals known", D.Id.c_str());
	}

	// 2. the hero at the Saturday chess
	const int Chess = C.Find("cog_chess");
	const FCupDef& CD = C.Defs[(size_t)(Chess >= 0 ? Chess : 0)];
	const long long Sat = First(5, 3);
	const int T0 = Table(M, CD.Plot, "chess");
	const int T1 = Table(M, CD.Plot, "chess", T0);
	const int Dom = Table(M, "P-SANATORIUM", "dominoes");
	long long Signed = -1;
	CHECK(Chess >= 0 && C.NextDay(Chess, Day(Sat, 10.f)) == Sat && C.NextDay(Chess, Day(Sat, 13.5f)) == Sat + 7 && C.NextDay(Chess, Day(Sat - 2, 20.f)) == Sat,
		"the sheet is for today until the start, then for next week");
	CHECK(C.SignUp(Chess, Day(Sat, 10.f), &Signed) && Signed == Sat && !C.SignUp(Chess, Day(Sat, 11.f)) && C.IsSigned(Chess, Sat),
		"signing up on Saturday morning: today's tournament, once");
	FBracket B = C.Bracket(M, Chess, Sat, Day(Sat, 12.f));
	int HeroSeats = 0;
	for (int S : B.Seat) HeroSeats += S == FBracket::HeroSeat ? 1 : 0;
	std::string Why;
	CHECK(B.bHero && HeroSeats == 1 && B.bHeroAlive && B.HeroReached == 0 && BracketSane(M, C, B, Why), "the hero has a seat (%s)", Why.c_str());
	int Cup = -1, Round = -1;
	long long OnDay = -1;
	CHECK(C.MatchAt(M, T0, Day(Sat, 12.5f)) < 0, "no match before the start");
	const int Opp0 = C.MatchAt(M, T0, Day(Sat, 13.2f), &Cup, &OnDay, &Round);
	const int HeroSeat = (int)(std::find(std::begin(B.Seat), std::end(B.Seat), FBracket::HeroSeat) - std::begin(B.Seat));
	CHECK(Opp0 >= 0 && Opp0 == B.Seat[HeroSeat ^ 1] && Cup == Chess && OnDay == Sat && Round == 0,
		"at 13:12 at a chess table of the club: the quarter-final against the seat beside his (%s)", Opp0 >= 0 ? M.Regulars[(size_t)Opp0].Id.c_str() : "-");
	CHECK(C.MatchAt(M, T1, Day(Sat, 13.2f)) == Opp0 && (Dom < 0 || C.MatchAt(M, Dom, Day(Sat, 13.2f)) < 0) && C.MatchAt(M, T0, Day(Sat + 7, 13.2f)) < 0,
		"the other chess table too; not the sanatorium's dominoes, not next Saturday (not signed)");
	// the tournament game is played whatever today's count: three casual games first
	FAsk Ask;
	for (int I = 0; I < 3; ++I) M.Run(T0, Day(Sat, 12.f), Ask);
	const FOutcome Plain = M.Run(T0, Day(Sat, 13.2f), Ask);
	Ask.bFreeGame = true;
	const FOutcome Free = M.Run(T0, Day(Sat, 13.2f), Ask);
	CHECK(Plain.Game < 0 && Free.Game >= 0, "after the day's three games a casual one runs plainly, the tournament's is played (%d %d)", Plain.Game, Free.Game);
	FGameState G;
	M.StartGameSeeded(T0, 7, G);
	CHECK(G.Def >= 0, "a game starts at the table");
	FCupResult R = C.Record(M, Chess, Sat, 0, EGameResult::Draw);
	CHECK(R.Note == ECupNote::Replay && C.Record(Chess, Sat).empty() && C.MatchAt(M, T0, Day(Sat, 13.6f)) == Opp0, "a draw is replayed: same opponent");
	R = C.Record(M, Chess, Sat, 0, EGameResult::Win);
	CHECK(R.Note == ECupNote::Next && std::fabs(R.NextHour - (CD.Start + CD.Step)) < 0.01f && C.MatchAt(M, T0, Day(Sat, 14.f)) < 0,
		"the quarter-final won: the semi-final at %.2f, nothing to play before it", R.NextHour);
	CHECK(C.Record(M, Chess, Sat, 0, EGameResult::Win).Note == ECupNote::None, "a round is recorded once");
	const int Opp1 = C.MatchAt(M, T0, Day(Sat, CD.Start + CD.Step + 0.1f), nullptr, nullptr, &Round);
	B = C.Bracket(M, Chess, Sat, Day(Sat, CD.Start + CD.Step + 0.1f));
	const int Semi = 4 + HeroSeat / 4;
	const int OtherQf = (HeroSeat / 2) ^ 1;
	CHECK(Opp1 >= 0 && Round == 1 && B.Winner[OtherQf] >= 0 && Opp1 == B.Seat[B.Winner[OtherQf]] && (B.Side[Semi][0] == HeroSeat || B.Side[Semi][1] == HeroSeat),
		"the semi-final against the winner of the next quarter-final (%s)", Opp1 >= 0 ? M.Regulars[(size_t)Opp1].Id.c_str() : "-");
	R = C.Record(M, Chess, Sat, 1, EGameResult::Win);
	const int Opp2 = C.MatchAt(M, T0, Day(Sat, CD.Start + 2.f * CD.Step + 0.1f), nullptr, nullptr, &Round);
	CHECK(R.Note == ECupNote::Next && Opp2 >= 0 && Round == 2, "the semi-final won, the final open (%s)", Opp2 >= 0 ? M.Regulars[(size_t)Opp2].Id.c_str() : "-");
	R = C.Record(M, Chess, Sat, 2, EGameResult::Win);
	const bool bStamp = std::find(R.NewStamps.begin(), R.NewStamps.end(), CD.Collect) != R.NewStamps.end();
	B = C.Bracket(M, Chess, Sat, Day(Sat, 23.f));
	CHECK(R.Note == ECupNote::Champion && bStamp && M.HasStamp(CD.Collect) && B.Champion() == FBracket::HeroSeat && B.HeroReached == 3 && C.CupsWon() == 1 && C.CupsWon(Chess) == 1,
		"the final won: the cup (%s), the bracket's champion is the hero", CD.Collect.c_str());
	std::vector<FCupNews> News;
	C.News(M, Sat, Day(Sat + 1, 7.f), News);
	bool bNewsChess = false;
	for (const FCupNews& N : News) bNewsChess |= N.Cup == Chess && N.Winner.empty() && N.HeroReached == 3 && !N.RunnerUp.empty();
	CHECK(bNewsChess, "the next morning's news: the hero won the chess (%d tournaments that day)", (int)News.size());

	// 3. a no-show, a loss
	const long long Sat2 = Sat + 7, Sat3 = Sat + 14;
	CHECK(C.SignUp(Chess, Day(Sat, 20.f), &Signed) && Signed == Sat2, "signing up on Saturday evening: next week's");
	B = C.Bracket(M, Chess, Sat2, Day(Sat2, CD.Start + CD.Step + 0.05f));
	const int Hs2 = (int)(std::find(std::begin(B.Seat), std::end(B.Seat), FBracket::HeroSeat) - std::begin(B.Seat));
	CHECK(B.bNoShow[Hs2 / 2] && B.Winner[Hs2 / 2] == (Hs2 ^ 1) && B.HeroReached == 0 && !B.bHeroAlive && C.MatchAt(M, T0, Day(Sat2, CD.Start + CD.Step + 0.1f)) < 0,
		"not at the table by the end of the first round: a no-show, out");
	C.SignUp(Chess, Day(Sat2, 20.f));
	R = C.Record(M, Chess, Sat3, 0, EGameResult::Lose);
	B = C.Bracket(M, Chess, Sat3, Day(Sat3, 23.f));
	CHECK(R.Note == ECupNote::Out && B.HeroReached == 0 && !B.bHeroAlive && B.IsDone() && B.Champion() != FBracket::HeroSeat && C.MatchAt(M, T0, Day(Sat3, CD.Start + CD.Step + 0.1f)) < 0,
		"a lost quarter-final: out, the others play it out");
	News.clear();
	C.News(M, Sat3, Day(Sat3 + 1, 7.f), News);
	CHECK(News.size() == 1 && News[0].HeroReached == 0 && !News[0].Winner.empty(), "the news of that day: a regular won, the hero out in the first round");

	// 4. the save
	std::map<std::string, std::string> Saved;
	M.SaveState(Saved);
	C.SaveState(Saved);
	FModel M2;
	FCups C2;
	M2.Load(Text, Error);
	C2.Load(CupText, Error);
	C2.Bind(M2);
	M2.LoadState(Saved);
	C2.LoadState(Saved);
	const FBracket L1 = C.Bracket(M, Chess, Sat3, Day(Sat3, 23.f)), L2 = C2.Bracket(M2, Chess, Sat3, Day(Sat3, 23.f));
	CHECK(C2.IsSigned(Chess, Sat) && C2.IsSigned(Chess, Sat2) && C2.Record(Chess, Sat) == "WWW" && C2.Record(Chess, Sat3) == "L" && C2.CupsWon() == 1 && M2.HasStamp(CD.Collect)
		&& std::equal(std::begin(L1.Winner), std::end(L1.Winner), std::begin(L2.Winner)), "the save keeps the sheets, the results and the cup");
	C2.Reset();
	CHECK(!C2.IsSigned(Chess, Sat) && C2.CupsWon() == 0, "a new game forgets them");

	// 5. the board's pages and their words
	std::string Ru;
	std::set<std::string> Keys;
	if (ReadFile("unreal/Lyudmilovsk/Content/Localization/city/strings_ru.json", Ru))
	{
		LyudJson::FValue Root;
		std::string E2;
		if (LyudJson::Parse(Ru, Root, E2))
		{
			for (const auto& F : Root.Fields) Keys.insert(F.first);
		}
	}
	std::string TownRu;
	if (ReadFile("unreal/Lyudmilovsk/Content/Localization/town/strings_ru.json", TownRu))
	{
		LyudJson::FValue Root;
		std::string E2;
		if (LyudJson::Parse(TownRu, Root, E2))
		{
			for (const auto& F : Root.Fields) Keys.insert(F.first);
		}
	}
	int Missing = 0, Pages = 0;
	std::string FirstMissing;
	for (int Cp = 0; Cp < (int)C.Defs.size(); ++Cp)
	{
		for (long long Dd = 0; Dd < 30; ++Dd)
		{
			for (float H : { 9.f, 14.f, 20.f })
			{
				const std::vector<FCupPage> P = C.Pages(M, Cp, Day(Dd, H));
				Pages += (int)P.size();
				for (const FCupPage& Pg : P)
				{
					for (const FCupLine& L : Pg)
					{
						std::vector<std::string> Need = { L.Key };
						for (const std::string& A : L.Args) if (!A.empty() && A[0] == '@') Need.push_back(A.substr(1));
						for (const std::string& K : Need)
						{
							if (!Keys.count(K)) { ++Missing; if (FirstMissing.empty()) FirstMissing = K; }
						}
					}
				}
			}
		}
	}
	CHECK(!Keys.empty() && Missing == 0 && Pages >= 3 * 30 * 3 * 2, "the boards' pages over a month: %d pages, every word in the tables (%d missing, %s)", Pages, Missing, FirstMissing.c_str());
	const std::vector<FCupPage> P = C.Pages(M, Chess, Day(Sat, 23.f));
	CHECK(P.size() == 3 && P[1].size() == 1 + 3 + 7 + 1 && P[1].back().Key == "city.cup.champion_you" && P[0].back().Key == "city.cup.signed",
		"Saturday night's board: the bracket with the hero as champion, his name on next week's sheet");
	const std::vector<FCupPage> First0 = C2.Pages(M2, Chess, Day(0, 12.f));
	CHECK(First0.size() == 2 && First0[1].size() == 2 && First0[1][1].Key == "city.cup.case.empty", "the first day: no bracket yet, the cup case empty");
}
