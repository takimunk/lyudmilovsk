#include "People/CounterService.h"

#include "CounterModel.h"
#include "PeopleSim.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "People/PeopleLife.h"
#include "People/TownTalk.h"
#include "UI/LyudGameScreen.h"
#if __has_include("Dev/LyudDev.h")
#include "Dev/LyudDev.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLyudCounter, Log, All);

namespace LyudCounterServiceImpl
{
	/** One buffer for the lines' states (game thread only): no allocation after the first look. */
	std::vector<int> GLine;

	FVector ToUE(const LyudPeople::V3& V) { return FVector(V.X, V.Y, V.Z) * 100.0; }
	LyudPeople::V3 ToSim(const FVector& V) { return LyudPeople::V3((float)(V.X / 100.0), (float)(V.Y / 100.0), (float)(V.Z / 100.0)); }

	FVector FeetOf(const AHeroCharacter* Hero)
	{
		const UCapsuleComponent* Capsule = Hero->GetCapsuleComponent();
		return Hero->GetActorLocation() - FVector(0.0, 0.0, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.0);
	}

	void Toast(const UObject* Context, const FText& Text)
	{
		if (ULyudGameScreen* Screen = ULyudGameScreen::Get(Context))
		{
			Screen->Toast(Text);
		}
	}

	constexpr float HoldEvery = 0.25f;      // s between two SetHeroAt
	constexpr float LookEvery = 0.5f;       // s: the one serving him looks at him again
	constexpr float LineEvery = 0.5f;       // s between two looks at the lines near him
	constexpr double LinesNearCm = 1500.0;  // the lines' barks are heard within this of their front
	constexpr float ServerCm = 350.f;       // whoever serves him stands within this of him
	constexpr double HandoverFor = 3.0;     // s after the shop shuts the handover still goes on the counter
	constexpr float StillCmS = 150.f;       // he stands in a line only walking slower than this
	constexpr double GreetAgain = 300.0;    // s: no second hello from the same person
}

namespace CounterSvc = LyudCounterServiceImpl;

ULyudCounterService* ULyudCounterService::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULyudCounterService>() : nullptr;
}

double ULyudCounterService::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

AActor* ULyudCounterService::ShownBody(int32 PersonId) const
{
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	AActor* Body = Life && PersonId >= 0 ? Life->GetShownBody(PersonId) : nullptr;
	return IsValid(Body) && !Body->IsHidden() ? Body : nullptr;
}

AActor* ULyudCounterService::BodyOf(int32 PersonId) const
{
	if (PersonId < 0)
	{
		return nullptr;
	}
	if (AActor* Body = ShownBody(PersonId))
	{
		return Body;
	}
	for (const FExtra& E : Extras)
	{
		if (E.Pid == PersonId && E.Body.IsValid() && !E.Body->IsHidden())
		{
			return E.Body.Get();
		}
	}
	return nullptr;
}

void ULyudCounterService::NoteGreeted(const AActor* Body)
{
	GreetedBody = Body;
	GreetedAt = Now();
}

void ULyudCounterService::RegisterBody(AActor* Body, int32 PersonId, bool bServer)
{
	if (!Body)
	{
		return;
	}
	Extras.RemoveAll([](const FExtra& E) { return !E.Body.IsValid(); });
	for (FExtra& E : Extras)
	{
		if (E.Body.Get() == Body)
		{
			E.Pid = PersonId;
			E.bServer = bServer;
			return;
		}
	}
	FExtra E;
	E.Body = Body;
	E.Pid = PersonId;
	E.bServer = bServer;
	Extras.Add(E);
}

void ULyudCounterService::UnregisterBody(AActor* Body)
{
	Extras.RemoveAll([Body](const FExtra& E) { return !E.Body.IsValid() || E.Body.Get() == Body; });
}

void ULyudCounterService::OpenedAt(const FString& InCounter, AActor* InServerBody, int32 ServerPid)
{
	PendingCounter = InCounter;
	PendingBody = InServerBody;
	PendingPid = ServerPid;
	PendingAt = Now();
}

void ULyudCounterService::AnnounceChange(int32 Spent, int32 Purse) const
{
	if (Spent <= 0)
	{
		return;
	}
	// the money counted: the note he gave and what came back
	const int32 Note = LyudCounter::PayWith(Spent, Purse);
	CounterSvc::Toast(this, Note > 0
		? LyudText::Get(TEXT("ui.counter.change"), { FStringFormatArg(Spent), FStringFormatArg(Note), FStringFormatArg(Note - Spent) })
		: LyudText::Get(TEXT("ui.counter.exact"), { FStringFormatArg(Spent) }));
}

void ULyudCounterService::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	AHeroCharacter* Hero = World ? Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	if (!Hero || !Life || !Life->GetSim())
	{
		return;
	}
	HoldClock += DeltaTime;
	if (HoldClock >= CounterSvc::HoldEvery)
	{
		HoldClock = 0.f;
		TickHold(Hero);
	}
	TickShop(Hero, DeltaTime);
	LineClock += DeltaTime;
	if (LineClock >= CounterSvc::LineEvery)
	{
		LineClock = 0.f;
		TickLines(Hero);
	}
}

void ULyudCounterService::TickHold(AHeroCharacter* Hero)
{
	LyudPeople::FWorld* Sim = UPeopleLifeSubsystem::Get(this)->GetSim();
	const bool bOn = !Hero->IsHidden() && !Hero->IsOnBike() && Hero->GetVelocity().Size2D() < CounterSvc::StillCmS;
	if (bOn || bHeldOn)
	{
		Sim->SetHeroAt(CounterSvc::ToSim(CounterSvc::FeetOf(Hero)), bOn);
	}
	bHeldOn = bOn;
}

int32 ULyudCounterService::FindServer(const FVector& Near, float MaxCm, AActor*& OutBody) const
{
	const LyudPeople::FWorld* Sim = UPeopleLifeSubsystem::Get(this)->GetSim();
	int32 Best = -1;
	OutBody = nullptr;
	double BestD = (double)MaxCm * MaxCm;
	for (const FExtra& E : Extras)
	{
		const double D = E.bServer && E.Body.IsValid() && !E.Body->IsHidden() ? FVector::DistSquared2D(E.Body->GetActorLocation(), Near) : 1e30;
		if (D < BestD)
		{
			BestD = D;
			Best = E.Pid;
			OutBody = E.Body.Get();
		}
	}
	for (const LyudPeople::FPerson& P : Sim->People)
	{
		if (P.State != LyudPeople::EState::Doing || P.Activity < 0 || !Sim->Activities[(size_t)P.Activity].bStaff)
		{
			continue;
		}
		const double D = FVector::DistSquared2D(CounterSvc::ToUE(P.Pos), Near);
		AActor* Body = D < BestD ? ShownBody(P.Id) : nullptr;
		if (Body)
		{
			BestD = D;
			Best = P.Id;
			OutBody = Body;
		}
	}
	return Best;
}

void ULyudCounterService::TickShop(AHeroCharacter* Hero, float DeltaTime)
{
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	if (!UI || !Inv)
	{
		return;
	}
	const bool bShop = UI->IsShopOpen();
	if (bShop && !bShopWas)
	{
		// the shop opens: whoever is on shift by him turns to him and nods (and says hello, unless they just did); a hall
		// that opened it named its counter and its server just before
		AActor* Body = nullptr;
		if (Now() - PendingAt < 2.0 && PendingBody.IsValid())
		{
			Server = PendingPid;
			Body = PendingBody.Get();
			Counter = PendingCounter;
		}
		else
		{
			Server = FindServer(Hero->GetActorLocation(), CounterSvc::ServerCm, Body);
			Counter.Reset();
		}
		PendingAt = -100.0;
		ServerBody = Body;
		RublesAtOpen = Inv->Rubles;
		LookClock = CounterSvc::LookEvery;
		if (Body)
		{
			Life->ReactManner(Server, TEXT("nod"), 1.5f);
			const bool bGreeted = GreetedBody.Get() == Body && Now() - GreetedAt < CounterSvc::GreetAgain;
			ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this);
			if (!bGreeted && Talk && Talk->SayEvent(Body, TEXT("counter_hello"), Server))
			{
				NoteGreeted(Body);
			}
		}
	}
	if (bShop && ServerBody.IsValid())
	{
		LookClock += DeltaTime;
		if (LookClock >= CounterSvc::LookEvery)
		{
			LookClock = 0.f;
			if (ANpcBody* Body = Cast<ANpcBody>(ServerBody.Get()))
			{
				Body->LookAtPoint(Hero->EyeLocation(), CounterSvc::LookEvery * 2.f);
			}
		}
	}
	if (!bShop && bShopWas)
	{
		ClosedAt = Now();
		const int32 Spent = RublesAtOpen - Inv->Rubles;
		if (Spent > 0)
		{
			AnnounceChange(Spent, RublesAtOpen);
			if (AActor* Body = ServerBody.Get())
			{
				Life->ReactManner(Server, TEXT("point"), 1.5f);
				if (ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this))
				{
					Talk->SayEvent(Body, TEXT("counter_give"), Server);
				}
			}
		}
	}
	if (!bShop && Now() - ClosedAt > CounterSvc::HandoverFor)
	{
		Counter.Reset();
	}
	bShopWas = bShop;
}

bool ULyudCounterService::HandoverPoint(int32 Index, FVector& Out) const
{
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	// bShopWas: the shop shut this very frame and our tick has not seen it yet
	if (!UI || (!UI->IsShopOpen() && !bShopWas && Now() - ClosedAt > CounterSvc::HandoverFor))
	{
		return false;
	}
	return HandoverPointFrom(ServerBody.Get(), Index, Out);
}

bool ULyudCounterService::HandoverPointFrom(const AActor* Body, int32 Index, FVector& Out) const
{
	const UWorld* World = GetWorld();
	const AHeroCharacter* Hero = World ? Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
	if (!Hero || !IsValid(Body) || Body->IsHidden())
	{
		return false;
	}
	const FVector Feet = CounterSvc::FeetOf(Hero);
	FVector To = Body->GetActorLocation() - Feet;
	To.Z = 0.0;
	const double Dist = To.Size();
	if (Dist < 30.0 || Dist > CounterSvc::ServerCm + 50.0)
	{
		return false;
	}
	To /= Dist;
	const FVector Side(-To.Y, To.X, 0.0);
	static const double Sideways[] = { 0.0, 14.0, -14.0, 28.0 };
	// on the counter's top between them (a counter is about a metre high), never past the hero's reach
	Out = Feet + To * FMath::Min(Dist * 0.5, 55.0) + Side * Sideways[FMath::Clamp(Index, 0, 3)] + FVector(0.0, 0.0, 100.0);
	return true;
}

void ULyudCounterService::TickLines(AHeroCharacter* Hero)
{
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LyudPeople::FWorld* Sim = Life->GetSim();
	const int32 NumLines = (int32)Sim->Queues.size();
	if (LineStart.Num() != NumLines + 1)
	{
		LineStart.SetNum(NumLines + 1);
		int32 N = 0;
		for (int32 Q = 0; Q < NumLines; Q++)
		{
			LineStart[Q] = N;
			N += (int32)Sim->Queues[(size_t)Q].size();
		}
		LineStart[NumLines] = N;
		LastAt.Init(-1, N);
		LastFront.Init(-1, NumLines);
	}
	const FVector At = Hero->GetActorLocation();
	const double T = Now();
	int32 InLine = -1;
	for (int32 Q = 0; Q < NumLines; Q++)
	{
		const LyudPeople::FSlot& Front = Sim->Slots[(size_t)Sim->Queues[(size_t)Q][0]];
		const bool bNear = FVector::DistSquared(CounterSvc::ToUE(Front.Pos), At) < CounterSvc::LinesNearCm * CounterSvc::LinesNearCm;
		if (!bNear && LastFront[Q] == -1)
		{
			continue;
		}
		Sim->QueueState(Q, CounterSvc::GLine);
		const int32 Mine = LyudCounter::HeroSpot(CounterSvc::GLine);
		if (Mine >= 0)
		{
			InLine = Q;
			if (HeroLine != Q && Mine > 0)
			{
				const int32 Ahead = LyudCounter::Ahead(CounterSvc::GLine, Mine);
				if (Ahead > 0)
				{
					CounterSvc::Toast(this, LyudText::Get(TEXT("ui.counter.ahead"), { FStringFormatArg(Ahead) }));
				}
			}
		}
		const int32 Who = CounterSvc::GLine.empty() ? -1 : CounterSvc::GLine[0];
		if (bNear && Who >= 0 && LastFront[Q] != Who && LastFront[Q] != -1 && T >= NextNextBark)
		{
			// the front changed: the one on shift calls the next
			AActor* StaffBody = nullptr;
			const int32 Staff = FindServer(CounterSvc::ToUE(Front.Pos), 400.f, StaffBody);
			ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this);
			if (StaffBody && Talk && Talk->SayEvent(StaffBody, TEXT("queue_next"), Staff))
			{
				NextNextBark = T + 8.0;
			}
		}
		LastFront[Q] = bNear ? (Who >= 0 ? Who : -3) : -1;
		for (int32 N = 0; N < (int32)CounterSvc::GLine.size(); N++)
		{
			const int32 Person = CounterSvc::GLine[(size_t)N];
			bool bWasThere = false;
			for (int32 K = LineStart[Q]; K < LineStart[Q + 1]; K++)
			{
				bWasThere |= LastAt[K] == Person;
			}
			// somebody new at the back of a line with somebody before them asks who is last (one in two)
			if (bNear && N > 0 && Person >= 0 && !bWasThere && LyudCounter::Ahead(CounterSvc::GLine, N) > 0 && T >= NextJoinBark &&
				((uint32)Person * 2654435761u) % 2u == 0u)
			{
				ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this);
				if (Talk && Talk->SayEvent(BodyOf(Person), TEXT("queue_join"), Person))
				{
					NextJoinBark = T + 10.0;
				}
			}
		}
		for (int32 N = 0; N < LineStart[Q + 1] - LineStart[Q]; N++)
		{
			LastAt[LineStart[Q] + N] = bNear && N < (int32)CounterSvc::GLine.size() ? CounterSvc::GLine[(size_t)N] : -1;
		}
	}
	HeroLine = InLine;
}

FString ULyudCounterService::Status() const
{
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LyudPeople::FWorld* Sim = Life ? Life->GetSim() : nullptr;
	const UWorld* World = GetWorld();
	const APawn* Hero = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	if (!Sim || !Hero)
	{
		return TEXT("Counter: no people's life or no hero");
	}
	FString Out = FString::Printf(TEXT("Counter: %d lines, %d steps up so far; server %d (%s), counter %s, rubles at open %d, %d registered bodies\n"),
		(int32)Sim->Queues.size(), Sim->QueueMoves, Server, ServerBody.IsValid() ? *ServerBody->GetName() : TEXT("-"), Counter.IsEmpty() ? TEXT("-") : *Counter,
		RublesAtOpen, Extras.Num());
	for (int32 Q = 0; Q < (int32)Sim->Queues.size(); Q++)
	{
		const LyudPeople::FSlot& Front = Sim->Slots[(size_t)Sim->Queues[(size_t)Q][0]];
		const double Dist = FVector::Dist(CounterSvc::ToUE(Front.Pos), Hero->GetActorLocation());
		if (Dist > 5000.0)
		{
			continue;
		}
		Sim->QueueState(Q, CounterSvc::GLine);
		FString Spots;
		for (const int Who : CounterSvc::GLine)
		{
			if (Who == -2)
			{
				Spots += TEXT(" [hero]");
			}
			else if (Who < 0)
			{
				Spots += TEXT(" .");
			}
			else
			{
				Spots += TEXT(" ") + FString(UTF8_TO_TCHAR(Sim->People[(size_t)Who].Key.c_str()));
			}
		}
		Out += FString::Printf(TEXT("  %s (%.0f m):%s\n"), UTF8_TO_TCHAR(Sim->QueueIds[(size_t)Q].c_str()), Dist / 100.0, *Spots);
	}
	return Out;
}

int32 ULyudCounterService::Crowd(int32 Count)
{
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	LyudPeople::FWorld* Sim = Life ? Life->GetSim() : nullptr;
	const UWorld* World = GetWorld();
	const APawn* Hero = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	if (!Sim || !Hero)
	{
		return 0;
	}
	const int Q = Sim->NearestQueue(CounterSvc::ToSim(Hero->GetActorLocation()), 40.f);
	const int32 Sent = Q >= 0 ? Sim->SendToQueue(Q, FMath::Clamp(Count, 1, 6)) : 0;
	UE_LOG(LogLyudCounter, Display, TEXT("Counter: %d sent to the line %s"), Sent, Q >= 0 ? UTF8_TO_TCHAR(Sim->QueueIds[(size_t)Q].c_str()) : TEXT("(none within 40 m)"));
	return Sent;
}

namespace
{
	ULyudCounterService* CounterOf(UWorld* W) { return ULyudCounterService::Get(W); }

	FAutoConsoleCommandWithWorldAndArgs GCounterCmd(TEXT("lyud.Counter"),
		TEXT("lyud.Counter status | crowd [n]: the lines near the hero; n townsfolk (3) sent to the nearest line (docs/counters.md)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			ULyudCounterService* C = CounterOf(W);
			if (!C)
			{
				return;
			}
			if (Args.Num() > 0 && Args[0] == TEXT("crowd"))
			{
				C->Crowd(Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 3);
				return;
			}
			UE_LOG(LogLyudCounter, Display, TEXT("%s"), *C->Status());
		}));

#if __has_include("Dev/LyudDev.h")
	LyudDev::FAutoAction GDevCrowd(TEXT("counters.crowd"), TEXT("counters"), TEXT("dev.action.counters.crowd"),
		[](UWorld* W) { if (ULyudCounterService* C = CounterOf(W)) { C->Crowd(3); } });
	LyudDev::FAutoAction GDevStatus(TEXT("counters.status"), TEXT("counters"), TEXT("dev.action.counters.status"),
		[](UWorld* W) { if (ULyudCounterService* C = CounterOf(W)) { UE_LOG(LogLyudCounter, Display, TEXT("%s"), *C->Status()); } });
#endif
}
