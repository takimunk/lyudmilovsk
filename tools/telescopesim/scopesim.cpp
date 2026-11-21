// The telescopes' model outside the engine: checks, a night's table, pictures. See run.sh.
#include "TelescopeModel.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace LyudScope;

namespace
{
	const double Lat = 54.96, Lon = 20.40, UtcOffset = 2.0;
	int Failed = 0, Passed = 0;

	void Check(bool bOk, const char* What, double Got = 0.0, double Want = 0.0)
	{
		if (bOk)
		{
			++Passed;
		}
		else
		{
			++Failed;
			std::printf("FAIL %s (got %.3f, want %.3f)\n", What, Got, Want);
		}
	}

	void Near(double Got, double Want, double Tol, const char* What)
	{
		double D = std::fabs(Got - Want);
		if (D > 180.0)
		{
			D = 360.0 - D;
		}
		Check(D <= Tol, What, Got, Want);
	}

	FSky SkyAt(int Y, int M, int D, double Ut)
	{
		return Sky(DayNumber(Y, M, D, Ut), Lat, Lon);
	}

	/** A dark, clear, focused look at a body with an eyepiece. */
	FLook LookAt(const FSky& S, EBody Body, int Ep)
	{
		FLook L;
		L.Aim.AltDeg = S.Of(Body).AltDeg;
		L.Aim.AzDeg = S.Of(Body).AzDeg;
		L.Eyepiece = Ep;
		L.Knob = InfinityKnob;
		return L;
	}

	void TestPlaces()
	{
		// astropy 8 (the built-in ephemeris), topocentric, no refraction: alt, az
		struct FRef { int Y, M, D; double Ut; double Sun[2], Venus[2], Mars[2], Jupiter[2], Saturn[2]; };
		const FRef Refs[] = {
			{ 2026, 10, 21, 3.0, { -20.59, 78.97 }, { -28.46, 82.47 }, { 41.97, 123.31 }, { 33.99, 115.80 }, { 4.26, 266.51 } },
			{ 2027, 2, 10, 22.0, { -47.97, 340.77 }, { -48.59, 48.49 }, { 44.25, 142.75 }, { 48.82, 159.82 }, { -14.70, 296.51 } },
			{ 2026, 8, 15, 18.5, { -3.16, 299.77 }, { 4.78, 254.53 }, { -10.76, 349.66 }, { -5.17, 312.40 }, { -9.76, 69.71 } },
			{ 2026, 12, 25, 5.0, { -15.98, 108.12 }, { 14.79, 143.55 }, { 41.07, 216.96 }, { 38.25, 232.48 }, { -34.10, 0.83 } },
		};
		for (const FRef& R : Refs)
		{
			const FSky S = SkyAt(R.Y, R.M, R.D, R.Ut);
			Near(S.SunAltDeg, R.Sun[0], 0.3, "sun altitude");
			Near(S.SunAzDeg, R.Sun[1], 0.5, "sun azimuth");
			const EBody Bodies[4] = { EBody::Venus, EBody::Mars, EBody::Jupiter, EBody::Saturn };
			const double* Want[4] = { R.Venus, R.Mars, R.Jupiter, R.Saturn };
			for (int I = 0; I < 4; ++I)
			{
				Near(S.Of(Bodies[I]).AltDeg, Want[I][0], 0.4, BodyId(Bodies[I]));
				// the azimuth's tolerance on the sky, not on the compass
				const double Tol = 0.4 / std::max(0.2, std::cos(Want[I][0] * 3.14159265 / 180.0)) + 0.1;
				Near(S.Of(Bodies[I]).AzDeg, Want[I][1], Tol, BodyId(Bodies[I]));
			}
		}
	}

	void TestLooks()
	{
		// Jupiter at its opposition of February 2027: big, bright, opposite the sun
		{
			const FSky S = SkyAt(2027, 2, 10, 22.0);
			const FBody& J = S.Of(EBody::Jupiter);
			Check(J.ElongationDeg > 170.0, "Jupiter opposite the sun", J.ElongationDeg, 180.0);
			Check(J.DiameterArcsec > 43.0 && J.DiameterArcsec < 47.0, "Jupiter's size at opposition", J.DiameterArcsec, 45.0);
			Check(J.Magnitude < -2.3 && J.Magnitude > -3.0, "Jupiter's magnitude", J.Magnitude, -2.6);
			Check(J.Lit > 0.99, "Jupiter full", J.Lit, 1.0);
		}
		// Venus: a thin big crescent near its inferior conjunction (24 October 2026), a small gibbous disk in summer
		{
			const FBody V1 = SkyAt(2026, 10, 12, 16.0).Of(EBody::Venus);
			Check(V1.Lit < 0.08 && V1.DiameterArcsec > 52.0, "Venus a thin crescent in October 2026", V1.Lit, 0.05);
			const FBody V2 = SkyAt(2026, 8, 15, 18.0).Of(EBody::Venus);
			Check(V2.ElongationDeg > 44.0 && V2.ElongationDeg < 47.5, "Venus at its greatest elongation", V2.ElongationDeg, 45.9);
			Check(V2.Lit > 0.4 && V2.Lit < 0.6, "Venus half lit at elongation", V2.Lit, 0.5);
			Check(V2.Magnitude < -4.0, "Venus bright", V2.Magnitude, -4.3);
		}
		// Saturn: the ring edge-on in March 2025, open again by late 2026; it never opens past 27 degrees
		{
			const FBody S1 = SkyAt(2025, 3, 23, 12.0).Of(EBody::Saturn);
			Check(std::fabs(S1.RingTiltDeg) < 1.0, "Saturn's ring edge-on in March 2025", S1.RingTiltDeg, 0.0);
			const FBody S2 = SkyAt(2026, 10, 20, 20.0).Of(EBody::Saturn);
			Check(std::fabs(S2.RingTiltDeg) > 3.0 && std::fabs(S2.RingTiltDeg) < 12.0, "Saturn's ring open in October 2026", S2.RingTiltDeg, 7.0);
			Check(S2.DiameterArcsec > 18.0 && S2.DiameterArcsec < 21.0, "Saturn's size", S2.DiameterArcsec, 19.5);
			double Most = 0.0;
			for (int Y = 2026; Y < 2056; ++Y)
			{
				Most = std::max(Most, std::fabs(SkyAt(Y, 6, 1, 0.0).Of(EBody::Saturn).RingTiltDeg));
			}
			Check(Most > 24.0 && Most < 27.5, "Saturn's ring opens to ~27 degrees", Most, 27.0);
		}
		// Jupiter's moons: Meeus' example 44.a (1992 December 16, 0h UT)
		{
			const FBody J = SkyAt(1992, 12, 16, 0.0).Of(EBody::Jupiter);
			const double X[4] = { -3.44, 7.44, 1.24, 7.08 }, Y[4] = { -0.21, 0.25, 0.65, 1.10 };
			for (int I = 0; I < 4; ++I)
			{
				Near(J.MoonX[I], X[I], 0.12, "a moon of Jupiter, X");
				// Io's Y is checked by its size only (its sign in the book's table is not at hand)
				Near(I == 0 ? -std::fabs(J.MoonY[I]) : J.MoonY[I], Y[I], 0.12, "a moon of Jupiter, Y");
			}
			const std::string Row = MoonsDiagram(J);
			Check(Row.size() == 27 && Row[13] == 'O', "the moons' drawing");
			int Dots = 0;
			for (const char C : Row)
			{
				Dots += C == 'o' ? 1 : 0;
			}
			Check(Dots == 4, "four moons drawn", Dots, 4);
			// they stay within their orbits and move from night to night
			const FBody J2 = SkyAt(1992, 12, 17, 0.0).Of(EBody::Jupiter);
			Check(std::fabs(J2.MoonX[0] - J.MoonX[0]) > 1.0, "Io moves in a day");
			const double Reach[4] = { 5.95, 9.5, 15.05, 26.6 };
			for (int Day = 0; Day < 400; ++Day)
			{
				const FBody Jd = Sky(9800.0 + Day * 0.37, Lat, Lon).Of(EBody::Jupiter);
				for (int I = 0; I < 4; ++I)
				{
					if (std::fabs(Jd.MoonX[I]) > Reach[I])
					{
						Check(false, "a moon outside its orbit", Jd.MoonX[I], Reach[I]);
					}
				}
			}
		}
	}

	void TestInstrument()
	{
		// the focus: infinity at its mark, near things nearer the knob's start, nothing sharp past infinity
		Near(FocusDioptre(InfinityKnob), 0.0, 1e-9, "infinity at the mark");
		Check(FocusDistanceM(0.0) > 15.0 && FocusDistanceM(0.0) < 25.0, "the nearest focus about 20 m", FocusDistanceM(0.0), 20.0);
		Check(FocusDioptre(1.0) < 0.0, "past infinity");
		for (int Ep = 0; Ep < NumEyepieces; ++Ep)
		{
			const double F = Eyepiece(Ep).FieldDeg;
			Check(Blur(InfinityKnob, 0.0, F) < SharpBlur, "the sky sharp at infinity");
			Check(Blur(1.0, 0.0, F) > Blur(InfinityKnob, 0.0, F), "past infinity blurs");
		}
		Check(Blur(InfinityKnob, 150.0, Eyepiece(2).FieldDeg) > SharpBlur, "a bird at 150 m is blurred at infinity, x60");
		Check(Blur(InfinityKnob, 3000.0, Eyepiece(1).FieldDeg) < SharpBlur, "a ship at 3 km is sharp at infinity, x12");
		Check(Blur(0.3, 0.0, Eyepiece(1).FieldDeg) > SharpBlur * 1.5, "the sky blurred with the knob at a near thing, x12");
		// there is a knob position that makes a thing at 150 m sharp
		{
			bool bFound = false;
			for (double K = 0.0; K <= 1.0; K += 0.002)
			{
				bFound = bFound || Blur(K, 150.0, Eyepiece(2).FieldDeg) < SharpBlur;
			}
			Check(bFound, "150 m can be focused at x60");
		}
		// the aim: a push carries the view the same way whatever the frame rate, and settles
		for (const double Dt : { 1.0 / 30.0, 1.0 / 120.0 })
		{
			FAim A;
			A.AzDeg = 100.0;
			A.AltDeg = 20.0;
			const FMount M;
			StepAim(A, 350.0, 0.0, 5.0, Dt, M);
			for (int I = 0; I < static_cast<int>(3.0 / Dt); ++I)
			{
				StepAim(A, 0.0, 0.0, 5.0, Dt, M);
			}
			// half a field's worth of counts: 2.5 degrees on the sky = 2.5 / cos(20) of azimuth
			Near(A.AzDeg, 100.0 + 2.5 / std::cos(20.0 * 3.14159265 / 180.0), 0.12, "a push's travel");
			Check(std::fabs(A.VAz) < 1e-3, "the tube settles");
		}
		{
			// the stops
			FAim A;
			A.AzDeg = 10.0;
			A.AltDeg = 80.0;
			FMount M;
			M.AzCenterDeg = 0.0;
			M.AzRangeDeg = 40.0;
			for (int I = 0; I < 300; ++I)
			{
				StepAim(A, 400.0, 400.0, 15.0, 1.0 / 60.0, M);
			}
			Near(A.AltDeg, M.AltMaxDeg, 1e-6, "the upper stop");
			Near(A.AzDeg, 40.0, 1e-6, "the side stop");
			for (int I = 0; I < 300; ++I)
			{
				StepAim(A, -400.0, -400.0, 15.0, 1.0 / 60.0, M);
			}
			Near(A.AltDeg, M.AltMinDeg, 1e-6, "the lower stop");
			Near(A.AzDeg, 320.0, 1e-6, "the other side stop");
		}
		// Project agrees with Separation, and equatorial <-> horizontal round-trips
		{
			const FSky S = SkyAt(2026, 10, 21, 3.0);
			double X = 0.0, Y = 0.0;
			Check(Project(30.0, 100.0, 32.0, 100.0, X, Y) && std::fabs(X) < 1e-9 && Y > 0.0, "up is up");
			Near(std::atan(Y) * 180.0 / 3.14159265, 2.0, 1e-6, "two degrees up");
			Check(Project(30.0, 100.0, 30.0, 102.0, X, Y) && X > 0.0, "a growing azimuth is to the right");
			double Ra = 0.0, Dec = 0.0, Alt = 0.0, Az = 0.0;
			ToEquatorial(S, 37.0, 211.0, Ra, Dec);
			ToHorizontal(S, Ra, Dec, Alt, Az);
			Near(Alt, 37.0, 1e-6, "round trip, altitude");
			Near(Az, 211.0, 1e-6, "round trip, azimuth");
		}
	}

	void TestSights()
	{
		// 21 October 2026, 05:00 local: Jupiter and Mars high in the south-east before dawn
		FSky S = SkyAt(2026, 10, 21, 3.0);
		SetMoon(S, -30.0, 300.0, 14.0, 0.7);
		Check(Visibility(S, EBody::Jupiter) > 0.9, "Jupiter shows before dawn");
		Check(Visibility(S, EBody::Venus) < 0.01, "Venus is down");
		{
			FLook L = LookAt(S, EBody::Jupiter, 0);
			FTarget T = Target(S, L);
			Check(T.Body == static_cast<int>(EBody::Jupiter) && T.Sight == ESight::Jupiter && T.Hint == 2, "the finder shows Jupiter, asks for power");
			L.Eyepiece = 2;
			T = Target(S, L);
			Check(T.Sight == ESight::JupiterMoons, "x60 shows the moons");
			L.Knob = 0.5;
			T = Target(S, L);
			Check(T.Sight == ESight::Num && T.Hint == 1, "out of focus: nothing, a hint to focus");
			L.Knob = InfinityKnob;
			L.CloudCover = 1.0;
			T = Target(S, L);
			Check(T.Sight == ESight::Num && T.Hint == 4, "overcast: nothing");
			L.CloudCover = 0.0;
			L.bBlocked = true;
			Check(Target(S, L).Body < 0, "a wall in the way: nothing");
			L.bBlocked = false;
			L.Aim.AzDeg += 0.39;     // 0.4 of the x60 field off: in it, not in the middle
			T = Target(S, L);
			Check(T.Body == static_cast<int>(EBody::Jupiter) && T.Sight == ESight::Num, "off the middle: bring it in");
			// held for two seconds it counts once
			L.Aim.AzDeg -= 0.39;
			T = Target(S, L);
			FWatch W;
			int Counted = 0;
			for (int I = 0; I < 300; ++I)
			{
				Counted += StepWatch(W, T, 1.0 / 60.0) ? 1 : 0;
				if (I == 60)
				{
					Check(Counted == 0, "one second is not enough");
				}
			}
			Check(Counted == 1, "a held look counts once", Counted, 1);
		}
		// the same morning, the sun up: no moons, then no Jupiter
		{
			FSky Day = SkyAt(2026, 10, 21, 9.0);
			SetMoon(Day, -30.0, 300.0, 14.0, 0.7);
			Check(Day.SunAltDeg > 10.0 && Visibility(Day, EBody::Jupiter) < 0.01, "no Jupiter by day");
		}
		// the moon: its phases by the lit part; a planet behind it is hidden
		{
			FSky M = S;
			SetMoon(M, 40.0, 180.0, 14.0, 0.5);
			FLook L = LookAt(M, EBody::Moon, 0);
			Check(Target(M, L).Sight == ESight::MoonQuarter, "a half moon");
			L.Aim.AzDeg += 5.0;     // inside the 14-degree disk still
			Check(Target(M, L).Sight == ESight::MoonQuarter, "anywhere on the disk");
			SetMoon(M, 40.0, 180.0, 14.0, 0.99);
			Check(Target(M, LookAt(M, EBody::Moon, 1)).Sight == ESight::MoonFull, "a full moon");
			SetMoon(M, 40.0, 180.0, 14.0, 0.15);
			Check(Target(M, LookAt(M, EBody::Moon, 1)).Sight == ESight::MoonCrescent, "a crescent");
			SetMoon(M, M.Of(EBody::Jupiter).AltDeg + 2.0, M.Of(EBody::Jupiter).AzDeg, 14.0, 0.8);
			Check(Visibility(M, EBody::Jupiter) == 0.0, "Jupiter behind the moon");
			Check(Target(M, LookAt(M, EBody::Jupiter, 2)).Sight == ESight::MoonGibbous, "the moon is what is seen there");
		}
		// Saturn's ring wants x60; Venus' phase too
		{
			FSky E = SkyAt(2026, 10, 20, 19.0);
			SetMoon(E, -20.0, 0.0, 14.0, 0.5);
			Check(Visibility(E, EBody::Saturn) > 0.9, "Saturn shows in the evening of 20 October");
			Check(Target(E, LookAt(E, EBody::Saturn, 1)).Sight == ESight::Saturn, "x12: Saturn");
			Check(Target(E, LookAt(E, EBody::Saturn, 2)).Sight == ESight::SaturnRing, "x60: the ring");
			FSky V = SkyAt(2026, 12, 25, 5.0);
			SetMoon(V, -20.0, 0.0, 14.0, 0.5);
			Check(Visibility(V, EBody::Venus) > 0.9, "Venus the morning star in December");
			Check(Target(V, LookAt(V, EBody::Venus, 3)).Sight == ESight::VenusPhase, "x400: the phase of Venus");
		}
		// the sun: danger without the filter, spots with it
		{
			FSky Day = SkyAt(2026, 10, 21, 10.0);
			SetMoon(Day, -30.0, 300.0, 14.0, 0.7);
			FLook L;
			L.Aim.AltDeg = Day.SunAltDeg + 6.0;
			L.Aim.AzDeg = Day.SunAzDeg;
			L.Eyepiece = 0;
			Check(SunDanger(Day, L), "the sun near the finder's field");
			L.Aim.AzDeg += 60.0;
			Check(!SunDanger(Day, L), "away from the sun");
			L.Aim.AzDeg -= 60.0;
			L.Aim.AltDeg = Day.SunAltDeg;
			L.bFilter = true;
			Check(!SunDanger(Day, L) && Target(Day, L).Sight == ESight::Sunspots, "the filter: spots");
			FSky Night = S;
			L.Aim.AltDeg = Night.SunAltDeg;
			L.Aim.AzDeg = Night.SunAzDeg;
			Check(Target(Night, L).Sight == ESight::Num, "no sun at night");
		}
		// the rare sky in the wide eyepieces
		{
			FLook L;
			L.Aim.AltDeg = 15.0;
			L.Aim.AzDeg = 350.0;
			L.Eyepiece = 0;
			L.Aurora = 0.8;
			Check(Target(S, L).Sight == ESight::Aurora, "the aurora in the north");
			L.Aim.AzDeg = 170.0;
			Check(Target(S, L).Sight == ESight::Num, "no aurora in the south");
			L.Aim.AzDeg = 350.0;
			L.Eyepiece = 3;
			Check(Target(S, L).Sight == ESight::Num, "not at x400");
		}
	}

	void TestLog()
	{
		FLog Log;
		FNote N;
		const double Night1 = 9800.9;     // an evening
		Check(Record(Log, ESight::JupiterMoons, Night1, N) && N.bFirst && N.bNewNight && N.bAlsoFirst && N.JupiterNights == 1, "the first moons");
		Check(Log.Has(ESight::Jupiter) && Log.Count() == 2, "Jupiter comes with its moons", Log.Count(), 2);
		Check(!Record(Log, ESight::JupiterMoons, Night1 + 0.2, N), "the same night after midnight: nothing new");
		Check(Record(Log, ESight::JupiterMoons, Night1 + 1.0, N) && !N.bFirst && N.bNewNight && N.JupiterNights == 2, "the next night");
		Record(Log, ESight::JupiterMoons, Night1 + 2.1, N);
		Record(Log, ESight::JupiterMoons, Night1 + 3.0, N);
		Check(N.JupiterNights == 4, "four nights", N.JupiterNights, 4);
		Check(!Record(Log, ESight::Jupiter, Night1 + 3.0, N), "Jupiter again: nothing new");
		Check(Record(Log, ESight::MoonFull, Night1, N) && N.bFirst && N.Also == ESight::Num, "a full moon");
		Check(NightOf(10.4) == 9 && NightOf(10.6) == 10, "nights run noon to noon");
		for (int I = 0; I < static_cast<int>(ESight::Num); ++I)
		{
			Check(SightId(static_cast<ESight>(I))[0] != 0, "every sight has an id");
		}
	}

	void TestStars()
	{
		FSky S = SkyAt(2026, 10, 21, 0.0);
		SetMoon(S, -30.0, 300.0, 14.0, 0.7);
		std::vector<FStar> Buf(600);
		int Total = 0;
		for (int Ep = 0; Ep < NumEyepieces; ++Ep)
		{
			const double Limit = LimitMagnitude(Ep, S.SunAltDeg);
			const int N = Stars(S, 45.0, 180.0, Eyepiece(Ep).FieldDeg, Limit, Buf.data(), static_cast<int>(Buf.size()));
			Check(N >= 2 && N <= 200, "a field of stars at every power", N, 30);
			Total += N;
			const double Edge = std::tan(Eyepiece(Ep).FieldDeg * 0.5 * 3.14159265 / 180.0) * 1.05;
			for (int I = 0; I < N; ++I)
			{
				if (std::hypot(Buf[I].X, Buf[I].Y) > Edge || Buf[I].Magnitude > Limit)
				{
					Check(false, "a star outside the field or too faint");
				}
			}
		}
		// the same stars a moment later, a little moved (the sky turns); none in daylight; none behind the moon
		const int N0 = Stars(S, 45.0, 180.0, 4.0, 8.5, Buf.data(), 600);
		std::vector<FStar> Buf2(600);
		const FSky S2 = Sky(S.Day + 20.0 / 86400.0, Lat, Lon);
		const int N1 = Stars(S2, 45.0, 180.0, 4.0, 8.5, Buf2.data(), 600);
		Check(std::abs(N0 - N1) <= 3, "the same field twenty seconds on", N1, N0);
		if (N0 > 0 && N1 > 0)
		{
			const double Moved = std::atan(std::hypot(Buf[0].X - Buf2[0].X, Buf[0].Y - Buf2[0].Y)) * 180.0 / 3.14159265;
			Check(Moved > 0.01 && Moved < 0.2, "the sky turns ~15 arc seconds a second", Moved, 0.06);
		}
		Check(Stars(S, 45.0, 180.0, 4.0, LimitMagnitude(1, 10.0), Buf.data(), 600) == 0, "no stars by day");
		FSky M = S;
		SetMoon(M, 45.0, 180.0, 14.0, 1.0);
		Check(Stars(M, 45.0, 180.0, 4.0, 8.5, Buf.data(), 600) == 0, "no stars through the moon");
		(void)Total;
	}

	void TestPictures()
	{
		const FSky S = SkyAt(2026, 12, 25, 5.0);
		for (const EBody Body : { EBody::Venus, EBody::Mars, EBody::Jupiter, EBody::Saturn })
		{
			FImage Img;
			Draw(Body, S.Of(Body), 64, Img);
			int Solid = 0, Clear = 0;
			for (int I = 0; I < 64 * 64; ++I)
			{
				Solid += Img.Rgba[I * 4 + 3] > 200 ? 1 : 0;
				Clear += Img.Rgba[I * 4 + 3] == 0 ? 1 : 0;
			}
			Check(Solid > 150 && Clear > 400 && Img.Rgba[3] == 0, "a planet's picture: a disk on nothing", Solid, 1000);
			Check(Img.DiskPixels > 20.0, "the picture's scale", Img.DiskPixels, 50.0);
		}
		// Venus' lit part in the picture follows its phase
		{
			FBody V;
			V.PhaseAngleDeg = 90.0;
			FImage Img;
			Draw(EBody::Venus, V, 96, Img);
			int Left = 0, Right = 0;
			for (int Y = 0; Y < 96; ++Y)
			{
				for (int X = 0; X < 96; ++X)
				{
					(X < 48 ? Left : Right) += Img.Rgba[(Y * 96 + X) * 4 + 3] > 128 ? 1 : 0;
				}
			}
			Check(Right > 10 * (Left + 1), "a half Venus is lit on the sun's side", Right, Left);
		}
	}

	void WritePpm(const std::string& Path, int W, int H, const std::vector<uint8_t>& Rgb)
	{
		if (FILE* F = std::fopen(Path.c_str(), "wb"))
		{
			std::fprintf(F, "P6\n%d %d\n255\n", W, H);
			std::fwrite(Rgb.data(), 1, Rgb.size(), F);
			std::fclose(F);
		}
	}

	/** The view in the eyepiece as the game composes it: the dark sky, the field stars, the body with its moons. */
	void Eyepiece(const std::string& Path, const FSky& S, EBody Body, int Ep, double Knob)
	{
		const int N = 720;
		const double R = N * 0.46;
		const FBody& B = S.Of(Body);
		const double Field = LyudScope::Eyepiece(Ep).FieldDeg;
		const double Scale = R / std::tan(Field * 0.5 * 3.14159265 / 180.0);
		std::vector<float> Img(static_cast<size_t>(N) * N * 3, 0.f);
		const double BlurPx = std::max(0.0, Blur(Knob, 0.0, Field) * 2.0 * R);
		auto Splat = [&](double Cx, double Cy, double Radius, float Cr, float Cg, float Cb)
		{
			const double Rad = std::max(Radius, 0.8) + BlurPx * 0.5;
			const double Dim = (std::max(Radius, 0.8) * std::max(Radius, 0.8)) / (Rad * Rad);
			for (int Y = static_cast<int>(Cy - Rad - 2); Y <= static_cast<int>(Cy + Rad + 2); ++Y)
			{
				for (int X = static_cast<int>(Cx - Rad - 2); X <= static_cast<int>(Cx + Rad + 2); ++X)
				{
					if (X < 0 || Y < 0 || X >= N || Y >= N)
					{
						continue;
					}
					const double D = std::hypot(X + 0.5 - Cx, Y + 0.5 - Cy);
					const double A = std::max(0.0, std::min(1.0, Rad + 0.7 - D)) * Dim;
					float* P = &Img[(static_cast<size_t>(Y) * N + X) * 3];
					P[0] += static_cast<float>(Cr * A);
					P[1] += static_cast<float>(Cg * A);
					P[2] += static_cast<float>(Cb * A);
				}
			}
		};
		std::vector<FStar> Stars(600);
		const int Ns = LyudScope::Stars(S, B.AltDeg, B.AzDeg, Field, LimitMagnitude(Ep, S.SunAltDeg), Stars.data(), 600);
		const double Limit = LimitMagnitude(Ep, S.SunAltDeg);
		for (int I = 0; I < Ns; ++I)
		{
			const double Bright = std::min(1.0, 0.25 + 0.3 * (Limit - Stars[I].Magnitude));
			const double W = Stars[I].Warmth;
			Splat(N * 0.5 + Stars[I].X * Scale, N * 0.5 - Stars[I].Y * Scale, 0.7 + 0.35 * std::max(0.0, Limit - Stars[I].Magnitude - 1.0),
				static_cast<float>(Bright * (0.8 + 0.2 * W)), static_cast<float>(Bright * 0.9), static_cast<float>(Bright * (1.0 - 0.3 * W)));
		}
		// the body's picture, turned so that celestial north is where the eyepiece has it
		const double DiskPx = B.DiameterArcsec / 3600.0 * 3.14159265 / 180.0 * Scale;
		FImage Pic;
		const int PicSize = 192;
		Draw(Body, B, PicSize, Pic);
		const double North = B.ParallacticDeg * 3.14159265 / 180.0;
		// Venus and Mars are drawn with the sun on the right: turn that to the bright limb (north through east = counter-clockwise)
		const double Turn = (Body == EBody::Venus || Body == EBody::Mars) ? North + (B.BrightLimbDeg + 90.0) * 3.14159265 / 180.0 : North;
		const double PxPerTexel = DiskPx / Pic.DiskPixels;
		if (DiskPx > 2.5)
		{
			const double Half = PicSize * 0.5 * PxPerTexel;
			for (int Y = static_cast<int>(N * 0.5 - Half * 1.5); Y < static_cast<int>(N * 0.5 + Half * 1.5); ++Y)
			{
				for (int X = static_cast<int>(N * 0.5 - Half * 1.5); X < static_cast<int>(N * 0.5 + Half * 1.5); ++X)
				{
					if (X < 0 || Y < 0 || X >= N || Y >= N)
					{
						continue;
					}
					// screen (x right, y up) -> the picture's frame, turned back by Turn (counter-clockwise)
					const double Sx = X + 0.5 - N * 0.5, Sy = N * 0.5 - (Y + 0.5);
					const double U = (Sx * std::cos(Turn) + Sy * std::sin(Turn)) / PxPerTexel;
					const double V = (-Sx * std::sin(Turn) + Sy * std::cos(Turn)) / PxPerTexel;
					const int Tx = static_cast<int>(std::floor(U + PicSize * 0.5)), Ty = static_cast<int>(std::floor(PicSize * 0.5 - V));
					if (Tx < 0 || Ty < 0 || Tx >= PicSize || Ty >= PicSize)
					{
						continue;
					}
					const uint8_t* T = &Pic.Rgba[(static_cast<size_t>(Ty) * PicSize + Tx) * 4];
					const float A = T[3] / 255.f;
					float* P = &Img[(static_cast<size_t>(Y) * N + X) * 3];
					for (int C = 0; C < 3; ++C)
					{
						P[C] = P[C] * (1.f - A) + T[C] / 255.f * A;
					}
				}
			}
		}
		else
		{
			Splat(N * 0.5, N * 0.5, 1.6, 1.f, 0.97f, 0.85f);
		}
		if (Body == EBody::Jupiter)
		{
			for (int I = 0; I < 4; ++I)
			{
				if (!B.bMoonSeen[I])
				{
					continue;
				}
				// X toward west (the right with north up), Y toward north, turned with the eyepiece's north
				const double Mx = B.MoonX[I] * DiskPx * 0.5, My = B.MoonY[I] * DiskPx * 0.5;
				Splat(N * 0.5 + Mx * std::cos(North) - My * std::sin(North), N * 0.5 - (Mx * std::sin(North) + My * std::cos(North)), 1.1, 0.95f, 0.93f, 0.85f);
			}
		}
		std::vector<uint8_t> Rgb(static_cast<size_t>(N) * N * 3, 0);
		for (int Y = 0; Y < N; ++Y)
		{
			for (int X = 0; X < N; ++X)
			{
				const double D = std::hypot(X + 0.5 - N * 0.5, Y + 0.5 - N * 0.5);
				const double Mask = std::max(0.0, std::min(1.0, (R - D) / 3.0)) * (1.0 - 0.35 * std::pow(std::min(1.0, D / R), 6.0));
				for (int C = 0; C < 3; ++C)
				{
					const double Sky = C == 2 ? 0.035 : 0.02;
					Rgb[(static_cast<size_t>(Y) * N + X) * 3 + C] = static_cast<uint8_t>(255.0 * std::min(1.0, (Img[(static_cast<size_t>(Y) * N + X) * 3 + C] + Sky) * Mask));
				}
			}
		}
		WritePpm(Path, N, N, Rgb);
	}

	void Preview(const std::string& Dir)
	{
		const FSky Dec = SkyAt(2026, 12, 25, 4.0);
		const FSky Oct = SkyAt(2026, 10, 20, 19.5);
		const FSky Oct2 = SkyAt(2026, 10, 12, 16.0);
		struct FShot { const char* Name; const FSky* S; EBody Body; int Ep; double Knob; };
		const FShot Shots[] = {
			{ "jupiter_x60", &Dec, EBody::Jupiter, 2, InfinityKnob },
			{ "jupiter_x400", &Dec, EBody::Jupiter, 3, InfinityKnob },
			{ "jupiter_x60_blurred", &Dec, EBody::Jupiter, 2, InfinityKnob - 0.1 },
			{ "jupiter_x12", &Dec, EBody::Jupiter, 1, InfinityKnob },
			{ "saturn_x60", &Oct, EBody::Saturn, 2, InfinityKnob },
			{ "saturn_x400", &Oct, EBody::Saturn, 3, InfinityKnob },
			{ "venus_x400_december", &Dec, EBody::Venus, 3, InfinityKnob },
			{ "venus_x400_crescent", &Oct2, EBody::Venus, 3, InfinityKnob },
			{ "mars_x400", &Dec, EBody::Mars, 3, InfinityKnob },
		};
		for (const FShot& Shot : Shots)
		{
			Eyepiece(Dir + "/" + Shot.Name + ".ppm", *Shot.S, Shot.Body, Shot.Ep, Shot.Knob);
			std::printf("%s: %s %.1f\" lit %.2f mag %.1f\n", Shot.Name, BodyId(Shot.Body), Shot.S->Of(Shot.Body).DiameterArcsec,
				Shot.S->Of(Shot.Body).Lit, Shot.S->Of(Shot.Body).Magnitude);
		}
	}

	void Night(int Y, int M, int D)
	{
		std::printf("The night of %04d-%02d-%02d (local time, UTC+%g): altitude/azimuth, how well it shows\n", Y, M, D, UtcOffset);
		std::printf("hour   sun     venus          mars           jupiter        saturn         moons\n");
		for (double Hour = 16.0; Hour <= 32.0; Hour += 1.0)
		{
			FSky S = Sky(DayNumber(Y, M, D, Hour - UtcOffset), Lat, Lon);
			std::printf("%02d:00 %6.1f ", static_cast<int>(Hour) % 24, S.SunAltDeg);
			for (const EBody Body : { EBody::Venus, EBody::Mars, EBody::Jupiter, EBody::Saturn })
			{
				const FBody& B = S.Of(Body);
				std::printf(" %5.1f/%5.1f %3.1f", B.AltDeg, B.AzDeg, Visibility(S, Body));
			}
			std::printf("  %s\n", MoonsDiagram(S.Of(EBody::Jupiter)).c_str());
		}
		const FSky S = Sky(DayNumber(Y, M, D, 22.0 - UtcOffset), Lat, Lon);
		for (const EBody Body : { EBody::Venus, EBody::Mars, EBody::Jupiter, EBody::Saturn })
		{
			const FBody& B = S.Of(Body);
			std::printf("%-8s %5.1f\"  mag %5.1f  lit %.2f  %5.1f deg from the sun", BodyId(Body), B.DiameterArcsec, B.Magnitude, B.Lit, B.ElongationDeg);
			if (Body == EBody::Saturn)
			{
				std::printf("  ring %+.1f deg", B.RingTiltDeg);
			}
			std::printf("\n");
		}
	}
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "test";
	if (Mode == "night" && Argc >= 5)
	{
		Night(std::atoi(Argv[2]), std::atoi(Argv[3]), std::atoi(Argv[4]));
		return 0;
	}
	if (Mode == "preview" && Argc >= 3)
	{
		Preview(Argv[2]);
		return 0;
	}
	TestPlaces();
	TestLooks();
	TestInstrument();
	TestSights();
	TestLog();
	TestStars();
	TestPictures();
	std::printf("telescopesim: %d checks, %d failed\n", Passed + Failed, Failed);
	return Failed == 0 ? 0 : 1;
}
