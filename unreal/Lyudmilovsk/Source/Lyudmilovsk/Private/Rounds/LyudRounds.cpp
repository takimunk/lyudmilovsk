// The bicycle rounds: the morning papers to the doorsteps of a terrace, the post's parcels to the flats (docs/rounds.md).

#include "Rounds/LyudRounds.h"

#include "Bikes/HeroBike.h"
#include "City/LyudCityThings.h"
#include "Components/StaticMeshComponent.h"
#include "Dev/LyudDev.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Fishing/LyudFishing.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kite/LyudKite.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NpcDialogue.h"
#include "People/PeopleLife.h"
#include "Plants/GameCalendar.h"
#include "Rounds/RoundsModel.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Smoking/LyudSmoking.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudRounds, Log, All);

namespace LyudRounds
{
	/** Content/City/rounds.json, read once. */
	struct FData
	{
		FVector Bundle = FVector::ZeroVector;
		double PaperFrom = 6.0, PaperUntil = 7.5, PaperDeadline = 8.0;
		int Spare = 4;
		std::vector<std::vector<FStop>> Routes;  // per terrace route
		std::vector<int> RouteTerrace;
		FString Postman;
		FStop PostmanDoor;
		double PostFrom = 9.0, PostUntil = 12.0, PostDeadline = 14.0;
		TArray<int32> PostDays;  // 1 Monday .. 7 Sunday
		int Parcels = 5;
		std::vector<FStop> Post;
	};
}

namespace LyudRoundsImpl
{
	using namespace LyudRounds;

	FVector At(const FStop& S) { return FVector(S.X, S.Y, S.Z); }

	FStop ReadStop(const TSharedPtr<FJsonObject>& O)
	{
		FStop S;
		if (!O.IsValid())
		{
			return S;
		}
		S.Id = TCHAR_TO_UTF8(*O->GetStringField(TEXT("id")));
		S.Person = TCHAR_TO_UTF8(*O->GetStringField(TEXT("person")));
		const FString T = O->GetStringField(TEXT("terrace"));
		S.Terrace = T.Len() > 1 ? FCString::Atoi(*T.Mid(1)) : 0;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O->TryGetArrayField(TEXT("at"), A) && A->Num() == 3)
		{
			S.X = (*A)[0]->AsNumber();
			S.Y = (*A)[1]->AsNumber();
			S.Z = (*A)[2]->AsNumber();
		}
		S.Yaw = O->GetNumberField(TEXT("yaw"));
		return S;
	}

	FVector ReadVec(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Field, A) && A->Num() == 3)
		{
			return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
		}
		return FVector::ZeroVector;
	}

	/** How near the hero must be: to take the bundle or ring (cm), and to see a door's prompt for a throw. */
	constexpr double ReachCm = 260.0;
	constexpr double DoorNearCm = 2500.0;
	constexpr float PaperLife = 45.f;
	constexpr float ThrowSpeed = 1050.f;
	constexpr float Gravity = 980.f;
}

ULyudRounds::ULyudRounds() = default;
ULyudRounds::~ULyudRounds() = default;

ULyudRounds* ULyudRounds::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudRounds>() : nullptr;
}

void ULyudRounds::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Data = MakePimpl<LyudRounds::FData>();
	Round = MakePimpl<LyudRounds::FRound>();
	Round->bOver = true;
	bLoaded = LoadData();
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("Rounds"), this);
		NewGameHandle = Saves->OnNewGame.AddWeakLambda(this, [this] { ResetAll(); });
	}
}

void ULyudRounds::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(World))
		{
			Saves->OnNewGame.Remove(NewGameHandle);
			Saves->UnregisterSaveObject(TEXT("Rounds"));
		}
		if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(World))
		{
			UI->SetPrompt(this, FText::GetEmpty());
		}
	}
	if (Holder)
	{
		Holder->Destroy();
		Holder = nullptr;
	}
	PaperMeshes.Reset();
	Super::Deinitialize();
}

void ULyudRounds::ResetAll()
{
	PaperDay = PostDay = -1;
	PapersDelivered = ParcelsHanded = RoublesEarned = 0;
	if (Round)
	{
		Round->bOver = true;
	}
}

bool ULyudRounds::LoadData()
{
	using namespace LyudRoundsImpl;
	FString Text;
	const FString Path = FPaths::ProjectContentDir() / TEXT("City/rounds.json");
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		UE_LOG(LogLyudRounds, Display, TEXT("Rounds: no %s: no rounds"), *Path);
		return false;
	}
	LyudRounds::FData& D = *Data;
	const TSharedPtr<FJsonObject> Paper = Root->GetObjectField(TEXT("paper"));
	const TSharedPtr<FJsonObject> Post = Root->GetObjectField(TEXT("post"));
	if (!Paper.IsValid() || !Post.IsValid())
	{
		return false;
	}
	D.Bundle = ReadVec(Paper, TEXT("bundle"));
	D.PaperFrom = Paper->GetNumberField(TEXT("from"));
	D.PaperUntil = Paper->GetNumberField(TEXT("until"));
	D.PaperDeadline = Paper->GetNumberField(TEXT("deadline"));
	D.Spare = (int)Paper->GetNumberField(TEXT("spare"));
	for (const TSharedPtr<FJsonValue>& R : Paper->GetArrayField(TEXT("routes")))
	{
		const TSharedPtr<FJsonObject> RO = R->AsObject();
		std::vector<LyudRounds::FStop> Stops;
		for (const TSharedPtr<FJsonValue>& S : RO->GetArrayField(TEXT("stops")))
		{
			Stops.push_back(ReadStop(S->AsObject()));
		}
		const FString T = RO->GetStringField(TEXT("terrace"));
		D.RouteTerrace.push_back(T.Len() > 1 ? FCString::Atoi(*T.Mid(1)) : 0);
		D.Routes.push_back(MoveTemp(Stops));
	}
	D.Postman = Post->GetStringField(TEXT("postman"));
	D.PostmanDoor = ReadStop(Post->GetObjectField(TEXT("door")));
	D.PostFrom = Post->GetNumberField(TEXT("from"));
	D.PostUntil = Post->GetNumberField(TEXT("until"));
	D.PostDeadline = Post->GetNumberField(TEXT("deadline"));
	D.Parcels = (int)Post->GetNumberField(TEXT("parcels"));
	for (const TSharedPtr<FJsonValue>& V : Post->GetArrayField(TEXT("days")))
	{
		D.PostDays.Add((int32)V->AsNumber());
	}
	for (const TSharedPtr<FJsonValue>& S : Post->GetArrayField(TEXT("stops")))
	{
		D.Post.push_back(ReadStop(S->AsObject()));
	}
	UE_LOG(LogLyudRounds, Display, TEXT("Rounds: %d paper routes, %d post flats, postman %s"), (int32)D.Routes.size(), (int32)D.Post.size(), *D.Postman);
	return !D.Routes.empty();
}

int32 ULyudRounds::Today() const
{
	const UGameCalendar* Cal = GetWorld() ? GetWorld()->GetSubsystem<UGameCalendar>() : nullptr;
	if (!Cal)
	{
		return 0;
	}
	return (int32)(Cal->Now().GetDate() - FDateTime(2026, 10, 20)).GetDays();
}

double ULyudRounds::HourNow() const
{
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	return Now.GetHour() + Now.GetMinute() / 60.0 + Now.GetSecond() / 3600.0;
}

bool ULyudRounds::IsActive() const { return Round && !Round->bOver; }
bool ULyudRounds::IsPaper() const { return IsActive() && Round->Kind == LyudRounds::EKind::Paper; }
FVector ULyudRounds::BundleAt() const { return bLoaded ? Data->Bundle : FVector::ZeroVector; }
FVector ULyudRounds::PostmanDoorAt() const { return bLoaded ? LyudRoundsImpl::At(Data->PostmanDoor) : FVector::ZeroVector; }

void ULyudRounds::GetMarks(TArray<FLyudRoundMark>& Out) const
{
	Out.Reset();
	if (!IsActive())
	{
		return;
	}
	const LyudRounds::FRound& R = *Round;
	for (int32 I = 0; I < (int32)R.Stops.size(); ++I)
	{
		const LyudRounds::FStop& S = R.Kind == LyudRounds::EKind::Paper ? Data->Routes[(size_t)R.Route][(size_t)R.Stops[(size_t)I]]
			: Data->Post[(size_t)R.Stops[(size_t)I]];
		FLyudRoundMark M;
		M.At = LyudRoundsImpl::At(S);
		M.bDone = R.Results[(size_t)I] != LyudRounds::EResult::Pending;
		M.bWell = R.Results[(size_t)I] == LyudRounds::EResult::Good || R.Results[(size_t)I] == LyudRounds::EResult::Perfect;
		Out.Add(M);
	}
}

// ------------------------------------------------------------------------------------------------------------- starting

bool ULyudRounds::StartPaper(bool bForce)
{
	if (!bLoaded || IsActive())
	{
		return false;
	}
	const int32 Day = Today();
	const double H = HourNow();
	ULyudGameScreen* GS = ULyudGameScreen::Get(this);
	if (!bForce && (PaperDay == Day || H < Data->PaperFrom || H > Data->PaperUntil))
	{
		return false;
	}
	const int Route = LyudRounds::PaperRouteFor(Day, (int)Data->Routes.size());
	std::vector<int> Entries;
	for (int I = 0; I < (int)Data->Routes[(size_t)Route].size(); ++I)
	{
		Entries.push_back(I);
	}
	const double Deadline = bForce ? H + (Data->PaperDeadline - Data->PaperFrom) : Data->PaperDeadline;
	Round->Begin(LyudRounds::EKind::Paper, Route, Entries, (int)Entries.size() + Data->Spare, H, Deadline);
	RoundTerrace = Data->RouteTerrace[(size_t)Route];
	PaperDay = Day;
	Prompted = -1;  // the round's first prompt is always said (its code may equal the one showing)
	if (GS)
	{
		GS->Toast(LyudText::Get(TEXT("rounds.paper.start"), { FStringFormatArg(RoundTerrace), FStringFormatArg((int32)Entries.size()),
			FStringFormatArg(Round->Papers) }), true);
	}
	ULyudQuests::Notify(this, TEXT("round_start"), TEXT("paper"));
	UE_LOG(LogLyudRounds, Display, TEXT("Rounds: paper round on T%d, %d doors, %d papers, by %.2f"), RoundTerrace, (int32)Entries.size(), Round->Papers, Deadline);
	return true;
}

bool ULyudRounds::StartPost(bool bForce)
{
	if (!bLoaded || IsActive() || Data->Post.empty())
	{
		return false;
	}
	const int32 Day = Today();
	const double H = HourNow();
	const int32 Weekday = (int32)UGameCalendar::NowIn(GetWorld()).GetDayOfWeek() + 1;  // EDayOfWeek: Monday = 0
	if (!bForce && (PostDay == Day || H < Data->PostFrom || H > Data->PostUntil || !Data->PostDays.Contains(Weekday)))
	{
		return false;
	}
	const std::vector<int> Picks = LyudRounds::PostPicks(Day, (int)Data->Post.size(), Data->Parcels);
	const double Deadline = bForce ? H + (Data->PostDeadline - Data->PostFrom) : Data->PostDeadline;
	Round->Begin(LyudRounds::EKind::Post, -1, Picks, 0, H, Deadline);
	RoundTerrace = 0;
	PostDay = Day;
	Prompted = -1;
	if (ULyudGameScreen* GS = ULyudGameScreen::Get(this))
	{
		GS->Toast(LyudText::Get(TEXT("rounds.post.start"), { FStringFormatArg((int32)Picks.size()) }), true);
	}
	ULyudQuests::Notify(this, TEXT("round_start"), TEXT("post"));
	UE_LOG(LogLyudRounds, Display, TEXT("Rounds: post round, %d parcels, by %.2f"), (int32)Picks.size(), Deadline);
	return true;
}

void ULyudRounds::EndRound()
{
	if (IsActive())
	{
		Finish(false);
	}
}

void ULyudRounds::Finish(bool bInTime)
{
	LyudRounds::FRound& R = *Round;
	if (R.bOver)
	{
		return;
	}
	const double H = HourNow();
	const int32 Pay = R.Pay(bInTime ? FMath::Min(H, R.Deadline) : R.Deadline + 1.0);
	const std::string Stamp = R.Stamp(RoundTerrace);
	const bool bPaper = R.Kind == LyudRounds::EKind::Paper;
	R.bOver = true;
	if (bPaper)
	{
		PapersDelivered += R.Count(LyudRounds::EResult::Good) + R.Count(LyudRounds::EResult::Perfect);
	}
	else
	{
		ParcelsHanded += R.Count(LyudRounds::EResult::Good) + R.Count(LyudRounds::EResult::Perfect);
	}
	RoublesEarned += Pay;
	if (Pay > 0)
	{
		if (ULyudInventory* Inv = ULyudInventory::Get(this))
		{
			Inv->Earn(Pay);
		}
	}
	if (ULyudGameScreen* GS = ULyudGameScreen::Get(this))
	{
		GS->Toast(LyudText::Get(bPaper ? TEXT("rounds.paper.done") : TEXT("rounds.post.done"),
			{ FStringFormatArg(R.Count(LyudRounds::EResult::Perfect)), FStringFormatArg(R.Count(LyudRounds::EResult::Good)),
			  FStringFormatArg(R.Count(LyudRounds::EResult::Missed)), FStringFormatArg(Pay) }), true);
	}
	if (!Stamp.empty())
	{
		if (ULyudCityThings* Things = ULyudCityThings::Get(this))
		{
			Things->GiveStamp(UTF8_TO_TCHAR(Stamp.c_str()));
		}
	}
	if (R.Left() == 0)
	{
		ULyudQuests::Notify(this, TEXT("round_done"), bPaper ? TEXT("paper") : TEXT("post"));
		if (bPaper && R.Hit() >= 0.75)
		{
			ULyudQuests::Notify(this, TEXT("round_done"), FString::Printf(TEXT("t%d"), RoundTerrace));
		}
	}
	RingingEntry = -1;
	SetPrompt(-2, FText::GetEmpty());
	UE_LOG(LogLyudRounds, Display, TEXT("Rounds: %s round over: %d on the step, %d near, %d missed, %d left, %d stray, paid %d%s"),
		bPaper ? TEXT("paper") : TEXT("post"), R.Count(LyudRounds::EResult::Perfect), R.Count(LyudRounds::EResult::Good),
		R.Count(LyudRounds::EResult::Missed), R.Left(), R.Stray, Pay, Stamp.empty() ? TEXT("") : *FString::Printf(TEXT(", stamp %s"), UTF8_TO_TCHAR(Stamp.c_str())));
}

// ---------------------------------------------------------------------------------------------------------------- the tick

void ULyudRounds::SetPrompt(int32 What, const FText& Text)
{
	if (What == Prompted)
	{
		return;
	}
	Prompted = What;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, Text);
	}
}

void ULyudRounds::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!bLoaded || !World || !World->IsGameWorld())
	{
		return;
	}
	TickFlights(DeltaTime);
	if (IsActive() && HourNow() > Round->Deadline + 1.0)
	{
		Finish(false);  // an hour past the deadline: the round is over, paid for what was done
	}
	TickDoors();
}

void ULyudRounds::TickDoors()
{
	using namespace LyudRoundsImpl;
	UWorld* World = GetWorld();
	APlayerController* PC = World->GetFirstPlayerController();
	AHeroCharacter* Hero = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	if (!Hero || Hero->IsHidden() || (UI && UI->IsOpen()))
	{
		SetPrompt(-2, FText::GetEmpty());
		return;
	}
	const FVector HeroAt = Hero->GetActorLocation();
	auto Near = [&HeroAt](const FVector& P, double R) { return FVector::Dist2D(HeroAt, P) < R && FMath::Abs(HeroAt.Z - P.Z) < 300.0; };
	const double H = HourNow();
	const int32 Day = Today();

	// a door rung: the wait, then whoever is in opens
	if (RingingEntry >= 0)
	{
		if ((RingWait -= World->GetDeltaSeconds()) <= 0.f)
		{
			Answer(RingingEntry);
			RingingEntry = -1;
		}
		return;
	}

	if (!IsActive())
	{
		// the bundle on the kiosk's step, the postman's door
		if (Near(Data->Bundle, ReachCm) && PaperDay != Day && H >= Data->PaperFrom && H <= Data->PaperUntil)
		{
			if (Prompted != -10)
			{
				SetPrompt(-10, LyudText::Get(TEXT("rounds.prompt.bundle")));
			}
			if (ANpcTalkDirector::UsePressed(PC))
			{
				StartPaper(false);
			}
			return;
		}
		const int32 Weekday = (int32)UGameCalendar::NowIn(World).GetDayOfWeek() + 1;
		if (Near(At(Data->PostmanDoor), ReachCm) && PostDay != Day && H >= Data->PostFrom && H <= Data->PostUntil && Data->PostDays.Contains(Weekday))
		{
			if (Prompted != -11)
			{
				SetPrompt(-11, LyudText::Get(TEXT("rounds.prompt.postman")));
			}
			if (ANpcTalkDirector::UsePressed(PC))
			{
				if (StartPost(false))
				{
					if (UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this))
					{
						const int32 Id = Life->FindPerson(Data->Postman);
						const FText Name = Id >= 0 ? Life->GetDisplayName(Id) : LyudText::Get(TEXT("rounds.postman"));
						if (ULyudGameScreen* GS = ULyudGameScreen::Get(this))
						{
							GS->Toast(LyudText::Get(FString::Printf(TEXT("rounds.postman.give.%d"), 1 + Day % 3), { FStringFormatArg(Name.ToString()) }));
						}
					}
				}
			}
			return;
		}
		SetPrompt(-2, FText::GetEmpty());
		return;
	}

	LyudRounds::FRound& R = *Round;
	if (R.Kind == LyudRounds::EKind::Paper)
	{
		// the papers: the left button throws (not with a rod, a kite or a cigarette in hand, nor as a bird)
		const bool bHandsFree = !ULyudFishing::HasHand(this) && !ULyudKiteFlying::HasHand(this) && !ULyudSmoking::HasHand(this);
		const ULyudInventory* Inv = ULyudInventory::Get(this);
		const bool bEmptyHand = !Inv || Inv->EquippedItem().IsNone();
		if (R.Papers > 0 && bHandsFree && bEmptyHand && !PC->IsLookInputIgnored() && PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
		{
			Throw();
		}
		// the prompt: the nearest waiting door
		int32 Best = -1;
		double BestD = DoorNearCm;
		for (int32 I = 0; I < (int32)R.Stops.size(); ++I)
		{
			if (R.Results[(size_t)I] != LyudRounds::EResult::Pending)
			{
				continue;
			}
			const double D = FVector::Dist2D(HeroAt, At(Data->Routes[(size_t)R.Route][(size_t)R.Stops[(size_t)I]]));
			if (D < BestD)
			{
				BestD = D;
				Best = I;
			}
		}
		// the words are looked up and filled only when the prompt changes, not every frame of the round
		const int32 PaperPrompt = Best >= 0 ? 100 + R.Papers * 1000 : -3 - R.Papers;
		if (Prompted != PaperPrompt)
		{
			SetPrompt(PaperPrompt, Best >= 0
				? LyudText::Get(TEXT("rounds.prompt.throw"), { FStringFormatArg(R.Papers) })
				: LyudText::Get(TEXT("rounds.prompt.papers"), { FStringFormatArg(R.Papers), FStringFormatArg(R.Left()) }));
		}
		if (R.AllDone() && !Flights[0].bFlying && !Flights[1].bFlying && !Flights[2].bFlying && !Flights[3].bFlying && !Flights[4].bFlying && !Flights[5].bFlying)
		{
			Finish(H <= R.Deadline);
		}
		return;
	}

	// the post: E at a parcel's door rings
	for (int32 I = 0; I < (int32)R.Stops.size(); ++I)
	{
		if (R.Results[(size_t)I] != LyudRounds::EResult::Pending)
		{
			continue;
		}
		const LyudRounds::FStop& S = Data->Post[(size_t)R.Stops[(size_t)I]];
		if (!Near(At(S), ReachCm))
		{
			continue;
		}
		// who lives here is looked up only when the prompt is first said and when the bell is pressed, not every frame at the door
		const bool bPressed = ANpcTalkDirector::UsePressed(PC);
		if (Prompted == 200 + I && !bPressed)
		{
			return;
		}
		const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
		const int32 Id = Life ? Life->FindPerson(UTF8_TO_TCHAR(S.Person.c_str())) : -1;
		if (Prompted != 200 + I)
		{
			const FText Name = Id >= 0 && Life ? Life->GetDisplayName(Id) : FText::GetEmpty();
			SetPrompt(200 + I, LyudText::Get(TEXT("rounds.prompt.ring"), { FStringFormatArg(Name.ToString()) }));
		}
		if (bPressed)
		{
			// the bell, and a moment's wait: the nervy are at the door at once, a sleeper takes a while
			const float Nerve = Life && Id >= 0 ? Life->GetTrait(Id, TEXT("nerve")) : 0.5f;
			const int32 Sleep = Life ? Life->FindActivity(TEXT("sleep")) : -1;
			int32 Act = -1;
			bool bDoing = false;
			EPersonPose Pose = EPersonPose::Stand;
			FVector Where;
			const bool bAsleep = Life && Id >= 0 && Life->GetActivityState(Id, Act, bDoing, Pose, Where)
				&& (Pose == EPersonPose::Sleep || (Act == Sleep && Sleep >= 0));
			RingingEntry = I;
			RingWait = (float)LyudRounds::WaitSeconds(Nerve, bAsleep, Day * 31 + I);
			SetPrompt(-4, LyudText::Get(TEXT("rounds.prompt.rang")));
		}
		return;
	}
	if (const int32 ParcelPrompt = -5 - R.Left(); Prompted != ParcelPrompt)
	{
		SetPrompt(ParcelPrompt, LyudText::Get(TEXT("rounds.prompt.parcels"), { FStringFormatArg(R.Left()) }));
	}
	if (R.AllDone())
	{
		Finish(H <= R.Deadline);
	}
}

void ULyudRounds::Answer(int32 Entry)
{
	if (!IsActive() || Round->Kind != LyudRounds::EKind::Post)
	{
		return;
	}
	LyudRounds::FRound& R = *Round;
	const LyudRounds::FStop& S = Data->Post[(size_t)R.Stops[(size_t)Entry]];
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const int32 Id = Life ? Life->FindPerson(UTF8_TO_TCHAR(S.Person.c_str())) : -1;
	// in: they are at home (within the flat's reach of its door), whatever they are doing there
	int32 Act = -1;
	bool bDoing = false;
	EPersonPose Pose = EPersonPose::Stand;
	FVector Where = FVector::ZeroVector;
	const bool bKnown = Life && Id >= 0 && Life->GetActivityState(Id, Act, bDoing, Pose, Where);
	const bool bIn = bKnown && FVector::Dist2D(Where, LyudRoundsImpl::At(S)) < 2500.0 && Pose != EPersonPose::Hidden;
	const int32 Day = Today();
	ULyudGameScreen* GS = ULyudGameScreen::Get(this);
	if (!bIn)
	{
		R.Set(Entry, LyudRounds::EResult::Missed);
		if (GS)
		{
			GS->Toast(LyudText::Get(FString::Printf(TEXT("rounds.nobody.%d"), 1 + (Day + Entry) % 3)));
		}
		Prompted = -1;  // said again next frame (not -2: that means "nothing shows", and a clear would then be skipped)
		return;
	}
	const int32 Sleep = Life->FindActivity(TEXT("sleep"));
	const char* Set = LyudRounds::AnswerSet(Life->GetTrait(Id, TEXT("warmth")), Life->GetTrait(Id, TEXT("nerve")),
		Pose == EPersonPose::Sleep || (Act == Sleep && Sleep >= 0));
	R.Set(Entry, LyudRounds::EResult::Perfect);
	Life->NoteMet(Id);
	if (GS)
	{
		const FText Line = LyudText::Get(FString::Printf(TEXT("rounds.answer.%s.%d"), UTF8_TO_TCHAR(Set), 1 + (Day + Entry) % 3));
		GS->Toast(LyudText::Get(TEXT("rounds.answer.say"), { FStringFormatArg(Life->GetDisplayName(Id).ToString()), FStringFormatArg(Line.ToString()) }));
	}
	Prompted = -1;
}

// ---------------------------------------------------------------------------------------------------------- the papers flying

bool ULyudRounds::EnsureHolder()
{
	if (Holder)
	{
		return true;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	if (!PaperMesh)
	{
		PaperMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	}
	if (!PaperMat)
	{
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			PaperMat = UMaterialInstanceDynamic::Create(Base, this);
			PaperMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.78f, 0.76f, 0.7f));
		}
	}
	FActorSpawnParameters SP;
	SP.ObjectFlags |= RF_Transient;
	Holder = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SP);
	if (!Holder)
	{
		return false;
	}
	USceneComponent* RootC = NewObject<USceneComponent>(Holder, TEXT("Root"));
	Holder->SetRootComponent(RootC);
	RootC->RegisterComponent();
	for (int32 I = 0; I < MaxFlights; ++I)
	{
		UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(Holder);
		M->SetStaticMesh(PaperMesh);
		if (PaperMat)
		{
			M->SetMaterial(0, PaperMat);
		}
		M->SetMobility(EComponentMobility::Movable);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCastShadow(false);
		M->bAffectDynamicIndirectLighting = false;
		M->bAffectDistanceFieldLighting = false;
		// a folded newspaper rolled for throwing: 7 cm across, 28 cm long (the basic cylinder is 100 x 100 cm)
		M->SetWorldScale3D(FVector(0.07f, 0.07f, 0.28f));
		M->SetVisibility(false);
		M->SetupAttachment(RootC);
		M->RegisterComponent();
		PaperMeshes.Add(M);
	}
	return true;
}

void ULyudRounds::Throw()
{
	if (!EnsureHolder() || !IsPaper() || Round->Papers <= 0)
	{
		return;
	}
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	FVector Eye;
	FRotator View;
	PC->GetPlayerViewPoint(Eye, View);
	// from the right hand, along the view, a little up; with the speed he rides at
	const FVector Dir = View.Vector();
	FFlight& F = Flights[NextFlight];
	F.At = Eye + Dir * 40.f + FRotationMatrix(View).GetUnitAxis(EAxis::Y) * 25.f - FVector(0.f, 0.f, 25.f);
	F.Vel = Dir * LyudRoundsImpl::ThrowSpeed + FVector(0.f, 0.f, 180.f) + Pawn->GetVelocity();
	F.Spin = FRotator(FMath::FRandRange(-30.f, 30.f), View.Yaw, 90.f);
	F.Age = 0.f;
	F.bFlying = true;
	F.bLying = false;
	PaperMeshes[NextFlight]->SetWorldLocationAndRotation(F.At, F.Spin);
	PaperMeshes[NextFlight]->SetVisibility(true);
	NextFlight = (NextFlight + 1) % MaxFlights;
	--Round->Papers;
}

void ULyudRounds::TickFlights(float DeltaTime)
{
	if (!Holder)
	{
		return;
	}
	UWorld* World = GetWorld();
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(LyudPaper), false, Pawn);
	Q.AddIgnoredActor(Holder);
	for (int32 I = 0; I < MaxFlights; ++I)
	{
		FFlight& F = Flights[I];
		if (!F.bFlying && !F.bLying)
		{
			continue;
		}
		F.Age += DeltaTime;
		if (F.bLying)
		{
			if (F.Age > LyudRoundsImpl::PaperLife)
			{
				F.bLying = false;
				PaperMeshes[I]->SetVisibility(false);
			}
			continue;
		}
		const FVector From = F.At;
		F.Vel.Z -= LyudRoundsImpl::Gravity * DeltaTime;
		F.At += F.Vel * DeltaTime;
		F.Spin.Pitch += 720.f * DeltaTime;
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, From, F.At, ECC_Visibility, Q))
		{
			F.bFlying = false;
			F.bLying = true;
			F.Age = 0.f;
			const FVector Down = Hit.ImpactPoint + Hit.ImpactNormal * 3.5f;
			PaperMeshes[I]->SetWorldLocationAndRotation(Down, FRotator(0.f, F.Spin.Yaw, 90.f));
			Land(I, Hit.ImpactPoint);
		}
		else if (F.Age > 6.f)
		{
			F.bFlying = false;  // over the cliff or into the sea
			PaperMeshes[I]->SetVisibility(false);
			Land(I, F.At);
		}
		else
		{
			PaperMeshes[I]->SetWorldLocationAndRotation(F.At, F.Spin);
		}
	}
}

void ULyudRounds::Land(int32, const FVector& Where)
{
	if (!IsPaper())
	{
		return;
	}
	LyudRounds::FRound& R = *Round;
	// the waiting door it came down nearest (within reach of one), judged
	int32 Best = -1;
	double BestD = LyudRounds::ClaimCm;
	for (int32 I = 0; I < (int32)R.Stops.size(); ++I)
	{
		if (R.Results[(size_t)I] != LyudRounds::EResult::Pending)
		{
			continue;
		}
		const LyudRounds::FStop& S = Data->Routes[(size_t)R.Route][(size_t)R.Stops[(size_t)I]];
		const double D = FVector::Dist2D(Where, LyudRoundsImpl::At(S));
		if (D < BestD && FMath::Abs(Where.Z - S.Z) < 400.0)
		{
			BestD = D;
			Best = I;
		}
	}
	ULyudGameScreen* GS = ULyudGameScreen::Get(this);
	if (Best < 0)
	{
		++R.Stray;
		return;
	}
	const LyudRounds::FStop& S = Data->Routes[(size_t)R.Route][(size_t)R.Stops[(size_t)Best]];
	const LyudRounds::EResult Res = LyudRounds::JudgeThrow(S, Where.X, Where.Y, Where.Z);
	R.Set(Best, Res);
	if (GS)
	{
		GS->Toast(LyudText::Get(Res == LyudRounds::EResult::Perfect ? TEXT("rounds.throw.perfect") : Res == LyudRounds::EResult::Good
			? TEXT("rounds.throw.good") : TEXT("rounds.throw.missed"), { FStringFormatArg(R.Left()) }));
	}
	Prompted = -1;  // the count changed: said again next frame (-2 would make a clear be skipped and leave the old words up)
}

FString ULyudRounds::Report() const
{
	if (!bLoaded)
	{
		return TEXT("Rounds: no data");
	}
	FString S = FString::Printf(TEXT("Rounds: day %d, %.2f h; paper done day %d, post done day %d; %d papers, %d parcels, %d roubles in all"),
		Today(), HourNow(), PaperDay, PostDay, PapersDelivered, ParcelsHanded, RoublesEarned);
	if (IsActive())
	{
		S += FString::Printf(TEXT("; %s round: %d left, %d papers, %d stray, by %.2f"), IsPaper() ? TEXT("paper") : TEXT("post"),
			Round->Left(), Round->Papers, Round->Stray, Round->Deadline);
	}
	return S;
}

// ---------------------------------------------------------------------------------------------- the console and developer mode

static FAutoConsoleCommandWithWorldAndArgs GRoundsCmd(TEXT("lyud.Rounds"),
	TEXT("lyud.Rounds paper | post | end | status: the bicycle rounds now, whatever the hour (docs/rounds.md)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudRounds* R = ULyudRounds::Get(World);
		if (!R)
		{
			return;
		}
		const FString Cmd = Args.Num() > 0 ? Args[0].ToLower() : TEXT("status");
		if (Cmd == TEXT("paper"))
		{
			R->StartPaper(true);
		}
		else if (Cmd == TEXT("post"))
		{
			R->StartPost(true);
		}
		else if (Cmd == TEXT("end"))
		{
			R->EndRound();
		}
		UE_LOG(LogLyudRounds, Display, TEXT("%s"), *R->Report());
	}));

static LyudDev::FAutoAction GDevRoundsPaper(TEXT("rounds.paper"), TEXT("rounds"), TEXT("dev.action.rounds.paper"), [](UWorld* W)
{
	if (ULyudRounds* R = ULyudRounds::Get(W))
	{
		R->StartPaper(true);
	}
});

static LyudDev::FAutoAction GDevRoundsPost(TEXT("rounds.post"), TEXT("rounds"), TEXT("dev.action.rounds.post"), [](UWorld* W)
{
	if (ULyudRounds* R = ULyudRounds::Get(W))
	{
		R->StartPost(true);
	}
});

static LyudDev::FAutoAction GDevRoundsEnd(TEXT("rounds.end"), TEXT("rounds"), TEXT("dev.action.rounds.end"), [](UWorld* W)
{
	if (ULyudRounds* R = ULyudRounds::Get(W))
	{
		R->EndRound();
	}
});

static LyudDev::FAutoTeleport GDevRoundsKiosk(TEXT("rounds.kiosk"), TEXT("rounds"), TEXT("dev.tp.rounds.kiosk"), [](UWorld* W, FTransform& Out)
{
	const ULyudRounds* R = ULyudRounds::Get(W);
	if (!R || R->BundleAt().IsZero())
	{
		return false;
	}
	Out = FTransform(FRotator::ZeroRotator, R->BundleAt() + FVector(-250.f, 0.f, 100.f));
	return true;
});

static LyudDev::FAutoTeleport GDevRoundsPostman(TEXT("rounds.postman"), TEXT("rounds"), TEXT("dev.tp.rounds.postman"), [](UWorld* W, FTransform& Out)
{
	const ULyudRounds* R = ULyudRounds::Get(W);
	if (!R || R->PostmanDoorAt().IsZero())
	{
		return false;
	}
	Out = FTransform(FRotator::ZeroRotator, R->PostmanDoorAt() + FVector(0.f, 0.f, 100.f));
	return true;
});
