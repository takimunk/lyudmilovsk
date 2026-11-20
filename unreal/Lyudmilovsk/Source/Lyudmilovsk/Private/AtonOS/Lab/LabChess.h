#pragma once

// The chess program of the lab's terminals (ATON OS, docs/aton_lab.md): the rules of chess and a small engine to play
// against. Plain C++ (no Unreal types), so tools/atonsim builds and tests it on any machine.

#include <cstdint>
#include <string>
#include <vector>

namespace AtonLab
{
namespace Chess
{
	/** A move by its squares: 0..63, a1 = 0, b1 = 1, h8 = 63. Promo 'q', 'r', 'b', 'n' or 0. */
	struct FChessMove
	{
		int From = -1;
		int To = -1;
		char Promo = 0;

		bool operator==(const FChessMove& Other) const { return From == Other.From && To == Other.To && Promo == Other.Promo; }
		bool Valid() const { return From >= 0 && From < 64 && To >= 0 && To < 64 && From != To; }
	};

	enum class EChessResult { Playing, WhiteMates, BlackMates, Stalemate, DrawFifty, DrawRepetition, DrawMaterial };

	namespace Detail
	{
		/** A position: the board ('P','N','B','R','Q','K' white, lowercase black, '.' empty) and what a FEN adds to it. */
		struct FPos
		{
			char B[64];
			bool bWhite = true;
			/** 1 white short, 2 white long, 4 black short, 8 black long. */
			int Castle = 0;
			/** The square a pawn has just passed over (-1: none). */
			int Ep = -1;
			int Half = 0;
			int Full = 1;
			/** Where the kings stand: [0] white, [1] black. */
			int KingSq[2] = { 4, 60 };
		};

		/** What Make changed, for Unmake. */
		struct FUndo
		{
			char Captured = '.';
			int CapSq = -1;
			int Castle = 0;
			int Ep = -1;
			int Half = 0;
			char Moved = '.';
		};
	}

	class FChessGame
	{
	public:
		/** The start position. */
		FChessGame();
		void Reset();

		/** False on a bad FEN (the position stays as it was). */
		bool SetFen(const std::string& Fen);
		std::string Fen() const;

		bool WhiteToMove() const { return Pos.bWhite; }
		/** 'P','N','B','R','Q','K' white, lowercase black, '.' empty. */
		char At(int Sq) const { return Sq >= 0 && Sq < 64 ? Pos.B[Sq] : '.'; }

		std::vector<FChessMove> LegalMoves() const;
		std::vector<FChessMove> LegalMovesFrom(int Sq) const;
		/** False if illegal. A pawn move to the last rank with Promo 0 promotes to a queen. */
		bool Play(const FChessMove& M);
		/** Takes back one ply; false if there is none. */
		bool Undo();

		/** The side to move is in check. */
		bool InCheck() const;
		EChessResult Result() const;
		int Plies() const { return (int)Hist.size(); }
		/** An invalid move when nothing was played. */
		FChessMove LastMove() const { return Hist.empty() ? FChessMove() : Hist.back().Move; }

		/** A legal move of the current position in algebraic notation with ASCII letters: "e4", "Nf3", "exd5", "O-O", "e8=Q", "+" / "#". */
		std::string Notation(const FChessMove& M) const;

		/**
		 * The engine's move: Level 1 a beginner (shallow, now and then a weaker move, by Seed), 2 a club player, 3 as strong as
		 * a call of about a tenth of a second allows. An invalid move when the game is over. The same for the same position
		 * (with its history), Level and Seed.
		 */
		FChessMove BestMove(int Level, unsigned Seed) const;

		/** The whole game (start FEN and moves) as one ASCII line without '|' or a line break. */
		std::string Save() const;
		bool Restore(const std::string& S);

	private:
		struct FPly
		{
			FChessMove Move;
			Detail::FUndo Undo;
		};

		Detail::FPos Pos;
		std::string StartFen;
		std::vector<FPly> Hist;
		/** The key of the start position and of the position after every ply (repetition). */
		std::vector<std::uint64_t> Keys;
	};
}
}
