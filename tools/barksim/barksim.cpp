// The townsfolk's own lines (Private/People/TownBarks.cpp, docs/town_talk.md) run on the town's people outside the engine.
//   tools/barksim/run.sh              checks: town_barks.txt reads clean, every key has its words in ru/en/ja, every set fits
//                                     somebody of the town on a day, most people up and about have something to say
//   tools/barksim/run.sh day [lang]   a Wednesday of the town: what is said hour by hour (samples), then how often each set wins
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/PeopleSim.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/TownBarks.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>

using namespace LyudPeople;
using LyudBarks::EKind;
using LyudBarks::FBook;
using LyudBarks::FNow;
using LyudBarks::FSet;
using LyudBarks::FWho;

static const char* DataDir = "unreal/Lyudmilovsk/Content/PeopleData";
static const char* AnimalsDir = "unreal/Lyudmilovsk/Content/AnimalsData";
static const char* BarksFile = "unreal/Lyudmilovsk/Content/Dialogue/town_barks.txt";
static const char* LocDir = "unreal/Lyudmilovsk/Content/Localization";
static int Failures = 0;

#define CHECK(Cond, ...) do { if (Cond) { std::printf("  ok   "); } else { std::printf("  FAIL "); Failures++; } std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

static std::string ReadFile(const std::string& Path)
{
	std::ifstream F(Path, std::ios::binary);
	std::stringstream SS;
	SS << F.rdbuf();
	return SS.str();
}

/** A flat "key": "text" table; false if it doesn't read. */
static bool LoadTable(const std::string& Path, std::map<std::string, std::string>& Out)
{
	LyudJson::FValue J;
	std::string Err;
	if (!LyudJson::Parse(ReadFile(Path), J, Err) || J.Type != LyudJson::FValue::Object)
	{
		std::printf("  %s: %s\n", Path.c_str(), Err.c_str());
		return false;
	}
	for (const auto& F : J.Fields)
	{
		Out[F.first] = F.second.S();
	}
	return true;
}

/** Characters (not bytes) of UTF-8 text. */
static int Chars(const std::string& S)
{
	int N = 0;
	for (unsigned char C : S)
	{
		N += (C & 0xC0) != 0x80 ? 1 : 0;
	}
	return N;
}

/** The game's clock as tools/peoplesim has it: seconds since a Monday 00:00, sunrise 7:45, sunset 17:15 (late October). */
struct FClock
{
	double Seconds = 0.0;
	void Set(FWorld& W) const
	{
		const double H = Seconds / 3600.0;
		W.Hour = (float)std::fmod(H, 24.0);
		W.Weekday = (int)std::fmod(std::floor(H / 24.0), 7.0);
		W.Time = Seconds;
		W.Sunrise = 7.75f;
		W.Sunset = 17.25f;
		W.bSunUp = W.Hour > W.Sunrise && W.Hour < W.Sunset;
		W.SunYaw = W.Hour < 12.f ? 0.f : 30.f;
	}
	/** A rough sun over the Baltic in late October: up to 22 degrees at noon. */
	float SunAltitude(const FWorld& W) const
	{
		const float Day = W.Sunset - W.Sunrise;
		const float T = (W.Hour - W.Sunrise) / Day;
		return T > 0.f && T < 1.f ? 22.f * std::sin(3.14159265f * T) : -12.f;
	}
	static FClock At(int Weekday, float Hour) { FClock C; C.Seconds = (Weekday * 24.0 + Hour) * 3600.0; return C; }
};

/** The skies the test goes through: what ULyudWeatherSubsystem would give (cloud, rain mm/h, snow part, fog, C, wet). */
struct FSky { const char* Name; float Cloud, Rain, Snow, Fog, TempC, Wet; };
static const FSky Skies[] = {
	{ "clear", 0.15f, 0.f, 0.f, 0.f, 9.f, 0.f },
	{ "overcast", 0.9f, 0.f, 0.f, 0.1f, 7.f, 0.1f },
	{ "rain", 0.95f, 2.f, 0.f, 0.2f, 8.f, 0.8f },
	{ "fog", 0.5f, 0.f, 0.f, 0.8f, 6.f, 0.3f },
	{ "snow", 0.9f, 0.8f, 1.f, 0.2f, -3.f, 0.5f },
	{ "warm", 0.1f, 0.f, 0.f, 0.f, 25.f, 0.f },
};

static void SetNow(const FWorld& W, const FClock& C, const FSky& Sky, int Month, FNow& Now)
{
	Now.Hour = W.Hour;
	Now.Month = Month;
	Now.Weekday = W.Weekday;
	Now.Sun = LyudBarks::SunState(C.SunAltitude(W), Sky.Cloud);
	LyudBarks::WeatherWords(Sky.Cloud, Sky.Rain, Sky.Snow, Sky.Fog, Sky.TempC, Sky.Wet, Now.Weather);
}

/** Two people side by side: seated at the same place within 2 m (a bench, a table, the club's row of computers). */
static bool Together(const FPerson& A, const FPerson& B)
{
	return A.Id != B.Id && A.Place >= 0 && A.Place == B.Place && IsSeated(A.Pose) && IsSeated(B.Pose)
		&& A.State == EState::Doing && B.State == EState::Doing && (A.Pos - B.Pos).Len2D() < 2.f;
}

/** What a set needs of the moment that a day of the town can't give the way the set wants (for the coverage's who-only part). */
static bool HoursFit(const FSet& S, float Hour)
{
	return S.Hours.Empty() || S.Hours.Has(Hour, true);
}

static int Run(bool bDay, const std::string& Lang)
{
	FBook Book;
	Book.Seed(20261107u);
	const bool bParsed = Book.Parse(ReadFile(BarksFile));
	std::printf("town_barks.txt: %zu sets, %zu keys\n", Book.Sets.size(), Book.Keys().size());
	for (const std::string& Wn : Book.Warnings)
	{
		std::printf("  warning: %s\n", Wn.c_str());
	}
	CHECK(bParsed && Book.Warnings.empty(), "the file reads clean (%zu warnings)", Book.Warnings.size());

	// --- the words: every key in ru, en and ja; no words for keys nobody uses; not shadowed by the hand-written tables
	std::map<std::string, std::string> Tables[3], Main[3], Names;
	const char* Langs[3] = { "ru", "en", "ja" };
	for (int L = 0; L < 3; L++)
	{
		LoadTable(std::string(LocDir) + "/barks/strings_" + Langs[L] + ".json", Tables[L]);
		LoadTable(std::string(LocDir) + "/strings_" + Langs[L] + ".json", Main[L]);
	}
	LoadTable(std::string(LocDir) + "/town/strings_" + Lang + ".json", Names);
	const std::vector<std::string> Keys = Book.Keys();
	int Missing = 0, Empty = 0, Shadowed = 0, Unused = 0;
	for (const std::string& K : Keys)
	{
		for (int L = 0; L < 3; L++)
		{
			const auto It = Tables[L].find(K);
			if (It == Tables[L].end())
			{
				if (Missing++ < 8) std::printf("  no %s words for %s\n", Langs[L], K.c_str());
			}
			else if (It->second.empty())
			{
				Empty++;
			}
			Shadowed += Main[L].count(K) ? 1 : 0;
		}
	}
	const std::set<std::string> KeySet(Keys.begin(), Keys.end());
	for (int L = 0; L < 3; L++)
	{
		for (const auto& T : Tables[L])
		{
			if (!KeySet.count(T.first))
			{
				if (Unused++ < 8) std::printf("  %s words nobody says: %s\n", Langs[L], T.first.c_str());
			}
		}
	}
	CHECK(Missing == 0 && Empty == 0, "every line has its words in ru, en and ja (%d missing, %d empty)", Missing, Empty);
	CHECK(Unused == 0, "the barks tables hold no words for unused keys (%d)", Unused);
	CHECK(Shadowed == 0, "no key is also in the hand-written tables, which would win (%d)", Shadowed);
	auto Words = [&](const std::string& Key) -> std::string
	{
		const int L = Lang == "en" ? 1 : Lang == "ja" ? 2 : 0;
		const auto It = Tables[L].find("line." + Key);
		return It != Tables[L].end() ? It->second : Key;
	};
	{
		// the bubbles over the heads take ~2 lines of 34 characters (ru, en) or 18 (ja)
		int Longest[3] = { 0, 0, 0 };
		std::string LongestKey[3];
		for (const std::string& K : Keys)
		{
			for (int L = 0; L < 3; L++)
			{
				const auto It = Tables[L].find(K);
				const int N = It != Tables[L].end() ? Chars(It->second) : 0;
				if (N > Longest[L])
				{
					Longest[L] = N;
					LongestKey[L] = K;
				}
			}
		}
		CHECK(Longest[0] <= 100 && Longest[1] <= 100 && Longest[2] <= 48, "lines fit a bubble: longest ru %d (%s), en %d (%s), ja %d (%s)",
			Longest[0], LongestKey[0].c_str(), Longest[1], LongestKey[1].c_str(), Longest[2], LongestKey[2].c_str());
	}

	// --- a day of the town
	FWorld W;
	if (!W.Load(DataDir, AnimalsDir))
	{
		std::printf("load failed: %s\n", W.LastError.c_str());
		return 1;
	}
	W.AssignHomes();
	std::vector<std::string> FestIds, Phases;
	for (const FSet& S : Book.Sets)
	{
		for (const std::string& F : S.Festival.Yes)
		{
			if (F != "any" && std::find(FestIds.begin(), FestIds.end(), F) == FestIds.end()) FestIds.push_back(F);
		}
		for (const std::string& Ph : S.Phase.Yes)
		{
			if (std::find(Phases.begin(), Phases.end(), Ph) == Phases.end()) Phases.push_back(Ph);
		}
	}
	// Wednesday 04:00 to Thursday 04:00; a holiday on the square in the evening (mingling), fireworks watched from the
	// promenade, the fair at night (as ULyudFestivalSubsystem would set them)
	FClock C = FClock::At(2, 4.f);
	C.Set(W);
	W.Resettle();
	std::vector<char> FitA(Book.Sets.size(), 0), Picked(Book.Sets.size(), 0);
	std::vector<int> Wins(Book.Sets.size(), 0);
	FWho Who, Other;
	FNow Now;
	std::vector<int> Staff;
	int Samples = 0, WithLine = 0, Greeted = 0, GreetSamples = 0, Pairs = 0, PairsTalk = 0, EventLines = 0, EventAsks = 0;
	int SampleI = 0;
	std::map<int, int> PrintedHour;
	const float Dt = 1.f;
	bool bGather = false;
	for (double T = 0.0; T < 24.0 * 3600.0; T += Dt)
	{
		C.Seconds += Dt;
		C.Set(W);
		const float H = W.Hour;
		if (!bGather && W.Weekday == 2 && H >= 17.9f)
		{
			W.SetGathering("test_square", "S-SQUARE", 18.f, 21.f, 1.6f, EGatherMode::Mingle, false, V3());
			W.SetGathering("test_fireworks", "promenade", 21.f, 22.f, 1.2f, EGatherMode::Watch, false, V3());
			W.SetGathering("test_fair", "fair", 18.f, 23.f, 1.f, EGatherMode::Fair, false, V3());
			bGather = true;
		}
		W.Step(Dt);
		W.Events.clear();
		const int Sec = (int)std::lround(std::fmod(C.Seconds, 86400.0));
		if (Sec % 120 != 0)
		{
			continue;
		}
		const bool bFestNow = W.Weekday == 2 && H >= 18.f && H < 23.f;
		for (const FPerson& P : W.People)
		{
			if (!P.bTown || !LyudBarks::CanSpeak(W, P) || P.bExternal)
			{
				continue;
			}
			LyudBarks::FillWho(W, P, Who);
			Who.bHeroNear = true;
			// the who-part of every set: does this person fit it at this hour (whatever the sky and the holiday)
			for (size_t I = 0; I < Book.Sets.size(); I++)
			{
				const FSet& S = Book.Sets[I];
				if (S.Kind != EKind::Talk && S.Kind != EKind::Event && !FitA[I] && HoursFit(S, H) && Book.MatchWho(S.A, Who))
				{
					FitA[I] = 1;
				}
			}
			// what they would say now, under one of the skies; on the holiday evening, the holiday's
			const FSky& Sky = Skies[(SampleI++) % (sizeof(Skies) / sizeof(Skies[0]))];
			SetNow(W, C, Sky, 10, Now);
			if (bFestNow && !FestIds.empty())
			{
				Now.Festival = FestIds[(size_t)SampleI % FestIds.size()];
				Now.Phase = Phases.empty() || SampleI % 3 == 0 ? std::string() : Phases[(size_t)SampleI % Phases.size()];
			}
			else
			{
				Now.Festival.clear();
				Now.Phase.clear();
			}
			int SetIx = -1;
			Samples++;
			const std::string* Line = Book.PickLine(EKind::Say, Who, Now, nullptr, &SetIx);
			if (Line)
			{
				WithLine++;
				Wins[(size_t)SetIx]++;
				Picked[(size_t)SetIx] = 1;
				const int Hr = (int)H;
				if (bDay && PrintedHour[Hr] < 3 && SampleI % 7 == 0)
				{
					PrintedHour[Hr]++;
					const auto Nm = Names.find("npc." + P.Key + ".name");
					std::printf("%02d:%02d %-24s %-10s %3d %-14s %-12s %-8s [%s] %s\n", Hr, (int)((H - (float)Hr) * 60.f),
						Nm != Names.end() ? Nm->second.c_str() : P.Key.c_str(), P.Occupation.c_str(), P.Age,
						Who.Doing.empty() ? "-" : Who.Doing[0].c_str(), Who.At.size() > 1 ? Who.At[1].c_str() : "-", Sky.Name,
						Book.Sets[(size_t)SetIx].Name.c_str(), Words(*Line).c_str());
				}
			}
			GreetSamples++;
			if (Book.PickLine(EKind::Greet, Who, Now, nullptr, &SetIx))
			{
				Greeted++;
				Picked[(size_t)SetIx] = 1;
				FitA[(size_t)SetIx] = 1;
			}
		}
		// pairs: talking (the life's chat pairs) or side by side
		for (const FPerson& P : W.People)
		{
			if (!P.bTown || !LyudBarks::CanSpeak(W, P))
			{
				continue;
			}
			for (const FPerson& Q : W.People)
			{
				if (Q.Id <= P.Id || !Q.bTown || !LyudBarks::CanSpeak(W, Q) || !((P.Partner == Q.Id && Q.Partner == P.Id) || Together(P, Q)))
				{
					continue;
				}
				Pairs++;
				LyudBarks::FillWho(W, P, Who);
				LyudBarks::FillWho(W, Q, Other);
				Who.bHeroNear = Other.bHeroNear = true;
				for (size_t I = 0; I < Book.Sets.size(); I++)
				{
					const FSet& S = Book.Sets[I];
					if (S.Kind == EKind::Talk && !FitA[I] && HoursFit(S, H)
						&& ((Book.MatchWho(S.A, Who) && Book.MatchWho(S.B, Other)) || (Book.MatchWho(S.A, Other) && Book.MatchWho(S.B, Who))))
					{
						FitA[I] = 1;
					}
				}
				SetNow(W, C, Skies[(SampleI++) % (sizeof(Skies) / sizeof(Skies[0]))], 10, Now);
				bool bSwap = false;
				int SetIx = -1;
				if (const std::vector<std::string>* Talk = Book.PickTalk(Who, Other, Now, bSwap, &SetIx))
				{
					PairsTalk++;
					Wins[(size_t)SetIx]++;
					Picked[(size_t)SetIx] = 1;
					const int Hr = (int)H;
					if (bDay && PrintedHour[100 + Hr] < 1)
					{
						PrintedHour[100 + Hr]++;
						const FPerson& First = bSwap ? Q : P;
						const FPerson& Second = bSwap ? P : Q;
						std::printf("%02d:%02d talk [%s] %s (%s %d) and %s (%s %d):\n", Hr, (int)((H - (float)Hr) * 60.f), Book.Sets[(size_t)SetIx].Name.c_str(),
							First.Key.c_str(), First.Occupation.c_str(), First.Age, Second.Key.c_str(), Second.Occupation.c_str(), Second.Age);
						for (size_t L = 0; L < Talk->size(); L++)
						{
							std::printf("        %s %s\n", L % 2 == 0 ? "A" : "B", Words((*Talk)[L]).c_str());
						}
					}
				}
			}
		}
		// the restaurant's waiter and cook at the moments of a visit
		static const char* const Events[] = { "rest_sit", "rest_carry", "rest_serve", "rest_bill", "rest_bye" };
		for (const char* Role : { "waiter", "cook" })
		{
			W.StaffOnShift("restaurant", Role, Staff);
			if (Staff.empty())
			{
				continue;
			}
			LyudBarks::FillWho(W, W.People[(size_t)Staff[0]], Who);
			LyudBarks::FillWhoRole(Who, Role);
			Who.bHeroNear = true;
			SetNow(W, C, Skies[0], 10, Now);
			Now.Festival = bFestNow && !FestIds.empty() ? FestIds[(size_t)(SampleI++) % FestIds.size()] : std::string();
			for (const char* Ev : Events)
			{
				if (std::strcmp(Role, "cook") == 0 && std::strcmp(Ev, "rest_carry") != 0)
				{
					continue;
				}
				EventAsks++;
				for (size_t I = 0; I < Book.Sets.size(); I++)
				{
					const FSet& S = Book.Sets[I];
					if (S.Kind == EKind::Event && S.On == Ev && Book.MatchWho(S.A, Who) && Book.MatchNow(S, Now))
					{
						FitA[I] = 1;
					}
				}
				int SetIx = -1;
				if (Book.PickLine(EKind::Event, Who, Now, Ev, &SetIx))
				{
					EventLines++;
					Picked[(size_t)SetIx] = 1;
					FitA[(size_t)SetIx] = 1;
				}
			}
		}
	}
	std::printf("a day of the town: %d samples of somebody up and about, %d with a line; %d pairs, %d with an exchange; "
		"%d greetings of %d; %d event lines of %d\n", Samples, WithLine, Pairs, PairsTalk, Greeted, GreetSamples, EventLines, EventAsks);
	CHECK(Samples > 0 && WithLine > Samples * 0.75, "most people up and about have something to say (%.0f%%)", Samples ? 100.0 * WithLine / Samples : 0.0);
	CHECK(GreetSamples > 0 && Greeted > GreetSamples * 0.9, "nearly everyone greets the player walking past (%.0f%%)", GreetSamples ? 100.0 * Greeted / GreetSamples : 0.0);
	CHECK(Pairs > 0 && PairsTalk > Pairs * 0.3, "pairs side by side or talking find an exchange (%.0f%% of %d)", Pairs ? 100.0 * PairsTalk / Pairs : 0.0, Pairs);
	CHECK(EventAsks > 0 && EventLines == EventAsks, "the restaurant's waiter and cook have a line at every moment (%d of %d)", EventLines, EventAsks);
	int Never = 0;
	for (size_t I = 0; I < Book.Sets.size(); I++)
	{
		if (!FitA[I])
		{
			Never++;
			std::printf("  nobody fits [%s] (line %d) on this day\n", Book.Sets[I].Name.c_str(), Book.Sets[I].FileLine);
		}
	}
	// a Wednesday in October can't fit everything (summer, weekends, a festival of its own date): the rest are told
	CHECK(Never <= (int)Book.Sets.size() / 10, "nearly every set fits somebody of the town on a day (%d don't)", Never);
	if (bDay)
	{
		std::vector<std::pair<int, std::string>> Top;
		for (size_t I = 0; I < Book.Sets.size(); I++)
		{
			Top.push_back({ Wins[I], Book.Sets[I].Name });
		}
		std::sort(Top.rbegin(), Top.rend());
		std::printf("how often each set won (of %d lines, %d exchanges):\n ", WithLine, PairsTalk);
		for (size_t I = 0; I < Top.size(); I++)
		{
			std::printf(" %s %d", Top[I].second.c_str(), Top[I].first);
			if (I % 8 == 7) std::printf("\n ");
		}
		std::printf("\n");
	}
	std::printf(Failures ? "%d FAILED\n" : "all ok\n", Failures);
	return Failures ? 1 : 0;
}

int main(int Argc, char** Argv)
{
	const bool bDay = Argc > 1 && std::strcmp(Argv[1], "day") == 0;
	const std::string Lang = bDay && Argc > 2 ? Argv[2] : "ru";
	return Run(bDay, Lang);
}
