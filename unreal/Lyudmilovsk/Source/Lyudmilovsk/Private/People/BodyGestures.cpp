#include "BodyGestures.h"

#include "TwoBoneIK.h"

using LyudPeople::EManner;
using LyudPeople::EProp;

namespace
{
	/** A goal for an arm written for the right hand, "out" being away from the body's middle: (out, forward, up). */
	struct FV
	{
		float Out, Fwd, Up;
	};

	FVector Side(int32 S, const FV& V)
	{
		// the mesh's left is +X: the left arm's "out" is +X, the right arm's -X
		return FVector(S == 0 ? V.Out : -V.Out, V.Fwd, V.Up);
	}

	void Hand(FLyudGesturePose& G, int32 S, float Weight, uint8 Anchor, const FV& At, const FV& Fingers, const FV& Palm, float Curl)
	{
		FLyudArmGoal& A = G.Arm[S];
		A.Weight = Weight;
		A.Anchor = Anchor;
		A.Offset = Side(S, At);
		A.Fingers = Side(S, Fingers).GetSafeNormal();
		A.Palm = Side(S, Palm).GetSafeNormal();
		A.Curl = Curl;
	}

	FV Mix(const FV& A, const FV& B, float T)
	{
		return { FMath::Lerp(A.Out, B.Out, T), FMath::Lerp(A.Fwd, B.Fwd, T), FMath::Lerp(A.Up, B.Up, T) };
	}

	/** 0 .. 1 .. 0 over a gesture that begins and ends: in over In seconds, out over the last Out. */
	float Envelope(float T, float Left, float In, float Out)
	{
		const float A = FMath::SmoothStep(0.f, In, T);
		return Left < 0.f ? A : A * FMath::SmoothStep(0.f, Out, Left);
	}

	/** A hand's trip somewhere and back within a cycle: 0 at rest, 1 there (from At for Stay seconds, Move to get there). */
	float Trip(float T, float Cycle, float At, float Stay, float Move)
	{
		const float P = FMath::Fmod(T, Cycle);
		return FMath::SmoothStep(At - Move, At, P) * (1.f - FMath::SmoothStep(At + Stay, At + Stay + Move, P));
	}

	constexpr uint8 Pelvis = 0, Chest = 1, Head = 2;

	// --- the worker's helpers: component space worked out from the local pose, bone by bone ---

	FTransform CS(const FCompactPose& Pose, FCompactPoseBoneIndex I)
	{
		FTransform T = Pose[I];
		for (FCompactPoseBoneIndex P = Pose.GetParentBoneIndex(I); P.IsValid(); P = Pose.GetParentBoneIndex(P))
		{
			T = T * Pose[P];
		}
		return T;
	}

	FTransform ParentCS(const FCompactPose& Pose, FCompactPoseBoneIndex I)
	{
		const FCompactPoseBoneIndex P = Pose.GetParentBoneIndex(I);
		return P.IsValid() ? CS(Pose, P) : FTransform::Identity;
	}

	/** Turns a bone (and so its children) by a component-space rotation about its own joint. */
	void TurnCS(FCompactPose& Pose, FCompactPoseBoneIndex I, const FQuat& Q)
	{
		const FTransform Parent = ParentCS(Pose, I);
		const FQuat Now = (Pose[I] * Parent).GetRotation();
		Pose[I].SetRotation((Parent.GetRotation().Inverse() * (Q * Now)).GetNormalized());
	}

	void TurnDeg(FCompactPose& Pose, int32 Bone, const FVector& Axis, float Deg)
	{
		if (Bone != INDEX_NONE && FMath::Abs(Deg) > 0.01f)
		{
			TurnCS(Pose, FCompactPoseBoneIndex(Bone), FQuat(Axis, FMath::DegreesToRadians(Deg)));
		}
	}

	void SetCS(FCompactPose& Pose, FCompactPoseBoneIndex I, const FTransform& Want)
	{
		Pose[I] = Want.GetRelativeTransform(ParentCS(Pose, I));
		Pose[I].NormalizeRotation();
	}

	int32 Find(const FBoneContainer& Bones, const TCHAR* Name, const TCHAR* Suffix = nullptr)
	{
		const FName N(Suffix ? *FString::Printf(TEXT("%s_%s"), Name, Suffix) : Name);
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(N);
		return MeshIndex == INDEX_NONE ? INDEX_NONE : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex)).GetInt();
	}

	void FindBones(const FBoneContainer& Bones, FLyudGestureBones& B)
	{
		B.For = &Bones;
		B.Serial = Bones.GetSerialNumber();
		B.Pelvis = Find(Bones, TEXT("pelvis"));
		B.Spine[0] = Find(Bones, TEXT("spine_01"));
		B.Spine[1] = Find(Bones, TEXT("spine_02"));
		B.Spine[2] = Find(Bones, TEXT("spine_03"));
		B.Neck = Find(Bones, TEXT("neck_01"));
		B.Head = Find(Bones, TEXT("head"));
		static const TCHAR* Sides[2] = { TEXT("l"), TEXT("r") };
		static const TCHAR* Fingers[4] = { TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") };
		for (int32 S = 0; S < 2; S++)
		{
			B.Arm[S][0] = Find(Bones, TEXT("upperarm"), Sides[S]);
			B.Arm[S][1] = Find(Bones, TEXT("lowerarm"), Sides[S]);
			B.Arm[S][2] = Find(Bones, TEXT("hand"), Sides[S]);
			B.Knuckle[S] = Find(Bones, TEXT("middle_01"), Sides[S]);
			B.ThumbBase[S] = Find(Bones, TEXT("thumb_01"), Sides[S]);
			B.PinkyBase[S] = Find(Bones, TEXT("pinky_01"), Sides[S]);
			B.ThumbTip[S] = Find(Bones, TEXT("thumb_03"), Sides[S]);
			for (int32 F = 0; F < 4; F++)
			{
				for (int32 J = 0; J < 3; J++)
				{
					B.Finger[S][F * 3 + J] = Find(Bones, *FString::Printf(TEXT("%s_0%d"), Fingers[F], J + 1), Sides[S]);
				}
			}
			for (int32 J = 0; J < 3; J++)
			{
				B.Thumb[S][J] = Find(Bones, *FString::Printf(TEXT("thumb_0%d"), J + 1), Sides[S]);
			}
		}
	}

	/** The palm's frame of the pose now (as Bikes/LyudBikeRider's): the fingers' way from the wrist to the middle knuckle,
	 *  and "out of the palm", the side the thumb's tip is on. False without the hand's bones. */
	bool PalmFrame(const FCompactPose& Pose, const FLyudGestureBones& B, int32 S, FVector& Fd, FVector& Pn)
	{
		if (B.Arm[S][2] == INDEX_NONE || B.Knuckle[S] == INDEX_NONE || B.ThumbBase[S] == INDEX_NONE || B.PinkyBase[S] == INDEX_NONE)
		{
			return false;
		}
		const FVector Wr = CS(Pose, FCompactPoseBoneIndex(B.Arm[S][2])).GetLocation();
		const FVector Kn = CS(Pose, FCompactPoseBoneIndex(B.Knuckle[S])).GetLocation();
		Fd = (Kn - Wr).GetSafeNormal(UE_SMALL_NUMBER, FVector::YAxisVector);
		FVector Across = CS(Pose, FCompactPoseBoneIndex(B.ThumbBase[S])).GetLocation() - CS(Pose, FCompactPoseBoneIndex(B.PinkyBase[S])).GetLocation();
		Across = (Across - Fd * FVector::DotProduct(Across, Fd)).GetSafeNormal();
		Pn = FVector::CrossProduct(Across, Fd);
		const int32 TipBone = B.ThumbTip[S] != INDEX_NONE ? B.ThumbTip[S] : B.ThumbBase[S];
		const FVector Tip = CS(Pose, FCompactPoseBoneIndex(TipBone)).GetLocation();
		if (FVector::DotProduct(Tip - (Wr + (Kn - Wr) * 0.6f), Pn) < 0.f)
		{
			Pn = -Pn;
		}
		return true;
	}
}

namespace LyudGesture
{
	FName ClipFor(EManner Manner)
	{
		static const FName FoldArms(TEXT("FoldArms"));
		return Manner == EManner::FoldArms ? FoldArms : NAME_None;
	}

	EHold HoldOf(EProp Prop)
	{
		switch (Prop)
		{
		case EProp::Phone: return EHold::Palm;
		case EProp::Cigarette:
		case EProp::Brush: return EHold::Fingers;
		case EProp::Camera:
		case EProp::Binoculars:
		case EProp::Newspaper:
		case EProp::Book:
		case EProp::Knitting: return EHold::TwoHands;
		case EProp::Rod: return EHold::Pointing;
		case EProp::Bag: return EHold::Hanging;
		default: return EHold::Upright;
		}
	}

	void Eval(EManner Manner, float T, float Left, const FBody& Body, FLyudGesturePose& G)
	{
		G = FLyudGesturePose();
		G.Scale = Body.Scale;
		// the main hand and the other one
		const int32 M = Body.bLeftHanded ? 0 : 1, O = 1 - M;
		// which way "toward the main hand's side" is for the head (yaw and roll are toward the left)
		const float MainSide = Body.bLeftHanded ? 1.f : -1.f;
		const float Ph = (float)(Body.Seed % 628u) * 0.01f;
		const float E = Envelope(T, Left, 0.5f, 0.5f);
		const float Sit = Body.bSeated ? 1.f : 0.f;
		switch (Manner)
		{
		case EManner::HandsHips:
			for (int32 S = 0; S < 2; S++)
			{
				Hand(G, S, E, Pelvis, { 17.f, 1.f, 5.f }, { 0.1f, 0.5f, -0.85f }, { -1.f, 0.f, 0.f }, 0.25f);
			}
			break;
		case EManner::HandsBack:
			// one hand holds the other wrist behind the back
			Hand(G, M, E, Pelvis, { 2.f, -15.f, -1.f }, { -0.8f, -0.1f, -0.6f }, { 0.f, -1.f, 0.f }, 0.5f);
			Hand(G, O, E, Pelvis, { 5.f, -14.f, 1.f }, { -0.8f, -0.1f, -0.6f }, { 0.f, -1.f, 0.f }, 0.3f);
			G.SpinePitch = (Body.bWalking ? 3.f : 1.f) * E;
			break;
		case EManner::Stretch:
		{
			const float W = Envelope(T, Left, 1.1f, 0.9f);
			for (int32 S = 0; S < 2; S++)
			{
				Hand(G, S, W, Head, { 17.f, -3.f, 36.f }, { 0.25f, -0.1f, 1.f }, { 0.f, 1.f, 0.f }, 0.1f);
			}
			G.SpinePitch = -7.f * W;
			G.HeadPitch = 12.f * W;
			break;
		}
		case EManner::Yawn:
		{
			const float W = Envelope(T, Left, 0.8f, 0.7f);
			Hand(G, M, W, Head, { 1.f, 11.5f, 3.f }, { -0.7f, 0.f, 0.7f }, { 0.f, -1.f, 0.f }, 0.25f);
			G.HeadPitch = 9.f * W;
			G.Jaw = 0.85f * FMath::SmoothStep(0.5f, 1.3f, T) * (Left < 0.f ? 1.f : FMath::SmoothStep(0.2f, 0.9f, Left));
			G.bEyesShut = G.Jaw > 0.5f;
			break;
		}
		case EManner::ScratchHead:
		{
			const float Rub = FMath::Sin(T * 21.f) * 1.3f;
			Hand(G, M, E, Head, { 7.f, -3.f + Rub, 19.f }, { -0.5f, 0.1f, -0.2f }, { -0.6f, 0.f, -0.8f }, 0.45f);
			G.HeadPitch = -6.f * E;
			G.HeadRoll = -MainSide * 5.f * E;
			break;
		}
		case EManner::CheckWatch:
			// the watch is on the other wrist: the forearm across the chest, a look down at it
			Hand(G, O, E, Chest, { -3.f, 23.f, -9.f + 4.f * Sit }, { -1.f, 0.15f, 0.f }, { 0.f, 0.2f, -1.f }, 0.5f);
			G.HeadPitch = -24.f * E;
			G.HeadYaw = MainSide * 8.f * E;
			break;
		case EManner::LookSky:
			G.HeadPitch = (24.f + 3.f * FMath::Sin(T * 0.5f + Ph)) * E;
			G.HeadYaw = 14.f * FMath::Sin(T * 0.23f + Ph) * E;
			G.SpinePitch = -4.f * E;
			break;
		case EManner::ShadeEyes:
			Hand(G, M, E, Head, { 3.f, 11.f, 13.f }, { -1.f, 0.15f, 0.f }, { 0.f, 0.f, -1.f }, 0.1f);
			G.HeadPitch = 3.f * E;
			break;
		case EManner::RubHands:
		{
			const float R = FMath::Sin(T * 15.f) * 2.2f;
			Hand(G, M, E, Chest, { 1.f, 21.f + R, -7.f }, { -0.25f, 0.9f, 0.35f }, { -1.f, 0.f, 0.f }, 0.15f);
			Hand(G, O, E, Chest, { 1.f, 21.f - R, -7.f }, { -0.25f, 0.9f, 0.35f }, { -1.f, 0.f, 0.f }, 0.15f);
			G.SpinePitch = 5.f * E;
			G.HeadPitch = -8.f * E;
			break;
		}
		case EManner::HugSelf:
			// each hand on the other arm, shoulders up, a little hunched
			Hand(G, M, E, Chest, { -13.f, 13.f, -6.f - 3.f * Sit }, { -0.9f, 0.2f, 0.35f }, { 0.2f, -1.f, 0.f }, 0.45f);
			Hand(G, O, E, Chest, { -11.f, 15.f, -11.f - 3.f * Sit }, { -0.9f, 0.2f, 0.3f }, { 0.2f, -1.f, 0.f }, 0.45f);
			G.SpinePitch = 6.f * E;
			G.HeadPitch = -7.f * E;
			break;
		case EManner::Wave:
		{
			const float W = Envelope(T, Left, 0.35f, 0.45f);
			Hand(G, M, W, Head, { 24.f + 6.f * FMath::Sin(T * 13.f), 6.f, 17.f }, { 0.15f, 0.f, 1.f }, { 0.f, 1.f, 0.f }, 0.f);
			break;
		}
		case EManner::Nod:
			G.HeadPitch = -11.f * FMath::Max(0.f, FMath::Sin(T * 7.f)) * Envelope(T, Left, 0.15f, 0.3f);
			break;
		case EManner::Point:
		{
			const float W = Envelope(T, Left, 0.35f, 0.5f);
			Hand(G, M, W, Chest, { 13.f, 46.f, 3.f }, { 0.15f, 1.f, 0.05f }, { 0.f, 0.f, -1.f }, 0.1f);
			break;
		}
		case EManner::Clap:
		{
			const float C = 3.5f + 3.5f * FMath::Sin(T * 21.f + Ph);
			Hand(G, M, E, Chest, { C, 25.f, 0.f }, { -0.2f, 0.75f, 0.6f }, { -1.f, 0.f, 0.f }, 0.05f);
			Hand(G, O, E, Chest, { C - 2.f, 25.f, 1.f }, { -0.2f, 0.75f, 0.6f }, { -1.f, 0.f, 0.f }, 0.05f);
			break;
		}
		case EManner::Sway:
			G.SpineRoll = 4.5f * FMath::Sin(T * 3.1f + Ph) * E;
			G.HeadRoll = 4.f * FMath::Sin(T * 3.1f + Ph + 0.6f) * E;
			G.HeadPitch = -4.f * FMath::Max(0.f, FMath::Sin(T * 6.2f + Ph)) * E;
			break;
		case EManner::Doze:
		{
			// the head sinks, jerks up a little now and then, sinks again
			const float W = Envelope(T, Left, 3.f, 0.8f);
			const float Jerk = FMath::Max(0.f, FMath::Sin(T * 0.37f + Ph) - 0.93f) * 14.f;
			G.HeadPitch = (-27.f + 22.f * Jerk + 1.5f * FMath::Sin(T * 1.4f)) * W;
			G.HeadRoll = 6.f * W;
			G.SpinePitch = 6.f * W;
			G.bEyesShut = W > 0.5f && Jerk < 0.3f;
			break;
		}
		case EManner::CoverHead:
			Hand(G, M, E, Head, { 3.f, 6.f, 24.f }, { -1.f, 0.2f, 0.f }, { 0.f, 0.f, -1.f }, 0.15f);
			G.SpinePitch = 9.f * E;
			G.HeadPitch = -8.f * E;
			break;
		case EManner::PhoneCall:
			Hand(G, M, E, Head, { 9.5f, 2.f, 6.f }, { 0.f, -0.35f, 0.95f }, { -1.f, 0.f, 0.f }, 0.55f);
			G.HeadRoll = -MainSide * 4.f * E;
			break;
		case EManner::PhoneScroll:
			Hand(G, M, E, Chest, { 6.f, 27.f, -8.f + 6.f * Sit }, { -0.25f, 0.6f, 0.75f }, { -0.15f, -0.75f, 0.65f }, 0.5f);
			G.HeadPitch = -22.f * E;
			break;
		case EManner::Smoke:
		{
			// the hand rests low, comes up for a drag every nine seconds or so
			const float Up = Trip(T + Ph, 9.f, 2.2f, 1.3f, 1.1f);
			const FV Rest = Body.bSeated ? FV{ 20.f, 26.f, 16.f } : FV{ 25.f, 12.f, 2.f };
			// (the mouth is ~3 cm over the head bone's joint, ~12 cm ahead of it)
			if (Up > 0.5f)
			{
				Hand(G, M, E, Head, { 3.f, 12.5f, 3.f }, { -0.6f, 0.1f, 0.8f }, { 0.f, -1.f, 0.f }, 0.5f);
			}
			else
			{
				Hand(G, M, E * (Body.bSeated ? 1.f : 0.85f), Pelvis, Rest, { 0.2f, 0.8f, 0.5f }, { -0.6f, 0.f, 0.8f }, 0.5f);
			}
			break;
		}
		case EManner::Photo:
			for (int32 S = 0; S < 2; S++)
			{
				Hand(G, S, E, Head, { 7.5f, 15.f, 9.f }, { -0.1f, 0.3f, 0.95f }, { -1.f, 0.f, 0.f }, 0.5f);
			}
			break;
		case EManner::FeedBirds:
		{
			// a pinch from the lap, a toss ahead
			const float Toss = Trip(T + Ph, 4.5f, 1.6f, 0.25f, 0.45f);
			const FV Lap = Body.bSeated ? FV{ 9.f, 20.f, 16.f } : FV{ 12.f, 18.f, 8.f };
			Hand(G, M, E, Pelvis, Mix(Lap, { 16.f, 50.f, 22.f }, Toss), { 0.1f, 1.f, 0.f }, Mix({ 0.f, 0.f, 1.f }, { 0.f, 0.3f, -1.f }, Toss), 0.6f - 0.5f * Toss);
			G.HeadPitch = -10.f * E;
			break;
		}
		case EManner::Umbrella:
			Hand(G, M, E, Chest, { 13.f, 20.f, -6.f + 4.f * Sit }, { -0.3f, 0.95f, 0.f }, { -0.95f, -0.3f, 0.f }, 0.9f);
			break;
		case EManner::Fish:
			Hand(G, M, E, Pelvis, { 9.f, 24.f, 12.f }, { -0.5f, 0.6f, -0.2f }, { -0.3f, 0.2f, 0.9f }, 0.9f);
			Hand(G, O, E, Pelvis, { 4.f, 42.f, 20.f + 1.5f * FMath::Sin(T * 0.7f + Ph) }, { -0.6f, 0.5f, -0.2f }, { 0.2f, -0.2f, 0.95f }, 0.9f);
			break;
		case EManner::Paint:
		{
			// strokes on a canvas an arm's length ahead; a step back to look now and then
			const float Look = Trip(T + Ph, 17.f, 12.f, 2.5f, 1.f);
			Hand(G, M, E * (1.f - 0.6f * Look), Chest, { 10.f + 7.f * FMath::Sin(T * 1.7f + Ph), 46.f, 2.f + 6.f * FMath::Sin(T * 1.1f) }, { 0.f, 0.9f, 0.4f },
				{ -0.7f, 0.f, -0.7f }, 0.7f);
			G.HeadRoll = 7.f * Look;
			break;
		}
		case EManner::Binoculars:
			for (int32 S = 0; S < 2; S++)
			{
				Hand(G, S, E, Head, { 6.5f, 12.f, 10.f }, { -0.2f, 0.2f, 0.95f }, { -1.f, 0.f, 0.f }, 0.6f);
			}
			G.HeadPitch = 6.f * E;
			G.HeadYaw = 18.f * FMath::Sin(T * 0.21f + Ph) * E;
			break;
		case EManner::Newspaper:
			for (int32 S = 0; S < 2; S++)
			{
				Hand(G, S, E, Chest, { 19.f, 31.f, -10.f }, { 0.f, 0.45f, 0.9f }, { -1.f, 0.f, 0.f }, 0.7f);
			}
			G.HeadPitch = -15.f * E;
			G.HeadYaw = 9.f * FMath::Sin(T * 0.35f + Ph) * E;
			break;
		case EManner::Knit:
		{
			const float K = FMath::Sin(T * 7.f + Ph);
			Hand(G, M, E, Chest, { 5.f, 24.f + 1.2f * K, -22.f + 1.f * K }, { -0.6f, 0.75f, 0.2f }, { -0.5f, -0.3f, -0.8f }, 0.75f);
			Hand(G, O, E, Chest, { 5.f, 24.f - 1.2f * K, -22.f - 1.f * K }, { -0.6f, 0.75f, 0.2f }, { -0.5f, -0.3f, -0.8f }, 0.75f);
			G.HeadPitch = -22.f * E;
			break;
		}
		case EManner::IceCream:
		case EManner::Drink:
		{
			const bool bCone = Manner == EManner::IceCream;
			const float Up = Trip(T + Ph, bCone ? 6.f : 11.f, 2.f, bCone ? 1.4f : 1.1f, 0.9f);
			if (Up > 0.5f)
			{
				Hand(G, M, E, Head, { 3.f, 14.f, bCone ? -2.f : 0.f }, { -0.5f, 0.85f, 0.f }, { -0.85f, -0.5f, 0.f }, 0.85f);
			}
			else
			{
				Hand(G, M, E, Chest, { 12.f, 24.f, -8.f + 5.f * Sit }, { -0.3f, 0.95f, 0.f }, { -0.95f, -0.3f, 0.f }, 0.85f);
			}
			G.HeadPitch = (bCone ? -8.f : 6.f) * Up;
			break;
		}
		case EManner::CarryBag:
			// the arm with the bag hangs still and heavy instead of swinging with the walk
			Hand(G, M, 0.85f * E, Pelvis, { 25.f, 2.f, -24.f }, { 0.f, 0.1f, -1.f }, { -1.f, 0.f, 0.f }, 0.85f);
			G.SpineRoll = -MainSide * 2.f * E;
			break;
		default:
			break;
		}
	}

	void Apply(FCompactPose& Pose, const FLyudGesturePose& G, FLyudGestureBones& B)
	{
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		if (B.For != &Bones || B.Serial != Bones.GetSerialNumber() || B.Serial == 0)
		{
			FindBones(Bones, B);
		}
		// the back first (the arms hang off it), then the head
		const FVector X = FVector::XAxisVector, Y = FVector::YAxisVector, Z = FVector::UpVector;
		for (int32 I = 0; I < 3; I++)
		{
			TurnDeg(Pose, B.Spine[I], X, -G.SpinePitch / 3.f);
			TurnDeg(Pose, B.Spine[I], Y, -G.SpineRoll / 3.f);
		}
		for (const TPair<int32, float>& N : { TPair<int32, float>(B.Neck, 0.4f), TPair<int32, float>(B.Head, 0.6f) })
		{
			TurnDeg(Pose, N.Key, Z, G.HeadYaw * N.Value);
			TurnDeg(Pose, N.Key, X, G.HeadPitch * N.Value);
			TurnDeg(Pose, N.Key, Y, -G.HeadRoll * N.Value);
		}
		const int32 Anchors[3] = { B.Pelvis, B.Spine[2], B.Head };
		for (int32 S = 0; S < 2; S++)
		{
			const FLyudArmGoal& A = G.Arm[S];
			const int32 AnchorBone = Anchors[FMath::Min<int32>(A.Anchor, 2)];
			if (A.Weight < 0.003f || B.Arm[S][0] == INDEX_NONE || B.Arm[S][1] == INDEX_NONE || B.Arm[S][2] == INDEX_NONE || AnchorBone == INDEX_NONE)
			{
				continue;
			}
			const float W = FMath::Clamp(A.Weight, 0.f, 1.f);
			const FCompactPoseBoneIndex Up(B.Arm[S][0]), Lo(B.Arm[S][1]), Ha(B.Arm[S][2]);
			FTransform U = CS(Pose, Up), L = CS(Pose, Lo), H = CS(Pose, Ha);
			const FVector Shoulder = U.GetLocation();
			const float Len = (float)((L.GetLocation() - Shoulder).Size() + (H.GetLocation() - L.GetLocation()).Size());
			// the wrist sits a palm short of where the hand is wanted, back along the fingers
			FVector Goal = CS(Pose, FCompactPoseBoneIndex(AnchorBone)).GetLocation() + A.Offset * G.Scale - A.Fingers * (4.f * G.Scale);
			// never at full stretch, never inside the chest
			Goal = Shoulder + (Goal - Shoulder).GetClampedToSize(0.25f * Len, 0.96f * Len);
			const FVector Want = FMath::Lerp(H.GetLocation(), Goal, W);
			// the elbow out to the side and down, a little back
			const FVector Out = S == 0 ? X : -X;
			const FVector Pole = Shoulder + (Out * 0.75f - Z * 0.9f - Y * 0.35f) * 50.f;
			AnimationCore::SolveTwoBoneIK(U, L, H, Pole, Want, false, 1.0, 1.0);
			SetCS(Pose, Up, U);
			SetCS(Pose, Lo, L);
			SetCS(Pose, Ha, H);
			// the hand turned: the fingers' way first, then the roll that brings the palm round (as the bike's grip)
			FVector Fd, Pn;
			if (!PalmFrame(Pose, B, S, Fd, Pn))
			{
				continue;
			}
			const FVector WantFd = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(Fd, A.Fingers), W).RotateVector(Fd).GetSafeNormal();
			const FQuat Q1 = FQuat::FindBetweenNormals(Fd, WantFd);
			FVector PnTurned = Q1.RotateVector(Pn);
			PnTurned = (PnTurned - WantFd * FVector::DotProduct(PnTurned, WantFd)).GetSafeNormal();
			const FVector WantPn = (A.Palm - WantFd * FVector::DotProduct(A.Palm, WantFd)).GetSafeNormal(UE_SMALL_NUMBER, PnTurned);
			const float Roll = W * FMath::Atan2((float)FVector::DotProduct(FVector::CrossProduct(PnTurned, WantPn), WantFd), (float)FVector::DotProduct(PnTurned, WantPn));
			TurnCS(Pose, Ha, FQuat(WantFd, Roll) * Q1);
			// the fingers close toward the palm, knuckle by knuckle; the thumb comes across
			if (A.Curl * W > 0.02f && PalmFrame(Pose, B, S, Fd, Pn))
			{
				const FVector Axis = FVector::CrossProduct(Fd, Pn).GetSafeNormal();
				static const float Bend[3] = { 50.f, 60.f, 35.f };
				for (int32 F = 0; F < 12; F++)
				{
					TurnDeg(Pose, B.Finger[S][F], Axis, Bend[F % 3] * A.Curl * W);
				}
				for (int32 J = 0; J < 3; J++)
				{
					TurnDeg(Pose, B.Thumb[S][J], Fd, (S == 0 ? -1.f : 1.f) * 14.f * A.Curl * W);
				}
			}
		}
	}
}
