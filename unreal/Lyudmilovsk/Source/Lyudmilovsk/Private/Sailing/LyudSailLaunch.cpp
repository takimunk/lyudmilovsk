// The coach's aluminium launch: LyudSail::FLaunch driven to a point, drawn (hull, the outboard turning on its clamp,
// a signal flag on the stern staff, the wake), its driver on the aft thwart, the outboard's four loops and the whistle
// and horn signals. ULyudSailing calls Advance (the actor never ticks). docs/yacht_school.md "Engine: the boats".
#include "Sailing/LyudDinghy.h"

#include "LyudDinghyImpl.h"

#include "Animation/AnimSequence.h"
#include "Audio/LyudSoundscape.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "People/LyudBodyAnim.h"
#include "People/PeopleLife.h"
#include "Sea/LyudSea.h"
#include "Templates/PimplPtr.h"
#include "Weather/LyudWind.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudSailLaunch, Log, All);

namespace
{
	// blender/sailing: the motor's clamp on the transom, the stern staff (port quarter), the aft thwart's top
	const float LaunchMotorX = -232.f, LaunchMotorZ = 43.f;
	const float LaunchStaffX = -222.f, LaunchStaffY = -55.f, LaunchSheerZ = 42.f;
	const float LaunchSeatX = -185.f, LaunchSeatTopZ = 31.f;
	// bodies and engine loops only this near the player's view (cm)
	const float LaunchBodyNear = 30000.f, LaunchBodyFar = 36000.f, LaunchHearFar = 45000.f;

	const TCHAR* const LaunchLoopPools[4] = { TEXT("sail_outboard_idle"), TEXT("sail_outboard_low"), TEXT("sail_outboard_mid"), TEXT("sail_outboard_high") };

	/** sailing.json's flag ids -> part C's flag meshes (SM_Sail_Flag_<id>). */
	FString LaunchFlagAsset(FName FlagId)
	{
		FString Id = FlagId.ToString().ToLower();
		Id.RemoveFromStart(TEXT("flag_"));
		static const TMap<FString, FString> Alias = {
			{ TEXT("p"), TEXT("prep") }, { TEXT("blue"), TEXT("prep") }, { TEXT("race"), TEXT("races") }, { TEXT("s"), TEXT("races") },
			{ TEXT("optimist"), TEXT("class_optimist") }, { TEXT("cadet"), TEXT("class_cadet") }, { TEXT("luch"), TEXT("class_luch") },
			{ TEXT("class"), TEXT("class_luch") }, { TEXT("first_sub"), TEXT("recall") }, { TEXT("x"), TEXT("recall") },
			{ TEXT("ap"), TEXT("shore") }, { TEXT("n"), TEXT("closed") }, { TEXT("school"), TEXT("sail_on") } };
		if (const FString* To = Alias.Find(Id))
		{
			Id = *To;
		}
		return FString(TEXT("Flag_")) + Id;
	}

	/** A plain colour for a flag that has no mesh yet. */
	FLinearColor LaunchFlagTint(FName FlagId)
	{
		const FString Id = FlagId.ToString().ToLower();
		if (Id.Contains(TEXT("recall")) || Id.EndsWith(TEXT("_x")) || Id.Contains(TEXT("first_sub")) || Id.EndsWith(TEXT("_p")) || Id.Contains(TEXT("blue")))
		{
			return FLinearColor(0.05f, 0.15f, 0.6f);
		}
		if (Id.Contains(TEXT("shore")) || Id.EndsWith(TEXT("_ap")) || Id.Contains(TEXT("storm")) || Id.Contains(TEXT("closed")))
		{
			return FLinearColor(0.75f, 0.06f, 0.05f);
		}
		if (Id.Contains(TEXT("school")) || Id.Contains(TEXT("sail_on")))
		{
			return FLinearColor(0.1f, 0.55f, 0.2f);
		}
		return FLinearColor(0.9f, 0.75f, 0.1f);
	}
}

// ------------------------------------------------------------------ the actor

ALyudSailLaunch::ALyudSailLaunch()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
	Impl = MakePimpl<FImpl>();
	Loops.SetNum(4);
}

ALyudSailLaunch::~ALyudSailLaunch() = default;

LyudSail::FLaunch& ALyudSailLaunch::Launch()
{
	return Impl->L;
}

FVector2D ALyudSailLaunch::PosM() const
{
	return FVector2D(Impl->L.X, Impl->L.Y);
}

float ALyudSailLaunch::YawRad() const
{
	return float(Impl->L.Yaw);
}

FVector2D ALyudSailLaunch::HitchM() const
{
	// the towing eye on the transom
	const LyudSail::FLaunch& L = Impl->L;
	return FVector2D(L.X - FMath::Cos(L.Yaw) * 2.3, L.Y - FMath::Sin(L.Yaw) * 2.3);
}

void ALyudSailLaunch::PlaceAt(const FVector2D& InPos, float InYaw)
{
	FImpl& I = *Impl;
	if (!I.bBuilt)
	{
		I.Build(*this);
	}
	I.L = LyudSail::FLaunch();
	I.L.X = InPos.X;
	I.L.Y = InPos.Y;
	I.L.Yaw = InYaw;
	I.bParked = false;
	SetActorHiddenInGame(false);
	I.Sample(*this);
	I.Place(*this, 0.f);
}

void ALyudSailLaunch::ParkAt(const FTransform& World)
{
	FImpl& I = *Impl;
	if (!I.bBuilt)
	{
		I.Build(*this);
	}
	const FVector At = World.GetLocation();
	I.L = LyudSail::FLaunch();
	I.L.X = At.X * 0.01;
	I.L.Y = At.Y * 0.01;
	I.L.Yaw = FMath::DegreesToRadians(World.Rotator().Yaw);
	I.StopEngine(*this, 0.4f);
	I.bParked = true;
	SetActorHiddenInGame(false);
	// moored she floats where she is put; hauled out (the transform over the water's level) she lies as given
	I.Sample(*this);
	FVector Where = At;
	FRotator Lie = World.Rotator();
	if (I.SurfaceZ > At.Z - 15.f)
	{
		Where.Z = I.SurfaceZ;
		Lie = FRotator(0.f, Lie.Yaw, 0.f);
	}
	SetActorLocationAndRotation(Where, Lie);
	if (I.Hull)
	{
		I.Hull->SetRelativeRotation(FRotator::ZeroRotator);
	}
	if (I.Motor)
	{
		// tilted up out of the water
		I.Motor->SetRelativeRotation(FRotator(-35.f, 0.f, 0.f));
	}
	I.Wake.Hide(Where);
	if (ANpcBody* Was = I.Driver.Get())
	{
		// nobody sits in a moored launch (the person stays claimed until SetDriver(NAME_None))
		Was->Destroy();
		I.Driver.Reset();
		I.bDriverTried = false;
	}
}

void ALyudSailLaunch::Advance(const FVector2D& TargetM, float CruiseMs, float RadiusM, float DeltaTime)
{
	FImpl& I = *Impl;
	if (!I.bBuilt)
	{
		I.Build(*this);
	}
	const float Dt = FMath::Clamp(DeltaTime, 0.f, 0.25f);
	if (Dt <= 0.f)
	{
		return;
	}
	I.bParked = false;
	double Throttle = 0.0, Steer = 0.0;
	if (CruiseMs > 0.f)
	{
		LyudSail::LaunchDriveTo(I.L, TargetM.X, TargetM.Y, CruiseMs, RadiusM, Throttle, Steer);
	}
	I.Sample(*this);
	LyudSail::StepLaunch(I.L, Throttle, Steer, I.Env, Dt);
	I.Turn = FMath::FInterpTo(I.Turn, float(Steer), Dt, 5.f);
	// how far the player's view is: the driver's body and the engine's loops are for near
	I.ViewDist = 0.f;
	if (const UWorld* W = GetWorld())
	{
		if (const APlayerController* PC = W->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
		{
			I.ViewDist = float(FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), GetActorLocation()));
		}
	}
	I.Place(*this, Dt);
	I.Engine(*this, Dt);
	I.DriverTick(*this);
}

void ALyudSailLaunch::SetDriver(FName PersonKey)
{
	FImpl& I = *Impl;
	if (I.DriverKey == PersonKey)
	{
		return;
	}
	LyudSailKit::ReleaseBody(this, I.Driver, I.DriverId);
	I.DriverKey = PersonKey;
	I.bDriverTried = false;
	if (!PersonKey.IsNone())
	{
		// theirs from now, body or not: the life does not stand them on the slip meanwhile
		if (UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this))
		{
			I.DriverId = Life->FindPerson(PersonKey.ToString());
			if (I.DriverId >= 0)
			{
				Life->SetPresentedExternally(I.DriverId, true);
			}
		}
	}
}

void ALyudSailLaunch::SetFlag(FName FlagId)
{
	FImpl& I = *Impl;
	if (!I.bBuilt)
	{
		I.Build(*this);
	}
	if (I.FlagId != FlagId)
	{
		I.FlagId = FlagId;
		I.DressFlag(*this);
	}
}

void ALyudSailLaunch::Sound(int32 Signal)
{
	static const TCHAR* const Pools[5] = { TEXT("sail_whistle_short"), TEXT("sail_whistle_long"), TEXT("sail_whistle_recall"), TEXT("sail_horn_short"), TEXT("sail_horn_long") };
	if (ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this))
	{
		Scape->PlayEffect(FName(Pools[FMath::Clamp(Signal, 0, 4)]), GetActorLocation() + FVector(0.f, 0.f, 120.f), 1.f);
	}
}

void ALyudSailLaunch::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Impl)
	{
		Impl->StopEngine(*this, 0.f);
		LyudSailKit::ReleaseBody(this, Impl->Driver, Impl->DriverId);
	}
	Super::EndPlay(Reason);
}

// ------------------------------------------------------------------ building

void ALyudSailLaunch::FImpl::Build(ALyudSailLaunch& A)
{
	bBuilt = true;
	Rng.Initialize(int32(A.GetUniqueID()));
	Hull = LyudSailKit::Frame(&A, A.Root, TEXT("Hull"), A.Kept);
	HullMesh = LyudSailKit::Part(&A, Hull, TEXT("HullMesh"), A.Kept);
	Motor = LyudSailKit::Part(&A, Hull, TEXT("Motor"), A.Kept);
	Staff = LyudSailKit::Part(&A, Hull, TEXT("Staff"), A.Kept);
	Flag = LyudSailKit::Part(&A, Hull, TEXT("Flag"), A.Kept);
	Staff->SetCastShadow(false);
	Flag->SetCastShadow(false);
	const FLinearColor Alu(0.56f, 0.58f, 0.6f);
	if (UStaticMesh* M = LyudSailKit::Asset(TEXT("Launch")))
	{
		// the staff is part of the imported hull
		HullMesh->SetStaticMesh(M);
		Staff->SetVisibility(false);
	}
	else
	{
		HullMesh->SetStaticMesh(LyudSailKit::LaunchHull().Mesh);
		LyudSailKit::PaintBuilt(HullMesh, LyudSailKit::LaunchHull(), Alu, Alu);
		Staff->SetStaticMesh(LyudSailKit::RopePiece().Mesh);
		LyudSailKit::PaintBuilt(Staff, LyudSailKit::RopePiece(), FLinearColor::White, FLinearColor::White);
		Staff->SetRelativeLocationAndRotation(FVector(LaunchStaffX, LaunchStaffY, LaunchSheerZ), FRotator(90.f, 0.f, 0.f));
		Staff->SetRelativeScale3D(FVector(140.f, 2.f, 2.f));
	}
	if (UStaticMesh* M = LyudSailKit::Asset(TEXT("Launch_Motor")))
	{
		Motor->SetStaticMesh(M);
	}
	else
	{
		Motor->SetStaticMesh(LyudSailKit::LaunchMotor().Mesh);
		LyudSailKit::PaintBuilt(Motor, LyudSailKit::LaunchMotor(), Alu, Alu);
	}
	Motor->SetRelativeLocation(FVector(LaunchMotorX, 0.f, LaunchMotorZ));
	// a signal flag flies under the staff's own red one
	Flag->SetRelativeLocation(FVector(LaunchStaffX, LaunchStaffY, LaunchSheerZ + 98.f));
	if (UInstancedStaticMeshComponent* Pool = LyudSailKit::FoamPool(&A, A.Root, 36, A.Kept))
	{
		Wake.Init(Pool, 36, A.GetActorLocation());
	}
	DressFlag(A);
}

void ALyudSailLaunch::FImpl::DressFlag(ALyudSailLaunch& A)
{
	if (!Flag)
	{
		return;
	}
	if (FlagId.IsNone())
	{
		Flag->SetVisibility(false);
		return;
	}
	Flag->EmptyOverrideMaterials();
	if (UStaticMesh* M = LyudSailKit::Asset(LaunchFlagAsset(FlagId)))
	{
		Flag->SetStaticMesh(M);
		bFlagAsset = true;
	}
	else
	{
		const FLinearColor Tint = LaunchFlagTint(FlagId);
		Flag->SetStaticMesh(LyudSailKit::FlagQuad().Mesh);
		LyudSailKit::PaintBuilt(Flag, LyudSailKit::FlagQuad(), Tint, Tint);
		bFlagAsset = false;
	}
	Flag->SetVisibility(true);
}

// ------------------------------------------------------------------ each step

void ALyudSailLaunch::FImpl::Sample(ALyudSailLaunch& A)
{
	const FVector C(L.X * 100.0, L.Y * 100.0, 0.0);
	const ALyudSea* Sea = ALyudSea::Get(&A);
	const float Level = Sea ? Sea->SeaLevel() : float(A.GetActorLocation().Z);
	if (const ULyudWindSubsystem* Wind = ULyudWindSubsystem::Get(&A))
	{
		const FVector V = Wind->GetWindAt(FVector(C.X, C.Y, Level), 2.f, true).Velocity;
		Env.WindX = V.X;
		Env.WindY = V.Y;
	}
	if (!Sea)
	{
		Env.Depth = 100.0;
		Env.NormalX = Env.NormalY = 0.0;
		Env.NormalZ = 1.0;
		Env.WaveHs = 0.0;
		SurfaceZ = Level;
		return;
	}
	const FLyudSeaSample S = Sea->SampleSurface(C);
	Env.Depth = S.Depth * 0.01;
	Env.NormalX = S.Normal.X;
	Env.NormalY = S.Normal.Y;
	Env.NormalZ = S.Normal.Z;
	Env.WaveHs = Sea->WaveHeight() * 0.01;
	SurfaceZ = S.SurfaceZ;
}

void ALyudSailLaunch::FImpl::Place(ALyudSailLaunch& A, float DeltaTime)
{
	const float YawDeg = FMath::RadiansToDegrees(float(L.Yaw));
	const FVector At(L.X * 100.0, L.Y * 100.0, SurfaceZ);
	A.SetActorLocationAndRotation(At, FRotator(0.f, YawDeg, 0.f));
	if (!Hull)
	{
		return;
	}
	// she banks into her turns and lifts her bow over the hump
	Hull->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(float(L.Pitch)), 0.f, FMath::RadiansToDegrees(float(L.Heel))));
	// the outboard on its clamp: the tiller goes away from the turn, the leg toward it
	Motor->SetRelativeRotation(FRotator(0.f, -28.f * Turn, 0.f));
	if (DeltaTime <= 0.f)
	{
		return;
	}
	if (Flag->IsVisible())
	{
		// streaming in the wind she feels (the true wind less her own way), in the hull's frame
		const FVector2D Felt(Env.WindX - L.VX, Env.WindY - L.VY);
		if (Felt.SizeSquared() > 0.25)
		{
			const float Want = FMath::RadiansToDegrees(float(FMath::Atan2(Felt.Y, Felt.X))) - YawDeg;
			FlagYaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(FlagYaw, Want), -220.f * DeltaTime, 220.f * DeltaTime);
			FlagYaw = FMath::UnwindDegrees(FlagYaw);
		}
		const float Droop = FMath::Clamp(1.f - float(Felt.Size()) / 4.f, 0.f, 1.f);
		Flag->SetRelativeRotation(FRotator(-70.f * Droop, FlagYaw, 0.f));
	}

	// the wake: foam off the transom every stretch of water, a rooster tail on the plane
	const FVector Fwd(FMath::Cos(L.Yaw), FMath::Sin(L.Yaw), 0.0);
	const float Speed = FMath::Abs(float(L.Speed));
	if (Wake.Ism && ViewDist < 60000.f)
	{
		WakeRun += Speed * DeltaTime;
		if (WakeRun > 0.8f)
		{
			WakeRun = 0.f;
			if (L.Wake > 0.05)
			{
				const FVector Stern = At - Fwd * 235.f + FVector(0.f, 0.f, 3.f);
				Wake.Add(Stern, YawDeg, 150.f * (0.6f + 0.9f * float(L.Wake)), 2.5f + 2.5f * float(L.Wake), 0.f);
				if (L.Planing > 0.5)
				{
					Wake.Add(Stern - Fwd * 60.f, YawDeg, 70.f + 50.f * float(L.Planing), 0.7f, 45.f * float(L.Planing));
				}
			}
		}
		Wake.Tick(DeltaTime, At);
	}
	else
	{
		Wake.Hide(At);
	}
}

void ALyudSailLaunch::FImpl::StopEngine(ALyudSailLaunch& A, float Fade)
{
	for (int32 I = 0; I < A.Loops.Num(); ++I)
	{
		if (UAudioComponent* Loop = A.Loops[I])
		{
			ULyudSoundscapeSubsystem::ReleaseLoop(Loop, Fade);
			A.Loops[I] = nullptr;
		}
		if (I < 4)
		{
			LoopGain[I] = 0.f;
			LoopQuiet[I] = 0.f;
		}
	}
	if (bRunning && Fade > 0.f && ViewDist < LaunchHearFar)
	{
		if (ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(&A))
		{
			Scape->PlayEffect(TEXT("sail_outboard_off"), A.GetActorLocation(), 0.9f);
		}
	}
	bRunning = false;
}

void ALyudSailLaunch::FImpl::Engine(ALyudSailLaunch& A, float DeltaTime)
{
	ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(&A);
	if (!Scape || !Hull)
	{
		return;
	}
	const bool bHeard = ViewDist < LaunchHearFar;
	const FVector At = A.GetActorLocation();
	if (!bRunning)
	{
		// the cord pulled: she catches and settles to her idle
		bRunning = true;
		if (bHeard)
		{
			Scape->PlayEffect(TEXT("sail_outboard_pull_catch"), At, 0.9f);
		}
	}
	// 1000 rpm idling .. 4800 flat out, by how hard the engine works
	const float Rpm = 1000.f + 3800.f * FMath::Clamp(float(L.EngineLoad), 0.f, 1.f);
	const LyudSail::Sound::FOutboardMix Mix = LyudSail::Sound::MixOutboard(Rpm);
	const float Vol = Scape->MasterVolume * Scape->EffectsVolume;
	for (int32 I = 0; I < 4 && I < A.Loops.Num(); ++I)
	{
		const float Want = bHeard ? Mix.Gain[I] : 0.f;
		LoopGain[I] = FMath::FInterpTo(LoopGain[I], Want, DeltaTime, 6.f);
		UAudioComponent* Loop = A.Loops[I];
		if (!Loop && Want > 0.03f)
		{
			Loop = Scape->AttachLoop(FName(LaunchLoopPools[I]), Motor, 0.01f);
			A.Loops[I] = Loop;
			LoopQuiet[I] = 0.f;
		}
		if (Loop)
		{
			Loop->SetVolumeMultiplier(FMath::Max(LoopGain[I] * Vol, 0.001f));
			Loop->SetPitchMultiplier(Mix.Pitch[I]);
			LoopQuiet[I] = Want < 0.01f ? LoopQuiet[I] + DeltaTime : 0.f;
			if (LoopQuiet[I] > 2.f)
			{
				ULyudSoundscapeSubsystem::ReleaseLoop(Loop, 0.2f);
				A.Loops[I] = nullptr;
			}
		}
	}
	// the aluminium bottom banging on the chop at speed: the faster and the rougher, the more often
	const float Speed = FMath::Abs(float(L.Speed));
	if (bHeard && Speed > 4.f)
	{
		SlapIn -= DeltaTime * (Speed / 8.f) * (0.4f + float(Env.WaveHs) / 0.3f);
		if (SlapIn <= 0.f)
		{
			SlapIn = Rng.FRandRange(0.6f, 1.4f);
			Scape->PlayEffect(TEXT("sail_alu_slap"), At, FMath::Clamp(0.4f + Speed / 16.f, 0.4f, 1.f));
		}
	}
}

void ALyudSailLaunch::FImpl::DriverTick(ALyudSailLaunch& A)
{
	ANpcBody* Body = Driver.Get();
	if (DriverKey.IsNone() || !Hull)
	{
		return;
	}
	if (Body && ViewDist > LaunchBodyFar)
	{
		Body->Destroy();
		Driver.Reset();
		bDriverTried = false;
		return;
	}
	if (Body || bDriverTried || ViewDist > LaunchBodyNear)
	{
		return;
	}
	bDriverTried = true;
	Body = LyudSailKit::SpawnBody(&A, DriverKey, DriverId);
	if (!Body)
	{
		return;
	}
	Driver = Body;
	// the clip's origin is under the aft thwart's middle, the thwart's top 40 cm over it for the clips' body;
	// facing the bow (the mesh's +Y on the boat's +X), the outboard's tiller behind on the right
	Body->AttachToComponent(Hull, FAttachmentTransformRules::KeepRelativeTransform);
	Body->SetActorRelativeLocation(FVector(LaunchSeatX, 0.f, LaunchSeatTopZ - 40.f * Body->GetBodyScale()));
	Body->SetActorRelativeRotation(FRotator(0.f, -90.f, 0.f));
	UAnimSequence* Seq = LyudSailKit::Clip(TEXT("Sail_Coach_Drive"));
	if (!Seq)
	{
		Seq = LyudSailKit::Clip(TEXT("SitIdle"));
	}
	if (ULyudBodyAnimInstance* Anim = Cast<ULyudBodyAnimInstance>(Body->Mesh->GetAnimInstance()))
	{
		Anim->SetOnFeet(false);
		if (Seq)
		{
			Anim->PlayClip(Seq, true, 1.f, 0.f);
		}
	}
	Body->SetActorHiddenInGame(Seq == nullptr);
	UE_LOG(LogLyudSailLaunch, Display, TEXT("SAIL launch driver %s aboard"), *DriverKey.ToString());
}
