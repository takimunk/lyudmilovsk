#include "People/LyudFriends.h"

#include "Friends.h"
#include "MiniJson.h"
#include "PeopleManners.h"
#include "PeopleSim.h"
#include "City/LyudCityThings.h"
#include "City/SLyudCityPage.h"
#include "CollisionQueryParams.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Letters/LyudLetters.h"
#include "LyudGameMenu.h"
#include "LyudGoodsItem.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NpcDialogue.h"
#include "People/PeopleLife.h"
#include "People/TownTalk.h"
#include "Plants/GameCalendar.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#if __has_include("Dev/LyudDev.h")
#include "Dev/LyudDev.h"
#define LYUD_FRIENDS_DEV 1
#else
#define LYUD_FRIENDS_DEV 0
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLyudFriends, Log, All);

namespace LyudFriendsImpl
{
	/** A gift is offered to a townsperson this near (cm) and this near the middle of the view (cos). */
	constexpr double GiftReach = 190.0;
	constexpr double GiftCone = 0.94;
	/** A friend waves first from this far (cm) and no nearer than this (nearer they simply greet). */
	constexpr double WaveFar = 2200.0;
	constexpr double WaveNear = 500.0;
	/** His card is mentioned by somebody this near (cm). */
	constexpr double CardNear = 600.0;
	/** A friend comes to his bench from this far (cm). */
	constexpr double BenchFar = 6000.0;
	/** A family's outing counts when the hero is this near one of them (cm). */
	constexpr double OutingNear = 3000.0;
	/** A host's home: his visit counts within this of the chair at their table (cm). */
	constexpr double VisitNear = 1000.0;
	constexpr int32 PageSize = 9;

	FString Str(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
	std::string Std(const FString& S)
	{
		const FTCHARToUTF8 Conv(*S);
		return std::string(Conv.Get(), (size_t)Conv.Length());
	}

	/** Days since 1970-01-01 (the letters' calendar: the same numbers). */
	int32 DaysOf(const FDateTime& T)
	{
		return (int32)((T.GetDate() - FDateTime(1970, 1, 1)).GetTotalDays());
	}
}

using namespace LyudFriendsImpl;

ULyudFriends::ULyudFriends() = default;
ULyudFriends::~ULyudFriends() = default;

ULyudFriends* ULyudFriends::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudFriends>() : nullptr;
}

void ULyudFriends::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Model = MakePimpl<LyudFriends::FModel>();
	Load();
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("Friends"), this);
		Saves->OnNewGame.AddWeakLambda(this, [this]()
		{
			EndMode();
			if (Model)
			{
				Model->Reset();
			}
			Saved.Reset();
		});
	}
}

void ULyudFriends::Deinitialize()
{
	EndMode();
	SetPrompt(-1);
	Super::Deinitialize();
}

void ULyudFriends::Load()
{
	FString Text;
	const FString File = FPaths::ProjectContentDir() / TEXT("PeopleData/friends.json");
	if (!FFileHelper::LoadFileToString(Text, *File))
	{
		UE_LOG(LogLyudFriends, Warning, TEXT("Friends: %s not found: no friends"), *File);
		return;
	}
	std::string Error;
	if (!Model->Data.Load(Std(Text), Error))
	{
		UE_LOG(LogLyudFriends, Warning, TEXT("Friends: %s"), *Str(Error));
		return;
	}
	bLoaded = true;
	UE_LOG(LogLyudFriends, Display, TEXT("Friends: %d hosts, %d gifts, %d nicknames"), (int32)Model->Data.Hosts.size(), (int32)Model->Data.Gifts.size(),
		(int32)Model->Data.Nicknames.size());
	if (bSavedPending)
	{
		bSavedPending = false;
		OnGameLoaded_Implementation();
	}
}

void ULyudFriends::OnGameSaving_Implementation()
{
	if (!bLoaded || !Model)
	{
		return;
	}
	std::map<std::string, std::string> Out;
	Model->Save(Out);
	Saved.Reset();
	for (const std::pair<const std::string, std::string>& KV : Out)
	{
		Saved.Add(Str(KV.first), Str(KV.second));
	}
}

void ULyudFriends::OnGameLoaded_Implementation()
{
	EndMode();
	if (!bLoaded)
	{
		bSavedPending = true;
		return;
	}
	std::map<std::string, std::string> In;
	for (const TPair<FString, FString>& KV : Saved)
	{
		In[Std(KV.Key)] = Std(KV.Value);
	}
	Model->LoadState(In);
	WavedToday.Reset();
	CardSaid.Reset();
	BenchFriend = -1;
}

// --- who is who ----------------------------------------------------------------------------------------------------------

int32 ULyudFriends::Today(float* Hour, int32* Weekday) const
{
	const FDateTime N = UGameCalendar::NowIn(GetWorld());
	if (Hour)
	{
		*Hour = (float)N.GetHour() + (float)N.GetMinute() / 60.f;
	}
	if (Weekday)
	{
		*Weekday = ((int32)N.GetDayOfWeek() + 7) % 7;     // EDayOfWeek: Monday = 0
	}
	return DaysOf(N);
}

FString ULyudFriends::KeyOf(int32 PersonId) const
{
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LyudPeople::FWorld* S = Life ? Life->GetSim() : nullptr;
	return S && PersonId >= 0 && PersonId < (int32)S->People.size() ? Str(S->People[(size_t)PersonId].Key) : FString();
}

int32 ULyudFriends::PersonOf(const FString& Key) const
{
	if (const int32* Id = PersonIds.Find(Key))
	{
		return *Id;
	}
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	if (!Life || !Life->IsRunning())
	{
		return -1;
	}
	const int32 Id = Life->FindPerson(Key);
	PersonIds.Add(Key, Id);
	return Id;
}

FText ULyudFriends::NameOf(const FString& Key) const
{
	if (const LyudFriends::FHost* H = Model ? Model->Data.HostByKey(Std(Key)) : nullptr)
	{
		if (H->Person.empty())
		{
			return LyudText::Get(Str(H->Name));
		}
	}
	const int32 Id = PersonOf(Key);
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	return Life && Id >= 0 ? Life->GetDisplayName(Id) : LyudText::Get(FString::Printf(TEXT("npc.%s.name"), *Key));
}

bool ULyudFriends::WhoOf(const FString& Key, LyudFriends::FWho& Out) const
{
	Out = LyudFriends::FWho();
	Out.Key = Std(Key);
	Out.bHost = Model && Model->Data.HostByKey(Out.Key) != nullptr;
	const int32 Id = PersonOf(Key);
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LyudPeople::FWorld* S = Life ? Life->GetSim() : nullptr;
	if (!S || Id < 0 || Id >= (int32)S->People.size())
	{
		Out.Age = 55;      // a host the life doesn't have: a grown-up with their own home
		return Out.bHost;
	}
	const LyudPeople::FPerson& P = S->People[(size_t)Id];
	Out.Age = P.Age;
	Out.Job = P.Occupation;
	if (const LyudPeople::FManners* M = Life->GetManners())
	{
		if (const LyudPeople::FPersona* Pe = M->PersonaOf(Id))
		{
			for (int32 B = 0; B < 16; ++B)
			{
				const char* Name = Pe->Has(1u << B) ? LyudPeople::HabitName(1u << B) : nullptr;
				if (Name && *Name)
				{
					Out.Habits.push_back(Name);
				}
			}
			Out.Warmth = Pe->Warmth;
			Out.Curiosity = Pe->Curiosity;
			Out.Tidy = Pe->Tidy;
		}
	}
	return true;
}

const char* ULyudFriends::BondWord(int32 PersonId) const
{
	return bLoaded && Model && PersonId >= 0 ? Model->BondWord(Std(KeyOf(PersonId))) : "";
}

FString ULyudFriends::NickOf(int32 PersonId) const
{
	const LyudFriends::FBond* B = bLoaded && Model && PersonId >= 0 ? Model->Find(Std(KeyOf(PersonId))) : nullptr;
	return B && !B->Nick.empty() ? LyudText::Get(TEXT("friends.nick.") + Str(B->Nick)).ToString() : FString();
}

void ULyudFriends::Grew(const FString& Key, int32 Added)
{
	if (Added <= 0 || !Model)
	{
		return;
	}
	// the nickname comes with being a friend: told once, on the screen
	LyudFriends::FWho Who;
	if (WhoOf(Key, Who) && Model->EnsureNick(Std(Key), Who))
	{
		const LyudFriends::FBond* B = Model->Find(Std(Key));
		Toast(LyudText::Get(TEXT("friends.know.toast"), { FStringFormatArg(NameOf(Key).ToString()),
			FStringFormatArg(LyudText::Get(TEXT("friends.nick.") + Str(B->Nick)).ToString()) }));
	}
}

void ULyudFriends::OnGreeted(int32 PersonId)
{
	if (bLoaded && PersonId >= 0)
	{
		const FString Key = KeyOf(PersonId);
		Grew(Key, Model->Greeted(Std(Key), Today()));
	}
}

void ULyudFriends::OnHeard(int32 PersonId)
{
	if (bLoaded && PersonId >= 0)
	{
		const FString Key = KeyOf(PersonId);
		Grew(Key, Model->Heard(Std(Key), Today()));
	}
}

void ULyudFriends::OnCardDelivered(const FString& To)
{
	if (!bLoaded)
	{
		return;
	}
	// a correspondent of the open homes is their host's bond
	FString Key = To;
	for (const LyudFriends::FHost& H : Model->Data.Hosts)
	{
		if (!H.Correspondent.empty() && Str(H.Correspondent) == To)
		{
			Key = Str(H.Key());
		}
	}
	Grew(Key, Model->Card(Std(Key), Today()));
	CardSaid.Remove(Key);
}

void ULyudFriends::Toast(const FText& Text) const
{
	if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
	{
		Screen->Toast(Text);
	}
}

bool ULyudFriends::SayEvent(int32 PersonId, FName Event)
{
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this);
	FPersonInfo Info;
	if (!Life || !Talk || !Life->GetPerson(PersonId, Info) || !IsValid(Info.Actor.Get()) || Info.Actor->IsHidden())
	{
		return false;
	}
	return Talk->SayEvent(Info.Actor.Get(), Event, PersonId);
}

// --- the tick ------------------------------------------------------------------------------------------------------------

void ULyudFriends::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !bLoaded || !Model)
	{
		return;
	}
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	StepClock -= DeltaTime;
	if (StepClock <= 0.f)
	{
		StepClock = 2.f;
		StepInvites();
		StepQuests();
		if (Hero)
		{
			TickVisits(Hero);
			TickOuting(Hero);
		}
	}
	if (Hero)
	{
		NearClock -= DeltaTime;
		if (NearClock <= 0.f)
		{
			NearClock = 0.5f;
			TickNear(Hero);
		}
		BenchClock -= DeltaTime;
		if (BenchClock <= 0.f)
		{
			BenchClock = 1.f;
			TickBench(Hero);
		}
	}
	if (Mode != EMode::None)
	{
		SetPrompt(-1);
		TickMode(PC, DeltaTime);
		return;
	}
	if (Hero)
	{
		TickTeaChair(Hero);
		TickOffer(Hero, PC);
	}
}

void ULyudFriends::StepInvites()
{
	float Hour = 12.f;
	int32 Weekday = 0;
	const int32 Day = Today(&Hour, &Weekday);
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LyudPeople::FWorld* S = Life ? Life->GetSim() : nullptr;
	// the families of the close friends: their weekend outings as the life has them
	std::map<std::string, LyudFriends::FOutingInfo> Outings;
	if (S)
	{
		for (const auto& KV : Model->Bonds)
		{
			if (Model->Level(KV.first) < Model->Data.R.OutingLevel)
			{
				continue;
			}
			const int32 Id = PersonOf(Str(KV.first));
			if (Id < 0 || Id >= (int32)S->People.size())
			{
				continue;
			}
			const LyudPeople::FPerson& P = S->People[(size_t)Id];
			if (P.OutingAct >= 0 && P.OutingAct < (int32)S->Activities.size())
			{
				Outings[KV.first] = { S->Activities[(size_t)P.OutingAct].Id, P.OutingDay, P.OutingHour };
			}
		}
	}
	ULyudLetters* Letters = ULyudLetters::Get(this);
	for (const LyudFriends::FInvite& I : Model->Step(Day, Hour, Weekday, Outings))
	{
		const FString Key = Str(I.Key);
		const int32 Ahead = I.Day - Day;
		const FString DayKey = Ahead == 0 ? FString(TEXT("@friends.day.today")) : (Ahead == 1 ? FString(TEXT("@friends.day.tomorrow"))
			: FString::Printf(TEXT("@friends.day.in.%d"), ((Weekday + Ahead) % 7 + 7) % 7));
		const FString Time = FString::Printf(TEXT("%d:%02d"), I.Hour, I.Minute);
		TArray<FString> Args = { DayKey, Time };
		FString From = Str(I.From);
		if (I.Kind == "outing")
		{
			Args.Add(TEXT("@npc.") + Key + TEXT(".name"));
			From = Key;
		}
		const FString Id = FString::Printf(TEXT("friend:%s:%s:%d"), *Str(I.Kind), *Key, I.Day);
		const bool bPosted = Letters && Letters->PostTownLetter(Id, From, Str(I.Body), Args);
		UE_LOG(LogLyudFriends, Display, TEXT("Friends: %s invitation from %s for day %d at %s (%s)"), *Str(I.Kind), *Key, I.Day, *Time,
			bPosted ? TEXT("posted") : TEXT("no post"));
	}
}

void ULyudFriends::StepQuests()
{
	const ULyudQuests* Quests = ULyudQuests::Get(this);
	if (!Quests)
	{
		return;
	}
	const int32 Day = Today();
	for (const FLyudQuestProgress& P : Quests->Progress)
	{
		if (P.State != ELyudQuestState::Done)
		{
			continue;
		}
		if (const LyudFriends::FHost* H = Model->Data.HostByQuest(Std(P.Quest.ToString())))
		{
			const FString Key = Str(H->Key());
			Grew(Key, Model->QuestDone(H->Key(), H->Quest, Day));
		}
	}
}

void ULyudFriends::TickVisits(AHeroCharacter* Hero)
{
	const FVector At = Hero->GetActorLocation();
	const int32 Day = Today();
	for (const LyudFriends::FHost& H : Model->Data.Hosts)
	{
		const FVector Chair(H.ChairAt[0], H.ChairAt[1], H.ChairAt[2]);
		if (FVector::DistSquared2D(At, Chair) < FMath::Square(VisitNear) && FMath::Abs(At.Z - Chair.Z) < 300.0)
		{
			Grew(Str(H.Key()), Model->Visited(H.Key(), Day));
		}
	}
}

void ULyudFriends::TickNear(AHeroCharacter* Hero)
{
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	if (!Life || !Life->IsRunning())
	{
		return;
	}
	const int32 Day = Today();
	if (Day != NearDay)
	{
		NearDay = Day;
		WavedToday.Reset();
	}
	const FVector HeroAt = Hero->GetActorLocation();
	const FVector Look = Hero->GetControlRotation().Vector().GetSafeNormal2D();
	for (const auto& KV : Model->Bonds)
	{
		const int32 L = Model->Level(KV.first);
		if (L < 1)
		{
			continue;
		}
		const FString Key = Str(KV.first);
		const int32 Id = PersonOf(Key);
		int32 Act = -1;
		bool bDoing = false;
		EPersonPose Pose = EPersonPose::Stand;
		FVector At;
		if (Id < 0 || !Life->GetActivityState(Id, Act, bDoing, Pose, At))
		{
			continue;
		}
		const double D = FVector::Dist2D(At, HeroAt);
		if (D > WaveFar || FMath::Abs(At.Z - HeroAt.Z) > 400.0)
		{
			continue;
		}
		// his card on their shelf: mentioned the day it came and the next, once, when he is close
		if (KV.second.CardDay >= 0 && Day - KV.second.CardDay <= 1 && D < CardNear && !CardSaid.Contains(Key))
		{
			if (SayEvent(Id, TEXT("friend_card")))
			{
				CardSaid.Add(Key);
			}
			continue;
		}
		// a friend waves first from across the street (in front of him, once a day), and a good friend calls out too
		if (L >= Model->Data.R.WaveLevel && D > WaveNear && !WavedToday.Contains(Key)
			&& FVector::DotProduct((At - HeroAt).GetSafeNormal2D(), Look) > 0.5 && Pose != EPersonPose::Sleep)
		{
			if (Life->ReactManner(Id, TEXT("wave"), 2.6f))
			{
				WavedToday.Add(Key);
				if (L >= Model->Data.R.NickLevel)
				{
					SayEvent(Id, TEXT("friend_wave"));
				}
			}
		}
	}
}

void ULyudFriends::TickBench(AHeroCharacter* Hero)
{
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const ULyudCityThings* City = ULyudCityThings::Get(this);
	const bool bSeated = Hero->IsSeated() || (City && City->IsSittingOnThing());
	if (!Life || !Life->IsRunning() || !bSeated || Mode == EMode::Tea)
	{
		BenchFriend = -1;
		bBenchSat = false;
		return;
	}
	const FVector HeroAt = Hero->GetActorLocation();
	const UWorld* World = GetWorld();
	const float Now = World ? (float)World->GetTimeSeconds() : 0.f;
	if (BenchFriend >= 0)
	{
		int32 Act = -1;
		bool bDoing = false;
		EPersonPose Pose = EPersonPose::Stand;
		FVector At;
		if (!Life->GetActivityState(BenchFriend, Act, bDoing, Pose, At) || FVector::Dist2D(At, HeroAt) > BenchFar)
		{
			BenchFriend = -1;
			return;
		}
		const bool bThere = Pose == EPersonPose::Sit && FVector::Dist2D(At, HeroAt) < 280.0;
		if (bThere && !bBenchSat)
		{
			bBenchSat = true;
			BenchLines = 0;
			BenchNextLine = Now + 2.5f;
			const FString Key = KeyOf(BenchFriend);
			Toast(LyudText::Get(TEXT("friends.bench.toast"), { FStringFormatArg(NameOf(Key).ToString()) }));
			Grew(Key, Model->Bench(Std(Key), Today()));
		}
		if (bBenchSat && BenchLines < 3 && Now >= BenchNextLine)
		{
			BenchLines += SayEvent(BenchFriend, TEXT("friend_bench")) ? 1 : 0;
			BenchNextLine = Now + 22.f;
		}
		return;
	}
	// a friend about (awake, not at work, not already been with him today) comes over and sits down beside him
	float Hour = 12.f;
	const int32 Day = Today(&Hour);
	if (Hour < 8.f || Hour > 21.f)
	{
		return;
	}
	for (const auto& KV : Model->Bonds)
	{
		if (Model->Level(KV.first) < Model->Data.R.BenchLevel || KV.second.BenchDay == Day)
		{
			continue;
		}
		const int32 Id = PersonOf(Str(KV.first));
		int32 Act = -1;
		bool bDoing = false;
		EPersonPose Pose = EPersonPose::Stand;
		FVector At;
		if (Id < 0 || !Life->GetActivityState(Id, Act, bDoing, Pose, At) || Pose == EPersonPose::Sleep || FVector::Dist2D(At, HeroAt) > BenchFar)
		{
			continue;
		}
		FPersonInfo Info;
		if (!Life->GetPerson(Id, Info) || Info.Activity.StartsWith(TEXT("work")) || Info.Activity == TEXT("sleep") || Info.State == TEXT("away"))
		{
			continue;
		}
		if (Life->SitBeside(Id, HeroAt - FVector(0.0, 0.0, 45.0), 260.f, 15.f))
		{
			BenchFriend = Id;
			bBenchSat = false;
			UE_LOG(LogLyudFriends, Display, TEXT("Friends: %s comes to sit with the hero"), *Str(KV.first));
			return;
		}
	}
}

void ULyudFriends::TickOuting(AHeroCharacter* Hero)
{
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LyudPeople::FWorld* S = Life ? Life->GetSim() : nullptr;
	if (!S)
	{
		return;
	}
	float Hour = 12.f;
	const int32 Day = Today(&Hour);
	const FVector HeroAt = Hero->GetActorLocation();
	TArray<FString> Joined;
	for (const auto& KV : Model->Bonds)
	{
		if (KV.second.OutingDay != Day)
		{
			continue;
		}
		const int32 Id = PersonOf(Str(KV.first));
		if (Id < 0 || Id >= (int32)S->People.size())
		{
			continue;
		}
		const LyudPeople::FPerson& P = S->People[(size_t)Id];
		if (P.OutingAct < 0 || P.OutingAct >= (int32)S->Activities.size())
		{
			continue;
		}
		const LyudFriends::FOutingInfo O{ S->Activities[(size_t)P.OutingAct].Id, P.OutingDay, P.OutingHour };
		int32 Act = -1;
		bool bDoing = false;
		EPersonPose Pose = EPersonPose::Stand;
		FVector At;
		if (Model->OutingNow(KV.first, Day, Hour, O) && Life->GetActivityState(Id, Act, bDoing, Pose, At) && Act == P.OutingAct
			&& FVector::Dist2D(At, HeroAt) < OutingNear)
		{
			Joined.Add(Str(KV.first));
		}
	}
	for (const FString& Key : Joined)
	{
		const int32 Added = Model->Outing(Std(Key), Day);
		if (Added <= 0)
		{
			continue;
		}
		const int32 Id = PersonOf(Key);
		Life->ReactManner(Id, TEXT("wave"), 2.6f);
		SayEvent(Id, TEXT("friend_outing"));
		Toast(LyudText::Get(TEXT("friends.outing.toast"), { FStringFormatArg(NameOf(Key).ToString()) }));
		if (ULyudQuests* Quests = ULyudQuests::Get(this))
		{
			Quests->AddNote(TEXT("friends.outing.diary"), { TEXT("@npc.") + Key + TEXT(".name") });
		}
		Grew(Key, Added);
	}
}

// --- the tea at a host's ---------------------------------------------------------------------------------------------------

void ULyudFriends::TickTeaChair(AHeroCharacter* Hero)
{
	const ULyudCityThings* City = ULyudCityThings::Get(this);
	const FString Seat = City ? City->SeatedThingId() : FString();
	if (Seat.IsEmpty())
	{
		TeaNotNowShown.Reset();
		TeaClosedSeat.Reset();
		return;
	}
	float Hour = 12.f;
	const int32 Day = Today(&Hour);
	for (const LyudFriends::FHost& H : Model->Data.Hosts)
	{
		if (Str(H.Chair) != Seat)
		{
			continue;
		}
		if (TeaClosedSeat == Seat)
		{
			return;     // the tea at this table just ended (or was left): not again until he gets up
		}
		if (Model->TeaNow(H.Key(), Day, Hour))
		{
			OpenTea(H);
			return;
		}
		// invited, but not now: when he is expected (once while he sits)
		const LyudFriends::FBond* B = Model->Find(H.Key());
		if (B && B->TeaDay >= 0 && TeaNotNowShown != Seat)
		{
			TeaNotNowShown = Seat;
			int32 Weekday = 0;
			Today(nullptr, &Weekday);
			const int32 Ahead = B->TeaDay - Day;
			const FString DayKey = Ahead <= 0 ? FString(TEXT("friends.day.today")) : (Ahead == 1 ? FString(TEXT("friends.day.tomorrow"))
				: FString::Printf(TEXT("friends.day.in.%d"), ((Weekday + Ahead) % 7 + 7) % 7));
			Toast(LyudText::Get(TEXT("friends.tea.not_now"), { FStringFormatArg(LyudText::Get(DayKey).ToString()),
				FStringFormatArg(FString::Printf(TEXT("%d:00"), B->TeaHour)) }));
		}
		return;
	}
	(void)Hero;
}

void ULyudFriends::OpenTea(const LyudFriends::FHost& Host)
{
	TeaHost = Str(Host.Id);
	TeaClosedSeat = Str(Host.Chair);
	TeaAsked.Init(false, FMath::Max(1, Host.Topics));
	TeaShown = -1;
	TeaLine = LyudText::Get(TEXT("friends.tea.") + TeaHost + TEXT(".welcome"));
	bTeaBye = false;
	TeaPerson = Host.Person.empty() ? -1 : PersonOf(Str(Host.Person));
	Mode = EMode::Tea;
	OpenPage();
	PlaceTeaThings(Host);
	ShowTea();
	UE_LOG(LogLyudFriends, Display, TEXT("Friends: tea at %s's"), *TeaHost);
}

void ULyudFriends::ShowTea()
{
	const LyudFriends::FHost* H = Model->Data.HostById(Std(TeaHost));
	if (!H || !Page)
	{
		return;
	}
	const FString Name = LyudText::Get(Str(H->Name)).ToString();
	FString Body = TeaLine.ToString();
	if (!bTeaBye)
	{
		Body += TEXT("\n");
		for (int32 I = 0; I < TeaAsked.Num(); ++I)
		{
			const FString Q = LyudText::Get(FString::Printf(TEXT("friends.tea.%s.q%d"), *TeaHost, I + 1)).ToString();
			Body += FString::Printf(TEXT("\n%s%d. %s"), TeaAsked[I] ? TEXT("  ") : TEXT(""), I + 1, *Q);
		}
		Body += FString::Printf(TEXT("\n%d. %s"), TeaAsked.Num() + 1, *LyudText::Get(TEXT("friends.gift.title")).ToString());
	}
	Page->SetPage(LyudText::Get(TEXT("friends.tea.title"), { FStringFormatArg(Name) }), FText::FromString(Body),
		LyudText::Get(TEXT("friends.tea.keys")));
}

void ULyudFriends::EndTea()
{
	const LyudFriends::FHost* H = Model->Data.HostById(Std(TeaHost));
	bool bAny = false;
	for (const bool B : TeaAsked)
	{
		bAny = bAny || B;
	}
	if (H && bAny)
	{
		const FString Key = Str(H->Key());
		Grew(Key, Model->Tea(H->Key(), Today()));
		if (ULyudQuests* Quests = ULyudQuests::Get(this))
		{
			Quests->AddNote(TEXT("friends.tea.diary"), { TEXT("@") + Str(H->Name) });
			Quests->Notify(this, TEXT("do"), TEXT("friends_tea"));
		}
	}
	ClearTeaThings();
}

void ULyudFriends::PlaceTeaThings(const LyudFriends::FHost& Host)
{
	ClearTeaThings();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// on the table in front of his chair: two cups, the teapot's stand-in (a tin of tea) and something sweet
	const FVector Chair(Host.ChairAt[0], Host.ChairAt[1], Host.ChairAt[2]);
	const FVector Fwd = FRotator(0.f, Host.ChairYaw, 0.f).Vector();
	const FVector Right = FRotator(0.f, Host.ChairYaw + 90.f, 0.f).Vector();
	const FVector Top = Chair + Fwd * 55.0 + FVector(0, 0, 130);
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(FriendsTea), false);
	if (APawn* Hero = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		Q.AddIgnoredActor(Hero);
	}
	if (!World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 110), ECC_Visibility, Q) || Hit.ImpactNormal.Z < 0.85f
		|| Hit.ImpactPoint.Z - Chair.Z < 50.0 || Hit.ImpactPoint.Z - Chair.Z > 110.0)
	{
		UE_LOG(LogLyudFriends, Display, TEXT("Friends: no table top in front of %s: the tea is served in words"), *Str(Host.Chair));
		return;
	}
	struct FPut { const TCHAR* Item; double Fwd; double Side; };
	static const FPut Puts[] = { { TEXT("tea"), 0.0, -8.0 }, { TEXT("tea"), 30.0, 10.0 }, { TEXT("gingerbread"), 14.0, 22.0 } };
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.ObjectFlags |= RF_Transient;
	for (const FPut& Put : Puts)
	{
		const FVector At = Hit.ImpactPoint + Fwd * Put.Fwd + Right * Put.Side + FVector(0, 0, 0.5);
		ALyudGoodsItem* Item = World->SpawnActor<ALyudGoodsItem>(ALyudGoodsItem::StaticClass(), At, FRotator(0.f, Host.ChairYaw + 180.f, 0.f), P);
		if (Item)
		{
			Item->Init(Put.Item);
			Item->SetActorEnableCollision(false);
			ALyudGoodsItem::SetShelfStock(Item);
			TeaThings.Add(Item);
		}
	}
}

void ULyudFriends::ClearTeaThings()
{
	for (TObjectPtr<ALyudGoodsItem>& T : TeaThings)
	{
		if (T)
		{
			T->Destroy();
		}
	}
	TeaThings.Reset();
	if (HostBody)
	{
		HostBody->Destroy();
		HostBody = nullptr;
	}
	if (TeaPerson >= 0)
	{
		if (UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this))
		{
			Life->SetPresentedExternally(TeaPerson, false);
		}
	}
	TeaPerson = -1;
}

void ULyudFriends::DriveHostBody(float DeltaTime)
{
	// the host the town's life has sits at the table with him: their own body, taken over while the tea lasts
	const LyudFriends::FHost* H = Model->Data.HostById(Std(TeaHost));
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	UWorld* World = GetWorld();
	if (!H || H->HostChair.empty() || TeaPerson < 0 || !Life || !World)
	{
		return;
	}
	const FVector At(H->HostChairAt[0], H->HostChairAt[1], H->HostChairAt[2]);
	const FVector Chair(H->ChairAt[0], H->ChairAt[1], H->ChairAt[2]);
	const float Yaw = (Chair - At).GetSafeNormal2D().Rotation().Yaw;
	if (!HostBody)
	{
		const FString ModelName = Life->ShownModel(TeaPerson);
		if (ModelName.IsEmpty())
		{
			return;
		}
		FActorSpawnParameters SP;
		SP.ObjectFlags |= RF_Transient;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		HostBody = World->SpawnActor<ANpcBody>(At, FRotator(0.f, Yaw, 0.f), SP);
		if (!HostBody || !HostBody->Setup(ModelName, FString()))
		{
			if (HostBody)
			{
				HostBody->Destroy();
			}
			HostBody = nullptr;
			TeaPerson = -1;     // no model: the tea is told in words
			return;
		}
		Life->SetPresentedExternally(TeaPerson, true);
		HostBody->Drive(At, Yaw, EPersonPose::Sit, 1, 0.f, H->HostChairHeight * 100.f, DeltaTime, true);
		return;
	}
	HostBody->Drive(At, Yaw, EPersonPose::Sit, 0, 0.f, H->HostChairHeight * 100.f, DeltaTime, false);
	if (APawn* Hero = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		HostBody->LookAtPoint(Hero->GetActorLocation() + FVector(0, 0, 60), 1.f);
	}
}

// --- gifts -------------------------------------------------------------------------------------------------------------------

void ULyudFriends::TickOffer(AHeroCharacter* Hero, APlayerController* PC)
{
	UWorld* World = GetWorld();
	if (!World || !PC || World->GetTimeSeconds() < RefusedUntil)
	{
		SetPrompt(-1);
		return;
	}
	OfferClock -= World->GetDeltaSeconds();
	if (OfferClock <= 0.f)
	{
		OfferClock = 0.15f;
		int32 Offer = -1;
		const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
		const ULyudCityThings* City = ULyudCityThings::Get(World);
		const ULyudLetters* Letters = ULyudLetters::Get(World);
		const bool bCalm = UI && UI->CanOpen(true) && !Hero->IsOnBike() && !Hero->IsLeadingBike() && !(City && City->HasModeUp())
			&& !(Letters && (Letters->IsOffering() || Letters->HasModeUp()));
		const ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(World);
		AActor* Actor = nullptr;
		const int32 Id = bCalm && Talk ? Talk->NearestSpeaker(Actor) : -1;
		if (Id >= 0 && Actor)
		{
			const FVector Eye = Hero->GetPawnViewLocation();
			const FVector To = Actor->GetActorLocation() + FVector(0, 0, 40) - Eye;
			const bool bNear = FVector::Dist2D(Actor->GetActorLocation(), Hero->GetActorLocation()) < GiftReach;
			const bool bInView = FVector::DotProduct(To.GetSafeNormal(), Hero->GetControlRotation().Vector()) > GiftCone;
			// a gift for somebody he knows (they greeted him at least once)
			const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(World);
			if (bNear && bInView && Life && (Life->GetMet(Id) > 0 || Model->Level(Std(KeyOf(Id))) >= 1))
			{
				Offer = Id;
			}
		}
		SetPrompt(Offer);
	}
	if (OfferedPerson >= 0 && ANpcTalkDirector::UsePressed(PC))
	{
		OpenGift(OfferedPerson, KeyOf(OfferedPerson));
	}
}

void ULyudFriends::SetPrompt(int32 PersonId)
{
	if (PersonId == OfferedPerson)
	{
		return;
	}
	OfferedPerson = PersonId;
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (!UI)
	{
		return;
	}
	UI->SetPrompt(this, PersonId < 0 ? FText::GetEmpty()
		: LyudText::Get(TEXT("friends.gift.prompt"), { FStringFormatArg(NameOf(KeyOf(PersonId)).ToString()) }));
}

void ULyudFriends::OpenGift(int32 PersonId, const FString& Key)
{
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	GiftItems.Reset();
	for (const LyudFriends::FGift& G : Model->Data.Gifts)
	{
		const FName Item(*Str(G.Item));
		if (Inv && Inv->Count(Item) > 0)
		{
			GiftItems.Add(Item);
		}
	}
	if (GiftItems.Num() == 0)
	{
		Toast(LyudText::Get(TEXT("friends.gift.none")));
		if (UWorld* World = GetWorld())
		{
			RefusedUntil = World->GetTimeSeconds() + 3.0;
		}
		SetPrompt(-1);
		return;
	}
	GiftPerson = PersonId;
	GiftKey = Key;
	GiftPage = 0;
	GiftReturn = Mode;
	if (Mode == EMode::None)
	{
		OpenPage();
	}
	Mode = EMode::Gift;
	SetPrompt(-1);
	ShowGift();
}

void ULyudFriends::ShowGift()
{
	if (!Page)
	{
		return;
	}
	FString Body;
	const int32 First = GiftPage * PageSize;
	for (int32 I = First; I < FMath::Min(First + PageSize, GiftItems.Num()); ++I)
	{
		Body += FString::Printf(TEXT("%d. %s\n"), I - First + 1, *ULyudInventory::ItemName(GiftItems[I]).ToString());
	}
	Page->SetPage(LyudText::Get(TEXT("friends.gift.prompt"), { FStringFormatArg(NameOf(GiftKey).ToString()) }), FText::FromString(Body.TrimEnd()),
		LyudText::Get(TEXT("friends.gift.keys")));
}

void ULyudFriends::GiveChosen(int32 Entry)
{
	if (!GiftItems.IsValidIndex(Entry))
	{
		return;
	}
	const FName Item = GiftItems[Entry];
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Inv || !Inv->Remove(Item, 1))
	{
		return;
	}
	LyudFriends::FWho Who;
	WhoOf(GiftKey, Who);
	int32 Added = 0;
	const LyudFriends::EGift G = Model->Give(Std(GiftKey), Std(Item.ToString()), Who, Today(), &Added);
	const FString Word = Str(LyudFriends::GiftWord(G));
	const int32 Lines = G == LyudFriends::EGift::Again ? 3 : (G == LyudFriends::EGift::Plain ? 4 : 5);
	const FString LineKey = FString::Printf(TEXT("line.friends.gift.%s.%d"), *Word, FMath::RandRange(1, Lines));
	if (ULyudQuests* Quests = ULyudQuests::Get(this))
	{
		Quests->AddNote(TEXT("friends.gift.diary"), { NameOf(GiftKey).ToString(), ULyudInventory::ItemName(Item).ToString() });
		Quests->Notify(this, TEXT("do"), TEXT("friends_gift"));
	}
	const EMode Back = GiftReturn;
	if (Back == EMode::Tea)
	{
		// at the table: the host's words on the page
		const FString Nick = NickOf(TeaPerson);
		TeaLine = LyudText::Get(LineKey, { FStringFormatArg(Nick.IsEmpty() ? LyudText::Get(TEXT("friends.nick.neighbour")).ToString() : Nick) });
		Mode = EMode::Tea;
		ShowTea();
	}
	else
	{
		EndMode();
		const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
		ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this);
		FPersonInfo Info;
		if (Life && Talk && Life->GetPerson(GiftPerson, Info) && IsValid(Info.Actor.Get()))
		{
			Talk->SayLine(Info.Actor.Get(), LineKey, GiftPerson);
		}
		if (UPeopleLifeSubsystem* L = UPeopleLifeSubsystem::Get(this))
		{
			L->ReactManner(GiftPerson, G == LyudFriends::EGift::Loved ? TEXT("wave") : TEXT("nod"), 2.f);
		}
	}
	Grew(GiftKey, Added);
	UE_LOG(LogLyudFriends, Display, TEXT("Friends: gave %s to %s: %s (+%d)"), *Item.ToString(), *GiftKey, *Word, Added);
}

// --- the page --------------------------------------------------------------------------------------------------------------

void ULyudFriends::OpenPage()
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport)
	{
		return;
	}
	if (!Page)
	{
		Page = SNew(SLyudCityPage);
		Viewport->AddViewportWidgetContent(Page.ToSharedRef(), 61);
	}
	Page->SetScreen(false);
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (PC && !MoveBlockedBy.IsValid())
	{
		PC->SetIgnoreMoveInput(true);
		MoveBlockedBy = PC;
	}
	ModeFrame = GFrameCounter;
}

void ULyudFriends::ClosePage()
{
	if (!Page)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(Page.ToSharedRef());
	}
	Page.Reset();
}

void ULyudFriends::EndMode()
{
	if (Mode == EMode::Tea || GiftReturn == EMode::Tea)
	{
		EndTea();
	}
	if (APlayerController* PC = MoveBlockedBy.Get())
	{
		PC->SetIgnoreMoveInput(false);
	}
	MoveBlockedBy.Reset();
	ClosePage();
	Mode = EMode::None;
	GiftReturn = EMode::None;
	GiftItems.Reset();
	TeaHost.Reset();
}

void ULyudFriends::TickMode(APlayerController* PC, float DeltaTime)
{
	UWorld* World = GetWorld();
	const ULyudInventoryUI* UI = World ? ULyudInventoryUI::Get(World) : nullptr;
	if (!World || !PC || !Page || (UI && UI->IsOpen()))
	{
		EndMode();
		return;
	}
	if (Mode == EMode::Tea || GiftReturn == EMode::Tea)
	{
		DriveHostBody(DeltaTime);
		// up from the chair: the tea is over
		const ULyudCityThings* City = ULyudCityThings::Get(World);
		if (!City || City->SeatedThingId().IsEmpty())
		{
			EndMode();
			return;
		}
	}
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	if ((Menu && Menu->IsOpen()) || GFrameCounter == ModeFrame)
	{
		return;
	}
	const bool bEsc = PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::SpaceBar);
	static const FKey Digits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	if (Mode == EMode::Gift)
	{
		if (bEsc)
		{
			if (GiftReturn == EMode::Tea)
			{
				Mode = EMode::Tea;
				ShowTea();
			}
			else
			{
				EndMode();
			}
			return;
		}
		const int32 Pages = FMath::Max(1, (GiftItems.Num() + PageSize - 1) / PageSize);
		if (PC->WasInputKeyJustPressed(EKeys::D) || PC->WasInputKeyJustPressed(EKeys::A))
		{
			GiftPage = (GiftPage + (PC->WasInputKeyJustPressed(EKeys::D) ? 1 : Pages - 1)) % Pages;
			ShowGift();
			return;
		}
		for (int32 I = 0; I < PageSize; ++I)
		{
			if (PC->WasInputKeyJustPressed(Digits[I]))
			{
				GiveChosen(GiftPage * PageSize + I);
				return;
			}
		}
		return;
	}
	// the tea: 1..3 a topic (each once), 4 a gift, Esc says goodbye (the first time) and gets up from the table (the second)
	if (bEsc || (bTeaBye && PC->WasInputKeyJustPressed(EKeys::E)))
	{
		if (bTeaBye)
		{
			EndMode();
			return;
		}
		bTeaBye = true;
		const FString Nick = NickOf(TeaPerson);
		TeaLine = LyudText::Get(TEXT("friends.tea.") + TeaHost + TEXT(".bye"), { FStringFormatArg(Nick.IsEmpty() ? LyudText::Get(TEXT("friends.nick.neighbour")).ToString() : Nick) });
		ShowTea();
		return;
	}
	if (bTeaBye)
	{
		return;
	}
	for (int32 I = 0; I < TeaAsked.Num(); ++I)
	{
		if (PC->WasInputKeyJustPressed(Digits[I]))
		{
			TeaAsked[I] = true;
			TeaShown = I;
			const FString Nick = NickOf(TeaPerson);
			TeaLine = LyudText::Get(FString::Printf(TEXT("friends.tea.%s.a%d"), *TeaHost, I + 1),
				{ FStringFormatArg(Nick.IsEmpty() ? LyudText::Get(TEXT("friends.nick.neighbour")).ToString() : Nick) });
			ShowTea();
			return;
		}
	}
	if (TeaAsked.Num() < PageSize && PC->WasInputKeyJustPressed(Digits[TeaAsked.Num()]))
	{
		const LyudFriends::FHost* H = Model->Data.HostById(Std(TeaHost));
		if (H)
		{
			OpenGift(-1, Str(H->Key()));
		}
	}
}

// --- the diary -------------------------------------------------------------------------------------------------------------

void ULyudFriends::DiaryFriends(TArray<FDiaryFriend>& Out) const
{
	Out.Reset();
	if (!bLoaded || !Model)
	{
		return;
	}
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LyudPeople::FWorld* S = Life ? Life->GetSim() : nullptr;
	const ULyudLetters* Letters = ULyudLetters::Get(this);
	TArray<TPair<int32, FString>> Order;
	for (const auto& KV : Model->Bonds)
	{
		if (Model->Level(KV.first) >= 1)
		{
			Order.Add({ KV.second.Points, Str(KV.first) });
		}
	}
	Order.Sort([](const TPair<int32, FString>& A, const TPair<int32, FString>& B) { return A.Key > B.Key; });
	auto Fact = [](const TCHAR* Key, const FString& Arg) { return LyudText::Get(Key, { FStringFormatArg(Arg) }); };
	for (const TPair<int32, FString>& O : Order)
	{
		const LyudFriends::FBond& B = *Model->Find(Std(O.Value));
		FDiaryFriend F;
		F.Name = NameOf(O.Value);
		F.Level = LyudText::Get(FString::Printf(TEXT("friends.level.%d"), Model->Level(Std(O.Value))));
		const int32 Id = PersonOf(O.Value);
		if (S && Id >= 0 && Id < (int32)S->People.size())
		{
			const LyudPeople::FPerson& P = S->People[(size_t)Id];
			const auto Job = Model->Data.Jobs.find(P.Occupation);
			if (Job != Model->Data.Jobs.end())
			{
				F.Facts.Add(Fact(TEXT("friends.fact.job"), LyudText::Get(Str(Job->second)).ToString()));
			}
			const FText Address = Letters ? Letters->HomeAddress(Str(P.HomeId)) : FText::GetEmpty();
			if (!Address.IsEmpty())
			{
				F.Facts.Add(Fact(TEXT("friends.fact.home"), Address.ToString()));
			}
			// the family: the others of the household
			FString Family;
			if (!P.Household.empty())
			{
				for (const LyudPeople::FPerson& Q : S->People)
				{
					if (Q.Id != P.Id && Q.Household == P.Household && Q.bTown)
					{
						Family += (Family.IsEmpty() ? TEXT("") : TEXT(", ")) + Life->GetDisplayName(Q.Id).ToString();
					}
				}
			}
			if (!Family.IsEmpty())
			{
				F.Facts.Add(Fact(TEXT("friends.fact.family"), Family));
			}
			if (B.Outings > 0 && P.OutingDay >= 0)
			{
				const int32 Hh = (int32)P.OutingHour;
				F.Facts.Add(Fact(TEXT("friends.fact.outing"), FString::Printf(TEXT("%s %d:%02d"),
					*LyudText::Get(FString::Printf(TEXT("friends.day.in.%d"), P.OutingDay)).ToString(), Hh, (int32)((P.OutingHour - (float)Hh) * 60.f + 0.5f))));
			}
		}
		if (!B.Nick.empty())
		{
			F.Facts.Add(Fact(TEXT("friends.fact.nick"), LyudText::Get(TEXT("friends.nick.") + Str(B.Nick)).ToString()));
		}
		if (!B.Loves.empty())
		{
			FString Likes;
			for (const std::string& L : B.Loves)
			{
				Likes += (Likes.IsEmpty() ? TEXT("") : TEXT(", ")) + ULyudInventory::ItemName(FName(*Str(L))).ToString();
			}
			F.Facts.Add(Fact(TEXT("friends.fact.likes"), Likes));
		}
		if (B.Cards > 0)
		{
			F.Facts.Add(Fact(TEXT("friends.fact.cards"), FString::FromInt(B.Cards)));
		}
		if (B.Teas > 0)
		{
			F.Facts.Add(Fact(TEXT("friends.fact.teas"), FString::FromInt(B.Teas)));
		}
		if (B.FirstDay >= 0)
		{
			const FDateTime When = FDateTime(1970, 1, 1) + FTimespan::FromDays(B.FirstDay);
			F.Facts.Add(Fact(TEXT("friends.fact.met"), FText::AsDate(When, EDateTimeStyle::Long, FText::GetInvariantTimeZone()).ToString()));
		}
		Out.Add(MoveTemp(F));
	}
}

// --- console and developer mode ------------------------------------------------------------------------------------------

FString ULyudFriends::Report() const
{
	if (!bLoaded || !Model)
	{
		return TEXT("Friends: not loaded");
	}
	FString Out = FString::Printf(TEXT("Friends: %d bonds\n"), (int32)Model->Bonds.size());
	for (const auto& KV : Model->Bonds)
	{
		const LyudFriends::FBond& B = KV.second;
		Out += FString::Printf(TEXT("  %s: level %d (%d points), nick %s, cards %d, gifts %d, teas %d, benches %d, outings %d, tea day %d, outing day %d\n"),
			*Str(KV.first), Model->Level(KV.first), B.Points, B.Nick.empty() ? TEXT("-") : *Str(B.Nick), B.Cards, B.Gifts, B.Teas, B.Benches, B.Outings,
			B.TeaDay, B.OutingDay);
	}
	return Out;
}

bool ULyudFriends::DevBefriend(const FString& Key)
{
	if (!bLoaded || Key.IsEmpty())
	{
		return false;
	}
	Model->DevBefriend(Std(Key), Today());
	Grew(Key, 1);
	return true;
}

bool ULyudFriends::DevBefriendNearest()
{
	const ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this);
	AActor* Actor = nullptr;
	const int32 Id = Talk ? Talk->NearestSpeaker(Actor) : -1;
	return Id >= 0 && DevBefriend(KeyOf(Id));
}

bool ULyudFriends::DevTeaNow(const FString& HostId)
{
	const LyudFriends::FHost* H = bLoaded ? Model->Data.HostById(Std(HostId)) : nullptr;
	float Hour = 12.f;
	const int32 Day = Today(&Hour);
	return H && Model->DevTeaNow(H->Key(), Day, (int32)Hour);
}

void ULyudFriends::DevInvites()
{
	if (!bLoaded)
	{
		return;
	}
	for (auto& KV : Model->Bonds)
	{
		KV.second.TeaLast = -1000;
		KV.second.OutingLast = -1000;
	}
	StepInvites();
}

bool ULyudFriends::TeaStand(const FString& HostId, FTransform& Out) const
{
	const LyudFriends::FHost* H = bLoaded ? Model->Data.HostById(Std(HostId)) : nullptr;
	if (!H)
	{
		return false;
	}
	const FVector Chair(H->ChairAt[0], H->ChairAt[1], H->ChairAt[2]);
	const FVector Back = -FRotator(0.f, H->ChairYaw, 0.f).Vector();
	Out = FTransform(FRotator(0.f, H->ChairYaw, 0.f), Chair + Back * 70.0);
	return true;
}

namespace LyudFriendsConsole
{
	ULyudFriends* Of(UWorld* World) { return World ? World->GetSubsystem<ULyudFriends>() : nullptr; }

	FAutoConsoleCommandWithWorldAndArgs GReport(TEXT("lyud.Friends.Report"), TEXT("The hero's friends: every bond, its level and what made it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudFriends* F = Of(World)) { UE_LOG(LogLyudFriends, Display, TEXT("%s"), *F->Report()); }
		}));
	FAutoConsoleCommandWithWorldAndArgs GBefriend(TEXT("lyud.Friends.Befriend"), TEXT("lyud.Friends.Befriend [key]: the nearest townsperson (or key) becomes a close friend."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ULyudFriends* F = Of(World);
			const bool bOk = F && (Args.Num() > 0 ? F->DevBefriend(Args[0]) : F->DevBefriendNearest());
			UE_LOG(LogLyudFriends, Display, TEXT("Friends: befriend %s"), bOk ? TEXT("done") : TEXT("nobody"));
		}));
	FAutoConsoleCommandWithWorldAndArgs GTea(TEXT("lyud.Friends.Tea"), TEXT("lyud.Friends.Tea <host>: an invitation to tea for now (holm, okada, tadesse, ...)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ULyudFriends* F = Of(World);
			const bool bOk = F && Args.Num() > 0 && F->DevTeaNow(Args[0]);
			UE_LOG(LogLyudFriends, Display, TEXT("Friends: tea %s"), bOk ? TEXT("is on: sit at their table") : TEXT("refused"));
		}));
	FAutoConsoleCommandWithWorldAndArgs GInvites(TEXT("lyud.Friends.Invites"), TEXT("Every invitation that is due now (the waits forgotten)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudFriends* F = Of(World)) { F->DevInvites(); }
		}));
}

#if LYUD_FRIENDS_DEV
namespace LyudFriendsDev
{
	using namespace LyudDev;
	FAutoAction GBefriendAction(TEXT("friends.befriend"), TEXT("friends"), TEXT("dev.action.friends.befriend"),
		[](UWorld* W) { if (ULyudFriends* F = LyudFriendsConsole::Of(W)) { F->DevBefriendNearest(); } });
	FAutoAction GTeaAction(TEXT("friends.tea_now"), TEXT("friends"), TEXT("dev.action.friends.tea_now"),
		[](UWorld* W) { if (ULyudFriends* F = LyudFriendsConsole::Of(W)) { F->DevTeaNow(TEXT("tadesse")); } });
	FAutoAction GInvitesAction(TEXT("friends.invite_all"), TEXT("friends"), TEXT("dev.action.friends.invite_all"),
		[](UWorld* W) { if (ULyudFriends* F = LyudFriendsConsole::Of(W)) { F->DevInvites(); } });
	FAutoAction GReportAction(TEXT("friends.report"), TEXT("friends"), TEXT("dev.action.friends.report"),
		[](UWorld* W) { if (ULyudFriends* F = LyudFriendsConsole::Of(W)) { UE_LOG(LogLyudFriends, Display, TEXT("%s"), *F->Report()); } });
	FAutoTeleport GTeaTp(TEXT("friends_tea_tadesse"), TEXT("friends"), TEXT("dev.tp.friends_tea_tadesse"), [](UWorld* W, FTransform& Out)
	{
		const ULyudFriends* F = LyudFriendsConsole::Of(W);
		return F && F->TeaStand(TEXT("tadesse"), Out);
	});
}
#endif
