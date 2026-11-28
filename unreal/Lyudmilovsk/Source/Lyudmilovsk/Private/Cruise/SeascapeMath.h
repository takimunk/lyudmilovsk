// The cruise's seascape, in plain C++ (no Unreal types; tools/seascapesim/run.sh tests it outside the engine).
//
// The hero sails aboard a ship that stands still at the cruise stage; the sea round it is made to pass: the islands
// of the chart are placed in the world relative to the stage ship (ALyudCruiseSeascape), the waves' phases are carried
// aft (ALyudSea::SetSurfaceDrift), foam is shed by a hull that goes nowhere (ULyudShipWake). The sums of all three
// are here.
//
// Frames. The chart is the map's metres carried out to sea: X west, Y north, the same axes as the Unreal world
// (world cm = chart m * 100, + 790 in Y). A heading is atan2(dY, dX) of the bow, radians. The stage ship's bow
// points along StageYaw (radians, the same sense). A chart point P is shown at
//     Stage + Rot(StageYaw - Heading) * (P - Ship)
// so what is dead ahead on the chart is dead ahead of the stage ship, and when the ship turns the islands swing round
// it (the sun, the wind and the waves' own directions stay the world's: accepted, docs/ship.md).
#pragma once

#include <algorithm>
#include <cmath>

namespace LyudSeascape
{
	constexpr double Pi = 3.14159265358979323846;

	struct FVec2
	{
		double X = 0.0, Y = 0.0;
	};

	inline double WrapAngle(double A)
	{
		A = std::fmod(A + Pi, 2.0 * Pi);
		if (A < 0.0)
		{
			A += 2.0 * Pi;
		}
		return A - Pi;
	}

	/** The ship as the seascape needs her. */
	struct FShip
	{
		FVec2 Chart;                 // m
		double Heading = 0.0;        // rad, chart
		double StageYaw = 0.0;       // rad, the standing ship's bow in the world
	};

	/** A chart point in metres from the stage ship's pivot, in world axes. */
	inline FVec2 ChartToStage(const FShip& S, const FVec2& P)
	{
		const double A = S.StageYaw - S.Heading;
		const double C = std::cos(A), Sn = std::sin(A);
		const double Dx = P.X - S.Chart.X, Dy = P.Y - S.Chart.Y;
		return { Dx * C - Dy * Sn, Dx * Sn + Dy * C };
	}

	/** The world yaw (rad) of a thing whose yaw on the chart is ChartYaw. */
	inline double YawToStage(const FShip& S, double ChartYaw)
	{
		return WrapAngle(ChartYaw + S.StageYaw - S.Heading);
	}

	/** The velocity (m/s, world axes) the water must seem to have past the standing ship: straight aft. */
	inline FVec2 WaterVelocity(const FShip& S, double SpeedMS)
	{
		return { -SpeedMS * std::cos(S.StageYaw), -SpeedMS * std::sin(S.StageYaw) };
	}

	/**
	 * "Hull down": how many metres of a thing Dist metres away the sea's curve hides from an eye EyeM over the water.
	 * The world is flat, so the seascape sinks the thing by this much instead. Nothing is hidden nearer than the
	 * horizon sqrt(2 R EyeM); beyond it the hidden height grows as (Dist - horizon)^2 / 2R. R is the Earth's radius
	 * as TUNED for the game (smaller than the real 6.37e6 m x 7/6 for refraction, so that 30 m stacks rise out of
	 * the sea inside the 18 km the haze lets one see).
	 */
	inline double HullDownM(double DistM, double EyeM, double RadiusM)
	{
		const double Horizon = std::sqrt(2.0 * RadiusM * std::max(EyeM, 0.0));
		const double Over = DistM - Horizon;
		return Over > 0.0 ? Over * Over / (2.0 * RadiusM) : 0.0;
	}

	/** Is a thing worth drawing: within its own range and within what the air lets through (1.4 x the visibility)? */
	inline bool IsShown(double DistM, double RangeM, double VisibilityM, double TopM, double HiddenM)
	{
		return DistM < RangeM && DistM < 1.4 * VisibilityM && HiddenM < TopM;
	}

	/** The far level of a piece with hysteresis: beyond FarM + 10 % far, nearer than FarM - 10 % near, else as it was. */
	inline bool UseFar(double DistM, double FarM, bool bWasFar)
	{
		return bWasFar ? DistM > FarM * 0.9 : DistM > FarM * 1.1;
	}

	/** How bright a turning beam looks from Bearing (rad, from the lamp to the eye): 1 as it sweeps across, 0 away.
	 *  The lamp turns once in PeriodS seconds; WidthRad is the beam's half width as seen. */
	inline double BeamAt(double TimeS, double PeriodS, double Bearing, double WidthRad)
	{
		const double Beam = 2.0 * Pi * std::fmod(TimeS, PeriodS) / PeriodS;
		const double D = WrapAngle(Beam - Bearing) / WidthRad;
		return std::exp(-D * D);
	}
	inline double BeamAngle(double TimeS, double PeriodS)
	{
		return 2.0 * Pi * std::fmod(TimeS, PeriodS) / PeriodS;
	}

	/** What the hull sheds a second at a speed through the water, in a sea of significant height HsM. */
	struct FWakeRates
	{
		float Strength = 0.f;      // 0 at anchor .. 1 at full speed: scales sizes and the sounds
		float BowRings = 0.f;      // rings of foam a second from the stem, along the two arms of the V
		float SideStreaks = 0.f;   // streaks a second along the waterline, both sides together
		float SternPatches = 0.f;  // patches a second in the screw's wash
		float Spray = 0.f;         // droplets a second at the stem (a head sea only)
		float SternLife = 0.f;     // seconds a patch of the wash lives (the white road is Speed x this long)
	};

	constexpr float WakeMinSpeed = 0.3f;     // m/s: under this she is at anchor and sheds nothing
	constexpr float WakeFullSpeed = 3.5f;    // m/s: the cruise's full speed (about 7 knots)
	constexpr float WakeRoadM = 130.f;       // the wash is drawn this far astern

	inline FWakeRates WakeRates(float SpeedMS, float HsM, float HeadSea01)
	{
		FWakeRates R;
		if (SpeedMS < WakeMinSpeed)
		{
			return R;
		}
		const float K = std::min(SpeedMS / WakeFullSpeed, 1.3f);
		R.Strength = std::min(K, 1.f);
		R.BowRings = 9.f * K;
		R.SideStreaks = 2.f * SpeedMS / 2.4f;           // one each side every 2.4 m run
		R.SternLife = std::min(40.f, WakeRoadM / SpeedMS);
		R.SternPatches = std::min(SpeedMS / 0.8f, 6.f);  // one every 0.8 m run: at most 6 x 40 = 240 alive
		const float Sea = std::clamp((HsM - 0.8f) / 1.2f, 0.f, 1.f);
		R.Spray = 45.f * R.Strength * Sea * std::clamp(HeadSea01, 0.f, 1.f);
		return R;
	}

	/** The most particles the rates above can keep alive (the pools' size): rings 9 s, streaks 12 s, patches SternLife. */
	inline int WakeMaxFoam(float SpeedMS)
	{
		const FWakeRates R = WakeRates(SpeedMS, 0.f, 0.f);
		return int(R.BowRings * 9.f + R.SideStreaks * 12.f + R.SternPatches * R.SternLife) + 8;
	}

	/** The phase a wave train of wavenumber K (rad/m) along (DirX, DirY) gets when the whole pattern has been carried
	 *  (DriftX, DriftY) metres: the surface at x is the old surface at x - Drift. Wrapped to 0..2 pi in double. */
	inline double DriftPhase(double BasePhase, double K, double DirX, double DirY, double DriftX, double DriftY)
	{
		double P = std::fmod(BasePhase - K * (DirX * DriftX + DirY * DriftY), 2.0 * Pi);
		if (P < 0.0)
		{
			P += 2.0 * Pi;
		}
		return P;
	}

	/** Ship's bells: how many strokes at a clock time (seconds of the day), or 0 when it is not a half hour's start.
	 *  Four-hour watches from midnight: 00:30 one bell .. 04:00 eight bells, and round again. */
	inline int BellsAt(int SecondOfDay, int ToleranceS = 0)
	{
		const int Half = 1800;
		const int Into = ((SecondOfDay % Half) + Half) % Half;
		if (Into > ToleranceS)
		{
			return 0;
		}
		const int N = ((SecondOfDay / Half) % 8 + 8) % 8;
		return N == 0 ? 8 : N;
	}
}
