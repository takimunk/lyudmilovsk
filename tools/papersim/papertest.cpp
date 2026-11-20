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
		for (const char* Sub : { "", "city/", "town/", "quests/", "paper/", "films/" })
		{
			LoadTable(Content + "Localization/" + Sub + "strings_" + Lang + ".json", L);
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
