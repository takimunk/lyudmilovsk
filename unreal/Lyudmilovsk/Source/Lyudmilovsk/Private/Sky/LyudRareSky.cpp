#include "Sky/LyudRareSky.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sky/LyudSky.h"
#include "UObject/ConstructorHelpers.h"
#include "Weather/LyudWeather.h"
#include "RareSkyModel.h"
#include "../Weather/WeatherModel.h"

namespace
{
	TAutoConsoleVariable<float> CVarKp(TEXT("lyud.Rare.Kp"), -1.f, TEXT("Force the geomagnetic Kp 0..9 (negative: the weather's)."));
	TAutoConsoleVariable<float> CVarAurora(TEXT("lyud.Rare.Aurora"), -1.f,
		TEXT("Force the aurora's strength at our latitude 0..1 (negative: the weather's, or from a forced Kp)."));
	TAutoConsoleVariable<float> CVarOnset(TEXT("lyud.Rare.AuroraOnset"), -1e9f,
		TEXT("Stage a substorm whose breakup was this many minutes ago (negative = before it; below -1e8: none)."));
	TAutoConsoleVariable<float> CVarHalo(TEXT("lyud.Rare.Halo"), -1.f, TEXT("Force the halo's chance 0..1 (negative: the weather's)."));
	TAutoConsoleVariable<float> CVarRainbow(TEXT("lyud.Rare.Rainbow"), -1.f, TEXT("Force the rainbow's chance 0..1 (negative: the weather's)."));
	TAutoConsoleVariable<float> CVarNlc(TEXT("lyud.Rare.Noctilucent"), -1.f, TEXT("Force the noctilucent clouds' chance 0..1 (negative: the weather's)."));
	TAutoConsoleVariable<float> CVarFlare(TEXT("lyud.Rare.Flare"), -1.f, TEXT("Force a flare on the star at this strength 0..1 (negative: the calendar's)."));
	TAutoConsoleVariable<int32> CVarDebug(TEXT("lyud.Rare.Debug"), 0, TEXT("1: print the rare sky on screen."));
	TAutoConsoleVariable<float> CVarStrikeHold(TEXT("lyud.Rare.StrikeHold"), -1.f,
		TEXT("Hold strikes at this age, seconds (tests: a screenshot of a stroke; negative: they run and fade)."));

	ALyudRareSky* FindRareSky(UWorld* World)
	{
		TActorIterator<ALyudRareSky> It(World);
		return It ? *It : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs StrikeCommand(TEXT("lyud.Rare.Strike"),
		TEXT("Lightning now: lyud.Rare.Strike [distance m, default 2000] [1 = to the ground (default), 0 = in the cloud] [bearing deg from the view, default random]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ALyudRareSky* Rare = FindRareSky(World);
			const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			if (!Rare || !PC || !PC->PlayerCameraManager)
			{
				return;
			}
			const float Dist = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 2000.f;
			const bool bGround = Args.Num() > 1 ? FCString::Atoi(*Args[1]) != 0 : true;
			const float Yaw = Args.Num() > 2 ? PC->PlayerCameraManager->GetCameraRotation().Yaw + FCString::Atof(*Args[2]) : FMath::FRandRange(0.f, 360.f);
			const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
			const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
			Rare->StrikeAt(Cam + Dir * Dist * 100.f, Dist, bGround);
		}));

	float Smooth(float A, float B, float X) { return FMath::SmoothStep(A, B, X); }

	/** The air's transmittance toward an altitude (rgb), as the sky material's (sky_hlsl.TRANS_HLSL). */
	FLinearColor Transmittance(float AltDeg)
	{
		const float H = FMath::Max(AltDeg, 0.f);
		const float Am = 1.f / (FMath::Sin(FMath::DegreesToRadians(H)) + 0.50572f * FMath::Pow(H + 6.07995f, -1.6364f));
		const float K = Smooth(-6.f, 0.5f, AltDeg);
		return FLinearColor(FMath::Exp(-0.07f * (Am - 1.f)) * K, FMath::Exp(-0.14f * (Am - 1.f)) * K, FMath::Exp(-0.30f * (Am - 1.f)) * K);
	}

	void SetupDome(UStaticMeshComponent* C)
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->bAffectDistanceFieldLighting = false;
		C->bAffectDynamicIndirectLighting = false;
		C->SetMobility(EComponentMobility::Movable);
		C->SetVisibility(false);
		C->bVisibleInReflectionCaptures = false;
		C->bVisibleInRealTimeSkyCaptures = false;
	}
}

ALyudRareSky::ALyudRareSky()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
	OpticsDome = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OpticsDome"));
	OpticsDome->SetupAttachment(RootComponent);
	SetupDome(OpticsDome);
	BoltDome = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoltDome"));
	BoltDome->SetupAttachment(RootComponent);
	SetupDome(BoltDome);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/EngineSky/SM_SkySphere.SM_SkySphere"));
	if (SphereMesh.Succeeded())
	{
		OpticsDome->SetStaticMesh(SphereMesh.Object);
		BoltDome->SetStaticMesh(SphereMesh.Object);
	}

	StrikeLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StrikeLight"));
	StrikeLight->SetupAttachment(RootComponent);
	StrikeLight->SetMobility(EComponentMobility::Movable);
	StrikeLight->SetIntensityUnits(ELightUnits::Candelas);
	StrikeLight->SetAttenuationRadius(1.2e6f);
	StrikeLight->SetCastShadows(false);
	StrikeLight->SetLightColor(FLinearColor(0.8f, 0.86f, 1.f));
	StrikeLight->SetSourceRadius(2000.f);
	StrikeLight->bUseInverseSquaredFalloff = true;
	// outdoors only, like the sun (ALyudSky)
	StrikeLight->LightingChannels.bChannel0 = false;
	StrikeLight->LightingChannels.bChannel1 = true;
	StrikeLight->SetVisibility(false);
}

void ALyudRareSky::BeginPlay()
{
	Super::BeginPlay();
	TActorIterator<ALyudSky> It(GetWorld());
	Sky = It ? *It : nullptr;
	// the two materials are built with the sky's (unreal/scripts/import_sky.py)
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Scene01/Sky/M_LyudSkyOptics.M_LyudSkyOptics")))
	{
		OpticsMID = UMaterialInstanceDynamic::Create(M, this);
		OpticsDome->SetMaterial(0, OpticsMID);
	}
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Scene01/Sky/M_LyudLightning.M_LyudLightning")))
	{
		BoltMID = UMaterialInstanceDynamic::Create(M, this);
		BoltDome->SetMaterial(0, BoltMID);
	}
	if (!OpticsMID || !BoltMID)
	{
		UE_LOG(LogTemp, Warning, TEXT("LyudRareSky: no M_LyudSkyOptics / M_LyudLightning under /Game/Scene01/Sky: run import_sky.py"));
	}
	if (const UStaticMesh* M = OpticsDome->GetStaticMesh())
	{
		SphereRadius = FMath::Max(1.f, M->GetBounds().SphereRadius);
	}
	if (ULyudWeatherSubsystem* W = ULyudWeatherSubsystem::Get(this))
	{
		W->OnLightning.AddDynamic(this, &ALyudRareSky::OnLightning);
	}
}

void ALyudRareSky::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ULyudWeatherSubsystem* W = ULyudWeatherSubsystem::Get(this))
	{
		W->OnLightning.RemoveDynamic(this, &ALyudRareSky::OnLightning);
	}
	Super::EndPlay(Reason);
}

void ALyudRareSky::OnLightning(FVector Location, float DistanceFromViewM)
{
	// most strikes flash inside the cloud; the near ones are more often seen reaching the ground
	const float Cg = CloudToGround * (DistanceFromViewM < 8000.f ? 1.f : 0.5f);
	StrikeAt(Location, DistanceFromViewM, FMath::FRand() < Cg);
}

void ALyudRareSky::StrikeAt(FVector Where, float DistanceM, bool bCloudToGround)
{
	FStrike S;
	// the ground under the strike (the sea where nothing is hit)
	FHitResult Hit;
	const FVector From(Where.X, Where.Y, Where.Z + 300000.f);
	const float SeaZ = Sky ? float(Sky->GetActorLocation().Z) : 0.f;
	S.Ground = GetWorld()->LineTraceSingleByChannel(Hit, From, FVector(Where.X, Where.Y, SeaZ - 1000.f), ECC_Visibility)
		? Hit.ImpactPoint : FVector(Where.X, Where.Y, SeaZ);
	S.Seed = FMath::FRandRange(0.f, 500.f);
	S.bGround = bCloudToGround;
	// the return strokes down the same channel, tens of milliseconds apart (in the cloud: a few flickering pulses)
	S.NumStrokes = bCloudToGround ? 1 + FMath::RandRange(0, 3) : 2 + FMath::RandRange(0, 3);
	float T = bCloudToGround ? 0.035f : 0.f;
	for (int32 I = 0; I < S.NumStrokes; ++I)
	{
		S.Strokes[I] = T;
		T += bCloudToGround ? FMath::FRandRange(0.04f, 0.12f) : FMath::FRandRange(0.06f, 0.2f);
	}
	Strikes.Add(S);
	if (Strikes.Num() > 4)
	{
		Strikes.RemoveAt(0);
	}
}

void ALyudRareSky::UpdateLightning(float DeltaSeconds, const FVector& Camera)
{
	float Flash = 0.f;
	const FStrike* Best = nullptr;
	float BestI = -1.f, BestBolt = 0.f, BestLeader = 1.f;
	for (int32 K = Strikes.Num() - 1; K >= 0; --K)
	{
		FStrike& S = Strikes[K];
		const float Hold = CVarStrikeHold.GetValueOnGameThread();
		S.Age = Hold >= 0.f ? Hold : S.Age + DeltaSeconds;
		float I = 0.f;
		for (int32 J = 0; J < S.NumStrokes; ++J)
		{
			const float Dt = S.Age - S.Strokes[J];
			I += Dt >= 0.f ? FMath::Exp(-Dt / (S.bGround ? 0.045f : 0.08f)) * (J == 0 ? 1.f : 0.75f) : 0.f;
		}
		const float Last = S.Strokes[S.NumStrokes - 1];
		if (S.Age > Last + 0.8f)
		{
			Strikes.RemoveAt(K);
			continue;
		}
		// the channel: a faint stepped leader creeping down, then lit by each stroke, glowing on briefly after
		const float Leader = S.bGround ? FMath::Clamp(S.Age / FMath::Max(S.Strokes[0], 1e-3f), 0.f, 1.f) : 1.f;
		const float Bolt = S.bGround ? FMath::Max(I, S.Age < S.Strokes[0] ? 0.06f : 0.12f * FMath::Exp(-(S.Age - Last) / 0.12f)) : 0.f;
		const float Dist = FVector::Dist(Camera, S.Ground);
		Flash += I * FMath::Clamp(300000.f / FMath::Max(Dist, 1.f), 0.1f, 1.5f) * (S.bGround ? 1.f : 0.6f);
		if (I + Bolt > BestI)
		{
			BestI = I + Bolt;
			Best = &S;
			BestBolt = Bolt;
			BestLeader = Leader;
		}
	}
	Now.Lightning = Flash;

	UMaterialInstanceDynamic* SkyMID = Sky ? Sky->GetSkyMaterialInstance() : nullptr;
	if (!Best)
	{
		StrikeLight->SetVisibility(false);
		BoltDome->SetVisibility(false);
		if (SkyMID)
		{
			SkyMID->SetVectorParameterValue(TEXT("Flash"), FLinearColor(0.f, 0.f, 1.f, 0.f));
		}
		return;
	}
	const FStrike& S = *Best;
	float I = 0.f;
	for (int32 J = 0; J < S.NumStrokes; ++J)
	{
		const float Dt = S.Age - S.Strokes[J];
		I += Dt >= 0.f ? FMath::Exp(-Dt / (S.bGround ? 0.045f : 0.08f)) : 0.f;
	}
	const FVector Light = S.Ground + FVector(0, 0, CloudBase * (S.bGround ? 0.55f : 1.25f));
	StrikeLight->SetWorldLocation(Light);
	StrikeLight->SetIntensity(StrikeCandela * I * (S.bGround ? 1.f : 0.45f));
	StrikeLight->SetVisibility(I > 0.003f);
	if (SkyMID)
	{
		const FVector Dir = (S.Ground + FVector(0, 0, CloudBase) - Camera).GetSafeNormal();
		SkyMID->SetVectorParameterValue(TEXT("Flash"), FLinearColor(float(Dir.X), float(Dir.Y), float(Dir.Z), Flash * SkyFlashGain));
	}
	if (BoltMID && S.bGround)
	{
		const float H = float(FVector::Dist2D(Camera, S.Ground));
		BoltDome->SetWorldLocation(Camera);
		BoltDome->SetWorldScale3D(FVector(FMath::Max(H * 0.92f, 3000.f) / SphereRadius));
		BoltMID->SetVectorParameterValue(TEXT("Bolt"), FLinearColor(float(S.Ground.X), float(S.Ground.Y), float(S.Ground.Z), S.Seed));
		BoltMID->SetVectorParameterValue(TEXT("BoltLook"), FLinearColor(CloudBase, BestBolt * BoltGain, 1.f, BestLeader));
		BoltDome->SetVisibility(BestBolt > 0.002f);
	}
	else
	{
		BoltDome->SetVisibility(false);
	}
}

void ALyudRareSky::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Sky)
	{
		return;
	}
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const FVector Camera = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;

	// what the weather says
	LyudRareSky::FInputs In;
	In.Hours = (Sky->ShownTime() - FDateTime(2000, 1, 1)).GetTotalHours();
	if (const ULyudWeatherSubsystem* W = ULyudWeatherSubsystem::Get(this))
	{
		const LyudWeather::FWeatherState& Wx = W->GetModelState();
		In.Kp = Wx.Kp;
		In.Aurora = Wx.Aurora;
		In.Halo = Wx.Halo;
		In.Rainbow = Wx.Rainbow;
		In.Noctilucent = Wx.Noctilucent;
		In.CloudCover = Wx.CloudCover;
		In.HighCover = Wx.High.Cover;
		In.HighDensity = Wx.High.Density;
		In.PrecipRate = Wx.PrecipRate;
		In.ShowerCells = Wx.ShowerCells;
	}
	const float FKp = CVarKp.GetValueOnGameThread();
	if (FKp >= 0.f)
	{
		In.Kp = FKp;
		// at our latitude (52 magnetic) the aurora starts to show low in the north at Kp ~5
		In.Aurora = FMath::SmoothStep(3.5f, 6.5f, FKp);
	}
	auto Force = [](float& V, const TAutoConsoleVariable<float>& C) { if (C.GetValueOnGameThread() >= 0.f) { V = C.GetValueOnGameThread(); } };
	Force(In.Aurora, CVarAurora);
	Force(In.Halo, CVarHalo);
	Force(In.Rainbow, CVarRainbow);
	Force(In.Noctilucent, CVarNlc);
	In.ForceOnsetMinutes = CVarOnset.GetValueOnGameThread();
	In.SunAltDeg = Sky->SunAltitude();
	In.MoonAltDeg = Sky->MoonAltitude();
	In.MoonPhase = Sky->MoonLitFraction();
	LyudRareSky::FState S = LyudRareSky::Evaluate(In);
	if (CVarFlare.GetValueOnGameThread() >= 0.f)
	{
		S.Flare.Strength = CVarFlare.GetValueOnGameThread();
		S.Flare.Peak = FMath::Max(S.Flare.Strength, 0.6f);
		S.Flare.Lat = 0.3f;
		S.Flare.Lon = -0.35f;
	}

	Now.Kp = In.Kp;
	Now.Aurora = S.Aurora.Visible;
	Now.AuroraPhase = ANSI_TO_TCHAR(LyudRareSky::PhaseName(S.Aurora.Phase));
	Now.AuroraEdgeKm = S.Aurora.EdgeKm;
	Now.Halo = S.Halo.Strength;
	Now.SunDogs = S.Halo.SunDogs;
	Now.Rainbow = S.Rainbow.Strength;
	Now.Noctilucent = S.Noctilucent;
	Now.Flare = S.Flare.Strength;

	// the dome: the aurora, noctilucent clouds, the flare
	if (UMaterialInstanceDynamic* M = Sky->GetSkyMaterialInstance())
	{
		const LyudRareSky::FAurora& A = S.Aurora;
		const FVector MagN = Sky->BearingDirection(LyudRareSky::FSite().PoleBearingDeg);
		M->SetVectorParameterValue(TEXT("AurA"), FLinearColor(A.Visible * AuroraGain, A.EdgeKm, A.SpreadKm, A.Activity));
		M->SetVectorParameterValue(TEXT("AurB"), FLinearColor(A.Pulsate, A.Red, A.Fringe, A.LowerKm));
		M->SetVectorParameterValue(TEXT("AurC"), FLinearColor(float(MagN.X), float(MagN.Y), A.Seed, AuroraFolding));
		M->SetVectorParameterValue(TEXT("NlcA"), FLinearColor(S.Noctilucent * NoctilucentGain, float(FMath::Fmod(FMath::FloorToDouble(In.Hours / 24.0) * 0.731, 50.0)), 0.f, 0.f));
		M->SetVectorParameterValue(TEXT("Flare"), FLinearColor(S.Flare.Lat, S.Flare.Lon, S.Flare.Strength, S.Flare.Peak));
	}
	Sky->FlareBoost = S.Flare.Strength;

	// the optics round the camera: the sun's, or Europa's by night
	const bool bMoon = S.Halo.bMoon || S.Rainbow.bMoon;
	const bool bOptics = OpticsMID && (S.Halo.Strength > 0.002f || S.Rainbow.Strength > 0.002f);
	OpticsDome->SetVisibility(bOptics);
	if (bOptics)
	{
		OpticsDome->SetWorldLocation(Camera);
		OpticsDome->SetWorldScale3D(FVector(OpticsRadius / SphereRadius));
		const FVector L = bMoon ? Sky->MoonDirection() : Sky->SunDirection();
		const float AltDeg = bMoon ? Sky->MoonAltitude() : Sky->SunAltitude();
		const float RadiusDeg = 0.5f * (bMoon ? Sky->MoonAngularDiameterDeg : Sky->SunAngularDiameterDeg);
		// the light's brightness: the sun's at the day exposure; Europa's (the phase dims it fast) at the night one
		const float Lum = bMoon ? Sky->FullMoonIlluminance * FMath::Pow(Sky->MoonLitFraction(), 2.5f) / FMath::Max(Sky->SunIlluminance, 1e-3f) * 4.f : 1.f;
		const FLinearColor Tr = Transmittance(AltDeg);
		OpticsMID->SetVectorParameterValue(TEXT("Light"), FLinearColor(float(L.X), float(L.Y), float(L.Z), FMath::DegreesToRadians(RadiusDeg * OpticsDiskScale)));
		OpticsMID->SetVectorParameterValue(TEXT("LightColor"), Tr * (Lum * OpticsGain));
		OpticsMID->SetVectorParameterValue(TEXT("Halo"), FLinearColor(S.Halo.Strength, S.Halo.SunDogs, S.Halo.Pillar, S.Halo.Ring46));
		OpticsMID->SetVectorParameterValue(TEXT("Rainbow"), FLinearColor(S.Rainbow.Strength, S.Rainbow.Rain, float(FMath::Fmod(FMath::FloorToDouble(In.Hours * 3.0) * 1.37, 60.0)), 0.f));
	}

	UpdateLightning(DeltaSeconds, Camera);

	if (CVarDebug.GetValueOnGameThread() > 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(int32(0x7A2E5), 0.f, FColor::Emerald, DebugReport());
	}
}

FString ALyudRareSky::DebugReport() const
{
	return FString::Printf(TEXT("RARESKY kp %.1f aurora %.2f %s edge %.0f km | halo %.2f dogs %.2f | rainbow %.2f | nlc %.2f | flare %.2f | lightning %.2f"),
		Now.Kp, Now.Aurora, *Now.AuroraPhase, Now.AuroraEdgeKm, Now.Halo, Now.SunDogs, Now.Rainbow, Now.Noctilucent, Now.Flare, Now.Lightning);
}

void ULyudRareSkySubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	if (!World.IsGameWorld() || FindRareSky(&World))
	{
		return;
	}
	if (TActorIterator<ALyudSky>(&World))
	{
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		World.SpawnActor<ALyudRareSky>(ALyudRareSky::StaticClass(), FTransform::Identity, P);
	}
}
