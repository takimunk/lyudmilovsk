#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class ULyudShipPoker;

/** A player at the table as the panel paints him. */
struct FLyudPokerSeatView
{
	FText Name;
	FString Stack;
	/** His bet of this round, or what he did ("folded", "changed 2", "all in"). */
	FString Note;
	/** The name of his hand (the hero's always; a regular's when shown down). */
	FText Hand;
	bool bDealer = false;
	bool bTurn = false;
	bool bDim = false;
	bool bFaces = false;
	bool bWon = false;
	int32 NumCards = 0;
	int32 Cards[5] = { 0, 0, 0, 0, 0 };
};

/** Everything the poker panel paints, made by ULyudShipPoker::Refresh when something happened (never per frame). */
struct FLyudPokerView
{
	/** Goes up with every Refresh: the panel fits the lines to their places again only when it changed. */
	uint32 Serial = 0;
	/** 0: choosing the table; 1: playing; 2: the sitting is over. */
	int32 Stage = 0;
	bool bHelp = false;
	FText Title;
	FString HandNo;
	FString Pot;
	FString Stack;
	FText DealerWord;
	TArray<FLyudPokerSeatView> Seats;
	FLyudPokerSeatView Hero;
	/** At the draw: the card under the cursor (-1: not the draw) and the cards marked to change. */
	int32 Cursor = -1;
	int32 Discard = 0;
	FText Tell;
	FText Banner;
	FText Banner2;
	bool bBannerBad = false;
	TArray<FText> Log;
	/** The lines of the choosing page, of the last page, of the rules. */
	TArray<FText> Lines;
	TArray<FText> Help;
	FText Keys;
	FText Keys2;
};

/**
 * The poker table at the foot of the screen (docs/ship.md "Poker"), in the look of the HUD: the regulars across the top
 * (name, stack, bet, their cards face down or shown), the pot, the line that gives a man away, the hero's five cards drawn as
 * little cards (rank and suit, red and black; the suits are drawn from boxes, no font needs them), a short log and the keys.
 * It only paints ULyudShipPoker::View(): no layout, no input, nothing allocated per frame. A line too long for its place
 * (Japanese names and sums run wide) is set smaller, down to 8 pt, and clipped if that is still not enough; the sizes are
 * measured once per change of the view (FLyudPokerView::Serial), never per frame.
 */
class SLyudPokerPanel : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudPokerPanel) {}
		SLATE_ARGUMENT(ULyudShipPoker*, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1.f, 1.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Culling, FSlateWindowElementList& Out,
		int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

protected:
	virtual bool ComputeVolatility() const override { return true; }

private:
	TWeakObjectPtr<ULyudShipPoker> Owner;
	/** "2" .. "10", "J", "Q", "K", "A" by rank - 2. */
	TArray<FString> Ranks;

	/** The sizes the fitted lines came out at, in the order they are painted (negative: clipped too); kept while the view's
	 *  Serial and the panel's width stay the same. Mutable: filled in OnPaint, on the game thread only. */
	mutable TArray<int32> Fitted;
	mutable int32 FitAt = 0;
	mutable uint32 FitSerial = 0;
	mutable float FitWidth = -1.f;

	/** The size (points) at which S is no wider than MaxWidth; negative when even the smallest is too wide. */
	int32 FitSize(const FString& S, int32 Size, const char* Face, float MaxWidth) const;
	/** A line of text kept inside MaxWidth: smaller when it is long, clipped at the worst. */
	void FitText(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& At, const FString& S, int32 Size,
		const char* Face, const FLinearColor& Color, float MaxWidth) const;
	void FitText(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& At, const FText& S, int32 Size,
		const char* Face, const FLinearColor& Color, float MaxWidth) const;

	void PaintCard(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& At, const FVector2f& Size, int32 Card,
		bool bFace, float Fade) const;
	void PaintSuit(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& Centre, float Size, int32 Suit,
		const FLinearColor& Color) const;
};
