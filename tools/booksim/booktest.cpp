// Offline checks of the library books' page layout (unreal/.../Private/Books/BookLayout.h): bash tools/booksim/run.sh [book.txt ...]
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Books/BookLayout.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>

using namespace LyudBooks;

static int Fails = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Fails; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static float Wd(char16_t C) { return IsCJK(C) ? 16.f : (C == u' ' ? 4.f : 8.f); }

static FParams Params(float Width, int Rows)
{
	FParams P;
	P.Width = Width;
	P.Rows = Rows;
	P.Indent = 16.f;
	P.Width16 = Wd;
	return P;
}

// every line fits its width, lines are in text order, no text is lost between lines (only spaces and newlines skipped)
static void Invariants(const std::u16string& T, const FLayout& L, const FParams& P)
{
	int32_t Prev = 0;
	bool bOrder = true, bFit = true, bGap = true;
	for (size_t I = 0; I < L.Lines.size(); ++I)
	{
		const FLine& Ln = L.Lines[I];
		if (Ln.Start < Prev) bOrder = false;
		for (int32_t K = Prev; K < Ln.Start; ++K)
		{
			const char16_t C = T[(size_t)K];
			if (!(IsSpace(C) || C == u'\n' || C == u'\r' || C == u'#')) { bGap = false; }
		}
		float X = 0.f;
		for (int32_t K = Ln.Start; K < Ln.Start + Ln.Len; ++K) X += Ln.Kind == ELineKind::Heading ? Wd(T[(size_t)K]) * 1.08f : Wd(T[(size_t)K]);
		const float Avail = Ln.Kind == ELineKind::First ? P.Width - P.Indent : P.Width;
		if (X > Avail + 0.01f && Ln.Len > 1) bFit = false;
		Prev = Ln.Start + Ln.Len;
	}
	CHECK(bOrder);
	CHECK(bFit);
	CHECK(bGap);
	bool bRows = true, bPages = true;
	for (size_t Pg = 0; Pg < L.Pages.size(); ++Pg)
	{
		const FPage& Page = L.Pages[Pg];
		for (int32_t K = 0; K < Page.NumLines; ++K)
		{
			if (L.Lines[(size_t)(Page.FirstLine + K)].Row >= P.Rows) bRows = false;
		}
		if (Pg > 0 && Page.Start < L.Pages[Pg - 1].Start) bPages = false;
	}
	CHECK(bRows);
	CHECK(bPages);
	CHECK(!L.Pages.empty());
}

static std::u16string U(const char* S) { return FromUtf8(S, std::strlen(S)); }

int main(int Argc, char** Argv)
{
	{
		// utf-8
		const std::u16string S = U("Ёж 吾輩 😀");
		CHECK(S.size() == 7 + 1);
		CHECK(S[0] == 0x0401);
		CHECK(S[3] == 0x543E);
		CHECK(S[6] == 0xD83D && S[7] == 0xDE00);
	}
	{
		// prose wraps on spaces, the first line indented
		const std::u16string T = U("one two three four five six seven eight nine ten eleven twelve");
		const FParams P = Params(100.f, 10);
		const FLayout L = Layout(T, P);
		Invariants(T, L, P);
		CHECK(L.Lines.size() >= 4);
		CHECK(L.Lines[0].Kind == ELineKind::First);
		CHECK(L.Lines.back().bLast);
		CHECK(L.Pages.size() == 1);
	}
	{
		// headings open pages, PageOf finds offsets
		std::string S;
		for (int C = 0; C < 5; ++C)
		{
			S += "# Chapter " + std::to_string(C + 1) + "\n\n";
			for (int K = 0; K < 30; ++K) S += "Some words of a paragraph that goes on for a while and wraps. ";
			S += "\n\n";
		}
		const std::u16string T = U(S.c_str());
		const FParams P = Params(300.f, 20);
		const FLayout L = Layout(T, P);
		Invariants(T, L, P);
		CHECK(L.Headings.size() == 5);
		bool bTop = true;
		for (const FHeading& H : L.Headings)
		{
			const FPage& Pg = L.Pages[(size_t)H.Page];
			if (L.Lines[(size_t)Pg.FirstLine].Kind != ELineKind::Heading) bTop = false;
		}
		CHECK(bTop);
		CHECK(L.PageOf(0) == 0);
		CHECK(L.PageOf((int32_t)T.size() - 1) == (int32_t)L.Pages.size() - 1);
		const int32_t Mid = L.Pages[3].Start + 5;
		CHECK(L.PageOf(Mid) == 3);
		// the same bookmark in a bigger font lands on the page holding it
		const FParams P2 = Params(200.f, 12);
		const FLayout L2 = Layout(T, P2);
		const FPage& At = L2.Pages[(size_t)L2.PageOf(Mid)];
		CHECK(At.Start <= Mid && Mid < At.End);
	}
	{
		// Japanese: breaks between any two characters, never a 。 at a line start nor 「 at an end
		std::string S;
		for (int K = 0; K < 40; ++K) S += "吾輩は猫である。名前はまだ無い。「どこで生れたか」とんと見当がつかぬ。";
		const std::u16string T = U(S.c_str());
		const FParams P = Params(16.f * 13.f, 30);
		const FLayout L = Layout(T, P);
		Invariants(T, L, P);
		bool bKinsoku = true;
		for (const FLine& Ln : L.Lines)
		{
			if (Ln.Len == 0) continue;
			if (NoLineStart(T[(size_t)Ln.Start])) bKinsoku = false;
			if (NoLineEnd(T[(size_t)(Ln.Start + Ln.Len - 1)])) bKinsoku = false;
		}
		CHECK(bKinsoku);
		CHECK(L.Lines.size() > 30);
	}
	{
		// verse keeps its lines
		const std::u16string T = U("Мой дядя самых честных правил,\nКогда не в шутку занемог,\nОн уважать себя заставил\n\nИ лучше выдумать не мог.");
		const FParams P = Params(400.f, 20);
		const FLayout L = Layout(T, P);
		Invariants(T, L, P);
		CHECK(L.Lines.size() == 4);
		CHECK(L.Lines[0].Kind == ELineKind::Verse);
		CHECK(L.Lines[3].Kind == ELineKind::First);
	}
	{
		// a word longer than the line is cut, nothing hangs
		const std::u16string T = U("Supercalifragilisticexpialidocious_and_more_and_more_and_more");
		const FParams P = Params(80.f, 10);
		const FLayout L = Layout(T, P);
		Invariants(T, L, P);
		CHECK(L.Lines.size() >= 5);
	}
	{
		// short sections (stanzas under numbers) run on the page; a long one before a heading still opens a new page
		std::string S;
		for (int I = 0; I < 6; ++I)
		{
			S += "# " + std::to_string(I + 1) + "\n\nline one\nline two\nline three\n\n";
		}
		const std::u16string T = U(S.c_str());
		const FParams P = Params(200.f, 60);
		const FLayout L = Layout(T, P);
		Invariants(T, L, P);
		CHECK(L.Headings.size() == 6);
		CHECK(L.Pages.size() == 1);
		std::string Long = "# A\n\n";
		for (int I = 0; I < 40; ++I)
		{
			Long += "A paragraph of prose that fills its line and more.\n\n";
		}
		Long += "# B\n\nshort\n";
		const std::u16string T2 = U(Long.c_str());
		const FLayout L2 = Layout(T2, P);
		Invariants(T2, L2, P);
		CHECK(L2.Headings.size() == 2);
		CHECK(L2.Lines[(size_t)L2.Pages[(size_t)L2.Headings[1].Page].FirstLine].Kind == ELineKind::Heading);
	}
	{
		// empty text: one empty page
		const FLayout L = Layout(std::u16string(), Params(100.f, 10));
		CHECK(L.Pages.size() == 1);
		CHECK(L.PageOf(10) == 0);
	}
	// real books given on the command line: invariants and the time it takes
	for (int A = 1; A < Argc; ++A)
	{
		std::ifstream F(Argv[A], std::ios::binary);
		std::stringstream Ss;
		Ss << F.rdbuf();
		const std::string Raw = Ss.str();
		const auto T0 = std::chrono::steady_clock::now();
		const std::u16string T = FromUtf8(Raw.data(), Raw.size());
		const FParams P = Params(560.f, 26);
		const FLayout L = Layout(T, P);
		const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
		Invariants(T, L, P);
		std::printf("%s: %zu chars, %zu pages, %zu headings, %.1f ms\n", Argv[A], T.size(), L.Pages.size(), L.Headings.size(), Ms);
	}
	std::printf("%d checks, %d failed\n", Checks, Fails);
	return Fails ? 1 : 0;
}
