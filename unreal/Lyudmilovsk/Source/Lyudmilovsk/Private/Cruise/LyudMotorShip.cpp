#include "Cruise/LyudMotorShip.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Cruise/LyudCruise.h"
#include "Cruise/LyudShipWake.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "../Transport/LyudMeshKit.h"
#include "ShipSeaMath.h"
#include "Sea/LyudSea.h"
#include "Sky/LyudSky.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Weather/LyudRainFog.h"
#include "Weather/LyudWind.h"
#include "World/LyudCityBuilding.h"

namespace
{
const FName ShipPlot(TEXT("P-SHIP"));
constexpr float CableCm = 4500.f;          // from the stem to where her anchor holds: she swings about that point
constexpr float ShownWithinCm = 2.0e6f;    // 20 km: beyond it the haze has her anyway
}

ALyudMotorShip::ALyudMotorShip()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
}

void ALyudMotorShip::BeginPlay()
{
	Super::BeginPlay();
	Wake = NewObject<ULyudShipWake>(this, TEXT("Wake"));
	Wake->SetupAttachment(RootComponent);
	Wake->RegisterComponent();
	AddInstanceComponent(Wake);
	// seen from the promenade two kilometres off: bigger foam, made farther from the camera
	Wake->NearCm = 300000.f;
	Wake->Amount = 2.f;
	// the horn carries over the town: full within 300 m, gone by 7 km
	HornAtten = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& A = HornAtten->Attenuation;
	A.bAttenuate = true;
	A.bSpatialize = true;
	A.AttenuationShape = EAttenuationShape::Sphere;
	A.AttenuationShapeExtents = FVector(30000.f, 0.f, 0.f);
	A.FalloffDistance = 700000.f;
	A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	A.bAttenuateWithLPF = true;
	A.LPFRadiusMin = 60000.f;
	A.LPFRadiusMax = 700000.f;
	HornOne = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Ship/S_ship_horn_01.S_ship_horn_01"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	HornThree = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Ship/S_ship_horn3_01.S_ship_horn3_01"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	MakeLamps();
	Show(false);
}

void ALyudMotorShip::MakeStandIn()
{
	// a plain white ship of her size: hull, two tiers of deckhouse, a funnel, a mast (ship frame: x from the stern)
	FLyudMeshKit K;
	const FTransform I = FTransform::Identity;
	K.BoxIn(TEXT("hull"), I, FVector(2900.f, 0.f, 60.f), FVector(5800.f, 1000.f, 440.f), false);
	K.BoxIn(TEXT("hull"), I, FVector(5950.f, 0.f, 120.f), FVector(500.f, 520.f, 320.f), false);
	K.BoxIn(TEXT("house"), I, FVector(3000.f, 0.f, 410.f), FVector(4300.f, 860.f, 260.f), false);
	K.BoxIn(TEXT("house"), I, FVector(3300.f, 0.f, 670.f), FVector(3000.f, 760.f, 250.f), false);
	K.BoxIn(TEXT("house"), I, FVector(4500.f, 0.f, 910.f), FVector(700.f, 640.f, 230.f), false);
	K.BoxIn(TEXT("funnel"), I, FVector(2350.f, 0.f, 1070.f), FVector(380.f, 260.f, 540.f), false);
	K.BoxIn(TEXT("hull"), I, FVector(4300.f, 0.f, 1300.f), FVector(16.f, 16.f, 560.f), false);
	TArray<FName> Slots;
	StandIn = K.Build(this, TEXT("MotorShipStandIn"), Slots);
	if (!StandIn)
	{
		return;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, TEXT("StandInHull"));
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(StandIn);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->bAffectDynamicIndirectLighting = false;
	C->bAffectDistanceFieldLighting = false;
	if (UMaterialInterface* Paint = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Transport/Materials/M_TR_Paint.M_TR_Paint"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		for (int32 S = 0; S < Slots.Num(); S++)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Paint, this);
			const bool bFunnel = Slots[S] == TEXT("funnel");
			M->SetVectorParameterValue(TEXT("Color"), bFunnel ? FLinearColor(0.75f, 0.55f, 0.1f) : (Slots[S] == TEXT("hull") ? FLinearColor(0.82f, 0.83f, 0.82f) : FLinearColor(0.9f, 0.9f, 0.86f)));
			M->SetScalarParameterValue(TEXT("Roughness"), 0.6f);
			C->SetMaterial(S, M);
		}
	}
	C->SetGenerateOverlapEvents(false);
	C->SetCanEverAffectNavigation(false);
	C->SetupAttachment(RootComponent);
	C->RegisterComponent();
	AddInstanceComponent(C);
	// as the ship is now (hidden while the hero is aboard or she is laid up)
	C->SetVisibility(bShown);
	Parts.Add(C);
	bStandIn = true;
}

bool ALyudMotorShip::Dress()
{
	const ULyudCruise* Cruise = ULyudCruise::Get(this);
	if (!Cruise)
	{
		return false;
	}
	const ALyudCityBuilding* Stage = nullptr;
	for (TActorIterator<ALyudCityBuilding> It(GetWorld()); It; ++It)
	{
		if (It->PlotId == ShipPlot)
		{
			Stage = *It;
			break;
		}
	}
	if (!Stage || !Stage->Shell || !Stage->Shell->GetStaticMesh())
	{
		if (!bStandIn && Parts.Num() == 0)
		{
			MakeStandIn();
		}
		return false;
	}
	for (UStaticMeshComponent* Old : Parts)
	{
		if (Old)
		{
			Old->DestroyComponent();
		}
	}
	Parts.Reset();
	bStandIn = false;
	// the building's frame in the ship's frame: the same pieces, placed as they stand at the stage
	const FTransform ShipToWorld = Cruise->StageTransform();
	int32 N = 0;
	for (const UStaticMeshComponent* Src : { Stage->Shell.Get(), Stage->Detail.Get(), Stage->Windows.Get() })
	{
		if (!Src || !Src->GetStaticMesh())
		{
			continue;
		}
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("ShipPart%d"), N++));
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Src->GetStaticMesh());
		for (int32 M = 0; M < Src->GetNumMaterials(); M++)
		{
			C->SetMaterial(M, Src->GetMaterial(M));
		}
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(Src == Stage->Shell.Get());
		// she swings and sails: out of the Lumen scene and the distance fields
		C->bAffectDynamicIndirectLighting = false;
		C->bAffectDistanceFieldLighting = false;
		C->SetupAttachment(RootComponent);
		C->RegisterComponent();
		C->SetRelativeTransform(Src->GetComponentTransform().GetRelativeTransform(ShipToWorld));
		AddInstanceComponent(C);
		C->SetVisibility(bShown);
		Parts.Add(C);
	}
	return Parts.Num() > 0;
}

void ALyudMotorShip::MakeLamps()
{
	// the anchor light forward, the masthead light, the stern light: small unlit balls (no light components)
	UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Transport/Materials/M_TR_Glow.M_TR_Glow"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Ball)
	{
		return;
	}
	const FVector At[] = { FVector(5900.f, 0.f, 1050.f), FVector(4300.f, 0.f, 1620.f), FVector(60.f, 0.f, 620.f) };
	for (int32 I = 0; I < 3; I++)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("ShipLamp%d"), I));
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Ball);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->bAffectDynamicIndirectLighting = false;
		C->bAffectDistanceFieldLighting = false;
		if (Glow)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Glow, this);
			M->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.95f, 0.85f));
			M->SetScalarParameterValue(TEXT("Intensity"), 0.f);
			C->SetMaterial(0, M);
			LampMids.Add(M);
		}
		C->SetupAttachment(RootComponent);
		C->RegisterComponent();
		C->SetRelativeLocation(At[I]);
		C->SetRelativeScale3D(FVector(0.5f));
		C->SetVisibility(false);
		AddInstanceComponent(C);
		Lamps.Add(C);
	}
}

void ALyudMotorShip::Show(bool bOn)
{
	bShown = bOn;
	for (UStaticMeshComponent* C : Parts)
	{
		if (C)
		{
			C->SetVisibility(bOn);
		}
	}
	if (!bOn)
	{
		for (UStaticMeshComponent* C : Lamps)
		{
			C->SetVisibility(false);
		}
		LampGlow = -1.f;
	}
	// nothing to do while she is away or laid up: look twice a second
	SetActorTickInterval(bOn ? 0.f : 0.5f);
}

void ALyudMotorShip::Horn(int32 Blasts)
{
	USoundBase* S = Blasts >= 3 ? HornThree.Get() : HornOne.Get();
	if (S && bShown)
	{
		UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation() + FVector(0.0, 0.0, 1300.0), 1.f, 1.f, 0.f, HornAtten);
	}
}

void ALyudMotorShip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ULyudCruise* Cruise = ULyudCruise::Get(this);
	if (!Cruise || !Cruise->IsRunning())
	{
		if (bShown)
		{
			Show(false);
		}
		return;
	}
	if (!bDressed)
	{
		// the stage ship's building may come into the level after this actor: look for it now and then
		DressClock -= FMath::Max(DeltaSeconds, 0.016f);
		if (DressClock <= 0.f)
		{
			DressClock = 3.f;
			bDressed = Dress();
		}
	}
	const FLyudCruiseView& V = Cruise->View();
	if (!Sea.IsValid())
	{
		Sea = ALyudSea::Get(this);
	}
	const FTransform Roads = Cruise->RoadsTransform();
	const bool bAtRoads = V.Phase == ELyudCruisePhase::Port || V.Phase == ELyudCruisePhase::Boarding || V.Phase == ELyudCruisePhase::Landing;
	const bool bAway = V.Phase == ELyudCruisePhase::Underway || V.Phase == ELyudCruisePhase::Circling || V.Phase == ELyudCruisePhase::Anchored;
	FVector Cam = Roads.GetLocation();
	if (const APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		Cam = PCM->GetCameraLocation();
	}
	// where she is: on the roads, or on the chart (the chart is the world: cm = m x 100, the map's Y origin as the roads')
	FVector Mid;
	float Yaw;
	FVector Velocity = FVector::ZeroVector;
	const float Level = Sea.IsValid() ? Sea->SeaLevel() : float(Roads.GetLocation().Z);
	if (bAway)
	{
		const FVector RoadsMid = Roads.TransformPosition(FVector(LengthCm * 0.5f, 0.f, 0.f));
		// the roads' own chart point fixes the frame's offset (790 cm in Y), whatever the model calls "home"
		Mid = FVector(V.ChartPosM.X * 100.0, V.ChartPosM.Y * 100.0 + 790.0, RoadsMid.Z);
		Yaw = FMath::RadiansToDegrees(V.HeadingRad);
		Velocity = FVector(FMath::Cos(V.HeadingRad), FMath::Sin(V.HeadingRad), 0.f) * V.SpeedMS * 100.f;
		bSwingSet = false;
	}
	else
	{
		// at anchor: head to wind, turning no faster than a degree a second, yawing a little about it
		const float RoadsYaw = Roads.GetRotation().Rotator().Yaw;
		float Want = RoadsYaw;
		if (const ULyudWindSubsystem* Wind = ULyudWindSubsystem::Get(this))
		{
			const FVector W = Wind->GetWindAt(Roads.GetLocation(), 10.f, true).Velocity;
			if (W.SizeSquared2D() > 1.f)
			{
				Want = FMath::RadiansToDegrees(float(FMath::Atan2(-W.Y, -W.X)));
			}
		}
		const float T = GetWorld()->GetTimeSeconds();
		Want += 7.f * FMath::Sin(T * 0.045f) + 2.5f * FMath::Sin(T * 0.13f + 1.f);
		if (!bSwingSet)
		{
			SwingYaw = Want;
			bSwingSet = true;
		}
		SwingYaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(SwingYaw, Want), -1.f, 1.f) * FMath::Min(DeltaSeconds, 0.5f);
		Yaw = SwingYaw;
		// she hangs back from her anchor, which lies ahead of where the roads' transform puts her stem
		const FVector Anchor = Roads.TransformPosition(FVector(LengthCm + CableCm, 0.f, 0.f));
		const FVector Fwd = FRotator(0.f, Yaw, 0.f).Vector();
		Mid = Anchor - Fwd * (CableCm + LengthCm * 0.5f);
	}
	float Reach = ShownWithinCm;
	if (const ULyudRainFogSubsystem* Fog = ULyudRainFogSubsystem::Get(this))
	{
		Reach = FMath::Min(Reach, Fog->ReadWeather().VisibilityM * 140.f);
	}
	const bool bWant = (bAtRoads || bAway) && !V.bHeroAboard && FVector::Dist2D(Mid, Cam) < Reach;
	if (bWant != bShown)
	{
		Show(bWant);
	}
	if (!bShown)
	{
		return;
	}
	// afloat: on the tide, lifting and rolling a little with the sea
	// (the level is the sea's now, tide and all; the ride is a small share of the surface at her bow, stern and sides,
	// read five times a second and eased every frame: LyudShipSea::RideFrom, within 20 cm, 0.8 and 1.5 degrees)
	RideClock -= DeltaSeconds;
	if (RideClock <= 0.f && Sea.IsValid())
	{
		RideClock = 0.2f;
		const FRotator Flat(0.f, Yaw, 0.f);
		const FVector Fwd = Flat.Vector() * (LengthCm * 0.4f), Side = Flat.RotateVector(FVector::RightVector) * 450.f;
		const FVector Here(Mid.X, Mid.Y, Level);
		const LyudShipSea::FRide R = LyudShipSea::RideFrom(Sea->SurfaceZ(Here + Fwd) - Level, Sea->SurfaceZ(Here - Fwd) - Level,
			Sea->SurfaceZ(Here - Side) - Level, Sea->SurfaceZ(Here + Side) - Level, LengthCm * 0.8, 900.0);
		RideTo = FVector(R.HeaveCm, R.PitchDeg, R.RollDeg);
	}
	RideNow += (RideTo - RideNow) * (1.0 - FMath::Exp(-double(FMath::Min(DeltaSeconds, 0.5f)) / 1.5));
	const FRotator Rot(float(RideNow.Y), Yaw, float(RideNow.Z));
	const FVector Origin = FVector(Mid.X, Mid.Y, Level + RideNow.X) - Rot.RotateVector(FVector(LengthCm * 0.5f, 0.f, 0.f));
	SetActorLocationAndRotation(Origin, Rot);
	if (Wake)
	{
		Wake->Feed(GetActorTransform(), Velocity, FVector::ZeroVector, DeltaSeconds);
	}
	// lamps: the anchor light at anchor, the masthead and stern lights under way, from dusk to dawn
	if (!Sky.IsValid())
	{
		TActorIterator<ALyudSky> It(GetWorld());
		if (It)
		{
			Sky = *It;
		}
	}
	const float Alt = Sky.IsValid() ? Sky->SunAltitude() : 20.f;
	const float Dark = FMath::Clamp((4.f - Alt) / 8.f, 0.f, 1.f);
	const float Code = Dark + (bAway && V.SpeedMS > 0.2f ? 10.f : 0.f);
	if (FMath::Abs(Code - LampGlow) > 0.03f)
	{
		LampGlow = Code;
		const bool bWay = Code >= 10.f;
		for (int32 I = 0; I < Lamps.Num(); I++)
		{
			const bool bOn = Dark > 0.1f && (I == 0 ? !bWay : bWay);
			Lamps[I]->SetVisibility(bOn);
			if (LampMids.IsValidIndex(I))
			{
				LampMids[I]->SetScalarParameterValue(TEXT("Intensity"), bOn ? 60.f * Dark : 0.f);
			}
		}
	}
	// seen from the shore two kilometres off a lamp must not be smaller than a pixel
	const float LampScale = FMath::Max(0.5f, float(FVector::Dist(Origin, Cam)) * 0.000011f);
	for (UStaticMeshComponent* L : Lamps)
	{
		if (L->IsVisible())
		{
			L->SetRelativeScale3D(FVector(LampScale));
		}
	}
}

FString ALyudMotorShip::Report() const
{
	const FVector At = GetActorLocation();
	return FString::Printf(TEXT("motor ship: %s, %s, at map %.0f %.0f, yaw %.0f, wake foam %d"), bShown ? TEXT("shown") : TEXT("hidden"),
		bStandIn ? TEXT("stand-in hull") : (bDressed ? TEXT("the stage ship's meshes") : TEXT("not dressed")), At.X * 0.01, (At.Y - 790.0) * 0.01,
		GetActorRotation().Yaw, Wake ? Wake->NumFoam() : 0);
}
