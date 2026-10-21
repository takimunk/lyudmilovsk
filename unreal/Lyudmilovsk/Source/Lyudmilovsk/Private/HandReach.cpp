#include "HandReach.h"

#include "BonePose.h"

namespace
{
	const FName Pelvis(TEXT("pelvis"));
	const FName UpperArm(TEXT("upperarm_r"));
	const FName LowerArm(TEXT("lowerarm_r"));
	const FName Hand(TEXT("hand_r"));
	const FName Thumb[] = { TEXT("thumb_01_r"), TEXT("thumb_02_r"), TEXT("thumb_03_r") };
	const FName Pinky(TEXT("pinky_01_r"));
	/** From the bones to the skin of the hero's palm and fingers, cm (SK_Hero's MakeHuman hand). */
	constexpr float PalmSkin = 1.7f;
	constexpr float FingerSkin = 0.9f;
	const FName Middle[] = { TEXT("middle_01_r"), TEXT("middle_02_r"), TEXT("middle_03_r") };

	/** The fingers: bones from the knuckle out and how far each joint folds at a full closure (degrees). */
	struct FFingerDef { TArray<FName, TInlineAllocator<3>> Bones; TArray<float, TInlineAllocator<3>> MaxFold; float TipLength; };
	const TArray<FFingerDef>& Fingers()
	{
		static const TArray<FFingerDef> F = {
			{ { TEXT("index_01_r"), TEXT("index_02_r"), TEXT("index_03_r") }, { 75.f, 90.f, 60.f }, 2.2f },
			{ { TEXT("middle_01_r"), TEXT("middle_02_r"), TEXT("middle_03_r") }, { 75.f, 90.f, 60.f }, 2.4f },
			{ { TEXT("ring_01_r") }, { 150.f }, 5.f },
			{ { TEXT("pinky_01_r") }, { 150.f }, 4.2f },
		};
		return F;
	}

	FCompactPoseBoneIndex Find(const FBoneContainer& Bones, FName Name)
	{
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(Name);
		return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	}

	FTransform ComponentSpace(const FCompactPose& P, FCompactPoseBoneIndex B)
	{
		FTransform T = P[B];
		for (FCompactPoseBoneIndex Up = P.GetParentBoneIndex(B); Up.IsValid(); Up = P.GetParentBoneIndex(Up))
		{
			T = T * P[Up];
		}
		return T;
	}

	void SetComponentSpace(FCompactPose& P, FCompactPoseBoneIndex B, const FTransform& CS)
	{
		const FCompactPoseBoneIndex Up = P.GetParentBoneIndex(B);
		P[B] = Up.IsValid() ? CS.GetRelativeTransform(ComponentSpace(P, Up)) : CS;
	}

	/** Turns a bone (and so everything below it) about its own joint by Delta, a rotation in component space. */
	void RotateBone(FCompactPose& P, FCompactPoseBoneIndex B, const FQuat& Delta)
	{
		FTransform CS = ComponentSpace(P, B);
		CS.SetRotation((Delta * CS.GetRotation()).GetNormalized());
		SetComponentSpace(P, B, CS);
	}

	FVector Loc(const FCompactPose& P, FCompactPoseBoneIndex B)
	{
		return ComponentSpace(P, B).GetLocation();
	}

	/** Bends a two-bone limb so its end reaches Target (clamped to the limb's length); Pole is the way the joint bends out. */
	void TwoBoneIK(FCompactPose& P, FCompactPoseBoneIndex A, FCompactPoseBoneIndex B, FCompactPoseBoneIndex C, const FVector& Target, const FVector& Pole)
	{
		const FVector a = Loc(P, A), b = Loc(P, B), c = Loc(P, C);
		const float L1 = (b - a).Size(), L2 = (c - b).Size();
		const FVector ToTarget = Target - a;
		const float Dist = FMath::Clamp((float)ToTarget.Size(), FMath::Abs(L1 - L2) + 0.5f, L1 + L2 - 0.05f);
		const FVector U = ToTarget.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		const float CosA = FMath::Clamp((L1 * L1 + Dist * Dist - L2 * L2) / (2.f * L1 * Dist), -1.f, 1.f);
		const float SinA = FMath::Sqrt(1.f - CosA * CosA);
		FVector N = Pole - U * FVector::DotProduct(Pole, U);
		if (N.SizeSquared() < 1e-4f)
		{
			N = (b - a) - U * FVector::DotProduct(b - a, U);
		}
		N = N.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		const FVector NewB = a + U * (L1 * CosA) + N * (L1 * SinA);
		RotateBone(P, A, FQuat::FindBetweenVectors(b - a, NewB - a));
		const FVector b2 = Loc(P, B), c2 = Loc(P, C);
		RotateBone(P, B, FQuat::FindBetweenVectors(c2 - b2, (a + U * Dist) - b2));
	}

	/** The fingers' bones, the palm frame and everything else the hand solve reads, found once per call. */
	struct FHand
	{
		FCompactPoseBoneIndex Upper, Lower, Wrist, MiddleKnuckle, ThumbBase, PinkyBase;

		explicit FHand(const FBoneContainer& Bones)
			: Upper(Find(Bones, UpperArm)), Lower(Find(Bones, LowerArm)), Wrist(Find(Bones, Hand))
			, MiddleKnuckle(Find(Bones, Middle[0])), ThumbBase(Find(Bones, Thumb[0])), PinkyBase(Find(Bones, Pinky))
		{
		}
		bool IsValid() const { return Upper.IsValid() && Lower.IsValid() && Wrist.IsValid() && MiddleKnuckle.IsValid() && ThumbBase.IsValid() && PinkyBase.IsValid(); }
	};

	FPalmFrame PalmOf(const FCompactPose& P, const FHand& H, float NormalSign)
	{
		FPalmFrame F;
		const FVector Wrist = Loc(P, H.Wrist), Knuckle = Loc(P, H.MiddleKnuckle);
		F.FingerDir = (Knuckle - Wrist).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		FVector Across = Loc(P, H.ThumbBase) - Loc(P, H.PinkyBase);
		Across -= F.FingerDir * FVector::DotProduct(Across, F.FingerDir);
		Across = Across.GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
		F.PalmNormal = FVector::CrossProduct(Across, F.FingerDir) * NormalSign;
		F.Palm = Wrist + F.FingerDir * (0.6f * (Knuckle - Wrist).Size());
		F.NormalSign = NormalSign;
		F.bValid = true;
		return F;
	}

	float DistanceToShape(const FReachShape& S, const FVector& P)
	{
		return S.Distance(P);
	}

	/** A finger bent by Fold (0..1 of its full curl) about Axis: joints and tip in component space. */
	void FingerPoints(const FCompactPose& P, const FFingerDef& F, const TArray<FCompactPoseBoneIndex, TInlineAllocator<3>>& Idx, const FVector& Axis,
		float Fold, const FVector& FingerDir, TArray<FVector, TInlineAllocator<4>>& Out)
	{
		Out.Reset();
		TArray<FVector, TInlineAllocator<4>> Joint;
		for (FCompactPoseBoneIndex B : Idx)
		{
			Joint.Add(Loc(P, B));
		}
		// the tip lies one segment beyond the last joint (a finger of one bone has no further joint to measure)
		FVector Seg = Joint.Num() > 1 ? (Joint.Last() - Joint[Joint.Num() - 2]) : FingerDir * F.TipLength;
		Seg = Seg.GetSafeNormal() * F.TipLength;
		Joint.Add(Joint.Last() + Seg);
		// fold each joint about its own pivot, the folds of the joints before it carrying it along
		TArray<FVector, TInlineAllocator<4>> Pos = Joint;
		for (int32 J = 0; J < Idx.Num(); ++J)
		{
			const FQuat Q(Axis, FMath::DegreesToRadians(F.MaxFold[J] * Fold));
			for (int32 K = J + 1; K < Pos.Num(); ++K)
			{
				Pos[K] = Pos[J] + Q.RotateVector(Pos[K] - Pos[J]);
			}
		}
		Out = Pos;
	}
}

float FReachPrim::Distance(const FVector& P) const
{
	const FVector L = Xf.InverseTransformPositionNoScale(P);
	switch (Kind)
	{
	case EKind::Sphere:
		return (float)L.Size() - Radius;
	case EKind::Capsule:
		return (float)(L - FVector(0.f, 0.f, FMath::Clamp((float)L.Z, -HalfLength, HalfLength))).Size() - Radius;
	default:
	{
		const FVector Q = L.GetAbs() - Extent;
		const FVector Outside(FMath::Max(Q.X, 0.0), FMath::Max(Q.Y, 0.0), FMath::Max(Q.Z, 0.0));
		return (float)Outside.Size() + FMath::Min((float)Q.GetMax(), 0.f);
	}
	}
}

float FReachShape::Distance(const FVector& P) const
{
	float D = TNumericLimits<float>::Max();
	for (const FReachPrim& Pr : Prims)
	{
		D = FMath::Min(D, Pr.Distance(P));
	}
	return D;
}

FVector FReachShape::Normal(const FVector& P) const
{
	constexpr float E = 0.2f;
	const FVector G(Distance(P + FVector(E, 0, 0)) - Distance(P - FVector(E, 0, 0)),
		Distance(P + FVector(0, E, 0)) - Distance(P - FVector(0, E, 0)),
		Distance(P + FVector(0, 0, E)) - Distance(P - FVector(0, 0, E)));
	return G.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
}

bool FReachShape::Trace(const FVector& Start, const FVector& Target, FVector& OutHit) const
{
	const FVector Dir = (Target - Start).GetSafeNormal();
	const float Len = (float)(Target - Start).Size() + 50.f;
	float T = 0.f;
	for (int32 i = 0; i < 128 && T < Len; ++i)
	{
		const FVector P = Start + Dir * T;
		const float D = Distance(P);
		if (D < 0.05f)
		{
			OutHit = P;
			return true;
		}
		T += FMath::Max(D, 0.05f);
	}
	return false;
}

FReachShape FReachShape::Transformed(const FTransform& T) const
{
	FReachShape Out;
	for (FReachPrim Pr : Prims)
	{
		Pr.Xf = Pr.Xf * FTransform(T.GetRotation(), T.GetTranslation());
		Out.Prims.Add(Pr);
	}
	return Out;
}

FBox FReachShape::Bounds() const
{
	FBox B(ForceInit);
	for (const FReachPrim& Pr : Prims)
	{
		const FVector E = Pr.Kind == FReachPrim::EKind::Box ? Pr.Extent
			: Pr.Kind == FReachPrim::EKind::Capsule ? FVector(Pr.Radius, Pr.Radius, Pr.HalfLength + Pr.Radius) : FVector(Pr.Radius);
		B += FBox(-E, E).TransformBy(Pr.Xf);
	}
	return B;
}

void FReachShape::AddCapsule(const FVector& Center, const FVector& Axis, float HalfLength, float Radius)
{
	FReachPrim Pr;
	Pr.Kind = HalfLength > 0.01f ? FReachPrim::EKind::Capsule : FReachPrim::EKind::Sphere;
	Pr.Xf = FTransform(FRotationMatrix::MakeFromZ(Axis.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector)).ToQuat(), Center);
	Pr.HalfLength = HalfLength;
	Pr.Radius = Radius;
	Prims.Add(Pr);
}

namespace HandReach
{
	FPalmFrame CurrentPalm(const FCompactPose& Pose)
	{
		const FHand H(Pose.GetBoneContainer());
		if (!H.IsValid())
		{
			return FPalmFrame();
		}
		// "out of the palm" is the side the thumb tip sits on (the thumb folds in front of the palm in any
		// relaxed hand). Judging it by where the palm faces in the animation flipped with the clip, and then
		// the hand turned its back to the item and the fingers folded the wrong way.
		FPalmFrame F = PalmOf(Pose, H, 1.f);
		const FCompactPoseBoneIndex ThumbTip = Find(Pose.GetBoneContainer(), Thumb[2]);
		const FVector Tip = ThumbTip.IsValid() ? Loc(Pose, ThumbTip) : Loc(Pose, H.ThumbBase);
		if (FVector::DotProduct(Tip - F.Palm, F.PalmNormal) < 0.f)
		{
			F = PalmOf(Pose, H, -1.f);
		}
		return F;
	}

	void ApplyPosture(FCompactPose& Pose, const FHeroReachPose& Reach)
	{
		const float W = FMath::Clamp(Reach.Weight, 0.f, 1.f) * Reach.PostureWeight;
		if (W < 0.001f)
		{
			return;
		}
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		const FHand H(Bones);
		const FCompactPoseBoneIndex Spine[] = { Find(Bones, TEXT("spine_01")), Find(Bones, TEXT("spine_02")), Find(Bones, TEXT("spine_03")) };
		if (!H.IsValid() || !Spine[0].IsValid() || !Spine[1].IsValid() || !Spine[2].IsValid())
		{
			return;
		}
		// only the body above the legs bends: the feet stay where they stand, no crouch (Dima: just lean
		// and reach). The least forward bend about the lower spine that brings the shoulder within the arm's
		// length of the wrist target, the head not ending up over the hand.
		const FVector Shoulder = Loc(Pose, H.Upper);
		const float ArmLength = (float)((Loc(Pose, H.Lower) - Shoulder).Size() + (Loc(Pose, H.Wrist) - Loc(Pose, H.Lower)).Size());
		const FVector Pivot = Loc(Pose, Spine[0]);
		const float PalmReach = 0.6f * (float)(Loc(Pose, H.MiddleKnuckle) - Loc(Pose, H.Wrist)).Size();
		const FVector WristTarget = Reach.Palm - Reach.FingerDir * PalmReach;
		const FCompactPoseBoneIndex HeadBone = Find(Bones, TEXT("head"));
		const FVector Head = HeadBone.IsValid() ? Loc(Pose, HeadBone) : Shoulder + FVector(0.f, 0.f, 20.f);
		constexpr float MaxLeanDeg = 100.f, HeadClear = 10.f;
		float Best = MaxLeanDeg, BestDist = TNumericLimits<float>::Max();
		for (float A = 0.f; A <= MaxLeanDeg; A += 2.f)
		{
			// the mesh faces +Y: a negative angle about +X tips the back forward
			const FQuat Tip(FVector::XAxisVector, -FMath::DegreesToRadians(A));
			const FVector Sh = Pivot + Tip.RotateVector(Shoulder - Pivot);
			const FVector Hd = Pivot + Tip.RotateVector(Head - Pivot);
			const float D = (float)(WristTarget - Sh).Size();
			if (D <= 0.95f * ArmLength && FVector::Dist2D(Hd, Reach.Palm) >= HeadClear)
			{
				Best = A;
				break;
			}
			if (D < BestDist)
			{
				BestDist = D;
				Best = A;
			}
		}
		const float Lean = Best * W;
		if (Lean < 0.01f)
		{
			return;
		}
		// a hinge at the hips takes most of it (the thighs turned back so the legs stay where they stand,
		// down to the floor), the back the rest, spread over the spine bones about their own joints
		const FCompactPoseBoneIndex Pel = Find(Bones, Pelvis);
		const FCompactPoseBoneIndex Thighs[] = { Find(Bones, TEXT("thigh_l")), Find(Bones, TEXT("thigh_r")) };
		float Back = Lean;
		if (Pel.IsValid() && Thighs[0].IsValid() && Thighs[1].IsValid())
		{
			const float Hips = 0.55f * Lean;
			TArray<FTransform, TInlineAllocator<2>> ThighCS = { ComponentSpace(Pose, Thighs[0]), ComponentSpace(Pose, Thighs[1]) };
			RotateBone(Pose, Pel, FQuat(FVector::XAxisVector, -FMath::DegreesToRadians(Hips)));
			for (int32 i = 0; i < 2; ++i)
			{
				FTransform T = ComponentSpace(Pose, Thighs[i]);
				T.SetRotation(ThighCS[i].GetRotation());
				SetComponentSpace(Pose, Thighs[i], T);
			}
			Back = Lean - Hips;
		}
		const float Share[] = { 0.45f, 0.3f, 0.25f };
		for (int32 i = 0; i < 3; ++i)
		{
			RotateBone(Pose, Spine[i], FQuat(FVector::XAxisVector, -FMath::DegreesToRadians(Back * Share[i])));
		}
	}

	void ApplyArm(FCompactPose& Pose, const FHeroReachPose& Reach, const FPalmFrame& AnimPalm)
	{
		const float W = FMath::Clamp(Reach.Weight, 0.f, 1.f);
		if (W < 0.001f)
		{
			return;
		}
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		const FHand H(Bones);
		if (!H.IsValid())
		{
			return;
		}
		const float Sign = AnimPalm.bValid ? AnimPalm.NormalSign : 1.f;
		const float Ease = FMath::SmoothStep(0.f, 1.f, W);
		// blend from the animation's palm to the wanted one
		FVector Palm = Reach.Palm, Fd = Reach.FingerDir, Pn = Reach.PalmNormal;
		if (AnimPalm.bValid && W < 1.f)
		{
			Palm = FMath::Lerp(AnimPalm.Palm, Reach.Palm, Ease);
			Fd = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(AnimPalm.FingerDir, Reach.FingerDir), Ease).RotateVector(AnimPalm.FingerDir);
			Pn = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(AnimPalm.PalmNormal, Reach.PalmNormal), Ease).RotateVector(AnimPalm.PalmNormal);
		}
		Fd = Fd.GetSafeNormal();
		Pn = (Pn - Fd * FVector::DotProduct(Pn, Fd)).GetSafeNormal();
		// the palm frame runs along the bones, inside the hand: its skin is PalmSkin further out. Keep that
		// skin on the item's surface, never in it (else the hand sinks into the bottle)
		if (Reach.bHasShape && !Reach.Shape.IsEmpty())
		{
			const float D = DistanceToShape(Reach.Shape, Palm + Pn * PalmSkin);
			if (D < 0.f)
			{
				Palm -= Pn * (-D);
			}
		}

		// arm: the wrist goes where the palm minus the half-palm is; the elbow bends out and down
		const float PalmReach = 0.6f * (float)(Loc(Pose, H.MiddleKnuckle) - Loc(Pose, H.Wrist)).Size();
		const FVector Wrist = Palm - Fd * PalmReach;
		const FVector Shoulder = Loc(Pose, H.Upper);
		FVector Outward = Loc(Pose, H.Upper) - Loc(Pose, Find(Bones, Pelvis));
		Outward.Z = 0.f;
		const FVector Pole = Outward.GetSafeNormal() * 0.8f + FVector(0.f, 0.f, -1.f) + (Wrist - Shoulder).GetSafeNormal() * -0.2f;
		TwoBoneIK(Pose, H.Upper, H.Lower, H.Wrist, Wrist, Pole);

		// the hand turns to the palm frame
		FPalmFrame Cur = PalmOf(Pose, H, Sign);
		const FQuat Q1 = FQuat::FindBetweenNormals(Cur.FingerDir, Fd);
		const FVector Pn1 = Q1.RotateVector(Cur.PalmNormal);
		const FVector P1 = (Pn1 - Fd * FVector::DotProduct(Pn1, Fd)).GetSafeNormal();
		const float Roll = FMath::Atan2((float)FVector::DotProduct(FVector::CrossProduct(P1, Pn), Fd), (float)FVector::DotProduct(P1, Pn));
		RotateBone(Pose, H.Wrist, FQuat(Fd, Roll) * Q1);
		Cur = PalmOf(Pose, H, Sign);

		// fingers: close each until it touches the shape, thumb across the palm
		const float Closing = FMath::Clamp(Reach.Curl, 0.f, 1.f);
		const FVector Curl = FVector::CrossProduct(Cur.FingerDir, Cur.PalmNormal).GetSafeNormal();
		auto Fold = [&](const FFingerDef& F, const FVector& Axis0, const FVector& Toward, bool bContact)
		{
			TArray<FCompactPoseBoneIndex, TInlineAllocator<3>> Idx;
			for (FName N : F.Bones)
			{
				Idx.Add(Find(Bones, N));
				if (!Idx.Last().IsValid())
				{
					return;
				}
			}
			// the sign of the axis: the one that brings the tip toward the palm side
			TArray<FVector, TInlineAllocator<4>> A, B;
			FingerPoints(Pose, F, Idx, Axis0, 0.f, Cur.FingerDir, A);
			FingerPoints(Pose, F, Idx, Axis0, 0.5f, Cur.FingerDir, B);
			const FVector Axis = (B.Last() - Toward).SizeSquared() < (A.Last() - Toward).SizeSquared() ? Axis0 : -Axis0;
			float Amount = 0.55f * Closing;           // loose, half-closed
			if (Reach.bHasShape && bContact)
			{
				Amount = 1.f;
				for (float T = 0.f; T <= 1.f; T += 0.04f)
				{
					TArray<FVector, TInlineAllocator<4>> Pts;
					FingerPoints(Pose, F, Idx, Axis, T, Cur.FingerDir, Pts);
					bool bTouch = false;
					for (int32 K = 1; K < Pts.Num(); ++K)
					{
						bTouch |= DistanceToShape(Reach.Shape, Pts[K]) <= FingerSkin;   // the bone line is FingerSkin inside the finger's skin
					}
					if (bTouch)
					{
						Amount = T * 1.04f;
						break;
					}
				}
				Amount = FMath::Min(Amount, Closing);
			}
			else if (!Reach.bHasShape)
			{
				Amount = 0.9f * Closing;
			}
			for (int32 J = 0; J < Idx.Num(); ++J)
			{
				RotateBone(Pose, Idx[J], FQuat(Axis, FMath::DegreesToRadians(F.MaxFold[J] * Amount)));
			}
		};
		const FVector TowardPalm = Cur.Palm + Cur.PalmNormal * 6.f;
		for (const FFingerDef& F : Fingers())
		{
			Fold(F, Curl, TowardPalm, true);
		}
		// thumb: swings across the palm (about the palm normal), then folds a little
		{
			FFingerDef Th;
			for (FName N : Thumb)
			{
				Th.Bones.Add(N);
			}
			Th.MaxFold = { 50.f, 40.f, 35.f };
			Th.TipLength = 2.4f;
			Fold(Th, Cur.PalmNormal, Cur.Palm + Cur.PalmNormal * 3.f - Cur.FingerDir * 2.f, true);
		}
	}
}
