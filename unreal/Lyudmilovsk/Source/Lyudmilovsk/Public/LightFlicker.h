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

	/**
	 * Lights on actors tagged FarTag fade and flicker harder the further they are along Y from
	 * FarStartY toward FarEndY (the endless corridor): at the end they barely glow and blink all the time.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flicker")
	FName FarTag = TEXT("FarFlicker");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flicker")
	float FarStartY = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flicker")
	float FarEndY = -40000.f;

	/** Seconds between blinks of one light at the far end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flicker")
	FVector2D FarInterval = FVector2D(0.15f, 0.9f);

	/** Brightness of a far-end light when it is on, as a fraction of its normal intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flicker")
	float FarGlow = 0.06f;

	/** Fraction of far-tagged lights that are dead at the very end of the tunnel (it grows with the distance). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flicker")
	float MaxDeadFraction = 0.92f;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	struct FFlickeringLight
	{
		TWeakObjectPtr<ULightComponent> Light;
		float BaseIntensity = 0.f;
		float NextBlink = 0.f;
		bool bDead = false;
		float Far = 0.f;           // 0 near, 1 at the far end
		TArray<FVector2f> Dips;   // (start, end) times of the current blink pattern
	};
	TArray<FFlickeringLight> Lights;
	float Clock = 0.f;
};
