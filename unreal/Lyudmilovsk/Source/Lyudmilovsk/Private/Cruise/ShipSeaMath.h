// The sea round a ship that stands still, in plain C++ (no Unreal types; tools/sea/run.sh tests it): the tide held at
// the datum while the hero is aboard, the ride of the ship seen from town (heave, roll and pitch from a few points of
// the surface) and the camera's sway aboard. See docs/ship.md "The sea at the hull".
#pragma once

#include <algorithm>
#include <cmath>

namespace LyudShipSea
{
	/** The tide hold 0..1 after Dt seconds: goes to 1 (held) or 0 at a constant rate over BlendSeconds; <= 0 = at once. */
	inline double StepHold(double Hold, bool bWant, double Dt, double BlendSeconds)
	{
		const double To = bWant ? 1.0 : 0.0;
		if (BlendSeconds <= 0.0)
		{
			return To;
		}
		const double Step = std::max(Dt, 0.0) / BlendSeconds;
		return Hold < To ? std::min(Hold + Step, To) : std::max(Hold - Step, To);
	}

	/** A level under the datum (cm: the tide, its low water, the wet line) as the hold shows it: at the datum when held. */
	inline double Held(double Cm, double Hold)
	{
		return Cm * (1.0 - std::min(std::max(Hold, 0.0), 1.0));
	}

	/** How a moored hull rides the sea, from the surface at four points. */
	struct FRide
	{
		double HeaveCm = 0.0, PitchDeg = 0.0, RollDeg = 0.0;   // pitch + = bow up, roll + = starboard down
	};

	/**
	 * The ride a 60 m ship takes from the water's height (cm over the still level) at her bow, stern, port and starboard
	 * sides (LengthCm and BeamCm apart): a big hull follows only a share of the waves, and never much (she is a kit
	 * mesh with no keel line to show): heave within 20 cm, pitch within 0.8 deg, roll within 1.5 deg.
	 */
	inline FRide RideFrom(double BowCm, double SternCm, double PortCm, double StbdCm, double LengthCm, double BeamCm)
	{
		const double Deg = 57.29577951308232;
		FRide R;
		R.HeaveCm = std::min(std::max(0.3 * 0.25 * (BowCm + SternCm + PortCm + StbdCm), -20.0), 20.0);
		R.PitchDeg = std::min(std::max(0.3 * std::atan2(BowCm - SternCm, std::max(LengthCm, 1.0)) * Deg, -0.8), 0.8);
		R.RollDeg = std::min(std::max(0.12 * std::atan2(PortCm - StbdCm, std::max(BeamCm, 1.0)) * Deg, -1.5), 1.5);
		return R;
	}

	/** Eases a ride toward a target (first-order, time constant TauSeconds). */
	inline FRide EaseRide(const FRide& Now, const FRide& To, double Dt, double TauSeconds)
	{
		const double K = TauSeconds > 0.0 ? 1.0 - std::exp(-std::max(Dt, 0.0) / TauSeconds) : 1.0;
		FRide R;
		R.HeaveCm = Now.HeaveCm + (To.HeaveCm - Now.HeaveCm) * K;
		R.PitchDeg = Now.PitchDeg + (To.PitchDeg - Now.PitchDeg) * K;
		R.RollDeg = Now.RollDeg + (To.RollDeg - Now.RollDeg) * K;
		return R;
	}

	/** The most the camera rolls aboard, degrees, at lyud.Cruise.Sway 1 in a sea of SwayFullHsM or more. */
	constexpr double SwayMaxRollDeg = 0.6;
	constexpr double SwayFullHsM = 1.2;

	/**
	 * The camera's sway aboard at time T (s): two slow sines each way (periods 7.3 and 11.9 s for the roll, 5.9 and
	 * 9.7 s for the pitch, never in step), scaled by the sea (HsM) and by Scale 0..1. |roll| <= 0.6 x Scale deg,
	 * |pitch| <= 0.24 x Scale deg; a flat calm or Scale 0 gives exactly none.
	 */
	inline void Sway(double T, double HsM, double Scale, double& OutRollDeg, double& OutPitchDeg)
	{
		const double TwoPi = 6.283185307179586;
		const double A = SwayMaxRollDeg * std::min(std::max(Scale, 0.0), 1.0) * std::min(std::max(HsM / SwayFullHsM, 0.0), 1.0);
		OutRollDeg = A * (0.6 * std::sin(TwoPi * T / 7.3) + 0.4 * std::sin(TwoPi * T / 11.9 + 1.3));
		OutPitchDeg = 0.4 * A * (0.6 * std::sin(TwoPi * T / 5.9 + 0.7) + 0.4 * std::sin(TwoPi * T / 9.7 + 2.1));
	}
}
