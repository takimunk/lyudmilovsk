// Offline checks of reading anywhere and the book club (unreal/.../Private/Books/ReadingModel.h; docs/books.md):
// bash tools/booksim/run.sh reading
//
// The light rule (roof, day, dusk, night with headlamp or a street lamp, rain, snow), the quote mode's sentences and the text
// kept in the diary, the quote store (no sentence twice, at most 200), standard pages, overdue loans, and the club as
// Content/Reading/reading.json sets it (12 books, its evening, the turns, the librarian's advice, a friend's answer).
#include "Books/ReadingModel.h"

#include <cstdio>
#include <fstream>
#include <sstream>

using namespace LyudReading;

static int Fails = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Fails; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static FLight At(float Sky, float Alt, float Rain = 0.f, float Snow = 0.f, bool bLamp = false, float LampM = -1.f)
{
	FLightIn In;
	In.OpenSky = Sky;
	In.SunAltDeg = Alt;
	In.RainMmH = Rain;
	In.Snow = Snow;
	In.bHeadlamp = bLamp;
	In.LampM = LampM;
	return Judge(In);
}

static void Light()
{
	// under a roof: always, full page, whatever the hour or the weather
	CHECK(At(0.f, -30.f).Verdict == EVerdict::Ok && At(0.f, -30.f).Bright == 1.f);
	CHECK(At(0.2f, -30.f, 8.f).Verdict == EVerdict::Ok);
	// outdoors by day: fine
	CHECK(At(1.f, 40.f).Verdict == EVerdict::Ok && At(1.f, 40.f).Bright == 1.f);
	// the dusk: dims, then too dark
	const FLight Dusk = At(1.f, -1.f);
	CHECK(Dusk.Verdict == EVerdict::Dim && Dusk.Bright > 0.35f && Dusk.Bright < 0.85f);
	CHECK(At(1.f, -4.f).Verdict == EVerdict::Dark);
	CHECK(At(1.f, -20.f).Verdict == EVerdict::Dark && !At(1.f, -20.f).CanRead());
	// brightness never rises as the sun sinks
	float Prev = 2.f;
	bool bMono = true;
	for (float A = 10.f; A >= -10.f; A -= 0.25f)
	{
		const FLight L = At(1.f, A);
		bMono &= L.Bright <= Prev + 1e-5f;
		Prev = L.Bright;
	}
	CHECK(bMono);
	// at night: the headlamp, or a lit street lamp near; a far one dims; too far: dark
	CHECK(At(1.f, -20.f, 0.f, 0.f, true).Verdict == EVerdict::Ok);
	CHECK(At(1.f, -20.f, 0.f, 0.f, false, 3.f).Verdict == EVerdict::Ok);
	CHECK(At(1.f, -20.f, 0.f, 0.f, false, 9.5f).Verdict == EVerdict::Dim);
	CHECK(At(1.f, -20.f, 0.f, 0.f, false, 14.f).Verdict == EVerdict::Dark);
	// rain on the page: never outdoors, not even by day or with the headlamp; a drizzle under 0.3 mm/h is nothing
	CHECK(At(1.f, 40.f, 2.f).Verdict == EVerdict::Rain && !At(1.f, 40.f, 2.f).CanRead());
	CHECK(At(1.f, -20.f, 2.f, 0.f, true).Verdict == EVerdict::Rain);
	CHECK(At(1.f, 40.f, 0.1f).Verdict == EVerdict::Ok);
	// snow is not rain: a snowfall doesn't wet the page
	CHECK(At(1.f, 20.f, 2.f, 1.f).Verdict == EVerdict::Ok);
	// a half-open place (a veranda) counts as outdoors past the roof line
	CHECK(At(0.5f, 20.f, 3.f).Verdict == EVerdict::Rain);
	// every readable verdict draws the page between 0.35 and 1
	bool bRange = true;
	for (float S = 0.f; S <= 1.f; S += 0.25f)
		for (float A = -20.f; A <= 20.f; A += 1.f)
			for (float M = -1.f; M <= 13.f; M += 2.f)
			{
				const FLight L = At(S, A, 0.f, 0.f, false, M);
				if (L.CanRead()) bRange &= L.Bright >= 0.35f && L.Bright <= 1.f;
			}
	CHECK(bRange);
}

static std::u16string Sub(const std::u16string& T, FSpan S) { return T.substr((size_t)S.Start, (size_t)S.Len); }

static void Sentences()
{
	const std::u16string Ru = u"# Глава первая\n\nБыл вечер. «Куда вы?» — спросил он.\nОна молчала… Потом ушла!\n\nНовый абзац без точки\n\nИ ещё.";
	const std::vector<FSpan> S = SentencesIn(Ru, 0, (int32_t)Ru.size());
	CHECK(S.size() == 6);
	if (S.size() == 6)
	{
		CHECK(Sub(Ru, S[0]) == u"Был вечер.");
		CHECK(Sub(Ru, S[1]) == u"«Куда вы?» — спросил он.");
		CHECK(Sub(Ru, S[2]) == u"Она молчала…");
		CHECK(Sub(Ru, S[3]) == u"Потом ушла!");
		CHECK(Sub(Ru, S[4]) == u"Новый абзац без точки");
		CHECK(Sub(Ru, S[5]) == u"И ещё.");
	}
	// only sentences starting in [From, To); one that runs past To is kept whole
	const int32_t Mid = (int32_t)Ru.find(u"Она");
	const std::vector<FSpan> Part = SentencesIn(Ru, Mid, Mid + 3);
	CHECK(Part.size() == 1 && Part.size() && Sub(Ru, Part[0]) == u"Она молчала…");
	// abbreviations with a dot inside a word ("e.g") do not end a sentence; a quote closing after the stop belongs to it
	const std::u16string En = u"He said \"Go.\" Then, e.g.so, nothing? Yes.";
	const std::vector<FSpan> E = SentencesIn(En, 0, (int32_t)En.size());
	CHECK(E.size() == 3 && E.size() && Sub(En, E[0]) == u"He said \"Go.\"");
	// Japanese: 。 ends a sentence even with no space after it
	const std::u16string Ja = u"吾輩は猫である。名前はまだ無い。どこで生れたか、とんと見当がつかぬ！";
	const std::vector<FSpan> J = SentencesIn(Ja, 0, (int32_t)Ja.size());
	CHECK(J.size() == 3 && J.size() && Sub(Ja, J[1]) == u"名前はまだ無い。");
	// a sentence that never ends is cut past MaxSentence at a space
	std::u16string Long;
	for (int I = 0; I < 200; ++I) Long += u"слово ";
	const std::vector<FSpan> L = SentencesIn(Long, 0, (int32_t)Long.size());
	CHECK(!L.empty() && L.size() && L[0].Len <= MaxSentence + 8);
	// out-of-range bounds are clamped, never read past the text
	CHECK(SentencesIn(Ru, -50, 100000).size() == 6);
	CHECK(SentencesIn(Ru, 10000, 20000).empty());
	CHECK(SentencesIn(u"", 0, 10).empty());
}

static void Quotes()
{
	// line breaks become one space; none between Japanese characters; long ones cut with an ellipsis
	const std::u16string T = u"Она  молчала,\nпотом\r\n ушла.";
	CHECK(QuoteText(T, { 0, (int32_t)T.size() }) == u"Она молчала, потом ушла.");
	const std::u16string J = u"名前は\nまだ無い。";
	CHECK(QuoteText(J, { 0, (int32_t)J.size() }) == u"名前はまだ無い。");
	std::u16string Long(1000, u'а');
	const std::u16string Q = QuoteText(Long, { 0, 1000 });
	CHECK((int32_t)Q.size() == MaxQuote + 1 && Q.back() == 0x2026);
	CHECK(QuoteText(T, { 5, 100000 }).size() > 0);

	FQuotes Store;
	CHECK(Store.Add({ "ru/faust", 120, u"Остановись, мгновенье!", 3 }));
	CHECK(!Store.Add({ "ru/faust", 120, u"Остановись, мгновенье!", 4 }));   // the same sentence: once
	CHECK(Store.Add({ "en/faust", 120, u"Stay, thou art so fair!", 4 }));   // the same offset in another language: another book
	CHECK(!Store.Add({ "ru/faust", 999, u"", 4 }));                         // nothing selected
	CHECK(Store.List.size() == 2 && Store.Has("ru/faust", 120) && !Store.Has("ru/faust", 121));
	for (int I = 0; I < 300; ++I) Store.Add({ "ru/dead_souls", I * 10, u"x", 5 });
	CHECK(Store.List.size() == FQuotes::MaxQuotes);
	CHECK(!Store.Has("ru/faust", 120) && Store.List.back().Offset == 2990);   // the oldest went
}

static void Stats()
{
	CHECK(StdPages(0, false) == 0 && StdPages(-5, true) == 0);
	CHECK(StdPages(10, false) == 1);
	CHECK(StdPages(1800 * 10, false) == 10 && StdPages(800 * 10, true) == 10);
	CHECK(StdPages(1800 * 10 + 800, false) == 10 && StdPages(1800 * 10 + 1000, false) == 11);
	CHECK(!IsOverdue(10, 16) && IsOverdue(10, 17) && IsOverdue(10, 40));
}

static std::string ReadFile(const char* Path)
{
	std::ifstream F(Path, std::ios::binary);
	std::stringstream S;
	S << F.rdbuf();
	return S.str();
}

static void Club(const char* JsonPath)
{
	FClub C;
	std::string Err;
	const std::string Json = ReadFile(JsonPath);
	CHECK(!Json.empty());
	CHECK(LoadClub(Json, C.D, Err));
	if (!Err.empty()) std::printf("  %s\n", Err.c_str());
	if (C.D.Books.size() != 12) return;
	// a book a month, never the same twice in a year
	std::set<std::string> Books(C.D.Books.begin(), C.D.Books.end());
	CHECK(Books.size() == 12);
	CHECK(C.BookOf(1) == C.D.Books[0] && C.BookOf(12) == C.D.Books[11] && C.BookOf(13) == C.D.Books[0] && C.BookOf(0) == C.D.Books[11]);
	// Thursday evenings, never on a festival or a holiday
	CHECK(C.D.Weekday == 3 && C.MeetsAt(3, 19.f, false, false));
	CHECK(!C.MeetsAt(3, 17.f, false, false) && !C.MeetsAt(3, 20.f, false, false));
	CHECK(!C.MeetsAt(2, 19.f, false, false) && !C.MeetsAt(3, 19.f, true, false) && !C.MeetsAt(3, 19.f, false, true));
	CHECK(C.MeetsOn(3, false, false) && !C.MeetsOn(3, true, false) && !C.MeetsOn(4, false, false));
	// five members, one for each speaking role, every role in the order has its member
	CHECK(C.D.Members.size() == 5 && C.NumLines() == 5);
	bool bRoles = true;
	for (const std::string& R : C.D.Order) bRoles &= C.MemberOf(R) != nullptr;
	CHECK(bRoles);
	CHECK(C.SpeakerRole(0) == C.D.Order[0] && C.SpeakerRole(5) == C.D.Order[0] && C.SpeakerRole(-1) == C.D.Order[4]);
	CHECK(C.LineNumber(0) == 1 && C.LineNumber(4) == 5 && C.LineNumber(5) == 1);
	// he joins only having read the month's book
	CHECK(FClub::MayJoin("faust", { "faust", "hamlet" }) && !FClub::MayJoin("faust", { "hamlet" }));
	// the librarian's advice: "if you liked X", newest first; skips what he has read and what the shelf lacks
	std::set<std::string> All;
	for (const auto& P : C.D.Pairs) { All.insert(P.first); All.insert(P.second); }
	for (const std::string& S : C.D.Starters) All.insert(S);
	for (const std::string& B : C.D.Books) All.insert(B);
	CHECK(!C.D.Pairs.empty() && !C.D.Starters.empty());
	const auto& P0 = C.D.Pairs[0];
	CHECK(C.Recommend({ P0.first }, All, 1) == P0.second);
	std::set<std::string> NoY = All;
	NoY.erase(P0.second);
	const std::string R2 = C.Recommend({ P0.first }, NoY, 1);
	CHECK(R2 != P0.second && R2 != P0.first);
	// nothing read: this month's book first
	CHECK(C.Recommend({}, All, 3) == C.BookOf(3));
	// he has read everything: nothing to say
	std::vector<std::string> Everything(All.begin(), All.end());
	CHECK(C.Recommend(Everything, All, 3).empty());
	// a pair never recommends what you just read
	bool bSelf = true;
	for (const auto& P : C.D.Pairs) bSelf &= P.first != P.second;
	CHECK(bSelf);
	// a friend answers in 4..8 days with one of the wordings, the same every time
	bool bDelay = true, bOpinion = true, bStable = true;
	std::set<int> Seen;
	for (const std::string& W : C.D.Books)
		for (const char* F : { "t_a", "t_b", "t_c", "t_d" })
		{
			const int D = C.ReplyDelay(W, F), O = C.Opinion(W, F);
			bDelay &= D >= C.D.ReplyMin && D <= C.D.ReplyMax;
			bOpinion &= O >= 1 && O <= C.D.Opinions;
			bStable &= D == C.ReplyDelay(W, F) && O == C.Opinion(W, F);
			Seen.insert(O);
		}
	CHECK(bDelay && bOpinion && bStable);
	CHECK((int)Seen.size() >= C.D.Opinions - 1);   // the wordings are all in use, not one for everybody
	// broken data is refused, not half-loaded
	FClubData Bad;
	CHECK(!LoadClub("{\"club\": {\"books\": [\"a\"]}}", Bad, Err));
	CHECK(!LoadClub("not json", Bad, Err));
}

int main(int Argc, char** Argv)
{
	const char* Json = Argc > 1 ? Argv[1] : "../../unreal/Lyudmilovsk/Content/Reading/reading.json";
	Light();
	Sentences();
	Quotes();
	Stats();
	Club(Json);
	std::printf("reading: %d checks, %d failed\n", Checks, Fails);
	return Fails ? 1 : 0;
}
