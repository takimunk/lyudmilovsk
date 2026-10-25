#include "Animals/AnimalAnimInstance.h"

#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "Animation/AttributesRuntime.h"
#include "BonePose.h"

void UAnimalAnimInstance::Play(FName Clip, UAnimSequence* Seq, float Rate, bool bLoop, float BlendTime, bool bRestart)
{
	const float Speed = BlendTime > 1e-3f ? 1.f / BlendTime : 1000.f;
	FAnimalClipLayer* Found = nullptr;
	for (FAnimalClipLayer& L : Layers)
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
		FAnimalClipLayer& L = Layers.AddDefaulted_GetRef();
		L.Clip = Clip;
		L.Weight = Layers.Num() == 1 ? 1.f : 0.f;
		L.Target = 1.f;
		L.FadeSpeed = Speed;
		// a locomotion clip coming in picks up the phase of the one going out (no stumble on a gait change)
		if (bLoop && Seq && Layers.Num() > 1)
		{
			const FAnimalClipLayer& Prev = Layers[0];
			if (Prev.Seq && Prev.bLoop)
			{
				const double PL = FMath::Max(1e-3, (double)Prev.Seq->GetPlayLength());
				L.Time = FMath::Fmod(Prev.Time, PL) / PL * Seq->GetPlayLength();
			}
		}
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

void UAnimalAnimInstance::SetRate(FName Clip, float Rate)
{
	for (FAnimalClipLayer& L : Layers)
	{
		if (L.Clip == Clip)
		{
			L.Rate = Rate;
		}
	}
}

FName UAnimalAnimInstance::LeadClip() const
{
	const FAnimalClipLayer* Best = nullptr;
	for (const FAnimalClipLayer& L : Layers)
	{
		if (!Best || L.Target > Best->Target || (L.Target == Best->Target && L.Weight > Best->Weight))
		{
			Best = &L;
		}
	}
	return Best ? Best->Clip : NAME_None;
}

float UAnimalAnimInstance::LeadProgress() const
{
	const FName Lead = LeadClip();
	for (const FAnimalClipLayer& L : Layers)
	{
		if (L.Clip == Lead && L.Seq)
		{
			const double Len = FMath::Max(1e-3, (double)L.Seq->GetPlayLength());
			return L.bLoop ? (float)(FMath::Fmod(L.Time, Len) / Len) : (float)FMath::Min(1.0, L.Time / Len);
		}
	}
	return 0.f;
}

void UAnimalAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	for (int32 i = Layers.Num() - 1; i >= 0; --i)
	{
		FAnimalClipLayer& L = Layers[i];
		L.Weight = FMath::FInterpConstantTo(L.Weight, L.Target, DeltaSeconds, L.FadeSpeed);
		if (L.Seq)
		{
			L.Time += DeltaSeconds * L.Rate;
			if (!L.bLoop)
			{
				L.Time = FMath::Clamp(L.Time, 0.0, (double)L.Seq->GetPlayLength());
			}
		}
		if (L.Weight <= 1e-3f && L.Target <= 0.f)
		{
			Layers.RemoveAt(i);
		}
	}
	Over.Time += DeltaSeconds;
}

void FAnimalAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	const UAnimalAnimInstance* A = CastChecked<UAnimalAnimInstance>(InAnimInstance);
	Layers.Reset();
	for (const FAnimalClipLayer& L : A->Layers)
	{
		if (L.Seq && L.Weight > 1e-3f)
		{
			Layers.Add(L);
		}
	}
	Over = A->Over;
}

namespace
{
	FCompactPoseBoneIndex FindBone(const FBoneContainer& Bones, const TCHAR* Name)
	{
		const int32 M = Bones.GetPoseBoneIndexForBoneName(Name);
		return M == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(M));
	}

	FQuat RefCS(const FBoneContainer& Bones, FCompactPoseBoneIndex B)
	{
		FQuat Q = FQuat::Identity;
		for (FCompactPoseBoneIndex I = B; I.IsValid(); I = Bones.GetParentBoneIndex(I))
		{
			Q = Bones.GetRefPoseTransform(I).GetRotation() * Q;
		}
		return Q;
	}

	void RotateCS(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex B, const FQuat& Delta)
	{
		if (!B.IsValid())
		{
			return;
		}
		FTransform T = CS.GetComponentSpaceTransform(B);
		T.SetRotation((Delta * T.GetRotation()).GetNormalized());
		TArray<FBoneTransform> Out;
		Out.Add(FBoneTransform(B, T));
		CS.SafeSetCSBoneTransforms(Out);
	}
}

bool FAnimalAnimProxy::Evaluate(FPoseContext& Output)
{
	if (Layers.Num() == 0)
	{
		Output.ResetToRefPose();
		return true;
	}
	auto Sample = [](const FAnimalClipLayer& L, FCompactPose& Pose, FBlendedCurve& Curve, UE::Anim::FStackAttributeContainer& Attr)
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
		for (const FAnimalClipLayer& L : Layers)
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

	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const FCompactPoseBoneIndex Head = FindBone(Bones, TEXT("head")), Neck1 = FindBone(Bones, TEXT("neck_01")),
		Neck2 = FindBone(Bones, TEXT("neck_02")), Jaw = FindBone(Bones, TEXT("jaw"));
	if (!Head.IsValid())
	{
		return true;
	}
	FCSPose<FCompactPose> CS;
	CS.InitPose(Output.Pose);
	// the tail wags: each bone turns about the up axis, the wave running out to the tip
	if (FMath::Abs(Over.WagDeg) > 0.05f)
	{
		for (int32 K = 1; K <= 9; K++)
		{
			const FCompactPoseBoneIndex T = FindBone(Bones, *FString::Printf(TEXT("tail_%02d"), K));
			if (!T.IsValid())
			{
				break;
			}
			const float A = Over.WagDeg * FMath::Sin(Over.WagPhase - K * 0.45f) * (0.6f + 0.1f * K);
			RotateCS(CS, T, FQuat(FVector::UpVector, FMath::DegreesToRadians(A)));
		}
	}
	// the head to the look target: the neck takes a third, the head the rest, within the reach of a cat's or dog's neck
	if (Over.LookWeight > 1e-3f)
	{
		const FTransform HeadCS = CS.GetComponentSpaceTransform(Head);
		const FVector Fwd0 = RefCS(Bones, Head).Inverse().RotateVector(FVector::XAxisVector);
		const FVector Cur = HeadCS.GetRotation().RotateVector(Fwd0).GetSafeNormal();
		FVector Want = (Over.Look - HeadCS.GetLocation()).GetSafeNormal();
		if (!Want.IsNearlyZero())
		{
			const FRotator R = Want.Rotation();
			Want = FRotator(FMath::Clamp(R.Pitch, -55.f, 45.f), FMath::Clamp(FRotator::NormalizeAxis(R.Yaw), -110.f, 110.f), 0.f).Vector();
			const FQuat Full = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(Cur, Want), FMath::Clamp(Over.LookWeight, 0.f, 1.f));
			RotateCS(CS, Neck1, FQuat::Slerp(FQuat::Identity, Full, 0.2f));
			RotateCS(CS, Neck2, FQuat::Slerp(FQuat::Identity, Full, 0.2f));
			const FVector Cur2 = CS.GetComponentSpaceTransform(Head).GetRotation().RotateVector(Fwd0).GetSafeNormal();
			RotateCS(CS, Head, FQuat::FindBetweenNormals(Cur2, Full.RotateVector(Cur)));
		}
	}
	// the jaw opens about the head's side axis; panting adds a slow open-close
	const float JawOpen = FMath::Clamp(Over.Jaw + Over.Pant * (0.35f + 0.15f * FMath::Sin(Over.Time * 14.f)), 0.f, 1.f);
	if (Jaw.IsValid() && JawOpen > 0.01f)
	{
		const FQuat HeadQ = CS.GetComponentSpaceTransform(Head).GetRotation();
		const FVector Side = HeadQ.RotateVector(RefCS(Bones, Head).Inverse().RotateVector(FVector::YAxisVector));
		RotateCS(CS, Jaw, FQuat(Side, FMath::DegreesToRadians(JawOpen * 26.f)));
	}
	// ears back (fear, a hiss) or forward (interest), about the head's side axis, a little outward
	if (FMath::Abs(Over.EarsBackDeg) > 0.5f)
	{
		const FQuat HeadQ = CS.GetComponentSpaceTransform(Head).GetRotation();
		const FQuat HeadRef = RefCS(Bones, Head);
		for (const TCHAR* Ear : { TEXT("ear_l"), TEXT("ear_r") })
		{
			const FCompactPoseBoneIndex E = FindBone(Bones, Ear);
			if (!E.IsValid())
			{
				continue;
			}
			const bool bLeft = Ear[4] == 'l';
			const FVector Side = HeadQ.RotateVector(HeadRef.Inverse().RotateVector(FVector::YAxisVector));
			const FVector Up = HeadQ.RotateVector(HeadRef.Inverse().RotateVector(FVector::ZAxisVector));
			RotateCS(CS, E, FQuat(Up, FMath::DegreesToRadians(Over.EarsBackDeg * 0.4f * (bLeft ? -1.f : 1.f))) * FQuat(Side, FMath::DegreesToRadians(-Over.EarsBackDeg)));
		}
	}
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose);
	return true;
}
