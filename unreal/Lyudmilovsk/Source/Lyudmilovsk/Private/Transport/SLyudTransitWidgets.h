#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SCompoundWidget.h"

class ULyudTransportSubsystem;

/** The cable-car metro's words and pictures in Slate: drawn by the boards in the world (UWidgetComponent) and the
 *  full-screen route diagram. All of them read the running network through LyudTransit::Active(). */
namespace LyudTransit
{
	ULyudTransportSubsystem* Active();
	void SetActive(ULyudTransportSubsystem* T);
	/** "2 мин", "40 с", "прибывает" for a wait in seconds. */
	FText WaitText(float Seconds);
}

DECLARE_DELEGATE_OneParam(FOnTransitPick, int32);

/**
 * The route diagram: the two lines in their colours over a paper ground, the stations (changes as double rings), the
 * names, the depots, and live: every cabin on the move as a dot on its line. bBoard: the enamel board by the club's
 * exit (cream, "you are here" at Aton by the club). Interactive (the full screen): hover a station to see the wait,
 * click to pick it (OnPick).
 */
class SLyudTransitDiagram : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudTransitDiagram) : _bLive(true), _bBoard(false), _From(-1) {}
		SLATE_ARGUMENT(bool, bLive)
		SLATE_ARGUMENT(bool, bBoard)
		SLATE_ARGUMENT(int32, From)
		SLATE_EVENT(FOnTransitPick, OnPick)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1600.f, 900.f); }
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override { Hover = -1; }

	void SetFrom(int32 Station) { From = Station; }
	int32 GetHover() const { return Hover; }

private:
	bool bLive = true, bBoard = false;
	int32 From = -1;
	int32 Hover = -1;
	FOnTransitPick OnPick;
	FVector2D ToScreen(const FVector2D& Schema, const FVector2D& Size) const;
	int32 StationUnder(const FVector2D& Local, const FVector2D& Size) const;
	EActiveTimerReturnType Repaint(double, float);
};

/** A station's board on its platform: the line, and for each way the terminal and how long till the next cabin. */
class SLyudStationBoard : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudStationBoard) : _Station(-1), _Line(-1) {}
		SLATE_ARGUMENT(int32, Station)
		SLATE_ARGUMENT(int32, Line)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(900.f, 360.f); }

private:
	int32 Station = -1, Line = -1;
	EActiveTimerReturnType Repaint(double, float);
};

/** A station's name sign: the name big, the badges of its lines. */
class SLyudStationSign : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudStationSign) : _Station(-1) {}
		SLATE_ARGUMENT(int32, Station)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) { Station = InArgs._Station; }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1000.f, 260.f); }

private:
	int32 Station = -1;
};

/** The small board inside a cabin over its door: where it goes and the next stop. */
class SLyudCabinBoard : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudCabinBoard) : _Cabin(-1) {}
		SLATE_ARGUMENT(int32, Cabin)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(600.f, 150.f); }

private:
	int32 Cabin = -1;
	EActiveTimerReturnType Repaint(double, float);
};

/** A cabin's number and name, painted on its side. */
class SLyudCabinPlate : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudCabinPlate) : _Number(0), _Color(FLinearColor::White) {}
		SLATE_ARGUMENT(int32, Number)
		SLATE_ARGUMENT(FText, Name)
		SLATE_ARGUMENT(FLinearColor, Color)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) { Number = InArgs._Number; Name = InArgs._Name; Color = InArgs._Color; }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(560.f, 160.f); }

private:
	int32 Number = 0;
	FText Name;
	FLinearColor Color;
};

/** The full screen: the diagram to pick where to go from a station's call panel (or to look at, from the club's
 *  board), the wait once a cabin is called; Esc closes it. */
class SLyudTransitScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudTransitScreen) : _From(-1) {}
		SLATE_ARGUMENT(int32, From)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	int32 From = -1;
	int32 Picked = -1;
	TSharedPtr<SLyudTransitDiagram> Diagram;
	void OnPick(int32 Station);
	FText Footer() const;
};
