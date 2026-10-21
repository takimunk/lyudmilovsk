#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HandReach.h"
#include "HeroReachComponent.generated.h"

class AHeroCharacter;

/** A thing the hero can pick up: a capsule in the world (a sphere when HalfLength is 0), Axis pointing the way the item's "top" goes. */
USTRUCT()
struct FReachItem
{
	GENERATED_BODY()

	TWeakObjectPtr<AActor> Actor;
	FVector Center = FVector::ZeroVector;
	FVector Axis = FVector::UpVector;
	float HalfLength = 0.f;
	float Radius = 4.f;
	/** From the capsule's centre to the part that goes to the mouth (a bottle's neck tip), cm along Axis; 0 = HalfLength. */
	float MouthDistance = 0.f;
	/** The item's own direction that faces out of its front (a bottle's label); turned toward the viewer when held. */
	FVector Front = FVector::ForwardVector;
};

/**
 * The hero's arm: reaches for any item, closes the fingers on it and carries it, then does something with
 * it. The hand is placed by the animation (HandReach: arm IK, crouch and lean, fingers that stop on the
 * item's surface), so a bottle on a vending machine's tray, an apple in the grass and a book on a desk all
 * use the same calls:
 *
 *   Reach->Take(Item, [](){ item is in the hand: put it in the inventory });
 *   Reach->Drink(Bottle, Seconds, [](float Left){ liquid level }, [](){ done });
 *
 * While a move runs the hero cannot walk or look (the camera is nudged toward what the hand goes for).
 * Nothing here is saved: a move that is cut short by a load just ends with the level.
 */
UCLASS()
class LYUDMILOVSK_API UHeroReachComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeroReachComponent();

	/** The hero's reach component, added on first use. */
	static UHeroReachComponent* Get(AHeroCharacter* Hero);
	static UHeroReachComponent* GetForPlayer(const UObject* WorldContext);

	bool IsBusy() const { return Steps.Num() > 0; }

	/** Reach out for Item (it stays where it is until the fingers close), pick it up and bring it up before the eyes, then OnHeld; the item is in the hand (attached to it). */
	bool Pick(const FReachItem& Item, TFunction<void()> OnHeld);

	/** Pick Item up and put it away: the hand goes to the pocket, the item is destroyed there, then OnStowed. */
	bool Take(const FReachItem& Item, TFunction<void()> OnStowed);

	/**
	 * Bring a bottle from the pocket (Item is spawned in front of the hand by the caller, not yet held), lift it to the mouth, tip it
	 * and drink for Seconds; OnLevel gets the share of the liquid left (1 to 0), OnDone comes when the hand is back down. The item
	 * stays held; the caller destroys or stows it (Stow).
	 */
	bool Drink(const FReachItem& Item, float Seconds, TFunction<void(float)> OnLevel, TFunction<void()> OnDone);

	/** Bring a food item to the mouth and bite it Bites times (OnBite gets the share left). */
	bool Eat(const FReachItem& Item, int32 Bites, TFunction<void(float)> OnBite, TFunction<void()> OnDone);

	/** The held item goes to the pocket (destroyed there) and the hand goes back to the animation. */
	void Stow(TFunction<void()> OnStowed);

	/** Held item: let go where it is (it drops; physics if it has any). */
	void Drop();

	AActor* HeldItem() const { return Held.Get(); }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The reach pose the animation is being fed this frame (for tests). */
	const FHeroReachPose& GetPose() const { return Pose; }

	/** For tests: pose the hero's body meshes as if Reach was given, without running the game. */
	void DebugForcePose(const FHeroReachPose& InPose);

	/**
	 * Headless test of the reach solver: poses Hero (its mesh set, no game running) with the right hand sent to Palm (world,
	 * with FingerDir / PalmNormal, closing on the capsule ShapeCenter/ShapeAxis/HalfLength/Radius) and returns the bones' world
	 * positions as "name x y z" lines, first with the reach, then (after "---") the plain animation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero")
	static FString DebugReachTest(AHeroCharacter* Hero, FVector Palm, FVector FingerDir, FVector PalmNormal, float Curl,
		FVector ShapeCenter, FVector ShapeAxis, float ShapeHalfLength, float ShapeRadius, float LookPitch);

private:
	/** Where the item goes in a step: in the world or relative to the eyes (yaw frame), with an orientation. */
	struct FKey
	{
		bool bHead = false;
		FVector Pos = FVector::ZeroVector;     // world cm, or (forward, right, up) from the eye
		FQuat Rot = FQuat::Identity;           // world, or relative to the yaw frame
		float HandBack = 0.f;                  // cm the palm stays off the item, along -PalmNormal
		float Weight = 1.f;
		float Curl = 0.f;
	};
	struct FStep
	{
		float Duration = 0.5f;
		FKey Target;
		TFunction<void()> OnStart;
		TFunction<void()> OnEnd;
		TFunction<void(float)> OnProgress;    // 0..1 within the step
		bool bAimCamera = false;
	};

	struct FGrip
	{
		FVector PalmLocal, FingerLocal, NormalLocal;
	};

	void Begin(const FReachItem& Item, bool bFromPocket);
	void PlanGrip(const FReachItem& Item);
	FKey CarryKey(const FReachItem& Item, float Weight = 1.f) const;
	FKey MouthKey(const FReachItem& Item, float Tilt, float Weight = 1.f) const;
	FKey PocketKey(const FReachItem& Item) const;
	FQuat HeldRotation(const FReachItem& Item, const FVector& NewUp, const FVector& FaceDir) const;
	FTransform ItemFrame(const FVector& Pos, const FQuat& Rot) const;
	void EvalKey(const FKey& K, FVector& OutItemPos, FQuat& OutItemRot) const;
	void AttachHeld();
	void Finish();
	void PushPose();
	FTransform YawFrame() const;

	TObjectPtr<AHeroCharacter> Hero;
	TArray<FStep> Steps;
	int32 Current = 0;
	float StepTime = 0.f;
	FKey From;
	FReachItem Item;
	FGrip Grip;
	FQuat Item0Rot;
	TWeakObjectPtr<AActor> Held;
	FHeroReachPose Pose;
	bool bLocked = false;
	bool bPoseForced = false;
	float PoseWeight = 0.f;
};
