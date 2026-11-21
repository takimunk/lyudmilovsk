// The records and the town radio outside the engine: behaviour checks of MusicModel.h on the real Content/City/music.json
// (python3 tools/music/music.py), and every string key the station can say checked against the ru/en/ja tables.
//   tools/musicsim/run.sh              the checks
//   tools/musicsim/run.sh show [day]   a day's programme with the Russian words
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Music/MusicModel.h"

#include <fstream>
#include <sstream>

using namespace LyudMusic;

static int Failures = 0, Passed = 0;
#define CHECK(Cond, ...) do { if (Cond) { Passed++; } else { std::printf("  FAIL "); std::printf(__VA_ARGS__); std::printf("\n"); Failures++; } } while (0)

static bool ReadFile(const std::string& Path, std::string& Out)
{
	std::ifstream F(Path, std::ios::binary);
	if (!F)
	{
		return false;
	}
	std::stringstream SS;
	SS << F.rdbuf();
	Out = SS.str();
	return true;
}

static std::map<std::string, std::string> Table(const std::string& Lang)
{
	std::map<std::string, std::string> Out;
	std::string Text, Err;
	LyudJson::FValue J;
	if (ReadFile("unreal/Lyudmilovsk/Content/Localization/music/strings_" + Lang + ".json", Text) && LyudJson::Parse(Text, J, Err))
	{
		for (const auto& F : J.Fields)
		{
			Out[F.first] = F.second.S();
		}
	}
	return Out;
}

static int Placeholders(const std::string& S)
{
	int N = 0;
	for (size_t I = 0; I + 2 < S.size(); ++I)
	{
		if (S[I] == '{' && S[I + 1] >= '0' && S[I + 1] <= '9' && S[I + 2] == '}')
		{
			N = std::max(N, S[I + 1] - '0' + 1);
		}
	}
	return N;
}

static std::string Say(const FCaption& C, const std::map<std::string, std::string>& T)
{
	auto It = T.find(C.Key);
	std::string S = It == T.end() ? "<" + C.Key + ">" : It->second;
	for (size_t A = 0; A < C.Args.size(); ++A)
	{
		std::string V = C.Args[A].S;
		if (C.Args[A].bKey)
		{
			auto K = T.find(V);
			V = K == T.end() ? "<" + V + ">" : K->second;
		}
		const std::string P = "{" + std::to_string(A) + "}";
		for (size_t Pos; (Pos = S.find(P)) != std::string::npos;)
		{
			S.replace(Pos, P.size(), V);
		}
	}
	return S;
}

static FFacts Facts(long long Day)
{
	FFacts F;
	const uint32_t H = Hash((uint32_t)Day, 1u);
	F.TempC = (float)((int)(H % 35u) - 8);
	F.WindSpeed = (float)((H >> 6) % 18u);
	F.WindFromDeg = (float)((H >> 11) % 360u);
	F.Cloud = (float)((H >> 3) % 100u) / 100.f;
	F.Rain = ((H >> 9) % 4u) == 0 ? 2.f : 0.f;
	F.Snow = F.TempC < 1.f ? 1.f : 0.f;
	F.Fog = ((H >> 13) % 7u) == 0 ? 0.7f : 0.f;
	F.Thunder = ((H >> 17) % 11u) == 0 ? 0.5f : 0.f;
	F.bTideRising = (H >> 20) & 1u;
	F.TideTime = "14:20";
	F.Season = (int)((Day / 90) % 4);
	F.PaperHeadline = (Day % 3) ? "" : "headline";
	return F;
}

int main(int Argc, char** Argv)
{
	std::string Text, Err;
	if (!ReadFile("unreal/Lyudmilovsk/Content/City/music.json", Text))
	{
		std::printf("no Content/City/music.json (python3 tools/music/music.py)\n");
		return 1;
	}
	FModel M;
	if (!M.Load(Text, Err))
	{
		std::printf("load failed: %s\n", Err.c_str());
		return 1;
	}
	const auto Ru = Table("ru"), En = Table("en"), Ja = Table("ja");

	if (Argc >= 2 && std::string(Argv[1]) == "show")
	{
		const long long Day = Argc >= 3 ? std::atoll(Argv[2]) : 3;
		const FFacts F = Facts(Day);
		int LastSeg = -99;
		const FCaption* LastCap = nullptr;
		FAir Air;
		for (double S = 0; S < 86400; S += 1.0)
		{
			M.AirAt(Day, (int)(Day % 7), S, F, Air);
			if (Air.Segment != LastSeg)
			{
				std::printf("\n%s  [%s] %s\n", FModel::Clock(S).c_str(), KindName(Air.Kind), Air.Show.empty() ? "" : Say(FCaption{ 0, 0, Air.Show, {}, false }, Ru).c_str());
				LastSeg = Air.Segment;
				LastCap = nullptr;
			}
			if (Air.Cap && (!LastCap || Air.Cap->Start != LastCap->Start || Air.Cap->Key != LastCap->Key))
			{
				// only the first hour of music: the rest repeats the same shape
				if (Air.Kind != EKind::Music || S - M.Schedule[(size_t)Air.Segment].AtMin * 60.0 < 1200)
				{
					std::printf("  %s %s\n", FModel::Clock(S).c_str(), Say(*Air.Cap, Ru).c_str());
				}
			}
			LastCap = Air.Cap;
		}
		return 0;
	}

	// 1. every key the station can say exists in all three tables with the same placeholders, and the args fill them
	std::set<std::string> Missing;
	long long Caps = 0;
	auto CheckKey = [&](const std::string& K, int Args)
	{
		for (const auto* T : { &Ru, &En, &Ja })
		{
			auto It = T->find(K);
			if (It == T->end())
			{
				Missing.insert(K);
				return;
			}
			if (Args >= 0 && Placeholders(It->second) > Args)
			{
				Missing.insert(K + " (placeholders > args)");
			}
		}
	};
	FAir Air;
	long long FestSignOn = 0, FestNews = 0, FestHour = 0, FestShow = 0, FestWrongDay = 0, PlainOnHoliday = 0;
	for (long long Day = 0; Day < 120; ++Day)
	{
		FFacts F = Facts(Day);
		// every tenth day is a holiday (the name key stands in for festival.<id>.name, which is in the game's own table)
		const bool bHoliday = Day % 10 == 4;
		F.Festival = bHoliday ? "music.radio.name" : "";
		if (Day == 30)
		{
			M.Request.Track = 3;
			M.Request.PostedDay = 29;
		}
		bool bHero = false;
		double LastEnd = 0, Heard = 0;
		for (double S = 0; S < 86400; S += bHoliday ? 3.0 : 15.0)   // (a holiday is sampled finely: its greetings sit between the 15 s marks)
		{
			M.AirAt(Day, (int)(Day % 7), S, F, Air);
			if (Air.Kind == EKind::OffAir)
			{
				Heard = S;
				const int Min = (int)(S / 60);
				CHECK(Min >= 61 && Min < 360, "off air at day %lld %s", Day, FModel::Clock(S).c_str());
				continue;
			}
			if (Air.Play || Air.Cap || Air.Kind == EKind::SignOff)
			{
				Heard = S;
			}
			// the breath between two records is a few seconds; never longer
			CHECK(S - Heard <= 15.0, "silence on air at day %lld %s (%s)", Day, FModel::Clock(S).c_str(), KindName(Air.Kind));
			if (Air.Play)
			{
				CHECK(Air.Play->Track >= 0 && Air.Play->Track < (int)M.Tracks.size(), "bad track");
				CHECK(S - Air.Play->Start + Air.Play->From < M.Tracks[(size_t)Air.Play->Track].Seconds + 0.01, "a track plays past its end (day %lld %s)", Day, FModel::Clock(S).c_str());
				if (Air.Kind == EKind::Music && Air.Play->Start > LastEnd)
				{
					LastEnd = Air.Play->Start;
				}
			}
			if (Air.Cap)
			{
				++Caps;
				const bool bFestCap = Air.Cap->Key.compare(0, 17, "music.radio.fest.") == 0;
				FestSignOn += Air.Cap->Key == "music.radio.fest.sign_on";
				FestNews += Air.Cap->Key == "music.radio.fest.news";
				FestHour += Air.Cap->Key == "music.radio.fest.hour";
				FestWrongDay += bFestCap && !bHoliday;
				CheckKey(Air.Cap->Key, (int)Air.Cap->Args.size());
				for (const FArg& A : Air.Cap->Args)
				{
					if (A.bKey)
					{
						CheckKey(A.S, 0);
					}
				}
				bHero |= Air.Cap->bHeroRequest;
			}
			if (!Air.Show.empty())
			{
				CheckKey(Air.Show, 0);
				const bool bFestShow = Air.Show == "music.radio.show.festival";
				FestShow += bFestShow;
				FestWrongDay += bFestShow && !bHoliday;
				PlainOnHoliday += bHoliday && Air.Kind == EKind::Music && !bFestShow;
			}
		}
		if (Day == 30)
		{
			CHECK(bHero, "the hero's request (posted day 29) is read on day 30");
			CHECK(M.ConsumeRequest(30), "the request is used up after its show");
			CHECK(!M.Request.Valid(), "no request left");
		}
	}
	for (const std::string& K : Missing)
	{
		std::printf("  missing or short: %s\n", K.c_str());
	}
	CHECK(Missing.empty(), "every caption key in ru/en/ja (%zu missing)", Missing.size());
	CHECK(Caps > 1000, "the station says things (%lld captions over 120 days)", Caps);
	// the holiday broadcast (docs/festivals.md "The new toys"): twelve holidays in the 120 days
	CHECK(FestSignOn >= 12 && FestNews >= 24 && FestHour >= 60 && FestShow > 1000, "a holiday's air: greeted at sign-on, in the bulletins and every music hour (%lld, %lld, %lld; %lld samples of the holiday programme)",
		FestSignOn, FestNews, FestHour, FestShow);
	CHECK(FestWrongDay == 0 && PlainOnHoliday == 0, "the holiday programme only on a holiday, and all day then (%lld on plain days, %lld plain music hours on holidays)", FestWrongDay, PlainOnHoliday);

	// 2. the same moment is the same air (every radio in step), and different days differ
	{
		FAir A, B;
		const FFacts F = Facts(5);
		M.AirAt(5, 5, 13 * 3600 + 600, F, A);
		M.AirAt(5, 5, 13 * 3600 + 600, F, B);
		CHECK(A.Play && B.Play && A.Play->Track == B.Play->Track && A.Play->Start == B.Play->Start, "deterministic air");
		int Diff = 0;
		for (long long D = 0; D < 14; ++D)
		{
			FAir X, Y;
			M.AirAt(D, (int)(D % 7), 13 * 3600, Facts(D), X);
			M.AirAt(D + 1, (int)((D + 1) % 7), 13 * 3600, Facts(D + 1), Y);
			Diff += (X.Play && Y.Play && X.Play->Track != Y.Play->Track) ? 1 : 0;
		}
		CHECK(Diff >= 7, "the 13:00 record differs most days (%d of 14)", Diff);
	}

	// 3. schedule shape: news at 7, weather at 9, requests at 20, shipping at 23, Sunday's own show at 10, off air at 3
	{
		FAir X;
		const FFacts F = Facts(2);
		M.AirAt(2, 2, 7 * 3600 + 30, F, X);
		CHECK(X.Kind == EKind::News, "news at 07:00:30");
		M.AirAt(2, 2, 9 * 3600 + 30, F, X);
		CHECK(X.Kind == EKind::Weather, "weather at 09:00:30");
		M.AirAt(2, 2, 20 * 3600 + 600, F, X);
		CHECK(X.Kind == EKind::Requests, "requests at 20:10");
		M.AirAt(2, 2, 23 * 3600 + 60, F, X);
		CHECK(X.Kind == EKind::Shipping, "shipping forecast at 23:01");
		M.AirAt(2, 2, 3 * 3600, F, X);
		CHECK(X.Kind == EKind::OffAir, "off air at 03:00");
		M.AirAt(2, 2, 0 * 3600 + 1800, F, X);
		CHECK(X.Kind == EKind::Music && X.Play, "music past midnight (00:30)");
		M.AirAt(6, 6, 10 * 3600 + 600, Facts(6), X);
		CHECK(X.Kind == EKind::Music && X.Show == "music.radio.show.sunday", "Sunday's record show at 10:10");
		M.AirAt(4, 4, 10 * 3600 + 600, Facts(4), X);
		CHECK(X.Show != "music.radio.show.sunday", "no Sunday show on a Friday");
		M.AirAt(2, 2, 6 * 3600 + 5, F, X);
		bool bChime = false;
		for (const FSfx& S : X.Timeline.Sfx)
		{
			bChime |= S.Sound == "radio_chime";
		}
		CHECK(X.Kind == EKind::SignOn && bChime, "sign-on with the chime at 06:00");
	}

	// 4. roles of the city's acts
	CHECK(M.RoleOf("jukebox") == ERole::Jukebox, "jukebox");
	CHECK(M.RoleOf("tarelka_jukebox") == ERole::Jukebox, "the cafe's jukebox");
	CHECK(M.RoleOf("record_player@open") == ERole::Player, "a staff variant of a player");
	CHECK(M.RoleOf("radio") == ERole::Radio && M.RoleOf("pu_bakery_radio") == ERole::Radio, "radios");
	CHECK(M.RoleOf("okafor_radio") == ERole::None && M.RoleOf("radio_short") == ERole::None, "a radio amateur's set is not a broadcast radio");
	CHECK(M.RoleOf("look_records_hits") == ERole::Request, "the request box");
	CHECK(M.RoleOf("holm_record") == ERole::None, "a resident's own record player stays his story");
	CHECK(M.IsAutoOn("pu_bakery_radio") && !M.IsAutoOn("radio"), "auto radios");

	// 5. records, players, radios, the request, the save
	{
		CHECK(M.Records.size() == 18, "18 records (%zu)", M.Records.size());
		for (const FRecord& R : M.Records)
		{
			CHECK(M.RecordOfLine(R.JukeboxLine) == (int)(&R - &M.Records[0]), "jukebox line %s finds its record", R.JukeboxLine.c_str());
			CheckKey(R.Note, 0);
			CheckKey(M.Tracks[(size_t)R.Track].Title, 0);
		}
		std::vector<int> Owned = { 2, 5, 9 };
		const int A = M.NextOwned("P-WAVE/x", Owned), B = M.NextOwned("P-WAVE/x", Owned), C = M.NextOwned("P-WAVE/x", Owned), D = M.NextOwned("P-WAVE/x", Owned);
		CHECK(A == 2 && B == 5 && C == 9 && D == 2, "a player goes through the hero's records in turn (%d %d %d %d)", A, B, C, D);
		Owned = { 2, 4, 5, 9 };
		CHECK(M.NextOwned("P-WAVE/x", Owned) == 4, "a record bought in between comes in its place");
		CHECK(M.NextOwned("P-WAVE/y", {}) == -1, "no records: none");
		CHECK(M.HouseRecord("P-WAVE/y") >= 0, "a house record");
		// radios
		CHECK(!M.IsRadioOn("r1", "radio", 3, 600), "a radio starts off");
		CHECK(M.ToggleRadio("r1", "radio", 3, 600) && M.IsRadioOn("r1", "radio", 3, 1300), "switched on, it stays on");
		CHECK(!M.ToggleRadio("r1", "radio", 3, 1300) && !M.IsRadioOn("r1", "radio", 3, 1300), "and off");
		CHECK(M.IsRadioOn("b1", "pu_bakery_radio", 3, 600) && !M.IsRadioOn("b1", "pu_bakery_radio", 3, 23 * 60), "a bakery radio plays in its hours");
		CHECK(!M.ToggleRadio("b1", "pu_bakery_radio", 3, 600) && !M.IsRadioOn("b1", "pu_bakery_radio", 3, 700), "switched off for the day");
		CHECK(M.IsRadioOn("b1", "pu_bakery_radio", 4, 600), "back on the next day");
		// request
		M.LastRecord = 7;
		CHECK(M.PostRequest(10, "P-WAVE/records_hits") && M.Request.Track == M.Records[7].Track, "a request for the last record played");
		CHECK(!M.PostRequest(10, "P-WAVE/records_hits"), "one card at a time");
		CHECK(!M.ConsumeRequest(10), "not used up on the day it was posted");
		// save round trip
		M.ToggleRadio("r2", "radio", 10, 600);
		std::map<std::string, std::string> Saved;
		M.SaveState(Saved);
		FModel M2;
		M2.Load(Text, Err);
		M2.LoadState(Saved);
		CHECK(M2.Request.Track == M.Request.Track && M2.Request.PostedDay == 10 && M2.LastRecord == 7, "the request and the last record are saved");
		CHECK(M2.RadiosOn.count("r2") && M2.PlayerLast == M.PlayerLast, "radios and players are saved");
		M2.ResetState();
		CHECK(!M2.Request.Valid() && M2.RadiosOn.empty(), "a new game forgets");
	}
	for (const std::string& K : Missing)
	{
		std::printf("  missing: %s\n", K.c_str());
	}
	std::printf("%d passed, %d failed\n", Passed, Failures);
	return Failures ? 1 : 0;
}
