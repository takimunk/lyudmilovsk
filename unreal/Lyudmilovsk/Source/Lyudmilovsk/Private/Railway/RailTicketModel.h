#pragma once

// The sea railway as a passenger (docs/railway.md "Riding as a passenger"): plain C++, no engine types, so tools/railsim checks
// it offline. Data: Content/Railway/passenger.json (tools/rail_passenger/passenger.py).
//
// One fare buys a ticket at a halt's ticket box; on each ride the day's conductor walks the car from the far end, reaches the
// hero and checks: a ticket in the pocket is punched (it becomes a punched ticket, a keepsake), one punched this ride is waved
// through, none earns a stern word for the first "warns" rides of the game and the fine after that, or (no money for it) the
// order to get off at the next halt. Which conductor works is a pure function of the day.

#include "../People/MiniJson.h"

#include <string>
#include <vector>

namespace LyudTicket
{
	struct FConductor
	{
		std::string Id, Model, Name;
		int Warns = 0;
		std::string Ask, Punch, Again, Warn, Fine, Broke;   // string keys
	};

	enum class EVerdict { Punch, Again, Warn, Fine, Broke };

	struct FCheck
	{
		EVerdict Verdict = EVerdict::Punch;
		std::string Line;        // the conductor's line (key)
		int Fine = 0;            // roubles taken
		bool bTakeTicket = false;  // one ticket leaves the pocket, a punched one comes in
	};

	struct FModel
	{
		int Fare = 5;
		int FineRoubles = 20;
		float WalkCms = 85.f;
		float StartAfter = 12.f;
		std::vector<FConductor> Conductors;
		std::string LastError;

		bool Load(const std::string& Text)
		{
			LyudJson::FValue Root;
			std::string Error;
			if (!LyudJson::Parse(Text, Root, Error))
			{
				LastError = "passenger.json: " + Error;
				return false;
			}
			Fare = Root["fare"].I(5);
			FineRoubles = Root["fine"].I(20);
			WalkCms = Root["walk_cms"].F(85.f);
			StartAfter = Root["start_after"].F(12.f);
			Conductors.clear();
			for (const LyudJson::FValue& V : Root["conductors"].Items)
			{
				FConductor C;
				C.Id = V["id"].S();
				C.Model = V["model"].S();
				C.Name = V["name"].S();
				C.Warns = V["warns"].I();
				C.Ask = V["ask"].S();
				C.Punch = V["punch"].S();
				C.Again = V["again"].S();
				C.Warn = V["warn"].S();
				C.Fine = V["fine"].S();
				C.Broke = V["broke"].S();
				Conductors.push_back(C);
			}
			if (Conductors.empty() || Fare <= 0 || FineRoubles <= 0)
			{
				LastError = "passenger.json: needs a fare, a fine and a conductor";
				return false;
			}
			return true;
		}

		/** The conductor on duty on game day Day (days since some epoch; the same all day). */
		int ConductorOn(long long Day) const
		{
			if (Conductors.empty())
			{
				return -1;
			}
			const unsigned long long H = (unsigned long long)(Day * 2654435761LL + 12345LL);
			return (int)((H >> 7) % (unsigned long long)Conductors.size());
		}

		/**
		 * The check of one ride: Tickets unpunched in the pocket, whether one was punched already this ride, how many rides
		 * without a ticket the hero has had before (Offences) and his money.
		 */
		FCheck Check(int Conductor, int Tickets, bool bPunchedThisRide, int Offences, int Money) const
		{
			FCheck R;
			if (Conductor < 0 || Conductor >= (int)Conductors.size())
			{
				return R;
			}
			const FConductor& C = Conductors[(size_t)Conductor];
			if (bPunchedThisRide)
			{
				R.Verdict = EVerdict::Again;
				R.Line = C.Again;
			}
			else if (Tickets > 0)
			{
				R.Verdict = EVerdict::Punch;
				R.Line = C.Punch;
				R.bTakeTicket = true;
			}
			else if (Offences < C.Warns)
			{
				R.Verdict = EVerdict::Warn;
				R.Line = C.Warn;
			}
			else if (Money >= FineRoubles)
			{
				R.Verdict = EVerdict::Fine;
				R.Line = C.Fine;
				R.Fine = FineRoubles;
			}
			else
			{
				R.Verdict = EVerdict::Broke;
				R.Line = C.Broke;
			}
			return R;
		}

		/** Whether a verdict counts as a ride without a ticket (the next ones get stricter). */
		static bool IsOffence(EVerdict V) { return V == EVerdict::Warn || V == EVerdict::Fine || V == EVerdict::Broke; }

		/**
		 * The conductor's walk along the car's aisle, in cm along the line from the train's reference point: from the end
		 * farther from the hero (Lo..Hi the seats' span) to a stop StandOff short of the hero (At), the seconds it takes.
		 */
		static void Walk(float Lo, float Hi, float At, float StandOff, float& From, float& To)
		{
			const bool bFromHi = (Hi - At) >= (At - Lo);
			From = bFromHi ? Hi : Lo;
			To = bFromHi ? At + StandOff : At - StandOff;
			if ((bFromHi && To > From) || (!bFromHi && To < From))
			{
				To = From;
			}
		}
	};
}
