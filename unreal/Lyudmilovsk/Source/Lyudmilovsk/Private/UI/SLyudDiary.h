#pragma once

#include "CoreMinimal.h"
#include "UI/LyudDiaryStyle.h"
#include "UI/LyudGameScreen.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

class SBox;
class SLyudMapView;
class UTextureRenderTarget2D;
class ULyudInventory;
class ULyudInventoryUI;
class FWidgetRenderer;

/** One spread of the diary: which section and which of its spreads (the diary's entries run over several). */
struct FLyudDiarySpread
{
	LyudDiary::ESection Section = LyudDiary::ESection::Things;
	int32 Part = 0;
};

/** What the reader chose, kept across the spreads, the rebuilds and the pictures of the pages. */
struct FLyudDiaryState
{
	FName SelectedItem;
	FName SelectedQuest;
	FVector2D MapCenter = FVector2D::ZeroVector;
	float MapZoom = 1.f;
	bool bMapSet = false;
	/** The diary's entries on each page (indices into ULyudQuests::Entries), oldest first. */
	TArray<TArray<int32>> DiaryPages;
};

/** A hand-drawn mark over whatever it is laid over (an overlay slot): an underline, a circle, a tick... */
class SLyudInkMark : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudInkMark) : _Kind(LyudDiary::EMark::Underline), _Color(LyudDiary::Ink), _Thickness(2.2f), _Seed(1), _Show(true) {}
		SLATE_ARGUMENT(LyudDiary::EMark, Kind)
		SLATE_ARGUMENT(FLinearColor, Color)
		SLATE_ARGUMENT(float, Thickness)
		SLATE_ARGUMENT(uint32, Seed)
		SLATE_ATTRIBUTE(bool, Show)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

private:
	LyudDiary::EMark Kind = LyudDiary::EMark::Underline;
	FLinearColor Color;
	float Thickness = 2.f;
	uint32 Seed = 1;
	TAttribute<bool> Show;
};

/** Something on a page that can be clicked: a word underlined on hover, a sketch boxed in pencil; chosen, it is circled or highlighted. */
class SLyudInkButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudInkButton) : _HoverMark(LyudDiary::EMark::Underline), _SelectedMark(LyudDiary::EMark::Highlight), _Selected(false),
		_Seed(1), _MarkColor(LyudDiary::Ink), _SelectedColor(LyudDiary::Highlight), _Inset(FMargin(0.f)) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_ARGUMENT(LyudDiary::EMark, HoverMark)
		SLATE_ARGUMENT(LyudDiary::EMark, SelectedMark)
		SLATE_ARGUMENT(bool, Selected)
		SLATE_ARGUMENT(uint32, Seed)
		SLATE_ARGUMENT(FLinearColor, MarkColor)
		SLATE_ARGUMENT(FLinearColor, SelectedColor)
		/** The mark goes round the content less this. */
		SLATE_ARGUMENT(FMargin, Inset)
		SLATE_EVENT(FSimpleDelegate, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

private:
	LyudDiary::EMark HoverMark = LyudDiary::EMark::Underline;
	LyudDiary::EMark SelectedMark = LyudDiary::EMark::Highlight;
	bool bSelected = false;
	bool bPressed = false;
	uint32 Seed = 1;
	FLinearColor MarkColor, SelectedColor;
	FMargin Inset;
	FSimpleDelegate OnClicked;
};

/** One spread's two pages with what is written on them; the live one on the screen and the ones drawn into pictures are the same. */
class SLyudDiarySpread : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudDiarySpread) : _Owner(nullptr), _PageNumber(1) {}
		SLATE_ARGUMENT(ULyudGameScreen*, Owner)
		SLATE_ARGUMENT(FLyudDiarySpread, Spread)
		SLATE_ARGUMENT(TSharedPtr<FLyudDiaryState>, State)
		/** The left page's number (the right one's is the next). */
		SLATE_ARGUMENT(int32, PageNumber)
		/** Something chosen on the page: the screen builds the spread again. */
		SLATE_EVENT(FSimpleDelegate, OnChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	TSharedPtr<SLyudMapView> GetMap() const { return Map; }

private:
	TSharedRef<SWidget> Page(bool bRight, TSharedRef<SWidget> Content);
	TSharedRef<SWidget> ThingsLeft();
	TSharedRef<SWidget> ThingsRight();
	TSharedRef<SWidget> QuestsLeft();
	TSharedRef<SWidget> QuestsRight();
	TSharedRef<SWidget> DiaryPage(int32 PageIndex);
	TSharedRef<SWidget> MapSpread();
	TSharedRef<SWidget> MapNote();
	void Changed() { OnChanged.ExecuteIfBound(); }

	TWeakObjectPtr<ULyudGameScreen> Owner;
	TWeakObjectPtr<ULyudInventory> Inventory;
	TWeakObjectPtr<ULyudInventoryUI> InventoryUI;
	FLyudDiarySpread Spread;
	TSharedPtr<FLyudDiaryState> State;
	int32 PageNumber = 1;
	FSimpleDelegate OnChanged;
	TSharedPtr<SLyudMapView> Map;
};

/**
 * A page being turned, from pictures of the two spreads: the sheet is a strip bent round as it goes over (its outer edge
 * leading, the bottom corner most), seen in perspective from the reader's eyes, shaded by how it faces the light; the
 * spread under it shows on either side. Drawn with custom vertices from the render targets.
 */
class SLyudDiaryFlip : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudDiaryFlip) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	/** From (the spread now) and To (the one coming); bForward turns a right page to the left. Sheets: how many go over (a riffle shows up to 3). */
	void Start(UTextureRenderTarget2D* From, UTextureRenderTarget2D* To, bool bForward, int32 Sheets);
	/** 0..1 through the turn. */
	void SetProgress(float T) { Progress = T; }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(LyudDiary::SpreadW, LyudDiary::PageH); }

private:
	void DrawSheet(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, float T, bool bFrontContent, bool bBackContent) const;

	FSlateBrush FromBrush, ToBrush, BlankBrush;
	bool bForward = true;
	int32 Sheets = 1;
	float Progress = 0.f;
};

/**
 * The diary on the screen: the open spread laid exactly over the 3D book's pages (or in the middle when there is no book: a
 * bird has no hands), the index tabs on the fore-edges, the dog-ears to turn a page, the turning page. The game screen
 * (ULyudGameScreen) drives it: where the pages are, how visible it is, which spread.
 */
class SLyudDiaryScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudDiaryScreen) : _Owner(nullptr) {}
		SLATE_ARGUMENT(ULyudGameScreen*, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SLyudDiaryScreen() override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Where the spread's paper is on the viewport, pixels (top left, size); a zero size: in the middle, 84% of the height. */
	void SetPageRect(const FVector2D& Pos, const FVector2D& Size) { RectPos = Pos; RectSize = Size; }
	void SetAlpha(float A);

	/** The spreads again from the inventory and the quests (they changed, or the language did). */
	void Rebuild();
	/** The spread for a tab of the game screen (the diary tab: its newest spread). */
	int32 SpreadFor(ELyudScreenTab Tab) const;
	int32 CurrentSpread() const { return Current; }
	int32 NumSpreads() const { return Spreads.Num(); }
	const FLyudDiarySpread& SpreadAt(int32 I) const { return Spreads[I]; }
	/** Shows a spread at once (no turning). */
	void SetSpread(int32 I);
	/** Turns the pages over to spread I (one sheet, or a riffle for a jump). False if it is already there or a turn runs. */
	bool TurnTo(int32 I);
	bool IsTurning() const { return bTurning; }
	/** How many index tabs lie on the left of the spread shown. */
	int32 TabsOnLeft() const;

	void ShowPlace(FName Place);
	void CenterMapOnPlayer();

	/** Draws a spread's two pages into a render target (the picture on the 3D book, the pages of a turn). */
	void DrawSpread(int32 I, UTextureRenderTarget2D* Target);

	/** The render target size for a spread's picture. */
	static FIntPoint PictureSize();

private:
	TSharedRef<SLyudDiarySpread> MakeSpread(int32 I);
	TSharedRef<SWidget> MakeTabs();
	TSharedRef<SWidget> MakeCorner(bool bRight);
	void Paginate();
	void Refresh();
	FVector2D LocalPos() const;
	float LocalScale() const;

	TWeakObjectPtr<ULyudGameScreen> Owner;
	TSharedPtr<FLyudDiaryState> State;
	TArray<FLyudDiarySpread> Spreads;
	int32 Current = 0;
	TSharedPtr<SBox> LiveBox;
	TSharedPtr<SBox> TabsBox;
	TSharedPtr<SLyudDiarySpread> Live;
	TSharedPtr<SLyudDiaryFlip> Flip;
	TUniquePtr<FWidgetRenderer> Renderer;

	FVector2D RectPos = FVector2D::ZeroVector, RectSize = FVector2D::ZeroVector;
	FVector2D ViewSize = FVector2D(1920, 1080);
	float ViewScale = 1.f;
	bool bTurning = false;
	float TurnTime = 0.f, TurnLength = 0.5f;
	int32 TurnTarget = 0;
	FString Language;
};
