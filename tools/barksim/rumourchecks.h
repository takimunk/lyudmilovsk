// The talk of the town about the hero (Private/People/TownRumours.h, docs/town_rumours.md) on the town's real people:
// the rules, the ties, how a rumour spreads (the same day near, a day later across town), how it dies, and that nobody
// remarks twice in a day or twice on one rumour. Included by barksim.cpp (its CHECK, Failures, ReadFile, LoadTable).
#pragma once

#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/TownRumours.h"

static void RumourChecks(const FWorld& W, const FBook& Book, bool bStory)
{
	std::printf("rumours (docs/town_rumours.md):\n");
	LyudRumour::FTown Town;
	const bool bLoaded = Town.Load(ReadFile(std::string(DataDir) + "/rumours.json"));
	CHECK(bLoaded, "rumours.json reads (%zu kinds, %zu rules) %s", Town.Kinds.size(), Town.Rules.size(), Town.LastError.c_str());
	if (!bLoaded)
	{
		return;
	}

	// --- every string a rumour can show exists in the three languages
	std::map<std::string, std::string> All[3];
	const char* Langs[3] = { "ru", "en", "ja" };
	for (int L = 0; L < 3; L++)
	{
		for (const char* Sub : { "", "/rumours", "/city", "/town", "/instruments", "/barks", "/paper" })
		{
			LoadTable(std::string(LocDir) + Sub + "/strings_" + Langs[L] + ".json", All[L]);
		}
	}
	int Kind = -1;
	std::string Detail;
	struct FCase { const char* Event; const char* Target; const char* Kind; const char* Detail; };
	const FCase Cases[] = {
		{ "stamp", "work_book/kiosk", "job", "rumour.job.kiosk" },
		{ "stamp", "kites/first_flight", "kite", "" },
		{ "play_tune", "bonfire", "sing", "" },
		{ "play_tune", "fur_elise", "tune", "music.tune.fur_elise" },
		{ "beat", "npc.t_sorokin_gennady.name", "beat", "npc.t_sorokin_gennady.name" },
		{ "beat", "city.dnc.p.lan", "dance", "city.dnc.p.lan" },     // a dance at the Vaults is not a game won (docs/dances.md)
		{ "cup_won", "cog_chess", "cup", "city.coll.cups.cog_chess" },
		{ "bird_tamed", "herring_gull", "tamed", "bird.herring_gull.name" },
		{ "fish_caught", "fish_perch", "catch", "item.fish_perch.name" },
		{ "stamp_set", "san_course", "cure", "" },
		{ "radio", "request_heard", "radio", "" },
	};
	int Wrong = 0, NoWords = 0;
	for (const FCase& C : Cases)
	{
		const bool bOk = Town.Match(C.Event, C.Target, Kind, Detail) && Town.Kinds[(size_t)Kind].Id == C.Kind && Detail == C.Detail;
		if (!bOk)
		{
			std::printf("  %s %s -> %s %s\n", C.Event, C.Target, Kind >= 0 ? Town.Kinds[(size_t)Kind].Id.c_str() : "-", Detail.c_str());
		}
		Wrong += bOk ? 0 : 1;
		for (int L = 0; L < 3 && *C.Detail; L++)
		{
			NoWords += All[L].count(C.Detail) ? 0 : 1;
		}
	}
	const bool bNotNews = !Town.Match("stamp", "chess_wins/sorokin", Kind, Detail) && !Town.Match("radio", "on", Kind, Detail)
		&& !Town.Match("beat", "", Kind, Detail);
	CHECK(Wrong == 0 && bNotNews, "game events become the right rumours, and everyday ones none (%d wrong)", Wrong);
	CHECK(NoWords == 0, "their details have words in ru, en and ja (%d missing)", NoWords);

	// --- the townsfolk's remarks: every kind has a greeting; {1} only where the kind has a detail
	std::set<std::string> Greeted, WithDetail;
	for (const LyudRumour::FRule& R : Town.Rules)
	{
		if (!R.Detail.empty())
		{
			WithDetail.insert(R.Kind);
		}
	}
	int BadSlot = 0;
	for (const FSet& S : Book.Sets)
	{
		for (const std::string& K : S.A.Heard.Yes)
		{
			if (Town.FindKind(K) < 0)
			{
				std::printf("  [%s]: heard_of = %s is no kind\n", S.Name.c_str(), K.c_str());
				BadSlot++;
			}
			if (S.Kind == EKind::Greet)
			{
				Greeted.insert(K);
			}
		}
		std::vector<std::string> Lines = S.Lines;
		for (const std::vector<std::string>& T : S.Talks)
		{
			Lines.insert(Lines.end(), T.begin(), T.end());
		}
		for (const std::string& Key : Lines)
		{
			const auto It = All[0].find("line." + Key);
			if (It == All[0].end() || It->second.find("{1}") == std::string::npos)
			{
				continue;
			}
			bool bOk = !S.A.Heard.Yes.empty();
			for (const std::string& K : S.A.Heard.Yes)
			{
				bOk = bOk && WithDetail.count(K) > 0;
			}
			if (!bOk)
			{
				std::printf("  %s: {1} but [%s] can be about a rumour without a detail\n", Key.c_str(), S.Name.c_str());
				BadSlot++;
			}
		}
	}
	CHECK(BadSlot == 0, "remark sets name real kinds and use {1} only for kinds with a detail (%d wrong)", BadSlot);
	CHECK(Greeted.size() == Town.Kinds.size(), "every kind has a greeting about it (%zu of %zu)", Greeted.size(), Town.Kinds.size());

	// --- the ties
	LyudRumour::FTies Ties;
	LyudRumour::BuildTies(W, Ties, Town.FarM, Town.BigGroup, Town.GroupLinks);
	int Tied = 0;
	size_t TieN = 0;
	for (const auto& T : Ties.Of)
	{
		Tied += T.empty() ? 0 : 1;
		TieN += T.size();
	}
	const int N = Ties.Num();
	CHECK(N > 0 && Tied >= N * 9 / 10 && TieN >= (size_t)N * 3, "nearly everyone has ties to pass things on (%d of %d, %.1f each)", Tied, N,
		N ? (double)TieN / N : 0.0);

	// --- a regular beaten at the club on day 10 at 15:00: whoever was close by (a few townsfolk living in one corner of town)
	auto HomeOf = [&W](int Id, V3& At)
	{
		const FPerson& P = W.People[(size_t)Id];
		const int Pl = P.Home >= 0 ? P.Home : W.FindPlace(P.HomeId);
		if (Pl < 0)
		{
			return false;
		}
		At = W.Places[(size_t)Pl].Portal;
		return true;
	};
	std::vector<int> Witnesses;
	V3 Corner;
	for (const FPerson& P : W.People)
	{
		V3 H;
		if (P.bTown && P.Age >= 18 && HomeOf(P.Id, H))
		{
			if (Witnesses.empty())
			{
				Corner = H;
			}
			if (std::hypot(H.X - Corner.X, H.Y - Corner.Y) < 60.f)
			{
				Witnesses.push_back(P.Id);
			}
			if (Witnesses.size() >= 6)
			{
				break;
			}
		}
	}
	const int Day0 = 10;
	const int Beat = Town.FindKind("beat");
	const int Ri = Town.Add(Beat, "npc.t_sorokin_gennady.name", Day0, 15 * 60, Witnesses, Ties);
	CHECK(Ri >= 0, "a rumour is added (%zu witnesses)", Witnesses.size());
	if (Ri < 0)
	{
		return;
	}
	const LyudRumour::FKind& BK = Town.Kinds[(size_t)Beat];
	std::vector<int> Know((size_t)BK.Life + 1, 0);
	for (int D = 0; D <= BK.Life; ++D)
	{
		Know[(size_t)D] = Town.CountKnowing(Town.Rumours[(size_t)Ri], Day0 + D);
	}
	CHECK(Know[0] >= (int)Witnesses.size() && Know[1] > Know[0] && Know[(size_t)BK.Life] == 0,
		"it spreads, then dies: knowing it day 0 %d, day 1 %d, day 2 %d, day %d %d", Know[0], Know[1], Know[2], BK.Life, Know[(size_t)BK.Life]);
	CHECK(Know[1] < N / 3, "but it stays the talk of some, not the whole town (%d of %d on day 1)", Know[1], N);
	// near the witnesses it is heard the same day, across town mostly a day later
	int NearSame = 0, NearAll = 0, FarSame = 0, FarAll = 0;
	const LyudRumour::FRumour& R = Town.Rumours[(size_t)Ri];
	for (int P = 0; P < N; ++P)
	{
		V3 H;
		if (R.HeardOn[(size_t)P] < 0 || !HomeOf(P, H) || std::find(Witnesses.begin(), Witnesses.end(), P) != Witnesses.end())
		{
			continue;
		}
		const bool bFar = std::hypot(H.X - Corner.X, H.Y - Corner.Y) > 2.f * Town.FarM;
		(bFar ? FarAll : NearAll)++;
		(bFar ? FarSame : NearSame) += R.HeardOn[(size_t)P] == 0 ? 1 : 0;
	}
	CHECK(FarAll > 0 && NearAll > 0 && (double)FarSame / FarAll < (double)NearSame / NearAll,
		"near it is heard the same day more than across town (same day: near %d of %d, far %d of %d)", NearSame, NearAll, FarSame, FarAll);

	// --- a cup: the paper prints it, so much of the town hears it the next day
	const int Cup = Town.FindKind("cup");
	const int Ci = Town.Add(Cup, "city.coll.cups.cog_chess", Day0, 17 * 60, Witnesses, Ties);
	const int CupDay1 = Ci >= 0 ? Town.CountKnowing(Town.Rumours[(size_t)Ci], Day0 + 1) : 0;
	CHECK(Ci >= 0 && CupDay1 > Know[1] && CupDay1 > N / 5, "a cup goes further (the paper): %d know it the next day", CupDay1);

	// --- the same news twice: no fresh start while it goes round; a bird tamed is news only once ever
	const int Again = Town.Add(Beat, "npc.t_sorokin_gennady.name", Day0 + 1, 10 * 60, Witnesses, Ties);
	const int Tamed = Town.FindKind("tamed");
	const int T1 = Town.Add(Tamed, "bird.herring_gull.name", Day0, 9 * 60, Witnesses, Ties);
	const int T2 = Town.Add(Tamed, "bird.herring_gull.name", Day0 + 20, 9 * 60, Witnesses, Ties);
	CHECK(Again < 0 && T1 >= 0 && T2 < 0, "the same news doesn't start again while it goes round, a tamed bird is news once ever");
	Town.Expire(Day0 + 20);
	CHECK(Town.Rumours.empty(), "old rumours are dropped (%zu left on day %d)", Town.Rumours.size(), Day0 + 20);

	// --- a week of remarks: everyone who would bring a rumour up does (the game is far calmer: only those near the player)
	Town.Rumours.clear();
	Town.Spent.clear();
	Town.Add(Beat, "npc.t_sorokin_gennady.name", Day0, 15 * 60, Witnesses, Ties);
	Town.Add(Cup, "city.coll.cups.cog_chess", Day0, 17 * 60, Witnesses, Ties);
	Town.Add(Town.FindKind("job"), "rumour.job.kiosk", Day0 + 1, 11 * 60, std::vector<int>(Witnesses.begin(), Witnesses.begin() + Witnesses.size() / 2), Ties);
	std::map<std::pair<int, int>, int> Said;     // (person, rumour) -> times
	std::vector<int> PerDay;
	int Twice = 0, TwoADay = 0;
	std::vector<int> Can;
	for (int D = Day0; D < Day0 + 7; ++D)
	{
		int Today = 0;
		for (int P = 0; P < N; ++P)
		{
			int Mine = 0;
			for (int Try = 0; Try < 3; ++Try)    // asked three times a day: still one remark at most
			{
				Town.Mentionable(P, D, Can);
				if (Can.empty())
				{
					break;
				}
				Twice += Said[{ P, Can[0] }]++ > 0 ? 1 : 0;
				Town.MarkTold(Can[0], P, D);
				Mine++;
			}
			TwoADay += Mine > 1 ? 1 : 0;
			Today += Mine;
		}
		PerDay.push_back(Today);
	}
	CHECK(Twice == 0 && TwoADay == 0, "nobody remarks twice on a rumour or twice a day (%d, %d)", Twice, TwoADay);
	CHECK(PerDay[1] > 0 && PerDay[6] == 0 && PerDay[4] < PerDay[1],
		"remarks rise and fade over the week: %d %d %d %d %d %d %d", PerDay[0], PerDay[1], PerDay[2], PerDay[3], PerDay[4], PerDay[5], PerDay[6]);

	// --- the save: the same spread and the same remarks after a load
	Town.Rumours.clear();
	Town.Add(Beat, "npc.t_sorokin_gennady.name", Day0, 15 * 60, Witnesses, Ties);
	Town.MarkTold(0, Witnesses[0], Day0);
	const std::vector<std::string> Saved = Town.Save(W);
	LyudRumour::FTown Loaded;
	Loaded.Load(ReadFile(std::string(DataDir) + "/rumours.json"));
	Loaded.Load(Saved, W, Ties);
	const bool bSame = Loaded.Rumours.size() == Town.Rumours.size() && !Loaded.Rumours.empty()
		&& Loaded.Rumours[0].HeardOn == Town.Rumours[0].HeardOn && Loaded.Rumours[0].Told == Town.Rumours[0].Told
		&& Loaded.Spent == Town.Spent;
	CHECK(bSame, "a save and a load give the same rumours, spread and remarks (%zu lines)", Saved.size());

	// --- the paper's column and the letters: the best of the talk first
	std::vector<int> Talk;
	Town.Rumours.clear();
	Town.Add(Town.FindKind("film"), "", Day0, 20 * 60, Witnesses, Ties);
	Town.Add(Beat, "npc.t_sorokin_gennady.name", Day0, 15 * 60, Witnesses, Ties);
	Town.Talk(Day0 + 1, Talk);
	CHECK(Talk.size() == 2 && Town.Rumours[(size_t)Talk[0]].Kind == Beat, "the talk of the town puts the bigger news first");
	Town.Talk(Day0 + 3, Talk);
	CHECK(Talk.empty(), "and forgets it after a few days (%zu left on day 3)", Talk.size());

	if (bStory)
	{
		Town.Rumours.clear();
		Town.Add(Beat, "npc.t_sorokin_gennady.name", Day0, 15 * 60, Witnesses, Ties);
		const LyudRumour::FRumour& S = Town.Rumours[0];
		std::map<std::string, std::string> Names;
		LoadTable(std::string(LocDir) + "/town/strings_ru.json", Names);
		for (int D = 0; D < 3; ++D)
		{
			std::printf("  day %d:", D);
			int Shown = 0;
			for (int P = 0; P < N && Shown < 12; ++P)
			{
				if (S.HeardOn[(size_t)P] == D)
				{
					const auto Nm = Names.find("npc." + W.People[(size_t)P].Key + ".name");
					std::printf(" %s;", Nm != Names.end() ? Nm->second.c_str() : W.People[(size_t)P].Key.c_str());
					Shown++;
				}
			}
			std::printf(" (%d in all)\n", Town.CountKnowing(S, Day0 + D) - (D ? Town.CountKnowing(S, Day0 + D - 1) : 0));
		}
	}
}
