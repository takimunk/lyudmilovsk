#include "Sky/LyudSky.h"

#include "Sky/LyudSunActivity.h"

#include "Weather/LyudGrade.h"
#include "Weather/LyudWeather.h"
#include "Weather/WeatherModel.h"
#include "MaterialShared.h"
#include "RHIGlobals.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Plants/GameCalendar.h"
#include "HAL/IConsoleManager.h"
#include "UObject/ConstructorHelpers.h"

static TAutoConsoleVariable<float> CVarSkyHourOffset(TEXT("lyud.SkyHourOffset"), 0.f,
	TEXT("Hours added to the time the sky shows (sun, moon, stars), in play and in the editor."));
static TAutoConsoleVariable<float> CVarOvercastEV(TEXT("lyud.Sky.OvercastEV"), 2.f,
	TEXT("By day the eye opens up by this many stops under a thick dark overcast (scaled by how opaque the weather's cloud is)."));
static TAutoConsoleVariable<float> CVarLowSunEV(TEXT("lyud.Sky.LowSunEV"), 0.7f,
	TEXT("By day the eye opens up by this many stops when the sun is low (25 degrees down to 5), as in a winter noon."));
static TAutoConsoleVariable<float> CVarSunDiskGain(TEXT("lyud.Sky.SunDiskGain"), 1.f, TEXT("Multiplies the sun disk's brightness (tuning)."));
static TAutoConsoleVariable<float> CVarMoonDiskGain(TEXT("lyud.Sky.MoonDiskGain"), 1.f, TEXT("Multiplies Europa's brightness on the sky (tuning)."));
static TAutoConsoleVariable<float> CVarSunSpecular(TEXT("lyud.Sky.SunSpecular"), 1.f, TEXT("Multiplies the sunlight's specular (tuning: the sea's glitter path)."));
static TAutoConsoleVariable<float> CVarCloudDensity(TEXT("lyud.Sky.CloudDensity"), 1.f, TEXT("Multiplies the clouds' density (tuning)."));
static TAutoConsoleVariable<float> CVarCloudOpaque(TEXT("lyud.Sky.CloudOpaque"), 1.f, TEXT("Multiplies the clouds' opaque cut-off CloudOpaqueBelow (0 = the engine's tracing to the end; tuning)."));
static TAutoConsoleVariable<float> CVarCloudSamples(TEXT("lyud.Sky.CloudSamples"), 1.f, TEXT("Multiplies the clouds' ray-march samples (tuning)."));
static TAutoConsoleVariable<float> CVarNightMie(TEXT("lyud.Sky.NightMie"), -1.f, TEXT("Overrides NightMieShare when >= 0 (tuning: the haze round the moon)."));
static TAutoConsoleVariable<float> CVarMoonSize(TEXT("lyud.Sky.MoonSizeDeg"), 0.f, TEXT("Europa's apparent size, degrees across, when > 0 (overrides ALyudSky::MoonSizeDeg)."));
static TAutoConsoleVariable<float> CVarSunAureole(TEXT("lyud.Sky.SunAureole"), 1.f, TEXT("Multiplies the warm glow round the sun in the haze (tuning)."));
static TAutoConsoleVariable<float> CVarSunHorizonDimming(TEXT("lyud.Sky.SunHorizonDimming"), -1.f, TEXT("Overrides ALyudSky::SunHorizonDimming when >= 0 (tuning: 1 = the disk dims with the air as physics says)."));
static TAutoConsoleVariable<float> CVarLowSunLight(TEXT("lyud.Sky.LowSunLight"), 1.f, TEXT("Multiplies the low sun's extra light, specular and haze glow (tuning; 0 = the atmosphere alone)."));
static TAutoConsoleVariable<float> CVarSunShafts(TEXT("lyud.Sky.SunShafts"), 1.f, TEXT("Multiplies the sun's light shafts (god rays; 0 = off)."));
static TAutoConsoleVariable<float> CVarSunDetail(TEXT("lyud.Sky.SunDetailContrast"), -1.f, TEXT("Overrides ALyudSky::SunDetailContrast when > 0 (tuning)."));
static TAutoConsoleVariable<float> CVarLowSunDiskGain(TEXT("lyud.Sky.LowSunDiskGain"), -1.f, TEXT("Overrides ALyudSky::LowSunDiskGain when >= 0 (tuning)."));
static TAutoConsoleVariable<float> CVarLowSunCloud(TEXT("lyud.Sky.LowSunCloud"), -1.f, TEXT("Overrides ALyudSky::LowSunCloudLight when >= 0 (tuning: the share of a low sun's extra light the clouds get)."));
static TAutoConsoleVariable<float> CVarSunRimClear(TEXT("lyud.Sky.SunRimClear"), -1.f, TEXT("Overrides ALyudSky::SunRimClear when >= 0 (tuning: the haze's glow taken off round a low disk)."));
static TAutoConsoleVariable<float> CVarSunLimb(TEXT("lyud.Sky.SunLimb"), -1.f, TEXT("Overrides ALyudSky::SunLimbDarkening when > 0 (tuning)."));
static TAutoConsoleVariable<float> CVarLowSunMieG(TEXT("lyud.Sky.LowSunMieG"), -1.f, TEXT("Overrides ALyudSky::LowSunMieAnisotropy when > 0 (tuning)."));
static TAutoConsoleVariable<float> CVarNightLight(TEXT("lyud.Sky.NightLight"), 1.f, TEXT("Multiplies the moonlight and the night sky's glow (tuning)."));

namespace
{
	/** The star dome's radius, cm: beyond the sea plane and the farthest cloud. */
	constexpr double DomeRadius = 2.0e7;

	float SmoothStep01(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	void SetupDirectional(UDirectionalLightComponent* L, int32 AtmosphereIndex)
	{
		L->SetMobility(EComponentMobility::Movable);
		L->SetAtmosphereSunLight(true);
		L->SetAtmosphereSunLightIndex(AtmosphereIndex);
		// outdoors only: outdoor meshes are channels 0 + 1, the club is channel 0
		L->LightingChannels.bChannel0 = false;
		L->LightingChannels.bChannel1 = true;
		L->LightingChannels.bChannel2 = false;
		L->bCastShadowsOnClouds = false;
		L->bCastCloudShadows = true;
	}

	/** The disk-integrated brightness of a Lommel-Seeliger sphere at phase angle A (rad), 1 at full. */
	float LommelSeeligerPhase(float A)
	{
		if (A < 1e-3f)
		{
			return 1.f;
		}
		if (A > PI - 1e-3f)
		{
			return 0.f;
		}
		const float H = 0.5f * A;
		return FMath::Max(0.f, 1.f - FMath::Sin(H) * FMath::Tan(H) * FMath::Loge(1.f / FMath::Tan(0.5f * H)));
	}

	/** The sky's extinction toward an altitude (deg), per channel: sky_hlsl.py TRANS_HLSL (keep in step). */
	FLinearColor AirTransmittance(float AltDeg)
	{
		const float H = FMath::Max(AltDeg, 0.f);
		const float Am = 1.f / (FMath::Sin(FMath::DegreesToRadians(H)) + 0.50572f * FMath::Pow(H + 6.07995f, -1.6364f));
		const float Sink = SmoothStep01(-6.f, 0.5f, AltDeg);
		return FLinearColor(FMath::Exp(-0.07f * (Am - 1.f)) * Sink, FMath::Exp(-0.14f * (Am - 1.f)) * Sink, FMath::Exp(-0.30f * (Am - 1.f)) * Sink);
	}
}

ALyudSky::ALyudSky()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);

	SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	SunLight->SetupAttachment(RootComponent);
	SetupDirectional(SunLight, 0);
	SunLight->ForwardShadingPriority = 1;
	SunLight->LightSourceAngle = SunAngularDiameterDeg;
	// the sky material draws the star itself (spots, granulation, corona), not the atmosphere's flat disk
	SunLight->AtmosphereSunDiskColorScale = FLinearColor::Black;
	// the disk is drawn far dimmer than a real sun of this light's strength: so must its mirror image be (the sea would
	// show a blown white blob where the star sets), and its sheen on wet sand and leaves
	SunLight->SpecularScale = SunSpecularScale;
	// god rays: the bright disk blurred radially over the trees, the town and the clouds in front of it (UpdateSky
	// scales them by how low the sun is)
	SunLight->bEnableLightShaftBloom = true;
	SunLight->BloomScale = 0.f;
	SunLight->BloomThreshold = SunShaftThreshold;
	SunLight->BloomMaxBrightness = 8.f;
	SunLight->BloomTint = FColor(255, 200, 150);

	MoonLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	MoonLight->SetupAttachment(RootComponent);
	SetupDirectional(MoonLight, 1);
	MoonLight->ForwardShadingPriority = 0;
	MoonLight->LightSourceAngle = MoonSizeDeg;
	MoonLight->bCastCloudShadows = false;   // by night only (UpdateSky)
	MoonLight->CloudScatteredLuminanceScale = FLinearColor(MoonCloudScatter, MoonCloudScatter, MoonCloudScatter);
	MoonLight->AtmosphereSunDiskColorScale = FLinearColor::Black;

	// planet top at the actor: put the actor at sea level
	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(RootComponent);
	Atmosphere->TransformMode = ESkyAtmosphereTransformMode::PlanetTopAtComponentTransform;
	// a damp coast, a little hazier than the engine's clear default, but with a softer forward glow than real haze:
	// the aureole round a 10-degree star must not drown the disk
	Atmosphere->MieScatteringScale = 0.0045f;
	Atmosphere->MieAnisotropy = 0.62f;
	Atmosphere->AerialPespectiveViewDistanceScale = 1.6f;

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(RootComponent);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->SourceType = SLS_CapturedScene;

	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(RootComponent);
	Clouds->LayerBottomAltitude = 1.2f;
	Clouds->LayerHeight = 5.f;
	// the view and shadow sample counts come from CloudViewSamples / CloudShadowSamples (UpdateClouds)
	Clouds->ViewSampleCountScale = CloudViewSamples;
	Clouds->ShadowViewSampleCountScale = CloudShadowSamples;
	Clouds->ReflectionViewSampleCountScaleValue = 1.f;

	Dome = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StarDome"));
	Dome->SetupAttachment(RootComponent);
	Dome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dome->SetCastShadow(false);
	Dome->bAffectDistanceFieldLighting = false;
	Dome->bAffectDynamicIndirectLighting = false;
	Dome->SetMobility(EComponentMobility::Movable);
	OutdoorExposure = CreateDefaultSubobject<UPostProcessComponent>(TEXT("OutdoorExposure"));
	OutdoorExposure->SetupAttachment(RootComponent);
	OutdoorExposure->bUnbound = true;
	OutdoorExposure->Priority = 1000.f;
	OutdoorExposure->BlendWeight = 0.f;
	OutdoorExposure->Settings.bOverride_AutoExposureBias = true;
	OutdoorExposure->Settings.bOverride_BloomIntensity = true;
	OutdoorExposure->Settings.BloomIntensity = OutdoorBloom;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/EngineSky/SM_SkySphere.SM_SkySphere"));
	if (SphereMesh.Succeeded())
	{
		Dome->SetStaticMesh(SphereMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CloudMat(TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"));
	if (CloudMat.Succeeded())
	{
		CloudMaterial = CloudMat.Object;
	}
}

FDateTime ALyudSky::ShownTime() const
{
	const UWorld* World = GetWorld();
	const FDateTime Base = World && World->IsGameWorld() ? UGameCalendar::NowIn(World) : EditorPreviewTime;
	return Base + FTimespan::FromHours(HourOffset + CVarSkyHourOffset.GetValueOnGameThread());
}

float ALyudSky::LowSun() const
{
	return SmoothStep01(25.f, 2.f, (float)SunPos.AltitudeDeg);
}

FVector ALyudSky::WorldDirection(const FVector& Enu) const
{
	const FVector North = FRotator(0.0, NorthYawDeg, 0.0).RotateVector(FVector(0.0, 1.0, 0.0));
	const FVector East = FVector::CrossProduct(FVector::UpVector, North);
	return (East * Enu.X + North * Enu.Y + FVector::UpVector * Enu.Z).GetSafeNormal();
}

float ALyudSky::MoonSizeNowDeg() const
{
	const float Pin = CVarMoonSize.GetValueOnGameThread();
	return FMath::Clamp(Pin > 0.f ? Pin : MoonSizeDeg, 0.1f, 40.f);
}

FVector ALyudSky::BearingDirection(double BearingDeg) const
{
	const double B = FMath::DegreesToRadians(BearingDeg);
	return WorldDirection(FVector(FMath::Sin(B), FMath::Cos(B), 0.0));
}

void ALyudSky::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Dome->SetVisibility(SkyMaterial != nullptr);
	SunLight->SetLightSourceAngle(SunAngularDiameterDeg);
	SunLight->SetSpecularScale(SunSpecularScale);
	MoonLight->SetLightSourceAngle(MoonSizeNowDeg());
	SunLight->SetAtmosphereSunDiskColorScale(FLinearColor::Black);
	MoonLight->SetAtmosphereSunDiskColorScale(FLinearColor::Black);
	OutdoorExposure->Settings.BloomIntensity = OutdoorBloom;
	SkyMID = nullptr;
	CloudMID = nullptr;
	MakeInstances();
	if (Dome->GetStaticMesh())
	{
		const double R = FMath::Max(1.0, (double)Dome->GetStaticMesh()->GetBounds().SphereRadius);
		Dome->SetRelativeScale3D(FVector(DomeRadius / R));
	}
	SkyLight->SetIntensity(SkyLightIntensity);
	UpdateSky();
}

void ALyudSky::BeginPlay()
{
	Super::BeginPlay();
	// an actor loaded with its level gets no OnConstruction in a game: make the dome's and the clouds' instances here
	MakeInstances();
	// the club's exposure: the strongest unbound volume that sets one
	float Best = -FLT_MAX;
	for (TActorIterator<APostProcessVolume> It(GetWorld()); It; ++It)
	{
		if (It->bUnbound && It->bEnabled && It->Settings.bOverride_AutoExposureBias && It->Priority > Best)
		{
			Best = It->Priority;
			BaseExposureBias = It->Settings.AutoExposureBias;
		}
	}
	UpdateSky();
}

void ALyudSky::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	const bool bGame = World && World->IsGameWorld();
	const float Shift = CVarSkyHourOffset.GetValueOnGameThread();
	if (bGame || Shift != LastShift)
	{
		LastShift = Shift;
		UpdateSky();
		if (!bGame)
		{
			UE_LOG(LogTemp, Display, TEXT("%s"), *DebugReport());
		}
	}
	if (bGame)
	{
		UpdatePlayerChannels(DeltaSeconds);
		UpdateExposure(DeltaSeconds);
	}
}

void ALyudSky::UpdateSky()
{
	const FDateTime Utc = ShownTime() - FTimespan::FromHours(UtcOffsetHours);
	SunPos = LyudEphemeris::Sun(Utc, LatitudeDeg, LongitudeDeg);
	MoonPos = LyudEphemeris::Moon(Utc, LatitudeDeg, LongitudeDeg, &MoonPhaseEphemeris);

	// a directional light shines along its forward axis: from the body toward the ground
	SunLight->SetWorldRotation((-WorldDirection(SunPos.EastNorthUp())).Rotation());
	MoonLight->SetWorldRotation((-WorldDirection(MoonPos.EastNorthUp())).Rotation());

	// The atmosphere reddens and dims the sun near the horizon by itself; below it the sun still lights
	// the upper air (twilight) until it is well down, but no longer the ground.
	const float SunAlt = (float)SunPos.AltitudeDeg;
	// a low sun: the atmosphere alone leaves its light weak and the scene flat and grey; the eye adapts to it and the
	// lit faces still glow warm against long deep shadows, so its light is kept up, the sky light down, its mirror
	// image on the sea stronger (the glitter path), and the haze's forward glow stronger
	const float Low = LowSun() * FMath::Max(CVarLowSunLight.GetValueOnGameThread(), 0.f);
	// a flare on the star: up to a quarter brighter and whiter at a big one's peak (ALyudRareSky), a little for the
	// activity's everyday flares (ULyudSunActivity)
	const float Flare = FMath::Max(FlareBoost, ActivityFlare * ActivityFlareLight / 0.25f);
	SunLight->SetIntensity(SunIlluminance * SmoothStep01(-12.f, -4.f, SunAlt) * (1.f + LowSunLightGain * Low) * (1.f + 0.25f * Flare));
	SunLight->SetLightColor(FMath::Lerp(SunColor, FLinearColor(1.f, 0.97f, 0.95f), 0.35f * Flare));
	// the clouds get only LowSunCloudLight of that extra light, and the sky and the haze over the land (the aerial
	// perspective) none: lit by the boosted light they paled to the disk's brightness, the clouds drawn over it and the
	// haze round it washing the setting star out
	const float CloudShare = CVarLowSunCloud.GetValueOnGameThread() >= 0.f ? CVarLowSunCloud.GetValueOnGameThread() : LowSunCloudLight;
	const float CloudK = (1.f + LowSunLightGain * Low * CloudShare) / (1.f + LowSunLightGain * Low);
	if (!FMath::IsNearlyEqual(SunLight->CloudScatteredLuminanceScale.R, CloudK, 1e-3f))
	{
		SunLight->CloudScatteredLuminanceScale = FLinearColor(CloudK, CloudK, CloudK);
		SunLight->MarkRenderStateDirty();
	}
	const float HazeK = 1.f / (1.f + LowSunLightGain * Low);
	if (!FMath::IsNearlyEqual(Atmosphere->SkyAndAerialPerspectiveLuminanceFactor.R, HazeK, 1e-3f))
	{
		Atmosphere->SetSkyAndAerialPerspectiveLuminanceFactor(FLinearColor(HazeK, HazeK, HazeK));
	}
	const float SkyK = SkyLightIntensity * FMath::Lerp(1.f, LowSunAmbient, Low * SmoothStep01(-4.f, 2.f, SunAlt));
	if (!FMath::IsNearlyEqual(SkyLight->Intensity, SkyK, 1e-3f))
	{
		SkyLight->SetIntensity(SkyK);
	}
	// god rays from the low star (a fifth of them when high), gone once it sets
	const float Shafts = SunShaftBloom * CVarSunShafts.GetValueOnGameThread() * FMath::Lerp(0.2f, 1.f, Low) * SmoothStep01(-3.f, 1.f, SunAlt);
	if (!FMath::IsNearlyEqual(SunLight->BloomScale, Shafts, 1e-3f))
	{
		SunLight->SetBloomScale(Shafts);
		SunLight->SetBloomThreshold(SunShaftThreshold);
	}
	const float Dark = 1.f - SmoothStep01(-10.f, -2.f, SunAlt);   // 1 once the sun is down past civil twilight
	// Europa's phase from the very directions the sky material lights its disk with: the phase angle is the angle at
	// Europa between the sun and the viewer, so the light's phase and the disk's are the same
	const FVector SunDir = WorldDirection(SunPos.EastNorthUp());
	const FVector MoonDir = WorldDirection(MoonPos.EastNorthUp());
	const float CosA = FMath::Clamp(-(float)FVector::DotProduct(SunDir, MoonDir), -1.f, 1.f);
	MoonPhaseAngle = FMath::Acos(CosA);
	MoonPhase = 0.5 * (1.0 + CosA);
	const float B0 = FMath::Max(MoonOppositionSurge, 0.f);
	const float Surge = (1.f + B0 / (1.f + FMath::Tan(0.5f * FMath::Min(MoonPhaseAngle, 3.1f)) / FMath::Max(MoonOppositionWidth, 1e-3f))) / (1.f + B0);
	MoonLawFactor = Surge * FMath::Exp(-MoonRoughness * MoonPhaseAngle);
	// its light: the disk's brightness integrated, through the same air as the disk (a low moon dims and yellows),
	// gone once it sets
	const FLinearColor Air = AirTransmittance((float)MoonPos.AltitudeDeg);
	const float AirY = FMath::Max(0.2126f * Air.R + 0.7152f * Air.G + 0.0722f * Air.B, 1e-4f);
	const float MoonUp = SmoothStep01(-2.f, 3.f, (float)MoonPos.AltitudeDeg);
	MoonLight01 = LommelSeeligerPhase(MoonPhaseAngle) * MoonLawFactor * AirY * MoonUp;
	const float MoonLux = FullMoonIlluminance * MoonLight01 * CVarNightLight.GetValueOnGameThread();
	MoonLight->SetIntensity(MoonLux);
	MoonLight->SetLightColor(MoonColor * FLinearColor(Air.R / AirY, Air.G / AirY, Air.B / AirY));
	MoonLight->SetVisibility(MoonLux > 1e-4f);
	// the night's ambient in the moonlight's aquamarine
	const FLinearColor Ambient = FMath::Lerp(FLinearColor::White, NightAmbientTint, Dark);
	if (!SkyLight->GetLightColor().Equals(Ambient, 1e-3f))
	{
		SkyLight->SetLightColor(Ambient);
	}

	const float Spec = SunSpecularScale * CVarSunSpecular.GetValueOnGameThread() * (1.f + LowSunSpecularGain * Low);
	if (!FMath::IsNearlyEqual(SunLight->SpecularScale, Spec))
	{
		SunLight->SetSpecularScale(Spec);
	}
	// by night the haze is thinner: the moonlight it scatters would wrap Europa in a big glow
	const float MieNight = CVarNightMie.GetValueOnGameThread() >= 0.f ? CVarNightMie.GetValueOnGameThread() : NightMieShare;
	// and the weather's haze on top (humid air, mist, rain: ULyudGradeSubsystem, eased there)
	// a low sun's aureole: more haze along its long path and a narrower, brighter forward lobe
	const float Mie = 0.0045f * FMath::Lerp(MieNight, 1.f, SmoothStep01(-10.f, 0.f, SunAlt)) * ULyudGradeSubsystem::GetSkyHazeScale(GetWorld())
		* (1.f + LowSunMieGain * Low);
	if (!FMath::IsNearlyEqual(Atmosphere->MieScatteringScale, Mie, 1e-6f))
	{
		Atmosphere->SetMieScatteringScale(Mie);
	}
	const float MieG = FMath::Lerp(0.62f, CVarLowSunMieG.GetValueOnGameThread() > 0.f ? CVarLowSunMieG.GetValueOnGameThread() : LowSunMieAnisotropy, FMath::Clamp(Low, 0.f, 1.f) * SmoothStep01(-6.f, 0.f, SunAlt));
	if (!FMath::IsNearlyEqual(Atmosphere->MieAnisotropy, MieG, 1e-3f))
	{
		Atmosphere->SetMieAnisotropy(MieG);
	}
	UpdateClouds();

	// one shadowed directional light at a time
	const bool bSunShadows = SunAlt > -1.f;
	SunLight->SetCastShadows(bSunShadows);
	MoonLight->SetCastShadows(!bSunShadows && Dark > 0.5f);
	// and one light's cloud shadows: clouds crossing the moon dim the moonlight under them
	if (SunLight->bCastCloudShadows != bSunShadows)
	{
		SunLight->bCastCloudShadows = bSunShadows;
		SunLight->MarkRenderStateDirty();
	}
	if (MoonLight->bCastCloudShadows == bSunShadows)
	{
		MoonLight->bCastCloudShadows = !bSunShadows;
		MoonLight->MarkRenderStateDirty();
	}
	SunLight->SetForwardShadingPriority(bSunShadows ? 1 : 0);
	MoonLight->SetForwardShadingPriority(bSunShadows ? 0 : 1);
	UpdateSkyMaterial();
}

void ALyudSky::MakeInstances()
{
	if (!SkyMID && SkyMaterial && Dome->GetStaticMesh())
	{
		SkyMID = UMaterialInstanceDynamic::Create(SkyMaterial, this);
		SkyMID->SetScalarParameterValue(TEXT("StarBrightness"), StarBrightness);
		Dome->SetMaterial(0, SkyMID);
	}
	if (!CloudMID && CloudMaterial)
	{
		CloudMID = UMaterialInstanceDynamic::Create(CloudMaterial, this);
		CloudMID->SetFlags(RF_Transient);
		// the clouds take the instance once its shaders are ready (UpdateClouds): the volumetric cloud renderer
		// asserts on the surface material a still-compiling one falls back to; until then no clouds (a level saved
		// with an instance on the component would bring it back)
		bCloudMIDSet = false;
		Clouds->SetMaterial(nullptr);
		float V = 0.f;
		CloudBaseDensity = CloudMID->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Cloud_GlobalDensity")), V) ? V : -1.f;
		if (CloudBaseDensity < 0.f)
		{
			TArray<FMaterialParameterInfo> Infos;
			TArray<FGuid> Ids;
			CloudMID->GetAllScalarParameterInfo(Infos, Ids);
			FString Names;
			for (const FMaterialParameterInfo& I : Infos)
			{
				Names += I.Name.ToString() + TEXT(", ");
			}
			UE_LOG(LogTemp, Warning, TEXT("LyudSky: the cloud material has no \"Cloud_GlobalDensity\"; its scalars: %s"), *Names);
		}
	}
	UpdateClouds();
}

void ALyudSky::UpdateClouds()
{
	if (CloudMID && !bCloudMIDSet)
	{
		const FMaterialResource* R = CloudMID->GetMaterialResource(GMaxRHIShaderPlatform);
		const bool bMap = R && R->GetGameThreadShaderMap() != nullptr;
		const bool bFinished = R && R->IsCompilationFinished();
		const bool bComplete = R && R->IsGameThreadShaderMapComplete();
		const double Now0 = FPlatformTime::Seconds();
		if (!bComplete)
		{
			CloudCompleteSince = 0.0;
		}
		else if (CloudCompleteSince <= 0.0)
		{
			CloudCompleteSince = Now0;
		}
		// a couple of seconds after the game thread sees it complete, so the render thread has it too
		if (bComplete && Now0 - CloudCompleteSince > 2.0 && CloudMID->GetMaterial()->MaterialDomain == MD_Volume && (R->GetCompileErrors().Num() == 0))
		{
			Clouds->SetMaterial(CloudMID);
			bCloudMIDSet = true;
			UE_LOG(LogTemp, Display, TEXT("LyudSky: the cloud material is ready (complete %d)"), bComplete ? 1 : 0);
		}
		else
		{
			if (R && bFinished && !bComplete && !bCloudShadersForced)
			{
				// an incomplete shader map (only what was asked for so far): compile the rest, the cloud passes among it
				bCloudShadersForced = true;
				CloudMID->GetMaterial()->CacheShaders(EMaterialShaderPrecompileMode::Background);
			}
			const double Now = FPlatformTime::Seconds();
			if (Now > NextCloudLog)
			{
				NextCloudLog = Now + 10.0;
				UE_LOG(LogTemp, Display, TEXT("LyudSky: waiting for the cloud material: resource %d map %d finished %d complete %d errors %d"),
					R ? 1 : 0, bMap ? 1 : 0, bFinished ? 1 : 0, bComplete ? 1 : 0, R ? R->GetCompileErrors().Num() : -1);
			}
			if (R && R->GetCompileErrors().Num() > 0 && !bCloudErrorsLogged)
			{
				bCloudErrorsLogged = true;
				for (const FString& E : R->GetCompileErrors())
				{
					UE_LOG(LogTemp, Error, TEXT("LyudSky: cloud material: %s"), *E);
				}
			}
		}
	}
	const float K = CVarCloudSamples.GetValueOnGameThread();
	if (!FMath::IsNearlyEqual(Clouds->ViewSampleCountScale, CloudViewSamples * K))
	{
		Clouds->SetViewSampleCountScale(CloudViewSamples * K);
		Clouds->SetShadowViewSampleCountScale(CloudShadowSamples * K);
	}
	const float Stop = FMath::Clamp(CVarCloudOpaque.GetValueOnGameThread() * CloudOpaqueBelow, 0.005f, 0.9f);
	if (!FMath::IsNearlyEqual(Clouds->StopTracingTransmittanceThreshold, Stop))
	{
		Clouds->SetStopTracingTransmittanceThreshold(Stop);
	}
	if (CloudMID && CloudBaseDensity >= 0.f)
	{
		const float D = CloudBaseDensity * CloudDensityScale * CVarCloudDensity.GetValueOnGameThread();
		float Now = 0.f;
		if (!CloudMID->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Cloud_GlobalDensity")), Now) || !FMath::IsNearlyEqual(Now, D))
		{
			CloudMID->SetScalarParameterValue(TEXT("Cloud_GlobalDensity"), D);
		}
	}
}

void ALyudSky::UpdateSkyMaterial()
{
	if (!SkyMID)
	{
		return;
	}
	auto Vec = [](const FVector& V) { return FLinearColor(float(V.X), float(V.Y), float(V.Z), 0.f); };
	const FVector SunDir = WorldDirection(SunPos.EastNorthUp());
	const FVector MoonDir = WorldDirection(MoonPos.EastNorthUp());
	// the celestial pole: the stars, the star's spin and Europa's north all turn about it
	const double Lat = FMath::DegreesToRadians(LatitudeDeg);
	const FVector Pole = WorldDirection(FVector(0.0, FMath::Cos(Lat), FMath::Sin(Lat)));
	SkyMID->SetVectorParameterValue(TEXT("SunDir"), Vec(SunDir));
	SkyMID->SetVectorParameterValue(TEXT("MoonDir"), Vec(MoonDir));
	SkyMID->SetVectorParameterValue(TEXT("Pole"), Vec(Pole));

	// the star turns once in SunRotationDays; its spots are new every in-game day (it is night at the change)
	const FDateTime Now = ShownTime();
	const double Days = (Now - FDateTime(2026, 1, 1)).GetTotalDays();
	const float Spin = float(FMath::Fmod(Days / FMath::Max(SunRotationDays, 0.1f), 1.0) * 2.0 * PI);
	const float DaySeed = float(FMath::FloorToDouble(Days));
	// a low sun: the disk brighter (the sky's own luminance stays where the atmosphere puts it although the light is
	// kept up: UpdateSky scales the atmosphere's sky and haze back, SkyLumScale stays 1)
	const float Low = LowSun() * FMath::Max(CVarLowSunLight.GetValueOnGameThread(), 0.f);
	const float DiskLow = CVarLowSunDiskGain.GetValueOnGameThread() >= 0.f ? CVarLowSunDiskGain.GetValueOnGameThread() : LowSunDiskGain;
	const float SunLum = SunIlluminance * SunDiskGain * CVarSunDiskGain.GetValueOnGameThread() * (1.f + DiskLow * Low);
	SkyMID->SetVectorParameterValue(TEXT("SunDisk"), FLinearColor(FMath::DegreesToRadians(SunAngularDiameterDeg * 0.5f), SunLum, DaySeed, Spin));
	// the day's spots are put on the face the star turns to us at noon, so they cross the disk while it is up
	const float NoonSpin = float(FMath::Fmod((DaySeed + 0.5) / FMath::Max(SunRotationDays, 0.1f), 1.0) * 2.0 * PI);
	SkyMID->SetVectorParameterValue(TEXT("SunSpots"), FLinearColor(NoonSpin, SunSpotUmbra, SunGranulation, SunFaculae));
	// the aureole, how much of the air's dimming the disk keeps, the prominences' and the corona's gains
	const float Dimming = CVarSunHorizonDimming.GetValueOnGameThread() >= 0.f ? CVarSunHorizonDimming.GetValueOnGameThread() : SunHorizonDimming;
	SkyMID->SetVectorParameterValue(TEXT("SunLook"), FLinearColor(SunAureole * CVarSunAureole.GetValueOnGameThread(),
		FMath::Clamp(Dimming, 0.f, 1.f), SunProminenceGain, SunCoronaGain));
	const float Detail = CVarSunDetail.GetValueOnGameThread() > 0.f ? CVarSunDetail.GetValueOnGameThread() : SunDetailContrast;
	const float Rim = CVarSunRimClear.GetValueOnGameThread() >= 0.f ? CVarSunRimClear.GetValueOnGameThread() : SunRimClear;
	const float LimbK = CVarSunLimb.GetValueOnGameThread() > 0.f ? CVarSunLimb.GetValueOnGameThread() : SunLimbDarkening;
	SkyMID->SetVectorParameterValue(TEXT("SunLook2"), FLinearColor(Detail, FMath::Clamp(Rim * Low, 0.f, 0.9f), LimbK, 0.f));
	// the star's activity: regions with their spots, prominences, flares (ULyudSunActivity sets their parameters)
	if (ULyudSunActivity* Activity = ULyudSunActivity::Get(this))
	{
		ActivityFlare = Activity->Update(SkyMID, Now, SunRotationDays, NoonSpin);
	}
	// z: Europa's phase law over Lommel-Seeliger, the same factor its light takes (UpdateSky)
	SkyMID->SetVectorParameterValue(TEXT("MoonLook"), FLinearColor(0.f, MoonNightHalo, MoonLawFactor, 0.f));

	const float Day = SmoothStep01(-6.f, 6.f, (float)SunPos.AltitudeDeg);
	const float NightK = CVarNightLight.GetValueOnGameThread();
	const float MoonLum = SunIlluminance * MoonDiskGain * FMath::Lerp(MoonNightBoost, MoonDayBoost, Day) * CVarMoonDiskGain.GetValueOnGameThread();
	// the ashen light on the dark side: a little of the sunlit ice's brightness, more by night
	const float MoonSize = MoonSizeNowDeg();
	if (!FMath::IsNearlyEqual(MoonLight->LightSourceAngle, MoonSize))
	{
		MoonLight->SetLightSourceAngle(MoonSize);
	}
	SkyMID->SetVectorParameterValue(TEXT("MoonDisk"), FLinearColor(FMath::DegreesToRadians(MoonSize * 0.5f), MoonLum, 0.012f, 1.f - Day));
	SkyMID->SetVectorParameterValue(TEXT("NightGlow"), NightSkyGlow * ((1.f - Day) * NightK));
}

void ALyudSky::UpdatePlayerChannels(float DeltaSeconds)
{
	if (!bLightPlayerOutdoors)
	{
		return;
	}
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	// Under the open sky when nothing is above the head, or only outdoor meshes (channel 1: a tree, the
	// porch over the door, the cable car's roof), which cast the sun's shadow themselves.
	const FVector Head = Pawn->GetPawnViewLocation();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LyudSkyRoof), false, Pawn);
	TArray<AActor*> Attached;
	Pawn->GetAttachedActors(Attached, true, true);
	Params.AddIgnoredActors(Attached);
	bool bOpen = true;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Head, Head + FVector(0, 0, RoofSearchHeight), ECC_Visibility, Params))
	{
		const UPrimitiveComponent* C = Hit.GetComponent();
		// an outdoor mesh that is a building with rooms (tag "Roof", e.g. the Academgorodok's houses) is a roof
		bOpen = C && C->LightingChannels.bChannel1 && !(C->GetOwner() && C->GetOwner()->ActorHasTag(TEXT("Roof")));
	}
	// a short vote so a gap between ceiling tiles or a door frame overhead does not flicker the light
	OutdoorsVote = FMath::FInterpConstantTo(OutdoorsVote, bOpen ? 1.f : 0.f, DeltaSeconds, 5.f);
	const bool bNow = bPlayerOutdoors ? OutdoorsVote > 0.3f : OutdoorsVote > 0.7f;
	if (bNow == bPlayerOutdoors && LitPawn == Pawn)
	{
		return;
	}
	bPlayerOutdoors = bNow;
	LitPawn = Pawn;
	TArray<AActor*> Actors = Attached;
	Actors.Add(Pawn);
	for (AActor* A : Actors)
	{
		TInlineComponentArray<UPrimitiveComponent*> Prims(A);
		for (UPrimitiveComponent* P : Prims)
		{
			P->SetLightingChannels(true, bPlayerOutdoors, P->LightingChannels.bChannel2);
		}
	}
}

void ALyudSky::UpdateExposure(float DeltaSeconds)
{
	OutdoorWeight = FMath::FInterpConstantTo(OutdoorWeight, bPlayerOutdoors ? 1.f : 0.f, DeltaSeconds, 1.f / FMath::Max(EyeAdaptSeconds, 0.01f));
	const float Night = 1.f - SmoothStep01(-8.f, 8.f, (float)SunPos.AltitudeDeg);
	// a quarter of a stop wider per stop of moonlight lost, up to MoonlessExposureEV
	const float Moonless = FMath::Clamp(-FMath::Log2(MoonLight01 + 0.03f) / 4.f, 0.f, MoonlessExposureEV);
	// by day the eye adapts too: under a thick overcast and to a low winter sun (the weather's cloud, eased)
	float Opacity = 0.f;
	if (const ULyudWeatherSubsystem* Weather = ULyudWeatherSubsystem::Get(this))
	{
		const LyudWeather::FWeatherState& W = Weather->GetModelState();
		Opacity = 1.f - (1.f - W.Low.Cover * FMath::Clamp(W.Low.Density * 1.2f, 0.f, 1.f)) * (1.f - 0.7f * W.Mid.Cover * W.Mid.Density);
	}
	const float LowSun = SmoothStep01(25.f, 5.f, (float)SunPos.AltitudeDeg);
	const float DayEV = (1.f - Night) * (CVarOvercastEV.GetValueOnGameThread() * Opacity + CVarLowSunEV.GetValueOnGameThread() * LowSun);
	DayAdaptEV = FMath::FInterpTo(DayAdaptEV, DayEV, DeltaSeconds, 0.5f);
	OutdoorExposure->Settings.AutoExposureBias = BaseExposureBias + (NightExposureEV + Moonless) * Night + DayAdaptEV;
	OutdoorExposure->Settings.BloomIntensity = FMath::Lerp(OutdoorBloom, NightBloom, Night);
	OutdoorExposure->BlendWeight = OutdoorWeight;
}

FString ALyudSky::DebugReport() const
{
	return FString::Printf(TEXT("LYUDSKY %s sun alt %.2f az %.2f | moon alt %.2f az %.2f phase %.2f | sun lux %.2f moon lux %.4f | outdoors %d exposure %.2f x %.2f | moon phase angle %.1f law %.3f light %.4f ephemeris phase %.3f"),
		*ShownTime().ToString(TEXT("%Y-%m-%d %H:%M")), SunPos.AltitudeDeg, SunPos.AzimuthDeg, MoonPos.AltitudeDeg,
		MoonPos.AzimuthDeg, MoonPhase, SunLight->Intensity, MoonLight->Intensity, bPlayerOutdoors ? 1 : 0,
		OutdoorExposure->Settings.AutoExposureBias, OutdoorWeight, FMath::RadiansToDegrees(MoonPhaseAngle), MoonLawFactor,
		MoonLight01, MoonPhaseEphemeris) + FString::Printf(TEXT(" | low sun %.2f sky light %.2f shafts %.2f spec %.4f mie g %.2f | %s"),
		LowSun(), SkyLight->Intensity, SunLight->BloomScale, SunLight->SpecularScale, Atmosphere->MieAnisotropy,
		ULyudSunActivity::Get(this) ? *ULyudSunActivity::Get(this)->GetDebugLine() : TEXT("no activity"));
}
