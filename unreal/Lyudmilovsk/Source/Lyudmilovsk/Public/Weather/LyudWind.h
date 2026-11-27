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

/**
 * The air at a point with the land and the sea taken in (ULyudWindSubsystem::GetLocalWind): the lee of the cliff, of
 * the forest and of houses, the speed-up over crests, ridge lift and lee sink, the freer wind over open water.
 */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudLocalWind
{
	GENERATED_BODY()

	/** World velocity of the air, m/s; Z is rising (ridge lift) or sinking (lee) air. */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") FVector Velocity = FVector::ZeroVector;
	/** Horizontal speed, m/s. */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float Speed = 0.f;
	/** Compass bearing the air here comes from, degrees. */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float FromDegrees = 0.f;
	/** The open-air gust factor here (1 = mean, 1.5 = a strong gust, 0.5 = a lull). */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float Gust = 1.f;
	/** What the land does to the speed: 0 dead lee .. 1 open .. about 1.4 on a crest or a headland. */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float Exposure = 1.f;
	/** 0 open .. 1 deep in a lee. */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float Shelter = 0.f;
	/** Rising (+) or sinking (-) air, m/s. */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float Updraft = 0.f;
	/** The tumbling the mean does not carry, rms m/s (rotor behind the cliff, rough land, gusty air). */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float Turbulence = 0.f;
	/** Height of the point over the ground or the water under it, m. */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float HeightM = 10.f;
	/** Open water upwind of the point, m (0 over land, capped at 3000). */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float FetchM = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Wind") bool bOverWater = false;
};

/** The slow swing of the direction and the area's breathing now (LyudWeather::FWindShift). */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudWindShift
{
	GENERATED_BODY()

	/** Degrees the wind stands veered (+, clockwise) or backed (-) from the weather's direction now. */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float VeerDegrees = 0.f;
	/** The area's puff (> 1) or lull (< 1) on the weather's speed. */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float Factor = 1.f;
	/** How far it swings either way today, degrees, and how long a swing takes, game minutes. */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float RangeDegrees = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float PeriodMinutes = 8.f;
	/** Game minutes until the veer next changes sign. */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float NextTurnMinutes = 0.f;
};

/** What the next hours hold (LyudWeather::FWindOutlook). */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudWindOutlook
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Wind") float MinSpeed = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float MaxSpeed = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float MeanSpeed = 0.f;
	/** The strongest gust to expect, m/s. */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float GustSpeed = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float FromDegrees = 0.f;
	/** How the direction ends against how it starts, degrees (+ veers, - backs). */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float TurnDegrees = 0.f;
	/** m/s per hour: freshening > 0, easing < 0. */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") float Trend = 0.f;
	/** Beaufort force of the mean and of the strongest gust. */
	UPROPERTY(BlueprintReadOnly, Category = "Wind") int32 Force = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Wind") int32 ForceMax = 0;
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
	// ---- The wind where you stand (docs/wind.md). GetWindAt above stays what it was: the open-air wind over a flat
	// world. These take the land and the sea in, and are what sails, kites, flags, smoke and birds should ask.

	/**
	 * The air at a world point (cm): lee and speed-up of the relief, ridge lift, turbulence, the freer wind over
	 * open water, with the gusts. The height over the ground or water is worked out from the point itself.
	 * Before the relief grid is ready (a few seconds after the level starts) it answers as over open flat land.
	 */
	UFUNCTION(BlueprintCallable, Category = "Wind")
	FLyudLocalWind GetLocalWind(const FVector& Location) const;
	/** The same for something that stands a known height over its ground (a flag on a 6 m pole, a mast head). */
	UFUNCTION(BlueprintCallable, Category = "Wind")
	FLyudLocalWind GetLocalWindAbove(const FVector& GroundLocation, float HeightM) const;
	/** The wind felt by something moving (world velocity cm/s) at a point: local wind minus its own motion, m/s. */
	UFUNCTION(BlueprintCallable, Category = "Wind")
	FVector GetApparentWind(const FVector& Location, const FVector& BodyVelocityCmS) const;
	/** The slow swing of the direction now (already inside GetMeanWind, GetFromDegrees and every sample). */
	UFUNCTION(BlueprintCallable, Category = "Wind") FLyudWindShift GetShift() const;
	/** The weather's direction without the swing, compass degrees it comes from. */
	UFUNCTION(BlueprintCallable, Category = "Wind") float GetSteadyFromDegrees() const;
	/** The open-air gust factor that will be at Location in Seconds (real seconds) if the wind holds. */
	UFUNCTION(BlueprintCallable, Category = "Wind") float GetGustAhead(const FVector& Location, float Seconds) const;
	/** Seconds until the next gust (or lull) worth a word reaches Location, its factor; false if none within WithinSeconds. */
	UFUNCTION(BlueprintCallable, Category = "Wind")
	bool GetNextGust(const FVector& Location, float WithinSeconds, float& OutInSeconds, float& OutFactor, bool& bOutLull) const;
	/** Beaufort force 0..12 of the mean wind now (or of any speed: BeaufortOf). */
	UFUNCTION(BlueprintCallable, Category = "Wind") int32 GetBeaufort() const;
	UFUNCTION(BlueprintPure, Category = "Wind") static int32 BeaufortOf(float SpeedMs);
	/** String-table keys: "wind.force.<0..12>" and "wind.from.<n|ne|e|se|s|sw|w|nw>" (Content/Localization). */
	UFUNCTION(BlueprintPure, Category = "Wind") static FString BeaufortKey(int32 Force);
	UFUNCTION(BlueprintPure, Category = "Wind") static FString CompassKey(float FromDegrees);
	/** "SW 4, gusts 6": a short line for a board, the paper, a harbour master (localized through the keys above). */
	UFUNCTION(BlueprintCallable, Category = "Wind") FText DescribeWind() const;
	/** What the next game hours hold. */
	UFUNCTION(BlueprintCallable, Category = "Wind") FLyudWindOutlook GetOutlook(float SpanHours = 3.f) const;
	/** Game hours until the mean wind stays within [MinSpeed, MaxSpeed] for HoldHours (0 = now); -1 = not in 48 h. */
	UFUNCTION(BlueprintCallable, Category = "Wind")
	float HoursUntilWind(float MinSpeed, float MaxSpeed, float HoldHours = 1.f, bool bDaylight = true) const;
	/** The relief grid is built (GetLocalWind answers with the land in). */
	UFUNCTION(BlueprintCallable, Category = "Wind") bool IsReliefReady() const;

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
	mutable float LastShift = 0.f;
	double DriftX = 0.0, DriftY = 0.0, GustClock = 0.0;
	UPROPERTY(Transient) TObjectPtr<UMaterialParameterCollection> Collection;
	bool bCollectionLooked = false;
};
