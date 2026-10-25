// The sea's waves, in plain C++ (no Unreal types; tools/sea/run.sh tests it outside the engine).
// ALyudSea builds them from the wind and hands the same trains to its material through a small
// texture, so the surface a floating thing feels (HeightAt, SurfaceAt) is the surface on screen.
//
// The sea is a sum of Gerstner wave trains whose wavelengths and directions are FIXED: directions
// cover the whole circle (a golden-angle lattice over log wavelength), and the wind only sets how
// tall each one is (a fetch-limited JONSWAP spectrum spread around the wind). A change of wind
// therefore never shifts a wave's phase: the sea just grows, calms or turns by trains rising
// and fading, with no seam or jump however far from the origin one looks.
//
// Two sets: geometry trains (about 1-100 m) move the surface's vertices and tilt its normals;
// detail trains (5 cm - 0.9 m) only tilt the normals, are scaled by the local gusts in the
// material (cat's paws, glassy calms) and fade into roughness where a pixel is wider than them.
//
// Time loops every LoopSeconds: every train's angular frequency is a whole number of turns per
// loop, so the material can be handed a wrapped clock without losing float precision.
//
// Shallow water: trains longer than about twice the depth shrink, and the whole surface is held
// under the breaking height (0.4 x depth), so waves die on the beach instead of passing through it.
//
// Frame: Unreal world metres, Z up; time in seconds.
#pragma once

#include <cstdint>
#include <vector>

#include "WindModel.h"

namespace LyudWeather
{
	struct FWaveParams
	{
		int GeoCount = 64;
		double GeoLambdaMin = 0.9, GeoLambdaMax = 100.0;
		int DetailCount = 48;
		double DetailLambdaMin = 0.05, DetailLambdaMax = 0.9;
		/** Open water upwind of the coast, km (the Baltic: tens of kilometres, not an ocean). */
		double FetchKm = 60.0;
		/** Directional spread: cos^(2 s) of half the angle off the wind. */
		double Spread = 3.0;
		/** How sharp the crests are (Gerstner horizontal displacement), kept under folding. */
		double Choppiness = 0.8;
		double LoopSeconds = 1200.0;
		uint32_t Seed = 7;
	};

	struct FWaveTrain
	{
		float DirX = 1.f, DirY = 0.f;
		float K = 1.f;        // wavenumber, rad/m
		float Amp = 0.f;      // amplitude, m
		float Omega = 1.f;    // rad/s, a whole number of turns per loop
		float Phase = 0.f;    // rad
		float Lambda = 1.f;   // m
	};

	struct FSeaSurface
	{
		float Height = 0.f;              // m over sea level
		float Normal[3] = { 0.f, 0.f, 1.f };
		float Velocity[3] = { 0.f, 0.f, 0.f };   // the water at the surface, m/s
	};

	class FWaveField
	{
	public:
		FWaveParams Params;
		/** Sorted by wavenumber (longest first), so a renderer can stop at the first one too short to show. */
		std::vector<FWaveTrain> Geo, Detail;
		/** Horizontal displacement factor (Gerstner Q), the same for every geometry train. */
		float Chop = 0.f;
		/** Significant wave height of the geometry trains, m, and the spectrum's peak wavelength, m. */
		float Hs = 0.f;
		float PeakLambda = 0.f;
		/** The sea-state wind the trains were built for. */
		float WindSpeed = 0.f, WindDirX = 1.f, WindDirY = 0.f;

		/** The trains' fixed wavelengths, directions, phases and frequencies (amplitudes zero). */
		void Init(const FWaveParams& P);
		/** Amplitudes for a sea-state wind (FWindState from SeaStateWind). */
		void SetWind(float Speed, float DirX, float DirY);

		/** How much of a train's height survives in DepthM of water, and the surface-wide cap under breaking. */
		static float DepthAtten(float K, float DepthM);
		float BreakCap(float DepthM) const;

		/** Where the water particle that rests at (X0, Y0) is at time T: offset from rest, m. */
		void Displace(double X0, double Y0, double T, float DepthM, float Out[3]) const;
		/** The surface above the world point (X, Y): height, normal and water velocity. */
		FSeaSurface SurfaceAt(double X, double Y, double T, float DepthM) const;
		float HeightAt(double X, double Y, double T, float DepthM) const { return SurfaceAt(X, Y, T, DepthM).Height; }
		/** Smallest Jacobian of the horizontal displacement found at (X0, Y0), for checking crests never fold. */
		float Jacobian(double X0, double Y0, double T) const;
		/** The loop's clock for a time in seconds. */
		double Wrap(double Seconds) const;
	};

	/** Angular frequency of a water wave of wavenumber K (gravity + surface tension), rad/s. */
	double DispersionOmega(double K);
}
