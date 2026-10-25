#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "BirdAnimInstance.generated.h"

class UAnimSequence;

/** One clip playing on a bird: where it is, how fast it goes, how much of the pose it is. */
USTRUCT()
struct FBirdClipLayer
{
	GENERATED_BODY()

	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Seq;
	double Time = 0.0;
	float Rate = 1.f;
	float Weight = 0.f;
	/** Weight it is heading to, and how fast (per second). */
	float Target = 0.f;
	float FadeSpeed = 8.f;
	bool bLoop = true;
	FName Clip;
};

/** What the head should do this frame (component space, cm). */
USTRUCT()
struct FBirdLook
{
	GENERATED_BODY()

	FVector Target = FVector::ZeroVector;
	float Weight = 0.f;
	/** Extra roll of the head (one eye on the thing), degrees. */
	float Tilt = 0.f;
};

/**
 * Plays a bird's clips without an anim graph (like the hero's): a few layers cross-faded by weight, then the neck and
 * head turned toward a look target in component space, split between the two bones, within the species' reach.
 * ABirdActor drives it; the layers live here and are copied to the worker thread each frame.
 */
USTRUCT()
struct FBirdAnimProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FBirdAnimProxy() = default;
	FBirdAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	TArray<FBirdClipLayer, TInlineAllocator<4>> Layers;
	FBirdLook Look;
	/** Body roll / tail twist added on top (player flight), degrees. */
	float BankDeg = 0.f;
	float TailPitchDeg = 0.f;
	/** Length of the outermost tail feathers (a female swallow's shorter streamers). */
	float StreamerScale = 1.f;
};

UCLASS(Transient, NotBlueprintable)
class LYUDMILOVSK_API UBirdAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Fades every layer but Clip out and Clip in over BlendTime; Rate is clip seconds per second. */
	void Play(FName Clip, UAnimSequence* Seq, float Rate, bool bLoop, float BlendTime = 0.15f, bool bRestart = true);
	/** Sets a layer's weight target without touching the others (flight mixes: Flap + Glide + Bound). */
	void Mix(FName Clip, UAnimSequence* Seq, float Weight, float Rate, float BlendTime = 0.2f);
	void SetRate(FName Clip, float Rate);
	/** The leading layer's clip and how far through it (0..1); 1 when a one-shot has played out. */
	FName LeadClip() const;
	float LeadProgress() const;

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(Transient) TArray<FBirdClipLayer> Layers;
	FBirdLook Look;
	float BankDeg = 0.f;
	float TailPitchDeg = 0.f;
	float StreamerScale = 1.f;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FBirdAnimProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete InProxy; }
};
