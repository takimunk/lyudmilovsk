#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudWeather.generated.h"

/** The sky over the outdoor map at one moment (LyudWeather::FSkyState). */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudSkyWeather
{
	GENERATED_BODY()

	/** 0 clear .. 1 overcast. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float Cloud = 0.f;
	/** Falling water, mm/h (drizzle 0.3, rain 2, a downpour 10+); snow counts as its water. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float Rain = 0.f;
	/** 0 rain .. 1 all of it falls as snow. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float Snow = 0.f;
	/** 0 .. 1 how stormy a shower is (lightning about 2 x this per minute). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float Thunder = 0.f;
	/** 0 .. 1 mist (visibility ~10 km down to ~200 m). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float Fog = 0.f;
	/** Air temperature at 2 m, C. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float TempC = 10.f;
	/** 0 dry .. 1 soaked ground (follows the rain, dries over hours). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float Wet = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLyudLightning, FVector, Location, float, DistanceFromViewM);

/**
 * The weather beyond the wind (docs/sea_and_wind.md, "Clouds and rain"): cloud, rain or snow, thunderstorms, mist, the
 * air's temperature and how wet the ground is, over the whole outdoor map. Like the wind it is a pure function of the
 * game calendar (LyudWeather::Sky in Private/Weather/RainModel.cpp; tools/audiosim/run.sh weather prints its climate),
 * and it rides on the wind's weather: the fronts that bring a fresh wind bring the cloud and rain with them.
 *
 * It sets lyud.Overcast (1 when the cloud is over 0.75) for what people say about the sun, and in a thunderstorm
 * fires OnLightning now and then with where the strike is (the sound's thunder follows by the speed of sound; the
 * sky can flash on it). Rain, clouds and wet materials on screen are for the sky and sea threads to build on GetSky().
 *
 * Console: lyud.Weather.Rain / Cloud / Fog / Temp force a value (negative = the calendar's), lyud.Weather.Strike
 * [metres] makes lightning now, lyud.Weather.Debug 1 prints the weather on screen.
 */
UCLASS()
class LYUDMILOVSK_API ULyudWeatherSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudWeatherSubsystem* Get(const UObject* WorldContext);
	/** For Blueprints and Python: unreal.LyudWeatherSubsystem.get_weather(world). */
	UFUNCTION(BlueprintPure, Category = "Weather", meta = (WorldContext = "WorldContext"))
	static ULyudWeatherSubsystem* GetWeather(const UObject* WorldContext) { return Get(WorldContext); }

	/** Picks the sequence of days (with the wind's own seed). 2660: broken cloud and dry over the scene's start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") int32 Seed = 2660;
	/** How much more often than the Baltic climate it rains (0 = never). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") float Wetness = 1.f;

	/** The sky now. */
	UFUNCTION(BlueprintCallable, Category = "Weather") FLyudSkyWeather GetSky() const;
	/** Lightning: a strike somewhere (world cm) and how far it is from the camera, m. */
	UPROPERTY(BlueprintAssignable, Category = "Weather") FLyudLightning OnLightning;
	/** A strike DistanceM from the camera now, in a random direction (tests, the console). */
	UFUNCTION(BlueprintCallable, Category = "Weather") void Strike(float DistanceM);
	/** Text for logs and tests. */
	UFUNCTION(BlueprintCallable, Category = "Weather") FString DebugReport() const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	mutable FLyudSkyWeather Cached;
	mutable double CachedHours = -1e9;
	mutable float LastForce[4] = { -2.f, -2.f, -2.f, -2.f };
	float NextStrike = 0.f;
	int32 LastOvercast = -1;
	bool CameraLocation(FVector& Out) const;
};
