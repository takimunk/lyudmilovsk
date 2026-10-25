#include "Birds/BirdActor.h"

#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "Birds/BirdAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Plants/GameCalendar.h"

namespace
{
	const TCHAR* AllClips[] = { TEXT("Idle"), TEXT("Look"), TEXT("Alert"), TEXT("Hop"), TEXT("Walk"), TEXT("Peck"), TEXT("Eat"), TEXT("Preen"),
		TEXT("Call"), TEXT("Takeoff"), TEXT("Flap"), TEXT("Glide"), TEXT("Bound"), TEXT("Land"), TEXT("Sleep") };
}

ABirdActor::ABirdActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetAnimInstanceClass(UBirdAnimInstance::StaticClass());
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Body->bCastDynamicShadow = true;
}

void ABirdActor::BeginPlay()
{
	Super::BeginPlay();
	Rng.Initialize(Seed ? Seed : GetUniqueID());
	if (SetSpecies(Species))
	{
		SetState(InitialState, 0.f);
		ApplyPlumage();   // now that the anim instance is up (the streamer length lives there)
	}
}

bool ABirdActor::SetSpecies(FName Id)
{
	const FBirdSpecies* S = UBirdSpeciesLibrary::Find(Id);
	USkeletalMesh* Mesh = bHeroBody ? UBirdSpeciesLibrary::LoadHeroMesh(Id) : nullptr;
	if (!Mesh)
	{
		Mesh = UBirdSpeciesLibrary::LoadSkeletalMesh(Id);
	}
	if (!S || !Mesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("Bird: no species or mesh for %s"), *Id.ToString());
		return false;
	}
	Species = Id;
	Data = *S;
	Body->SetSkinnedAssetAndUpdate(Mesh);
	Body->SetAnimInstanceClass(UBirdAnimInstance::StaticClass());
	Clips.Reset();
	for (const TCHAR* C : AllClips)
	{
		if (UAnimSequence* Seq = UBirdSpeciesLibrary::LoadClip(Id, C))
		{
			Clips.Add(C, Seq);
		}
	}
	ApplyPlumage();
	return true;
}

void ABirdActor::SetSex(EBirdSex NewSex)
{
	Sex = NewSex;
	ApplyPlumage();
}

void ABirdActor::ApplyPlumage()
{
	USkinnedAsset* Mesh = Body ? Body->GetSkinnedAsset() : nullptr;
	if (!Mesh)
	{
		return;
	}
	const bool bFemale = Sex == EBirdSex::Female;
	const bool bWinter = Data.InWinterPlumage(UGameCalendar::NowIn(GetWorld()).GetMonth());
	// the most specific material set that exists: FemaleWinter, Female or Winter, else the base
	TArray<FString> Suffixes;
	if (bFemale && Data.bFemalePlumage && bWinter)
	{
		Suffixes.Add(TEXT("_FemaleWinter"));
	}
	if (bFemale && Data.bFemalePlumage)
	{
		Suffixes.Add(TEXT("_Female"));
	}
	if (bWinter)
	{
		Suffixes.Add(TEXT("_Winter"));
	}
	const FString N = Species.ToString();
	const TArray<FSkeletalMaterial>& Slots = Mesh->GetMaterials();
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		UMaterialInterface* Base = Slots[i].MaterialInterface;
		UMaterialInterface* Use = Base;
		for (const FString& Sfx : Suffixes)
		{
			if (!Base)
			{
				break;
			}
			const FString P = FString::Printf(TEXT("/Game/Birds/%s/%s%s.%s%s"), *N, *Base->GetName(), *Sfx, *Base->GetName(), *Sfx);
			if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, *P, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				Use = M;
				break;
			}
		}
		Body->SetMaterial(i, Use);
	}
	Body->SetRelativeScale3D(FVector(bFemale ? Data.FemaleScale : 1.f));
	UE_LOG(LogTemp, Verbose, TEXT("Bird %s: %s, %d material sets tried"), *N, bFemale ? TEXT("female") : TEXT("male"), Suffixes.Num());
	if (UBirdAnimInstance* A = Anim())
	{
		A->StreamerScale = bFemale ? Data.FemaleStreamer : 1.f;
	}
}

UBirdAnimInstance* ABirdActor::Anim() const
{
	return Body ? Cast<UBirdAnimInstance>(Body->GetAnimInstance()) : nullptr;
}

UAnimSequence* ABirdActor::Clip(FName Name) const
{
	const TObjectPtr<UAnimSequence>* P = Clips.Find(Name);
	return P ? P->Get() : nullptr;
}

bool ABirdActor::IsOneShot(EBirdState S)
{
	switch (S)
	{
	case EBirdState::Peck:
	case EBirdState::Eat:
	case EBirdState::Preen:
	case EBirdState::Call:
	case EBirdState::Takeoff:
	case EBirdState::Land:
		return true;
	default:
		return false;
	}
}

void ABirdActor::SetState(EBirdState NewState, float BlendTime)
{
	if (!IsOneShot(State) && IsOneShot(NewState))
	{
		Resume = State;
	}
	if (NewState == EBirdState::Takeoff)
	{
		Resume = EBirdState::Fly;
	}
	else if (NewState == EBirdState::Land)
	{
		Resume = EBirdState::Perch;
	}
	State = NewState;
	StateTime = 0.f;
	StartClip(NewState, BlendTime);
}

void ABirdActor::StartClip(EBirdState S, float BlendTime)
{
	UBirdAnimInstance* A = Anim();
	if (!A)
	{
		return;
	}
	const FName Name = UBirdSpeciesLibrary::ClipForState(S);
	UAnimSequence* Seq = Clip(Name);
	if (!Seq)
	{
		Seq = Clip(TEXT("Idle"));
	}
	float Rate = 1.f;
	if (S == EBirdState::Fly)
	{
		Rate = Data.WingbeatHz;
		FlightPhase = 0.f;
		bFlightBurst = true;
		FlightSegment = 0.f;
	}
	// loops start somewhere random, so a flock doesn't breathe and turn its heads together
	A->Play(Name, Seq, Rate * (IsOneShot(S) ? 1.f : Rng.FRandRange(0.92f, 1.08f)), !IsOneShot(S), BlendTime, true);
	if (!IsOneShot(S))
	{
		for (FBirdClipLayer& L : A->Layers)
		{
			if (L.Clip == Name && Seq)
			{
				L.Time = Rng.FRandRange(0.f, Seq->GetPlayLength());
			}
		}
	}
}

void ABirdActor::SetLookTarget(FVector WorldPoint, bool bActive)
{
	LookWorld = WorldPoint;
	if (bActive && !bLookActive)
	{
		LookHold = 0.f;
	}
	bLookActive = bActive;
}

void ABirdActor::SetFlightInput(float Flap01, float Fold01, float Bank, float Pitch, bool bPlayerControl)
{
	bPlayer = bPlayerControl;
	InFlap = FMath::Clamp(Flap01, 0.f, 1.f);
	InFold = FMath::Clamp(Fold01, 0.f, 1.f);
	InBank = FMath::Clamp(Bank, -1.f, 1.f);
	InPitch = FMath::Clamp(Pitch, -1.f, 1.f);
	if (bPlayer && State != EBirdState::Fly && State != EBirdState::Glide)
	{
		SetState(EBirdState::Fly);
	}
}

FTransform ABirdActor::GetEyeTransform() const
{
	const USkinnedAsset* Mesh = Body ? Body->GetSkinnedAsset() : nullptr;
	const int32 HeadIndex = Mesh ? Mesh->GetRefSkeleton().FindBoneIndex(TEXT("head")) : INDEX_NONE;
	if (HeadIndex == INDEX_NONE)
	{
		return GetActorTransform();
	}
	const FVector L = Body->GetSocketLocation(TEXT("eye_l")), R = Body->GetSocketLocation(TEXT("eye_r"));
	// the head bone's axes as they were in the bind pose (forward +X, up +Z), now
	const FQuat Ref = FAnimationRuntime::GetComponentSpaceTransformRefPose(Mesh->GetRefSkeleton(), HeadIndex).GetRotation();
	const FQuat Head = Body->GetSocketQuaternion(TEXT("head"));
	const FVector Fwd = Head * Ref.Inverse().RotateVector(FVector::XAxisVector);
	const FVector Up = Head * Ref.Inverse().RotateVector(FVector::ZAxisVector);
	return FTransform(FRotationMatrix::MakeFromXZ(Fwd, Up).ToQuat(), (L + R) * 0.5f);
}

void ABirdActor::TickFlight(float DeltaSeconds)
{
	UBirdAnimInstance* A = Anim();
	if (!A)
	{
		return;
	}
	UAnimSequence* Flap = Clip(TEXT("Flap"));
	UAnimSequence* Glide = Clip(TEXT("Glide"));
	UAnimSequence* Bound = Clip(TEXT("Bound"));
	const float Climb = FMath::Clamp(Velocity.Z / 300.f, -1.f, 1.f);
	if (bPlayer)
	{
		const float Beat = Data.WingbeatHz * (0.7f + 0.5f * InFlap + 0.15f * Climb);
		A->Mix(TEXT("Flap"), Flap, InFlap * (1.f - InFold), Beat, 0.12f);
		A->Mix(TEXT("Glide"), Glide, (1.f - InFlap) * (1.f - InFold), 1.f, 0.2f);
		A->Mix(TEXT("Bound"), Bound, InFold, 1.f, 0.15f);
		A->BankDeg = InBank * 30.f;
		A->TailPitchDeg = -InPitch * 20.f;
		return;
	}
	A->BankDeg = 0.f;
	A->TailPitchDeg = 0.f;
	// the flight style: bursts of beats, then a pause (folded dip or open glide), lengths from the species
	FlightPhase += DeltaSeconds;
	if (FlightPhase >= FlightSegment)
	{
		FlightPhase = 0.f;
		bFlightBurst = !bFlightBurst;
		const float Hz = FMath::Max(Data.WingbeatHz, 1.f);
		auto Beats = [&](int32 Lo, int32 Hi) { return Rng.RandRange(Lo, Hi) / Hz; };
		switch (Data.Flight)
		{
		case EBirdFlightStyle::Bounding: FlightSegment = bFlightBurst ? Beats(3, 7) : Rng.FRandRange(0.2f, 0.4f); break;
		case EBirdFlightStyle::Undulating: FlightSegment = bFlightBurst ? Beats(6, 11) : Rng.FRandRange(0.25f, 0.5f); break;
		case EBirdFlightStyle::Clapping: FlightSegment = bFlightBurst ? Beats(10, 30) : Rng.FRandRange(0.6f, 1.6f); break;
		case EBirdFlightStyle::Soaring: FlightSegment = bFlightBurst ? Beats(3, 7) : Rng.FRandRange(2.f, 5.f); break;
		case EBirdFlightStyle::Flicker: FlightSegment = bFlightBurst ? Beats(4, 9) : Rng.FRandRange(0.6f, 2.f); break;
		default: bFlightBurst = true; FlightSegment = 5.f; break;
		}
		if (Climb > 0.3f)   // climbing: keep beating
		{
			bFlightBurst = true;
		}
	}
	const bool bFold = Data.Flight == EBirdFlightStyle::Bounding;
	const float Beat = Data.WingbeatHz * (1.f + 0.2f * Climb);
	A->Mix(TEXT("Flap"), Flap, bFlightBurst ? 1.f : 0.f, Beat, 0.08f);
	A->Mix(TEXT("Glide"), Glide, !bFlightBurst && !bFold ? 1.f : 0.f, 1.f, 0.12f);
	A->Mix(TEXT("Bound"), Bound, !bFlightBurst && bFold ? 1.f : 0.f, 1.f, 0.06f);
}

void ABirdActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UBirdAnimInstance* A = Anim();
	if (!A)
	{
		return;
	}
	StateTime += DeltaSeconds;
	const float Speed2D = Velocity.Size2D();
	switch (State)
	{
	case EBirdState::Hop:
	case EBirdState::Walk:
		A->SetRate(UBirdSpeciesLibrary::ClipForState(State), Data.StrideCm > 0.f ? Speed2D / Data.StrideCm : 1.f);
		break;
	case EBirdState::Fly:
		TickFlight(DeltaSeconds);
		break;
	default:
		break;
	}
	if (IsOneShot(State) && A->LeadProgress() >= 0.999f && StateTime > 0.05f)
	{
		const EBirdState Done = State;
		SetState(Resume, 0.12f);
		OnActionFinished.Broadcast(this, Done);
	}
	// the head: holds still, then jerks to the target (a bird's head moves in saccades), with a tilt for one eye
	const FTransform CT = Body->GetComponentTransform();
	const FVector WantCS = CT.InverseTransformPosition(LookWorld);
	LookHold -= DeltaSeconds;
	if (bLookActive && LookHold <= 0.f)
	{
		LookHold = Rng.FRandRange(0.25f, 0.9f);
		LookTilt = Rng.FRandRange(-25.f, 25.f);
		LookShown = LookShown.IsNearlyZero() ? WantCS : LookShown;
	}
	if (bLookActive)
	{
		LookShown = FMath::VInterpTo(LookShown, WantCS, DeltaSeconds, LookHold > 0.15f ? 18.f : 4.f);
	}
	LookWeight = FMath::FInterpTo(LookWeight, bLookActive ? 1.f : 0.f, DeltaSeconds, 6.f);
	A->Look.Target = LookShown;
	A->Look.Weight = State == EBirdState::Sleep || State == EBirdState::Preen ? 0.f : LookWeight;
	A->Look.Tilt = LookTilt * LookWeight;
}

namespace
{
	FAutoConsoleCommandWithWorldAndArgs GBirdSpawn(TEXT("Bird.Spawn"),
		TEXT("Bird.Spawn <SpeciesId> [State] [female]: a bird 1.5 m in front of the camera, on the ground (testing)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.Num() < 1)
			{
				return;
			}
			APlayerController* PC = World->GetFirstPlayerController();
			FVector Loc = FVector::ZeroVector;
			FRotator Rot = FRotator::ZeroRotator;
			if (PC)
			{
				PC->GetPlayerViewPoint(Loc, Rot);
			}
			const FVector At = Loc + FRotator(0.f, Rot.Yaw, 0.f).Vector() * 150.f;
			FActorSpawnParameters P;
			P.bDeferConstruction = true;
			ABirdActor* B = World->SpawnActor<ABirdActor>(ABirdActor::StaticClass(), FTransform(FRotator(0.f, Rot.Yaw + 180.f, 0.f), At), P);
			if (!B)
			{
				return;
			}
			B->Species = FName(*Args[0]);
			if (Args.Num() > 1)
			{
				const int64 V = StaticEnum<EBirdState>()->GetValueByNameString(Args[1]);
				if (V != INDEX_NONE)
				{
					B->InitialState = (EBirdState)V;
				}
			}
			if (Args.Num() > 2 && Args[2].StartsWith(TEXT("f"), ESearchCase::IgnoreCase))
			{
				B->Sex = EBirdSex::Female;
			}
			B->FinishSpawning(B->GetActorTransform());
		}));
}
