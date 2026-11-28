// tools/peoplesim/run.sh days [days] [off]: the town over several days, looked at for how much one day is like another
// (docs/town_life.md "The days", Private/People/PeopleDays.cpp). Prints a few residents' diaries, how alike everybody's
// days are, what the calendar brought (colds, bad nights, birthdays, the pay, the pension, the errands of the week), the
// day's plans kept (tea at a friend's, a couple's stroll, chess on a bench, Sunday at the grandparents') and the
// meetings on the way. "off" runs the same town without the days (FWorld::bDays false): what it was before.
// LYUD_DAYS_WHO=key,key prints those people's diaries instead of the usual sample.
// Included by peoplesim.cpp.

static int TownDays(int Argc, char** Argv)
{
	int NumDays = 7;
	bool bOff = false;
	for (int I = 0; I < Argc; I++)
	{
		if (std::string(Argv[I]) == "off")
		{
			bOff = true;
		}
		else if (std::atoi(Argv[I]) > 0)
		{
			NumDays = std::atoi(Argv[I]);
		}
	}
	FWorld W;
	if (!W.Load(DataDir, AnimalsDir))
	{
		std::printf("load failed: %s\n", W.LastError.c_str());
		return 1;
	}
	W.AssignHomes();
	AttachRail(W);
	// the manners' share of the life: the rituals, the households' outings, the weekly dates (as the game has them)
	LyudPeople::FManners Manners;
	if (Manners.Load(ReadFile(std::string(DataDir) + "/manners.json")))
	{
		Manners.Bind(W);
		Manners.ApplyLikes(W);
	}
	W.bDays = !bOff;
	// Monday 04:00 of an even week (the pension on Wednesday, the pay on Friday), dry and mild
	FClock C = FClock::At(0, 4.f);
	C.Set(W);
	W.TempC = 14.f;
	W.Resettle();
	const int N = (int)W.People.size();
	static const char* DayNames[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
	struct FBlock { float From; int Act; int Place; double Secs; };
	// per person per day: the blocks of what they did (the ways between left out)
	std::vector<std::vector<std::vector<FBlock>>> Diary((size_t)N, std::vector<std::vector<FBlock>>((size_t)NumDays));
	std::vector<std::vector<uint8_t>> Today((size_t)N, std::vector<uint8_t>((size_t)NumDays, 0));
	std::vector<std::vector<std::string>> Plans((size_t)N, std::vector<std::string>((size_t)NumDays));
	std::map<std::string, double> Minutes;
	const int Visit = W.FindActivity("visit_home"), Stroll = W.FindActivity("couple_stroll"),
		Chess = W.FindActivity("chess_bench"), School = W.FindActivity("school_town"), Pension = W.FindActivity("pension_office"),
		EatOut = W.FindActivity("eat_out"), Coffee = W.FindActivity("coffee");
	int PlansMade[(int)EPlan::Num] = {}, PlansKept[(int)EPlan::Num] = {};
	std::vector<uint8_t> Kept((size_t)N, 0);
	int ColdDays = 0, SleepyDays = 0, Birthdays = 0, Paid = 0, Pensioners = 0, PensionFetched = 0;
	double ColdOutMin = 0.0, ColdSchoolMin = 0.0, StrangerMin = 0.0, VisitMin = 0.0, StrollNearMin = 0.0, StrollMin = 0.0, ChessNearMin = 0.0, ChessMin = 0.0;
	double EveOutPaid = 0.0, EveOutOther = 0.0;
	int PaidDays = 0, OtherDays = 0;
	double LateSum = 0.0;
	int LateN = 0;
	std::vector<uint8_t> Fetched((size_t)N, 0);
	double Ms = 0.0;
	long Steps = 0;
	const float Dt = 2.f;
	int LastDay = -1;
	double NextMinute = 0.0;
	for (double T = 0.0; T < NumDays * 86400.0; T += Dt)
	{
		C.Seconds += Dt;
		C.Set(W);
		W.TempC = 14.f;
		const auto T0 = std::chrono::high_resolution_clock::now();
		W.Step(Dt);
		Ms += std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - T0).count();
		Steps++;
		W.Events.clear();
		// a diary's day runs from 04:00 to 04:00
		const int Day = std::min(NumDays - 1, (int)(T / 86400.0));
		if (Day != LastDay && W.Hour >= 4.f && W.Hour < 23.f)
		{
			LastDay = Day;
			std::fill(Kept.begin(), Kept.end(), 0);
			std::fill(Fetched.begin(), Fetched.end(), 0);
			for (const FPerson& P : W.People)
			{
				if (!P.bTown)
				{
					continue;
				}
				Today[(size_t)P.Id][(size_t)Day] = P.Today;
				ColdDays += P.Today & LyudPeople::TodayCold ? 1 : 0;
				SleepyDays += P.Today & LyudPeople::TodaySleepy ? 1 : 0;
				Birthdays += P.Today & LyudPeople::TodayBirthday ? 1 : 0;
				Paid += P.Today & LyudPeople::TodayPayday ? 1 : 0;
				Pensioners += P.Today & LyudPeople::TodayPension ? 1 : 0;
				if (P.LateH > 0.f)
				{
					LateSum += P.LateH;
					LateN++;
				}
				if (P.PlanAct >= 0)
				{
					// (a plan is counted once: by the guest, by the one of a pair with the lower id)
					const bool bCount = P.PlanAct == Visit || ((P.PlanAct == Stroll || P.PlanAct == Chess) && P.PlanWith > P.Id);
					PlansMade[(int)P.PlanKind] += bCount ? 1 : 0;
					Plans[(size_t)P.Id][(size_t)Day] = W.DescribeToday(P.Id);
				}
			}
			const bool bPayday = W.Weekday == 4 && W.Week % 2 == 0;
			(bPayday ? PaidDays : OtherDays) += W.Weekday < 5 ? 1 : 0;
		}
		for (const FPerson& P : W.People)
		{
			if (!P.bTown || P.Activity < 0 || !(P.State == EState::Doing || P.State == EState::Away || P.State == EState::Settling))
			{
				continue;
			}
			std::vector<FBlock>& B = Diary[(size_t)P.Id][(size_t)Day];
			if (B.empty() || B.back().Act != P.Activity || B.back().Place != P.Place)
			{
				B.push_back({ W.Hour, P.Activity, P.Place, 0.0 });
			}
			B.back().Secs += Dt;
		}
		if (T < NextMinute)
		{
			continue;
		}
		NextMinute += 60.0;
		for (const FPerson& P : W.People)
		{
			if (!P.bTown || P.Activity < 0 || P.State != EState::Doing)
			{
				continue;
			}
			Minutes[W.Activities[(size_t)P.Activity].Id] += 1.0;
			if (P.Today & LyudPeople::TodayCold)
			{
				ColdOutMin += P.Place != P.Home && W.Activities[(size_t)P.Activity].bFree && W.Activities[(size_t)P.Activity].Id != "clinic_cold" ? 1.0 : 0.0;
				ColdSchoolMin += P.Activity == School ? 1.0 : 0.0;
			}
			if (P.Activity == Pension && !Fetched[(size_t)P.Id])
			{
				Fetched[(size_t)P.Id] = 1;
				PensionFetched++;
			}
			if (W.Weekday < 5 && W.Hour >= 17.f && W.Hour < 22.f && (P.Activity == EatOut || P.Activity == Coffee))
			{
				(W.Weekday == 4 && W.Week % 2 == 0 ? EveOutPaid : EveOutOther) += 1.0;
			}
			const FPerson* Q = P.PlanWith >= 0 ? &W.People[(size_t)P.PlanWith] : nullptr;
			if (P.Activity == Visit)
			{
				VisitMin += 1.0;
				// a guest sits only in the flat they were asked to, with somebody of that flat at home
				bool bHostHome = false;
				for (const int R : W.Places[(size_t)P.Place].Residents)
				{
					bHostHome |= W.People[(size_t)R].Place == P.Place && W.People[(size_t)R].State == EState::Doing;
				}
				StrangerMin += P.Place != P.PlanPlace || P.Place == P.Home ? 1.0 : 0.0;
				if (bHostHome && !Kept[(size_t)P.Id])
				{
					Kept[(size_t)P.Id] = 1;
					PlansKept[(int)P.PlanKind]++;
				}
			}
			else if ((P.Activity == Stroll || P.Activity == Chess) && Q)
			{
				const bool bNear = Q->Activity == P.Activity && Q->State == EState::Doing && (Q->Pos - P.Pos).Len2D() < (P.Activity == Stroll ? 40.f : 6.f);
				(P.Activity == Stroll ? StrollMin : ChessMin) += 1.0;
				(P.Activity == Stroll ? StrollNearMin : ChessNearMin) += bNear ? 1.0 : 0.0;
				if (bNear && P.PlanWith > P.Id && !Kept[(size_t)P.Id])
				{
					Kept[(size_t)P.Id] = 1;
					PlansKept[(int)P.PlanKind]++;
				}
			}
		}
	}
	// how alike the days are: for each townsperson, each pair of weekdays one after another (Mon-Tue .. Thu-Fri), the
	// share of what they did (activity and place, 10 minutes and more) on the one day that they did on the other too
	auto DaySet = [&](int Id, int Day)
	{
		std::set<std::pair<int, int>> S;
		for (const FBlock& B : Diary[(size_t)Id][(size_t)Day])
		{
			if (B.Secs >= 600.0)
			{
				S.insert({ B.Act, B.Place });
			}
		}
		return S;
	};
	auto DaySeq = [&](int Id, int Day)
	{
		std::vector<int> S;
		for (const FBlock& B : Diary[(size_t)Id][(size_t)Day])
		{
			if (B.Secs >= 600.0 && (S.empty() || S.back() != B.Act))
			{
				S.push_back(B.Act);
			}
		}
		return S;
	};
	double Alike = 0.0;
	int Pairs = 0, SameSeq = 0, Kinds = 0, Town = 0;
	for (const FPerson& P : W.People)
	{
		if (!P.bTown || P.StayCycle > 0)
		{
			continue;
		}
		Town++;
		std::set<int> Acts;
		for (int D = 0; D < NumDays; D++)
		{
			for (const FBlock& B : Diary[(size_t)P.Id][(size_t)D])
			{
				if (B.Secs >= 600.0)
				{
					Acts.insert(B.Act);
				}
			}
			if (D + 1 >= NumDays || D % 7 >= 4)
			{
				continue;
			}
			const auto A = DaySet(P.Id, D), B = DaySet(P.Id, D + 1);
			int Both = 0;
			for (const auto& X : A)
			{
				Both += B.count(X) ? 1 : 0;
			}
			const int Either = (int)A.size() + (int)B.size() - Both;
			Alike += Either > 0 ? (double)Both / Either : 1.0;
			SameSeq += DaySeq(P.Id, D) == DaySeq(P.Id, D + 1) ? 1 : 0;
			Pairs++;
		}
		Kinds += (int)Acts.size();
	}
	// the diaries of a few: the first pensioner, pupil, seller and commuter of the list, and whoever had the fullest week
	std::vector<int> Who;
	if (const char* Env = std::getenv("LYUD_DAYS_WHO"))
	{
		std::stringstream SS(Env);
		std::string Key;
		while (std::getline(SS, Key, ','))
		{
			if (W.FindPerson(Key) >= 0)
			{
				Who.push_back(W.FindPerson(Key));
			}
		}
	}
	if (Who.empty())
	{
		for (const char* Job : { "pensioner", "pupil", "seller", "away", "programmer" })
		{
			for (const FPerson& P : W.People)
			{
				if (P.bTown && P.Occupation == Job)
				{
					Who.push_back(P.Id);
					break;
				}
			}
		}
		int Fullest = -1, Most = 0;
		for (const FPerson& P : W.People)
		{
			int Marks = 0;
			for (int D = 0; D < NumDays; D++)
			{
				Marks += (Today[(size_t)P.Id][(size_t)D] ? 1 : 0) + (Plans[(size_t)P.Id][(size_t)D].empty() ? 0 : 1);
			}
			if (P.bTown && Marks > Most)
			{
				Most = Marks;
				Fullest = P.Id;
			}
		}
		if (Fullest >= 0)
		{
			Who.push_back(Fullest);
		}
	}
	std::printf("the town's days: %d days from Monday 04:00, %d townsfolk, the days %s\n", NumDays, Town, bOff ? "OFF (as before)" : "on");
	for (const int Id : Who)
	{
		const FPerson& P = W.People[(size_t)Id];
		std::printf("\n%s, %d, %s (%s; wakes %.1f, bed %.1f)\n", P.Key.c_str(), P.Age, P.Occupation.c_str(), P.Chronotype.c_str(), P.Wake, P.BedTime);
		for (int D = 0; D < NumDays; D++)
		{
			std::string Line;
			for (const FBlock& B : Diary[(size_t)Id][(size_t)D])
			{
				if (B.Secs < 480.0 || B.Act == W.SleepAct)
				{
					continue;
				}
				char Buf[160];
				const bool bHome = B.Place == P.Home;
				std::snprintf(Buf, sizeof Buf, "%s%02d:%02d %s%s%s", Line.empty() ? "" : ", ", (int)B.From, (int)(std::fmod(B.From, 1.f) * 60.f),
					W.Activities[(size_t)B.Act].Id.c_str(), bHome || B.Place < 0 ? "" : "@", bHome || B.Place < 0 ? "" : W.Places[(size_t)B.Place].Id.c_str());
				Line += Buf;
			}
			std::string Mark;
			for (int Bit = 0; Bit < LyudPeople::NumTodayBits; Bit++)
			{
				if (Today[(size_t)Id][(size_t)D] & (1 << Bit))
				{
					Mark += std::string(Mark.empty() ? " [" : ", ") + LyudPeople::TodayName(Bit);
				}
			}
			Mark += Mark.empty() ? "" : "]";
			std::printf("  %s%s: %s\n", DayNames[D % 7], Mark.c_str(), Line.c_str());
			if (!Plans[(size_t)Id][(size_t)D].empty())
			{
				std::printf("       (%s)\n", Plans[(size_t)Id][(size_t)D].c_str());
			}
		}
	}
	std::printf("\nhow alike the days are (Mon-Tue .. Thu-Fri, %d person-day pairs):\n", Pairs);
	std::printf("  what was done on one day and on the next too (activity and place): %.1f%%\n", Pairs ? 100.0 * Alike / Pairs : 0.0);
	std::printf("  two days one after another with the very same order of things: %.1f%%\n", Pairs ? 100.0 * SameSeq / Pairs : 0.0);
	std::printf("  different things done in the %d days, a person: %.1f\n", NumDays, Town ? (double)Kinds / Town : 0.0);
	std::printf("what the days brought: %d person-days with a cold, %d after a bad night (up %.0f min late on average, %d of them), %d birthdays, %d paid, %d pensions due\n",
		ColdDays, SleepyDays, LateN ? 60.0 * LateSum / LateN : 0.0, LateN, Birthdays, Paid, Pensioners);
	std::printf("the plans (made / kept): tea %d / %d, birthday guests %d / %d, family dinners %d / %d (guests), strolls %d / %d, chess %d / %d\n",
		PlansMade[(int)EPlan::Tea], PlansKept[(int)EPlan::Tea], PlansMade[(int)EPlan::Birthday], PlansKept[(int)EPlan::Birthday],
		PlansMade[(int)EPlan::Family], PlansKept[(int)EPlan::Family], PlansMade[(int)EPlan::Stroll], PlansKept[(int)EPlan::Stroll],
		PlansMade[(int)EPlan::Chess], PlansKept[(int)EPlan::Chess]);
	std::printf("on the way: %d stops for a word, %d greetings in passing, %d doors held\n", W.Meets, W.Nods, W.Holds);
	std::printf("minutes:");
	for (const char* Id : { "market_day", "library_return", "pension_office", "clinic_cold", "visit_home", "host_guests", "couple_stroll", "chess_bench",
		"eat_out", "coffee", "shop_town", "nap", "read_library", "sport", "culture_evening" })
	{
		std::printf(" %s %.0f", Id, Minutes[Id]);
	}
	std::printf("\n");
	if (!bOff)
	{
		const int AllMade = PlansMade[1] + PlansMade[2] + PlansMade[3] + PlansMade[4] + PlansMade[5];
		const int AllKept = PlansKept[1] + PlansKept[2] + PlansKept[3] + PlansKept[4] + PlansKept[5];
		CHECK(ColdDays > 0 && ColdOutMin < ColdDays * 25.0 && ColdSchoolMin == 0.0, "a cold keeps people at home and out of school (%d person-days; %.0f min out at something they chose, %.0f at school)", ColdDays, ColdOutMin, ColdSchoolMin);
		CHECK(SleepyDays > 0 && LateN > 0, "a bad night makes for a late morning (%d person-days, %d up late)", SleepyDays, LateN);
		CHECK(NumDays < 3 || (Pensioners > 50 && PensionFetched > Pensioners * 0.6), "the pension is fetched at the office on its Wednesday (%d of %d)", PensionFetched, Pensioners);
		CHECK(NumDays < 5 || (Paid > 100 && EveOutPaid / std::max(1, PaidDays) > 1.08 * EveOutOther / std::max(1, OtherDays)),
			"payday's evening fills the cafes (%.0f person-minutes against %.0f on another weekday's evening)", EveOutPaid / std::max(1, PaidDays), EveOutOther / std::max(1, OtherDays));
		CHECK(AllMade > NumDays * 20 && AllKept > AllMade * 0.5, "the day's plans are made and more than half kept (%d of %d)", AllKept, AllMade);
		CHECK(VisitMin > 0.0 && StrangerMin == 0.0, "guests sit only where they were asked (%.0f person-minutes, %.0f elsewhere)", VisitMin, StrangerMin);
		CHECK(StrollMin > 0.0 && StrollNearMin > StrollMin * 0.5, "a couple's stroll is walked together (%.0f of %.0f person-minutes within 40 m)", StrollNearMin, StrollMin);
		CHECK(ChessMin > 0.0 && ChessNearMin > ChessMin * 0.5, "chess is played side by side on one bench (%.0f of %.0f person-minutes)", ChessNearMin, ChessMin);
		CHECK(NumDays < 6 || Minutes["market_day"] > 300.0, "Saturday is the market's day (%.0f person-minutes)", Minutes["market_day"]);
		CHECK(Minutes["library_return"] > 100.0, "books go back to the library on people's own days (%.0f person-minutes)", Minutes["library_return"]);
		CHECK(W.Meets > NumDays * 5 && W.Nods > W.Meets, "people who know each other stop for a word or greet on the way (%d, %d)", W.Meets, W.Nods);
	}
	std::printf("  a step of %.0f s costs %.3f ms (%ld steps, %d people)\n", Dt, Steps ? Ms / Steps : 0.0, Steps, N);
	std::printf("%s\n", Failures ? "FAILED" : "ok");
	return Failures ? 1 : 0;
}
