#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "PlantDevTool.generated.h"

class AProceduralPlant;
class SWidget;

/**
 * Plants developer mode, in play: F7 (fn+F7 on a Mac keyboard) opens a panel over the game to pick a
 * tree (the one in view, or step through them) and drive it: age, day of the year, how fast each
 * runs, species, seed, the far-away summary. Overrides live on the tree (DevAge / DevDay) and are
 * not saved. F7 is not consumed, and the game keeps its keyboard while the panel is open.
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API APlantDevTool : public AActor
{
	GENERATED_BODY()

public:
	APlantDevTool();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	void Toggle();
	void SelectInView();
	void Step(int32 Delta);
	AProceduralPlant* Selected() const { return Plant.Get(); }

	/** Years per second added to the selected tree's age, and days per second to its day of the year. */
	float AgeSpeed = 0.f;
	float DaySpeed = 0.f;

private:
	bool TryBindInput();
	void Open();
	void Close();
	TArray<AProceduralPlant*> PlantsByDistance() const;

	TWeakObjectPtr<AProceduralPlant> Plant;
	TSharedPtr<SWidget> Panel;
	bool bBound = false;
};

/** Spawns the plants developer tool into every game world. */
UCLASS()
class LYUDMILOVSK_API UPlantDevSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& World) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type Type) const override { return Type == EWorldType::Game || Type == EWorldType::PIE; }
};
