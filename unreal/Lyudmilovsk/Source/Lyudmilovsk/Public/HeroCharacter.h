#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HeroCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;
class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * The player, seen from inside: the hero's whole body (SK_Hero: the Zoomer in Murmuration's clothes)
 * stands in the world and the camera sits at its eyes, as in Murmuration's first person. The head is
 * hidden (the shadow is the headless body's), looking down shows your
 * chest, arms, trousers and shoes. The body turns with the look the way a person does: standing
 * still the head and chest turn first and the feet follow past ~35 degrees, walking it faces where
 * you look. Movement is the template's: WASD / arrows run at 6 m/s, Shift walks, Space jumps, the mouse
 * looks; the template's locomotion clips animate the body.
 *
 * Look, move and jump use the template's own Enhanced Input actions and mapping contexts exactly as
 * TP_FirstPersonCharacter does (same scaling, no smoothing); Shift is read from the key. Other
 * actors' key bindings (E, F, Tab...) keep working alongside.
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

	UPROPERTY(EditAnywhere, Category = "Hero|Input")
	TSoftObjectPtr<UInputAction> MoveAction;
	UPROPERTY(EditAnywhere, Category = "Hero|Input")
	TSoftObjectPtr<UInputAction> LookAction;
	UPROPERTY(EditAnywhere, Category = "Hero|Input")
	TSoftObjectPtr<UInputAction> MouseLookAction;
	UPROPERTY(EditAnywhere, Category = "Hero|Input")
	TSoftObjectPtr<UInputAction> JumpAction;
	/** Added if the player controller hasn't already. */
	UPROPERTY(EditAnywhere, Category = "Hero|Input")
	TArray<TSoftObjectPtr<UInputMappingContext>> MappingContexts;

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
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Hero")
	TObjectPtr<UCameraComponent> Camera;

private:
	void ReadInput(float DeltaSeconds);
	void AddMappingContexts();
	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void TurnBody(float DeltaSeconds);
	void FeedAnimation(float DeltaSeconds);

	float BodyYaw = 0.f;
	double Distance = 0.0;
	double IdleTime = 0.0;
	bool bWantsWalk = false;
};
