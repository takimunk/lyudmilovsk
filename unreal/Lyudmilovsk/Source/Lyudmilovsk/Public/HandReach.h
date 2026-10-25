#pragma once

#include "CoreMinimal.h"

struct FCompactPose;

/** One solid piece of an item: a sphere, a capsule (along its local Z) or a box, posed by Xf (no scale), cm. */
struct FReachPrim
{
	enum class EKind : uint8 { Sphere, Capsule, Box };
	EKind Kind = EKind::Sphere;
	FTransform Xf;
	float Radius = 4.f;
	float HalfLength = 0.f;
	FVector Extent = FVector(4.f);

	float Distance(const FVector& P) const;
};

/**
 * What a hand closes on: the item's own solid shape, any item, as the union of a few primitives (taken
 * from its collision, or its bounds when it has none). The palm finds its real surface and each finger
 * folds until its skin touches it, so a bottle, an apple, a box or a book are held as they are shaped.
 */
struct FReachShape
{
	TArray<FReachPrim, TInlineAllocator<4>> Prims;

	bool IsEmpty() const { return Prims.Num() == 0; }
	/** Distance from P to the surface (negative inside). */
	float Distance(const FVector& P) const;
	/** Outward surface normal near P (the distance's gradient). */
	FVector Normal(const FVector& P) const;
	/** Where a ray from Start toward Target first meets the surface (sphere tracing); false if it misses. */
	bool Trace(const FVector& Start, const FVector& Target, FVector& OutHit) const;
	FReachShape Transformed(const FTransform& T) const;
	FBox Bounds() const;

	void AddCapsule(const FVector& Center, const FVector& Axis, float HalfLength, float Radius);
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

	/** A deep crouch on the pose as it is (standing or walking): the hips go down Drop cm and back, the back leans
	 *  forward, the legs bend so each foot stays where the animation put it, flat. Alpha 0 leaves the pose alone. */
	void ApplyCrouch(FCompactPose& Pose, float Alpha, float Drop);

	/** The right hand's palm in the pose as it is now (the animation's own): what a reach blends in from and out to. */
	FPalmFrame CurrentPalm(const FCompactPose& Pose);
}
