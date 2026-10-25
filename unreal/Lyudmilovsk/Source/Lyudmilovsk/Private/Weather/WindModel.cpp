#include "WindModel.h"

#include <algorithm>
#include <cmath>

namespace LyudWeather
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		/** 32-bit integer hash to 0..1; the HLSL mirror uses the same constants (tools/sea/sea_hlsl.py). */
		float Hash(uint32_t X, uint32_t Y, uint32_t Z, uint32_t Seed)
		{
			uint32_t H = X * 0x8da6b343u ^ Y * 0xd8163841u ^ Z * 0xcb1ab31fu ^ Seed * 0x165667b1u;
			H = (H ^ (H >> 15)) * 0x2c1b3c6du;
			H = (H ^ (H >> 12)) * 0x297a2d39u;
			H ^= H >> 15;
			return float(H >> 8) * (1.f / 16777215.f);
		}

		double Quintic(double T) { return T * T * T * (T * (T * 6.0 - 15.0) + 10.0); }
		double Clamp(double V, double A, double B) { return std::min(std::max(V, A), B); }
		double SmoothStep(double A, double B, double X)
		{
			const double T = Clamp((X - A) / (B - A), 0.0, 1.0);
			return T * T * (3.0 - 2.0 * T);
		}
		uint32_t Wrap(int64_t I, int Period) { return uint32_t(((I % Period) + Period) % Period); }

		/** Bearing (compass, degrees) -> world unit vector pointing that way (north +Y, east -X). */
		void BearingToWorld(double Deg, float& X, float& Y)
		{
			const double R = Deg * Pi / 180.0;
			X = float(-std::sin(R));
			Y = float(std::cos(R));
		}

		double WorldToBearing(double X, double Y)
		{
			double D = std::atan2(-X, Y) * 180.0 / Pi;
			return D < 0.0 ? D + 360.0 : D;
		}
	}

	float SmoothNoise1(double T, uint32_t Seed)
	{
		const double F = std::floor(T);
		const int64_t I = int64_t(F);
		const double U = Quintic(T - F);
		const float A = Hash(uint32_t(I), 0, 0, Seed), B = Hash(uint32_t(I + 1), 0, 0, Seed);
		return float((A + (B - A) * U) * 2.0 - 1.0);
	}

	float ValueNoise3(double X, double Y, double Z, uint32_t Seed, int Period)
	{
		const double FX = std::floor(X), FY = std::floor(Y), FZ = std::floor(Z);
		const int64_t IX = int64_t(FX), IY = int64_t(FY), IZ = int64_t(FZ);
		const double U = Quintic(X - FX), V = Quintic(Y - FY), W = Quintic(Z - FZ);
		float C[8];
		for (int K = 0; K < 8; K++)
		{
			C[K] = Hash(Wrap(IX + (K & 1), Period), Wrap(IY + ((K >> 1) & 1), Period), Wrap(IZ + (K >> 2), Period * 8), Seed);
		}
		auto L = [](double A, double B, double T) { return A + (B - A) * T; };
		const double X00 = L(C[0], C[1], U), X10 = L(C[2], C[3], U), X01 = L(C[4], C[5], U), X11 = L(C[6], C[7], U);
		return float(L(L(X00, X10, V), L(X01, X11, V), W) * 2.0 - 1.0);
	}

	double HoursSince2000(int Year, int Month, int Day, double HourOfDay)
	{
		// days from civil (Howard Hinnant), relative to 2000-03-01 then shifted to 2000-01-01
		const int Y = Year - (Month <= 2 ? 1 : 0);
		const int Era = (Y >= 0 ? Y : Y - 399) / 400;
		const int Yoe = Y - Era * 400;
		const int Doy = (153 * (Month + (Month > 2 ? -3 : 9)) + 2) / 5 + Day - 1;
		const int Doe = Yoe * 365 + Yoe / 4 - Yoe / 100 + Doy;
		const int64_t Days = int64_t(Era) * 146097 + Doe - 730425;   // 730425 = days(0000-03-01 .. 2000-01-01)
		return double(Days) * 24.0 + HourOfDay;
	}

	float DayFactor(double Hours, const FWindParams& P)
	{
		const double Days = Hours / 24.0;
		const double HourOfDay = Hours - std::floor(Days) * 24.0;
		const double DayOfYear = std::fmod(Days, 365.2425) + 1.0;
		const double Decl = -23.44 * std::cos(2.0 * Pi * (DayOfYear + 10.0) / 365.25) * Pi / 180.0;
		const double Lat = P.LatitudeDeg * Pi / 180.0;
		const double CosH = Clamp(-std::tan(Lat) * std::tan(Decl), -1.0, 1.0);
		const double HalfDay = std::acos(CosH) / Pi * 12.0;
		const double Noon = 12.0 + P.UtcOffsetHours - P.LongitudeDeg / 15.0;
		const double Rise = Noon - HalfDay, Set = Noon + HalfDay;
		// the air is stirred from an hour after sunrise until a little before sunset, most a couple of hours after noon
		const double X = (HourOfDay - Rise - 1.0) / std::max(Set - Rise - 1.5, 0.5);
		if (X <= 0.0 || X >= 1.0)
		{
			return 0.f;
		}
		const double Shape = std::sin(Pi * std::pow(X, 1.25));
		// a low winter sun stirs the air less than a high summer one
		const double NoonElevation = 90.0 - P.LatitudeDeg + Decl * 180.0 / Pi;
		return float(Shape * Clamp(NoonElevation / 45.0, 0.4, 1.0));
	}

	FWindState Regime(double Hours, const FWindParams& P)
	{
		FWindState S;
		S.Hours = Hours;
		const uint32_t Seed = P.Seed;
		const double DirN = 0.7 * SmoothNoise1(Hours / 30.0, Seed) + 0.3 * SmoothNoise1(Hours / 9.0, Seed + 1);
		const double From = P.PrevailingFromDeg + P.DirectionSwingDeg * DirN * 1.3;
		// two scales of weather (fronts a day apart, squally spells of hours), pushed out toward the extremes
		// so that calm days and gales both come up instead of everything hovering near the median
		const double SpeedN = 0.75 * SmoothNoise1(Hours / 22.0, Seed + 2) + 0.45 * SmoothNoise1(Hours / 6.0, Seed + 3);
		const double Spread = (SpeedN < 0.0 ? -1.0 : 1.0) * std::pow(std::fabs(SpeedN), 0.7) * 1.6;
		const double Weather = Clamp(P.MedianSpeed * std::exp(P.SpeedLogSigma * Spread), P.MinSpeed, P.MaxSpeed);
		S.Weather = float(Weather);
		S.Day = DayFactor(Hours, P);

		float WX, WY;
		BearingToWorld(From + 180.0, WX, WY);
		// at night the air settles; on a quiet night it goes almost still (the ground cools and the wind lifts off it)
		const double Night = 0.3 + (P.NightFactor - 0.3) * SmoothStep(1.5, 5.5, Weather);
		const double Mean = Weather * (Night + (1.0 - Night) * S.Day);
		double VX = WX * Mean, VY = WY * Mean;
		// the sea breeze: only felt when the weather itself is quiet
		const double Breeze = P.SeaBreeze * S.Day * (1.0 - SmoothStep(2.5, 7.0, Weather));
		float BX, BY;
		BearingToWorld(P.SeaBearingDeg + 180.0, BX, BY);
		VX += BX * Breeze;
		VY += BY * Breeze;

		const double Speed = std::sqrt(VX * VX + VY * VY);
		S.Speed = float(Speed);
		if (Speed > 1e-4)
		{
			S.DirX = float(VX / Speed);
			S.DirY = float(VY / Speed);
		}
		else
		{
			S.DirX = WX;
			S.DirY = WY;
		}
		S.FromDeg = float(std::fmod(WorldToBearing(-S.DirX, -S.DirY), 360.0));
		S.Gustiness = float(P.GustinessNight + (P.GustinessDay - P.GustinessNight) * S.Day);
		return S;
	}

	FWindState SeaStateWind(double Hours, const FWindParams& P, double OverHours)
	{
		// the waves are what the wind has been doing lately: a weighted mean of its vector, the latest most
		constexpr int N = 7;
		double VX = 0.0, VY = 0.0, Wsum = 0.0, G = 0.0, D = 0.0;
		for (int I = 0; I < N; I++)
		{
			const double Ago = OverHours * I / (N - 1);
			const FWindState S = Regime(Hours - Ago, P);
			const double W = std::exp(-Ago / (OverHours * 0.45));
			VX += S.DirX * S.Speed * W;
			VY += S.DirY * S.Speed * W;
			G += S.Gustiness * W;
			D += S.Day * W;
			Wsum += W;
		}
		FWindState S = Regime(Hours, P);
		VX /= Wsum;
		VY /= Wsum;
		const double Speed = std::sqrt(VX * VX + VY * VY);
		if (Speed > 1e-4)
		{
			S.DirX = float(VX / Speed);
			S.DirY = float(VY / Speed);
		}
		S.Speed = float(Speed);
		S.Gustiness = float(G / Wsum);
		S.Day = float(D / Wsum);
		S.FromDeg = float(std::fmod(WorldToBearing(-S.DirX, -S.DirY), 360.0));
		return S;
	}

	FGust GustAt(const FWindState& S, double X, double Y, double OffsetX, double OffsetY, double Seconds)
	{
		// three scales of patches: lulls and surges ~0.5 km across, gusts ~130 m, and squalls of ~30 m inside them
		const double PX = std::fmod(X - OffsetX, GustPeriod) + GustPeriod, PY = std::fmod(Y - OffsetY, GustPeriod) + GustPeriod;
		const double C = std::fmod(Seconds / 10.0, 1024.0);
		const float N0 = ValueNoise3(PX / 512.0, PY / 512.0, C * 0.25, 11, 8);
		const float N1 = ValueNoise3(PX / 128.0, PY / 128.0, C, 12, 32);
		const float N2 = ValueNoise3(PX / 32.0, PY / 32.0, C * 2.0, 13, 128);
		const float NV = ValueNoise3(PX / 128.0, PY / 128.0, C, 14, 32);
		const float G = 0.45f * N0 + 0.45f * N1 + 0.3f * N2;
		FGust Out;
		Out.Factor = std::min(std::max(1.f + S.Gustiness * G * 2.2f, 0.05f), 2.5f);
		Out.Veer = 0.6f * S.Gustiness * NV;
		return Out;
	}

	float HeightFactor(float HeightAboveGroundM, bool bOverWater)
	{
		const double Z0 = bOverWater ? 0.0002 : 0.05;
		const double Z = std::max(double(HeightAboveGroundM), 0.3);
		return float(Clamp(std::log(Z / Z0) / std::log(10.0 / Z0), 0.0, 1.6));
	}
}
