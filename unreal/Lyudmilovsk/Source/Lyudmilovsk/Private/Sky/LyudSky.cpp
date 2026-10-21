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
	SunLight->LightSourceAngle = 0.53f;

	MoonLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	MoonLight->SetupAttachment(RootComponent);
	SetupDirectional(MoonLight, 1);
	MoonLight->ForwardShadingPriority = 0;
	MoonLight->LightSourceAngle = 0.52f;
	MoonLight->bCastCloudShadows = false;

	// planet top at the actor: put the actor at sea level
	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(RootComponent);
	Atmosphere->TransformMode = ESkyAtmosphereTransformMode::PlanetTopAtComponentTransform;
	// a damp coast: more haze than the engine's clear default
	Atmosphere->MieScatteringScale = 0.008f;
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
	// cheaper than the defaults: the club's forest already takes most of the frame
	Clouds->ViewSampleCountScale = 0.5f;
	Clouds->ShadowViewSampleCountScale = 0.5f;
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
	if (CloudMaterial)
	{
		Clouds->SetMaterial(CloudMaterial);
	}
	Dome->SetVisibility(SkyMaterial != nullptr);
	if (SkyMaterial && Dome->GetStaticMesh())
	{
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(SkyMaterial, this);
		Mid->SetScalarParameterValue(TEXT("StarBrightness"), StarBrightness);
		Dome->SetMaterial(0, Mid);
		const double R = FMath::Max(1.0, (double)Dome->GetStaticMesh()->GetBounds().SphereRadius);
		Dome->SetRelativeScale3D(FVector(DomeRadius / R));
	}
	SkyLight->SetIntensity(SkyLightIntensity);
	UpdateSky();
}

void ALyudSky::BeginPlay()
{
	Super::BeginPlay();
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
	const float MoonLux = FullMoonIlluminance * FMath::Pow((float)MoonPhase, 2.5f) * MoonUp;
	MoonLight->SetIntensity(MoonLux);
	MoonLight->SetLightColor(MoonColor);
	MoonLight->SetVisibility(MoonLux > 1e-4f);

	// one shadowed directional light at a time
	const bool bSunShadows = SunAlt > -1.f;
	SunLight->SetCastShadows(bSunShadows);
	MoonLight->SetCastShadows(!bSunShadows && Dark > 0.5f);
	SunLight->SetForwardShadingPriority(bSunShadows ? 1 : 0);
	MoonLight->SetForwardShadingPriority(bSunShadows ? 0 : 1);
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
		bOpen = C && C->LightingChannels.bChannel1;
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
	OutdoorExposure->BlendWeight = OutdoorWeight;
}

FString ALyudSky::DebugReport() const
{
	return FString::Printf(TEXT("LYUDSKY %s sun alt %.2f az %.2f | moon alt %.2f az %.2f phase %.2f | sun lux %.2f moon lux %.4f | outdoors %d exposure %.2f x %.2f"),
		*ShownTime().ToString(TEXT("%Y-%m-%d %H:%M")), SunPos.AltitudeDeg, SunPos.AzimuthDeg, MoonPos.AltitudeDeg,
		MoonPos.AzimuthDeg, MoonPhase, SunLight->Intensity, MoonLight->Intensity, bPlayerOutdoors ? 1 : 0,
		OutdoorExposure->Settings.AutoExposureBias, OutdoorWeight);
}
