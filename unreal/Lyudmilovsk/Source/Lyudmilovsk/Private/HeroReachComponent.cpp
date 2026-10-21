#include "HeroReachComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HeroAnimInstance.h"
#include "HeroCharacter.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	/** Where the item rests in the hand in front of the eyes, in the yaw frame (forward, right, up) from the eye, cm. */
	const FVector CarryPos(38.f, 14.f, -17.f);
	// the cargo pocket on the right thigh, outside the oversized overshirt; the hand gets there round the
	// side (PocketWay), not straight down through the clothes
	const FVector PocketPos(-2.f, 33.f, -72.f);
	const FVector PocketWay(14.f, 37.f, -46.f);
	const FVector MouthPos(8.f, 1.5f, -8.5f);
	const FVector NeckUpright = FVector(-0.1f, -0.5f, 0.85f).GetSafeNormal();
	const FVector NeckTipped = FVector(-0.25f, -0.55f, -0.8f).GetSafeNormal();

	float Ease(float T)
	{
		return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(T, 0.f, 1.f));
	}
}

UHeroReachComponent::UHeroReachComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

UHeroReachComponent* UHeroReachComponent::Get(AHeroCharacter* InHero)
{
	if (!InHero)
	{
		return nullptr;
	}
	UHeroReachComponent* C = InHero->FindComponentByClass<UHeroReachComponent>();
	if (!C)
	{
		C = NewObject<UHeroReachComponent>(InHero, TEXT("HeroReach"));
		C->Hero = InHero;
		C->RegisterComponent();
	}
	C->Hero = InHero;
	return C;
}

UHeroReachComponent* UHeroReachComponent::GetForPlayer(const UObject* WorldContext)
{
	return Get(Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(WorldContext, 0)));
}

FTransform UHeroReachComponent::YawFrame() const
{
	const APlayerController* PC = Cast<APlayerController>(Hero->GetController());
	const float Yaw = PC ? PC->GetControlRotation().Yaw : Hero->GetActorRotation().Yaw;
	// the upright eye point: the reach's own crouch moves the real eyes, and keys placed by them would chase it
	return FTransform(FRotator(0.f, Yaw, 0.f), Hero->StandingEyeLocation());
}

FTransform UHeroReachComponent::ItemFrame(const FVector& Pos, const FQuat& Rot) const
{
	return FTransform(Rot, Pos);
}

FQuat UHeroReachComponent::HeldRotation(const FReachItem& It, const FVector& NewUp, const FVector& FaceDir) const
{
	FVector Face = FaceDir - NewUp * FVector::DotProduct(FaceDir, NewUp);
	if (Face.SizeSquared() < 1e-4f)
	{
		Face = FVector::CrossProduct(NewUp, FVector::RightVector);
	}
	// the item's own front is its local X; the item stays turned the way it was gripped, only its top changes
	return FRotationMatrix::MakeFromZX(NewUp, Face.GetSafeNormal()).ToQuat();
}

void UHeroReachComponent::EvalKey(const FKey& K, FVector& OutPos, FQuat& OutRot) const
{
	if (!K.bHead)
	{
		OutPos = K.Pos;
		OutRot = K.Rot;
		return;
	}
	const FTransform Frame = YawFrame();
	OutPos = Frame.TransformPosition(K.Pos);
	OutRot = Frame.GetRotation() * K.Rot;
}

UHeroReachComponent::FKey UHeroReachComponent::CarryKey(const FReachItem& It, float Weight) const
{
	FKey K;
	K.bHead = true;
	K.Pos = CarryPos;
	// neck up, the front turned back at the viewer (the yaw frame's -X)
	K.Rot = HeldRotation(It, FVector::UpVector, -FVector::ForwardVector);
	K.Weight = Weight;
	K.Curl = 1.f;
	return K;
}

UHeroReachComponent::FKey UHeroReachComponent::MouthKey(const FReachItem& It, float Tilt, float Weight) const
{
	FKey K;
	K.bHead = true;
	const FVector Neck = FMath::Lerp(NeckUpright, NeckTipped, Tilt).GetSafeNormal();
	K.Pos = MouthPos - Neck * FMath::Max((It.MouthDistance > 0.f ? It.MouthDistance : It.HalfLength) - 1.f, 0.f);
	K.Rot = HeldRotation(It, Neck, -FVector::ForwardVector);
	K.Weight = Weight;
	K.Curl = 1.f;
	return K;
}

UHeroReachComponent::FKey UHeroReachComponent::PocketKey(const FReachItem& It) const
{
	FKey K = CarryKey(It);
	K.Pos = PocketPos;
	return K;
}

FReachShape UHeroReachComponent::ShapeOf(const FReachItem& It)
{
	if (!It.Shape.IsEmpty())
	{
		return It.Shape;
	}
	FReachShape Out;
	TArray<UPrimitiveComponent*> Comps;
	if (AActor* A = It.Actor.Get())
	{
		A->GetComponents<UPrimitiveComponent>(Comps);
	}
	// 1. the item's own simple collision: spheres, capsules, boxes (a convex hull as its oriented box)
	for (UPrimitiveComponent* C : Comps)
	{
		const UBodySetup* BS = C ? C->GetBodySetup() : nullptr;
		if (!BS || !C->IsVisible())
		{
			continue;
		}
		const FTransform CT = C->GetComponentTransform();
		const FVector S = CT.GetScale3D().GetAbs();
		const FKAggregateGeom& G = BS->AggGeom;
		for (const FKSphereElem& E : G.SphereElems)
		{
			FReachPrim Pr;
			Pr.Kind = FReachPrim::EKind::Sphere;
			Pr.Xf = FTransform(CT.GetRotation(), CT.TransformPosition(E.Center));
			Pr.Radius = E.Radius * S.GetMax();
			Out.Prims.Add(Pr);
		}
		for (const FKSphylElem& E : G.SphylElems)
		{
			FReachPrim Pr;
			Pr.Kind = FReachPrim::EKind::Capsule;
			Pr.Xf = FTransform(CT.GetRotation() * E.Rotation.Quaternion(), CT.TransformPosition(E.Center));
			Pr.Radius = E.Radius * FMath::Max(S.X, S.Y);
			Pr.HalfLength = 0.5f * E.Length * S.Z;
			Out.Prims.Add(Pr);
		}
		for (const FKBoxElem& E : G.BoxElems)
		{
			FReachPrim Pr;
			Pr.Kind = FReachPrim::EKind::Box;
			Pr.Xf = FTransform(CT.GetRotation() * E.Rotation.Quaternion(), CT.TransformPosition(E.Center));
			Pr.Extent = 0.5f * FVector(E.X, E.Y, E.Z) * S;
			Out.Prims.Add(Pr);
		}
		for (const FKConvexElem& E : G.ConvexElems)
		{
			const FTransform ET = E.GetTransform();
			FReachPrim Pr;
			Pr.Kind = FReachPrim::EKind::Box;
			Pr.Xf = FTransform(CT.GetRotation() * ET.GetRotation(), CT.TransformPosition(ET.TransformPosition(E.ElemBox.GetCenter())));
			Pr.Extent = E.ElemBox.GetExtent() * S;
			Out.Prims.Add(Pr);
		}
	}
	if (!Out.IsEmpty())
	{
		return Out;
	}
	// 2. the shape the caller described
	if (It.Radius > 0.f)
	{
		Out.AddCapsule(It.Center, It.Axis, It.HalfLength, It.Radius);
		return Out;
	}
	// 3. the visible parts' own (local, oriented) bounds
	for (UPrimitiveComponent* C : Comps)
	{
		if (!C || !C->IsVisible())
		{
			continue;
		}
		const FTransform CT = C->GetComponentTransform();
		const FBox L = C->CalcBounds(FTransform::Identity).GetBox();
		FReachPrim Pr;
		Pr.Kind = FReachPrim::EKind::Box;
		Pr.Xf = FTransform(CT.GetRotation(), CT.TransformPosition(L.GetCenter()));
		Pr.Extent = L.GetExtent() * CT.GetScale3D().GetAbs();
		Out.Prims.Add(Pr);
	}
	if (Out.IsEmpty())
	{
		Out.AddCapsule(It.Center, It.Axis, 0.f, 4.f);
	}
	return Out;
}

void UHeroReachComponent::PlanGrip(const FReachItem& It)
{
	const FVector Z = FVector::UpVector;
	FVector H = (It.Center - Hero->GetActorLocation()).GetSafeNormal2D();
	if (H.IsNearlyZero())
	{
		H = Hero->GetActorForwardVector();
	}
	const FVector R = FVector::CrossProduct(Z, H);   // the hero's right when facing the item
	const FTransform Frame(Item0Rot, It.Center);
	const FReachShape World = LocalShape.Transformed(Frame);
	const FBox B = World.Bounds();
	const FVector Ext = B.GetExtent();
	// tall things (a bottle, a can) are taken from the right side, palm toward them; low or flat ones
	// (an apple, a book, a box) from above, palm down. Either way the palm goes onto the real surface.
	const bool bSide = Ext.Z > 1.25f * FMath::Max(Ext.X, Ext.Y);
	const FVector Approach = bSide ? R : Z;
	FVector Hit;
	if (!World.Trace(It.Center + Approach * 60.f, It.Center, Hit))
	{
		Hit = It.Center + Approach * FMath::Max(It.Radius, 3.f);
	}
	const FVector Out = World.Normal(Hit);
	const FVector Pn = -Out;
	FVector Fd = H;
	if (!bSide)
	{
		// fingers across a long item (a book, a stick), along the approach for a round one
		const FVector Long = Ext.X > 1.3f * Ext.Y ? FVector::XAxisVector : Ext.Y > 1.3f * Ext.X ? FVector::YAxisVector : FVector::ZeroVector;
		if (!Long.IsZero())
		{
			Fd = FVector::CrossProduct(Long, Z).GetSafeNormal();
			if (FVector::DotProduct(Fd, H) < 0.f)
			{
				Fd = -Fd;
			}
		}
	}
	Fd = (Fd - Pn * FVector::DotProduct(Fd, Pn)).GetSafeNormal(UE_SMALL_NUMBER, H);
	// the palm frame's point sits on the item's surface; the hand's own skin is kept outside by HandReach.
	// From above, the palm lies a little behind the centre so the fingers wrap over it.
	FVector Palm = Hit + Out * 0.8f;
	if (!bSide)
	{
		Palm -= Fd * (0.3f * FMath::Min(Ext.X, Ext.Y));
	}
	Grip.PalmLocal = Frame.InverseTransformPosition(Palm);
	Grip.FingerLocal = Frame.InverseTransformVectorNoScale(Fd);
	Grip.NormalLocal = Frame.InverseTransformVectorNoScale(Pn);
}

void UHeroReachComponent::Begin(const FReachItem& It, bool bFromPocket)
{
	Item = It;
	Item0Rot = FRotationMatrix::MakeFromZX(It.Axis.GetSafeNormal(), It.Front).ToQuat();
	LocalShape = ShapeOf(It).Transformed(FTransform(Item0Rot, It.Center).Inverse());
	PlanGrip(It);
	Steps.Reset();
	Current = 0;
	StepTime = 0.f;
	From = FKey();
	From.Weight = 0.f;
	if (!bLocked)
	{
		if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
		{
			PC->SetIgnoreLookInput(true);
			PC->SetIgnoreMoveInput(true);
		}
		bLocked = true;
	}
}

bool UHeroReachComponent::Pick(const FReachItem& It, TFunction<void()> OnHeld)
{
	if (!Hero || IsBusy() || !It.Actor.IsValid())
	{
		return false;
	}
	Begin(It, false);
	FKey Reach;
	Reach.Pos = It.Center;
	Reach.Rot = Item0Rot;
	Reach.Curl = 0.05f;
	{
		FStep S; S.Duration = 0.8f; S.Target = Reach; S.Target.HandBack = 11.f; S.bAimCamera = true;
		Steps.Add(S);
	}
	{
		FStep S; S.Duration = 0.4f; S.Target = Reach; S.bAimCamera = true;
		Steps.Add(S);
	}
	{
		FStep S; S.Duration = 0.35f; S.Target = Reach; S.Target.Curl = 1.f; S.bAimCamera = true;
		S.OnEnd = [this] { AttachHeld(); };
		Steps.Add(S);
	}
	{
		FStep S; S.Duration = 0.8f; S.Target = CarryKey(It);
		Steps.Add(S);
	}
	{
		FStep S; S.Duration = 0.3f; S.Target = CarryKey(It);
		S.OnEnd = OnHeld;
		Steps.Add(S);
	}
	return true;
}

bool UHeroReachComponent::Take(const FReachItem& It, TFunction<void()> OnStowed)
{
	return Pick(It, [this, OnStowed] { Stow(OnStowed); });
}

void UHeroReachComponent::Stow(TFunction<void()> OnStowed)
{
	const FReachItem It = Item;
	{
		FStep S; S.Duration = 0.3f; S.Target = PocketKey(It); S.Target.Pos = PocketWay;
		Steps.Add(S);
	}
	{
		FStep S; S.Duration = 0.35f; S.Target = PocketKey(It);
		S.OnEnd = [this]
		{
			if (AActor* A = Held.Get())
			{
				A->Destroy();
			}
			Held.Reset();
		};
		Steps.Add(S);
	}
	{
		FStep S; S.Duration = 0.5f; S.Target = PocketKey(It); S.Target.Weight = 0.f; S.Target.Curl = 0.f;
		S.OnEnd = OnStowed;
		Steps.Add(S);
	}
}

bool UHeroReachComponent::Drink(const FReachItem& It, float Seconds, TFunction<void(float)> OnLevel, TFunction<void()> OnDone)
{
	if (!Hero || IsBusy() || !It.Actor.IsValid())
	{
		return false;
	}
	Begin(It, true);
	FKey Reach;
	Reach.Pos = It.Center;
	Reach.Rot = Item0Rot;
	Reach.Curl = 0.05f;
	{
		FStep S; S.Duration = 0.55f; S.Target = Reach; S.Target.HandBack = 8.f;
		Steps.Add(S);
	}
	{
		FStep S; S.Duration = 0.25f; S.Target = Reach;
		Steps.Add(S);
	}
	{
		FStep S; S.Duration = 0.25f; S.Target = Reach; S.Target.Curl = 1.f;
		S.OnEnd = [this] { AttachHeld(); };
		Steps.Add(S);
	}
	{
		FStep S; S.Duration = 0.75f; S.Target = MouthKey(It, 0.f);
		Steps.Add(S);
	}
	{
		FStep S; S.Duration = 0.9f; S.Target = MouthKey(It, 1.f);
		Steps.Add(S);
	}
	{
		FStep S; S.Duration = FMath::Max(Seconds, 0.5f); S.Target = MouthKey(It, 1.f);
		S.OnProgress = [OnLevel](float T) { if (OnLevel) { OnLevel(1.f - T); } };
		Steps.Add(S);
	}
	{
		FStep S; S.Duration = 0.7f; S.Target = MouthKey(It, 0.f);
		Steps.Add(S);
	}
	{
		FStep S; S.Duration = 0.6f; S.Target = CarryKey(It);
		S.OnEnd = OnDone;
		Steps.Add(S);
	}
	return true;
}

bool UHeroReachComponent::Eat(const FReachItem& It, int32 Bites, TFunction<void(float)> OnBite, TFunction<void()> OnDone)
{
	if (!Hero || IsBusy() || !It.Actor.IsValid())
	{
		return false;
	}
	Begin(It, true);
	FKey Reach;
	Reach.Pos = It.Center;
	Reach.Rot = Item0Rot;
	Reach.Curl = 0.05f;
	{
		FStep S; S.Duration = 0.55f; S.Target = Reach; S.Target.HandBack = 8.f;
		Steps.Add(S);
	}
	{
		FStep S; S.Duration = 0.25f; S.Target = Reach;
		Steps.Add(S);
	}
	{
		FStep S; S.Duration = 0.25f; S.Target = Reach; S.Target.Curl = 1.f;
		S.OnEnd = [this] { AttachHeld(); };
		Steps.Add(S);
	}
	FKey Near = MouthKey(It, 0.f);
	Near.Pos = MouthPos + FVector(5.f, 0.f, -3.f);
	Near.Rot = HeldRotation(It, FVector(0.f, -0.3f, 0.95f).GetSafeNormal(), -FVector::ForwardVector);
	FKey At = Near;
	At.Pos = MouthPos + FVector(0.f, 0.f, -1.f);
	{
		FStep S; S.Duration = 0.7f; S.Target = Near;
		Steps.Add(S);
	}
	for (int32 B = 0; B < FMath::Max(Bites, 1); ++B)
	{
		{ FStep S; S.Duration = 0.3f; S.Target = At; Steps.Add(S); }
		{
			FStep S; S.Duration = 0.4f; S.Target = Near;
			const float Left = 1.f - float(B + 1) / FMath::Max(Bites, 1);
			S.OnStart = [OnBite, Left] { if (OnBite) { OnBite(Left); } };
			Steps.Add(S);
		}
	}
	{
		FStep S; S.Duration = 0.6f; S.Target = CarryKey(It);
		S.OnEnd = OnDone;
		Steps.Add(S);
	}
	return true;
}

void UHeroReachComponent::AttachHeld()
{
	AActor* A = Item.Actor.Get();
	USkeletalMeshComponent* Body = Hero ? Hero->GetMesh() : nullptr;
	if (A && Body)
	{
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(A->GetRootComponent()))
		{
			Prim->SetSimulatePhysics(false);
			Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		A->AttachToComponent(Body, FAttachmentTransformRules::KeepWorldTransform, TEXT("hand_r"));
		Held = A;
	}
}

void UHeroReachComponent::Drop()
{
	if (AActor* A = Held.Get())
	{
		A->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(A->GetRootComponent()))
		{
			Prim->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Prim->SetSimulatePhysics(true);
		}
	}
	Held.Reset();
}

void UHeroReachComponent::Finish()
{
	Steps.Reset();
	Current = 0;
	Pose = FHeroReachPose();
	PushPose();
	if (bLocked)
	{
		if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
		{
			PC->SetIgnoreLookInput(false);
			PC->SetIgnoreMoveInput(false);
		}
		bLocked = false;
	}
}

void UHeroReachComponent::PushPose()
{
	if (!Hero)
	{
		return;
	}
	USkeletalMeshComponent* Body = Hero->GetMesh();
	TArray<USkeletalMeshComponent*, TInlineAllocator<2>> Meshes;
	Meshes.Add(Body);
	for (USceneComponent* Child : Body->GetAttachChildren())
	{
		if (USkeletalMeshComponent* M = Cast<USkeletalMeshComponent>(Child))
		{
			Meshes.Add(M);
		}
	}
	for (USkeletalMeshComponent* M : Meshes)
	{
		if (UHeroAnimInstance* Anim = Cast<UHeroAnimInstance>(M->GetAnimInstance()))
		{
			Anim->Reach = Pose;
		}
	}
}

void UHeroReachComponent::DebugForcePose(const FHeroReachPose& InPose)
{
	bPoseForced = true;
	Pose = InPose;
	PushPose();
}

void UHeroReachComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Hero || bPoseForced || Steps.Num() == 0)
	{
		return;
	}
	if (!Steps.IsValidIndex(Current))
	{
		Finish();
		return;
	}
	FStep& Step = Steps[Current];
	if (StepTime == 0.f && Step.OnStart)
	{
		TFunction<void()> Start = Step.OnStart;
		Start();
	}
	StepTime += DeltaTime;
	const float T = Step.Duration > 0.f ? FMath::Clamp(StepTime / Step.Duration, 0.f, 1.f) : 1.f;
	const float S = Ease(T);

	FVector P0, P1;
	FQuat R0, R1;
	EvalKey(From, P0, R0);
	EvalKey(Step.Target, P1, R1);
	// the first move has no earlier key: it starts where it ends, only the weight (and so the blend with the animation) grows
	if (Current == 0 && From.Weight == 0.f && From.Pos.IsZero())
	{
		P0 = P1;
		R0 = R1;
	}
	const FVector ItemPos = FMath::Lerp(P0, P1, S);
	const FQuat ItemRot = FQuat::Slerp(R0, R1, S).GetNormalized();
	const float HandBack = FMath::Lerp(From.HandBack, Step.Target.HandBack, S);

	const FTransform Frame(ItemRot, ItemPos);
	const FVector PalmW = Frame.TransformPosition(Grip.PalmLocal);
	const FVector FingerW = Frame.TransformVectorNoScale(Grip.FingerLocal);
	const FVector NormalW = Frame.TransformVectorNoScale(Grip.NormalLocal);

	USkeletalMeshComponent* Body = Hero->GetMesh();
	const FTransform ToMesh = Body->GetComponentTransform().Inverse();
	Pose.Weight = FMath::Lerp(From.Weight, Step.Target.Weight, S);
	Pose.Palm = ToMesh.TransformPosition(PalmW - NormalW * HandBack);
	Pose.FingerDir = ToMesh.TransformVectorNoScale(FingerW).GetSafeNormal();
	Pose.PalmNormal = ToMesh.TransformVectorNoScale(NormalW).GetSafeNormal();
	Pose.Curl = FMath::Lerp(From.Curl, Step.Target.Curl, S);
	Pose.bHasShape = true;
	Pose.Shape = LocalShape.Transformed(Frame).Transformed(ToMesh);
	Pose.PostureWeight = 1.f;
	PushPose();

	if (Step.bAimCamera)
	{
		if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
		{
			const FRotator Want = (Item.Center - Hero->EyeLocation()).Rotation();
			FRotator Now = PC->GetControlRotation();
			Now = FMath::RInterpTo(Now, FRotator(FMath::Clamp(FRotator::NormalizeAxis(Want.Pitch), -75.f, 75.f), Want.Yaw, 0.f), DeltaTime, 2.2f);
			PC->SetControlRotation(Now);
		}
	}
	if (Step.OnProgress)
	{
		Step.OnProgress(T);
	}
	if (T >= 1.f)
	{
		TFunction<void()> End = Step.OnEnd;
		From = Step.Target;
		StepTime = 0.f;
		++Current;
		if (End)
		{
			End();
		}
		if (Current >= Steps.Num())
		{
			Finish();
		}
	}
}

FString UHeroReachComponent::DebugReachTest(AHeroCharacter* InHero, FVector Palm, FVector FingerDir, FVector PalmNormal, float Curl,
	FVector ShapeCenter, FVector ShapeAxis, float ShapeHalfLength, float ShapeRadius, float LookPitch)
{
	if (!InHero)
	{
		return TEXT("no hero");
	}
	USkeletalMeshComponent* Body = InHero->GetMesh();
	if (!Body->GetSkeletalMeshAsset())
	{
		Body->SetSkeletalMesh(InHero->BodyMesh.LoadSynchronous());
	}
	Body->bEnableUpdateRateOptimizations = false;
	Body->InitAnim(true);
	UHeroReachComponent* C = Get(InHero);
	FString Out;
	auto Dump = [&](const TCHAR* Label)
	{
		Out += Label;
		Out += TEXT("\n");
		for (const TCHAR* Name : { TEXT("pelvis"), TEXT("spine_03"), TEXT("neck_01"), TEXT("head"), TEXT("eye_l"), TEXT("eye_r"), TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
			TEXT("thumb_01_r"), TEXT("thumb_02_r"), TEXT("thumb_03_r"), TEXT("index_01_r"), TEXT("index_02_r"), TEXT("index_03_r"),
			TEXT("middle_01_r"), TEXT("middle_02_r"), TEXT("middle_03_r"), TEXT("ring_01_r"), TEXT("pinky_01_r"),
			TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("ball_l"), TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"), TEXT("ball_r") })
		{
			const FVector P = Body->GetBoneLocation(Name, EBoneSpaces::WorldSpace);
			Out += FString::Printf(TEXT("%s %.2f %.2f %.2f\n"), Name, P.X, P.Y, P.Z);
		}
	};
	const FTransform ToMesh = Body->GetComponentTransform().Inverse();
	FHeroReachPose P;
	P.Weight = 1.f;
	P.Palm = ToMesh.TransformPosition(Palm);
	P.FingerDir = ToMesh.TransformVectorNoScale(FingerDir).GetSafeNormal();
	P.PalmNormal = ToMesh.TransformVectorNoScale(PalmNormal).GetSafeNormal();
	P.Curl = Curl;
	P.bHasShape = ShapeRadius > 0.f;
	P.Shape.Prims.Reset();
	if (P.bHasShape)
	{
		P.Shape.AddCapsule(ToMesh.TransformPosition(ShapeCenter), ToMesh.TransformVectorNoScale(ShapeAxis), ShapeHalfLength, ShapeRadius);
	}
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		if (Pass == 1)
		{
			P = FHeroReachPose();
		}
		C->DebugForcePose(P);
		if (UHeroAnimInstance* Anim = Cast<UHeroAnimInstance>(Body->GetAnimInstance()))
		{
			Anim->State.LookPitch = LookPitch;
		}
		Body->TickAnimation(0.f, false);
		Body->RefreshBoneTransforms();
		Dump(Pass == 0 ? TEXT("reach") : TEXT("---\nplain"));
	}
	return Out;
}
