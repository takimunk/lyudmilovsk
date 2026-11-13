#include "City/SLyudCityPage.h"

#include "UI/LyudUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace LyudCityPageStyle
{
	/** The screen style's panel: nearly black, no frame (no texture: a rounded box brush like the HUD's). */
	const FSlateBrush* ScreenBrush()
	{
		static const FSlateRoundedBoxBrush Brush(FLinearColor(0.008f, 0.008f, 0.01f, 0.86f), 4.f);
		return &Brush;
	}
}

void SLyudCityPage::Construct(const FArguments&)
{
	SetVisibility(EVisibility::HitTestInvisible);
	ChildSlot
	[
		SAssignNew(Outer, SBox).HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 0.f, 180.f))
		[
			SAssignNew(WidthBox, SBox).WidthOverride(640.f)
			[
				SAssignNew(Frame, SBorder)
				.BorderImage(LyudUI::PanelBrush())
				.Padding(FMargin(26.f, 18.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
					[
						SAssignNew(TitleText, STextBlock).Font(LyudUI::Font(18, "Bold")).ColorAndOpacity(LyudUI::Text)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(BodyText, STextBlock).Font(LyudUI::Font(16)).ColorAndOpacity(LyudUI::Body).AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(FMargin(0.f, 12.f, 0.f, 0.f))
					[
						SAssignNew(KeysText, STextBlock).Font(LyudUI::Font(13)).ColorAndOpacity(LyudUI::Dim)
					]
				]
			]
		]
	];
}

void SLyudCityPage::SetPage(const FText& Title, const FText& Body, const FText& Keys)
{
	TitleText->SetText(Title);
	TitleText->SetVisibility(Title.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
	BodyText->SetText(Body);
	KeysText->SetText(Keys);
}

void SLyudCityPage::SetScreen(bool bScreen)
{
	if (bScreen == bScreenStyle)
	{
		return;
	}
	bScreenStyle = bScreen;
	Outer->SetPadding(FMargin(0.f, 0.f, 0.f, bScreen ? 110.f : 180.f));
	WidthBox->SetWidthOverride(bScreen ? 980.f : 640.f);
	Frame->SetBorderImage(bScreen ? LyudCityPageStyle::ScreenBrush() : LyudUI::PanelBrush());
	Frame->SetPadding(bScreen ? FMargin(40.f, 16.f) : FMargin(26.f, 18.f));
	TitleText->SetFont(bScreen ? LyudUI::Font(13, "Bold") : LyudUI::Font(18, "Bold"));
	TitleText->SetColorAndOpacity(bScreen ? LyudUI::Dim : LyudUI::Text);
	TitleText->SetJustification(bScreen ? ETextJustify::Center : ETextJustify::Left);
	BodyText->SetFont(LyudUI::Font(bScreen ? 21 : 16));
	BodyText->SetColorAndOpacity(bScreen ? LyudUI::Bright : LyudUI::Body);
	BodyText->SetJustification(bScreen ? ETextJustify::Center : ETextJustify::Left);
}
