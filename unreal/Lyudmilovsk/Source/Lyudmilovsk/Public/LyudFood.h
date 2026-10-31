#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyudHandUse.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudFood.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/** How one food is held and eaten (Content/Food/food.json "foods".<item id>; the defaults for the rest). Frames as FLyudHandThing. */
struct FLyudFoodDef
{
	/** Bite stages /Game/Food/SM_Food_<Id>_<k>, k = 0 (whole) .. Stages-1 (the last bite); 0: the item's plain look shrinking. */
	int32 Stages = 0;
	FTransform InPalm = FTransform::Identity;
	FVector MouthFull = FVector::ZeroVector;
	FVector MouthEmpty = FVector::ZeroVector;
	bool bHasMouth = false;
	/** The palm's turn at the mouth (look frame) if not the default's (fingers up, palm to the face: across the mouth). A stick
	 *  eaten from its end lies along the palm toward the face: fingers to the left, palm up. Zero: the default. */
	FVector MouthFingers = FVector::ZeroVector;
	FVector MouthPalm = FVector::ZeroVector;
	float Small = 0.1f, Big = 0.22f, SteadyEach = 0.08f, SteadyEvery = 0.85f;
	/** What it sounds like bitten: crisp (corn), soft (bread, pie), squeak (fresh cheese), chewy (smoked cheese), crunch (chips). */
	FName Texture = TEXT("soft");
	/** Seconds of chewing the whole thing makes (spread over its bites). */
	float ChewSeconds = 20.f;
	/** The wrapper or napkin it comes in rustles when it comes out of the pocket. */
	bool bWrapped = false;
};

/**
 * A food the hero holds: its bite stages (Blender: blender/food/build_food.py, one mesh per stage, authored in the palm's frame:
 * X along the fingers, Z out of the palm, origin the palm's middle), or the item's plain look (ALyudGoodsItem, an apple's sphere)
 * shrinking as it is eaten. NotPlaceable: spawned by ULyudFoodSubsystem.
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ALyudFoodItem : public AActor
{
	GENERATED_BODY()

public:
	ALyudFoodItem();

	/** Item Id's stage meshes; false if it has none (the caller uses the plain look). */
	bool Init(FName InItem, float Left);
	/** Shows the stage for what is left (1 whole .. 0). */
	void SetLeft(float Left);
	FName ItemId() const { return Item; }

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> StageMeshes;
	FName Item;
	int32 Shown = -1;
};

/**
 * Food in the hand, eaten with the left mouse button (ULyudHandUse: click a small bite, hold a big one, keep holding to eat on):
 * an equipped Food item (the inventory's "Взять в руку") comes up from the pocket into the right hand by itself; bites crunch,
 * the mouth chews for a while after and swallows; what is left of a started one stays in the pocket (ULyudInventory::Started).
 * Eaten up, the hand is empty again.
 *
 * Buying: whatever is bought at a counter (ULyudInventoryUI's trade screen) is handed over when the screen shuts: it appears
 * across the counter and the hand takes it into the pocket (UHeroReachComponent::Take).
 *
 * Data: Content/Food/food.json. Sounds: /Game/Audio/Food (audio/sounds.json group food: food_bite_<texture>, food_chew,
 * food_swallow, food_wrap). Console: lyud.Food.Hold <item> (gives one and puts it in the hand).
 */
UCLASS()
class LYUDMILOVSK_API ULyudFoodSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudFoodSubsystem* Get(const UObject* WorldContext);
	static const FLyudFoodDef& FindFood(FName Id);

	/** Puts one Food item from the pocket into the hand now (the inventory's take-in-hand); false if there is none. */
	bool HoldFromPocket(FName Id);
	/** The hand takes Id across the counter at Where into the pocket (cosmetic: the trade has already put it there). */
	void HandOver(FName Id, const FVector& Where);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudFoodSubsystem, STATGROUP_Tickables); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	/** Goods bought at the open trade screen, handed over when it shuts. */
	TArray<FName> Bought;
	struct FHandOver { FName Id; FVector Where; };
	TArray<FHandOver> HandOvers;
	bool bShopWasOpen = false;
	FName Holding;
	float ChewLeft = 0.f;
	float NextChew = 0.f;
	bool bSwallow = false;
	float SwallowIn = 0.f;

	void OnBought(FName Id, int32 Count);
	void Sound(FName Pool, float Volume = 1.f);
	FVector MouthLocation() const;
};
