// Offline checks of the lab's chess program (Private/AtonOS/Lab/LabChess.cpp): the rules by perft, the notation, the
// draws, saving, and the engine (legal moves only, mate in one, time per move). Built and run by tools/atonsim/run.sh.

#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/AtonOS/Lab/LabChess.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using AtonLab::Chess::EChessResult;
using AtonLab::Chess::FChessGame;
using AtonLab::Chess::FChessMove;

static int GChecks = 0;
static int GFailed = 0;

static void Expect(bool bOk, const std::string& What)
{
	++GChecks;
	if (!bOk)
	{
		++GFailed;
		std::printf("FAIL %s\n", What.c_str());
	}
}

static int Sq(const char* Name)
{
	return (Name[1] - '1') * 8 + (Name[0] - 'a');
}

static FChessMove Mv(const char* Text)
{
	FChessMove M;
	M.From = Sq(Text);
	M.To = Sq(Text + 2);
	M.Promo = Text[4];
	return M;
}

static long Perft(FChessGame& G, int Depth)
{
	const std::vector<FChessMove> Moves = G.LegalMoves();
	if (Depth <= 1)
	{
		return (long)Moves.size();
	}
	long N = 0;
	for (const FChessMove& M : Moves)
	{
		G.Play(M);
		N += Perft(G, Depth - 1);
		G.Undo();
	}
	return N;
}

static void PerftCase(const char* Name, const char* Fen, const std::vector<long>& Want)
{
	FChessGame G;
	Expect(G.SetFen(Fen), std::string("fen ") + Name);
	for (size_t D = 0; D < Want.size(); ++D)
	{
		const long Got = Perft(G, (int)D + 1);
		Expect(Got == Want[D], std::string("perft ") + Name + " depth " + std::to_string(D + 1) + ": " + std::to_string(Got) + " (want "
			+ std::to_string(Want[D]) + ")");
	}
	Expect(G.Fen() == Fen, std::string("perft leaves the position: ") + Name);
}

static void Note(const char* Fen, const char* Move, const char* Want)
{
	FChessGame G;
	Expect(G.SetFen(Fen), std::string("fen for ") + Want);
	const std::string Got = G.Notation(Mv(Move));
	Expect(Got == Want, std::string("notation ") + Move + ": " + Got + " (want " + Want + ")");
}

static double MoveMs(const FChessGame& G, int Level, FChessMove& Out)
{
	const auto T0 = std::chrono::steady_clock::now();
	Out = G.BestMove(Level, 7u);
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
}

static bool IsLegal(const FChessGame& G, const FChessMove& M)
{
	for (const FChessMove& L : G.LegalMoves())
	{
		if (L == M)
		{
			return true;
		}
	}
	return false;
}

int main()
{
	const char* Start = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
	const char* Kiwi = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
	const char* Pos3 = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1";

	PerftCase("start", Start, { 20, 400, 8902, 197281 });
	PerftCase("kiwipete", Kiwi, { 48, 2039, 97862 });
	PerftCase("position 3", Pos3, { 14, 191, 2812, 43238 });

	// FEN
	{
		FChessGame G;
		Expect(G.Fen() == Start, "start FEN");
		Expect(G.Play(Mv("e2e4")), "1. e4");
		Expect(G.Fen() == "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1", "FEN after 1. e4: " + G.Fen());
		const char* Fens[] = { Kiwi, Pos3, "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2", "8/8/8/4k3/8/8/4K2R/8 b - - 37 80" };
		for (const char* F : Fens)
		{
			FChessGame H;
			Expect(H.SetFen(F) && H.Fen() == F, std::string("FEN round trip ") + F);
		}
		const char* Bad[] = { "", "hello", "8/8/8/8/8/8/8/8 w - - 0 1", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1",
			"rnbqkbnr/pppppppp/9/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP w KQkq - 0 1",
			"4k3/8/8/8/8/8/8/4K2R w - e9 0 1", "4k3/4R3/8/8/8/8/8/4K3 w - - 0 1" };
		for (const char* F : Bad)
		{
			FChessGame H;
			H.Play(Mv("e2e4"));
			const std::string Before = H.Fen();
			Expect(!H.SetFen(F) && H.Fen() == Before && H.Plies() == 1, std::string("bad FEN refused: ") + F);
		}
		Expect(G.At(Sq("e4")) == 'P' && G.At(Sq("e2")) == '.' && G.At(Sq("e8")) == 'k' && !G.WhiteToMove(), "At / WhiteToMove");
		Expect(G.LastMove() == Mv("e2e4") && G.Plies() == 1, "LastMove / Plies");
		Expect(!G.Play(Mv("e2e4")) && !G.Play(Mv("e7e4")) && !G.Play(FChessMove()), "illegal moves refused");
		Expect(G.LegalMovesFrom(Sq("g8")).size() == 2 && G.LegalMovesFrom(Sq("e4")).empty(), "LegalMovesFrom");
	}

	// undo
	{
		FChessGame G;
		G.SetFen(Kiwi);
		const char* Line[] = { "e1g1", "e8c8", "d5e6", "h3g2", "e6f7", "g2f1q" };
		std::vector<std::string> Fens;
		for (const char* M : Line)
		{
			Fens.push_back(G.Fen());
			Expect(G.Play(Mv(M)), std::string("play ") + M);
		}
		for (size_t I = Fens.size(); I-- > 0;)
		{
			Expect(G.Undo() && G.Fen() == Fens[I], "undo restores ply " + std::to_string(I));
		}
		Expect(!G.Undo(), "nothing left to undo");
	}

	// results
	{
		FChessGame G;
		Expect(G.Result() == EChessResult::Playing && !G.InCheck(), "start: playing");
		G.SetFen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
		Expect(G.Result() == EChessResult::Stalemate && G.LegalMoves().empty() && !G.BestMove(3, 1u).Valid(), "stalemate");
		G.SetFen("6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1");
		Expect(G.Play(Mv("a1a8")) && G.Result() == EChessResult::WhiteMates && G.InCheck(), "back-rank mate");
		G.Reset();
		for (const char* M : { "f2f3", "e7e5", "g2g4", "d8h4" })
		{
			G.Play(Mv(M));
		}
		Expect(G.Result() == EChessResult::BlackMates, "fool's mate");
		G.SetFen("8/8/8/4k3/8/8/4K2R/8 w - - 99 60");
		Expect(G.Result() == EChessResult::Playing && G.Play(Mv("h2h3")) && G.Result() == EChessResult::DrawFifty, "fifty moves");
		G.Reset();
		const char* Shuffle[] = { "g1f3", "g8f6", "f3g1", "f6g8", "g1f3", "g8f6", "f3g1" };
		for (const char* M : Shuffle)
		{
			G.Play(Mv(M));
		}
		Expect(G.Result() == EChessResult::Playing, "twice is not a draw");
		G.Play(Mv("f6g8"));
		Expect(G.Result() == EChessResult::DrawRepetition, "threefold repetition");
		G.Undo();
		Expect(G.Result() == EChessResult::Playing, "undo takes the repetition back");
		G.SetFen("8/8/8/4k3/8/8/4K3/8 w - - 0 1");
		Expect(G.Result() == EChessResult::DrawMaterial, "K v K");
		G.SetFen("8/8/8/4k3/8/8/4KN2/8 w - - 0 1");
		Expect(G.Result() == EChessResult::DrawMaterial, "K+N v K");
		G.SetFen("8/8/8/4kb2/8/8/4K3/8 b - - 0 1");
		Expect(G.Result() == EChessResult::DrawMaterial, "K v K+B");
		G.SetFen("8/8/8/4k3/8/8/4KP2/8 w - - 0 1");
		Expect(G.Result() == EChessResult::Playing, "K+P v K plays on");
		// en passant and the default promotion
		G.SetFen("4k3/8/8/8/5p2/8/4P3/4K3 w - - 0 1");
		Expect(G.Play(Mv("e2e4")) && G.Play(Mv("f4e3")) && G.At(Sq("e4")) == '.' && G.At(Sq("e3")) == 'p', "en passant");
		G.SetFen("7k/P7/8/8/8/8/8/K7 w - - 0 1");
		Expect(G.Play(Mv("a7a8")) && G.At(Sq("a8")) == 'Q', "a bare pawn move to the last rank makes a queen");
		G.SetFen("7k/P7/8/8/8/8/8/K7 w - - 0 1");
		Expect(G.Play(Mv("a7a8n")) && G.At(Sq("a8")) == 'N', "underpromotion");
	}

	// notation
	Note(Start, "e2e4", "e4");
	Note(Start, "g1f3", "Nf3");
	Note(Kiwi, "e1g1", "O-O");
	Note(Kiwi, "e1c1", "O-O-O");
	Note(Kiwi, "e5g6", "Nxg6");
	Note(Kiwi, "d5e6", "dxe6");
	Note(Kiwi, "e2a6", "Bxa6");
	Note("7k/P7/8/8/8/8/8/K7 w - - 0 1", "a7a8q", "a8=Q+");
	Note("7k/P7/8/8/8/8/8/K7 w - - 0 1", "a7a8", "a8=Q+");
	Note("6k1/P7/8/8/8/8/8/K7 w - - 0 1", "a7a8n", "a8=N");
	Note("6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1", "a1a8", "Ra8#");
	Note("4k3/8/8/8/8/5N2/8/1N2K3 w - - 0 1", "b1d2", "Nbd2");
	Note("4k3/8/8/8/8/5N2/8/1N2K3 w - - 0 1", "f3d2", "Nfd2");
	Note("4k3/8/8/8/R7/8/8/R3K3 w - - 0 1", "a1a2", "R1a2");
	Note("4k3/8/8/8/5p2/8/4P3/4K3 b - - 0 1", "f4f3", "f3");
	Note("rnbqkbnr/ppp1pppp/8/8/3pP3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 2", "d4e3", "dxe3");
	Note("4k3/8/8/8/8/8/8/4K2R w K - 0 1", "h1h8", "Rh8+");
	{
		FChessGame G;
		Expect(G.Notation(Mv("e2e5")).empty(), "no notation for an illegal move");
	}

	// save / restore
	{
		FChessGame G;
		G.SetFen(Kiwi);
		for (const char* M : { "e1g1", "e8c8", "d5e6", "h3g2", "e6f7", "g2f1q" })
		{
			G.Play(Mv(M));
		}
		const std::string Saved = G.Save();
		Expect(Saved.find('|') == std::string::npos && Saved.find('\n') == std::string::npos, "save is one line without |");
		FChessGame H;
		Expect(H.Restore(Saved) && H.Fen() == G.Fen() && H.Plies() == G.Plies() && H.Save() == Saved, "restore gives the same game");
		Expect(H.Undo() && H.Undo() && H.Fen() != G.Fen(), "a restored game can be taken back");
		FChessGame K;
		K.Play(Mv("d2d4"));
		const std::string Before = K.Fen();
		Expect(!K.Restore("nonsense") && !K.Restore(std::string(Start) + ";e2e5") && !K.Restore(std::string(Start) + ";e2") && K.Fen() == Before,
			"a broken save changes nothing");
		FChessGame Fresh;
		Expect(K.Restore(Fresh.Save()) && K.Fen() == Start && K.Plies() == 0, "an empty game restores");
	}

	// the engine
	{
		const char* Mates[][2] = {
			{ "6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1", "a1a8" },
			{ "r5k1/5ppp/8/8/8/8/5PPP/6K1 b - - 0 1", "a8a1" },
			{ "7k/5Q2/5K2/8/8/8/8/8 w - - 0 1", "f7g7" },
		};
		for (int Level = 2; Level <= 3; ++Level)
		{
			for (const auto& Case : Mates)
			{
				FChessGame G;
				G.SetFen(Case[0]);
				const FChessMove M = G.BestMove(Level, 3u);
				FChessGame After = G;
				const bool bMates = After.Play(M) && (After.Result() == EChessResult::WhiteMates || After.Result() == EChessResult::BlackMates);
				Expect(bMates, std::string("mate in one at level ") + std::to_string(Level) + ": " + Case[0]);
			}
		}
		// a hanging queen is taken at every level's best
		{
			FChessGame G;
			G.SetFen("4k3/8/8/3q4/8/8/8/3RK3 w - - 0 1");
			Expect(G.BestMove(2, 1u) == Mv("d1d5") && G.BestMove(3, 1u) == Mv("d1d5"), "the free queen is taken");
		}
		FChessGame G;
		Expect(G.BestMove(1, 5u) == G.BestMove(1, 5u) && G.BestMove(3, 5u) == G.BestMove(3, 9u), "the same move for the same position, level and seed");
		const int Pairs[3][2] = { { 1, 3 }, { 3, 2 }, { 2, 1 } };
		for (int Game = 0; Game < 3; ++Game)
		{
			FChessGame Play;
			bool bAllLegal = true;
			while (Play.Result() == EChessResult::Playing && Play.Plies() < 300)
			{
				const int Level = Pairs[Game][Play.WhiteToMove() ? 0 : 1];
				const FChessMove M = Play.BestMove(Level, 100u + (unsigned)Game * 17u + (unsigned)Play.Plies());
				if (!IsLegal(Play, M) || !Play.Play(M))
				{
					bAllLegal = false;
					break;
				}
			}
			static const char* Names[] = { "playing", "white mates", "black mates", "stalemate", "fifty moves", "repetition", "material" };
			std::printf("game %d (level %d v %d): %d plies, %s\n", Game + 1, Pairs[Game][0], Pairs[Game][1], Play.Plies(),
				Names[(int)Play.Result()]);
			Expect(bAllLegal, "engine game " + std::to_string(Game + 1) + ": every move legal");
			Expect(Play.Result() != EChessResult::Playing || Play.Plies() >= 300, "engine game " + std::to_string(Game + 1) + " ends");
			FChessGame Again;
			Expect(Again.Restore(Play.Save()) && Again.Fen() == Play.Fen(), "engine game " + std::to_string(Game + 1) + " saves and restores");
		}
		// time per move
		for (const char* Fen : { Start, Kiwi })
		{
			FChessGame T;
			T.SetFen(Fen);
			for (int Level = 1; Level <= 3; ++Level)
			{
				FChessMove M;
				double Worst = 0.0;
				for (int Run = 0; Run < 3; ++Run)
				{
					const double Ms = MoveMs(T, Level, M);
					Worst = Ms > Worst ? Ms : Worst;
				}
				std::printf("time level %d %s: %.1f ms (%s)\n", Level, Fen == Start ? "start" : "kiwipete", Worst, T.Notation(M).c_str());
				Expect(IsLegal(T, M), "timed move is legal");
				if (Level == 3)
				{
					Expect(Worst < 400.0, std::string("level 3 under 400 ms: ") + std::to_string(Worst));
				}
			}
		}
	}

	std::printf("chess: %d checks, %d failed\n", GChecks, GFailed);
	return GFailed ? 1 : 0;
}
