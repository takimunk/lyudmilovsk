#include "Dev/LyudDevPanel.h"

#include "Birds/BirdSpecies.h"
#include "Dev/LyudDev.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Plants/PlantDevTool.h"
#include "Plants/PlantDevWidgets.h"
#include "UI/LyudQuests.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SWrapBox.h"

// a named namespace, not an anonymous one: unity builds put this file next to others with their own helpers
namespace LyudDevTabs
{
	using namespace PlantDevUI;

	enum : int32
	{
		TabWorld = 0,
		TabTeleport,
		TabQuests,
		TabPocket,
		TabBirds,
		TabActions,
		TabCount
	};

	const TCHAR* TabKeys[TabCount] = { TEXT("ui.dev.tab.world"), TEXT("ui.dev.tab.teleport"), TEXT("ui.dev.tab.quests"), TEXT("ui.dev.tab.pocket"),
		TEXT("ui.dev.tab.birds"), TEXT("ui.dev.tab.actions") };

	const FLinearColor Dim(0.7f, 0.75f, 0.8f);

	using FToolPtr = TWeakObjectPtr<APlantDevTool>;

	UWorld* WorldOf(const FToolPtr& Tool) { return Tool.IsValid() ? Tool->GetWorld() : nullptr; }

	TAttribute<EVisibility> ShownOnTab(const FToolPtr& Tool, int32 Tab)
	{
		return TAttribute<EVisibility>::CreateLambda([Tool, Tab]() { return Tool.IsValid() && Tool->Tab == Tab ? EVisibility::Visible : EVisibility::Collapsed; });
	}

	/** Whether a row passes the tab's search: every word of it in the shown name or the id. */
	bool Matches(const FString& Filter, const FString& Name, FName Id)
	{
		TArray<FString> Words;
		Filter.ParseIntoArrayWS(Words);
		for (const FString& Word : Words)
		{
			if (!Name.Contains(Word) && !Id.ToString().Contains(Word))
			{
				return false;
			}
		}
		return true;
	}

	TSharedRef<SWidget> SearchBox(TSharedRef<FString> Filter)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Label(LyudText::Get(TEXT("ui.dev.search")), 90.f)]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SEditableTextBox).Font(Font())
				.OnTextChanged_Lambda([Filter](const FText& Typed) { *Filter = Typed.ToString(); })
				.OnTextCommitted_Lambda([](const FText&, ETextCommit::Type) { FSlateApplication::Get().SetAllUserFocusToGameViewport(); })
			];
	}

	TSharedRef<SWidget> Small(const FText& Text, FLinearColor Color = Dim)
	{
		return SNew(STextBlock).AutoWrapText(true).Text(Text).Font(Font(9)).ColorAndOpacity(Color);
	}

	TSharedRef<SWidget> GroupTitle(const FText& Text)
	{
		return SNew(STextBlock).Text(Text).Font(Font(12)).ColorAndOpacity(Heading);
	}

	/** A group's or an action's words: "dev.group.<g>" in the tables, else the raw name. */
	FText GroupName(FName Group) { return LyudDev::NameOf(TEXT("dev.group.") + Group.ToString(), Group); }

	// ---- Teleport ------------------------------------------------------------------------------------------------------

	TSharedRef<SWidget> TeleportTab(const FToolPtr& Tool)
	{
		TSharedRef<FString> Filter = MakeShared<FString>();
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		Box->AddSlot().AutoHeight().Padding(0, 2)[SearchBox(Filter)];
		Box->AddSlot().AutoHeight().Padding(0, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Button(LyudText::Get(TEXT("dev.action.move.bookmark")), [Tool]()
				{
					if (Tool.IsValid()) { LyudDev::AddBookmark(WorldOf(Tool), FString()); Tool->Rebuild(); }
				})]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)[Button(LyudText::Get(TEXT("dev.action.move.where")), [Tool]() { LyudDev::Where(WorldOf(Tool), true); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)[Button(LyudText::Get(TEXT("dev.action.move.fly")), [Tool]() { LyudDev::Do(WorldOf(Tool), TEXT("move.fly")); },
				[Tool]() { const LyudDev::FAction* A = LyudDev::FindAction(TEXT("move.fly")); return A && A->IsOn && A->IsOn(WorldOf(Tool)); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)[Button(LyudText::Get(TEXT("ui.dev.reload")), [Tool]()
				{
					if (Tool.IsValid()) { LyudDev::Reload(); Tool->Rebuild(); }
				})]
		];
		Box->AddSlot().AutoHeight().Padding(0, 2)
		[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Dim).Text_Lambda([Tool]() { return FText::FromString(LyudDev::Where(WorldOf(Tool), false)); })];

		FName Group;
		TSharedPtr<SWrapBox> Wrap;
		for (const LyudDev::FTeleport& T : LyudDev::Teleports())
		{
			if (T.Group != Group || !Wrap)
			{
				Group = T.Group;
				const FName ThisGroup = Group;
				Wrap = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f));
				Box->AddSlot().AutoHeight().Padding(0, 8, 0, 2)[GroupTitle(GroupName(ThisGroup))];
				Box->AddSlot().AutoHeight()[Wrap.ToSharedRef()];
			}
			const FText Name = !T.Name.IsEmpty() ? LyudDev::NameOf(T.Name, T.Id) : !T.Place.IsNone() ? ULyudQuests::PlaceName(T.Place) : FText::FromName(T.Id);
			const FString NameText = Name.ToString();
			const FName Id = T.Id;
			const LyudDev::FTeleport Point = T;
			Wrap->AddSlot()
			[
				SNew(SBox).Visibility_Lambda([Filter, NameText, Id]() { return Matches(*Filter, NameText, Id) ? EVisibility::Visible : EVisibility::Collapsed; })
				.ToolTipText(FText::FromString(FString::Printf(TEXT("%s · %s · map %.0f %.0f"), *Id.ToString(), *Point.Source, Point.Map.X, Point.Map.Y)))
				[Button(Name, [Tool, Point]() { LyudDev::Teleport(WorldOf(Tool), Point); }, nullptr, 10)]
			];
		}
		return Box;
	}

	// ---- Quests --------------------------------------------------------------------------------------------------------

	TSharedRef<SWidget> QuestsTab(const FToolPtr& Tool)
	{
		TSharedRef<FString> Filter = MakeShared<FString>();
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		Box->AddSlot().AutoHeight().Padding(0, 2)[SearchBox(Filter)];
		Box->AddSlot().AutoHeight().Padding(0, 2)[Small(LyudText::Get(TEXT("ui.dev.quests_hint")))];
		for (const FLyudQuestDef& Def : ULyudQuests::Defs())
		{
			const FName Quest = Def.Id;
			const FString Title = ULyudQuests::QuestTitle(Quest).ToString();
			auto Progress = [Tool, Quest]() -> const FLyudQuestProgress*
			{
				const ULyudQuests* Q = ULyudQuests::Get(WorldOf(Tool));
				return Q ? Q->FindProgress(Quest) : nullptr;
			};
			TSharedRef<SVerticalBox> One = SNew(SVerticalBox)
				.Visibility_Lambda([Filter, Title, Quest]() { return Matches(*Filter, Title, Quest) ? EVisibility::Visible : EVisibility::Collapsed; });
			One->AddSlot().AutoHeight().Padding(0, 8, 0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[SNew(STextBlock).Font(Font(12)).ColorAndOpacity(Heading).Text_Lambda([Title, Quest, Progress]()
					{
						const FLyudQuestProgress* P = Progress();
						const TCHAR* State = !P || P->State == ELyudQuestState::Inactive ? TEXT("ui.dev.quest.inactive")
							: P->State == ELyudQuestState::Done ? TEXT("ui.dev.quest.done") : TEXT("ui.dev.quest.active");
						return FText::FromString(FString::Printf(TEXT("%s (%s) · %s"), *Title, *Quest.ToString(), *LyudText::Get(State).ToString()));
					})]
				+ SHorizontalBox::Slot().AutoWidth()[Button(LyudText::Get(TEXT("ui.dev.quest.forget")), [Tool, Quest]()
					{
						if (ULyudQuests* Q = ULyudQuests::Get(WorldOf(Tool))) Q->DevForget(Quest);
					}, nullptr, 10)]
			];
			// the stages: a click jumps there (and sets it up by the stage's dev block)
			TSharedRef<SWrapBox> Stages = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f));
			int32 MaxObjectives = 0;
			for (int32 S = 0; S < Def.Stages.Num(); ++S)
			{
				MaxObjectives = FMath::Max(MaxObjectives, Def.Stages[S].Objectives.Num());
				Stages->AddSlot()[Button(FText::FromString(FString::Printf(TEXT("%d %s"), S + 1, *Def.Stages[S].Id.ToString())),
					[Tool, Quest, S]() { LyudDev::JumpToStage(WorldOf(Tool), Quest, S); },
					[Progress, S]() { const FLyudQuestProgress* P = Progress(); return P && P->State == ELyudQuestState::Active && P->Stage == S; }, 10)];
			}
			One->AddSlot().AutoHeight()[Stages];
			// the current stage's objectives: a click fires the real event for it
			for (int32 O = 0; O < MaxObjectives; ++O)
			{
				auto Objective = [Tool, Quest, O]() -> FText
				{
					const ULyudQuests* Q = ULyudQuests::Get(WorldOf(Tool));
					const TArray<TPair<const FLyudQuestObjective*, int32>> Objs = Q ? Q->CurrentObjectives(Quest) : TArray<TPair<const FLyudQuestObjective*, int32>>();
					if (!Objs.IsValidIndex(O))
					{
						return FText::GetEmpty();
					}
					const FLyudQuestObjective& Obj = *Objs[O].Key;
					return FText::FromString(FString::Printf(TEXT("✓ %s  [%s %s]"), *ULyudQuests::ObjectiveText(Quest, Obj, Objs[O].Value).ToString(),
						*Obj.Event.ToString(), *Obj.Target));
				};
				One->AddSlot().AutoHeight().Padding(12, 2, 0, 0)
				[
					SNew(SBox).Visibility_Lambda([Objective]() { return Objective().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
					[
						SNew(SButton).IsFocusable(false).OnClicked_Lambda([Tool, Quest, O]() { LyudDev::CompleteObjective(WorldOf(Tool), Quest, O); return FReply::Handled(); })
						[SNew(STextBlock).Font(Font(10)).AutoWrapText(true).Text_Lambda(Objective)]
					]
				];
			}
			Box->AddSlot().AutoHeight()[One];
		}
		return Box;
	}

	// ---- Pocket --------------------------------------------------------------------------------------------------------

	TSharedRef<SWidget> PocketTab(const FToolPtr& Tool)
	{
		TSharedRef<FString> Filter = MakeShared<FString>();
		auto Inv = [Tool]() { return ULyudInventory::Get(WorldOf(Tool)); };
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		Box->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[SNew(STextBlock).Font(Font(12)).ColorAndOpacity(FLinearColor::White).Text_Lambda([Inv]()
				{ return LyudText::Get(TEXT("ui.dev.rubles"), { FStringFormatArg(Inv() ? Inv()->Rubles : 0) }); })]
			+ SHorizontalBox::Slot().AutoWidth()[Button(FText::FromString(TEXT("+100")), [Inv]() { if (Inv()) Inv()->Earn(100); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)[Button(FText::FromString(TEXT("+1000")), [Inv]() { if (Inv()) Inv()->Earn(1000); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)[Button(LyudText::Get(TEXT("ui.dev.rubles_zero")), [Inv]() { if (Inv()) Inv()->Spend(Inv()->Rubles); })]
		];
		Box->AddSlot().AutoHeight().Padding(0, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Button(LyudText::Get(TEXT("dev.action.pocket.all")), [Tool]() { LyudDev::Do(WorldOf(Tool), TEXT("pocket.all")); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)[Button(LyudText::Get(TEXT("dev.action.pocket.reset")), [Tool]() { LyudDev::Do(WorldOf(Tool), TEXT("pocket.reset")); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)[Button(LyudText::Get(TEXT("ui.dev.pocket_held")), [Tool]()
				{
					if (Tool.IsValid()) Tool->bPocketHeldOnly = !Tool->bPocketHeldOnly;
				}, [Tool]() { return Tool.IsValid() && Tool->bPocketHeldOnly; })]
		];
		Box->AddSlot().AutoHeight().Padding(0, 2)[SearchBox(Filter)];
		for (const FLyudItemDef& Def : ULyudInventory::AllDefs())
		{
			const FName Id = Def.Id;
			const FString Name = ULyudInventory::ItemName(Id).ToString();
			const bool bWearable = Def.Slot != ELyudEquipSlot::None;
			TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
				.Visibility_Lambda([Tool, Filter, Name, Id, Inv]()
				{
					const bool bHeldOnly = Tool.IsValid() && Tool->bPocketHeldOnly;
					return Matches(*Filter, Name, Id) && (!bHeldOnly || (Inv() && Inv()->Count(Id) > 0)) ? EVisibility::Visible : EVisibility::Collapsed;
				})
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[SNew(STextBlock).Font(Font(10)).ColorAndOpacity(FLinearColor::White).ToolTipText(FText::FromName(Id)).Text_Lambda([Name, Id, Inv]()
					{
						const int32 N = Inv() ? Inv()->Count(Id) : 0;
						const bool bWorn = Inv() && (Inv()->IsWorn(Id) || Inv()->EquippedItem() == Id);
						return FText::FromString(FString::Printf(TEXT("%s  ×%d%s"), *Name, N, bWorn ? TEXT("  ●") : TEXT("")));
					})]
				+ SHorizontalBox::Slot().AutoWidth()[Button(FText::FromString(TEXT("+1")), [Inv, Id]() { if (Inv()) Inv()->Add(Id, 1); }, nullptr, 9)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3, 0, 0, 0)[Button(FText::FromString(TEXT("+10")), [Inv, Id]() { if (Inv()) Inv()->Add(Id, 10); }, nullptr, 9)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3, 0, 0, 0)[Button(LyudText::Get(TEXT("ui.dev.minus")), [Inv, Id]() { if (Inv()) Inv()->Remove(Id, 1); }, nullptr, 9)];
			if (bWearable)
			{
				Row->AddSlot().AutoWidth().Padding(3, 0, 0, 0)[Button(LyudText::Get(TEXT("ui.dev.wear")), [Inv, Id]()
					{
						ULyudInventory* I = Inv();
						if (!I) return;
						if (I->Count(Id) == 0) I->Add(Id, 1);
						if (ULyudInventory::SlotOf(Id) == ELyudEquipSlot::Hand) I->Equip(Id); else I->Wear(Id);
					}, nullptr, 9)];
			}
			Box->AddSlot().AutoHeight().Padding(0, 1)[Row];
		}
		return Box;
	}

	// ---- Birds ---------------------------------------------------------------------------------------------------------

	TSharedRef<SWidget> BirdsTab(const FToolPtr& Tool)
	{
		TSharedRef<FString> Filter = MakeShared<FString>();
		auto Run = [Tool](const FString& Cmd) { if (UWorld* W = WorldOf(Tool)) GEngine->Exec(W, *Cmd); };
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		Box->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Button(LyudText::Get(TEXT("dev.action.move.bird_return")), [Run]() { Run(TEXT("Bird.Return now")); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)[Button(LyudText::Get(TEXT("dev.action.pocket.feed")), [Run]() { Run(TEXT("Tame.Give 20")); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)[Button(LyudText::Get(TEXT("ui.dev.birds.meet_all")), [Run]()
				{
					for (const FBirdSpecies& S : UBirdSpeciesLibrary::All()) Run(TEXT("Birds.Meet ") + S.Id.ToString());
				})]
		];
		Box->AddSlot().AutoHeight().Padding(0, 2)[Small(LyudText::Get(TEXT("ui.dev.birds_hint")))];
		Box->AddSlot().AutoHeight().Padding(0, 2)[SearchBox(Filter)];
		for (const FBirdSpecies& S : UBirdSpeciesLibrary::All())
		{
			const FString Id = S.Id.ToString();
			const FName SpeciesId = S.Id;
			const FString Name = LyudText::Get(TEXT("bird.") + S.Key + TEXT(".name")).ToString();
			Box->AddSlot().AutoHeight().Padding(0, 1)
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([Filter, Name, SpeciesId]() { return Matches(*Filter, Name, SpeciesId) ? EVisibility::Visible : EVisibility::Collapsed; })
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[SNew(STextBlock).Font(Font(10)).ColorAndOpacity(FLinearColor::White).Text(FText::FromString(Name + TEXT("  ·  ") + S.Latin))]
				+ SHorizontalBox::Slot().AutoWidth()[Button(LyudText::Get(TEXT("ui.dev.birds.become")), [Run, Id]() { Run(TEXT("Bird.Become ") + Id); }, nullptr, 9)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3, 0, 0, 0)[Button(LyudText::Get(TEXT("ui.dev.birds.fly")), [Run, Id]() { Run(TEXT("Bird.Become ") + Id + TEXT(" air")); }, nullptr, 9)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3, 0, 0, 0)[Button(LyudText::Get(TEXT("ui.dev.birds.spawn")), [Run, Id]() { Run(TEXT("Birds.SpawnNear ") + Id); }, nullptr, 9)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3, 0, 0, 0)[Button(LyudText::Get(TEXT("ui.dev.birds.befriend")), [Run, Id]()
					{
						// one calm bird beside the hero, then made a friend
						Run(TEXT("Birds.SpawnNear ") + Id);
						Run(TEXT("Birds.Meet ") + Id + TEXT(" tame"));
					}, nullptr, 9)]
			];
		}
		return Box;
	}

	// ---- Actions and the console line ----------------------------------------------------------------------------------

	TSharedRef<SWidget> ActionsTab(const FToolPtr& Tool)
	{
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		// a console line: any command, with the last few to click again
		TSharedRef<TArray<FString>> History = MakeShared<TArray<FString>>();
		Box->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Label(LyudText::Get(TEXT("ui.dev.console")), 90.f)]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SEditableTextBox).Font(Font()).ClearKeyboardFocusOnCommit(false).SelectAllTextOnCommit(true)
				.HintText(LyudText::Get(TEXT("ui.dev.console_hint")))
				.OnTextCommitted_Lambda([Tool, History](const FText& Typed, ETextCommit::Type How)
				{
					const FString Cmd = Typed.ToString().TrimStartAndEnd();
					if (How != ETextCommit::OnEnter || Cmd.IsEmpty())
					{
						return;
					}
					if (UWorld* W = WorldOf(Tool))
					{
						UE_LOG(LogTemp, Warning, TEXT("DEV console %s"), *Cmd);
						GEngine->Exec(W, *Cmd);
					}
					History->Remove(Cmd);
					History->Insert(Cmd, 0);
					History->SetNum(FMath::Min(History->Num(), 6));
				})
			]
		];
		for (int32 H = 0; H < 6; ++H)
		{
			Box->AddSlot().AutoHeight().Padding(90, 1, 0, 0)
			[
				SNew(SBox).HAlign(HAlign_Left).Visibility_Lambda([History, H]() { return History->IsValidIndex(H) ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SButton).IsFocusable(false).OnClicked_Lambda([Tool, History, H]()
					{
						if (UWorld* W = WorldOf(Tool); W && History->IsValidIndex(H)) GEngine->Exec(W, *(*History)[H]);
						return FReply::Handled();
					})
					[SNew(STextBlock).Font(Font(9)).Text_Lambda([History, H]() { return History->IsValidIndex(H) ? FText::FromString((*History)[H]) : FText::GetEmpty(); })]
				]
			];
		}
		FName Group;
		TSharedPtr<SWrapBox> Wrap;
		for (const LyudDev::FAction* A : LyudDev::Actions())
		{
			if (A->Group != Group || !Wrap)
			{
				Group = A->Group;
				Wrap = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f));
				Box->AddSlot().AutoHeight().Padding(0, 8, 0, 2)[GroupTitle(GroupName(Group))];
				Box->AddSlot().AutoHeight()[Wrap.ToSharedRef()];
			}
			const FName Id = A->Id;
			const bool bToggle = static_cast<bool>(A->IsOn);
			Wrap->AddSlot()
			[
				SNew(SBox).ToolTipText(FText::FromString(TEXT("Dev.Do ") + Id.ToString()))
				[Button(LyudDev::NameOf(A->Name, Id), [Tool, Id]() { LyudDev::Do(WorldOf(Tool), Id); },
					bToggle ? TFunction<bool()>([Tool, Id]() { const LyudDev::FAction* Act = LyudDev::FindAction(Id); return Act && Act->IsOn && Act->IsOn(WorldOf(Tool)); }) : TFunction<bool()>(), 10)]
			];
		}
		return Box;
	}
}

namespace LyudDevPanel
{
	using namespace LyudDevTabs;

	TSharedRef<SWidget> TabBar(APlantDevTool* InTool)
	{
		const FToolPtr Tool(InTool);
		TSharedRef<SWrapBox> Bar = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f));
		for (int32 T = 0; T < TabCount; ++T)
		{
			Bar->AddSlot()[Button(LyudText::Get(TabKeys[T]), [Tool, T]() { if (Tool.IsValid()) Tool->Tab = T; },
				[Tool, T]() { return Tool.IsValid() && Tool->Tab == T; })];
		}
		return Bar;
	}

	TSharedRef<SWidget> Tabs(APlantDevTool* InTool)
	{
		const FToolPtr Tool(InTool);
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility(ShownOnTab(Tool, TabTeleport))[TeleportTab(Tool)]]
			+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility(ShownOnTab(Tool, TabQuests))[QuestsTab(Tool)]]
			+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility(ShownOnTab(Tool, TabPocket))[PocketTab(Tool)]]
			+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility(ShownOnTab(Tool, TabBirds))[BirdsTab(Tool)]]
			+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility(ShownOnTab(Tool, TabActions))[ActionsTab(Tool)]];
	}
}
