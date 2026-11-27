// The street's everyday life, the townsfolk's side (docs/street_life.md): the conditions ULyudStreetLife switches
// (Private/People/StreetLifeModel.h) drive the activities and duties of tools/town/street_life.py. Included by peoplesim.cpp:
// run.sh street (also part of run.sh town).
//
// Checked, each scene with its condition switched as the game would, on the real town:
//   a hot Saturday afternoon: both vendors at their stalls (StaffOnShift), residents in the lines, the lines filled from the
//     front; a cool one (the condition off): no vendor, nobody at the stalls;
//   dawn: the yard keeper on his round along the promenade, the milkwoman at the doors, the shopkeepers at their fronts
//     before the shift, then at work;
//   a wash-day morning: the laundresses at their lines at nine, gone after; the taking in when the condition comes on;
//   a fine afternoon: grandmothers on the yards' benches; a warm evening: men at the domino tables;
//   an autumn morning: the keeper raking round the yards; the subbotnik's Saturday: a crowd at the yards' work spots.

static int StreetLife()
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
	std::printf("the street's life (docs/street_life.md):\n");
	const char* Conds[] = { "street_hot", "street_sweep", "laundry_hang", "laundry_take", "yard_grannies", "yard_dominoes", "leaf_raking", "subbotnik" };
	auto AllOff = [&]() { for (const char* C : Conds) W.SetCondition(C, false); };
	const int Kvass = W.FindPlace("P-STREET-KVASS"), Cart = W.FindPlace("P-STREET-ICECART");
	CHECK(Kvass >= 0 && Cart >= 0, "the stalls are places of the town (P-STREET-KVASS %d, P-STREET-ICECART %d)", Kvass, Cart);
	if (Kvass < 0 || Cart < 0)
	{
		return 1;
	}
	FClock C;
	int Overbooked = 0;
	auto Step = [&](float Dt)
	{
		C.Seconds += Dt;
		C.Set(W);
		W.Step(Dt);
		W.Events.clear();
	};
	auto Begin = [&](int Weekday, float Hour)
	{
		C = FClock::At(Weekday, Hour);
		C.Set(W);
		W.Resettle();
		// Resettle counts a sleeper's tiredness from yesterday's waking (worn out at dawn): everybody as after a
		// night's sleep and a bite, else the morning's workers break off to rest
		for (FPerson& P : W.People)
		{
			P.N[(int)ENeed::Tired] = 0.15f;
			P.N[(int)ENeed::Hunger] = 0.3f;
		}
	};
	auto Doing = [&](const char* Act, const char* KindOrPrefix, bool bPrefix = false, bool bWalking = false)
	{
		const int A = W.FindActivity(Act);
		int N = 0;
		for (const FPerson& P : W.People)
		{
			if (P.Activity != A || P.Place < 0 || (P.State != EState::Doing && !bWalking))
			{
				continue;
			}
			const FPlace& Pl = W.Places[(size_t)P.Place];
			if (bPrefix ? Pl.Id.rfind(KindOrPrefix, 0) == 0 : Pl.Kind == KindOrPrefix)
			{
				N++;
			}
		}
		return N;
	};
	auto CountOver = [&]()
	{
		for (const FSlot& S : W.Slots)
		{
			Overbooked += (int)S.Users.size() > S.Capacity ? 1 : 0;
		}
	};
	std::vector<int> Staff;

	// ---- a hot Saturday afternoon: the barrel and the cart out from ten
	AllOff();
	W.SetCondition("street_sweep", true);
	W.TempC = 27.f;
	W.SetCondition("street_hot", true);
	Begin(5, 9.f);
	int VendK = 0, VendC = 0, Buyers = 0, PeakLine = 0, Gaps = 0;
	for (int I = 0; I < 7 * 3600; I++)
	{
		Step(1.f);
		if (I % 60 != 0)
		{
			continue;
		}
		const float Hour = 9.f + I / 3600.f;
		if (Hour >= 10.75f)
		{
			W.StaffOnShift("P-STREET-KVASS", "vendor", Staff);
			VendK += Staff.empty() ? 0 : 1;
			W.StaffOnShift("P-STREET-ICECART", "vendor", Staff);
			VendC += Staff.empty() ? 0 : 1;
		}
		const int B = Doing("buy_kvass", "P-STREET-KVASS", true) + Doing("buy_icecream", "P-STREET-ICECART", true);
		Buyers += B;
		// the lines fill from the front: nobody at a spot with a free one before it
		for (const int Pl : { Kvass, Cart })
		{
			int Line = 0;
			bool bFree = false;
			for (const int S : W.Places[(size_t)Pl].Slots)
			{
				const FSlot& Sl = W.Slots[(size_t)S];
				if (Sl.Queue < 0)
				{
					continue;
				}
				const bool bTaken = !Sl.Users.empty();
				Line += bTaken ? 1 : 0;
				Gaps += bTaken && bFree ? 1 : 0;
				bFree = bFree || !bTaken;
			}
			PeakLine = std::max(PeakLine, Line);
		}
		CountOver();
	}
	const int Minutes = (int)((16.f - 10.75f) * 60.f);
	CHECK(VendK >= Minutes * 8 / 10 && VendC >= Minutes * 8 / 10, "a hot day: the vendors stand at the barrel and the cart (%d and %d of %d minutes)", VendK, VendC, Minutes);
	CHECK(Buyers >= 30, "residents come for kvass and ice cream on a hot afternoon (%d person-minutes)", Buyers);
	CHECK(PeakLine >= 2, "a line forms at a stall (%d at most)", PeakLine);
	CHECK(Gaps <= Minutes / 10, "the lines fill from the front (%d minutes with a gap)", Gaps);
	// ---- the same afternoon, cool: the condition off
	W.SetCondition("street_hot", false);
	W.TempC = 16.f;
	Begin(5, 12.f);
	int CoolVend = 0, CoolBuy = 0;
	for (int I = 0; I < 2 * 3600; I++)
	{
		Step(1.f);
		if (I % 60 != 0 || I < 600)
		{
			continue;
		}
		W.StaffOnShift("P-STREET-KVASS", "vendor", Staff);
		CoolVend += (int)Staff.size();
		CoolBuy += Doing("buy_kvass", "P-STREET-KVASS", true) + Doing("buy_icecream", "P-STREET-ICECART", true);
	}
	CHECK(CoolVend == 0 && CoolBuy == 0, "a cool day: no vendor, nobody at the stalls (%d, %d)", CoolVend, CoolBuy);

	// ---- dawn on a Wednesday: the sweep, the milk, the shutters
	AllOff();
	W.TempC = 14.f;
	W.SetCondition("street_sweep", true);
	Begin(2, 4.5f);
	int Sweep = 0, Milk = 0, Shutters = 0, ShutterPeople = 0;
	std::set<std::string> AtFront;
	for (int I = 0; I < 6 * 3600; I++)
	{
		Step(1.f);
		if (I % 60 != 0)
		{
			continue;
		}
		const float Hour = 4.5f + I / 3600.f;
		if (Hour >= 5.75f && Hour < 7.3f) Sweep += Doing("sweep_street", "sweep_round", false, true) > 0 ? 1 : 0;
		if (Hour >= 6.25f && Hour < 7.4f) Milk += Doing("deliver_milk", "milk_round", false, true) > 0 ? 1 : 0;
		const int Sh = Doing("open_shutters", "shop_front");
		Shutters += Sh;
		const int A = W.FindActivity("open_shutters");
		for (const FPerson& P : W.People)
		{
			if (P.Activity == A && P.State == EState::Doing) AtFront.insert(P.Key);
		}
		CountOver();
	}
	ShutterPeople = (int)AtFront.size();
	CHECK(Sweep >= 70, "dawn: the yard keeper sweeps the promenade (%d of ~93 minutes)", Sweep);
	CHECK(Milk >= 50, "the milkwoman goes round the doors (%d of ~69 minutes)", Milk);
	CHECK(ShutterPeople >= 8, "shopkeepers put up the shutters in front of their shops before the morning shift (%d people, %d person-minutes)", ShutterPeople, Shutters);
	// sweeping in the snow months: the condition off
	W.SetCondition("street_sweep", false);
	Begin(2, 5.f);
	int WinterSweep = 0;
	for (int I = 0; I < 2 * 3600; I++)
	{
		Step(1.f);
		if (I % 60 == 0) WinterSweep += Doing("sweep_street", "sweep_round");
	}
	CHECK(WinterSweep == 0, "winter: no sweeping round (%d)", WinterSweep);

	// ---- a wash-day morning (Thursday): hung at nine, taken in at 13:00 (rain coming)
	AllOff();
	W.SetCondition("street_sweep", true);
	W.TempC = 19.f;
	Begin(3, 8.f);
	int Hung = 0, Took = 0, LaterAtLine = 0;
	for (int I = 0; I < 6 * 3600; I++)
	{
		const float Hour = 8.f + I / 3600.f;
		W.SetCondition("laundry_hang", Hour >= 9.f && Hour < 9.5f);
		W.SetCondition("laundry_take", Hour >= 13.f && Hour < 13.4f);
		Step(1.f);
		if (I % 60 != 0)
		{
			continue;
		}
		if (Hour >= 9.1f && Hour < 9.5f) Hung = std::max(Hung, Doing("hang_laundry", "laundry_line"));
		if (Hour >= 13.1f && Hour < 13.4f) Took = std::max(Took, Doing("take_laundry", "laundry_line"));
		if (Hour >= 10.f && Hour < 12.9f) LaterAtLine = std::max(LaterAtLine, Doing("hang_laundry", "laundry_line") + Doing("take_laundry", "laundry_line"));
		CountOver();
	}
	CHECK(Hung >= 6, "a fine wash-day morning: the laundresses hang the washing at nine (%d of 9 yards)", Hung);
	CHECK(Took >= 6, "rain coming: they take it in (%d of 9)", Took);
	CHECK(LaterAtLine == 0, "between the two nobody stands at the lines (%d)", LaterAtLine);

	// ---- a fine afternoon: the grandmothers' benches; a warm evening: dominoes
	AllOff();
	W.SetCondition("street_sweep", true);
	W.TempC = 20.f;
	Begin(2, 15.f);
	int Grannies = 0, Dominoes = 0, OffBench = 0;
	for (int I = 0; I < 6 * 3600; I++)
	{
		const float Hour = 15.f + I / 3600.f;
		W.SetCondition("yard_grannies", Hour >= 16.f && Hour < 20.f);
		W.SetCondition("yard_dominoes", Hour >= 17.5f && Hour < 21.5f);
		Step(1.f);
		if (I % 60 != 0)
		{
			continue;
		}
		if (Hour >= 17.f && Hour < 19.5f) Grannies = std::max(Grannies, Doing("granny_bench", "yard_bench"));
		if (Hour >= 19.f && Hour < 20.8f) Dominoes = std::max(Dominoes, Doing("yard_dominoes", "domino_table"));
		if (Hour < 15.9f) OffBench += Doing("granny_bench", "yard_bench");
		CountOver();
	}
	CHECK(Grannies >= 8, "grandmothers sit on the benches by the entrances (%d at most)", Grannies);
	CHECK(Dominoes >= 4, "an evening's dominoes at the yards' tables (%d at most)", Dominoes);
	CHECK(OffBench == 0, "not before the condition (%d)", OffBench);

	// ---- an autumn morning: the leaves; a spring Saturday: the subbotnik
	AllOff();
	W.SetCondition("street_sweep", true);
	W.TempC = 9.f;
	W.SetCondition("leaf_raking", true);
	Begin(2, 7.5f);
	int Raking = 0;
	for (int I = 0; I < 3 * 3600; I++)
	{
		Step(1.f);
		if (I % 60 == 0 && I > 1800) Raking += Doing("rake_leaves", "leaf_round", false, true) > 0 ? 1 : 0;
	}
	CHECK(Raking >= 100, "autumn: the yard keeper rakes the leaves round the yards (%d of 150 minutes)", Raking);
	W.SetCondition("leaf_raking", false);
	W.TempC = 12.f;
	W.SetCondition("subbotnik", true);
	Begin(5, 8.5f);
	int Sub = 0;
	std::set<std::string> Who;
	for (int I = 0; I < 4 * 3600; I++)
	{
		Step(1.f);
		if (I % 60 != 0)
		{
			continue;
		}
		const int N = Doing("subbotnik_paint", "subbotnik") + Doing("subbotnik_whitewash", "subbotnik") + Doing("subbotnik_sweep", "subbotnik");
		Sub = std::max(Sub, N);
		for (const char* A : { "subbotnik_paint", "subbotnik_whitewash", "subbotnik_sweep" })
		{
			const int Ai = W.FindActivity(A);
			for (const FPerson& P : W.People) if (P.Activity == Ai && P.State == EState::Doing) Who.insert(P.Key);
		}
		CountOver();
	}
	W.SetCondition("subbotnik", false);
	CHECK(Sub >= 20, "the subbotnik: residents paint the benches, whitewash the trunks, sweep the yards (%d at once, %zu in all)", Sub, Who.size());
	CHECK(Overbooked == 0, "no slot overbooked (%d)", Overbooked);
	AllOff();
	return Failures > Fails0 ? 1 : 0;
}
