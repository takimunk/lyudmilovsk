#include "BeachModel.h"

#include <algorithm>
#include <cmath>

namespace LyudBeach
{
	namespace
	{
		constexpr double G = 9.81;

		double Clamp01(double V) { return V < 0.0 ? 0.0 : V > 1.0 ? 1.0 : V; }
	}

	uint32_t Hash(uint32_t A, uint32_t B, uint32_t C)
	{
		uint32_t H = A * 0x9e3779b1u;
		H ^= B + 0x7f4a7c15u + (H << 6) + (H >> 2);
		H ^= C + 0x165667b1u + (H << 6) + (H >> 2);
		H ^= H >> 16;
		H *= 0x7feb352du;
		H ^= H >> 15;
		H *= 0x846ca68bu;
		H ^= H >> 16;
		return H;
	}

	double Unit(uint32_t H)
	{
		return (H >> 8) * (1.0 / 16777216.0);
	}

	// ------------------------------------------------------------------ stones

	double StoneFlatness(int CellX, int CellY, int Day, int Try)
	{
		const uint32_t H = Hash(static_cast<uint32_t>(CellX) * 73856093u ^ static_cast<uint32_t>(CellY) * 19349663u,
			static_cast<uint32_t>(Day), static_cast<uint32_t>(Try));
		const double U = Unit(H);
		// seven in ten are pebbles (0 .. 0.6), the rest flat, and a really good one is rare
		if (U < 0.7)
		{
			return U / 0.7 * (FlatFrom - 0.02);
		}
		const double V = (U - 0.7) / 0.3;
		return FlatFrom + (1.0 - FlatFrom) * V * V;
	}

	double SwingAt(double HeldSeconds)
	{
		if (HeldSeconds <= 0.0)
		{
			return 0.0;
		}
		const double P = std::fmod(HeldSeconds / SwingSeconds, 2.0);
		return P <= 1.0 ? P : 2.0 - P;
	}

	double ReleaseQuality(double Swing)
	{
		return Clamp01(1.0 - std::fabs(Swing - BestSwing) / SwingWidth);
	}

	FThrowOut Throw(const FThrowIn& In)
	{
		FThrowOut Out;
		const double Q = Clamp01(In.Quality);
		const double F = Clamp01(In.Flatness);
		const double Wave = std::max(0.0, std::min(In.WaveM, 2.0));
		double Speed = 8.0 + 7.0 * Q;
		// how much of its speed a touch leaves it: a flat stone thrown well keeps nine tenths, a pebble a third
		double Keep = IsFlat(F) ? 0.52 + 0.42 * F * Q : 0.15 + 0.35 * F * Q;
		Keep = std::max(0.0, Keep - 0.25 * Wave);
		// a wave's face trips it (ripples under 15 cm do not)
		const double Trip = std::min(0.6, 0.3 * std::max(0.0, Wave - 0.15) * (1.2 - F));
		// the first leg: from the hand, a metre up, almost flat
		double T = std::sqrt(2.0 * 1.0 / G) * (0.75 + 0.25 * (1.0 - Q));
		double X = Speed * T;
		double Height = 1.0;
		for (int Touch = 0; Touch < 64; ++Touch)
		{
			FHop Hop;
			Hop.X = X;
			Hop.T = T;
			Hop.Height = Height;
			const double After = Speed * Keep;
			const bool bTripped = Touch > 0 && Unit(Hash(In.Seed, static_cast<uint32_t>(Touch), 0x51ed270bu)) < Trip;
			if (In.DepthM < 0.15 || After < 2.5 || bTripped)
			{
				Out.Hops.push_back(Hop);
				break;
			}
			Hop.SpeedOut = After;
			Out.Hops.push_back(Hop);
			Speed = After;
			const double Up = 0.14 * Speed;
			const double Air = 2.0 * Up / G;
			Height = Up * Up / (2.0 * G);
			T += Air;
			X += Speed * Air;
		}
		Out.Skips = static_cast<int>(Out.Hops.size()) - 1;
		Out.Distance = Out.Hops.back().X;
		Out.Seconds = Out.Hops.back().T;
		return Out;
	}

	double StoneAt(const FThrowOut& Flight, double T, double& XOut)
	{
		XOut = 0.0;
		if (Flight.Hops.empty())
		{
			return 0.0;
		}
		double T0 = 0.0, X0 = 0.0;
		for (size_t I = 0; I < Flight.Hops.size(); ++I)
		{
			const FHop& Hop = Flight.Hops[I];
			if (T <= Hop.T)
			{
				const double U = Hop.T > T0 ? (T - T0) / (Hop.T - T0) : 1.0;
				XOut = X0 + (Hop.X - X0) * U;
				// the first leg falls from the hand; the others are arcs
				return I == 0 ? Hop.Height * (1.0 - U * U) : Hop.Height * 4.0 * U * (1.0 - U);
			}
			T0 = Hop.T;
			X0 = Hop.X;
		}
		XOut = Flight.Hops.back().X;
		return 0.0;
	}

	// ------------------------------------------------------------------ the sandcastle

	bool CanBuild(double GroundZ, double SeaZ, double WetLineZ)
	{
		return GroundZ > SeaZ + DampUnderM && GroundZ < std::max(WetLineZ, SeaZ) + DampOverM;
	}

	bool Build(FCastle& Castle, double Dt)
	{
		if (!Castle.bAlive || Castle.Work >= CastleStages || Dt <= 0.0)
		{
			return false;
		}
		const int Before = Castle.Stage();
		Castle.Work = std::min(static_cast<double>(CastleStages), Castle.Work + Dt / StageSeconds);
		// patting the sand mends what the water took
		Castle.Worn = std::max(0.0, Castle.Worn - Dt / (StageSeconds * 2.0));
		return Castle.Stage() > Before;
	}

	bool Wash(FCastle& Castle, double SeaZ, double WaveM, bool bRain, double Dt)
	{
		if (!Castle.bAlive || Dt <= 0.0)
		{
			return false;
		}
		double Over = SeaZ + 0.4 * std::max(0.0, WaveM) - Castle.Z;
		if (Castle.HasMoat())
		{
			Over -= 0.06;
		}
		double Rate = 0.0;
		if (Over > 0.0)
		{
			// the run-ups lick the foot: gone in ten minutes; the sea standing over it: in under a minute
			Rate = std::max(0.2, std::min(3.0, Over / 0.10)) / 120.0;
		}
		if (bRain)
		{
			Rate += 1.0 / (3.0 * 3600.0);
		}
		Castle.Worn += Rate * Dt;
		if (Castle.Worn >= 1.0)
		{
			Castle.Worn = 1.0;
			Castle.bAlive = false;
			return true;
		}
		return false;
	}

	double HoursUntilWash(double Z, double WaveM, const std::function<double(double)>& LevelAt, double Within)
	{
		for (double H = 0.0; H <= Within; H += 1.0 / 12.0)
		{
			if (LevelAt(H) + 0.4 * std::max(0.0, WaveM) > Z)
			{
				return H;
			}
		}
		return -1.0;
	}

	// ------------------------------------------------------------------ the buoy swim

	void RaceReady(FRace& Race, double CourseM)
	{
		Race = FRace();
		Race.State = ERace::Ready;
		Race.CourseM = std::max(10.0, CourseM);
	}

	ERaceEvent RaceStep(FRace& Race, const FRaceIn& In, double Dt)
	{
		switch (Race.State)
		{
		case ERace::Ready:
			if (In.bSwimming)
			{
				Race.State = ERace::Out;
				Race.Seconds = 0.0;
				return ERaceEvent::Started;
			}
			Race.ReadyFor += Dt;
			if (Race.ReadyFor > ReadySeconds)
			{
				Race.State = ERace::Idle;
				return ERaceEvent::GaveUp;
			}
			return ERaceEvent::None;
		case ERace::Out:
		case ERace::Back:
			Race.Seconds += Dt;
			if (Race.Seconds > SlowestPerM * 2.0 * Race.CourseM)
			{
				Race.State = ERace::Failed;
				return ERaceEvent::GaveUp;
			}
			if (Race.State == ERace::Out)
			{
				if (In.ToBuoyM < BuoyTouchM)
				{
					Race.State = ERace::Back;
					Race.OutSeconds = Race.Seconds;
					return ERaceEvent::Turned;
				}
				// back on the sand without the buoy: he changed his mind
				if (In.bAshore && Race.Seconds > 8.0)
				{
					Race.State = ERace::Failed;
					return ERaceEvent::GaveUp;
				}
			}
			else if (In.bAshore)
			{
				Race.State = ERace::Done;
				return ERaceEvent::Finished;
			}
			return ERaceEvent::None;
		default:
			return ERaceEvent::None;
		}
	}

	int Medal(double Seconds, double CourseM)
	{
		if (Seconds <= 0.0)
		{
			return 0;
		}
		const double Speed = 2.0 * CourseM / Seconds;
		return Speed >= 1.35 ? 3 : Speed >= 1.15 ? 2 : Speed >= 0.9 ? 1 : 0;
	}

	double LifeguardSeconds(double CourseM)
	{
		return 2.0 * CourseM / 1.5;
	}

	// ------------------------------------------------------------------ shells and amber

	const char* FindId(EFind Find)
	{
		switch (Find)
		{
		case EFind::Cockle: return "shell_cockle";
		case EFind::Mussel: return "shell_mussel";
		case EFind::Tellin: return "shell_tellin";
		case EFind::Whelk: return "shell_whelk";
		case EFind::Amber: return "amber";
		case EFind::AmberFly: return "amber_fly";
		default: return "";
		}
	}

	double StormScore(const double* WindMs, int Count)
	{
		double Max = 0.0;
		for (int I = 0; I < Count; ++I)
		{
			Max = std::max(Max, WindMs[I]);
		}
		return Clamp01((Max - 9.0) / 8.0);
	}

	bool FindIn(int Cell, int Day, double Storm, FFind& Out)
	{
		const double S = Clamp01(Storm);
		const uint32_t H = Hash(static_cast<uint32_t>(Cell), static_cast<uint32_t>(Day), 0x0a3be7c5u);
		const double U = Unit(H);
		// amber comes only with a storm (a crumb once in a blue moon on a calm day); shells always, many more after one
		const double Amber = S < 0.25 ? 0.004 : 0.11 * S;
		const double Shell = 0.12 + 0.45 * S;
		if (U >= Amber + Shell)
		{
			return false;
		}
		const double R1 = Unit(Hash(H, 1u));
		const double R2 = Unit(Hash(H, 2u));
		const double R3 = Unit(Hash(H, 3u));
		Out.Cell = Cell;
		Out.Along = (Cell + 0.15 + 0.7 * R1) * FindCellM;
		if (U < Amber)
		{
			Out.Kind = R3 < 0.08 ? EFind::AmberFly : EFind::Amber;
			// amber is light: the last run-up leaves it at the top of the wet sand, in the line of weed
			Out.Across = 0.72 + 0.28 * R2;
		}
		else
		{
			Out.Kind = R3 < 0.45 ? EFind::Cockle : R3 < 0.75 ? EFind::Mussel : R3 < 0.93 ? EFind::Tellin : EFind::Whelk;
			Out.Across = 0.1 + 0.85 * R2;
		}
		return true;
	}

	void Finds(int CellFrom, int CellTo, int Day, double Storm, std::vector<FFind>& Out)
	{
		for (int Cell = CellFrom; Cell <= CellTo; ++Cell)
		{
			FFind Find;
			if (FindIn(Cell, Day, Storm, Find))
			{
				Out.push_back(Find);
			}
		}
	}

	// ------------------------------------------------------------------ the book

	FNews NoteThrow(FBook& Book, int Skips)
	{
		FNews News;
		Book.Throws++;
		if (Skips > Book.BestSkips)
		{
			News.bBest = Skips >= 3;
			News.bTownRecord = Skips > TownRecord && Book.BestSkips <= TownRecord;
			Book.BestSkips = Skips;
		}
		return News;
	}

	FNews NoteSwim(FBook& Book, double Seconds)
	{
		FNews News;
		Book.Swims++;
		if (Seconds > 0.0 && (Book.BestSwim <= 0.0 || Seconds < Book.BestSwim))
		{
			News.bBest = true;
			Book.BestSwim = Seconds;
		}
		return News;
	}

	FNews NoteFind(FBook& Book, EFind Find)
	{
		FNews News;
		const uint32_t Bit = 1u << static_cast<int>(Find);
		News.bFirstOfKind = !(Book.FoundBits & Bit);
		Book.FoundBits |= Bit;
		if (IsAmber(Find))
		{
			Book.Ambers++;
		}
		else
		{
			Book.Shells++;
		}
		return News;
	}
}
