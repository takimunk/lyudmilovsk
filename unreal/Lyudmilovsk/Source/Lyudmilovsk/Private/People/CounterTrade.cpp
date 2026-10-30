#include "People/CounterTrade.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
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
	FPersonInfo P;
	if (!People->GetPerson(People->FindPerson(Counters[Counter].Person), P))
	{
		return false;
	}
	if (Where)
	{
		*Where = P.Location;
	}
	return P.Activity == Counters[Counter].Activity && P.State == TEXT("doing") && P.Pose != EPersonPose::Hidden;
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
	if (Counter == Prompted)
	{
		return;
	}
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, Counter >= 0 ? LyudText::Get(TEXT("ui.shop.prompt")) : FText::GetEmpty());
	}
	Prompted = Counter;
}

void ULyudCounterTradeSubsystem::Tick(float)
{
	UWorld* World = GetWorld();
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	if (!PC || !UI || UI->IsOpen() || Pawn->IsHidden())
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
	SetPrompted(Best);
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
