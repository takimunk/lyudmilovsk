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

	/** What to draw at one age: bark segments and leaf clusters per colour bucket, in cm. */
	struct FInstances
	{
		TArray<FTransform> Bark;
		TArray<FTransform> Leaves[FPlantSeason::NumBuckets];
		/** Height and widest reach of what was drawn, m. */
		float Height = 0.f;
		float Reach = 0.f;
		int32 LiveBranches = 0;
	};

	void Build(const FPlantSpecies& Species, uint32 Seed);
	bool IsBuilt() const { return !Branches.IsEmpty(); }
	void Evaluate(float Age, const FPlantSeason& Season, FInstances& Out) const;
	const TArray<FBranch>& GetBranches() const { return Branches; }

private:
	float LengthAt(const FBranch& B, float Age) const;
	FPlantSpecies S;
	uint32 Seed = 0;
	TArray<FBranch> Branches; // parents always before their children
};
