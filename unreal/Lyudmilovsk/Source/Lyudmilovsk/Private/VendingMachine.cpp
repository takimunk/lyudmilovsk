#include "VendingMachine.h"

#include "BirdFeedItem.h"
#include "BottleItem.h"
#include "LyudGoodsItem.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "HeroCharacter.h"
#include "HeroReachComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	/** What the machine sells: four drinks and, in the fifth slot, a bag of bird feed (Flavor none; Count handfuls). */
	struct FDrink { const TCHAR* Item; const TCHAR* Flavor; int32 Price; int32 Count; };
	const FDrink Drinks[] = {
		{ TEXT("drink_baikal"), TEXT("baikal"), 25, 1 },
		{ TEXT("drink_dyushes"), TEXT("dyushes"), 25, 1 },
		{ TEXT("drink_buratino"), TEXT("buratino"), 25, 1 },
		{ TEXT("drink_tarkhun"), TEXT("tarkhun"), 25, 1 },
		{ TEXT("bird_feed"), nullptr, 15, 5 },
	};
	constexpr int32 NumDrinks = 4;
	constexpr int32 MaxSlots = 9;

	/** The bottle flavor of an item (empty if it is not a labelled bottle). */
	FName FlavorOf(FName Item)
	{
		const FLyudItemDef* D = ULyudInventory::FindDef(Item);
		return D ? D->Flavor : FName();
	}

	/** Content/Transport/lobby.json "stock" <Preset>: [{"item", "price", "count"}...] */
	TArray<FVendingSlot> LoadPreset(FName Preset)
	{
		TArray<FVendingSlot> Out;
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Transport/lobby.json"))))
		{
			return Out;
		}
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			return Out;
		}
		const TSharedPtr<FJsonObject>* Stock = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (Root->TryGetObjectField(TEXT("stock"), Stock) && (*Stock)->TryGetArrayField(Preset.ToString(), List))
		{
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FVendingSlot S;
				S.Item = FName(*O->GetStringField(TEXT("item")));
				S.Price = O->HasField(TEXT("price")) ? (int32)O->GetNumberField(TEXT("price")) : 0;
				S.Count = O->HasField(TEXT("count")) ? (int32)O->GetNumberField(TEXT("count")) : 1;
				if (ULyudInventory::FindDef(S.Item))
				{
					Out.Add(S);
				}
			}
		}
		return Out;
	}
	constexpr float DispenseTime = 0.7f;
}

AVendingMachine::AVendingMachine()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	UStaticMesh* Cube = nullptr;
	auto Plate = [this, Root, &Cube](const TCHAR* Name, const FVector& Loc, const FVector& Size)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		if (!Cube)
		{
			Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		}
		C->SetStaticMesh(Cube);
		C->SetRelativeLocation(Loc);
		C->SetRelativeScale3D(Size / 100.f);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		return C;
	};
	// the delivery tray under the flap: a dark plastic shelf with a back lip, a hand's width deep
	Tray = Plate(TEXT("Tray"), FVector(37.5f + 7.f, 10.f, 12.f), FVector(14.f, 44.f, 1.6f));
	TrayBack = Plate(TEXT("TrayLip"), FVector(37.5f + 13.4f, 10.f, 14.f), FVector(1.2f, 44.f, 4.5f));
}

void AVendingMachine::BeginPlay()
{
	Super::BeginPlay();
	ResolveSlots();
	if (bStockShelves)
	{
		bClassic ? StockShelves() : StockShelvesUniversal();
	}
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Base)
	{
		for (UStaticMeshComponent* C : { Tray.Get(), TrayBack.Get() })
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.025f, 0.025f, 0.03f));
			C->SetMaterial(0, MID);
		}
	}
}

void AVendingMachine::ResolveSlots()
{
	if (Slots.IsEmpty() && !Preset.IsNone())
	{
		Slots = LoadPreset(Preset);
	}
	bClassic = Slots.IsEmpty();
	if (bClassic)
	{
		for (const FDrink& D : Drinks)
		{
			FVendingSlot S;
			S.Item = D.Item;
			S.Price = D.Flavor ? 0 : D.Price;
			S.Count = D.Count;
			Slots.Add(S);
		}
	}
	if (Slots.Num() > MaxSlots)
	{
		Slots.SetNum(MaxSlots);
	}
}

void AVendingMachine::StockShelvesUniversal()
{
	// goods lie behind the glass, one product per column and two of each, in rows three wide (the model's: lobby_props.py Vending, shelves 30 cm apart)
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	constexpr float ShelfZ0 = 58.f, ShelfPitch = 38.f, FrontX = 12.f, MidY = -9.f, ColPitch = 25.f;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 I = 0; I < Slots.Num(); ++I)
	{
		const FLyudItemDef* D = ULyudInventory::FindDef(Slots[I].Item);
		if (!D)
		{
			continue;
		}
		for (int32 Copy = 0; Copy < 2; ++Copy)
		{
			const FVector Local(FrontX - Copy * 12.f, MidY + ((I % 3) - 1) * ColPitch + (Copy ? 2.f : -2.f), ShelfZ0 + (I / 3) * ShelfPitch);
			AActor* G = nullptr;
			if (!D->Flavor.IsNone())
			{
				ABottleItem* B = World->SpawnActor<ABottleItem>(GetActorTransform().TransformPosition(Local + FVector(0, 0, ABottleItem::BodyRadius + 0.2f)), FRotator::ZeroRotator, Params);
				if (B)
				{
					B->Init(D->Flavor, 1.f);
					B->LieDown(B->GetActorLocation(), GetActorRightVector());
					B->SetActorScale3D(FVector(0.9f));
				}
				G = B;
			}
			else if (D->Look != ELyudItemLook::Auto)
			{
				ALyudGoodsItem* Shown = World->SpawnActor<ALyudGoodsItem>(GetActorTransform().TransformPosition(Local), GetActorRotation(), Params);
				if (Shown)
				{
					Shown->Init(Slots[I].Item);
					const float Tall = FMath::Max(D->Size.Y, D->Size.Z);
					const float Fit = FMath::Min(1.f, 30.f / Tall);
					Shown->SetActorScale3D(FVector(Fit));
					// stand it on the shelf; flat things lean against the back, picture out
					Shown->SetActorLocation(GetActorTransform().TransformPosition(Local + FVector(0, 0, Shown->HalfHeight() * Fit + 0.2f)));
				}
				G = Shown;
			}
			if (G)
			{
				G->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
				Stock.Add(G);
			}
		}
	}
}

void AVendingMachine::StockShelves()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// five glass shelves behind the window (the model's own: 5 shelves 22 cm apart from 50 cm up, window 65 cm wide, centred 11.75 cm
	// to the machine's right); the bottles lie across them three high and three wide, labels out toward the glass
	constexpr float Scale = 0.9f, ShelfZ0 = 50.4f, ShelfPitch = 22.f, FrontX = 24.2f, MidY = 11.75f;
	const float Len = 2.f * ABottleItem::BottleHalfLength * Scale, R = ABottleItem::BodyRadius * Scale;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 Shelf = 0; Shelf < 5; ++Shelf)
	{
		if (Shelf == 0)
		{
			// the bottom shelf: bags of bird feed standing in a row, fronts to the glass
			for (int32 Col = -2; Col <= 3; ++Col)
			{
				const FVector Local(FrontX + 0.6f, MidY + (Col - 0.5f) * (2.f * ABirdFeedBag::HalfWidth + 0.8f), ShelfZ0 + ABirdFeedBag::HalfHeight + 0.2f);
				ABirdFeedBag* Bag = World->SpawnActor<ABirdFeedBag>(GetActorTransform().TransformPosition(Local), GetActorRotation() + FRotator(0.f, (Col % 2) * 4.f - 2.f, 0.f), Params);
				if (Bag)
				{
					Bag->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
					Stock.Add(Bag);
				}
			}
			continue;
		}
		const TCHAR* Kind = Drinks[(Shelf % NumDrinks)].Flavor;
		for (int32 Level = 0; Level < 3; ++Level)
		{
			for (int32 Col = -1; Col <= 1; ++Col)
			{
				const FVector Local(FrontX + (Level % 2) * 1.2f, MidY + Col * (Len + 0.4f), ShelfZ0 + Shelf * ShelfPitch + R + 0.2f + Level * 2.f * R * 0.96f);
				ABottleItem* B = World->SpawnActor<ABottleItem>(GetActorTransform().TransformPosition(Local), FRotator::ZeroRotator, Params);
				if (!B)
				{
					continue;
				}
				B->Init(Kind, 1.f);
				const FVector Neck = GetActorRightVector() * ((Shelf + Col + Level) % 2 ? 1.f : -1.f);
				B->SetActorRotation(FRotationMatrix::MakeFromZX(Neck, GetActorForwardVector()).Rotator());
				B->SetActorScale3D(FVector(Scale));
				B->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
				Stock.Add(B);
			}
		}
	}
}

void AVendingMachine::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, FText::GetEmpty());
	}
	Super::EndPlay(Reason);
}

FVector AVendingMachine::TrayRest() const
{
	// the bottle lies on the tray: across the machine, label up
	return GetActorTransform().TransformPosition(FVector(37.5f + 7.5f, 10.f, 13.f + ABottleItem::BodyRadius + 0.2f));
}

FText AVendingMachine::PromptFor(int32 Sel) const
{
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	const FText Name = ULyudInventory::ItemName(Slots[Sel].Item);
	const int32 Cost = PriceOf(Sel);
	FString Choose;
	if (bClassic)
	{
		TArray<FStringFormatArg> Names;
		for (const FVendingSlot& S : Slots)
		{
			Names.Add(FStringFormatArg(ULyudInventory::ItemName(S.Item).ToString()));
		}
		Choose = LyudText::Get(TEXT("ui.vending.choose"), Names).ToString();
	}
	else
	{
		Choose = LyudText::Get(TEXT("ui.vending.pick"), { FStringFormatArg(Sel + 1), FStringFormatArg(Slots.Num()) }).ToString();
	}
	if (Inv && Inv->Rubles < Cost)
	{
		return FText::FromString(LyudText::Get(TEXT("ui.vending.nomoney"), { FStringFormatArg(Name.ToString()), FStringFormatArg(Cost), FStringFormatArg(Inv->Rubles) }).ToString()
			+ TEXT("     ") + Choose);
	}
	return FText::FromString(LyudText::Get(TEXT("ui.vending.buy"), { FStringFormatArg(Name.ToString()), FStringFormatArg(Cost) }).ToString()
		+ TEXT("     ") + Choose);
}

int32 AVendingMachine::PriceOf(int32 Sel) const
{
	// a slot's own price; else, in the classic machine, bottles cost the machine's Price (set per machine); else the item's usual
	const FVendingSlot& S = Slots[Sel];
	if (S.Price > 0)
	{
		return S.Price;
	}
	if (!FlavorOf(S.Item).IsNone())
	{
		return Price;
	}
	const FLyudItemDef* D = ULyudInventory::FindDef(S.Item);
	return D && D->Price > 0 ? D->Price : Price;
}

void AVendingMachine::Buy()
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	UWorld* World = GetWorld();
	if (!Inv || !World || !Inv->Spend(PriceOf(Flavor)))
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Start = GetActorTransform().TransformPosition(FlapLocal - FVector(10.f, 0.f, 0.f));
	const FName Sold = Slots[Flavor].Item;
	if (!FlavorOf(Sold).IsNone())
	{
		ABottleItem* B = World->SpawnActor<ABottleItem>(Start, FRotator::ZeroRotator, Params);
		if (B)
		{
			B->Init(FlavorOf(Sold), 1.f);
			B->LieDown(B->GetActorLocation(), GetActorRightVector());
		}
		Goods = B;
	}
	else if (Sold != TEXT("bird_feed"))
	{
		ALyudGoodsItem* G = World->SpawnActor<ALyudGoodsItem>(Start, FRotator::ZeroRotator, Params);
		if (G)
		{
			G->Init(Sold);
			G->LieDown(Start, GetActorRightVector());
		}
		Goods = G;
	}
	else
	{
		ABirdFeedBag* Bag = World->SpawnActor<ABirdFeedBag>(Start, FRotator::ZeroRotator, Params);
		if (Bag)
		{
			Bag->LieDown(Start, GetActorRightVector());
		}
		Goods = Bag;
	}
	State = EState::Dispensing;
	Clock = 0.f;
}

FVector AVendingMachine::TrayRestFor(const AActor* Item) const
{
	// a bag lies on its back, thinner than a bottle
	if (const ALyudGoodsItem* G = Cast<ALyudGoodsItem>(Item))
	{
		return GetActorTransform().TransformPosition(FVector(37.5f + 7.5f, 10.f, 13.f + FMath::Min(G->HalfDepth(), 4.f) + 0.2f));
	}
	return Cast<ABirdFeedBag>(Item)
		? GetActorTransform().TransformPosition(FVector(37.5f + 7.5f, 10.f, 13.f + ABirdFeedBag::HalfDepth + 0.2f))
		: TrayRest();
}

void AVendingMachine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (!Hero || !PC || !UI)
	{
		return;
	}

	if (State == EState::Dispensing)
	{
		Clock += DeltaSeconds;
		const float T = FMath::Clamp(Clock / DispenseTime, 0.f, 1.f);
		if (AActor* Item = Goods.Get())
		{
			// the bottle (or the bag) slides out of the flap, then drops the last few centimetres onto the tray
			const FVector Start = GetActorTransform().TransformPosition(FlapLocal - FVector(10.f, 0.f, 0.f));
			const FVector Mid = GetActorTransform().TransformPosition(FlapLocal + FVector(3.f, 0.f, 0.f));
			const FVector End = TrayRestFor(Item);
			const FVector P = T < 0.5f ? FMath::Lerp(Start, Mid, FMath::SmoothStep(0.f, 1.f, T / 0.5f))
			                          : FMath::Lerp(Mid, End, FMath::Square((T - 0.5f) / 0.5f));
			Item->SetActorLocation(P);
		}
		if (T >= 1.f)
		{
			State = EState::Taking;
			UHeroReachComponent* Reach = UHeroReachComponent::Get(Hero);
			const FName ItemId = Slots[Flavor].Item;
			const int32 Count = Slots[Flavor].Count;
			FReachItem Reachable;
			if (const ABottleItem* B = Cast<ABottleItem>(Goods.Get()))
			{
				Reachable = B->MakeReachItem();
			}
			else if (const ALyudGoodsItem* G = Cast<ALyudGoodsItem>(Goods.Get()))
			{
				Reachable = G->MakeReachItem();
			}
			else if (const ABirdFeedBag* Bag = Cast<ABirdFeedBag>(Goods.Get()))
			{
				Reachable = Bag->MakeReachItem();
			}
			if (!Goods || !Reach || !Reach->Take(Reachable, [this, ItemId, Count]()
			{
				if (ULyudInventory* Inv = ULyudInventory::Get(this))
				{
					Inv->Add(ItemId, Count);
				}
				State = EState::Idle;
			}))
			{
				// the hand is busy: the player gets it anyway
				if (ULyudInventory* Inv = ULyudInventory::Get(this))
				{
					Inv->Add(ItemId, Count);
				}
				if (Goods)
				{
					Goods->Destroy();
				}
				State = EState::Idle;
			}
		}
		UI->SetPrompt(this, FText::GetEmpty());
		bPromptShown = false;
		return;
	}
	if (State == EState::Taking)
	{
		UI->SetPrompt(this, FText::GetEmpty());
		bPromptShown = false;
		return;
	}

	// idle: is the player at the machine, looking at it?
	const FVector Flap = GetActorTransform().TransformPosition(FlapLocal);
	const FVector Eye = Hero->EyeLocation();
	const float Flat = FVector::Dist2D(Hero->GetActorLocation(), Flap);
	const FVector ToMachine = (GetActorLocation() + FVector(0.f, 0.f, 100.f) - Eye).GetSafeNormal();
	const bool bFront = FVector::DotProduct(GetActorForwardVector(), (Hero->GetActorLocation() - GetActorLocation()).GetSafeNormal2D()) > 0.3f;
	const bool bLooking = FVector::DotProduct(PC->GetControlRotation().Vector(), ToMachine) > 0.55f;
	const bool bNear = Flat < UseDistance && Flat > 35.f && FMath::Abs((Hero->GetActorLocation() - Flap).Z) < 130.f;
	UHeroReachComponent* Reach = Hero->FindComponentByClass<UHeroReachComponent>();
	const bool bFree = !UI->IsOpen() && !(Reach && Reach->IsBusy()) && !Hero->IsHidden();
	if (bNear && bFront && bLooking && bFree)
	{
		const FKey Keys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
		static_assert(UE_ARRAY_COUNT(Keys) == MaxSlots, "a key per slot");
		for (int32 I = 0; I < Slots.Num(); ++I)
		{
			if (PC->WasInputKeyJustPressed(Keys[I]))
			{
				Flavor = I;
			}
		}
		UI->SetPrompt(this, PromptFor(Flavor));
		bPromptShown = true;
		const ULyudInventory* Inv = ULyudInventory::Get(this);
		if (PC->WasInputKeyJustPressed(EKeys::E) && Inv && Inv->Rubles >= PriceOf(Flavor))
		{
			Buy();
		}
	}
	else if (bPromptShown)
	{
		UI->SetPrompt(this, FText::GetEmpty());
		bPromptShown = false;
	}
}

void AVendingMachine::DebugBuy(int32 Slot)
{
	if (State == EState::Idle && Slot >= 0 && Slot < Slots.Num())
	{
		Flavor = Slot;
		Buy();
	}
}

static FAutoConsoleCommandWithWorldAndArgs GVendingBuy(TEXT("Vending.Buy"),
	TEXT("Vending.Buy [slot 1-5]: the nearest machine sells to the hero as if E was pressed (headless checks)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
		AVendingMachine* Best = nullptr;
		for (TActorIterator<AVendingMachine> It(World); It && Pawn; ++It)
		{
			if (!Best || FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation()) < FVector::Dist(Best->GetActorLocation(), Pawn->GetActorLocation()))
			{
				Best = *It;
			}
		}
		if (Best)
		{
			Best->DebugBuy((Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 5) - 1);
		}
	}));
