#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "BirdTame.generated.h"

class ABirdFeedBag;
class AHeroCharacter;
class ASeedHandful;
class SLyudHeldItem;

/**
 * Befriending a bird with feed, from the hero's side. Buy a bag of feed at the vending machine (slot 5), take it in the
 * hand from the inventory (Tab: "Взять в руку"): the hero carries the bag at the hip. F holds the open palm out with a
 * handful of seeds on it (and F again takes it back); C or Ctrl crouches, Shift creeps. The birds themselves decide
 * (UBirdLifeSubsystem: they see how fast the hero moves, how sharply the view turns, whether he is crouched and the hand
 * still): a bird that notices turns, watches, moves its head, may call, and if nothing scares it flies onto the palm and
 * pecks. At the first peck one handful is used up and the camera goes into the bird (UBirdFlightSubsystem::BecomeBird);
 * the hero waits where he crouched until the player comes back. The starlings' murmuration comes to the same hand.
 *
 * Every key is polled, never consumed (E, F, Tab are shared). lyud.Tame 0 switches all of it off (A/B benchmarks).
 * The words are string-table keys (ui.tame.*, ui.held.*); the bird's name is bird.<species_snake>.name.
 */
UCLASS()
class LYUDMILOVSK_API UBirdTameSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBirdTameSubsystem* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UBirdTameSubsystem, STATGROUP_Tickables); }

	/** The item that is bird feed. */
	static const FName FeedId;

	/** Is the palm held out with feed on it (fully out, birds can see it)? */
	bool IsOffering() const;
	/** Hold the hand out or take it back, as F does (false if there is no feed). */
	bool SetOffering(bool bOn);
	/** Where the feed is on the palm, world (valid while offering). */
	FVector OfferLocation() const { return OfferAt; }

	/** The bird that is coming to the hand or eating from it (-1 none). */
	int32 FocusBird() const { return Focus; }

	/** Dev/test: give the hero feed (handfuls) and put it in the hand. */
	void DebugGiveFeed(int32 Handfuls);

	/** Dev/test: move the hero toward a world point at Speed cm/s, crouched or not, the view turning to it at TurnRate deg/s
	 *  (a sharp turn scares birds), stopping StopAt cm short; the scripted approach the offscreen recordings use. */
	void DebugWalkTo(const FVector& Target, float Speed, bool bCrouch, float TurnRate, float StopAt);
	void DebugStop();

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	UFUNCTION() void HandleNotice(int32 BirdId);
	UFUNCTION() void HandleTurn(int32 BirdId);
	UFUNCTION() void HandleWatch(int32 BirdId);
	UFUNCTION() void HandleCall(int32 BirdId);
	UFUNCTION() void HandleApproach(int32 BirdId);
	UFUNCTION() void HandleEat(int32 BirdId);
	UFUNCTION() void HandleFlee(int32 BirdId);
	UFUNCTION() void HandleReturnedToHero(APawn* Hero);

private:
	enum class EPhase : uint8 { Idle, Eating, Bird };

	void BindBirds();
	void UnbindBirds();
	void UpdateHand(AHeroCharacter* Hero, float DeltaTime);
	void UpdateHud(AHeroCharacter* Hero);
	void TellBirds(bool bOn);
	void Say(const TCHAR* Key, int32 BirdId, float Seconds = 3.f);
	FText BirdName(int32 BirdId) const;
	void LogEvent(const TCHAR* What, int32 BirdId) const;
	bool IsNearOffer(int32 BirdId) const;
	void Eaten();
	void Reset();
	void DriveDebugWalk(AHeroCharacter* Hero, float DeltaTime);

	UPROPERTY(Transient) TObjectPtr<ABirdFeedBag> Bag;
	UPROPERTY(Transient) TObjectPtr<ASeedHandful> Handful;
	TSharedPtr<SLyudHeldItem> Hud;

	bool bWantOffer = false;
	bool bApproaching = false;
	bool bTold = false;
	bool bBound = false;
	bool bFlightBound = false;
	EPhase Phase = EPhase::Idle;
	float PhaseTime = 0.f;
	int32 Focus = -1;
	FVector OfferAt = FVector::ZeroVector;
	FVector BirdOfferAt = FVector::ZeroVector;
	float SayLeft = 0.f;
	float LogTimer = 0.f;
	float SpillCooldown = 0.f;

	bool bDebugWalk = false;
	FVector DebugTarget = FVector::ZeroVector;
	float DebugSpeed = 0.f;
	float DebugTurnRate = 90.f;
	float DebugStopAt = 0.f;
};
