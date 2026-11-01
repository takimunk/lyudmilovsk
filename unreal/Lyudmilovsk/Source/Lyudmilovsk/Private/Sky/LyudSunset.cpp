#include "Sky/LyudSunset.h"

#include "Sky/LyudClouds.h"
#include "Sky/LyudSky.h"
#include "Sky/SunsetModel.h"
#include "Weather/LyudWeather.h"
#include "Weather/WeatherModel.h"

#include "Components/SkyAtmosphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarSunsetProfile(TEXT("lyud.Sunset.Profile"), -1,
		TEXT("Forces the sunset's (and sunrise's) kind: -1 = the day's own; 0 golden, 1 orange, 2 pink, 3 red, 4 crimson, 5 grey, 6 spectacular."));
	TAutoConsoleVariable<float> CVarSunsetIntensity(TEXT("lyud.Sunset.Intensity"), 1.f,
		TEXT("Scales every change the sunset makes to the sky, the light and the clouds (1 = as designed, 0 = off)."));
	TAutoConsoleVariable<int32> CVarSunsetSeed(TEXT("lyud.Sunset.Seed"), 0,
		TEXT("Seed of the days' sunset characters (another seed, another run of evenings)."));
	TAutoConsoleVariable<float> CVarSunsetClean(TEXT("lyud.Sunset.Clean"), -1.f,
		TEXT("Overrides how washed the air is 0..1 (after rain; -1 = from the weather)."));
	TAutoConsoleVariable<float> CVarSunsetHaze(TEXT("lyud.Sunset.Haze"), -1.f,
		TEXT("Overrides how hazy the air is 0..1 (-1 = from the weather)."));
	TAutoConsoleVariable<int32> CVarSunsetDebug(TEXT("lyud.Sunset.Debug"), 0, TEXT("1: print the sunset's profile on screen."));

	FLinearColor ToLinear(const LyudSunset::FRGB& C) { return FLinearColor(C.R, C.G, C.B, 1.f); }

	bool Changed(float A, float B) { return !FMath::IsNearlyEqual(A, B, FMath::Max(FMath::Abs(B), 1e-6f) * 2e-3f); }
}

ULyudSunset* ULyudSunset::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULyudSunset>() : nullptr;
}

const FLyudSunsetLook& ULyudSunset::Update(ALyudSky* Sky, const FDateTime& Local, float SunAltDeg)
{
	using namespace LyudSunset;
	const float Strength = FMath::Max(CVarSunsetIntensity.GetValueOnGameThread(), 0.f);
	USkyAtmosphereComponent* Atmosphere = Sky ? Sky->Atmosphere.Get() : nullptr;
	// outside the band (or on Europa) nothing at all: the atmosphere back to its own values once, then no work
	if (Band(SunAltDeg) <= 0.f || Strength <= 0.f || !Atmosphere || Sky->IsSpaceView())
	{
		if (bApplied && Atmosphere)
		{
			ApplyAtmosphere(Atmosphere, 1.f, 1.f, 1.f, 1.f, FLinearColor::White, true);
			bApplied = false;
		}
		Look = FLyudSunsetLook();
		return Look;
	}

	// the day's character: the evening's after noon, the morning's before (the band is empty at noon, so no jump)
	const bool bSunrise = Local.GetHour() < 12;
	const int64 Day = int64(FMath::FloorToDouble((Local - FDateTime(2000, 1, 1)).GetTotalDays()));
	const int32 Seed = CVarSunsetSeed.GetValueOnGameThread();
	const int32 Forced = CVarSunsetProfile.GetValueOnGameThread();
	const int64 Key = ((Day * 2 + (bSunrise ? 1 : 0)) * 1000003 + Seed) * 16 + (Forced + 1);
	if (!Profile.IsValid() || Key != ProfileKey)
	{
		ProfileKey = Key;
		Profile = MakeShared<FDayProfile>(LyudSunset::Profile(uint32(Seed), Day, bSunrise, Local.GetMonth(), Forced));
	}

	// the weather's air, with the clouds as they are drawn (a lyud.Clouds.Preset or override counts)
	const ULyudWeatherSubsystem* Weather = ULyudWeatherSubsystem::Get(this);
	LyudWeather::FWeatherState W = Weather ? Weather->GetModelState() : LyudWeather::FWeatherState();
	if (const ULyudCloudSubsystem* Clouds = ULyudCloudSubsystem::Get(this))
	{
		const FLyudCloudSky Drawn = Clouds->GetCloudSky();
		auto Take = [](LyudWeather::FCloudLayer& To, const FLyudCloudLayer& From)
		{
			To.Cover = From.Cover;
			To.Density = From.Density;
			To.Sheet = From.Sheet;
		};
		Take(W.Low, Drawn.Low);
		Take(W.Mid, Drawn.Mid);
		Take(W.High, Drawn.High);
	}
	const FAir Air = AirOf(W, CVarSunsetClean.GetValueOnGameThread(), CVarSunsetHaze.GetValueOnGameThread());
	const FLook L = LyudSunset::Look(*Profile, Air, SunAltDeg, Strength);

	ApplyAtmosphere(Atmosphere, L.RayleighScale, L.MieAbsScale, L.MieHeightScale, L.OzoneScale, ToLinear(L.SkyTint), false);
	bApplied = true;

	Look.Weight = L.Weight;
	Look.MieScale = L.MieScale;
	Look.MieGAdd = L.MieGAdd;
	Look.LightTint = ToLinear(L.LightTint);
	Look.CloudTint = ToLinear(L.CloudTint);
	Look.AureoleScale = L.AureoleScale;
	Look.DiskScale = L.DiskScale;
	Look.ExposureEV = L.ExposureEV;

	const double Now = FPlatformTime::Seconds();
	if (Now >= NextDebug || CVarSunsetDebug.GetValueOnGameThread() != 0)
	{
		NextDebug = Now + 1.0;
		DebugLine = FString::Printf(TEXT("SUNSET %s %s%s hue %.2f int %.2f aero %.2f glow %.2f after %.2f | air overcast %.2f broken %.2f clean %.2f haze %.2f rain %.2f | w %.2f mie x%.2f abs x%.2f ray x%.2f oz x%.2f light %.2f %.2f %.2f cloud %.2f %.2f %.2f"),
			Profile->bSunrise ? TEXT("sunrise") : TEXT("sunset"), UTF8_TO_TCHAR(KindName(Profile->Kind)), Profile->bSpectacular ? TEXT(" (spectacular)") : TEXT(""),
			Profile->Hue, Profile->Intensity, Profile->Aerosol, Profile->CloudGlow, Profile->Afterglow,
			Air.Overcast, Air.Broken, Air.Clean, Air.Haze, Air.Rain, L.Weight, L.MieScale, L.MieAbsScale, L.RayleighScale, L.OzoneScale,
			L.LightTint.R, L.LightTint.G, L.LightTint.B, L.CloudTint.R, L.CloudTint.G, L.CloudTint.B);
		if (CVarSunsetDebug.GetValueOnGameThread() != 0 && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(int32(0x5A12), 0.f, FColor::Orange, DebugLine);
		}
	}
	return Look;
}

void ULyudSunset::ApplyAtmosphere(USkyAtmosphereComponent* A, float RayleighK, float MieAbsK, float MieHeightK, float OzoneK, const FLinearColor& SkyTint, bool bExact)
{
	if (BaseOf.Get() != A)
	{
		// the class's own values (ALyudSky's default atmosphere), never the live ones: an editor world saved during a
		// sunset must not make its tint the new base
		const ALyudSky* Cdo = GetDefault<ALyudSky>();
		const USkyAtmosphereComponent* D = Cdo && Cdo->Atmosphere ? Cdo->Atmosphere.Get() : GetDefault<USkyAtmosphereComponent>();
		BaseOf = A;
		BaseRayleigh = D->RayleighScatteringScale;
		BaseMieAbs = D->MieAbsorptionScale;
		BaseMieHeight = D->MieExponentialDistribution;
		BaseOzone = D->OtherAbsorptionScale;
		BaseSkyLum = D->SkyLuminanceFactor;
	}
	// each setter re-sends the atmosphere's render state: only on a real change (exactly back at the end)
	auto Differs = [bExact](float Now, float Want) { return bExact ? Now != Want : Changed(Now, Want); };
	if (Differs(A->RayleighScatteringScale, BaseRayleigh * RayleighK))
	{
		A->SetRayleighScatteringScale(BaseRayleigh * RayleighK);
	}
	if (Differs(A->MieAbsorptionScale, BaseMieAbs * MieAbsK))
	{
		A->SetMieAbsorptionScale(BaseMieAbs * MieAbsK);
	}
	if (Differs(A->MieExponentialDistribution, BaseMieHeight * MieHeightK))
	{
		A->SetMieExponentialDistribution(BaseMieHeight * MieHeightK);
	}
	if (Differs(A->OtherAbsorptionScale, BaseOzone * OzoneK))
	{
		A->SetOtherAbsorptionScale(BaseOzone * OzoneK);
	}
	const FLinearColor Sky = BaseSkyLum * SkyTint;
	if (Differs(A->SkyLuminanceFactor.R, Sky.R) || Differs(A->SkyLuminanceFactor.G, Sky.G) || Differs(A->SkyLuminanceFactor.B, Sky.B))
	{
		A->SetSkyLuminanceFactor(Sky);
	}
}
