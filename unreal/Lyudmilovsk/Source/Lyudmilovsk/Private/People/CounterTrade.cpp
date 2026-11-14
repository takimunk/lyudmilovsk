#include "People/CounterTrade.h"

#include "HeroCharacter.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Fishing/LyudFishing.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "People/PeopleLife.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

ULyudCounterTradeSubsystem* ULyudCounterTradeSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudCounterTradeSubsystem>() : nullptr;
}

void ULyudCounterTradeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Load();
}

void ULyudCounterTradeSubsystem::Deinitialize()
{
	SetPrompted(-1);
	Super::Deinitialize();
}

void ULyudCounterTradeSubsystem::Load()
{
	Counters.Reset();
	CacheRun = nullptr;
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("PeopleData/counters.json"))))
	{
		return;
	}
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid() || !Root->TryGetArrayField(TEXT("counters"), List))
	{
		UE_LOG(LogTemp, Warning, TEXT("CounterTrade: PeopleData/counters.json unreadable"));
		return;
	}
	for (const TSharedPtr<FJsonValue>& V : *List)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		if (!O.IsValid())
		{
			continue;
		}
		FLyudCounter C;
		C.Person = O->GetStringField(TEXT("person"));
		C.Activity = O->GetStringField(TEXT("activity"));
		O->TryGetStringField(TEXT("title"), C.TitleKey);
		O->TryGetBoolField(TEXT("buys_fish"), C.bBuysFish);
		O->TryGetNumberField(TEXT("cooks_fish"), C.CookFee);
		double Reach = 0.0;
		if (O->TryGetNumberField(TEXT("reach_cm"), Reach) && Reach > 0.0)
		{
			C.Reach = (float)Reach;
		}
		const TArray<TSharedPtr<FJsonValue>>* Goods = nullptr;
		if (O->TryGetArrayField(TEXT("goods"), Goods))
		{
			for (const TSharedPtr<FJsonValue>& G : *Goods)
			{
				const TSharedPtr<FJsonObject> GO = G->AsObject();
				FLyudShopGood Good;
				Good.Item = FName(*GO->GetStringField(TEXT("item")));
				Good.Price = GO->HasField(TEXT("price")) ? (int32)GO->GetNumberField(TEXT("price")) : 0;
				Good.Count = GO->HasField(TEXT("count")) ? (int32)GO->GetNumberField(TEXT("count")) : 1;
				if (ULyudInventory::FindDef(Good.Item))
				{
					C.Goods.Add(Good);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("CounterTrade: %s sells an unknown item %s"), *C.Person, *Good.Item.ToString());
				}
			}
		}
		if (!C.Person.IsEmpty() && C.Goods.Num() > 0)
		{
			Counters.Add(MoveTemp(C));
		}
	}
}

bool ULyudCounterTradeSubsystem::IsOpen(int32 Counter, FVector* Where) const
{
	const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	if (!People || !Counters.IsValidIndex(Counter))
	{
		return false;
	}
	// asked every frame: the seller's id and the activity's index once per run of the people's life, then a cheap read
	const void* Run = People->GetSim();
	if (!Run)
	{
		return false;  // the people's life hasn't started (or has stopped): nobody is at a counter
	}
	if (Run != CacheRun || CachePerson.Num() != Counters.Num() || CacheActivity.Num() != Counters.Num())
	{
		CacheRun = Run;
		CachePerson.SetNum(Counters.Num());
		CacheActivity.SetNum(Counters.Num());
		for (int32 I = 0; I < Counters.Num(); ++I)
		{
			CachePerson[I] = People->FindPerson(Counters[I].Person);
			CacheActivity[I] = People->FindActivity(Counters[I].Activity);
		}
	}
	int32 Activity = -1;
	bool bDoing = false;
	EPersonPose Pose = EPersonPose::Hidden;
	FVector At = FVector::ZeroVector;
	if (!People->GetActivityState(CachePerson[Counter], Activity, bDoing, Pose, At))
	{
		return false;
	}
	if (Where)
	{
		*Where = At;
	}
	// (an activity activities.json doesn't have never matches; none at all matches a person doing nothing)
	const bool bKnown = CacheActivity[Counter] >= 0 || Counters[Counter].Activity.IsEmpty();
	return bKnown && Activity == CacheActivity[Counter] && bDoing && Pose != EPersonPose::Hidden;
}

bool ULyudCounterTradeSubsystem::OpenCounter(int32 Counter)
{
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (!UI || !Counters.IsValidIndex(Counter))
	{
		return false;
	}
	const FLyudCounter& C = Counters[Counter];
	const FText Name = LyudText::Get(FString::Printf(TEXT("npc.%s.name"), *C.Person));
	UI->OpenShop(LyudText::Get(C.TitleKey, { FStringFormatArg(Name.ToString()) }), C.Goods);
	return UI->IsOpen();
}

void ULyudCounterTradeSubsystem::SetPrompted(int32 Counter)
{
	// a counter that buys the catch offers it while there are fish in the pocket; the diner's offers to fry the biggest one.
	// What decides the line is gathered first (numbers and names, no strings); the line is built only when it changes.
	FPromptKey Key;
	Key.Counter = Counter;
	if (Counters.IsValidIndex(Counter))
	{
		const FLyudCounter& C = Counters[Counter];
		const ULyudFishingState* State = ULyudFishingState::Get(this);
		if (State && C.bBuysFish)
		{
			Key.Fish = State->FishHeld(this, &Key.Roubles);
		}
		if (State && C.CookFee > 0)
		{
			Key.Biggest = State->BiggestHeld(this);
			Key.Fee = C.CookFee;
		}
		Key.Deal = Key.Fish > 0 && Key.Roubles > 0 ? EFishDeal::Sell : (!Key.Biggest.IsNone() ? EFishDeal::Cook : EFishDeal::None);
	}
	if (Key == PromptedKey)
	{
		return;
	}
	PromptedKey = Key;
	Prompted = Counter;
	Deal = Key.Deal;
	FText Line;
	if (Counter >= 0)
	{
		switch (Key.Deal)
		{
		case EFishDeal::Sell:
			Line = LyudText::Get(TEXT("ui.shop.prompt_sell_fish"), { FStringFormatArg(Key.Fish), FStringFormatArg(Key.Roubles) });
			break;
		case EFishDeal::Cook:
			Line = LyudText::Get(TEXT("ui.shop.prompt_cook_fish"), { FStringFormatArg(ULyudInventory::ItemName(Key.Biggest).ToString()), FStringFormatArg(Key.Fee) });
			break;
		default:
			Line = LyudText::Get(TEXT("ui.shop.prompt"));
			break;
		}
	}
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, Line);
	}
}

void ULyudCounterTradeSubsystem::Tick(float)
{
	UWorld* World = GetWorld();
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	// (the boy himself: a bird the player has become buys and sells nothing)
	if (!PC || !UI || UI->IsOpen() || Pawn->IsHidden() || !Pawn->IsA<AHeroCharacter>())
	{
		SetPrompted(-1);
		return;
	}
	// the nearest open counter whose seller the hero stands by and looks at
	const FVector Eye = Pawn->GetPawnViewLocation();
	const FVector Look = PC->GetControlRotation().Vector();
	int32 Best = -1;
	float BestDist = TNumericLimits<float>::Max();
	for (int32 I = 0; I < Counters.Num(); ++I)
	{
		FVector At;
		if (!IsOpen(I, &At))
		{
			continue;
		}
		const float Dist = FVector::Dist2D(Pawn->GetActorLocation(), At);
		// a seated seller's chest is lower than a standing one's; either way the hero looks roughly at them
		const FVector ToSeller = (At + FVector(0.f, 0.f, 110.f) - Eye).GetSafeNormal();
		if (Dist < Counters[I].Reach && Dist < BestDist && FVector::DotProduct(Look, ToSeller) > 0.55f)
		{
			Best = I;
			BestDist = Dist;
		}
	}
	if (World->GetTimeSeconds() < SoldUntil)
	{
		return;
	}
	SetPrompted(Best);
	if (Best >= 0 && Deal != EFishDeal::None && PC->WasInputKeyJustPressed(EKeys::Y))
	{
		ULyudFishingState* State = ULyudFishingState::Get(this);
		FText Said;
		if (State && Deal == EFishDeal::Sell)
		{
			if (const int32 Got = State->SellAll(this); Got > 0)
			{
				Said = LyudText::Get(TEXT("ui.shop.sold_fish"), { FStringFormatArg(Got) });
			}
		}
		else if (State && Deal == EFishDeal::Cook)
		{
			const FName Fish = State->BiggestHeld(this);
			Said = State->Cook(this, Fish, Counters[Best].CookFee)
				? LyudText::Get(TEXT("ui.shop.cooking_fish"), { FStringFormatArg(ULyudInventory::ItemName(Fish).ToString()) })
				: LyudText::Get(TEXT("ui.shop.nomoney"));
		}
		if (!Said.IsEmpty())
		{
			Prompted = -1;
			PromptedKey.Counter = -2;
			SoldUntil = World->GetTimeSeconds() + 2.5;
			UI->SetPrompt(this, Said);
		}
	}
	if (Best >= 0 && PC->WasInputKeyJustPressed(EKeys::T))
	{
		SetPrompted(-1);
		OpenCounter(Best);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GTradeOpen(TEXT("lyud.Trade.Open"),
	TEXT("lyud.Trade.Open <person>: opens that seller's counter (PeopleData/counters.json) wherever the hero is (checks)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudCounterTradeSubsystem* T = ULyudCounterTradeSubsystem::Get(World);
		if (!T)
		{
			return;
		}
		for (int32 I = 0; I < T->GetCounters().Num(); ++I)
		{
			if (Args.Num() == 0 || T->GetCounters()[I].Person == Args[0])
			{
				UE_LOG(LogTemp, Display, TEXT("Trade: %s %s"), *T->GetCounters()[I].Person, T->IsOpen(I) ? TEXT("at work") : TEXT("away"));
				T->OpenCounter(I);
				return;
			}
		}
	}));
