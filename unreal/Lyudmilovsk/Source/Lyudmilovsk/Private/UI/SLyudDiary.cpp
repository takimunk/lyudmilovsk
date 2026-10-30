#include "UI/SLyudDiary.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Plants/GameCalendar.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/CoreStyle.h"
#include "UI/LyudBirdGuide.h"
#include "UI/LyudQuests.h"
#include "UI/LyudUIText.h"
#include "UI/SLyudMapView.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

using namespace LyudDiary;

namespace
{
	TAutoConsoleVariable<float> CVarPictureScale(TEXT("lyud.Diary.PictureScale"), 1.5f,
		TEXT("The pictures of the diary's pages (the 3D book, a turning page): render-target pixels per design unit."));
	TAutoConsoleVariable<int32> CVarPictureGamma(TEXT("lyud.Diary.PictureGamma"), 0,
		TEXT("1: the page pictures are drawn with gamma correction (if the turning page looks lighter or darker than the page)."));

	uint32 SeedOf(const FString& S)
	{
		return GetTypeHash(S) | 1u;
	}

	uint32 SeedOf(FName N, uint32 Salt = 0)
	{
		return (GetTypeHash(N) ^ (Salt * 2654435761u)) | 1u;
	}

	TSharedRef<STextBlock> Words(const FText& T, float Size, const FLinearColor& Color, bool bBold = false, bool bWrap = false)
	{
		return SNew(STextBlock).Text(T).Font(Hand(Size, bBold)).ColorAndOpacity(Color).AutoWrapText(bWrap);
	}

	TSharedRef<STextBlock> Key(const TCHAR* K, float Size, const FLinearColor& Color, bool bBold = false, bool bWrap = false)
	{
		return Words(LyudText::Get(K), Size, Color, bBold, bWrap);
	}

	/** A word with a hand-drawn mark on it (a title underlined twice, a done task struck through). */
	TSharedRef<SWidget> Marked(TSharedRef<SWidget> W, EMark Kind, const FLinearColor& Color, uint32 Seed, float Thickness = 2.2f)
	{
		return SNew(SOverlay)
			+ SOverlay::Slot()[W]
			+ SOverlay::Slot()[SNew(SLyudInkMark).Kind(Kind).Color(Color).Seed(Seed).Thickness(Thickness)];
	}

	TSharedRef<SWidget> Title(const FText& T)
	{
		return SNew(SBox).Padding(FMargin(0, 0, 0, 18)).HAlign(HAlign_Left)
		[
			Marked(Words(T, 50.f, Ink, true), EMark::DoubleUnderline, InkSoft, SeedOf(T.ToString()), 2.6f)
		];
	}

	TSharedRef<SWidget> Subtitle(const FText& T)
	{
		return SNew(SBox).Padding(FMargin(0, 10, 0, 8)).HAlign(HAlign_Left)
		[
			Marked(Words(T, 34.f, Pencil, true), EMark::Underline, PencilFaint, SeedOf(T.ToString()) + 3, 1.6f)
		];
	}

	/** bWrap only where the link is given a width (a filled slot): in a wrap box a wrapping text shrinks to its last width. */
	TSharedRef<SWidget> Link(const FText& T, TFunction<void()> Do, uint32 Seed, float Size = 30.f, bool bWrap = false)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(bWrap ? VAlign_Top : VAlign_Center)[Words(FText::FromString(TEXT("→ ")), Size, InkSoft)];
		if (bWrap)
		{
			Row->AddSlot().FillWidth(1.f).VAlign(VAlign_Top)[Words(T, Size, Ink, true, true)];
		}
		else
		{
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center)[Words(T, Size, Ink, true)];
		}
		return SNew(SLyudInkButton).HoverMark(EMark::Underline).Seed(Seed)
			.OnClicked_Lambda([Do]() { Do(); })
			[
				SNew(SBox).Padding(FMargin(4, 0))[Row]
			];
	}

	/** A pencil sketch of an item; the coloured icon, pale, when it has no sketch yet. */
	TSharedRef<SWidget> Sketch(ULyudInventoryUI* UI, FName Id, float Size)
	{
		const FSlateBrush* B = Id.IsNone() ? nullptr : SketchBrush(Id);
		TSharedRef<SWidget> Img = B ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(B))
			: UI && !Id.IsNone() ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(UI->IconBrush(Id)).ColorAndOpacity(FLinearColor(1.f, 0.96f, 0.88f, 0.85f)))
			: StaticCastSharedRef<SWidget>(SNew(SSpacer));
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)[Img];
	}

	TSharedRef<SWidget> Scroll(TSharedRef<SWidget> W)
	{
		return SNew(SScrollBox).ScrollBarVisibility(EVisibility::Collapsed) + SScrollBox::Slot()[W];
	}

	const TCHAR* SectionKey(ESection S)
	{
		switch (S)
		{
		case ESection::Things: return TEXT("ui.diary.things");
		case ESection::Quests: return TEXT("ui.diary.quests");
		case ESection::Diary: return TEXT("ui.diary.diary");
		default: return TEXT("ui.diary.map");
		}
	}
}

// ------------------------------------------------------------------ marks and buttons

void SLyudInkMark::Construct(const FArguments& InArgs)
{
	Kind = InArgs._Kind;
	Color = InArgs._Color;
	Thickness = InArgs._Thickness;
	Seed = InArgs._Seed;
	Show = InArgs._Show;
	SetVisibility(EVisibility::HitTestInvisible);
}

int32 SLyudInkMark::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (Show.Get(true))
	{
		DrawMark(Out, LayerId, G, Kind, FVector2D::ZeroVector, G.GetLocalSize(), Color * InWidgetStyle.GetColorAndOpacityTint(), Thickness, Seed);
	}
	return LayerId + 1;
}

void SLyudInkButton::Construct(const FArguments& InArgs)
{
	HoverMark = InArgs._HoverMark;
	SelectedMark = InArgs._SelectedMark;
	bSelected = InArgs._Selected;
	Seed = InArgs._Seed;
	MarkColor = InArgs._MarkColor;
	SelectedColor = InArgs._SelectedColor;
	Inset = InArgs._Inset;
	OnClicked = InArgs._OnClicked;
	ChildSlot[InArgs._Content.Widget];
}

int32 SLyudInkButton::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Pos(Inset.Left, Inset.Top), Size = G.GetLocalSize() - FVector2D(Inset.Left + Inset.Right, Inset.Top + Inset.Bottom);
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
	// the highlighter goes under the words, a circle or a box over them
	// and stays inside the button: its broad strokes would run onto the next row's words
	if (bSelected && SelectedMark == EMark::Highlight)
	{
		Out.PushClip(FSlateClippingZone(G.MakeChild(Size + FVector2D(16.f, 0.f), FSlateLayoutTransform(Pos - FVector2D(8.f, 0.f))).ToPaintGeometry()));
		DrawMark(Out, LayerId, G, SelectedMark, Pos, Size, SelectedColor * Tint, 3.f, Seed);
		Out.PopClip();
	}
	const int32 Top = SCompoundWidget::OnPaint(Args, G, MyCullingRect, Out, LayerId + 1, InWidgetStyle, bParentEnabled);
	if (bSelected && SelectedMark != EMark::Highlight)
	{
		DrawMark(Out, Top + 1, G, SelectedMark, Pos, Size, SelectedColor * Tint, 2.6f, Seed);
	}
	if (IsHovered() && !(bSelected && SelectedMark == HoverMark))
	{
		DrawMark(Out, Top + 1, G, HoverMark, Pos, Size, MarkColor * Tint * FLinearColor(1, 1, 1, bPressed ? 1.f : 0.75f), 2.f, Seed + 11);
	}
	return Top + 2;
}

FReply SLyudInkButton::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& E)
{
	if (E.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bPressed = true;
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	return FReply::Unhandled();
}

FReply SLyudInkButton::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& E)
{
	if (E.GetEffectingButton() != EKeys::LeftMouseButton || !bPressed)
	{
		return FReply::Unhandled();
	}
	bPressed = false;
	if (MyGeometry.IsUnderLocation(E.GetScreenSpacePosition()))
	{
		// a copy: the click may rebuild the page this button is on
		FSimpleDelegate Do = OnClicked;
		Do.ExecuteIfBound();
	}
	return FReply::Handled().ReleaseMouseCapture();
}

FCursorReply SLyudInkButton::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return FCursorReply::Cursor(EMouseCursor::Hand);
}

// ------------------------------------------------------------------ a spread

void SLyudDiarySpread::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	Inventory = ULyudInventory::Get(Owner.Get());
	InventoryUI = ULyudInventoryUI::Get(Owner.Get());
	Spread = InArgs._Spread;
	State = InArgs._State.IsValid() ? InArgs._State : MakeShared<FLyudDiaryState>();
	PageNumber = InArgs._PageNumber;
	OnChanged = InArgs._OnChanged;

	TSharedRef<SWidget> L = SNew(SSpacer), R = SNew(SSpacer);
	switch (Spread.Section)
	{
	case ESection::Things:
		L = ThingsLeft();
		R = ThingsRight();
		break;
	case ESection::Quests:
		L = QuestsLeft();
		R = QuestsRight();
		break;
	case ESection::Diary:
		L = DiaryPage(Spread.Part * 2);
		R = DiaryPage(Spread.Part * 2 + 1);
		break;
	default:
		break;
	}
	if (Spread.Section == ESection::Map)
	{
		ChildSlot
		[
			SNew(SBox).WidthOverride(SpreadW).HeightOverride(PageH)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Page(false, SNew(SSpacer))]
					+ SHorizontalBox::Slot().AutoWidth()[Page(true, SNew(SSpacer))]
				]
				+ SOverlay::Slot()[MapSpread()]
			]
		];
		return;
	}
	ChildSlot
	[
		SNew(SBox).WidthOverride(SpreadW).HeightOverride(PageH)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Page(false, L)]
			+ SHorizontalBox::Slot().AutoWidth()[Page(true, R)]
		]
	];
}

TSharedRef<SWidget> SLyudDiarySpread::Page(bool bRight, TSharedRef<SWidget> Content)
{
	// the header: the date on the left page, the money on the right, in pencil; the page number at the foot
	const ULyudInventory* Inv = Inventory.Get();
	TSharedRef<SWidget> Head = bRight
		? StaticCastSharedRef<SWidget>(Words(LyudText::Get(TEXT("ui.inventory.money"), { FStringFormatArg(Inv ? Inv->Rubles : 0) }), 28.f, Pencil))
		: StaticCastSharedRef<SWidget>(SNew(STextBlock).Font(Hand(28.f)).ColorAndOpacity(Pencil)
			.Text(LyudUIText::DateTime(UGameCalendar::NowIn(Owner.IsValid() ? Owner->GetWorld() : nullptr))));
	return SNew(SBox).WidthOverride(PageW).HeightOverride(PageH)
	[
		SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SImage).Image(PaperBrush(bRight))]
		+ SOverlay::Slot().VAlign(VAlign_Top).HAlign(bRight ? HAlign_Right : HAlign_Left).Padding(FMargin(Margin + (bRight ? 0 : 24), 30, Margin + (bRight ? 24 : 0), 0))
		[
			Head
		]
		+ SOverlay::Slot().Padding(FMargin(bRight ? Margin + 20 : Margin + 8, 96, bRight ? Margin + 8 : Margin + 20, 80))
		[
			Content
		]
		+ SOverlay::Slot().VAlign(VAlign_Bottom).HAlign(bRight ? HAlign_Right : HAlign_Left).Padding(FMargin(Margin, 0, Margin, 30))
		[
			Words(FText::AsNumber(PageNumber + (bRight ? 1 : 0)), 26.f, PencilFaint)
		]
	];
}

// ------------------------------------------------------------------ things: what is worn, the chosen thing / what is in the pockets

TSharedRef<SWidget> SLyudDiarySpread::ThingsLeft()
{
	ULyudInventory* Inv = Inventory.Get();
	ULyudInventoryUI* UI = InventoryUI.Get();
	if (!Inv)
	{
		return SNew(SSpacer);
	}
	const TArray<TPair<FName, int32>> Held = Inv->Held();
	FName& Selected = State->SelectedItem;
	if (Selected.IsNone() || !Inv->Count(Selected))
	{
		Selected = Held.Num() ? Held[0].Key : FName();
	}

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight()[Title(LyudText::Get(TEXT("ui.diary.things")))];
	Box->AddSlot().AutoHeight()[Subtitle(LyudText::Get(TEXT("ui.diary.worn")))];

	// what is worn: two columns of slots
	TSharedRef<SWrapBox> Worn = SNew(SWrapBox).PreferredSize(PageW - 2 * Margin - 28).InnerSlotPadding(FVector2D(10.f, 6.f));
	for (const ELyudEquipSlot Slot : ULyudInventory::Slots())
	{
		const FName Id = Inv->InSlot(Slot);
		Worn->AddSlot()
		[
			SNew(SBox).WidthOverride(334.f)
			[
				SNew(SLyudInkButton).HoverMark(EMark::Box).SelectedMark(EMark::Highlight).Selected(!Id.IsNone() && Id == Selected).Seed(SeedOf(Id, 1))
				.MarkColor(PencilFaint).Inset(FMargin(2.f))
				.OnClicked_Lambda([this, Id]() { if (!Id.IsNone()) { State->SelectedItem = Id; Changed(); } })
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Sketch(UI, Id, 64.f)]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10, 0, 0, 0)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[Words(ULyudInventory::SlotName(Slot), 22.f, PencilFaint)]
						+ SVerticalBox::Slot().AutoHeight()
						[
							Id.IsNone() ? StaticCastSharedRef<SWidget>(Key(TEXT("ui.inventory.slot_empty"), 28.f, PencilFaint))
								: StaticCastSharedRef<SWidget>(Words(ULyudInventory::ItemName(Id), 28.f, Ink, false, true))
						]
					]
				]
			]
		];
	}
	Box->AddSlot().AutoHeight()[Worn];

	// the chosen thing, drawn big, what it is and what can be done with it
	const FLyudItemDef* Def = ULyudInventory::FindDef(Selected);
	if (!Def)
	{
		Box->AddSlot().AutoHeight().Padding(0, 30, 0, 0)[Key(TEXT("ui.inventory.empty"), 30.f, PencilFaint, false, true)];
		return Box;
	}
	const FName Id = Def->Id;
	TWeakObjectPtr<ULyudInventory> WeakInv = Inventory;
	TWeakObjectPtr<ULyudInventoryUI> WeakUI = InventoryUI;
	TSharedRef<SWrapBox> Actions = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(18.f, 4.f));
	auto AddAction = [&Actions, Id](const TCHAR* K, TFunction<void()> Do) { Actions->AddSlot()[Link(LyudText::Get(K), Do, SeedOf(Id, 7))]; };
	if (Def->Kind == ELyudItemKind::Wear)
	{
		const bool bOn = Inv->IsWorn(Id);
		const ELyudEquipSlot Slot = Def->Slot;
		AddAction(bOn ? TEXT("ui.use.take_off") : TEXT("ui.use.put_on"), [WeakInv, Id, bOn, Slot]
		{
			if (ULyudInventory* I = WeakInv.Get())
			{
				bOn ? I->TakeOff(Slot) : (void)I->Wear(Id);
			}
		});
	}
	else if (Def->Kind == ELyudItemKind::Feed)
	{
		const bool bInHand = Inv->EquippedItem() == Id;
		AddAction(bInHand ? TEXT("ui.use.unequip") : TEXT("ui.use.equip"), [WeakInv, WeakUI, Id, bInHand]
		{
			if (ULyudInventory* I = WeakInv.Get())
			{
				I->Equip(bInHand ? FName() : Id);
			}
			if (ULyudInventoryUI* U = WeakUI.Get(); U && !bInHand)
			{
				U->Close();
			}
		});
	}
	else if (Def->Kind == ELyudItemKind::Drink || Def->Kind == ELyudItemKind::Food)
	{
		AddAction(Def->Kind == ELyudItemKind::Drink ? TEXT("ui.use.drink") : TEXT("ui.use.eat"), [WeakUI, Id]
		{
			if (ULyudInventoryUI* U = WeakUI.Get())
			{
				U->Use(Id);
			}
		});
	}
	else if (Id == ULyudBirdGuide::ItemId)
	{
		// the guide is a book of its own: the diary goes back into the bag and the guide comes out
		AddAction(TEXT("ui.use.open_guide"), [WeakUI]
		{
			UWorld* World = WeakUI.IsValid() ? WeakUI->GetWorld() : nullptr;
			TWeakObjectPtr<UWorld> WeakWorld = World;
			if (ULyudGameScreen* Screen = ULyudGameScreen::Get(World))
			{
				Screen->CloseThen([WeakWorld] { if (ULyudBirdGuideScreen* Guide = ULyudBirdGuideScreen::Get(WeakWorld.Get())) { Guide->Open(); } });
			}
		});
	}
	Box->AddSlot().AutoHeight().Padding(0, 26, 0, 0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[Sketch(UI, Id, 190.f)]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(18, 6, 0, 0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Marked(Words(ULyudInventory::ItemName(Id), 40.f, Ink, true, true), EMark::Underline, InkSoft, SeedOf(Id, 3))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
			[
				Words(Def->Slot != ELyudEquipSlot::None ? ULyudInventory::SlotName(Def->Slot) : FText::GetEmpty(), 24.f, PencilFaint)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 12)[Words(ULyudInventory::ItemDescription(Id), 28.f, Pencil, false, true)]
			+ SVerticalBox::Slot().AutoHeight()[Actions]
		]
	];
	return Box;
}

TSharedRef<SWidget> SLyudDiarySpread::ThingsRight()
{
	ULyudInventory* Inv = Inventory.Get();
	ULyudInventoryUI* UI = InventoryUI.Get();
	if (!Inv)
	{
		return SNew(SSpacer);
	}
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight().Padding(0, 22, 0, 0)[Subtitle(LyudText::Get(TEXT("ui.diary.pockets")))];
	const TArray<TPair<FName, int32>> Held = Inv->Held();
	if (!Held.Num())
	{
		Box->AddSlot().AutoHeight().Padding(0, 20, 0, 0)[Key(TEXT("ui.inventory.empty"), 30.f, PencilFaint)];
		return Box;
	}
	TSharedRef<SWrapBox> Grid = SNew(SWrapBox).PreferredSize(PageW - 2 * Margin - 28).InnerSlotPadding(FVector2D(8.f, 10.f));
	for (const TPair<FName, int32>& P : Held)
	{
		const FName Id = P.Key;
		const bool bOn = Inv->IsWorn(Id) || Inv->EquippedItem() == Id;
		Grid->AddSlot()
		[
			SNew(SBox).WidthOverride(170.f).HeightOverride(224.f)
			[
				SNew(SLyudInkButton).HoverMark(EMark::Box).SelectedMark(EMark::Highlight).Selected(Id == State->SelectedItem).Seed(SeedOf(Id, 2))
				.MarkColor(PencilFaint).SelectedColor(Highlight).Inset(FMargin(10.f, 6.f))
				.OnClicked_Lambda([this, Id]() { State->SelectedItem = Id; Changed(); })
				[
					// worn or in the hand, and how many, on a line of their own above the sketch
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(8, 2, 8, 0)
					[
						SNew(SBox).HeightOverride(34.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
							[
								Words(bOn ? LyudText::Get(Inv->IsWorn(Id) ? TEXT("ui.inventory.badge_on") : TEXT("ui.inventory.badge_hand")) : FText::GetEmpty(), 22.f, Green, true)
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								Words(P.Value > 1 ? FText::FromString(FString::Printf(TEXT("x%d"), P.Value)) : FText::GetEmpty(), 26.f, Red, true)
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Sketch(UI, Id, 108.f)]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(4, 2, 4, 0)
					[
						SNew(STextBlock).Text(ULyudInventory::ItemName(Id)).Font(Hand(24.f)).ColorAndOpacity(Ink).AutoWrapText(true).Justification(ETextJustify::Center)
					]
				]
			]
		];
	}
	Box->AddSlot().FillHeight(1.f)[Scroll(Grid)];
	return Box;
}

// ------------------------------------------------------------------ quests: the list / the chosen one

TSharedRef<SWidget> SLyudDiarySpread::QuestsLeft()
{
	ULyudQuests* Quests = ULyudQuests::Get(Owner.Get());
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight()[Title(LyudText::Get(TEXT("ui.diary.quests")))];
	if (!Quests)
	{
		return Box;
	}
	TArray<FName> Active, Done;
	for (const bool bMain : { true, false })
	{
		for (const FLyudQuestProgress& P : Quests->Progress)
		{
			const FLyudQuestDef* D = ULyudQuests::FindDef(P.Quest);
			if (D && D->bMain == bMain)
			{
				(P.State == ELyudQuestState::Done ? Done : Active).AddUnique(P.Quest);
			}
		}
	}
	Active.RemoveAll([Quests](FName Q) { return Quests->State(Q) != ELyudQuestState::Active; });
	FName& Selected = State->SelectedQuest;
	if (Selected.IsNone() || Quests->State(Selected) == ELyudQuestState::Inactive)
	{
		Selected = Active.Num() ? Active[0] : Done.Num() ? Done[0] : FName();
	}
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	auto AddQuests = [&](const TArray<FName>& Qs, const TCHAR* Heading, bool bDone)
	{
		if (!Qs.Num())
		{
			return;
		}
		List->AddSlot().AutoHeight()[Subtitle(LyudText::Get(Heading))];
		for (const FName Q : Qs)
		{
			const FLyudQuestDef* D = ULyudQuests::FindDef(Q);
			const bool bMain = D && D->bMain;
			const bool bShown = Quests->ShownQuest() == Q;
			const FText Name = ULyudQuests::QuestTitle(Q);
			// a box to tick before each, the followed one with an arrow in the margin, the done ones ticked and struck through
			TSharedRef<SWidget> Box2 = SNew(SBox).WidthOverride(26.f).HeightOverride(26.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[SNew(SLyudInkMark).Kind(EMark::Box).Color(Pencil).Seed(SeedOf(Q, 4)).Thickness(1.8f)]
					+ SOverlay::Slot()[SNew(SLyudInkMark).Kind(EMark::Tick).Color(Ink).Seed(SeedOf(Q, 5)).Thickness(3.f).Show(bDone)]
				];
			TSharedRef<SWidget> Label = Words(Name, bMain ? 34.f : 31.f, bDone ? PencilFaint : Ink, bMain, true);
			List->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
				[
					SNew(SBox).WidthOverride(40.f).HeightOverride(22.f)
					[
						SNew(SLyudInkMark).Kind(EMark::Arrow).Color(Red).Seed(SeedOf(Q, 6)).Thickness(2.4f).Show(bShown)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)[Box2]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SLyudInkButton).HoverMark(EMark::Underline).SelectedMark(EMark::Highlight).Selected(Q == Selected).Seed(SeedOf(Q, 8))
					.OnClicked_Lambda([this, Q]() { State->SelectedQuest = Q; Changed(); })
					[
						bDone ? Marked(Label, EMark::Strike, InkSoft, SeedOf(Q, 9), 1.8f) : Label
					]
				]
			];
		}
	};
	AddQuests(Active, TEXT("ui.diary.active"), false);
	AddQuests(Done, TEXT("ui.diary.done"), true);
	if (!Active.Num() && !Done.Num())
	{
		List->AddSlot().AutoHeight()[Key(TEXT("ui.journal.none"), 30.f, PencilFaint)];
	}
	Box->AddSlot().FillHeight(1.f)[Scroll(List)];
	return Box;
}

TSharedRef<SWidget> SLyudDiarySpread::QuestsRight()
{
	ULyudQuests* Quests = ULyudQuests::Get(Owner.Get());
	const FName Quest = State->SelectedQuest;
	const FLyudQuestDef* Def = ULyudQuests::FindDef(Quest);
	const FLyudQuestProgress* P = Quests ? Quests->FindProgress(Quest) : nullptr;
	if (!Def || !P)
	{
		return SNew(SSpacer);
	}
	const bool bActive = P->State == ELyudQuestState::Active;
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight().Padding(0, 16, 0, 0)[Key(Def->bMain ? TEXT("ui.diary.main") : TEXT("ui.diary.side"), 26.f, PencilFaint)];
	Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
	[
		SNew(SBox).HAlign(HAlign_Left)[Marked(Words(ULyudQuests::QuestTitle(Quest), 44.f, Ink, true, true), EMark::Underline, InkSoft, SeedOf(Quest, 10), 2.4f)]
	];
	Box->AddSlot().AutoHeight().Padding(0, 4, 0, 6)[Words(LyudText::Get(FString::Printf(TEXT("quest.%s.desc"), *Quest.ToString())), 28.f, Pencil, false, true)];

	FName MarkerPlace;
	if (bActive)
	{
		Box->AddSlot().AutoHeight()[Subtitle(LyudText::Get(TEXT("ui.diary.now")))];
		int32 N = 0;
		for (const TPair<const FLyudQuestObjective*, int32>& O : Quests->CurrentObjectives(Quest))
		{
			const bool bMet = O.Value >= O.Key->Need;
			if (!bMet && MarkerPlace.IsNone())
			{
				MarkerPlace = O.Key->Place;
			}
			const uint32 S = SeedOf(Quest, 20 + N++);
			Box->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(4, 8, 14, 0)
				[
					SNew(SBox).WidthOverride(24.f).HeightOverride(24.f)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[SNew(SLyudInkMark).Kind(EMark::Box).Color(Pencil).Seed(S).Thickness(1.8f)]
						+ SOverlay::Slot()[SNew(SLyudInkMark).Kind(EMark::Tick).Color(Green).Seed(S + 1).Thickness(3.f).Show(bMet)]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)[Words(ULyudQuests::ObjectiveText(Quest, *O.Key, O.Value), 30.f, bMet ? PencilFaint : Ink, false, true)]
			];
		}
		TSharedRef<SWrapBox> Actions = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(22.f, 4.f));
		TWeakObjectPtr<ULyudQuests> WeakQ = Quests;
		if (Quests->ShownQuest() != Quest)
		{
			Actions->AddSlot()[Link(LyudText::Get(TEXT("ui.journal.track")), [WeakQ, Quest] { if (ULyudQuests* Q = WeakQ.Get()) { Q->Tracked = Quest; Q->OnChanged.Broadcast(); } }, SeedOf(Quest, 30))];
		}
		if (!MarkerPlace.IsNone())
		{
			TWeakObjectPtr<ULyudGameScreen> WeakOwner = Owner;
			Actions->AddSlot()[Link(LyudText::Get(TEXT("ui.journal.on_map")), [WeakOwner, MarkerPlace] { if (ULyudGameScreen* O = WeakOwner.Get()) { O->ShowOnMap(MarkerPlace); } }, SeedOf(Quest, 31))];
		}
		Box->AddSlot().AutoHeight().Padding(0, 8, 0, 0)[Actions];
	}

	TSharedRef<SVerticalBox> Story = SNew(SVerticalBox);
	for (const FLyudJournalEntry& E : Quests->Entries)
	{
		if (E.Quest != Quest)
		{
			continue;
		}
		Story->AddSlot().AutoHeight().Padding(0, 0, 0, 10)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Words(LyudUIText::Date(E.When), 22.f, PencilFaint)]
			+ SVerticalBox::Slot().AutoHeight()[Words(E.Text(), 28.f, Ink, false, true)]
		];
	}
	Box->AddSlot().AutoHeight()[Subtitle(LyudText::Get(TEXT("ui.diary.story")))];
	Box->AddSlot().FillHeight(1.f)[Scroll(Story)];
	return Box;
}

// ------------------------------------------------------------------ the diary: entries by day, oldest first

TSharedRef<SWidget> SLyudDiarySpread::DiaryPage(int32 PageIndex)
{
	ULyudQuests* Quests = ULyudQuests::Get(Owner.Get());
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	if (PageIndex == 0)
	{
		Box->AddSlot().AutoHeight()[Title(LyudText::Get(TEXT("ui.diary.diary")))];
	}
	if (!Quests || !State->DiaryPages.IsValidIndex(PageIndex))
	{
		if (PageIndex == 0)
		{
			Box->AddSlot().AutoHeight()[Key(TEXT("ui.journal.diary_empty"), 30.f, PencilFaint)];
		}
		return Box;
	}
	FDateTime Day;
	const TArray<int32>& On = State->DiaryPages[PageIndex];
	// a page that goes on with the day of the page before does not repeat its date
	if (On.Num() && On[0] > 0 && Quests->Entries.IsValidIndex(On[0] - 1))
	{
		Day = Quests->Entries[On[0] - 1].When.GetDate();
	}
	for (const int32 I : On)
	{
		if (!Quests->Entries.IsValidIndex(I))
		{
			continue;
		}
		const FLyudJournalEntry& E = Quests->Entries[I];
		if (E.When.GetDate() != Day)
		{
			Day = E.When.GetDate();
			const FText D = LyudUIText::Date(E.When);
			Box->AddSlot().AutoHeight().Padding(0, 14, 0, 6).HAlign(HAlign_Left)[Marked(Words(D, 36.f, Ink, true), EMark::Underline, InkSoft, SeedOf(D.ToString()))];
		}
		TSharedRef<SVerticalBox> Line = SNew(SVerticalBox);
		if (!E.Quest.IsNone())
		{
			Line->AddSlot().AutoHeight()[Words(ULyudQuests::QuestTitle(E.Quest), 22.f, PencilFaint)];
		}
		Line->AddSlot().AutoHeight()[Words(E.Text(), 29.f, Ink, false, true)];
		Box->AddSlot().AutoHeight().Padding(0, 0, 0, 10)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 4, 12, 0)[SNew(SBox).WidthOverride(64.f)[Words(FText::FromString(LyudUIText::Clock(E.When)), 24.f, Pencil)]]
			+ SHorizontalBox::Slot().FillWidth(1.f)[Line]
		];
	}
	return Box;
}

// ------------------------------------------------------------------ the map across the spread, with a sticky note

TSharedRef<SWidget> SLyudDiarySpread::MapSpread()
{
	SAssignNew(Map, SLyudMapView).Owner(Owner.Get()).bInk(true);
	if (State->bMapSet)
	{
		Map->CenterOn(State->MapCenter, State->MapZoom);
	}
	TWeakPtr<FLyudDiaryState> WeakState = State;
	Map->OnMoved = [WeakState](const FVector2D& C, float Z)
	{
		if (TSharedPtr<FLyudDiaryState> S = WeakState.Pin())
		{
			S->MapCenter = C;
			S->MapZoom = Z;
			S->bMapSet = true;
		}
	};
	return SNew(SOverlay)
		+ SOverlay::Slot().Padding(FMargin(Margin, 100, Margin, 70))[Map.ToSharedRef()]
		// the title heads the right page: the left one's top line has the date
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(PageW + Margin + 20, 22, 0, 0))[Title(LyudText::Get(TEXT("ui.diary.map")))]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(Margin + 30, 0, 0, 22))[Key(TEXT("ui.diary.hint.map"), 24.f, PencilFaint)]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0, 0, Margin + 10, 90))[MapNote()];
}

TSharedRef<SWidget> SLyudDiarySpread::MapNote()
{
	ULyudQuests* Quests = ULyudQuests::Get(Owner.Get());
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	FVector2D At, Facing;
	if (Owner.IsValid() && Owner->PlayerOnMap(At, Facing))
	{
		FName Near;
		double Best = 1e18;
		for (const FLyudMapPlace& P : ULyudQuests::Places())
		{
			const double D = FVector2D::DistSquared(P.At, At);
			if (Quests && Quests->IsFound(P.Id) && P.Kind != TEXT("water") && P.Kind != TEXT("land") && D < Best)
			{
				Best = D;
				Near = P.Id;
			}
		}
		Box->AddSlot().AutoHeight()[Key(TEXT("ui.diary.here"), 30.f, Ink, true)];
		Box->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
		[
			Words(Near.IsNone() ? FText::GetEmpty() : LyudText::Get(TEXT("ui.map.near"), { FStringFormatArg(ULyudQuests::PlaceName(Near).ToString()),
				FStringFormatArg(FMath::RoundToInt(FMath::Sqrt(Best))) }), 26.f, Ink, false, true)
		];
		Box->AddSlot().AutoHeight().HAlign(HAlign_Fill)[Link(LyudText::Get(TEXT("ui.diary.to_me")), [this] { if (Map) { Map->CenterOnPlayer(); } }, 77, 26.f, true)];
	}
	// with the bird guide: its birds on this map too, switched here
	if (ULyudBirdGuide::IsOwned(Owner.Get()))
	{
		TWeakObjectPtr<ULyudGameScreen> WeakOwner = Owner;
		Box->AddSlot().AutoHeight().Padding(0, 4, 0, 0)
		[
			SNew(SLyudInkButton).HoverMark(EMark::Underline).Seed(91)
			.OnClicked_Lambda([WeakOwner]() { if (ULyudBirdGuide* G = ULyudBirdGuide::Get(WeakOwner.Get())) { G->bMapLayer = !G->bMapLayer; } })
			[
				SNew(SBox).Padding(FMargin(4, 0))
				[
					SNew(STextBlock).Font(Hand(26.f, true)).ColorAndOpacity(Ink).Text_Lambda([WeakOwner]()
					{
						const ULyudBirdGuide* G = ULyudBirdGuide::Get(WeakOwner.Get());
						return LyudText::Get(G && G->bMapLayer ? TEXT("ui.diary.birds_on") : TEXT("ui.diary.birds_off"));
					})
				]
			]
		];
	}
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	int32 NFound = 0, NAll = 0;
	for (const FLyudMapPlace& P : ULyudQuests::Places())
	{
		if (P.Kind == TEXT("water") || P.Kind == TEXT("land"))
		{
			continue;
		}
		++NAll;
		if (!Quests || !Quests->IsFound(P.Id))
		{
			continue;
		}
		++NFound;
		const FName Id = P.Id;
		const FVector2D PlaceAt = P.At;
		List->AddSlot().AutoHeight()
		[
			SNew(SLyudInkButton).Seed(SeedOf(Id, 40)).OnClicked_Lambda([this, PlaceAt]() { if (Map) { Map->CenterOn(PlaceAt, 1.5f); } })
			[
				Words(FText::FromString(TEXT("· ") + ULyudQuests::PlaceName(Id).ToString()), 26.f, Ink, false, true)
			]
		];
	}
	Box->AddSlot().AutoHeight().Padding(0, 8, 0, 2)[Key(TEXT("ui.diary.places"), 30.f, Ink, true)];
	Box->AddSlot().AutoHeight()[SNew(SBox).MaxDesiredHeight(190.f)[Scroll(List)]];
	Box->AddSlot().AutoHeight().Padding(0, 6, 0, 0)[Words(LyudText::Get(TEXT("ui.map.found"), { FStringFormatArg(NFound), FStringFormatArg(NAll) }), 22.f, Pencil)];

	// the note, a little crooked, held by a strip of tape
	return SNew(SBox).WidthOverride(330.f).RenderTransformPivot(FVector2D(0.5f, 0.f)).RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(-2.f))))
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SImage).Image(StickyBrush())]
			+ SOverlay::Slot().Padding(FMargin(22, 30, 18, 18))[Box]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0, -14, 0, 0))
			[
				SNew(SBox).WidthOverride(120.f).HeightOverride(34.f)
				[
					SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(FLinearColor(0.95f, 0.95f, 0.9f, 0.55f))
				]
			]
		];
}

// ------------------------------------------------------------------ a turning page

void SLyudDiaryFlip::Construct(const FArguments& InArgs)
{
	// shown (HitTestInvisible) only while a page turns: idle it has no pictures and would cover the page in white
}

void SLyudDiaryFlip::Start(UTextureRenderTarget2D* From, UTextureRenderTarget2D* To, bool bInForward, int32 InSheets)
{
	FromBrush = FSlateBrush();
	ToBrush = FSlateBrush();
	FromBrush.SetResourceObject(From);
	ToBrush.SetResourceObject(To);
	FromBrush.ImageSize = ToBrush.ImageSize = FVector2D(SpreadW, PageH);
	bForward = bInForward;
	Sheets = FMath::Clamp(InSheets, 1, 3);
	Progress = 0.f;
}

namespace
{
	/** One row of the sheet (v down the page) at turn t: points across it in the page's frame (x from the spine, z toward the reader). */
	void SheetRow(float T, float V, int32 NU, TArray<FVector2f>& XZ, TArray<float>& Angle)
	{
		XZ.SetNum(NU + 1);
		Angle.SetNum(NU + 1);
		const float Theta = PI * FMath::SmoothStep(0.f, 1.f, T);
		// the outer edge leads, the bottom corner most (the thumb lifts it), most in the middle of the turn
		const float Curl = 1.05f * FMath::Sin(PI * T);
		float X = 0.f, Z = 0.f;
		for (int32 I = 0; I <= NU; ++I)
		{
			const float U = float(I) / NU;
			const float Phi = FMath::Clamp(Theta + Curl * U * U * (0.55f + 0.45f * V) - Curl * 0.25f * U, 0.f, PI);
			Angle[I] = Phi;
			if (I > 0)
			{
				const float Du = PageW / NU;
				X += Du * FMath::Cos(Phi);
				Z += Du * FMath::Sin(Phi);
			}
			XZ[I] = FVector2f(X, Z);
		}
	}
}

void SLyudDiaryFlip::DrawSheet(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, float T, bool bFrontContent, bool bBackContent) const
{
	constexpr int32 NU = 26, NV = 8;
	const float Eye = PageH * 1.12f;      // the reader's eyes over the spread (the book at ~33 cm), design units
	const FVector2f Mid(SpreadW * 0.5f, PageH * 0.5f);
	const float Dir = bForward ? 1.f : -1.f;
	const FSlateRenderTransform& RT = G.GetAccumulatedRenderTransform();
	// the sheet's front is the page being turned, its back the page that lands on the other side
	const FSlateBrush* FrontB = bFrontContent ? &FromBrush : PaperBrush(bForward);
	const FSlateBrush* BackB = bBackContent ? &ToBrush : PaperBrush(!bForward);
	auto UVFront = [&](float U, float V)
	{
		if (!bFrontContent)
		{
			return FVector2f(bForward ? U : 1.f - U, V);
		}
		return FVector2f(0.5f + Dir * 0.5f * U, V);
	};
	auto UVBack = [&](float U, float V)
	{
		if (!bBackContent)
		{
			return FVector2f(bForward ? 1.f - U : U, V);
		}
		return FVector2f(0.5f - Dir * 0.5f * U, V);
	};
	TArray<FVector2f> P;
	TArray<float> Shade;
	P.SetNum((NU + 1) * (NV + 1));
	Shade.SetNum(P.Num());
	TArray<FVector2f> XZ;
	TArray<float> Ang;
	for (int32 J = 0; J <= NV; ++J)
	{
		const float V = float(J) / NV;
		SheetRow(T, V, NU, XZ, Ang);
		for (int32 I = 0; I <= NU; ++I)
		{
			const float X = Mid.X + Dir * XZ[I].X;
			const float Y = V * PageH;
			const float K = Eye / FMath::Max(Eye - XZ[I].Y, 1.f);
			P[J * (NU + 1) + I] = FVector2f(Mid.X + (X - Mid.X) * K, Mid.Y + (Y - Mid.Y) * K);
			// lit from in front: flat it is as bright as the page, on edge darker
			Shade[J * (NU + 1) + I] = 0.7f + 0.3f * FMath::Abs(FMath::Cos(Ang[I]));
		}
	}
	TArray<FSlateVertex> FrontV, BackV;
	TArray<SlateIndex> FrontI, BackI;
	auto Vert = [&](TArray<FSlateVertex>& Vs, int32 Idx, const FVector2f& UV)
	{
		const uint8 S = uint8(FMath::Clamp(Shade[Idx], 0.f, 1.f) * 255.f);
		Vs.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, P[Idx], UV, FColor(S, S, S, 255)));
		return SlateIndex(Vs.Num() - 1);
	};
	for (int32 J = 0; J < NV; ++J)
	{
		for (int32 I = 0; I < NU; ++I)
		{
			const int32 A = J * (NU + 1) + I, B = A + 1, C = A + NU + 1, D = C + 1;
			const float U0 = float(I) / NU, U1 = float(I + 1) / NU, V0 = float(J) / NV, V1 = float(J + 1) / NV;
			// which side faces the reader: the winding on the screen against the flat page's
			const FVector2f E1 = P[B] - P[A], E2 = P[C] - P[A];
			const bool bFront = (E1.X * E2.Y - E1.Y * E2.X) * Dir > 0.f;
			TArray<FSlateVertex>& Vs = bFront ? FrontV : BackV;
			TArray<SlateIndex>& Is = bFront ? FrontI : BackI;
			auto UV = [&](float U, float V) { return bFront ? UVFront(U, V) : UVBack(U, V); };
			const SlateIndex Ia = Vert(Vs, A, UV(U0, V0)), Ib = Vert(Vs, B, UV(U1, V0)), Ic = Vert(Vs, C, UV(U0, V1)), Id = Vert(Vs, D, UV(U1, V1));
			Is.Append({ Ia, Ib, Ic, Ib, Id, Ic });
		}
	}
	FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer();
	if (FrontI.Num())
	{
		FSlateDrawElement::MakeCustomVerts(Out, Layer, Renderer->GetResourceHandle(*FrontB), FrontV, FrontI, nullptr, 0, 0);
	}
	if (BackI.Num())
	{
		FSlateDrawElement::MakeCustomVerts(Out, Layer, Renderer->GetResourceHandle(*BackB), BackV, BackI, nullptr, 0, 0);
	}
}

int32 SLyudDiaryFlip::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// under the sheets: the page not turned (from the spread now) and the page being uncovered (from the coming one)
	FSlateBrush StayB = FromBrush, ComeB = ToBrush;
	StayB.SetUVRegion(bForward ? FBox2f(FVector2f(0.f, 0.f), FVector2f(0.5f, 1.f)) : FBox2f(FVector2f(0.5f, 0.f), FVector2f(1.f, 1.f)));
	ComeB.SetUVRegion(bForward ? FBox2f(FVector2f(0.5f, 0.f), FVector2f(1.f, 1.f)) : FBox2f(FVector2f(0.f, 0.f), FVector2f(0.5f, 1.f)));
	const FVector2D Half(PageW, PageH);
	const FVector2D StayAt = bForward ? FVector2D::ZeroVector : FVector2D(PageW, 0.f), ComeAt = bForward ? FVector2D(PageW, 0.f) : FVector2D::ZeroVector;
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
	FSlateDrawElement::MakeBox(Out, LayerId, G.ToPaintGeometry(FVector2f(Half), FSlateLayoutTransform(FVector2f(StayAt))), &StayB, ESlateDrawEffect::None, Tint);
	FSlateDrawElement::MakeBox(Out, LayerId, G.ToPaintGeometry(FVector2f(Half), FSlateLayoutTransform(FVector2f(ComeAt))), &ComeB, ESlateDrawEffect::None, Tint);

	// the sheets, staggered; one alone carries both pictures, in a riffle the first carries the old page and the last the new
	const float Stagger = Sheets > 1 ? 0.16f : 0.f;
	TArray<TPair<float, int32>> Order;
	for (int32 K = 0; K < Sheets; ++K)
	{
		const float T = FMath::Clamp((Progress - K * Stagger) / (1.f - (Sheets - 1) * Stagger), 0.f, 1.f);
		Order.Add({ T, K });
	}
	// over the spine (past half way) the earliest lies lowest; before it, the latest
	Order.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B)
	{
		const bool AOver = A.Key > 0.5f, BOver = B.Key > 0.5f;
		if (AOver != BOver)
		{
			return AOver;
		}
		return AOver ? A.Key > B.Key : A.Key < B.Key;
	});
	int32 Layer = LayerId + 1;
	for (const TPair<float, int32>& O : Order)
	{
		const float T = O.Key;
		if (T <= 0.f || T >= 1.f)
		{
			continue;
		}
		// the shadow the sheet throws on the page it is about to cover
		const float Lift = FMath::Sin(PI * T);
		const float Land = bForward ? PageW * (1.f - FMath::Clamp((T - 0.5f) * 2.f, 0.f, 1.f)) : PageW + PageW * FMath::Clamp((T - 0.5f) * 2.f, 0.f, 1.f);
		TArray<FSlateGradientStop> Stops;
		if (bForward)
		{
			Stops = { FSlateGradientStop(FVector2D(FMath::Max(Land - 160.f, 0.f), 0.f), FLinearColor(0, 0, 0, 0)), FSlateGradientStop(FVector2D(PageW, 0.f), FLinearColor(0, 0, 0, 0.28f * Lift)) };
			FSlateDrawElement::MakeGradient(Out, Layer, G.ToPaintGeometry(FVector2f(Half), FSlateLayoutTransform()), Stops, Orient_Vertical);
		}
		else
		{
			Stops = { FSlateGradientStop(FVector2D(0.f, 0.f), FLinearColor(0, 0, 0, 0.28f * Lift)), FSlateGradientStop(FVector2D(FMath::Min(Land - PageW + 160.f, PageW), 0.f), FLinearColor(0, 0, 0, 0)) };
			FSlateDrawElement::MakeGradient(Out, Layer, G.ToPaintGeometry(FVector2f(Half), FSlateLayoutTransform(FVector2f(PageW, 0.f))), Stops, Orient_Vertical);
		}
		DrawSheet(Out, Layer + 1, G, T, O.Value == 0, O.Value == Sheets - 1);
		Layer += 2;
	}
	return Layer;
}

// ------------------------------------------------------------------ the screen

FIntPoint SLyudDiaryScreen::PictureSize()
{
	const float S = FMath::Clamp(CVarPictureScale.GetValueOnGameThread(), 0.5f, 3.f);
	return FIntPoint(FMath::RoundToInt(SpreadW * S), FMath::RoundToInt(PageH * S));
}

void SLyudDiaryScreen::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	State = MakeShared<FLyudDiaryState>();
	Language = LyudText::Language();
	SetVisibility(EVisibility::SelfHitTestInvisible);

	ChildSlot
	[
		SNew(SCanvas)
		+ SCanvas::Slot()
		.Position_Lambda([this]() { return LocalPos(); })
		.Size_Lambda([this]() { return FVector2D(SpreadW + 2.f * TabRoom, PageH) * LocalScale(); })
		[
			SNew(SDPIScaler).DPIScale_Lambda([this]() { return LocalScale(); })
			[
				SNew(SBox).WidthOverride(SpreadW + 2.f * TabRoom).HeightOverride(PageH)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[SAssignNew(TabsBox, SBox)]
					+ SOverlay::Slot().Padding(FMargin(TabRoom, 0.f))[SAssignNew(LiveBox, SBox)]
					+ SOverlay::Slot().Padding(FMargin(TabRoom, 0.f))[SAssignNew(Flip, SLyudDiaryFlip).Visibility(EVisibility::Collapsed)]
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(TabRoom, 0.f))[MakeCorner(false)]
					+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(TabRoom, 0.f))[MakeCorner(true)]
				]
			]
		]
	];
	Rebuild();
}

SLyudDiaryScreen::~SLyudDiaryScreen() = default;

float SLyudDiaryScreen::LocalScale() const
{
	// the paper's height on the screen in Slate units over its design height
	const float H = RectSize.Y > 1.f ? RectSize.Y / ViewScale : ViewSize.Y * 0.84f;
	return FMath::Max(H / PageH, 0.05f);
}

FVector2D SLyudDiaryScreen::LocalPos() const
{
	const float S = LocalScale();
	if (RectSize.Y > 1.f)
	{
		return RectPos / ViewScale - FVector2D(TabRoom * S, 0.f);
	}
	const FVector2D Size = FVector2D(SpreadW + 2.f * TabRoom, PageH) * S;
	return (ViewSize - Size) * 0.5f;
}

void SLyudDiaryScreen::SetAlpha(float A)
{
	SetRenderOpacity(FMath::Clamp(A, 0.f, 1.f));
	SetVisibility(A > 0.02f ? EVisibility::SelfHitTestInvisible : EVisibility::HitTestInvisible);
}

void SLyudDiaryScreen::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	ViewSize = AllottedGeometry.GetLocalSize();
	ViewScale = FMath::Max(AllottedGeometry.Scale, 0.01f);
	if (Language != LyudText::Language())
	{
		Rebuild();
	}
	if (bTurning)
	{
		TurnTime += InDeltaTime;
		const float T = FMath::Clamp(TurnTime / TurnLength, 0.f, 1.f);
		Flip->SetProgress(T);
		if (T >= 1.f)
		{
			bTurning = false;
			Flip->SetVisibility(EVisibility::Collapsed);
			SetSpread(TurnTarget);
		}
	}
}

void SLyudDiaryScreen::Paginate()
{
	// the diary's entries laid out the way a page takes them: the date line when the day changes, the quest's name, the
	// lines of the entry wrapped at the page's width (measured in the hand's font)
	State->DiaryPages.Reset();
	const ULyudQuests* Quests = ULyudQuests::Get(Owner.Get());
	if (!Quests || !Quests->Entries.Num())
	{
		return;
	}
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FSlateFontInfo Body = Hand(29.f), Small = Hand(22.f), DateF = Hand(36.f, true);
	const float TextW = PageW - 2.f * Margin - 28.f - 76.f;
	const float LineH = Measure->GetMaxCharacterHeight(Body) * 1.02f;
	const float Room0 = PageH - 96.f - 80.f - 110.f, Room = PageH - 96.f - 80.f;
	float Left = Room0;
	FDateTime Day;
	State->DiaryPages.AddDefaulted();
	for (int32 I = 0; I < Quests->Entries.Num(); ++I)
	{
		const FLyudJournalEntry& E = Quests->Entries[I];
		float H = 10.f;
		if (E.When.GetDate() != Day)
		{
			H += Measure->GetMaxCharacterHeight(DateF) + 20.f;
		}
		if (!E.Quest.IsNone())
		{
			H += Measure->GetMaxCharacterHeight(Small);
		}
		// words wrapped greedily
		const FString Text = E.Text().ToString();
		TArray<FString> WordsList;
		Text.ParseIntoArray(WordsList, TEXT(" "));
		float X = 0.f;
		int32 Lines = 1;
		const float Space = Measure->Measure(TEXT(" "), Body).X;
		for (const FString& W : WordsList)
		{
			const float Wd = Measure->Measure(W, Body).X;
			if (X > 0.f && X + Space + Wd > TextW)
			{
				++Lines;
				X = Wd;
			}
			else
			{
				X += (X > 0.f ? Space : 0.f) + Wd;
			}
		}
		// Japanese has no spaces: wrap by its width
		if (WordsList.Num() <= 1)
		{
			Lines = FMath::Max(1, FMath::CeilToInt(Measure->Measure(Text, Body).X / TextW));
		}
		H += Lines * LineH;
		if (H > Left && State->DiaryPages.Last().Num())
		{
			State->DiaryPages.AddDefaulted();
			Left = Room;
			// a new page names its day again only when the day changes on it (SLyudDiarySpread::DiaryPage)
		}
		Day = E.When.GetDate();
		State->DiaryPages.Last().Add(I);
		Left -= H;
	}
}

void SLyudDiaryScreen::Rebuild()
{
	Language = LyudText::Language();
	Paginate();
	const int32 DiarySpreads = FMath::Max(1, (State->DiaryPages.Num() + 1) / 2);
	const FLyudDiarySpread Was = Spreads.IsValidIndex(Current) ? Spreads[Current] : FLyudDiarySpread();
	Spreads.Reset();
	Spreads.Add({ ESection::Things, 0 });
	Spreads.Add({ ESection::Quests, 0 });
	for (int32 P = 0; P < DiarySpreads; ++P)
	{
		Spreads.Add({ ESection::Diary, P });
	}
	Spreads.Add({ ESection::Map, 0 });
	// stay on the same spread (or the same section's last one, if it got shorter)
	Current = 0;
	for (int32 I = 0; I < Spreads.Num(); ++I)
	{
		if (Spreads[I].Section == Was.Section && Spreads[I].Part <= Was.Part)
		{
			Current = I;
		}
	}
	Refresh();
}

void SLyudDiaryScreen::Refresh()
{
	Live = MakeSpread(Current);
	LiveBox->SetContent(Live.ToSharedRef());
	TabsBox->SetContent(MakeTabs());
}

TSharedRef<SLyudDiarySpread> SLyudDiaryScreen::MakeSpread(int32 I)
{
	TWeakPtr<SLyudDiaryScreen> Weak = StaticCastSharedRef<SLyudDiaryScreen>(AsShared());
	return SNew(SLyudDiarySpread).Owner(Owner.Get()).Spread(Spreads[FMath::Clamp(I, 0, Spreads.Num() - 1)]).State(State).PageNumber(2 * I + 2)
		.OnChanged_Lambda([Weak]()
		{
			// rebuilt next frame: the click that asked for it is still running in the old page
			if (TSharedPtr<SLyudDiaryScreen> S = Weak.Pin())
			{
				S->Language.Reset();
			}
		});
}

int32 SLyudDiaryScreen::SpreadFor(ELyudScreenTab Tab) const
{
	const ESection S = Tab == ELyudScreenTab::Inventory ? ESection::Things : Tab == ELyudScreenTab::Journal ? ESection::Quests
		: Tab == ELyudScreenTab::Diary ? ESection::Diary : ESection::Map;
	int32 Found = 0;
	for (int32 I = 0; I < Spreads.Num(); ++I)
	{
		if (Spreads[I].Section == S)
		{
			Found = I;
			if (S != ESection::Diary)
			{
				break;
			}
		}
	}
	return Found;
}

void SLyudDiaryScreen::SetSpread(int32 I)
{
	Current = FMath::Clamp(I, 0, Spreads.Num() - 1);
	Refresh();
	if (ULyudGameScreen* O = Owner.Get())
	{
		O->OnSpreadShown();
	}
}

int32 SLyudDiaryScreen::TabsOnLeft() const
{
	if (!Spreads.IsValidIndex(Current))
	{
		return 0;
	}
	const FLyudDiarySpread& S = Spreads[Current];
	return static_cast<int32>(S.Section) + (S.Part > 0 ? 1 : 0);
}

bool SLyudDiaryScreen::TurnTo(int32 I)
{
	I = FMath::Clamp(I, 0, Spreads.Num() - 1);
	if (I == Current || bTurning)
	{
		return false;
	}
	ULyudGameScreen* O = Owner.Get();
	UTextureRenderTarget2D* From = O ? O->PictureTarget(0) : nullptr;
	UTextureRenderTarget2D* To = O ? O->PictureTarget(1) : nullptr;
	const int32 Sheets = FMath::Abs(I - Current);
	if (!From || !To)
	{
		SetSpread(I);
		return true;
	}
	DrawSpread(Current, From);
	DrawSpread(I, To);
	Flip->Start(From, To, I > Current, Sheets);
	Flip->SetVisibility(EVisibility::HitTestInvisible);
	LiveBox->SetContent(SNew(SSpacer));
	TurnTarget = I;
	TurnTime = 0.f;
	TurnLength = Sheets > 1 ? 0.62f : 0.48f;
	bTurning = true;
	O->PlayDiarySound(Sheets > 1 ? TEXT("diary_pages") : TEXT("diary_page"));
	return true;
}

void SLyudDiaryScreen::ShowPlace(FName Place)
{
	if (const FLyudMapPlace* P = ULyudQuests::FindPlace(Place))
	{
		State->MapCenter = P->At;
		State->MapZoom = 1.5f;
		State->bMapSet = true;
		if (Live && Live->GetMap())
		{
			Live->GetMap()->CenterOn(P->At, 1.5f);
		}
	}
}

void SLyudDiaryScreen::CenterMapOnPlayer()
{
	if (Live && Live->GetMap())
	{
		Live->GetMap()->CenterOnPlayer();
	}
}

void SLyudDiaryScreen::DrawSpread(int32 I, UTextureRenderTarget2D* Target)
{
	if (!Target || !Spreads.IsValidIndex(I))
	{
		return;
	}
	const bool bGamma = CVarPictureGamma.GetValueOnGameThread() != 0;
	if (!Renderer)
	{
		Renderer = MakeUnique<FWidgetRenderer>(bGamma, true);
	}
	const FVector2D Size(Target->SizeX, Target->SizeY);
	TSharedRef<SWidget> W = SNew(SDPIScaler).DPIScale(Size.Y / PageH)
		[
			SNew(SBox).WidthOverride(SpreadW).HeightOverride(PageH)[MakeSpread(I)]
		];
	// twice: the first pass lays the text out (wrapped lines know their width only after a layout)
	Renderer->DrawWidget(Target, W, Size, 0.f, false);
	Renderer->DrawWidget(Target, W, Size, 0.f, false);
}

TSharedRef<SWidget> SLyudDiaryScreen::MakeTabs()
{
	// a card tab for each section on the fore-edges: right of the spread for what is ahead, left for what is behind
	TSharedRef<SCanvas> C = SNew(SCanvas);
	const int32 OnLeft = TabsOnLeft();
	for (int32 K = 0; K < static_cast<int32>(ESection::Num); ++K)
	{
		const ESection S = static_cast<ESection>(K);
		const bool bLeft = K < OnLeft;
		const float H = 184.f, W = TabRoom - 8.f;
		const float Y = 146.f + 224.f * K - H * 0.5f + 92.f;
		const float X = bLeft ? 8.f : TabRoom + SpreadW;
		const bool bHere = Spreads.IsValidIndex(Current) && Spreads[Current].Section == S;
		C->AddSlot().Position(FVector2D(X, Y)).Size(FVector2D(W, H))
		[
			SNew(SLyudInkButton).HoverMark(EMark::Box).Selected(false).Seed(900 + K).MarkColor(PencilFaint)
			.OnClicked_Lambda([this, S]()
			{
				for (int32 I = 0; I < Spreads.Num(); ++I)
				{
					if (Spreads[I].Section == S)
					{
						TurnTo(S == ESection::Diary ? SpreadFor(ELyudScreenTab::Diary) : I);
						break;
					}
				}
			})
			[
				SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(TabColor(S) * (bHere ? 1.f : 0.86f))
			]
		];
		// the name written along the tab: laid out the tab's length wide, then turned onto it
		const FVector2D LabelSize(H - 16.f, W);
		C->AddSlot().Position(FVector2D(X + W * 0.5f, Y + H * 0.5f) - LabelSize * 0.5f).Size(LabelSize)
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).Visibility(EVisibility::HitTestInvisible)
			.RenderTransformPivot(FVector2D(0.5f, 0.5f)).RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(bLeft ? -90.f : 90.f))))
			[
				SNew(STextBlock).Text(LyudText::Get(SectionKey(S))).Font(Hand(26.f, true)).ColorAndOpacity(Ink).Justification(ETextJustify::Center)
			]
		];
	}
	return C;
}

TSharedRef<SWidget> SLyudDiaryScreen::MakeCorner(bool bRight)
{
	// a dog-ear at the bottom corner: turns a page forward (right) or back (left)
	return SNew(SBox).WidthOverride(120.f).HeightOverride(110.f)
		[
			SNew(SLyudInkButton).HoverMark(EMark::Circle).Seed(bRight ? 501 : 502).MarkColor(PencilFaint).Inset(FMargin(30.f, 30.f))
			.OnClicked_Lambda([this, bRight]() { TurnTo(Current + (bRight ? 1 : -1)); })
			[
				SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
				.Visibility_Lambda([this, bRight]() { return (bRight ? Current + 1 < Spreads.Num() : Current > 0) ? EVisibility::Visible : EVisibility::Hidden; })
				[
					SNew(SBox).WidthOverride(48.f).HeightOverride(22.f)
					.RenderTransformPivot(FVector2D(0.5f, 0.5f)).RenderTransform(FSlateRenderTransform(FScale2D(bRight ? 1.f : -1.f, 1.f)))
					[
						SNew(SLyudInkMark).Kind(EMark::Arrow).Color(Pencil).Seed(bRight ? 503 : 504).Thickness(2.4f)
					]
				]
			]
		];
}
