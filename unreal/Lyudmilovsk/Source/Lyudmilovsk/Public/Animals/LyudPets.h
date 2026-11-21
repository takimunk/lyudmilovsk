#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "LyudPets.generated.h"

class AActor;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Animals that live with the town's people (docs/animals.md "With people", backlog item 46). The behaviour is the animals'
 * plain model (Private/Animals/AnimalPetsSim.cpp, checked by tools/animalsim); who owns what is Content/AnimalsData/pets.json
 * (tools/town/town_pets.py). This ties the two lives together:
 *
 *  * A household's dog is on the leash while its owner's activity is walk_dog or walk_dog_late (UPeopleLifeSubsystem::GetActivityState):
 *    the owner's place goes to FWorld::OwnerWalk every frame, and a thin cable is drawn from the hand to the collar (one
 *    stretched cylinder a dog near the camera; no shadow, no collision, out of Lumen and the distance fields).
 *  * The flats' cats need nothing from here but the clock (the animals' life has it): sill by day, door at feeding time.
 *  * The lady's round: while she is at feed_cats (or feed_cats_late) by a feeder the bowls are down (FWorld::PutFood) and she calls the cats.
 *  * Hedgehogs: drawn from FWorld::Hedgehogs (two meshes: walking, a ball), the month from the calendar.
 *  * "May I stroke it?": UAnimalFriendSubsystem asks here before a hand goes out to a dog on a leash; the owner answers
 *    (lines pet_stroke_yes / pet_stroke_no), yes lets the hand come (FWorld::AllowStroke).
 *
 *  * Yard cats and the townsfolk (docs/animals.md "Town cats"): a cat sees the resident it likes home (friends in pets.json),
 *    cats beg at the market's fish stalls while a fishmonger stands there (a scrap now and then, or a "shoo"), cleaners and
 *    the watchman send a cat out of their way, and the flats' cats keep their sills by the weather (FWorld::Weather).
 *
 * Nothing is saved: the pets are made from pets.json after the animals' life has started, at the end of its list, so the
 * strays' ids (and the friendships saved by them) stay as they were. Console: lyud.Pets.Report, lyud.Pets.Walk <n> (the
 * n-th owner goes out with the dog now), lyud.Pets.Hedgehogs (they come out now, wherever the clock is).
 */
UCLASS()
class LYUDMILOVSK_API ULyudPets : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudPets* Get(const UObject* WorldContext);

	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudPets, STATGROUP_Tickables); }

	/** A dog on a leash whose owner has not been asked yet: the hand's prompt is the question. */
	bool NeedsAsking(int32 AnimalId) const;
	/** The hero asks the owner; true: yes (the dog lets the hand come for a while). The owner says the answer. */
	bool AskToStroke(int32 AnimalId);
	/** The resident who owns the animal (people's id), -1 nobody. */
	int32 OwnerOf(int32 AnimalId) const;
	FString Report() const;
	/** Console: the n-th dog's owner is sent out for the walk now. */
	bool SendOut(int32 Index);
	/** Console: hedgehogs out whatever the hour (till the next map). */
	bool bHedgehogsNow = false;

private:
	struct FDog
	{
		int32 Animal = -1;
		int32 Person = -1;
		FVector LastAt = FVector::ZeroVector;
		bool bOut = false;
		double NextTalk = 0.0;
		int32 AskedDay = -1;        // the day the owner last said no
	};
	struct FCat
	{
		int32 Animal = -1;
		int32 Person = -1;
	};
	/** A resident a yard cat likes: it sees them home (FWorld::FavouriteHome). */
	struct FCatFriend
	{
		int32 Person = -1;
		int32 Animal = -1;
		FVector Door = FVector::ZeroVector;
		FVector LastAt = FVector::ZeroVector;
		bool bSeen = false;
		bool bWith = false;
		double NextSay = 0.0;
	};
	/** A fish stall of the market: open while a fishmonger stands at it (FWorld::Stalls). */
	struct FFishStall
	{
		int32 Stall = -1;
		FVector At = FVector::ZeroVector;
		int32 Seller = -1;          // who stands there now
		double NextThrow = 0.0;
	};
	TArray<FCatFriend> CatFriends;
	TArray<FFishStall> FishStalls;
	TArray<int32> Fishmongers;
	/** Cleaners and the watchman, and the activities at which they send a cat out of the way. */
	TArray<int32> Shooers;
	TArray<int32> ShooActs;
	double NextTownCats = 0.0;
	double NextShooSay = 0.0;
	TArray<FDog> Dogs;
	TArray<FCat> Cats;
	int32 Lady = -1;
	/** The owners' walks and the lady's rounds (activities.json ids: the morning's and the evening's are two activities). */
	TArray<int32> WalkActs;
	TArray<int32> FeedActs;
	bool bLoaded = false;
	bool bTried = false;
	double NextSlow = 0.0;
	double NextSill = 0.0;
	double NextHog = 0.0;
	double NextCall = 0.0;

	UPROPERTY(Transient) TObjectPtr<AActor> Holder;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Leashes;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Hogs;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> HogMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> HogBall;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Cylinder;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> LeashMat;

	bool Load();
	UStaticMeshComponent* NewPart(UStaticMesh* Mesh);
	void TickDogs(float DeltaTime, const FVector& Camera);
	void TickLady();
	/** The yard cats and the townsfolk, four times a second: the weather on the sills, who is seen home, the fish stalls, the brooms. */
	void TickTownCats(float Dt);
	void Say(int32 Person, const TCHAR* Event);
	void TickHedgehogs(const FVector& Camera);
	void TickRemarks(const FVector& HeroAt);
};
