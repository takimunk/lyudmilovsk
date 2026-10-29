#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"

class UTexture2D;
struct FGeometry;
class FSlateWindowElementList;

/**
 * The look of the diary (docs/diary.md): cream dot-grid paper, a blue-black ballpoint for what the hero wrote, a soft
 * graphite pencil for notes and lines, a red pen for the map's marks, a yellow highlighter for what is chosen; his handwriting
 * is Caveat (ru, en) or Yomogi (ja), both SIL OFL, imported as font faces under /Game/UI/Diary/Fonts.
 *
 * Every hand-drawn line (underlines, circles, boxes, ticks) wobbles by a seed that belongs to what it marks, so it stays the
 * same from frame to frame and from the screen to the picture on the 3D book.
 */
namespace LyudDiary
{
	/** The spread in design units: two A4 pages 840 x 1188 (40 units a centimetre); the tabs stick out TabRoom beyond. */
	inline constexpr float PageW = 840.f;
	inline constexpr float PageH = 1188.f;
	inline constexpr float SpreadW = 2.f * PageW;
	inline constexpr float TabRoom = 64.f;
	inline constexpr float Margin = 64.f;
	inline constexpr float UnitsPerCm = 40.f;

	inline const FLinearColor Ink(0.075f, 0.1f, 0.27f);
	inline const FLinearColor InkSoft(0.16f, 0.2f, 0.4f, 0.9f);
	inline const FLinearColor Pencil(0.27f, 0.26f, 0.29f, 0.92f);
	inline const FLinearColor PencilFaint(0.3f, 0.29f, 0.32f, 0.5f);
	inline const FLinearColor Red(0.66f, 0.14f, 0.09f);
	inline const FLinearColor Green(0.2f, 0.42f, 0.2f);
	inline const FLinearColor Highlight(1.f, 0.88f, 0.22f, 0.4f);
	inline const FLinearColor Paper(0.95f, 0.93f, 0.87f);
	inline const FLinearColor Sticky(1.f, 0.93f, 0.55f);

	enum class ESection : uint8 { Things, Quests, Diary, Map, Num };

	/** The index tabs' card, one colour per section (the same on the 3D book). */
	FLinearColor TabColor(ESection S);

	/** The hero's hand: pen (bold = pressed harder, for titles). Sizes are design units at the spread's scale. */
	FSlateFontInfo Hand(float Size, bool bBold = false);

	/** The paper of a left or right page (/Game/UI/Diary/T_PaperLeft|Right); a plain cream when it is missing. */
	const FSlateBrush* PaperBrush(bool bRight);
	/** The sticky note's paper (the same paper, yellow). */
	const FSlateBrush* StickyBrush();
	/** The pencil sketch of an item (/Game/Inventory/Localized/<lang>/T_Sketch_<id>); null when it has none. */
	const FSlateBrush* SketchBrush(FName Item);
	/** The ink map (/Game/UI/Diary/T_MapInk); null when missing. */
	UTexture2D* MapInk();

	/** Stable 0..1 noise for a seed and a step. */
	float Hash(uint32 Seed, int32 I);

	enum class EMark : uint8 { Underline, DoubleUnderline, Circle, Box, Strike, Highlight, Tick, Cross, Arrow, Bracket };

	/** Draws a hand-made mark over a box (local, design units). */
	void DrawMark(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, EMark Kind, const FVector2D& Pos, const FVector2D& Size,
		const FLinearColor& Color, float Thickness, uint32 Seed);
	/** A wobbly pen line through points (local). */
	void DrawStroke(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const TArray<FVector2D>& Points, const FLinearColor& Color,
		float Thickness, uint32 Seed, float Wobble = 1.2f);
}
