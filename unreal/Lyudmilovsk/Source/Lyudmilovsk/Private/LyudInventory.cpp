#include "LyudInventory.h"

#include "BottleItem.h"
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
		D.Add(Feed);
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

void ULyudInventory::Reset()
{
	Counts.Reset();
	Rubles = 100;
	Equipped = NAME_None;
	OnChanged.Broadcast();
}

void ULyudInventory::Add(FName Id, int32 N)
{
	if (N > 0)
	{
		Counts.FindOrAdd(Id) += N;
		OnChanged.Broadcast();
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
	}
	OnChanged.Broadcast();
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

class SLyudInventory : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudInventory) {}
		SLATE_ARGUMENT(ULyudInventoryUI*, Owner)
		SLATE_ARGUMENT(ULyudInventory*, Inventory)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Owner = InArgs._Owner;
		Inventory = InArgs._Inventory;
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
		const FSlateFontInfo Title = FCoreStyle::GetDefaultFontStyle("Bold", 28);
		const FSlateFontInfo Body = FCoreStyle::GetDefaultFontStyle("Regular", 18);
		const FSlateFontInfo Small = FCoreStyle::GetDefaultFontStyle("Regular", 14);

		TSharedRef<SWrapBox> Grid = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8.f, 8.f));
		const TArray<TPair<FName, int32>> Held = Inv->Held();
		if (Selected.IsNone() || !Inv->Count(Selected))
		{
			Selected = Held.Num() ? Held[0].Key : FName();
		}
		for (const TPair<FName, int32>& P : Held)
		{
			const FLyudItemDef* Def = ULyudInventory::FindDef(P.Key);
			const FName Id = P.Key;
			const bool bSel = Id == Selected;
			TWeakObjectPtr<ULyudInventoryUI> WeakUI = Owner;
			Grid->AddSlot()
			[
				SNew(SButton)
				.ButtonColorAndOpacity(bSel ? FLinearColor(1.f, 0.75f, 0.35f, 1.f) : FLinearColor(0.35f, 0.35f, 0.4f, 1.f))
				.OnClicked_Lambda([this, Id]() { Selected = Id; Rebuild(); return FReply::Handled(); })
				[
					SNew(SBox).WidthOverride(104.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							SNew(SBox).WidthOverride(80.f).HeightOverride(80.f)
							[
								SNew(SImage).Image(UI->IconBrush(Id))
							]
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							SNew(STextBlock).Font(Small).Text(Def ? ULyudInventory::ItemName(Id) : FText::FromName(Id)).AutoWrapText(true)
							.Justification(ETextJustify::Center)
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							SNew(STextBlock).Font(Small).ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
							.Text(P.Value > 1 ? FText::FromString(FString::Printf(TEXT("x%d"), P.Value)) : FText::GetEmpty())
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							SNew(STextBlock).Font(Small).ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.45f))
							.Text(Inv->EquippedItem() == Id ? LyudText::Get(TEXT("ui.inventory.inhand")) : FText::GetEmpty())
						]
					]
				]
			];
		}

		const FLyudItemDef* SelDef = ULyudInventory::FindDef(Selected);
		TSharedRef<SVerticalBox> Detail = SNew(SVerticalBox);
		if (SelDef)
		{
			Detail->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
			[
				SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 22)).Text(ULyudInventory::ItemName(SelDef->Id))
			];
			Detail->AddSlot().AutoHeight().Padding(0, 0, 0, 10)
			[
				SNew(STextBlock).Font(Body).Text(ULyudInventory::ItemDescription(SelDef->Id)).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.8f))
			];
			if (SelDef->Kind == ELyudItemKind::Feed)
			{
				const FName Id = SelDef->Id;
				const bool bInHand = Inv->EquippedItem() == Id;
				Detail->AddSlot().AutoHeight().HAlign(HAlign_Left)
				[
					SNew(SButton)
					.OnClicked_Lambda([this, Id, bInHand]()
					{
						if (ULyudInventory* I = Inventory.Get())
						{
							I->Equip(bInHand ? FName() : Id);
						}
						if (ULyudInventoryUI* UI2 = Owner.Get(); UI2 && !bInHand)
						{
							UI2->Close();
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Font(Body).Text(FText::FromString(TEXT("  ") + LyudText::Get(bInHand ? TEXT("ui.use.unequip") : TEXT("ui.use.equip")).ToString() + TEXT("  ")))
					]
				];
			}
			else if (SelDef->Kind != ELyudItemKind::Misc)
			{
				const FName Id = SelDef->Id;
				Detail->AddSlot().AutoHeight().HAlign(HAlign_Left)
				[
					SNew(SButton)
					.OnClicked_Lambda([this, Id]()
					{
						if (ULyudInventoryUI* UI2 = Owner.Get())
						{
							UI2->Use(Id);
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Font(Body).Text(FText::FromString(TEXT("  ") + LyudText::Get(SelDef->Kind == ELyudItemKind::Drink ? TEXT("ui.use.drink") : TEXT("ui.use.eat")).ToString() + TEXT("  ")))
					]
				];
			}
		}
		else
		{
			Detail->AddSlot().AutoHeight()
			[
				SNew(STextBlock).Font(Body).Text(LyudText::Get(TEXT("ui.inventory.empty"))).ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
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
					SNew(SBox).WidthOverride(680.f).MinDesiredHeight(380.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f)
							[
								SNew(STextBlock).Font(Title).Text(LyudText::Get(TEXT("ui.inventory.title")))
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock).Font(Body).ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.45f))
								.Text(LyudText::Get(TEXT("ui.inventory.money"), { FStringFormatArg(Inv->Rubles) }))
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)
						[
							Grid
						]
						+ SVerticalBox::Slot().FillHeight(1.f)
						[
							Detail
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
						[
							SNew(STextBlock).Font(Small).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
							.Text(LyudText::Get(TEXT("ui.inventory.close")))
						]
					]
				]
			]
		];
	}

private:
	TWeakObjectPtr<ULyudInventoryUI> Owner;
	TWeakObjectPtr<ULyudInventory> Inventory;
	FName Selected;
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
	Close();
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

bool ULyudInventoryUI::CanOpen() const
{
	const UWorld* World = GetWorld();
	if (!World || bOpen || bUsing)
	{
		return false;
	}
	const AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Hero || Hero->IsHidden() || UGameplayStatics::IsGamePaused(World))
	{
		return false;
	}
	if (const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>(); Menu && Menu->IsOpen())
	{
		return false;
	}
	const UHeroReachComponent* Reach = Hero->FindComponentByClass<UHeroReachComponent>();
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
	const bool bToggle = PC->WasInputKeyJustPressed(EKeys::Tab) || PC->WasInputKeyJustPressed(EKeys::I);
	if (bOpen)
	{
		const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
		if (bToggle || PC->WasInputKeyJustPressed(EKeys::Escape) || (Menu && Menu->IsOpen()))
		{
			Close();
		}
	}
	else if (bToggle && CanOpen())
	{
		Open();
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
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	ULyudInventory* Inv = ULyudInventory::Get(World);
	if (!Viewport || !Inv || bOpen)
	{
		return;
	}
	Screen = SNew(SLyudInventory).Owner(this).Inventory(Inv);
	Viewport->AddViewportWidgetContent(Screen.ToSharedRef(), 70);
	Inv->OnChanged.AddWeakLambda(this, [this] { if (Screen) { Screen->Rebuild(); } });
	bOpen = true;
	Lock(true);
}

void ULyudInventoryUI::Close()
{
	if (!bOpen)
	{
		return;
	}
	bOpen = false;
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport(); Viewport && Screen)
		{
			Viewport->RemoveViewportWidgetContent(Screen.ToSharedRef());
		}
		if (ULyudInventory* Inv = ULyudInventory::Get(World))
		{
			Inv->OnChanged.RemoveAll(this);
		}
	}
	Screen.Reset();
	Lock(false);
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
