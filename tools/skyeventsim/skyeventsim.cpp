// The sky events' model outside the engine: checks against dates everybody can look up, and a year's calendar. See run.sh.
#include "SkyEventsModel.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace LyudSkyEvents;
using LyudScope::DayNumber;

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
			std::printf("  FAIL %s (got %.4f, want %.4f)\n", What, Got, Want);
		}
	}

	void Near(double Got, double Want, double Tol, const char* What)
	{
		Check(std::fabs(Got - Want) <= Tol, What, Got, Want);
	}

	/** A day number as a date in local time. */
	std::string Date(double Day, bool bTime = true)
	{
		// Schlyter's day 0 is 1999-12-31 00:00 UT: a civil calendar walked from there
		double Local = Day + UtcOffset / 24.0;
		long N = static_cast<long>(std::floor(Local));
		const double Hours = (Local - N) * 24.0;
		int Y = 1999, M = 12, D = 31;
		static const int Len[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
		auto Leap = [](int Yr) { return (Yr % 4 == 0 && Yr % 100 != 0) || Yr % 400 == 0; };
		while (N > 0)
		{
			const int L = Len[M - 1] + (M == 2 && Leap(Y) ? 1 : 0);
			if (++D > L) { D = 1; if (++M > 12) { M = 1; ++Y; } }
			--N;
		}
		while (N < 0)
		{
			if (--D < 1) { if (--M < 1) { M = 12; --Y; } D = Len[M - 1] + (M == 2 && Leap(Y) ? 1 : 0); }
			++N;
		}
		char Buf[48];
		if (bTime) { std::snprintf(Buf, sizeof(Buf), "%04d-%02d-%02d %02d:%02d", Y, M, D, static_cast<int>(Hours), static_cast<int>(std::fmod(Hours, 1.0) * 60.0)); }
		else { std::snprintf(Buf, sizeof(Buf), "%04d-%02d-%02d", Y, M, D); }
		return Buf;
	}

	const char* Compass(double Az)
	{
		static const char* Names[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
		return Names[static_cast<int>(std::fmod(Az + 22.5 + 360.0, 360.0) / 45.0) % 8];
	}

	int Test()
	{
		// --- the sun's longitude out of the planets' model: the equinoxes and solstices of 2026
		Near(SunLongitude(LyudScope::Sky(DayNumber(2026, 3, 20, 14.77), Lat, Lon)), 0.0 + 360.0 * (SunLongitude(LyudScope::Sky(DayNumber(2026, 3, 20, 14.77), Lat, Lon)) > 180.0), 0.05, "the sun at 0 on the March equinox 2026");
		Near(SunLongitude(LyudScope::Sky(DayNumber(2026, 6, 21, 8.4), Lat, Lon)), 90.0, 0.05, "the sun at 90 on the June solstice 2026");
		Near(SunLongitude(LyudScope::Sky(DayNumber(2026, 12, 21, 20.83), Lat, Lon)), 270.0, 0.05, "the sun at 270 on the December solstice 2026");

		// --- showers: each peaks on its date, every year
		struct FWant { const char* Id; int Month, Day; };
		const FWant Want[] = { { "quadrantids", 1, 3 }, { "lyrids", 4, 22 }, { "eta_aquariids", 5, 6 }, { "perseids", 8, 12 },
			{ "orionids", 10, 21 }, { "leonids", 11, 17 }, { "geminids", 12, 14 }, { "ursids", 12, 22 } };
		Check(Showers().size() == 8, "eight showers", static_cast<double>(Showers().size()), 8.0);
		for (int Year = 2026; Year <= 2029; ++Year)
		{
			for (size_t I = 0; I < Showers().size(); ++I)
			{
				const FShower& Sh = Showers()[I];
				Check(std::string(Sh.Id) == Want[I].Id, "the showers' order");
				double BestDay = 0.0, Best = -1.0;
				for (double H = -96.0; H <= 96.0; H += 1.0)
				{
					const double Day = DayNumber(Year, Want[I].Month, Want[I].Day, 12.0) + H / 24.0;
					const double Rate = ZenithRate(Sh, SunLongitude(LyudScope::Sky(Day, Lat, Lon)));
					if (Rate > Best) { Best = Rate; BestDay = Day; }
				}
				Near(BestDay, DayNumber(Year, Want[I].Month, Want[I].Day, 12.0), 1.3, Sh.Id);
				Near(Best, Sh.Zhr, Sh.Zhr * 0.12, "its rate at the maximum");
				// a month away there is nothing of it
				Check(ZenithRate(Sh, SunLongitude(LyudScope::Sky(BestDay + 45.0, Lat, Lon))) == 0.0, "nothing of it six weeks later");
			}
		}
		// --- what one counts: the Perseids' night of 2026 (new moon: a perfect one), against an ordinary August night
		{
			double Peak = 0.0, Dusk = 0.0, Noon = 0.0;
			for (double H = 18.0; H <= 30.0; H += 0.25)
			{
				const LyudScope::FSky S = LyudScope::Sky(DayNumber(2026, 8, 12, H), Lat, Lon);
				const FMeteors M = Meteors(S, 0.0, -30.0, 0.0);
				Peak = std::max(Peak, M.ShowerPerHour);
				if (H == 18.0) { Dusk = M.ShowerPerHour + M.SporadicPerHour; }
				Check(M.Shower == 3 || M.ShowerPerHour == 0.0, "the Perseids are the night's shower");
				Check(M.LimitMagnitude <= 6.5 + 1e-9 && M.LimitMagnitude >= 2.0 - 1e-9, "the limit in 2..6.5", M.LimitMagnitude, 6.5);
			}
			Noon = Meteors(LyudScope::Sky(DayNumber(2026, 8, 12, 10.0), Lat, Lon), 0.0, -30.0, 0.0).ShowerPerHour;
			Check(Peak > 40.0 && Peak <= 100.0, "the Perseids before dawn: 40 to 100 an hour", Peak, 70.0);
			Check(Dusk < 2.0, "nothing counted at 20:00 in August (the sun is up)", Dusk, 0.0);
			Check(Noon == 0.0, "none by day", Noon, 0.0);
			const LyudScope::FSky S = LyudScope::Sky(DayNumber(2026, 8, 12, 23.5), Lat, Lon);
			const double Clear = Meteors(S, 0.0, -30.0, 0.0).ShowerPerHour;
			const double MoonLit = Meteors(S, 1.0, 40.0, 0.0).ShowerPerHour;
			const double Cloudy = Meteors(S, 0.0, -30.0, 0.8).ShowerPerHour;
			const double Overcast = Meteors(S, 0.0, -30.0, 1.0).ShowerPerHour;
			Check(MoonLit < 0.25 * Clear && MoonLit > 0.05 * Clear, "a full moon leaves a sixth of them", MoonLit, Clear / 6.0);
			Near(Cloudy, 0.2 * Clear, 1e-6, "clouds hide their share");
			Check(Overcast == 0.0, "none under an overcast");
			const FMeteors Plain = Meteors(LyudScope::Sky(DayNumber(2026, 9, 20, 23.5), Lat, Lon), 0.0, -30.0, 0.0);
			Check(Plain.ShowerPerHour == 0.0 && Plain.Shower == -1, "no shower on a September night");
			Check(Plain.SporadicPerHour > 3.0 && Plain.SporadicPerHour < 14.0, "a few sporadic ones an hour", Plain.SporadicPerHour, 8.0);
			const double Evening = Meteors(LyudScope::Sky(DayNumber(2026, 11, 5, 18.0), Lat, Lon), 0.0, -30.0, 0.0).SporadicPerHour;
			const double Morning = Meteors(LyudScope::Sky(DayNumber(2026, 11, 6, 3.0), Lat, Lon), 0.0, -30.0, 0.0).SporadicPerHour;
			Check(Morning > 1.5 * Evening, "more sporadic ones before dawn than in the evening", Morning, Evening);
			// the Eta Aquariids are a southern shower: few for us even at their best
			double Eta = 0.0;
			for (double H = 18.0; H <= 30.0; H += 0.25)
			{
				Eta = std::max(Eta, Meteors(LyudScope::Sky(DayNumber(2026, 5, 5, H), Lat, Lon), 0.0, -30.0, 0.0).ShowerPerHour);
			}
			Check(Eta < 10.0, "the Eta Aquariids stay low at 55 north", Eta, 5.0);
		}

		// --- the moon: syzygies everybody can look up
		Near(NextPhase(DayNumber(2000, 1, 1, 0.0), false), DayNumber(2000, 1, 6, 18.23), 0.08, "new moon 2000-01-06 18:14 UT");
		Near(NextPhase(DayNumber(2000, 1, 10, 0.0), true), DayNumber(2000, 1, 21, 4.67), 0.08, "full moon 2000-01-21 04:40 UT (the eclipse)");
		Near(NextPhase(DayNumber(2026, 8, 1, 0.0), false), DayNumber(2026, 8, 12, 17.6), 0.08, "new moon 2026-08-12 17:37 UT (the eclipse)");
		Near(NextPhase(DayNumber(2024, 4, 1, 0.0), false), DayNumber(2024, 4, 8, 18.35), 0.08, "new moon 2024-04-08 18:21 UT (the eclipse)");
		Near(NextPhase(DayNumber(2025, 3, 1, 0.0), true), DayNumber(2025, 3, 14, 6.92), 0.08, "full moon 2025-03-14 06:55 UT (the eclipse)");
		// at an eclipse the moon is on the ecliptic
		Check(std::fabs(Moon(DayNumber(2026, 8, 12, 17.6)).LatDeg) < 1.0, "the moon near its node at the eclipse of 2026-08-12", Moon(DayNumber(2026, 8, 12, 17.6)).LatDeg, 0.0);
		Check(std::fabs(Moon(DayNumber(2025, 3, 14, 6.92)).LatDeg) < 0.6, "the moon near its node at the eclipse of 2025-03-14", Moon(DayNumber(2025, 3, 14, 6.92)).LatDeg, 0.0);
		{
			// a year of months: 29.3 to 29.8 days each, the lit part and the names follow, the libration stays in its bounds
			double Prev = NextPhase(DayNumber(2026, 10, 20, 0.0), false);
			double MinLon = 0.0, MaxLon = 0.0, MinLat = 0.0, MaxLat = 0.0;
			for (int M = 0; M < 13; ++M)
			{
				const double Next = NextPhase(Prev + 1.0, false);
				Check(Next - Prev > 29.2 && Next - Prev < 29.9, "a month of 29.2 to 29.9 days", Next - Prev, 29.53);
				const double Full = NextPhase(Prev + 1.0, true);
				Check(Full > Prev + 13.5 && Full < Prev + 16.0, "the full moon in its middle", Full - Prev, 14.77);
				Check(Moon(Prev).Lit < 0.01 && std::string(PhaseId(Moon(Prev))) == "new", "new: dark");
				Check(Moon(Full).Lit > 0.99 && std::string(PhaseId(Moon(Full))) == "full", "full: lit");
				Check(std::string(PhaseId(Moon(Prev + 3.5))) == "waxing_crescent", "a waxing crescent three days in");
				Check(std::string(PhaseId(Moon(Full + 4.0))) == "waning_gibbous", "a waning gibbous four days after full");
				Check(Moon(Prev + 5.0).bWaxing && !Moon(Full + 5.0).bWaxing, "waxing before full, waning after");
				Near(Moon(Prev + 7.4).Lit, 0.5, 0.12, "half lit a week in");
				for (double D = Prev; D < Next; D += 0.25)
				{
					const FMoon Mo = Moon(D);
					MinLon = std::min(MinLon, Mo.LibrationLonDeg); MaxLon = std::max(MaxLon, Mo.LibrationLonDeg);
					MinLat = std::min(MinLat, Mo.LibrationLatDeg); MaxLat = std::max(MaxLat, Mo.LibrationLatDeg);
					Check(std::fabs(Mo.LatDeg) < 5.4, "the moon within 5.3 degrees of the ecliptic", Mo.LatDeg, 5.3);
					Check(Mo.AgeDays >= 0.0 && Mo.AgeDays < 29.54, "its age in the month", Mo.AgeDays, 15.0);
				}
				Prev = Next;
			}
			Check(MaxLon > 6.0 && MaxLon < 8.3 && MinLon < -6.0 && MinLon > -8.3, "the libration in longitude swings 6 to 8 degrees each way", MaxLon, 7.9);
			Check(MaxLat > 6.0 && MaxLat < 7.0 && MinLat < -6.0 && MinLat > -7.0, "the libration in latitude swings 6 to 7 degrees each way", MaxLat, 6.7);
		}

		// --- satellites
		{
			const double From = DayNumber(2026, 10, 20, 0.0);
			const std::vector<FPass> Month = Passes(From, From + 30.0, Lat, Lon);
			int PerSat[3] = { 0, 0, 0 };
			int Fades = 0;
			for (const FPass& P : Month)
			{
				PerSat[P.Satellite]++;
				Fades += P.bFades ? 1 : 0;
				const double Minutes = (P.To - P.From) * 1440.0;
				Check(Minutes >= 0.0 && Minutes < 16.0, "a pass lasts minutes", Minutes, 5.0);
				Check(P.PeakAltDeg > 10.0 && P.PeakAltDeg <= 90.0, "it climbs above 10 degrees", P.PeakAltDeg, 40.0);
				Check(P.PeakDay >= P.From && P.PeakDay <= P.To, "the highest point within the pass");
				const LyudScope::FSky S = LyudScope::Sky(P.PeakDay, Lat, Lon);
				Check(S.SunAltDeg < -6.0, "seen in a dark sky", S.SunAltDeg, -6.0);
				const FSatNow W = Where(Satellites()[static_cast<size_t>(P.Satellite)], S);
				Check(W.bSeen && W.bSunlit, "sunlit at its highest");
				Check(W.RangeKm >= Satellites()[static_cast<size_t>(P.Satellite)].HeightKm - 1.0, "never nearer than its height", W.RangeKm, 400.0);
			}
			Check(Month.size() >= 20 && Month.size() < 400, "a month has dozens of passes", static_cast<double>(Month.size()), 60.0);
			Check(PerSat[0] > 3 && PerSat[1] > 3 && PerSat[2] > 3, "each of the three is seen in a month", PerSat[0], 10.0);
			Check(Fades > 0, "some go out in the earth's shadow");
			// the low station is in the earth's shadow at midnight in winter: nothing of it then
			int Midnight = 0;
			for (const FPass& P : Passes(DayNumber(2026, 12, 10, 0.0), DayNumber(2026, 12, 25, 0.0), Lat, Lon))
			{
				const double LocalHour = std::fmod(std::fmod(P.PeakDay, 1.0) * 24.0 + UtcOffset + 24.0, 24.0);
				Midnight += P.Satellite == 0 && (LocalHour > 22.5 || LocalHour < 1.5) ? 1 : 0;
			}
			Check(Midnight == 0, "the low station is never seen around midnight in December", Midnight, 0.0);
			// the same question, the same answer
			const std::vector<FPass> Again = Passes(From, From + 30.0, Lat, Lon);
			Check(Again.size() == Month.size() && (Month.empty() || Again[0].From == Month[0].From), "the passes are a function of the date");
			// the orbit: 51.6 degrees never comes over the pole, the polar one does
			double MaxMirAlt = -90.0;
			for (double D = From; D < From + 2.0; D += 30.0 / 86400.0)
			{
				MaxMirAlt = std::max(MaxMirAlt, Where(Satellites()[0], LyudScope::Sky(D, 89.0, 0.0)).AltDeg);
			}
			Check(MaxMirAlt < 0.0, "the station is never above the pole's horizon", MaxMirAlt, -5.0);
		}

		// --- conjunctions everybody can look up
		{
			// the great conjunction: Jupiter and Saturn a tenth of a degree apart on 21 December 2020, low in the evening
			const std::vector<FConjunction> Great = Conjunctions(DayNumber(2020, 12, 1, 0.0), DayNumber(2021, 1, 10, 0.0), 30.0, 20.4, 3.0);
			bool bFound = false;
			for (const FConjunction& C : Great)
			{
				if (C.A == LyudScope::EBody::Jupiter && C.B == LyudScope::EBody::Saturn)
				{
					bFound = true;
					Near(C.Day, DayNumber(2020, 12, 21, 16.0), 1.2, "the great conjunction on 21 December 2020");
					Check(C.SeparationDeg < 0.25, "a tenth of a degree apart", C.SeparationDeg, 0.1);
					Check(C.bEvening, "in the evening sky");
				}
			}
			Check(bFound, "Jupiter and Saturn found in December 2020");
			// Venus and Jupiter in the evening of 9 June 2026
			bFound = false;
			for (const FConjunction& C : Conjunctions(DayNumber(2026, 5, 20, 0.0), DayNumber(2026, 6, 30, 0.0), 30.0, 20.4, 3.0))
			{
				if (C.A == LyudScope::EBody::Venus && C.B == LyudScope::EBody::Jupiter)
				{
					bFound = true;
					Near(C.Day, DayNumber(2026, 6, 9, 18.0), 1.5, "Venus and Jupiter on 9 June 2026");
					Check(C.SeparationDeg < 2.2 && C.SeparationDeg > 1.0, "a degree and a half apart", C.SeparationDeg, 1.6);
					Check(C.bEvening, "in the evening sky");
				}
			}
			Check(bFound, "Venus and Jupiter found in June 2026");
			// whatever the game's first year has: every one is two planets under the limit in a dark sky
			const std::vector<FConjunction> Year = Conjunctions(DayNumber(2026, 10, 20, 0.0), DayNumber(2027, 10, 20, 0.0), Lat, Lon, 3.0);
			for (const FConjunction& C : Year)
			{
				const LyudScope::FSky S = LyudScope::Sky(C.Day, Lat, Lon);
				Check(S.SunAltDeg < -6.0 && S.Of(C.A).AltDeg > 5.0 && S.Of(C.B).AltDeg > 5.0, "seen: dark, both up");
				Near(LyudScope::Separation(S.Of(C.A).AltDeg, S.Of(C.A).AzDeg, S.Of(C.B).AltDeg, S.Of(C.B).AzDeg), C.SeparationDeg, 1e-6, "its separation");
				Check(C.From <= C.Day && C.Day <= C.To && C.SeparationDeg < 3.0, "closest within its nights");
			}
		}
		std::printf("skyeventsim: %d checks, %d failed\n", Passed + Failed, Failed);
		return Failed ? 1 : 0;
	}

	int Year(int Y, int M, int D, int Days)
	{
		const double From = DayNumber(Y, M, D, 0.0) - UtcOffset / 24.0;
		std::printf("The sky from %04d-%02d-%02d, %d days, at %.2f N %.2f E (local time, UTC+%.0f)\n\n", Y, M, D, Days, Lat, Lon, UtcOffset);
		std::printf("Meteor showers (the best hour of the best night, a clear sky, the real moon's light)\n");
		for (size_t I = 0; I < Showers().size(); ++I)
		{
			double BestDay = 0.0, Best = -1.0;
			FMoon BestMoon;
			for (double Day = From; Day < From + Days; Day += 1.0 / 48.0)
			{
				const LyudScope::FSky S = LyudScope::Sky(Day, Lat, Lon);
				if (S.SunAltDeg > -6.0 || ZenithRate(Showers()[I], SunLongitude(S)) < 0.3 * Showers()[I].Zhr) { continue; }
				const FMoon Mo = Moon(Day);
				// the real moon's height is not worked out here: up for half the night on average, by its phase
				const FMeteors Me = Meteors(S, Mo.Lit, Mo.Lit > 0.5 ? 30.0 : -10.0, 0.0);
				if (Me.Shower == static_cast<int>(I) && Me.ShowerPerHour > Best) { Best = Me.ShowerPerHour; BestDay = Day; BestMoon = Mo; }
			}
			if (Best >= 0.0)
			{
				std::printf("  %-14s %s  %3.0f an hour  (moon %s, %2.0f%% lit)\n", Showers()[I].Id, Date(BestDay).c_str(), Best, PhaseId(BestMoon), BestMoon.Lit * 100.0);
			}
		}
		std::printf("\nMoon\n");
		for (double Day = From; Day < From + Days;)
		{
			const double New = NextPhase(Day, false);
			const double Full = NextPhase(New, true);
			const FMoon AtFull = Moon(Full);
			std::printf("  new %s   full %s   libration at full: lon %+5.1f lat %+5.1f\n", Date(New).c_str(), Date(Full).c_str(), AtFull.LibrationLonDeg, AtFull.LibrationLatDeg);
			Day = New + 20.0;
		}
		std::printf("\nPlanets side by side (within 3 degrees, dark sky)\n");
		for (const FConjunction& C : Conjunctions(From, From + Days, Lat, Lon, 3.0))
		{
			std::printf("  %-8s %-8s closest %s  %.1f deg apart, %2.0f deg up, %s; nights %s .. %s\n", LyudScope::BodyId(C.A), LyudScope::BodyId(C.B), Date(C.Day).c_str(),
				C.SeparationDeg, C.AltDeg, C.bEvening ? "evening" : "before dawn", Date(C.From, false).c_str(), Date(C.To, false).c_str());
		}
		std::printf("\nSatellites: the first ten days' passes above 30 degrees\n");
		for (const FPass& P : Passes(From, From + std::min(Days, 10), Lat, Lon))
		{
			if (P.PeakAltDeg < 30.0) { continue; }
			std::printf("  %-7s %s  %4.1f min  %s -> %2.0f deg %s -> %s%s\n", Satellites()[static_cast<size_t>(P.Satellite)].Id, Date(P.From).c_str(), (P.To - P.From) * 1440.0,
				Compass(P.FromAzDeg), P.PeakAltDeg, Compass(P.PeakAzDeg), Compass(P.ToAzDeg), P.bFades ? " (fades in the shadow)" : "");
		}
		return 0;
	}
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "test";
	if (Mode == "test")
	{
		return Test();
	}
	if (Mode == "year")
	{
		const int Y = Argc > 4 ? std::atoi(Argv[2]) : 2026, M = Argc > 4 ? std::atoi(Argv[3]) : 10, D = Argc > 4 ? std::atoi(Argv[4]) : 20;
		return Year(Y, M, D, Argc > 5 ? std::atoi(Argv[5]) : 365);
	}
	std::printf("modes: test | year [YYYY MM DD [days]]\n");
	return 2;
}
