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
	/** Ground covered, cm, negative while stepping backwards (drives the walk, so feet don't slip). */
	double Distance = 0.0;
	double IdleTime = 0.0;
};

/**
 * Murmuration's first-person body without an anim graph: its idle and walk (baked onto the hero by
 * blender/hero/build_hero.py) blended by speed; the walk is one gait cycle played by distance over
 * its stride, so the planted foot doesn't slip (backwards plays it backwards, sideways counts as
 * forward). Then the spine, neck and head follow the look: up and sideways they share the pitch and
 * the twist from the hips, looking down past ~25 degrees the neck and head flex (a chin tuck), so
 * the eyes come forward and you see your chest, legs and shoes.
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
	TObjectPtr<UAnimSequence> Walk;
	float WalkSpeed = 140.f;
};

UCLASS(Transient, NotBlueprintable)
class LYUDMILOVSK_API UHeroAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	FHeroBodyState State;

	/** The planted foot moves back this far per walk cycle (fp_body.py: 2 x REACH / STANCE), cm. */
	static constexpr float WalkStride = 90.f;

	/** Speed at which the walk has fully replaced the idle (the pawn's walking pace), cm/s. */
	float WalkSpeed = 140.f;

	virtual void NativeInitializeAnimation() override;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Idle;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Walk;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FHeroAnimProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete InProxy; }
};
