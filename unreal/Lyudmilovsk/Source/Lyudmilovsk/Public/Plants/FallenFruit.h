#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FallenFruit.generated.h"

class AProceduralPlant;
class UMaterialInterface;
class USphereComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * A fruit that has dropped from an AProceduralPlant: a physics ball on the ground (its skin drawn
 * with the tree's fruit mesh) until it rots or the player picks it up (E, looking at it). Its tree
 * spawns and removes it; nothing about it is saved but whether it was picked (on the tree).
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API AFallenFruit : public AActor
{
	GENERATED_BODY()

public:
	AFallenFruit();

	/** Diameter in cm; Data: the fruit's five skin numbers (M_PlantFruit's Fruit0..4). */
	void Init(UStaticMesh* FruitMesh, UMaterialInterface* Material, float Diameter, const float* Data, int32 NumData);

	UPROPERTY(VisibleAnywhere, Category = "Fruit") TObjectPtr<USphereComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "Fruit") TObjectPtr<UStaticMeshComponent> Mesh;

	/** The tree it fell from and its key there (year and twig). */
	UPROPERTY(Transient) TWeakObjectPtr<AProceduralPlant> Tree;
	int32 Key = 0;
};
