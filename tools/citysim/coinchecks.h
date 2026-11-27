// Coins, scales and strips (docs/city_interiors.md "Coins, scales and strips") on the real data: the payphones dial a number
// picked (the numbers torn off the bus-stop notices join the dial), the bus-stop weighing machine and the fair's globe give the
// fortune cards, the clinic's scales say the weight, the drinking fountains are held to the right height, the staff doors
// answer a knock, the lobby's magazine is taken. These scenes are no cure: they never count as procedures. Included by
// citytest.cpp (its CHECK, ReadFile, AtDate, PlayProc).

inline void CoinChecks(const std::string& Path)
{
	std::string Text, Error, Ru;
	if (!ReadFile(Path, Text))
	{
		return;
	}
	std::printf("coins, scales and strips (real data)\n");
	FModel M;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	ReadFile("unreal/Lyudmilovsk/Content/Localization/city/strings_ru.json", Ru);
	auto Has = [&Ru](const std::string& Key) { return Ru.find("\"" + Key + "\"") != std::string::npos; };
	const int Phone = M.FindThing("W1-U12/payphone"), Kiosk1Phone = M.FindThing("P-KIOSK1/k1_payphone"), Notices = M.FindThing("P-KIOSK2/stop_notices"),
		Scale = M.FindThing("P-KIOSK3/k3_scales"), Globe = M.FindThing("P-FAIR/fortune_globe"), Clinic = M.FindThing("P-CLINIC/patient_0"),
		Fountain = M.FindThing("P-KIOSK2/k2_fountain"), Cooler = M.FindThing("P-CLINIC/water_cooler"), LibDoor = M.FindThing("P-LIBRARY/office_door"),
		StoreDoor = M.FindThing("P-STORE/g_staff_door"), Magazine = M.FindThing("P-CAPSULE/lobby_magazine"), Arcade = M.FindThing("P-CLUB3/arcade_1");
	if (Phone < 0 || Kiosk1Phone < 0 || Notices < 0 || Scale < 0 || Globe < 0 || Clinic < 0 || Fountain < 0 || Cooler < 0 || LibDoor < 0 || StoreDoor < 0
		|| Magazine < 0 || Arcade < 0)
	{
		CHECK(false, "the town's machines, notices and staff doors are there");
		return;
	}
	FAsk Ask;
	Ask.Money = 1000;
	Ask.StaffOnShift = [](const std::string&, const std::string&) { return true; };
	auto Day = [](int Since, float Hour) { FWhen W = AtDate((int)(Since - 10) % 7, Hour, 1000 + Since, 0, Since); W.Weather = WeatherClear; return W; };
	const std::vector<FProcCompanion> Nobody;
	auto Saw = [](const std::vector<std::string>& Seen, const std::string& Key) { return std::find(Seen.begin(), Seen.end(), Key) != Seen.end(); };
	const int ProcsBefore = M.ProcCount();

	// the payphone: no strips yet, the second number on the dial is the enquiries; the kiosk's own phone plays the same scene
	CHECK(M.ProcNow(Phone, Day(10, 12.f), Ask) >= 0 && M.ProcNow(Kiosk1Phone, Day(10, 12.f), Ask) >= 0, "the payphones dial as a scene (the kiosk's too)");
	std::vector<std::string> Seen;
	const FProcEnd Call = PlayProc(M, Phone, Day(10, 12.f), Ask, EProcPlay::Good, Nobody, &Seen);
	CHECK(Call.Result == 0 && Saw(Seen, "city.tch.call.ans.info"), "no number torn off yet: the second on the dial is 09, the enquiries");
	CHECK(M.HasStamp("coin_slots/phone") && Call.Run.Verdict == EVerdict::Ok && Call.Run.Price == 1, "the call runs (1 rouble) and stamps coin_slots/phone");
	// the speaking clock reads the game's time
	CHECK(Has("city.tch.call.ans.clock") && Ru.find("{time}") != std::string::npos
		&& FillTime("— {time}.", 14.5f) == "— 14:30.", "the speaking clock's words carry {time}: %s", FillTime("{time}", 14.5f).c_str());
	// a strip torn off the bus-stop notice: its number joins the dial (and comes second, after the clock)
	const FOutcome Read = M.Run(Notices, Day(10, 12.f), Ask);
	CHECK(Read.Line == "city.tch.strip.puppy" && M.HasStamp("strips/puppy"), "the first look at the stop's notices tears off a number (%s)", Read.Line.c_str());
	const FOutcome Read2 = M.Run(Notices, Day(11, 12.f), Ask);
	CHECK(Read2.Line == "city.tch.strip.cat" && M.HasStamp("strips/cat"), "the next day another notice, another strip (%s)", Read2.Line.c_str());
	Seen.clear();
	PlayProc(M, Phone, Day(11, 13.f), Ask, EProcPlay::Good, Nobody, &Seen);
	CHECK(Saw(Seen, "city.tch.call.ans.puppy"), "with the puppy's number: the second on the dial is the puppies");
	// the strips are once a board: after four days the notices go back to their own run
	M.Run(Notices, Day(12, 12.f), Ask);
	M.Run(Notices, Day(13, 12.f), Ask);
	const FOutcome Read5 = M.Run(Notices, Day(14, 12.f), Ask);
	CHECK(M.CountStamps("strips") == 4 && Read5.Line.rfind("city.tch.strip.", 0) != 0, "four strips, then the board's own notices (%s)", Read5.Line.c_str());

	// the weighing machine at bus stop 3: the weight, then a fortune card; eight goes, eight cards
	Seen.clear();
	const FProcEnd Weigh = PlayProc(M, Scale, Day(10, 12.f), Ask, EProcPlay::Good, Nobody, &Seen);
	CHECK(Weigh.Result == 0 && Weigh.Line.rfind("city.tch.fortune.", 0) == 0 && Saw(Seen, "city.tch.fscale.weight") && M.HasStamp("coin_slots/scale"),
		"the weighing machine: the needle, then a card (%s)", Weigh.Line.c_str());
	for (int D = 11; D < 18; ++D)
	{
		PlayProc(M, Scale, Day(D, 12.f), Ask, EProcPlay::Good, Nobody);
	}
	CHECK(M.CountStamps("fortunes") == 8, "eight weighings, all eight cards (%d)", M.CountStamps("fortunes"));
	const FOutcome Spin = M.Run(Globe, Day(15, 15.f), Ask);
	CHECK(Spin.Line.rfind("city.tch.fortune.", 0) == 0 && M.HasStamp("coin_slots/globe"), "the fair's globe gives the same cards (%s)", Spin.Line.c_str());

	// the clinic's scales: the nurse by day, the weight in the line; the quest's "do scales" still counts
	Seen.clear();
	const FProcEnd Med = PlayProc(M, Clinic, Day(11, 10.f), Ask, EProcPlay::Good, Nobody, &Seen);
	CHECK(Med.Result == 0 && Saw(Seen, "city.tch.mscales.weight.nurse") && Med.Run.Target == "scales", "the clinic's scales: the nurse writes the weight; target %s",
		Med.Run.Target.c_str());

	// the fountains: held to the mark is a proper drink, held on is a jet in the nose; frozen in the cold
	const FProcEnd Drink = PlayProc(M, Fountain, Day(10, 12.f), Ask, EProcPlay::Good, Nobody);
	const FProcEnd Splash = PlayProc(M, Fountain, Day(10, 12.5f), Ask, EProcPlay::Poor, Nobody);
	FWhen Cold = Day(10, 12.f);
	Cold.Weather = WeatherCold;
	CHECK(Drink.Result == 0 && Splash.Result == 2 && M.ProcNow(Fountain, Cold, Ask) < 0, "the fountain: a good drink, a splash, off in the frost (%d %d)", Drink.Result,
		Splash.Result);
	const FProcEnd Cone = PlayProc(M, Cooler, Day(11, 10.f), Ask, EProcPlay::Good, Nobody);
	CHECK(Cone.Result == 0 && Cone.Line == "city.tch.cooler.good", "the clinic's cooler: a paper cone filled just right");

	CHECK(M.ProcCount() == ProcsBefore, "the town's scenes are no cure: procedures still %d", M.ProcCount());

	// the staff doors answer a knock by the hour; the library's closed Monday has its own
	const FOutcome Knock = M.Run(LibDoor, Day(11, 15.f), Ask), Monday = M.Run(LibDoor, Day(10, 12.f), Ask), Night = M.Run(StoreDoor, Day(11, 23.f), Ask);
	CHECK(Knock.Line.rfind("city.tch.lib.", 0) == 0 && Monday.Line.find("city.tch.lib.shut.") != std::string::npos && Night.Line.rfind("city.tch.store.", 0) == 0,
		"the staff doors answer (%s, %s, %s)", Knock.Line.c_str(), Monday.Line.c_str(), Night.Line.c_str());
	// the lobby's magazine is really taken, once a day
	const FOutcome Mag = M.Run(Magazine, Day(11, 9.f), Ask);
	CHECK(Mag.Give.size() == 1 && Mag.Give[0].first == "magazine_travel", "the lobby's magazine goes in the pocket");
	// «Морской бой»: a game of its own, the stamp on a win
	const FAct* Sea = M.ActOf(Arcade);
	CHECK(Sea && Sea->Game >= 0 && M.Games[(size_t)Sea->Game].Collect == "coin_slots/seabattle", "the Cog club's arcade is «Морской бой»");
	// every word of the scenes in the table
	int Missing = 0;
	for (const char* K : { "city.tch.call.pick", "city.tch.call.dial.clock", "city.tch.call.ans.random", "city.tch.fscale.weight", "city.tch.fortune.8",
			 "city.tch.mscales.weight.alone", "city.tch.fountain.poor.2", "city.tch.lib.shut.2", "city.tch.store.plate", "city.coll.coin_slots.title",
			 "city.coll.reward.fortunes.note" })
	{
		Missing += Has(K) ? 0 : 1;
	}
	CHECK(Missing == 0, "the scenes' words are in the tables (%d missing)", Missing);
}
