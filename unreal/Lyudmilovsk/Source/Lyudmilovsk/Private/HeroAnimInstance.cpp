#include "HeroAnimInstance.h"

#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "Animation/AttributesRuntime.h"
#include "BonePose.h"

namespace
{
	UAnimSequence* LoadAnim(const FString& Name)
	{
		return LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Characters/Hero/Anims/A_Hero_%s.A_Hero_%s"), *Name, *Name));
	}

	// clockwise from ahead, 45 degrees apart
	const TCHAR* Directions[] = { TEXT("Fwd"), TEXT("Fwd_Right"), TEXT("Right"), TEXT("Bwd_Right"),
		TEXT("Bwd"), TEXT("Bwd_Left"), TEXT("Left"), TEXT("Fwd_Left") };

	/** Ground one loop of the clip covers (its root motion), cm. */
	float StrideOf(const UAnimSequence* Seq)
	{
		if (!Seq)
		{
			return 100.f;
		}
		const double Len = Seq->GetPlayLength();
		const float Dist = Seq->ExtractRootMotionFromRange(0.0, Len, FAnimExtractContext()).GetTranslation().Size2D();
		return Dist > 10.f ? Dist : 100.f * Len;
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
	Walk.Reset();
	Jog.Reset();
	WalkStride.Reset();
	JogStride.Reset();
	for (const TCHAR* Dir : Directions)
	{
		Walk.Add(LoadAnim(FString(TEXT("Walk_")) + Dir));
		Jog.Add(LoadAnim(FString(TEXT("Jog_")) + Dir));
		WalkStride.Add(StrideOf(Walk.Last()));
		JogStride.Add(StrideOf(Jog.Last()));
	}
	// the speeds the clips were made for: the gaits blend between them
	if (Walk[0] && Jog[0])
	{
		WalkClipSpeed = WalkStride[0] / Walk[0]->GetPlayLength();
		JogClipSpeed = FMath::Max(WalkClipSpeed + 1.f, JogStride[0] / Jog[0]->GetPlayLength());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Hero: locomotion clips missing (unreal/scripts/import_hero.sh)"));
	}
}

void FHeroAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	const UHeroAnimInstance* Hero = CastChecked<UHeroAnimInstance>(InAnimInstance);
	State = Hero->State;
	Idle = Hero->Idle;
	Walk = Hero->Walk;
	Jog = Hero->Jog;
	WalkStride = Hero->WalkStride;
	JogStride = Hero->JogStride;
	WalkClipSpeed = Hero->WalkClipSpeed;
	JogClipSpeed = Hero->JogClipSpeed;
}

bool FHeroAnimProxy::Evaluate(FPoseContext& Output)
{
	if (!Idle)
	{
		Output.ResetToRefPose();
		return true;
	}

	// idle, walk and jog by speed, the two nearest of 8 directions by the direction of travel; every
	// clip is played by distance over its own stride, so the planted foot doesn't slip
	struct FEntry { const UAnimSequence* Seq; double Time; float Weight; };
	TArray<FEntry, TInlineAllocator<5>> Entries;
	const bool bLoco = Walk.Num() == 8 && Jog.Num() == 8;
	const float Moving = bLoco ? FMath::Clamp(State.Speed / WalkClipSpeed, 0.f, 1.f) : 0.f;
	const float Run = FMath::Clamp((State.Speed - WalkClipSpeed) / (JogClipSpeed - WalkClipSpeed), 0.f, 1.f);
	if (Moving < 1.f)
	{
		Entries.Add({ Idle, FMath::Fmod(State.IdleTime, (double)Idle->GetPlayLength()), 1.f - Moving });
	}
	if (Moving > 0.f)
	{
		const float A = FMath::Fmod(State.MoveAngle + 360.f, 360.f) / 45.f;
		const int32 I0 = FMath::FloorToInt(A) % 8;
		const int32 I1 = (I0 + 1) % 8;
		const float F = A - FMath::FloorToFloat(A);
		auto Add = [&](const TArray<TObjectPtr<UAnimSequence>>& Set, const TArray<float>& Stride, int32 I, float W)
		{
			if (W > 1e-3f && Set[I])
			{
				Entries.Add({ Set[I], FMath::Fmod(State.Distance, (double)Stride[I]) / Stride[I] * Set[I]->GetPlayLength(), W });
			}
		};
		Add(Walk, WalkStride, I0, Moving * (1.f - Run) * (1.f - F));
		Add(Walk, WalkStride, I1, Moving * (1.f - Run) * F);
		Add(Jog, JogStride, I0, Moving * Run * (1.f - F));
		Add(Jog, JogStride, I1, Moving * Run * F);
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
		TArray<FCompactPose, TInlineAllocator<5>> Poses;
		TArray<FBlendedCurve, TInlineAllocator<5>> Curves;
		TArray<UE::Anim::FStackAttributeContainer, TInlineAllocator<5>> Attrs;
		TArray<float, TInlineAllocator<5>> Weights;
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
