// The diary that writes itself, outside the engine (docs/diary.md, "Days"): the same DayLog.cpp the game runs, fed 60 days
// of made-up events and weather, and checked. Build and run with tools/daylog/run.sh.
//   run.sh                 the checks (exit 1 on a failure)
//   run.sh show [lang] [n] the first n days written out (ru by default)
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/UI/DayLog.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/MiniJson.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace LyudDayLog;

namespace
{
	const std::string Content = "unreal/Lyudmilovsk/Content/";
	int Failures = 0;

	void Fail(const std::string& What)
	{
		if (Failures < 30)
		{
			std::printf("FAIL %s\n", What.c_str());
		}
		++Failures;
	}

	std::string ReadFile(const std::string& Path)
	{
		std::ifstream F(Path, std::ios::binary);
		std::stringstream S;
		S << F.rdbuf();
		return S.str();
	}

	using FTable = std::map<std::string, std::string>;

	void Merge(FTable& T, const std::string& Path)
	{
		LyudJson::FValue V;
		std::string Err;
		const std::string Text = ReadFile(Path);
		if (Text.empty())
		{
			return;
		}
		if (!LyudJson::Parse(Text, V, Err))
		{
			Fail(Path + ": " + Err);
			return;
		}
		for (const auto& KV : V.Fields)
		{
			T[KV.first] = KV.second.S();
		}
	}

	FTable Strings(const std::string& Lang)
	{
		FTable T;
		Merge(T, Content + "Localization/strings_" + Lang + ".json");
		for (const char* Sub : { "diary", "barks", "city", "films", "friends", "garden", "letters", "music", "paper", "quests", "town", "rail", "aton", "instruments" })
		{
			Merge(T, Content + "Localization/" + Sub + "/strings_" + Lang + ".json");
		}
		return T;
	}

	/** A line in words, as ULyudDayLog shows it: a missing name falls back to the plain variant. */
	std::string Render(const FLine& L, const FTable& T, bool& bFellBack)
	{
		bFellBack = false;
		std::vector<std::string> Args;
		for (const std::string& A : L.Args)
		{
			if (!A.empty() && A[0] == '@')
			{
				auto It = T.find(A.substr(1));
				if (It == T.end())
				{
					bFellBack = true;
					break;
				}
				Args.push_back(It->second);
			}
			else
			{
				Args.push_back(A);
			}
		}
		const std::string Key = bFellBack ? L.Fallback : L.Key;
		auto It = T.find(Key);
		if (It == T.end())
		{
			return "<" + Key + ">";
		}
		std::string S = It->second;
		for (size_t I = 0; I < Args.size() && !bFellBack; ++I)
		{
			const std::string P = "{" + std::to_string(I) + "}";
			for (size_t At; (At = S.find(P)) != std::string::npos;)
			{
				S.replace(At, P.size(), Args[I]);
			}
		}
		return S;
	}

	/** Made-up days: a real mix of the game's events (real ids, so the names resolve) and an hourly sky. */
	void MakeDays(FModel& M, int First, int N, uint32_t Seed)
	{
		std::mt19937 R(Seed);
		auto Chance = [&R](float P) { return std::uniform_real_distribution<float>(0.f, 1.f)(R) < P; };
		auto Of = [&R](const auto& V) { return V[std::uniform_int_distribution<size_t>(0, V.size() - 1)(R)]; };
		const std::vector<std::string> Birds = { "GreatTit", "HerringGull", "Rook", "Starling", "BlueTit", "HoodedCrow", "Robin", "Chaffinch", "NoSuchBird" };
		const std::vector<std::string> Fish = { "fish_cod", "fish_herring", "fish_eelpout", "fish_turbot" };
		const std::vector<std::string> Npcs = { "belov_misha", "clerk", "nobody_known" };
		const std::vector<std::string> Food = { "apple", "fish_cod", "nonexistent_food" };
		const std::vector<std::string> Places = { "academ", "alley", "beach", "cable_lower", "cable_upper", "club" };
		const std::vector<std::string> Fests = { "sun_day.zenith", "city_day.fireworks", "sky_lantern" };
		const std::vector<std::string> Films = { "chomon_hotel", "cohl_fantasmagorie" };
		const std::vector<std::pair<std::string, std::string>> Misc = {
			{ "do", "job_cafe_scoops" }, { "do", "job_board" }, { "win", "sp_dominoes" }, { "play_tune", "kalinka" }, { "radio", "on" },
			{ "read_book", "tolstoy" }, { "read", "paper_issue" }, { "letter_sent", "tonya" }, { "do", "garden_pick" }, { "do", "san_walk" },
			{ "do", "girma_telescope" }, { "do", "school_microscope" }, { "do", "un_chess" }, { "do", "jukebox_records" }, { "do", "rcb_tea" },
			{ "do", "sp_bikes" }, { "do", "haircut" }, { "do", "aton_scope" }, { "kite_flown", "" }, { "bird", "" }, { "corvid_gift", "Rook" },
			{ "corvid_snatch", "HoodedCrow" }, { "photo", "beach" }, { "leave_computer", "" }, { "get", "apple" }, { "walk", "" }, { "do", "look_wallpaper" },
			// tonight's systems (2026-11-21): lighthouse, brook, station, animals, cure, telescopes, habits, rounds, winter, cups
			{ "do", "keeper_log" }, { "reach", "mayak_lighthouse" }, { "reach", "brook_bridge" }, { "reach", "cable_lower" }, { "do", "kiosk_cablecar" },
			{ "animal_friend", "Maru" }, { "animal_stroked", "Cat" }, { "animal_fed", "Dog" }, { "animal_named", "Cat" }, { "procedure", "oxygen" },
			{ "observe", "jupiter_night" }, { "observe", "saturn" }, { "bird_habit", "anting" }, { "round_done", "paper" }, { "winter", "snowman" },
			{ "winter", "skate" }, { "winter", "banya" }, { "winter", "new_year_swim" }, { "cup_won", "chess" }, { "beat", "petrovich" },
			{ "quote", "x" }, { "book_club", "x" }, { "fined", "conductor" }, { "stamp_set", "x" }, { "do", "pu_cafe_cat" }, { "do", "mk_catch" } };
		const char* Precips[] = { "", "", "", "", "drizzle", "rain", "snow", "thunder" };
		for (int D = First; D < First + N; ++D)
		{
			// one day in ten the player didn't play
			if (Chance(0.1f))
			{
				continue;
			}
			const int Kind = std::uniform_int_distribution<int>(0, 7)(R);
			const float Base = std::uniform_real_distribution<float>(-12.f, 28.f)(R);
			for (int H = 8; H < 23; ++H)
			{
				FSky S;
				S.Precip = Chance(0.35f) ? Precips[Kind] : "";
				S.Cloud = std::uniform_real_distribution<float>(0.f, 1.f)(R);
				S.TempC = Base + std::uniform_real_distribution<float>(-3.f, 3.f)(R);
				S.Fog = Kind == 0 && H < 11 ? 0.8f : 0.f;
				S.bRainbow = Kind == 5 && H == 15 && Chance(0.3f);
				S.bAurora = Kind == 6 && H == 22 && Chance(0.3f);
				M.Sample(D, H, S);
				M.Sample(D, H, S);  // the second sample of an hour must not count
			}
			const int Events = std::uniform_int_distribution<int>(0, 12)(R);
			for (int E = 0; E < Events; ++E)
			{
				switch (std::uniform_int_distribution<int>(0, 9)(R))
				{
				case 0: M.Note(D, Chance(0.7f) ? "bird_seen" : "bird_tamed", Of(Birds)); break;
				case 1: M.Note(D, "fish_caught", Of(Fish)); break;
				case 2: M.Note(D, "talk", Of(Npcs)); break;
				case 3: M.Note(D, "eat", Of(Food)); break;
				case 4: M.Note(D, "reach", Of(Places)); break;
				case 5: if (Chance(0.15f)) M.Note(D, "festival_visit", Of(Fests)); break;
				case 6: M.Note(D, "film_watched", Of(Films)); break;
				default: { const auto P = Of(Misc); M.Note(D, P.first, P.second); } break;
				}
			}
		}
	}
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "check";
	FModel M;
	std::string Err;
	if (!M.Data.Load(ReadFile(Content + "UI/daylog.json"), Err))
	{
		std::printf("FAIL daylog.json: %s\n", Err.c_str());
		return 1;
	}
	const int First = 20746;   // 2026-10-20, the game's first day
	MakeDays(M, First, 60, 1234);
	const std::vector<FEntry> Entries = M.Write(First + 59, 23);

	if (Mode == "show")
	{
		const std::string Lang = Argc > 2 ? Argv[2] : "ru";
		const int N = Argc > 3 ? std::atoi(Argv[3]) : 10;
		const FTable T = Strings(Lang);
		for (int I = 0; I < (int)Entries.size() && I < N; ++I)
		{
			std::printf("day %d (%s, doodle %s)\n", Entries[I].Day - First + 1, Entries[I].Weather.c_str(), Entries[I].Doodle.c_str());
			for (const FLine& L : Entries[I].Lines)
			{
				bool bFb = false;
				std::printf("   %s\n", Render(L, T, bFb).c_str());
			}
		}
		return 0;
	}

	// 1) every day with anything has an entry, today's only in the evening
	int Played = 0;
	for (const FDay& D : M.Days)
	{
		Played += D.Empty() ? 0 : 1;
	}
	if ((int)Entries.size() != Played)
	{
		Fail("entries " + std::to_string(Entries.size()) + " for " + std::to_string(Played) + " days played");
	}
	if (!M.Write(First + 59, 10).empty() && M.Write(First + 59, 10).back().Day == First + 59)
	{
		Fail("today's entry written in the morning");
	}

	// 2) the words: every key in every language, names resolve or fall back, no sentence two days running
	int Lines = 0, Fallbacks = 0, Named = 0;
	std::map<std::string, int> Topics, Weathers;
	for (const char* Lang : { "ru", "en", "ja" })
	{
		const FTable T = Strings(Lang);
		std::set<std::string> Yesterday;
		int Prev = -10;
		for (const FEntry& E : Entries)
		{
			if (E.Lines.empty() || E.Lines.size() > 7)
			{
				Fail("day " + std::to_string(E.Day - First) + ": " + std::to_string(E.Lines.size()) + " lines");
			}
			std::set<std::string> Today;
			for (const FLine& L : E.Lines)
			{
				if (!T.count(L.Key) || (!L.Fallback.empty() && !T.count(L.Fallback)))
				{
					Fail(std::string(Lang) + ": missing " + L.Key + " / " + L.Fallback);
				}
				bool bFb = false;
				const std::string S = Render(L, T, bFb);
				if (S.find('{') != std::string::npos || S.find('<') == 0)
				{
					Fail(std::string(Lang) + ": unfilled " + S);
				}
				if (E.Day == Prev + 1 && Yesterday.count(S))
				{
					Fail(std::string(Lang) + ": the same sentence two days running: " + S);
				}
				Today.insert(S);
				if (std::string(Lang) == "ru")
				{
					++Lines;
					Fallbacks += bFb ? 1 : 0;
					Named += L.Args.empty() || bFb ? 0 : 1;
					++Topics[L.Key.substr(10, L.Key.find('.', 10) - 10)];
				}
			}
			if (std::string(Lang) == "ru")
			{
				++Weathers[E.Weather];
			}
			Yesterday = Today;
			Prev = E.Day;
		}
	}

	// 3) the same facts, the same words: written twice, and after a save and a load
	{
		std::map<std::string, std::string> Saved;
		M.Save(Saved);
		FModel M2;
		M2.Data = M.Data;
		M2.LoadState(Saved);
		const std::vector<FEntry> Again = M2.Write(First + 59, 23);
		bool bSame = Again.size() == Entries.size();
		for (size_t I = 0; bSame && I < Again.size(); ++I)
		{
			bSame = Again[I].Lines.size() == Entries[I].Lines.size() && Again[I].Doodle == Entries[I].Doodle;
			for (size_t J = 0; bSame && J < Again[I].Lines.size(); ++J)
			{
				bSame = Again[I].Lines[J].Key == Entries[I].Lines[J].Key && Again[I].Lines[J].Args == Entries[I].Lines[J].Args;
			}
		}
		if (!bSame)
		{
			Fail("a save and a load changed the words");
		}
		size_t Bytes = 0;
		for (const auto& KV : Saved)
		{
			Bytes += KV.first.size() + KV.second.size();
		}
		std::printf("save: %zu days, %zu bytes\n", Saved.size(), Bytes);
	}

	// 3b) tonight's events land on their own topics (first match wins, so order in content.py matters)
	{
		const std::vector<std::vector<std::string>> Want = {
			{ "do", "keeper_cat", "lighthouse" }, { "reach", "mayak_station", "lighthouse" }, { "reach", "brook_mouth", "brook" },
			{ "reach", "cable_upper", "cablecar" }, { "do", "kiosk_cablecar", "cablecar" }, { "animal_lap", "Maru", "maru" },
			{ "animal_lap", "Cat", "cat" }, { "animal_sniffed", "Dog", "dog" }, { "animal_friend", "Dog", "animal_friend" },
			{ "procedure", "quartz", "cure" }, { "observe", "jupiter_next", "jupiter" }, { "observe", "moon", "sky" },
			{ "bird_habit", "x", "bird_habit" }, { "cup_won", "x", "cup" }, { "beat", "x", "win" }, { "round_done", "t3", "rounds" },
			{ "winter", "snowball_fight", "snow_play" }, { "winter", "fall", "skate" }, { "winter", "venik", "banya" },
			{ "winter", "walrus_club", "walrus" }, { "fined", "conductor", "fined" }, { "collection", "x", "collection" },
			{ "do", "pu_bakery_cat", "cat" }, { "reach", "beach", "place" }, { "do", "tch_call", "phone_call" }, { "do", "xk1_phone", "phone_call" },
			{ "do", "xk3_weigh", "fortune" }, { "do", "tch_globe", "fortune" } };
		for (const auto& W : Want)
		{
			const FTopic* T = M.Data.Match(W[0], W[1]);
			if (!T || T->Id != W[2])
			{
				Fail(W[0] + " " + W[1] + " -> " + (T ? T->Id : std::string("nothing")) + ", want " + W[2]);
			}
		}
		if (M.Data.Match("do", "mk_catch") || M.Data.Match("do", "pu_fish_catch"))
		{
			Fail("a fish catch counted as a cat");
		}
	}

	// 4) noise never counts; firsts only once; a microscope is not a telescope
	{
		FModel N;
		N.Data = M.Data;
		if (N.Note(1, "walk", "") || N.Note(1, "get", "apple") || N.Note(1, "do", "job_board") || N.Note(1, "do", "school_microscope"))
		{
			Fail("noise counted");
		}
		N.Note(1, "reach", "beach");
		N.Note(2, "reach", "beach");
		N.Sample(1, 12, FSky());
		N.Sample(2, 12, FSky());
		const std::vector<FEntry> E = N.Write(2, 23);
		int PlaceLines = 0;
		for (const FEntry& X : E)
		{
			for (const FLine& L : X.Lines)
			{
				PlaceLines += L.Key.rfind("diary.day.place.", 0) == 0 ? 1 : 0;
			}
		}
		if (PlaceLines != 1)
		{
			Fail("a place found twice: " + std::to_string(PlaceLines) + " lines");
		}
	}

	std::printf("days: %zu entries, %d lines (%d named, %d fell back to plain)\n", Entries.size(), Lines, Named, Fallbacks);
	std::printf("weather:");
	for (const auto& KV : Weathers)
	{
		std::printf(" %s %d", KV.first.c_str(), KV.second);
	}
	std::printf("\nlines by topic:");
	for (const auto& KV : Topics)
	{
		std::printf(" %s %d", KV.first.c_str(), KV.second);
	}
	std::printf("\n%s\n", Failures ? "FAILED" : "OK");
	return Failures ? 1 : 0;
}
