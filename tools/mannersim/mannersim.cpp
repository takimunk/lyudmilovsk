// The townsfolk's characters and manners (Private/People/PeopleManners.cpp, docs/town_manners.md) run on the town's
// people outside the engine.
//   tools/mannersim/run.sh            checks
//   tools/mannersim/run.sh day        the mild day hour by hour
//   tools/mannersim/run.sh who [key]  personas
//   tools/mannersim/run.sh friends    the friends among the residents only (docs/friends.md)
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/PeopleManners.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/Friends.h"

#include <algorithm>
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
	int Weekday = 2;                            // Wednesday (0 Monday)
	bool bSeaFestival = false;                  // the sea festival's program in the town (festivals.json sea_festival)
	bool bPrint = false;
	// results
	std::map<std::string, double> Seconds;      // manner -> person-seconds (people out of doors or seen indoors)
	std::map<std::string, double> ActMinutes;   // activity -> person-minutes
	double DrawnSeconds = 0.0, MannerSeconds = 0.0;
	int Wrong = 0;
	std::string WrongWhat;
	int MinDistinctDay = 99;
	int OutBeforeRain = -1, OutInRain = -1, UmbrellasInRain = 0, UmbrellasDry = 0, CoverInRain = 0;
	int Waves = 0, Glances = 0, Goodbyes = 0;
	double UpdateNs = 0.0;
	long Updates = 0;
	int BenchNoon = 0;
	int Ritualists = 0, RitualKept = 0;
	int FrontEvening = 0;       // people on the sea front and the squares, summed over the evening's samples (18:30-21:00)
	bool bDates = false;
	std::map<std::string, int> DateHalf;
	int DatesClose = 0;         // of the kept: side by side (within 3 m) at some minute
	int AllDates = 0, Dates = 0, DatesKept = 0, DatesIll = 0;      // pairs of friends with a weekly date; those whose day is this Wednesday; kept
	// ten minutes of something in the sky at 21:00: people out of doors, and those of them looking up, pointing or photographing,
	// during it and in the ten minutes before; and an arm pointed at the sky at any other time
	int SkyOut = 0, SkyUp = 0, BeforeOut = 0, BeforeUp = 0, PointNoShow = 0;
	// the days' things (docs/town_manners.md): person-seconds with each prop in the hands; bBirthdays gives a dozen grown-ups
	// their birthday and a guest each at six; chess on a bench sampled each minute (the plan's two players within 1.7 m:
	// the board goes on the seat between them)
	bool bBirthdays = false;
	std::map<std::string, double> PropSeconds;
	int Birthdays = 0, Guests = 0, ChessSamples = 0, ChessPaired = 0, ChessPlanned = 0;
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
	C.Seconds = (D.Weekday * 24.0 + 5.0) * 3600.0;       // the day's 05:00 (a Wednesday unless the day says)
	C.Set(W);
	W.TempC = D.TempC;
	W.Resettle();
	if (D.bBirthdays)
	{
		std::vector<int> Grown;
		for (const FPerson& P : W.People)
		{
			if (P.bTown && P.Age >= 25 && P.Age < 70 && P.Home >= 0)
			{
				Grown.push_back(P.Id);
			}
		}
		for (size_t K = 0; K + 1 < Grown.size() && D.Birthdays < 12; K += Grown.size() / 13)
		{
			const int Host = Grown[K], Guest = Grown[K + 1];
			W.SetToday(Host, TodayBirthday);
			D.Birthdays++;
			if (W.People[(size_t)Guest].Home != W.People[(size_t)Host].Home)
			{
				W.SetToday(Guest, TodayGuest);
				D.Guests += W.MakePlan(Guest, Host, EPlan::Birthday, 18.f) ? 1 : 0;
			}
		}
	}
	const int ChessAct = W.FindActivity("chess_bench");
	std::vector<int> Shown;
	FMannerNow Now;
	Now.TempC = D.TempC;
	const float Dt = 1.f;
	std::map<std::string, int> HourCount;
	int LastHour = -1;
	std::vector<char> RitualKept;
	std::vector<char> DateKept;
	std::vector<char> DateIll;      // (a date with one of the two down with a cold today is off: docs/town_life.md "The days")
	for (double T = 0.0; T < 19.0 * 3600.0; T += Dt)        // till midnight
	{
		C.Seconds += Dt;
		C.Set(W);
		const bool bRain = D.RainFrom >= 0.f && W.Hour >= D.RainFrom && W.Hour < D.RainTo;
		W.bRain = bRain;
		W.Rain = bRain ? 0.7f : 0.f;
		if (D.bSeaFestival && (long)std::lround(T) % 60 == 0)
		{
			// the sea festival's program as ULyudFestivalSubsystem sends it to the people's life (festivals.json sea_festival:
			// the fair 10-17, the concert on the square 17-21:30, the dance on the square from 22:06; the fireworks are the
			// sky show at 21:00 below)
			const auto Phase = [&W](const char* Id, const char* Venue, float From, float To, float Pull, EGatherMode Mode)
			{
				const bool bOn = W.Hour >= From - 0.1f && W.Hour < To;
				const bool bSet = std::any_of(W.Gatherings.begin(), W.Gatherings.end(), [Id](const FWorld::FGathering& G) { return G.Id == Id; });
				if (bOn && !bSet)
				{
					W.SetGathering(Id, Venue, From, To, Pull, Mode, false, V3());
				}
				else if (!bOn && bSet)
				{
					W.ClearGathering(Id);
				}
			};
			Phase("sea_festival.fair", "fair", 10.f, 17.f, 1.0f, EGatherMode::Fair);
			Phase("sea_festival.concert", "square", 17.f, 21.5f, 1.2f, EGatherMode::Watch);
			Phase("sea_festival.dance", "square", 22.1f, 24.f, 0.7f, EGatherMode::Mingle);
		}
		W.Step(Dt);
		W.Events.clear();
		if ((long)std::lround(T) % 60 == 0)
		{
			// friends whose weekly date is today: the two of them at it together
			DateKept.resize(W.People.size(), 0);
			DateIll.resize(W.People.size(), 0);
			for (const FPerson& P : W.People)
			{
				if (P.DateWith > P.Id && P.DateDay == W.Weekday && ((P.Today | W.People[(size_t)P.DateWith].Today) & TodayCold))
				{
					DateIll[(size_t)P.Id] = 1;
				}
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
			// chess on a bench: the plan's partner on the next seat
			for (const FPerson& P : W.People)
			{
				if (ChessAct >= 0 && P.Activity == ChessAct && P.State == EState::Doing)
				{
					D.ChessSamples++;
					const FPerson* O = P.PlanWith >= 0 ? &W.People[(size_t)P.PlanWith] : nullptr;
					if (O && P.PlanKind == EPlan::Chess)
					{
						D.ChessPlanned++;
						D.ChessPaired += O->Activity == ChessAct && O->State == EState::Doing && O->Place == P.Place && (O->Pos - P.Pos).Len2D() < 1.7f ? 1 : 0;
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
			D.Goodbyes = M.Goodbyes;
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
				if (Def && Def->Prop != EProp::None)
				{
					D.PropSeconds[PropName(Def->Prop)] += Dt;
				}
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
		const bool bIll = (size_t)P.Id < DateIll.size() && DateIll[(size_t)P.Id];
		D.Dates += P.DateWith > P.Id && P.DateDay == D.Weekday && !bIll ? 1 : 0;
		D.DatesIll += P.DateWith > P.Id && P.DateDay == D.Weekday && bIll ? 1 : 0;
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
	long WayMinutes = 0, WayNear = 0, HomeMinutes = 0, HomeNear = 0, KidHome = 0, KidHomeWith = 0, KidOut = 0, KidOutWith = 0, Breaths = 0, OldWalk = 0;
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
		// on the way to the outing: on foot out of doors, and how many of them with one of theirs beside them
		for (const FPerson& P : W.People)
		{
			if (P.OutingAct < 0 || W.Weekday != P.OutingDay || P.State != EState::Going || P.Activity != P.OutingAct || P.bIndoors || P.Bike >= 0
				|| P.Home < 0)
			{
				continue;
			}
			bool bBeside = false;
			for (const int R : W.Places[(size_t)P.Home].Residents)
			{
				const FPerson& Q = W.People[(size_t)R];
				bBeside |= R != P.Id && Q.Household == P.Household && Q.State == EState::Going && Q.Activity == P.Activity
					&& (Q.Pos - P.Pos).Len2D() < 10.f;
			}
			WayMinutes++;
			WayNear += bBeside ? 1 : 0;
		}
		// the old on their way on foot, and those of them standing to get their breath back
		for (const FPerson& P : W.People)
		{
			if (P.Age >= 68 && P.State == EState::Going && !P.bIndoors && P.Bike < 0)
			{
				OldWalk++;
				Breaths += P.BreathAt >= 0 && P.Speed <= 0.f ? 1 : 0;
			}
		}
		// a child on the way out to play, and those with a grown-up of theirs walking beside
		for (const FPerson& P : W.People)
		{
			if (P.Age >= 12 || P.State != EState::Going || P.Home < 0 || P.bIndoors || (P.Activity != W.PlayActs[0] && P.Activity != W.PlayActs[1]))
			{
				continue;
			}
			bool bBeside = false;
			for (const int R : W.Places[(size_t)P.Home].Residents)
			{
				const FPerson& Q = W.People[(size_t)R];
				bBeside |= Q.Age >= 18 && Q.Household == P.Household && Q.State == EState::Going && Q.Activity == W.MindAct && (Q.Pos - P.Pos).Len2D() < 12.f;
			}
			KidOut++;
			KidOutWith += bBeside ? 1 : 0;
		}
		// a child on the way home from play, and those with a grown-up of theirs walking beside
		for (const FPerson& P : W.People)
		{
			if (P.Age >= 12 || P.State != EState::Going || P.Home < 0 || P.Place != P.Home || P.bIndoors
				|| (P.PrevActivity != W.PlayActs[0] && P.PrevActivity != W.PlayActs[1]))
			{
				continue;
			}
			bool bBeside = false;
			for (const int R : W.Places[(size_t)P.Home].Residents)
			{
				const FPerson& Q = W.People[(size_t)R];
				bBeside |= Q.Age >= 18 && Q.Household == P.Household && Q.State == EState::Going && Q.Place == P.Place && (Q.Pos - P.Pos).Len2D() < 12.f;
			}
			KidHome++;
			KidHomeWith += bBeside ? 1 : 0;
		}
		// home from the outing ("home with theirs", and whoever got up first): on foot out of doors, with one of theirs beside
		for (const FPerson& P : W.People)
		{
			if (P.OutingAct < 0 || W.Weekday != P.OutingDay || P.State != EState::Going || P.Place != P.Home || P.bIndoors || P.Home < 0
				|| W.Hour < P.OutingHour || W.Hour > P.OutingHour + 2.5f)
			{
				continue;
			}
			bool bBeside = false;
			for (const int R : W.Places[(size_t)P.Home].Residents)
			{
				const FPerson& Q = W.People[(size_t)R];
				bBeside |= R != P.Id && Q.Household == P.Household && Q.State == EState::Going && Q.Place == P.Place && (Q.Pos - P.Pos).Len2D() < 10.f;
			}
			HomeMinutes++;
			HomeNear += bBeside ? 1 : 0;
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
	CHECK(WayMinutes > 300, "a household sets out for its outing together (%ld of %ld person-minutes on the way with one of theirs within 10 m)",
		WayNear, WayMinutes);
	CHECK(HomeMinutes > 300 && HomeNear * 5 > HomeMinutes, "a household goes home from its outing together (%ld of %ld person-minutes on the way home with one of theirs within 10 m)",
		HomeNear, HomeMinutes);
	CHECK(Breaths > 10 && Breaths * 8 < OldWalk, "the old stop for breath on a long way, and not for long (%ld of %ld person-minutes of their walking)", Breaths, OldWalk);
	CHECK(KidOut > 100 && KidOutWith * 10 > KidOut, "a grown-up goes out with the child who goes out to play (%ld of %ld child-minutes on the way there)", KidOutWith, KidOut);
	CHECK(KidHome > 100 && KidHomeWith * 10 > KidHome * 3, "the grown-up who stood by walks the child home from play (%ld of %ld child-minutes on the way home)", KidHomeWith, KidHome);
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


// ------------------------------------------------------------------ friends among the residents (docs/friends.md)

static LyudFriends::FWho FriendWho(const FWorld& W, const FManners& M, const FPerson& P)
{
	LyudFriends::FWho Who;
	Who.Key = P.Key;
	Who.Age = P.Age;
	Who.Job = P.Occupation;
	if (const FPersona* Pe = M.PersonaOf(P.Id))
	{
		for (int B = 0; B < 16; B++)
		{
			if (Pe->Has(1u << B) && HabitName(1u << B))
			{
				Who.Habits.push_back(HabitName(1u << B));
			}
		}
		Who.Warmth = Pe->Warmth;
		Who.Curiosity = Pe->Curiosity;
		Who.Tidy = Pe->Tidy;
	}
	(void)W;
	return Who;
}

static void Friends()
{
	std::printf("friends among the residents:\n");
	FWorld W;
	FManners M;
	if (!LoadAll(W, M))
	{
		Failures++;
		return;
	}
	LyudFriends::FModel F;
	std::string Err;
	const bool bLoaded = F.Data.Load(ReadFile(std::string(DataDir) + "/friends.json"), Err);
	CHECK(bLoaded && F.Data.Hosts.size() == 10, "friends.json reads clean: %zu hosts, %zu gifts, %zu nicknames %s", F.Data.Hosts.size(), F.Data.Gifts.size(),
		F.Data.Nicknames.size(), Err.c_str());
	if (!bLoaded)
	{
		return;
	}
	int Chairs = 0, HostPeople = 0, HostPeopleOk = 0;
	for (const LyudFriends::FHost& H : F.Data.Hosts)
	{
		Chairs += (H.ChairAt[0] != 0.f || H.ChairAt[1] != 0.f) ? 1 : 0;
		if (!H.Person.empty())
		{
			HostPeople++;
			HostPeopleOk += W.FindPerson(H.Person) >= 0 ? 1 : 0;
		}
	}
	CHECK(Chairs == 10, "every host has a chair for the hero at their table (%d of 10)", Chairs);
	CHECK(HostPeople >= 2 && HostPeopleOk == HostPeople, "the hosts the town's life has are in it (%d of %d)", HostPeopleOk, HostPeople);

	// something in the shops for everybody: each townsperson loves at least one of the gifts, kids the toys, nobody old a toy
	int Town = 0, WithLove = 0, KidToys = 0, Kids = 0, OldToys = 0;
	std::set<std::string> Nicks;
	for (const FPerson& P : W.People)
	{
		if (!P.bTown || P.Occupation == "away")
		{
			continue;
		}
		Town++;
		const LyudFriends::FWho Who = FriendWho(W, M, P);
		bool bAny = false;
		for (const LyudFriends::FGift& G : F.Data.Gifts)
		{
			bAny = bAny || F.Likes(G.Item, Who) == LyudFriends::EGift::Loved;
		}
		WithLove += bAny ? 1 : 0;
		if (P.Age < 12)
		{
			Kids++;
			KidToys += F.Likes("toy_duck", Who) == LyudFriends::EGift::Loved || F.Likes("spinning_top", Who) == LyudFriends::EGift::Loved ? 1 : 0;
		}
		else if (P.Age >= 60)
		{
			OldToys += F.Likes("toy_duck", Who) == LyudFriends::EGift::Loved ? 1 : 0;
		}
		Nicks.insert(F.PickNick(Who));
	}
	CHECK(WithLove * 100 >= Town * 97, "nearly everybody loves one of the gifts (%d of %d)", WithLove, Town);
	CHECK(Kids > 0 && KidToys == Kids && OldToys == 0, "children love a toy (%d of %d), the old don't (%d)", KidToys, Kids, OldToys);
	CHECK(Nicks.size() >= 8 && !Nicks.count(""), "the town calls the hero by many nicknames (%zu different)", Nicks.size());

	// how it grows: a hello a day alone doesn't make a friend in a month; hellos, a few gifts and cards do in two weeks
	const FPerson* Gran = nullptr;
	for (const FPerson& P : W.People)
	{
		if (P.bTown && P.Age >= 65 && P.Occupation == "pensioner" && W.Places[(size_t)std::max(0, P.Home)].Kind == "flat")
		{
			Gran = &P;
			break;
		}
	}
	CHECK(Gran != nullptr, "a pensioner to befriend: %s", Gran ? Gran->Key.c_str() : "none");
	if (!Gran)
	{
		return;
	}
	const LyudFriends::FWho GranWho = FriendWho(W, M, *Gran);
	std::string Loved;
	for (const LyudFriends::FGift& G : F.Data.Gifts)
	{
		if (Loved.empty() && F.Likes(G.Item, GranWho) == LyudFriends::EGift::Loved)
		{
			Loved = G.Item;
		}
	}
	const int Day0 = 20750;
	for (int D = 0; D < 30; D++)
	{
		F.Greeted("t_hello_only", Day0 + D);
		F.Greeted("t_hello_only", Day0 + D);     // twice a day counts once
	}
	CHECK(F.Level("t_hello_only") == 2, "a hello a day for a month makes them friendly, not a friend (level %d, %d points)", F.Level("t_hello_only"),
		F.Find("t_hello_only")->Points);
	int CloseOn = -1, FriendOn = -1;
	for (int D = 0; D < 30 && CloseOn < 0; D++)
	{
		F.Greeted(Gran->Key, Day0 + D);
		F.Heard(Gran->Key, Day0 + D);
		F.Heard(Gran->Key, Day0 + D);
		if (D % 3 == 1)
		{
			F.Give(Gran->Key, Loved, GranWho, Day0 + D);
		}
		if (D % 6 == 4)
		{
			F.Card(Gran->Key, Day0 + D);
		}
		if (F.Level(Gran->Key) >= 3 && FriendOn < 0)
		{
			FriendOn = D;
			F.EnsureNick(Gran->Key, GranWho);
		}
		CloseOn = F.Level(Gran->Key) >= 4 ? D : -1;
	}
	CHECK(FriendOn >= 6 && FriendOn <= 12 && CloseOn > FriendOn && CloseOn <= 20, "hellos, chats, a gift they love every few days and a card a week: a friend in %d days, close in %d",
		FriendOn, CloseOn);
	const LyudFriends::FBond* GB = F.Find(Gran->Key);
	CHECK(GB && !GB->Nick.empty() && GB->Nick == F.PickNick(GranWho) && GB->Loves.count(Loved), "the friend gave him a nickname (%s) and he knows what they love (%s)",
		GB ? GB->Nick.c_str() : "", Loved.c_str());
	CHECK(F.Give(Gran->Key, "toy_duck", GranWho, Day0 + 40) != LyudFriends::EGift::Again && F.Give(Gran->Key, Loved, GranWho, Day0 + 40) == LyudFriends::EGift::Again,
		"one gift a day counts; the second is \"not every day!\"");
	CHECK(std::string(F.BondWord(Gran->Key)) == "close" && std::string(F.BondWord("t_nobody")).empty(), "town talk's bond = close for them, nothing for a stranger");

	// tea with a host: visits, the home quest and a card make them friendly; the invitation comes in the morning for the day after next
	const LyudFriends::FHost& Holm = *F.Data.HostById("holm");
	const std::string HK = Holm.Key();
	const std::map<std::string, LyudFriends::FOutingInfo> NoOutings;
	int D = Day0;
	F.Visited(HK, D);
	F.QuestDone(HK, Holm.Quest, D);
	F.QuestDone(HK, Holm.Quest, D);     // once
	CHECK(F.Step(D, 9.f, 0, NoOutings).empty(), "no invitation before they are friendly (level %d)", F.Level(HK));
	F.Card(HK, D + 1);
	F.Visited(HK, D + 1);
	std::vector<LyudFriends::FInvite> Inv = F.Step(D + 2, 7.f, 2, NoOutings);
	CHECK(Inv.empty(), "nobody writes before 8 in the morning");
	Inv = F.Step(D + 2, 9.5f, 2, NoOutings);
	CHECK(Inv.size() == 1 && Inv[0].Kind == "tea" && Inv[0].Day == D + 4 && Inv[0].Hour == Holm.Hour && Inv[0].Body == "friends.tea.holm.invite",
		"a friendly host invites him to tea (%zu invitation: %s on day +%d at %d)", Inv.size(), Inv.empty() ? "" : Inv[0].Body.c_str(),
		Inv.empty() ? 0 : Inv[0].Day - D, Inv.empty() ? 0 : Inv[0].Hour);
	CHECK(F.Step(D + 3, 9.5f, 3, NoOutings).empty(), "one invitation at a time");
	CHECK(!F.TeaNow(HK, D + 3, (float)Holm.Hour) && !F.TeaNow(HK, D + 4, (float)Holm.Hour - 1.f) && F.TeaNow(HK, D + 4, (float)Holm.Hour + 0.2f)
		&& !F.TeaNow(HK, D + 4, (float)Holm.Hour + 3.f), "the tea is on that day from half an hour before the hour to two and a half after");
	const int Before = F.Find(HK)->Points;
	F.Tea(HK, D + 4);
	CHECK(F.Find(HK)->Points == Before + F.Data.R.Tea && F.Find(HK)->Teas == 1 && !F.TeaNow(HK, D + 4, (float)Holm.Hour + 0.5f), "a tea counts and uses the invitation up");
	CHECK(F.Step(D + 8, 9.5f, 0, NoOutings).empty() && F.Step(D + 11, 9.5f, 3, NoOutings).size() == 1, "the next invitation a week after the tea");
	// missed: forgotten after its day, and the next one a week later
	const int Missed = F.Find(HK)->TeaDay;
	F.Step(Missed + 1, 9.f, 0, NoOutings);
	CHECK(F.Find(HK)->TeaDay < 0 && F.Step(Missed + 3, 9.5f, 0, NoOutings).empty() && F.Step(Missed + 8, 9.5f, 0, NoOutings).size() == 1,
		"a missed tea is forgotten; another invitation a week later");

	// a close friend's family outing (the people's life gives them one: FPerson::Outing*)
	const FPerson* Fam = nullptr;
	for (const FPerson& P : W.People)
	{
		if (P.bTown && P.OutingAct >= 0 && P.Age >= 18 && P.OutingPlace >= 0)
		{
			const std::string& A = W.Activities[(size_t)P.OutingAct].Id;
			if (std::find(F.Data.OutingActs.begin(), F.Data.OutingActs.end(), A) != F.Data.OutingActs.end())
			{
				Fam = &P;
				break;
			}
		}
	}
	CHECK(Fam != nullptr, "a family with a weekend outing: %s", Fam ? Fam->Key.c_str() : "none");
	if (Fam)
	{
		std::map<std::string, LyudFriends::FOutingInfo> Outings;
		Outings[Fam->Key] = { W.Activities[(size_t)Fam->OutingAct].Id, Fam->OutingDay, Fam->OutingHour };
		const int Wed = Day0 + 30 - ((Day0 + 30 + 3) % 7 + 7) % 7 + 2;      // a Wednesday (day 0 = 1970-01-01, a Thursday)
		F.Greeted(Fam->Key, Wed - 1);
		CHECK(F.Step(Wed, 9.f, 2, Outings).empty(), "a family doesn't invite a boy they hardly know");
		F.DevBefriend(Fam->Key, Wed);
		Inv = F.Step(Wed, 9.f, 2, Outings);
		const int OutDay = Wed + (Fam->OutingDay - 2);
		CHECK(Inv.size() == 1 && Inv[0].Kind == "outing" && Inv[0].Day == OutDay && Inv[0].Hour == (int)Fam->OutingHour,
			"a close friend's family invites him on Wednesday to their %s on %s at %d:%02d", Outings[Fam->Key].Act.c_str(),
			Fam->OutingDay == 5 ? "Saturday" : "Sunday", Inv.empty() ? 0 : Inv[0].Hour, Inv.empty() ? 0 : Inv[0].Minute);
		CHECK(F.OutingNow(Fam->Key, OutDay, Fam->OutingHour + 0.2f, Outings[Fam->Key]) && !F.OutingNow(Fam->Key, OutDay - 1, Fam->OutingHour, Outings[Fam->Key]),
			"the outing is on that day at their hour");
		CHECK(F.Outing(Fam->Key, OutDay) == F.Data.R.Outing && F.Outing(Fam->Key, OutDay) == 0, "joining it counts, once");
		{
			// a holiday together (docs/festivals.md "The new toys"): a friend met at the gathering counts once a day; the save keeps the day
			const int FestDay = OutDay + 3;
			const int First = F.Festival(Fam->Key, FestDay), Again = F.Festival(Fam->Key, FestDay);
			std::map<std::string, std::string> FestSaved;
			F.Save(FestSaved);
			LyudFriends::FModel F2;
			F2.Data = F.Data;
			F2.LoadState(FestSaved);
			CHECK(First == F.Data.R.Festival && Again == 0 && F2.Festival(Fam->Key, FestDay) == 0 && F2.Festival(Fam->Key, FestDay + 1) == F.Data.R.Festival
				&& F.Festival("nobody_at_all", FestDay) == 0, "a holiday spent with a friend counts %d, once a day, after a load too; a stranger's wave counts nothing", F.Data.R.Festival);
		}
		auto OutingsOnly = [](const std::vector<LyudFriends::FInvite>& V)
		{
			int N = 0;
			for (const LyudFriends::FInvite& I : V)
			{
				N += I.Kind == "outing" ? 1 : 0;
			}
			return N;
		};
		CHECK(OutingsOnly(F.Step(Wed + 7, 9.f, 2, Outings)) == 0 && OutingsOnly(F.Step(Wed + 21, 9.f, 2, Outings)) == 1, "the next outing invitation three weeks later");
	}

	// the save
	std::map<std::string, std::string> Saved, Again;
	F.Save(Saved);
	LyudFriends::FModel G;
	G.Data = F.Data;
	G.LoadState(Saved);
	G.Save(Again);
	CHECK(Saved == Again && G.Level(Gran->Key) == F.Level(Gran->Key) && G.Find(Gran->Key)->Nick == GB->Nick, "save and load give the same friends (%zu bonds)", G.Bonds.size());

	// a friend comes to sit with him: a seat of a bench next to the one he is on
	int Bench = -1, Next = -1;
	for (int S = 0; S < (int)W.Slots.size() && Next < 0; S++)
	{
		if (W.Places[(size_t)W.Slots[(size_t)S].Place].Kind != "bench" || W.Slots[(size_t)S].Height <= 0.f)
		{
			continue;
		}
		for (int T = 0; T < (int)W.Slots.size(); T++)
		{
			if (T != S && W.Slots[(size_t)T].Place == W.Slots[(size_t)S].Place && (W.Slots[(size_t)T].Pos - W.Slots[(size_t)S].Pos).Len() < 2.f)
			{
				Bench = S;
				Next = T;
				break;
			}
		}
	}
	CHECK(Next >= 0, "a bench with two seats side by side");
	if (Next >= 0)
	{
		int Ok = 0, Tried = 0;
		for (FPerson& P : W.People)
		{
			if (!P.bTown || P.Age < 18 || P.Occupation == "away" || Tried >= 5)
			{
				continue;
			}
			Tried++;
			if (W.OrderBeside(P.Id, W.Slots[(size_t)Bench].Pos, 2.5f, 15.0))
			{
				Ok += P.Reserved >= 0 && P.Reserved != Bench && (W.Slots[(size_t)P.Reserved].Pos - W.Slots[(size_t)Bench].Pos).Len() < 2.5f ? 1 : 0;
				break;
			}
		}
		CHECK(Ok == 1, "a friend sent to sit beside him takes the next seat, not his (%d)", Ok);
	}
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "test";
	if (Mode == "friends")
	{
		Friends();
		std::printf(Failures ? "%d FAILED\n" : "all ok\n", Failures);
		return Failures ? 1 : 0;
	}
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
	FDay Mild, Shower, Frost, Warm, Festival;
	Mild.Name = "a mild day";
	Shower.Name = "a shower at 14:00";
	Shower.RainFrom = 14.f;
	Shower.RainTo = 15.f;
	Frost.Name = "frost";
	Frost.TempC = -8.f;
	Warm.Name = "a warm day";
	Warm.TempC = 23.f;
	// the town's holiday: the sea festival falls on a Friday (the fourth of October), the evening the Vaults dance too
	Festival.Name = "the sea festival, a Friday";
	Festival.Weekday = 4;
	Festival.bSeaFestival = true;
	for (FDay* D : { &Mild, &Shower, &Frost, &Warm, &Festival })
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
		if (Mild.Seconds[Name] + Shower.Seconds[Name] + Frost.Seconds[Name] + Warm.Seconds[Name] + Festival.Seconds[Name] <= 0.0)
		{
			Never += std::string(" ") + Name;
		}
	}
	// (claps and sways need a holiday's concert and dance, or the Vaults' Friday dances: the festival day has them)
	CHECK(Never.empty(), "every manner is seen in some weather or at the holiday%s%s", Never.empty() ? "" : ": never", Never.c_str());
	CHECK(Festival.Seconds["clap"] > 0.0 && Festival.Seconds["sway"] > 0.0, "the holiday's crowd claps at the concert and sways at the dances (%.1f, %.1f person-hours)",
		Festival.Seconds["clap"] / 3600.0, Festival.Seconds["sway"] / 3600.0);
	CHECK(Mild.SkyOut > 0 && Mild.SkyUp * 4 > Mild.SkyOut && Mild.SkyUp * (long)Mild.BeforeOut > 4L * Mild.BeforeUp * Mild.SkyOut,
		"something in the sky turns the heads up (%d of %d person-samples out of doors during it, %d of %d before)",
		Mild.SkyUp, Mild.SkyOut, Mild.BeforeUp, Mild.BeforeOut);
	CHECK(Mild.PointNoShow == 0, "nobody points at an empty sky (%d)", Mild.PointNoShow);
	{
		// the days' things: a mild day with a dozen birthdays and their guests (docs/town_manners.md "The days' things")
		FDay Bd;
		Bd.Name = "birthdays";
		Bd.bBirthdays = true;
		RunDay(Bd);
		CHECK(Bd.Wrong == 0, "birthdays: nothing that must never be seen (%d%s%s)", Bd.Wrong, Bd.Wrong ? ": " : "", Bd.WrongWhat.c_str());
		CHECK(Bd.Birthdays > 0 && Bd.PropSeconds["cake_box"] > 60.0, "a birthday's cake is carried home in its box (%d birthdays: %.0f person-minutes)",
			Bd.Birthdays, Bd.PropSeconds["cake_box"] / 60.0);
		CHECK(Bd.Guests > 0 && Bd.PropSeconds["flowers"] > 30.0, "birthday guests walk there with carnations (%d guests: %.0f person-minutes; a bag %.0f)",
			Bd.Guests, Bd.PropSeconds["flowers"] / 60.0, Bd.PropSeconds["bag"] / 60.0);
		CHECK(Mild.ChessPlanned == 0 || Mild.ChessPaired * 10 >= Mild.ChessPlanned * 6,
			"the plan's chess players sit on neighbouring seats, the board between them (%d of %d person-minutes within 1.7 m; %d at chess_bench in all)",
			Mild.ChessPaired, Mild.ChessPlanned, Mild.ChessSamples);
	}
	CHECK(Mild.AllDates > 250 && Mild.Dates > 30 && Mild.DatesKept * 5 > Mild.Dates * 2,
		"friends keep their weekly date (%d of the %d pairs whose day it is met, %d more off with a cold; %d pairs in the town)", Mild.DatesKept, Mild.Dates, Mild.DatesIll, Mild.AllDates);
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
	CHECK(Mild.Goodbyes > 5, "a wave or a nod over the shoulder on leaving somebody they know (%d in the day)", Mild.Goodbyes);
	CHECK(Mild.Glances > 100, "people glance at each other in passing (%d)", Mild.Glances);
	const double Ns = Mild.UpdateNs / std::max(1L, Mild.Updates);
	CHECK(Ns < 400.0, "an update of one person costs %.0f ns (48 drawn: %.4f ms a frame)", Ns, Ns * 48.0 / 1e6);
	Friends();
	std::printf(Failures ? "%d FAILED\n" : "all ok\n", Failures);
	return Failures ? 1 : 0;
}
