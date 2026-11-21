// The town paper outside the engine (Private/City/TownPaperModel.h, docs/town_paper.md): a year and more of made-up days
// (weather, tides, the hero's deeds drawn by a seeded die, the real festival calendar) set into issues and checked.
//   papertest test                 the checks (exit 1 on a failure)
//   papertest show [days] [lang]   the issues of the first N days (default 3) as text in ru|en|ja
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/City/TownPaperModel.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace LyudPaper;

static int Fails = 0, Passes = 0;
#define CHECK(C, ...) do { if (!(C)) { Fails++; std::printf("FAIL "); std::printf(__VA_ARGS__); std::printf("\n"); } else { Passes++; std::printf("ok   "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static std::string ReadFile(const std::string& Path)
{
	std::ifstream F(Path);
	std::stringstream S;
	S << F.rdbuf();
	return S.str();
}

static const char* Langs[] = { "ru", "en", "ja" };
static std::map<std::string, std::string> Table[3];

static void LoadTable(const std::string& Path, int L)
{
	LyudJson::FValue V;
	std::string Err;
	if (!LyudJson::Parse(ReadFile(Path), V, Err))
	{
		std::printf("FAIL cannot read %s: %s\n", Path.c_str(), Err.c_str());
		Fails++;
		return;
	}
	for (const auto& F : V.Fields)
	{
		if (F.second.Type == LyudJson::FValue::String) Table[L][F.first] = F.second.Str;
	}
}

static std::string Fill(const std::string& Tpl, const std::vector<std::string>& Args)
{
	std::string Out;
	for (size_t I = 0; I < Tpl.size(); ++I)
	{
		if (Tpl[I] == '{' && I + 2 < Tpl.size() && Tpl[I + 2] == '}' && Tpl[I + 1] >= '0' && Tpl[I + 1] <= '9')
		{
			const size_t A = (size_t)(Tpl[I + 1] - '0');
			Out += A < Args.size() ? Args[A] : "{?}";
			I += 2;
		}
		else
		{
			Out += Tpl[I];
		}
	}
	return Out;
}

static int MaxPlaceholder(const std::string& Tpl)
{
	int M = -1;
	for (size_t I = 0; I + 2 < Tpl.size(); ++I)
	{
		if (Tpl[I] == '{' && Tpl[I + 2] == '}' && Tpl[I + 1] >= '0' && Tpl[I + 1] <= '9') M = std::max(M, Tpl[I + 1] - '0');
	}
	return M;
}

static std::string Render(const FLine& L, int Lang)
{
	std::vector<std::string> Args;
	for (const std::string& A : L.Args)
	{
		if (!A.empty() && A[0] == '@')
		{
			const auto It = Table[Lang].find(A.substr(1));
			Args.push_back(It == Table[Lang].end() ? "[" + A.substr(1) + "]" : It->second);
		}
		else
		{
			Args.push_back(A);
		}
	}
	const auto It = Table[Lang].find(L.Key);
	return It == Table[Lang].end() ? "[" + L.Key + "]" : Fill(It->second, Args);
}

struct FDie
{
	uint32_t S;
	explicit FDie(uint32_t Seed) : S(Seed) {}
	uint32_t Next() { S = Hash(S + 0x9e3779b9u); return S; }
	float U() { return (float)(Next() & 0xffffff) / (float)0x1000000; }
	int I(int N) { return (int)(Next() % (uint32_t)N); }
};

/** A made-up day: the season's temperatures, a sky by the die, tides that walk 50 minutes a day, now and then a deed. */
/** The column «Говорят в городе» (docs/town_rumours.md): rumours.json "paper" (kind -> how many lines), and a detail key per kind. */
static std::vector<std::pair<std::string, int>> TalkKinds;
static std::string TalkDetail(const std::string& Kind)
{
	if (Kind == "beat") return "@npc.t_sorokin_gennady.name";
	if (Kind == "job") return "@rumour.job.kiosk";
	if (Kind == "tune") return "@music.tune.fur_elise";
	return std::string();
}

static FFacts MakeFacts(int Since, FDie& Die, const std::vector<std::string>& Quests)
{
	FFacts F;
	F.Since = Since;
	F.Date = LyudFest::AddDays(LyudFest::FDate{ 2026, 10, 20 }, Since);
	const int Doy = LyudFest::DaysFromCivil(F.Date) - LyudFest::DaysFromCivil(LyudFest::FDate{ F.Date.Y, 1, 1 });
	const float Mean = 8.f - 11.f * std::cos((Doy - 20) / 365.f * 6.2832f);
	FWeather& W = F.Tomorrow;
	W.bKnown = Die.U() > 0.02f;
	W.TMin = (int)std::lround(Mean - 3.f - Die.U() * 4.f);
	W.TMax = (int)std::lround(Mean + 2.f + Die.U() * 5.f);
	W.WindMs = (int)(Die.U() * Die.U() * 20.f);
	W.WindFromDeg = Die.I(360);
	W.SeaC = (int)std::lround(std::max(0.f, Mean - 1.f));
	static const char* Skies[] = { "clear", "fair", "cloudy", "overcast", "drizzle", "rain", "showers", "thunder", "fog" };
	W.Sky = Skies[Die.I(9)];
	if (Mean < 1.f && Die.U() < 0.5f) W.Sky = Die.U() < 0.7f ? "snow" : "sleet";
	W.bFirstSnow = W.Sky == "snow" && F.Date.M == 11 && F.Date.D < 25 && Die.U() < 0.3f;
	W.bAurora = Die.U() < 0.05f;
	const int Shift = (Since * 50) % (12 * 60 + 25);
	for (int K = 0; K < 4; ++K)
	{
		const int M = (Shift + K * 372) % (24 * 60);
		F.Tides.push_back({ M, K % 2 == 0, K % 2 == 0 ? 0 : -300 });
	}
	std::sort(F.Tides.begin(), F.Tides.end(), [](const FTide& A, const FTide& B) { return A.Minute < B.Minute; });
	static const char* Birds[] = { "HouseSparrow", "GreatTit", "Robin", "Waxwing", "MuteSwan", "Raven", "Jackdaw" };
	static const char* Fish[] = { "Herring", "Perch", "Cod", "Flounder", "SeaTrout", "Turbot" };
	if (Die.U() < 0.08f) F.Deeds.push_back({ "tamed", Birds[Die.I(7)], 0 });
	if (Die.U() < 0.06f) F.Deeds.push_back({ "rare", Birds[3 + Die.I(3)], 0 });
	if (Die.U() < 0.10f) F.Deeds.push_back({ "seen", Birds[Die.I(3)], 0 });
	if (Die.U() < 0.10f) F.Deeds.push_back({ Die.U() < 0.3f ? "record" : "catch", Fish[Die.I(6)], 200 + Die.I(4000) });
	if (Die.U() < 0.05f && !Quests.empty()) F.Deeds.push_back({ "quest", Quests[(size_t)Die.I((int)Quests.size())], 0 });
	if (Die.U() < 0.3f) F.BiteFish = Fish[Die.I(6)];
	// the photo club's contest: judged on the 26th, so the issue of the 27th may tell the hero's place (a die of its own: the
	// other days' deeds stay as they were)
	if (F.Date.D == FPaper::ContestJuryDay + 1)
	{
		FDie Shot(977u + (uint32_t)Since);
		if (Shot.U() < 0.8f)
		{
			static const char* Spots[] = { "pier", "promenade", "beach", "" };
			const int Spot = Shot.I(4);
			F.Deeds.push_back({ "contest", Spots[Spot], 1 + Shot.I(6) });
		}
	}
	// the talk of the town about the hero (ULyudTownRumours::PaperColumn): now and then a line or two
	for (int T = 0; T < 2 && !TalkKinds.empty() && Die.U() < 0.2f; ++T)
	{
		const auto& K = TalkKinds[(size_t)Die.I((int)TalkKinds.size())];
		FLine L{ "rumour.paper." + K.first + "." + std::to_string(1 + Die.I(K.second)), {} };
		const std::string D = TalkDetail(K.first);
		if (!D.empty()) L.Args.push_back(D);
		F.Talk.push_back(L);
	}
	// the clubs' tournaments (tournaments.json: Saturday chess, Wednesday dominoes, Sunday table tennis): today's to come,
	// yesterday's over, with a champion from the regulars or the hero, the hero in it now and then
	static const char* Cups[3][3] = { { "city.cup.cog_chess.title", "city.cup.venue.cog", "5" }, { "city.cup.san_dominoes.title", "city.cup.venue.san", "2" },
		{ "city.cup.tt_cup.title", "city.cup.venue.t3", "6" } };
	static const char* Names[] = { "npc.t_gonzalez_giuseppe.name", "npc.t_saar_noam.name", "npc.t_tran_anahit.name", "npc.t_okonkwo_ursula.name", "npc.t_kask_kenji.name" };
	const int Wd = LyudFest::Weekday(F.Date);
	for (const auto& C : Cups)
	{
		const int CupWd = C[2][0] - '0';
		FCup Cup;
		Cup.Title = C[0];
		Cup.Venue = C[1];
		Cup.StartMinute = 13 * 60;
		if (CupWd == Wd)
		{
			F.CupsToday.push_back(Cup);
		}
		if ((CupWd + 1) % 7 == Wd)
		{
			Cup.bOver = true;
			Cup.HeroReached = Die.U() < 0.5f ? -1 : Die.I(4);
			Cup.Winner = Cup.HeroReached == 3 ? std::string() : Names[Die.I(5)];
			Cup.RunnerUp = Cup.HeroReached == 2 ? std::string() : Names[Die.I(5)];
			F.CupsYesterday.push_back(Cup);
		}
	}
	return F;
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "test";
	const std::string Content = "unreal/Lyudmilovsk/Content/";
	FPaper Paper;
	const bool bLoaded = Paper.Load(ReadFile(Content + "City/town_paper.json"));
	const bool bCal = Paper.LoadCalendar(ReadFile(Content + "PeopleData/festivals.json"));
	for (int L = 0; L < 3; ++L)
	{
		const std::string Lang = Langs[L];
		for (const char* Sub : { "", "city/", "town/", "quests/", "paper/", "films/", "rumours/", "instruments/" })
		{
			LoadTable(Content + "Localization/" + Sub + "strings_" + Lang + ".json", L);
		}
	}
	{
		LyudJson::FValue R;
		std::string Err;
		if (LyudJson::Parse(ReadFile(Content + "PeopleData/rumours.json"), R, Err))
		{
			for (const auto& Fd : R["paper"].Fields) TalkKinds.push_back({ Fd.first, Fd.second.I(0) });
		}
	}
	std::vector<std::string> Quests;
	{
		LyudJson::FValue Q;
		std::string Err;
		if (LyudJson::Parse(ReadFile(Content + "UI/quests.json"), Q, Err))
		{
			const LyudJson::FValue& List = Q["quests"].Type == LyudJson::FValue::Array ? Q["quests"] : Q;
			for (const LyudJson::FValue& E : List.Items)
			{
				if (!E["id"].S().empty()) Quests.push_back(E["id"].S());
			}
		}
	}
	const int Days = Mode == "show" ? (Argc > 2 ? std::atoi(Argv[2]) : 3) : 800;
	const int ShowLang = Argc > 3 ? (std::string(Argv[3]) == "en" ? 1 : (std::string(Argv[3]) == "ja" ? 2 : 0)) : 0;

	FDie Die(12345u);
	std::vector<FIssue> All;
	std::string Prev;
	for (int D = 0; D < Days; ++D)
	{
		FFacts F = MakeFacts(D, Die, Quests);
		F.PrevFront = Prev;
		All.push_back(Paper.Build(F));
		Prev = All.back().Front;
	}
	if (Mode == "show")
	{
		for (const FIssue& I : All)
		{
			std::printf("==================== day %d\n", I.Since);
			for (const FPage& P : I.Pages)
			{
				for (const FLine& L : P) std::printf("%s\n\n", Render(L, ShowLang).c_str());
				std::printf("------\n");
			}
		}
		return 0;
	}

	CHECK(bLoaded, "town_paper.json loads (%s)", Paper.LastError.c_str());
	CHECK(bCal, "festivals.json loads");
	CHECK(!Quests.empty(), "quests.json gives quest ids (%d)", (int)Quests.size());
	int NoFour = 0, Empty = 0, RepeatFront = 0, RepeatStory = 0, Raw = 0, BadArgs = 0, Unfilled = 0, RoundTrip = 0;
	std::string FirstRaw, FirstBadArgs, FirstRepeat;
	std::map<std::string, int> FrontKinds;
	std::set<std::string> Used;
	for (size_t N = 0; N < All.size(); ++N)
	{
		const FIssue& I = All[N];
		NoFour += I.Pages.size() == 4 ? 0 : 1;
		for (const FPage& P : I.Pages) Empty += P.size() >= 2 ? 0 : 1;
		if (N > 0 && I.Front == All[N - 1].Front)
		{
			RepeatFront++;
			if (FirstRepeat.empty()) FirstRepeat = I.Front;
		}
		if (N > 0 && I.Pages.size() > 1 && All[N - 1].Pages.size() > 1 && I.Pages[1].back().Key == All[N - 1].Pages[1].back().Key) RepeatStory++;
		const FLine* Front = FPaper::FrontLine(I);
		if (Front)
		{
			const std::string& K = Front->Key;
			FrontKinds[K.compare(0, 12, "paper.story.") == 0 ? "story" : K.substr(0, K.rfind('.'))]++;
		}
		FPaper::Keys(I, Used);
		for (const FPage& P : I.Pages)
		{
			for (const FLine& L : P)
			{
				for (int Lg = 0; Lg < 3; ++Lg)
				{
					const auto It = Table[Lg].find(L.Key);
					if (It == Table[Lg].end())
					{
						if (FirstRaw.empty()) FirstRaw = std::string(Langs[Lg]) + ":" + L.Key;
						Raw++;
						continue;
					}
					if (MaxPlaceholder(It->second) >= (int)L.Args.size())
					{
						if (FirstBadArgs.empty()) FirstBadArgs = std::string(Langs[Lg]) + ":" + L.Key;
						BadArgs++;
					}
					for (const std::string& A : L.Args)
					{
						if (!A.empty() && A[0] == '@' && !Table[Lg].count(A.substr(1)))
						{
							if (FirstRaw.empty()) FirstRaw = std::string(Langs[Lg]) + ":" + A.substr(1);
							Raw++;
						}
					}
					const std::string Text = Render(L, Lg);
					if (Text.find("{?}") != std::string::npos || Text.find('[') == 0) Unfilled++;
				}
			}
		}
		FIssue Back;
		if (!FPaper::Unpack(FPaper::Pack(I), Back) || FPaper::Pack(Back) != FPaper::Pack(I)) RoundTrip++;
	}
	CHECK(NoFour == 0, "every issue has four pages (%d do not)", NoFour);
	CHECK(Empty == 0, "every page has a heading and something under it (%d empty)", Empty);
	CHECK(RepeatFront == 0, "no front page two days running (%d repeats, first %s)", RepeatFront, FirstRepeat.c_str());
	CHECK(RepeatStory == 0, "no town story two days running (%d repeats)", RepeatStory);
	CHECK(Raw == 0, "every key and '@' argument is in ru, en and ja (%d missing, first %s)", Raw, FirstRaw.c_str());
	CHECK(BadArgs == 0, "every template's placeholders have arguments (%d short, first %s)", BadArgs, FirstBadArgs.c_str());
	CHECK(Unfilled == 0, "every line renders with nothing left over (%d)", Unfilled);
	CHECK(RoundTrip == 0, "every issue survives Pack/Unpack for the save (%d do not)", RoundTrip);
	int Fest = 0;
	for (const auto& K : FrontKinds) Fest += (K.first.find("fest") != std::string::npos) ? K.second : 0;
	CHECK(Fest >= 20, "festivals make the front page through the year (%d days)", Fest);
	CHECK(FrontKinds["story"] >= 200, "ordinary days get the season's stories (%d days)", FrontKinds["story"]);
	CHECK(FrontKinds["paper.front.tamed_swan"] > 0 && FrontKinds["paper.front.record"] > 0, "the hero's swan and records make the front (%d, %d)",
		FrontKinds["paper.front.tamed_swan"], FrontKinds["paper.front.record"]);
	// the tournaments: the hero's cup on the front, the others' results and today's on the inside pages
	int CupWon = 0, CupToday = 0, HeroWins = 0;
	for (const FIssue& I : All)
	{
		for (const FPage& P : I.Pages)
		{
			for (const FLine& L : P)
			{
				CupWon += L.Key == "paper.cup.won" ? 1 : 0;
				CupToday += L.Key == "paper.cup.today" ? 1 : 0;
				HeroWins += L.Key == "paper.cup.hero_won" ? 1 : 0;
			}
		}
	}
	CHECK(FrontKinds["paper.front.cup"] > 0 && CupWon > 100 && CupToday > 300, "tournaments: the hero's cup on %d fronts (+%d inside), %d results, %d of today's",
		FrontKinds["paper.front.cup"], HeroWins, CupWon, CupToday);
	// the photo club's contest (docs/photo.md): the theme is announced on the 1st, the hero's place is told on the 27th
	{
		static const char* const Themes[4] = { "sea", "birds", "work", "festival" };
		int Firsts = 0, OpenRight = 0, OpenElsewhere = 0;
		for (const FIssue& I : All)
		{
			int Opens = 0;
			bool bRight = false;
			for (const FLine& L : I.Pages[3])
			{
				if (L.Key == "paper.contest.open")
				{
					++Opens;
					bRight = L.Args.size() == 4 && L.Args[0] == std::string("@paper.contest.theme.") + Themes[(I.Date.M - 1) % 4] && L.Args[1] == "25"
						&& L.Args[2] == "26" && L.Args[3] == "@paper.month.gen." + std::to_string(I.Date.M);
				}
			}
			for (size_t Pg = 0; Pg < 3; ++Pg)
			{
				for (const FLine& L : I.Pages[Pg]) Opens += L.Key == "paper.contest.open" ? 10 : 0;
			}
			if (I.Date.D == 1)
			{
				++Firsts;
				OpenRight += Opens == 1 && bRight ? 1 : 0;
			}
			else
			{
				OpenElsewhere += Opens;
			}
		}
		CHECK(Firsts >= 26 && OpenRight == Firsts && OpenElsewhere == 0, "the photo contest opens on every 1st with the month's theme, the 25th and the 26th, and on no other day (%d of %d, %d elsewhere)",
			OpenRight, Firsts, OpenElsewhere);
		CHECK(std::string(FPaper::ContestTheme(1)) == "sea" && std::string(FPaper::ContestTheme(2)) == "birds" && std::string(FPaper::ContestTheme(3)) == "work"
			&& std::string(FPaper::ContestTheme(4)) == "festival" && std::string(FPaper::ContestTheme(10)) == "birds" && std::string(FPaper::ContestTheme(11)) == "work"
			&& std::string(FPaper::ContestTheme(12)) == "festival", "the themes by the month: sea, birds, work, festival, and round again");
		// a made-up deed of each place: the issue of the 27th of a month (the jury sat on the 26th)
		auto Issue27 = [&](int Year, int Month, int Place, const char* Spot, const std::string& PrevFront, bool bSwan)
		{
			FFacts F;
			F.Date = LyudFest::FDate{ Year, Month, 27 };
			F.Since = LyudFest::DaysFromCivil(F.Date) - LyudFest::DaysFromCivil(LyudFest::FDate{ 2026, 10, 20 });
			F.PrevFront = PrevFront;
			if (Place > 0) F.Deeds.push_back({ "contest", Spot, Place });
			if (bSwan) F.Deeds.push_back({ "tamed", "MuteSwan", 0 });
			return Paper.Build(F);
		};
		auto Has = [](const FPage& P, const std::string& Prefix)
		{
			int N = 0;
			for (const FLine& L : P) N += L.Key.rfind(Prefix, 0) == 0 ? 1 : 0;
			return N;
		};
		auto Sound = [&](const FIssue& I)
		{
			// every key and argument in the three tables, every placeholder filled, the save's round trip
			int Bad = 0;
			for (const FPage& P : I.Pages)
			{
				for (const FLine& L : P)
				{
					for (int Lg = 0; Lg < 3; ++Lg)
					{
						const auto It = Table[Lg].find(L.Key);
						Bad += It == Table[Lg].end() || MaxPlaceholder(It->second) >= (int)L.Args.size() ? 1 : 0;
						for (const std::string& A : L.Args) Bad += !A.empty() && A[0] == '@' && !Table[Lg].count(A.substr(1)) ? 1 : 0;
					}
				}
			}
			FIssue Back;
			Bad += !FPaper::Unpack(FPaper::Pack(I), Back) || FPaper::Pack(Back) != FPaper::Pack(I) ? 1 : 0;
			FPaper::Keys(I, Used);
			return Bad == 0;
		};
		int FrontOk = 0, NoPlaceOk = 0, Unsound = 0;
		std::set<std::string> FrontKeys;
		for (int Mo = 1; Mo <= 12; ++Mo)
		{
			const std::string Theme = std::string("@paper.contest.theme.") + Themes[(Mo - 1) % 4];
			const FIssue A = Issue27(2027, Mo, 1, "pier", "", false);
			const FLine* FA = FPaper::FrontLine(A);
			FrontOk += FA && FA->Key.rfind("paper.front.contest.", 0) == 0 && FA->Args.size() == 2 && FA->Args[0] == Theme && FA->Args[1] == "@place.pier.name"
				&& Has(A.Pages[1], "paper.brief.contest") == 0 ? 1 : 0;
			const FIssue B = Issue27(2027, Mo, 1, "", "", false);
			const FLine* FB = FPaper::FrontLine(B);
			NoPlaceOk += FB && FB->Key.rfind("paper.front.contest_noplace.", 0) == 0 && FB->Args.size() == 1 && FB->Args[0] == Theme
				&& Has(B.Pages[1], "paper.brief.contest") == 0 ? 1 : 0;
			if (FA) FrontKeys.insert(FA->Key);
			if (FB) FrontKeys.insert(FB->Key);
			Unsound += (Sound(A) ? 0 : 1) + (Sound(B) ? 0 : 1);
		}
		CHECK(FrontOk == 12 && NoPlaceOk == 12, "the first place is the front story of the 27th in every month, with the theme and the place when it is known (%d and %d of 12)", FrontOk, NoPlaceOk);
		CHECK((int)FrontKeys.size() == Paper.VariantsOf("front.contest") + Paper.VariantsOf("front.contest_noplace") && FrontKeys.size() == 5,
			"its wordings take turns: three with the place, two without (%d seen)", (int)FrontKeys.size());
		// yesterday's front was this very story: another wording of it, never the same one
		const FIssue First = Issue27(2027, 1, 1, "pier", "", false);
		const FIssue Again = Issue27(2027, 1, 1, "pier", First.Front, false);
		const FLine* FAgain = FPaper::FrontLine(Again);
		CHECK(FAgain && FAgain->Key.rfind("paper.front.contest.", 0) == 0 && Again.Front != First.Front, "a front that would repeat yesterday's takes the next wording");
		// a swan tamed the same day keeps the front; the contest is then a brief
		const FIssue Swan = Issue27(2027, 1, 1, "pier", "", true);
		const FLine* FSwan = FPaper::FrontLine(Swan);
		CHECK(FSwan && FSwan->Key.rfind("paper.front.tamed_swan.", 0) == 0 && Has(Swan.Pages[1], "paper.brief.contest_1") == 1 && Sound(Swan),
			"a tamed swan keeps the front: the first place is then a brief in the chronicle");
		int BriefOk = 0;
		for (int Place = 2; Place <= 3; ++Place)
		{
			const FIssue I = Issue27(2027, 2, Place, "beach", "", false);
			const FLine* F = FPaper::FrontLine(I);
			const std::string Key = "paper.brief.contest_" + std::to_string(Place);
			bool bArg = false;
			for (const FLine& L : I.Pages[1]) bArg = bArg || (L.Key == Key && L.Args.size() == 1 && L.Args[0] == "@paper.contest.theme.birds");
			BriefOk += F && F->Key.find("contest") == std::string::npos && Has(I.Pages[1], "paper.brief.contest") == 1 && bArg && Sound(I) ? 1 : 0;
		}
		CHECK(BriefOk == 2, "the second and the third place are a brief in the chronicle with February's theme, never the front (%d of 2)", BriefOk);
		int Silent = 0;
		for (int Place = 4; Place <= 6; ++Place)
		{
			const FIssue I = Issue27(2027, 3, Place, "promenade", "", false);
			int Mentions = 0;
			for (const FPage& P : I.Pages)
			{
				for (const FLine& L : P) Mentions += L.Key.find("contest") != std::string::npos ? 1 : 0;
			}
			Silent += Mentions == 0 && I.Pages.size() == 4 ? 1 : 0;
		}
		const FIssue None = Issue27(2027, 3, 0, "", "", false);
		CHECK(Silent == 3 && None.Pages.size() == 4, "places four to six are no news, and a month the hero sat out is an ordinary issue (%d of 3)", Silent);
		CHECK(Unsound == 0, "every contest issue has its words in ru, en and ja and survives the save (%d do not)", Unsound);
		// an issue kept by a save made before the contest existed reads as it was (the archive holds keys, not deeds)
		// (the save's line written out by hand, as the model before the contest wrote it)
		const std::string Old = std::string("7\x1f" "12008\x1f" "2026\x1f" "10\x1f" "27\x1f" "paper.front.record.1|@fish.cod.name|2300")
			+ "\x1d" "paper.masthead\x1f" "12008\x1f" "27\x1f" "@paper.month.gen.10\x1f" "2026\x1f" "@paper.dow.1"
			+ "\x1e" "paper.front.record.1\x1f" "@fish.cod.name\x1f" "2300" + "\x1e" "paper.inside"
			+ "\x1d" "paper.head.town" "\x1e" "paper.brief.quest\x1f" "@quest.city_portrait.title"
			+ "\x1d" "paper.head.weather" "\x1e" "paper.weather.unknown" + "\x1d" "paper.head.bills" "\x1e" "paper.soon.none";
		FIssue OldBack;
		const bool bOld = FPaper::Unpack(Old, OldBack);
		CHECK(bOld && FPaper::Pack(OldBack) == Old && OldBack.Since == 7 && OldBack.Number == 12008 && OldBack.Date.M == 10 && OldBack.Date.D == 27
			&& OldBack.Pages.size() == 4 && OldBack.Pages[0].size() == 3 && OldBack.Pages[0][1].Args.size() == 2 && OldBack.Pages[1][1].Key == "paper.brief.quest"
			&& OldBack.Front == "paper.front.record.1|@fish.cod.name|2300", "an issue from an old save unpacks as it was packed");
	}
	// every string the paper owns is used by some day (dead text is a sign of a broken rule)
	int Unused = 0;
	std::string FirstUnused;
	for (const auto& KV : Table[0])
	{
		const std::string& K = KV.first;
		if (K.compare(0, 6, "paper.") != 0 || K.compare(0, 14, "paper.archive.") == 0 || K == "paper.read" || K == "paper.title" ||
			K == "paper.brief.win" || K.compare(0, 11, "paper.fest.") == 0 || K.compare(0, 10, "paper.dow.") == 0)
		{
			continue;
		}
		if (!Used.count(K))
		{
			Unused++;
			if (FirstUnused.empty()) FirstUnused = K;
		}
	}
	CHECK(Unused <= 12, "nearly every paper string shows on some day of 800 (%d never, first %s)", Unused, FirstUnused.c_str());
	// birthdays: some day of the year has one, and ages are sane
	int Bdays = 0, BadAge = 0;
	for (const FIssue& I : All)
	{
		for (const FLine& L : I.Pages[1])
		{
			if (L.Key.compare(0, 11, "paper.bday.") == 0)
			{
				Bdays++;
				const int Age = std::atoi(L.Args[1].c_str());
				BadAge += (Age > 0 && Age < 110) ? 0 : 1;
				break;
			}
		}
	}
	CHECK(BadAge == 0, "every birthday age is sane (%d are not)", BadAge);
	CHECK(Bdays > 300, "most days carry a resident's birthday (%d of %d)", Bdays, (int)All.size());
	std::printf("front pages: ");
	for (const auto& K : FrontKinds) std::printf("%s %d, ", K.first.c_str(), K.second);
	std::printf("\n%d passed, %d failed\n", Passes, Fails);
	return Fails ? 1 : 0;
}
