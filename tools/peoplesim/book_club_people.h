// The library's book club, the townsfolk's side (docs/books.md "The book club"): the place P-LIBRARY-CLUB (five seats round
// the library lounge's table, each a member's own) and the activity book_club, chosen only while the game's condition
// book_club is on (ULyudBookClub switches it on Thursdays 18:30-20:00, never on a festival day or a public holiday).
// Included by peoplesim.cpp: run.sh bookclub (also part of run.sh town).
//
// Checked: on a Thursday evening with the condition on, four or five members sit at their own seats by 19:15 and nobody
// else is there; off at 20:00, they are gone within a few minutes; a Thursday with the condition kept off (a festival) the
// seats stay empty; a Wednesday evening with it switched on (it never is, but) nobody comes: the activity is Thursday's.

static int BookClub()
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
	std::printf("the book club (the library's lounge, Thursday evenings):\n");
	const int Club = W.FindPlace("P-LIBRARY-CLUB");
	const int Act = W.FindActivity("book_club");
	CHECK(Club >= 0 && Act >= 0, "the club's place and activity are in the town (P-LIBRARY-CLUB %d, book_club %d)", Club, Act);
	if (Club < 0 || Act < 0)
	{
		return 1;
	}
	std::set<std::string> Members;
	for (const int S : W.Places[(size_t)Club].Slots)
	{
		if (!W.Slots[(size_t)S].OwnerKey.empty())
		{
			Members.insert(W.Slots[(size_t)S].OwnerKey);
		}
	}
	CHECK(Members.size() == 5 && W.Places[(size_t)Club].Slots.size() == 5, "five seats, each a member's own (%zu members)", Members.size());

	FClock C;
	int Overbooked = 0;
	auto Step = [&](float Dt)
	{
		C.Seconds += Dt;
		C.Set(W);
		W.Step(Dt);
		W.Events.clear();
		for (const int S : W.Places[(size_t)Club].Slots)
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
	struct FCount { int Seated = 0, Strangers = 0, Going = 0; };
	auto Count = [&]()
	{
		FCount N;
		for (const FPerson& P : W.People)
		{
			if (P.Activity == Act)
			{
				N.Going++;
			}
			if (P.Place == Club && P.State == EState::Doing)
			{
				N.Seated += P.Activity == Act && P.Pose == EPose::Sit ? 1 : 0;
				N.Strangers += Members.count(P.Key) ? 0 : 1;
			}
		}
		return N;
	};
	// ---- a Thursday: the condition on at 18:10, off at 20:00 (as ULyudBookClub does)
	W.SetCondition("book_club", false);
	Begin(3, 16.f);
	int Early = 0, At1915 = -1, Strangers = 0, After = -1, PeakSeated = 0;
	for (int I = 0; I < (int)(4.5f * 3600.f); I++)
	{
		const float Hour = 16.f + I / 3600.f;
		W.SetCondition("book_club", Hour >= 18.17f && Hour < 20.f);
		Step(1.f);
		if (I % 30 != 0)
		{
			continue;
		}
		const FCount N = Count();
		Strangers = std::max(Strangers, N.Strangers);
		PeakSeated = std::max(PeakSeated, N.Seated);
		if (Hour < 18.15f)
		{
			Early = std::max(Early, N.Going);
		}
		if (At1915 < 0 && Hour >= 19.25f)
		{
			if (std::getenv("LYUD_CLUB_DEBUG"))
				for (const FPerson& P : W.People)
					if (Members.count(P.Key))
						std::printf("    %s act %s state %d place %d\n", P.Key.c_str(), P.Activity >= 0 ? W.Activities[(size_t)P.Activity].Id.c_str() : "-", (int)P.State, P.Place);
			At1915 = N.Seated;
		}
		if (After < 0 && Hour >= 20.15f)
		{
			After = N.Going;
		}
	}
	CHECK(Early == 0, "nobody sets off before the club opens (%d)", Early);
	CHECK(At1915 >= 4, "Thursday 19:15: the members sit at their seats round the table (%d of 5; at most %d at once)", At1915, PeakSeated);
	CHECK(Strangers == 0, "nobody but the members sits there (%d)", Strangers);
	CHECK(After == 0, "20:09, the club over: everybody has gone (%d still at it)", After);
	// ---- a festival Thursday: the condition stays off
	W.SetCondition("book_club", false);
	Begin(3, 18.f);
	int FestMax = 0;
	for (int I = 0; I < 2 * 3600; I++)
	{
		Step(1.f);
		if (I % 60 == 0) FestMax = std::max(FestMax, Count().Going);
	}
	CHECK(FestMax == 0, "a festival Thursday (no club): nobody goes (%d)", FestMax);
	// ---- a Wednesday with the condition on: the activity is Thursday's only
	Begin(2, 18.f);
	W.SetCondition("book_club", true);
	int WedMax = 0;
	for (int I = 0; I < 2 * 3600; I++)
	{
		Step(1.f);
		if (I % 60 == 0) WedMax = std::max(WedMax, Count().Going);
	}
	W.SetCondition("book_club", false);
	CHECK(WedMax == 0, "a Wednesday evening: nobody goes, even with the condition on (%d)", WedMax);
	CHECK(Overbooked == 0, "no club seat overbooked (%d)", Overbooked);
	return Failures > Fails0 ? 1 : 0;
}
