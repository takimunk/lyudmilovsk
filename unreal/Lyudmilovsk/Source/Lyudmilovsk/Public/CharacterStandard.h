#pragma once

#include "CoreMinimal.h"

/**
 * Character size standard: every door, passage and gap the player or an NPC must get through is
 * sized from these numbers. Units are Unreal centimetres. Python (the Blender build, the Unreal
 * import scripts) reads this same file through tools/character_standard.py, so edit it only here.
 */
namespace LyudmilovskCharacter
{
	/** The first-person player's collision capsule. */
	constexpr float PlayerCapsuleRadius = 34.f;
	constexpr float PlayerCapsuleHalfHeight = 96.f;

	/** The tallest NPC we ever place (standing, head to floor). */
	constexpr float MaxNpcHeight = 195.f;

	/** Free space kept on each side of the capsule when it passes through a gap. */
	constexpr float PassMargin = 20.f;

	/** Narrowest clear width of any doorway: capsule diameter plus a margin on both sides. With the leaf(s) swung fully open. */
	constexpr float MinDoorClearWidth = 2.f * PlayerCapsuleRadius + 2.f * PassMargin;

	/** Lowest clear height of any doorway: the taller of the capsule and the tallest NPC, plus headroom. */
	constexpr float MinDoorClearHeight = (2.f * PlayerCapsuleHalfHeight > MaxNpcHeight ? 2.f * PlayerCapsuleHalfHeight : MaxNpcHeight) + 15.f;
}
