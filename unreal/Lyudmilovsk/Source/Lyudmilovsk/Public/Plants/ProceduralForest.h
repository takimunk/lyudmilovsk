#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralForest.generated.h"

class AProceduralPlant;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/** One kind of tree in a forest, with the two ages its trees come in. */
USTRUCT(BlueprintType)
struct FForestMix
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest", meta = (GetOptions = "GetSpeciesNames")) FName Species = TEXT("ScotsPine");
	/** Share of the trees; the stands of one species come and go over tens of metres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest", meta = (ClampMin = "0")) float Weight = 1.f;
	/** Years of a young tree and of a mature one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest") float YoungAge = 12.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest") float MatureAge = 45.f;
	/** Fraction of this kind that is young. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest", meta = (ClampMin = "0", ClampMax = "1")) float YoungShare = 0.3f;
};

/**
 * A forest of hundreds to thousands of the real procedural trees (AProceduralPlant's skeleton and seasons),
 * cheap enough for a Mac: the trees are not actors. Each (species, variant, age) is baked once into a static
 * mesh with four LODs (full tree, limbs only, sparse clusters, trunk and a few big clusters) by
 * PlantMeshes::BuildTreeBake, and drawn as hierarchical instances, which cull by view and distance and pick
 * their LOD on their own. The few trees nearest the camera are swapped for live AProceduralPlant actors
 * (same species, seed, age and place, so the swap does not show) with falling leaves, fruit and growth,
 * and swapped back when the player walks away. The date moves the baked trees on too: they are baked
 * again when it has shifted by a couple of weeks.
 *
 * Place it with Mix, Count and HalfExtent, or add trees one by one with AddTree. Bake and layout take a
 * few seconds at BeginPlay, one bake per quarter second, trees appearing as their bake finishes.
 */
UCLASS()
class LYUDMILOVSK_API AProceduralForest : public AActor
{
	GENERATED_BODY()

public:
	AProceduralForest();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest") TArray<FForestMix> Mix;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest") int32 Seed = 1;
	/** Half the width and depth of the forest, cm, about the actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest", meta = (ClampMin = "100")) FVector2D HalfExtent = FVector2D(10000.0, 10000.0);
	/** About this many trees, scattered on a jittered grid so none stand on top of each other. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest", meta = (ClampMin = "0")) int32 Count = 400;
	/** Grid cell of the scatter, cm: trees stand at least a third of it apart. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest", meta = (ClampMin = "50")) float Spacing = 450.f;
	/** Trees of a kind are baked in this many seeds, so no two neighbours are twins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest", meta = (ClampMin = "1", ClampMax = "8")) int32 Variants = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest") bool bSnapToGround = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest", meta = (ClampMin = "0", ClampMax = "1")) float MinSlopeNormalZ = 0.75f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest") float MinWorldZ = -100000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest") float MaxWorldZ = 100000.f;
	/** Ground that belongs to an actor with this tag (a path, the beach, a building's yard) stays treeless. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest") FName BareGroundTag = TEXT("NoTree");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest") float TraceUp = 4000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest") float TraceDown = 8000.f;

	/** Trees closer than this to the camera become live plants, cm; at most MaxLive at a time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Detail") float LiveDistance = 3000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Detail") int32 MaxLive = 10;
	/** Camera distances where the baked trees drop to LOD1, LOD2 and LOD3, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Detail") FVector LodDistances = FVector(4500.0, 12000.0, 30000.0);
	/** Trees farther than this are not drawn, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Detail") float CullDistance = 120000.f;
	/** The baked trees are redone when the date has moved this many days. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Detail") float RebakeDays = 14.f;
	/** Lay the forest out and bake it when the level opens in the editor (a few seconds per thousand trees; off for big levels). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Detail") bool bPreviewInEditor = false;
	/** Developer mode: hold the season at a day of the year (below 0: the calendar). */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Forest|Dev") float DevDay = -1.f;

	/** Puts a tree of a species in place (a stand laid by hand, or by the world's tools). Call before Rebuild, or after to add it at the next bake. */
	UFUNCTION(BlueprintCallable, Category = "Forest") void AddTree(FName Species, FVector Location, float Age, int32 Variant = 0, float Yaw = 0.f, float Scale = 1.f);
	/** Scatter the trees from Mix and start baking. */
	UFUNCTION(BlueprintCallable, Category = "Forest") void Rebuild();
	/** Bake every kind now (in the editor, where nothing ticks; in play they are baked one per tick). */
	UFUNCTION(BlueprintCallable, Category = "Forest") void BakeAll();
	UFUNCTION(BlueprintCallable, Category = "Forest") int32 NumTrees() const { return Trees.Num(); }
	UFUNCTION(BlueprintCallable, Category = "Forest") int32 NumBaked() const;
	UFUNCTION(BlueprintCallable, Category = "Forest") int32 NumLive() const { return LiveCount; }
	UFUNCTION(BlueprintCallable, Category = "Forest") FString DescribeState() const;
	UFUNCTION() TArray<FName> GetSpeciesNames() const;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	struct FTree
	{
		FName Species;
		uint32 BakeSeed = 0;
		float Age = 0.f;
		FTransform Xf;
		int32 Group = INDEX_NONE;
		int32 Instance = INDEX_NONE;
		TWeakObjectPtr<AProceduralPlant> Live;
	};
	/** All the trees of one species, variant and age: one baked mesh, one set of instances. */
	struct FGroup
	{
		FName Species;
		uint32 BakeSeed = 0;
		float Age = 0.f;
		TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Comp;
		TObjectPtr<UMaterialInstanceDynamic> WoodMaterial, LeafMaterial;
		TArray<int32> TreeIds;
		bool bBaked = false;
		bool bInstancesAdded = false;
		float BakedDay = -1000.f;
	};

	float CurrentDay() const;
	uint32 BakeSeedFor(FName Species, int32 Variant) const;
	int32 GroupFor(FName Species, uint32 BakeSeed, float Age);
	void Clear();
	void Scatter();
	void BakeNext(float Day);
	void Bake(FGroup& G, float Day);
	void UpdateLive(const FVector& Camera);
	void SetHidden(FTree& T, bool bHidden);

	TArray<FTree> Trees;
	TArray<FGroup> Groups;
	int32 LiveCount = 0;
	float BakedDay = -1000.f;
	double BakeMs = 0.0;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BarkMaterialAsset;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> LeafMaterialAsset;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> NeedleMaterialAsset;
	UPROPERTY(Transient) TArray<TObjectPtr<UObject>> Keep;
};
