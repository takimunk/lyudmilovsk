#pragma once

#include "CoreMinimal.h"

/** Constants and small helpers shared by the StarlingFlock*.cpp files. */
namespace StarlingFlockMath
{
	constexpr float MASS = 0.08f; // kg (European starling, ~75-90 g)
	constexpr float GRAV = 9.81f;
	constexpr float MIN_CLEAR = 0.3f; // m: hard floor over water and land
	constexpr float BAND_TAU = 6.f;
	constexpr float BAND_RATE = 2.5f;
	constexpr float MAX_TURN = PI / 3.f; // leaders steer at most this far off the flock's heading
	constexpr float HOP_T = 0.2f; // s

	FORCEINLINE float Hyp2(float A, float B) { return FMath::Sqrt(A * A + B * B); }
	FORCEINLINE float Hyp3(float A, float B, float C) { return FMath::Sqrt(A * A + B * B + C * C); }
	FORCEINLINE float Smooth(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}
	FORCEINLINE float Wrap24(float H) { return FMath::Fmod(FMath::Fmod(H, 24.f) + 24.f, 24.f); }
	/** Signed difference a - b on the 24 h circle, in (-12, 12]. */
	FORCEINLINE float Dh24(float A, float B)
	{
		float D = Wrap24(A - B);
		return D > 12.f ? D - 24.f : D;
	}
	/** A random number in [0, 1) from a bird and a step, for the parallel social solve. */
	FORCEINLINE float HashRnd(uint32 A, uint32 B)
	{
		uint32 H = A * 0x9E3779B1u ^ (B + 0x7F4A7C15u) * 0x85EBCA77u;
		H ^= H >> 15; H *= 0x2C1B3C6Du; H ^= H >> 12; H *= 0x297A2D39u; H ^= H >> 15;
		return (H >> 8) * (1.f / 16777216.f);
	}
	/** Grow or shrink a patch radius toward holding `Want` birds. */
	FORCEINLINE float AdaptRadius(float R, int32 Have, float Want)
	{
		if (Want < 1.f) return 0.f;
		return FMath::Clamp(R * (Have < Want ? 1.04f : 0.97f), 1.5f, 250.f);
	}
}
