#include "LighthouseModel.h"

#include <algorithm>
#include <cmath>

namespace LyudLighthouse
{
	float Smooth(float E0, float E1, float X)
	{
		if (E0 == E1)
		{
			return X < E0 ? 0.f : 1.f;
		}
		const float T = std::min(std::max((X - E0) / (E1 - E0), 0.f), 1.f);
		return T * T * (3.f - 2.f * T);
	}

	bool LampTime(float Hour, float Sunrise, float Sunset, float VisibilityM)
	{
		const float On = LitAt(Sunset);
		const float Off = OutAt(Sunrise);
		// polar days and nights are not this coast's (lat 55), but keep it sane: On after Off
		const bool bNight = On > Off ? (Hour >= On || Hour < Off) : (Hour >= On && Hour < Off);
		return bNight || VisibilityM < DayFogM;
	}

	float StepLevel(float Level, bool bOn, float Dt)
	{
		const float Rate = Dt / WarmS;
		return bOn ? std::min(Level + Rate, 1.f) : std::max(Level - 2.f * Rate, 0.f);
	}

	float TurnDeg(double Seconds)
	{
		const double Turns = Seconds / double(TurnPeriodS);
		return float((Turns - std::floor(Turns)) * 360.0);
	}

	FBeam BeamFor(float VisibilityM, float Rain, float Level)
	{
		FBeam B;
		const float Vis = std::max(VisibilityM, 1.f);
		// haze: a long visibility leaves little to scatter (a faint card), 5..15 km is the best, fog eats it
		const float Haze = 1.f - Smooth(8000.f, 30000.f, Vis);
		const float Eaten = Smooth(150.f, 3000.f, Vis);
		B.Card = Level * (0.35f + 0.65f * Haze) * (0.25f + 0.75f * Eaten) * (1.f - 0.3f * std::min(std::max(Rain, 0.f), 1.f));
		B.LengthM = std::min(BeamLongM, std::max(BeamShortM, 0.45f * Vis));
		B.Spot = Level * (0.15f + 0.85f * Smooth(300.f, 5000.f, Vis));
		return B;
	}

	float FogHorn(float VisibilityM)
	{
		return Smooth(2500.f, 1200.f, VisibilityM);
	}

	float OffBeam(float TurnDegrees, float ViewYawDeg)
	{
		float D = std::fmod(ViewYawDeg - TurnDegrees, 180.f);
		if (D < 0.f)
		{
			D += 180.f;
		}
		return D > 90.f ? 180.f - D : D;
	}

	float GlassGlow(float OffBeamDeg, float Level)
	{
		// the panel's flash: a few degrees wide (cos^n), on top of the mantle's glow through the glass
		const float A = std::fabs(OffBeamDeg) * 3.14159265f / 180.f;
		const float Flash = std::pow(std::max(std::cos(A), 0.f), 600.f);
		return Level * (0.22f + 0.78f * Flash);
	}

	int Compass8(float FromDeg)
	{
		float D = std::fmod(FromDeg, 360.f);
		if (D < 0.f)
		{
			D += 360.f;
		}
		return int(std::floor((D + 22.5f) / 45.f)) % 8;
	}

	int Beaufort(float Ms)
	{
		static const float Top[12] = { 0.3f, 1.6f, 3.4f, 5.5f, 8.0f, 10.8f, 13.9f, 17.2f, 20.8f, 24.5f, 28.5f, 32.7f };
		for (int I = 0; I < 12; I++)
		{
			if (Ms < Top[I])
			{
				return I;
			}
		}
		return 12;
	}

	int SkyCode(const FReading& R)
	{
		if (R.Thunder > 0.25f)
		{
			return 9;
		}
		if (R.VisibilityM < 1000.f && R.Fog > 0.3f)
		{
			return 10;
		}
		if (R.PrecipMmH > 0.1f)
		{
			if (R.SnowFraction > 0.7f)
			{
				return 8;
			}
			if (R.SnowFraction > 0.25f)
			{
				return 7;
			}
			return R.PrecipMmH < 0.5f ? 4 : (R.PrecipMmH < 4.f ? 5 : 6);
		}
		if (R.VisibilityM < 5000.f)
		{
			return 11;
		}
		return R.Cloud < 0.15f ? 0 : (R.Cloud < 0.45f ? 1 : (R.Cloud < 0.85f ? 2 : 3));
	}

	int Tendency(float HPa3h)
	{
		return HPa3h < -1.f ? -1 : (HPa3h > 1.f ? 1 : 0);
	}

	int VisClass(float M)
	{
		return M < 200.f ? 0 : (M < 1000.f ? 1 : (M < 4000.f ? 2 : (M < 10000.f ? 3 : 4)));
	}

	float HornHours(const std::vector<float>& HourlyVisibilityM)
	{
		float H = 0.f;
		for (float V : HourlyVisibilityM)
		{
			H += FogHorn(V) > 0.5f ? 1.f : 0.f;
		}
		return H;
	}

	std::string Remark(const std::vector<FReading>& Day, float FogHornHours, unsigned Seed)
	{
		if (Day.empty())
		{
			return std::string();
		}
		float WindMax = 0.f, TMin = 1e9f, CloudMin = 1.f, IceMax = 0.f, Thunder = 0.f;
		for (const FReading& R : Day)
		{
			WindMax = std::max(WindMax, R.WindMs);
			TMin = std::min(TMin, R.TempC);
			CloudMin = std::min(CloudMin, R.Cloud);
			IceMax = std::max(IceMax, R.SeaIceCover);
			Thunder = std::max(Thunder, R.Thunder);
		}
		const char* Kind = nullptr;
		int Variants = 2;
		if (Beaufort(WindMax) >= 8)
		{
			Kind = "gale";
			Variants = 3;
		}
		else if (Thunder > 0.25f)
		{
			Kind = "thunder";
		}
		else if (FogHornHours >= 1.f)
		{
			Kind = "fog";
			Variants = 3;
		}
		else if (IceMax > 0.3f)
		{
			Kind = "ice";
		}
		else if (TMin < -5.f)
		{
			Kind = "frost";
		}
		else if (Beaufort(WindMax) <= 2 && CloudMin < 0.2f)
		{
			Kind = "calm";
			Variants = 3;
		}
		if (!Kind)
		{
			// a plain day: now and then the keeper notes something anyway
			return Seed % 3 == 0 ? std::string("plain.") + char('1' + int((Seed / 3) % 4)) : std::string();
		}
		return std::string(Kind) + "." + char('1' + int(Seed % unsigned(Variants)));
	}
}
