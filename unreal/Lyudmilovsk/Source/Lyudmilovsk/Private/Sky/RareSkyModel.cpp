#include "RareSkyModel.h"

#include <algorithm>
#include <cmath>

namespace LyudRareSky
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr double KmPerDeg = 111.2;

		uint64_t Mix(uint64_t X)
		{
			X += 0x9E3779B97F4A7C15ull;
			X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
			X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
			return X ^ (X >> 31);
		}

		/** A uniform 0..1 from a seed, an integer key and a salt. */
		double Hash(uint32_t Seed, int64_t Key, uint32_t Salt)
		{
			const uint64_t H = Mix(Mix(uint64_t(Seed) * 0x100000001B3ull ^ uint64_t(Salt)) ^ uint64_t(Key));
			return double(H >> 11) * (1.0 / 9007199254740992.0);
		}

		double Clamp01(double X) { return X < 0.0 ? 0.0 : (X > 1.0 ? 1.0 : X); }
		double Smooth(double A, double B, double X)
		{
			const double T = Clamp01((X - A) / (B - A));
			return T * T * (3.0 - 2.0 * T);
		}

		/** Smooth value noise over time: Period hours between its knots. */
		double TimeNoise(double Hours, double Period, uint32_t Seed, uint32_t Salt)
		{
			const double X = Hours / Period;
			const double I = std::floor(X);
			const double F = X - I;
			const double U = F * F * (3.0 - 2.0 * F);
			const double A = Hash(Seed, int64_t(I), Salt), B = Hash(Seed, int64_t(I) + 1, Salt);
			return A + (B - A) * U;
		}

		/** The magnetic night a moment belongs to (noon to noon in magnetic time) and the hour it starts at. */
		int64_t MagNight(double Hours, double MagMidnight, double& OutStart)
		{
			const double Rel = Hours - MagMidnight + 12.0;
			const int64_t N = int64_t(std::floor(Rel / 24.0));
			OutStart = double(N) * 24.0 + MagMidnight - 12.0;
			return N;
		}

		constexpr int Candidates = 6;

		/** Candidate K of night N: its breakup hour, its strength and whether it happens at this Kp (0..1, soft). */
		void Candidate(int64_t N, int K, double NightStart, uint32_t Seed, float Kp, double& Onset, double& Amp, double& On)
		{
			// the candidates spread over the dark hours from 6 h before magnetic midnight to 5 h after it, about 2 h apart
			Onset = NightStart + 6.0 + (K + 0.15 + 0.7 * Hash(Seed, N * 16 + K, 11)) * (11.0 / Candidates);
			Amp = 0.55 + 0.75 * Hash(Seed, N * 16 + K, 12);
			// a quiet night has one weak substorm now and then, an active one several
			const double P = Clamp01(0.08 + 0.13 * Kp);
			const double H = Hash(Seed, N * 16 + K, 13);
			On = Smooth(H - 0.06, H + 0.06, P);
		}
	}

	const char* PhaseName(EAuroraPhase P)
	{
		switch (P)
		{
		case EAuroraPhase::Quiet: return "quiet";
		case EAuroraPhase::Growth: return "growth";
		case EAuroraPhase::Breakup: return "breakup";
		case EAuroraPhase::Recovery: return "recovery";
		default: return "none";
		}
	}

	double OvalEdgeLat(double Kp, double MltHours)
	{
		// the equatorward boundary of the diffuse aurora at magnetic midnight, ~66 degrees on a still night to ~48 in a
		// Kp 9 storm (Starkov 1994, simplified), a few degrees nearer the pole toward dawn and dusk and far more by day
		const double Midnight = 66.0 - 2.0 * Kp;
		return Midnight + 4.5 * (1.0 - std::cos(2.0 * Pi * MltHours / 24.0));
	}

	int SubstormOnsets(double Hours, uint32_t Seed, float Kp, double* Out, int Max)
	{
		double Start = 0.0;
		const FSite Site;
		const int64_t N = MagNight(Hours, Site.MagMidnightHour, Start);
		int Count = 0;
		for (int K = 0; K < Candidates && Count < Max; ++K)
		{
			double Onset, Amp, On;
			Candidate(N, K, Start, Seed, Kp, Onset, Amp, On);
			if (On > 0.5)
			{
				Out[Count++] = Onset;
			}
		}
		return Count;
	}

	int Flares(double From, double To, uint32_t Seed, double* OutStart, float* OutPeak, int Max)
	{
		// slots of 6 h, each with its chance of a small flare (one every ~4 days) and of a big one (~6 a year)
		int Count = 0;
		const int64_t S0 = int64_t(std::floor(From / 6.0)), S1 = int64_t(std::floor(To / 6.0));
		for (int64_t S = S0; S <= S1 && Count < Max; ++S)
		{
			const double Start = (double(S) + Hash(Seed, S, 21)) * 6.0;
			if (Start < From || Start >= To)
			{
				continue;
			}
			const double H = Hash(Seed, S, 22);
			double Peak = 0.0;
			if (H < 0.0068)
			{
				Peak = 0.55 + 0.45 * Hash(Seed, S, 23);
			}
			else if (H < 0.0068 + 0.0625)
			{
				Peak = 0.12 + 0.25 * Hash(Seed, S, 23);
			}
			if (Peak > 0.0)
			{
				OutStart[Count] = Start;
				OutPeak[Count] = float(Peak);
				++Count;
			}
		}
		return Count;
	}

	namespace
	{
		FAurora EvalAurora(const FInputs& In, const FSite& Site)
		{
			FAurora A;
			if (In.Aurora <= 0.f)
			{
				return A;
			}
			double NightStart = 0.0;
			const int64_t N = MagNight(In.Hours, Site.MagMidnightHour, NightStart);
			const double Mlt = std::fmod(In.Hours - Site.MagMidnightHour + 48.0, 24.0);
			double EdgeLat = OvalEdgeLat(In.Kp, Mlt);

			// the substorms of this night (and the tail of the last one of the night before)
			double Add = 0.0, Activity = 0.08, Pulsate = 0.0, Expand = 0.0, Creep = 0.0, Fringe = 0.0;
			double Best = -1.0, BestMin = -999.0, BestSeed = 0.0;
			EAuroraPhase Phase = EAuroraPhase::Quiet;
			for (int Night = -1; Night <= 1; ++Night)
			{
				// Night 1 stands for the forced substorm (tests): one candidate, breakup ForceOnsetMinutes ago
				// a staged substorm stands alone (the night's own would blur what is being looked at)
				const bool bForced = In.ForceOnsetMinutes > -1e8f;
				if ((Night == 1) != bForced)
				{
					continue;
				}
				const double Ns = NightStart + Night * 24.0;
				for (int K = 0; K < (Night == 1 ? 1 : Candidates); ++K)
				{
					double Onset, Amp, On;
					Candidate(N + Night, K, Ns, In.Seed, In.Kp, Onset, Amp, On);
					if (Night == 1)
					{
						Onset = In.Hours - In.ForceOnsetMinutes / 60.0;
						Amp = 1.0;
						On = 1.0;
					}
					if (On <= 0.0)
					{
						continue;
					}
					const double T = (In.Hours - Onset) * 60.0;   // minutes from the breakup
					const double Recov = 25.0 + 20.0 * Hash(In.Seed, (N + Night) * 16 + K, 14);
					double B = 0.0, Act = 0.0, Pul = 0.0, Exp = 0.0, Cr = 0.0, Fr = 0.0;
					EAuroraPhase P = EAuroraPhase::Quiet;
					if (T > -70.0 && T < 0.0)
					{
						// growth: the quiet arc brightens a little and creeps toward the equator
						const double G = Smooth(-70.0, 0.0, T);
						B = 0.25 * G;
						Cr = 1.4 * G;
						Act = 0.12;
						P = EAuroraPhase::Growth;
					}
					else if (T >= 0.0 && T < 12.0)
					{
						// breakup: the arc flares within a minute or two, splits into rays and folds, races about
						// and spreads toward the pole
						B = 1.0 - std::exp(-T / 1.2);
						Cr = 1.4 * (1.0 - Smooth(0.0, 6.0, T));
						Exp = 3.5 * Smooth(0.0, 12.0, T);
						Act = 1.0;
						Fr = 1.0;
						P = EAuroraPhase::Breakup;
					}
					else if (T >= 12.0 && T < 12.0 + 4.0 * Recov)
					{
						// recovery: the display fades to diffuse glow and pulsating patches, the oval shrinks back
						const double D = std::exp(-(T - 12.0) / Recov);
						B = 0.15 + 0.85 * D;
						B *= 1.0 - Smooth(2.5 * Recov, 4.0 * Recov, T - 12.0);
						Exp = 3.5 * D;
						Act = 0.15 + 0.6 * D * D;
						Pul = Smooth(12.0, 25.0, T) * (1.0 - Smooth(2.0 * Recov, 4.0 * Recov, T - 12.0));
						Fr = D * D;
						P = EAuroraPhase::Recovery;
					}
					B *= Amp * On;
					Add += B;
					Activity = std::max(Activity, Act * On);
					Pulsate = std::max(Pulsate, Pul * On);
					Expand = std::max(Expand, Exp * Amp * On);
					Creep = std::max(Creep, Cr * On);
					Fringe = std::max(Fringe, Fr * Amp * On);
					if (B > Best)
					{
						Best = B;
						Phase = B > 0.02 ? P : EAuroraPhase::Quiet;
						BestMin = T;
						BestSeed = double((N + Night) * 16 + K);
					}
				}
			}
			EdgeLat -= Creep;
			A.Phase = Phase;
			A.MinutesFromOnset = float(BestMin);
			A.Seed = float(std::fmod(BestSeed * 0.618034, 97.0));
			// the engine's strength is for the oval's usual place at our latitude; the substorms swing it
			A.Brightness = float(std::min(1.6, In.Aurora * (0.35 + Add)));
			A.EdgeKm = float((EdgeLat - Site.MagLatDeg) * KmPerDeg);
			A.SpreadKm = float(150.0 + 45.0 * In.Kp + Expand * KmPerDeg);
			A.Activity = float(Activity);
			A.Pulsate = float(Pulsate);
			// the high red (630 nm) glow is what a storm shows far south; the bright green curtains hold the lower air
			A.Red = float(Clamp01(0.18 + 0.075 * In.Kp + 0.1 * (1.0 - Activity)));
			A.Fringe = float(Clamp01(Fringe));
			A.LowerKm = float(106.0 - 12.0 * Activity);
			// twilight drowns it: a faint glow from nautical twilight, full from astronomical
			const double Dark = 1.0 - Smooth(-17.0, -8.0, In.SunAltDeg);
			A.Visible = float(A.Brightness * Dark);
			return A;
		}

		FHalo EvalHalo(const FInputs& In)
		{
			FHalo H;
			if (In.Halo <= 0.f)
			{
				return H;
			}
			// the light: the red dwarf by day, Europa by night when it is bright enough
			const bool bSun = In.SunAltDeg > -4.f;
			const bool bMoon = !bSun && In.SunAltDeg < -8.f && In.MoonAltDeg > -3.f && In.MoonPhase > 0.35f;
			if (!bSun && !bMoon)
			{
				return H;
			}
			// the veil drifts over in patches (~30 min), thicker or thinner; too thick and it hides the halo again
			const double Patch = TimeNoise(In.Hours, 0.5, In.Seed, 31);
			const double Thr = 0.95 - In.Halo;
			const double Veil = Smooth(Thr, Thr + 0.1, Patch) * (1.0 - Smooth(0.6, 0.95, In.HighDensity));
			const double Alt = bSun ? In.SunAltDeg : In.MoonAltDeg;
			H.bMoon = bMoon;
			H.Strength = float(Veil * (0.55 + 0.45 * In.Halo) * Smooth(-3.0, 4.0, Alt));
			// plate crystals (sun dogs, the pillar) are in some veils and not others, hour by hour
			const double Plates = TimeNoise(In.Hours, 1.3, In.Seed, 32);
			H.SunDogs = float(H.Strength * Smooth(0.35, 0.7, Plates) * (1.0 - Smooth(28.0, 50.0, Alt)));
			H.Pillar = float(H.Strength * Smooth(0.45, 0.8, Plates) * (1.0 - Smooth(4.0, 9.0, Alt)));
			H.Ring46 = float(H.Strength * 0.35 * Smooth(0.6, 0.85, TimeNoise(In.Hours, 2.0, In.Seed, 33)));
			return H;
		}

		FRainbow EvalRainbow(const FInputs& In)
		{
			FRainbow R;
			if (In.Rainbow <= 0.f)
			{
				return R;
			}
			// the red dwarf is 10 degrees across: part of it is still up with its centre at -5 degrees, and the 42-degree bow
			// (smeared over the disk) can stand until its centre climbs to ~47
			const bool bSun = In.SunAltDeg > -5.f;
			const bool bMoon = In.SunAltDeg < -9.f && In.MoonAltDeg > -3.f && In.MoonPhase > 0.7f;
			if (!bSun && !bMoon)
			{
				return R;
			}
			const double Alt = bSun ? In.SunAltDeg : In.MoonAltDeg;
			// a shower opposite the light lasts a few to twenty minutes
			const double Shower = TimeNoise(In.Hours, 0.2, In.Seed, 41);
			const double Thr = 0.95 - In.Rainbow;
			const double S = Smooth(Thr, Thr + 0.1, Shower);
			R.bMoon = bMoon;
			R.Rain = float(Clamp01(S * (0.6 + 0.4 * In.ShowerCells)));
			R.Strength = float(S * (1.0 - Smooth(38.0, 47.0, Alt)) * Smooth(-5.0, 2.0, Alt));
			return R;
		}

		float EvalNoctilucent(const FInputs& In)
		{
			if (In.Noctilucent <= 0.f)
			{
				return 0.f;
			}
			// they shine while the sun is 6-16 degrees down: lit 83 km up while the ground is dark
			const double Window = Smooth(-17.0, -12.0, In.SunAltDeg) * (1.0 - Smooth(-7.0, -4.0, In.SunAltDeg));
			double Start = 0.0;
			const int64_t N = MagNight(In.Hours, 0.0, Start);
			const double Night = 0.35 + 0.65 * Hash(In.Seed, N, 51);
			return float(In.Noctilucent * Window * Night);
		}

		FFlare EvalFlare(const FInputs& In)
		{
			FFlare F;
			double Starts[8];
			float Peaks[8];
			// flares that may still be shining: they last up to a few hours
			const int Count = Flares(In.Hours - 4.0, In.Hours + 1e-6, In.Seed, Starts, Peaks, 8);
			for (int I = 0; I < Count; ++I)
			{
				const double T = (In.Hours - Starts[I]) * 60.0;
				if (T < 0.0)
				{
					continue;
				}
				const double Peak = Peaks[I];
				const double Rise = 1.5 + 4.0 * Peak;
				const double Tau = 6.0 + 35.0 * Peak;
				const double S = Peak * (T < Rise ? Smooth(0.0, Rise, T) : std::exp(-(T - Rise) / Tau));
				if (S > F.Strength)
				{
					const int64_t K = int64_t(std::floor(Starts[I] * 60.0));
					F.Strength = float(S);
					F.Peak = float(Peak);
					F.Minutes = float(T);
					F.Lat = float((Hash(In.Seed, K, 24) < 0.5 ? -1.0 : 1.0) * (0.15 + 0.45 * Hash(In.Seed, K, 25)));
					F.Lon = float((Hash(In.Seed, K, 26) - 0.5) * 2.0);
				}
			}
			return F;
		}
	}

	FState Evaluate(const FInputs& In, const FSite& Site)
	{
		FState S;
		S.Aurora = EvalAurora(In, Site);
		S.Halo = EvalHalo(In);
		S.Rainbow = EvalRainbow(In);
		S.Noctilucent = EvalNoctilucent(In);
		S.Flare = EvalFlare(In);
		return S;
	}
}
