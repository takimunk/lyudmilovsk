#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CurtainPusher.generated.h"

class UMaterialParameterCollection;

/**
 * Feeds the positions of up to two nearby characters into a material parameter collection, so
 * curtain materials can bend their strips away from whoever walks through (vertex offset).
 */
UCLASS()
class LYUDMILOVSK_API ACurtainPusher : public AActor
{
	GENERATED_BODY()

public:
	ACurtainPusher();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curtain")
	TObjectPtr<UMaterialParameterCollection> Collection;

	/** Only characters closer than this to the curtain count, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curtain")
	float Range = 400.f;

	virtual void Tick(float DeltaSeconds) override;
};
