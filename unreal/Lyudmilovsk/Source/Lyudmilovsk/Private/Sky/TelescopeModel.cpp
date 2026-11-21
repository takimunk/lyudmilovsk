#include "TelescopeModel.h"

#include <algorithm>
#include <cmath>

namespace LyudScope
{
	namespace
	{
		const double Pi = 3.14159265358979323846;
		const double D2R = Pi / 180.0;
		const double R2D = 180.0 / Pi;

		double Sind(double A) { return std::sin(A * D2R); }
		double Cosd(double A) { return std::cos(A * D2R); }
		double Atan2d(double Y, double X) { return std::atan2(Y, X) * R2D; }
		double Asind(double X) { return std::asin(std::max(-1.0, std::min(1.0, X))) * R2D; }
		double Acosd(double X) { return std::acos(std::max(-1.0, std::min(1.0, X))) * R2D; }

		double Wrap360(double A)
		{
			A = std::fmod(A, 360.0);
			return A < 0.0 ? A + 360.0 : A;
		}

		/** -180..180. */
		double Wrap180(double A)
		{
			A = Wrap360(A);
			return A > 180.0 ? A - 360.0 : A;
		}

		double Clamp01(double X) { return X < 0.0 ? 0.0 : (X > 1.0 ? 1.0 : X); }

		double Smooth(double A, double B, double X)
		{
			const double T = Clamp01((X - A) / (B - A));
			return T * T * (3.0 - 2.0 * T);
		}

		uint32_t Hash(uint32_t A, uint32_t B, uint32_t C)
		{
			uint32_t H = A * 0x9E3779B1u ^ (B + 0x7F4A7C15u) * 0x85EBCA6Bu ^ (C + 0x165667B1u) * 0xC2B2AE35u;
			H ^= H >> 15;
			H *= 0x2C1B3C6Du;
			H ^= H >> 12;
			H *= 0x297A2D39u;
			H ^= H >> 15;
			return H;
		}

		double Unit(uint32_t H) { return (H & 0xFFFFFFu) / double(0x1000000); }

		/** Smooth value noise 0..1. */
		double Noise2(double X, double Y, uint32_t Seed)
		{
			const double Fx = std::floor(X), Fy = std::floor(Y);
			const int32_t Ix = static_cast<int32_t>(Fx), Iy = static_cast<int32_t>(Fy);
			double Tx = X - Fx, Ty = Y - Fy;
			Tx = Tx * Tx * (3.0 - 2.0 * Tx);
			Ty = Ty * Ty * (3.0 - 2.0 * Ty);
			const double A = Unit(Hash(static_cast<uint32_t>(Ix), static_cast<uint32_t>(Iy), Seed));
			const double B = Unit(Hash(static_cast<uint32_t>(Ix + 1), static_cast<uint32_t>(Iy), Seed));
			const double C = Unit(Hash(static_cast<uint32_t>(Ix), static_cast<uint32_t>(Iy + 1), Seed));
			const double D = Unit(Hash(static_cast<uint32_t>(Ix + 1), static_cast<uint32_t>(Iy + 1), Seed));
			return (A + (B - A) * Tx) + ((C + (D - C) * Tx) - (A + (B - A) * Tx)) * Ty;
		}

		struct FElements
		{
			double N0, N1, I0, I1, W0, W1, A, E0, E1, M0, M1;
		};

		// Schlyter's elements: value at d = 0 and the change a day
		const FElements SunEl = { 0.0, 0.0, 0.0, 0.0, 282.9404, 4.70935e-5, 1.0, 0.016709, -1.151e-9, 356.0470, 0.9856002585 };
		const FElements VenusEl = { 76.6799, 2.46590e-5, 3.3946, 2.75e-8, 54.8910, 1.38374e-5, 0.723330, 0.006773, -1.302e-9, 48.0052, 1.6021302244 };
		const FElements MarsEl = { 49.5574, 2.11081e-5, 1.8497, -1.78e-8, 286.5016, 2.92961e-5, 1.523688, 0.093405, 2.516e-9, 18.6021, 0.5240207766 };
		const FElements JupiterEl = { 100.4542, 2.76854e-5, 1.3030, -1.557e-7, 273.8777, 1.64505e-5, 5.20256, 0.048498, 4.469e-9, 19.8950, 0.0830853001 };
		const FElements SaturnEl = { 113.6634, 2.38980e-5, 2.4886, -1.081e-7, 339.3939, 2.97661e-5, 9.55475, 0.055546, -9.499e-9, 316.9670, 0.0334442282 };

		struct FHelio
		{
			double X = 0.0, Y = 0.0, Z = 0.0;
		};

		/** Heliocentric ecliptic place of a planet (the sun's elements give the earth's orbit as seen from the earth). */
		FHelio Orbit(const FElements& El, double Day, double* OutMeanAnomaly = nullptr)
		{
			const double N = El.N0 + El.N1 * Day;
			const double Inc = El.I0 + El.I1 * Day;
			const double W = El.W0 + El.W1 * Day;
			const double Ecc = El.E0 + El.E1 * Day;
			const double M = Wrap360(El.M0 + El.M1 * Day);
			if (OutMeanAnomaly)
			{
				*OutMeanAnomaly = M;
			}
			double E = M + R2D * Ecc * Sind(M) * (1.0 + Ecc * Cosd(M));
			for (int Step = 0; Step < 8; ++Step)
			{
				const double Next = E - (E - R2D * Ecc * Sind(E) - M) / (1.0 - Ecc * Cosd(E));
				const bool bDone = std::fabs(Next - E) < 1e-7;
				E = Next;
				if (bDone)
				{
					break;
				}
			}
			const double Xv = El.A * (Cosd(E) - Ecc);
			const double Yv = El.A * std::sqrt(1.0 - Ecc * Ecc) * Sind(E);
			const double V = Atan2d(Yv, Xv);
			const double R = std::sqrt(Xv * Xv + Yv * Yv);
			FHelio H;
			H.X = R * (Cosd(N) * Cosd(V + W) - Sind(N) * Sind(V + W) * Cosd(Inc));
			H.Y = R * (Sind(N) * Cosd(V + W) + Cosd(N) * Sind(V + W) * Cosd(Inc));
			H.Z = R * Sind(V + W) * Sind(Inc);
			return H;
		}

		/** Unit vector toward (alt, az) in east, north, up. */
		void Enu(double AltDeg, double AzDeg, double& E, double& N, double& U)
		{
			E = Sind(AzDeg) * Cosd(AltDeg);
			N = Cosd(AzDeg) * Cosd(AltDeg);
			U = Sind(AltDeg);
		}
	}

	const char* BodyId(EBody Body)
	{
		switch (Body)
		{
		case EBody::Moon: return "moon";
		case EBody::Venus: return "venus";
		case EBody::Mars: return "mars";
		case EBody::Jupiter: return "jupiter";
		case EBody::Saturn: return "saturn";
		default: return "";
		}
	}

	double DayNumber(int Year, int Month, int DayOfMonth, double UtHours)
	{
		const int D = 367 * Year - 7 * (Year + (Month + 9) / 12) / 4 + 275 * Month / 9 + DayOfMonth - 730530;
		return D + UtHours / 24.0;
	}

	double Separation(double AltA, double AzA, double AltB, double AzB)
	{
		double Ea, Na, Ua, Eb, Nb, Ub;
		Enu(AltA, AzA, Ea, Na, Ua);
		Enu(AltB, AzB, Eb, Nb, Ub);
		return Acosd(Ea * Eb + Na * Nb + Ua * Ub);
	}

	void ToHorizontal(const FSky& S, double RaDeg, double DecDeg, double& AltDeg, double& AzDeg)
	{
		const double Ha = S.LstDeg - RaDeg;
		const double X = Cosd(Ha) * Cosd(DecDeg);
		const double Y = Sind(Ha) * Cosd(DecDeg);
		const double Z = Sind(DecDeg);
		const double Xh = X * Sind(S.LatDeg) - Z * Cosd(S.LatDeg);
		const double Zh = X * Cosd(S.LatDeg) + Z * Sind(S.LatDeg);
		AzDeg = Wrap360(Atan2d(Y, Xh) + 180.0);
		AltDeg = Asind(Zh);
	}

	void ToEquatorial(const FSky& S, double AltDeg, double AzDeg, double& RaDeg, double& DecDeg)
	{
		// the inverse of ToHorizontal: (Xh, Y, Zh) from alt and az, then turned back by the latitude
		const double A = AzDeg - 180.0;
		const double Xh = Cosd(A) * Cosd(AltDeg);
		const double Y = Sind(A) * Cosd(AltDeg);
		const double Zh = Sind(AltDeg);
		const double X = Xh * Sind(S.LatDeg) + Zh * Cosd(S.LatDeg);
		const double Z = -Xh * Cosd(S.LatDeg) + Zh * Sind(S.LatDeg);
		const double Ha = Atan2d(Y, X);
		DecDeg = Asind(Z);
		RaDeg = Wrap360(S.LstDeg - Ha);
	}

	bool Project(double AimAltDeg, double AimAzDeg, double AltDeg, double AzDeg, double& X, double& Y)
	{
		double Ae, An, Au, Se, Sn, Su;
		Enu(AimAltDeg, AimAzDeg, Ae, An, Au);
		Enu(AltDeg, AzDeg, Se, Sn, Su);
		const double Fwd = Ae * Se + An * Sn + Au * Su;
		if (Fwd <= 0.05)
		{
			X = Y = 0.0;
			return false;
		}
		// right: toward growing azimuth; up: toward growing altitude
		const double Re = Cosd(AimAzDeg), Rn = -Sind(AimAzDeg);
		const double Ue = -Sind(AimAzDeg) * Sind(AimAltDeg), Un = -Cosd(AimAzDeg) * Sind(AimAltDeg), Uu = Cosd(AimAltDeg);
		X = (Re * Se + Rn * Sn) / Fwd;
		Y = (Ue * Se + Un * Sn + Uu * Su) / Fwd;
		return true;
	}

	FSky Sky(double Day, double LatDeg, double LonDeg)
	{
		FSky S;
		S.Day = Day;
		S.LatDeg = LatDeg;
		S.LonDeg = LonDeg;
		const double Ecl = 23.4393 - 3.563e-7 * Day;

		// the sun (the earth's orbit seen from the earth)
		double Ms = 0.0;
		const FHelio SunOrbit = Orbit(SunEl, Day, &Ms);
		const double Ws = SunEl.W0 + SunEl.W1 * Day;
		const double Xs = SunOrbit.X, Ys = SunOrbit.Y;     // the sun's inclination and node are zero: this is r cos(lon), r sin(lon)
		S.SunDistanceAu = std::sqrt(Xs * Xs + Ys * Ys);
		S.SunRaDeg = Wrap360(Atan2d(Ys * Cosd(Ecl), Xs));
		S.SunDecDeg = Atan2d(Ys * Sind(Ecl), std::sqrt(Xs * Xs + Ys * Cosd(Ecl) * Ys * Cosd(Ecl)));
		const double Ut = (Day - std::floor(Day)) * 24.0;
		S.LstDeg = Wrap360(Ms + Ws + 180.0 + Ut * 15.0 + LonDeg);
		ToHorizontal(S, S.SunRaDeg, S.SunDecDeg, S.SunAltDeg, S.SunAzDeg);

		double Mj = 0.0, MsatAnomaly = 0.0;
		Orbit(JupiterEl, Day, &Mj);
		Orbit(SaturnEl, Day, &MsatAnomaly);

		const EBody Planets[4] = { EBody::Venus, EBody::Mars, EBody::Jupiter, EBody::Saturn };
		const FElements* Elements[4] = { &VenusEl, &MarsEl, &JupiterEl, &SaturnEl };
		// the disk's diameter at 1 AU (arc seconds) and the magnitude's constant and phase slope
		const double Size1[4] = { 16.92, 9.36, 196.94, 165.6 };
		for (int P = 0; P < 4; ++P)
		{
			FHelio H = Orbit(*Elements[P], Day);
			double R = std::sqrt(H.X * H.X + H.Y * H.Y + H.Z * H.Z);
			if (Planets[P] == EBody::Jupiter || Planets[P] == EBody::Saturn)
			{
				// the two giants pull each other about: the main terms (the "great inequality")
				double Lon = Atan2d(H.Y, H.X);
				double Lat = Atan2d(H.Z, std::sqrt(H.X * H.X + H.Y * H.Y));
				const double J = Mj, T = MsatAnomaly;
				if (Planets[P] == EBody::Jupiter)
				{
					Lon += -0.332 * Sind(2 * J - 5 * T - 67.6) - 0.056 * Sind(2 * J - 2 * T + 21) + 0.042 * Sind(3 * J - 5 * T + 21)
						- 0.036 * Sind(J - 2 * T) + 0.022 * Cosd(J - T) + 0.023 * Sind(2 * J - 3 * T + 52) - 0.016 * Sind(J - 5 * T - 69);
				}
				else
				{
					Lon += 0.812 * Sind(2 * J - 5 * T - 67.6) - 0.229 * Cosd(2 * J - 4 * T - 2) + 0.119 * Sind(J - 2 * T - 3)
						+ 0.046 * Sind(2 * J - 6 * T - 69) + 0.014 * Sind(J - 3 * T + 32);
					Lat += -0.020 * Cosd(2 * J - 4 * T - 2) + 0.018 * Sind(2 * J - 6 * T - 49);
				}
				H.X = R * Cosd(Lon) * Cosd(Lat);
				H.Y = R * Sind(Lon) * Cosd(Lat);
				H.Z = R * Sind(Lat);
			}
			const double Xg = H.X + Xs, Yg = H.Y + Ys, Zg = H.Z;
			const double Dist = std::sqrt(Xg * Xg + Yg * Yg + Zg * Zg);
			const double Xe = Xg;
			const double Ye = Yg * Cosd(Ecl) - Zg * Sind(Ecl);
			const double Ze = Yg * Sind(Ecl) + Zg * Cosd(Ecl);

			FBody& B = S.Of(Planets[P]);
			B.RaDeg = Wrap360(Atan2d(Ye, Xe));
			B.DecDeg = Atan2d(Ze, std::sqrt(Xe * Xe + Ye * Ye));
			ToHorizontal(S, B.RaDeg, B.DecDeg, B.AltDeg, B.AzDeg);
			B.DistanceAu = Dist;
			B.SunDistanceAu = R;
			B.DiameterArcsec = Size1[P] / Dist;
			const double Sd = S.SunDistanceAu;
			B.PhaseAngleDeg = Acosd((R * R + Dist * Dist - Sd * Sd) / (2.0 * R * Dist));
			B.Lit = (1.0 + Cosd(B.PhaseAngleDeg)) * 0.5;
			B.ElongationDeg = Acosd((Sd * Sd + Dist * Dist - R * R) / (2.0 * Sd * Dist));
			const double Fv = B.PhaseAngleDeg;
			const double Base = 5.0 * std::log10(R * Dist);
			switch (Planets[P])
			{
			case EBody::Venus:
				B.Magnitude = -4.34 + Base + 0.013 * Fv + 4.2e-7 * Fv * Fv * Fv;
				break;
			case EBody::Mars:
				B.Magnitude = -1.51 + Base + 0.016 * Fv;
				B.SpinDeg = Wrap360(350.89198 * Day);
				break;
			case EBody::Jupiter:
				B.Magnitude = -9.25 + Base + 0.014 * Fv;
				B.SpinDeg = Wrap360(870.27 * Day);
				break;
			default:
			{
				// the ring's tilt toward us, from Saturn's geocentric ecliptic longitude and latitude
				const double Los = Atan2d(Yg, Xg);
				const double Las = Atan2d(Zg, std::sqrt(Xg * Xg + Yg * Yg));
				const double Ir = 28.06, Nr = 169.51 + 3.82e-5 * Day;
				const double Tilt = Asind(Sind(Las) * Cosd(Ir) - Cosd(Las) * Sind(Ir) * Sind(Los - Nr));
				B.RingTiltDeg = Tilt;
				B.Magnitude = -9.0 + Base + 0.044 * Fv - 2.6 * Sind(std::fabs(Tilt)) + 1.2 * Sind(Tilt) * Sind(Tilt);
				break;
			}
			}
			// the bright limb: the position angle of the sun as seen from the body's place in the sky
			B.BrightLimbDeg = Wrap360(Atan2d(Cosd(S.SunDecDeg) * Sind(S.SunRaDeg - B.RaDeg),
				Sind(S.SunDecDeg) * Cosd(B.DecDeg) - Cosd(S.SunDecDeg) * Sind(B.DecDeg) * Cosd(S.SunRaDeg - B.RaDeg)));
			// celestial north in the eyepiece: where a point just north of the body lands
			double NAlt = 0.0, NAz = 0.0, Nx = 0.0, Ny = 1.0;
			ToHorizontal(S, B.RaDeg, std::min(B.DecDeg + 0.05, 89.9), NAlt, NAz);
			if (Project(B.AltDeg, B.AzDeg, NAlt, NAz, Nx, Ny))
			{
				B.ParallacticDeg = Atan2d(-Nx, Ny);
			}
		}

		// Jupiter's moons (Meeus ch. 44, the low-accuracy method); its day count runs from 2000 January 1.5
		{
			FBody& Jb = S.Of(EBody::Jupiter);
			const double D = Day - 1.5;
			const double V = 172.74 + 0.00111588 * D;
			const double M = 357.529 + 0.9856003 * D;
			const double N = 20.020 + 0.0830853 * D + 0.329 * Sind(V);
			const double J = 66.115 + 0.9025179 * D - 0.329 * Sind(V);
			const double A = 1.915 * Sind(M) + 0.020 * Sind(2 * M);
			const double B = 5.555 * Sind(N) + 0.168 * Sind(2 * N);
			const double K = J + A - B;
			const double Re = 1.00014 - 0.01671 * Cosd(M) - 0.00014 * Cosd(2 * M);
			const double Rj = 5.20872 - 0.25208 * Cosd(N) - 0.00611 * Cosd(2 * N);
			const double Delta = std::sqrt(Rj * Rj + Re * Re - 2.0 * Rj * Re * Cosd(K));
			const double Psi = Asind(Re / Delta * Sind(K));
			const double Lambda = 34.35 + 0.083091 * D + 0.329 * Sind(V) + B;
			const double Ds = 3.12 * Sind(Lambda + 42.8);
			const double De = Ds - 2.22 * Sind(Psi) * Cosd(Lambda + 22.0) - 1.30 * (Rj - Delta) / Delta * Sind(Lambda - 100.5);
			const double Dt = D - Delta / 173.0;
			double U[4] = {
				163.8069 + 203.4058646 * Dt + Psi - B,
				358.4140 + 101.2916335 * Dt + Psi - B,
				5.7176 + 50.2345180 * Dt + Psi - B,
				224.8092 + 21.4879800 * Dt + Psi - B };
			const double G = 331.18 + 50.310482 * Dt;
			const double Hh = 87.45 + 21.569231 * Dt;
			const double Rm[4] = {
				5.9057 - 0.0244 * Cosd(2 * (U[0] - U[1])),
				9.3966 - 0.0882 * Cosd(2 * (U[1] - U[2])),
				14.9883 - 0.0216 * Cosd(G),
				26.3627 - 0.1939 * Cosd(Hh) };
			const double Corr[4] = { 0.473 * Sind(2 * (U[0] - U[1])), 1.065 * Sind(2 * (U[1] - U[2])), 0.165 * Sind(G), 0.843 * Sind(Hh) };
			for (int I = 0; I < 4; ++I)
			{
				U[I] += Corr[I];
				Jb.MoonX[I] = Rm[I] * Sind(U[I]);
				Jb.MoonY[I] = -Rm[I] * Cosd(U[I]) * Sind(De);
				// on the disk (a transit) or behind it: a small telescope shows nothing
				Jb.bMoonSeen[I] = Jb.MoonX[I] * Jb.MoonX[I] + Jb.MoonY[I] * Jb.MoonY[I] > 1.05 * 1.05;
			}
		}
		return S;
	}

	void SetMoon(FSky& S, double AltDeg, double AzDeg, double SizeDeg, double Lit)
	{
		FBody& M = S.Of(EBody::Moon);
		M.AltDeg = AltDeg;
		M.AzDeg = AzDeg;
		ToEquatorial(S, AltDeg, AzDeg, M.RaDeg, M.DecDeg);
		M.DiameterArcsec = SizeDeg * 3600.0;
		M.Lit = Clamp01(Lit);
		M.PhaseAngleDeg = Acosd(2.0 * M.Lit - 1.0);
		M.Magnitude = -12.0;
		M.ElongationDeg = Separation(AltDeg, AzDeg, S.SunAltDeg, S.SunAzDeg);
	}

	// ---------------------------------------------------------------- the instrument

	const FEyepiece& Eyepiece(int Index)
	{
		static const FEyepiece All[NumEyepieces] = { { 3, 15.0 }, { 12, 4.0 }, { 60, 0.8 }, { 400, 0.12 } };
		return All[std::max(0, std::min(NumEyepieces - 1, Index))];
	}

	void StepAim(FAim& Aim, double MouseX, double MouseY, double FieldDeg, double Dt, const FMount& Mount)
	{
		// 700 counts of the mouse carry the view across one field; the push decays over Tau, so the tube glides and settles
		const double Tau = 0.22;
		const double Gain = FieldDeg / 700.0;
		const double CosAlt = std::max(0.2, Cosd(Aim.AltDeg));
		Aim.VAz += MouseX * Gain / (Tau * CosAlt);
		Aim.VAlt += MouseY * Gain / Tau;
		// never faster than three fields a second: a telescope is not swung about
		const double Vmax = FieldDeg * 3.0;
		Aim.VAz = std::max(-Vmax / CosAlt, std::min(Vmax / CosAlt, Aim.VAz));
		Aim.VAlt = std::max(-Vmax, std::min(Vmax, Aim.VAlt));
		Dt = std::max(0.0, std::min(0.1, Dt));
		const double Decay = std::exp(-Dt / Tau);
		// the exact integral of the decaying speed over the step, so the travel does not depend on the frame rate
		const double Travel = Tau * (1.0 - Decay);
		double Az = Aim.AzDeg + Aim.VAz * Travel;
		double Alt = Aim.AltDeg + Aim.VAlt * Travel;
		Aim.VAz *= Decay;
		Aim.VAlt *= Decay;
		if (Alt < Mount.AltMinDeg)
		{
			Alt = Mount.AltMinDeg;
			Aim.VAlt = 0.0;
		}
		else if (Alt > Mount.AltMaxDeg)
		{
			Alt = Mount.AltMaxDeg;
			Aim.VAlt = 0.0;
		}
		if (Mount.AzRangeDeg < 179.9)
		{
			const double Off = Wrap180(Az - Mount.AzCenterDeg);
			if (Off < -Mount.AzRangeDeg)
			{
				Az = Mount.AzCenterDeg - Mount.AzRangeDeg;
				Aim.VAz = 0.0;
			}
			else if (Off > Mount.AzRangeDeg)
			{
				Az = Mount.AzCenterDeg + Mount.AzRangeDeg;
				Aim.VAz = 0.0;
			}
		}
		Aim.AzDeg = Wrap360(Az);
		Aim.AltDeg = Alt;
	}

	double FocusDioptre(double Knob)
	{
		Knob = Clamp01(Knob);
		if (Knob <= InfinityKnob)
		{
			const double T = (InfinityKnob - Knob) / InfinityKnob;
			return 0.05 * T * T * T;
		}
		return -0.004 * (Knob - InfinityKnob) / (1.0 - InfinityKnob);
	}

	double FocusDistanceM(double Knob)
	{
		const double U = FocusDioptre(Knob);
		return U <= 1e-7 ? 1e7 : 1.0 / U;
	}

	double Blur(double Knob, double DistanceM, double FieldDeg)
	{
		const double Target = DistanceM > 0.0 ? 1.0 / DistanceM : 0.0;
		const double BlurDeg = ApertureM * std::fabs(FocusDioptre(Knob) - Target) * R2D;
		return BlurDeg / std::max(FieldDeg, 1e-3);
	}

	// ---------------------------------------------------------------- what is seen

	const char* SightId(ESight Sight)
	{
		switch (Sight)
		{
		case ESight::MoonCrescent: return "moon_crescent";
		case ESight::MoonQuarter: return "moon_quarter";
		case ESight::MoonGibbous: return "moon_gibbous";
		case ESight::MoonFull: return "moon_full";
		case ESight::Venus: return "venus";
		case ESight::VenusPhase: return "venus_phase";
		case ESight::Mars: return "mars";
		case ESight::Jupiter: return "jupiter";
		case ESight::JupiterMoons: return "jupiter_moons";
		case ESight::Saturn: return "saturn";
		case ESight::SaturnRing: return "saturn_ring";
		case ESight::Sunspots: return "sunspots";
		case ESight::Aurora: return "aurora";
		case ESight::Noctilucent: return "noctilucent";
		default: return "";
		}
	}

	double Visibility(const FSky& S, EBody Body)
	{
		const FBody& B = S.Of(Body);
		const FBody& Moon = S.Of(EBody::Moon);
		const double MoonRadius = Moon.DiameterArcsec / 7200.0;
		if (Body == EBody::Moon)
		{
			// the moon shows by day too
			return Smooth(-1.0, 1.5, B.AltDeg + MoonRadius * 0.5);
		}
		if (MoonRadius > 0.0 && Separation(B.AltDeg, B.AzDeg, Moon.AltDeg, Moon.AzDeg) < MoonRadius)
		{
			return 0.0;
		}
		// the brighter the planet, the earlier in the dusk it comes out: Venus with the sun still up, Saturn in deep twilight
		const double Limit = std::max(-9.0, std::min(3.0, -5.0 - 1.6 * B.Magnitude));
		const double Dark = Clamp01((Limit + 1.0 - S.SunAltDeg) / 3.0);
		return Dark * Smooth(1.5, 5.0, B.AltDeg);
	}

	double CloudGap(double CloudCover, double Day, double AltDeg, double AzDeg)
	{
		const double C = Clamp01(CloudCover);
		if (C < 0.05)
		{
			return 1.0;
		}
		// the gaps drift: a new pattern every ten minutes or so, patches some twenty degrees wide
		const double N = Noise2(AzDeg / 22.0 + Day * 144.0, AltDeg / 22.0 + Day * 37.0, 5519u);
		return Smooth(C * 1.25 - 0.25, C * 1.25 + 0.05, N);
	}

	FTarget Target(const FSky& S, const FLook& L)
	{
		FTarget T;
		const FEyepiece& Ep = Eyepiece(L.Eyepiece);
		const double Field = Ep.FieldDeg;
		T.BlurShare = Blur(L.Knob, 0.0, Field);
		const bool bSharp = T.BlurShare < SharpBlur;
		if (L.bBlocked)
		{
			return T;
		}
		const double Gap = CloudGap(L.CloudCover, S.Day, L.Aim.AltDeg, L.Aim.AzDeg);
		if (L.bFilter)
		{
			// the solar filter: only the sun comes through
			const double Sep = Separation(L.Aim.AltDeg, L.Aim.AzDeg, S.SunAltDeg, S.SunAzDeg);
			if (S.SunAltDeg > 1.0 && Sep < L.SunSizeDeg * 0.5 + Field * 0.2)
			{
				T.Off = Sep / Field;
				T.Visible = Gap;
				if (Gap < 0.4)
				{
					T.Hint = 4;
				}
				else if (!bSharp)
				{
					T.Hint = 1;
				}
				else
				{
					T.Sight = ESight::Sunspots;
				}
			}
			return T;
		}
		double Best = 1e9;
		for (int I = 0; I < static_cast<int>(EBody::Num); ++I)
		{
			const EBody Body = static_cast<EBody>(I);
			const FBody& B = S.Bodies[I];
			if (B.DiameterArcsec <= 0.0)
			{
				continue;
			}
			const double Radius = Body == EBody::Moon ? B.DiameterArcsec / 7200.0 : 0.0;
			const double Sep = std::max(0.0, Separation(L.Aim.AltDeg, L.Aim.AzDeg, B.AltDeg, B.AzDeg) - Radius);
			const double Vis = Visibility(S, Body);
			if (Sep < Field * 0.5 && Vis > 0.02 && Sep < Best)
			{
				Best = Sep;
				T.Body = I;
				T.Off = Sep / Field;
				T.Visible = Vis * Gap;
			}
		}
		if (T.Body >= 0)
		{
			const EBody Body = static_cast<EBody>(T.Body);
			const FBody& B = S.Bodies[T.Body];
			if (T.Off > 0.3)
			{
				return T;     // in the field but off the middle: bring it in
			}
			if (Gap < 0.4)
			{
				T.Hint = 4;
				return T;
			}
			if (T.Visible < 0.3)
			{
				T.Hint = 3;
				return T;
			}
			if (!bSharp)
			{
				T.Hint = 1;
				return T;
			}
			switch (Body)
			{
			case EBody::Moon:
				if (B.Lit >= 0.97) { T.Sight = ESight::MoonFull; }
				else if (B.Lit >= 0.65) { T.Sight = ESight::MoonGibbous; }
				else if (B.Lit >= 0.35) { T.Sight = ESight::MoonQuarter; }
				else if (B.Lit >= 0.03) { T.Sight = ESight::MoonCrescent; }
				break;
			case EBody::Venus:
				T.Sight = Ep.Power >= 60 ? ESight::VenusPhase : ESight::Venus;
				T.Hint = Ep.Power >= 60 ? 0 : 2;
				break;
			case EBody::Mars:
				T.Sight = ESight::Mars;
				break;
			case EBody::Jupiter:
			{
				int Seen = 0;
				for (int I = 0; I < 4; ++I)
				{
					Seen += B.bMoonSeen[I] ? 1 : 0;
				}
				// the moons need some power and a darker sky than the planet itself
				const bool bMoons = Ep.Power >= 12 && Seen >= 3 && S.SunAltDeg < -6.0;
				T.Sight = bMoons ? ESight::JupiterMoons : ESight::Jupiter;
				T.Hint = bMoons ? 0 : (Ep.Power < 12 ? 2 : (S.SunAltDeg >= -6.0 ? 3 : 0));
				break;
			}
			case EBody::Saturn:
				T.Sight = Ep.Power >= 60 ? ESight::SaturnRing : ESight::Saturn;
				T.Hint = Ep.Power >= 60 ? 0 : 2;
				break;
			default:
				break;
			}
			return T;
		}
		// nothing with a name in the field: the rare sky, in the wide eyepieces
		if (Ep.FieldDeg >= 4.0 && Gap >= 0.4)
		{
			if (L.Aurora > 0.25 && S.SunAltDeg < -8.0 && L.Aim.AltDeg > 2.0 && L.Aim.AltDeg < 60.0
				&& std::fabs(Wrap180(L.Aim.AzDeg - 344.0)) < 75.0)
			{
				T.Sight = ESight::Aurora;
			}
			else if (L.Noctilucent > 0.25 && S.SunAltDeg < -5.0 && L.Aim.AltDeg > 1.0 && L.Aim.AltDeg < 30.0
				&& std::fabs(Wrap180(L.Aim.AzDeg - S.SunAzDeg)) < 60.0)
			{
				T.Sight = ESight::Noctilucent;
			}
		}
		return T;
	}

	bool SunDanger(const FSky& S, const FLook& L)
	{
		if (L.bFilter || S.SunAltDeg < -0.5)
		{
			return false;
		}
		const double Sep = Separation(L.Aim.AltDeg, L.Aim.AzDeg, S.SunAltDeg, S.SunAzDeg);
		return Sep < L.SunSizeDeg * 0.5 + Eyepiece(L.Eyepiece).FieldDeg * 0.5 + 0.5;
	}

	bool StepWatch(FWatch& W, const FTarget& T, double Dt)
	{
		if (T.Sight == ESight::Num)
		{
			W = FWatch();
			return false;
		}
		if (T.Sight != W.Sight)
		{
			W.Sight = T.Sight;
			W.Held = 0.0;
			W.bCounted = false;
		}
		W.Held += std::max(0.0, Dt);
		if (!W.bCounted && W.Held >= HoldSeconds)
		{
			W.bCounted = true;
			return true;
		}
		return false;
	}

	// ---------------------------------------------------------------- the log

	int FLog::Count() const
	{
		int N = 0;
		for (int I = 0; I < static_cast<int>(ESight::Num); ++I)
		{
			N += (Seen >> I) & 1u;
		}
		return N;
	}

	int NightOf(double LocalDay)
	{
		return static_cast<int>(std::floor(LocalDay - 0.5));
	}

	bool Record(FLog& Log, ESight Sight, double LocalDay, FNote& Out)
	{
		Out = FNote();
		if (Sight == ESight::Num)
		{
			return false;
		}
		Out.Sight = Sight;
		Out.bFirst = !Log.Has(Sight);
		Log.Seen |= 1u << static_cast<int>(Sight);
		switch (Sight)
		{
		case ESight::JupiterMoons: Out.Also = ESight::Jupiter; break;
		case ESight::SaturnRing: Out.Also = ESight::Saturn; break;
		case ESight::VenusPhase: Out.Also = ESight::Venus; break;
		default: break;
		}
		if (Out.Also != ESight::Num)
		{
			Out.bAlsoFirst = !Log.Has(Out.Also);
			Log.Seen |= 1u << static_cast<int>(Out.Also);
		}
		if (Sight == ESight::JupiterMoons)
		{
			const int Night = NightOf(LocalDay);
			if (Night != Log.LastJupiterNight)
			{
				Log.LastJupiterNight = Night;
				++Log.JupiterNights;
				Out.bNewNight = true;
			}
		}
		Out.JupiterNights = Log.JupiterNights;
		return Out.bFirst || Out.bAlsoFirst || Out.bNewNight;
	}

	std::string MoonsDiagram(const FBody& Jupiter)
	{
		// 27 cells, Jupiter in the middle; a moon at +X (west) is on the right with north up; 2.1 radii a cell
		std::string Row(27, '-');
		Row[13] = 'O';
		for (int I = 0; I < 4; ++I)
		{
			if (!Jupiter.bMoonSeen[I])
			{
				continue;
			}
			int Cell = 13 + static_cast<int>(std::lround(Jupiter.MoonX[I] / 2.1));
			if (Cell == 13)
			{
				Cell += Jupiter.MoonX[I] >= 0.0 ? 1 : -1;
			}
			Cell = std::max(0, std::min(26, Cell));
			// two moons in one cell: the second goes into the next free one outward
			const int Step = Cell >= 13 ? 1 : -1;
			while (Cell > 0 && Cell < 26 && Row[Cell] != '-')
			{
				Cell += Step;
			}
			Row[Cell] = 'o';
		}
		return Row;
	}

	// ---------------------------------------------------------------- the field stars

	double LimitMagnitude(int EyepieceIndex, double SunAltDeg)
	{
		static const double Dark[NumEyepieces] = { 6.5, 8.5, 11.0, 13.8 };
		const double D = Dark[std::max(0, std::min(NumEyepieces - 1, EyepieceIndex))];
		// the twilight washes them out between -14 and -4 degrees of the sun
		return D - 9.0 * Clamp01((SunAltDeg + 14.0) / 10.0);
	}

	int Stars(const FSky& S, double AimAltDeg, double AimAzDeg, double FieldDeg, double Limit, FStar* Out, int Max)
	{
		int Count = 0;
		if (Max <= 0 || Limit < 2.0)
		{
			return 0;
		}
		double Ra0 = 0.0, Dec0 = 0.0;
		ToEquatorial(S, AimAltDeg, AimAzDeg, Ra0, Dec0);
		const FBody& Moon = S.Of(EBody::Moon);
		const double MoonRadius = Moon.DiameterArcsec / 7200.0;
		const double Half = FieldDeg * 0.56;
		const double Edge = std::tan(FieldDeg * 0.5 * D2R) * 1.04;
		for (int Band = 2; Band <= 14 && Band <= static_cast<int>(Limit); ++Band)
		{
			// stars of magnitude Band..Band+1 live in cells this wide, about one star to two cells
			const double Cell = 2.5 * std::pow(0.6, Band - 5);
			const int I0 = static_cast<int>(std::floor((Dec0 - Half) / Cell));
			const int I1 = static_cast<int>(std::floor((Dec0 + Half) / Cell));
			for (int I = I0; I <= I1; ++I)
			{
				const double DecMid = (I + 0.5) * Cell;
				if (DecMid < -89.0 || DecMid > 89.0)
				{
					continue;
				}
				const double CosDec = std::max(0.05, Cosd(DecMid));
				const int Around = std::max(1, static_cast<int>(360.0 * CosDec / Cell));
				const double Width = 360.0 / Around;
				const double Span = std::min(180.0, Half / CosDec);
				const int J0 = static_cast<int>(std::floor((Ra0 - Span) / Width));
				const int J1 = static_cast<int>(std::floor((Ra0 + Span) / Width));
				for (int J = J0; J <= J1 && J - J0 < Around; ++J)
				{
					const int Jw = ((J % Around) + Around) % Around;
					const uint32_t H = Hash(static_cast<uint32_t>(Band), static_cast<uint32_t>(I + 4096), static_cast<uint32_t>(Jw));
					if ((H & 1u) == 0u)
					{
						continue;
					}
					const double Ra = (Jw + Unit(Hash(H, 1u, 7u))) * Width;
					const double Dec = (I + Unit(Hash(H, 2u, 7u))) * Cell;
					double Mag = Band + Unit(Hash(H, 3u, 7u));
					double Alt = 0.0, Az = 0.0;
					ToHorizontal(S, Ra, Dec, Alt, Az);
					if (Alt < 1.0)
					{
						continue;
					}
					// the air dims the low ones
					Mag += 0.25 * (1.0 / Sind(std::max(Alt, 3.0)) - 1.0);
					if (Mag > Limit)
					{
						continue;
					}
					if (MoonRadius > 0.0 && Separation(Alt, Az, Moon.AltDeg, Moon.AzDeg) < MoonRadius * 1.02)
					{
						continue;
					}
					double X = 0.0, Y = 0.0;
					if (!Project(AimAltDeg, AimAzDeg, Alt, Az, X, Y) || X * X + Y * Y > Edge * Edge)
					{
						continue;
					}
					FStar& Star = Out[Count++];
					Star.X = static_cast<float>(X);
					Star.Y = static_cast<float>(Y);
					Star.Magnitude = static_cast<float>(Mag);
					Star.Warmth = static_cast<float>(Unit(Hash(H, 4u, 7u)));
					if (Count >= Max)
					{
						return Count;
					}
				}
			}
		}
		return Count;
	}

	// ---------------------------------------------------------------- pictures

	namespace
	{
		struct FRgba
		{
			double R = 0.0, G = 0.0, B = 0.0, A = 0.0;
		};

		FRgba Mix(const FRgba& X, const FRgba& Y, double T)
		{
			T = Clamp01(T);
			return { X.R + (Y.R - X.R) * T, X.G + (Y.G - X.G) * T, X.B + (Y.B - X.B) * T, X.A + (Y.A - X.A) * T };
		}

		FRgba Scale(const FRgba& X, double K)
		{
			return { X.R * K, X.G * K, X.B * K, X.A };
		}

		/** How lit a point of a unit disk is at a phase angle, the sun on the right: 0 in the night side, 1 well into the day. */
		double PhaseLight(double U, double V, double PhaseAngleDeg)
		{
			const double Chord = std::sqrt(std::max(0.0, 1.0 - V * V));
			const double Terminator = -Cosd(PhaseAngleDeg) * Chord;
			return std::pow(Clamp01((U - Terminator) * 3.0), 0.6);
		}

		FRgba VenusAt(double U, double V, const FBody& B)
		{
			const double R2 = U * U + V * V;
			if (R2 > 1.0)
			{
				return FRgba();
			}
			const double Limb = 0.72 + 0.28 * std::sqrt(1.0 - R2);
			const double Light = PhaseLight(U, V, B.PhaseAngleDeg);
			return { 1.0 * Limb * Light, 0.98 * Limb * Light, 0.90 * Limb * Light, Light > 0.01 ? 1.0 : 0.0 };
		}

		FRgba MarsAt(double U, double V, const FBody& B)
		{
			const double R2 = U * U + V * V;
			if (R2 > 1.0)
			{
				return FRgba();
			}
			const double Z = std::sqrt(1.0 - R2);
			// the markings ride the planet's turn
			const double Lon = Atan2d(U, Z) + B.SpinDeg;
			const double Lat = Asind(V);
			const double Mark = Noise2(Lon / 28.0, Lat / 20.0, 4u) * 0.65 + Noise2(Lon / 11.0, Lat / 9.0, 9u) * 0.35;
			FRgba C = Mix({ 0.90, 0.52, 0.30, 1.0 }, { 0.46, 0.27, 0.19, 1.0 }, Smooth(0.5, 0.72, Mark) * (1.0 - Smooth(40.0, 70.0, std::fabs(Lat))));
			// the polar caps
			C = Mix(C, { 0.97, 0.95, 0.92, 1.0 }, Smooth(74.0, 80.0, Lat) + 0.7 * Smooth(80.0, 85.0, -Lat));
			const double Limb = 0.55 + 0.45 * std::pow(Z, 0.6);
			const double Light = PhaseLight(U, V, B.PhaseAngleDeg);
			C = Scale(C, Limb * Light);
			C.A = Light > 0.01 ? 1.0 : 0.0;
			return C;
		}

		FRgba JupiterAt(double U, double V, const FBody& B)
		{
			const double Vp = V / 0.935;     // flattened at the poles
			const double R2 = U * U + Vp * Vp;
			if (R2 > 1.0)
			{
				return FRgba();
			}
			const double Z = std::sqrt(1.0 - R2);
			const double Lon = Atan2d(U, Z) + B.SpinDeg;
			// the belts' edges waver a little along the longitude
			const double S = Vp + 0.018 * (Noise2(Lon / 14.0, Vp * 9.0, 21u) - 0.5);
			const FRgba Zone = { 0.95, 0.90, 0.78, 1.0 };
			const FRgba Belt = { 0.63, 0.43, 0.29, 1.0 };
			const FRgba Thin = { 0.74, 0.60, 0.46, 1.0 };
			const FRgba Polar = { 0.60, 0.56, 0.50, 1.0 };
			FRgba C = Zone;
			// the two great equatorial belts, the thinner temperate ones, the grey polar hoods
			C = Mix(C, Belt, Smooth(0.09, 0.115, S) * (1.0 - Smooth(0.25, 0.28, S)));
			C = Mix(C, Belt, Smooth(-0.31, -0.28, S) * (1.0 - Smooth(-0.13, -0.105, S)));
			C = Mix(C, Thin, Smooth(0.42, 0.44, S) * (1.0 - Smooth(0.49, 0.51, S)));
			C = Mix(C, Thin, Smooth(-0.56, -0.54, S) * (1.0 - Smooth(-0.48, -0.46, S)));
			C = Mix(C, Polar, Smooth(0.70, 0.86, std::fabs(S)));
			// streaks inside the belts
			C = Scale(C, 0.94 + 0.12 * Noise2(Lon / 6.0, S * 40.0, 33u));
			// the Great Red Spot in the south belt's lower edge
			const double SpotLon = Wrap180(Lon - 110.0);
			const double Spot = (SpotLon / 11.0) * (SpotLon / 11.0) + ((S + 0.345) / 0.055) * ((S + 0.345) / 0.055);
			C = Mix(C, { 0.82, 0.40, 0.27, 1.0 }, 1.0 - Smooth(0.6, 1.1, Spot));
			return Scale(C, 0.35 + 0.65 * std::pow(Z, 0.45));
		}

		FRgba SaturnAt(double U, double V, const FBody& B)
		{
			const double SinB = Sind(B.RingTiltDeg);
			const double Open = std::max(0.02, std::fabs(SinB));
			// the ring: a circle in its own plane, squashed by the tilt
			const double Rr = std::sqrt(U * U + (V / Open) * (V / Open));
			FRgba Ring;
			if (Rr >= 1.24 && Rr <= 2.27)
			{
				const FRgba Tint = { 0.93, 0.87, 0.72, 1.0 };
				double Alpha = 0.0, Bright = 1.0;
				if (Rr < 1.53) { Alpha = 0.22; Bright = 0.6; }                       // the dusky C ring
				else if (Rr < 1.95) { Alpha = 0.96; Bright = 0.9 + 0.1 * (Rr - 1.53) / 0.42; }     // B, the bright one
				else if (Rr < 2.03) { Alpha = 0.06; Bright = 0.3; }                  // Cassini's division
				else { Alpha = 0.78; Bright = 0.74; }                                // A
				Ring = Scale(Tint, Bright);
				Ring.A = Alpha;
			}
			const double Vp = V / 0.90;
			const double R2 = U * U + Vp * Vp;
			if (R2 <= 1.0)
			{
				const double Z = std::sqrt(1.0 - R2);
				FRgba Globe = { 0.92, 0.82, 0.60, 1.0 };
				Globe = Mix(Globe, { 0.80, 0.68, 0.48, 1.0 }, Smooth(0.16, 0.2, std::fabs(Vp)) * (1.0 - Smooth(0.32, 0.36, std::fabs(Vp))) * 0.7);
				Globe = Mix(Globe, { 0.66, 0.62, 0.50, 1.0 }, Smooth(0.62, 0.9, std::fabs(Vp)));
				Globe = Scale(Globe, 0.35 + 0.65 * std::pow(Z, 0.45));
				// the near half of the ring passes in front of the globe: below the middle when we see the north face
				const bool bInFront = Ring.A > 0.0 && ((V < 0.0) == (SinB > 0.0));
				if (bInFront)
				{
					FRgba Over = Mix(Globe, Ring, Ring.A);
					Over.A = 1.0;
					return Over;
				}
				return Globe;
			}
			return Ring;
		}
	}

	void Draw(EBody Body, const FBody& B, int Size, FImage& Out)
	{
		Out.Size = Size;
		Out.Rgba.assign(static_cast<size_t>(Size) * Size * 4, 0);
		const double HalfExtent = Body == EBody::Saturn ? 2.45 : 1.15;     // radii from the middle to the picture's edge
		Out.DiskPixels = Size / HalfExtent;
		const int Sub = 3;
		for (int Py = 0; Py < Size; ++Py)
		{
			for (int Px = 0; Px < Size; ++Px)
			{
				double R = 0.0, G = 0.0, Bl = 0.0, A = 0.0;
				for (int Sy = 0; Sy < Sub; ++Sy)
				{
					for (int Sx = 0; Sx < Sub; ++Sx)
					{
						const double U = ((Px + (Sx + 0.5) / Sub) / Size * 2.0 - 1.0) * HalfExtent;
						const double V = (1.0 - (Py + (Sy + 0.5) / Sub) / Size * 2.0) * HalfExtent;
						FRgba C;
						switch (Body)
						{
						case EBody::Venus: C = VenusAt(U, V, B); break;
						case EBody::Mars: C = MarsAt(U, V, B); break;
						case EBody::Jupiter: C = JupiterAt(U, V, B); break;
						case EBody::Saturn: C = SaturnAt(U, V, B); break;
						default: break;
						}
						R += C.R * C.A;
						G += C.G * C.A;
						Bl += C.B * C.A;
						A += C.A;
					}
				}
				uint8_t* P = &Out.Rgba[(static_cast<size_t>(Py) * Size + Px) * 4];
				if (A > 0.0)
				{
					P[0] = static_cast<uint8_t>(std::lround(255.0 * Clamp01(R / A)));
					P[1] = static_cast<uint8_t>(std::lround(255.0 * Clamp01(G / A)));
					P[2] = static_cast<uint8_t>(std::lround(255.0 * Clamp01(Bl / A)));
					P[3] = static_cast<uint8_t>(std::lround(255.0 * Clamp01(A / (Sub * Sub))));
				}
			}
		}
	}

	uint32_t DrawKey(EBody Body, const FBody& B)
	{
		switch (Body)
		{
		case EBody::Venus: return static_cast<uint32_t>(std::lround(B.PhaseAngleDeg / 3.0));
		case EBody::Mars: return static_cast<uint32_t>(std::lround(B.SpinDeg / 8.0)) * 64u + static_cast<uint32_t>(std::lround(B.PhaseAngleDeg / 4.0));
		case EBody::Jupiter: return static_cast<uint32_t>(std::lround(B.SpinDeg / 5.0));
		case EBody::Saturn: return static_cast<uint32_t>(std::lround((B.RingTiltDeg + 40.0) * 4.0));
		default: return 0u;
		}
	}
}
