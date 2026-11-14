#include "UI/LyudQuestDirector.h"

#include "Birds/BirdFeeder.h"
#include "Birds/BirdFlightSubsystem.h"
#include "Birds/BirdLife.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "HeroReachComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudGoodsItem.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "NpcDialogue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "People/LyudBodyAnim.h"
#include "People/PeopleLife.h"
#include "People/TownTalk.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"
#include "UI/LyudQuests.h"

namespace QuestDirectorLocal
{
	/** The hero's feeders tell of their visitors only while he is this near (cm): waiting by the feeder is the point. */
	const float FeederWatchCm = 6000.f;
	/** A pickup is put out when the hero is this near (the room round it is there to lie on), cm. */
	const float PickupSpawnCm = 4000.f;
	/** ...and taken from this far, looking at it. */
	const float PickupReachCm = 190.f;

	FString FullKey(const FString& Key)
	{
		return Key.StartsWith(TEXT("line.")) ? Key : TEXT("line.") + Key;
	}

	TArray<FName> Names(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		TArray<FName> Out;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (O->TryGetArrayField(Field, Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				Out.Add(FName(*V->AsString()));
			}
		}
		return Out;
	}
}

ULyudQuestDirector* ULyudQuestDirector::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudQuestDirector>() : nullptr;
}

void ULyudQuestDirector::Load()
{
	Pickups.Reset();
	Listeners.Reset();
	Remarks.Reset();
	const FString Path = FPaths::ProjectContentDir() / TEXT("UI/quest_scenes.json");
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
	{
		UE_LOG(LogTemp, Warning, TEXT("QuestDirector: no Content/UI/quest_scenes.json (or it does not parse)"));
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Root->TryGetArrayField(TEXT("pickups"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FPickup P;
			P.Id = FName(*O->GetStringField(TEXT("id")));
			P.Item = FName(*O->GetStringField(TEXT("item")));
			P.Quest = FName(*O->GetStringField(TEXT("quest")));
			P.Stages = QuestDirectorLocal::Names(O, TEXT("stages"));
			// Unreal metres, as people_places.json: the floor under it; it is dropped from drop_m above that floor
			P.At = FVector(O->GetNumberField(TEXT("ux")), O->GetNumberField(TEXT("uy")), O->GetNumberField(TEXT("floor_uz"))) * 100.0;
			double Drop = 1.5, Yaw = 0.0;
			O->TryGetNumberField(TEXT("drop_m"), Drop);
			O->TryGetNumberField(TEXT("yaw"), Yaw);
			P.DropCm = (float)Drop * 100.f;
			P.Yaw = (float)Yaw;
			if (ULyudInventory::FindDef(P.Item))
			{
				Pickups.Add(P);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("QuestDirector: pickup %s: no item %s"), *P.Id.ToString(), *P.Item.ToString());
			}
		}
	}
	if (Root->TryGetArrayField(TEXT("listeners"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FListener L;
			L.Person = O->GetStringField(TEXT("person"));
			double Within = 14.0, AirWithin = 45.0;
			O->TryGetNumberField(TEXT("within_m"), Within);
			O->TryGetNumberField(TEXT("air_within_m"), AirWithin);
			L.WithinCm = (float)Within * 100.f;
			L.AirWithinCm = (float)AirWithin * 100.f;
			const TArray<TSharedPtr<FJsonValue>>* Reactions = nullptr;
			if (O->TryGetArrayField(TEXT("reactions"), Reactions))
			{
				for (const TSharedPtr<FJsonValue>& RV : *Reactions)
				{
					const TSharedPtr<FJsonObject> RO = RV->AsObject();
					FReaction R;
					FString Word;
					if (RO->TryGetStringField(TEXT("species"), Word))
					{
						R.Species = FName(*Word);
					}
					if (RO->TryGetStringField(TEXT("quest"), Word))
					{
						R.Quest = FName(*Word);
					}
					if (RO->TryGetStringField(TEXT("stage"), Word))
					{
						R.Stage = FName(*Word);
					}
					RO->TryGetBoolField(TEXT("air"), R.bAir);
					RO->TryGetStringField(TEXT("diary"), R.Note);
					const TArray<TSharedPtr<FJsonValue>>* Lines = nullptr;
					if (RO->TryGetArrayField(TEXT("lines"), Lines))
					{
						for (const TSharedPtr<FJsonValue>& LV : *Lines)
						{
							R.Lines.Add(LV->AsString());
						}
					}
					L.Reactions.Add(MoveTemp(R));
				}
			}
			Listeners.Add(MoveTemp(L));
		}
	}
	if (Root->TryGetArrayField(TEXT("remarks"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FRemark R;
			R.Id = FName(*O->GetStringField(TEXT("id")));
			R.Person = O->GetStringField(TEXT("person"));
			R.Quest = FName(*O->GetStringField(TEXT("quest")));
			R.Stages = QuestDirectorLocal::Names(O, TEXT("stages"));
			double Within = 6.0;
			O->TryGetNumberField(TEXT("within_m"), Within);
			R.WithinCm = (float)Within * 100.f;
			O->TryGetBoolField(TEXT("at_once"), R.bAtOnce);
			O->TryGetStringField(TEXT("diary"), R.Note);
			for (const FName Line : QuestDirectorLocal::Names(O, TEXT("lines")))
			{
				R.Lines.Add(Line.ToString());
			}
			if (R.Lines.Num() > 0)
			{
				Remarks.Add(MoveTemp(R));
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("QuestDirector: %d pickups, %d listeners, %d remarks"), Pickups.Num(), Listeners.Num(), Remarks.Num());
}

void ULyudQuestDirector::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Load();
	if (UBirdFlightSubsystem* Flight = InWorld.GetSubsystem<UBirdFlightSubsystem>())
	{
		CallHandle = Flight->OnPlayerCall.AddUObject(this, &ULyudQuestDirector::HandleBirdCall);
	}
	bStarted = true;
}

void ULyudQuestDirector::Deinitialize()
{
	bStarted = false;
	if (UWorld* World = GetWorld())
	{
		if (UBirdFlightSubsystem* Flight = World->GetSubsystem<UBirdFlightSubsystem>())
		{
			Flight->OnPlayerCall.Remove(CallHandle);
		}
	}
	for (FPickup& P : Pickups)
	{
		if (P.Actor.IsValid() && !P.bTaking)
		{
			P.Actor->Destroy();
		}
	}
	Pickups.Reset();
	Speeches.Reset();
	Super::Deinitialize();
}

AActor* ULyudQuestDirector::PersonActor(int32 PersonId) const
{
	const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	FPersonInfo Info;
	if (!People || !People->GetPerson(PersonId, Info))
	{
		return nullptr;
	}
	AActor* A = Info.Actor.Get();
	return IsValid(A) && !A->IsHidden() ? A : nullptr;
}

void ULyudQuestDirector::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}
	SlowTimer -= DeltaTime;
	const bool bSlow = SlowTimer <= 0.f;
	if (bSlow)
	{
		SlowTimer = 1.f;
		TickFeeders(PC->GetPawn());
		TickRemarks(PC->GetPawn());
		// the hero is a bird: the voices of what the listeners may say to it are loaded ahead (in the background)
		if (PC->GetPawn() && !PC->GetPawn()->IsA<AHeroCharacter>())
		{
			for (const FListener& L : Listeners)
			{
				for (const FReaction& R : L.Reactions)
				{
					for (const FString& K : R.Lines)
					{
						LyudText::WarmVoice(this, QuestDirectorLocal::FullKey(K));
					}
				}
			}
		}
	}
	TickPickups(PC, bSlow);
	TickSpeech();
}

// ------------------------------------------------------------------ pickups

void ULyudQuestDirector::DropPickup(FPickup& P)
{
	UWorld* World = GetWorld();
	const FLyudItemDef* Def = ULyudInventory::FindDef(P.Item);
	if (!World || !Def)
	{
		return;
	}
	// onto whatever stands there (a desk), else the floor: the level's own things only, never somebody sitting at the desk
	FVector On = P.At;
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(QuestPickup), false);
	if (World->LineTraceSingleByObjectType(Hit, P.At + FVector(0.0, 0.0, P.DropCm), P.At - FVector(0.0, 0.0, 30.0), FCollisionObjectQueryParams(ECC_WorldStatic), Q))
	{
		On = Hit.ImpactPoint;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	ALyudGoodsItem* Goods = World->SpawnActor<ALyudGoodsItem>(On, FRotator::ZeroRotator, Params);
	if (!Goods)
	{
		return;
	}
	Goods->Init(P.Item);
	// lying on its back, the picture up, its top away along Yaw
	Goods->LieDown(On + FVector(0.0, 0.0, Goods->HalfDepth() + 0.2f), FRotator(0.f, P.Yaw, 0.f).Vector());
	P.Actor = Goods;
	P.bTaking = false;
	UE_LOG(LogTemp, Log, TEXT("QuestDirector: pickup %s (%s) lies at %s"), *P.Id.ToString(), *P.Item.ToString(), *Goods->GetActorLocation().ToCompactString());
}

void ULyudQuestDirector::TickPickups(APlayerController* PC, bool bSlow)
{
	ULyudQuests* Quests = ULyudQuests::Get(this);
	ULyudInventory* Inv = ULyudInventory::Get(this);
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	AHeroCharacter* Hero = Cast<AHeroCharacter>(PC->GetPawn());
	if (!Quests || !Inv)
	{
		return;
	}
	// what lies out: looked over once a second
	if (bSlow)
	{
		for (FPickup& P : Pickups)
		{
			if (P.bTaking)
			{
				continue;
			}
			const bool bWanted = Hero && !Hero->IsHidden() && P.Stages.Contains(Quests->StageId(P.Quest)) && !Quests->HasFlag(FName(*(TEXT("pickup.") + P.Id.ToString())))
				&& Inv->Count(P.Item) == 0 && FVector::Dist(Hero->GetActorLocation(), P.At) < QuestDirectorLocal::PickupSpawnCm;
			if (bWanted && !P.Actor.IsValid())
			{
				DropPickup(P);
			}
			else if (!bWanted && P.Actor.IsValid() && (!Hero || FVector::Dist(Hero->GetActorLocation(), P.At) > QuestDirectorLocal::PickupSpawnCm * 1.2f || !P.Stages.Contains(Quests->StageId(P.Quest)) || Inv->Count(P.Item) > 0))
			{
				P.Actor->Destroy();
				P.Actor = nullptr;
			}
		}
	}
	// nothing lies out: nothing to look for
	if (!bPrompt && !Pickups.ContainsByPredicate([](const FPickup& P) { return P.Actor.IsValid(); }))
	{
		return;
	}
	if (bSlow && !TalkDirector.IsValid())
	{
		TActorIterator<ANpcTalkDirector> It(GetWorld());
		TalkDirector = It ? *It : nullptr;
	}
	const bool bTalk = TalkDirector.IsValid() && TalkDirector->IsEngaged();
	// the one the hero looks at from near by
	FPickup* At = nullptr;
	const ULyudGameMenuSubsystem* Menu = GetWorld()->GetSubsystem<ULyudGameMenuSubsystem>();
	UHeroReachComponent* Reach = Hero ? UHeroReachComponent::Get(Hero) : nullptr;
	if (Hero && UI && Reach && !Hero->IsHidden() && !UI->IsOpen() && !(Menu && Menu->IsOpen()) && !PC->IsMoveInputIgnored() && !Reach->IsBusy() && !bTalk)
	{
		const FVector Eye = Hero->EyeLocation();
		const FVector Look = PC->GetControlRotation().Vector();
		float BestDot = 0.9f;
		for (FPickup& P : Pickups)
		{
			if (!P.Actor.IsValid() || P.bTaking)
			{
				continue;
			}
			const FVector To = P.Actor->GetActorLocation() - Eye;
			const float Dot = FVector::DotProduct(Look, To.GetSafeNormal());
			if (To.Size() < QuestDirectorLocal::PickupReachCm && Dot > BestDot)
			{
				BestDot = Dot;
				At = &P;
			}
		}
	}
	if (!At)
	{
		if (bPrompt && UI)
		{
			UI->SetPrompt(this, FText::GetEmpty());
		}
		bPrompt = false;
		return;
	}
	if (!bPrompt || PromptFor != At->Id)
	{
		UI->SetPrompt(this, LyudText::Get(TEXT("ui.quest.take"), { FStringFormatArg(ULyudInventory::ItemName(At->Item).ToString()) }));
		PromptFor = At->Id;
	}
	bPrompt = true;
	if (PC->WasInputKeyJustPressed(EKeys::E))
	{
		const FName PickupId = At->Id;
		const FName Item = At->Item;
		TWeakObjectPtr<ULyudQuestDirector> Self(this);
		At->bTaking = true;
		const bool bReaching = Reach->Take(At->Actor->MakeReachItem(), [Self, PickupId, Item]()
		{
			ULyudQuestDirector* Me = Self.Get();
			if (!Me)
			{
				return;
			}
			if (ULyudQuests* Q = ULyudQuests::Get(Me))
			{
				Q->SetFlag(FName(*(TEXT("pickup.") + PickupId.ToString())));
			}
			if (ULyudInventory* Pocket = ULyudInventory::Get(Me))
			{
				Pocket->Add(Item, 1);
			}
			for (FPickup& P : Me->Pickups)
			{
				if (P.Id == PickupId)
				{
					P.bTaking = false;
					P.Actor = nullptr;
				}
			}
		});
		if (!bReaching)
		{
			At->bTaking = false;
		}
		UI->SetPrompt(this, FText::GetEmpty());
		bPrompt = false;
	}
}

// ------------------------------------------------------------------ the feeders' visitors

void ULyudQuestDirector::TickFeeders(const APawn* Pawn)
{
	UBirdFeederSubsystem* Feeders = UBirdFeederSubsystem::Get(this);
	const UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this);
	if (!Feeders || !Life || !Pawn || Feeders->Num() == 0)
	{
		OnTrays.Reset();
		return;
	}
	// does a quest wait for a visitor now
	bool bQuestWaits = false;
	if (const ULyudQuests* Quests = ULyudQuests::Get(this))
	{
		for (const FLyudQuestProgress& QP : Quests->Progress)
		{
			for (const TPair<const FLyudQuestObjective*, int32>& Obj : Quests->CurrentObjectives(QP.Quest))
			{
				bQuestWaits |= Obj.Key->Event == TEXT("feeder_visit") && Obj.Value < Obj.Key->Need;
			}
		}
	}
	TSet<int32> Now;
	for (int32 I = 0; I < Feeders->Num(); ++I)
	{
		if (FVector::Dist(Pawn->GetActorLocation(), Feeders->State(I).Location) > QuestDirectorLocal::FeederWatchCm)
		{
			continue;
		}
		// the calendar runs at real time: while a quest waits for a visitor and the hero keeps watch, a minute by the feeder
		// counts as an hour (found after about 90 s instead of 90 minutes)
		if (bQuestWaits && Feeders->State(I).Handfuls > 0.f)
		{
			Feeders->Hurry(I, 59.f);
		}
		for (const int32 Bird : Feeders->BirdsOn(I))
		{
			Now.Add(Bird);
			FBirdLifeInfo Info;
			if (!OnTrays.Contains(Bird) && Life->GetBird(Bird, Info))
			{
				ULyudQuests::Notify(this, TEXT("feeder_visit"), Info.SpeciesId.ToString());
			}
		}
	}
	OnTrays = MoveTemp(Now);
}

// ------------------------------------------------------------------ a word in passing

void ULyudQuestDirector::TickRemarks(const APawn* Pawn)
{
	const AHeroCharacter* Hero = Cast<AHeroCharacter>(Pawn);
	ULyudQuests* Quests = ULyudQuests::Get(this);
	const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	if (Remarks.Num() == 0 || !Hero || Hero->IsHidden() || !Quests || !People || !People->IsRunning())
	{
		return;
	}
	if (!TalkDirector.IsValid())
	{
		TActorIterator<ANpcTalkDirector> It(GetWorld());
		TalkDirector = It ? *It : nullptr;
	}
	// never during a talk, and one voice at a time
	if ((TalkDirector.IsValid() && TalkDirector->IsEngaged()) || Speeches.Num() > 0)
	{
		return;
	}
	const FVector HeroAt = Hero->GetActorLocation();
	for (const FRemark& R : Remarks)
	{
		const FName Flag(*(TEXT("remark.") + R.Id.ToString()));
		if (Quests->HasFlag(Flag))
		{
			continue;
		}
		if (!R.Stages.Contains(Quests->StageId(R.Quest)))
		{
			ArmedRemarks.Remove(R.Id);
			continue;
		}
		const int32 Id = People->FindPerson(R.Person);
		FPersonInfo Info;
		if (Id < 0 || !People->GetPerson(Id, Info) || !IsValid(Info.Actor) || Info.Actor->IsHidden() || Info.Pose == EPersonPose::Sleep || Info.Pose == EPersonPose::Hidden)
		{
			continue;
		}
		const float D = FVector::Dist(HeroAt, Info.Actor->GetActorLocation());
		if (D < R.WithinCm * 3.f)
		{
			// coming up to them: the voice is ready by the time they speak
			for (const FString& K : R.Lines)
			{
				LyudText::WarmVoice(this, QuestDirectorLocal::FullKey(K));
			}
		}
		// news takes a moment to go round: a remark is ready once the hero has been out of its reach while its stage holds
		// ("at_once": he begins the stage beside them and they speak then)
		if (D > R.WithinCm * 1.5f)
		{
			ArmedRemarks.Add(R.Id);
		}
		if (D >= R.WithinCm || !(R.bAtOnce || ArmedRemarks.Contains(R.Id)))
		{
			continue;
		}
		// not through a wall
		// (people at home are drawn from 40 m, walls or not; their head, standing or seated, over a counter or a screen)
		const FVector Eyes = Hero->GetPawnViewLocation();
		// (the body itself: the owner's actor is his call zone, his body sits behind the window)
		const USkeletalMeshComponent* Shown = Info.Actor->FindComponentByClass<USkeletalMeshComponent>();
		const FVector Feet = Shown ? Shown->GetComponentLocation() : Info.Actor->GetActorLocation();
		if (TalkDirector.IsValid() && !TalkDirector->InSight(Eyes, Feet + FVector(0.0, 0.0, 150.0), Info.Actor) && !TalkDirector->InSight(Eyes, Feet + FVector(0.0, 0.0, 110.0), Info.Actor))
		{
			continue;
		}
		// one at a time; said once, kept with the save
		if (Say(R.Person, R.Lines, &HeroAt))
		{
			Quests->SetFlag(Flag);
			if (!R.Note.IsEmpty())
			{
				Quests->AddNote(R.Note);
			}
			UE_LOG(LogTemp, Log, TEXT("QuestDirector: remark %s by %s (%.1f m)"), *R.Id.ToString(), *R.Person, D * 0.01f);
			return;
		}
	}
}

// ------------------------------------------------------------------ a bird calls beside somebody

void ULyudQuestDirector::HandleBirdCall(FName Species, const FVector& At, bool bInAir)
{
	const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	const ULyudQuests* Quests = ULyudQuests::Get(this);
	if (!People || !People->IsRunning() || !Quests)
	{
		return;
	}
	const FListener* Best = nullptr;
	const FReaction* BestPick = nullptr;
	int32 BestId = -1;
	float BestD = 0.f;
	int32 BestRank = -1;
	TArray<FString> Calls;
	for (const FListener& L : Listeners)
	{
		const int32 Id = People->FindPerson(L.Person);
		FPersonInfo Info;
		if (Id < 0 || !People->GetPerson(Id, Info) || !IsValid(Info.Actor) || Info.Actor->IsHidden() || Info.Pose == EPersonPose::Sleep || Info.Pose == EPersonPose::Hidden)
		{
			continue;
		}
		const float D = FVector::Dist(At, Info.Actor->GetActorLocation());
		if (D > (bInAir ? L.AirWithinCm : L.WithinCm))
		{
			continue;
		}
		// which reaction: read before the quests hear of the call (the call may finish the stage a reaction is for)
		const FReaction* Pick = nullptr;
		for (const FReaction& R : L.Reactions)
		{
			if (R.bAir == bInAir && (R.Species.IsNone() || R.Species == Species) && (R.Quest.IsNone() || Quests->StageId(R.Quest) == R.Stage) && R.Lines.Num() > 0)
			{
				Pick = &R;
				break;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("QuestDirector: a %s calls %.1f m from %s (%s)"), *Species.ToString(), D * 0.01f, *L.Person, bInAir ? TEXT("on the wing") : TEXT("sitting"));
		const FString Call = Species.ToString() + TEXT(".") + Info.Body.ToString();
		if (!bInAir)
		{
			Calls.Add(Call);
		}
		// one person answers a call: the one a quest waits to hear this very call beside (3), else a reaction for a quest's
		// stage (2), else one for the species (1), else anybody's (0); among equals the nearest
		int32 Rank = -1;
		if (Pick)
		{
			Rank = !Pick->Quest.IsNone() ? 2 : !Pick->Species.IsNone() ? 1 : 0;
			if (!bInAir && !Pick->Quest.IsNone())
			{
				for (const TPair<const FLyudQuestObjective*, int32>& Obj : Quests->CurrentObjectives(Pick->Quest))
				{
					if (Obj.Key->Event == TEXT("bird_call") && Obj.Value < Obj.Key->Need && Obj.Key->Target == Call)
					{
						Rank = 3;
					}
				}
			}
		}
		if (Pick && (Rank > BestRank || (Rank == BestRank && D < BestD)))
		{
			Best = &L;
			BestPick = Pick;
			BestId = Id;
			BestD = D;
			BestRank = Rank;
		}
	}
	// the quests hear of the call after every listener's reaction is chosen (a call may finish the stage a reaction is for)
	for (const FString& Call : Calls)
	{
		ULyudQuests::Notify(this, TEXT("bird_call"), Call);
	}
	if (Best)
	{
		// a quest's reaction is never lost to a passing remark still on their lips
		if (!BestPick->Quest.IsNone())
		{
			for (int32 SI = Speeches.Num() - 1; SI >= 0; --SI)
			{
				if (Speeches[SI].Person == BestId)
				{
					if (Speeches[SI].Voice.IsValid())
					{
						Speeches[SI].Voice->Stop();
					}
					Speeches.RemoveAt(SI);
				}
			}
		}
		// the hero's diary keeps what was said to the bird, the first time
		if (Say(Best->Person, BestPick->Lines, &At) && !BestPick->Note.IsEmpty())
		{
			const FName Heard(*(TEXT("diary.") + BestPick->Note));
			if (ULyudQuests* Diary = ULyudQuests::Get(this); Diary && !Diary->HasFlag(Heard))
			{
				Diary->SetFlag(Heard);
				Diary->AddNote(BestPick->Note);
			}
		}
	}
}

// ------------------------------------------------------------------ lines said aloud

bool ULyudQuestDirector::Say(const FString& PersonKey, const TArray<FString>& Keys, const FVector* LookAt)
{
	const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	const int32 Id = People ? People->FindPerson(PersonKey) : -1;
	if (Id < 0 || Keys.Num() == 0 || !PersonActor(Id) || Speeches.ContainsByPredicate([Id](const FSpeech& S) { return S.Person == Id; }))
	{
		return false;
	}
	FSpeech& S = Speeches.AddDefaulted_GetRef();
	S.Person = Id;
	S.Keys = Keys;
	for (const FString& K : Keys)
	{
		LyudText::WarmVoice(this, QuestDirectorLocal::FullKey(K));
	}
	S.NextAt = 0.0;
	S.bLook = LookAt != nullptr;
	S.LookAt = LookAt ? *LookAt : FVector::ZeroVector;
	return true;
}

void ULyudQuestDirector::TickSpeech()
{
	if (Speeches.Num() == 0)
	{
		return;
	}
	// a talk begins: whatever was being said in passing stops
	if (TalkDirector.IsValid() && TalkDirector->IsTalking())
	{
		for (FSpeech& Cut : Speeches)
		{
			if (Cut.Voice.IsValid())
			{
				Cut.Voice->Stop();
			}
			// (not the one the talk is with: their lips already move with the talk's first line)
			AActor* Speaker = PersonActor(Cut.Person);
			if (Speaker && Speaker != TalkDirector->TalkingTo())
			{
				ULyudBodyAnimInstance::StopLine(Speaker->FindComponentByClass<USkeletalMeshComponent>());
			}
		}
		Speeches.Reset();
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 I = Speeches.Num() - 1; I >= 0; --I)
	{
		FSpeech& S = Speeches[I];
		AActor* Actor = PersonActor(S.Person);
		USkeletalMeshComponent* Mesh = Actor ? Actor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
		if (!Actor)
		{
			if (S.Voice.IsValid())
			{
				S.Voice->Stop();
			}
			Speeches.RemoveAt(I);
			continue;
		}
		// the eyes on what they speak to (the bird that called)
		if (S.bLook && Mesh)
		{
			if (ULyudBodyAnimInstance* Anim = Cast<ULyudBodyAnimInstance>(Mesh->GetAnimInstance()))
			{
				const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
				Anim->SetLookCandidate(Pawn ? Pawn->GetActorLocation() : S.LookAt, true);
			}
		}
		if (Now < S.NextAt)
		{
			continue;
		}
		if (S.Next >= S.Keys.Num())
		{
			ULyudBodyAnimInstance::StopLine(Mesh);
			Speeches.RemoveAt(I);
			continue;
		}
		const FString Key = QuestDirectorLocal::FullKey(S.Keys[S.Next++]);
		const FString Words = LyudText::Get(Key).ToString();
		// up as long as the voice takes, else as long as it takes to read
		float Seconds = FMath::Clamp(2.2f + 0.06f * (float)Words.Len(), 2.5f, 7.f);
		if (ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this))
		{
			Talk->SayLine(Actor, Key, S.Person);
		}
		if (USoundBase* Sound = LyudText::LoadVoice(Key))
		{
			S.Voice = UGameplayStatics::SpawnSound2D(this, Sound);
			ULyudBodyAnimInstance::SpeakLine(Mesh, Key);
			Seconds = FMath::Max(Sound->GetDuration(), 1.f) + 0.45f;
		}
		S.NextAt = Now + Seconds;
		UE_LOG(LogTemp, Log, TEXT("QuestDirector: person %d says %s"), S.Person, *Key);
	}
}

FString ULyudQuestDirector::Report() const
{
	FString Out;
	const ULyudQuests* Quests = ULyudQuests::Get(this);
	for (const FPickup& P : Pickups)
	{
		Out += FString::Printf(TEXT("pickup %s item %s quest %s (%s) %s%s\n"), *P.Id.ToString(), *P.Item.ToString(), *P.Quest.ToString(),
			Quests ? *Quests->StageId(P.Quest).ToString() : TEXT("?"), P.Actor.IsValid() ? *P.Actor->GetActorLocation().ToCompactString() : TEXT("not out"),
			Quests && Quests->HasFlag(FName(*(TEXT("pickup.") + P.Id.ToString()))) ? TEXT(" taken") : TEXT(""));
	}
	const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	for (const FListener& L : Listeners)
	{
		const int32 Id = People ? People->FindPerson(L.Person) : -1;
		FPersonInfo Info;
		const bool bThere = Id >= 0 && People->GetPerson(Id, Info);
		Out += FString::Printf(TEXT("listener %s: %s, %s at %s, %d reactions\n"), *L.Person, bThere ? *Info.Activity : TEXT("nobody of that key"),
			bThere && IsValid(Info.Actor) && !Info.Actor->IsHidden() ? TEXT("seen") : TEXT("unseen"), bThere ? *Info.Location.ToCompactString() : TEXT("-"), L.Reactions.Num());
	}
	if (Quests)
	{
		for (const FRemark& R : Remarks)
		{
			Out += FString::Printf(TEXT("remark %s: %s within %.0f m, %s\n"), *R.Id.ToString(), *R.Person, R.WithinCm * 0.01f,
				Quests->HasFlag(FName(*(TEXT("remark.") + R.Id.ToString()))) ? TEXT("said") : R.Stages.Contains(Quests->StageId(R.Quest)) ? TEXT("waiting for the hero") : TEXT("not its stage"));
		}
	}
	Out += FString::Printf(TEXT("birds on the watched feeders %d, speaking %d\n"), OnTrays.Num(), Speeches.Num());
	return Out;
}

static FAutoConsoleCommandWithWorldAndArgs GQuestScenes(TEXT("Quest.Scenes"), TEXT("Quest.Scenes: logs what the quest director stages now (QUESTTEST lines): pickups, listeners"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (const ULyudQuestDirector* D = ULyudQuestDirector::Get(World))
		{
			TArray<FString> Rows;
			D->Report().ParseIntoArrayLines(Rows);
			for (const FString& Row : Rows)
			{
				UE_LOG(LogTemp, Warning, TEXT("QUESTTEST %s"), *Row);
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GQuestSay(TEXT("Quest.Say"), TEXT("Quest.Say <person key> <line key> [line key...]: somebody says lines over their head, voiced"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudQuestDirector* D = ULyudQuestDirector::Get(World); D && Args.Num() > 1)
		{
			TArray<FString> Keys(Args);
			Keys.RemoveAt(0);
			const bool bOk = D->Say(Args[0], Keys);
			UE_LOG(LogTemp, Warning, TEXT("QUESTTEST say %s: %s"), *Args[0], bOk ? TEXT("ok") : TEXT("not to be seen now, or speaking"));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GQuestBirdCall(TEXT("Quest.BirdCall"), TEXT("Quest.BirdCall <Species> <person key> [air]: as if the hero, a bird of that species, called 3 m from that person (tests)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(World);
		UBirdFlightSubsystem* Flight = World ? World->GetSubsystem<UBirdFlightSubsystem>() : nullptr;
		FPersonInfo Info;
		if (People && Flight && Args.Num() > 1 && People->GetPerson(People->FindPerson(Args[1]), Info))
		{
			Flight->OnPlayerCall.Broadcast(FName(*Args[0]), Info.Location + FVector(300.0, 0.0, 0.0), Args.Num() > 2);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GQuestGoToPerson(TEXT("Quest.GoToPerson"), TEXT("Quest.GoToPerson <person key> [metres away, default 2]: the hero to somebody, wherever they are now (developer mode, tests)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(World);
		APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
		FPersonInfo Info;
		if (!People || !Pawn || Args.Num() < 1 || !People->GetPerson(People->FindPerson(Args[0]), Info))
		{
			UE_LOG(LogTemp, Warning, TEXT("QUESTTEST goto: nobody of that key (or the people's life is not running)"));
			return;
		}
		const float Away = (Args.Num() > 1 ? FMath::Max(1.f, FCString::Atof(*Args[1])) : 2.f) * 100.f;
		// in front of them, on whatever is under that spot (their own floor if nothing is found)
		const FVector Front = FRotator(0.f, Info.Yaw, 0.f).Vector();
		FVector To = Info.Location + Front * Away;
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(QuestGoTo), false);
		Q.AddIgnoredActor(Pawn);
		// from 1.5 m over their floor: under a stall's awning, never onto it
		if (World->LineTraceSingleByChannel(Hit, To + FVector(0.0, 0.0, 150.0), To - FVector(0.0, 0.0, 600.0), ECC_Visibility, Q))
		{
			To = Hit.ImpactPoint;
		}
		Pawn->TeleportTo(To + FVector(0.0, 0.0, 95.0), FRotator(0.f, Info.Yaw + 180.f, 0.f), false, true);
		if (AController* C = Pawn->GetController())
		{
			// eyes on their face (about 1.55 m over their feet), so the talk (E, Npc.Talk) finds them close by too
			const FVector Eyes = Pawn->GetActorLocation() + FVector(0.0, 0.0, Pawn->BaseEyeHeight);
			const FVector Face = Info.Location + FVector(0.0, 0.0, 155.0);
			C->SetControlRotation(FRotator((float)(Face - Eyes).Rotation().Pitch, Info.Yaw + 180.f, 0.f));
		}
		UE_LOG(LogTemp, Warning, TEXT("QUESTTEST goto %s: %s (%s, %s) at %s"), *Args[0], *Info.Activity, *Info.State, IsValid(Info.Actor) && !Info.Actor->IsHidden() ? TEXT("seen") : TEXT("unseen"), *Info.Location.ToCompactString());
	}));
