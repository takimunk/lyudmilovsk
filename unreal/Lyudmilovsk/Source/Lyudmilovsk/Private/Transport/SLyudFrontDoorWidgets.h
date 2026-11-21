#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

/**
 * Aton's departures board (docs/front_door.md): a split-flap board on the hall's wall. Every few seconds it asks the rules
 * (LyudFrontDoor::Departures) for the next cabins out of its station on its line and, where a row's words change, its
 * letters flip over one by one through a few others before they settle. Flips() counts the rows that changed, so the hall
 * plays the clack (board_flap) when it grows. Repaints only while flipping (else once a second).
 */
class SLyudFlapBoard : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudFlapBoard) : _Station(-1), _Line(-1) {}
		SLATE_ARGUMENT(int32, Station)
		SLATE_ARGUMENT(int32, Line)
	SLATE_END_ARGS()

	static constexpr int32 Rows = 5;
	static constexpr int32 Cols = 30;

	void Construct(const FArguments& InArgs);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1200.f, 560.f); }

	int32 Flips() const { return FlipCount; }
	bool IsFlipping() const { return bFlipping; }
	/** The rows' words now (tests, the console). */
	FString Describe() const;

private:
	struct FCell
	{
		TCHAR Shown = TEXT(' ');
		TCHAR Target = TEXT(' ');
		int32 Steps = 0;
	};
	int32 Station = -1, Line = -1;
	FCell Cells[Rows][Cols];
	float Refresh = 0.f;
	int32 FlipCount = 0;
	bool bFlipping = false;
	uint32 Spin = 0;

	EActiveTimerReturnType Step(double Now, float Dt);
	void Rebuild();
	void SetRow(int32 Row, const FString& Text);
};

/** The club's sign over the forecourt: the station's name in big letters (Cyrillic in every language, like the club's
 *  own lettering), dull brass by day, warm lit bulbs at dusk. */
class SLyudClubSign : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudClubSign) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&) {}
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1400.f, 360.f); }

	void SetLit(bool bOn) { if (bLit != bOn) { bLit = bOn; Invalidate(EInvalidateWidgetReason::Paint); } }

private:
	bool bLit = false;
};
