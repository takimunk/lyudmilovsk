#pragma once

#include "CoreMinimal.h"

struct FCompactPose;

/**
 * Where a body on (or beside) a bicycle puts itself, in the mesh component space of that body (cm; the people's and
 * the hero's meshes face +Y, their left is +X, up is +Z). The bike's side (ALyudBike::RiderPose) fills it every frame
 * on the game thread from the bike's parts: the saddle, the pedals as the cranks turn, the hoods as the bars steer;
 * the animation thread bends the body onto them (LyudBikeRider::ApplyBody before the look, ApplyArms after it).
 * The same targets serve the hero (HeroAnimInstance) and the residents (LyudBodyAnim), whatever their size.
 */
struct FBikeRiderPose
{
	/** 0 the animation's own body .. 1 fully on the targets. */
	float Weight = 0.f;
	/** The bike's axes in component space: forward, up, its left. */
	FVector Fwd = FVector::YAxisVector;
	FVector Up = FVector::ZAxisVector;
	FVector Left = FVector::XAxisVector;

	/** The pelvis bone's place and how much the hips are taken there (0 standing on its own legs beside the bike). */
	FVector Pelvis = FVector::ZeroVector;
	float PelvisWeight = 0.f;
	/** The trunk's lean forward from the bike's up (deg); 0 = upright. When AutoLean the lean is the least that brings
	 *  the shoulders within reach of the hands' targets (riding on the hoods), never under TorsoPitch. */
	float TorsoPitch = 0.f;
	bool bAutoLean = false;
	/** Sideways tilt of the trunk toward the bike's left (deg): into a turn, onto the leg standing on the ground. */
	float TorsoRoll = 0.f;
	/** The head kept looking ahead over the bars (share of the trunk's lean the neck and head take back, 0..1). */
	float HeadUp = 0.f;

	struct FFoot
	{
		/** Where the ball of the foot goes (on a pedal's top, on the ground) and the way the toes point; Weight 0 = the
		 *  animation's own foot. Pole: the way the knee bends out. */
		FVector Ball = FVector::ZeroVector;
		FVector Toes = FVector::YAxisVector;
		FVector Pole = FVector::YAxisVector;
		float Weight = 0.f;
	};
	FFoot Feet[2];     // left, right

	struct FHand
	{
		/** A bar or a hood the hand closes round: a capsule along Axis through Grip (its radius), and the palm's way
		 *  onto it: PalmNormal points out of the palm toward the bar, FingerDir from the wrist to the knuckles. */
		FVector Grip = FVector::ZeroVector;
		FVector Axis = FVector::XAxisVector;
		float Radius = 1.6f;
		FVector PalmNormal = -FVector::ZAxisVector;
		FVector FingerDir = FVector::YAxisVector;
		float Weight = 0.f;
	};
	FHand Hands[2];    // left, right

	bool IsActive() const { return Weight > 0.001f; }
};

namespace LyudBikeRider
{
	/** The hips onto the saddle (or wherever Pelvis is), the trunk leaned, the legs bent so each foot's ball is on its
	 *  target. Call on the clip's pose, before any look or head turn. */
	void ApplyBody(FCompactPose& Pose, const FBikeRiderPose& R);
	/** The arms to the hands' targets, each hand turned onto its bar and its fingers closed round it (they stop where
	 *  their skin meets the bar). Call last. */
	void ApplyArms(FCompactPose& Pose, const FBikeRiderPose& R);
}
