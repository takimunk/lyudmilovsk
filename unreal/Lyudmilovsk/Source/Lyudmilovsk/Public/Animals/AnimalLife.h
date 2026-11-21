#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "AnimalLife.generated.h"

namespace LyudAnimals { class FWorld; }
class AAnimalActor;

UENUM(BlueprintType)
enum class EAnimalKind : uint8 { Cat, Dog };

/** What a cat or dog is doing (the simulation's acts, Private/Animals/AnimalSim.h EAct, in the same order). */
UENUM(BlueprintType)
enum class EAnimalActivity : uint8
{
	Sleep, Rest, Sit, Groom, Stand, Walk, Trot, Run, Sniff, Eat, Watch, Approach, Greet, Beg, Rub, PlayBow, Play, Stalk, Crouch, Pounce,
	Chase, Flee, Climb, Perch, Hiss, Bark, Follow,
	/** With a friend (docs/animals.md "Friends"): nose at the hand held out, stroked, belly up, asleep on the lap, in the water. */
	SniffHand, Petted, RollOver, Lap, Swim
};

USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FAnimalInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Animals") int32 Id = -1;
	UPROPERTY(BlueprintReadOnly, Category = "Animals") EAnimalKind Kind = EAnimalKind::Cat;
	/** Cat, Laika, Shepherd, Mutt, Maru (the model: /Game/Animals/<Breed>). */
	UPROPERTY(BlueprintReadOnly, Category = "Animals") FName Breed;
	/** Its coat (blender/animals/coats.py), e.g. tabby, ginger, black_white. */
	UPROPERTY(BlueprintReadOnly, Category = "Animals") FName Coat;
	UPROPERTY(BlueprintReadOnly, Category = "Animals") bool bFemale = false;
	UPROPERTY(BlueprintReadOnly, Category = "Animals") bool bYoung = false;
	UPROPERTY(BlueprintReadOnly, Category = "Animals") FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Animals") FVector Velocity = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Animals") float Yaw = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Animals") EAnimalActivity Activity = EAnimalActivity::Rest;
	/** Why (Sleep, Eat, Bask, Roam, Visit, Play, Hunt, Person, Rest, Refuge). */
	UPROPERTY(BlueprintReadOnly, Category = "Animals") FName Goal;
	/** Needs, 0 fine .. 1 urgent. */
	UPROPERTY(BlueprintReadOnly, Category = "Animals") float Hunger = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Animals") float Tired = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Animals") float Playful = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Animals") float Lonely = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Animals") float Curious = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Animals") float Fear = 0.f;
	/** 0..1 toward the player: grows when the player is calm near it. */
	UPROPERTY(BlueprintReadOnly, Category = "Animals") float Trust = 0.f;
	/** 0..1 friendship: grown by stroking and treats (a friend from 0.5: UAnimalLifeSubsystem::IsFriend). */
	UPROPERTY(BlueprintReadOnly, Category = "Animals") float Bond = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Animals") bool bFriend = false;
	/** Its name as shown: the one the player gave it, else its own (NameKey in the player's language), else empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Animals") FString Name;
	/** The localization key of the name it already has (animal.maru.name), empty for the nameless. */
	UPROPERTY(BlueprintReadOnly, Category = "Animals") FString NameKey;
	UPROPERTY(BlueprintReadOnly, Category = "Animals") bool bNoticedPlayer = false;
	/** Its body near the camera (AAnimalActor), null far away. */
	UPROPERTY(BlueprintReadOnly, Category = "Animals") TObjectPtr<AActor> Actor = nullptr;
};

/** What the save keeps per animal (by id: the population is the same every game, from the seed). */
USTRUCT()
struct LYUDMILOVSK_API FAnimalMemory
{
	GENERATED_BODY()

	UPROPERTY(SaveGame) float Trust = 0.f;
	UPROPERTY(SaveGame) float Bond = 0.f;
	UPROPERTY(SaveGame) FString Name;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAnimalLifeEvent, int32, AnimalId, FName, Event);

/**
 * The town's cats and dogs (docs/animals.md): ~44 cats round the houses (a third of them at the Academgorodok) and ~19 dogs
 * (four packs with their dens, yard dogs, the dog by the club's door). Each has needs (food, sleep, play, company,
 * novelty), a temper, a home and a range, and lives by the game's clock: cats out at dawn and dusk and about at night,
 * dogs in the mornings and evenings. They meet each other (packmates greet and play, strangers stare, posture, bark or
 * hiss and one gives way; a dog goes for a cat it doesn't know and the cat runs up a tree, or stands its ground), they
 * stalk and chase the birds (bird life flees from them, UBirdLifeSubsystem::SetBeasts), and they mind the player:
 * notice, watch, run from a runner, come up to someone calm (a cat rubs their legs, a hungry dog begs).
 *
 * The simulation is plain C++ (Private/Animals/AnimalSim.cpp; tools/animalsim runs it outside the engine). The ground
 * they walk on and their places come from Content/AnimalsData (tools/animal_map.py). Near the camera the animals are
 * AAnimalActor bodies (furred, LODs); far ones aren't drawn and think less often.
 *
 * Friends (docs/animals.md "Friends"): a hand held out and sniffed, stroking, treats grow FAnimalInfo::Bond; a friend dog walks
 * at the player's heel (Follow), waits outside doors, swims after them, sits by their bench; a friend cat comes to sleep on
 * their lap. The model is Private/Animals/AnimalFriendSim.cpp; the player's keys, hand and diary are UAnimalFriendSubsystem.
 * Starts by itself in Lvl_Scene01 (or any map with lyud.Animals.AnyMap 1); lyud.Animals 0 turns it off.
 */
UCLASS()
class LYUDMILOVSK_API UAnimalLifeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UAnimalLifeSubsystem();
	virtual ~UAnimalLifeSubsystem() override;

	static UAnimalLifeSubsystem* Get(const UObject* WorldContext);
	/** For Blueprints and Python: unreal.AnimalLifeSubsystem.get_animal_life(world). */
	UFUNCTION(BlueprintPure, Category = "Animals", meta = (WorldContext = "WorldContext"))
	static UAnimalLifeSubsystem* GetAnimalLife(const UObject* WorldContext) { return Get(WorldContext); }

	UFUNCTION(BlueprintCallable, Category = "Animals")
	bool GetAnimal(int32 Id, FAnimalInfo& Out) const;

	/** Ids of the animals within Radius (cm) of Location, nearest first. */
	UFUNCTION(BlueprintCallable, Category = "Animals")
	TArray<int32> FindAnimalsNear(FVector Location, float Radius) const;

	/** A loud noise at Location (cm): cats within Radius run, dogs bark or run. */
	UFUNCTION(BlueprintCallable, Category = "Animals")
	void Scare(FVector Location, float Radius, float Strength = 1.f);

	/**
	 * One animal at Location (cm) that lives right there (tests): Breed Cat, Laika, Shepherd, Mutt or Maru, Coat its index.
	 * Works in any level (an empty world without the town's map). Console: lyud.Animals.SpawnNear <Breed> [coat] [x y z].
	 */
	UFUNCTION(BlueprintCallable, Category = "Animals")
	int32 SpawnAnimal(FName Breed, int32 Coat, FVector Location, bool bYoung = false);

	/** The player speaks to it (E, AAnimalNameTags): a dog turns to them and barks back gladly (OnAnimalEvent BarkHappy),
	 *  a cat looks and meows. False when it can't (running away, up a tree). Console: lyud.Animals.Hail [id]. */
	UFUNCTION(BlueprintCallable, Category = "Animals")
	bool Hail(int32 Id);

	/** The name the player gave it (kept with the save). */
	UFUNCTION(BlueprintCallable, Category = "Animals")
	void SetAnimalName(int32 Id, const FString& InName);

	// --- friends (docs/animals.md "Friends"; the player's side of it is UAnimalFriendSubsystem) ---

	/** The player (crouched) holds a hand out to it: true if it comes to sniff it (HandSniff), false if it will not yet (HandRefuse). */
	UFUNCTION(BlueprintCallable, Category = "Animals|Friends")
	bool OfferHand(int32 Id);
	/** One stroke (a few seconds): only one that has sniffed the hand, or a friend. EndStroke: the hand left early. */
	UFUNCTION(BlueprintCallable, Category = "Animals|Friends")
	bool StrokeAnimal(int32 Id);
	UFUNCTION(BlueprintCallable, Category = "Animals|Friends")
	void EndStroke(int32 Id);
	/** Food on the palm (SetPlayerHand with bOffer): Like 0..1 is how much its kind wants that food. It comes, sniffs, eats (Ate) or leaves it. */
	UFUNCTION(BlueprintCallable, Category = "Animals|Friends")
	bool OfferFood(int32 Id, float Like);
	/** A friend dog comes along at the player's heel (one at a time; not at night); EndWalk leaves it there or sends it home. */
	UFUNCTION(BlueprintCallable, Category = "Animals|Friends")
	bool StartWalk(int32 Id);
	UFUNCTION(BlueprintCallable, Category = "Animals|Friends")
	void EndWalk(bool bHome);
	/** The dog walking with the player, the cat on (or coming to) their lap; -1 none. */
	UFUNCTION(BlueprintPure, Category = "Animals|Friends")
	int32 Walker() const;
	UFUNCTION(BlueprintPure, Category = "Animals|Friends")
	int32 LapCat() const;
	UFUNCTION(BlueprintPure, Category = "Animals|Friends")
	bool IsFriend(int32 Id) const;
	/** Where the player's hand is held out (world, cm) and whether food lies on it; bSet false: the hand is down again. */
	void SetPlayerHand(bool bSet, const FVector& Palm = FVector::ZeroVector, bool bOffer = false);
	/** Its body near the camera, null far away (no strings made, unlike GetAnimal). */
	AAnimalActor* BodyOf(int32 Id) const;
	/** Tests and developer mode: makes it a friend at once (Befriended is sent like any other time). */
	UFUNCTION(BlueprintCallable, Category = "Animals|Friends")
	void Befriend(int32 Id);

	UFUNCTION(BlueprintPure, Category = "Animals")
	bool IsRunning() const { return Sim.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Animals")
	int32 NumAnimals() const;

	/** One line: how many do what (console lyud.Animals.Report). */
	UFUNCTION(BlueprintCallable, Category = "Animals")
	FString Report() const;

	/** A line about one animal (needs, act, goal, place). */
	UFUNCTION(BlueprintCallable, Category = "Animals")
	FString Describe(int32 Id) const;

	/** Notice, Watch, Approach, Flee, Bark, Growl, Whine, Meow, Purr, Hiss, Pounce, ChaseStart, Climb, Greet, PlayStart, BarkHappy. */
	UPROPERTY(BlueprintAssignable, Category = "Animals") FAnimalLifeEvent OnAnimalEvent;

	UPROPERTY(SaveGame) TMap<int32, FAnimalMemory> Saved;

	LyudAnimals::FWorld* GetSim() const { return Sim.Get(); }
	/** Runs the simulation headless over game hours (no frames), writing tools/animalsim's day CSV (render_animals.py). */
	void DumpDay(float StartHour, float Hours, const FString& OutCsv);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bStarted || bWantStart; }

private:
	TPimplPtr<LyudAnimals::FWorld> Sim;
	UPROPERTY(Transient) TObjectPtr<class AAnimalLifeRenderer> Renderer;
	UPROPERTY(Transient) TObjectPtr<class AAnimalNameTags> NameTags;
	bool bWantStart = false;
	bool bSaveHooked = false;
	void HookSave();
	bool bStarted = false;
	float StartDelay = 0.f;
	int32 SunDay = -1;
	bool bClockSeen = false;
	float PreyTimer = 0.f;
	float SaveTimer = 0.f;
	struct FSightCache { double Time = -1.0; bool bClear = true; };
	TMap<uint64, FSightCache> Sight;
	TMap<int32, double> ProbeTime;
	bool bHandSet = false;
	bool bHandOffer = false;
	FVector HandPalm = FVector::ZeroVector;

	bool Start();
	bool StartEmpty(float GroundZ);
	void Stop();
	void AddRealTrees();
	void UpdateClock();
	void UpdatePlayer();
	void LinkBirds(float DeltaTime);
	void DispatchEvents();
	void SyncMemory(bool bToSave);
	void InstallCallbacks();
};

/** Draws the animal life: a pool of AAnimalActor bodies for the animals nearest the camera (within their draw distance). */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API AAnimalLifeRenderer : public AActor
{
	GENERATED_BODY()

public:
	AAnimalLifeRenderer();

	void Draw(const LyudAnimals::FWorld& World, const FVector& Camera);
	AAnimalActor* BodyOf(int32 Id) const;

	int32 BodiesLastFrame = 0;

private:
	UPROPERTY(Transient) TMap<int32, TObjectPtr<AAnimalActor>> Bodies;
	UPROPERTY(Transient) TArray<TObjectPtr<AAnimalActor>> Spare;

	void Drive(AAnimalActor* Body, const LyudAnimals::FWorld& World, int32 Id);
};
