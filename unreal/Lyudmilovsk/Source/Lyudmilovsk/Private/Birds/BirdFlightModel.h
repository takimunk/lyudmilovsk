// The player bird's flight, in plain C++ (no Unreal types) so it is built and tested outside the
// engine too (unreal/scripts/test_bird_flight.sh). docs/bird_flight.md has the idea; in short: a point
// mass with an airspeed vector that chases the gaze, a glide polar for drag, lift limited by speed,
// and wingbeats as bursts of thrust and lift. Metres, seconds, radians. Axes as Unreal's: X forward at
// yaw 0, Y right (positive yaw turns right), Z up.
#pragma once

#include <cmath>
#include <cstdint>

namespace BirdFlight
{
	struct V3
	{
		double X = 0, Y = 0, Z = 0;
		V3() = default;
		V3(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
		V3 operator+(const V3& B) const { return { X + B.X, Y + B.Y, Z + B.Z }; }
		V3 operator-(const V3& B) const { return { X - B.X, Y - B.Y, Z - B.Z }; }
		V3 operator*(double S) const { return { X * S, Y * S, Z * S }; }
		V3& operator+=(const V3& B) { X += B.X; Y += B.Y; Z += B.Z; return *this; }
		double Dot(const V3& B) const { return X * B.X + Y * B.Y + Z * B.Z; }
		double Size() const { return std::sqrt(X * X + Y * Y + Z * Z); }
		double Size2D() const { return std::sqrt(X * X + Y * Y); }
	};

	/** The species table's flight styles (Content/Birds/species.json, FlightStyle). */
	enum class EStyle : uint8_t { Flapping, Bounding, Undulating, Clapping, Soaring, Flicker };

	struct FParams
	{
		EStyle Style = EStyle::Undulating;
		double Mass = 0.075;          // kg
		double Span = 0.38;           // m
		double BeatHz = 12.0;         // the species' wingbeat rate
		double Cruise = 15.0;         // m/s, steady flapping flight
		double BestGlide = 12.0;      // m/s, the speed of the flattest glide
		double Stall = 6.0;           // m/s; below it the wings can't hold the bird up
		double MaxSpeed = 30.0;       // m/s, a steep dive with the wings in
		double GlideRatio = 8.0;      // metres forward per metre down at BestGlide
		double TurnRate = 2.0;        // rad/s, the fastest the course follows the gaze
		double MaxLoad = 3.5;         // g, the hardest pull the wings can make
		/** One click (a burst of BeatsPerClick real wingbeats): speed added along the path and
		 *  lift added upward at standstill, m/s. */
		double ClickThrust = 1.0;
		double ClickLift = 1.0;
		int BeatsPerClick = 2;
		/** How long one click's burst takes, s (BeatsPerClick / BeatHz). */
		double ClickTime() const { return BeatsPerClick / BeatHz; }
		/** A one-wing click turns the course by this much, rad (rowing with one oar). */
		double OarKick = 0.35;
		/** Seconds for the body to catch up with the gaze (the head-to-body lag). */
		double Follow = 0.25;
		bool Hops = true;             // on the ground: hops (sparrow) or walks (pigeon)
		double Gravity = 9.81;
	};

	/** Flight parameters from the species table's numbers. */
	FParams MakeParams(EStyle Style, double MassG, double SpanCm, double BeatHz, double CruiseMs);

	struct FInput
	{
		double AimYaw = 0, AimPitch = 0;    // the gaze, world, rad
		bool ClickL = false, ClickR = false; // a wing's click started this step (both = a full beat)
		double TuckL = 0, TuckR = 0;         // 0..1, wing pulled in
		double Flare = 0;                    // 0..1, tail fanned, wings forward, feet down
		double Roll = 0;                     // -1..1 tail trim (A / D)
	};

	/** The air at the bird: wind (horizontal, m/s) and rising air (m/s up). */
	struct FAir
	{
		V3 Wind;
		double Updraft = 0;
	};

	struct FState
	{
		V3 Pos;                    // m
		V3 Vel;                    // airspeed vector, m/s
		V3 Ground;                 // velocity over the ground (Vel + wind + updraft), m/s, after Step
		double Bank = 0;           // rad, + right wing down
		double Load = 1;           // g the wings pull
		double SpreadL = 1, SpreadR = 1;     // 0 in .. 1 spread, smoothed
		/** Time left of the current click's burst per wing, s; 0 = not beating. */
		double BurstL = 0, BurstR = 0;
		/** A click that came during the second half of a burst waits for it. */
		bool QueuedL = false, QueuedR = false;
		/** Wing stroke phase 0..1 per wing (0..0.5 down, 0.5..1 up); for the animation. */
		double PhaseL = 0, PhaseR = 0;
		double YawKick = 0;        // rad of oar turn still to apply
	};

	void Step(const FParams& P, const FInput& In, const FAir& Air, FState& S, double Dt);

	/** Airspeed. */
	inline double Speed(const FState& S) { return S.Vel.Size(); }

	/** Sink rate of the glide polar at speed V with the wings spread, m/s (positive down). */
	double GlideSink(const FParams& P, double V);

	/**
	 * What holding Space does: the species' natural flapping pattern. Call every step with the time
	 * Space has been held; returns whether to click both wings now, and sets Tuck for the bounding
	 * finches' closed-wing phase. State is the generator's memory (start at 0).
	 */
	bool AutoFlap(const FParams& P, double Dt, double& Clock, double& Tuck);

	inline double Wrap(double A)
	{
		const double TwoPi = 6.283185307179586;
		A = std::fmod(A + 3.141592653589793, TwoPi);
		if (A < 0) A += TwoPi;
		return A - 3.141592653589793;
	}
}
