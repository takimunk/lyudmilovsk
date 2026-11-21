#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class ULyudInstruments;

/**
 * The instrument at the foot of the screen (docs/instruments.md): the keys of an octave and a half with their keyboard letters
 * (the piano), or the strings and the eight chords (the guitar); over them, while a tune plays, its notes sliding to the line
 * where E strikes them, the word for the last stroke, and the count-in; the list of tunes he knows (Q) and the stars of the
 * one just finished. It only paints what ULyudInstruments holds: every word is looked up once, in Construct, nothing is
 * allocated per frame.
 */
class SLyudInstrument : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudInstrument) {}
		SLATE_ARGUMENT(ULyudInstruments*, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1.f, 1.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Culling, FSlateWindowElementList& Out,
		int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

protected:
	virtual bool ComputeVolatility() const override { return true; }

private:
	TWeakObjectPtr<ULyudInstruments> Owner;
	bool bGuitar = false;

	// words, looked up once
	FText Name, HintFree, HintTune, HintList, ListTitle, ListEmpty, Perfect, Good, Miss, ByFire;
	TArray<FText> Titles;          // per tune index
	TArray<FString> TuneNumbers;   // "1" .. "9"
	TArray<FString> ChordLabels;   // "1  C" ...
	TArray<FString> KeyLetters;    // per semitone of the keyboard (A W S E D ...)
	TArray<FString> NoteNames;     // per semitone class (C C# D ...)
	TArray<FString> StarStrings;   // 0..3 stars
	FString Count[4];

	void PaintKeys(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& At, const FVector2f& Size,
		const ULyudInstruments& I) const;
	void PaintStrings(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& At, const FVector2f& Size,
		const ULyudInstruments& I) const;
	void PaintLane(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& At, const FVector2f& Size,
		const ULyudInstruments& I) const;
	void PaintList(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& At, const FVector2f& Size,
		const ULyudInstruments& I) const;
};
