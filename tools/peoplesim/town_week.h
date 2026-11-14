// The town over a simulated week (tools/peoplesim/run.sh week): invariants that must hold every minute, every day,
// through what the game does to the life (UPeopleLifeSubsystem, Private/People/PeopleLife.cpp): a new game, the time
// scale (longer steps), clock jumps (Resettle, the townsfolk placed SettleBatch a step), a save loaded mid-day and at
// 02:00 / 03:00 (the townsfolk aren't saved: a fresh world, AssignHomes, Resettle), a public holiday (SetDayOff), a
// gathering set and cleared (SetGathering / ClearGathering), weekdays and the weekend. Included by peoplesim.cpp.
//
// The clock is the game's (UGameCalendar): a plain date and time with no daylight saving (FDateTime), the life's Time
// counted in seconds from the start date, Tuesday 20 October 2026 16:00. The week runs over Sunday 25 October (the
// night European clocks go back): the game's days stay 24 hours.

#include <cstdint>
#include <vector>

namespace TownWeek
{
	/** Seconds since Monday 19 October 2026 00:00, and the game's life clock (seconds since the start date). */
	struct FGameClock
	{
		double Abs = 0.0;
		static constexpr double StartAbs = (24.0 + 16.0) * 3600.0;     // Tuesday 16:00
		void Set(FWorld& W) const
		{
			const double H = Abs / 3600.0;
			W.Hour = (float)std::fmod(H, 24.0);
			W.Weekday = (int)std::fmod(std::floor(H / 24.0), 7.0);
			W.Time = Abs - StartAbs;
			W.Sunrise = 7.75f;
			W.Sunset = 17.25f;
			W.bSunUp = W.Hour > W.Sunrise && W.Hour < W.Sunset;
			W.SunYaw = 30.f;
		}
		int Day() const { return (int)std::floor(Abs / 86400.0); }        // 0 Monday 19th
		float Hour() const { return (float)std::fmod(Abs / 3600.0, 24.0); }
		static double At(int Day, float Hour) { return Day * 86400.0 + Hour * 3600.0; }
	};

	static bool InH(float From, float To, float H)
	{
		if (To - From >= 24.f || To == From)
		{
			return true;
		}
		From = std::fmod(From + 48.f, 24.f);
		To = std::fmod(To + 48.f, 24.f);
		return From <= To ? (H >= From && H < To) : (H >= From || H < To);
	}

	/** Whether a duty's shift covers the moment (weekday, hour), from its start moved by Lead (h, may be negative) to its
	 *  end moved by Tail. A shift over midnight (18:00-01:30) belongs to the day it starts on: its small hours are the
	 *  next morning's. bDayOff: a holiday duty is skipped. */
	static bool InShift(const FWorld& W, const FDuty& D, int Weekday, float Hour, float Lead, float Tail, bool bDayOff)
	{
		const int A = W.ActOf(D);
		if (bDayOff && A >= 0 && W.Activities[A].bHolidayOff)
		{
			return false;
		}
		// the shifts of yesterday, today and tomorrow as spans of hours from today's midnight
		const float Len = D.To > D.From ? D.To - D.From : D.To + 24.f - D.From;
		for (int Off = -1; Off <= 1; Off++)
		{
			if (!((D.Days >> ((Weekday + Off + 7) % 7)) & 1))
			{
				continue;
			}
			const float S = Off * 24.f + D.From - Lead, E = Off * 24.f + D.From + Len + Tail;
			if (Hour >= S && Hour < E)
			{
				return true;
			}
		}
		return false;
	}

	struct FBounds
	{
		float X0 = 1e9f, Y0 = 1e9f, X1 = -1e9f, Y1 = -1e9f;
		std::set<std::pair<int, int>> Cells;        // 50 m cells within ~250 m of a place, a slot or a stair
		void Add(const V3& P)
		{
			if (P.X == 0.f && P.Y == 0.f && P.Z == 0.f)
			{
				return;
			}
			X0 = std::min(X0, P.X);
			Y0 = std::min(Y0, P.Y);
			X1 = std::max(X1, P.X);
			Y1 = std::max(Y1, P.Y);
			const int CX = (int)std::floor(P.X / 50.f), CY = (int)std::floor(P.Y / 50.f);
			for (int DX = -5; DX <= 5; DX++)
			{
				for (int DY = -5; DY <= 5; DY++)
				{
					Cells.insert({ CX + DX, CY + DY });
				}
			}
		}
		bool Near(const V3& P) const { return Cells.count({ (int)std::floor(P.X / 50.f), (int)std::floor(P.Y / 50.f) }) > 0; }
	};

	static uint64_t StateHash(const FWorld& W)
	{
		uint64_t H = 1469598103934665603ull;
		auto Mix = [&H](int64_t V) { H = (H ^ (uint64_t)V) * 1099511628211ull; };
		for (const FPerson& P : W.People)
		{
			Mix((int)P.State);
			Mix(P.Activity);
			Mix(P.Slot);
			Mix((int64_t)std::llround(P.Pos.X * 100.0));
			Mix((int64_t)std::llround(P.Pos.Y * 100.0));
			Mix((int64_t)std::llround(P.Pos.Z * 100.0));
			for (int N = 0; N < NumNeeds; N++)
			{
				Mix((int64_t)std::llround(P.N[N] * 1e5));
			}
		}
		return H;
	}

	/** A fresh life as UPeopleLifeSubsystem::Start makes it: loaded, homes given, the townsfolk placed over steps. */
	static bool Boot(FWorld& W, const FGameClock& C)
	{
		W.Opt.SettleBatch = 120;
		if (!W.Load(DataDir, AnimalsDir))
		{
			std::printf("load failed: %s\n", W.LastError.c_str());
			return false;
		}
		W.AssignHomes();
		AttachRail(W);
		C.Set(W);
		W.PrevHour = -1.f;
		return true;
	}

	/** Runs the settling steps (a frame each, a quarter of a second) till everybody is placed; the worst step's cost, ms. */
	static double Settle(FWorld& W, FGameClock& C, int& Steps)
	{
		double Worst = 0.0;
		for (Steps = 0; Steps < 200 && W.Settling(); Steps++)
		{
			C.Abs += 0.25;
			C.Set(W);
			W.Time -= 0.25;     // Step adds it
			const auto T0 = std::chrono::steady_clock::now();
			W.Step(0.25f);
			Worst = std::max(Worst, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count());
			W.Events.clear();
		}
		return Worst;
	}
}

static int Week()
{
	using namespace TownWeek;
	auto W = std::make_unique<FWorld>();
	FGameClock C;
	C.Abs = FGameClock::StartAbs;
	if (!Boot(*W, C))
	{
		return 1;
	}
	W->NewGame();
	int Town = 0;
	std::vector<int> Pupils;
	for (const FPerson& P : W->People)
	{
		Town += P.bTown ? 1 : 0;
		for (const FDuty& D : P.Duties)
		{
			if (P.bTown && D.Role == "pupil")
			{
				Pupils.push_back(P.Id);
			}
		}
	}
	std::printf("the town's week: %d townsfolk (%zu people), %zu places, %zu slots; Tuesday 20 Oct 16:00 to Monday 26 Oct 13:00\n",
		Town, W->People.size(), W->Places.size(), W->Slots.size());

	// --- the homes, before anything moves
	{
		std::vector<int> HomeOf(W->People.size(), 0);
		int Twice = 0, Wrong = 0, Homeless = 0;
		for (int I = 0; I < (int)W->Places.size(); I++)
		{
			for (const int R : W->Places[I].Residents)
			{
				HomeOf[R]++;
				Wrong += W->People[R].Home != I ? 1 : 0;
			}
		}
		for (const FPerson& P : W->People)
		{
			Twice += HomeOf[P.Id] > 1 ? 1 : 0;
			Homeless += P.Home < 0 ? 1 : 0;
		}
		CHECK(Homeless == 0 && Twice == 0 && Wrong == 0, "everybody lives in exactly one flat, the one they're listed in (%d homeless, %d in two, %d mismatched)", Homeless, Twice, Wrong);
	}

	// the map's extent: the places, their slots, the town's stairs
	FBounds B;
	for (const FPlace& Pl : W->Places)
	{
		B.Add(Pl.Portal);
		for (const V3& R : Pl.RailWay) B.Add(R);
	}
	for (const FSlot& S : W->Slots)
	{
		B.Add(S.Pos);
		B.Add(S.Approach);
	}
	for (const auto& St : W->Town.Stairs)
	{
		B.Add(St.Bottom);
		B.Add(St.Top);
	}

	// the workplaces with staff of their own (townsfolk.json duties at a place)
	std::map<std::pair<int, std::string>, std::vector<std::pair<int, int>>> Staff;      // (place, role) -> (person, duty index)
	for (const FPerson& P : W->People)
	{
		for (int I = 0; I < (int)P.Duties.size(); I++)
		{
			const FDuty& D = P.Duties[I];
			if (P.bTown && D.Place >= 0 && D.Role != "pupil")
			{
				Staff[{ D.Place, D.Role }].push_back({ P.Id, I });
			}
		}
	}

	// --- what's watched over the week
	int NaN = 0, Outside = 0, TwoSlots = 0, Stale = 0, Overbooked = 0, Incoherent = 0;
	std::string NaNWho, OutsideWho, TwoWho, IncoherentWho;
	std::map<int, double> SleptMin;                 // person -> minutes asleep at home in the current noon-to-noon day
	int SleepDays = 0, ShortSleep = 0;
	std::string ShortWho;
	struct FStay { int Place = -1; double Since = 0.0; int Act = -1; };
	std::vector<FStay> Stay(W->People.size());
	int Stuck = 0, StuckIdle = 0;
	std::string StuckWho, StuckIdleWho;
	std::vector<double> IdleSince(W->People.size(), -1.0);
	std::map<std::pair<int, std::string>, std::pair<int, int>> Staffed;     // (place, role) -> (minutes expected open, minutes with one of them on shift)
	std::map<int, int> OffShift;                    // person -> minutes at work outside their shift
	int Arrivals = 0, BadArrivals = 0, Restless = 0;
	std::string RestlessWho;
	std::map<int, std::pair<long, int>> Choices;    // person -> (clock hour, activities started in it)
	float WorstArrive = 0.f;
	std::string WorstArriveWho;
	double Ms = 0.0, WorstMs = 0.0;
	long Steps1 = 0;
	std::map<std::string, std::string> Notes;       // sampled moments' results, printed with the checks
	int SchoolDays = 0, SchoolDaysFull = 0, WeekendSchool = -1, WeekendAway = -1, DayOffSchool = -1, DayOffOffice = -1, DayOffShops = -1;
	int GatherAt = -1, GatherLeft = -1, GatherLeft2 = -1;
	int Asleep02 = -1, Asleep03 = -1, Lit02 = -1, Working02 = -1, Occupied = 0;
	int SettleWorstSteps = 0;
	double SettleWorstMs = 0.0;
	int Unplaced = 0;
	uint64_t HashA = 0, HashB = 0;
	bool bDeterministic = false;
	int JumpStaff = -1;
	for (const FPlace& Pl : W->Places)
	{
		Occupied += !Pl.Windows.empty() && !Pl.Residents.empty() ? 1 : 0;
	}
	std::vector<int> Out, Changed;

	auto DoingAt = [&](const FWorld& S, const char* Act, int Place) {
		int N = 0;
		const int A = S.FindActivity(Act);
		for (const FPerson& P : S.People)
		{
			N += P.bTown && P.Activity == A && (P.State == EState::Doing || P.State == EState::Settling || P.State == EState::Away) && (Place < 0 || P.Place == Place) ? 1 : 0;
		}
		return N;
	};
	auto GatherNow = [&](const FWorld& S) {
		int N = 0;
		for (const FPerson& P : S.People)
		{
			N += P.Activity >= 0 && S.Activities[P.Activity].bGather && (P.State == EState::Doing || P.State == EState::Going) ? 1 : 0;
		}
		return N;
	};
	auto AsleepAtHome = [&](const FWorld& S) {
		int N = 0;
		for (const FPerson& P : S.People)
		{
			N += P.bTown && P.Pose == EPose::Sleep && P.Place == P.Home ? 1 : 0;
		}
		return N;
	};
	// a load or a clock jump: the life starts over at the clock (the windows and the stays with it)
	auto Restarted = [&](FWorld& S) {
		int Steps = 0;
		const double Worst = Settle(S, C, Steps);
		if (Worst > SettleWorstMs) SettleWorstMs = Worst;
		SettleWorstSteps = std::max(SettleWorstSteps, Steps);
		int U = 0;
		for (const FPerson& P : S.People) U += P.bTown && P.Activity < 0 && P.State == EState::Idle ? 1 : 0;
		Unplaced = std::max(Unplaced, U);
		for (FStay& St : Stay) St = FStay();
		std::fill(IdleSince.begin(), IdleSince.end(), -1.0);
	};

	const double End = FGameClock::At(7, 13.f);
	bool bJumped = false, bDayOffSet = false, bDayOffCleared = false, bLoadedMidDay = false, bGatherSet = false, bGatherCleared = false;
	bool bLoad02 = false, bLoad03 = false;
	double NoonWas = std::floor((C.Abs - 12 * 3600.0) / 86400.0);
	double NextSample = std::ceil(C.Abs / 60.0) * 60.0;
	while (C.Abs < End)
	{
		FWorld& S = *W;
		const int Day = C.Day();
		const float H = C.Hour();
		// the time scale: a second a step (as the game at up to x60) on the first evening and around the rush hours, longer
		// steps the rest of the time (the developer's slider: PeopleLife steps a fast clock in a few longer steps)
		const bool bFine = Day <= 1 || (H >= 7.f && H < 9.f) || (H >= 17.f && H < 18.f);
		const float Dt = bFine ? 1.f : 4.f;

		// --- what the game does to the life, at its moments
		// Wednesday 13:00: the clock jumps three hours (the developer's time jump, or a sleep): Resettle, placed over frames
		if (!bJumped && Day == 2 && H >= 13.f)
		{
			bJumped = true;
			C.Abs += 3 * 3600.0;
			C.Set(S);
			S.Resettle();
			Restarted(S);
			std::vector<int> Shop;
			JumpStaff = 0;
			for (const char* Id : { "P-STORE", "P-MARKET", "restaurant", "P-CAFE", "P-LIBRARY", "P-CLINIC" })
			{
				S.StaffOnShift(Id, "", Shop);
				JumpStaff += Shop.empty() ? 0 : 1;
			}
			continue;
		}
		// Thursday: a public holiday (the festival calendar's SetDayOff at midnight), over at Friday's midnight
		if (!bDayOffSet && Day == 3)
		{
			bDayOffSet = true;
			S.bDayOff = true;
		}
		if (bDayOffSet && !bDayOffCleared && Day == 4)
		{
			bDayOffCleared = true;
			S.bDayOff = false;
		}
		// Friday 12:30: a save is loaded (the townsfolk aren't in it: a fresh world, placed at the clock); the same load
		// twice gives the same town two hours on (the life is deterministic for its seed)
		if (!bLoadedMidDay && Day == 4 && H >= 12.5f)
		{
			bLoadedMidDay = true;
			auto Fresh = std::make_unique<FWorld>();
			auto Twin = std::make_unique<FWorld>();
			FGameClock C2 = C;
			if (!Boot(*Fresh, C) || !Boot(*Twin, C2))
			{
				return 1;
			}
			Fresh->Resettle();
			Twin->Resettle();
			W = std::move(Fresh);
			Restarted(*W);
			int Dummy = 0;
			Settle(*Twin, C2, Dummy);
			for (int I = 0; I < 7200; I++)
			{
				for (FWorld* X : { W.get(), Twin.get() })
				{
					FGameClock& K = X == W.get() ? C : C2;
					K.Abs += 1.0;
					K.Set(*X);
					X->Time -= 1.0;
					X->Step(1.f);
					X->Events.clear();
				}
			}
			HashA = StateHash(*W);
			HashB = StateHash(*Twin);
			bDeterministic = HashA == HashB && C.Abs == C2.Abs;
			NextSample = std::ceil(C.Abs / 60.0) * 60.0;
			continue;
		}
		// Saturday: a holiday on the square 15:00-19:00 (pulled), cleared by the festival at 18:00 (its phase over early)
		if (!bGatherSet && Day == 5 && H >= 14.9f)
		{
			bGatherSet = true;
			S.SetGathering("week_fest", "S-SQUARE", 15.f, 19.f, 1.6f, EGatherMode::Mingle, false, V3());
		}
		if (bGatherSet && !bGatherCleared && Day == 5 && H >= 18.f)
		{
			bGatherCleared = true;
			S.ClearGathering("week_fest");
		}
		// Monday 02:00 and 03:00: a save loaded in the small hours (a fresh world each time)
		if ((!bLoad02 && Day == 7 && H >= 2.f) || (!bLoad03 && Day == 7 && H >= 3.f))
		{
			const bool bTwo = !bLoad02;
			(bTwo ? bLoad02 : bLoad03) = true;
			auto Fresh = std::make_unique<FWorld>();
			if (!Boot(*Fresh, C))
			{
				return 1;
			}
			Fresh->Resettle();
			W = std::move(Fresh);
			Restarted(*W);
			FWorld& N = *W;
			(bTwo ? Asleep02 : Asleep03) = AsleepAtHome(N);
			if (bTwo)
			{
				N.UpdateLights(Changed);
				Lit02 = 0;
				for (const int8_t L : N.FlatLit) Lit02 += L == 1 ? 1 : 0;
				Working02 = 0;
				for (const FPerson& P : N.People)
				{
					const bool bWork = P.Activity >= 0 && N.Activities[P.Activity].bStaff && P.State != EState::Idle;
					bool bNight = false;
					for (const FDuty& D : P.Duties) bNight |= InShift(N, D, N.Weekday, N.Hour, 0.5f, 0.5f, N.bDayOff);
					Working02 += bWork && !bNight ? 1 : 0;
				}
			}
			NextSample = std::ceil(C.Abs / 60.0) * 60.0;
			continue;
		}

		// --- a step
		C.Abs += Dt;
		C.Set(S);
		S.Time -= Dt;
		const auto T0 = std::chrono::steady_clock::now();
		S.Step(Dt);
		const double StepMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
		if (Dt == 1.f)
		{
			Ms += StepMs;
			WorstMs = std::max(WorstMs, StepMs);
			Steps1++;
		}
		for (const FEvent& E : S.Events)
		{
			if (E.Kind == EEvent::Start && S.People[E.Person].bTown)
			{
				// a choice made: how many this clock hour (one every couple of minutes is a person who can't settle)
				const long Hour = (long)std::floor(C.Abs / 3600.0);
				std::pair<long, int>& Ch = Choices[E.Person];
				Ch = Ch.first == Hour ? std::make_pair(Hour, Ch.second + 1) : std::make_pair(Hour, 1);
				if (Ch.second == 13)
				{
					if (Restless++ < 3)
					{
						char Buf[160];
						std::snprintf(Buf, sizeof Buf, "%s%s (day %d %02d:00, %s)", RestlessWho.empty() ? "" : ", ", S.People[E.Person].Key.c_str(), Day, (int)H,
							S.People[E.Person].Activity >= 0 ? S.Activities[S.People[E.Person].Activity].Id.c_str() : "-");
						RestlessWho += Buf;
					}
				}
			}
			if (E.Kind != EEvent::Arrive)
			{
				continue;
			}
			const FPerson& P = S.People[E.Person];
			Arrivals++;
			bool bOk = P.Place >= 0 && P.Slot >= 0 && P.Slot < (int)S.Slots.size() && S.Slots[P.Slot].Place == P.Place;
			float Dist = 1e9f;
			if (bOk)
			{
				const FSlot& Sl = S.Slots[P.Slot];
				Dist = std::min((P.Pos - Sl.Pos).Len(), (P.Pos - Sl.Approach).Len());
				for (const V3& Sp : Sl.Spots) Dist = std::min(Dist, (P.Pos - Sp).Len());
				Dist = std::min(Dist, (P.Pos - S.Places[P.Place].Portal).Len());
				if (Dist > WorstArrive)
				{
					WorstArrive = Dist;
					WorstArriveWho = P.Key + " at " + S.Places[P.Place].Id;
				}
			}
			BadArrivals += !bOk || Dist > 4.f ? 1 : 0;
		}
		S.Events.clear();

		// --- a new day (noon to noon): who slept at home how long
		const double Noon = std::floor((C.Abs - 12 * 3600.0) / 86400.0);
		if (Noon != NoonWas)
		{
			// a full noon-to-noon day only (the first one starts at 16:00)
			if (NoonWas >= 2.0)
			{
				for (const FPerson& P : S.People)
				{
					if (!P.bTown) continue;
					SleepDays++;
					if (SleptMin[P.Id] < 4 * 60)
					{
						if (ShortSleep++ < 3)
						{
							char Buf[160];
							std::snprintf(Buf, sizeof Buf, "%s%s %.1f h (day %d)", ShortWho.empty() ? "" : ", ", P.Key.c_str(), SleptMin[P.Id] / 60.0, (int)NoonWas);
							ShortWho += Buf;
						}
					}
				}
			}
			SleptMin.clear();
			NoonWas = Noon;
		}
		if (C.Abs < NextSample)
		{
			continue;
		}
		NextSample += 60.0;

		// --- every minute
		const float Hr = S.Hour;
		const int Wd = S.Weekday;
		std::vector<int> InSlots(S.People.size(), 0);
		for (int I = 0; I < (int)S.Slots.size(); I++)
		{
			const FSlot& Sl = S.Slots[I];
			Overbooked += (int)Sl.Users.size() > Sl.Capacity ? 1 : 0;
			for (const int U : Sl.Users)
			{
				InSlots[U]++;
				if (S.People[U].Reserved != I)
				{
					Stale++;
				}
			}
		}
		for (FPerson& P : S.People)
		{
			const bool bFinite = std::isfinite(P.Pos.X) && std::isfinite(P.Pos.Y) && std::isfinite(P.Pos.Z) && std::isfinite(P.Yaw);
			if (!bFinite && NaN++ == 0) NaNWho = P.Key;
			if (bFinite && (!B.Near(P.Pos) || std::fabs(P.Pos.Z) > 400.f) && Outside++ == 0)
			{
				char Buf[160];
				std::snprintf(Buf, sizeof Buf, "%s at %.0f %.0f %.0f", P.Key.c_str(), P.Pos.X, P.Pos.Y, P.Pos.Z);
				OutsideWho = Buf;
			}
			if (InSlots[P.Id] > 1 && TwoSlots++ == 0) TwoWho = P.Key;
			// where they are agrees with what they do: at the slot's place, the slot held (away: none held)
			const bool bAt = P.State == EState::Doing || P.State == EState::Settling;
			if (bAt && (P.Slot < 0 || P.Activity < 0 || S.Slots[P.Slot].Place != P.Place || (!S.Activities[P.Activity].bAway && P.Reserved != P.Slot)))
			{
				if (Incoherent++ == 0) IncoherentWho = P.Key;
			}
			if (!P.bTown) continue;
			if (P.Pose == EPose::Sleep && P.Place == P.Home && P.State == EState::Doing) SleptMin[P.Id] += 1.0;
			// stuck: idle with nothing to do, or at one place (not home) longer than anything there lasts
			if (P.State == EState::Idle && P.Activity < 0)
			{
				if (IdleSince[P.Id] < 0.0) IdleSince[P.Id] = S.Time;
				else if (S.Time - IdleSince[P.Id] > 1800.0 && StuckIdle++ == 0) StuckIdleWho = P.Key;
			}
			else
			{
				IdleSince[P.Id] = -1.0;
			}
			FStay& St = Stay[P.Id];
			const int At = bAt || P.State == EState::Away ? P.Place : -1;
			if (At != St.Place)
			{
				St.Place = At;
				St.Since = S.Time;
				St.Act = P.Activity;
			}
			else if (At >= 0 && At != P.Home)
			{
				// a shift lasts its hours and a while after (+3 h: a swim at the pool after a shift there); anything else ten
				// hours at most (a holiday spent at one café, coffee after lunch after coffee, comes to eight)
				float Limit = 10.f;
				for (const FDuty& D : P.Duties)
				{
					if (D.Place == At || S.ActOf(D) == P.Activity)
					{
						Limit = std::max(Limit, std::fmod(D.To - D.From + 24.f, 24.f) + 3.f);
					}
				}
				if (S.Time - St.Since > Limit * 3600.0)
				{
					if (Stuck++ < 4)
					{
						char Buf[64];
						std::snprintf(Buf, sizeof Buf, " (day %d %05.2f)", Day, Hr);
						StuckWho += (StuckWho.empty() ? "" : ", ") + P.Key + " at " + S.Places[At].Id + " doing " + (P.Activity >= 0 ? S.Activities[P.Activity].Id : "-") + Buf;
					}
					St.Since = S.Time;      // counted once per overstay
				}
			}
			// at work outside their shift (half an hour's grace each side)
			if (P.Activity >= 0 && (bAt || P.State == EState::Away) && P.Activity != S.SleepAct)
			{
				for (const FDuty& D : P.Duties)
				{
					if (S.ActOf(D) == P.Activity && !InShift(S, D, Wd, Hr, 0.8f, 0.5f, S.bDayOff))
					{
						OffShift[P.Id]++;
						break;
					}
				}
			}
		}
		// the workplaces staffed while open: someone's shift is well under way at each of them
		for (const auto& Pl : Staff)
		{
			bool bOpen = false;
			for (const auto& PD : Pl.second)
			{
				bOpen |= InShift(S, S.People[PD.first].Duties[PD.second], Wd, Hr, -0.75f, -0.25f, S.bDayOff);
			}
			if (!bOpen) continue;
			S.StaffOnShift(S.Places[Pl.first.first].Id, Pl.first.second, Out);
			Staffed[Pl.first].first++;
			Staffed[Pl.first].second += Out.empty() ? 0 : 1;
		}

		// LYUD_WEEK_TRACE=<key>: one person's week, minute by minute (debugging)
		if (const char* Tr = std::getenv("LYUD_WEEK_TRACE"))
		{
			const int Id = S.FindPerson(Tr);
			if (Id >= 0)
			{
				const FPerson& P = S.People[Id];
				std::printf("    day %d %05.2f %-9s %-14s %-22s %-12s hunger %.2f tired %.2f left %.0f min, bed %.2f\n", Day, Hr, StateName(P.State),
					P.Activity >= 0 ? S.Activities[P.Activity].Id.c_str() : "-", P.Place >= 0 ? S.Places[P.Place].Id.c_str() : "-", P.Why.c_str(),
					P.N[(int)ENeed::Hunger], P.N[(int)ENeed::Tired], (P.Until - S.Time) / 60.0, S.BedOf(P));
			}
		}
		// --- the sampled moments
		const int Min = (int)std::lround(Hr * 60.f);
		if (Min == 11 * 60)
		{
			const int Sch = S.FindPlace("P-SCHOOL");
			S.StaffOnShift("P-SCHOOL", "pupil", Out);
			const int AtSchool = (int)Out.size();
			if (Wd <= 4 && !S.bDayOff)
			{
				SchoolDays++;
				SchoolDaysFull += AtSchool >= (int)Pupils.size() * 0.9 ? 1 : 0;
				Notes["school" + std::to_string(Day)] = std::to_string(AtSchool);
			}
			if (S.bDayOff)
			{
				DayOffSchool = AtSchool;
				DayOffOffice = DoingAt(S, "work_office", -1);
				DayOffShops = DoingAt(S, "work_town", -1);
			}
			if (Wd == 5 || Wd == 6)
			{
				WeekendSchool = std::max(WeekendSchool, DoingAt(S, "school_town", Sch));
				WeekendAway = std::max(WeekendAway, DoingAt(S, "work_away", -1));
			}
		}
		if (Day == 5 && Min == 17 * 60) GatherAt = DoingAt(S, "gather_mingle", S.FindPlace("S-SQUARE"));
		if (Day == 5 && Min == 18 * 60 + 30) GatherLeft = GatherNow(S);
		if (Day == 5 && Min == 19 * 60) GatherLeft2 = GatherNow(S);
	}

	// --- the checks
	CHECK(NaN == 0, "no position or heading is NaN (%d person-minutes%s%s)", NaN, NaN ? ", first " : "", NaNWho.c_str());
	CHECK(Outside == 0, "everybody stays on the map, near its places (%d person-minutes off it%s%s)", Outside, Outside ? "; first " : "", OutsideWho.c_str());
	CHECK(TwoSlots == 0 && Stale == 0, "nobody holds two slots, every slot's user holds it (%d in two, %d stale%s%s)", TwoSlots, Stale, TwoSlots ? "; " : "", TwoWho.c_str());
	CHECK(Incoherent == 0, "whoever is at an activity is at its slot's place and holds the slot (%d person-minutes%s%s)", Incoherent, Incoherent ? "; first " : "", IncoherentWho.c_str());
	CHECK(Overbooked == 0, "no slot overbooked (%d slot-minutes)", Overbooked);
	CHECK(SleepDays > Town * 4 && ShortSleep == 0, "everybody sleeps at home 4 h or more every day (%d person-days, %d short%s%s)", SleepDays, ShortSleep, ShortSleep ? ": " : "", ShortWho.c_str());
	CHECK(Stuck == 0, "nobody stays at one place (not home) past its hours (%d overstays%s%s)", Stuck, Stuck ? ": " : "", StuckWho.c_str());
	CHECK(StuckIdle == 0, "nobody stands idle with nothing to do for 30 min (%d%s%s)", StuckIdle, StuckIdle ? "; first " : "", StuckIdleWho.c_str());
	{
		int Open = 0, Manned = 0, Poor = 0;
		std::string PoorWho;
		for (const auto& St : Staffed)
		{
			Open += St.second.first;
			Manned += St.second.second;
			const float F = St.second.first ? (float)St.second.second / St.second.first : 1.f;
			if (F < 0.85f)
			{
				char Buf[96];
				std::snprintf(Buf, sizeof Buf, "%s%s %s %.0f%%", Poor++ ? ", " : "", W->Places[St.first.first].Id.c_str(), St.first.second.c_str(), F * 100.f);
				PoorWho += Buf;
			}
		}
		CHECK(Poor == 0 && Manned >= Open * 0.95, "every workplace has each of its jobs staffed while open (%d of %d open minutes; %d below 85%%%s%s)",
			Manned, Open, Poor, Poor ? ": " : "", PoorWho.c_str());
	}
	{
		int Late = 0;
		std::string LateWho;
		for (const auto& O : OffShift)
		{
			if (O.second > 30)
			{
				if (Late++ < 4)
				{
					const FPerson& P = W->People[O.first];
					char Buf[200];
					std::snprintf(Buf, sizeof Buf, "%s%s (%s %.1f-%.1f, %d min)", LateWho.empty() ? "" : ", ", P.Key.c_str(),
						P.Duties.empty() ? "" : P.Duties[0].Activity.c_str(), P.Duties.empty() ? 0.f : P.Duties[0].From, P.Duties.empty() ? 0.f : P.Duties[0].To, O.second);
					LateWho += Buf;
				}
			}
		}
		CHECK(Late == 0, "nobody works outside their shift for over 30 min (staff leave after closing; a night shift's small hours are the next morning's) (%d%s%s)",
			Late, Late ? ": " : "", LateWho.c_str());
	}
	{
		std::string Days;
		for (const auto& N : Notes) Days += " " + N.second;
		CHECK(SchoolDays >= 3 && SchoolDaysFull == SchoolDays, "school days 11:00: nine in ten pupils at school (%d of %d days; pupils %zu, at school:%s)", SchoolDaysFull, SchoolDays, Pupils.size(), Days.c_str());
	}
	CHECK(WeekendSchool == 0 && WeekendAway == 0, "the weekend 11:00: nobody at school or at work out of town (%d, %d)", WeekendSchool, WeekendAway);
	CHECK(DayOffSchool == 0 && DayOffOffice == 0, "a public holiday (SetDayOff): no pupils, the offices shut (%d at school, %d at the offices)", DayOffSchool, DayOffOffice);
	CHECK(DayOffShops > 20, "... but the shops, cafés and the market work (%d at work)", DayOffShops);
	CHECK(JumpStaff >= 5, "a clock jump to Wednesday 16:00: the store, the market, the restaurant, the café, the library and the clinic staffed at once (%d of 6)", JumpStaff);
	CHECK(SettleWorstSteps < 60 && Unplaced == 0, "loads and clock jumps place everybody (at most %d steps, worst %.1f ms; %d left unplaced)", SettleWorstSteps, SettleWorstMs, Unplaced);
	CHECK(bDeterministic, "a save loaded twice gives the same town two hours on (deterministic for the seed: %016llx %s %016llx)",
		(unsigned long long)HashA, HashA == HashB ? "==" : "!=", (unsigned long long)HashB);
	CHECK(GatherAt > 40, "a gathering on the square Saturday 17:00 pulls the town (%d there)", GatherAt);
	// (whoever came lately stays the activity's shortest time, 40 min, before looking round: the crowd thins, then goes)
	CHECK(GatherLeft >= 0 && GatherLeft < GatherAt / 3 && GatherLeft2 >= 0 && GatherLeft2 < 5,
		"... cleared at 18:00, it lets them go: at 18:30 %d still at it or on their way, at 19:00 %d", GatherLeft, GatherLeft2);
	CHECK(Asleep02 > Town * 0.8 && Asleep03 > Town * 0.8, "a save loaded at 02:00 / 03:00: most asleep in their own beds (%d / %d of %d)", Asleep02, Asleep03, Town);
	CHECK(Lit02 >= 0 && Lit02 < Occupied * 0.2, "... few windows lit (%d of %d)", Lit02, Occupied);
	CHECK(Working02 == 0, "... and nobody at work but the night shifts (%d)", Working02);
	CHECK(Restless == 0, "nobody starts a new activity every few minutes (13 or more in an hour: %d person-hours%s%s)", Restless, Restless ? "; " : "", RestlessWho.c_str());
	CHECK(Arrivals > 10000 && BadArrivals == 0, "every trip ends at its place, at the slot (%d arrivals, %d off; worst %.1f m, %s)", Arrivals, BadArrivals, WorstArrive, WorstArriveWho.c_str());
	CHECK(Steps1 > 0 && Ms / Steps1 < 1.5 && WorstMs < 25.0, "a second's step costs %.3f ms on average, %.1f ms at worst (%ld steps, %zu people)", Steps1 ? Ms / Steps1 : 0.0, WorstMs, Steps1, W->People.size());
	std::printf(Failures ? "%d FAILED\n" : "all ok\n", Failures);
	return Failures ? 1 : 0;
}
