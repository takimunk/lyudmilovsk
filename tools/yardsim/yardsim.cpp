// Children's games in the yards, outside the engine (docs/yard_games.md, backlog item 32): the rules of each game on a
// made-up group (start, roles, the game's own events, the end), the hero hiding and seeking, children called away, then
// a school day and a Saturday afternoon of the real town's life (PeopleSim) with the games offered the children at play,
// as ULyudYardGames does. Run by tools/yardsim/run.sh.
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/YardGames.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/PeopleSim.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace LyudYard;

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

/** What is wrong with a game's roles now ("" nothing): the parts each game must have while it is played. */
static std::string RoleProblem(const FYard& Y, const FGame& G)
{
	if (G.Phase != EPhase::Play)
	{
		return "";
	}
	char B[160];
	const int N = (int)G.Kids.size();
	switch (G.Kind)
	{
	case EGame::Hopscotch:
		if (G.NumIn(ERole::Hopper) > 1) return "two on the squares";
		break;
	case EGame::Rope:
		if (G.PhaseT > 4.f && N >= 3 && G.NumIn(ERole::Turner) != 2)
		{
			std::snprintf(B, sizeof B, "%d turners", G.NumIn(ERole::Turner));
			return B;
		}
		if (G.NumIn(ERole::Jumper) > 1) return "two jumpers";
		break;
	case EGame::Hide:
		if (G.NumIn(ERole::Seeker) != 1)
		{
			std::snprintf(B, sizeof B, "%d seekers", G.NumIn(ERole::Seeker));
			return B;
		}
		break;
	case EGame::Football:
	{
		int T[2] = { 0, 0 }, Kp[2] = { 0, 0 };
		for (const FKid& K : G.Kids)
		{
			T[K.Team & 1]++;
			Kp[K.Team & 1] += K.Role == ERole::Keeper ? 1 : 0;
		}
		if (std::abs(T[0] - T[1]) > 2) return "uneven sides";
		if (Kp[0] > 1 || Kp[1] > 1) return "two keepers";
		const FArea& A = Y.Areas[(size_t)G.Area];
		const FV2 D = G.Ball.Pos - A.C;
		const FV2 F = FV2::Dir(A.FieldYaw);
		if (std::fabs(D.Dot(F)) > A.FieldHalfL + 2.f || std::fabs(D.Dot(FV2(-F.Y, F.X))) > A.FieldHalfW + 2.f) return "the ball off the field";
		break;
	}
	case EGame::Tag:
		if (G.NumIn(ERole::It) != 1)
		{
			std::snprintf(B, sizeof B, "%d are it", G.NumIn(ERole::It));
			return B;
		}
		break;
	default: break;
	}
	const FArea& A = Y.Areas[(size_t)G.Area];
	for (const FKid& K : G.Kids)
	{
		if (K.Id != HeroId && (K.Pos - A.C).Len() > A.R + 7.5f) return "a child off the area";
		if (K.Lift < -0.01f || K.Lift > 0.5f) return "a child in the air";
	}
	return "";
}

static std::vector<FCandidate> Group(const FArea& A, int N, int FirstId)
{
	std::vector<FCandidate> Out;
	for (int I = 0; I < N; I++)
	{
		FCandidate C;
		C.Id = FirstId + I;
		C.Age = 6 + (I * 3) % 8;
		C.Pos = A.C + FV2::Dir(40.f * (float)I) * (4.f + (float)(I % 3));
		Out.push_back(C);
	}
	return Out;
}

/** One game on a made-up group of N children for a while: its events counted, the roles checked every step. */
static void RunScripted(const std::string& Json, EGame Kind, int N)
{
	FYard Y;
	Y.Load(Json);
	int Area = -1;
	for (int I = 0; I < (int)Y.Areas.size() && Area < 0; I++)
	{
		if (Y.Areas[(size_t)I].Fits(Kind) && Y.Areas[(size_t)I].Kind == "yard") Area = I;
	}
	for (int I = 0; I < (int)Y.Areas.size() && Area < 0; I++)
	{
		if (Y.Areas[(size_t)I].Fits(Kind)) Area = I;
	}
	std::printf("%s at %s, %d children:\n", GameName(Kind), Area >= 0 ? Y.Areas[(size_t)Area].Id.c_str() : "-", N);
	if (Area < 0)
	{
		CHECK(false, "an area fits %s", GameName(Kind));
		return;
	}
	const std::vector<FCandidate> Kids = Group(Y.Areas[(size_t)Area], N, 100);
	const bool bStarted = Y.Start(Area, Kind, Kids, 600.f);
	CHECK(bStarted, "the game starts (min %d players)", FYard::MinPlayers(Kind));
	if (!bStarted) return;
	std::map<EEvent, int> Ev;
	std::string Problem;
	float PlayAt = -1.f, T = 0.f, MaxLift = 0.f;
	int Leader = Y.Games[0].Leader, OldestAge = 0;
	for (const FCandidate& C : Kids) OldestAge = std::max(OldestAge, C.Age);
	const int LeaderAge = Y.Games[0].Find(Leader) ? Y.Games[0].Find(Leader)->Age : -1;
	std::set<int> Seekers, Its, Hoppers, Jumpers;
	int MaxLevel = 0;
	const float Dt = 0.1f;
	while (T < 700.f && !Y.Games.empty())
	{
		Y.Offer(Area, Kids);
		for (int K = 0; K < 10 && !Y.Games.empty(); K++)
		{
			Y.Step(Dt);
			T += Dt;
			if (Y.Games.empty()) break;
			const FGame& G = Y.Games[0];
			if (G.Phase == EPhase::Play && PlayAt < 0.f) PlayAt = T;
			const std::string P = RoleProblem(Y, G);
			if (!P.empty() && Problem.empty())
			{
				char B[200];
				std::snprintf(B, sizeof B, "%s at %.1f s", P.c_str(), T);
				Problem = B;
			}
			for (const FKid& Kd : G.Kids)
			{
				MaxLift = std::max(MaxLift, Kd.Lift);
				if (Kd.Role == ERole::Seeker) Seekers.insert(Kd.Id);
				if (Kd.Role == ERole::It) Its.insert(Kd.Id);
				if (Kd.Role == ERole::Hopper) Hoppers.insert(Kd.Id);
				if (Kd.Role == ERole::Jumper) Jumpers.insert(Kd.Id);
				MaxLevel = std::max(MaxLevel, Kd.Level);
			}
		}
		for (const FEvent& E : Y.Events) Ev[E.Kind]++;
		Y.Events.clear();
	}
	for (const FEvent& E : Y.Events) Ev[E.Kind]++;
	CHECK(LeaderAge == OldestAge, "the oldest leads (leader %d, age %d)", Leader, LeaderAge);
	CHECK(PlayAt > 0.f && PlayAt < 16.f, "they gather and the play begins (%.1f s)", PlayAt);
	CHECK(Ev[EEvent::Roles] >= 1, "the roles are given out (%d times)", Ev[EEvent::Roles]);
	CHECK(Problem.empty(), "the roles hold all through (%s)", Problem.empty() ? "every step" : Problem.c_str());
	switch (Kind)
	{
	case EGame::Hopscotch:
		CHECK(Hoppers.size() >= 3, "they take turns on the squares (%zu hopped)", Hoppers.size());
		CHECK(MaxLift > 0.1f && MaxLevel >= 1, "they hop (%.2f m) and get further (level %d)", MaxLift, MaxLevel);
		CHECK(Ev[EEvent::Miss] >= 1, "a foot on a line ends a turn (%d)", Ev[EEvent::Miss]);
		break;
	case EGame::Rope:
		CHECK(Jumpers.size() >= 3, "they take turns jumping (%zu jumped)", Jumpers.size());
		CHECK(MaxLift > 0.15f, "they jump the rope (%.2f m)", MaxLift);
		CHECK(Ev[EEvent::Swap] >= 1, "who trips turns the rope (%d swaps)", Ev[EEvent::Swap]);
		break;
	case EGame::Hide:
		CHECK(Ev[EEvent::Found] >= 3, "the seeker finds them (%d found, %d home free)", Ev[EEvent::Found], Ev[EEvent::Saved]);
		CHECK(Ev[EEvent::RoundEnd] >= 2 && Seekers.size() >= 2, "rounds end and the first found seeks next (%d rounds, %zu seekers)", Ev[EEvent::RoundEnd], Seekers.size());
		break;
	case EGame::Football:
		CHECK(Ev[EEvent::Goal] >= 1, "goals are scored (%d)", Ev[EEvent::Goal]);
		break;
	case EGame::Tag:
		CHECK(Ev[EEvent::Tagged] >= 3 && Its.size() >= 3, "it tags and the tagged chase (%d tags, %zu were it)", Ev[EEvent::Tagged], Its.size());
		break;
	default: break;
	}
	CHECK(Ev[EEvent::End] == 1 && Y.Games.empty(), "it ends when its time is up (%.0f s)", T);
}

/** Children called home one by one: they leave, and the game ends when too few are left. */
static void RunLeaving(const std::string& Json)
{
	std::printf("children called away:\n");
	FYard Y;
	Y.Load(Json);
	int Area = -1;
	for (int I = 0; I < (int)Y.Areas.size() && Area < 0; I++) if (Y.Areas[(size_t)I].Fits(EGame::Tag)) Area = I;
	std::vector<FCandidate> Kids = Group(Y.Areas[(size_t)Area], 5, 200);
	Y.Start(Area, EGame::Tag, Kids, 1200.f);
	int Leaves = 0, Ends = 0, Why = -1;
	for (int S = 0; S < 120 && !Y.Games.empty(); S++)
	{
		if (S % 20 == 19 && !Kids.empty()) Kids.pop_back();
		Y.Offer(Area, Kids);
		for (int K = 0; K < 10; K++) Y.Step(0.1f);
		for (const FEvent& E : Y.Events)
		{
			Leaves += E.Kind == EEvent::Leave ? 1 : 0;
			if (E.Kind == EEvent::End) { Ends++; Why = E.Other; }
		}
		Y.Events.clear();
	}
	CHECK(Leaves == 3 && Ends == 1 && Why == 1, "three leave, then it ends with too few (%d left, end %d, why %d)", Leaves, Ends, Why);
}

/** The hero joins hide-and-seek and crouches behind a far thing; then seeks and finds them all with E. */
static void RunHero(const std::string& Json)
{
	std::printf("the hero in hide-and-seek:\n");
	FYard Y;
	Y.Load(Json);
	int Area = -1;
	for (int I = 0; I < (int)Y.Areas.size() && Area < 0; I++) if (Y.Areas[(size_t)I].Fits(EGame::Hide) && Y.Areas[(size_t)I].Kind == "yard") Area = I;
	const FArea& A = Y.Areas[(size_t)Area];
	const std::vector<FCandidate> Kids = Group(A, 4, 300);
	Y.Start(Area, EGame::Hide, Kids, 1500.f);
	Y.bHeroHere = true;
	Y.HeroPos = A.C;
	// till they count
	for (int K = 0; K < 200 && (Y.Games[0].Phase != EPhase::Play); K++) Y.Step(0.1f);
	const bool bJoined = Y.HeroJoin(0);
	CHECK(bJoined && Y.Games[0].Find(HeroId) && Y.Games[0].Find(HeroId)->Role == ERole::Hider, "the hero joins while the seeker counts, as a hider");
	// the far hiding place the children didn't take
	int Far = 0;
	for (int C = 0; C < (int)A.Cover.size(); C++) if ((A.Cover[(size_t)C].At - A.Wall).Len() > (A.Cover[(size_t)Far].At - A.Wall).Len()) Far = C;
	Y.HeroPos = A.Cover[(size_t)Far].At;
	bool bFound = false, bRoundEnd = false;
	for (int K = 0; K < 3000 && !bRoundEnd; K++)
	{
		Y.Offer(Area, Kids);
		Y.Step(0.1f);
		for (const FEvent& E : Y.Events)
		{
			bFound = bFound || (E.Kind == EEvent::Found && E.Who == HeroId);
			bRoundEnd = bRoundEnd || E.Kind == EEvent::RoundEnd;
		}
		Y.Events.clear();
	}
	CHECK(bRoundEnd, "the round ends with the hero found or never found (%s)", bFound ? "found" : "not found");
	// now he seeks: walks up to each hider and says found
	const bool bSeek = Y.HeroSeek(0);
	for (int K = 0; K < 400 && Y.Games[0].Count > 0.f; K++) { Y.Offer(Area, Kids); Y.Step(0.1f); }
	CHECK(bSeek && Y.Games[0].Seeker == HeroId, "the hero is the seeker (counts %.0f s)", 15.f);
	for (int K = 0; K < 60; K++) Y.Step(0.1f);   // they settle behind their things
	int Found = 0;
	const int Before = Y.HeroFind();
	CHECK(Before == -1 || (Y.HeroPos - A.Wall).Len() < 3.f, "nobody is found from where nobody hides");
	for (int Round = 0; Round < 10; Round++)
	{
		const FGame& G = Y.Games[0];
		int Target = -1;
		for (const FKid& K : G.Kids) if (K.Role == ERole::Hider) { Target = K.Id; Y.HeroPos = K.Pos + FV2(1.f, 0.f); break; }
		if (Target < 0) break;
		Y.Step(0.05f);
		Found += Y.HeroFind() >= 0 ? 1 : 0;
	}
	bool bAll = false;
	for (const FEvent& E : Y.Events) bAll = bAll || E.Kind == EEvent::RoundEnd;
	Y.Step(0.1f);
	for (const FEvent& E : Y.Events) bAll = bAll || E.Kind == EEvent::RoundEnd;
	CHECK(Found >= 3 && bAll, "the hero finds them with E and the round ends (%d found)", Found);
	Y.bHeroHere = false;
	Y.Step(0.1f);
	CHECK(!Y.Games.empty() && !Y.Games[0].Find(HeroId), "the hero walking off leaves the game, which goes on without him");
}

/** The game's clock over an afternoon, as tools/togethersim (the life's Time counts from 20 October 2026 16:00). */
static void SetClock(LyudPeople::FWorld& W, int DaysFromStart, int Weekday, double Abs)
{
	W.Hour = (float)std::fmod(Abs / 3600.0, 24.0);
	W.Weekday = Weekday;
	W.Time = ((double)DaysFromStart * 24.0 - 16.0) * 3600.0 + Abs;
	W.Sunrise = 4.75f;
	W.Sunset = 21.95f;
	W.bSunUp = W.Hour > W.Sunrise && W.Hour < W.Sunset;
	W.SunYaw = 30.f;
}

static void RunTown(const std::string& Json, int Weekday, const char* Name)
{
	using LyudPeople::FWorld;
	using LyudPeople::FPlace;
	using LyudPeople::FPerson;
	using LyudPeople::EState;
	std::printf("the town's children, %s afternoon (13:00-20:30):\n", Name);
	auto W = std::make_unique<FWorld>();
	W->Opt.SettleBatch = 0;
	const char* Animals = std::getenv("LYUD_ANIMALS_DIR") && *std::getenv("LYUD_ANIMALS_DIR") ? std::getenv("LYUD_ANIMALS_DIR") : "unreal/Lyudmilovsk/Content/AnimalsData";
	if (!W->Load(std::string(Content) + "/PeopleData", Animals))
	{
		CHECK(false, "the people's life loads (%s)", W->LastError.c_str());
		return;
	}
	W->AssignHomes();
	FYard Y;
	Y.Load(Json);
	int Yards = 0;
	for (const FPlace& P : W->Places) Yards += P.Kind == "yard" ? 1 : 0;
	CHECK(Yards >= 4, "the yards are the people's places (%d of kind yard)", Yards);
	const int Play[2] = { W->FindActivity("playground"), W->FindActivity("play_run") };
	// 2027-06-07 is a Monday: days from the game's start (2026-10-20)
	const int Days = 230 + Weekday;
	double Abs = 9.0 * 3600.0;
	SetClock(*W, Days, Weekday, Abs);
	W->PrevHour = -1.f;
	W->Wind = 3.f;
	W->Resettle();
	std::map<int, std::map<EGame, int>> Started;           // area -> kind -> games
	std::map<int, double> PlayKidSeconds;                   // area -> child-seconds in a game
	std::map<int, std::set<int>> Players;                   // area -> who played there
	std::set<int> InYardPlace;
	int MaxGames = 0, Starts = 0, Ends = 0, TwoGames = 0, KidsAtPlay = 0;
	std::string Problem;
	const double End = 20.5 * 3600.0;
	while (Abs < End)
	{
		Abs += 1.0;
		SetClock(*W, Days, Weekday, Abs);
		W->Step(1.f);
		W->Events.clear();
		if (Abs < 13.0 * 3600.0) continue;
		// the children at play near each area, every second, as ULyudYardGames gathers them
		std::vector<std::vector<FCandidate>> Offers(Y.Areas.size());
		int AtPlay = 0;
		for (const FPerson& P : W->People)
		{
			if (P.Age < 5 || P.Age > 13 || P.Activity < 0 || (P.Activity != Play[0] && P.Activity != Play[1])) continue;
			if (P.State != EState::Doing && P.State != EState::Going) continue;
			AtPlay++;
			if (P.Place >= 0 && W->Places[(size_t)P.Place].Kind == "yard") InYardPlace.insert(P.Id);
			const FV2 Pos(P.Pos.X, P.Pos.Y);
			const int Gi = Y.InGame(P.Id);
			// (one in a game is offered to its own area only)
			const int Own = Gi >= 0 ? Y.Games[(size_t)Gi].Area : -1;
			for (int A = Own >= 0 ? Own : 0; A < (Own >= 0 ? Own + 1 : (int)Y.Areas.size()); A++)
			{
				const FArea& Ar = Y.Areas[(size_t)A];
				if (FYard::Near(Ar, Pos, P.State == EState::Doing, Gi >= 0 && Y.Games[(size_t)Gi].Area == A))
				{
					FCandidate C;
					C.Id = P.Id;
					C.Age = P.Age;
					C.Pos = Pos;
					Offers[(size_t)A].push_back(C);
					break;
				}
			}
		}
		KidsAtPlay = std::max(KidsAtPlay, AtPlay);
		for (int A = 0; A < (int)Y.Areas.size(); A++) Y.Offer(A, Offers[(size_t)A]);
		for (int K = 0; K < 4; K++) Y.Step(0.25f);
		MaxGames = std::max(MaxGames, (int)Y.Games.size());
		std::set<int> Seen;
		for (const FGame& G : Y.Games)
		{
			for (const FKid& K : G.Kids)
			{
				TwoGames += Seen.insert(K.Id).second ? 0 : 1;
				PlayKidSeconds[G.Area] += 1.0;
				Players[G.Area].insert(K.Id);
			}
			const std::string P = RoleProblem(Y, G);
			if (!P.empty() && Problem.empty()) Problem = Y.Areas[(size_t)G.Area].Id + ": " + P;
		}
		for (const FEvent& E : Y.Events)
		{
			if (E.Kind == EEvent::Start) { Started[E.Area][E.G]++; Starts++; }
			Ends += E.Kind == EEvent::End ? 1 : 0;
		}
		Y.Events.clear();
	}
	Y.EndAll();
	for (const FEvent& E : Y.Events) Ends += E.Kind == EEvent::End ? 1 : 0;
	Y.Events.clear();
	std::set<EGame> Kinds;
	int YardGames = 0;
	for (const auto& A : Started)
	{
		std::string Line;
		int N = 0;
		for (const auto& K : A.second)
		{
			Kinds.insert(K.first);
			N += K.second;
			Line += std::string(" ") + GameName(K.first) + " x" + std::to_string(K.second);
		}
		if (Y.Areas[(size_t)A.first].Kind == "yard") YardGames += N;
		std::printf("    %-15s %3d games,%s; %zu children, %.0f child-minutes\n", Y.Areas[(size_t)A.first].Id.c_str(), N, Line.c_str(),
			Players[A.first].size(), PlayKidSeconds[A.first] / 60.0);
	}
	std::printf("    (up to %d children at play at once, %zu in the yards' places; up to %d games at once)\n", KidsAtPlay, InYardPlace.size(), MaxGames);
	CHECK(InYardPlace.size() >= 5, "children go out to play in their yards (%zu)", InYardPlace.size());
	CHECK(Starts >= 6 && Kinds.size() >= 4, "games start in the afternoon (%d games, %zu kinds)", Starts, Kinds.size());
	CHECK(YardGames >= 3, "games in the yards (%d)", YardGames);
	CHECK(Starts == Ends, "every game that started ended (%d started, %d ended)", Starts, Ends);
	CHECK(TwoGames == 0, "nobody is in two games at once");
	CHECK(Problem.empty(), "the roles hold in every game (%s)", Problem.empty() ? "all afternoon" : Problem.c_str());
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "all";
	std::string Json;
	if (!ReadFile(std::string(Content) + "/PeopleData/yard_games.json", Json))
	{
		return 1;
	}
	FYard Probe;
	CHECK(Probe.Load(Json), "yard_games.json loads (%zu areas) %s", Probe.Areas.size(), Probe.LastError.c_str());
	if (Mode == "all" || Mode == "rules")
	{
		RunScripted(Json, EGame::Hopscotch, 4);
		RunScripted(Json, EGame::Rope, 6);
		RunScripted(Json, EGame::Hide, 6);
		RunScripted(Json, EGame::Football, 8);
		RunScripted(Json, EGame::Tag, 6);
		RunLeaving(Json);
		RunHero(Json);
	}
	if (Mode == "all" || Mode == "town")
	{
		RunTown(Json, 0, "Monday");
		RunTown(Json, 5, "Saturday");
	}
	std::printf("%s: %d failure(s)\n", Failures ? "FAIL" : "ok", Failures);
	return Failures ? 1 : 0;
}
