#include "HeroAnimInstance.h"

#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "Animation/AttributesRuntime.h"
#include "BonePose.h"

namespace
{
	UAnimSequence* LoadAnim(const TCHAR* Name)
	{
		return LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Characters/Hero/Anims/A_Hero_%s.A_Hero_%s"), Name, Name));
	}

	/** Shares of the look carried by the spine, neck and head (Murmuration's first-person body):
	 *  pitch up, twist (yaw), and the forward flex at full look-down (degrees). */
	struct FLookShare { const TCHAR* Bone; float Up; float Twist; float FlexDeg; };
	const FLookShare LookShares[] = {
		{ TEXT("spine_01"), 0.04f, 0.125f, 1.25f },
		{ TEXT("spine_02"), 0.04f, 0.125f, 1.25f },
		{ TEXT("spine_03"), 0.22f, 0.30f, 8.f },
		{ TEXT("neck_01"), 0.45f, 0.45f, 14.f },
		{ TEXT("head"), 0.f, 0.f, 5.f },
	};
}

void UHeroAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	Idle = LoadAnim(TEXT("Idle"));
	Walk = LoadAnim(TEXT("Walk"));
	if (!Idle || !Walk)
	{
		UE_LOG(LogTemp, Warning, TEXT("Hero: A_Hero_Idle / A_Hero_Walk missing (unreal/scripts/import_hero.sh)"));
	}
}

void FHeroAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	const UHeroAnimInstance* Hero = CastChecked<UHeroAnimInstance>(InAnimInstance);
	State = Hero->State;
	Idle = Hero->Idle;
	Walk = Hero->Walk;
	WalkSpeed = Hero->WalkSpeed;
}

bool FHeroAnimProxy::Evaluate(FPoseContext& Output)
{
	if (!Idle)
	{
		Output.ResetToRefPose();
		return true;
	}

	// idle and walk, by how fast you go
	struct FEntry { const UAnimSequence* Seq; double Time; float Weight; };
	TArray<FEntry, TInlineAllocator<2>> Entries;
	const float Moving = Walk ? FMath::Clamp(State.Speed / WalkSpeed, 0.f, 1.f) : 0.f;
	if (Moving < 1.f)
	{
		Entries.Add({ Idle, FMath::Fmod(State.IdleTime, (double)Idle->GetPlayLength()), 1.f - Moving });
	}
	if (Moving > 0.f)
	{
		double Phase = FMath::Fmod(State.Distance / UHeroAnimInstance::WalkStride, 1.0);
		if (Phase < 0.0)
		{
			Phase += 1.0;
		}
		Entries.Add({ Walk, Phase * Walk->GetPlayLength(), Moving });
	}

	auto Sample = [](const FEntry& E, FCompactPose& Pose, FBlendedCurve& Curve, UE::Anim::FStackAttributeContainer& Attr)
	{
		FAnimationPoseData Data(Pose, Curve, Attr);
		E.Seq->GetAnimationPose(Data, FAnimExtractContext(E.Time, false, FDeltaTimeRecord(), true));
	};
	if (Entries.Num() == 1)
	{
		Sample(Entries[0], Output.Pose, Output.Curve, Output.CustomAttributes);
	}
	else
	{
		TArray<FCompactPose, TInlineAllocator<2>> Poses;
		TArray<FBlendedCurve, TInlineAllocator<2>> Curves;
		TArray<UE::Anim::FStackAttributeContainer, TInlineAllocator<2>> Attrs;
		TArray<float, TInlineAllocator<2>> Weights;
		Poses.SetNum(Entries.Num());
		Curves.SetNum(Entries.Num());
		Attrs.SetNum(Entries.Num());
		float Total = 0.f;
		for (const FEntry& E : Entries)
		{
			Total += E.Weight;
		}
		for (int32 i = 0; i < Entries.Num(); ++i)
		{
			Poses[i].SetBoneContainer(&Output.Pose.GetBoneContainer());
			Curves[i].InitFrom(Output.Curve);
			Sample(Entries[i], Poses[i], Curves[i], Attrs[i]);
			Weights.Add(Entries[i].Weight / Total);
		}
		FAnimationPoseData OutData(Output);
		FAnimationRuntime::BlendPosesTogether(Poses, Curves, Attrs, Weights, OutData);
	}

	// in place: the capsule moves the body
	const FCompactPoseBoneIndex RootIndex(0);
	Output.Pose[RootIndex].SetTranslation(FVector::ZeroVector);

	// the spine, neck and head follow the look (component space: the mesh faces +Y, up is +Z,
	// so look-right is +Z yaw and look-up a rotation about +X)
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const float LookUp = FMath::Max(0.f, State.LookPitch);
	const float LookDown = -FMath::Min(0.f, State.LookPitch);
	const float Flex = FMath::Square(FMath::SmoothStep(25.f, 80.f, LookDown));
	FCSPose<FCompactPose> CS;
	CS.InitPose(Output.Pose);
	FQuat ParentCS = FQuat::Identity;
	bool bFirst = true;
	for (const FLookShare& S : LookShares)
	{
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(S.Bone);
		if (MeshIndex == INDEX_NONE)
		{
			break;
		}
		const FCompactPoseBoneIndex B = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
		if (B == INDEX_NONE)
		{
			break;
		}
		if (bFirst)
		{
			ParentCS = CS.GetComponentSpaceTransform(Bones.GetParentBoneIndex(B)).GetRotation();
			bFirst = false;
		}
		const float PitchDeg = LookUp * S.Up - Flex * S.FlexDeg;
		// twist about up, then pitch about the look's own sideways axis
		const FVector LookSide = FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(State.LookYaw)).RotateVector(FVector::XAxisVector);
		const FQuat Q = FQuat(LookSide, FMath::DegreesToRadians(PitchDeg))
			* FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(State.LookYaw * S.Twist));
		// local = parent^-1 * Q * parent * local
		const FQuat Local = Output.Pose[B].GetRotation();
		const FQuat NewLocal = (ParentCS.Inverse() * Q * ParentCS * Local).GetNormalized();
		Output.Pose[B].SetRotation(NewLocal);
		ParentCS = ParentCS * NewLocal;
	}
	return true;
}
