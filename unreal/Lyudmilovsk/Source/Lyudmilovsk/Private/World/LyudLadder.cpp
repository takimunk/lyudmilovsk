#include "World/LyudLadder.h"

#include "Audio/LyudSoundscape.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HeroAnimInstance.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	TAutoConsoleVariable<float> CVarLadderAuto(TEXT("lyud.Ladder.AutoClimb"), 0.f,
		TEXT("Tests: while the player is on a ladder, climb as if W (1) or S (-1) were held."));

	ACharacter* LadderHero(UWorld* W)
	{
		APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		return PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs LadderSpawnCmd(TEXT("lyud.Ladder.Spawn"),
		TEXT("lyud.Ladder.Spawn [rungs] [cm ahead]: a free-standing ladder in front of the player, facing them (tests)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			ACharacter* Hero = LadderHero(W);
			if (!Hero)
			{
				return;
			}
			const FVector Fwd = FRotator(0.f, Hero->GetActorRotation().Yaw, 0.f).Vector();
			const float Ahead = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 80.f;
			const FVector Feet = Hero->GetActorLocation() - FVector(0.f, 0.f, Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			P.bDeferConstruction = true;
			// its +X toward the player
			ALyudLadder* L = W->SpawnActor<ALyudLadder>(ALyudLadder::StaticClass(), FTransform((-Fwd).Rotation(), Feet + Fwd * Ahead), P);
			if (L)
			{
				L->Ladder->RungCount = Args.Num() > 0 ? FMath::Max(2, FCString::Atoi(*Args[0])) : 14;
				L->Ladder->TopExit = FVector(-40.f, 0.f, 0.f);
				L->FinishSpawning(FTransform((-Fwd).Rotation(), Feet + Fwd * Ahead));
				UE_LOG(LogTemp, Display, TEXT("LADDER spawned %d rungs at %s"), L->Ladder->RungCount, *L->GetActorLocation().ToString());
			}
		}));

	FAutoConsoleCommandWithWorld LadderGrabCmd(TEXT("lyud.Ladder.Grab"), TEXT("The player takes hold of the nearest interactive ladder (tests)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
		{
			ACharacter* Hero = LadderHero(W);
			ULyudLadderSubsystem* S = W ? W->GetSubsystem<ULyudLadderSubsystem>() : nullptr;
			ULyudLadderComponent* Best = S ? S->Nearest(Hero ? Hero->GetActorLocation() : FVector::ZeroVector) : nullptr;
			if (Hero && Best)
			{
				ULyudLadderClimber::Get(Hero)->GrabByPlayer(Best);
				UE_LOG(LogTemp, Display, TEXT("LADDER grabbed"));
			}
		}));

	FAutoConsoleCommandWithWorld LadderReportCmd(TEXT("lyud.Ladder.Report"), TEXT("Logs the player's climb (LADDER ...)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
		{
			ACharacter* Hero = LadderHero(W);
			const ULyudLadderClimber* C = ULyudLadderClimber::Get(Hero, false);
			UE_LOG(LogTemp, Display, TEXT("LADDER report: on %d player %d feet %.1f weight %.2f hero %s"), C && C->IsOnLadder() ? 1 : 0,
				C && C->IsPlayerClimbing() ? 1 : 0, C ? C->GetFeetZ() : -1.f, C ? C->GetWeight() : 0.f,
				Hero ? *Hero->GetActorLocation().ToString() : TEXT("-"));
		}));

	float Ramp(float F, float A, float B)
	{
		const float T = FMath::Clamp((F - A) / (B - A), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	/** The hero's animation instance (the body the targets are for), if the character is the hero. */
	UHeroAnimInstance* HeroAnim(const ACharacter* C)
	{
		return C && C->GetMesh() ? Cast<UHeroAnimInstance>(C->GetMesh()->GetAnimInstance()) : nullptr;
	}
}

// ------------------------------------------------------------------ the ladder

FVector ULyudLadderComponent::RungWorld(int32 Index, float Across) const
{
	// the climber faces -X: their right is the frame's -Y
	return ToWorld(FVector(0.f, -Across, RungZ(Index)));
}

FVector ULyudLadderComponent::Facing() const
{
	FVector F = -GetComponentTransform().GetUnitAxis(EAxis::X);
	F.Z = 0.f;
	return F.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
}

void ULyudLadderComponent::OnRegister()
{
	Super::OnRegister();
	if (UWorld* W = GetWorld(); W && W->IsGameWorld())
	{
		if (ULyudLadderSubsystem* S = W->GetSubsystem<ULyudLadderSubsystem>())
		{
			S->Register(this);
		}
	}
}

void ULyudLadderComponent::OnUnregister()
{
	if (UWorld* W = GetWorld())
	{
		if (ULyudLadderSubsystem* S = W->GetSubsystem<ULyudLadderSubsystem>())
		{
			S->Unregister(this);
		}
	}
	Super::OnUnregister();
}

// ------------------------------------------------------------------ hand over hand

LyudLadder::FLimbs LyudLadder::Limbs(const ULyudLadderComponent& L, float FeetZ, float HandAboveFeet)
{
	FLimbs Out;
	const float S = FMath::Max(L.RungSpacing, 1.f);
	// the feet's place in rungs (0 = on the lowest), in cycles of two rungs
	const float P = (FeetZ - L.FirstRungZ) / S;
	const float U = P * 0.5f;
	const float N = FMath::FloorToFloat(U);
	const float F = U - N;
	const int32 Base = 2 * int32(N);
	const int32 HandBase = FMath::RoundToInt(HandAboveFeet / S);
	// right hand, left foot, left hand, right foot, each in its own part of the cycle
	struct FStep { int32 Limb; int32 Rung; float A, B; };
	const FStep Steps[4] = {
		{ 1, Base + HandBase, 0.00f, 0.25f },
		{ 2, Base, 0.20f, 0.45f },
		{ 0, Base + HandBase + 1, 0.50f, 0.75f },
		{ 3, Base + 1, 0.70f, 0.95f },
	};
	const int32 Top = L.RungCount - 1;
	for (const FStep& St : Steps)
	{
		const bool bHand = St.Limb < 2;
		const float M = Ramp(F, St.A, St.B);
		const float Side = (St.Limb == 0 || St.Limb == 2) ? -1.f : 1.f;     // left -, right + (across toward the right)
		const float Across = Side * (bHand ? FMath::Min(15.f, L.HalfWidth - 4.f) : FMath::Min(11.f, L.HalfWidth - 6.f));
		auto Point = [&](int32 Rung, bool& bFloor)
		{
			// the climber's right is the frame's -Y
			if (!bHand && L.RungZ(Rung) < 0.5f * L.RungSpacing)
			{
				// below the lowest rung: standing on the floor behind the ladder
				bFloor = true;
				return FVector(14.f, -Across, 0.f);
			}
			bFloor = false;
			const int32 R = FMath::Clamp(Rung, 0, Top);
			// a hand closes round the rung's middle, a boot's ball stands on its top
			return FVector(0.f, -Across, L.RungZ(R) + (bHand ? 0.f : L.RungRadius));
		};
		bool bF0 = false, bF1 = false;
		const FVector P0 = Point(St.Rung, bF0);
		const FVector P1 = Point(St.Rung + 2, bF1);
		const float Arc = FMath::Sin(PI * M);
		FVector At = FMath::Lerp(P0, P1, M);
		// off the rung, out and round it, onto the next
		At += bHand ? FVector(9.f * Arc, 0.f, 0.f) : FVector(12.f * Arc, 0.f, 5.f * Arc);
		Out.At[St.Limb] = At;
		Out.Moving[St.Limb] = (M > 0.f && M < 1.f) ? M : 0.f;
		Out.Rung[St.Limb] = M < 0.5f ? St.Rung : St.Rung + 2;
		Out.Close[St.Limb] = bHand ? 1.f - FMath::Pow(Arc, 0.6f) : 1.f;
		if (!bHand)
		{
			Out.bOnFloor[St.Limb - 2] = M < 0.5f ? bF0 : bF1;
		}
	}
	return Out;
}

// ------------------------------------------------------------------ the climber

ULyudLadderClimber::ULyudLadderClimber()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// after the character's own tick (its animation state), before the animation runs
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

ULyudLadderClimber* ULyudLadderClimber::Get(ACharacter* Who, bool bCreate)
{
	if (!Who)
	{
		return nullptr;
	}
	ULyudLadderClimber* C = Who->FindComponentByClass<ULyudLadderClimber>();
	if (!C && bCreate)
	{
		C = NewObject<ULyudLadderClimber>(Who, TEXT("LadderClimber"));
		C->RegisterComponent();
		C->AddTickPrerequisiteActor(Who);
	}
	return C;
}

void ULyudLadderClimber::Attach(ULyudLadderComponent* InLadder, float InFeetZ, float BlendSeconds)
{
	if (!InLadder)
	{
		return;
	}
	const bool bWasOn = Ladder.IsValid() && !bReleasing;
	Ladder = InLadder;
	FeetZ = InFeetZ;
	bReleasing = false;
	bStepping = false;
	BlendRate = BlendSeconds > 0.01f ? 1.f / BlendSeconds : 1000.f;
	if (!bWasOn && BlendSeconds <= 0.01f)
	{
		Weight = 1.f;
	}
	for (int32 I = 0; I < 4; ++I)
	{
		LastRung[I] = INT32_MIN;
		bWasMoving[I] = false;
	}
	if (AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwner()))
	{
		const FVector F = Ladder->Facing();
		Hero->SetBodyYaw(float(F.Rotation().Yaw));
		Hero->HoldBodyYaw(true);
	}
	SetComponentTickEnabled(true);
	Place();
}

void ULyudLadderClimber::Release(float BlendSeconds)
{
	if (!Ladder.IsValid() || bReleasing)
	{
		return;
	}
	bReleasing = true;
	BlendRate = BlendSeconds > 0.01f ? 1.f / BlendSeconds : 1000.f;
	if (AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwner()))
	{
		Hero->HoldBodyYaw(false);
	}
}

void ULyudLadderClimber::SetFeetZ(float InFeetZ)
{
	FeetZ = InFeetZ;
	Place();
}

void ULyudLadderClimber::Place()
{
	ACharacter* C = Cast<ACharacter>(GetOwner());
	const ULyudLadderComponent* L = Ladder.Get();
	if (!C || !L || bReleasing || bStepping)
	{
		return;
	}
	const FVector Feet = L->ToWorld(FVector(L->BodyStandoff, 0.f, FeetZ));
	const FVector Want = Feet + FVector(0.f, 0.f, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	// blending onto the ladder: from where the body was (the caller's own move) to the ladder's place
	const FVector Now = C->GetActorLocation();
	const FVector At = Weight >= 0.999f ? Want : FMath::Lerp(Now, Want, FMath::Clamp(Weight + 0.15f, 0.f, 1.f));
	C->SetActorLocationAndRotation(At, L->Facing().Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
}

void ULyudLadderClimber::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Ladder.IsValid())
	{
		// the ladder went away under the climber (its actor destroyed or streamed out): what the climb took is given back
		ACharacter* Who = Cast<ACharacter>(GetOwner());
		if (!bReleasing)
		{
			if (AHeroCharacter* Hero = Cast<AHeroCharacter>(Who))
			{
				Hero->HoldBodyYaw(false);
			}
		}
		if (bPlayerDriven)
		{
			// the player's own climb took the movement away (a scripted climb's caller looks after its own)
			bPlayerDriven = false;
			if (Who && Who->GetCharacterMovement() && Who->GetCharacterMovement()->MovementMode == MOVE_None)
			{
				Who->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			}
		}
		bStepping = false;
		bReleasing = false;
		Weight = 0.f;
		if (UHeroAnimInstance* Anim = HeroAnim(Who))
		{
			Anim->Ladder = FBikeRiderPose();
		}
		SetComponentTickEnabled(false);
		return;
	}
	Weight = FMath::FInterpConstantTo(Weight, bReleasing ? 0.f : 1.f, DeltaTime, BlendRate);
	if (bPlayerDriven)
	{
		TickPlayer(DeltaTime);
	}
	if (!Ladder.IsValid())
	{
		return;
	}
	// (a step off the top takes a little longer than the body's blend off the ladder: the climber stays until the step
	// has put the hero down and given him his movement back, or he would be left hanging in MOVE_None)
	if (bReleasing && Weight <= 0.f && !bStepping)
	{
		Ladder = nullptr;
		bReleasing = false;
		if (UHeroAnimInstance* Anim = HeroAnim(Cast<ACharacter>(GetOwner())))
		{
			Anim->Ladder = FBikeRiderPose();
		}
		SetComponentTickEnabled(false);
		return;
	}
	if (!bReleasing && Weight < 1.f)
	{
		// still blending on: keep closing in on the ladder's place
		Place();
	}
	Pose(DeltaTime);
}

void ULyudLadderClimber::Pose(float DeltaTime)
{
	ACharacter* C = Cast<ACharacter>(GetOwner());
	UHeroAnimInstance* Anim = HeroAnim(C);
	const ULyudLadderComponent* L = Ladder.Get();
	if (!Anim || !L)
	{
		return;
	}
	const LyudLadder::FLimbs Limbs = LyudLadder::Limbs(*L, FeetZ, HandAboveFeet);
	const FTransform M = C->GetMesh()->GetComponentTransform();
	const FTransform& LT = L->GetComponentTransform();
	auto CS = [&M, &LT](const FVector& Local) { return M.InverseTransformPosition(LT.TransformPosition(Local)); };
	auto Dir = [&M](const FVector& World) { return M.InverseTransformVectorNoScale(World.GetSafeNormal()); };
	const FVector Fwd = L->Facing();
	const FVector Up = FVector::UpVector;
	const FVector Left = -FVector::CrossProduct(Up, Fwd);
	const FVector Along = LT.GetUnitAxis(EAxis::Y);

	FBikeRiderPose& R = Anim->Ladder;
	R = FBikeRiderPose();
	R.Weight = Weight;
	R.Fwd = Dir(Fwd);
	R.Up = Dir(Up);
	R.Left = Dir(Left);
	R.Pelvis = CS(FVector(L->BodyStandoff, 0.f, FeetZ + HipsAboveFeet));
	R.PelvisWeight = 1.f;
	R.TorsoPitch = LeanDeg;
	R.bAutoLean = true;
	for (int32 S = 0; S < 2; ++S)
	{
		// hands: closed round the rung from above and behind, fingers over its top, the palm toward the ladder
		FBikeRiderPose::FHand& H = R.Hands[S];
		H.Grip = CS(Limbs.At[S]);
		H.Axis = Dir(Along);
		H.Radius = L->RungRadius;
		H.PalmNormal = Dir(Fwd - 0.45f * Up);
		H.FingerDir = Dir(Up + 0.45f * Fwd);
		H.Weight = 1.f;
		H.Close = Limbs.Close[S];
		// feet: the ball on the rung, toes into the ladder, the knees forward and a little out
		FBikeRiderPose::FFoot& F = R.Feet[S];
		F.Ball = CS(Limbs.At[2 + S]);
		F.Toes = Dir(Limbs.bOnFloor[S] ? Fwd : Fwd - 0.12f * Up);
		F.Pole = Dir(Fwd + (S == 0 ? 0.3f : -0.3f) * Left);
		F.Weight = 1.f;
	}

	// a hand or a boot meeting its rung
	for (int32 I = 0; I < 4; ++I)
	{
		const bool bMoving = Limbs.Moving[I] > 0.f;
		if (bWasMoving[I] && !bMoving && LastRung[I] != INT32_MIN && L->GripSound != NAME_None && Weight > 0.5f)
		{
			if (ULyudSoundscapeSubsystem* Snd = ULyudSoundscapeSubsystem::Get(this))
			{
				Snd->PlayEffect(L->GripSound, LT.TransformPosition(Limbs.At[I]), L->GripVolume * (I < 2 ? 0.7f : 1.f));
			}
		}
		bWasMoving[I] = bMoving;
		LastRung[I] = Limbs.Rung[I];
	}
}

// ------------------------------------------------------------------ the player's climb

void ULyudLadderClimber::GrabByPlayer(ULyudLadderComponent* InLadder)
{
	ACharacter* C = Cast<ACharacter>(GetOwner());
	if (!C || !InLadder)
	{
		return;
	}
	// from where the feet are now: on the floor, or (from above) at the top
	const float Z = InLadder->FromWorld(C->GetActorLocation() - FVector(0.f, 0.f, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight())).Z;
	const float Start = FMath::Clamp(Z, 0.f, InLadder->TopRungZ() - HandAboveFeet + InLadder->RungSpacing);
	UCharacterMovementComponent* Move = C->GetCharacterMovement();
	Move->StopMovementImmediately();
	Move->SetMovementMode(MOVE_None);
	bPlayerDriven = true;
	ClimbInput = 0.f;
	Attach(InLadder, Start, 0.35f);
}

void ULyudLadderClimber::LetGo()
{
	if (!bPlayerDriven)
	{
		return;
	}
	EndPlayerClimb();
}

void ULyudLadderClimber::EndPlayerClimb()
{
	bPlayerDriven = false;
	bStepping = false;
	Release(0.3f);
	if (ACharacter* C = Cast<ACharacter>(GetOwner()))
	{
		C->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	}
}

void ULyudLadderClimber::StepOff(const FVector& ToFeet)
{
	ACharacter* C = Cast<ACharacter>(GetOwner());
	if (!C)
	{
		return;
	}
	bStepping = true;
	StepT = 0.f;
	StepFrom = C->GetActorLocation();
	StepTo = ToFeet + FVector(0.f, 0.f, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	Release(0.7f);
}

void ULyudLadderClimber::TickPlayer(float DeltaTime)
{
	ACharacter* C = Cast<ACharacter>(GetOwner());
	ULyudLadderComponent* L = Ladder.Get();
	if (!C || !L)
	{
		bPlayerDriven = false;
		return;
	}
	if (bStepping)
	{
		// over the top (or off the bottom): a short arc onto the exit
		StepT = FMath::Min(1.f, StepT + DeltaTime / 0.8f);
		const float A = StepT * StepT * (3.f - 2.f * StepT);
		C->SetActorLocation(FMath::Lerp(StepFrom, StepTo, A) + FVector(0.f, 0.f, 25.f * FMath::Sin(PI * A)), false, nullptr, ETeleportType::TeleportPhysics);
		if (StepT >= 1.f)
		{
			bStepping = false;
			bPlayerDriven = false;
			C->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		}
		return;
	}
	if (bReleasing)
	{
		return;
	}
	if (CVarLadderAuto.GetValueOnGameThread() != 0.f)
	{
		ClimbInput = FMath::Clamp(CVarLadderAuto.GetValueOnGameThread(), -1.f, 1.f);
	}
	const float Speed = L->ClimbRungsPerSecond * L->RungSpacing;
	const float Top = L->TopRungZ() - HandAboveFeet + L->RungSpacing;
	FeetZ += ClimbInput * Speed * DeltaTime * FMath::Clamp(Weight * 1.5f, 0.f, 1.f);
	if (FeetZ <= 0.f && ClimbInput < 0.f)
	{
		// down on the floor: off the ladder, standing
		FeetZ = 0.f;
		Place();
		EndPlayerClimb();
		C->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		return;
	}
	if (FeetZ >= Top)
	{
		FeetZ = Top;
		if (ClimbInput > 0.f)
		{
			StepOff(L->ToWorld(L->TopExit + FVector(0.f, 0.f, L->TopRungZ() + L->RungSpacing * 0.5f)));
			return;
		}
	}
	Place();
	ClimbInput = 0.f;
}

// ------------------------------------------------------------------ the prompt and E

ULyudLadderSubsystem* ULyudLadderSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<ULyudLadderSubsystem>() : nullptr;
}

ULyudLadderComponent* ULyudLadderSubsystem::Nearest(const FVector& At) const
{
	ULyudLadderComponent* Best = nullptr;
	double BestD = TNumericLimits<double>::Max();
	for (const TWeakObjectPtr<ULyudLadderComponent>& W : Ladders)
	{
		ULyudLadderComponent* L = W.Get();
		if (L && L->bInteractive && FVector::DistSquared(L->GetComponentLocation(), At) < BestD)
		{
			BestD = FVector::DistSquared(L->GetComponentLocation(), At);
			Best = L;
		}
	}
	return Best;
}

bool ULyudLadderSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULyudLadderSubsystem::Tick(float DeltaTime)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	ACharacter* Hero = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!Hero || Ladders.Num() == 0)
	{
		return;
	}
	ULyudLadderClimber* Climber = ULyudLadderClimber::Get(Hero, false);
	if (Climber && Climber->IsPlayerClimbing())
	{
		if (PC->WasInputKeyJustPressed(EKeys::SpaceBar))
		{
			Climber->LetGo();
		}
		if (bPrompt)
		{
			bPrompt = false;
			if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
			{
				UI->SetPrompt(this, FText::GetEmpty());
			}
		}
		return;
	}
	// the interactive ladder the player stands at and looks toward
	const FVector Eye = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : Hero->GetPawnViewLocation();
	const FVector Look = PC->GetControlRotation().Vector();
	ULyudLadderComponent* Best = nullptr;
	float BestD = 130.f;
	for (int32 I = Ladders.Num() - 1; I >= 0; --I)
	{
		ULyudLadderComponent* L = Ladders[I].Get();
		if (!L)
		{
			Ladders.RemoveAtSwap(I);
			continue;
		}
		if (!L->bInteractive)
		{
			continue;
		}
		const FVector Local = L->FromWorld(Eye);
		if (Local.X < -10.f || Local.X > 140.f || Local.Z < -20.f || Local.Z > L->TopRungZ() + 220.f || FMath::Abs(Local.Y) > L->HalfWidth + 50.f)
		{
			continue;
		}
		if (FVector::DotProduct(Look, L->Facing()) < 0.2f)
		{
			continue;
		}
		if (Local.X < BestD)
		{
			BestD = float(Local.X);
			Best = L;
		}
	}
	const bool bShow = Best != nullptr;
	if (bShow != bPrompt)
	{
		bPrompt = bShow;
		if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
		{
			UI->SetPrompt(this, bShow ? LyudText::Get(TEXT("ui.ladder.climb")) : FText::GetEmpty());
		}
	}
	if (Best && PC->WasInputKeyJustPressed(EKeys::E))
	{
		ULyudLadderClimber::Get(Hero)->GrabByPlayer(Best);
	}
}

// ------------------------------------------------------------------ a ladder that draws itself

ALyudLadder::ALyudLadder()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Ladder = CreateDefaultSubobject<ULyudLadderComponent>(TEXT("Ladder"));
	Ladder->SetupAttachment(RootComponent);
	Rails = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Rails"));
	Rails->SetupAttachment(RootComponent);
	Rungs = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Rungs"));
	Rungs->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Rails->SetStaticMesh(Cube.Object);
	Rungs->SetStaticMesh(Cylinder.Object);
}

void ALyudLadder::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rails->ClearInstances();
	Rungs->ClearInstances();
	const ULyudLadderComponent* L = Ladder;
	const float Height = L->TopRungZ() + L->FirstRungZ;
	// the basic shapes are 100 cm: rails 5 x 3 cm, rungs RungRadius thick across the rails
	for (const float Y : { -L->HalfWidth - 1.5f, L->HalfWidth + 1.5f })
	{
		Rails->AddInstance(FTransform(FQuat::Identity, FVector(-1.f, Y, Height * 0.5f), FVector(0.05f, 0.03f, Height / 100.f)));
	}
	for (int32 I = 0; I < L->RungCount; ++I)
	{
		const float D = L->RungRadius * 2.f / 100.f;
		Rungs->AddInstance(FTransform(FRotator(0.f, 0.f, 90.f), FVector(0.f, 0.f, L->RungZ(I)), FVector(D, D, (L->HalfWidth * 2.f) / 100.f)));
	}
	if (Material)
	{
		Rails->SetMaterial(0, Material);
		Rungs->SetMaterial(0, Material);
	}
}
