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
 *
 * Birds (docs/bird_guide.md): the bird guide's own map (bGuide) is the printed one held round the hero, the live birds within
 * the guide's radius as coloured marks (a hover names them), the land beyond the radius faded out. The diary's map draws the same
 * birds in pencil while the hero has the guide and its layer is on (ULyudBirdGuide::ShowsOnMap).
 */
class SLyudMapView : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudMapView) : _Owner(nullptr), _bInk(false), _bGuide(false) {}
		SLATE_ARGUMENT(ULyudGameScreen*, Owner)
		/** The diary's map: pen and pencil on the page's paper (tools/ui_map_ink.py), the names in the hero's hand. */
		SLATE_ARGUMENT(bool, bInk)
		/** The bird guide's map: always round the hero, the live birds near him (ULyudBirdGuide). */
		SLATE_ARGUMENT(bool, bGuide)
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
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

private:
	FVector2D ToScreen(const FVector2D& Map, const FVector2D& Size) const;
	FVector2D ToMap(const FVector2D& Screen, const FVector2D& Size) const;
	float MinZoom(const FVector2D& Size) const;
	/** The birds near the hero (the guide's radius): their marks, the radius ring, and in the guide the name of the one under the mouse. */
	void DrawBirds(FSlateWindowElementList& Out, int32 LayerId, const FGeometry& G, const FVector2D& Size, const FLinearColor& Fade) const;
	bool ShowsBirds() const;

	TWeakObjectPtr<ULyudGameScreen> Owner;
	TSharedPtr<FSlateBrush> MapBrush;
	FVector2D Center = FVector2D::ZeroVector;
	float Zoom = 1.f;
	bool bDragging = false;
	bool bInk = false;
	bool bGuide = false;
	bool bGuideZoomSet = false;
	/** The mouse over the map (local), for the guide's hover; negative when it is off. */
	FVector2D Mouse = FVector2D(-1.0, -1.0);
	FVector2D LastSize = FVector2D(900.f, 560.f);
};
