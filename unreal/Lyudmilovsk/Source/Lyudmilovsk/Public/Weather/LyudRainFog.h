#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudRainFog.generated.h"

class UDecalComponent;
class UExponentialHeightFogComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMaterialParameterCollection;
class ALyudSky;
class UStaticMeshComponent;
class UTexture2D;

/**
 * What the rain and fog effects need from the weather, in one place: the weather engine's state is read here and
 * nowhere else in the effects, so a change of the engine touches only ULyudRainFogSubsystem::ReadWeather.
 */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudRainFogWeather
{
	GENERATED_BODY()

	/** Liquid water falling, mm/h (the snow's share is taken out: snow is drawn by the winter effects). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float RainRate = 0.f;
	/** 0 rain .. 1 drizzle (fine slow drops in a dense mist of them). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float Drizzle = 0.f;
	/** 0 .. 1 how much of what falls is snow (sleet is between). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float Snow = 0.f;
	/** 0 dry .. 1 soaked ground with puddles. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float Wet = 0.f;
	/** 0 .. 1 cloud cover (the light the rain and the fog get). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float Cloud = 0.f;
	/** Horizontal visibility in the general air (haze, rain), m. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float VisibilityM = 30000.f;
	/** 0 .. 1 a fog layer lying on the ground or the sea. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float Fog = 0.f;
	/** Visibility inside that fog layer, m. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float FogVisibilityM = 30000.f;
	/** How deep the fog layer is, m (ground fog a few metres, sea fog tens). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float FogTopM = 0.f;
	/** Height of the fog layer's floor above sea level, m (the sea's or the valley's). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float FogBaseM = 0.f;
	/** The wind at the drops, world cm/s. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") FVector Wind = FVector::ZeroVector;
};

/**
 * The rain and the fog on screen around the camera (docs/rain_and_fog.md):
 *  - drops: one mesh of streak quads moved entirely in the vertex shader, wrapped in a box around the camera in
 *    world space (no swimming as you walk), slanted by the wind, fine and slow in drizzle; splashes on whatever the
 *    rain hits; on the sea the water material draws the rings itself (lyud.Rain into MPC_LyudWeather);
 *  - a roof map: a grid of downward traces around the camera keeps the height of the topmost thing the rain meets,
 *    so no rain falls under eaves, in the club, the train or a cabin, and no floor under a roof gets wet;
 *  - wet ground: one deferred decal around the camera darkens and smooths what the sky rains on, with puddles
 *    rippling on the flat;
 *  - fog: the level's height fog becomes the weather's: haze from the visibility, and a second layer lying on the sea
 *    or the valley (sea fog, ground fog), lit by the sky; it fades out indoors (the club keeps its own fog).
 *
 * Console: lyud.Rain.Fx / lyud.Fog.Fx 0 turn them off, lyud.Rain.Debug 1 prints the state.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudRainFx : public AActor
{
	GENERATED_BODY()

public:
	ALyudRainFx();

	/** Number of streaks at the heaviest rain (fewer are drawn for lighter rain). */
	UPROPERTY(EditAnywhere, Category = "Rain") int32 MaxDrops = 14000;
	UPROPERTY(EditAnywhere, Category = "Rain") int32 MaxSplashes = 3000;
	/** The box of drops around the camera, cm (sides, height). */
	UPROPERTY(EditAnywhere, Category = "Rain") FVector2D DropBox = FVector2D(3000.f, 2000.f);
	UPROPERTY(EditAnywhere, Category = "Rain") float SplashBox = 2400.f;
	/** Roof map: cells per side and their size, cm. */
	UPROPERTY(EditAnywhere, Category = "Rain") int32 RoofCells = 64;
	UPROPERTY(EditAnywhere, Category = "Rain") float RoofCellSize = 150.f;
	/** Downward traces per frame to keep the roof map fresh. */
	UPROPERTY(EditAnywhere, Category = "Rain") int32 TracesPerFrame = 256;

	UPROPERTY(VisibleAnywhere, Category = "Rain") TObjectPtr<UStaticMeshComponent> Drops;
	UPROPERTY(VisibleAnywhere, Category = "Rain") TObjectPtr<UStaticMeshComponent> Splashes;
	UPROPERTY(VisibleAnywhere, Category = "Rain") TObjectPtr<UDecalComponent> WetDecal;

	/** 0 .. 1 how much open sky is around the camera (0 in the club or a tunnel); the effects fade with it. */
	float GetOpenSky() const { return OpenSky; }
	/** Height of the topmost surface the rain meets at Location (world cm), or false outside the roof map. */
	bool RoofHeightAt(const FVector& Location, float& OutZ) const;
	FString DebugReport() const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void BuildMeshes();
	void UpdateRoofMap(const FVector& Camera);
	void UpdateMaterials(const FLyudRainFogWeather& W, const FVector& Camera, float DeltaSeconds);
	void UpdateFog(const FLyudRainFogWeather& W, float DeltaSeconds);
	float SkyLuminance(const FLyudRainFogWeather& W) const;

	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DropMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SplashMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> WetMID;
	UPROPERTY() TObjectPtr<UTexture2D> RoofTex;
	UPROPERTY() TObjectPtr<UMaterialParameterCollection> Collection;
	TWeakObjectPtr<ALyudSky> Sky;
	TWeakObjectPtr<UExponentialHeightFogComponent> Fog;

	/** Roof map: top height (cm) and 1 where it is the sea, per cell, row-major from Origin. */
	TArray<FVector2f> Roof;
	TArray<uint8> Dirty;
	FIntPoint Origin = FIntPoint(MAX_int32, MAX_int32);
	int32 Cursor = 0;
	int32 DirtyLeft = 0;
	float SeaZ = -1e7f;
	float OpenSky = 0.f;
	float OpenSkyRaw = 0.f;
	bool bRoofUploaded = false;

	/** The level's fog as it was (the club's), restored indoors. */
	struct FFogState
	{
		float Density = 0.f, Falloff = 0.2f, Start = 0.f, Second = 0.f, SecondFalloff = 0.2f, SecondHeight = 0.f;
		FLinearColor Inscatter = FLinearColor::Black;
		float ZOffset = 0.f;
	} FogBase;
	bool bFogTaken = false;
	float FogWeight = 0.f;
	float Smoothed[4] = { 0.f, 0.f, 0.f, 0.f };
};

/** Spawns the rain and fog effects into an outdoor world (one that has ALyudSky) and gives the weather they draw. */
UCLASS()
class LYUDMILOVSK_API ULyudRainFogSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudRainFogSubsystem* Get(const UObject* WorldContext);
	/** For Blueprints and Python: unreal.LyudRainFogSubsystem.get_rain_fog(world). */
	UFUNCTION(BlueprintPure, Category = "Weather", meta = (WorldContext = "WorldContext"))
	static ULyudRainFogSubsystem* GetRainFog(const UObject* WorldContext) { return Get(WorldContext); }

	/** The weather the effects show now (forced values from lyud.Weather.* included). */
	UFUNCTION(BlueprintCallable, Category = "Weather") FLyudRainFogWeather ReadWeather() const;
	/** 0 .. 1 open sky around the camera (0 indoors): for sounds that change under a roof. */
	UFUNCTION(BlueprintCallable, Category = "Weather") float GetOpenSky() const;
	UFUNCTION(BlueprintCallable, Category = "Weather") ALyudRainFx* GetEffects() const { return Fx.Get(); }
	UFUNCTION(BlueprintCallable, Category = "Weather") FString DebugReport() const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TWeakObjectPtr<ALyudRainFx> Fx;
	bool bLooked = false;
};
