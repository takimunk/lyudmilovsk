#include "City/SLyudCityBook.h"

#include "UI/LyudUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace LyudCityBookStyle
{
	/** Paper (a notice, a newspaper): a warm off-white sheet with a thin brown edge (a rounded box brush: no texture). */
	const FSlateBrush* PaperBrush()
	{
		static const FSlateRoundedBoxBrush Brush(FLinearColor(0.80f, 0.75f, 0.64f, 0.97f), 2.f, FLinearColor(0.38f, 0.28f, 0.16f), 1.f);
		return &Brush;
	}

	const FLinearColor Ink(0.13f, 0.10f, 0.07f);
	const FLinearColor InkDim(0.36f, 0.28f, 0.19f);
}

void SLyudCityBook::Construct(const FArguments&)
{
	SetVisibility(EVisibility::Collapsed);
	ChildSlot
	[
		SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 60.f))
		[
			SNew(SBox).WidthOverride(720.f).MinDesiredHeight(380.f)
			[
				SAssignNew(Frame, SBorder)
				.BorderImage(LyudUI::PanelBrush())
				.Padding(FMargin(40.f, 28.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 4.f))
					[
						SAssignNew(KindText, STextBlock).Font(LyudUI::Font(12)).ColorAndOpacity(LyudUI::Dim)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 16.f))
					[
						SAssignNew(TitleText, STextBlock).Font(LyudUI::Font(22, "Bold")).ColorAndOpacity(LyudUI::Text).AutoWrapText(true)
					]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SAssignNew(BodyText, STextBlock).Font(LyudUI::Font(17)).ColorAndOpacity(LyudUI::Body).AutoWrapText(true).LineHeightPercentage(1.15f)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 18.f, 0.f, 0.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
						[
							SAssignNew(CounterText, STextBlock).Font(LyudUI::Font(13)).ColorAndOpacity(LyudUI::Dim)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(24.f, 0.f, 0.f, 0.f))
						[
							SAssignNew(KeysText, STextBlock).Font(LyudUI::Font(13)).ColorAndOpacity(LyudUI::Dim).AutoWrapText(true).Justification(ETextJustify::Right)
						]
					]
				]
			]
		]
	];
}

void SLyudCityBook::SetKind(const FString& Kind, const FText& KindLabel)
{
	KindText->SetText(KindLabel);
	const bool bWantPaper = Kind == TEXT("notice") || Kind == TEXT("newspaper");
	if (bWantPaper == bPaper)
	{
		return;
	}
	bPaper = bWantPaper;
	Frame->SetBorderImage(bPaper ? LyudCityBookStyle::PaperBrush() : LyudUI::PanelBrush());
	KindText->SetColorAndOpacity(bPaper ? LyudCityBookStyle::InkDim : LyudUI::Dim);
	TitleText->SetColorAndOpacity(bPaper ? LyudCityBookStyle::Ink : LyudUI::Text);
	BodyText->SetColorAndOpacity(bPaper ? LyudCityBookStyle::Ink : LyudUI::Body);
	CounterText->SetColorAndOpacity(bPaper ? LyudCityBookStyle::InkDim : LyudUI::Dim);
	KeysText->SetColorAndOpacity(bPaper ? LyudCityBookStyle::InkDim : LyudUI::Dim);
}

void SLyudCityBook::SetPage(const FText& Title, const FText& Body, const FText& Counter, const FText& Keys)
{
	TitleText->SetText(Title);
	BodyText->SetText(Body);
	CounterText->SetText(Counter);
	KeysText->SetText(Keys);
}
