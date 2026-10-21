#pragma once

#include "CoreMinimal.h"
#include "Plants/PlantSpecies.h"
#include "PlantModel.generated.h"

/**
 * Level 0 of a plant: what it looks like from far away, in O(1) from (species, seed, age, day),
 * without building any branches. Like a sparse index: a forest can be culled, coloured and drawn
 * as silhouettes from this alone; only plants near the camera go on to the skeleton (level 1)
 * and leaves (level 2).
 */
USTRUCT(BlueprintType)
struct FPlantSummary
{
	GENERATED_BODY()

	/** m */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plant") float Height = 0.f;
	/** Bare trunk below this, m. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plant") float CrownBaseHeight = 0.f;
	/** m */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plant") float CrownRadius = 0.f;
	/** m */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plant") float TrunkRadius = 0.f;
	/** 0 bare .. 1 full summer foliage. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plant") float LeafAmount = 0.f;
	/** Mean colour of the leaves on the tree (linear). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plant") FLinearColor LeafColour = FLinearColor::Black;
};

/** The leaves' state on a day of the year, for one tree. */
struct FPlantSeason
{
	static constexpr int32 NumBuckets = 3;
	/** Leaf size, 0..1 (they unfold in spring). */
	float LeafScale = 0.f;
	/** Per colour bucket: fraction of its leaves already fallen, and its colour. Bucket 0 turns first. */
	float Fallen[NumBuckets] = {};
	FLinearColor Colour[NumBuckets];
	/** This tree's own day of the year (the calendar's, shifted by its seed). */
	float Day = 0.f;
	/** Fruit size 0..1 (0: no fruit this time of year), colour (one plain colour, for far away) and ripeness 0..1. */
	float FruitScale = 0.f;
	FLinearColor FruitColour;
	float FruitRipeness = 0.f;
	/** Per colour bucket: the fraction that had fallen LitterDays ago (older litter has rotted away). */
	float LitterFrom[NumBuckets] = {};
	/** Snow lying on the branches, 0..1. */
	float Snow = 0.f;
};

/** Pure functions of the species: no state, no randomness beyond the seed. */
namespace PlantModel
{
	/** 0..1 from a seed and up to three keys; the same inputs give the same number on every machine. */
	LYUDMILOVSK_API float Hash01(uint32 Seed, uint32 A, uint32 B = 0, uint32 C = 0);

	/** Height at an age, m. */
	LYUDMILOVSK_API float Height(const FPlantSpecies& S, float Age);
	/** Age at which the tree reaches a height (inverse of Height); infinity if it never does. */
	LYUDMILOVSK_API float AgeAtHeight(const FPlantSpecies& S, float H);
	LYUDMILOVSK_API FPlantSeason Season(const FPlantSpecies& S, uint32 Seed, float DayOfYear);
	LYUDMILOVSK_API FPlantSummary Summary(const FPlantSpecies& S, uint32 Seed, float Age, float DayOfYear);
}

/** One tree's branches as they will be at every age; built once from (species, seed). */
class LYUDMILOVSK_API FPlantSkeleton
{
public:
	struct FBranch
	{
		int32 Parent = INDEX_NONE;
		int32 Order = 0;
		/** Arc length on the parent where it buds, m. */
		float BaseS = 0.f;
		/** Tree age at which it buds and at which it is shed, years. */
		float Birth = 0.f;
		float Death = TNumericLimits<float>::Max();
		/** L(t) = MaxLength * (1 - exp(-t / Tau)); the trunk follows the height curve instead. */
		float MaxLength = 0.f;
		float Tau = 1.f;
		/** The whole curve it will ever grow along, one point every Step metres from its base (tree space, m, Z up). */
		float Step = 0.25f;
		TArray<FVector3f> Points;

		/** Years after its birth at which it is S long; infinity if never. */
		float TimeAtLength(float S) const;
		FVector3f At(float S) const;
		FVector3f DirAt(float S) const;
	};

	/** One branch's wood as drawn: points along its axis and the radius there (m). */
	struct FTube
	{
		int32 Order = 0;
		TArray<FVector3f> P;
		TArray<float> R;
	};

	/** What to draw at one age: the wood, and leaf clusters per colour bucket and fruit (instances, cm). */
	struct FInstances
	{
		TArray<FTube> Tubes;
		TArray<FTransform> Leaves[FPlantSeason::NumBuckets];
		/** Fruit still hanging, and its ids (stable: the twig it grows on). */
		TArray<FTransform> Fruit;
		TArray<int32> FruitIds;
		/** Each hanging fruit's skin, FruitDataFloats per fruit (blush, blush side, stripes, tint, ripeness shift). */
		static constexpr int32 FruitDataFloats = 5;
		TArray<float> FruitData;
		/** Fruit that has dropped and not yet rotted: id, where it hung (m) and its skin. */
		struct FFallen { int32 Id; FVector3f At; float Data[FruitDataFloats]; };
		TArray<FFallen> FallenFruit;
		/** Fallen leaves lying under the tree: where (m, the ground's height is the caller's), turn, size, bucket. */
		struct FLitter { FVector2f At; float Yaw; float Size; int32 Bucket; };
		TArray<FLitter> Litter;
		/** Leaves that let go since the previous season given to Evaluate: where they hung (cm) and where they land. */
		struct FDrop { FTransform From; FLitter To; };
		TArray<FDrop> Dropped;
		/** Height and widest reach of what was drawn, m. */
		float Height = 0.f;
		float Reach = 0.f;
		int32 LiveBranches = 0;
		int32 NumRings() const;
	};

	void Build(const FPlantSpecies& Species, uint32 Seed);
	bool IsBuilt() const { return !Branches.IsEmpty(); }
	/** Prev (optional): the season at the last evaluation, to list the leaves that dropped since. */
	void Evaluate(float Age, const FPlantSeason& Season, FInstances& Out, const FPlantSeason* Prev = nullptr) const;
	const TArray<FBranch>& GetBranches() const { return Branches; }

private:
	float LengthAt(const FBranch& B, float Age) const;
	FPlantSpecies S;
	uint32 Seed = 0;
	TArray<FBranch> Branches; // parents always before their children
};
