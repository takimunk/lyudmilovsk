// The days of the people's life in the game (docs/town_life.md "The days"): the console and developer mode's hooks for
// what Private/People/PeopleDays.cpp works out (a cold, a bad night, a birthday, the pay, the pension, the day's plans).
// Nothing here ticks: the life itself rolls the day at midnight and after a load (FWorld::RollToday).
#include "People/PeopleLife.h"

#include "PeopleSim.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#if __has_include("Dev/LyudDev.h")
#include "Dev/LyudDev.h"
#endif

namespace LyudTownDaysLocal
{
	namespace LP = LyudPeople;

	LP::FWorld* SimOf(UWorld* World)
	{
		const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(World);
		return People ? People->GetSim() : nullptr;
	}

	/** The townsperson up and about nearest the player (within 60 m), -1 none. */
	int32 NearestTownsperson(UWorld* World, const LP::FWorld& S)
	{
		const APawn* Hero = UGameplayStatics::GetPlayerPawn(World, 0);
		if (!Hero)
		{
			return -1;
		}
		const FVector At = Hero->GetActorLocation() * 0.01;
		const LP::V3 Here((float)At.X, (float)At.Y, (float)At.Z);
		int32 Best = -1;
		float BestD = 60.f;
		for (const LP::FPerson& P : S.People)
		{
			if (!P.bTown || P.StayCycle > 0 || P.Home < 0 || P.Pose == LP::EPose::Hidden || P.Pose == LP::EPose::Sleep || S.InsideShell(P))
			{
				continue;
			}
			const float D = (P.Pos - Here).Len2D();
			if (D < BestD)
			{
				BestD = D;
				Best = P.Id;
			}
		}
		return Best;
	}

	/** A friend of theirs in the town who lives elsewhere, -1 none. */
	int32 FriendOf(const LP::FWorld& S, const LP::FPerson& P)
	{
		for (const std::string& Key : P.Friends)
		{
			const int Id = S.FindPerson(Key);
			if (Id >= 0 && Id != P.Id && S.People[(size_t)Id].bTown && S.People[(size_t)Id].Home >= 0 && S.People[(size_t)Id].Home != P.Home)
			{
				return Id;
			}
		}
		return -1;
	}

	void Report(UWorld* World)
	{
		const LP::FWorld* S = SimOf(World);
		if (!S)
		{
			return;
		}
		int32 Bits[LP::NumTodayBits] = {};
		int32 Plans[(int32)LP::EPlan::Num] = {};
		for (const LP::FPerson& P : S->People)
		{
			for (int32 B = 0; B < LP::NumTodayBits; B++)
			{
				Bits[B] += (P.Today >> B) & 1;
			}
			Plans[(int32)P.PlanKind] += P.PlanAct >= 0 ? 1 : 0;
		}
		FString Line = FString::Printf(TEXT("TownDays: day %d (weekday %d, week %d):"), S->TodayDay, S->Weekday, S->Week);
		for (int32 B = 0; B < LP::NumTodayBits; B++)
		{
			Line += FString::Printf(TEXT(" %hs %d"), LP::TodayName(B), Bits[B]);
		}
		Line += TEXT("; in a plan:");
		for (int32 K = 1; K < (int32)LP::EPlan::Num; K++)
		{
			Line += FString::Printf(TEXT(" %hs %d"), LP::PlanName((LP::EPlan)K), Plans[K]);
		}
		Line += FString::Printf(TEXT("; on the way: %d stops for a word, %d greetings, %d doors held"), S->Meets, S->Nods, S->Holds);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Line);
		const int32 Near = NearestTownsperson(World, *S);
		if (Near >= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("TownDays: nearest %hs: %hs"), S->People[(size_t)Near].Key.c_str(), S->DescribeToday(Near).c_str());
		}
	}

	/** Everybody it fits gets this day now (the pension: the old who don't work; the pay: whoever has a duty). */
	void GiveAll(UWorld* World, uint8 Bit)
	{
		LP::FWorld* S = SimOf(World);
		if (!S)
		{
			return;
		}
		int32 Num = 0;
		for (LP::FPerson& P : S->People)
		{
			if (!P.bTown || P.StayCycle > 0 || P.Home < 0)
			{
				continue;
			}
			const bool bOld = P.Age >= 60 && (P.Duties.empty() || P.Occupation == "pensioner");
			if (Bit == LP::TodayPension ? bOld : (P.Age >= 18 && !P.Duties.empty() && !bOld))
			{
				S->SetToday(P.Id, (uint8)(P.Today | Bit));
				Num++;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("TownDays: %d people have %hs today"), Num, Bit == LP::TodayPension ? "the pension" : "the pay");
	}

	/** The nearest townsperson and a friend of theirs get a plan for now. */
	void PlanNearest(UWorld* World, LP::EPlan Kind)
	{
		LP::FWorld* S = SimOf(World);
		const int32 Near = S ? NearestTownsperson(World, *S) : -1;
		const int32 Friend = Near >= 0 ? FriendOf(*S, S->People[(size_t)Near]) : -1;
		const bool bOk = Friend >= 0 && S->MakePlan(Near, Friend, Kind, S->Hour + 0.4f);
		UE_LOG(LogTemp, Warning, TEXT("TownDays: %hs for %hs: %hs"), LP::PlanName(Kind), Near >= 0 ? S->People[(size_t)Near].Key.c_str() : "nobody near",
			bOk ? S->DescribeToday(Near).c_str() : "can't (no friend elsewhere in the town, or no place for it)");
	}
}

static FAutoConsoleCommandWithWorldAndArgs GPeopleToday(TEXT("lyud.People.Today"),
	TEXT("lyud.People.Today [key [cold|sleepy|birthday|payday|pension|guest|clear]]: what today is for somebody and their plan (docs/town_life.md \"The days\"); with a word, gives them that day. No key: the town's count."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		using namespace LyudTownDaysLocal;
		const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(World);
		LP::FWorld* S = SimOf(World);
		if (!People || !S)
		{
			return;
		}
		if (Args.Num() == 0)
		{
			Report(World);
			return;
		}
		const int32 Id = People->FindPerson(Args[0]);
		if (Id < 0 || Id >= (int32)S->People.size())
		{
			UE_LOG(LogTemp, Warning, TEXT("TownDays: nobody called %s"), *Args[0]);
			return;
		}
		if (Args.Num() > 1)
		{
			const uint8 Bit = LP::TodayFromName(TCHAR_TO_UTF8(*Args[1]));
			S->SetToday(Id, Args[1] == TEXT("clear") ? (uint8)0 : (uint8)(S->People[(size_t)Id].Today | Bit));
		}
		UE_LOG(LogTemp, Warning, TEXT("TownDays: %s: %hs"), *Args[0], S->DescribeToday(Id).c_str());
	}));

static FAutoConsoleCommandWithWorldAndArgs GPeoplePlan(TEXT("lyud.People.Plan"),
	TEXT("lyud.People.Plan <key> <with> <tea|birthday|family|stroll|chess>: a plan for now (tea, birthday, family: key goes to with's flat)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		using namespace LyudTownDaysLocal;
		const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(World);
		LP::FWorld* S = SimOf(World);
		if (!People || !S || Args.Num() < 3)
		{
			return;
		}
		LP::EPlan Kind = LP::EPlan::None;
		for (int32 K = 1; K < (int32)LP::EPlan::Num; K++)
		{
			if (Args[2] == FString(UTF8_TO_TCHAR(LP::PlanName((LP::EPlan)K))))
			{
				Kind = (LP::EPlan)K;
			}
		}
		const int32 Id = People->FindPerson(Args[0]);
		const bool bOk = Kind != LP::EPlan::None && S->MakePlan(Id, People->FindPerson(Args[1]), Kind, S->Hour + 0.4f);
		UE_LOG(LogTemp, Warning, TEXT("TownDays: plan %s: %hs"), *Args[2], bOk ? S->DescribeToday(Id).c_str() : "can't");
	}));

#if __has_include("Dev/LyudDev.h")
namespace LyudTownDaysLocal
{
	LyudDev::FAutoAction GDevReport(TEXT("townlife.report"), TEXT("townlife"), TEXT("dev.action.townlife.report"), [](UWorld* W) { Report(W); });
	LyudDev::FAutoAction GDevPension(TEXT("townlife.pension"), TEXT("townlife"), TEXT("dev.action.townlife.pension"),
		[](UWorld* W) { GiveAll(W, LP::TodayPension); });
	LyudDev::FAutoAction GDevPayday(TEXT("townlife.payday"), TEXT("townlife"), TEXT("dev.action.townlife.payday"),
		[](UWorld* W) { GiveAll(W, LP::TodayPayday); });
	LyudDev::FAutoAction GDevTea(TEXT("townlife.tea"), TEXT("townlife"), TEXT("dev.action.townlife.tea"),
		[](UWorld* W) { PlanNearest(W, LP::EPlan::Tea); });
	LyudDev::FAutoAction GDevChess(TEXT("townlife.chess"), TEXT("townlife"), TEXT("dev.action.townlife.chess"),
		[](UWorld* W) { PlanNearest(W, LP::EPlan::Chess); });
	LyudDev::FAutoAction GDevCold(TEXT("townlife.cold"), TEXT("townlife"), TEXT("dev.action.townlife.cold"), [](UWorld* W)
	{
		LP::FWorld* S = SimOf(W);
		const int32 Near = S ? NearestTownsperson(W, *S) : -1;
		if (Near >= 0)
		{
			S->SetToday(Near, (uint8)(S->People[(size_t)Near].Today | LP::TodayCold));
			UE_LOG(LogTemp, Warning, TEXT("TownDays: %hs: %hs"), S->People[(size_t)Near].Key.c_str(), S->DescribeToday(Near).c_str());
		}
	});
}
#endif
