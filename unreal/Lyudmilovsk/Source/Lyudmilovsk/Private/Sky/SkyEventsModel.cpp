#include "SkyEventsModel.h"

#include <algorithm>
#include <cmath>

namespace LyudSkyEvents
{
	namespace
	{
		const double Pi = 3.14159265358979323846;
		const double D2R = Pi / 180.0;
		const double R2D = 180.0 / Pi;
		double SinD(double A) { return std::sin(A * D2R); }
		double CosD(double A) { return std::cos(A * D2R); }
		double Turn(double A)
		{
			A = std::fmod(A, 360.0);
			return A < 0.0 ? A + 360.0 : A;
		}
		/** -180..180. */
		double Half(double A)
		{
			A = Turn(A);
			return A > 180.0 ? A - 360.0 : A;
		}
		double Obliquity(double Day) { return 23.4393 - 3.563e-7 * Day; }
	}

	// ------------------------------------------------------------------ meteor showers

	const std::vector<FShower>& Showers()
	{
		// the maxima and rates of the working list of visual showers (IMO); the slopes are the rounded ones of the same list
		static const std::vector<FShower> All = {
			{ "quadrantids", 283.15, 110.0, 2.5, 230.0, 49.0 },      // Jan 3-4: a few hours of it
			{ "lyrids", 32.32, 18.0, 0.22, 271.0, 34.0 },            // Apr 22
			{ "eta_aquariids", 45.5, 50.0, 0.08, 338.0, -1.0 },      // May 6: low for us, the radiant rises before dawn
			{ "perseids", 140.0, 100.0, 0.2, 48.0, 58.0 },           // Aug 12-13
			{ "orionids", 208.0, 20.0, 0.12, 95.0, 16.0 },           // Oct 21
			{ "leonids", 235.27, 15.0, 0.39, 152.0, 22.0 },          // Nov 17
			{ "geminids", 262.2, 150.0, 0.59, 112.0, 33.0 },         // Dec 14
			{ "ursids", 270.7, 10.0, 0.61, 217.0, 76.0 },            // Dec 22
		};
		return All;
	}

	double SunLongitude(const LyudScope::FSky& S)
	{
		const double Eps = Obliquity(S.Day);
		const double Y = SinD(S.SunRaDeg) * CosD(Eps) + std::tan(S.SunDecDeg * D2R) * SinD(Eps);
		return Turn(std::atan2(Y, CosD(S.SunRaDeg)) * R2D);
	}

	double ZenithRate(const FShower& Shower, double SunLonDeg)
	{
		const double Off = std::fabs(Half(SunLonDeg - Shower.PeakSunLon));
		const double Rate = Shower.Zhr * std::pow(10.0, -Shower.Slope * Off);
		return Rate < 1.0 ? 0.0 : Rate;
	}

	FMeteors Meteors(const LyudScope::FSky& S, double MoonLit, double MoonAltDeg, double CloudCover)
	{
		FMeteors Out;
		// the faintest star: 6.5 in a dark country night, gone by civil twilight; a full moon high up costs two magnitudes
		const double Dark = std::clamp((-6.0 - S.SunAltDeg) / 12.0, 0.0, 1.0);
		const double MoonUp = std::clamp((MoonAltDeg + 2.0) / 20.0, 0.0, 1.0);
		Out.LimitMagnitude = 2.0 + 4.5 * Dark - 2.0 * std::clamp(MoonLit, 0.0, 1.0) * MoonUp * Dark;
		const double Clear = 1.0 - std::clamp(CloudCover, 0.0, 1.0);
		const double SunLon = SunLongitude(S);
		const std::vector<FShower>& All = Showers();
		double Best = 0.0;
		for (size_t I = 0; I < All.size(); ++I)
		{
			const double Zhr = ZenithRate(All[I], SunLon);
			if (Zhr <= 0.0)
			{
				continue;
			}
			double Alt = 0.0, Az = 0.0;
			LyudScope::ToHorizontal(S, All[I].RadiantRaDeg, All[I].RadiantDecDeg, Alt, Az);
			// the standard correction: the radiant's height, and a population index of 2.5 a magnitude of sky lost
			const double Rate = Alt > 0.0 ? Zhr * SinD(Alt) * std::pow(2.5, Out.LimitMagnitude - 6.5) * Clear : 0.0;
			if (Out.Shower < 0 || Rate > Best)
			{
				Best = Rate;
				Out.Shower = static_cast<int>(I);
				Out.RadiantAltDeg = Alt;
				Out.RadiantAzDeg = Az;
				Out.ShowerPerHour = Rate;
			}
		}
		// the sporadic ones: more toward dawn (we face the way the earth goes), about ten an hour then in a perfect sky
		const double LocalHour = std::fmod(std::fmod(S.Day, 1.0) * 24.0 + S.LonDeg / 15.0 + 48.0, 24.0);
		const double TowardDawn = 0.5 - 0.5 * std::cos((LocalHour - 18.0) / 24.0 * 2.0 * Pi);
		Out.SporadicPerHour = Dark <= 0.0 ? 0.0 : (4.0 + 8.0 * TowardDawn) * std::pow(3.0, Out.LimitMagnitude - 6.5) * Clear;
		if (Dark <= 0.0)
		{
			Out.ShowerPerHour = 0.0;
		}
		return Out;
	}

	// ------------------------------------------------------------------ the real moon

	FMoon Moon(double Day)
	{
		// Schlyter's elements with the larger perturbations (the same day number and method as the planets' model)
		const double N = Turn(125.1228 - 0.0529538083 * Day);
		const double Incl = 5.1454;
		const double W = Turn(318.0634 + 0.1643573223 * Day);
		const double Ecc = 0.054900;
		const double Mm = Turn(115.3654 + 13.0649929509 * Day);
		const double Ws = Turn(282.9404 + 4.70935e-5 * Day);
		const double Ms = Turn(356.0470 + 0.9856002585 * Day);
		double E = Mm + R2D * Ecc * SinD(Mm) * (1.0 + Ecc * CosD(Mm));
		for (int I = 0; I < 6; ++I)
		{
			E = E - (E - R2D * Ecc * SinD(E) - Mm) / (1.0 - Ecc * CosD(E));
		}
		const double Xv = 60.2666 * (CosD(E) - Ecc);
		const double Yv = 60.2666 * std::sqrt(1.0 - Ecc * Ecc) * SinD(E);
		const double V = std::atan2(Yv, Xv) * R2D;
		const double R = std::sqrt(Xv * Xv + Yv * Yv);
		const double Xh = R * (CosD(N) * CosD(V + W) - SinD(N) * SinD(V + W) * CosD(Incl));
		const double Yh = R * (SinD(N) * CosD(V + W) + CosD(N) * SinD(V + W) * CosD(Incl));
		const double Zh = R * SinD(V + W) * SinD(Incl);
		double Lon = std::atan2(Yh, Xh) * R2D;
		double Lat = std::atan2(Zh, std::sqrt(Xh * Xh + Yh * Yh)) * R2D;
		const double Ls = Ws + Ms;
		const double Lm = N + W + Mm;
		const double D = Lm - Ls;
		const double F = Lm - N;
		Lon += -1.274 * SinD(Mm - 2 * D) + 0.658 * SinD(2 * D) - 0.186 * SinD(Ms) - 0.059 * SinD(2 * Mm - 2 * D)
			- 0.057 * SinD(Mm - 2 * D + Ms) + 0.053 * SinD(Mm + 2 * D) + 0.046 * SinD(2 * D - Ms) + 0.041 * SinD(Mm - Ms)
			- 0.035 * SinD(D) - 0.031 * SinD(Mm + Ms) - 0.015 * SinD(2 * F - 2 * D) + 0.011 * SinD(Mm - 4 * D);
		Lat += -0.173 * SinD(F - 2 * D) - 0.055 * SinD(Mm - F - 2 * D) - 0.046 * SinD(Mm + F - 2 * D) + 0.033 * SinD(F + 2 * D)
			+ 0.017 * SinD(2 * Mm + F);
		// the sun's true longitude
		const double SunLon = Ls + 1.915 * SinD(Ms) + 0.020 * SinD(2 * Ms);
		FMoon Out;
		Out.LonDeg = Turn(Lon);
		Out.LatDeg = Lat;
		const double Along = Turn(Lon - SunLon);
		Out.ElongationDeg = std::acos(std::clamp(CosD(Lat) * CosD(Along), -1.0, 1.0)) * R2D;
		Out.Lit = 0.5 * (1.0 - CosD(Out.ElongationDeg));
		Out.bWaxing = Along < 180.0;
		Out.AgeDays = Along / 360.0 * 29.530588;
		// the optical libration (Meeus, ch. 53): the moon's equator leans 1.54 degrees to the ecliptic
		const double I0 = 1.54242;
		const double Wl = Lon - N;
		const double A = std::atan2(SinD(Wl) * CosD(Lat) * CosD(I0) - SinD(Lat) * SinD(I0), CosD(Wl) * CosD(Lat)) * R2D;
		Out.LibrationLonDeg = Half(A - F);
		Out.LibrationLatDeg = std::asin(std::clamp(-SinD(Wl) * CosD(Lat) * SinD(I0) - SinD(Lat) * CosD(I0), -1.0, 1.0)) * R2D;
		return Out;
	}

	const char* PhaseId(const FMoon& M)
	{
		if (M.Lit < 0.02) { return "new"; }
		if (M.Lit > 0.98) { return "full"; }
		if (M.Lit > 0.44 && M.Lit < 0.56) { return M.bWaxing ? "first_quarter" : "last_quarter"; }
		if (M.Lit <= 0.44) { return M.bWaxing ? "waxing_crescent" : "waning_crescent"; }
		return M.bWaxing ? "waxing_gibbous" : "waning_gibbous";
	}

	double NextPhase(double Day, bool bFull)
	{
		// how far along the month (0 new, 180 full), walked forward then closed in on by halving
		auto Along = [bFull](double At)
		{
			const FMoon M = Moon(At);
			const double A = M.bWaxing ? M.ElongationDeg : 360.0 - M.ElongationDeg;
			return Half(A - (bFull ? 180.0 : 0.0));
		};
		double Lo = Day;
		double Hi = Day + 0.25;
		for (int I = 0; I < 140; ++I, Lo = Hi, Hi += 0.25)
		{
			const double A = Along(Lo), B = Along(Hi);
			if (A < 0.0 && B >= 0.0 && B - A < 90.0)
			{
				break;
			}
		}
		for (int I = 0; I < 30; ++I)
		{
			const double Mid = 0.5 * (Lo + Hi);
			(Along(Mid) < 0.0 ? Lo : Hi) = Mid;
		}
		return 0.5 * (Lo + Hi);
	}

	// ------------------------------------------------------------------ satellites

	const std::vector<FSatellite>& Satellites()
	{
		static const std::vector<FSatellite> All = {
			{ "mir", 390.0, 51.6, 40.0, 0.0, -2.5 },
			{ "meteor", 830.0, 98.7, 200.0, 120.0, 2.5 },
			{ "zarya", 560.0, 65.0, 310.0, 250.0, 3.5 },
		};
		return All;
	}

	FSatNow Where(const FSatellite& Sat, const LyudScope::FSky& S)
	{
		const double Re = 6378.137, Mu = 398600.4418;
		const double R = Re + Sat.HeightKm;
		// a circular orbit whose plane turns by the earth's bulge (J2)
		const double PerDay = std::sqrt(Mu / (R * R * R)) * 86400.0 * R2D;
		const double NodeRate = -9.9639 * std::pow(Re / R, 3.5) * CosD(Sat.InclinationDeg);
		const double U = Sat.PhaseDeg + PerDay * S.Day;
		const double Node = Sat.NodeDeg + NodeRate * S.Day;
		const double Sx = R * (CosD(Node) * CosD(U) - SinD(Node) * SinD(U) * CosD(Sat.InclinationDeg));
		const double Sy = R * (SinD(Node) * CosD(U) + CosD(Node) * SinD(U) * CosD(Sat.InclinationDeg));
		const double Sz = R * SinD(U) * SinD(Sat.InclinationDeg);
		// us, in the same frame: the sidereal time is our right ascension
		const double Ux = CosD(S.LatDeg) * CosD(S.LstDeg), Uy = CosD(S.LatDeg) * SinD(S.LstDeg), Uz = SinD(S.LatDeg);
		const double Dx = Sx - Re * Ux, Dy = Sy - Re * Uy, Dz = Sz - Re * Uz;
		FSatNow Out;
		Out.RangeKm = std::sqrt(Dx * Dx + Dy * Dy + Dz * Dz);
		Out.AltDeg = std::asin((Dx * Ux + Dy * Uy + Dz * Uz) / Out.RangeKm) * R2D;
		// east and north at our place
		const double Ex = -SinD(S.LstDeg), Ey = CosD(S.LstDeg);
		const double Nx = -SinD(S.LatDeg) * CosD(S.LstDeg), Ny = -SinD(S.LatDeg) * SinD(S.LstDeg), Nz = CosD(S.LatDeg);
		Out.AzDeg = Turn(std::atan2(Dx * Ex + Dy * Ey, Dx * Nx + Dy * Ny + Dz * Nz) * R2D);
		// in the sun unless it is behind the earth's cylinder of shadow
		const double Hx = CosD(S.SunDecDeg) * CosD(S.SunRaDeg), Hy = CosD(S.SunDecDeg) * SinD(S.SunRaDeg), Hz = SinD(S.SunDecDeg);
		const double AlongSun = Sx * Hx + Sy * Hy + Sz * Hz;
		const double Off = std::sqrt(std::max(0.0, R * R - AlongSun * AlongSun));
		Out.bSunlit = AlongSun > 0.0 || Off > Re;
		Out.bSeen = Out.bSunlit && Out.AltDeg > 10.0 && S.SunAltDeg < -6.0;
		// fainter with the square of the distance
		Out.Magnitude = Sat.Magnitude + 5.0 * std::log10(std::max(1.0, Out.RangeKm / Sat.HeightKm));
		return Out;
	}

	std::vector<FPass> Passes(double DayFrom, double DayTo, double LatDeg, double LonDeg)
	{
		std::vector<FPass> Out;
		const std::vector<FSatellite>& All = Satellites();
		const double Step = 20.0 / 86400.0;
		std::vector<bool> bIn(All.size(), false);
		std::vector<FPass> Open(All.size());
		std::vector<bool> bLastSunlit(All.size(), true);
		for (double Day = DayFrom; Day < DayTo; Day += Step)
		{
			const LyudScope::FSky S = LyudScope::Sky(Day, LatDeg, LonDeg);
			if (S.SunAltDeg > -5.0)
			{
				// broad day: nothing to see; coarse steps until dusk
				Day += 9.0 * Step;
				for (size_t I = 0; I < All.size(); ++I)
				{
					if (bIn[I])
					{
						bIn[I] = false;
						Out.push_back(Open[I]);
					}
				}
				continue;
			}
			for (size_t I = 0; I < All.size(); ++I)
			{
				const FSatNow W = Where(All[I], S);
				if (W.bSeen)
				{
					if (!bIn[I])
					{
						bIn[I] = true;
						Open[I] = FPass();
						Open[I].Satellite = static_cast<int>(I);
						Open[I].From = Day;
						Open[I].FromAzDeg = W.AzDeg;
					}
					Open[I].To = Day;
					Open[I].ToAzDeg = W.AzDeg;
					if (W.AltDeg > Open[I].PeakAltDeg)
					{
						Open[I].PeakAltDeg = W.AltDeg;
						Open[I].PeakAzDeg = W.AzDeg;
						Open[I].PeakDay = Day;
					}
				}
				else if (bIn[I])
				{
					bIn[I] = false;
					Open[I].bFades = !W.bSunlit && W.AltDeg > 10.0;
					Out.push_back(Open[I]);
				}
			}
		}
		for (size_t I = 0; I < All.size(); ++I)
		{
			if (bIn[I])
			{
				Out.push_back(Open[I]);
			}
		}
		std::sort(Out.begin(), Out.end(), [](const FPass& A, const FPass& B) { return A.From < B.From; });
		return Out;
	}

	// ------------------------------------------------------------------ planets side by side

	std::vector<FConjunction> Conjunctions(double DayFrom, double DayTo, double LatDeg, double LonDeg, double LimitDeg)
	{
		using LyudScope::EBody;
		const EBody Planets[] = { EBody::Venus, EBody::Mars, EBody::Jupiter, EBody::Saturn };
		std::vector<FConjunction> Out;
		for (int A = 0; A < 4; ++A)
		{
			for (int B = A + 1; B < 4; ++B)
			{
				bool bIn = false;
				FConjunction Cur;
				// every 20 minutes of the dark hours of every night
				for (double Day = DayFrom; Day < DayTo; Day += 1.0 / 72.0)
				{
					const LyudScope::FSky S = LyudScope::Sky(Day, LatDeg, LonDeg);
					const LyudScope::FBody& Pa = S.Of(Planets[A]);
					const LyudScope::FBody& Pb = S.Of(Planets[B]);
					const bool bDark = S.SunAltDeg < -6.0 && Pa.AltDeg > 5.0 && Pb.AltDeg > 5.0;
					const double Sep = bDark ? LyudScope::Separation(Pa.AltDeg, Pa.AzDeg, Pb.AltDeg, Pb.AzDeg) : 999.0;
					if (Sep < LimitDeg)
					{
						if (!bIn)
						{
							bIn = true;
							Cur = FConjunction();
							Cur.A = Planets[A];
							Cur.B = Planets[B];
							Cur.From = Day;
							Cur.SeparationDeg = 999.0;
						}
						Cur.To = Day;
						if (Sep < Cur.SeparationDeg)
						{
							Cur.SeparationDeg = Sep;
							Cur.Day = Day;
							Cur.AltDeg = std::min(Pa.AltDeg, Pb.AltDeg);
							const double LocalHour = std::fmod(std::fmod(Day, 1.0) * 24.0 + LonDeg / 15.0 + 48.0, 24.0);
							Cur.bEvening = LocalHour >= 12.0;
						}
					}
					else if (bIn && Day - Cur.To > 3.0)
					{
						// three nights apart: it is over
						bIn = false;
						Out.push_back(Cur);
					}
				}
				if (bIn)
				{
					Out.push_back(Cur);
				}
			}
		}
		std::sort(Out.begin(), Out.end(), [](const FConjunction& X, const FConjunction& Y) { return X.Day < Y.Day; });
		return Out;
	}
}
