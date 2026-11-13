#include "North/LyudCampfire.h"

#include "../Transport/LyudMeshKit.h"
#include "Audio/LyudSoundscape.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "North/LyudNorthFx.h"
#include "North/NorthModel.h"
#include "Plants/GameCalendar.h"
#include "Sky/LyudSky.h"

namespace
{
	const TCHAR* FirePaintPath = TEXT("/Game/Transport/Materials/M_TR_Paint.M_TR_Paint");
	const TCHAR* FireGlowPath = TEXT("/Game/Transport/Materials/M_TR_Glow.M_TR_Glow");
	constexpr int32 FireEmbers = 8;
	/** The hero lights or puts out the fire within this distance of it (cm, flat). */
	constexpr float FireReach = 260.f;
}

ALyudCampfire::ALyudCampfire()
{
	PrimaryActorTick.TickInterval = 0.05f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ALyudCampfire::BeginPlay()
{
	Build();
	Super::BeginPlay();
}

void ALyudCampfire::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetPrompt(false);
	if (Crackle)
	{
		Crackle->bAutoDestroy = true;
		Crackle->FadeOut(0.5f, 0.f);
		Crackle = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void ALyudCampfire::Build()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	UMaterialInterface* Paint = LoadObject<UMaterialInterface>(nullptr, FirePaintPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, FireGlowPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	auto Add = [this](UStaticMesh* Mesh, UMaterialInterface* M, bool bMoving)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(Mesh);
		if (bMoving)
		{
			LyudNorthFx::Lightweight(C);
		}
		else
		{
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetGenerateOverlapEvents(false);
		}
		C->SetLightingChannels(true, true, false);
		if (M)
		{
			for (int32 I = 0; I < C->GetNumMaterials(); I++) C->SetMaterial(I, M);
		}
		C->SetupAttachment(RootComponent);
		C->RegisterComponent();
		return C;
	};
	TArray<FName> Slots;
	// six charred logs leaning together (the ring of stones is the imported SM_North_Fire_ring)
	FLyudMeshKit L;
	for (int32 I = 0; I < 6; I++)
	{
		const FVector D = FRotator(0.f, I * 60.f + 15.f, 0.f).Vector();
		L.Rod(TEXT("wood"), D * 48.f + FVector(0.f, 0.f, 4.f), D * 6.f + FVector(0.f, 0.f, 52.f), 5.f, 6, true, false);
	}
	CharMat = Paint ? UMaterialInstanceDynamic::Create(Paint, this) : nullptr;
	if (CharMat)
	{
		CharMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.09f, 0.055f, 0.035f));
		CharMat->SetScalarParameterValue(TEXT("Roughness"), 0.92f);
	}
	Logs = Add(L.Build(this, TEXT("CampfireLogs"), Slots), CharMat, false);
	// three tongues of flame, each flickering by its own clock (as the festival bonfire's)
	FlameMat = Glow ? UMaterialInstanceDynamic::Create(Glow, this) : nullptr;
	if (FlameMat)
	{
		FlameMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.42f, 0.08f));
		FlameMat->SetScalarParameterValue(TEXT("Intensity"), 18.f);
	}
	for (int32 I = 0; I < 3; I++)
	{
		FLyudMeshKit F;
		const float H = 50.f + I * 16.f, R = 24.f - I * 4.f;
		const TArray<FVector2D> P = { { 0.f, 0.f }, { R, 8.f }, { R * 0.55f, H * 0.55f }, { 0.f, H } };
		const TArray<FName> M = { TEXT("glow"), TEXT("glow"), TEXT("glow") };
		F.Lathe(FTransform(FRotator(0.f, I * 40.f, 0.f), FVector(FMath::Cos(I * 2.1f) * 6.f, FMath::Sin(I * 2.1f) * 6.f, 6.f)), P, M, 7, false, false);
		UStaticMeshComponent* C = Add(F.Build(this, *FString::Printf(TEXT("CampfireFlame%d"), I), Slots), FlameMat, true);
		C->SetVisibility(false);
		Flames.Add(C);
	}
	// embers: tiny glowing bits rising off the fire
	EmberMat = Glow ? UMaterialInstanceDynamic::Create(Glow, this) : nullptr;
	if (EmberMat)
	{
		EmberMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.35f, 0.05f));
		EmberMat->SetScalarParameterValue(TEXT("Intensity"), 40.f);
	}
	FLyudMeshKit E;
	E.Box(TEXT("glow"), FTransform::Identity, FVector(0.6f, 0.6f, 0.6f), false);
	UStaticMesh* Bit = E.Build(this, TEXT("CampfireEmber"), Slots);
	for (int32 I = 0; I < FireEmbers; I++)
	{
		UStaticMeshComponent* C = Add(Bit, EmberMat, true);
		C->SetVisibility(false);
		EmberParts.Add(C);
	}
	Embers.SetNum(FireEmbers);
	// the light: warm, flickering, no shadows (perf checklist)
	Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(RootComponent);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(0.f);
	Light->SetLightColor(FLinearColor(1.f, 0.55f, 0.25f));
	Light->SetAttenuationRadius(1800.f);
	Light->SetCastShadows(false);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	Light->SetVisibility(false);
	Light->RegisterComponent();
	AddInstanceComponent(Light);
}

bool ALyudCampfire::Scheduled()
{
	float SunAlt = 10.f;
	FDateTime Now = UGameCalendar::NowIn(GetWorld());
	if (!Sky.IsValid())
	{
		TActorIterator<ALyudSky> It(GetWorld());
		Sky = It ? *It : nullptr;
	}
	if (const ALyudSky* S = Sky.Get())
	{
		SunAlt = S->SunAltitude();
		Now = S->ShownTime();                          // the hour the sky shows, so the fire and the dusk agree
	}
	else
	{
		// no sky: by the clock (8 pm .. 5 am counts as dark)
		const float H = Now.GetHour() + Now.GetMinute() / 60.f;
		SunAlt = (H >= 20.f || H < 5.f) ? -5.f : 10.f;
	}
	const float Hour = Now.GetHour() + Now.GetMinute() / 60.f;
	return LyudNorthModel::FireScheduled(bScheduled, SunAlt, Hour, LightSunAltitude, OutHour);
}

void ALyudCampfire::Toggle()
{
	Forced = -1;
	bHeroChose = true;
	bHeroLit = !bLit;
	bLit = bHeroLit;
	UE_LOG(LogTemp, Display, TEXT("NORTH campfire %s by the hero"), bLit ? TEXT("lit") : TEXT("put out"));
}

void ALyudCampfire::SetForced(int32 Mode)
{
	Forced = Mode;
	if (Mode < 0)
	{
		bHeroChose = false;
	}
	// wake it up enough to show the change at once when the player is here
	ScheduleTimer = 0.f;
}

void ALyudCampfire::SetAwake(bool bInAwake, bool bInNear)
{
	const bool bWasAwake = bAwake;
	Super::SetAwake(bInAwake, bInNear);
	// near: every frame (E is polled); else the flames' 20 Hz is enough
	SetActorTickInterval(bNear ? 0.f : 0.05f);
	if (bAwake && !bWasAwake)
	{
		// arriving at night finds it burning already: the schedule now, the fire at its level
		ScheduleTimer = 0.f;
		bScheduled = Scheduled();
		const bool bWant = Forced >= 0 ? Forced == 1 : (bHeroChose ? bHeroLit : bScheduled);
		bLit = bWant;
		Burn = bWant ? 1.f : 0.f;
	}
	if (!bAwake)
	{
		ShowFire(false);
		SetPrompt(false);
		for (FEmber& Em : Embers)
		{
			Em.Life = -1.f;
		}
		for (UStaticMeshComponent* C : EmberParts)
		{
			C->SetVisibility(false);
		}
	}
	if (!bNear && Crackle)
	{
		Crackle->bAutoDestroy = true;
		Crackle->FadeOut(1.f, 0.f);
		Crackle = nullptr;
	}
}

void ALyudCampfire::ShowFire(bool bOn)
{
	if (bOn == bFireShown)
	{
		return;
	}
	bFireShown = bOn;
	for (UStaticMeshComponent* C : Flames)
	{
		C->SetVisibility(bOn);
	}
	if (Light)
	{
		Light->SetVisibility(bOn);
		if (!bOn)
		{
			Light->SetIntensity(0.f);
		}
	}
}

void ALyudCampfire::SetPrompt(bool bOn)
{
	if (bOn == bPrompt)
	{
		return;
	}
	bPrompt = bOn;
	bPromptLit = bLit;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, bOn ? LyudText::Get(bLit ? TEXT("ui.north.fire_out") : TEXT("ui.north.fire_light")) : FText::GetEmpty());
	}
}

void ALyudCampfire::UpdatePrompt()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const ACharacter* Hero = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	bool bShow = false;
	if (Hero && !(UI && UI->IsOpen()))
	{
		const FVector Me = GetActorLocation();
		const FVector Feet = Hero->GetActorLocation();
		if (FVector::Dist2D(Me, Feet) < FireReach && FMath::Abs(Feet.Z - Me.Z) < 250.f)
		{
			// looking down at it, roughly
			const FVector Eye = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : Hero->GetPawnViewLocation();
			bShow = FVector::DotProduct((Me + FVector(0.f, 0.f, 30.f) - Eye).GetSafeNormal(), PC->GetControlRotation().Vector()) > 0.55f;
		}
	}
	if (bShow && bPrompt && bPromptLit != bLit)
	{
		// the words follow the fire (lit / out) while the prompt stays up
		bPromptLit = bLit;
		if (ULyudInventoryUI* Ui = ULyudInventoryUI::Get(this))
		{
			Ui->SetPrompt(this, LyudText::Get(bLit ? TEXT("ui.north.fire_out") : TEXT("ui.north.fire_light")));
		}
	}
	SetPrompt(bShow);
	// polled, never consumed: E belongs to several things at once
	if (bShow && PC->WasInputKeyJustPressed(EKeys::E))
	{
		Toggle();
	}
}

void ALyudCampfire::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Clock += DeltaTime;
	ScheduleTimer -= DeltaTime;
	if (ScheduleTimer <= 0.f)
	{
		ScheduleTimer = 2.f;
		const bool bNow = Scheduled();
		if (bNow != bScheduled)
		{
			// the evening came or the night ended: the hero's last choice gives way to the schedule
			bScheduled = bNow;
			bHeroChose = false;
		}
	}
	bLit = Forced >= 0 ? Forced == 1 : (bHeroChose ? bHeroLit : bScheduled);
	// it takes a few seconds to catch and longer to die down
	Burn = FMath::Clamp(Burn + DeltaTime * (bLit ? 1.f / 3.f : -1.f / 6.f), 0.f, 1.f);
	ShowFire(Burn > 0.01f);
	if (bNear)
	{
		UpdatePrompt();
	}
	else
	{
		SetPrompt(false);
	}
	// the crackle while near and burning
	if (bNear && Burn > 0.2f && !Crackle)
	{
		if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this))
		{
			Crackle = S->AttachLoop(TEXT("north_campfire"), RootComponent, 0.7f);
		}
	}
	else if (Crackle && (!bNear || Burn < 0.1f))
	{
		Crackle->bAutoDestroy = true;
		Crackle->FadeOut(1.5f, 0.f);
		Crackle = nullptr;
	}
	if (!bFireShown)
	{
		return;
	}
	for (int32 I = 0; I < Flames.Num(); I++)
	{
		const float F = 0.8f + 0.2f * FMath::Sin(Clock * (8.f + I * 2.3f) + I) * FMath::Sin(Clock * (3.3f + I) + I * 2.f);
		Flames[I]->SetRelativeScale3D(FVector(0.85f + 0.15f * F * Burn, 0.85f + 0.15f * F * Burn, F * Burn));
	}
	const float Flick = 0.78f + 0.22f * FMath::Sin(Clock * 11.f) * FMath::Sin(Clock * 4.3f) + 0.05f * FMath::Sin(Clock * 23.f);
	if (Light)
	{
		Light->SetIntensity(LightCandelas * Flick * Burn);
	}
	if (FlameMat)
	{
		FlameMat->SetScalarParameterValue(TEXT("Intensity"), 18.f * (0.8f + 0.2f * Flick));
	}
	// grey smoke into the North's steam pool
	if (ULyudNorthSubsystem* N = ULyudNorthSubsystem::Get(this))
	{
		SmokeDebt += DeltaTime * 2.5f * Burn;
		while (SmokeDebt >= 1.f)
		{
			SmokeDebt -= 1.f;
			if (ALyudNorthSteam* Fx = N->Steam())
			{
				const float A = LyudNorthModel::Rand01(Rng) * 2.f * PI;
				Fx->Emit(GetActorLocation() + FVector(FMath::Cos(A) * 10.f, FMath::Sin(A) * 10.f, 90.f), FVector(0.f, 0.f, 90.f),
					30.f, 220.f + 80.f * LyudNorthModel::Rand01(Rng), 0.18f, 6.f, 15.f, 0.8f, 0.55f, 0.9f);
			}
		}
	}
	// embers: now and then one jumps off and rises, fading
	for (int32 I = 0; I < Embers.Num() && I < EmberParts.Num(); I++)
	{
		FEmber& Em = Embers[I];
		UStaticMeshComponent* C = EmberParts[I];
		if (Em.Life < 0.f)
		{
			if (Burn > 0.3f && LyudNorthModel::Rand01(Rng) < DeltaTime * 0.8f)
			{
				const float A = LyudNorthModel::Rand01(Rng) * 2.f * PI;
				Em.Pos = FVector(FMath::Cos(A) * 12.f, FMath::Sin(A) * 12.f, 30.f);
				Em.Vel = FVector(FMath::Cos(A) * 15.f, FMath::Sin(A) * 15.f, 90.f + 70.f * LyudNorthModel::Rand01(Rng));
				Em.Age = 0.f;
				Em.Life = 1.2f + 1.3f * LyudNorthModel::Rand01(Rng);
				C->SetVisibility(true);
			}
			continue;
		}
		Em.Age += DeltaTime;
		if (Em.Age >= Em.Life)
		{
			Em.Life = -1.f;
			C->SetVisibility(false);
			continue;
		}
		Em.Vel *= FMath::Exp(-0.6f * DeltaTime);
		Em.Pos += (Em.Vel + FVector(FMath::Sin(Clock * 3.f + I) * 12.f, FMath::Cos(Clock * 2.6f + I) * 12.f, 0.f)) * DeltaTime;
		C->SetRelativeLocation(Em.Pos);
		C->SetRelativeScale3D(FVector(1.f - Em.Age / Em.Life));
	}
}
