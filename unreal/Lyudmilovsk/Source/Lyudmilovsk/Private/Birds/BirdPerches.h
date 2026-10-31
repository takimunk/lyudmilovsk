#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * Where the player bird can land when it is pulled to a perch (hold Q, "the landing magnet"): a real branch of a
 * tree (the forests' and the placed trees', taken from the same branch skeleton their wood is drawn from: a point on
 * a limb's axis, its direction and radius), and any walkable static surface the traces find ahead: roofs, wall
 * tops, rails, the ground. Never a point in the air: a perch is always on wood or on something with collision.
 */
class FBirdPerches
{
public:
	struct FPerch
	{
		FVector Point = FVector::ZeroVector;  // where the feet go (the top of the branch), cm
		bool bBranch = false;                 // a branch of a tree (no collision: the bird holds on there)
		FVector Axis = FVector::ZeroVector;   // a branch: a point on its axis under the feet, its direction, its radius (cm)
		FVector Dir = FVector::ForwardVector;
		float Radius = 0.f;
	};

	/** The size of the bird that perches: its feet grip branches from MinRadius to MaxRadius (cm) and its body (a
	 *  sphere of BodyRadius, StandHeight above the feet) needs room. */
	void SetBird(float InMinRadius, float InMaxRadius, float InBodyRadius, float InStandHeight)
	{
		MinRadius = InMinRadius; MaxRadius = InMaxRadius; BodyRadius = InBodyRadius; StandHeight = InStandHeight;
	}

	/** The best perch within MaxDist (cm) of From, roughly the way Dir looks; false if there is none. */
	bool Find(UWorld* World, const FVector& From, const FVector& Dir, float MaxDist, const AActor* Ignore, FPerch& Out);

	/** The branches (cm) of every tree within Radius of At, for tests: Fn(A, B, RadiusA, RadiusB). */
	void ForEachBranch(UWorld* World, const FVector& At, float Radius, TFunctionRef<void(const FVector&, const FVector&, float, float)> Fn);

private:
	void BuildCrowns(UWorld* World);

	/** One tree: what its wood is drawn from, and where its crown is (for a quick first look). */
	struct FTree
	{
		FName Species;
		uint32 Seed = 0;
		float Age = 0.f;
		FTransform Xf;
		FVector3f Centre;   // the crown's middle, cm
		FVector2f Radii;    // horizontal, vertical, cm
		bool bPoint = false;   // a cable-car perch: a single point on its rail, checked by a trace
	};
	/** A branch piece in world space, cm: from A to B, radius at each end, its order (0 trunk) and how far along. */
	struct FSeg
	{
		FVector3f A, B;
		float RA, RB;
		uint8 Order;
		float Along;   // 0 at the branch's base .. 1 at its tip (twigs sway toward their tips)
	};
	const TArray<FSeg>& Branches(UWorld* World, const FTree& T);

	TArray<FTree> Trees;
	TMap<FIntPoint, TArray<int32>> Grid;   // 20 m cells -> Trees
	TMap<uint64, TArray<FSeg>> BranchCache;
	bool bBuilt = false;
	float MinRadius = 0.4f, MaxRadius = 3.f, BodyRadius = 5.f, StandHeight = 8.f;
};
