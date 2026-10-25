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

	/** How ALyudDoor is built (Doors/LyudDoor.h): a lining inside the wall hole, the leaf in it flush with the face it swings to.
	 *  The open leaf stands across the opening by its thickness, so a hole is clear width + lining on both sides + leaf per leaf. */
	constexpr float DoorLeafThickness = 4.f;
	constexpr float DoorLiningThickness = 3.f;

	/** Standard doorways (clear sizes, leaves open). Builders cut holes from them (tools/character_standard.py door_hole). */
	constexpr float StandardDoorClearWidth = 110.f;
	constexpr float StandardDoorClearHeight = 215.f;
	constexpr float WideDoorClearWidth = 130.f;
	constexpr float DoubleDoorClearWidth = 150.f;
	constexpr float DoubleDoorClearHeight = 230.f;
}
