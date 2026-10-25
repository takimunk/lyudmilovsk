#pragma once

#include "CoreMinimal.h"
#include "UI/LyudGameScreen.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class SLyudMapView;
class SWidgetSwitcher;
class ULyudInventory;
class ULyudInventoryUI;

/** The game screen's widget: the tabs, the money and the date on top, one page under them (inventory, journal, map). */
class SLyudGameScreenWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudGameScreenWidget) {}
		SLATE_ARGUMENT(ULyudGameScreen*, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetTab(ELyudScreenTab Tab);
	/** Builds the pages again from the inventory and the quests (they changed, or the language did). */
	void Rebuild();
	void ShowPlace(FName Place);
	void CenterMapOnPlayer();
	void ShowDiary() { bDiary = true; Rebuild(); }

private:
	TSharedRef<SWidget> MakeHeader();
	TSharedRef<SWidget> MakeInventory();
	TSharedRef<SWidget> MakeJournal();
	TSharedRef<SWidget> MakeQuest(FName Quest);
	TSharedRef<SWidget> MakeDiary();
	TSharedRef<SWidget> MakeMapSide();
	TSharedRef<SWidget> ItemIcon(FName Id, float Size);

	TWeakObjectPtr<ULyudGameScreen> Owner;
	TWeakObjectPtr<ULyudInventory> Inventory;
	TWeakObjectPtr<ULyudInventoryUI> InventoryUI;
	ELyudScreenTab Tab = ELyudScreenTab::Inventory;

	TSharedPtr<SBox> HeaderBox;
	TSharedPtr<SBox> InventoryBox;
	TSharedPtr<SBox> JournalBox;
	TSharedPtr<SBox> MapSideBox;
	TSharedPtr<SWidgetSwitcher> Pages;
	TSharedPtr<SLyudMapView> Map;

	FName SelectedItem;
	FName SelectedQuest;
	bool bDiary = false;
	FString Language;
};
