#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudWeather.generated.h"

namespace LyudWeather { struct FWeatherState; class FChain; class FSeason; }

/** Synoptic weather types (LyudWeather::ERegime, same order). docs/weather.md tells their story. */
UENUM(BlueprintType)
enum class ELyudWeatherRegime : uint8
{
	High,         // anticyclone: settled, light winds; clear, or a grey inversion deck in winter
	WarmFront,    // cirrus -> cirrostratus (halo) -> altostratus -> nimbostratus and steady rain, wind backing S
	WarmSector,   // mild damp air: low stratus, drizzle, sea fog in spring
	ColdFront,    // squall line: Cb, a burst of heavy rain or snow, gusts, the wind veering NW, pressure jumps
	Showery,      // polar air behind the front: Cu/Cb, showers and bright spells, rainbows
	Occluded,     // a slow low overhead: grey, long rain or drizzle
	ArcticHigh,   // cold continental high: hard frost, dry E wind, steam fog over open sea
	ThunderyLow   // summer: hot humid, thunderstorms, hail
};

UENUM(BlueprintType)
enum class ELyudPrecipType : uint8
{
	None, Drizzle, Rain, Showers, Thunderstorm, Hail, Graupel, Sleet, Snow, SnowShowers, FreezingDrizzle, FreezingRain
};

/** Cloud genera (LyudWeather::ECloud, same order). */
UENUM(BlueprintType)
enum class ELyudCloudType : uint8
{
	None,
	Cirrus, Cirrocumulus, Cirrostratus,
	Altocumulus, Altostratus, AltocumulusLenticular,
	Nimbostratus, Stratocumulus, Stratus,
	Cumulus, CumulusCongestus, Cumulonimbus
};

UENUM(BlueprintType)
enum class ELyudFogType : uint8
{
	None,
	Radiation,   // clear calm nights over land, shallow, lifts in the morning sun
	Advection,   // sea fog: warm moist air over the cold sea, rolls ashore, deep
	Steam,       // sea smoke: arctic air over open water, wisps over the sea only
	Frontal      // mist in steady rain under a low deck
};

/** One cloud layer (low, middle or high). */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudCloudLayer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Weather") ELyudCloudType Type = ELyudCloudType::None;
	/** 0..1 of the sky. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float Cover = 0.f;
	/** Base and top, m above sea level. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float BaseM = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float TopM = 0.f;
	/** 0 thin (the sun shows through) .. 1 dark. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather") float Density = 0.f;
};

/**
 * The whole weather at one moment (LyudWeather::FWeatherState; the same fields, Unreal types). Over the whole map;
 * local effects (shower patches, drifts, the unfrozen shore strip) are for whoever draws them.
 */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudWeatherState
{
	GENERATED_BODY()

	// the synoptic episode
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Synoptic") ELyudWeatherRegime Regime = ELyudWeatherRegime::High;
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Synoptic") ELyudWeatherRegime NextRegime = ELyudWeatherRegime::High;
	/** 0 at the episode's start .. 1 at its end; hours until the next one. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Synoptic") float RegimeProgress = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Synoptic") float HoursToNext = 0.f;
	/** 0..1 how deep the low (or strong the high). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Synoptic") float Intensity = 0.5f;
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Synoptic") float PressureHPa = 1013.f;
	/** hPa per 3 h (falling ahead of a low, jumping up behind a cold front). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Synoptic") float PressureTendency = 0.f;

	// the air
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Air") float TempC = 10.f;
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Air") float DewPointC = 6.f;
	/** Relative humidity 0..1. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Air") float Humidity = 0.75f;
	/** The 0 C level, m above sea level (0 when it freezes at the ground). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Air") float FreezingLevelM = 2000.f;
	/** 0..1 how stable the low air is (smoke and fog stay low). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Air") float Inversion = 0.f;
	/** 0..1 thermals (cumulus growth). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Air") float Convection = 0.f;

	// the weather's wind (ULyudWindSubsystem has the wind at a point, with the day's rhythm and gusts)
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Wind") float WindFromDeg = 235.f;
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Wind") float WindSpeed = 6.f;
	/** Extra gust spread fronts and showers add. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Wind") float WindGustiness = 0.f;
	/** The wind at ~5 km: where the clouds drift from, m/s. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Wind") float UpperFromDeg = 260.f;
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Wind") float UpperSpeed = 20.f;

	// clouds
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Clouds") FLyudCloudLayer Low;
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Clouds") FLyudCloudLayer Mid;
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Clouds") FLyudCloudLayer High;
	/** Total cover seen from the ground 0..1. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Clouds") float CloudCover = 0.f;
	/** 0..1 lens clouds (Mid.Type is AltocumulusLenticular when they are the middle layer). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Clouds") float Lenticular = 0.f;
	/** 0..1 share of the sky in shower/Cb cells (patchy rain over the map). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Clouds") float ShowerCells = 0.f;

	// falling water
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Precipitation") ELyudPrecipType Precip = ELyudPrecipType::None;
	/** mm/h of water (drizzle 0.2, rain 2, a downpour 10+). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Precipitation") float PrecipRate = 0.f;
	/** 0 rain .. 1 snow. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Precipitation") float SnowFraction = 0.f;
	/** Fresh snow on level ground, cm per hour. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Precipitation") float SnowfallCmPerHour = 0.f;
	/** 0..1 (lightning ~2 x this per minute). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Precipitation") float Thunder = 0.f;
	/** 0..1 hail / graupel share and size. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Precipitation") float Hail = 0.f;

	// fog
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Fog") ELyudFogType FogType = ELyudFogType::None;
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Fog") float Fog = 0.f;
	/** Horizontal visibility, m (fog, rain and snow together). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Fog") float VisibilityM = 30000.f;
	/** How deep the fog is, m. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Fog") float FogTopM = 0.f;

	// what the past leaves on the ground and the sea
	/** 0 dry .. 1 soaked ground, puddles. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Ground") float Wet = 0.f;
	/** Undisturbed level snow, cm (drifts, paths and the shore strip are the winter system's). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Ground") float SnowDepthCm = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Ground") float SnowWaterMm = 0.f;
	/** kg/m3: ~60 fresh fluff .. 400 old wet. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Ground") float SnowDensity = 100.f;
	/** Days since fresh snow (how white it is). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Ground") float SnowAgeDays = 0.f;
	/** 0..1 melting now (slush, drips). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Ground") float Melt = 0.f;
	/** 0..1 hoarfrost / rime now. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Ground") float Frost = 0.f;
	/** 0..1 glaze ice from freezing rain. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Ground") float Glaze = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Ground") float GroundFrostCm = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Sea") float SeaTempC = 10.f;
	/** This winter's net freezing degree-days. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Sea") float FreezingDegreeDays = 0.f;
	/** Level sea ice thickness, cm (0 = open water). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Sea") float SeaIceCm = 0.f;
	/** 0..1 of the open sea frozen over. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Sea") float SeaIceCover = 0.f;

	// the sky beyond the weather (chances 0..1; whoever draws them checks darkness, the sun and the cloud)
	/** Planetary geomagnetic index 0..9. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Sky") float Kp = 1.f;
	/** 0..1 how bright the aurora would be here if dark and clear (Kp 5.3 faint on the north horizon .. 8.5 overhead). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Sky") float Aurora = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Sky") float Halo = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Sky") float Rainbow = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Sky") float Noctilucent = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Sky") float Waterspout = 0.f;
};

/** The sky over the outdoor map at one moment: the old short summary (kept for the sound and people). */
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

/** The climate the engine plays (LyudWeather::FClimate). */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudClimateSettings
{
	GENERATED_BODY()

	/** Picks the sequence of weather (every seed a different history; the default gives the scene's start its WSW breeze and broken cloud). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") int32 Seed = 750;
	/** How much more it rains and snows than the coast's climate (1 = ~800 mm a year). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") float Wetness = 1.f;
	/** How hard the winters are (cold highs' frost and how often they come; 1 = sea ice in most winters). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") float WinterSeverity = 1.f;
	/** Yearly mean temperature and half swing, C. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") float MeanTempC = 8.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") float YearSwingC = 9.8f;
	/** How active the sun is (aurora storms; 1 = solar cycle 25 as it is). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") float SolarActivity = 1.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLyudLightning, FVector, Location, float, DistanceFromViewM);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLyudRegimeChanged, ELyudWeatherRegime, From, ELyudWeatherRegime, To);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLyudPrecipChanged, ELyudPrecipType, From, ELyudPrecipType, To);

/**
 * The weather engine (docs/weather.md): seasons and the Baltic coast's climate played out as a chain of synoptic
 * episodes (highs, warm and cold fronts, showery polar air, occlusions, winter arctic highs, summer thundery lows),
 * with air masses, three cloud layers, the kind of precipitation, fog by its cause, the snow pack, frozen ground,
 * the sea's temperature and ice, and the geomagnetic Kp for aurora. The model is plain C++ in
 * Private/Weather/WeatherModel.cpp (tools/weather/run.sh runs and checks it outside the engine). It is a function of
 * the game calendar (UGameCalendar + lyud.SkyHourOffset, the same clock as the sky and the wind), so saves, jumps of
 * the clock and forecasts all agree; the snow and ice integrate hour by hour from the 1 September before.
 * The wind (ULyudWindSubsystem) takes its weather from the same chain, so fronts veer and freshen it.
 *
 * Everything that draws weather reads GetState() (or GetModelState() in C++) and keeps its own visuals; this
 * subsystem sets lyud.Overcast for people's talk and fires OnLightning, OnRegimeChanged and OnPrecipChanged.
 *
 * Console:
 *   lyud.Weather.Debug 1|2         on-screen summary (2: all fields)
 *   lyud.Weather.Preset <name>     clear, fair, overcast, drizzle, rain, downpour, storm, thunder, fog, seafog,
 *                                  snow, blizzard, freezingrain, frost, lenticular, aurora, halo, rainbow, off
 *   lyud.Weather.Regime <name>     plays one episode type now (High, WarmFront, ... ; off)
 *   lyud.Weather.Rain / Cloud / Fog / Temp / Thunder / Snow / Kp / SnowDepth / SeaIce   force one value (negative = off)
 *   lyud.Weather.Forecast [days]   prints the coming days' weather to the log
 *   lyud.Weather.Strike [m]        lightning now
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") FLyudClimateSettings Climate;

	/** The weather now (forced values applied). */
	UFUNCTION(BlueprintCallable, Category = "Weather") FLyudWeatherState GetState() const;
	/** The same for C++, without the copy into Unreal types. */
	const LyudWeather::FWeatherState& GetModelState() const;
	/** The weather at another moment (forecasts, people's talk, the diary); no forcing. */
	UFUNCTION(BlueprintCallable, Category = "Weather") FLyudWeatherState GetStateAt(const FDateTime& When) const;
	/** The short summary (cloud, rain, snow, thunder, fog, temperature, wet). */
	UFUNCTION(BlueprintCallable, Category = "Weather") FLyudSkyWeather GetSky() const;
	/** Local time the weather is for (calendar + lyud.SkyHourOffset). */
	UFUNCTION(BlueprintCallable, Category = "Weather") FDateTime WeatherTime() const;

	/** Lightning: a strike somewhere (world cm) and how far it is from the camera, m. */
	UPROPERTY(BlueprintAssignable, Category = "Weather") FLyudLightning OnLightning;
	/** A new synoptic episode began. */
	UPROPERTY(BlueprintAssignable, Category = "Weather") FLyudRegimeChanged OnRegimeChanged;
	/** The kind of precipitation changed (began, stopped, rain to snow...). */
	UPROPERTY(BlueprintAssignable, Category = "Weather") FLyudPrecipChanged OnPrecipChanged;
	/** A strike DistanceM from the camera now, in a random direction (tests, the console). */
	UFUNCTION(BlueprintCallable, Category = "Weather") void Strike(float DistanceM);

	/** Text for logs and tests (one line; bFull: every field). */
	UFUNCTION(BlueprintCallable, Category = "Weather") FString DebugReport(bool bFull = false) const;
	/** The coming days, one line per 3 hours. */
	UFUNCTION(BlueprintCallable, Category = "Weather") FString Forecast(float Days = 3.f) const;

	/** Forces a preset (see the console list); "" or "off" clears. */
	UFUNCTION(BlueprintCallable, Category = "Weather") bool SetPreset(const FString& Name);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	ULyudWeatherSubsystem();
	virtual ~ULyudWeatherSubsystem() override;

private:
	double NowHours() const;
	void PushClimate() const;
	/** The model's state at Hours with the forcing applied (cached). */
	const LyudWeather::FWeatherState& StateAt(double Hours) const;

	// plain C++ state owned here (raw: the generated code can't see their definitions)
	mutable LyudWeather::FChain* Chain = nullptr;
	LyudWeather::FSeason* Season = nullptr;
	LyudWeather::FWeatherState* Cached = nullptr;
	mutable double CachedHours = -1e9;
	mutable uint32 CachedForceHash = 0;
	mutable uint32 ClimateHash = 0;
	float NextStrike = 0.f;
	int32 LastOvercast = -1;
	uint8 LastRegime = 255, LastPrecip = 255;
	bool CameraLocation(FVector& Out) const;
};
