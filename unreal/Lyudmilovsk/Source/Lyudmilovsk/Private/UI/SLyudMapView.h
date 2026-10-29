#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class ULyudGameScreen;
struct FSlateBrush;

/**
 * The world map: the painted picture (tools/ui_map.py), the names of the places (found ones bright, the rest a faint dot), the
 * markers of the quests' next steps (the followed quest's larger), the player as an arrow, a scale bar. The wheel zooms at the
 * cursor, dragging moves it, C (the game screen) brings it back to the player. In the diary (bInk) it is drawn by hand: the ink
 * map on the page's own paper, the names in the hero's handwriting, the next steps circled in red pen, the hero a red arrow.
 */
class SLyudMapView : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudMapView) : _Owner(nullptr), _bInk(false) {}
		SLATE_ARGUMENT(ULyudGameScreen*, Owner)
		/** The diary's map: pen and pencil on the page's paper (tools/ui_map_ink.py), the names in the hero's hand. */
		SLATE_ARGUMENT(bool, bInk)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SLyudMapView() override;

	/** Map metres to put in the middle; Zoom pixels per metre (0 keeps it). */
	void CenterOn(const FVector2D& At, float Zoom = 0.f);
	void CenterOnPlayer();
	FVector2D GetCenter() const { return Center; }
	float GetZoom() const { return Zoom; }
	/** Called when the reader moves or zooms the map. */
	TFunction<void(const FVector2D&, float)> OnMoved;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(900.f, 560.f); }
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	FVector2D ToScreen(const FVector2D& Map, const FVector2D& Size) const;
	FVector2D ToMap(const FVector2D& Screen, const FVector2D& Size) const;
	float MinZoom(const FVector2D& Size) const;

	TWeakObjectPtr<ULyudGameScreen> Owner;
	TSharedPtr<FSlateBrush> MapBrush;
	FVector2D Center = FVector2D::ZeroVector;
	float Zoom = 1.f;
	bool bDragging = false;
	bool bInk = false;
	FVector2D LastSize = FVector2D(900.f, 560.f);
};
