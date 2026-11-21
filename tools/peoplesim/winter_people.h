// Winter play, the townsfolk's side (docs/people.md "Winter play"): the conditions the game switches (rink, rink_busy,
// walrus, banya) and the places of Content/PeopleData/winter_spots.json (the rink on the pond, the banya by the pier, the
// ice hole). Included by peoplesim.cpp: run.sh test runs it on the whole town (run.sh winter alone).
//
// Checked: nobody skates while the rink is shut; on a weekday evening with it open the town comes out to skate, round
// the loop anticlockwise seen from above at 3-4.5 m/s with pauses at its slots, couples and children among them; with
// rink_busy more of them, all day; shut again, they are off the ice within a few minutes; the walruses (a few regulars)
// only in their hour, in the water 1-4 minutes at a time, then on the deck; the banya's visitors unseen inside, more of
// them on Saturday; no slot overbooked. The week with the conditions toggling is town_week.h's (run.sh week).

namespace WinterPeople
{
	/** Whether a point is inside a place's loop (ray casting in X, Y). */
	static bool InLoop(const FPlace& Pl, const V3& P)
	{
		bool bIn = false;
		const size_t N = Pl.Loop.size();
		for (size_t I = 0, J = N - 1; I < N; J = I++)
		{
			const V3& A = Pl.Loop[I];
			const V3& B = Pl.Loop[J];
			if ((A.Y > P.Y) != (B.Y > P.Y) && P.X < (B.X - A.X) * (P.Y - A.Y) / (B.Y - A.Y) + A.X)
			{
				bIn = !bIn;
			}
		}
		return bIn;
	}

	struct FSample
	{
		int Skating = 0;        // at skate / skate_busy on the rink (doing it)
		int Gliding = 0;        // of them going round now
		int Standing = 0;       // of them stopped at a slot (a breath, a word)
		int Any = 0;            // at a skate activity at all (on the way too)
		int OnIce = 0;          // anybody on the rink's ice (inside its loop or off the grid there)
		int Walrus = 0;         // at walrus_swim (doing it)
		int InWater = 0;        // of them in the water (pose walrus)
		int WalrusAny = 0;      // at walrus_swim at all
		int InHole = 0;         // anybody off the grid at the ice hole
		int Banya = 0;          // at banya_visit, there
		int BanyaSeen = 0;      // of them not inside (drawn): must be 0
		int BanyaAny = 0;
	};
}

static int Winter()
{
	using namespace WinterPeople;
	const int Fails0 = Failures;
	FWorld W;
	if (!W.Load(DataDir, AnimalsDir))
	{
		std::printf("load failed: %s\n", W.LastError.c_str());
		return 1;
	}
	W.AssignHomes();
	AttachRail(W);
	const int Rink = W.FindPlace("S-RINK"), Banya = W.FindPlace("S-BANYA"), Hole = W.FindPlace("S-ICEHOLE");
	const int Skate = W.FindActivity("skate"), SkateBusy = W.FindActivity("skate_busy"), Swim = W.FindActivity("walrus_swim"),
		Visit = W.FindActivity("banya_visit");
	CHECK(Rink >= 0 && Banya >= 0 && Hole >= 0, "winter_spots.json's places are loaded (S-RINK %d, S-BANYA %d, S-ICEHOLE %d)", Rink, Banya, Hole);
	CHECK(Skate >= 0 && SkateBusy >= 0 && Swim >= 0 && Visit >= 0, "the winter activities are there (skate, skate_busy, walrus_swim, banya_visit)");
	if (Rink < 0 || Banya < 0 || Hole < 0 || Skate < 0 || SkateBusy < 0 || Swim < 0 || Visit < 0)
	{
		return 1;
	}
	{
		const FPlace& R = W.Places[(size_t)Rink];
		int Reach = 0;
		for (const int S : R.Slots) Reach += W.Slots[(size_t)S].Reach.size() == 2 ? 1 : 0;
		int WaterReach = 0, Water = 0;
		for (const int S : W.Places[(size_t)Hole].Slots)
		{
			Water += W.Slots[(size_t)S].Kind == "walrus" ? 1 : 0;
			WaterReach += W.Slots[(size_t)S].Kind == "walrus" && W.Slots[(size_t)S].Reach.size() == 2 ? 1 : 0;
		}
		float Area = 0.f;
		for (size_t I = 0; I < R.Loop.size(); I++)
		{
			const V3& A = R.Loop[I];
			const V3& B = R.Loop[(I + 1) % R.Loop.size()];
			Area += A.X * B.Y - B.X * A.Y;
		}
		CHECK(R.Loop.size() >= 8 && R.LoopLen > 40.f && Area < 0.f, "the rink's loop: %zu points, %.0f m round, anticlockwise seen from above", R.Loop.size(), R.LoopLen);
		CHECK(Reach == (int)R.Slots.size() && Water == 4 && WaterReach == 4, "every slot on the ice and in the water has its way on from the kerb / the deck (%d of %zu; %d of %d)",
			Reach, R.Slots.size(), WaterReach, Water);
		CHECK(W.Places[(size_t)Banya].bShell, "the banya is indoors: whoever is in is unseen, at its door");
	}

	FClock C;
	int Overbooked = 0;
	auto Sample = [&]()
	{
		FSample S;
		const FPlace& R = W.Places[(size_t)Rink];
		for (const FPerson& P : W.People)
		{
			const bool bSkate = P.Activity == Skate || P.Activity == SkateBusy;
			S.Any += bSkate ? 1 : 0;
			if (bSkate && P.State == EState::Doing && P.Place == Rink)
			{
				S.Skating++;
				S.Gliding += P.LoopAt >= 0.f ? 1 : 0;
				S.Standing += P.LoopAt < 0.f ? 1 : 0;
			}
			// somebody at their own slot of another place (a stroll round the pond whose slot the loop's outline takes in) is not a skater left on the ice
			const bool bElsewhere = P.State == EState::Doing && P.Place >= 0 && P.Place != Rink && !bSkate;
			S.OnIce += P.OffGrid == Rink || (!bElsewhere && P.State != EState::Away && P.Pose != EPose::Hidden && InLoop(R, P.Pos) && std::fabs(P.Pos.Z - R.Loop[0].Z) < 1.5f) ? 1 : 0;
			S.WalrusAny += P.Activity == Swim ? 1 : 0;
			if (P.Activity == Swim && P.State == EState::Doing)
			{
				S.Walrus++;
				S.InWater += P.Pose == EPose::Walrus ? 1 : 0;
			}
			S.InHole += P.OffGrid == Hole ? 1 : 0;
			S.BanyaAny += P.Activity == Visit ? 1 : 0;
			if (P.Activity == Visit && P.Place == Banya && (P.State == EState::Doing || P.State == EState::Settling || P.State == EState::Leaving))
			{
				S.Banya++;
				S.BanyaSeen += W.InsideShell(P) ? 0 : 1;
			}
		}
		return S;
	};
	auto Step = [&](float Dt)
	{
		C.Seconds += Dt;
		C.Set(W);
		W.Step(Dt);
		W.Events.clear();
		for (const FSlot& Sl : W.Slots)
		{
			Overbooked += (int)Sl.Users.size() > Sl.Capacity ? 1 : 0;
		}
	};
	auto Begin = [&](int Weekday, float Hour)
	{
		C = FClock::At(Weekday, Hour);
		C.Set(W);
		W.Resettle();
	};
	const auto T0 = std::chrono::steady_clock::now();
	// LYUD_WINTER_ONLY=evening|saturday|walrus|banya: one part only (debugging)
	const char* Only = std::getenv("LYUD_WINTER_ONLY");
	auto Want = [Only](const char* Part) { return !Only || std::strstr(Only, Part) != nullptr; };

	// children on the ice and people going round beside one of theirs: counted on the weekday evening and the Saturday
	// together (a family's outing is a matter of who happens to come when; one evening alone may well have no pair)
	std::set<int> KidSkaters;
	int PairSeconds = 0;
	auto CountTogether = [&](const FPerson& P, float Seconds)
	{
		if (P.Age < 14) KidSkaters.insert(P.Id);
		if (P.LoopAt >= 0.f && P.Speed > 1.f && P.SkateWith >= 0) PairSeconds += (int)Seconds;
	};
	if (Want("evening"))
	{
	// --- a weekday evening (Wednesday): shut till 17:30, open till 19:30, shut again
	W.SetCondition("rink", false);
	W.SetCondition("rink_busy", false);
	W.SetCondition("walrus", false);
	W.SetCondition("banya", false);
	Begin(2, 15.f);
	int ShutMax = 0;
	for (int I = 0; I < 2.5 * 3600; I++)
	{
		Step(1.f);
		if (I % 60 == 0)
		{
			const FSample S = Sample();
			ShutMax = std::max(ShutMax, S.Any + S.WalrusAny + S.BanyaAny);
		}
	}
	CHECK(ShutMax == 0, "the rink, the ice hole and the banya shut: nobody goes skating, swimming or to the banya (at most %d)", ShutMax);
	CHECK(W.IsCondition("rink") == false && W.FindActivity("skate") >= 0, "(conditions start off)");
	W.SetCondition("rink", true);
	CHECK(W.IsCondition("rink"), "SetCondition / IsCondition: the rink is open");
	int EveMax = 0, EveSum = 0, EveN = 0, StandSeen = 0, Wrong = 0, Glides = 0, Kids = 0, Pairs = 0;
	float SpeedMax = 0.f, SpeedSum = 0.f;
	int SpeedN = 0;
	std::set<int> EveSkaters;
	std::vector<float> Cruise;
	const FPlace& R = W.Places[(size_t)Rink];
	V3 Mid;
	for (const V3& L : R.Loop) Mid += L * (1.f / (float)R.Loop.size());
	for (int I = 0; I < 2 * 3600; I++)
	{
		Step(1.f);
		for (const FPerson& P : W.People)
		{
			if ((P.Activity != Skate && P.Activity != SkateBusy) || P.State != EState::Doing)
			{
				continue;
			}
			EveSkaters.insert(P.Id);
			CountTogether(P, 1.f);
			if (P.LoopAt >= 0.f && P.Speed > 1.f)
			{
				// round anticlockwise seen from above: clockwise in the X, Y plane (the middle on the right of the heading there)
				const V3 Rad = P.Pos - Mid;
				const float Hx = std::cos(P.Yaw * 0.01745329f), Hy = std::sin(P.Yaw * 0.01745329f);
				Wrong += Rad.X * Hy - Rad.Y * Hx > 0.f ? 1 : 0;
				Glides++;
				SpeedMax = std::max(SpeedMax, P.Speed);
				if (!P.bLoopStop && P.SkateWith < 0 && P.LoopRun > 10.f)
				{
					SpeedSum += P.Speed;
					SpeedN++;
				}
				Pairs += P.SkateWith >= 0 ? 1 : 0;
			}
		}
		if (I % 60 == 0 && I >= 30 * 60)
		{
			const FSample S = Sample();
			EveMax = std::max(EveMax, S.Skating);
			EveSum += S.Skating;
			EveN++;
			StandSeen += S.Standing;
		}
	}
	for (const int Id : EveSkaters) Kids += W.People[(size_t)Id].Age < 14 ? 1 : 0;
	const float EveMean = EveN ? (float)EveSum / EveN : 0.f;
	const float Mean = SpeedN ? SpeedSum / SpeedN : 0.f;
	CHECK(EveMax >= 6 && EveMax <= 24, "Wednesday 18:00-19:30, the rink open: people skate (%.1f on the ice on average, %d at most, %zu in all)", EveMean, EveMax, EveSkaters.size());
	CHECK(Glides > 0 && Wrong == 0, "they go round anticlockwise seen from above (%d of %d skater-seconds the wrong way)", Wrong, Glides);
	CHECK(Mean >= 3.f && Mean <= 4.5f && SpeedMax <= 4.5f * 1.15f + 0.01f, "at 3-4.5 m/s (%.2f on average gliding on their own, %.2f at most)", Mean, SpeedMax);
	CHECK(StandSeen > 0, "with pauses at the slots: a breath, a word (%d skater-minutes standing)", StandSeen);
	std::printf("       (this evening: %d children on the ice, %d skater-seconds beside one of theirs)\n", Kids, Pairs);
	// shut at 19:30: off the ice within a few minutes
	W.SetCondition("rink", false);
	int At75 = -1, OnIce3 = -1, OnIce5 = -1, Any5 = -1;
	for (int I = 1; I <= 5 * 60; I++)
	{
		Step(1.f);
		const FSample S = Sample();
		if (I == 75) At75 = S.Skating;
		if (I == 180) OnIce3 = S.OnIce;
		if (I == 300)
		{
			OnIce5 = S.OnIce;
			for (const FPerson& P : W.People)
			{
				if (std::getenv("LYUD_WINTER_DEBUG") && (P.OffGrid == Rink || (P.State != EState::Away && P.Pose != EPose::Hidden && InLoop(W.Places[(size_t)Rink], P.Pos))))
				{
					std::printf("        DEBUG on the ice: %s %s %s at %s %s offgrid %d pos %.1f %.1f %.1f\n", P.Key.c_str(), StateName(P.State),
						P.Activity >= 0 ? W.Activities[P.Activity].Id.c_str() : "-", P.Place >= 0 ? W.Places[P.Place].Id.c_str() : "-", P.Why.c_str(), P.OffGrid, P.Pos.X, P.Pos.Y, P.Pos.Z);
				}
			}
			Any5 = S.Any;
		}
	}
	CHECK(At75 == 0, "the rink shut at 19:30: nobody skates on a minute later (%d at 19:31:15)", At75);
	CHECK(OnIce3 <= 2 && OnIce5 == 0 && Any5 == 0, "... and they walk off the ice: %d still on it at 19:33, %d at 19:35 (%d at a skate activity)", OnIce3, OnIce5, Any5);

	}
	if (Want("saturday"))
	{
	// --- Saturday: the rink open from the morning, then a festival day (rink_busy) the same hours
	float SatMean[2] = { 0.f, 0.f };
	int SatMin[2] = { 1 << 30, 1 << 30 }, Busy = 0;
	for (int Pass = 0; Pass < 2; Pass++)
	{
		W.SetCondition("rink", true);
		W.SetCondition("rink_busy", Pass == 1);
		Begin(5, 9.5f);
		int Sum = 0, N = 0;
		for (int I = 0; I < 7 * 1800; I++)
		{
			Step(2.f);
			if (I % 30 == 0 && C.Seconds >= (5 * 24 + 10.5) * 3600.0)
			{
				const FSample S = Sample();
				Sum += S.Skating;
				if (std::getenv("LYUD_WINTER_DEBUG"))
				{
					std::printf("        DEBUG Saturday %s %05.2f: %d skating, %d at it\n", Pass ? "busy" : "open", std::fmod(C.Seconds / 3600.0, 24.0), S.Skating, S.Any);
				}
				N++;
				// (on the hour from noon: the rink's weekend hours begin at 11)
				const int Min = (int)std::lround(std::fmod(C.Seconds / 3600.0, 24.0) * 60.0);
				if (Min % 60 == 0 && Min >= 12 * 60)
				{
					SatMin[Pass] = std::min(SatMin[Pass], S.Skating);
				}
				if (Pass == 1)
				{
					for (const FPerson& P : W.People) Busy += P.Activity == SkateBusy && P.State == EState::Doing ? 1 : 0;
				}
				for (const FPerson& P : W.People)
				{
					if ((P.Activity == Skate || P.Activity == SkateBusy) && P.State == EState::Doing)
					{
						CountTogether(P, 60.f);     // a sample every 30 steps of 2 s
					}
				}
			}
		}
		SatMean[Pass] = N ? (float)Sum / N : 0.f;
	}
	CHECK(SatMean[0] >= 4.f, "Saturday 10:30-16:30, the rink open: skaters from the morning (%.1f on average, at least %d on the hour from noon)", SatMean[0], SatMin[0]);
	CHECK(SatMean[1] > SatMean[0] * 1.25f && SatMin[1] >= 5 && Busy > 0, "... the first-snow festival (rink_busy): more of them, all day (%.1f on average, at least %d on the hour from noon)",
		SatMean[1], SatMin[1]);
	W.SetCondition("rink", false);
	W.SetCondition("rink_busy", false);

	}
	if (Want("evening") || Want("saturday"))
	{
	CHECK(!KidSkaters.empty() && PairSeconds > 0, "children among the skaters (%zu of them) and some go round beside one of theirs (%d skater-seconds), the evening and the Saturday together",
		KidSkaters.size(), PairSeconds);
	}
	if (Want("walrus"))
	{
	// --- Sunday morning: the walruses' hour 8:00-10:00
	Begin(6, 7.f);
	int EarlyWalrus = 0;
	for (int I = 0; I < 3600; I++)
	{
		Step(1.f);
		if (I % 60 == 0) EarlyWalrus = std::max(EarlyWalrus, Sample().WalrusAny);
	}
	W.SetCondition("walrus", true);
	std::set<int> Walruses;
	std::map<int, double> InSince, VisitSince;
	double DipMin = 1e9, DipMax = 0.0, VisitMin = 1e9, VisitMax = 0.0;
	int WaterMax = 0, OffSlot = 0, Deck = 0;
	for (int I = 0; I < 2 * 3600; I++)
	{
		Step(1.f);
		for (const FPerson& P : W.People)
		{
			const bool bIn = P.Activity == Swim && P.State == EState::Doing && P.Pose == EPose::Walrus && P.Path.empty();
			if (P.Activity == Swim && P.State == EState::Doing)
			{
				Walruses.insert(P.Id);
				if (!VisitSince.count(P.Id)) VisitSince[P.Id] = W.Time;
				Deck += P.Pose == EPose::Stand && W.Slots[(size_t)P.Slot].Kind == "stand" ? 1 : 0;
				OffSlot += bIn && W.Slots[(size_t)P.Slot].Kind != "walrus" ? 1 : 0;
			}
			else if (VisitSince.count(P.Id))
			{
				const double V = W.Time - VisitSince[P.Id];
				if (std::getenv("LYUD_WINTER_DEBUG") && V < 13 * 60.0) std::printf("        DEBUG visit %s %.0f s: now %s %s %s\n", P.Key.c_str(), V, StateName(P.State), P.Activity >= 0 ? W.Activities[P.Activity].Id.c_str() : "-", P.Why.c_str());
				VisitMin = std::min(VisitMin, V);
				VisitMax = std::max(VisitMax, V);
				VisitSince.erase(P.Id);
			}
			if (bIn && !InSince.count(P.Id))
			{
				InSince[P.Id] = W.Time;
			}
			else if (!bIn && InSince.count(P.Id))
			{
				const double D = W.Time - InSince[P.Id];
				if (std::getenv("LYUD_WINTER_DEBUG") && (D < 55.0 || D > 245.0)) std::printf("        DEBUG dip %s %.0f s: %s %s %s pose %s dips %d until %.0f\n", P.Key.c_str(), D, StateName(P.State), P.Activity >= 0 ? W.Activities[P.Activity].Id.c_str() : "-", P.Why.c_str(), PoseName(P.Pose), P.Dips, P.Until - W.Time);
				if (P.Activity == Swim)
				{
					DipMin = std::min(DipMin, D);
					DipMax = std::max(DipMax, D);
				}
				InSince.erase(P.Id);
			}
		}
		if (I % 30 == 0) WaterMax = std::max(WaterMax, Sample().InWater);
	}
	CHECK(EarlyWalrus == 0, "Sunday 7:00-8:00, before the walruses' hour: nobody at the ice hole (%d)", EarlyWalrus);
	CHECK(Walruses.size() >= 3 && Walruses.size() <= 20, "the walruses' hour 8:00-10:00: a handful of regulars come (%zu)", Walruses.size());
	CHECK(WaterMax >= 1 && WaterMax <= 4 && OffSlot == 0, "in the water at the hole's four places, up to the shoulders (%d at most at once)", WaterMax);
	CHECK(DipMin >= 55.0 && DipMax <= 245.0, "a dip lasts 1-4 minutes (%.0f-%.0f s)", DipMin, DipMax);
	CHECK(Deck > 0 && VisitMin >= 13.0 * 60.0 - 1.0 && VisitMax <= 40.0 * 60.0, "then on the deck rubbing down; 15-35 minutes in all, a little more after waiting for room in the water (%.0f-%.0f min seen whole, %d person-seconds on the deck)",
		VisitMin / 60.0, VisitMax / 60.0, Deck);
	std::string Who;
	for (const int Id : Walruses) Who += (Who.empty() ? "" : " ") + W.People[(size_t)Id].Key;
	std::printf("        the walruses: %s\n", Who.c_str());
	W.SetCondition("walrus", false);
	int Water90 = -1, Swim180 = -1, Hole180 = -1;
	for (int I = 1; I <= 180; I++)
	{
		Step(1.f);
		if (I == 90) Water90 = Sample().InWater;
		if (I == 180)
		{
			Swim180 = Sample().WalrusAny;
			Hole180 = Sample().InHole;
		}
	}
	CHECK(Water90 == 0 && Hole180 == 0 && Swim180 == 0, "the hour over at 10:00: out of the water within a minute and a half (%d), gone within three (%d at it, %d in the hole)",
		Water90, Swim180, Hole180);

	}
	if (Want("banya"))
	{
	// --- the banya open 10:00-21:00: a Wednesday and a Saturday
	W.SetCondition("banya", true);
	int Visits[2] = { 0, 0 }, BanyaMax = 0, Seen = 0;
	for (int Pass = 0; Pass < 2; Pass++)
	{
		Begin(Pass == 0 ? 2 : 5, 10.f);
		std::set<int> Inside;
		for (int I = 0; I < 11 * 1800; I++)
		{
			Step(2.f);
			for (const FPerson& P : W.People)
			{
				if (P.Activity == Visit && P.State == EState::Doing && P.Place == Banya && !Inside.count(P.Id))
				{
					Inside.insert(P.Id);
					Visits[Pass]++;
				}
			}
			if (I % 30 == 0)
			{
				const FSample S = Sample();
				BanyaMax = std::max(BanyaMax, S.Banya);
				Seen += S.BanyaSeen;
			}
		}
	}
	CHECK(Visits[0] > 0 && Seen == 0, "the banya heated: people go (%d on a Wednesday), unseen inside (%d person-minutes drawn)", Visits[0], Seen);
	CHECK(Visits[1] * 10 >= Visits[0] * 13 && BanyaMax <= 12, "Saturday the busiest (%d), never more than its shelves hold (%d at once)", Visits[1], BanyaMax);
	W.SetCondition("banya", false);
	}
	CHECK(Overbooked == 0, "winter play: no slot overbooked (%d slot-seconds)", Overbooked);
	std::printf("  winter play's checks took %.1f s\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count());
	return Failures > Fails0 ? 1 : 0;
}
