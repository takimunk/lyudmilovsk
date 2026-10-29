#pragma once

#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "HeroShadowMesh.generated.h"

/**
 * The hero's shadow: an invisible copy of the body, head and all, that only casts the shadow. The
 * visible body hides its head for the first-person camera, and a hidden bone hides it from the shadow
 * too, so the body itself casts none and this copy casts it. It runs no animation of its own: every
 * frame, after the body's animation and reach, it takes the body's pose bone for bone, so the shadow
 * never drifts from the body (an own anim instance did, and shaded the chest).
 */
UCLASS()
class LYUDMILOVSK_API UHeroShadowMesh : public UPoseableMeshComponent
{
	GENERATED_BODY()

public:
	UHeroShadowMesh();

	/** Follows Source's mesh and pose from now on. */
	void Follow(USkeletalMeshComponent* Source);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Body;
};
