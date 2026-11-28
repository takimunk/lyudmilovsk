// The flight home from Europa in the descent craft «Ласточка» (docs/spacecraft.md), in plain C++ (no Unreal types) so the
// same rules run in the game (ULyudSpaceflightSubsystem, ALyudSpacecraft) and in the cloud (tools/spacecraftsim/run.sh).
// The programmer's guide is tools/spacecraft/MODEL_NOTES.md; read that first.
//
// Data: Content/Spacecraft/flight.json (tools/spacecraft/flight.json through tools/spacecraft/flight.py). Every number in
// it is also built in here as FConfig's defaults (the checks compare the two), so the model works without the file.
// What this file decides: the prelaunch checklist; the 900 s of the flight as PURE FUNCTIONS OF THE FLIGHT TIME (FProfile:
// range, speed, load, shake, plasma, the chute, where the planet, Europa and the star are in the cabin's frame, the track
// over the planet, the instruments); the nap (8x in the coast); the edge-triggered cues for sound, the hero's lines and the
// automatic voice; the record player on the shelf; the lamps; what happens to the capsule after the splash; the save.
//
// FRAMES (all right-handed, unit vectors, degrees):
//   Cabin frame C: X forward = out through the big porthole, Y LEFT, Z up at rest (the heat shield is at -Z).
//                  Unreal's cabin-local (X forward, Y right, Z up) = (X, -Y, Z).
//   Flight frame F (inertial, fixed at launch): the site on Europa; Z up, X horizontal toward the planet's azimuth,
//                  Y = Z x X. The planet stands at +35 degrees of elevation in the XZ plane at launch.
#pragma once

#include "../People/MiniJson.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace LyudFlight
{
	const double Pi = 3.14159265358979323846;
	const double G0 = 9.80665;

	inline double Rad(double D) { return D * Pi / 180.0; }
	inline double Deg(double R) { return R * 180.0 / Pi; }
	inline double Clamp(double X, double A, double B) { return X < A ? A : (X > B ? B : X); }
	inline double Lerp(double A, double B, double W) { return A + (B - A) * W; }
	/** Smoothstep of X from A to B (0 below A, 1 above B). */
	inline double Smooth(double A, double B, double X)
	{
		if (B <= A)
		{
			return X >= B ? 1.0 : 0.0;
		}
		const double U = Clamp((X - A) / (B - A), 0.0, 1.0);
		return U * U * (3.0 - 2.0 * U);
	}
	/** A jolt: Amp at T0, dying with Tau; nothing before T0. */
	inline double Pulse(double T, double T0, double Amp, double Tau) { return T < T0 ? 0.0 : Amp * std::exp(-(T - T0) / Tau); }
	/** 1 inside [A, B] with soft edges of Edge seconds inside the window. */
	inline double Window(double T, double A, double B, double Edge) { return Smooth(A, A + Edge, T) * (1.0 - Smooth(B - Edge, B, T)); }

	struct FVec
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
	};
	inline FVec V3(double X, double Y, double Z) { FVec V; V.X = X; V.Y = Y; V.Z = Z; return V; }
	inline FVec Add(const FVec& A, const FVec& B) { return V3(A.X + B.X, A.Y + B.Y, A.Z + B.Z); }
	inline FVec Sub(const FVec& A, const FVec& B) { return V3(A.X - B.X, A.Y - B.Y, A.Z - B.Z); }
	inline FVec Mul(const FVec& A, double S) { return V3(A.X * S, A.Y * S, A.Z * S); }
	inline double Dot(const FVec& A, const FVec& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	inline FVec Cross(const FVec& A, const FVec& B) { return V3(A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X); }
	inline double Len(const FVec& A) { return std::sqrt(Dot(A, A)); }
	inline FVec Norm(const FVec& A)
	{
		const double L = Len(A);
		return L > 1e-12 ? Mul(A, 1.0 / L) : V3(1.0, 0.0, 0.0);
	}
	inline double AngleDeg(const FVec& A, const FVec& B) { return Deg(std::acos(Clamp(Dot(A, B), -1.0, 1.0))); }

	/** A rotation: the body's three axes written in the parent frame. */
	struct FMat
	{
		FVec X = V3(1.0, 0.0, 0.0), Y = V3(0.0, 1.0, 0.0), Z = V3(0.0, 0.0, 1.0);
	};
	inline FVec ToLocal(const FMat& M, const FVec& V) { return V3(Dot(M.X, V), Dot(M.Y, V), Dot(M.Z, V)); }
	inline FVec ToParent(const FMat& M, const FVec& V) { return Add(Add(Mul(M.X, V.X), Mul(M.Y, V.Y)), Mul(M.Z, V.Z)); }
	inline FMat MulMat(const FMat& A, const FMat& B)
	{
		FMat M;
		M.X = ToParent(A, B.X);
		M.Y = ToParent(A, B.Y);
		M.Z = ToParent(A, B.Z);
		return M;
	}
	/** Yaw (toward +Y, left), then pitch (nose up), then roll (about the nose, left side up): the horizon ball's angles. */
	inline FMat Euler(double YawDeg, double PitchDeg, double RollDeg)
	{
		const double CY = std::cos(Rad(YawDeg)), SY = std::sin(Rad(YawDeg));
		const double CP = std::cos(Rad(PitchDeg)), SP = std::sin(Rad(PitchDeg));
		const double CR = std::cos(Rad(RollDeg)), SR = std::sin(Rad(RollDeg));
		auto Turn = [&](const FVec& V)
		{
			const FVec P = V3(V.X * CP - V.Z * SP, V.Y, V.X * SP + V.Z * CP);
			return V3(P.X * CY - P.Y * SY, P.X * SY + P.Y * CY, P.Z);
		};
		FMat M;
		M.X = Turn(V3(1.0, 0.0, 0.0));
		M.Y = Turn(V3(0.0, CR, SR));
		M.Z = Turn(V3(0.0, -SR, CR));
		return M;
	}

	struct FQuat
	{
		double W = 1.0, X = 0.0, Y = 0.0, Z = 0.0;
	};
	inline FQuat QuatOf(const FMat& M)
	{
		// m[row][col], column j = the body's axis j
		const double M00 = M.X.X, M01 = M.Y.X, M02 = M.Z.X;
		const double M10 = M.X.Y, M11 = M.Y.Y, M12 = M.Z.Y;
		const double M20 = M.X.Z, M21 = M.Y.Z, M22 = M.Z.Z;
		FQuat Q;
		const double Tr = M00 + M11 + M22;
		if (Tr > 0.0)
		{
			const double S = std::sqrt(Tr + 1.0) * 2.0;
			Q.W = 0.25 * S;
			Q.X = (M21 - M12) / S;
			Q.Y = (M02 - M20) / S;
			Q.Z = (M10 - M01) / S;
		}
		else if (M00 > M11 && M00 > M22)
		{
			const double S = std::sqrt(1.0 + M00 - M11 - M22) * 2.0;
			Q.W = (M21 - M12) / S;
			Q.X = 0.25 * S;
			Q.Y = (M01 + M10) / S;
			Q.Z = (M02 + M20) / S;
		}
		else if (M11 > M22)
		{
			const double S = std::sqrt(1.0 + M11 - M00 - M22) * 2.0;
			Q.W = (M02 - M20) / S;
			Q.X = (M01 + M10) / S;
			Q.Y = 0.25 * S;
			Q.Z = (M12 + M21) / S;
		}
		else
		{
			const double S = std::sqrt(1.0 + M22 - M00 - M11) * 2.0;
			Q.W = (M10 - M01) / S;
			Q.X = (M02 + M20) / S;
			Q.Y = (M12 + M21) / S;
			Q.Z = 0.25 * S;
		}
		return Q;
	}
	inline FMat MatOf(const FQuat& Q)
	{
		FMat M;
		M.X = V3(1.0 - 2.0 * (Q.Y * Q.Y + Q.Z * Q.Z), 2.0 * (Q.X * Q.Y + Q.W * Q.Z), 2.0 * (Q.X * Q.Z - Q.W * Q.Y));
		M.Y = V3(2.0 * (Q.X * Q.Y - Q.W * Q.Z), 1.0 - 2.0 * (Q.X * Q.X + Q.Z * Q.Z), 2.0 * (Q.Y * Q.Z + Q.W * Q.X));
		M.Z = V3(2.0 * (Q.X * Q.Z + Q.W * Q.Y), 2.0 * (Q.Y * Q.Z - Q.W * Q.X), 1.0 - 2.0 * (Q.X * Q.X + Q.Y * Q.Y));
		return M;
	}
	inline FMat Slerp(const FMat& A, const FMat& B, double W)
	{
		if (W <= 0.0)
		{
			return A;
		}
		if (W >= 1.0)
		{
			return B;
		}
		const FQuat QA = QuatOf(A);
		FQuat QB = QuatOf(B);
		double D = QA.W * QB.W + QA.X * QB.X + QA.Y * QB.Y + QA.Z * QB.Z;
		if (D < 0.0)
		{
			D = -D;
			QB.W = -QB.W; QB.X = -QB.X; QB.Y = -QB.Y; QB.Z = -QB.Z;
		}
		double SA = 1.0 - W, SB = W;
		if (D < 0.9995)
		{
			const double Th = std::acos(Clamp(D, -1.0, 1.0));
			const double Sn = std::sin(Th);
			SA = std::sin((1.0 - W) * Th) / Sn;
			SB = std::sin(W * Th) / Sn;
		}
		FQuat Q;
		Q.W = QA.W * SA + QB.W * SB;
		Q.X = QA.X * SA + QB.X * SB;
		Q.Y = QA.Y * SA + QB.Y * SB;
		Q.Z = QA.Z * SA + QB.Z * SB;
		const double L = std::sqrt(Q.W * Q.W + Q.X * Q.X + Q.Y * Q.Y + Q.Z * Q.Z);
		Q.W /= L; Q.X /= L; Q.Y /= L; Q.Z /= L;
		return MatOf(Q);
	}

	/** A curve through knots that never overshoots (monotone cubic, Fritsch-Carlson): flat where the knots are flat. */
	struct FCurve
	{
		std::vector<double> T, V, M;

		void Clear() { T.clear(); V.clear(); M.clear(); }
		void Add(double InT, double InV) { T.push_back(InT); V.push_back(InV); }
		void Build()
		{
			const size_t N = T.size();
			M.assign(N, 0.0);
			if (N < 2)
			{
				return;
			}
			std::vector<double> D(N - 1), H(N - 1);
			for (size_t K = 0; K + 1 < N; ++K)
			{
				H[K] = std::max(1e-9, T[K + 1] - T[K]);
				D[K] = (V[K + 1] - V[K]) / H[K];
			}
			M[0] = D[0];
			M[N - 1] = D[N - 2];
			for (size_t K = 1; K + 1 < N; ++K)
			{
				if (D[K - 1] * D[K] <= 0.0)
				{
					M[K] = 0.0;
				}
				else
				{
					const double W1 = 2.0 * H[K] + H[K - 1], W2 = H[K] + 2.0 * H[K - 1];
					M[K] = (W1 + W2) / (W1 / D[K - 1] + W2 / D[K]);
				}
			}
			// the ends: no more than three secants (keeps the first and the last span monotone)
			auto Tame = [](double& Slope, double Secant)
			{
				if (Slope * Secant <= 0.0)
				{
					Slope = 0.0;
				}
				else if (std::fabs(Slope) > 3.0 * std::fabs(Secant))
				{
					Slope = 3.0 * Secant;
				}
			};
			Tame(M[0], D[0]);
			Tame(M[N - 1], D[N - 2]);
		}
		double At(double X, double* Slope = nullptr) const
		{
			const size_t N = T.size();
			if (N == 0 || M.size() != N)
			{
				if (Slope) { *Slope = 0.0; }
				return 0.0;
			}
			if (X <= T[0] || N == 1)
			{
				if (Slope) { *Slope = 0.0; }
				return V[0];
			}
			if (X >= T[N - 1])
			{
				if (Slope) { *Slope = 0.0; }
				return V[N - 1];
			}
			size_t K = (size_t)(std::upper_bound(T.begin(), T.end(), X) - T.begin()) - 1;
			K = std::min(K, N - 2);
			const double H = T[K + 1] - T[K];
			const double U = (X - T[K]) / H;
			const double U2 = U * U, U3 = U2 * U;
			if (Slope)
			{
				*Slope = ((6.0 * U2 - 6.0 * U) * V[K] + (3.0 * U2 - 4.0 * U + 1.0) * H * M[K] + (-6.0 * U2 + 6.0 * U) * V[K + 1]
					+ (3.0 * U2 - 2.0 * U) * H * M[K + 1]) / H;
			}
			return (2.0 * U3 - 3.0 * U2 + 1.0) * V[K] + (U3 - 2.0 * U2 + U) * H * M[K] + (-2.0 * U3 + 3.0 * U2) * V[K + 1] + (U3 - U2) * H * M[K + 1];
		}
	};

	// ------------------------------------------------------------------ the names

	enum class EPhase : uint8_t { Parked, Prelaunch, Ascent, Coast, Approach, Entry, Chute, Splash, Afloat };
	const int PhaseCount = 9;

	inline const char* PhaseName(EPhase P)
	{
		static const char* const Names[PhaseCount] = { "parked", "prelaunch", "ascent", "coast", "approach", "entry", "chute", "splash", "afloat" };
		return Names[(int)P < PhaseCount ? (int)P : 0];
	}

	/** The five guarded toggles of the checklist, in the order they must go on. */
	enum ESwitch { SwPower, SwAir, SwGyro, SwSeal, SwArm, SwitchCount };

	inline const char* SwitchName(int S)
	{
		static const char* const Names[SwitchCount] = { "power", "air", "gyro", "seal", "arm" };
		return S >= 0 && S < SwitchCount ? Names[S] : "";
	}

	enum class EChute : uint8_t { None, Drogue, Main };

	/** The 4 x 6 annunciator board, row by row. */
	enum ELamp
	{
		LampPower, LampAir, LampGyro, LampHatch, LampArmed, LampEngine,
		LampSep, LampZeroG, LampBurn, LampComm, LampBlackout, LampGHigh,
		LampHullHot, LampDrogue, LampMainChute, LampAltLow, LampJets, LampSplash,
		LampBeacon, LampO2Low, LampCo2High, LampBattLow, LampMasterCaution, LampRecord,
		LampCount
	};

	struct FLampDef
	{
		const char* Id;
		const char* Colour; // green | amber | red | white
		bool bFault;        // never lit in a nominal flight
		const char* Rule;   // for people (flight.json, the notes); the rule itself is FFlight::Lamps
	};

	inline const FLampDef& LampDef(int L)
	{
		static const FLampDef Defs[LampCount] = {
			{ "power", "green", false, "the power switch is on" },
			{ "air", "green", false, "the air switch is on; blinks while the cabin pressure is under 700 mmHg" },
			{ "gyro", "green", false, "the gyros switch is on; blinks for the 4 s of spin-up" },
			{ "hatch", "green", false, "the hatch seal switch is on" },
			{ "armed", "amber", false, "armed and not launched yet; blinks" },
			{ "engine", "white", false, "the ascent engine burns (0..75 s)" },
			{ "sep", "white", false, "a separation: 4 s after the stage is left, 8 s after the instrument section goes" },
			{ "zero_g", "white", false, "in flight before the entry, the load under 0.02 g" },
			{ "burn", "amber", false, "blinks for 10 s before the correction burn, steady during it" },
			{ "comm", "green", false, "the radio link: on with the power, off in the blackout, blinks for 5 s after it" },
			{ "blackout", "amber", false, "the radio blackout of the entry" },
			{ "g_high", "red", false, "the load is over 3 g" },
			{ "hull_hot", "red", false, "the heat shield is over 900 C; blinks over 1400 C" },
			{ "drogue", "green", false, "the drogue is out (810..825 s)" },
			{ "main_chute", "green", false, "the main canopy is out, till 5 s after the splash" },
			{ "alt_low", "amber", false, "under the canopy and less than 150 m over the sea; blinks" },
			{ "jets", "white", false, "the soft-landing jets (885 s to the splash)" },
			{ "splash", "white", false, "down on the sea" },
			{ "beacon", "white", false, "the recovery beacon, from 2 s after the splash; blinks" },
			{ "o2_low", "red", true, "oxygen under 18 %" },
			{ "co2_high", "amber", false, "carbon dioxide over 0.8 % (it goes out when the scrubber has run)" },
			{ "batt_low", "red", true, "the battery under 24.5 V" },
			{ "master_caution", "red", false, "any red lamp (steady); blinks for 3 s after a refused switch" },
			{ "record", "white", false, "the flight recorder's tape runs: from the launch to the end of the flight" },
		};
		return Defs[L >= 0 && L < LampCount ? L : 0];
	}

	inline int LampOf(const std::string& Id)
	{
		for (int L = 0; L < LampCount; ++L)
		{
			if (Id == LampDef(L).Id)
			{
				return L;
			}
		}
		return -1;
	}

	/** What a cue is for. */
	enum ECue { CueSound = 0, CueSay = 1, CueAuto = 2 };

	inline const char* CueKindName(int K) { return K == CueSay ? "say" : (K == CueAuto ? "auto" : "sound"); }

	struct FCue
	{
		int Kind = CueSound;
		std::string Id;
		double T = 0.0;         // flight seconds; < 0: found by the model (bComputed)
		bool bComputed = false;
	};

	/** Edge-triggered: each fires once. Kind CueSay: Id is the hero's line key; CueAuto: Id is "auto.<id>". */
	struct FEvent
	{
		int Kind = CueSound;
		std::string Id;
		std::string Arg;      // a reason key (refused), a record id (record_start), a switch id
		double T = 0.0;       // flight seconds when it fired
		bool bAsleep = false; // fired while the hero naps (keep sounds soft; the model has already dropped the say cues)
	};

	// ------------------------------------------------------------------ the numbers

	struct FConfig
	{
		// times, flight seconds
		double TAscentEnd = 75.0, TApproach = 600.0, TEntry = 720.0, TChute = 810.0, TSplashPhase = 885.0, TEnd = 900.0;
		double TStageLeft = 1.5, TLookStart = 78.0, TLookEnd = 96.0, TTurnStart = 195.0, TTurnEnd = 225.0;
		double TBurn = 300.0, BurnDur = 6.0, TSep = 700.0, TMain = 825.0, TTouch = 888.0;
		double TWhiteIn = 840.0, TWhitePeak = 850.0, TWhiteOut = 860.0;
		// the nap
		double NapScale = 8.0, NapEndBefore = 20.0, NapLatestBefore = 40.0, NapBurnGuard = 12.0;
		// the planet
		double PlanetRadiusKm = 2000.0, PlanetDiaStartDeg = 26.0, PlanetDiaEntryDeg = 150.0, PlanetElevDeg = 35.0;
		double GrowthRampEnd = 200.0, IslandLatDeg = 18.0, ArrivalHeadingDeg = 70.0;
		// Europa as the flight shows it
		double EuropaRadiusKm = 200.0, EuropaG = 0.13;
		// the load, g
		double GAscentStart = 1.5, GAscentEnd = 1.7, GBurn = 0.3, GEntryPeak = 4.2, TEntryPeak = 768.0, EntrySigmaRise = 17.0, EntrySigmaFall = 14.0;
		double GSepJolt = 0.8, GDrogueSnatch = 1.0, GMainSnatch = 1.5, GSplashSpike = 2.4, EntryTilt = 0.27;
		// the plasma and the heat
		double TPlasmaPeak = 755.0, PlasmaSigmaRise = 14.0, PlasmaSigmaFall = 17.0, BlackoutPlasma = 0.25, ScorchMax = 0.8;
		double HullPeakC = 1650.0, HullSpaceC = -120.0, HullLag = 8.0, HullCoolLag = 30.0, PlasmaKLow = 1800.0, PlasmaKHigh = 7000.0;
		// under the canopy, m/s down
		double VEntryEnd = 120.0, VDrogue = 50.0, TauDrogue = 6.0, VMain = 7.0, TauMain = 14.0, VTouch = 2.0;
		double SwingDrogueDeg = 12.0, SwingMainDeg = 7.0, SwingRestDeg = 1.5, ChuteTwistDeg = 35.0;
		// the attitude
		double AscentPitchDeg = -20.0, ApproachLimbDeg = 102.0, EntryLimbDeg = 115.0, CoastRollTurns = 1.0;
		// instruments and lamps
		double LampGHigh = 3.0, LampHullHot = 900.0, LampHullBlink = 1400.0, LampAltLowM = 150.0, LampO2Low = 18.0, LampCo2High = 0.8, LampBattLow = 24.5;
		double PressNominal = 760.0, PressOk = 700.0, GyroSpinUp = 4.0, LampTest = 1.2, CautionSeconds = 3.0, BlinkPeriod = 0.6;
		// the record player
		double SkipShake = 0.6, SkipSlot = 2.3;
		// the capsule afloat
		double GoneDistM = 300.0, GoneSeconds = 60.0;
		// curves: [t, value]
		FCurve Track;    // the arc along the planet's surface from the craft's ground point to the splash point, degrees
		FCurve CabinTemp; // C
		FCurve Fuel;      // %
		std::vector<FCue> Cues;

		FConfig()
		{
			static const double TrackK[][2] = { { 0, 95 }, { 75, 95 }, { 250, 91 }, { 400, 80 }, { 500, 66 }, { 600, 46 }, { 660, 29 }, { 690, 18 },
				{ 720, 8 }, { 765, 1.4 }, { 810, 0.05 }, { 825, 0.02 }, { 850, 0.006 }, { 888, 0 } };
			for (const auto& K : TrackK) { Track.Add(K[0], K[1]); }
			static const double TempK[][2] = { { 0, 19 }, { 75, 20 }, { 300, 19.5 }, { 700, 19.5 }, { 740, 21 }, { 790, 29 }, { 815, 31.5 }, { 885, 27 }, { 900, 26 } };
			for (const auto& K : TempK) { CabinTemp.Add(K[0], K[1]); }
			static const double FuelK[][2] = { { 0, 100 }, { 2, 99 }, { 75, 40 }, { 195, 40 }, { 225, 38.5 }, { 300, 38.5 }, { 306, 31 }, { 590, 31 }, { 625, 29.5 },
				{ 700, 29.5 }, { 720, 27.5 }, { 810, 22 }, { 885, 22 }, { 888, 6 }, { 900, 6 } };
			for (const auto& K : FuelK) { Fuel.Add(K[0], K[1]); }
			struct FRow { int Kind; const char* Id; double T; };
			static const FRow Rows[] = {
				{ CueSound, "liftoff", 0 }, { CueAuto, "auto.launch", 0 }, { CueSound, "stage_left", 1.5 }, { CueSay, "line.hero.flight_liftoff.1", 8 },
				{ CueSound, "engine_cutoff", 75 }, { CueAuto, "auto.cutoff", 76 }, { CueAuto, "auto.weightless", 80 },
				{ CueSay, "line.hero.flight_weightless.1", 86 }, { CueSay, "line.hero.flight_europa_behind.1", 185 }, { CueSound, "turn_start", 195 },
				{ CueAuto, "auto.burn", 292 }, { CueSound, "correction_burn_on", 300 }, { CueSound, "correction_burn_off", 306 },
				{ CueSay, "line.hero.flight_planet_grows.1", 325 }, { CueAuto, "auto.half", 450 }, { CueSay, "line.hero.flight_ocean.1", 520 },
				{ CueAuto, "auto.approach", 596 }, { CueSound, "approach_start", 600 }, { CueSay, "line.hero.flight_island.1", -1 },
				{ CueAuto, "auto.sep", 694 }, { CueSound, "module_sep", 700 }, { CueSound, "entry_interface", 720 }, { CueAuto, "auto.entry", 721.5 },
				{ CueAuto, "auto.blackout", 727 }, { CueSound, "blackout_start", -1 }, { CueSay, "line.hero.flight_fire.1", 745 },
				{ CueSound, "peak_g", -1 }, { CueSound, "blackout_end", -1 }, { CueSound, "drogue", 810 }, { CueSound, "main_chute", 825 },
				{ CueAuto, "auto.chute", 827.5 }, { CueSay, "line.hero.flight_chute.1", 833 }, { CueSound, "whiteout", 840 }, { CueSound, "real_world", 850 },
				{ CueAuto, "auto.splash", 868 }, { CueSound, "jets", 885 }, { CueSound, "splash", 888 }, { CueSay, "line.hero.flight_home.1", 890 },
				{ CueAuto, "auto.home", 896 },
			};
			for (const FRow& R : Rows)
			{
				FCue C;
				C.Kind = R.Kind;
				C.Id = R.Id;
				C.T = R.T;
				C.bComputed = R.T < 0;
				Cues.push_back(C);
			}
		}

		/** Every plain number, with its place in flight.json: Fn(section, key, value). */
		template <typename TCfg, typename TFn>
		static void Scalars(TCfg& C, TFn&& Fn)
		{
			Fn("times", "ascent_end", C.TAscentEnd); Fn("times", "approach", C.TApproach); Fn("times", "entry", C.TEntry); Fn("times", "chute", C.TChute);
			Fn("times", "splash_phase", C.TSplashPhase); Fn("times", "end", C.TEnd); Fn("times", "stage_left", C.TStageLeft);
			Fn("times", "look_back_start", C.TLookStart); Fn("times", "look_back_end", C.TLookEnd); Fn("times", "turn_start", C.TTurnStart);
			Fn("times", "turn_end", C.TTurnEnd); Fn("times", "burn", C.TBurn); Fn("times", "burn_seconds", C.BurnDur); Fn("times", "module_sep", C.TSep);
			Fn("times", "main_chute", C.TMain); Fn("times", "touchdown", C.TTouch); Fn("times", "whiteout_in", C.TWhiteIn);
			Fn("times", "whiteout_peak", C.TWhitePeak); Fn("times", "whiteout_out", C.TWhiteOut);
			Fn("nap", "scale", C.NapScale); Fn("nap", "ends_before_approach", C.NapEndBefore); Fn("nap", "latest_start_before_approach", C.NapLatestBefore);
			Fn("nap", "burn_guard", C.NapBurnGuard);
			Fn("planet", "radius_km", C.PlanetRadiusKm); Fn("planet", "diameter_start_deg", C.PlanetDiaStartDeg);
			Fn("planet", "diameter_entry_deg", C.PlanetDiaEntryDeg); Fn("planet", "elevation_deg", C.PlanetElevDeg);
			Fn("planet", "growth_ramp_end", C.GrowthRampEnd); Fn("planet", "island_lat_deg", C.IslandLatDeg);
			Fn("planet", "arrival_heading_deg", C.ArrivalHeadingDeg);
			Fn("europa", "flight_radius_km", C.EuropaRadiusKm); Fn("europa", "gravity_g", C.EuropaG);
			Fn("g", "ascent_start", C.GAscentStart); Fn("g", "ascent_end", C.GAscentEnd); Fn("g", "burn", C.GBurn); Fn("g", "entry_peak", C.GEntryPeak);
			Fn("g", "entry_peak_t", C.TEntryPeak); Fn("g", "entry_sigma_rise", C.EntrySigmaRise); Fn("g", "entry_sigma_fall", C.EntrySigmaFall);
			Fn("g", "sep_jolt", C.GSepJolt); Fn("g", "drogue_snatch", C.GDrogueSnatch); Fn("g", "main_snatch", C.GMainSnatch);
			Fn("g", "splash_spike", C.GSplashSpike); Fn("g", "entry_tilt", C.EntryTilt);
			Fn("heat", "plasma_peak_t", C.TPlasmaPeak); Fn("heat", "plasma_sigma_rise", C.PlasmaSigmaRise); Fn("heat", "plasma_sigma_fall", C.PlasmaSigmaFall);
			Fn("heat", "blackout_plasma", C.BlackoutPlasma); Fn("heat", "scorch_max", C.ScorchMax); Fn("heat", "hull_peak_c", C.HullPeakC);
			Fn("heat", "hull_space_c", C.HullSpaceC); Fn("heat", "hull_lag", C.HullLag); Fn("heat", "hull_cool_lag", C.HullCoolLag); Fn("heat", "plasma_k_low", C.PlasmaKLow);
			Fn("heat", "plasma_k_high", C.PlasmaKHigh);
			Fn("chute", "v_entry_end", C.VEntryEnd); Fn("chute", "v_drogue", C.VDrogue); Fn("chute", "tau_drogue", C.TauDrogue); Fn("chute", "v_main", C.VMain);
			Fn("chute", "tau_main", C.TauMain); Fn("chute", "v_touch", C.VTouch); Fn("chute", "swing_drogue_deg", C.SwingDrogueDeg);
			Fn("chute", "swing_main_deg", C.SwingMainDeg); Fn("chute", "swing_rest_deg", C.SwingRestDeg); Fn("chute", "twist_deg", C.ChuteTwistDeg);
			Fn("attitude", "ascent_pitch_deg", C.AscentPitchDeg); Fn("attitude", "approach_limb_deg", C.ApproachLimbDeg);
			Fn("attitude", "entry_limb_deg", C.EntryLimbDeg); Fn("attitude", "coast_roll_turns", C.CoastRollTurns);
			Fn("panel", "lamp_g_high", C.LampGHigh); Fn("panel", "lamp_hull_hot_c", C.LampHullHot); Fn("panel", "lamp_hull_blink_c", C.LampHullBlink);
			Fn("panel", "lamp_alt_low_m", C.LampAltLowM); Fn("panel", "lamp_o2_low_pct", C.LampO2Low); Fn("panel", "lamp_co2_high_pct", C.LampCo2High);
			Fn("panel", "lamp_batt_low_v", C.LampBattLow); Fn("panel", "press_nominal_mmhg", C.PressNominal); Fn("panel", "press_ok_mmhg", C.PressOk);
			Fn("panel", "gyro_spin_up", C.GyroSpinUp); Fn("panel", "lamp_test", C.LampTest); Fn("panel", "caution_seconds", C.CautionSeconds);
			Fn("panel", "blink_period", C.BlinkPeriod);
			Fn("record", "skip_shake", C.SkipShake); Fn("record", "skip_slot", C.SkipSlot);
			Fn("afloat", "gone_dist_m", C.GoneDistM); Fn("afloat", "gone_seconds", C.GoneSeconds);
		}

		template <typename TCfg, typename TFn>
		static void Curves(TCfg& C, TFn&& Fn)
		{
			Fn("track_deg", C.Track);
			Fn("cabin_temp_c", C.CabinTemp);
			Fn("fuel_pct", C.Fuel);
		}

		/** Takes what Content/Spacecraft/flight.json gives; what is missing keeps its built-in value. */
		bool LoadJson(const std::string& Text, std::string& Error)
		{
			LyudJson::FValue J;
			if (!LyudJson::Parse(Text, J, Error))
			{
				return false;
			}
			Scalars(*this, [&J](const char* Section, const char* Key, double& V)
			{
				const LyudJson::FValue* N = J[Section].Find(Key);
				if (N && N->Type == LyudJson::FValue::Number)
				{
					V = N->Num;
				}
			});
			bool bOk = true;
			Curves(*this, [&J, &bOk, &Error](const char* Key, FCurve& C)
			{
				const LyudJson::FValue* A = J.Find(Key);
				if (!A)
				{
					return;
				}
				FCurve Out;
				for (const LyudJson::FValue& K : A->Items)
				{
					if (K.Items.size() != 2 || (!Out.T.empty() && K[0].Num <= Out.T.back()))
					{
						bOk = false;
						Error = std::string("flight.json: bad knots in ") + Key;
						return;
					}
					Out.Add(K[0].Num, K[1].Num);
				}
				if (Out.T.size() < 2)
				{
					bOk = false;
					Error = std::string("flight.json: too few knots in ") + Key;
					return;
				}
				C = Out;
			});
			if (!bOk)
			{
				return false;
			}
			if (const LyudJson::FValue* L = J.Find("lamps"))
			{
				if ((int)L->Items.size() != LampCount)
				{
					Error = "flight.json: the lamp board is 24 lamps";
					return false;
				}
				for (int K = 0; K < LampCount; ++K)
				{
					if ((*L)[(size_t)K]["id"].S() != LampDef(K).Id || (*L)[(size_t)K]["colour"].S() != LampDef(K).Colour)
					{
						Error = std::string("flight.json: lamp ") + std::to_string(K) + " is not the model's " + LampDef(K).Id;
						return false;
					}
				}
			}
			if (const LyudJson::FValue* S = J.Find("switches"))
			{
				if ((int)S->Items.size() != SwitchCount)
				{
					Error = "flight.json: five switches";
					return false;
				}
				for (int K = 0; K < SwitchCount; ++K)
				{
					if ((*S)[(size_t)K].S() != SwitchName(K))
					{
						Error = std::string("flight.json: switch ") + std::to_string(K) + " is not the model's " + SwitchName(K);
						return false;
					}
				}
			}
			if (const LyudJson::FValue* Q = J.Find("cues"))
			{
				std::vector<FCue> Out;
				for (const LyudJson::FValue& K : Q->Items)
				{
					FCue C;
					const std::string Kind = K["kind"].S();
					C.Kind = Kind == "say" ? CueSay : (Kind == "auto" ? CueAuto : CueSound);
					C.Id = K["id"].S();
					C.bComputed = K["t"].Type != LyudJson::FValue::Number;
					C.T = C.bComputed ? -1.0 : K["t"].Num;
					if (C.Id.empty())
					{
						Error = "flight.json: a cue without an id";
						return false;
					}
					Out.push_back(C);
				}
				Cues = Out;
			}
			return true;
		}
	};

	// ------------------------------------------------------------------ what a tick gives

	struct FLampState
	{
		bool bOn = false;    // the rule holds
		bool bBlink = false; // it blinks while on
		bool bLit = false;   // the bulb glows right now (the blink and the lamp test applied)
	};

	struct FInstruments
	{
		bool bPowered = false;    // the panel is alive (power switch); drop the needles and blank the digits when false
		bool bLampTest = false;   // every bulb glows for a moment after the power comes on
		double PressMmHg = 0.0;   // press
		double O2Pct = 0.0;       // o2
		double Co2Pct = 0.0;      // co2
		double TempC = 0.0;       // temp
		double Volts = 0.0;       // volts
		double FuelPct = 0.0;     // fuel
		double HullC = 0.0;       // hull, degrees
		double Hull01 = 0.0;      // hull, the bar 0..1
		double GMeter = 0.0;      // gmeter, 0..6
		double ClockSec = 0.0;    // clock: seconds since ПУСК
		int ClockDigits[6] = { 0, 0, 0, 0, 0, 0 }; // H H M M S S
		int RangeKm = 0;          // range: 6 digits
		double SpeedShown = 0.0;  // speed: km/s, or m/s under the canopy (bSpeedMs)
		bool bSpeedMs = false;
		double AltDialKm = 0.0;   // alt: over Europa till the approach, then over the sea (bAltSea); jumps once, at the approach
		bool bAltSea = false;
		double GlobeLatDeg = 0.0, GlobeLonRelDeg = 0.0; // globe: the point under the craft (longitude from the island's)
		// crt: all in the screen's 0..1 square (x right, y up); the path is CrtPath(0..1)
		double CrtEuropaX = 0.0, CrtEuropaY = 0.0, CrtEuropaR = 0.0, CrtPlanetX = 0.0, CrtPlanetY = 0.0, CrtPlanetR = 0.0;
		double CrtDotX = 0.0, CrtDotY = 0.0, CrtProgress = 0.0;
		int CrtPhase = 0;         // (int)EPhase: the word is flight.phase.<PhaseName>
		int CrtMode = 0;          // the CRT mode switch, 0..2
		bool bCabinLight = false, bRadio = false;
	};

	struct FRecordState
	{
		int Index = -1;        // the record on the platter (into the list given), -1 none
		std::string Id;
		bool bPlaying = false; // the needle is down
		double PosSec = 0.0, LengthSec = 0.0;
		double Pitch = 1.0;    // play rate: the platter drags under load
		double Wow = 0.0;      // 0..1 flutter depth
	};

	/** Everything the cabin shows at one moment. Directions are unit vectors in the cabin frame C (see the top). */
	struct FState
	{
		EPhase Phase = EPhase::Parked;
		double T = 0.0;             // flight seconds since ПУСК (0 before; stays at 900 afloat)
		double PhaseProgress = 0.0; // 0..1 through the phase (0 in parked, prelaunch, afloat)
		double TimeScale = 1.0;     // flight seconds per real second right now
		bool bNap = false;
		bool bFlying = false;       // ascent .. splash
		bool bEngine = false, bBurn = false, bBlackout = false;

		double RangeKm = 0.0;       // to the planet's surface (= the altitude over the sea)
		double RangeRateKms = 0.0;  // negative: closing
		double AltEuropaKm = 0.0;   // how far the pad is
		double SpeedKms = 0.0;
		double VSpeedMs = 0.0;      // the alt dial's rate: + climbing from Europa (till the approach), - coming down to the sea
		double AltSeaM = 0.0;       // RangeKm in metres; from bRealWorld on it is the real height over the real sea

		FVec G;                     // the felt load in g: where loose things fall, in the cabin frame
		double GLoad = 0.0;         // its size
		double Shake = 0.0;         // 0..1
		double Plasma = 0.0;        // 0..1
		double PlasmaK = 0.0;       // its colour temperature, kelvin (meaningful while Plasma > 0)
		double Scorch = 0.0;        // 0..1 on the glass; stays till the craft is back on Europa
		double HullC = 0.0;
		double Whiteout = 0.0;      // 0..1, peaks at 850 s
		bool bRealWorld = false;    // from the whiteout's peak: the cabin hangs in the real level at AltSeaM

		EChute Chute = EChute::None;
		double ChuteInflation = 0.0;                    // 0..1 of the stage that is out
		double SwingPitchDeg = 0.0, SwingRollDeg = 0.0; // the canopy's swing (already in the attitude below)
		double ChuteYawDeg = 0.0;                       // the slow twist under the lines (already in YawDeg)

		FVec PlanetDir;             // to the planet's centre
		double PlanetDiaDeg = 0.0;  // its angular diameter
		FVec PlanetIslandAxis;      // the planet's own axis through the island (its splash point), from the planet's centre
		FVec PlanetNorthAxis;       // the planet's north pole axis
		FVec EuropaDir;
		double EuropaDiaDeg = 0.0;
		double EuropaVisible = 0.0; // 0 when the planet hides it
		FVec SunDir;                // the star
		FVec FrameX, FrameY, FrameZ; // the flight frame F's axes in the cabin frame (turn the star sphere by these)

		double RollDeg = 0.0, PitchDeg = 0.0, YawDeg = 0.0; // the horizon ball (not wrapped)

		double SubLatDeg = 0.0;       // the point under the craft: latitude on the planet
		double SubLonRelDeg = 0.0;    // its longitude minus the island's (east positive)
		double TrackArcDeg = 0.0;     // the arc from that point to the island
		double IslandRangeKm = 0.0;   // the same along the surface
		double IslandBearingDeg = 0.0; // compass bearing from that point to the island
		double IslandVisible = 0.0;   // 0..1: over the horizon as seen from here
		FVec IslandDir;               // from the craft to the island

		FInstruments I;
		FLampState Lamps[LampCount];
		bool Switches[SwitchCount] = { false, false, false, false, false };
		FRecordState Record;
	};

	/** The CRT's drawn path: P 0 (the pad on Europa) .. 1 (the splash), in the screen's 0..1 square. */
	inline void CrtPath(double P, double& X, double& Y)
	{
		const double EX = 0.14, EY = 0.26, PX = 0.68, PY = 0.56, Bend = 600.0 / 900.0;
		P = Clamp(P, 0.0, 1.0);
		const double R0 = 0.30, R1 = 0.165, A0 = Rad(205.0), A1 = Rad(20.0);
		const double SX = PX + R0 * std::cos(A0), SY = PY + R0 * std::sin(A0);
		if (P < Bend)
		{
			const double U = P / Bend;
			X = Lerp(EX + 0.045, SX, U);
			Y = Lerp(EY + 0.02, SY, U) + 0.05 * std::sin(U * Pi) * (1.0 - U);
			return;
		}
		const double U = (P - Bend) / (1.0 - Bend);
		const double A = Lerp(A0, A1, U), R = Lerp(R0, R1, U * U);
		X = PX + R * std::cos(A);
		Y = PY + R * std::sin(A);
	}

	// ------------------------------------------------------------------ the flight as functions of its time

	class FProfile
	{
	public:
		FConfig C;
		double TBlackoutStart = 0.0, TBlackoutEnd = 0.0, TPeakG = 0.0, TIslandSeen = 0.0, TAltLow = 0.0;
		double RangeStartKm = 0.0, RangeEntryKm = 0.0, AltChuteM = 0.0, AltMainM = 0.0, AltJetsM = 0.0, AltSwapM = 0.0;
		std::vector<FCue> Cues; // sorted by time, every time found

		FProfile() { Build(FConfig()); }
		explicit FProfile(const FConfig& In) { Build(In); }

		void Build(const FConfig& In)
		{
			C = In;
			C.Track.Build();
			C.CabinTemp.Build();
			C.Fuel.Build();
			A0 = Rad(C.PlanetDiaStartDeg * 0.5);
			RangeStartKm = C.PlanetRadiusKm * (1.0 / std::sin(A0) - 1.0);
			// the planet's growth: the log of its diameter; its rate follows the ascent's kinematics, then eases to a constant
			const double GK = Stiff(A0);
			const int NA = (int)std::lround(C.TAscentEnd / Step), NE = (int)std::lround(C.TEntry / Step);
			Rate.assign((size_t)NE + 2, 0.0);
			Cum.assign((size_t)NE + 2, 0.0);
			double V = 0.0;
			for (int K = 1; K <= NA; ++K)
			{
				V += 0.5 * (AscentAccel((K - 1) * Step) + AscentAccel(K * Step)) * Step;
				Rate[(size_t)K] = V / GK;
				Cum[(size_t)K] = Cum[(size_t)K - 1] + 0.5 * (Rate[(size_t)K - 1] + Rate[(size_t)K]) * Step;
			}
			const double R75 = Rate[(size_t)NA], Total = std::log(C.PlanetDiaEntryDeg / C.PlanetDiaStartDeg);
			const double Ramp = std::max(1.0, C.GrowthRampEnd - C.TAscentEnd);
			const double K0 = (Total - Cum[(size_t)NA] - R75 * Ramp * 0.5) / (Ramp * 0.5 + (C.TEntry - C.GrowthRampEnd));
			for (int K = NA + 1; K <= NE + 1; ++K)
			{
				Rate[(size_t)K] = R75 + (K0 - R75) * Smooth(C.TAscentEnd, C.GrowthRampEnd, K * Step);
				Cum[(size_t)K] = Cum[(size_t)K - 1] + 0.5 * (Rate[(size_t)K - 1] + Rate[(size_t)K]) * Step;
			}
			// under the canopy, from the sea up
			V825 = C.VDrogue + (C.VEntryEnd - C.VDrogue) * std::exp(-(C.TMain - C.TChute) / C.TauDrogue);
			V885 = C.VMain + (V825 - C.VMain) * std::exp(-(C.TSplashPhase - C.TMain) / C.TauMain);
			AltJetsM = ChuteAlt(C.TSplashPhase);
			AltMainM = ChuteAlt(C.TMain);
			AltChuteM = ChuteAlt(C.TChute);
			AltSwapM = ChuteAlt(C.TWhitePeak);
			double RateE = 0.0;
			RangeEntryKm = Range(C.TEntry, &RateE);
			RateEntry = RateE;
			// the planet in the flight frame
			PlanetF = V3(std::cos(Rad(C.PlanetElevDeg)), 0.0, std::sin(Rad(C.PlanetElevDeg)));
			U0 = Mul(PlanetF, -1.0);
			Arc0 = C.Track.At(0.0);
			UIsland = RotY(U0, Arc0);
			const FVec Travel = Cross(V3(0.0, 1.0, 0.0), UIsland);
			const double H = Rad(C.ArrivalHeadingDeg);
			const FVec NorthT = Sub(Mul(Travel, std::cos(H)), Mul(Cross(Travel, UIsland), std::sin(H)));
			North = Norm(Add(Mul(UIsland, std::sin(Rad(C.IslandLatDeg))), Mul(NorthT, std::cos(Rad(C.IslandLatDeg)))));
			PlanetX = Norm(Sub(UIsland, Mul(North, Dot(UIsland, North))));
			PlanetY = Cross(North, PlanetX);
			// the heat shield and the glass
			const int NH = (int)std::lround(C.TEnd / Step) + 1;
			Hull.assign((size_t)NH + 1, C.HullSpaceC);
			Scorched.assign((size_t)NH + 1, 0.0);
			double Drive = C.HullPeakC;
			for (int Pass = 0; Pass < 2; ++Pass)
			{
				double Max = C.HullSpaceC;
				for (int K = 1; K <= NH; ++K)
				{
					const double T = K * Step;
					double Target = C.HullSpaceC + (Drive - C.HullSpaceC) * std::pow(Plasma(T), 0.8), Tau = C.HullLag;
					if (T >= C.TTouch)
					{
						Target = 25.0;
						Tau = 5.0;
					}
					else if (T >= C.TChute)
					{
						Target = 40.0;
						Tau = 30.0;
					}
					if (T < C.TChute && Target < Hull[(size_t)K - 1])
					{
						Tau = C.HullCoolLag; // a soaked shield cools slowly
					}
					Hull[(size_t)K] = Hull[(size_t)K - 1] + (Target - Hull[(size_t)K - 1]) * (1.0 - std::exp(-Step / Tau));
					Max = std::max(Max, Hull[(size_t)K]);
				}
				if (Pass == 0 && Max > C.HullSpaceC + 1.0)
				{
					Drive = C.HullSpaceC + (C.HullPeakC - C.HullSpaceC) * (C.HullPeakC - C.HullSpaceC) / (Max - C.HullSpaceC);
				}
			}
			for (int K = 1; K <= NH; ++K)
			{
				Scorched[(size_t)K] = Scorched[(size_t)K - 1] + 0.5 * (Plasma((K - 1) * Step) + Plasma(K * Step)) * Step;
			}
			const double Sum = std::max(1e-9, Scorched[(size_t)NH]);
			for (double& S : Scorched)
			{
				S = S / Sum * C.ScorchMax;
			}
			// the moments the model finds itself
			TBlackoutStart = Cross1([this](double T) { return Plasma(T) - C.BlackoutPlasma; }, C.TEntry, C.TPlasmaPeak);
			TBlackoutEnd = Cross1([this](double T) { return C.BlackoutPlasma - Plasma(T); }, C.TPlasmaPeak, C.TChute);
			TPeakG = C.TEntryPeak;
			double Best = 0.0;
			for (double T = C.TEntry; T < C.TChute; T += 0.05)
			{
				const double L = Load(T);
				if (L > Best)
				{
					Best = L;
					TPeakG = T;
				}
			}
			TIslandSeen = Cross1([this](double T) { return IslandVisible(T) - 0.5; }, C.TAscentEnd, C.TEntry);
			TAltLow = Cross1([this](double T) { return C.LampAltLowM - ChuteAlt(T); }, C.TMain, C.TTouch);
			Cues = C.Cues;
			for (FCue& Q : Cues)
			{
				if (!Q.bComputed)
				{
					continue;
				}
				if (Q.Id == "blackout_start") { Q.T = TBlackoutStart; }
				else if (Q.Id == "blackout_end") { Q.T = TBlackoutEnd; }
				else if (Q.Id == "peak_g") { Q.T = TPeakG; }
				else if (Q.Id == "line.hero.flight_island.1") { Q.T = std::min(TIslandSeen + 3.0, C.TSep - 12.0); }
				else { Q.T = 0.0; }
			}
			std::stable_sort(Cues.begin(), Cues.end(), [](const FCue& L, const FCue& R) { return L.T < R.T; });
		}

		// ---- phases

		EPhase PhaseAt(double T) const
		{
			return T < C.TAscentEnd ? EPhase::Ascent : T < C.TApproach ? EPhase::Coast : T < C.TEntry ? EPhase::Approach : T < C.TChute ? EPhase::Entry
				: T < C.TSplashPhase ? EPhase::Chute : EPhase::Splash;
		}
		double PhaseStart(EPhase P) const
		{
			switch (P)
			{
			case EPhase::Coast: return C.TAscentEnd;
			case EPhase::Approach: return C.TApproach;
			case EPhase::Entry: return C.TEntry;
			case EPhase::Chute: return C.TChute;
			case EPhase::Splash: return C.TSplashPhase;
			case EPhase::Afloat: return C.TEnd;
			default: return 0.0;
			}
		}
		double PhaseEnd(EPhase P) const
		{
			switch (P)
			{
			case EPhase::Ascent: return C.TAscentEnd;
			case EPhase::Coast: return C.TApproach;
			case EPhase::Approach: return C.TEntry;
			case EPhase::Entry: return C.TChute;
			case EPhase::Chute: return C.TSplashPhase;
			case EPhase::Splash: return C.TEnd;
			default: return 0.0;
			}
		}
		/** The moments where a continuous output may jump (a bang, a snatch, the splash). */
		std::vector<double> Jolts() const { return { C.TSep, C.TChute, C.TMain, C.TTouch }; }

		// ---- the path

		/** km to the planet's surface; Rate in km/s (negative: closing). */
		double Range(double T, double* OutRate = nullptr) const
		{
			T = Clamp(T, 0.0, C.TEnd);
			double H = 0.0, R = 0.0;
			if (T <= C.TEntry)
			{
				double LnRate = 0.0;
				const double A = A0 * std::exp(LnDia(T, LnRate));
				H = C.PlanetRadiusKm * (1.0 / std::sin(A) - 1.0);
				R = -Stiff(A) * LnRate;
			}
			else if (T < C.TChute)
			{
				const double Span = C.TChute - C.TEntry, U = (T - C.TEntry) / Span, U2 = U * U, U3 = U2 * U;
				const double V0 = RangeEntryKm, V1 = AltChuteM / 1000.0, M0 = RateEntry, M1 = -C.VEntryEnd / 1000.0;
				H = (2.0 * U3 - 3.0 * U2 + 1.0) * V0 + (U3 - 2.0 * U2 + U) * Span * M0 + (-2.0 * U3 + 3.0 * U2) * V1 + (U3 - U2) * Span * M1;
				R = ((6.0 * U2 - 6.0 * U) * V0 + (3.0 * U2 - 4.0 * U + 1.0) * Span * M0 + (-6.0 * U2 + 6.0 * U) * V1 + (3.0 * U2 - 2.0 * U) * Span * M1) / Span;
			}
			else
			{
				H = ChuteAlt(T) / 1000.0;
				R = -ChuteV(T) / 1000.0;
			}
			if (OutRate)
			{
				*OutRate = R;
			}
			return H;
		}
		double PlanetDiaDeg(double T) const
		{
			const double H = Range(T);
			return 2.0 * Deg(std::asin(Clamp(C.PlanetRadiusKm / (C.PlanetRadiusKm + H), -1.0, 1.0)));
		}
		/** m/s down under the canopy. */
		double ChuteV(double T) const
		{
			if (T < C.TMain)
			{
				return C.VDrogue + (C.VEntryEnd - C.VDrogue) * std::exp(-(T - C.TChute) / C.TauDrogue);
			}
			if (T < C.TSplashPhase)
			{
				return C.VMain + (V825 - C.VMain) * std::exp(-(T - C.TMain) / C.TauMain);
			}
			if (T < C.TTouch)
			{
				return V885 + (C.VTouch - V885) * (T - C.TSplashPhase) / (C.TTouch - C.TSplashPhase);
			}
			return 0.0;
		}
		/** m over the sea under the canopy (the integral of ChuteV up from the splash). */
		double ChuteAlt(double T) const
		{
			if (T >= C.TTouch)
			{
				return 0.0;
			}
			const double DJ = C.TTouch - C.TSplashPhase;
			if (T >= C.TSplashPhase)
			{
				const double X = T - C.TSplashPhase;
				return V885 * (DJ - X) + (C.VTouch - V885) * (DJ * DJ - X * X) / (2.0 * DJ);
			}
			const double Jets = V885 * DJ + (C.VTouch - V885) * DJ * 0.5;
			if (T >= C.TMain)
			{
				return Jets + C.VMain * (C.TSplashPhase - T)
					+ (V825 - C.VMain) * C.TauMain * (std::exp(-(T - C.TMain) / C.TauMain) - std::exp(-(C.TSplashPhase - C.TMain) / C.TauMain));
			}
			const double Main = Jets + C.VMain * (C.TSplashPhase - C.TMain) + (V825 - C.VMain) * C.TauMain * (1.0 - std::exp(-(C.TSplashPhase - C.TMain) / C.TauMain));
			return Main + C.VDrogue * (C.TMain - T)
				+ (C.VEntryEnd - C.VDrogue) * C.TauDrogue * (std::exp(-(T - C.TChute) / C.TauDrogue) - std::exp(-(C.TMain - C.TChute) / C.TauDrogue));
		}

		// ---- the load, the shake, the fire

		double AscentThrust(double T) const { return Smooth(0.0, 2.5, T) * (1.0 - Smooth(C.TAscentEnd - 1.2, C.TAscentEnd, T)); }
		/** km/s2 away from Europa. */
		double AscentAccel(double T) const
		{
			return T >= C.TAscentEnd ? 0.0 : (Lerp(C.GAscentStart, C.GAscentEnd, T / C.TAscentEnd) - C.EuropaG) * AscentThrust(T) * G0 / 1000.0;
		}
		double BurnWindow(double T) const { return Window(T, C.TBurn, C.TBurn + C.BurnDur, 0.4); }
		double EntryBell(double T) const
		{
			if (T < C.TEntry || T > C.TChute)
			{
				return 0.0;
			}
			const double S = T < C.TEntryPeak ? C.EntrySigmaRise : C.EntrySigmaFall, X = (T - C.TEntryPeak) / S;
			return std::exp(-0.5 * X * X) * Smooth(C.TEntry, C.TEntry + 10.0, T);
		}
		double EntryBase(double T) const { return Smooth(C.TEntry + 15.0, C.TChute - 8.0, T); }
		double JetsWindow(double T) const { return Window(T, C.TSplashPhase, C.TTouch, 0.3); }

		/** The felt load, g. */
		double Load(double T) const
		{
			if (T < C.TAscentEnd)
			{
				const double Down = Smooth(C.TAscentEnd - 1.2, C.TAscentEnd, T);
				return (C.EuropaG + (Lerp(C.GAscentStart, C.GAscentEnd, T / C.TAscentEnd) - C.EuropaG) * Smooth(0.0, 2.5, T)) * (1.0 - Down);
			}
			if (T < C.TEntry)
			{
				return C.GBurn * BurnWindow(T) + Pulse(T, C.TSep, C.GSepJolt, 0.12);
			}
			if (T < C.TChute)
			{
				return EntryBase(T) + (C.GEntryPeak - EntryBase(C.TEntryPeak)) * EntryBell(T);
			}
			if (T < C.TSplashPhase)
			{
				const double Decel = T < C.TMain ? (ChuteV(T) - C.VDrogue) / C.TauDrogue : (ChuteV(T) - C.VMain) / C.TauMain;
				return 1.0 + Decel / G0 + Pulse(T, C.TChute, C.GDrogueSnatch, 0.25) + Pulse(T, C.TMain, C.GMainSnatch, 0.3);
			}
			if (T < C.TTouch)
			{
				return 1.0 + (V885 - C.VTouch) / (C.TTouch - C.TSplashPhase) / G0 * JetsWindow(T);
			}
			const double X = T - C.TTouch;
			return 1.0 + Pulse(T, C.TTouch, C.GSplashSpike, 0.15) + 0.12 * std::exp(-X / 4.0) * std::sin(2.0 * Pi * X / 2.4);
		}
		double Shake(double T) const
		{
			double S = 0.0;
			if (T < C.TAscentEnd)
			{
				S = (0.30 + 0.25 * std::exp(-T / 6.0)) * AscentThrust(T);
			}
			else if (T < C.TEntry)
			{
				S = 0.15 * BurnWindow(T) + Pulse(T, C.TSep, 0.55, 0.35);
			}
			else if (T < C.TChute)
			{
				S = 0.95 * std::pow(std::max(EntryBell(T), Plasma(T) * 0.8), 0.6);
			}
			else if (T < C.TTouch)
			{
				S = 0.06 + Pulse(T, C.TChute, 0.9, 1.2) + Pulse(T, C.TMain, 0.7, 1.5) + 0.45 * JetsWindow(T);
			}
			else
			{
				S = 0.03 + Pulse(T, C.TTouch, 1.0, 1.3);
			}
			return Clamp(S, 0.0, 1.0);
		}
		double Plasma(double T) const
		{
			if (T <= C.TEntry || T >= C.TChute)
			{
				return 0.0;
			}
			const double S = T < C.TPlasmaPeak ? C.PlasmaSigmaRise : C.PlasmaSigmaFall, X = (T - C.TPlasmaPeak) / S;
			return std::exp(-0.5 * X * X) * Smooth(C.TEntry + 2.0, C.TEntry + 12.0, T) * (1.0 - Smooth(C.TChute - 14.0, C.TChute - 3.0, T));
		}
		double HullC(double T) const { return Table(Hull, T); }
		double Scorch(double T) const { return Table(Scorched, T); }
		double Whiteout(double T) const { return Smooth(C.TWhiteIn, C.TWhitePeak, T) * (1.0 - Smooth(C.TWhitePeak, C.TWhiteOut, T)); }

		// ---- the chute

		EChute ChuteStage(double T) const { return T < C.TChute ? EChute::None : (T < C.TMain ? EChute::Drogue : EChute::Main); }
		double ChuteInflation(double T) const
		{
			if (T < C.TChute)
			{
				return 0.0;
			}
			if (T < C.TMain)
			{
				return Smooth(C.TChute, C.TChute + 1.2, T);
			}
			return (0.35 * Smooth(C.TMain, C.TMain + 1.2, T) + 0.65 * Smooth(C.TMain + 4.0, C.TMain + 6.5, T)) * (1.0 - Smooth(C.TTouch + 0.5, C.TTouch + 5.0, T));
		}
		void Swing(double T, double& PitchDeg, double& RollDeg) const
		{
			PitchDeg = 0.0;
			RollDeg = 0.0;
			if (T < C.TChute)
			{
				return;
			}
			const double XD = T - C.TChute, XM = T - C.TMain;
			const double Dr = C.SwingDrogueDeg * std::exp(-XD / 8.0) * (XM > 0.0 ? std::exp(-XM / 1.5) : 1.0);
			const double Mn = XM > 0.0 ? (C.SwingMainDeg * std::exp(-XM / 10.0) + C.SwingRestDeg) * Smooth(0.0, 2.0, XM) : 0.0;
			const double Fade = 1.0 - Smooth(C.TTouch, C.TTouch + 0.4, T);
			PitchDeg = (Dr * std::sin(2.0 * Pi * XD / 2.6) + (XM > 0.0 ? Mn * std::sin(2.0 * Pi * XM / 5.2) : 0.0)) * Fade;
			RollDeg = (0.6 * Dr * std::sin(2.0 * Pi * XD / 3.1) + (XM > 0.0 ? 0.7 * Mn * std::sin(2.0 * Pi * XM / 4.7) : 0.0)) * Fade;
		}

		// ---- the track over the planet

		double TrackArcDeg(double T, double* Slope = nullptr) const { return C.Track.At(T, Slope); }
		double HorizonDeg(double T) const
		{
			const double H = Range(T);
			return Deg(std::acos(Clamp(C.PlanetRadiusKm / (C.PlanetRadiusKm + H), -1.0, 1.0)));
		}
		double IslandVisible(double T) const
		{
			if (T >= C.TTouch)
			{
				return 1.0;
			}
			const double B = HorizonDeg(T);
			return B < 1e-9 ? 1.0 : Smooth(0.0, 0.15, (B - TrackArcDeg(T)) / B);
		}
		/** From the planet's centre to the craft, in the flight frame. */
		FVec Up(double T) const { return RotY(U0, Arc0 - TrackArcDeg(T)); }

		// ---- the air

		double O2Pct(double T) const
		{
			const double X = std::fmod(T + 60.0, 270.0);
			return 21.0 - 0.8 * (X < 240.0 ? X / 240.0 : 1.0 - Smooth(240.0, 270.0, X));
		}
		double Co2Pct(double T) const
		{
			const double X = std::fmod(T + 40.0, 170.0);
			return 0.25 + 0.65 * (X < 150.0 ? X / 150.0 : 1.0 - Smooth(150.0, 170.0, X));
		}
		double Volts(double T) const
		{
			double V = 27.5 - 0.6 * AscentThrust(T) * (T < C.TAscentEnd ? 1.0 : 0.0) - 0.9 * BurnWindow(T) - 1.5 * Plasma(T) - 0.4 * JetsWindow(T);
			V -= Pulse(T, C.TSep, 0.8, 0.5) + Pulse(T, C.TChute, 0.6, 0.5) + Pulse(T, C.TMain, 0.6, 0.5);
			return V - 0.4 * Smooth(C.TTouch, C.TTouch + 3.0, T);
		}

		// ---- everything at T (the switches, the lamps, the record and the stateful needles are FFlight's)

		/** Sun: the star's direction in the flight frame F. */
		void Eval(double T, const FVec& SunF, FState& S) const
		{
			T = Clamp(T, 0.0, C.TEnd);
			S.T = T;
			double RangeRate = 0.0, ArcSlope = 0.0;
			S.RangeKm = Range(T, &RangeRate);
			S.RangeRateKms = RangeRate;
			S.AltSeaM = S.RangeKm * 1000.0;
			const double Arc = TrackArcDeg(T, &ArcSlope);
			const double Tangential = (C.PlanetRadiusKm + S.RangeKm) * Rad(std::fabs(ArcSlope));
			S.SpeedKms = std::sqrt(RangeRate * RangeRate + Tangential * Tangential);
			S.VSpeedMs = (T < C.TApproach ? -RangeRate : RangeRate) * 1000.0;
			S.GLoad = Load(T);
			S.Shake = Shake(T);
			S.Plasma = Plasma(T);
			S.PlasmaK = S.Plasma > 0.0 ? C.PlasmaKLow + (C.PlasmaKHigh - C.PlasmaKLow) * std::pow(S.Plasma, 0.6) : 0.0;
			S.Scorch = Scorch(T);
			S.HullC = HullC(T);
			S.Whiteout = Whiteout(T);
			S.bRealWorld = T >= C.TWhitePeak;
			S.Chute = ChuteStage(T);
			S.ChuteInflation = ChuteInflation(T);
			Swing(T, S.SwingPitchDeg, S.SwingRollDeg);
			S.ChuteYawDeg = C.ChuteTwistDeg * Smooth(C.TChute + 2.0, C.TSplashPhase, T);
			S.bEngine = T > 0.0 && T < C.TAscentEnd;
			S.bBurn = T >= C.TBurn && T < C.TBurn + C.BurnDur;
			S.bBlackout = T >= TBlackoutStart && T < TBlackoutEnd;

			// where things are, in the flight frame
			const FVec U = Up(T);
			const FVec Fwd = Cross(V3(0.0, 1.0, 0.0), U);
			const double Rp = C.PlanetRadiusKm;
			const FVec Craft = Mul(U, Rp + S.RangeKm), Pad = Mul(U0, Rp + RangeStartKm);
			const FVec FromPad = Sub(Craft, Pad);
			S.AltEuropaKm = Len(FromPad);
			const FVec EuropaF = Norm(Sub(V3(0.0, 0.0, -C.EuropaRadiusKm), FromPad));
			const FVec PlanetDirF = Mul(U, -1.0);
			S.PlanetDiaDeg = 2.0 * Deg(std::asin(Clamp(Rp / (Rp + S.RangeKm), -1.0, 1.0)));
			S.EuropaDiaDeg = 2.0 * Deg(std::asin(Clamp(C.EuropaRadiusKm / (C.EuropaRadiusKm + S.AltEuropaKm), -1.0, 1.0)));
			S.EuropaVisible = Smooth(-0.5 * S.EuropaDiaDeg, 0.5 * S.EuropaDiaDeg, AngleDeg(EuropaF, PlanetDirF) - 0.5 * S.PlanetDiaDeg);
			const FVec IslandF = Norm(Sub(Mul(UIsland, Rp), Craft));

			// the attitude: on the pad and in the ascent upright in F (tilting toward the planet), then nose on Europa
			// (the look back), then the turn to the planet's own up-and-forward frame L for the rest
			const double WLook = Smooth(C.TLookStart, C.TLookEnd, T), WTurn = Smooth(C.TTurnStart, C.TTurnEnd, T);
			const double WApp = Smooth(C.TApproach - 10.0, C.TApproach + 25.0, T), WEntry = Smooth(C.TSep + 2.0, C.TEntry - 2.0, T);
			const double WChute = Smooth(C.TChute - 2.0, C.TChute + 6.0, T);
			const double PitchA = C.AscentPitchDeg * Smooth(15.0, C.TAscentEnd - 5.0, T);
			const double PitchE = Deg(std::atan2(EuropaF.Z, EuropaF.X));
			const double Half = 0.5 * S.PlanetDiaDeg;
			double PitchL = Lerp(-90.0, -(C.ApproachLimbDeg - Half), WApp);
			PitchL = Lerp(PitchL, -(C.EntryLimbDeg - Half), WEntry);
			PitchL = Lerp(PitchL, 0.0, WChute) + S.SwingPitchDeg;
			const double YawL = 180.0 * WEntry + S.ChuteYawDeg;
			const double RollL = 360.0 * C.CoastRollTurns * Smooth(C.TTurnEnd + 10.0, C.TApproach - C.NapEndBefore, T) + S.SwingRollDeg;
			FMat Att = Euler(0.0, PitchA, 0.0);
			if (WLook > 0.0 && WTurn < 1.0)
			{
				FMat Look;
				Look.X = EuropaF;
				Look.Y = V3(0.0, 1.0, 0.0);
				Look.Z = Cross(Look.X, Look.Y);
				Att = Slerp(Att, Look, WLook);
			}
			if (WTurn > 0.0)
			{
				FMat L;
				L.X = Fwd;
				L.Y = V3(0.0, 1.0, 0.0);
				L.Z = U;
				Att = Slerp(Att, MulMat(L, Euler(YawL, PitchL, RollL)), WTurn);
			}
			S.PitchDeg = Lerp(Lerp(PitchA, PitchE, WLook), PitchL, WTurn);
			S.YawDeg = YawL * WTurn;
			S.RollDeg = RollL * WTurn;
			S.PlanetDir = ToLocal(Att, PlanetDirF);
			S.PlanetIslandAxis = ToLocal(Att, UIsland);
			S.PlanetNorthAxis = ToLocal(Att, North);
			S.EuropaDir = ToLocal(Att, EuropaF);
			S.SunDir = ToLocal(Att, SunF);
			S.IslandDir = ToLocal(Att, IslandF);
			S.FrameX = ToLocal(Att, V3(1.0, 0.0, 0.0));
			S.FrameY = ToLocal(Att, V3(0.0, 1.0, 0.0));
			S.FrameZ = ToLocal(Att, V3(0.0, 0.0, 1.0));

			// the load's direction: into the couch (-Z), a little aft-tilted in the entry, true down under the canopy
			if (T < C.TChute)
			{
				const double Tilt = -C.EntryTilt * Smooth(C.TEntry, C.TEntry + 15.0, T) * (1.0 - Smooth(C.TChute - 10.0, C.TChute, T));
				S.G = Mul(Norm(V3(Tilt, 0.0, -1.0)), S.GLoad);
			}
			else
			{
				S.G = Mul(ToLocal(Att, PlanetDirF), S.GLoad);
			}

			// the track
			S.TrackArcDeg = Arc;
			S.IslandRangeKm = Rp * Rad(Arc);
			S.IslandVisible = IslandVisible(T);
			S.SubLatDeg = Deg(std::asin(Clamp(Dot(U, North), -1.0, 1.0)));
			S.SubLonRelDeg = Deg(std::atan2(Dot(U, PlanetY), Dot(U, PlanetX)));
			const FVec NorthHere = Norm(Sub(North, Mul(U, Dot(North, U)))), EastHere = Cross(NorthHere, U);
			const FVec ToIsland = Sub(UIsland, Mul(U, Dot(UIsland, U)));
			S.IslandBearingDeg = Len(ToIsland) < 1e-9 ? C.ArrivalHeadingDeg : Deg(std::atan2(Dot(ToIsland, EastHere), Dot(ToIsland, NorthHere)));
			if (S.IslandBearingDeg < 0.0)
			{
				S.IslandBearingDeg += 360.0;
			}

			// the instruments that are functions of the time
			FInstruments& I = S.I;
			I.O2Pct = O2Pct(T);
			I.Co2Pct = Co2Pct(T);
			I.TempC = C.CabinTemp.At(T);
			I.Volts = Volts(T);
			I.FuelPct = C.Fuel.At(T);
			I.HullC = S.HullC;
			I.Hull01 = Clamp((S.HullC - C.HullSpaceC) / (C.HullPeakC - C.HullSpaceC), 0.0, 1.0);
			I.GMeter = Clamp(S.GLoad, 0.0, 6.0);
			I.ClockSec = T;
			const int Sec = (int)std::floor(T + 1e-9);
			const int HMS[3] = { Sec / 3600, (Sec / 60) % 60, Sec % 60 };
			for (int K = 0; K < 3; ++K)
			{
				I.ClockDigits[K * 2] = HMS[K] / 10 % 10;
				I.ClockDigits[K * 2 + 1] = HMS[K] % 10;
			}
			I.RangeKm = (int)std::lround(S.RangeKm);
			I.bSpeedMs = T >= C.TChute;
			I.SpeedShown = I.bSpeedMs ? S.SpeedKms * 1000.0 : S.SpeedKms;
			I.bAltSea = T >= C.TApproach;
			I.AltDialKm = I.bAltSea ? S.RangeKm : S.AltEuropaKm;
			I.GlobeLatDeg = S.SubLatDeg;
			I.GlobeLonRelDeg = S.SubLonRelDeg;
			CrtPath(0.0, I.CrtEuropaX, I.CrtEuropaY);
			I.CrtEuropaX -= 0.045;
			I.CrtEuropaY -= 0.02;
			I.CrtEuropaR = 0.05;
			I.CrtPlanetX = 0.68;
			I.CrtPlanetY = 0.56;
			I.CrtPlanetR = 0.16;
			I.CrtProgress = T / C.TEnd;
			CrtPath(I.CrtProgress, I.CrtDotX, I.CrtDotY);
		}

	private:
		static constexpr double Step = 0.1;
		double A0 = 0.0, RateEntry = 0.0, V825 = 0.0, V885 = 0.0, Arc0 = 0.0;
		std::vector<double> Rate, Cum, Hull, Scorched;
		FVec PlanetF, U0, UIsland, North, PlanetX, PlanetY;

		/** km of range per unit of the log of the planet's diameter, at the half-angle A. */
		double Stiff(double A) const { return C.PlanetRadiusKm * A * std::cos(A) / (std::sin(A) * std::sin(A)); }
		double LnDia(double T, double& OutRate) const
		{
			const double X = Clamp(T, 0.0, C.TEntry) / Step;
			const size_t K = std::min((size_t)X, Rate.size() - 2);
			const double F = (X - (double)K) * Step;
			OutRate = Rate[K] + (Rate[K + 1] - Rate[K]) * F / Step;
			return Cum[K] + Rate[K] * F + (Rate[K + 1] - Rate[K]) * F * F / (2.0 * Step);
		}
		double Table(const std::vector<double>& Tb, double T) const
		{
			const double X = Clamp(T, 0.0, C.TEnd) / Step;
			const size_t K = std::min((size_t)X, Tb.size() - 2);
			return Lerp(Tb[K], Tb[K + 1], X - (double)K);
		}
		static FVec RotY(const FVec& V, double AngleDegrees)
		{
			const double Cs = std::cos(Rad(AngleDegrees)), Sn = std::sin(Rad(AngleDegrees));
			return V3(V.X * Cs + V.Z * Sn, V.Y, -V.X * Sn + V.Z * Cs);
		}
		/** The first moment in [A, B] where Fn turns positive (0.05 s scan, then bisection). */
		template <typename TFn>
		static double Cross1(TFn&& Fn, double A, double B)
		{
			double Prev = A;
			for (double T = A; T <= B; T += 0.05)
			{
				if (Fn(T) > 0.0)
				{
					double Lo = Prev, Hi = T;
					for (int K = 0; K < 30; ++K)
					{
						const double Mid = 0.5 * (Lo + Hi);
						(Fn(Mid) > 0.0 ? Hi : Lo) = Mid;
					}
					return Hi;
				}
				Prev = T;
			}
			return B;
		}
	};

	// ------------------------------------------------------------------ the craft, the hero in it, the save

	struct FRecord
	{
		std::string Id;
		double LengthSec = 180.0;
	};

	class FFlight
	{
	public:
		FProfile P;

		// ---- what the engine tells every tick (or when it changes)
		bool bSeated = false;       // the hero is on his couch
		bool bBuckled = false;
		bool bHatchShut = false;
		bool bWheelSealed = false;  // the hatch wheel is turned home
		double HeroDistM = 0.0;     // afloat: how far the hero is from the capsule
		long long GameDay = 0;      // the game's day number
		FVec SunF = Norm(V3(-0.30, 0.75, 0.59)); // the star in the flight frame; set it before the launch (kept from then)
		double Fast = 1.0;          // lyud.Flight.Fast: the whole model runs this many times faster (not saved)
		// dev: faults (the two lamps a nominal flight never lights)
		bool bFaultO2 = false, bFaultBatt = false;

		FFlight() { Reset(); }
		explicit FFlight(const FConfig& Cfg) : P(Cfg) { Reset(); }

		/** A new game: the craft on Europa, cold; nothing flown. */
		void Reset()
		{
			ResetCraft();
			Flights = 0;
			Events.clear();
		}

		// ---- reading

		EPhase Phase() const { return Ph; }
		double Time() const { return T; }
		bool Flying() const { return Ph >= EPhase::Ascent && Ph <= EPhase::Splash; }
		bool Napping() const { return bNap; }
		int FlightsFlown() const { return Flights; }
		/** The cabin stands on its stage on Europa (parked or prelaunch). */
		bool CraftOnEuropa() const { return Ph == EPhase::Parked || Ph == EPhase::Prelaunch; }
		/** The capsule bobs on the real sea. */
		bool CapsuleAfloat() const { return Ph == EPhase::Afloat; }
		bool Switch(int S) const { return S >= 0 && S < SwitchCount && Sw[S]; }
		bool Powered() const { return Sw[SwPower]; }
		double NapSleptSec() const { return LastNapSec; }
		const std::vector<FRecord>& Records() const { return Recs; }

		/** The whole picture of this moment. */
		FState State() const
		{
			FState S;
			P.Eval(Flying() || Ph == EPhase::Afloat ? T : 0.0, SunF, S);
			S.Phase = Ph;
			S.bFlying = Flying();
			S.bNap = bNap;
			S.TimeScale = Flying() ? Fast * (bNap ? P.C.NapScale : 1.0) : Fast;
			S.PhaseProgress = Flying() ? Clamp((T - P.PhaseStart(Ph)) / std::max(1e-9, P.PhaseEnd(Ph) - P.PhaseStart(Ph)), 0.0, 1.0) : 0.0;
			if (!Flying())
			{
				S.bEngine = S.bBurn = S.bBlackout = false;
			}
			if (Ph == EPhase::Afloat)
			{
				S.GLoad = 1.0;
				S.G = V3(0.0, 0.0, -1.0);
				S.Shake = 0.0;
				S.I.GMeter = 1.0;
			}
			else if (!Flying())
			{
				S.Scorch = 0.0;
			}
			for (int K = 0; K < SwitchCount; ++K)
			{
				S.Switches[K] = Sw[K];
			}
			FInstruments& I = S.I;
			I.bPowered = Sw[SwPower];
			I.bLampTest = Sw[SwPower] && LampTestT > 0.0;
			I.PressMmHg = Press;
			if (bFaultO2) { I.O2Pct = 17.0; }
			if (bFaultBatt) { I.Volts = 23.9; }
			I.CrtPhase = (int)Ph;
			I.CrtMode = CrtMode;
			I.bCabinLight = bLight && Sw[SwPower];
			I.bRadio = bRadio && Sw[SwPower];
			Lamps(S);
			S.Record = RecordState(S);
			return S;
		}

		// ---- the checklist

		/** Flip one of the five switches. "" when it went; otherwise the reason's string key (and nothing changed). */
		std::string Toggle(int S)
		{
			if (S < 0 || S >= SwitchCount)
			{
				return "ui.flight.no.order";
			}
			if (Flying() || Ph == EPhase::Afloat)
			{
				return "ui.flight.no.switch_flight";
			}
			if (!bSeated)
			{
				return "ui.flight.no.not_seated";
			}
			if (!Sw[S])
			{
				for (int K = 0; K < S; ++K)
				{
					if (!Sw[K])
					{
						return Refuse(std::string("ui.flight.no.order.") + SwitchName(S));
					}
				}
				if (S == SwSeal && !bHatchShut)
				{
					return Refuse("ui.flight.no.hatch_open");
				}
				if (S == SwSeal && !bWheelSealed)
				{
					return Refuse("ui.flight.no.hatch_unsealed");
				}
				Sw[S] = true;
				Emit(CueSound, "switch_on", SwitchName(S));
				if (S == SwPower)
				{
					LampTestT = P.C.LampTest;
				}
				if (S == SwArm)
				{
					Emit(CueAuto, "auto.ready", "");
				}
				return "";
			}
			for (int K = S + 1; K < SwitchCount; ++K)
			{
				if (Sw[K])
				{
					return Refuse(S == SwPower ? "ui.flight.no.order.power" : "ui.flight.no.order");
				}
			}
			Sw[S] = false;
			Emit(CueSound, "switch_off", SwitchName(S));
			if (S == SwGyro)
			{
				GyroSpin = 0.0;
			}
			if (S == SwPower)
			{
				LampTestT = 0.0;
				CautionT = 0.0;
			}
			return "";
		}

		/** The ПУСК button. "" when the craft lifts off. */
		std::string Launch()
		{
			if (Flying() || Ph == EPhase::Afloat)
			{
				return "ui.flight.no.switch_flight";
			}
			if (!bSeated)
			{
				return "ui.flight.no.not_seated";
			}
			for (int K = 0; K < SwitchCount; ++K)
			{
				if (!Sw[K])
				{
					return Refuse("ui.flight.no.order.launch");
				}
			}
			if (!bHatchShut || !bWheelSealed)
			{
				return Refuse("ui.flight.no.hatch_open");
			}
			if (!bBuckled)
			{
				return "ui.flight.no.not_buckled";
			}
			Ph = EPhase::Ascent;
			T = 0.0;
			NextCue = 0;
			Emit(CueSound, "launch", "");
			FireCues();
			return "";
		}

		/** The comforts: they are plain toggles (they act when the power is on). */
		void ToggleLight() { bLight = !bLight; Emit(CueSound, "switch_click", "light"); }
		void ToggleRadio() { bRadio = !bRadio; Emit(CueSound, "switch_click", "radio"); }
		void NextCrtMode() { CrtMode = (CrtMode + 1) % 3; Emit(CueSound, "switch_click", "crt"); }

		/** May the hero do this now? "" yes; otherwise the reason's key. */
		std::string CanOpenHatch() const
		{
			return Flying() ? "ui.flight.no.hatch_flight" : (Sw[SwSeal] && Ph != EPhase::Afloat ? "ui.flight.no.order" : "");
		}
		std::string CanUnbuckle() const { return Flying() && P.Load(T) > 0.6 ? "ui.flight.no.unbuckle_g" : ""; }
		std::string CanStand() const
		{
			const bool bBusy = Flying() && (Ph != EPhase::Coast || P.Load(T) > 0.05 || bNap);
			return bBusy ? "ui.flight.no.stand_now" : "";
		}
		std::string CanSwimOut() const { return Flying() ? "ui.flight.no.swim_early" : ""; }
		/** The red handle overhead: it never does anything. */
		std::string PullChuteHandle() const { return Flying() && T >= P.C.TChute ? "ui.flight.no.chute_late" : (Ph == EPhase::Afloat ? "ui.flight.no.chute_late" : "ui.flight.no.chute_early"); }

		// ---- the nap

		std::string StartNap()
		{
			const FConfig& C = P.C;
			if (bNap)
			{
				return "";
			}
			if (Ph != EPhase::Coast || T > C.TApproach - C.NapLatestBefore || (T > C.TBurn - C.NapBurnGuard && T < C.TBurn + C.BurnDur + 2.0))
			{
				return "ui.flight.no.nap_now";
			}
			if (!bBuckled)
			{
				return "ui.flight.no.nap_unbuckled";
			}
			bNap = true;
			NapFromT = T;
			Emit(CueSound, "nap_start", "");
			return "";
		}
		void EndNap()
		{
			if (bNap)
			{
				bNap = false;
				LastNapSec = T - NapFromT;
				Emit(CueSound, "nap_end", "");
			}
		}

		// ---- the record player

		/** The sleeves in the rack, in the order the hero goes through them. The record on the platter is kept by its id. */
		void SetRecords(const std::vector<FRecord>& In)
		{
			Recs = In;
			RecIndex = -1;
			for (size_t K = 0; K < Recs.size(); ++K)
			{
				if (!RecId.empty() && Recs[K].Id == RecId)
				{
					RecIndex = (int)K;
				}
			}
			if (RecIndex < 0)
			{
				RecId.clear();
				bRecPlaying = false;
				RecPos = 0.0;
			}
		}
		/** The hero's turn through the rack: the next record goes on and the needle down. */
		std::string RecordNext() { return RecordPut(Recs.empty() ? 0 : (RecIndex + 1) % (int)Recs.size()); }
		std::string RecordPut(int Index)
		{
			if (Recs.empty() || Index < 0 || Index >= (int)Recs.size())
			{
				return "ui.flight.no.no_records";
			}
			if (Ph == EPhase::Entry)
			{
				return "ui.flight.no.entry_player";
			}
			RecIndex = Index;
			RecId = Recs[(size_t)Index].Id;
			RecPos = 0.0;
			bRecPlaying = true;
			Emit(CueSound, "record_start", RecId);
			return "";
		}
		/** The needle down on the record that is on the platter (from where it was lifted; from the start after a side's end). */
		std::string RecordNeedleDown()
		{
			if (RecIndex < 0)
			{
				return "ui.flight.no.no_records";
			}
			if (Ph == EPhase::Entry)
			{
				return "ui.flight.no.entry_player";
			}
			if (!bRecPlaying)
			{
				if (RecPos >= Recs[(size_t)RecIndex].LengthSec)
				{
					RecPos = 0.0;
				}
				bRecPlaying = true;
				Emit(CueSound, "record_start", RecId);
			}
			return "";
		}
		void RecordStop()
		{
			if (bRecPlaying)
			{
				bRecPlaying = false;
				Emit(CueSound, "record_stop", RecId);
			}
		}

		// ---- time

		/** Dt: real seconds. Fires the cues into the queue (TakeEvents). */
		void Advance(double Dt)
		{
			const FConfig& C = P.C;
			Dt = std::max(0.0, Dt) * std::max(0.0, Fast);
			if (Dt <= 0.0)
			{
				return;
			}
			Ui += Dt;
			LampTestT = std::max(0.0, LampTestT - Dt);
			CautionT = std::max(0.0, CautionT - Dt);
			GyroSpin = Sw[SwGyro] ? std::min(1.0, GyroSpin + Dt / std::max(0.01, C.GyroSpinUp)) : 0.0;
			if (!Flying() && Ph != EPhase::Afloat)
			{
				Ph = bSeated ? EPhase::Prelaunch : EPhase::Parked;
			}
			const double T0 = T;
			if (Flying())
			{
				double Left = Dt;
				while (Left > 1e-12 && Flying())
				{
					const double NapEnd = C.TApproach - C.NapEndBefore;
					double Step = Left, Scale = 1.0;
					if (bNap)
					{
						Scale = C.NapScale;
						Step = std::min(Left, std::max(0.0, (NapEnd - T) / Scale));
					}
					const double From = T;
					T = std::min(C.TEnd, T + Step * Scale);
					Left -= Step;
					FireCues();
					NeedleSkips(From, T);
					if (bNap && T >= NapEnd - 1e-9)
					{
						EndNap();
						Emit(CueSound, "nap_wake", "");
					}
					if (T >= C.TEnd)
					{
						Ph = EPhase::Afloat;
						Flights++;
						SplashDay = GameDay;
						FarT = 0.0;
						Emit(CueSound, "afloat", "");
					}
					else
					{
						Ph = P.PhaseAt(T);
					}
				}
			}
			else if (Ph == EPhase::Afloat)
			{
				FarT = HeroDistM > C.GoneDistM ? FarT + Dt : 0.0;
				if (!bSaidAfloat && !bHatchShut)
				{
					bSaidAfloat = true;
					Emit(CueSay, "line.hero.flight_afloat.1", "");
				}
				if (FarT >= C.GoneSeconds || GameDay > SplashDay)
				{
					Emit(CueSound, "capsule_gone", "");
					ResetCraft();
					Emit(CueSound, "craft_back", "");
				}
			}
			// the cabin's air: the pressure follows what the hatch and the air switch allow
			{
				const bool bTight = bHatchShut && bWheelSealed;
				const double Outside = Ph == EPhase::Afloat ? C.PressNominal : 0.0;
				double Target = Press;
				if (Flying())
				{
					Target = C.PressNominal + 2.2 * (C.CabinTemp.At(T) - C.CabinTemp.At(0.0));
				}
				else if (!bTight)
				{
					Target = Outside;
				}
				else if (Sw[SwAir])
				{
					Target = C.PressNominal;
				}
				Press += (Target - Press) * (1.0 - std::exp(-Dt / 3.0));
			}
			// the record turns in real time; the platter drags under load
			if (bRecPlaying && RecIndex >= 0)
			{
				const double TM = Flying() || Ph == EPhase::Afloat ? 0.5 * (T0 + T) : 0.0;
				RecPos += Dt * RecordPitch(Flying() ? P.Load(TM) : 1.0, Flying() ? P.Shake(TM) : 0.0);
				if (RecPos >= Recs[(size_t)RecIndex].LengthSec)
				{
					RecPos = Recs[(size_t)RecIndex].LengthSec;
					bRecPlaying = false;
					Emit(CueSound, "record_side_end", RecId);
				}
			}
		}

		/** The cues since the last call, oldest first. */
		std::vector<FEvent> TakeEvents()
		{
			std::vector<FEvent> Out;
			Out.swap(Events);
			return Out;
		}

		// ---- dev (the F7 panel, lyud.Flight.*)

		/** Jump to a flight second (0..900): launched, the cues before it counted as fired. */
		void DevJump(double InT)
		{
			for (int K = 0; K < SwitchCount; ++K)
			{
				Sw[K] = true;
			}
			GyroSpin = 1.0;
			Press = P.C.PressNominal;
			bNap = false;
			T = Clamp(InT, 0.0, P.C.TEnd - 0.001);
			Ph = P.PhaseAt(T);
			NextCue = 0;
			while (NextCue < (int)P.Cues.size() && P.Cues[(size_t)NextCue].T < T)
			{
				NextCue++;
			}
		}
		/** The craft back on Europa, cold (the count of flights stays). */
		void DevReset() { ResetCraft(); }

		// ---- the save: "v1;key=value;..." (the records' list is the engine's; the record on the platter is kept by id)

		std::string Save() const
		{
			std::string O = "v1";
			auto Num = [&O](const char* K, double V)
			{
				char B[64];
				std::snprintf(B, sizeof B, ";%s=%.17g", K, V);
				O += B;
			};
			auto Int = [&O](const char* K, long long V)
			{
				char B[64];
				std::snprintf(B, sizeof B, ";%s=%lld", K, V);
				O += B;
			};
			int Bits = 0;
			for (int K = 0; K < SwitchCount; ++K)
			{
				Bits |= Sw[K] ? 1 << K : 0;
			}
			Int("ph", (int)Ph); Num("t", T); Int("cue", NextCue); Int("sw", Bits); Int("nap", bNap ? 1 : 0); Num("napt", NapFromT); Num("naps", LastNapSec);
			Num("press", Press); Num("gyro", GyroSpin); Num("ltest", LampTestT); Num("caut", CautionT); Num("ui", Ui);
			Int("fl", Flights); Int("sday", SplashDay); Int("gday", GameDay); Num("far", FarT); Num("dist", HeroDistM);
			Int("in", (bSeated ? 1 : 0) | (bBuckled ? 2 : 0) | (bHatchShut ? 4 : 0) | (bWheelSealed ? 8 : 0));
			Int("cmf", (bLight ? 1 : 0) | (bRadio ? 2 : 0) | (CrtMode << 2));
			Int("flt", (bFaultO2 ? 1 : 0) | (bFaultBatt ? 2 : 0) | (bSaidAfloat ? 4 : 0));
			Num("sx", SunF.X); Num("sy", SunF.Y); Num("sz", SunF.Z);
			Num("rpos", RecPos); Int("rplay", bRecPlaying ? 1 : 0);
			O += ";rec=" + RecId;
			return O;
		}

		/** False (and a cold craft) when the text is not a save of this model. Call SetRecords before or after. */
		bool Load(const std::string& Text)
		{
			const std::vector<FRecord> Keep = Recs;
			Reset();
			Recs = Keep;
			if (Text.compare(0, 2, "v1") != 0 || (Text.size() > 2 && Text[2] != ';'))
			{
				return false;
			}
			size_t At = 2;
			std::string SavedRec;
			while (At < Text.size())
			{
				size_t End = Text.find(';', At + 1);
				End = End == std::string::npos ? Text.size() : End;
				const std::string Pair = Text.substr(At + 1, End - At - 1);
				At = End;
				const size_t Eq = Pair.find('=');
				if (Eq == std::string::npos)
				{
					continue;
				}
				const std::string K = Pair.substr(0, Eq), V = Pair.substr(Eq + 1);
				const double D = std::strtod(V.c_str(), nullptr);
				const long long N = std::strtoll(V.c_str(), nullptr, 10);
				if (K == "ph") { Ph = (EPhase)Clamp((double)N, 0.0, (double)(PhaseCount - 1)); }
				else if (K == "t") { T = Clamp(D, 0.0, P.C.TEnd); }
				else if (K == "cue") { NextCue = (int)Clamp((double)N, 0.0, (double)P.Cues.size()); }
				else if (K == "sw") { for (int S = 0; S < SwitchCount; ++S) { Sw[S] = (N >> S & 1) != 0; } }
				else if (K == "nap") { bNap = N != 0; }
				else if (K == "napt") { NapFromT = D; }
				else if (K == "naps") { LastNapSec = D; }
				else if (K == "press") { Press = D; }
				else if (K == "gyro") { GyroSpin = Clamp(D, 0.0, 1.0); }
				else if (K == "ltest") { LampTestT = D; }
				else if (K == "caut") { CautionT = D; }
				else if (K == "ui") { Ui = D; }
				else if (K == "fl") { Flights = (int)N; }
				else if (K == "sday") { SplashDay = N; }
				else if (K == "gday") { GameDay = N; }
				else if (K == "far") { FarT = D; }
				else if (K == "dist") { HeroDistM = D; }
				else if (K == "in") { bSeated = (N & 1) != 0; bBuckled = (N & 2) != 0; bHatchShut = (N & 4) != 0; bWheelSealed = (N & 8) != 0; }
				else if (K == "cmf") { bLight = (N & 1) != 0; bRadio = (N & 2) != 0; CrtMode = (int)(N >> 2) % 3; }
				else if (K == "flt") { bFaultO2 = (N & 1) != 0; bFaultBatt = (N & 2) != 0; bSaidAfloat = (N & 4) != 0; }
				else if (K == "sx") { SunF.X = D; }
				else if (K == "sy") { SunF.Y = D; }
				else if (K == "sz") { SunF.Z = D; }
				else if (K == "rpos") { RecPos = D; }
				else if (K == "rplay") { bRecPlaying = N != 0; }
				else if (K == "rec") { SavedRec = V; }
			}
			if (Flying())
			{
				Ph = P.PhaseAt(T);
			}
			bNap = bNap && Ph == EPhase::Coast;
			RecId = SavedRec;
			const double Pos = RecPos;
			const bool bPlay = bRecPlaying;
			if (!Recs.empty())
			{
				SetRecords(Recs);
				if (RecIndex >= 0)
				{
					RecPos = Pos;
					bRecPlaying = bPlay;
				}
			}
			return true;
		}

	private:
		EPhase Ph = EPhase::Parked;
		double T = 0.0;
		int NextCue = 0;
		bool Sw[SwitchCount] = { false, false, false, false, false };
		bool bNap = false;
		double NapFromT = 0.0, LastNapSec = 0.0;
		double Press = 0.0, GyroSpin = 0.0, LampTestT = 0.0, CautionT = 0.0, Ui = 0.0;
		int Flights = 0;
		long long SplashDay = 0;
		double FarT = 0.0;
		bool bLight = true, bRadio = false, bSaidAfloat = false;
		int CrtMode = 0;
		std::vector<FRecord> Recs;
		int RecIndex = -1;
		std::string RecId;
		double RecPos = 0.0;
		bool bRecPlaying = false;
		std::vector<FEvent> Events;

		void ResetCraft()
		{
			Ph = EPhase::Parked;
			T = 0.0;
			NextCue = 0;
			for (bool& S : Sw)
			{
				S = false;
			}
			bNap = false;
			NapFromT = LastNapSec = 0.0;
			Press = GyroSpin = LampTestT = CautionT = 0.0;
			FarT = 0.0;
			bSaidAfloat = false;
			bRecPlaying = false;
			RecPos = 0.0;
			RecIndex = -1;
			RecId.clear();
		}
		void Emit(int Kind, const std::string& Id, const std::string& Arg)
		{
			FEvent E;
			E.Kind = Kind;
			E.Id = Id;
			E.Arg = Arg;
			E.T = T;
			E.bAsleep = bNap;
			Events.push_back(E);
		}
		std::string Refuse(const std::string& Reason)
		{
			CautionT = P.C.CautionSeconds;
			Emit(CueSound, "buzzer", Reason);
			return Reason;
		}
		void FireCues()
		{
			while (NextCue < (int)P.Cues.size() && P.Cues[(size_t)NextCue].T <= T)
			{
				const FCue& Q = P.Cues[(size_t)NextCue++];
				if (bNap && Q.Kind == CueSay)
				{
					continue; // the hero is asleep: he says nothing
				}
				FEvent E;
				E.Kind = Q.Kind;
				E.Id = Q.Id;
				E.T = Q.T;
				E.bAsleep = bNap;
				Events.push_back(E);
			}
		}
		/** The needle jumps at fixed moments (every SkipSlot seconds of the flight) when the cabin shakes hard enough. */
		void NeedleSkips(double From, double To)
		{
			if (!bRecPlaying || RecIndex < 0 || P.C.SkipSlot <= 0.0)
			{
				return;
			}
			for (long long K = (long long)std::floor(From / P.C.SkipSlot) + 1; K * P.C.SkipSlot <= To; ++K)
			{
				if (P.Shake((double)K * P.C.SkipSlot) > P.C.SkipShake)
				{
					uint32_t H = (uint32_t)K * 2654435761u;
					H ^= H >> 15;
					const double Jump = -3.0 + 7.0 * (double)(H % 1000u) / 999.0;
					RecPos = Clamp(RecPos + Jump, 0.0, std::max(0.0, Recs[(size_t)RecIndex].LengthSec - 0.5));
					Emit(CueSound, "needle_skip", RecId);
				}
			}
		}
		static double RecordPitch(double Ld, double Shk) { return 1.0 - 0.02 * Clamp(Ld - 1.0, 0.0, 4.0) - 0.01 * Shk; }
		FRecordState RecordState(const FState& S) const
		{
			FRecordState R;
			R.Index = RecIndex;
			R.Id = RecId;
			R.bPlaying = bRecPlaying;
			R.PosSec = RecPos;
			R.LengthSec = RecIndex >= 0 && RecIndex < (int)Recs.size() ? Recs[(size_t)RecIndex].LengthSec : 0.0;
			const double Ld = Flying() ? S.GLoad : 1.0, Shk = Flying() ? S.Shake : 0.0;
			R.Pitch = RecordPitch(Ld, Shk);
			R.Wow = Clamp(0.6 * Shk + 0.08 * std::max(0.0, Ld - 1.5), 0.0, 1.0);
			return R;
		}
		void Lamps(FState& S) const
		{
			const FConfig& C = P.C;
			const bool bFly = Flying(), bDown = Ph == EPhase::Afloat, bAir = bFly || bDown;
			const FInstruments& I = S.I;
			auto Set = [&S](int L, bool bOn, bool bBlink = false)
			{
				S.Lamps[L].bOn = bOn;
				S.Lamps[L].bBlink = bOn && bBlink;
			};
			Set(LampPower, Sw[SwPower]);
			Set(LampAir, Sw[SwAir], Press < C.PressOk);
			Set(LampGyro, Sw[SwGyro], GyroSpin < 1.0);
			Set(LampHatch, Sw[SwSeal]);
			Set(LampArmed, Sw[SwArm] && !bAir, true);
			Set(LampEngine, bFly && T < C.TAscentEnd);
			Set(LampSep, bFly && ((T >= C.TStageLeft && T < C.TStageLeft + 4.0) || (T >= C.TSep && T < C.TSep + 8.0)));
			Set(LampZeroG, bFly && T < C.TEntry && S.GLoad < 0.02);
			Set(LampBurn, bFly && T >= C.TBurn - 10.0 && T < C.TBurn + C.BurnDur, T < C.TBurn);
			Set(LampComm, !(bFly && S.bBlackout), bFly && T >= P.TBlackoutEnd && T < P.TBlackoutEnd + 5.0);
			Set(LampBlackout, bFly && S.bBlackout);
			Set(LampGHigh, bFly && S.GLoad > C.LampGHigh);
			Set(LampHullHot, bFly && S.HullC > C.LampHullHot, S.HullC > C.LampHullBlink);
			Set(LampDrogue, bFly && T >= C.TChute && T < C.TMain);
			Set(LampMainChute, bFly && T >= C.TMain && T < C.TTouch + 5.0);
			Set(LampAltLow, bFly && T >= C.TMain && T < C.TTouch && S.AltSeaM < C.LampAltLowM, true);
			Set(LampJets, bFly && T >= C.TSplashPhase && T < C.TTouch + 0.5);
			Set(LampSplash, bDown || (bFly && T >= C.TTouch));
			Set(LampBeacon, bDown || (bFly && T >= C.TTouch + 2.0), true);
			Set(LampO2Low, I.O2Pct < C.LampO2Low);
			Set(LampCo2High, I.Co2Pct > C.LampCo2High);
			Set(LampBattLow, I.Volts < C.LampBattLow);
			const bool bRed = S.Lamps[LampGHigh].bOn || S.Lamps[LampHullHot].bOn || S.Lamps[LampO2Low].bOn || S.Lamps[LampBattLow].bOn;
			Set(LampMasterCaution, bRed || CautionT > 0.0, !bRed);
			Set(LampRecord, bFly);
			const bool bPhase = std::fmod(Ui, std::max(0.05, C.BlinkPeriod)) < 0.5 * std::max(0.05, C.BlinkPeriod);
			for (FLampState& L : S.Lamps)
			{
				if (!Sw[SwPower])
				{
					L.bOn = L.bBlink = L.bLit = false;
				}
				else
				{
					L.bLit = I.bLampTest || (L.bOn && (!L.bBlink || bPhase));
				}
			}
		}
	};

	// ------------------------------------------------------------------ for the checks and a debug page

	struct FNamed
	{
		const char* Name;
		double V;
		bool bContinuous; // a real quantity that moves smoothly (but for the jolts); false: a flag, a digit, a stepped value
	};

	/** Every number of a state, named. */
	inline void Flatten(const FState& S, std::vector<FNamed>& Out)
	{
		Out.clear();
		auto Put = [&Out](const char* N, double V, bool bC = true)
		{
			FNamed F;
			F.Name = N;
			F.V = V;
			F.bContinuous = bC;
			Out.push_back(F);
		};
		auto Vec = [&Put](const char* NX, const char* NY, const char* NZ, const FVec& V)
		{
			Put(NX, V.X);
			Put(NY, V.Y);
			Put(NZ, V.Z);
		};
		Put("phase", (double)(int)S.Phase, false); Put("t", S.T); Put("phase_progress", S.PhaseProgress, false);
		Put("time_scale", S.TimeScale, false); Put("nap", S.bNap, false); Put("flying", S.bFlying, false);
		Put("engine", S.bEngine, false); Put("burn", S.bBurn, false); Put("blackout", S.bBlackout, false);
		Put("range_km", S.RangeKm); Put("range_rate_kms", S.RangeRateKms); Put("alt_europa_km", S.AltEuropaKm); Put("speed_kms", S.SpeedKms);
		Put("vspeed_ms", S.VSpeedMs, false); Put("alt_sea_m", S.AltSeaM);
		Vec("g.x", "g.y", "g.z", S.G); Put("g_load", S.GLoad); Put("shake", S.Shake); Put("plasma", S.Plasma); Put("plasma_k", S.PlasmaK, false);
		Put("scorch", S.Scorch); Put("hull_c", S.HullC); Put("whiteout", S.Whiteout); Put("real_world", S.bRealWorld, false);
		Put("chute", (double)(int)S.Chute, false); Put("chute_inflation", S.ChuteInflation); Put("swing_pitch", S.SwingPitchDeg);
		Put("swing_roll", S.SwingRollDeg); Put("chute_yaw", S.ChuteYawDeg);
		Vec("planet.x", "planet.y", "planet.z", S.PlanetDir); Put("planet_dia", S.PlanetDiaDeg);
		Vec("planet_island.x", "planet_island.y", "planet_island.z", S.PlanetIslandAxis);
		Vec("planet_north.x", "planet_north.y", "planet_north.z", S.PlanetNorthAxis);
		Vec("europa.x", "europa.y", "europa.z", S.EuropaDir); Put("europa_dia", S.EuropaDiaDeg); Put("europa_visible", S.EuropaVisible);
		Vec("sun.x", "sun.y", "sun.z", S.SunDir);
		Vec("frame_x.x", "frame_x.y", "frame_x.z", S.FrameX); Vec("frame_y.x", "frame_y.y", "frame_y.z", S.FrameY);
		Vec("frame_z.x", "frame_z.y", "frame_z.z", S.FrameZ);
		Put("roll", S.RollDeg); Put("pitch", S.PitchDeg); Put("yaw", S.YawDeg);
		Put("sub_lat", S.SubLatDeg); Put("sub_lon_rel", S.SubLonRelDeg); Put("track_arc", S.TrackArcDeg); Put("island_range_km", S.IslandRangeKm);
		Put("island_bearing", S.IslandBearingDeg, false); Put("island_visible", S.IslandVisible);
		Vec("island.x", "island.y", "island.z", S.IslandDir);
		const FInstruments& I = S.I;
		Put("i.powered", I.bPowered, false); Put("i.lamp_test", I.bLampTest, false); Put("i.press", I.PressMmHg); Put("i.o2", I.O2Pct);
		Put("i.co2", I.Co2Pct); Put("i.temp", I.TempC); Put("i.volts", I.Volts); Put("i.fuel", I.FuelPct); Put("i.hull01", I.Hull01);
		Put("i.gmeter", I.GMeter); Put("i.clock", I.ClockSec);
		Put("i.clock_digits", I.ClockDigits[0] * 1e5 + I.ClockDigits[1] * 1e4 + I.ClockDigits[2] * 1e3 + I.ClockDigits[3] * 1e2 + I.ClockDigits[4] * 10 + I.ClockDigits[5], false);
		Put("i.range", I.RangeKm, false); Put("i.speed_shown", I.SpeedShown, false); Put("i.speed_ms", I.bSpeedMs, false);
		Put("i.alt_dial", I.AltDialKm, false); Put("i.alt_sea", I.bAltSea, false); Put("i.globe_lat", I.GlobeLatDeg); Put("i.globe_lon", I.GlobeLonRelDeg);
		Put("i.crt_dot_x", I.CrtDotX); Put("i.crt_dot_y", I.CrtDotY); Put("i.crt_progress", I.CrtProgress); Put("i.crt_phase", I.CrtPhase, false);
		Put("i.crt_mode", I.CrtMode, false); Put("i.light", I.bCabinLight, false); Put("i.radio", I.bRadio, false);
		static const char* const LampNames[LampCount] = { "lamp.power", "lamp.air", "lamp.gyro", "lamp.hatch", "lamp.armed", "lamp.engine", "lamp.sep",
			"lamp.zero_g", "lamp.burn", "lamp.comm", "lamp.blackout", "lamp.g_high", "lamp.hull_hot", "lamp.drogue", "lamp.main_chute", "lamp.alt_low",
			"lamp.jets", "lamp.splash", "lamp.beacon", "lamp.o2_low", "lamp.co2_high", "lamp.batt_low", "lamp.master_caution", "lamp.record" };
		for (int L = 0; L < LampCount; ++L)
		{
			Put(LampNames[L], (S.Lamps[L].bOn ? 1 : 0) + (S.Lamps[L].bBlink ? 2 : 0) + (S.Lamps[L].bLit ? 4 : 0), false);
		}
		Put("switches", (S.Switches[0] ? 1 : 0) + (S.Switches[1] ? 2 : 0) + (S.Switches[2] ? 4 : 0) + (S.Switches[3] ? 8 : 0) + (S.Switches[4] ? 16 : 0), false);
		Put("rec.index", S.Record.Index, false); Put("rec.playing", S.Record.bPlaying, false); Put("rec.pos", S.Record.PosSec, false);
		Put("rec.pitch", S.Record.Pitch); Put("rec.wow", S.Record.Wow);
	}
}
