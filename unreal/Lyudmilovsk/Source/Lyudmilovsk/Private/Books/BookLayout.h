#pragma once

// Pages of a library book (docs/books.md "Layout"): the text of Content/Books/<lang>/<id>.txt laid out into lines and pages
// for one font size and page size. Engine-free (std only) so tools/booksim checks it with g++; the reader (SLyudBookReader)
// gives it the glyph widths, measured once per font on the game thread, and runs it on a worker.
//
// The text format (tools/books/fetch_books.py writes it): UTF-8 (here already UTF-16), paragraphs separated by a blank line,
// a line break inside a paragraph is kept (verse, a letter's address), a paragraph starting "# " is a heading.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace LyudBooks
{
	enum class ELineKind : uint8_t
	{
		Body,
		/** The first line of a prose paragraph: indented. */
		First,
		Heading,
		/** A kept line of verse: never justified. */
		Verse,
	};

	/** One line on a page: [Start, Start + Len) of the text, how it is drawn. A blank line is not stored: the line's Y says where it goes. */
	struct FLine
	{
		int32_t Start = 0;
		int32_t Len = 0;
		ELineKind Kind = ELineKind::Body;
		/** Row on its page (0 = top). */
		int16_t Row = 0;
		/** The last line of its paragraph (not justified). */
		bool bLast = false;
	};

	struct FPage
	{
		/** Index of its first line in FLayout::Lines (pages without lines: an empty book). */
		int32_t FirstLine = 0;
		int32_t NumLines = 0;
		/** Where the page's text starts and ends (a bookmark is a character offset: it survives another font size). */
		int32_t Start = 0;
		int32_t End = 0;
	};

	struct FHeading
	{
		int32_t Page = 0;
		int32_t Start = 0;
		int32_t Len = 0;
	};

	struct FParams
	{
		/** The text block's width, px (slate units). */
		float Width = 600.f;
		/** Rows on a page. */
		int32_t Rows = 30;
		/** Indent of a paragraph's first line, px. */
		float Indent = 24.f;
		/** Body glyph width (px) of one UTF-16 unit; a surrogate pair is asked once with its high unit. */
		std::function<float(char16_t)> Width16;
		/** Heading glyph width (bold); empty: the body's times 1.08. */
		std::function<float(char16_t)> HeadWidth16;
	};

	struct FLayout
	{
		std::vector<FLine> Lines;
		std::vector<FPage> Pages;
		std::vector<FHeading> Headings;

		int32_t PageOf(int32_t Offset) const
		{
			if (Pages.empty())
			{
				return 0;
			}
			int32_t Lo = 0, Hi = (int32_t)Pages.size() - 1;
			while (Lo < Hi)
			{
				const int32_t Mid = (Lo + Hi + 1) / 2;
				if (Pages[(size_t)Mid].Start <= Offset)
				{
					Lo = Mid;
				}
				else
				{
					Hi = Mid - 1;
				}
			}
			return Lo;
		}
	};

	inline bool IsCJK(char16_t C)
	{
		return (C >= 0x3000 && C <= 0x9FFF) || (C >= 0xF900 && C <= 0xFAFF) || (C >= 0xFF00 && C <= 0xFFEF) || (C >= 0xAC00 && C <= 0xD7AF);
	}

	/** Japanese line rules (kinsoku): these never start a line... */
	inline bool NoLineStart(char16_t C)
	{
		static const char16_t* const Set = u"、。，．・：；？！ー」』）】〉》〕｝］ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ…‥゛゜ゝゞヽヾ々,.;:!?)]}»”’";
		for (const char16_t* P = Set; *P; ++P)
		{
			if (*P == C)
			{
				return true;
			}
		}
		return false;
	}

	/** ...and these never end one. */
	inline bool NoLineEnd(char16_t C)
	{
		static const char16_t* const Set = u"「『（【〈《〔｛［([{«“‘";
		for (const char16_t* P = Set; *P; ++P)
		{
			if (*P == C)
			{
				return true;
			}
		}
		return false;
	}

	inline bool IsSpace(char16_t C) { return C == u' ' || C == u'\t' || C == 0x00A0 || C == 0x3000; }

	/**
	 * Lays Text out. Headings start a new page (unless the page is still empty or holds only headings) with a blank row after
	 * them; paragraphs are separated by nothing (prose: the first line is indented) and verse stanzas by a blank row. A
	 * paragraph never leaves a single line alone at the foot of a page when it goes on (a widow line moves over).
	 */
	inline FLayout Layout(const std::u16string& Text, const FParams& P)
	{
		FLayout L;
		const int32_t N = (int32_t)Text.size();
		const int32_t Rows = P.Rows < 4 ? 4 : P.Rows;
		auto W = [&P](char16_t C) -> float { return P.Width16 ? P.Width16(C) : 8.f; };
		auto HW = [&P, &W](char16_t C) -> float { return P.HeadWidth16 ? P.HeadWidth16(C) : W(C) * 1.08f; };

		int32_t Row = 0;
		bool bPageHasBody = false;
		auto NewPage = [&](int32_t StartAt)
		{
			if (!L.Pages.empty())
			{
				FPage& Last = L.Pages.back();
				Last.NumLines = (int32_t)L.Lines.size() - Last.FirstLine;
			}
			FPage Pg;
			Pg.FirstLine = (int32_t)L.Lines.size();
			Pg.Start = StartAt;
			Pg.End = StartAt;
			L.Pages.push_back(Pg);
			Row = 0;
			bPageHasBody = false;
		};
		NewPage(0);

		// breaks [S, E) into lines no wider than Avail (the first one FirstAvail), calling Emit(start, len, first, last)
		auto Wrap = [&](int32_t S, int32_t E, float FirstAvail, float Avail, bool bHead, const std::function<void(int32_t, int32_t, bool, bool)>& Emit)
		{
			int32_t LineStart = S;
			// skip leading spaces of the paragraph
			while (LineStart < E && IsSpace(Text[(size_t)LineStart]) && Text[(size_t)LineStart] != 0x3000)
			{
				++LineStart;
			}
			bool bFirst = true;
			while (LineStart < E)
			{
				const float Max = bFirst ? FirstAvail : Avail;
				float X = 0.f;
				int32_t I = LineStart;
				int32_t LastBreak = -1;   // a break after index LastBreak-1 (the line is [LineStart, LastBreak))
				int32_t LastBreakNext = -1;
				while (I < E)
				{
					const char16_t C = Text[(size_t)I];
					const bool bPair = C >= 0xD800 && C <= 0xDBFF && I + 1 < E;
					const float Cw = bHead ? HW(C) : W(C);
					if (X + Cw > Max && I > LineStart)
					{
						break;
					}
					X += Cw;
					const int32_t Next = I + (bPair ? 2 : 1);
					if (IsSpace(C))
					{
						LastBreak = I;
						LastBreakNext = Next;
					}
					else if (Next < E)
					{
						const char16_t D = Text[(size_t)Next];
						if ((IsCJK(C) || IsCJK(D)) && !NoLineStart(D) && !NoLineEnd(C) && !IsSpace(D))
						{
							LastBreak = Next;
							LastBreakNext = Next;
						}
						else if (C == u'-' || C == 0x2014 || C == 0x2013)
						{
							LastBreak = Next;
							LastBreakNext = Next;
						}
					}
					I = Next;
				}
				int32_t LineEnd;
				int32_t NextStart;
				if (I >= E)
				{
					LineEnd = E;
					NextStart = E;
				}
				else if (LastBreak > LineStart)
				{
					LineEnd = LastBreak;
					NextStart = LastBreakNext;
				}
				else
				{
					// one word wider than the line: cut it where it overflows
					LineEnd = I > LineStart ? I : LineStart + 1;
					NextStart = LineEnd;
				}
				// trailing spaces stay off the line
				int32_t TrimEnd = LineEnd;
				while (TrimEnd > LineStart && IsSpace(Text[(size_t)(TrimEnd - 1)]) && Text[(size_t)(TrimEnd - 1)] != 0x3000)
				{
					--TrimEnd;
				}
				while (NextStart < E && IsSpace(Text[(size_t)NextStart]) && Text[(size_t)NextStart] != 0x3000)
				{
					++NextStart;
				}
				Emit(LineStart, TrimEnd - LineStart, bFirst, NextStart >= E);
				bFirst = false;
				LineStart = NextStart;
			}
		};

		auto Put = [&](int32_t Start, int32_t Len, ELineKind Kind, bool bLast)
		{
			if (Row >= Rows)
			{
				NewPage(Start);
			}
			FLine Ln;
			Ln.Start = Start;
			Ln.Len = Len;
			Ln.Kind = Kind;
			Ln.Row = (int16_t)Row;
			Ln.bLast = bLast;
			L.Lines.push_back(Ln);
			L.Pages.back().End = Start + Len;
			++Row;
			if (Kind != ELineKind::Heading)
			{
				bPageHasBody = true;
			}
		};

		int32_t Pos = 0;
		bool bPrevVerse = false;
		std::vector<std::pair<int32_t, int32_t>> Rows16;
		while (Pos < N)
		{
			// the next paragraph: up to a blank line
			while (Pos < N && (Text[(size_t)Pos] == u'\n' || Text[(size_t)Pos] == u'\r'))
			{
				++Pos;
			}
			if (Pos >= N)
			{
				break;
			}
			int32_t End = Pos;
			bool bHasBreak = false;
			while (End < N)
			{
				if (Text[(size_t)End] == u'\n')
				{
					int32_t K = End + 1;
					while (K < N && Text[(size_t)K] == u'\r')
					{
						++K;
					}
					if (K >= N || Text[(size_t)K] == u'\n')
					{
						break;
					}
					bHasBreak = true;
				}
				++End;
			}
			const bool bHead = End - Pos > 2 && Text[(size_t)Pos] == u'#' && Text[(size_t)Pos + 1] == u' ';
			if (bHead)
			{
				const int32_t S = Pos + 2;
				// a heading opens a page, unless the page is still empty of body text, or it would be the page's last rows
				if (bPageHasBody)
				{
					NewPage(Pos);
				}
				else if (Row > 0 && Row > Rows - 4)
				{
					NewPage(Pos);
				}
				else if (Row > 0)
				{
					++Row;  // a blank row between two headings
				}
				FHeading H;
				H.Page = (int32_t)L.Pages.size() - 1;
				H.Start = S;
				H.Len = End - S;
				L.Headings.push_back(H);
				const float Avail = P.Width;
				Wrap(S, End, Avail, Avail, true, [&](int32_t St, int32_t Ln, bool, bool bL) { Put(St, Ln, ELineKind::Heading, bL); });
				++Row;  // a blank row under it
				if (Row >= Rows)
				{
					NewPage(End);
				}
				bPrevVerse = false;
				Pos = End;
				continue;
			}
			if (bHasBreak)
			{
				// verse: every kept line wraps on its own (hanging indent for the rest), a blank row round the stanza
				if (Row > 0 && !bPrevVerse)
				{
					++Row;
				}
				int32_t LS = Pos;
				while (LS < End)
				{
					int32_t LE = LS;
					while (LE < End && Text[(size_t)LE] != u'\n')
					{
						++LE;
					}
					int32_t LineEndTrim = LE;
					if (LineEndTrim > LS && Text[(size_t)(LineEndTrim - 1)] == u'\r')
					{
						--LineEndTrim;
					}
					if (LineEndTrim > LS)
					{
						Wrap(LS, LineEndTrim, P.Width, P.Width - P.Indent, false,
							[&](int32_t St, int32_t Ln, bool, bool bL) { Put(St, Ln, ELineKind::Verse, bL); });
					}
					LS = LE + 1;
				}
				++Row;  // the blank row after the stanza
				bPrevVerse = true;
				Pos = End;
				continue;
			}
			// prose: lay the paragraph out first, then keep it from leaving one line alone at the foot of the page
			Rows16.clear();
			bool bFirstLine = true;
			std::vector<FLine> Para;
			Wrap(Pos, End, P.Width - P.Indent, P.Width, false, [&](int32_t St, int32_t Ln, bool bF, bool bL)
			{
				FLine Li;
				Li.Start = St;
				Li.Len = Ln;
				Li.Kind = bF ? ELineKind::First : ELineKind::Body;
				Li.bLast = bL;
				Para.push_back(Li);
				bFirstLine = false;
			});
			if (bPrevVerse && Row >= Rows)
			{
				NewPage(Pos);
			}
			const int32_t Left = Rows - Row;
			if (Para.size() >= 2 && Left == 1 && bPageHasBody)
			{
				NewPage(Pos);
			}
			for (size_t K = 0; K < Para.size(); ++K)
			{
				// the paragraph's last line alone at the top of the next page: take one more line over with it
				if (Para.size() >= 3 && K + 2 == Para.size() && Row == Rows - 1)
				{
					NewPage(Para[K].Start);
				}
				Put(Para[K].Start, Para[K].Len, Para[K].Kind, Para[K].bLast);
			}
			bPrevVerse = false;
			Pos = End;
		}
		if (!L.Pages.empty())
		{
			FPage& Last = L.Pages.back();
			Last.NumLines = (int32_t)L.Lines.size() - Last.FirstLine;
			Last.End = N;
			if (Last.NumLines == 0 && L.Pages.size() > 1)
			{
				L.Pages.pop_back();
				L.Pages.back().End = N;
			}
		}
		for (size_t K = 0; K + 1 < L.Pages.size(); ++K)
		{
			L.Pages[K].End = L.Pages[K + 1].Start;
		}
		return L;
	}

	/** UTF-8 to UTF-16 (the book files; invalid bytes become U+FFFD). */
	inline std::u16string FromUtf8(const char* S, size_t Len)
	{
		std::u16string Out;
		Out.reserve(Len);
		size_t I = 0;
		while (I < Len)
		{
			const uint8_t C = (uint8_t)S[I];
			uint32_t Cp = 0xFFFD;
			size_t Need = 0;
			if (C < 0x80) { Cp = C; Need = 0; }
			else if ((C & 0xE0) == 0xC0) { Cp = C & 0x1F; Need = 1; }
			else if ((C & 0xF0) == 0xE0) { Cp = C & 0x0F; Need = 2; }
			else if ((C & 0xF8) == 0xF0) { Cp = C & 0x07; Need = 3; }
			else { ++I; Out.push_back(0xFFFD); continue; }
			if (Need > 0 && I + Need >= Len)
			{
				Out.push_back(0xFFFD);
				break;
			}
			bool bOk = true;
			for (size_t K = 1; K <= Need; ++K)
			{
				const uint8_t D = (uint8_t)S[I + K];
				if ((D & 0xC0) != 0x80)
				{
					bOk = false;
					break;
				}
				Cp = (Cp << 6) | (D & 0x3F);
			}
			if (!bOk)
			{
				Out.push_back(0xFFFD);
				++I;
				continue;
			}
			I += Need + 1;
			if (Cp >= 0x10000)
			{
				Cp -= 0x10000;
				Out.push_back((char16_t)(0xD800 + (Cp >> 10)));
				Out.push_back((char16_t)(0xDC00 + (Cp & 0x3FF)));
			}
			else
			{
				Out.push_back((char16_t)Cp);
			}
		}
		if (!Out.empty() && Out[0] == 0xFEFF)
		{
			Out.erase(0, 1);
		}
		return Out;
	}
}
