#include "SeaCurrent.h"

#include <algorithm>
#include <cmath>

namespace LyudSeaCurrent
{
namespace
{
	inline float Clamp(float V, float A, float B) { return V < A ? A : (V > B ? B : V); }
	inline float Smooth(float E0, float E1, float V)
	{
		const float T = Clamp((V - E0) / (E1 - E0), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	/** The tide's fastest rate at springs, m/s (a 4.5 m range over a 12.42 h cycle). */
	constexpr float PeakRate = 3.2e-4f;

	/** The log boundary layer: share of the flow at OverBed in a column Depth deep (1 on average over the column). */
	float BedLayer(const FPlace& P)
	{
		const float Z0 = 0.003f + 0.02f * Clamp(P.Rough, 0.f, 1.f);
		const float H = std::max(P.Depth, 0.2f);
		const float Z = Clamp(P.OverBed, Z0 * 1.5f, H);
		// the depth-mean of ln(z/z0) is ln(H/z0) - 1
		return Clamp(std::log(Z / Z0) / std::max(std::log(H / Z0) - 1.f, 0.5f), 0.f, 1.15f);
	}
}

FVec3 Mean(const FDrive& D, const FPlace& P)
{
	FVec3 U;
	if (P.Depth <= 0.05f)
	{
		return U;
	}
	const float R = Clamp(D.TideRate / PeakRate, -1.3f, 1.3f);
	// the tidal stream runs along the coast, north with the flood, south with the ebb; feeble in the last metres
	const float Along = 0.32f * R * Smooth(0.4f, 3.f, P.Depth);
	// over the flat the water fills and drains it across the shore: the rising tide flows landward (-X)
	const float Across = -Clamp(D.TideRate * 80.f * (1.f + 2.f * std::exp(-P.Depth / 2.f)), -0.12f, 0.12f) * (1.f - Smooth(6.f, 10.f, P.Depth));
	// the wind's longshore current, a jet over the bars a few metres deep
	const float Jet = 0.5f + 0.8f * std::exp(-((P.Depth - 3.f) / 3.f) * ((P.Depth - 3.f) / 3.f));
	const float WindAlong = 0.022f * D.WindY * Jet;
	// right under the surface the wind drags the water along with it
	const float UnderSurface = std::max(P.Depth - P.OverBed, 0.f);
	const float Drag = 0.015f * std::exp(-UnderSurface / 2.f);
	const float Layer = BedLayer(P);
	U.X = (Across + Drag * D.WindX) * Layer;
	U.Y = (Along + WindAlong + Drag * D.WindY) * Layer;
	return U;
}

float Turbulence(const FDrive& D, const FPlace& P)
{
	const FVec3 M = Mean(D, P);
	const float Speed = std::sqrt(M.X * M.X + M.Y * M.Y);
	// eddies: a share of the flow (more over stones and close to the bed) and a floor of stirring by the waves
	const float Share = 0.15f + 0.3f * Clamp(P.Rough, 0.f, 1.f) + 0.15f * (1.f - Smooth(0.3f, 2.f, P.OverBed));
	return Clamp((Share * Speed + 0.012f) / std::max(Speed + 0.012f, 1e-4f), 0.f, 1.f);
}

FVec3 At(const FDrive& D, const FPlace& P)
{
	FVec3 U = Mean(D, P);
	if (P.Depth <= 0.05f)
	{
		return U;
	}
	const float Speed = std::sqrt(U.X * U.X + U.Y * U.Y);
	const float Amp = (0.15f + 0.3f * Clamp(P.Rough, 0.f, 1.f)) * Speed + 0.012f;
	// a divergence-free field of a few drifting stream-function waves, metres across; its velocity is the curl
	struct FWave { float KX, KY, W, Ph, A; };
	static const FWave Waves[] = {
		{ 0.61f, 0.23f, 0.11f, 0.3f, 1.f }, { -0.27f, 0.74f, 0.07f, 1.9f, 0.8f }, { 0.98f, -0.51f, 0.16f, 4.1f, 0.5f },
		{ -1.31f, -0.95f, 0.21f, 2.7f, 0.35f }, { 0.12f, 1.62f, 0.13f, 5.3f, 0.3f }, { 2.11f, 0.77f, 0.29f, 0.9f, 0.2f },
	};
	float VX = 0.f, VY = 0.f, VZ = 0.f, Norm = 0.f;
	const double T = std::fmod(D.Time, 86400.0);
	for (const FWave& W : Waves)
	{
		const double Arg = W.KX * P.X + W.KY * P.Y - W.W * T + W.Ph;
		const float C = float(std::cos(Arg));
		// psi = A sin(arg) / |k|: u = dpsi/dy, v = -dpsi/dx (unit speed per wave)
		const float K = std::sqrt(W.KX * W.KX + W.KY * W.KY);
		VX += W.A * C * W.KY / K;
		VY += -W.A * C * W.KX / K;
		VZ += W.A * float(std::sin(Arg * 0.7 + W.Ph));
		Norm += W.A;
	}
	// eddies are damped right at the bed
	const float Layer = Clamp(BedLayer(P), 0.2f, 1.f);
	U.X += Amp * VX / Norm * Layer;
	U.Y += Amp * VY / Norm * Layer;
	U.Z = Amp * 0.35f * VZ / Norm * Smooth(0.05f, 0.6f, P.OverBed);
	return U;
}
}
