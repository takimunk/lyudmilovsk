#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ULyudGameMenuSubsystem;
class ULyudSaveSubsystem;
class SEditableTextBox;
class SVerticalBox;
class SWidgetSwitcher;

/** The menu's screens: main menu, pause menu, save to a slot, load a slot, settings. */
class SLyudGameMenu : public SCompoundWidget
{
public:
	enum class EScreen : uint8 { Main, Pause, Save, Load, Settings };

	SLATE_BEGIN_ARGS(SLyudGameMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ULyudGameMenuSubsystem>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Shows a root screen (Main or Pause). bStartup: the main menu that opens with the game, "Новая игра" just starts it. */
	void ShowRoot(EScreen Root, bool bStartup);

	/** Opens a screen over the current one (Esc comes back). */
	void Push(EScreen NewScreen) { Show(NewScreen); }

	/** Esc: one screen back; false at the root (the caller closes the menu). */
	bool Back();

	EScreen GetScreen() const { return Screen; }

	/** The widget keyboard focus should go to. */
	TSharedPtr<SWidget> GetFocusTarget() const;
	virtual bool SupportsKeyboardFocus() const override { return true; }

private:
	/** Builds every screen (again after the language changes). */
	void BuildContent();
	void Show(EScreen NewScreen);
	void Activate(EScreen NewScreen);
	void RebuildSlots();
	ULyudSaveSubsystem* Saves() const;
	FString DefaultTitle() const;
	void SetStatus(const FString& Text);

	TSharedRef<SWidget> MakeButton(const FText& Label, TFunction<void()> OnClick, TAttribute<EVisibility> Visible = EVisibility::Visible);
	TSharedRef<SWidget> MakeMain();
	TSharedRef<SWidget> MakePause();
	TSharedRef<SWidget> MakeSave();
	TSharedRef<SWidget> MakeLoad();
	TSharedRef<SWidget> MakeSettings();

	TWeakObjectPtr<ULyudGameMenuSubsystem> Owner;
	TSharedPtr<SWidgetSwitcher> Switcher;
	TSharedPtr<SEditableTextBox> TitleBox;
	TSharedPtr<SVerticalBox> SaveList;
	TSharedPtr<SVerticalBox> LoadList;
	FText Status;
	/** Slot whose "Удалить" was pressed once and waits for the confirming second press. */
	FString ArmedDelete;
	/** Screens opened since the root, the current one last; Esc pops. */
	TArray<EScreen> History;
	EScreen Screen = EScreen::Pause;
	bool bStartup = false;
	bool bHasSaves = false;
	/** The language the screens were built in (a switch from the console rebuilds them on the next open). */
	FString BuiltLanguage;
};
