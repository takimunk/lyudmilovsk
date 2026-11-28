// The sailing school's people (docs/yacht_school.md "People (part F)"): the members of Content/PeopleData/sailing_members.json
// living the school's week on the whole town. Included by peoplesim.cpp: run.sh sailing.
//
// The life is driven as the game drives it: every sim minute the school's conditions come from the rules
// (Private/Sailing/SailSchoolModel.h SchoolConditionsAt, with the day's go / no-go from SchoolDecide for the weather given)
// and are set on the life (sail_season, sail_kids, ...), and Private/People/SailPeople.h hands the programmes to the groups
// (FWorld::SetGroupActivity), as UPeopleLifeSubsystem::UpdateSailing does. The sun is the model's (SchoolDaylight), not
// FClock's fixed winter day.
//
// Checked: the members are bound (every boat's sailor and every coach a townsperson, nobody twice) and of their group's
// ages; attendance >= 85% over a term week in May and a holiday week in July, while the children still go to school, eat
// and sleep; afloat they are unseen (the engine draws them in the boats); a day too windy for the children's boats: the
// shore lesson, nobody afloat; out of season nobody afloat, but the fitting-out in April (the staff on a weekday, the
// members too on the Saturday); the regatta Sunday draws spectators and the parents wait for the little ones; the head
// coach is at the boathouse in her office hours (Tue-Sat 10-13 in season) and the bosun in his (9-19), StaffOnShift
// finding them; no child afloat after sunset (a late October lesson cut short by the dark); nobody at a beach slot inside
// the boathouse's or the park's footprint; the rest of the town's day about as it is without the school.

#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/SailPeople.h"

namespace SailingPeopleTest
{
	using namespace LyudSail;

	static bool InCorners(const std::vector<FSchoolV2>& C, double X, double Y)
	{
		bool bIn = false;
		const size_t N = C.size();
		for (size_t I = 0, J = N - 1; I < N; J = I++)
		{
			if ((C[I].Y > Y) != (C[J].Y > Y) && X < (C[J].X - C[I].X) * (Y - C[I].Y) / (C[J].Y - C[I].Y) + C[I].X)
			{
				bIn = !bIn;
			}
		}
		return bIn;
	}

	static bool IsSailPlace(const FPlace& Pl) { return Pl.Id.rfind("S-SAIL-", 0) == 0; }

	/** One session's attendance: per member, the minutes counted and those with the school. */
	struct FAttend
	{
		std::string What;
		std::map<int, std::pair<int, int>> ByMember;
	};
}

static int Sailing()
{
	using namespace SailingPeopleTest;
	const int Fails0 = Failures;
	FWorld W;
	if (!W.Load(DataDir, AnimalsDir))
	{
		std::printf("load failed: %s\n", W.LastError.c_str());
		return 1;
	}
	W.AssignHomes();
	AttachRail(W);
	FSchoolParams P;
	{
		std::string Err;
		if (!P.Load(ReadFile("unreal/Lyudmilovsk/Content/City/sailing.json"), Err))
		{
			std::printf("sailing.json: %s\n", Err.c_str());
			return 1;
		}
	}
	const int School = W.FindPlace("S-SAIL-SCHOOL"), Parents = W.FindPlace("S-SAIL-PARENTS");
	const int Watch = W.FindActivity("watch_regatta"), Wait = W.FindActivity("wait_child"), Shore = W.FindActivity("sail_shore_lesson"),
		CoachShore = W.FindActivity("sail_coach_shore");
	CHECK(School >= 0 && Parents >= 0 && W.FindPlace("S-SAIL-WATCH-BEACH") >= 0, "sailing_spots.json's places are loaded (S-SAIL-SCHOOL %d, S-SAIL-PARENTS %d)", School, Parents);
	CHECK(Watch >= 0 && Wait >= 0 && Shore >= 0 && CoachShore >= 0, "the school's activities are there (watch_regatta, wait_child, sail_shore_lesson, sail_coach_shore)");
	if (School < 0 || Parents < 0 || Watch < 0 || Wait < 0 || Shore < 0 || CoachShore < 0)
	{
		return 1;
	}
	CHECK(W.Places[(size_t)School].Kind == "sail_school", "the boathouse is a place of kind sail_school");

	// ---- the members: bound, unique, of their ages
	const int GKids = W.FindGroup("kids"), GJuniors = W.FindGroup("juniors"), GAdults = W.FindGroup("adults"), GParents = W.FindGroup("kids_parents"),
		GCrew = W.FindGroup("crew"), GStaff = W.FindGroup("staff");
	std::map<std::string, int> BySlot;
	int Twice = 0, BadAge = 0, Unbound = 0;
	std::string BadAgeWho;
	{
		std::set<int> Seen;
		for (const int Id : W.MemberIds)
		{
			const FPerson& Pe = W.People[(size_t)Id];
			Twice += Seen.insert(Id).second ? 0 : 1;
			if (!Pe.MemberSlot.empty())
			{
				Twice += BySlot.count(Pe.MemberSlot) ? 1 : 0;
				BySlot[Pe.MemberSlot] = Id;
			}
			const bool bKid = W.IsMember(Pe, GKids) && Pe.StaffRole.empty(), bJun = W.IsMember(Pe, GJuniors) && Pe.StaffRole.empty(),
				bAdult = W.IsMember(Pe, GAdults) && Pe.StaffRole.empty();
			const bool bOk = (!bKid || (Pe.Age >= 7 && Pe.Age <= 11)) && (!bJun || (Pe.Age >= 12 && Pe.Age <= 17)) && (!bAdult || Pe.Age >= 18)
				&& (Pe.StaffRole.empty() || Pe.Age >= 18) && (!W.IsMember(Pe, GParents) || Pe.Age >= 18);
			if (!bOk)
			{
				BadAge++;
				BadAgeWho += " " + Pe.Key;
			}
		}
		for (const FSchoolSailor& S : P.Sailors)
		{
			const auto It = BySlot.find(S.Slot);
			Unbound += S.Person.empty() || It == BySlot.end() || W.People[(size_t)It->second].Key != S.Person ? 1 : 0;
			if (!S.Crew.empty()) Unbound += BySlot.count(S.Crew) ? 0 : 1;
		}
		for (const std::string& C : P.Coaches)
		{
			Unbound += BySlot.count(C) ? 0 : 1;
		}
	}
	int Kids = 0, Juniors = 0, Adults = 0, Staff = 0;
	for (const auto& KV : BySlot)
	{
		const FPerson& Pe = W.People[(size_t)KV.second];
		Staff += Pe.StaffRole.empty() ? 0 : 1;
		Kids += Pe.StaffRole.empty() && W.IsMember(Pe, GKids) ? 1 : 0;
		Juniors += Pe.StaffRole.empty() && W.IsMember(Pe, GJuniors) ? 1 : 0;
		Adults += Pe.StaffRole.empty() && W.IsMember(Pe, GAdults) ? 1 : 0;
	}
	CHECK(Unbound == 0 && !P.Sailors.empty(), "every boat's sailor and crew and every coach is a townsperson (%zu sailors, %zu coaches; %d unbound)", P.Sailors.size(), P.Coaches.size(), Unbound);
	CHECK(Twice == 0, "nobody is a member twice, no slot is held twice (%zu members, %zu slots)", W.MemberIds.size(), BySlot.size());
	CHECK(Kids >= 6 && Juniors >= 8 && Adults >= 4 && Staff == 4, "the school's people: %d children, %d juniors, %d adults, %d staff", Kids, Juniors, Adults, Staff);
	CHECK(BadAge == 0, "everyone is of their group's ages (children 7-11, juniors 12-17, adults and staff and parents 18+)%s", BadAgeWho.c_str());
	CHECK(BySlot.count("coach_head") && W.People[(size_t)BySlot["coach_head"]].TalkAs == "Sail_coach_head" && BySlot.count("bosun")
		&& W.People[(size_t)BySlot["bosun"]].TalkAs == "Sail_bosun", "the staff talk by their roles' dialogue ids (Sail_coach_head, Sail_bosun)");
	if (Unbound || !BySlot.count("coach_head") || !BySlot.count("bosun") || !BySlot.count("coach_kids"))
	{
		return 1;
	}
	const int Head = BySlot["coach_head"], Bosun = BySlot["bosun"], KidsCoach = BySlot["coach_kids"];

	// ---- nobody's slot inside the boathouse's or the park's footprint (the beach's displaced slots moved out)
	{
		int Inside = 0;
		std::string Where;
		for (const FPlace& Pl : W.Places)
		{
			if (IsSailPlace(Pl)) continue;
			for (const int S : Pl.Slots)
			{
				const V3& Q = W.Slots[(size_t)S].Pos;
				if (InCorners(P.Boathouse.Corners, Q.X, Q.Y) || InCorners(P.Park.Corners, Q.X, Q.Y))
				{
					Inside++;
					Where = Pl.Id;
				}
			}
		}
		CHECK(Inside == 0, "no other place's slot inside the boathouse's or the park's footprint (%d%s%s)", Inside, Where.empty() ? "" : ", ", Where.c_str());
	}

	// ---- the driver: the clock, the model's sun and conditions, the programmes
	FClock C;
	FSchoolDate Date(2026, 5, 11);
	std::vector<FSchoolSessionState> States;
	FSchoolWeather Wx;
	bool bSchool = true;        // false: the school's conditions all off and no programmes (the town without it)
	auto Light = [&]()
	{
		const FSchoolDaylight L = SchoolDaylight(P, Date);
		W.Sunrise = (float)L.Sunrise;
		W.Sunset = (float)L.Sunset;
		W.bSunUp = W.Hour > W.Sunrise && W.Hour < W.Sunset;
	};
	auto Drive = [&]()
	{
		FSchoolConditions M;
		if (bSchool)
		{
			M = SchoolConditionsAt(P, Date, W.Hour, W.bDayOff, Wx.bSeaIce, &States);
		}
		W.SetCondition("sail_season", M.bSeason);
		W.SetCondition("sail_kids", M.bKids);
		W.SetCondition("sail_juniors", M.bJuniors);
		W.SetCondition("sail_adults", M.bAdults);
		W.SetCondition("sail_race", M.bRace);
		W.SetCondition("sail_shore", M.bShore);
		W.SetCondition("sail_fitout", M.bFitOut);
		W.SetCondition("sail_watch", M.bRace);
		LyudPeople::SailPeople::Apply(W, bSchool ? LyudPeople::SailPeople::ProgramsAt(P, Date, W.Hour, W.bDayOff, LyudPeople::SailPeople::FromModel(M))
			: std::vector<LyudPeople::SailPeople::FProgram>());
	};
	// the day's go / no-go for each afloat session, decided at its start by the day's weather (the engine's state)
	auto Decide = [&]()
	{
		States.clear();
		for (const FTimetableSession& S : SchoolSessionsOn(P, Date, W.bDayOff))
		{
			if (!S.bAfloat) continue;
			const FSchoolGo G = SchoolDecide(P, Date, Wx, S.Classes);
			if (G.Go != ESchoolGo::Sail)
			{
				FSchoolSessionState St;
				St.Key = S.Key;
				St.Go = ESchoolGo::Shore;
				St.Reason = G.Reason;
				States.push_back(St);
			}
		}
	};
	int Overbooked = 0;
	const int Trace = std::getenv("LYUD_SAIL_TRACE") ? W.FindPerson(std::getenv("LYUD_SAIL_TRACE")) : -1;
	// a day From-To; bGoOn: the clock already stands at From (the night before was lived through: no Resettle)
	auto RunDay = [&](const FSchoolDate& D, float From, float To, const std::function<void()>& EachMinute, bool bGoOn = false)
	{
		Date = D;
		if (!bGoOn) C = FClock::At(SchoolWeekday(D), From);
		C.Set(W);
		Light();
		Decide();
		Drive();
		if (!bGoOn) W.Resettle();
		const float Dt = 2.f;
		for (double T = 0.0; T < (To - From) * 3600.0; T += Dt)
		{
			C.Seconds += Dt;
			C.Set(W);
			Light();
			const bool bMinute = std::fmod(T, 60.0) < Dt * 0.5;
			if (bMinute) Drive();
			W.Step(Dt);
			W.Events.clear();
			if (bMinute)
			{
				for (const FSlot& Sl : W.Slots) Overbooked += (int)Sl.Users.size() > Sl.Capacity ? 1 : 0;
				if (EachMinute) EachMinute();
				// LYUD_SAIL_TRACE=<key>: one person every five minutes (debugging)
				if (Trace >= 0 && std::fmod(T, 300.0) < Dt * 0.5)
				{
					const FPerson& Tp = W.People[(size_t)Trace];
					std::printf("    %02d:%02d %s %s at %s state %d prog %d\n", (int)W.Hour, (int)(std::fmod(W.Hour, 1.f) * 60.f), Tp.Key.c_str(),
						Tp.Activity >= 0 ? W.Activities[(size_t)Tp.Activity].Id.c_str() : "-", Tp.Place >= 0 ? W.Places[(size_t)Tp.Place].Id.c_str() : "-",
						(int)Tp.State, Tp.Program);
				}
			}
		}
	};
	// the night before D lived through in longer steps, 22:30-06:30 (a clock jump would leave them as tired as at bedtime);
	// bFresh: the clock set to 22:30 first and the town resettled there
	auto LiveNight = [&](const FSchoolDate& D, bool bFresh)
	{
		Date = D;
		if (bFresh)
		{
			C = FClock::At((SchoolWeekday(D) + 6) % 7, 22.5f);
			C.Set(W);
			Light();
			Drive();
			W.Resettle();
		}
		const double Until = C.Seconds + 8.0 * 3600.0;
		while (C.Seconds < Until)
		{
			C.Seconds += 4.0;
			C.Set(W);
			Light();
			if (std::fmod(C.Seconds, 60.0) < 2.0) Drive();
			W.Step(4.f);
			W.Events.clear();
		}
	};
	auto Afloat = [&](const FPerson& Pe)
	{
		return Pe.Activity >= 0 && W.Activities[(size_t)Pe.Activity].bAfloat && (Pe.State == EState::Doing || Pe.State == EState::Settling);
	};
	auto AtSchool = [&](const FPerson& Pe)
	{
		return Afloat(Pe) || (Pe.Place == School && Pe.Activity >= 0 && Pe.Program >= 0);
	};
	auto Hunger = [&](const FPerson& Pe)
	{
		return Pe.State == EState::Doing && Pe.Activity >= 0 && W.Activities[(size_t)Pe.Activity].Need[(int)ENeed::Hunger] >= 0.3f;
	};
	// the members a session calls (pupils and the adults' group; staff apart)
	auto Called = [&](const FTimetableSession& S, const FPerson& Pe)
	{
		if (!Pe.StaffRole.empty()) return false;
		if (S.Group == "race")
		{
			static const char* const Cls[] = { "race_optimist", "race_cadet", "race_luch" };
			for (int K = 0; K < 3; K++) if ((S.Classes >> K & 1) && W.IsMember(Pe, W.FindGroup(Cls[K]))) return true;
			return false;
		}
		if (S.Kind == "lesson" || S.Kind == "shore") return W.IsMember(Pe, W.FindGroup(S.Group));
		return false;
	};

	int AfloatSeen = 0, AfloatShown = 0, KidsDark = 0, FootIn = 0, FootWalk = 0;
	std::string KidsDarkWho, FootWho;
	// watches every minute of every day: who is afloat (unseen), no child afloat after sunset, nobody standing in the footprints
	auto Watchers = [&]()
	{
		const FSchoolDaylight L = SchoolDaylight(P, Date);
		for (const FPerson& Pe : W.People)
		{
			if (Afloat(Pe) && Pe.State == EState::Doing)
			{
				AfloatSeen++;
				AfloatShown += Pe.Pose == EPose::Hidden ? 0 : 1;
				if (W.IsMember(Pe, GKids) && Pe.StaffRole.empty() && W.Hour > L.Sunset)
				{
					KidsDark++;
					KidsDarkWho = Pe.Key;
				}
			}
			const bool bInFoot = InCorners(P.Boathouse.Corners, Pe.Pos.X, Pe.Pos.Y) || InCorners(P.Park.Corners, Pe.Pos.X, Pe.Pos.Y);
			const bool bOther = Pe.Place >= 0 && !IsSailPlace(W.Places[(size_t)Pe.Place]);
			if (bInFoot && bOther && Pe.Pose != EPose::Hidden && Pe.State == EState::Doing && Pe.Speed < 0.2f)
			{
				// standing or sitting there: a slot of another place inside the building or the park
				FootIn++;
				FootWho = Pe.Key + " at " + W.Places[(size_t)Pe.Place].Id + (Pe.Activity >= 0 ? " " + W.Activities[(size_t)Pe.Activity].Id : std::string());
			}
			else if (bInFoot && bOther && Pe.Pose != EPose::Hidden)
			{
				// walking through (the walk grid does not know the boathouse yet: docs/yacht_school.md "Open issues")
				FootWalk++;
			}
		}
	};

	// a week: each day 06:30-22:30, attendance by session, the head coach's and the bosun's hours, the children's day
	struct FWeekStats
	{
		std::vector<FAttend> Sessions;
		int HeadMin = 0, HeadAt = 0, HeadFound = 0, BosunMin = 0, BosunFound = 0;
		std::map<int, int> KidSchool, KidEat;       // weekday minutes at school (the town's school), at a meal
		int KidsAsleep = 0, KidsChecked = 0;
		int AfloatPeak = 0;
		std::map<int, double> NonMemberLoad;        // the Tuesday: non-members' person-minutes doing, by place
		int ParentsWaiting = 0, ParentsPeak = 0;
		int Spectators = 0, SpectatorPeak = 0;
		std::set<int> SpectatorIds;
		int RacersAfloatPeak = 0;
	};
	const int SchoolTown = W.FindPlace("P-SCHOOL");
	auto RunWeek = [&](const FSchoolDate& Monday, bool bSummer, FWeekStats& St)
	{
		for (int Dd = 0; Dd < 7; Dd++)
		{
			const FSchoolDate D = SchoolAddDays(Monday, Dd);
			const int Wd = SchoolWeekday(D);
			const std::vector<FTimetableSession> Plan = SchoolSessionsOn(P, D, false);
			const size_t First = St.Sessions.size();
			for (const FTimetableSession& S : Plan)
			{
				FAttend A;
				char Buf[64];
				std::snprintf(Buf, sizeof Buf, "%04d-%02d-%02d %s %s %.2f", D.Y, D.M, D.D, S.Group.c_str(), S.Kind.c_str(), S.From);
				A.What = Buf;
				St.Sessions.push_back(A);
			}
			LiveNight(D, Dd == 0);
			RunDay(D, 6.5f, 22.5f, [&]()
			{
				Watchers();
				const float H = W.Hour;
				for (size_t K = 0; K < Plan.size(); K++)
				{
					// counted from a quarter of an hour in to a quarter of an hour before the end (the walk there, the walk home)
					const FTimetableSession& S = Plan[K];
					if (H < S.From + 0.25 || H >= S.To - 0.25) continue;
					for (const int Id : W.MemberIds)
					{
						const FPerson& Pe = W.People[(size_t)Id];
						if (!Called(S, Pe)) continue;
						auto& Cnt = St.Sessions[First + K].ByMember[Id];
						Cnt.first++;
						Cnt.second += AtSchool(Pe) ? 1 : 0;
					}
				}
				// the head coach: Tue-Sat 10-13 at the boathouse, by her duty or her session
				if (Wd >= 1 && Wd <= 5 && H >= 10.25f && H < 12.75f)
				{
					const FPerson& Hc = W.People[(size_t)Head];
					St.HeadMin++;
					St.HeadAt += Hc.Place == School && (Hc.State == EState::Doing || Hc.State == EState::Settling) ? 1 : 0;
					std::vector<int> On;
					W.StaffOnShift("S-SAIL-SCHOOL", "coach_head", On);
					bool bFound = false;
					for (const int O : On) bFound |= O == Head;
					W.StaffOnShift("S-SAIL-SCHOOL", "coach", On);
					for (const int O : On) bFound |= O == Head;
					St.HeadFound += bFound ? 1 : 0;
				}
				if (H >= 9.5f && H < 18.5f)      // his day: 9-19
				{
					std::vector<int> On;
					W.StaffOnShift("S-SAIL-SCHOOL", "bosun", On);
					bool bFound = false;
					for (const int O : On) bFound |= O == Bosun;
					St.BosunMin++;
					St.BosunFound += bFound ? 1 : 0;
				}
				int Af = 0, Racers = 0, Waiting = 0, Watching = 0;
				for (const FPerson& Pe : W.People)
				{
					const bool bAf = Afloat(Pe);
					Af += bAf ? 1 : 0;
					Racers += bAf && Pe.StaffRole.empty() && Pe.Activity == W.FindActivity("sail_race") ? 1 : 0;
					if (Pe.Activity == Wait && Pe.State == EState::Doing) Waiting++;
					if (Pe.Activity == Watch && Pe.State == EState::Doing)
					{
						Watching++;
						St.SpectatorIds.insert(Pe.Id);
					}
				}
				St.AfloatPeak = std::max(St.AfloatPeak, Af);
				St.RacersAfloatPeak = std::max(St.RacersAfloatPeak, Racers);
				St.ParentsWaiting += Waiting;
				St.ParentsPeak = std::max(St.ParentsPeak, Waiting);
				St.Spectators += Watching;
				St.SpectatorPeak = std::max(St.SpectatorPeak, Watching);
				for (const int Id : W.MemberIds)
				{
					const FPerson& Pe = W.People[(size_t)Id];
					if (!W.IsMember(Pe, GKids) || !Pe.StaffRole.empty()) continue;
					St.KidSchool[Dd] += Pe.Place == SchoolTown && Pe.State == EState::Doing ? 1 : 0;
					St.KidEat[Id] += Hunger(Pe) ? 1 : 0;
					if (H >= 22.f && H < 22.1f)
					{
						St.KidsChecked++;
						St.KidsAsleep += Pe.Place == Pe.Home ? 1 : 0;
					}
				}
				if (Dd == 1)
				{
					for (const FPerson& Pe : W.People)
					{
						if (Pe.Member.empty() && Pe.State == EState::Doing && Pe.Place >= 0 && !IsSailPlace(W.Places[(size_t)Pe.Place]))
							St.NonMemberLoad[Pe.Place] += 1.0;
					}
				}
			}, true);
		}
		(void)bSummer;
	};
	auto Attendance = [&](const FWeekStats& St, int& Calls, int& Came, std::string& Worst)
	{
		Calls = Came = 0;
		double WorstFrac = 2.0;
		for (const FAttend& A : St.Sessions)
		{
			int SC = 0, SA = 0;
			for (const auto& KV : A.ByMember)
			{
				if (KV.second.first < 10) continue;
				SC++;
				SA += KV.second.second >= 0.6 * KV.second.first ? 1 : 0;
			}
			Calls += SC;
			Came += SA;
			if (SC > 0 && (double)SA / SC < WorstFrac)
			{
				WorstFrac = (double)SA / SC;
				char Buf[160];
				std::snprintf(Buf, sizeof Buf, "%s %d/%d", A.What.c_str(), SA, SC);
				Worst = Buf;
			}
		}
	};
	const auto T0 = std::chrono::steady_clock::now();
	const char* Only = std::getenv("LYUD_SAIL_ONLY");
	auto Want = [Only](const char* Part) { return !Only || std::strstr(Only, Part) != nullptr; };

	// ---- a term week in May (Monday 11 - Sunday 17, 2026): the Sunday regatta, the Wednesday evening races
	FWeekStats May;
	if (Want("may"))
	{
		Wx = FSchoolWeather();
		Wx.Wind = 4.5; Wx.Gust = 6.0; Wx.WaterC = 12.0;
		RunWeek(FSchoolDate(2026, 5, 11), false, May);
		int Calls = 0, Came = 0;
		std::string Worst;
		Attendance(May, Calls, Came, Worst);
		CHECK(Calls > 40 && Came >= 0.85 * Calls, "a term week in May: the members come to their sessions (%d of %d calls, %.0f%%; worst %s)", Came, Calls,
			Calls ? 100.0 * Came / Calls : 0.0, Worst.c_str());
		int SchoolDays = 0;
		for (int Dd = 0; Dd < 5; Dd++) SchoolDays += May.KidSchool[Dd] > 60 * 3 ? 1 : 0;
		int Fed = 0;
		for (const auto& KV : May.KidEat) Fed += KV.second >= 7 * 20 ? 1 : 0;
		CHECK(SchoolDays == 5, "... and the children still go to school every weekday (%d of 5 days with them there)", SchoolDays);
		CHECK(Fed == (int)May.KidEat.size() && Fed > 0, "... and eat (%d of %zu children at a meal 20+ minutes a day on average)", Fed, May.KidEat.size());
		CHECK(May.KidsChecked > 0 && May.KidsAsleep >= 0.9 * May.KidsChecked, "... and are at home by 22:00 (%d of %d child-evenings)", May.KidsAsleep, May.KidsChecked);
		CHECK(May.AfloatPeak >= 6, "boats out: up to %d people afloat at once", May.AfloatPeak);
		CHECK(May.HeadMin > 0 && May.HeadAt >= 0.8 * May.HeadMin && May.HeadFound >= 0.8 * May.HeadMin,
			"the head coach is at the boathouse Tue-Sat 10-13 (%d of %d minutes there, StaffOnShift finds her %d)", May.HeadAt, May.HeadMin, May.HeadFound);
		CHECK(May.BosunMin > 0 && May.BosunFound >= 0.8 * May.BosunMin, "the bosun is on at the boathouse 9-19 every day (StaffOnShift finds him %d of %d minutes)",
			May.BosunFound, May.BosunMin);
		CHECK(May.SpectatorIds.size() >= 8 && May.SpectatorPeak >= 4, "the regatta Sunday draws spectators (%zu people watch, up to %d at once)", May.SpectatorIds.size(),
			May.SpectatorPeak);
		CHECK(May.RacersAfloatPeak >= 6, "... racing: up to %d members afloat in the races", May.RacersAfloatPeak);
		CHECK(May.ParentsPeak >= 1, "the parents wait for the little ones (up to %d at once, %d person-minutes)", May.ParentsPeak, May.ParentsWaiting);
	}

	// ---- the town without the school: the same Tuesday, the school's conditions off, no programmes
	if (Want("may") && Want("town"))
	{
		bSchool = false;
		std::map<int, double> Without;
		// as the week's Tuesday: the Monday lived through first, then the night
		LiveNight(FSchoolDate(2026, 5, 11), true);
		RunDay(FSchoolDate(2026, 5, 11), 6.5f, 22.5f, nullptr, true);
		LiveNight(FSchoolDate(2026, 5, 12), false);
		RunDay(FSchoolDate(2026, 5, 12), 6.5f, 22.5f, [&]()
		{
			for (const FPerson& Pe : W.People)
			{
				if (Pe.Member.empty() && Pe.State == EState::Doing && Pe.Place >= 0 && !IsSailPlace(W.Places[(size_t)Pe.Place]))
					Without[Pe.Place] += 1.0;
			}
		}, true);
		bSchool = true;
		// by place (who sits on which bench is a matter of chance: reported) and by kind of place (the town's day: checked)
		auto Compare = [&](bool bByKind, std::string& WorstAt, double& WorstD)
		{
			std::map<std::string, std::pair<double, double>> Load;      // key -> (with the school, without)
			auto Key = [&](int Pl) { return bByKind ? W.Places[(size_t)Pl].Kind : W.Places[(size_t)Pl].Id; };
			for (const auto& KV : May.NonMemberLoad) Load[Key(KV.first)].first += KV.second;
			for (const auto& KV : Without) Load[Key(KV.first)].second += KV.second;
			double Sum = 0.0, Diff = 0.0;
			WorstD = 0.0;
			for (const auto& KV : Load)
			{
				const double D = std::fabs(KV.second.first - KV.second.second);
				Sum += KV.second.second;
				Diff += D;
				if (D > WorstD)
				{
					WorstD = D;
					WorstAt = KV.first;
				}
			}
			return Sum > 0.0 ? Diff / Sum : 1.0;
		};
		std::string WorstPlace, WorstKind;
		double WorstPlaceD = 0.0, WorstKindD = 0.0;
		const double ByPlace = Compare(false, WorstPlace, WorstPlaceD), ByKind = Compare(true, WorstKind, WorstKindD);
		CHECK(ByKind < 0.05, "the rest of the town's Tuesday about as without the school (non-members' person-minutes by kind of place: %.1f%% moved, most at %s, %.0f; "
			"by place %.1f%%, most at %s, %.0f)", 100.0 * ByKind, WorstKind.c_str(), WorstKindD, 100.0 * ByPlace, WorstPlace.c_str(), WorstPlaceD);
	}

	// ---- a holiday week in July (Monday 13 - Sunday 19): the children every weekday morning, the juniors after lunch
	if (Want("july"))
	{
		FWeekStats Jul;
		Wx = FSchoolWeather();
		Wx.Wind = 5.0; Wx.Gust = 6.5; Wx.WaterC = 19.0;
		RunWeek(FSchoolDate(2026, 7, 13), true, Jul);
		int Calls = 0, Came = 0;
		std::string Worst;
		Attendance(Jul, Calls, Came, Worst);
		CHECK(Calls > 80 && Came >= 0.85 * Calls, "a holiday week in July: the members come to their sessions (%d of %d calls, %.0f%%; worst %s)", Came, Calls,
			Calls ? 100.0 * Came / Calls : 0.0, Worst.c_str());
		int Fed = 0;
		for (const auto& KV : Jul.KidEat) Fed += KV.second >= 7 * 20 ? 1 : 0;
		CHECK(Fed == (int)Jul.KidEat.size() && Fed > 0, "... and the children eat (%d of %zu)", Fed, Jul.KidEat.size());
		CHECK(Jul.KidsChecked > 0 && Jul.KidsAsleep >= 0.9 * Jul.KidsChecked, "... and are at home by 22:00 (%d of %d child-evenings)", Jul.KidsAsleep, Jul.KidsChecked);
		CHECK(Jul.HeadMin > 0 && Jul.HeadFound >= 0.8 * Jul.HeadMin, "the head coach is findable Tue-Sat 10-13 in July too (%d of %d minutes)", Jul.HeadFound, Jul.HeadMin);
	}

	// ---- a day too windy for the boats: the children's Tuesday lesson held ashore
	if (Want("wind"))
	{
		Wx = FSchoolWeather();
		Wx.Wind = P.Wind[2][1] + 1.0;       // past even the luches' limit, short of a storm
		Wx.Gust = Wx.Wind + 2.0;
		const FSchoolDate D(2026, 5, 19);
		int AfloatMax = 0, ShoreMax = 0, CoachShoreMin = 0, KidsLesson = 0;
		RunDay(D, 13.5f, 18.5f, [&]()
		{
			int Af = 0, Sh = 0;
			for (const FPerson& Pe : W.People)
			{
				Af += Afloat(Pe) ? 1 : 0;
				Sh += Pe.Activity == Shore && Pe.State == EState::Doing && W.IsMember(Pe, GKids) ? 1 : 0;
			}
			AfloatMax = std::max(AfloatMax, Af);
			ShoreMax = std::max(ShoreMax, Sh);
			KidsLesson += W.IsCondition("sail_kids") && W.IsCondition("sail_shore") ? 1 : 0;
			CoachShoreMin += W.People[(size_t)KidsCoach].Activity == CoachShore ? 1 : 0;
		});
		CHECK(!States.empty() && KidsLesson > 60, "a %.0f m/s day: the go / no-go says ashore (sail_kids with sail_shore %d minutes)", Wx.Wind, KidsLesson);
		CHECK(AfloatMax == 0, "... nobody afloat (at most %d)", AfloatMax);
		CHECK(ShoreMax >= 5, "... the children at the shore lesson (up to %d at once), their coach teaching it (%d minutes)", ShoreMax, CoachShoreMin);
		CHECK(CoachShoreMin > 30, "... the children's coach at sail_coach_shore %d minutes", CoachShoreMin);
	}

	// ---- out of season: nobody afloat; the fitting-out in April (the staff on Wednesday, the members as well on Saturday)
	if (Want("season"))
	{
		Wx = FSchoolWeather();
		Wx.Wind = 4.0; Wx.WaterC = 6.0;
		const FSchoolDate Open = SeasonOpening(P, 2026);
		const FSchoolDate Wed = SchoolWeekdayOnOrAfter(SchoolAddDays(Open, -P.FitoutDays), 2), Sat = SchoolWeekdayOnOrAfter(SchoolAddDays(Open, -P.FitoutDays), 5);
		CHECK(SeasonStateOn(P, Wed) == ESeasonState::FitOut && SeasonStateOn(P, Sat) == ESeasonState::FitOut, "the fitting-out fortnight: %04d-%02d-%02d (Wed) and %04d-%02d-%02d (Sat)",
			Wed.Y, Wed.M, Wed.D, Sat.Y, Sat.M, Sat.D);
		int AfloatMax = 0, StaffWork = 0, CrewWorkSat = 0, CrewWorkWed = 0;
		std::set<int> StaffIds, CrewIds;
		auto Count = [&](bool bSat)
		{
			for (const int Id : W.MemberIds)
			{
				const FPerson& Pe = W.People[(size_t)Id];
				AfloatMax = std::max(AfloatMax, Afloat(Pe) ? 1 : 0);
				const bool bWork = Pe.Place == School && Pe.State == EState::Doing && Pe.Program >= 0;
				if (bWork && W.IsMember(Pe, GStaff))
				{
					StaffWork++;
					StaffIds.insert(Id);
				}
				else if (bWork && W.IsMember(Pe, GCrew))
				{
					(bSat ? CrewWorkSat : CrewWorkWed)++;
					if (bSat) CrewIds.insert(Id);
				}
			}
			for (const FPerson& Pe : W.People) AfloatMax = std::max(AfloatMax, Afloat(Pe) ? 1 : 0);
		};
		RunDay(Wed, 9.f, 18.f, [&]() { Count(false); });
		const int StaffWed = (int)StaffIds.size();
		RunDay(Sat, 9.f, 18.f, [&]() { Count(true); });
		CHECK(AfloatMax == 0, "out of season nobody is afloat");
		CHECK(StaffWed >= 3 && StaffWork > 300, "the fitting-out: %d of the staff at work in the boathouse on the Wednesday (%d person-minutes in both days)", StaffWed, StaffWork);
		CHECK(CrewIds.size() >= 5 && CrewWorkWed == 0, "... and on the Saturday %zu members come to help (%d person-minutes; %d on the Wednesday)", CrewIds.size(), CrewWorkSat, CrewWorkWed);
		// laid up: a March week has no boats, the theory evenings only
		int AfloatMar = 0;
		RunDay(FSchoolDate(2026, 3, 14), 9.f, 19.f, [&]() { for (const FPerson& Pe : W.People) AfloatMar += Afloat(Pe) ? 1 : 0; });
		CHECK(AfloatMar == 0, "laid up (a Saturday in March, the theory evening): nobody afloat");
	}

	// ---- the dark: a late October Tuesday, the children's lesson cut short by the sunset
	if (Want("dark"))
	{
		Wx = FSchoolWeather();
		Wx.Wind = 5.0; Wx.WaterC = 10.0;
		const FSchoolDate D(2026, 10, 27);
		const FSchoolDaylight L = SchoolDaylight(P, D);
		int KidsAfloat = 0;
		RunDay(D, 13.5f, 19.f, [&]()
		{
			Watchers();
			for (const int Id : W.MemberIds) KidsAfloat += Afloat(W.People[(size_t)Id]) && W.IsMember(W.People[(size_t)Id], GKids) ? 1 : 0;
		});
		CHECK(KidsAfloat > 0, "late October (sunset %02d:%02d): the children still go out for a while (%d child-minutes afloat)", (int)L.Sunset,
			(int)(std::fmod(L.Sunset, 1.0) * 60.0), KidsAfloat);
	}
	CHECK(KidsDark == 0, "no child afloat after sunset (%d child-minutes%s%s)", KidsDark, KidsDarkWho.empty() ? "" : ", ", KidsDarkWho.c_str());
	CHECK(AfloatSeen == 0 || AfloatShown == 0, "afloat they are unseen by the life: the engine draws them in the boats (%d of %d person-minutes shown)", AfloatShown, AfloatSeen);
	CHECK(FootIn == 0, "nobody stays at another place inside the boathouse's or the park's footprint (%d person-minutes%s%s)", FootIn, FootWho.empty() ? "" : "; ",
		FootWho.c_str());
	std::printf("  note: %d person-minutes walking through the footprints (the walk grid does not know the boathouse yet)\n", FootWalk);
	CHECK(Overbooked == 0, "no slot overbooked (%d slot-minutes)", Overbooked);
	std::printf("  sailing's checks took %.1f s\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count());
	return Failures > Fails0 ? 1 : 0;
}
