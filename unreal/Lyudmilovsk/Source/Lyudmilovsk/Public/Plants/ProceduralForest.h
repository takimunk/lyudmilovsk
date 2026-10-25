#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralForest.generated.h"

class AProceduralPlant;
class UInstancedStaticMeshComponent;
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
 * few seconds at BeginPlay, trees appearing as their bake finishes. The bakes are shared by every forest of the world (UTreeBakeCache), made on worker threads.
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest", meta = (ClampMin = "1", ClampMax = "8")) int32 Variants = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest") bool bSnapToGround = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest", meta = (ClampMin = "0", ClampMax = "1")) float MinSlopeNormalZ = 0.75f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest") float MinWorldZ = -100000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest") float MaxWorldZ = 100000.f;
	/** Ground that belongs to an actor with this tag (a path, the beach, a building's yard) stays treeless. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest") FName BareGroundTag = TEXT("NoTree");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest") float TraceUp = 4000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest") float TraceDown = 8000.f;

	/** Trees closer than this to the camera become live plants, cm; at most MaxLive at a time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Detail") float LiveDistance = 1800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Detail") int32 MaxLive = 10;
	/**
	 * Only trees you can do something with are swapped for live plants (fruit to pick up); a pine or a birch looks the same
	 * as its baked LOD0, and building a live plant is a hitch of tens of milliseconds. Off: every species is swapped in.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Detail") bool bLiveOnlyInteractive = true;
	/** No live plant is swapped in while the camera moves faster than this, cm/s (a bird in flight passes the trees by; a run is ~600). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Detail") float LiveMaxSpeed = 900.f;
	/** Live plants are only swapped in for a camera at most this high above the tree's foot, cm: from higher up the baked tree is as good. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Detail") float LiveMaxHeight = 800.f;
	/** At most this many live plants are built in one tick of the forest (a plant takes tens of ms to build). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Detail", meta = (ClampMin = "1")) int32 LivePerTick = 1;
	/** Camera distances where the baked trees drop to LOD1, LOD2 and LOD3, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Detail") FVector LodDistances = FVector(1500.0, 5500.0, 15000.0);
	/** Trees farther than this are not drawn, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Detail") float CullDistance = 50000.f;
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
	/** Every tree: Fn(Species, Transform, Age) (the bird life perches on their crowns). */
	template <typename FFn> void ForEachTree(FFn&& Fn) const { for (const FTree& T : Trees) Fn(T.Species, T.Xf, T.Age); }
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
		TObjectPtr<UInstancedStaticMeshComponent> Comp;
		TArray<int32> TreeIds;
		/** The version of the shared bake the component shows now (-1: none yet). */
		int32 Version = -1;
		bool bInstancesAdded = false;
	};

	float CurrentDay() const;
	uint32 BakeSeedFor(FName Species, int32 Variant) const;
	int32 GroupFor(FName Species, uint32 BakeSeed, float Age);
	void Clear();
	void Scatter();
	void SyncBakes(float Day);
	void UpdateLive(const FVector& Camera);
	bool Interactive(FName Species) const;
	void SetHidden(FTree& T, bool bHidden);

	FVector LastCamera = FVector::ZeroVector;
	double LastCameraTime = -1.0;
	TArray<FTree> Trees;
	TArray<FGroup> Groups;
	int32 LiveCount = 0;
	int32 Applied = 0;
};
