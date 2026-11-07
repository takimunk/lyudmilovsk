#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudPublicSpaces.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class AProceduralForest;
class AProceduralPlant;
class ALyudFairRides;
class ALyudFountain;
class FJsonObject;

/**
 * Lyudmilovsk's public spaces (docs/city/public_spaces.md): the promenade's paving and furniture, the beach's umbrellas
 * and loungers, Sun Square with its fountain's stone and the stage, the seaside park with its pond and playground,
 * the fair's stalls and carousel, the pier's railings and the old stair up to the club. Built at play start from
 * Content/City/public_spaces.json (tools/public_spaces.py, from the town plan), never saved in the level.
 *
 * Everything is instanced kit meshes made in code (Private/City/CityKit.*) with one vertex-coloured material, built
 * once; nothing is updated per frame. Lamps: the glass glows at night (one parameter on the lamps' material) and a
 * pool of a few shadowless point lights follows the camera from lamp to lamp, so the lit lamps near you light the
 * ground without a light per lamp. The beach's umbrellas and loungers are out from June to August.
 *
 * Tagged NoTree / NoGrass so the forests and the grass leave the paving bare.
 * Console: lyud.City.PublicSpaces 0 (at the next start), lyud.City.StreetLights N (pool size, 0 = none),
 *          lyud.City.Night -1 by the sun / 0 day / 1 night, lyud.City.Summer -1 by the date / 0 / 1.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudPublicSpaces : public AActor
{
	GENERATED_BODY()

public:
	ALyudPublicSpaces();

	/** Builds everything from the layout; false if there is none. */
	bool Build(const TSharedPtr<FJsonObject>& Doc);

	UFUNCTION(BlueprintCallable, Category = "City") bool IsNight() const { return bNight; }
	UFUNCTION(BlueprintCallable, Category = "City") bool IsSummer() const { return bSummer; }
	UFUNCTION(BlueprintCallable, Category = "City") FString DebugReport() const;
	/** Instances of a prop kind (tests). */
	UFUNCTION(BlueprintCallable, Category = "City") int32 CountProps(const FString& Kind) const;

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	struct FGroup
	{
		FString Kind;
		bool bSummer = false;
		bool bGlow = false;
		TWeakObjectPtr<UInstancedStaticMeshComponent> Comp;      // held by Comps (UPROPERTY)
	};
	TArray<FGroup> Groups;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> Meshes;
	UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Comps;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> GlowMid;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> PropMaterial;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Pool;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> PondWater;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> PondIce;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Cascade;
	UPROPERTY(Transient) TArray<TObjectPtr<class ALyudFountain>> Fountains;
	TArray<TPair<float, int32>> NearLamps;
	TWeakObjectPtr<AProceduralForest> Forest;
	TWeakObjectPtr<ALyudFairRides> Rides;
	TArray<TWeakObjectPtr<AProceduralPlant>> Plants;
	struct FPendingPlant { FName Species; int32 Seed = 0; float Age = 1.f; FTransform Xf; };
	TArray<FPendingPlant> PendingPlants;
	void SpawnPendingPlants();

	TArray<FVector> LampHeads;
	TArray<FName> LampKinds;
	bool bShown = true, bNight = false, bNightKnown = false, bSummer = false, bSummerKnown = false, bIce = false, bIceKnown = false;
	float Glow = 0.f, CheckTimer = 0.f, PoolTimer = 0.f;
	FVector LastPoolCam = FVector(1e9);

	UInstancedStaticMeshComponent* MakeIsm(UStaticMesh* Mesh, const FString& Name, bool bCollide, UMaterialInterface* Mat, float CullCm);
	UStaticMeshComponent* MakePart(UStaticMesh* Mesh, const FString& Name, bool bCollide, UMaterialInterface* Mat, float CullCm, const FTransform& Xf);
	void UpdateNight(bool bForce);
	void UpdateSeason(bool bForce);
	void UpdatePool();
};

/**
 * The fair's rides (docs/city/public_spaces.md): the carousel's rotor and a small Ferris wheel whose gondolas hang upright.
 * They run on the fair's hours (every day 10-21 in June-August, Sat-Sun 10-19 otherwise; lyud.City.Rides -1 by the clock,
 * 0 stopped, 1 running): a ride of ~2.5 min, ~1.5 min standing for boarding. Only the transforms move (no mesh rebuilds);
 * the turning parts are out of Lumen's GI and distance fields and cast no shadow; the actor ticks every frame only while
 * something moves.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudFairRides : public AActor
{
	GENERATED_BODY()

public:
	ALyudFairRides();
	/** Kind "carousel" or "wheel" at a place (cm) and yaw. The rotors (with their bulbs) get GlowMat (the street lamps' MID,
	 *  glowing at night); the gondolas get Mat. */
	void AddRide(const FString& Kind, const FVector& At, float Yaw, UMaterialInterface* Mat, UMaterialInterface* GlowMat);
	UFUNCTION(BlueprintCallable, Category = "City") bool IsRunning() const { return Speed > 0.01f; }
	UFUNCTION(BlueprintCallable, Category = "City") FString DebugReport() const;
	virtual void Tick(float DeltaSeconds) override;

private:
	struct FRide
	{
		bool bWheel = false;
		FVector Hub = FVector::ZeroVector;     // the carousel's foot / the wheel's axle, world cm
		float Yaw = 0.f, Radius = 0.f;
		double Angle = 0.0, V = 0.0, Target = 0.0;     // degrees (unwrapped), deg/s, where it is braking to
		bool bStopping = false;
		TWeakObjectPtr<UStaticMeshComponent> Rotor;
		TWeakObjectPtr<UInstancedStaticMeshComponent> Gondolas;
		int32 NumGondolas = 0;
	};
	TArray<FRide> Rides;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> Meshes;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Parts;
	float Speed = 0.f;          // 0..1 of full speed
	float Cycle = 0.f;          // seconds into the run/board cycle
	bool bOpenKnown = false, bOpen = false;
	float OpenCheck = 0.f;

	bool OpenNow() const;
	void Pose(FRide& R);
};

/** One public place people use: a bench, the fountain's rim, a railing, a stall's counter, a crowd's square. */
USTRUCT(BlueprintType)
struct FLyudPublicSpot
{
	GENERATED_BODY()

	/** Stable id: the town plan's (S-FOUNTAIN, S-BENCH-P07...) or the layout's own (S-BENCH-SQ03, S-FAIR-STALL12...). */
	UPROPERTY(BlueprintReadOnly, Category = "City") FName Id;
	/** bench, fountain, promenade, beach, fair, square, park, pond, playground, pier, stage, sunrise_point... */
	UPROPERTY(BlueprintReadOnly, Category = "City") FName Kind;
	/** A finer kind: rail, kerb, stroll, crowd, towel, loungers, customers, audience, parents... (may be none). */
	UPROPERTY(BlueprintReadOnly, Category = "City") FName Sub;
	/** World cm, and the way it faces (a bench: where its sitters look). */
	UPROPERTY(BlueprintReadOnly, Category = "City") FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "City") FRotator Rotation = FRotator::ZeroRotator;
	/** How many people it takes. */
	UPROPERTY(BlueprintReadOnly, Category = "City") int32 Capacity = 1;
	/** Seat points (world cm; Rotation of each: the way the sitter faces); empty: stand anywhere about Location. */
	UPROPERTY(BlueprintReadOnly, Category = "City") TArray<FTransform> Seats;
	/** Seat heights over the ground, cm, per seat. */
	UPROPERTY(BlueprintReadOnly, Category = "City") TArray<float> SeatHeights;
	/** What each seat is: bench (sit at its height), stand, sit_ground, lie. */
	UPROPERTY(BlueprintReadOnly, Category = "City") TArray<FName> SeatKinds;
	/** A stroll line (world cm) for walks: the promenade, the park's loop, round the pond. */
	UPROPERTY(BlueprintReadOnly, Category = "City") TArray<FVector> Path;
	/** Only in summer (beach loungers, towels). */
	UPROPERTY(BlueprintReadOnly, Category = "City") bool bSummerOnly = false;
	/** Who sits where: per seat, or per place when there are no seats (Capacity entries). */
	TArray<TWeakObjectPtr<UObject>> Taken;
};

/**
 * The town's public spots for the town life and the holidays (docs/city/public_spaces.md "Spots API"): read from
 * Content/City/public_spaces.json (the same list as PeopleData/town_plan.json "spots"), with seat reservations.
 * Also builds the public spaces (ALyudPublicSpaces) when the outdoor map starts.
 *
 *   ULyudPublicSpotsSubsystem* S = ULyudPublicSpotsSubsystem::Get(this);
 *   FName Bench = S->FindNearestFree("bench", MyLocation, 5000.f);
 *   int32 Seat = S->Reserve(Bench, this);           // -1: full
 *   FTransform Where = S->GetSeat(Bench, Seat);      // walk there, sit facing its rotation
 *   S->Release(Bench, this);
 */
UCLASS()
class LYUDMILOVSK_API ULyudPublicSpotsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudPublicSpotsSubsystem* Get(const UObject* WorldContext);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Every spot of a kind (None: all). */
	UFUNCTION(BlueprintCallable, Category = "City") TArray<FLyudPublicSpot> GetSpots(FName Kind) const;
	UFUNCTION(BlueprintCallable, Category = "City") bool GetSpot(FName Id, FLyudPublicSpot& Out) const;
	/** The nearest spot of a kind (and Sub, if given) with a free place within MaxDistCm; None if none. */
	UFUNCTION(BlueprintCallable, Category = "City") FName FindNearestFree(FName Kind, FVector From, float MaxDistCm = 1e7f, FName Sub = NAME_None) const;
	/** Takes a place for Who (a seat index, or a place number on seatless spots); -1 if full or unknown. Again for the same Who: its place. */
	UFUNCTION(BlueprintCallable, Category = "City") int32 Reserve(FName Id, UObject* Who);
	/** Gives Who's place back (any place it holds on that spot). */
	UFUNCTION(BlueprintCallable, Category = "City") void Release(FName Id, UObject* Who);
	/** Gives back every place Who holds anywhere (a person going home). */
	UFUNCTION(BlueprintCallable, Category = "City") void ReleaseAll(UObject* Who);
	/** Where a reserved place is: the seat's transform, or a point spread round the spot for seatless spots. */
	UFUNCTION(BlueprintCallable, Category = "City") FTransform GetSeat(FName Id, int32 Place) const;
	UFUNCTION(BlueprintCallable, Category = "City") int32 FreePlaces(FName Id) const;
	UFUNCTION(BlueprintCallable, Category = "City") int32 NumSpots() const { return Spots.Num(); }
	UFUNCTION(BlueprintCallable, Category = "City") ALyudPublicSpaces* GetSpaces() const { return Spaces.Get(); }

private:
	TArray<FLyudPublicSpot> Spots;
	TMap<FName, int32> ById;
	TSharedPtr<FJsonObject> Doc;
	TWeakObjectPtr<ALyudPublicSpaces> Spaces;
};
