#pragma once

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

/** The look of the game screen and the HUD: warm lamp light on dark painted metal, like the corridor and the game menu. */
namespace LyudUI
{
	inline const FLinearColor Text(1.f, 0.78f, 0.47f);
	inline const FLinearColor Bright(1.f, 0.92f, 0.78f);
	inline const FLinearColor Body(0.86f, 0.82f, 0.74f);
	inline const FLinearColor Dim(0.62f, 0.5f, 0.36f);
	inline const FLinearColor Faint(0.42f, 0.36f, 0.28f);
	inline const FLinearColor Good(0.62f, 0.86f, 0.55f);
	inline const FLinearColor Bad(0.86f, 0.36f, 0.26f);
	inline const FLinearColor Panel(0.035f, 0.03f, 0.025f, 0.96f);
	inline const FLinearColor Cell(0.075f, 0.064f, 0.052f, 1.f);

	inline FSlateFontInfo Font(int32 Size, const char* Face = "Regular")
	{
		return FCoreStyle::GetDefaultFontStyle(Face, Size);
	}

	inline const FSlateBrush* ShadeBrush()
	{
		static const FSlateColorBrush Brush(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		return &Brush;
	}

	inline const FSlateBrush* PanelBrush()
	{
		static const FSlateRoundedBoxBrush Brush(Panel, 6.f, FLinearColor(0.35f, 0.25f, 0.14f), 1.f);
		return &Brush;
	}

	inline const FSlateBrush* CellBrush(bool bSelected = false)
	{
		static const FSlateRoundedBoxBrush Normal(Cell, 4.f, FLinearColor(0.22f, 0.16f, 0.09f), 1.f);
		static const FSlateRoundedBoxBrush Selected(FLinearColor(0.2f, 0.13f, 0.06f), 4.f, Text, 1.5f);
		return bSelected ? &Selected : &Normal;
	}

	inline const FSlateBrush* HudBrush()
	{
		static const FSlateRoundedBoxBrush Brush(FLinearColor(0.02f, 0.017f, 0.014f, 0.62f), 4.f);
		return &Brush;
	}

	inline const FButtonStyle& Button()
	{
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(FLinearColor(0.09f, 0.075f, 0.06f), 3.f, FLinearColor(0.3f, 0.22f, 0.13f), 1.f))
			.SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.22f, 0.15f, 0.07f), 3.f, Text, 1.f))
			.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.32f, 0.21f, 0.09f), 3.f, Text, 1.f))
			.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(0.05f, 0.045f, 0.04f), 3.f))
			.SetNormalPadding(FMargin(14.f, 7.f))
			.SetPressedPadding(FMargin(14.f, 8.f, 14.f, 6.f));
		return Style;
	}

	/** A cell, list row or tab: no frame of its own (the content draws the frame). */
	inline const FButtonStyle& FlatButton()
	{
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateNoResource())
			.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1.f, 0.78f, 0.47f, 0.07f), 4.f))
			.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1.f, 0.78f, 0.47f, 0.14f), 4.f))
			.SetNormalPadding(FMargin(0.f))
			.SetPressedPadding(FMargin(0.f));
		return Style;
	}
}
