#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyudSave.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "BirdFeeder.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

/** The bird feeds' table (Content/BirdsData/feeds.json): how much each species wants each feed. */
namespace LyudBirdFeeds
{
	/** Is Id one of the feeds in the table? */
	LYUDMILOVSK_API bool IsFeed(FName Id);
	/** How much a species (species.json id) wants a feed, 0..1; Unknown for a feed that is not in the table (none: nothing held). */
	LYUDMILOVSK_API float Like(FName Feed, FName Species, float Unknown = 1.f);
	/** The same for every species of the bird life, by its index there (64 slots). */
	LYUDMILOVSK_API void LikeTable(FName Feed, float Out[64], float Unknown = 1.f);
}

/**
 * A wooden bird feeder the hero put up (docs/bird_taming.md "Feeds and the feeder"): a square tray with a low rim on a
 * post, four corner posts holding a gabled plank roof, built from the engine's cube like the feed bag. Origin at the foot
 * of the post on the ground, local X toward the side it was set up from. The feed in it lies as a heap on the tray.
 * Spawned and kept by UBirdFeederSubsystem (NotPlaceable).
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ABirdFeeder : public AActor
{
	GENERATED_BODY()

public:
	ABirdFeeder();

	virtual void BeginPlay() override;

	/** Shows Handfuls of Feed on the tray (a heap in the feed's colour; nothing when empty). */
	void ShowFeed(FName Feed, float Handfuls);

	/** The middle of the tray's floor, world. */
	FVector TrayLocation() const { return GetActorTransform().TransformPosition(FVector(0.f, 0.f, TrayTop)); }

	/** Height of the tray's floor over the ground, its half width, cm. */
	static constexpr float TrayTop = 112.f;
	static constexpr float TrayHalf = 25.f;

private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Post;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Tray;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Wood;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Roof;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Heap;
	FName Shown;
	int32 ShownGrains = -1;
};

/** One feeder in the save: where it stands and what is in it. */
USTRUCT()
struct FBirdFeederState
{
	GENERATED_BODY()

	UPROPERTY(SaveGame) FVector Location = FVector::ZeroVector;
	UPROPERTY(SaveGame) float Yaw = 0.f;
	UPROPERTY(SaveGame) FName Feed;
	UPROPERTY(SaveGame) float Handfuls = 0.f;
	/** When this feed was first put in (game calendar): the birds round about find it a couple of hours later. */
	UPROPERTY(SaveGame) FDateTime FilledAt;
};

/**
 * The hero's bird feeders. "Поставить" on the feeder in the diary sets one up on the ground in front of the hero; at a
 * feeder, E with a feed in the hand pours a handful in (another feed throws the old one out), E with an empty hand takes
 * the feeder back (with the whole handfuls left). Each feeder is a tray in the bird life (UBirdLifeSubsystem::AddFeeder):
 * between 1.5 and 2.5 game hours after a feed went in, the birds whose range it is in, who feed in that kind of place and
 * like that feed (Content/BirdsData/feeds.json) come and peck; eating there with the hero calm near by they come to trust
 * him, so the palm (UBirdTameSubsystem) wins them over quickly. What they eat goes from the feeder.
 *
 * Kept with the save ("Feeders"). E is polled, never consumed. Console: lyud.Feeder.Place, lyud.Feeder.Fill <feed> [n],
 * lyud.Feeder.Age <hours> (as if the feed went in that long ago), lyud.Feeder.Report.
 */
UCLASS()
class LYUDMILOVSK_API UBirdFeederSubsystem : public UTickableWorldSubsystem, public ILyudSaveable
{
	GENERATED_BODY()

public:
	static UBirdFeederSubsystem* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UBirdFeederSubsystem, STATGROUP_Tickables); }

	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;

	/** The item that is a feeder. */
	static const FName FeederItem;
	/** Handfuls a feeder holds. */
	static constexpr float Capacity = 5.f;

	/** Sets a feeder from the pocket up in front of the hero; false (and a note) when there is no feeder or no room. */
	bool PlaceFromPocket();
	/** Pours Count handfuls of Feed from the pocket into the feeder nearest the hero (within reach); returns how many went in. */
	int32 Fill(int32 Index, FName Feed, int32 Count = 1);
	/** Takes feeder Index back into the pocket. */
	void TakeBack(int32 Index);
	/** The feeder the hero stands at and looks at (-1 none). */
	int32 FeederInReach() const;

	int32 Num() const { return States.Num(); }
	const FBirdFeederState& State(int32 Index) const { return States[Index]; }
	ABirdFeeder* Actor(int32 Index) const { return Actors.IsValidIndex(Index) ? Actors[Index].Get() : nullptr; }
	/** The birds on feeder Index's tray now. */
	TArray<int32> BirdsOn(int32 Index) const;
	/** Dev/test: as if the feed went into feeder Index Hours ago. */
	void Age(int32 Index, float Hours);
	/** Dev/test: a feeder at Location (ground under it), facing Yaw. */
	int32 PlaceAt(const FVector& Location, float Yaw);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	/** Spawns the actors of every state and puts them up in the bird life (again, after it restarted). */
	void SpawnAll();
	void Register();
	void UpdatePrompt(class APlayerController* PC);
	float Ready(const FBirdFeederState& S) const;

	UPROPERTY(SaveGame) TArray<FBirdFeederState> States;
	UPROPERTY(Transient) TArray<TObjectPtr<ABirdFeeder>> Actors;
	/** Each feeder's id in the bird life, and the bird life's start count those ids belong to. */
	TArray<int32> SimIds;
	int32 SimStart = -1;
	bool bPrompt = false;
};
