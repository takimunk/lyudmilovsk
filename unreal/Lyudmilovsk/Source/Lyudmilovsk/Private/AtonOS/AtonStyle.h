#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

/** Colours, fonts and widget styles of ATON OS: the teal desktop and beige bars of the old screen image. */
namespace AtonStyle
{
	inline FLinearColor C(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }

	inline const FLinearColor Desktop = C(0x27, 0x76, 0x7A);
	inline const FLinearColor DesktopShadow = C(0x0E, 0x45, 0x48);
	inline const FLinearColor Bar = C(0xE8, 0xE4, 0xD8);
	inline const FLinearColor BarDim = C(0xB9, 0xB5, 0xAA);
	inline const FLinearColor Ink = C(0x16, 0x16, 0x16);
	inline const FLinearColor InkSoft = C(0x5A, 0x58, 0x52);
	inline const FLinearColor Paper = C(0xF6, 0xF3, 0xEA);
	inline const FLinearColor PaperLine = C(0xD8, 0xD3, 0xC4);
	inline const FLinearColor Console = C(0x2E, 0x2D, 0x33);
	inline const FLinearColor Green = C(0x9C, 0xF5, 0xAE);
	inline const FLinearColor Link = C(0x1F, 0x5F, 0xA8);
	inline const FLinearColor Select = C(0xC9, 0xDD, 0xD9);
	inline const FLinearColor White = FLinearColor::White;

	inline FSlateFontInfo Font(int32 Size, bool bBold = false)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	inline FSlateFontInfo Mono(int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle("Mono", Size);
	}

	/** A flat white brush; tint it with BorderBackgroundColor / ColorAndOpacity. */
	inline const FSlateBrush* Flat()
	{
		static FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}

	inline const FSlateBrush* None()
	{
		static FSlateNoResource Brush;
		return &Brush;
	}

	/** A flat button: Normal / hovered / pressed fill colours. */
	inline FButtonStyle MakeButton(const FLinearColor& Normal, const FLinearColor& Hovered, const FLinearColor& Pressed)
	{
		FButtonStyle S;
		S.SetNormal(FSlateColorBrush(Normal));
		S.SetHovered(FSlateColorBrush(Hovered));
		S.SetPressed(FSlateColorBrush(Pressed));
		S.SetDisabled(FSlateColorBrush(Normal));
		S.SetNormalPadding(FMargin(0));
		S.SetPressedPadding(FMargin(0));
		return S;
	}

	/** Beige button on paper or bars. */
	inline const FButtonStyle& Button()
	{
		static FButtonStyle S = MakeButton(C(0xDD, 0xD8, 0xCA), C(0xEF, 0xEB, 0xE0), C(0xC4, 0xBF, 0xB0));
		return S;
	}

	/** Invisible until hovered: menu items, list rows, desktop icons. */
	inline const FButtonStyle& Ghost()
	{
		static FButtonStyle S = MakeButton(FLinearColor::Transparent, FLinearColor(1, 1, 1, 0.18f), FLinearColor(0, 0, 0, 0.12f));
		return S;
	}

	inline const FTextBlockStyle& PaperText(int32 Size = 15)
	{
		static TMap<int32, FTextBlockStyle> Cache;
		if (FTextBlockStyle* Found = Cache.Find(Size))
		{
			return *Found;
		}
		FTextBlockStyle S = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
		S.SetFont(Font(Size));
		S.SetColorAndOpacity(Ink);
		S.SetSelectedBackgroundColor(Select);
		return Cache.Add(Size, S);
	}

	inline const FEditableTextStyle& EditText(bool bConsole)
	{
		static FEditableTextStyle PaperStyle = []
		{
			FEditableTextStyle S = FCoreStyle::Get().GetWidgetStyle<FEditableTextStyle>("NormalEditableText");
			S.SetFont(Font(15));
			S.SetColorAndOpacity(Ink);
			S.SetCaretImage(FSlateColorBrush(Ink));
			S.SetBackgroundImageSelected(FSlateColorBrush(Select));
			return S;
		}();
		static FEditableTextStyle ConsoleStyle = []
		{
			FEditableTextStyle S = FCoreStyle::Get().GetWidgetStyle<FEditableTextStyle>("NormalEditableText");
			S.SetFont(Mono(15));
			S.SetColorAndOpacity(Green);
			S.SetCaretImage(FSlateColorBrush(Green));
			S.SetBackgroundImageSelected(FSlateColorBrush(FLinearColor(0.2f, 0.5f, 0.3f)));
			return S;
		}();
		return bConsole ? ConsoleStyle : PaperStyle;
	}
}
