// The townsfolk's characters and manners (Private/People/PeopleManners.cpp, docs/town_manners.md) run on the town's
// people outside the engine.
//   tools/mannersim/run.sh            checks
//   tools/mannersim/run.sh day        the mild day hour by hour
//   tools/mannersim/run.sh who [key]  personas
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/PeopleManners.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>

using namespace LyudPeople;

static const char* DataDir = "unreal/Lyudmilovsk/Content/PeopleData";
static const char* AnimalsDir = "unreal/Lyudmilovsk/Content/AnimalsData";
static int Failures = 0;

#define CHECK(Cond, ...) do { if (Cond) { std::printf("  ok   "); } else { std::printf("  FAIL "); Failures++; } std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

static std::string ReadFile(const std::string& Path)
{
	std::ifstream F(Path, std::ios::binary);
	std::stringstream SS;
	SS << F.rdbuf();
	return SS.str();
}

struct FClock
{
	double Seconds = 0.0;       // since Monday 00:00
	void Set(FWorld& W) const
	{
		const double H = Seconds / 3600.0;
		W.Hour = (float)std::fmod(H, 24.0);
		W.Weekday = (int)std::fmod(std::floor(H / 24.0), 7.0);
		W.Time = Seconds;
		W.Sunrise = 7.75f;
		W.Sunset = 17.25f;
		W.bSunUp = W.Hour > W.Sunrise && W.Hour < W.Sunset;
		W.SunYaw = W.Hour < 12.f ? 0.f : 30.f;
	}
	/** The sun's height for the manners: a sine over the day, 28 deg at noon, under the horizon at night. */
	float SunAlt(const FWorld& W) const
	{
		const float T = (W.Hour - W.Sunrise) / (W.Sunset - W.Sunrise);
		return T > 0.f && T < 1.f ? 28.f * std::sin(T * 3.14159f) : -12.f;
	}
};

static bool LoadAll(FWorld& W, FManners& M)
{
	if (!W.Load(DataDir, AnimalsDir))
	{
		std::printf("load failed: %s\n", W.LastError.c_str());
		return false;
	}
	W.AssignHomes();
	if (!M.Load(ReadFile(std::string(DataDir) + "/manners.json")))
	{
		std::printf("manners.json: %s\n", M.LastError.c_str());
		return false;
	}
	M.Bind(W);
	M.ApplyLikes(W);
	return true;
}

/** Whoever the game could draw: in the world, not inside a building's shell. */
static bool Drawn(const FWorld& W, const FPerson& P)
{
	return P.Pose != EPose::Hidden && P.State != EState::Away && P.State != EState::Riding && !W.InsideShell(P) && !P.bExternal;
}

struct FDay
{
	const char* Name = "";
	float TempC = 12.f;
	float RainFrom = -1.f, RainTo = -1.f;       // hours of a shower
	bool bPrint = false;
	// results
	std::map<std::string, double> Seconds;      // manner -> person-seconds (people out of doors or seen indoors)
	std::map<std::string, double> ActMinutes;   // activity -> person-minutes
	double DrawnSeconds = 0.0, MannerSeconds = 0.0;
	int Wrong = 0;
	std::string WrongWhat;
	int MinDistinctDay = 99;
	int OutBeforeRain = -1, OutInRain = -1, UmbrellasInRain = 0, UmbrellasDry = 0, CoverInRain = 0;
	int Waves = 0, Glances = 0;
	double UpdateNs = 0.0;
	long Updates = 0;
	int BenchNoon = 0;
	int Ritualists = 0, RitualKept = 0;
	int FrontEvening = 0;       // people on the sea front and the squares, summed over the evening's samples (18:30-21:00)
	bool bDates = false;
	std::map<std::string, int> DateHalf;
	int DatesClose = 0;         // of the kept: side by side (within 3 m) at some minute
	int AllDates = 0, Dates = 0, DatesKept = 0;      // pairs of friends with a weekly date; those whose day is this Wednesday; kept
	// ten minutes of something in the sky at 21:00: people out of doors, and those of them looking up, pointing or photographing,
	// during it and in the ten minutes before; and an arm pointed at the sky at any other time
	int SkyOut = 0, SkyUp = 0, BeforeOut = 0, BeforeUp = 0, PointNoShow = 0;
};

static void RunDay(FDay& D)
{
	FWorld W;
	FManners M;
	if (!LoadAll(W, M))
	{
		Failures++;
		return;
	}
	FClock C;
	C.Seconds = (2 * 24.0 + 5.0) * 3600.0;       // Wednesday 05:00
	C.Set(W);
	W.TempC = D.TempC;
	W.Resettle();
	std::vector<int> Shown;
	FMannerNow Now;
	Now.TempC = D.TempC;
	const float Dt = 1.f;
	std::map<std::string, int> HourCount;
	int LastHour = -1;
	std::vector<char> RitualKept;
	std::vector<char> DateKept;
	for (double T = 0.0; T < 19.0 * 3600.0; T += Dt)        // till midnight
	{
		C.Seconds += Dt;
		C.Set(W);
		const bool bRain = D.RainFrom >= 0.f && W.Hour >= D.RainFrom && W.Hour < D.RainTo;
		W.bRain = bRain;
		W.Rain = bRain ? 0.7f : 0.f;
		W.Step(Dt);
		W.Events.clear();
		if ((long)std::lround(T) % 60 == 0)
		{
			// friends whose weekly date is today: the two of them at it together
			DateKept.resize(W.People.size(), 0);
			for (const FPerson& P : W.People)
			{
				if (P.DateWith > P.Id && P.DateDay == W.Weekday && P.State == EState::Doing && P.Activity == P.DateAct)
				{
					const FPerson& F = W.People[(size_t)P.DateWith];
					if (F.State == EState::Doing && F.Activity == F.DateAct && (F.Pos - P.Pos).Len2D() < 40.f)
					{
						DateKept[(size_t)P.Id] = (F.Pos - P.Pos).Len2D() < 3.f || DateKept[(size_t)P.Id] == 2 ? 2 : 1;
					}
				}
			}
			if (D.bDates)
			{
				for (const FPerson& P : W.People)
				{
					if (P.DateWith >= 0 && P.DateDay == W.Weekday && std::fabs(W.Hour - (P.DateHour + 0.5f)) < 0.009f)
					{
						const bool bAt = P.State == EState::Doing && P.Activity == P.DateAct;
						D.DateHalf[W.Activities[(size_t)P.DateAct].Id + " -> " + (bAt ? (P.Place == P.DatePlace ? "AT IT" : "at it elsewhere")
							: P.Activity < 0 ? "nothing" : W.Activities[(size_t)P.Activity].Id + (P.State == EState::Doing ? "" : " (going)"))]++;
					}
				}
			}
			// who is at their ritual within the hour of it
			RitualKept.resize(W.People.size(), 0);
			for (const FPerson& P : W.People)
			{
				if (P.RitualAct >= 0 && P.Activity == P.RitualAct && P.State == EState::Doing && std::fabs(W.Hour - P.RitualHour) < 1.f)
				{
					RitualKept[(size_t)P.Id] = 1;
				}
			}
		}
		Now.Clock = (float)T;
		Now.Rain = W.Rain;
		Now.SunAlt = C.SunAlt(W);
		Now.Cloud = bRain ? 0.95f : 0.3f;
		const bool bShow = !bRain && W.Hour >= 21.f && W.Hour < 21.f + 10.f / 60.f;
		const bool bBeforeShow = !bRain && W.Hour >= 21.f - 10.f / 60.f && W.Hour < 21.f;
		Now.SkyShow = bShow ? 0.9f : 0.f;
		Shown.clear();
		for (const FPerson& P : W.People)
		{
			if (Drawn(W, P))
			{
				Shown.push_back(P.Id);
			}
			else
			{
				M.Release(P.Id, Now.Clock);
			}
		}
		const auto T0 = std::chrono::high_resolution_clock::now();
		for (const int Id : Shown)
		{
			M.Update(W, Id, Now);
		}
		D.UpdateNs += std::chrono::duration<double, std::nano>(std::chrono::high_resolution_clock::now() - T0).count();
		D.Updates += (long)Shown.size();
		const int Sec = (int)std::lround(T);
		if (Sec % 2 == 0)
		{
			// in the game only the few dozen nearest are drawn; here everybody, so only neighbours are paired
			M.Passing(W, Shown.data(), (int)Shown.size(), Now);
		}
		std::set<std::string> Distinct;
		int Out = 0;
		for (const int Id : Shown)
		{
			const FPerson& P = W.People[Id];
			const FMannerState& S = *M.StateOf(Id);
			const char* Name = MannerName(S.Cur);
			D.Seconds[Name] += Dt;
			D.DrawnSeconds += Dt;
			Out += !P.bIndoors ? 1 : 0;
			if (Sec % 10 == 0 && !P.bIndoors && (bShow || bBeforeShow))
			{
				const bool bUp = S.Cur == EManner::LookSky || S.Cur == EManner::PointSky || (S.Cur == EManner::Photo && bShow);
				(bShow ? D.SkyOut : D.BeforeOut)++;
				(bShow ? D.SkyUp : D.BeforeUp) += bUp ? 1 : 0;
			}
			// (a pointing begun in the show's last seconds may run a few seconds over)
			D.PointNoShow += S.Cur == EManner::PointSky && !bShow && !(W.Hour >= 21.f && W.Hour < 21.f + 11.f / 60.f) ? 1 : 0;
			if (S.Cur != EManner::None)
			{
				D.MannerSeconds += Dt;
				Distinct.insert(Name);
				const FMannerDef* Def = M.DefOf(S);
				// what must never be seen
				const bool bBadPose = Def && !S.bReact && !((Def->Poses >> (int)P.Pose) & 1u);
				const bool bSmokeKid = S.Cur == EManner::Smoke && P.Age < 18;
				const bool bSmokeIn = S.Cur == EManner::Smoke && P.bIndoors;
				const bool bUmbrellaIn = S.Cur == EManner::Umbrella && P.bIndoors;
				const bool bLying = P.Pose == EPose::Sleep;
				if (bBadPose || bSmokeKid || bSmokeIn || bUmbrellaIn || bLying)
				{
					D.Wrong++;
					D.WrongWhat = std::string(Name) + " by " + P.Key + " (" + PoseName(P.Pose) + ")";
				}
				if (S.Cur == EManner::Umbrella)
				{
					(bRain ? D.UmbrellasInRain : D.UmbrellasDry) += Sec % 60 == 0 ? 1 : 0;
				}
				if (S.Cur == EManner::CoverHead && bRain && Sec % 60 == 0)
				{
					D.CoverInRain++;
				}
				if (S.Cur == EManner::Wave && S.Start == Now.Clock)
				{
					D.Waves++;
				}
			}
			if (S.LookAt >= 0 && S.LookUntil > Now.Clock && Sec % 2 == 0)
			{
				D.Glances++;
			}
		}
		if (Sec % 60 == 0)
		{
			for (const FPerson& P : W.People)
			{
				if (P.bTown && P.Activity >= 0 && P.State == EState::Doing)
				{
					D.ActMinutes[W.Activities[P.Activity].Id] += 1.0;
				}
			}
		}
		if (Sec % 600 == 0 && W.Hour >= 18.5f && W.Hour < 21.f)
		{
			// the sea front of an evening
			for (const FPerson& P : W.People)
			{
				if (P.bTown && P.State == EState::Doing && P.Place >= 0 && !P.bIndoors)
				{
					const std::string& K = W.Places[(size_t)P.Place].Kind;
					D.FrontEvening += K == "promenade" || K == "pier" || K == "fountain" || K == "square" ? 1 : 0;
				}
			}
		}
		if (Sec % 600 == 0 && W.Hour > 9.f && W.Hour < 17.f && !bRain)
		{
			D.MinDistinctDay = std::min(D.MinDistinctDay, (int)Distinct.size());
		}
		if (D.RainFrom >= 0.f)
		{
			// free people out of doors just before the shower and a quarter of an hour into it
			int Free = 0;
			for (const int Id : Shown)
			{
				const FPerson& P = W.People[Id];
				Free += !P.bIndoors && P.State == EState::Doing && P.Activity >= 0 && W.Activities[P.Activity].bFree && !W.Activities[P.Activity].bGather ? 1 : 0;
			}
			if (D.OutBeforeRain < 0 && W.Hour >= D.RainFrom - 0.02f)
			{
				D.OutBeforeRain = Free;
			}
			if (D.OutInRain < 0 && W.Hour >= D.RainFrom + 0.25f)
			{
				D.OutInRain = Free;
			}
		}
		if (std::fabs(W.Hour - 12.f) < 0.001f || (W.Hour >= 12.f && D.BenchNoon == 0 && W.Hour < 12.01f))
		{
			for (const FPerson& P : W.People)
			{
				D.BenchNoon += P.bTown && P.State == EState::Doing && P.Activity >= 0 && W.Activities[P.Activity].Id == "sit_bench" ? 1 : 0;
			}
		}
		if (D.bPrint)
		{
			if (Sec % 300 == 0)
			{
				for (const int Id : Shown)
				{
					HourCount[MannerName(M.StateOf(Id)->Cur)]++;
				}
				HourCount["(drawn)"] += (int)Shown.size();
				HourCount["(outdoors)"] += Out;
			}
			const int H = (int)W.Hour;
			if (H != LastHour && LastHour >= 0)
			{
				std::printf("%02d:00  drawn %4d  outdoors %4d |", LastHour, HourCount["(drawn)"] / 12, HourCount["(outdoors)"] / 12);
				for (const auto& KV : HourCount)
				{
					if (KV.first[0] != '(' && KV.first != "none" && KV.second / 12 > 0)
					{
						std::printf(" %s %d", KV.first.c_str(), KV.second / 12);
					}
				}
				std::printf("\n");
				HourCount.clear();
			}
			LastHour = H;
		}
	}
	for (const FPerson& P : W.People)
	{
		D.AllDates += P.DateWith > P.Id ? 1 : 0;
		D.Dates += P.DateWith > P.Id && P.DateDay == 2 ? 1 : 0;
		D.DatesKept += P.DateWith > P.Id && (size_t)P.Id < DateKept.size() && DateKept[(size_t)P.Id] ? 1 : 0;
		D.DatesClose += P.DateWith > P.Id && (size_t)P.Id < DateKept.size() && DateKept[(size_t)P.Id] == 2 ? 1 : 0;
	}
	for (const FPerson& P : W.People)
	{
		D.Ritualists += P.RitualAct >= 0 ? 1 : 0;
		D.RitualKept += P.RitualAct >= 0 && (size_t)P.Id < RitualKept.size() && RitualKept[(size_t)P.Id] ? 1 : 0;
	}
}

/** At their household's outing now: at the activity, at its place or (those that wander: a stroll, the fair) within 220 m of it. */
static bool AtOuting(const FWorld& W, const FPerson& P)
{
	return P.OutingAct >= 0 && W.Weekday == P.OutingDay && P.State == EState::Doing && P.Activity == P.OutingAct
		&& (P.Place == P.OutingPlace || (P.Pos - W.Places[(size_t)P.OutingPlace].Portal).Len2D() < 220.f);
}

/** A weekend: how many households keep their outing (two and more of them at it, at its place, at the same minute). */
static void Outings(bool bPrint)
{
	FWorld W;
	FManners M;
	if (!LoadAll(W, M))
	{
		Failures++;
		return;
	}
	FClock C;
	C.Seconds = (5 * 24.0 + 7.0) * 3600.0;       // Saturday 07:00
	C.Set(W);
	W.TempC = 12.f;
	W.Resettle();
	std::map<std::string, int> Size, Best, Now;
	std::map<std::string, int> ByAct, Half, Lead;
	long WalkPairs = 0, WalkNear = 0, PlayMinutes = 0, MindedMinutes = 0, MindAlone = 0, MindMinutes = 0;
	for (const FPerson& P : W.People)
	{
		if (P.OutingAct >= 0)
		{
			Size[P.Household]++;
		}
	}
	for (const FPerson& P : W.People)
	{
		if (P.OutingAct >= 0 && Best.find(P.Household) == Best.end())
		{
			Best[P.Household] = 0;
			ByAct[W.Activities[(size_t)P.OutingAct].Id]++;
		}
	}
	const float Dt = 1.f;
	for (double T = 0.0; T < 38.0 * 3600.0; T += Dt)        // till Sunday 21:00
	{
		C.Seconds += Dt;
		C.Set(W);
		W.Step(Dt);
		W.Events.clear();
		if ((long)std::lround(T) % 60 != 0)
		{
			continue;
		}
		if (bPrint)
		{
			for (const FPerson& P : W.People)
			{
				if (P.OutingAct >= 0 && W.Weekday == P.OutingDay && std::fabs(W.Hour - (P.OutingHour + 0.5f)) < 0.009f)
				{
					const bool bAt = AtOuting(W, P);
					std::string What = bAt ? "AT IT" : P.Activity < 0 ? "nothing" : W.Activities[(size_t)P.Activity].Id;
					if (!bAt && P.Activity == P.OutingAct)
					{
						What += P.State != EState::Doing ? " (on the way)" : " (far off)";
					}
					Half[W.Activities[(size_t)P.OutingAct].Id + " -> " + What]++;
				}
			}
		}
		// children out at play, and those of them with a grown-up of theirs standing by
		for (const FPerson& P : W.People)
		{
			if (P.Age < 12 && P.State == EState::Doing && (P.Activity == W.PlayActs[0] || P.Activity == W.PlayActs[1]) && P.Home >= 0)
			{
				bool bGrownUp = false, bBy = false;
				for (const int R : W.Places[(size_t)P.Home].Residents)
				{
					const FPerson& Q = W.People[(size_t)R];
					bGrownUp |= Q.Age >= 18 && Q.Household == P.Household;
					bBy |= Q.Activity == W.MindAct && Q.State == EState::Doing && Q.Household == P.Household && (Q.Pos - P.Pos).Len2D() < 80.f;
				}
				PlayMinutes += bGrownUp ? 1 : 0;
				if (bPrint && !bBy)
				{
					for (const int R : W.Places[(size_t)P.Home].Residents)
					{
						const FPerson& Q = W.People[(size_t)R];
						if (Q.Age >= 18 && Q.Household == P.Household)
						{
							const int Sl = 0;
							const float Sc = W.ChildToMind(Q) < 0 ? -1e9f : 0.f;
							Half[std::string("unminded, grown-up at: ") + (Q.Activity < 0 ? "nothing" : W.Activities[(size_t)Q.Activity].Id)
								+ (Q.State == EState::Doing ? "" : " (going)") + (Sc < -1e8f ? " [can't]" : Sl < 0 ? " [no slot]" : "")]++;
						}
					}
				}
				MindedMinutes += bBy ? 1 : 0;
			}
			MindAlone += P.Activity == W.MindAct && P.State == EState::Doing && W.ChildToMind(P) < 0 ? 1 : 0;
			MindMinutes += P.Activity == W.MindAct && P.State == EState::Doing ? 1 : 0;
		}
		Now.clear();
		Lead.clear();
		for (const FPerson& P : W.People)
		{
			if (AtOuting(W, P))
			{
				const int N = ++Now[P.Household];
				int& B = Best[P.Household];
				B = std::max(B, N);
				// those of them who walk (a stroll, the fair): how far from the first of the household at it
				if (W.Activities[(size_t)P.Activity].bWander)
				{
					const auto First = Lead.find(P.Household);
					if (First == Lead.end())
					{
						Lead[P.Household] = P.Id;
					}
					else
					{
						WalkPairs++;
						WalkNear += (W.People[(size_t)First->second].Pos - P.Pos).Len2D() < 40.f ? 1 : 0;
					}
				}
			}
		}
	}
	int Kept = 0, Whole = 0, Lone = 0;
	for (const auto& KV : Best)
	{
		Kept += KV.second >= 2 ? 1 : 0;
		Whole += KV.second >= Size[KV.first] ? 1 : 0;
		Lone += KV.second == 1 ? 1 : 0;
	}
	if (bPrint)
	{
		for (const auto& KV : ByAct)
		{
			std::printf("  %s: %d households\n", KV.first.c_str(), KV.second);
		}
		for (const auto& KV : Half)
		{
			if (KV.second >= 4)
			{
				std::printf("  half an hour in: %s: %d\n", KV.first.c_str(), KV.second);
			}
		}
	}
	CHECK((int)Best.size() > 150 && Kept * 2 > (int)Best.size(),
		"households keep their weekend outing (%d of %d with two and more of them there together, %d of those whole; %d with one alone)",
		Kept, (int)Best.size(), Whole, Lone);
	CHECK(PlayMinutes > 500 && MindedMinutes * 5 > PlayMinutes * 2 && MindAlone * 10 < MindMinutes + 10,
		"a grown-up stands by while their child plays (%ld of %ld child-minutes; %ld of %ld minding minutes linger after the child has gone)",
		MindedMinutes, PlayMinutes, MindAlone, MindMinutes);
	CHECK(WalkPairs > 500 && WalkNear * 10 > WalkPairs * 7, "a household out walking keeps together (%ld of %ld person-minutes within 40 m of the first of them)",
		WalkNear, WalkPairs);
}

static void Personas()
{
	FWorld W;
	FManners M;
	if (!LoadAll(W, M))
	{
		Failures++;
		return;
	}
	for (const std::string& Wn : M.Warnings)
	{
		std::printf("  warning: %s\n", Wn.c_str());
	}
	CHECK(M.Warnings.empty(), "manners.json reads clean (%zu rows)", M.Defs.size());
	// every manner but None has a row
	std::string Missing;
	for (int I = 1; I < NumManners; I++)
	{
		bool bRow = false;
		for (const FMannerDef& D : M.Defs)
		{
			bRow |= (int)D.Manner == I;
		}
		if (!bRow)
		{
			Missing += std::string(" ") + MannerName((EManner)I);
		}
	}
	CHECK(Missing.empty(), "every manner has a row%s%s", Missing.empty() ? "" : ": missing", Missing.c_str());
	int Adults = 0, Kids = 0, Town = 0, Left = 0, KidSmokers = 0;
	std::map<std::string, int> Habit, HabitKids;
	double Warm = 0.0, Nerve = 0.0, Cur = 0.0, Tidy = 0.0;
	int Warm25 = 0, Warm75 = 0;
	for (const FPerson& P : W.People)
	{
		if (!P.bTown)
		{
			continue;
		}
		const FPersona& Who = *M.PersonaOf(P.Id);
		Town++;
		(P.Age >= 18 ? Adults : Kids)++;
		Left += Who.bLeftHanded ? 1 : 0;
		Warm += Who.Warmth;
		Nerve += Who.Nerve;
		Cur += Who.Curiosity;
		Tidy += Who.Tidy;
		Warm25 += Who.Warmth < 0.35f ? 1 : 0;
		Warm75 += Who.Warmth > 0.65f ? 1 : 0;
		for (uint32_t B = 1; B <= HabitHandsBack; B <<= 1)
		{
			if (Who.Has(B))
			{
				(P.Age >= 18 ? Habit : HabitKids)[HabitName(B)]++;
			}
		}
		KidSmokers += P.Age < 18 && Who.Has(HabitSmoker) ? 1 : 0;
	}
	std::printf("  %d townsfolk (%d grown-ups, %d under 18): warmth %.2f nerve %.2f curiosity %.2f tidy %.2f on average; left-handed %d\n", Town, Adults, Kids,
		Warm / Town, Nerve / Town, Cur / Town, Tidy / Town, Left);
	std::printf("  habits, grown-ups:");
	for (const auto& KV : Habit)
	{
		std::printf(" %s %d (%.0f%%)", KV.first.c_str(), KV.second, 100.0 * KV.second / Adults);
	}
	std::printf("\n  habits, under 18:");
	for (const auto& KV : HabitKids)
	{
		std::printf(" %s %d (%.0f%%)", KV.first.c_str(), KV.second, 100.0 * KV.second / std::max(1, Kids));
	}
	std::printf("\n");
	const double Smokers = 100.0 * Habit["smoker"] / Adults;
	CHECK(Smokers > 8.0 && Smokers < 26.0, "a minority of grown-ups smoke (%.0f%%)", Smokers);
	CHECK(KidSmokers == 0, "nobody under 18 smokes (%d)", KidSmokers);
	CHECK(Left > Town * 0.05 && Left < Town * 0.16, "about one in ten is left-handed (%d of %d)", Left, Town);
	CHECK(Warm25 > Town * 0.08 && Warm75 > Town * 0.08, "both the reserved and the warm are about (%d reserved, %d warm)", Warm25, Warm75);
	CHECK(Habit["umbrella"] > Adults * 0.35 && Habit["umbrella"] < Adults * 0.85, "some carry an umbrella, some don't (%d of %d grown-ups)", Habit["umbrella"], Adults);
	CHECK(Habit["feeder"] > 20 && Habit["knitter"] > 8 && Habit["photographer"] > 8 && Habit["reader"] > 60 && Habit["dozer"] > 30,
		"every habit has its people (feeders %d, knitters %d, photographers %d, readers %d, dozers %d)", Habit["feeder"], Habit["knitter"],
		Habit["photographer"], Habit["reader"], Habit["dozer"]);
	// the same person, the same character
	const FPersona A = MakePersona(W.People[W.People.size() / 2]), B = MakePersona(W.People[W.People.size() / 2]);
	CHECK(A.Habits == B.Habits && A.Warmth == B.Warmth, "a persona is the same every time it is made");
}

static void Noticing()
{
	FWorld W;
	FManners M;
	if (!LoadAll(W, M))
	{
		Failures++;
		return;
	}
	FClock C;
	C.Seconds = (2 * 24.0 + 13.0) * 3600.0;      // Wednesday 13:00
	C.Set(W);
	W.Resettle();
	for (int I = 0; I < 600; I++)
	{
		C.Seconds += 1.0;
		C.Set(W);
		W.Step(1.f);
		W.Events.clear();
	}
	// somebody standing out of doors, and the player walking past, running at them, standing in their face
	int Who = -1;
	for (const FPerson& P : W.People)
	{
		if (P.bTown && Drawn(W, P) && !P.bIndoors && P.State == EState::Doing && (P.Pose == EPose::Look || P.Pose == EPose::Stand) && P.Age >= 18)
		{
			Who = P.Id;
			break;
		}
	}
	CHECK(Who >= 0, "somebody stands out of doors at 13:10");
	if (Who < 0)
	{
		return;
	}
	const FPerson& P = W.People[Who];
	const float Yaw = P.Yaw / 57.29578f;
	const V3 Fwd(std::cos(Yaw), std::sin(Yaw), 0.f);
	FMannerNow Now;
	Now.SunAlt = 25.f;
	FHeroNow Hero;
	Hero.bThere = true;
	// walking past in front of them, 3 m off, for 20 s: sooner or later they look
	bool bLooked = false;
	for (int I = 0; I < 80 && !bLooked; I++)
	{
		Now.Clock = I * 0.25f;
		Hero.Pos = P.Pos + Fwd * 3.f + V3(-Fwd.Y, Fwd.X, 0.f) * (-4.f + I * 0.1f);
		Hero.Vel = V3(-Fwd.Y, Fwd.X, 0.f) * 1.3f;
		M.Notice(W, Who, Now, Hero);
		bLooked = M.StateOf(Who)->LookAt == -1;
	}
	CHECK(bLooked, "somebody looks at the player walking past");
	CHECK(M.TakeEvent(Who) == ENoticeEvent::None, "... and makes nothing of it");
	// behind their back at 8 m, standing still: unseen
	M.Release(Who, 100.f);
	bool bSeenBehind = false;
	for (int I = 0; I < 80; I++)
	{
		Now.Clock = 130.f + I * 0.25f;
		Hero.Pos = P.Pos - Fwd * 8.f;
		Hero.Vel = V3();
		M.Notice(W, Who, Now, Hero);
		bSeenBehind |= M.StateOf(Who)->LookAt == -1;
	}
	CHECK(!bSeenBehind, "nobody sees the player standing behind their back");
	// running at them
	ENoticeEvent Got = ENoticeEvent::None;
	for (int I = 0; I < 40 && Got == ENoticeEvent::None; I++)
	{
		Now.Clock = 200.f + I * 0.25f;
		Hero.Pos = P.Pos + Fwd * (10.f - I * 0.25f * 5.f);
		Hero.Vel = Fwd * -5.f;
		M.Notice(W, Who, Now, Hero);
		Got = M.TakeEvent(Who);
	}
	CHECK(Got == ENoticeEvent::HeroRuns, "running at somebody is noticed (%s)", NoticeEventName(Got));
	// standing in their face
	Got = ENoticeEvent::None;
	for (int I = 0; I < 120 && Got == ENoticeEvent::None; I++)
	{
		Now.Clock = 300.f + I * 0.25f;
		Hero.Pos = P.Pos + Fwd * 0.8f;
		Hero.Vel = V3();
		M.Notice(W, Who, Now, Hero);
		Got = M.TakeEvent(Who);
	}
	CHECK(Got == ENoticeEvent::HeroTooClose, "standing in somebody's face is noticed (%s)", NoticeEventName(Got));
	// a lamp in the eyes at night
	Got = ENoticeEvent::None;
	Now.SunAlt = -20.f;
	Hero.bLamp = true;
	for (int I = 0; I < 40 && Got == ENoticeEvent::None; I++)
	{
		Now.Clock = 500.f + I * 0.25f;
		Hero.Pos = P.Pos + Fwd * 3.f;
		Hero.Vel = V3();
		M.Notice(W, Who, Now, Hero);
		Got = M.TakeEvent(Who);
	}
	CHECK(Got == ENoticeEvent::HeroLamp, "a head lamp in the eyes at night is noticed (%s)", NoticeEventName(Got));
	// creeping up behind their back, crouched
	Got = ENoticeEvent::None;
	Now.SunAlt = 25.f;
	Hero.bLamp = false;
	Hero.bCrouch = true;
	for (int I = 0; I < 40 && Got == ENoticeEvent::None; I++)
	{
		Now.Clock = 800.f + I * 0.25f;
		Hero.Pos = P.Pos - Fwd * (8.f - I * 0.25f * 1.f);
		Hero.Vel = Fwd * 1.f;
		M.Notice(W, Who, Now, Hero);
		Got = M.TakeEvent(Who);
	}
	CHECK(Got == ENoticeEvent::HeroSneaks, "creeping up behind somebody is felt (%s)", NoticeEventName(Got));
	// tearing past on a bicycle
	Got = ENoticeEvent::None;
	Hero.bCrouch = false;
	Hero.bBike = true;
	for (int I = 0; I < 40 && Got == ENoticeEvent::None; I++)
	{
		Now.Clock = 1000.f + I * 0.25f;
		Hero.Pos = P.Pos + Fwd * (14.f - I * 0.25f * 6.f) + V3(-Fwd.Y, Fwd.X, 0.f) * 0.8f;
		Hero.Vel = Fwd * -6.f;
		M.Notice(W, Who, Now, Hero);
		Got = M.TakeEvent(Who);
	}
	CHECK(Got == ENoticeEvent::HeroRides, "a bicycle tearing past is noticed (%s)", NoticeEventName(Got));
	// standing still with the line out a few steps in front of them
	Got = ENoticeEvent::None;
	Hero.bBike = false;
	Hero.bFishing = true;
	Hero.Vel = V3();
	for (int I = 0; I < 400 && Got == ENoticeEvent::None; I++)
	{
		Now.Clock = 2000.f + I * 0.25f;
		Hero.Pos = P.Pos + Fwd * 4.f;
		M.Notice(W, Who, Now, Hero);
		Got = M.TakeEvent(Who);
	}
	CHECK(Got == ENoticeEvent::HeroFishes, "the player fishing near somebody is noticed (%s)", NoticeEventName(Got));
	// in the water 20 m off and 4 m below
	Got = ENoticeEvent::None;
	Hero.bFishing = false;
	Hero.bSwim = true;
	for (int I = 0; I < 400 && Got == ENoticeEvent::None; I++)
	{
		Now.Clock = 3000.f + I * 0.25f;
		Hero.Pos = P.Pos + Fwd * 20.f + V3(0.f, 0.f, -4.f);
		M.Notice(W, Who, Now, Hero);
		Got = M.TakeEvent(Who);
	}
	CHECK(Got == ENoticeEvent::HeroSwims, "the player in the water is seen from the shore (%s)", NoticeEventName(Got));
	// nothing of the kind while he just stands there
	Got = ENoticeEvent::None;
	Hero.bSwim = false;
	for (int I = 0; I < 200 && Got == ENoticeEvent::None; I++)
	{
		Now.Clock = 4000.f + I * 0.25f;
		Hero.Pos = P.Pos + Fwd * 4.f;
		M.Notice(W, Who, Now, Hero);
		Got = M.TakeEvent(Who);
	}
	CHECK(Got == ENoticeEvent::None, "standing a few steps off is nothing to remark on (%s)", NoticeEventName(Got));
	// sitting down by somebody on a bench
	int Sitter = -1;
	for (const FPerson& Q : W.People)
	{
		if (Q.bTown && Drawn(W, Q) && !Q.bIndoors && Q.State == EState::Doing && Q.Pose == EPose::Sit)
		{
			Sitter = Q.Id;
			break;
		}
	}
	CHECK(Sitter >= 0, "somebody sits out of doors at 13:10");
	if (Sitter >= 0)
	{
		const FPerson& Q = W.People[Sitter];
		Got = ENoticeEvent::None;
		Hero.bSeated = true;
		for (int I = 0; I < 400 && Got == ENoticeEvent::None; I++)
		{
			Now.Clock = 5000.f + I * 0.25f;
			Hero.Pos = Q.Pos + V3(0.9f, 0.f, 0.f);
			M.Notice(W, Sitter, Now, Hero);
			Got = M.TakeEvent(Sitter);
		}
		CHECK(Got == ENoticeEvent::HeroSits, "sitting down beside somebody is noticed (%s)", NoticeEventName(Got));
	}
	Hero.bBike = false;
	// a wave on demand, and none over a newspaper
	CHECK(M.React(Who, EManner::Wave, 1300.f, 2.f) && M.StateOf(Who)->Cur == EManner::Wave, "a wave can be asked for");
	Now.Clock = 1303.f;
	M.Update(W, Who, Now);
	CHECK(M.StateOf(Who)->Cur != EManner::Wave, "... and runs out by itself");
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "test";
	if (Mode == "who")
	{
		FWorld W;
		FManners M;
		if (!LoadAll(W, M))
		{
			return 1;
		}
		if (Argc > 2)
		{
			std::printf("%s\n", M.Describe(W, W.FindPerson(Argv[2])).c_str());
			return 0;
		}
		for (const FPerson& P : W.People)
		{
			if (P.bTown && P.Id % 37 == 0)
			{
				std::printf("%s (%d, %s)\n", M.Describe(W, P.Id).c_str(), P.Age, P.Occupation.c_str());
			}
		}
		return 0;
	}
	if (Mode == "dates")
	{
		// a quick look: a mild Wednesday without the report
		FDay D;
		D.Name = "mild";
		D.bDates = true;
		RunDay(D);
		for (const auto& KV : D.DateHalf)
		{
			std::printf("  half an hour in: %s: %d\n", KV.first.c_str(), KV.second);
		}
		std::printf("friends' weekly dates: %d pairs in the town, %d on Wednesdays, %d of them met, %d side by side\n", D.AllDates, D.Dates, D.DatesKept, D.DatesClose);
		return 0;
	}
	if (Mode == "outing")
	{
		Outings(true);
		return Failures ? 1 : 0;
	}
	if (Mode == "day")
	{
		FDay D;
		D.Name = "mild";
		D.bPrint = true;
		D.TempC = Argc > 2 ? (float)std::atof(Argv[2]) : 12.f;
		if (Argc > 3)
		{
			D.RainFrom = (float)std::atof(Argv[3]);
			D.RainTo = D.RainFrom + 1.f;
		}
		RunDay(D);
		std::printf("person-hours by manner:");
		for (const auto& KV : D.Seconds)
		{
			std::printf(" %s %.1f", KV.first.c_str(), KV.second / 3600.0);
		}
		std::printf("\n");
		return 0;
	}
	std::printf("who they are:\n");
	Personas();
	std::printf("what they notice:\n");
	Noticing();
	std::printf("a weekend:\n");
	Outings(false);
	FDay Mild, Shower, Frost, Warm;
	Mild.Name = "a mild day";
	Shower.Name = "a shower at 14:00";
	Shower.RainFrom = 14.f;
	Shower.RainTo = 15.f;
	Frost.Name = "frost";
	Frost.TempC = -8.f;
	Warm.Name = "a warm day";
	Warm.TempC = 23.f;
	for (FDay* D : { &Mild, &Shower, &Frost, &Warm })
	{
		RunDay(*D);
		std::printf("%s (%.0f C): %.0f%% of the time people seen do something with their hands;", D->Name, D->TempC, 100.0 * D->MannerSeconds / std::max(1.0, D->DrawnSeconds));
		int N = 0;
		for (const auto& KV : D->Seconds)
		{
			if (KV.first != "none" && KV.second > 600.0)
			{
				std::printf(" %s %.0f", KV.first.c_str(), KV.second / 3600.0);
				N++;
			}
		}
		std::printf(" (person-hours; %d manners)\n", N);
		CHECK(D->Wrong == 0, "nothing that must never be seen (%d%s%s)", D->Wrong, D->Wrong ? ": " : "", D->WrongWhat.c_str());
	}
	const double Share = Mild.MannerSeconds / std::max(1.0, Mild.DrawnSeconds);
	CHECK(Share > 0.15 && Share < 0.6, "people aren't statues, nor fidgets (%.0f%% of the time a manner)", Share * 100.0);
	CHECK(Mild.MinDistinctDay >= 10, "at any moment of the day at least ten different manners are about (%d)", Mild.MinDistinctDay);
	std::string Never;
	for (int I = 1; I < NumManners; I++)
	{
		const char* Name = MannerName((EManner)I);
		if (Mild.Seconds[Name] + Shower.Seconds[Name] + Frost.Seconds[Name] + Warm.Seconds[Name] <= 0.0)
		{
			Never += std::string(" ") + Name;
		}
	}
	// claps and sways need a holiday (tools/festivalsim sets the gatherings); everything else is seen in these four days
	CHECK(Never.empty() || Never == " clap sway" || Never == " clap" || Never == " sway", "every manner is seen in some weather%s%s", Never.empty() ? "" : ": never", Never.c_str());
	CHECK(Mild.SkyOut > 0 && Mild.SkyUp * 4 > Mild.SkyOut && Mild.SkyUp * (long)Mild.BeforeOut > 4L * Mild.BeforeUp * Mild.SkyOut,
		"something in the sky turns the heads up (%d of %d person-samples out of doors during it, %d of %d before)",
		Mild.SkyUp, Mild.SkyOut, Mild.BeforeUp, Mild.BeforeOut);
	CHECK(Mild.PointNoShow == 0, "nobody points at an empty sky (%d)", Mild.PointNoShow);
	CHECK(Mild.AllDates > 250 && Mild.Dates > 30 && Mild.DatesKept * 5 > Mild.Dates * 2,
		"friends keep their weekly date (%d of the %d pairs whose day it is met; %d pairs in the town)", Mild.DatesKept, Mild.Dates, Mild.AllDates);
	CHECK(Warm.FrontEvening > Mild.FrontEvening && Warm.FrontEvening * 2 > Frost.FrontEvening * 3,
		"a warm evening brings the town out to the sea front (%d person-samples warm, %d mild, %d in the frost)", Warm.FrontEvening, Mild.FrontEvening,
		Frost.FrontEvening);
	CHECK(Mild.Ritualists > 300 && Mild.RitualKept > Mild.Ritualists / 4, "people keep their rituals (%d of the %d who have one were at it at their hour; the rest at work)",
		Mild.RitualKept, Mild.Ritualists);
	CHECK(Mild.UmbrellasDry == 0 && Frost.UmbrellasDry == 0, "no umbrellas on a dry day");
	CHECK(Shower.UmbrellasInRain > 20 && Shower.CoverInRain > 3, "in the shower some open umbrellas, the others run for it (%d umbrella-minutes, %d running with an arm over the head)",
		Shower.UmbrellasInRain, Shower.CoverInRain);
	CHECK(Shower.OutBeforeRain > 30 && Shower.OutInRain < Shower.OutBeforeRain * 0.45, "the shower sends people under a roof (%d at leisure out of doors before, %d a quarter of an hour in)",
		Shower.OutBeforeRain, Shower.OutInRain);
	CHECK(Frost.Seconds["rub_hands"] + Frost.Seconds["hug_self"] > 3600.0 && Mild.Seconds["rub_hands"] <= 0.0, "in the frost people rub their hands (%.0f person-hours), on a mild day nobody does",
		(Frost.Seconds["rub_hands"] + Frost.Seconds["hug_self"]) / 3600.0);
	CHECK(Frost.ActMinutes["sit_bench"] < Mild.ActMinutes["sit_bench"] * 0.75, "the frost empties the benches (%.0f person-minutes against %.0f)", Frost.ActMinutes["sit_bench"], Mild.ActMinutes["sit_bench"]);
	CHECK(Warm.Seconds["ice_cream"] > 1800.0 && Frost.Seconds["ice_cream"] <= 0.0, "ice cream on the warm day only (%.1f person-hours)", Warm.Seconds["ice_cream"] / 3600.0);
	CHECK(Mild.ActMinutes["jog"] > 200.0 && Mild.ActMinutes["jog"] < Mild.ActMinutes["stroll"] * 0.35, "some jog, most stroll (%.0f against %.0f person-minutes)", Mild.ActMinutes["jog"], Mild.ActMinutes["stroll"]);
	CHECK(Mild.ActMinutes["play_run"] > 300.0, "children run about (%.0f person-minutes)", Mild.ActMinutes["play_run"]);
	CHECK(Mild.ActMinutes["feed_birds"] > 300.0 && Mild.ActMinutes["read_bench"] > 300.0, "somebody feeds the birds, somebody reads on a bench (%.0f, %.0f person-minutes)",
		Mild.ActMinutes["feed_birds"], Mild.ActMinutes["read_bench"]);
	CHECK(Mild.Waves > 5, "people who know each other wave in passing (%d waves)", Mild.Waves);
	CHECK(Mild.Glances > 100, "people glance at each other in passing (%d)", Mild.Glances);
	const double Ns = Mild.UpdateNs / std::max(1L, Mild.Updates);
	CHECK(Ns < 400.0, "an update of one person costs %.0f ns (48 drawn: %.4f ms a frame)", Ns, Ns * 48.0 / 1e6);
	std::printf(Failures ? "%d FAILED\n" : "all ok\n", Failures);
	return Failures ? 1 : 0;
}
