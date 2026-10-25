#include "UI/SLyudGameScreen.h"

#include "LyudInventory.h"
#include "LyudText.h"
#include "Plants/GameCalendar.h"
#include "UI/LyudQuests.h"
#include "UI/LyudUIStyle.h"
#include "UI/LyudUIText.h"
#include "UI/SLyudMapView.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

using namespace LyudUI;

namespace
{
	TSharedRef<SWidget> Words(const FText& T, int32 Size, const FLinearColor& Color, const char* Face = "Regular", bool bWrap = false)
	{
		return SNew(STextBlock).Text(T).Font(Font(Size, Face)).ColorAndOpacity(Color).AutoWrapText(bWrap);
	}

	TSharedRef<SWidget> Key(const TCHAR* K, int32 Size, const FLinearColor& Color, const char* Face = "Regular", bool bWrap = false)
	{
		return Words(LyudText::Get(K), Size, Color, Face, bWrap);
	}

	TSharedRef<SWidget> ActionButton(const TCHAR* K, TFunction<void()> Do, bool bEnabled = true)
	{
		return SNew(SButton).ButtonStyle(&Button()).IsEnabled(bEnabled)
			.OnClicked_Lambda([Do]() { Do(); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(LyudText::Get(K)).Font(Font(16)).ColorAndOpacity(Text)
			];
	}

	TSharedRef<SWidget> SectionTitle(const TCHAR* K)
	{
		return SNew(SBox).Padding(FMargin(0, 10, 0, 6))[Key(K, 13, Dim, "Bold")];
	}
}

void SLyudGameScreenWidget::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	Inventory = ULyudInventory::Get(Owner.Get());
	InventoryUI = ULyudInventoryUI::Get(Owner.Get());
	Language = LyudText::Language();

	ChildSlot
	[
		SNew(SBorder).BorderImage(ShadeBrush()).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(24.f)
		[
			SNew(SBorder).BorderImage(PanelBrush()).Padding(FMargin(22.f, 16.f))
			[
				SNew(SBox).WidthOverride(1180.f).HeightOverride(720.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SAssignNew(HeaderBox, SBox)]
					+ SVerticalBox::Slot().FillHeight(1.f).Padding(0, 12, 0, 0)
					[
						SAssignNew(Pages, SWidgetSwitcher)
						+ SWidgetSwitcher::Slot()[SAssignNew(InventoryBox, SBox)]
						+ SWidgetSwitcher::Slot()[SAssignNew(JournalBox, SBox)]
						+ SWidgetSwitcher::Slot()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f)
							[
								SNew(SBorder).BorderImage(CellBrush()).Padding(1.f)[SAssignNew(Map, SLyudMapView).Owner(Owner.Get())]
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(14, 0, 0, 0)[SAssignNew(MapSideBox, SBox).WidthOverride(250.f)]
						]
					]
				]
			]
		]
	];
	Rebuild();
}

void SLyudGameScreenWidget::SetTab(ELyudScreenTab InTab)
{
	Tab = InTab;
	if (Pages)
	{
		Pages->SetActiveWidgetIndex(static_cast<int32>(Tab));
	}
	Rebuild();
}

void SLyudGameScreenWidget::ShowPlace(FName Place)
{
	if (const FLyudMapPlace* P = ULyudQuests::FindPlace(Place); P && Map)
	{
		Map->CenterOn(P->At, 1.5f);
	}
}

void SLyudGameScreenWidget::CenterMapOnPlayer()
{
	if (Map)
	{
		Map->CenterOnPlayer();
	}
}

void SLyudGameScreenWidget::Rebuild()
{
	Language = LyudText::Language();
	HeaderBox->SetContent(MakeHeader());
	InventoryBox->SetContent(MakeInventory());
	JournalBox->SetContent(MakeJournal());
	MapSideBox->SetContent(MakeMapSide());
}

TSharedRef<SWidget> SLyudGameScreenWidget::MakeHeader()
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	struct FTabInfo { ELyudScreenTab Tab; const TCHAR* Name; const TCHAR* Keys; };
	for (const FTabInfo& T : { FTabInfo{ ELyudScreenTab::Inventory, TEXT("ui.screen.tab.inventory"), TEXT("ui.screen.key.inventory") },
		FTabInfo{ ELyudScreenTab::Journal, TEXT("ui.screen.tab.journal"), TEXT("ui.screen.key.journal") },
		FTabInfo{ ELyudScreenTab::Map, TEXT("ui.screen.tab.map"), TEXT("ui.screen.key.map") } })
	{
		const bool bOn = T.Tab == Tab;
		const ELyudScreenTab Which = T.Tab;
		Row->AddSlot().AutoWidth().Padding(0, 0, 6, 0)
		[
			SNew(SButton).ButtonStyle(&FlatButton())
			.OnClicked_Lambda([this, Which]() { if (Owner.IsValid()) { Owner->Open(Which); } return FReply::Handled(); })
			[
				SNew(SBorder).BorderImage(CellBrush(bOn)).Padding(FMargin(16, 8))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Key(T.Name, 20, bOn ? Bright : Dim, "Bold")]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 2, 0, 0)[Key(T.Keys, 12, Faint)]
				]
			]
		];
	}
	const ULyudInventory* Inv = Inventory.Get();
	Row->AddSlot().FillWidth(1.f)[SNew(SSpacer)];
	Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 22, 0)
	[
		SNew(STextBlock).Font(Font(16)).ColorAndOpacity(Body)
		.Text_Lambda([this]() { return LyudUIText::DateTime(UGameCalendar::NowIn(Owner.IsValid() ? Owner->GetWorld() : nullptr)); })
	];
	Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
	[
		Words(LyudText::Get(TEXT("ui.inventory.money"), { FStringFormatArg(Inv ? Inv->Rubles : 0) }), 18, Text, "Bold")
	];
	const TCHAR* Hint = Tab == ELyudScreenTab::Inventory ? TEXT("ui.screen.hint.inventory") : Tab == ELyudScreenTab::Journal ? TEXT("ui.screen.hint.journal") : TEXT("ui.screen.hint.map");
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Row]
		+ SVerticalBox::Slot().AutoHeight().Padding(2, 6, 0, 0)[Key(Hint, 12, Faint)];
}

TSharedRef<SWidget> SLyudGameScreenWidget::ItemIcon(FName Id, float Size)
{
	ULyudInventoryUI* UI = InventoryUI.Get();
	return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
	[
		Id.IsNone() || !UI ? StaticCastSharedRef<SWidget>(SNew(SSpacer)) : StaticCastSharedRef<SWidget>(SNew(SImage).Image(UI->IconBrush(Id)))
	];
}

// ------------------------------------------------------------------ inventory: what is worn, what is carried, the chosen thing

TSharedRef<SWidget> SLyudGameScreenWidget::MakeInventory()
{
	ULyudInventory* Inv = Inventory.Get();
	if (!Inv)
	{
		return SNew(SSpacer);
	}
	const TArray<TPair<FName, int32>> Held = Inv->Held();
	if (SelectedItem.IsNone() || !Inv->Count(SelectedItem))
	{
		SelectedItem = Held.Num() ? Held[0].Key : FName();
	}

	// the figure: one row per slot
	TSharedRef<SVerticalBox> Worn = SNew(SVerticalBox);
	Worn->AddSlot().AutoHeight()[SectionTitle(TEXT("ui.inventory.worn"))];
	for (const ELyudEquipSlot Slot : ULyudInventory::Slots())
	{
		const FName Id = Inv->InSlot(Slot);
		const bool bSel = !Id.IsNone() && Id == SelectedItem;
		Worn->AddSlot().AutoHeight().Padding(0, 0, 0, 5)
		[
			SNew(SButton).ButtonStyle(&FlatButton()).IsEnabled(!Id.IsNone())
			.OnClicked_Lambda([this, Id]() { SelectedItem = Id; Rebuild(); return FReply::Handled(); })
			[
				SNew(SBorder).BorderImage(CellBrush(bSel)).Padding(6.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBorder).BorderImage(ShadeBrush()).Padding(2.f)[ItemIcon(Id, 46.f)]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10, 0, 0, 0)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[Words(ULyudInventory::SlotName(Slot), 12, Dim)]
						+ SVerticalBox::Slot().AutoHeight()
						[
							Id.IsNone() ? Key(TEXT("ui.inventory.slot_empty"), 15, Faint) : Words(ULyudInventory::ItemName(Id), 15, Bright)
						]
					]
				]
			]
		];
	}

	// the pocket
	TSharedRef<SWrapBox> Grid = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8.f, 8.f));
	for (const TPair<FName, int32>& P : Held)
	{
		const FName Id = P.Key;
		const bool bSel = Id == SelectedItem;
		const bool bOn = Inv->IsWorn(Id) || Inv->EquippedItem() == Id;
		Grid->AddSlot()
		[
			SNew(SButton).ButtonStyle(&FlatButton())
			.OnClicked_Lambda([this, Id]() { SelectedItem = Id; Rebuild(); return FReply::Handled(); })
			[
				SNew(SBorder).BorderImage(CellBrush(bSel)).Padding(6.f)
				[
					SNew(SBox).WidthOverride(96.f).HeightOverride(124.f)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ItemIcon(Id, 76.f)]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 0)
							[
								SNew(STextBlock).Text(ULyudInventory::ItemName(Id)).Font(Font(12)).ColorAndOpacity(Body).AutoWrapText(true).Justification(ETextJustify::Center)
							]
						]
						+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top)
						[
							Words(P.Value > 1 ? FText::FromString(FString::Printf(TEXT("x%d"), P.Value)) : FText::GetEmpty(), 13, Bright, "Bold")
						]
						+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
						[
							Words(bOn ? LyudText::Get(Inv->IsWorn(Id) ? TEXT("ui.inventory.badge_on") : TEXT("ui.inventory.badge_hand")) : FText::GetEmpty(), 11, Text, "Bold")
						]
					]
				]
			]
		];
	}

	// the chosen thing and what can be done with it
	TSharedRef<SVerticalBox> Detail = SNew(SVerticalBox);
	const FLyudItemDef* Def = ULyudInventory::FindDef(SelectedItem);
	if (Def)
	{
		const FName Id = Def->Id;
		TWeakObjectPtr<ULyudInventory> WeakInv = Inventory;
		TWeakObjectPtr<ULyudInventoryUI> WeakUI = InventoryUI;
		TSharedRef<SHorizontalBox> Actions = SNew(SHorizontalBox);
		auto AddAction = [&Actions](TSharedRef<SWidget> W) { Actions->AddSlot().AutoWidth().Padding(0, 0, 8, 0)[W]; };
		if (Def->Kind == ELyudItemKind::Wear)
		{
			const bool bOn = Inv->IsWorn(Id);
			const ELyudEquipSlot Slot = Def->Slot;
			AddAction(ActionButton(bOn ? TEXT("ui.use.take_off") : TEXT("ui.use.put_on"), [WeakInv, Id, bOn, Slot]
			{
				if (ULyudInventory* I = WeakInv.Get())
				{
					bOn ? I->TakeOff(Slot) : (void)I->Wear(Id);
				}
			}));
		}
		else if (Def->Kind == ELyudItemKind::Feed)
		{
			const bool bInHand = Inv->EquippedItem() == Id;
			AddAction(ActionButton(bInHand ? TEXT("ui.use.unequip") : TEXT("ui.use.equip"), [WeakInv, WeakUI, Id, bInHand]
			{
				if (ULyudInventory* I = WeakInv.Get())
				{
					I->Equip(bInHand ? FName() : Id);
				}
				if (ULyudInventoryUI* U = WeakUI.Get(); U && !bInHand)
				{
					U->Close();
				}
			}));
		}
		else if (Def->Kind == ELyudItemKind::Drink || Def->Kind == ELyudItemKind::Food)
		{
			AddAction(ActionButton(Def->Kind == ELyudItemKind::Drink ? TEXT("ui.use.drink") : TEXT("ui.use.eat"), [WeakUI, Id]
			{
				if (ULyudInventoryUI* U = WeakUI.Get())
				{
					U->Use(Id);
				}
			}));
		}
		Detail->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SBorder).BorderImage(ShadeBrush()).Padding(3.f)[ItemIcon(Id, 112.f)]]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(16, 0, 0, 0)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Words(ULyudInventory::ItemName(Id), 22, Bright, "Bold")]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
				[
					Words(Def->Slot != ELyudEquipSlot::None ? ULyudInventory::SlotName(Def->Slot) : FText::GetEmpty(), 12, Dim)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 10)[Words(ULyudInventory::ItemDescription(Id), 15, Body, "Regular", true)]
				+ SVerticalBox::Slot().AutoHeight()[Actions]
			]
		];
	}
	else
	{
		Detail->AddSlot().AutoHeight()[Key(TEXT("ui.inventory.empty"), 16, Faint)];
	}

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(290.f)[Worn]]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(20, 0, 0, 0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[SectionTitle(TEXT("ui.inventory.carried"))]
			+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SScrollBox) + SScrollBox::Slot()[Grid]]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
			[
				SNew(SBorder).BorderImage(CellBrush()).Padding(14.f)[SNew(SBox).MinDesiredHeight(150.f)[Detail]]
			]
		];
}

// ------------------------------------------------------------------ journal: the quests and the diary

TSharedRef<SWidget> SLyudGameScreenWidget::MakeJournal()
{
	ULyudQuests* Quests = ULyudQuests::Get(Owner.Get());
	if (!Quests)
	{
		return SNew(SSpacer);
	}
	// the list: active (main first), then done
	TArray<FName> Active, Done;
	for (const bool bMain : { true, false })
	{
		for (const FLyudQuestProgress& P : Quests->Progress)
		{
			const FLyudQuestDef* D = ULyudQuests::FindDef(P.Quest);
			if (D && D->bMain == bMain)
			{
				(P.State == ELyudQuestState::Active ? Active : P.State == ELyudQuestState::Done ? Done : Active).AddUnique(P.Quest);
			}
		}
	}
	Active.RemoveAll([Quests](FName Q) { return Quests->State(Q) != ELyudQuestState::Active; });
	if (SelectedQuest.IsNone() || Quests->State(SelectedQuest) == ELyudQuestState::Inactive)
	{
		SelectedQuest = Active.Num() ? Active[0] : Done.Num() ? Done[0] : FName();
	}

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	{
		TSharedRef<SHorizontalBox> Switch = SNew(SHorizontalBox);
		for (const bool bD : { false, true })
		{
			Switch->AddSlot().AutoWidth().Padding(0, 0, 6, 0)
			[
				SNew(SButton).ButtonStyle(&FlatButton()).OnClicked_Lambda([this, bD]() { bDiary = bD; Rebuild(); return FReply::Handled(); })
				[
					SNew(SBorder).BorderImage(CellBrush(bDiary == bD)).Padding(FMargin(12, 5))
					[
						Key(bD ? TEXT("ui.journal.diary") : TEXT("ui.journal.quests"), 15, bDiary == bD ? Bright : Dim, "Bold")
					]
				]
			];
		}
		List->AddSlot().AutoHeight().Padding(0, 0, 0, 6)[Switch];
	}
	auto AddQuests = [&](const TArray<FName>& Qs, const TCHAR* Title)
	{
		if (!Qs.Num())
		{
			return;
		}
		List->AddSlot().AutoHeight()[SectionTitle(Title)];
		for (const FName Q : Qs)
		{
			const FLyudQuestDef* D = ULyudQuests::FindDef(Q);
			const bool bSel = !bDiary && Q == SelectedQuest;
			const bool bDone = Quests->State(Q) == ELyudQuestState::Done;
			const bool bShown = Quests->ShownQuest() == Q;
			List->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
			[
				SNew(SButton).ButtonStyle(&FlatButton())
				.OnClicked_Lambda([this, Q]() { SelectedQuest = Q; bDiary = false; Rebuild(); return FReply::Handled(); })
				[
					SNew(SBorder).BorderImage(CellBrush(bSel)).Padding(FMargin(10, 7))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
						[
							Words(FText::FromString(bShown ? TEXT("◆") : TEXT("◇")), 12, D && D->bMain ? Text : Dim)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							Words(ULyudQuests::QuestTitle(Q), 16, bDone ? Faint : Bright, D && D->bMain ? "Bold" : "Regular", true)
						]
					]
				]
			];
		}
	};
	AddQuests(Active, TEXT("ui.journal.active"));
	AddQuests(Done, TEXT("ui.journal.done"));
	if (!Active.Num() && !Done.Num())
	{
		List->AddSlot().AutoHeight()[Key(TEXT("ui.journal.none"), 15, Faint)];
	}

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(330.f)[SNew(SScrollBox) + SScrollBox::Slot()[List]]]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(20, 0, 0, 0)
		[
			SNew(SBorder).BorderImage(CellBrush()).Padding(FMargin(22, 16))
			[
				bDiary || SelectedQuest.IsNone() ? MakeDiary() : MakeQuest(SelectedQuest)
			]
		];
}

TSharedRef<SWidget> SLyudGameScreenWidget::MakeQuest(FName Quest)
{
	ULyudQuests* Quests = ULyudQuests::Get(Owner.Get());
	const FLyudQuestDef* Def = ULyudQuests::FindDef(Quest);
	const FLyudQuestProgress* P = Quests ? Quests->FindProgress(Quest) : nullptr;
	if (!Def || !P)
	{
		return SNew(SSpacer);
	}
	const bool bActive = P->State == ELyudQuestState::Active;
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight()[Key(Def->bMain ? TEXT("ui.journal.main") : TEXT("ui.journal.side"), 12, Dim, "Bold")];
	Box->AddSlot().AutoHeight().Padding(0, 2, 0, 8)[Words(ULyudQuests::QuestTitle(Quest), 26, Bright, "Bold", true)];
	Box->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
	[
		Words(LyudText::Get(FString::Printf(TEXT("quest.%s.desc"), *Quest.ToString())), 15, Body, "Italic", true)
	];

	// now: the current stage's objectives
	FName MarkerPlace;
	if (bActive)
	{
		Box->AddSlot().AutoHeight()[SectionTitle(TEXT("ui.journal.now"))];
		for (const TPair<const FLyudQuestObjective*, int32>& O : Quests->CurrentObjectives(Quest))
		{
			const bool bMet = O.Value >= O.Key->Need;
			if (!bMet && MarkerPlace.IsNone())
			{
				MarkerPlace = O.Key->Place;
			}
			Box->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 10, 0)[Words(FText::FromString(bMet ? TEXT("●") : TEXT("○")), 15, bMet ? Good : Text)]
				+ SHorizontalBox::Slot().FillWidth(1.f)[Words(ULyudQuests::ObjectiveText(Quest, *O.Key, O.Value), 16, bMet ? Faint : Bright, "Regular", true)]
			];
		}
		TSharedRef<SHorizontalBox> Actions = SNew(SHorizontalBox);
		TWeakObjectPtr<ULyudQuests> WeakQ = Quests;
		if (Quests->ShownQuest() != Quest)
		{
			Actions->AddSlot().AutoWidth().Padding(0, 0, 8, 0)
			[
				ActionButton(TEXT("ui.journal.track"), [this, WeakQ, Quest] { if (ULyudQuests* Q = WeakQ.Get()) { Q->Tracked = Quest; Q->OnChanged.Broadcast(); } })
			];
		}
		if (!MarkerPlace.IsNone())
		{
			TWeakObjectPtr<ULyudGameScreen> WeakOwner = Owner;
			Actions->AddSlot().AutoWidth()
			[
				ActionButton(TEXT("ui.journal.on_map"), [WeakOwner, MarkerPlace] { if (ULyudGameScreen* O = WeakOwner.Get()) { O->ShowOnMap(MarkerPlace); } })
			];
		}
		Box->AddSlot().AutoHeight().Padding(0, 10, 0, 0)[Actions];
	}

	// what happened so far, from the journal
	Box->AddSlot().AutoHeight()[SectionTitle(TEXT("ui.journal.story"))];
	for (const FLyudJournalEntry& E : Quests->Entries)
	{
		if (E.Quest != Quest)
		{
			continue;
		}
		Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Words(LyudUIText::Date(E.When), 11, Dim)]
			+ SVerticalBox::Slot().AutoHeight()[Words(E.Text(), 15, Body, "Regular", true)]
		];
	}
	return SNew(SScrollBox) + SScrollBox::Slot()[Box];
}

TSharedRef<SWidget> SLyudGameScreenWidget::MakeDiary()
{
	ULyudQuests* Quests = ULyudQuests::Get(Owner.Get());
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight().Padding(0, 0, 0, 10)[Key(TEXT("ui.journal.diary_title"), 26, Bright, "Bold")];
	if (!Quests || !Quests->Entries.Num())
	{
		Box->AddSlot().AutoHeight()[Key(TEXT("ui.journal.diary_empty"), 15, Faint)];
		return Box;
	}
	// newest first, a date line when the day changes
	FDateTime Day;
	for (int32 I = Quests->Entries.Num() - 1; I >= 0; --I)
	{
		const FLyudJournalEntry& E = Quests->Entries[I];
		if (E.When.GetDate() != Day)
		{
			Day = E.When.GetDate();
			Box->AddSlot().AutoHeight().Padding(0, 8, 0, 4)[Words(LyudUIText::Date(E.When), 13, Dim, "Bold")];
		}
		TSharedRef<SVerticalBox> Line = SNew(SVerticalBox);
		if (!E.Quest.IsNone())
		{
			Line->AddSlot().AutoHeight()[Words(ULyudQuests::QuestTitle(E.Quest), 11, Text)];
		}
		Line->AddSlot().AutoHeight()[Words(E.Text(), 15, Body, "Regular", true)];
		Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)[SNew(SBox).WidthOverride(44.f)[Words(FText::FromString(LyudUIText::Clock(E.When)), 12, Faint)]]
			+ SHorizontalBox::Slot().FillWidth(1.f)[Line]
		];
	}
	return SNew(SScrollBox) + SScrollBox::Slot()[Box];
}

// ------------------------------------------------------------------ map: the list beside it

TSharedRef<SWidget> SLyudGameScreenWidget::MakeMapSide()
{
	ULyudQuests* Quests = ULyudQuests::Get(Owner.Get());
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	FVector2D At, Facing;
	if (Owner.IsValid() && Owner->PlayerOnMap(At, Facing))
	{
		// the nearest place found, as "where am I"
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
		Box->AddSlot().AutoHeight()[SectionTitle(TEXT("ui.map.here"))];
		Box->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
		[
			Words(Near.IsNone() ? FText::GetEmpty() : LyudText::Get(TEXT("ui.map.near"), { FStringFormatArg(ULyudQuests::PlaceName(Near).ToString()),
				FStringFormatArg(FMath::RoundToInt(FMath::Sqrt(Best))) }), 15, Bright, "Regular", true)
		];
		Box->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0, 4, 0, 0)
		[
			ActionButton(TEXT("ui.map.to_me"), [this] { CenterMapOnPlayer(); })
		];
	}
	Box->AddSlot().AutoHeight()[SectionTitle(TEXT("ui.map.places"))];
	int32 NFound = 0;
	for (const FLyudMapPlace& P : ULyudQuests::Places())
	{
		if (!Quests || !Quests->IsFound(P.Id) || P.Kind == TEXT("water") || P.Kind == TEXT("land"))
		{
			continue;
		}
		++NFound;
		const FName Id = P.Id;
		Box->AddSlot().AutoHeight().Padding(0, 0, 0, 3)
		[
			SNew(SButton).ButtonStyle(&FlatButton()).OnClicked_Lambda([this, Id]() { ShowPlace(Id); return FReply::Handled(); })
			[
				SNew(SBox).Padding(FMargin(6, 3))[Words(ULyudQuests::PlaceName(Id), 15, Body, "Regular", true)]
			]
		];
	}
	int32 NAll = 0;
	for (const FLyudMapPlace& P : ULyudQuests::Places())
	{
		NAll += P.Kind != TEXT("water") && P.Kind != TEXT("land");
	}
	Box->AddSlot().AutoHeight().Padding(0, 8, 0, 0)
	[
		Words(LyudText::Get(TEXT("ui.map.found"), { FStringFormatArg(NFound), FStringFormatArg(NAll) }), 12, Dim)
	];
	return SNew(SScrollBox) + SScrollBox::Slot()[Box];
}
