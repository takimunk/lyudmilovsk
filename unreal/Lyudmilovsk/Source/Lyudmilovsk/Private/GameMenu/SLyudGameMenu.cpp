#include "GameMenu/SLyudGameMenu.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LyudGameMenu.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "UI/LyudSettings.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Text/STextBlock.h"


namespace LyudMenuStyle
{
	// Warm lamp light on dark painted metal, like the corridor.
	static const FLinearColor Text(1.f, 0.78f, 0.47f);
	static const FLinearColor Dim(0.62f, 0.5f, 0.36f);
	static const FLinearColor Shade(0.f, 0.f, 0.f, 0.72f);
	static const FLinearColor Panel(0.035f, 0.03f, 0.025f, 0.96f);

	static FSlateFontInfo Font(int32 Size, const char* Face = "Regular")
	{
		return FCoreStyle::GetDefaultFontStyle(Face, Size);
	}

	static const FButtonStyle& Button()
	{
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(FLinearColor(0.09f, 0.075f, 0.06f), 3.f, FLinearColor(0.3f, 0.22f, 0.13f), 1.f))
			.SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.22f, 0.15f, 0.07f), 3.f, Text, 1.f))
			.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.32f, 0.21f, 0.09f), 3.f, Text, 1.f))
			.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(0.05f, 0.045f, 0.04f), 3.f))
			.SetNormalPadding(FMargin(18.f, 9.f))
			.SetPressedPadding(FMargin(18.f, 10.f, 18.f, 8.f));
		return Style;
	}

	static const FSlateBrush* ShadeBrush()
	{
		static const FSlateColorBrush Brush(Shade);
		return &Brush;
	}

	static const FSlateBrush* PanelBrush()
	{
		static const FSlateRoundedBoxBrush Brush(Panel, 6.f, FLinearColor(0.35f, 0.25f, 0.14f), 1.f);
		return &Brush;
	}
}

static FText GameTimeText(double Seconds)
{
	const int32 Minutes = FMath::FloorToInt32(Seconds / 60.0);
	return LyudText::Get(TEXT("ui.menu.game_time"), { FStringFormatArg(Minutes / 60), FStringFormatArg(FString::Printf(TEXT("%02d"), Minutes % 60)) });
}

static FText SlotSubtitle(const FLyudSlotInfo& Info)
{
	return LyudText::Get(TEXT("ui.menu.slot_subtitle"), { FStringFormatArg(Info.SavedAt.ToString(TEXT("%d.%m.%Y %H:%M"))), FStringFormatArg(GameTimeText(Info.GameTimeSeconds).ToString()) });
}

void SLyudGameMenu::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	BuildContent();
}

void SLyudGameMenu::BuildContent()
{
	using namespace LyudMenuStyle;
	BuiltLanguage = LyudText::Language();
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(ShadeBrush())
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(16.f)
		[
			SNew(SBox)
			.WidthOverride(560.f)
			.MaxDesiredHeight(640.f)
			[
				SNew(SBorder)
				.BorderImage(PanelBrush())
				.Padding(FMargin(32.f, 26.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 18.f)
					[
						SNew(STextBlock)
						.Text(LyudText::Get(TEXT("ui.menu.title")))
						.Font(Font(30, "Bold"))
						.ColorAndOpacity(Text)
						.Justification(ETextJustify::Center)
					]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SAssignNew(Switcher, SWidgetSwitcher)
						+ SWidgetSwitcher::Slot()[MakeMain()]
						+ SWidgetSwitcher::Slot()[MakePause()]
						+ SWidgetSwitcher::Slot()[MakeSave()]
						+ SWidgetSwitcher::Slot()[MakeLoad()]
						+ SWidgetSwitcher::Slot()[MakeSettings()]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text_Lambda([this] { return Status; })
						.Font(Font(14))
						.ColorAndOpacity(Dim)
						.Justification(ETextJustify::Center)
						.AutoWrapText(true)
					]
				]
			]
		]
	];
}

ULyudSaveSubsystem* SLyudGameMenu::Saves() const
{
	return Owner.IsValid() ? ULyudSaveSubsystem::Get(Owner->GetWorld()) : nullptr;
}

void SLyudGameMenu::SetStatus(const FString& Text)
{
	Status = FText::FromString(Text);
}

FString SLyudGameMenu::DefaultTitle() const
{
	return LyudText::Get(TEXT("ui.menu.default_title"), { FStringFormatArg(FDateTime::Now().ToString(TEXT("%d.%m %H:%M"))) }).ToString();
}

TSharedRef<SWidget> SLyudGameMenu::MakeButton(const FText& Label, TFunction<void()> OnClick, TAttribute<EVisibility> Visible)
{
	using namespace LyudMenuStyle;
	return SNew(SBox)
		.Padding(0.f, 5.f)
		.Visibility(Visible)
		[
			SNew(SButton)
			.ButtonStyle(&Button())
			.HAlign(HAlign_Center)
			.OnClicked_Lambda([OnClick] { OnClick(); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(Label).Font(Font(20)).ColorAndOpacity(Text)
			]
		];
}

TSharedRef<SWidget> SLyudGameMenu::MakeMain()
{
	TWeakObjectPtr<ULyudGameMenuSubsystem> O = Owner;
	const bool bCanQuit = !PLATFORM_IOS;
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeButton(LyudText::Get(TEXT("ui.menu.continue")), [this]
			{
				const TArray<FLyudSlotInfo> Slots = Saves() ? Saves()->GetSlots() : TArray<FLyudSlotInfo>();
				if (Slots.Num() > 0)
				{
					Saves()->LoadSlot(Slots[0].Slot);
				}
			},
			TAttribute<EVisibility>::CreateLambda([this] { return bHasSaves ? EVisibility::Visible : EVisibility::Collapsed; }))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeButton(LyudText::Get(TEXT("ui.menu.new_game")), [this, O]
			{
				if (bStartup)
				{
					if (O.IsValid()) { O->Close(); }
				}
				else if (Saves())
				{
					Saves()->NewGame();
				}
			})
		]
		+ SVerticalBox::Slot().AutoHeight()[MakeButton(LyudText::Get(TEXT("ui.menu.load")), [this] { Show(EScreen::Load); })]
		+ SVerticalBox::Slot().AutoHeight()[MakeButton(LyudText::Get(TEXT("ui.menu.settings")), [this] { Show(EScreen::Settings); })]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeButton(LyudText::Get(TEXT("ui.menu.quit")), [O]
			{
				if (O.IsValid())
				{
					UKismetSystemLibrary::QuitGame(O->GetWorld(), O->GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
				}
			}, bCanQuit ? EVisibility::Visible : EVisibility::Collapsed)
		];
}

TSharedRef<SWidget> SLyudGameMenu::MakePause()
{
	TWeakObjectPtr<ULyudGameMenuSubsystem> O = Owner;
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[MakeButton(LyudText::Get(TEXT("ui.menu.resume")), [O] { if (O.IsValid()) { O->Close(); } })]
		+ SVerticalBox::Slot().AutoHeight()[MakeButton(LyudText::Get(TEXT("ui.menu.save")), [this] { Show(EScreen::Save); })]
		+ SVerticalBox::Slot().AutoHeight()[MakeButton(LyudText::Get(TEXT("ui.menu.load")), [this] { Show(EScreen::Load); })]
		+ SVerticalBox::Slot().AutoHeight()[MakeButton(LyudText::Get(TEXT("ui.menu.settings")), [this] { Show(EScreen::Settings); })]
		+ SVerticalBox::Slot().AutoHeight()[MakeButton(LyudText::Get(TEXT("ui.menu.main_menu")), [this] { Show(EScreen::Main); })];
}

TSharedRef<SWidget> SLyudGameMenu::MakeSettings()
{
	using namespace LyudMenuStyle;
	// the language: each named in itself, the current one lit
	TSharedRef<SHorizontalBox> Langs = SNew(SHorizontalBox);
	for (const TCHAR* Lang : { TEXT("ru"), TEXT("en"), TEXT("ja") })
	{
		const FString L = Lang;
		const bool bOn = LyudText::Language() == L;
		Langs->AddSlot().FillWidth(1.f).Padding(4.f, 0.f)
		[
			SNew(SButton)
			.ButtonStyle(&Button())
			.HAlign(HAlign_Center)
			.OnClicked_Lambda([this, L]
			{
				LyudSettings::SetLanguage(L);
				// every screen's words again, staying here
				BuildContent();
				Activate(EScreen::Settings);
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(LyudText::Get(TEXT("ui.settings.lang.") + L)).Font(Font(18, bOn ? "Bold" : "Regular")).ColorAndOpacity(bOn ? Text : Dim)
			]
		];
	}
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(STextBlock).Text(LyudText::Get(TEXT("ui.settings.language"))).Font(Font(14)).ColorAndOpacity(Dim)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 14.f)[Langs];

	// the volumes: a slider each, applied and stored as it moves
	TWeakObjectPtr<ULyudGameMenuSubsystem> O = Owner;
	const TCHAR* VolumeNames[] = { TEXT("ui.settings.volume.master"), TEXT("ui.settings.volume.ambience"), TEXT("ui.settings.volume.music"), TEXT("ui.settings.volume.effects") };
	for (int32 I = 0; I < static_cast<int32>(LyudSettings::EVolume::Count); ++I)
	{
		const LyudSettings::EVolume Which = static_cast<LyudSettings::EVolume>(I);
		Box->AddSlot().AutoHeight().Padding(0.f, 3.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.42f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LyudText::Get(VolumeNames[I])).Font(Font(16)).ColorAndOpacity(Text)
			]
			+ SHorizontalBox::Slot().FillWidth(0.46f).VAlign(VAlign_Center)
			[
				SNew(SSlider)
				.Value_Lambda([Which] { return LyudSettings::Volume(Which); })
				.SliderBarColor(Dim)
				.SliderHandleColor(Text)
				.OnValueChanged_Lambda([O, Which](float V) { LyudSettings::SetVolume(O.IsValid() ? O->GetWorld() : nullptr, Which, V); })
			]
			+ SHorizontalBox::Slot().FillWidth(0.12f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(Font(15)).ColorAndOpacity(Dim).Justification(ETextJustify::Right)
				.Text_Lambda([Which] { return FText::AsPercent(LyudSettings::Volume(Which)); })
			]
		];
	}

	// the HUD on or off
	Box->AddSlot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
	[
		SNew(SButton)
		.ButtonStyle(&Button())
		.HAlign(HAlign_Center)
		.OnClicked_Lambda([] { LyudSettings::SetHudShown(!LyudSettings::HudShown()); return FReply::Handled(); })
		[
			SNew(STextBlock).Font(Font(18)).ColorAndOpacity(Text)
			.Text_Lambda([] { return LyudText::Get(LyudSettings::HudShown() ? TEXT("ui.settings.hud_on") : TEXT("ui.settings.hud_off")); })
		]
	];
	Box->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)[MakeButton(LyudText::Get(TEXT("ui.menu.back")), [this] { Back(); })];
	return Box;
}

TSharedRef<SWidget> SLyudGameMenu::MakeSave()
{
	using namespace LyudMenuStyle;
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(STextBlock).Text(LyudText::Get(TEXT("ui.menu.save_name"))).Font(Font(14)).ColorAndOpacity(Dim)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SAssignNew(TitleBox, SEditableTextBox)
			.Font(Font(18))
			.SelectAllTextWhenFocused(true)
			.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type Commit)
			{
				if (Commit == ETextCommit::OnEnter && Saves())
				{
					const FString Title = TitleBox->GetText().ToString().TrimStartAndEnd();
					const FString Slot = Saves()->SaveToSlot(Title.IsEmpty() ? DefaultTitle() : Title);
					SetStatus(Slot.IsEmpty() ? LyudText::Get(TEXT("ui.menu.save_failed")).ToString() : LyudText::Get(TEXT("ui.menu.saved"), { FStringFormatArg(Title.IsEmpty() ? DefaultTitle() : Title) }).ToString());
					Back();
				}
			})
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeButton(LyudText::Get(TEXT("ui.menu.save_new")), [this]
			{
				if (!Saves())
				{
					return;
				}
				FString Title = TitleBox->GetText().ToString().TrimStartAndEnd();
				if (Title.IsEmpty())
				{
					Title = DefaultTitle();
				}
				const FString Slot = Saves()->SaveToSlot(Title);
				SetStatus(Slot.IsEmpty() ? LyudText::Get(TEXT("ui.menu.save_failed")).ToString() : LyudText::Get(TEXT("ui.menu.saved"), { FStringFormatArg(Title) }).ToString());
				Back();
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 4.f)
		[
			SNew(STextBlock).Text(LyudText::Get(TEXT("ui.menu.overwrite"))).Font(Font(14)).ColorAndOpacity(Dim)
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(SaveList, SVerticalBox)]
		]
		+ SVerticalBox::Slot().AutoHeight()[MakeButton(LyudText::Get(TEXT("ui.menu.back")), [this] { Back(); })];
}

TSharedRef<SWidget> SLyudGameMenu::MakeLoad()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(LoadList, SVerticalBox)]
		]
		+ SVerticalBox::Slot().AutoHeight()[MakeButton(LyudText::Get(TEXT("ui.menu.back")), [this] { Back(); })];
}

void SLyudGameMenu::RebuildSlots()
{
	using namespace LyudMenuStyle;
	const TArray<FLyudSlotInfo> Slots = Saves() ? Saves()->GetSlots() : TArray<FLyudSlotInfo>();

	for (const bool bSaving : { true, false })
	{
		TSharedPtr<SVerticalBox> List = bSaving ? SaveList : LoadList;
		List->ClearChildren();
		if (Slots.Num() == 0)
		{
			List->AddSlot().AutoHeight().Padding(0.f, 8.f)
			[
				SNew(STextBlock).Text(LyudText::Get(TEXT("ui.menu.no_saves"))).Font(Font(16)).ColorAndOpacity(Dim).Justification(ETextJustify::Center)
			];
		}
		for (const FLyudSlotInfo& Info : Slots)
		{
			const FString Slot = Info.Slot;
			const FString Title = Info.Title;
			List->AddSlot().AutoHeight().Padding(0.f, 4.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SButton)
					.ButtonStyle(&Button())
					.OnClicked_Lambda([this, Slot, Title, bSaving]
					{
						if (Saves())
						{
							if (bSaving)
							{
								const bool bOk = !Saves()->SaveToSlot(Title, Slot).IsEmpty();
								SetStatus(bOk ? LyudText::Get(TEXT("ui.menu.overwritten"), { FStringFormatArg(Title) }).ToString() : LyudText::Get(TEXT("ui.menu.save_failed")).ToString());
								Back();
							}
							else if (!Saves()->LoadSlot(Slot))
							{
								SetStatus(LyudText::Get(TEXT("ui.menu.load_failed"), { FStringFormatArg(Title) }).ToString());
							}
						}
						return FReply::Handled();
					})
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Title)).Font(Font(18)).ColorAndOpacity(Text)]
						+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(SlotSubtitle(Info)).Font(Font(13)).ColorAndOpacity(Dim)]
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.ButtonStyle(&Button())
					.VAlign(VAlign_Center)
					.OnClicked_Lambda([this, Slot]
					{
						if (ArmedDelete != Slot)
						{
							ArmedDelete = Slot;
							return FReply::Handled();
						}
						ArmedDelete.Reset();
						if (Saves())
						{
							Saves()->DeleteSlot(Slot);
						}
						RebuildSlots();
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Text_Lambda([this, Slot] { return ArmedDelete == Slot ? LyudText::Get(TEXT("ui.menu.delete_sure")) : LyudText::Get(TEXT("ui.menu.delete")); })
						.Font(Font(14))
						.ColorAndOpacity(Dim)
					]
				]
			];
		}
	}
}

void SLyudGameMenu::Show(EScreen NewScreen)
{
	History.Add(NewScreen);
	Activate(NewScreen);
}

void SLyudGameMenu::Activate(EScreen NewScreen)
{
	Screen = NewScreen;
	ArmedDelete.Reset();
	if (NewScreen == EScreen::Save || NewScreen == EScreen::Load)
	{
		RebuildSlots();
		Status = FText::GetEmpty();
	}
	if (NewScreen == EScreen::Save)
	{
		TitleBox->SetText(FText::FromString(DefaultTitle()));
	}
	Switcher->SetActiveWidgetIndex(static_cast<int32>(NewScreen));
	if (TSharedPtr<SWidget> Target = GetFocusTarget())
	{
		FSlateApplication::Get().SetAllUserFocus(Target, EFocusCause::SetDirectly);
	}
}

void SLyudGameMenu::ShowRoot(EScreen Root, bool bInStartup)
{
	bStartup = bInStartup;
	if (BuiltLanguage != LyudText::Language())
	{
		BuildContent();
	}
	bHasSaves = Saves() && Saves()->GetSlots().Num() > 0;
	Status = FText::GetEmpty();
	History.Reset();
	Show(Root);
}

bool SLyudGameMenu::Back()
{
	if (History.Num() <= 1)
	{
		return false;
	}
	History.Pop();
	Activate(History.Last());
	return true;
}

TSharedPtr<SWidget> SLyudGameMenu::GetFocusTarget() const
{
	switch (Screen)
	{
	case EScreen::Save: return TitleBox;
	default: return SharedThis(const_cast<SLyudGameMenu*>(this));
	}
}

