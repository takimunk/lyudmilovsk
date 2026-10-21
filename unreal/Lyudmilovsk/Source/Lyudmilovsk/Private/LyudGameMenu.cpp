#include "LyudGameMenu.h"

#include "AtonComputer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameMenu/SLyudGameMenu.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "LyudSave.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SViewport.h"
#include "Widgets/Text/STextBlock.h"

static TAutoConsoleVariable<int32> CVarMainMenuOnStart(
	TEXT("lyud.MainMenuOnStart"), -1,
	TEXT("Show the main menu when the game starts: 1 always, 0 never, -1 only outside the editor."));

static TAutoConsoleVariable<int32> CVarMenuButton(
	TEXT("lyud.MenuButton"), -1,
	TEXT("On-screen \"Меню\" button: 1 always, 0 never, -1 only on phones and tablets."));

/** Sees keys before the game and the widgets do, so the menu opens from the game, the ATON OS or a cutscene alike. */
class FLyudMenuInput : public IInputProcessor
{
public:
	explicit FLyudMenuInput(ULyudGameMenuSubsystem* InOwner) : Owner(InOwner) {}

	virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& InKeyEvent) override
	{
		ULyudGameMenuSubsystem* Menu = Owner.Get();
		return Menu && !InKeyEvent.IsRepeat() && Menu->HandleKey(InKeyEvent.GetKey());
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("LyudGameMenu"); }

private:
	TWeakObjectPtr<ULyudGameMenuSubsystem> Owner;
};

void ULyudGameMenuSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&World);
	if (Saves)
	{
		Saves->NotifyWorldBeginPlay(World);
	}
	UGameViewportClient* Viewport = World.GetGameViewport();
	if (!Viewport || !FSlateApplication::IsInitialized())
	{
		return;
	}

	Menu = SNew(SLyudGameMenu).Owner(this);
	Input = MakeShared<FLyudMenuInput>(this);
	FSlateApplication::Get().RegisterInputPreProcessor(Input);

	const int32 ButtonMode = CVarMenuButton.GetValueOnGameThread();
	if (ButtonMode == 1 || (ButtonMode < 0 && (PLATFORM_IOS || PLATFORM_ANDROID)))
	{
		TWeakObjectPtr<ULyudGameMenuSubsystem> Self(this);
		MenuButton = SNew(SBox)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Top)
			.Padding(FMargin(0.f, 24.f, 24.f, 0.f))
			[
				SNew(SButton)
				.OnClicked_Lambda([Self] { if (Self.IsValid() && !Self->IsOpen()) { Self->Open(false); } return FReply::Handled(); })
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Меню"))).Font(FCoreStyle::GetDefaultFontStyle("Regular", 20))
				]
			];
		Viewport->AddViewportWidgetContent(MenuButton.ToSharedRef(), 90);
	}

	const int32 MainMode = CVarMainMenuOnStart.GetValueOnGameThread();
	const bool bWantMain = MainMode == 1 || (MainMode < 0 && !GIsEditor);
	if (Saves && !Saves->bMainMenuShown && bWantMain)
	{
		Saves->bMainMenuShown = true;
		Open(true);
	}
	else if (Saves)
	{
		Saves->bMainMenuShown = true;
	}
}

void ULyudGameMenuSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(World))
		{
			Saves->NotifyWorldEnding(*World);
		}
		if (UGameViewportClient* Viewport = World->GetGameViewport())
		{
			if (Menu && bOpen)
			{
				Viewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
			}
			if (MenuButton)
			{
				Viewport->RemoveViewportWidgetContent(MenuButton.ToSharedRef());
			}
		}
	}
	if (Input && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Input);
	}
	Input.Reset();
	Menu.Reset();
	MenuButton.Reset();
	bOpen = false;
	Super::Deinitialize();
}

void ULyudGameMenuSubsystem::Open(bool bMain)
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Menu || !Viewport)
	{
		return;
	}
	if (!bOpen)
	{
		bOpen = true;
		bWasPaused = UGameplayStatics::IsGamePaused(World);
		Viewport->AddViewportWidgetContent(Menu.ToSharedRef(), 100);
		UGameplayStatics::SetGamePaused(World, true);
	}
	Menu->ShowRoot(bMain ? SLyudGameMenu::EScreen::Main : SLyudGameMenu::EScreen::Pause, bMain);
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		PC->SetShowMouseCursor(true);
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Menu->GetFocusTarget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
	}
}

void ULyudGameMenuSubsystem::Close()
{
	UWorld* World = GetWorld();
	if (!bOpen || !World)
	{
		return;
	}
	bOpen = false;
	if (UGameViewportClient* Viewport = World->GetGameViewport())
	{
		Viewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
	}
	UGameplayStatics::SetGamePaused(World, bWasPaused);
	RestoreGameInput();
}

void ULyudGameMenuSubsystem::RestoreGameInput()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}
	// Back at the computer: the OS takes the mouse and keyboard again.
	for (TActorIterator<AAtonComputer> It(World); It; ++It)
	{
		if (It->IsActive())
		{
			It->Activate(PC);
			return;
		}
	}
	PC->SetShowMouseCursor(false);
	PC->SetInputMode(FInputModeGameOnly());
}

bool ULyudGameMenuSubsystem::GameHasFocus() const
{
	// Outside the editor every key is the game's. In PIE, keys typed into other editor panels are not.
	if (!GIsEditor)
	{
		return true;
	}
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	TSharedPtr<SViewport> ViewportWidget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
	if (!ViewportWidget)
	{
		return false;
	}
	for (TSharedPtr<SWidget> W = FSlateApplication::Get().GetUserFocusedWidget(0); W; W = W->GetParentWidget())
	{
		if (W == ViewportWidget)
		{
			return true;
		}
	}
	// The ATON OS lives on a world-space widget, outside the viewport's widget tree.
	for (TActorIterator<AAtonComputer> It(World); It; ++It)
	{
		if (It->IsActive())
		{
			return true;
		}
	}
	return false;
}

bool ULyudGameMenuSubsystem::HandleKey(const FKey& Key)
{
	UWorld* World = GetWorld();
	if (!Menu || !World)
	{
		return false;
	}
	ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(World);
	if (Saves && Saves->IsTravelling())
	{
		return false;
	}
	const bool bEsc = Key == EKeys::Escape || Key == EKeys::Android_Back;
	const bool bToggle = Key == EKeys::F10 || Key == EKeys::Gamepad_Special_Right;
	if (!bEsc && !bToggle)
	{
		return false;
	}
	if (bOpen)
	{
		// Esc steps back a screen; at the pause menu it resumes. The start-up main menu stays until a choice is made.
		if (!Menu->Back() && Menu->GetScreen() == SLyudGameMenu::EScreen::Pause)
		{
			Close();
		}
		return true;
	}
	if (!GameHasFocus())
	{
		return false;
	}
	// In the editor Esc stops PIE, so there the menu opens with F10.
	if (bEsc && World->WorldType == EWorldType::PIE)
	{
		return false;
	}
	// Esc first closes the ATON OS start menu, if it is open.
	if (bEsc)
	{
		for (TActorIterator<AAtonComputer> It(World); It; ++It)
		{
			if (It->IsActive() && It->IsOSMenuOpen())
			{
				return false;
			}
		}
	}
	Open(false);
	return true;
}
