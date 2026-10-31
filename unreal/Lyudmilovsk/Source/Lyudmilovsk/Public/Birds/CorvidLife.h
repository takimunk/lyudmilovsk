#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "CorvidLife.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
namespace LyudBirds { class FWorld; class FCorvids; }

/** Something a corvid did (BirdCorvid.h ECorvidEvent): Pick, Drop, Cache, Dig, Rob, Steal, Snatch, Gift, Scold, Play, Tease, Crack, Eat. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCorvidLifeEvent, int32, BirdId, FName, What, int32, ItemId);

/**
 * The crow family in the game (docs/corvids.md): draws the things they carry, hide and drop (acorns, nuts, cones, mussels,
 * crusts, a dead herring, bottle caps, foil, rings, buttons, coins, keys, sea glass, sticks) as instanced meshes, in the
 * bills of near bodies on their jaw bone; lets the player pick finds up with E (presents, things lying about, a store they
 * watched being dug in) and throw a handful of feed with G; takes the feed a magpie snatches out of the hand; tells the
 * player when a crow brings a present or a gang of them starts scolding; keeps each bird's bond and grudge in the save.
 *
 * The minds themselves are plain C++ in the bird life (Private/Birds/BirdCorvid.cpp, FCorvids on UBirdLifeSubsystem's
 * simulation); lyud.Birds.Corvids 0 leaves the corvids living like the other birds (it switches while running, for A/B).
 */
UCLASS()
class LYUDMILOVSK_API UCorvidLifeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UCorvidLifeSubsystem* Get(const UObject* WorldContext);

	/** Get for Blueprints and Python: unreal.CorvidLifeSubsystem.get_corvid_life(world). */
	UFUNCTION(BlueprintPure, Category = "Birds", meta = (WorldContext = "WorldContext"))
	static UCorvidLifeSubsystem* GetCorvidLife(const UObject* WorldContext) { return Get(WorldContext); }

	/** Fires for every corvid event near the player (within ~120 m) on the game thread. */
	UPROPERTY(BlueprintAssignable, Category = "Birds") FCorvidLifeEvent OnCorvid;

	/** Bond (0..1, from food) and grudge (0..1, from harm) toward the player per bird id, kept with the save. */
	UPROPERTY(SaveGame) TMap<int32, float> SavedBond;
	UPROPERTY(SaveGame) TMap<int32, float> SavedGrudge;

	/** Throws a handful of feed onto the ground in front of the player (G with feed in the hand; tests call it directly). */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	bool ThrowFeed(bool bFromInventory = true);

	/** Puts an item of a kind (Acorn, Walnut, Mussel, BottleCap, Fish, Stick...) on the ground at Location (cm); its id or -1. */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	int32 SpawnItem(FName Kind, FVector Location);

	/** Picks up the find nearest in front of the player within reach, as E does (tests); the inventory id taken, or none. */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	FName TakeNearest();

	/** Ids of the corvids carrying something now, nearest to Location (cm) first (tests, close-ups). */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	TArray<int32> GetCarriers(FVector Location, float Radius) const;

	/** A corvid's task now (None, Fetch, Cache, Crack, Gift, Scold...) and what it holds (an item kind, or empty). */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	FString DescribeCorvid(int32 BirdId) const;

	/** The inventory id a find goes into the pocket as (find_acorn...), none for things that can't be picked up. */
	static FName FindItemId(int32 Kind);

	/** One line: items, tasks, events (console: lyud.Corvids.Report). */
	FString Report() const;

	/** Sets bond and grudge of every corvid within Radius (cm) of Location (developer mode, tests). */
	void SetFeelings(FVector Location, float Radius, float Bond, float Grudge);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	UPROPERTY(Transient) TObjectPtr<class ACorvidItemsRenderer> Renderer;
	int32 SimStart = -1;
	float SaveTimer = 0.f;
	int32 Prompted = -1;
	double LastScoldToast = -1e9;
	double LastGiftToast = -1e9;
	LyudBirds::FWorld* Sim() const;
	LyudBirds::FCorvids* Corvids() const;
	void SyncFeelings(bool bToSave);
	void UpdatePlayer();
	void Dispatch();
	void Take(int32 Item);
	int32 Pickable() const;
};

/** Draws the corvids' items: one instanced mesh per kind (/Game/Birds/Items/SM_CorvidItem_<Kind>, a placeholder shape until imported). */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ACorvidItemsRenderer : public AActor
{
	GENERATED_BODY()

public:
	ACorvidItemsRenderer();
	void Setup();
	void Draw(const LyudBirds::FWorld& World, const FVector& Camera);
	UStaticMesh* MeshOf(int32 Kind) const;
	int32 DrawnLastFrame = 0;

private:
	UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Kinds;
	TArray<TArray<FTransform>> Xf;
	TArray<TArray<FTransform>> Last;
	/** Placeholder scale per kind when the item mesh isn't imported (an engine sphere of that size). */
	TArray<FVector> PlaceholderScale;
};
