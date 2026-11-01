#include "Bikes/LyudBikeRider.h"

#include "BonePose.h"

// The same small pose tools as HandReach.cpp (component space by walking up the parents), kept apart so either can change.
namespace
{
	/** From the bones to the skin of the palm and the fingers, and from the ball of the foot's bone to the shoe's sole, cm. */
	constexpr float PalmSkin = 1.7f;
	constexpr float FingerSkin = 0.9f;
	constexpr float BallOverSole = 4.f;

	FCompactPoseBoneIndex BikeFind(const FBoneContainer& Bones, const FString& Name)
	{
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(FName(*Name));
		return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	}

	FTransform BikeCS(const FCompactPose& P, FCompactPoseBoneIndex B)
	{
		FTransform T = P[B];
		for (FCompactPoseBoneIndex Up = P.GetParentBoneIndex(B); Up.IsValid(); Up = P.GetParentBoneIndex(Up))
		{
			T = T * P[Up];
		}
		return T;
	}

	/** The bone in the skeleton's reference pose (the T or A pose the meshes were rigged in: feet flat, toes forward). */
	FTransform BikeRefCS(const FCompactPose& P, FCompactPoseBoneIndex B)
	{
		const FBoneContainer& Bones = P.GetBoneContainer();
		FTransform T = Bones.GetRefPoseTransform(B);
		for (FCompactPoseBoneIndex Up = P.GetParentBoneIndex(B); Up.IsValid(); Up = P.GetParentBoneIndex(Up))
		{
			T = T * Bones.GetRefPoseTransform(Up);
		}
		return T;
	}

	void BikeSetCS(FCompactPose& P, FCompactPoseBoneIndex B, const FTransform& CS)
	{
		const FCompactPoseBoneIndex Up = P.GetParentBoneIndex(B);
		P[B] = Up.IsValid() ? CS.GetRelativeTransform(BikeCS(P, Up)) : CS;
		P[B].NormalizeRotation();
	}

	void BikeTurn(FCompactPose& P, FCompactPoseBoneIndex B, const FQuat& Delta)
	{
		if (!B.IsValid())
		{
			return;
		}
		FTransform CS = BikeCS(P, B);
		CS.SetRotation((Delta * CS.GetRotation()).GetNormalized());
		BikeSetCS(P, B, CS);
	}

	FVector BikeLoc(const FCompactPose& P, FCompactPoseBoneIndex B)
	{
		return BikeCS(P, B).GetLocation();
	}

	void BikeTwoBoneIK(FCompactPose& P, FCompactPoseBoneIndex A, FCompactPoseBoneIndex B, FCompactPoseBoneIndex C, const FVector& Target, const FVector& Pole)
	{
		const FVector a = BikeLoc(P, A), b = BikeLoc(P, B), c = BikeLoc(P, C);
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
		BikeTurn(P, A, FQuat::FindBetweenVectors(b - a, NewB - a));
		const FVector b2 = BikeLoc(P, B), c2 = BikeLoc(P, C);
		BikeTurn(P, B, FQuat::FindBetweenVectors(c2 - b2, (a + U * Dist) - b2));
	}

	/** The rotation taking the frame (X along Fwd0, Y toward Side0) onto (Fwd1, Side1). */
	FQuat BasisTurn(const FVector& Fwd0, const FVector& Side0, const FVector& Fwd1, const FVector& Side1)
	{
		const FQuat Q0 = FRotationMatrix::MakeFromXY(Fwd0, Side0).ToQuat();
		const FQuat Q1 = FRotationMatrix::MakeFromXY(Fwd1, Side1).ToQuat();
		return (Q1 * Q0.Inverse()).GetNormalized();
	}

	float CapsuleDistance(const FVector& P, const FVector& C, const FVector& Axis, float HalfLength, float Radius)
	{
		const float T = FMath::Clamp((float)FVector::DotProduct(P - C, Axis), -HalfLength, HalfLength);
		return (float)(P - (C + Axis * T)).Size() - Radius;
	}

	struct FBikeFinger { TArray<FCompactPoseBoneIndex, TInlineAllocator<3>> Bones; TArray<float, TInlineAllocator<3>> MaxFold; float TipLength = 2.2f; };

	/** A finger's joints and tip after folding Fold (0..1 of each joint's MaxFold) about Axis. */
	void BikeFingerPoints(const FCompactPose& P, const FBikeFinger& F, const FVector& Axis, float Fold, const FVector& FingerDir, TArray<FVector, TInlineAllocator<4>>& Out)
	{
		Out.Reset();
		for (FCompactPoseBoneIndex B : F.Bones)
		{
			Out.Add(BikeLoc(P, B));
		}
		FVector Seg = Out.Num() > 1 ? (Out.Last() - Out[Out.Num() - 2]) : FingerDir;
		Out.Add(Out.Last() + Seg.GetSafeNormal() * F.TipLength);
		for (int32 J = 0; J < F.Bones.Num(); ++J)
		{
			const FQuat Q(Axis, FMath::DegreesToRadians(F.MaxFold[J] * Fold));
			for (int32 K = J + 1; K < Out.Num(); ++K)
			{
				Out[K] = Out[J] + Q.RotateVector(Out[K] - Out[J]);
			}
		}
	}
}

namespace LyudBikeRider
{
	void ApplyBody(FCompactPose& Pose, const FBikeRiderPose& R)
	{
		const float W = FMath::Clamp(R.Weight, 0.f, 1.f);
		if (W < 0.001f)
		{
			return;
		}
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		const FCompactPoseBoneIndex Pel = BikeFind(Bones, TEXT("pelvis"));
		const FCompactPoseBoneIndex Spine[] = { BikeFind(Bones, TEXT("spine_01")), BikeFind(Bones, TEXT("spine_02")), BikeFind(Bones, TEXT("spine_03")) };
		if (!Pel.IsValid() || !Spine[0].IsValid())
		{
			return;
		}
		const FVector Fwd = R.Fwd.GetSafeNormal(), Up = R.Up.GetSafeNormal(), Left = R.Left.GetSafeNormal();

		// the hips: moved onto the saddle, the legs follow below by IK
		const float PW = W * FMath::Clamp(R.PelvisWeight, 0.f, 1.f);
		if (PW > 0.f)
		{
			FTransform P = BikeCS(Pose, Pel);
			P.SetLocation(FMath::Lerp(P.GetLocation(), R.Pelvis, PW));
			BikeSetCS(Pose, Pel, P);
		}

		// the trunk: forward from the hips (a forward lean is a negative turn about the bike's left), the least lean that
		// lets the hands reach their bars with the elbows a little bent
		float Lean = R.TorsoPitch;
		if (R.bAutoLean)
		{
			const FCompactPoseBoneIndex Shoulders[] = { BikeFind(Bones, TEXT("upperarm_l")), BikeFind(Bones, TEXT("upperarm_r")) };
			const FCompactPoseBoneIndex Elbows[] = { BikeFind(Bones, TEXT("lowerarm_l")), BikeFind(Bones, TEXT("lowerarm_r")) };
			const FCompactPoseBoneIndex Wrists[] = { BikeFind(Bones, TEXT("hand_l")), BikeFind(Bones, TEXT("hand_r")) };
			const FVector Pivot = BikeLoc(Pose, Pel);
			float Best = Lean, BestErr = TNumericLimits<float>::Max();
			for (float A = R.TorsoPitch; A <= 75.f; A += 1.5f)
			{
				const FQuat Tip(Left, -FMath::DegreesToRadians(A));
				float Err = 0.f;
				for (int32 S = 0; S < 2; ++S)
				{
					if (!Shoulders[S].IsValid() || !Elbows[S].IsValid() || !Wrists[S].IsValid() || R.Hands[S].Weight < 0.5f)
					{
						continue;
					}
					const FVector Sh = BikeLoc(Pose, Shoulders[S]);
					const float Arm = (float)((BikeLoc(Pose, Elbows[S]) - Sh).Size() + (BikeLoc(Pose, Wrists[S]) - BikeLoc(Pose, Elbows[S])).Size());
					const FBikeRiderPose::FHand& H = R.Hands[S];
					const FVector WristAt = H.Grip - H.PalmNormal.GetSafeNormal() * (H.Radius + PalmSkin) - H.FingerDir.GetSafeNormal() * 5.f;
					const FVector ShNow = Pivot + Tip.RotateVector(Sh - Pivot);
					Err += FMath::Max(0.f, (float)(WristAt - ShNow).Size() - 0.9f * Arm);
				}
				if (Err < BestErr - 0.05f)
				{
					BestErr = Err;
					Best = A;
				}
				if (Err <= 0.f)
				{
					break;
				}
			}
			Lean = Best;
		}
		Lean *= W;
		BikeTurn(Pose, Pel, FQuat(Left, -FMath::DegreesToRadians(0.4f * Lean)));
		const float Share[] = { 0.25f, 0.2f, 0.15f };
		for (int32 I = 0; I < 3; ++I)
		{
			BikeTurn(Pose, Spine[I], FQuat(Left, -FMath::DegreesToRadians(Share[I] * Lean)));
		}
		if (FMath::Abs(R.TorsoRoll) > 0.01f)
		{
			BikeTurn(Pose, Spine[0], FQuat(Fwd, FMath::DegreesToRadians(R.TorsoRoll * W)));      // about forward, + tips the top to the left
		}
		// the neck and head come back up to look ahead over the bars
		const float Back = FMath::Clamp(R.HeadUp, 0.f, 1.f) * Lean;
		BikeTurn(Pose, BikeFind(Bones, TEXT("neck_01")), FQuat(Left, FMath::DegreesToRadians(0.55f * Back)));
		BikeTurn(Pose, BikeFind(Bones, TEXT("head")), FQuat(Left, FMath::DegreesToRadians(0.45f * Back)));

		// the legs: the ball of each foot onto its target, the foot turned to the pedal (toes along Toes, the sole on it)
		const TCHAR* Sides[] = { TEXT("l"), TEXT("r") };
		for (int32 S = 0; S < 2; ++S)
		{
			const FBikeRiderPose::FFoot& F = R.Feet[S];
			const float FW = W * FMath::Clamp(F.Weight, 0.f, 1.f);
			if (FW < 0.001f)
			{
				continue;
			}
			const FCompactPoseBoneIndex Thigh = BikeFind(Bones, FString::Printf(TEXT("thigh_%s"), Sides[S]));
			const FCompactPoseBoneIndex Calf = BikeFind(Bones, FString::Printf(TEXT("calf_%s"), Sides[S]));
			const FCompactPoseBoneIndex Foot = BikeFind(Bones, FString::Printf(TEXT("foot_%s"), Sides[S]));
			const FCompactPoseBoneIndex Ball = BikeFind(Bones, FString::Printf(TEXT("ball_%s"), Sides[S]));
			if (!Thigh.IsValid() || !Calf.IsValid() || !Foot.IsValid())
			{
				continue;
			}
			// the foot's own axes, from the reference pose (toes along the mesh's +Y, its left +X)
			const FTransform RefFoot = BikeRefCS(Pose, Foot);
			const FVector RefBallOff = Ball.IsValid() ? BikeRefCS(Pose, Ball).GetLocation() - RefFoot.GetLocation() : FVector(0.f, 14.f, -6.f);
			const FVector LocalFwd = RefFoot.GetRotation().UnrotateVector(FVector(0.f, RefBallOff.Y, 0.f).GetSafeNormal(UE_SMALL_NUMBER, FVector::YAxisVector));
			const FVector LocalSide = RefFoot.GetRotation().UnrotateVector(FVector::XAxisVector);
			FVector LocalBall = RefFoot.GetRotation().UnrotateVector(RefBallOff);
			if (Ball.IsValid())
			{
				const float Now = (float)(BikeLoc(Pose, Ball) - BikeLoc(Pose, Foot)).Size();
				LocalBall *= Now / FMath::Max(1.f, (float)RefBallOff.Size());
			}
			const FVector Hip = BikeLoc(Pose, Thigh);
			const float LegLen = (float)((BikeLoc(Pose, Calf) - Hip).Size() + (BikeLoc(Pose, Foot) - BikeLoc(Pose, Calf)).Size());
			// a foot out of reach points its toes down (as anyone does at the bottom of the stroke), up to 40 degrees more
			FVector Toes = F.Toes.GetSafeNormal(UE_SMALL_NUMBER, Fwd);
			const FVector Side = (Left - Toes * FVector::DotProduct(Left, Toes)).GetSafeNormal(UE_SMALL_NUMBER, Left);
			FQuat FootRot = FQuat::Identity;
			FVector Ankle = FVector::ZeroVector;
			for (float Down = 0.f; Down <= 40.f; Down += 4.f)
			{
				const FVector T = FQuat(Side, -FMath::DegreesToRadians(Down)).RotateVector(Toes);   // about the left: + would lift the toes
				FootRot = BasisTurn(LocalFwd, LocalSide, T, Side);
				Ankle = F.Ball + Up * BallOverSole - FootRot.RotateVector(LocalBall);
				if ((Ankle - Hip).Size() <= LegLen - 1.f)
				{
					break;
				}
			}
			const FTransform Cur = BikeCS(Pose, Foot);
			BikeTwoBoneIK(Pose, Thigh, Calf, Foot, FMath::Lerp(Cur.GetLocation(), Ankle, FW), F.Pole);
			FTransform NewFoot = BikeCS(Pose, Foot);
			NewFoot.SetRotation(FQuat::Slerp(Cur.GetRotation(), FootRot, FW).GetNormalized());
			BikeSetCS(Pose, Foot, NewFoot);
		}
	}

	void ApplyArms(FCompactPose& Pose, const FBikeRiderPose& R)
	{
		const float W = FMath::Clamp(R.Weight, 0.f, 1.f);
		if (W < 0.001f)
		{
			return;
		}
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		const FCompactPoseBoneIndex Pel = BikeFind(Bones, TEXT("pelvis"));
		const TCHAR* Sides[] = { TEXT("l"), TEXT("r") };
		for (int32 S = 0; S < 2; ++S)
		{
			const FBikeRiderPose::FHand& H = R.Hands[S];
			const float HW = W * FMath::Clamp(H.Weight, 0.f, 1.f);
			if (HW < 0.001f)
			{
				continue;
			}
			auto Bone = [&](const TCHAR* Name) { return BikeFind(Bones, FString::Printf(TEXT("%s_%s"), Name, Sides[S])); };
			const FCompactPoseBoneIndex Upper = Bone(TEXT("upperarm")), Lower = Bone(TEXT("lowerarm")), Wrist = Bone(TEXT("hand"));
			const FCompactPoseBoneIndex Knuckle = Bone(TEXT("middle_01")), ThumbBase = Bone(TEXT("thumb_01")), PinkyBase = Bone(TEXT("pinky_01"));
			if (!Upper.IsValid() || !Lower.IsValid() || !Wrist.IsValid() || !Knuckle.IsValid() || !ThumbBase.IsValid() || !PinkyBase.IsValid())
			{
				continue;
			}
			// the palm frame of the pose now: fingers from the wrist to the middle knuckle, "out of the palm" the side the thumb's tip is on
			auto PalmNow = [&](FVector& OutPalm, FVector& OutFd, FVector& OutPn)
			{
				const FVector Wr = BikeLoc(Pose, Wrist), Kn = BikeLoc(Pose, Knuckle);
				OutFd = (Kn - Wr).GetSafeNormal(UE_SMALL_NUMBER, FVector::YAxisVector);
				FVector Across = BikeLoc(Pose, ThumbBase) - BikeLoc(Pose, PinkyBase);
				Across = (Across - OutFd * FVector::DotProduct(Across, OutFd)).GetSafeNormal();
				OutPn = FVector::CrossProduct(Across, OutFd);
				const FCompactPoseBoneIndex ThumbTip = Bone(TEXT("thumb_03"));
				const FVector Tip = ThumbTip.IsValid() ? BikeLoc(Pose, ThumbTip) : BikeLoc(Pose, ThumbBase);
				OutPalm = Wr + OutFd * (0.6f * (float)(Kn - Wr).Size());
				if (FVector::DotProduct(Tip - OutPalm, OutPn) < 0.f)
				{
					OutPn = -OutPn;
				}
			};
			FVector Palm0, Fd0, Pn0;
			PalmNow(Palm0, Fd0, Pn0);
			// wanted: the palm's skin on the bar, the fingers across it
			FVector Fd = H.FingerDir.GetSafeNormal();
			FVector Pn = (H.PalmNormal - Fd * FVector::DotProduct(H.PalmNormal, Fd)).GetSafeNormal(UE_SMALL_NUMBER, -R.Up);
			const FVector PalmAt = H.Grip - Pn * (H.Radius + PalmSkin);
			const float PalmReach = 0.6f * (float)(BikeLoc(Pose, Knuckle) - BikeLoc(Pose, Wrist)).Size();
			const FVector WristAt = FMath::Lerp(Palm0 - Fd0 * PalmReach, PalmAt - Fd * PalmReach, HW);
			const FVector Shoulder = BikeLoc(Pose, Upper);
			FVector Outward = Shoulder - (Pel.IsValid() ? BikeLoc(Pose, Pel) : Shoulder);
			Outward -= R.Up * FVector::DotProduct(Outward, R.Up);
			const FVector Pole = Outward.GetSafeNormal() * 0.9f - R.Up * 0.5f - R.Fwd * 0.2f;
			BikeTwoBoneIK(Pose, Upper, Lower, Wrist, WristAt, Pole);

			// the hand turned onto the bar: fingers' way first, then the roll that brings the palm round
			FVector Palm1, Fd1, Pn1;
			PalmNow(Palm1, Fd1, Pn1);
			const FVector WantFd = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(Fd1, Fd), HW).RotateVector(Fd1).GetSafeNormal();
			const FQuat Q1 = FQuat::FindBetweenNormals(Fd1, WantFd);
			FVector PnTurned = Q1.RotateVector(Pn1);
			PnTurned = (PnTurned - WantFd * FVector::DotProduct(PnTurned, WantFd)).GetSafeNormal();
			FVector WantPn = (Pn - WantFd * FVector::DotProduct(Pn, WantFd)).GetSafeNormal();
			const float Roll = HW * FMath::Atan2((float)FVector::DotProduct(FVector::CrossProduct(PnTurned, WantPn), WantFd), (float)FVector::DotProduct(PnTurned, WantPn));
			BikeTurn(Pose, Wrist, FQuat(WantFd, Roll) * Q1);

			// the fingers close round the bar until their skin meets it (thumb round the other way)
			FVector Palm2, Fd2, Pn2;
			PalmNow(Palm2, Fd2, Pn2);
			const FVector Axis = H.Axis.GetSafeNormal(UE_SMALL_NUMBER, R.Left);
			const FVector Curl = FVector::CrossProduct(Fd2, Pn2).GetSafeNormal();
			auto Fold = [&](const FBikeFinger& F, const FVector& Axis0, const FVector& Toward)
			{
				if (F.Bones.Num() == 0)
				{
					return;
				}
				TArray<FVector, TInlineAllocator<4>> A, B;
				BikeFingerPoints(Pose, F, Axis0, 0.f, Fd2, A);
				BikeFingerPoints(Pose, F, Axis0, 0.5f, Fd2, B);
				const FVector Ax = (B.Last() - Toward).SizeSquared() < (A.Last() - Toward).SizeSquared() ? Axis0 : -Axis0;
				float Amount = 0.85f;
				for (float T = 0.f; T <= 1.f; T += 0.04f)
				{
					TArray<FVector, TInlineAllocator<4>> Pts;
					BikeFingerPoints(Pose, F, Ax, T, Fd2, Pts);
					bool bTouch = false;
					for (int32 K = 1; K < Pts.Num() && !bTouch; ++K)
					{
						for (const float U : { 0.33f, 0.66f, 1.f })
						{
							bTouch |= CapsuleDistance(FMath::Lerp(Pts[K - 1], Pts[K], U), H.Grip, Axis, 7.f, H.Radius) <= FingerSkin;
						}
					}
					if (bTouch)
					{
						Amount = T;
						break;
					}
				}
				Amount *= HW * FMath::Clamp(H.Close, 0.f, 1.f);
				for (int32 J = 0; J < F.Bones.Num(); ++J)
				{
					BikeTurn(Pose, F.Bones[J], FQuat(Ax, FMath::DegreesToRadians(F.MaxFold[J] * Amount)));
				}
			};
			const FVector TowardPalm = Palm2 + Pn2 * 6.f;
			for (const TCHAR* Name : { TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") })
			{
				FBikeFinger F;
				for (int32 J = 1; J <= 3; ++J)
				{
					const FCompactPoseBoneIndex B = BikeFind(Bones, FString::Printf(TEXT("%s_0%d_%s"), Name, J, Sides[S]));
					if (!B.IsValid())
					{
						break;
					}
					F.Bones.Add(B);
				}
				// a one-bone finger (the hero's ring and little finger) folds as much as three joints would
				if (F.Bones.Num() == 1)
				{
					F.MaxFold = { 150.f };
					F.TipLength = 4.6f;
				}
				else
				{
					F.MaxFold = { 75.f, 90.f, 60.f };
					F.MaxFold.SetNum(F.Bones.Num());
				}
				Fold(F, Curl, TowardPalm);
			}
			FBikeFinger Th;
			for (int32 J = 1; J <= 3; ++J)
			{
				const FCompactPoseBoneIndex B = Bone(*FString::Printf(TEXT("thumb_0%d"), J));
				if (B.IsValid())
				{
					Th.Bones.Add(B);
				}
			}
			Th.MaxFold = { 45.f, 40.f, 35.f };
			Th.MaxFold.SetNum(Th.Bones.Num());
			Th.TipLength = 2.4f;
			Fold(Th, Pn2, Palm2 + Pn2 * 3.f - Fd2 * 2.f);
		}
	}
}
