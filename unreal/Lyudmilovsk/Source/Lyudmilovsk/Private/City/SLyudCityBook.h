#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SBorder;
class STextBlock;

/**
 * The reading view (docs/city_interiors.md "The reading page"): a book, a notice, a ledger or a newspaper from a shelf or a
 * board, one page at a time: what it is (a small line), the title, the page's text, "page 2 of 4" and the keys. In the warm
 * style of SLyudCityPage, larger and in the middle of the screen; a notice and a newspaper on paper (light, dark ink).
 *
 * Made once by ULyudCityThings and kept in the viewport, collapsed while no text is open; the texts change only when a page
 * is turned. It takes no input and is never hit-tested (ULyudCityThings polls the keys).
 */
class SLyudCityBook : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudCityBook) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** The look of a kind ("book", "notice", "ledger", "newspaper"): paper for a notice and a newspaper; kept until changed. */
	void SetKind(const FString& Kind, const FText& KindLabel);
	/** One page: the title (every page), its text, the counter line ("page 2 of 4") and the keys. */
	void SetPage(const FText& Title, const FText& Body, const FText& Counter, const FText& Keys);

private:
	TSharedPtr<SBorder> Frame;
	TSharedPtr<STextBlock> KindText;
	TSharedPtr<STextBlock> TitleText;
	TSharedPtr<STextBlock> BodyText;
	TSharedPtr<STextBlock> CounterText;
	TSharedPtr<STextBlock> KeysText;
	bool bPaper = false;
};
