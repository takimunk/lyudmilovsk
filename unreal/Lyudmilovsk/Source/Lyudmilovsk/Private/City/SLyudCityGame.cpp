#include "City/SLyudCityGame.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/LyudUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --- the bar

void SLyudCityGameBar::Construct(const FArguments&)
{
	SetVisibility(EVisibility::HitTestInvisible);
}

void SLyudCityGameBar::SetBar(float InMarker, float InZoneFrom, float InZoneTo, float InInnerFrom, float InInnerTo, bool bInBall)
{
	Marker = FMath::Clamp(InMarker, 0.f, 1.f);
	ZoneFrom = FMath::Clamp(InZoneFrom, 0.f, 1.f);
	ZoneTo = FMath::Clamp(InZoneTo, ZoneFrom, 1.f);
	InnerFrom = FMath::Clamp(InInnerFrom, 0.f, 1.f);
	InnerTo = FMath::Clamp(InInnerTo, InnerFrom, 1.f);
	bBall = bInBall;
}

int32 SLyudCityGameBar::OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer,
	const FWidgetStyle&, bool) const
{
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FVector2D Size = FVector2D(Geo.GetLocalSize());
	const float TrackH = 12.f;
	const float TrackY = (float)(Size.Y - TrackH) * 0.5f;
	const float W = (float)Size.X;
	auto Box = [&](int32 L, float X0, float Y0, float BoxW, float BoxH, const FLinearColor& Col)
	{
		if (BoxW <= 0.f || BoxH <= 0.f)
		{
			return;
		}
		FSlateDrawElement::MakeBox(Out, L, Geo.ToPaintGeometry(FVector2f(BoxW, BoxH), FSlateLayoutTransform(FVector2f(X0, Y0))), White,
			ESlateDrawEffect::None, Col);
	};
	// the track, the zone (warm), its middle (bright), then the marker: an upright line (aim) or a ball (rally)
	Box(Layer, 0.f, TrackY, W, TrackH, FLinearColor(0.f, 0.f, 0.f, 0.45f));
	Box(Layer + 1, ZoneFrom * W, TrackY, (ZoneTo - ZoneFrom) * W, TrackH, FLinearColor(LyudUI::Dim.R, LyudUI::Dim.G, LyudUI::Dim.B, 0.85f));
	if (InnerTo > InnerFrom)
	{
		Box(Layer + 2, InnerFrom * W, TrackY, (InnerTo - InnerFrom) * W, TrackH, FLinearColor(LyudUI::Text.R, LyudUI::Text.G, LyudUI::Text.B, 0.95f));
	}
	if (bBall)
	{
		const float D = 14.f;
		Box(Layer + 3, FMath::Clamp(Marker * W - D * 0.5f, 0.f, W - D), (float)(Size.Y - D) * 0.5f, D, D, LyudUI::Bright);
	}
	else
	{
		const float LineW = 4.f;
		Box(Layer + 3, FMath::Clamp(Marker * W - LineW * 0.5f, 0.f, W - LineW), 0.f, LineW, (float)Size.Y, LyudUI::Bright);
	}
	return Layer + 3;
}

// --- the panel

void SLyudCityGame::Construct(const FArguments&)
{
	SetVisibility(EVisibility::HitTestInvisible);
	TSharedRef<SVerticalBox> Moves = SNew(SVerticalBox);
	for (int32 I = 0; I < 3; ++I)
	{
		Moves->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f))
		[
			SAssignNew(MoveText[I], STextBlock).Font(LyudUI::Font(15)).ColorAndOpacity(LyudUI::Bright)
		];
	}
	ChildSlot
	[
		SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 0.f, 160.f))
		[
			SNew(SBox).WidthOverride(640.f)
			[
				SNew(SBorder)
				.BorderImage(LyudUI::PanelBrush())
				.Padding(FMargin(26.f, 18.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 4.f))
					[
						SAssignNew(TitleText, STextBlock).Font(LyudUI::Font(18, "Bold")).ColorAndOpacity(LyudUI::Text)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
					[
						SAssignNew(StatusText, STextBlock).Font(LyudUI::Font(15)).ColorAndOpacity(LyudUI::Body)
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SAssignNew(BarBox, SBox).WidthOverride(560.f).HeightOverride(30.f)
						[
							SAssignNew(Bar, SLyudCityGameBar)
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(MovesBox, SBox)
						[
							Moves
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 0.f))
					[
						SAssignNew(HintText, STextBlock).Font(LyudUI::Font(15)).ColorAndOpacity(LyudUI::Body).AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(FMargin(0.f, 12.f, 0.f, 0.f))
					[
						SAssignNew(KeysText, STextBlock).Font(LyudUI::Font(13)).ColorAndOpacity(LyudUI::Dim)
					]
				]
			]
		]
	];
	SetMode(false);
}

void SLyudCityGame::SetTitle(const FText& Title)
{
	TitleText->SetText(Title);
}

void SLyudCityGame::SetStatus(const FText& Status)
{
	StatusText->SetText(Status);
}

void SLyudCityGame::SetHint(const FText& Hint)
{
	HintText->SetText(Hint);
	HintText->SetVisibility(Hint.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
}

void SLyudCityGame::SetKeys(const FText& Keys)
{
	KeysText->SetText(Keys);
}

void SLyudCityGame::SetMode(bool bDuel)
{
	BarBox->SetVisibility(bDuel ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
	MovesBox->SetVisibility(bDuel ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
}

void SLyudCityGame::SetMove(int32 Index, const FText& Text)
{
	if (Index >= 0 && Index < 3 && MoveText[Index].IsValid())
	{
		MoveText[Index]->SetText(Text);
	}
}

void SLyudCityGame::SetBar(float Marker, float ZoneFrom, float ZoneTo, float InnerFrom, float InnerTo, bool bBall)
{
	Bar->SetBar(Marker, ZoneFrom, ZoneTo, InnerFrom, InnerTo, bBall);
}
