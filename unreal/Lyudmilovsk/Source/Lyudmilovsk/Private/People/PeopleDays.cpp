// The days of the people's life (docs/town_life.md "The days"): what makes one day unlike another. Part of
// LyudPeople::FWorld (PeopleSim.h), in plain C++ like the rest of it, so tools/peoplesim runs it outside the engine.
//
// Everything here is worked out from the calendar's day and who somebody is (a hash of the seed, the day and their id),
// so nothing of it is saved: a load on the same day gives the same day.
//
//  - what today is for somebody (FPerson::Today): a cold (two days at home, no school), a bad night (up late, coffee, a
//    nap), their birthday (guests in the evening), the pay (every second Friday: the cafe, the shops), the pension
//    (every second Wednesday: the office in the morning, then the market);
//  - the day's plan with somebody (FPerson::PlanAct..., FWorld::MeetNow 3): tea at a friend's flat, a birthday's guests,
//    a couple's evening stroll on the sea front, chess on a bench, Sunday dinner at the grandparents';
//  - their own weekday for an errand ("own_day": the library on Thursdays) and what they feel like today (DayMood);
//  - on the way: two who know each other stop for a word or greet in passing (StreetMeets), a door is held for whoever
//    comes behind (HoldDoor).
//
// Cost: RollToday runs once a day (a few passes over the people); StreetMeets every two seconds over those out walking
// only (a few hundred at most); HoldDoor once a way, at a door.
#include "PeopleSim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>

namespace LyudPeople
{
	namespace
	{

		uint32_t Mix(uint32_t A, uint32_t B, uint32_t C = 0x9e3779b9u)
		{
			uint32_t H = A * 2654435761u ^ (B + 0x7f4a7c15u) * 2246822519u ^ C * 3266489917u;
			H ^= H >> 15;
			H *= 2246822519u;
			H ^= H >> 13;
			H *= 3266489917u;
			H ^= H >> 16;
			return H;
		}

		/** 0 .. 1 */
		float Unit(uint32_t H) { return (float)(H >> 8) / 16777216.f; }

		float YawTo(const V3& From, const V3& To)
		{
			return std::atan2(To.Y - From.Y, To.X - From.X) * 57.29578f;
		}

		bool InHoursOf(float From, float To, float H)
		{
			From = std::fmod(From + 48.f, 24.f);
			To = std::fmod(To + 48.f, 24.f);
			return From <= To ? (H >= From && H < To) : (H >= From || H < To);
		}

		/** The family name's part of a townsperson's key (t_<family>_<given>), "" for anybody else. */
		std::string FamilyOf(const FPerson& P)
		{
			if (!P.bTown || P.Key.size() < 4 || P.Key.compare(0, 2, "t_") != 0)
			{
				return std::string();
			}
			const size_t End = P.Key.rfind('_');
			return End != std::string::npos && End > 2 ? P.Key.substr(2, End - 2) : std::string();
		}

		const char* const TodayNames[NumTodayBits] = { "cold", "sleepy", "birthday", "payday", "pension", "guest" };
		const char* const PlanNames[(int)EPlan::Num] = { "", "tea", "birthday", "stroll", "chess", "family" };
	}

	const char* TodayName(int Bit) { return Bit >= 0 && Bit < NumTodayBits ? TodayNames[Bit] : ""; }
	const char* PlanName(EPlan P) { return P < EPlan::Num ? PlanNames[(int)P] : ""; }

	uint8_t TodayFromName(const std::string& S)
	{
		for (int B = 0; B < NumTodayBits; B++)
		{
			if (S == TodayNames[B])
			{
				return (uint8_t)(1 << B);
			}
		}
		return 0;
	}

	int FWorld::OwnDayOf(const FPerson& P, const FActivity& A) const
	{
		int Days[7], Num = 0;
		for (int D = 0; D < 7; D++)
		{
			if ((A.Days >> D) & 1)
			{
				Days[Num++] = D;
			}
		}
		if (Num == 0)
		{
			return -1;
		}
		const uint32_t H = Mix((uint32_t)P.Id, (uint32_t)(&A - Activities.data()), Opt.Seed ^ 0x51ed270bu);
		return Days[H % (uint32_t)Num];
	}

	float FWorld::DayMood(const FPerson& P, const FActivity& A) const
	{
		return Unit(Mix((uint32_t)P.Id, (uint32_t)TodayDay, (uint32_t)(&A - Activities.data()) + Opt.Seed)) - 0.5f;
	}

	bool FWorld::BusyAt(const FPerson& P, float AtHour, float ForHours) const
	{
		// asleep (their usual hours, a little either side), or at a duty of today's
		for (const float H : { AtHour, AtHour + ForHours })
		{
			const float At = std::fmod(H + 24.f, 24.f);
			if (InHoursOf(P.BedTime + P.BedJ - 0.4f, P.Wake + P.WakeJ + P.LateH + 0.6f, At))
			{
				return true;
			}
			for (const FDuty& D : P.Duties)
			{
				const int A = ActOf(D);
				if (!((D.Days >> Weekday) & 1) || (D.StayDay >= 0 && D.StayDay != StayDayOf(P)) || A < 0)
				{
					continue;
				}
				const FActivity& Act = Activities[(size_t)A];
				if ((bDayOff && Act.bHolidayOff) || ((P.Today & TodayCold) && Act.bSickOff))
				{
					continue;
				}
				if (InHoursOf(D.From - 0.6f, D.To + 0.35f, At))
				{
					return true;
				}
			}
		}
		// their weekly date or the household's outing is today round this hour
		if (P.DateAct >= 0 && P.DateDay == Weekday && std::fabs(P.DateHour - AtHour) < 2.25f)
		{
			return true;
		}
		return P.OutingAct >= 0 && P.OutingDay == Weekday && std::fabs(P.OutingHour - AtHour) < 2.25f;
	}

	void FWorld::ClearPlan(FPerson& P)
	{
		P.PlanKind = EPlan::None;
		P.bPlanHost = false;
		P.PlanAct = P.PlanPlace = P.PlanWith = -1;
		P.PlanHour = 0.f;
	}

	int FWorld::NearestPlaceFor(const FActivity& A, const V3& Near, int Island, int Room) const
	{
		int Best = -1;
		float BestD = 1e30f;
		for (const int Pi : A.PlaceIdx)
		{
			const FPlace& Pl = Places[(size_t)Pi];
			if (Pl.Island != Island)
			{
				continue;
			}
			const float D = (Pl.Portal - Near).Len2D();
			if (D >= BestD)
			{
				continue;
			}
			int Seats = 0;
			for (const int S : Pl.Slots)
			{
				const FSlot& Sl = Slots[(size_t)S];
				if (Sl.Owner == -1 && (A.SlotK.empty() || std::find(A.SlotK.begin(), A.SlotK.end(), Sl.K) != A.SlotK.end()))
				{
					Seats += Sl.Capacity;
				}
			}
			if (Seats >= Room)
			{
				BestD = D;
				Best = Pi;
			}
		}
		return Best;
	}

	void FWorld::SetToday(int Person, uint8_t Bits)
	{
		if (Person >= 0 && Person < (int)People.size())
		{
			FPerson& P = People[(size_t)Person];
			P.Today = Bits;
			P.LateH = 0.f;
			P.OnceDone = 0;
			P.Recheck = std::min(P.Recheck, 2.f);
		}
	}

	bool FWorld::MakePlan(int Person, int With, EPlan Kind, float AtHour)
	{
		if (Person < 0 || Person >= (int)People.size() || With < 0 || With >= (int)People.size() || Person == With)
		{
			return false;
		}
		FPerson& P = People[(size_t)Person];
		FPerson& Q = People[(size_t)With];
		const int Visit = PlanActs[0], Host = PlanActs[1], Stroll = PlanActs[2], Chess = PlanActs[3];
		if (P.Home < 0 || Q.Home < 0)
		{
			return false;
		}
		const int Island = std::max(0, Places[(size_t)P.Home].Island);
		int ActP = -1, ActQ = -1, Place = -1;
		switch (Kind)
		{
		case EPlan::Tea:
		case EPlan::Birthday:
		case EPlan::Family:
			// P is the guest, at Q's
			ActP = Visit;
			ActQ = Host;
			Place = P.Home != Q.Home ? Q.Home : -1;
			break;
		case EPlan::Stroll:
			ActP = ActQ = Stroll;
			Place = Stroll >= 0 ? NearestPlaceFor(Activities[(size_t)Stroll], Places[(size_t)P.Home].Portal, Island, 2) : -1;
			break;
		case EPlan::Chess:
			ActP = ActQ = Chess;
			Place = Chess >= 0 ? NearestPlaceFor(Activities[(size_t)Chess], (Places[(size_t)P.Home].Portal + Places[(size_t)Q.Home].Portal) * 0.5f, Island, 2) : -1;
			break;
		default:
			break;
		}
		if (ActP < 0 || ActQ < 0 || Place < 0)
		{
			return false;
		}
		const bool bAtHome = ActQ == Host;
		P.PlanKind = Kind;
		P.PlanAct = ActP;
		P.PlanPlace = Place;
		P.PlanWith = With;
		P.PlanHour = AtHour;
		P.bPlanHost = false;
		// (a host with guests already keeps the first of them as the one they sit with)
		if (Q.PlanAct < 0 || !bAtHome || !Q.bPlanHost)
		{
			Q.PlanKind = Kind;
			Q.PlanAct = ActQ;
			Q.PlanPlace = Place;
			Q.PlanWith = Person;
			Q.PlanHour = AtHour;
			Q.bPlanHost = bAtHome;
		}
		if (Kind == EPlan::Birthday)
		{
			P.Today |= TodayGuest;
		}
		P.Recheck = std::min(P.Recheck, 3.f);
		Q.Recheck = std::min(Q.Recheck, 3.f);
		return true;
	}

	void FWorld::RollToday(bool bForce)
	{
		if (!bForce && TodayDay == DayIndex())
		{
			return;
		}
		TodayDay = DayIndex();
		const uint32_t Day = (uint32_t)TodayDay;
		const uint32_t Seed = Opt.Seed;
		const int Visit = PlanActs[0], Host = PlanActs[1], Stroll = PlanActs[2], Chess = PlanActs[3];
		const bool bWeekend = Weekday >= 5;

		// --- what today is for each -----------------------------------------------------------------------
		for (FPerson& P : People)
		{
			ClearPlan(P);
			P.Today = 0;
			P.LateH = 0.f;
			P.OnceDone = 0;
			// the townsfolk only: the story's people keep their days (quests count on them), a sanatorium's guest the regime
			if (!P.bTown || P.StayCycle > 0 || P.Home < 0)
			{
				continue;
			}
			const uint32_t Id = (uint32_t)P.Id;
			// a cold: begun today or yesterday (two days), about three in a hundred at a time
			if (Unit(Mix(Id, Day, Seed ^ 0xc01du)) < 0.016f || Unit(Mix(Id, Day - 1u, Seed ^ 0xc01du)) < 0.016f)
			{
				P.Today |= TodayCold;
			}
			// a bad night (not the children's): up later, as far as the day's first duty lets them
			else if (P.Age >= 14 && Unit(Mix(Id, Day, Seed ^ 0x51eeu)) < 0.07f)
			{
				P.Today |= TodaySleepy;
				const float Usual = P.Wake + P.WakeJ;
				if (Usual > 4.f && Usual < 11.f)
				{
					float Late = 0.4f + 0.7f * Unit(Mix(Id, Day, Seed ^ 0x1a7eu));
					for (const FDuty& D : P.Duties)
					{
						if (((D.Days >> Weekday) & 1) && D.From > Usual - 1.f)
						{
							Late = std::min(Late, D.From - 0.7f - Usual);
						}
					}
					P.LateH = Late > 0.15f ? Late : 0.f;
				}
			}
			if (Mix(Id * 7919u + 17u, Seed, 0xb1d7u) % 365u == Day % 365u)
			{
				P.Today |= TodayBirthday;
			}
			const bool bWorks = P.Age >= 18 && !P.Duties.empty() && P.Occupation != "pupil" && P.Occupation != "student"
				&& P.Occupation != "pensioner" && P.Occupation != "unemployed" && P.Occupation != "homemaker";
			// (Week counts Mondays from a day the game's first week makes even: the pension on its Wednesday, the pay on its Friday)
			const bool bEven = ((Week % 2) + 2) % 2 == 0;
			if (bWorks && Weekday == 4 && bEven)
			{
				P.Today |= TodayPayday;
			}
			if (P.Age >= 60 && !bWorks && Weekday == 2 && bEven)
			{
				P.Today |= TodayPension;
			}
		}
		if (Visit < 0 || Host < 0)
		{
			return;     // no plans' activities in this data (the tests' made-up town)
		}

		// --- the day's plans --------------------------------------------------------------------------------
		// friends by id (everybody's, once: PickSlot's company uses the same list)
		bool bNeedIds = false;
		for (const FPerson& P : People)
		{
			bNeedIds |= !P.bFriendIds && !P.Friends.empty();
		}
		if (bNeedIds)
		{
			std::unordered_map<std::string, int> ByKey;
			ByKey.reserve(People.size() * 2);
			for (const FPerson& P : People)
			{
				ByKey.emplace(P.Key, P.Id);
			}
			for (const FPerson& P : People)
			{
				if (P.bFriendIds)
				{
					continue;
				}
				P.bFriendIds = true;
				P.FriendIds.clear();
				for (const std::string& K : P.Friends)
				{
					const auto It = ByKey.find(K);
					if (It != ByKey.end() && It->second != P.Id)
					{
						P.FriendIds.push_back(It->second);
					}
				}
				std::sort(P.FriendIds.begin(), P.FriendIds.end());
			}
		}
		auto CanPlan = [this](const FPerson& P)
		{
			return P.bTown && P.StayCycle == 0 && P.Home >= 0 && P.PlanAct < 0 && !(P.Today & TodayCold) && Places[(size_t)P.Home].Island >= 0;
		};
		auto FreeHour = [this](const FPerson& A, const FPerson* B, const float* Hours, int Num, uint32_t H, float Len) -> float
		{
			// one of a few hours both are free at (tried from a hashed start), -1 none
			for (int Try = 0; Try < Num; Try++)
			{
				const float At = Hours[(H + (uint32_t)Try) % (uint32_t)Num] + 0.5f * Unit(Mix(H, (uint32_t)Try, 0x40e5u));
				if (!BusyAt(A, At, Len) && (!B || !BusyAt(*B, At, Len)))
				{
					return At;
				}
			}
			return -1.f;
		};
		auto SetPlan = [](FPerson& P, EPlan Kind, int Act, int Place, int With, float AtHour, bool bHost)
		{
			P.PlanKind = Kind;
			P.PlanAct = Act;
			P.PlanPlace = Place;
			P.PlanWith = With;
			P.PlanHour = AtHour;
			P.bPlanHost = bHost;
		};
		auto Housemates = [this](const FPerson& P, std::vector<int>& Out)
		{
			Out.clear();
			for (const int R : Places[(size_t)P.Home].Residents)
			{
				if (R != P.Id && People[(size_t)R].Household == P.Household)
				{
					Out.push_back(R);
				}
			}
		};
		std::vector<int> Mates;

		// a birthday: the family at home, a few friends as guests
		for (FPerson& P : People)
		{
			if (!(P.Today & TodayBirthday) || !CanPlan(P))
			{
				continue;
			}
			static const float KidHours[] = { 16.f, 16.5f, 15.5f }, Hours[] = { 18.5f, 19.f, 18.f, 19.5f }, FreeDay[] = { 15.f, 17.f, 18.f, 16.f };
			const uint32_t H = Mix((uint32_t)P.Id, Day, Seed ^ 0xb17du);
			const float At = P.Age < 13 ? FreeHour(P, nullptr, KidHours, 3, H, 1.5f) : FreeHour(P, nullptr, bWeekend ? FreeDay : Hours, 4, H, 1.5f);
			if (At < 0.f)
			{
				continue;
			}
			SetPlan(P, EPlan::Birthday, Host, P.Home, -1, At, true);
			Housemates(P, Mates);
			int AtHome = 1;
			for (const int R : Mates)
			{
				FPerson& Q = People[(size_t)R];
				if (CanPlan(Q) && !BusyAt(Q, At, 1.f))
				{
					SetPlan(Q, EPlan::Birthday, Host, P.Home, P.Id, At, false);
					Q.Today |= TodayGuest;
					AtHome++;
				}
			}
			int Guests = 0;
			const int Room = std::max(1, 5 - AtHome);
			for (const int F : P.FriendIds)
			{
				FPerson& Q = People[(size_t)F];
				// (a child's guests are children; nobody is asked to their own flat)
				if (Guests >= Room || !CanPlan(Q) || Q.Home == P.Home || Places[(size_t)Q.Home].Island != Places[(size_t)P.Home].Island
					|| (P.Age < 13 && Q.Age > 15) || BusyAt(Q, At, 1.5f))
				{
					continue;
				}
				SetPlan(Q, EPlan::Birthday, Visit, P.Home, P.Id, At + 0.1f * (float)Guests, false);
				Q.Today |= TodayGuest;
				if (P.PlanWith < 0)
				{
					P.PlanWith = Q.Id;
				}
				Guests++;
			}
			if (P.PlanWith < 0 && !Mates.empty())
			{
				P.PlanWith = Mates[0];
			}
		}

		// Sunday dinner at the grandparents': a household of the same family name with nobody over the elders' age less 18
		// goes to theirs (every other Sunday or so)
		if (Weekday == 6)
		{
			std::map<std::string, std::vector<int>> Families;      // family name -> the eldest of each household of it
			std::map<std::string, int> Eldest;
			for (const FPerson& P : People)
			{
				if (!P.bTown || P.Home < 0 || P.StayCycle > 0 || P.Household.empty())
				{
					continue;
				}
				const auto It = Eldest.find(P.Household);
				if (It == Eldest.end() || People[(size_t)It->second].Age < P.Age)
				{
					Eldest[P.Household] = P.Id;
				}
			}
			for (const auto& E : Eldest)
			{
				const std::string Family = FamilyOf(People[(size_t)E.second]);
				if (!Family.empty())
				{
					Families[Family].push_back(E.second);
				}
			}
			for (const auto& Fam : Families)
			{
				const std::vector<int>& Heads = Fam.second;
				for (const int Old : Heads)
				{
					FPerson& Elder = People[(size_t)Old];
					if (Elder.Age < 60 || !CanPlan(Elder))
					{
						continue;
					}
					for (const int Young : Heads)
					{
						FPerson& Head = People[(size_t)Young];
						if (Young == Old || Head.Age > Elder.Age - 18 || Head.Home == Elder.Home || !CanPlan(Head)
							|| Places[(size_t)Head.Home].Island != Places[(size_t)Elder.Home].Island
							|| Unit(Mix((uint32_t)Old, (uint32_t)Young, Day ^ Seed)) > 0.55f)
						{
							continue;
						}
						static const float Hours[] = { 13.f, 13.5f, 14.f, 12.5f };
						const float At = FreeHour(Elder, &Head, Hours, 4, Mix((uint32_t)Old, Day, 0xfa3u), 1.5f);
						if (At < 0.f)
						{
							continue;
						}
						SetPlan(Elder, EPlan::Family, Host, Elder.Home, Head.Id, At, true);
						Housemates(Elder, Mates);
						int Seats = 5;
						for (const int R : Mates)
						{
							FPerson& Q = People[(size_t)R];
							if (CanPlan(Q) && !BusyAt(Q, At, 1.f))
							{
								SetPlan(Q, EPlan::Family, Host, Elder.Home, Head.Id, At, false);
								Seats--;
							}
						}
						SetPlan(Head, EPlan::Family, Visit, Elder.Home, Elder.Id, At, false);
						Housemates(Head, Mates);
						for (const int R : Mates)
						{
							FPerson& Q = People[(size_t)R];
							if (Seats > 1 && CanPlan(Q) && !BusyAt(Q, At, 1.5f))
							{
								SetPlan(Q, EPlan::Family, Visit, Elder.Home, Elder.Id, At, false);
								Seats--;
							}
						}
						break;      // one family to dinner
					}
				}
			}
		}

		// a couple's evening stroll on the sea front: two grown-ups of a household, a man and a woman of an age, with no
		// small child to leave at home; about one evening in three (more on Friday and Saturday)
		if (Stroll >= 0)
		{
			const FActivity& A = Activities[(size_t)Stroll];
			for (FPerson& P : People)
			{
				if (!CanPlan(P) || P.Age < 20 || P.Household.empty())
				{
					continue;
				}
				Housemates(P, Mates);
				int Other = -1, Grown = 0;
				bool bSmall = false;
				for (const int R : Mates)
				{
					const FPerson& Q = People[(size_t)R];
					bSmall |= Q.Age < 12;
					if (Q.Age >= 20)
					{
						Grown++;
						Other = R;
					}
				}
				if (Grown != 1 || bSmall || Other < P.Id)
				{
					continue;       // (each couple once: from the one with the lower id)
				}
				FPerson& Q = People[(size_t)Other];
				const uint32_t H = Mix((uint32_t)P.Id, Day, Seed ^ 0x57011u);
				if (!CanPlan(Q) || Q.bFemale == P.bFemale || std::abs(Q.Age - P.Age) > 15
					|| Unit(H) > ((Weekday == 4 || Weekday == 5) ? 0.45f : 0.3f))
				{
					continue;
				}
				// towards sunset, not before half past five nor after half past eight; the old a little earlier
				float At = Sunset - 0.4f + 1.4f * Unit(Mix(H, 1u)) - (std::min(P.Age, Q.Age) >= 65 ? 0.7f : 0.f);
				At = std::max(17.5f, std::min(20.5f, At));
				if (BusyAt(P, At, 1.f) || BusyAt(Q, At, 1.f))
				{
					continue;
				}
				const int Island = Places[(size_t)P.Home].Island;
				const int Place = NearestPlaceFor(A, Places[(size_t)P.Home].Portal, Island, 2);
				if (Place < 0)
				{
					continue;
				}
				SetPlan(P, EPlan::Stroll, Stroll, Place, Q.Id, At, false);
				SetPlan(Q, EPlan::Stroll, Stroll, Place, P.Id, At, false);
			}
		}

		// tea at a friend's flat: the old and the sociable more, everybody more at the weekend
		for (FPerson& P : People)
		{
			if (!CanPlan(P) || P.Age < 14 || P.FriendIds.empty())
			{
				continue;
			}
			const uint32_t H = Mix((uint32_t)P.Id, Day, Seed ^ 0x7ea7u);
			const float Chance = 0.05f * (0.6f + 0.8f * P.Social) * (P.Age >= 60 ? 1.6f : 1.f) * (bWeekend ? 1.5f : 1.f);
			if (Unit(H) > Chance)
			{
				continue;
			}
			const int Num = (int)P.FriendIds.size();
			for (int Try = 0; Try < 3 && P.PlanAct < 0; Try++)
			{
				FPerson& Q = People[(size_t)P.FriendIds[(size_t)((H >> 9) + (uint32_t)Try) % (size_t)Num]];
				if (!CanPlan(Q) || Q.Age < 14 || Q.Home == P.Home || Places[(size_t)Q.Home].Island != Places[(size_t)P.Home].Island)
				{
					continue;
				}
				static const float DayHours[] = { 11.f, 15.5f, 16.5f, 12.f }, EveHours[] = { 17.5f, 18.5f, 19.f, 18.f },
					WeekendHours[] = { 12.f, 14.5f, 16.f, 17.5f };
				const bool bOld = P.Age >= 60 && Q.Age >= 60;
				const float At = FreeHour(P, &Q, bOld ? DayHours : (bWeekend ? WeekendHours : EveHours), 4, Mix(H, (uint32_t)Try), 1.25f);
				if (At < 0.f)
				{
					continue;
				}
				SetPlan(P, EPlan::Tea, Visit, Q.Home, Q.Id, At, false);
				SetPlan(Q, EPlan::Tea, Host, Q.Home, P.Id, At, true);
			}
		}

		// chess on a bench between their doors: two friends of 40 and over, by daylight
		if (Chess >= 0)
		{
			const FActivity& A = Activities[(size_t)Chess];
			for (FPerson& P : People)
			{
				if (!CanPlan(P) || P.Age < 45 || P.FriendIds.empty())
				{
					continue;
				}
				const uint32_t H = Mix((uint32_t)P.Id, Day, Seed ^ 0xc4e55u);
				if (Unit(H) > (P.Age >= 60 ? 0.1f : 0.05f) * (0.5f + P.Studious))
				{
					continue;
				}
				const int Num = (int)P.FriendIds.size();
				for (int Try = 0; Try < 3 && P.PlanAct < 0; Try++)
				{
					FPerson& Q = People[(size_t)P.FriendIds[(size_t)((H >> 9) + (uint32_t)Try) % (size_t)Num]];
					if (!CanPlan(Q) || Q.Age < 40 || Q.Home == P.Home || Places[(size_t)Q.Home].Island != Places[(size_t)P.Home].Island)
					{
						continue;
					}
					static const float DayHours[] = { 10.5f, 11.5f, 15.f, 14.f }, EveHours[] = { 16.f, 15.f, 16.5f, 14.5f };
					const float At = FreeHour(P, &Q, P.Age >= 60 && Q.Age >= 60 ? DayHours : EveHours, 4, Mix(H, (uint32_t)Try), 1.25f);
					if (At < 0.f || At + 0.75f > Sunset || At < Sunrise + 0.5f)
					{
						continue;
					}
					const int Place = NearestPlaceFor(A, (Places[(size_t)P.Home].Portal + Places[(size_t)Q.Home].Portal) * 0.5f, Places[(size_t)P.Home].Island, 4);
					if (Place < 0)
					{
						continue;
					}
					SetPlan(P, EPlan::Chess, Chess, Place, Q.Id, At, false);
					SetPlan(Q, EPlan::Chess, Chess, Place, P.Id, At, false);
				}
			}
		}
	}

	void FWorld::StreetMeets()
	{
		// whoever is out walking somewhere they chose, not in a hurry
		static thread_local std::vector<int> Walk;
		Walk.clear();
		for (const FPerson& P : People)
		{
			if (P.State == EState::Going && !P.bIndoors && P.Pose == EPose::Walk && P.Bike < 0 && P.BikeAct == EBikeAct::None && !P.bHurry
				&& !P.bTrain && P.MeetFor <= 0.f && P.DoorFor <= 0.f && P.HoldFor <= 0.f && P.OffGrid < 0 && Time >= P.MeetAgainAt && P.Activity >= 0
				&& P.Activity != SleepAct && P.PathI < (int)P.Path.size() && P.Path[(size_t)P.PathI].Kind == 0 && Walk.size() < 400)
			{
				Walk.push_back(P.Id);
			}
		}
		for (size_t I = 0; I < Walk.size(); I++)
		{
			FPerson& A = People[(size_t)Walk[I]];
			for (size_t J = I + 1; J < Walk.size() && Time >= A.MeetAgainAt; J++)
			{
				FPerson& B = People[(size_t)Walk[J]];
				const V3 D = B.Pos - A.Pos;
				if (std::fabs(D.X) > 3.5f || std::fabs(D.Y) > 3.5f || std::fabs(D.Z) > 1.5f || D.Len2D() > 3.5f || Time < B.MeetAgainAt)
				{
					continue;
				}
				// going the same way together already (a household's outing, a couple's stroll, a date): nothing to stop for
				if (A.Place == B.Place && A.Place >= 0)
				{
					continue;
				}
				if (!A.bFriendIds || !B.bFriendIds)
				{
					continue;       // (the lists are made by the day's first roll)
				}
				const bool bFriends = std::binary_search(A.FriendIds.begin(), A.FriendIds.end(), B.Id)
					|| std::binary_search(B.FriendIds.begin(), B.FriendIds.end(), A.Id);
				const bool bHouse = !A.Household.empty() && A.Household == B.Household;
				bool bKnow = bFriends || bHouse;
				if (!bKnow && A.Home >= 0 && B.Home >= 0)
				{
					// neighbours of one house, people of one workplace
					const std::string& HA = Places[(size_t)A.Home].Building;
					bKnow = !HA.empty() && HA == Places[(size_t)B.Home].Building;
					for (size_t DA = 0; DA < A.Duties.size() && !bKnow; DA++)
					{
						for (const FDuty& DB : B.Duties)
						{
							bKnow |= A.Duties[DA].Place >= 0 && A.Duties[DA].Place == DB.Place;
						}
					}
				}
				if (!bKnow)
				{
					continue;
				}
				const uint32_t H = Mix((uint32_t)A.Id, (uint32_t)B.Id, (uint32_t)(Time / 2.0));
				const FActivity& ActA = Activities[(size_t)A.Activity];
				const FActivity& ActB = Activities[(size_t)B.Activity];
				// a word: friends and their own, with time for it (free, bound for nothing that waits), not in the rain or the
				// dead of night; the sociable more
				const bool bTime = ActA.bFree && ActB.bFree && !MeetNow(A, ActA) && !MeetNow(B, ActB) && !bRain
					&& !((A.Today | B.Today) & TodayCold) && Hour > 6.5f && Hour < 23.f;
				// (neighbours and people of one workplace now and then too)
				if (bTime && Unit(H) < (bFriends || bHouse ? 0.3f + 0.5f * std::min(A.Social, B.Social) : 0.12f))
				{
					const float For = 14.f + 36.f * Unit(Mix(H, 7u));
					A.MeetWith = B.Id;
					B.MeetWith = A.Id;
					// (a pair for town talk's exchanges while they stand: Move ends it)
					if (A.Partner < 0 && B.Partner < 0)
					{
						A.Partner = B.Id;
						B.Partner = A.Id;
					}
					A.MeetFor = B.MeetFor = For;
					A.MeetAgainAt = B.MeetAgainAt = Time + 600.0;
					A.Speed = B.Speed = 0.f;
					A.Pose = B.Pose = EPose::Talk;
					A.Yaw = YawTo(A.Pos, B.Pos);
					B.Yaw = YawTo(B.Pos, A.Pos);
					Meets++;
					Emit(EEvent::StreetMeet, A, B.Id);
				}
				else
				{
					// a greeting in passing (a nod, a wave: the game's manners and town talk)
					A.MeetAgainAt = B.MeetAgainAt = Time + 150.0;
					Nods++;
					Emit(EEvent::StreetNod, A, B.Id);
				}
				break;
			}
		}
	}

	void FWorld::HoldDoor(FPerson& P)
	{
		P.bHeld = true;
		if (P.Place < 0 || bRain || P.Bike >= 0)
		{
			return;
		}
		int Best = -1;
		float BestD = 7.5f;
		for (const FPerson& Q : People)
		{
			if (Q.Id == P.Id || Q.State != EState::Going || Q.Place != P.Place || Q.bIndoors || Q.Bike >= 0 || Q.MeetFor > 0.f || Q.DoorFor > 0.f || Q.HoldFor > 0.f)
			{
				continue;
			}
			const float D = (Q.Pos - P.Pos).Len2D();
			if (D > 1.5f && D < BestD && std::fabs(Q.Pos.Z - P.Pos.Z) < 1.5f)
			{
				BestD = D;
				Best = Q.Id;
			}
		}
		if (Best < 0)
		{
			return;
		}
		// they stand by the door turned to whoever comes, who goes in first (Move: till they are in, or these seconds)
		P.MeetWith = Best;
		P.DoorFor = std::min(8.f, BestD / std::max(0.6f, People[(size_t)Best].WalkSpeed) + 2.f);
		P.Speed = 0.f;
		P.Pose = EPose::Stand;
		P.Yaw = YawTo(P.Pos, People[(size_t)Best].Pos);
		Holds++;
		Emit(EEvent::DoorHold, P, Best);
	}

	std::string FWorld::DescribeToday(int Person) const
	{
		if (Person < 0 || Person >= (int)People.size())
		{
			return std::string();
		}
		const FPerson& P = People[(size_t)Person];
		std::string Out = "today:";
		for (int B = 0; B < NumTodayBits; B++)
		{
			if (P.Today & (1 << B))
			{
				Out += std::string(" ") + TodayNames[B];
			}
		}
		if (!P.Today)
		{
			Out += " -";
		}
		char Buf[160];
		if (P.LateH > 0.f)
		{
			std::snprintf(Buf, sizeof Buf, " (up %.0f min late)", P.LateH * 60.f);
			Out += Buf;
		}
		if (P.PlanAct >= 0)
		{
			std::snprintf(Buf, sizeof Buf, "; plan: %s%s at %02d:%02d, %s at %s", PlanName(P.PlanKind), P.bPlanHost ? " (host)" : "",
				(int)P.PlanHour, (int)(std::fmod(P.PlanHour, 1.f) * 60.f), Activities[(size_t)P.PlanAct].Id.c_str(),
				P.PlanPlace >= 0 ? Places[(size_t)P.PlanPlace].Id.c_str() : "?");
			Out += Buf;
			if (P.PlanWith >= 0)
			{
				Out += " with " + People[(size_t)P.PlanWith].Key;
			}
		}
		return Out;
	}
}
