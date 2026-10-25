#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudWind.generated.h"

class UMaterialParameterCollection;

/** The settings of the wind's weather (LyudWeather::FWindParams): where it comes from, how strong, how it varies. */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudWindSettings
{
	GENERATED_BODY()

	/** Compass bearing the wind comes from most often (225 = south-west; the Baltic's autumn). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind") float PrevailingFromDeg = 235.f;
	/** How far the weather swings it either way over a day or two, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind") float DirectionSwingDeg = 75.f;
	/** Median wind of the weather at 10 m over the sea, m/s, and its spread (log-normal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind") float MedianSpeed = 6.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind") float SpeedLogSigma = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind") float MaxSpeed = 20.f;
	/** Night wind as a share of the afternoon's on a windy night (quiet nights go almost still). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind") float NightFactor = 0.6f;
	/** Gust spread as a share of the mean, by day and by night. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind") float GustinessDay = 0.32f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind") float GustinessNight = 0.14f;
	/** Onshore breeze on a calm sunny afternoon, m/s, and the bearing the sea lies at from the coast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind") float SeaBreeze = 2.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind") float SeaBearingDeg = 270.f;
	/** Picks the weather: every seed is a different sequence of days (921: a breezy WSW afternoon on 20 October 2026). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind") int32 Seed = 921;
};

/** The wind at a point at one moment. */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudWindSample
{
	GENERATED_BODY()

	/** World velocity of the air, m/s (horizontal). */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") FVector Velocity = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float Speed = 0.f;
	/** The gust here: factor on the area's mean wind (1 = mean, 1.5 = a strong gust, 0.5 = a lull). */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float Gust = 1.f;
};

/**
 * The wind over the world, its own thing that everything outdoors can ask (trees, grass, birds, the sea,
 * smoke, sails): a direction and strength that follow the game calendar, gusts drifting over the ground,
 * calmer nights and stirred afternoons, and on quiet sunny days a sea breeze. It is a pure function of the
 * date and hour (LyudWeather in Private/Weather/WindModel.cpp; tools/sea/run.sh runs it outside the
 * engine), so saves, jumps of the clock and every machine agree on the weather.
 *
 * Every frame of play it also writes the wind into the material parameter collection
 * /Game/Weather/MPC_LyudWeather (Wind = dir x, dir y, speed m/s, gustiness; WindDrift = how far the air
 * has drifted x, y in m wrapped to 4096, the gust clock s, how stirred the day is), so materials can
 * sway with the same gusts (tools/sea/sea_hlsl.py GUST_HLSL computes the gust field from those).
 *
 * Console: lyud.Wind.Speed / lyud.Wind.From / lyud.Wind.Gustiness force a value (negative = the
 * calendar's), lyud.Wind.Debug 1 prints the wind on screen. The time follows UGameCalendar plus
 * lyud.SkyHourOffset, the same as the sky.
 */
UCLASS()
class LYUDMILOVSK_API ULyudWindSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudWindSubsystem* Get(const UObject* WorldContext);
	/** For Blueprints and Python: unreal.LyudWindSubsystem.get_wind(world). */
	UFUNCTION(BlueprintPure, Category = "Wind", meta = (WorldContext = "WorldContext"))
	static ULyudWindSubsystem* GetWind(const UObject* WorldContext) { return Get(WorldContext); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind") FLyudWindSettings Settings;

	/** The wind at a world point (cm) with its gust, at a height over the ground (m; 10 = the standard). */
	UFUNCTION(BlueprintCallable, Category = "Wind")
	FLyudWindSample GetWindAt(const FVector& Location, float HeightAboveGroundM = 10.f, bool bOverWater = false) const;
	/** The area's mean wind now (no gusts), world velocity m/s at 10 m. */
	UFUNCTION(BlueprintCallable, Category = "Wind") FVector GetMeanWind() const;
	UFUNCTION(BlueprintCallable, Category = "Wind") float GetMeanSpeed() const;
	/** Compass bearing the wind comes from, degrees (270 = from the west, off the sea). */
	UFUNCTION(BlueprintCallable, Category = "Wind") float GetFromDegrees() const;
	/** Gust spread as a share of the mean (0.14 quiet night .. 0.35 gusty afternoon). */
	UFUNCTION(BlueprintCallable, Category = "Wind") float GetGustiness() const;
	/** 0 at night .. 1 the stirred afternoon air. */
	UFUNCTION(BlueprintCallable, Category = "Wind") float GetDayStir() const;
	/** The wind the sea's waves have grown in (the last few hours' mean), world velocity m/s. */
	UFUNCTION(BlueprintCallable, Category = "Wind") FVector GetSeaStateWind() const;
	/** How far the air has drifted (m, wrapped to 4096) and the gust clock (s): what materials get. */
	FVector GetDrift() const { return FVector(DriftX, DriftY, GustClock); }
	/** Local time the wind is for (calendar + lyud.SkyHourOffset). */
	UFUNCTION(BlueprintCallable, Category = "Wind") FDateTime WindTime() const;
	/** Text for logs and tests. */
	UFUNCTION(BlueprintCallable, Category = "Wind") FString DebugReport() const;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FState
	{
		double Hours = -1e9;
		FVector Mean = FVector::ZeroVector;
		FVector Sea = FVector::ZeroVector;
		float Speed = 0.f, FromDeg = 0.f, Gustiness = 0.f, Day = 0.f;
	};
	/** The calendar's wind, recomputed when the clock has moved (cheap; cached per hour fraction). */
	const FState& State() const;
	double NowHours() const;

	mutable FState Cached;
	mutable float LastForce[3] = { -2.f, -2.f, -2.f };
	double DriftX = 0.0, DriftY = 0.0, GustClock = 0.0;
	UPROPERTY(Transient) TObjectPtr<UMaterialParameterCollection> Collection;
	bool bCollectionLooked = false;
};
