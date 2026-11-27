// The yacht school's dinghies, in plain C++ (no Unreal types) so they can be run and checked outside the engine
// (tools/sailsim/run.sh). ALyudDinghy / ULyudSailing wrap it for the game; docs/yacht_school.md "Sail physics".
//
// A dinghy here is a body on the water plane (surge, sway, yaw) that also heels: 4 degrees of freedom, with the
// waves' pitch and roll taken from the sea surface the caller samples. Three classes (the contract's table):
// the «Оптимист» pram, the two-handed «Кадет» with a jib, and the «Луч» single-hander.
//
//   - Air: the apparent wind is the true wind at the boat (the caller samples the game's wind with its gusts)
//     minus the boat's own motion. The boom swings to leeward as far as the sheet lets it; with too small an
//     angle of attack the sail luffs (LuffAmount), attached flow gives lift upwind, a stalled plate pushes
//     downwind. That makes the no-go zone, the fast reach, the slow rolly run, the gybe's slam, and irons.
//   - Water: friction + a wave-making hump at hull speed, and planing for the cadet and the luch in a blow;
//     the centreboard and rudder are foils (leeway, more with the board up; no steerage without way; the rudder
//     reverses when the boat goes astern); weather helm grows with heel; waves add resistance.
//   - Heel: the hull's form, the crew's weight where it sits (Hike), the sail's force on its lever. Too much
//     heel spills wind and ships water; past the limit the boat capsizes, may turn turtle, is righted from the
//     centreboard, comes up swamped and is bailed.
//   - FHelm is the helmsman every NPC boat and every test sails with (skill 0..1, seeded character); FLaunch
//     is the coach's motor boat; StepFar moves a distant boat from a polar table for almost nothing.
//
// Frame: world metres, the water plane XY (Unreal world / 100; north = +Y). Yaw is the Unreal yaw in RADIANS:
// forward = (cos Yaw, sin Yaw), starboard (right) = (-sin Yaw, cos Yaw), so a growing yaw turns to starboard.
// Angles in radians, speeds m/s, forces N. Signed "side" values are + to starboard: heel (+ = starboard rail
// down, Unreal roll), boom and jib angle (+ = boom out to starboard), hike (+ = crew weight to starboard),
// rudder (+ = turn to starboard), wind angles (+ = the wind comes over the starboard side: starboard tack).
// Everything is deterministic for the same inputs and seeds; Step allocates nothing.
#pragma once

#include <cstdint>

namespace LyudSail
{
	constexpr double Pi = 3.14159265358979323846;
	constexpr double Gravity = 9.81;
	constexpr double AirDensity = 1.225;
	constexpr double WaterDensity = 1010.0;   // the Baltic is brackish

	enum class EBoat : uint8_t { Optimist, Cadet, Luch };
	constexpr int BoatCount = 3;

	/** One class of boat. Lengths m, areas m^2, masses kg, angles rad. */
	struct FBoatDef
	{
		const char* Id;
		double Loa, Lwl, Beam;
		double MainArea, JibArea;       // the contract's sail area = MainArea + JibArea
		double HullMass;                // all up without crew
		int Crew;                       // people aboard
		double CrewMass;                // default total crew weight
		double BoomLen;                 // foot of the mainsail
		double CeHeight;                // centre of effort over the water
		double MastX;                   // mast ahead of the centre of lateral resistance
		double BoomMin, BoomMax;        // boom angle with the sheet hard in / all out
		double JibMin, JibMax;
		double BoardArea, BoardLen;     // centreboard: area and depth under the hull when down
		double RudderArea, RudderX;     // rudder: area and how far aft of the centreboard
		double HullDraft;               // the hull alone draws this much
		double HullLatArea;             // hull's own lateral area under water
		double Wetted;                  // wetted surface
		double Windage;                 // drag area of hull, spars and crew, m^2
		double Planing;                 // 0..1 how readily it planes
		double PlaneDrag;               // planing resistance as a share of weight
		double FormK;                   // form stability: peak righting moment = weight * Beam * FormK
		double HikeArm;                 // crew's centre of gravity off the centreline when fully hiked
		double SwampHeel;               // heel at which water comes over the lee rail
		double WaterCap;                // kg of water a full cockpit holds
		double RightWater;              // 0..1 how full it comes up after a capsize
		double SelfBail;                // 1/s the water drains by itself when moving
		double RightTime;               // s an average crew needs to right it
		double MastLen;
	};
	const FBoatDef& Def(EBoat Class);
	const char* BoatId(EBoat Class);
	/** Class by the contract's id ("optimist", "cadet", "luch"); false when unknown. */
	bool BoatById(const char* Id, EBoat& Out);
	/** A plausible crew weight for the class: one sailor of this age (both, for the cadet), kg. */
	double CrewMassFor(EBoat Class, double AgeYears);

	enum class EState : uint8_t
	{
		Ashore,     // on its trolley or the sand: nothing moves
		Moored,     // on a buoy or held at the slip: swings to the wind
		Sailing,
		Swamped,    // sailing, but full of water: sluggish until bailed
		Capsized,   // on its side
		Turtled,    // upside down (rare)
		Righting,   // the crew is on the centreboard
		Aground,    // the board or the hull is on the bottom
		Towed,      // on a line behind the launch (StepTowed)
	};
	const char* StateName(EState S);

	enum class EPoint : uint8_t { InIrons, CloseHauled, CloseReach, BeamReach, BroadReach, Run };
	const char* PointName(EPoint P);

	enum class ETell : uint8_t { Flowing, Lifting, Stalled };

	/** FBoat::Events bits: what happened during the last Step / StepFar. */
	enum EEvent : uint32_t
	{
		EvTack = 1u << 0,           // the sail filled on the new side after going through the wind
		EvGybe = 1u << 1,           // the boom came across downwind
		EvCapsize = 1u << 2,
		EvRighted = 1u << 3,
		EvGrounded = 1u << 4,
		EvPlaningStart = 1u << 5,
		EvPlaningEnd = 1u << 6,
		EvHullSlap = 1u << 7,       // the bow fell on a wave (Out.SlapStrength)
		EvBoomSlam = 1u << 8,       // the boom fetched up on its sheet (Out.BoomSlamSpeed)
		EvTurtle = 1u << 9,
		EvSwamped = 1u << 10,
		EvFloatedOff = 1u << 11,    // came off the bottom
		EvLuff = 1u << 12,          // the sail started to flap
		EvBailed = 1u << 13,        // dry again
	};

	/** What the crew does. The model moves the real tiller, sheet, bodies and board toward these at human speeds. */
	struct FControls
	{
		double Rudder = 0.0;        // -1..1, + turns to starboard
		double Sheet = 0.5;         // 0 hard in .. 1 all out
		double JibSheet = -1.0;     // 0..1, or < 0: the jib follows the mainsheet (cadet)
		double Hike = 0.0;          // -1..1 crew weight, + to starboard
		double Board = 1.0;         // 0 up .. 1 down
		bool bRight = false;        // capsized: climb on the centreboard and right it
		bool bBail = false;         // bail the water out
	};

	/** Help for the player (the engine toggles them; the polar table is built with the first two on). */
	struct FAssist
	{
		bool bAutoSheet = false;    // the sheet trims itself and eases in gusts (Controls.Sheet is ignored)
		bool bAutoHike = false;     // the crew's weight goes where it is needed (Controls.Hike is ignored)
		bool bNoCapsize = false;    // the boat never goes over and ships no water
	};

	/** The world at the boat this step. */
	struct FEnv
	{
		double WindX = 0.0, WindY = 0.0;        // true wind velocity at sail height, gusts included (blows toward)
		double CurrentX = 0.0, CurrentY = 0.0;  // slow current (not the waves' orbital motion)
		double Depth = 100.0;                   // water depth under the boat, m
		double NormalX = 0.0, NormalY = 0.0, NormalZ = 1.0;   // the sea surface's normal at the boat (world)
		double WaveHs = 0.0;                    // significant wave height here, m
	};

	/** What the engine draws and plays. Filled by every Step / StepFar. */
	struct FOutputs
	{
		double X = 0.0, Y = 0.0, Yaw = 0.0;
		double VX = 0.0, VY = 0.0;              // over the ground
		double YawRate = 0.0;
		double Speed = 0.0;                     // through the water along the keel (negative = going astern)
		double Sog = 0.0;                       // speed over the ground
		double Leeway = 0.0;                    // + = sliding to starboard
		double Heel = 0.0, Pitch = 0.0;         // + starboard down, + bow up (wave slope and trim included in both)
		double Boom = 0.0, Jib = 0.0;           // sail angles off the centreline, + to starboard
		double MainLuff = 0.0, JibLuff = 0.0;   // 0 full and quiet .. 1 flapping
		int CamberSide = 0;                     // which way the sail bellies: +1 to starboard, -1 to port, 0 flapping
		double RudderAngle = 0.0;               // blade angle, rad, + = turning to starboard
		double Board = 1.0, Hike = 0.0, Sheet = 0.5;   // where they really are now
		double AppWindSpeed = 0.0, AppWindAngle = 0.0; // + from starboard, 0 = from ahead, pi = from astern
		double TrueWindSpeed = 0.0, TrueWindAngle = 0.0;
		EPoint Point = EPoint::BeamReach;
		bool bByTheLee = false;                 // on a run with the wind on the boom's side: a gybe is near
		ETell TellWindward = ETell::Flowing, TellLeeward = ETell::Flowing;
		double TrimError = 0.0;                 // -1 under-sheeted (sheet in) .. 0 right .. +1 over-sheeted (ease)
		double SheetLoad = 0.0, SheetLoad01 = 0.0;     // pull on the mainsheet, N, and 0..1 of a hard pull
		double HelmLoad = 0.0;                  // -1..1 push of the water on the rudder blade (+ = to starboard): the pull in the hand
		double Planing = 0.0;                   // 0 displacement .. 1 on the plane
		double Wake = 0.0, Spray = 0.0;         // 0..1
		double SlapStrength = 0.0;              // 0..1 with EvHullSlap
		double BoomSlamSpeed = 0.0;             // rad/s with EvBoomSlam / EvGybe
		double Water = 0.0;                     // 0 dry .. 1 full
		double Righting = 0.0;                  // 0..1 progress while Righting
		EState State = EState::Ashore;
		uint32_t Events = 0;
	};

	struct FBoat
	{
		EBoat Class = EBoat::Luch;
		EState State = EState::Ashore;
		double X = 0.0, Y = 0.0, Yaw = 0.0;
		double VX = 0.0, VY = 0.0, YawRate = 0.0;
		double Heel = 0.0, HeelRate = 0.0;
		double Boom = 0.0, BoomRate = 0.0, Jib = 0.0;
		// where the gear really is (the controls move toward FControls)
		double Rudder = 0.0, Sheet = 1.0, JibSheet = 1.0, Hike = 0.0, Board = 1.0;
		double Water = 0.0;
		double CrewMass = 75.0;
		double CrewSkill = 0.5;     // how fast the crew moves, rights and bails
		FAssist Assist;
		uint32_t Seed = 1;
		uint32_t Events = 0;
		FOutputs Out;
		// inner state
		double Depower = 0.0;       // auto-sheet: how far the sail is eased for the gusts, 0..1
		double PowerFull = 0.0;     // auto-sheet: the sail's push if it were trimmed full, as a share of what the crew can hold
		double HeelSlow = 0.0;      // the heel off the sea's slope, smoothed over a second (what the crew leans against)
		double HeelMoment = 0.0;    // the sails' heeling moment now, N m, + = pushing the starboard rail down
		double StateTime = 0.0;     // s in this state
		double RightNeed = 0.0, RightDone = 0.0;
		bool bWasTurtled = false;
		int BoomSide = 0;           // the side the sail last filled on
		bool bPlaning = false, bLuffing = false, bSwampedSaid = false;
		double RollPhase = 0.0, FlapPhase = 0.0;
		double WavePitch = 0.0, WaveRoll = 0.0, WavePitchRate = 0.0;
		double SlapWait = 0.0;
		bool bFar = false;          // moved by StepFar last
		double FarSpeed = 0.0;
		double TurtleRoll = 0.0;
	};

	/** A boat of a class with its crew (kg; <= 0 = the class's default), ashore at the origin. */
	void Init(FBoat& B, EBoat Class, double CrewMassKg, double CrewSkill, uint32_t Seed);
	/** Puts it somewhere in a state (speeds zeroed). Sailing = afloat and free. */
	void Place(FBoat& B, double X, double Y, double Yaw, EState State);
	/** Afloat and moving: Place(Sailing) with a speed along the keel. */
	void PlaceSailing(FBoat& B, double X, double Y, double Yaw, double Speed);

	/** One tick of the full model (any dt; it substeps at <= 1/60 s). Clears and fills B.Events and B.Out. */
	void Step(FBoat& B, const FControls& C, const FEnv& E, double Dt);

	/**
	 * A distant boat: turns toward CourseYaw and takes the polar table's speed for its angle to the wind. No
	 * forces, no neighbours; SpeedScale < 1 holds it back (station keeping, a file). The next Step() carries on
	 * from where it is with no jump.
	 */
	void StepFar(FBoat& B, double CourseYaw, double SpeedScale, const FEnv& E, double Dt);

	/** Towed by the launch: the bow follows a hitch point on a rope of RopeLen metres. */
	void StepTowed(FBoat& B, double HitchX, double HitchY, double RopeLen, const FEnv& E, double Dt);

	/** Wrapped to -pi..pi. */
	double WrapAngle(double A);
	/** World angle the wind comes FROM (so a boat with this yaw is head to wind). */
	double WindFromYaw(double WindX, double WindY);
	/** True wind angle of a heading: + = wind over the starboard side, 0 = head to wind. */
	double TrueWindAngle(double Yaw, double WindX, double WindY);
	/** The sheet (0..1) that trims the mainsail best for the boat's apparent wind now. */
	double IdealSheet(const FBoat& B);
	/** Speed made good toward a point, m/s. */
	double Vmg(const FBoat& B, double TX, double TY);
	/** How deep the boat draws now with its board where it is, m. */
	double Draught(const FBoat& B);

	// ---- the polar table -------------------------------------------------------------------------------------------

	constexpr int PolarWinds = 10, PolarAngles = 19;   // 0,1,2,3,4.5,6,8,10,12,14 m/s x 0..180 by 10 degrees
	struct FPolar
	{
		float Wind[PolarWinds];
		float Speed[PolarWinds][PolarAngles];
		float Heel[PolarWinds][PolarAngles];
		float Depower[PolarWinds][PolarAngles];
		float BeatAngle[PolarWinds], RunAngle[PolarWinds];   // best angles to the wind upwind and downwind, rad
	};
	/** The class's table, built from the full model the first time it is asked for (about 0.1 s a class). */
	const FPolar& Polar(EBoat Class);
	/** Build all three now (call at load, off the first frame). */
	void WarmPolars();
	/** Steady speed at a true wind angle (rad, any sign) in a true wind (m/s at sail height). */
	double PolarSpeed(EBoat Class, double Twa, double Tws);
	double PolarHeel(EBoat Class, double Twa, double Tws);
	/** The angle to the wind that makes the best way upwind / downwind (pi = straight down). */
	double BeatAngle(EBoat Class, double Tws);
	double RunAngle(EBoat Class, double Tws);

	// ---- the tutor ---------------------------------------------------------------------------------------------------

	enum class EAdvice : uint8_t
	{
		None,
		SheetIn,        // the sail is flapping: pull the sheet
		Ease,           // over-sheeted or overpowered: let the sheet out
		BearAway,       // pinching: turn away from the wind
		InIrons,        // head to wind and stopped: push the tiller the other way, back out
		Hike,           // lean out
		BoardUp,        // shallow water
		BoardDown,      // sliding sideways upwind with the board up
		ByTheLee,       // the wind is on the boom's side: gybe or head up
		RightBoat,      // capsized
		Bail,           // swamped
		FloatOff,       // aground: board up, get off
	};
	const char* AdviceName(EAdvice A);
	/** The one thing a coach would call out to this boat now (after a Step). */
	EAdvice Advice(const FBoat& B, const FEnv& E);

	// ---- the helmsman ------------------------------------------------------------------------------------------------

	enum class EOrder : uint8_t
	{
		Idle,       // let the sheet fly and wait
		SailTo,     // to a point (beating and gybing as needed); bArrived inside Radius
		Station,    // hold near a point within Radius (slow reaches to and fro)
		Follow,     // in file behind a leader (the coach's launch): slot Slot, Gap metres apart
		Ashore,     // sail to the beach point, board up in the shallows, run it up the sand
	};

	struct FOrder
	{
		EOrder Kind = EOrder::Idle;
		double X = 0.0, Y = 0.0;        // the point
		double Radius = 8.0;
		double LeaderX = 0.0, LeaderY = 0.0, LeaderYaw = 0.0, LeaderSpeed = 0.0;
		int Slot = 1;
		double Gap = 12.0;
		double Lane = 120.0;            // beating: how far to either side of the wind line before tacking back
	};

	/** Another boat or thing on the water the helmsman keeps clear of. */
	struct FNeighbour
	{
		double X = 0.0, Y = 0.0, VX = 0.0, VY = 0.0;
		double Length = 3.0;
		int Tack = 0;                   // +1 starboard tack, -1 port tack, 0 = not sailing (launch, mark, capsized): always keep clear
		                                // (a boat slower than 0.6 m/s is kept clear of whatever its tack)
	};

	typedef double (*FDepthFn)(void* User, double X, double Y);

	struct FHelmInput
	{
		double WindX = 0.0, WindY = 0.0;        // true wind at the boat (the same as FEnv's)
		double Depth = 100.0;                   // under the boat
		FDepthFn DepthFn = nullptr;             // depth anywhere (looked at a few points ahead, 4 times a second)
		void* DepthUser = nullptr;
		double MinDepth = 0.6;                  // water the helmsman will not go into
		const double* Poly = nullptr;           // allowed water: PolyCount points x,y (any simple polygon)
		int PolyCount = 0;
		const FNeighbour* Neighbours = nullptr;
		int NeighbourCount = 0;
	};

	enum class EManeuver : uint8_t { None, Tack, Gybe, WearRound, Irons };

	struct FHelm
	{
		double Skill = 0.5;
		uint32_t Seed = 1;
		FOrder Order;
		// read by the caller
		bool bArrived = false;          // SailTo / Ashore: there
		bool bOnStation = false;        // Station: inside the radius; Follow: in the slot
		double CourseYaw = 0.0;         // the heading it wants now (StepFar takes it)
		double SpeedScale = 1.0;        // how much of full speed it wants (StepFar takes it)
		int Tacks = 0, Gybes = 0;
		EManeuver Maneuver = EManeuver::None;
		// character (from the seed)
		double Overstand = 0.05, HeelPref = 0.22, React = 0.5, Timid = 0.0, HeaderDeg = 8.0, Wobble = 0.0;
		// inner state
		bool bInit = false;
		int Tack = 1;                   // the tack it is sailing or going to
		double FastX = 0.0, FastY = 0.0, SlowX = 0.0, SlowY = 0.0;   // the wind as it has felt it
		double ManeuverTime = 0.0, SinceManeuver = 100.0;
		double HeelSeen = 0.0, PushSeen = 0.0, PowerFull = 0.0, PaceShare = 1.0, Ease = 0.0, HeaderTime = 0.0, IronsTime = 0.0, DownTime = 0.0;
		double AvoidYaw = 0.0, AvoidHold = 0.0, AvoidClock = 0.0, AvoidSlow = 1.0;
		bool bAvoidTack = false;
		double TackPen = 0.0, TackHold = 0.0;
		int Mode = 0;                   // 0 reach, 1 beat, 2 run
		int StationSide = 1;
		double Clock = 0.0, SheetBias = 0.0;
		double WearYaw = 0.0;
		int WearPhase = 0;
	};

	void HelmInit(FHelm& H, double Skill, uint32_t Seed);
	/** A new order (clears bArrived / bOnStation). Changing only H.Order's numbers (a moving leader) needs no call. */
	void HelmOrder(FHelm& H, const FOrder& Order);
	/** The crew's controls for this tick (call every tick before Step; it reads B.Out of the last Step). */
	FControls HelmUpdate(FHelm& H, const FBoat& B, const FHelmInput& In, double Dt);
	/** The cheap version for a distant boat: sets H.CourseYaw and H.SpeedScale for StepFar (call a few times a second). */
	void HelmFar(FHelm& H, const FBoat& B, const FHelmInput& In, double Dt);

	// ---- the coach's launch ------------------------------------------------------------------------------------------

	struct FLaunch
	{
		double X = 0.0, Y = 0.0, Yaw = 0.0;
		double VX = 0.0, VY = 0.0, YawRate = 0.0;
		double Speed = 0.0;         // along the keel
		double Throttle = 0.0;      // where the lever really is, -1..1
		double Heel = 0.0, Pitch = 0.0;   // banks into the turn, bow up over the hump
		double Planing = 0.0, Wake = 0.0;
		double EngineLoad = 0.0;    // 0..1 for the motor's sound
	};
	constexpr double LaunchLength = 4.6;
	constexpr double LaunchTopSpeed = 10.5;
	/** Throttle -1..1 (astern is slow), Turn -1..1 (+ to starboard). */
	void StepLaunch(FLaunch& L, double Throttle, double Turn, const FEnv& E, double Dt);
	/** A coxswain: throttle and turn to go to a point at a cruising speed and stop within Radius of it. */
	void LaunchDriveTo(const FLaunch& L, double TX, double TY, double Cruise, double Radius, double& OutThrottle, double& OutTurn);
}
