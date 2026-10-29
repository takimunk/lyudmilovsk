#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Weather/LyudWeather.h"
#include "LyudClouds.generated.h"

class ALyudSky;
class UMaterialInstanceDynamic;

/** Everything the clouds are drawn from: the weather engine's layers (FLyudWeatherState) plus the wind aloft and what falls. */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudCloudSky
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clouds") FLyudCloudLayer Low;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clouds") FLyudCloudLayer Mid;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clouds") FLyudCloudLayer High;
	/** 0..1 lens clouds (strong stable moist flow aloft). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clouds") float Lenticular = 0.f;
	/** 0..1 share of the sky in shower/Cb cells. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clouds") float ShowerCells = 0.f;
	/** Wind at 10 m (the low clouds drift a little faster and veered) and at ~5 km (the middle and high clouds). Bearings it comes from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clouds") float WindFromDeg = 235.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clouds") float WindSpeed = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clouds") float UpperFromDeg = 260.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clouds") float UpperSpeed = 20.f;
	/** mm/h falling (rain shafts under the clouds), 0..1 thunder, 0..1 chance of a 22-degree halo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clouds") float PrecipRate = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clouds") float Thunder = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clouds") float Halo = 0.f;
};

/**
 * Draws the weather's clouds (docs/clouds.md). Each tick it takes the cloud layers from the weather, turns each genus
 * into shape numbers (cell size, deck or heaps, rounded tops, anvil, erosion, streets, ragged base, isolated towers,
 * rain shafts) eased over a few seconds so a change of genus morphs instead of popping, drifts the layers with their
 * winds and sets all of it on ALyudSky's materials: the low and middle layers, lens clouds and rain shafts on the
 * volumetric clouds (M_LyudClouds, tools/sky/cloud_hlsl.py VOLUME_HLSL), whose slab it fits round the layers, and the
 * high layer (cirrus, cirrocumulus, cirrostratus with its halo) on the sky dome (HIGH_HLSL). A lightning strike lights
 * the cloud round it.
 *
 * Console: lyud.Clouds.Preset <name|off> forces a sky (clear, cu, cucon, cb, sc, st, ns, ac, as, lens, ci, cc, cs,
 * warmfront, coldfront, showers), lyud.Clouds.Cover multiplies every layer's cover (tests), lyud.Clouds.Drift scales
 * the drift, lyud.Clouds.Debug 1 prints the layers.
 */
UCLASS()
class LYUDMILOVSK_API ULyudCloudSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudCloudSubsystem* Get(const UObject* WorldContext);
	/** For Blueprints and Python: unreal.LyudCloudSubsystem.get_clouds(world). */
	UFUNCTION(BlueprintPure, Category = "Clouds", meta = (WorldContext = "WorldContext"))
	static ULyudCloudSubsystem* GetClouds(const UObject* WorldContext) { return Get(WorldContext); }

	/** The sky the clouds are drawn from now (after a preset or the weather). */
	UFUNCTION(BlueprintCallable, Category = "Clouds") FLyudCloudSky GetCloudSky() const { return Target; }
	/** Forces a sky (tests, Blueprints); ClearOverride gives it back to the weather. */
	UFUNCTION(BlueprintCallable, Category = "Clouds") void SetOverride(const FLyudCloudSky& Sky);
	UFUNCTION(BlueprintCallable, Category = "Clouds") void ClearOverride();
	/** A named sky (see the class comment); false if the name is unknown. */
	UFUNCTION(BlueprintCallable, Category = "Clouds") bool SetPreset(const FString& Name);
	/** Snap the eased shapes to the target now (tests, a jump of the clock). */
	UFUNCTION(BlueprintCallable, Category = "Clouds") void Snap() { bSnap = true; }
	UFUNCTION(BlueprintCallable, Category = "Clouds") FString DebugReport() const;

	/** The sky for a preset name. */
	static bool PresetSky(const FString& Name, FLyudCloudSky& Out);
	/** The weather's sky now: the weather subsystem's layers, or until it has them, layers made up from its cloud/rain/thunder. */
	FLyudCloudSky FromWeather() const;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual bool IsTickableInEditor() const override { return true; }

	/** The shape numbers of one volumetric layer, as the shader takes them (five float4s, see cloud_hlsl.py). */
	struct FShape
	{
		FVector4f A = FVector4f(0.f), B = FVector4f(1.f, 0.f, 0.f, 0.f), C = FVector4f(0.f, 1.f, 0.f, 0.f), E = FVector4f(0.f);
	};
	static FShape ShapeOf(const FLyudCloudLayer& L, float PrecipRate, float ShowerCells);

private:
	ALyudSky* FindSky();
	void Apply(ALyudSky* Sky, float Dt);
	UFUNCTION() void OnStrike(FVector Location, float DistanceM);

	TWeakObjectPtr<ALyudSky> SkyActor;
	bool bOverride = false;
	bool bSnap = true;
	FLyudCloudSky Override;
	FLyudCloudSky Target;
	FShape Shape[2];
	/** High layer: cover, altitude km, density; fibres, grain, veil, halo. */
	FVector4f HighA = FVector4f(0.f), HighB = FVector4f(0.f);
	FVector4f LensA = FVector4f(0.f);
	/** Drift of the low, middle and high layers, km (the shader samples at position + offset). */
	FVector2D Drift[3] = { FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector };
	float SlabBottom = -1.f, SlabHeight = -1.f;
	float FlashLeft = 0.f;
	FVector4f FlashAt = FVector4f(0.f);
	bool bBound = false;
	double DebugNext = 0.0;
	FString LastPreset;
};
