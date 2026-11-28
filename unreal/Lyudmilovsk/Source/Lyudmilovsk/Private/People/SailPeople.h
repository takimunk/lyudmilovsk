// The sailing school's people (docs/yacht_school.md "People (part F)"), plain C++ (no Unreal types): what each group of the
// school's members does now, from the school's rules (Private/Sailing/SailSchoolModel.h) and the conditions the engine sets
// on the people's life (sail_season, sail_kids, sail_juniors, sail_adults, sail_race, sail_shore, sail_fitout). The game's
// UPeopleLifeSubsystem and tools/peoplesim both call it every few seconds and hand the result to FWorld::SetGroupActivity.
//
// The conditions stay the engine's word: a group's session is followed only while its condition is on (a lesson: sail_kids /
// sail_juniors / sail_adults; a regatta: sail_race; the fitting-out and the laying-up: sail_fitout; a theory evening out of
// season: sail_shore), and sail_shore turns an afloat lesson into the shore lesson (the start's no-go, or a recall: the
// sailors come in and finish ashore). The phase itself comes from the rules' clock (SchoolSessionAt), so the people's parts
// change at the same minutes as the engine's. Before a session the members are called LeadH early to the first part
// (changing, the briefing) while sail_season is on, so they are there when it begins.
#pragma once

#include "PeopleSim.h"
#include "../Sailing/SailSchoolModel.h"

#include <string>
#include <vector>

namespace LyudPeople
{
	namespace SailPeople
	{
		/** The conditions the engine sets (UPeopleLifeSubsystem::SetCondition), as read back from the life. */
		struct FConds
		{
			bool bSeason = false, bKids = false, bJuniors = false, bAdults = false, bRace = false, bShore = false, bFitOut = false;
		};

		inline FConds ReadConds(const FWorld& W)
		{
			FConds C;
			C.bSeason = W.IsCondition("sail_season");
			C.bKids = W.IsCondition("sail_kids");
			C.bJuniors = W.IsCondition("sail_juniors");
			C.bAdults = W.IsCondition("sail_adults");
			C.bRace = W.IsCondition("sail_race");
			C.bShore = W.IsCondition("sail_shore");
			C.bFitOut = W.IsCondition("sail_fitout");
			return C;
		}

		/** The model's conditions as the engine would set them (tools/peoplesim drives the life with these). */
		inline FConds FromModel(const LyudSail::FSchoolConditions& M)
		{
			FConds C;
			C.bSeason = M.bSeason; C.bKids = M.bKids; C.bJuniors = M.bJuniors; C.bAdults = M.bAdults;
			C.bRace = M.bRace; C.bShore = M.bShore; C.bFitOut = M.bFitOut;
			return C;
		}

		struct FProgram
		{
			std::string Group, Activity;
		};

		/** Every group the school's programmes use (sailing_members.json's groups): Apply clears them before setting. */
		inline const std::vector<std::string>& Groups()
		{
			static const std::vector<std::string> G = { "kids", "juniors", "adults", "race", "race_optimist", "race_cadet", "race_luch",
				"all", "theory", "staff", "crew" };
			return G;
		}

		constexpr double LeadH = 0.4;       // called to the first part so long before a session (24 min: the walk, a minute to spare)

		/** The groups a session calls and the activity of its part. */
		inline void SessionGroups(const LyudSail::FTimetableSession& S, int Weekday, const std::string& Act, std::vector<FProgram>& Out)
		{
			if (Act.empty())
			{
				return;
			}
			if (S.Kind == "fitout" || S.Kind == "layup")
			{
				Out.push_back({ "staff", Act });
				if (Weekday >= 5) Out.push_back({ "crew", Act });       // the members come to help at the weekend
				return;
			}
			if (S.Group == "race")
			{
				Out.push_back({ "race", Act });
				static const char* const Cls[] = { "race_optimist", "race_cadet", "race_luch" };
				for (int C = 0; C < 3; C++) if (S.Classes >> C & 1) Out.push_back({ Cls[C], Act });
				return;
			}
			if (S.Group == "all")
			{
				Out.push_back({ "all", Act });      // out of season: the head coach's theory evening, the bosun's repairs
				if (Weekday == 5) Out.push_back({ "theory", Act });     // the members on Saturdays
				return;
			}
			Out.push_back({ S.Group, Act });
		}

		/** Whether the engine's conditions say this session is on. */
		inline bool SessionOn(const LyudSail::FTimetableSession& S, const FConds& C)
		{
			if (S.Kind == "fitout" || S.Kind == "layup") return C.bFitOut;
			if (S.Group == "race") return C.bRace;
			if (S.Group == "kids") return C.bKids;
			if (S.Group == "juniors") return C.bJuniors;
			if (S.Group == "adults") return C.bAdults;
			return C.bShore;        // "all": the theory evening
		}

		/** What the school's groups do at Date, Hour. */
		inline std::vector<FProgram> ProgramsAt(const LyudSail::FSchoolParams& P, const LyudSail::FSchoolDate& Date, double Hour, bool bDayOff,
			const FConds& C)
		{
			std::vector<FProgram> Out;
			const int Wd = LyudSail::SchoolWeekday(Date);
			const LyudSail::FSchoolNow Plan = LyudSail::SchoolSessionAt(P, Date, Hour, bDayOff);
			if (Plan.bOn && SessionOn(Plan.Session, C))
			{
				LyudSail::FSchoolNow Now = Plan;
				if (C.bShore && Plan.Session.bAfloat)
				{
					// the start's decision was no-go (or a recall): the session is held ashore
					std::vector<LyudSail::FSchoolSessionState> St(1);
					St[0].Key = Plan.Key;
					St[0].Go = LyudSail::ESchoolGo::Shore;
					Now = LyudSail::SchoolSessionAt(P, Date, Hour, bDayOff, &St);
				}
				SessionGroups(Now.Session, Wd, Now.Activity, Out);
			}
			// the next session's people on their way: called to its first part a little before it begins
			for (const LyudSail::FTimetableSession& S : LyudSail::SchoolSessionsOn(P, Date, bDayOff))
			{
				if (Hour >= S.From || Hour < S.From - LeadH || S.Phases.empty() || (Plan.bOn && Plan.Key == S.Key))
				{
					continue;
				}
				const bool bOpenSeason = S.Kind == "lesson" || S.Kind == "race" || (S.Kind == "theory" && S.Group != "all");
				if (bOpenSeason && !C.bSeason)
				{
					continue;
				}
				SessionGroups(S, Wd, S.Phases[0].Activity, Out);
			}
			return Out;
		}

		/** Hands the programmes to the life: the school's groups cleared, then set (a group named twice: the first wins). */
		inline void Apply(FWorld& W, const std::vector<FProgram>& Programs)
		{
			for (const std::string& G : Groups())
			{
				W.SetGroupActivity(G, std::string());
			}
			std::vector<std::string> Done;
			for (const FProgram& Pr : Programs)
			{
				bool bDone = false;
				for (const std::string& D : Done) bDone |= D == Pr.Group;
				if (!bDone)
				{
					W.SetGroupActivity(Pr.Group, Pr.Activity);
					Done.push_back(Pr.Group);
				}
			}
		}
	}
}
