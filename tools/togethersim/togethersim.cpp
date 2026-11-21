// Tonight's features together (tools/togethersim/run.sh): one game week with all of them on at once, on the real data and
// the real people's life (Private/People/PeopleSim.cpp), the real festivals (Private/Festival/FestivalCalendar.cpp) and the
// real city things with their tournaments (Private/City/CityThingsModel.h, CityCupsModel.h). The week is Monday 7 to Sunday
// 13 June 2027: the town's own Big Sun Day falls on its Saturday (a day off, the eve on Friday), the children fly kites in
// the calendar's wind, the three cups play on Wednesday, Saturday and Sunday, the small jobs, the post round and the
// allotment's neighbour keep their hours, and the hero is on the sanatorium's course all week.
//
// What it checks, every game minute:
//  * a small job (x_jobs.json) is offered on its board's hours only while its boss is really at work there
//    (UPeopleLifeSubsystem::StaffOnShift, the same call the game makes): a job open for more than a short gap with nobody to
//    pay the hero is a broken promise of the "Help wanted" board;
//  * a cup's bracket (FCups::Bracket, as the board and the paper print it) seats nobody who is at work, at school, asleep or
//    out at a festival while their round is on;
//  * the old postman and the allotment's neighbour (Content/City/rounds.json, Content/Garden/garden.json) are not at work, at
//    school or asleep in the hours the game puts them at their door or their bed;
//  * the festival's day off does what the other systems assume: nobody works or goes to school, the town gathers.
//
//   tools/togethersim/run.sh            the week (about two minutes)
//   tools/togethersim/run.sh brief      the same with the per-day table left out
#include "WindModel.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/PeopleSim.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Festival/FestivalCalendar.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/City/CityThingsModel.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/City/CityCupsModel.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace LyudPeople;

static const char* Content = "unreal/Lyudmilovsk/Content";
static int Failures = 0;

#define CHECK(Cond, ...)                                     \
	do                                                       \
	{                                                        \
		const bool bOk_ = (Cond);                            \
		std::printf("  %s ", bOk_ ? "ok  " : "FAIL");        \
		std::printf(__VA_ARGS__);                            \
		std::printf("\n");                                   \
		Failures += bOk_ ? 0 : 1;                            \
	} while (0)

static bool ReadFile(const std::string& Path, std::string& Out)
{
	std::ifstream F(Path, std::ios::binary);
	if (!F)
	{
		std::printf("cannot read %s\n", Path.c_str());
		return false;
	}
	std::stringstream S;
	S << F.rdbuf();
	Out = S.str();
	return true;
}

static const char* const WeekdayName[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };

static std::string Hm(float H)
{
	char B[16];
	const int M = (int)std::lround(H * 60.0);
	std::snprintf(B, sizeof B, "%02d:%02d", (M / 60) % 24, M % 60);
	return B;
}

/** The game's clock over the week: a date and an hour; the life's Time counts from the game's start (20 October 2026 16:00). */
struct FWeekClock
{
	LyudFest::FDate Monday{ 2027, 6, 7 };
	double Abs = 0.0;   // seconds since Monday 00:00
	int StartDays = 0;  // days from the game's first day to Monday

	FWeekClock()
	{
		StartDays = LyudFest::DaysFromCivil(Monday) - LyudFest::DaysFromCivil(LyudFest::FDate{ 2026, 10, 20 });
	}
	int Day() const { return (int)std::floor(Abs / 86400.0); }
	float Hour() const { return (float)std::fmod(Abs / 3600.0, 24.0); }
	LyudFest::FDate Date() const { return LyudFest::AddDays(Monday, Day()); }
	long long Since() const { return (long long)StartDays + Day(); }
	void Set(FWorld& W) const
	{
		W.Hour = Hour();
		W.Weekday = Day() % 7;
		W.Time = ((double)StartDays * 24.0 - 16.0) * 3600.0 + Abs;
		W.Sunrise = 4.75f;   // mid June on the Baltic
		W.Sunset = 21.95f;
		W.bSunUp = W.Hour > W.Sunrise && W.Hour < W.Sunset;
		W.SunYaw = 30.f;
	}
	LyudCity::FWhen When(bool bFestival) const
	{
		LyudCity::FWhen Wh;
		Wh.Since = Since();
		Wh.Day = 1000 + Since();
		Wh.Hour = Hour();
		Wh.Weekday = Day() % 7;
		const LyudFest::FDate D = Date();
		Wh.MonthDay = D.M * 100 + D.D;
		Wh.bFestival = bFestival;
		Wh.Weather = 128 | 32;   // dry and clear: the jobs' lines all answer
		return Wh;
	}
};

/** What somebody is doing now, in words, and whether it keeps them from being somewhere else. */
static std::string Doing(const FWorld& W, const FPerson& P, bool* bBusy)
{
	const std::string A = P.Activity >= 0 ? W.Activities[(size_t)P.Activity].Id : std::string("idle");
	const bool bWork = A.rfind("work_", 0) == 0 || A == "school" || A == "school_town";
	const bool bAway = bWork || A == "sleep" || A.rfind("gather_", 0) == 0;
	if (bBusy)
	{
		*bBusy = bAway && P.State != EState::Idle;
	}
	std::string Where = P.Place >= 0 ? W.Places[(size_t)P.Place].Id : std::string();
	return Where.empty() ? A : A + " at " + Where;
}

/** A stretch of minutes something was wrong: who, which day, from, to, why. */
struct FGap
{
	std::string What;
	int Day = 0;
	float From = 0.f, To = 0.f;
	std::string Why;
};

/** Gathers minutes into stretches per key (a stretch is closed by a minute that is fine again). */
struct FGaps
{
	struct FOpen { int Day; float From; float To; std::string Why; };
	std::map<std::string, FOpen> Open;
	std::vector<FGap> Done;

	void Bad(const std::string& Key, int Day, float H, const std::string& Why)
	{
		auto It = Open.find(Key);
		if (It != Open.end() && It->second.Day == Day && H - It->second.To < 0.05f)
		{
			It->second.To = H + 1.f / 60.f;
			return;
		}
		Close(Key);
		Open[Key] = FOpen{ Day, H, H + 1.f / 60.f, Why };
	}
	void Good(const std::string& Key) { Close(Key); }
	void Close(const std::string& Key)
	{
		auto It = Open.find(Key);
		if (It != Open.end())
		{
			Done.push_back(FGap{ Key, It->second.Day, It->second.From, It->second.To, It->second.Why });
			Open.erase(It);
		}
	}
	void CloseAll()
	{
		while (!Open.empty())
		{
			Close(Open.begin()->first);
		}
	}
	/** The stretches longer than MinMinutes. */
	std::vector<FGap> Longer(float MinMinutes) const
	{
		std::vector<FGap> Out;
		for (const FGap& G : Done)
		{
			if ((G.To - G.From) * 60.f > MinMinutes + 0.5f)
			{
				Out.push_back(G);
			}
		}
		return Out;
	}
};

static void Print(const std::vector<FGap>& Gs, size_t Max = 12)
{
	for (size_t I = 0; I < Gs.size() && I < Max; ++I)
	{
		const FGap& G = Gs[I];
		std::printf("        %s %s %s-%s: %s\n", WeekdayName[G.Day % 7], G.What.c_str(), Hm(G.From).c_str(), Hm(G.To).c_str(), G.Why.c_str());
	}
	if (Gs.size() > Max)
	{
		std::printf("        ... and %d more\n", (int)(Gs.size() - Max));
	}
}

int main(int Argc, char** Argv)
{
	const bool bBrief = Argc > 1 && std::string(Argv[1]) == "brief";
	const std::string C = Content;
	std::string Interiors, Cups, Festivals, Rounds, Garden, Error;
	if (!ReadFile(C + "/City/interiors.json", Interiors) || !ReadFile(C + "/City/tournaments.json", Cups) || !ReadFile(C + "/PeopleData/festivals.json", Festivals)
		|| !ReadFile(C + "/City/rounds.json", Rounds) || !ReadFile(C + "/Garden/garden.json", Garden))
	{
		return 1;
	}
	std::printf("tonight's features together: Monday 7 to Sunday 13 June 2027 (Big Sun Day on Saturday)\n");

	// --- the systems, loaded as the game loads them
	auto W = std::make_unique<FWorld>();
	W->Opt.SettleBatch = 0;
	const char* Animals = std::getenv("LYUD_ANIMALS_DIR") && *std::getenv("LYUD_ANIMALS_DIR") ? std::getenv("LYUD_ANIMALS_DIR") : "unreal/Lyudmilovsk/Content/AnimalsData";
	if (!W->Load(C + "/PeopleData", Animals))
	{
		std::printf("load failed: %s\n", W->LastError.c_str());
		return 1;
	}
	W->AssignHomes();
	LyudCity::FModel M;
	LyudCity::FCups Cup;
	LyudFest::FCalendar Cal;
	const bool bCity = M.Load(Interiors, Error) && Cup.Load(Cups, Error) && Cup.Bind(M);
	const bool bFest = Cal.Load(Festivals);
	CHECK(bCity && bFest, "the city things, the cups and the festivals load (%s%s)", Error.c_str(), Cal.LastError.c_str());
	if (!bCity || !bFest)
	{
		return 1;
	}
	LyudJson::FValue RoundsJ, GardenJ;
	std::string JErr;
	const bool bJson = LyudJson::Parse(Rounds, RoundsJ, JErr) && LyudJson::Parse(Garden, GardenJ, JErr);
	CHECK(bJson, "rounds.json and garden.json parse (%s)", JErr.c_str());
	if (!bJson)
	{
		return 1;
	}

	// the people the other systems name
	const std::string PostmanKey = RoundsJ["post"]["postman"].S();
	const float PostFrom = RoundsJ["post"]["from"].F(9.f), PostUntil = RoundsJ["post"]["until"].F(12.f);
	std::set<int> PostDays;
	for (size_t I = 0; I < RoundsJ["post"]["days"].Size(); ++I)
	{
		PostDays.insert((int)RoundsJ["post"]["days"][I].F());
	}
	const LyudJson::FValue& N = GardenJ["neighbour"];
	const std::string NeighbourKey = N["person"].S();
	const float NFrom = N["hours"][(size_t)0].F(7.f), NTo = N["hours"][(size_t)1].F(10.5f);
	std::set<int> NDays;
	static const char* const Wd3[7] = { "mon", "tue", "wed", "thu", "fri", "sat", "sun" };
	for (size_t I = 0; I < N["days"].Size(); ++I)
	{
		for (int D = 0; D < 7; ++D)
		{
			NDays.insert(N["days"][I].S() == Wd3[D] ? D : -1);
		}
	}
	NDays.erase(-1);
	const int Postman = W->FindPerson(PostmanKey), Neighbour = W->FindPerson(NeighbourKey);
	CHECK(Postman >= 0 && Neighbour >= 0, "the postman (%s) and the allotment's neighbour (%s) live in the town", PostmanKey.c_str(), NeighbourKey.c_str());

	// the small jobs: things whose act is a job_ act with a boss
	// a job keeps hours of its own (a rule's: the kiosk's papers 8-10, the night log 18-7) or only its counter's (the café's
	// ice cream in the café's open hours): a counter is kept by its staff in turns (two days off each), and its job, like the
	// counter, answers while somebody is on (the board says so); only a job's own hours are a promise
	struct FJob { int Thing; int Act; std::string Role; std::string Plot; bool bOwnHours; };
	std::vector<FJob> Jobs;
	auto HoursKey = [](const LyudCity::FThing& T) { return std::to_string(T.bHours ? T.Open : 0.f) + "-" + std::to_string(T.bHours ? T.Close : 24.f) + "/" + std::to_string((int)T.Days); };
	for (int T = 0; T < (int)M.Things.size(); ++T)
	{
		const int A = M.Things[(size_t)T].Act;
		if (A >= 0 && M.Acts[(size_t)A].Id.rfind("job_", 0) == 0 && !M.Acts[(size_t)A].NeedsStaff.empty())
		{
			std::map<std::string, int> Usual;
			for (const LyudCity::FThing& O : M.Things)
			{
				Usual[HoursKey(O)] += O.Plot == M.Things[(size_t)T].Plot ? 1 : 0;
			}
			const auto Most = std::max_element(Usual.begin(), Usual.end(), [](const auto& X, const auto& Y) { return X.second < Y.second; });
			Jobs.push_back(FJob{ T, A, M.Acts[(size_t)A].NeedsStaff, M.Things[(size_t)T].Plot, Most->first != HoursKey(M.Things[(size_t)T]) });
		}
	}
	CHECK(Jobs.size() >= 8, "the small jobs with a boss (%d things)", (int)Jobs.size());

	// the cups' regulars as people of the town (a regular's name is npc.<key>.name)
	std::vector<int> RegularPerson(M.Regulars.size(), -1);
	for (size_t R = 0; R < M.Regulars.size(); ++R)
	{
		const std::string& Nm = M.Regulars[R].Name;
		if (Nm.rfind("npc.", 0) == 0 && Nm.size() > 9)
		{
			RegularPerson[R] = W->FindPerson(Nm.substr(4, Nm.size() - 9));
		}
	}
	// no cup on a public holiday, as ULyudCityThings asks its festivals (a day since the game began -> its date)
	Cup.IsHoliday = [&Cal](long long Since) {
		return Cal.IsDayOff(LyudFest::AddDays(LyudFest::FDate{ 2026, 10, 20 }, (int)Since));
	};
	// the hero signs up for every cup of the week (his seat is in each bracket, so the field is seven regulars)
	FWeekClock Clk;
	for (int K = 0; K < (int)Cup.Defs.size(); ++K)
	{
		FWeekClock At = Clk;
		At.Abs = (double)Cup.Defs[(size_t)K].Weekday * 86400.0 + 8.0 * 3600.0;
		Cup.SignUp(K, At.When(false));
	}

	// --- the week
	LyudWeather::FWindParams WindP;
	WindP.Seed = 921;
	const double Hours2000 = LyudWeather::HoursSince2000(2027, 6, 7, 0.0);
	Clk.Abs = -6.0 * 3600.0;   // from Sunday 18:00, so Monday starts with everybody placed
	Clk.Set(*W);
	W->PrevHour = -1.f;
	W->Wind = LyudWeather::Regime(Hours2000 + Clk.Abs / 3600.0, WindP).Speed;
	W->Resettle();

	FGaps JobGaps, CupClash, PostClash, NeighbourClash;
	std::set<std::string> Gathering;              // festival.phase keys set on the life
	int DayOffWorkers = 0, DayOffPupils = 0, DayOffMinutes = 0, FestPeak = 0, KiteMinutes = 0, KiteWindy = 0;
	float KiteLo = 99.f, KiteHi = 0.f;
	std::map<int, int> DayKites, DayJobsOpen, DayJobsManned, DayGather;
	std::map<std::string, int> JobOpen, JobManned;   // act id -> minutes offered / with a boss there
	int CupChecks = 0;
	std::set<std::string> CupsSkipped;
	const float Dt = 2.f;
	const double End = 7.0 * 86400.0;
	double NextMinute = 0.0;
	while (Clk.Abs < End)
	{
		Clk.Abs += Dt;
		Clk.Set(*W);
		W->Step(Dt);
		W->Events.clear();
		if (Clk.Abs < NextMinute)
		{
			continue;
		}
		NextMinute += 60.0;
		if (Clk.Abs < 0.0)
		{
			continue;
		}
		const int Day = Clk.Day();
		const float H = Clk.Hour();
		const LyudFest::FDate Date = Clk.Date();
		if (std::fmod(Clk.Abs, 300.0) < 60.0)
		{
			W->Wind = LyudWeather::Regime(Hours2000 + Clk.Abs / 3600.0, WindP).Speed;
		}

		// the festivals, as ULyudFestivalSubsystem hands them to the people's life
		std::string OffId;
		const bool bOff = Cal.IsDayOff(Date, &OffId);
		W->bDayOff = bOff;
		std::set<std::string> Want;
		bool bFestivalToday = false;
		for (const LyudFest::FActive& A : Cal.At(Date, H))
		{
			const LyudFest::FFestival& F = Cal.Festivals[(size_t)A.Festival];
			bFestivalToday = bFestivalToday || F.Rule != "weekly";
			for (int Pi : A.Phases)
			{
				const LyudFest::FPhase& P = F.Phases[(size_t)Pi];
				if (P.Venue.empty())
				{
					continue;
				}
				const std::string Key = F.Id + "." + P.Id;
				Want.insert(Key);
				if (!Gathering.count(Key))
				{
					const std::string Where = (P.Face == "stage" && P.Venue == "square" && W->FindPlace("S-SQUARE-AUDIENCE") >= 0) ? std::string("S-SQUARE-AUDIENCE") : P.Venue;
					const EGatherMode Mode = P.Mode == "watch" ? EGatherMode::Watch : P.Mode == "sit" ? EGatherMode::Sit : P.Mode == "fair" ? EGatherMode::Fair : EGatherMode::Mingle;
					W->SetGathering(Key, Where, std::fmod(P.From + 48.f, 24.f), std::fmod(P.To + 48.f, 24.f), P.Pull, Mode, false, V3());
					Gathering.insert(Key);
				}
			}
		}
		for (auto It = Gathering.begin(); It != Gathering.end();)
		{
			if (!Want.count(*It))
			{
				W->ClearGathering(*It);
				It = Gathering.erase(It);
			}
			else
			{
				++It;
			}
		}

		// the town now
		int Gathered = 0, Kites = 0;
		for (const FPerson& P : W->People)
		{
			if (P.Activity < 0 || P.State == EState::Idle)
			{
				continue;
			}
			const std::string& A = W->Activities[(size_t)P.Activity].Id;
			Gathered += A.rfind("gather_", 0) == 0 ? 1 : 0;
			Kites += A == "fly_kite" && P.State == EState::Doing ? 1 : 0;
			if (bOff && P.State == EState::Doing && H > 9.f && H < 17.f)
			{
				DayOffWorkers += (A == "work_office" || A == "work_away") ? 1 : 0;
				DayOffPupils += (A == "school" || A == "school_town") ? 1 : 0;
			}
		}
		DayOffMinutes += bOff ? 1 : 0;
		FestPeak = std::max(FestPeak, bOff ? Gathered : 0);
		DayGather[Day] = std::max(DayGather[Day], Gathered);
		DayKites[Day] = std::max(DayKites[Day], Kites);
		if (Kites > 0)
		{
			KiteMinutes += Kites;
			KiteLo = std::min(KiteLo, W->Wind);
			KiteHi = std::max(KiteHi, W->Wind);
			KiteWindy += (W->Wind < 2.f || W->Wind > 15.f) ? Kites : 0;
		}

		// the small jobs: offered on the board's hours, and somebody there to pay
		const LyudCity::FWhen Wh = Clk.When(bFestivalToday);
		for (const FJob& J : Jobs)
		{
			const std::string Key = M.Things[(size_t)J.Thing].Id;
			if (M.ActIndexNow(J.Thing, Wh, false) != J.Act || M.IsShut(J.Thing, Wh, false))
			{
				JobGaps.Good(Key);
				continue;
			}
			std::vector<int> Staff;
			W->StaffOnShift(J.Plot, J.Role, Staff);
			const std::string& ActId = M.Acts[(size_t)J.Act].Id;
			JobOpen[ActId]++;
			DayJobsOpen[Day]++;
			JobManned[ActId] += Staff.empty() ? 0 : 1;
			DayJobsManned[Day] += Staff.empty() ? 0 : 1;
			if (!Staff.empty() || !J.bOwnHours)
			{
				JobGaps.Good(Key);
			}
			else
			{
				JobGaps.Bad(Key, Day, H, std::string("no ") + J.Role + " on shift" + (bOff ? " (" + OffId + ", a day off)" : ""));
			}
		}

		// the cups: who the bracket seats while their round is on
		for (int K = 0; K < (int)Cup.Defs.size(); ++K)
		{
			const LyudCity::FCupDef& D = Cup.Defs[(size_t)K];
			if (D.Weekday != Day % 7)
			{
				continue;
			}
			const float Rel = H - D.Start;
			const int Round = (int)std::floor(Rel / D.Step);
			if (Rel < 0.f || Round >= LyudCity::FBracket::Rounds)
			{
				continue;
			}
			if (Cup.IsOff(Clk.Since()))
			{
				CupsSkipped.insert(D.Id);
				continue;
			}
			const LyudCity::FBracket B = Cup.Bracket(M, K, Clk.Since(), Wh);
			for (int Mi = LyudCity::FBracket::FirstMatch(Round); Mi < LyudCity::FBracket::FirstMatch(Round) + (Round == 0 ? 4 : Round == 1 ? 2 : 1); ++Mi)
			{
				for (int S = 0; S < 2; ++S)
				{
					const int Seat = B.Side[Mi][S];
					if (Seat < 0 || B.Seat[Seat] == LyudCity::FBracket::HeroSeat)
					{
						continue;
					}
					const int R = B.Seat[Seat];
					const int Pi = R >= 0 && R < (int)RegularPerson.size() ? RegularPerson[(size_t)R] : -1;
					const std::string Key = D.Id + " " + M.Regulars[(size_t)R].Id;
					if (Pi < 0)
					{
						continue;
					}
					CupChecks++;
					bool bBusy = false;
					const std::string What = Doing(*W, W->People[(size_t)Pi], &bBusy);
					if (bBusy)
					{
						CupClash.Bad(Key, Day, H, "round " + std::to_string(Round + 1) + ", but " + W->People[(size_t)Pi].Key + " is " + What);
					}
					else
					{
						CupClash.Good(Key);
					}
				}
			}
		}

		// the postman at his door, the neighbour at her bed
		auto Expect = [&](FGaps& G, int Pi, const char* Role, bool bWindow) {
			if (Pi < 0 || !bWindow)
			{
				return;
			}
			bool bBusy = false;
			const std::string What = Doing(*W, W->People[(size_t)Pi], &bBusy);
			if (bBusy && What.rfind("gather_", 0) != 0)
			{
				G.Bad(Role, Day, H, std::string(Role) + " is " + What);
			}
			else
			{
				G.Good(Role);
			}
		};
		Expect(PostClash, Postman, "the postman", PostDays.count(Day % 7) && H >= PostFrom && H < PostUntil);
		Expect(NeighbourClash, Neighbour, "the neighbour", NDays.count(Day % 7) && H >= NFrom && H < NTo);
	}
	JobGaps.CloseAll();
	CupClash.CloseAll();
	PostClash.CloseAll();
	NeighbourClash.CloseAll();

	if (!bBrief)
	{
		std::printf("\n  day  gathered  kites  jobs open/with a boss (minutes)\n");
		for (int D = 0; D < 7; ++D)
		{
			const LyudFest::FDate Dd = LyudFest::AddDays(Clk.Monday, D);
			std::string Off;
			const bool bO = Cal.IsDayOff(Dd, &Off);
			std::printf("  %s %02d  %8d  %5d  %5d/%-5d %s\n", WeekdayName[D], Dd.D, DayGather[D], DayKites[D], DayJobsOpen[D], DayJobsManned[D], bO ? Off.c_str() : "");
		}
		std::printf("\n");
	}

	// --- the verdicts
	CHECK(DayOffMinutes > 0 && FestPeak > 200 && DayOffWorkers == 0 && DayOffPupils == 0,
		"Big Sun Day: a day off (%d min), the town gathers (%d at once), nobody at the office or at school (%d, %d)", DayOffMinutes, FestPeak,
		DayOffWorkers, DayOffPupils);
	CHECK(KiteMinutes > 0 && KiteWindy == 0, "children fly kites in a breeze (%d person-minutes in %.1f-%.1f m/s, %d in a calm or a gale)", KiteMinutes,
		KiteLo, KiteHi, KiteWindy);

	// a job's boss walks in a few minutes after its hours begin: a gap up to 20 minutes is the walk
	std::vector<FGap> JobBad, JobHoliday;
	for (const FGap& G : JobGaps.Longer(20.f))
	{
		(G.Why.find("a day off") != std::string::npos ? JobHoliday : JobBad).push_back(G);
	}
	for (const FJob& J : Jobs)
	{
		const std::string& Id = M.Acts[(size_t)J.Act].Id;
		if (JobOpen.count(Id))
		{
			std::printf("       %-20s offered %4d min, with its boss there %4d min (%s)\n", Id.c_str(), JobOpen[Id], JobManned[Id], M.Things[(size_t)J.Thing].Id.c_str());
		}
	}
	CHECK(JobBad.empty(), "every small job with hours of its own has its boss at work whenever it is offered (%d stretches without)", (int)JobBad.size());
	Print(JobBad);
	CHECK(JobHoliday.empty(), "no small job is offered on a day off with its boss on holiday (%d stretches)", (int)JobHoliday.size());
	Print(JobHoliday);
	const std::vector<FGap> CupBad = CupClash.Longer(15.f);
	CHECK(CupChecks > 0 && CupBad.empty(), "the cups seat nobody who is at work, at school, asleep or at a festival in their round (%d looks, %d clashes)", CupChecks,
		(int)CupBad.size());
	Print(CupBad);
	CHECK(CupsSkipped.count("cog_chess") == 1 && CupsSkipped.size() == 1, "the Saturday chess cup is not played on Big Sun Day, the other two are (%d skipped)",
		(int)CupsSkipped.size());
	const std::vector<FGap> PostBad = PostClash.Longer(15.f), NBad = NeighbourClash.Longer(15.f);
	CHECK(PostBad.empty(), "the old postman (%s) is free in his round's hours", PostmanKey.c_str());
	Print(PostBad);
	CHECK(NBad.empty(), "the allotment's neighbour (%s) is free in her hours at the bed", NeighbourKey.c_str());
	Print(NBad);

	std::printf("%s\n", Failures ? (std::to_string(Failures) + " FAILED").c_str() : "all ok");
	return Failures ? 1 : 0;
}
