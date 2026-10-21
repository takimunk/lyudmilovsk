#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HeroCharacter.generated.h"

class UCameraComponent;
class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * The player, seen from inside: the hero's whole body (SK_Hero: the Zoomer in Murmuration's clothes)
 * stands in the world and the camera sits at its eyes, as in Murmuration's first person. The head is
 * hidden (a second, invisible copy of the body still casts the full shadow), looking down shows your
 * chest, arms, trousers and shoes. The body turns with the look the way a person does: standing
 * still the head and chest turn first and the feet follow past ~35 degrees, walking it faces where
 * you look. Movement is the template's: WASD / arrows run at 6 m/s, Shift walks, Space jumps, the mouse
 * looks; the template's locomotion clips animate the body.
 *
 * Input is read straight from the keys (no Enhanced Input actions), so interactables that bind E
 * and the game menu work as before.
 */
UCLASS(config = Game)
class LYUDMILOVSK_API AHeroCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHeroCharacter();

	UPROPERTY(EditAnywhere, Category = "Hero")
	TSoftObjectPtr<USkeletalMesh> BodyMesh;

	/** How far the shoes' thick soles reach below the body's floor (the mesh is lifted by this), cm. */
	UPROPERTY(EditAnywhere, Category = "Hero")
	float SoleDrop = 4.4f;

	UPROPERTY(EditAnywhere, Category = "Hero|Movement")
	float WalkSpeed = 290.f;

	UPROPERTY(EditAnywhere, Category = "Hero|Movement")
	float RunSpeed = 600.f;

	/** Degrees of turn per mouse count. */
	UPROPERTY(EditAnywhere, Config, Category = "Hero|Look")
	float LookSensitivity = 0.175f;

	UPROPERTY(EditAnywhere, Category = "Hero|Look")
	float PitchMin = -80.f;

	UPROPERTY(EditAnywhere, Category = "Hero|Look")
	float PitchMax = 83.f;

	/** Horizontal field of view; looking steeply down it widens by FovLookDownExtra (the eye sees its
	 *  own chest and shoes in a far wider field than the screen's). */
	UPROPERTY(EditAnywhere, Category = "Hero|Look")
	float Fov = 90.f;

	UPROPERTY(EditAnywhere, Category = "Hero|Look")
	float FovLookDownExtra = 16.f;

	/** The eye point in the world (between the eyes, after the animation). */
	FVector EyeLocation() const;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void CalcCamera(float DeltaTime, struct FMinimalViewInfo& OutResult) override;
	virtual void PossessedBy(AController* NewController) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Hero")
	TObjectPtr<UCameraComponent> Camera;

	/** The full body, invisible, only for the shadow (the visible one has no head). */
	UPROPERTY(VisibleAnywhere, Category = "Hero")
	TObjectPtr<USkeletalMeshComponent> ShadowMesh;

private:
	void ReadInput(float DeltaSeconds);
	void TurnBody(float DeltaSeconds);
	void FeedAnimation(float DeltaSeconds);

	float BodyYaw = 0.f;
	double Distance = 0.0;
	double IdleTime = 0.0;
	bool bWantsWalk = false;
	bool bJumpHeld = false;
};
