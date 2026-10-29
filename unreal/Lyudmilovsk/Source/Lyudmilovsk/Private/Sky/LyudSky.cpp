#include "Sky/LyudSky.h"

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
static TAutoConsoleVariable<float> CVarSunDiskGain(TEXT("lyud.Sky.SunDiskGain"), 1.f, TEXT("Multiplies the sun disk's brightness (tuning)."));
static TAutoConsoleVariable<float> CVarMoonDiskGain(TEXT("lyud.Sky.MoonDiskGain"), 1.f, TEXT("Multiplies Europa's brightness on the sky (tuning)."));
static TAutoConsoleVariable<float> CVarSunSpecular(TEXT("lyud.Sky.SunSpecular"), 1.f, TEXT("Multiplies the sunlight's specular (tuning: the sea's glitter path)."));
static TAutoConsoleVariable<float> CVarCloudDensity(TEXT("lyud.Sky.CloudDensity"), 1.f, TEXT("Multiplies the clouds' density (tuning)."));
static TAutoConsoleVariable<float> CVarCloudOpaque(TEXT("lyud.Sky.CloudOpaque"), 1.f, TEXT("Multiplies the clouds' opaque cut-off CloudOpaqueBelow (0 = the engine's tracing to the end; tuning)."));
static TAutoConsoleVariable<float> CVarCloudSamples(TEXT("lyud.Sky.CloudSamples"), 1.f, TEXT("Multiplies the clouds' ray-march samples (tuning)."));
static TAutoConsoleVariable<float> CVarNightMie(TEXT("lyud.Sky.NightMie"), -1.f, TEXT("Overrides NightMieShare when >= 0 (tuning: the haze round the moon)."));
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

	MoonLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	MoonLight->SetupAttachment(RootComponent);
	SetupDirectional(MoonLight, 1);
	MoonLight->ForwardShadingPriority = 0;
	MoonLight->LightSourceAngle = MoonAngularDiameterDeg;
	MoonLight->bCastCloudShadows = false;
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

FVector ALyudSky::WorldDirection(const FVector& Enu) const
{
	const FVector North = FRotator(0.0, NorthYawDeg, 0.0).RotateVector(FVector(0.0, 1.0, 0.0));
	const FVector East = FVector::CrossProduct(FVector::UpVector, North);
	return (East * Enu.X + North * Enu.Y + FVector::UpVector * Enu.Z).GetSafeNormal();
}

void ALyudSky::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Dome->SetVisibility(SkyMaterial != nullptr);
	SunLight->SetLightSourceAngle(SunAngularDiameterDeg);
	SunLight->SetSpecularScale(SunSpecularScale);
	MoonLight->SetLightSourceAngle(MoonAngularDiameterDeg);
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
	MoonPos = LyudEphemeris::Moon(Utc, LatitudeDeg, LongitudeDeg, &MoonPhase);

	// a directional light shines along its forward axis: from the body toward the ground
	SunLight->SetWorldRotation((-WorldDirection(SunPos.EastNorthUp())).Rotation());
	MoonLight->SetWorldRotation((-WorldDirection(MoonPos.EastNorthUp())).Rotation());

	// The atmosphere reddens and dims the sun near the horizon by itself; below it the sun still lights
	// the upper air (twilight) until it is well down, but no longer the ground.
	const float SunAlt = (float)SunPos.AltitudeDeg;
	SunLight->SetIntensity(SunIlluminance * SmoothStep01(-12.f, -4.f, SunAlt));
	SunLight->SetLightColor(SunColor);
	const float Dark = 1.f - SmoothStep01(-10.f, -2.f, SunAlt);   // 1 once the sun is down past civil twilight
	const float MoonUp = SmoothStep01(-2.f, 3.f, (float)MoonPos.AltitudeDeg);
	// the lit fraction dims the moon faster than linearly (a half moon is ~1/10 of a full one)
	const float MoonLux = FullMoonIlluminance * FMath::Pow((float)MoonPhase, 2.5f) * MoonUp * CVarNightLight.GetValueOnGameThread();
	MoonLight->SetIntensity(MoonLux);
	MoonLight->SetLightColor(MoonColor);
	MoonLight->SetVisibility(MoonLux > 1e-4f);

	const float Spec = SunSpecularScale * CVarSunSpecular.GetValueOnGameThread();
	if (!FMath::IsNearlyEqual(SunLight->SpecularScale, Spec))
	{
		SunLight->SetSpecularScale(Spec);
	}
	// by night the haze is thinner: the moonlight it scatters would wrap Europa in a big glow
	const float MieNight = CVarNightMie.GetValueOnGameThread() >= 0.f ? CVarNightMie.GetValueOnGameThread() : NightMieShare;
	const float Mie = 0.0045f * FMath::Lerp(MieNight, 1.f, SmoothStep01(-10.f, 0.f, SunAlt));
	if (!FMath::IsNearlyEqual(Atmosphere->MieScatteringScale, Mie, 1e-6f))
	{
		Atmosphere->SetMieScatteringScale(Mie);
	}
	UpdateClouds();

	// one shadowed directional light at a time
	const bool bSunShadows = SunAlt > -1.f;
	SunLight->SetCastShadows(bSunShadows);
	MoonLight->SetCastShadows(!bSunShadows && Dark > 0.5f);
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
		Clouds->SetMaterial(CloudMID);
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
	const float SunLum = SunIlluminance * SunDiskGain * CVarSunDiskGain.GetValueOnGameThread();
	SkyMID->SetVectorParameterValue(TEXT("SunDisk"), FLinearColor(FMath::DegreesToRadians(SunAngularDiameterDeg * 0.5f), SunLum, DaySeed, Spin));
	// the day's spots are put on the face the star turns to us at noon, so they cross the disk while it is up
	const float NoonSpin = float(FMath::Fmod((DaySeed + 0.5) / FMath::Max(SunRotationDays, 0.1f), 1.0) * 2.0 * PI);
	SkyMID->SetVectorParameterValue(TEXT("SunSpots"), FLinearColor(NoonSpin, SunSpotUmbra, SunGranulation, SunFaculae));
	SkyMID->SetVectorParameterValue(TEXT("MoonLook"), FLinearColor(0.f, MoonNightHalo, 0.f, 0.f));

	const float Day = SmoothStep01(-6.f, 6.f, (float)SunPos.AltitudeDeg);
	const float NightK = CVarNightLight.GetValueOnGameThread();
	const float MoonLum = SunIlluminance * MoonDiskGain * FMath::Lerp(MoonNightBoost, MoonDayBoost, Day) * CVarMoonDiskGain.GetValueOnGameThread();
	// the ashen light on the dark side: a little of the sunlit ice's brightness, more by night
	SkyMID->SetVectorParameterValue(TEXT("MoonDisk"), FLinearColor(FMath::DegreesToRadians(MoonAngularDiameterDeg * 0.5f), MoonLum, 0.012f, 1.f - Day));
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
	OutdoorExposure->Settings.AutoExposureBias = BaseExposureBias + NightExposureEV * Night;
	OutdoorExposure->Settings.BloomIntensity = FMath::Lerp(OutdoorBloom, NightBloom, Night);
	OutdoorExposure->BlendWeight = OutdoorWeight;
}

FString ALyudSky::DebugReport() const
{
	return FString::Printf(TEXT("LYUDSKY %s sun alt %.2f az %.2f | moon alt %.2f az %.2f phase %.2f | sun lux %.2f moon lux %.4f | outdoors %d exposure %.2f x %.2f"),
		*ShownTime().ToString(TEXT("%Y-%m-%d %H:%M")), SunPos.AltitudeDeg, SunPos.AzimuthDeg, MoonPos.AltitudeDeg,
		MoonPos.AzimuthDeg, MoonPhase, SunLight->Intensity, MoonLight->Intensity, bPlayerOutdoors ? 1 : 0,
		OutdoorExposure->Settings.AutoExposureBias, OutdoorWeight);
}
