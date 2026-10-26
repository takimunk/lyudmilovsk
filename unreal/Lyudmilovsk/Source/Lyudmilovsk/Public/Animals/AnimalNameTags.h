#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AnimalNameTags.generated.h"

class SWidget;
struct FAnimalTagView;

/**
 * Names over the animals that have one (Maru, the club's dog; later the ones the player names): always shown while
 * she is in sight within LabelRange, in the style of the people's names (NpcDialogue). Close up and looked at, the label
 * offers [E] (ui.animal.call) and E speaks to her as to a person: UAnimalLifeSubsystem::Hail, a dog barks back gladly.
 * Spawned by the animal life when it starts.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API AAnimalNameTags : public AActor
{
	GENERATED_BODY()

public:
	AAnimalNameTags();

	/** cm: names are shown this far (fading out over the last 5 m). */
	UPROPERTY(EditAnywhere, Category = "Animals") float LabelRange = 3500.f;
	/** cm from the eyes: E reaches her this far. */
	UPROPERTY(EditAnywhere, Category = "Animals") float CallRange = 450.f;

	/** The animal E would speak to now (-1 none): tests. */
	UFUNCTION(BlueprintPure, Category = "Animals") int32 GetTarget() const { return Target; }
	/** Labels drawn last frame: tests. */
	UFUNCTION(BlueprintPure, Category = "Animals") int32 NumLabels() const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	TSharedPtr<FAnimalTagView> View;
	TSharedPtr<SWidget> Overlay;
	int32 Target = -1;
	float CallCooldown = 0.f;

	void Call();
	bool PlayerBusy() const;
};
