#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "AnimalFriends.generated.h"

class AHeroCharacter;
class APlayerController;
class UAnimalLifeSubsystem;
struct FSlateBrush;

/** A cat or a dog that became the hero's friend (the diary's "Friends" pages). */
USTRUCT()
struct LYUDMILOVSK_API FLyudAnimalFriend
{
	GENERATED_BODY()

	/** The animal life's id (the population is the same every game, from the seed). */
	UPROPERTY(SaveGame) int32 AnimalId = -1;
	UPROPERTY(SaveGame) bool bCat = true;
	/** Cat, Laika, Shepherd, Mutt, Maru: its portrait is /Game/UI/Diary/Friends/T_Friend_<Breed>. */
	UPROPERTY(SaveGame) FName Breed;
	UPROPERTY(SaveGame) bool bFemale = false;
	/** The name it already had (animal.maru.name), empty for the nameless. */
	UPROPERTY(SaveGame) FString NameKey;
	/** The name the hero gave it. */
	UPROPERTY(SaveGame) FString Name;
	/** When and where they became friends (UGameCalendar; a map.json place or none), and how: hand (stroked) | food (a treat). */
	UPROPERTY(SaveGame) FDateTime Met;
	UPROPERTY(SaveGame) FName Place;
	UPROPERTY(SaveGame) FName How;
	UPROPERTY(SaveGame) int32 Strokes = 0;
	UPROPERTY(SaveGame) int32 Treats = 0;
	UPROPERTY(SaveGame) int32 Walks = 0;
};

/**
 * The hero's animal friends (docs/animals.md "Friends", docs/diary.md): who, since when, how they met; written on the diary's
 * "Friends" pages (after the birds) with a portrait and the name. Saved with the game ("AnimalFriends"). The bond itself and the
 * given name live in the animal life's own save (FAnimalMemory); this is the diary's record.
 *
 * UAnimalFriendSubsystem writes in it. Console: lyud.Pets.Friends.
 */
UCLASS()
class LYUDMILOVSK_API ULyudAnimalFriends : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static ULyudAnimalFriends* Get(const UObject* WorldContext);
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(SaveGame) TArray<FLyudAnimalFriend> Friends;

	/** A new game: nobody yet. */
	void Reset();
	const FLyudAnimalFriend* Find(int32 AnimalId) const;
	FLyudAnimalFriend* FindMutable(int32 AnimalId);
	/** Adds the friend (false if it was one already). */
	bool NoteFriend(const UObject* WorldContext, const FLyudAnimalFriend& Who, const FVector& Where);
	void SetName(int32 AnimalId, const FString& Name);
	/** Counts for the page: What is Strokes, Treats or Walks. */
	void Count(int32 AnimalId, FName What);

	/** The name as shown: the given one, else its own, else "a cat" / "a dog" by its sex (animal.kind.*). */
	static FText DisplayName(const FLyudAnimalFriend& F);
	/** The portrait for the diary (/Game/UI/Diary/Friends/T_Friend_<Breed>); null when there is none. */
	static const FSlateBrush* Portrait(FName Breed);

	DECLARE_MULTICAST_DELEGATE(FChanged);
	FChanged OnChanged;
};

/**
 * The hero's side of befriending the town's cats and dogs (docs/animals.md "Friends"; the animals' side is the simulation,
 * Private/Animals/AnimalFriendSim.cpp, through UAnimalLifeSubsystem).
 *
 *  - Crouched (C) by an animal, E holds a hand out: one that trusts him comes and sniffs it. E again strokes it (his hand on
 *    its head, sliding back; it leans in, a dog's tail goes, a cat purrs and may roll over). A cat has enough after a few.
 *  - F holds out the best treat in the pocket for that animal (fish for cats, meat for dogs: the table is TreatTable in the
 *    .cpp): it comes, sniffs, eats it off the palm, or leaves what its kind does not eat.
 *  - Strokes and treats grow the bond; at a half it is a friend: a note in the journal, a page in the diary, a name to give.
 *  - A friend dog: E (standing) calls it along; it walks at his heel, waits outside doors, swims after him, sits by his bench,
 *    goes home at night; E again sends it home. A friend cat comes to his lap when he sits near its home (F strokes it there).
 *
 * Reads keys by polling and never consumes them. Sends the quests animal_sniffed|animal_stroked|animal_fed|animal_friend|
 * animal_walk|animal_home|animal_lap <Maru|Cat|Dog> and animal_walk_reach <place>; residents remark on the dog (town talk
 * "dog_walk"). Console: lyud.Pets.Do hand|stroke|feed|walk|home [id], lyud.Pets.Befriend [id], lyud.Pets.Name <id> <name>,
 * lyud.Pets.Friends. lyud.Pets 0 turns the player's side off.
 */
UCLASS()
class LYUDMILOVSK_API UAnimalFriendSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UAnimalFriendSubsystem* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAnimalFriendSubsystem, STATGROUP_Tickables); }

	/** The animal E / F would act on now (-1 none). */
	UFUNCTION(BlueprintPure, Category = "Animals|Friends") int32 GetTarget() const { return Target; }
	/** E belongs to this subsystem now (the name tags' own "[E] call" keeps out of the way). */
	bool TakesE() const { return EAction != EKeyAction::None && EAction != EKeyAction::CrouchHint; }

	/** What E and F do, by name (console, tests, developer mode): hand, stroke, feed, walk, home. Id -1: the target, else the
	 *  nearest animal. False if it could not. Also crouch and stand (the hero's C, for headless checks). */
	UFUNCTION(BlueprintCallable, Category = "Animals|Friends") bool Do(FName What, int32 Id = -1);
	/** Opens the little box to type a friend's name in (the diary's "name it"). */
	UFUNCTION(BlueprintCallable, Category = "Animals|Friends") void BeginNaming(int32 Id);
	UFUNCTION(BlueprintCallable, Category = "Animals|Friends") void SetName(int32 Id, const FString& Name);
	bool IsNaming() const { return NamePopup.IsValid(); }
	/** The nearest animal to the hero within Radius cm (-1 none); bFriendDog: only a dog that is a friend. */
	int32 Nearest(float Radius, bool bFriendDog = false) const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	enum class EKeyAction : uint8 { None, Hand, Stroke, Walk, Home, CrouchHint, Treat, LapStroke };
	enum class EHand : uint8 { None, Offer, Stroke, Food, Lap };

	UFUNCTION() void OnAnimalEvent(int32 AnimalId, FName Event);

	int32 Target = -1;
	EKeyAction EAction = EKeyAction::None;
	EKeyAction FAction = EKeyAction::None;
	// what the prompt line was built from (rebuilt only when one changes)
	int32 ShownTarget = -2;
	EKeyAction ShownE = EKeyAction::None, ShownF = EKeyAction::None;
	FName ShownTreat;
	bool bPromptUp = false;
	float SayLeft = 0.f;

	EHand Hand = EHand::None;
	int32 HandAnimal = -1;
	float HandTime = 0.f;
	float HandLeft = 0.f;
	bool bHandHeld = false;
	FName FoodItem;
	UPROPERTY(Transient) TObjectPtr<AActor> FoodActor;

	FName Treat;            // the best treat in the pocket for the target's kind
	float TreatLikes = 0.f;
	bool bTreatForCat = false;
	float TreatTimer = 0.f;

	bool bHooked = false;
	float SlowTimer = 0.f;
	float RemarkCooldown = 20.f;
	TArray<FName> WalkReached;
	TSharedPtr<class SWidget> NamePopup;
	int32 NamingId = -1;

	void PickTarget(const AHeroCharacter& Hero, const APlayerController& PC);
	void PickActions(const AHeroCharacter& Hero);
	void UpdateTreat(bool bCat);
	void UpdatePrompt();
	void Say(const TCHAR* Key, float Seconds = 3.f);
	bool StartHand(EHand Mode, int32 Id);
	void EndHand();
	void TickHand(AHeroCharacter& Hero, const APlayerController& PC, float DeltaTime);
	void TickWalk(const AHeroCharacter& Hero);
	FVector HeadOf(int32 Id) const;
	bool InSight(const AHeroCharacter& Hero, int32 Id) const;
	void CloseNaming(bool bKeep, const FString& Name);
	FString QuestTarget(int32 Id) const;
};
