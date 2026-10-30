#pragma once

// The F7 panel's small Slate pieces, shared by its world, weather and tree sections.

#include "CoreMinimal.h"
#include "LyudText.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

class APlantDevTool;

namespace PlantDevUI
{
	inline FSlateFontInfo Font(int32 Size = 11) { return FCoreStyle::GetDefaultFontStyle("Regular", Size); }
	inline const FLinearColor Heading(1.f, 0.85f, 0.5f);

	inline TSharedRef<SWidget> Label(const FText& Text, float Width)
	{
		return SNew(SBox).WidthOverride(Width).VAlign(VAlign_Center)[SNew(STextBlock).Text(Text).Font(Font()).ColorAndOpacity(FLinearColor::White)];
	}

	/** A button; bLit (when given) tints it while that choice is on. */
	inline TSharedRef<SWidget> Button(const FText& Text, TFunction<void()> OnClick, TFunction<bool()> bLit = nullptr, int32 Size = 11)
	{
		return SNew(SButton).IsFocusable(false).OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			.ButtonColorAndOpacity_Lambda([bLit]() { return bLit && bLit() ? FLinearColor(1.f, 0.7f, 0.2f) : FLinearColor::White; })
			[SNew(STextBlock).Text(Text).Font(Font(Size))];
	}

	/** Label, slider and its value as text. */
	inline TSharedRef<SWidget> SliderRow(const FText& Name, float Min, float Max, TFunction<float()> Get, TFunction<void(float)> Set, TFunction<FText()> Show)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Label(Name, 190.f)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SSlider).IsFocusable(false).MinValue(Min).MaxValue(Max)
				.Value_Lambda([Get]() { return Get(); })
				.OnValueChanged_Lambda([Set](float V) { Set(V); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(110.f).VAlign(VAlign_Center).Padding(FMargin(8.f, 0.f, 0.f, 0.f))
				[SNew(STextBlock).Text_Lambda([Show]() { return Show(); }).Font(Font()).ColorAndOpacity(FLinearColor::White)]
			];
	}

	/** A section's title that folds it: "▾ Weather" / "▸ Weather". */
	inline TSharedRef<SWidget> SectionHeader(const FString& TitleKey, bool* bOpen, TWeakObjectPtr<UObject> Owner)
	{
		const FText Title = LyudText::Get(TitleKey);
		return SNew(SButton).IsFocusable(false).ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.OnClicked_Lambda([bOpen, Owner]() { if (Owner.IsValid()) *bOpen = !*bOpen; return FReply::Handled(); })
			[
				SNew(STextBlock).Font(Font(12)).ColorAndOpacity(Heading).Text_Lambda([bOpen, Owner, Title]()
					{ return LyudText::Get(Owner.IsValid() && *bOpen ? TEXT("ui.dev.open") : TEXT("ui.dev.closed"), { FStringFormatArg(Title.ToString()) }); })
			];
	}
}

namespace PlantDevWeather
{
	/** The weather section's body: what the weather is now, presets, weather types, phenomena, values, Auto. */
	TSharedRef<SWidget> Build(APlantDevTool* Tool);
	/** Hands the weather back to the simulation: every preset and forced value the panel can set is cleared. */
	void Auto(UWorld* World);
	/** One line for the logs: the weather now and the panel's forces. */
	FString Report(UWorld* World);
}
