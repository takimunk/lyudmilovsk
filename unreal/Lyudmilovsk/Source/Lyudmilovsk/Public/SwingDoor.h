#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SwingDoor.generated.h"

class UBoxComponent;

/**
 * Hinged door that swings smoothly between open and closed. Starts open; the player toggles it
 * with E while standing in the Zone. The leaf is a separate actor whose origin is on the hinge.
 */
UCLASS()
class LYUDMILOVSK_API ASwingDoor : public AActor
{
	GENERATED_BODY()

public:
	ASwingDoor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<UBoxComponent> Zone;

	/** The door leaf (pivot on the hinge, placed closed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	TObjectPtr<AActor> Leaf;

	/** Yaw added to the closed pose when open, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float OpenAngle = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bStartOpen = true;

	/** Swing speed, degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float SwingSpeed = 160.f;

	UFUNCTION(BlueprintCallable, Category = "Door")
	void Toggle();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void OnInteract();

	FRotator ClosedRotation;
	float Angle = 0.f;
	bool bOpen = true;
};
