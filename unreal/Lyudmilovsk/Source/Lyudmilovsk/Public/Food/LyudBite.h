#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudBite.generated.h"

class AActor;
class UMaterialInterface;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

/** What the hand eats with. */
UENUM()
enum class ELyudUtensil : uint8
{
	/** A fork from the plate to the mouth; each bite brings a load on its tines (two noodles, a twirl). */
	Fork,
	/** The same with a spoon (soup, porridge). */
	Spoon,
	/** The dish itself is in the hand and bitten (a pie slice, a piece of cheese). */
	Hand,
	/** The dish is a cup or a glass in the hand: a bite is a sip. */
	Cup,
	/** Chopsticks from the bowl to the mouth (the restaurant's ramen): like a fork, the load hangs under the tips. */
	Chopsticks,
	/** Fingers: a piece is torn off the dish and brought up in the hand (bread, khachapuri); the load sits between the fingers. */
	Fingers,
};

UENUM()
enum class ELyudBiteSize : uint8
{
	/** A click: two noodles on the fork, a sip. */
	Small,
	/** A held click: a full fork, a gulp. Holding on keeps eating. */
	Big,
};

/**
 * A meal the hero eats (ULyudBiteSubsystem::BeginMeal). The caller owns the dish: it says where it is and changes its look in OnBite.
 * Shared mechanic of every food in the game (the diner, the cheese stall): keep this struct small and stable.
 */
struct FLyudMeal
{
	/** The item eaten (LyudInventory.cpp id): the quests' "use" event and the sounds (S_<Item>_<kind> before the common S_<kind>). */
	FName Item;
	/** The plate, bowl or cup in the world. Fork/Spoon: bites are taken from above its origin. Hand/Cup: it goes into the hand. */
	TWeakObjectPtr<AActor> Dish;
	ELyudUtensil Utensil = ELyudUtensil::Fork;
	/** How much is left, 1 = whole; each bite takes SmallBite or BigBite of the whole. */
	float Amount = 1.f;
	float SmallBite = 0.07f;
	float BigBite = 0.18f;
	/** Fork/Spoon: what rides on it after a scoop (null: an engine sphere tinted LoadTint). Its own origin sits on the tines. */
	UStaticMesh* LoadMesh = nullptr;
	/** A big bite's load if it looks different (a twirl of spaghetti; null: LoadMesh, larger). */
	UStaticMesh* LoadMeshBig = nullptr;
	UMaterialInterface* LoadMaterial = nullptr;
	FLinearColor LoadTint = FLinearColor(0.9f, 0.75f, 0.4f);
	/** Fork/Spoon/Chopsticks: the utensil (null: /Game/Food/Meshes/SM_Fork or SM_Spoon). Origin at the grip, tip along +X. */
	UStaticMesh* UtensilMesh = nullptr;
	/** Where the load rides from the utensil's grip (cm, its frame; X the tip); the default is the fork's tines. */
	FVector LoadOffset = FVector(11.5f, 0.f, 0.6f);
	/** Hand/Cup: the dish's radius and half height (cm) the fingers close on, and where its rim is above its origin (Cup). */
	float DishRadius = 4.f;
	float DishHalfHeight = 5.f;
	/** Called as each bite reaches the mouth with the amount left (0 the last bite). */
	TFunction<void(float /*Left*/, ELyudBiteSize)> OnBite;
	/** The meal ended: eaten up (Left 0) or the hero walked off / EndMeal (Left > 0). Hand/Cup: the dish is let go of first. */
	TFunction<void(float /*Left*/)> OnEnd;
};

/**
 * Eating, the same everywhere: a meal begins (BeginMeal) and the left mouse button eats it. Read by polling, never consumed.
 *
 *   click        a small bite (the hand goes down, takes a little, brings it to the mouth, a soft bite and chewing)
 *   hold         a big bite (decided while the hand goes down: still held after ClickSeconds)
 *   keep holding the next big bite as soon as the chewing allows, until the dish is empty
 *
 * The right hand is posed through UHeroReachComponent's override hold (fingers closed on the fork or the cup); the hero can look
 * around meanwhile; walking more than LeaveDistance away ends the meal (the fork goes back on the plate). Sounds: /Game/Audio/Food/
 * S_<kind>_<nn> (kinds bite_small, bite_big, chew, sip, gulp, clink, scoop; S_<Item>_<kind>_<nn> wins when it exists), played softly.
 */
UCLASS()
class LYUDMILOVSK_API ULyudBiteSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudBiteSubsystem* Get(const UObject* WorldContext);

	/** Starts a meal (ends the one before); false when the hero cannot (no hero, a reach is running). */
	bool BeginMeal(const FLyudMeal& Meal);
	/** Stops eating: the hand goes back, OnEnd with what is left. */
	void EndMeal();
	bool IsEating() const { return bActive; }
	float AmountLeft() const { return Meal.Amount; }
	/** 0..1 through the scoop (the fork in the food: a knife saws then), -1 in any other phase. */
	float ScoopPhase() const;
	/** The dish being eaten (null when idle). */
	AActor* MealDish() const { return bActive ? Meal.Dish.Get() : nullptr; }
	FName MealItem() const { return Meal.Item; }

	/** Takes one bite as if the button was clicked (Small) or held (Big): tests and scripted scenes. */
	void TakeBite(ELyudBiteSize Size);

	/** Every bite that reached the mouth (item, size, amount left): quests, the soundscape, tests. */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnBite, FName /*Item*/, ELyudBiteSize, float /*Left*/);
	FOnBite OnBite;

	static constexpr float ClickSeconds = 0.25f;
	static constexpr float LeaveDistance = 120.f;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudBiteSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bActive; }
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	enum class EPhase : uint8 { Rest, Down, Scoop, Up, Mouth, Back };

	FLyudMeal Meal;
	bool bActive = false;
	EPhase Phase = EPhase::Rest;
	float PhaseT = 0.f;
	/** Seconds the button has been down in this bite (the size is decided by it), and a bite queued by TakeBite. */
	float PressT = 0.f;
	bool bPressed = false;
	bool bQueued = false;
	ELyudBiteSize QueuedSize = ELyudBiteSize::Small;
	ELyudBiteSize Size = ELyudBiteSize::Small;
	float ChewLeft = 0.f;
	int32 BiteCount = 0;
	FVector StartLocation = FVector::ZeroVector;
	/** The hand's key poses in the yaw frame (forward, right, up from the eyes): where it starts a phase and where it goes. */
	FTransform From, To;
	FTransform Current;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Utensil;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> UtensilMeshC;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> LoadC;
	/** Sounds by kind (kept here: unreferenced sounds get collected while they play). */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USoundBase>> Sounds;
	/** The meal's meshes and material, held for the meal: FLyudMeal is a plain struct the GC doesn't see, and a caller may
	 * pass assets nothing else references (they'd be collected mid-meal and the next load swap would use a freed mesh). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> MealAssets;

	FTransform YawFrame() const;
	FTransform KeyFor(EPhase P) const;
	void StartPhase(EPhase P, float Seconds);
	float PhaseSeconds(EPhase P) const;
	void PoseHand(float DeltaTime);
	void ReachMouth();
	void Finish(bool bEaten);
	void SpawnUtensil();
	void Play(const TCHAR* Kind, float Volume = 0.55f);
	USoundBase* FindSound(const FString& Kind);
};
