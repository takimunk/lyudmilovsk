#pragma once

#include "CoreMinimal.h"

/**
 * What the player bird looks like up close, from its own eye (BirdSelfBody): the paint of each group of
 * feathers, the beak, the legs, the shape of wing and tail. From the field guides, per species of
 * Content/Birds/species.json; autumn plumage (the game is in October), adult male where they differ.
 */
namespace BirdLook
{
	/** One feather's paint: Base with Accent at the tip, the root, the fringe and as bars; a pale spot
	 *  near the tip; an iridescent sheen (Hue 0 green .. 1 purple-blue). The feather material reads these
	 *  as per-instance custom data 0..12 in this order. */
	struct FPaint
	{
		FLinearColor Base = FLinearColor(0.3f, 0.25f, 0.2f);
		FLinearColor Accent = FLinearColor(0.1f, 0.08f, 0.06f);
		float Tip = 0.f;     // fraction of the length from the tip in Accent
		float Root = 0.f;    // fraction from the root in Accent
		float Fringe = 0.f;  // width of an Accent edge round the vane, 0..0.5
		float Bars = 0.f;    // dark Accent bars across the vane (count)
		float Spot = 0.f;    // pale spot near the tip, size 0..0.5
		float Gloss = 0.f;
		float Hue = 0.5f;
		static constexpr int32 NumFloats = 13;
		void Write(float* Out) const;
	};

	/** A group of feathers, with the outermost OuterCount painted differently (gull wing tips, white tail sides). */
	struct FGroup
	{
		FPaint Paint;
		int32 OuterCount = 0;
		FPaint Outer;
		const FPaint& For(int32 Index, int32 Count) const { return Index >= Count - OuterCount ? Outer : Paint; }
	};

	enum class EWing : uint8 { Round, Pointed, Sickle, Broad };
	enum class ETail : uint8 { Square, Notch, Fork, Streamers, Wedge, Long, Rounded };

	struct FLook
	{
		FGroup Primaries, Secondaries, Tertials, GreaterCoverts, LesserCoverts, PrimaryCoverts, Scapulars, Tail, Breast, Back;
		FLinearColor Beak = FLinearColor(0.05f, 0.05f, 0.05f);
		FLinearColor BeakBase = FLinearColor(0.05f, 0.05f, 0.05f);
		FLinearColor Legs = FLinearColor(0.3f, 0.2f, 0.18f);
		/** Beak length (cm, tip to the feathers) and depth at the base over its length. */
		float BeakCm = 1.2f;
		float BeakDepth = 0.45f;
		/** Downward curve of the upper mandible, 0 straight .. 1 a falcon's hook. */
		float BeakHook = 0.f;
		EWing Wing = EWing::Round;
		ETail TailShape = ETail::Square;
		/** Tail length over body length (beak to tail tip includes it). */
		float TailFrac = 0.35f;
	};

	/** The look for a species id; an unknown id gets a plain brown bird. */
	const FLook& Get(FName SpeciesId);
}
