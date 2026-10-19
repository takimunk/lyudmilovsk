#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LightFlicker.generated.h"

class ULightComponent;

/**
 * Makes lights on actors tagged FlickerTag blink now and then: a short dip in brightness,
 * sometimes twice, every few seconds per light at random.
 */
UCLASS()
class LYUDMILOVSK_API ALightFlicker : public AActor
{
	GENERATED_BODY()

public:
	ALightFlicker();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flicker")
	FName FlickerTag = TEXT("Flicker");

	/** Seconds between blinks of one light (random within the range). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flicker")
	FVector2D Interval = FVector2D(4.f, 14.f);

	/** Brightness during a blink, as a fraction of the normal intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flicker")
	float DimTo = 0.25f;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	struct FFlickeringLight
	{
		TWeakObjectPtr<ULightComponent> Light;
		float BaseIntensity = 0.f;
		float NextBlink = 0.f;
		TArray<FVector2f> Dips;   // (start, end) times of the current blink pattern
	};
	TArray<FFlickeringLight> Lights;
	float Clock = 0.f;
};
