// The yacht school's fleet outside the engine: whole training sessions and races sailed by the helmsmen of SailModel.cpp
// under the school's rules (SailSchoolModel.h) through the fleet's brain (SailFleetModel.h), on the real
// Content/City/sailing.json and in the game's gust field (Private/Weather/WindModel.cpp). See run.sh and
// docs/yacht_school.md "Engine: the school".
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "SailFleetModel.h"
#include "WindModel.h"

using namespace LyudSail;

static int Failures = 0, Checks = 0;
static bool bDebug = false;

static void Check(bool Ok, const char* What, double Value)
{
	std::printf("%s %-92s %.3f\n", Ok ? "ok  " : "FAIL", What, Value);
	Checks++;
	if (!Ok) Failures++;
}
static std::string Str(const char* Fmt, ...) __attribute__((format(printf, 1, 2)));
static std::string Str(const char* Fmt, ...)
{
	char Buf[320];
	va_list Args;
	va_start(Args, Fmt);
	std::vsnprintf(Buf, sizeof(Buf), Fmt, Args);
	va_end(Args);
	return Buf;
}

// ---- the game's wind over the water (as tools/sailsim) -----------------------------------------------------------------

struct FWind
{
	LyudWeather::FWindState S;
	double Speed = 0.0, DirX = 1.0, DirY = 0.0;     // at sail height, blows toward
	double DriftX = 0.0, DriftY = 0.0, Clock = 0.0;

	void Set(double InSpeed, double FromDeg, double Gustiness)
	{
		Speed = InSpeed;
		// the school's frame: +X to the sea (compass west), +Y north
		const FSchoolV2 To = SchoolWindTo(FromDeg);
		DirX = To.X;
		DirY = To.Y;
		S.Speed = float(InSpeed);
		S.FromDeg = float(FromDeg);
		S.Gustiness = float(Gustiness);
	}
	void Advance(double Dt)
	{
		DriftX = std::fmod(DriftX + DirX * Speed * Dt + LyudWeather::GustPeriod, LyudWeather::GustPeriod);
		DriftY = std::fmod(DriftY + DirY * Speed * Dt + LyudWeather::GustPeriod, LyudWeather::GustPeriod);
		Clock += Dt;
	}
	void At(double X, double Y, double& Wx, double& Wy) const
	{
		const LyudWeather::FGust G = LyudWeather::GustAt(S, X, Y, DriftX, DriftY, Clock);
		const double C = std::cos(G.Veer), Sn = std::sin(G.Veer);
		Wx = (DirX * C - DirY * Sn) * Speed * G.Factor;
		Wy = (DirX * Sn + DirY * C) * Speed * G.Factor;
	}
};

// ---- the world of a run ------------------------------------------------------------------------------------------------

static FSchoolParams GParams;
static double GTide = 0.0;

static double DepthAt(void*, double X, double Y)
{
	return SchoolDepthAt(GParams, FSchoolV2(X, Y), GTide);
}

struct FSim
{
	FWind Wind;
	std::vector<FBoat> Boats;
	FFleetBrain Brain;
	FLaunch L;
	double T = 0.0;
	int Launched = 0;
	FSchoolV2 LaunchAt, Eye;
	bool bFinite = true;
	int Groundings = 0, Turtles = 0;
	double MaxLaunchSpeed = 0.0;

	void Setup(const std::vector<int>& Sailors, double WindMs, double FromDeg, uint32_t Seed)
	{
		// the school speaks of the mean wind at 10 m; a dinghy's sail is at 2
		Wind.Set(WindMs * LyudWeather::HeightFactor(2.0f, true), FromDeg, 0.15);
		Boats.resize(Sailors.size());
		Brain.Init(GParams, Seed);
		Brain.DepthFn = DepthAt;
		Brain.Units.resize(Sailors.size());
		for (size_t I = 0; I < Sailors.size(); I++)
		{
			const FSchoolSailor& S = GParams.Sailors[(size_t)Sailors[I]];
			const double Age = S.Group == "kids" ? 8.0 + double(I % 4) : (S.Group == "juniors" ? 13.0 + double(I % 4) : 30.0);
			const double Skill = FleetSkill(S, Age, 0.4, 0.0, Seed);
			Init(Boats[I], EBoat(S.Class), CrewMassFor(EBoat(S.Class), Age), Skill, Seed * 31u + (uint32_t)I);
			FFleetUnit& U = Brain.Units[I];
			U.Boat = &Boats[I];
			U.Sailor = Sailors[I];
			HelmInit(U.Helm, Skill, Seed * 7919u + 17u * (uint32_t)I);
		}
		const FSchoolLaunch Lp = SchoolLaunchPoint(GParams, GTide);
		LaunchAt = Lp.At;
		Eye = Lp.At;
		Brain.Launch = &L;
		Brain.SetLaunchPoint(Lp.At);
		Brain.SetWeather(FSchoolV2(Wind.DirX, Wind.DirY), WindMs, GTide, 17.0);
		L = FLaunch();
		L.X = Brain.Home.X;
		L.Y = Brain.Home.Y;
		Brain.LaunchTarget = Brain.Home;
	}

	FEnv EnvAt(double X, double Y) const
	{
		FEnv E;
		Wind.At(X, Y, E.WindX, E.WindY);
		E.Depth = DepthAt(nullptr, X, Y);
		E.WaveHs = 0.03 * Wind.Speed;
		return E;
	}

	/** The next boat is pushed off the trolley at the water's edge: head to wind, then she bears away. */
	void LaunchNext()
	{
		if (Launched >= (int)Boats.size())
		{
			return;
		}
		FBoat& B = Boats[(size_t)Launched];
		const FSchoolV2 At = Brain.PushOff(LaunchAt) + GParams.LaunchDir.Left() * (((Launched % 3) - 1) * 3.0);
		PlaceSailing(B, At.X, At.Y, Brain.PushOffYaw(), 1.0);
		Brain.LaunchUnit(Launched);
		Launched++;
	}

	void Tick(double Dt)
	{
		const int N = (int)Boats.size();
		int Lod[32];
		double Dist[32];
		for (int I = 0; I < N && I < 32; I++)
		{
			FFleetUnit& U = Brain.Units[(size_t)I];
			U.Env = EnvAt(Boats[(size_t)I].X, Boats[(size_t)I].Y);
			Dist[I] = std::hypot(Boats[(size_t)I].X - Eye.X, Boats[(size_t)I].Y - Eye.Y);
			Lod[I] = U.Afloat() ? FleetLodOf(Dist[I], U.Lod) : FleetLodFar;
		}
		FleetLodBudget(Lod, Dist, nullptr, std::min(N, 32), 6);
		for (int I = 0; I < N && I < 32; I++) Brain.Units[(size_t)I].Lod = Lod[I];
		Brain.Think(Dt);
		for (int I = 0; I < N; I++)
		{
			FFleetUnit& U = Brain.Units[(size_t)I];
			FBoat& B = Boats[(size_t)I];
			if (!U.Afloat()) continue;
			if (U.Stage == EFleetStage::Towed)
			{
				StepTowed(B, L.X - std::cos(L.Yaw) * 2.3, L.Y - std::sin(L.Yaw) * 2.3, 6.0, U.Env, Dt);
			}
			else if (U.Lod == FleetLodFar)
			{
				StepFar(B, U.Helm.CourseYaw, U.Helm.SpeedScale, U.Env, Dt);
			}
			else
			{
				Step(B, U.Controls, U.Env, Dt);
			}
			if (B.Events & EvGrounded) Groundings++;
			if (B.Events & EvTurtle) Turtles++;
			bFinite = bFinite && std::isfinite(B.X + B.Y + B.Yaw + B.VX + B.VY + B.Heel);
		}
		double Th = 0.0, Tu = 0.0;
		LaunchDriveTo(L, Brain.LaunchTarget.X, Brain.LaunchTarget.Y, Brain.LaunchCruise, Brain.LaunchRadius, Th, Tu);
		StepLaunch(L, Th, Tu, EnvAt(L.X, L.Y), Dt);
		MaxLaunchSpeed = std::max(MaxLaunchSpeed, L.Speed);
		Brain.After(Dt);
		if (bDebug)
		{
			for (int K = 0; K < Brain.EventCount; K++)
			{
				const FFleetEvent& E = Brain.Events[K];
				if (E.Kind == EFleetSignal::Launched) continue;
				std::printf("   t %6.0f race %6.0f  event %2d unit %2d other %2d value %.2f\n", T, Brain.RaceT, (int)E.Kind, E.Unit, E.Other, E.Value);
			}
		}
		Wind.Advance(Dt);
		T += Dt;
	}

	void Trace(const char* Head) const
	{
		std::printf("  %-10s t %5.0f s  launch %4.0f,%4.0f v %.1f |", Head, T, L.X, L.Y, L.Speed);
		for (size_t I = 0; I < Boats.size(); I++)
		{
			const FBoat& B = Boats[I];
			const FFleetUnit& U = Brain.Units[I];
			std::printf(" %c%d%c %4.0f,%4.0f %.1f%s", BoatId(B.Class)[0], (int)U.Stage, U.Lod == FleetLodFar ? '~' : ' ', B.X, B.Y, B.Out.Sog,
				B.State == EState::Sailing || B.State == EState::Ashore ? "" : Str("(%s)", StateName(B.State)).c_str());
		}
		std::printf("\n");
	}
};

static std::vector<int> SailorsOf(const std::string& Group, int ClassMask)
{
	std::vector<int> Out;
	for (size_t I = 0; I < GParams.Sailors.size(); I++)
	{
		const FSchoolSailor& S = GParams.Sailors[I];
		if ((Group.empty() || S.Group == Group) && (ClassMask >> S.Class & 1)) Out.push_back((int)I);
	}
	return Out;
}

// ---- a training session ------------------------------------------------------------------------------------------------

struct FSessionResult
{
	int Boats = 0, Home = 0, Capsizes = 0, Contacts = 0, Laps = 0, Tows = 0;
	double MaxOutside = 0.0, RecallMin = 0.0, Metres = 0.0;
	bool bFinite = true, bLaid[2] = { true, true };
};

static std::string AreaOfDrill(const std::string& Group, const std::string& Drill)
{
	const FSchoolDrill* D = GParams.Drill(Drill);
	if (D && D->Area != "group" && GParams.Area(D->Area)) return D->Area;
	const auto It = GParams.GroupArea.find(Group);
	return It != GParams.GroupArea.end() && !It->second.empty() ? It->second[0] : std::string("race");
}

static FSessionResult RunSession(const std::string& Group, double WindMs, double FromDeg, const std::string& DrillA, const std::string& DrillB, bool bTrace)
{
	FSessionResult R;
	FSim S;
	S.Setup(SailorsOf(Group, 7), WindMs, FromDeg, 1000u + (uint32_t)FromDeg + (uint32_t)(WindMs * 10.0));
	R.Boats = (int)S.Boats.size();
	const double Dt = 1.0 / 30.0;
	const double BlockS = 2400.0;
	S.Brain.SetDrill(DrillA, AreaOfDrill(Group, DrillA));
	R.bLaid[0] = GParams.Drill(DrillA) == nullptr || GParams.Drill(DrillA)->Course.empty() || S.Brain.DrillCourse.bOk;
	double NextLaunch = 0.0, RecallAt = -1.0;
	int NextTrace = 0, Stage = 0;
	while (S.T < 2.0 * BlockS + 1800.0)
	{
		if (S.T >= NextLaunch && S.Launched < R.Boats && Stage < 2)
		{
			S.LaunchNext();
			NextLaunch = S.T + 7.0;
		}
		if (Stage == 0 && S.T >= BlockS)
		{
			Stage = 1;
			S.Brain.SetDrill(DrillB, AreaOfDrill(Group, DrillB));
			R.bLaid[1] = GParams.Drill(DrillB) == nullptr || GParams.Drill(DrillB)->Course.empty() || S.Brain.DrillCourse.bOk;
		}
		if (Stage == 1 && S.T >= 2.0 * BlockS)
		{
			Stage = 2;
			RecallAt = S.T;
			S.Brain.Recall(true);
		}
		S.Tick(Dt);
		for (int K = 0; K < S.Brain.EventCount; K++) if (S.Brain.Events[K].Kind == EFleetSignal::TowStart) R.Tows++;
		if (Stage == 2 && S.Brain.AllAshore())
		{
			break;
		}
		if (bTrace && int(S.T / 300.0) >= NextTrace)
		{
			NextTrace = int(S.T / 300.0) + 1;
			S.Trace(Stage == 0 ? DrillA.c_str() : (Stage == 1 ? DrillB.c_str() : "recall"));
		}
	}
	R.RecallMin = RecallAt >= 0.0 ? (S.T - RecallAt) / 60.0 : 0.0;
	R.Contacts = S.Brain.Contacts;
	R.bFinite = S.bFinite;
	for (const FFleetUnit& U : S.Brain.Units)
	{
		if (U.Stage == EFleetStage::Landed) R.Home++;
		R.Capsizes += U.Capsizes;
		R.Laps += U.Laps;
		R.Metres += U.Metres;
		R.MaxOutside = std::max(R.MaxOutside, U.OutsideM);
		if (bTrace) std::printf("  %-8s skill %.2f: laps %d capsizes %d bumps %d %.0f m, outside %.1f m, stage %d %s at %.0f,%.0f\n", GParams.Sailors[(size_t)U.Sailor].Slot.c_str(),
			U.Helm.Skill, U.Laps, U.Capsizes, U.Bumps, U.Metres, U.OutsideM, (int)U.Stage, StateName(U.Boat->State), U.Boat->X, U.Boat->Y);
	}
	return R;
}

// ---- a race ------------------------------------------------------------------------------------------------------------

struct FRaceResult
{
	bool bCourse = false, bFinite = true;
	int Boats = 0, Finished = 0, Dsq = 0, Dnf = 0, Dns = 0, Ocs = 0, Contacts = 0, Home = 0, Capsizes = 0, GeneralRecalls = 0, Touches = 0;
	double FirstS = 0.0, LastS = 0.0, LimitS = 0.0, MaxOutside = 0.0, HomeMin = 0.0, LengthM = 0.0, WorstStart = 0.0;
	std::string Winner;
};

static FRaceResult RunRace(double WindMs, double FromDeg, ERaceCourse Kind, int ClassMask, bool bTrace)
{
	FRaceResult R;
	FSim S;
	const uint32_t Seed = 500u + (uint32_t)FromDeg * 3u + (uint32_t)(WindMs * 10.0);
	S.Setup(SailorsOf(std::string(), ClassMask), WindMs, FromDeg, Seed);
	R.Boats = (int)S.Boats.size();
	const bool bKids = ClassMask == (1 << SchoolOptimist);
	const int Class = bKids ? SchoolOptimist : SchoolLuch;
	const FSchoolV2 WindTo(S.Wind.DirX, S.Wind.DirY);
	const FRaceCourse C = RaceSetCourse(GParams, Kind, Class, WindTo, WindMs, R.Boats, RaceSeed(FSchoolDate(2027, 6, 20), 0, 0, Class), GTide);
	R.bCourse = C.bOk;
	R.LengthM = C.LengthM;
	R.LimitS = GParams.TimeLimitS[Class];
	if (!C.bOk)
	{
		return R;
	}
	if (bKids) S.Brain.SetRace(FRaceCourse(), 0, &C, Class); else S.Brain.SetRace(C, ClassMask, nullptr, Class);
	const double Dt = 1.0 / 30.0;
	double NextLaunch = 0.0, HomeAt = -1.0;
	int NextTrace = 0;
	bool bOver = false;
	while (S.T < 3.0 * 3600.0)
	{
		if (S.T >= NextLaunch && S.Launched < R.Boats && !bOver)
		{
			S.LaunchNext();
			NextLaunch = S.T + 6.0;
		}
		S.Tick(Dt);
		for (int K = 0; K < S.Brain.EventCount; K++)
		{
			const FFleetEvent& E = S.Brain.Events[K];
			if (E.Kind == EFleetSignal::RaceOver && !bOver)
			{
				bOver = true;
				HomeAt = S.T;
				// the tables before the trackers are touched again
				for (const FFleetUnit& U : S.Brain.Units)
				{
					const ERaceStatus St = U.Tracker.Status;
					R.Finished += St == ERaceStatus::Finished ? 1 : 0;
					R.Dsq += St == ERaceStatus::Dsq ? 1 : 0;
					R.Dnf += St == ERaceStatus::Dnf ? 1 : 0;
					R.Dns += St == ERaceStatus::Dns ? 1 : 0;
					R.Ocs += St == ERaceStatus::Ocs ? 1 : 0;
					R.Touches += U.Tracker.Touches;
					if (St == ERaceStatus::Finished)
					{
						R.LastS = std::max(R.LastS, U.Tracker.FinishT);
						R.WorstStart = std::max(R.WorstStart, U.Tracker.StartT);
					}
				}
				const std::vector<FRaceFinish>& Res = S.Brain.ResultsOf(Class);
				if (!Res.empty()) { R.Winner = Res[0].Slot; R.FirstS = Res[0].TimeS; }
				if (bTrace)
				{
					for (int Cl = 0; Cl < SchoolBoats; Cl++)
					{
						for (const FRaceFinish& F : S.Brain.ResultsOf(Cl))
						{
							std::printf("  %-9s %-8s %-8s %6.0f s  %.1f points\n", SchoolBoatId(Cl), F.Slot.c_str(), RaceStatusId(F.Status), F.TimeS, F.Points);
						}
					}
				}
				S.Brain.EndRace(true);
			}
		}
		if (bOver && S.Brain.AllAshore())
		{
			break;
		}
		if (bTrace && int(S.T / 240.0) >= NextTrace)
		{
			NextTrace = int(S.T / 240.0) + 1;
			S.Trace(Str("race %+.0f", S.Brain.RaceT).c_str());
		}
	}
	R.HomeMin = HomeAt >= 0.0 ? (S.T - HomeAt) / 60.0 : 999.0;
	R.Contacts = S.Brain.Contacts;
	R.GeneralRecalls = S.Brain.GeneralRecalls;
	R.bFinite = S.bFinite;
	for (const FFleetUnit& U : S.Brain.Units)
	{
		if (U.Stage == EFleetStage::Landed) R.Home++;
		R.Capsizes += U.Capsizes;
		R.MaxOutside = std::max(R.MaxOutside, U.OutsideM);
	}
	return R;
}

// ---- the checks --------------------------------------------------------------------------------------------------------

static void TestSmall()
{
	std::printf("-- skill, LOD, contacts, the statistical session\n");
	double Lo[3] = { 9, 9, 9 }, Hi[3] = { 0, 0, 0 };
	for (const FSchoolSailor& S : GParams.Sailors)
	{
		const int G = S.Group == "kids" ? 0 : (S.Group == "juniors" ? 1 : 2);
		const double A = FleetSkill(S, 0.0, 0.0, 0.0, 77u), B = FleetSkill(S, 0.0, 1.0, 0.12, 77u);
		Lo[G] = std::min(Lo[G], A); Hi[G] = std::max(Hi[G], A);
		if (!(B > A && B - A < 0.21)) { Check(false, "a season and what was learned improve a member (by under 0.21)", B - A); }
	}
	Check(Lo[0] >= 0.11 && Hi[0] <= 0.39, Str("beginners' skill in 0.15-0.35 (+- talent): %.2f..%.2f", Lo[0], Hi[0]).c_str(), Hi[0]);
	Check(Lo[1] >= 0.36 && Hi[1] <= 0.74, Str("juniors' skill in 0.4-0.7: %.2f..%.2f", Lo[1], Hi[1]).c_str(), Hi[1]);
	Check(Lo[2] >= 0.26 && Hi[2] <= 0.84, Str("adults' skill in 0.3-0.8: %.2f..%.2f", Lo[2], Hi[2]).c_str(), Hi[2]);
	Check(FleetSkill(GParams.Sailors[0], 9.0, 0.3, 0.0, 5u) == FleetSkill(GParams.Sailors[0], 9.0, 0.3, 0.0, 5u), "the same member, the same skill", 1.0);

	Check(FleetLodOf(59.0, 2) == 0 && FleetLodOf(65.0, 2) == 1 && FleetLodOf(65.0, 0) == 0 && FleetLodOf(75.0, 0) == 1, "near to 60 m, kept to 72 m", 1.0);
	Check(FleetLodOf(249.0, 2) == 1 && FleetLodOf(265.0, 2) == 2 && FleetLodOf(265.0, 1) == 1 && FleetLodOf(285.0, 1) == 2, "mid to 250 m, kept to 280 m", 1.0);
	{
		int Lod[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		const double Dist[8] = { 5, 50, 10, 40, 20, 30, 45, 55 };
		const bool Pin[8] = { false, false, false, false, false, false, false, true };
		FleetLodBudget(Lod, Dist, Pin, 8, 6);
		int Near = 0;
		for (int I = 0; I < 8; I++) Near += Lod[I] == 0;
		Check(Near == 6 && Lod[7] == 0 && Lod[1] == 1 && Lod[6] == 1, "the near budget drops the farthest, never the pinned boat", Near);
	}
	{
		FBoat A, B;
		Init(A, EBoat::Luch, 0.0, 0.5, 1);
		Init(B, EBoat::Optimist, 0.0, 0.5, 2);
		PlaceSailing(A, 0.0, 0.0, 0.0, 2.0);
		PlaceSailing(B, 2.0, 0.3, Pi * 0.5, 0.0);
		A.VX = 2.0;
		const double Hit = FleetBump(A, B);
		FSchoolV2 A0, A1, B0, B1, Pa, Pb;
		double Ra, Rb;
		FleetHull(A, A0, A1, Ra);
		FleetHull(B, B0, B1, Rb);
		const double Gap = FleetSegSeg(A0, A1, B0, B1, Pa, Pb) - Ra - Rb;
		Check(Hit > 1.0 && Gap > -1e-6 && A.VX < 1.5 && B.VX > 0.3, "a luch running into an optimist: apart again, way shared", Gap);
		FBoat C;
		Init(C, EBoat::Luch, 0.0, 0.5, 3);
		PlaceSailing(C, 50.0, 0.0, 0.0, 1.0);
		Check(FleetBump(A, C) == 0.0, "boats 50 m apart do not touch", 0.0);
		FFleetObstacle Pier;
		Pier.A = FSchoolV2(-10.0, 3.0); Pier.B = FSchoolV2(10.0, 3.0); Pier.R = 1.0;
		PlaceSailing(C, 0.0, 1.0, Pi * 0.5, 1.5);
		C.VY = 1.5;
		const double HitP = FleetBumpFixed(C, Pier);
		Check(HitP > 1.0 && C.VY <= 0.01 && C.Y < 1.0, "a boat into the pier: stopped and pushed out", C.Y);
	}
	{
		int Light = 0, Blow = 0;
		double M = 0.0;
		for (const FSchoolSailor& S : GParams.Sailors)
		{
			const double Sk = FleetSkill(S, 0.0, 0.5, 0.0, 9u);
			const double W = GParams.Wind[S.Class][1];
			Light += FleetResolveSession(GParams, 42u, S, Sk, 0.4 * W, 2.0).Capsizes;
			Blow += FleetResolveSession(GParams, 42u, S, Sk, 0.95 * W, 2.0).Capsizes;
			M += FleetResolveSession(GParams, 42u, S, Sk, 5.0, 2.0).Metres;
		}
		const FFleetSessionStat A = FleetResolveSession(GParams, 42u, GParams.Sailors[3], 0.3, 7.0, 2.0), B = FleetResolveSession(GParams, 42u, GParams.Sailors[3], 0.3, 7.0, 2.0);
		Check(Light <= 6 && Blow > Light + 6 && A.Capsizes == B.Capsizes && A.Metres == B.Metres, Str("unwatched sessions: %d capsizes in light air, %d near the limit; deterministic", Light, Blow).c_str(), Blow);
		Check(M / (double)GParams.Sailors.size() > 3000.0 && M / (double)GParams.Sailors.size() < 16000.0, "unwatched sessions: metres sailed in two hours at 5 m/s (mean)", M / (double)GParams.Sailors.size());
	}
}

static void TestSessions(bool bQuick)
{
	std::printf("-- training sessions (two drill blocks of 40 min, then the recall): group, wind at the sail, from\n");
	struct FCase { const char* Group; double Wind; const char* A; const char* B; };
	const FCase Cases[] = {
		{ "kids", 3.0, "follow", "triangle" }, { "kids", 5.0, "figure8", "tack_whistle" }, { "kids", 4.0, "capsize", "free" },
		{ "juniors", 4.0, "triangle", "tack_whistle" }, { "juniors", 6.5, "starts", "figure8" },
		{ "adults", 6.0, "tack_whistle", "triangle" }, { "adults", 8.5, "starts", "free" },
	};
	int Run = 0;
	for (size_t K = 0; K < sizeof(Cases) / sizeof(Cases[0]); K++)
	{
		const FCase& C = Cases[K];
		for (int D = 0; D < 8; D += (bQuick ? 4 : 2))
		{
			const double From = 45.0 * D + 22.5 * (double)(K % 2);
			const FSessionResult R = RunSession(C.Group, C.Wind, From, C.A, C.B, false);
			Run++;
			const std::string Name = Str("%s %.1f m/s from %3.0f, %s + %s", C.Group, C.Wind, From, C.A, C.B);
			Check(R.Home == R.Boats && R.RecallMin <= 22.0 && R.bFinite, Str("%s: all %d ashore after the recall (minutes)", Name.c_str(), R.Boats).c_str(), R.RecallMin);
			Check(R.MaxOutside <= 15.0 && R.bLaid[0] && R.bLaid[1], Str("%s: nobody leaves the area (farthest outside, m); buoys laid", Name.c_str()).c_str(), R.MaxOutside);
			Check(R.Contacts <= 3 * R.Boats && R.Laps >= R.Boats / 2 && R.Metres / R.Boats > 1500.0, Str("%s: contacts (laps %d, %.0f m a boat, %d capsizes, %d tows)", Name.c_str(), R.Laps, R.Metres / R.Boats, R.Capsizes, R.Tows).c_str(), R.Contacts);
		}
	}
	Check(Run > 0, "sessions run", Run);
}

static void TestRaces(bool bQuick)
{
	std::printf("-- races of the 12 cadets and luches (and the 8 optimists): wind at the sail, from; all must finish inside the limit\n");
	for (int Wi = 0; Wi < 3; Wi++)
	{
		const double Wind = Wi == 0 ? 3.0 : (Wi == 1 ? 6.0 : 9.0);
		for (int D = 0; D < 8; D += (bQuick ? 3 : 1))
		{
			const double From = 45.0 * D + 10.0;
			const ERaceCourse Kind = (D + Wi) % 2 == 0 ? ERaceCourse::WindwardLeeward : ERaceCourse::Triangle;
			const FRaceResult R = RunRace(Wind, From, Kind, (1 << SchoolCadet) | (1 << SchoolLuch), false);
			const std::string Name = Str("%.0f m/s from %3.0f %s (%.0f m)", Wind, From, Kind == ERaceCourse::Triangle ? "triangle" : "w-l", R.LengthM);
			if (!R.bCourse)
			{
				Check(false, Str("%s: a course fits the race area", Name.c_str()).c_str(), 0.0);
				continue;
			}
			Check(R.Finished == R.Boats && R.FirstS <= R.LimitS && R.bFinite, Str("%s: %d of %d finish (dsq %d dnf %d dns %d ocs %d); first, s", Name.c_str(), R.Finished, R.Boats, R.Dsq, R.Dnf, R.Dns, R.Ocs).c_str(), R.FirstS);
			Check(R.LastS <= R.FirstS + GParams.FinishWindowS && R.WorstStart < 120.0, Str("%s: the last inside the window (worst start %.0f s, %d general recalls); last, s", Name.c_str(), R.WorstStart, R.GeneralRecalls).c_str(), R.LastS);
			Check(R.MaxOutside <= 15.0 && R.Contacts <= 30, Str("%s: nobody leaves the area (outside %.1f m); contacts (touches %d, capsizes %d)", Name.c_str(), R.MaxOutside, R.Touches, R.Capsizes).c_str(), R.Contacts);
			Check(R.Home == R.Boats && R.HomeMin <= 30.0, Str("%s: all ashore after the race (minutes)", Name.c_str()).c_str(), R.HomeMin);
		}
	}
	for (int D = 0; D < 8; D += (bQuick ? 4 : 2))
	{
		const double From = 45.0 * D + 30.0;
		const FRaceResult R = RunRace(4.5, From, ERaceCourse::KidsTriangle, 1 << SchoolOptimist, false);
		const std::string Name = Str("optimists 4.5 m/s from %3.0f (%.0f m)", From, R.LengthM);
		Check(R.bCourse && R.Finished == R.Boats && R.FirstS <= R.LimitS, Str("%s: %d of %d finish (dsq %d dnf %d dns %d); first, s", Name.c_str(), R.Finished, R.Boats, R.Dsq, R.Dnf, R.Dns).c_str(), R.FirstS);
		Check(R.MaxOutside <= 15.0 && R.Home == R.Boats && R.HomeMin <= 30.0, Str("%s: inside the area, all ashore after (minutes)", Name.c_str()).c_str(), R.HomeMin);
	}
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "test";
	bDebug = std::getenv("FLEET_DEBUG") != nullptr;
	std::ifstream In("unreal/Lyudmilovsk/Content/City/sailing.json");
	std::stringstream Buf;
	Buf << In.rdbuf();
	std::string Error;
	if (!In || !GParams.Load(Buf.str(), Error))
	{
		std::printf("FAIL cannot read Content/City/sailing.json: %s\n", Error.c_str());
		return 1;
	}
	GTide = -0.3;
	std::printf("pier %.1f,%.1f - %.1f,%.1f halfw %.1f; launch at D %.0f\n", GParams.PierA.X, GParams.PierA.Y, GParams.PierB.X, GParams.PierB.Y, GParams.PierHalfW, SchoolLaunchPoint(GParams, GTide).D);
	WarmPolars();
	if (Mode == "session")
	{
		const std::string Group = Argc > 2 ? Argv[2] : "kids";
		const double Wind = Argc > 3 ? std::atof(Argv[3]) : 4.0, From = Argc > 4 ? std::atof(Argv[4]) : 250.0;
		const std::string A = Argc > 5 ? Argv[5] : "follow", B = Argc > 6 ? Argv[6] : "triangle";
		const FSessionResult R = RunSession(Group, Wind, From, A, B, true);
		std::printf("boats %d home %d capsizes %d contacts %d laps %d tows %d outside %.1f m recall %.1f min metres/boat %.0f\n", R.Boats, R.Home, R.Capsizes, R.Contacts, R.Laps, R.Tows,
			R.MaxOutside, R.RecallMin, R.Metres / std::max(1, R.Boats));
		return 0;
	}
	if (Mode == "race")
	{
		const double Wind = Argc > 2 ? std::atof(Argv[2]) : 6.0, From = Argc > 3 ? std::atof(Argv[3]) : 250.0;
		const std::string K = Argc > 4 ? Argv[4] : "wl";
		const bool bKids = K == "kids";
		const FRaceResult R = RunRace(Wind, From, bKids ? ERaceCourse::KidsTriangle : (K == "tri" ? ERaceCourse::Triangle : ERaceCourse::WindwardLeeward),
			bKids ? 1 << SchoolOptimist : (1 << SchoolCadet) | (1 << SchoolLuch), true);
		std::printf("course %d length %.0f m boats %d finished %d dsq %d dnf %d dns %d ocs %d first %.0f s last %.0f s limit %.0f contacts %d touches %d capsizes %d outside %.1f home %d in %.1f min, general recalls %d, winner %s\n",
			int(R.bCourse), R.LengthM, R.Boats, R.Finished, R.Dsq, R.Dnf, R.Dns, R.Ocs, R.FirstS, R.LastS, R.LimitS, R.Contacts, R.Touches, R.Capsizes, R.MaxOutside, R.Home, R.HomeMin, R.GeneralRecalls, R.Winner.c_str());
		return 0;
	}
	const bool bQuick = Mode == "quick";
	TestSmall();
	TestSessions(bQuick);
	TestRaces(bQuick);
	std::printf("%d checks, %d failed\n", Checks, Failures);
	return Failures ? 1 : 0;
}
