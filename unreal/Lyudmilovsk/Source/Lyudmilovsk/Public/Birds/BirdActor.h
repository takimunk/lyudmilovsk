#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Birds/BirdSpecies.h"
#include "BirdActor.generated.h"

class USkeletalMeshComponent;
class UBirdAnimInstance;
class UAnimSequence;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBirdActionFinished, ABirdActor*, Bird, EBirdState, State);

/**
 * One bird up close: its species' skeletal mesh (/Game/Birds/<Id>/SK_<Id>, four LODs by screen size) and clips, played
 * by state. It only animates: whoever owns it (bird life, befriending, the bird's own flight) moves the actor and says
 * what the bird is doing. Forward +X, up +Z, the origin between the feet when perched.
 *
 *   SetState(Fly) flaps at the species' wingbeat rate and, by its flight style, mixes in the dips of tits and finches,
 *   the glides of thrushes, gulls, pigeons and swifts. SetVelocity scales hop / walk to the ground speed (no foot
 *   sliding) and the wingbeat to climbing. SetLookTarget aims the head (fast, in small jerks like a bird's).
 *   One-shot states (Peck Eat Preen Call Takeoff Land) end in OnActionFinished and go on by themselves
 *   (Takeoff -> Fly, Land -> Perch, the rest back to what the bird was doing).
 *   SetFlightInput is for a bird the player flies: wings and tail follow the controls instead of the flight style.
 *
 * Winter plumage (species.json winterMonths) is picked from the game calendar on SetSpecies; Sex picks the female's plumage,
 * size and tail where they differ.
 * Console: Bird.Spawn <Id> [state] spawns one in front of the camera (testing).
 */
UCLASS()
class LYUDMILOVSK_API ABirdActor : public AActor
{
	GENERATED_BODY()

public:
	ABirdActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bird")
	TObjectPtr<USkeletalMeshComponent> Body;

	/** Species id (species.json); set before BeginPlay or call SetSpecies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bird")
	FName Species = TEXT("Starling");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bird")
	EBirdState InitialState = EBirdState::Perch;

	/** Male or female: her plumage where it differs (species.json dimorphism), her size, a swallow's shorter streamers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bird")
	EBirdSex Sex = EBirdSex::Male;

	/** The player's bird: the detailed body SK_<Id>_Hero (~3x the feathers, 4096 atlas) when the species has one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bird")
	bool bHeroBody = false;

	/** Random phase / pauses (birds of one flock must not move in lockstep). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bird")
	int32 Seed = 0;

	UPROPERTY(BlueprintAssignable, Category = "Bird")
	FOnBirdActionFinished OnActionFinished;

	UFUNCTION(BlueprintCallable, Category = "Bird")
	bool SetSpecies(FName Id);

	UFUNCTION(BlueprintCallable, Category = "Bird")
	void SetSex(EBirdSex NewSex);

	UFUNCTION(BlueprintCallable, Category = "Bird")
	void SetState(EBirdState NewState, float BlendTime = 0.15f);

	UFUNCTION(BlueprintPure, Category = "Bird")
	EBirdState GetState() const { return State; }

	/** World velocity, cm/s. */
	UFUNCTION(BlueprintCallable, Category = "Bird")
	void SetVelocity(FVector WorldVelocity) { Velocity = WorldVelocity; }

	/** Aim the head at a world point (bActive false lets it go back to the clip's own head). */
	UFUNCTION(BlueprintCallable, Category = "Bird")
	void SetLookTarget(FVector WorldPoint, bool bActive = true);

	/** Opens the beak and calls once (bird life plays the sound). */
	UFUNCTION(BlueprintCallable, Category = "Bird")
	void PlayCall() { SetState(EBirdState::Call); }

	/** A bird the player flies: Flap 0..1 (0 glides), Fold 0..1 (wings in: a dive), Bank and Pitch -1..1. Call every
	 *  frame while flying; the clip mix follows. bPlayerControl false hands the wings back to the flight style. */
	UFUNCTION(BlueprintCallable, Category = "Bird")
	void SetFlightInput(float Flap01, float Fold01, float Bank, float Pitch, bool bPlayerControl = true);

	/** The bird's eyes: the middle of eye_l and eye_r, looking where the head looks. */
	UFUNCTION(BlueprintPure, Category = "Bird")
	FTransform GetEyeTransform() const;

	UFUNCTION(BlueprintPure, Category = "Bird")
	const FBirdSpecies& GetSpeciesData() const { return Data; }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	FBirdSpecies Data;
	EBirdState State = EBirdState::Perch;
	EBirdState Resume = EBirdState::Perch;
	FVector Velocity = FVector::ZeroVector;
	bool bLookActive = false;
	FVector LookWorld = FVector::ZeroVector;
	FVector LookShown = FVector::ZeroVector;   // component space, where the head is pointed now
	float LookWeight = 0.f;
	float LookHold = 0.f;                        // the head stays put this long, then jerks to the target
	float LookTilt = 0.f;
	bool bPlayer = false;
	float InFlap = 1.f, InFold = 0.f, InBank = 0.f, InPitch = 0.f;
	float FlightPhase = 0.f;                     // the flight style's burst / pause cycle
	float FlightSegment = 0.f;
	bool bFlightBurst = true;
	float StateTime = 0.f;
	FRandomStream Rng;

	UPROPERTY(Transient) TMap<FName, TObjectPtr<UAnimSequence>> Clips;

	UBirdAnimInstance* Anim() const;
	UAnimSequence* Clip(FName Name) const;
	/** The species' materials for this sex and season (MI_<Id>_<Kind>[_Female][Winter] when there are such). */
	void ApplyPlumage();
	void StartClip(EBirdState S, float BlendTime);
	void TickFlight(float DeltaSeconds);
	static bool IsOneShot(EBirdState S);
};
