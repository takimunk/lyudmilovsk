#pragma once

// The motor ship's weekly two-day cruise (docs/ship.md): plain C++, no engine types, so tools/cruisesim checks it offline.
// Data: Content/Cruise/cruise.json (tools/ship/cruise.py).
//
// Everything is a pure function of the calendar: T = seconds since 1 January 2026 00:00 of the game's date. A voyage sails
// every Friday 18:00 of the season and is back on the roads on Sunday 18:00; "tv" is the time since it sailed (the boarding
// is tv -7200..0, the landing 172800..180000). The route is a closed belt on the chart (the map's metres carried out to sea,
// X west, Y north): circles (the home roads, turning points, islands passed, islands circled, the anchorage) joined by their
// tangents, so the position and the heading have no jump anywhere, and the end is the beginning. The ship runs along it by
// timed fixes (an island's event is when it is abeam): between two fixes the speed ramps smoothly to a steady one and back.
// The little that is not the calendar is the hero's own state (FHero: aboard or not, class, cabin, the voyage boarded,
// voyages sailed), which the game saves.

#include "../People/MiniJson.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace LyudCruise
{
	constexpr double Pi = 3.14159265358979323846;
	constexpr double DaySec = 86400.0;
	constexpr double WeekSec = 7.0 * DaySec;

	/** The same order as ELyudCruisePhase (Public/Cruise/LyudCruise.h). */
	enum class EPhase : uint8_t { Laid, Port, Boarding, Underway, Circling, Anchored, Landing };

	inline const char* PhaseName(EPhase P)
	{
		switch (P)
		{
		case EPhase::Laid: return "laid";
		case EPhase::Port: return "port";
		case EPhase::Boarding: return "boarding";
		case EPhase::Underway: return "underway";
		case EPhase::Circling: return "circling";
		case EPhase::Anchored: return "anchored";
		default: return "landing";
		}
	}

	struct FVec2
	{
		double X = 0.0, Y = 0.0;
	};
	inline FVec2 operator+(const FVec2& A, const FVec2& B) { return { A.X + B.X, A.Y + B.Y }; }
	inline FVec2 operator-(const FVec2& A, const FVec2& B) { return { A.X - B.X, A.Y - B.Y }; }
	inline FVec2 operator*(const FVec2& A, double K) { return { A.X * K, A.Y * K }; }
	inline double Dot(const FVec2& A, const FVec2& B) { return A.X * B.X + A.Y * B.Y; }
	inline double Cross(const FVec2& A, const FVec2& B) { return A.X * B.Y - A.Y * B.X; }
	inline double Len(const FVec2& A) { return std::sqrt(Dot(A, A)); }
	/** A quarter turn to the left (counter-clockwise on the chart). */
	inline FVec2 Left(const FVec2& A) { return { -A.Y, A.X }; }

	/** An angle into (-pi, pi]. */
	inline double WrapAngle(double A)
	{
		A = std::fmod(A, 2.0 * Pi);
		if (A > Pi)
		{
			A -= 2.0 * Pi;
		}
		else if (A <= -Pi)
		{
			A += 2.0 * Pi;
		}
		return A;
	}

	/** A calendar date of T (seconds since 1 January 2026): month * 100 + day, and the weekday (Monday = 0). */
	struct FDate
	{
		int Year = 2026, Month = 1, Day = 1, Weekday = 3;
		int MonthDay() const { return Month * 100 + Day; }
	};

	inline long long FloorDiv(double A, double B)
	{
		return (long long)std::floor(A / B);
	}

	inline FDate DateOf(double T)
	{
		// days since 1970-01-01 (a Thursday); 1 January 2026 is day 20454. Civil date by Howard Hinnant's algorithm.
		const long long Z0 = FloorDiv(T, DaySec) + 20454;
		FDate D;
		D.Weekday = (int)(((Z0 + 3) % 7 + 7) % 7);
		const long long Z = Z0 + 719468;
		const long long Era = (Z >= 0 ? Z : Z - 146096) / 146097;
		const long long Doe = Z - Era * 146097;
		const long long Yoe = (Doe - Doe / 1460 + Doe / 36524 - Doe / 146096) / 365;
		const long long Doy = Doe - (365 * Yoe + Yoe / 4 - Yoe / 100);
		const long long Mp = (5 * Doy + 2) / 153;
		D.Day = (int)(Doy - (153 * Mp + 2) / 5 + 1);
		D.Month = (int)(Mp < 10 ? Mp + 3 : Mp - 9);
		D.Year = (int)(Yoe + Era * 400 + (D.Month <= 2 ? 1 : 0));
		return D;
	}

	/** "HH:MM" as seconds of the day; -1 when it is not a time. */
	inline double ParseClock(const std::string& Text)
	{
		const size_t Colon = Text.find(':');
		if (Colon == std::string::npos || Colon == 0 || Colon + 1 >= Text.size())
		{
			return -1.0;
		}
		const int H = std::atoi(Text.substr(0, Colon).c_str());
		const int M = std::atoi(Text.substr(Colon + 1).c_str());
		return H >= 0 && H <= 24 && M >= 0 && M < 60 ? H * 3600.0 + M * 60.0 : -1.0;
	}

	/** "MM-DD" as month * 100 + day; -1 when it is not a date. */
	inline int ParseMonthDay(const std::string& Text)
	{
		const size_t Dash = Text.find('-');
		if (Dash == std::string::npos || Dash == 0 || Dash + 1 >= Text.size())
		{
			return -1;
		}
		const int M = std::atoi(Text.substr(0, Dash).c_str());
		const int D = std::atoi(Text.substr(Dash + 1).c_str());
		return M >= 1 && M <= 12 && D >= 1 && D <= 31 ? M * 100 + D : -1;
	}

	struct FIsland
	{
		std::string Id, Kind, Name;
		FVec2 At;
		/** The circle that holds all of it, its height over the water, how near its centre the route may come (m). */
		double Radius = 100.0, Height = 20.0, KeepOff = 150.0;
	};

	/** A programme event: on while T0 <= tv < T1 (seconds since the voyage sailed). */
	struct FEvent
	{
		std::string Id, Announce, Name, Where;
		/** 1 Friday, 2 Saturday, 3 Sunday (the day it begins). */
		int Day = 1;
		double T0 = 0.0, T1 = 0.0;
	};

	struct FTicket
	{
		std::string Item, Class;
		/** 0 hold, 1 std, 2 lux. */
		int ClassIndex = 1;
		int Cabin = 0;
		int Price = 0;
	};

	inline int ClassIndexOf(const std::string& Class)
	{
		return Class == "hold" ? 0 : (Class == "lux" ? 2 : 1);
	}
	inline const char* ClassName(int Index)
	{
		return Index <= 0 ? "hold" : (Index >= 2 ? "lux" : "std");
	}

	enum class EStop : uint8_t { Home, Via, Pass, Orbit, Anchor };

	/** A circle of the route's belt. */
	struct FStop
	{
		EStop Kind = EStop::Via;
		FVec2 C;
		double R = 300.0;
		std::string Island, Event;
		int Laps = 0;
		/** A pass: the speed the island is passed at (m/s). */
		double Speed = 1.5;
		// --- built
		/** +1 counter-clockwise on the chart (the heading grows), -1 clockwise. */
		int Dir = 1;
		/** The arc on the circle: the angle it begins at, how far it turns (>= 0), the path length at its beginning. */
		double AngIn = 0.0, Sweep = 0.0, S0 = 0.0;
		/** Index of Event in FModel::Events (-1: none). */
		int EventIndex = -1;
	};

	/** A piece of the path: a straight leg (P0 + U * s) or an arc. */
	struct FSeg
	{
		double S0 = 0.0, Length = 0.0;
		bool bArc = false;
		FVec2 P0, U;
		FVec2 C;
		double R = 0.0, A0 = 0.0;
		int Dir = 1;
		int Stop = -1;
	};

	/** The ship is at path length S with speed V at tv T. */
	struct FFix
	{
		double T = 0.0, S = 0.0, V = 0.0;
	};

	/** Where the ship is. */
	struct FState
	{
		EPhase Phase = EPhase::Laid;
		/** The voyage of this week (VoyageAt) and whether it runs (in season). */
		long long Voyage = 0;
		bool bRuns = false;
		/** Seconds since it sailed (< 0 before). */
		double Tv = 0.0;
		/** 1..3 from the boarding to the landing's end; 0 otherwise. */
		int Day = 0;
		FVec2 Pos;
		double Heading = 0.0, Speed = 0.0, TurnRate = 0.0;
		/** Metres along the route. */
		double S = 0.0;
	};

	/** The hero's own part (saved by the game). */
	struct FHero
	{
		bool bAboard = false;
		int Class = 1;
		int Cabin = 0;
		long long Voyage = -1;
		int Cruises = 0;

		std::string Save() const
		{
			char Buf[96];
			std::snprintf(Buf, sizeof(Buf), "%d %d %d %lld %d", bAboard ? 1 : 0, Class, Cabin, Voyage, Cruises);
			return Buf;
		}
		bool Load(const std::string& Text)
		{
			int A = 0, C = 1, N = 0, Done = 0;
			long long V = -1;
			if (std::sscanf(Text.c_str(), "%d %d %d %lld %d", &A, &C, &N, &V, &Done) != 5)
			{
				return false;
			}
			bAboard = A != 0;
			Class = std::min(2, std::max(0, C));
			Cabin = std::max(0, N);
			Voyage = V;
			Cruises = std::max(0, Done);
			return true;
		}
	};

	class FModel
	{
	public:
		// --- data
		int SeasonFrom = 415, SeasonTo = 1115;
		/** Friday (Monday = 0), 16:00, 18:00, two days at sea, two hours of landing. */
		int Weekday = 4;
		double BoardClock = 16.0 * 3600.0, SailClock = 18.0 * 3600.0;
		double TotalSec = 2.0 * DaySec, LandingSec = 7200.0;
		double SightM = 4000.0;
		/** The speed ramps of a leg (s), the window the turn rate is smoothed over (s), an instant event's length (s). */
		double RampSec = 300.0, TurnWindowSec = 40.0, InstantSec = 600.0;
		/** The wait acts' cap (minutes). */
		int WaitCapMin = 180;
		std::vector<FIsland> Islands;
		std::vector<FEvent> Events;
		std::vector<FTicket> Tickets;
		std::vector<FStop> Stops;
		// --- built
		std::vector<FSeg> Segs;
		std::vector<FFix> Fixes;
		double Length = 0.0;
		std::vector<std::string> FlagNames;
		std::string LastError;

		double BoardSec() const { return SailClock - BoardClock; }

		// --- loading

		bool Load(const std::string& Text)
		{
			LyudJson::FValue Root;
			std::string Error;
			if (!LyudJson::Parse(Text, Root, Error))
			{
				LastError = "cruise.json: " + Error;
				return false;
			}
			const int From = ParseMonthDay(Root["season"]["from"].S());
			const int To = ParseMonthDay(Root["season"]["to"].S());
			if (From < 0 || To < 0 || From >= To)
			{
				LastError = "cruise.json: season needs from < to (MM-DD)";
				return false;
			}
			SeasonFrom = From;
			SeasonTo = To;
			const LyudJson::FValue& V = Root["voyage"];
			Weekday = std::min(6, std::max(0, V["weekday"].I(4)));
			BoardClock = ParseClock(V["board"].S("16:00"));
			SailClock = ParseClock(V["sail"].S("18:00"));
			TotalSec = DaySec * std::max(1, V["days"].I(2));
			LandingSec = 3600.0 * std::max(0.5f, V["landing_hours"].F(2.f));
			if (BoardClock < 0.0 || SailClock <= BoardClock)
			{
				LastError = "cruise.json: voyage board must be before sail (HH:MM)";
				return false;
			}
			SightM = Root["sight_m"].F(4000.f);
			RampSec = Root["ramp_s"].F(300.f);
			WaitCapMin = Root["wait_cap_min"].I(180);
			Islands.clear();
			for (const LyudJson::FValue& I : Root["islands"].Items)
			{
				FIsland L;
				L.Id = I["id"].S();
				L.Kind = I["kind"].S();
				L.Name = I["name"].S();
				L.At = { (double)I["x"].Num, (double)I["y"].Num };
				L.Radius = I["radius"].F(100.f);
				L.Height = I["height"].F(20.f);
				L.KeepOff = I["keep_off"].F((float)L.Radius);
				if (L.Id.empty())
				{
					LastError = "cruise.json: an island without an id";
					return false;
				}
				Islands.push_back(std::move(L));
			}
			Events.clear();
			for (const LyudJson::FValue& E : Root["events"].Items)
			{
				FEvent Ev;
				Ev.Id = E["id"].S();
				Ev.Announce = E["announce"].S();
				Ev.Name = E["name"].S();
				Ev.Where = E["where"].S();
				Ev.Day = E["day"].I(1);
				const double A = ParseClock(E["from"].S());
				const double B = E["to"].IsNull() ? -1.0 : ParseClock(E["to"].S());
				if (Ev.Id.empty() || A < 0.0 || Ev.Day < 1 || Ev.Day > 4)
				{
					LastError = "cruise.json: event " + Ev.Id + " needs an id, a day 1..3 and from HH:MM";
					return false;
				}
				Ev.T0 = (Ev.Day - 1) * DaySec + A - SailClock;
				Ev.T1 = B < 0.0 ? Ev.T0 + InstantSec : (Ev.Day - 1) * DaySec + B - SailClock + (B <= A ? DaySec : 0.0);
				Events.push_back(std::move(Ev));
			}
			std::stable_sort(Events.begin(), Events.end(), [](const FEvent& L, const FEvent& R) { return L.T0 < R.T0; });
			if (Events.size() > 64)
			{
				LastError = "cruise.json: at most 64 events";
				return false;
			}
			Tickets.clear();
			for (const LyudJson::FValue& K : Root["tickets"].Items)
			{
				FTicket Tk;
				Tk.Item = K["item"].S();
				Tk.Class = K["class"].S("std");
				Tk.ClassIndex = ClassIndexOf(Tk.Class);
				Tk.Cabin = K["cabin"].I(0);
				Tk.Price = K["price"].I(0);
				if (!Tk.Item.empty())
				{
					Tickets.push_back(std::move(Tk));
				}
			}
			Stops.clear();
			for (const LyudJson::FValue& R : Root["route"].Items)
			{
				FStop St;
				const std::string Kind = R["kind"].S();
				St.Kind = Kind == "home" ? EStop::Home : (Kind == "pass" ? EStop::Pass : (Kind == "orbit" ? EStop::Orbit : (Kind == "anchor" ? EStop::Anchor : EStop::Via)));
				St.Island = R["island"].S();
				St.Event = R["event"].S();
				St.Laps = std::max(0, R["laps"].I(0));
				St.Speed = R["speed"].F(1.5f);
				St.R = R["r"].F(300.f);
				const LyudJson::FValue& At = R["at"];
				if (At.Type == LyudJson::FValue::Array && At.Size() >= 2)
				{
					St.C = { (double)At[0].Num, (double)At[1].Num };
				}
				else if (const FIsland* Isl = FindIsland(St.Island))
				{
					St.C = Isl->At;
				}
				else
				{
					LastError = "cruise.json: a route stop needs \"at\" or a known \"island\"";
					return false;
				}
				if (St.R < 50.0)
				{
					LastError = "cruise.json: a route stop's r must be 50 m or more";
					return false;
				}
				if (St.Kind != EStop::Home && St.Kind != EStop::Via && FindEvent(St.Event) < 0)
				{
					LastError = "cruise.json: route stop at " + St.Island + ": unknown event " + St.Event;
					return false;
				}
				Stops.push_back(std::move(St));
			}
			if (Stops.size() < 3 || Stops[0].Kind != EStop::Home)
			{
				LastError = "cruise.json: the route needs 3 stops or more and begins with the home stop";
				return false;
			}
			return Build();
		}

		const FIsland* FindIsland(const std::string& Id) const
		{
			for (const FIsland& I : Islands)
			{
				if (I.Id == Id)
				{
					return &I;
				}
			}
			return nullptr;
		}
		/** The first event with that id (a few ids come twice: "wake"); -1 none. */
		int FindEvent(const std::string& Id) const
		{
			for (size_t I = 0; I < Events.size(); ++I)
			{
				if (Events[I].Id == Id)
				{
					return (int)I;
				}
			}
			return -1;
		}
		const FTicket* FindTicket(const std::string& Item) const
		{
			for (const FTicket& K : Tickets)
			{
				if (K.Item == Item)
				{
					return &K;
				}
			}
			return nullptr;
		}
		const FTicket* TicketOfClass(int ClassIndex) const
		{
			for (const FTicket& K : Tickets)
			{
				if (K.ClassIndex == ClassIndex)
				{
					return &K;
				}
			}
			return nullptr;
		}

		// --- the route

		/** The tangent leg from circle A to circle B (each with its turning direction): the unit direction and its length;
		 *  false when one circle is inside the other's reach. */
		static bool Tangent(const FStop& A, const FStop& B, FVec2& U, double& L)
		{
			const FVec2 D = B.C - A.C;
			const double K = B.Dir * B.R - A.Dir * A.R;
			const double DD = Dot(D, D);
			if (DD <= K * K + 1.0)
			{
				return false;
			}
			L = std::sqrt(DD - K * K);
			U = (D * L - Left(D) * K) * (1.0 / DD);
			return true;
		}
		/** Where a ship heading along U touches a circle it turns round in direction Dir. */
		static FVec2 Touch(const FStop& St, const FVec2& U)
		{
			return St.C - Left(U) * (St.Dir * St.R);
		}

		bool Build()
		{
			const size_t N = Stops.size();
			// which way each circle is rounded: the way the route bends at it
			for (size_t I = 0; I < N; ++I)
			{
				Stops[I].EventIndex = FindEvent(Stops[I].Event);
				const FVec2 In = Stops[I].C - Stops[(I + N - 1) % N].C;
				const FVec2 Out = Stops[(I + 1) % N].C - Stops[I].C;
				Stops[I].Dir = Cross(In, Out) >= 0.0 ? 1 : -1;
			}
			std::vector<FVec2> U(N);
			std::vector<double> LegLen(N);
			for (int Pass = 0; Pass < 8; ++Pass)
			{
				for (size_t I = 0; I < N; ++I)
				{
					if (!Tangent(Stops[I], Stops[(I + 1) % N], U[I], LegLen[I]))
					{
						LastError = "cruise.json: route stops " + std::to_string(I) + " and " + std::to_string((I + 1) % N) + " overlap";
						return false;
					}
				}
				bool bFlipped = false;
				for (size_t I = 0; I < N; ++I)
				{
					FStop& St = Stops[I];
					const FVec2 PIn = Touch(St, U[(I + N - 1) % N]) - St.C;
					const FVec2 POut = Touch(St, U[I]) - St.C;
					St.AngIn = std::atan2(PIn.Y, PIn.X);
					double Turn = St.Dir * (std::atan2(POut.Y, POut.X) - St.AngIn);
					Turn = std::fmod(Turn, 2.0 * Pi);
					if (Turn < 0.0)
					{
						Turn += 2.0 * Pi;
					}
					if (Turn > 2.0 * Pi - 1e-9)
					{
						Turn = 0.0;
					}
					if (Turn > 1.5 * Pi && St.Kind != EStop::Orbit && Pass < 7)
					{
						// rounded the wrong way (a near-straight passage): the other side
						St.Dir = -St.Dir;
						bFlipped = true;
					}
					St.Sweep = Turn + 2.0 * Pi * St.Laps;
				}
				if (!bFlipped)
				{
					break;
				}
			}
			// the path begins and ends in the middle of the home circle's arc
			Segs.clear();
			double S = 0.0;
			const auto AddArc = [this, &S](int Stop, double A0, double Sweep)
			{
				const FStop& St = Stops[(size_t)Stop];
				FSeg G;
				G.S0 = S;
				G.Length = St.R * Sweep;
				G.bArc = true;
				G.C = St.C;
				G.R = St.R;
				G.A0 = A0;
				G.Dir = St.Dir;
				G.Stop = Stop;
				Segs.push_back(G);
				S += G.Length;
			};
			const double HomeHalf = Stops[0].Sweep * 0.5;
			Stops[0].S0 = -Stops[0].R * HomeHalf;
			AddArc(0, Stops[0].AngIn + Stops[0].Dir * HomeHalf, HomeHalf);
			for (size_t I = 0; I < N; ++I)
			{
				FSeg G;
				G.S0 = S;
				G.Length = LegLen[I];
				G.P0 = Touch(Stops[I], U[I]);
				G.U = U[I];
				G.Stop = -1;
				Segs.push_back(G);
				S += G.Length;
				const size_t Next = (I + 1) % N;
				if (Next == 0)
				{
					AddArc(0, Stops[0].AngIn, HomeHalf);
				}
				else
				{
					Stops[Next].S0 = S;
					AddArc((int)Next, Stops[Next].AngIn, Stops[Next].Sweep);
				}
			}
			Length = S;

			// the timed fixes: the islands' events are where their circles are
			Fixes.clear();
			Fixes.push_back({ 0.0, 0.0, 0.0 });
			for (size_t I = 1; I < N; ++I)
			{
				const FStop& St = Stops[I];
				if (St.Kind == EStop::Via || St.Kind == EStop::Home)
				{
					continue;
				}
				const FEvent& Ev = Events[(size_t)St.EventIndex];
				const double Arc = St.R * St.Sweep;
				const double Mid = St.S0 + Arc * 0.5;
				const double Dur = Ev.T1 - Ev.T0;
				if (St.Kind == EStop::Pass)
				{
					Fixes.push_back({ Ev.T0, Mid - St.Speed * Dur * 0.5, St.Speed });
					Fixes.push_back({ Ev.T1, Mid + St.Speed * Dur * 0.5, St.Speed });
				}
				else if (St.Kind == EStop::Orbit)
				{
					Fixes.push_back({ Ev.T0, St.S0, Arc / Dur });
					Fixes.push_back({ Ev.T1, St.S0 + Arc, Arc / Dur });
				}
				else
				{
					Fixes.push_back({ Ev.T0, Mid, 0.0 });
					Fixes.push_back({ Ev.T1, Mid, 0.0 });
				}
			}
			Fixes.push_back({ TotalSec, Length, 0.0 });
			for (size_t I = 1; I < Fixes.size(); ++I)
			{
				const FFix& A = Fixes[I - 1];
				const FFix& B = Fixes[I];
				if (B.T <= A.T || B.S < A.S - 1e-6 || CruiseSpeed(A, B) < -1e-9)
				{
					char Buf[200];
					std::snprintf(Buf, sizeof(Buf), "cruise.json: the route's timing does not work between tv %.0f and %.0f (s %.0f -> %.0f)", A.T, B.T, A.S, B.S);
					LastError = Buf;
					return false;
				}
			}
			BuildFlagNames();
			return true;
		}

		/** The ramp of a leg between two fixes (s) and its steady speed (m/s). */
		double RampOf(const FFix& A, const FFix& B) const
		{
			return std::min(RampSec, (B.T - A.T) / 3.0);
		}
		double CruiseSpeed(const FFix& A, const FFix& B) const
		{
			const double Tr = RampOf(A, B);
			return ((B.S - A.S) - (A.V + B.V) * Tr * 0.5) / ((B.T - A.T) - Tr);
		}

		/** Metres along the route and the speed at tv (clamped to the voyage). */
		void Along(double Tv, double& S, double& V) const
		{
			if (Fixes.empty() || Tv <= Fixes.front().T)
			{
				S = 0.0;
				V = 0.0;
				return;
			}
			if (Tv >= Fixes.back().T)
			{
				S = Length;
				V = 0.0;
				return;
			}
			size_t Lo = 0, Hi = Fixes.size() - 1;
			while (Hi - Lo > 1)
			{
				const size_t Mid = (Lo + Hi) / 2;
				(Fixes[Mid].T <= Tv ? Lo : Hi) = Mid;
			}
			const FFix& A = Fixes[Lo];
			const FFix& B = Fixes[Hi];
			const double T = B.T - A.T;
			const double Tr = RampOf(A, B);
			const double Vc = CruiseSpeed(A, B);
			const double X = Tv - A.T;
			const auto Smooth = [](double W) { return W * W * (3.0 - 2.0 * W); };
			const auto Area = [](double W) { return W * W * W - 0.5 * W * W * W * W; };
			if (X <= Tr)
			{
				const double W = X / Tr;
				V = A.V + (Vc - A.V) * Smooth(W);
				S = A.S + A.V * X + (Vc - A.V) * Tr * Area(W);
			}
			else if (X <= T - Tr)
			{
				V = Vc;
				S = A.S + (A.V + Vc) * Tr * 0.5 + Vc * (X - Tr);
			}
			else
			{
				const double X2 = X - (T - Tr);
				const double W = X2 / Tr;
				V = Vc + (B.V - Vc) * Smooth(W);
				S = A.S + (A.V + Vc) * Tr * 0.5 + Vc * (T - 2.0 * Tr) + Vc * X2 + (B.V - Vc) * Tr * Area(W);
			}
		}

		/** The point and the heading at S metres along the route. */
		void PathAt(double S, FVec2& Pos, double& Heading) const
		{
			if (Segs.empty())
			{
				Pos = {};
				Heading = 0.0;
				return;
			}
			S = std::min(Length, std::max(0.0, S));
			size_t Lo = 0, Hi = Segs.size();
			while (Hi - Lo > 1)
			{
				const size_t Mid = (Lo + Hi) / 2;
				(Segs[Mid].S0 <= S ? Lo : Hi) = Mid;
			}
			const FSeg& G = Segs[Lo];
			const double X = std::min(G.Length, S - G.S0);
			if (!G.bArc)
			{
				Pos = G.P0 + G.U * X;
				Heading = std::atan2(G.U.Y, G.U.X);
				return;
			}
			const double A = G.A0 + G.Dir * X / G.R;
			Pos = { G.C.X + G.R * std::cos(A), G.C.Y + G.R * std::sin(A) };
			Heading = WrapAngle(A + G.Dir * Pi * 0.5);
		}

		// --- the calendar

		bool InSeason(double T) const
		{
			const int Md = DateOf(T).MonthDay();
			return Md >= SeasonFrom && Md <= SeasonTo;
		}
		/** When voyage K sails (T). Voyage 0 is the first Friday of 2026 (2 January). */
		double SailTime(long long K) const
		{
			// 1 January 2026 is a Thursday (weekday 3)
			const int First = ((Weekday - 3) % 7 + 7) % 7;
			return (First + 7.0 * (double)K) * DaySec + SailClock;
		}
		/** The voyage whose week T is in: from its boarding to the next one's. */
		long long VoyageAt(double T) const
		{
			return FloorDiv(T - (SailTime(0) - BoardSec()), WeekSec);
		}
		/** Does voyage K sail (its three days are all in the season)? */
		bool VoyageRuns(long long K) const
		{
			const double Sail = SailTime(K);
			return InSeason(Sail) && InSeason(Sail + TotalSec) && DateOf(Sail).MonthDay() < DateOf(Sail + TotalSec).MonthDay();
		}
		/** The next boarding's beginning at or after T (of a voyage that sails); < 0: none in two years. */
		double NextBoarding(double T) const
		{
			for (long long K = VoyageAt(T), End = K + 110; K < End; ++K)
			{
				const double Board = SailTime(K) - BoardSec();
				if (Board >= T && VoyageRuns(K))
				{
					return Board;
				}
			}
			return -1.0;
		}

		FState At(double T) const
		{
			FState St;
			St.Voyage = VoyageAt(T);
			St.bRuns = VoyageRuns(St.Voyage);
			St.Tv = T - SailTime(St.Voyage);
			const bool bOn = St.bRuns && St.Tv < TotalSec + LandingSec;
			if (!bOn)
			{
				St.Phase = InSeason(T) ? EPhase::Port : EPhase::Laid;
				PathAt(0.0, St.Pos, St.Heading);
				return St;
			}
			St.Day = 1 + (int)FloorDiv(St.Tv + SailClock, DaySec);
			double V = 0.0;
			Along(St.Tv, St.S, V);
			PathAt(St.S, St.Pos, St.Heading);
			St.Speed = V;
			if (St.Tv < 0.0)
			{
				St.Phase = EPhase::Boarding;
				return St;
			}
			if (St.Tv >= TotalSec)
			{
				St.Phase = EPhase::Landing;
				return St;
			}
			// the turn rate over a window (the path's own steps at the arcs' ends are never seen as steps)
			const double Half = TurnWindowSec * 0.5;
			double Sa = 0.0, Sb = 0.0, Va = 0.0, Vb = 0.0, Ha = 0.0, Hb = 0.0;
			FVec2 Pa, Pb;
			Along(St.Tv - Half, Sa, Va);
			Along(St.Tv + Half, Sb, Vb);
			PathAt(Sa, Pa, Ha);
			PathAt(Sb, Pb, Hb);
			St.TurnRate = WrapAngle(Hb - Ha) / TurnWindowSec;
			St.Phase = EPhase::Underway;
			for (const FStop& Stop : Stops)
			{
				if (Stop.Kind != EStop::Orbit && Stop.Kind != EStop::Anchor)
				{
					continue;
				}
				const FEvent& Ev = Events[(size_t)Stop.EventIndex];
				if (St.Tv >= Ev.T0 && St.Tv < Ev.T1)
				{
					St.Phase = Stop.Kind == EStop::Orbit ? EPhase::Circling : EPhase::Anchored;
				}
			}
			return St;
		}

		// --- the programme

		/** The events on at T as bits by index (0 when no voyage is on). */
		uint64_t EventsOn(double T) const
		{
			const FState St = At(T);
			return St.Phase == EPhase::Laid || St.Phase == EPhase::Port ? 0 : EventsOnTv(St.Tv);
		}
		uint64_t EventsOnTv(double Tv) const
		{
			uint64_t Bits = 0;
			for (size_t I = 0; I < Events.size(); ++I)
			{
				if (Tv >= Events[I].T0 && Tv < Events[I].T1)
				{
					Bits |= 1ull << I;
				}
			}
			return Bits;
		}
		bool IsEventOn(double T, const std::string& Id) const
		{
			const uint64_t Bits = EventsOn(T);
			for (size_t I = 0; I < Events.size(); ++I)
			{
				if ((Bits >> I & 1ull) && Events[I].Id == Id)
				{
					return true;
				}
			}
			return false;
		}
		/** The next event to begin after T: its index and the seconds to it (of the voyage on now, else the boarding of the next
		 *  voyage that sails); -1: none. */
		int NextEvent(double T, double& Seconds) const
		{
			const FState St = At(T);
			if (St.Phase != EPhase::Laid && St.Phase != EPhase::Port)
			{
				for (size_t I = 0; I < Events.size(); ++I)
				{
					if (Events[I].T0 > St.Tv)
					{
						Seconds = Events[I].T0 - St.Tv;
						return (int)I;
					}
				}
			}
			const double Board = NextBoarding(T + 1.0);
			if (Board < 0.0 || Events.empty())
			{
				Seconds = -1.0;
				return -1;
			}
			// the first event of a voyage is its boarding
			Seconds = Board + (Events[0].T0 + BoardSec()) - T;
			return 0;
		}
		/** The minutes an act that waits for the next event lets pass (to its beginning, rounded up; at most the cap; during the
		 *  landing, to its end); 0: no voyage is on. */
		int WaitMinutes(double T) const
		{
			const FState St = At(T);
			if (St.Phase == EPhase::Laid || St.Phase == EPhase::Port)
			{
				return 0;
			}
			double Left = TotalSec + LandingSec - St.Tv;
			for (const FEvent& Ev : Events)
			{
				if (Ev.T0 > St.Tv)
				{
					Left = Ev.T0 - St.Tv;
					break;
				}
			}
			return std::min(WaitCapMin, std::max(1, (int)std::ceil(Left / 60.0)));
		}
		/** The start (T) of an event of the voyage on at T, or of the next voyage that sails when none is on or it is over
		 *  there; < 0: no such event. */
		double EventTime(double T, const std::string& Id) const
		{
			const int Index = FindEvent(Id);
			if (Index < 0)
			{
				return -1.0;
			}
			const FState St = At(T);
			if (St.bRuns && St.Tv < Events[(size_t)Index].T0)
			{
				return SailTime(St.Voyage) + Events[(size_t)Index].T0;
			}
			const double Board = NextBoarding(T + 1.0);
			return Board < 0.0 ? -1.0 : Board + BoardSec() + Events[(size_t)Index].T0;
		}

		/** Is that island within sight of the ship at T? */
		bool InSight(const FState& St, const FIsland& I) const
		{
			return Len(St.Pos - I.At) <= SightM + I.Radius;
		}

		// --- the hero

		/** May he come aboard at T (the gate's rule)? */
		bool CanBoard(double T, const FHero& H, bool bHasTicket) const
		{
			return !H.bAboard && bHasTicket && At(T).Phase == EPhase::Boarding;
		}
		/** He came aboard at T with that ticket. */
		void Board(FHero& H, double T, const FTicket& Ticket) const
		{
			H.bAboard = true;
			H.Class = Ticket.ClassIndex;
			H.Cabin = Ticket.Cabin;
			H.Voyage = VoyageAt(T);
		}
		/** Is a hero aboard to be put ashore at T: his voyage is over (Sunday 20:00 and later), or is not the one of this week
		 *  (a long sleep, a load, the calendar moved back)? */
		bool MustGoAshore(double T, const FHero& H) const
		{
			if (!H.bAboard)
			{
				return false;
			}
			const FState St = At(T);
			return !(St.Voyage == H.Voyage && St.bRuns && St.Tv < TotalSec + LandingSec);
		}
		/** He is ashore at T; true when he sailed (he was aboard when it left: the voyage counts). */
		bool Ashore(FHero& H, double T) const
		{
			if (!H.bAboard)
			{
				return false;
			}
			const bool bSailed = T >= SailTime(H.Voyage);
			H.bAboard = false;
			H.Cabin = 0;
			H.Cruises += bSailed ? 1 : 0;
			return bSailed;
		}

		// --- the flags (ULyudCityThings::SetFlag; the "flag" condition of the city's lines and acts)

		void BuildFlagNames()
		{
			FlagNames = { "cruise_season", "cruise_voyage", "cruise_boarding", "cruise_landing", "cruise_has_ticket", "cruise_can_board",
				"cruise_aboard", "cruise_day1", "cruise_day2", "cruise_day3", "cruise_underway", "cruise_circling", "cruise_anchor",
				"cruise_port", "cruise_class_hold", "cruise_class_std", "cruise_class_lux", "cruise_night", "cruise_done" };
			EventFlag.assign(Events.size(), 0);
			for (size_t I = 0; I < Events.size(); ++I)
			{
				const std::string Name = "cruise_ev_" + Events[I].Id;
				const auto It = std::find(FlagNames.begin(), FlagNames.end(), Name);
				EventFlag[I] = (size_t)(It - FlagNames.begin());
				if (It == FlagNames.end())
				{
					FlagNames.push_back(Name);
				}
			}
			IslandFlag = FlagNames.size();
			for (const FIsland& I : Islands)
			{
				FlagNames.push_back("cruise_at_" + I.Id);
			}
		}

		/** Every flag of FlagNames at T (1 on, 0 off), in its order. */
		void Flags(double T, const FHero& H, bool bHasTicket, std::vector<uint8_t>& Out) const
		{
			Out.assign(FlagNames.size(), 0);
			const FState St = At(T);
			const bool bVoyage = St.Phase != EPhase::Laid && St.Phase != EPhase::Port;
			const double Clock = std::fmod(std::fmod(T, DaySec) + DaySec, DaySec);
			size_t I = 0;
			Out[I++] = InSeason(T);
			Out[I++] = bVoyage;
			Out[I++] = St.Phase == EPhase::Boarding;
			Out[I++] = St.Phase == EPhase::Landing;
			Out[I++] = bHasTicket;
			Out[I++] = St.Phase == EPhase::Boarding && bHasTicket && !H.bAboard;
			Out[I++] = H.bAboard;
			Out[I++] = St.Day == 1;
			Out[I++] = St.Day == 2;
			Out[I++] = St.Day == 3;
			Out[I++] = St.Phase == EPhase::Underway || St.Phase == EPhase::Circling;
			Out[I++] = St.Phase == EPhase::Circling;
			Out[I++] = St.Phase == EPhase::Anchored;
			Out[I++] = St.Phase == EPhase::Port || St.Phase == EPhase::Boarding || St.Phase == EPhase::Landing;
			Out[I++] = H.bAboard && H.Class == 0;
			Out[I++] = H.bAboard && H.Class == 1;
			Out[I++] = H.bAboard && H.Class == 2;
			Out[I++] = bVoyage && (Clock >= 23.0 * 3600.0 || Clock < 7.0 * 3600.0);
			Out[I++] = H.Cruises > 0;
			if (bVoyage)
			{
				const uint64_t Bits = EventsOnTv(St.Tv);
				for (size_t E = 0; E < Events.size(); ++E)
				{
					if (Bits >> E & 1ull)
					{
						Out[EventFlag[E]] = 1;
					}
				}
			}
			if (St.Phase != EPhase::Laid)
			{
				for (size_t K = 0; K < Islands.size(); ++K)
				{
					Out[IslandFlag + K] = InSight(St, Islands[K]);
				}
			}
		}

	private:
		size_t IslandFlag = 0;
		std::vector<size_t> EventFlag;
	};
}
