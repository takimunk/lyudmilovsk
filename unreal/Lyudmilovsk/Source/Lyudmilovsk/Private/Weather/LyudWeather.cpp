#include "Weather/LyudWeather.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Camera/PlayerCameraManager.h"
#include "Weather/LyudWind.h"
#include "RainModel.h"
#include "WindModel.h"

namespace
{
	TAutoConsoleVariable<float> CVarRain(TEXT("lyud.Weather.Rain"), -1.f, TEXT("Force the rain, mm/h (negative: the calendar's weather)."));
	TAutoConsoleVariable<float> CVarCloud(TEXT("lyud.Weather.Cloud"), -1.f, TEXT("Force the cloud cover 0..1 (negative: the calendar's)."));
	TAutoConsoleVariable<float> CVarFog(TEXT("lyud.Weather.Fog"), -1.f, TEXT("Force the mist 0..1 (negative: the calendar's)."));
	TAutoConsoleVariable<float> CVarTemp(TEXT("lyud.Weather.Temp"), -100.f, TEXT("Force the air temperature, C (below -99: the calendar's)."));
	TAutoConsoleVariable<float> CVarThunder(TEXT("lyud.Weather.Thunder"), -1.f, TEXT("Force how stormy it is 0..1 (lightning ~2 x this per minute; negative: the calendar's)."));
	TAutoConsoleVariable<int32> CVarDebug(TEXT("lyud.Weather.Debug"), 0, TEXT("1: print the weather on screen."));

	FAutoConsoleCommandWithWorldAndArgs StrikeCommand(TEXT("lyud.Weather.Strike"),
		TEXT("Lightning now: lyud.Weather.Strike [distance m, default 1500]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudWeatherSubsystem* W = ULyudWeatherSubsystem::Get(World))
			{
				W->Strike(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 1500.f);
			}
		}));

	/** The wind's settings as the model's parameters (same as ULyudWindSubsystem's). */
	LyudWeather::FWindParams WindParams(const UWorld* World)
	{
		LyudWeather::FWindParams P;
		if (const ULyudWindSubsystem* Wind = World ? World->GetSubsystem<ULyudWindSubsystem>() : nullptr)
		{
			const FLyudWindSettings& S = Wind->Settings;
			P.PrevailingFromDeg = S.PrevailingFromDeg;
			P.DirectionSwingDeg = S.DirectionSwingDeg;
			P.MedianSpeed = S.MedianSpeed;
			P.SpeedLogSigma = S.SpeedLogSigma;
			P.MaxSpeed = S.MaxSpeed;
			P.NightFactor = S.NightFactor;
			P.GustinessDay = S.GustinessDay;
			P.GustinessNight = S.GustinessNight;
			P.SeaBreeze = S.SeaBreeze;
			P.SeaBearingDeg = S.SeaBearingDeg;
			P.Seed = uint32(S.Seed);
		}
		return P;
	}
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

FLyudSkyWeather ULyudWeatherSubsystem::GetSky() const
{
	const UWorld* World = GetWorld();
	const ULyudWindSubsystem* Wind = World ? World->GetSubsystem<ULyudWindSubsystem>() : nullptr;
	FDateTime T = Wind ? Wind->WindTime() : FDateTime(2026, 10, 20, 16, 0, 0);
	const double Hours = LyudWeather::HoursSince2000(T.GetYear(), T.GetMonth(), T.GetDay(), T.GetTimeOfDay().GetTotalHours());
	const float Force[4] = { CVarRain.GetValueOnGameThread(), CVarCloud.GetValueOnGameThread(), CVarFog.GetValueOnGameThread(), CVarTemp.GetValueOnGameThread() };
	const bool bForceChanged = FMemory::Memcmp(Force, LastForce, sizeof(Force)) != 0;
	// the sky changes over minutes: recompute twice a game minute (the ground's wetness looks back 8 hours)
	if (FMath::Abs(Hours - CachedHours) > 1.0 / 120.0 || bForceChanged)
	{
		FMemory::Memcpy(LastForce, Force, sizeof(Force));
		CachedHours = Hours;
		LyudWeather::FSkyParams P;
		P.Seed = uint32(Seed);
		P.Wetness = Wetness;
		const LyudWeather::FSkyState S = LyudWeather::Sky(Hours, WindParams(World), P);
		FLyudSkyWeather& C = Cached;
		C.Cloud = Force[1] >= 0.f ? Force[1] : S.Cloud;
		C.Rain = Force[0] >= 0.f ? Force[0] : S.Rain;
		C.Fog = Force[2] >= 0.f ? Force[2] : S.Fog;
		C.TempC = Force[3] > -99.f ? Force[3] : S.TempC;
		C.Snow = Force[3] > -99.f ? FMath::SmoothStep(2.f, -0.5f, C.TempC) : S.Snow;
		C.Thunder = S.Thunder;
		C.Wet = Force[0] >= 0.f ? FMath::Clamp(Force[0] / 2.f, 0.f, 1.f) : S.Wet;
		// forced rain needs a sky to fall from
		if (Force[0] > 0.f && Force[1] < 0.f)
		{
			C.Cloud = FMath::Max(C.Cloud, 0.9f);
		}
	}
	FLyudSkyWeather Out = Cached;
	const float ForceThunder = CVarThunder.GetValueOnGameThread();
	if (ForceThunder >= 0.f)
	{
		Out.Thunder = ForceThunder;
	}
	return Out;
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
	const FLyudSkyWeather S = GetSky();
	// people's talk about the sun reads lyud.Overcast
	const int32 Overcast = S.Cloud > 0.75f ? 1 : 0;
	if (Overcast != LastOvercast)
	{
		LastOvercast = Overcast;
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Overcast")))
		{
			V->Set(Overcast, ECVF_SetByCode);
		}
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
	if (CVarDebug.GetValueOnGameThread() > 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(int32(0x57EA7), 0.f, FColor::Cyan, DebugReport());
	}
}

FString ULyudWeatherSubsystem::DebugReport() const
{
	const FLyudSkyWeather S = GetSky();
	return FString::Printf(TEXT("WEATHER cloud %.2f rain %.2f mm/h snow %.2f thunder %.2f fog %.2f temp %.1f C wet %.2f"),
		S.Cloud, S.Rain, S.Snow, S.Thunder, S.Fog, S.TempC, S.Wet);
}
