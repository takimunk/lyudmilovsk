#pragma once

#include "CoreMinimal.h"
#include "Books/BookLayout.h"
#include "Books/LyudBooks.h"
#include "Books/ReadingModel.h"
#include "Fonts/SlateFontInfo.h"
#include "Tasks/Task.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Views/SListView.h"

/**
 * An open library book: two pages on a desk-lamp-lit spread (one page on a narrow window), drawn line by line from the
 * layout BookLayout.h makes on a worker. Clicks on the right half and the wheel down turn forward; the screen
 * (ULyudBookScreen) takes them with TakeAskedTurns so the reading time and the bookmark are kept in one place.
 */
class SLyudBookReader : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudBookReader) {}
		SLATE_ARGUMENT(FLyudBookInfo, Book)
		SLATE_ARGUMENT(int32, Bookmark)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SLyudBookReader() override;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(800.f, 600.f); }
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return false; }

	const FString& BookKey() const { return Book.Key; }
	const FString& Lang() const { return Book.Lang; }
	bool IsLaidOut() const { return Layout.IsValid(); }
	/** The character offset of the first page shown (the bookmark). */
	int32 Offset() const;
	/** Turns by spreads; returns the characters of the pages newly shown when turning forward (the reading time), else 0. */
	int32 Turn(int32 Spreads);
	/** On the last spread (the credits page in view). */
	bool AtEnd() const;
	void GoToOffset(int32 Offset);
	void StepFont(int32 Step);
	void ToggleContents();
	bool ContentsShown() const { return bContents; }
	void ScrollContents(int32 Rows);
	int32 TakeAskedTurns() { const int32 T = AskedTurns; AskedTurns = 0; return T; }

	// --- quotes (docs/books.md "Quotes"): C picks a sentence of the spread, W/S another, E copies it into the diary
	/** Into the quote mode on the spread shown; false when it has no sentence to offer. */
	bool EnterQuote();
	void LeaveQuote() { bQuote = false; }
	bool InQuote() const { return bQuote; }
	void QuoteStep(int32 By);
	/** The chosen sentence: its clean text for the diary and where it starts. False: none. */
	bool ChosenQuote(FString& OutText, int32& OutOffset) const;
	/** How bright the page is drawn (the dusk outdoors): 0.35..1. */
	void SetBright(float B) { Bright = FMath::Clamp(B, 0.35f, 1.f); }

private:
	struct FLoaded
	{
		FString Text;
		TSharedPtr<std::u16string, ESPMode::ThreadSafe> U16;
		bool bOk = false;
	};

	FLyudBookInfo Book;
	FSimpleDelegate OnClose;
	/** The text as loaded (drawn from with MakeText ranges: no string is made per frame). */
	FString Text;
	TSharedPtr<std::u16string, ESPMode::ThreadSafe> U16;
	TSharedPtr<LyudBooks::FLayout, ESPMode::ThreadSafe> Layout;
	UE::Tasks::TTask<FLoaded> LoadTask;
	UE::Tasks::TTask<TSharedPtr<LyudBooks::FLayout, ESPMode::ThreadSafe>> LayoutTask;
	bool bLoading = true;
	bool bLayingOut = false;
	bool bFailed = false;

	/** What the current layout was made for. */
	FVector2f LaidFor = FVector2f::ZeroVector;
	int32 LaidFontIndex = -1;
	/** The size the layout is wanted for (the allotted geometry, read in Tick). */
	FVector2f Allotted = FVector2f::ZeroVector;
	int32 FontIndex = 2;
	/** First page shown (even on a spread): -1 until the first layout places the bookmark. */
	int32 PageIndex = -1;
	int32 WantOffset = 0;
	bool bContents = false;
	int32 ContentsTop = 0;
	int32 AskedTurns = 0;
	bool bQuote = false;
	std::vector<LyudReading::FSpan> QuoteSpans;
	int32 QuoteIx = 0;
	float Bright = 1.f;
	FString QuoteKeysText;
	void PaintQuote(int32 Page, const FGeometry& G, FVector2f PageOrigin, FSlateWindowElementList& Out, int32 Layer) const;

	// geometry of the spread for the current Allotted (computed in Tick, used in OnPaint and the mouse)
	struct FSpread
	{
		bool bTwo = true;
		FVector2f Origin = FVector2f::ZeroVector;
		FVector2f PageSize = FVector2f(400.f, 600.f);
		FVector2f TextOffset = FVector2f(40.f, 60.f);
		FVector2f TextSize = FVector2f(320.f, 480.f);
		float LineH = 24.f;
		int32 Rows = 20;
	};
	FSpread Spread;
	FSlateFontInfo BodyFont;
	FSlateFontInfo HeadFont;
	FSlateFontInfo SmallFont;

	// words drawn every frame, made once (no FText formatted per frame)
	FString LoadingText;
	FString MissingText;
	FString KeysText;
	FString ContentsTitle;
	FString CreditsEnd;
	FString CreditsLicense;
	FString CreditsSource;
	/** The running heads and the page numbers of the spread shown, made again when it changes. */
	FString HeadLeft;
	FString HeadRight;
	FString Folio[2];
	int32 FolioFor = -2;
	void UpdateFolio();

	static float FontSizeAt(int32 Index);
	void ComputeSpread();
	void StartLayout();
	int32 PagesPerSpread() const { return Spread.bTwo ? 2 : 1; }
	/** Pages of the text plus the credits page at the end. */
	int32 NumPages() const;
	float LineWidth(int32 Start, int32 Len, bool bHead) const;
	void PaintPage(int32 Page, const FGeometry& G, FVector2f PageOrigin, FSlateWindowElementList& Out, int32 Layer, bool bLeft) const;
	void PaintCredits(const FGeometry& G, FVector2f TextOrigin, FSlateWindowElementList& Out, int32 Layer) const;
	void PaintContents(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 ContentsRows() const;
	FString RunningHead(int32 Page) const;
};

/** The lending desk's catalogue (or his own books away from the desk): search, language, take, give back, read. */
class SLyudBookCatalog : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnRead, const FString&);

	SLATE_BEGIN_ARGS(SLyudBookCatalog) : _Books(nullptr), _AtDesk(true) {}
		SLATE_ARGUMENT(ULyudBooks*, Books)
		SLATE_ARGUMENT(bool, AtDesk)
		SLATE_EVENT(FOnRead, OnRead)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TWeakObjectPtr<ULyudBooks> Books;
	bool bAtDesk = true;
	FOnRead OnRead;
	FSimpleDelegate OnClose;
	FString Filter;
	FString LangFilter;
	TArray<TSharedPtr<FString>> Rows;
	TSharedPtr<SListView<TSharedPtr<FString>>> List;
	TSharedPtr<class STextBlock> Status;
	/** The librarian's line at the desk (ULyudReading::Advice), or what a "recommend to a friend" just did. */
	FText Note;

	void Refill();
	TSharedRef<ITableRow> MakeRow(TSharedPtr<FString> Key, const TSharedRef<STableViewBase>& Owner);
	TSharedRef<SWidget> LangButton(const FString& Lang, const FText& Label);
	FText StatusText() const;
};
