// The flight home from Europa outside the engine: behaviour checks of Private/Spacecraft/FlightModel.h (docs/spacecraft.md,
// tools/spacecraft/MODEL_NOTES.md) on the real tools/spacecraft/flight.json and Content/Spacecraft/flight.json.
//   tools/spacecraftsim/run.sh              the checks
//   tools/spacecraftsim/run.sh show [step]  the flight as a table
//   tools/spacecraftsim/run.sh steps        the largest change per 1/60 s of every continuous output
//   tools/spacecraftsim/run.sh dump         the built-in numbers as flight.json
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Spacecraft/FlightModel.h"

#include <fstream>
#include <map>
#include <set>
#include <sstream>

using namespace LyudFlight;

static int Failures = 0, Passed = 0;
#define CHECK(Cond, ...) do { if (Cond) { Passed++; } else { std::printf("  FAIL "); std::printf(__VA_ARGS__); std::printf("\n"); Failures++; } } while (0)

static bool ReadFile(const std::string& Path, std::string& Out)
{
	std::ifstream F(Path, std::ios::binary);
	if (!F)
	{
		return false;
	}
	std::stringstream SS;
	SS << F.rdbuf();
	Out = SS.str();
	return true;
}

static std::vector<FRecord> Rack()
{
	std::vector<FRecord> R(3);
	R[0].Id = "record_sc_house";
	R[0].LengthSec = 200.0;
	R[1].Id = "record_sc_waltz";
	R[1].LengthSec = 150.0;
	R[2].Id = "record_sc_short";
	R[2].LengthSec = 40.0;
	return R;
}

/** The hero climbs in, shuts the hatch, sits, buckles. */
static void Board(FFlight& F)
{
	F.bSeated = true;
	F.bBuckled = true;
	F.bHatchShut = true;
	F.bWheelSealed = true;
	F.Advance(0.1);
}

/** The whole checklist and ПУСК. */
static void Launch(FFlight& F)
{
	Board(F);
	for (int S = 0; S < SwitchCount; ++S)
	{
		F.Toggle(S);
		F.Advance(1.0);
	}
	F.Advance(5.0);
	F.Launch();
}

struct FRun
{
	std::vector<FEvent> Events;
	double PhaseAt[PhaseCount] = { -1, -1, -1, -1, -1, -1, -1, -1, -1 }; // the real second (from ПУСК) each phase was first seen
	double Real = 0.0;
	FState Final;
};

/** Flies from where F is to afloat. */
static FRun Fly(FFlight& F, double Dt, double NapAt = -1.0)
{
	FRun R;
	for (const FEvent& E : F.TakeEvents())
	{
		R.Events.push_back(E);
	}
	int Guard = 0;
	while (F.Phase() != EPhase::Afloat && Guard++ < 2000000)
	{
		if (R.PhaseAt[(int)F.Phase()] < 0)
		{
			R.PhaseAt[(int)F.Phase()] = R.Real;
		}
		if (NapAt >= 0.0 && F.Time() >= NapAt && !F.Napping() && F.NapSleptSec() <= 0.0)
		{
			F.StartNap();
		}
		F.Advance(Dt);
		R.Real += Dt;
		for (const FEvent& E : F.TakeEvents())
		{
			R.Events.push_back(E);
		}
	}
	R.PhaseAt[(int)EPhase::Afloat] = R.Real;
	R.Final = F.State();
	return R;
}

static std::string Num(double V)
{
	char B[64];
	std::snprintf(B, sizeof B, "%.10g", V);
	return B;
}

// ------------------------------------------------------------------ dump: the built-in numbers as flight.json

static void Dump()
{
	FConfig C;
	std::string O = "{\n";
	O += " \"_doc\": \"The flight home from Europa in the craft Lastochka (docs/spacecraft.md): every number of the flight model "
		"(Private/Spacecraft/FlightModel.h, namespace LyudFlight; guide: tools/spacecraft/MODEL_NOTES.md). The same numbers are built into the "
		"model as defaults and tools/spacecraftsim/run.sh fails when the two differ: change a number HERE AND in FConfig, then run "
		"python3 tools/spacecraft/flight.py (writes Content/Spacecraft/flight.json) and tools/spacecraftsim/run.sh. Times are flight seconds "
		"from the launch button; a cue with \\\"t\\\": \\\"computed\\\" is found by the model (the blackout from the plasma, the peak load, the island "
		"coming over the horizon). The lamps' and the switches' rules are code: the lists here must match the model's.\",\n";
	O += " \"version\": 1,\n";
	O += " \"phases\": [\"parked\", \"prelaunch\", \"ascent\", \"coast\", \"approach\", \"entry\", \"chute\", \"splash\", \"afloat\"],\n";
	std::string Section;
	FConfig::Scalars(C, [&](const char* S, const char* K, double& V)
	{
		if (Section != S)
		{
			O += Section.empty() ? "" : "\n },\n";
			Section = S;
			O += std::string(" \"") + S + "\": {\n";
		}
		else
		{
			O += ",\n";
		}
		O += std::string("  \"") + K + "\": " + Num(V);
	});
	O += "\n },\n";
	FConfig::Curves(C, [&](const char* K, FCurve& Cv)
	{
		O += std::string(" \"") + K + "\": [";
		for (size_t I = 0; I < Cv.T.size(); ++I)
		{
			O += (I ? ", [" : "[") + Num(Cv.T[I]) + ", " + Num(Cv.V[I]) + "]";
		}
		O += "],\n";
	});
	O += " \"switches\": [";
	for (int S = 0; S < SwitchCount; ++S)
	{
		O += std::string(S ? ", \"" : "\"") + SwitchName(S) + "\"";
	}
	O += "],\n \"lamps\": [\n";
	for (int L = 0; L < LampCount; ++L)
	{
		const FLampDef& D = LampDef(L);
		O += std::string("  {\"id\": \"") + D.Id + "\", \"row\": " + std::to_string(L / 6) + ", \"col\": " + std::to_string(L % 6) + ", \"colour\": \""
			+ D.Colour + "\", \"fault\": " + (D.bFault ? "true" : "false") + ", \"rule\": \"" + D.Rule + "\"}" + (L + 1 < LampCount ? ",\n" : "\n");
	}
	O += " ],\n \"cues\": [\n";
	for (size_t I = 0; I < C.Cues.size(); ++I)
	{
		const FCue& Q = C.Cues[I];
		O += std::string("  {\"kind\": \"") + CueKindName(Q.Kind) + "\", \"id\": \"" + Q.Id + "\", \"t\": " + (Q.bComputed ? "\"computed\"" : Num(Q.T)) + "}"
			+ (I + 1 < C.Cues.size() ? ",\n" : "\n");
	}
	O += " ]\n}\n";
	std::fputs(O.c_str(), stdout);
}

// ------------------------------------------------------------------ show

static void ShowRow(const FProfile& P, double T, const char* Note)
{
	FState S;
	P.Eval(T, Norm(V3(-0.30, 0.75, 0.59)), S);
	std::printf("%6.1f %-8s %8.1f %7.2f %8.1f %6.1f %6.1f %5.2f %4.2f %4.2f %6.0f %5.2f %6.1f %5.2f %7.1f %6.1f %6.1f %6.1f  %s\n", T, PhaseName(P.PhaseAt(T)),
		S.RangeKm, S.SpeedKms, S.AltEuropaKm, S.PlanetDiaDeg, S.EuropaDiaDeg, S.GLoad, S.Shake, S.Plasma, S.HullC, S.Whiteout, S.TrackArcDeg, S.IslandVisible,
		S.PitchDeg, S.YawDeg, S.RollDeg, Deg(std::acos(Clamp(S.PlanetDir.X, -1.0, 1.0))), Note);
}

static void Show(double Step)
{
	FProfile P;
	std::printf("The flight of the «Ласточка», minute by minute (FlightModel.h on its built-in numbers)\n");
	std::printf("     t phase    range_km    km/s  europa_km  plan°  euro°     g shak plas hull_C white  arc°  isle   pitch    yaw   roll  plan_off°\n");
	size_t Q = 0;
	for (double T = 0.0; T <= P.C.TEnd + 1e-9; T += Step)
	{
		while (Q < P.Cues.size() && P.Cues[Q].T < T - 1e-9)
		{
			ShowRow(P, P.Cues[Q].T, (std::string(CueKindName(P.Cues[Q].Kind)) + " " + P.Cues[Q].Id).c_str());
			Q++;
		}
		std::string Note;
		while (Q < P.Cues.size() && P.Cues[Q].T <= T + 1e-9)
		{
			Note += (Note.empty() ? "" : ", ") + std::string(CueKindName(P.Cues[Q].Kind)) + " " + P.Cues[Q].Id;
			Q++;
		}
		ShowRow(P, T, Note.c_str());
	}
	std::printf("\nUnder the canopy: %.0f m at the drogue (%.0f m/s), %.0f m at the main, %.0f m at the whiteout's peak (the real world from there), "
		"%.1f m when the jets fire\n", P.AltChuteM, P.C.VEntryEnd, P.AltMainM, P.AltSwapM, P.AltJetsM);
	for (double T : { 810.0, 815.0, 825.0, 830.0, 840.0, 850.0, 860.0, 870.0, 880.0, 885.0, 886.5, 888.0 })
	{
		std::printf("  t %.1f: %7.1f m, %5.1f m/s down, %.2f g\n", T, P.ChuteAlt(T), P.ChuteV(T), P.Load(T + 1e-6));
	}
	std::printf("Found: blackout %.1f..%.1f s, peak load %.2f g at %.1f s, the island over the horizon at %.1f s, 150 m at %.1f s\n", P.TBlackoutStart,
		P.TBlackoutEnd, P.Load(P.TPeakG), P.TPeakG, P.TIslandSeen, P.TAltLow);
}

// ------------------------------------------------------------------ continuity

static bool NearJolt(const FProfile& P, double T0, double T1)
{
	for (double J : P.Jolts())
	{
		if (T0 <= J + 1.0 && T1 >= J - 1e-9) // the jolt and the second it takes to ring out
		{
			return true;
		}
	}
	return false;
}

/** The most a continuous output may change in 1/60 s (outside the jolts). */
static double Bound(const std::string& N)
{
	static const std::map<std::string, double> B = {
		{ "t", 0.02 }, { "range_km", 0.6 }, { "range_rate_kms", 0.02 }, { "alt_europa_km", 0.8 }, { "speed_kms", 0.02 }, { "alt_sea_m", 600.0 },
		{ "g_load", 0.06 }, { "g.x", 0.06 }, { "g.y", 0.06 }, { "g.z", 0.06 }, { "shake", 0.04 }, { "plasma", 0.004 }, { "scorch", 0.001 }, { "hull_c", 4.0 },
		{ "whiteout", 0.004 }, { "chute_inflation", 0.03 }, { "swing_pitch", 0.6 }, { "swing_roll", 0.4 }, { "chute_yaw", 0.02 },
		{ "planet_dia", 0.012 }, { "europa_dia", 0.12 }, { "europa_visible", 0.01 }, { "roll", 0.5 }, { "pitch", 0.6 }, { "yaw", 0.35 },
		{ "sub_lat", 0.01 }, { "sub_lon_rel", 0.012 }, { "track_arc", 0.01 }, { "island_range_km", 0.35 }, { "island_visible", 0.004 },
		{ "i.press", 1.0 }, { "i.o2", 0.002 }, { "i.co2", 0.002 }, { "i.temp", 0.01 }, { "i.volts", 0.07 }, { "i.fuel", 0.15 }, { "i.hull01", 0.003 },
		{ "i.gmeter", 0.06 }, { "i.clock", 0.02 }, { "i.globe_lat", 0.01 }, { "i.globe_lon", 0.012 }, { "i.crt_dot_x", 0.001 }, { "i.crt_dot_y", 0.001 },
		{ "i.crt_progress", 0.0001 }, { "rec.pitch", 0.002 }, { "rec.wow", 0.02 },
	};
	const auto It = B.find(N);
	if (It != B.end())
	{
		return It->second;
	}
	return 0.012; // a unit vector's component: under 0.7 degrees a tick
}

static void Steps()
{
	FProfile P;
	const FVec Sun = Norm(V3(-0.30, 0.75, 0.59));
	std::vector<FNamed> A, B;
	std::map<std::string, std::pair<double, double>> Max;
	std::vector<std::string> Order;
	FState S;
	P.Eval(0.0, Sun, S);
	Flatten(S, A);
	const double Dt = 1.0 / 60.0;
	for (int K = 1; K * Dt <= P.C.TEnd; ++K)
	{
		P.Eval(K * Dt, Sun, S);
		Flatten(S, B);
		if (!NearJolt(P, (K - 1) * Dt, K * Dt))
		{
			for (size_t I = 0; I < B.size(); ++I)
			{
				if (B[I].bContinuous)
				{
					auto It = Max.find(B[I].Name);
					if (It == Max.end())
					{
						Order.push_back(B[I].Name);
						It = Max.insert({ B[I].Name, { 0.0, 0.0 } }).first;
					}
					const double D = std::fabs(B[I].V - A[I].V);
					if (D > It->second.first)
					{
						It->second = { D, K * Dt };
					}
				}
			}
		}
		A.swap(B);
	}
	for (const std::string& N : Order)
	{
		std::printf("%-18s %12.6f at %7.2f s   bound %g%s\n", N.c_str(), Max[N].first, Max[N].second, Bound(N), Max[N].first > Bound(N) ? "   OVER" : "");
	}
}

// ------------------------------------------------------------------ the checks

static void CheckData()
{
	std::printf("flight.json against the built-in numbers\n");
	const FConfig Built;
	for (const char* Path : { "tools/spacecraft/flight.json", "unreal/Lyudmilovsk/Content/Spacecraft/flight.json" })
	{
		std::string Text, Err;
		if (!ReadFile(Path, Text))
		{
			CHECK(false, "no %s (tools/spacecraftsim/run.sh dump > tools/spacecraft/flight.json; python3 tools/spacecraft/flight.py)", Path);
			continue;
		}
		// start from a config that is wrong everywhere: every number must come from the file
		FConfig Zero;
		FConfig::Scalars(Zero, [](const char*, const char*, double& V) { V = -12345.0; });
		FConfig::Curves(Zero, [](const char*, FCurve& C) { C.Clear(); });
		Zero.Cues.clear();
		FConfig File = Zero;
		CHECK(File.LoadJson(Text, Err), "%s: %s", Path, Err.c_str());
		std::vector<double> A, B;
		std::vector<std::string> Names;
		FConfig::Scalars(Built, [&](const char* S, const char* K, const double& V) { A.push_back(V); Names.push_back(std::string(S) + "." + K); });
		FConfig::Scalars(File, [&](const char*, const char*, const double& V) { B.push_back(V); });
		for (size_t I = 0; I < A.size(); ++I)
		{
			CHECK(std::fabs(A[I] - B[I]) < 1e-9, "%s: %s is %g, the model has %g", Path, Names[I].c_str(), B[I], A[I]);
		}
		std::vector<const FCurve*> CA, CB;
		std::vector<std::string> CN;
		FConfig::Curves(Built, [&](const char* K, const FCurve& C) { CA.push_back(&C); CN.push_back(K); });
		FConfig::Curves(File, [&](const char*, const FCurve& C) { CB.push_back(&C); });
		for (size_t I = 0; I < CA.size(); ++I)
		{
			CHECK(CA[I]->T.size() == CB[I]->T.size(), "%s: %s has %zu knots, the model %zu", Path, CN[I].c_str(), CB[I]->T.size(), CA[I]->T.size());
			for (size_t K = 0; K < std::min(CA[I]->T.size(), CB[I]->T.size()); ++K)
			{
				CHECK(std::fabs(CA[I]->T[K] - CB[I]->T[K]) < 1e-9 && std::fabs(CA[I]->V[K] - CB[I]->V[K]) < 1e-9, "%s: %s knot %zu differs", Path,
					CN[I].c_str(), K);
			}
		}
		CHECK(File.Cues.size() == Built.Cues.size(), "%s: %zu cues, the model has %zu", Path, File.Cues.size(), Built.Cues.size());
		for (size_t I = 0; I < std::min(File.Cues.size(), Built.Cues.size()); ++I)
		{
			const FCue& X = File.Cues[I];
			const FCue& Y = Built.Cues[I];
			CHECK(X.Id == Y.Id && X.Kind == Y.Kind && X.bComputed == Y.bComputed && std::fabs(X.T - Y.T) < 1e-9, "%s: cue %zu (%s) differs from the model's %s",
				Path, I, X.Id.c_str(), Y.Id.c_str());
		}
		// the loader refuses a board that is not the model's
		std::string Bad = Text;
		const size_t At = Bad.find("\"g_high\"");
		if (At != std::string::npos)
		{
			Bad.replace(At, 8, "\"g_huge\"");
			FConfig Tmp;
			CHECK(!Tmp.LoadJson(Bad, Err), "%s: a wrong lamp id was accepted", Path);
		}
		FConfig Tmp;
		CHECK(!Tmp.LoadJson("{", Err), "broken json was accepted");
	}
}

static void CheckProfile()
{
	std::printf("the flight's curves\n");
	const FProfile P;
	const FConfig& C = P.C;
	const FVec Sun = Norm(V3(-0.30, 0.75, 0.59));
	// the contract's table
	CHECK(C.TAscentEnd == 75 && C.TApproach == 600 && C.TEntry == 720 && C.TChute == 810 && C.TSplashPhase == 885 && C.TEnd == 900, "the phase times are the contract's");
	CHECK(C.TSep == 700 && C.TMain == 825 && C.TBurn == 300 && C.BurnDur == 6 && C.NapScale == 8, "the contract's moments");
	const double PhaseT[] = { 0, 74.9, 75, 599.9, 600, 719.9, 720, 809.9, 810, 884.9, 885, 899.9 };
	const EPhase PhaseE[] = { EPhase::Ascent, EPhase::Ascent, EPhase::Coast, EPhase::Coast, EPhase::Approach, EPhase::Approach, EPhase::Entry, EPhase::Entry,
		EPhase::Chute, EPhase::Chute, EPhase::Splash, EPhase::Splash };
	for (int K = 0; K < 12; ++K)
	{
		CHECK(P.PhaseAt(PhaseT[K]) == PhaseE[K], "phase at %.1f is %s", PhaseT[K], PhaseName(P.PhaseAt(PhaseT[K])));
	}
	// the planet grows: 26 degrees at Europa, 150 at the entry, never shrinking, visibly every minute, at a steady relative rate
	CHECK(std::fabs(P.PlanetDiaDeg(0) - 26.0) < 0.01, "the planet is %.3f degrees at Europa", P.PlanetDiaDeg(0));
	CHECK(std::fabs(P.PlanetDiaDeg(C.TEntry) - 150.0) < 0.2, "the planet is %.3f degrees at the entry", P.PlanetDiaDeg(C.TEntry));
	for (double T = 0.5; T <= C.TTouch; T += 0.5)
	{
		CHECK(P.PlanetDiaDeg(T) >= P.PlanetDiaDeg(T - 0.5) - 1e-9, "the planet shrinks at %.1f s", T);
	}
	for (double T = 180.0; T + 60.0 <= C.TEntry; T += 30.0)
	{
		const double Grow = P.PlanetDiaDeg(T + 60.0) / P.PlanetDiaDeg(T);
		CHECK(Grow > 1.12 && Grow < 1.26, "the planet grows %.1f %% in the minute from %.0f s", (Grow - 1.0) * 100.0, T);
	}
	CHECK(P.PlanetDiaDeg(C.TApproach) > 95.0 && P.PlanetDiaDeg(C.TApproach) < 115.0, "the planet is a wall at the approach: %.1f", P.PlanetDiaDeg(C.TApproach));
	// the load
	double Max = 0.0, Mean = 0.0;
	int N = 0;
	for (int W = 0; W < 90; ++W)
	{
		double Hi = 0.0, Lo = 0.0;
		for (double T = W * 10.0; T <= W * 10.0 + 10.0; T += 0.01)
		{
			Hi = std::max(Hi, P.Load(T));
			Lo = std::min(Lo, P.Load(T));
			if (T > 5.0 && T < 70.0)
			{
				Mean += P.Load(T);
				N++;
			}
		}
		Max = std::max(Max, Hi);
		CHECK(Hi <= 4.5 && Lo >= 0.0, "the load is %.2f..%.2f g in the 10 s from %d s", Lo, Hi, W * 10);
	}
	CHECK(std::fabs(Max - 4.2) < 0.05, "the peak load is %.3f g", Max);
	CHECK(std::fabs(Mean / N - 1.6) < 0.05, "the ascent's load is %.3f g", Mean / N);
	CHECK(std::fabs(P.Load(P.TPeakG) - Max) < 1e-3 && P.TPeakG > C.TEntry + 30 && P.TPeakG < C.TChute - 20, "the peak is at %.1f s", P.TPeakG);
	CHECK(std::fabs(P.Load(0.0) - C.EuropaG) < 1e-9, "on the pad the load is Europa's");
	for (double T = C.TAscentEnd; T < C.TSep; T += 1.0)
	{
		const bool bBurn = T >= C.TBurn && T <= C.TBurn + C.BurnDur;
		CHECK(bBurn ? P.Load(T) <= C.GBurn + 1e-9 : P.Load(T) == 0.0, "the coast's load is %.3f at %.0f s", P.Load(T), T);
	}
	CHECK(std::fabs(P.Load(C.TBurn + 3.0) - 0.3) < 1e-9, "the correction burn is %.2f g", P.Load(C.TBurn + 3.0));
	CHECK(P.Load(C.TSep + 0.01) > 0.5 && P.Load(C.TSep - 0.01) == 0.0, "the separation's bang");
	CHECK(P.Load(C.TChute + 0.01) > P.Load(C.TChute - 0.01) + 1.0, "the drogue's jolt");
	CHECK(P.Load(C.TMain + 0.01) > P.Load(C.TMain - 0.01) + 1.0, "the main canopy's jolt");
	CHECK(std::fabs(P.Load(C.TSplashPhase - 10.0) - 1.0) < 0.05, "1 g under the canopy: %.3f", P.Load(C.TSplashPhase - 10.0));
	CHECK(P.Load(C.TTouch + 0.01) > 2.5, "the splash's spike: %.2f", P.Load(C.TTouch + 0.01));
	CHECK(std::fabs(P.Load(C.TEnd) - 1.0) < 0.05, "1 g afloat: %.3f", P.Load(C.TEnd));
	// the entry's bell: up, then down, one hump
	{
		int Turns = 0;
		double Prev = P.Load(C.TEntry), PrevD = 1.0;
		for (double T = C.TEntry + 0.5; T < C.TChute; T += 0.5)
		{
			const double D = P.Load(T) - Prev;
			if (D * PrevD < 0.0 && std::fabs(D) > 1e-6)
			{
				Turns++;
			}
			if (std::fabs(D) > 1e-6)
			{
				PrevD = D;
			}
			Prev = P.Load(T);
		}
		CHECK(Turns == 1, "the entry's load has %d turns (one hump wanted)", Turns);
	}
	// under the canopy
	CHECK(std::fabs(P.ChuteV(C.TSplashPhase - 0.01) - 7.0) < 1.5, "%.2f m/s before the jets", P.ChuteV(C.TSplashPhase - 0.01));
	CHECK(std::fabs(P.ChuteV(C.TTouch - 1e-6) - 2.0) < 0.05, "%.2f m/s at the water", P.ChuteV(C.TTouch - 1e-6));
	CHECK(P.ChuteAlt(C.TTouch) == 0.0 && P.ChuteAlt(C.TTouch - 0.01) < 0.05, "the sea is at 0");
	CHECK(P.AltSwapM > 250.0 && P.AltSwapM < 800.0, "the real world starts %.0f m up", P.AltSwapM);
	CHECK(P.AltChuteM > 1500.0 && P.AltChuteM < 5000.0, "the drogue opens %.0f m up", P.AltChuteM);
	for (double T = C.TChute; T < C.TTouch; T += 0.25)
	{
		const double V = (P.ChuteAlt(T) - P.ChuteAlt(T + 0.25)) / 0.25, Mid = P.ChuteV(T + 0.125);
		CHECK(std::fabs(V - Mid) < 0.02 * Mid + 0.05, "the height and the speed disagree at %.2f s: %.2f against %.2f m/s", T, V, Mid);
		CHECK(P.ChuteV(T + 0.25) <= P.ChuteV(T) + 1e-9, "the descent speeds up at %.2f s", T);
	}
	// the range and its rate agree; the range only falls
	for (double T = 1.0; T < C.TTouch; T += 1.0)
	{
		double R0 = 0.0;
		const double H0 = P.Range(T - 0.01), H1 = P.Range(T + 0.01);
		P.Range(T, &R0);
		CHECK(std::fabs((H1 - H0) / 0.02 - R0) < 0.01 * std::fabs(R0) + 2e-4, "the range and its rate disagree at %.0f s: %.5f against %.5f km/s", T, (H1 - H0) / 0.02, R0);
		CHECK(H1 <= H0 + 1e-9, "the range grows at %.0f s", T);
	}
	// the ascent is its load: 1.6 g less Europa's 0.13 for 75 s
	{
		FState S;
		P.Eval(C.TAscentEnd, Sun, S);
		CHECK(S.SpeedKms > 0.95 && S.SpeedKms < 1.2, "%.3f km/s at the cutoff", S.SpeedKms);
		CHECK(S.AltEuropaKm > 34.0 && S.AltEuropaKm < 46.0, "%.1f km over Europa at the cutoff", S.AltEuropaKm);
		P.Eval(30.0, Sun, S);
		CHECK(std::fabs(S.VSpeedMs - (1.58 - 0.13) * G0 * 28.6) < 40.0, "%.0f m/s up at 30 s", S.VSpeedMs);
	}
	// fire, heat, glass, cloud
	CHECK(P.Plasma(C.TEntry) == 0.0 && P.Plasma(C.TChute) == 0.0 && std::fabs(P.Plasma(C.TPlasmaPeak) - 1.0) < 1e-6, "the plasma's bell");
	CHECK(P.TBlackoutStart > C.TEntry && P.TBlackoutStart < P.TPeakG && P.TPeakG < P.TBlackoutEnd && P.TBlackoutEnd < C.TChute, "blackout %.1f..%.1f round the peak %.1f",
		P.TBlackoutStart, P.TBlackoutEnd, P.TPeakG);
	{
		double HMax = -1e9, HAt = 0.0;
		for (int W = 0; W < 90; ++W)
		{
			bool bGlass = true, bUnit = true;
			for (double T = W * 10.0; T < W * 10.0 + 10.0; T += 0.1)
			{
				if (P.HullC(T) > HMax)
				{
					HMax = P.HullC(T);
					HAt = T;
				}
				bGlass = bGlass && P.Scorch(T + 0.1) >= P.Scorch(T) - 1e-12;
				bUnit = bUnit && P.Plasma(T) >= 0.0 && P.Plasma(T) <= 1.0 && P.Shake(T) >= 0.0 && P.Shake(T) <= 1.0 && P.Whiteout(T) >= 0.0 && P.Whiteout(T) <= 1.0;
			}
			CHECK(bGlass, "the glass clears in the 10 s from %d s", W * 10);
			CHECK(bUnit, "plasma, shake or whiteout out of 0..1 in the 10 s from %d s", W * 10);
		}
		CHECK(std::fabs(HMax - C.HullPeakC) < 2.0 && HAt > C.TPlasmaPeak && HAt < C.TChute, "the heat shield peaks at %.0f C at %.1f s", HMax, HAt);
		CHECK(std::fabs(P.HullC(0.0) - C.HullSpaceC) < 1e-9 && P.HullC(C.TEnd) < 150.0, "cold before, quenched after: %.0f C", P.HullC(C.TEnd));
		CHECK(std::fabs(P.Scorch(C.TEnd) - C.ScorchMax) < 1e-6 && P.Scorch(C.TEntry) == 0.0, "the glass is scorched %.2f", P.Scorch(C.TEnd));
	}
	CHECK(P.Whiteout(C.TWhitePeak) == 1.0 && P.Whiteout(C.TWhiteIn) == 0.0 && P.Whiteout(C.TWhiteOut) == 0.0, "the whiteout peaks at %.0f s", C.TWhitePeak);
	{
		FState A, B;
		P.Eval(C.TWhitePeak - 1e-6, Sun, A);
		P.Eval(C.TWhitePeak, Sun, B);
		CHECK(!A.bRealWorld && B.bRealWorld && A.Whiteout > 0.999999, "the real world starts exactly at the whiteout's peak");
		CHECK(std::fabs(B.AltSeaM - P.AltSwapM) < 1e-6, "and from %.1f m", B.AltSeaM);
	}
	CHECK(P.Shake(P.TPeakG) > C.SkipShake && P.Shake(40.0) < C.SkipShake && P.Shake(400.0) == 0.0, "the shake");
	// the chute
	CHECK(P.ChuteStage(C.TChute - 0.01) == EChute::None && P.ChuteStage(C.TChute) == EChute::Drogue && P.ChuteStage(C.TMain) == EChute::Main, "the chute's stages");
	CHECK(P.ChuteInflation(C.TChute + 3.0) == 1.0 && P.ChuteInflation(C.TMain + 2.0) < 0.5 && P.ChuteInflation(C.TMain + 10.0) == 1.0, "the canopy fills, reefed first");
	// the track: the island is beyond the horizon all the coast, comes over it in the approach, is under the craft at the end
	for (double T = 0.0; T <= C.TApproach; T += 5.0)
	{
		CHECK(P.IslandVisible(T) == 0.0, "the island shows at %.0f s (%.1f over a horizon of %.1f)", T, P.TrackArcDeg(T), P.HorizonDeg(T));
		CHECK(P.TrackArcDeg(T) > P.HorizonDeg(T) + 5.0, "the island is only %.1f degrees beyond the horizon at %.0f s", P.TrackArcDeg(T) - P.HorizonDeg(T), T);
	}
	CHECK(P.TIslandSeen > C.TApproach + 20.0 && P.TIslandSeen < C.TSep - 10.0, "the island comes over the horizon at %.1f s", P.TIslandSeen);
	for (double T = C.TApproach; T < C.TTouch; T += 1.0)
	{
		CHECK(P.IslandVisible(T + 1.0) >= P.IslandVisible(T) - 1e-9, "the island sinks again at %.0f s", T);
		CHECK(P.TrackArcDeg(T + 1.0) <= P.TrackArcDeg(T) + 1e-12, "the craft goes back along its track at %.0f s", T);
	}
	CHECK(P.IslandVisible(C.TEntry) == 1.0 && P.TrackArcDeg(C.TTouch) == 0.0, "the island in full view at the entry, under the craft at the splash");
	// instruments
	for (double T = 0.0; T <= C.TEnd; T += 3.0)
	{
		FState S;
		P.Eval(T, Sun, S);
		const FInstruments& I = S.I;
		CHECK(I.O2Pct > 19.5 && I.O2Pct <= 21.0 && I.Co2Pct >= 0.25 && I.Co2Pct < 1.0, "the air at %.0f s: O2 %.2f, CO2 %.2f", T, I.O2Pct, I.Co2Pct);
		CHECK(I.Volts > 25.0 && I.Volts <= 27.5, "%.2f V at %.0f s", I.Volts, T);
		CHECK(I.TempC > 18.0 && I.TempC < 33.0 && I.FuelPct >= 5.0 && I.FuelPct <= 100.0, "temp %.1f fuel %.1f at %.0f s", I.TempC, I.FuelPct, T);
		CHECK(T < 1.0 || I.FuelPct <= P.C.Fuel.At(T - 0.5) + 1e-9, "the fuel grows at %.0f s", T);
		CHECK(I.CrtDotX > 0.0 && I.CrtDotX < 1.0 && I.CrtDotY > 0.0 && I.CrtDotY < 1.0, "the CRT's dot is off the screen at %.0f s", T);
		const int Sec = (int)T;
		CHECK(I.ClockDigits[0] == 0 && I.ClockDigits[1] == 0 && I.ClockDigits[2] * 10 + I.ClockDigits[3] == Sec / 60 && I.ClockDigits[4] * 10 + I.ClockDigits[5] == Sec % 60,
			"the clock at %.0f s", T);
		CHECK(I.RangeKm >= 0 && I.RangeKm <= 999999 && I.bSpeedMs == (T >= C.TChute) && I.bAltSea == (T >= C.TApproach), "the digits at %.0f s", T);
		CHECK(I.Hull01 >= 0.0 && I.Hull01 <= 1.0 && I.GMeter >= 0.0 && I.GMeter <= 6.0, "the bar and the g-meter at %.0f s", T);
	}
	{
		double TMax = 0.0, TAt = 0.0;
		for (double T = 0.0; T <= C.TEnd; T += 1.0)
		{
			if (C.CabinTemp.At(T) > TMax)
			{
				TMax = P.C.CabinTemp.At(T);
				TAt = T;
			}
		}
		CHECK(TAt > C.TEntry + 40.0 && TAt < C.TMain + 5.0 && TMax > 28.0, "the cabin is warmest (%.1f C) at %.0f s", TMax, TAt);
		CHECK(std::fabs(C.Fuel.At(C.TBurn) - C.Fuel.At(C.TBurn + C.BurnDur)) > 5.0 && C.Fuel.At(230.0) == C.Fuel.At(290.0), "the fuel drops in the burn only");
	}
}

static void CheckGeometry()
{
	std::printf("the sky round the cabin\n");
	const FProfile P;
	const FConfig& C = P.C;
	const FVec Sun = Norm(V3(-0.30, 0.75, 0.59));
	FState S;
	P.Eval(0.0, Sun, S);
	CHECK(std::fabs(S.PlanetDir.X - std::cos(Rad(35.0))) < 1e-9 && std::fabs(S.PlanetDir.Z - std::sin(Rad(35.0))) < 1e-9, "on the pad the planet is 35 degrees up, ahead");
	CHECK(S.EuropaDir.Z < -0.999999 && S.EuropaDiaDeg > 179.9, "on the pad Europa is the ground");
	CHECK(std::fabs(S.G.Z + C.EuropaG) < 1e-9 && S.G.X == 0.0, "on the pad things fall down at 0.13 g");
	CHECK(std::fabs(Dot(S.SunDir, Sun) - 1.0) < 1e-9, "on the pad the cabin frame is the flight frame");
	double PrevE = 1e9;
	for (double T = 0.0; T <= C.TEnd; T += 1.5)
	{
		P.Eval(T, Sun, S);
		const FVec* Units[] = { &S.PlanetDir, &S.EuropaDir, &S.SunDir, &S.IslandDir, &S.PlanetIslandAxis, &S.PlanetNorthAxis, &S.FrameX, &S.FrameY, &S.FrameZ };
		bool bUnit = true;
		for (const FVec* V : Units)
		{
			bUnit = bUnit && std::fabs(Len(*V) - 1.0) < 1e-9;
		}
		CHECK(bUnit, "a direction is not a unit vector at %.1f s", T);
		CHECK(std::fabs(Dot(S.FrameX, S.FrameY)) < 1e-9 && std::fabs(Dot(S.FrameX, S.FrameZ)) < 1e-9 && Len(Sub(Cross(S.FrameX, S.FrameY), S.FrameZ)) < 1e-9,
			"the flight frame is not a right-handed frame at %.1f s", T);
		CHECK(std::fabs(Len(S.G) - S.GLoad) < 1e-9, "the load's vector and its size differ at %.1f s", T);
		CHECK(S.EuropaDiaDeg <= PrevE + 1e-9, "Europa grows at %.1f s", T);
		PrevE = S.EuropaDiaDeg;
		// the island's axis, the north pole and the craft's place agree with the numbers of the track
		CHECK(std::fabs(AngleDeg(S.PlanetIslandAxis, Mul(S.PlanetDir, -1.0)) - S.TrackArcDeg) < 1e-6, "the island's axis is not %.2f degrees from the craft's at %.1f s", S.TrackArcDeg, T);
		CHECK(std::fabs(90.0 - AngleDeg(S.PlanetNorthAxis, Mul(S.PlanetDir, -1.0)) - S.SubLatDeg) < 1e-6, "the latitude under the craft at %.1f s", T);
		CHECK(std::fabs(AngleDeg(S.PlanetNorthAxis, S.PlanetIslandAxis) - (90.0 - C.IslandLatDeg)) < 1e-6, "the island is at latitude 18");
		CHECK(std::fabs(S.IslandRangeKm - C.PlanetRadiusKm * Rad(S.TrackArcDeg)) < 1e-6, "the island's range at %.1f s", T);
		if (T < C.TChute)
		{
			CHECK(S.G.Y == 0.0 && S.G.Z <= 0.0, "the load is not into the couch at %.1f s", T);
		}
		if (T >= C.TLookEnd && T <= C.TTurnStart)
		{
			CHECK(S.EuropaDir.X > 0.9999, "Europa is not in the big porthole at %.1f s (the look back)", T);
		}
		if (T >= C.TTurnEnd && T <= C.TApproach - 10.0)
		{
			CHECK(S.PlanetDir.X > 0.9999, "the planet is not in the big porthole at %.1f s (the coast)", T);
			CHECK(S.EuropaDir.X < (T < 450.0 ? -0.7 : -0.3), "Europa is not astern at %.1f s (x %.2f)", T, S.EuropaDir.X);
		}
		if (T >= C.TApproach + 25.0 && T <= C.TSep)
		{
			// the limb is a little above the middle of the big porthole, ahead
			const double Off = AngleDeg(S.PlanetDir, V3(1.0, 0.0, 0.0));
			CHECK(std::fabs(Off - (0.5 * S.PlanetDiaDeg - (C.ApproachLimbDeg - 90.0))) < 1e-6 && S.PlanetDir.Z < 0.0, "the limb is not in the porthole at %.1f s", T);
			CHECK(S.IslandVisible < 0.5 || S.IslandDir.X > 0.5, "the island is over the horizon but not ahead at %.1f s (x %.2f)", T, S.IslandDir.X);
		}
		if (T >= C.TEntry && T <= C.TChute - 2.0)
		{
			// the heat shield leads: the planet below the couch, the porthole looks back along the track at the limb
			CHECK(S.PlanetDir.Z < -0.3 && S.PlanetDir.X > 0.0, "the entry's attitude at %.1f s", T);
			CHECK(std::fabs(S.YawDeg - 180.0) < 1e-6, "the yaw at %.1f s", T);
		}
		if (T >= C.TChute + 6.0 && T <= C.TTouch)
		{
			CHECK(S.PlanetDir.Z < -0.97, "the cabin does not hang upright at %.1f s (z %.3f)", T, S.PlanetDir.Z);
			CHECK(S.G.Z < 0.0 && std::fabs(S.G.X) < 0.3 * S.GLoad, "under the canopy things fall down at %.1f s", T);
		}
	}
	P.Eval(C.TEnd, Sun, S);
	CHECK(std::fabs(S.SubLatDeg - C.IslandLatDeg) < 1e-6 && std::fabs(S.SubLonRelDeg) < 1e-6, "the splash is at the island: lat %.4f, lon %.4f", S.SubLatDeg, S.SubLonRelDeg);
	CHECK(S.EuropaDiaDeg < 6.0 && S.EuropaDiaDeg > 1.0, "Europa ends as a small disk: %.2f degrees", S.EuropaDiaDeg);
	P.Eval(185.0, Sun, S);
	CHECK(S.EuropaDiaDeg < 20.0 && S.EuropaDiaDeg > 8.0 && S.EuropaDir.X > 0.9999, "Europa at the hero's line (185 s): %.1f degrees, in the porthole", S.EuropaDiaDeg);
	P.Eval(C.TEntry - 5.0, Sun, S);
	const double Bearing = S.IslandBearingDeg;
	CHECK(std::fabs(Bearing - C.ArrivalHeadingDeg) < 8.0, "the craft arrives heading %.1f (wanted about %.0f)", Bearing, C.ArrivalHeadingDeg);
	// the same sky whatever the star: only SunDir changes
	FState S2;
	P.Eval(400.0, Sun, S);
	P.Eval(400.0, V3(0.0, 0.0, 1.0), S2);
	CHECK(S.PlanetDir.X == S2.PlanetDir.X && S.FrameZ.X == S2.SunDir.X && S.FrameZ.Z == S2.SunDir.Z, "the star's direction is turned by the attitude");
	// the CRT's path is one line from Europa round the planet
	double PX = 0.0, PY = 0.0;
	CrtPath(0.0, PX, PY);
	for (int K = 1; K <= 200; ++K)
	{
		double X = 0.0, Y = 0.0;
		CrtPath(K / 200.0, X, Y);
		CHECK(std::hypot(X - PX, Y - PY) < 0.02 && X > 0.02 && X < 0.98 && Y > 0.02 && Y < 0.98, "the CRT's path jumps at %.3f", K / 200.0);
		PX = X;
		PY = Y;
	}
}

static void CheckContinuity()
{
	std::printf("every continuous output moves smoothly (1/60 s), but for the jolts\n");
	const FProfile P;
	const FVec Sun = Norm(V3(-0.30, 0.75, 0.59));
	const double Dt = 1.0 / 60.0;
	std::vector<FNamed> A, B;
	FState S;
	P.Eval(0.0, Sun, S);
	Flatten(S, A);
	std::vector<double> Worst(A.size(), 0.0);
	std::vector<double> WorstAt(A.size(), 0.0);
	const int Per = 600; // a verdict per output per 10 s
	for (int K = 1; K * Dt <= P.C.TEnd + 1e-9; ++K)
	{
		P.Eval(K * Dt, Sun, S);
		Flatten(S, B);
		if (!NearJolt(P, (K - 1) * Dt, K * Dt))
		{
			for (size_t I = 0; I < B.size(); ++I)
			{
				const double D = std::fabs(B[I].V - A[I].V);
				if (B[I].bContinuous && D > Worst[I])
				{
					Worst[I] = D;
					WorstAt[I] = K * Dt;
				}
			}
		}
		if (K % Per == 0)
		{
			for (size_t I = 0; I < B.size(); ++I)
			{
				if (B[I].bContinuous)
				{
					CHECK(Worst[I] <= Bound(B[I].Name), "%s jumps by %g in a tick at %.2f s (bound %g)", B[I].Name, Worst[I], WorstAt[I], Bound(B[I].Name));
					Worst[I] = 0.0;
				}
			}
		}
		A.swap(B);
	}
	// the jolts are jolts
	for (double J : P.Jolts())
	{
		CHECK(std::fabs(P.Load(J + 1e-6) - P.Load(J - 1e-6)) > 0.5, "no jolt at %.0f s", J);
	}
	// the dial's two declared switches
	FState X, Y;
	P.Eval(P.C.TApproach - 1e-6, Sun, X);
	P.Eval(P.C.TApproach, Sun, Y);
	CHECK(!X.I.bAltSea && Y.I.bAltSea && X.VSpeedMs > 0.0 && Y.VSpeedMs < 0.0 && std::fabs(X.RangeRateKms - Y.RangeRateKms) < 1e-6,
		"the alt dial changes from Europa to the sea at the approach");
}

static void CheckFlight()
{
	std::printf("a whole flight at 60 Hz and at 4 Hz\n");
	FFlight A, B;
	Launch(A);
	Launch(B);
	CHECK(A.Phase() == EPhase::Ascent && A.Time() == 0.0, "launched");
	const FRun RA = Fly(A, 1.0 / 60.0), RB = Fly(B, 0.25);
	const double Starts[PhaseCount] = { -1, -1, 0, 75, 600, 720, 810, 885, 900 };
	for (int Ph = (int)EPhase::Ascent; Ph < PhaseCount; ++Ph)
	{
		CHECK(std::fabs(RA.PhaseAt[Ph] - Starts[Ph]) <= 1.0 / 60.0 + 1e-6, "60 Hz: %s starts at %.3f s", PhaseName((EPhase)Ph), RA.PhaseAt[Ph]);
		CHECK(std::fabs(RB.PhaseAt[Ph] - Starts[Ph]) <= 0.25 + 1e-6, "4 Hz: %s starts at %.3f s", PhaseName((EPhase)Ph), RB.PhaseAt[Ph]);
	}
	CHECK(std::fabs(RA.Real - 900.0) < 0.02 && std::fabs(RB.Real - 900.0) < 0.26, "the flight takes %.2f and %.2f s", RA.Real, RB.Real);
	std::vector<FNamed> FA, FB;
	Flatten(RA.Final, FA);
	Flatten(RB.Final, FB);
	for (size_t I = 0; I < FA.size(); ++I)
	{
		const bool bLamp = std::strncmp(FA[I].Name, "lamp.", 5) == 0;
		const double X = bLamp ? (double)((int)FA[I].V & 3) : FA[I].V, Y = bLamp ? (double)((int)FB[I].V & 3) : FB[I].V;
		CHECK(std::fabs(X - Y) <= 1e-6 + (std::strcmp(FA[I].Name, "i.press") == 0 ? 2.0 : 0.0), "the end differs in %s: %g at 60 Hz, %g at 4 Hz", FA[I].Name, X, Y);
	}
	CHECK(RA.Final.Phase == EPhase::Afloat && RA.Final.T == 900.0 && RA.Final.GLoad == 1.0 && RA.Final.bRealWorld && RA.Final.AltSeaM == 0.0, "afloat at the end");
	CHECK(A.FlightsFlown() == 1 && A.CapsuleAfloat() && !A.CraftOnEuropa(), "one flight flown, the capsule on the sea");

	// the cues: each once, in order, the same at both rates
	std::printf("the cues\n");
	const FProfile& P = A.P;
	std::vector<FEvent> CA, CB;
	for (const FEvent& E : RA.Events)
	{
		if (E.Id != "launch" && E.Id != "afloat" && E.Id != "switch_on" && E.Id != "auto.ready")
		{
			CA.push_back(E);
		}
	}
	for (const FEvent& E : RB.Events)
	{
		if (E.Id != "launch" && E.Id != "afloat" && E.Id != "switch_on" && E.Id != "auto.ready")
		{
			CB.push_back(E);
		}
	}
	CHECK(CA.size() == P.Cues.size() && CB.size() == P.Cues.size(), "%zu cues fired at 60 Hz, %zu at 4 Hz, the flight has %zu", CA.size(), CB.size(), P.Cues.size());
	for (size_t I = 0; I < std::min(CA.size(), P.Cues.size()); ++I)
	{
		CHECK(CA[I].Id == P.Cues[I].Id && CA[I].Kind == P.Cues[I].Kind && CA[I].T == P.Cues[I].T, "60 Hz: cue %zu is %s, wanted %s", I, CA[I].Id.c_str(), P.Cues[I].Id.c_str());
		CHECK(I == 0 || CA[I].T >= CA[I - 1].T, "cue %s is before the one before it", CA[I].Id.c_str());
		CHECK(!CA[I].bAsleep, "cue %s says the hero sleeps", CA[I].Id.c_str());
	}
	for (size_t I = 0; I < std::min(CB.size(), P.Cues.size()); ++I)
	{
		CHECK(CB[I].Id == P.Cues[I].Id && CB[I].T == P.Cues[I].T, "4 Hz: cue %zu is %s, wanted %s", I, CB[I].Id.c_str(), P.Cues[I].Id.c_str());
	}
	std::map<std::string, int> Count;
	std::map<std::string, double> When;
	for (const FEvent& E : RA.Events)
	{
		Count[E.Id]++;
		When[E.Id] = E.T;
	}
	const char* Sounds[] = { "launch", "liftoff", "stage_left", "engine_cutoff", "turn_start", "correction_burn_on", "correction_burn_off", "approach_start",
		"module_sep", "entry_interface", "blackout_start", "peak_g", "blackout_end", "drogue", "main_chute", "whiteout", "real_world", "jets", "splash", "afloat" };
	double Prev = -1.0;
	for (const char* Id : Sounds)
	{
		CHECK(Count[Id] == 1, "%s fired %d times", Id, Count[Id]);
		CHECK(When[Id] >= Prev, "%s (%.1f s) is out of order", Id, When[Id]);
		Prev = When[Id];
	}
	const char* Lines[] = { "liftoff", "weightless", "europa_behind", "planet_grows", "ocean", "island", "fire", "chute", "home" };
	Prev = -1.0;
	for (const char* L : Lines)
	{
		const std::string Id = std::string("line.hero.flight_") + L + ".1";
		CHECK(Count[Id] == 1 && When[Id] > Prev, "the hero's line %s: %d times, at %.1f s", L, Count[Id], When[Id]);
		Prev = When[Id];
	}
	const char* Autos[] = { "ready", "launch", "cutoff", "weightless", "burn", "half", "approach", "sep", "entry", "blackout", "chute", "splash", "home" };
	for (const char* L : Autos)
	{
		CHECK(Count[std::string("auto.") + L] == 1, "the voice's auto.%s fired %d times", L, Count[std::string("auto.") + L]);
	}
	// nobody talks over anybody: the hero's lines and the voice keep 3.5 s apart
	{
		double Last = -100.0;
		std::string LastId;
		for (const FCue& Q : P.Cues)
		{
			if (Q.Kind != CueSound)
			{
				CHECK(Q.T - Last >= 3.5 || Last < 0.0, "%s (%.1f s) is on top of %s (%.1f s)", Q.Id.c_str(), Q.T, LastId.c_str(), Last);
				Last = Q.T;
				LastId = Q.Id;
			}
		}
	}
	CHECK(When["line.hero.flight_island.1"] > P.TIslandSeen && When["line.hero.flight_island.1"] < P.C.TSep - 5.0, "the island's line comes when it is in sight");
	CHECK(When["correction_burn_on"] == 300.0 && When["correction_burn_off"] == 306.0 && When["module_sep"] == 700.0 && When["real_world"] == 850.0, "the contract's cue times");

	// the words exist (tools/spacecraft/strings.json is the words' source)
	std::string Text, Err;
	LyudJson::FValue Words;
	if (ReadFile("tools/spacecraft/strings.json", Text) && LyudJson::Parse(Text, Words, Err))
	{
		std::printf("the words the model names\n");
		for (const FCue& Q : P.Cues)
		{
			if (Q.Kind == CueSay)
			{
				CHECK(Words.Find(Q.Id) != nullptr, "no words for %s", Q.Id.c_str());
			}
			else if (Q.Kind == CueAuto)
			{
				CHECK(Words.Find("flight." + Q.Id) != nullptr, "no words for flight.%s", Q.Id.c_str());
			}
		}
		CHECK(Words.Find("flight.auto.ready") && Words.Find("line.hero.flight_afloat.1"), "no words for the two cues outside the timeline");
		for (int L = 0; L < LampCount; ++L)
		{
			CHECK(Words.Find(std::string("flight.lamp.") + LampDef(L).Id) != nullptr, "no words for the lamp %s", LampDef(L).Id);
		}
		for (int Ph = 0; Ph < PhaseCount; ++Ph)
		{
			CHECK(Words.Find(std::string("flight.phase.") + PhaseName((EPhase)Ph)) != nullptr, "no words for the phase %s", PhaseName((EPhase)Ph));
		}
		for (int S = 0; S < SwitchCount; ++S)
		{
			CHECK(Words.Find(std::string("flight.sw.") + SwitchName(S)) && Words.Find(std::string("ui.flight.no.order.") + SwitchName(S)), "no words for the switch %s", SwitchName(S));
		}
		const char* Reasons[] = { "order", "order.launch", "hatch_unsealed", "hatch_open", "not_seated", "not_buckled", "no_records", "entry_player", "hatch_flight",
			"switch_flight", "chute_early", "chute_late", "unbuckle_g", "stand_now", "nap_now", "nap_unbuckled", "swim_early" };
		for (const char* R : Reasons)
		{
			CHECK(Words.Find(std::string("ui.flight.no.") + R) != nullptr, "no words for the reason %s", R);
		}
	}
	else
	{
		std::printf("  (tools/spacecraft/strings.json is not here: the words are not checked)\n");
	}
}

static void CheckChecklist()
{
	std::printf("the checklist\n");
	FFlight F;
	CHECK(F.Phase() == EPhase::Parked && F.CraftOnEuropa() && !F.Powered(), "a cold craft on Europa");
	CHECK(F.Toggle(SwPower) == "ui.flight.no.not_seated" && !F.Switch(SwPower), "no switches from the floor");
	CHECK(F.Launch() == "ui.flight.no.not_seated", "no launch from the floor");
	F.bSeated = true;
	F.Advance(0.1);
	CHECK(F.Phase() == EPhase::Prelaunch, "seated: prelaunch");
	F.TakeEvents();
	// every wrong first switch is refused with what must come first, a buzzer and the caution lamp
	for (int S = 1; S < SwitchCount; ++S)
	{
		const std::string R = F.Toggle(S);
		CHECK(R == std::string("ui.flight.no.order.") + SwitchName(S) && !F.Switch(S), "%s before power: '%s'", SwitchName(S), R.c_str());
		const std::vector<FEvent> E = F.TakeEvents();
		CHECK(E.size() == 1 && E[0].Id == "buzzer" && E[0].Arg == R, "the buzzer for %s", SwitchName(S));
	}
	CHECK(F.Launch() == "ui.flight.no.order.launch", "the button is dead");
	F.TakeEvents();
	FState S0 = F.State();
	CHECK(!S0.Lamps[LampMasterCaution].bLit && !S0.I.bPowered, "no lamp without power");
	CHECK(F.Toggle(SwPower).empty() && F.Powered(), "power on");
	S0 = F.State();
	int Lit = 0;
	for (const FLampState& L : S0.Lamps)
	{
		Lit += L.bLit ? 1 : 0;
	}
	CHECK(S0.I.bLampTest && Lit == LampCount, "the lamp test: %d bulbs", Lit);
	F.Advance(3.1);
	S0 = F.State();
	CHECK(!S0.I.bLampTest && S0.Lamps[LampPower].bLit && S0.Lamps[LampComm].bOn && !S0.Lamps[LampAir].bOn, "after the test: power and comm");
	CHECK(!S0.Lamps[LampMasterCaution].bOn, "the caution has gone out after 3 s");
	CHECK(F.Toggle(SwGyro) == "ui.flight.no.order.gyro", "gyros before air");
	S0 = F.State();
	CHECK(S0.Lamps[LampMasterCaution].bOn && S0.Lamps[LampMasterCaution].bBlink, "the caution lamp blinks after a refusal");
	F.Advance(3.1);
	CHECK(!F.State().Lamps[LampMasterCaution].bOn, "and goes out");
	CHECK(F.Toggle(SwAir).empty(), "air on");
	S0 = F.State();
	CHECK(S0.Lamps[LampAir].bOn && S0.Lamps[LampAir].bBlink && S0.I.PressMmHg < 1.0, "the air lamp blinks: the hatch is open, no pressure");
	CHECK(F.Toggle(SwGyro).empty(), "gyros on");
	CHECK(F.State().Lamps[LampGyro].bBlink, "the gyros spin up");
	F.Advance(4.1);
	CHECK(F.State().Lamps[LampGyro].bOn && !F.State().Lamps[LampGyro].bBlink, "the gyros are up");
	CHECK(F.Toggle(SwSeal) == "ui.flight.no.hatch_open" && !F.Switch(SwSeal), "the seal with the hatch open");
	F.bHatchShut = true;
	CHECK(F.Toggle(SwSeal) == "ui.flight.no.hatch_unsealed", "the seal before the wheel");
	F.bWheelSealed = true;
	CHECK(F.CanOpenHatch().empty(), "the hatch may still be opened");
	CHECK(F.Toggle(SwSeal).empty() && F.State().Lamps[LampHatch].bOn, "sealed");
	CHECK(F.CanOpenHatch() == "ui.flight.no.order", "the wheel stays while the seal switch is on");
	F.Advance(20.0);
	S0 = F.State();
	CHECK(S0.I.PressMmHg > 750.0 && !S0.Lamps[LampAir].bBlink, "the cabin fills: %.0f mmHg", S0.I.PressMmHg);
	// off again, in reverse only
	CHECK(F.Toggle(SwPower) == "ui.flight.no.order.power" && F.Powered(), "power goes off last");
	CHECK(F.Toggle(SwAir) == "ui.flight.no.order" && F.Switch(SwAir), "air cannot go off under the gyros");
	CHECK(F.Toggle(SwSeal).empty() && !F.Switch(SwSeal), "the seal off again");
	CHECK(F.Toggle(SwSeal).empty(), "and on");
	F.TakeEvents();
	CHECK(F.Toggle(SwArm).empty(), "armed");
	{
		const std::vector<FEvent> E = F.TakeEvents();
		CHECK(E.size() == 2 && E[0].Id == "switch_on" && E[0].Arg == "arm" && E[1].Id == "auto.ready" && E[1].Kind == CueAuto, "the voice says ready");
	}
	S0 = F.State();
	CHECK(S0.Lamps[LampArmed].bOn && S0.Lamps[LampArmed].bBlink, "the armed lamp blinks");
	F.bBuckled = false;
	CHECK(F.Launch() == "ui.flight.no.not_buckled" && !F.Flying(), "no launch unbuckled");
	F.bBuckled = true;
	CHECK(F.StartNap() == "ui.flight.no.nap_now", "no nap on the pad");
	CHECK(F.PullChuteHandle() == "ui.flight.no.chute_early", "the red handle on the pad");
	CHECK(F.Launch().empty() && F.Flying() && F.Phase() == EPhase::Ascent, "ПУСК");
	{
		const std::vector<FEvent> E = F.TakeEvents();
		CHECK(E.size() == 3 && E[0].Id == "launch" && E[1].Id == "liftoff" && E[2].Id == "auto.launch", "launch, liftoff, the voice");
	}
	for (int S = 0; S < SwitchCount; ++S)
	{
		CHECK(F.Toggle(S) == "ui.flight.no.switch_flight" && F.Switch(S), "%s is locked in flight", SwitchName(S));
	}
	CHECK(F.Launch() == "ui.flight.no.switch_flight", "the button is locked in flight");
	CHECK(F.CanOpenHatch() == "ui.flight.no.hatch_flight" && F.CanSwimOut() == "ui.flight.no.swim_early", "the hatch in flight");
	F.Advance(10.0);
	CHECK(F.CanUnbuckle() == "ui.flight.no.unbuckle_g" && F.CanStand() == "ui.flight.no.stand_now", "pressed into the couch");
	CHECK(!F.State().Lamps[LampArmed].bOn && F.State().Lamps[LampEngine].bOn && F.State().Lamps[LampRecord].bOn, "armed out, engine and recorder on");
	F.Advance(80.0);
	CHECK(F.CanUnbuckle().empty() && F.CanStand().empty(), "weightless: free to move");
	F.DevJump(302.0);
	CHECK(F.CanStand() == "ui.flight.no.stand_now", "not during the burn");
	F.DevJump(815.0);
	CHECK(F.PullChuteHandle() == "ui.flight.no.chute_late" && F.Phase() == EPhase::Chute, "the red handle, late");
	// the comforts
	const bool bLight = F.State().I.bCabinLight;
	F.ToggleLight();
	CHECK(F.State().I.bCabinLight != bLight, "the cabin light");
	F.ToggleRadio();
	CHECK(F.State().I.bRadio, "the radio");
	F.NextCrtMode();
	F.NextCrtMode();
	CHECK(F.State().I.CrtMode == 2, "the CRT's mode");
	F.NextCrtMode();
	CHECK(F.State().I.CrtMode == 0, "and round");
	// every order of two switches from cold: only the next in line goes on
	for (int Mask = 0; Mask < SwitchCount; ++Mask)
	{
		for (int S = 0; S < SwitchCount; ++S)
		{
			FFlight G;
			G.bSeated = G.bBuckled = G.bHatchShut = G.bWheelSealed = true;
			for (int K = 0; K < Mask; ++K)
			{
				G.Toggle(K);
			}
			const bool bWas = G.Switch(S);
			const std::string R = G.Toggle(S);
			const bool bMay = bWas ? S == Mask - 1 : S == Mask;
			CHECK(R.empty() == bMay && G.Switch(S) == (bMay ? !bWas : bWas), "%d on, then %s: '%s'", Mask, SwitchName(S), R.c_str());
		}
	}
}

static void CheckNap()
{
	std::printf("the nap\n");
	FFlight F;
	F.SetRecords(Rack());
	Launch(F);
	F.Advance(30.0);
	CHECK(F.StartNap() == "ui.flight.no.nap_now", "no nap in the ascent");
	F.Advance(70.0);
	F.bBuckled = false;
	CHECK(F.StartNap() == "ui.flight.no.nap_unbuckled", "no nap unbuckled");
	F.bBuckled = true;
	F.TakeEvents();
	CHECK(F.StartNap().empty() && F.Napping(), "asleep at %.1f s", F.Time());
	const double T0 = F.Time();
	CHECK(F.State().TimeScale == 8.0, "time runs 8x");
	F.Advance(10.0);
	CHECK(std::fabs(F.Time() - (T0 + 80.0)) < 1e-9, "10 s asleep are 80 of the flight");
	CHECK(F.CanStand() == "ui.flight.no.stand_now", "not up while asleep");
	F.Advance(2.0);
	F.EndNap();
	CHECK(!F.Napping() && std::fabs(F.NapSleptSec() - 96.0) < 1e-9 && F.State().TimeScale == 1.0, "woken by hand");
	{
		std::vector<FEvent> E = F.TakeEvents();
		bool bSaid = false, bStart = false, bEnd = false;
		for (const FEvent& X : E)
		{
			bSaid = bSaid || X.Kind == CueSay;
			bStart = bStart || X.Id == "nap_start";
			bEnd = bEnd || X.Id == "nap_end";
			CHECK(X.Id == "nap_end" || X.bAsleep, "%s is not marked asleep", X.Id.c_str());
		}
		CHECK(bStart && bEnd && !bSaid, "nap_start, nap_end, and the hero said nothing in his sleep (europa_behind was due)");
	}
	F.DevJump(292.0);
	CHECK(F.StartNap() == "ui.flight.no.nap_now", "no falling asleep into the burn");
	F.DevJump(320.0);
	CHECK(F.StartNap().empty(), "asleep after the burn");
	// it ends by itself 20 s before the approach, exactly, at any tick
	for (double Dt : { 1.0 / 60.0, 0.25, 1.0, 50.0 })
	{
		FFlight G;
		Launch(G);
		G.DevJump(320.0);
		G.StartNap();
		G.TakeEvents();
		double Real = 0.0;
		while (G.Napping() && Real < 100.0)
		{
			G.Advance(Dt);
			Real += Dt;
		}
		const double Woke = G.Time() - (Real - 260.0 / 8.0); // the flight second he woke: the rest of the last tick ran at 1x
		CHECK(std::fabs(Woke - 580.0) < 1e-6 && Real >= 32.5 - 1e-9 && Real < 32.5 + Dt + 1e-9, "tick %.3f: woke at %.4f after %.2f s", Dt, Woke, Real);
		CHECK(G.Phase() == EPhase::Coast || Dt > 20.0, "awake before the approach");
		int Ends = 0, Wakes = 0, Says = 0;
		for (const FEvent& X : G.TakeEvents())
		{
			Ends += X.Id == "nap_end";
			Wakes += X.Id == "nap_wake";
			Says += X.Kind == CueSay;
		}
		CHECK(Ends == 1 && Wakes == 1 && Says == 0, "tick %.3f: %d nap_end, %d nap_wake, %d lines said asleep", Dt, Ends, Wakes, Says);
		CHECK(std::fabs(G.NapSleptSec() - 260.0) < 1e-6, "slept %.1f flight seconds", G.NapSleptSec());
	}
	F.DevJump(561.0);
	CHECK(F.StartNap() == "ui.flight.no.nap_now", "too late to sleep");
	F.DevJump(650.0);
	CHECK(F.StartNap() == "ui.flight.no.nap_now", "no nap in the approach");
	// a flight slept through arrives the same, sooner
	FFlight A, B;
	Launch(A);
	Launch(B);
	const FRun RA = Fly(A, 1.0 / 60.0), RB = Fly(B, 1.0 / 60.0, 100.0);
	CHECK(RB.Real < RA.Real - 400.0 && RB.Real > 400.0, "the slept flight takes %.1f s", RB.Real);
	std::vector<FNamed> FA, FB;
	Flatten(RA.Final, FA);
	Flatten(RB.Final, FB);
	int Diff = 0;
	for (size_t I = 0; I < FA.size(); ++I)
	{
		Diff += std::strncmp(FA[I].Name, "lamp.", 5) != 0 && std::fabs(FA[I].V - FB[I].V) > (std::strcmp(FA[I].Name, "i.press") == 0 ? 2.0 : 1e-6) ? 1 : 0;
	}
	CHECK(Diff == 0, "the slept flight ends differently in %d outputs", Diff);
	std::set<std::string> Got;
	for (const FEvent& E : RB.Events)
	{
		Got.insert(E.Id);
	}
	CHECK(Got.count("correction_burn_on") && Got.count("auto.half") && !Got.count("line.hero.flight_planet_grows.1") && Got.count("line.hero.flight_island.1"),
		"asleep: the sounds and the voice fire, the hero's lines of those minutes do not");
}

static void CheckRecords()
{
	std::printf("the record player\n");
	FFlight F;
	CHECK(F.RecordNext() == "ui.flight.no.no_records" && F.RecordNeedleDown() == "ui.flight.no.no_records", "no records, no music");
	F.SetRecords(Rack());
	CHECK(F.State().Record.Index == -1 && !F.State().Record.bPlaying, "nothing on the platter");
	CHECK(F.RecordNext().empty(), "the first record");
	FRecordState R = F.State().Record;
	CHECK(R.Index == 0 && R.Id == "record_sc_house" && R.bPlaying && R.PosSec == 0.0 && R.LengthSec == 200.0 && R.Pitch == 1.0 && R.Wow == 0.0, "it plays on the pad");
	F.Advance(12.5);
	CHECK(std::fabs(F.State().Record.PosSec - 12.5) < 1e-9, "12.5 s in");
	F.RecordStop();
	F.Advance(5.0);
	CHECK(!F.State().Record.bPlaying && std::fabs(F.State().Record.PosSec - 12.5) < 1e-9, "the needle is up; the record stays");
	CHECK(F.RecordNeedleDown().empty() && F.State().Record.bPlaying, "the needle down again, from the same place");
	CHECK(F.RecordNext().empty() && F.State().Record.Index == 1 && F.State().Record.PosSec == 0.0, "the next record");
	CHECK(F.RecordNext().empty() && F.State().Record.Index == 2, "the third");
	F.TakeEvents();
	F.Advance(39.0);
	CHECK(F.State().Record.bPlaying, "still playing at 39 s");
	F.Advance(1.5);
	R = F.State().Record;
	CHECK(!R.bPlaying && R.PosSec == 40.0 && R.Index == 2, "the side ends: the needle lifts, nothing follows by itself");
	{
		const std::vector<FEvent> E = F.TakeEvents();
		CHECK(E.size() == 1 && E[0].Id == "record_side_end" && E[0].Arg == "record_sc_short", "record_side_end");
	}
	F.Advance(100.0);
	CHECK(!F.State().Record.bPlaying && F.State().Record.Index == 2, "and stays lifted");
	CHECK(F.RecordNeedleDown().empty() && F.State().Record.PosSec == 0.0, "the same side again from the start");
	CHECK(F.RecordNext().empty() && F.State().Record.Index == 0, "the turn wraps to the first");
	CHECK(F.RecordPut(7) == "ui.flight.no.no_records" && F.RecordPut(1).empty() && F.State().Record.Index == 1, "a record by its number");
	// in flight: the load drags the platter, the shake flutters it
	Launch(F);
	F.RecordPut(0);
	F.Advance(30.0);
	R = F.State().Record;
	CHECK(R.Pitch < 0.995 && R.Pitch > 0.97 && R.Wow > 0.1 && R.Wow < 0.5, "in the ascent: pitch %.4f, wow %.3f", R.Pitch, R.Wow);
	CHECK(R.PosSec < 30.0 && R.PosSec > 29.0, "and it has run %.3f s in 30", R.PosSec);
	F.DevJump(200.0);
	R = F.State().Record;
	CHECK(R.Pitch == 1.0 && R.Wow == 0.0, "weightless: the spring-loaded arm plays true");
	// no skips anywhere but where the cabin shakes over 0.6
	{
		FFlight G;
		G.SetRecords({ FRecord{ "long", 5000.0 } });
		Launch(G);
		G.RecordNext();
		G.TakeEvents();
		int Skips = 0, Bad = 0;
		double First = -1.0, Last = -1.0;
		while (G.Flying())
		{
			G.Advance(1.0 / 60.0);
			for (const FEvent& E : G.TakeEvents())
			{
				if (E.Id == "needle_skip")
				{
					Skips++;
					First = First < 0.0 ? G.Time() : First;
					Last = G.Time();
					Bad += G.P.Shake(std::floor(G.Time() / G.P.C.SkipSlot) * G.P.C.SkipSlot) > G.P.C.SkipShake ? 0 : 1;
				}
			}
		}
		CHECK(Skips >= 8 && Skips < 60 && Bad == 0, "the needle skipped %d times (%d without cause)", Skips, Bad);
		CHECK(First > G.P.C.TEntry && Last < G.P.C.TTouch + 3.0, "from %.1f to %.1f s: the entry, the chutes, the splash", First, Last);
		CHECK(G.State().Record.bPlaying, "and plays on afloat");
		// the same skips at 4 Hz
		FFlight H;
		H.SetRecords({ FRecord{ "long", 5000.0 } });
		Launch(H);
		H.RecordNext();
		H.TakeEvents();
		int Skips4 = 0;
		while (H.Flying())
		{
			H.Advance(0.25);
			for (const FEvent& E : H.TakeEvents())
			{
				Skips4 += E.Id == "needle_skip";
			}
		}
		CHECK(Skips4 == Skips, "%d skips at 4 Hz, %d at 60", Skips4, Skips);
		CHECK(std::fabs(H.State().Record.PosSec - G.State().Record.PosSec) < 0.6, "the needle is at %.2f and %.2f s", H.State().Record.PosSec, G.State().Record.PosSec);
	}
	// not in the entry
	F.DevJump(719.0);
	CHECK(F.RecordNext().empty(), "a record may start in the approach");
	F.Advance(2.0);
	CHECK(F.Phase() == EPhase::Entry && F.State().Record.bPlaying, "and plays into the entry");
	CHECK(F.RecordNext() == "ui.flight.no.entry_player" && F.RecordPut(0) == "ui.flight.no.entry_player", "no new record in the entry");
	F.RecordStop();
	CHECK(F.RecordNeedleDown() == "ui.flight.no.entry_player" && !F.State().Record.bPlaying, "the needle will not stay down in the entry");
	F.DevJump(770.0);
	R = F.State().Record;
	CHECK(R.Pitch < 0.95 && R.Wow > 0.6, "at the peak: pitch %.3f, wow %.2f", R.Pitch, R.Wow);
	F.DevJump(812.0);
	CHECK(F.RecordNext().empty(), "under the canopy it may play again");
	// a new rack keeps the record that is on by its id
	std::vector<FRecord> More = Rack();
	More.insert(More.begin(), FRecord{ "record_sc_new", 100.0 });
	const std::string On = F.State().Record.Id;
	F.SetRecords(More);
	CHECK(F.State().Record.Id == On && F.State().Record.Index == 3 && F.State().Record.bPlaying, "the rack changed: the same record turns");
	F.SetRecords({ FRecord{ "other", 10.0 } });
	CHECK(F.State().Record.Index == -1 && !F.State().Record.bPlaying, "its record taken away: the platter is empty");
}

static void CheckLamps()
{
	std::printf("the lamps\n");
	FFlight F;
	Launch(F);
	bool Seen[LampCount] = {}, Blinked[LampCount] = {}, Dark[LampCount] = {};
	int LitTicks[LampCount] = {}, OnButDark[LampCount] = {}, OffButLit[LampCount] = {};
	int Rule[5] = {}, Tick = 0;
	// the checklist's lamps were on before the launch
	FState S = F.State();
	for (int L = 0; L < LampCount; ++L)
	{
		Seen[L] = S.Lamps[L].bOn;
	}
	FFlight Pad;
	Board(Pad);
	for (int K = 0; K < SwitchCount; ++K)
	{
		Pad.Toggle(K);
		Pad.Advance(0.5);
		S = Pad.State();
		for (int L = 0; L < LampCount; ++L)
		{
			Seen[L] = Seen[L] || S.Lamps[L].bOn;
			Blinked[L] = Blinked[L] || S.Lamps[L].bBlink;
		}
	}
	while (F.Phase() != EPhase::Afloat)
	{
		F.Advance(1.0 / 30.0);
		S = F.State();
		for (int L = 0; L < LampCount; ++L)
		{
			Seen[L] = Seen[L] || S.Lamps[L].bOn;
			Blinked[L] = Blinked[L] || S.Lamps[L].bBlink;
			Dark[L] = Dark[L] || !S.Lamps[L].bOn;
			LitTicks[L] += S.Lamps[L].bLit ? 1 : 0;
			OnButDark[L] += S.Lamps[L].bOn && !S.Lamps[L].bBlink && !S.Lamps[L].bLit ? 1 : 0;
			OffButLit[L] += !S.Lamps[L].bOn && (S.Lamps[L].bLit || S.Lamps[L].bBlink) ? 1 : 0;
		}
		const bool bRed = S.Lamps[LampGHigh].bOn || S.Lamps[LampHullHot].bOn;
		Rule[0] += S.Lamps[LampMasterCaution].bOn == bRed ? 0 : 1;
		Rule[1] += S.Lamps[LampBlackout].bOn == S.bBlackout && S.Lamps[LampComm].bOn == !S.bBlackout ? 0 : 1;
		Rule[2] += S.Lamps[LampZeroG].bOn == (S.T < 720.0 && S.GLoad < 0.02) ? 0 : 1;
		Rule[3] += S.Lamps[LampGHigh].bOn == (S.GLoad > 3.0) ? 0 : 1;
		Rule[4] += S.Lamps[LampDrogue].bOn == (S.Chute == EChute::Drogue) ? 0 : 1;
		if (++Tick % 300 == 0)
		{
			static const char* const Names[5] = { "master_caution follows the red lamps", "comm and blackout follow the blackout", "zero_g follows the load",
				"g_high follows the load", "drogue follows the chute" };
			for (int K = 0; K < 5; ++K)
			{
				CHECK(Rule[K] == 0, "%s: wrong in %d ticks of the 10 s to %.0f s", Names[K], Rule[K], S.T);
				Rule[K] = 0;
			}
		}
	}
	for (int L = 0; L < LampCount; ++L)
	{
		CHECK(OnButDark[L] == 0, "%s was on but dark in %d ticks", LampDef(L).Id, OnButDark[L]);
		CHECK(OffButLit[L] == 0, "%s was off but lit in %d ticks", LampDef(L).Id, OffButLit[L]);
	}
	for (int Aft = 0; Aft < 60; ++Aft)
	{
		F.Advance(0.1);
		S = F.State();
		for (int L = 0; L < LampCount; ++L)
		{
			Seen[L] = Seen[L] || S.Lamps[L].bOn;
			Blinked[L] = Blinked[L] || S.Lamps[L].bBlink;
		}
	}
	// the fault lamps (o2_low, batt_low) never light in a nominal flight; every other one does
	for (int L = 0; L < LampCount; ++L)
	{
		CHECK(Seen[L] == !LampDef(L).bFault, "%s %s in a nominal flight", LampDef(L).Id, Seen[L] ? "lights" : "never lights");
		CHECK(Dark[L] || L == LampPower || L == LampAir || L == LampGyro || L == LampHatch || L == LampRecord, "%s never goes out in the flight", LampDef(L).Id);
		const std::string Col = LampDef(L).Colour;
		CHECK(Col == "green" || Col == "amber" || Col == "red" || Col == "white", "%s has the colour %s", LampDef(L).Id, Col.c_str());
		CHECK(LampOf(LampDef(L).Id) == L, "LampOf(%s)", LampDef(L).Id);
	}
	CHECK(LampDef(LampO2Low).bFault && LampDef(LampBattLow).bFault, "the two fault lamps");
	const int Blinkers[] = { LampAir, LampGyro, LampArmed, LampBurn, LampComm, LampHullHot, LampAltLow, LampBeacon };
	for (int L : Blinkers)
	{
		CHECK(Blinked[L], "%s never blinks", LampDef(L).Id);
	}
	CHECK(LitTicks[LampBeacon] > 100 && LitTicks[LampBeacon] < 300, "the beacon blinks half the time: %d ticks of about 360", LitTicks[LampBeacon]);
	CHECK(S.Lamps[LampSplash].bOn && S.Lamps[LampBeacon].bOn && S.Lamps[LampPower].bOn && !S.Lamps[LampMainChute].bOn && !S.Lamps[LampRecord].bOn, "afloat: splash, beacon");
	// the faults
	FFlight G;
	Launch(G);
	G.Advance(100.0);
	S = G.State();
	CHECK(!S.Lamps[LampO2Low].bOn && !S.Lamps[LampBattLow].bOn && !S.Lamps[LampMasterCaution].bOn, "no fault");
	G.bFaultO2 = true;
	S = G.State();
	CHECK(S.Lamps[LampO2Low].bOn && S.Lamps[LampMasterCaution].bOn && !S.Lamps[LampMasterCaution].bBlink && S.I.O2Pct < 18.0, "the oxygen fault");
	G.bFaultO2 = false;
	G.bFaultBatt = true;
	S = G.State();
	CHECK(S.Lamps[LampBattLow].bOn && S.Lamps[LampMasterCaution].bOn && S.I.Volts < 24.5, "the battery fault");
	// moments
	G.bFaultBatt = false;
	const struct { double T; int Lamp; bool bOn; } At[] = { { 1.0, LampSep, false }, { 3.0, LampSep, true }, { 6.0, LampSep, false }, { 703.0, LampSep, true },
		{ 709.0, LampSep, false }, { 289.0, LampBurn, false }, { 295.0, LampBurn, true }, { 303.0, LampBurn, true }, { 307.0, LampBurn, false },
		{ 50.0, LampEngine, true }, { 76.0, LampEngine, false }, { 815.0, LampDrogue, true }, { 830.0, LampMainChute, true }, { 830.0, LampDrogue, false },
		{ 860.0, LampAltLow, false }, { 880.0, LampAltLow, true }, { 886.0, LampJets, true }, { 884.0, LampJets, false }, { 889.0, LampSplash, true },
		{ 887.0, LampSplash, false }, { 891.0, LampBeacon, true }, { 760.0, LampHullHot, true }, { 730.0, LampHullHot, false }, { 768.0, LampGHigh, true },
		{ 760.0, LampBlackout, true }, { 760.0, LampComm, false }, { 800.0, LampComm, true }, { 400.0, LampRecord, true }, { 400.0, LampZeroG, true } };
	for (const auto& X : At)
	{
		G.DevJump(X.T);
		CHECK(G.State().Lamps[X.Lamp].bOn == X.bOn, "%s is %s at %.0f s", LampDef(X.Lamp).Id, X.bOn ? "off" : "on", X.T);
	}
	G.DevJump(295.0);
	CHECK(G.State().Lamps[LampBurn].bBlink, "burn blinks before the burn");
	G.DevJump(303.0);
	CHECK(!G.State().Lamps[LampBurn].bBlink, "and is steady in it");
}

static void CheckAfloat()
{
	std::printf("the capsule afloat, the craft back on Europa\n");
	FFlight F;
	F.SetRecords(Rack());
	F.GameDay = 41;
	Launch(F);
	F.DevJump(899.0);
	F.Advance(2.0);
	CHECK(F.CapsuleAfloat() && F.FlightsFlown() == 1 && !F.CraftOnEuropa(), "afloat");
	CHECK(F.CanOpenHatch().empty(), "afloat the hatch opens");
	F.TakeEvents();
	FState S = F.State();
	CHECK(S.Scorch > 0.7 && S.bRealWorld && S.AltSeaM == 0.0 && S.I.ClockDigits[2] == 1 && S.I.ClockDigits[3] == 5, "scorched glass, the clock stopped at 15:00");
	CHECK(F.Toggle(SwArm) == "ui.flight.no.switch_flight" && F.CanSwimOut().empty() && F.PullChuteHandle() == "ui.flight.no.chute_late", "afloat: switches locked, free to swim");
	// the hatch opens: the hero's last line, once
	F.Advance(5.0);
	CHECK(F.TakeEvents().empty(), "quiet while the hatch is shut");
	F.bHatchShut = false;
	F.bWheelSealed = false;
	F.Advance(0.1);
	{
		const std::vector<FEvent> E = F.TakeEvents();
		CHECK(E.size() == 1 && E[0].Kind == CueSay && E[0].Id == "line.hero.flight_afloat.1", "the hero's line at the open hatch");
	}
	F.Advance(10.0);
	CHECK(F.TakeEvents().empty() && F.State().I.PressMmHg > 700.0, "once; sea air in the cabin");
	// near: it stays
	F.HeroDistM = 250.0;
	F.Advance(500.0);
	CHECK(F.CapsuleAfloat(), "the hero is near: the capsule stays");
	// far for 59 s, back, far again: the count starts again
	F.HeroDistM = 301.0;
	for (int K = 0; K < 59; ++K)
	{
		F.Advance(1.0);
	}
	CHECK(F.CapsuleAfloat(), "59 s away: still there");
	F.HeroDistM = 100.0;
	F.Advance(1.0);
	F.HeroDistM = 400.0;
	for (int K = 0; K < 59; ++K)
	{
		F.Advance(1.0);
	}
	CHECK(F.CapsuleAfloat(), "the minute starts again when he comes back");
	F.Advance(1.5);
	CHECK(!F.CapsuleAfloat() && F.CraftOnEuropa() && F.Phase() == EPhase::Parked, "a minute away: gone, and the craft stands on Europa");
	{
		const std::vector<FEvent> E = F.TakeEvents();
		CHECK(E.size() == 2 && E[0].Id == "capsule_gone" && E[1].Id == "craft_back", "capsule_gone, craft_back");
	}
	S = F.State();
	CHECK(!S.I.bPowered && S.Scorch == 0.0 && S.T == 0.0 && !S.Switches[0] && !S.bRealWorld && S.Record.Index == -1 && F.FlightsFlown() == 1, "cold and clean; the count stays");
	// the next day does it too
	FFlight G;
	G.GameDay = 7;
	Launch(G);
	G.DevJump(899.5);
	G.Advance(1.0);
	G.HeroDistM = 5.0;
	G.Advance(1000.0);
	CHECK(G.CapsuleAfloat(), "the same day: there");
	G.GameDay = 8;
	G.Advance(0.1);
	CHECK(G.CraftOnEuropa(), "the next day: gone");
	// and it flies again
	G.bHatchShut = G.bWheelSealed = true;
	Launch(G);
	CHECK(G.Flying(), "a second launch");
	G.TakeEvents();
	G.Advance(0.5);
	const FRun R = Fly(G, 0.5);
	CHECK(G.FlightsFlown() == 2 && G.CapsuleAfloat(), "two flights flown");
	int Cut = 0, Splash = 0;
	for (const FEvent& E : R.Events)
	{
		Cut += E.Id == "engine_cutoff";
		Splash += E.Id == "splash";
	}
	CHECK(Cut == 1 && Splash == 1, "the second flight's cues fire again");
	G.DevReset();
	CHECK(G.CraftOnEuropa() && G.FlightsFlown() == 2, "the dev reset");
}

static bool SameState(const FState& A, const FState& B, std::string& What)
{
	std::vector<FNamed> FA, FB;
	Flatten(A, FA);
	Flatten(B, FB);
	for (size_t I = 0; I < FA.size(); ++I)
	{
		if (FA[I].V != FB[I].V)
		{
			What = FA[I].Name;
			return false;
		}
	}
	if (A.Record.Id != B.Record.Id)
	{
		What = "rec.id";
		return false;
	}
	return true;
}

static void CheckSave()
{
	std::printf("save and load\n");
	uint32_t Seed = 20261127u;
	auto Rnd = [&Seed]()
	{
		Seed = Seed * 1664525u + 1013904223u;
		return (double)(Seed >> 8) / 16777216.0;
	};
	for (int Case = 0; Case < 20; ++Case)
	{
		// a flight with a life in it: records, a nap, the comforts; saved at a random moment (pad, flight, afloat)
		FFlight A;
		A.SetRecords(Rack());
		A.GameDay = 100 + Case;
		A.SunF = Norm(V3(Rnd() - 0.5, Rnd() - 0.5, Rnd() - 0.5));
		const double Dt = Case % 2 ? 1.0 / 60.0 : 0.1;
		const double Stop = Case == 0 ? -1.0 : (Case == 1 ? 905.0 : Rnd() * 900.0);
		if (Case == 0)
		{
			Board(A);
			A.Toggle(SwPower);
			A.Toggle(SwAir);
			A.Toggle(SwArm);
			A.Advance(0.7);
			A.RecordNext();
			A.Advance(3.3);
		}
		else
		{
			Launch(A);
			if (Case % 3 == 0)
			{
				A.ToggleRadio();
				A.NextCrtMode();
			}
			double Real = 0.0;
			const double PlayAt = Rnd() * 600.0, NapAt = Case % 4 == 1 ? 100.0 + Rnd() * 300.0 : 1e9;
			bool bPut = false;
			while ((A.Flying() ? A.Time() : 900.0 + Real) < Stop)
			{
				if (!bPut && A.Time() >= PlayAt)
				{
					A.RecordNext();
					bPut = true;
				}
				if (A.Time() >= NapAt && A.NapSleptSec() <= 0.0 && !A.Napping())
				{
					A.StartNap();
				}
				A.Advance(Dt);
				Real = A.Flying() ? 0.0 : Real + Dt;
			}
		}
		A.TakeEvents();
		const std::string Text = A.Save();
		FFlight B;
		if (Case % 2)
		{
			B.SetRecords(Rack());
		}
		CHECK(B.Load(Text), "case %d: the save does not load: %s", Case, Text.c_str());
		if (!(Case % 2))
		{
			B.SetRecords(Rack()); // the engine may give the rack after the load
		}
		CHECK(B.Save() == Text, "case %d: saved again it is another text", Case);
		std::string What;
		CHECK(SameState(A.State(), B.State(), What), "case %d (t %.1f, %s): the loaded state differs in %s", Case, A.Time(), PhaseName(A.Phase()), What.c_str());
		CHECK(A.FlightsFlown() == B.FlightsFlown() && A.Phase() == B.Phase() && A.Napping() == B.Napping(), "case %d: the counts", Case);
		// the same future: fly both on (a new record on the way, the hero swims off at the end)
		int Bad = 0, Evs = 0;
		std::string FirstBad;
		bool bEvents = true;
		for (int K = 0; K < 4000; ++K)
		{
			if (K == 200)
			{
				A.RecordNext();
				B.RecordNext();
			}
			if (K == 3000)
			{
				A.HeroDistM = B.HeroDistM = 500.0;
			}
			const double Step = K < 600 ? Dt : 0.5;
			A.Advance(Step);
			B.Advance(Step);
			const std::vector<FEvent> EA = A.TakeEvents(), EB = B.TakeEvents();
			bEvents = bEvents && EA.size() == EB.size();
			for (size_t I = 0; I < std::min(EA.size(), EB.size()); ++I)
			{
				bEvents = bEvents && EA[I].Id == EB[I].Id && EA[I].T == EB[I].T && EA[I].Arg == EB[I].Arg && EA[I].bAsleep == EB[I].bAsleep;
				Evs++;
			}
			if (K % 50 == 0 || K < 5)
			{
				if (!SameState(A.State(), B.State(), What))
				{
					Bad++;
					FirstBad = FirstBad.empty() ? What : FirstBad;
				}
			}
		}
		CHECK(Bad == 0, "case %d (saved at t %.1f): the future differs %d times, first in %s", Case, std::strtod(Text.c_str() + Text.find(";t=") + 3, nullptr), Bad, FirstBad.c_str());
		CHECK(bEvents && Evs > 0, "case %d: the futures' cues differ (%d compared)", Case, Evs);
		CHECK((A.CraftOnEuropa() && B.CraftOnEuropa()) || Case == 0, "case %d: both end with the craft back on Europa", Case);
	}
	FFlight X;
	CHECK(!X.Load("") && !X.Load("v2;t=5") && !X.Load("garbage") && X.Phase() == EPhase::Parked, "what is not a save is refused");
	CHECK(X.Load("v1") && X.Phase() == EPhase::Parked && X.FlightsFlown() == 0, "an empty save is a cold craft");
	CHECK(X.Load("v1;ph=99;t=1e9;cue=-5;sw=31;fl=3;junk;x=y") && X.Time() == 900.0 && X.FlightsFlown() == 3, "a damaged save is clamped");
	X.Advance(1.0);
	CHECK(X.Phase() == EPhase::Afloat || X.CraftOnEuropa(), "and the model lives on");
	// a save from the afloat capsule read on a later day: the capsule is gone
	FFlight Y;
	Y.GameDay = 5;
	Launch(Y);
	Y.DevJump(899.9);
	Y.Advance(0.5);
	const std::string Text = Y.Save();
	FFlight Z;
	CHECK(Z.Load(Text) && Z.CapsuleAfloat() && Z.FlightsFlown() == 1, "saved afloat");
	Z.GameDay = 6;
	Z.Advance(0.016);
	CHECK(Z.CraftOnEuropa() && Z.FlightsFlown() == 1, "loaded a day later: back on Europa");
}

static void CheckFast()
{
	std::printf("the fast-forward\n");
	FFlight A, B, C;
	for (FFlight* F : { &A, &B, &C })
	{
		F->SetRecords(Rack());
		Launch(*F);
		F->RecordNext();
		F->TakeEvents();
	}
	B.Fast = 20.0;
	C.Fast = 20.0;
	std::vector<FEvent> EA, EB;
	int Same = 0, N = 0;
	for (int K = 0; K < 60 * 46; ++K)
	{
		// B at 20x with a tick of 1/60 is A with a tick of 1/3
		A.Advance(20.0 / 60.0);
		B.Advance(1.0 / 60.0);
		std::string What;
		Same += SameState(A.State(), B.State(), What) || What == "time_scale" ? 1 : 0;
		N++;
		for (const FEvent& E : A.TakeEvents()) { EA.push_back(E); }
		for (const FEvent& E : B.TakeEvents()) { EB.push_back(E); }
	}
	CHECK(Same == N, "20x is the same flight in %d of %d ticks", Same, N);
	CHECK(EA.size() == EB.size() && EA.size() > 30 && A.CapsuleAfloat() && B.CapsuleAfloat(), "and the same %zu cues", EA.size());
	CHECK(B.State().TimeScale == 20.0 && A.State().TimeScale == 1.0, "the time scale says so");
	// and a 20x flight at 60 Hz against a normal one at 60 Hz
	FFlight D;
	Launch(D);
	const FRun RD = Fly(D, 1.0 / 60.0);
	C.RecordStop();
	const FRun RC = Fly(C, 1.0 / 60.0);
	CHECK(std::fabs(RC.Real - 45.0) < 0.02, "20x takes %.2f s", RC.Real);
	std::vector<FNamed> FC, FD;
	Flatten(RC.Final, FC);
	Flatten(RD.Final, FD);
	int Diff = 0;
	for (size_t I = 0; I < FC.size(); ++I)
	{
		const bool bSkip = std::strncmp(FC[I].Name, "lamp.", 5) == 0 || std::strncmp(FC[I].Name, "rec.", 4) == 0 || std::strcmp(FC[I].Name, "time_scale") == 0;
		Diff += !bSkip && std::fabs(FC[I].V - FD[I].V) > (std::strcmp(FC[I].Name, "i.press") == 0 ? 2.0 : 1e-6) ? 1 : 0;
	}
	CHECK(Diff == 0, "the 20x flight ends differently in %d outputs", Diff);
	FFlight Z;
	Z.Fast = 0.0;
	Launch(Z);
	Z.Fast = 0.0;
	Z.Advance(10.0);
	CHECK(Z.Time() == 0.0, "a factor of 0 is a pause");
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "";
	if (Mode == "dump")
	{
		Dump();
		return 0;
	}
	if (Mode == "show")
	{
		Show(Argc > 2 ? std::max(1.0, std::atof(Argv[2])) : 30.0);
		return 0;
	}
	if (Mode == "steps")
	{
		Steps();
		return 0;
	}
	CheckData();
	CheckProfile();
	CheckGeometry();
	CheckContinuity();
	CheckFlight();
	CheckChecklist();
	CheckNap();
	CheckRecords();
	CheckLamps();
	CheckAfloat();
	CheckSave();
	CheckFast();
	std::printf("%d checks passed, %d failed\n", Passed, Failures);
	return Failures ? 1 : 0;
}
