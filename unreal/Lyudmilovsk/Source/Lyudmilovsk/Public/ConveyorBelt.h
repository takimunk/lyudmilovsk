#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ConveyorBelt.generated.h"

class UBoxComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;
class USoundBase;

/**
 * Moving walkway driven by whoever stands on it (player or NPC, any ACharacter): it speeds up
 * in the direction they walk or run, harder the faster they move; walking against it brakes and
 * reverses it; when they stop or step off it eases to a halt.
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

	/** Axis the belt runs along (it can move either way). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor")
	FVector Direction = FVector(0.f, 1.f, 0.f);

	/** Top belt speed either way, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor")
	float MaxSpeed = 220.f;

	/** How fast it speeds up while a rider runs along it at RunSpeed, cm/s². */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor")
	float Acceleration = 160.f;

	/** How fast it slows down when the riders stand still or nobody is on it, cm/s². */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor")
	float Deceleration = 300.f;

	/** Rider speed treated as full push (a run), cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor")
	float RunSpeed = 600.f;

	/** Rider speeds below this count as standing still, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor")
	float WalkThreshold = 30.f;

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
