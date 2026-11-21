// The rental's beach set (docs/city_interiors.md "The rental's beach set"; backlog item 35) on the real data: the key board of
// every rental unit hires the set for the day (the stay beach_set, which ULyudBeachHire asks before it lets the hero set
// the deck chair up on the sand). Included by citytest.cpp (its CHECK, ReadFile, AtDate).

inline void RentalChecks(const std::string& Path)
{
	std::string Text, Error;
	if (!ReadFile(Path, Text))
	{
		return;
	}
	std::printf("the rental's beach set (real data)\n");
	FModel M;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	std::vector<int> Boards;
	for (const char* Unit : { "W1-U03", "W1-U15", "W1-U27" })
	{
		const int T = M.FindThing(std::string(Unit) + "/rental_keys");
		if (T >= 0) Boards.push_back(T);
	}
	const FStayDef* Def = M.FindStayDef("beach_set");
	CHECK(Boards.size() == 3 && Def && Def->CurfewAct < 0, "three rental units with a key board; the stay beach_set keeps no curfew (%d boards)", (int)Boards.size());
	if (Boards.size() != 3)
	{
		return;
	}
	FAsk Ask;
	Ask.Money = 100;
	bool bRenterIn = true;
	Ask.StaffOnShift = [&bRenterIn](const std::string&, const std::string& Role) { return Role != "renter" || bRenterIn; };
	auto Day = [](int Since, float Hour) { FWhen W = AtDate((int)(Since + 1) % 7, Hour, 1000 + Since, 0, Since); W.Weather = 0x40; return W; };
	FAsk Poor = Ask;
	Poor.Money = 10;
	CHECK(M.Check(Boards[0], Day(20, 12.f), Poor) == EVerdict::NoMoney && !M.OnStay("beach_set", 20), "ten roubles do not hire it");
	bRenterIn = false;
	const FOutcome Shut = M.Run(Boards[0], Day(20, 12.f), Ask);
	CHECK((Shut.Verdict != EVerdict::Ok || Shut.bClosed) && Shut.Price == 0 && !M.OnStay("beach_set", 20), "nobody hires it out while the attendant is away");
	bRenterIn = true;
	const FOutcome Hire = M.Run(Boards[0], Day(20, 12.f), Ask);
	CHECK(Hire.Verdict == EVerdict::Ok && !Hire.bClosed && Hire.Price == 15 && Hire.Line == "city.rental.hire.1" && M.OnStay("beach_set", 20) && !M.OnStay("beach_set", 21),
		"the key board hires the set for 15 roubles: his today, the attendant's again tomorrow (%s)", Hire.Line.c_str());
	CHECK(M.Check(Boards[0], Day(20, 15.f), Ask) == EVerdict::DailyDone, "once a day at a board");
	CHECK(M.Check(Boards[1], Day(20, 16.f), Ask) == EVerdict::DailyDone && !M.OnStay("beach_set", 21), "nor at another unit's board the same day: one set a day");
	const bool bGapDay = !M.OnStay("beach_set", 22);
	const FOutcome Next = M.Run(Boards[2], Day(23, 10.f), Ask);
	CHECK(bGapDay && Next.Verdict == EVerdict::Ok && M.OnStay("beach_set", 23) && !M.OnStay("beach_set", 24), "a day skipped: no set that day, hired anew after it");
	std::map<std::string, std::string> Saved;
	M.SaveState(Saved);
	FModel M2;
	M2.Load(Text, Error);
	M2.LoadState(Saved);
	CHECK(Saved.count("s:beach_set") && M2.OnStay("beach_set", 23) && !M2.OnStay("beach_set", 24), "the hire is in the save");
	const std::set<std::string> Ru = RewardTest::KeysOf("unreal/Lyudmilovsk/Content/Localization/city/strings_ru.json");
	const std::set<std::string> En = RewardTest::KeysOf("unreal/Lyudmilovsk/Content/Localization/city/strings_en.json");
	const std::set<std::string> Ja = RewardTest::KeysOf("unreal/Lyudmilovsk/Content/Localization/city/strings_ja.json");
	int Missing = 0;
	for (const char* K : { "city.rental.hire.prompt", "city.rental.hire.1", "city.rental.hire.2", "city.rental.set.prompt", "city.rental.sit.prompt",
		"city.rental.move.prompt", "city.rental.taken_back", "city.rental.sunbath.toast", "city.rental.sunbath.diary", "dev.action.interiors.beach_set" })
	{
		Missing += Ru.count(K) && En.count(K) && Ja.count(K) ? 0 : 1;
	}
	CHECK(Missing == 0, "the hire's, the sand's and the sunbath's words are in the three tables (%d missing)", Missing);
}
