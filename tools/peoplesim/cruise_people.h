// The motor ship's people (docs/ship.md "People aboard"; tools/ship/cruise_people.py makes the data): the crew, the band,
// the tourists and the cruising townsfolk's twins aboard «Полдень» (island 4: PeopleData/ship_places.json,
// ship_activities.json, ship_people.json, read last by FWorld::Load), and those townsfolk's own records, which are gone
// from the pier head meanwhile. Included by peoplesim.cpp: run.sh cruise.
//
// Checked, minute by minute on the real data, a season's voyage from Friday 12:00 to Monday 12:00 (the whole town lives on):
// nobody of the ship ever leaves her (island 4, her box) nor finds "no way", nobody jumps (the walk grid's clamp); the
// twins' town records wait at the pier head, are gone while their twins are aboard and back on Sunday evening, never both in
// the world; every meal seats everybody at their own chair, no place aboard holds more than it has room for; the wheel is
// never left while she is under way; the band plays through both concerts; the counters' people are on shift in their
// hours (FWorld::StaffOnShift); everybody musters at the boats; two nights in their own berths; a Resettle (a load, a clock
// jump) lands on the same duty as stepping. Then a Wednesday (nobody aboard), the other week (the other set of twins) and
// a weekend out of season (nobody aboard, the townsfolk at home).

namespace Cruise
{
	struct FWho
	{
		int Id = -1;
		bool bCrew = false, bBand = false, bTwin = false;
		int Set = -1;           // a twin's set (0: the even weeks)
		int Town = -1;          // a twin's town record
		int SleepMin[2] = { 0, 0 };     // minutes asleep in their own berth, the two nights
		int SleepAll = 0;
		bool bCabin = false;    // seen in their cabin on Friday
		V3 Prev;
		bool bPrevHere = false;
	};

	static bool Here(const FPerson& P) { return P.State != EState::Away; }

	static bool LoadWorld(FWorld& W, bool bSeason)
	{
		if (!W.Load(DataDir, AnimalsDir))
		{
			std::printf("load failed: %s\n", W.LastError.c_str());
			return false;
		}
		W.AssignHomes();
		AttachRail(W);
		W.SetCondition("cruise_season", bSeason);
		return true;
	}

	static std::vector<FWho> Who(const FWorld& W)
	{
		std::vector<FWho> Out;
		for (const FPerson& P : W.People)
		{
			if (P.Home < 0 || W.Places[(size_t)P.Home].Island < 4)
			{
				continue;
			}
			FWho X;
			X.Id = P.Id;
			X.bTwin = P.Key.rfind("c_", 0) == 0;
			X.bBand = P.Occupation == "ship_musician";
			X.bCrew = !X.bTwin && !X.bBand && P.Occupation != "ship_tourist";
			if (X.bTwin)
			{
				X.Town = W.FindPerson("t_" + P.Key.substr(2));
				X.Set = P.StayFrom >= 7 ? 1 : 0;
			}
			Out.push_back(X);
		}
		return Out;
	}

	static const char* ActName(const FWorld& W, const FPerson& P)
	{
		return P.Activity >= 0 ? W.Activities[(size_t)P.Activity].Id.c_str() : "-";
	}

	static int OnShift(const FWorld& W, const char* Role, bool bDoing = false)
	{
		std::vector<int> Ids;
		W.StaffOnShift("P-SHIP", Role, Ids);
		if (!bDoing)
		{
			return (int)Ids.size();
		}
		int N = 0;
		for (const int Id : Ids)
		{
			N += W.People[(size_t)Id].State == EState::Doing ? 1 : 0;
		}
		return N;
	}
}

static int CruisePeople()
{
	using namespace Cruise;
	const int Fails0 = Failures;
	FWorld W;
	if (!LoadWorld(W, true))
	{
		return 1;
	}
	std::printf("the motor ship's people (a voyage in season, Friday 12:00 to Monday 12:00):\n");
	std::vector<FWho> Ship = Who(W);
	int NCrew = 0, NBand = 0, NTwin = 0, NTourist = 0;
	for (const FWho& X : Ship)
	{
		NCrew += X.bCrew;
		NBand += X.bBand;
		NTwin += X.bTwin;
		NTourist += !X.bCrew && !X.bBand && !X.bTwin;
	}
	const int Work = W.FindPlace("P-SHIP"), Dining = W.FindPlace("P-SHIP-DINING"), Shore = W.FindPlace("S-SHIP-SHORE"), Pier = W.FindPlace("S-PIER-CRUISE");
	CHECK(Work >= 0 && Dining >= 0 && Shore >= 0 && Pier >= 0 && !Ship.empty(), "the ship's places and people are loaded (%d crew, %d band, %d tourists, %d twins)",
		NCrew, NBand, NTourist, NTwin);
	if (Work < 0 || Dining < 0 || Shore < 0 || Pier < 0 || Ship.empty())
	{
		return 1;
	}
	// her box: every slot of island 4, and a little round it
	V3 Lo(1e9f, 1e9f, 1e9f), Hi(-1e9f, -1e9f, -1e9f);
	int ShipSlots = 0;
	for (const FSlot& Sl : W.Slots)
	{
		if (W.Places[(size_t)Sl.Place].Island >= 4)
		{
			ShipSlots++;
			Lo = V3(std::min(Lo.X, Sl.Pos.X), std::min(Lo.Y, Sl.Pos.Y), std::min(Lo.Z, Sl.Pos.Z));
			Hi = V3(std::max(Hi.X, Sl.Pos.X), std::max(Hi.Y, Sl.Pos.Y), std::max(Hi.Z, Sl.Pos.Z));
		}
	}
	auto InBox = [&](const V3& P) { return P.X > Lo.X - 3.f && P.X < Hi.X + 3.f && P.Y > Lo.Y - 3.f && P.Y < Hi.Y + 3.f && P.Z > Lo.Z - 1.f && P.Z < Hi.Z + 3.f; };
	bool bHomes = true, bBeds = true, bOwnBed = true, bTwinsOk = true, bPrivate = true;
	std::set<int> BedsTaken;
	for (const FWho& X : Ship)
	{
		const FPerson& P = W.People[(size_t)X.Id];
		bHomes &= P.Home >= 0 && W.Places[(size_t)P.Home].Id == P.HomeId;
		bBeds &= P.BedSlot >= 0 && W.Slots[(size_t)P.BedSlot].Place == P.Home && W.Slots[(size_t)P.BedSlot].Kind == "bed";
		bOwnBed &= BedsTaken.insert(P.BedSlot).second;
		bTwinsOk &= !X.bTwin || (X.Town >= 0 && W.People[(size_t)X.Town].StayCycle == 14 && W.People[(size_t)X.Town].Body == P.Body
			&& W.People[(size_t)X.Town].Home != P.Home && W.People[(size_t)X.Town].Island == 0);
		bPrivate &= W.Places[(size_t)P.Home].bPrivate;
	}
	bool bTownHomes = true;
	for (const FPerson& P : W.People)
	{
		// nobody of the town was given a cabin, nobody of the ship a flat ashore
		bTownHomes &= P.Home < 0 || (W.Places[(size_t)P.Home].Island >= 4) == (P.Key.rfind("ship_", 0) == 0 || P.Key.rfind("c_", 0) == 0);
	}
	CHECK(bHomes && bPrivate, "everybody aboard has the cabin or the berth the data gives them (private: never handed out)");
	CHECK(bBeds && bOwnBed, "a berth each, no berth twice (%zu berths)", BedsTaken.size());
	CHECK(bTwinsOk, "each twin has a town record with the same body, a fortnight's stay and a home ashore");
	CHECK(bTownHomes, "no townsperson lives aboard, nobody of the ship lives ashore");

	FClock C;
	const int SleepAct = W.FindActivity("sleep"), DrillAct = W.FindActivity("cruise_drill"), DanceAct = W.FindActivity("cruise_dance");
	const int BoardAct = W.FindActivity("cruise_board");
	// ---- the voyage
	C = FClock::At(4, 12.f);
	C.Set(W);
	W.Resettle();
	int OffShip = 0, NoWay = 0, Jumps = 0, Both = 0, Over = 0, WheelGap = 0, WheelMin = 0;
	float MaxStep = 0.f;
	std::string OffWho, NoWayWho, JumpWho, BothWho, OverWhat, WheelWhen;
	int BandGap[3] = { 0, 0, 0 }, BandMin[3] = { 0, 0, 0 };
	struct FRole { const char* Role; int Gap = 0, Min = 0; std::string When; };
	FRole Roles[5] = { { "purser" }, { "barman" }, { "steward" }, { "cook" }, { "radio" } };
	struct FMeal { const char* Act; int Day; float Hour; int Expected = 0, Seated = 0; bool bDone = false; std::string Missing; };
	FMeal Meals[] = { { "cruise_dinner", 4, 19.95f }, { "crew_breakfast", 5, 7.42f }, { "cruise_breakfast", 5, 8.55f }, { "cruise_lunch", 5, 13.5f },
		{ "cruise_tea", 5, 16.3f }, { "cruise_gala_dinner", 5, 19.75f }, { "cruise_breakfast", 6, 8.55f }, { "cruise_lunch", 6, 13.5f } };
	int Muster = -1, MusterOf = 0, AtRail = -1, RailOf = 0, InSaloon = -1, SaloonOf = 0, Dancing = -1, Quiz = -1, QuizOf = 0;
	bool bHaruto = false, bLeads = false;
	int PierWait = 0, PierOf = 0, TownGoneWrong = 0, TownBackLate = 0, OtherSetAboard = 0, OtherTownGone = 0, AboardBy1750 = 0, PaxOf = 0;
	int StillAboard = -1;
	std::string PierWho, GoneWho;
	const double End = (7.0 * 24.0 + 12.0) * 3600.0;
	long Steps = 0;
	const auto T0 = std::chrono::steady_clock::now();
	while (C.Seconds < End)
	{
		C.Seconds += 1.0;
		C.Set(W);
		W.Step(1.f);
		W.Events.clear();
		Steps++;
		const int Day = W.Weekday;
		const float H = W.Hour;
		// every step: nobody aboard jumps (a walk's step is a metre or two; sitting down, lying down a little more)
		for (FWho& X : Ship)
		{
			const FPerson& P = W.People[(size_t)X.Id];
			const bool bHere = Here(P);
			if (bHere && X.bPrevHere)
			{
				const float D = (P.Pos - X.Prev).Len();
				MaxStep = std::max(MaxStep, D);
				if (D > 6.f)
				{
					Jumps++;
					JumpWho = P.Key + " " + ActName(W, P);
				}
			}
			X.Prev = P.Pos;
			X.bPrevHere = bHere;
		}
		if (Steps % 60 != 0)
		{
			continue;
		}
		const bool bVoyage = (Day == 4 && H >= 14.f) || Day == 5 || (Day == 6 && H < 20.f);
		const bool bUnderWay = (Day == 4 && H >= 18.f) || Day == 5 || (Day == 6 && H >= 6.f && H < 18.f);
		for (FWho& X : Ship)
		{
			const FPerson& P = W.People[(size_t)X.Id];
			if (Here(P))
			{
				if (P.Island != 4 || !InBox(P.Pos))
				{
					OffShip++;
					OffWho = P.Key;
				}
				if (P.Why.rfind("no way", 0) == 0)
				{
					NoWay++;
					NoWayWho = P.Key + ": " + P.Why;
				}
				if (X.bTwin && X.Set == 1)
				{
					OtherSetAboard++;
				}
				if (P.Place == P.Home && P.State == EState::Doing && Day == 4)
				{
					X.bCabin = true;
				}
				if (P.Activity == SleepAct && P.State == EState::Doing && P.Slot == P.BedSlot)
				{
					X.SleepAll++;
					// the night of Friday: till Saturday noon; the night of Saturday: from Saturday 20:00 on
					const int Night = (Day == 4 || (Day == 5 && H < 12.f)) ? 0 : ((Day == 5 && H >= 20.f) || (Day == 6 && H < 12.f)) ? 1 : -1;
					if (Night >= 0)
					{
						X.SleepMin[Night]++;
					}
				}
			}
			if (X.bTwin && X.Town >= 0)
			{
				const FPerson& Tn = W.People[(size_t)X.Town];
				if (Here(P) && Here(Tn))
				{
					Both++;
					BothWho = P.Key;
				}
				if (Tn.Why.rfind("no way", 0) == 0)
				{
					NoWay++;
					NoWayWho = Tn.Key + ": " + Tn.Why;
				}
				if (X.Set == 1 && (W.Gone(Tn) || !Here(Tn) ? Tn.Activity == W.GoneAct : false))
				{
					OtherTownGone++;
				}
				if (X.Set == 0)
				{
					// the town's own record: away from Friday 17:20 (the last launch) to Sunday 18:10, back by 19:45
					const bool bAway = (Day == 4 && H >= 17.35f) || Day == 5 || (Day == 6 && H < 18.1f);
					const bool bBack = (Day == 6 && H >= 19.75f) || Day == 0 || (Day == 4 && H < 15.5f);
					if (bAway && Here(Tn))
					{
						TownGoneWrong++;
						GoneWho = Tn.Key;
					}
					if (bBack && !Here(Tn) && Tn.Activity == W.GoneAct)
					{
						TownBackLate++;
						GoneWho = Tn.Key;
					}
				}
			}
		}
		// no place aboard holds more than it has room for
		for (const FSlot& Sl : W.Slots)
		{
			if (W.Places[(size_t)Sl.Place].Island >= 4 && Sl.Place != Shore && (int)Sl.Users.size() > Sl.Capacity)
			{
				Over++;
				OverWhat = W.Places[(size_t)Sl.Place].Id + " " + Sl.Kind + " " + Sl.Tag;
			}
		}
		// the wheel: somebody of the watch in the wheelhouse every minute she is under way
		if (bUnderWay)
		{
			WheelMin++;
			if (OnShift(W, "captain", true) + OnShift(W, "mate", true) + OnShift(W, "helmsman", true) < 1)
			{
				WheelGap++;
				char B[32];
				std::snprintf(B, sizeof B, "day %d %05.2f", Day, H);
				WheelWhen = B;
			}
		}
		// the band: the quartet through both concerts, three of them at the captain's dinner
		{
			const int N = OnShift(W, "band_piano", true) + OnShift(W, "band_bass", true) + OnShift(W, "band_drums", true) + OnShift(W, "band_reed", true);
			const int Which = Day == 4 && H >= 21.f && H < 22.95f ? 0 : Day == 5 && H >= 20.6f && H < 23.45f ? 1 : Day == 5 && H >= 19.05f && H < 20.3f ? 2 : -1;
			if (Which >= 0)
			{
				BandMin[Which]++;
				BandGap[Which] += N < (Which == 2 ? 3 : 4) ? 1 : 0;
			}
		}
		// the counters' people in their hours (a few minutes' grace at each end: the way there)
		if (bVoyage)
		{
			const bool bHours[5] = {
				(Day == 4 ? H >= 14.5f : H >= 8.15f) && H < (Day == 6 ? 19.9f : 22.95f),
				(Day == 4 ? H >= 14.5f : (H >= 10.15f || H < 0.95f)) && (Day != 6 || H < 19.9f),
				(Day == 4 ? H >= 14.5f : (H >= 7.15f && H < 9.4f) || H >= 11.2f) && H < (Day == 6 ? 19.9f : 20.95f),
				(Day == 4 ? H >= 14.5f : H >= 6.15f) && H < 19.9f,
				(Day == 4 ? H >= 14.5f : H >= 8.15f) && H < 19.9f };
			for (int R = 0; R < 5; R++)
			{
				if (bHours[R])
				{
					Roles[R].Min++;
					if (OnShift(W, Roles[R].Role) < 1)
					{
						Roles[R].Gap++;
						char B[32];
						std::snprintf(B, sizeof B, "day %d %05.2f", Day, H);
						Roles[R].When = B;
					}
				}
			}
		}
		// the meals: everybody whose meal it is, at their own chair
		for (FMeal& M : Meals)
		{
			if (M.bDone || Day != M.Day || H < M.Hour)
			{
				continue;
			}
			M.bDone = true;
			const int A = W.FindActivity(M.Act);
			for (const FWho& X : Ship)
			{
				const FPerson& P = W.People[(size_t)X.Id];
				if (!Here(P))
				{
					continue;
				}
				for (const FDuty& D : P.Duties)
				{
					if (D.Act == A && ((D.Days >> Day) & 1) && H >= D.From + 0.2f && H < D.To - 0.02f)
					{
						M.Expected++;
						const bool bSat = P.Activity == A && P.State == EState::Doing && P.Slot >= 0 && W.Slots[(size_t)P.Slot].Place == Dining
							&& W.Slots[(size_t)P.Slot].Tag == D.Role && P.Pose == EPose::Eat;
						M.Seated += bSat ? 1 : 0;
						if (!bSat)
						{
							M.Missing += " " + P.Key + "(" + ActName(W, P) + ")";
						}
					}
				}
			}
		}
		auto IsPax = [&](const FWho& X) { return !X.bCrew && !X.bBand && (!X.bTwin || X.Set == 0); };
		if (Day == 4 && H >= 16.4f && H < 16.42f)
		{
			// the cruising townsfolk at the pier head, waiting for the launch (each a little before their own)
			for (const FWho& X : Ship)
			{
				if (X.bTwin && X.Set == 0)
				{
					const FPerson& Tn = W.People[(size_t)X.Town];
					PierOf++;
					const bool bAt = (Tn.Place == Pier && Tn.Activity == BoardAct) || Tn.Activity == W.GoneAct;
					PierWait += bAt ? 1 : 0;
					if (!bAt)
					{
						PierWho += " " + Tn.Key + "(" + ActName(W, Tn) + ")";
					}
				}
			}
		}
		if (Day == 4 && H >= 17.83f && H < 17.85f)
		{
			for (const FWho& X : Ship)
			{
				if (IsPax(X))
				{
					PaxOf++;
					AboardBy1750 += Here(W.People[(size_t)X.Id]) ? 1 : 0;
				}
			}
		}
		if (Muster < 0 && Day == 4 && H >= 18.75f)
		{
			Muster = 0;
			for (const FWho& X : Ship)
			{
				const FPerson& P = W.People[(size_t)X.Id];
				if (IsPax(X) || X.bBand)
				{
					MusterOf++;
					Muster += P.Activity == DrillAct && P.State == EState::Doing && P.Slot >= 0 && W.Slots[(size_t)P.Slot].Kind == "muster" ? 1 : 0;
				}
			}
			bLeads = OnShift(W, "bosun_muster", true) == 1 && OnShift(W, "entertainer_muster", true) == 1;
		}
		if (AtRail < 0 && Day == 5 && H >= 10.5f)
		{
			AtRail = 0;
			for (const FWho& X : Ship)
			{
				const FPerson& P = W.People[(size_t)X.Id];
				if (IsPax(X))
				{
					RailOf++;
					const bool bAtRail = P.State == EState::Doing && P.Slot >= 0 && W.Slots[(size_t)P.Slot].Kind == "rail";
					AtRail += bAtRail ? 1 : 0;
					bHaruto |= bAtRail && P.Key == "ship_haruto";
				}
			}
		}
		if (InSaloon < 0 && Day == 5 && H >= 21.25f)
		{
			InSaloon = Dancing = 0;
			const int Saloon = W.FindPlace("P-SHIP-SALOON");
			for (const FWho& X : Ship)
			{
				const FPerson& P = W.People[(size_t)X.Id];
				if (IsPax(X))
				{
					SaloonOf++;
					InSaloon += P.Place == Saloon && P.State == EState::Doing ? 1 : 0;
					Dancing += P.Activity == DanceAct && P.State == EState::Doing ? 1 : 0;
				}
			}
		}
		if (Quiz < 0 && Day == 6 && H >= 10.4f)
		{
			Quiz = 0;
			const int QuizAct = W.FindActivity("cruise_quiz");
			for (const FWho& X : Ship)
			{
				const FPerson& P = W.People[(size_t)X.Id];
				for (const FDuty& D : P.Duties)
				{
					if (D.Act == QuizAct && Here(P))
					{
						QuizOf++;
						Quiz += P.Activity == QuizAct && P.State == EState::Doing ? 1 : 0;
					}
				}
			}
			Quiz += OnShift(W, "entertainer", true) == 1 ? 100 : 0;
		}
		if (StillAboard < 0 && Day == 6 && H >= 20.1f)
		{
			StillAboard = 0;
			for (const FWho& X : Ship)
			{
				StillAboard += Here(W.People[(size_t)X.Id]) ? 1 : 0;
			}
		}
		// a load, a clock jump in the middle of the voyage lands everybody on the same duty as stepping did
		for (const float At : { 3.f, 13.5f, 21.5f })
		{
			if (Day == 5 && H >= At && H < At + 0.015f)
			{
				FWorld W2;
				if (!LoadWorld(W2, true))
				{
					return 1;
				}
				C.Set(W2);
				W2.Resettle();
				int Same = 0, Of = 0;
				std::string Diff;
				for (const FWho& X : Ship)
				{
					const FPerson& P = W.People[(size_t)X.Id];
					const FPerson& Q = W2.People[(size_t)X.Id];
					// (whoever is at a fixed hour of theirs or asleep: free time is anybody's choice)
					const bool bFixed = !Here(P) || P.Activity == SleepAct || (P.Activity >= 0 && !W.Activities[(size_t)P.Activity].bFree);
					if (bFixed && P.State != EState::Going && P.State != EState::Idle)
					{
						Of++;
						const bool bSame = Q.Activity == P.Activity && (Q.Slot == P.Slot || (P.Activity >= 0 && !W.Activities[(size_t)P.Activity].bStaff
							&& P.Activity != SleepAct && Q.Slot >= 0 && P.Slot >= 0 && W.Slots[(size_t)Q.Slot].Kind == W.Slots[(size_t)P.Slot].Kind));
						Same += bSame ? 1 : 0;
						if (!bSame)
						{
							Diff += " " + P.Key + "(" + ActName(W, P) + "/" + ActName(W2, Q) + ")";
						}
					}
				}
				CHECK(Same == Of && Of >= 20, "Saturday %05.2f: a Resettle lands the same as stepping (%d of %d at a fixed hour or asleep%s)", At, Same, Of, Diff.c_str());
			}
		}
	}
	const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
	CHECK(OffShip == 0, "nobody of the ship ever leaves her: island 4, within her box (%d minutes off%s%s)", OffShip, OffShip ? ", last " : "", OffWho.c_str());
	CHECK(NoWay == 0, "nobody is left with no way (%d%s%s)", NoWay, NoWay ? ": " : "", NoWayWho.c_str());
	CHECK(Jumps == 0 && MaxStep < 6.f, "nobody aboard jumps: the longest step in a second is %.2f m (%d over 6 m%s%s)", MaxStep, Jumps, Jumps ? ", last " : "", JumpWho.c_str());
	CHECK(Over == 0, "no chair, berth or spot aboard is taken twice (%d%s%s)", Over, Over ? ": " : "", OverWhat.c_str());
	CHECK(PaxOf > 0 && AboardBy1750 == PaxOf, "Friday 17:50: all the passengers are aboard (%d of %d)", AboardBy1750, PaxOf);
	int Cabins = 0, Pax = 0;
	for (const FWho& X : Ship)
	{
		if (!X.bCrew && !X.bBand && (!X.bTwin || X.Set == 0))
		{
			Pax++;
			Cabins += X.bCabin ? 1 : 0;
		}
	}
	CHECK(Cabins == Pax, "each of them went to their cabin first (%d of %d)", Cabins, Pax);
	CHECK(PierOf > 0 && PierWait == PierOf, "Friday 16:24: the cruising townsfolk wait at the pier head or have gone aboard (%d of %d%s)", PierWait, PierOf, PierWho.c_str());
	CHECK(TownGoneWrong == 0 && TownBackLate == 0, "their town records are away from Friday 17:20 to Sunday 18:10 and back by 19:45 (%d minutes still in town, %d late%s%s)",
		TownGoneWrong, TownBackLate, GoneWho.empty() ? "" : ", last ", GoneWho.c_str());
	CHECK(Both == 0, "nobody is in two places: a twin and the town record never both in the world (%d%s%s)", Both, Both ? ", last " : "", BothWho.c_str());
	CHECK(OtherSetAboard == 0 && OtherTownGone == 0, "the other week's set stays in town (%d minutes aboard, %d away)", OtherSetAboard, OtherTownGone);
	CHECK(Muster == MusterOf && MusterOf >= 15 && bLeads, "Friday 18:45, the lifeboat drill: everybody at the muster station, the bosun and the entertainer before them (%d of %d)",
		Muster, MusterOf);
	for (const FMeal& M : Meals)
	{
		CHECK(M.bDone && M.Expected > 0 && M.Seated == M.Expected, "%s %05.2f %s: everybody at their own chair (%d of %d%s)", M.Day == 4 ? "Friday" : M.Day == 5 ? "Saturday" : "Sunday",
			M.Hour, M.Act, M.Seated, M.Expected, M.Missing.c_str());
	}
	CHECK(WheelGap == 0 && WheelMin > 2000, "the wheel is never left while she is under way (%d of %d minutes with nobody of the watch%s%s)", WheelGap, WheelMin,
		WheelGap ? ", last " : "", WheelWhen.c_str());
	CHECK(BandGap[0] == 0 && BandMin[0] > 100, "Friday's concert: the quartet at the bandstand all through (%d of %d minutes short)", BandGap[0], BandMin[0]);
	CHECK(BandGap[1] == 0 && BandMin[1] > 150, "Saturday's concert: the quartet at the bandstand all through (%d of %d minutes short)", BandGap[1], BandMin[1]);
	CHECK(BandGap[2] == 0 && BandMin[2] > 60, "the captain's dinner: three of the band play (%d of %d minutes short)", BandGap[2], BandMin[2]);
	for (const FRole& R : Roles)
	{
		CHECK(R.Gap == 0 && R.Min > 600, "StaffOnShift(P-SHIP, %s) in its hours (%d of %d minutes with nobody%s%s)", R.Role, R.Gap, R.Min, R.Gap ? ", last " : "", R.When.c_str());
	}
	CHECK(AtRail >= RailOf * 6 / 10 && bHaruto, "Saturday 10:30, Bird Island: most passengers at the rails, Haruto among them (%d of %d)", AtRail, RailOf);
	CHECK(InSaloon * 2 >= SaloonOf && Dancing == 2, "Saturday 21:15, the concert: half the passengers and more in the saloon, Misha and Olya dancing (%d of %d, %d dancing)",
		InSaloon, SaloonOf, Dancing);
	CHECK(Quiz >= 100 && Quiz - 100 == QuizOf && QuizOf >= 8, "Sunday 10:24, the quiz: those who play sit in the saloon, the entertainer leads (%d of %d)", Quiz % 100, QuizOf);
	int ShortNight = 0, ShortSleep = 0;
	std::string ShortWho;
	for (const FWho& X : Ship)
	{
		const FPerson& P = W.People[(size_t)X.Id];
		if (X.bTwin && X.Set != 0)
		{
			continue;
		}
		if (!X.bCrew && !X.bBand && (X.SleepMin[0] < 300 || X.SleepMin[1] < 300))
		{
			ShortNight++;
			ShortWho += " " + P.Key;
		}
		if (X.SleepAll < 600)
		{
			ShortSleep++;
			ShortWho += " " + P.Key;
		}
	}
	CHECK(ShortNight == 0, "every passenger sleeps both nights in their own berth, five hours and more (%d short%s)", ShortNight, ShortWho.c_str());
	CHECK(ShortSleep == 0, "everybody aboard (the watches too) sleeps ten hours and more over the voyage in their own berth (%d short%s)", ShortSleep, ShortWho.c_str());
	CHECK(StillAboard == 0, "Sunday 20:06: everybody has gone ashore (%d still aboard)", StillAboard);
	std::printf("  %ld steps, %.4f ms a step for %d people (%d of them the ship's, %d slots aboard)\n", Steps, Ms / (double)Steps, (int)W.People.size(), (int)Ship.size(), ShipSlots);

	// ---- a Wednesday in season: nobody aboard, the cruising townsfolk in town
	auto Aboard = [&](FWorld& X2, int* TownGone)
	{
		int N = 0;
		for (const FWho& X : Ship)
		{
			N += Here(X2.People[(size_t)X.Id]) ? 1 : 0;
			if (TownGone && X.bTwin)
			{
				*TownGone += !Here(X2.People[(size_t)X.Town]) && X2.People[(size_t)X.Town].Activity == X2.GoneAct ? 1 : 0;
			}
		}
		return N;
	};
	{
		C = FClock::At(2, 12.f);
		C.Set(W);
		W.Resettle();
		int TownGone = 0;
		const int N = Aboard(W, &TownGone);
		CHECK(N == 0 && TownGone == 0, "a Wednesday noon: nobody aboard, the cruising townsfolk all in town (%d aboard, %d away)", N, TownGone);
	}
	// ---- the next week: the other set
	{
		C = FClock::At(5, 13.5f);
		C.Seconds += 7.0 * 24.0 * 3600.0;
		C.Set(W);
		W.Resettle();
		int A = 0, B = 0, TownA = 0, TownB = 0;
		for (const FWho& X : Ship)
		{
			if (X.bTwin)
			{
				const FPerson& P = W.People[(size_t)X.Id];
				const bool bLunch = Here(P) && P.Place == Dining && P.Pose == EPose::Eat;
				(X.Set == 0 ? A : B) += bLunch ? 1 : 0;
				// in town: not away on the stay (somebody may be riding the cable car just now)
				(X.Set == 0 ? TownA : TownB) += W.People[(size_t)X.Town].Activity != W.GoneAct ? 1 : 0;
			}
		}
		CHECK(A == 0 && B == NTwin / 2 && TownA == NTwin / 2 && TownB == 0, "the next Saturday's lunch: the other set of twins at table, the first set in town (%d and %d at table, %d and %d in town)",
			A, B, TownA, TownB);
	}
	// ---- out of season: nobody aboard all the weekend, the townsfolk live their town lives
	{
		W.SetCondition("cruise_season", false);
		C = FClock::At(4, 14.f);
		C.Set(W);
		W.Resettle();
		int MaxAboard = 0, TownGone = 0, AtPier = 0;
		for (int I = 0; I < 5 * 3600; I++)
		{
			C.Seconds += 1.0;
			C.Set(W);
			W.Step(1.f);
			W.Events.clear();
			if (I % 60 == 0)
			{
				MaxAboard = std::max(MaxAboard, Aboard(W, &TownGone));
				for (const FWho& X : Ship)
				{
					AtPier += X.bTwin && W.People[(size_t)X.Town].Activity == BoardAct ? 1 : 0;
				}
			}
		}
		for (const float Hour : { 3.f, 13.5f, 21.f })
		{
			C = FClock::At(5, Hour);
			C.Set(W);
			W.Resettle();
			MaxAboard = std::max(MaxAboard, Aboard(W, &TownGone));
		}
		CHECK(MaxAboard == 0 && TownGone == 0 && AtPier == 0, "out of season (Friday 14:00-19:00 stepped, Saturday resettled): nobody aboard, nobody waits for the launch (%d aboard, %d away, %d at the pier)",
			MaxAboard, TownGone, AtPier);
		int Staff = 0;
		for (const char* R : { "purser", "barman", "steward", "cook", "radio", "captain" })
		{
			Staff += OnShift(W, R);
		}
		CHECK(Staff == 0, "and nobody on shift at her counters (%d)", Staff);
	}
	return Failures > Fails0 ? 1 : 0;
}
