#include "HeroCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "CharacterStandard.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HeroAnimInstance.h"
#include "InputCoreTypes.h"

namespace
{
	const FName EyeL(TEXT("eye_l"));
	const FName EyeR(TEXT("eye_r"));
	const FName HeadBone(TEXT("head"));
	/** Murmuration's near plane: your own hands and chest are a few centimetres from the eye. */
	constexpr float NearClip = 4.f;

	float Wrap180(float A)
	{
		return FRotator::NormalizeAxis(A);
	}
}

AHeroCharacter::AHeroCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	BodyMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/Hero/SK_Hero.SK_Hero")));

	GetCapsuleComponent()->InitCapsuleSize(LyudmilovskCharacter::PlayerCapsuleRadius, LyudmilovskCharacter::PlayerCapsuleHalfHeight);

	// the body turns on its own (TurnBody), not with the mouse
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = false;
	// the template's movement (character movement defaults + its falling settings)
	Move->MaxWalkSpeed = RunSpeed;
	Move->BrakingDecelerationFalling = 1500.f;
	Move->AirControl = 0.5f;

	USkeletalMeshComponent* Body = GetMesh();
	Body->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -LyudmilovskCharacter::PlayerCapsuleHalfHeight + SoleDrop), FRotator(0.f, -90.f, 0.f));
	Body->SetAnimInstanceClass(UHeroAnimInstance::StaticClass());
	// the body casts its own shadow (headless: a hidden full copy for the shadow darkened the chest
	// under the invisible head and doubled the skinning cost)
	Body->SetCastShadow(true);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Body->bEnableUpdateRateOptimizations = false;


	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(Fov);
}

void AHeroCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (USkeletalMesh* Mesh = BodyMesh.LoadSynchronous())
	{
		GetMesh()->SetSkeletalMesh(Mesh);
		GetMesh()->HideBoneByName(HeadBone, PBO_None);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Hero: %s not found (import it with unreal/scripts/import_hero.sh)"), *BodyMesh.ToString());
	}
	if (GEngine && !FMath::IsNearlyEqual(GNearClippingPlane, NearClip))
	{
		GEngine->Exec(GetWorld(), *FString::Printf(TEXT("r.SetNearClipPlane %g"), NearClip));
	}
	BodyYaw = GetActorRotation().Yaw;
}

void AHeroCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	BodyYaw = GetActorRotation().Yaw;
}

void AHeroCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ReadInput(DeltaSeconds);
	TurnBody(DeltaSeconds);
	FeedAnimation(DeltaSeconds);
}

void AHeroCharacter::ReadInput(float DeltaSeconds)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || IsHidden())
	{
		return;
	}
	if (APlayerCameraManager* Cam = PC->PlayerCameraManager)
	{
		Cam->ViewPitchMin = PitchMin;
		Cam->ViewPitchMax = PitchMax;
	}

	if (!PC->IsLookInputIgnored())
	{
		float DX = 0.f, DY = 0.f;
		PC->GetInputMouseDelta(DX, DY);
		const float PadSpeed = 140.f * DeltaSeconds;
		DX = DX * LookSensitivity + PC->GetInputAnalogKeyState(EKeys::Gamepad_RightX) * PadSpeed;
		DY = DY * LookSensitivity + PC->GetInputAnalogKeyState(EKeys::Gamepad_RightY) * PadSpeed;
		if (DX != 0.f || DY != 0.f)
		{
			FRotator R = PC->GetControlRotation();
			R.Yaw = Wrap180(R.Yaw + DX);
			R.Pitch = FMath::Clamp(Wrap180(R.Pitch + DY), PitchMin, PitchMax);
			R.Roll = 0.f;
			PC->SetControlRotation(R);
		}
	}

	float Fwd = 0.f, Side = 0.f;
	if (!PC->IsMoveInputIgnored())
	{
		auto Down = [PC](const FKey& K) { return PC->IsInputKeyDown(K) ? 1.f : 0.f; };
		Fwd = Down(EKeys::W) + Down(EKeys::Up) - Down(EKeys::S) - Down(EKeys::Down) + PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
		Side = Down(EKeys::D) + Down(EKeys::Right) - Down(EKeys::A) - Down(EKeys::Left) + PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
		bWantsWalk = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);
		const bool bJump = PC->IsInputKeyDown(EKeys::SpaceBar) || PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom);
		if (bJump && !bJumpHeld)
		{
			Jump();
		}
		else if (!bJump && bJumpHeld)
		{
			StopJumping();
		}
		bJumpHeld = bJump;
	}
	GetCharacterMovement()->MaxWalkSpeed = bWantsWalk ? WalkSpeed : RunSpeed;
	const FRotator Yaw(0.f, PC->GetControlRotation().Yaw, 0.f);
	FVector Wish = Yaw.RotateVector(FVector(Fwd, Side, 0.f));
	if (Wish.SizeSquared() > 1.f)
	{
		Wish.Normalize();
	}
	if (!Wish.IsNearlyZero())
	{
		AddMovementInput(Wish);
	}
}

void AHeroCharacter::TurnBody(float DeltaSeconds)
{
	// Murmuration: standing still the head and chest take the first ~35 degrees, then the feet step
	// round; moving, the body faces where you look at once (the 8-way clips do the strafing)
	const float LookYaw = GetControlRotation().Yaw;
	const bool bMoving = GetVelocity().Size2D() > 5.f;
	const float Dead = bMoving ? 7.f : 34.f;
	float D = Wrap180(LookYaw - BodyYaw);
	if (FMath::Abs(D) > Dead)
	{
		const float Over = D - FMath::Sign(D) * Dead;
		BodyYaw += Over * FMath::Min(1.f, DeltaSeconds * (bMoving ? 12.f : 5.f));
	}
	if (bMoving)
	{
		BodyYaw += Wrap180(LookYaw - BodyYaw) * FMath::Min(1.f, DeltaSeconds * 15.f);
	}
	BodyYaw = Wrap180(BodyYaw);
	SetActorRotation(FRotator(0.f, BodyYaw, 0.f));
}

void AHeroCharacter::FeedAnimation(float DeltaSeconds)
{
	const FVector Vel = GetVelocity();
	const float Speed = Vel.Size2D();
	FHeroBodyState S;
	S.Speed = Speed;
	const FVector Local = GetActorRotation().UnrotateVector(Vel);
	S.MoveAngle = Speed > 1.f ? FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X)) : 0.f;
	Distance += Speed * DeltaSeconds;
	IdleTime += DeltaSeconds;
	const FRotator Look = GetControlRotation();
	S.LookYaw = Wrap180(Look.Yaw - BodyYaw);
	S.LookPitch = Wrap180(Look.Pitch);
	S.Distance = Distance;
	S.IdleTime = IdleTime;
	if (UHeroAnimInstance* Anim = Cast<UHeroAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		Anim->State = S;
	}
}

FVector AHeroCharacter::EyeLocation() const
{
	const USkeletalMeshComponent* Body = GetMesh();
	if (Body && Body->GetSkeletalMeshAsset() && Body->GetBoneIndex(EyeL) != INDEX_NONE)
	{
		return 0.5f * (Body->GetSocketLocation(EyeL) + Body->GetSocketLocation(EyeR));
	}
	return GetActorLocation() + FVector(0.f, 0.f, 70.f);
}

void AHeroCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult);
	OutResult.Location = EyeLocation();
	const float Down = -Wrap180(OutResult.Rotation.Pitch);
	OutResult.FOV = Fov + FovLookDownExtra * FMath::SmoothStep(40.f, 80.f, Down);
}
