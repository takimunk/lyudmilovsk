#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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
class LYUDMILOVSK_API AHeroSeat : public AActor
{
	GENERATED_BODY()

public:
	AHeroSeat();

	/** The sit-down cutscene (Level Sequence). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	TSoftObjectPtr<ULevelSequence> SitSequence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	FString Prompt = TEXT("[E]  Сесть за компьютер");

	UFUNCTION(BlueprintCallable, Category = "Seat")
	void StartSit(APlayerController* PC);

	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Seat")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY()
	TObjectPtr<ALevelSequenceActor> SequenceActor;

private:
	TWeakObjectPtr<APlayerController> InRangePC;
	bool bSitting = false;
};
