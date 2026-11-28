// The street's day by the weather and the season (docs/street_life.md): plain C++, no engine (tools/streetsim tests it).
//
// The town's everyday street life is the people's life (activities and duties in Content/PeopleData) switched by a few
// conditions the game sets (UPeopleLifeSubsystem::SetCondition, ULyudStreetLife): an activity with "when" is only done
// while its condition is on. This works the conditions out from the date, the hour and the weather, and a plan of the
// day made once a morning from the weather's forecast (the barrel comes out when the afternoon will be hot; the washing
// goes up on a fine wash-day morning and is taken in before the rain the forecast brings, or in the evening).
#pragma once

#include <cmath>
#include <cstdint>
#include <functional>

namespace LyudStreet
{
	enum ECond : uint32_t
	{
		Hot = 1u << 0,          // street_hot: the kvass barrel and the ice-cream cart are out, the vendors there
		Sweep = 1u << 1,        // street_sweep: the yard keeper sweeps the promenade at dawn (not in the snow months)
		LaundryHang = 1u << 2,  // laundry_hang: the laundresses hang the washing out
		LaundryOut = 1u << 3,   // laundry_out: the washing is on the lines (the things only, no activity)
		LaundryTake = 1u << 4,  // laundry_take: they take it in (rain coming, or the evening)
		Grannies = 1u << 5,     // yard_grannies: the grandmothers' benches by the entrances
		Dominoes = 1u << 6,     // yard_dominoes: the domino tables in the yards' evening
		Raking = 1u << 7,       // leaf_raking: the yard keeper rakes the autumn's leaves
		LeafPiles = 1u << 8,    // leaf_piles: the raked piles lie in the yards (the things only)
		Subbotnik = 1u << 9,    // subbotnik: the spring cleaning morning, everybody out painting and whitewashing
		TrunksWhite = 1u << 10, // trunks_white: the yards' trunks whitewashed (from the subbotnik to the end of October)
		// the warm season's evenings on the promenade and Sun Square (docs/promenade.md)
		PromEvening = 1u << 11, // prom_evening: the evening promenade (15 May to 15 September, 18:30-22:30, dry, 14 C and over)
		PromDances = 1u << 12,  // prom_dances: the summer dances on the square (25 May to 5 September, Fri and Sat 19-23, a dry evening)
		PromBand = 1u << 13,    // prom_band: the brass band at the stage (June to August, Sunday 15-17, a dry afternoon)
		HallDances = 1u << 14,  // hall_dances: the hall «Своды»'s dances are on (all the year but June to August: then they are outdoors)
	};
	constexpr int NumConds = 15;

	inline const char* CondName(int I)
	{
		static const char* Names[NumConds] = { "street_hot", "street_sweep", "laundry_hang", "laundry_out", "laundry_take", "yard_grannies",
											   "yard_dominoes", "leaf_raking", "leaf_piles", "subbotnik", "trunks_white",
											   "prom_evening", "prom_dances", "prom_band", "hall_dances" };
		return I >= 0 && I < NumConds ? Names[I] : "";
	}

	inline int CondIndex(const char* Name)
	{
		for (int I = 0; I < NumConds; I++)
		{
			const char* A = CondName(I);
			const char* B = Name;
			while (*A && *A == *B) { A++; B++; }
			if (*A == 0 && *B == 0) return I;
		}
		return -1;
	}

	/** The weather at a moment (the game's FLyudWeatherState / FLyudSkyWeather): air C, rain mm/h, cloud 0..1, snow share 0..1. */
	struct FSky
	{
		float TempC = 12.f;
		float Rain = 0.f;
		float Cloud = 0.5f;
		float Snow = 0.f;
	};

	/** Monday = 0 for a Gregorian date (Sakamoto's rule). */
	inline int WeekdayOf(int Y, int M, int D)
	{
		static const int T[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
		if (M < 3) Y -= 1;
		const int Sun0 = (Y + Y / 4 - Y / 100 + Y / 400 + T[M - 1] + D) % 7;     // 0 = Sunday
		return (Sun0 + 6) % 7;
	}

	/** The spring subbotnik: the third Saturday of April. */
	inline int SubbotnikDay(int Y)
	{
		const int First = WeekdayOf(Y, 4, 1);
		const int FirstSat = 1 + (5 - First + 7) % 7;
		return FirstSat + 14;
	}

	inline bool InHours(float H, float From, float To) { return From <= To ? (H >= From && H < To) : (H >= From || H < To); }

	/** The autumn's leaves: from 20 September to 15 November. */
	inline bool LeafSeason(int M, int D) { return (M == 9 && D >= 20) || M == 10 || (M == 11 && D <= 15); }

	/** The evening promenade's season: 15 May to 15 September. */
	inline bool PromSeason(int M, int D) { const int K = M * 100 + D; return K >= 515 && K <= 915; }

	/** The summer dances on the square: 25 May to 5 September (the hall «Своды» keeps its own till the end of May and from September). */
	inline bool DanceSeason(int M, int D) { const int K = M * 100 + D; return K >= 525 && K <= 905; }

	/** The brass band's Sundays: June, July and August. */
	inline bool BandSeason(int M) { return M >= 6 && M <= 8; }

	/** The hall's dance room is used for the dances: not in June, July and August. */
	inline bool HallSeason(int M) { return M < 6 || M > 8; }

	/** A wash day: Monday, Thursday and Saturday from April to October. */
	inline bool WashDay(int M, int Weekday) { return M >= 4 && M <= 10 && (Weekday == 0 || Weekday == 3 || Weekday == 5); }

	/** The day's plan, made once a morning from the forecast. */
	struct FDayPlan
	{
		int Key = -1;               // year * 10000 + month * 100 + day
		bool bHotDay = false;       // the afternoon will be over 22 C and dry: the barrel and the cart come out
		bool bHang = false;         // a wash day with a fine morning: the washing goes up at 9
		float TakeAt = 18.f;        // when it is taken in: the first quarter hour with rain within 45 minutes, else 18 (17 on a hot day)
		float HotMax = 0.f;         // the afternoon's warmest of the samples (C), for the report
		bool bDanceEve = false;     // a dance evening on the square (its season, Fri or Sat) the forecast keeps dry: else it is called off
		bool bBandDay = false;      // a band Sunday whose afternoon the forecast keeps dry: else the concert is called off
	};

	/** The plan of a day. At(hour) is the forecast for that hour of the day (the game: ULyudWeatherSubsystem::GetStateAt). */
	inline FDayPlan PlanDay(int Y, int M, int D, int Weekday, const std::function<FSky(float)>& At)
	{
		FDayPlan P;
		P.Key = Y * 10000 + M * 100 + D;
		if (M >= 5 && M <= 9)
		{
			const FSky A = At(12.f), B = At(14.f), C = At(16.f);
			P.HotMax = std::fmax(A.TempC, std::fmax(B.TempC, C.TempC));
			P.bHotDay = P.HotMax > 22.f && B.Rain < 0.2f && B.Snow < 0.5f;
		}
		if (WashDay(M, Weekday))
		{
			const FSky Nine = At(9.f);
			bool bDry = Nine.Rain < 0.1f && Nine.Cloud < 0.75f && Nine.TempC >= 8.f;
			for (float H = 9.5f; bDry && H <= 11.f; H += 0.5f)
			{
				bDry = At(H).Rain < 0.2f;
			}
			P.bHang = bDry;
			if (bDry)
			{
				P.TakeAt = P.HotMax > 22.f ? 17.f : 18.f;
				for (float H = 10.f; H < P.TakeAt; H += 0.25f)
				{
					if (At(H + 0.75f).Rain >= 0.2f)
					{
						P.TakeAt = H;
						break;
					}
				}
			}
		}
		// the dances on the square: the evening's forecast at 19, 20:30 and 22 dry and not cold, or the poster says "called off"
		if (DanceSeason(M, D) && (Weekday == 4 || Weekday == 5))
		{
			bool bDry = true;
			for (float H = 19.f; bDry && H <= 22.f; H += 1.5f)
			{
				const FSky S = At(H);
				bDry = S.Rain < 0.2f && S.Snow < 0.5f && S.TempC >= 12.f;
			}
			P.bDanceEve = bDry;
		}
		// the brass band: a Sunday of its season, dry at 15 and 16
		if (BandSeason(M) && Weekday == 6)
		{
			P.bBandDay = At(15.f).Rain < 0.2f && At(16.f).Rain < 0.2f;
		}
		return P;
	}

	/** The moment. */
	struct FNow
	{
		int Year = 2026, Month = 6, Day = 1, Weekday = 0;
		float Hour = 12.f;
		FSky Sky;
	};

	/** The conditions now (ECond bits). */
	inline uint32_t Conditions(const FNow& N, const FDayPlan& P)
	{
		uint32_t B = 0;
		const float H = N.Hour;
		const bool bRain = N.Sky.Rain >= 0.2f, bShower = N.Sky.Rain >= 0.5f, bSnow = N.Sky.Snow >= 0.5f && N.Sky.Rain > 0.05f;
		const int M = N.Month;
		// the barrel and the cart: a hot day from ten to seven, packed up while a shower lasts
		if (P.bHotDay && InHours(H, 10.f, 19.f) && !bShower)
		{
			B |= Hot;
		}
		// the dawn sweep: March to November (the winter's snow is shovelled, docs/people.md "Winter play")
		if (M >= 3 && M <= 11 && !bSnow)
		{
			B |= Sweep;
		}
		if (P.bHang)
		{
			if (InHours(H, 9.f, 9.5f)) B |= LaundryHang;
			if (InHours(H, 9.3f, P.TakeAt + 0.4f)) B |= LaundryOut;
			if (InHours(H, P.TakeAt, P.TakeAt + 0.4f)) B |= LaundryTake;
		}
		// the grandmothers: April to October, the late morning and the evening, dry and not cold
		if (M >= 4 && M <= 10 && (InHours(H, 10.f, 12.5f) || InHours(H, 16.f, 20.f)) && !bRain && N.Sky.TempC >= 10.f)
		{
			B |= Grannies;
		}
		// dominoes: May to September evenings, dry and mild
		if (M >= 5 && M <= 9 && InHours(H, 17.5f, 21.5f) && !bRain && N.Sky.TempC >= 14.f)
		{
			B |= Dominoes;
		}
		if (LeafSeason(M, N.Day))
		{
			if (InHours(H, 8.f, 11.5f) && !bShower && !bSnow) B |= Raking;
			if (InHours(H, 8.5f, 18.f)) B |= LeafPiles;
		}
		const int Sub = SubbotnikDay(N.Year);
		if (M == 4 && N.Day == Sub && InHours(H, 9.f, 13.f))
		{
			B |= Subbotnik;
		}
		// whitewashed from the subbotnik (its first hour on) to the end of October
		if ((M == 4 && (N.Day > Sub || (N.Day == Sub && H >= 10.f))) || (M >= 5 && M <= 10))
		{
			B |= TrunksWhite;
		}
		// the evening promenade: a warm dry evening of its season
		if (PromSeason(M, N.Day) && InHours(H, 18.5f, 22.5f) && !bRain && N.Sky.TempC >= 14.f)
		{
			B |= PromEvening;
		}
		// the dances on the square: a dry evening by the forecast, and off while a shower lasts (the band covers the amplifier)
		if (P.bDanceEve && (N.Weekday == 4 || N.Weekday == 5) && InHours(H, 19.f, 23.f) && !bShower)
		{
			B |= PromDances;
		}
		// the brass band's Sunday afternoon at the stage
		if (P.bBandDay && N.Weekday == 6 && InHours(H, 15.f, 17.f) && !bShower)
		{
			B |= PromBand;
		}
		if (HallSeason(M))
		{
			B |= HallDances;
		}
		return B;
	}
}
