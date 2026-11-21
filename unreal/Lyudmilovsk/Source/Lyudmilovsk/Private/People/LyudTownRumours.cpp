#include "People/LyudTownRumours.h"

#include "TownRumours.h"
#include "TownBarks.h"
#include "PeopleSim.h"
#include "People/PeopleLife.h"
#include "LyudText.h"
#include "Plants/GameCalendar.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#if __has_include("Dev/LyudDev.h")
#include "Dev/LyudDev.h"
#define LYUD_RUMOURS_DEV 1
#else
#define LYUD_RUMOURS_DEV 0
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLyudRumours, Log, All);

namespace LyudTownRumoursImpl
{
	FString Str(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
	std::string Std(const FString& S)
	{
		const FTCHARToUTF8 Conv(*S);
		return std::string(Conv.Get(), (size_t)Conv.Length());
	}

	/** Days since 1970-01-01 (the letters' and the friends' calendar: the same numbers). */
	int32 DaysOf(const FDateTime& T)
	{
		return (int32)((T.GetDate() - FDateTime(1970, 1, 1)).GetTotalDays());
	}

	/** "npc.<key>.name" -> <key> (a resident named in a rumour: the regular beaten), else empty. */
	std::string PersonKeyOf(const std::string& Detail)
	{
		static const std::string Head = "npc.", Tail = ".name";
		if (Detail.size() > Head.size() + Tail.size() && Detail.compare(0, Head.size(), Head) == 0
			&& Detail.compare(Detail.size() - Tail.size(), Tail.size(), Tail) == 0)
		{
			return Detail.substr(Head.size(), Detail.size() - Head.size() - Tail.size());
		}
		return std::string();
	}
}

ULyudTownRumours::ULyudTownRumours() = default;
ULyudTownRumours::~ULyudTownRumours() = default;

ULyudTownRumours* ULyudTownRumours::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudTownRumours>() : nullptr;
}

void ULyudTownRumours::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Town = MakePimpl<LyudRumour::FTown>();
	Ties = MakePimpl<LyudRumour::FTies>();
	LoadData();
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("TownRumours"), this);
		Saves->OnNewGame.AddWeakLambda(this, [this]()
		{
			ClearAll();
		});
	}
}

void ULyudTownRumours::Deinitialize()
{
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(this))
	{
		Saves->UnregisterSaveObject(TEXT("TownRumours"));
	}
	Super::Deinitialize();
}

bool ULyudTownRumours::LoadData()
{
	FString Text;
	const FString File = FPaths::ProjectContentDir() / TEXT("PeopleData/rumours.json");
	if (!FFileHelper::LoadFileToString(Text, *File))
	{
		UE_LOG(LogLyudRumours, Warning, TEXT("TownRumours: %s not found: the town talks of nothing the hero does"), *File);
		return false;
	}
	if (!Town->Load(LyudTownRumoursImpl::Std(Text)))
	{
		UE_LOG(LogLyudRumours, Warning, TEXT("TownRumours: %s: %s"), *File, *LyudTownRumoursImpl::Str(Town->LastError));
		return false;
	}
	WitnessCm = Town->WitnessM * 100.f;
	RemarkEvery = Town->RemarkEveryS;
	bLoaded = true;
	UE_LOG(LogLyudRumours, Display, TEXT("TownRumours: %d kinds, %d rules"), (int32)Town->Kinds.size(), (int32)Town->Rules.size());
	return true;
}

bool ULyudTownRumours::EnsureTies()
{
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LyudPeople::FWorld* S = Life ? Life->GetSim() : nullptr;
	if (!bLoaded || !S || S->People.empty())
	{
		return false;
	}
	if (TiesFor != (int32)S->People.size())
	{
		LyudRumour::BuildTies(*S, *Ties, Town->FarM, Town->BigGroup, Town->GroupLinks);
		TiesFor = (int32)S->People.size();
		// the rumours already going round spread over the new ties (their tellers by id stay: the same town)
		for (LyudRumour::FRumour& R : Town->Rumours)
		{
			Town->Spread(R, *Ties);
		}
		UE_LOG(LogLyudRumours, Display, TEXT("TownRumours: ties of %d people"), TiesFor);
	}
	if (bSavedPending)
	{
		bSavedPending = false;
		std::vector<std::string> Lines;
		for (const FString& L : Saved)
		{
			Lines.push_back(LyudTownRumoursImpl::Std(L));
		}
		Town->Load(Lines, *S, *Ties);
		Town->Expire(Today());
		UE_LOG(LogLyudRumours, Display, TEXT("TownRumours: %d rumours going round after the load"), (int32)Town->Rumours.size());
	}
	return true;
}

void ULyudTownRumours::ReadPending() const
{
	// the save is applied the tick after BeginPlay and the people's life starts half a second later: the store waits in
	// Saved till then, and is read by whoever asks first once the town's people are there
	if (bSavedPending)
	{
		const_cast<ULyudTownRumours*>(this)->EnsureTies();
	}
}

void ULyudTownRumours::OnGameSaving_Implementation()
{
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LyudPeople::FWorld* S = Life ? Life->GetSim() : nullptr;
	if (!bLoaded || !S || bSavedPending)
	{
		return;     // nothing read yet: the save keeps what it had
	}
	Saved.Reset();
	for (const std::string& L : Town->Save(*S))
	{
		Saved.Add(LyudTownRumoursImpl::Str(L));
	}
}

void ULyudTownRumours::OnGameLoaded_Implementation()
{
	if (Town)
	{
		Town->Rumours.clear();
		Town->Spent.clear();
	}
	bSavedPending = true;      // read when the town's people are there (EnsureTies)
	LastRemarkAt = -1e9;
	GoingRoundDay = -1;
	EnsureTies();
}

void ULyudTownRumours::ClearAll()
{
	if (Town)
	{
		Town->Rumours.clear();
		Town->Spent.clear();
	}
	Saved.Reset();
	bSavedPending = false;
	GoingRoundDay = -1;
}

int32 ULyudTownRumours::Today(int32* Minute) const
{
	const FDateTime N = UGameCalendar::NowIn(GetWorld());
	if (Minute)
	{
		*Minute = N.GetHour() * 60 + N.GetMinute();
	}
	return LyudTownRumoursImpl::DaysOf(N);
}

void ULyudTownRumours::OnGameEvent(const UObject* WorldContext, FName Event, const FString& Target)
{
	ULyudTownRumours* Self = Get(WorldContext);
	if (!Self || !Self->bLoaded)
	{
		return;
	}
	int Kind = -1;
	std::string Detail;
	if (Self->Town->Match(LyudTownRumoursImpl::Std(Event.ToString()), LyudTownRumoursImpl::Std(Target), Kind, Detail))
	{
		Self->Add(Kind, Detail, -1);
	}
}

bool ULyudTownRumours::AddNow(const FString& Kind, const FString& Detail)
{
	if (!bLoaded)
	{
		return false;
	}
	const int K = Town->FindKind(LyudTownRumoursImpl::Std(Kind));
	return K >= 0 && Add(K, LyudTownRumoursImpl::Std(Detail), -1) >= 0;
}

int32 ULyudTownRumours::Add(int32 Kind, const std::string& Detail, int32 Witness)
{
	if (Kind < 0 || Kind >= (int32)Town->Kinds.size())
	{
		return -1;
	}
	if (!EnsureTies())
	{
		UE_LOG(LogLyudRumours, Display, TEXT("TownRumours: no town's people yet: %s not news"), *LyudTownRumoursImpl::Str(Town->Kinds[(size_t)Kind].Id));
		return -1;
	}
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LyudPeople::FWorld& S = *Life->GetSim();
	// whoever is up and about close to the hero saw it; a resident it names (the regular beaten) knows it too
	std::vector<int> Witnesses;
	if (const APawn* Hero = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FVector At = Hero->GetActorLocation();
		for (const LyudPeople::FPerson& P : S.People)
		{
			const FVector Pos = FVector(P.Pos.X, P.Pos.Y, P.Pos.Z) * 100.0;
			if (FVector::Dist2D(Pos, At) <= WitnessCm && FMath::Abs(Pos.Z - At.Z) < 600.0 && LyudBarks::CanSpeak(S, P))
			{
				Witnesses.push_back(P.Id);
			}
		}
	}
	const std::string Named = LyudTownRumoursImpl::PersonKeyOf(Detail);
	if (!Named.empty())
	{
		Witness = S.FindPerson(Named);
	}
	if (Witness >= 0)
	{
		Witnesses.push_back(Witness);
	}
	int32 Minute = 0;
	const int32 Day = Today(&Minute);
	const int I = Town->Add(Kind, Detail, Day, Minute, Witnesses, *Ties);
	GoingRoundDay = -1;
	if (I >= 0)
	{
		const LyudRumour::FRumour& R = Town->Rumours[(size_t)I];
		UE_LOG(LogLyudRumours, Display, TEXT("TownRumours: %s %s: %d saw it, %d know it tomorrow, %d the day after"), *LyudTownRumoursImpl::Str(Town->Kinds[(size_t)Kind].Id),
			*LyudTownRumoursImpl::Str(Detail), (int32)R.Witnesses.size(), Town->CountKnowing(R, Day + 1), Town->CountKnowing(R, Day + 2));
	}
	return I;
}

bool ULyudTownRumours::RemarkReady() const
{
	ReadPending();
	const UWorld* World = GetWorld();
	return bLoaded && World && !Town->Rumours.empty() && World->GetTimeSeconds() - LastRemarkAt >= RemarkEvery;
}

void ULyudTownRumours::KindsFor(int32 Person, TArray<FString>& Out) const
{
	Out.Reset();
	if (!bLoaded || Town->Rumours.empty() || Person < 0 || Person >= TiesFor)
	{
		return;
	}
	Town->Mentionable(Person, Today(), Scratch);
	for (const int I : Scratch)
	{
		Out.AddUnique(LyudTownRumoursImpl::Str(Town->Kinds[(size_t)Town->Rumours[(size_t)I].Kind].Id));
	}
}

FString ULyudTownRumours::Remarked(int32 Person, const TArray<FString>& Kinds)
{
	if (!bLoaded || Person < 0)
	{
		return FString();
	}
	const int32 Day = Today();
	Town->Mentionable(Person, Day, Scratch);
	for (const int I : Scratch)
	{
		const LyudRumour::FRumour& R = Town->Rumours[(size_t)I];
		if (Kinds.Contains(LyudTownRumoursImpl::Str(Town->Kinds[(size_t)R.Kind].Id)))
		{
			const std::string Detail = R.Detail;
			Town->MarkTold(I, Person, Day);
			if (const UWorld* World = GetWorld())
			{
				LastRemarkAt = World->GetTimeSeconds();
			}
			return DetailText(Detail);
		}
	}
	return FString();
}

FString ULyudTownRumours::DetailText(const std::string& Detail) const
{
	return Detail.empty() ? FString() : LyudText::Get(LyudTownRumoursImpl::Str(Detail)).ToString();
}

void ULyudTownRumours::PaperColumn(int32 Day, TArray<TPair<FString, TArray<FString>>>& Out) const
{
	Out.Reset();
	if (!bLoaded)
	{
		return;
	}
	ReadPending();
	std::vector<std::pair<std::string, std::vector<std::string>>> Lines;
	Town->PaperColumn(Day, Lines);
	for (const std::pair<std::string, std::vector<std::string>>& L : Lines)
	{
		TArray<FString> Args;
		for (const std::string& A : L.second)
		{
			Args.Add(LyudTownRumoursImpl::Str(A));
		}
		Out.Emplace(LyudTownRumoursImpl::Str(L.first), MoveTemp(Args));
	}
}

bool ULyudTownRumours::PostScript(int32 Day, FString& Key, TArray<FString>& Args) const
{
	ReadPending();
	std::string K;
	std::vector<std::string> A;
	if (!bLoaded || !Town->PostScript(Day, K, A))
	{
		return false;
	}
	Key = LyudTownRumoursImpl::Str(K);
	Args.Reset();
	for (const std::string& X : A)
	{
		Args.Add(LyudTownRumoursImpl::Str(X));
	}
	return true;
}

int32 ULyudTownRumours::DayOf(const FDateTime& When)
{
	return LyudTownRumoursImpl::DaysOf(When);
}

const std::vector<std::string>& ULyudTownRumours::KindsGoingRound()
{
	ReadPending();
	const int32 Day = bLoaded ? Today() : -1;
	if (Day != GoingRoundDay || Town->Rumours.size() != GoingRoundN)
	{
		GoingRoundDay = Day;
		GoingRoundN = bLoaded ? Town->Rumours.size() : 0;
		GoingRound.clear();
		if (bLoaded)
		{
			Town->Expire(Day);
			GoingRoundN = Town->Rumours.size();
			for (const LyudRumour::FRumour& R : Town->Rumours)
			{
				const std::string& K = Town->Kinds[(size_t)R.Kind].Id;
				if (std::find(GoingRound.begin(), GoingRound.end(), K) == GoingRound.end())
				{
					GoingRound.push_back(K);
				}
			}
		}
	}
	return GoingRound;
}

FString ULyudTownRumours::DetailOf(const FString& Kind) const
{
	if (!bLoaded)
	{
		return FString();
	}
	ReadPending();
	const int K = Town->FindKind(LyudTownRumoursImpl::Std(Kind));
	for (auto It = Town->Rumours.rbegin(); It != Town->Rumours.rend(); ++It)
	{
		if (It->Kind == K)
		{
			return DetailText(It->Detail);
		}
	}
	return FString();
}

FString ULyudTownRumours::Status()
{
	if (!bLoaded)
	{
		return TEXT("TownRumours: no data (Content/PeopleData/rumours.json)");
	}
	EnsureTies();
	const int32 Day = Today();
	Town->Expire(Day);
	FString Out = FString::Printf(TEXT("TownRumours: %d going round on day %d; next remark %s"), (int32)Town->Rumours.size(), Day,
		RemarkReady() ? TEXT("may come now") : TEXT("after the pause"));
	for (const LyudRumour::FRumour& R : Town->Rumours)
	{
		Out += FString::Printf(TEXT("\n  %s «%s» day %+d at %02d:%02d: %d saw it, %d know it today, %d tomorrow, %d have remarked"),
			*LyudTownRumoursImpl::Str(Town->Kinds[(size_t)R.Kind].Id), *DetailText(R.Detail), R.Day - Day, R.Minute / 60, R.Minute % 60, (int32)R.Witnesses.size(),
			Town->CountKnowing(R, Day), Town->CountKnowing(R, Day + 1), (int32)R.Told.size());
	}
	return Out;
}

namespace LyudTownRumoursConsole
{
	ULyudTownRumours* Of(UWorld* World) { return World ? World->GetSubsystem<ULyudTownRumours>() : nullptr; }

	FAutoConsoleCommandWithWorldAndArgs GStatus(TEXT("lyud.Rumours.Status"), TEXT("The talk of the town about the hero: each rumour, who saw it, who knows it, who has remarked."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudTownRumours* R = Of(World)) { UE_LOG(LogLyudRumours, Display, TEXT("%s"), *R->Status()); }
		}));
	FAutoConsoleCommandWithWorldAndArgs GAdd(TEXT("lyud.Rumours.Add"), TEXT("lyud.Rumours.Add <kind> [detail key]: as if it happened now, here (cup, beat, job, tamed, fine, ...)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ULyudTownRumours* R = Of(World);
			const bool bOk = R && Args.Num() > 0 && R->AddNow(Args[0], Args.Num() > 1 ? Args[1] : FString());
			UE_LOG(LogLyudRumours, Display, TEXT("TownRumours: add %s"), bOk ? TEXT("done") : TEXT("refused (unknown kind, or the same news is going round)"));
		}));
	FAutoConsoleCommandWithWorldAndArgs GClear(TEXT("lyud.Rumours.Clear"), TEXT("The town forgets everything it said of the hero."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudTownRumours* R = Of(World)) { R->ClearAll(); }
		}));
}

#if LYUD_RUMOURS_DEV
namespace LyudTownRumoursDev
{
	using namespace LyudDev;
	FAutoAction GCup(TEXT("rumour.cup"), TEXT("friends"), TEXT("dev.action.rumour.cup"),
		[](UWorld* W) { if (ULyudTownRumours* R = LyudTownRumoursConsole::Of(W)) { R->AddNow(TEXT("cup"), TEXT("city.coll.cups.cog_chess")); } });
	FAutoAction GBeat(TEXT("rumour.beat"), TEXT("friends"), TEXT("dev.action.rumour.beat"),
		[](UWorld* W) { if (ULyudTownRumours* R = LyudTownRumoursConsole::Of(W)) { R->AddNow(TEXT("beat"), TEXT("npc.t_sorokin_gennady.name")); } });
	FAutoAction GStatusAction(TEXT("rumour.status"), TEXT("friends"), TEXT("dev.action.rumour.status"),
		[](UWorld* W) { if (ULyudTownRumours* R = LyudTownRumoursConsole::Of(W)) { UE_LOG(LogLyudRumours, Display, TEXT("%s"), *R->Status()); } });
	FAutoAction GClearAction(TEXT("rumour.clear"), TEXT("friends"), TEXT("dev.action.rumour.clear"),
		[](UWorld* W) { if (ULyudTownRumours* R = LyudTownRumoursConsole::Of(W)) { R->ClearAll(); } });
}
#endif
