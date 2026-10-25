#pragma once

#include "CoreMinimal.h"
#include "Plants/PlantSpecies.h"
#include "VineSpecies.generated.h"

/**
 * The rules a climbing vine grows by (AProceduralVine). Its leaves, bark, berries and seasons are an
 * ordinary FPlantSpecies (Plant), read by the same PlantModel::Season as the trees; what is new is how
 * the stems find their way: they creep over whatever collision they meet, a step at a time.
 *
 * Units: metres, years, radians per metre, days of the year.
 */
USTRUCT(BlueprintType)
struct FVineSpecies
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vine") FName Name;
	/** Leaves (Leaf = one leaflet; Leaflets of them make a leaf), bark, berries, phenology and colours. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vine") FPlantSpecies Plant;

	// --- Leaves

	/** Leaflets per leaf, spread like fingers (Virginia creeper: 5); 1 = a simple leaf. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") int32 Leaflets = 5;
	/** Leaf stalk, m: the leaves stand off the wall on it, overlapping like shingles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float PetioleLength = 0.1f;
	/** Share of the steps along a stem that carry a cluster of leaves (Plant.Leaf.PerCluster of them). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float LeafChance = 0.9f;

	// --- Growth: each shoot is a chain of Step-long steps over the surface; a shoot of order k grows
	// Speed * SpeedFalloff^k metres a growing season (from GrowStartDay to GrowEndDay), slowing with height.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float Step = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float Speed = 2.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float SpeedFalloff = 0.6f;
	/** Leaders slow down toward this height, m, and stop at it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float MaxHeight = 18.f;
	/** Shoots never get farther than this from the root, m (the plant's reach along the wall). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float MaxReach = 9.f;
	/** Longest a shoot of order 0 / 1 / 2+ gets, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") FVector MaxShootLength = FVector(30.0, 7.0, 2.5);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float GrowStartDay = 140.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float GrowEndDay = 235.f;
	/** Shoots from the root at planting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") int32 RootShoots = 3;
	/** How hard shoots turn toward the sky, 1/m; side shoots by SideUpPull of it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float UpPull = 1.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float SideUpPull = 0.35f;
	/** Random wandering of a shoot, radians per sqrt(m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float Wobble = 0.55f;
	/** The stems keep this far from each other, m: a shoot steers away from wall already taken, and gives up when boxed in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float Spacing = 0.2f;

	// --- Branching

	/** A side shoot every this much stem, m (randomised by half), at BranchAngle off the parent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float BranchSpacing = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float BranchChance = 0.75f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float BranchAngle = 55.f;
	/** A bud waits this long before it shoots, years (it breaks in a later season). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float BudDelay = 0.6f;
	/** Highest order of side shoot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") int32 MaxOrder = 3;

	// --- Free shoots: where there is nothing to hold (a roof edge, the top of a window) a shoot hangs
	// out and down for up to FreeLength, then stops.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Free") float FreeLength = 0.7f;
	/** How fast a free shoot bends down, radians per metre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Free") float FreeDroop = 2.2f;

	// --- Stems

	/** Distance of a stem's axis from the wall beyond its own radius, m (the adhesive pads). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stems") float StickGap = 0.004f;
	/** Pipe model: radius = MinRadius + RadiusPerSqrtLength * sqrt(stem it carries, m) + RingWidth * age of the wood. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stems") float MinRadius = 0.0025f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stems") float RadiusPerSqrtLength = 0.0016f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stems") float RingWidth = 0.0006f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stems") float MaxRadius = 0.06f;
};

/** The built-in vines (VineSpecies.cpp). */
namespace VineSpeciesLibrary
{
	LYUDMILOVSK_API const TArray<FVineSpecies>& All();
	/** nullptr when there is no such vine. */
	LYUDMILOVSK_API const FVineSpecies* Find(FName Name);
	LYUDMILOVSK_API TArray<FName> Names();
}
