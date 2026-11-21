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
			D |= (uint8_t)(1u << (X.I() % 7));
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
			if (Tr.Chance < 1.f && LifeHash(Riders[Tr.Rider].Key, 7919u * (uint32_t)(InDay + 1)) >= Tr.Chance)
			{
				continue;
			}
			FLifeRun R;
			R.Trip = T;
			R.At = Tr.At;
			R.HallMinutes = 2.f + 6.f * LifeHash(Riders[Tr.Rider].Key, 31u + (uint32_t)T);
			Today.push_back(R);
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
}
