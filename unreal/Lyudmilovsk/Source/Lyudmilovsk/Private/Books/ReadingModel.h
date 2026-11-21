#pragma once

// Reading the library's classics anywhere, and the library's book club (docs/books.md "Reading anywhere", "The book club";
// interactives backlog items 47 and 48). The rules only, in plain C++ so tools/booksim/readingtest.cpp checks them with g++:
//
//  - Light (Judge): may the hero read here and now, and how bright the page is. Under a roof always; outdoors by day; in the
//    dusk the page dims; at night only with the headlamp on or a lit street lamp near; never outdoors in the rain.
//  - Sentences (SentencesIn) of the spread in view, for the reader's quote mode, and the quote's clean text (QuoteText).
//  - The quote store (FQuotes): what he copied into the diary's «Цитаты», never the same sentence twice.
//  - Reading stats (StdPages): pages of a "standard" page, so a book is as long in any type size.
//  - Overdue loans (IsOverdue, a week) and the book club (FClub): its evening, the month's book, the lines in turns, the
//    librarian's advice from what he has read (Recommend), a friend's opinion of the book he recommended.
//
// Days are whole game days since the game began (UGameCalendar's start, 2026-10-20 = day 0); weekdays 0 Monday .. 6 Sunday.

#include "People/MiniJson.h"

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace LyudReading
{
	// ------------------------------------------------------------------------------------------------------------ light

	struct FLightIn
	{
		/** 0 under a roof (the club, a cabin, the train, a café) .. 1 open sky (ULyudRainFogSubsystem::GetOpenSky). */
		float OpenSky = 1.f;
		/** The sun's altitude, degrees. */
		float SunAltDeg = 30.f;
		/** Rain falling, mm/h, and how much of what falls is snow (0..1). */
		float RainMmH = 0.f;
		float Snow = 0.f;
		bool bHeadlamp = false;
		/** The nearest lit street lamp, metres (< 0: none lit, or none near). */
		float LampM = -1.f;
	};

	enum class EVerdict : uint8_t
	{
		Ok,
		/** He can read, the page is dim (the dusk, a lamp a little far). */
		Dim,
		/** Too dark to read. */
		Dark,
		/** Rain on the page: he won't read outdoors. */
		Rain,
	};

	struct FLight
	{
		EVerdict Verdict = EVerdict::Ok;
		/** How bright the page is drawn, 0.35..1. */
		float Bright = 1.f;
		bool CanRead() const { return Verdict == EVerdict::Ok || Verdict == EVerdict::Dim; }
	};

	/** Below this open sky it is "under a roof". */
	constexpr float RoofSky = 0.3f;
	/** Rain that wets a page (mm/h of liquid water): a drizzle of 0.2 does not stop him yet. */
	constexpr float WetRain = 0.3f;
	/** The sun this high: full daylight; this low: night (civil twilight's end). */
	constexpr float DayAlt = 3.f;
	constexpr float NightAlt = -6.f;
	/** A lit street lamp nearer than this lights the page well; up to LampFarM dimly. */
	constexpr float LampNearM = 6.f;
	constexpr float LampFarM = 11.f;

	inline FLight Judge(const FLightIn& In)
	{
		FLight L;
		if (In.OpenSky < RoofSky)
		{
			return L;   // under a roof: the room's own light (the club's, the cabin's, the café's)
		}
		if (In.RainMmH * (1.f - std::min(1.f, std::max(0.f, In.Snow))) >= WetRain)
		{
			L.Verdict = EVerdict::Rain;
			L.Bright = 0.f;
			return L;
		}
		// the sky's light: 1 by day, falling through the dusk to 0 at the night line
		const float Sky = In.SunAltDeg >= DayAlt ? 1.f : (In.SunAltDeg <= NightAlt ? 0.f : (In.SunAltDeg - NightAlt) / (DayAlt - NightAlt));
		float Lamp = 0.f;
		if (In.bHeadlamp)
		{
			Lamp = 0.9f;
		}
		if (In.LampM >= 0.f && In.LampM < LampFarM)
		{
			const float Street = In.LampM <= LampNearM ? 0.85f : 0.85f - 0.35f * (In.LampM - LampNearM) / (LampFarM - LampNearM);
			Lamp = std::max(Lamp, Street);
		}
		const float Light = std::max(Sky, Lamp);
		if (Light >= 0.8f)
		{
			L.Bright = Light >= 1.f ? 1.f : 0.85f + 0.15f * (Light - 0.8f) / 0.2f;
			return L;
		}
		if (Light >= 0.4f)
		{
			L.Verdict = EVerdict::Dim;
			L.Bright = 0.45f + 0.4f * (Light - 0.4f) / 0.4f;
			return L;
		}
		L.Verdict = EVerdict::Dark;
		L.Bright = 0.f;
		return L;
	}

	// -------------------------------------------------------------------------------------------------------- sentences

	struct FSpan
	{
		int32_t Start = 0;
		int32_t Len = 0;
	};

	inline bool IsSpace16(char16_t C) { return C == u' ' || C == u'\t' || C == u'\n' || C == u'\r' || C == 0x3000 || C == 0x00A0; }
	inline bool IsStop(char16_t C) { return C == u'.' || C == u'!' || C == u'?' || C == 0x2026 || C == 0x3002 || C == 0xFF01 || C == 0xFF1F; }
	inline bool IsCloser(char16_t C)
	{
		return C == u'"' || C == u'\'' || C == u')' || C == u']' || C == 0x00BB || C == 0x201D || C == 0x2019 || C == 0x300D || C == 0x300F
			|| C == 0xFF09;
	}
	inline bool IsCjkStop(char16_t C) { return C == 0x3002 || C == 0xFF01 || C == 0xFF1F; }
	inline bool IsDash(char16_t C) { return C == 0x2014 || C == 0x2013; }
	inline bool IsLower(char16_t C) { return (C >= u'a' && C <= u'z') || (C >= 0x0430 && C <= 0x044F) || C == 0x0451; }

	/** The longest sentence the quote mode offers whole (a longer one is cut at the next comma or space past this). */
	constexpr int32_t MaxSentence = 420;

	/**
	 * The sentences that start in [From, To) of Text (the spread in view): a sentence ends after . ! ? … 。！？ (and any
	 * closing quotes or brackets) followed by a space, a line break or the end; a paragraph break (a blank line) and a heading
	 * line ("# ") end one too. One that runs past To is kept whole (up to MaxSentence). Headings are not offered.
	 */
	inline std::vector<FSpan> SentencesIn(const std::u16string& Text, int32_t From, int32_t To)
	{
		std::vector<FSpan> Out;
		const int32_t N = (int32_t)Text.size();
		From = std::max<int32_t>(0, std::min(From, N));
		To = std::max<int32_t>(From, std::min(To, N));
		int32_t I = From;
		while (I < To)
		{
			while (I < To && IsSpace16(Text[(size_t)I]))
			{
				++I;
			}
			if (I >= To)
			{
				break;
			}
			// a heading line: skipped
			const bool bLineStart = I == 0 || Text[(size_t)I - 1] == u'\n';
			if (bLineStart && Text[(size_t)I] == u'#' && I + 1 < N && Text[(size_t)I + 1] == u' ')
			{
				while (I < N && Text[(size_t)I] != u'\n')
				{
					++I;
				}
				continue;
			}
			const int32_t S = I;
			int32_t E = -1;
			while (I < N)
			{
				const char16_t C = Text[(size_t)I];
				if (C == u'\n' && I + 1 < N && (Text[(size_t)I + 1] == u'\n' || (Text[(size_t)I + 1] == u'\r')))
				{
					E = I;  // a blank line: the paragraph's end
					break;
				}
				if (IsStop(C))
				{
					int32_t K = I + 1;
					while (K < N && (IsStop(Text[(size_t)K]) || IsCloser(Text[(size_t)K])))
					{
						++K;
					}
					// «Куда вы?» — спросил он. / "Go," he said... it goes on: a dash or a small letter after the stop
					int32_t J = K;
					while (J < N && (Text[(size_t)J] == u' ' || Text[(size_t)J] == 0x00A0))
					{
						++J;
					}
					if (J > K && J < N && (IsDash(Text[(size_t)J]) || IsLower(Text[(size_t)J])))
					{
						I = J;
						continue;
					}
					if (K >= N || IsSpace16(Text[(size_t)K]) || IsCjkStop(C))
					{
						E = K;
						I = K;
						break;
					}
					I = K;
					continue;
				}
				if (I - S >= MaxSentence && (C == u',' || C == u' ' || C == 0x3001))
				{
					E = I + 1;
					I = E;
					break;
				}
				++I;
			}
			if (E < 0)
			{
				E = N;
				I = N;
			}
			int32_t TrimE = E;
			while (TrimE > S && IsSpace16(Text[(size_t)TrimE - 1]))
			{
				--TrimE;
			}
			int32_t Letters = 0;
			for (int32_t K = S; K < TrimE && Letters < 2; ++K)
			{
				Letters += IsSpace16(Text[(size_t)K]) || IsStop(Text[(size_t)K]) ? 0 : 1;
			}
			if (Letters >= 2)
			{
				Out.push_back({ S, TrimE - S });
			}
			if (I < E)
			{
				I = E;
			}
		}
		return Out;
	}

	/** The text kept in the diary: line breaks become spaces (none between Japanese characters), runs of spaces one, and a
	 *  very long sentence is cut with an ellipsis at MaxQuote characters. */
	constexpr int32_t MaxQuote = 360;

	inline std::u16string QuoteText(const std::u16string& Text, FSpan Span)
	{
		std::u16string Out;
		const int32_t E = std::min<int32_t>((int32_t)Text.size(), Span.Start + Span.Len);
		bool bSpace = false;
		for (int32_t K = std::max<int32_t>(0, Span.Start); K < E; ++K)
		{
			char16_t C = Text[(size_t)K];
			if (C == u'\r')
			{
				continue;
			}
			if (IsSpace16(C) && C != 0x3000)
			{
				bSpace = !Out.empty();
				continue;
			}
			if (bSpace)
			{
				const char16_t P = Out.back();
				const bool bCjk = (P >= 0x3000 && P <= 0x9FFF) || (C >= 0x3000 && C <= 0x9FFF) || (P >= 0xFF00 && P <= 0xFFEF);
				if (!bCjk)
				{
					Out.push_back(u' ');
				}
				bSpace = false;
			}
			Out.push_back(C);
			if ((int32_t)Out.size() >= MaxQuote)
			{
				Out.push_back(0x2026);
				break;
			}
		}
		return Out;
	}

	// ----------------------------------------------------------------------------------------------------------- quotes

	struct FQuote
	{
		std::string Book;     // "<lang>/<work>"
		int32_t Offset = 0;   // where the sentence starts in the text
		std::u16string Text;
		int32_t Day = 0;
	};

	/** The diary's quotes, oldest first; at most MaxQuotes (the oldest go). */
	struct FQuotes
	{
		static constexpr size_t MaxQuotes = 200;
		std::vector<FQuote> List;

		bool Has(const std::string& Book, int32_t Offset) const
		{
			for (const FQuote& Q : List)
			{
				if (Q.Book == Book && Q.Offset == Offset)
				{
					return true;
				}
			}
			return false;
		}
		/** False when this very sentence is in the diary already (or empty). */
		bool Add(const FQuote& Q)
		{
			if (Q.Text.empty() || Has(Q.Book, Q.Offset))
			{
				return false;
			}
			List.push_back(Q);
			if (List.size() > MaxQuotes)
			{
				List.erase(List.begin());
			}
			return true;
		}
	};

	// ------------------------------------------------------------------------------------------------------------ stats

	/** Characters of a standard page (a paperback's): Japanese text is denser. */
	inline int32_t CharsPerPage(bool bJa) { return bJa ? 800 : 1800; }
	inline int32_t StdPages(int64_t Chars, bool bJa)
	{
		const int32_t Per = CharsPerPage(bJa);
		return Chars <= 0 ? 0 : (int32_t)std::max<int64_t>(1, (Chars + Per / 2) / Per);
	}

	/** A loan is overdue a week after it was taken (the librarian's postcard goes once a loan). */
	constexpr int32_t LoanDays = 7;
	inline bool IsOverdue(int32_t BorrowDay, int32_t Today) { return Today - BorrowDay >= LoanDays; }

	inline uint32_t Hash(const std::string& S, uint32_t Seed = 2166136261u)
	{
		uint32_t H = Seed;
		for (const char C : S)
		{
			H = (H ^ (uint8_t)C) * 16777619u;
		}
		return H;
	}

	// --------------------------------------------------------------------------------------------------------- the club

	struct FMember
	{
		std::string Key;    // a townsperson (t_*)
		std::string Role;   // librarian, elder, student, programmer, writer
	};

	struct FClubData
	{
		int Weekday = 3;            // Thursday
		float From = 18.5f;         // the hours they sit
		float To = 20.f;
		std::string Place = "P-LIBRARY-CLUB";
		/** The book of each month, January first (work ids of tools/books/canon.txt). */
		std::vector<std::string> Books;
		std::vector<FMember> Members;
		/** The order the members speak in, by role (the talk lines line.town.bookclub.<work>.<n>, n from 1). */
		std::vector<std::string> Order;
		int HeroLines = 2;           // reading.club.hero.<work>.<n>
		/** "If you liked X": Y (work ids). */
		std::vector<std::pair<std::string, std::string>> Pairs;
		/** Asked when nothing he read leads anywhere: the librarian's first picks, in order. */
		std::vector<std::string> Starters;
		/** A friend's opinion of a book he recommended: this many wordings (reading.friend.opinion.<n>), and the days it takes. */
		int Opinions = 6;
		int ReplyMin = 4, ReplyMax = 8;
	};

	inline bool LoadClub(const std::string& Json, FClubData& Out, std::string& Error)
	{
		LyudJson::FValue Root;
		if (!LyudJson::Parse(Json, Root, Error))
		{
			return false;
		}
		const LyudJson::FValue& C = Root["club"];
		if (C.Type != LyudJson::FValue::Object)
		{
			Error = "reading.json: no club";
			return false;
		}
		Out = FClubData();
		Out.Weekday = (int)C["weekday"].Num;
		Out.From = (float)C["from"].Num;
		Out.To = (float)C["to"].Num;
		Out.Place = C["place"].Str;
		Out.HeroLines = (int)C["hero_lines"].Num;
		for (const LyudJson::FValue& V : C["books"].Items) Out.Books.push_back(V.Str);
		for (const LyudJson::FValue& V : C["order"].Items) Out.Order.push_back(V.Str);
		for (const LyudJson::FValue& V : C["members"].Items) Out.Members.push_back({ V["key"].Str, V["role"].Str });
		for (const LyudJson::FValue& V : Root["pairs"].Items)
		{
			if (V.Items.size() == 2) Out.Pairs.push_back({ V.Items[0].Str, V.Items[1].Str });
		}
		for (const LyudJson::FValue& V : Root["starters"].Items) Out.Starters.push_back(V.Str);
		const LyudJson::FValue& F = Root["friends"];
		if (F.Type == LyudJson::FValue::Object)
		{
			Out.Opinions = (int)F["opinions"].Num;
			Out.ReplyMin = (int)F["reply_min"].Num;
			Out.ReplyMax = (int)F["reply_max"].Num;
		}
		if (Out.Books.size() != 12 || Out.Order.empty() || Out.Members.empty() || Out.To <= Out.From)
		{
			Error = "reading.json: the club needs 12 books, members, an order and hours";
			return false;
		}
		return true;
	}

	struct FClub
	{
		FClubData D;

		/** The month's book (Month 1..12). */
		const std::string& BookOf(int Month) const
		{
			static const std::string None;
			return D.Books.empty() ? None : D.Books[(size_t)((Month - 1 + 1200) % 12) % D.Books.size()];
		}
		/** The circle sits now: its weekday, its hours, not on a festival day or a public holiday. */
		bool MeetsAt(int Weekday, float Hour, bool bFestival, bool bHoliday) const
		{
			return Weekday == D.Weekday && Hour >= D.From && Hour < D.To && !bFestival && !bHoliday;
		}
		/** Today is a club day (the paper's line, the members' plans) whatever the hour. */
		bool MeetsOn(int Weekday, bool bFestival, bool bHoliday) const { return Weekday == D.Weekday && !bFestival && !bHoliday; }
		/** Who speaks the n-th line (0-based, round and round the order) and which line it is (1-based within the book). */
		int NumLines() const { return (int)D.Order.size(); }
		const std::string& SpeakerRole(int Turn) const { return D.Order[(size_t)(((Turn % NumLines()) + NumLines()) % NumLines())]; }
		int LineNumber(int Turn) const { return ((Turn % NumLines()) + NumLines()) % NumLines() + 1; }
		const FMember* MemberOf(const std::string& Role) const
		{
			for (const FMember& M : D.Members)
			{
				if (M.Role == Role) return &M;
			}
			return nullptr;
		}
		/** He may join the talk: he has read this month's book to its end (in any language). */
		static bool MayJoin(const std::string& Book, const std::set<std::string>& FinishedWorks) { return FinishedWorks.count(Book) > 0; }

		/**
		 * The librarian's advice: from what he finished (newest first, work ids), the first "if you liked X" whose Y he has not
		 * finished and the shelf has (Available); else this month's book if unread and on the shelf; else the first starter
		 * unread and on the shelf; else "".
		 */
		std::string Recommend(const std::vector<std::string>& FinishedNewestFirst, const std::set<std::string>& Available, int Month) const
		{
			const std::set<std::string> Done(FinishedNewestFirst.begin(), FinishedNewestFirst.end());
			auto Fits = [&](const std::string& W) { return !W.empty() && !Done.count(W) && Available.count(W); };
			for (const std::string& F : FinishedNewestFirst)
			{
				for (const auto& P : D.Pairs)
				{
					if (P.first == F && Fits(P.second)) return P.second;
				}
			}
			if (Fits(BookOf(Month))) return BookOf(Month);
			for (const std::string& S : D.Starters)
			{
				if (Fits(S)) return S;
			}
			return std::string();
		}

		/** A friend he recommended a book to answers this many days later, with this wording (the same every playthrough). */
		int ReplyDelay(const std::string& Work, const std::string& Friend) const
		{
			const int Span = std::max(1, D.ReplyMax - D.ReplyMin + 1);
			return D.ReplyMin + (int)(Hash(Work + "|" + Friend) % (uint32_t)Span);
		}
		int Opinion(const std::string& Work, const std::string& Friend) const
		{
			return 1 + (int)(Hash(Friend + "#" + Work, 7u) % (uint32_t)std::max(1, D.Opinions));
		}
	};

	/** A book he recommended to a friend: when, and whether the friend has told him what they thought. */
	struct FRecommendation
	{
		std::string Work;
		std::string Friend;
		int32_t Day = 0;
		bool bAnswered = false;
	};
}
