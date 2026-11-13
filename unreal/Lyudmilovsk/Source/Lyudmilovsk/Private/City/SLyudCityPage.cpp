#include "City/SLyudCityPage.h"

#include "UI/LyudUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SLyudCityPage::Construct(const FArguments&)
{
	SetVisibility(EVisibility::HitTestInvisible);
	ChildSlot
	[
		SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 0.f, 180.f))
		[
			SNew(SBox).WidthOverride(640.f)
			[
				SNew(SBorder)
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
