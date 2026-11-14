#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

class SBox;
class STextBlock;

/**
 * The bar of a mini-game (docs/city_interiors.md "Mini-games"): a track, a zone with its bright middle and a marker (aim), or a
 * ball flying along the track toward a window at the hero's end (rally). It only paints what SetBar gave it: no layout, no
 * allocation, nothing rebuilt per frame (volatile like the bird HUD, so the moving marker is painted every frame).
 */
class SLyudCityGameBar : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudCityGameBar) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Positions 0..1 along the track; bBall draws the marker as a ball (rally), else as an upright line (aim). */
	void SetBar(float InMarker, float InZoneFrom, float InZoneTo, float InInnerFrom, float InInnerTo, bool bInBall);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(520.f, 30.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Culling, FSlateWindowElementList& Out,
		int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

protected:
	virtual bool ComputeVolatility() const override { return true; }

private:
	float Marker = 0.f;
	float ZoneFrom = 0.f;
	float ZoneTo = 0.f;
	float InnerFrom = 0.f;
	float InnerTo = 0.f;
	bool bBall = false;
};

/**
 * A mini-game's panel, in the style of the city page (SLyudCityPage): the title (the game and the opponent), the score line,
 * the bar (aim, rally) or the three moves (duel), a line of what just happened (a hit, the opponent's look) and the keys. It takes
 * no input and is never hit-tested (ULyudCityThings polls the keys without consuming them). Made once and kept (collapsed between
 * games); the texts are set only when they change.
 */
class SLyudCityGame : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudCityGame) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetTitle(const FText& Title);
	void SetStatus(const FText& Status);
	/** What just happened (empty: the line is hidden). */
	void SetHint(const FText& Hint);
	void SetKeys(const FText& Keys);
	/** The bar shown (aim, rally) or the three moves (duel). */
	void SetMode(bool bDuel);
	void SetMove(int32 Index, const FText& Text);
	void SetBar(float Marker, float ZoneFrom, float ZoneTo, float InnerFrom, float InnerTo, bool bBall);

private:
	TSharedPtr<STextBlock> TitleText;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<STextBlock> HintText;
	TSharedPtr<STextBlock> KeysText;
	TSharedPtr<STextBlock> MoveText[3];
	TSharedPtr<SBox> BarBox;
	TSharedPtr<SBox> MovesBox;
	TSharedPtr<SLyudCityGameBar> Bar;
};
