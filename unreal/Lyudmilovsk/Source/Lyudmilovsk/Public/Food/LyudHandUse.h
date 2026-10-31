#pragma once

#include "CoreMinimal.h"
#include "HeroReachComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudHandUse.generated.h"

/** How a thing in the hand was taken to the lips: a click (a small bite, a short puff), a hold (a big bite, a long drag), held on (eating
 *  on and on, drawing on and on: a use every SteadyEvery seconds until the button goes up). */
UENUM()
enum class ELyudMouthUse : uint8
{
	Small,
	Big,
	Steady,
};

/**
 * One thing the hero holds in the right hand and takes to the mouth with the left mouse button: food (bites), a cigarette
 * (puffs), a cup (sips). The caller spawns the actor and fills this in; ULyudHandUse carries it, plays the hand and calls back.
 * Frames: the palm's frame is X along the fingers (wrist to knuckles), Z out of the palm, origin the palm's middle on the skin;
 * look frames are (forward, right, up) from the eyes, turned with the look's yaw (and PitchFollow of its pitch).
 */
struct FLyudHandThing
{
	TWeakObjectPtr<AActor> Actor;
	/** The inventory item it is (for the prompt and OnUsed listeners); the caller keeps the pocket. */
	FName Item;
	/** Where the actor sits in the palm's frame (a corn cob across the palm, a cigarette between two fingers). */
	FTransform InPalm = FTransform::Identity;
	/** What the fingers close on, in the actor's frame (empty: the actor's visible bounds). */
	FReachShape Shape;
	/** The point (actor frame) that goes to the lips when it is whole and when it is nearly used up: it moves as the bites eat in. */
	FVector MouthFull = FVector::ZeroVector;
	FVector MouthEmpty = FVector::ZeroVector;
	/** The hand while carrying it (walking and looking stay free), and the palm's turn at the mouth: Mouth's Pos is not used, the
	 *  hand goes where MouthPoint meets the lips. */
	FHeroHoldPose Carry;
	FHeroHoldPose Mouth;
	/** Shares of the whole a click, a hold and each steady use take, and a steady use every SteadyEvery seconds. */
	float Small = 0.08f;
	float Big = 0.18f;
	float SteadyEach = 0.06f;
	float SteadyEvery = 0.8f;
	/** Seconds at the lips for a click's and a hold's use (the hold's tears off its piece). */
	float SmallAtLips = 0.2f;
	float BigAtLips = 0.6f;
	/** Seconds the hand takes to the mouth and back. A press still down when the hand gets there is a hold. */
	float ToMouth = 0.34f;
	float FromMouth = 0.42f;
	/** Pushed this far into the mouth at a use (cm, along the look): a bite goes in, a cigarette barely touches. */
	float Push = 1.4f;
	/** How much of it is left (1 whole, 0 used up); uses never take more than this. */
	float Left = 1.f;
	/** At the lips, at the moment of each use: what kind, the share it took and what is left after (sounds, bite marks, smoke). */
	TFunction<void(ELyudMouthUse /*Kind*/, float /*Took*/, float /*Left*/)> OnUse;
	/** Every frame while it is at the lips (a cigarette's ember glowing on a drag); optional. */
	TFunction<void(float /*DeltaTime*/)> OnAtLips;
	/** Its last bit is used (Left 0): the actor is destroyed and the hand goes down. */
	TFunction<void()> OnUsedUp;
	/** It goes away before it is used up (PutAway, another thing in the hand, the level ends): the actor is destroyed; keep the rest. */
	TFunction<void(float /*Left*/)> OnPutAway;
	/** The prompt line while it is in the hand (the keys); empty: "LMB — bite · hold — eat · Q — put away". */
	FText Prompt;
};

/**
 * The thing in the hero's right hand that is used at the mouth with the left mouse button, for anything: food, cigarettes, a cup
 * at the diner. A click takes it to the lips for a small use, a hold for a big one, and holding on keeps it there using it on
 * and on until the button goes up. Q (or PutAway) puts it away. The hand is the reach component's override hold (exact in the
 * look's frame, so a bite lands on the lips however fast the view turns); the actor rides on the hand bone.
 *
 *   FLyudHandThing T; T.Actor = Spawned; T.InPalm = ...; T.MouthFull = ...; T.OnUse = [](ELyudMouthUse K, float Took, float Left) { ... };
 *   ULyudHandUse::Get(this)->Hold(MoveTemp(T));
 *
 * Food from the pocket is LyudFood's (ULyudFoodSubsystem: an equipped Food item comes into the hand by itself). The hand gives way
 * to everything else: while a reach move runs or a screen is up the thing is hidden and waits; the diary taking the hand puts it
 * away. Nothing is saved here (the food keeps its own Left in the pocket).
 *
 * Console (checks without a mouse): lyud.Hand.Press 1|0 holds or lets go of the button, lyud.Hand.Click, lyud.Hand.PutAway.
 * Log lines "HANDUSE ..." for tests.
 */
UCLASS()
class LYUDMILOVSK_API ULyudHandUse : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudHandUse* Get(const UObject* WorldContext);

	/** Puts Thing in the hand (whatever was there is put away first). False without a hero or an actor. */
	bool Hold(FLyudHandThing Thing);
	/** The thing in the hand goes away (OnPutAway with what is left); the hand goes back to the animation. */
	void PutAway();
	bool IsHolding() const { return bHas; }
	FName HeldItem() const { return bHas ? Thing.Item : FName(); }
	AActor* HeldActor() const { return bHas ? Thing.Actor.Get() : nullptr; }
	float Left() const { return bHas ? Thing.Left : 0.f; }
	/** At the lips now (a use running or held on). */
	bool IsAtLips() const { return Phase == EPhase::Use || Phase == EPhase::Steady; }
	/** The button, for scripts and tests: down until Release (OR'ed with the real mouse button). */
	void Press() { bForced = true; }
	void Release() { bForced = false; }
	/** A click: down for one frame. */
	void Click() { bClickQueued = true; }
	/** Where the thing touches the lips (world), for sounds. */
	FVector LipsLocation() const;

	/** Every use of every thing: its item, the kind, what is left. */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FUsed, FName /*Item*/, ELyudMouthUse /*Kind*/, float /*Left*/);
	FUsed OnUsed;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudHandUse, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bHas; }
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	enum class EPhase : uint8 { Carry, ToLips, Use, Steady, FromLips };

	FLyudHandThing Thing;
	bool bHas = false;
	EPhase Phase = EPhase::Carry;
	/** 0 carried .. 1 at the lips. */
	float AtLips = 0.f;
	float PhaseTime = 0.f;
	float PressTime = 0.f;
	float SteadyTime = 0.f;
	ELyudMouthUse Kind = ELyudMouthUse::Small;
	bool bWasDown = false;
	bool bForced = false;
	bool bClickQueued = false;
	bool bClickDown = false;
	bool bAttached = false;
	bool bHidden = false;
	bool bPrompted = false;
	/** The hand has been in the carry hold since (s): the thing shows once the hand is up. */
	float HoldTime = 0.f;
	/** The last frame's lips point (world). */
	FVector Lips = FVector::ZeroVector;

	bool ButtonDown(const class APlayerController* PC);
	void Use(ELyudMouthUse InKind);
	void Drop(bool bUsedUp);
	void SetHidden(bool bHide);
	void SetPrompted(bool bOn);
	FHeroHoldPose PoseNow(float Push) const;
	FVector MouthPoint() const;
};
