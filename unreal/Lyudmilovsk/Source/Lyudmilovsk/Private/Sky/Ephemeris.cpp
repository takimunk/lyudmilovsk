#include "Sky/Ephemeris.h"

namespace
{
	constexpr double Rad = PI / 180.0;

	double Rev(double Deg)
	{
		return Deg - FMath::FloorToDouble(Deg / 360.0) * 360.0;
	}

	double SinD(double D) { return FMath::Sin(D * Rad); }
	double CosD(double D) { return FMath::Cos(D * Rad); }
	double Atan2D(double Y, double X) { return FMath::Atan2(Y, X) / Rad; }

	double Obliquity(double D)
	{
		return 23.4393 - 3.563e-7 * D;
	}

	/** Sun's mean anomaly and argument of perihelion, degrees. */
	void SunElements(double D, double& M, double& W)
	{
		W = 282.9404 + 4.70935e-5 * D;
		M = Rev(356.0470 + 0.9856002585 * D);
	}

	/** Sun's ecliptic longitude, degrees (its latitude is 0). */
	double SunLongitude(double D)
	{
		double M, W;
		SunElements(D, M, W);
		const double E0 = 0.016709 - 1.151e-9 * D;
		const double E = M + E0 / Rad * SinD(M) * (1.0 + E0 * CosD(M));
		const double Xv = CosD(E) - E0;
		const double Yv = FMath::Sqrt(1.0 - E0 * E0) * SinD(E);
		return Rev(Atan2D(Yv, Xv) + W);
	}

	/** Local sidereal time, degrees. */
	double SiderealTime(double D, double LongitudeDeg, const FDateTime& Utc)
	{
		double M, W;
		SunElements(D, M, W);
		const double Ut = Utc.GetTimeOfDay().GetTotalHours();
		return Rev(M + W + 180.0 + Ut * 15.0 + LongitudeDeg);
	}

	/** Ecliptic (longitude, latitude) to right ascension and declination, degrees. */
	void RaDec(double Lon, double Lat, double D, double& Ra, double& Dec)
	{
		const double Ecl = Obliquity(D);
		const double Xe = CosD(Lon) * CosD(Lat);
		const double Y0 = SinD(Lon) * CosD(Lat);
		const double Z0 = SinD(Lat);
		const double Ye = Y0 * CosD(Ecl) - Z0 * SinD(Ecl);
		const double Ze = Y0 * SinD(Ecl) + Z0 * CosD(Ecl);
		Ra = Atan2D(Ye, Xe);
		Dec = Atan2D(Ze, FMath::Sqrt(Xe * Xe + Ye * Ye));
	}

	/** Ecliptic (longitude, latitude) to the horizon of an observer. */
	FSkyPosition Horizon(double Lon, double Lat, double D, double LstDeg, double LatitudeDeg)
	{
		double Ra, Dec;
		RaDec(Lon, Lat, D, Ra, Dec);
		const double Ha = LstDeg - Ra;

		const double X = CosD(Ha) * CosD(Dec);
		const double Y = SinD(Ha) * CosD(Dec);
		const double Z = SinD(Dec);
		const double Xh = X * SinD(LatitudeDeg) - Z * CosD(LatitudeDeg);
		const double Zh = X * CosD(LatitudeDeg) + Z * SinD(LatitudeDeg);
		FSkyPosition P;
		P.AzimuthDeg = Rev(Atan2D(Y, Xh) + 180.0);
		P.AltitudeDeg = Atan2D(Zh, FMath::Sqrt(Xh * Xh + Y * Y));
		return P;
	}
}

FVector FSkyPosition::EastNorthUp() const
{
	const double CA = CosD(AltitudeDeg);
	return FVector(SinD(AzimuthDeg) * CA, CosD(AzimuthDeg) * CA, SinD(AltitudeDeg));
}

double LyudEphemeris::DayNumber(const FDateTime& Utc)
{
	// FDateTime's Julian day is at noon; Schlyter's day 0.0 is 1999-12-31 00:00 UT = JD 2451543.5
	return Utc.GetJulianDay() - 2451543.5;
}

FSkyPosition LyudEphemeris::Sun(const FDateTime& Utc, double LatitudeDeg, double LongitudeDeg)
{
	const double D = DayNumber(Utc);
	return Horizon(SunLongitude(D), 0.0, D, SiderealTime(D, LongitudeDeg, Utc), LatitudeDeg);
}

namespace
{
	/** The moon's geocentric ecliptic longitude and latitude (degrees) and distance (Earth radii) on day D. */
	void MoonEcliptic(double D, double& Lon, double& Lat, double& R)
	{
		const double N = Rev(125.1228 - 0.0529538083 * D);
		const double I = 5.1454;
		const double W = Rev(318.0634 + 0.1643573223 * D);
		const double A = 60.2666;   // Earth radii
		const double E0 = 0.054900;
		const double M = Rev(115.3654 + 13.0649929509 * D);

		double E = M + E0 / Rad * SinD(M) * (1.0 + E0 * CosD(M));
		for (int32 It = 0; It < 4; ++It)
		{
			E = E - (E - E0 / Rad * SinD(E) - M) / (1.0 - E0 * CosD(E));
		}
		const double Xv = A * (CosD(E) - E0);
		const double Yv = A * FMath::Sqrt(1.0 - E0 * E0) * SinD(E);
		const double V = Atan2D(Yv, Xv);
		R = FMath::Sqrt(Xv * Xv + Yv * Yv);
		const double Xh = R * (CosD(N) * CosD(V + W) - SinD(N) * SinD(V + W) * CosD(I));
		const double Yh = R * (SinD(N) * CosD(V + W) + CosD(N) * SinD(V + W) * CosD(I));
		const double Zh = R * SinD(V + W) * SinD(I);
		Lon = Atan2D(Yh, Xh);
		Lat = Atan2D(Zh, FMath::Sqrt(Xh * Xh + Yh * Yh));

		// the largest perturbations by the sun
		double Ms, Ws;
		SunElements(D, Ms, Ws);
		const double Ls = Ms + Ws;
		const double Lm = M + W + N;
		const double Dm = Lm - Ls;
		const double F = Lm - N;
		Lon += -1.274 * SinD(M - 2 * Dm) + 0.658 * SinD(2 * Dm) - 0.186 * SinD(Ms) - 0.059 * SinD(2 * M - 2 * Dm)
			- 0.057 * SinD(M - 2 * Dm + Ms) + 0.053 * SinD(M + 2 * Dm) + 0.046 * SinD(2 * Dm - Ms) + 0.041 * SinD(M - Ms)
			- 0.035 * SinD(Dm) - 0.031 * SinD(M + Ms) - 0.015 * SinD(2 * F - 2 * Dm) + 0.011 * SinD(M - 4 * Dm);
		Lat += -0.173 * SinD(F - 2 * Dm) - 0.055 * SinD(M - F - 2 * Dm) - 0.046 * SinD(M + F - 2 * Dm)
			+ 0.033 * SinD(F + 2 * Dm) + 0.017 * SinD(2 * M + F);
	}
}

FSkyPosition LyudEphemeris::Moon(const FDateTime& Utc, double LatitudeDeg, double LongitudeDeg, double* PhaseFraction)
{
	const double D = DayNumber(Utc);
	double Lon, Lat, R;
	MoonEcliptic(D, Lon, Lat, R);
	FSkyPosition P = Horizon(Lon, Lat, D, SiderealTime(D, LongitudeDeg, Utc), LatitudeDeg);
	// seen from the surface, not the Earth's centre: the moon sits up to a degree lower
	P.AltitudeDeg -= FMath::Asin(1.0 / R) / Rad * CosD(P.AltitudeDeg);

	if (PhaseFraction)
	{
		const double CosElong = CosD(SunLongitude(D) - Lon) * CosD(Lat);
		*PhaseFraction = 0.5 * (1.0 - CosElong);
	}
	return P;
}

void LyudEphemeris::HourAngles(const FDateTime& Utc, double LongitudeDeg, double& SunHaDeg, double& MoonHaDeg)
{
	const double D = DayNumber(Utc);
	const double Lst = SiderealTime(D, LongitudeDeg, Utc);
	double Ra, Dec, Lon, Lat, R;
	RaDec(SunLongitude(D), 0.0, D, Ra, Dec);
	SunHaDeg = Rev(Lst - Ra);
	MoonEcliptic(D, Lon, Lat, R);
	RaDec(Lon, Lat, D, Ra, Dec);
	MoonHaDeg = Rev(Lst - Ra);
}
