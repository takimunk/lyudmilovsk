#include "Weather/LyudGrade.h"

#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "RainModel.h"
#include "Sky/LyudSky.h"
#include "Weather/LyudWeather.h"
#include "WeatherModel.h"
#include "WindModel.h"

namespace
{
	TAutoConsoleVariable<int32> CVarGrade(TEXT("lyud.Grade"), 1, TEXT("0: no seasonal/weather colour grade outdoors (A/B)."));
	TAutoConsoleVariable<float> CVarStrength(TEXT("lyud.Grade.Strength"), 1.f, TEXT("Scales the seasonal/weather colour grade (1 = as designed; it is meant to go unnoticed)."));
	TAutoConsoleVariable<int32> CVarDebug(TEXT("lyud.Grade.Debug"), 0, TEXT("1: print the colour grade's inputs on screen."));

	float Smooth(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	FVector4 Tint(float R, float G, float B) { return FVector4(R, G, B, 1.f); }
}

bool ULyudGradeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId ULyudGradeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudGradeSubsystem, STATGROUP_Tickables);
}

void ULyudGradeSubsystem::Deinitialize()
{
	if (Sky.IsValid() && Sky->Atmosphere && BaseMie >= 0.f)
	{
		Sky->Atmosphere->SetMieScatteringScale(BaseMie);
	}
	Super::Deinitialize();
}

void ULyudGradeSubsystem::EnsureActor()
{
	UWorld* World = GetWorld();
	const double Now = FPlatformTime::Seconds();
	if (!Sky.IsValid() && Now >= NextLook)
	{
		NextLook = Now + 2.0;
		if (TActorIterator<ALyudSky> It(World); It)
		{
			Sky = *It;
		}
	}
	if (!Sky.IsValid() || (GradeActor && Grade))
	{
		return;
	}
	FActorSpawnParameters P;
	P.ObjectFlags |= RF_Transient;
	P.Name = MakeUniqueObjectName(World, AActor::StaticClass(), TEXT("LyudGrade"));
	GradeActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, P);
	if (!GradeActor)
	{
		return;
	}
	Grade = NewObject<UPostProcessComponent>(GradeActor, TEXT("Grade"));
	Grade->bUnbound = true;
	Grade->Priority = 1001.f;
	Grade->BlendWeight = 0.f;
	GradeActor->SetRootComponent(Grade);
	Grade->RegisterComponent();
	FPostProcessSettings& S = Grade->Settings;
	S.bOverride_ColorSaturationShadows = S.bOverride_ColorGainShadows = S.bOverride_ColorOffsetShadows = true;
	S.bOverride_ColorSaturationMidtones = S.bOverride_ColorGainMidtones = S.bOverride_ColorContrastMidtones = S.bOverride_ColorGammaMidtones = true;
	S.bOverride_ColorSaturationHighlights = S.bOverride_ColorGainHighlights = true;
	S.bOverride_ColorContrast = true;
	if (Sky->Atmosphere)
	{
		BaseMie = Sky->Atmosphere->MieScatteringScale;
	}
}

void ULyudGradeSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	EnsureActor();
	if (!Grade || !Sky.IsValid())
	{
		return;
	}
	const ULyudWeatherSubsystem* Weather = ULyudWeatherSubsystem::Get(World);
	if (!Weather)
	{
		return;
	}
	const LyudWeather::FWeatherState& W = Weather->GetModelState();
	const LyudWeather::FClimate C = LyudWeather::GetClimate();
	const FDateTime T = Weather->WeatherTime();
	const double Hours = LyudWeather::HoursSince2000(T.GetYear(), T.GetMonth(), T.GetDay(), T.GetTimeOfDay().GetTotalHours());
	const float SunAlt = float(LyudWeather::SunElevationDeg(Hours, C));
	const double Doy = LyudWeather::DayOfYearOf(Hours);
	// 1 in mid-winter .. 0 in mid-summer, the snow on the ground adding to the winter's look
	const float Winter = FMath::Clamp(float(0.5 + 0.5 * FMath::Cos(2.0 * PI * (Doy - C.ColdestDay) / 365.25)) + 0.3f * Smooth(2.f, 20.f, W.SnowDepthCm), 0.f, 1.f);
	const float Summer = 1.f - Winter;

	// the times of day: dusk and night (the blue hour first), the day, the low golden sun
	const float Dark = 1.f - Smooth(-8.f, 4.f, SunAlt);
	const float Day = Smooth(2.f, 20.f, SunAlt);
	const float GoldenSun = Smooth(-1.f, 4.f, SunAlt) * Smooth(16.f, 7.f, SunAlt);
	const float Clear = 1.f - Smooth(0.5f, 0.95f, W.CloudCover);
	const float Rain = Smooth(0.1f, 3.f, W.PrecipRate);

	WinterNight = Winter * Dark;
	SummerDay = Summer * Day * (1.f - 0.6f * W.CloudCover);
	Golden = Summer * GoldenSun * Clear;
	Grey = FMath::Clamp(Smooth(0.6f, 1.f, W.CloudCover) * Day * 0.7f + Rain * 0.5f, 0.f, 1.f);
	Haze = W.Fog;

	const float K = CVarGrade.GetValueOnGameThread() > 0 ? FMath::Max(CVarStrength.GetValueOnGameThread(), 0.f) : 0.f;
	// all of it a few percent: the eye adapts to a tint it can't see, and that is the aim
	const float B = 0.07f * WinterNight * K;   // winter blue in shadows and middle tones
	const float A = 0.035f * SummerDay * K;    // summer air: lighter, a breath of ultramarine in the shade
	const float G = 0.03f * Golden * K;        // low summer sun: warm highlights
	const float M = 0.06f * Grey * K;          // grey and wet: muted colour
	const float F = 0.06f * Haze * K;          // fog: flatter

	FPostProcessSettings& S = Grade->Settings;
	S.ColorGainShadows = Tint(1.f - 0.55f * B - 0.25f * A, 1.f - 0.15f * B - 0.1f * A, 1.f + 0.45f * B + 0.35f * A);
	S.ColorOffsetShadows = Tint(0.f, 0.f, 0.004f * A + 0.003f * B);
	S.ColorSaturationShadows = Tint(1.f + 0.4f * B - M, 1.f + 0.4f * B - M, 1.f + 0.4f * B - M);
	S.ColorGainMidtones = Tint(1.f - 0.25f * B + 0.1f * A + 0.15f * G, 1.f - 0.08f * B + 0.1f * A, 1.f + 0.2f * B + 0.2f * A - 0.1f * G);
	S.ColorGammaMidtones = Tint(1.f + 0.3f * A, 1.f + 0.3f * A, 1.f + 0.3f * A);
	S.ColorContrastMidtones = Tint(1.f - 0.4f * A - F, 1.f - 0.4f * A - F, 1.f - 0.4f * A - F);
	S.ColorSaturationMidtones = Tint(1.f + 0.4f * A - M, 1.f + 0.4f * A - M, 1.f + 0.4f * A - M);
	// the lamps stay sodium-warm: the highlights get a touch more colour, never the blue
	S.ColorSaturationHighlights = Tint(1.f + 0.6f * B - 0.5f * M, 1.f + 0.6f * B - 0.5f * M, 1.f + 0.6f * B - 0.5f * M);
	S.ColorGainHighlights = Tint(1.f + 0.4f * G + 0.08f * B, 1.f + 0.15f * G + 0.02f * B, 1.f - 0.25f * G - 0.05f * B);
	S.ColorContrast = Tint(1.f - 0.5f * F, 1.f - 0.5f * F, 1.f - 0.5f * F);

	// indoors it eases out over a few seconds (the eye's own adaptation is ALyudSky's)
	Outdoor = FMath::FInterpConstantTo(Outdoor, Sky->IsPlayerOutdoors() ? 1.f : 0.f, DeltaTime, 0.35f);
	Grade->BlendWeight = Outdoor * (K > 0.f ? 1.f : 0.f);

	// the sky's haze: crisp dry winter air, milky humid summer air, more in mist (the atmosphere's Mie scattering)
	if (Sky->Atmosphere && BaseMie >= 0.f)
	{
		const float Humid = FMath::Pow(FMath::Clamp(W.Humidity, 0.f, 1.f), 4.f);
		float Mie = BaseMie * (0.8f + 0.35f * Summer + 0.45f * Humid + 1.2f * Smooth(20000.f, 3000.f, W.VisibilityM));
		if (K <= 0.f)
		{
			Mie = BaseMie;
		}
		// ease, and only touch the component when it moved (it re-renders the sky's LUTs)
		LastMie = LastMie < 0.f ? Mie : FMath::FInterpTo(LastMie, Mie, DeltaTime, 0.2f);
		if (FMath::Abs(LastMie - Sky->Atmosphere->MieScatteringScale) > BaseMie * 0.003f)
		{
			Sky->Atmosphere->SetMieScatteringScale(LastMie);
		}
	}
	if (CVarDebug.GetValueOnGameThread() > 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(int32(0x6AADE), 0.f, FColor::Orange, DebugReport());
	}
}

FString ULyudGradeSubsystem::DebugReport() const
{
	return FString::Printf(TEXT("GRADE outdoor %.2f winter-night %.2f summer-day %.2f golden %.2f grey %.2f haze %.2f mie %.5f (base %.5f)"),
		Outdoor, WinterNight, SummerDay, Golden, Grey, Haze, LastMie, BaseMie);
}
