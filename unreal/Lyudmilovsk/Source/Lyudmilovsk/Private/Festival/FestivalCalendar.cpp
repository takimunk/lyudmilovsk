#include "FestivalCalendar.h"

#include "../People/MiniJson.h"

#include <algorithm>

namespace LyudFest
{
	// Howard Hinnant's days_from_civil / civil_from_days
	int DaysFromCivil(const FDate& Date)
	{
		const int Y = Date.Y - (Date.M <= 2 ? 1 : 0);
		const int Era = (Y >= 0 ? Y : Y - 399) / 400;
		const int Yoe = Y - Era * 400;
		const int Doy = (153 * (Date.M + (Date.M > 2 ? -3 : 9)) + 2) / 5 + Date.D - 1;
		const int Doe = Yoe * 365 + Yoe / 4 - Yoe / 100 + Doy;
		return Era * 146097 + Doe - 719468;
	}

	FDate CivilFromDays(int Z)
	{
		Z += 719468;
		const int Era = (Z >= 0 ? Z : Z - 146096) / 146097;
		const int Doe = Z - Era * 146097;
		const int Yoe = (Doe - Doe / 1460 + Doe / 36524 - Doe / 146096) / 365;
		const int Doy = Doe - (365 * Yoe + Yoe / 4 - Yoe / 100);
		const int Mp = (5 * Doy + 2) / 153;
		FDate Out;
		Out.D = Doy - (153 * Mp + 2) / 5 + 1;
		Out.M = Mp + (Mp < 10 ? 3 : -9);
		Out.Y = Yoe + Era * 400 + (Out.M <= 2 ? 1 : 0);
		return Out;
	}

	int Weekday(const FDate& Date)
	{
		// 1970-01-01 was a Thursday (3)
		const int W = (DaysFromCivil(Date) + 3) % 7;
		return W < 0 ? W + 7 : W;
	}

	int DaysInMonth(int Y, int M)
	{
		static const int N[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
		if (M == 2)
		{
			const bool bLeap = (Y % 4 == 0 && Y % 100 != 0) || Y % 400 == 0;
			return bLeap ? 29 : 28;
		}
		return N[(M - 1 + 12) % 12];
	}

	FDate OrthodoxEaster(int Y)
	{
		// Meeus's Julian computus, then the Julian date to the Gregorian one
		const int A = Y % 4, B = Y % 7, C = Y % 19;
		const int D = (19 * C + 15) % 30;
		const int E = (2 * A + 4 * B - D + 34) % 7;
		const int Month = (D + E + 114) / 31;
		const int Day = (D + E + 114) % 31 + 1;
		const int Gap = Y / 100 - Y / 400 - 2;      // 13 days in 1900-2099
		return AddDays(FDate{ Y, Month, Day }, Gap);
	}

	FDate WesternEaster(int Y)
	{
		const int A = Y % 19, B = Y / 100, C = Y % 100, D = B / 4, E = B % 4;
		const int F = (B + 8) / 25, G = (B - F + 1) / 3;
		const int H = (19 * A + B - D - G + 15) % 30;
		const int I = C / 4, K = C % 4;
		const int L = (32 + 2 * E + 2 * I - H - K) % 7;
		const int M = (A + 11 * H + 22 * L) / 451;
		const int Month = (H + L - 7 * M + 114) / 31;
		const int Day = (H + L - 7 * M + 114) % 31 + 1;
		return FDate{ Y, Month, Day };
	}

	bool NthWeekday(int Y, int M, int N, int Wd, FDate& Out)
	{
		if (N > 0)
		{
			const FDate First{ Y, M, 1 };
			const int Shift = (Wd - Weekday(First) + 7) % 7;
			const int Day = 1 + Shift + (N - 1) * 7;
			if (Day > DaysInMonth(Y, M))
			{
				return false;
			}
			Out = FDate{ Y, M, Day };
			return true;
		}
		const FDate Last{ Y, M, DaysInMonth(Y, M) };
		const int Back = (Weekday(Last) - Wd + 7) % 7;
		Out = FDate{ Y, M, Last.D - Back - (-N - 1) * 7 };
		return Out.D >= 1;
	}

	namespace
	{
		std::vector<std::string> Strings(const LyudJson::FValue& V)
		{
			std::vector<std::string> Out;
			if (V.Type == LyudJson::FValue::String)
			{
				Out.push_back(V.S());
			}
			for (const LyudJson::FValue& I : V.Items)
			{
				Out.push_back(I.S());
			}
			return Out;
		}

		std::vector<int> Ints(const LyudJson::FValue& V)
		{
			std::vector<int> Out;
			for (const LyudJson::FValue& I : V.Items)
			{
				Out.push_back(I.I());
			}
			return Out;
		}

		bool Has(const std::vector<int>& A, int X) { return std::find(A.begin(), A.end(), X) != A.end(); }
	}

	bool FCalendar::Load(const std::string& Text)
	{
		LyudJson::FValue J;
		if (!LyudJson::Parse(Text, J, LastError))
		{
			LastError = "festivals.json: " + LastError;
			return false;
		}
		Festivals.clear();
		for (const LyudJson::FValue& V : J["festivals"].Items)
		{
			FFestival F;
			F.Id = V["id"].S();
			const LyudJson::FValue& W = V["when"];
			F.Rule = W["rule"].S("date");
			F.Month = W["month"].I(1);
			F.DayOfMonth = W["day"].I(1);
			F.Nth = W["nth"].I(1);
			F.Wd = W["weekday"].I(5);
			F.Offset = W["offset"].I(0);
			F.Weekdays = Ints(W["weekdays"]);
			F.Months = Ints(W["months"]);
			F.Trigger = W["trigger"].S();
			F.Days = std::max(1, V["days"].I(1));
			F.bHoliday = V["holiday"].F() > 0.f;
			F.HolidayDays = Ints(V["holiday_days"]);
			F.DecorFrom = V["decor"]["from"].F(8.f);
			F.DecorTo = V["decor"]["to"].F(24.f);
			F.Decor = Strings(V["decor"]["kinds"]);
			F.DecorVenues = Strings(V["decor"]["venues"]);
			F.Priority = V["priority"].I(0);
			for (const LyudJson::FValue& P : V["program"].Items)
			{
				FPhase Ph;
				Ph.Id = P["id"].S();
				Ph.From = P["hours"][0].F(10.f);
				Ph.To = P["hours"][1].F(Ph.From + 1.f);
				Ph.Day = P.Find("day") ? P["day"].I(0) : 0;
				Ph.Venue = P["venue"].S("square");
				Ph.Mode = P["mode"].S("mingle");
				Ph.Pull = P["pull"].F(1.f);
				Ph.Face = P["face"].S();
				Ph.Show = Strings(P["show"]);
				F.Phases.push_back(Ph);
			}
			if (!F.Id.empty())
			{
				Festivals.push_back(F);
			}
		}
		return true;
	}

	int FCalendar::Find(const std::string& Id) const
	{
		for (int I = 0; I < (int)Festivals.size(); I++)
		{
			if (Festivals[I].Id == Id)
			{
				return I;
			}
		}
		return -1;
	}

	bool FCalendar::StartsOn(int Fi, const FDate& Date) const
	{
		if (Fi < 0 || Fi >= (int)Festivals.size())
		{
			return false;
		}
		const FFestival& F = Festivals[Fi];
		// the day the rule names is the shifted one: undo the shift and test the rule's own day
		const FDate D = AddDays(Date, -F.Offset);
		if (!F.Months.empty() && !Has(F.Months, Date.M))
		{
			return false;
		}
		if (F.Rule == "date")
		{
			return D.M == F.Month && D.D == F.DayOfMonth;
		}
		if (F.Rule == "nth_weekday")
		{
			FDate N;
			return D.M == F.Month && NthWeekday(D.Y, F.Month, F.Nth, F.Wd, N) && N == D;
		}
		if (F.Rule == "easter_orthodox" || F.Rule == "easter_western")
		{
			// the shift can carry the day into the year before or after Easter's
			for (const int Y : { Date.Y - 1, Date.Y, Date.Y + 1 })
			{
				const FDate E = F.Rule == "easter_orthodox" ? OrthodoxEaster(Y) : WesternEaster(Y);
				if (AddDays(E, F.Offset) == Date)
				{
					return true;
				}
			}
			return false;
		}
		if (F.Rule == "weekly")
		{
			return Has(F.Weekdays, Weekday(D));
		}
		if (F.Rule == "trigger")
		{
			const auto It = Fired.find(F.Trigger);
			if (It == Fired.end())
			{
				return false;
			}
			for (const FDate& T : It->second)
			{
				if (AddDays(T, F.Offset) == Date)
				{
					return true;
				}
			}
		}
		return false;
	}

	std::vector<FActive> FCalendar::At(const FDate& Date, float Hour) const
	{
		std::vector<FActive> Out;
		for (int Fi = 0; Fi < (int)Festivals.size(); Fi++)
		{
			const FFestival& F = Festivals[Fi];
			// a festival that started up to Days (+1 for a program past midnight) days ago, or starts tomorrow (decorations
			// put up the evening before: DecorFrom < 0)
			for (int Back = -1; Back <= F.Days; Back++)
			{
				const FDate Start = AddDays(Date, -Back);
				const float Rel = Back * 24.f + Hour;
				const float DecorEnd = (F.Days - 1) * 24.f + F.DecorTo;
				if (Rel < F.DecorFrom || Rel >= DecorEnd || !StartsOn(Fi, Start))
				{
					continue;
				}
				FActive A;
				A.Festival = Fi;
				A.Start = Start;
				A.Rel = Rel;
				float BestPull = -1.f;
				for (int Pi = 0; Pi < (int)F.Phases.size(); Pi++)
				{
					const FPhase& Ph = F.Phases[Pi];
					for (int Day = 0; Day < F.Days; Day++)
					{
						if (Ph.Day >= 0 && Ph.Day != Day)
						{
							continue;
						}
						const float From = Day * 24.f + Ph.From, To = Day * 24.f + Ph.To;
						if (Rel >= From && Rel < To)
						{
							A.Phases.push_back(Pi);
							if (Ph.Pull > BestPull)
							{
								BestPull = Ph.Pull;
								A.Phase = Pi;
								A.PhaseT = (Rel - From) / std::max(0.01f, To - From);
							}
						}
					}
				}
				if (A.Phase >= 0)
				{
					std::stable_partition(A.Phases.begin(), A.Phases.end(), [&](int P) { return P == A.Phase; });
				}
				Out.push_back(A);
				break;
			}
		}
		std::stable_sort(Out.begin(), Out.end(), [this](const FActive& A, const FActive& B)
		{
			return Festivals[A.Festival].Priority > Festivals[B.Festival].Priority;
		});
		return Out;
	}

	bool FCalendar::IsDayOff(const FDate& Date, std::string* OutId) const
	{
		for (int Fi = 0; Fi < (int)Festivals.size(); Fi++)
		{
			const FFestival& F = Festivals[Fi];
			if (!F.bHoliday)
			{
				continue;
			}
			for (int Day = 0; Day < F.Days; Day++)
			{
				if ((F.HolidayDays.empty() || Has(F.HolidayDays, Day)) && StartsOn(Fi, AddDays(Date, -Day)))
				{
					if (OutId)
					{
						*OutId = F.Id;
					}
					return true;
				}
			}
		}
		return false;
	}

	bool FCalendar::Next(int Fi, const FDate& From, FDate& Out) const
	{
		const int D0 = DaysFromCivil(From);
		for (int I = 0; I < 366 * 3; I++)
		{
			const FDate D = CivilFromDays(D0 + I);
			if (StartsOn(Fi, D))
			{
				Out = D;
				return true;
			}
		}
		return false;
	}

	std::vector<std::pair<FDate, int>> FCalendar::Upcoming(const FDate& From, int Days) const
	{
		std::vector<std::pair<FDate, int>> Out;
		const int D0 = DaysFromCivil(From);
		for (int I = 0; I < Days; I++)
		{
			const FDate D = CivilFromDays(D0 + I);
			for (int Fi = 0; Fi < (int)Festivals.size(); Fi++)
			{
				if (StartsOn(Fi, D))
				{
					Out.push_back({ D, Fi });
				}
			}
		}
		return Out;
	}

	void FCalendar::Fire(const std::string& Trigger, const FDate& Date)
	{
		std::vector<FDate>& Days = Fired[Trigger];
		if (std::find(Days.begin(), Days.end(), Date) == Days.end())
		{
			Days.push_back(Date);
		}
	}
}
