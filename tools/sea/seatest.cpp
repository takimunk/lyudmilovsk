// Checks and dumps for the wind and wave models (Private/Weather), outside the engine. See run.sh.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "WaveModel.h"
#include "WindModel.h"
#include "../Cruise/ShipSeaMath.h"

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
	const double Year0 = HoursSince2000(2026, 1, 1, 0.0);
	for (double H = 0; H < 365 * 24; H += 1.0)
	{
		const FWindState S = Regime(Year0 + H, P);
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
		const FWindState A = Regime(Year0 + H, P), B = Regime(Year0 + H + 0.05, P);
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
	// the calm footprint (a hull standing still at the cruise stage): a gale outside, a quiet patch at the hull
	{
		FWaveField G;
		G.Init(FWaveParams());
		G.SetWind(16.f, 0.6f, 0.8f);
		FCalmFootprint None;
		Check(None.At(0.0, 0.0) == 1.f && None.At(26000.0, 4000.0) == 1.f, "no footprint set: calm is exactly 1", None.At(0.0, 0.0));
		FCalmFootprint Fp;
		Fp.CX = 26000.0; Fp.CY = 4000.0; Fp.DirX = 1.0; Fp.DirY = 0.0; Fp.HalfLen = 31.0; Fp.HalfWid = 5.0; Fp.Edge = 10.0;
		Fp.Inner = FCalmFootprint::InnerFor(G.Hs, 0.15f);
		Check(Fp.Inner > 0.f && Fp.Inner < 0.12f, "gale: the footprint keeps under 12 % of the waves", Fp.Inner);
		Check(FCalmFootprint::InnerFor(0.1f, 0.15f) == 1.f, "a sea lower than the bound is left alone", FCalmFootprint::InnerFor(0.1f, 0.15f));
		double In = 0, Out = 0, Far = 0, Jump = 0, Same = 0;
		for (int I = 0; I < 3000; I++)
		{
			const double T = I * 0.37;
			const double X = Fp.CX + std::fmod(I * 13.37, 62.0) - 31.0, Y = Fp.CY + std::fmod(I * 7.77, 10.0) - 5.0;
			In = std::max(In, (double)std::fabs(G.HeightAt(X, Y, T, 25.f, Fp.At(X, Y))));
			const double XF = Fp.CX + 41.0 + std::fmod(I * 3.1, 500.0), YF = Fp.CY + 15.0 + std::fmod(I * 1.7, 500.0);
			Out = std::max(Out, (double)std::fabs(G.HeightAt(XF, YF, T, 25.f, Fp.At(XF, YF))));
			Far += Fp.At(XF, YF) != 1.f;
			Same += G.HeightAt(XF, YF, T, 25.f, Fp.At(XF, YF)) != G.HeightAt(XF, YF, T, 25.f);
			// across the soft edge, 5 cm apart: no step in the surface
			const double YE = Fp.CY + 5.0 + std::fmod(I * 0.0331, 10.0);
			Jump = std::max(Jump, (double)std::fabs(G.HeightAt(X, YE, T, 25.f, Fp.At(X, YE)) - G.HeightAt(X, YE + 0.05, T, 25.f, Fp.At(X, YE + 0.05))));
		}
		std::printf("     gale Hs %.2f m: inside the hull's box the surface stays within %.3f m, outside %.2f m\n", G.Hs, In, Out);
		Check(In < 0.2, "inside the footprint the surface stays within 0.2 m (deck D is 0.3 m up)", In);
		Check(Out > 1.0, "outside it the gale is the gale", Out);
		Check(Far == 0 && Same == 0, "beyond the edge: calm exactly 1, heights bit for bit", Far + Same);
		Check(Jump < 0.12, "no step across the soft edge (5 cm apart, m)", Jump);
		Check(std::fabs(Fp.Distance(Fp.CX + 31.0, Fp.CY)) < 1e-9 && std::fabs(Fp.Distance(Fp.CX, Fp.CY) + 5.0) < 1e-9
			&& std::fabs(Fp.Distance(Fp.CX + 34.0, Fp.CY + 9.0) - 5.0) < 1e-9, "distance to the box (edge, centre, corner)", Fp.Distance(Fp.CX + 34.0, Fp.CY + 9.0));
		Check(Fp.At(Fp.CX, Fp.CY) == Fp.Inner && Fp.At(Fp.CX, Fp.CY + 15.0) == 1.f, "inner share inside, 1 at the edge", Fp.At(Fp.CX, Fp.CY + 10.0));
		double Mono = 0, Last = Fp.Inner;
		for (double D = 0; D <= 10.0; D += 0.01) { const double C = Fp.At(Fp.CX, Fp.CY + 5.0 + D); Mono += C < Last - 1e-7; Last = C; }
		Check(Mono == 0, "calm grows steadily across the edge", Mono);
		// turned 30 degrees: the same patch in its own frame
		FCalmFootprint Tu = Fp;
		Tu.DirX = std::cos(M_PI / 6); Tu.DirY = std::sin(M_PI / 6);
		Check(std::fabs(Tu.Distance(Fp.CX + 31.0 * Tu.DirX, Fp.CY + 31.0 * Tu.DirY)) < 1e-9, "a turned footprint: its bow is on its edge", Tu.Distance(Fp.CX + 31.0 * Tu.DirX, Fp.CY + 31.0 * Tu.DirY));
		// the cap of the sea state aboard: the wind that grows Hs 1.2 m
		const float UCap = G.WindForHs(1.2f);
		FWaveField C = G;
		C.SetWind(UCap, 0.6f, 0.8f);
		Check(C.Hs <= 1.2f && C.Hs > 1.17f && UCap > 5.f && UCap < 10.f, "WindForHs(1.2): the capped sea is Hs 1.2 m", C.Hs);
		Check(G.WindForHs(50.f) >= 39.f, "a cap no wind reaches caps nothing", G.WindForHs(50.f));
	}
	// the tide held at the datum aboard, the ride of the ship on the roads, the camera's sway
	{
		using namespace LyudShipSea;
		Check(Held(-450.0, 0.0) == -450.0 && Held(-450.0, 1.0) == 0.0 && Held(-450.0, 0.5) == -225.0, "tide hold: free, held at the datum, half way", Held(-450.0, 0.5));
		Check(StepHold(0.0, true, 0.2, 0.0) == 1.0 && StepHold(1.0, false, 0.2, 0.0) == 0.0, "hold at once behind a fade", 1);
		double H = 0.0, MaxStep = 0.0;
		int N = 0;
		for (; H < 1.0 && N < 100000; N++) { const double B = H; H = StepHold(H, true, 1.0 / 60, 60.0); MaxStep = std::max(MaxStep, H - B); }
		Check(std::fabs(N / 60.0 - 60.0) < 0.1 && MaxStep * 450.0 < 0.13, "eased over a minute: 4.5 m in steps under 1.3 mm a frame", MaxStep * 450.0);
		Check(StepHold(0.4, false, 100.0, 60.0) == 0.0 && StepHold(0.4, true, 100.0, 60.0) == 1.0, "the hold never overshoots", 0);
		FRide R = RideFrom(200.0, -200.0, 150.0, -150.0, 6200.0, 1000.0);
		Check(R.PitchDeg > 0.0 && R.PitchDeg <= 0.8 && R.RollDeg > 0.0 && R.RollDeg <= 1.5 && R.HeaveCm == 0.0, "ride: bow up, starboard down, bounded", R.RollDeg);
		R = RideFrom(900.0, 900.0, 900.0, 900.0, 6200.0, 1000.0);
		Check(R.HeaveCm == 20.0 && R.PitchDeg == 0.0 && R.RollDeg == 0.0, "ride: heave within 20 cm", R.HeaveCm);
		FRide E;
		for (int I = 0; I < 600; I++) E = EaseRide(E, R, 1.0 / 60, 1.5);
		Check(std::fabs(E.HeaveCm - 20.0) < 0.1, "ride eases to its target", E.HeaveCm);
		double MaxRoll = 0, MaxPitch = 0, MaxRate = 0, Rl, Pt, Rl2, Pt2;
		for (int I = 0; I < 200000; I++)
		{
			Sway(I * 0.05, 3.0, 1.0, Rl, Pt);
			Sway(I * 0.05 + 0.01, 3.0, 1.0, Rl2, Pt2);
			MaxRoll = std::max(MaxRoll, std::fabs(Rl)); MaxPitch = std::max(MaxPitch, std::fabs(Pt));
			MaxRate = std::max(MaxRate, std::fabs(Rl2 - Rl) / 0.01);
		}
		Check(MaxRoll <= 0.6 && MaxRoll > 0.5 && MaxPitch <= 0.24, "sway in a gale at full scale: roll within 0.6 deg", MaxRoll);
		Check(MaxRate < 0.5, "sway is slow (deg/s)", MaxRate);
		Sway(123.4, 0.0, 1.0, Rl, Pt);
		Sway(123.4, 2.0, 0.0, Rl2, Pt2);
		Check(Rl == 0.0 && Pt == 0.0 && Rl2 == 0.0 && Pt2 == 0.0, "no sway in a flat calm or at lyud.Cruise.Sway 0", 0);
		Sway(50.0, 0.6, 0.5, Rl, Pt);
		Sway(50.0, 1.2, 1.0, Rl2, Pt2);
		Check(std::fabs(Rl * 4.0 - Rl2) < 1e-12, "sway scales with the sea and the cvar", Rl);
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
