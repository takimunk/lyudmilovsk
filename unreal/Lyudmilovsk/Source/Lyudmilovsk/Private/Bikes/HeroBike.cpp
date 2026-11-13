#include "Bikes/HeroBike.h"

#include "Bikes/LyudBikeSubsystem.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroAnimInstance.h"
#include "HeroCharacter.h"
#include "HeroReachComponent.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Railway/LyudTrain.h"
#include "Sea/LyudSea.h"
#include "Transport/LyudCableway.h"

namespace
{
	TAutoConsoleVariable<int32> CVarBikeLog(TEXT("lyud.Bike.Log"), 0, TEXT("1: the hero's bike logs its state once a second"));

	/** Riding: top speeds (cm/s) pedalling and pedalling hard, the push of the legs, brakes, the air and the tyres. */
	constexpr float CruiseTop = 750.f;
	constexpr float SprintTop = 1150.f;
	constexpr float PedalAccel = 190.f;
	constexpr float BrakeDecel = 520.f;
	constexpr float RollDrag = 9.f;
	constexpr float AirDrag = 0.00011f;
	/** The steering: most the bars turn standing, the sideways grip a turn may ask of the tyres (cm/s2), the wheelbase. */
	constexpr float MaxSteer = 32.f;
	constexpr float Grip = 420.f;
	constexpr float Wheelbase = 100.f;
	constexpr float MountSeconds = 1.7f;
	constexpr float DismountSeconds = 1.4f;
	/** Getting on: the right pedal forward and up (the cranks go backwards to it freely). */
	constexpr float MountCrank = -40.f;
	/** Where the hero stands to get on or off, from the bike's ground point (its left side). */
	const FVector StandOff(-6.f, -40.f, 0.f);

	/** A floor that goes away with whoever stands on it: a cable-car cabin, a station's lift, a train's car. The bike
	 *  keeps its own place in the world (it isn't carried), so it is not ridden there: walked in by the bars only. */
	bool IsCarryingFloor(const AActor* Under)
	{
		return Under && (Under->IsA<ALyudCabin>() || Under->IsA<ALyudStationLift>() || Under->IsA<ALyudTrain>());
	}
}

UHeroBikeComponent::UHeroBikeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UHeroBikeComponent* UHeroBikeComponent::Get(const AActor* Hero)
{
	if (!Hero)
	{
		return nullptr;
	}
	// asked several times a frame (the bikes, the hero's tick): the last answer is kept (per thread, so any caller is safe);
	// a hero without the component is searched again each time, as before
	static thread_local TWeakObjectPtr<const AActor> CachedHero;
	static thread_local TWeakObjectPtr<UHeroBikeComponent> CachedBike;
	if (CachedHero.Get() == Hero)
	{
		UHeroBikeComponent* B = CachedBike.Get();
		if (B && B->GetOwner() == Hero)
		{
			return B;
		}
	}
	UHeroBikeComponent* B = Hero->FindComponentByClass<UHeroBikeComponent>();
	if (B)
	{
		CachedHero = Hero;
		CachedBike = B;
	}
	return B;
}

AHeroCharacter* UHeroBikeComponent::Hero() const
{
	return Cast<AHeroCharacter>(GetOwner());
}

ALyudBike* UHeroBikeComponent::BikeInView() const
{
	const AHeroCharacter* H = Hero();
	const APlayerController* PC = H ? Cast<APlayerController>(H->GetController()) : nullptr;
	const ULyudBikeSubsystem* Bikes = ULyudBikeSubsystem::Get(this);
	if (!PC || !Bikes)
	{
		return nullptr;
	}
	FVector Eye;
	FRotator View;
	PC->GetPlayerViewPoint(Eye, View);
	// the bike the view ray meets within reach, else the nearest one close in front of the feet (looking down at it)
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BikeView), false, H);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + View.Vector() * 260.f, ECC_Visibility, Q))
	{
		if (ALyudBike* B = Cast<ALyudBike>(Hit.GetActor()); B && Bikes->IsFree(B))
		{
			return B;
		}
	}
	ALyudBike* Best = nullptr;
	float BestD = 170.f;
	for (ALyudBike* B : Bikes->GetBikes())
	{
		if (!B || !Bikes->IsFree(B))
		{
			continue;
		}
		const FVector To = B->GetActorLocation() - H->GetActorLocation();
		const float D = (float)To.Size2D();
		if (D < BestD && FVector::DotProduct(To.GetSafeNormal2D(), View.Vector().GetSafeNormal2D()) > 0.3f)
		{
			Best = B;
			BestD = D;
		}
	}
	return Best;
}

bool UHeroBikeComponent::Mount(ALyudBike* Which)
{
	AHeroCharacter* H = Hero();
	if (!Which)
	{
		Which = Bike.IsValid() ? Bike.Get() : BikeInView();
	}
	if (!H || !Which || (Which->IsTaken() && Which->GetHolder() != H) || H->IsSeated())
	{
		return false;
	}
	// not in a cabin, a lift or a train: it would leave from under the bike with the hero held on the saddle in the air
	if (OnCarryingFloor(Act == ELyudBikeAct::Lead ? Ground : Which->GetActorLocation(), Which))
	{
		return false;
	}
	if (Act != ELyudBikeAct::Lead)
	{
		Ground = Which->GetActorLocation();
		Yaw = (float)Which->GetActorRotation().Yaw;
		StartRoll = Which->ParkedLean != 0.f ? Which->ParkedLean : -ALyudBike::Geom().ParkRoll;
		StandDown = Which->ParkedLean != 0.f ? 0.f : 1.f;
		Speed = 0.f;
		Steer = 0.f;
	}
	else
	{
		StartRoll = LeanDeg;
	}
	Bike = Which;
	Which->SetHolder(H);
	if (ULyudBikeSubsystem* Bikes = ULyudBikeSubsystem::Get(this))
	{
		Bikes->HeroHas(Which, true);
	}
	Act = ELyudBikeAct::Mount;
	Phase = 0.f;
	Weight = 1.f;
	bWantOff = false;
	FootDown = 0.f;
	// the body leaves its feet: no walking, no collision of its own, the camera eases over to the bike
	UCharacterMovementComponent* Move = H->GetCharacterMovement();
	Move->StopMovementImmediately();
	Move->SetMovementMode(MOVE_None);
	if (H->GetCapsuleComponent()->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		HeroCollision = H->GetCapsuleComponent()->GetCollisionEnabled();
	}
	H->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshRelative = H->GetMesh()->GetRelativeTransform();
	if (H->bIsCrouched)
	{
		H->UnCrouch();
	}
	H->SetCrouchToggled(false);
	EyeEase = 0.f;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(GetWorld()))
	{
		UI->SetPrompt(this, FText::GetEmpty());
	}
	return true;
}

void UHeroBikeComponent::Dismount()
{
	if (Act == ELyudBikeAct::Ride)
	{
		bWantOff = true;
	}
}

bool UHeroBikeComponent::Lead(ALyudBike* Which)
{
	AHeroCharacter* H = Hero();
	if (!Which)
	{
		Which = BikeInView();
	}
	if (!H || !Which || Which->IsTaken() || IsOnBike() || H->IsSeated())
	{
		return false;
	}
	Bike = Which;
	Which->SetHolder(H);
	if (ULyudBikeSubsystem* Bikes = ULyudBikeSubsystem::Get(this))
	{
		Bikes->HeroHas(Which, true);
	}
	Act = ELyudBikeAct::Lead;
	Weight = 0.f;
	Ground = Which->GetActorLocation();
	Yaw = (float)Which->GetActorRotation().Yaw;
	LeanDeg = Which->ParkedLean != 0.f ? Which->ParkedLean : -ALyudBike::Geom().ParkRoll;
	StandDown = Which->ParkedLean != 0.f ? 0.f : 1.f;
	Speed = 0.f;
	return true;
}

void UHeroBikeComponent::LetGo()
{
	if (Act == ELyudBikeAct::Lead)
	{
		Release(false);
	}
}

bool UHeroBikeComponent::DropNow()
{
	if (Act == ELyudBikeAct::None)
	{
		return false;
	}
	Release(true);
	// nothing of the ride is left over for the next frames: no braking to a stop, no held input, and the eye doesn't
	// glide from where the bike was to wherever the hero is put next (SmoothEye starts again from the new eye)
	bWantOff = false;
	Speed = 0.f;
	Steer = 0.f;
	Pitch = 0.f;
	AutoUntil = 0.f;
	bMoveInput = false;
	MoveInput = FVector2D::ZeroVector;
	bEyeValid = false;
	EyeEase = 1.f;
	return true;
}

bool UHeroBikeComponent::DropBike(AActor* OfHero)
{
	UHeroBikeComponent* C = Get(OfHero);
	return C && C->DropNow();
}

bool UHeroBikeComponent::OnCarryingFloor(const FVector& At, const ALyudBike* Which) const
{
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BikeFloor), false, GetOwner());
	if (Which)
	{
		Q.AddIgnoredActor(Which);
	}
	return GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 70.f), At - FVector(0.f, 0.f, 260.f), ECC_Visibility, Q)
		&& IsCarryingFloor(Hit.GetActor());
}

void UHeroBikeComponent::UpdateKeys()
{
	AHeroCharacter* H = Hero();
	APlayerController* PC = H ? Cast<APlayerController>(H->GetController()) : nullptr;
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(GetWorld());
	if (!PC)
	{
		return;
	}
	const bool bFree = !PC->IsMoveInputIgnored() && !(UI && UI->IsOpen());
	const UHeroReachComponent* Reach = UHeroReachComponent::Get(H);
	const bool bBusy = (Reach && Reach->IsBusy()) || H->IsSeated() || H->IsHidden();
	const bool bE = bFree && PC->WasInputKeyJustPressed(EKeys::E);
	const bool bR = bFree && PC->WasInputKeyJustPressed(EKeys::R);
	FText Prompt;
	switch (Act)
	{
	case ELyudBikeAct::None:
		if (!bBusy)
		{
			if (ALyudBike* B = BikeInView())
			{
				Prompt = LyudText::Get(TEXT("ui.bike.prompt"));
				if (bE)
				{
					Mount(B);
				}
				else if (bR)
				{
					Lead(B);
				}
			}
		}
		break;
	case ELyudBikeAct::Lead:
		Prompt = LyudText::Get(TEXT("ui.bike.prompt_lead"));
		if (bE)
		{
			Mount(Bike.Get());
		}
		else if (bR)
		{
			LetGo();
		}
		break;
	case ELyudBikeAct::Ride:
		if (Speed < 60.f)
		{
			Prompt = LyudText::Get(TEXT("ui.bike.prompt_off"));
		}
		if (bE)
		{
			Dismount();
		}
		break;
	default:
		break;
	}
	if (UI && !(IsOnBike() && Act != ELyudBikeAct::Ride))
	{
		UI->SetPrompt(this, Prompt);
	}
}

void UHeroBikeComponent::HeroTick(float DeltaSeconds)
{
	AHeroCharacter* H = Hero();
	if (!H)
	{
		return;
	}
	UpdateKeys();
	if (Act != ELyudBikeAct::None && !Bike.IsValid())
	{
		Release(true);
	}
	switch (Act)
	{
	case ELyudBikeAct::Mount:
		Phase = FMath::Min(1.f, Phase + DeltaSeconds / MountSeconds);
		// the bike comes upright as the hero takes it, the stand kicks up once he sits
		LeanDeg = FMath::Lerp(StartRoll, 0.f, FMath::SmoothStep(0.05f, 0.45f, Phase));
		StandDown = FMath::Min(StandDown, 1.f - FMath::SmoothStep(0.72f, 0.92f, Phase));
		Bike->Roll(DeltaSeconds, 0.f, Steer * (1.f - Phase), false, MountCrank, bLoose, false);
		Follow(DeltaSeconds);
		if (Phase >= 1.f)
		{
			Act = ELyudBikeAct::Ride;
			Phase = 0.f;
		}
		break;
	case ELyudBikeAct::Ride:
		Ride(DeltaSeconds);
		break;
	case ELyudBikeAct::Dismount:
		Phase = FMath::Min(1.f, Phase + DeltaSeconds / DismountSeconds);
		LeanDeg = FMath::Lerp(StartRoll, -ALyudBike::Geom().ParkRoll, FMath::SmoothStep(0.75f, 1.f, Phase));
		StandDown = FMath::SmoothStep(0.05f, 0.3f, Phase);
		Bike->Roll(DeltaSeconds, 0.f, FMath::Lerp(0.f, -12.f, Phase), false, MountCrank, bLoose, false);
		Follow(DeltaSeconds);
		if (Phase >= 1.f)
		{
			Release(true);
		}
		break;
	case ELyudBikeAct::Lead:
	{
		// the bike on the hero's right, the bars a step ahead, heading the way he walks
		const FVector Vel = H->GetVelocity();
		const float V = (float)Vel.Size2D();
		const float BodyYaw = (float)H->GetActorRotation().Yaw;
		const float Want = V > 30.f ? (float)Vel.Rotation().Yaw : BodyYaw;
		const float D = FMath::FindDeltaAngleDegrees(Yaw, Want);
		Yaw += D * FMath::Min(1.f, DeltaSeconds * 4.f);
		Weight = FMath::Min(1.f, Weight + DeltaSeconds * 3.f);
		const FRotator R(0.f, Yaw, 0.f);
		const FVector Feet = H->GetActorLocation() - FVector(0.f, 0.f, H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		const FVector Want3 = Feet + R.RotateVector(FVector(-ALyudBike::Geom().BarTops[0].X + 42.f, 40.f, 0.f));
		Ground = FMath::Lerp(Ground, Want3, FMath::Min(1.f, DeltaSeconds * 8.f * FMath::Max(0.25f, Weight)));
		LeanDeg = FMath::FInterpTo(LeanDeg, -3.f, DeltaSeconds, 3.f);
		StandDown = FMath::FInterpConstantTo(StandDown, 0.f, DeltaSeconds, 3.f);
		Speed = V;
		float Z = Ground.Z;
		bool bL = false;
		if (GroundAt(Ground, Z, bL))
		{
			Ground.Z = Z;
			bLoose = bL;
		}
		Bike->Place(Ground, Yaw, 0.f, LeanDeg, StandDown);
		Bike->Roll(DeltaSeconds, V * FMath::Cos(FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(Yaw, (float)Vel.Rotation().Yaw))), FMath::Clamp(D * 0.6f, -25.f, 25.f), false, -1.f, bLoose, false);
		FeedBody();
		break;
	}
	default:
		break;
	}
	bMoveInput = false;
	MoveInput = FVector2D::ZeroVector;
	if (CVarBikeLog.GetValueOnGameThread() && Bike.IsValid())
	{
		static double Last = 0.0;
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now - Last > 1.0)
		{
			Last = Now;
			UE_LOG(LogTemp, Warning, TEXT("HEROBIKE act %d phase %.2f speed %.0f steer %.1f lean %.1f pitch %.1f yaw %.0f at %s crank %.0f"), (int32)Act, Phase, Speed, Steer, LeanDeg, Pitch,
				Yaw, *Ground.ToCompactString(), Bike->GetCrank());
		}
	}
}

bool UHeroBikeComponent::GroundAt(const FVector& At, float& OutZ, bool& bOutLoose) const
{
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BikeGround), false, GetOwner());
	if (Bike.IsValid())
	{
		Q.AddIgnoredActor(Bike.Get());
	}
	if (!GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 70.f), At - FVector(0.f, 0.f, 260.f), ECC_Visibility, Q))
	{
		return false;
	}
	// a cabin's, a lift's or a car's floor is no ground for the wheels (Ride stops before it, as before a wall)
	if (IsCarryingFloor(Hit.GetActor()))
	{
		return false;
	}
	OutZ = (float)Hit.ImpactPoint.Z;
	// the town's landscape is earth, grass and gravel; paths, roads, floors and decks are hard
	const UPrimitiveComponent* C = Hit.GetComponent();
	// the class's name is looked at once per class, not made into a string on every trace
	static const UClass* LastClass = nullptr;
	static bool bLastLoose = false;
	if (C && C->GetClass() != LastClass)
	{
		LastClass = C->GetClass();
		bLastLoose = LastClass->GetName().Contains(TEXT("Landscape"));
	}
	bOutLoose = C && bLastLoose;
	return true;
}

void UHeroBikeComponent::Ride(float DeltaSeconds)
{
	AHeroCharacter* H = Hero();
	APlayerController* PC = Cast<APlayerController>(H->GetController());
	float Throttle = 0.f, SteerIn = 0.f;
	if (bMoveInput)
	{
		Throttle = MoveInput.Y;
		SteerIn = MoveInput.X;
	}
	if (AutoUntil > 0.f)
	{
		AutoUntil -= DeltaSeconds;
		Throttle = AutoThrottle;
		SteerIn = AutoSteer;
	}
	bSprint = PC && (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift)) && Throttle > 0.1f;
	if (bWantOff)
	{
		// getting off: brakes first, then down on the left
		Throttle = -1.f;
		SteerIn = 0.f;
	}
	bPedal = Throttle > 0.1f;
	bBrake = Throttle < -0.1f;
	const float Top = bSprint ? SprintTop : CruiseTop;
	float A = -(RollDrag + AirDrag * Speed * Speed) * (bLoose ? 2.2f : 1.f);
	if (bPedal)
	{
		A += PedalAccel * (bSprint ? 1.5f : 1.f) * Throttle * FMath::Clamp(1.f - Speed / Top, 0.f, 1.f) + (Speed < Top ? RollDrag : 0.f);
	}
	if (bBrake)
	{
		A -= BrakeDecel * -Throttle;
	}
	A -= 980.f * FMath::Sin(FMath::DegreesToRadians(Pitch));
	Speed = FMath::Clamp(Speed + A * DeltaSeconds, 0.f, SprintTop * 1.3f);

	// steering: less of the bars the faster it goes (the tyres' grip), the bars come back by themselves
	const float V2 = FMath::Max(Speed * Speed, 1.f);
	const float Limit = FMath::Min(MaxSteer, FMath::RadiansToDegrees(FMath::Atan(Wheelbase * Grip / V2)));
	const float WantSteer = SteerIn * Limit;
	Steer = FMath::FInterpConstantTo(Steer, WantSteer, DeltaSeconds, SteerIn != 0.f ? 70.f : 50.f);
	const float YawRate = Speed * FMath::Tan(FMath::DegreesToRadians(Steer)) / Wheelbase;       // rad/s
	// leaning into the turn as far as speed and turn ask (a body's balance), a little slower than the turn itself
	const float WantLean = FMath::RadiansToDegrees(FMath::Atan(Speed * YawRate / 980.f));
	// stopped: a foot down on the left, the bike leaning onto it
	Still = Speed < 25.f && !bPedal ? Still + DeltaSeconds : 0.f;
	FootDown = FMath::FInterpConstantTo(FootDown, Still > 0.35f ? 1.f : 0.f, DeltaSeconds, Still > 0.35f ? 2.5f : 4.f);
	LeanDeg = FMath::FInterpTo(LeanDeg, FMath::Clamp(WantLean, -35.f, 35.f) * (1.f - FootDown) - 9.f * FootDown, DeltaSeconds, 5.f);

	// move: the ground under both wheels, blocked by walls, high kerbs and deep water
	const FRotator R0(0.f, Yaw, 0.f);
	const float NewYaw = Yaw + FMath::RadiansToDegrees(YawRate * DeltaSeconds);
	const FRotator R1(0.f, NewYaw, 0.f);
	FVector Next = Ground + R1.Vector() * Speed * DeltaSeconds;
	const FLyudBikeGeom& G = ALyudBike::Geom();
	float ZF = Next.Z, ZR = Next.Z;
	bool bLF = false, bLR = false;
	const FVector FrontAt = Next + R1.RotateVector(FVector(G.AxleF.X, 0.f, 0.f));
	const FVector RearAt = Next + R1.RotateVector(FVector(G.AxleR.X, 0.f, 0.f));
	const bool bFront = GroundAt(FrontAt, ZF, bLF);
	const bool bRear = GroundAt(RearAt, ZR, bLR);
	bool bBlocked = !bFront || !bRear;
	const float OldFront = Ground.Z + FMath::Sin(FMath::DegreesToRadians(Pitch)) * G.AxleF.X;
	if (bFront && ZF - OldFront > 13.f)
	{
		bBlocked = true;          // a step or a kerb: no bunny hops
	}
	if (!bBlocked && Speed > 1.f)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BikeSweep), false, H);
		Q.AddIgnoredActor(Bike.Get());
		FCollisionObjectQueryParams Obj;
		Obj.AddObjectTypesToQuery(ECC_WorldStatic);
		Obj.AddObjectTypesToQuery(ECC_WorldDynamic);
		const FVector Z(0.f, 0.f, 45.f);
		const FVector From = Ground + R0.RotateVector(FVector(G.AxleF.X + 10.f, 0.f, 0.f)) + Z;
		const FVector To = Next + R1.RotateVector(FVector(G.AxleF.X + 10.f, 0.f, 0.f)) + Z;
		if (GetWorld()->SweepSingleByObjectType(Hit, From, To, FQuat::Identity, Obj, FCollisionShape::MakeSphere(22.f), Q) && !Hit.bStartPenetrating)
		{
			bBlocked = true;
		}
	}
	if (!bSeaLooked)
	{
		bSeaLooked = true;
		for (TActorIterator<ALyudSea> It(GetWorld()); It; ++It)
		{
			Sea = *It;
		}
	}
	if (!bBlocked && Sea && Sea->SurfaceZ(FrontAt) - ZF > 30.f)
	{
		bBlocked = true;          // the sea: no further than the wheels' depth
	}
	if (bBlocked)
	{
		Speed = 0.f;
		Next = Ground;
	}
	else
	{
		Yaw = FRotator::NormalizeAxis(NewYaw);
		const float Run = G.AxleF.X - G.AxleR.X;
		Next.Z = ZR + (ZF - ZR) * (-G.AxleR.X / Run);
		Pitch = FMath::FInterpTo(Pitch, FMath::RadiansToDegrees(FMath::Atan2(ZF - ZR, Run)), DeltaSeconds, 12.f);
		bLoose = bLF || bLR;
		Ground = Next;
	}
	StandDown = 0.f;
	const bool bCoast = !bPedal;
	CoastTime = bCoast ? CoastTime + DeltaSeconds : 0.f;
	// coasting the rider levels the pedals (the right one forward); stopped with a foot down, the left pedal down
	const float CrankTo = FootDown > 0.3f ? 90.f : (CoastTime > 0.6f ? 0.f : -1.f);
	Bike->Place(Ground, Yaw, Pitch, LeanDeg, 0.f);
	Bike->Roll(DeltaSeconds, Speed, Steer, bPedal, CrankTo, bLoose, bBrake);
	PlaceHero();
	if (bWantOff && Speed < 15.f)
	{
		bWantOff = false;
		Act = ELyudBikeAct::Dismount;
		Phase = 0.f;
		StartRoll = LeanDeg;
		Speed = 0.f;
		Bike->Hush();
	}
}

void UHeroBikeComponent::Follow(float DeltaSeconds)
{
	Bike->Place(Ground, Yaw, Pitch, LeanDeg, StandDown);
	PlaceHero();
}

void UHeroBikeComponent::PlaceHero()
{
	AHeroCharacter* H = Hero();
	ALyudBike* B = Bike.Get();
	if (!H || !B)
	{
		return;
	}
	// the capsule (no collision now) over the bike, facing its way; the body's mesh on the bike's ground frame (it leans
	// with it); the targets put the hips on the saddle
	const float Half = H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	H->SetBodyYaw(Yaw);
	H->SetActorLocationAndRotation(Ground + FVector(0.f, 0.f, Half), FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	H->GetMesh()->SetWorldTransform(FTransform(FRotator(0.f, -90.f, 0.f)) * B->GroundFrame(), false, nullptr, ETeleportType::TeleportPhysics);
	FeedBody();
}

void UHeroBikeComponent::FeedBody()
{
	AHeroCharacter* H = Hero();
	UHeroAnimInstance* Anim = H ? Cast<UHeroAnimInstance>(H->GetMesh()->GetAnimInstance()) : nullptr;
	if (!Anim)
	{
		return;
	}
	if (Act == ELyudBikeAct::None || !Bike.IsValid())
	{
		Anim->Bike = FBikeRiderPose();
		return;
	}
	FLyudBikeRide Ride;
	Ride.Act = Act;
	Ride.Phase = Phase;
	Ride.Weight = Weight;
	Ride.bSprint = bSprint;
	// SK_Hero's hips stand ~97 cm over the floor (the shared clips' body 93)
	Ride.Scale = 1.04f;
	Ride.HeadUp = 0.25f;
	Bike->RiderPose(H->GetMesh(), Ride, Anim->Bike);
	if (Act == ELyudBikeAct::Ride && FootDown > 0.f)
	{
		// stopped: forward off the saddle onto the top tube, the left foot down on the ground
		const FLyudBikeGeom& G = ALyudBike::Geom();
		const FTransform M = H->GetMesh()->GetComponentTransform();
		const FTransform BF = Bike->GroundFrame();
		const float FD = FMath::SmoothStep(0.f, 1.f, FootDown);
		const FVector Hips = M.InverseTransformPosition(BF.TransformPosition(G.SaddleTop + FVector(16.f, -3.f, -9.f)));
		Anim->Bike.Pelvis = FMath::Lerp(Anim->Bike.Pelvis, Hips, FD);
		FVector Foot = BF.TransformPosition(FVector(G.SaddleTop.X + 30.f, -36.f, 0.f));
		float Z;
		bool bL;
		if (GroundAt(Foot, Z, bL))
		{
			Foot.Z = Z;
		}
		Anim->Bike.Feet[0].Ball = FMath::Lerp(Anim->Bike.Feet[0].Ball, M.InverseTransformPosition(Foot), FD);
		Anim->Bike.Feet[0].Toes = FMath::Lerp(Anim->Bike.Feet[0].Toes, M.InverseTransformVectorNoScale(FRotator(0.f, Yaw, 0.f).Vector()), FD).GetSafeNormal();
		Anim->Bike.TorsoRoll = FMath::Lerp(Anim->Bike.TorsoRoll, 6.f, FD);
	}
}

void UHeroBikeComponent::Release(bool bStandBeside)
{
	AHeroCharacter* H = Hero();
	ALyudBike* B = Bike.Get();
	const bool bWasOn = Act == ELyudBikeAct::Mount || Act == ELyudBikeAct::Ride || Act == ELyudBikeAct::Dismount;
	Act = ELyudBikeAct::None;
	Phase = 0.f;
	Weight = 0.f;
	if (B)
	{
		B->SetHolder(nullptr);
		B->Park(Ground, Yaw, 0.f);
		B->bMoved = true;
		if (ULyudBikeSubsystem* Bikes = ULyudBikeSubsystem::Get(this))
		{
			Bikes->HeroHas(B, false);
		}
	}
	Bike.Reset();
	if (!H)
	{
		return;
	}
	if (bWasOn)
	{
		// back on his feet beside it (on its left, or its right if the left is a wall)
		const float Half = H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FRotator R(0.f, Yaw, 0.f);
		FVector At = Ground + R.RotateVector(StandOff) + FVector(0.f, 0.f, Half + 2.f);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BikeStand), false, H);
		if (B)
		{
			Q.AddIgnoredActor(B);
		}
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(H->GetCapsuleComponent()->GetScaledCapsuleRadius(), Half);
		if (GetWorld()->OverlapBlockingTestByChannel(At, FQuat::Identity, ECC_Pawn, Shape, Q))
		{
			const FVector Other = Ground + R.RotateVector(StandOff * FVector(1.f, -1.f, 1.f)) + FVector(0.f, 0.f, Half + 2.f);
			if (!GetWorld()->OverlapBlockingTestByChannel(Other, FQuat::Identity, ECC_Pawn, Shape, Q))
			{
				At = Other;
			}
		}
		H->GetMesh()->SetRelativeTransform(MeshRelative);
		H->SetActorLocationAndRotation(At, R, false, nullptr, ETeleportType::TeleportPhysics);
		H->SetBodyYaw(Yaw);
		H->GetCapsuleComponent()->SetCollisionEnabled(HeroCollision);
		H->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	FeedBody();
}

FVector UHeroBikeComponent::SmoothEye(const FVector& Eye, float DeltaSeconds)
{
	// getting on and off the camera glides over (a second or so); riding it is the eyes exactly
	const bool bGlide = Act == ELyudBikeAct::Mount || Act == ELyudBikeAct::Dismount || EyeEase < 1.f;
	FVector Want = Eye;
	if (IsOnBike() && Bike.IsValid())
	{
		// leaning over the bars the eyes come forward of the collar a little (the camera would see the overshirt's inside)
		Want += Bike->GetActorForwardVector() * 7.f * (Act == ELyudBikeAct::Ride ? 1.f : (Act == ELyudBikeAct::Dismount ? 1.f - Phase : Phase));
	}
	if (!bEyeValid || !bGlide)
	{
		LastEye = Want;
		bEyeValid = true;
		if (!IsOnBike())
		{
			EyeEase = 1.f;
		}
		return Want;
	}
	EyeEase = FMath::Min(1.f, EyeEase + DeltaSeconds / 0.6f);
	LastEye = FMath::Lerp(LastEye, Want, FMath::Min(1.f, DeltaSeconds * FMath::Lerp(5.f, 40.f, EyeEase)));
	return LastEye;
}

// --- console: tests and checks without the keys ---
static FAutoConsoleCommandWithWorldAndArgs GBikeCmd(TEXT("Bike.Hero"),
	TEXT("Bike.Hero mount|off|lead|letgo|ride <throttle> <steer> <seconds>: the hero and the nearest bike (tests)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		AHeroCharacter* Hero = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
		UHeroBikeComponent* C = UHeroBikeComponent::Get(Hero);
		ULyudBikeSubsystem* Bikes = ULyudBikeSubsystem::Get(World);
		if (!C || !Bikes || Args.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("BIKETEST no hero bike component"));
			return;
		}
		const FString Cmd = Args[0];
		ALyudBike* Near = Bikes->Nearest(Hero->GetActorLocation(), 100000.f);
		if (Cmd == TEXT("mount"))
		{
			UE_LOG(LogTemp, Warning, TEXT("BIKETEST mount %d"), C->Mount(C->GetBike() ? C->GetBike() : Near) ? 1 : 0);
		}
		else if (Cmd == TEXT("off"))
		{
			C->Dismount();
		}
		else if (Cmd == TEXT("lead"))
		{
			UE_LOG(LogTemp, Warning, TEXT("BIKETEST lead %d"), C->Lead(Near) ? 1 : 0);
		}
		else if (Cmd == TEXT("letgo"))
		{
			C->LetGo();
		}
		else if (Cmd == TEXT("ride") && Args.Num() >= 4)
		{
			C->SetAutopilot(FCString::Atof(*Args[1]), FCString::Atof(*Args[2]), FCString::Atof(*Args[3]));
		}
	}));
