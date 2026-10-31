#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Birds/BirdPawn.h"
#include "BirdFlightSubsystem.generated.h"

class ABirdPawn;
class APlayerController;
class ULyudSaveGame;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnBirdFlightChanged, APawn* /*Hero*/);

/**
 * Becoming a bird and back (docs/bird_flight.md). The befriending calls BecomeBird once a bird has eaten
 * from the hand: the bird is claimed from bird life, the camera flies from the hero's eyes into the
 * bird's, and the player controls an ABirdPawn. Holding R flies the camera back to the hero, who never
 * moved; the bird goes back to bird life where it is.
 *
 * Saving as a bird saves the hero where he stands and the bird (species, place, speed); loading puts
 * you straight back into the bird.
 *
 * Developer console: Bird.Become [Species] [air], Bird.Return, Bird.Script <name> [record=<dir>] [quit],
 * Bird.Hero <x> <y> (the hero to a map point outdoors, standing on the ground, metres as world_map.py).
 */
UCLASS()
class LYUDMILOVSK_API UBirdFlightSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** A scripted flight (Bird.Script). */
	struct FScript;

	static UBirdFlightSubsystem* Get(const UObject* WorldContext);

	/** The befriended bird BirdId (bird life) becomes the player. False if it can't be claimed. */
	static bool BecomeBird(APlayerController* PC, int32 BirdId);
	/** True while the player is a bird (the camera flights in and out included). */
	static bool IsBird(const UObject* WorldContext);

	/** Any species at any place, without bird life (developer commands, a loaded game). Shown is an
	 *  actor that showed this bird so far, hidden while you are it. */
	bool BecomeSpecies(APlayerController* PC, FName Species, const FVector& Where, float Yaw, const FVector& VelocityCm, bool bStanding,
	                   AActor* Shown = nullptr, bool bInstant = false);
	/** Back to the hero: the camera flight, or at once. */
	void ReturnToHero(bool bInstant = false);

	ABirdPawn* GetBird() const;
	APawn* GetHero() const { return Hero.Get(); }

	FOnBirdFlightChanged OnBecameBird;
	FOnBirdFlightChanged OnReturnedToHero;

	/** Seconds the camera takes from the hero's eyes into the bird's. */
	static constexpr float ArrivalSeconds = 1.6f;

	/** The hero to a map point (metres, tools/world_map.py frame), on the ground, facing YawDeg. */
	void PlaceHero(float MapX, float MapY, float YawDeg);
	/** Plays a scripted flight after WarmSeconds of wall clock; Record is a folder for one screenshot per
	 *  1/30 s of game time (fixed time step from then on). */
	void RunScript(const FString& Name, const FString& Record, bool bQuitAfter, float WarmSeconds = 0.f);
	/** The number of the next recorded frame (scripts log it to mark a view). */
	int32 GetRecordFrame() const { return RecordFrame; }

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UBirdFlightSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return !IsTemplate(); }

	// a loaded game's bird
	UPROPERTY(SaveGame) bool bSavedAsBird = false;
	UPROPERTY(SaveGame) FName SavedSpecies;
	UPROPERTY(SaveGame) FVector SavedLocation = FVector::ZeroVector;
	UPROPERTY(SaveGame) FVector SavedVelocity = FVector::ZeroVector;
	UPROPERTY(SaveGame) FRotator SavedAim = FRotator::ZeroRotator;
	UPROPERTY(SaveGame) bool bSavedStanding = false;

private:
	void FinishReturn();
	void HandleFate(EBirdFate Fate, const FVector& At);
	void HandleCall();
	void TickEncounter(float Dt);
	void TickHero(float Dt);
	/** The befriended bird comes back to the hero's hand (R held on dry land). */
	bool RecallBird();
	bool IsHeroInWater() const;
	void SetHeroHint(const FText& Text);
	void OnBeforeSave(ULyudSaveGame* Save);
	void OnAfterLoad(ULyudSaveGame* Save);
	void SetHeroHeadShown(bool bShown);
	FMinimalViewInfo HeroView() const;

	TWeakObjectPtr<APlayerController> Player;
	TWeakObjectPtr<APawn> Hero;
	TWeakObjectPtr<ABirdPawn> Bird;
	TWeakObjectPtr<AActor> ShownActor;
	int32 ClaimedId = -1;
	FRotator HeroControl = FRotator::ZeroRotator;
	FDelegateHandle SaveHandle, LoadHandle;

	// the befriended bird, for calling it back once you are the hero again
	FName LastSpecies;
	int32 LastClaimId = -1;
	bool bOurFemale = false;
	float RecallHeld = 0.f, HintShowT = 0.f;
	TSharedPtr<class SWidget> HeroHint;
	TSharedPtr<class STextBlock> HeroHintText;

	// another bird answering the player's call
	enum class EAnswer : uint8 { None, Look, Court, Fight, CallBack };
	struct FEncounter
	{
		int32 Id = -1;
		TWeakObjectPtr<AActor> Actor;
		EAnswer Kind = EAnswer::None;
		float T = 0.f, NextPeck = 0.f;
		int32 Pecks = 0;
		FName Species;
	};
	FEncounter Meet;
	bool bHeroHiddenForReturn = false;
	/** The hero's centre below the water's surface while he swims (the eyes ~20 cm out), cm. */
	static constexpr float HeroFloatDepth = 45.f;

	// a script playing
	TSharedPtr<FScript> Script;
	FString RecordDir;
	int32 RecordFrame = 0;
	bool bQuitAfterScript = false;
	float QuitIn = -1.f;
	double WarmUntil = 0.0;
};
