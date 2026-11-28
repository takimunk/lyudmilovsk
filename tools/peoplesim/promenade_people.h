// The warm season's evenings on Sun Square and the promenade, the townsfolk's side (docs/promenade.md): the places of
// tools/town/town_promenade.py (P-PROM-DANCE, P-PROM-BAND, S-PROM-EVENING) and its activities, switched by the conditions
// ULyudStreetLife works out (StreetLifeModel.h: prom_dances, prom_band, prom_evening, hall_dances). Included by peoplesim.cpp:
// run.sh promenade.
//
// Checked, each scene with its conditions switched as the game would, on the real town:
//   a July Friday evening (prom_dances 19-23, prom_evening 18:30-22:30, hall_dances off): the square's floor fills (dancers
//     standing at the pairs' spots, older ones watching round it, some of the hero's partners among them), nobody there
//     before seven or after eleven, the ages right, the hall's dance room empty;
//   the same Friday with the dance called off by the forecast (prom_dances never on): nobody dances on the square;
//   a July Sunday afternoon (prom_band 15-17): the band's eight on the stage, the town in front of it, children running about,
//     everybody gone soon after five;
//   a warm Wednesday evening (prom_evening): people on the evening round, the promenade's benches fuller than on the same
//     evening with the condition off;
//   no slot of the three places overbooked.

static int Promenade()
{
	const int Fails0 = Failures;
	FWorld W;
	if (!W.Load(DataDir, AnimalsDir))
	{
		std::printf("load failed: %s\n", W.LastError.c_str());
		return 1;
	}
	W.AssignHomes();
	AttachRail(W);
	std::printf("the promenade's summer evenings (docs/promenade.md):\n");
	const int Floor = W.FindPlace("P-PROM-DANCE"), Band = W.FindPlace("P-PROM-BAND"), Round = W.FindPlace("S-PROM-EVENING");
	const int Stage = W.FindPlace("S-STAGE"), Hall = W.FindPlace("P-HALL-DANCE");
	const int Dance = W.FindActivity("prom_dance"), Watch = W.FindActivity("prom_dance_watch"), Play = W.FindActivity("band_play");
	const int Listen = W.FindActivity("band_listen"), Kids = W.FindActivity("band_kids"), Walk = W.FindActivity("prom_evening_walk");
	const int Bench = W.FindActivity("prom_sunset_bench"), HallDance = W.FindActivity("dance_evening");
	CHECK(Floor >= 0 && Band >= 0 && Round >= 0 && Stage >= 0 && Hall >= 0, "the places are in the town (P-PROM-DANCE %d, P-PROM-BAND %d, S-PROM-EVENING %d, S-STAGE %d, P-HALL-DANCE %d)",
		Floor, Band, Round, Stage, Hall);
	CHECK(Dance >= 0 && Watch >= 0 && Play >= 0 && Listen >= 0 && Kids >= 0 && Walk >= 0 && Bench >= 0 && HallDance >= 0, "the activities are in the town");
	if (Floor < 0 || Band < 0 || Round < 0 || Stage < 0 || Hall < 0 || Dance < 0 || Watch < 0 || Play < 0 || Listen < 0 || Kids < 0 || Walk < 0 || Bench < 0)
	{
		return 1;
	}
	// the hero's partners (town_dances.py gives them the like for the hall's dances; the square's floor is the same girls')
	std::set<std::string> Partners;
	{
		std::ifstream F(std::string(DataDir) + "/townsfolk.json");
		const std::string Text((std::istreambuf_iterator<char>(F)), std::istreambuf_iterator<char>());
		size_t At = 0;
		while ((At = Text.find("\"dance_evening\"", At)) != std::string::npos)
		{
			const size_t K = Text.rfind("\"key\": \"", At);
			if (K != std::string::npos)
			{
				const size_t B = K + 8;
				Partners.insert(Text.substr(B, Text.find('"', B) - B));
			}
			At += 15;
		}
	}
	// the promenade's own benches (the plan's by the town, the layout's by the sea)
	std::set<int> PromBenches;
	for (int Pi = 0; Pi < (int)W.Places.size(); Pi++)
	{
		const std::string& Id = W.Places[(size_t)Pi].Id;
		if (Id.rfind("S-BENCH-P", 0) == 0 && Id.rfind("S-BENCH-PK", 0) != 0 && Id.rfind("S-BENCH-PR", 0) != 0)
		{
			PromBenches.insert(Pi);
		}
		if (Id.rfind("S-BENCH-R", 0) == 0)
		{
			PromBenches.insert(Pi);
		}
	}
	CHECK(PromBenches.size() >= 40, "the promenade's benches are places (%zu)", PromBenches.size());

	const char* Conds[] = { "prom_evening", "prom_dances", "prom_band", "hall_dances" };
	auto AllOff = [&]() { for (const char* K : Conds) W.SetCondition(K, false); };
	FClock C;
	int Overbooked = 0;
	auto Step = [&](float Dt)
	{
		C.Seconds += Dt;
		C.Set(W);
		W.Step(Dt);
		W.Events.clear();
		for (const int Pl : { Floor, Band, Round })
		{
			for (const int S : W.Places[(size_t)Pl].Slots)
			{
				Overbooked += (int)W.Slots[(size_t)S].Users.size() > W.Slots[(size_t)S].Capacity ? 1 : 0;
			}
		}
	};
	auto Begin = [&](int Weekday, float Hour)
	{
		C = FClock::At(Weekday, Hour);
		C.Set(W);
		W.Resettle();
	};
	auto Doing = [&](const FPerson& P, int Act, int Place) { return P.Activity == Act && (Place < 0 || P.Place == Place) && P.State == EState::Doing; };

	// ---- a July Friday evening: the dance on the square
	auto Friday = [&](const char* Name, bool bDance)
	{
		AllOff();
		W.TempC = 20.f;
		Begin(4, 17.5f);
		int Early = 0, PeakDance = 0, PeakWatch = 0, PeakPartners = 0, Late = 0, WrongAge = 0, WrongPose = 0, InHall = 0;
		for (int I = 0; I < (int)(6.5f * 3600.f); I++)
		{
			const float Hour = 17.5f + I / 3600.f;
			W.SetCondition("prom_evening", Hour >= 18.5f && Hour < 22.5f);
			W.SetCondition("prom_dances", bDance && Hour >= 19.f && Hour < 23.f);
			Step(1.f);
			if (I % 30 != 0)
			{
				continue;
			}
			int D = 0, Wa = 0, Pa = 0, Going = 0;
			for (const FPerson& P : W.People)
			{
				Going += P.Activity == Dance || P.Activity == Watch ? 1 : 0;
				InHall += Doing(P, HallDance, Hall) ? 1 : 0;
				if (Doing(P, Dance, Floor))
				{
					D++;
					Pa += Partners.count(P.Key) ? 1 : 0;
					WrongAge += P.Age < 15 || P.Age > 45 ? 1 : 0;
					WrongPose += P.Pose == EPose::Stand ? 0 : 1;
				}
				else if (Doing(P, Watch, Floor))
				{
					Wa++;
					WrongAge += P.Age < 46 ? 1 : 0;
				}
			}
			if (Hour < 18.95f)
			{
				Early = std::max(Early, Going);
			}
			else if (Hour < 23.f)
			{
				PeakDance = std::max(PeakDance, D);
				PeakWatch = std::max(PeakWatch, Wa);
				PeakPartners = std::max(PeakPartners, Pa);
			}
			else if (Hour > 23.05f)
			{
				Late = std::max(Late, D + Wa);
			}
		}
		CHECK(Early == 0, "%s: nobody sets off for the square's dance before seven (%d)", Name, Early);
		if (bDance)
		{
			CHECK(PeakDance >= 10, "%s: the square's floor fills, at most %d dancing at once (at least 10)", Name, PeakDance);
			CHECK(PeakWatch >= 3, "%s: older ones watch round the floor, at most %d at once (at least 3)", Name, PeakWatch);
			CHECK(PeakPartners >= 2, "%s: some of the hero's partners dance there, at most %d at once (at least 2)", Name, PeakPartners);
		}
		else
		{
			CHECK(PeakDance + PeakWatch == 0, "%s: the dance is called off, nobody on the floor (%d, %d)", Name, PeakDance, PeakWatch);
		}
		CHECK(Late == 0, "%s: after eleven nobody is left on the floor (%d)", Name, Late);
		CHECK(WrongAge == 0 && WrongPose == 0, "%s: the dancers are 15 to 45 and stand, the watchers 46 and over (%d, %d not)", Name, WrongAge, WrongPose);
		CHECK(InHall == 0, "%s: the hall's dance room stays empty in July (%d)", Name, InHall);
		std::printf("    %s: up to %d dancing, %d watching, %d partners\n", Name, PeakDance, PeakWatch, PeakPartners);
	};
	Friday("a July Friday", true);
	Friday("a July Friday called off for rain", false);

	// ---- a July Sunday afternoon: the brass band
	{
		AllOff();
		W.TempC = 23.f;
		Begin(6, 13.5f);
		int PeakPlay = 0, PeakListen = 0, PeakKids = 0, After = 0, Early = 0, YoungListen = 0;
		double ListenMin = 0.0;
		for (int I = 0; I < (int)(4.5f * 3600.f); I++)
		{
			const float Hour = 13.5f + I / 3600.f;
			W.SetCondition("prom_band", Hour >= 15.f && Hour < 17.f);
			Step(1.f);
			if (I % 30 != 0)
			{
				continue;
			}
			int Pl = 0, Li = 0, Ki = 0;
			for (const FPerson& P : W.People)
			{
				Pl += Doing(P, Play, Band) ? 1 : 0;
				if (Doing(P, Listen, Band))
				{
					Li++;
					YoungListen += P.Age < 13 ? 1 : 0;
				}
				Ki += P.Activity == Kids && P.Place == Band ? 1 : 0;
			}
			if (Hour < 14.95f)
			{
				Early = std::max(Early, Pl + Li + Ki);
			}
			else if (Hour < 17.f)
			{
				PeakPlay = std::max(PeakPlay, Pl);
				PeakListen = std::max(PeakListen, Li);
				PeakKids = std::max(PeakKids, Ki);
				ListenMin += Li * 0.5;
			}
			else if (Hour > 17.1f)
			{
				After = std::max(After, Pl + Li + Ki);
			}
		}
		CHECK(Early == 0, "the band's Sunday: nobody at the stage before three (%d)", Early);
		CHECK(PeakPlay >= 6, "the band's Sunday: the band on the stage, at most %d playing at once (at least 6 of 8)", PeakPlay);
		CHECK(PeakListen >= 15, "the band's Sunday: the town stands in front of the stage, at most %d listening at once (at least 15)", PeakListen);
		CHECK(PeakKids >= 3, "the band's Sunday: children run about behind them, at most %d at once (at least 3)", PeakKids);
		CHECK(YoungListen == 0, "the band's Sunday: the listeners are 13 and over (%d not)", YoungListen);
		CHECK(After == 0, "the band's Sunday: everybody gone soon after five (%d)", After);
		std::printf("    the band's Sunday: %d playing, up to %d listening (%.0f person-minutes), %d children\n", PeakPlay, PeakListen, ListenMin, PeakKids);
	}

	// ---- a warm Wednesday evening: the promenade, then the same evening without it
	auto Evening = [&](bool bOn, double& WalkMin, double& BenchMin, int& PeakWalk)
	{
		AllOff();
		W.TempC = 19.f;
		Begin(2, 17.5f);
		WalkMin = BenchMin = 0.0;
		PeakWalk = 0;
		for (int I = 0; I < (int)(5.5f * 3600.f); I++)
		{
			const float Hour = 17.5f + I / 3600.f;
			W.SetCondition("prom_evening", bOn && Hour >= 18.5f && Hour < 22.5f);
			Step(1.f);
			if (I % 30 != 0 || Hour < 19.f || Hour > 22.f)
			{
				continue;
			}
			int Wk = 0, Bn = 0;
			for (const FPerson& P : W.People)
			{
				Wk += P.Activity == Walk || (P.Activity >= 0 && P.Place == Round) ? 1 : 0;
				Bn += P.State == EState::Doing && P.Pose == EPose::Sit && PromBenches.count(P.Place) ? 1 : 0;
			}
			PeakWalk = std::max(PeakWalk, Wk);
			WalkMin += Wk * 0.5;
			BenchMin += Bn * 0.5;
		}
	};
	double WalkOn, BenchOn, WalkOff, BenchOff;
	int PeakOn, PeakOff;
	Evening(true, WalkOn, BenchOn, PeakOn);
	Evening(false, WalkOff, BenchOff, PeakOff);
	CHECK(PeakOn >= 30, "a warm evening: the promenade's round is walked, at most %d at once (at least 30)", PeakOn);
	CHECK(PeakOff == 0 || WalkOff < WalkOn * 0.2, "a cold evening (the condition off): the round all but empty (%.0f against %.0f person-minutes)", WalkOff, WalkOn);
	CHECK(BenchOn > BenchOff * 1.2, "a warm evening: the promenade's benches fill (%.0f against %.0f person-minutes, 19-22)", BenchOn, BenchOff);
	std::printf("    a warm Wednesday: up to %d on the evening round (%.0f person-minutes), the promenade's benches %.0f against %.0f\n", PeakOn, WalkOn, BenchOn, BenchOff);
	CHECK(Overbooked == 0, "no slot of the square's floor, the band's rows or the evening round overbooked (%d)", Overbooked);
	return Failures > Fails0 ? 1 : 0;
}
