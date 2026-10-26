#include "People/LyudBodyAnim.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/SkeletalMeshActor.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
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
}

static TAutoConsoleVariable<int32> CVarFaceLog(TEXT("lyud.Face.Log"), 0, TEXT("1: log each body's face values once a second."));

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
	for (int32 I = 0; I < FaceCurves.Num(); I++)
	{
		Output.Curve.Set(FaceCurveNames()[I], FaceCurves[I]);
	}
	if (FMath::Abs(HeadYaw) + FMath::Abs(HeadPitch) > 0.05f)
	{
		// the turn on top of the clip, a third in the neck and the rest in the head, about the component's up (yaw) and
		// its sideways axis (pitch; the mesh faces +Y, its left is +X)
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		FCSPose<FCompactPose> CS;
		CS.InitPose(Output.Pose);
		for (const TPair<const TCHAR*, float>& B : { TPair<const TCHAR*, float>(TEXT("neck_01"), 0.35f), TPair<const TCHAR*, float>(TEXT("head"), 0.65f) })
		{
			const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(B.Key);
			if (MeshIndex == INDEX_NONE)
			{
				continue;
			}
			const FCompactPoseBoneIndex Index = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
			if (!Index.IsValid())
			{
				continue;
			}
			FTransform T = CS.GetComponentSpaceTransform(Index);
			const FQuat Q = FQuat(FVector::UpVector, FMath::DegreesToRadians(HeadYaw * B.Value))
				* FQuat(FVector::XAxisVector, FMath::DegreesToRadians(HeadPitch * B.Value));
			T.SetRotation(Q * T.GetRotation());
			CS.SetComponentSpaceTransform(Index, T);
		}
		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose);
	}
	return true;
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
	UpdateLook(Mesh, DeltaSeconds);
	if (bFace)
	{
		UpdateBlink(Mesh);
		UpdateMouth(Mesh, DeltaSeconds);
		GetProxyOnGameThread<FLyudBodyAnimProxy>().FaceCurves = TArray<float>(Morph, 6);
	}
}

void ULyudBodyAnimInstance::UpdateBlink(USkeletalMeshComponent* Mesh)
{
	float V = 0.f;
	if (bEyesShut)
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
	if (!FaceMID)
	{
		UMaterialInterface* M = Mesh->GetMaterial(0);
		FHashedMaterialParameterInfo Info(TEXT("EyeL_Look"));
		FLinearColor Probe;
		if (M && M->GetVectorParameterValue(Info, Probe))
		{
			FaceMID = Mesh->CreateDynamicMaterialInstance(0, M);
		}
		if (!FaceMID)
		{
			return;
		}
	}
	// the painted iris slides on the face: ~11 mm eye radius (face_rig.py frame: +x the character's left, z up)
	const FLinearColor Look(FMath::Sin(FMath::DegreesToRadians(Eye.X)) * 0.011f, FMath::Sin(FMath::DegreesToRadians(Eye.Y)) * 0.008f, 0.f, 0.f);
	FaceMID->SetVectorParameterValue(TEXT("EyeL_Look"), Look);
	FaceMID->SetVectorParameterValue(TEXT("EyeR_Look"), Look);
}

void ULyudBodyAnimInstance::UpdateMouth(USkeletalMeshComponent* Mesh, float Dt)
{
	FVector Want = FVector::ZeroVector;
	if (SpeakStart >= 0.f)
	{
		const float Frame = (Clock - SpeakStart) * 30.f;
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
		UE_LOG(LogTemp, Warning, TEXT("LyudFace %s: jaw %.2f funnel %.2f smile %.2f, eyes (%.1f, %.1f), head (%.1f, %.1f), MID %d"),
			*GetNameSafe(Mesh->GetSkeletalMeshAsset()), Mouth.X, Mouth.Y, Mouth.Z, Eye.X, Eye.Y, Head.X, Head.Y, FaceMID ? 1 : 0);
	}
	Morph[2] = Mouth.X;
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
