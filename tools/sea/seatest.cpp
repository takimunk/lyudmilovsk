// Checks and dumps for the wind and wave models (Private/Weather), outside the engine. See run.sh.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "WaveModel.h"
#include "WindModel.h"

using namespace LyudWeather;

static int Failures = 0;
static void Check(bool Ok, const char* What, double Value)
{
	std::printf("%s %-58s %.4f\n", Ok ? "ok  " : "FAIL", What, Value);
	if (!Ok) Failures++;
}

static int Test()
{
	FWindParams P;
	Check(std::fabs(HoursSince2000(2000, 1, 1, 0.0)) < 1e-9, "2000-01-01 is hour 0", HoursSince2000(2000, 1, 1, 0.0));
	Check(std::fabs(HoursSince2000(2026, 10, 20, 16.0) - (9789.0 * 24 + 16)) < 1e-9, "2026-10-20 16:00 is day 9789", HoursSince2000(2026, 10, 20, 16.0) / 24);

	// day factor: dark at night, stirred in the afternoon; October sunrise ~7:40, sunset ~17:25 here
	const double Oct = HoursSince2000(2026, 10, 20, 0.0);
	Check(DayFactor(Oct + 3.0, P) == 0.f, "no stirring at 03:00", DayFactor(Oct + 3.0, P));
	Check(DayFactor(Oct + 22.0, P) == 0.f, "no stirring at 22:00", DayFactor(Oct + 22.0, P));
	Check(DayFactor(Oct + 13.5, P) > 0.35f, "stirred at 13:30 in October", DayFactor(Oct + 13.5, P));
	const double Jun = HoursSince2000(2026, 6, 21, 0.0);
	Check(DayFactor(Jun + 5.5, P) > 0.f && DayFactor(Jun + 14.0, P) > DayFactor(Oct + 14.0, P), "June: stirred earlier and more", DayFactor(Jun + 14.0, P));

	// a year of weather: median near MedianSpeed, prevailing from the south-west, nights calmer than afternoons
	std::vector<double> Speeds;
	double Night = 0, NightN = 0, Aft = 0, AftN = 0, SinS = 0, CosS = 0, Calm = 0, Gale = 0;
	const double Y0 = HoursSince2000(2026, 1, 1, 0.0);
	for (double H = 0; H < 365 * 24; H += 1.0)
	{
		const FWindState S = Regime(Y0 + H, P);
		Speeds.push_back(S.Speed);
		const double Hod = std::fmod(H, 24.0);
		if (Hod >= 1 && Hod <= 4) { Night += S.Speed; NightN++; }
		if (Hod >= 13 && Hod <= 15) { Aft += S.Speed; AftN++; }
		SinS += std::sin(S.FromDeg * M_PI / 180) * S.Speed;
		CosS += std::cos(S.FromDeg * M_PI / 180) * S.Speed;
		Calm += S.Speed < 1.5;
		Gale += S.Speed > 14.0;
		if (std::fabs(S.DirX * S.DirX + S.DirY * S.DirY - 1.f) > 1e-3f) { Check(false, "unit direction", S.DirX); break; }
	}
	std::sort(Speeds.begin(), Speeds.end());
	const double Med = Speeds[Speeds.size() / 2];
	double Mean = 0; for (double V : Speeds) Mean += V; Mean /= Speeds.size();
	double Prev = std::atan2(SinS, CosS) * 180 / M_PI; if (Prev < 0) Prev += 360;
	Check(Med > 3.5 && Med < 7.0, "year median wind 3.5..7 m/s", Med);
	Check(Mean > 4.0 && Mean < 8.0, "year mean wind 4..8 m/s", Mean);
	Check(Prev > 200 && Prev < 280, "resultant wind from SW..W (deg)", Prev);
	Check(Night / NightN < 0.9 * Aft / AftN, "nights (01-04) calmer than afternoons (13-15)", (Night / NightN) / (Aft / AftN));
	Check(Calm / Speeds.size() > 0.01 && Calm / Speeds.size() < 0.2, "calm (<1.5 m/s) hours 1..20 %", 100 * Calm / Speeds.size());
	Check(Gale / Speeds.size() < 0.06, "strong (>14 m/s) hours under 6 %", 100 * Gale / Speeds.size());

	// smooth in time: no jumps hour to hour
	double MaxDs = 0, MaxDd = 0;
	for (double H = 0; H < 30 * 24; H += 0.05)
	{
		const FWindState A = Regime(Y0 + H, P), B = Regime(Y0 + H + 0.05, P);
		MaxDs = std::max(MaxDs, (double)std::fabs(A.Speed - B.Speed));
		double Dd = std::fabs(A.FromDeg - B.FromDeg); if (Dd > 180) Dd = 360 - Dd;
		if (A.Speed > 1.5) MaxDd = std::max(MaxDd, Dd);
	}
	Check(MaxDs < 0.35, "speed change per 3 min under 0.35 m/s", MaxDs);
	Check(MaxDd < 6.0, "direction change per 3 min under 6 deg (wind > 1.5)", MaxDd);

	// gusts: mean factor ~1, spread ~ gustiness, continuous over the drift wrap
	{
		FWindState S = Regime(Oct + 14.0, P);
		double Sum = 0, Sum2 = 0, Mx = 0; int N = 0;
		for (int I = 0; I < 20000; I++)
		{
			const FGust G = GustAt(S, (I % 200) * 7.3, (I / 200) * 11.1, 0, 0, I * 0.37);
			Sum += G.Factor; Sum2 += G.Factor * G.Factor; Mx = std::max(Mx, (double)G.Factor); N++;
		}
		const double M = Sum / N, Sd = std::sqrt(Sum2 / N - M * M);
		Check(std::fabs(M - 1.0) < 0.06, "gust factor mean ~1", M);
		Check(Sd > 0.4 * S.Gustiness && Sd < 1.6 * S.Gustiness, "gust spread ~ gustiness (ratio)", Sd / S.Gustiness);
		Check(Mx > 1.25 && Mx < 2.5, "peak gust factor 1.25..2.5", Mx);
		const FGust A = GustAt(S, 10, 20, 4095.999, 0, 50), B = GustAt(S, 10, 20, -0.001, 0, 50);
		Check(std::fabs(A.Factor - B.Factor) < 1e-3, "gust field seamless across the drift wrap", std::fabs(A.Factor - B.Factor));
		const FGust C = GustAt(S, 10, 20, 0, 0, 10239.999), D = GustAt(S, 10, 20, 0, 0, 0.0);
		Check(std::fabs(C.Factor - D.Factor) < 2e-3, "gust field seamless across the clock wrap", std::fabs(C.Factor - D.Factor));
	}

	// waves
	FWaveField F;
	F.Init(FWaveParams());
	const float Ws[] = { 0.5f, 3.f, 6.f, 9.f, 13.f, 18.f };
	for (float U : Ws)
	{
		F.SetWind(U, 1.f, 0.f);
		char Buf[128];
		std::snprintf(Buf, sizeof Buf, "wind %4.1f m/s: Hs %.2f m, peak %5.1f m, chop %.2f", U, F.Hs, F.PeakLambda, F.Chop);
		std::printf("     %s\n", Buf);
	}
	F.SetWind(8.f, 1.f, 0.f);
	Check(F.Hs > 0.6f && F.Hs < 1.6f, "8 m/s over 60 km: Hs 0.6..1.6 m", F.Hs);
	Check(F.PeakLambda > 20.f && F.PeakLambda < 60.f, "8 m/s: peak wavelength 20..60 m", F.PeakLambda);
	F.SetWind(2.f, 1.f, 0.f);
	Check(F.Hs < 0.15f, "2 m/s: Hs under 15 cm", F.Hs);
	F.SetWind(15.f, 1.f, 0.f);
	Check(F.Hs > 1.8f && F.Hs < 4.5f, "15 m/s: Hs 1.8..4.5 m", F.Hs);

	// every frequency a whole number of turns per loop: the wrapped clock gives the same sea
	F.SetWind(9.f, 0.6f, 0.8f);
	{
		const double T1 = 12345.678, T2 = F.Wrap(T1);
		const float H1 = F.HeightAt(37.5, -12.25, T1, 50.f), H2 = F.HeightAt(37.5, -12.25, T2, 50.f);
		Check(std::fabs(H1 - H2) < 2e-3f, "height same at t and wrapped t", std::fabs(H1 - H2));
	}
	// the surface found over a point is the displaced particle's height there
	{
		double MaxErr = 0, MinJ = 1e9, MinN = 1;
		for (int I = 0; I < 2000; I++)
		{
			const double X0 = std::fmod(I * 13.37, 300.0) - 150, Y0 = std::fmod(I * 7.77, 300.0) - 150, T = I * 0.731;
			float D[3];
			F.Displace(X0, Y0, T, 50.f, D);
			const FSeaSurface S = F.SurfaceAt(X0 + D[0], Y0 + D[1], T, 50.f);
			MaxErr = std::max(MaxErr, (double)std::fabs(S.Height - D[2]));
			MinJ = std::min(MinJ, (double)F.Jacobian(X0, Y0, T));
			MinN = std::min(MinN, (double)S.Normal[2]);
		}
		Check(MaxErr < 0.02, "SurfaceAt(displaced point) = particle height (m)", MaxErr);
		Check(MinJ > 0.0, "crests never fold (min Jacobian > 0)", MinJ);
		Check(MinN > 0.3, "normals point up", MinN);
	}
	// velocity = d(position)/dt
	{
		const double X0 = 11, Y0 = -4, T = 100, Dt = 1e-3;
		float A[3], B[3];
		F.Displace(X0, Y0, T - Dt, 50.f, A);
		F.Displace(X0, Y0, T + Dt, 50.f, B);
		const FSeaSurface S = F.SurfaceAt(X0 + (A[0] + B[0]) / 2, Y0 + (A[1] + B[1]) / 2, T, 50.f);
		const double Vz = (B[2] - A[2]) / (2 * Dt);
		Check(std::fabs(S.Velocity[2] - Vz) < 0.05, "vertical velocity matches d/dt (m/s)", std::fabs(S.Velocity[2] - Vz));
	}
	// the beach: waves shrink in shallow water and die at the water line
	{
		double Deep = 0, Shallow = 0, Edge = 0;
		for (int I = 0; I < 400; I++)
		{
			Deep = std::max(Deep, (double)std::fabs(F.HeightAt(I * 1.7, I * 0.3, I * 0.9, 30.f)));
			Shallow = std::max(Shallow, (double)std::fabs(F.HeightAt(I * 1.7, I * 0.3, I * 0.9, 0.6f)));
			Edge = std::max(Edge, (double)std::fabs(F.HeightAt(I * 1.7, I * 0.3, I * 0.9, 0.02f)));
		}
		Check(Shallow < 0.6 * 0.6 && Shallow < Deep, "0.6 m deep: crest under 0.36 m and under deep water's", Shallow);
		Check(Edge < 0.02, "at the water line: no waves", Edge);
	}
	// a change of wind direction changes no train's phase, only amplitudes
	{
		FWaveField A = F;
		A.SetWind(9.f, -0.8f, 0.6f);
		bool Same = true;
		for (size_t I = 0; I < F.Geo.size(); I++)
			Same &= F.Geo[I].Phase == A.Geo[I].Phase && F.Geo[I].K == A.Geo[I].K && F.Geo[I].DirX == A.Geo[I].DirX;
		Check(Same, "wind turn keeps every train's phase, wavenumber, direction", 1);
	}
	std::printf("%s: %d failure(s)\n", Failures ? "FAILED" : "PASSED", Failures);
	return Failures ? 1 : 0;
}

static int Days(int Argc, char** Argv)
{
	int Y = 2026, M = 10, D = 20, N = 4;
	if (Argc >= 6) { Y = std::atoi(Argv[2]); M = std::atoi(Argv[3]); D = std::atoi(Argv[4]); N = std::atoi(Argv[5]); }
	FWindParams P;
	FWaveField F;
	F.Init(FWaveParams());
	FILE* Out = std::fopen("Saved/Sea/wind_days.csv", "w");
	std::fprintf(Out, "hour,from_deg,speed,weather,day,gustiness,sea_speed,sea_from,hs,peak\n");
	const double H0 = HoursSince2000(Y, M, D, 0.0);
	for (double H = 0; H <= N * 24; H += 0.5)
	{
		const FWindState S = Regime(H0 + H, P), SS = SeaStateWind(H0 + H, P);
		F.SetWind(SS.Speed, SS.DirX, SS.DirY);
		std::fprintf(Out, "%.1f,%.1f,%.2f,%.2f,%.2f,%.2f,%.2f,%.1f,%.2f,%.1f\n", H, S.FromDeg, S.Speed, S.Weather, S.Day, S.Gustiness, SS.Speed, SS.FromDeg, F.Hs, F.PeakLambda);
		if (std::fmod(H, 3.0) == 0.0)
			std::printf("day %d %02d:00  from %5.1f  %5.2f m/s  day %.2f  gust %.2f  sea %.1f m/s  Hs %.2f m  peak %.0f m\n", int(H / 24), int(std::fmod(H, 24)), S.FromDeg, S.Speed, S.Day, S.Gustiness, SS.Speed, F.Hs, F.PeakLambda);
	}
	std::fclose(Out);
	return 0;
}

static int Field(int Argc, char** Argv)
{
	const float U = Argc >= 3 ? (float)std::atof(Argv[2]) : 8.f;
	FWaveField F;
	F.Init(FWaveParams());
	F.SetWind(U, 1.f, 0.f);
	const int N = 512;
	const double Size = 200.0;
	std::vector<float> H(N * N);
	float Lo = 1e9, Hi = -1e9;
	for (int J = 0; J < N; J++)
		for (int I = 0; I < N; I++)
		{
			const float V = F.HeightAt(I * Size / N, J * Size / N, 30.0, 40.f);
			H[J * N + I] = V; Lo = std::min(Lo, V); Hi = std::max(Hi, V);
		}
	char Name[64];
	std::snprintf(Name, sizeof Name, "Saved/Sea/sea_%g.pgm", U);
	FILE* Out = std::fopen(Name, "wb");
	std::fprintf(Out, "P5 %d %d 255\n", N, N);
	for (float V : H) { unsigned char C = (unsigned char)(255 * (V - Lo) / std::max(Hi - Lo, 1e-6f)); std::fwrite(&C, 1, 1, Out); }
	std::fclose(Out);
	std::printf("%s: Hs %.2f m, range %.2f..%.2f m\n", Name, F.Hs, Lo, Hi);
	return 0;
}

int main(int Argc, char** Argv)
{
	const std::string Cmd = Argc >= 2 ? Argv[1] : "test";
	if (Cmd == "days") return Days(Argc, Argv);
	if (Cmd == "field") return Field(Argc, Argv);
	return Test();
}
