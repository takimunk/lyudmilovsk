#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * Where the player bird can land when it is pulled to a perch (hold Q, "the landing magnet"): the crowns of the
 * forests' and the placed trees' (their outer twigs and tops, which have no collision, so the bird holds on there),
 * and any walkable static surface the traces find ahead: roofs, wall tops, rails, the ground. Picks the best one
 * in front of the bird, preferring something above the ground.
 */
class FBirdPerches
{
public:
	struct FPerch
	{
		FVector Point = FVector::ZeroVector;  // where the feet go, cm
		bool bBranch = false;                 // a twig in a crown (hold on there: no collision under it)
	};

	/** The best perch within MaxDist (cm) of From, roughly the way Dir looks; false if there is none. */
	bool Find(UWorld* World, const FVector& From, const FVector& Dir, float MaxDist, const AActor* Ignore, FPerch& Out);

private:
	void BuildCrowns(UWorld* World);

	struct FCrown
	{
		FVector3f Centre;   // cm
		FVector2f Radii;    // horizontal, vertical, cm
	};
	TMap<FIntPoint, TArray<FCrown>> Grid;   // 20 m cells
	bool bBuilt = false;
};
