#include "FrontDoor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace LyudFrontDoor
{
	using namespace LyudTransport;

	float Hash01(const std::string& S, uint32_t Salt)
	{
		uint32_t H = 2166136261u ^ (Salt * 16777619u);
		for (const char C : S)
		{
			H = (H ^ (uint8_t)C) * 16777619u;
		}
		H ^= H >> 15;
		H *= 2246822519u;
		H ^= H >> 13;
		return (float)(H & 0xFFFFFF) / (float)0x1000000;
	}

	// --- the departures board ---------------------------------------------------------------------------------------

	int DirOn(const FLine& L, int A, int B)
	{
		int IA = -1, IB = -1;
		for (int I = 0; I < (int)L.Stations.size(); I++)
		{
			IA = L.Stations[(size_t)I] == A ? I : IA;
			IB = L.Stations[(size_t)I] == B ? I : IB;
		}
		if (IA < 0 || IB < 0 || IA == IB)
		{
			return -1;
		}
		return IB > IA ? 0 : 1;
	}

	int LegEnd(const FNetwork& N, int L, int St, int To)
	{
		const FLine& Ln = N.Lines[(size_t)L];
		if (std::find(Ln.Stations.begin(), Ln.Stations.end(), To) != Ln.Stations.end())
		{
			return To;
		}
		// a change: the station of this line that one of To's lines stops at too
		for (const FLine& Other : N.Lines)
		{
			if (&Other == &Ln || std::find(Other.Stations.begin(), Other.Stations.end(), To) == Other.Stations.end())
			{
				continue;
			}
			for (const int C : Ln.Stations)
			{
				if (C != St && std::find(Other.Stations.begin(), Other.Stations.end(), C) != Other.Stations.end())
				{
					return C;
				}
			}
		}
		return -1;
	}

	std::string Clock(float Hour)
	{
		int M = (int)std::lround(Hour * 60.f);
		M = ((M % 1440) + 1440) % 1440;
		char Buf[8];
		std::snprintf(Buf, sizeof(Buf), "%02d:%02d", M / 60, M % 60);
		return Buf;
	}

	void Departures(const FNetwork& N, const FStationLife* Life, int St, int L, float Hour, int Max, std::vector<FDeparture>& Out)
	{
		Out.clear();
		if (L < 0 || L >= (int)N.Lines.size() || St < 0 || St >= (int)N.Stations.size())
		{
			return;
		}
		const FLine& Ln = N.Lines[(size_t)L];
		auto Terminal = [&](int Dir) { return Dir == 0 ? Ln.Stations.back() : Ln.Stations.front(); };
		// the cabins: standing here for a way out, or on their way here with a stop here in their list
		for (int CI = 0; CI < (int)N.Cabins.size(); CI++)
		{
			const FCabin& C = N.Cabins[(size_t)CI];
			if (C.Line != L || C.State == ECabinState::Depot)
			{
				continue;
			}
			int Here = -1;
			for (const int T : C.Targets)
			{
				Here = Ln.Stops[(size_t)T].Station == St ? T : Here;
			}
			const bool bStanding = C.AtStop >= 0 && Ln.Stops[(size_t)C.AtStop].Station == St && C.State != ECabinState::Moving;
			if (!bStanding && Here < 0)
			{
				continue;
			}
			const int Dest = N.Destination(CI);
			int To = Dest >= 0 && Dest < (int)Ln.Stops.size() ? Ln.Stops[(size_t)Dest].Station : -1;
			FDeparture D;
			D.Cabin = CI;
			D.People = (int)C.Riders.size() + C.Reserved;
			if (bStanding)
			{
				// an idle cabin with nobody to take stands here unasked: it is the next one, whenever somebody calls it
				if (To == St || To < 0)
				{
					const int Dir = Ln.Stops[(size_t)C.AtStop].bEnd ? (St == Ln.Stations.front() ? 0 : 1) : Ln.Stops[(size_t)C.AtStop].Dir;
					To = Terminal(Dir);
				}
				D.Kind = EDepKind::Boarding;
				D.Hour = Hour + (float)(std::max(0.0, C.DwellUntil - N.Time) / 3600.0);
			}
			else
			{
				D.Kind = EDepKind::Arriving;
				const float Metres = Ln.Ahead(C.S, Ln.Stops[(size_t)Here].S);
				D.Hour = Hour + Metres / std::max(1.f, (C.Riders.empty() ? N.P.SpeedEmpty : N.P.Speed) * 0.8f) / 3600.f;
				if (To == St || To < 0)
				{
					To = Terminal(Ln.Stops[(size_t)Here].bEnd ? (St == Ln.Stations.front() ? 0 : 1) : Ln.Stops[(size_t)Here].Dir);
				}
			}
			D.To = To;
			if (D.To != St)
			{
				Out.push_back(D);
			}
		}
		// the station life: riders in the hall now or coming into it within the hour will call one when they go up
		if (Life)
		{
			bool bCalled = false;
			const float Eta0 = std::max(0.f, N.Eta(St, L, -1, bCalled));
			const float Wait = std::min(Eta0, 600.f) / 3600.f;
			for (const FLifeRun& R : Life->Today)
			{
				if (R.Phase != ELifePhase::Pending && R.Phase != ELifePhase::InHall)
				{
					continue;
				}
				const FLifeTrip& T = Life->Trips[(size_t)R.Trip];
				if (T.From != St)
				{
					continue;
				}
				const float Ask = R.At + R.HallMinutes / 60.f;
				if (Ask < Hour - 0.02f || Ask > Hour + 1.5f)
				{
					continue;
				}
				const int To = LegEnd(N, L, St, T.To);
				if (To < 0 || DirOn(Ln, St, To) < 0)
				{
					continue;
				}
				// riders going up within a minute and a half of each other share a cabin (four at most)
				const int Dir = DirOn(Ln, St, To);
				FDeparture* Same = nullptr;
				for (FDeparture& D : Out)
				{
					if (D.Kind == EDepKind::Planned && D.People < N.P.Capacity && DirOn(Ln, St, D.To) == Dir && std::fabs(D.Hour - (Ask + Wait)) < 0.025f)
					{
						Same = &D;
					}
				}
				if (Same)
				{
					Same->People++;
					// the farther of the two
					if (Dir == 0 ? DirOn(Ln, Same->To, To) == 0 : DirOn(Ln, Same->To, To) == 1)
					{
						Same->To = To;
					}
					continue;
				}
				FDeparture D;
				D.Kind = EDepKind::Planned;
				D.Hour = std::max(Hour, Ask + Wait);
				D.To = To;
				D.People = 1;
				Out.push_back(D);
			}
		}
		std::stable_sort(Out.begin(), Out.end(), [](const FDeparture& A, const FDeparture& B) { return A.Hour < B.Hour; });
		if ((int)Out.size() > Max)
		{
			Out.resize((size_t)std::max(0, Max));
		}
		if (Out.empty() && Max > 0)
		{
			bool bCalled = false;
			FDeparture D;
			D.Kind = EDepKind::OnCall;
			D.Hour = Hour + std::max(0.f, N.Eta(St, L, -1, bCalled)) / 3600.f;
			D.To = St == Ln.Stations.back() ? Ln.Stations.front() : Ln.Stations.back();
			Out.push_back(D);
		}
	}

	// --- the counter's line ----------------------------------------------------------------------------------------

	int JoinSpot(const std::vector<int>& Line)
	{
		for (int I = 0; I < (int)Line.size(); I++)
		{
			if (Line[(size_t)I] == -1)
			{
				return I;
			}
		}
		return -1;
	}

	int StepUp(const std::vector<int>& Line, int& OutTo)
	{
		OutTo = -1;
		for (int I = 1; I < (int)Line.size(); I++)
		{
			if (Line[(size_t)I] >= 0 && Line[(size_t)I - 1] == -1)
			{
				OutTo = I - 1;
				return I;
			}
		}
		return -1;
	}

	// --- the usual -------------------------------------------------------------------------------------------------

	void FUsual::Add(const std::string& Counter, const std::string& Item, int Count)
	{
		if (!Counter.empty() && !Item.empty() && Count > 0)
		{
			Bought[Counter + "|" + Item] += Count;
		}
	}

	std::string FUsual::Of(const std::string& Counter, int* OutTimes) const
	{
		const std::string Pre = Counter + "|";
		std::string Best;
		int BestN = 0;
		for (auto It = Bought.lower_bound(Pre); It != Bought.end() && It->first.compare(0, Pre.size(), Pre) == 0; ++It)
		{
			if (It->second > BestN)
			{
				BestN = It->second;
				Best = It->first.substr(Pre.size());
			}
		}
		if (OutTimes)
		{
			*OutTimes = BestN;
		}
		return BestN >= Times ? Best : std::string();
	}

	int FUsual::Visits(const std::string& Counter) const
	{
		const std::string Pre = Counter + "|";
		int N = 0;
		for (auto It = Bought.lower_bound(Pre); It != Bought.end() && It->first.compare(0, Pre.size(), Pre) == 0; ++It)
		{
			N += It->second;
		}
		return N;
	}

	std::string FUsual::Save() const
	{
		std::string Out;
		for (const auto& KV : Bought)
		{
			Out += KV.first + "=" + std::to_string(KV.second) + ";";
		}
		return Out;
	}

	void FUsual::Load(const std::string& S)
	{
		Bought.clear();
		std::stringstream In(S);
		std::string Part;
		while (std::getline(In, Part, ';'))
		{
			const size_t Eq = Part.rfind('=');
			if (Eq != std::string::npos && Eq > 0)
			{
				Bought[Part.substr(0, Eq)] = std::atoi(Part.c_str() + Eq + 1);
			}
		}
	}

	// --- the lost and found ------------------------------------------------------------------------------------------

	const char* const LostKinds[] = { "lost_umbrella", "lost_mitten", "lost_scarf", "lost_glasses", "lost_keys", "lost_bear" };
	const char* const Gifts[] = { "chocolate", "candy", "cookies", "pie", "postcard", "seeds" };

	void FLostFound::StartDay(int InDay)
	{
		if (InDay == Day)
		{
			return;
		}
		// days skipped are lived through one by one (a long sleep): the old things leave, new ones come
		const int From = Day < 0 ? InDay : std::max(Day + 1, InDay - KeepDays);
		Day = InDay;
		for (int D = From; D <= InDay; D++)
		{
			Shelf.erase(std::remove_if(Shelf.begin(), Shelf.end(), [&](const FLost& L) { return D - L.Day >= KeepDays; }), Shelf.end());
			if (Owners.empty())
			{
				continue;
			}
			const std::string Seed = "lost" + std::to_string(D);
			const int Finds = (int)(Hash01(Seed, 1u) * 2.6f);     // 0, 1 or 2 (two less often)
			for (int K = 0; K < Finds && (int)Shelf.size() < MaxShelf; K++)
			{
				FLost L;
				L.Day = D;
				L.Owner = (int)(Hash01(Seed, 10u + (uint32_t)K) * (float)Owners.size()) % (int)Owners.size();
				L.Kind = (int)(Hash01(Seed, 20u + (uint32_t)K) * NumLostKinds) % NumLostKinds;
				// one thing of a person's at a time (on the shelf or in his pocket)
				bool bDup = false;
				for (const FLost& O : Shelf)
				{
					bDup |= O.Owner == L.Owner;
				}
				for (const FLost& O : Carried)
				{
					bDup |= O.Owner == L.Owner;
				}
				if (!bDup)
				{
					Shelf.push_back(L);
				}
			}
		}
	}

	bool FLostFound::Take(int Index, FLost& Out)
	{
		if (Index < 0 || Index >= (int)Shelf.size())
		{
			return false;
		}
		Out = Shelf[(size_t)Index];
		Shelf.erase(Shelf.begin() + Index);
		Carried.push_back(Out);
		return true;
	}

	int FLostFound::OwnerOf(const std::string& Key) const
	{
		for (int I = 0; I < (int)Owners.size(); I++)
		{
			if (Owners[(size_t)I] == Key)
			{
				return I;
			}
		}
		return -1;
	}

	int FLostFound::CarriedFor(const std::string& Key) const
	{
		const int O = OwnerOf(Key);
		for (int I = 0; I < (int)Carried.size() && O >= 0; I++)
		{
			if (Carried[(size_t)I].Owner == O)
			{
				return I;
			}
		}
		return -1;
	}

	std::string FLostFound::GiveBack(int Index)
	{
		if (Index < 0 || Index >= (int)Carried.size())
		{
			return std::string();
		}
		const FLost L = Carried[(size_t)Index];
		Carried.erase(Carried.begin() + Index);
		Returned++;
		const std::string& Key = L.Owner >= 0 && L.Owner < (int)Owners.size() ? Owners[(size_t)L.Owner] : std::string("?");
		return Gifts[(int)(Hash01(Key, 30u + (uint32_t)L.Day) * NumGifts) % NumGifts];
	}

	std::string FLostFound::Save() const
	{
		// day;returned;S kind owner day,...;C kind owner day,... (owners by key, so a regenerated town keeps what it can)
		std::string Out = std::to_string(Day) + ";" + std::to_string(Returned) + ";";
		auto Part = [&](const std::vector<FLost>& V)
		{
			for (const FLost& L : V)
			{
				if (L.Owner >= 0 && L.Owner < (int)Owners.size())
				{
					Out += std::to_string(L.Kind) + " " + Owners[(size_t)L.Owner] + " " + std::to_string(L.Day) + ",";
				}
			}
			Out += ";";
		};
		Part(Shelf);
		Part(Carried);
		return Out;
	}

	void FLostFound::Load(const std::string& S)
	{
		Shelf.clear();
		Carried.clear();
		std::vector<std::string> Parts;
		std::stringstream In(S);
		std::string P;
		while (std::getline(In, P, ';'))
		{
			Parts.push_back(P);
		}
		if (Parts.size() < 2)
		{
			return;
		}
		Day = std::atoi(Parts[0].c_str());
		Returned = std::atoi(Parts[1].c_str());
		for (size_t K = 2; K < Parts.size() && K < 4; K++)
		{
			std::stringstream Items(Parts[K]);
			std::string Item;
			while (std::getline(Items, Item, ','))
			{
				std::stringstream F(Item);
				FLost L;
				std::string Key;
				if (F >> L.Kind >> Key >> L.Day)
				{
					L.Owner = OwnerOf(Key);
					if (L.Owner >= 0 && L.Kind >= 0 && L.Kind < NumLostKinds)
					{
						(K == 2 ? Shelf : Carried).push_back(L);
					}
				}
			}
		}
	}

	void OwnersAt(const FStationLife& Life, int St, std::vector<std::string>& OutKeys, std::vector<float>& OutHours)
	{
		OutKeys.clear();
		OutHours.clear();
		for (const FLifeTrip& T : Life.Trips)
		{
			if ((T.From != St && T.To != St) || T.Rider < 0 || T.Rider >= (int)Life.Riders.size() || T.Cond != ELifeCond::None)
			{
				continue;
			}
			const std::string& Key = Life.Riders[(size_t)T.Rider].Key;
			const auto It = std::find(OutKeys.begin(), OutKeys.end(), Key);
			if (It == OutKeys.end())
			{
				OutKeys.push_back(Key);
				OutHours.push_back(T.At);
			}
			else
			{
				float& H = OutHours[(size_t)(It - OutKeys.begin())];
				H = std::min(H, T.At);
			}
		}
	}

	// --- the travel card -------------------------------------------------------------------------------------------

	bool FTravelCard::Has(const std::string& Id) const
	{
		return std::find(Stamps.begin(), Stamps.end(), Id) != Stamps.end();
	}

	bool FTravelCard::Punch(const std::string& Id)
	{
		if (!bHave || Id.empty() || Has(Id))
		{
			return false;
		}
		Stamps.push_back(Id);
		return true;
	}

	std::string FTravelCard::Save() const
	{
		std::string Out = bHave ? "1" : "0";
		for (const std::string& S : Stamps)
		{
			Out += "," + S;
		}
		return Out;
	}

	void FTravelCard::Load(const std::string& S)
	{
		Stamps.clear();
		std::stringstream In(S);
		std::string P;
		bool bFirst = true;
		while (std::getline(In, P, ','))
		{
			if (bFirst)
			{
				bHave = P == "1";
				bFirst = false;
			}
			else if (!P.empty() && !Has(P))
			{
				Stamps.push_back(P);
			}
		}
	}
}
