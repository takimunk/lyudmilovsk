#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyudSave.h"
#include "HeroSeat.generated.h"

class UBoxComponent;
class ULevelSequence;
class ULevelSequencePlayer;
class ALevelSequenceActor;

/**
 * The hero's chair. Place it on the floor at the chair, facing the screen (+X): the keyboard is ~60 cm
 * and the screen ~91 cm ahead of the origin, the cutscene is played in this actor's space.
 * Walk into the trigger and press E: the player is taken over by the
 * "sit at the computer" cutscene (hero sits, hands on the keyboard, camera flies to the screen).
 */
UCLASS()
class LYUDMILOVSK_API AHeroSeat : public AActor, public ILyudSaveable
{
	GENERATED_BODY()

public:
	AHeroSeat();

	/** The sit-down cutscene (Level Sequence). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	TSoftObjectPtr<ULevelSequence> SitSequence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	FString PromptKey = TEXT("ui.seat.prompt");

	bool bPromptShown = false;

	/** bSkipToEnd: the hero is seated at once (a loaded save), the cutscene jumps to its last frame. */
	UFUNCTION(BlueprintCallable, Category = "Seat")
	void StartSit(APlayerController* PC, bool bSkipToEnd = false);

	UFUNCTION(BlueprintPure, Category = "Seat")
	bool IsSitting() const { return bSitting; }

	virtual void OnGameLoaded_Implementation() override;

	/** The player stands by the chair and E would sit him down. */
	bool IsPlayerInRange() const { return bInRange && !bSitting; }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Seat")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY()
	TObjectPtr<ALevelSequenceActor> SequenceActor;

private:
	void OnInteract();

	bool bInRange = false;
	TWeakObjectPtr<APlayerController> InRangePC;
	UPROPERTY(SaveGame)
	bool bSitting = false;
};
