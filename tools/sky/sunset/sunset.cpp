// The sunset model outside the engine (tools/sky/sunset/run.sh): prints what it does and checks it.
#include "Sky/SunsetModel.h"
#include "Weather/WeatherModel.h"

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace LyudSunset;

namespace
{
	int Failures = 0;

	void Check(bool bOk, const char* What, double Value)
	{
		std::printf("%s %-58s %.3f\n", bOk ? "ok  " : "FAIL", What, Value);
		Failures += bOk ? 0 : 1;
	}

	bool Neutral(const FLook& L)
	{
		return L.MieScale == 1.f && L.MieGAdd == 0.f && L.MieAbsScale == 1.f && L.MieHeightScale == 1.f && L.RayleighScale == 1.f
			&& L.OzoneScale == 1.f && L.SkyTint.R == 1.f && L.SkyTint.G == 1.f && L.SkyTint.B == 1.f && L.LightTint.R == 1.f
			&& L.LightTint.G == 1.f && L.LightTint.B == 1.f && L.CloudTint.R == 1.f && L.CloudTint.G == 1.f && L.CloudTint.B == 1.f
			&& L.AureoleScale == 1.f && L.DiskScale == 1.f && L.ExposureEV == 0.f;
	}

	float MaxDiff(const FLook& A, const FLook& B)
	{
		const float V[] = {A.MieScale - B.MieScale, A.MieGAdd - B.MieGAdd, A.MieAbsScale - B.MieAbsScale, A.MieHeightScale - B.MieHeightScale,
			A.RayleighScale - B.RayleighScale, A.OzoneScale - B.OzoneScale, A.SkyTint.R - B.SkyTint.R, A.SkyTint.G - B.SkyTint.G,
			A.SkyTint.B - B.SkyTint.B, A.LightTint.R - B.LightTint.R, A.LightTint.G - B.LightTint.G, A.LightTint.B - B.LightTint.B,
			A.CloudTint.R - B.CloudTint.R, A.CloudTint.G - B.CloudTint.G, A.CloudTint.B - B.CloudTint.B, A.AureoleScale - B.AureoleScale,
			A.DiskScale - B.DiskScale, A.ExposureEV - B.ExposureEV};
		float M = 0.f;
		for (float X : V) M = std::fmax(M, std::fabs(X));
		return M;
	}

	LyudWeather::FWeatherState Weather(const char* Name)
	{
		LyudWeather::FWeatherState W;
		W.Humidity = 0.7f; W.VisibilityM = 30000.f; W.WindSpeed = 5.f;
		if (!std::strcmp(Name, "broken")) { W.High.Cover = 0.5f; W.High.Density = 0.3f; W.Mid.Cover = 0.4f; W.Mid.Density = 0.4f; }
		if (!std::strcmp(Name, "overcast")) { W.Low.Cover = 1.f; W.Low.Density = 0.9f; W.Low.Sheet = 1.f; W.Humidity = 0.9f; }
		if (!std::strcmp(Name, "afterrain")) { W.Wet = 0.8f; W.Low.Cover = 0.3f; W.Low.Density = 0.5f; W.Low.Sheet = 0.2f; W.WindSpeed = 8.f; }
		if (!std::strcmp(Name, "haze")) { W.Humidity = 0.95f; W.VisibilityM = 6000.f; W.Inversion = 0.6f; }
		if (!std::strcmp(Name, "rain")) { W.PrecipRate = 2.f; W.Low.Cover = 1.f; W.Low.Density = 1.f; }
		return W;
	}

	void Print(const char* Tag, const FDayProfile& P, const FLook& L)
	{
		std::printf("%-10s %-8s%s hue %.2f int %.2f aero %.2f glow %.2f | mie %.2f abs %.2f ray %.2f oz %.2f | light %.2f %.2f %.2f cloud %.2f %.2f %.2f sky %.2f %.2f %.2f | disk %.2f aur %.2f\n",
			Tag, KindName(P.Kind), P.bSpectacular ? "*" : " ", P.Hue, P.Intensity, P.Aerosol, P.CloudGlow, L.MieScale, L.MieAbsScale,
			L.RayleighScale, L.OzoneScale, L.LightTint.R, L.LightTint.G, L.LightTint.B, L.CloudTint.R, L.CloudTint.G, L.CloudTint.B,
			L.SkyTint.R, L.SkyTint.G, L.SkyTint.B, L.DiskScale, L.AureoleScale);
	}
}

int main(int Argc, char** Argv)
{
	const bool bOnlyTest = Argc > 1 && !std::strcmp(Argv[1], "test");
	const uint32_t Seed = 0;
	const int64_t Day0 = 9800;   // 2026-10-31
	if (!bOnlyTest)
	{
		std::printf("a month of evenings (clear air, sun at +2):\n");
		const FAir Clear = AirOf(Weather("clear"));
		for (int D = 0; D < 30; D++)
		{
			char Tag[16];
			std::snprintf(Tag, sizeof Tag, "day %d", D);
			const FDayProfile P = Profile(Seed, Day0 + D, false, 11);
			Print(Tag, P, Look(P, Clear, 2.f));
		}
		std::printf("\none evening in each weather, and each kind forced (sun at +2 / -4):\n");
		const char* Ws[] = {"clear", "broken", "overcast", "afterrain", "haze", "rain"};
		for (int K = 0; K <= 6; K++)
		{
			const FDayProfile P = Profile(Seed, Day0, false, 11, K);
			for (const char* Wn : Ws)
			{
				Print(Wn, P, Look(P, AirOf(Weather(Wn)), 2.f));
			}
			Print("  at -4", P, Look(P, AirOf(Weather("broken")), -4.f));
		}
		std::printf("\n");
	}

	// checks
	{
		const FDayProfile A = Profile(7, 12345, false, 6), B = Profile(7, 12345, false, 6);
		Check(A.Kind == B.Kind && A.Hue == B.Hue && A.Intensity == B.Intensity && A.Aerosol == B.Aerosol && A.CloudGlow == B.CloudGlow && A.bSpectacular == B.bSpectacular, "deterministic: the same day gives the same profile", 0);
		const FDayProfile S = Profile(7, 12345, true, 6);
		Check(S.Hue != A.Hue || S.Kind != A.Kind, "the sunrise has its own profile", S.Hue - A.Hue);
	}
	{
		int Kinds[int(EKind::Count)] = {}, Spec = 0;
		const int N = 20000;
		for (int D = 0; D < N; D++)
		{
			const FDayProfile P = Profile(Seed, D, false, 1 + (D / 30) % 12);
			Kinds[int(P.Kind)]++;
			Spec += P.bSpectacular ? 1 : 0;
		}
		Check(Spec > N * 0.03 && Spec < N * 0.07, "spectacular evenings ~5%", double(Spec) / N);
		for (int K = 0; K < int(EKind::Count); K++)
		{
			char Msg[64];
			std::snprintf(Msg, sizeof Msg, "kind %s comes 4..40%% of evenings", KindName(EKind(K)));
			Check(Kinds[K] > N * 0.04 && Kinds[K] < N * 0.4, Msg, double(Kinds[K]) / N);
		}
		// neighbouring days differ
		int Same = 0;
		for (int D = 0; D < 1000; D++) Same += Profile(Seed, D, false, 11).Kind == Profile(Seed, D + 1, false, 11).Kind ? 1 : 0;
		Check(Same < 400, "neighbouring evenings mostly differ in kind", Same / 1000.0);
	}
	{
		const FDayProfile P = Profile(Seed, Day0, false, 11, 6);
		const FAir Air = AirOf(Weather("broken"));
		bool bNeutral = true;
		for (float Alt : {-90.f, -30.f, -9.f, -9.5f, 11.f, 15.f, 25.f, 45.f, 60.f}) bNeutral &= Neutral(Look(P, Air, Alt));
		Check(bNeutral, "exactly neutral at midday and at night", 0);
		Check(Neutral(Look(P, Air, 2.f, 0.f)), "lyud.Sunset.Intensity 0 is neutral", 0);
		float Worst = 0.f;
		for (float Alt = -12.f; Alt < 14.f; Alt += 0.01f) Worst = std::fmax(Worst, MaxDiff(Look(P, Air, Alt), Look(P, Air, Alt + 0.01f)));
		Check(Worst < 0.03f, "continuous in the sun's altitude (max step per 0.01 deg)", Worst);
		// continuous in the weather: blend clear -> overcast in small steps
		LyudWeather::FWeatherState W0 = Weather("clear"), W1 = Weather("overcast");
		float WorstW = 0.f;
		FLook Prev = Look(P, AirOf(W0), 1.f);
		for (int I = 1; I <= 200; I++)
		{
			const float T = I / 200.f;
			LyudWeather::FWeatherState W = W0;
			W.Low.Cover = T * W1.Low.Cover; W.Low.Density = T * W1.Low.Density; W.Low.Sheet = T; W.Humidity = 0.7f + 0.2f * T;
			const FLook L = Look(P, AirOf(W), 1.f);
			WorstW = std::fmax(WorstW, MaxDiff(L, Prev));
			Prev = L;
		}
		Check(WorstW < 0.08f, "continuous in the weather (clear -> overcast, 200 steps)", WorstW);
	}
	{
		// the weather's character: overcast dulls, broken clouds glow most, haze reddens and dims the disk
		const FDayProfile P = Profile(Seed, Day0, false, 11, int(EKind::Pink));
		const FLook Clear = Look(P, AirOf(Weather("clear")), 2.f), Broken = Look(P, AirOf(Weather("broken")), 2.f);
		const FLook Over = Look(P, AirOf(Weather("overcast")), 2.f), Haze = Look(P, AirOf(Weather("haze")), 2.f);
		const FLook Wet = Look(P, AirOf(Weather("afterrain")), 2.f);
		auto Chroma = [](const FRGB& C) { return std::fmax(C.R, std::fmax(C.G, C.B)) - std::fmin(C.R, std::fmin(C.G, C.B)); };
		Check(Chroma(Broken.CloudTint) > Chroma(Clear.CloudTint) && Broken.CloudTint.R > Clear.CloudTint.R, "broken cloud glows more than clear", Chroma(Broken.CloudTint));
		Check(Chroma(Over.CloudTint) < 0.5f * Chroma(Broken.CloudTint) && Chroma(Over.LightTint) < Chroma(Clear.LightTint), "overcast is dull", Chroma(Over.CloudTint));
		Check(Haze.MieScale > Clear.MieScale && Haze.DiskScale < Clear.DiskScale, "haze: more Mie, a dimmer disk", Haze.MieScale);
		Check(Wet.MieScale < Clear.MieScale && Chroma(Wet.LightTint) >= Chroma(Clear.LightTint), "after rain: cleaner air, colours at least as saturated", Wet.MieScale);
		const FDayProfile G = Profile(Seed, Day0, false, 11, int(EKind::Golden)), C = Profile(Seed, Day0, false, 11, int(EKind::Crimson));
		const FLook Lg = Look(G, AirOf(Weather("broken")), 2.f), Lc = Look(C, AirOf(Weather("broken")), 2.f);
		Check(Lc.CloudTint.G / Lc.CloudTint.R < Lg.CloudTint.G / Lg.CloudTint.R, "crimson clouds redder than golden ones", Lc.CloudTint.G / Lc.CloudTint.R);
		// no runaway values
		float MaxC = 0.f;
		for (int K = 0; K <= 6; K++)
			for (const char* Wn : {"clear", "broken", "overcast", "afterrain", "haze", "rain"})
				for (float Alt = -9.f; Alt < 11.f; Alt += 0.5f)
				{
					const FLook L = Look(Profile(Seed, Day0 + K, false, 11, K), AirOf(Weather(Wn)), Alt);
					MaxC = std::fmax(MaxC, std::fmax(L.CloudTint.R, std::fmax(L.LightTint.R, L.SkyTint.R)));
				}
		Check(MaxC < 2.5f, "tints stay bounded (max channel)", MaxC);
	}
	std::printf("%s (%d failed)\n", Failures ? "FAILED" : "all ok", Failures);
	return Failures ? 1 : 0;
}
