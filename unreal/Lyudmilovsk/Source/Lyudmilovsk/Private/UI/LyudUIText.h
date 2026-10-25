#pragma once

#include "CoreMinimal.h"
#include "LyudText.h"

/** Dates and times on the game screen and the HUD, in the current language (ui.date.*). */
namespace LyudUIText
{
	/** "25 октября" / "October 25" / "10月25日". */
	inline FText Date(const FDateTime& D)
	{
		const FString Month = LyudText::Get(FString::Printf(TEXT("ui.date.month.%d"), D.GetMonth())).ToString();
		return LyudText::Get(TEXT("ui.date.day"), { FStringFormatArg(D.GetDay()), FStringFormatArg(Month) });
	}

	inline FString Clock(const FDateTime& D)
	{
		return FString::Printf(TEXT("%d:%02d"), D.GetHour(), D.GetMinute());
	}

	/** "25 октября, 16:20". */
	inline FText DateTime(const FDateTime& D)
	{
		return LyudText::Get(TEXT("ui.date.datetime"), { FStringFormatArg(Date(D).ToString()), FStringFormatArg(Clock(D)) });
	}

	/** Pads a button's words the way the other screens do. */
	inline FText Padded(const TCHAR* Key)
	{
		return FText::FromString(TEXT("  ") + LyudText::Get(Key).ToString() + TEXT("  "));
	}
}
