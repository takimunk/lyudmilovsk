// The yacht school for the developer: the console's lyud.Sail.* commands and the F7 panel's group "sailing"
// (the dev-mode skill; docs/yacht_school.md "Engine: the school"). Everything here is a shortcut round the school's own
// gates (the season, the hour, the weather, the fees): nothing of it is reachable in play.
#include "Sailing/LyudSailing.h"

#include "LyudSailingState.h"

#include "Dev/LyudDev.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "Sailing/LyudDinghy.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudSailDev, Log, All);

using namespace LyudSail;

namespace LyudSailingDevImpl
{
	/** "optimist" / "cadet" / "luch" or 0 / 1 / 2; the hero's own class when it is neither. */
	int32 ClassOf(const FString& Word, int32 Default)
	{
		if (Word.IsEmpty())
		{
			return Default;
		}
		for (int32 C = 0; C < SchoolBoats; C++)
		{
			if (Word.Equals(UTF8_TO_TCHAR(SchoolBoatId(C)), ESearchCase::IgnoreCase))
			{
				return C;
			}
		}
		return Word.IsNumeric() ? FMath::Clamp(FCString::Atoi(*Word), 0, SchoolBoats - 1) : Default;
	}

	void SetVar(const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			V->Set(Value, ECVF_SetByConsole);
		}
	}

	float GetVar(const TCHAR* Name, float Default)
	{
		const IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name);
		return V ? V->GetFloat() : Default;
	}

	/** The developer's session that stands in for the timetable's: everybody of the group (a race: of the school) may go. */
	FSchoolNow Session(const FLyudSailingState& S, bool bRace, const std::string& Group)
	{
		FSchoolNow Now;
		Now.bOn = true;
		Now.Key = S.Day * 16 + 15;
		Now.Group = bRace ? std::string("race") : Group;
		Now.Kind = bRace ? "race" : "lesson";
		Now.Phase = "water";
		Now.Activity = "sail";
		Now.Drill = bRace ? std::string() : std::string("triangle");
		Now.bAfloat = true;
		if (const FSchoolGroup* G = S.P.Group(Now.Group))
		{
			Now.Coach = G->Coach;
		}
		const auto Areas = S.P.GroupArea.find(Now.Group);
		Now.Area = Areas != S.P.GroupArea.end() && !Areas->second.empty() ? Areas->second.front() : std::string("race");
		FTimetableSession& T = Now.Session;
		T.Key = Now.Key;
		T.Group = Now.Group;
		T.Coach = Now.Coach;
		T.Kind = Now.Kind;
		T.Races = bRace ? 1 : 0;
		T.From = 0.0;
		T.To = 48.0;
		T.bAfloat = true;
		T.WaterFrom = 0.0;
		T.WaterTo = 48.0;
		T.Classes = 0;
		for (const FSchoolSailor& Sailor : S.P.Sailors)
		{
			if (Sailor.Class >= 0 && Sailor.Class < SchoolBoats && (bRace || Sailor.Group == Group))
			{
				T.Classes |= 1 << Sailor.Class;
			}
		}
		return Now;
	}
}

// ------------------------------------------------------------------ the subsystem's part

void ULyudSailing::DevSeason(int32 Mode)
{
	if (!M || !bLoaded)
	{
		return;
	}
	M->ForcedSeason = FMath::Clamp(Mode, -1, 1);
	FactsIn = 0.f;
	ConditionsIn = 0.f;
	UE_LOG(LogLyudSailDev, Display, TEXT("SAIL season: %s"), M->ForcedSeason < 0 ? TEXT("the calendar's") : (M->ForcedSeason == 1 ? TEXT("open (a July day)") : TEXT("laid up (a January day)")));
}

int32 ULyudSailing::DevSeasonNow() const
{
	return M ? M->ForcedSeason : -1;
}

void ULyudSailing::DevBoard(int32 BoatClass)
{
	if (!M || !bLoaded || bSwitchedOff)
	{
		return;
	}
	FLyudSailingState& S = *M;
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (!Hero || !MakeFleet() || !S.Lp.bOk)
	{
		UE_LOG(LogLyudSailDev, Warning, TEXT("SAIL board: no hero, no fleet or no water at the slip (the school's facts come two seconds after the world begins)"));
		return;
	}
	if (S.Hero.bBoatOut)
	{
		HeroAshore(false);
	}
	// his unit takes a hull of the class asked for (his own number nine when it is a Luch)
	const int32 Class = FMath::Clamp(BoatClass, 0, SchoolBoats - 1);
	int32 Want = -1;
	for (size_t I = 0; I < S.Hulls.size(); I++)
	{
		const FLyudSailHull& Hull = S.Hulls[I];
		if (Hull.BoatClass != Class || Hull.bAfloat || (Hull.Unit > 0) || !Dinghies[(int32)I])
		{
			continue;
		}
		if (Want < 0 || S.P.BoatSlots[(size_t)Hull.Slot].Id == S.P.HeroBoat)
		{
			Want = (int32)I;
		}
	}
	if (Want < 0)
	{
		UE_LOG(LogLyudSailDev, Warning, TEXT("SAIL board: no free boat of the class %s"), UTF8_TO_TCHAR(SchoolBoatId(Class)));
		return;
	}
	if (Want != S.HeroHull)
	{
		if (S.HeroHull >= 0)
		{
			S.Hulls[(size_t)S.HeroHull].Unit = -1;
			S.Hulls[(size_t)S.HeroHull].Shown = -1;
		}
		S.HeroHull = Want;
		S.Hulls[(size_t)Want].Unit = 0;
		S.UnitHull[0] = Want;
		S.Brain.Units[0].Boat = &Dinghies[Want]->Boat();
	}
	if (!PutHeroBoatOut(false))
	{
		return;
	}
	if (ALyudDinghy* Boat = HeroDinghy())
	{
		Boat->Board(Hero, false);
	}
	ParkHulls(false);
}

bool ULyudSailing::DevLesson(const FString& Id)
{
	if (!M || !bLoaded || !M->bFacts || bSwitchedOff)
	{
		return false;
	}
	FLyudSailingState& S = *M;
	int32 Lesson = S.P.Lesson(LyudSailingImpl::Std(Id));
	if (Id.IsEmpty() || Id == TEXT("next"))
	{
		Lesson = SchoolNextLesson(S.P, S.Journal);
		if (Lesson < 0)
		{
			Lesson = (int32)S.P.Lessons.size() - 1;
		}
	}
	if (Lesson < 0)
	{
		UE_LOG(LogLyudSailDev, Warning, TEXT("SAIL lesson: no lesson %s"), *Id);
		return false;
	}
	if (!S.Journal.bMember)
	{
		Emit(SchoolJoin(S.P, S.Journal, S.Date));
	}
	if (S.Hero.bLesson)
	{
		EndLesson(false);
	}
	if (!StartLesson(Lesson))
	{
		UE_LOG(LogLyudSailDev, Warning, TEXT("SAIL lesson: %s cannot begin (no fleet yet, or no water at the slip)"), *LyudSailingImpl::Str(S.P.Lessons[(size_t)Lesson].Id));
		return false;
	}
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	ALyudDinghy* Boat = S.P.Lessons[(size_t)Lesson].bAfloat ? HeroDinghy() : nullptr;
	if (Boat && Hero && !Boat->IsHeroAboard())
	{
		Boat->Board(Hero, false);
	}
	return true;
}

void ULyudSailing::DevFleet(int32 N)
{
	if (!M || !bLoaded || !M->bFacts || bSwitchedOff)
	{
		return;
	}
	FLyudSailingState& S = *M;
	S.DevFleet = N;
	if (N <= 0)
	{
		// everybody home (0 keeps them ashore until another number is given; a negative one gives the timetable back)
		if (S.bDevWater)
		{
			S.DevNow.Session.WaterTo = 0.0;
			S.Now = S.DevNow;
		}
		else if (S.WaterKey != 0 && !S.bHomeCalled)
		{
			S.bHomeCalled = true;
			S.Brain.Recall(false);
		}
		UE_LOG(LogLyudSailDev, Display, TEXT("SAIL fleet: %s"), N == 0 ? TEXT("everybody ashore") : TEXT("as the timetable says"));
		return;
	}
	if (S.WaterKey != 0 || S.bDevWater)
	{
		UE_LOG(LogLyudSailDev, Display, TEXT("SAIL fleet: boats are on the water already; at most %d go out the next time"), N);
		return;
	}
	if (!MakeFleet() || !S.Lp.bOk)
	{
		UE_LOG(LogLyudSailDev, Warning, TEXT("SAIL fleet: no fleet yet or no water at the slip"));
		return;
	}
	// the group of the hour, else the one with the most boats that fits the number
	std::string Group = S.Now.bOn && S.Now.Group != "race" && !S.Now.Group.empty() ? S.Now.Group : std::string(N <= 8 ? "juniors" : "");
	S.DevNow = LyudSailingDevImpl::Session(S, Group.empty(), Group);
	if (Group.empty())
	{
		// more boats than a group has: the whole school out for a practice
		S.DevNow.Kind = "lesson";
		S.DevNow.Session.Kind = "lesson";
		S.DevNow.Session.Races = 0;
		S.DevNow.Drill = "triangle";
	}
	FSchoolSessionState St;
	St.Key = S.DevNow.Key;
	S.States.push_back(St);
	FSchoolGo Go;
	Go.Go = ESchoolGo::Sail;
	for (int C = 0; C < SchoolBoats; C++)
	{
		Go.ClassOk[C] = true;
	}
	Go.Launch = S.Lp;
	S.Decisions[St.Key] = Go;
	S.bDevWater = true;
	S.Now = S.DevNow;
	UE_LOG(LogLyudSailDev, Display, TEXT("SAIL fleet: up to %d boats go out now (%s)"), N, *LyudSailingImpl::Str(S.DevNow.Group));
}

void ULyudSailing::DevRace(const FString& CourseKind)
{
	if (!M || !bLoaded || !M->bFacts || bSwitchedOff)
	{
		return;
	}
	FLyudSailingState& S = *M;
	const FString Kind = CourseKind.ToLower();
	S.DevCourse = Kind == TEXT("triangle") ? "triangle" : (Kind == TEXT("kids") ? "kids_triangle" : (Kind == TEXT("wl") ? "windward_leeward" : ""));
	if (!MakeFleet() || !S.Lp.bOk)
	{
		UE_LOG(LogLyudSailDev, Warning, TEXT("SAIL race: no fleet yet or no water at the slip"));
		return;
	}
	const bool bHeroIn = S.Race.bHeroIn || (S.Hero.bBoatOut && !S.Hero.bLeftElsewhere);
	if (S.WaterKey != 0)
	{
		if (S.bWaterRace && S.Race.Stage == 1)
		{
			UE_LOG(LogLyudSailDev, Display, TEXT("SAIL race: a race is on already"));
			return;
		}
		// the boats that are out race now
		S.Race = FLyudSailRaceRun();
		S.Race.bHeroIn = bHeroIn;
		S.Race.Wanted = 1;
		S.Race.Event = RaceDayOn(S.P, S.Date).Event;
		S.bWaterRace = true;
		if (!StartRace())
		{
			S.bWaterRace = false;
			S.Race.Stage = 0;
		}
		return;
	}
	if (S.bDevWater)
	{
		// asked a moment ago: they are on their way
		return;
	}
	if (S.DevFleet == 0)
	{
		S.DevFleet = -1;
	}
	S.Race = FLyudSailRaceRun();
	S.Race.bHeroIn = bHeroIn;
	S.DevNow = LyudSailingDevImpl::Session(S, true, std::string("race"));
	FSchoolSessionState St;
	St.Key = S.DevNow.Key;
	S.States.push_back(St);
	FSchoolGo Go;
	Go.Go = ESchoolGo::Sail;
	for (int C = 0; C < SchoolBoats; C++)
	{
		Go.ClassOk[C] = true;
	}
	Go.Launch = S.Lp;
	S.Decisions[St.Key] = Go;
	S.bDevWater = true;
	S.Now = S.DevNow;
	UE_LOG(LogLyudSailDev, Display, TEXT("SAIL race: the school goes out to race now%s"), bHeroIn ? TEXT(", the hero with it") : TEXT(""));
}

void ULyudSailing::DevCapsize()
{
	if (!M || !bLoaded || !bFleetMade)
	{
		return;
	}
	FLyudSailingState& S = *M;
	ALyudDinghy* Boat = S.Hero.bBoatOut && S.Hero.bAboard ? HeroDinghy() : nullptr;
	if (!Boat)
	{
		const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		const FVector Eye = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
		double Best = 0.0;
		for (size_t I = 0; I < S.Hulls.size(); I++)
		{
			ALyudDinghy* Other = Dinghies[(int32)I];
			if (!Other || !S.Hulls[I].bAfloat)
			{
				continue;
			}
			const double D = FVector::DistSquared2D(Other->GetActorLocation(), Eye);
			if (!Boat || D < Best)
			{
				Boat = Other;
				Best = D;
			}
		}
	}
	if (!Boat)
	{
		UE_LOG(LogLyudSailDev, Display, TEXT("SAIL capsize: no boat afloat"));
		return;
	}
	// pushed past the point of no return on the side she leans to: the model itself takes her over at its next step
	FBoat& B = Boat->Boat();
	const double Side = B.Heel >= 0.0 ? 1.0 : -1.0;
	B.Assist.bNoCapsize = false;
	B.Heel = Side * 1.35;
	B.HeelRate = Side * 1.0;
	UE_LOG(LogLyudSailDev, Display, TEXT("SAIL capsize: number %d goes over"), Boat->GetSailNumber());
}

void ULyudSailing::DevRank(const FString& Id)
{
	if (!M || !bLoaded)
	{
		return;
	}
	FLyudSailingState& S = *M;
	const int32 Rank = Id == TEXT("none") ? 0 : S.P.Rank(LyudSailingImpl::Std(Id));
	if (Rank < 0)
	{
		UE_LOG(LogLyudSailDev, Warning, TEXT("SAIL rank: no rank %s"), *Id);
		return;
	}
	FSchoolLog& Log = S.Journal;
	if (!Log.bMember)
	{
		Log.bMember = true;
		Log.JoinedDay = S.Day;
	}
	// the lessons, the races and the miles the rank stands for (nothing is taken away when it is a lower one but the rank)
	uint32_t Lessons = 0;
	for (int32 R = 1; R <= Rank && R < (int32)S.P.Ranks.size(); R++)
	{
		const FSchoolRankDef& Def = S.P.Ranks[(size_t)R];
		for (const std::string& L : Def.Lessons)
		{
			const int At = S.P.Lesson(L);
			if (At >= 0 && At < 32)
			{
				Lessons |= 1u << At;
			}
		}
		Log.Races = FMath::Max(Log.Races, Def.Races);
		Log.Podiums = FMath::Max(Log.Podiums, Def.Podiums);
		Log.MetresSailed = FMath::Max(Log.MetresSailed, Def.Miles * FMath::Max(1.0, S.P.MileMetres));
	}
	Log.LessonsDone = Rank == 0 ? 0u : (Log.LessonsDone | Lessons);
	Log.Rank = Rank;
	UE_LOG(LogLyudSailDev, Display, TEXT("SAIL rank: %s (lessons %u)"), Rank < (int32)S.P.Ranks.size() ? *LyudSailingImpl::Str(S.P.Ranks[(size_t)Rank].Id) : TEXT("?"), (uint32)Log.LessonsDone);
}

void ULyudSailing::GiveKit()
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Inv || !M || !bLoaded)
	{
		return;
	}
	const FName Card = LyudSailingImpl::Name(M->P.CardItem);
	for (const FName Item : { Card, FName(TEXT("life_jacket")), FName(TEXT("sail_gloves")), FName(TEXT("bailer")) })
	{
		if (!Item.IsNone() && Inv->Count(Item) == 0)
		{
			Inv->Add(Item);
		}
	}
	Inv->Wear(TEXT("life_jacket"));
}

void ULyudSailing::SetNightLeave(bool bOn)
{
	if (M)
	{
		M->Hero.bNightLeave = bOn;
	}
}

bool ULyudSailing::HasNightLeave() const
{
	return M && M->Hero.bNightLeave;
}

// ------------------------------------------------------------------ the console

namespace LyudSailingDevImpl
{
	FAutoConsoleCommandWithWorldAndArgs GSailReport(TEXT("lyud.Sail.Report"), TEXT("lyud.Sail.Report: what the yacht school is doing, to the log"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (const ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				UE_LOG(LogLyudSailDev, Display, TEXT("%s"), *Sail->Report());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GSailSeason(TEXT("lyud.Sail.Season"), TEXT("lyud.Sail.Season 0|1|-1: the school laid up / open whatever the date (a January / July day of the same weekday); -1 the calendar"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				Sail->DevSeason(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : (Sail->DevSeasonNow() == 1 ? -1 : 1));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GSailBoard(TEXT("lyud.Sail.Board"), TEXT("lyud.Sail.Board [optimist|cadet|luch]: the hero aboard a boat of the class off the slip, now"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				Sail->DevBoard(LyudSailingDevImpl::ClassOf(Args.Num() > 0 ? Args[0] : FString(), SchoolLuch));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GSailLesson(TEXT("lyud.Sail.Lesson"), TEXT("lyud.Sail.Lesson <id|next>: starts a lesson of the hero's course (shore, first_sail, tacking ...)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				Sail->DevLesson(Args.Num() > 0 ? Args[0] : FString(TEXT("next")));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GSailRace(TEXT("lyud.Sail.Race"), TEXT("lyud.Sail.Race [wl|triangle|kids]: the school goes out and races now (the hero too when his boat is afloat)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				Sail->DevRace(Args.Num() > 0 ? Args[0] : FString());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GSailFleet(TEXT("lyud.Sail.Fleet"), TEXT("lyud.Sail.Fleet <n>: a training fleet of up to n boats on the water now; 0 everybody ashore; -1 the timetable again"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				Sail->DevFleet(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 12);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GSailCapsize(TEXT("lyud.Sail.Capsize"), TEXT("lyud.Sail.Capsize: the hero's boat (else the nearest one afloat) goes over"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				Sail->DevCapsize();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GSailRank(TEXT("lyud.Sail.Rank"), TEXT("lyud.Sail.Rank <none|yunga|helm3|helm2|helm1>: the hero's rank, with the lessons it stands for"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				Sail->DevRank(Args.Num() > 0 ? Args[0] : FString(TEXT("helm3")));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GSailKit(TEXT("lyud.Sail.Kit"), TEXT("lyud.Sail.Kit: the member's card, a life jacket (put on), gloves and a bailer"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				Sail->GiveKit();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GSailNight(TEXT("lyud.Sail.Night"), TEXT("lyud.Sail.Night 0|1: the head coach's leave to take the hire boat out after dusk"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				Sail->SetNightLeave(Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !Sail->HasNightLeave());
				UE_LOG(LogLyudSailDev, Display, TEXT("SAIL night leave: %s"), Sail->HasNightLeave() ? TEXT("given") : TEXT("taken back"));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GSailOff(TEXT("lyud.Sail.Off"), TEXT("lyud.Sail.Off: the whole yacht school off / on again (the same as lyud.Sail 0|1)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			LyudSailingDevImpl::SetVar(TEXT("lyud.Sail"), LyudSailingDevImpl::GetVar(TEXT("lyud.Sail"), 1.f) > 0.5f ? 0.f : 1.f);
		}));

	// ---- developer mode (F7, Actions), group "sailing"

	LyudDev::FAutoAction GSailDevReport(TEXT("sailing.report"), TEXT("sailing"), TEXT("dev.action.sailing.report"),
		[](UWorld* World)
		{
			if (const ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				UE_LOG(LogLyudSailDev, Display, TEXT("%s"), *Sail->Report());
			}
		});

	LyudDev::FAutoAction GSailDevSeason(TEXT("sailing.season"), TEXT("sailing"), TEXT("dev.action.sailing.season"),
		[](UWorld* World)
		{
			if (ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				Sail->DevSeason(Sail->DevSeasonNow() == 1 ? -1 : 1);
			}
		},
		[](UWorld* World)
		{
			const ULyudSailing* Sail = ULyudSailing::Get(World);
			return Sail && Sail->DevSeasonNow() == 1;
		});

	LyudDev::FAutoAction GSailDevBoard(TEXT("sailing.board"), TEXT("sailing"), TEXT("dev.action.sailing.board"),
		[](UWorld* World)
		{
			if (ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				Sail->DevBoard(SchoolLuch);
			}
		});

	LyudDev::FAutoAction GSailDevLesson(TEXT("sailing.lesson"), TEXT("sailing"), TEXT("dev.action.sailing.lesson"),
		[](UWorld* World)
		{
			if (ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				Sail->DevLesson(TEXT("next"));
			}
		});

	LyudDev::FAutoAction GSailDevRace(TEXT("sailing.race"), TEXT("sailing"), TEXT("dev.action.sailing.race"),
		[](UWorld* World)
		{
			if (ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				Sail->DevRace(FString());
			}
		});

	LyudDev::FAutoAction GSailDevFleet(TEXT("sailing.fleet"), TEXT("sailing"), TEXT("dev.action.sailing.fleet"),
		[](UWorld* World)
		{
			if (ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				Sail->DevFleet(12);
			}
		});

	LyudDev::FAutoAction GSailDevCapsize(TEXT("sailing.capsize"), TEXT("sailing"), TEXT("dev.action.sailing.capsize"),
		[](UWorld* World)
		{
			if (ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				Sail->DevCapsize();
			}
		});

	LyudDev::FAutoAction GSailDevRank(TEXT("sailing.rank"), TEXT("sailing"), TEXT("dev.action.sailing.rank"),
		[](UWorld* World)
		{
			if (ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				Sail->DevRank(TEXT("helm3"));
			}
		});

	LyudDev::FAutoAction GSailDevKit(TEXT("sailing.kit"), TEXT("sailing"), TEXT("dev.action.sailing.kit"),
		[](UWorld* World)
		{
			if (ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				Sail->GiveKit();
			}
		});

	// a sailing breeze from the land's side of the pier: 6 m/s from the east (again: the weather's own wind back)
	LyudDev::FAutoAction GSailDevWind(TEXT("sailing.wind"), TEXT("sailing"), TEXT("dev.action.sailing.wind"),
		[](UWorld* World)
		{
			const bool bOn = LyudSailingDevImpl::GetVar(TEXT("lyud.Wind.Speed"), -1.f) >= 0.f;
			LyudSailingDevImpl::SetVar(TEXT("lyud.Wind.Speed"), bOn ? -1.f : 6.f);
			LyudSailingDevImpl::SetVar(TEXT("lyud.Wind.From"), bOn ? -1.f : 90.f);
		},
		[](UWorld* World) { return LyudSailingDevImpl::GetVar(TEXT("lyud.Wind.Speed"), -1.f) >= 0.f; });

	// the boat part's help at the helm: everything automatic and no capsize (again: the default)
	LyudDev::FAutoAction GSailDevEasy(TEXT("sailing.easy"), TEXT("sailing"), TEXT("dev.action.sailing.easy"),
		[](UWorld* World)
		{
			LyudSailingDevImpl::SetVar(TEXT("lyud.Sail.Easy"), LyudSailingDevImpl::GetVar(TEXT("lyud.Sail.Easy"), 1.f) > 1.5f ? 1.f : 2.f);
		},
		[](UWorld* World) { return LyudSailingDevImpl::GetVar(TEXT("lyud.Sail.Easy"), 1.f) > 1.5f; });

	LyudDev::FAutoAction GSailDevNight(TEXT("sailing.night"), TEXT("sailing"), TEXT("dev.action.sailing.night"),
		[](UWorld* World)
		{
			if (ULyudSailing* Sail = ULyudSailing::Get(World))
			{
				Sail->SetNightLeave(!Sail->HasNightLeave());
			}
		},
		[](UWorld* World)
		{
			const ULyudSailing* Sail = ULyudSailing::Get(World);
			return Sail && Sail->HasNightLeave();
		});

	LyudDev::FAutoAction GSailDevOff(TEXT("sailing.off"), TEXT("sailing"), TEXT("dev.action.sailing.off"),
		[](UWorld* World)
		{
			LyudSailingDevImpl::SetVar(TEXT("lyud.Sail"), LyudSailingDevImpl::GetVar(TEXT("lyud.Sail"), 1.f) > 0.5f ? 0.f : 1.f);
		},
		[](UWorld* World) { return LyudSailingDevImpl::GetVar(TEXT("lyud.Sail"), 1.f) < 0.5f; });
}
