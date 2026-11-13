#include "People/LyudBodyAnim.h"
#include "BodyGestures.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/SkeletalMeshActor.h"
#include "AnimationRuntime.h"
#include "TwoBoneIK.h"
#include "GameFramework/Pawn.h"
#include "BonePose.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"
#include "Kismet/GameplayStatics.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	void Advance(FLyudBodyClip& C, float Dt)
	{
		if (!C.Seq)
		{
			return;
		}
		const float Len = C.Seq->GetPlayLength();
		C.Time += Dt * C.Rate;
		if (Len <= 0.f)
		{
			C.Time = 0.f;
		}
		else if (C.bLoop)
		{
			C.Time = FMath::Fmod(C.Time, Len);
			if (C.Time < 0.f)
			{
				C.Time += Len;
			}
		}
		else
		{
			C.Time = FMath::Clamp(C.Time, 0.f, Len);
		}
	}

	void Sample(const FLyudBodyClip& C, FPoseContext& Out)
	{
		FAnimationPoseData Data(Out);
		C.Seq->GetAnimationPose(Data, FAnimExtractContext((double)C.Time, false, {}, C.bLoop));
	}

	/** The lip-sync curves of every voice line of a language (tools/lipsync.py), loaded once. */
	struct FLipSync
	{
		TArray<uint8> Jaw, Funnel, Smile;
	};

	const TMap<FString, FLipSync>& LipSyncFor(const FString& Lang)
	{
		static TMap<FString, TMap<FString, FLipSync>> Cache;
		if (const TMap<FString, FLipSync>* Found = Cache.Find(Lang))
		{
			return *Found;
		}
		TMap<FString, FLipSync>& Out = Cache.Add(Lang);
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / FString::Printf(TEXT("Dialogue/lipsync_%s.json"), *Lang))))
		{
			return Out;
		}
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			return Out;
		}
		const TSharedPtr<FJsonObject>* Lines;
		if (!Root->TryGetObjectField(TEXT("lines"), Lines))
		{
			return Out;
		}
		auto Decode = [](const FString& S, TArray<uint8>& To)
		{
			To.SetNumUninitialized(S.Len());
			for (int32 I = 0; I < S.Len(); I++)
			{
				To[I] = (uint8)FMath::Clamp(S[I] - TEXT('0'), 0, 9);
			}
		};
		for (const auto& Pair : (*Lines)->Values)
		{
			const TSharedPtr<FJsonObject> L = Pair.Value->AsObject();
			if (L.IsValid())
			{
				FLipSync& S = Out.FindOrAdd(FString(Pair.Key));
				Decode(L->GetStringField(TEXT("jaw")), S.Jaw);
				Decode(L->GetStringField(TEXT("funnel")), S.Funnel);
				Decode(L->GetStringField(TEXT("smile")), S.Smile);
			}
		}
		return Out;
	}

	float CurveAt(const TArray<uint8>& C, float Frame)
	{
		if (C.Num() == 0)
		{
			return 0.f;
		}
		const int32 I = FMath::Clamp((int32)Frame, 0, C.Num() - 1);
		const int32 J = FMath::Min(I + 1, C.Num() - 1);
		const float T = FMath::Clamp(Frame - (float)I, 0.f, 1.f);
		return FMath::Lerp((float)C[I], (float)C[J], T) / 9.f;
	}

	const FName BlinkL(TEXT("eyeBlinkLeft")), BlinkR(TEXT("eyeBlinkRight")), JawOpen(TEXT("jawOpen")),
		MouthFunnel(TEXT("mouthFunnel")), MouthSmile(TEXT("mouthSmile")), BrowUp(TEXT("browUp"));
	const FName NamePcGamepad(TEXT("PC_Gamepad")), NamePcType(TEXT("PC_Type")), NamePcGame(TEXT("PC_Game")), NamePcScroll(TEXT("PC_Scroll"));

	// FLyudBodyAnimProxy::BoneIdx's order: the legs and the arms (left, right; upper, lower, end), then the trunk and head
	enum EBodyBone
	{
		BB_Pelvis, BB_ThighL, BB_CalfL, BB_FootL, BB_ThighR, BB_CalfR, BB_FootR,
		BB_UpperarmL, BB_LowerarmL, BB_HandL, BB_UpperarmR, BB_LowerarmR, BB_HandR,
		BB_Spine1, BB_Spine2, BB_Spine3, BB_Head, BB_ClavicleL, BB_ClavicleR, BB_Neck, BB_Num
	};
	static_assert(BB_Num == FLyudBodyAnimProxy::NumBones, "FLyudBodyAnimProxy::NumBones");

	const FName& BodyBoneName(int32 I)
	{
		static const FName Names[BB_Num] = { TEXT("pelvis"), TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("thigh_r"),
			TEXT("calf_r"), TEXT("foot_r"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"), TEXT("upperarm_r"),
			TEXT("lowerarm_r"), TEXT("hand_r"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("head"),
			TEXT("clavicle_l"), TEXT("clavicle_r"), TEXT("neck_01") };
		return Names[I];
	}
}

static TAutoConsoleVariable<int32> CVarFaceLog(TEXT("lyud.Face.Log"), 0, TEXT("1: log each body's face values once a second."));
static TAutoConsoleVariable<int32> CVarEyeFix(TEXT("lyud.Face.EyeFix"), 0, TEXT("1: every eyeball looks at lyud.Face.EyeYaw/EyePitch (tests)."));
static TAutoConsoleVariable<float> CVarEyeYaw(TEXT("lyud.Face.EyeYaw"), 0.f, TEXT("Degrees toward the person's left (lyud.Face.EyeFix)."));
static TAutoConsoleVariable<float> CVarEyePitch(TEXT("lyud.Face.EyePitch"), 0.f, TEXT("Degrees up (lyud.Face.EyeFix)."));
static TAutoConsoleVariable<int32> CVarEyesShut(TEXT("lyud.Face.Shut"), 0, TEXT("1: every face keeps its eyes shut (tests)."));

const TArray<FName>& FaceCurveNames()
{
	static const TArray<FName> Names = { BlinkL, BlinkR, JawOpen, MouthFunnel, MouthSmile, BrowUp };
	return Names;
}

// --- the worker side ------------------------------------------------------------------------------------

void FLyudBodyAnimProxy::Update(float DeltaSeconds)
{
	Advance(Cur, DeltaSeconds);
	if (Alpha < 1.f)
	{
		Advance(Prev, DeltaSeconds);
		Alpha = BlendTime > 0.f ? FMath::Min(1.f, Alpha + DeltaSeconds / BlendTime) : 1.f;
	}
}

bool FLyudBodyAnimProxy::Evaluate(FPoseContext& Output)
{
	if (!Cur.Seq)
	{
		Output.ResetToRefPose();
		return true;
	}
	Sample(Cur, Output);
	if (Alpha < 1.f && Prev.Seq)
	{
		FPoseContext From(Output);
		Sample(Prev, From);
		const float W = FMath::SmoothStep(0.f, 1.f, Alpha);
		FAnimationPoseData OutData(Output);
		FAnimationRuntime::BlendTwoPosesTogetherInPlace(OutData, FAnimationPoseData(From), W);
	}
	// the face's morph targets as curves (SK_Human_Skeleton marks them morph target curves, import_character.py)
	if (bFaceCurves)
	{
		for (int32 I = 0; I < (int32)UE_ARRAY_COUNT(FaceCurves); I++)
		{
			Output.Curve.Set(FaceCurveNames()[I], FaceCurves[I]);
		}
	}
	Procedural(Output);
	// a manner's gesture: the arms to where it wants the hands, the head and the back (BodyGestures.cpp)
	if (Gesture.IsActive())
	{
		LyudGesture::Apply(Output.Pose, Gesture, GestureBones);
	}
	// on a bicycle: the hips on the saddle, the feet on the pedals, the hands on the bars (Bikes/LyudBikeRider)
	if (Bike.IsActive())
	{
		LyudBikeRider::ApplyBody(Output.Pose, Bike);
		LyudBikeRider::ApplyArms(Output.Pose, Bike);
	}
	return true;
}

void FLyudBodyAnimProxy::AddReferencedObjects(UAnimInstance* InAnimInstance, FReferenceCollector& Collector)
{
	FAnimInstanceProxy::AddReferencedObjects(InAnimInstance, Collector);
	Collector.AddReferencedObject(Cur.Seq, InAnimInstance);
	Collector.AddReferencedObject(Prev.Seq, InAnimInstance);
}

void FLyudBodyAnimProxy::Procedural(FPoseContext& Output)
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	if (BonesFor != &Bones || BonesSerial != Bones.GetSerialNumber() || BonesSerial == 0)
	{
		BonesFor = &Bones;
		BonesSerial = Bones.GetSerialNumber();
		for (int32 I = 0; I < BB_Num; I++)
		{
			const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(BodyBoneName(I));
			BoneIdx[I] = MeshIndex == INDEX_NONE ? INDEX_NONE : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex)).GetInt();
		}
	}
	auto Bone = [this](int32 Which) { return FCompactPoseBoneIndex(BoneIdx[Which]); };
	FCSPose<FCompactPose> CS;
	CS.InitPose(Output.Pose);
	// turns a bone (and so its children) about a component-space axis through its joint; the mesh faces +Y, its left
	// is +X, up +Z
	auto Turn = [&CS](FCompactPoseBoneIndex I, const FVector& Axis, float Deg)
	{
		if (I.IsValid() && FMath::Abs(Deg) > 1e-3f)
		{
			FTransform T = CS.GetComponentSpaceTransform(I);
			T.SetRotation(FQuat(Axis, FMath::DegreesToRadians(Deg)) * T.GetRotation());
			CS.SetComponentSpaceTransform(I, T);
		}
	};
	// the feet first (they move the pelvis), then the upper body on top
	const FCompactPoseBoneIndex Pelvis = Bone(BB_Pelvis);
	if (FootIKAlpha > 0.01f && Pelvis.IsValid())
	{
		FCompactPoseBoneIndex Leg[2][3];
		bool bOk = true;
		for (int32 S = 0; S < 2; S++)
		{
			for (int32 J = 0; J < 3; J++)
			{
				Leg[S][J] = Bone(BB_ThighL + S * 3 + J);
				bOk &= Leg[S][J].IsValid();
			}
		}
		if (bOk)
		{
			// how planted each foot is in the clip (a lifted foot in mid-stride still follows the ground, half as much)
			float Plant[2];
			for (int32 S = 0; S < 2; S++)
			{
				const float Z = CS.GetComponentSpaceTransform(Leg[S][2]).GetLocation().Z;
				Plant[S] = FMath::Clamp(1.f - (Z - 10.f) / 15.f, 0.5f, 1.f);
			}
			// the pelvis goes down to the lower foot (a leg can't stretch), never up
			const float Down = FMath::Min3(FootOffset[0], FootOffset[1], 0.f) * FootIKAlpha;
			FTransform P = CS.GetComponentSpaceTransform(Pelvis);
			P.AddToTranslation(FVector(0.f, 0.f, Down));
			CS.SetComponentSpaceTransform(Pelvis, P);
			for (int32 S = 0; S < 2; S++)
			{
				FTransform Hip = CS.GetComponentSpaceTransform(Leg[S][0]);
				FTransform Knee = CS.GetComponentSpaceTransform(Leg[S][1]);
				FTransform Ankle = CS.GetComponentSpaceTransform(Leg[S][2]);
				const FVector Want = Ankle.GetLocation() + FVector(0.f, 0.f, (FootOffset[S] * Plant[S] - Down) * FootIKAlpha);
				// the knee bends the way it already bends
				const FVector Mid = (Hip.GetLocation() + Ankle.GetLocation()) * 0.5f;
				FVector Bend = Knee.GetLocation() - Mid;
				if (Bend.SizeSquared() < 1.f)
				{
					Bend = FVector(0.f, 10.f, 0.f);
				}
				const FVector Pole = Knee.GetLocation() + Bend.GetSafeNormal() * 60.f;
				const FQuat FootRot = Ankle.GetRotation();
				AnimationCore::SolveTwoBoneIK(Hip, Knee, Ankle, Pole, Want, false, 1.0, 1.0);
				// the foot keeps its clip's turn, tilted onto the slope as far as it is planted
				const FQuat Slope = FQuat::FindBetweenNormals(FVector::UpVector, FootNormal[S]);
				Ankle.SetRotation(FQuat::Slerp(FQuat::Identity, Slope, Plant[S] * FootIKAlpha) * FootRot);
				CS.SetComponentSpaceTransform(Leg[S][0], Hip);
				CS.SetComponentSpaceTransform(Leg[S][1], Knee);
				CS.SetComponentSpaceTransform(Leg[S][2], Ankle);
			}
		}
	}
	// the hands onto the desk under them (club desks stand at their own heights, their chairs at their own distances)
	FCompactPoseBoneIndex Arm[2][3];
	for (int32 S = 0; S < 2; S++)
	{
		for (int32 J = 0; J < 3; J++)
		{
			Arm[S][J] = Bone(BB_UpperarmL + S * 3 + J);
		}
		// where the clip puts the hand, for the game side's desk traces (read after this evaluation)
		ClipHand[S] = Arm[S][2].IsValid() ? CS.GetComponentSpaceTransform(Arm[S][2]).GetLocation() : FVector::ZeroVector;
	}
	if (HandIKAlpha > 0.01f)
	{
		// a desk further than the clip's (its keyboard 47 cm ahead of the seat): the body leans in toward it instead of the
		// arms reaching out straight; the head keeps most of its look at the screen
		const float Ahead = FMath::Max(HandOffset[0].Y, HandOffset[1].Y) * HandIKAlpha;
		const float DeskLean = FMath::Clamp((Ahead - 6.f) * 0.55f, 0.f, 14.f);
		Turn(Bone(BB_Spine1), FVector::XAxisVector, -DeskLean * 0.4f);
		Turn(Bone(BB_Spine2), FVector::XAxisVector, -DeskLean * 0.35f);
		Turn(Bone(BB_Spine3), FVector::XAxisVector, -DeskLean * 0.25f);
		Turn(Bone(BB_Head), FVector::XAxisVector, DeskLean * 0.6f);
		for (int32 S = 0; S < 2; S++)
		{
			const FCompactPoseBoneIndex A = Arm[S][0], Bi = Arm[S][1], C = Arm[S][2];
			if (!A.IsValid() || !Bi.IsValid() || !C.IsValid() || HandOffset[S].SizeSquared() < 0.04f)
			{
				continue;
			}
			FTransform Up = CS.GetComponentSpaceTransform(A), Lo = CS.GetComponentSpaceTransform(Bi), Ha = CS.GetComponentSpaceTransform(C);
			// the hand goes where the desk is from the clip's hand, the lean having brought the shoulder closer; never
			// further from the shoulder than an arm bent ~115 deg at the elbow (or the clip's own reach): an arm on a
			// desk is never locked straight
			FVector Want = FMath::Lerp(Ha.GetLocation(), ClipHand[S] + HandOffset[S], HandIKAlpha);
			const float Len = (Lo.GetLocation() - Up.GetLocation()).Size() + (Ha.GetLocation() - Lo.GetLocation()).Size();
			const float MaxReach = FMath::Max(0.84f * Len, (float)(Ha.GetLocation() - Up.GetLocation()).Size());
			Want = Up.GetLocation() + (Want - Up.GetLocation()).GetClampedToMaxSize(MaxReach);
			const FVector Mid = (Up.GetLocation() + Ha.GetLocation()) * 0.5f;
			const FVector Pole = Lo.GetLocation() + (Lo.GetLocation() - Mid).GetSafeNormal() * 40.f;
			const FQuat HandRot = Ha.GetRotation();
			AnimationCore::SolveTwoBoneIK(Up, Lo, Ha, Pole, Want, false, 1.0, 1.0);
			Ha.SetRotation(HandRot);
			CS.SetComponentSpaceTransform(A, Up);
			CS.SetComponentSpaceTransform(Bi, Lo);
			CS.SetComponentSpaceTransform(C, Ha);
		}
	}
	// breathing: the chest lifts and opens, the shoulders rise a little, the head keeps still
	const float B = (Breath - 0.5f) * BreathDepth;
	Turn(Bone(BB_Spine2), FVector::XAxisVector, -0.9f * B);
	Turn(Bone(BB_Spine3), FVector::XAxisVector, -1.3f * B);
	Turn(Bone(BB_ClavicleL), FVector::YAxisVector, -1.6f * B);
	Turn(Bone(BB_ClavicleR), FVector::YAxisVector, 1.6f * B);
	Turn(Bone(BB_Neck), FVector::XAxisVector, 1.6f * B);
	// leaning with the motion: forward into a start or uphill, back when stopping, into a turn
	Turn(Bone(BB_Spine1), FVector::XAxisVector, -LeanPitch * 0.6f);
	Turn(Bone(BB_Spine1), FVector::YAxisVector, LeanRoll * 0.6f);
	Turn(Bone(BB_Spine3), FVector::XAxisVector, -LeanPitch * 0.4f);
	// the head's turn toward what the person looks at, a third in the neck and the rest in the head
	if (FMath::Abs(HeadYaw) + FMath::Abs(HeadPitch) > 0.05f)
	{
		for (const TPair<int32, float>& N : { TPair<int32, float>(BB_Neck, 0.35f), TPair<int32, float>(BB_Head, 0.65f) })
		{
			const FCompactPoseBoneIndex I = Bone(N.Key);
			if (I.IsValid())
			{
				FTransform T = CS.GetComponentSpaceTransform(I);
				const FQuat Q = FQuat(FVector::UpVector, FMath::DegreesToRadians(HeadYaw * N.Value))
					* FQuat(FVector::XAxisVector, FMath::DegreesToRadians(HeadPitch * N.Value));
				T.SetRotation(Q * T.GetRotation());
				CS.SetComponentSpaceTransform(I, T);
			}
		}
	}
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose);
}

// --- the game side ---------------------------------------------------------------------------------------

void ULyudBodyAnimInstance::PlayClip(UAnimSequence* Seq, bool bLoop, float Rate, float BlendTime, float StartTime)
{
	FLyudBodyAnimProxy& P = GetProxyOnGameThread<FLyudBodyAnimProxy>();
	if (P.Cur.Seq == Seq)
	{
		P.Cur.Rate = Rate;
		P.Cur.bLoop = bLoop;
		return;
	}
	if (P.Cur.Seq && BlendTime > 0.f)
	{
		// blending from a blend: keep whichever weighs more as the old pose
		P.Prev = P.Alpha >= 0.5f ? P.Cur : P.Prev;
		P.Alpha = 0.f;
	}
	else
	{
		P.Alpha = 1.f;
	}
	P.BlendTime = BlendTime;
	P.Cur = FLyudBodyClip{ Seq, StartTime, Rate, bLoop };
}

void ULyudBodyAnimInstance::SetClipRate(float Rate)
{
	GetProxyOnGameThread<FLyudBodyAnimProxy>().Cur.Rate = Rate;
}

UAnimSequence* ULyudBodyAnimInstance::CurrentClip() const
{
	return GetProxyOnGameThread<FLyudBodyAnimProxy>().Cur.Seq;
}

float ULyudBodyAnimInstance::CurrentClipPhase() const
{
	const FLyudBodyClip& C = GetProxyOnGameThread<FLyudBodyAnimProxy>().Cur;
	const float L = C.Seq ? C.Seq->GetPlayLength() : 0.f;
	return L > 0.f ? FMath::Frac(C.Time / L) : 0.f;
}

void ULyudBodyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	Rng.Initialize((int32)GetTypeHash(GetOuter()->GetName()) ^ (int32)GetUniqueID());
	BlinkAt = Rng.FRandRange(0.5f, 4.f);
	// a generated character with face_rig.py's morph targets
	bFace = Mesh && Mesh->GetSkeletalMeshAsset() && Mesh->GetSkeletalMeshAsset()->FindMorphTarget(BlinkL) != nullptr;
	// a placed person (ULyudFaceSubsystem) keeps the clip it was given in the level; the instance can be made again
	// when the component re-initialises, so it is read here rather than handed over once
	if (Mesh && Mesh->AnimationData.AnimToPlay && !CurrentClip())
	{
		if (UAnimSequence* Clip = Cast<UAnimSequence>(Mesh->AnimationData.AnimToPlay))
		{
			// a person at a club computer (their own seated loop) works through the computer loops instead
			if (Mesh->AnimationData.bSavedPlaying && (Clip->GetName().Contains(TEXT("_Sit_")) || Clip->GetName().Contains(TEXT("Excel"))))
			{
				SetComputer(StyleOfClip(Clip->GetName()));
				return;
			}
			const float At = Mesh->AnimationData.SavedPosition > 0.f ? Mesh->AnimationData.SavedPosition
				: Rng.FRandRange(0.f, Clip->GetPlayLength());      // people in a row shouldn't move in step
			PlayClip(Clip, Mesh->AnimationData.bSavedLooping, Mesh->AnimationData.bSavedPlaying ? Mesh->AnimationData.SavedPlayRate : 0.f, 0.f, At);
		}
	}
}

void ULyudBodyAnimInstance::SetLookCandidate(const FVector& Where, bool bValid)
{
	Candidate = Where;
	bCandidate = bValid;
	bCandidateGiven = true;
}

bool ULyudBodyAnimInstance::Speak(const FString& LineKey)
{
	const FLipSync* S = LipSyncFor(LyudText::Language()).Find(LineKey);
	if (!S)
	{
		S = LipSyncFor(TEXT("ru")).Find(LineKey);
	}
	if (!S)
	{
		return false;
	}
	Jaw = S->Jaw;
	Funnel = S->Funnel;
	Smile = S->Smile;
	SpeakStart = Clock;
	SpeakStartWorld = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	// the speaker looks at whom they talk to
	AttendUntil = Clock + 4.f;
	bAttending = bCandidate;
	return true;
}

void ULyudBodyAnimInstance::StopSpeaking()
{
	SpeakStart = -1.f;
}

bool ULyudBodyAnimInstance::SpeakLine(USkeletalMeshComponent* Mesh, const FString& LineKey)
{
	ULyudBodyAnimInstance* A = Mesh ? Cast<ULyudBodyAnimInstance>(Mesh->GetAnimInstance()) : nullptr;
	return A && A->Speak(LineKey);
}

void ULyudBodyAnimInstance::StopLine(USkeletalMeshComponent* Mesh)
{
	if (ULyudBodyAnimInstance* A = Mesh ? Cast<ULyudBodyAnimInstance>(Mesh->GetAnimInstance()) : nullptr)
	{
		A->StopSpeaking();
	}
}

void ULyudBodyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	GetProxyOnGameThread<FLyudBodyAnimProxy>().Bike = BikePose;
	USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (!Mesh || DeltaSeconds <= 0.f)
	{
		return;
	}
	Clock += DeltaSeconds;
	if (!bCandidateGiven)
	{
		// nobody said what to look at: the player, when near (6 m)
		const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
		Candidate = Cam ? Cam->GetCameraLocation() : FVector::ZeroVector;
		bCandidate = Cam && FVector::DistSquared(Candidate, Mesh->GetComponentLocation()) < 600.f * 600.f;
	}
	bCandidateGiven = false;
	UpdateComputer(Mesh);
	UpdateGamepad(Mesh);
	UpdateLook(Mesh, DeltaSeconds);
	UpdateBody(Mesh, DeltaSeconds);
	UpdateGesture(DeltaSeconds);
	if (bFace)
	{
		UpdateBlink(Mesh);
		UpdateMouth(Mesh, DeltaSeconds);
		FLyudBodyAnimProxy& P = GetProxyOnGameThread<FLyudBodyAnimProxy>();
		static_assert(sizeof(P.FaceCurves) == sizeof(Morph), "FaceCurves");
		FMemory::Memcpy(P.FaceCurves, Morph, sizeof(Morph));
		P.bFaceCurves = true;
	}
}

FName ULyudBodyAnimInstance::StyleOfClip(const FString& Clip)
{
	if (Clip.Contains(TEXT("Gamepad")))
	{
		return TEXT("Gamepad");
	}
	if (Clip.Contains(TEXT("Browse")))
	{
		return TEXT("Browse");
	}
	if (Clip.Contains(TEXT("ChinRest")))
	{
		return TEXT("Reader");
	}
	if (Clip.Contains(TEXT("Excel")))
	{
		return TEXT("Office");
	}
	if (Clip.Contains(TEXT("Sit_Idle")) || Clip.Contains(TEXT("LeanBack")))
	{
		return TEXT("Idle");
	}
	return TEXT("Gamer");                                    // KeyboardMouse, Stream
}

void ULyudBodyAnimInstance::SetComputer(FName Style)
{
	if (Style != PcStyle)
	{
		PcStyle = Style;
		PcUntil = 0.f;
		PcClip = NAME_None;
	}
}

void ULyudBodyAnimInstance::UpdateComputer(USkeletalMeshComponent* Mesh)
{
	if (PcStyle.IsNone())
	{
		PcClip = NAME_None;
		return;
	}
	if (Clock < PcUntil && !PcClip.IsNone())
	{
		return;
	}
	// each style's mix (weights) of the loops; the stretch is a one-off
	struct FPick { const TCHAR* Clip; float Weight; };
	static const TMap<FName, TArray<FPick>> Mix = {
		{ TEXT("Gamer"), { { TEXT("PC_Game"), 5.f }, { TEXT("PC_Type"), 1.5f }, { TEXT("PC_Scroll"), 1.5f }, { TEXT("PC_LeanBack"), 1.f }, { TEXT("PC_Stretch"), 0.5f }, { TEXT("PC_Read"), 0.5f } } },
		{ TEXT("Browse"), { { TEXT("PC_Scroll"), 4.f }, { TEXT("PC_Read"), 2.5f }, { TEXT("PC_Type"), 1.5f }, { TEXT("PC_LeanBack"), 1.5f }, { TEXT("PC_Stretch"), 0.7f } } },
		{ TEXT("Reader"), { { TEXT("PC_Read"), 4.f }, { TEXT("PC_Scroll"), 2.f }, { TEXT("PC_LeanBack"), 1.f }, { TEXT("PC_Stretch"), 0.5f } } },
		{ TEXT("Office"), { { TEXT("PC_Type"), 4.f }, { TEXT("PC_Scroll"), 2.f }, { TEXT("PC_Read"), 1.5f }, { TEXT("PC_Stretch"), 0.7f }, { TEXT("PC_LeanBack"), 0.5f } } },
		{ TEXT("Idle"), { { TEXT("PC_LeanBack"), 3.f }, { TEXT("PC_Scroll"), 2.5f }, { TEXT("PC_Read"), 1.5f }, { TEXT("PC_Type"), 1.f }, { TEXT("PC_Stretch"), 0.5f } } },
		{ TEXT("Gamepad"), { { TEXT("PC_Gamepad"), 8.f }, { TEXT("PC_LeanBack"), 0.8f }, { TEXT("PC_Stretch"), 0.4f } } } };
	const TArray<FPick>* Picks = Mix.Find(PcStyle);
	if (!Picks)
	{
		Picks = Mix.Find(TEXT("Gamer"));
	}
	float Sum = 0.f;
	for (const FPick& P : *Picks)
	{
		Sum += P.Clip == PcClip.ToString() ? 0.f : P.Weight;           // something else next (unless it's all there is)
	}
	float R = Rng.FRand() * Sum;
	FName Pick = PcClip;
	for (const FPick& P : *Picks)
	{
		if (P.Clip == PcClip.ToString())
		{
			continue;
		}
		R -= P.Weight;
		Pick = P.Clip;
		if (R <= 0.f)
		{
			break;
		}
	}
	if (PcStyle == TEXT("Gamepad") && PcClip != TEXT("PC_Gamepad") && !PcClip.IsNone())
	{
		Pick = TEXT("PC_Gamepad");                           // back to the game after a break
	}
	TObjectPtr<UAnimSequence>& Seq = PcClips.FindOrAdd(Pick);
	if (!Seq)
	{
		const FString Path = FString::Printf(TEXT("/Game/Characters/Humans/Anims/A_Human_%s.A_Human_%s"), *Pick.ToString(), *Pick.ToString());
		Seq = LoadObject<UAnimSequence>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!Seq)
	{
		PcUntil = Clock + 5.f;
		return;
	}
	PcClip = Pick;
	const bool bOnce = Pick == TEXT("PC_Stretch");
	PlayClip(Seq, !bOnce, Rng.FRandRange(0.9f, 1.1f), 0.6f, bOnce ? 0.f : Rng.FRandRange(0.f, Seq->GetPlayLength()));
	PcUntil = Clock + (bOnce ? Seq->GetPlayLength() - 0.3f : Rng.FRandRange(6.f, 18.f) * (Pick == TEXT("PC_Gamepad") || Pick == TEXT("PC_Game") ? 1.6f : 1.f));
}

void ULyudBodyAnimInstance::UpdateGamepad(USkeletalMeshComponent* Mesh)
{
	// the gamepad loop itself playing (not still blending in from another): PcClips holds the clip loaded for PC_Gamepad
	const TObjectPtr<UAnimSequence>* Pad = PcClip == NamePcGamepad ? PcClips.Find(NamePcGamepad) : nullptr;
	const bool bShow = Pad && *Pad && CurrentClip() == Pad->Get();
	if (bShow && !Gamepad && Mesh->GetOwner())
	{
		if (UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Characters/Humans/Props/SM_Gamepad.SM_Gamepad")))
		{
			Gamepad = NewObject<UStaticMeshComponent>(Mesh->GetOwner(), TEXT("Gamepad"));
			Gamepad->SetStaticMesh(SM);
			Gamepad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			// moved every frame in the hands: out of the distance fields and Lumen (perf checklist)
			Gamepad->bAffectDistanceFieldLighting = false;
			Gamepad->bAffectDynamicIndirectLighting = false;
			Gamepad->SetupAttachment(Mesh);
			Gamepad->RegisterComponent();
		}
	}
	if (!Gamepad)
	{
		return;
	}
	Gamepad->SetVisibility(bShow);
	if (bShow)
	{
		// between the palms, its length from the right hand to the left, the sticks up toward the face
		const FVector L = Mesh->GetSocketLocation(TEXT("hand_l")), R = Mesh->GetSocketLocation(TEXT("hand_r"));
		const FVector Ml = Mesh->GetSocketLocation(TEXT("middle_01_l")), Mr = Mesh->GetSocketLocation(TEXT("middle_01_r"));
		const FVector Mid = (L + R + Ml + Mr) * 0.25f;
		const FVector Across = (L - R).GetSafeNormal();
		const FVector Fwd = ((Ml + Mr) * 0.5f - (L + R) * 0.5f).GetSafeNormal();
		const FVector Up = FVector::CrossProduct(Across, Fwd).GetSafeNormal();
		// the mesh: length along X, far edge +Y (glTF -Z -> UE +Y), top +Z
		const FMatrix M = FRotationMatrix::MakeFromXZ(Across, Up.Z >= 0.f ? Up : -Up);
		Gamepad->SetWorldLocationAndRotation(Mid + (Up.Z >= 0.f ? Up : -Up) * 2.f, M.ToQuat());
	}
}

void ULyudBodyAnimInstance::UpdateBody(USkeletalMeshComponent* Mesh, float Dt)
{
	FLyudBodyAnimProxy& P = GetProxyOnGameThread<FLyudBodyAnimProxy>();
	const FTransform C = Mesh->GetComponentTransform();
	const FVector Pos = C.GetLocation();
	const float Yaw = (float)C.Rotator().Yaw;
	// motion: smoothed velocity and turn rate (cm/s, deg/s)
	if (bHasLast && Dt > 1e-4f)
	{
		const FVector V = (Pos - LastPos) / Dt;
		const FVector Acc = (V - Vel) / FMath::Max(Dt, 0.016f);
		Vel += (V - Vel) * FMath::Min(1.f, Dt * 6.f);
		YawRate += (FMath::FindDeltaAngleDegrees(LastYaw, Yaw) / Dt - YawRate) * FMath::Min(1.f, Dt * 5.f);
		const float Speed = Vel.Size2D();
		// teleports (a person placed, a snap into a seat) aren't motion
		if (Speed > 800.f)
		{
			Vel = FVector::ZeroVector;
		}
		// breathless after walking (a little) and running (a lot)
		Exertion = FMath::Clamp(Exertion + Dt * (Speed > 250.f ? 0.06f : Speed > 60.f ? 0.006f : -0.03f), 0.f, 1.f);
		if (bOnFeet)
		{
			const FVector Fwd = C.GetRotation().RotateVector(FVector::YAxisVector);     // the mesh faces +Y
			const float FwdAcc = FMath::Clamp((float)FVector::DotProduct(Acc, Fwd), -300.f, 300.f);
			const FVector2D Want(FMath::Clamp(FwdAcc * 0.012f + Speed * 0.006f, -4.f, 6.f),
				FMath::Clamp(-YawRate * Speed * 0.0009f, -7.f, 7.f));
			Lean += (Want - Lean) * FMath::Min(1.f, Dt * 4.f);
		}
		else
		{
			Lean *= FMath::Max(0.f, 1.f - Dt * 4.f);
		}
	}
	LastPos = Pos;
	LastYaw = Yaw;
	bHasLast = true;
	// breathing: ~4.2 s at rest down to ~2 s out of breath, a little irregular
	BreathPeriod = FMath::Lerp(4.2f + 0.6f * FMath::Sin(Clock * 0.13f + Rng.GetInitialSeed()), 2.0f, Exertion);
	BreathPhase = FMath::Fmod(BreathPhase + Dt / BreathPeriod, 1.f);
	// in quicker than out
	const float Ph = BreathPhase < 0.4f ? BreathPhase / 0.4f * 0.5f : 0.5f + (BreathPhase - 0.4f) / 0.6f * 0.5f;
	P.Breath = 0.5f - 0.5f * FMath::Cos(Ph * 2.f * PI);
	P.BreathDepth = 1.f + 1.6f * Exertion;
	P.LeanPitch = Lean.X;
	P.LeanRoll = Lean.Y;
	// the feet on the ground: a trace under each foot (where it was last frame)
	static const IConsoleVariable* FeetIK = IConsoleManager::Get().RegisterConsoleVariable(TEXT("lyud.Body.FeetIK"), 1,
		TEXT("0: no body plants its feet on the ground by traces (checks: is a pose's foot lifted by what is under it?)"));
	FootAlpha += ((bOnFeet && FeetIK->GetInt() != 0 ? 1.f : 0.f) - FootAlpha) * FMath::Min(1.f, Dt * 5.f);
	FootTraceAge += Dt;
	if (FootAlpha > 0.01f && GetWorld())
	{
		// near the camera (25 m) the feet are traced every frame; further off a few times a second (the smoothing below
		// carries them between), where a foot's few cm can't be told
		const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
		const bool bNear = !Cam || FVector::DistSquared(Cam->GetCameraLocation(), Pos) < 2500.f * 2500.f;
		const bool bTrace = bNear || FootTraceAge >= 0.2f;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(LyudFeet), false, Mesh->GetOwner());
		if (bTrace)
		{
			FootTraceAge = 0.f;
			if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
			{
				Q.AddIgnoredActor(Player);
			}
		}
		static const FName Feet[2] = { TEXT("foot_l"), TEXT("foot_r") };
		for (int32 S = 0; S < 2; S++)
		{
			if (bTrace)
			{
				const FVector F = Mesh->GetSocketLocation(Feet[S]);
				FHitResult Hit;
				float Off = 0.f;
				FVector N = FVector::UpVector;
				// only the ground itself: from a step's height (not from the knee, where a counter's plinth, a shelf or a
				// till stand) down, and only what stands still (not another person, a cat, a bag on the floor); a hit that
				// isn't something to stand on (a wall, a riser, a counter's side) leaves the foot where the clip has it
				if (GetWorld()->LineTraceSingleByObjectType(Hit, FVector(F.X, F.Y, Pos.Z + 30.f), FVector(F.X, F.Y, Pos.Z - 60.f), FCollisionObjectQueryParams(ECC_WorldStatic), Q)
					&& !Hit.bStartPenetrating)
				{
					const FVector Nw = C.InverseTransformVectorNoScale(Hit.ImpactNormal).GetSafeNormal();
					if (Nw.Z >= 0.7f)
					{
						Off = FMath::Clamp((float)(Hit.ImpactPoint.Z - Pos.Z), -40.f, 25.f);
						N = Nw;
					}
				}
				FootHitOff[S] = Off;
				FootHitNor[S] = N;
			}
			const float Off = FootHitOff[S];
			const FVector N = FootHitNor[S];
			FootOff[S] += (Off - FootOff[S]) * FMath::Min(1.f, Dt * 12.f);
			FootNor[S] = FMath::Lerp(FootNor[S], N, FMath::Min(1.f, Dt * 8.f)).GetSafeNormal();
			P.FootOffset[S] = FootOff[S] / FMath::Max((float)C.GetScale3D().Z, 0.1f);     // in the (scaled) mesh's own cm
			P.FootNormal[S] = FootNor[S];
		}
	}
	P.FootIKAlpha = FootAlpha;
	// at a computer with the hands on the desk: the surface under each hand
	const bool bDeskHands = PcClip == NamePcType || PcClip == NamePcGame || PcClip == NamePcScroll;
	HandAlpha += ((bDeskHands ? 1.f : 0.f) - HandAlpha) * FMath::Min(1.f, Dt * 3.f);
	if (HandAlpha > 0.01f && GetWorld())
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(LyudHands), false, Mesh->GetOwner());
		const FVector Fwd = C.GetRotation().RotateVector(FVector::YAxisVector);            // the mesh faces +Y
		// a body sitting still at its desk: the desk found last time stays where it was (traced again after a while, or
		// when the body or the hand moves on)
		const bool bBodyStill = C.GetLocation().Equals(DeskFrom.GetLocation(), 0.5f) && C.GetRotation().Equals(DeskFrom.GetRotation(), 1e-4f)
			&& C.GetScale3D().Equals(DeskFrom.GetScale3D(), 1e-3f);
		DeskFrom = C;
		for (int32 S = 0; S < 2; S++)
		{
			// where the clip puts the hand (before the IK, the last evaluation), then the desk: right under it, or a little
			// further forward (the chair stands back), its top within a hand's height of the clip's
			const FVector Clip = P.ClipHand[S];
			const FVector H = C.TransformPosition(Clip);
			FVector Off = FVector::ZeroVector;
			DeskAge[S] += Dt;
			if (!Clip.IsNearlyZero() && (!bBodyStill || DeskAge[S] > 0.25f || FVector::DistSquared(H, DeskH[S]) > 2.f * 2.f))
			{
				DeskAge[S] = 0.f;
				DeskH[S] = H;
				bDeskHit[S] = false;
				for (float D = 0.f; D <= 30.f; D += 5.f)
				{
					const FVector At = H + Fwd * D;
					FHitResult Hit;
					if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 12.f), At - FVector(0.f, 0.f, 20.f), ECC_Visibility, Q)
						&& Hit.ImpactNormal.Z > 0.8f)
					{
						bDeskHit[S] = true;
						DeskD[S] = D;
						DeskZ[S] = (float)Hit.ImpactPoint.Z;
						break;
					}
				}
			}
			if (!Clip.IsNearlyZero() && bDeskHit[S])
			{
				// the desk's top under (or ahead of) the clip's hand now: the flat desk keeps its height where it was found
				const float D = DeskD[S];
				const FVector At = H + Fwd * D;
				const FVector World(At.X + Fwd.X * (D > 0.f ? 4.f : 0.f), At.Y + Fwd.Y * (D > 0.f ? 4.f : 0.f), DeskZ[S] + 3.f);
				// in the mesh's own (scaled) space: a child's body is a scaled grown-up's
				Off = C.InverseTransformPosition(World) - Clip;
				Off.Z = FMath::Clamp((float)Off.Z, -12.f, 10.f);
			}
			HandOff[S] += (Off - HandOff[S]) * FMath::Min(1.f, Dt * 4.f);
			P.HandOffset[S] = HandOff[S];
		}
	}
	P.HandIKAlpha = HandAlpha;
	if (CVarFaceLog.GetValueOnAnyThread() && FMath::FloorToInt(Clock) != FMath::FloorToInt(Clock - Dt))
	{
		UE_LOG(LogTemp, Warning, TEXT("LyudBody %s: hands %.2f (%.1f, %.1f cm), feet %.2f (%.1f, %.1f cm), lean (%.1f, %.1f), breath %.2f x%.2f, clip %s"),
			*GetNameSafe(Mesh->GetSkeletalMeshAsset()), HandAlpha, HandOff[0].Size(), HandOff[1].Size(), FootAlpha, FootOff[0], FootOff[1], Lean.X, Lean.Y, P.Breath, P.BreathDepth,
			*GetNameSafe(CurrentClip()));
	}
}

void ULyudBodyAnimInstance::UpdateGesture(float Dt)
{
	// eased toward what is wanted: weights and angles always; a hand's goal itself only while the hand is on its way
	// there or back (a goal that jumps under a hand already at it would throw the arm across)
	const float K = FMath::Min(1.f, Dt * 5.f);
	for (int32 S = 0; S < 2; S++)
	{
		FLyudArmGoal& N = GestureNow.Arm[S];
		const FLyudArmGoal& W = GestureWant.Arm[S];
		if (N.Weight < 0.02f)
		{
			const float Was = N.Weight;
			N = W;
			N.Weight = Was;
		}
		else if (W.Weight > 0.f)
		{
			const float G = FMath::Min(1.f, Dt * 7.f);
			// between anchors the goal can't be eased: the hand goes back to the clip first
			if (N.Anchor != W.Anchor)
			{
				N.Weight += (0.f - N.Weight) * FMath::Min(1.f, Dt * 8.f);
				continue;
			}
			N.Offset += (W.Offset - N.Offset) * G;
			N.Fingers = (N.Fingers + (W.Fingers - N.Fingers) * G).GetSafeNormal(UE_SMALL_NUMBER, W.Fingers);
			N.Palm = (N.Palm + (W.Palm - N.Palm) * G).GetSafeNormal(UE_SMALL_NUMBER, W.Palm);
			N.Curl += (W.Curl - N.Curl) * G;
		}
		N.Weight += (W.Weight - N.Weight) * K;
	}
	GestureNow.HeadPitch += (GestureWant.HeadPitch - GestureNow.HeadPitch) * K;
	GestureNow.HeadYaw += (GestureWant.HeadYaw - GestureNow.HeadYaw) * K;
	GestureNow.HeadRoll += (GestureWant.HeadRoll - GestureNow.HeadRoll) * K;
	GestureNow.SpinePitch += (GestureWant.SpinePitch - GestureNow.SpinePitch) * K;
	GestureNow.SpineRoll += (GestureWant.SpineRoll - GestureNow.SpineRoll) * K;
	GestureNow.Jaw += (GestureWant.Jaw - GestureNow.Jaw) * FMath::Min(1.f, Dt * 8.f);
	GestureNow.bEyesShut = GestureWant.bEyesShut;
	GestureNow.Scale = GestureWant.Scale;
	FLyudBodyAnimProxy& P = GetProxyOnGameThread<FLyudBodyAnimProxy>();
	P.Gesture = GestureNow;
}

void ULyudBodyAnimInstance::UpdateBlink(USkeletalMeshComponent* Mesh)
{
	float V = 0.f;
	if (bEyesShut || GestureNow.bEyesShut || CVarEyesShut.GetValueOnGameThread())
	{
		V = 1.f;
	}
	else
	{
		if (Clock >= BlinkAt)
		{
			BlinkStart = Clock;
			if (CVarFaceLog.GetValueOnAnyThread())
			{
				UE_LOG(LogTemp, Warning, TEXT("LyudFace %s: blink at %.2f s"), *GetNameSafe(Mesh->GetSkeletalMeshAsset()), Clock);
			}
			// now and then a double blink; more often while talking
			bDouble = Rng.FRand() < 0.12f;
			BlinkAt = Clock + (bDouble ? 0.32f : Rng.FRandRange(2.f, 6.f) * (SpeakStart >= 0.f || bChatter ? 0.7f : 1.f));
		}
		// 70 ms closing, 50 ms shut, 110 ms opening
		const float T = Clock - BlinkStart;
		V = T < 0.07f ? T / 0.07f : T < 0.12f ? 1.f : T < 0.23f ? 1.f - (T - 0.12f) / 0.11f : 0.f;
		V = FMath::SmoothStep(0.f, 1.f, V);
	}
	Morph[0] = Morph[1] = V;
}

void ULyudBodyAnimInstance::UpdateLook(USkeletalMeshComponent* Mesh, float Dt)
{
	// where the attention goes: the candidate (the player) for 2-6 s at a time, 3-8 s elsewhere, while they are in
	// front within 70 deg; otherwise an idle look about every 3-8 s
	const FTransform C = Mesh->GetComponentTransform();
	const FVector HeadAt = Mesh->GetSocketLocation(TEXT("head"));
	FVector2D Want = FVector2D::ZeroVector;
	float HeadShare = 0.55f;
	bool bTarget = false;
	if (bCandidate && !bEyesShut)
	{
		const FVector D = C.InverseTransformVectorNoScale(Candidate - HeadAt);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.X, D.Y));
		const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(D.Z, FVector2D(D.X, D.Y).Size()));
		if (FMath::Abs(Yaw) < 70.f)
		{
			if (Clock >= AttendUntil)
			{
				bAttending = !bAttending;
				AttendUntil = Clock + (bAttending ? Rng.FRandRange(2.f, 6.f) : Rng.FRandRange(3.f, 8.f));
			}
			if (bAttending || SpeakStart >= 0.f)
			{
				Want = FVector2D(Yaw, FMath::Clamp(Pitch, -25.f, 25.f));
				bTarget = true;
			}
		}
	}
	if (!bTarget)
	{
		if (Clock >= WanderAt)
		{
			WanderAt = Clock + Rng.FRandRange(3.f, 8.f);
			Wander = Rng.FRand() < 0.4f ? FVector2D::ZeroVector : FVector2D(Rng.FRandRange(-30.f, 30.f), Rng.FRandRange(-10.f, 6.f));
		}
		Want = bEyesShut ? FVector2D::ZeroVector : Wander;
		HeadShare = 0.35f;
	}
	// small saccades every 0.3-2 s
	if (Clock >= SaccadeAt)
	{
		SaccadeAt = Clock + Rng.FRandRange(0.3f, 2.f);
		Saccade = FVector2D(Rng.FRandRange(-4.f, 4.f), Rng.FRandRange(-2.5f, 2.5f));
	}
	const FVector2D HeadWant(FMath::Clamp(Want.X * HeadShare, -35.f, 35.f), FMath::Clamp(Want.Y * HeadShare, -12.f, 12.f));
	Head += (HeadWant - Head) * FMath::Min(1.f, Dt * 3.f);
	const FVector2D EyeWant(FMath::Clamp(Want.X - Head.X + Saccade.X, -28.f, 28.f), FMath::Clamp(Want.Y - Head.Y + Saccade.Y, -14.f, 14.f));
	Eye += (EyeWant - Eye) * FMath::Min(1.f, Dt * 25.f);     // eyes jump, heads follow
	FLyudBodyAnimProxy& P = GetProxyOnGameThread<FLyudBodyAnimProxy>();
	P.HeadYaw = Head.X;
	P.HeadPitch = Head.Y;
	if (!bFace)
	{
		return;
	}
	if (!bEyesMade)
	{
		MakeEyes(Mesh);
	}
	if (EyeMIDs.Num() == 0)
	{
		return;
	}
	// the gaze in mesh space (+Y forward, +X the character's left), then in each ball's frame (X forward, turned by
	// EyeYaw; Y across, Z up); Eye.X turns toward the character's left
	FVector2D E = Eye;
	if (CVarEyeFix.GetValueOnGameThread())
	{
		E = FVector2D(CVarEyeYaw.GetValueOnGameThread(), CVarEyePitch.GetValueOnGameThread());
	}
	const float Y = FMath::DegreesToRadians(E.X), Pt = FMath::DegreesToRadians(E.Y);
	const FVector D(FMath::Sin(Y) * FMath::Cos(Pt), FMath::Cos(Y) * FMath::Cos(Pt), FMath::Sin(Pt));
	for (int32 I = 0; I < 2; I++)
	{
		const FRotator R(0.f, 90.f - EyeYaw[I], 0.f);
		const FVector L = R.UnrotateVector(D);
		EyeMIDs[I]->SetVectorParameterValue(TEXT("Gaze"), FLinearColor(L.X, L.Y, L.Z, 0.f));
	}
}

void ULyudBodyAnimInstance::MakeEyes(USkeletalMeshComponent* Mesh)
{
	// blender/characters/eyes.py -> unreal/scripts/set_eyes.py, in the face's MI (mesh cm): Ball (middle, turn about Z in
	// Blender's sense, the outer end back) and Size (depth, width, height, how much further back the dark socket sits
	// that fills the corners); the ball is the engine sphere stretched, its X looking forward (mesh +Y) before the turn
	bEyesMade = true;
	USkeletalMesh* SK = Mesh->GetSkeletalMeshAsset();
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* EyeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Characters/Humans/Materials/M_HumanEyeball.M_HumanEyeball"));
	const int32 HeadBone = SK ? SK->GetRefSkeleton().FindBoneIndex(TEXT("head")) : INDEX_NONE;
	UMaterialInterface* Face = Mesh->GetMaterial(0);
	if (!Sphere || !EyeMat || HeadBone == INDEX_NONE || !Mesh->GetOwner() || !Face)
	{
		return;
	}
	FLinearColor Ball[2], Size[2], Iris(0.05f, 0.03f, 0.02f, 1.f);
	for (int32 S = 0; S < 2; S++)
	{
		if (!Face->GetVectorParameterValue(FHashedMaterialParameterInfo(S ? TEXT("EyeR_Ball") : TEXT("EyeL_Ball")), Ball[S])
			|| !Face->GetVectorParameterValue(FHashedMaterialParameterInfo(S ? TEXT("EyeR_Size") : TEXT("EyeL_Size")), Size[S])
			|| Size[S].R <= 0.f)
		{
			return;                                       // no eyes set (glasses: the painted ones stay)
		}
	}
	Face->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("IrisColor")), Iris);
	const FTransform HeadRef = FAnimationRuntime::GetComponentSpaceTransformRefPose(SK->GetRefSkeleton(), HeadBone);
	for (int32 K = 0; K < 4; K++)
	{
		const int32 S = K % 2;
		const bool bSocket = K >= 2;
		FVector Dim(Size[S].R, Size[S].G, Size[S].B);
		FVector At(Ball[S].R, Ball[S].G, Ball[S].B);
		if (bSocket)
		{
			At.Y -= Size[S].A;
			Dim = FVector(Dim.X * 0.7f, Dim.Y * 1.2f, Dim.Z * 1.35f);
		}
		// Blender's yaw t about Z (y flipped into UE): forward (sin t, cos t) -> UE yaw 90 - t
		EyeYaw[S] = Ball[S].A;
		const FTransform InMesh(FRotator(0.f, 90.f - Ball[S].A, 0.f), At, Dim / 50.f);
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Mesh->GetOwner());
		C->SetStaticMesh(Sphere);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetReceivesDecals(false);
		// they move with the head every frame: out of the distance fields and Lumen (perf checklist); a few cm, unshadowed
		C->bAffectDistanceFieldLighting = false;
		C->bAffectDynamicIndirectLighting = false;
		C->SetupAttachment(Mesh, TEXT("head"));
		C->SetRelativeTransform(InMesh.GetRelativeTransform(HeadRef));
		C->RegisterComponent();
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(EyeMat, C);
		M->SetVectorParameterValue(TEXT("Size"), FLinearColor(Dim.X, Dim.Y, Dim.Z, bSocket ? 1.f : 0.f));
		M->SetVectorParameterValue(TEXT("IrisColor"), Iris);
		M->SetScalarParameterValue(TEXT("IrisR"), 0.49f * Size[S].R);
		M->SetScalarParameterValue(TEXT("PupilR"), 0.18f * Size[S].R);
		C->SetMaterial(0, M);
		Eyeballs.Add(C);
		if (!bSocket)
		{
			EyeMIDs.Add(M);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("LyudFace %s: eyeballs (r %.2f, %.2f cm, turned %.0f, %.0f)"), *GetNameSafe(SK), Size[0].R, Size[1].R, Ball[0].A, Ball[1].A);
}

void ULyudBodyAnimInstance::UpdateMouth(USkeletalMeshComponent* Mesh, float Dt)
{
	FVector Want = FVector::ZeroVector;
	if (SpeakStart >= 0.f)
	{
		// on the world's clock (the voice's), not the body's own: an unseen body's Clock stands still
		const UWorld* World = GetWorld();
		const float Frame = (World ? (float)(World->GetTimeSeconds() - SpeakStartWorld) : Clock - SpeakStart) * 30.f;
		if (Frame >= (float)Jaw.Num())
		{
			SpeakStart = -1.f;
		}
		else
		{
			Want = FVector(CurveAt(Jaw, Frame), CurveAt(Funnel, Frame), CurveAt(Smile, Frame));
		}
	}
	else if (bChatter && !bEyesShut)
	{
		// talking without a voice: bursts of syllables (~5 a second) while it's their turn, quiet while the other talks
		if (Clock >= ChatterNext)
		{
			bChatterOn = !bChatterOn;
			ChatterNext = Clock + (bChatterOn ? Rng.FRandRange(1.f, 3.5f) : Rng.FRandRange(0.8f, 3.f));
		}
		if (bChatterOn)
		{
			const float S = 0.5f + 0.5f * FMath::Sin(Clock * 31.f) * FMath::Sin(Clock * 7.3f + 1.f);
			Want = FVector(0.08f + 0.32f * S, 0.25f * (0.5f + 0.5f * FMath::Sin(Clock * 3.1f)), 0.2f * (0.5f + 0.5f * FMath::Sin(Clock * 4.7f + 2.f)));
		}
	}
	Mouth += (Want - Mouth) * FMath::Min(1.f, Dt * 18.f);
	if (CVarFaceLog.GetValueOnAnyThread() && FMath::FloorToInt(Clock) != FMath::FloorToInt(Clock - Dt))
	{
		UE_LOG(LogTemp, Warning, TEXT("LyudFace %s: jaw %.2f funnel %.2f smile %.2f, eyes (%.1f, %.1f), head (%.1f, %.1f), eyeballs %d"),
			*GetNameSafe(Mesh->GetSkeletalMeshAsset()), Mouth.X, Mouth.Y, Mouth.Z, Eye.X, Eye.Y, Head.X, Head.Y, Eyeballs.Num());
	}
	// a yawn opens the mouth whatever is being said
	Morph[2] = FMath::Max((float)Mouth.X, GestureNow.Jaw);
	Morph[3] = Mouth.Y;
	Morph[4] = Mouth.Z;
	// the brows lift a little on loud syllables
	Morph[5] = FMath::Clamp((Mouth.X - 0.35f) * 1.5f, 0.f, 0.4f);
}

// --- the level's placed people ----------------------------------------------------------------------------

bool ULyudFaceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void ULyudFaceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	int32 N = 0;
	for (TActorIterator<ASkeletalMeshActor> It(&InWorld); It; ++It)
	{
		USkeletalMeshComponent* Mesh = It->GetSkeletalMeshComponent();
		USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
		if (!Asset || !Asset->GetSkeleton() || Asset->GetSkeleton()->GetName() != TEXT("SK_Human_Skeleton")
			|| Mesh->GetAnimationMode() != EAnimationMode::AnimationSingleNode)
		{
			continue;
		}
		if (!Cast<UAnimSequence>(Mesh->AnimationData.AnimToPlay))
		{
			continue;
		}
		// hidden (their person away from the station, a stray copy) or off screen, they don't animate: the life's body or
		// nobody shows them meanwhile (ANpcBody does the same); the lips keep the voice's time on the world clock (Speak)
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		// the instance plays AnimationData's clip itself (NativeInitializeAnimation)
		Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		Mesh->SetAnimInstanceClass(ULyudBodyAnimInstance::StaticClass());
		N++;
	}
	UE_LOG(LogTemp, Log, TEXT("LyudFace: %d placed people blink, look about and speak"), N);
}

// --- console (tests, unreal/scripts/test_faces.py) ------------------------------------------------------------

static FAutoConsoleCommandWithWorldAndArgs GFaceSpeak(TEXT("lyud.Face.Speak"),
	TEXT("lyud.Face.Speak <mesh, e.g. SK_Nao> <line key>: that person's lips play the line's lip-sync (no sound)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 2 || !World)
		{
			return;
		}
		int32 N = 0;
		for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && It->GetSkeletalMeshAsset() && It->GetSkeletalMeshAsset()->GetName() == Args[0])
			{
				N += ULyudBodyAnimInstance::SpeakLine(*It, Args[1]) ? 1 : 0;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("LyudFace: %s speaks %s on %d bodies"), *Args[0], *Args[1], N);
	}));

static FAutoConsoleCommandWithWorldAndArgs GFaceChatter(TEXT("lyud.Face.Chatter"),
	TEXT("lyud.Face.Chatter <0|1>: every body chatters without a voice (tests)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const bool bOn = Args.Num() > 0 && Args[0] != TEXT("0");
		for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
		{
			if (ULyudBodyAnimInstance* A = It->GetWorld() == World ? Cast<ULyudBodyAnimInstance>(It->GetAnimInstance()) : nullptr)
			{
				A->SetChatter(bOn);
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GBodyOnFeet(TEXT("lyud.Body.OnFeet"),
	TEXT("lyud.Body.OnFeet <0|1>: every body plants its feet on the ground (tests; ANpcBody sets it by its pose)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const bool bOn = Args.Num() == 0 || Args[0] != TEXT("0");
		for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
		{
			if (ULyudBodyAnimInstance* A = It->GetWorld() == World ? Cast<ULyudBodyAnimInstance>(It->GetAnimInstance()) : nullptr)
			{
				A->SetOnFeet(bOn);
			}
		}
	}));
