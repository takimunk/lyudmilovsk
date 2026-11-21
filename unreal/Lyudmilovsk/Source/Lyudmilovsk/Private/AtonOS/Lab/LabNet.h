// The lab network «ЛАБ-2» behind the Academgorodok terminals (docs/aton_lab.md), in plain C++ so that tools/atonsim
// checks it outside the engine: the duty shift's password, which letters of the old correspondence are open, the
// telescope's queue by night, the weather station's small sums, and everything the hero did there as one saved line.
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace AtonLab
{
	/** The digits of what was typed: ASCII and full-width (a Japanese keyboard's) digits, everything else dropped. */
	std::string PasswordDigits(const std::string& Utf8Typed);
	/** The duty shift's password, however it was typed: "0314", "03:14", "０３１４". */
	bool PasswordOk(const std::string& Utf8Typed);

	/** A letter of Content/AtonOS/lab_mail.json; its words are ui.aton.lab.mail.<Id>.from|to|date|subject|body. */
	struct FLabLetter
	{
		std::string Id;
		/** "guest" (anyone at the terminal) or "staff" (the duty shift, after the password). */
		std::string Box;
		/** A letter that must have been read first (a thread opens letter by letter); empty: none. */
		std::string After;
	};
	bool LoadLetters(const std::string& JsonText, std::vector<FLabLetter>& Out, std::string* Error = nullptr);

	// ---- the telescope's queue: four slots a night, a night = noon to noon
	constexpr int ScopeSlots = 4;
	constexpr int ScopeTargets = 6;
	constexpr int ScopeStaff = 5;
	/** The hour on the clock of slot 0..3: 22, 0, 2, 4. */
	int ScopeSlotHour(int Slot);
	/** Hours from the game's first midnight to the slot's start. */
	double ScopeSlotAt(int Night, int Slot);
	/** The night (0 = the one that follows the game's first day) hour HoursSinceFirstMidnight belongs to. */
	int ScopeNightOf(double HoursSinceFirstMidnight);
	/** "moon", "evening_star", "giant", "rings", "double", "field14": words ui.aton.lab.scope.target.<id>. */
	const char* ScopeTargetId(int Target);

	struct FScopeSlot
	{
		int Target = 0;
		/** Who has it: 0..ScopeStaff-1 (ui.aton.scope.staff.<n+1>), or -1 free. */
		int Owner = -1;
	};
	/** The night's programme, the same every time it is asked; one or two of the four slots are free. */
	std::vector<FScopeSlot> ScopeNight(int Night);

	struct FScopeResult
	{
		int Night = 0;
		int Slot = 0;
		int Target = 0;
		/** The sky was clear enough: the plate came out. */
		bool bClear = false;
	};

	// ---- the weather station's sums
	/** 0 N, 1 NE ... 7 NW: where the wind blows from. */
	int Compass8(float FromDeg);
	/** Eighths of the sky covered, 0..8. */
	int Oktas(float Cover);
	int Beaufort(float SpeedMs);
	/** W/m2 on the mast's pyranometer: the sun's height through the cloud. */
	float SolarFlux(float SunAltitudeDeg, float CloudCover);

	/** What the hero has done on the lab network; kept in the save as one line. */
	class FLabState
	{
	public:
		bool bStaff = false;
		int FailedLogins = 0;
		std::set<std::string> Read;
		std::set<std::string> Endings;
		/** By the chess program's level 1..3. */
		int ChessWon[3] = { 0, 0, 0 };
		int ChessDrawn[3] = { 0, 0, 0 };
		int ChessLost[3] = { 0, 0, 0 };
		int ChessLevel = 1;
		/** The games left unfinished (the models' own Save() lines). */
		std::string ChessSave;
		std::string AdvSave;
		/** Night -> the slot the hero took. */
		std::map<int, int> Bookings;
		std::vector<FScopeResult> Results;

		/** The letters of a box that show now, in the file's order: a staff box only for the duty shift, a letter only once its After was read. */
		std::vector<const FLabLetter*> Visible(const std::vector<FLabLetter>& Letters, const std::string& Box) const;
		int Unread(const std::vector<FLabLetter>& Letters, const std::string& Box) const;

		/** Takes a free slot of a night that has not begun; one slot a night. */
		bool Book(int Night, int Slot, double NowHours);
		bool Unbook(int Night, double NowHours);
		/**
		 * Turns every booked slot that has passed into a result (once): CloudAt gives the cloud cover 0..1 at an hour since
		 * the game's first midnight. Returns how many results were added.
		 */
		template <typename TCloudAt>
		int Resolve(double NowHours, TCloudAt CloudAt)
		{
			int Added = 0;
			for (const std::pair<const int, int>& B : Bookings)
			{
				const double At = ScopeSlotAt(B.first, B.second);
				if (At + 2.0 > NowHours || HasResult(B.first))
				{
					continue;
				}
				FScopeResult R;
				R.Night = B.first;
				R.Slot = B.second;
				R.Target = ScopeNight(B.first)[(size_t)B.second].Target;
				R.bClear = CloudAt(At + 1.0) < 0.6f;
				Results.push_back(R);
				++Added;
			}
			return Added;
		}
		bool HasResult(int Night) const;

		std::string Save() const;
		/** False on a line it cannot read (the state is left as it was). */
		bool Restore(const std::string& Line);
	};
}
