#include "BoothOwner.h"

#include "Animation/AnimSequence.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

ABoothOwner::ABoothOwner()
{
	PrimaryActorTick.bCanEverTick = true;
	CallZone = CreateDefaultSubobject<UBoxComponent>(TEXT("CallZone"));
	CallZone->SetBoxExtent(FVector(60.f, 200.f, 90.f));
	CallZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CallZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	CallZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CallZone->SetGenerateOverlapEvents(true);
	RootComponent = CallZone;
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(CallZone);
	Body->SetUsingAbsoluteLocation(true);
	Body->SetUsingAbsoluteRotation(true);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABoothOwner::BeginPlay()
{
	Super::BeginPlay();
	Enter(EState::Seated);
}

void ABoothOwner::SetPose(const FVector& Loc, float Yaw)
{
	Pos = Loc;
	Facing = FRotator::NormalizeAxis(Yaw);
	Body->SetWorldLocationAndRotation(Pos, FRotator(0.f, Facing + MeshYawOffset, 0.f));
}

void ABoothOwner::Play(UAnimSequence* Anim, bool bLoop)
{
	if (Anim)
	{
		Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Body->PlayAnimation(Anim, bLoop);
	}
}

void ABoothOwner::Enter(EState NewState)
{
	State = NewState;
	StateTime = 0.f;
	switch (State)
	{
	case EState::Seated:
		SetPose(SeatLocation, SeatYaw);
		Play(SitAnim, true);
		break;
	case EState::StandingUp:
		Play(StandUpAnim, false);
		break;
	case EState::ToWindow:
	case EState::ToSeat:
		Play(WalkAnim, true);
		break;
	case EState::AtWindow:
		Play(StandAnim, true);
		break;
	case EState::SittingDown:
		Play(SitDownAnim, false);
		break;
	}
}

FVector ABoothOwner::StandSpot() const
{
	return SeatLocation + FRotator(0.f, SeatYaw, 0.f).Vector() * StandOffset;
}

bool ABoothOwner::PlayerAtWindow() const
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	return Pawn && CallZone->IsOverlappingActor(Pawn);
}

bool ABoothOwner::TurnTo(float Yaw, float DeltaSeconds)
{
	const float Delta = FRotator::NormalizeAxis(Yaw - Facing);
	const float Step = FMath::Clamp(Delta, -TurnSpeed * DeltaSeconds, TurnSpeed * DeltaSeconds);
	SetPose(Pos, Facing + Step);
	return FMath::Abs(Delta - Step) < 1.f;
}

bool ABoothOwner::WalkTo(const FVector& Target, float DeltaSeconds)
{
	const FVector To = Target - Pos;
	const float Dist = To.Size2D();
	if (Dist < 2.f)
	{
		SetPose(Target, Facing);
		return true;
	}
	// face the way he walks before moving off
	if (!TurnTo(To.Rotation().Yaw, DeltaSeconds))
	{
		return false;
	}
	const FVector Step = To.GetSafeNormal2D() * FMath::Min(Dist, WalkSpeed * DeltaSeconds);
	SetPose(Pos + Step, Facing);
	return false;
}

void ABoothOwner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	StateTime += DeltaSeconds;
	const bool bCalled = PlayerAtWindow();
	AwayTime = bCalled ? 0.f : AwayTime + DeltaSeconds;
	// without walking animations he just stays at his computer
	const bool bCanGetUp = WalkAnim && StandAnim;
	auto AnimDone = [this](UAnimSequence* Anim) { return !Anim || StateTime >= Anim->GetPlayLength(); };

	switch (State)
	{
	case EState::Seated:
		if (bCalled && bCanGetUp)
		{
			Enter(EState::StandingUp);
		}
		break;
	case EState::StandingUp:
		if (AnimDone(StandUpAnim))
		{
			SetPose(StandSpot(), Facing);
			Enter(EState::ToWindow);
		}
		break;
	case EState::ToWindow:
		if (WalkTo(WindowLocation, DeltaSeconds))
		{
			Enter(EState::AtWindow);
		}
		break;
	case EState::AtWindow:
	{
		if (AwayTime > ReturnDelay)
		{
			Enter(EState::ToSeat);
			break;
		}
		float Look = WindowYaw;
		if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			const float ToPlayer = (Pawn->GetActorLocation() - Pos).Rotation().Yaw;
			Look = WindowYaw + FMath::Clamp(FRotator::NormalizeAxis(ToPlayer - WindowYaw), -LookLimit, LookLimit);
		}
		TurnTo(Look, DeltaSeconds);
		break;
	}
	case EState::ToSeat:
		if (bCalled)
		{
			Enter(EState::ToWindow);
		}
		else if (WalkTo(StandSpot(), DeltaSeconds) && TurnTo(SeatYaw, DeltaSeconds))
		{
			Enter(EState::SittingDown);
		}
		break;
	case EState::SittingDown:
		if (AnimDone(SitDownAnim))
		{
			Enter(EState::Seated);
		}
		break;
	}
}
