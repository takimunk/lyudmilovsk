#include "City/SLyudCityProc.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/LyudUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --- the gauge

void SLyudCityProcGauge::Construct(const FArguments&)
{
	SetVisibility(EVisibility::HitTestInvisible);
}

void SLyudCityProcGauge::SetGauge(ELyudProcGauge InKind, float InLevel, float InMarkLo, float InMarkHi, int32 InDone, int32 InOf, bool bInHeld)
{
	Kind = InKind;
	Level = FMath::Clamp(InLevel, 0.f, 1.f);
	MarkLo = FMath::Clamp(InMarkLo, 0.f, 1.f);
	MarkHi = FMath::Clamp(InMarkHi, MarkLo, 1.f);
	Done = FMath::Max(0, InDone);
	Of = FMath::Clamp(InOf, 0, 24);
	bHeld = bInHeld;
}

int32 SLyudCityProcGauge::OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer,
	const FWidgetStyle&, bool) const
{
	if (Kind == ELyudProcGauge::None)
	{
		return Layer;
	}
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FVector2f Size = FVector2f(Geo.GetLocalSize());
	auto Box = [&](int32 L, float X0, float Y0, float W, float H, const FLinearColor& Col)
	{
		if (W <= 0.f || H <= 0.f)
		{
			return;
		}
		FSlateDrawElement::MakeBox(Out, L, Geo.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X0, Y0))), White,
			ESlateDrawEffect::None, Col);
	};
	// a circle as a closed line of 48 points, in the widget's own array (sized once: no allocation per frame)
	auto Circle = [&](int32 L, const FVector2f& C, float R, const FLinearColor& Col, float Thick)
	{
		Pts.SetNumUninitialized(49, EAllowShrinking::No);
		for (int32 I = 0; I <= 48; ++I)
		{
			const float A = 2.f * PI * (float)I / 48.f;
			Pts[I] = C + FVector2f(FMath::Cos(A), FMath::Sin(A)) * R;
		}
		FSlateDrawElement::MakeLines(Out, L, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Col, true, Thick);
	};
	const FLinearColor Track(0.f, 0.f, 0.f, 0.45f);
	const FLinearColor Warm(LyudUI::Dim.R, LyudUI::Dim.G, LyudUI::Dim.B, 0.85f);
	const FLinearColor Light(LyudUI::Text.R, LyudUI::Text.G, LyudUI::Text.B, 0.95f);
	const FVector2f Mid(Size.X * 0.5f, Size.Y * 0.5f);
	switch (Kind)
	{
	case ELyudProcGauge::Cup:
	{
		// a glass: its body, the mark band, the water up to the level
		const float W = 70.f, H = Size.Y - 10.f, X0 = Mid.X - W * 0.5f, Y0 = 5.f;
		Box(Layer, X0, Y0, W, H, Track);
		Box(Layer + 1, X0, Y0 + H * (1.f - MarkHi), W, H * (MarkHi - MarkLo), Warm);
		Box(Layer + 2, X0 + 4.f, Y0 + H * (1.f - Level), W - 8.f, H * Level, FLinearColor(0.55f, 0.75f, 0.85f, 0.85f));
		Box(Layer + 3, X0 - 6.f, Y0 + H * (1.f - MarkHi), 6.f, 2.f, Light);
		Box(Layer + 3, X0 + W, Y0 + H * (1.f - MarkLo), 6.f, 2.f, Light);
		return Layer + 3;
	}
	case ELyudProcGauge::Clock:
	{
		// a clock face, its hand turning once a beat, and a dot for every beat met
		const float R = FMath::Min(Size.Y * 0.5f - 18.f, 60.f);
		const FVector2f C(Mid.X, Mid.Y - 8.f);
		Circle(Layer, C, R, Light, 2.f);
		const float A = 2.f * PI * Level - PI * 0.5f;
		Pts.SetNumUninitialized(2, EAllowShrinking::No);
		Pts[0] = C;
		Pts[1] = C + FVector2f(FMath::Cos(A), FMath::Sin(A)) * (R - 6.f);
		FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, LyudUI::Bright, true, 3.f);
		Box(Layer + 1, C.X - 2.f, C.Y - R - 2.f, 4.f, 10.f, Warm);     // twelve o'clock: the beat
		const float Dot = 10.f, Gap = 6.f, RowW = Of * Dot + FMath::Max(0, Of - 1) * Gap;
		for (int32 I = 0; I < Of; ++I)
		{
			Box(Layer + 1, Mid.X - RowW * 0.5f + I * (Dot + Gap), Size.Y - Dot - 2.f, Dot, Dot, I < Done ? LyudUI::Bright : Track);
		}
		return Layer + 1;
	}
	case ELyudProcGauge::Ring:
	{
		// the ring to breathe with, and a faint one at its fullest
		const float RMax = FMath::Min(Size.Y * 0.5f - 6.f, 70.f), RMin = RMax * 0.25f;
		Circle(Layer, Mid, RMax, Track, 2.f);
		Circle(Layer + 1, Mid, RMin + (RMax - RMin) * Level, bHeld ? LyudUI::Bright : Light, bHeld ? 5.f : 3.f);
		return Layer + 1;
	}
	case ELyudProcGauge::Bar:
	{
		const float H = 14.f, Y0 = Mid.Y - H * 0.5f;
		Box(Layer, 0.f, Y0, Size.X, H, Track);
		Box(Layer + 1, 0.f, Y0, Size.X * Level, H, LyudUI::Bright);
		return Layer + 1;
	}
	default:
		return Layer;
	}
}

// --- the panel

void SLyudCityProc::Construct(const FArguments&)
{
	SetVisibility(EVisibility::HitTestInvisible);
	TSharedRef<SVerticalBox> Options = SNew(SVerticalBox);
	for (int32 I = 0; I < 3; ++I)
	{
		Options->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f))
		[
			SAssignNew(OptionText[I], STextBlock).Font(LyudUI::Font(15)).ColorAndOpacity(LyudUI::Bright).Visibility(EVisibility::Collapsed)
		];
	}
	ChildSlot
	[
		SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 0.f, 140.f))
		[
			SNew(SBox).WidthOverride(680.f)
			[
				SNew(SBorder)
				.BorderImage(LyudUI::PanelBrush())
				.Padding(FMargin(26.f, 18.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))
					[
						SAssignNew(TitleText, STextBlock).Font(LyudUI::Font(18, "Bold")).ColorAndOpacity(LyudUI::Text)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
					[
						SAssignNew(LineText, STextBlock).Font(LyudUI::Font(16)).ColorAndOpacity(LyudUI::Body).AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SAssignNew(GaugeBox, SBox).WidthOverride(560.f).HeightOverride(150.f).Visibility(EVisibility::Collapsed)
						[
							SAssignNew(Gauge, SLyudCityProcGauge)
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						Options
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 0.f))
					[
						SAssignNew(StatusText, STextBlock).Font(LyudUI::Font(14)).ColorAndOpacity(LyudUI::Body)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))
					[
						SAssignNew(HintText, STextBlock).Font(LyudUI::Font(15)).ColorAndOpacity(LyudUI::Bright)
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(FMargin(0.f, 10.f, 0.f, 0.f))
					[
						SAssignNew(KeysText, STextBlock).Font(LyudUI::Font(13)).ColorAndOpacity(LyudUI::Dim)
					]
				]
			]
		]
	];
}

void SLyudCityProc::SetTitle(const FText& Title)
{
	TitleText->SetText(Title);
}

void SLyudCityProc::SetLine(const FText& Line)
{
	LineText->SetText(Line);
}

void SLyudCityProc::SetStatus(const FText& Status)
{
	StatusText->SetText(Status);
	StatusText->SetVisibility(Status.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
}

void SLyudCityProc::SetHint(const FText& Hint)
{
	HintText->SetText(Hint);
	HintText->SetVisibility(Hint.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
}

void SLyudCityProc::SetKeys(const FText& Keys)
{
	KeysText->SetText(Keys);
}

void SLyudCityProc::SetOption(int32 Index, const FText& Text)
{
	if (Index >= 0 && Index < 3 && OptionText[Index].IsValid())
	{
		OptionText[Index]->SetText(Text);
		OptionText[Index]->SetVisibility(Text.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
	}
}

void SLyudCityProc::SetGauge(ELyudProcGauge Kind, float Level, float MarkLo, float MarkHi, int32 Done, int32 Of, bool bHeld)
{
	GaugeBox->SetVisibility(Kind == ELyudProcGauge::None ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
	Gauge->SetGauge(Kind, Level, MarkLo, MarkHi, Done, Of, bHeld);
}
