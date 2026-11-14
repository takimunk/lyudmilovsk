#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SBorder;
class SBox;
class STextBlock;

/**
 * A page of text from a thing in a building (a shelf's book, a notice on a board, a plaque): a title, the text and the key line
 * ("E — next" / "E — close"), low in the middle of the screen over the game, in the HUD's warm style. It takes no input and is
 * never hit-tested: the hero keeps walking and looking while it is up (ULyudCityThings turns and shuts it).
 *
 * The screen style (SetScreen: a film, a play, the TV, a filmstrip) is the same widget dark and wide like a cinema's subtitles:
 * a small title, one subtitle at a time, centred.
 */
class SLyudCityPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudCityPage) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Shows Title (empty: no title line), Body and the key line Keys. */
	void SetPage(const FText& Title, const FText& Body, const FText& Keys);

	/** The screen style (true) or the page style (false); kept until changed. */
	void SetScreen(bool bScreen);

private:
	TSharedPtr<SBox> Outer;
	TSharedPtr<SBox> WidthBox;
	TSharedPtr<SBorder> Frame;
	TSharedPtr<STextBlock> TitleText;
	TSharedPtr<STextBlock> BodyText;
	TSharedPtr<STextBlock> KeysText;
	bool bScreenStyle = false;
};
