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
	Wings = Bird->Wings;
	Grip = Bird->Grip;
	FoldSeq = Bird->FoldSeq;
	TuckSeq = Bird->TuckSeq;
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

namespace
{
	const TCHAR* LegBones[] = { TEXT("thigh"), TEXT("shin"), TEXT("tarsus"), TEXT("toes"), TEXT("hallux") };

	void SampleAt0(UAnimSequence* Seq, FCompactPose& Pose)
	{
		FBlendedCurve Curve;
		Curve.InitFrom(Pose.GetBoneContainer());
		UE::Anim::FStackAttributeContainer Attr;
		FAnimationPoseData Data(Pose, Curve, Attr);
		Seq->GetAnimationPose(Data, FAnimExtractContext(0.0, false));
	}

	/** +1 for a bone on the bird's left (UE -Y), -1 on its right. */
	float SideSign(const TCHAR* Side) { return Side[0] == TEXT('L') ? 1.f : -1.f; }
}

void FBirdAnimProxy::ApplyWingsLocal(FCompactPose& Pose) const
{
	// spread = the bind pose, folded = Idle's first frame; the upstroke flexes the wrist a little (the hand comes in)
	const FBoneContainer& Bones = Pose.GetBoneContainer();
	const float W = FMath::Clamp(Wings.Weight, 0.f, 1.f);
	FCompactPose Fold, Tuck;
	if (FoldSeq)
	{
		Fold.SetBoneContainer(&Bones);
		SampleAt0(FoldSeq, Fold);
	}
	if (TuckSeq)
	{
		Tuck.SetBoneContainer(&Bones);
		SampleAt0(TuckSeq, Tuck);
	}
	for (const TCHAR* Side : { TEXT("L"), TEXT("R") })
	{
		const bool bL = Side[0] == TEXT('L');
		const float Phase = bL ? Wings.StrokeL : Wings.StrokeR;
		const float Up = FMath::Max(0.f, FMath::Sin(2.f * PI * (Phase - 0.5f))) * Wings.StrokeAmp;   // > 0 on the upstroke
		const float Spread = FMath::Clamp((bL ? Wings.SpreadL : Wings.SpreadR) * (1.f - 0.35f * Up), 0.f, 1.f);
		auto Set = [&](const FString& Name, float SpreadOfBone)
		{
			const FCompactPoseBoneIndex B = FindBone(Bones, *Name);
			if (!B.IsValid())
			{
				return;
			}
			const FQuat Ref = Bones.GetRefPoseTransform(B).GetRotation();
			const FQuat Folded = FoldSeq ? Fold[B].GetRotation() : Ref;
			const FQuat Want = FQuat::Slerp(Folded, Ref, SpreadOfBone).GetNormalized();
			Pose[B].SetRotation(FQuat::Slerp(Pose[B].GetRotation(), Want, W).GetNormalized());
			if (FoldSeq)
			{
				Pose[B].SetTranslation(FMath::Lerp(Pose[B].GetTranslation(), FMath::Lerp(Fold[B].GetTranslation(), Bones.GetRefPoseTransform(B).GetTranslation(), SpreadOfBone), W));
			}
		};
		// the hand folds first and opens last
		Set(FString::Printf(TEXT("humerus_%s"), Side), FMath::Clamp(Spread * 1.4f, 0.f, 1.f));
		Set(FString::Printf(TEXT("forearm_%s"), Side), FMath::Clamp(Spread * 1.2f - 0.1f, 0.f, 1.f));
		Set(FString::Printf(TEXT("hand_%s"), Side), Spread);
		Set(FString::Printf(TEXT("primaries_%s"), Side), Spread);
		for (int32 i = 1; i <= 12; ++i)
		{
			Set(FString::Printf(TEXT("p%d_%s"), i, Side), Spread);
		}
		for (int32 i = 1; i <= 24; ++i)
		{
			Set(FString::Printf(TEXT("s%d_%s"), i, Side), FMath::Clamp(Spread * 1.2f - 0.1f, 0.f, 1.f));
		}
		// legs: tucked under the belly (Glide) .. hanging down (Idle)
		for (const TCHAR* Leg : LegBones)
		{
			const FCompactPoseBoneIndex B = FindBone(Bones, *FString::Printf(TEXT("%s_%s"), Leg, Side));
			if (B.IsValid() && FoldSeq && TuckSeq)
			{
				const FQuat Q = FQuat::Slerp(Tuck[B].GetRotation(), Fold[B].GetRotation(), FMath::Clamp(Wings.LegsDown, 0.f, 1.f));
				Pose[B].SetRotation(FQuat::Slerp(Pose[B].GetRotation(), Q, W).GetNormalized());
			}
		}
	}
	// the tail closed as in the bind pose: the fan opens in ApplyWingsCS
	for (int32 K = 1; K <= 8; ++K)
	{
		for (const TCHAR* Side : { TEXT("L"), TEXT("R") })
		{
			const FCompactPoseBoneIndex B = FindBone(Bones, *FString::Printf(TEXT("tf_%s_%d"), Side, K));
			if (B.IsValid())
			{
				Pose[B].SetRotation(FQuat::Slerp(Pose[B].GetRotation(), Bones.GetRefPoseTransform(B).GetRotation(), W).GetNormalized());
			}
		}
	}
}

void FBirdAnimProxy::ApplyWingsCS(FCSPose<FCompactPose>& CS) const
{
	const FBoneContainer& Bones = CS.GetPose().GetBoneContainer();
	const float W = FMath::Clamp(Wings.Weight, 0.f, 1.f);
	const FCompactPoseBoneIndex Body = FindBone(Bones, TEXT("body"));
	// the body's own axes (it may be banked): forward, up
	const FQuat BodyQ = Body.IsValid() ? CS.GetComponentSpaceTransform(Body).GetRotation() * RefCS(Bones, Body).Inverse() : FQuat::Identity;
	const FVector Fwd = BodyQ.RotateVector(FVector::XAxisVector), UpV = BodyQ.RotateVector(FVector::ZAxisVector);
	auto Turn = [&](FCompactPoseBoneIndex B, const FVector& Axis, float Rad)
	{
		if (B.IsValid() && FMath::Abs(Rad) > 1e-5f)
		{
			RotateCS(CS, B, FQuat(Axis, Rad * W));
		}
	};
	for (const TCHAR* Side : { TEXT("L"), TEXT("R") })
	{
		const bool bL = Side[0] == TEXT('L');
		const float S = SideSign(Side);
		const float Phase = bL ? Wings.StrokeL : Wings.StrokeR;
		const float Spread = bL ? Wings.SpreadL : Wings.SpreadR;
		// elevation: top of the stroke at phase 0, bottom at 0.5; a pigeon's beat is ~110 degrees, a bit more up than down
		const float Elev = Wings.StrokeAmp * (FMath::DegreesToRadians(55.f) * FMath::Cos(2.f * PI * Phase) + FMath::DegreesToRadians(8.f));
		const FCompactPoseBoneIndex Hum = FindBone(Bones, *FString::Printf(TEXT("humerus_%s"), Side));
		const FCompactPoseBoneIndex Hand = FindBone(Bones, *FString::Printf(TEXT("hand_%s"), Side));
		// first person sweep and droop, the flare's forward reach
		const float Sweep = FMath::DegreesToRadians(Wings.SweepDeg + 25.f * Wings.Flare) * Spread;
		Turn(Hum, UpV, S * Sweep);
		Turn(Hum, Fwd, -S * (Elev + FMath::DegreesToRadians(12.f) * Wings.Flare) + S * Wings.DroopRad * Spread);
		// the downstroke pronates (leading edge down), the flare supinates: twist about the wing's span
		if (Hum.IsValid() && Hand.IsValid())
		{
			const FVector Span = (CS.GetComponentSpaceTransform(Hand).GetLocation() - CS.GetComponentSpaceTransform(Hum).GetLocation()).GetSafeNormal();
			const float Twist = Wings.StrokeAmp * FMath::DegreesToRadians(14.f) * FMath::Sin(2.f * PI * Phase) - FMath::DegreesToRadians(30.f) * Wings.Flare;
			Turn(Hum, Span, S * Twist);
		}
		// the primaries part at the tips when spread
		const float Part = FMath::DegreesToRadians(1.6f) * FMath::Clamp(Spread * 2.f - 1.f, 0.f, 1.f);
		for (int32 i = 1; i <= 12; ++i)
		{
			Turn(FindBone(Bones, *FString::Printf(TEXT("p%d_%s"), i, Side)), UpV, S * Part * (i - 6.f));
		}
		// the tail fans out from the middle pair
		for (int32 K = 1; K <= 8; ++K)
		{
			Turn(FindBone(Bones, *FString::Printf(TEXT("tf_%s_%d"), Side, K)), UpV, S * FMath::DegreesToRadians(9.f) * (K - 0.5f) * Wings.TailFan);
		}
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
	// the outermost tail feather pair, shorter or longer (its bone scales along with the card). Bone names are as
	// Unreal imported them: Blender's "tf.L.6" became "tf_L_6"

	if (FMath::Abs(StreamerScale - 1.f) > 1e-3)
	{
		for (int32 K = 7; K >= 1; --K)
		{
			const FCompactPoseBoneIndex L = FindBone(Bones, *FString::Printf(TEXT("tf_L_%d"), K));
			if (L.IsValid())
			{
				for (const TCHAR* Side : { TEXT("L"), TEXT("R") })
				{
					const FCompactPoseBoneIndex B = FindBone(Bones, *FString::Printf(TEXT("tf_%s_%d"), Side, K));
					if (B.IsValid())
					{
						Output.Pose[B].SetScale3D(FVector(StreamerScale));
					}
				}
				break;
			}
		}
	}
	if (Wings.Weight > 1e-3f)
	{
		ApplyWingsLocal(Output.Pose);
	}
	FCSPose<FCompactPose> CS;
	CS.InitPose(Output.Pose);
	if (Wings.Weight > 1e-3f)
	{
		ApplyWingsCS(CS);
	}
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
	// (worn from its own eye: as far as the neck goes, so the bill stays where the camera looks)
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
			const FRotator Clamped(FMath::Clamp(R.Pitch, Look.bDirect ? -88.f : -70.f, Look.bDirect ? 80.f : 60.f),
			                       FMath::Clamp(FRotator::NormalizeAxis(R.Yaw), Look.bDirect ? -170.f : -150.f, Look.bDirect ? 170.f : 150.f), 0.f);
			Want = Clamped.Vector();
			const FQuat Full = FQuat::FindBetweenNormals(Cur, Want);
			const FQuat Turn = FQuat::Slerp(FQuat::Identity, Full, FMath::Clamp(Look.Weight, 0.f, 1.f));
			if (Look.bDirect)
			{
				// worn from its own eye the neck only swings sideways (half the turn, about the body's up): pitched, it would
				// throw the head forward and down in front of the breast; the head does the rest
				const float Delta = FRotator::NormalizeAxis(Want.Rotation().Yaw - Cur.Rotation().Yaw);
				RotateCS(CS, Neck, FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(0.5f * Delta) * FMath::Clamp(Look.Weight, 0.f, 1.f)));
			}
			else
			{
				RotateCS(CS, Neck, FQuat::Slerp(FQuat::Identity, Turn, 0.35f));
			}
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
	// the neck lifts: the head goes to where NeckReach puts it, the neck swung round toward it from its root and
	// stretched to reach it (a bird's long neck comes out of its feathers), so the head rides up and back over the
	// breast; it keeps looking where it looked
	if (Look.NeckLift > 1e-3f && !Look.NeckReach.IsNearlyZero())
	{
		const float Lift = FMath::Clamp(Look.NeckLift, 0.f, 1.f);
		const FVector Base = CS.GetComponentSpaceTransform(Neck).GetLocation();
		const FTransform Was = CS.GetComponentSpaceTransform(Head);
		const FVector Want = Was.GetLocation() + Look.NeckReach * Lift;
		if (!(Was.GetLocation() - Base).IsNearlyZero() && !(Want - Base).IsNearlyZero())
		{
			RotateCS(CS, Neck, FQuat::FindBetweenVectors(Was.GetLocation() - Base, Want - Base));
			FTransform H = CS.GetComponentSpaceTransform(Head);
			H.SetLocation(Want);
			H.SetRotation(Was.GetRotation());
			TArray<FBoneTransform> Out;
			Out.Add(FBoneTransform(Head, H));
			CS.SafeSetCSBoneTransforms(Out);
		}
	}
	if (Grip.Weight > 1e-3f)
	{
		ApplyGrip(CS);
	}
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose);
	return true;
}

void FBirdAnimProxy::ApplyGrip(FCSPose<FCompactPose>& CS) const
{
	// each toe (two bones: the toe and its tip) bent round the branch, joint by joint: every piece a chord of the
	// circle the toe's own line runs on (the branch's radius plus the toe's thickness) in the plane across the branch
	const FBoneContainer& Bones = CS.GetPose().GetBoneContainer();
	const FVector A = Grip.Axis.GetSafeNormal();
	if (A.IsNearlyZero())
	{
		return;
	}
	const float W = FMath::Clamp(Grip.Weight, 0.f, 1.f);
	const FVector UpAcross = (FVector::ZAxisVector - A * A.Z).GetSafeNormal();
	for (const TCHAR* Side : { TEXT("L"), TEXT("R") })
	{
		// the leg first: knee to ankle to foot as two bones, the foot put on top of the branch right above its axis
		// (one foot may be higher than the other on a sloping branch), the knee bending the way it bent
		const FCompactPoseBoneIndex Shin = FindBone(Bones, *FString::Printf(TEXT("shin_%s"), Side));
		const FCompactPoseBoneIndex Tarsus = FindBone(Bones, *FString::Printf(TEXT("tarsus_%s"), Side));
		const FCompactPoseBoneIndex Foot = FindBone(Bones, *FString::Printf(TEXT("toes_%s"), Side));
		const FCompactPoseBoneIndex Tip = FindBone(Bones, *FString::Printf(TEXT("toetip_%s"), Side));
		if (Shin.IsValid() && Tarsus.IsValid() && Foot.IsValid() && !UpAcross.IsNearlyZero())
		{
			const FVector Knee = CS.GetComponentSpaceTransform(Shin).GetLocation();
			const FVector Ankle = CS.GetComponentSpaceTransform(Tarsus).GetLocation();
			const FVector F = CS.GetComponentSpaceTransform(Foot).GetLocation();
			const float ToeHalf = Tip.IsValid() ? FVector::Dist(F, CS.GetComponentSpaceTransform(Tip).GetLocation()) : FVector::Dist(Ankle, F) * 0.3f;
			const FVector OnAxis = Grip.Centre + A * FVector::DotProduct(F - Grip.Centre, A);
			const FVector Target = FMath::Lerp(F, OnAxis + UpAcross * (Grip.Radius + ToeHalf * 0.12f), W);
			const float La = FVector::Dist(Knee, Ankle), Lb = FVector::Dist(Ankle, F);
			const FVector KT = Target - Knee;
			const float D = FMath::Clamp((float)KT.Size(), FMath::Abs(La - Lb) + 1e-3f, La + Lb - 1e-3f);
			const FVector Dk = KT.GetSafeNormal();
			FVector Pole = (Ankle - Knee) - Dk * FVector::DotProduct(Ankle - Knee, Dk);
			if (Pole.Normalize() && !Dk.IsNearlyZero())
			{
				const float X = (La * La - Lb * Lb + D * D) / (2.f * D);
				const FVector AnkleNew = Knee + Dk * X + Pole * FMath::Sqrt(FMath::Max(0.f, La * La - X * X));
				RotateCS(CS, Shin, FQuat::FindBetweenVectors(Ankle - Knee, AnkleNew - Knee));
				const FVector FootNow = CS.GetComponentSpaceTransform(Foot).GetLocation();
				const FVector AnkleNow = CS.GetComponentSpaceTransform(Tarsus).GetLocation();
				RotateCS(CS, Tarsus, FQuat::FindBetweenVectors(FootNow - AnkleNow, Knee + Dk * D - AnkleNow));
			}
		}
		for (const TCHAR* Toe : { TEXT("toes"), TEXT("hallux") })
		{
			const FCompactPoseBoneIndex B0 = FindBone(Bones, *FString::Printf(TEXT("%s_%s"), Toe, Side));
			const FCompactPoseBoneIndex B1 = FindBone(Bones, *FString::Printf(TEXT("%stip_%s"), Toe[0] == TEXT('t') ? TEXT("toe") : Toe, Side));
			if (!B0.IsValid() || !B1.IsValid())
			{
				continue;
			}
			const FVector Root = CS.GetComponentSpaceTransform(B0).GetLocation();
			const FVector Joint = CS.GetComponentSpaceTransform(B1).GetLocation();
			const float L = FVector::Dist(Root, Joint);   // the joint sits halfway: both pieces are this long
			if (L < 1e-3f)
			{
				continue;
			}
			FVector Dir = (Joint - Root) / L;
			// the circle: round the branch's axis, through the toe's root (never inside the wood plus the toe's thickness)
			const FVector OnAxis = Grip.Centre + A * FVector::DotProduct(Root - Grip.Centre, A);
			const float Rr = FMath::Max(FVector::Dist(Root, OnAxis), Grip.Radius + L * 0.12f);
			float Wrapped = 0.f;
			for (const FCompactPoseBoneIndex Bi : { B0, B1 })
			{
				const FVector P = CS.GetComponentSpaceTransform(Bi).GetLocation();
				const FVector C = Grip.Centre + A * FVector::DotProduct(P - Grip.Centre, A);
				const FVector Out = (P - C).GetSafeNormal();
				FVector Flat = Dir - A * FVector::DotProduct(Dir, A);
				const float LFlat = L * Flat.Size();
				Flat = Flat.GetSafeNormal();
				if (Out.IsNearlyZero() || Flat.IsNearlyZero())
				{
					break;
				}
				// the tangent at P the way the toe points, and the chord's angle in from it toward the axis
				FVector Tangent = FVector::CrossProduct(A, Out);
				if (FVector::DotProduct(Tangent, Flat) < 0.f) Tangent = -Tangent;
				float Phi = 2.f * FMath::Asin(FMath::Clamp(LFlat / (2.f * Rr), 0.f, 1.f));
				Phi = FMath::Min(Phi, FMath::Max(0.f, FMath::DegreesToRadians(300.f) - Wrapped));   // the claws stop short of the root
				Wrapped += Phi;
				const FVector Want = Tangent * FMath::Cos(Phi * 0.5f) - Out * FMath::Sin(Phi * 0.5f);
				const FQuat Q = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(Flat, Want), W);
				RotateCS(CS, Bi, Q);
				Dir = Q.RotateVector(Dir);
			}
		}
	}
}
