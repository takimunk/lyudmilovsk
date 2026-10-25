// The cable-car metro's working (CablewaySim.h). Plain C++; tools/transportsim runs it outside the engine.
#include "CablewaySim.h"
#include "../People/MiniJson.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace LyudTransport
{
	const char* CabinStateName(ECabinState S)
	{
		static const char* Names[] = { "depot", "moving", "dwelling", "idle" };
		return Names[(int)S];
	}

	const char* EventName(EEvent E)
	{
		static const char* Names[] = { "Launch", "Arrive", "Depart", "Board", "Alight", "Done", "Park" };
		return (int)E < (int)EEvent::Num ? Names[(int)E] : "?";
	}

	int FRider::Station() const
	{
		if (Legs.empty())
		{
			return -1;
		}
		const FLeg& L = Legs[std::min(LegI, (int)Legs.size() - 1)];
		return State == ERiderState::Done ? L.To : L.From;
	}

	// --- line geometry ---------------------------------------------------------------------------------------

	V3 FLine::At(float S, V3* OutDir) const
	{
		if (Loop.size() < 2 || Length <= 0.f)
		{
			if (OutDir)
			{
				*OutDir = V3(1, 0, 0);
			}
			return Loop.empty() ? V3() : Loop[0];
		}
		S = std::fmod(S, Length);
		if (S < 0.f)
		{
			S += Length;
		}
		const auto It = std::upper_bound(LoopS.begin(), LoopS.end(), S);
		int I = std::max(0, (int)(It - LoopS.begin()) - 1);
		I = std::min(I, (int)Loop.size() - 2);
		const float Seg = std::max(1e-4f, LoopS[I + 1] - LoopS[I]);
		const float T = std::min(1.f, std::max(0.f, (S - LoopS[I]) / Seg));
		const V3 A = Loop[I], B = Loop[I + 1];
		if (OutDir)
		{
			const V3 D = B - A;
			const float L = std::sqrt(D.X * D.X + D.Y * D.Y + D.Z * D.Z);
			*OutDir = L > 1e-5f ? D * (1.f / L) : V3(1, 0, 0);
		}
		return A + (B - A) * T;
	}

	float FLine::Ahead(float From, float To) const
	{
		float D = std::fmod(To - From, Length);
		if (D < 0.f)
		{
			D += Length;
		}
		return D;
	}

	int FLine::StopOf(int Station, int Dir) const
	{
		for (int I = 0; I < (int)Stops.size(); I++)
		{
			if (Stops[I].Station == Station && (Dir < 0 || Stops[I].Dir == Dir || Stops[I].bEnd))
			{
				return I;
			}
		}
		return -1;
	}

	// --- loading ---------------------------------------------------------------------------------------------

	bool FNetwork::Load(const std::string& Text)
	{
		LyudJson::FValue J;
		LyudJson::FParser Parser(Text);
		if (!Parser.Parse(J, LastError))
		{
			return false;
		}
		const LyudJson::FValue& D = J["defaults"];
		P.Speed = D["speed"].F(P.Speed);
		P.SpeedEmpty = D["speed_empty"].F(P.SpeedEmpty);
		P.SpeedStation = D["speed_station"].F(P.SpeedStation);
		P.Dwell = D["dwell"].F(P.Dwell);
		P.IdleReturn = D["idle_return"].F(P.IdleReturn);
		P.Headway = D["headway"].F(P.Headway);
		P.PlatformLen = D["platform_len"].F(P.PlatformLen);
		P.Capacity = D["capacity"].I(P.Capacity);
		const LyudJson::FValue& Fr = J["frame"]["portal_face_y"];
		bHasFrame = Fr.Type == LyudJson::FValue::Number;
		PortalFaceY = Fr.F();

		Stations.clear();
		Lines.clear();
		Cabins.clear();
		Riders.clear();
		Events.clear();
		const LyudJson::FValue& JS = J["stations"];
		for (size_t I = 0; I < JS.Size(); I++)
		{
			const LyudJson::FValue& S = JS[I];
			FStation St;
			St.Id = S["id"].S();
			St.Name = S["name"].S();
			St.Style = S["style"].S("saucer");
			St.X = S["at"][0].F();
			St.Y = S["at"][1].F();
			St.Ground = S["ground"].F();
			St.Deck = S["deck"].F();
			St.Lift = S["lift"].F();
			St.bOnGround = S["on_ground"].Type == LyudJson::FValue::Bool && S["on_ground"].B;
			St.CoreX = S["core"][0].F(St.X);
			St.CoreY = S["core"][1].F(St.Y);
			St.CoreGround = S["core_ground"].F(St.Ground);
			const LyudJson::FValue& Sc = S["schema"];
			if (Sc.Type == LyudJson::FValue::Array && Sc.Size() == 2)
			{
				St.SchemaX = Sc[0].I();
				St.SchemaY = Sc[1].I();
				St.bHasSchema = true;
			}
			Stations.push_back(St);
		}
		const LyudJson::FValue& JL = J["lines"];
		for (size_t I = 0; I < JL.Size(); I++)
		{
			const LyudJson::FValue& L = JL[I];
			FLine Ln;
			Ln.Id = L["id"].S();
			Ln.Name = L["name"].S();
			Ln.Color = L["color"].S("#E8962E");
			Ln.Number = L["number"].I();
			for (size_t K = 0; K < L["stops"].Size(); K++)
			{
				Ln.Stations.push_back(FindStation(L["stops"][K].S()));
			}
			Ln.Depot = FindStation(L["depot"].S());
			const LyudJson::FValue& Lp = L["loop"];
			for (size_t K = 0; K < Lp.Size(); K++)
			{
				Ln.Loop.push_back(V3(Lp[K][0].F(), Lp[K][1].F(), Lp[K][2].F()));
				Ln.LoopS.push_back(Lp[K][3].F());
			}
			Ln.Length = L["loop_length"].F(Ln.LoopS.empty() ? 0.f : Ln.LoopS.back());
			const LyudJson::FValue& Ss = L["stop_s"];
			for (size_t K = 0; K < Ss.Size(); K++)
			{
				FStop Sp;
				Sp.Station = FindStation(Ss[K]["station"].S());
				Sp.Dir = Ss[K]["dir"].I();
				Sp.S = Ss[K]["s"].F();
				Sp.bEnd = Ss[K]["end"].Type == LyudJson::FValue::Bool && Ss[K]["end"].B;
				Ln.Stops.push_back(Sp);
			}
			std::sort(Ln.Stops.begin(), Ln.Stops.end(), [](const FStop& A, const FStop& B) { return A.S < B.S; });
			const LyudJson::FValue& Py = L["pylons"];
			for (size_t K = 0; K < Py.Size(); K++)
			{
				FPylon Pl;
				Pl.X = Py[K]["x"].F();
				Pl.Y = Py[K]["y"].F();
				Pl.Ground = Py[K]["ground"].F();
				Pl.Top = Py[K]["top"].F();
				Pl.Height = Py[K]["height"].F();
				Pl.Yaw = Py[K]["yaw"].F();
				Pl.Turn = Py[K]["turn"].F();
				Pl.bCorner = Py[K]["kind"].S() == "corner";
				Ln.Pylons.push_back(Pl);
			}
			Ln.DepotStop = Ln.StopOf(Ln.Depot, -1);
			for (int K = 0; K < (int)Ln.Stops.size(); K++)
			{
				if (Ln.Stops[K].Station == Ln.Depot && Ln.Stops[K].bEnd)
				{
					Ln.DepotStop = K;
				}
			}
			if (Ln.Stations.size() < 2 || Ln.Loop.size() < 2 || Ln.DepotStop < 0)
			{
				LastError = "line " + Ln.Id + ": needs two stations, a loop and a depot";
				return false;
			}
			Lines.push_back(Ln);
		}
		for (int I = 0; I < (int)Stations.size(); I++)
		{
			FStation& St = Stations[I];
			St.AxisYaw.assign(Lines.size(), 0.f);
			St.Ends.assign(Lines.size(), 0);
			for (int L = 0; L < (int)Lines.size(); L++)
			{
				if (!Lines[L].Stations.empty() && Lines[L].Stations.front() == I)
				{
					St.Ends[L] = 1;
				}
				else if (!Lines[L].Stations.empty() && Lines[L].Stations.back() == I)
				{
					St.Ends[L] = -1;
				}
			}
		}
		for (size_t I = 0; I < JS.Size(); I++)
		{
			const LyudJson::FValue& Lv = JS[I]["levels"];
			for (size_t K = 0; K < Lv.Size(); K++)
			{
				FLevel Le;
				Le.Line = FindLine(Lv[K]["line"].S());
				Le.Z = Lv[K]["z"].F();
				Stations[I].Levels.push_back(Le);
			}
			const LyudJson::FValue& Ax = JS[I]["axes"];
			for (const auto& F : Ax.Fields)
			{
				const int L = FindLine(F.first);
				if (L >= 0)
				{
					Stations[I].AxisYaw[L] = F.second.F();
				}
			}
		}
		const LyudJson::FValue& JC = J["cabins"];
		for (size_t I = 0; I < JC.Size(); I++)
		{
			FCabin C;
			C.Id = JC[I]["id"].S();
			C.Name = JC[I]["name"].S();
			C.Number = JC[I]["number"].I();
			C.Line = FindLine(JC[I]["line"].S());
			if (C.Line < 0)
			{
				LastError = "cabin " + C.Id + ": no line";
				return false;
			}
			Cabins.push_back(C);
		}
		const LyudJson::FValue& B = J["club_board"];
		bHasClubBoard = B.Type == LyudJson::FValue::Object;
		if (bHasClubBoard)
		{
			ClubBoard = V3(B["at"][0].F(), B["at"][1].F(), B["z"].F());
			ClubBoardYaw = B["yaw"].F();
		}
		Reset();
		return true;
	}

	void FNetwork::Reset()
	{
		Time = 0.0;
		Riders.clear();
		Events.clear();
		std::vector<int> Slot(Lines.size(), 0);
		std::vector<bool> Kept(Lines.size(), false);
		for (FCabin& C : Cabins)
		{
			const FLine& L = Lines[C.Line];
			C.Targets.clear();
			C.Riders.clear();
			C.Reserved = 0;
			C.Speed = 0.f;
			C.Door = 0.f;
			C.bToDepot = C.bHold = C.bPlayerAboard = false;
			C.Odometer = C.EmptyMetres = 0.f;
			if (!Kept[C.Line])
			{
				// one cabin waits at the line's first station (its far end from the depot)
				Kept[C.Line] = true;
				const int K = L.StopOf(L.Stations.front(), -1);
				C.State = ECabinState::Idle;
				C.AtStop = K;
				C.S = L.Stops[K].S;
				C.IdleSince = 0.0;
			}
			else
			{
				C.State = ECabinState::Depot;
				C.AtStop = L.DepotStop;
				C.S = L.Stops[L.DepotStop].S;
				C.DepotSlot = Slot[C.Line]++;
			}
		}
	}

	int FNetwork::FindStation(const std::string& Id) const
	{
		for (int I = 0; I < (int)Stations.size(); I++)
		{
			if (Stations[I].Id == Id)
			{
				return I;
			}
		}
		return -1;
	}

	int FNetwork::FindLine(const std::string& Id) const
	{
		for (int I = 0; I < (int)Lines.size(); I++)
		{
			if (Lines[I].Id == Id)
			{
				return I;
			}
		}
		return -1;
	}

	int FNetwork::FindCabin(const std::string& Id) const
	{
		for (int I = 0; I < (int)Cabins.size(); I++)
		{
			if (Cabins[I].Id == Id)
			{
				return I;
			}
		}
		return -1;
	}

	// --- trips -----------------------------------------------------------------------------------------------

	static bool OnLine(const FLine& L, int St)
	{
		return std::find(L.Stations.begin(), L.Stations.end(), St) != L.Stations.end();
	}

	bool FNetwork::Plan(int From, int To, std::vector<FLeg>& Out) const
	{
		Out.clear();
		if (From < 0 || To < 0 || From == To)
		{
			return false;
		}
		auto Leg = [this](int L, int A, int B)
		{
			FLeg G;
			G.Line = L;
			G.From = A;
			G.To = B;
			const FLine& Ln = Lines[L];
			float Best = 1e30f;
			for (int I = 0; I < (int)Ln.Stops.size(); I++)
			{
				if (Ln.Stops[I].Station != A)
				{
					continue;
				}
				for (int K = 0; K < (int)Ln.Stops.size(); K++)
				{
					if (Ln.Stops[K].Station != B)
					{
						continue;
					}
					const float D = Ln.Ahead(Ln.Stops[I].S, Ln.Stops[K].S);
					if (D < Best)
					{
						Best = D;
						G.BoardStop = I;
						G.AlightStop = K;
					}
				}
			}
			return G;
		};
		// one line
		for (int L = 0; L < (int)Lines.size(); L++)
		{
			if (OnLine(Lines[L], From) && OnLine(Lines[L], To))
			{
				Out.push_back(Leg(L, From, To));
				return true;
			}
		}
		// two lines, changing at a shared station (the nearest change wins)
		float Best = 1e30f;
		std::vector<FLeg> Try;
		for (int A = 0; A < (int)Lines.size(); A++)
		{
			if (!OnLine(Lines[A], From))
			{
				continue;
			}
			for (int B = 0; B < (int)Lines.size(); B++)
			{
				if (B == A || !OnLine(Lines[B], To))
				{
					continue;
				}
				for (int X : Lines[A].Stations)
				{
					if (X == From || X == To || !OnLine(Lines[B], X))
					{
						continue;
					}
					Try = { Leg(A, From, X), Leg(B, X, To) };
					const float D = Lines[A].Ahead(Lines[A].Stops[Try[0].BoardStop].S, Lines[A].Stops[Try[0].AlightStop].S)
						+ Lines[B].Ahead(Lines[B].Stops[Try[1].BoardStop].S, Lines[B].Stops[Try[1].AlightStop].S);
					if (D < Best)
					{
						Best = D;
						Out = Try;
					}
				}
			}
		}
		return !Out.empty();
	}

	int FNetwork::FindRider(int Who) const
	{
		for (int I = 0; I < (int)Riders.size(); I++)
		{
			if (Riders[I].Who == Who && (Riders[I].State == ERiderState::Waiting || Riders[I].State == ERiderState::Riding))
			{
				return I;
			}
		}
		return -1;
	}

	void FNetwork::Cancel(int RI)
	{
		if (RI < 0 || RI >= (int)Riders.size())
		{
			return;
		}
		FRider& R = Riders[RI];
		if (R.State == ERiderState::Waiting && R.Cabin >= 0)
		{
			FCabin& C = Cabins[R.Cabin];
			C.Reserved = std::max(0, C.Reserved - 1);
		}
		if (R.State == ERiderState::Riding && R.Cabin >= 0)
		{
			FCabin& C = Cabins[R.Cabin];
			C.Riders.erase(std::remove(C.Riders.begin(), C.Riders.end(), RI), C.Riders.end());
		}
		R.State = ERiderState::Cancelled;
		R.Cabin = -1;
	}

	int FNetwork::Travel(int Who, int From, int To)
	{
		const int Old = FindRider(Who);
		if (Old >= 0)
		{
			if (Riders[Old].State == ERiderState::Riding)
			{
				return -1;      // already aboard: not from a platform
			}
			Cancel(Old);
		}
		FRider R;
		R.Who = Who;
		if (!Plan(From, To, R.Legs))
		{
			return -1;
		}
		R.Asked = Time;
		Riders.push_back(R);
		const int RI = (int)Riders.size() - 1;
		StartLeg(Riders[RI]);
		Assign(Riders[RI], RI);
		return RI;
	}

	void FNetwork::StartLeg(FRider& R)
	{
		R.State = ERiderState::Waiting;
		R.Cabin = -1;
		R.Seat = -1;
		R.Since = Time;
	}

	float FNetwork::CostTo(const FCabin& C, int Line, int Stop) const
	{
		const FLine& L = Lines[Line];
		const float StopS = L.Stops[Stop].S;
		if (C.State == ECabinState::Depot)
		{
			return L.Ahead(L.Stops[L.DepotStop].S, StopS) / P.SpeedEmpty + 20.f;
		}
		if (C.AtStop == Stop && C.State != ECabinState::Moving)
		{
			return 0.f;
		}
		const float D = L.Ahead(C.S, StopS);
		int Between = 0;
		for (int T : C.Targets)
		{
			if (L.Ahead(C.S, L.Stops[T].S) < D)
			{
				Between++;
			}
		}
		const float V = (C.Riders.empty() && C.Reserved == 0 ? P.SpeedEmpty : P.Speed) * 0.85f;
		return D / V + Between * (P.Dwell + 8.f);
	}

	void FNetwork::Assign(FRider& R, int RI)
	{
		if (R.State != ERiderState::Waiting || R.Cabin >= 0)
		{
			return;
		}
		const FLeg& G = R.Legs[R.LegI];
		int Best = -1;
		float BestCost = 1e30f;
		for (int CI = 0; CI < (int)Cabins.size(); CI++)
		{
			const FCabin& C = Cabins[CI];
			if (C.Line != G.Line || (int)C.Riders.size() + C.Reserved >= P.Capacity)
			{
				continue;
			}
			// a cabin out of the depot costs a launch, so one already out and on its way is preferred a little
			const float Cost = CostTo(C, G.Line, G.BoardStop) + (C.State == ECabinState::Depot ? 10.f : 0.f);
			if (Cost < BestCost)
			{
				BestCost = Cost;
				Best = CI;
			}
		}
		if (Best < 0)
		{
			return;     // all full: tried again next step
		}
		FCabin& C = Cabins[Best];
		R.Cabin = Best;
		C.Reserved++;
		if (std::find(C.Targets.begin(), C.Targets.end(), G.BoardStop) == C.Targets.end())
		{
			C.Targets.push_back(G.BoardStop);
		}
		C.bToDepot = false;
		if (C.State == ECabinState::Depot)
		{
			// leaves the depot in Step, once the rope there is clear
		}
		else if (C.State == ECabinState::Idle)
		{
			if (C.AtStop == G.BoardStop)
			{
				ArriveAt(Best, G.BoardStop);
			}
			else
			{
				C.State = ECabinState::Moving;
				C.AtStop = -1;
			}
		}
		else if (C.State == ECabinState::Dwelling && C.AtStop == G.BoardStop)
		{
			ArriveAt(Best, G.BoardStop);    // boards now
		}
	}

	int FNetwork::PlayerBoard(int CI, int ToStation)
	{
		if (CI < 0 || CI >= (int)Cabins.size())
		{
			return -1;
		}
		FCabin& C = Cabins[CI];
		const FLine& L = Lines[C.Line];
		if (C.AtStop < 0)
		{
			return -1;
		}
		int RI = FindRider(PlayerId);
		if (RI >= 0 && Riders[RI].State == ERiderState::Riding && Riders[RI].Cabin == CI)
		{
			return RI;
		}
		// their trip from this platform, if it goes on this line from here
		if (RI >= 0 && Riders[RI].State == ERiderState::Waiting && Riders[RI].Legs[Riders[RI].LegI].Line == C.Line
			&& L.Stops[Riders[RI].Legs[Riders[RI].LegI].BoardStop].Station == L.Stops[C.AtStop].Station)
		{
			FRider& R = Riders[RI];
			if (R.Cabin >= 0 && R.Cabin != CI)
			{
				Cabins[R.Cabin].Reserved = std::max(0, Cabins[R.Cabin].Reserved - 1);
			}
			else if (R.Cabin == CI)
			{
				C.Reserved = std::max(0, C.Reserved - 1);
			}
			// boarded where this cabin stands: it may go the other way round, so the stop to get off is found again
			FLeg& G = R.Legs[R.LegI];
			G.BoardStop = C.AtStop;
			float Best = 1e30f;
			for (int K = 0; K < (int)L.Stops.size(); K++)
			{
				if (L.Stops[K].Station == G.To && L.Ahead(L.Stops[C.AtStop].S, L.Stops[K].S) < Best)
				{
					Best = L.Ahead(L.Stops[C.AtStop].S, L.Stops[K].S);
					G.AlightStop = K;
				}
			}
			R.State = ERiderState::Riding;
			R.Cabin = CI;
			R.Seat = (int)C.Riders.size();
			C.Riders.push_back(RI);
			if (std::find(C.Targets.begin(), C.Targets.end(), G.AlightStop) == C.Targets.end())
			{
				C.Targets.push_back(G.AlightStop);
			}
			Emit(EEvent::Board, CI, C.AtStop, RI);
			return RI;
		}
		if (RI >= 0)
		{
			Cancel(RI);
		}
		const int From = L.Stops[C.AtStop].Station;
		if (ToStation < 0 || !OnLine(L, ToStation) || ToStation == From)
		{
			// the next station along the loop
			float Best = 1e30f;
			for (const FStop& Sp : L.Stops)
			{
				const float D = L.Ahead(L.Stops[C.AtStop].S, Sp.S);
				if (Sp.Station != From && D > 0.f && D < Best)
				{
					Best = D;
					ToStation = Sp.Station;
				}
			}
		}
		FRider R;
		R.Who = PlayerId;
		FLeg G;
		G.Line = C.Line;
		G.From = From;
		G.To = ToStation;
		G.BoardStop = C.AtStop;
		float Best = 1e30f;
		for (int K = 0; K < (int)L.Stops.size(); K++)
		{
			if (L.Stops[K].Station == ToStation && L.Ahead(L.Stops[C.AtStop].S, L.Stops[K].S) < Best)
			{
				Best = L.Ahead(L.Stops[C.AtStop].S, L.Stops[K].S);
				G.AlightStop = K;
			}
		}
		R.Legs.push_back(G);
		R.State = ERiderState::Riding;
		R.Cabin = CI;
		R.Asked = R.Since = Time;
		R.Seat = (int)C.Riders.size();
		Riders.push_back(R);
		RI = (int)Riders.size() - 1;
		C.Riders.push_back(RI);
		if (std::find(C.Targets.begin(), C.Targets.end(), G.AlightStop) == C.Targets.end())
		{
			C.Targets.push_back(G.AlightStop);
		}
		if (C.State == ECabinState::Idle)
		{
			C.State = ECabinState::Dwelling;
			C.DwellUntil = Time + 5.0;
		}
		Emit(EEvent::Board, CI, C.AtStop, RI);
		return RI;
	}

	void FNetwork::PlayerAlight(int CI)
	{
		const int RI = FindRider(PlayerId);
		if (RI < 0 || Riders[RI].Cabin != CI || Riders[RI].State != ERiderState::Riding)
		{
			return;
		}
		FCabin& C = Cabins[CI];
		C.Riders.erase(std::remove(C.Riders.begin(), C.Riders.end(), RI), C.Riders.end());
		Riders[RI].State = ERiderState::Done;
		Riders[RI].Cabin = -1;
		Emit(EEvent::Alight, CI, C.AtStop, RI);
		// the stops nobody needs any more go (the player's own one, if they got out early)
		std::vector<int> Keep;
		for (int T : C.Targets)
		{
			bool bNeeded = false;
			for (int R2 : C.Riders)
			{
				bNeeded |= Riders[R2].Legs[Riders[R2].LegI].AlightStop == T;
			}
			for (const FRider& R2 : Riders)
			{
				bNeeded |= R2.State == ERiderState::Waiting && R2.Cabin == CI && R2.Legs[R2.LegI].BoardStop == T;
			}
			if (bNeeded)
			{
				Keep.push_back(T);
			}
		}
		C.Targets = Keep;
	}

	// --- running ---------------------------------------------------------------------------------------------

	int FNetwork::NextTarget(const FCabin& C) const
	{
		const FLine& L = Lines[C.Line];
		int Best = -1;
		float BestD = 1e30f;
		for (int T : C.Targets)
		{
			const float D = (C.AtStop == T) ? 0.f : L.Ahead(C.S, L.Stops[T].S);
			if (D < BestD)
			{
				BestD = D;
				Best = T;
			}
		}
		if (Best < 0 && C.bToDepot)
		{
			Best = L.DepotStop;
		}
		return Best;
	}

	void FNetwork::ArriveAt(int CI, int Stop)
	{
		FCabin& C = Cabins[CI];
		const FLine& L = Lines[C.Line];
		C.S = L.Stops[Stop].S;
		C.Speed = 0.f;
		C.AtStop = Stop;
		if (C.bToDepot && Stop == L.DepotStop && C.Targets.empty() && C.Riders.empty() && C.Reserved == 0 && !C.bPlayerAboard)
		{
			C.State = ECabinState::Depot;
			C.bToDepot = false;
			int Slot = 0;
			for (const FCabin& O : Cabins)
			{
				if (&O != &C && O.Line == C.Line && O.State == ECabinState::Depot)
				{
					Slot = std::max(Slot, O.DepotSlot + 1);
				}
			}
			C.DepotSlot = Slot;
			Emit(EEvent::Park, CI, Stop, -1);
			return;
		}
		const bool bNew = C.State != ECabinState::Dwelling;
		C.State = ECabinState::Dwelling;
		if (bNew)
		{
			C.DwellUntil = Time + P.Dwell;
			Emit(EEvent::Arrive, CI, Stop, -1);
		}
		// out first
		for (int K = (int)C.Riders.size() - 1; K >= 0; K--)
		{
			const int RI = C.Riders[K];
			FRider& R = Riders[RI];
			if (R.Legs[R.LegI].AlightStop != Stop)
			{
				continue;
			}
			C.Riders.erase(C.Riders.begin() + K);
			R.Cabin = -1;
			Emit(EEvent::Alight, CI, Stop, RI);
			if (R.LegI + 1 < (int)R.Legs.size())
			{
				R.LegI++;
				StartLeg(R);
				Assign(R, RI);
			}
			else
			{
				R.State = ERiderState::Done;
				Emit(EEvent::Done, CI, Stop, RI);
			}
		}
		// then in: whoever waits here for this line (the player gets in by walking in: PlayerBoard)
		for (int RI = 0; RI < (int)Riders.size(); RI++)
		{
			FRider& R = Riders[RI];
			if (R.State != ERiderState::Waiting || R.Who == PlayerId)
			{
				continue;
			}
			const FLeg& G = R.Legs[R.LegI];
			if (G.Line != C.Line || G.BoardStop != Stop)
			{
				continue;
			}
			if (R.Cabin != CI)
			{
				if ((int)C.Riders.size() + C.Reserved >= P.Capacity)
				{
					continue;
				}
				if (R.Cabin >= 0)
				{
					Cabins[R.Cabin].Reserved = std::max(0, Cabins[R.Cabin].Reserved - 1);
				}
			}
			else
			{
				C.Reserved = std::max(0, C.Reserved - 1);
			}
			R.State = ERiderState::Riding;
			R.Cabin = CI;
			R.Seat = (int)C.Riders.size();
			C.Riders.push_back(RI);
			if (std::find(C.Targets.begin(), C.Targets.end(), G.AlightStop) == C.Targets.end())
			{
				C.Targets.push_back(G.AlightStop);
			}
			Emit(EEvent::Board, CI, Stop, RI);
			C.DwellUntil = std::max(C.DwellUntil, Time + 4.0);
		}
		// the stop is served (a player still due here keeps it until they get in or the cabin goes)
		bool bPlayerDue = false;
		for (const FRider& R : Riders)
		{
			if (R.Who == PlayerId && R.State == ERiderState::Waiting && R.Cabin == CI && R.Legs[R.LegI].BoardStop == Stop)
			{
				bPlayerDue = true;
			}
		}
		if (!bPlayerDue)
		{
			C.Targets.erase(std::remove(C.Targets.begin(), C.Targets.end(), Stop), C.Targets.end());
		}
	}

	void FNetwork::Leave(FCabin& C, int CI)
	{
		const int Stop = C.AtStop;
		C.Targets.erase(std::remove(C.Targets.begin(), C.Targets.end(), Stop), C.Targets.end());
		// a player who was to get in here and did not: their cabin is gone, another is found
		for (int RI = 0; RI < (int)Riders.size(); RI++)
		{
			FRider& R = Riders[RI];
			if (R.State == ERiderState::Waiting && R.Cabin == CI && R.Legs[R.LegI].BoardStop == Stop)
			{
				R.Cabin = -1;
				C.Reserved = std::max(0, C.Reserved - 1);
			}
		}
		Emit(EEvent::Depart, CI, Stop, -1);
		C.AtStop = -1;
		C.State = ECabinState::Moving;
		C.Speed = 0.f;
	}

	bool FNetwork::Clear(int CI, float S) const
	{
		const FCabin& C = Cabins[CI];
		const FLine& L = Lines[C.Line];
		for (int O = 0; O < (int)Cabins.size(); O++)
		{
			const FCabin& Oc = Cabins[O];
			if (O != CI && Oc.Line == C.Line && Oc.State != ECabinState::Depot && Oc.State != ECabinState::Idle
				&& (L.Ahead(S, Oc.S) < P.Headway || L.Ahead(Oc.S, S) < P.Headway))
			{
				return false;
			}
		}
		return true;
	}

	void FNetwork::Dispatch()
	{
		for (int RI = 0; RI < (int)Riders.size(); RI++)
		{
			FRider& R = Riders[RI];
			if (R.State == ERiderState::Waiting && R.Cabin < 0)
			{
				Assign(R, RI);
			}
		}
	}

	void FNetwork::Step(float Dt)
	{
		const int N = std::max(1, (int)std::ceil(Dt / 0.1f));
		const float H = Dt / N;
		for (int Sub = 0; Sub < N; Sub++)
		{
			Time += H;
			Dispatch();
			for (int CI = 0; CI < (int)Cabins.size(); CI++)
			{
				FCabin& C = Cabins[CI];
				const FLine& L = Lines[C.Line];
				switch (C.State)
				{
				case ECabinState::Depot:
				{
					if (C.Targets.empty())
					{
						break;
					}
					// out of the depot onto the loop when nobody is within the headway of the depot's stop
					const float DS = L.Stops[L.DepotStop].S;
					if (!Clear(CI, DS))
					{
						break;
					}
					C.State = ECabinState::Moving;
					C.AtStop = -1;
					C.S = DS;
					C.Speed = 0.f;
					Emit(EEvent::Launch, CI, L.DepotStop, -1);
					if (NextTarget(C) == L.DepotStop)
					{
						ArriveAt(CI, L.DepotStop);
					}
					break;
				}
				case ECabinState::Idle:
				{
					C.Door = std::max(0.f, C.Door - H / P.DoorTime);
					if (C.bPlayerAboard)
					{
						C.Door = std::min(1.f, C.Door + 2.f * H / P.DoorTime);
						C.IdleSince = Time;
					}
					if (!C.Targets.empty())
					{
						if (NextTarget(C) == C.AtStop)
						{
							ArriveAt(CI, C.AtStop);
						}
						else if (Clear(CI, C.S))
						{
							Leave(C, CI);       // off the siding onto the rope when it is clear
						}
						break;
					}
					// one cabin may wait at each end station that has no depot; the others go home
					const FStop& Here = L.Stops[C.AtStop];
					bool bKeeper = Here.bEnd && Here.Station != L.Depot;
					for (int O = 0; O < CI && bKeeper; O++)
					{
						if (Cabins[O].Line == C.Line && Cabins[O].State == ECabinState::Idle && Cabins[O].AtStop == C.AtStop)
						{
							bKeeper = false;
						}
					}
					if (!bKeeper && !C.bPlayerAboard && Time - C.IdleSince > P.IdleReturn)
					{
						C.bToDepot = true;
						if (C.AtStop == L.DepotStop)
						{
							ArriveAt(CI, C.AtStop);
						}
						else if (Clear(CI, C.S))
						{
							Leave(C, CI);
						}
						else
						{
							C.bToDepot = false;
						}
					}
					break;
				}
				case ECabinState::Dwelling:
					if (Time < C.DwellUntil || C.bHold)
					{
						C.Door = std::min(1.f, C.Door + H / P.DoorTime);
						break;
					}
					C.Door = std::max(0.f, C.Door - H / P.DoorTime);
					if (C.Door > 0.f)
					{
						break;
					}
					{
						// anybody still to carry, or to fetch: go; else wait here
						C.Targets.erase(std::remove(C.Targets.begin(), C.Targets.end(), C.AtStop), C.Targets.end());
						bool bPlayerDue = false;
						for (const FRider& R : Riders)
						{
							if (R.Who == PlayerId && R.State == ERiderState::Waiting && R.Cabin == CI && R.Legs[R.LegI].BoardStop == C.AtStop)
							{
								bPlayerDue = true;
							}
						}
						if (bPlayerDue && Time < C.DwellUntil + 25.0)
						{
							// the player asked for it here: give them a little longer, doors open
							C.Door = 1.f;
							C.DwellUntil = Time + 5.0;
							break;
						}
						if (C.Targets.empty() && C.Riders.empty() && C.Reserved == 0)
						{
							Leave(C, CI);
							C.State = ECabinState::Idle;
							C.AtStop = NextTarget(C) >= 0 ? C.AtStop : C.AtStop;
							// stays where it stood
							for (int K = 0; K < (int)L.Stops.size(); K++)
							{
								if (std::fabs(L.Stops[K].S - C.S) < 0.01f)
								{
									C.AtStop = K;
								}
							}
							C.IdleSince = Time;
						}
						else
						{
							Leave(C, CI);
						}
					}
					break;
				case ECabinState::Moving:
				{
					const int T = NextTarget(C);
					if (T < 0)
					{
						// nothing to do: stop at the next stop and wait there
						float Best = 1e30f;
						int K0 = -1;
						for (int K = 0; K < (int)L.Stops.size(); K++)
						{
							const float D = L.Ahead(C.S, L.Stops[K].S);
							if (D < Best)
							{
								Best = D;
								K0 = K;
							}
						}
						C.Targets.push_back(K0);
						break;
					}
					const float Dist = L.Ahead(C.S, L.Stops[T].S);
					const bool bEmpty = C.Riders.empty();
					float V = bEmpty ? P.SpeedEmpty : P.Speed;
					// through a station slowly (off the rope, on the rail)
					for (const FStop& Sp : L.Stops)
					{
						const float Fwd = L.Ahead(C.S, Sp.S), Back = L.Ahead(Sp.S, C.S);
						const float Zone = P.PlatformLen * 0.5f + 6.f;
						if (Fwd < Zone || Back < Zone)
						{
							V = std::min(V, P.SpeedThrough);
						}
						else if (Fwd < Zone + 60.f)
						{
							V = std::min(V, std::sqrt(P.SpeedThrough * P.SpeedThrough + 2.f * P.Accel * (Fwd - Zone)));
						}
					}
					// stopping at the target
					V = std::min(V, std::sqrt(2.f * P.Accel * std::max(0.f, Dist)) + 0.15f);
					// keep the headway to the cabin ahead (parked ones stand on the siding)
					float Gap = 1e30f;
					for (int O = 0; O < (int)Cabins.size(); O++)
					{
						const FCabin& Oc = Cabins[O];
						if (O == CI || Oc.Line != C.Line || Oc.State == ECabinState::Depot || Oc.State == ECabinState::Idle)
						{
							continue;
						}
						const float G = L.Ahead(C.S, Oc.S);
						if (G > 0.01f && G < Gap)
						{
							Gap = G;
						}
					}
					if (Gap < 1e29f)
					{
						V = std::min(V, std::sqrt(2.f * P.Accel * std::max(0.f, Gap - P.Headway)));
					}
					if (V > C.Speed)
					{
						C.Speed = std::min(V, C.Speed + P.Accel * H);
					}
					else
					{
						C.Speed = std::max(V, C.Speed - 2.5f * P.Accel * H);
					}
					const float Move = std::min(C.Speed * H, Dist);
					C.S = std::fmod(C.S + Move, L.Length);
					C.Odometer += Move;
					if (bEmpty)
					{
						C.EmptyMetres += Move;
					}
					if (Dist - Move < 0.02f)
					{
						ArriveAt(CI, T);
					}
					break;
				}
				}
			}
		}
	}

	// --- queries ---------------------------------------------------------------------------------------------

	float FNetwork::Eta(int St, int Li, int Dir, bool& bCalled) const
	{
		bCalled = false;
		if (Li < 0 || Li >= (int)Lines.size())
		{
			return -1.f;
		}
		const FLine& L = Lines[Li];
		float Best = 1e30f;
		for (int K = 0; K < (int)L.Stops.size(); K++)
		{
			if (L.Stops[K].Station != St || (Dir >= 0 && L.Stops[K].Dir != Dir && !L.Stops[K].bEnd))
			{
				continue;
			}
			for (const FCabin& C : Cabins)
			{
				if (C.Line != Li || std::find(C.Targets.begin(), C.Targets.end(), K) == C.Targets.end())
				{
					continue;
				}
				const float T = (C.AtStop == K && C.State != ECabinState::Moving) ? 0.f
					: L.Ahead(C.S, L.Stops[K].S) / std::max(1.f, (C.Riders.empty() ? P.SpeedEmpty : P.Speed) * 0.8f);
				if (T < Best)
				{
					Best = T;
					bCalled = true;
				}
			}
		}
		if (bCalled)
		{
			return Best;
		}
		for (int K = 0; K < (int)L.Stops.size(); K++)
		{
			if (L.Stops[K].Station != St || (Dir >= 0 && L.Stops[K].Dir != Dir && !L.Stops[K].bEnd))
			{
				continue;
			}
			for (const FCabin& C : Cabins)
			{
				if (C.Line == Li && (int)C.Riders.size() + C.Reserved < P.Capacity)
				{
					Best = std::min(Best, CostTo(C, Li, K));
				}
			}
		}
		return Best < 1e29f ? Best : -1.f;
	}

	int FNetwork::NextStation(int CI) const
	{
		const FCabin& C = Cabins[CI];
		const int T = NextTarget(C);
		return T >= 0 ? Lines[C.Line].Stops[T].Station : -1;
	}

	int FNetwork::Destination(int CI) const
	{
		const FCabin& C = Cabins[CI];
		const FLine& L = Lines[C.Line];
		// the farthest stop ahead that it has to stop at
		int Best = -1;
		float BestD = -1.f;
		for (int T : C.Targets)
		{
			const float D = L.Ahead(C.S, L.Stops[T].S);
			if (D > BestD)
			{
				BestD = D;
				Best = T;
			}
		}
		if (Best < 0 && C.bToDepot)
		{
			Best = L.DepotStop;
		}
		return Best >= 0 ? L.Stops[Best].Station : -1;
	}

	V3 FNetwork::CabinAt(int CI, V3* OutDir) const
	{
		const FCabin& C = Cabins[CI];
		return Lines[C.Line].At(C.S, OutDir);
	}

	std::string FNetwork::Report() const
	{
		std::string Out;
		char Buf[256];
		for (int CI = 0; CI < (int)Cabins.size(); CI++)
		{
			const FCabin& C = Cabins[CI];
			const int Next = NextStation(CI);
			std::snprintf(Buf, sizeof(Buf), "%s %s %-8s s=%6.0f v=%4.1f riders=%d res=%d next=%s%s\n", C.Id.c_str(), Lines[C.Line].Id.c_str(),
				CabinStateName(C.State), C.S, C.Speed, (int)C.Riders.size(), C.Reserved, Next >= 0 ? Stations[Next].Id.c_str() : "-",
				C.bToDepot ? " (home)" : "");
			Out += Buf;
		}
		int Waiting = 0, Riding = 0;
		for (const FRider& R : Riders)
		{
			Waiting += R.State == ERiderState::Waiting;
			Riding += R.State == ERiderState::Riding;
		}
		std::snprintf(Buf, sizeof(Buf), "riders waiting %d, riding %d, t=%.0f\n", Waiting, Riding, Time);
		Out += Buf;
		return Out;
	}
}
