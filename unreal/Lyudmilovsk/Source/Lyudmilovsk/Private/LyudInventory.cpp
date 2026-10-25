#include "LyudInventory.h"

#include "BoothOwner.h"
#include "BottleItem.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HeroCharacter.h"
#include "HeroReachComponent.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

// ------------------------------------------------------------------ the item table

namespace
{
	TArray<FLyudItemDef> MakeDefs()
	{
		TArray<FLyudItemDef> D;
		auto Drink = [&D](const TCHAR* Id, const TCHAR* Flavor)
		{
			FLyudItemDef I;
			I.Id = Id;
			I.Kind = ELyudItemKind::Drink;
			I.Leaves = TEXT("empty_bottle");
			I.Flavor = Flavor;
			I.Radius = ABottleItem::BodyRadius;
			I.HalfLength = 6.f;
			D.Add(I);
		};
		Drink(TEXT("drink_baikal"), TEXT("baikal"));
		Drink(TEXT("drink_dyushes"), TEXT("dyushes"));
		Drink(TEXT("drink_buratino"), TEXT("buratino"));
		Drink(TEXT("drink_tarkhun"), TEXT("tarkhun"));
		FLyudItemDef Empty;
		Empty.Id = TEXT("empty_bottle");
		Empty.Flavor = TEXT("baikal");
		D.Add(Empty);
		FLyudItemDef Apple;
		Apple.Id = TEXT("apple");
		Apple.Kind = ELyudItemKind::Food;
		Apple.Radius = 3.8f;
		D.Add(Apple);
		// a paper bag of seeds and millet from the vending machine; each unit is one handful for one bird
		FLyudItemDef Feed;
		Feed.Id = TEXT("bird_feed");
		Feed.Kind = ELyudItemKind::Feed;
		Feed.Radius = 4.f;
		Feed.Slot = ELyudEquipSlot::Hand;
		D.Add(Feed);
		// things to wear: the clothes the hero woke up in (blender/hero/build_hero.py), a head lamp and binoculars from the club owner
		auto Wear = [&D](const TCHAR* Id, ELyudEquipSlot Slot)
		{
			FLyudItemDef I;
			I.Id = Id;
			I.Kind = ELyudItemKind::Wear;
			I.Slot = Slot;
			D.Add(I);
		};
		Wear(TEXT("head_lamp"), ELyudEquipSlot::Head);
		Wear(TEXT("binoculars"), ELyudEquipSlot::Neck);
		Wear(TEXT("overshirt"), ELyudEquipSlot::Outer);
		Wear(TEXT("hoodie"), ELyudEquipSlot::Body);
		Wear(TEXT("cargo_trousers"), ELyudEquipSlot::Legs);
		Wear(TEXT("sneakers"), ELyudEquipSlot::Feet);
		return D;
	}
}

const TArray<FLyudItemDef>& ULyudInventory::AllDefs()
{
	static const TArray<FLyudItemDef> Defs = MakeDefs();
	return Defs;
}

FText ULyudInventory::ItemName(FName Id)
{
	return LyudText::Get(FString::Printf(TEXT("item.%s.name"), *Id.ToString()));
}

FText ULyudInventory::ItemDescription(FName Id)
{
	return LyudText::Get(FString::Printf(TEXT("item.%s.desc"), *Id.ToString()));
}

const FLyudItemDef* ULyudInventory::FindDef(FName Id)
{
	return AllDefs().FindByPredicate([Id](const FLyudItemDef& D) { return D.Id == Id; });
}

// ------------------------------------------------------------------ the pocket

ULyudInventory* ULyudInventory::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<ULyudInventory>() : nullptr;
}

void ULyudInventory::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (ULyudSaveSubsystem* Saves = Collection.InitializeDependency<ULyudSaveSubsystem>())
	{
		Saves->RegisterSaveObject(TEXT("Inventory"), this);
	}
	Reset();
}

namespace
{
	// for trying the birds right away (Dima, 2026-10-25), until feed is sold in the game proper: a new game starts with a
	// bag of feed in the hand. 0 = start with empty pockets.
	TAutoConsoleVariable<int32> CVarStartFeed(TEXT("lyud.StartFeed"), 5,
		TEXT("Handfuls of bird feed in the hero's hand when a game starts (a testing aid; 0 = none)."));
}

void ULyudInventory::Reset()
{
	Counts.Reset();
	Rubles = 100;
	Equipped = NAME_None;
	Worn.Reset();
	for (const TCHAR* Clothes : { TEXT("overshirt"), TEXT("hoodie"), TEXT("cargo_trousers"), TEXT("sneakers") })
	{
		Counts.Add(Clothes, 1);
		Worn.Add(Clothes);
	}
	if (const int32 Feed = CVarStartFeed.GetValueOnGameThread(); Feed > 0)
	{
		Counts.Add(TEXT("bird_feed"), Feed);
		Equipped = TEXT("bird_feed");
	}
	OnChanged.Broadcast();
}

void ULyudInventory::Add(FName Id, int32 N)
{
	if (N > 0)
	{
		Counts.FindOrAdd(Id) += N;
		OnChanged.Broadcast();
		OnAdded.Broadcast(Id, N);
		ULyudQuests::Notify(this, TEXT("get"), Id.ToString(), N);
	}
}

bool ULyudInventory::Remove(FName Id, int32 N)
{
	int32* C = Counts.Find(Id);
	if (!C || *C < N)
	{
		return false;
	}
	*C -= N;
	if (*C <= 0)
	{
		Counts.Remove(Id);
		if (Equipped == Id)
		{
			Equipped = NAME_None;
		}
		Worn.Remove(Id);
	}
	OnChanged.Broadcast();
	ULyudQuests::Notify(this, TEXT("use"), Id.ToString(), N);
	return true;
}

int32 ULyudInventory::Count(FName Id) const
{
	const int32* C = Counts.Find(Id);
	return C ? *C : 0;
}

bool ULyudInventory::Spend(int32 Amount)
{
	if (Amount > Rubles)
	{
		return false;
	}
	Rubles -= Amount;
	OnChanged.Broadcast();
	return true;
}

void ULyudInventory::Equip(FName Id)
{
	const FName New = Id.IsNone() || Count(Id) > 0 ? Id : FName();
	if (New != Equipped)
	{
		Equipped = New;
		OnChanged.Broadcast();
		if (!New.IsNone())
		{
			ULyudQuests::Notify(this, TEXT("wear"), New.ToString());
		}
	}
}

ELyudEquipSlot ULyudInventory::SlotOf(FName Id)
{
	const FLyudItemDef* D = FindDef(Id);
	return D ? D->Slot : ELyudEquipSlot::None;
}

const TArray<ELyudEquipSlot>& ULyudInventory::Slots()
{
	static const TArray<ELyudEquipSlot> All = { ELyudEquipSlot::Head, ELyudEquipSlot::Neck, ELyudEquipSlot::Outer, ELyudEquipSlot::Body,
		ELyudEquipSlot::Legs, ELyudEquipSlot::Feet, ELyudEquipSlot::Hand };
	return All;
}

FText ULyudInventory::SlotName(ELyudEquipSlot Slot)
{
	static const TCHAR* Keys[] = { TEXT("none"), TEXT("head"), TEXT("neck"), TEXT("outer"), TEXT("body"), TEXT("legs"), TEXT("feet"), TEXT("hand") };
	return LyudText::Get(FString(TEXT("ui.slot.")) + Keys[(int32)Slot]);
}

FName ULyudInventory::InSlot(ELyudEquipSlot Slot) const
{
	if (Slot == ELyudEquipSlot::Hand)
	{
		return EquippedItem();
	}
	for (const FName& W : Worn)
	{
		if (SlotOf(W) == Slot && Count(W) > 0)
		{
			return W;
		}
	}
	return FName();
}

bool ULyudInventory::Wear(FName Id)
{
	const ELyudEquipSlot Slot = SlotOf(Id);
	if (Slot == ELyudEquipSlot::None || Count(Id) < 1)
	{
		return false;
	}
	if (Slot == ELyudEquipSlot::Hand)
	{
		Equip(Id);
		return true;
	}
	if (Worn.Contains(Id))
	{
		return true;
	}
	Worn.RemoveAll([Slot](const FName& W) { return SlotOf(W) == Slot; });
	Worn.Add(Id);
	OnChanged.Broadcast();
	ULyudQuests::Notify(this, TEXT("wear"), Id.ToString());
	return true;
}

void ULyudInventory::TakeOff(ELyudEquipSlot Slot)
{
	if (Slot == ELyudEquipSlot::Hand)
	{
		Equip(FName());
		return;
	}
	if (Worn.RemoveAll([Slot](const FName& W) { return SlotOf(W) == Slot; }) > 0)
	{
		OnChanged.Broadcast();
	}
}

void ULyudInventory::Earn(int32 Amount)
{
	Rubles += Amount;
	OnChanged.Broadcast();
}

TArray<TPair<FName, int32>> ULyudInventory::Held() const
{
	TArray<TPair<FName, int32>> Out;
	for (const FLyudItemDef& D : AllDefs())
	{
		if (const int32 C = Count(D.Id))
		{
			Out.Add({ D.Id, C });
		}
	}
	// items from outside the table (a later system) still show up
	for (const TPair<FName, int32>& P : Counts)
	{
		if (!FindDef(P.Key))
		{
			Out.Add(P);
		}
	}
	return Out;
}

// ------------------------------------------------------------------ the screen

class SLyudPrompt : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudPrompt) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&)
	{
		ChildSlot
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 0.f, 120.f))
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.55f))
				.Padding(FMargin(18.f, 8.f))
				[
					SAssignNew(Label, STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 20)).ColorAndOpacity(FLinearColor(1.f, 0.9f, 0.7f))
					.Justification(ETextJustify::Center)
				]
			]
		];
	}

	void SetText(const FText& T) { Label->SetText(T); }

private:
	TSharedPtr<STextBlock> Label;
};

/** A trader's screen: the goods with their icons, price and a buy button; the hero's money on top. */
class SLyudShop : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudShop) {}
		SLATE_ARGUMENT(ULyudInventoryUI*, Owner)
		SLATE_ARGUMENT(ULyudInventory*, Inventory)
		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(TArray<FLyudShopGood>, Goods)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Owner = InArgs._Owner;
		Inventory = InArgs._Inventory;
		Title = InArgs._Title;
		Goods = InArgs._Goods;
		Rebuild();
	}

	void Rebuild()
	{
		ULyudInventory* Inv = Inventory.Get();
		ULyudInventoryUI* UI = Owner.Get();
		if (!Inv || !UI)
		{
			return;
		}
		const FSlateFontInfo Body = FCoreStyle::GetDefaultFontStyle("Regular", 18);
		const FSlateFontInfo Small = FCoreStyle::GetDefaultFontStyle("Regular", 14);
		TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
		for (int32 GoodIndex = 0; GoodIndex < Goods.Num(); ++GoodIndex)
		{
			const FLyudShopGood G = Goods[GoodIndex];
			const bool bCanPay = Inv->Rubles >= G.Price;
			List->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.12f, 1.f))
				.Padding(10.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(72.f).HeightOverride(72.f)[SNew(SImage).Image(UI->IconBrush(G.Item))]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(14, 0)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
							.Text(G.Count > 1 ? FText::FromString(ULyudInventory::ItemName(G.Item).ToString() + TEXT("  ") + LyudText::Get(TEXT("ui.shop.count"), { FStringFormatArg(G.Count) }).ToString())
							                  : ULyudInventory::ItemName(G.Item))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(Small).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.75f)).Text(ULyudInventory::ItemDescription(G.Item))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 14, 0)
					[
						SNew(STextBlock).Font(Body).ColorAndOpacity(bCanPay ? FLinearColor(1.f, 0.85f, 0.45f) : FLinearColor(0.8f, 0.3f, 0.25f))
						.Text(LyudText::Get(TEXT("ui.shop.price"), { FStringFormatArg(G.Price) }))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SButton).IsEnabled(bCanPay)
						.OnClicked_Lambda([this, G, GoodIndex]()
						{
							ULyudInventoryUI* U = Owner.Get();
							Status = U && U->BuyShopGood(GoodIndex) ? LyudText::Get(TEXT("ui.shop.bought"), { FStringFormatArg(ULyudInventory::ItemName(G.Item).ToString()) })
							                                         : LyudText::Get(TEXT("ui.shop.nomoney"));
							Rebuild();
							return FReply::Handled();
						})
						[
							SNew(STextBlock).Font(Body).Text(FText::FromString(TEXT("  ") + LyudText::Get(TEXT("ui.shop.buy")).ToString() + TEXT("  ")))
						]
					]
				]
			];
		}
		ChildSlot
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.03f, 0.03f, 0.04f, 0.93f))
				.Padding(24.f)
				[
					SNew(SBox).WidthOverride(700.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 26)).Text(Title)]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock).Font(Body).ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.45f))
								.Text(LyudText::Get(TEXT("ui.inventory.money"), { FStringFormatArg(Inv->Rubles) }))
							]
						]
						+ SVerticalBox::Slot().AutoHeight()[List]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
						[
							SNew(STextBlock).Font(Body).ColorAndOpacity(FLinearColor(0.6f, 0.9f, 0.6f)).Text(Status)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
						[
							SNew(STextBlock).Font(Small).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f)).Text(LyudText::Get(TEXT("ui.shop.close")))
						]
					]
				]
			]
		];
	}

private:
	TWeakObjectPtr<ULyudInventoryUI> Owner;
	TWeakObjectPtr<ULyudInventory> Inventory;
	FText Title;
	TArray<FLyudShopGood> Goods;
	FText Status;
};

ULyudInventoryUI* ULyudInventoryUI::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudInventoryUI>() : nullptr;
}

void ULyudInventoryUI::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	LyudText::Reload();
	// every level start is a new game unless a load restores the pocket right after
	if (ULyudInventory* Inv = ULyudInventory::Get(&InWorld))
	{
		Inv->Reset();
	}
}

void ULyudInventoryUI::Deinitialize()
{
	CloseShop();
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport(); Viewport && Prompt)
		{
			Viewport->RemoveViewportWidgetContent(Prompt.ToSharedRef());
		}
	}
	Prompt.Reset();
	Super::Deinitialize();
}

bool ULyudInventoryUI::IsOpen() const
{
	const ULyudGameScreen* GameScreen = ULyudGameScreen::Get(this);
	return bShop || (GameScreen && GameScreen->IsOpen());
}

bool ULyudInventoryUI::CanOpen(bool bNeedHero) const
{
	const UWorld* World = GetWorld();
	if (!World || IsOpen() || bUsing || ShopClosedFrame == GFrameCounter)
	{
		return false;
	}
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	const AHeroCharacter* Hero = Cast<AHeroCharacter>(Pawn);
	if (!Pawn || (bNeedHero && (!Hero || Hero->IsHidden())) || UGameplayStatics::IsGamePaused(World))
	{
		return false;
	}
	if (const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>(); Menu && Menu->IsOpen())
	{
		return false;
	}
	const UHeroReachComponent* Reach = Hero ? Hero->FindComponentByClass<UHeroReachComponent>() : nullptr;
	return !(Reach && Reach->IsBusy());
}

void ULyudInventoryUI::Tick(float)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}
	// Tab / I / J / M for the game screen are read by ULyudGameScreen
	if (bShop)
	{
		const bool bToggle = PC->WasInputKeyJustPressed(EKeys::Tab) || PC->WasInputKeyJustPressed(EKeys::I);
		const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
		if (bToggle || PC->WasInputKeyJustPressed(EKeys::T) || PC->WasInputKeyJustPressed(EKeys::Escape) || (Menu && Menu->IsOpen()))
		{
			CloseShop();
		}
	}
	if (bUsing)
	{
		const AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
		const UHeroReachComponent* Reach = Hero ? Hero->FindComponentByClass<UHeroReachComponent>() : nullptr;
		bUsing = Reach && Reach->IsBusy();
	}
}

void ULyudInventoryUI::Lock(bool bLock)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || bLocked == bLock)
	{
		return;
	}
	bLocked = bLock;
	PC->SetIgnoreLookInput(bLock);
	PC->SetIgnoreMoveInput(bLock);
	PC->bShowMouseCursor = bLock;
	if (bLock)
	{
		PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void ULyudInventoryUI::Open()
{
	if (bShop)
	{
		return;
	}
	if (ULyudGameScreen* GameScreen = ULyudGameScreen::Get(this))
	{
		GameScreen->Open(ELyudScreenTab::Inventory);
	}
}

void ULyudInventoryUI::OpenShop(const FText& Title, const TArray<FLyudShopGood>& Goods)
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	ULyudInventory* Inv = ULyudInventory::Get(World);
	if (!Viewport || !Inv || !CanOpen())
	{
		return;
	}
	ShopGoods = Goods;
	Shop = SNew(SLyudShop).Owner(this).Inventory(Inv).Title(Title).Goods(Goods);
	Viewport->AddViewportWidgetContent(Shop.ToSharedRef(), 70);
	Inv->OnChanged.AddWeakLambda(this, [this] { if (Shop) { Shop->Rebuild(); } });
	bShop = true;
	Lock(true);
}

bool ULyudInventoryUI::BuyShopGood(int32 Index)
{
	ULyudInventory* Inv = ULyudInventory::Get(GetWorld());
	if (!Inv || !bShop || !ShopGoods.IsValidIndex(Index) || !Inv->Spend(ShopGoods[Index].Price))
	{
		return false;
	}
	Inv->Add(ShopGoods[Index].Item, ShopGoods[Index].Count);
	return true;
}

void ULyudInventoryUI::CloseShop()
{
	if (!bShop)
	{
		return;
	}
	bShop = false;
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport(); Viewport && Shop)
		{
			Viewport->RemoveViewportWidgetContent(Shop.ToSharedRef());
		}
		if (ULyudInventory* Inv = ULyudInventory::Get(World))
		{
			Inv->OnChanged.RemoveAll(this);
		}
	}
	Shop.Reset();
	ShopClosedFrame = GFrameCounter;
	Lock(false);
}

void ULyudInventoryUI::Close()
{
	if (ULyudGameScreen* GameScreen = ULyudGameScreen::Get(this))
	{
		GameScreen->Close();
	}
}

void ULyudInventoryUI::SetPrompt(const UObject* Key, const FText& Text)
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport)
	{
		return;
	}
	if (Text.IsEmpty())
	{
		if (PromptKey.Get() == Key && Prompt)
		{
			Viewport->RemoveViewportWidgetContent(Prompt.ToSharedRef());
			Prompt.Reset();
			PromptKey.Reset();
		}
		return;
	}
	if (!Prompt)
	{
		Prompt = SNew(SLyudPrompt);
		Viewport->AddViewportWidgetContent(Prompt.ToSharedRef(), 60);
	}
	PromptKey = Key;
	Prompt->SetText(Text);
}

const FSlateBrush* ULyudInventoryUI::IconBrush(FName Id)
{
	if (const TSharedPtr<FSlateBrush>* B = Brushes.Find(Id))
	{
		return B->Get();
	}
	TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
	Brush->ImageSize = FVector2D(80.f, 80.f);
	UTexture2D* Tex = LyudText::LoadTexture(TEXT("Inventory"), FString(TEXT("T_Icon_")) + Id.ToString());
	if (Tex)
	{
		Icons.Add(Id, Tex);
		Brush->SetResourceObject(Tex);
	}
	else
	{
		Brush->TintColor = FLinearColor(0.3f, 0.3f, 0.35f, 1.f);
		Brush->DrawAs = ESlateBrushDrawType::Box;
	}
	Brushes.Add(Id, Brush);
	return Brush.Get();
}

AActor* ULyudInventoryUI::SpawnHeld(FName Id, const FVector& Pos)
{
	UWorld* World = GetWorld();
	const FLyudItemDef* Def = ULyudInventory::FindDef(Id);
	if (!World || !Def)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (!Def->Flavor.IsNone())
	{
		ABottleItem* Bottle = World->SpawnActor<ABottleItem>(Pos, FRotator::ZeroRotator, Params);
		if (Bottle)
		{
			Bottle->Init(Def->Flavor, Def->Id == TEXT("empty_bottle") ? 0.f : 1.f);
		}
		return Bottle;
	}
	// a plain round item (the apple): an engine sphere
	AStaticMeshActor* Ball = World->SpawnActor<AStaticMeshActor>(Pos, FRotator::ZeroRotator, Params);
	if (Ball)
	{
		Ball->SetMobility(EComponentMobility::Movable);
		UStaticMeshComponent* C = Ball->GetStaticMeshComponent();
		C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		C->SetWorldScale3D(FVector(Def->Radius * 2.f / 100.f));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Ball);
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.55f, 0.12f, 0.04f));
			C->SetMaterial(0, MID);
		}
	}
	return Ball;
}

void ULyudInventoryUI::Use(FName Id)
{
	UWorld* World = GetWorld();
	ULyudInventory* Inv = ULyudInventory::Get(World);
	const FLyudItemDef* Def = ULyudInventory::FindDef(Id);
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	UHeroReachComponent* Reach = UHeroReachComponent::Get(Hero);
	if (!Inv || !Def || !Reach || Reach->IsBusy() || Inv->Count(Id) < 1 || Def->Kind == ELyudItemKind::Misc || Def->Kind == ELyudItemKind::Feed)
	{
		return;
	}
	Close();

	// the item appears in front of the hand and the hand takes it
	const APlayerController* PC = Cast<APlayerController>(Hero->GetController());
	const FRotator Yaw(0.f, PC ? PC->GetControlRotation().Yaw : Hero->GetActorRotation().Yaw, 0.f);
	const FVector Pos = Hero->EyeLocation() + Yaw.RotateVector(FVector(38.f, 14.f, -17.f));
	AActor* Held = SpawnHeld(Id, Pos);
	if (!Held)
	{
		return;
	}
	FReachItem Item;
	if (ABottleItem* Bottle = Cast<ABottleItem>(Held))
	{
		Bottle->SetActorRotation(Yaw);
		Item = Bottle->MakeReachItem();
	}
	else
	{
		Item.Actor = Held;
		Item.Center = Pos;
		Item.Axis = FVector::UpVector;
		Item.Radius = Def->Radius;
		Item.Front = Yaw.Vector();
	}
	const FName Leaves = Def->Leaves;
	TWeakObjectPtr<AActor> WeakHeld = Held;
	TWeakObjectPtr<UHeroReachComponent> WeakReach = Reach;
	auto Consumed = [this, Id, Leaves, WeakReach]()
	{
		if (UHeroReachComponent* R = WeakReach.Get())
		{
			R->Stow([this, Id, Leaves]()
			{
				if (ULyudInventory* I = ULyudInventory::Get(this))
				{
					I->Remove(Id);
					if (!Leaves.IsNone())
					{
						I->Add(Leaves);
					}
				}
			});
		}
	};
	bUsing = true;
	if (Def->Kind == ELyudItemKind::Drink)
	{
		Reach->Drink(Item, 4.5f,
			[WeakHeld](float Left) { if (ABottleItem* B = Cast<ABottleItem>(WeakHeld.Get())) { B->SetLevel(Left); } },
			Consumed);
	}
	else
	{
		const FVector Scale0 = Held->GetActorScale3D();
		Reach->Eat(Item, 3, [WeakHeld, Scale0](float Left) { if (AActor* A = WeakHeld.Get()) { A->SetActorScale3D(Scale0 * FMath::Lerp(0.55f, 1.f, Left)); } }, Consumed);
	}
}

// --- dev commands for headless checks: the screen and the hand without the keyboard ---
static FAutoConsoleCommandWithWorldAndArgs GInvOpen(TEXT("Inventory.Open"), TEXT("Inventory.Open [0]: opens (or with 0 closes) the inventory screen"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(World))
		{
			if (Args.Num() > 0 && FCString::Atoi(*Args[0]) == 0)
			{
				UI->Close();
			}
			else
			{
				UI->Open();
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GInvEquip(TEXT("Inventory.Equip"), TEXT("Inventory.Equip <item id> | none: puts the item in the hero's hand"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudInventory* Inv = ULyudInventory::Get(World))
		{
			Inv->Equip(Args.Num() > 0 && Args[0] != TEXT("none") ? FName(*Args[0]) : FName());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GShopOpen(TEXT("Shop.Open"), TEXT("Shop.Open [buy index | close]: opens the club owner's trade screen (headless checks); with an index, buys that good; close closes it"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
		if (!UI)
		{
			return;
		}
		if (Args.Num() > 0 && Args[0] == TEXT("close"))
		{
			UI->CloseShopForTest();
			return;
		}
		if (Args.Num() > 0 && UI->IsShopOpen())
		{
			UE_LOG(LogTemp, Warning, TEXT("SHOPTEST buy %d -> %d, rubles %d, feed %d"), FCString::Atoi(*Args[0]), UI->BuyShopGood(FCString::Atoi(*Args[0])) ? 1 : 0,
				ULyudInventory::Get(World)->Rubles, ULyudInventory::Get(World)->Count(TEXT("bird_feed")));
			return;
		}
		// the club owner's goods, as he sells them at his window
		TActorIterator<ABoothOwner> Owner(World);
		TArray<FLyudShopGood> Goods = { FLyudShopGood{ TEXT("bird_feed"), 12, 5 } };
		if (Owner)
		{
			Goods = (*Owner)->Goods();
		}
		UI->OpenShop(LyudText::Get(TEXT("ui.shop.title"), { FStringFormatArg(LyudText::Get(TEXT("npc.owner.name")).ToString()) }), Goods);
		UE_LOG(LogTemp, Warning, TEXT("SHOPTEST open -> %d"), UI->IsShopOpen() ? 1 : 0);
	}));
