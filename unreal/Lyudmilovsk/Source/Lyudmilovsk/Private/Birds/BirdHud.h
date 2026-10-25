#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class ABirdPawn;

/**
 * What the player bird has on screen, all drawn, nothing to click: the controls for the first seconds
 * as a bird, the ring that fills while R is held, a faint ring on the spot a flare will land on, and
 * with lyud.Bird.Debug 1 the flight numbers.
 */
class SBirdHud : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SBirdHud) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, ABirdPawn* InPawn);

	static TSharedPtr<SBirdHud> AddTo(ABirdPawn* Pawn);
	static void Remove(const TSharedPtr<SBirdHud>& Hud);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(64.f, 64.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Culling, FSlateWindowElementList& Out,
	                      int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

protected:
	virtual bool ComputeVolatility() const override { return true; }

private:
	TWeakObjectPtr<ABirdPawn> Pawn;
	TArray<FText> Hints;
	FText Returning, AirPods;
};
