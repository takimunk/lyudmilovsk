#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimalAnimInstance.generated.h"

class UAnimSequence;

/** One clip playing on a cat or dog: where it is, how fast, how much of the pose. */
USTRUCT()
struct FAnimalClipLayer
{
	GENERATED_BODY()

	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Seq;
	double Time = 0.0;
	float Rate = 1.f;
	float Weight = 0.f;
	float Target = 0.f;
	float FadeSpeed = 5.f;
	bool bLoop = true;
	FName Clip;
};

/** What the head, jaw, ears and tail do on top of the clips this frame. */
USTRUCT()
struct FAnimalOverlay
{
	GENERATED_BODY()

	/** Head look target, component space (cm), and how much. */
	FVector Look = FVector::ZeroVector;
	float LookWeight = 0.f;
	/** 0..1 mouth open (bark, meow, hiss, panting). */
	float Jaw = 0.f;
	/** Tail wag: amplitude (degrees a bone) and phase (radians). */
	float WagDeg = 0.f;
	float WagPhase = 0.f;
	/** Ears: + laid back (fear, anger), - pricked forward (interest); degrees. */
	float EarsBackDeg = 0.f;
	/** Breathing / panting depth 0..1. */
	float Pant = 0.f;
	float Time = 0.f;
};

/**
 * Plays an animal's clips without an anim graph (like the birds'): cross-faded layers, then the head turned toward a
 * look target (the neck takes part), the jaw opened, the ears turned and the tail wagged, in component space.
 * AAnimalActor drives it.
 */
USTRUCT()
struct FAnimalAnimProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FAnimalAnimProxy() = default;
	FAnimalAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	TArray<FAnimalClipLayer, TInlineAllocator<4>> Layers;
	FAnimalOverlay Over;
};

UCLASS(Transient, NotBlueprintable)
class LYUDMILOVSK_API UAnimalAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Fades every layer but Clip out and Clip in over BlendTime; Rate is clip seconds per second. */
	void Play(FName Clip, UAnimSequence* Seq, float Rate, bool bLoop, float BlendTime = 0.25f, bool bRestart = false);
	void SetRate(FName Clip, float Rate);
	FName LeadClip() const;
	/** 0..1 through the leading clip (1 when a one-shot has played out). */
	float LeadProgress() const;

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(Transient) TArray<FAnimalClipLayer> Layers;
	FAnimalOverlay Over;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FAnimalAnimProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete InProxy; }
};
