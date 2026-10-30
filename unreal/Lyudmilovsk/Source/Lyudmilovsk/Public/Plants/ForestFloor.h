#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "ForestFloor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMaterialParameterCollection;
class UStaticMesh;
class UTexture2D;
class UTextureRenderTarget2D;

/** What lies on the forest floor at one spot (the litter field, docs/forest_floor.md). */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FForestLitter
{
	GENERATED_BODY()

	/** 0..1 cover of fallen needles (pines, spruces), and how much of them are spruce's (darker, finer). */
	UPROPERTY(BlueprintReadOnly, Category = "Litter") float Needles = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Litter") float Spruce = 0.f;
	/** 0..1 cover of fallen leaves (this autumn's plus what is left of last year's). */
	UPROPERTY(BlueprintReadOnly, Category = "Litter") float Leaves = 0.f;
	/** Their colour: 0 birch and aspen yellow .. 0.5 maple and rowan orange-red .. 1 oak and alder brown. */
	UPROPERTY(BlueprintReadOnly, Category = "Litter") float Hue = 0.f;
	/** 0 fresh and crisp .. 1 dark, matted and rotting (old leaves, after a winter under the snow). */
	UPROPERTY(BlueprintReadOnly, Category = "Litter") float Decay = 0.f;
	/** 0..1 how much this spot is under the trees at all. */
	UPROPERTY(BlueprintReadOnly, Category = "Litter") float Canopy = 0.f;
};

/**
 * The forest floor around the player (docs/forest_floor.md): everything between the trunks of the AProceduralForest
 * woods, laid out in 8 m world cells around the camera from a hash of the cell (every machine and every visit grows
 * the same floor) and from what stands around it:
 *
 *  - ground cover, every one a species of the plant system: bilberry, lingonberry, heather, bracken, broad buckler fern,
 *    wood sorrel (herbs, FPlantSpecies::Herb), feather moss, haircap moss, reindeer lichen and wavy hair-grass (grass
 *    tufts, FGrassSpecies), and seedlings of the trees around (their forest bakes, UTreeBakeCache);
 *  - litter and debris built in code: fallen leaves of the deciduous trees about (by species and season), pine and spruce
 *    cones under their trees, birch bark curls, sticks, fallen branches, logs, stumps, roots round old trunks, stones and
 *    a few glacial boulders, moss cushions;
 *  - the litter field: a 192 m window of 1.5 m cells round the camera saying how much needle and leaf litter lies there,
 *    its colour and its decay, from the trees about (species, age, distance, crown), the calendar (each species' leaf
 *    fall) and the season; the ground's material (M_Ground's forest layer) and the 3D litter read it (RT_Litter,
 *    MPC_LyudWeather).
 *
 * The weather: wet ground darkens and glosses everything (the rain's wet), frost rimes the litter and the moss, drought
 * pales and dries it, the wind sways the shrubs and ferns and stirs the loose leaves, leaves fall from the trees in their
 * season (more in a gust); snow buries what is lower than it lies (the ground's own snow covers it, footprints still
 * print) and whitens what sticks out.
 *
 * Cheap: hierarchical instances per kind, no collision but logs, stumps and boulders, shadows only from the large wood
 * and stones, every kind culled at its own distance (moss at 14 m, logs at 70 m). Spawned by ULyudForestFloorSubsystem
 * into any game world with forests; lyud.ForestFloor 0 hides it.
 */
UCLASS()
class LYUDMILOVSK_API AForestFloor : public AActor
{
	GENERATED_BODY()

public:
	AForestFloor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor") int32 Seed = 1;
	/** Multiplies every kind's density (lyud.ForestFloor.Density multiplies it again). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor", meta = (ClampMin = "0")) float DensityScale = 1.f;
	/** Cells, cm; the floor is laid out to each kind's cull distance plus Margin round the camera, again when it moves Relayout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor") float CellSize = 800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor") float Margin = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor") float Relayout = 600.f;
	/** At most this many ground traces a frame while cells are being laid out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor") int32 TracesPerTick = 700;
	/** Developer mode: hold the season at a day of the year (below 0: the calendar). */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Floor|Dev") float DevDay = -1.f;

	/** Lays the whole floor out round Location now, on this thread (tests, the first frame). */
	UFUNCTION(BlueprintCallable, Category = "Floor") void RebuildAround(FVector Location);
	/** The litter at a world point now (from the trees about and the date). */
	UFUNCTION(BlueprintCallable, Category = "Floor") FForestLitter LitterAt(FVector Location) const;
	UFUNCTION(BlueprintCallable, Category = "Floor") int32 NumInstances() const;
	UFUNCTION(BlueprintCallable, Category = "Floor") int32 NumTrees() const { return TreeCount; }
	/** One line per kind: instances, triangles, cull, and the mesh lint (degenerate, stretched, overlapping faces). */
	UFUNCTION(BlueprintCallable, Category = "Floor") FString DescribeState() const;
	UFUNCTION(BlueprintCallable, Category = "Floor") float CurrentDay() const;
	/**
	 * Lays out every cell within Radius of Centre the way the floor would (nothing is shown) and checks each piece
	 * against the built world: the walk grid's paved and yard cells and the NoTree/NoGrass/Roof actors within 15 m, the
	 * forest ground under it, the wild trees about it. Writes the 2 m occupancy (0 none, 1 a piece, 2 a big one) to
	 * OutFile (FFA1, the walk grid's frame) for tools/forestfloor_audit.py. Returns a summary line.
	 */
	UFUNCTION(BlueprintCallable, Category = "Floor") FString AuditMap(const FString& OutFile, FVector Centre, float Radius);
	/** 1 where the floor may lie (wild forest ground, 15 m from anything built), 0 elsewhere. */
	UFUNCTION(BlueprintCallable, Category = "Floor") float SiteAt(FVector Location) const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void BeginDestroy() override;
	virtual void Tick(float DeltaSeconds) override;

	struct FTreeRef
	{
		FVector3f At;
		float Height = 0.f;
		float Crown = 0.f;
		float TrunkR = 0.f;
		/** Index into the floor's tree classes (needles, leaf fall days, leaf hue, cone kind). */
		uint8 Class = 0;
		/** A tree of the wild woods (not a town's, a garden's, an alley's or a lone planted one). */
		bool bWild = false;
	};
	struct FCellItem
	{
		FTransform3f Xf;
		float Custom0 = 0.f, Custom1 = 0.f;
		/** Height of its top over the ground, cm (buried by deeper snow). */
		float Top = 0.f;
		uint16 Kind = 0;
	};
	struct FCell
	{
		TArray<FCellItem> Items;
		/** Bands laid out so far (bit per band: near, mid, far). */
		uint8 Bands = 0;
		float Ground[9 * 9];
		uint8 Floor[9 * 9];
		bool bTraced = false;
	};

private:
	struct FKind;

	void EnsureKinds();
	void ClearKinds();
	void BuildTreeIndex();
	void BuildKind(FKind& K);
	bool CameraLocation(FVector& Out) const;
	void TraceCell(const FIntPoint& C, FCell& Cell);
	void LayCell(const FIntPoint& C, FCell& Cell, int32 Band);
	void CommitKind(int32 Index);
	void UpdateSeason(float Day, bool bForce);
	void UpdateWeather(float DeltaSeconds);
	void UpdateLitterField(const FVector& Camera, float Day, bool bForce);
	float GroundAt(const FCell& Cell, const FIntPoint& C, float X, float Y, float& OutNormalZ, float& OutFloor) const;
	FForestLitter LitterFrom(const FVector& At, float Day) const;
	template <typename FFn> void ForTreesNear(const FVector& At, float Radius, FFn&& Fn) const;
	float SnowDepthAt(const FVector& At) const;
	void BuildSites();
	void ResetLayout();
	int32 WildTreesNear(float X, float Y, float Radius) const;
	/** The site grid at a world point: forest ground (0/1) and metres to the nearest built thing. */
	bool SiteCell(double X, double Y, int32& OutIndex) const;
	float SiteSoft(double X, double Y) const;

	TArray<FKind*> Kinds;
	TMap<FIntPoint, FCell> Cells;
	TMap<FIntPoint, TArray<FTreeRef>> TreeGrid;
	/**
	 * Where the floor may lie (BuildSites), on the walk grid's 2 m cells (Content/AnimalsData/animal_world.bin, built by
	 * tools/animal_map.py from tools/world_map.py's zones): the forest ground of the wild woods (not the town, the
	 * Academgorodok, the beach), and the metres from each cell to the nearest built thing (paved, a yard, a building's,
	 * station's or roof's footprint: the NoTree/NoGrass/Roof actors).
	 */
	TArray<uint8> SiteForest;
	TArray<uint8> SiteClear;
	TArray<uint8> SiteKind;
	TArray<FBox> BuiltBoxes;
	int32 SiteNX = 0, SiteNY = 0;
	FVector2D SiteOrigin = FVector2D::ZeroVector;
	float SiteCellCm = 200.f;
	int32 GuardShown = MIN_int32;
	int32 TreeCount = 0;
	int32 ForestTreeSum = -1;
	double NextTreeCheck = 0.0;
	FVector Centre = FVector(1e12);
	FVector2D GridOrigin = FVector2D::ZeroVector;
	float LastSnowCm = -1.f;
	float ShownDay = -1000.f;
	float LaidDay = -1000.f;
	double NextSeason = 0.0;
	TArray<int32> DirtyKinds;
	float MaxReach = 0.f;
	bool bStarted = false;
	bool bHidden = false;
	/** The litter field's window: centre and the day it was computed for. */
	FVector2D LitterCentre = FVector2D(1e12, 1e12);
	float LitterDay = -1000.f;
	/** Smoothed weather the materials see: wet, rime, dry, rain, snow, leaf fall. */
	float Weather[6] = {};

	UPROPERTY(Transient) TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Comps;
	UPROPERTY(Transient) TArray<TObjectPtr<UObject>> KeepAlive;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> WeatherMIDs;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> PlantMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> GrassMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> DebrisMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FallingMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialParameterCollection> Collection;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> LitterRT;
};

/** Spawns one AForestFloor into a game world that has forests (none placed by hand). */
UCLASS()
class LYUDMILOVSK_API ULyudForestFloorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	UFUNCTION(BlueprintCallable, Category = "Floor") AForestFloor* GetFloor() const { return Floor.Get(); }
	/** For Python: unreal.LyudForestFloorSubsystem.find_floor(world). */
	UFUNCTION(BlueprintPure, Category = "Floor", meta = (WorldContext = "WorldContext"))
	static AForestFloor* FindFloor(const UObject* WorldContext);

private:
	TWeakObjectPtr<AForestFloor> Floor;
};
