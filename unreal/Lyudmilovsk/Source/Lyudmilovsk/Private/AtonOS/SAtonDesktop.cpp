#include "SAtonDesktop.h"

#include "AtonApps.h"
#include "AtonStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

static constexpr float MenuBarHeight = 30.f;

// ---------------------------------------------------------------- window

void SAtonWindow::Construct(const FArguments& InArgs)
{
	IsActive = InArgs._IsActive;
	OnActivate = InArgs._OnActivate;
	const FSimpleDelegate OnClose = InArgs._OnClose;
	TAttribute<bool> Active = IsActive;

	ChildSlot
	[
		// Hard drop shadow, like the old screen picture.
		SNew(SOverlay)
		+ SOverlay::Slot().Padding(8, 8, 0, 0)
		[
			SNew(SBorder).BorderImage(AtonStyle::Flat()).BorderBackgroundColor(FLinearColor(0, 0, 0, 0.35f))
		]
		+ SOverlay::Slot().Padding(0, 0, 8, 8)
		[
			SNew(SBorder).BorderImage(AtonStyle::Flat()).BorderBackgroundColor(AtonStyle::Ink).Padding(1)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(AtonStyle::Flat())
					.BorderBackgroundColor_Lambda([Active] { return Active.Get() ? AtonStyle::Bar : AtonStyle::BarDim; })
					.Padding(FMargin(10, 0, 2, 0))
					[
						SNew(SBox).HeightOverride(TitleHeight - 1)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
							[
								SNew(STextBlock).Text(InArgs._Title).Font(AtonStyle::Font(13)).ColorAndOpacity(AtonStyle::Ink)
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(SButton)
								.ButtonStyle(&AtonStyle::Ghost())
								.ContentPadding(FMargin(9, 1))
								.OnClicked_Lambda([OnClose] { OnClose.ExecuteIfBound(); return FReply::Handled(); })
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("×"))).Font(AtonStyle::Font(16)).ColorAndOpacity(AtonStyle::Ink)
								]
							]
						]
					]
				]
				+ SVerticalBox::Slot().FillHeight(1)
				[
					InArgs._Content.Widget
				]
			]
		]
	];
}

FReply SAtonWindow::OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Any click in the window raises it; the click itself still goes to whatever is under the cursor.
	OnActivate.ExecuteIfBound();
	return FReply::Unhandled();
}

FReply SAtonWindow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Local.Y < TitleHeight)
	{
		bDragging = true;
		DragStartMouse = MouseEvent.GetScreenSpacePosition();
		DragStartPosition = Position;
		DragScale = FMath::Max(MyGeometry.Scale, KINDA_SMALL_NUMBER);
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	return FReply::Unhandled();
}

FReply SAtonWindow::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bDragging && HasMouseCapture())
	{
		const FVector2D Delta = (MouseEvent.GetScreenSpacePosition() - DragStartMouse) / DragScale;
		// Keep the title bar reachable.
		Position.X = FMath::Clamp(DragStartPosition.X + Delta.X, 80.f - Size.X, Bounds.X - 80.f);
		Position.Y = FMath::Clamp(DragStartPosition.Y + Delta.Y, 0.f, Bounds.Y - TitleHeight);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SAtonWindow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bDragging)
	{
		bDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

// ---------------------------------------------------------------- desktop

void SAtonDesktop::Construct(const FArguments& InArgs)
{
	ScreenSize = InArgs._ScreenSize;
	OnLeave = InArgs._OnLeave;
	CanLeaveAttr = InArgs._CanLeave;
	BootTime = FPlatformTime::Seconds();

	ChildSlot
	[
		SNew(SBox).WidthOverride(ScreenSize.X).HeightOverride(ScreenSize.Y)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder).BorderImage(AtonStyle::Flat()).BorderBackgroundColor(AtonStyle::Desktop)
			]
			+ SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeMenuBar()
				]
				+ SVerticalBox::Slot().FillHeight(1)
				[
					SNew(SOverlay)
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(10, 14)
					[
						MakeIcons()
					]
					+ SOverlay::Slot()
					[
						SAssignNew(Canvas, SConstraintCanvas)
					]
				]
			]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(4, MenuBarHeight, 0, 0)
			[
				MakeSystemMenu()
			]
		]
	];

	// The machine wakes up into the terminal, as on the screen before you sat down.
	OpenApp(AtonApps::Terminal);
}

TSharedRef<SWidget> SAtonDesktop::MakeMenuBar()
{
	return SNew(SBorder)
		.BorderImage(AtonStyle::Flat())
		.BorderBackgroundColor(AtonStyle::Bar)
		.Padding(FMargin(6, 0, 16, 0))
		[
			SNew(SBox).HeightOverride(MenuBarHeight)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Fill)
				[
					SNew(SButton)
					.ButtonStyle(&AtonStyle::Ghost())
					.ContentPadding(FMargin(12, 0))
					.VAlign(VAlign_Center)
					.OnClicked_Lambda([this] { bMenuOpen = !bMenuOpen; return FReply::Handled(); })
					[
						SNew(STextBlock).Text(LyudText::Get(TEXT("ui.aton.os"))).Font(AtonStyle::Font(14, true)).ColorAndOpacity(AtonStyle::Ink)
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(14, 0)
				[
					SNew(STextBlock)
					.Font(AtonStyle::Font(14))
					.ColorAndOpacity(AtonStyle::InkSoft)
					.Text_Lambda([this]
					{
						const FAtonAppInfo* Info = AtonApps::Find(FrontApp());
						return Info ? Info->Title() : FText::GetEmpty();
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(AtonStyle::Mono(14))
					.ColorAndOpacity(AtonStyle::Ink)
					.Text_Lambda([this] { return FText::FromString(Clock()); })
				]
			]
		];
}

TSharedRef<SWidget> SAtonDesktop::MakeIcons()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	for (const FAtonAppInfo& App : AtonApps::List())
	{
		if (!App.bOnDesktop)
		{
			continue;
		}
		const FName Id = App.Id;
		Box->AddSlot().AutoHeight().Padding(0, 0, 0, 2)
		[
			SNew(SButton)
			.ButtonStyle(&AtonStyle::Ghost())
			.ContentPadding(FMargin(4, 4))
			.OnClicked_Lambda([this, Id] { OpenApp(Id); return FReply::Handled(); })
			[
				SNew(SBox).WidthOverride(96)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(SBox).WidthOverride(52).HeightOverride(52)
						[
							SNew(SBorder)
							.BorderImage(AtonStyle::Flat())
							.BorderBackgroundColor(App.Tile)
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock).Text(FText::FromString(App.Glyph)).Font(AtonStyle::Mono(17)).ColorAndOpacity(AtonStyle::Ink)
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 5, 0, 0)
					[
						SNew(STextBlock)
						.Text(App.Title())
						.Font(AtonStyle::Font(12))
						.ColorAndOpacity(AtonStyle::White)
						.Justification(ETextJustify::Center)
						.ShadowOffset(FVector2D(1, 1))
						.ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.5f))
					]
				]
			]
		];
	}
	return Box;
}

TSharedRef<SWidget> SAtonDesktop::MakeSystemMenu()
{
	auto Item = [this](const TCHAR* LabelKey, TFunction<void()> Action, TAttribute<EVisibility> Visible = EVisibility::Visible) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ButtonStyle(&AtonStyle::Ghost())
			.Visibility(Visible)
			.ContentPadding(FMargin(14, 6, 30, 6))
			.OnClicked_Lambda([this, Action] { bMenuOpen = false; Action(); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(LyudText::Get(LabelKey)).Font(AtonStyle::Font(14)).ColorAndOpacity(AtonStyle::Ink)
			];
	};

	return SNew(SBorder)
		.Visibility_Lambda([this] { return bMenuOpen ? EVisibility::Visible : EVisibility::Collapsed; })
		.BorderImage(AtonStyle::Flat())
		.BorderBackgroundColor(AtonStyle::Ink)
		.Padding(1)
		[
			SNew(SBorder).BorderImage(AtonStyle::Flat()).BorderBackgroundColor(AtonStyle::Bar).Padding(FMargin(0, 4))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Item(TEXT("ui.aton.app.about"), [this] { OpenApp(AtonApps::About); })]
				+ SVerticalBox::Slot().AutoHeight()[Item(TEXT("ui.aton.app.computer"), [this] { OpenApp(AtonApps::Computer); })]
				+ SVerticalBox::Slot().AutoHeight()[Item(TEXT("ui.aton.app.terminal"), [this] { OpenApp(AtonApps::Terminal); })]
				+ SVerticalBox::Slot().AutoHeight()
				[
					Item(TEXT("ui.aton.menu.leave"), [this] { Leave(); },
						TAttribute<EVisibility>::CreateLambda([this] { return CanLeave() ? EVisibility::Visible : EVisibility::Collapsed; }))
				]
			]
		];
}

SAtonDesktop::FOpenWindow* SAtonDesktop::Find(FName AppId)
{
	return Windows.FindByPredicate([AppId](const FOpenWindow& W) { return W.AppId == AppId; });
}

FName SAtonDesktop::FrontApp() const
{
	const FOpenWindow* Front = nullptr;
	for (const FOpenWindow& W : Windows)
	{
		if (!Front || W.Z > Front->Z)
		{
			Front = &W;
		}
	}
	return Front ? Front->AppId : NAME_None;
}

void SAtonDesktop::BringToFront(FName AppId)
{
	FOpenWindow* W = Find(AppId);
	if (!W)
	{
		return;
	}
	bMenuOpen = false;
	if (FrontApp() != AppId)
	{
		W->Z = NextZ++;
		W->Slot->SetZOrder(W->Z);
		// Right away, so the click that raised the window can still focus what it hit (a text field, say).
		if (TSharedPtr<SWidget> Target = W->Program->FocusTarget())
		{
			FSlateApplication::Get().SetAllUserFocus(Target, EFocusCause::Mouse);
		}
	}
}

void SAtonDesktop::OpenApp(FName AppId, const FString& Arg)
{
	const FAtonAppInfo* Info = AtonApps::Find(AppId);
	if (!Info || !Canvas.IsValid())
	{
		return;
	}
	if (!Find(AppId))
	{
		FOpenWindow W;
		W.AppId = AppId;
		W.Program = AtonApps::Make(AppId, *this);
		W.Z = NextZ++;
		SAssignNew(W.Window, SAtonWindow)
			.Title(Info->Title())
			.IsActive_Lambda([this, AppId] { return FrontApp() == AppId; })
			.OnActivate_Lambda([this, AppId] { BringToFront(AppId); })
			.OnClose_Lambda([this, AppId] { CloseApp(AppId); })
			[
				W.Program.ToSharedRef()
			];

		const FVector2D Area(ScreenSize.X, ScreenSize.Y - MenuBarHeight);
		const FVector2D Size = Info->WindowSize + FVector2D(8, 8); // + shadow
		const FVector2D Centre = (Area - Size) * 0.5f + FVector2D(40, 0);
		const FVector2D Step(28.f * (Cascade % 6) - 70.f, 26.f * (Cascade % 6) - 60.f);
		Cascade++;
		W.Window->Size = Size;
		W.Window->Bounds = Area;
		W.Window->Position = FVector2D(FMath::Clamp(Centre.X + Step.X, 110.f, FMath::Max(110.f, Area.X - Size.X)),
		                               FMath::Clamp(Centre.Y + Step.Y, 4.f, FMath::Max(4.f, Area.Y - Size.Y)));

		TWeakPtr<SAtonWindow> Weak = W.Window;
		SConstraintCanvas::FScopedWidgetSlotArguments Slot = Canvas->AddSlot();
		Slot.Anchors(FAnchors(0, 0))
			.Alignment(FVector2D(0, 0))
			.ZOrder(W.Z)
			.Offset_Lambda([Weak]
			{
				const TSharedPtr<SAtonWindow> Win = Weak.Pin();
				return Win ? FMargin(Win->Position.X, Win->Position.Y, Win->Size.X, Win->Size.Y) : FMargin(0);
			})
			[
				W.Window.ToSharedRef()
			];
		W.Slot = Slot.GetSlot();
		Windows.Add(W);
	}
	else
	{
		BringToFront(AppId);
	}

	FOpenWindow* Open = Find(AppId);
	if (!Arg.IsEmpty())
	{
		Open->Program->HandleArg(Arg);
	}
	PendingFocus = Open->Program->FocusTarget();
	PendingFocusFrames = 2;
}

void SAtonDesktop::CloseApp(FName AppId)
{
	const int32 Index = Windows.IndexOfByPredicate([AppId](const FOpenWindow& W) { return W.AppId == AppId; });
	if (Index == INDEX_NONE)
	{
		return;
	}
	Canvas->RemoveSlot(Windows[Index].Window.ToSharedRef());
	Windows.RemoveAt(Index);
	FocusFrontWindow();
}

void SAtonDesktop::FocusFrontWindow()
{
	FOpenWindow* Front = Find(FrontApp());
	TSharedPtr<SWidget> Target = Front ? Front->Program->FocusTarget() : nullptr;
	PendingFocus = Target.IsValid() ? Target : TSharedPtr<SWidget>(AsShared());
	PendingFocusFrames = 2;
}

void SAtonDesktop::Leave()
{
	OnLeave.ExecuteIfBound();
}

FString SAtonDesktop::Clock() const
{
	// It was 03:14 when the hero woke up; the clock runs from there.
	const int32 Minutes = (3 * 60 + 14 + FMath::FloorToInt((FPlatformTime::Seconds() - BootTime) / 60.0)) % (24 * 60);
	return FString::Printf(TEXT("%02d:%02d"), Minutes / 60, Minutes % 60);
}

void SAtonDesktop::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (PendingFocusFrames > 0 && --PendingFocusFrames == 0)
	{
		if (TSharedPtr<SWidget> Target = PendingFocus.Pin())
		{
			FSlateApplication::Get().SetAllUserFocus(Target, EFocusCause::SetDirectly);
		}
		PendingFocus.Reset();
	}
}

FReply SAtonDesktop::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape && bMenuOpen)
	{
		bMenuOpen = false;
		return FReply::Handled();
	}
	return FReply::Unhandled();
}
