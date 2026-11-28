// The yacht school's fleet on the water, in plain C++ (docs/yacht_school.md "Engine: the school"): who sails where and why.
// It joins the two models: the boats and helmsmen of SailModel.h (part A) and the school's rules of SailSchoolModel.h
// (part B). ULyudSailing (LyudSailingFleet.cpp, LyudSailingRace.cpp) drives it for the game; tools/sailfleetsim runs a whole
// training session and a race with it on the real Content/City/sailing.json.
//
//   - FleetSkill: a member's skill as a helmsman from the data's ranking, the age, the season and what was learned.
//   - FleetLodOf / FleetLodBudget: near / mid / far by the distance from the eye, with hysteresis and a cap on near boats.
//   - FleetContacts: hull against hull (capsules) and against fixed things (the pier, a moored boat): pushed apart, way lost.
//   - FFleetBrain: the session's brain. The caller owns the FBoat of every unit and the FLaunch and steps them; the brain
//     gives each unit's helmsman its orders (out through the corridor, the drill of the hour, a race from the warning signal
//     to the finish, home and up the beach), keeps the neighbours' lists, reads every boat's race (FRaceTracker), sends the
//     coach's launch where it is needed (SchoolLaunchTask) and says what should be heard and shown (FFleetEvent).
//     A tick: fill Unit.Env, Think(Dt), step the boats with Unit.Controls (near) or Unit.Helm.CourseYaw / SpeedScale (far)
//     or StepTowed (Unit.Stage == Towed), step the launch toward LaunchTarget, then After(Dt).
//   - FleetResolveSession: a session nobody watches, by numbers (capsizes, metres) for the log and the skills.
//
// No Unreal types, nothing allocated per tick (the vectors are sized when a drill or a race is set).
#pragma once

#include "SailModel.h"
#include "SailSchoolModel.h"

namespace LyudSail
{
	// ------------------------------------------------------------------------------------------------------------------
	// Skill

	/** The helmsman's skill range of a group: beginners 0.15-0.35, juniors 0.4-0.7, adults 0.3-0.8. */
	inline void FleetSkillRange(const std::string& Group, double& Lo, double& Hi)
	{
		if (Group == "kids") { Lo = 0.15; Hi = 0.35; }
		else if (Group == "juniors") { Lo = 0.40; Hi = 0.70; }
		else { Lo = 0.30; Hi = 0.80; }
	}

	/** A member's skill at the helm: the data's ranking inside the group's range, a little for the age inside the group,
	 *  a seeded talent, what the season has taught so far (SeasonShare 0 the opening .. 1 the closing) and what the member
	 *  has learned in sessions (Learned, saved, at most 0.12). AgeYears 0: unknown. */
	inline double FleetSkill(const FSchoolSailor& S, double AgeYears, double SeasonShare, double Learned, uint32_t Seed)
	{
		double Lo = 0.3, Hi = 0.8;
		FleetSkillRange(S.Group, Lo, Hi);
		double T = SchoolClamp((S.Skill - 0.2) / 0.65, 0.0, 1.0);
		if (AgeYears > 0.0)
		{
			const double A0 = S.Group == "kids" ? 7.0 : (S.Group == "juniors" ? 12.0 : 18.0);
			const double A1 = S.Group == "kids" ? 11.0 : (S.Group == "juniors" ? 17.0 : 40.0);
			T = 0.75 * T + 0.25 * SchoolClamp((AgeYears - A0) / (A1 - A0), 0.0, 1.0);
		}
		FSchoolRng Rng(SchoolHash(Seed, SchoolHashStr(S.Slot)));
		Rng.Next();
		const double Talent = (Rng.U() - 0.5) * 0.06;
		const double V = Lo + (Hi - Lo) * T + Talent + 0.08 * SchoolClamp(SeasonShare, 0.0, 1.0) + SchoolClamp(Learned, 0.0, 0.12);
		return SchoolClamp(V, 0.1, Hi + 0.15);
	}

	/** What a session on the water teaches (added to Learned). */
	inline double FleetSkillGain(double HoursAfloat, int Capsizes)
	{
		return 0.004 * std::max(0.0, HoursAfloat) + 0.001 * std::min(3, std::max(0, Capsizes));
	}

	// ------------------------------------------------------------------------------------------------------------------
	// LOD

	constexpr double FleetNearM = 60.0, FleetNearOutM = 72.0, FleetMidM = 250.0, FleetMidOutM = 280.0;
	constexpr int FleetLodNear = 0, FleetLodMid = 1, FleetLodFar = 2;

	/** Near (0) to 60 m, mid (1) to 250 m, far (2) beyond; a boat leaves its bucket only 12 / 30 m past the edge. */
	inline int FleetLodOf(double DistM, int Prev)
	{
		if (DistM <= FleetNearM || (Prev == FleetLodNear && DistM <= FleetNearOutM)) return FleetLodNear;
		if (DistM <= FleetMidM || (Prev <= FleetLodMid && DistM <= FleetMidOutM)) return FleetLodMid;
		return FleetLodFar;
	}

	/** At most MaxNear boats near: the farthest of the others go to mid. Pinned ones (the hero's boat) stay near and count. */
	inline void FleetLodBudget(int* Lod, const double* Dist, const bool* Pinned, int N, int MaxNear)
	{
		int Near = 0;
		for (int I = 0; I < N; I++) if (Lod[I] == FleetLodNear) Near++;
		while (Near > std::max(0, MaxNear))
		{
			int Worst = -1;
			for (int I = 0; I < N; I++)
			{
				if (Lod[I] != FleetLodNear || (Pinned && Pinned[I])) continue;
				if (Worst < 0 || Dist[I] > Dist[Worst]) Worst = I;
			}
			if (Worst < 0) break;
			Lod[Worst] = FleetLodMid;
			Near--;
		}
	}

	// ------------------------------------------------------------------------------------------------------------------
	// Contacts

	/** The nearest points of two segments: the distance, and the points themselves. */
	inline double FleetSegSeg(const FSchoolV2& A0, const FSchoolV2& A1, const FSchoolV2& B0, const FSchoolV2& B1, FSchoolV2& OutA, FSchoolV2& OutB)
	{
		const FSchoolV2 Da = A1 - A0, Db = B1 - B0, R = A0 - B0;
		const double A = Da.Dot(Da), E = Db.Dot(Db), F = Db.Dot(R);
		double S = 0.0, T = 0.0;
		if (A <= 1e-9 && E <= 1e-9) { OutA = A0; OutB = B0; return (A0 - B0).Len(); }
		if (A <= 1e-9) { T = SchoolClamp(F / E, 0.0, 1.0); }
		else
		{
			const double C = Da.Dot(R);
			if (E <= 1e-9) { S = SchoolClamp(-C / A, 0.0, 1.0); }
			else
			{
				const double B = Da.Dot(Db), Den = A * E - B * B;
				S = Den > 1e-9 ? SchoolClamp((B * F - C * E) / Den, 0.0, 1.0) : 0.0;
				T = (B * S + F) / E;
				if (T < 0.0) { T = 0.0; S = SchoolClamp(-C / A, 0.0, 1.0); }
				else if (T > 1.0) { T = 1.0; S = SchoolClamp((B - C) / A, 0.0, 1.0); }
			}
		}
		OutA = A0 + Da * S;
		OutB = B0 + Db * T;
		return (OutA - OutB).Len();
	}

	/** A fixed thing boats bump into: a capsule from A to B of radius R (a round thing: A == B). */
	struct FFleetObstacle
	{
		FSchoolV2 A, B;
		double R = 1.0;
	};

	/** A hull as a capsule along its keel. */
	inline void FleetHull(const FBoat& B, FSchoolV2& Bow, FSchoolV2& Stern, double& Radius)
	{
		const FBoatDef& D = Def(B.Class);
		Radius = D.Beam * 0.5;
		const double Half = std::max(0.0, D.Loa * 0.5 - Radius);
		const FSchoolV2 Fwd(std::cos(B.Yaw), std::sin(B.Yaw));
		const FSchoolV2 At(B.X, B.Y);
		Bow = At + Fwd * Half;
		Stern = At - Fwd * Half;
	}

	/** Two hulls overlapping: both are moved apart (by the lighter more) and lose the way they had toward each other and a
	 *  share of the rest. Returns the closing speed (m/s, 0: no contact). */
	inline double FleetBump(FBoat& A, FBoat& B)
	{
		FSchoolV2 A0, A1, B0, B1, Pa, Pb;
		double Ra = 0.0, Rb = 0.0;
		FleetHull(A, A0, A1, Ra);
		FleetHull(B, B0, B1, Rb);
		const double D = FleetSegSeg(A0, A1, B0, B1, Pa, Pb);
		const double Over = Ra + Rb - D;
		if (Over <= 0.0)
		{
			return 0.0;
		}
		FSchoolV2 N = D > 1e-4 ? (Pa - Pb) * (1.0 / D) : (FSchoolV2(A.X - B.X, A.Y - B.Y).Norm());
		if (N.Len() < 0.5) N = FSchoolV2(1.0, 0.0);
		const double Ma = Def(A.Class).HullMass + A.CrewMass, Mb = Def(B.Class).HullMass + B.CrewMass;
		const double Wa = Mb / (Ma + Mb), Wb = Ma / (Ma + Mb);
		A.X += N.X * Over * Wa; A.Y += N.Y * Over * Wa;
		B.X -= N.X * Over * Wb; B.Y -= N.Y * Over * Wb;
		const double Closing = -((A.VX - B.VX) * N.X + (A.VY - B.VY) * N.Y);
		if (Closing > 0.0)
		{
			A.VX += N.X * Closing * Wa; A.VY += N.Y * Closing * Wa;
			B.VX -= N.X * Closing * Wb; B.VY -= N.Y * Closing * Wb;
			A.VX *= 0.8; A.VY *= 0.8; B.VX *= 0.8; B.VY *= 0.8;
		}
		return std::max(0.0, Closing) + 0.05;
	}

	/** A hull against a fixed thing: pushed out, the way into it lost. Returns the closing speed (0: no contact). */
	inline double FleetBumpFixed(FBoat& A, const FFleetObstacle& O)
	{
		FSchoolV2 A0, A1, Pa, Pb;
		double Ra = 0.0;
		FleetHull(A, A0, A1, Ra);
		const double D = FleetSegSeg(A0, A1, O.A, O.B, Pa, Pb);
		const double Over = Ra + O.R - D;
		if (Over <= 0.0)
		{
			return 0.0;
		}
		FSchoolV2 N = D > 1e-4 ? (Pa - Pb) * (1.0 / D) : FSchoolV2(1.0, 0.0);
		A.X += N.X * Over; A.Y += N.Y * Over;
		const double Closing = -(A.VX * N.X + A.VY * N.Y);
		if (Closing > 0.0)
		{
			// the way into it is lost at once; along it she slides on (a hull pinned on a lee pier must be able to sail off its end)
			A.VX += N.X * Closing; A.VY += N.Y * Closing;
			A.VX *= 0.985; A.VY *= 0.985;
		}
		return std::max(0.0, Closing) + 0.05;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// A session nobody watches

	struct FFleetSessionStat
	{
		int Capsizes = 0;
		double Metres = 0.0;
	};

	/** One sailor's two hours by numbers: the same for the same seed. Wind: the mean over the water, m/s. */
	inline FFleetSessionStat FleetResolveSession(const FSchoolParams& P, uint32_t Seed, const FSchoolSailor& S, double Skill, double Wind, double Hours)
	{
		FFleetSessionStat Out;
		FSchoolRng Rng(SchoolHash(Seed, SchoolHashStr(S.Slot) ^ 0x5A17u));
		Rng.Next(); Rng.Next();
		const int C = std::max(0, std::min(SchoolBoats - 1, S.Class));
		const double Stress = SchoolClamp((Wind - 0.55 * P.Wind[C][1]) / (0.45 * P.Wind[C][1]), 0.0, 1.5);
		const double Rate = (0.05 + 2.2 * Stress * Stress) * (1.15 - SchoolClamp(Skill, 0.0, 1.0)) * std::max(0.0, Hours);     // capsizes expected
		double Left = Rate;
		while (Left > 0.0 && Out.Capsizes < 12)
		{
			if (Rng.U() < std::min(1.0, Left)) Out.Capsizes++;
			Left -= 1.0;
		}
		const double Speed = P.SpeedMs[C] * (1.0 - std::exp(-std::max(0.0, Wind) / 3.5)) * (0.6 + 0.4 * SchoolClamp(Skill, 0.0, 1.0));
		Out.Metres = Speed * 3600.0 * std::max(0.0, Hours) * (0.55 + 0.2 * Rng.U());
		return Out;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// The session's brain

	enum class EFleetStage : uint8_t
	{
		Ashore,     // not launched yet (or never)
		Out1,       // out along the corridor
		Out2,       // across to the area of the drill or the race
		Work,       // the drill, the race
		Home1,      // back to the corridor's outer end
		Home2,      // in to the beach (EOrder::Ashore)
		Landed,     // on the sand: the caller parks her
		Towed,      // behind the launch
	};

	enum class EFleetMode : uint8_t { Idle, Drill, Race, Recall };

	enum class EFleetSignal : uint8_t
	{
		WhistleShort, WhistleLong, WhistleRecall, HornShort, HornLong,
		FlagUp, FlagDown,       // Other: the index into FFleetBrain::Sequence
		FlagX,                  // the individual recall flag: Value 1 up, 0 down
		Bump,                   // Unit, Other (-1: a fixed thing), Value the closing speed
		Capsize, Landed, Launched,
		Ocs, OcsCleared, Started, Rounded, WrongSide, Touch, Finished,     // Unit; Finished: Value = the place so far
		GeneralRecall, RaceOver, RaceAbandoned,
		TowStart, TowEnd, Helped,
	};

	struct FFleetEvent
	{
		EFleetSignal Kind = EFleetSignal::WhistleShort;
		int Unit = -1, Other = -1;
		double Value = 0.0;
	};

	struct FFleetWp
	{
		double X = 0.0, Y = 0.0, R = 5.0;
		int Rounding = -1;      // the course's rounding this point belongs to (-1: none)
		int Part = 0;           // 0 entry, 1 apex, 2 exit
	};

	struct FFleetUnit
	{
		// set by the caller
		FBoat* Boat = nullptr;
		int Sailor = -1;                // index into FSchoolParams::Sailors (-1: the hero)
		bool bHero = false;             // sailed by the player: no helmsman, but seen, bumped and scored
		int Lod = FleetLodFar;
		FEnv Env;                       // the world at the boat this tick
		FSchoolV2 Beach;                // where she lands
		// the brain's
		FHelm Helm;
		FControls Controls;             // for Step (near and mid boats)
		EFleetStage Stage = EFleetStage::Ashore;
		int Leg = 0, Laps = 0, Sub = 0, Lane = 0;
		double Timer = 0.0, RunAt = 0.0, FarDt = 0.0, PenT = 0.0;
		int Pen = -1;                   // a penalty turn's point (0..3), -1 none
		FSchoolV2 PenAt, Free;
		FRaceTracker Tracker;
		int Group = -1;                 // the race group she sails in (-1 none)
		double CapsizedS = 0.0, Metres = 0.0, AfloatS = 0.0;
		int Capsizes = 0, Bumps = 0, Place = 0;
		int Nb[6] = { -1, -1, -1, -1, -1, -1 };
		int NbCount = 0;
		bool bInArea = false;
		bool bStation = false;          // her order is to keep station (StationAt)
		double StationR = 10.0;
		double AgroundS = 0.0;          // seconds she has sat on the bottom
		double FendS = 0.0;             // seconds she has lain without way beside the pier
		FSchoolV2 Goal;                 // where her stage's order takes her (the way round the pier is found from it)
		bool bVia = false;              // sailing to the pier's seaward end first
		bool bLineUp = false;           // going for the line: first to a point below it, between its ends
		double ViaT = 0.0;
		double TouchT = 100.0;          // seconds since her hull last touched anything
		double OutsideM = 0.0;          // the farthest she has been outside her area while at work (after entering it)

		bool Afloat() const { return Stage != EFleetStage::Ashore && Stage != EFleetStage::Landed; }
	};

	/** A race of the boats that share a course (the cadets and the luches in the race area; the optimists in theirs). */
	struct FFleetRaceGroup
	{
		bool bOn = false;
		int ClassMask = 0;
		FRaceCourse Course;
		std::vector<FFleetWp> Route;
		double FirstFinish = -1.0;
		int Finished = 0, Entries = 0;
		bool bClosed = false;
	};

	class FFleetBrain
	{
	public:
		const FSchoolParams* P = nullptr;
		std::vector<FFleetUnit> Units;
		FLaunch* Launch = nullptr;              // the caller's, stepped by the caller toward LaunchTarget
		FDepthFn DepthFn = nullptr;
		void* DepthUser = nullptr;
		std::vector<FFleetObstacle> Obstacles;  // the pier and whatever else is fixed (the caller adds the moored rowing boat)

		// what is going on
		EFleetMode Mode = EFleetMode::Idle;
		std::string Drill, AreaId;
		FRaceCourse DrillCourse;                // a drill's buoys (triangle, figure-eight, practice line): bOk when laid
		std::vector<FFleetWp> DrillRoute;
		FSchoolV2 WindTo = FSchoolV2(-1.0, 0.0), Middle, Station, Home, CorridorEnd, Gate[2];
		double WindMs = 4.0, TideM = 0.0, WaterC = 16.0;
		uint32_t Seed = 1;
		bool bRecall = false;
		double Clock = 0.0, WhistleIn = 30.0, CapsizeIn = 40.0;
		int TackSide = 1;
		std::vector<FSchoolV2> LeadPath;
		int LeadAt = 0;
		double LeadHold = 0.0;

		// the race
		FFleetRaceGroup Groups[2];
		std::vector<FRaceSignal> Sequence;
		int SeqAt = 0;
		double RaceT = 0.0;                     // seconds from the gun
		bool bRaceRunning = false, bXUp = false;
		bool bHeld = false;                     // the sequence waits for the fleet to come to the line
		double HoldT = 0.0;
		int GeneralRecalls = 0;

		// the launch
		FSchoolV2 LaunchTarget;
		double LaunchCruise = 0.0, LaunchRadius = 8.0;
		FSchoolLaunchTask Task;
		int Towing = -1;
		int ForceTow = -1;                      // a unit the launch must fetch and tow home whatever she is doing (the hero who ignored the recall)
		double TaskIn = 0.0, HelpT = 0.0;
		double RecallT = 0.0;                   // seconds since the recall
		double StragglerS = 600.0;              // this long after the recall the launch tows in whoever is still out, the farthest first
		FSchoolV2 PierVia;                      // open water off the pier's seaward end
		bool bPier = false;

		// out
		FFleetEvent Events[48];
		int EventCount = 0;
		int Contacts = 0;

		void Init(const FSchoolParams& InP, uint32_t InSeed)
		{
			P = &InP;
			Seed = InSeed ? InSeed : 1u;
			Rng = FSchoolRng(Seed);
			Flat.clear();
			for (const FSchoolArea& A : InP.Areas)
			{
				std::vector<double> F;
				for (const FSchoolV2& V : A.Poly) { F.push_back(V.X); F.push_back(V.Y); }
				Flat.push_back(F);
			}
			Corridor = -1;
			for (size_t I = 0; I < InP.Areas.size(); I++) if (InP.Areas[I].Id == "corridor") Corridor = (int)I;
			CorridorEnd = SchoolLaunchLineAt(InP, 115.0);
			Home = SchoolLaunchLineAt(InP, 30.0);
			Obstacles.clear();
			FFleetObstacle Pier;
			Pier.A = InP.PierA; Pier.B = InP.PierB; Pier.R = InP.PierHalfW;
			bPier = (Pier.A - Pier.B).Len() > 1.0;
			if (bPier)
			{
				Obstacles.push_back(Pier);
				const bool bSeaB = (Pier.B - Pier.A).Dot(InP.LaunchDir) >= 0.0;
				const FSchoolV2 End = bSeaB ? Pier.B : Pier.A, Root = bSeaB ? Pier.A : Pier.B;
				PierVia = End + (End - Root).Norm() * (Pier.R + 32.0);
			}
			States.reserve(32);
			Results.reserve(32);
		}

		/** Where the trolleys stop today (SchoolLaunchPointSampled): the boats land in a row across the corridor there. */
		void SetLaunchPoint(const FSchoolV2& At)
		{
			Home = At + P->LaunchDir * 14.0;
			const FSchoolV2 Across = P->LaunchDir.Left();
			const int N = (int)Units.size();
			const double Gap = std::min(2.6, 13.0 / std::max(1, N - 1));
			for (int I = 0; I < N; I++)
			{
				Units[(size_t)I].Beach = At + Across * (((double)I - 0.5 * (N - 1)) * Gap) + P->LaunchDir * -1.5;
			}
		}

		/** Where the crews, wading, let go of their boats: the first water out along the launch line deep enough for the board
		 *  (1.15 m; at most 120 m beyond the trolleys). */
		FSchoolV2 PushOff(const FSchoolV2& LaunchAt) const
		{
			if (!DepthFn) return LaunchAt + P->LaunchDir * 25.0;
			for (double D = 2.0; D <= 120.0; D += 2.0)
			{
				const FSchoolV2 At = LaunchAt + P->LaunchDir * D;
				if (DepthFn(DepthUser, At.X, At.Y) >= 1.15) return At;
			}
			return LaunchAt + P->LaunchDir * 120.0;
		}

		/** The heading a boat is let go on: straight out when the wind lets her, else close-hauled (55 degrees off the wind) on
		 *  the tack that takes her more to seaward, and of two alike the one away from the pier. */
		double PushOffYaw() const
		{
			const double Out = std::atan2(P->LaunchDir.Y, P->LaunchDir.X);
			const double From = WindFromYaw(WindTo.X, WindTo.Y);
			if (std::fabs(WrapAngle(From - Out)) > 60.0 * Pi / 180.0) return Out;
			const double A = WrapAngle(From + 55.0 * Pi / 180.0), B = WrapAngle(From - 55.0 * Pi / 180.0);
			const FSchoolV2 Away = (CorridorEnd - (P->PierA + P->PierB) * 0.5).Norm();
			auto Score = [this, &Away](double Yaw) { const FSchoolV2 F(std::cos(Yaw), std::sin(Yaw)); return F.Dot(P->LaunchDir) + 0.35 * F.Dot(Away); };
			return Score(A) >= Score(B) ? A : B;
		}

		void SetWeather(const FSchoolV2& InWindTo, double InWindMs, double InTideM, double InWaterC)
		{
			if (InWindTo.Len() > 0.2) WindTo = InWindTo.Norm();
			WindMs = InWindMs; TideM = InTideM; WaterC = InWaterC;
		}

		/** A unit goes afloat (the caller has placed her boat on the water at the launch point). */
		void LaunchUnit(int I)
		{
			FFleetUnit& U = Units[(size_t)I];
			U.Stage = Mode == EFleetMode::Drill && Drill == "follow" ? EFleetStage::Work : EFleetStage::Out1;
			U.bInArea = false;
			U.OutsideM = 0.0;
			U.CapsizedS = 0.0;
			U.Sub = 0; U.Leg = 0; U.Pen = -1;
			U.Lane = I;
			if (!U.bHero) Enter(U, I);
			Emit(EFleetSignal::Launched, I);
		}

		/** The drill of this block of the session (FSchoolNow::Drill and Area). Lays its buoys (DrillCourse.Marks) and gives
		 *  everybody afloat their new work. */
		void SetDrill(const std::string& InDrill, const std::string& InArea)
		{
			Mode = EFleetMode::Drill;
			bRecall = false;
			Drill = InDrill;
			AreaId = InArea;
			DrillCourse = FRaceCourse();
			DrillRoute.clear();
			LeadPath.clear();
			const FSchoolArea* A = P->Area(AreaId);
			Middle = A ? SchoolCentroid(*A) : CorridorEnd;
			const FSchoolDrill* D = P->Drill(Drill);
			int Class = SchoolLuch;
			for (const FFleetUnit& U : Units) if (U.Sailor >= 0) { Class = std::min(Class, P->Sailors[(size_t)U.Sailor].Class); }
			const uint32_t S2 = SchoolHash(Seed, SchoolHashStr(Drill));
			if (D && !D->Course.empty())
			{
				const ERaceCourse Kind = RaceCourseKind(D->Course);
				DrillCourse = RaceSetCourse(*P, Kind, Class, WindTo, WindMs, (int)Units.size(), S2, TideM, AreaId, Kind == ERaceCourse::Figure8 ? 2 : 1);
				if (DrillCourse.bOk)
				{
					Middle = DrillCourse.Middle;
					if (Kind != ERaceCourse::StartLine) BuildRoute(DrillCourse, DrillRoute, Class);
				}
			}
			Station = D ? SchoolDrillStation(*D, Middle, WindTo) : Middle;
			if (D && D->Launch == "line" && DrillCourse.bOk) Station = DrillCourse.LineBoat;
			if (A) Station = SchoolNearestInside(*A, Station, 12.0);
			// the gates of the tacking drill: a beat from the lee side of the area to its weather side and a run back
			const FSchoolV2 Up = WindTo * -1.0;
			Gate[0] = A ? SchoolNearestInside(*A, Middle + Up * -140.0, 30.0) : Middle;
			Gate[1] = A ? SchoolNearestInside(*A, Middle + Up * 140.0, 30.0) : Middle;
			if (Drill == "follow")
			{
				// the launch leads the file out of the corridor and round a slow loop in the area
				LeadPath.push_back(CorridorEnd);
				for (int K = 0; K < 8; K++)
				{
					const double Ang = 2.0 * SchoolPi * K / 8.0;
					const FSchoolV2 Pt = Middle + FSchoolV2(std::cos(Ang) * 55.0, std::sin(Ang) * 75.0);
					LeadPath.push_back(A ? SchoolNearestInside(*A, Pt, 25.0) : Pt);
				}
				LeadAt = 0;
				LeadHold = 45.0;
			}
			WhistleIn = 25.0;
			CapsizeIn = 40.0;
			for (size_t I = 0; I < Units.size(); I++)
			{
				FFleetUnit& U = Units[I];
				if (!U.Afloat() || U.bHero || U.Stage == EFleetStage::Towed) continue;
				U.Sub = 0; U.Leg = (int)(I % 3u); U.Pen = -1;
				if (Drill == "follow") U.Stage = EFleetStage::Work;
				else if (U.Stage == EFleetStage::Work || U.Stage == EFleetStage::Out2) U.Stage = EFleetStage::Out2;
				else if (U.Stage == EFleetStage::Home1 || U.Stage == EFleetStage::Home2) U.Stage = EFleetStage::Out1;
				U.bInArea = false;
				Enter(U, (int)I);
			}
		}

		/** A race: the groups' courses are set by the caller (RaceSetCourse at the warning signal); the sequence runs from
		 *  -WarningS. Group 0 the race area's classes, 1 the children's. */
		void SetRace(const FRaceCourse& Main, int MainMask, const FRaceCourse* Kids, int LeadClass)
		{
			Mode = EFleetMode::Race;
			bRecall = false;
			Drill.clear();
			DrillCourse = FRaceCourse();
			DrillRoute.clear();
			Groups[0] = FFleetRaceGroup();
			Groups[1] = FFleetRaceGroup();
			Groups[0].bOn = Main.bOk && MainMask != 0;
			Groups[0].Course = Main;
			Groups[0].ClassMask = MainMask;
			if (Groups[0].bOn) BuildRoute(Main, Groups[0].Route, Main.Class);
			if (Kids && Kids->bOk)
			{
				Groups[1].bOn = true;
				Groups[1].Course = *Kids;
				Groups[1].ClassMask = 1 << SchoolOptimist;
				BuildRoute(*Kids, Groups[1].Route, SchoolOptimist);
			}
			Sequence = RaceStartSequence(*P, LeadClass);
			AreaId = Groups[0].bOn ? Main.Area : (Kids ? Kids->Area : std::string());
			Middle = Groups[0].bOn ? Main.Middle : (Kids ? Kids->Middle : CorridorEnd);
			Station = Groups[0].bOn ? CommitteeAt(Main) : (Kids ? CommitteeAt(*Kids) : Middle);
			GeneralRecalls = 0;
			bRaceRunning = false;
			bHeld = true;
			HoldT = 0.0;
			for (int G = 0; G < 2; G++) Groups[G].bClosed = !Groups[G].bOn;
			for (size_t I = 0; I < Units.size(); I++)
			{
				FFleetUnit& U = Units[I];
				U.Tracker.Reset();
				U.Group = GroupOf(U);
				if (U.Group >= 0) Groups[U.Group].Entries++;
				if (!U.Afloat() || U.bHero || U.Stage == EFleetStage::Towed) continue;
				if (U.Stage != EFleetStage::Out1) U.Stage = EFleetStage::Out2;
				U.bInArea = false;
				Enter(U, (int)I);
			}
		}

		/** Everybody home: three blasts, the helmsmen ordered ashore, the launch after the stragglers. */
		void Recall(bool bWhistle)
		{
			if (Mode == EFleetMode::Recall)
			{
				return;
			}
			if (Mode == EFleetMode::Race && bRaceRunning) CloseRace(true);
			bHeld = false;
			Mode = EFleetMode::Recall;
			bRecall = true;
			RecallT = 0.0;
			if (bWhistle) Emit(EFleetSignal::WhistleRecall, -1);
			for (size_t I = 0; I < Units.size(); I++)
			{
				FFleetUnit& U = Units[I];
				if (!U.Afloat() || U.bHero || U.Stage == EFleetStage::Towed) continue;
				U.Stage = EFleetStage::Home1;
				U.Pen = -1;
				Enter(U, (int)I);
			}
		}

		/** The race's time is up, or the day's last race is done: back to waiting below the line (bHome false) or home. */
		void EndRace(bool bHome)
		{
			if (bRaceRunning) CloseRace(false);
			if (bHome) Recall(false);
		}

		bool AllAshore() const
		{
			for (const FFleetUnit& U : Units) if (U.Afloat() && !U.bHero) return false;
			return true;
		}
		int AfloatCount() const
		{
			int N = 0;
			for (const FFleetUnit& U : Units) if (U.Afloat()) N++;
			return N;
		}

		/** The results of the last closed race of a class, scored (RaceScore). */
		const std::vector<FRaceFinish>& ResultsOf(int Class)
		{
			Results.clear();
			for (const FFleetUnit& U : Units)
			{
				const int C = ClassOf(U);
				if (C != Class || U.Group < 0) continue;
				FRaceFinish F;
				F.Slot = U.bHero ? std::string("hero") : P->Sailors[(size_t)U.Sailor].Slot;
				F.Status = U.Tracker.Status == ERaceStatus::Waiting ? ERaceStatus::Dns : (U.Tracker.Status == ERaceStatus::Racing ? ERaceStatus::Dnf : U.Tracker.Status);
				F.TimeS = U.Tracker.FinishT;
				Results.push_back(F);
			}
			RaceScore(Results);
			return Results;
		}

		/** The fleet's place of a unit in its race now (1 = leading): by the marks rounded, then the way to the next. */
		int PlaceNow(int I) const
		{
			const FFleetUnit& Me = Units[(size_t)I];
			if (Me.Group < 0) return 0;
			const double Mine = Progress(Me);
			int Ahead = 0;
			for (const FFleetUnit& U : Units)
			{
				if (&U != &Me && U.Group == Me.Group && ClassOf(U) == ClassOf(Me) && Progress(U) > Mine) Ahead++;
			}
			return Ahead + 1;
		}

		/** The next thing the hero steers for in his race: the mark to round, or the line. */
		FSchoolV2 NextMarkOf(int I) const
		{
			const FFleetUnit& U = Units[(size_t)I];
			if (U.Group < 0) return FSchoolV2();
			const FRaceCourse& C = Groups[U.Group].Course;
			return C.Waypoint(U.Tracker.bStarted ? U.Tracker.Next : -1);
		}

		// ---- the tick ----------------------------------------------------------------------------------------------------

		/** Before the boats are stepped: orders, neighbours, the helmsmen's controls. */
		void Think(double Dt)
		{
			EventCount = 0;
			Clock += Dt;
			NbClock -= Dt;
			if (NbClock <= 0.0)
			{
				NbClock = 0.2;
				RefreshNeighbours();
			}
			if (Mode == EFleetMode::Drill) ThinkDrill(Dt);
			else if (Mode == EFleetMode::Race) ThinkRace(Dt);
			for (size_t I = 0; I < Units.size(); I++)
			{
				FFleetUnit& U = Units[I];
				if (!U.Boat || !U.Afloat() || U.bHero || U.Stage == EFleetStage::Towed) continue;
				Orders(U, (int)I, Dt);
				FHelmInput In;
				FillInput(U, (int)I, In);
				if (U.Lod == FleetLodFar)
				{
					// far helmsmen think four times a second, each in its own tick
					U.FarDt += Dt;
					if (U.FarDt >= 0.25 || ((int)I & 3) == (Stagger & 3))
					{
						if (U.FarDt >= 0.2)
						{
							HelmFar(U.Helm, *U.Boat, In, U.FarDt);
							U.FarDt = 0.0;
						}
					}
				}
				else
				{
					U.FarDt = 0.0;
					U.Controls = HelmUpdate(U.Helm, *U.Boat, In, Dt);
				}
			}
			Stagger++;
			ThinkLaunch(Dt);
		}

		/** After the boats are stepped: what happened to them, hull contacts, the race's reading, landings. */
		void After(double Dt)
		{
			for (size_t I = 0; I < Units.size(); I++)
			{
				FFleetUnit& U = Units[I];
				if (!U.Boat || !U.Afloat()) continue;
				FBoat& B = *U.Boat;
				U.AfloatS += Dt;
				U.Metres += std::hypot(B.VX, B.VY) * Dt;
				if (B.Events & EvCapsize)
				{
					U.Capsizes++;
					Emit(EFleetSignal::Capsize, (int)I);
				}
				const bool bDown = B.State == EState::Capsized || B.State == EState::Turtled;
				U.CapsizedS = bDown ? U.CapsizedS + Dt : 0.0;
				// how far outside her water (only once she has been inside it, and only at work)
				if (U.Stage == EFleetStage::Work && !AreaId.empty())
				{
					const FSchoolArea* A = AreaOf(U);
					if (A)
					{
						const FSchoolV2 At(B.X, B.Y);
						const bool bIn = SchoolInside(*A, At);
						if (bIn && SchoolEdgeDistance(*A, At) > 10.0) U.bInArea = true;
						else if (!bIn && U.bInArea) U.OutsideM = std::max(U.OutsideM, SchoolEdgeDistance(*A, At));
					}
				}
				// landed
				// (aground along the beach on the way in: the crew is over the side and walks her home through the shallows)
				U.AgroundS = B.State == EState::Aground || (U.Env.Depth < 0.7 && std::hypot(B.VX, B.VY) < 0.25) ? U.AgroundS + Dt : 0.0;
				const double FromBeach = (FSchoolV2(B.X, B.Y) - U.Beach).Len();
				const bool bWalk = (U.Stage == EFleetStage::Home1 || U.Stage == EFleetStage::Home2) && U.AgroundS > 12.0 && FromBeach < 130.0;
				if (!U.bHero && ((U.Stage == EFleetStage::Home2 && (U.Helm.bArrived || B.State == EState::Aground) && FromBeach < 60.0) || bWalk))
				{
					Place(B, B.X, B.Y, B.Yaw, EState::Ashore);
					U.Stage = EFleetStage::Landed;
					Emit(EFleetSignal::Landed, (int)I);
				}
			}
			// hulls
			for (size_t I = 0; I < Units.size(); I++)
			{
				FFleetUnit& U = Units[I];
				if (!U.Boat || !U.Afloat()) continue;
				U.TouchT += Dt;
				for (int K = 0; K < U.NbCount; K++)
				{
					const int J = U.Nb[K];
					if (J <= (int)I || J >= (int)Units.size()) continue;       // each pair once (the lists are symmetric enough: see RefreshNeighbours)
					FFleetUnit& V = Units[(size_t)J];
					if (!V.Boat || !V.Afloat()) continue;
					const double Hit = FleetBump(*U.Boat, *V.Boat);
					if (Hit > 0.0)
					{
						// boats lying against each other are one contact, not one a tick
						if (U.TouchT > 4.0 && V.TouchT > 4.0)
						{
							Contacts++;
							U.Bumps++;
							V.Bumps++;
							Emit(EFleetSignal::Bump, (int)I, J, Hit);
						}
						U.TouchT = 0.0;
						V.TouchT = 0.0;
					}
				}
				for (const FFleetObstacle& O : Obstacles)
				{
					const double Hit = FleetBumpFixed(*U.Boat, O);
					if (Hit > 0.0)
					{
						if (U.TouchT > 4.0) Emit(EFleetSignal::Bump, (int)I, -1, Hit);
						U.TouchT = 0.0;
					}
					// blown onto the pier without way: the crew fend off and paddle her clear of it
					FSchoolV2 Near;
					const FSchoolV2 At(U.Boat->X, U.Boat->Y);
					const double Off = SchoolSegDist(At, O.A, O.B, &Near) - O.R;
					if (U.Boat->State == EState::Sailing && U.Stage != EFleetStage::Towed && Off < 5.0 && std::hypot(U.Boat->VX, U.Boat->VY) < 0.4) U.FendS += Dt;
					else if (Off > 11.0 || U.Boat->State != EState::Sailing) U.FendS = 0.0;
					if (U.FendS > 15.0 && Off < 11.0 && U.Boat->State == EState::Sailing)
					{
						const FSchoolV2 Away = (At - Near).Norm();
						U.Boat->X += Away.X * 0.6 * Dt;
						U.Boat->Y += Away.Y * 0.6 * Dt;
					}
				}
				if (Launch)
				{
					FFleetObstacle O;
					O.A = FSchoolV2(Launch->X, Launch->Y) + FSchoolV2(std::cos(Launch->Yaw), std::sin(Launch->Yaw)) * 1.4;
					O.B = FSchoolV2(Launch->X, Launch->Y) - FSchoolV2(std::cos(Launch->Yaw), std::sin(Launch->Yaw)) * 1.4;
					O.R = 0.85;
					if (U.Stage != EFleetStage::Towed) FleetBumpFixed(*U.Boat, O);
				}
			}
			if (Mode == EFleetMode::Race && bRaceRunning) AfterRace(Dt);
		}

	private:
		FSchoolRng Rng;
		std::vector<std::vector<double>> Flat;      // the areas' polygons as x, y pairs (FHelmInput::Poly)
		std::vector<FSchoolBoatState> States;
		std::vector<FRaceFinish> Results;
		FNeighbour NbBuf[16];
		double NbClock = 0.0;
		int Stagger = 0;
		int Corridor = -1;

		static double HelmDepth(void* User, double X, double Y)
		{
			const FFleetBrain* Self = static_cast<const FFleetBrain*>(User);
			const FSchoolV2 At(X, Y);
			for (const FFleetObstacle& O : Self->Obstacles)
			{
				if (SchoolSegDist(At, O.A, O.B, nullptr) < O.R + 3.0) return 0.0;
			}
			return Self->DepthFn ? Self->DepthFn(Self->DepthUser, X, Y) : 10.0;
		}

		void Emit(EFleetSignal Kind, int Unit, int Other = -1, double Value = 0.0)
		{
			if (EventCount < (int)(sizeof(Events) / sizeof(Events[0])))
			{
				FFleetEvent& E = Events[EventCount++];
				E.Kind = Kind; E.Unit = Unit; E.Other = Other; E.Value = Value;
			}
		}

		int ClassOf(const FFleetUnit& U) const
		{
			return U.Boat ? (int)U.Boat->Class : (U.Sailor >= 0 ? P->Sailors[(size_t)U.Sailor].Class : P->HeroClass);
		}
		int GroupOf(const FFleetUnit& U) const
		{
			const int C = ClassOf(U);
			for (int G = 0; G < 2; G++) if (Groups[G].bOn && (Groups[G].ClassMask >> C & 1)) return G;
			return -1;
		}
		const FSchoolArea* AreaOf(const FFleetUnit& U) const
		{
			if (Mode == EFleetMode::Race) return U.Group >= 0 ? P->Area(Groups[U.Group].Course.Area) : nullptr;
			return P->Area(AreaId);
		}
		int AreaIndexOf(const FFleetUnit& U) const
		{
			const FSchoolArea* A = AreaOf(U);
			return A ? (int)(A - P->Areas.data()) : -1;
		}
		static FSchoolV2 CommitteeAt(const FRaceCourse& C)
		{
			// just outside the starboard end of the line, so the line's end is the launch's staff
			return C.LineBoat + (C.LineBoat - C.LinePin).Norm() * 2.5;
		}
		double Progress(const FFleetUnit& U) const
		{
			if (U.Group < 0) return -1.0;
			if (U.Tracker.Status == ERaceStatus::Finished) return 1000.0 - U.Tracker.FinishT * 1e-3;
			if (!U.Tracker.bStarted || !U.Boat) return 0.0;
			const FRaceCourse& C = Groups[U.Group].Course;
			const FSchoolV2 To = C.Waypoint(U.Tracker.Next);
			const double Leg = std::max(1.0, (To - C.Waypoint(U.Tracker.Next - 1)).Len());
			return 1.0 + U.Tracker.Next + SchoolClamp(1.0 - (To - FSchoolV2(U.Boat->X, U.Boat->Y)).Len() / Leg, 0.0, 0.999);
		}

		/** Three points round every rounding of a course: before the mark on the side she must pass it, abeam of it on the
		 *  outside of the turn, after it on the way to the next. */
		void BuildRoute(const FRaceCourse& C, std::vector<FFleetWp>& Out, int Class) const
		{
			Out.clear();
			const double Off = 5.0 + P->Loa[std::max(0, std::min(SchoolBoats - 1, Class))];
			for (int K = 0; K < (int)C.Order.size(); K++)
			{
				const FSchoolV2 M = C.Waypoint(K);
				const FSchoolV2 In = (M - C.Waypoint(K - 1)).Norm(), Next = (C.Waypoint(K + 1) - M).Norm();
				const double Side = C.Side[(size_t)K] > 0 ? 1.0 : -1.0;
				// the mark left to port: the boat passes on the mark's right hand as she goes (right = -Left in this frame)
				const FSchoolV2 E = M + In.Left() * (-Side * Off) - In * 2.0;
				const FSchoolV2 A = M + FRaceTracker::OutRay(C, K) * Off;
				const FSchoolV2 X = M + Next.Left() * (-Side * Off) + Next * 2.0;
				FFleetWp W;
				W.Rounding = K;
				W.R = 4.5;
				W.X = E.X; W.Y = E.Y; W.Part = 0; Out.push_back(W);
				W.X = A.X; W.Y = A.Y; W.Part = 1; Out.push_back(W);
				W.X = X.X; W.Y = X.Y; W.Part = 2; Out.push_back(W);
			}
		}

		void RefreshNeighbours()
		{
			const int N = (int)Units.size();
			for (int I = 0; I < N; I++)
			{
				FFleetUnit& U = Units[(size_t)I];
				U.NbCount = 0;
				if (!U.Boat || !U.Afloat()) continue;
				double Best[6];
				for (int J = 0; J < N; J++)
				{
					const FFleetUnit& V = Units[(size_t)J];
					if (J == I || !V.Boat || !V.Afloat()) continue;
					const double Dx = V.Boat->X - U.Boat->X, Dy = V.Boat->Y - U.Boat->Y;
					const double D2 = Dx * Dx + Dy * Dy;
					if (D2 > 70.0 * 70.0) continue;
					int At = U.NbCount;
					while (At > 0 && Best[At - 1] > D2) At--;
					if (At >= 6) continue;
					for (int K = std::min(U.NbCount, 5); K > At; K--) { Best[K] = Best[K - 1]; U.Nb[K] = U.Nb[K - 1]; }
					Best[At] = D2;
					U.Nb[At] = J;
					if (U.NbCount < 6) U.NbCount++;
				}
			}
		}

		void FillInput(const FFleetUnit& U, int I, FHelmInput& In)
		{
			(void)I;
			In.WindX = U.Env.WindX;
			In.WindY = U.Env.WindY;
			In.Depth = U.Env.Depth;
			// the helmsman's chart: the caller's depths, and no water at all where the pier stands
			In.DepthFn = &FFleetBrain::HelmDepth;
			In.DepthUser = this;
			In.MinDepth = 0.6;
			// her water's polygon only while she works in it (the ways out and home cross open water)
			int Ai = U.Stage == EFleetStage::Work && U.bInArea && !(Mode == EFleetMode::Drill && Drill == "follow") ? AreaIndexOf(U) : -1;
			// a mark laid near the area's edge is still to be rounded: close to her point she looks at the boats, not the edge
			if (Ai >= 0 && U.Boat && U.Helm.Order.Kind == EOrder::SailTo && std::hypot(U.Helm.Order.X - U.Boat->X, U.Helm.Order.Y - U.Boat->Y) < 45.0)
			{
				Ai = -1;
			}
			if (U.Stage == EFleetStage::Out1 && Corridor >= 0 && U.Boat && SchoolInside(P->Areas[(size_t)Corridor], FSchoolV2(U.Boat->X, U.Boat->Y)))
			{
				Ai = Corridor;      // out between the bathers and the pier
			}
			if (Ai >= 0)
			{
				In.Poly = Flat[(size_t)Ai].data();
				In.PolyCount = (int)(Flat[(size_t)Ai].size() / 2);
			}
			int C = 0;
			for (int K = 0; K < U.NbCount; K++)
			{
				const FFleetUnit& V = Units[(size_t)U.Nb[K]];
				if (!V.Boat || !V.Afloat()) continue;
				const FBoat& B = *V.Boat;
				FNeighbour& Nb = NbBuf[C++];
				Nb.X = B.X; Nb.Y = B.Y; Nb.VX = B.VX; Nb.VY = B.VY;
				Nb.Length = Def(B.Class).Loa;
				const bool bSailing = B.State == EState::Sailing || B.State == EState::Swamped;
				Nb.Tack = !bSailing ? 0 : (B.Out.TrueWindAngle >= 0.0 ? 1 : -1);
			}
			if (Launch && U.Boat && std::hypot(Launch->X - U.Boat->X, Launch->Y - U.Boat->Y) < 70.0
				&& !(Mode == EFleetMode::Drill && Drill == "follow" && U.Stage == EFleetStage::Work))
			{
				FNeighbour& Nb = NbBuf[C++];
				Nb.X = Launch->X; Nb.Y = Launch->Y; Nb.VX = Launch->VX; Nb.VY = Launch->VY;
				Nb.Length = LaunchLength;
				Nb.Tack = 0;
			}
			// the pier, when she is near it: its nearest point is a thing to keep clear of
			if (!Obstacles.empty() && U.Boat && U.Stage != EFleetStage::Work)
			{
				FSchoolV2 Near;
				const double D = SchoolSegDist(FSchoolV2(U.Boat->X, U.Boat->Y), Obstacles[0].A, Obstacles[0].B, &Near);
				if (D < Obstacles[0].R + 35.0)
				{
					FNeighbour& Nb = NbBuf[C++];
					Nb.X = Near.X; Nb.Y = Near.Y; Nb.VX = 0.0; Nb.VY = 0.0;
					Nb.Length = 2.0 * Obstacles[0].R + 6.0;
					Nb.Tack = 0;
				}
			}
			// the buoys she is rounding
			const FRaceCourse* Course = Mode == EFleetMode::Race ? (U.Group >= 0 ? &Groups[U.Group].Course : nullptr) : (DrillCourse.bOk ? &DrillCourse : nullptr);
			if (Course && U.Boat)
			{
				for (const FRaceMark& M : Course->Marks)
				{
					if (C >= 14 || std::hypot(M.At.X - U.Boat->X, M.At.Y - U.Boat->Y) > 40.0) continue;
					FNeighbour& Nb = NbBuf[C++];
					Nb.X = M.At.X; Nb.Y = M.At.Y; Nb.VX = 0.0; Nb.VY = 0.0;
					Nb.Length = 1.6;
					Nb.Tack = 0;
				}
			}
			In.Neighbours = NbBuf;
			In.NeighbourCount = C;
		}

		/** (FOrder::Lane is left wide open: as SailModel.cpp's Heading() reads it, a narrow lane makes a boat tack away from the
		 *  mark's side of the wind every few seconds, which in light air stops her. Her water's polygon keeps her in.) */
		static void SailTo(FFleetUnit& U, const FSchoolV2& To, double Radius, double Lane = 80.0)
		{
			(void)Lane;
			FOrder O;
			O.Kind = EOrder::SailTo;
			O.X = To.X; O.Y = To.Y; O.Radius = Radius; O.Lane = 1.0e5;
			HelmOrder(U.Helm, O);
			U.bStation = false;
		}
		/** Keep station at a point: from far off she sails there first (Keep() turns the order into a station on arriving). */
		static void StationAt(FFleetUnit& U, const FSchoolV2& At, double Radius)
		{
			FOrder O;
			const double Far = U.Boat ? std::hypot(U.Boat->X - At.X, U.Boat->Y - At.Y) : 0.0;
			O.Kind = Far > Radius + 25.0 ? EOrder::SailTo : EOrder::Station;
			O.X = At.X; O.Y = At.Y; O.Radius = Radius;
			HelmOrder(U.Helm, O);
			U.StationR = Radius;
			U.bStation = true;
		}
		static void Keep(FFleetUnit& U)
		{
			if (U.bStation && U.Helm.Order.Kind == EOrder::SailTo && U.Helm.bArrived)
			{
				FOrder O = U.Helm.Order;
				O.Kind = EOrder::Station;
				O.Radius = U.StationR;
				HelmOrder(U.Helm, O);
			}
		}

		/** A point of free sailing: somewhere well inside the area. */
		FSchoolV2 FreePoint(const FFleetUnit& U)
		{
			const FSchoolArea* A = AreaOf(U);
			if (!A) return Middle;
			double X0 = 1e18, X1 = -1e18, Y0 = 1e18, Y1 = -1e18;
			for (const FSchoolV2& V : A->Poly) { X0 = std::min(X0, V.X); X1 = std::max(X1, V.X); Y0 = std::min(Y0, V.Y); Y1 = std::max(Y1, V.Y); }
			const FSchoolV2 Pt(Middle.X + (Rng.U() - 0.5) * std::min(240.0, X1 - X0), Middle.Y + (Rng.U() - 0.5) * std::min(240.0, Y1 - Y0));
			return SchoolNearestInside(*A, Pt, 30.0);
		}

		/** The point where she comes into her area from the corridor. */
		FSchoolV2 EntryPoint(const FFleetUnit& U) const
		{
			const FSchoolArea* A = AreaOf(U);
			if (Mode == EFleetMode::Race && U.Group >= 0) return PreStart(U);
			if (!A) return Middle;
			const FSchoolV2 Toward = Middle + (CorridorEnd - Middle).Norm() * std::min(60.0, (CorridorEnd - Middle).Len() * 0.5);
			return SchoolNearestInside(*A, Toward, 20.0);
		}

		/** The course she comes to the line on: a close reach on the starboard tack (16 degrees free of close-hauled), so she
		 *  sails straight at her place on the line. (Beating up to it, SailModel's helmsman stands on past the lay line and
		 *  comes in round the line's end.) */
		FSchoolV2 StartCourse(const FFleetUnit& U) const
		{
			const FRaceCourse& C = Groups[U.Group].Course;
			const double A = BeatAngle(EBoat(ClassOf(U)), std::max(1.0, WindMs)) + 16.0 * Pi / 180.0;
			return C.Up * std::cos(A) + (C.LinePin - C.LineBoat).Norm() * std::sin(A);
		}
		/** Where she waits for her start: back along that course from her place on the line, in one of three ranks. */
		FSchoolV2 PreStart(const FFleetUnit& U) const
		{
			return LinePoint(U) - StartCourse(U) * (30.0 + 13.0 * (U.Lane % 3));
		}
		FSchoolV2 LinePoint(const FFleetUnit& U) const
		{
			const FFleetRaceGroup& G = Groups[U.Group];
			const double Share = ((double)(U.Lane % std::max(1, G.Entries)) + 1.0) / ((double)std::max(1, G.Entries) + 1.0);
			return G.Course.LineBoat + (G.Course.LinePin - G.Course.LineBoat) * (0.12 + 0.76 * Share);
		}

		/** The first order of a stage (called whenever a unit's stage or work changes). */
		void Enter(FFleetUnit& U, int I)
		{
			U.Helm.bArrived = false;
			U.Timer = 0.0;
			U.bVia = false;
			switch (U.Stage)
			{
			case EFleetStage::Out1:
				U.Goal = CorridorEnd + P->LaunchDir.Left() * (((I % 5) - 2) * 2.0);
				SailTo(U, U.Goal, 14.0, 40.0);
				break;
			case EFleetStage::Out2:
				U.Goal = EntryPoint(U);
				SailTo(U, U.Goal, 22.0);
				break;
			case EFleetStage::Home1:
				U.Goal = CorridorEnd + P->LaunchDir.Left() * (((I % 5) - 2) * 2.5);
				SailTo(U, U.Goal, 16.0, 60.0);
				break;
			case EFleetStage::Home2:
			{
				U.Goal = U.Beach;
				FOrder O;
				O.Kind = EOrder::Ashore;
				O.X = U.Beach.X; O.Y = U.Beach.Y; O.Radius = 3.5; O.Lane = 10.0;
				HelmOrder(U.Helm, O);
				U.bStation = false;
				break;
			}
			case EFleetStage::Work:
				EnterWork(U, I);
				break;
			default:
				break;
			}
		}

		void EnterWork(FFleetUnit& U, int I)
		{
			U.Sub = 0;
			U.Pen = -1;
			if (Mode == EFleetMode::Race)
			{
				if (U.Group < 0 || U.Tracker.Status == ERaceStatus::Finished || Groups[U.Group].bClosed || (!bRaceRunning && !bHeld))
				{
					U.Sub = 5;
					StationAt(U, U.Group >= 0 ? PreStart(U) : Middle, 14.0);
				}
				else if (U.Tracker.bStarted)
				{
					U.Sub = 3;
					RouteOrder(U, Groups[U.Group].Route);
				}
				else
				{
					StationAt(U, PreStart(U), 11.0);
				}
				return;
			}
			if (Drill == "follow")
			{
				FOrder O;
				O.Kind = EOrder::Follow;
				O.Slot = I + 1;
				O.Gap = 11.0;
				if (Launch) { O.LeaderX = Launch->X; O.LeaderY = Launch->Y; O.LeaderYaw = Launch->Yaw; O.LeaderSpeed = Launch->Speed; }
				HelmOrder(U.Helm, O);
				U.bStation = false;
			}
			else if ((Drill == "triangle" || Drill == "figure8") && !DrillRoute.empty())
			{
				U.Leg = (U.Leg / 3 * 3) % (int)DrillRoute.size();
				RouteOrder(U, DrillRoute);
			}
			else if (Drill == "tack_whistle")
			{
				U.Sub = 1;      // beating up
				SailTo(U, TackTarget(U), 20.0, 400.0);
			}
			else if (Drill == "starts" && DrillCourse.bOk)
			{
				StationAt(U, StartsWait(I), 10.0);
			}
			else if (Drill == "capsize")
			{
				const double Ang = 2.0 * SchoolPi * (I % 8) / 8.0;
				StationAt(U, Station + FSchoolV2(std::cos(Ang), std::sin(Ang)) * 28.0, 14.0);
			}
			else
			{
				U.Free = FreePoint(U);
				SailTo(U, U.Free, 15.0);
			}
		}

		FSchoolV2 StartsWait(int I) const
		{
			const FSchoolV2 Along = (DrillCourse.LinePin - DrillCourse.LineBoat);
			return DrillCourse.LineBoat + Along * (0.15 + 0.7 * ((I % 6) + 0.5) / 6.0) - DrillCourse.Up * (24.0 + 12.0 * (I % 3));
		}

		/** The tacking drill's aim: a point well to windward on the side of the whistle's tack, so she sails close-hauled on it. */
		FSchoolV2 TackTarget(const FFleetUnit& U) const
		{
			const FSchoolV2 Up = WindTo * -1.0;
			const FSchoolV2 At(U.Boat ? U.Boat->X : Middle.X, U.Boat ? U.Boat->Y : Middle.Y);
			return At + Up * 260.0 + Up.Left() * (260.0 * 1.25 * TackSide);
		}

		void RouteOrder(FFleetUnit& U, const std::vector<FFleetWp>& Route)
		{
			if (Route.empty())
			{
				return;
			}
			const FFleetWp& W = Route[(size_t)(U.Leg % (int)Route.size())];
			SailTo(U, FSchoolV2(W.X, W.Y), W.R, 70.0);
		}

		/** A unit's orders as things happen (every tick; cheap). */
		void Orders(FFleetUnit& U, int I, double Dt)
		{
			FBoat& B = *U.Boat;
			const FSchoolV2 At(B.X, B.Y);
			U.Timer += Dt;
			// the pier between her and where she is going (out, across or home): round its seaward end first
			if (bPier && U.Stage != EFleetStage::Work)
			{
				U.ViaT += Dt;
				FSchoolV2 Pa, Pb;
				const bool bCross = FleetSegSeg(At, U.Goal, Obstacles[0].A, Obstacles[0].B, Pa, Pb) < Obstacles[0].R * (U.bVia ? 1.3 : 0.5)
					&& (At - PierVia).Len() > 12.0;
				if (bCross != U.bVia && U.ViaT > 4.0)
				{
					U.ViaT = 0.0;
					if (bCross)
					{
						SailTo(U, PierVia, 10.0);
						U.Helm.bArrived = false;
						U.bVia = true;
					}
					else
					{
						Enter(U, I);
					}
				}
				if (U.bVia)
				{
					return;
				}
			}
			switch (U.Stage)
			{
			case EFleetStage::Out1:
				if (U.Helm.bArrived || (At - CorridorEnd).Len() < 25.0 || (At - P->LaunchOrigin).Dot(P->LaunchDir) > 105.0)
				{
					U.Stage = EFleetStage::Out2;
					Enter(U, I);
				}
				break;
			case EFleetStage::Out2:
			{
				const FSchoolArea* A = AreaOf(U);
				if (U.Helm.bArrived || (A && SchoolInside(*A, At) && SchoolEdgeDistance(*A, At) > 14.0))
				{
					U.Stage = EFleetStage::Work;
					U.bInArea = A != nullptr && SchoolInside(*A, At);
					Enter(U, I);
				}
				break;
			}
			case EFleetStage::Home1:
				if (U.Helm.bArrived || (At - CorridorEnd).Len() < 30.0
					|| ((At - P->LaunchOrigin).Dot(P->LaunchDir) < 110.0 && std::fabs((At - P->LaunchOrigin).Dot(P->LaunchDir.Left())) < 16.0))
				{
					U.Stage = EFleetStage::Home2;
					Enter(U, I);
				}
				break;
			case EFleetStage::Work:
				Keep(U);
				if (Mode == EFleetMode::Race) RaceOrders(U, I);
				else if (Mode == EFleetMode::Drill) DrillOrders(U, I);
				break;
			default:
				break;
			}
		}

		void DrillOrders(FFleetUnit& U, int I)
		{
			FBoat& B = *U.Boat;
			if (Drill == "follow")
			{
				if (Launch)
				{
					FOrder& O = U.Helm.Order;
					O.LeaderX = Launch->X; O.LeaderY = Launch->Y; O.LeaderYaw = Launch->Yaw; O.LeaderSpeed = Launch->Speed;
				}
				return;
			}
			if ((Drill == "triangle" || Drill == "figure8") && !DrillRoute.empty())
			{
				if (U.Helm.bArrived)
				{
					U.Leg = (U.Leg + 1) % (int)DrillRoute.size();
					if (U.Leg == 0) U.Laps++;
					RouteOrder(U, DrillRoute);
				}
				return;
			}
			if (Drill == "tack_whistle")
			{
				const FSchoolV2 At(B.X, B.Y);
				if (U.Sub == 1 && ((At - Gate[1]).Dot(WindTo * -1.0) > 0.0 || U.Helm.bArrived))
				{
					U.Sub = 2;      // at the weather gate: run back to the lee one
					U.Laps++;
					SailTo(U, Gate[0] + WindTo.Left() * (((I % 5) - 2) * 14.0), 22.0);
				}
				else if (U.Sub == 2 && U.Helm.bArrived)
				{
					U.Sub = 1;
					SailTo(U, TackTarget(U), 20.0, 400.0);
				}
				return;
			}
			if (Drill == "starts" && DrillCourse.bOk)
			{
				// three minutes a round: wait below the line, go for it in the last half minute, sail on a little, come back
				const double Phase = std::fmod(Clock, 180.0);
				if (U.Sub == 0 && Phase > 150.0 - 20.0 * (1.0 - U.Helm.Skill))
				{
					U.Sub = 1;
					SailTo(U, StartsWait(I) + DrillCourse.Up * 85.0, 12.0, 30.0);
				}
				else if (U.Sub == 1 && (U.Helm.bArrived || Phase < 100.0) && Phase > 20.0 && Phase < 150.0)
				{
					U.Sub = 0;
					U.Laps++;
					StationAt(U, StartsWait(I), 10.0);
				}
				return;
			}
			if (Drill == "capsize")
			{
				return;     // ThinkDrill tips them over one at a time by the launch
			}
			if (U.Helm.bArrived || U.Timer > 240.0)
			{
				U.Timer = 0.0;
				U.Laps++;
				U.Free = FreePoint(U);
				SailTo(U, U.Free, 15.0);
			}
		}

		void ThinkDrill(double Dt)
		{
			if (Drill == "tack_whistle")
			{
				WhistleIn -= Dt;
				if (WhistleIn <= 0.0)
				{
					WhistleIn = 20.0 + 25.0 * Rng.U();
					TackSide = -TackSide;
					bool bAny = false;
					for (size_t I = 0; I < Units.size(); I++)
					{
						FFleetUnit& U = Units[I];
						if (U.bHero || U.Stage != EFleetStage::Work || U.Sub != 1 || !U.Boat) continue;
						SailTo(U, TackTarget(U), 20.0, 400.0);
						bAny = true;
					}
					if (bAny) Emit(EFleetSignal::WhistleShort, -1);
				}
			}
			else if (Drill == "starts" && DrillCourse.bOk)
			{
				const double Phase = std::fmod(Clock, 180.0), Was = std::fmod(Clock - Dt + 180.0, 180.0);
				if (Was < 120.0 && Phase >= 120.0) Emit(EFleetSignal::WhistleLong, -1);
				if (Was < 150.0 && Phase >= 150.0) Emit(EFleetSignal::WhistleShort, -1);
			}
			else if (Drill == "capsize" && Launch)
			{
				CapsizeIn -= Dt;
				if (CapsizeIn <= 0.0)
				{
					CapsizeIn = 45.0;
					// the next one near the launch who has gone over the fewest times
					int Best = -1;
					for (size_t I = 0; I < Units.size(); I++)
					{
						const FFleetUnit& U = Units[I];
						if (U.bHero || U.Stage != EFleetStage::Work || !U.Boat || U.Boat->State != EState::Sailing) continue;
						if (std::hypot(U.Boat->X - Launch->X, U.Boat->Y - Launch->Y) > 60.0) continue;
						if (Best < 0 || U.Capsizes < Units[(size_t)Best].Capsizes) Best = (int)I;
					}
					if (Best >= 0)
					{
						FFleetUnit& U = Units[(size_t)Best];
						Place(*U.Boat, U.Boat->X, U.Boat->Y, U.Boat->Yaw, EState::Capsized);
						U.Capsizes++;
						Emit(EFleetSignal::WhistleShort, -1);
						Emit(EFleetSignal::Capsize, Best);
					}
				}
			}
		}

		// ---- the race -----------------------------------------------------------------------------------------------------

		void BeginSequence()
		{
			RaceT = Sequence.empty() ? 0.0 : Sequence[0].T - 0.01;
			SeqAt = 0;
			bRaceRunning = true;
			bXUp = false;
			for (int G = 0; G < 2; G++)
			{
				Groups[G].FirstFinish = -1.0;
				Groups[G].Finished = 0;
				Groups[G].bClosed = !Groups[G].bOn;
			}
			for (size_t I = 0; I < Units.size(); I++)
			{
				FFleetUnit& U = Units[I];
				U.Tracker.Reset();
				U.Place = 0;
				U.Sub = 0;
				U.Leg = 0;
				U.Pen = -1;
				// how long before the gun she goes for the line: her own guess of the time it takes, her own error
				FSchoolRng R(SchoolHash(Seed + (uint32_t)GeneralRecalls * 31u, (uint32_t)I * 7919u + 3u));
				R.Next();
				const double Sk = U.bHero ? 0.5 : U.Helm.Skill;
				U.RunAt = R.N() * (1.0 - Sk) * 5.0 + (1.0 - Sk) * 3.0;       // seconds late (negative: early)
			}
		}

		void ThinkRace(double Dt)
		{
			if (bHeld)
			{
				// the warning signal waits for the whole fleet to come to the line; after twenty minutes for four boats in five; half an hour at most
				HoldT += Dt;
				int Want = 0, Ready = 0;
				for (const FFleetUnit& U : Units)
				{
					if (U.Group < 0 || U.bHero) continue;
					Want++;
					if (U.Stage == EFleetStage::Work && U.Boat && (FSchoolV2(U.Boat->X, U.Boat->Y) - PreStart(U)).Len() < 75.0) Ready++;
				}
				if ((Want > 0 && Ready == Want && HoldT > 20.0) || (Want > 0 && Ready * 5 >= Want * 4 && HoldT > 1200.0) || HoldT > 1800.0)
				{
					bHeld = false;
					BeginSequence();
				}
				return;
			}
			if (!bRaceRunning)
			{
				return;
			}
			const double Was = RaceT;
			RaceT += Dt;
			while (SeqAt < (int)Sequence.size() && Sequence[(size_t)SeqAt].T <= RaceT)
			{
				const FRaceSignal& S = Sequence[(size_t)SeqAt];
				Emit(S.bUp ? EFleetSignal::FlagUp : EFleetSignal::FlagDown, -1, SeqAt);
				for (int H = 0; H < S.Horns; H++) Emit(S.bLong ? EFleetSignal::HornLong : EFleetSignal::HornShort, -1);
				SeqAt++;
			}
			(void)Was;
		}

		/** How long she needs from her waiting place to the line, close-hauled. */
		double RunTime(const FFleetUnit& U) const
		{
			const FRaceCourse& C = Groups[U.Group].Course;
			(void)C;
			const double Way = std::max(4.0, (LinePoint(U) - FSchoolV2(U.Boat->X, U.Boat->Y)).Len());
			const int Cl = ClassOf(U);
			const double Tws = std::hypot(U.Env.WindX, U.Env.WindY);
			const double Reach = BeatAngle(EBoat(Cl), Tws) + 16.0 * Pi / 180.0;
			const double V = std::max(0.35, PolarSpeed(EBoat(Cl), Reach, Tws) * (0.72 + 0.28 * U.Helm.Skill));
			return Way / V * 1.12 + 7.0;
		}

		void RaceOrders(FFleetUnit& U, int I)
		{
			if (U.Group < 0)
			{
				return;
			}
			FFleetRaceGroup& G = Groups[U.Group];
			const FRaceCourse& C = G.Course;
			const FSchoolV2 At(U.Boat->X, U.Boat->Y);
			// a touch of a mark: a turn round four points, then on
			if (U.Pen >= 0)
			{
				if (U.Helm.bArrived || U.Timer > 40.0)
				{
					U.Timer = 0.0;
					U.Pen++;
					if (U.Pen >= 13 || U.Tracker.PenaltiesOwed == 0)
					{
						U.Pen = -1;
						if (U.Sub == 3) RouteOrder(U, G.Route); else EnterSub(U, I);
					}
					else
					{
						const double Ang = SchoolPi * 0.5 * U.Pen;
						SailTo(U, U.PenAt + FSchoolV2(std::cos(Ang), std::sin(Ang)) * 7.0, 3.5, 15.0);
					}
				}
				return;
			}
			switch (U.Sub)
			{
			case 0:     // waiting below the line
				if (bRaceRunning && !G.bClosed && RaceT >= -RunTime(U) + U.RunAt)
				{
					U.Sub = 1;
					EnterSub(U, I);
				}
				break;
			case 1:     // going for the line
				if (U.bLineUp && !U.Tracker.bStarted && !U.Tracker.bOcs)
				{
					if (U.Helm.bArrived || BelowLine(U, At))
					{
						U.bLineUp = false;
						SailTo(U, LinePoint(U) + StartCourse(U) * 45.0, 10.0, 25.0);
						U.Helm.bArrived = false;
					}
					break;
				}
				if (U.Tracker.bStarted)
				{
					U.Sub = 3;
					U.Leg = 0;
					RouteOrder(U, G.Route);
				}
				else if (U.Tracker.bOcs)
				{
					U.Sub = 2;
					EnterSub(U, I);
				}
				else if (U.Helm.bArrived || (RaceT > 0.0 && (At - C.LineMid()).Dot(C.Up) > 12.0))
				{
					// across with no start to her name (round an end, or before the gun and back): below the line and again
					U.Sub = 2;
					EnterSub(U, I);
				}
				break;
			case 2:     // back below the line
				if ((!U.Tracker.bOcs && (At - C.LineMid()).Dot(C.Up) < -6.0) || U.Helm.bArrived)
				{
					U.Sub = RaceT >= -10.0 ? 1 : 0;
					EnterSub(U, I);
				}
				break;
			case 3:     // round the marks
				if (U.Tracker.Next >= (int)C.Order.size())
				{
					U.Sub = 4;
					EnterSub(U, I);
				}
				else if (U.Helm.bArrived)
				{
					const FFleetWp& W = G.Route[(size_t)(U.Leg % (int)G.Route.size())];
					if (W.Part == 2 && U.Tracker.Next <= W.Rounding)
					{
						U.Leg = W.Rounding * 3;         // the rounding did not count: round it again
					}
					else
					{
						U.Leg = std::min(U.Leg + 1, (int)G.Route.size() - 1);
						if (W.Part == 2) U.Leg = std::max(U.Leg, U.Tracker.Next * 3);
					}
					RouteOrder(U, G.Route);
				}
				else if (U.Tracker.Next * 3 > U.Leg + 2)
				{
					U.Leg = U.Tracker.Next * 3;         // rounded sooner than her points said
					RouteOrder(U, G.Route);
				}
				break;
			case 4:     // to the finish
				if (U.Tracker.Status == ERaceStatus::Finished || U.Tracker.Status == ERaceStatus::Dsq)
				{
					U.Sub = 5;
					EnterSub(U, I);
				}
				else if (U.Helm.bArrived)
				{
					// missed the line: up above it and down again
					U.Sub = (At - C.LineMid()).Dot(C.Up) > 8.0 ? 4 : 6;
					EnterSub(U, I);
				}
				break;
			case 6:     // above the line again
				if (U.Helm.bArrived || (At - C.LineMid()).Dot(C.Up) > 14.0)
				{
					U.Sub = 4;
					EnterSub(U, I);
				}
				break;
			default:    // done: wait below the line, out of the way
				break;
			}
		}

		/** Under the line and between its ends (a little inside them). */
		bool BelowLine(const FFleetUnit& U, const FSchoolV2& At) const
		{
			const FRaceCourse& C = Groups[U.Group].Course;
			const FSchoolV2 Along = C.LinePin - C.LineBoat;
			const double L2 = std::max(1.0, Along.Dot(Along));
			const double S = (At - C.LineBoat).Dot(Along) / L2;
			return S > 0.1 && S < 0.9 && (At - C.LineBoat).Dot(C.Up) < -2.0;
		}

		void EnterSub(FFleetUnit& U, int I)
		{
			(void)I;
			const FFleetRaceGroup& G = Groups[U.Group];
			const FRaceCourse& C = G.Course;
			U.Helm.bArrived = false;
			switch (U.Sub)
			{
			case 0: StationAt(U, PreStart(U), 11.0); break;
			case 1:
				// from beside the line's end she would sail round it: first under the line, then across
				U.bLineUp = !U.Boat || !BelowLine(U, FSchoolV2(U.Boat->X, U.Boat->Y));
				if (U.bLineUp) SailTo(U, LinePoint(U) - StartCourse(U) * 16.0, 6.0, 25.0); else SailTo(U, LinePoint(U) + StartCourse(U) * 45.0, 10.0, 25.0);
				break;
			case 2: SailTo(U, LinePoint(U) - StartCourse(U) * 22.0, 7.0, 25.0); break;
			case 3: RouteOrder(U, G.Route); break;
			case 4: SailTo(U, C.LineMid() + (LinePoint(U) - C.LineMid()) * 0.6 - C.Up * 24.0, 8.0, 30.0); break;
			case 6: SailTo(U, C.LineMid() + C.Up * 22.0, 8.0, 30.0); break;
			default: StationAt(U, C.LineMid() - C.Up * (60.0 + 12.0 * (U.Lane % 3)) + C.Up.Left() * (((U.Lane % 7) - 3) * 12.0), 16.0); break;
			}
		}

		void AfterRace(double Dt)
		{
			(void)Dt;
			int OcsNow = 0, OverAtGun = 0, Racers = 0;
			for (size_t I = 0; I < Units.size(); I++)
			{
				FFleetUnit& U = Units[I];
				if (U.Group < 0 || !U.Boat || !U.Afloat() || Groups[U.Group].bClosed) continue;
				FFleetRaceGroup& G = Groups[U.Group];
				Racers++;
				const bool bGunStep = !U.Tracker.bGun;
				const FSchoolV2 At(U.Boat->X, U.Boat->Y);
				for (const ERaceEvent E : U.Tracker.Step(*P, G.Course, RaceT, At, U.Boat->Yaw * 180.0 / SchoolPi))
				{
					switch (E)
					{
					case ERaceEvent::Ocs: Emit(EFleetSignal::Ocs, (int)I); if (bGunStep) OverAtGun++; break;
					case ERaceEvent::OcsCleared: Emit(EFleetSignal::OcsCleared, (int)I); break;
					case ERaceEvent::Started: Emit(EFleetSignal::Started, (int)I, -1, U.Tracker.StartT); break;
					case ERaceEvent::Rounded: Emit(EFleetSignal::Rounded, (int)I, U.Tracker.Next); break;
					case ERaceEvent::WrongSide: Emit(EFleetSignal::WrongSide, (int)I); break;
					case ERaceEvent::Touch:
						Emit(EFleetSignal::Touch, (int)I);
						if (!U.bHero && U.Pen < 0)
						{
							U.Pen = 0;
							U.Timer = 0.0;
							U.PenAt = At + (At - G.Course.Waypoint(U.Tracker.Next)).Norm() * 16.0;
							SailTo(U, U.PenAt + FSchoolV2(7.0, 0.0), 3.5, 15.0);
						}
						break;
					case ERaceEvent::Finished:
						G.Finished++;
						U.Place = G.Finished;
						if (G.FirstFinish < 0.0) G.FirstFinish = RaceT;
						Emit(EFleetSignal::Finished, (int)I, -1, (double)G.Finished);
						break;
					default: break;
					}
				}
				if (U.Tracker.bOcs) OcsNow++;
			}
			// the gun's step: a general recall when more than half were over
			if (OverAtGun > 0 && RaceGeneralRecall(*P, OverAtGun, Racers) && GeneralRecalls < 2)
			{
				GeneralRecalls++;
				Emit(EFleetSignal::GeneralRecall, -1);
				Emit(EFleetSignal::HornShort, -1);
				Emit(EFleetSignal::HornShort, -1);
				BeginSequence();
				for (size_t I = 0; I < Units.size(); I++)
				{
					FFleetUnit& U = Units[I];
					if (U.Group >= 0 && U.Stage == EFleetStage::Work && !U.bHero) EnterWork(U, (int)I);
				}
				return;
			}
			const bool bX = OcsNow > 0 && RaceT < P->XFlagS;
			if (bX != bXUp)
			{
				bXUp = bX;
				Emit(EFleetSignal::FlagX, -1, -1, bX ? 1.0 : 0.0);
				if (bX) Emit(EFleetSignal::HornShort, -1);
			}
			// the time limits: the first boat's by the class, the rest's window after her
			bool bAllClosed = true;
			for (int Gi = 0; Gi < 2; Gi++)
			{
				FFleetRaceGroup& G = Groups[Gi];
				if (!G.bOn || G.bClosed) continue;
				int Left = 0;
				for (const FFleetUnit& U : Units)
				{
					if (U.Group == Gi && U.Afloat() && (U.Tracker.Status == ERaceStatus::Racing || U.Tracker.Status == ERaceStatus::Waiting || U.Tracker.Status == ERaceStatus::Ocs)) Left++;
				}
				const double Limit = P->TimeLimitS[std::max(0, std::min(SchoolBoats - 1, G.Course.Class))];
				const bool bOut = (G.FirstFinish < 0.0 && RaceT > Limit) || (G.FirstFinish >= 0.0 && RaceT > G.FirstFinish + P->FinishWindowS);
				if ((Left == 0 && RaceT > 0.0) || bOut)
				{
					G.bClosed = true;
					for (FFleetUnit& U : Units) if (U.Group == Gi) U.Tracker.Close();
				}
				else bAllClosed = false;
			}
			if (bAllClosed)
			{
				CloseRace(false);
			}
		}

		void CloseRace(bool bAbandon)
		{
			if (!bRaceRunning)
			{
				return;
			}
			bRaceRunning = false;
			for (int Gi = 0; Gi < 2; Gi++) Groups[Gi].bClosed = true;
			for (size_t I = 0; I < Units.size(); I++)
			{
				FFleetUnit& U = Units[I];
				if (U.Group < 0) continue;
				if (!bAbandon) U.Tracker.Close();
				if (U.Stage == EFleetStage::Work && !U.bHero && U.Boat && U.Afloat())
				{
					U.Sub = 5;
					U.Pen = -1;
					EnterSub(U, (int)I);
				}
			}
			if (bXUp) { bXUp = false; Emit(EFleetSignal::FlagX, -1, -1, 0.0); }
			Emit(bAbandon ? EFleetSignal::RaceAbandoned : EFleetSignal::RaceOver, -1);
			if (!bAbandon) Emit(EFleetSignal::HornLong, -1);
		}

		// ---- the launch ----------------------------------------------------------------------------------------------------

		void ThinkLaunch(double Dt)
		{
			if (!Launch)
			{
				return;
			}
			const FSchoolV2 At(Launch->X, Launch->Y);
			// a boat on the tow line: home with her, let her go on the beach
			if (Towing >= 0)
			{
				FFleetUnit& U = Units[(size_t)Towing];
				LaunchTarget = Home;
				LaunchCruise = P->TowSpeed;
				LaunchRadius = 6.0;
				if ((At - Home).Len() < 12.0 || U.Stage != EFleetStage::Towed)
				{
					if (U.Stage == EFleetStage::Towed && U.Boat)
					{
						Place(*U.Boat, U.Beach.X, U.Beach.Y, std::atan2(-P->LaunchDir.Y, -P->LaunchDir.X), EState::Ashore);
						U.Stage = EFleetStage::Landed;
						Emit(EFleetSignal::TowEnd, Towing);
						Emit(EFleetSignal::Landed, Towing);
					}
					Towing = -1;
				}
				return;
			}
			if (bRecall) RecallT += Dt;
			TaskIn -= Dt;
			if (TaskIn <= 0.0)
			{
				TaskIn = 0.5;
				States.clear();
				for (const FFleetUnit& U : Units)
				{
					FSchoolBoatState S;
					S.bAshore = !U.Boat || !U.Afloat() || U.Stage == EFleetStage::Towed;
					if (U.Boat)
					{
						S.Pos = FSchoolV2(U.Boat->X, U.Boat->Y);
						S.bCapsized = U.CapsizedS > 0.0;
						S.CapsizedS = U.CapsizedS;
						S.bKid = U.Boat->Class == EBoat::Optimist;
						// on the way out and home nobody has strayed: only a boat at work can be out of her water
						if (U.Stage != EFleetStage::Work && !S.bCapsized && !(bRecall && S.bKid)) S.bAshore = true;
					}
					States.push_back(S);
				}
				Task = SchoolLaunchTask(*P, States, At, LaunchStation(), bRecall, Home);
				if (Task.Kind == FSchoolLaunchTask::Shepherd && Task.Boat >= 0 && Units[(size_t)Task.Boat].Stage != EFleetStage::Work) Task = FSchoolLaunchTask();
				// the one who must be fetched; then, long after the recall, whoever is still out (the farthest first)
				int Fetch = ForceTow >= 0 && ForceTow < (int)Units.size() && Units[(size_t)ForceTow].Boat && Units[(size_t)ForceTow].Afloat() ? ForceTow : -1;
				if (Fetch < 0 && bRecall && RecallT > StragglerS && (Task.Boat < 0 || Task.Kind == FSchoolLaunchTask::Station))
				{
					double Far = 45.0;
					for (size_t I = 0; I < Units.size(); I++)
					{
						const FFleetUnit& U = Units[I];
						if (!U.Boat || !U.Afloat() || U.bHero || U.Stage == EFleetStage::Towed) continue;
						const double D = (FSchoolV2(U.Boat->X, U.Boat->Y) - Home).Len();
						if (D > Far) { Far = D; Fetch = (int)I; }
					}
				}
				if (Fetch >= 0 && (Task.Boat < 0 || Task.Kind != FSchoolLaunchTask::Right))
				{
					Task.Kind = FSchoolLaunchTask::Tow;
					Task.Boat = Fetch;
				}
			}
			LaunchRadius = 8.0;
			if (Task.Kind == FSchoolLaunchTask::Station || Task.Boat < 0)
			{
				HelpT = 0.0;
				LaunchTarget = Task.Boat < 0 && bRecall ? Home : LaunchStation();
				LaunchCruise = Mode == EFleetMode::Drill && Drill == "follow" && !bRecall ? LeadSpeed() : 3.5;
				LaunchRadius = Mode == EFleetMode::Drill && Drill == "follow" && !bRecall ? 3.0 : (Mode == EFleetMode::Race ? 2.5 : 8.0);
				return;
			}
			FFleetUnit& U = Units[(size_t)Task.Boat];
			const FSchoolV2 Bt(U.Boat->X, U.Boat->Y);
			// stand off her weather side, not on top of her
			LaunchTarget = Bt + (At - Bt).Norm() * 5.0;
			LaunchCruise = P->LaunchSpeed;
			LaunchRadius = 3.0;
			if ((At - Bt).Len() < 9.0)
			{
				HelpT += Dt;
				if (Task.Kind == FSchoolLaunchTask::Tow && HelpT > 3.0)
				{
					U.Stage = EFleetStage::Towed;
					Towing = Task.Boat;
					if (ForceTow == Towing) ForceTow = -1;
					Task = FSchoolLaunchTask();
					TaskIn = 0.0;
					HelpT = 0.0;
					Emit(EFleetSignal::TowStart, Task.Boat);
				}
				else if (Task.Kind == FSchoolLaunchTask::Right && HelpT > 6.0 && U.CapsizedS > 0.0 && !U.bHero)
				{
					// the coach has her up: swamped, the crew bails
					Place(*U.Boat, U.Boat->X, U.Boat->Y, U.Boat->Yaw, EState::Swamped);
					U.CapsizedS = 0.0;
					HelpT = 0.0;
					Emit(EFleetSignal::Helped, Task.Boat);
				}
			}
			else HelpT = 0.0;
		}

		FSchoolV2 LaunchStation()
		{
			if (Mode == EFleetMode::Drill && Drill == "follow" && !LeadPath.empty())
			{
				return LeadPath[(size_t)LeadAt];
			}
			if (Mode == EFleetMode::Idle) return Home;
			return Station;
		}

		/** The file's leader: 1.2 to 2 m/s, slower while the last duckling is far behind; round its loop for ever. */
		double LeadSpeed()
		{
			const FSchoolV2 At(Launch->X, Launch->Y);
			if (LeadHold > 0.0)
			{
				LeadHold -= 1.0 / 30.0;
				if (LeadAt == 0 && (At - LeadPath[0]).Len() < 10.0) return 0.0;
			}
			if ((At - LeadPath[(size_t)LeadAt]).Len() < 9.0 && (LeadAt != 0 || LeadHold <= 0.0))
			{
				LeadAt = LeadAt + 1 >= (int)LeadPath.size() ? 1 : LeadAt + 1;
			}
			double Far = 0.0;
			int N = 0;
			for (const FFleetUnit& U : Units)
			{
				if (U.bHero || U.Stage != EFleetStage::Work || !U.Boat) continue;
				Far = std::max(Far, (FSchoolV2(U.Boat->X, U.Boat->Y) - At).Len());
				N++;
			}
			if (LeadAt == 0) return 3.0;
			return Far > 11.0 * N + 45.0 ? 1.2 : 1.8;
		}
	};
}
