#include "RailRun.h"

#include "../People/MiniJson.h"

#include <algorithm>
#include <cmath>

namespace LyudRail
{
namespace
{
constexpr double RunDT = 0.2;          // integration step, s
constexpr double DoorDelay = 2.5;      // doors open this long after the train stops
constexpr double DoorEarly = 6.0;      // and shut this long before it leaves

double PosMod(double A, double B)
{
	const double R = std::fmod(A, B);
	return R < 0.0 ? R + B : R;
}

double Limit(const FRunParams& P, double S)
{
	if (S < P.SlowUntilS)
	{
		return P.VLand;
	}
	if (std::fabs(S - P.IslandS) < P.IslandHalf)
	{
		return P.VIsland;
	}
	return P.VMax;
}
}

void FRun::Simulate(const FRunParams& P, int Dir, std::vector<double>& T, std::vector<double>& S, std::vector<double>& V,
	std::vector<double>& Open, std::vector<double>& Close)
{
	T.clear();
	S.clear();
	V.clear();
	const int NStops = int(P.Stops.size());
	Open.assign(NStops, -1.0);
	Close.assign(NStops, -1.0);
	// the stops in the order this way meets them, then the end
	std::vector<int> Order;
	for (int i = 0; i < NStops; i++)
	{
		Order.push_back(Dir > 0 ? i : NStops - 1 - i);
	}
	const double Start = Dir > 0 ? 0.0 : P.Length;
	const double End = Dir > 0 ? P.Length : 0.0;
	// the speed limit's falls ahead (for braking into a slower stretch): boundaries of the zones
	std::vector<double> Bounds = { P.SlowUntilS, P.IslandS - P.IslandHalf, P.IslandS + P.IslandHalf };

	double t = 0.0, s = Start, v = 0.0;
	size_t Next = 0;
	auto Push = [&]() { T.push_back(t); S.push_back(s); V.push_back(v * Dir); };
	Push();
	for (int Guard = 0; Guard < 2000000; Guard++)
	{
		const bool bEnd = Next >= Order.size();
		const double Target = bEnd ? End : P.Stops[Order[Next]].S;
		const double Dist = (Target - s) * Dir;
		if (Dist <= 0.02 || (v < 0.05 && Dist < 0.3))
		{
			s = Target;
			v = 0.0;
			Push();
			if (bEnd)
			{
				break;
			}
			const int K = Order[Next];
			Open[K] = t + DoorDelay;
			Close[K] = t + P.Dwell - DoorEarly;
			t += P.Dwell;
			Push();
			Next++;
			continue;
		}
		double Want = std::min(Limit(P, s), std::sqrt(2.0 * P.Decel * std::max(Dist - 0.01, 0.0)));
		for (double B : Bounds)
		{
			const double D = (B - s) * Dir;
			if (D > 0.0 && D < 600.0)
			{
				// the limit just past the boundary, coming this way
				const double After = Limit(P, B + Dir * 0.5);
				Want = std::min(Want, std::sqrt(After * After + 2.0 * P.Decel * D));
			}
		}
		Want = std::max(Want, 0.3);    // creeps the last centimetres
		if (v < Want)
		{
			v = std::min(Want, v + P.Accel * RunDT);
		}
		else
		{
			v = std::max(Want, v - P.Decel * 1.4 * RunDT);
		}
		s += Dir * std::min(v * RunDT, Dist);
		t += RunDT;
		Push();
	}
}

void FRun::Build(const FRunParams& InParams)
{
	P = InParams;
	P.Trains = std::max(P.Trains, 1);
	Simulate(P, 1, OutT, OutS, OutV, OutStopOpen, OutStopClose);
	Simulate(P, -1, InT, InS, InV, InStopOpen, InStopClose);
	InStart = OutT.back() + P.Layover;
	Cycle = InStart + InT.back() + P.Layover;
	if (Cycle > P.Period)
	{
		// one train on a single track: the period stretches to its round trip
		P.Period = std::ceil(Cycle / 60.0) * 60.0;
	}
}

void FRun::Sample(const std::vector<double>& T, const std::vector<double>& S, const std::vector<double>& V, double t,
	double& OutSv, double& OutVv)
{
	if (T.empty())
	{
		OutSv = 0.0;
		OutVv = 0.0;
		return;
	}
	if (t <= T.front())
	{
		OutSv = S.front();
		OutVv = 0.0;
		return;
	}
	if (t >= T.back())
	{
		OutSv = S.back();
		OutVv = 0.0;
		return;
	}
	const size_t I = size_t(std::upper_bound(T.begin(), T.end(), t) - T.begin());
	const size_t A = I - 1;
	const double Span = T[I] - T[A];
	const double F = Span > 1e-9 ? (t - T[A]) / Span : 0.0;
	OutSv = S[A] + (S[I] - S[A]) * F;
	OutVv = V[A] + (V[I] - V[A]) * F;
}

FTrainState FRun::At(double Time, int Index) const
{
	FTrainState St;
	const double Offset = P.First + double(Index) * P.Period / double(P.Trains);
	const double Ph = PosMod(Time - Offset, P.Period);
	St.Phase = Ph;
	const double OutDur = OutT.empty() ? 0.0 : OutT.back();
	const double InDur = InT.empty() ? 0.0 : InT.back();
	auto AtStop = [&](const std::vector<double>& Open, const std::vector<double>& Close, double t, double Dwell)
	{
		for (size_t k = 0; k < Open.size(); k++)
		{
			if (Open[k] >= 0.0 && t >= Open[k] - DoorDelay - 0.01 && t <= Open[k] - DoorDelay + Dwell + 0.01)
			{
				St.Stop = int(k);
				St.bDoorsOpen = t >= Open[k] && t <= Close[k];
				return;
			}
		}
	};
	if (Ph < OutDur)
	{
		Sample(OutT, OutS, OutV, Ph, St.S, St.V);
		St.Dir = 1;
		AtStop(OutStopOpen, OutStopClose, Ph, P.Dwell);
	}
	else if (Ph < InStart)
	{
		St.S = P.Length;
		St.Dir = 1;
		St.bInService = false;
	}
	else if (Ph < InStart + InDur)
	{
		Sample(InT, InS, InV, Ph - InStart, St.S, St.V);
		St.Dir = -1;
		AtStop(InStopOpen, InStopClose, Ph - InStart, P.Dwell);
	}
	else
	{
		St.S = 0.0;
		St.Dir = -1;
		St.bInService = false;
	}
	// in the tunnel's dark (more than a car's length in from the mouth) or gone into the haze
	St.bVisible = St.S > P.PortalS - 60.0 && St.S < P.HiddenS;
	return St;
}

double FRun::NextDoorsOpen(int Stop, int Dir, double Time, int* OutTrain) const
{
	if (Stop < 0 || Stop >= int(P.Stops.size()))
	{
		return -1.0;
	}
	const double Off = Dir > 0 ? OutStopOpen[Stop] : InStart + InStopOpen[Stop];
	if (Off < 0.0)
	{
		return -1.0;
	}
	double Best = -1.0;
	for (int i = 0; i < P.Trains; i++)
	{
		const double Base = P.First + double(i) * P.Period / double(P.Trains) + Off;
		const double K = std::ceil((Time - Base) / P.Period);
		const double When = Base + K * P.Period;
		if (Best < 0.0 || When < Best)
		{
			Best = When;
			if (OutTrain)
			{
				*OutTrain = i;
			}
		}
	}
	return Best;
}

double FRun::RideSeconds(int StopA, int StopB, int Dir) const
{
	const int N = int(P.Stops.size());
	if (StopA < 0 || StopB < 0 || StopA >= N || StopB >= N)
	{
		return -1.0;
	}
	const std::vector<double>& Open = Dir > 0 ? OutStopOpen : InStopOpen;
	const double D = Open[StopB] - Open[StopA];
	return D > 0.0 ? D : -1.0;
}

double FRun::NextPass(double S, double Time, int* OutDir) const
{
	auto Cross = [](const std::vector<double>& T, const std::vector<double>& Sv, double At) -> double
	{
		for (size_t i = 1; i < Sv.size(); i++)
		{
			if ((Sv[i - 1] - At) * (Sv[i] - At) <= 0.0 && Sv[i - 1] != Sv[i])
			{
				const double F = (At - Sv[i - 1]) / (Sv[i] - Sv[i - 1]);
				return T[i - 1] + (T[i] - T[i - 1]) * F;
			}
		}
		return -1.0;
	};
	const double OutOff = Cross(OutT, OutS, S);
	const double InOff = Cross(InT, InS, S);
	double Best = -1.0;
	for (int i = 0; i < P.Trains; i++)
	{
		const double Base = P.First + double(i) * P.Period / double(P.Trains);
		for (int d = 0; d < 2; d++)
		{
			const double Off = d == 0 ? OutOff : (InOff >= 0.0 ? InStart + InOff : -1.0);
			if (Off < 0.0)
			{
				continue;
			}
			const double K = std::ceil((Time - Base - Off) / P.Period);
			const double When = Base + Off + K * P.Period;
			if (Best < 0.0 || When < Best)
			{
				Best = When;
				if (OutDir)
				{
					*OutDir = d == 0 ? 1 : -1;
				}
			}
		}
	}
	return Best < 0.0 ? -1.0 : Best - Time;
}

int FRun::FindStop(const std::string& Id) const
{
	for (size_t i = 0; i < P.Stops.size(); i++)
	{
		if (P.Stops[i].Id == Id)
		{
			return int(i);
		}
	}
	return -1;
}

bool LoadParams(const std::string& BuiltJson, FRunParams& Out, std::string& Error)
{
	LyudJson::FValue J;
	if (!LyudJson::Parse(BuiltJson, J, Error))
	{
		return false;
	}
	const LyudJson::FValue& Ln = J["line"];
	const LyudJson::FValue& Tr = J["train"];
	if (Ln.Type != LyudJson::FValue::Object || Tr.Type != LyudJson::FValue::Object)
	{
		Error = "no line or train";
		return false;
	}
	FRunParams P;
	P.Length = Ln["length"].F(26000.f);
	const double X0 = Ln["tunnel_start_x"].F(0.f);
	P.HiddenS = Tr["hidden_beyond_x"].F(24000.f) - X0;
	P.PortalS = Ln["portal_s"].F(283.f);
	P.VMax = Tr["vmax"].F(15.f);
	P.VLand = Tr["vmax_land"].F(7.f);
	P.VIsland = Tr["vmax_island"].F(9.f);
	P.Accel = Tr["accel"].F(0.45f);
	P.Decel = Tr["decel"].F(0.6f);
	P.Dwell = Tr["dwell"].F(45.f);
	P.Layover = Tr["layover"].F(240.f);
	P.Period = Tr["period"].F(3600.f);
	P.First = Tr["first_departure"].F(0.f);
	P.Trains = Tr["trains"].I(1);
	const LyudJson::FValue& St = J["stops"];
	double LastStop = P.PortalS;
	for (size_t i = 0; i < St.Size(); i++)
	{
		FStop S;
		S.Id = St[i]["id"].S();
		S.S = St[i]["s"].F();
		P.Stops.push_back(S);
		if (i == 0)
		{
			LastStop = S.S;
		}
	}
	// slow by the tunnel, the crossing and the first (beach) halt
	P.SlowUntilS = std::max(P.PortalS + 80.0, LastStop + 40.0);
	const LyudJson::FValue& Isl = J["island"];
	if (Isl.Type == LyudJson::FValue::Object)
	{
		P.IslandS = Isl["halt_s"].F(2564.f);
		P.IslandHalf = Isl["rx"].F(105.f) * 1.4;
	}
	Out = P;
	return true;
}
}
