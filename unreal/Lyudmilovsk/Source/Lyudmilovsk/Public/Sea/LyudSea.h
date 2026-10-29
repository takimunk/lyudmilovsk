#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "LyudSea.generated.h"

class ACharacter;
class APhysicsVolume;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMaterialParameterCollection;
class UPostProcessComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture2D;
class ULyudSeabedComponent;
namespace LyudWeather { class FWaveField; }

/** The sea at one point: what a floating or swimming thing needs. */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudSeaSample
{
	GENERATED_BODY()

	/** World Z of the water surface over the point, cm. */
	UPROPERTY(BlueprintReadOnly, Category = "Sea") float SurfaceZ = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Sea") FVector Normal = FVector::UpVector;
	/** Velocity of the water at the surface, cm/s (orbital motion of the waves). */
	UPROPERTY(BlueprintReadOnly, Category = "Sea") FVector Velocity = FVector::ZeroVector;
	/** Still-water depth to the bottom, cm (negative on land; large out at sea). */
	UPROPERTY(BlueprintReadOnly, Category = "Sea") float Depth = 0.f;
	/** How far the point is under the surface, cm (negative above it). */
	UPROPERTY(BlueprintReadOnly, Category = "Sea") float Immersion = 0.f;
	/** Is there sea here at all (not land, not the land side of the coast)? */
	UPROPERTY(BlueprintReadOnly, Category = "Sea") bool bIsSea = false;
};

/**
 * The Baltic sea: a real surface of waves grown by the wind (ULyudWindSubsystem's sea-state wind, a
 * fetch-limited spectrum of Gerstner wave trains in LyudWeather::FWaveField), drawn as single-layer
 * water (refraction, absorption and scattering of the cloudy green-brown Baltic, depth colour, foam
 * on the crests in a fresh wind and surf over the shallows), with a bottom that gets shallow toward the
 * beach where the waves shrink and break.
 *
 * The surface is a ring mesh built in code around the camera: fine near the eye, coarse toward the
 * horizon 30 km off, moved with the camera. Its material gets the same wave trains as the CPU through
 * a small float texture, so SampleSurface (buoyancy, swimmers, splashes) sees exactly the waves on
 * screen. The water depth comes from the ground (the landscape's heights, sampled once into a texture).
 *
 * In play a water physics volume covers the sea (from LandSideX out to sea, WaterVolumeDepth under sea
 * level up to it), so a character that falls in swims (MOVE_Swimming); IsSwimmable asks the same of a point.
 *
 * The tide (LyudTide, docs/tides.md) moves the water: the actor stands at the datum, high water of springs,
 * and SeaLevel() is the datum plus the tide now (never above it). The depth map holds the depth under the
 * datum; every depth the sea reports or draws is that plus the tide, so at low water the line walks out
 * across the flat and the waves shrink and break at its moving edge. lyud.TideOverride pins the level
 * (metres over the datum), lyud.TideSpeed runs the tide's clock faster.
 *
 * Under the surface a post process takes over (fog of the water's colour by distance, darker with
 * depth): the start of diving. The seabed and its life are ULyudSeabedComponent.
 *
 * Quality (lyud.Sea.Quality 0 phone .. 2 desktop; -1 = by platform) sets the mesh's density and the
 * number of wave trains. lyud.Sea 0 hides it all, for A/B benchmarks; lyud.Sea.Debug 1 prints the sea state.
 */
UCLASS()
class LYUDMILOVSK_API ALyudSea : public AActor
{
	GENERATED_BODY()

public:
	ALyudSea();
	virtual ~ALyudSea() override;

	/** The sea of a world (the first one), or null. */
	static ALyudSea* Get(const UObject* WorldContext);
	UFUNCTION(BlueprintPure, Category = "Sea", meta = (WorldContext = "WorldContext"))
	static ALyudSea* GetSea(const UObject* WorldContext) { return Get(WorldContext); }

	/** Single-layer water material with the wave code (unreal/scripts/make_sea_materials.py). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea|Look") TObjectPtr<UMaterialInterface> SeaMaterial;
	/** Post process material under the water. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea|Look") TObjectPtr<UMaterialInterface> UnderwaterMaterial;

	/** Open water upwind, km: how big the wind can make the waves (the Baltic: tens of km). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea|Waves", meta = (ClampMin = "1")) float FetchKm = 60.f;
	/** How sharp the crests are (Gerstner choppiness, kept under folding). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea|Waves", meta = (ClampMin = "0", ClampMax = "1.2")) float Choppiness = 0.8f;
	/** Multiplies the wind the waves grow in (0 = a mirror calm, 2 = a storm sea in the same wind). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea|Waves", meta = (ClampMin = "0")) float WindScale = 1.f;

	/** World X (cm) east of which there is no sea (the land side of the coast; X grows to the west). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea|Coast") float LandSideX = -1.0e9f;
	/** Depth beyond the ground's heights, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea|Coast") float OpenSeaDepthM = 25.f;
	/** Cell of the depth map read from the ground, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea|Coast", meta = (ClampMin = "1")) float DepthCellM = 4.f;
	/** Spawn a water physics volume over the sea in play, so characters swim in it (MOVE_Swimming). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea|Swimming") bool bWaterVolume = true;
	/** How deep the water volume reaches under sea level, cm, and its fluid friction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea|Swimming") float WaterVolumeDepth = 3000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea|Swimming") float FluidFriction = 0.6f;
	/** A swimming player's eyes are kept at least this far over the waves, cm (0 = off; diving will lift it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea|Swimming") float SwimEyeClearance = 22.f;
	/** Shallower than this the player stands (walks out onto the beach), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea|Swimming") float SwimMinDepth = 120.f;
	/** How far the surface mesh reaches from the camera, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea|Detail") float OuterRadius = 3.0e6f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sea") TObjectPtr<UStaticMeshComponent> Surface;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sea") TObjectPtr<UPostProcessComponent> Underwater;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sea") TObjectPtr<ULyudSeabedComponent> Seabed;

	/** The water at a world point (cm): surface height, slope and motion over it, depth, immersion. */
	UFUNCTION(BlueprintCallable, Category = "Sea") FLyudSeaSample SampleSurface(const FVector& Location) const;
	/** World Z of the surface over a point, cm. */
	UFUNCTION(BlueprintCallable, Category = "Sea") float SurfaceZ(const FVector& Location) const { return SampleSurface(Location).SurfaceZ; }
	/** Still-water depth at a point now (with the tide), cm (negative on land and on the bare flat). */
	UFUNCTION(BlueprintCallable, Category = "Sea") float DepthAt(const FVector& Location) const;
	/** Depth at a point if the water stood at WaterZ (world cm), cm: e.g. LowWaterZ() for what is always under water. */
	UFUNCTION(BlueprintCallable, Category = "Sea") float DepthAtLevel(const FVector& Location, float WaterZ) const;
	/** World Z of the still water now, cm: the datum plus the tide. */
	UFUNCTION(BlueprintCallable, Category = "Sea") float SeaLevel() const { return DatumZ() + TideCm; }
	/** World Z of the datum (high water of springs, the map's SEA_Z): where the actor stands, cm. */
	UFUNCTION(BlueprintCallable, Category = "Sea") float DatumZ() const { return GetActorLocation().Z; }
	/** The tide now: the water over the datum, cm (0 at spring high water, negative below). */
	UFUNCTION(BlueprintCallable, Category = "Sea") float TideOffsetCm() const { return TideCm; }
	/** The highest water there ever is (the datum), and the lowest of springs, world Z cm. */
	UFUNCTION(BlueprintCallable, Category = "Sea") float HighWaterZ() const { return DatumZ(); }
	UFUNCTION(BlueprintCallable, Category = "Sea") float LowWaterZ() const;
	/** This tide's low water, world Z cm (neaps ~2 m, springs ~4.5 m under the datum). */
	UFUNCTION(BlueprintCallable, Category = "Sea") float TideLowZ() const { return DatumZ() + TideLowCm; }
	/** Where the sand is still wet: the highest the water stood in the last hour, world Z cm. */
	UFUNCTION(BlueprintCallable, Category = "Sea") float WetLineZ() const { return DatumZ() + WetLineCm; }
	/** 0 neaps .. 1 springs; is the water coming in? */
	UFUNCTION(BlueprintCallable, Category = "Sea") float TideSpring() const { return TideSpringK; }
	UFUNCTION(BlueprintCallable, Category = "Sea") bool IsTideRising() const { return bTideRising; }
	/** Significant wave height now, cm, and the wind the waves were grown in, m/s. */
	UFUNCTION(BlueprintCallable, Category = "Sea") float WaveHeight() const;
	UFUNCTION(BlueprintCallable, Category = "Sea") float SeaWindSpeed() const;
	UFUNCTION(BlueprintCallable, Category = "Sea") bool IsCameraUnderwater() const { return bCameraUnder; }
	/** Is this point in the sea's water (under the wavy surface, over the bed, on the sea side)? */
	UFUNCTION(BlueprintCallable, Category = "Sea") bool IsSwimmable(const FVector& Location) const;
	/** The water physics volume over the sea in play (null outside play or with bWaterVolume off). */
	UFUNCTION(BlueprintCallable, Category = "Sea") APhysicsVolume* GetWaterVolume() const { return WaterVolume; }
	/** Is the ground's depth map read in yet? */
	UFUNCTION(BlueprintCallable, Category = "Sea") bool IsDepthReady() const { return DepthRow >= DepthH && DepthH > 0; }
	UFUNCTION(BlueprintCallable, Category = "Sea") FString GetDebugReport() const;
	/** World Z of the ground (the landscape) under a point, cm; bOk false where there is none. */
	float GroundZ(double X, double Y, bool& bOk) const;
	/** The wave clock (s, wrapped to the loop) the surface shows now. */
	double WaveClock() const;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

private:
	void ApplyQuality(int32 Quality);
	void BuildSurfaceMesh();
	void EnsureMaterial();
	void UpdateWaves(bool bForce);
	void UploadWaveTable();
	void StepDepthMap(int32 Budget);
	void UploadDepthMap();
	/** The hollows on the tidal flat that keep water when the ebb leaves them (PoolW). */
	void FindPools();
	/** Depth under the datum, m (the depth map). */
	float DepthAtM(double XM, double YM) const;
	/** Still-water depth now (with the tide), m. */
	float WaterDepthAtM(double XM, double YM) const { return DepthAtM(XM, YM) + TideCm * 0.01f; }
	/** The tide's level from the sky's clock (or the calendar), the override and speed cvars; moves the water volume. */
	void UpdateTide(float DeltaSeconds);
	bool ViewLocation(FVector& Out) const;

	TPimplPtr<LyudWeather::FWaveField> Field;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SeaMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> UnderMID;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> WaveTable;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> DepthTex;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> SurfaceMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialParameterCollection> Collection;
	UPROPERTY(Transient) TObjectPtr<APhysicsVolume> WaterVolume;
	void SpawnWaterVolume();
	/** Holds a player in the sea in its water (the volume's flat top would let wave crests lift them out) and
	 * lifts them so the camera stays over the waves (eases up, snaps when far under). */
	void KeepSwimmerHeadUp(float DeltaSeconds);
	FString GetDebugReportCore() const;

	float TideCm = 0.f;
	float TideLowCm = 0.f;
	float WetLineCm = 0.f;
	float TideSpringK = 0.f;
	bool bTideRising = false;
	/** Hours the tide's clock runs ahead of the sky's (lyud.TideSpeed). */
	double TideExtraHours = 0.0;
	float TideAge = 1e9f;
	float VolumeTideCm = 0.f;

	int32 AppliedQuality = -2;
	int32 Segments = 128;
	float MinStepCm = 25.f;
	double Clock = 0.0;
	float WaveAge = 1e9f;
	float AppliedWind = -1.f;
	FVector2D AppliedDir = FVector2D::ZeroVector;
	float AppliedFetch = -1.f, AppliedChop = -1.f;
	bool bCameraUnder = false;
	TWeakObjectPtr<class ALyudSky> Sky;
	TWeakObjectPtr<ACharacter> Swimmer;
	bool bHoldingSwimmer = false;

	/** Depth map: still-water depth in m over a rectangle of the world (m), read row by row from the landscape. */
	TArray<float> Depth;
	/** Water a hollow on the flat keeps at low tide, m, per depth cell. */
	TArray<float> PoolW;
	int32 DepthW = 0, DepthH = 0, DepthRow = 0;
	FVector2D DepthMin = FVector2D::ZeroVector;
	FVector2D DepthSize = FVector2D(1.0, 1.0);
	bool bDepthUploaded = false;
	bool bDepthStarted = false;
};

/**
 * Makes the body it is attached to float: a few points (offsets in the parent's space) each push up by
 * the water they displace and are dragged along by the water's motion. Attach to a simulating primitive.
 */
UCLASS(ClassGroup = (Lyudmilovsk), meta = (BlueprintSpawnableComponent))
class LYUDMILOVSK_API ULyudBuoyancyComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	ULyudBuoyancyComponent();

	/** Floating points in the body's local space, cm; each carries an equal share of the volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buoyancy") TArray<FVector> Points;
	/** Volume of water the whole body displaces when fully under, litres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buoyancy") float VolumeLitres = 100.f;
	/** Height over which a point goes from dry to fully under, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buoyancy") float PointHeight = 30.f;
	/** Drag against the water per point (linear, kg/s), and against the air. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buoyancy") float WaterDrag = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buoyancy") float AirDrag = 0.2f;

	/** Share of the points under water last frame (0..1). */
	UFUNCTION(BlueprintCallable, Category = "Buoyancy") float GetSubmerged() const { return Submerged; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float Submerged = 0.f;
};

/**
 * Puts an ALyudSea into a game world that still has the old flat sea plane (a mesh with M_BalticSea,
 * from import_sky.py before the sea had its own actor): hides the plane and spawns the sea at its level,
 * with the land side at the plane's eastern edge. lyud.Sea.ReplacePlane 0 keeps the old plane.
 */
UCLASS()
class LYUDMILOVSK_API ULyudSeaSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
};
