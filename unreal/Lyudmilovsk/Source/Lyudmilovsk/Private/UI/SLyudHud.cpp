#include "UI/SLyudHud.h"

#include "Engine/Texture2D.h"
#include "LyudText.h"
#include "Plants/GameCalendar.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "UI/LyudUIStyle.h"
#include "UI/LyudUIText.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

using namespace LyudUI;

namespace
{
	const double ToastLife = 6.0;
	const double QuestToastLife = 8.0;
	const int32 MaxToasts = 4;
}

void SLyudHud::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	Language = LyudText::Language();
	SetVisibility(EVisibility::HitTestInvisible);
	MaskBrush = MakeShared<FSlateBrush>();
	if (ULyudGameScreen* O = Owner.Get())
	{
		if (UTexture2D* Tex = O->BinocularsMask())
		{
			MaskBrush->SetResourceObject(Tex);
		}
	}
	TWeakObjectPtr<ULyudGameScreen> WeakOwner = Owner;

	ChildSlot
	[
		SNew(SOverlay)
		// the binoculars: two round lenses, dark around them
		+ SOverlay::Slot()
		[
			SNew(SImage).Image(MaskBrush.Get())
			.Visibility_Lambda([WeakOwner]() { return WeakOwner.IsValid() && WeakOwner->BinocularsAmount() > 0.01f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.ColorAndOpacity_Lambda([WeakOwner]() { return FLinearColor(1.f, 1.f, 1.f, WeakOwner.IsValid() ? FMath::SmoothStep(0.f, 1.f, WeakOwner->BinocularsAmount()) : 0.f); })
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0, 22, 26, 0))
		[
			SNew(SBox).WidthOverride(360.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(SBorder).BorderImage(HudBrush()).Padding(FMargin(12, 5))
					[
						SNew(STextBlock).Font(Font(16)).ColorAndOpacity(Bright).ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.6f))
						.Text_Lambda([WeakOwner]() { return LyudUIText::DateTime(UGameCalendar::NowIn(WeakOwner.IsValid() ? WeakOwner->GetWorld() : nullptr)); })
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0, 8, 0, 0)[SAssignNew(QuestBox, SVerticalBox)]
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(26, 22, 0, 0))
		[
			SNew(SBox).WidthOverride(460.f)[SAssignNew(ToastBox, SVerticalBox)]
		]
	];
	RefreshQuest();
}

void SLyudHud::RefreshQuest()
{
	QuestBox->ClearChildren();
	const ULyudQuests* Q = ULyudQuests::Get(Owner.Get());
	const FName Quest = Q ? Q->ShownQuest() : FName();
	if (Quest.IsNone())
	{
		return;
	}
	TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
	Lines->AddSlot().AutoHeight().HAlign(HAlign_Right)
	[
		SNew(STextBlock).Text(ULyudQuests::QuestTitle(Quest)).Font(Font(15, "Bold")).ColorAndOpacity(Text).Justification(ETextJustify::Right)
	];
	for (const TPair<const FLyudQuestObjective*, int32>& O : Q->CurrentObjectives(Quest))
	{
		const bool bMet = O.Value >= O.Key->Need;
		Lines->AddSlot().AutoHeight().HAlign(HAlign_Right).Padding(0, 2, 0, 0)
		[
			SNew(STextBlock).Text(ULyudQuests::ObjectiveText(Quest, *O.Key, O.Value)).Font(Font(14)).AutoWrapText(true).Justification(ETextJustify::Right)
			.ColorAndOpacity(bMet ? Faint : Body)
		];
	}
	QuestBox->AddSlot().AutoHeight().HAlign(HAlign_Right)[SNew(SBorder).BorderImage(HudBrush()).Padding(FMargin(12, 7))[Lines]];
}

void SLyudHud::Toast(const FText& T, bool bQuest)
{
	Toasts.Add({ T, bQuest, FPlatformTime::Seconds() });
	while (Toasts.Num() > MaxToasts)
	{
		Toasts.RemoveAt(0);
	}
	bToastsDirty = true;
}

void SLyudHud::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	const double Now = FPlatformTime::Seconds();
	bToastsDirty |= Toasts.RemoveAll([Now](const FToast& T) { return Now - T.Born > (T.bQuest ? QuestToastLife : ToastLife); }) > 0;
	if (Language != LyudText::Language())
	{
		Language = LyudText::Language();
		RefreshQuest();
	}
	if (!bToastsDirty)
	{
		return;
	}
	bToastsDirty = false;
	ToastBox->ClearChildren();
	for (const FToast& T : Toasts)
	{
		const double Born = T.Born;
		const double Life = T.bQuest ? QuestToastLife : ToastLife;
		const FLinearColor Color = T.bQuest ? Text : Body;
		// fade in over 0.3 s, out over the last second
		auto Fade = [Born, Life]() { const double A = FPlatformTime::Seconds() - Born;
			return FLinearColor(1, 1, 1, FMath::Clamp(FMath::Min(A / 0.3, (Life - A) / 1.0), 0.0, 1.0)); };
		ToastBox->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0, 0, 0, 6)
		[
			SNew(SBorder).BorderImage(HudBrush()).Padding(FMargin(12, 6))
			.ColorAndOpacity_Lambda(Fade).BorderBackgroundColor_Lambda([Fade]() { return FSlateColor(Fade()); })
			[
				SNew(STextBlock).Text(T.Text).Font(Font(T.bQuest ? 16 : 15, T.bQuest ? "Bold" : "Regular")).ColorAndOpacity(Color).AutoWrapText(true)
			]
		];
	}
}
