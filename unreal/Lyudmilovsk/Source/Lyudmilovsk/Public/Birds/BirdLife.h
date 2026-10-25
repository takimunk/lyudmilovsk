#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "BirdLife.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;
class USoundBase;
namespace LyudBirds { class FWorld; }

/** What a wild bird is doing (the bird life's own states; ABirdActor's EBirdState is the body's animation). */
UENUM(BlueprintType)
enum class EBirdActivity : uint8
{
	Sleep, Perch, Preen, Forage, Peck, Swim, Watch, Takeoff, Fly, Land, Hawk, Soar, Flee, Chase, Mob, Approach, Eat, Claimed
};

USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FBirdLifeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Birds") int32 Id = -1;
	/** Row of Content/BirdsData/species.json (UBirdSpeciesLibrary). */
	UPROPERTY(BlueprintReadOnly, Category = "Birds") FName SpeciesId;
	UPROPERTY(BlueprintReadOnly, Category = "Birds") FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Birds") FVector Velocity = FVector::ZeroVector;
	/** Where the head points (unit vector). */
	UPROPERTY(BlueprintReadOnly, Category = "Birds") FVector HeadDirection = FVector::ForwardVector;
	UPROPERTY(BlueprintReadOnly, Category = "Birds") EBirdActivity Activity = EBirdActivity::Perch;
	/** 0 calm .. 1 flies off. */
	UPROPERTY(BlueprintReadOnly, Category = "Birds") float Alertness = 0.f;
	/** 0 wild .. 1 eats from the hand. */
	UPROPERTY(BlueprintReadOnly, Category = "Birds") float Trust = 0.f;
	/** Has it noticed the player (and is it watching them)? */
	UPROPERTY(BlueprintReadOnly, Category = "Birds") bool bNoticed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Birds") bool bWatching = false;
	/** Half the birds of each species are females (fixed per bird). */
	UPROPERTY(BlueprintReadOnly, Category = "Birds") bool bFemale = false;
	/** The bird's body near the player (the species thread's ABirdActor), when it has one; null far away. */
	UPROPERTY(BlueprintReadOnly, Category = "Birds") TObjectPtr<AActor> Actor = nullptr;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBirdLifeEvent, int32, BirdId);

/**
 * The wild birds of the outdoor map (docs/bird_life.md): ~1100 birds of 29 species that live where their
 * species would (gulls on the sea and the beach, pigeons and sparrows in the town, tits and robins in the
 * woods, rooks on the fields, martins over the cliff), keep their day by the game calendar (wake, feed,
 * rest at midday, gather and go to roost at dusk, sleep), give way to bigger birds, steal food (gulls,
 * crows), flee and mob hawks, and perceive the player: they notice, turn their head, watch, call and fly
 * off when the player runs, walks straight at them, turns the view sharply or moves the hand fast. Held
 * out food (SetOffer) and patience make a bird trust the player until it comes to the palm and eats.
 *
 * The simulation is plain C++ (Private/Birds/BirdSim.cpp, also run outside the engine by tools/birdsim).
 * LOD: birds near the camera think every frame, far ones every 0.5-2 s and move in coarse steps; birds
 * are drawn with instanced poses (ABirdLifeRenderer) and only within a distance that grows with their size.
 *
 * Starts by itself in Lvl_Scene01 (or any map with lyud.Birds.AnyMap 1); lyud.Birds 0 turns it off.
 */
UCLASS()
class LYUDMILOVSK_API UBirdLifeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UBirdLifeSubsystem();
	virtual ~UBirdLifeSubsystem() override;

	static UBirdLifeSubsystem* Get(const UObject* WorldContext);
	/** Get for Blueprints and Python: unreal.BirdLifeSubsystem.get_bird_life(world). */
	UFUNCTION(BlueprintPure, Category = "Birds", meta = (WorldContext = "WorldContext"))
	static UBirdLifeSubsystem* GetBirdLife(const UObject* WorldContext) { return Get(WorldContext); }

	/** Food held out in the hand: call every frame while it is (palm, world cm), and once with bOn false when it goes. */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	void SetOffer(FVector PalmLocation, FName Food, bool bOn);

	/** Something the player did that a bird minds (a dropped item, a shout): 0..2, fades in a second. */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	void AddPlayerThreat(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Birds")
	bool GetBird(int32 Id, FBirdLifeInfo& Out) const;

	/** Ids of the birds within Radius (cm) of Location, nearest first. */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	TArray<int32> FindBirdsNear(FVector Location, float Radius) const;

	/** The player becomes this bird (bird flight): it leaves the simulation; others still see it. Returns its body actor if it has one. */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	AActor* ClaimBird(int32 Id);

	/** While claimed: where the bird is (world cm, cm/s). */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	void SetClaimedBirdLocation(int32 Id, FVector Location, FVector Velocity);

	/** Gives the bird back to the wild (it flies to its flock). */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	void ReleaseBird(int32 Id);

	/** A sudden noise at Location (cm): birds within Radius (cm) start, the nearest fly. */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	void Scare(FVector Location, float Radius, float Strength = 1.f);

	/**
	 * Puts one calm bird of a species (species.json id) on the ground at Location (cm) that keeps to that spot unless
	 * scared (then it comes back a while later). Works in any level: without the outdoor map the bird life starts
	 * empty for it. Console: lyud.Birds.SpawnNear <Species> <x> <y> <z>. Returns its id, -1 if the species is unknown.
	 */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	int32 SpawnBird(FName Species, FVector Location);

	UFUNCTION(BlueprintPure, Category = "Birds")
	bool IsRunning() const { return Sim.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Birds")
	int32 NumBirds() const;

	/** Tests: for Seconds the birds see the player run from From to To (cm) at Speed (cm/s), whatever the pawn does. */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	void TestRun(FVector From, FVector To, float Speed, float Seconds);

	/** It noticed the player (its head comes round). */
	UPROPERTY(BlueprintAssignable, Category = "Birds") FBirdLifeEvent OnBirdNotice;
	/** It has turned to look at the player. */
	UPROPERTY(BlueprintAssignable, Category = "Birds") FBirdLifeEvent OnBirdTurn;
	/** It stopped what it was doing to watch the player (uneasy, or curious about the food). */
	UPROPERTY(BlueprintAssignable, Category = "Birds") FBirdLifeEvent OnBirdWatch;
	/** A call (contact, alarm, song); the species' sound plays at the bird when there is one. */
	UPROPERTY(BlueprintAssignable, Category = "Birds") FBirdLifeEvent OnBirdCall;
	/** It is coming to the food in the hand. */
	UPROPERTY(BlueprintAssignable, Category = "Birds") FBirdLifeEvent OnBirdApproach;
	/** First peck on the palm. */
	UPROPERTY(BlueprintAssignable, Category = "Birds") FBirdLifeEvent OnBirdEat;
	UPROPERTY(BlueprintAssignable, Category = "Birds") FBirdLifeEvent OnBirdFlee;

	/** Trust per bird id, kept with the save. */
	UPROPERTY(SaveGame) TMap<int32, float> SavedTrust;

	/** One line: how many birds do what, where (console: lyud.Birds.Report). */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	FString Report() const;
	/** Runs the simulation headless over game hours (no frames), writing a day CSV like tools/birdsim (for render_birds.py). */
	void DumpDay(float StartHour, float Hours, const FString& OutCsv);
	LyudBirds::FWorld* GetSim() const { return Sim.Get(); }

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bStarted || bWantStart; }

private:
	TPimplPtr<LyudBirds::FWorld> Sim;
	UPROPERTY(Transient) TObjectPtr<class ABirdLifeRenderer> Renderer;
	bool bWantStart = false;
	bool bStarted = false;
	float StartDelay = 0.f;
	int32 SunDay = -1;
	bool bClockSeen = false;
	FVector TestRunFeet = FVector::ZeroVector, TestRunTarget = FVector::ZeroVector;
	float TestRunSpeed = 0.f;
	double TestRunUntil = -1.0;
	FRotator LastView = FRotator::ZeroRotator;
	float TurnRate = 0.f;
	FVector LastPalm = FVector::ZeroVector;
	double LastOfferTime = -1.0;
	bool bOffer = false;
	float HandSpeed = 0.f;
	FVector HandVelocity = FVector::ZeroVector;
	float SaveTimer = 0.f;
	struct FSightCache { double Time = -1.0; bool bClear = true; };
	TMap<int32, FSightCache> Sight;
	TMap<FName, TArray<TObjectPtr<USoundBase>>> Sounds;
	UPROPERTY(Transient) TObjectPtr<class USoundAttenuation> Attenuation;
	int32 SoundsThisSecond = 0;
	double SoundSecond = 0.0;

	bool Start();
	bool StartEmpty(float GroundZ);
	void FillSpeciesFromTable();
	void Stop();
	void AddRealTrees();
	void UpdateClock();
	void UpdatePlayer(float DeltaTime);
	void DispatchEvents();
	void PlayCall(int32 Id);
	void SyncTrust(bool bToSave);
};

/** Draws the bird life: one instanced mesh per species and pose, near birds as ABirdActor bodies (when the species has them). */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ABirdLifeRenderer : public AActor
{
	GENERATED_BODY()

public:
	ABirdLifeRenderer();

	/** Builds the components for the species of the simulation. */
	void Setup(const LyudBirds::FWorld& World);
	/** Places every visible bird's instance (and the bodies near the camera). */
	void Draw(const LyudBirds::FWorld& World, const FVector& Camera, float PixelScale);
	AActor* BodyOf(int32 Id) const;
	/** Debug: a drawn instance of each species. */
	FString Describe() const;
	/** Promotes a bird to a body actor now (approach, claim). */
	AActor* EnsureBody(const LyudBirds::FWorld& World, int32 Id);
	void ReleaseBody(int32 Id);

	int32 DrawnLastFrame = 0;
	int32 BodiesLastFrame = 0;

	static constexpr int32 NumPoses = 7;   // Perch, Peck, Sleep, Flap0, Flap1, Flap2, Glide

private:
	struct FSpeciesDraw
	{
		/** NumPoses components for males, then (species with a female plumage) NumPoses for females. */
		TArray<TObjectPtr<UInstancedStaticMeshComponent>> Poses;
		TArray<TArray<FTransform>> Xf;
		/** Size relative to the mesh (placeholder meshes are a 21.5 cm starling). */
		float Scale = 1.f;
		/** Origin of the flight poses above the feet, cm. */
		float CenterUp = 0.f;
		bool bOwnMeshes = false;
		bool bHasBody = false;
		float FemaleScale = 1.f;
	};
	TArray<FSpeciesDraw> Species;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> PlaceholderMeshes;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
	UPROPERTY(Transient) TMap<int32, TObjectPtr<AActor>> Bodies;
	UClass* BodyClass = nullptr;
	/** Per body: the act it was last told (so clips are not restarted every frame). */
	TMap<int32, float> BodyActs;
	TArray<int32> Promoted;

	void Sync(UInstancedStaticMeshComponent* ISM, TArray<FTransform>& Arr);
	void DriveBody(AActor* Body, const LyudBirds::FWorld& World, int32 Id);
};
