// A complete collection pays (docs/city_interiors.md "A complete set pays"; backlog item 30) on the real data: every set has a
// reward with its words in three languages, pays once at its last stamp and never before, holds something the game can give,
// and the diary's page lists every set with its slots. Included by citytest.cpp (its CHECK, ReadFile).

namespace RewardTest
{
	inline std::set<std::string> KeysOf(const std::string& Path)
	{
		std::set<std::string> Keys;
		std::string Text;
		if (ReadFile(Path, Text))
		{
			LyudJson::FValue Root;
			std::string E2;
			if (LyudJson::Parse(Text, Root, E2))
			{
				for (const auto& F : Root.Fields) Keys.insert(F.first);
			}
		}
		return Keys;
	}
	/** Every stamp of Set but its last piece; then the last one. */
	inline void GiveAllBut(FModel& M, const FCollection& C, size_t Left)
	{
		for (size_t I = 0; I + Left < C.Pieces.size(); ++I)
		{
			M.GiveStamp(C.Id + "/" + C.Pieces[I]);
		}
	}
}

inline void RewardChecks(const std::string& Path)
{
	using namespace RewardTest;
	std::string Text, Error;
	if (!ReadFile(Path, Text))
	{
		std::printf("rewards: %s is not built (tools/city/interiors.py), skipped\n", Path.c_str());
		return;
	}
	std::printf("a complete collection pays (%s)\n", Path.c_str());
	FModel M;
	CHECK(M.Load(Text, Error), "the interiors load (%s)", Error.c_str());
	const std::set<std::string> Ru = KeysOf("unreal/Lyudmilovsk/Content/Localization/city/strings_ru.json");
	const std::set<std::string> En = KeysOf("unreal/Lyudmilovsk/Content/Localization/city/strings_en.json");
	const std::set<std::string> Ja = KeysOf("unreal/Lyudmilovsk/Content/Localization/city/strings_ja.json");
	std::set<std::string> Sold;
	for (const auto& Shop : M.Shops)
	{
		for (const FGood& G : Shop.second) Sold.insert(G.Item);
	}
	// 1. every set: a reward, its words, something to hold
	int NoReward = 0, NoWords = 0, BadHeld = 0, Own = 0;
	std::string First;
	std::map<std::string, int> ByKind;
	for (const FCollection& C : M.Collections)
	{
		const FReward& R = C.Reward;
		auto Bad = [&First, &C](int& N) { ++N; if (First.empty()) First = C.Id; };
		if (!R.Any()) { Bad(NoReward); continue; }
		for (const std::string& K : { R.Name, R.Note })
		{
			if (!Ru.count(K) || !En.count(K) || !Ja.count(K)) Bad(NoWords);
		}
		// (a set a branch brought without a reward of its own pays under the default words: interiors.py warns)
		Own += R.Name == "city.coll.reward." + C.Id + ".name" || R.Name == "city.coll.reward.default.name" ? 1 : 0;
		const int Held = (R.Item.empty() ? 0 : 1) + (R.ItemFrom.empty() ? 0 : 1) + (R.Free.empty() ? 0 : 1);
		const bool bShop = R.ItemFrom.empty() || (M.FindShop(R.ItemFrom) && !M.RewardChoices(R).empty() && R.Pay > 0);
		const bool bFree = R.Free.empty() || (R.Days > 0 && Sold.count(R.Free));
		if (Held > 1 || (Held == 0 && R.Pay <= 0) || !bShop || !bFree || (!R.Item.empty() && R.Count < 1)) Bad(BadHeld);
		++ByKind[!R.Free.empty() ? "free" : !R.ItemFrom.empty() ? "item_from" : !R.Item.empty() ? "item" : "pay"];
	}
	CHECK(M.Collections.size() >= 39 && NoReward == 0, "every collection has a reward (%d sets, %d without: %s)", (int)M.Collections.size(), NoReward, First.c_str());
	CHECK(NoWords == 0 && Own == (int)M.Collections.size(), "every reward is the set's own (or the default), named and told in ru, en, ja (%d words missing, %d own: %s)", NoWords, Own, First.c_str());
	CHECK(BadHeld == 0, "every reward holds one thing the game can give: %d an item, %d roubles, %d a shop's good, %d a free good (%d bad: %s)",
		ByKind["item"], ByKind["pay"], ByKind["item_from"], ByKind["free"], BadHeld, First.c_str());
	for (const char* K : { "city.coll.reward.toast", "city.coll.reward.diary", "city.coll.page.count", "city.coll.page.reward", "city.coll.page.promise",
		"city.coll.page.free", "city.coll.page.sum", "city.coll.page.empty", "city.coll.reward.default.name", "city.coll.reward.default.note" })
	{
		if (!Ru.count(K) || !En.count(K) || !Ja.count(K)) { ++NoWords; First = K; }
	}
	CHECK(NoWords == 0, "the toast's, the diary's and the page's words are in the tables (%s)", First.c_str());

	// 2. each set in turn: nothing one stamp short, paid at the last stamp, once
	int Early = 0, NotPaid = 0, Twice = 0, Others = 0;
	First.clear();
	long long Day = 3;
	for (const FCollection& C : M.Collections)
	{
		GiveAllBut(M, C, 1);
		if (!M.TakeRewards(Day).empty() || M.IsComplete(C.Id)) { ++Early; First = C.Id; }
		std::vector<std::string> New;
		M.GiveStamp(C.Id + "/" + C.Pieces.back(), &New);
		const std::vector<const FCollection*> Paid = M.TakeRewards(Day);
		if (New.size() != 1 || Paid.size() != 1 || !M.IsRewarded(C.Id)) { ++NotPaid; First = C.Id; }
		else if (Paid[0] != &C) { ++Others; First = C.Id; }
		if (!M.TakeRewards(Day + 1).empty() || M.GiveStamp(C.Id + "/" + C.Pieces.back())) { ++Twice; First = C.Id; }
		++Day;
	}
	CHECK(Early == 0 && NotPaid == 0 && Others == 0 && Twice == 0,
		"each of the %d sets pays at its last stamp, itself, once (%d early, %d unpaid, %d another's, %d twice: %s)", (int)M.Collections.size(), Early, NotPaid, Others, Twice, First.c_str());

	// 3. the rewards the brief names
	auto Reward = [&M](const char* Set) -> const FReward& { static const FReward None; const FCollection* C = M.FindCollection(Set); return C ? C->Reward : None; };
	CHECK(Reward("shelves").Name == "city.coll.reward.shelves.name" && !Reward("shelves").Item.empty(), "the library's shelves: the reader's card with a photograph, and %s", Reward("shelves").Item.c_str());
	CHECK(Reward("films").Any() && !Reward("films").Item.empty(), "the Rassvet's films: the framed poster, and %s x%d", Reward("films").Item.c_str(), Reward("films").Count);
	CHECK(Reward("chess_wins").Item == "badge", "the chess players beaten: the club's badge on the jacket");
	CHECK(Reward("san_course").Pay > 0 && Reward("san_cure").Any(), "the sanatorium's book and card: the intake's photograph with %d roubles, the card marked", Reward("san_course").Pay);
	const FReward& Rec = Reward("records");
	const std::vector<std::string> Choices = M.RewardChoices(Rec);
	bool bRecords = !Choices.empty();
	for (const std::string& Ch : Choices) bRecords = bRecords && Ch.rfind("record_", 0) == 0;
	CHECK(bRecords && Choices.size() >= 10 && Rec.Like == "record_" && Rec.Pay > 0, "the jukebox's records: a record of the Wave's %d he has none of, never its guitar (%d roubles when he owns them all)", (int)Choices.size(), Rec.Pay);
	// the kiosks' free paper for a month
	const FCollection* Kiosks = M.FindCollection("kiosks");
	const long long PaidOn = 3 + (Kiosks ? (long long)(Kiosks - M.Collections.data()) : 0);
	const std::vector<FGood>* News = M.FindShop("kiosk_news");
	const FGood* Paper = nullptr;
	const FGood* Other = nullptr;
	if (News)
	{
		for (const FGood& G : *News)
		{
			if (G.Item == "daily_paper") Paper = &G; else if (!Other) Other = &G;
		}
	}
	CHECK(Kiosks && Kiosks->Reward.Free == "daily_paper" && Kiosks->Reward.Days == 30 && Paper && Other && Paper->Price > 0
		&& M.PriceOn(*Paper, PaidOn) == 0 && M.PriceOn(*Paper, PaidOn + 29) == 0 && M.PriceOn(*Paper, PaidOn + 30) == Paper->Price
		&& M.PriceOn(*Other, PaidOn) == Other->Price && M.FreeUntil("daily_paper", PaidOn + 10) == PaidOn + 30 && M.FreeUntil("daily_paper", PaidOn + 30) == -1,
		"the kiosks: the paper costs nothing for thirty days from the day it paid, then its price again; nothing else is free");

	// 4. the save keeps what was paid; a set cleared pays again; a new game forgets; an old save pays at its first look
	std::map<std::string, std::string> Saved;
	M.SaveState(Saved);
	FModel M2;
	M2.Load(Text, Error);
	M2.LoadState(Saved);
	CHECK(Saved.count("g:kiosks") && M2.IsRewarded("kiosks") && M2.TakeRewards(100).empty() && Paper && M2.PriceOn(*Paper, PaidOn + 5) == 0,
		"a save and a load: every set still paid, nothing paid again, the paper still free");
	M2.ClearSet("films");
	const bool bCleared = !M2.IsRewarded("films") && M2.TakeRewards(100).empty();
	M2.GrantSet("films");
	const std::vector<const FCollection*> Again = M2.TakeRewards(101);
	CHECK(bCleared && Again.size() == 1 && Again[0]->Id == "films", "developer mode: a set cleared and granted again pays again");
	std::map<std::string, std::string> Old;
	for (const auto& P : Saved)
	{
		if (P.first.rfind("g:", 0) != 0) Old[P.first] = P.second;
	}
	FModel M3;
	M3.Load(Text, Error);
	M3.LoadState(Old);
	CHECK(M3.TakeRewards(200).size() == M.Collections.size() && M3.TakeRewards(200).empty(), "a save from before the rewards: its complete sets pay at the first look");
	M3.ResetState();
	CHECK(!M3.IsRewarded("kiosks") && M3.TakeRewards(0).empty(), "a new game forgets them");

	// 5. the diary's page: every set, its slots in order, the paid first, then the begun (the fullest first), then the rest
	FModel M4;
	M4.Load(Text, Error);
	const FCollection* A = M4.FindCollection("kiosks");
	const FCollection* B = M4.FindCollection("films");
	const FCollection* Cc = M4.FindCollection("cups");
	if (A && B && Cc)
	{
		GiveAllBut(M4, *A, 0);
		M4.TakeRewards(7);
		M4.GiveStamp("films/" + B->Pieces[1]);
		GiveAllBut(M4, *Cc, 1);
	}
	const std::vector<FCollectionRow> Rows = M4.CollectionRows();
	int Slots = 0, Pieces = 0;
	for (const FCollectionRow& R : Rows) Slots += (int)R.Slots.size();
	for (const FCollection& C : M4.Collections) Pieces += (int)C.Pieces.size();
	const bool bOrder = Rows.size() == M4.Collections.size() && Rows.size() > 3 && Rows[0].Of == A && Rows[0].bRewarded && Rows[0].Day == 7 && Rows[0].Got == (int)A->Pieces.size()
		&& Rows[1].Of == Cc && !Rows[1].bRewarded && Rows[2].Of == B && Rows[2].Got == 1 && Rows[2].Slots.size() == B->Pieces.size() && !Rows[2].Slots[0] && Rows[2].Slots[1]
		&& Rows[3].Got == 0;
	CHECK(bOrder && Slots == Pieces, "the diary's page: %d sets, %d slots; the paid set, then the fuller of the begun, then the untouched", (int)Rows.size(), Slots);
}
