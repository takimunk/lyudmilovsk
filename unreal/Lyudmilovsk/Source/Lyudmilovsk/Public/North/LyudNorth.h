#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudNorth.generated.h"

class ALyudNorthSteam;
class ALyudNorthPlume;
class ULyudNorthSubsystem;

/** What a fumarole of north.json is (docs/north.md). */
UENUM(BlueprintType)
enum class ELyudFumaroleKind : uint8
{
	/** A hissing steam jet from a crusted hole. */
	Vent,
	/** Soft wisps over a few square metres of warm ground. */
	Steam,
	/** A round pool of grey mud with slow bubbles. */
	MudPot,
	/** A still hot pool of clear greenish water. */
	Spring,
};

/** One fumarole as quests see it. */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudFumaroleInfo
{
	GENERATED_BODY()

	/** Stable id from north.json (F01..). */
	UPROPERTY(BlueprintReadOnly, Category = "North") FName Id;
	UPROPERTY(BlueprintReadOnly, Category = "North") ELyudFumaroleKind Kind = ELyudFumaroleKind::Vent;
	/** World cm; Z is the ground once its actor is in the level (0 before). */
	UPROPERTY(BlueprintReadOnly, Category = "North") FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "North") float Size = 1.f;
	/** Hot ground round it, cm. */
	UPROPERTY(BlueprintReadOnly, Category = "North") float HotRadius = 300.f;
};

/** A named place of north.json "places". */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudNorthPlace
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "North") FName Id;
	/** Its name's strings key (place.north.*). */
	UPROPERTY(BlueprintReadOnly, Category = "North") FString NameKey;
	/** World cm (Z 0: the ground is found by a trace). */
	UPROPERTY(BlueprintReadOnly, Category = "North") FVector Location = FVector::ZeroVector;
	/** Where an arriving hero looks, Unreal degrees. */
	UPROPERTY(BlueprintReadOnly, Category = "North") float Yaw = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLyudNorthRegionEvent, bool, bEntered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLyudNorthPlaceEvent, FName, PlaceId);

/**
 * Something of the North that sleeps while the player is far (the fumaroles, the camp's fire): ULyudNorthSubsystem wakes
 * it within lyud.North.Range (300 m: it may tick, steam, glow) and calls it near within 70 m (it may play its sound).
 * Asleep it does not tick, sound or keep particles. Placed by unreal/scripts/import_north.py.
 */
UCLASS(Abstract)
class LYUDMILOVSK_API ALyudNorthThing : public AActor
{
	GENERATED_BODY()

public:
	ALyudNorthThing();

	/** Called by the manager when the ranges change (never every frame). */
	virtual void SetAwake(bool bInAwake, bool bInNear);
	bool IsAwake() const { return bAwake; }
	bool IsNear() const { return bNear; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool bAwake = false;
	bool bNear = false;
};

/**
 * The North (docs/north.md "Game side"): reads Content/North/north.json (places, fumaroles, the far volcanoes), keeps
 * every ALyudNorthThing asleep until the player (the hero, or the bird he flies) comes within range, checking four times a
 * second instead of thirty-odd ticking actors, owns the one steam pool all vents share (ALyudNorthSteam) and the far
 * smoking volcano's plume, feeds them the wind and the air, and tells quests when the hero enters or leaves the North and
 * when he first reaches each named place. Saves nothing (yet).
 *
 * Console: lyud.North 0|1 hides/shows everything tagged North, lyud.North.Goto <place id>, lyud.North.Fire -1|0|1,
 * lyud.North.WakeAll 0|1, lyud.North.Range <m>, lyud.North.Report.
 */
UCLASS()
class LYUDMILOVSK_API ULyudNorthSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudNorthSubsystem* Get(const UObject* WorldContext);
	/** For Blueprints and Python: unreal.LyudNorthSubsystem.get_north(world). */
	UFUNCTION(BlueprintPure, Category = "North", meta = (WorldContext = "WorldContext"))
	static ULyudNorthSubsystem* GetNorth(const UObject* WorldContext) { return Get(WorldContext); }

	/** North of the old map's forest (map Y 2330 m): the meadows, the camp, the valley, the Cone. */
	UFUNCTION(BlueprintPure, Category = "North") static bool IsInNorth(const FVector& WorldLocation);
	/** Within the hot ground of a vent, a steaming patch, a mud pot or the spring. */
	UFUNCTION(BlueprintCallable, Category = "North") bool IsHotGround(const FVector& WorldLocation) const;
	/** The fumarole whose hot ground this is (false: none). */
	bool FindHotGround(const FVector& WorldLocation, FLyudFumaroleInfo& Out) const;
	UFUNCTION(BlueprintCallable, Category = "North") TArray<FLyudFumaroleInfo> GetFumaroles() const { return Fumaroles; }
	UFUNCTION(BlueprintCallable, Category = "North") TArray<FLyudNorthPlace> GetPlaces() const { return Places; }
	bool FindPlace(FName PlaceId, FLyudNorthPlace& Out) const;
	UFUNCTION(BlueprintPure, Category = "North") bool IsPlayerInNorth() const { return bPlayerInside; }
	UFUNCTION(BlueprintPure, Category = "North") bool HasReached(FName PlaceId) const { return Reached.Contains(PlaceId); }
	/** Things awake now (tests: the fumaroles near the hero). */
	UFUNCTION(BlueprintCallable, Category = "North") int32 NumAwake() const;
	UFUNCTION(BlueprintCallable, Category = "North") int32 NumThings() const { return Things.Num(); }
	UFUNCTION(BlueprintCallable, Category = "North") FString DebugReport() const;

	/** The player entered (true) or left (false) the North. */
	UPROPERTY(BlueprintAssignable, Category = "North") FLyudNorthRegionEvent OnRegionChanged;
	/** The player reached a place of north.json for the first time in this play. */
	UPROPERTY(BlueprintAssignable, Category = "North") FLyudNorthPlaceEvent OnPlaceReached;

	// actions for the console and the dev panel (plain functions a registrar can call)
	/** Puts the player's pawn on the ground at a place (false: no such place or no pawn). */
	static bool GotoPlace(UWorld* World, FName PlaceId);
	/** Shows or hides everything tagged North (no collision while hidden; asleep while hidden). */
	static void SetShown(UWorld* World, bool bOn);
	/** The camp's fire: -1 by its schedule, 0 out, 1 burning. */
	static void SetFire(UWorld* World, int32 Mode);
	/** Every North thing awake whatever the distance (tests, screenshots). */
	static void SetWakeAll(UWorld* World, bool bAll);

	void Register(ALyudNorthThing* Thing);
	void Unregister(ALyudNorthThing* Thing);
	/** The shared steam pool (spawned at the valley at the first call). */
	ALyudNorthSteam* Steam();
	/** The wind for the steam, world cm/s, and the air's density for steam (1 normal, ~2 cold and damp). */
	FVector GetWindCm() const { return WindCm; }
	float GetSteamDensity() const { return Density; }

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudNorthSubsystem, STATGROUP_Tickables); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FFarVolcano
	{
		FName Id;
		FVector Top = FVector::ZeroVector;     // world cm of the crater
		float CraterRadius = 0.f;              // cm
		float Plume = 0.f;
	};

	void LoadData();
	void UpdateRanges();
	void UpdateAir();
	void SpawnPlumes();
	bool PlayerLocation(FVector& Out) const;

	TArray<FLyudFumaroleInfo> Fumaroles;
	TArray<FLyudNorthPlace> Places;
	TArray<FFarVolcano> FarVolcanoes;
	TArray<TWeakObjectPtr<ALyudNorthThing>> Things;
	TSet<FName> Reached;
	UPROPERTY(Transient) TObjectPtr<ALyudNorthSteam> SteamFx;
	UPROPERTY(Transient) TArray<TObjectPtr<ALyudNorthPlume>> Plumes;
	FVector Valley = FVector::ZeroVector;
	FVector WindCm = FVector::ZeroVector;
	float Density = 1.f;
	float RangeTimer = 0.f;
	float AirTimer = 0.f;
	bool bPlayerInside = false;
	bool bWakeAll = false;
	bool bShown = true;
	bool bLoaded = false;
};
