#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "HeroAnimInstance.generated.h"

class UAnimSequence;

/** What the hero's body is doing this frame (copied to the worker thread). */
USTRUCT()
struct FHeroBodyState
{
	GENERATED_BODY()

	/** Ground speed, cm/s. */
	float Speed = 0.f;
	/** Direction of travel relative to the body, degrees (0 ahead, 90 right, +-180 back). */
	float MoveAngle = 0.f;
	/** Look relative to the body: yaw (right +) and pitch (up +), degrees. */
	float LookYaw = 0.f;
	float LookPitch = 0.f;
	/** Ground covered, cm (drives the locomotion clips, so feet don't slip). */
	double Distance = 0.0;
	double IdleTime = 0.0;
};

/**
 * Locomotion without an anim graph: the template's idle + 8-way walk + 8-way jog (the mannequin's
 * clips retargeted onto the hero by unreal/scripts/import_hero.py), blended by speed and direction,
 * each clip played by distance over its own stride. Then, as in Murmuration's first-person body,
 * the spine, neck and head follow the look: up and sideways they share the pitch and the twist from
 * the hips, looking down past ~25 degrees the neck and head flex (a chin tuck), so the eyes come
 * forward and you see your chest, legs and shoes.
 */
USTRUCT()
struct FHeroAnimProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FHeroAnimProxy() = default;
	FHeroAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	FHeroBodyState State;
	TObjectPtr<UAnimSequence> Idle;
	TArray<TObjectPtr<UAnimSequence>> Walk;    // Fwd, Fwd_Right, Right, Bwd_Right, Bwd, Bwd_Left, Left, Fwd_Left
	TArray<TObjectPtr<UAnimSequence>> Jog;
	TArray<float> WalkStride, JogStride;       // cm per clip loop
	float WalkClipSpeed = 290.f, JogClipSpeed = 580.f;
};

UCLASS(Transient, NotBlueprintable)
class LYUDMILOVSK_API UHeroAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	FHeroBodyState State;

	/** Speeds the walk and jog clips were made for (their root motion), cm/s. */
	float WalkClipSpeed = 290.f;
	float JogClipSpeed = 580.f;

	virtual void NativeInitializeAnimation() override;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Idle;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> Walk;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> Jog;
	TArray<float> WalkStride, JogStride;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FHeroAnimProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete InProxy; }
};
