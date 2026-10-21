#pragma once

#include "CoreMinimal.h"

struct FCompactPose;

/**
 * What a hand closes on: a capsule (a sphere when HalfLength is 0), in the hero mesh's component space, cm.
 * A bottle is a capsule along its axis, an apple a sphere, a book a flat capsule: the fingers stop on its surface.
 */
struct FReachShape
{
	FVector Center = FVector::ZeroVector;
	FVector Axis = FVector::UpVector;
	float HalfLength = 0.f;
	float Radius = 4.f;
};

/**
 * Where the hero's right hand should be, in the mesh component space (the animation thread reads it).
 * The hand is pulled to Palm with Weight (0 = the animation's own hand); the upper body leans forward
 * (Posture) as far as the arm needs to get there, the hips and feet stay put.
 */
struct FHeroReachPose
{
	float Weight = 0.f;
	/** Palm centre and its frame: FingerDir from the wrist to the knuckles, PalmNormal out of the palm. */
	FVector Palm = FVector::ZeroVector;
	FVector FingerDir = FVector::ForwardVector;
	FVector PalmNormal = FVector::UpVector;
	/** 0 the fingers hang loose, 1 they close until they touch Shape (or all the way, with no shape). */
	float Curl = 0.f;
	bool bHasShape = false;
	FReachShape Shape;
	/** How much the upper body may lean to get the hand there (0 stand still). */
	float PostureWeight = 1.f;

	bool IsActive() const { return Weight > 0.001f; }
};

/** A palm frame in the mesh component space (cm); NormalSign is the calibration of "out of the palm" for this skeleton. */
struct FPalmFrame
{
	FVector Palm = FVector::ZeroVector;
	FVector FingerDir = FVector::ForwardVector;
	FVector PalmNormal = FVector::UpVector;
	float NormalSign = 1.f;
	bool bValid = false;
};

/** The hero's reach, solved on the animation pose (the bone names are the hero's skeleton: UE mannequin style). */
namespace HandReach
{
	/** Bend the upper body forward so the right shoulder can reach Reach.Palm; hips and feet stay put. Call before the look. */
	void ApplyPosture(FCompactPose& Pose, const FHeroReachPose& Reach);

	/** Right arm to the palm, the hand turned to the palm frame, the fingers closed on the shape. Call after the look. */
	void ApplyArm(FCompactPose& Pose, const FHeroReachPose& Reach, const FPalmFrame& AnimPalm);

	/** The right hand's palm in the pose as it is now (the animation's own): what a reach blends in from and out to. */
	FPalmFrame CurrentPalm(const FCompactPose& Pose);
}
