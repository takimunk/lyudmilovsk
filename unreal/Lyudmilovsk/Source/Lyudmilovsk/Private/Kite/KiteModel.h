// A kite on a line, in plain C++ (no Unreal types) so it can be run and checked outside the engine
// (tools/kitesim/run.sh). AKite (Public/Kite/LyudKite.h) wraps it for the game: the hero's kite on the reel in
// his hands, and the kites the town's children fly on windy afternoons.
//
// The kite is a flat plate on a bridle: its plane stands square to the line, tipped back by the bridle angle, so
// the higher it climbs the flatter it meets the wind and the less it lifts; it settles where lift, drag, its
// weight and the line's pull balance (a real kite's "window"). The air is the game's real wind (the caller
// samples ULyudWindSubsystem::GetWindAt at the kite, gusts and the log profile with height included), so a calm
// lets it fall, a breeze holds it, a gust makes it surge and a gale makes it spin.
//
//   - Lift and drag of a flat plate: Cl = ClMax sin 2a, Cd = Cd0 + CdPlate sin^2 a (a = angle of attack).
//   - The line is a tether: never longer than the length let out; pulling it (reeling in) is what a flyer does in
//     a lull, letting it out takes the kite higher where the wind is stronger.
//   - Too much air (more than SpinSpeed for a while) and the kite starts to spin and loop down; it comes out of it
//     when the air eases, often only nearer the ground: reel it in. More pull than the line holds and it snaps.
//   - On the ground (below a few cm over its ground) it lies; in enough wind a tug lifts it again.
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

	enum class EShape : uint8_t { Diamond, Box };

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
	};
	FKiteDef Diamond();
	FKiteDef Box();

	enum class EState : uint8_t
	{
		Wound,      // on the reel in the hand (line under MinLine): nothing in the air
		Flying,
		Spinning,   // too much air: looping down
		Grounded,   // lies on the ground (the line slack)
		Snagged,    // caught in a tree or on a lamp post: lost
		Broken,     // the line snapped: lost
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
	};

	constexpr double MinLine = 1.2;     // under this the kite is wound up on the reel
	constexpr double LaunchLine = 3.0;  // let out to this from the hand at launch
	constexpr double Gravity = 9.81;
	constexpr double AirDensity = 1.225;

	/** Starts a kite from the hand: held up a little downwind of it, the line at LaunchLine. */
	void Launch(FKite& K, const FVec& Hand, const FVec& Wind);

	/** One step (any dt; it substeps at <= 1/120 s). */
	void Step(FKite& K, const FKiteInput& In, double Dt);

	/** Is it lost (snagged or snapped)? */
	inline bool IsLost(const FKite& K) { return K.State == EState::Snagged || K.State == EState::Broken; }

	/** Is it in the air? */
	inline bool IsUp(const FKite& K) { return K.State == EState::Flying || K.State == EState::Spinning; }

	/** Elevation of the line over the horizontal, degrees. */
	double Elevation(const FKite& K, const FVec& Hand);

	/** The kite's orientation for drawing: Forward = toward its nose (up the sail), Up = the sail's normal (facing the flyer). */
	void Frame(const FKite& K, const FVec& Hand, const FVec& Wind, FVec& OutForward, FVec& OutNormal);

	/**
	 * The line drawn as N+1 points from the hand to the kite: a taut line sags a little under its own weight
	 * (more for a long line and a weak pull), a slack one hangs in a deep curve. Out must hold N+1.
	 */
	void LinePoints(const FKite& K, const FVec& Hand, int N, FVec* Out);

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
}
