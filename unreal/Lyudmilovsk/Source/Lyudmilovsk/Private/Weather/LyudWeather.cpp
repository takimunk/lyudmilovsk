#include "Weather/LyudWeather.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Plants/GameCalendar.h"
#include "RainModel.h"
#include "WeatherModel.h"
#include "WindModel.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudWeather, Log, All);

namespace
{
	TAutoConsoleVariable<float> CVarRain(TEXT("lyud.Weather.Rain"), -1.f, TEXT("Force the precipitation, mm/h of water (negative: the calendar's)."));
	TAutoConsoleVariable<float> CVarCloud(TEXT("lyud.Weather.Cloud"), -1.f, TEXT("Force the cloud cover 0..1 (negative: the calendar's)."));
	TAutoConsoleVariable<float> CVarFog(TEXT("lyud.Weather.Fog"), -1.f, TEXT("Force the fog 0..1 (negative: the calendar's)."));
	TAutoConsoleVariable<float> CVarTemp(TEXT("lyud.Weather.Temp"), -100.f, TEXT("Force the air temperature, C (below -99: the calendar's)."));
	TAutoConsoleVariable<float> CVarThunder(TEXT("lyud.Weather.Thunder"), -1.f, TEXT("Force how stormy it is 0..1 (lightning ~2 x this per minute; negative: the calendar's)."));
	TAutoConsoleVariable<float> CVarSnow(TEXT("lyud.Weather.Snow"), -1.f, TEXT("Force the snow share of the precipitation 0..1 (negative: from the temperature)."));
	TAutoConsoleVariable<float> CVarKp(TEXT("lyud.Weather.Kp"), -1.f, TEXT("Force the geomagnetic Kp 0..9 (aurora from ~5.3; negative: the calendar's)."));
	TAutoConsoleVariable<float> CVarSnowDepth(TEXT("lyud.Weather.SnowDepth"), -1.f, TEXT("Force the level snow depth, cm (negative: the season's)."));
	TAutoConsoleVariable<float> CVarSeaIce(TEXT("lyud.Weather.SeaIce"), -1.f, TEXT("Force the sea ice thickness, cm (negative: the season's)."));
	TAutoConsoleVariable<float> CVarLens(TEXT("lyud.Weather.Lenticular"), -1.f, TEXT("Force the lenticular clouds 0..1 (negative: the calendar's)."));
	TAutoConsoleVariable<int32> CVarDebug(TEXT("lyud.Weather.Debug"), 0, TEXT("1: print the weather on screen; 2: every field."));

	/** The forced episode type (Count = none) and the preset's name. */
	LyudWeather::ERegime ForcedRegime = LyudWeather::ERegime::Count;
	double ForcedSince = 0.0;
	FString PresetName;

	void SetCVar(const TCHAR* Name, float V)
	{
		if (IConsoleVariable* C = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			C->Set(V, ECVF_SetByConsole);
		}
	}

	void ClearForces()
	{
		SetCVar(TEXT("lyud.Weather.Rain"), -1.f);
		SetCVar(TEXT("lyud.Weather.Cloud"), -1.f);
		SetCVar(TEXT("lyud.Weather.Fog"), -1.f);
		SetCVar(TEXT("lyud.Weather.Temp"), -100.f);
		SetCVar(TEXT("lyud.Weather.Thunder"), -1.f);
		SetCVar(TEXT("lyud.Weather.Snow"), -1.f);
		SetCVar(TEXT("lyud.Weather.Kp"), -1.f);
		SetCVar(TEXT("lyud.Weather.SnowDepth"), -1.f);
		SetCVar(TEXT("lyud.Weather.SeaIce"), -1.f);
		SetCVar(TEXT("lyud.Weather.Lenticular"), -1.f);
		if (!PresetName.IsEmpty())
		{
			// presets may set the wind too
			SetCVar(TEXT("lyud.Wind.Speed"), -1.f);
			SetCVar(TEXT("lyud.Wind.From"), -1.f);
		}
		ForcedRegime = LyudWeather::ERegime::Count;
		LyudWeather::SetHourShift(0.0);
		PresetName.Reset();
	}

	FAutoConsoleCommandWithWorldAndArgs StrikeCommand(TEXT("lyud.Weather.Strike"),
		TEXT("Lightning now: lyud.Weather.Strike [distance m, default 1500]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudWeatherSubsystem* W = ULyudWeatherSubsystem::Get(World))
			{
				W->Strike(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 1500.f);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs PresetCommand(TEXT("lyud.Weather.Preset"),
		TEXT("Force a weather: clear, fair, overcast, drizzle, rain, downpour, storm, thunder, fog, seafog, snow, blizzard, freezingrain, frost, lenticular, aurora, halo, rainbow; off"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudWeatherSubsystem* W = ULyudWeatherSubsystem::Get(World))
			{
				const FString Name = Args.Num() > 0 ? Args[0] : FString();
				if (!W->SetPreset(Name))
				{
					UE_LOG(LogLyudWeather, Warning, TEXT("lyud.Weather.Preset: unknown '%s'"), *Name);
				}
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs RegimeCommand(TEXT("lyud.Weather.Regime"),
		TEXT("Play one synoptic episode type now: High, WarmFront, WarmSector, ColdFront, Showery, Occluded, ArcticHigh, ThunderyLow; off"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudWeatherSubsystem* W = ULyudWeatherSubsystem::Get(World))
			{
				W->SetPreset(Args.Num() > 0 ? FString(TEXT("regime:")) + Args[0] : FString());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs ForecastCommand(TEXT("lyud.Weather.Forecast"),
		TEXT("Print the coming days' weather to the log: lyud.Weather.Forecast [days, default 3]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudWeatherSubsystem* W = ULyudWeatherSubsystem::Get(World))
			{
				TArray<FString> Lines;
				W->Forecast(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 3.f).ParseIntoArrayLines(Lines);
				for (const FString& L : Lines)
				{
					UE_LOG(LogLyudWeather, Warning, TEXT("%s"), *L);
				}
			}
		}));

	FLyudCloudLayer ToLayer(const LyudWeather::FCloudLayer& L)
	{
		FLyudCloudLayer O;
		O.Type = ELyudCloudType(L.Type);
		O.Cover = L.Cover;
		O.BaseM = L.BaseM;
		O.TopM = L.TopM;
		O.Density = L.Density;
		return O;
	}

	FLyudWeatherState ToUnreal(const LyudWeather::FWeatherState& S)
	{
		FLyudWeatherState O;
		O.Regime = ELyudWeatherRegime(S.Regime);
		O.NextRegime = ELyudWeatherRegime(S.NextRegime);
		O.RegimeProgress = S.RegimeProgress;
		O.HoursToNext = S.HoursToNext;
		O.Intensity = S.Intensity;
		O.PressureHPa = S.PressureHPa;
		O.PressureTendency = S.PressureTendency;
		O.TempC = S.TempC;
		O.DewPointC = S.DewPointC;
		O.Humidity = S.Humidity;
		O.FreezingLevelM = S.FreezingLevelM;
		O.Inversion = S.Inversion;
		O.Convection = S.Convection;
		O.WindFromDeg = S.WindFromDeg;
		O.WindSpeed = S.WindSpeed;
		O.WindGustiness = S.WindGustiness;
		O.UpperFromDeg = S.UpperFromDeg;
		O.UpperSpeed = S.UpperSpeed;
		O.Low = ToLayer(S.Low);
		O.Mid = ToLayer(S.Mid);
		O.High = ToLayer(S.High);
		O.CloudCover = S.CloudCover;
		O.Lenticular = S.Lenticular;
		O.ShowerCells = S.ShowerCells;
		O.Precip = ELyudPrecipType(S.Precip);
		O.PrecipRate = S.PrecipRate;
		O.SnowFraction = S.SnowFraction;
		O.SnowfallCmPerHour = S.SnowfallCmPerHour;
		O.Thunder = S.Thunder;
		O.Hail = S.Hail;
		O.FogType = ELyudFogType(S.FogType);
		O.Fog = S.Fog;
		O.VisibilityM = S.VisibilityM;
		O.FogTopM = S.FogTopM;
		O.Wet = S.Wet;
		O.SnowDepthCm = S.SnowDepthCm;
		O.SnowWaterMm = S.SnowWaterMm;
		O.SnowDensity = S.SnowDensity;
		O.SnowAgeDays = S.SnowAgeDays;
		O.Melt = S.Melt;
		O.Frost = S.Frost;
		O.Glaze = S.Glaze;
		O.GroundFrostCm = S.GroundFrostCm;
		O.SeaTempC = S.SeaTempC;
		O.FreezingDegreeDays = S.FreezingDegreeDays;
		O.SeaIceCm = S.SeaIceCm;
		O.SeaIceCover = S.SeaIceCover;
		O.Kp = S.Kp;
		O.Aurora = S.Aurora;
		O.Halo = S.Halo;
		O.Rainbow = S.Rainbow;
		O.Noctilucent = S.Noctilucent;
		O.Waterspout = S.Waterspout;
		return O;
	}

	/** The forced single values on top of the model, kept consistent with each other (forced rain needs a sky). */
	void ApplyForces(LyudWeather::FWeatherState& S)
	{
		using namespace LyudWeather;
		const float Temp = CVarTemp.GetValueOnGameThread();
		const float Rain = CVarRain.GetValueOnGameThread();
		const float Cloud = CVarCloud.GetValueOnGameThread();
		const float Fog = CVarFog.GetValueOnGameThread();
		const float Thunder = CVarThunder.GetValueOnGameThread();
		const float Snow = CVarSnow.GetValueOnGameThread();
		const float Kp = CVarKp.GetValueOnGameThread();
		const float Depth = CVarSnowDepth.GetValueOnGameThread();
		const float Ice = CVarSeaIce.GetValueOnGameThread();
		const float Lens = CVarLens.GetValueOnGameThread();
		if (Temp > -99.f)
		{
			const float Spread = S.TempC - S.DewPointC;
			S.TempC = Temp;
			S.DewPointC = Temp - Spread;
			S.FreezingLevelM = FMath::Max(0.f, Temp * 150.f);
			S.SnowFraction = FMath::SmoothStep(1.6f, -0.3f, Temp - Spread / 3.f);
		}
		if (Snow >= 0.f)
		{
			S.SnowFraction = FMath::Clamp(Snow, 0.f, 1.f);
		}
		if (Cloud >= 0.f)
		{
			const float C = FMath::Clamp(Cloud, 0.f, 1.f);
			const float K = S.CloudCover > 0.01f ? C / S.CloudCover : 0.f;
			S.CloudCover = C;
			if (S.Low.Cover + S.Mid.Cover + S.High.Cover < 0.02f && C > 0.f)
			{
				S.Low = { C > 0.8f ? ECloud::Stratocumulus : ECloud::Cumulus, C, 700.f, 1500.f, 0.5f };
			}
			else
			{
				S.Low.Cover = FMath::Min(S.Low.Cover * K, 1.f);
				S.Mid.Cover = FMath::Min(S.Mid.Cover * K, 1.f);
				S.High.Cover = FMath::Min(S.High.Cover * K, 1.f);
			}
		}
		if (Rain >= 0.f)
		{
			S.PrecipRate = Rain;
			if (Rain > 0.f && Cloud < 0.f)
			{
				S.CloudCover = FMath::Max(S.CloudCover, 0.95f);
				if (S.Low.Cover < 0.9f)
				{
					S.Low = { Rain > 6.f ? ECloud::Cumulonimbus : ECloud::Nimbostratus, 0.95f, 500.f, 4000.f, 0.9f };
				}
			}
		}
		if (Thunder >= 0.f)
		{
			S.Thunder = Thunder;
		}
		if (Rain >= 0.f || Temp > -99.f || Snow >= 0.f || Thunder >= 0.f)
		{
			const float R = S.PrecipRate;
			S.SnowfallCmPerHour = R * S.SnowFraction * (1.f + FMath::SmoothStep(0.f, -10.f, S.TempC));
			if (R < 0.03f) S.Precip = EPrecip::None;
			else if (S.SnowFraction > 0.75f) S.Precip = EPrecip::Snow;
			else if (S.SnowFraction > 0.25f) S.Precip = EPrecip::Sleet;
			else if (S.Thunder > 0.3f) S.Precip = EPrecip::Thunderstorm;
			else if (R < 0.4f) S.Precip = EPrecip::Drizzle;
			else S.Precip = EPrecip::Rain;
			if (Rain >= 0.f)
			{
				S.Wet = FMath::Max(S.Wet, FMath::Clamp(Rain * (1.f - S.SnowFraction) / 2.f, 0.f, 1.f));
			}
		}
		if (Fog >= 0.f)
		{
			S.Fog = FMath::Clamp(Fog, 0.f, 1.f);
			S.FogType = S.Fog < 0.03f ? EFog::None : (S.FogType == EFog::None ? EFog::Radiation : S.FogType);
			S.FogTopM = FMath::Max(S.FogTopM, 60.f);
		}
		if (Fog >= 0.f || Rain >= 0.f)
		{
			const float R = S.PrecipRate;
			const float KFog = 3.912f / (30000.f * FMath::Pow(10.f, -2.57f * S.Fog));
			const float KRain = 3.912f / (12000.f / FMath::Pow(FMath::Max(R * (1.f - S.SnowFraction), 0.01f), 0.6f));
			const float KSnow = 3.912f / (1500.f / FMath::Pow(FMath::Max(R * S.SnowFraction, 0.01f), 0.8f));
			S.VisibilityM = FMath::Clamp(3.912f / (KFog + KRain + KSnow), 30.f, 50000.f);
		}
		if (Kp >= 0.f)
		{
			S.Kp = Kp;
			S.Aurora = FMath::SmoothStep(5.3f, 8.5f, Kp);
		}
		if (Depth >= 0.f)
		{
			S.SnowDepthCm = Depth;
			S.SnowWaterMm = Depth * 2.5f;
			S.SnowDensity = 250.f;
		}
		if (Ice >= 0.f)
		{
			S.SeaIceCm = Ice;
			S.SeaIceCover = FMath::SmoothStep(0.5f, 6.f, Ice);
			if (S.SeaIceCover > 0.02f) S.SeaTempC = FMath::Min(S.SeaTempC, -0.3f);
		}
		if (Lens >= 0.f)
		{
			S.Lenticular = Lens;
			if (Lens > 0.25f)
			{
				S.Mid = { ECloud::AltocumulusLenticular, FMath::Max(S.Mid.Cover, 0.3f * Lens), 3000.f, 3600.f, 0.5f };
			}
		}
	}

	uint32 ForceHash()
	{
		const float V[] = { CVarRain.GetValueOnGameThread(), CVarCloud.GetValueOnGameThread(), CVarFog.GetValueOnGameThread(),
			CVarTemp.GetValueOnGameThread(), CVarThunder.GetValueOnGameThread(), CVarSnow.GetValueOnGameThread(),
			CVarKp.GetValueOnGameThread(), CVarSnowDepth.GetValueOnGameThread(), CVarSeaIce.GetValueOnGameThread(),
			CVarLens.GetValueOnGameThread(), float(LyudWeather::GetHourShift()) };
		return FCrc::MemCrc32(V, sizeof(V));
	}

	FString Describe(const LyudWeather::FWeatherState& S, bool bFull)
	{
		using namespace LyudWeather;
		FString Out = FString::Printf(
			TEXT("WEATHER %s->%s %.0f%% (in %.1f h) %.0f hPa %+.1f | %.1f C dew %.1f RH %.0f%% | wind %.0f deg %.1f m/s, aloft %.0f/%.0f | cloud %.2f: low %s %.2f@%.0f mid %s %.2f high %s %.2f | %s %.2f mm/h snow %.2f thunder %.2f | fog %s %.2f vis %.0f m | wet %.2f snow %.1f cm ice %.1f cm sea %.1f C | Kp %.1f aurora %.2f lens %.2f"),
			UTF8_TO_TCHAR(RegimeName(S.Regime)), UTF8_TO_TCHAR(RegimeName(S.NextRegime)), S.RegimeProgress * 100.f, S.HoursToNext,
			S.PressureHPa, S.PressureTendency, S.TempC, S.DewPointC, S.Humidity * 100.f, S.WindFromDeg, S.WindSpeed, S.UpperFromDeg, S.UpperSpeed,
			S.CloudCover, UTF8_TO_TCHAR(CloudName(S.Low.Type)), S.Low.Cover, S.Low.BaseM, UTF8_TO_TCHAR(CloudName(S.Mid.Type)), S.Mid.Cover,
			UTF8_TO_TCHAR(CloudName(S.High.Type)), S.High.Cover, UTF8_TO_TCHAR(PrecipName(S.Precip)), S.PrecipRate, S.SnowFraction, S.Thunder,
			UTF8_TO_TCHAR(FogName(S.FogType)), S.Fog, S.VisibilityM, S.Wet, S.SnowDepthCm, S.SeaIceCm, S.SeaTempC, S.Kp, S.Aurora, S.Lenticular);
		if (bFull)
		{
			Out += FString::Printf(TEXT("\n  intensity %.2f freezing level %.0f m inversion %.2f convection %.2f gust+ %.2f showercells %.2f hail %.2f snowfall %.2f cm/h fogtop %.0f m"),
				S.Intensity, S.FreezingLevelM, S.Inversion, S.Convection, S.WindGustiness, S.ShowerCells, S.Hail, S.SnowfallCmPerHour, S.FogTopM);
			Out += FString::Printf(TEXT("\n  layers low %.0f-%.0f m d%.2f, mid %.0f-%.0f d%.2f, high %.0f d%.2f | snow water %.0f mm density %.0f age %.1f d melt %.2f frost %.2f glaze %.2f soil frost %.0f cm FDD %.0f ice cover %.2f"),
				S.Low.BaseM, S.Low.TopM, S.Low.Density, S.Mid.BaseM, S.Mid.TopM, S.Mid.Density, S.High.BaseM, S.High.Density,
				S.SnowWaterMm, S.SnowDensity, S.SnowAgeDays, S.Melt, S.Frost, S.Glaze, S.GroundFrostCm, S.FreezingDegreeDays, S.SeaIceCover);
			Out += FString::Printf(TEXT("\n  halo %.2f rainbow %.2f noctilucent %.2f waterspout %.2f"), S.Halo, S.Rainbow, S.Noctilucent, S.Waterspout);
			if (!PresetName.IsEmpty())
			{
				Out += TEXT("\n  forced: ") + PresetName;
			}
		}
		return Out;
	}
}

ULyudWeatherSubsystem::ULyudWeatherSubsystem() = default;
ULyudWeatherSubsystem::~ULyudWeatherSubsystem()
{
	delete Chain;
	delete Season;
	delete Cached;
}

ULyudWeatherSubsystem* ULyudWeatherSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudWeatherSubsystem>() : nullptr;
}

bool ULyudWeatherSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

TStatId ULyudWeatherSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudWeatherSubsystem, STATGROUP_Tickables);
}

void ULyudWeatherSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!Cached) Cached = new LyudWeather::FWeatherState();
	if (!Season) Season = new LyudWeather::FSeason();
	PushClimate();
}

void ULyudWeatherSubsystem::Deinitialize()
{
	ClearForces();
	Super::Deinitialize();
}

void ULyudWeatherSubsystem::PushClimate() const
{
	LyudWeather::FClimate C;
	C.Seed = uint32(Climate.Seed);
	C.Wetness = Climate.Wetness;
	C.WinterSeverity = Climate.WinterSeverity;
	C.MeanTempC = Climate.MeanTempC;
	C.YearSwingC = Climate.YearSwingC;
	C.SolarActivity = Climate.SolarActivity;
	const uint32 H = FCrc::MemCrc32(&Climate, sizeof(Climate));
	if (H != ClimateHash || !Chain)
	{
		ClimateHash = H;
		LyudWeather::SetClimate(C);
		delete Chain;
		Chain = new LyudWeather::FChain(C);
		*Season = LyudWeather::FSeason();
		CachedHours = -1e9;
	}
}

FDateTime ULyudWeatherSubsystem::WeatherTime() const
{
	static IConsoleVariable* SkyOffset = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.SkyHourOffset"));
	const float Offset = SkyOffset ? SkyOffset->GetFloat() : 0.f;
	return UGameCalendar::NowIn(GetWorld()) + FTimespan::FromHours(Offset);
}

double ULyudWeatherSubsystem::NowHours() const
{
	const FDateTime T = WeatherTime();
	return LyudWeather::HoursSince2000(T.GetYear(), T.GetMonth(), T.GetDay(), T.GetTimeOfDay().GetTotalHours());
}

const LyudWeather::FWeatherState& ULyudWeatherSubsystem::StateAt(double Hours) const
{
	PushClimate();
	const uint32 F = ForceHash();
	// the weather changes over minutes: recompute twice a game minute, or at once when a value is forced
	if (FMath::Abs(Hours - CachedHours) > 1.0 / 120.0 || F != CachedForceHash)
	{
		CachedHours = Hours;
		CachedForceHash = F;
		*Cached = LyudWeather::Full(Hours, *Chain, *Season);
		ApplyForces(*Cached);
	}
	return *Cached;
}

const LyudWeather::FWeatherState& ULyudWeatherSubsystem::GetModelState() const
{
	return StateAt(NowHours());
}

FLyudWeatherState ULyudWeatherSubsystem::GetState() const
{
	return ToUnreal(GetModelState());
}

FLyudWeatherState ULyudWeatherSubsystem::GetStateAt(const FDateTime& When) const
{
	PushClimate();
	const double Hours = LyudWeather::HoursSince2000(When.GetYear(), When.GetMonth(), When.GetDay(), When.GetTimeOfDay().GetTotalHours());
	// a separate season so the game's one keeps stepping forward
	LyudWeather::FSeason Other;
	return ToUnreal(LyudWeather::Full(Hours, *Chain, Other));
}

FLyudSkyWeather ULyudWeatherSubsystem::GetSky() const
{
	const LyudWeather::FWeatherState& S = GetModelState();
	FLyudSkyWeather O;
	O.Cloud = S.CloudCover;
	O.Rain = S.PrecipRate;
	O.Snow = S.SnowFraction;
	O.Thunder = S.Thunder;
	O.Fog = S.Fog;
	O.TempC = S.TempC;
	O.Wet = S.Wet;
	return O;
}

bool ULyudWeatherSubsystem::SetPreset(const FString& InName)
{
	using namespace LyudWeather;
	const FString Name = InName.TrimStartAndEnd().ToLower();
	ClearForces();
	if (Name.IsEmpty() || Name == TEXT("off") || Name == TEXT("none"))
	{
		CachedHours = -1e9;
		return true;
	}
	// a preset = an episode type found in the chain near this time of year (played at this hour of the day) and/or
	// a few forced values
	ERegime Regime = ERegime::Count;
	TFunction<void()> Values;
	auto Set = [](const TCHAR* N, float V) { SetCVar(N, V); };
	if (Name.StartsWith(TEXT("regime:")))
	{
		Regime = ParseRegime(TCHAR_TO_UTF8(*Name.Mid(7)));
		if (Regime == ERegime::Count) return false;
	}
	else if (Name == TEXT("clear")) { Regime = ERegime::High; Values = [Set] { Set(TEXT("lyud.Weather.Cloud"), 0.f); Set(TEXT("lyud.Weather.Rain"), 0.f); Set(TEXT("lyud.Weather.Fog"), 0.f); }; }
	else if (Name == TEXT("fair")) { Regime = ERegime::Showery; Values = [Set] { Set(TEXT("lyud.Weather.Rain"), 0.f); Set(TEXT("lyud.Weather.Cloud"), 0.35f); }; }
	else if (Name == TEXT("overcast")) { Regime = ERegime::WarmSector; Values = [Set] { Set(TEXT("lyud.Weather.Rain"), 0.f); Set(TEXT("lyud.Weather.Cloud"), 1.f); }; }
	else if (Name == TEXT("drizzle")) { Regime = ERegime::WarmSector; Values = [Set] { Set(TEXT("lyud.Weather.Rain"), 0.2f); }; }
	else if (Name == TEXT("rain")) { Regime = ERegime::WarmFront; Values = [Set] { Set(TEXT("lyud.Weather.Rain"), 2.5f); }; }
	else if (Name == TEXT("downpour")) { Regime = ERegime::ColdFront; Values = [Set] { Set(TEXT("lyud.Weather.Rain"), 14.f); }; }
	else if (Name == TEXT("storm")) { Regime = ERegime::ColdFront; Values = [Set] { Set(TEXT("lyud.Weather.Rain"), 8.f); Set(TEXT("lyud.Wind.Speed"), 17.f); }; }
	else if (Name == TEXT("thunder")) { Regime = ERegime::ThunderyLow; Values = [Set] { Set(TEXT("lyud.Weather.Rain"), 12.f); Set(TEXT("lyud.Weather.Thunder"), 0.8f); Set(TEXT("lyud.Weather.Temp"), 22.f); }; }
	else if (Name == TEXT("fog")) { Regime = ERegime::High; Values = [Set] { Set(TEXT("lyud.Weather.Fog"), 0.9f); Set(TEXT("lyud.Weather.Rain"), 0.f); Set(TEXT("lyud.Wind.Speed"), 0.8f); }; }
	else if (Name == TEXT("seafog")) { Regime = ERegime::WarmSector; Values = [Set] { Set(TEXT("lyud.Weather.Fog"), 0.85f); Set(TEXT("lyud.Weather.Rain"), 0.f); Set(TEXT("lyud.Wind.Speed"), 4.f); Set(TEXT("lyud.Wind.From"), 270.f); }; }
	else if (Name == TEXT("snow")) { Regime = ERegime::WarmFront; Values = [Set] { Set(TEXT("lyud.Weather.Temp"), -3.f); Set(TEXT("lyud.Weather.Rain"), 1.2f); Set(TEXT("lyud.Weather.Snow"), 1.f); Set(TEXT("lyud.Weather.SnowDepth"), 25.f); }; }
	else if (Name == TEXT("blizzard")) { Regime = ERegime::ColdFront; Values = [Set] { Set(TEXT("lyud.Weather.Temp"), -8.f); Set(TEXT("lyud.Weather.Rain"), 3.f); Set(TEXT("lyud.Weather.Snow"), 1.f); Set(TEXT("lyud.Wind.Speed"), 16.f); Set(TEXT("lyud.Weather.SnowDepth"), 60.f); }; }
	else if (Name == TEXT("freezingrain")) { Regime = ERegime::WarmFront; Values = [Set] { Set(TEXT("lyud.Weather.Temp"), -2.f); Set(TEXT("lyud.Weather.Rain"), 1.5f); Set(TEXT("lyud.Weather.Snow"), 0.f); }; }
	else if (Name == TEXT("frost")) { Regime = ERegime::ArcticHigh; Values = [Set] { Set(TEXT("lyud.Weather.Temp"), -15.f); Set(TEXT("lyud.Weather.Cloud"), 0.f); Set(TEXT("lyud.Weather.Rain"), 0.f); Set(TEXT("lyud.Weather.SnowDepth"), 30.f); Set(TEXT("lyud.Weather.SeaIce"), 25.f); }; }
	else if (Name == TEXT("lenticular")) { Regime = ERegime::High; Values = [Set] { Set(TEXT("lyud.Weather.Lenticular"), 1.f); Set(TEXT("lyud.Weather.Rain"), 0.f); }; }
	else if (Name == TEXT("aurora")) { Regime = ERegime::ArcticHigh; Values = [Set] { Set(TEXT("lyud.Weather.Kp"), 8.f); Set(TEXT("lyud.Weather.Cloud"), 0.f); Set(TEXT("lyud.Weather.Rain"), 0.f); }; }
	else if (Name == TEXT("halo")) { Regime = ERegime::WarmFront; Values = [Set] { Set(TEXT("lyud.Weather.Rain"), 0.f); }; }
	else if (Name == TEXT("rainbow")) { Regime = ERegime::Showery; }
	else return false;

	if (Regime != ERegime::Count)
	{
		// the nearest episode of that type within six weeks of this date in any year: its middle, at this hour of the day
		const double Now = NowHours();
		const double DoyNow = DayOfYearOf(Now);
		double Best = 1e18, Pick = 0.0;
		for (int I = 0; I < 6000; I++)
		{
			const FEpisode E = Chain->Index(I);
			if (E.Start > Now + 24.0 * 365.0 * 6.0) break;
			if (E.Regime != Regime) continue;
			const double Mid = 0.5 * (E.Start + E.End);
			double Dd = FMath::Abs(DayOfYearOf(Mid) - DoyNow);
			Dd = FMath::Min(Dd, 365.25 - Dd);
			// halo wants the thin cirrostratus at the front's start; the rest the episode's middle
			const double Want = Name == TEXT("halo") ? E.Start + 0.18 * (E.End - E.Start) : Mid;
			const double Score = Dd * 24.0 + FMath::Abs(Want - Now) * 0.002;
			if (Score < Best)
			{
				Best = Score;
				// same hour of the day as the clock
				const double Hod = Now - FMath::Floor(Now / 24.0) * 24.0;
				double H = FMath::Floor(Want / 24.0) * 24.0 + Hod;
				if (H < E.Start) H += 24.0 * FMath::Max(0.0, FMath::CeilToDouble((E.Start - H) / 24.0));
				if (H > E.End) H = Want;
				Pick = H;
			}
		}
		ForcedRegime = Regime;
		ForcedSince = Now;
		SetHourShift(Pick - Now);
	}
	if (Values)
	{
		Values();
	}
	PresetName = Name;
	CachedHours = -1e9;
	UE_LOG(LogLyudWeather, Log, TEXT("weather preset %s (shift %.0f h)"), *Name, GetHourShift());
	return true;
}

bool ULyudWeatherSubsystem::CameraLocation(FVector& Out) const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (PC && PC->PlayerCameraManager)
	{
		Out = PC->PlayerCameraManager->GetCameraLocation();
		return true;
	}
	return false;
}

void ULyudWeatherSubsystem::Strike(float DistanceM)
{
	FVector View = FVector::ZeroVector;
	CameraLocation(View);
	const float Yaw = FMath::FRandRange(0.f, 2.f * PI);
	const FVector Where = View + FVector(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.f) * DistanceM * 100.f + FVector(0, 0, 150000.f);
	OnLightning.Broadcast(Where, FMath::Max(DistanceM, 0.f));
}

void ULyudWeatherSubsystem::Tick(float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	const LyudWeather::FWeatherState& S = GetModelState();
	// people's talk about the sun reads lyud.Overcast
	const int32 Overcast = S.CloudCover > 0.75f ? 1 : 0;
	if (Overcast != LastOvercast)
	{
		LastOvercast = Overcast;
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Overcast")))
		{
			V->Set(Overcast, ECVF_SetByCode);
		}
	}
	if (uint8(S.Regime) != LastRegime)
	{
		const ELyudWeatherRegime From = LastRegime == 255 ? ELyudWeatherRegime(S.Regime) : ELyudWeatherRegime(LastRegime);
		LastRegime = uint8(S.Regime);
		OnRegimeChanged.Broadcast(From, ELyudWeatherRegime(S.Regime));
	}
	if (uint8(S.Precip) != LastPrecip)
	{
		const ELyudPrecipType From = LastPrecip == 255 ? ELyudPrecipType(S.Precip) : ELyudPrecipType(LastPrecip);
		LastPrecip = uint8(S.Precip);
		OnPrecipChanged.Broadcast(From, ELyudPrecipType(S.Precip));
	}
	// lightning: a Poisson process at ~2 x Thunder strikes a minute, mostly kilometres off, now and then close
	if (S.Thunder > 0.01f)
	{
		NextStrike -= DeltaTime;
		if (NextStrike <= 0.f)
		{
			const float Mean = 60.f / (2.f * S.Thunder);
			NextStrike = -Mean * FMath::Loge(FMath::Max(FMath::FRand(), 1e-3f));
			Strike(FMath::Lerp(300.f, 6000.f, FMath::Pow(FMath::FRand(), 0.7f)));
		}
	}
	else
	{
		NextStrike = FMath::FRandRange(3.f, 20.f);
	}
	const int32 Debug = CVarDebug.GetValueOnGameThread();
	if (Debug > 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(int32(0x57EA7), 0.f, FColor::Cyan, Describe(S, Debug > 1));
	}
}

FString ULyudWeatherSubsystem::DebugReport(bool bFull) const
{
	return Describe(GetModelState(), bFull);
}

FString ULyudWeatherSubsystem::Forecast(float Days) const
{
	using namespace LyudWeather;
	PushClimate();
	const double Now = NowHours();
	FSeason Other;
	FString Out;
	for (double H = 0.0; H <= Days * 24.0 + 1e-3; H += 3.0)
	{
		const FWeatherState S = Full(Now + H, *Chain, Other);
		const double Hod = FMath::Fmod(Now + H, 24.0);
		Out += FString::Printf(TEXT("FORECAST +%3.0fh %02d:00 %-11s %5.1f C %4.0f hPa wind %3.0f %4.1f cloud %.2f %s/%s/%s %-12s %4.1f mm/h fog %.2f snow %4.1f cm Kp %.1f\n"),
			H, int(Hod), UTF8_TO_TCHAR(RegimeName(S.Regime)), S.TempC, S.PressureHPa, S.WindFromDeg, S.WindSpeed, S.CloudCover,
			UTF8_TO_TCHAR(CloudName(S.Low.Type)), UTF8_TO_TCHAR(CloudName(S.Mid.Type)), UTF8_TO_TCHAR(CloudName(S.High.Type)),
			UTF8_TO_TCHAR(PrecipName(S.Precip)), S.PrecipRate, S.Fog, S.SnowDepthCm, S.Kp);
	}
	return Out;
}
