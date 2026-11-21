#pragma once

#include "CoreMinimal.h"

/**
 * Smooth motion of everything that walks, runs and flies (people, animals, birds): the bodies follow their
 * simulations through critically damped springs instead of being set onto them, so a corner is rounded, a start
 * and a stop take a moment, a kerb is a short ramp and a turn has a beginning and an end. docs/people.md "Motion".
 * Console: lyud.Anim.Smooth 0 switches all of it off (the bodies sit exactly on their simulations, as they did).
 */
namespace LyudMotion
{
	/** lyud.Anim.Smooth (any thread). */
	LYUDMILOVSK_API bool Smooth();

	/** One step of a critically damped spring from Cur toward Target (Game Programming Gems 4, 1.10): never overshoots,
	 *  lags a target moving at a steady speed by speed x SmoothTime. Vel is the spring's own state. */
	template <typename T>
	T Damp(const T& Cur, const T& Target, T& Vel, float SmoothTime, float Dt)
	{
		if (SmoothTime <= 1e-4f || Dt <= 0.f)
		{
			return Dt <= 0.f ? Cur : Target;
		}
		const float Omega = 2.f / SmoothTime;
		const float X = Omega * Dt;
		const float Exp = 1.f / (1.f + X + 0.48f * X * X + 0.235f * X * X * X);
		const T Change = Cur - Target;
		const T Temp = (Vel + Change * Omega) * Dt;
		Vel = (Vel - Temp * Omega) * Exp;
		return Target + (Change + Temp) * Exp;
	}

	/** The same for an angle in degrees (the short way round); the result is not wrapped. */
	inline float DampAngle(float Cur, float Target, float& Vel, float SmoothTime, float Dt)
	{
		return Damp(Cur, Cur + FMath::FindDeltaAngleDegrees(Cur, Target), Vel, SmoothTime, Dt);
	}
}
