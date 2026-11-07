#include "Fishing/LyudFishBook.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Fishing/LyudFishing.h"
#include "FishingModel.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "UI/LyudBirdGuide.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudUIStyle.h"
#include "UI/LyudUIText.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "World/LyudPrefetch.h"

namespace LyudFishBook
{

	/** The book's printing: dark blue ink on a warm off-white page. */
	const FLinearColor BookInk(0.06f, 0.11f, 0.2f);
	const FLinearColor BookInkSoft(0.3f, 0.34f, 0.42f);
	const FLinearColor BookInkFaint(0.55f, 0.57f, 0.6f);
	const FLinearColor BookPaper(0.93f, 0.91f, 0.85f);
	/** The hero's own notes in the margin: pencil. */
	const FLinearColor BookPencil(0.32f, 0.3f, 0.28f);

	FName ItemOfFish(int32 C)
	{
		return FName(ANSI_TO_TCHAR(LyudFishing::ItemOf(LyudFishing::Catchable(C).Id).c_str()));
	}

	FName SpeciesOfFish(int32 C)
	{
		return FName(ANSI_TO_TCHAR(LyudFishing::Catchable(C).Id));
	}

	/** The baits it takes well (liking 0.35 and up), best first, as their names joined. */
	FString BaitsFor(int32 C)
	{
		TArray<TPair<float, int32>> Liked;
		for (int32 B = 0; B < LyudFishing::NumBaits(); B++)
		{
			const float L = LyudFishing::BaitLiking(C, B);
			if (L >= 0.35f)
			{
				Liked.Add({ L, B });
			}
		}
		Liked.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key > B.Key; });
		TArray<FString> Names;
		for (const TPair<float, int32>& P : Liked)
		{
			Names.Add(ULyudInventory::ItemName(FName(ANSI_TO_TCHAR(LyudFishing::Bait(P.Value).Item))).ToString());
		}
		return FString::Join(Names, *LyudText::Get(TEXT("ui.fishbook.list_sep")).ToString());
	}
}

/** The book open: the list of the fish on the left page, the chosen one's page on the right. */
class SLyudFishBookPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudFishBookPage) {}
		SLATE_ARGUMENT(ULyudInventoryUI*, UI)
		SLATE_ARGUMENT(int32, Selected)
		SLATE_EVENT(FOnInt32ValueChanged, OnSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		using namespace LyudFishBook;
		UI = InArgs._UI;
		Selected = FMath::Clamp(InArgs._Selected, 0, LyudFishing::NumCatchable() - 1);
		OnSelected = InArgs._OnSelected;
		static const FSlateRoundedBoxBrush Paper(BookPaper, 8.f, FLinearColor(0.28f, 0.32f, 0.4f), 2.f);
		static const FSlateColorBrush Fold(FLinearColor(0.f, 0.f, 0.f, 0.12f));
		List = SNew(SVerticalBox);
		Right = SNew(SBox).WidthOverride(560.f);
		RebuildList();
		RebuildRight();

		const ULyudFishingState* State = ULyudFishingState::Get(UI.Get());
		const int32 Kinds = State ? State->Log.Num() : 0;
		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SImage).Image(LyudUI::ShadeBrush())]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBorder).BorderImage(&Paper).Padding(FMargin(30.f, 22.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(LyudUI::Font(30, "Bold")).ColorAndOpacity(BookInk).Text(LyudText::Get(TEXT("ui.fishbook.title")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 14)
					[
						SNew(STextBlock).Font(LyudUI::Font(16, "Italic")).ColorAndOpacity(BookInkSoft).Text(LyudText::Get(TEXT("ui.fishbook.subtitle")))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).WidthOverride(380.f).HeightOverride(640.f)[SNew(SScrollBox) + SScrollBox::Slot()[List.ToSharedRef()]]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(18, 0)
						[
							SNew(SBox).WidthOverride(2.f).HeightOverride(640.f)[SNew(SImage).Image(&Fold)]
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).HeightOverride(640.f)[Right.ToSharedRef()]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(STextBlock).Font(LyudUI::Font(15, "Italic")).ColorAndOpacity(BookPencil)
							.Text(LyudText::Get(TEXT("ui.fishbook.totals"), { FStringFormatArg(State ? State->Total : 0), FStringFormatArg(Kinds),
								FStringFormatArg(LyudFishing::NumCatchable()), FStringFormatArg(State ? State->Cooked : 0) }))
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(STextBlock).Font(LyudUI::Font(14)).ColorAndOpacity(BookInkSoft).Text(LyudText::Get(TEXT("ui.fishbook.hint")))
						]
					]
				]
			]
		];
	}

private:
	TWeakObjectPtr<ULyudInventoryUI> UI;
	int32 Selected = 0;
	FOnInt32ValueChanged OnSelected;
	TSharedPtr<SVerticalBox> List;
	TSharedPtr<SBox> Right;

	const FLyudCatchRecord* Record(int32 C) const
	{
		const ULyudFishingState* State = ULyudFishingState::Get(UI.Get());
		const FName Species = LyudFishBook::SpeciesOfFish(C);
		return State ? State->Log.FindByPredicate([Species](const FLyudCatchRecord& R) { return R.Species == Species; }) : nullptr;
	}

	void Select(int32 C)
	{
		Selected = C;
		OnSelected.ExecuteIfBound(C);
		// rebuilt on the next frame, not inside the click of a row the rebuild removes
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float)
		{
			RebuildList();
			RebuildRight();
			return EActiveTimerReturnType::Stop;
		}));
	}

	void RebuildList()
	{
		using namespace LyudFishBook;
		List->ClearChildren();
		for (int32 C = 0; C < LyudFishing::NumCatchable(); C++)
		{
			const FLyudCatchRecord* R = Record(C);
			const FName Item = ItemOfFish(C);
			const bool bSel = C == Selected;
			List->AddSlot().AutoHeight().Padding(0, 0, 8, 3)
			[
				SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).ContentPadding(FMargin(6.f, 3.f))
				.OnClicked_Lambda([this, C]() { Select(C); return FReply::Handled(); })
				[
					SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(bSel ? FLinearColor(0.2f, 0.3f, 0.5f, 0.14f) : FLinearColor::Transparent).Padding(FMargin(4.f, 2.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
						[
							SNew(SBox).WidthOverride(40.f).HeightOverride(40.f)
							[
								SNew(SImage).Image(UI.IsValid() ? UI->IconBrush(Item) : nullptr)
								.ColorAndOpacity(R ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.35f))
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(LyudUI::Font(17, bSel ? "Bold" : "Regular")).ColorAndOpacity(R ? BookInk : BookInkFaint)
							.Text(ULyudInventory::ItemName(Item))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(LyudUI::Font(16, "Italic")).ColorAndOpacity(BookPencil)
							.Text(R ? FText::AsNumber(R->Count) : FText::GetEmpty())
						]
					]
				]
			];
		}
	}

	TSharedRef<SWidget> Line(const FText& Text, int32 Size, const FLinearColor& Color, const char* Face = "Regular", float Top = 6.f) const
	{
		return SNew(SBox).Padding(FMargin(0.f, Top, 0.f, 0.f))
		[
			SNew(STextBlock).Font(LyudUI::Font(Size, Face)).ColorAndOpacity(Color).AutoWrapText(true).Text(Text)
		];
	}

	void RebuildRight()
	{
		using namespace LyudFishBook;
		const int32 C = Selected;
		const LyudFishing::FCatchable& F = LyudFishing::Catchable(C);
		const FName Item = ItemOfFish(C);
		const FLyudCatchRecord* R = Record(C);
		const FString Baits = BaitsFor(C);
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		Box->AddSlot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(SBox).WidthOverride(220.f).HeightOverride(220.f)
			[
				SNew(SImage).Image(UI.IsValid() ? UI->IconBrush(Item) : nullptr).ColorAndOpacity(R ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.55f))
			]
		];
		Box->AddSlot().AutoHeight()[Line(ULyudInventory::ItemName(Item), 28, BookInk, "Bold", 10.f)];
		Box->AddSlot().AutoHeight()[Line(ULyudInventory::ItemDescription(Item), 16, BookInkSoft)];
		Box->AddSlot().AutoHeight()[Line(LyudText::Get(TEXT("ui.fishbook.size"), { FStringFormatArg(FMath::RoundToInt(F.Length * 100.f)) }), 16, BookInk, "Regular", 12.f)];
		Box->AddSlot().AutoHeight()[Line(Baits.IsEmpty() ? LyudText::Get(TEXT("ui.fishbook.bait_none"))
			: LyudText::Get(TEXT("ui.fishbook.bait"), { FStringFormatArg(Baits) }), 16, BookInk)];
		Box->AddSlot().AutoHeight()[Line(F.PerKilo > 0 ? LyudText::Get(TEXT("ui.fishbook.price"), { FStringFormatArg(F.PerKilo) })
			: LyudText::Get(TEXT("ui.fishbook.price_none")), 16, BookInk)];
		// the hero's own notes, in pencil
		if (R)
		{
			Box->AddSlot().AutoHeight()[Line(LyudText::Get(TEXT("ui.fishbook.caught"), { FStringFormatArg(R->Count) }), 18, BookPencil, "Italic", 18.f)];
			Box->AddSlot().AutoHeight()[Line(LyudText::Get(TEXT("ui.fishbook.biggest"),
				{ FStringFormatArg(FMath::RoundToInt(R->BiggestGrams)), FStringFormatArg(FMath::RoundToInt(R->BiggestCm)) }), 18, BookPencil, "Italic")];
			Box->AddSlot().AutoHeight()[Line(LyudText::Get(TEXT("ui.fishbook.first"), { FStringFormatArg(LyudUIText::Date(R->First).ToString()) }),
				18, BookPencil, "Italic")];
		}
		else
		{
			Box->AddSlot().AutoHeight()[Line(LyudText::Get(TEXT("ui.fishbook.not_yet")), 18, BookInkFaint, "Italic", 18.f)];
		}
		Right->SetContent(SNew(SScrollBox) + SScrollBox::Slot()[Box]);
	}
};

// ------------------------------------------------------------------ the screen

const FName ULyudFishBookScreen::ItemId(TEXT("fish_book"));

ULyudFishBookScreen* ULyudFishBookScreen::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudFishBookScreen>() : nullptr;
}

bool ULyudFishBookScreen::IsOwned(const UObject* WorldContext)
{
	const ULyudInventory* Inv = ULyudInventory::Get(WorldContext);
	return Inv && Inv->Count(ItemId) > 0;
}

void ULyudFishBookScreen::Deinitialize()
{
	Close();
	Super::Deinitialize();
}

void ULyudFishBookScreen::Lock(bool bLock)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}
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

void ULyudFishBookScreen::Open(FName Species)
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	const ULyudGameScreen* GameScreen = ULyudGameScreen::Get(World);
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	// one book open at a time: not over the bird guide
	const ULyudBirdGuideScreen* Guide = ULyudBirdGuideScreen::Get(World);
	if (IsOpen() || !Viewport || !UI || !IsOwned(World) || (GameScreen && GameScreen->IsOpen()) || UI->IsOpen() || (Guide && Guide->IsOpen()))
	{
		return;
	}
	if (!Species.IsNone())
	{
		const int32 C = LyudFishing::FindCatchable(TCHAR_TO_ANSI(*Species.ToString()));
		LastRead = C >= 0 ? C : LastRead;
	}
	TWeakObjectPtr<ULyudFishBookScreen> WeakThis(this);
	Page = SNew(SLyudFishBookPage).UI(UI).Selected(LastRead)
		.OnSelected_Lambda([WeakThis](int32 C) { if (WeakThis.IsValid()) { WeakThis->LastRead = C; } });
	Viewport->AddViewportWidgetContent(Page.ToSharedRef(), 65);
	OpenedFrame = GFrameCounter;
	Lock(true);
	UE_LOG(LogTemp, Display, TEXT("FISHING book open at %d"), LastRead);
}

void ULyudFishBookScreen::Close()
{
	if (!Page)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport())
		{
			Viewport->RemoveViewportWidgetContent(Page.ToSharedRef());
		}
	}
	Page.Reset();
	// the pause menu or the diary opening over it (Esc in a packaged game) set the input themselves: leave it to them
	UWorld* World = GetWorld();
	const ULyudGameMenuSubsystem* Menu = World ? World->GetSubsystem<ULyudGameMenuSubsystem>() : nullptr;
	const ULyudGameScreen* GameScreen = ULyudGameScreen::Get(World);
	if (!(Menu && Menu->IsOpen()) && !(GameScreen && GameScreen->IsOpen()))
	{
		Lock(false);
	}
}

void ULyudFishBookScreen::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !World->IsGameWorld())
	{
		return;
	}
	// once the book is in the pocket its pictures load in the background, so the first opening doesn't stop the game
	IconsIn -= DeltaTime;
	if (!bIconsReady && IconsIn <= 0.f && IsOwned(World))
	{
		IconsIn = 1.f;
		bIconsReady = true;
		for (int32 C = 0; C < LyudFishing::NumCatchable(); C++)
		{
			const FString Name = FString(TEXT("T_Icon_")) + LyudFishBook::ItemOfFish(C).ToString();
			for (const FString& Lang : { LyudText::Language(), FString(TEXT("ru")) })
			{
				bIconsReady &= ULyudPrefetch::Ready(World, FString::Printf(TEXT("/Game/Inventory/Localized/%s/%s.%s"), *Lang, *Name, *Name));
			}
		}
	}
	const bool bN = PC->WasInputKeyJustPressed(EKeys::N);
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	if (IsOpen())
	{
		const ULyudGameScreen* GameScreen = ULyudGameScreen::Get(World);
		const bool bOther = (Menu && Menu->IsOpen()) || (GameScreen && GameScreen->IsOpen());
		if (OpenedFrame != GFrameCounter && (bN || PC->WasInputKeyJustPressed(EKeys::Escape) || bOther || !IsOwned(World)))
		{
			Close();
		}
		return;
	}
	if (bN && !UGameplayStatics::IsGamePaused(World) && !(Menu && Menu->IsOpen()))
	{
		Open();
	}
}

static FAutoConsoleCommandWithWorldAndArgs GFishBookOpen(TEXT("lyud.Fish.Book"),
	TEXT("lyud.Fish.Book [close|<Species>]: opens the angler's book (gives it first if the hero has none; checks), or shuts it"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudFishBookScreen* S = ULyudFishBookScreen::Get(World);
		if (!S)
		{
			return;
		}
		if (Args.Num() > 0 && Args[0] == TEXT("close"))
		{
			S->Close();
			return;
		}
		if (ULyudInventory* Inv = ULyudInventory::Get(World); Inv && !ULyudFishBookScreen::IsOwned(World))
		{
			Inv->Add(ULyudFishBookScreen::ItemId);
		}
		S->Open(Args.Num() > 0 ? FName(*Args[0]) : NAME_None);
	}));
