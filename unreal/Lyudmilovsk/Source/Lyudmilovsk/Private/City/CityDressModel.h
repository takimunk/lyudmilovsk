#pragma once

// The town's swappable room dressing (docs/city_dress.md): when is a set of small props shown? Plain C++ on top of the city
// things' moment (LyudCity::FWhen), no engine types, so tools/citydress checks it against the Python of the Blender side
// (blender/city/citykit.py dress_visible) outside the engine. ULyudCityDress (Private/World/LyudCityDress.cpp) asks it.

#include "CityThingsModel.h"

#include <cstdint>
#include <string>

namespace LyudCity
{
	/** A set's condition (the export json's "when", citykit.dress_when). Every field left at its default holds always. */
	struct FDressWhen
	{
		/** Any of these EWeatherBit words now (0: no condition) / none of them (0: no condition). */
		uint8_t WeatherAny = 0;
		uint8_t WeatherNone = 0;
		/** Bit m-1 for month m (0: any month). */
		uint16_t Months = 0;
		/** Bit d for weekday d, Monday = 0 (0: any day). */
		uint8_t Weekdays = 0;
		/** [From, To) on a 24 h clock (To below From runs past midnight; 0..24 is always). */
		float HourFrom = 0.f;
		float HourTo = 24.f;
		/** "" no condition, "any" a holiday today, "none" not a holiday, else a festival id on now. */
		std::string Festival;
		/** Shown on this share of days (a stable die of plot, set and day). */
		float Chance = 1.f;
		/** Days since the game began modulo PhasePeriod in [PhaseFrom, PhaseTo) (PhasePeriod 0: no condition). */
		int PhasePeriod = 0;
		int PhaseFrom = 0;
		int PhaseTo = 0;
		/** -1 no condition, 1 in the building's open hours, 0 out of them. */
		int Open = -1;
		/** A trace of a visit (citykit visit=): shown for VisitKeep hours after the last matching visit ended (0: no condition).
		 *  Which visits match (the activities, the place, who) is the caller's: ULyudCityDress keeps the time of the last one. */
		float VisitKeep = 0.f;
	};

	/** The stable die of a set on a day: FNV-1a over "<plot>/<id>@<day>" and a mix; mirrors citykit.dress_hash01. */
	inline float DressHash01(const std::string& Plot, const std::string& Id, long long Day)
	{
		const std::string Key = Plot + "/" + Id + "@" + std::to_string(Day);
		uint32_t H = 2166136261u;
		for (const char C : Key)
		{
			H ^= (uint32_t)(unsigned char)C;
			H *= 16777619u;
		}
		H ^= H >> 15;
		H *= 2246822519u;
		H ^= H >> 13;
		return (float)(H & 0xFFFFFFu) / (float)(1u << 24);
	}

	/** Is a set with condition C shown at the moment W? bOpen: the building is in its open hours now. VisitAgo: hours since
	 *  the last visit that matches the set's visit= ended (< 0: none known since the game was loaded). */
	inline bool DressVisible(const FDressWhen& C, const FWhen& W, const std::string& Plot, const std::string& Id, bool bOpen,
		float VisitAgo = -1.f)
	{
		if (C.WeatherAny && !(W.Weather & C.WeatherAny))
		{
			return false;
		}
		if (C.WeatherNone && (W.Weather & C.WeatherNone))
		{
			return false;
		}
		if (C.Months && W.MonthDay > 0)
		{
			const int Month = W.MonthDay / 100;
			if (Month < 1 || Month > 12 || !(C.Months & (1u << (Month - 1))))
			{
				return false;
			}
		}
		if (C.Weekdays && !(C.Weekdays & (1u << (W.Weekday & 7))))
		{
			return false;
		}
		if (!InHours(W.Hour, C.HourFrom, C.HourTo))
		{
			return false;
		}
		if (!C.Festival.empty())
		{
			if (C.Festival == "any")
			{
				if (!W.bFestival)
				{
					return false;
				}
			}
			else if (C.Festival == "none")
			{
				if (W.bFestival)
				{
					return false;
				}
			}
			else if (std::find(W.Festivals.begin(), W.Festivals.end(), C.Festival) == W.Festivals.end())
			{
				return false;
			}
		}
		if (C.PhasePeriod > 0)
		{
			const long long K = ((W.Since % C.PhasePeriod) + C.PhasePeriod) % C.PhasePeriod;
			if (K < C.PhaseFrom || K >= C.PhaseTo)
			{
				return false;
			}
		}
		if (C.Open >= 0 && (C.Open == 1) != bOpen)
		{
			return false;
		}
		if (C.Chance < 1.f && DressHash01(Plot, Id, W.Day) >= C.Chance)
		{
			return false;
		}
		if (C.VisitKeep > 0.f && !(VisitAgo >= 0.f && VisitAgo < C.VisitKeep))
		{
			return false;
		}
		return true;
	}
}
