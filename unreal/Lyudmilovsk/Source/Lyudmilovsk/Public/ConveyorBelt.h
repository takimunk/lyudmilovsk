#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ConveyorBelt.generated.h"

class UBoxComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;
class USoundBase;

/**
 * Moving walkway. Starts when a character steps onto it and keeps speeding up while someone
 * rides it, carrying them along Direction; slows to a stop once the last one steps off.
 * The belt mesh scrolls through the "Offset" scalar parameter of its material.
 */
UCLASS()
class LYUDMILOVSK_API AConveyorBelt : public AActor
{
	GENERATED_BODY()

public:
	AConveyorBelt();

	/** Volume above the belt; characters overlapping it ride the walkway. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Conveyor")
	TObjectPtr<UBoxComponent> Zone;

	/** Motor hum, louder as the belt speeds up. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Conveyor")
	TObjectPtr<UAudioComponent> Motor;

	/** Actor whose static mesh is the belt surface (gets a dynamic material for scrolling). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor")
	TObjectPtr<AActor> BeltActor;

	/** More belt meshes that scroll in step (the repeated stretches of corridor in the distance). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor")
	TArray<TObjectPtr<AActor>> ExtraBelts;

	/** World direction of travel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor")
	FVector Direction = FVector(0.f, 1.f, 0.f);

	/** Cruising speed, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor")
	float MaxSpeed = 220.f;

	/** How fast it speeds up while someone rides it, cm/s². */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor")
	float Acceleration = 160.f;

	/** How fast it slows down once empty, cm/s². */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor")
	float Deceleration = 300.f;

	/** Distance between belt treads in the material, cm (must match the material). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor")
	float TreadSpacing = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor")
	TObjectPtr<USoundBase> MotorSound;

	UFUNCTION(BlueprintPure, Category = "Conveyor")
	float GetSpeed() const { return Speed; }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	float Speed = 0.f;
	float Offset = 0.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BeltMaterials;
};
