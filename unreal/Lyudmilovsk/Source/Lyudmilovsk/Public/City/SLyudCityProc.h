#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

class SBox;
class STextBlock;

/** What a procedure's gauge draws (docs/sanatorium_procedures.md): nothing, a cup filling to its mark, a clock's hand with the
 *  sips (or steps) taken, a ring that breathes, a bar of presses. */
enum class ELyudProcGauge : uint8
{
	None,
	Cup,
	Clock,
	Ring,
	Bar,
};

/**
 * The gauge of a procedure's step. It only paints what SetGauge gave it: no layout, no allocation, nothing rebuilt per frame
 * (volatile like the mini-game's bar, so the level and the hand are painted every frame).
 */
class SLyudCityProcGauge : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudCityProcGauge) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Level 0..1 (the cup's water, the ring's size, the hand's turn, the bar); the mark (a cup's band); Done/Of the dots under
	 *  a clock; bHeld: E is held (the ring glows). */
	void SetGauge(ELyudProcGauge InKind, float InLevel, float InMarkLo, float InMarkHi, int32 InDone, int32 InOf, bool bInHeld);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(520.f, 150.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Culling, FSlateWindowElementList& Out,
		int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

protected:
	virtual bool ComputeVolatility() const override { return true; }

private:
	ELyudProcGauge Kind = ELyudProcGauge::None;
	float Level = 0.f;
	float MarkLo = 0.f;
	float MarkHi = 0.f;
	int32 Done = 0;
	int32 Of = 0;
	bool bHeld = false;
	/** The points of the line being drawn (kept between frames so painting never allocates). */
	mutable TArray<FVector2f> Pts;
};

/**
 * A procedure's panel, in the style of the mini-game's (SLyudCityGame): the title, the line of the step (what the nurse says,
 * what happens), the gauge of a hands-on step, the three options of a pick, a status, a word on how the last press went, and the
 * keys. It takes no input and is never hit-tested (ULyudCityThings polls the keys without consuming them). Made once and kept
 * (collapsed between procedures); the texts are set only when they change.
 */
class SLyudCityProc : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudCityProc) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetTitle(const FText& Title);
	void SetLine(const FText& Line);
	void SetStatus(const FText& Status);
	void SetHint(const FText& Hint);
	void SetKeys(const FText& Keys);
	/** The options shown (a pick) or hidden (empty texts). */
	void SetOption(int32 Index, const FText& Text);
	void SetGauge(ELyudProcGauge Kind, float Level, float MarkLo, float MarkHi, int32 Done, int32 Of, bool bHeld);

private:
	TSharedPtr<STextBlock> TitleText;
	TSharedPtr<STextBlock> LineText;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<STextBlock> HintText;
	TSharedPtr<STextBlock> KeysText;
	TSharedPtr<STextBlock> OptionText[3];
	TSharedPtr<SBox> GaugeBox;
	TSharedPtr<SLyudCityProcGauge> Gauge;
};
