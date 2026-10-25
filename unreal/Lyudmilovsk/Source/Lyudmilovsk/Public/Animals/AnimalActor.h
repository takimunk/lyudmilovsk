#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Animals/AnimalLife.h"
#include "AnimalActor.generated.h"

class USkeletalMeshComponent;
class UAnimalAnimInstance;
class UAnimSequence;

/**
 * A cat or dog up close: its breed's furred skeletal mesh (/Game/Animals/<Breed>/SK_<Breed>, four LODs: fur shells
 * near, none far) in one of the breed's coats, playing its clips by what it does. It only animates; the animal life
 * (or a test) moves it and says what it does. Forward +X, up +Z, the origin on the ground between the feet.
 *
 *   SetActivity picks the clip; moving, the gait follows the speed (walk, trot, gallop, a cat's stalk), each played at
 *   speed / its stride speed so the feet don't slide. SetLookTarget turns the head; Bark/Meow/Hiss open the mouth;
 *   SetMood sets the tail's wag and the ears (a wagging dog, a cat's flat ears and lashing tail).
 * Console: Animal.Spawn <Breed> [coat] [activity] spawns one in front of the camera (testing).
 */
UCLASS()
class LYUDMILOVSK_API AAnimalActor : public AActor
{
	GENERATED_BODY()

public:
	AAnimalActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animal")
	TObjectPtr<USkeletalMeshComponent> Body;

	/** Cat, Laika, Shepherd, Mutt; set before BeginPlay or call SetBreed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal")
	FName Breed = TEXT("Cat");

	/** The coat's index in the breed's list (Content/AnimalsData/animal_bodies.json). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal")
	int32 Coat = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal")
	EAnimalActivity InitialActivity = EAnimalActivity::Stand;

	/** Overall scale (a kitten, a small dog of its breed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal")
	float Size = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal")
	int32 Seed = 0;

	UFUNCTION(BlueprintCallable, Category = "Animal")
	bool SetBreed(FName InBreed, int32 InCoat);

	/** What it does now, and its ground speed (cm/s) for the gait. */
	UFUNCTION(BlueprintCallable, Category = "Animal")
	void SetActivity(EAnimalActivity Activity, float SpeedCms = 0.f);

	UFUNCTION(BlueprintPure, Category = "Animal")
	EAnimalActivity GetActivity() const { return Act; }

	UFUNCTION(BlueprintCallable, Category = "Animal")
	void SetLookTarget(FVector WorldPoint, bool bActive = true);

	/** Tail and ears: Excite 0..1 (a dog's wag, a cat's lashing tail), Fear 0..1 (ears back), Friendly 0..1 (a dog's soft wag). */
	UFUNCTION(BlueprintCallable, Category = "Animal")
	void SetMood(float Excite, float Fear, float Friendly);

	/** A bark, a meow, a hiss, a growl: the mouth opens with it. */
	UFUNCTION(BlueprintCallable, Category = "Animal")
	void Vocalize(FName Sound);

	UFUNCTION(BlueprintPure, Category = "Animal")
	bool IsCat() const { return Breed == TEXT("Cat"); }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	EAnimalActivity Act = EAnimalActivity::Stand;
	float Speed = 0.f;
	FName PlayingClip;
	bool bLookActive = false;
	FVector LookWorld = FVector::ZeroVector;
	float LookWeight = 0.f;
	float Excite = 0.f, Fear = 0.f, Friendly = 0.5f;
	float JawPulse = 0.f, JawHold = 0.f;
	float WagPhase = 0.f;
	float Pant = 0.f;
	FRandomStream Rng;
	TMap<FName, float> ClipSpeeds;     // cm/s of the locomotion clips (stride / cycle)

	UPROPERTY(Transient) TMap<FName, TObjectPtr<UAnimSequence>> Clips;

	UAnimalAnimInstance* Anim() const;
	UAnimSequence* Clip(FName Name) const;
	void ApplyCoat();
	void UpdateClip(float BlendTime);
	FName ClipFor(EAnimalActivity A) const;
};
