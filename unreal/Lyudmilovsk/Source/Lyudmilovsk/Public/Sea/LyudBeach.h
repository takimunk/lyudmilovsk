#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudBeach.generated.h"

class AActor;
class AHeroCharacter;
class ALyudSea;
class SWidget;
class USceneComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;
struct FLyudBeachHud;

/**
 * What the hero has done on the beach (docs/beach.md), saved with the game ("Beach"): the best count of skips, the best
 * swim to the buoy, the flat stones in his pocket, his sandcastle (one at a time: where it stands, how much of it is built
 * and how much the water has taken), what he picked up on the waterline today.
 */
UCLASS()
class LYUDMILOVSK_API ULyudBeachBook : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static ULyudBeachBook* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(SaveGame) int32 BestSkips = 0;
	UPROPERTY(SaveGame) int32 Throws = 0;
	UPROPERTY(SaveGame) float BestSwim = 0.f;
	UPROPERTY(SaveGame) int32 Swims = 0;
	UPROPERTY(SaveGame) int32 CastlesBuilt = 0;
	UPROPERTY(SaveGame) int32 CastlesWashed = 0;
	UPROPERTY(SaveGame) int32 Shells = 0;
	UPROPERTY(SaveGame) int32 Ambers = 0;
	/** A bit per LyudBeach::EFind found at least once. */
	UPROPERTY(SaveGame) int32 FoundBits = 0;
	/** Flat stones in the pocket and how flat each is (0 .. 1). */
	UPROPERTY(SaveGame) TArray<float> Stones;

	UPROPERTY(SaveGame) bool bCastle = false;
	UPROPERTY(SaveGame) FVector CastleAt = FVector::ZeroVector;
	UPROPERTY(SaveGame) float CastleWork = 0.f;
	UPROPERTY(SaveGame) float CastleWorn = 0.f;
	UPROPERTY(SaveGame) FDateTime CastleBegun;
	/** The game's time the castle was last looked after (the water between then and now is made up for). */
	UPROPERTY(SaveGame) FDateTime CastleSeen;

	/** Children helped build this castle (the stamp "castle_kids"); its flag is the kiosk's paper one (sand_flag), not a
	 *  splinter and a sweet wrapper. */
	UPROPERTY(SaveGame) bool bCastleHelped = false;
	UPROPERTY(SaveGame) bool bCastlePaperFlag = false;
	/** Swims won against the summer visitors; roubles the souvenir units have paid for finds. */
	UPROPERTY(SaveGame) int32 SwimWins = 0;
	UPROPERTY(SaveGame) int32 SoldRoubles = 0;

	/** The day (days since 2000) the picked cells are of, and the cells of the waterline already emptied that day. */
	UPROPERTY(SaveGame) int32 PickDay = 0;
	UPROPERTY(SaveGame) TArray<int32> Picked;

	/** A new game: an empty book. */
	void Reset();
};

/**
 * The beach at play (docs/beach.md). Where the hero stands on damp sand by the water, on his feet and with empty hands:
 *
 *   - E (a tap): looks for a flat stone in the shingle (most are not); G held and let go on the swing: throws it, and it
 *     skips over the real surface of the sea (ALyudSea::SampleSurface), the count called out skip by skip. The town record
 *     (11) is chalked on the lifeguards' board;
 *   - E (held): builds a sandcastle a step in front of him: a heap, a keep, towers, walls, a moat, a flag. The tide washes
 *     it away hours later (LyudBeach::Wash by the sea's level, also over the hours the hero slept or was away);
 *   - shells always and amber after a storm lie along the waterline (the same ones all day): E picks one up;
 *   - at the post below each lifeguard tower E starts the swim to the buoy and back against the clock; in the summer season
 *     a few summer visitors (LyudBeach::Rival) swim it with him, their caps out on the water, and he gets his place;
 *   - the town's children at play on the sand near his castle join in (faster building, their words); the kiosk's paper
 *     flag (sand_flag) goes on the finished castle;
 *   - Y at a souvenir unit's counter sells what the waterline gave (keeping three pieces of amber while the seller still
 *     wants to see them for "The amber shore").
 *
 * The rules are LyudBeach (Private/Sea/BeachModel, plain C++, tools/beachsim). Stamps "beach/<piece>", quest events
 * "beach <what>" (skip3, skip5, skip_record, castle, castle_washed, swim, swim_gold, shell, amber), diary notes.
 *
 * Console: lyud.Beach.Info, lyud.Beach.Stone [flatness], lyud.Beach.Throw [quality], lyud.Beach.Castle [work 0..6],
 * lyud.Beach.Race, lyud.Beach.Storm <0..1|-1>, lyud.Beach.Reset.
 */
UCLASS()
class LYUDMILOVSK_API ULyudBeach : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudBeach* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudBeach, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bBegun; }

	/** Console and tests. */
	FString Report() const;
	void GiveStone(float Flatness);
	bool ThrowNow(float Quality);
	bool CastleHere(float Work);
	bool RaceNow();
	void SetStorm(float Score) { StormOverride = Score; FindsKey = 0; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	struct FShore
	{
		bool bOnSand = false;      // on his feet, hands empty, near the water
		bool bDamp = false;        // the sand under him holds a castle
		bool bWaterAhead = false;  // water to throw onto in front of him
		FVector Feet = FVector::ZeroVector;
		FVector Toward = FVector::ForwardVector;   // flat, where he looks
		float SeaZ = 0.f, WetZ = 0.f, WaveCm = 0.f;
	};
	struct FShownFind
	{
		int32 Cell = 0;
		int32 Kind = 0;
		FVector At = FVector::ZeroVector;
	};
	struct FPost
	{
		FVector At = FVector::ZeroVector;
		FVector Buoy = FVector::ZeroVector;
		bool bPlaced = false;
	};

	void SlowTick(AHeroCharacter& Hero, ALyudSea& Sea, float Dt);
	void UpdateShore(AHeroCharacter& Hero, ALyudSea& Sea);
	void UpdateFinds(ALyudSea& Sea);
	void UpdateCastle(ALyudSea& Sea, float Dt);
	void UpdateRace(AHeroCharacter& Hero, ALyudSea& Sea, float Dt);
	void UpdateStone(ALyudSea& Sea, float Dt);
	void UpdateInput(AHeroCharacter& Hero, ALyudSea& Sea, APlayerController& PC, float Dt);
	void UpdatePrompt();
	void PlacePosts(ALyudSea& Sea);
	void ShowCastle();
	void Search();
	void Throw(float Quality, ALyudSea& Sea);
	void Pick(int32 Index);
	void BeginCastle(ALyudSea& Sea);
	void Stamp(const TCHAR* Piece);
	void Say(const FText& Text, float Seconds = 5.f);
	void Play(FName Sound, const FVector& At);
	float StormNow();
	void MakeProps();
	UStaticMeshComponent* AddShape(USceneComponent* Parent, UStaticMesh* Mesh, const FLinearColor& Tint, const FVector& At, const FVector& Scale);
	UMaterialInstanceDynamic* TintOf(const FLinearColor& Tint);
	void Finish(int32 Skips);
	FVector OnGround(const FVector& At) const;
	bool Seaward(ALyudSea& Sea, const FVector& At, FVector& Out) const;
	void EnsureHud();
	/** The children (ages 4 .. 12) of the people's life, read once it runs; how many of them are at play by the castle. */
	void ReadKids();
	int32 CountHelpers(const FVector& At, int32* OutOne = nullptr);
	/** The visitors' caps on the water while a swim is on (every frame, three meshes at most). */
	void UpdateRivals(ALyudSea& Sea);
	void HideRivals();
	/** The souvenir counters (the things with the act buy_souvenirs), the one he stands at and what it would pay now. */
	void UpdateSouvenirs();
	void Sell();
	int32 SellOffer(TArray<TPair<FName, int32>>* OutItems = nullptr) const;

	bool bBegun = false;
	float SlowClock = 0.f;
	FShore Shore;
	FText LastPrompt;
	float SayLeft = 0.f;

	// E: a tap searches, a hold builds
	bool bEDown = false;
	float EHeld = 0.f;
	bool bBuilding = false;
	float SearchLeft = -1.f;
	TMap<uint64, int32> Tries;
	int32 TriesDay = 0;

	// the throw
	bool bSwinging = false;
	float SwingHeld = 0.f;
	bool bFlying = false;
	float FlightT = 0.f;
	int32 FlightTouch = 0;
	FVector FlightFrom = FVector::ZeroVector;
	FVector FlightDir = FVector::ForwardVector;
	TArray<FVector4f> FlightHops;      // X m, T s, height m, speed out
	int32 FlightSkips = 0;

	// the waterline's finds
	uint32 FindsKey = 0;
	TArray<FShownFind> Finds;
	int32 NearFind = -1;
	float StormOverride = -1.f;
	float StormScore = 0.f;
	int32 StormDay = -1;

	// the castle
	int32 ShownStage = -1;
	float ShownStanding = -1.f;

	// the swim
	TArray<FPost> Posts;
	bool bPostsPlaced = false;
	int32 NearPost = -1;
	int32 RacePost = -1;
	uint8 RaceState = 0;
	float RaceSeconds = 0.f, RaceReadyFor = 0.f, RaceOut = 0.f, RaceCourse = 60.f;

	// the children at the castle
	TArray<int32> Kids;
	bool bKidsRead = false;
	int32 Helpers = 0;
	float KidLineIn = 3.f;

	// the summer visitors in the swim (indices into LyudBeach::Rival), the game day the race is on
	int32 RivalIdx[3] = { 0, 0, 0 };
	int32 RivalNum = 0;
	int32 RaceDay = 0;
	bool bRivalsShown = false;

	// the souvenir counters
	TArray<FVector> Souvenirs;
	bool bSouvenirsRead = false;
	int32 NearSouvenir = -1;
	int32 Offer = 0;

	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> RivalMeshes;
	UPROPERTY(Transient) TObjectPtr<AActor> Props;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> CastleRoot;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> StoneMesh;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> FindMeshes;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> CastleMeshes;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> PostMeshes;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> BuoyMeshes;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Tints;
	TArray<FLinearColor> TintColors;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Sphere;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Cone;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Cylinder;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Cube;
	TWeakObjectPtr<ALyudSea> SeaActor;
	TSharedPtr<SWidget> Widget;
	TSharedPtr<FLyudBeachHud> Hud;
};
