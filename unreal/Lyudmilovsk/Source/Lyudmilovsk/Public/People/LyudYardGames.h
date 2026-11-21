// Children's games in the yards (docs/yard_games.md, interactives backlog item 32): the game side of LyudYard::FYard
// (Private/People/YardGames.h). Offers the children the people's life has at play near a play area to the model every
// second, draws the ones in a game near the camera with their own bodies (ANpcBody, taken over from the life the way the
// allotment's neighbour is) and their gestures on the hand-goal layer, draws the chalked squares, the rope and the ball,
// builds the yards' own things (benches, a sandpit, trees, bushes), shows the children's shouts over their heads, and
// lets the hero join hide-and-seek (E near a game), be found, and seek in turn.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudYardGames.generated.h"

namespace LyudYard { class FYard; struct FGame; struct FKid; struct FEvent; struct FCandidate; }
class ANpcBody;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/** The yards' own things and the games' props: one actor, built once when the town's life starts. */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudYardThings : public AActor
{
	GENERATED_BODY()

public:
	ALyudYardThings();

	UPROPERTY(Transient) TObjectPtr<USceneComponent> Root;
	/** The kit props by kind (CityKit meshes, as the town's own benches), static. */
	UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Kit;
	/** The chalked squares of every yard that plays hopscotch: thin strips on the ground, static. */
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Chalk;
	/** The rope's segments, one pool for the ropes being turned near the camera (moved each frame, not instanced). */
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Rope;
	/** A ball per football game drawn; the goals' bags (two a goal). */
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Balls;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Posts;
};

UCLASS()
class LYUDMILOVSK_API ULyudYardGames : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudYardGames();
	virtual ~ULyudYardGames() override;
	static ULyudYardGames* Get(const UObject* WorldContext);

	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudYardGames, STATGROUP_Tickables); }

	/** Starts a game of Kind (hopscotch, rope, hide, football, tag; "" any the area fits) at the play area nearest the
	 *  hero, with the children nearest it (developer mode, the bench: they come whatever their life was doing, and stay
	 *  till it ends). False if there is no area or too few children. */
	bool StartNearHero(const FString& Kind, int32 Players = 6);
	/** Three games at the three areas nearest the hero (the bench: three groups playing). How many started. */
	int32 StartThreeNearHero();
	/** The hero is the seeker of the hide-and-seek game nearest him (one is started first if there is none). */
	bool HeroSeekNear();
	void EndAll();
	/** One line per game (console lyud.Yard.Report). */
	FString Report() const;
	int32 NumGames() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	/** Shared: its deleter is made where the type is whole (LyudYardGames.cpp). */
	TSharedPtr<LyudYard::FYard> Model;
	bool bTriedLoad = false;
	float OfferClock = 0.f;
	TWeakObjectPtr<ALyudYardThings> Things;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> PropMaterial;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> Meshes;

	/** A child drawn by us: their body, the ground under them (traced when they have moved), walking back to the life's
	 *  place when they leave a game before being handed back. */
	struct FShown
	{
		TWeakObjectPtr<ANpcBody> Body;
		FVector2D GroundAt = FVector2D(1e9, 1e9);
		float GroundZ = 0.f;
		float Back = -1.f;      // s left walking back (-1: in a game)
		FVector Feet = FVector::ZeroVector;
		float Yaw = 0.f;
		bool bSeen = false;
	};
	TMap<int32, FShown> Shown;
	/** The games drawn now (their Uids), nearest the camera first. */
	TArray<int32> Drawn;
	/** Bushes waiting to be planted (one a tick near the camera). */
	struct FBush { FName Species; int32 Seed = 0; float Age = 0.f; FVector At = FVector::ZeroVector; };
	TArray<FBush> PendingBushes;

	// the hero's part (hide-and-seek)
	bool bHeroFoundThisRound = false;
	int32 HeroGameUid = -1;
	float HeroCountShown = -1.f;
	bool bHeroWasCounting = false;
	FText LastPrompt;
	/** PromptByKey's last key (a literal's address: compared, never read) and number. */
	const TCHAR* PromptKey = nullptr;
	int32 PromptArg = MIN_int32;
	/** Children whose body could not be made (the spawn or its setup failed: a missing mesh): not tried again every frame. */
	TSet<int32> NoBody;
	/** The ground under the things that stay put while a game is drawn (a rope's middle, the goals' bags): traced when
	 *  first drawn and once a second after, not every frame. */
	TArray<FShown> RopeGround, PostGround;

	bool Ensure();
	void BuildThings();
	void PlantBushes();
	void OfferChildren();
	void UpdateHero(float DeltaTime);
	void HandleEvents();
	void Present(float DeltaTime);
	void DriveKid(const LyudYard::FGame& G, const LyudYard::FKid& K, FShown& S, float DeltaTime);
	void DrawProps();
	void Release(int32 Id);
	float GroundAt(FShown& S, const FVector2D& At, float ZHint);
	void SayLine(int32 Id, const FString& Key);
	void Prompt(const FText& Text);
	/** The prompt by its LyudText key (null: none), with a number for {0} if given: looked up only when it changes. */
	void PromptByKey(const TCHAR* Key, int32 Arg = MIN_int32);
	FVector CameraLocation() const;
	FVector HeroLocation(FVector* OutVel = nullptr) const;
	int32 NearestArea(const FVector& Where, uint32 GameMask) const;
	TArray<LyudYard::FCandidate> NearestChildren(const FVector& Where, int32 N) const;
};
