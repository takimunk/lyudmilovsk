#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FallenFruit.generated.h"

class AProceduralPlant;
class UStaticMeshComponent;

/**
 * A fruit that has dropped from an AProceduralPlant: a physics object on the ground until it rots
 * or the player picks it up (E, looking at it). Its tree spawns and removes it; nothing about it is
 * saved but whether it was picked (on the tree).
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API AFallenFruit : public AActor
{
	GENERATED_BODY()

public:
	AFallenFruit();

	UPROPERTY(VisibleAnywhere, Category = "Fruit") TObjectPtr<UStaticMeshComponent> Mesh;

	/** The tree it fell from and its key there (year and twig). */
	UPROPERTY(Transient) TWeakObjectPtr<AProceduralPlant> Tree;
	int32 Key = 0;
};
