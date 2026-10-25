#pragma once

#include "CoreMinimal.h"
#include "Doors/LyudDoor.h"
#include "SwingDoor.generated.h"

class UBoxComponent;

/**
 * A door whose leaves are separate actors placed in the level, each with its origin on its hinge (the club's blast
 * door, the hall's partition): everything else (E, NPCs, saving, not shutting on anyone) is ALyudDoorBase's. The Zone
 * is the doorway: the player uses the door with E from inside it. The leaves may be placed closed or already open
 * (bLeavesPlacedOpen). New doors should be ALyudDoor; this one is for unique doors modelled with their building.
 */
UCLASS()
class LYUDMILOVSK_API ASwingDoor : public ALyudDoorBase
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

	virtual FBox GetDoorwayLocalBox() const override;
	/** Where the leaves move between closed and open, measured from them at BeginPlay. */
	virtual FBox GetSweepLocalBox() const override { return SweepBox.IsValid ? SweepBox : GetDoorwayLocalBox(); }
	virtual bool PlayerCanUse(const APawn* Pawn, const FVector& Eye, const FVector& Dir, float& OutDistance) const override;

protected:
	virtual void PrepareLeaves() override;
	virtual void ApplyOpenness(float InOpenness) override;
	virtual void GetLeafPrimitives(TArray<UPrimitiveComponent*>& Out) const override;

private:
	TArray<FRotator> ClosedRotations;
	FBox SweepBox = FBox(ForceInit);
};
