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

	/** One of the cast's shared clips (same skeleton): /Game/Characters/Humans/Anims/A_Human_<Name>. */
	UAnimSequence* LoadHumanAnim(const TCHAR* Name)
	{
		return LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Characters/Humans/Anims/A_Human_%s.A_Human_%s"), Name, Name),
			nullptr, LOAD_NoWarn | LOAD_Quiet);
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
	struct FLookShare { FName Bone; float Up; float Twist; float FlexDeg; };
	const FLookShare LookShares[] = {
		{ TEXT("spine_01"), 0.04f, 0.125f, 1.25f },
		{ TEXT("spine_02"), 0.04f, 0.125f, 1.25f },
		{ TEXT("spine_03"), 0.22f, 0.30f, 8.f },
		{ TEXT("neck_01"), 0.45f, 0.45f, 14.f },
		{ TEXT("head"), 0.f, 0.f, 5.f },
	};
}

void FHeroAnimProxy::KeepHandsOutsideClothes(FCompactPose& Pose)
{
	const FBoneContainer& Bones = Pose.GetBoneContainer();
	auto Find = [&](const FName Name)
	{
		const int32 M = Bones.GetPoseBoneIndexForBoneName(Name);
		return M == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(M));
	};
	auto CS = [&](FCompactPoseBoneIndex B)
	{
		FTransform T = Pose[B];
		for (FCompactPoseBoneIndex Up = Pose.GetParentBoneIndex(B); Up.IsValid(); Up = Pose.GetParentBoneIndex(Up))
		{
			T = T * Pose[Up];
		}
		return T;
	};
	// made once: this runs every evaluation
	static const FName PelvisName(TEXT("pelvis")), ChestName(TEXT("spine_03"));
	static const FName UpperNames[] = { TEXT("upperarm_l"), TEXT("upperarm_r") }, HandNames[] = { TEXT("hand_l"), TEXT("hand_r") };
	const FCompactPoseBoneIndex Pelvis = Find(PelvisName), Chest = Find(ChestName);
	if (!Pelvis.IsValid() || !Chest.IsValid())
	{
		return;
	}
	const float ZLow = (float)CS(Pelvis).GetLocation().Z - 25.f, ZHigh = (float)CS(Chest).GetLocation().Z + 15.f;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FCompactPoseBoneIndex Upper = Find(UpperNames[Side]);
		const FCompactPoseBoneIndex Hand = Find(HandNames[Side]);
		if (!Upper.IsValid() || !Hand.IsValid())
		{
			continue;
		}
		const FTransform UpperCS = CS(Upper);
		const FVector Shoulder = UpperCS.GetLocation();
		const FVector H = CS(Hand).GetLocation();
		const float HZ = (float)H.Z, HY = (float)H.Y;
		const float Sign = Shoulder.X >= 0.f ? 1.f : -1.f;     // the mesh faces +Y: its left is +X
		// how much the hand is in the clothes' way: by height along the torso and depth in front of / behind it
		const float InBand = FMath::SmoothStep(ZLow - 10.f, ZLow, HZ) * (1.f - FMath::SmoothStep(ZHigh, ZHigh + 10.f, HZ));
		const float InDepth = 1.f - FMath::SmoothStep(ClothesHalfDepth, ClothesHalfDepth + 12.f, FMath::Abs(HY));
		const float Want = ClothesHalfWidth * InBand * InDepth;
		const float Out = Sign * (float)H.X;
		if (Want <= Out)
		{
			continue;
		}
		const FVector From = H - Shoulder;
		const FVector To = FVector(Sign * Want, H.Y, H.Z) - Shoulder;
		const FQuat Delta = FQuat::FindBetweenVectors(From, To.GetSafeNormal() * From.Size());
		FTransform NewCS = UpperCS;
		NewCS.SetRotation((Delta * UpperCS.GetRotation()).GetNormalized());
		const FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Upper);
		Pose[Upper] = Parent.IsValid() ? NewCS.GetRelativeTransform(CS(Parent)) : NewCS;
	}
}

void FHeroAnimProxy::PlaceSwimmer(FCompactPose& Pose)
{
	const FBoneContainer& Bones = Pose.GetBoneContainer();
	const int32 HeadMesh = Bones.GetPoseBoneIndexForBoneName(TEXT("head"));
	if (HeadMesh == INDEX_NONE)
	{
		return;
	}
	const FCompactPoseBoneIndex Head = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(HeadMesh));
	if (!Head.IsValid())
	{
		return;
	}
	auto HeadCS = [&](const FCompactPose& P)
	{
		FTransform T = P[Head];
		for (FCompactPoseBoneIndex Up = P.GetParentBoneIndex(Head); Up.IsValid(); Up = P.GetParentBoneIndex(Up))
		{
			T = T * P[Up];
		}
		return T.GetLocation();
	};
	if (!bStandHead && Idle)
	{
		FCompactPose Stand;
		Stand.SetBoneContainer(&Bones);
		FBlendedCurve Curve;
		Curve.InitFrom(Bones);
		UE::Anim::FStackAttributeContainer Attr;
		FAnimationPoseData Data(Stand, Curve, Attr);
		Idle->GetAnimationPose(Data, FAnimExtractContext(0.0, false));
		Stand[FCompactPoseBoneIndex(0)].SetTranslation(FVector::ZeroVector);
		StandHead = HeadCS(Stand);
		bStandHead = true;
	}
	// the head where a standing one is (the eyes, the camera, stay put), the body turned about it: lying along the
	// stroke (the mesh faces +Y, so pitching the head up is a turn about +X)
	const FVector H = HeadCS(Pose);
	const FQuat Q = FQuat::Slerp(FQuat::Identity, FQuat(FVector::XAxisVector, FMath::DegreesToRadians(State.SwimPitch)), State.Swim);
	const FVector Want = FMath::Lerp(H, StandHead, State.Swim);
	const FTransform R(Q, Want - Q.RotateVector(H));
	const FCompactPoseBoneIndex Root(0);
	Pose[Root] = Pose[Root] * R;
}

void UHeroAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	Idle = LoadAnim(TEXT("Idle"));
	SwimIdle = LoadHumanAnim(TEXT("SwimIdle"));
	SwimFwd = LoadHumanAnim(TEXT("SwimFwd"));
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
	SwimIdle = Hero->SwimIdle;
	SwimFwd = Hero->SwimFwd;
	WalkClipSpeed = Hero->WalkClipSpeed;
	JogClipSpeed = Hero->JogClipSpeed;
	Reach = Hero->Reach;
	Bike = Hero->Bike;
	Ladder = Hero->Ladder;
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
	TArray<FEntry, TInlineAllocator<7>> Entries;
	// in the water the swim clips take over (treading water, the forward stroke), played by their own clock
	const float Swim = SwimIdle && SwimFwd ? State.Swim : 0.f;
	const float Land = 1.f - Swim;
	const bool bLoco = Walk.Num() == 8 && Jog.Num() == 8;
	// crouched the steps are short and slow: the walk plays in full from a slow creep on (played by distance its feet stay planted),
	// not half-blended with the idle, which would slide them
	const float Moving = bLoco ? FMath::Lerp(FMath::Clamp(State.Speed / WalkClipSpeed, 0.f, 1.f), FMath::Clamp(State.Speed / 30.f, 0.f, 1.f), State.Crouch) : 0.f;
	const float Run = FMath::Clamp((State.Speed - WalkClipSpeed) / (JogClipSpeed - WalkClipSpeed), 0.f, 1.f);
	if (Moving < 1.f && Land > 1e-3f)
	{
		Entries.Add({ Idle, FMath::Fmod(State.IdleTime, (double)Idle->GetPlayLength()), (1.f - Moving) * Land });
	}
	if (Moving > 0.f && Land > 1e-3f)
	{
		const float A = FMath::Fmod(State.MoveAngle + 360.f, 360.f) / 45.f;
		const int32 I0 = FMath::FloorToInt(A) % 8;
		const int32 I1 = (I0 + 1) % 8;
		const float F = A - FMath::FloorToFloat(A);
		auto Add = [&](const TArray<TObjectPtr<UAnimSequence>>& Set, const TArray<float>& Stride, int32 I, float W)
		{
			if (W * Land > 1e-3f && Set[I])
			{
				Entries.Add({ Set[I], FMath::Fmod(State.Distance, (double)Stride[I]) / Stride[I] * Set[I]->GetPlayLength(), W * Land });
			}
		};
		Add(Walk, WalkStride, I0, Moving * (1.f - Run) * (1.f - F));
		Add(Walk, WalkStride, I1, Moving * (1.f - Run) * F);
		Add(Jog, JogStride, I0, Moving * Run * (1.f - F));
		Add(Jog, JogStride, I1, Moving * Run * F);
	}
	if (Swim > 1e-3f)
	{
		const float Fwd = FMath::Clamp(State.SwimMove, 0.f, 1.f);
		if (Fwd < 1.f)
		{
			Entries.Add({ SwimIdle, FMath::Fmod(State.SwimTime, (double)SwimIdle->GetPlayLength()), Swim * (1.f - Fwd) });
		}
		if (Fwd > 0.f)
		{
			Entries.Add({ SwimFwd, FMath::Fmod(State.SwimTime, (double)SwimFwd->GetPlayLength()), Swim * Fwd });
		}
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
		TArray<FCompactPose, TInlineAllocator<7>> Poses;
		TArray<FBlendedCurve, TInlineAllocator<7>> Curves;
		TArray<UE::Anim::FStackAttributeContainer, TInlineAllocator<7>> Attrs;
		TArray<float, TInlineAllocator<7>> Weights;
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
	if (Swim > 1e-3f)
	{
		PlaceSwimmer(Output.Pose);
	}

	// the mannequin's arms swing close to its slim body; the hero's oversized overshirt and hoodie are
	// far wider, so the hands would pass through the clothes (and, running, in front of the eyes)
	KeepHandsOutsideClothes(Output.Pose);

	// sneaking up on a bird: down on the haunches, the walk's feet kept where they step
	HandReach::ApplyCrouch(Output.Pose, State.Crouch, CrouchDrop);
	// on a bench (a train's): the clips' idle stands, the body sits
	HandReach::ApplySit(Output.Pose, State.Sit, State.SeatZ);
	// on a bicycle: hips on the saddle, feet on the pedals, the trunk leaned to the bars (the arms come after the look)
	LyudBikeRider::ApplyBody(Output.Pose, Bike);
	// on a ladder: the hips out from it, the feet on its rungs (the hands come after the look)
	LyudBikeRider::ApplyBody(Output.Pose, Ladder);

	// reaching for something: the body crouches and leans to it first (the look builds on that)
	const FPalmFrame AnimPalm = Reach.IsActive() ? HandReach::CurrentPalm(Output.Pose) : FPalmFrame();
	if (Reach.IsActive())
	{
		HandReach::ApplyPosture(Output.Pose, Reach);
	}

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
	if (Reach.IsActive())
	{
		HandReach::ApplyArm(Output.Pose, Reach, AnimPalm);
	}
	LyudBikeRider::ApplyArms(Output.Pose, Bike);
	LyudBikeRider::ApplyArms(Output.Pose, Ladder);
	return true;
}
