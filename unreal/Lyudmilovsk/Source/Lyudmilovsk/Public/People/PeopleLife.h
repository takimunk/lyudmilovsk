#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "PeopleLife.generated.h"

namespace LyudPeople { class FWorld; }
class ABoothOwner;
class ANpcBody;
class USkeletalMeshComponent;

/** How someone looks now (the simulation's poses, Private/People/PeopleSim.h EPose, in the same order). */
UENUM(BlueprintType)
enum class EPersonPose : uint8 { Hidden, Stand, Walk, Talk, Look, Sit, Computer, Eat, Read, Sleep, Work };

USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FPersonInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "People") int32 Id = -1;
	/** owner, clerk, streamer, ... : npc.<key>.name in the strings. */
	UPROPERTY(BlueprintReadOnly, Category = "People") FString Key;
	/** The model: /Game/Characters/Humans/<Mesh>/SK_<Mesh>. */
	UPROPERTY(BlueprintReadOnly, Category = "People") FName Mesh;
	UPROPERTY(BlueprintReadOnly, Category = "People") int32 Age = 0;
	UPROPERTY(BlueprintReadOnly, Category = "People") FString Occupation;
	UPROPERTY(BlueprintReadOnly, Category = "People") FString Household;
	/** Their flat (e.g. Novikova1-2-17), empty if homeless. */
	UPROPERTY(BlueprintReadOnly, Category = "People") FString Home;
	/** What they do (activities.json id) and where (place id). */
	UPROPERTY(BlueprintReadOnly, Category = "People") FString Activity;
	UPROPERTY(BlueprintReadOnly, Category = "People") FString Place;
	/** idle, going, riding, settling, doing, leaving, away. */
	UPROPERTY(BlueprintReadOnly, Category = "People") FString State;
	UPROPERTY(BlueprintReadOnly, Category = "People") EPersonPose Pose = EPersonPose::Stand;
	UPROPERTY(BlueprintReadOnly, Category = "People") FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "People") float Yaw = 0.f;
	/** hunger, tired, social, fun, nature, mind, sun: 0 fine .. 1 urgent. */
	UPROPERTY(BlueprintReadOnly, Category = "People") TMap<FName, float> Needs;
	/** Who they talk to now (-1 nobody). */
	UPROPERTY(BlueprintReadOnly, Category = "People") int32 Partner = -1;
	/** What shows them now: their body walking about, the placed club actor at their station, the owner's booth actor. */
	UPROPERTY(BlueprintReadOnly, Category = "People") TObjectPtr<AActor> Actor = nullptr;
};

/** What the save keeps per person (by key). */
USTRUCT()
struct LYUDMILOVSK_API FPersonMemory
{
	GENERATED_BODY()

	UPROPERTY(SaveGame) TArray<float> Needs;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPeopleLifeEvent, int32, PersonId, FName, Event, int32, Other);

/**
 * The town's people (docs/people.md): everybody who sits in the club has a life: a profile (name, age, occupation,
 * household, temper), needs (food, sleep, company, fun, fresh air, the mind, the sun), a flat of their own in the
 * Academgorodok (reserved for their household and kept with the save) and a day by the game's clock: sleep, school
 * or work, and free time in which they choose for themselves among the activities of Content/PeopleData/activities.json
 * (the club and its computers, the library, walks, benches, the sunset, talking to each other, a meal at home, town).
 * Slots (beds, chairs, computers, benches) are reserved so nobody takes a place somebody else has. They walk the
 * town's walk grid, the Academgorodok's stairs and flats and the club's corridor, and ride the cable car between the
 * valley and the club's spur.
 *
 * The simulation is plain C++ (Private/People/PeopleSim.cpp; tools/peoplesim runs it outside the engine). Near the
 * camera a person is an ANpcBody (their MakeHuman model with the shared clips); at their own station in the club the
 * level's placed actor shows them (as before), the owner at his booth is ABoothOwner (as before).
 * Starts by itself in Lvl_Scene01; lyud.People 0 turns it off (the club as it was).
 */
UCLASS()
class LYUDMILOVSK_API UPeopleLifeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UPeopleLifeSubsystem();
	virtual ~UPeopleLifeSubsystem() override;

	static UPeopleLifeSubsystem* Get(const UObject* WorldContext);
	/** For Blueprints and Python: unreal.PeopleLifeSubsystem.get_people_life(world). */
	UFUNCTION(BlueprintPure, Category = "People", meta = (WorldContext = "WorldContext"))
	static UPeopleLifeSubsystem* GetPeopleLife(const UObject* WorldContext) { return Get(WorldContext); }

	UFUNCTION(BlueprintPure, Category = "People")
	bool IsRunning() const { return Sim.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "People")
	int32 NumPeople() const;

	UFUNCTION(BlueprintCallable, Category = "People")
	bool GetPerson(int32 Id, FPersonInfo& Out) const;

	/** By key (owner, clerk, streamer, ...); -1 if there is nobody of that key. */
	UFUNCTION(BlueprintPure, Category = "People")
	int32 FindPerson(const FString& Key) const;

	/** Sends someone to do something now (activities.json id), at a place (its id) or wherever suits them. */
	UFUNCTION(BlueprintCallable, Category = "People")
	bool Order(int32 Id, const FString& Activity, const FString& PlaceId = TEXT(""));

	/** Moves a household into a flat (its id, e.g. Novikova1-2-17); false if it is not free. Kept with the save. */
	UFUNCTION(BlueprintCallable, Category = "People")
	bool SetHome(const FString& Household, const FString& FlatId);

	/** The flats and who lives in them (id -> residents' keys), for the housing view and tests. */
	UFUNCTION(BlueprintCallable, Category = "People")
	TMap<FString, FString> Flats() const;

	/** One line: who does what (console lyud.People.Report). */
	UFUNCTION(BlueprintCallable, Category = "People")
	FString Report() const;

	UFUNCTION(BlueprintCallable, Category = "People")
	FString Describe(int32 Id) const;

	/** Start, Arrive, Leave, TalkStart, TalkEnd, Ride, Away, Back. */
	UPROPERTY(BlueprintAssignable, Category = "People") FPeopleLifeEvent OnPersonEvent;

	/** Household -> flat id. */
	UPROPERTY(SaveGame) TMap<FString, FString> Homes;
	UPROPERTY(SaveGame) TMap<FString, FPersonMemory> Memory;

	LyudPeople::FWorld* GetSim() const { return Sim.Get(); }

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bStarted || bWantStart; }

private:
	TPimplPtr<LyudPeople::FWorld> Sim;
	bool bWantStart = false;
	bool bStarted = false;
	bool bLoaded = false;
	float StartDelay = 0.f;
	double LastSeconds = 0.0;
	bool bClockSeen = false;
	int32 SunDay = -1;
	float SaveTimer = 0.f;
	float DialogueTimer = 0.f;

	/** One body per person, made when first needed. */
	UPROPERTY(Transient) TArray<TObjectPtr<ANpcBody>> Bodies;
	/** The club's placed actors (the scene's people at their stations), by station tag. */
	UPROPERTY(Transient) TMap<FString, TObjectPtr<AActor>> ClubActors;
	UPROPERTY(Transient) TObjectPtr<ABoothOwner> Owner;
	TSet<int32> Talkable;

	bool Start();
	void Stop();
	/** The hour, weekday and sun from the game's calendar; true if it jumped (everybody resettled). */
	bool UpdateClock(bool bFirst);
	void BindClub();
	void Present(float DeltaTime);
	void DispatchEvents();
	void SyncMemory(bool bToSave);
	void ApplySaved();
	ANpcBody* BodyFor(int32 Id);
	void ShowPlaced(AActor* Actor, bool bShow);
};

/**
 * One person's body when it isn't the level's own actor: their MakeHuman model (all on SK_Human_Skeleton) driven by
 * the people's life: walks (the shared walk clip at their speed), stands, talks, sits down and stands up, eats, reads,
 * lies down and sleeps, sits at a computer with their own seated clip. Hidden far from the camera, indoors out of reach.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ANpcBody : public AActor
{
	GENERATED_BODY()

public:
	ANpcBody();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "People")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	/** A blanket over them asleep: draped over the body's bones once they lie still (BuildBlanket). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "People")
	TObjectPtr<class UStaticMeshComponent> Blanket;

	/** Loads the model and remembers whose clips to use. False if the model is missing. */
	bool Setup(const FString& MeshName, const FString& ComputerClip);
	/** Pose this frame: where (cm; seated: the floor under the seat, lying: under the mattress's middle), facing (deg; lying:
	 *  the bed's head to foot), the pose, the transition (1 sit down, 2 stand up, 3 lie down, 4 get up), the walking speed
	 *  (cm/s) and the seat's or mattress's height (cm, 0 = the clips' own). */
	void Drive(const FVector& Location, float Yaw, EPersonPose Pose, int32 Transit, float Speed, float SeatHeight, float DeltaTime, bool bSnap);

	FString MeshName;

private:
	UPROPERTY(Transient) TMap<FName, TObjectPtr<class UAnimSequence>> Clips;
	FString ComputerClip;
	FName Playing;
	float Facing = 0.f;
	bool bPlaced = false;
	/** Body size against the body the shared clips are baked on (Guy_a: pelvis height / 93.7 cm). */
	float BodyScale = 1.f;
	/** Standing, looking and talking take turns among a few clips (a nod, folded arms) so nobody loops one gesture. */
	FName Role;
	float Clock = 0.f;
	float VariantUntil = 0.f;
	uint32 Seed = 0;

	UPROPERTY(Transient) TObjectPtr<class UStaticMesh> BlanketMesh;
	float StillAsleep = 0.f;
	bool bBlanket = false;
	/** The bed: the floor under the mattress's middle, head-to-foot yaw, mattress top (cm over that floor). */
	FVector BedCenter = FVector::ZeroVector;
	float BedYaw = 0.f;
	float BedTop = 50.f;

	class UAnimSequence* Clip(FName Name);
	void Play(FName Name, bool bLoop, float Rate = 1.f);
	/** A duvet over the sleeper: a height field over the bed shaped by capsules round the bones, from the shoulders to past the feet. */
	void BuildBlanket();
	/** Plays one of the role's variants (the role's own clip most of the time), picking again every 6-12 s. */
	void PlayVaried(FName InRole, std::initializer_list<FName> Variants, float Rate = 1.f);
};
