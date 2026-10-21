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
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudSave.h"
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
		auto Drink = [&D](const TCHAR* Id, const TCHAR* Name, const TCHAR* Desc, const TCHAR* Flavor)
		{
			FLyudItemDef I;
			I.Id = Id;
			I.Name = FText::FromString(Name);
			I.Description = FText::FromString(Desc);
			I.Kind = ELyudItemKind::Drink;
			I.Leaves = TEXT("empty_bottle");
			I.Flavor = Flavor;
			I.Radius = ABottleItem::BodyRadius;
			I.HalfLength = 6.f;
			D.Add(I);
		};
		Drink(TEXT("drink_baikal"), TEXT("Байкал"), TEXT("Газировка на травах, холодная из автомата. Пахнет хвоей."), TEXT("baikal"));
		Drink(TEXT("drink_dyushes"), TEXT("Дюшес"), TEXT("Грушевый лимонад. Липкий и сладкий, как в детстве."), TEXT("dyushes"));
		Drink(TEXT("drink_buratino"), TEXT("Буратино"), TEXT("Тёмный лимонад с цитрусами. Щиплет язык."), TEXT("buratino"));
		Drink(TEXT("drink_tarkhun"), TEXT("Тархун"), TEXT("Зелёная газировка на эстрагоне."), TEXT("tarkhun"));
		FLyudItemDef Empty;
		Empty.Id = TEXT("empty_bottle");
		Empty.Name = FText::FromString(TEXT("Пустая бутылка"));
		Empty.Description = FText::FromString(TEXT("Стеклянная, пол-литра. Когда-то за неё давали деньги."));
		Empty.Flavor = TEXT("baikal");
		D.Add(Empty);
		FLyudItemDef Apple;
		Apple.Id = TEXT("apple");
		Apple.Name = FText::FromString(TEXT("Яблоко"));
		Apple.Description = FText::FromString(TEXT("С яблони у выхода. Кисло-сладкое."));
		Apple.Kind = ELyudItemKind::Food;
		Apple.Radius = 3.8f;
		D.Add(Apple);
		return D;
	}
}

const TArray<FLyudItemDef>& ULyudInventory::AllDefs()
{
	static const TArray<FLyudItemDef> Defs = MakeDefs();
	return Defs;
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
							SNew(STextBlock).Font(Small).Text(Def ? Def->Name : FText::FromName(Id)).AutoWrapText(true)
							.Justification(ETextJustify::Center)
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							SNew(STextBlock).Font(Small).ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
							.Text(P.Value > 1 ? FText::FromString(FString::Printf(TEXT("x%d"), P.Value)) : FText::GetEmpty())
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
				SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 22)).Text(SelDef->Name)
			];
			Detail->AddSlot().AutoHeight().Padding(0, 0, 0, 10)
			[
				SNew(STextBlock).Font(Body).Text(SelDef->Description).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.8f))
			];
			if (SelDef->Kind != ELyudItemKind::Misc)
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
						SNew(STextBlock).Font(Body).Text(FText::FromString(SelDef->Kind == ELyudItemKind::Drink ? TEXT("  Выпить  ") : TEXT("  Съесть  ")))
					]
				];
			}
		}
		else
		{
			Detail->AddSlot().AutoHeight()
			[
				SNew(STextBlock).Font(Body).Text(FText::FromString(TEXT("В карманах пусто."))).ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
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
								SNew(STextBlock).Font(Title).Text(FText::FromString(TEXT("Инвентарь")))
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock).Font(Body).ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.45f))
								.Text(FText::FromString(FString::Printf(TEXT("Деньги: %d руб."), Inv->Rubles)))
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
							.Text(FText::FromString(TEXT("Tab или I — закрыть")))
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
	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Inventory/T_Icon_%s.T_Icon_%s"), *Id.ToString(), *Id.ToString()));
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
	if (!Inv || !Def || !Reach || Reach->IsBusy() || Inv->Count(Id) < 1 || Def->Kind == ELyudItemKind::Misc)
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
