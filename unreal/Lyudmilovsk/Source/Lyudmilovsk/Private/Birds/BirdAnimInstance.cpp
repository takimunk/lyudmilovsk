#include "Birds/BirdAnimInstance.h"

#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "Animation/AttributesRuntime.h"
#include "BonePose.h"

void UBirdAnimInstance::Play(FName Clip, UAnimSequence* Seq, float Rate, bool bLoop, float BlendTime, bool bRestart)
{
	const float Speed = BlendTime > 1e-3f ? 1.f / BlendTime : 1000.f;
	FBirdClipLayer* Found = nullptr;
	for (FBirdClipLayer& L : Layers)
	{
		L.FadeSpeed = Speed;
		if (L.Clip == Clip)
		{
			Found = &L;
			L.Target = 1.f;
		}
		else
		{
			L.Target = 0.f;
		}
	}
	if (!Found)
	{
		FBirdClipLayer& L = Layers.AddDefaulted_GetRef();
		L.Clip = Clip;
		L.Seq = Seq;
		L.Weight = Layers.Num() == 1 ? 1.f : 0.f;
		L.Target = 1.f;
		L.FadeSpeed = Speed;
		Found = &L;
	}
	else if (bRestart)
	{
		Found->Time = 0.0;
	}
	Found->Seq = Seq;
	Found->Rate = Rate;
	Found->bLoop = bLoop;
}

void UBirdAnimInstance::Mix(FName Clip, UAnimSequence* Seq, float Weight, float Rate, float BlendTime)
{
	for (FBirdClipLayer& L : Layers)
	{
		if (L.Clip == Clip)
		{
			L.Target = Weight;
			L.Rate = Rate;
			L.Seq = Seq;
			L.FadeSpeed = BlendTime > 1e-3f ? 1.f / BlendTime : 1000.f;
			return;
		}
	}
	if (Weight <= 0.f)
	{
		return;
	}
	FBirdClipLayer& L = Layers.AddDefaulted_GetRef();
	L.Clip = Clip;
	L.Seq = Seq;
	L.Rate = Rate;
	L.Target = Weight;
	L.FadeSpeed = BlendTime > 1e-3f ? 1.f / BlendTime : 1000.f;
	L.bLoop = true;
}

void UBirdAnimInstance::SetRate(FName Clip, float Rate)
{
	for (FBirdClipLayer& L : Layers)
	{
		if (L.Clip == Clip)
		{
			L.Rate = Rate;
		}
	}
}

FName UBirdAnimInstance::LeadClip() const
{
	const FBirdClipLayer* Best = nullptr;
	for (const FBirdClipLayer& L : Layers)
	{
		if (!Best || L.Target > Best->Target || (L.Target == Best->Target && L.Weight > Best->Weight))
		{
			Best = &L;
		}
	}
	return Best ? Best->Clip : NAME_None;
}

float UBirdAnimInstance::LeadProgress() const
{
	const FName Lead = LeadClip();
	for (const FBirdClipLayer& L : Layers)
	{
		if (L.Clip == Lead && L.Seq)
		{
			const double Len = FMath::Max(1e-3, (double)L.Seq->GetPlayLength());
			return L.bLoop ? (float)FMath::Fmod(L.Time, Len) / Len : (float)FMath::Min(1.0, L.Time / Len);
		}
	}
	return 0.f;
}

void UBirdAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	for (int32 i = Layers.Num() - 1; i >= 0; --i)
	{
		FBirdClipLayer& L = Layers[i];
		L.Weight = FMath::FInterpConstantTo(L.Weight, L.Target, DeltaSeconds, L.FadeSpeed);
		if (L.Seq)
		{
			L.Time += DeltaSeconds * L.Rate;
			const double Len = L.Seq->GetPlayLength();
			if (!L.bLoop)
			{
				L.Time = FMath::Clamp(L.Time, 0.0, Len);
			}
		}
		if (L.Weight <= 1e-3f && L.Target <= 0.f)
		{
			Layers.RemoveAt(i);
		}
	}
}

void FBirdAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	const UBirdAnimInstance* Bird = CastChecked<UBirdAnimInstance>(InAnimInstance);
	Layers.Reset();
	for (const FBirdClipLayer& L : Bird->Layers)
	{
		if (L.Seq && L.Weight > 1e-3f)
		{
			Layers.Add(L);
		}
	}
	Look = Bird->Look;
	BankDeg = Bird->BankDeg;
	TailPitchDeg = Bird->TailPitchDeg;
	StreamerScale = Bird->StreamerScale;
}

namespace
{
	FCompactPoseBoneIndex FindBone(const FBoneContainer& Bones, const TCHAR* Name)
	{
		const int32 M = Bones.GetPoseBoneIndexForBoneName(Name);
		return M == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(M));
	}

	/** Bone's reference-pose rotation in component space. */
	FQuat RefCS(const FBoneContainer& Bones, FCompactPoseBoneIndex B)
	{
		FQuat Q = FQuat::Identity;
		for (FCompactPoseBoneIndex I = B; I.IsValid(); I = Bones.GetParentBoneIndex(I))
		{
			Q = Bones.GetRefPoseTransform(I).GetRotation() * Q;
		}
		return Q;
	}

	/** Adds a component-space rotation to bone B (keeps its children following). */
	void RotateCS(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex B, const FQuat& Delta)
	{
		FTransform T = CS.GetComponentSpaceTransform(B);
		T.SetRotation((Delta * T.GetRotation()).GetNormalized());
		TArray<FBoneTransform> Out;
		Out.Add(FBoneTransform(B, T));
		CS.SafeSetCSBoneTransforms(Out);
	}
}

bool FBirdAnimProxy::Evaluate(FPoseContext& Output)
{
	if (Layers.Num() == 0)
	{
		Output.ResetToRefPose();
		return true;
	}
	auto Sample = [](const FBirdClipLayer& L, FCompactPose& Pose, FBlendedCurve& Curve, UE::Anim::FStackAttributeContainer& Attr)
	{
		const double Len = FMath::Max(1e-3, (double)L.Seq->GetPlayLength());
		const double T = L.bLoop ? FMath::Fmod(FMath::Fmod(L.Time, Len) + Len, Len) : FMath::Clamp(L.Time, 0.0, Len);
		FAnimationPoseData Data(Pose, Curve, Attr);
		L.Seq->GetAnimationPose(Data, FAnimExtractContext(T, false, FDeltaTimeRecord(), L.bLoop));
	};
	if (Layers.Num() == 1)
	{
		Sample(Layers[0], Output.Pose, Output.Curve, Output.CustomAttributes);
	}
	else
	{
		TArray<FCompactPose, TInlineAllocator<4>> Poses;
		TArray<FBlendedCurve, TInlineAllocator<4>> Curves;
		TArray<UE::Anim::FStackAttributeContainer, TInlineAllocator<4>> Attrs;
		TArray<float, TInlineAllocator<4>> Weights;
		Poses.SetNum(Layers.Num());
		Curves.SetNum(Layers.Num());
		Attrs.SetNum(Layers.Num());
		float Total = 0.f;
		for (const FBirdClipLayer& L : Layers)
		{
			Total += L.Weight;
		}
		for (int32 i = 0; i < Layers.Num(); ++i)
		{
			Poses[i].SetBoneContainer(&Output.Pose.GetBoneContainer());
			Curves[i].InitFrom(Output.Curve);
			Sample(Layers[i], Poses[i], Curves[i], Attrs[i]);
			Weights.Add(Layers[i].Weight / FMath::Max(Total, 1e-4f));
		}
		FAnimationPoseData OutData(Output);
		FAnimationRuntime::BlendPosesTogether(Poses, Curves, Attrs, Weights, OutData);
	}
	Output.Pose[FCompactPoseBoneIndex(0)].SetTranslation(FVector::ZeroVector);

	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const FCompactPoseBoneIndex Body = FindBone(Bones, TEXT("body")), Neck = FindBone(Bones, TEXT("neck")),
		Head = FindBone(Bones, TEXT("head")), Tail = FindBone(Bones, TEXT("tail"));
	if (!Head.IsValid() || !Neck.IsValid())
	{
		return true;
	}
	// the outermost tail feather pair, shorter or longer (its bone scales along with the card)
	if (FMath::Abs(StreamerScale - 1.f) > 1e-3)
	{
		for (int32 K = 7; K >= 1; --K)
		{
			const FCompactPoseBoneIndex L = FindBone(Bones, *FString::Printf(TEXT("tf.L.%d"), K));
			if (L.IsValid())
			{
				for (const TCHAR* Side : { TEXT("L"), TEXT("R") })
				{
					const FCompactPoseBoneIndex B = FindBone(Bones, *FString::Printf(TEXT("tf.%s.%d"), Side, K));
					if (B.IsValid())
					{
						Output.Pose[B].SetScale3D(FVector(StreamerScale));
					}
				}
				break;
			}
		}
	}
	FCSPose<FCompactPose> CS;
	CS.InitPose(Output.Pose);
	// banking (player flight): the body rolls about its length, the tail twists and pitches against it
	if (Body.IsValid() && (FMath::Abs(BankDeg) > 0.1f || FMath::Abs(TailPitchDeg) > 0.1f))
	{
		RotateCS(CS, Body, FQuat(FVector::XAxisVector, FMath::DegreesToRadians(BankDeg * 0.35f)));
		if (Tail.IsValid())
		{
			RotateCS(CS, Tail, FQuat(FVector::XAxisVector, FMath::DegreesToRadians(-BankDeg * 0.5f)) *
				FQuat(FVector::YAxisVector, FMath::DegreesToRadians(TailPitchDeg)));
		}
	}
	// the head turns to the target: the neck takes a third, the head the rest; a bird can look ~150 degrees round
	if (Look.Weight > 1e-3f)
	{
		const FTransform HeadCS = CS.GetComponentSpaceTransform(Head);
		// the head's forward (its forward in the bind pose is the mesh's +X)
		const FVector Fwd0 = RefCS(Bones, Head).Inverse().RotateVector(FVector::XAxisVector);
		const FVector Cur = HeadCS.GetRotation().RotateVector(Fwd0).GetSafeNormal();
		FVector Want = (Look.Target - HeadCS.GetLocation()).GetSafeNormal();
		if (!Want.IsNearlyZero())
		{
			// keep within reach of the body's forward (+X): yaw up to 150, pitch -70..60 degrees
			const FRotator R = Want.Rotation();
			const FRotator Clamped(FMath::Clamp(R.Pitch, -70.f, 60.f), FMath::Clamp(FRotator::NormalizeAxis(R.Yaw), -150.f, 150.f), 0.f);
			Want = Clamped.Vector();
			const FQuat Full = FQuat::FindBetweenNormals(Cur, Want);
			const FQuat Turn = FQuat::Slerp(FQuat::Identity, Full, FMath::Clamp(Look.Weight, 0.f, 1.f));
			RotateCS(CS, Neck, FQuat::Slerp(FQuat::Identity, Turn, 0.35f));
			const FTransform HeadAfterNeck = CS.GetComponentSpaceTransform(Head);
			const FVector Cur2 = HeadAfterNeck.GetRotation().RotateVector(Fwd0).GetSafeNormal();
			FQuat Rest = FQuat::FindBetweenNormals(Cur2, FQuat::Slerp(FQuat::Identity, Full, Look.Weight).RotateVector(Cur));
			if (FMath::Abs(Look.Tilt) > 0.1f)
			{
				Rest = FQuat(Want, FMath::DegreesToRadians(Look.Tilt)) * Rest;
			}
			RotateCS(CS, Head, Rest);
		}
	}
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose);
	return true;
}
