// A talk with a townsperson (Private/People/TownChat.cpp, docs/town_talk.md "A talk") run on the town's people outside the engine.
//   tools/townchatsim/run.sh                    checks: the data and the words, a Wednesday of the town (who can be talked to,
//                                               every talk's topics and lines, the names), the variety, the bond, the hold, the cost
//   tools/townchatsim/run.sh talks [lang] [n]   n talks of the day written out as the page shows them (ru, en, ja; 12)
//   tools/townchatsim/run.sh who <key> [lang]   one resident's talk at noon
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/Friends.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/PeopleManners.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/PeopleSim.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/TownBarks.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/TownChat.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
using LyudTownChat::ERefuse;
using LyudTownChat::FTalk;
using LyudTownChat::FTopic;

static const char* DataDir = "unreal/Lyudmilovsk/Content/PeopleData";
static const char* BarksFile = "unreal/Lyudmilovsk/Content/Dialogue/town_barks.txt";
static const char* LocDir = "unreal/Lyudmilovsk/Content/Localization";
static const char* Langs[3] = { "ru", "en", "ja" };
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

/** The game's clock as tools/barksim has it: seconds since a Monday 00:00, sunrise 7:45, sunset 17:15 (late October). */
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
	float SunAltitude(const FWorld& W) const
	{
		const float T = (W.Hour - W.Sunrise) / (W.Sunset - W.Sunrise);
		return T > 0.f && T < 1.f ? 22.f * std::sin(3.14159265f * T) : -12.f;
	}
	static FClock At(int Weekday, float Hour) { FClock C; C.Seconds = (Weekday * 24.0 + Hour) * 3600.0; return C; }
};

/** The skies the test goes through (cloud, rain mm/h, snow part, fog, C, wet), as tools/barksim's. */
struct FSky { const char* Name; float Cloud, Rain, Snow, Fog, TempC, Wet; };
static const FSky Skies[] = {
	{ "clear", 0.15f, 0.f, 0.f, 0.f, 9.f, 0.f }, { "overcast", 0.9f, 0.f, 0.f, 0.1f, 7.f, 0.1f }, { "rain", 0.95f, 2.f, 0.f, 0.2f, 8.f, 0.8f },
	{ "fog", 0.5f, 0.f, 0.f, 0.8f, 6.f, 0.3f }, { "snow", 0.9f, 0.8f, 1.f, 0.2f, -3.f, 0.5f }, { "warm", 0.1f, 0.f, 0.f, 0.f, 25.f, 0.f },
};
static const int NumSkies = (int)(sizeof(Skies) / sizeof(Skies[0]));

/** Their persona's strong sides, as the game fills them (TownTalk.cpp FillManner). */
static void FillTemper(const FPerson& P, FWho& Who)
{
	const FPersona Persona = MakePersona(P);
	Who.Temper.clear();
	auto Side = [&Who](float Trait, const char* High, const char* Low)
	{
		if (Trait > 0.66f && High) { Who.Temper.push_back(High); }
		else if (Trait < 0.34f && Low) { Who.Temper.push_back(Low); }
	};
	Side(Persona.Warmth, "warm", "reserved");
	Side(Persona.Nerve, "nervy", "calm");
	Side(Persona.Curiosity, "curious", nullptr);
	Side(Persona.Tidy, "tidy", "careless");
}

/** {0} .. {7} filled in; a placeholder with nothing for it stays (and is counted as a fault by the caller). */
static std::string Format(std::string Text, const std::string Args[8], const bool Have[8])
{
	for (int I = 0; I < 8; I++)
	{
		const std::string Tag = "{" + std::to_string(I) + "}";
		for (size_t At = Text.find(Tag); Have[I] && At != std::string::npos; At = Text.find(Tag, At + Args[I].size()))
		{
			Text.replace(At, Tag.size(), Args[I]);
		}
	}
	return Text;
}

struct FWords
{
	std::map<std::string, std::string> Barks[3], Chat[3], Names[3], Friends[3];
	bool Load()
	{
		bool bOk = true;
		for (int L = 0; L < 3; L++)
		{
			bOk &= LoadTable(std::string(LocDir) + "/barks/strings_" + Langs[L] + ".json", Barks[L]);
			bOk &= LoadTable(std::string(LocDir) + "/townchat/strings_" + Langs[L] + ".json", Chat[L]);
			bOk &= LoadTable(std::string(LocDir) + "/town/strings_" + Langs[L] + ".json", Names[L]);
			bOk &= LoadTable(std::string(LocDir) + "/friends/strings_" + Langs[L] + ".json", Friends[L]);
		}
		return bOk;
	}
	const std::string* Line(int L, const std::string& Key) const
	{
		const auto It = Barks[L].find("line." + Key);
		return It != Barks[L].end() ? &It->second : nullptr;
	}
	const std::string* Str(int L, const std::string& Key) const
	{
		const auto It = Chat[L].find(Key);
		return It != Chat[L].end() ? &It->second : nullptr;
	}
};

static const char* FormNames[6] = { "nom", "gen", "dat", "acc", "ins", "pre" };

/** One talk's inputs for a townsperson at this moment, as the game would fill them. */
struct FAsked
{
	int Met = 0;
	int Level = 0;
	const char* Bond = "";
};
static FAsked AskedOf(const FPerson& P)
{
	// a third never greeted him, a third on a couple of days, a third for a week; some of those are his friends
	const uint32_t H = LyudTownChat::Hash(P.Key);
	FAsked A;
	A.Met = H % 3u == 0u ? 0 : (H % 3u == 1u ? 2 : 6);
	const uint32_t F = (H >> 4) % 6u;
	A.Level = A.Met == 0 ? 0 : (F == 0u ? 3 : (F == 1u ? 2 : 1));
	A.Bond = A.Level >= 4 ? "close" : (A.Level == 3 ? "friend" : (A.Level == 2 ? "friendly" : ""));
	return A;
}

struct FRun
{
	FWorld W;
	FBook Book;
	LyudTownChat::FData Data;
	FWords Words;
	std::vector<std::string> RumourKinds;
	std::vector<std::string> FestIds;
	FWho Who;
	FNow Now;
	// what went wrong, counted
	int NoLine = 0, NoTitle = 0, BadCount = 0, Dup = 0, NoName = 0, Unfilled = 0, NoHello = 0, NoAsk = 0, StrayDetail = 0, StrayName = 0;
	std::string FirstFault;
	void Fault(int& Counter, const std::string& What)
	{
		if (Counter++ == 0 && FirstFault.empty())
		{
			FirstFault = What;
		}
	}

	void FillNow(const FClock& C, int SkyIx, bool bFestival, int FestIx)
	{
		const FSky& Sky = Skies[SkyIx % NumSkies];
		Now.Hour = W.Hour;
		Now.Month = 10;
		Now.Weekday = W.Weekday;
		Now.Sun = LyudBarks::SunState(C.SunAltitude(W), Sky.Cloud);
		LyudBarks::WeatherWords(Sky.Cloud, Sky.Rain, Sky.Snow, Sky.Fog, Sky.TempC, Sky.Wet, Now.Weather);
		Now.Festival = bFestival && !FestIds.empty() ? FestIds[(size_t)FestIx % FestIds.size()] : std::string();
		Now.Phase.clear();
	}

	void FillWho(const FPerson& P, const FAsked& A, int RumourIx)
	{
		LyudBarks::FillWho(W, P, Who);
		Who.bHeroNear = true;
		Who.Met = A.Met >= 5 ? "known" : (A.Met >= 1 ? "seen" : "new");
		Who.Bond = A.Bond;
		FillTemper(P, Who);
		Who.Heard.clear();
		if (RumourIx >= 0 && !RumourKinds.empty())
		{
			Who.Heard.push_back(RumourKinds[(size_t)RumourIx % RumourKinds.size()]);
		}
	}

	/** A line of on = Event for Who now; checks its words in the three languages against what the topic can fill in.
	 *  Other: whom it names (-1 nobody); bDetail: a rumour's detail is at hand. Out: its text in Lang (-1: none wanted). */
	const std::string* Say(const std::string& Event, int Other, bool bDetail, int Lang, std::string* Out, const std::string& Where)
	{
		const std::string* Key = Book.PickLine(EKind::Event, Who, Now, Event.c_str());
		if (!Key)
		{
			Fault(NoLine, Where + ": no line with on = " + Event);
			return nullptr;
		}
		std::string Args[8];
		bool Have[8] = { true, bDetail, false, false, false, false, false, false };
		for (int L = 0; L < 3; L++)
		{
			const std::string* Text = Words.Line(L, *Key);
			if (!Text)
			{
				Fault(NoLine, Where + ": no " + Langs[L] + " words for " + *Key);
				continue;
			}
			const auto Nick = Words.Friends[L].find("friends.nick.neighbour");
			Args[0] = Nick != Words.Friends[L].end() ? Nick->second : "?";
			Args[1] = L == 0 ? "«…»" : "\"…\"";
			if (Other >= 0)
			{
				const std::string Id = LyudTownChat::NameId(W.People[(size_t)Other].Key);
				for (int F = 0; F < 6; F++)
				{
					const std::string* Form = Words.Str(L, "townchat.fn." + Id + "." + FormNames[F]);
					Have[2 + F] = Form != nullptr;
					Args[2 + F] = Form ? *Form : std::string();
					if (!Form)
					{
						Fault(NoName, Where + ": no " + Langs[L] + " form " + FormNames[F] + " of the name " + Id);
					}
				}
			}
			if (!bDetail && Text->find("{1}") != std::string::npos)
			{
				Fault(StrayDetail, Where + ": " + *Key + " has {1} but no rumour is told");
			}
			if (Other < 0 && (Text->find("{2}") != std::string::npos || Text->find("{6}") != std::string::npos))
			{
				Fault(StrayName, Where + ": " + *Key + " names somebody but the topic has nobody");
			}
			const std::string Done = Format(*Text, Args, Have);
			if (Done.find('{') != std::string::npos)
			{
				Fault(Unfilled, Where + ": " + *Key + " in " + Langs[L] + " keeps a placeholder: " + Done);
			}
			if (L == Lang && Out)
			{
				*Out = Done;
			}
		}
		return Key;
	}
};

static const char* RefuseName(ERefuse R)
{
	static const char* const Names[] = { "free for a word", "not there", "riding", "serving a customer", "in a hurry", "shy" };
	return Names[(int)R];
}

static int Run(const std::string& Mode, const std::string& Lang, const std::string& Arg)
{
	FRun R;
	const int LangIx = Lang == "en" ? 1 : (Lang == "ja" ? 2 : 0);
	const bool bChecks = Mode.empty();
	const int PrintN = Mode == "talks" ? std::max(1, std::atoi(Arg.empty() ? "12" : Arg.c_str())) : 0;

	// --- the data, the lines, the words
	std::string Err;
	const bool bData = R.Data.Load(ReadFile(std::string(DataDir) + "/town_chat.json"), Err);
	R.Book.Seed(20261127u);
	const bool bBook = R.Book.Parse(ReadFile(BarksFile));
	const bool bWords = R.Words.Load();
	int ChatSets = 0, ChatLines = 0;
	std::set<std::string> ChatKeys;
	for (const FSet& S : R.Book.Sets)
	{
		if (S.Kind == EKind::Event && S.On.compare(0, 3, "tc_") == 0)
		{
			ChatSets++;
			ChatLines += (int)S.Lines.size();
			ChatKeys.insert(S.Lines.begin(), S.Lines.end());
		}
		for (const std::string& F : S.Festival.Yes)
		{
			if (F != "any" && std::find(R.FestIds.begin(), R.FestIds.end(), F) == R.FestIds.end()) R.FestIds.push_back(F);
		}
		for (const std::string& K : S.A.Heard.Yes)
		{
			if (K != "any" && std::find(R.RumourKinds.begin(), R.RumourKinds.end(), K) == R.RumourKinds.end()) R.RumourKinds.push_back(K);
		}
	}
	if (bChecks)
	{
		std::printf("town_chat.json: %zu activities told as news, %zu hobbies, %zu questions back, %zu names; %d sets, %d lines of a talk\n",
			R.Data.Did.size(), R.Data.Hobbies.size(), R.Data.Asks.size(), R.Data.Names.size(), ChatSets, ChatLines);
		CHECK(bData, "town_chat.json reads (%s)", Err.c_str());
		CHECK(bBook && R.Book.Warnings.empty(), "town_barks.txt reads clean (%zu warnings)", R.Book.Warnings.size());
		CHECK(bWords, "the string tables read");
		int Miss = 0;
		for (const LyudTownChat::FAsk& A : R.Data.Asks)
		{
			for (int N = 1; N <= A.Answers; N++)
			{
				for (int L = 0; L < 3; L++)
				{
					Miss += R.Words.Str(L, "townchat.ask." + A.Id + ".a" + std::to_string(N)) ? 0 : 1;
				}
			}
		}
		for (const char* Key : { "townchat.prompt", "townchat.said", "townchat.entry", "townchat.gift", "townchat.keys", "townchat.keys_next",
			"townchat.keys_ask", "townchat.keys_bye", "townchat.diary" })
		{
			for (int L = 0; L < 3; L++)
			{
				Miss += R.Words.Str(L, Key) ? 0 : 1;
			}
		}
		CHECK(Miss == 0, "the page's words and the hero's answers are in ru, en and ja (%d missing)", Miss);
	}
	if (!bData || !bBook || !bWords)
	{
		return 1;
	}

	// --- the town
	const char* Env = std::getenv("LYUD_ANIMALS_DIR");
	const std::string AnimalsDir = Env && *Env ? Env : "unreal/Lyudmilovsk/Content/AnimalsData";
	FWorld& W = R.W;
	if (!W.Load(DataDir, AnimalsDir))
	{
		std::printf("load failed: %s\n", W.LastError.c_str());
		return 1;
	}
	W.AssignHomes();
	int Town = 0, Named = 0;
	for (const FPerson& P : W.People)
	{
		if (P.bTown)
		{
			Town++;
			Named += R.Data.HasName(LyudTownChat::NameId(P.Key)) ? 1 : 0;
		}
	}
	if (bChecks)
	{
		CHECK(Town > 200 && Named == Town, "every townsperson's given name has its forms (%d of %d)", Named, Town);
	}

	// --- a Wednesday from 04:00: everybody asked every ten minutes
	FClock C = FClock::At(2, 4.f);
	C.Set(W);
	W.Resettle();
	int Asked = 0, Talks = 0, TopicsSum = 0, WithAsk = 0, SampleI = 0, Printed = 0, ShyGrown = 0, KidsFree = 0, KidsShyKnown = 0;
	int Refused[6] = {};
	std::map<std::string, int> TopicN, EventN;
	std::set<std::string> Said;
	std::set<int> Talked;
	std::map<int, std::map<std::string, int>> FirstLineAt;
	double BuildNs = 0.0;
	bool bGather = false;
	FTalk Talk;
	const float Dt = 1.f;
	for (double T = 0.0; T < 24.0 * 3600.0; T += Dt)
	{
		C.Seconds += Dt;
		C.Set(W);
		const float H = W.Hour;
		if (!bGather && W.Weekday == 2 && H >= 17.9f)
		{
			W.SetGathering("test_square", "S-SQUARE", 18.f, 21.f, 1.6f, EGatherMode::Mingle, false, V3());
			W.SetGathering("test_fair", "fair", 18.f, 23.f, 1.f, EGatherMode::Fair, false, V3());
			bGather = true;
		}
		W.Step(Dt);
		W.Events.clear();
		const int Sec = (int)std::lround(std::fmod(C.Seconds, 86400.0));
		if (Sec % 600 != 0)
		{
			continue;
		}
		const int Hr = (int)H;
		const bool bFestNow = W.Weekday == 2 && H >= 18.f && H < 23.f;
		const int Day = (int)(C.Seconds / 86400.0);
		for (const FPerson& P : W.People)
		{
			if (!P.bTown)
			{
				continue;
			}
			const FAsked A = AskedOf(P);
			const ERefuse Why = LyudTownChat::CanChat(W, P, R.Data, A.Met, A.Level);
			if (Why == ERefuse::Gone)
			{
				continue;
			}
			Asked++;
			Refused[(int)Why]++;
			SampleI++;
			if (P.Age < R.Data.R.KidAge && Why != ERefuse::Riding && Why != ERefuse::Work && Why != ERefuse::Hurry)
			{
				// a child: shy with a stranger, not with somebody they greeted on two days
				KidsFree += Why == ERefuse::None ? 1 : 0;
				KidsShyKnown += LyudTownChat::CanChat(W, P, R.Data, R.Data.R.KidShyMet, 0) == ERefuse::Shy ? 1 : 0;
				KidsShyKnown += LyudTownChat::CanChat(W, P, R.Data, 0, 0) == ERefuse::Shy ? 0 : 1;
			}
			ShyGrown += Why == ERefuse::Shy && P.Age >= R.Data.R.KidAge ? 1 : 0;
			R.FillNow(C, SampleI, bFestNow, SampleI);
			R.FillWho(P, A, SampleI % 4 == 0 ? SampleI / 4 : -1);
			R.Book.ClearRecent();       // every talk on its own: no help from the lines the others were given
			const std::string Where = P.Key + " at " + std::to_string(Hr) + "h";
			if (Why != ERefuse::None)
			{
				const char* Event = LyudTownChat::RefuseEvent(Why);
				if (*Event)
				{
					const std::string* Key = R.Say(Event, -1, false, -1, nullptr, Where);
					if (Key)
					{
						Said.insert(*Key);
					}
				}
				continue;
			}
			const auto T0 = std::chrono::steady_clock::now();
			LyudTownChat::Build(W, P, R.Data, R.Book, R.Who, R.Now, Day, Talk);
			BuildNs += (double)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - T0).count();
			Talks++;
			Talked.insert(P.Id);
			TopicsSum += (int)Talk.Topics.size();
			if ((int)Talk.Topics.size() < R.Data.R.TopicsMin || (int)Talk.Topics.size() > R.Data.R.TopicsMax)
			{
				R.Fault(R.BadCount, Where + ": " + std::to_string(Talk.Topics.size()) + " topics");
			}
			const bool bPrint = (PrintN > 0 && Printed < PrintN && (Hr == 8 || Hr == 12 || Hr == 16 || Hr == 19) && Sec % 3600 == 0
				&& SampleI % 37 == 0) || (Mode == "who" && P.Key == Arg && Hr == 12 && Sec % 3600 == 0);
			std::string Text;
			const std::string* Hello = R.Say(Talk.Hello, -1, false, LangIx, &Text, Where);
			if (!Hello)
			{
				R.Fault(R.NoHello, Where + ": no greeting");
			}
			else
			{
				Said.insert(*Hello);
			}
			if (bPrint)
			{
				Printed++;
				const auto Nm = R.Words.Names[LangIx].find("npc." + P.Key + ".name");
				std::printf("\n%02d:%02d  %s (%s, %d, %s; met %d, bond '%s'; %s)\n   %s\n", Hr, (int)((H - (float)Hr) * 60.f),
					Nm != R.Words.Names[LangIx].end() ? Nm->second.c_str() : P.Key.c_str(), P.Occupation.c_str(), P.Age,
					P.Activity >= 0 ? W.Activities[(size_t)P.Activity].Id.c_str() : "-", A.Met, A.Bond, R.Now.Weather.empty() ? "dry" : R.Now.Weather[0].c_str(),
					Text.c_str());
			}
			std::set<std::string> Ids;
			for (size_t I = 0; I < Talk.Topics.size(); I++)
			{
				const FTopic& Tp = Talk.Topics[I];
				if (!Ids.insert(Tp.Id).second)
				{
					R.Fault(R.Dup, Where + ": topic " + Tp.Id + " twice");
				}
				TopicN[Tp.Id]++;
				EventN[Tp.Event]++;
				for (int L = 0; L < 3; L++)
				{
					if (!R.Words.Str(L, Tp.Title))
					{
						R.Fault(R.NoTitle, Where + ": no " + Langs[L] + " title " + Tp.Title);
					}
				}
				const bool bDetail = Tp.Id == "rumour";
				const std::string* Key = R.Say(Tp.Event, Tp.Other, bDetail, LangIx, &Text, Where + " " + Tp.Id);
				if (Key)
				{
					Said.insert(*Key);
					if (I == 0)
					{
						FirstLineAt[Hr][*Key]++;
					}
				}
				if (bPrint)
				{
					const std::string* Title = R.Words.Str(LangIx, Tp.Title);
					std::printf("   %zu. %s\n      %s\n", I + 1, Title ? Title->c_str() : Tp.Title.c_str(), Text.c_str());
				}
			}
			if (Talk.Ask >= 0)
			{
				WithAsk++;
				const LyudTownChat::FAsk& Ask = R.Data.Asks[(size_t)Talk.Ask];
				if (P.Age < Ask.MinAge || P.Age > Ask.MaxAge || Talk.AskAfter < 1 || Talk.AskAfter > (int)Talk.Topics.size())
				{
					R.Fault(R.NoAsk, Where + ": question " + Ask.Id + " out of place");
				}
				const std::string* Q = R.Say("tc_ask_" + Ask.Id, -1, false, LangIx, &Text, Where + " ask");
				if (Q)
				{
					Said.insert(*Q);
				}
				if (bPrint)
				{
					std::printf("   (after %d) %s\n", Talk.AskAfter, Text.c_str());
				}
				for (int N = 1; N <= Ask.Answers; N++)
				{
					const std::string* Rep = R.Say("tc_reply_" + Ask.Id + "_" + std::to_string(N), -1, false, LangIx, &Text, Where + " reply");
					if (Rep)
					{
						Said.insert(*Rep);
					}
					else
					{
						R.Fault(R.NoAsk, Where + ": no reply " + std::to_string(N) + " to " + Ask.Id);
					}
					if (bPrint)
					{
						const std::string* Ans = R.Words.Str(LangIx, "townchat.ask." + Ask.Id + ".a" + std::to_string(N));
						std::printf("      %d) %s  ->  %s\n", N, Ans ? Ans->c_str() : "?", Text.c_str());
					}
				}
			}
			const std::string* Bye = R.Say(Talk.Bye, -1, false, LangIx, &Text, Where);
			if (Bye)
			{
				Said.insert(*Bye);
			}
			else
			{
				R.Fault(R.NoHello, Where + ": no goodbye");
			}
			if (bPrint)
			{
				std::printf("   %s\n", Text.c_str());
			}
		}
	}
	if (!bChecks)
	{
		if (Mode == "who" && Printed == 0)
		{
			const int Id = W.FindPerson(Arg);
			std::printf("%s: %s\n", Arg.c_str(), Id < 0 ? "nobody of that key" : "not free for a word at noon on the test's Wednesday");
		}
		return 0;
	}

	std::printf("a Wednesday: %d asked, %d talks with %d different people; refused: %d riding, %d serving, %d in a hurry, %d shy\n", Asked, Talks,
		(int)Talked.size(), Refused[(int)ERefuse::Riding], Refused[(int)ERefuse::Work], Refused[(int)ERefuse::Hurry], Refused[(int)ERefuse::Shy]);
	std::printf("  topics:");
	for (const auto& KV : TopicN)
	{
		std::printf(" %s %d%%", KV.first.c_str(), (int)std::lround(100.0 * KV.second / std::max(1, Talks)));
	}
	std::printf("\n");
	if (!R.FirstFault.empty())
	{
		std::printf("  first fault: %s\n", R.FirstFault.c_str());
	}
	CHECK((int)Talked.size() >= Town * 8 / 10, "most of the town can be talked to some time of the day (%d of %d)", (int)Talked.size(), Town);
	CHECK(R.BadCount == 0 && R.Dup == 0, "every talk has %d to %d different topics (%.1f on average; %d out of range, %d twice)", R.Data.R.TopicsMin,
		R.Data.R.TopicsMax, (double)TopicsSum / std::max(1, Talks), R.BadCount, R.Dup);
	CHECK(R.NoLine == 0 && R.NoHello == 0, "every greeting, topic, refusal and goodbye has a line with words in ru, en and ja (%d without)", R.NoLine + R.NoHello);
	CHECK(R.NoTitle == 0, "every topic's title (the hero's question) is in ru, en and ja (%d missing)", R.NoTitle);
	CHECK(R.NoName == 0 && R.Unfilled == 0 && R.StrayDetail == 0 && R.StrayName == 0,
		"names and details fill in: %d forms missing, %d placeholders left, %d details and %d names where there is none to give", R.NoName, R.Unfilled,
		R.StrayDetail, R.StrayName);
	CHECK(R.NoAsk == 0 && WithAsk > Talks / 5 && WithAsk < Talks * 4 / 5, "a question back in some talks, with a reply to each answer (%d%% of talks, %d faults)",
		(int)std::lround(100.0 * WithAsk / std::max(1, Talks)), R.NoAsk);
	CHECK(Refused[(int)ERefuse::Work] > 0 && Refused[(int)ERefuse::Shy] > 0, "somebody serving a customer and a shy child are met in a day (%d, %d)",
		Refused[(int)ERefuse::Work], Refused[(int)ERefuse::Shy]);
	CHECK(ShyGrown == 0 && KidsShyKnown == 0 && KidsFree > 0, "only children are shy, and only with a stranger (%d grown, %d wrong, %d children talk)", ShyGrown,
		KidsShyKnown, KidsFree);
	for (const char* Topic : { "day", "work", "weather", "hobby", "kin", "friend", "town", "festival", "rumour" })
	{
		if (TopicN[Topic] == 0)
		{
			CHECK(false, "topic %s is never offered", Topic);
		}
	}
	// the variety: the line most of an hour's talks begin with (the busy hours)
	int WorstShare = 0, WorstHour = -1;
	std::string WorstLine;
	for (const auto& KV : FirstLineAt)
	{
		int Sum = 0, Most = 0;
		std::string MostLine;
		for (const auto& L : KV.second)
		{
			Sum += L.second;
			if (L.second > Most)
			{
				Most = L.second;
				MostLine = L.first;
			}
		}
		const int Share = (int)std::lround(100.0 * Most / std::max(1, Sum));
		if (Sum >= 300 && Share > WorstShare)
		{
			WorstShare = Share;
			WorstHour = KV.first;
			WorstLine = MostLine;
		}
	}
	int Unsaid = 0;
	for (const std::string& K : ChatKeys)
	{
		if (!Said.count(K))
		{
			if (Unsaid++ < 12) std::printf("  never said: %s\n", K.c_str());
		}
	}
	CHECK(WorstShare <= 12, "no line opens more than 12%% of an hour's talks (the most: %d%% at %d h, %s)", WorstShare, WorstHour, WorstLine.c_str());
	CHECK(Unsaid * 100 <= (int)ChatKeys.size() * 15, "at least 85%% of the talk's lines are said in the day (%d of %zu never)", Unsaid, ChatKeys.size());
	const double AvgUs = BuildNs / 1000.0 / std::max(1, Talks);
	CHECK(AvgUs < 400.0, "a talk is built in %.1f us on average (on demand, one at a time)", AvgUs);

	// --- 200 residents at noon don't sound the same: a second pass at one moment, everybody asked once
	{
		std::set<std::string> Distinct;
		std::map<std::string, int> Openers;
		int N = 0;
		FClock Noon = C;
		int Ix = 0;
		for (const FPerson& P : W.People)
		{
			if (!P.bTown)
			{
				continue;
			}
			// (the town as the day's run left it at 04:00: asleep; ask them as if awake about their day by who they are)
			const FAsked A = AskedOf(P);
			Ix++;
			R.FillNow(Noon, Ix, false, 0);
			R.Now.Hour = 12.f;
			R.FillWho(P, A, -1);
			R.Book.ClearRecent();
			LyudTownChat::Build(W, P, R.Data, R.Book, R.Who, R.Now, 3, Talk);
			std::string Sig;
			for (const FTopic& Tp : Talk.Topics)
			{
				const std::string* Key = R.Book.PickLine(EKind::Event, R.Who, R.Now, Tp.Event.c_str());
				Sig += (Key ? *Key : std::string("?")) + (Tp.Other >= 0 ? "@" + W.People[(size_t)Tp.Other].Key : std::string()) + "|";
			}
			Distinct.insert(Sig);
			N++;
		}
		CHECK((int)Distinct.size() * 100 >= N * 90, "asked at one moment, %d of %d townsfolk say something of their own (their topics' lines together)",
			(int)Distinct.size(), N);
	}

	// --- the bond (docs/friends.md): a talk nudges it, capped a day
	{
		LyudFriends::FModel M;
		std::string FErr;
		const bool bF = M.Data.Load(ReadFile(std::string(DataDir) + "/friends.json"), FErr);
		CHECK(bF, "friends.json reads (%s)", FErr.c_str());
		const std::string Key = "t_test_person";
		const int A1 = M.Chatted(Key, 10), A2 = M.Chatted(Key, 10), A3 = M.ChatAnswered(Key, 10), A4 = M.ChatAnswered(Key, 10);
		CHECK(A1 == M.Data.R.Chat && A2 == 0 && A3 == M.Data.R.ChatAnswer && A4 == 0 && M.Find(Key)->Points == M.Data.R.ChatCap,
			"a day's talks give %d + %d and no more (got %d, %d, %d, %d)", M.Data.R.Chat, M.Data.R.ChatAnswer, A1, A2, A3, A4);
		CHECK(M.ChatAnswered("t_nobody", 10) == 0 && !M.Find("t_nobody"), "an answer without a talk counts nothing");
		for (int Day = 11; Day < 40; Day++)
		{
			M.Greeted(Key, Day);
			M.Chatted(Key, Day);
			M.ChatAnswered(Key, Day);
		}
		CHECK(M.Level(Key) == 3, "a hello and a talk every day for a month make a friend, not a close friend (level %d, %d points)", M.Level(Key),
			M.Find(Key)->Points);
		LyudFriends::FModel M2;
		M2.Data = M.Data;
		M2.Greeted("t_week", 1);
		for (int Day = 1; Day <= 7; Day++)
		{
			M2.Greeted("t_week", Day);
			M2.Chatted("t_week", Day);
		}
		CHECK(M2.Level("t_week") == 1, "a hello and a talk every day for a week: acquainted, not yet friendly (level %d)", M2.Level("t_week"));
		std::map<std::string, std::string> Saved;
		M.Save(Saved);
		LyudFriends::FModel M3;
		M3.Data = M.Data;
		M3.LoadState(Saved);
		const LyudFriends::FBond* B = M3.Find(Key);
		CHECK(B && B->ChatDay == 39 && B->Chats == 30 && B->ChatToday == M.Data.R.ChatCap && B->Points == M.Find(Key)->Points,
			"the talks are saved and loaded with the bond");
		std::map<std::string, std::string> Old;
		Old["b.t_old"] = "40,1,9,9,9,1,-1,-1,-1,0,0,0,0,0,-1,-1,17,-1,-1,-1,2,-1|||";
		M3.LoadState(Old);
		B = M3.Find("t_old");
		CHECK(B && B->Points == 40 && B->ChatDay == -1 && B->Chats == 0 && M3.Chatted("t_old", 12) == M.Data.R.Chat, "a save from before the talks loads");
	}

	// --- somebody stopped on their way stands while the talk lasts, then goes on
	{
		int Walker = -1;
		for (int Try = 0; Try < 6 * 3600 && Walker < 0; Try++)
		{
			C.Seconds += Dt;
			C.Set(W);
			W.Step(Dt);
			W.Events.clear();
			for (const FPerson& P : W.People)
			{
				if (P.bTown && P.State == EState::Going && P.Speed > 0.5f && !P.bIndoors && P.BikeMode == 0 && W.Hour > 8.f
					&& LyudTownChat::CanChat(W, P, R.Data, 6, 1) == ERefuse::None && (int)P.Path.size() - P.PathI > 4)
				{
					Walker = P.Id;
					break;
				}
			}
		}
		if (Walker < 0)
		{
			CHECK(false, "no walker found to stop");
		}
		else
		{
			const V3 At = W.People[(size_t)Walker].Pos;
			W.HoldWalk(Walker, 30.f);
			for (int I = 0; I < 20; I++)
			{
				C.Seconds += Dt;
				C.Set(W);
				W.Step(Dt);
				W.Events.clear();
			}
			const float Held = (W.People[(size_t)Walker].Pos - At).Len2D();
			const bool bStill = W.People[(size_t)Walker].State == EState::Going;
			W.HoldWalk(Walker, 0.f);
			for (int I = 0; I < 20; I++)
			{
				C.Seconds += Dt;
				C.Set(W);
				W.Step(Dt);
				W.Events.clear();
			}
			const float Went = (W.People[(size_t)Walker].Pos - At).Len2D();
			CHECK(Held < 0.05f && bStill && (Went > 3.f || W.People[(size_t)Walker].State != EState::Going),
				"a walker held for the talk stands (%.2f m in 20 s) and goes on after (%.1f m in the next 20 s)", Held, Went);
		}
	}

	std::printf(Failures == 0 ? "ALL OK\n" : "%d FAILED\n", Failures);
	return Failures == 0 ? 0 : 1;
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "";
	if (Mode == "talks")
	{
		return Run(Mode, Argc > 2 ? Argv[2] : "ru", Argc > 3 ? Argv[3] : "");
	}
	if (Mode == "who")
	{
		if (Argc < 3)
		{
			std::printf("who <resident key> [lang]\n");
			return 1;
		}
		return Run(Mode, Argc > 3 ? Argv[3] : "ru", Argv[2]);
	}
	return Run("", "ru", "");
}
