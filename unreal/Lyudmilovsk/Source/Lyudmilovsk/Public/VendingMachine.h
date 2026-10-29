#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VendingMachine.generated.h"

class ABottleItem;
class ALyudGoodsItem;
class UStaticMeshComponent;

/** One selection button of a vending machine: Count units of Item (any item of the table) for Price roubles (0: the item's usual price). */
USTRUCT(BlueprintType)
struct FVendingSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Item;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Price = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Count = 1;
};

/**
 * The drinks machine in the hall. Look at it from about a metre away: 1..4 choose a drink, 5 a bag of bird feed, E pays and the machine
 * pushes a bottle onto its tray; the hero's hand takes it (UHeroReachComponent) and it goes into the pocket
 * (ULyudInventory), from where it can be drunk (Tab, then "Выпить"). The model is part of the scene; this actor adds the
 * tray and the behaviour. Its +X is the machine's front. It reads E and the number keys by polling and never takes them.
 */
UCLASS()
class LYUDMILOVSK_API AVendingMachine : public AActor
{
	GENERATED_BODY()

public:
	AVendingMachine();

	/**
	 * Universal machine: what it sells, slot by slot (keys 1..9), any item of the inventory table. Empty and no Preset: the hall's
	 * classic four drinks and a bag of bird feed. Bottles and bird feed are delivered as ABottleItem / ABirdFeedBag, everything else with
	 * a Look as ALyudGoodsItem (the item's icon on its face).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vending")
	TArray<FVendingSlot> Slots;

	/** A stock list from Content/Transport/lobby.json "stock" (drinks, snacks, press, ...); it fills Slots at BeginPlay when Slots is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vending")
	FName Preset;

	/** Price of a bottle, roubles (the bird feed in slot 5 has its own). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vending")
	int32 Price = 25;

	/** From the machine's origin (its middle on the floor): the front face, the delivery flap's centre. */
	UPROPERTY(EditAnywhere, Category = "Vending")
	FVector FlapLocal = FVector(37.5f, 10.f, 22.f);

	/** How far from the flap the player may stand to use it (cm, on the floor). */
	UPROPERTY(EditAnywhere, Category = "Vending")
	float UseDistance = 140.f;

	/** Lay the shelves with bottles (the same labelled ones the player buys) when play begins. */
	UPROPERTY(EditAnywhere, Category = "Vending")
	bool bStockShelves = true;

	virtual void Tick(float DeltaSeconds) override;

	/** Sell slot Slot (0-based) to the hero now, as choosing it and pressing E does (tests). */
	void DebugBuy(int32 Slot);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	enum class EState : uint8 { Idle, Dispensing, Taking };

	void Buy();
	void StockShelves();
	void StockShelvesUniversal();
	AActor* SpawnGoods(const FVendingSlot& Slot, const FVector& Start);
	FVector TrayRest() const;
	FText PromptFor(int32 Flavor) const;
	int32 PriceOf(int32 Slot) const;
	/** Slots (filled from Preset or the classic five). */
	void ResolveSlots();
	bool bClassic = true;
	FVector TrayRestFor(const AActor* Item) const;

	UPROPERTY() TObjectPtr<UStaticMeshComponent> Tray;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> TrayBack;
	/** What was just bought, on its way to the tray: a bottle or a bag of bird feed. */
	UPROPERTY() TObjectPtr<AActor> Goods;
	UPROPERTY() TArray<TObjectPtr<AActor>> Stock;

	EState State = EState::Idle;
	int32 Flavor = 0;
	float Clock = 0.f;
	bool bPromptShown = false;
};
