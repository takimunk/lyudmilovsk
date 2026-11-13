#include "UI/LyudQuests.h"

#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Plants/GameCalendar.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// ------------------------------------------------------------------ the data files

namespace
{
	struct FQuestData
	{
		bool bLoaded = false;
		TArray<FLyudQuestDef> Quests;
		TArray<FLyudMapPlace> Places;
		FLyudMapFrame Frame;
	};

	FQuestData& Data()
	{
		static FQuestData D;
		return D;
	}

	TSharedPtr<FJsonObject> ReadJson(const TCHAR* Name)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("UI") / Name)))
		{
			UE_LOG(LogTemp, Warning, TEXT("LyudQuests: no Content/UI/%s"), Name);
			return nullptr;
		}
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
		{
			UE_LOG(LogTemp, Warning, TEXT("LyudQuests: Content/UI/%s does not parse"), Name);
			return nullptr;
		}
		return Root;
	}

	FLyudQuestObjective ParseObjective(const TSharedPtr<FJsonObject>& O)
	{
		FLyudQuestObjective Out;
		Out.Id = *O->GetStringField(TEXT("id"));
		Out.Event = *O->GetStringField(TEXT("event"));
		O->TryGetStringField(TEXT("target"), Out.Target);
		O->TryGetNumberField(TEXT("need"), Out.Need);
		Out.Need = FMath::Max(1, Out.Need);
		FString Place;
		if (O->TryGetStringField(TEXT("place"), Place))
		{
			Out.Place = *Place;
		}
		else if (Out.Event == TEXT("reach"))
		{
			Out.Place = *Out.Target;
		}
		return Out;
	}

	void Load()
	{
		FQuestData& D = Data();
		if (D.bLoaded)
		{
			return;
		}
		D.bLoaded = true;
		if (TSharedPtr<FJsonObject> Root = ReadJson(TEXT("quests.json")))
		{
			const TArray<TSharedPtr<FJsonValue>>* Quests = nullptr;
			Root->TryGetArrayField(TEXT("quests"), Quests);
			for (const TSharedPtr<FJsonValue>& QV : Quests ? *Quests : TArray<TSharedPtr<FJsonValue>>())
			{
				const TSharedPtr<FJsonObject> Q = QV->AsObject();
				FLyudQuestDef Def;
				Def.Id = *Q->GetStringField(TEXT("id"));
				Def.bMain = Q->GetStringField(TEXT("kind")) == TEXT("main");
				Q->TryGetBoolField(TEXT("auto_start"), Def.bAutoStart);
				const TSharedPtr<FJsonObject>* StartOn = nullptr;
				if (Q->TryGetObjectField(TEXT("start_on"), StartOn))
				{
					Def.StartEvent = *(*StartOn)->GetStringField(TEXT("event"));
					(*StartOn)->TryGetStringField(TEXT("target"), Def.StartTarget);
				}
				for (const TSharedPtr<FJsonValue>& SV : Q->GetArrayField(TEXT("stages")))
				{
					const TSharedPtr<FJsonObject> S = SV->AsObject();
					FLyudQuestStage Stage;
					Stage.Id = *S->GetStringField(TEXT("id"));
					for (const TSharedPtr<FJsonValue>& OV : S->GetArrayField(TEXT("objectives")))
					{
						Stage.Objectives.Add(ParseObjective(OV->AsObject()));
					}
					const TSharedPtr<FJsonObject>* Reward = nullptr;
					if (S->TryGetObjectField(TEXT("reward"), Reward))
					{
						(*Reward)->TryGetNumberField(TEXT("rubles"), Stage.GiveRubles);
						const TSharedPtr<FJsonObject>* Items = nullptr;
						if ((*Reward)->TryGetObjectField(TEXT("items"), Items))
						{
							for (const TPair<FString, TSharedPtr<FJsonValue>>& It : (*Items)->Values)
							{
								Stage.GiveItems.Add({ FName(*It.Key), static_cast<int32>(It.Value->AsNumber()) });
							}
						}
						const TArray<TSharedPtr<FJsonValue>>* Starts = nullptr;
						if ((*Reward)->TryGetArrayField(TEXT("start"), Starts))
						{
							for (const TSharedPtr<FJsonValue>& V : *Starts)
							{
								Stage.StartQuests.Add(*V->AsString());
							}
						}
					}
					Def.Stages.Add(MoveTemp(Stage));
				}
				if (Def.Stages.Num() > 0)
				{
					D.Quests.Add(MoveTemp(Def));
				}
			}
		}
		if (TSharedPtr<FJsonObject> Root = ReadJson(TEXT("map.json")))
		{
			if (const TSharedPtr<FJsonObject>* F = nullptr; Root->TryGetObjectField(TEXT("frame"), F))
			{
				(*F)->TryGetNumberField(TEXT("x_min"), D.Frame.XMin);
				(*F)->TryGetNumberField(TEXT("x_max"), D.Frame.XMax);
				(*F)->TryGetNumberField(TEXT("y_min"), D.Frame.YMin);
				(*F)->TryGetNumberField(TEXT("y_max"), D.Frame.YMax);
				(*F)->TryGetNumberField(TEXT("portal_face_y"), D.Frame.PortalFaceY);
			}
			const TArray<TSharedPtr<FJsonValue>>* Places = nullptr;
			Root->TryGetArrayField(TEXT("places"), Places);
			for (const TSharedPtr<FJsonValue>& PV : Places ? *Places : TArray<TSharedPtr<FJsonValue>>())
			{
				const TSharedPtr<FJsonObject> P = PV->AsObject();
				FLyudMapPlace Place;
				Place.Id = *P->GetStringField(TEXT("id"));
				Place.At = FVector2D(P->GetNumberField(TEXT("x")), P->GetNumberField(TEXT("y")));
				double R = 0.0;
				P->TryGetNumberField(TEXT("r"), R);
				Place.Radius = R;
				Place.Kind = *P->GetStringField(TEXT("kind"));
				P->TryGetBoolField(TEXT("known"), Place.bKnown);
				D.Places.Add(Place);
			}
		}
		UE_LOG(LogTemp, Log, TEXT("LyudQuests: %d quests, %d places"), D.Quests.Num(), D.Places.Num());
	}

	/** An objective's event may list alternatives: "leave_computer|walk". */
	bool EventMatches(FName Want, FName Got)
	{
		if (Want == Got)
		{
			return true;
		}
		TArray<FString> Alts;
		Want.ToString().ParseIntoArray(Alts, TEXT("|"));
		return Alts.Num() > 1 && Alts.Contains(Got.ToString());
	}

	bool TargetMatches(const FString& Want, const FString& Got)
	{
		if (Want.IsEmpty())
		{
			return true;
		}
		if (Want.EndsWith(TEXT("*")))
		{
			return Got.StartsWith(Want.LeftChop(1));
		}
		return Want == Got;
	}
}

const TArray<FLyudQuestDef>& ULyudQuests::Defs()
{
	Load();
	return Data().Quests;
}

const FLyudQuestDef* ULyudQuests::FindDef(FName Quest)
{
	return Defs().FindByPredicate([Quest](const FLyudQuestDef& D) { return D.Id == Quest; });
}

const TArray<FLyudMapPlace>& ULyudQuests::Places()
{
	Load();
	return Data().Places;
}

const FLyudMapPlace* ULyudQuests::FindPlace(FName Place)
{
	return Places().FindByPredicate([Place](const FLyudMapPlace& P) { return P.Id == Place; });
}

const FLyudMapFrame& ULyudQuests::Frame()
{
	Load();
	return Data().Frame;
}

void ULyudQuests::ReloadData()
{
	Data() = FQuestData();
}

FText ULyudQuests::QuestTitle(FName Quest)
{
	return LyudText::Get(FString::Printf(TEXT("quest.%s.title"), *Quest.ToString()));
}

FText ULyudQuests::ObjectiveText(FName Quest, const FLyudQuestObjective& O, int32 Count)
{
	const FText Line = LyudText::Get(FString::Printf(TEXT("quest.%s.%s.obj"), *Quest.ToString(), *O.Id.ToString()));
	if (O.Need <= 1)
	{
		return Line;
	}
	return LyudText::Get(TEXT("ui.journal.progress"), { FStringFormatArg(Line.ToString()), FStringFormatArg(FMath::Min(Count, O.Need)), FStringFormatArg(O.Need) });
}

FText ULyudQuests::PlaceName(FName Place)
{
	return LyudText::Get(FString::Printf(TEXT("place.%s.name"), *Place.ToString()));
}

FText FLyudJournalEntry::Text() const
{
	TArray<FStringFormatArg> A;
	for (const FString& S : Args)
	{
		A.Add(S.StartsWith(TEXT("@")) ? LyudText::Get(S.Mid(1)).ToString() : S);
	}
	return LyudText::Get(Key, A);
}

// ------------------------------------------------------------------ progress

ULyudQuests* ULyudQuests::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<ULyudQuests>() : nullptr;
}

void ULyudQuests::Notify(const UObject* WorldContext, FName Event, const FString& Target, int32 N)
{
	if (ULyudQuests* Q = Get(WorldContext))
	{
		Q->HandleEvent(Event, Target, N);
	}
}

void ULyudQuests::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (ULyudSaveSubsystem* Saves = Collection.InitializeDependency<ULyudSaveSubsystem>())
	{
		Saves->RegisterSaveObject(TEXT("Quests"), this);
		Saves->OnAfterLoad.AddWeakLambda(this, [this](ULyudSaveGame*)
		{
			// the save may be older than quests.json: keep each active quest on a real stage with one count per objective
			for (FLyudQuestProgress& P : Progress)
			{
				const FLyudQuestDef* Def = FindDef(P.Quest);
				if (!Def || P.State != ELyudQuestState::Active || Def->Stages.IsEmpty())
				{
					continue;
				}
				P.Stage = FMath::Clamp(P.Stage, 0, Def->Stages.Num() - 1);
				P.Counts.SetNumZeroed(Def->Stages[P.Stage].Objectives.Num());
			}
			OnChanged.Broadcast();
		});
	}
}

FDateTime ULyudQuests::Today() const
{
	const UGameInstance* GI = GetGameInstance();
	return UGameCalendar::NowIn(GI ? GI->GetWorld() : nullptr);
}

void ULyudQuests::Reset()
{
	Progress.Reset();
	Entries.Reset();
	Found.Reset();
	Flags.Reset();
	Tracked = NAME_None;
	Queue.Reset();
	Inside.Reset();
	for (const FLyudMapPlace& P : Places())
	{
		if (P.bKnown)
		{
			Found.Add(P.Id);
		}
	}
	for (const FLyudQuestDef& D : Defs())
	{
		if (D.bAutoStart)
		{
			Start(D.Id);
		}
	}
	OnChanged.Broadcast();
}

FLyudQuestProgress& ULyudQuests::ProgressOf(FName Quest)
{
	if (FLyudQuestProgress* P = Progress.FindByPredicate([Quest](const FLyudQuestProgress& X) { return X.Quest == Quest; }))
	{
		return *P;
	}
	FLyudQuestProgress& P = Progress.AddDefaulted_GetRef();
	P.Quest = Quest;
	return P;
}

const FLyudQuestProgress* ULyudQuests::FindProgress(FName Quest) const
{
	return Progress.FindByPredicate([Quest](const FLyudQuestProgress& X) { return X.Quest == Quest; });
}

ELyudQuestState ULyudQuests::State(FName Quest) const
{
	const FLyudQuestProgress* P = FindProgress(Quest);
	return P ? P->State : ELyudQuestState::Inactive;
}

void ULyudQuests::AddEntry(const FString& Key, FName Quest, const TArray<FString>& Args)
{
	FLyudJournalEntry& E = Entries.AddDefaulted_GetRef();
	E.Key = Key;
	E.Args = Args;
	E.When = Today();
	E.Quest = Quest;
}

bool ULyudQuests::Start(FName Quest)
{
	const FLyudQuestDef* Def = FindDef(Quest);
	if (!Def || State(Quest) != ELyudQuestState::Inactive)
	{
		return false;
	}
	FLyudQuestProgress& P = ProgressOf(Quest);
	P.State = ELyudQuestState::Active;
	P.Stage = 0;
	P.Started = Today();
	if (Tracked.IsNone() || (Def->bMain && State(Tracked) != ELyudQuestState::Active))
	{
		Tracked = Quest;
	}
	OnNews.Broadcast(ELyudQuestNews::Started, Quest);
	BeginStage(P, *Def);
	OnChanged.Broadcast();
	return true;
}

void ULyudQuests::BeginStage(FLyudQuestProgress& P, const FLyudQuestDef& Def)
{
	if (!Def.Stages.IsValidIndex(P.Stage))
	{
		P.Counts.Reset();
		return;
	}
	const FLyudQuestStage& S = Def.Stages[P.Stage];
	P.Counts.Init(0, S.Objectives.Num());
	const FString Log = FString::Printf(TEXT("quest.%s.%s.log"), *Def.Id.ToString(), *S.Id.ToString());
	AddEntry(Log, Def.Id);
	// an objective already met by what the hero has (an item in the pocket, a place already found)
	for (int32 I = 0; I < S.Objectives.Num(); ++I)
	{
		const FLyudQuestObjective& O = S.Objectives[I];
		if (O.Event == TEXT("have"))
		{
			if (const ULyudInventory* Inv = ULyudInventory::Get(GetGameInstance()->GetWorld()))
			{
				P.Counts[I] = Inv->Count(*O.Target);
			}
		}
		// already standing there
		else if (O.Event == TEXT("reach") && Inside.Contains(*O.Target))
		{
			P.Counts[I] = O.Need;
		}
	}
}

void ULyudQuests::FinishStage(FLyudQuestProgress& P, const FLyudQuestDef& Def)
{
	if (Def.Stages.IsEmpty())
	{
		return;
	}
	// a stage out of range (an old save, quests.json edited) finishes the nearest real one, so callers still move on
	P.Stage = FMath::Clamp(P.Stage, 0, Def.Stages.Num() - 1);
	const FLyudQuestStage& S = Def.Stages[P.Stage];
	if (ULyudInventory* Inv = ULyudInventory::Get(GetGameInstance()->GetWorld()))
	{
		for (const TPair<FName, int32>& G : S.GiveItems)
		{
			Inv->Add(G.Key, G.Value);
		}
		if (S.GiveRubles > 0)
		{
			Inv->Earn(S.GiveRubles);
		}
	}
	const FName Quest = Def.Id;
	if (P.Stage + 1 < Def.Stages.Num())
	{
		++P.Stage;
		OnNews.Broadcast(ELyudQuestNews::Stage, Quest);
		BeginStage(P, Def);
	}
	else
	{
		P.State = ELyudQuestState::Done;
		P.Finished = Today();
		P.Counts.Reset();
		AddEntry(FString::Printf(TEXT("quest.%s.done"), *Quest.ToString()), Quest);
		OnNews.Broadcast(ELyudQuestNews::Done, Quest);
		if (Tracked == Quest)
		{
			Tracked = NAME_None;
			Tracked = ShownQuest();
		}
	}
	// P may move if StartQuests adds progress records: copy the list first
	const TArray<FName> Starts = S.StartQuests;
	for (const FName& Q : Starts)
	{
		Start(Q);
	}
}

void ULyudQuests::HandleEvent(FName Event, const FString& Target, int32 N)
{
	Queue.Add({ Event, Target, N });
	if (bHandling)
	{
		return;
	}
	TGuardValue<bool> Guard(bHandling, true);
	bool bChanged = false;
	while (Queue.Num() > 0)
	{
		const TTuple<FName, FString, int32> E = Queue[0];
		Queue.RemoveAt(0);
		UE_LOG(LogTemp, Log, TEXT("LyudQuests: event %s %s x%d"), *E.Get<0>().ToString(), *E.Get<1>(), E.Get<2>());
		// quests that start on this event
		for (const FLyudQuestDef& D : Defs())
		{
			if (!D.StartEvent.IsNone() && D.StartEvent == E.Get<0>() && TargetMatches(D.StartTarget, E.Get<1>()) && State(D.Id) == ELyudQuestState::Inactive)
			{
				bChanged |= Start(D.Id);
			}
		}
		// the active ones' objectives (by index: a finished stage may add quests to Progress)
		for (int32 QI = 0; QI < Progress.Num(); ++QI)
		{
			if (Progress[QI].State != ELyudQuestState::Active)
			{
				continue;
			}
			const FLyudQuestDef* Def = FindDef(Progress[QI].Quest);
			if (!Def || !Def->Stages.IsValidIndex(Progress[QI].Stage))
			{
				continue;
			}
			// an event a later stage waits for (a teleport, a load, a step done out of order): the stages before it count as done
			// (never a talk: a quest may send the hero back to the one who gave it, and that talk is not a step done early)
			for (int32 Later = Progress[QI].Stage + 1; Later < Def->Stages.Num() && !(E.Get<0>() == TEXT("talk")); ++Later)
			{
				const bool bWanted = Def->Stages[Later].Objectives.ContainsByPredicate([&E](const FLyudQuestObjective& O)
				{
					return O.Event != TEXT("have") && EventMatches(O.Event, E.Get<0>()) && TargetMatches(O.Target, E.Get<1>());
				});
				if (bWanted)
				{
					UE_LOG(LogTemp, Log, TEXT("LyudQuests: %s skips ahead to stage %d on %s %s"), *Def->Id.ToString(), Later, *E.Get<0>().ToString(), *E.Get<1>());
					while (Progress[QI].State == ELyudQuestState::Active && Progress[QI].Stage < Later)
					{
						FinishStage(Progress[QI], *Def);
					}
					bChanged = true;
					break;
				}
			}
			if (Progress[QI].State != ELyudQuestState::Active)
			{
				continue;
			}
			const FLyudQuestStage& S = Def->Stages[Progress[QI].Stage];
			// one count per objective, whatever the save held (keeps the counts there are)
			if (Progress[QI].Counts.Num() != S.Objectives.Num())
			{
				Progress[QI].Counts.SetNumZeroed(S.Objectives.Num());
			}
			bool bAll = true;
			for (int32 I = 0; I < S.Objectives.Num(); ++I)
			{
				const FLyudQuestObjective& O = S.Objectives[I];
				int32& C = Progress[QI].Counts[I];
				const bool bWas = C >= O.Need;
				// "have" counts the pocket: items coming in count, "use" of it takes away
				const bool bHave = O.Event == TEXT("have");
				if ((EventMatches(O.Event, E.Get<0>()) || (bHave && E.Get<0>() == TEXT("get"))) && TargetMatches(O.Target, E.Get<1>()))
				{
					C += E.Get<2>();
					bChanged = true;
				}
				else if (bHave && E.Get<0>() == TEXT("use") && TargetMatches(O.Target, E.Get<1>()))
				{
					C = FMath::Max(0, C - E.Get<2>());
					bChanged = true;
				}
				// "bring <item>@<Npc>": a talk with that person while the pocket holds Need of the item hands them over
				if (O.Event == TEXT("bring") && E.Get<0>() == TEXT("talk") && C < O.Need)
				{
					FString BringItem, BringTo;
					ULyudInventory* Pocket = ULyudInventory::Get(GetGameInstance()->GetWorld());
					if (O.Target.Split(TEXT("@"), &BringItem, &BringTo) && BringTo == E.Get<1>() && Pocket && Pocket->Count(*BringItem) >= O.Need)
					{
						Pocket->Remove(*BringItem, O.Need);      // (its "use" event waits in the queue)
						C = O.Need;
						bChanged = true;
					}
				}
				if (!bWas && C >= O.Need && S.Objectives.Num() > 1)
				{
					OnNews.Broadcast(ELyudQuestNews::Objective, Def->Id);
				}
				bAll &= C >= O.Need;
			}
			if (bAll)
			{
				FinishStage(Progress[QI], *Def);
				bChanged = true;
				// the stage that begins may be met already by what the hero has (BeginStage counted it): finish it now.
				// Progress[QI] is indexed each time: FinishStage may add records to Progress.
				while (Progress[QI].State == ELyudQuestState::Active && Def->Stages.IsValidIndex(Progress[QI].Stage))
				{
					const FLyudQuestStage& Begun = Def->Stages[Progress[QI].Stage];
					bool bMet = Begun.Objectives.Num() > 0 && Progress[QI].Counts.Num() == Begun.Objectives.Num();
					for (int32 NI = 0; bMet && NI < Begun.Objectives.Num(); ++NI)
					{
						bMet = Progress[QI].Counts[NI] >= Begun.Objectives[NI].Need;
					}
					if (!bMet)
					{
						break;
					}
					FinishStage(Progress[QI], *Def);
				}
			}
		}
	}
	if (bChanged)
	{
		OnChanged.Broadcast();
	}
}

void ULyudQuests::Advance(FName Quest)
{
	FLyudQuestProgress* P = Progress.FindByPredicate([Quest](const FLyudQuestProgress& X) { return X.Quest == Quest; });
	const FLyudQuestDef* Def = FindDef(Quest);
	if (P && Def && P->State == ELyudQuestState::Active && !Def->Stages.IsEmpty())
	{
		FinishStage(*P, *Def);
		OnChanged.Broadcast();
	}
}

bool ULyudQuests::SetStage(FName Quest, const FString& Stage)
{
	const FLyudQuestDef* Def = FindDef(Quest);
	if (!Def || Def->Stages.IsEmpty())
	{
		return false;
	}
	const bool bDone = Stage == TEXT("done");
	int32 Index = Def->Stages.Num() - 1;
	if (!bDone)
	{
		Index = Def->Stages.IndexOfByPredicate([&Stage](const FLyudQuestStage& S) { return S.Id.ToString() == Stage; });
		if (Index == INDEX_NONE && Stage.IsNumeric())
		{
			Index = FCString::Atoi(*Stage);
		}
		if (!Def->Stages.IsValidIndex(Index))
		{
			return false;
		}
	}
	{
		FLyudQuestProgress& P = ProgressOf(Quest);
		if (P.State == ELyudQuestState::Inactive)
		{
			P.Started = Today();
		}
		P.State = ELyudQuestState::Active;
		P.Stage = Index;
		P.Finished = FDateTime();
		Tracked = Quest;
		if (bDone)
		{
			// may begin other quests (the stage's reward): P is not used after it
			FinishStage(P, *Def);
		}
		else
		{
			OnNews.Broadcast(ELyudQuestNews::Stage, Quest);
			BeginStage(P, *Def);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("LyudQuests: %s set to stage %s"), *Quest.ToString(), *Stage);
	OnChanged.Broadcast();
	return true;
}

FName ULyudQuests::StageId(FName Quest) const
{
	const FLyudQuestProgress* P = FindProgress(Quest);
	const FLyudQuestDef* Def = FindDef(Quest);
	if (!P || !Def || P->State == ELyudQuestState::Inactive)
	{
		return NAME_None;
	}
	if (P->State == ELyudQuestState::Done)
	{
		return TEXT("done");
	}
	return Def->Stages.IsValidIndex(P->Stage) ? Def->Stages[P->Stage].Id : NAME_None;
}

FName ULyudQuests::TalkStage(FName Quest, const FString& Npc) const
{
	const FName Now = StageId(Quest);
	if (!Now.IsNone())
	{
		// a stage that waits for something to be brought to this person, and the pocket hasn't got it: "<stage>_empty"
		for (const TPair<const FLyudQuestObjective*, int32>& O : CurrentObjectives(Quest))
		{
			FString BringItem, BringTo;
			if (O.Key->Event == TEXT("bring") && O.Value < O.Key->Need && O.Key->Target.Split(TEXT("@"), &BringItem, &BringTo) && BringTo == Npc)
			{
				const ULyudInventory* Pocket = ULyudInventory::Get(GetGameInstance()->GetWorld());
				if (!Pocket || Pocket->Count(*BringItem) < O.Key->Need)
				{
					return FName(*(Now.ToString() + TEXT("_empty")));
				}
			}
		}
		return Now;
	}
	const FLyudQuestDef* Def = FindDef(Quest);
	if (Def && Def->Stages.Num() > 0 && Def->StartEvent == TEXT("talk") && TargetMatches(Def->StartTarget, Npc))
	{
		return Def->Stages[0].Id;
	}
	return NAME_None;
}

void ULyudQuests::UpdatePosition(const UObject* WorldContext, const FVector& P)
{
	const FVector2D M = Frame().FromWorld(P);
	for (const FLyudMapPlace& Place : Places())
	{
		if (Place.Radius <= 0.f)
		{
			continue;
		}
		// in by the radius, out again only a little further (no flicker on the edge)
		const double D2 = FVector2D::DistSquared(M, Place.At);
		if (D2 > FMath::Square(Place.Radius * 1.15f))
		{
			Inside.Remove(Place.Id);
			continue;
		}
		if (D2 > FMath::Square(Place.Radius) || Inside.Contains(Place.Id))
		{
			continue;
		}
		Inside.Add(Place.Id);
		if (!Found.Contains(Place.Id))
		{
			Found.Add(Place.Id);
			AddEntry(TEXT("ui.journal.found"), NAME_None, { TEXT("@place.") + Place.Id.ToString() + TEXT(".name") });
			OnNews.Broadcast(ELyudQuestNews::Place, Place.Id);
			OnChanged.Broadcast();
		}
		HandleEvent(TEXT("reach"), Place.Id.ToString(), 1);
	}
}

FName ULyudQuests::ShownQuest() const
{
	if (!Tracked.IsNone() && State(Tracked) == ELyudQuestState::Active)
	{
		return Tracked;
	}
	FName Any;
	for (const FLyudQuestProgress& P : Progress)
	{
		if (P.State == ELyudQuestState::Active)
		{
			const FLyudQuestDef* D = FindDef(P.Quest);
			if (D && D->bMain)
			{
				return P.Quest;
			}
			if (Any.IsNone())
			{
				Any = P.Quest;
			}
		}
	}
	return Any;
}

TArray<TPair<const FLyudQuestObjective*, int32>> ULyudQuests::CurrentObjectives(FName Quest) const
{
	TArray<TPair<const FLyudQuestObjective*, int32>> Out;
	const FLyudQuestProgress* P = FindProgress(Quest);
	const FLyudQuestDef* Def = FindDef(Quest);
	if (!P || !Def || P->State != ELyudQuestState::Active || !Def->Stages.IsValidIndex(P->Stage))
	{
		return Out;
	}
	const FLyudQuestStage& S = Def->Stages[P->Stage];
	for (int32 I = 0; I < S.Objectives.Num(); ++I)
	{
		Out.Add({ &S.Objectives[I], P->Counts.IsValidIndex(I) ? P->Counts[I] : 0 });
	}
	return Out;
}

// --- dev commands ---
static FAutoConsoleCommandWithWorldAndArgs GQuestEvent(TEXT("Quest.Event"), TEXT("Quest.Event <event> [target] [n]: reports a game event to the quests"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() > 0)
		{
			ULyudQuests::Notify(World, *Args[0], Args.Num() > 1 ? Args[1] : FString(), Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 1);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GQuestStart(TEXT("Quest.Start"), TEXT("Quest.Start <quest id>: begins a quest"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudQuests* Q = ULyudQuests::Get(World); Q && Args.Num() > 0)
		{
			Q->Start(*Args[0]);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GQuestAdvance(TEXT("Quest.Advance"), TEXT("Quest.Advance <quest id>: finishes the quest's current stage"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudQuests* Q = ULyudQuests::Get(World); Q && Args.Num() > 0)
		{
			Q->Advance(*Args[0]);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GQuestSetStage(TEXT("Quest.SetStage"), TEXT("Quest.SetStage <quest id> <stage id | index | done>: puts a quest on a stage at once (begins it if it has not begun)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudQuests* Q = ULyudQuests::Get(World); Q && Args.Num() > 1)
		{
			const bool bOk = Q->SetStage(*Args[0], Args[1]);
			UE_LOG(LogTemp, Warning, TEXT("QUESTTEST set %s %s: %s"), *Args[0], *Args[1], bOk ? TEXT("ok") : TEXT("no such quest or stage"));
		}
	}));

static FAutoConsoleCommand GQuestList(TEXT("Quest.List"), TEXT("Quest.List: logs every quest with its stages and what each objective waits for (QUESTTEST lines)"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		for (const FLyudQuestDef& D : ULyudQuests::Defs())
		{
			for (int32 I = 0; I < D.Stages.Num(); ++I)
			{
				FString Objs;
				for (const FLyudQuestObjective& O : D.Stages[I].Objectives)
				{
					Objs += FString::Printf(TEXT(" [%s: %s %s x%d]"), *O.Id.ToString(), *O.Event.ToString(), *O.Target, O.Need);
				}
				UE_LOG(LogTemp, Warning, TEXT("QUESTTEST %s %d %s%s"), *D.Id.ToString(), I, *D.Stages[I].Id.ToString(), *Objs);
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GQuestReset(TEXT("Quest.Reset"), TEXT("Quest.Reset: the quests as a new game starts them"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (ULyudQuests* Q = ULyudQuests::Get(World))
		{
			Q->Reset();
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GQuestDump(TEXT("Quest.Dump"), TEXT("Quest.Dump: logs every quest's state (QUESTTEST lines)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		const ULyudQuests* Q = ULyudQuests::Get(World);
		if (!Q)
		{
			return;
		}
		for (const FLyudQuestDef& D : ULyudQuests::Defs())
		{
			const FLyudQuestProgress* P = Q->FindProgress(D.Id);
			FString Objs;
			for (const TPair<const FLyudQuestObjective*, int32>& O : Q->CurrentObjectives(D.Id))
			{
				Objs += FString::Printf(TEXT(" %s=%d/%d"), *O.Key->Id.ToString(), O.Value, O.Key->Need);
			}
			UE_LOG(LogTemp, Warning, TEXT("QUESTTEST %s state %d stage %d%s"), *D.Id.ToString(), P ? (int32)P->State : 0, P ? P->Stage : -1, *Objs);
		}
		UE_LOG(LogTemp, Warning, TEXT("QUESTTEST entries %d found %d tracked %s"), Q->Entries.Num(), Q->Found.Num(), *Q->Tracked.ToString());
	}));
