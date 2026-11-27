// The dances at the hall «Своды», the townsfolk's side (docs/dances.md): the place P-HALL-DANCE (pairs of standing slots on
// the dance room's floor, seats round the little tables and the lobby's benches) and the activities dance_evening (15 to 45)
// and dance_watch (55 and over), chosen on Friday and Saturday evenings only. Included by peoplesim.cpp: run.sh dances.
//
// Checked: a Friday evening fills the floor (dancers standing at the floor's slots, old ones sitting to watch, some of the
// twelve partners the hero may ask among them); nobody is there before seven or after the watchman's eleven; nobody too
// young or too old dances; a Saturday fills it too; a Wednesday evening it stays empty; no slot overbooked.

static int Dances()
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
	std::printf("the dances (the Vaults' dance room, Friday and Saturday evenings):\n");
	const int Floor = W.FindPlace("P-HALL-DANCE");
	const int Dance = W.FindActivity("dance_evening");
	const int Watch = W.FindActivity("dance_watch");
	CHECK(Floor >= 0 && Dance >= 0 && Watch >= 0, "the dance room's place and activities are in the town (P-HALL-DANCE %d, dance_evening %d, dance_watch %d)", Floor, Dance, Watch);
	if (Floor < 0 || Dance < 0 || Watch < 0)
	{
		return 1;
	}
	// the partners: whoever has a like for dancing (tools/town/town_dances.py gives it to the twelve of x_dances.json)
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
	CHECK(Partners.size() == 12, "twelve partners like dancing (%zu)", Partners.size());

	FClock C;
	int Overbooked = 0;
	auto Step = [&](float Dt)
	{
		C.Seconds += Dt;
		C.Set(W);
		W.Step(Dt);
		W.Events.clear();
		for (const int S : W.Places[(size_t)Floor].Slots)
		{
			Overbooked += (int)W.Slots[(size_t)S].Users.size() > W.Slots[(size_t)S].Capacity ? 1 : 0;
		}
	};
	auto Begin = [&](int Weekday, float Hour)
	{
		C = FClock::At(Weekday, Hour);
		C.Set(W);
		W.Resettle();
	};
	struct FCount { int Dancing = 0, Watching = 0, Partners = 0, Going = 0, WrongAge = 0, WrongPose = 0; };
	auto Count = [&]()
	{
		FCount N;
		for (const FPerson& P : W.People)
		{
			const bool bOurs = P.Activity == Dance || P.Activity == Watch;
			N.Going += bOurs ? 1 : 0;
			if (!bOurs || P.Place != Floor || P.State != EState::Doing)
			{
				continue;
			}
			if (P.Activity == Dance)
			{
				N.Dancing++;
				N.Partners += Partners.count(P.Key) ? 1 : 0;
				N.WrongAge += P.Age < 15 || P.Age > 45 ? 1 : 0;
				N.WrongPose += P.Pose == EPose::Stand ? 0 : 1;
			}
			else
			{
				N.Watching++;
				N.WrongAge += P.Age < 55 ? 1 : 0;
				N.WrongPose += P.Pose == EPose::Sit ? 0 : 1;
			}
		}
		return N;
	};
	auto Evening = [&](int Weekday, const char* Name, bool bExpect)
	{
		Begin(Weekday, 17.5f);
		int Early = 0, PeakDance = 0, PeakWatch = 0, PeakPartners = 0, Late = 0, WrongAge = 0, WrongPose = 0;
		for (int I = 0; I < (int)(6.5f * 3600.f); I++)
		{
			const float Hour = 17.5f + I / 3600.f;
			Step(1.f);
			if (I % 30 != 0)
			{
				continue;
			}
			const FCount N = Count();
			WrongAge = std::max(WrongAge, N.WrongAge);
			WrongPose = std::max(WrongPose, N.WrongPose);
			if (Hour < 18.9f)
			{
				Early = std::max(Early, N.Going);
			}
			else if (Hour < 23.f)
			{
				PeakDance = std::max(PeakDance, N.Dancing);
				PeakWatch = std::max(PeakWatch, N.Watching);
				PeakPartners = std::max(PeakPartners, N.Partners);
			}
			else
			{
				Late = std::max(Late, N.Dancing + N.Watching);
			}
		}
		CHECK(Early == 0, "%s: nobody sets off for the dance before seven (%d)", Name, Early);
		if (bExpect)
		{
			CHECK(PeakDance >= 8, "%s: the floor fills, at most %d dancing at once (at least 8)", Name, PeakDance);
			CHECK(PeakWatch >= 3, "%s: old ones watch from the tables and benches, at most %d at once (at least 3)", Name, PeakWatch);
			CHECK(PeakPartners >= 2, "%s: some of the hero's partners are there, at most %d at once (at least 2)", Name, PeakPartners);
		}
		else
		{
			CHECK(PeakDance + PeakWatch == 0, "%s: nobody comes, the dance is Friday's and Saturday's (%d, %d)", Name, PeakDance, PeakWatch);
		}
		CHECK(Late == 0, "%s: after the watchman's eleven nobody is left in the dance room (%d)", Name, Late);
		CHECK(WrongAge == 0, "%s: the dancers are 15 to 45 and the watchers 55 and over (%d not)", Name, WrongAge);
		CHECK(WrongPose == 0, "%s: the dancers stand, the watchers sit (%d not)", Name, WrongPose);
		std::printf("    %s: up to %d dancing, %d watching, %d partners\n", Name, PeakDance, PeakWatch, PeakPartners);
	};
	Evening(4, "Friday", true);
	Evening(5, "Saturday", true);
	Evening(2, "Wednesday", false);
	CHECK(Overbooked == 0, "no slot of the dance room overbooked (%d)", Overbooked);
	return Failures > Fails0 ? 1 : 0;
}
