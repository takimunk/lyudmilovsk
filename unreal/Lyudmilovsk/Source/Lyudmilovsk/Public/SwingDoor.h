#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyudSave.h"
#include "SwingDoor.generated.h"

class UBoxComponent;

/**
 * Hinged door (one or more leaves) that swings smoothly between open and closed. Starts open; the
 * player toggles it with E while standing in the Zone. Each leaf is a separate actor whose origin is
 * on its hinge; it may be placed in the level either closed or already open (bLeavesPlacedOpen).
 */
UCLASS()
class LYUDMILOVSK_API ASwingDoor : public AActor, public ILyudSaveable
{
	GENERATED_BODY()

public:
	ASwingDoor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<UBoxComponent> Zone;

	/** The door leaves (pivot on the hinge). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	TArray<TObjectPtr<AActor>> Leaves;

	/** Yaw added to each leaf's closed pose when open, degrees (one per leaf). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	TArray<float> OpenAngles;

	/** True if the leaves stand open in the level as placed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bLeavesPlacedOpen = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bStartOpen = true;

	/** Swing speed, degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float SwingSpeed = 160.f;

	UFUNCTION(BlueprintCallable, Category = "Door")
	void Toggle();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void OnGameLoaded_Implementation() override;

private:
	void OnInteract();

	void Apply();

	TArray<FRotator> ClosedRotations;
	float Openness = 1.f;   // 0 closed .. 1 open
	UPROPERTY(SaveGame)
	bool bOpen = true;
};
