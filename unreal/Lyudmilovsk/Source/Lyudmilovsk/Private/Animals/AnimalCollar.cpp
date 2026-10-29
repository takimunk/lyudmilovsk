#include "Animals/AnimalCollar.h"

#include "Audio/LyudSoundscape.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
	constexpr float Gravity = 980.f;            // cm/s2
	constexpr float SubStep = 1.f / 240.f;
	constexpr float AirDamp = 1.6f;              // 1/s: the swing dies down in a few seconds
	constexpr float Restitution = 0.1f;          // brass on fur: a soft knock, hardly a bounce
	constexpr float HearRange = 1500.f;          // cm: a tag's clink beyond that is not worth a sound

	UStaticMeshComponent* MakePart(USceneComponent* Owner, const TCHAR* Name, const FString& Path)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner->GetOwner(), Name);
		C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *Path));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->SetGenerateOverlapEvents(false);
		// Scene 01's lights: the club's on channel 0, the outdoor sun and moon on channel 1 (as the body)
		C->LightingChannels.bChannel0 = true;
		C->LightingChannels.bChannel1 = true;
		C->SetCachedMaxDrawDistance(3000.f);
		C->bCastDynamicShadow = true;
		return C;
	}
}

UAnimalCollarComponent::UAnimalCollarComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;     // after the bones moved this frame
}

void UAnimalCollarComponent::Setup(USkeletalMeshComponent* InBody, const FAnimalCollarSpec& InSpec, const FString& CordMesh, const FString& TagMesh)
{
	Body = InBody;
	Spec = InSpec;
	bHaveState = false;
	if (!Body || !Body->GetSkinnedAsset())
	{
		return;
	}
	if (!Cord)
	{
		Cord = MakePart(this, TEXT("CollarCord"), CordMesh);
		Tag = MakePart(this, TEXT("CollarTag"), TagMesh);
		Cord->RegisterComponent();
		Tag->RegisterComponent();
		// the tag is placed in the world every frame by the swing
		Tag->AttachToComponent(this, FAttachmentTransformRules::KeepWorldTransform);
		Tag->SetUsingAbsoluteLocation(true);
		Tag->SetUsingAbsoluteRotation(true);
		Tag->SetUsingAbsoluteScale(true);
	}
	// the cord's mesh is in the bind pose's frame: on the bone, undo the bone's rest transform
	const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
	const int32 Bone = Ref.FindBoneIndex(Spec.Bone);
	if (Bone == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("Animal collar: no bone %s"), *Spec.Bone.ToString());
		Cord->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		return;
	}
	Cord->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetIncludingScale, Spec.Bone);
	FTransform Rest = FTransform::Identity;
	for (int32 B = Bone; B != INDEX_NONE; B = Ref.GetParentIndex(B))
	{
		Rest = Rest * Ref.GetRefBonePose()[B];
	}
	Cord->SetRelativeTransform(Rest.Inverse());
	AttachToComponent(Cord, FAttachmentTransformRules::SnapToTargetIncludingScale);
}

void UAnimalCollarComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	if (Cord)
	{
		Cord->DestroyComponent();
	}
	if (Tag)
	{
		Tag->DestroyComponent();
	}
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UAnimalCollarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const AActor* Owner = GetOwner();
	if (!Cord || !Tag || !Body || !Owner || Owner->IsHidden() || DeltaTime <= 0.f)
	{
		bHaveState = false;
		return;
	}
	JingleCooldown = FMath::Max(0.f, JingleCooldown - DeltaTime);
	const FTransform& X = Cord->GetComponentTransform();
	const float Scale = (float)X.GetScale3D().X;
	const FVector NewPivot = X.TransformPosition(Spec.Hang);
	const FVector Chest = X.TransformPosition(Spec.ChestCenter);
	const FVector Out = X.TransformVectorNoScale(Spec.Forward).GetSafeNormal();
	const float Length = Spec.TagLength * Scale;
	const float ChestR = Spec.ChestRadius * Scale;
	// a fresh body (from the pool, a teleport): the tag hangs at rest
	if (!bHaveState || FVector::DistSquared(NewPivot, Pivot) > FMath::Square(60.f))
	{
		Pivot = NewPivot;
		PivotVel = FVector::ZeroVector;
		Com = Pivot - FVector(0, 0, Length);
		ComVel = FVector::ZeroVector;
		bHaveState = true;
		JingleCooldown = 1.f;           // it settles quietly

	}
	// not on screen, or a hitch (a long frame can't be integrated sensibly): keep it hanging with the neck
	if (!Body->WasRecentlyRendered(0.25f) || DeltaTime > 0.1f)
	{
		Com = NewPivot - FVector(0, 0, Length);
		Pivot = NewPivot;
		ComVel = PivotVel = FVector::ZeroVector;
	}
	else
	{
		const int32 N = FMath::Clamp(FMath::CeilToInt(DeltaTime / SubStep), 1, 12);
		const float Dt = DeltaTime / N;
		const FVector From = Pivot;
		for (int32 I = 1; I <= N; ++I)
		{
			Step(FMath::Lerp(From, NewPivot, float(I) / N), Chest, ChestR, Out, Length, Dt);
		}
	}
	// the tag: hanging from the pivot toward its centre of mass, its face turned out from the chest
	const FVector Up = (Pivot - Com).GetSafeNormal(1e-4f, FVector::UpVector);
	FVector Face = Out - Up * FVector::DotProduct(Out, Up);
	Face = Face.GetSafeNormal(1e-4f, FVector::ForwardVector);
	Tag->SetWorldLocationAndRotation(Pivot, FRotationMatrix::MakeFromZX(Up, Face).Rotator());
	Tag->SetWorldScale3D(FVector(Scale));
	SwingDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Up, FVector::UpVector), -1.f, 1.f)));
}

void UAnimalCollarComponent::Step(const FVector& NewPivot, const FVector& Chest, float ChestR, const FVector& Out, float Length, float Dt)
{
	PivotVel = (NewPivot - Pivot) / Dt;
	Pivot = NewPivot;
	PivotSpeed = FMath::Lerp(PivotSpeed, float(PivotVel.Size()), 1.f - FMath::Exp(-Dt / 0.15f));
	// free flight of the tag for a moment: gravity, the air holding back its swing relative to the neck
	const FVector Prev = Com;
	FVector V = ComVel + FVector(0, 0, -Gravity * Dt);
	V = PivotVel + (V - PivotVel) * FMath::Exp(-AirDamp * Dt);
	const FVector Free = Com + V * Dt;
	// then the ring holds it at its length from the pivot, and the chest stops it
	FVector P = Pivot + (Free - Pivot).GetSafeNormal(1e-4f, -FVector::UpVector) * Length;
	bool bHit = false;
	FVector N = FVector::ZeroVector;
	// the chest never swallows the pivot (a lowered or turned head brings the knot close to it): the tag would be
	// flung round over the top
	ChestR = FMath::Min(ChestR, float(FVector::Dist(Pivot, Chest)) - 0.5f);
	for (int32 K = 0; K < 2; ++K)
	{
		const FVector D = P - Chest;
		if (D.SizeSquared() < ChestR * ChestR)
		{
			N = D.GetSafeNormal(1e-4f, Out);
			P = Chest + N * ChestR;
			P = Pivot + (P - Pivot).GetSafeNormal(1e-4f, -FVector::UpVector) * Length;
			bHit = true;
		}
	}
	// the split ring hangs from a knot in the cord: the tag can swing out, not round over the top
	const FVector Arm = (P - Pivot) / FMath::Max(Length, 1e-3f);
	if (Arm.Z > 0.5f)
	{
		FVector Side = FVector(Arm.X, Arm.Y, 0.f).GetSafeNormal(1e-4f, Out);
		P = Pivot + (Side * 0.866f + FVector(0, 0, 0.5f)) * Length;
	}
	Com = P;
	FVector NewV = (Com - Prev) / Dt;
	if (bHit)
	{
		// a knock: what was coming at the chest bounces off a little; a hard one clinks
		// (the speed it had before this step's pull of gravity: resting on the chest is not a knock)
		const float In = -FVector::DotProduct(ComVel - PivotVel, N);
		if (In > 0.f)
		{
			NewV += N * (In * Restitution);
			// only a tag that was off the chest knocks: lying on it, it just moves with her
			if (In > 22.f && AirTime > 0.15f)
			{
				Jingle(FMath::GetMappedRangeValueClamped(FVector2f(22.f, 90.f), FVector2f(0.2f, 0.8f), In));
			}
		}
	}
	AirTime = bHit ? 0.f : AirTime + Dt;
	// a jerk on the ring (the head tossed, a bounce of the trot): the ring rattles on the knot
	const float Jerk = (NewV - V).Size() / Dt;
	// (only while she moves: the pose of a dog lying or sitting is updated in steps, which would read as jerks)
	if (Jerk > 3.5f * Gravity && !bHit && PivotSpeed > 35.f)
	{
		RestlessTime += Dt;
		if (RestlessTime > 0.02f)
		{
			Jingle(FMath::GetMappedRangeValueClamped(FVector2f(3.5f * Gravity, 9.f * Gravity), FVector2f(0.1f, 0.5f), Jerk));
			RestlessTime = 0.f;
		}
	}
	ComVel = NewV;
}

void UAnimalCollarComponent::Jingle(float Strength)
{
	if (JingleCooldown > 0.f)
	{
		return;
	}
	JingleCooldown = 0.22f + 0.14f * FMath::FRand();      // a tag on a trotting dog: two or three clinks a second at most
	Jingles++;
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (PC && PC->PlayerCameraManager && FVector::DistSquared(PC->PlayerCameraManager->GetCameraLocation(), Com) > HearRange * HearRange)
	{
		return;
	}
	if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
	{
		Sound->PlayEffect(TEXT("collar_jingle"), Com, Strength);
	}
}
