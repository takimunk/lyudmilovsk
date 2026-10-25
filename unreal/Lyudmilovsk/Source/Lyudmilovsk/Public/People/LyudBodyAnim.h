#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudBodyAnim.generated.h"

class UAnimSequence;
class UMaterialInstanceDynamic;
class USkeletalMeshComponent;

/** One clip playing: where in it, how fast, whether it loops. */
struct FLyudBodyClip
{
	TObjectPtr<UAnimSequence> Seq = nullptr;
	float Time = 0.f;
	float Rate = 1.f;
	bool bLoop = true;
};

/** Worker-side state of ULyudBodyAnimInstance: the clip now, the one it is blending from, the head's turn. */
struct FLyudBodyAnimProxy : public FAnimInstanceProxy
{
	FLyudBodyAnimProxy() = default;
	explicit FLyudBodyAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	FLyudBodyClip Cur, Prev;
	/** 0 .. 1 from Prev to Cur. */
	float Alpha = 1.f;
	float BlendTime = 0.3f;
	/** The head's turn toward what the person looks at, on top of the clip (deg; yaw toward the character's left). */
	float HeadYaw = 0.f;
	float HeadPitch = 0.f;
	/** eyeBlinkLeft, eyeBlinkRight, jawOpen, mouthFunnel, mouthSmile, browUp (morph target curves). */
	TArray<float> FaceCurves;

	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;
};

/**
 * A person's body without an animation blueprint (ANpcBody, the club's placed people; docs/people.md, docs/characters.md
 * "Face"): plays one clip at a time like EAnimationMode::AnimationSingleNode, but a new clip cross-fades in from the old
 * one (BlendTime, smoothstep) instead of popping. And the face lives (generated characters with face_rig.py's morph
 * targets and M_HumanFace):
 *  - blinks every 2-6 s (sometimes twice), eyes shut while asleep (SetEyesShut);
 *  - looks about: at the player now and then when they are near and in front (SetLookCandidate), else around; the head
 *    turns part of the way (neck and head on top of the clip), the eyes the rest with small saccades (the painted iris
 *    slides inside the opening, M_HumanFace EyeL/EyeR_Look);
 *  - speaks: a voice line's lip-sync curves (tools/lipsync.py -> Content/Dialogue/lipsync_<lang>.json) drive jawOpen,
 *    mouthFunnel and mouthSmile (SpeakLine); while chatting without a voice (SetChatter) the mouth moves in syllable-like
 *    bursts with pauses for the other one's turn.
 */
UCLASS(Transient, NotBlueprintable)
class LYUDMILOVSK_API ULyudBodyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Starts Seq (from its beginning) blending in over BlendTime seconds; the same clip again only changes the rate. */
	void PlayClip(UAnimSequence* Seq, bool bLoop, float Rate, float BlendTime = 0.3f, float StartTime = 0.f);
	void SetClipRate(float Rate);
	UAnimSequence* CurrentClip() const;

	/** Something worth a look (world cm), instead of the player's eyes when within 6 m (the default). Call every frame;
	 *  bValid false = nothing. */
	void SetLookCandidate(const FVector& Where, bool bValid);
	/** Asleep or lying with closed eyes. */
	void SetEyesShut(bool bShut) { bEyesShut = bShut; }
	/** Talking with someone without a voice line: the mouth moves on its own. */
	void SetChatter(bool bOn) { bChatter = bOn; }
	/** Plays the lip-sync of a voice line (its strings key, the language's curves) from now; false if there are none. */
	bool Speak(const FString& LineKey);
	void StopSpeaking();

	/** Lip-sync for whatever shows a person: their mesh's instance if it is ours. */
	static bool SpeakLine(USkeletalMeshComponent* Mesh, const FString& LineKey);
	static void StopLine(USkeletalMeshComponent* Mesh);

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FLyudBodyAnimProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete InProxy; }
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	bool bFace = false;
	bool bEyesShut = false;
	bool bChatter = false;
	FRandomStream Rng;
	float Clock = 0.f;
	// blinking
	float BlinkAt = 2.f;
	float BlinkStart = -10.f;
	bool bDouble = false;
	// looking
	FVector Candidate = FVector::ZeroVector;
	bool bCandidate = false;
	bool bCandidateGiven = false;
	bool bAttending = false;
	float AttendUntil = 0.f;
	float WanderAt = 0.f;
	FVector2D Wander = FVector2D::ZeroVector;      // yaw, pitch (deg) of an idle look about
	FVector2D Saccade = FVector2D::ZeroVector;
	float SaccadeAt = 0.f;
	FVector2D Head = FVector2D::ZeroVector;        // smoothed head yaw, pitch
	FVector2D Eye = FVector2D::ZeroVector;         // smoothed eye yaw, pitch
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FaceMID;
	// speaking
	TArray<uint8> Jaw, Funnel, Smile;
	float SpeakStart = -1.f;
	float ChatterNext = 0.f;
	bool bChatterOn = false;
	FVector Mouth = FVector::ZeroVector;           // smoothed jaw, funnel, smile
	float Morph[6] = {};                            // the face curves this frame (FaceCurveNames)

	void UpdateBlink(USkeletalMeshComponent* Mesh);
	void UpdateLook(USkeletalMeshComponent* Mesh, float Dt);
	void UpdateMouth(USkeletalMeshComponent* Mesh, float Dt);
};

/**
 * Gives the level's placed people (the club's SkeletalMeshActors on SK_Human_Skeleton playing a single clip) the same
 * body: their clip through ULyudBodyAnimInstance, so they blink, look about and can speak too.
 */
UCLASS()
class LYUDMILOVSK_API ULyudFaceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
};
