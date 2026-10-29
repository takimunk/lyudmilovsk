#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "LyudWinter.generated.h"

class ALyudSea;
class UBoxComponent;
class UDecalComponent;
class UMaterialInterface;
class UMaterialParameterCollection;
class UStaticMeshComponent;
class UTextureRenderTarget2D;
namespace LyudWinter { struct FWinterState; }

/** The winter at one moment, as the game draws it. */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudWinterNow
{
	GENERATED_BODY()

	/** Level snow on open ground, cm (the weather's), and how fast it falls, cm/h. */
	UPROPERTY(BlueprintReadOnly, Category = "Winter") float LevelCm = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Winter") float SnowfallCmPerHour = 0.f;
	/** 0 level .. 1 this winter's blowing snow has built the drifts fully. */
	UPROPERTY(BlueprintReadOnly, Category = "Winter") float Drift = 0.f;
	/** Where the drifting winds came from (compass degrees, the strongest). */
	UPROPERTY(BlueprintReadOnly, Category = "Winter") float DriftFromDeg = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Winter") float SnowAgeDays = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Winter") float Melt = 0.f;
	/** Sea ice: how much of the open sea is frozen 0..1 and how thick, cm. */
	UPROPERTY(BlueprintReadOnly, Category = "Winter") float IceCover = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Winter") float IceCm = 0.f;
	/** Snow on the trees' branches 0..1 (fresh snow sticks, wind and thaw shake it off). */
	UPROPERTY(BlueprintReadOnly, Category = "Winter") float CanopySnow = 0.f;
};

/**
 * Winter on the outdoor map (docs/winter.md): where the snow lies and how the sea freezes, from the weather's level
 * snow depth, snowfall, wind and sea ice (the weather engine says how much; this says where).
 *
 * The snow: a 1 m field of depth over the whole landscape (LyudWinter::FSnowField, plain C++): the level snow moved by
 * the winter's drifting winds from exposed places into sheltered ones (the lee of houses and hills, forest edges, the
 * cliff's brow) up to ~1 m, none under roofs, thinner under the forest, shed from steep slopes. The ground's material
 * (M_Ground) raises the landscape by it (world position offset from /Game/Winter/RT_SnowDepth); one decal over the
 * whole map (M_SnowDecal) whitens what the snow covers: the ground by the field, and every upward face under the open
 * sky (roofs, benches, rails, the platform's edge) by the level snow; plants carry their own (MPC CanopySnow).
 * The world as the snow sees it (the ground and the highest roof over every 2 m) is read by traces when winter first
 * comes, a few hundred rows a frame.
 *
 * Paths: the walk grid's paved cells (Content/AnimalsData/animal_world.bin) are cut into ~12 m patches; residents
 * shovel them (UPeopleLifeSubsystem's shovel activity: FindDirtyPatch, ClaimPatch, SetPatchProgress) and the snow goes
 * into berms beside them; patches far from the camera clear by themselves some hours after the snow began to lie.
 *
 * The sea: ALyudSeaIce, a flat sheet at the sea's level now (the tide moves it), masked where the water is within
 * 10-15 m of the shallows (the shore at this tide, the railway's bank, the island: a distance transform on a 3 m grid,
 * redone when the tide moves) and broken into floes when the cover is partial. The waves die under it (the sea's
 * WindScale). It holds the player where it is thick enough.
 *
 * Console: lyud.Winter 0 off; lyud.Winter.Snow / Drift / DriftFrom / Snowfall / Ice / IceCm force a value (negative =
 * the weather's); lyud.Winter.Debug 1 prints the state; lyud.Winter.Report logs it.
 */
UCLASS()
class LYUDMILOVSK_API ULyudWinterSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudWinterSubsystem();
	virtual ~ULyudWinterSubsystem() override;

	static ULyudWinterSubsystem* Get(const UObject* WorldContext);
	/** For Blueprints and Python: unreal.LyudWinterSubsystem.get_winter(world). */
	UFUNCTION(BlueprintPure, Category = "Winter", meta = (WorldContext = "WorldContext"))
	static ULyudWinterSubsystem* GetWinter(const UObject* WorldContext) { return Get(WorldContext); }

	UFUNCTION(BlueprintPure, Category = "Winter") FLyudWinterNow GetNow() const { return Now; }
	/** Snow depth at a world point, cm (0 off the map, under roofs, on a cleared path). */
	UFUNCTION(BlueprintPure, Category = "Winter") float GetSnowDepthAt(const FVector& Location) const;
	/** The snow's surface there (ground + snow), world Z cm; false where there is no ground. */
	UFUNCTION(BlueprintPure, Category = "Winter") bool GetSnowSurfaceAt(const FVector& Location, float& OutZ) const;
	/** 0..1 how frozen the sea is at a world point. */
	UFUNCTION(BlueprintPure, Category = "Winter") float GetIceAt(const FVector& Location) const;
	UFUNCTION(BlueprintPure, Category = "Winter") bool IsIceWalkable(const FVector& Location) const;
	/** The snow field, the paths and the ice mask are built (the traces done). */
	UFUNCTION(BlueprintPure, Category = "Winter") bool IsReady() const;
	UFUNCTION(BlueprintCallable, Category = "Winter") FString DebugReport() const;
	/** Snow on the branches 0..1 for the plants' bark, or Fallback (the calendar's guess) where no winter runs. */
	static float SnowOnBranches(const UWorld* World, float Fallback);
	/** How white the ground is 0..1 for the grass, or Fallback where no winter runs. */
	static float SnowOnGround(const UWorld* World, float Fallback);

	// --- shovelling: the people's life asks for work and reports how far it got
	/** The dirtiest path patch within RadiusCm of Near with at least MinCm of snow nobody is clearing; -1 none. */
	UFUNCTION(BlueprintCallable, Category = "Winter") int32 FindDirtyPatch(const FVector& Near, float RadiusCm, float MinCm = 4.f) const;
	UFUNCTION(BlueprintCallable, Category = "Winter") int32 NumPatches() const;
	/** Snow on a patch now, cm. */
	UFUNCTION(BlueprintPure, Category = "Winter") float GetPatchSnow(int32 Patch) const;
	/** The line a shoveller walks along a patch: its start and end on the ground (world cm). */
	UFUNCTION(BlueprintPure, Category = "Winter") bool GetPatchLine(int32 Patch, FVector& OutStart, FVector& OutEnd) const;
	/** Somebody (an id of theirs) takes a patch to clear; false if someone else has it. */
	UFUNCTION(BlueprintCallable, Category = "Winter") bool ClaimPatch(int32 Patch, int32 Who);
	UFUNCTION(BlueprintCallable, Category = "Winter") void ReleasePatch(int32 Patch, int32 Who);
	/** How far the clearing has swept 0..1; at 1 the patch is clear and its snow is in the berms. */
	UFUNCTION(BlueprintCallable, Category = "Winter") void SetPatchProgress(int32 Patch, float Progress);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual bool IsTickable() const override { return bStarted; }

private:
	TPimplPtr<LyudWinter::FWinterState> State;
	bool bStarted = false;
	FLyudWinterNow Now;

	UPROPERTY(Transient) TObjectPtr<UMaterialParameterCollection> Collection;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> DepthRT;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> CoverRT;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> IceRT;
	UPROPERTY(Transient) TObjectPtr<AActor> DecalActor;
	UPROPERTY(Transient) TObjectPtr<UDecalComponent> Decal;
	UPROPERTY(Transient) TObjectPtr<class ALyudSeaIce> Ice;
	TWeakObjectPtr<ALyudSea> Sea;
	float SeaWindScale = -1.f;
	float WadeSpeed = -1.f;
	float SweepTimer = 0.f;

	void ReadWeather(float DeltaTime);
	void StepCover();
	void StartBuild();
	void FinishBuild();
	void StepSnow(float DeltaTime);
	void StepIce(float DeltaTime);
	void UploadDepth(int32 X0, int32 Y0, int32 X1, int32 Y1);
	void UploadCover();
	void UploadIce();
	void UpdateCollection();
	void UpdateDecal();
	void Wade(float DeltaTime);
	void KeepOffDecals();
	bool CameraLocation(FVector& Out) const;
};

/**
 * The sea's ice: a flat sheet at the still water's level (with the tide), drawn by M_SeaIce (masked by the winter's ice
 * mask and its noises, snow on it, cracks and pressure ridges, thin dark nilas and pancakes at the edge) and solid where
 * it holds (a pool of boxes round the player, re-laid as they move). Spawned and driven by ULyudWinterSubsystem.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudSeaIce : public AActor
{
	GENERATED_BODY()

public:
	ALyudSeaIce();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ice") TObjectPtr<UStaticMeshComponent> Sheet;
	/** Tiles of solid ice round the player, cm, and how far they reach. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice") float TileCm = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice") float ReachCm = 3000.f;

	/** Puts the sheet at Z (the still water + freeboard) and lays the solid tiles round Around by Walkable. */
	void Drive(float SurfaceZ, float ThicknessCm, const FVector& Around, const TFunctionRef<bool(double X, double Y)>& Walkable, bool bForce);
	int32 NumSolid() const { return Solid; }

private:
	UPROPERTY(Transient) TArray<TObjectPtr<UBoxComponent>> Tiles;
	FVector LaidAt = FVector(1e12);
	float LaidZ = -1e9f;
	int32 Solid = 0;
};
