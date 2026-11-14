// The town's festival calendar outside the engine (Private/Festival/FestivalCalendar.h, docs/festivals.md).
//   festivalsim test                 checks of the date rules and of festivals.json
//   festivalsim year [YYYY]          every festival day of a year with its program
//   festivalsim at YYYY-MM-DD HH.H   what is on at a moment
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Festival/FestivalCalendar.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <tuple>
#include <vector>

using namespace LyudFest;

static int Fails = 0, Passes = 0;
// one line per check: "ok   <what>" or "FAIL <what>"
#define CHECK(C, ...) do { if (!(C)) { Fails++; std::printf("FAIL "); std::printf(__VA_ARGS__); std::printf("  (%s:%d)\n", __FILE__, __LINE__); } else { Passes++; std::printf("ok   "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static std::string ReadFile(const char* Path)
{
	std::ifstream F(Path);
	std::stringstream S;
	S << F.rdbuf();
	return S.str();
}

static const char* DayName(int W)
{
	static const char* N[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
	return N[W % 7];
}

static std::string Hours(float H)
{
	char B[16];
	const int M = (int)(H * 60.f + 0.5f);
	std::snprintf(B, sizeof(B), "%02d:%02d", (M / 60) % 24, M % 60);
	return B;
}

static void PrintAt(const FCalendar& C, const FDate& D, float H)
{
	std::string Off;
	const bool bOff = C.IsDayOff(D, &Off);
	std::printf("%04d-%02d-%02d %s %s%s\n", D.Y, D.M, D.D, DayName(Weekday(D)), Hours(H).c_str(), bOff ? (" day off (" + Off + ")").c_str() : "");
	for (const FActive& A : C.At(D, H))
	{
		const FFestival& F = C.Festivals[A.Festival];
		std::printf("  %-15s since %04d-%02d-%02d rel %.2f h", F.Id.c_str(), A.Start.Y, A.Start.M, A.Start.D, A.Rel);
		if (A.Phase >= 0)
		{
			const FPhase& P = F.Phases[A.Phase];
			std::printf("  phase %s at %s (%s, pull %.1f) %.0f%%", P.Id.c_str(), P.Venue.c_str(), P.Mode.c_str(), P.Pull, A.PhaseT * 100.f);
		}
		else
		{
			std::printf("  decorations only");
		}
		std::printf("\n");
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// the deeper checks: 2026..2060 against published tables and independent algorithms, the program's hours as the game and
// the people's life use them, and festivals meeting at one venue (ULyudFestivalSubsystem::Update's claims)

static const int Y0 = 2026, Y1 = 2060;

static std::string DateStr(const FDate& D)
{
	char B[16];
	std::snprintf(B, sizeof(B), "%04d-%02d-%02d", D.Y, D.M, D.D);
	return B;
}

static bool Valid(const FDate& D)
{
	return D.M >= 1 && D.M <= 12 && D.D >= 1 && D.D <= DaysInMonth(D.Y, D.M);
}

/** Gauss's Easter for the Julian calendar, moved to the Gregorian (13 days in 1900-2099): independent of OrthodoxEaster. */
static FDate GaussOrthodox(int Y)
{
	const int A = Y % 19, B = Y % 4, C = Y % 7;
	const int D = (19 * A + 15) % 30, E = (2 * B + 4 * C + 6 * D + 6) % 7;
	return AddDays({ Y, 3, 22 }, D + E + 13);
}

/** Gauss's Gregorian Easter with the two exceptions (26 April -> 19, 25 April -> 18 in some years). */
static FDate GaussWestern(int Y)
{
	const int A = Y % 19, B = Y % 4, C = Y % 7, K = Y / 100;
	const int P = (13 + 8 * K) / 25, Q = K / 4;
	const int M = (15 - P + K - Q) % 30, N = (4 + K - Q) % 7;
	int D = (19 * A + M) % 30;
	const int E = (2 * B + 4 * C + 6 * D + N) % 7;
	if (D == 29 && E == 6) return { Y, 4, 19 };
	if (D == 28 && E == 6 && (11 * M + 11) % 30 < 19) return { Y, 4, 18 };
	return AddDays({ Y, 3, 22 }, D + E);
}

/** The starts of festival F whose first day is in year Y. */
static std::vector<FDate> StartsIn(const FCalendar& C, int F, int Y)
{
	std::vector<FDate> Out;
	const int D0 = DaysFromCivil({ Y, 1, 1 }), D1 = DaysFromCivil({ Y + 1, 1, 1 });
	for (int D = D0; D < D1; D++)
	{
		if (C.StartsOn(F, CivilFromDays(D))) Out.push_back(CivilFromDays(D));
	}
	return Out;
}

/** A copy of LyudPeople::FWorld::InHours (PeopleSim.cpp): a gathering's hours as the people's life reads them. */
static bool PeopleInHours(float From, float To, float H)
{
	if (To - From >= 24.f || To == From) return true;
	From = std::fmod(From + 48.f, 24.f);
	To = std::fmod(To + 48.f, 24.f);
	if (From <= To) return H >= From && H < To;
	return H >= From || H < To;
}

/** ULyudFestivalSubsystem::Update's venue claims at one moment (LyudFestival.cpp, "auto Claim"): which festivals are on
 *  (something of theirs shows), and which phases run (their venue was theirs). */
struct FClaimed
{
	std::vector<int> On;                        // festivals with bAny
	std::vector<std::pair<int, int>> Ran;       // (festival, phase) that won its venue
	std::vector<std::tuple<int, int, int>> Lost;    // (festival, phase, holder) running by the calendar but its venue taken
};

static FClaimed Claims(const FCalendar& C, const FDate& D, float H)
{
	FClaimed Out;
	std::map<std::string, int> Claimed;
	for (const FActive& A : C.At(D, H))
	{
		const FFestival& F = C.Festivals[A.Festival];
		int Holder = -1;
		auto Claim = [&](const std::string& V)
		{
			const auto It = Claimed.find(V);
			Holder = It != Claimed.end() ? It->second : A.Festival;
			if (It != Claimed.end()) return It->second == A.Festival;
			Claimed[V] = A.Festival;
			return true;
		};
		std::vector<std::string> DV = F.DecorVenues;
		if (DV.empty())
		{
			for (const FPhase& P : F.Phases)
			{
				if (std::find(DV.begin(), DV.end(), P.Venue) == DV.end()) DV.push_back(P.Venue);
			}
		}
		bool bAny = false;
		for (const std::string& V : DV)
		{
			if (!Claim(V) || F.Decor.empty()) continue;
			const std::string StageAt = std::find(DV.begin(), DV.end(), "square") != DV.end() ? "square" : DV[0];
			const std::string StallsAt = std::find(DV.begin(), DV.end(), "fair") != DV.end() ? "fair" : DV[0];
			for (const std::string& K : F.Decor)
			{
				if (((K == "stage" || K == "tree") && V != StageAt) || (K == "stalls" && V != StallsAt)) continue;
				bAny = true;
			}
		}
		for (const int Pi : A.Phases)
		{
			if (!Claim(F.Phases[Pi].Venue))
			{
				Out.Lost.push_back({ A.Festival, Pi, Holder });
				continue;
			}
			bAny = true;
			Out.Ran.push_back({ A.Festival, Pi });
		}
		if (bAny) Out.On.push_back(A.Festival);
	}
	return Out;
}

static void Deep(FCalendar& C)
{
	// ---- Easter: the code against Gauss's formulas and a published table
	{
		std::string Bad;
		for (int Y = Y0; Y <= Y1; Y++)
		{
			if (!(OrthodoxEaster(Y) == GaussOrthodox(Y))) Bad += " O" + std::to_string(Y) + "=" + DateStr(OrthodoxEaster(Y)) + "/" + DateStr(GaussOrthodox(Y));
			if (!(WesternEaster(Y) == GaussWestern(Y))) Bad += " W" + std::to_string(Y) + "=" + DateStr(WesternEaster(Y)) + "/" + DateStr(GaussWestern(Y));
			if (Weekday(OrthodoxEaster(Y)) != 6 || Weekday(WesternEaster(Y)) != 6) Bad += " not-Sunday-" + std::to_string(Y);
		}
		CHECK(Bad.empty(), "Easter 2026..2060 (both churches) agrees with Gauss's algorithm and is a Sunday%s", Bad.c_str());
		// the published dates (Orthodox, Western), 2026-2040
		static const int T[][5] = {
			{ 2026, 4, 12, 4, 5 }, { 2027, 5, 2, 3, 28 }, { 2028, 4, 16, 4, 16 }, { 2029, 4, 8, 4, 1 }, { 2030, 4, 28, 4, 21 },
			{ 2031, 4, 13, 4, 13 }, { 2032, 5, 2, 3, 28 }, { 2033, 4, 24, 4, 17 }, { 2034, 4, 9, 4, 9 }, { 2035, 4, 29, 3, 25 },
			{ 2036, 4, 20, 4, 13 }, { 2037, 4, 5, 4, 5 }, { 2038, 4, 25, 4, 25 }, { 2039, 4, 17, 4, 10 }, { 2040, 5, 6, 4, 1 } };
		Bad.clear();
		for (const auto& R : T)
		{
			if (!(OrthodoxEaster(R[0]) == (FDate{ R[0], R[1], R[2] })) || !(WesternEaster(R[0]) == (FDate{ R[0], R[3], R[4] }))) Bad += " " + std::to_string(R[0]);
		}
		CHECK(Bad.empty(), "Easter 2026..2040 matches the published table%s", Bad.c_str());
	}

	// ---- the moon against eclipses (a solar eclipse is a new moon, a lunar one a full moon), UTC dates
	{
		struct FM { int Y, M, D; double Phase; };
		static const FM T[] = {
			{ 2026, 8, 12, 0.0 }, { 2027, 2, 6, 0.0 }, { 2027, 8, 2, 0.0 }, { 2028, 1, 26, 0.0 }, { 2028, 7, 22, 0.0 },
			{ 2030, 6, 1, 0.0 }, { 2030, 11, 25, 0.0 }, { 2033, 3, 30, 0.0 }, { 2034, 3, 20, 0.0 }, { 2035, 9, 2, 0.0 },
			{ 2026, 3, 3, 0.5 }, { 2028, 12, 31, 0.5 }, { 2029, 6, 26, 0.5 }, { 2029, 12, 20, 0.5 }, { 2032, 4, 25, 0.5 },
			{ 2033, 4, 14, 0.5 }, { 2040, 5, 26, 0.5 } };
		std::string Bad;
		for (const FM& M : T)
		{
			const FDate Want{ M.Y, M.M, M.D };
			if (!(MoonOnOrAfter(AddDays(Want, -10), M.Phase, 0.f) == Want)) Bad += " " + DateStr(Want) + "->" + DateStr(MoonOnOrAfter(AddDays(Want, -10), M.Phase, 0.f));
		}
		CHECK(Bad.empty(), "new and full moons on the dates of 17 eclipses 2026-2040%s", Bad.c_str());
		// consecutive moons 2026..2060 are 29-30 days apart, never skipped
		Bad.clear();
		for (const double Ph : { 0.0, 0.5 })
		{
			FDate D = MoonOnOrAfter({ Y0, 1, 1 }, Ph, 2.f);
			while (D.Y <= Y1)
			{
				const FDate N = MoonOnOrAfter(AddDays(D, 1), Ph, 2.f);
				const int Gap = DaysFromCivil(N) - DaysFromCivil(D);
				if (Gap < 29 || Gap > 30) Bad += " " + DateStr(D) + "+" + std::to_string(Gap);
				D = N;
			}
		}
		CHECK(Bad.empty(), "consecutive new / full moons 2026..2060 are 29 or 30 days apart%s", Bad.c_str());
	}

	// ---- the moon festivals against their published dates
	{
		auto Table = [&](const char* Id, const std::vector<FDate>& Want)
		{
			const int F = C.Find(Id);
			std::string Bad;
			for (const FDate& W : Want)
			{
				const std::vector<FDate> S = F >= 0 ? StartsIn(C, F, W.Y) : std::vector<FDate>();
				if (S.size() != 1 || !(S[0] == W)) Bad += " " + DateStr(W) + "(got " + (S.empty() ? std::string("none") : DateStr(S[0])) + ")";
			}
			CHECK(F >= 0 && Bad.empty(), "%s on its published dates %d..%d%s", Id, Want.front().Y, Want.back().Y, Bad.c_str());
		};
		Table("lunar_new_year", { { 2026, 2, 17 }, { 2027, 2, 6 }, { 2028, 1, 26 }, { 2029, 2, 13 }, { 2030, 2, 3 }, { 2031, 1, 23 },
			{ 2032, 2, 11 }, { 2033, 1, 31 }, { 2034, 2, 19 }, { 2035, 2, 8 }, { 2036, 1, 28 }, { 2037, 2, 15 }, { 2038, 2, 4 },
			{ 2039, 1, 24 }, { 2040, 2, 12 }, { 2041, 2, 1 }, { 2042, 1, 22 }, { 2043, 2, 10 }, { 2044, 1, 30 }, { 2045, 2, 17 },
			{ 2046, 2, 6 }, { 2047, 1, 26 }, { 2048, 2, 14 }, { 2049, 2, 2 }, { 2050, 1, 23 } });
		Table("mid_autumn", { { 2026, 9, 25 }, { 2027, 9, 15 }, { 2028, 10, 3 }, { 2029, 9, 22 }, { 2030, 9, 12 }, { 2031, 10, 1 },
			{ 2032, 9, 19 }, { 2033, 9, 8 }, { 2034, 9, 27 }, { 2035, 9, 16 } });
		Table("diwali", { { 2026, 11, 8 }, { 2027, 10, 29 }, { 2028, 10, 17 }, { 2029, 11, 5 }, { 2030, 10, 26 } });
		// (Thailand's calendar is arithmetic: with the full moon in Thailand's morning it may keep the day before, as on
		// 2024-11-15; the rule follows the astronomical moon, so only years whose full moon is in Thailand's afternoon)
		Table("loy_krathong", { { 2026, 11, 24 }, { 2028, 11, 2 }, { 2030, 11, 10 } });
		Table("holi", { { 2027, 3, 22 }, { 2028, 3, 11 }, { 2029, 3, 1 }, { 2030, 3, 20 } });
	}

	// ---- how often: once a year (Saturdays for the market, the fired days for a trigger), on a real date, 2026..2060
	{
		std::string Count, Range;
		int Starts = 0;
		for (int F = 0; F < (int)C.Festivals.size(); F++)
		{
			const FFestival& Fe = C.Festivals[F];
			for (int Y = Y0; Y <= Y1; Y++)
			{
				const std::vector<FDate> S = StartsIn(C, F, Y);
				Starts += (int)S.size();
				size_t Want = 1;
				if (Fe.Rule == "weekly")
				{
					Want = 0;
					for (int D = DaysFromCivil({ Y, 1, 1 }); D < DaysFromCivil({ Y + 1, 1, 1 }); D++)
					{
						const FDate Dd = CivilFromDays(D);
						Want += std::find(Fe.Weekdays.begin(), Fe.Weekdays.end(), Weekday(Dd)) != Fe.Weekdays.end() &&
							(Fe.Months.empty() || std::find(Fe.Months.begin(), Fe.Months.end(), Dd.M) != Fe.Months.end());
					}
				}
				else if (Fe.Rule == "trigger")
				{
					Want = 0;
					for (const FDate& T : C.Fired[Fe.Trigger]) Want += AddDays(T, Fe.Offset).Y == Y;
				}
				if (S.size() != Want)
				{
					Count += " " + Fe.Id + "@" + std::to_string(Y) + "=" + std::to_string(S.size());
				}
				for (const FDate& D : S)
				{
					// the date rule's own day (the shift undone) is where the rule says
					const FDate R = AddDays(D, -Fe.Offset);
					bool bOk = Valid(D);
					if (Fe.Rule == "date") bOk = bOk && R.M == Fe.Month && R.D == Fe.DayOfMonth;
					if (Fe.Rule == "nth_weekday") bOk = bOk && R.M == Fe.Month && Weekday(R) == Fe.Wd && (Fe.Nth > 0 ? (R.D - 1) / 7 + 1 == Fe.Nth : R.D + 7 > DaysInMonth(R.Y, R.M));
					if (Fe.Rule == "easter_orthodox") bOk = bOk && AddDays(OrthodoxEaster(Y), Fe.Offset) == D;
					if (Fe.Rule == "easter_western") bOk = bOk && AddDays(WesternEaster(Y), Fe.Offset) == D;
					if (Fe.Rule == "new_moon" || Fe.Rule == "full_moon")
					{
						const FDate From{ R.Y, Fe.Month, Fe.DayOfMonth };
						const int In = DaysFromCivil(R) - DaysFromCivil(From);
						bOk = bOk && In >= 0 && In < Fe.Window;
					}
					if (!bOk) Range += " " + Fe.Id + "@" + DateStr(D);
				}
			}
		}
		CHECK(Count.empty(), "every festival starts once a year 2026..2060 (the market every Saturday, a trigger when fired; %d starts)%s", Starts, Count.c_str());
		CHECK(Range.empty(), "every start 2026..2060 is a real date on its rule's day (month/day, Nth weekday, Easter + offset, the moon within its window)%s", Range.c_str());
	}

	// ---- the dates' bounds a player would know
	{
		auto Between = [&](const char* Id, int M0, int D0, int M1, int D1, int Wd)
		{
			const int F = C.Find(Id);
			std::string Bad;
			for (int Y = Y0; Y <= Y1 && F >= 0; Y++)
			{
				for (const FDate& D : StartsIn(C, F, Y))
				{
					if (D < FDate{ Y, M0, D0 } || FDate{ Y, M1, D1 } < D || (Wd >= 0 && Weekday(D) != Wd)) Bad += " " + DateStr(D);
				}
			}
			CHECK(F >= 0 && Bad.empty(), "%s 2026..2060 always between %02d-%02d and %02d-%02d%s%s", Id, M0, D0, M1, D1, Wd == 6 ? " on a Sunday" : (Wd == 5 ? " on a Saturday" : ""), Bad.c_str());
		};
		Between("maslenitsa", 2, 14, 3, 20, 6);         // Orthodox Easter 4 April .. 8 May, less 49 days
		Between("sea_festival", 10, 22, 10, 28, 5);
		Between("lantern_night", 11, 8, 11, 14, 5);
		Between("blossom", 5, 8, 5, 14, 6);
		Between("fisherman_day", 7, 8, 7, 14, 6);
		Between("harvest_fair", 9, 22, 9, 28, 5);
		Between("lunar_new_year", 1, 21, 2, 20, -1);
		Between("holi", 2, 24, 3, 26, -1);
		Between("mid_autumn", 9, 6, 10, 7, -1);
		Between("diwali", 10, 17, 11, 16, -1);
		Between("loy_krathong", 10, 30, 11, 29, -1);
	}

	// ---- the program's hours: inside the decorations' time, inside what At can see, sane for the people's life
	{
		std::string Outside, Reach, Gather, Late, Days;
		for (const FFestival& Fe : C.Festivals)
		{
			const float DecorEnd = (Fe.Days - 1) * 24.f + Fe.DecorTo;
			// At looks back at most Days days (and one ahead): decorations must come down by then
			if (Fe.DecorFrom < -24.f || DecorEnd > (Fe.Days + 1) * 24.f || Fe.DecorFrom >= DecorEnd) Reach += " " + Fe.Id;
			for (int Day = 0; Day < Fe.Days; Day++)
			{
				for (const FPhase& P : Fe.Phases)
				{
					if (P.Day >= 0 && P.Day != Day) continue;
					if (P.Day >= Fe.Days) Days += " " + Fe.Id + "." + P.Id;
					// an every-day phase past midnight would be the next day's by LyudFestival.cpp's floor(Rel / 24) (its shows' end)
					if (P.Day < 0 && P.To > 24.f) Days += " " + Fe.Id + "." + P.Id + "(every day, past midnight)";
					const float From = Day * 24.f + P.From, To = Day * 24.f + P.To;
					if (From < Fe.DecorFrom || To > DecorEnd) Outside += " " + Fe.Id + "." + P.Id;
					// the people's life gets (From mod 24, To mod 24): it must mean the same hours
					const float GF = std::fmod(P.From, 24.f), GT = std::fmod(P.To, 24.f);
					for (float H = 0.025f; H < 48.f; H += 0.05f)
					{
						const bool bPhase = (H >= P.From && H < P.To) || (H + 24.f >= P.From && H + 24.f < P.To) || (H - 24.f >= P.From && H - 24.f < P.To && P.From >= 24.f);
						if (H < 24.f && bPhase != PeopleInHours(GF, GT, H))
						{
							Gather += " " + Fe.Id + "." + P.Id + "@" + std::to_string(H);
							break;
						}
					}
					if (P.To - P.From >= 24.f) Gather += " " + Fe.Id + "." + P.Id + "(a day or longer)";
					// a night on the first day past 22:30: the people stay up for it
					if (Day == 0 && P.Day <= 0 && P.To > 22.5f && Fe.Late + 0.01f < P.To) Late += " " + Fe.Id + "." + P.Id;
				}
			}
			if (Fe.Late > 0.f && (Fe.Late < 12.f || Fe.Late > 36.f || Fe.Late > DecorEnd)) Late += " " + Fe.Id + "(late " + std::to_string(Fe.Late) + ")";
		}
		CHECK(Reach.empty(), "decorations: up at most a day before and down by the day after the last%s", Reach.c_str());
		CHECK(Days.empty(), "every phase's day is one of its festival's, an every-day phase ends by midnight%s", Days.c_str());
		CHECK(Outside.empty(), "every phase runs while its decorations are up (At reports nothing outside them)%s", Outside.c_str());
		CHECK(Gather.empty(), "the gathering hours sent to the people's life (From, To mod 24) cover the phase's hours exactly, over midnight too%s", Gather.c_str());
		CHECK(Late.empty(), "every phase past 22:30 on a festival's first night has people staying up till its end (late), late while the festival is still on%s", Late.c_str());
	}

	// ---- At over every hour: each phase is reported while it runs, on its own day, over midnight and the year's end
	{
		std::string Missing, Wrong;
		for (int F = 0; F < (int)C.Festivals.size(); F++)
		{
			const FFestival& Fe = C.Festivals[F];
			if (Fe.Rule == "weekly" || Fe.Rule == "trigger") continue;
			for (int Y = Y0; Y <= Y1; Y++)
			{
				for (const FDate& S : StartsIn(C, F, Y))
				{
					for (int Day = 0; Day < Fe.Days; Day++)
					{
						for (int Pi = 0; Pi < (int)Fe.Phases.size(); Pi++)
						{
							const FPhase& P = Fe.Phases[Pi];
							if (P.Day >= 0 && P.Day != Day) continue;
							const float Mid = Day * 24.f + (P.From + P.To) * 0.5f;
							const FDate D = AddDays(S, (int)std::floor(Mid / 24.f));
							const float H = Mid - 24.f * std::floor(Mid / 24.f);
							bool bSeen = false;
							for (const FActive& A : C.At(D, H))
							{
								if (A.Festival != F) continue;
								bSeen = std::find(A.Phases.begin(), A.Phases.end(), Pi) != A.Phases.end();
								if (!(A.Start == S) || std::fabs(A.Rel - Mid) > 0.01f) Wrong += " " + Fe.Id + "@" + DateStr(D);
							}
							if (!bSeen) Missing += " " + Fe.Id + "." + P.Id + "@" + DateStr(D);
						}
					}
				}
			}
		}
		CHECK(Missing.empty(), "At reports every phase of every festival 2026..2060 halfway through it%s", Missing.substr(0, 400).c_str());
		CHECK(Wrong.empty(), "At gives the right first day and hours since it (over midnight and New Year)%s", Wrong.substr(0, 400).c_str());

		// the winter fair over the year's end, every year: on 7 January still the one that started 18 December
		std::string Fair;
		const int WF = C.Find("winter_fair"), NY = C.Find("new_year");
		for (int Y = Y0; Y < Y1 && WF >= 0; Y++)
		{
			bool bJan7 = false, bJan8 = false;
			for (const FActive& A : C.At({ Y + 1, 1, 7 }, 15.f)) bJan7 |= A.Festival == WF && A.Start == (FDate{ Y, 12, 18 }) && A.Phase >= 0;
			for (const FActive& A : C.At({ Y + 1, 1, 8 }, 15.f)) bJan8 |= A.Festival == WF;
			if (!bJan7 || bJan8) Fair += " " + std::to_string(Y);
		}
		CHECK(WF >= 0 && Fair.empty(), "the winter fair runs to 7 January and is gone on the 8th, every year%s", Fair.c_str());
		// New Year: fireworks over midnight every year, the dance till 03:00 on the 1st, days off 31 Dec - 2 Jan
		std::string Ny;
		for (int Y = Y0; Y < Y1 && NY >= 0; Y++)
		{
			auto PhaseAt = [&](const FDate& D, float H) -> std::string
			{
				for (const FActive& A : C.At(D, H))
				{
					if (A.Festival == NY) return A.Phase >= 0 ? C.Festivals[NY].Phases[A.Phase].Id : "decor";
				}
				return "";
			};
			if (PhaseAt({ Y, 12, 31 }, 23.9f) != "fireworks" || PhaseAt({ Y + 1, 1, 1 }, 0.3f) != "fireworks" || PhaseAt({ Y + 1, 1, 1 }, 2.5f) != "dance" ||
				PhaseAt({ Y + 1, 1, 1 }, 15.f) != "walk" || !C.IsDayOff({ Y, 12, 31 }) || !C.IsDayOff({ Y + 1, 1, 2 }) || C.IsDayOff({ Y + 1, 1, 3 }))
			{
				Ny += " " + std::to_string(Y);
			}
		}
		CHECK(NY >= 0 && Ny.empty(), "New Year's fireworks cross midnight, the dance on the 1st at 02:30, days off 31 Dec - 2 Jan, every year%s", Ny.c_str());
		// the sea festival's decorations the evening before at 18:00, the dance past midnight into the Sunday
		std::string Sea;
		const int SF = C.Find("sea_festival");
		for (int Y = Y0; Y <= Y1 && SF >= 0; Y++)
		{
			const std::vector<FDate> S = StartsIn(C, SF, Y);
			if (S.size() != 1) continue;
			bool bEve = false, bEarly = false, bNight = false;
			for (const FActive& A : C.At(AddDays(S[0], -1), 18.1f)) bEve |= A.Festival == SF && A.Rel < 0.f;
			for (const FActive& A : C.At(AddDays(S[0], -1), 17.9f)) bEarly |= A.Festival == SF;
			for (const FActive& A : C.At(AddDays(S[0], 1), 0.3f)) bNight |= A.Festival == SF && A.Phase >= 0;
			if (!bEve || bEarly || !bNight) Sea += " " + DateStr(S[0]);
		}
		CHECK(Sea.empty(), "the sea festival: decorations from 18:00 the evening before, dancing at 00:18 the next night, every year%s", Sea.c_str());
	}

	// ---- the game's start: 2026-10-20 16:00, a Tuesday; the first month's festivals
	{
		const FDate Start{ 2026, 10, 20 };
		bool bQuiet = !C.IsDayOff(Start);
		for (const FActive& A : C.At(Start, 16.f)) bQuiet &= A.Phase < 0;
		CHECK(Weekday(Start) == 1 && bQuiet, "the game starts on a Tuesday at 16:00 with no festival program running and no day off");
		std::string Got;
		for (const auto& U : C.Upcoming(Start, 40))
		{
			if (C.Festivals[U.second].Rule != "weekly") Got += " " + C.Festivals[U.second].Id + "@" + DateStr(U.first);
		}
		const std::string Want = " sea_festival@2026-10-24 halloween@2026-10-31 dia_de_muertos@2026-11-01 diwali@2026-11-08 lantern_night@2026-11-14 first_snow@2026-11-20 loy_krathong@2026-11-24";
		CHECK(Got == Want, "the first 40 days of the game:%s", Got.c_str());
	}

	// ---- festivals meeting at a venue (the higher priority has it): nothing but the market vanishes, no show is lost
	{
		std::vector<std::vector<FDate>> S(C.Festivals.size());
		std::vector<int> Busy;          // days with two festivals (decorations or program) at once
		for (int F = 0; F < (int)C.Festivals.size(); F++)
		{
			for (int Y = Y0 - 1; Y <= Y1; Y++)
			{
				for (const FDate& D : StartsIn(C, F, Y)) S[F].push_back(D);
			}
		}
		std::map<int, int> PerDay;      // day -> how many festivals touch it
		for (int F = 0; F < (int)C.Festivals.size(); F++)
		{
			const FFestival& Fe = C.Festivals[F];
			for (const FDate& D : S[F])
			{
				const int D0 = DaysFromCivil(D) + (int)std::floor(Fe.DecorFrom / 24.f);
				const int D1 = DaysFromCivil(D) + (int)std::floor(((Fe.Days - 1) * 24.f + Fe.DecorTo - 0.01f) / 24.f);
				for (int X = D0; X <= D1; X++) PerDay[X]++;
			}
		}
		std::map<std::pair<int, int>, bool> On;                 // (festival, start day) seen on
		struct FSteps { int Cal = 0, Won = 0, Tie = 0; };
		std::map<std::tuple<int, int, int>, FSteps> Steps;     // (festival, start day, phase) -> steps: by the calendar, won, lost to an equal
		for (const auto& PD : PerDay)
		{
			if (PD.second < 2) continue;
			const FDate D = CivilFromDays(PD.first);
			if (D.Y < Y0 || D.Y > Y1) continue;
			for (float H = 0.05f; H < 24.f; H += 0.1f)
			{
				const std::vector<FActive> Act = C.At(D, H);
				const FClaimed Cl = Claims(C, D, H);
				for (const FActive& A : Act)
				{
					const int SD = DaysFromCivil(A.Start);
					On[{ A.Festival, SD }] |= std::find(Cl.On.begin(), Cl.On.end(), A.Festival) != Cl.On.end();
					for (const int Pi : A.Phases)
					{
						auto& St = Steps[{ A.Festival, SD, Pi }];
						St.Cal++;
						St.Won += std::find(Cl.Ran.begin(), Cl.Ran.end(), std::make_pair(A.Festival, Pi)) != Cl.Ran.end();
						for (const auto& L : Cl.Lost)
						{
							if (std::get<0>(L) == A.Festival && std::get<1>(L) == Pi && C.Festivals[std::get<2>(L)].Priority <= C.Festivals[A.Festival].Priority) St.Tie++;
						}
					}
				}
			}
		}
		std::string Vanish, Short, Tie;
		int Market = 0;
		for (const auto& O : On)
		{
			if (O.second) continue;
			if (C.Festivals[O.first.first].Rule == "weekly") { Market++; continue; }
			Vanish += " " + C.Festivals[O.first.first].Id + "@" + DateStr(CivilFromDays(O.first.second));
		}
		for (const auto& St : Steps)
		{
			const FFestival& Fe = C.Festivals[std::get<0>(St.first)];
			const FPhase& P = Fe.Phases[std::get<2>(St.first)];
			const std::string What = " " + Fe.Id + "." + P.Id + "@" + DateStr(CivilFromDays(std::get<1>(St.first)));
			if (St.second.Tie > 0) Tie += What;
			if (Fe.Rule != "weekly" && St.second.Won * 2 < St.second.Cal) Short += What;
		}
		// the higher priority wins a venue by design (docs/festivals.md: the sea festival takes the fair from the market)
		std::printf("     (%d market Saturdays give the fair to a bigger festival; more than half of a phase lost to a higher priority:%s)\n", Market,
			Short.empty() ? " none" : Short.substr(0, 700).c_str());
		CHECK(Vanish.empty(), "no festival 2026..2060 vanishes (every venue taken by another) but the weekly market%s", Vanish.c_str());
		CHECK(Tie.empty(), "no phase 2026..2060 loses its venue to a festival of the same or a lower priority (who wins must not hang on the file's order)%s", Tie.substr(0, 600).c_str());
	}
}

static int Test(FCalendar& C)
{
	// the date rules
	CHECK(Weekday({ 2026, 10, 20 }) == 1, "2026-10-20 is a Tuesday");
	CHECK(Weekday({ 2026, 11, 6 }) == 4, "2026-11-06 is a Friday");
	CHECK(CivilFromDays(DaysFromCivil({ 2028, 2, 29 })) == (FDate{ 2028, 2, 29 }), "round trip");
	CHECK(OrthodoxEaster(2026) == (FDate{ 2026, 4, 12 }), "Orthodox Easter 2026");
	CHECK(OrthodoxEaster(2027) == (FDate{ 2027, 5, 2 }), "Orthodox Easter 2027");
	CHECK(OrthodoxEaster(2025) == (FDate{ 2025, 4, 20 }), "Orthodox Easter 2025");
	CHECK(WesternEaster(2026) == (FDate{ 2026, 4, 5 }), "Western Easter 2026");
	CHECK(WesternEaster(2027) == (FDate{ 2027, 3, 28 }), "Western Easter 2027");
	FDate N;
	CHECK(NthWeekday(2026, 10, 4, 5, N) && N == (FDate{ 2026, 10, 24 }), "4th Saturday of October 2026");
	CHECK(NthWeekday(2026, 10, -1, 5, N) && N == (FDate{ 2026, 10, 31 }), "last Saturday of October 2026");
	CHECK(!NthWeekday(2026, 2, 5, 0, N), "no 5th Monday in February 2026");

	// the moon (Meeus): new moon 2026-02-17 12:01 UTC, full moon 2026-03-03 11:38 UTC (an eclipse), 2026-09-26 16:49 UTC
	CHECK(MoonOnOrAfter({ 2026, 2, 1 }, 0.0, 0.f) == (FDate{ 2026, 2, 17 }), "new moon 2026-02-17");
	CHECK(MoonOnOrAfter({ 2026, 2, 20 }, 0.5, 0.f) == (FDate{ 2026, 3, 3 }), "full moon 2026-03-03");
	CHECK(MoonOnOrAfter({ 2026, 9, 8 }, 0.5, 2.f) == (FDate{ 2026, 9, 26 }), "full moon 2026-09-26");
	{
		const double Jd = MoonPhaseJDE(323, 0.0);      // 2026-02-17 12:01 UTC = JD 2461089.0
		CHECK(std::fabs(Jd - 2461089.0) < 0.05, "new moon 2026-02-17 to the hour (JDE %.3f)", Jd);
	}

	// the data
	CHECK(C.Festivals.size() >= 10, "festivals.json has %d festivals", (int)C.Festivals.size());
	const int City = C.Find("sea_festival"), Market = C.Find("weekend_market"), NewYear = C.Find("new_year"), Mas = C.Find("maslenitsa");
	const int Mid = C.Find("midsummer"), Snow = C.Find("first_snow");
	CHECK(City >= 0 && Market >= 0 && NewYear >= 0 && Mas >= 0 && Mid >= 0 && Snow >= 0, "the main festivals are there");
	CHECK(C.StartsOn(City, { 2026, 10, 24 }), "the sea festival is 2026-10-24");
	CHECK(C.StartsOn(Mas, { 2026, 2, 22 }), "Maslenitsa's Sunday 2026-02-22 (Easter - 49)");
	CHECK(C.StartsOn(Market, { 2026, 10, 24 }) && C.StartsOn(Market, { 2026, 10, 31 }) && !C.StartsOn(Market, { 2026, 10, 25 }), "the market every Saturday");
	// the first week of the game: the sea festival comes on its fifth day
	bool bCityFirst = false;
	for (const auto& U : C.Upcoming({ 2026, 10, 20 }, 7))
	{
		bCityFirst |= U.second == City;
	}
	CHECK(bCityFirst, "the sea festival is in the game's first week");
	// the sea festival program
	{
		auto A = C.At({ 2026, 10, 24 }, 11.f);
		CHECK(!A.empty() && A[0].Festival == City, "the sea festival is first (priority) at 11:00");
		CHECK(!A.empty() && C.Festivals[City].Phases[A[0].Phase].Id == "fair", "11:00 the fair");
		A = C.At({ 2026, 10, 24 }, 21.7f);
		CHECK(!A.empty() && A[0].Festival == City && C.Festivals[City].Phases[A[0].Phase].Id == "fireworks", "21:42 the fireworks");
		A = C.At({ 2026, 10, 25 }, 0.25f);
		CHECK(!A.empty() && A[0].Festival == City && C.Festivals[City].Phases[A[0].Phase].Id == "dance", "00:15 the next night: still dancing");
		A = C.At({ 2026, 10, 23 }, 20.f);
		CHECK(!A.empty() && A[0].Festival == City && A[0].Phase < 0, "the evening before: decorations up, nothing on yet");
		A = C.At({ 2026, 10, 25 }, 3.f);
		bool bCity = false;
		for (const FActive& X : A) bCity |= X.Festival == City;
		CHECK(!bCity, "03:00 the next night: it is over");
		CHECK(C.IsDayOff({ 2026, 10, 24 }), "the sea festival is a day off");
	}
	// New Year over midnight, days off on the 1st and 2nd
	{
		auto A = C.At({ 2027, 1, 1 }, 0.1f);
		CHECK(!A.empty() && A[0].Festival == NewYear && C.Festivals[NewYear].Phases[A[0].Phase].Id == "fireworks", "00:06 on the 1st: fireworks");
		CHECK(C.IsDayOff({ 2027, 1, 1 }) && C.IsDayOff({ 2027, 1, 2 }) && !C.IsDayOff({ 2027, 1, 4 }), "1-2 January days off");
		A = C.At({ 2026, 12, 25 }, 15.f);
		bool bFair = false;
		for (const FActive& X : A) bFair |= C.Festivals[X.Festival].Id == "winter_fair" && X.Phase >= 0;
		CHECK(bFair, "the winter fair on 25 December");
	}
	// midsummer: people go to see the sunrise
	{
		auto A = C.At({ 2026, 6, 24 }, 4.f);
		CHECK(!A.empty() && A[0].Festival == Mid && C.Festivals[Mid].Phases[A[0].Phase].Id == "sunrise", "04:00 on 24 June: the sunrise");
	}
	// the world's festivals by the moon
	{
		const int Lny = C.Find("lunar_new_year"), Holi = C.Find("holi"), Diwali = C.Find("diwali"), Loy = C.Find("loy_krathong");
		CHECK(Lny >= 0 && Holi >= 0 && Diwali >= 0 && Loy >= 0, "the moon festivals are there");
		CHECK(C.StartsOn(Lny, { 2026, 2, 17 }) && C.StartsOn(Lny, { 2027, 2, 6 }) && C.StartsOn(Lny, { 2028, 1, 26 }), "Lunar New Year 2026-02-17, 2027-02-06, 2028-01-26");
		CHECK(C.StartsOn(Holi, { 2027, 3, 22 }) && C.StartsOn(Holi, { 2028, 3, 11 }) && C.StartsOn(Holi, { 2030, 3, 20 }), "Holi 2027-03-22, 2028-03-11, 2030-03-20");
		CHECK(C.StartsOn(Diwali, { 2026, 11, 8 }) && C.StartsOn(Diwali, { 2027, 10, 29 }) && C.StartsOn(Diwali, { 2028, 10, 17 }), "Diwali 2026-11-08, 2027-10-29, 2028-10-17");
		const int Moon = C.Find("mid_autumn");
		CHECK(Moon >= 0 && C.StartsOn(Moon, { 2026, 9, 25 }) && C.StartsOn(Moon, { 2027, 9, 15 }) && C.StartsOn(Moon, { 2030, 9, 12 }), "Mid-Autumn 2026-09-25, 2027-09-15, 2030-09-12");
		CHECK(C.StartsOn(Loy, { 2026, 11, 24 }), "Loy Krathong 2026-11-24");
		int Count = 0;
		for (int Y = 2026; Y < 2036; Y++)
		{
			for (int D = 0; D < 366; D++) Count += C.StartsOn(Lny, AddDays({ Y, 1, 1 }, D)) && AddDays({ Y, 1, 1 }, D).Y == Y;
		}
		CHECK(Count == 10, "Lunar New Year once a year (%d in 10 years)", Count);
	}
	// a trigger
	CHECK(!C.StartsOn(Snow, { 2026, 11, 20 }), "no snow yet");
	C.Fire("first_snow", { 2026, 11, 19 });
	CHECK(C.StartsOn(Snow, { 2026, 11, 20 }), "the day after the first snow");
	// every festival happens within a year, every phase is sane
	{
		std::string Never, NoProgram, BadHours, BadMode;
		for (int F = 0; F < (int)C.Festivals.size(); F++)
		{
			const FFestival& Fe = C.Festivals[F];
			FDate D;
			if (Fe.Rule != "trigger" && !C.Next(F, { 2026, 10, 20 }, D)) Never += " " + Fe.Id;
			if (Fe.Phases.empty()) NoProgram += " " + Fe.Id;
			for (const FPhase& P : Fe.Phases)
			{
				if (!(P.To > P.From && P.Pull > 0.f && P.Pull <= 2.f)) BadHours += " " + Fe.Id + "." + P.Id;
				if (!(P.Mode == "mingle" || P.Mode == "watch" || P.Mode == "sit" || P.Mode == "fair")) BadMode += " " + Fe.Id + "." + P.Id + "=" + P.Mode;
			}
		}
		CHECK(Never.empty(), "every festival but a trigger happens within 3 years of the game's start%s", Never.c_str());
		CHECK(NoProgram.empty(), "every festival has a program%s", NoProgram.c_str());
		CHECK(BadHours.empty(), "every phase has From < To and a pull in (0, 2]%s", BadHours.c_str());
		CHECK(BadMode.empty(), "every phase's mode is mingle/watch/sit/fair%s", BadMode.c_str());
	}
	Deep(C);
	if (Fails) std::printf("%d of %d checks FAILED\n", Fails, Fails + Passes); else std::printf("all ok (%d checks)\n", Passes);
	return Fails ? 1 : 0;
}

int main(int Argc, char** Argv)
{
	const std::string Text = ReadFile("unreal/Lyudmilovsk/Content/PeopleData/festivals.json");
	FCalendar C;
	if (!C.Load(Text))
	{
		std::printf("%s\n", C.LastError.c_str());
		return 2;
	}
	const std::string Cmd = Argc > 1 ? Argv[1] : "test";
	if (Cmd == "test")
	{
		return Test(C);
	}
	if (Cmd == "year")
	{
		const int Y = Argc > 2 ? std::atoi(Argv[2]) : 2026;
		for (const auto& U : C.Upcoming({ Y, 1, 1 }, 365))
		{
			const FFestival& F = C.Festivals[U.second];
			if (F.Rule == "weekly")
			{
				continue;
			}
			std::printf("%04d-%02d-%02d %s %-15s %d day(s)%s:", U.first.Y, U.first.M, U.first.D, DayName(Weekday(U.first)), F.Id.c_str(), F.Days, F.bHoliday ? " holiday" : "");
			for (const FPhase& P : F.Phases)
			{
				std::printf(" %s %s-%s@%s", P.Id.c_str(), Hours(P.From).c_str(), Hours(P.To).c_str(), P.Venue.c_str());
			}
			std::printf("\n");
		}
		return 0;
	}
	if (Cmd == "at" && Argc > 3)
	{
		FDate D;
		std::sscanf(Argv[2], "%d-%d-%d", &D.Y, &D.M, &D.D);
		PrintAt(C, D, (float)std::atof(Argv[3]));
		return 0;
	}
	std::printf("festivalsim test | year [YYYY] | at YYYY-MM-DD HH.H\n");
	return 1;
}
