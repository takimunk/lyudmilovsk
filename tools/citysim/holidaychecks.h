// Science Day, the Old New Year and Neptune Day (docs/festivals.md) on the real data: the six hands-on scenes play only on
// their holiday and in their hours, a careful hand gets the stamp and a careless one does not, the day's staff talk is
// offered on the day only and its food gives a stamp, and the nine stamps fill the holiday calendar. Included by
// citytest.cpp (its CHECK, ReadFile, PlayProc).

static void HolidayChecks(const std::string& Path)
{
	std::string Text, Error;
	if (!ReadFile(Path, Text))
	{
		return;
	}
	std::printf("the three new holidays: scenes, staff talk and stamps (real data)\n");
	FModel M;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	struct FScene
	{
		const char* Festival;
		int Weekday;      // 0 Monday: the day it falls on in the game's first year
		float Hour;       // inside the scene's hours
		float Off;        // outside them (the thing still open)
		const char* Thing;
		const char* Piece;
		const char* Good;
	};
	static const FScene Scenes[] = {
		{ "science_day", 5, 12.f, 20.f, "P-FAIR/lottery_tickets", "nh_calendar/sci_quiz", "city.nh.sci.quiz.good.1" },
		{ "science_day", 5, 13.f, 20.f, "P-FAIR/fortune_globe", "nh_calendar/sci_globe", "city.nh.sci.globe.good.1" },
		{ "old_new_year", 2, 18.f, 12.f, "W1-U17/dumpling_rolling", "nh_calendar/ony_vareniki", "city.nh.ony.var.good.1" },
		{ "old_new_year", 2, 19.f, 12.f, "W1-U17/dumpling_pot", "nh_calendar/ony_wax", "city.nh.ony.wax.good.1" },
		{ "neptune_day", 6, 13.f, 18.f, "P-LIFEGUARD1/rope_coil", "nh_calendar/nep_rope", "city.nh.nep.rope.good.1" },
		{ "neptune_day", 6, 14.f, 18.f, "W1-U15/rental_pump", "nh_calendar/nep_tube", "city.nh.nep.tube.good.1" },
	};
	struct FFood
	{
		const char* Festival;
		int Weekday;
		float Hour;
		const char* Thing;
		const char* Topic;
		const char* Piece;
	};
	static const FFood Foods[] = {
		{ "science_day", 5, 12.f, "P-FAIR/kiosk1_counter", "nh_fair.cocktail", "nh_calendar/sci_cocktail" },
		{ "old_new_year", 2, 18.f, "W1-U17/dumpling_counter", "nh_dumplings.surprise", "nh_calendar/ony_treat" },
		{ "neptune_day", 6, 17.f, "P-LIFEGUARD1/flask_tea", "nh_lifeguard.ukha", "nh_calendar/nep_ukha" },
	};
	FAsk Ask;
	Ask.Money = 1000;
	Ask.StaffOnShift = [](const std::string&, const std::string&) { return true; };
	auto Plain = [](int Weekday, float Hour, long long Day) { FWhen W = AtDate(Weekday, Hour, Day, 0, Day - 1000); W.Weather = 0x40; return W; };
	auto Feast = [&Plain](const char* Festival, int Weekday, float Hour, long long Day)
	{
		FWhen W = Plain(Weekday, Hour, Day);
		W.bFestival = true;
		W.Festivals.push_back(Festival);
		return W;
	};
	const std::vector<FProcCompanion> Nobody;
	int Missing = 0, PlainDay = 0, OnDay = 0, OffHours = 0, OtherFeast = 0, Poor = 0, Good = 0;
	std::string Bad;
	long long Day = 1100;
	for (const FScene& S : Scenes)
	{
		const int T = M.FindThing(S.Thing);
		if (T < 0)
		{
			Missing++;
			Bad += std::string(" ") + S.Thing;
			continue;
		}
		Day += 7;
		PlainDay += M.ProcNow(T, Plain(S.Weekday, S.Hour, Day), Ask) < 0 ? 1 : 0;
		OnDay += M.ProcNow(T, Feast(S.Festival, S.Weekday, S.Hour, Day), Ask) >= 0 ? 1 : 0;
		OffHours += M.ProcNow(T, Feast(S.Festival, S.Weekday, S.Off, Day), Ask) < 0 ? 1 : 0;
		OtherFeast += M.ProcNow(T, Feast("sea_festival", S.Weekday, S.Hour, Day), Ask) < 0 ? 1 : 0;
		const FProcEnd Careless = PlayProc(M, T, Feast(S.Festival, S.Weekday, S.Hour, Day), Ask, EProcPlay::Poor, Nobody);
		const bool bPoor = Careless.Result == 2 && !M.HasStamp(S.Piece);
		Poor += bPoor ? 1 : 0;
		const FProcEnd Careful = PlayProc(M, T, Feast(S.Festival, S.Weekday, S.Hour + 0.5f, Day), Ask, EProcPlay::Good, Nobody);
		const bool bGood = Careful.Result == 0 && Careful.Line == S.Good && Careful.Run.Verdict == EVerdict::Ok && M.HasStamp(S.Piece);
		Good += bGood ? 1 : 0;
		if (!bPoor || !bGood)
		{
			Bad += std::string(" ") + S.Thing + "(" + std::to_string(Careless.Result) + "," + std::to_string(Careful.Result) + " " + Careful.Line + ")";
		}
	}
	CHECK(Missing == 0, "the six things of the scenes are there (%d missing)%s", Missing, Missing ? Bad.c_str() : "");
	CHECK(PlainDay == 6, "on an ordinary day none of them is a scene: the old act answers (%d of 6)", PlainDay);
	CHECK(OnDay == 6 && OffHours == 6 && OtherFeast == 6, "each is a scene on its own holiday, in its hours only (%d on the day, %d not off hours, %d not on another holiday)",
		OnDay, OffHours, OtherFeast);
	CHECK(Poor == 6 && Good == 6, "a careless hand gets the poor words and no stamp, a careful one the good words and the stamp (%d, %d)%s", Poor, Good,
		Poor == 6 && Good == 6 ? "" : Bad.c_str());
	// the day's food: a staff talk topic offered on the holiday only, its first asking gives the stamp
	int FoodPlain = 0, FoodOn = 0, FoodStamp = 0;
	std::vector<int> Offer;
	for (const FFood& F : Foods)
	{
		const int T = M.FindThing(F.Thing);
		if (T < 0)
		{
			continue;
		}
		Day += 7;
		auto Has = [&M, &Offer, &F]()
		{
			for (const int Topic : Offer)
			{
				if (M.Topics[(size_t)Topic].Id == F.Topic)
				{
					return Topic;
				}
			}
			return -1;
		};
		M.TalkReady(T, Plain(F.Weekday, F.Hour, Day), Ask, Offer);
		FoodPlain += Has() < 0 ? 1 : 0;
		const FWhen W = Feast(F.Festival, F.Weekday, F.Hour, Day);
		M.TalkReady(T, W, Ask, Offer);
		const int Topic = Has();
		FoodOn += Topic >= 0 ? 1 : 0;
		if (Topic >= 0)
		{
			const FTopicAnswer A = M.AskTopic(Topic, W);
			FoodStamp += !A.Lines.empty() && M.HasStamp(F.Piece) ? 1 : 0;
		}
	}
	CHECK(FoodPlain == 3 && FoodOn == 3, "the day's food is talked of on its holiday only (%d not on a plain day, %d on the day)", FoodPlain, FoodOn);
	CHECK(FoodStamp == 3, "asking gives the food's stamp (%d of 3)", FoodStamp);
	CHECK(M.CountStamps("nh_calendar") == 9, "the holiday calendar is full: %d of 9", M.CountStamps("nh_calendar"));
}
