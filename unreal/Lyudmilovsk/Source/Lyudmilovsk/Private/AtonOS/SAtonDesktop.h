#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SConstraintCanvas.h"

class SAtonWindow;
class SAtonProgram;

/** What programs may ask of the OS. */
class IAtonShell
{
public:
	virtual ~IAtonShell() = default;
	/** Opens a program (or brings it to the front); Arg is program-specific, e.g. the note to show. */
	virtual void OpenApp(FName AppId, const FString& Arg = FString()) = 0;
	virtual void CloseApp(FName AppId) = 0;
	/** Get up from the computer (only offered when someone handles it). */
	virtual void Leave() = 0;
	virtual bool CanLeave() const = 0;
	/** The in-game clock, "03:14" at boot. */
	virtual FString Clock() const = 0;
};

/** A program window: title bar to drag it by, a close button and the program inside. */
class SAtonWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAtonWindow) {}
		SLATE_ARGUMENT(FText, Title)
		SLATE_ATTRIBUTE(bool, IsActive)
		SLATE_EVENT(FSimpleDelegate, OnActivate)
		SLATE_EVENT(FSimpleDelegate, OnClose)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D(600, 420);
	/** The desktop area windows are kept inside. */
	FVector2D Bounds = FVector2D(1024, 738);

	static constexpr float TitleHeight = 28.f;

	virtual FReply OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	TAttribute<bool> IsActive;
	FSimpleDelegate OnActivate;
	bool bDragging = false;
	FVector2D DragStartMouse;
	FVector2D DragStartPosition;
	float DragScale = 1.f;
};

/** The whole screen of ATON OS: menu bar, desktop icons and program windows. */
class SAtonDesktop : public SCompoundWidget, public IAtonShell
{
public:
	SLATE_BEGIN_ARGS(SAtonDesktop) {}
		SLATE_ARGUMENT(FVector2D, ScreenSize)
		/** Whether getting up is possible; the menu shows "Встать из-за компьютера" then. */
		SLATE_ATTRIBUTE(bool, CanLeave)
		SLATE_EVENT(FSimpleDelegate, OnLeave)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** The player sat down: give the keyboard to the front window. */
	void FocusFrontWindow();

	// IAtonShell
	virtual void OpenApp(FName AppId, const FString& Arg = FString()) override;
	virtual void CloseApp(FName AppId) override;
	virtual void Leave() override;
	virtual bool CanLeave() const override { return CanLeaveAttr.Get(false); }
	virtual FString Clock() const override;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

private:
	struct FOpenWindow
	{
		FName AppId;
		TSharedPtr<SAtonWindow> Window;
		TSharedPtr<SAtonProgram> Program;
		SConstraintCanvas::FSlot* Slot = nullptr;
		float Z = 0.f;
	};

	FOpenWindow* Find(FName AppId);
	void BringToFront(FName AppId);
	FName FrontApp() const;
	TSharedRef<SWidget> MakeMenuBar();
	TSharedRef<SWidget> MakeIcons();
	TSharedRef<SWidget> MakeSystemMenu();

	FVector2D ScreenSize;
	FSimpleDelegate OnLeave;
	TAttribute<bool> CanLeaveAttr;
	TSharedPtr<SConstraintCanvas> Canvas;
	TArray<FOpenWindow> Windows;
	float NextZ = 1.f;
	int32 Cascade = 0;
	bool bMenuOpen = false;
	double BootTime = 0.0;
	/** Programs to focus once they are on screen (Slate cannot focus a widget before its first paint). */
	TWeakPtr<SWidget> PendingFocus;
	int32 PendingFocusFrames = 0;
};
