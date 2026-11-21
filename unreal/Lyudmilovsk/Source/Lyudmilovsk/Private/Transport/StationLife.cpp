#include "StationLife.h"
#include "../People/MiniJson.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace LyudTransport
{
	float LifeHash(const std::string& S, uint32_t Salt)
	{
		// FNV-1a, then a mix: stable everywhere
		uint32_t H = 2166136261u ^ (Salt * 0x9E3779B1u);
		for (unsigned char C : S)
		{
			H ^= C;
			H *= 16777619u;
		}
		H ^= H >> 15;
		H *= 0x2C1B3C6Du;
		H ^= H >> 12;
		return (float)(H & 0xFFFFFF) / (float)0x1000000;
	}

	static uint8_t DaysOf(const LyudJson::FValue& V)
	{
		uint8_t D = 0;
		for (const LyudJson::FValue& X : V.Items)
		{
			// (a negative day in the data must not shift by a negative count)
			D |= (uint8_t)(1u << (((X.I() % 7) + 7) % 7));
		}
		return V.Items.empty() ? (uint8_t)0x7F : D;
	}

	bool FStationLife::Load(const std::string& Text, const FNetwork& N)
	{
		LyudJson::FValue J;
		if (!LyudJson::Parse(Text, J, LastError))
		{
			return false;
		}
		OpenHour = J["open"][0].F(OpenHour);
		CloseHour = J["open"][1].F(CloseHour);
		std::vector<std::string> Regulars;
		for (const LyudJson::FValue& R : J["regulars"].Items)
		{
			Regulars.push_back(R.S());
		}
		for (const LyudJson::FValue& R : J["riders"].Items)
		{
			FLifeRider Rd;
			Rd.Key = R["key"].S();
			Rd.Group = R["group"].S();
			Rd.Mesh = R["mesh"].S();
			Rd.Age = R["age"].I(35);
			Rd.With = R["with"].S();
			Rd.bRegular = std::find(Regulars.begin(), Regulars.end(), Rd.Key) != Regulars.end();
			const int RI = (int)Riders.size();
			bool bAny = false;
			for (const LyudJson::FValue& T : R["trips"].Items)
			{
				FLifeTrip Tr;
				Tr.Rider = RI;
				Tr.From = N.FindStation(T["from"].S());
				Tr.To = N.FindStation(T["to"].S());
				Tr.At = T["at"].F();
				Tr.Days = DaysOf(T["days"]);
				const std::string C = T["if"].S();
				Tr.Cond = C == "warm" ? ELifeCond::Warm : C == "festival_end" ? ELifeCond::FestivalEnd : ELifeCond::None;
				Tr.Chance = T["chance"].F(1.f);
				if (Tr.From >= 0 && Tr.To >= 0 && Tr.From != Tr.To)
				{
					Trips.push_back(Tr);
					bAny = true;
				}
			}
			if (bAny)
			{
				Riders.push_back(Rd);
			}
		}
		for (const LyudJson::FValue& S : J["staff"].Items)
		{
			FLifeStaff St;
			St.Key = S["key"].S();
			St.Role = S["role"].S();
			St.Shift = S["shift"].S();
			St.Mesh = S["mesh"].S();
			St.Station = N.FindStation(S["station"].S());
			St.From = S["hours"][0].F();
			St.To = S["hours"][1].F();
			St.Days = DaysOf(S["days"]);
			if (St.Station >= 0)
			{
				Staff.push_back(St);
			}
		}
		return true;
	}

	void FStationLife::StartDay(int InDay, int InWeekday, bool bWarm)
	{
		Day = InDay;
		Weekday = InWeekday;
		Today.clear();
		for (int T = 0; T < (int)Trips.size(); T++)
		{
			const FLifeTrip& Tr = Trips[T];
			if (!((Tr.Days >> Weekday) & 1) || Tr.Cond == ELifeCond::FestivalEnd || (Tr.Cond == ELifeCond::Warm && !bWarm))
			{
				continue;
			}
			// the chance by the rider and the day (both trips of a beach day go or neither: the rider's draw, not the trip's)
			const FLifeRider& Rd = Riders[Tr.Rider];
			if (Tr.Chance < 1.f && LifeHash(Rd.With.empty() ? Rd.Key : Rd.With, 7919u * (uint32_t)(InDay + 1)) >= Tr.Chance)
			{
				continue;
			}
			FLifeRun R;
			R.Trip = T;
			R.At = Tr.At;
			R.HallMinutes = 2.f + 6.f * LifeHash(Riders[Tr.Rider].Key, 31u + (uint32_t)T);
			Today.push_back(R);
		}
		// a child along with a parent keeps to the parent's minutes in the hall: the same cabin
		for (FLifeRun& K : Today)
		{
			const FLifeTrip& KT = Trips[K.Trip];
			if (Riders[KT.Rider].With.empty())
			{
				continue;
			}
			for (const FLifeRun& P : Today)
			{
				const FLifeTrip& PT = Trips[P.Trip];
				if (Riders[PT.Rider].Key == Riders[KT.Rider].With && PT.From == KT.From && PT.To == KT.To && PT.At == KT.At)
				{
					K.HallMinutes = P.HallMinutes;
				}
			}
		}
	}

	void FStationLife::FestivalEnded(float Hour)
	{
		for (int T = 0; T < (int)Trips.size(); T++)
		{
			const FLifeTrip& Tr = Trips[T];
			if (Tr.Cond != ELifeCond::FestivalEnd || LifeHash(Riders[Tr.Rider].Key, 104729u * (uint32_t)(Day + 1)) >= Tr.Chance)
			{
				continue;
			}
			FLifeRun R;
			R.Trip = T;
			R.At = Hour + Tr.At;
			R.HallMinutes = 1.f + 4.f * LifeHash(Riders[Tr.Rider].Key, 37u + (uint32_t)T);
			Today.push_back(R);
		}
	}

	bool FStationLife::IsOpen(float Hour) const
	{
		const float H = std::fmod(Hour + 24.f, 24.f);
		const float Close = CloseHour - 24.f;       // past midnight
		return H >= OpenHour || H < Close;
	}

	void FStationLife::Step(FNetwork& N, float Hour)
	{
		for (FLifeRun& R : Today)
		{
			const FLifeTrip& Tr = Trips[R.Trip];
			switch (R.Phase)
			{
			case ELifePhase::Pending:
				if (Hour > R.At + R.HallMinutes / 60.f + 0.25f)
				{
					R.Phase = ELifePhase::Skipped;      // the clock jumped past it (a sleep, a skip): missed
					Missed++;
					break;
				}
				if (Hour >= R.At)
				{
					// a trip that would end after closing (a festival running late): they walk instead
					if (Hour > CloseHour - 0.4f || !IsOpen(Hour))
					{
						R.Phase = ELifePhase::Skipped;
						Cut++;
						break;
					}
					R.Phase = ELifePhase::InHall;
				}
				break;
			case ELifePhase::InHall:
				if (Hour >= R.At + R.HallMinutes / 60.f)
				{
					R.NetRider = N.Travel(Base + Tr.Rider, Tr.From, Tr.To);
					R.RideStart = N.Time;
					R.Phase = R.NetRider >= 0 ? ELifePhase::Riding : ELifePhase::Skipped;
					Started += R.NetRider >= 0 ? 1 : 0;
				}
				break;
			case ELifePhase::Riding:
			{
				if (R.NetRider < 0 || R.NetRider >= (int)N.Riders.size() || N.Riders[R.NetRider].Who != Base + Tr.Rider)
				{
					R.Phase = ELifePhase::Done;         // its slot went to another trip (cancelled): gone
					break;
				}
				const FRider& Rd = N.Riders[R.NetRider];
				if (Rd.State == ERiderState::Riding && R.Wait <= 0.0)
				{
					R.Wait = std::max(0.001, N.Time - R.RideStart);
					MaxWait = std::max(MaxWait, R.Wait);
					SumWait += R.Wait;
				}
				if (Rd.State == ERiderState::Done)
				{
					R.Phase = ELifePhase::Out;
					R.OutUntil = N.Time + 60.0 + 60.0 * LifeHash(Riders[Tr.Rider].Key, 41u);
					Finished++;
				}
				else if (Rd.State == ERiderState::Cancelled)
				{
					R.Phase = ELifePhase::Done;
				}
				break;
			}
			case ELifePhase::Out:
				if (N.Time >= R.OutUntil)
				{
					R.Phase = ELifePhase::Done;
				}
				break;
			default:
				break;
			}
		}
	}

	void FStationLife::SetWind(FNetwork& N, float Speed)
	{
		Wind = Speed;
		if (!bWindStopped && Speed >= WindStop)
		{
			bWindStopped = true;
			CalmSince = -1.0;
			WindStops++;
		}
		else if (bWindStopped)
		{
			if (Speed < WindGo)
			{
				if (CalmSince < 0.0)
				{
					CalmSince = N.Time;
				}
				if (N.Time - CalmSince > 300.0)
				{
					bWindStopped = false;
				}
			}
			else
			{
				CalmSince = -1.0;
			}
		}
		N.bWindHold = bWindStopped;
	}

	void FStationLife::InHall(int Station, std::vector<int>& Out) const
	{
		Out.clear();
		for (int I = 0; I < (int)Today.size(); I++)
		{
			const FLifeRun& R = Today[I];
			const FLifeTrip& Tr = Trips[R.Trip];
			if ((R.Phase == ELifePhase::InHall && Tr.From == Station) || (R.Phase == ELifePhase::Out && Tr.To == Station))
			{
				Out.push_back(I);
			}
		}
	}

	int FStationLife::OnShift(int Station, const std::string& Role, int InWeekday, float Hour) const
	{
		const float H = std::fmod(Hour + 24.f, 24.f);
		for (int I = 0; I < (int)Staff.size(); I++)
		{
			const FLifeStaff& S = Staff[I];
			if (S.Station != Station || S.Role != Role)
			{
				continue;
			}
			if (S.To > S.From)
			{
				if (((S.Days >> InWeekday) & 1) && H >= S.From && H < S.To)
				{
					return I;
				}
			}
			else
			{
				// past midnight: the evening part today, the small hours as yesterday's shift
				const int Yesterday = (InWeekday + 6) % 7;
				if ((((S.Days >> InWeekday) & 1) && H >= S.From) || (((S.Days >> Yesterday) & 1) && H < S.To))
				{
					return I;
				}
			}
		}
		return -1;
	}

	std::string FStationLife::Report(const FNetwork& N) const
	{
		int Phases[6] = { 0, 0, 0, 0, 0, 0 };
		for (const FLifeRun& R : Today)
		{
			Phases[(int)R.Phase]++;
		}
		char Buf[512];
		std::snprintf(Buf, sizeof(Buf), "station life: day %d, %d riders, %d trips (%d today: %d pending, %d in a hall, %d riding, "
			"%d walking out, %d done, %d cut); %d staff; started %d, finished %d, wait avg %.0f s max %.0f s; wind %.1f m/s%s",
			Day, (int)Riders.size(), (int)Trips.size(), (int)Today.size(), Phases[0], Phases[1], Phases[2], Phases[3], Phases[4],
			Phases[5], (int)Staff.size(), Started, Finished, Finished ? SumWait / std::max(1, Finished) : 0.0, MaxWait, Wind,
			bWindStopped ? " (STOPPED)" : "");
		(void)N;
		return Buf;
	}

	// --- the hall's moods (docs/transport.md "Hall moods") ---

	float HallSpotScore(int Age, bool bOut, EHallSpot Kind, const FHallMood& M, float Roll)
	{
		const bool bKid = Age < 16, bTeen = Age < 18, bOld = Age >= 60;
		const bool bSeat = Kind == EHallSpot::Seat || Kind == EHallSpot::TableSeat;
		if (bOut && (bSeat || Kind == EHallSpot::Counter || Kind == EHallSpot::Queue))
		{
			return 0.f;                 // walking out: on their way, a look at the window or the diagram at most
		}
		if ((Kind == EHallSpot::Counter || Kind == EHallSpot::Queue) && !M.bBuffet)
		{
			return 0.f;
		}
		float S = 1.f;
		switch (Kind)
		{
		case EHallSpot::Seat:
			S = bOld ? 2.6f : bKid ? 0.6f : 1.f;
			S *= M.IsRush() ? 0.7f : 1.2f;        // the rush stands, ready to go
			break;
		case EHallSpot::TableSeat:
			S = bKid ? 0.4f : 1.f;
			S *= M.IsLunch() ? 2.2f : M.Hour >= 19.f ? 1.4f : 0.9f;
			S *= M.bWeekend ? 1.5f : 1.f;
			S *= M.bBuffet ? 1.f : 0.3f;
			break;
		case EHallSpot::Counter:
		case EHallSpot::Queue:
			S = bKid ? 0.5f : bOld ? 0.8f : 1.2f;
			S *= M.IsRush() ? 2.6f : M.IsLunch() ? 1.8f : 0.8f; // a coffee for the road
			S *= M.bStopped ? 2.f : M.bRain ? 1.4f : 1.f;     // stuck: tea while it blows over
			break;
		case EHallSpot::Rail:
			S = M.IsRush() ? 1.6f : 0.8f;
			S *= bOld ? 0.5f : 1.f;
			break;
		case EHallSpot::Window:
			S = bKid ? 4.f : bTeen ? 2.4f : 0.8f;
			S *= M.bRain ? 1.6f : 1.f;              // the rain on the glass, the sea under it
			S *= M.bWindy || M.bStopped ? 1.4f : 1.f;
			break;
		case EHallSpot::Diagram:
			S = bKid ? 2.f : 0.7f;
			S *= M.bStopped ? 1.6f : 1.f;
			break;
		case EHallSpot::Read:
			S = bOld ? 2.f : bKid ? 0.4f : 1.f;
			S *= M.Hour < 11.f ? 1.6f : 1.f;        // the morning paper
			break;
		case EHallSpot::StandTable:
			S = bKid ? 0.5f : 1.f;
			S *= M.IsRush() || M.IsLunch() ? 1.5f : 1.f;
			S *= M.bBuffet ? 1.f : 0.5f;
			break;
		}
		if (bOut)
		{
			S *= Kind == EHallSpot::Window || Kind == EHallSpot::Diagram ? 1.5f : 0.8f;
		}
		return S * (0.6f + 0.8f * Roll);
	}

	EHallPose HallPose(int Age, EHallSpot Kind, const FHallMood& M, float Roll)
	{
		const bool bKid = Age < 16, bOld = Age >= 60;
		switch (Kind)
		{
		case EHallSpot::Seat:
			if (bKid)
			{
				return Roll < 0.5f ? EHallPose::Look : EHallPose::Sit;
			}
			// the paper in the morning and with the old; a word with the neighbour when the wind holds everyone
			if (M.bStopped && Roll < 0.4f)
			{
				return EHallPose::Talk;
			}
			return Roll < (M.Hour < 11.f ? 0.55f : 0.3f) + (bOld ? 0.2f : 0.f) ? EHallPose::Read : EHallPose::Sit;
		case EHallSpot::TableSeat:
			if (M.IsLunch() || M.Hour >= 18.5f)
			{
				return Roll < 0.7f ? EHallPose::Eat : EHallPose::Talk;
			}
			return Roll < 0.4f ? EHallPose::Eat : Roll < 0.75f ? EHallPose::Read : EHallPose::Talk;
		case EHallSpot::Counter:
			return Roll < 0.6f ? EHallPose::Talk : EHallPose::Stand;   // ordering
		case EHallSpot::Queue:
			return Roll < 0.15f ? EHallPose::Look : EHallPose::Stand;
		case EHallSpot::Window:
			return EHallPose::Look;
		case EHallSpot::Diagram:
			return EHallPose::Look;
		case EHallSpot::Read:
			return Roll < 0.75f ? EHallPose::Read : EHallPose::Look;
		case EHallSpot::StandTable:
			return Roll < 0.5f ? EHallPose::Eat : Roll < 0.8f ? EHallPose::Talk : EHallPose::Stand;
		case EHallSpot::Rail:
		default:
			if (M.IsRush())
			{
				return Roll < 0.15f ? EHallPose::Talk : Roll < 0.35f ? EHallPose::Read : EHallPose::Stand;
			}
			return Roll < 0.3f ? EHallPose::Talk : Roll < 0.45f ? EHallPose::Look : EHallPose::Stand;
		}
	}

	int HallPick(const std::vector<FHallPlace>& Places, const std::string& Key, int Age, bool bOut, const FHallMood& M, uint32_t Salt)
	{
		bool bCounter = false;
		int QueueTaken = 0;     // the queue's spots taken, in order from the counter
		for (const FHallPlace& P : Places)
		{
			bCounter |= P.Kind == EHallSpot::Counter && P.bTaken;
		}
		for (int Q = 1; Q < 8; Q++)
		{
			bool bAny = false;
			for (const FHallPlace& P : Places)
			{
				bAny |= P.Kind == EHallSpot::Queue && P.Queue == Q && P.bTaken;
			}
			if (!bAny)
			{
				break;
			}
			QueueTaken = Q;
		}
		int Best = -1;
		float BestScore = 0.f;
		for (int I = 0; I < (int)Places.size(); I++)
		{
			const FHallPlace& P = Places[I];
			if (!P.bOpen || P.bTaken)
			{
				continue;
			}
			if (P.Kind == EHallSpot::Queue && (!bCounter || P.Queue != QueueTaken + 1))
			{
				continue;           // the queue grows from the counter back
			}
			const float S = HallSpotScore(Age, bOut, P.Kind, M, LifeHash(Key, Salt + 131u * (uint32_t)I));
			if (S > BestScore)
			{
				BestScore = S;
				Best = I;
			}
		}
		return Best;
	}

	int HallFiller(const FHallMood& M, uint32_t Salt)
	{
		const int H = (int)M.Hour;
		if (H < 7 || H >= 23)
		{
			return M.bRain && H >= 6 ? 1 : 0;
		}
		int N = (int)(Salt % 3u);                   // 0..2: in for the buffet
		if (M.bRain)
		{
			N += 2 + (int)((Salt >> 3) % 3u);       // out of the rain: the hall fills
		}
		if (M.bStopped)
		{
			N += 1;
		}
		if (M.bWeekend && M.Hour >= 11.f && M.Hour < 19.f)
		{
			N += 1;
		}
		return N > 6 ? 6 : N;
	}

	EHallPose BaristaPose(int Customers, const FHallMood& M, float Roll)
	{
		if (Customers > 0)
		{
			return Roll < 0.75f ? EHallPose::Work : EHallPose::Talk;
		}
		if (M.Hour >= 21.f || M.Hour < 7.f)
		{
			return Roll < 0.45f ? EHallPose::Read : Roll < 0.7f ? EHallPose::Work : EHallPose::Stand;  // a quiet evening
		}
		return Roll < 0.45f ? EHallPose::Work : Roll < 0.7f ? EHallPose::Look : EHallPose::Stand;     // wiping, stacking
	}

	EHallPose AttendantPose(int Waiting, const FHallMood& M, float Roll)
	{
		if (M.bStopped)
		{
			return Waiting > 0 && Roll < 0.6f ? EHallPose::Talk : EHallPose::Look;  // "it'll blow over"
		}
		if (!M.IsRush() && Waiting < 3 && Roll < 0.4f)
		{
			return EHallPose::Read;                 // the logbook
		}
		return Roll < 0.15f && Waiting > 0 ? EHallPose::Talk : EHallPose::Look;
	}

	const char* HallBark(const FHallMood& M)
	{
		return M.bStopped ? "stopped" : M.bWindy ? "wind" : M.bRain ? "rain" : M.Hour < 9.5f ? "morning" : M.Hour > 20.5f ? "evening" : "any";
	}

	// --- the cabins' life (docs/transport.md "Cabin life") ---

	ECabinAct CabinAct(int Age, int Aboard, const FCabinMood& M, float Roll)
	{
		const bool bKid = Age < 13, bTeen = Age < 18, bOld = Age >= 65;
		// the glass: children nearly always (the rain on it too, the fog a wonder), the young often, the grown-ups for the
		// bay on a clear day, few in the dark (only their own faces in the glass) and fewer in a full cabin
		float Glass = bKid ? 0.9f : bTeen ? 0.45f : bOld ? 0.08f : 0.18f;
		if (!bKid)
		{
			Glass *= M.bFog ? 0.3f : M.bRain ? 0.6f : 1.3f;
			Glass *= M.IsDark() ? 0.35f : 1.f;
			Glass *= Aboard >= 4 ? 0.6f : 1.f;
		}
		if (Roll < Glass)
		{
			return ECabinAct::Window;
		}
		// the paper: in the morning, the old, a dull grey ride
		float Read = bKid ? 0.05f : bOld ? 0.45f : 0.22f;
		Read *= M.Hour < 10.f ? 1.5f : 1.f;
		Read *= M.bFog || M.bRain ? 1.4f : 1.f;
		return Roll < Glass + (1.f - Glass) * Read ? ECabinAct::Read : ECabinAct::Sit;
	}

	void CabinActs(const std::vector<int>& Ages, const std::vector<float>& Rolls, const FCabinMood& M, std::vector<ECabinAct>& Out)
	{
		const int N = (int)Ages.size();
		Out.assign(N, ECabinAct::Sit);
		for (int I = 0; I < N; I++)
		{
			Out[I] = CabinAct(Ages[I], N, M, I < (int)Rolls.size() ? Rolls[I] : 0.5f);
		}
		// two sides of glass: the youngest keep them, the rest sit back down
		int AtGlass = 0;
		for (int Pass = 0; Pass < 2; Pass++)
		{
			for (int I = 0; I < N; I++)
			{
				if (Out[I] != ECabinAct::Window || ((Ages[I] < 13) != (Pass == 0)))
				{
					continue;
				}
				if (++AtGlass > 2)
				{
					Out[I] = ECabinAct::Sit;
				}
			}
		}
	}

	const char* CabinBark(const FCabinMood& M, bool bKidAtGlass, float Roll)
	{
		if (bKidAtGlass && Roll < 0.5f)
		{
			return "kid";
		}
		return M.bFog ? "fog" : M.bRain ? "rain" : M.IsDark() ? "night" : Roll < 0.75f ? "sea" : "any";
	}
}
