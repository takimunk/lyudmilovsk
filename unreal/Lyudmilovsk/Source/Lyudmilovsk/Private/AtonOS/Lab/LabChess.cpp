#include "LabChess.h"

#include <cstdlib>

namespace AtonLab
{
namespace Chess
{
	namespace Detail
	{
		const char* const StartPosition = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
		const int MateScore = 30000;
		const int InfScore = 32000;
		const int MaxPly = 40;

		struct FMoveList
		{
			FChessMove M[256];
			int N = 0;
			void Add(int From, int To, char Promo = 0)
			{
				if (N < 256)
				{
					M[N].From = From;
					M[N].To = To;
					M[N].Promo = Promo;
					++N;
				}
			}
		};

		inline bool IsWhitePiece(char C) { return C >= 'A' && C <= 'Z'; }
		inline bool IsBlackPiece(char C) { return C >= 'a' && C <= 'z'; }
		inline char LowerPiece(char C) { return IsWhitePiece(C) ? (char)(C + 32) : C; }
		inline int FileOf(int Sq) { return Sq & 7; }
		inline int RankOf(int Sq) { return Sq >> 3; }
		inline bool OnBoard(int F, int R) { return F >= 0 && F < 8 && R >= 0 && R < 8; }
		inline bool IsOwn(char C, bool bWhite) { return bWhite ? IsWhitePiece(C) : IsBlackPiece(C); }
		inline bool IsEnemy(char C, bool bWhite) { return bWhite ? IsBlackPiece(C) : IsWhitePiece(C); }

		const int KnightStep[8][2] = { { 1, 2 }, { 2, 1 }, { 2, -1 }, { 1, -2 }, { -1, -2 }, { -2, -1 }, { -2, 1 }, { -1, 2 } };
		const int KingStep[8][2] = { { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 }, { 0, -1 }, { 1, -1 } };

		/** Sq is attacked by a piece of the given colour. */
		bool Attacked(const FPos& P, int Sq, bool bByWhite)
		{
			const int F = FileOf(Sq), R = RankOf(Sq);
			// pawns: a white pawn attacks up the board, so it stands one rank below
			const int PawnRank = R + (bByWhite ? -1 : 1);
			const char Pawn = bByWhite ? 'P' : 'p';
			if (PawnRank >= 0 && PawnRank < 8)
			{
				if (F > 0 && P.B[PawnRank * 8 + F - 1] == Pawn) { return true; }
				if (F < 7 && P.B[PawnRank * 8 + F + 1] == Pawn) { return true; }
			}
			const char Knight = bByWhite ? 'N' : 'n';
			const char King = bByWhite ? 'K' : 'k';
			for (int I = 0; I < 8; ++I)
			{
				const int Nf = F + KnightStep[I][0], Nr = R + KnightStep[I][1];
				if (OnBoard(Nf, Nr) && P.B[Nr * 8 + Nf] == Knight) { return true; }
				const int Kf = F + KingStep[I][0], Kr = R + KingStep[I][1];
				if (OnBoard(Kf, Kr) && P.B[Kr * 8 + Kf] == King) { return true; }
			}
			const char Queen = bByWhite ? 'Q' : 'q';
			const char Rook = bByWhite ? 'R' : 'r';
			const char Bishop = bByWhite ? 'B' : 'b';
			for (int I = 0; I < 8; ++I)
			{
				const int Df = KingStep[I][0], Dr = KingStep[I][1];
				const bool bDiagonal = Df != 0 && Dr != 0;
				int Cf = F + Df, Cr = R + Dr;
				while (OnBoard(Cf, Cr))
				{
					const char C = P.B[Cr * 8 + Cf];
					if (C != '.')
					{
						if (C == Queen || C == (bDiagonal ? Bishop : Rook)) { return true; }
						break;
					}
					Cf += Df;
					Cr += Dr;
				}
			}
			return false;
		}

		inline bool KingAttacked(const FPos& P, bool bWhiteKing)
		{
			return Attacked(P, P.KingSq[bWhiteKing ? 0 : 1], !bWhiteKing);
		}

		int CastleMask(int Sq)
		{
			switch (Sq)
			{
			case 4: return 3;
			case 0: return 2;
			case 7: return 1;
			case 60: return 12;
			case 56: return 8;
			case 63: return 4;
			default: return 0;
			}
		}

		void Make(FPos& P, const FChessMove& M, FUndo& U)
		{
			char Piece = P.B[M.From];
			const bool bWhite = IsWhitePiece(Piece);
			const char Kind = LowerPiece(Piece);
			U.Moved = Piece;
			U.Castle = P.Castle;
			U.Ep = P.Ep;
			U.Half = P.Half;
			U.CapSq = M.To;
			U.Captured = P.B[M.To];
			if (Kind == 'p' && M.To == P.Ep && U.Captured == '.' && FileOf(M.From) != FileOf(M.To))
			{
				U.CapSq = M.To + (bWhite ? -8 : 8);
				U.Captured = P.B[U.CapSq];
				P.B[U.CapSq] = '.';
			}
			P.Ep = -1;
			if (Kind == 'p')
			{
				const int ToRank = RankOf(M.To);
				if (ToRank == 7 || ToRank == 0)
				{
					const char Promo = M.Promo ? M.Promo : 'q';
					Piece = bWhite ? (char)(Promo - 32) : Promo;
				}
				else if (M.To - M.From == 16 || M.From - M.To == 16)
				{
					P.Ep = (M.From + M.To) / 2;
				}
			}
			P.B[M.From] = '.';
			P.B[M.To] = Piece;
			if (Kind == 'k')
			{
				P.KingSq[bWhite ? 0 : 1] = M.To;
				if (M.To - M.From == 2)
				{
					P.B[M.To - 1] = P.B[M.To + 1];
					P.B[M.To + 1] = '.';
				}
				else if (M.From - M.To == 2)
				{
					P.B[M.To + 1] = P.B[M.To - 2];
					P.B[M.To - 2] = '.';
				}
			}
			P.Castle &= ~(CastleMask(M.From) | CastleMask(M.To));
			P.Half = (Kind == 'p' || U.Captured != '.') ? 0 : P.Half + 1;
			if (!bWhite)
			{
				++P.Full;
			}
			P.bWhite = !P.bWhite;
		}

		void Unmake(FPos& P, const FChessMove& M, const FUndo& U)
		{
			P.bWhite = !P.bWhite;
			const bool bWhite = IsWhitePiece(U.Moved);
			if (!bWhite)
			{
				--P.Full;
			}
			if (LowerPiece(U.Moved) == 'k')
			{
				P.KingSq[bWhite ? 0 : 1] = M.From;
				if (M.To - M.From == 2)
				{
					P.B[M.To + 1] = P.B[M.To - 1];
					P.B[M.To - 1] = '.';
				}
				else if (M.From - M.To == 2)
				{
					P.B[M.To - 2] = P.B[M.To + 1];
					P.B[M.To + 1] = '.';
				}
			}
			P.B[M.From] = U.Moved;
			P.B[M.To] = '.';
			if (U.Captured != '.')
			{
				P.B[U.CapSq] = U.Captured;
			}
			P.Castle = U.Castle;
			P.Ep = U.Ep;
			P.Half = U.Half;
		}

		void AddPawnMove(FMoveList& L, int From, int To)
		{
			const int ToRank = RankOf(To);
			if (ToRank == 7 || ToRank == 0)
			{
				L.Add(From, To, 'q');
				L.Add(From, To, 'r');
				L.Add(From, To, 'b');
				L.Add(From, To, 'n');
			}
			else
			{
				L.Add(From, To);
			}
		}

		/** Every move of the side to move by the pieces' rules; the mover's king may be left in check. */
		void GenPseudo(const FPos& P, FMoveList& L)
		{
			const bool bWhite = P.bWhite;
			for (int Sq = 0; Sq < 64; ++Sq)
			{
				const char C = P.B[Sq];
				if (!IsOwn(C, bWhite))
				{
					continue;
				}
				const int F = FileOf(Sq), R = RankOf(Sq);
				const char Kind = LowerPiece(C);
				if (Kind == 'p')
				{
					const int Dir = bWhite ? 1 : -1;
					const int Nr = R + Dir;
					if (Nr < 0 || Nr > 7)
					{
						continue;
					}
					if (P.B[Nr * 8 + F] == '.')
					{
						AddPawnMove(L, Sq, Nr * 8 + F);
						if (R == (bWhite ? 1 : 6) && P.B[(Nr + Dir) * 8 + F] == '.')
						{
							L.Add(Sq, (Nr + Dir) * 8 + F);
						}
					}
					for (int Df = -1; Df <= 1; Df += 2)
					{
						const int Nf = F + Df;
						if (Nf < 0 || Nf > 7)
						{
							continue;
						}
						const int To = Nr * 8 + Nf;
						if (IsEnemy(P.B[To], bWhite) || (To == P.Ep && P.B[To] == '.'))
						{
							AddPawnMove(L, Sq, To);
						}
					}
				}
				else if (Kind == 'n' || Kind == 'k')
				{
					for (int I = 0; I < 8; ++I)
					{
						const int Nf = F + (Kind == 'n' ? KnightStep[I][0] : KingStep[I][0]);
						const int Nr = R + (Kind == 'n' ? KnightStep[I][1] : KingStep[I][1]);
						if (OnBoard(Nf, Nr) && !IsOwn(P.B[Nr * 8 + Nf], bWhite))
						{
							L.Add(Sq, Nr * 8 + Nf);
						}
					}
					if (Kind == 'k')
					{
						const int Home = bWhite ? 4 : 60;
						const int Short = bWhite ? 1 : 4, Long = bWhite ? 2 : 8;
						const char Rook = bWhite ? 'R' : 'r';
						if (Sq == Home && (P.Castle & (Short | Long)) && !Attacked(P, Home, !bWhite))
						{
							if ((P.Castle & Short) && P.B[Home + 1] == '.' && P.B[Home + 2] == '.' && P.B[Home + 3] == Rook
								&& !Attacked(P, Home + 1, !bWhite))
							{
								L.Add(Home, Home + 2);
							}
							if ((P.Castle & Long) && P.B[Home - 1] == '.' && P.B[Home - 2] == '.' && P.B[Home - 3] == '.'
								&& P.B[Home - 4] == Rook && !Attacked(P, Home - 1, !bWhite))
							{
								L.Add(Home, Home - 2);
							}
						}
					}
				}
				else
				{
					for (int I = 0; I < 8; ++I)
					{
						const int Df = KingStep[I][0], Dr = KingStep[I][1];
						const bool bDiagonal = Df != 0 && Dr != 0;
						if ((Kind == 'b' && !bDiagonal) || (Kind == 'r' && bDiagonal))
						{
							continue;
						}
						int Cf = F + Df, Cr = R + Dr;
						while (OnBoard(Cf, Cr))
						{
							const char T = P.B[Cr * 8 + Cf];
							if (IsOwn(T, bWhite))
							{
								break;
							}
							L.Add(Sq, Cr * 8 + Cf);
							if (T != '.')
							{
								break;
							}
							Cf += Df;
							Cr += Dr;
						}
					}
				}
			}
		}

		void GenLegal(const FPos& Position, FMoveList& Out)
		{
			FPos P = Position;
			FMoveList All;
			GenPseudo(P, All);
			const bool bWhite = P.bWhite;
			for (int I = 0; I < All.N; ++I)
			{
				FUndo U;
				Make(P, All.M[I], U);
				if (!KingAttacked(P, bWhite))
				{
					Out.Add(All.M[I].From, All.M[I].To, All.M[I].Promo);
				}
				Unmake(P, All.M[I], U);
			}
		}

		bool HasLegalMove(const FPos& Position)
		{
			FPos P = Position;
			FMoveList All;
			GenPseudo(P, All);
			const bool bWhite = P.bWhite;
			for (int I = 0; I < All.N; ++I)
			{
				FUndo U;
				Make(P, All.M[I], U);
				const bool bLegal = !KingAttacked(P, bWhite);
				Unmake(P, All.M[I], U);
				if (bLegal)
				{
					return true;
				}
			}
			return false;
		}

		/** The position's key for repetition: board, side, castling rights and the en passant square when it can be taken. */
		std::uint64_t KeyOf(const FPos& P)
		{
			std::uint64_t H = 1469598103934665603ull;
			auto Mix = [&H](unsigned V)
			{
				H ^= (std::uint64_t)V;
				H *= 1099511628211ull;
			};
			for (int Sq = 0; Sq < 64; ++Sq)
			{
				Mix((unsigned char)P.B[Sq]);
			}
			Mix(P.bWhite ? 1u : 2u);
			Mix((unsigned)P.Castle + 16u);
			int Ep = -1;
			if (P.Ep >= 0)
			{
				const int F = FileOf(P.Ep);
				const int R = RankOf(P.Ep) + (P.bWhite ? -1 : 1);
				const char Pawn = P.bWhite ? 'P' : 'p';
				if ((F > 0 && P.B[R * 8 + F - 1] == Pawn) || (F < 7 && P.B[R * 8 + F + 1] == Pawn))
				{
					Ep = P.Ep;
				}
			}
			Mix((unsigned)(Ep + 1) + 64u);
			return H;
		}

		int PieceValue(char Kind)
		{
			switch (Kind)
			{
			case 'p': return 100;
			case 'n': return 320;
			case 'b': return 330;
			case 'r': return 500;
			case 'q': return 900;
			default: return 0;
			}
		}

		/** Where a piece likes to stand: F, R as its own side sees the board (R 0 = its back rank). */
		int SquareBonus(char Kind, int F, int R, bool bEndgame)
		{
			const int Fd = F < 4 ? 3 - F : F - 4;
			const int Rd = R < 4 ? 3 - R : R - 4;
			const int Cd = Fd + Rd;
			const bool bCentreFile = F == 3 || F == 4;
			switch (Kind)
			{
			case 'p':
				if (bEndgame)
				{
					return (R - 1) * 14;
				}
				return (R - 1) * 6 + (bCentreFile && (R == 3 || R == 4) ? 18 : 0) - (bCentreFile && R == 1 ? 15 : 0);
			case 'n': return 20 - 10 * Cd;
			case 'b': return 10 - 5 * Cd;
			case 'r': return (R == 6 ? 15 : 0) + (bCentreFile ? 5 : 0);
			case 'q': return -2 * Cd;
			case 'k':
				if (bEndgame)
				{
					return 20 - 10 * Cd;
				}
				return -20 * R + (R == 0 ? ((F == 6 || F == 2 || F == 1) ? 25 : (F >= 3 && F <= 5) ? -10 : 5) : 0);
			default: return 0;
			}
		}

		/** The position's worth for the side to move, in hundredths of a pawn. */
		int Evaluate(const FPos& P)
		{
			int Heavy = 0;
			bool bQueens = false;
			for (int Sq = 0; Sq < 64; ++Sq)
			{
				const char Kind = LowerPiece(P.B[Sq]);
				if (Kind == 'q')
				{
					bQueens = true;
				}
				if (Kind != '.' && Kind != 'p' && Kind != 'k')
				{
					Heavy += PieceValue(Kind);
				}
			}
			const bool bEndgame = !bQueens || Heavy <= 2600;
			int Score = 0;
			for (int Sq = 0; Sq < 64; ++Sq)
			{
				const char C = P.B[Sq];
				if (C == '.')
				{
					continue;
				}
				const bool bWhite = IsWhitePiece(C);
				const char Kind = LowerPiece(C);
				const int V = PieceValue(Kind) + SquareBonus(Kind, FileOf(Sq), bWhite ? RankOf(Sq) : 7 - RankOf(Sq), bEndgame);
				Score += bWhite ? V : -V;
			}
			return P.bWhite ? Score : -Score;
		}

		struct FSearch
		{
			FPos P;
			long Nodes = 0;
			long Budget = 0;
			bool bAbort = false;
		};

		bool IsNoisy(const FPos& P, const FChessMove& M)
		{
			return P.B[M.To] != '.' || M.Promo != 0 || (LowerPiece(P.B[M.From]) == 'p' && M.To == P.Ep);
		}

		int OrderScore(const FPos& P, const FChessMove& M)
		{
			const char Mover = LowerPiece(P.B[M.From]);
			const char Victim = LowerPiece(P.B[M.To]);
			int S = 0;
			if (Victim != '.')
			{
				S += 10000 + 10 * PieceValue(Victim) - PieceValue(Mover) / 10;
			}
			else if (Mover == 'p' && M.To == P.Ep)
			{
				S += 10000 + 990;
			}
			if (M.Promo)
			{
				S += 9000 + PieceValue(M.Promo);
			}
			if (S == 0)
			{
				const bool bWhite = P.bWhite;
				const int Fr = bWhite ? RankOf(M.From) : 7 - RankOf(M.From);
				const int Tr = bWhite ? RankOf(M.To) : 7 - RankOf(M.To);
				S = SquareBonus(Mover, FileOf(M.To), Tr, false) - SquareBonus(Mover, FileOf(M.From), Fr, false);
			}
			return S;
		}

		/** Best first. */
		void SortMoves(const FPos& P, FMoveList& L)
		{
			int Scores[256];
			for (int I = 0; I < L.N; ++I)
			{
				Scores[I] = OrderScore(P, L.M[I]);
			}
			for (int I = 1; I < L.N; ++I)
			{
				const FChessMove M = L.M[I];
				const int S = Scores[I];
				int J = I - 1;
				while (J >= 0 && Scores[J] < S)
				{
					L.M[J + 1] = L.M[J];
					Scores[J + 1] = Scores[J];
					--J;
				}
				L.M[J + 1] = M;
				Scores[J + 1] = S;
			}
		}

		int Quiesce(FSearch& S, int Alpha, int Beta, int Ply)
		{
			const int Stand = Evaluate(S.P);
			if (S.bAbort || ++S.Nodes > S.Budget)
			{
				S.bAbort = true;
				return Stand;
			}
			if (Stand >= Beta || Ply >= MaxPly)
			{
				return Stand;
			}
			if (Stand > Alpha)
			{
				Alpha = Stand;
			}
			FMoveList All, Noisy;
			GenPseudo(S.P, All);
			for (int I = 0; I < All.N; ++I)
			{
				if (IsNoisy(S.P, All.M[I]))
				{
					Noisy.Add(All.M[I].From, All.M[I].To, All.M[I].Promo);
				}
			}
			SortMoves(S.P, Noisy);
			const bool bWhite = S.P.bWhite;
			for (int I = 0; I < Noisy.N; ++I)
			{
				FUndo U;
				Make(S.P, Noisy.M[I], U);
				if (KingAttacked(S.P, bWhite))
				{
					Unmake(S.P, Noisy.M[I], U);
					continue;
				}
				const int Score = -Quiesce(S, -Beta, -Alpha, Ply + 1);
				Unmake(S.P, Noisy.M[I], U);
				if (Score >= Beta)
				{
					return Score;
				}
				if (Score > Alpha)
				{
					Alpha = Score;
				}
			}
			return Alpha;
		}

		int Negamax(FSearch& S, int Depth, int Alpha, int Beta, int Ply)
		{
			if (S.bAbort)
			{
				return Evaluate(S.P);
			}
			const bool bWhite = S.P.bWhite;
			const bool bInCheck = KingAttacked(S.P, bWhite);
			if (Depth <= 0)
			{
				if (!bInCheck || Ply >= MaxPly)
				{
					return Quiesce(S, Alpha, Beta, Ply);
				}
				// a check is looked at one move further, so a mate is seen as a mate
				Depth = 1;
			}
			if (++S.Nodes > S.Budget)
			{
				S.bAbort = true;
				return Evaluate(S.P);
			}
			FMoveList L;
			GenPseudo(S.P, L);
			SortMoves(S.P, L);
			int Legal = 0;
			int Best = -InfScore;
			for (int I = 0; I < L.N; ++I)
			{
				FUndo U;
				Make(S.P, L.M[I], U);
				if (KingAttacked(S.P, bWhite))
				{
					Unmake(S.P, L.M[I], U);
					continue;
				}
				++Legal;
				const int Score = -Negamax(S, Depth - 1, -Beta, -Alpha, Ply + 1);
				Unmake(S.P, L.M[I], U);
				if (Score > Best)
				{
					Best = Score;
				}
				if (Score > Alpha)
				{
					Alpha = Score;
				}
				if (Alpha >= Beta || S.bAbort)
				{
					break;
				}
			}
			if (Legal == 0)
			{
				return bInCheck ? -MateScore + Ply : 0;
			}
			return Best;
		}

		bool InsufficientMaterial(const FPos& P)
		{
			int Minor = 0;
			for (int Sq = 0; Sq < 64; ++Sq)
			{
				const char Kind = LowerPiece(P.B[Sq]);
				if (Kind == 'p' || Kind == 'r' || Kind == 'q')
				{
					return false;
				}
				if (Kind == 'n' || Kind == 'b')
				{
					++Minor;
				}
			}
			return Minor <= 1;
		}

		std::string SquareName(int Sq)
		{
			std::string S;
			S += (char)('a' + FileOf(Sq));
			S += (char)('1' + RankOf(Sq));
			return S;
		}

		std::string FenOf(const FPos& P)
		{
			std::string S;
			for (int R = 7; R >= 0; --R)
			{
				int Empty = 0;
				for (int F = 0; F < 8; ++F)
				{
					const char C = P.B[R * 8 + F];
					if (C == '.')
					{
						++Empty;
						continue;
					}
					if (Empty)
					{
						S += (char)('0' + Empty);
						Empty = 0;
					}
					S += C;
				}
				if (Empty)
				{
					S += (char)('0' + Empty);
				}
				if (R > 0)
				{
					S += '/';
				}
			}
			S += P.bWhite ? " w " : " b ";
			if (P.Castle == 0)
			{
				S += '-';
			}
			else
			{
				if (P.Castle & 1) { S += 'K'; }
				if (P.Castle & 2) { S += 'Q'; }
				if (P.Castle & 4) { S += 'k'; }
				if (P.Castle & 8) { S += 'q'; }
			}
			S += ' ';
			S += P.Ep >= 0 ? SquareName(P.Ep) : std::string("-");
			S += ' ';
			S += std::to_string(P.Half);
			S += ' ';
			S += std::to_string(P.Full);
			return S;
		}

		bool ParseNumber(const std::string& S, int& Out)
		{
			if (S.empty() || S.size() > 5)
			{
				return false;
			}
			int V = 0;
			for (char C : S)
			{
				if (C < '0' || C > '9')
				{
					return false;
				}
				V = V * 10 + (C - '0');
			}
			Out = V;
			return true;
		}

		bool ParseFen(const std::string& Fen, FPos& Out)
		{
			std::vector<std::string> Parts;
			std::string Cur;
			for (char C : Fen)
			{
				if (C == ' ')
				{
					if (!Cur.empty())
					{
						Parts.push_back(Cur);
						Cur.clear();
					}
				}
				else
				{
					Cur += C;
				}
			}
			if (!Cur.empty())
			{
				Parts.push_back(Cur);
			}
			if (Parts.size() < 2 || Parts.size() > 6)
			{
				return false;
			}
			FPos P;
			for (int Sq = 0; Sq < 64; ++Sq)
			{
				P.B[Sq] = '.';
			}
			int R = 7, F = 0;
			int Kings[2] = { 0, 0 };
			for (char C : Parts[0])
			{
				if (C == '/')
				{
					if (F != 8 || R == 0)
					{
						return false;
					}
					--R;
					F = 0;
				}
				else if (C >= '1' && C <= '8')
				{
					F += C - '0';
					if (F > 8)
					{
						return false;
					}
				}
				else
				{
					const char Kind = LowerPiece(C);
					if (F > 7 || (Kind != 'p' && Kind != 'n' && Kind != 'b' && Kind != 'r' && Kind != 'q' && Kind != 'k'))
					{
						return false;
					}
					if (Kind == 'p' && (R == 0 || R == 7))
					{
						return false;
					}
					if (Kind == 'k')
					{
						const int Side = IsWhitePiece(C) ? 0 : 1;
						++Kings[Side];
						P.KingSq[Side] = R * 8 + F;
					}
					P.B[R * 8 + F] = C;
					++F;
				}
			}
			if (R != 0 || F != 8 || Kings[0] != 1 || Kings[1] != 1)
			{
				return false;
			}
			if (Parts[1] == "w")
			{
				P.bWhite = true;
			}
			else if (Parts[1] == "b")
			{
				P.bWhite = false;
			}
			else
			{
				return false;
			}
			P.Castle = 0;
			if (Parts.size() > 2 && Parts[2] != "-")
			{
				for (char C : Parts[2])
				{
					if (C == 'K' && P.B[4] == 'K' && P.B[7] == 'R') { P.Castle |= 1; }
					else if (C == 'Q' && P.B[4] == 'K' && P.B[0] == 'R') { P.Castle |= 2; }
					else if (C == 'k' && P.B[60] == 'k' && P.B[63] == 'r') { P.Castle |= 4; }
					else if (C == 'q' && P.B[60] == 'k' && P.B[56] == 'r') { P.Castle |= 8; }
					else if (C != 'K' && C != 'Q' && C != 'k' && C != 'q') { return false; }
				}
			}
			P.Ep = -1;
			if (Parts.size() > 3 && Parts[3] != "-")
			{
				const std::string& E = Parts[3];
				if (E.size() != 2 || E[0] < 'a' || E[0] > 'h' || (E[1] != '3' && E[1] != '6'))
				{
					return false;
				}
				const int Sq = (E[1] - '1') * 8 + (E[0] - 'a');
				// the pawn that has just passed over it must be there
				const bool bWhitePassed = E[1] == '3';
				if (bWhitePassed == P.bWhite || P.B[Sq] != '.' || P.B[Sq + (bWhitePassed ? 8 : -8)] != (bWhitePassed ? 'P' : 'p'))
				{
					return false;
				}
				P.Ep = Sq;
			}
			P.Half = 0;
			P.Full = 1;
			if (Parts.size() > 4 && !ParseNumber(Parts[4], P.Half))
			{
				return false;
			}
			if (Parts.size() > 5 && (!ParseNumber(Parts[5], P.Full) || P.Full < 1))
			{
				return false;
			}
			// the side that has just moved may not have left its king in check
			if (KingAttacked(P, !P.bWhite))
			{
				return false;
			}
			Out = P;
			return true;
		}

		unsigned Scramble(std::uint64_t V)
		{
			V ^= V >> 33;
			V *= 0xff51afd7ed558ccdull;
			V ^= V >> 33;
			V *= 0xc4ceb9fe1a85ec53ull;
			V ^= V >> 33;
			return (unsigned)(V & 0x7fffffffu);
		}
	}

	FChessGame::FChessGame()
	{
		Reset();
	}

	void FChessGame::Reset()
	{
		SetFen(Detail::StartPosition);
	}

	bool FChessGame::SetFen(const std::string& Fen)
	{
		Detail::FPos P;
		if (!Detail::ParseFen(Fen, P))
		{
			return false;
		}
		Pos = P;
		StartFen = Detail::FenOf(Pos);
		Hist.clear();
		Keys.clear();
		Keys.push_back(Detail::KeyOf(Pos));
		return true;
	}

	std::string FChessGame::Fen() const
	{
		return Detail::FenOf(Pos);
	}

	std::vector<FChessMove> FChessGame::LegalMoves() const
	{
		Detail::FMoveList L;
		Detail::GenLegal(Pos, L);
		return std::vector<FChessMove>(L.M, L.M + L.N);
	}

	std::vector<FChessMove> FChessGame::LegalMovesFrom(int Sq) const
	{
		Detail::FMoveList L;
		Detail::GenLegal(Pos, L);
		std::vector<FChessMove> Out;
		for (int I = 0; I < L.N; ++I)
		{
			if (L.M[I].From == Sq)
			{
				Out.push_back(L.M[I]);
			}
		}
		return Out;
	}

	bool FChessGame::Play(const FChessMove& M)
	{
		if (!M.Valid())
		{
			return false;
		}
		FChessMove Want = M;
		if (Want.Promo >= 'A' && Want.Promo <= 'Z')
		{
			Want.Promo = (char)(Want.Promo + 32);
		}
		const int ToRank = Detail::RankOf(Want.To);
		if (Detail::LowerPiece(Pos.B[Want.From]) == 'p' && (ToRank == 0 || ToRank == 7))
		{
			if (Want.Promo == 0)
			{
				Want.Promo = 'q';
			}
		}
		else if (Want.Promo != 0)
		{
			return false;
		}
		Detail::FMoveList L;
		Detail::GenLegal(Pos, L);
		for (int I = 0; I < L.N; ++I)
		{
			if (L.M[I] == Want)
			{
				FPly Ply;
				Ply.Move = Want;
				Detail::Make(Pos, Want, Ply.Undo);
				Hist.push_back(Ply);
				Keys.push_back(Detail::KeyOf(Pos));
				return true;
			}
		}
		return false;
	}

	bool FChessGame::Undo()
	{
		if (Hist.empty())
		{
			return false;
		}
		Detail::Unmake(Pos, Hist.back().Move, Hist.back().Undo);
		Hist.pop_back();
		Keys.pop_back();
		return true;
	}

	bool FChessGame::InCheck() const
	{
		return Detail::KingAttacked(Pos, Pos.bWhite);
	}

	EChessResult FChessGame::Result() const
	{
		if (!Detail::HasLegalMove(Pos))
		{
			if (!InCheck())
			{
				return EChessResult::Stalemate;
			}
			return Pos.bWhite ? EChessResult::BlackMates : EChessResult::WhiteMates;
		}
		if (Detail::InsufficientMaterial(Pos))
		{
			return EChessResult::DrawMaterial;
		}
		if (Pos.Half >= 100)
		{
			return EChessResult::DrawFifty;
		}
		int Same = 0;
		for (std::uint64_t K : Keys)
		{
			if (K == Keys.back())
			{
				++Same;
			}
		}
		return Same >= 3 ? EChessResult::DrawRepetition : EChessResult::Playing;
	}

	std::string FChessGame::Notation(const FChessMove& M) const
	{
		if (!M.Valid())
		{
			return std::string();
		}
		FChessMove Want = M;
		const char Piece = Pos.B[Want.From];
		const char Kind = Detail::LowerPiece(Piece);
		const int ToRank = Detail::RankOf(Want.To);
		if (Kind == 'p' && (ToRank == 0 || ToRank == 7) && Want.Promo == 0)
		{
			Want.Promo = 'q';
		}
		Detail::FMoveList L;
		Detail::GenLegal(Pos, L);
		bool bLegal = false;
		bool bOther = false, bSameFile = false, bSameRank = false;
		for (int I = 0; I < L.N; ++I)
		{
			const FChessMove& O = L.M[I];
			if (O == Want)
			{
				bLegal = true;
			}
			else if (O.To == Want.To && O.From != Want.From && Pos.B[O.From] == Piece)
			{
				bOther = true;
				bSameFile = bSameFile || Detail::FileOf(O.From) == Detail::FileOf(Want.From);
				bSameRank = bSameRank || Detail::RankOf(O.From) == Detail::RankOf(Want.From);
			}
		}
		if (!bLegal)
		{
			return std::string();
		}
		std::string S;
		const bool bCapture = Pos.B[Want.To] != '.' || (Kind == 'p' && Detail::FileOf(Want.From) != Detail::FileOf(Want.To));
		if (Kind == 'k' && Want.To - Want.From == 2)
		{
			S = "O-O";
		}
		else if (Kind == 'k' && Want.From - Want.To == 2)
		{
			S = "O-O-O";
		}
		else
		{
			if (Kind == 'p')
			{
				if (bCapture)
				{
					S += (char)('a' + Detail::FileOf(Want.From));
				}
			}
			else
			{
				S += (char)(Kind - 32);
				if (bOther)
				{
					if (!bSameFile)
					{
						S += (char)('a' + Detail::FileOf(Want.From));
					}
					else if (!bSameRank)
					{
						S += (char)('1' + Detail::RankOf(Want.From));
					}
					else
					{
						S += Detail::SquareName(Want.From);
					}
				}
			}
			if (bCapture)
			{
				S += 'x';
			}
			S += Detail::SquareName(Want.To);
			if (Want.Promo)
			{
				S += '=';
				S += (char)(Want.Promo - 32);
			}
		}
		Detail::FPos After = Pos;
		Detail::FUndo U;
		Detail::Make(After, Want, U);
		if (Detail::KingAttacked(After, After.bWhite))
		{
			S += Detail::HasLegalMove(After) ? '+' : '#';
		}
		return S;
	}

	FChessMove FChessGame::BestMove(int Level, unsigned Seed) const
	{
		if (Result() != EChessResult::Playing)
		{
			return FChessMove();
		}
		Detail::FMoveList Root;
		Detail::GenLegal(Pos, Root);
		if (Root.N == 0)
		{
			return FChessMove();
		}
		Detail::SortMoves(Pos, Root);

		Detail::FSearch S;
		S.P = Pos;
		S.Budget = Level <= 1 ? 3000 : Level == 2 ? 25000 : 80000;
		const int MaxDepth = Level <= 1 ? 2 : Level == 2 ? 4 : 8;

		// a move that repeats a position seen twice already is a draw
		auto Repeats = [this](const Detail::FPos& After)
		{
			const std::uint64_t K = Detail::KeyOf(After);
			int Same = 0;
			for (std::uint64_t Seen : Keys)
			{
				if (Seen == K)
				{
					++Same;
				}
			}
			return Same >= 2;
		};

		if (Level <= 1)
		{
			// the beginner: every move weighed two plies deep, then now and then not the best of them
			int Scores[256];
			for (int I = 0; I < Root.N; ++I)
			{
				Detail::FUndo U;
				Detail::Make(S.P, Root.M[I], U);
				Scores[I] = Repeats(S.P) ? 0 : -Detail::Negamax(S, MaxDepth - 1, -Detail::InfScore, Detail::InfScore, 1);
				Detail::Unmake(S.P, Root.M[I], U);
			}
			int Order[256];
			for (int I = 0; I < Root.N; ++I)
			{
				Order[I] = I;
			}
			for (int I = 1; I < Root.N; ++I)
			{
				const int Idx = Order[I];
				int J = I - 1;
				while (J >= 0 && Scores[Order[J]] < Scores[Idx])
				{
					Order[J + 1] = Order[J];
					--J;
				}
				Order[J + 1] = Idx;
			}
			const unsigned Roll = Detail::Scramble(Keys.back() ^ ((std::uint64_t)Seed * 0x9E3779B97F4A7C15ull) ^ (std::uint64_t)Hist.size()) % 100u;
			int Pick = Roll < 55u ? 0 : Roll < 85u ? 1 : 2;
			const int BestScore = Scores[Order[0]];
			while (Pick > 0 && (Pick >= Root.N || Scores[Order[Pick]] < BestScore - 200 || BestScore > Detail::MateScore - 100))
			{
				--Pick;
			}
			return Root.M[Order[Pick]];
		}

		FChessMove Best = Root.M[0];
		for (int Depth = 1; Depth <= MaxDepth; ++Depth)
		{
			int Alpha = -Detail::InfScore;
			FChessMove IterBest;
			for (int I = 0; I < Root.N; ++I)
			{
				Detail::FUndo U;
				Detail::Make(S.P, Root.M[I], U);
				const int Score = Repeats(S.P) ? 0 : -Detail::Negamax(S, Depth - 1, -Detail::InfScore, -Alpha, 1);
				Detail::Unmake(S.P, Root.M[I], U);
				if (S.bAbort)
				{
					break;
				}
				if (Score > Alpha || !IterBest.Valid())
				{
					Alpha = Score;
					IterBest = Root.M[I];
				}
			}
			// a cut-short pass still counts: its first move was the pass before's best, searched deeper
			if (IterBest.Valid())
			{
				Best = IterBest;
				for (int I = 0; I < Root.N; ++I)
				{
					if (Root.M[I] == Best)
					{
						for (int J = I; J > 0; --J)
						{
							Root.M[J] = Root.M[J - 1];
						}
						Root.M[0] = Best;
						break;
					}
				}
			}
			if (S.bAbort || Alpha > Detail::MateScore - 100)
			{
				break;
			}
		}
		return Best;
	}

	std::string FChessGame::Save() const
	{
		std::string S = StartFen;
		S += ';';
		for (size_t I = 0; I < Hist.size(); ++I)
		{
			if (I > 0)
			{
				S += ',';
			}
			S += Detail::SquareName(Hist[I].Move.From);
			S += Detail::SquareName(Hist[I].Move.To);
			if (Hist[I].Move.Promo)
			{
				S += Hist[I].Move.Promo;
			}
		}
		return S;
	}

	bool FChessGame::Restore(const std::string& S)
	{
		const size_t Semi = S.find(';');
		if (Semi == std::string::npos)
		{
			return false;
		}
		FChessGame G;
		if (!G.SetFen(S.substr(0, Semi)))
		{
			return false;
		}
		size_t I = Semi + 1;
		while (I < S.size())
		{
			size_t End = S.find(',', I);
			if (End == std::string::npos)
			{
				End = S.size();
			}
			const std::string Tok = S.substr(I, End - I);
			if (Tok.size() < 4 || Tok.size() > 5)
			{
				return false;
			}
			for (int K = 0; K < 4; K += 2)
			{
				if (Tok[K] < 'a' || Tok[K] > 'h' || Tok[K + 1] < '1' || Tok[K + 1] > '8')
				{
					return false;
				}
			}
			FChessMove M;
			M.From = (Tok[1] - '1') * 8 + (Tok[0] - 'a');
			M.To = (Tok[3] - '1') * 8 + (Tok[2] - 'a');
			M.Promo = Tok.size() == 5 ? Tok[4] : 0;
			if (!G.Play(M))
			{
				return false;
			}
			I = End + 1;
		}
		*this = G;
		return true;
	}
}
}
