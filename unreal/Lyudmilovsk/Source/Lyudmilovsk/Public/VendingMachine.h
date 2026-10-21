#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VendingMachine.generated.h"

class ABottleItem;
class UStaticMeshComponent;

/**
 * The drinks machine in the hall. Look at it from about a metre away: 1..4 choose a drink, E pays and the machine
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

	/** Price of a bottle, roubles. */
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
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	enum class EState : uint8 { Idle, Dispensing, Taking };

	void Buy();
	void StockShelves();
	FVector TrayRest() const;
	FText PromptFor(int32 Flavor) const;

	UPROPERTY() TObjectPtr<UStaticMeshComponent> Tray;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> TrayBack;
	UPROPERTY() TObjectPtr<ABottleItem> Bottle;
	UPROPERTY() TArray<TObjectPtr<ABottleItem>> Stock;

	EState State = EState::Idle;
	int32 Flavor = 0;
	float Clock = 0.f;
	bool bPromptShown = false;
};
