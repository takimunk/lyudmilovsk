// A kite on a line, in plain C++ (no Unreal types) so it can be run and checked outside the engine
// (tools/kitesim/run.sh). AKite (Public/Kite/LyudKite.h) wraps it for the game: the hero's kite on the reel in
// his hands, and the kites the town's children fly on windy afternoons.
//
// The kite is a flat plate on a bridle: its plane stands square to the line, tipped back by the bridle angle, so
// the higher it climbs the flatter it meets the wind and the less it lifts; it settles where lift, drag, its
// weight and the line's pull balance (a real kite's "window"). The air is the game's real wind (the caller
// samples ULyudWindSubsystem::GetLocalWind at the kite: gusts, height, the land's lee and lift included), so a calm
// lets it fall, a breeze holds it, a gust makes it surge and a gale makes it spin.
//
//   - Lift and drag of a flat plate: Cl = ClMax sin 2a, Cd = Cd0 + CdPlate sin^2 a (a = angle of attack).
//   - The line is a tether: never longer than the length let out; pulling it (reeling in) is what a flyer does in
//     a lull, letting it out takes the kite higher where the wind is stronger.
//   - Too much air (more than SpinSpeed for a while) and the kite starts to spin and loop down; it comes out of it
//     when the air eases, often only nearer the ground: reel it in. More pull than the line holds and it snaps.
//   - On the ground (below a few cm over its ground) it lies; in enough wind a tug lifts it again.
//
// Five kinds. The diamond and the box kite are the plate above. The delta is a big light wing that flies from a
// breath of wind, stands high over the flyer and glides when the line goes slack. The dragon is a small face with
// a very long tail: draggy, steady, low. The stunt kite has two lines and no will of its own: it flies along its
// nose, FKiteInput::Steer turns the nose, and where it is in the wind's window decides how fast it goes and how
// hard it pulls (low in the middle: fast and heavy; at the edges and overhead: slow, light, and at last a stall).
// Loops, dives, ground passes and figure eights come out of that; FTricks names them.
//
// The air may rise or sink (FKiteInput::Wind.Z: ridge lift over the cliff's edge, sink behind it) and tumble
// (Turbulence, rms m/s: a seeded shake by the kite's own clock). The hand may move: a flyer who runs upwind tows
// the kite through the air, which is how one is launched in a light wind.
//
// Frame: metres, Z up (Unreal world / 100). Everything is deterministic for the same inputs.
#pragma once

#include <cstdint>

namespace LyudKite
{
	struct FVec
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
		FVec() = default;
		FVec(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
		FVec operator+(const FVec& B) const { return FVec(X + B.X, Y + B.Y, Z + B.Z); }
		FVec operator-(const FVec& B) const { return FVec(X - B.X, Y - B.Y, Z - B.Z); }
		FVec operator*(double S) const { return FVec(X * S, Y * S, Z * S); }
		FVec& operator+=(const FVec& B) { X += B.X; Y += B.Y; Z += B.Z; return *this; }
		FVec& operator-=(const FVec& B) { X -= B.X; Y -= B.Y; Z -= B.Z; return *this; }
	};
	double Dot(const FVec& A, const FVec& B);
	FVec Cross(const FVec& A, const FVec& B);
	double Length(const FVec& A);
	FVec Normal(const FVec& A, const FVec& Fallback = FVec(0, 0, 1));

	enum class EShape : uint8_t { Diamond, Box, Delta, Stunt, Dragon };
	constexpr int NumShapes = 5;

	/** One kind of kite. */
	struct FKiteDef
	{
		EShape Shape = EShape::Diamond;
		double Area = 0.45;         // m^2 of sail
		double Mass = 0.10;         // kg with its tail
		double ClMax = 1.15;        // flat plate lift peak (at 45 degrees)
		double Cd0 = 0.12;          // drag of the frame, tail and edge
		double CdPlate = 1.5;       // the plate's own drag at 90 degrees
		double BridleDeg = 24.0;    // the plane tipped back from square-to-the-line
		double SpinSpeed = 13.0;    // m/s of air it can't take for long
		double SpinAfter = 1.6;     // s over SpinSpeed before it starts to spin
		double CalmSpeed = 9.0;     // m/s under which a spin dies out
		double BreakN = 160.0;      // the line snaps over this pull, N
		double LineMax = 60.0;      // m on the reel
		double Damp = 0.6;          // tail and flutter: a drag on the kite's own motion, 1/s
		int Lines = 1;              // 2: steered by its two lines (FKiteInput::Steer), the line's length set once
		double Wobble = 1.0;        // how much it sways of itself (a long tail stills it)
		double Glide = 0.0;         // 0..1: on a slack line it takes a glider's trim instead of tumbling (the delta)
		double TurnRadius = 3.0;    // m: the tightest turn of a two-line kite (full Steer)
		double TurnMax = 6.5;       // rad/s: and the fastest it comes round
		double Keel = 1.6;          // a two-line wing's grip on the air sideways (it goes where its nose points)
		double TailLength = 0.0;    // m of tail, for drawing (the dragon's)
		double FlyFrom = 3.0;       // m/s at 10 m it wants at least (what tools/kitesim measures; for hints and the town)
		double FlyLine = 25.0;      // m of line it is usually flown on
		double SparSpeed = 1e9;     // m/s: a nose-first crash faster than this pops a spar out (wind it in to set it back)
	};
	FKiteDef Diamond();
	FKiteDef Box();
	/** A light-wind wing: flies from about 2 m/s, stands high, glides in a lull; a fresh breeze is too much for it. */
	FKiteDef Delta();
	/** A two-line sport kite: fast, pulls hard, goes where it is steered; wants 4 m/s. */
	FKiteDef Stunt();
	/** A small round face and twelve metres of tail: a show kite, steady and low. */
	FKiteDef Dragon();
	FKiteDef ByShape(EShape Shape);
	/** "diamond", "box", "delta", "stunt", "dragon". */
	const char* ShapeName(EShape Shape);

	enum class EState : uint8_t
	{
		Wound,      // on the reel in the hand (line under MinLine): nothing in the air
		Flying,
		Spinning,   // too much air: looping down
		Grounded,   // lies on the ground (the line slack)
		Snagged,    // caught in a tree or on a lamp post: lost
		Broken,     // the line snapped: lost
		Held,       // a helper holds it up downwind on a long line (LongLaunch): let go on the flyer's pull
		Loose,      // came free of what caught it (Free): flutters down with no line, then lies (Grounded)
	};
	const char* StateName(EState S);

	/** What the flyer and the world do this step. */
	struct FKiteInput
	{
		FVec Hand;                  // where the line leaves the reel
		FVec Wind;                  // the air at the kite, m/s (from the game's wind with its gusts and height)
		double GroundZ = 0.0;       // the ground under the kite (sand, the cliff top), m
		double Reel = 0.0;          // m/s of line: > 0 let out, < 0 reel in (a pull), 0 hold
		bool bHitObstacle = false;  // the kite touched a tree, a pole, a wall this step (the caller's sweep)
		double Steer = 0.0;         // two-line kites: -1 the left line pulled .. 1 the right (the nose turns that way)
		double Pull = 0.0;          // -1..1: the arms drawn back (both lines, or a tug on the one) take in up to ArmReach of
		                            // line; thrown forward (< 0) they give as much slack (how a stunt kite is set down)
		double Turbulence = 0.0;    // rms m/s of tumbling air here (a lee, rough land): shakes the kite
		bool bRelease = false;      // a held kite is let go now whatever the line does
	};

	struct FKite
	{
		FKiteDef Def;
		EState State = EState::Wound;
		FVec Pos, Vel;
		double Line = 0.0;          // m let out
		double Tension = 0.0;       // N on the line now
		double Over = 0.0;          // s of air over SpinSpeed
		double SpinPhase = 0.0;     // rad: where in its loop a spinning kite is
		double Roll = 0.0;          // rad: the kite's tilt about the line (wobble, spin), for drawing
		double Aoa = 0.0;           // rad: angle of attack now
		double Lift = 0.0;          // N of lift now (for the hum and the checks)
		double Air = 0.0;           // m/s of apparent air now
		double Clock = 0.0;         // s since launch
		uint32_t Seed = 1;
		// two-line kites
		FVec Nose = FVec(0, 0, 1);  // where the nose points (kept square to the line)
		double Twist = 0.0;         // turns the lines are wound round each other: + from right-hand loops, - left
		double Bank = 0.0;          // -1..1: how hard it is turning now, for drawing
		bool bNoseUp = true;        // on the ground: standing on its tail edge (ready to go) or flat on its nose
		bool bParked = false;       // on the ground nose-up, leaning back: it stays until the lines are pulled
		bool bSpar = false;         // a spar popped out in a hard crash: it won't fly until wound in
		double Flip = 0.0;          // s one line has been pulled to roll a crashed kite over
		FVec CrashLine;             // the line's direction when it crashed (walking round it turns it over too)
		double Impact = 0.0;        // m/s it last met the ground at
		// the flyer's hands
		double Arm = 0.0;           // m of line drawn in by the arms now (FKiteInput::Pull; < 0 given)
		FVec LastHand;              // the hand a step ago: a moving hand tows the kite
		bool bHand = false;         // LastHand is set
		FVec HandVel;               // m/s, eased
		FVec HeldAt;                // where the helper holds it (Held)
		double Draw = 0.0;          // m/s the line is being drawn toward the flyer now (reel, arms, steps back)
	};

	constexpr double MinLine = 1.2;     // under this the kite is wound up on the reel
	constexpr double LaunchLine = 3.0;  // let out to this from the hand at launch
	constexpr double ArmReach = 0.7;    // m a pull of the arms takes in
	constexpr double Gravity = 9.81;
	constexpr double AirDensity = 1.225;

	/** Starts a kite from the hand: held up a little downwind of it, the line at LaunchLine. */
	void Launch(FKite& K, const FVec& Hand, const FVec& Wind);

	/**
	 * The long launch: a helper stands HeldAt (15-25 m downwind is best) holding the kite up nose to the sky, the
	 * line run out to them. They let go when the flyer pulls on a taut line (reel in, FKiteInput::Pull, or steps
	 * back), or at bRelease. The way to get a stunt kite or a dragon up, and it works in less wind than a toss: the
	 * whole line is out at once, so the first swing takes the kite up into the stronger air.
	 */
	void LongLaunch(FKite& K, const FVec& Hand, const FVec& HeldAt, const FVec& Wind);

	/** The same without a helper: the kite stood on the sand At, nose up, leaning back; the flyer backs off (the line
	 *  follows) and pulls. A two-line kite's own way; a single-line kite laid like this needs a good breeze. */
	void LayDown(FKite& K, const FVec& Hand, const FVec& At, const FVec& Wind);

	/** One step (any dt; it substeps at <= 1/120 s). */
	void Step(FKite& K, const FKiteInput& In, double Dt);

	/** Is it lost (snagged or snapped)? */
	inline bool IsLost(const FKite& K) { return K.State == EState::Snagged || K.State == EState::Broken; }

	/** Is it in the air? */
	inline bool IsUp(const FKite& K) { return K.State == EState::Flying || K.State == EState::Spinning; }

	/** A caught kite comes free (see FSnag): it flutters down where it is and lies there, its line slack. */
	void Free(FKite& K);

	/** Elevation of the line over the horizontal, degrees. */
	double Elevation(const FKite& K, const FVec& Hand);

	/** The kite's orientation for drawing: Forward = toward its nose (up the sail), Up = the sail's normal (facing the flyer). */
	void Frame(const FKite& K, const FVec& Hand, const FVec& Wind, FVec& OutForward, FVec& OutNormal);

	/**
	 * The line drawn as N+1 points from the hand to the kite: a taut line sags a little under its own weight
	 * (more for a long line and a weak pull), a slack one hangs in a deep curve. Out must hold N+1.
	 */
	void LinePoints(const FKite& K, const FVec& Hand, int N, FVec* Out);

	/** The same between any two points (a stunt kite's two lines run from two hands to two wing tips). */
	void LineBetween(const FKite& K, const FVec& From, const FVec& To, int N, FVec* Out);

	/** Where the kite is in the wind's window as its flyer sees it: Azimuth (degrees off straight downwind, + to the
	 *  flyer's right in the game's frame) and Elevation (degrees). */
	void Window(const FKite& K, const FVec& Hand, const FVec& Wind, double& OutAzimuth, double& OutElevation);

	/** A two-line kite's heading, degrees: 0 nose to the sky, 90 flying to the right, 180 diving, -90 to the left. */
	double Heading(const FKite& K, const FVec& Hand);

	/** How much of full steering the lines still give: 1 clean, less with every twist over three. */
	double SteerGain(const FKite& K);

	/** 0..1: how loud the line hums (the air through a taut line). */
	double Hum(const FKite& K);

	/**
	 * How a practised flyer works the reel (the town's children; the tests): let out toward Want metres while it
	 * flies steady, pull in a lull when it sags, reel in hard while it spins (lower down the air is weaker), wind it
	 * up when it lies on the ground on a slack line, and a tug to lift it again. Returns FKiteInput::Reel, m/s.
	 */
	double AutoReel(const FKite& K, const FVec& Hand, double Want);

	constexpr double ReelOut = 3.0;     // m/s the line runs out at most
	constexpr double ReelIn = 2.2;      // m/s a hand winds it in

	/**
	 * How somebody who knows a stunt kite flies it: lazy figure eights across the window, turning up at each end; now
	 * and then a loop when there is height for one; out of a dive in good time; a crashed kite rolled back onto its
	 * tail. Returns FKiteInput::Steer. No memory of its own (the kite's clock and seed pace it), so it is the same
	 * every time. In steady air it never touches the ground; a lull or a hard gust low down can still put it there.
	 */
	double AutoSteer(const FKite& K, const FVec& Hand, const FVec& Wind, double GroundZ, bool bLoops = true);

	// ---------------------------------------------------------------- tricks

	enum class ETrick : uint8_t
	{
		None,
		High,       // any kite: held steady high up for a while
		Save,       // any kite: a spin or a stall near the ground flown out of
		LoopLeft,   // stunt: the nose all the way round
		LoopRight,
		Eight,      // stunt: two opposite loops in a row
		LowPass,    // stunt: across the window under two metres without touching
		Landing,    // stunt: set down softly at the window's edge, nose up
		DiveStop,   // stunt: a dive from height pulled out close to the ground
		Count
	};
	/** "high", "save", "loop_left", "loop_right", "eight", "low_pass", "landing", "dive_stop". */
	const char* TrickName(ETrick T);

	/** Watches a flight and names what the flyer does. One per flyer; Reset at a launch of a new kite. */
	struct FTricks
	{
		ETrick Last = ETrick::None;     // the last trick named
		double LastScore = 0.0;         // its points
		double LastAt = -1.0;           // the tracker's clock when it was named, s
		int Count = 0;                  // tricks so far
		double Score = 0.0;             // points so far
		int By[int(ETrick::Count)] = {};

		// what it is in the middle of
		double Clock = 0.0;
		double HighFor = 0.0;
		bool bHighDone = false;
		bool bDanger = false, bWasSpin = false;
		double DangerAt = 0.0, StallFor = 0.0;
		double TwistFrom = 0.0, TwistFar = 0.0, StraightFor = 0.0;  // where the nose last turned back, and how far it has come round since
		int TurnDir = 1;
		int LastLoop = 0;               // -1 left, 1 right
		double LastLoopAt = -1e9;
		double LowRun = 0.0;            // m flown low so far
		bool bLowDone = false;
		double DiveTop = 0.0;           // m over the ground the dive began at
		bool bDiving = false;
		double DiveLow = 1e9;
		bool bWasFlying = false;
		double SinceUp = 0.0;           // s in the air since the last time it lay on the ground
		FVec LastPos;
	};
	/** After each Step: returns the trick that was finished in it (or None). */
	ETrick Track(FTricks& T, const FKite& K, const FKiteInput& In, double Dt);

	// ---------------------------------------------------------------- a caught kite got down

	/**
	 * A kite caught in a tree or on a post, its line hanging to the ground. Tugs on the line wear its hold down when
	 * they come from the right side: from downwind of the branch, so the pull and the wind lift it off together.
	 * A gust at that moment does most of the work. A tug from upwind drags it deeper into the twigs.
	 */
	struct FSnag
	{
		FVec At;                    // where it hangs
		double Hold = 1.0;          // what keeps it there: 0 = free; a deep one is 1.5, the worst 2.5
		int Tugs = 0;
		int Wrong = 0;              // tugs that made it worse
		bool bFree = false;
	};
	/** How it is caught: Hold 0.7 .. 1.5 by the seed. */
	FSnag MakeSnag(const FVec& At, uint32_t Seed);
	/** -1..1: how well a pull from where the puller stands goes with the wind (1 dead downwind of it, -1 upwind;
	 *  in still air every side is as good: 0.5). */
	double SnagSide(const FSnag& S, const FVec& PullerAt, const FVec& Wind);
	/** One tug (Strength 0..1; Gust = the wind's gust factor now, 1 = mean). True when this one freed it. */
	bool Tug(FSnag& S, const FVec& PullerAt, const FVec& Wind, double Gust, double Strength = 1.0);
}
