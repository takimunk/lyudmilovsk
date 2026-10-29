#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "AnimalCollar.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;

/** Where a breed's collar sits (Content/AnimalsData/animal_bodies.json "collar", from blender/animals/collar.py), cm in the breed's frame. */
USTRUCT()
struct LYUDMILOVSK_API FAnimalCollarSpec
{
	GENERATED_BODY()

	/** The bone the cord rides (the meshes are in the bind pose's frame: they follow it). */
	UPROPERTY() FName Bone = TEXT("neck_01");
	/** The point the split ring hangs from (the knot's bottom). */
	UPROPERTY() FVector Hang = FVector::ZeroVector;
	/** From the hanging point down to the tag's centre of mass. */
	UPROPERTY() float TagLength = 2.7f;
	/** The chest under the throat as a sphere the tag rests and knocks against. */
	UPROPERTY() FVector ChestCenter = FVector::ZeroVector;
	UPROPERTY() float ChestRadius = 12.f;
	/** The throat's outward direction (the tag's face turns that way). */
	UPROPERTY() FVector Forward = FVector::ForwardVector;
};

/**
 * Maru's collar (docs/animals.md "Maru"): a thin braided cord round the neck that follows the neck bone, and the split
 * ring with the brass bone tag engraved «Мару» hanging from its knot as a pendulum: gravity and the neck's own movement
 * swing it (a trot sets it bouncing, a turn flings it out), it rests and knocks against her chest, and every knock or
 * jerk hard enough clinks (audio collar_jingle, ULyudSoundscapeSubsystem::PlayEffect), louder for a harder knock.
 * Created by AAnimalActor for a breed with a collar; costs nothing when she isn't drawn.
 */
UCLASS(ClassGroup = Animals, BlueprintType)
class LYUDMILOVSK_API UAnimalCollarComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UAnimalCollarComponent();

	void Setup(USkeletalMeshComponent* InBody, const FAnimalCollarSpec& InSpec, const FString& CordMesh, const FString& TagMesh);

	/** Clinks so far (tests). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collar") int32 Jingles = 0;
	/** The tag's swing off straight down, degrees (tests, the debug line). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collar") float SwingDeg = 0.f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Body;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Cord;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Tag;
	FAnimalCollarSpec Spec;

	bool bHaveState = false;
	FVector Pivot = FVector::ZeroVector;       // world, last step
	FVector PivotVel = FVector::ZeroVector;
	FVector Com = FVector::ZeroVector;         // the tag's centre of mass, world
	FVector ComVel = FVector::ZeroVector;
	float JingleCooldown = 0.f;
	float RestlessTime = 0.f;
	float PivotSpeed = 0.f;          // cm/s, smoothed
	float AirTime = 0.f;             // s since the tag last touched the chest

	void Step(const FVector& NewPivot, const FVector& Chest, float ChestR, const FVector& Out, float Length, float Dt);
	void Jingle(float Strength);
};
