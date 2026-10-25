#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TreeBakeCache.generated.h"

class UMaterialInstanceDynamic;
class UStaticMesh;
class UTexture2D;

/** One baked tree kind: species, seed, age (to a tenth of a year). */
struct FTreeBakeKey
{
	FName Species;
	uint32 Seed = 0;
	int32 AgeTenths = 0;
	bool operator==(const FTreeBakeKey& O) const { return Species == O.Species && Seed == O.Seed && AgeTenths == O.AgeTenths; }
	friend uint32 GetTypeHash(const FTreeBakeKey& K) { return HashCombineFast(HashCombineFast(GetTypeHash(K.Species), K.Seed), (uint32)K.AgeTenths); }
};

/** What a forest draws a kind with. */
struct FTreeBaked
{
	TObjectPtr<UStaticMesh> Mesh;
	TObjectPtr<UMaterialInstanceDynamic> Wood, Leaf, Card;
	/** The day of the year it was baked for. */
	float Day = -1000.f;
	/** Counts up every time the mesh is replaced by a newer bake; a user of it compares. */
	int32 Version = 0;
	int32 Triangles[4] = {};
};

/**
 * The trees baked for forests, shared by every AProceduralForest of the world, keyed by species, seed and
 * age, so a forest of 57 actors bakes ~40 kinds once, not once per actor. The heavy half of a bake (skeleton,
 * wood, leaf clusters, mesh descriptions) runs on worker threads; the game thread only turns the finished
 * descriptions into a static mesh, one a frame at most, and waits eight times as long as that took before
 * the next, so the baking never costs more than about a ninth of the frame time on average.
 * A date that has moved on rebakes a kind in the same way; users keep drawing the old bake until the
 * new one is ready.
 */
UCLASS()
class LYUDMILOVSK_API UTreeBakeCache : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * The bake of a kind for a day: null while it is still being made (it is queued by this call), else
	 * the newest one. Asks for a new bake, keeping the old one, when it is more than RebakeDays away from Day.
	 * LodDistances (m) are taken from the first request of a kind.
	 */
	const FTreeBaked* Request(FName Species, uint32 Seed, float Age, float Day, float RebakeDays, const FVector& LodDistances);
	/** The textures made for a species (null until one of its kinds has been finished). */
	bool GetSpeciesTextures(FName Species, UTexture2D*& Albedo, UTexture2D*& Normal, UTexture2D*& LeafDetail) const;
	/** Everything queued is made at once, on this thread (editor, tests). */
	void BakeAllNow();
	int32 NumBaked() const { return Baked.Num(); }
	int32 NumPending() const { return Wanted.Num() + Running + Finished.Num(); }
	FString DescribeStats() const;

	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UTreeBakeCache, STATGROUP_Tickables); }
	virtual bool IsTickableInEditor() const override { return false; }

	/** Worker threads at most. */
	int32 MaxWorkers = 3;

private:
	struct FJob;
	struct FSpeciesAssets
	{
		TObjectPtr<UTexture2D> Albedo, Normal, Detail, Card;
	};
	void Finish(FJob& Job);
	void Launch(const TSharedPtr<FJob>& Job);

	// not reflected: everything it points to is kept alive through KeepAlive
	TMap<FTreeBakeKey, FTreeBaked> Baked;
	TMap<FTreeBakeKey, FVector> LodsOf;
	TArray<TSharedPtr<FJob>> Wanted;
	TArray<TSharedPtr<FJob>> Finished;
	TSet<FTreeBakeKey> Queued;
	TMap<FName, FSpeciesAssets> Assets;
	FCriticalSection FinishedLock;
	TAtomic<int32> RunningAtomic = 0;
	int32 Running = 0;
	double NextFinishTime = 0.0;
	double PrepareMs = 0.0, FinishMs = 0.0, WorstFinishMs = 0.0;
	int32 Done = 0;
	int64 TotalTris[4] = {};
	UPROPERTY(Transient) TArray<TObjectPtr<UObject>> KeepAlive;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BarkMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> LeafMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> NeedleMaterial;
};
