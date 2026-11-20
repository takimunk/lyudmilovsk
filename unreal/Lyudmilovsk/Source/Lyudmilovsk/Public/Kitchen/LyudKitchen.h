#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "LyudKitchen.generated.h"

class APlayerController;
class AHeroCharacter;
class SLyudCityPage;
namespace LyudKitchen { struct FPot; }

/**
 * Cooking from what the hero grew and caught (docs/kitchen.md, backlog item 34). At a stove of the town (the commune
 * canteens' and the sanatorium's, reached at their counters; the town hall's kitchen, a few homes') Y opens the recipes:
 * ukha, fried potatoes with dill, fried fish, a salad, strawberry jam. The ingredients leave the pockets, the pot is on
 * the fire for half a minute or so and asks for a hand once or twice (Y: skim, stir, turn, salt); Y again takes it off when
 * it is done. A dish cooked without a slip gives a portion more; one left on the fire burns. The dishes are food items
 * (eaten by hand, gifts friends like). A jar of his own jam goes to the market's Sunday contest like the neighbour's. The
 * «Янтарь»'s kitchen door buys fresh vegetables with the fish (ULyudFishing asks VegOffer / SellVeg).
 *
 * The rules are LyudKitchen (Private/Kitchen/KitchenModel, plain C++, tools/kitchensim). Stamps "cook/<piece>", quest events
 * "cook dish" and "cook <recipe>", diary notes. A thing with what=hot_plate opens the same page with E (act "hot_plate",
 * "open": "cook"), as a one-ring plate (no jam). Nothing is placed in the level and nothing is drawn: the stoves are the
 * town's own things. The pot is not saved: a game loaded mid-cooking has lost what was in it.
 *
 * Console: lyud.Kitchen.Info, lyud.Kitchen.Open, lyud.Kitchen.Cook <recipe>, lyud.Kitchen.Finish, lyud.Kitchen.Kit.
 */
UCLASS()
class LYUDMILOVSK_API ULyudKitchen : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudKitchen();
	virtual ~ULyudKitchen() override;

	static ULyudKitchen* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudKitchen, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bBegun; }

	/** The recipes' page is up (others who use the digits or Y step aside). */
	bool HasPageUp() const { return Page.IsValid(); }

	/** What the «Янтарь»'s kitchen would take of the pockets' vegetables now (its day's want left): how many, for how much. */
	int32 VegOffer(int32* OutRoubles = nullptr) const;
	/** Sells it those; the roubles (0: nothing it wants). */
	int32 SellVeg();

	// --- console and developer mode
	FString Report() const;
	/** The recipes' page wherever he stands (as at a stove). */
	void DevOpen();
	/** Puts a recipe on the fire wherever he stands (as at a stove); false when the pockets lack something. */
	bool DevCook(const FString& Recipe);
	/** What is on the fire is done this moment, without a slip, and comes off; nothing on: cooks the first dish he can. */
	void DevFinish();
	/** Ingredients for all five dishes. */
	void DevKit();

	/** Recipes cooked at least once (a bit per recipe of LyudKitchen::Recipes), dishes cooked, dishes burnt. */
	UPROPERTY(SaveGame) int32 CookedBits = 0;
	UPROPERTY(SaveGame) int32 Dishes = 0;
	UPROPERTY(SaveGame) int32 Burnt = 0;
	/** The restaurant's buying of vegetables: the day it counts (days since 2000), things taken that day, all it paid. */
	UPROPERTY(SaveGame) int32 VegDay = -1;
	UPROPERTY(SaveGame) int32 VegTaken = 0;
	UPROPERTY(SaveGame) int32 VegRoubles = 0;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	struct FSpot
	{
		FVector At = FVector::ZeroVector;
		/** 0 a hot plate, 1 a stove (LyudKitchen::EHeat). */
		uint8 Heat = 1;
		/** Its kitchen's hours [From, To). */
		int32 From = 0, To = 24;
		/** Opened with E by the thing's own act ("open": "cook"), not with Y. */
		bool bByAct = false;
	};

	void FindSpots();
	int32 NearestSpot(const FVector& From, float WithinCm, bool bByAct) const;
	bool SpotOpen(const FSpot& Spot) const;
	int32 Held(const FString& Item) const;
	void OnCityOpen(FName Screen, int32 Thing);
	void OpenPage(int32 Spot, uint8 Heat);
	void ClosePage();
	void TickPage(APlayerController* PC);
	bool Start(int32 Recipe, uint8 Heat, int32 Spot, const FVector& At);
	void TickPot(float DeltaTime, const AHeroCharacter* Hero);
	void TakeOffNow();
	void SetPrompt(const FText& Text);
	void Toast(const FText& Text) const;
	int32 Today() const;

	bool bBegun = false;
	float SpotsIn = 0.f;
	float NearIn = 0.f;
	TArray<FSpot> Spots;
	int32 NearSpot = -1;
	bool bHinted = false;

	// the recipes' page
	TSharedPtr<SLyudCityPage> Page;
	int32 PageSpot = -1;
	uint8 PageHeat = 1;
	uint64 PageFrame = 0;
	TWeakObjectPtr<APlayerController> MoveBlockedBy;

	// the pot on the fire
	TPimplPtr<LyudKitchen::FPot> Pot;
	FVector PotAt = FVector::ZeroVector;
	int32 PotBerries = 2;
	float SoundIn = 0.f;
	bool bAwayWarned = false;
	FText PromptText;
	FDelegateHandle OpenHandle;
};
