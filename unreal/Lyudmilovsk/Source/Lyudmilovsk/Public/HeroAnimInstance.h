#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "HandReach.h"
#include "Bikes/LyudBikeRider.h"
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
	/** 0 standing, 1 in a deep crouch (HandReach::ApplyCrouch). */
	float Crouch = 0.f;
	/** 0 standing, 1 seated on a bench SeatZ cm high (HandReach::ApplySit; the train's benches). */
	float Sit = 0.f;
	float SeatZ = 42.f;
	/** 0 on land .. 1 swimming (the swim clips), how much of it is the forward stroke (0 treading water .. 1), the
	 *  body's pitch along the stroke (degrees, + head up) and the strokes' clock, s. */
	float Swim = 0.f;
	float SwimMove = 0.f;
	float SwimPitch = 0.f;
	double SwimTime = 0.0;
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
	/** Treading water and the forward stroke (UAL's, retargeted: blender/humans/clips.json SwimIdle, SwimFwd). */
	TObjectPtr<UAnimSequence> SwimIdle;
	TObjectPtr<UAnimSequence> SwimFwd;
	/** Where the head is standing (component space, the idle's first frame): the swimming body hangs from it. */
	FVector StandHead = FVector::ZeroVector;
	bool bStandHead = false;
	/** Turns the swimming body about the head: the swim clips' head put where the standing head is, pitched along the stroke. */
	void PlaceSwimmer(FCompactPose& Pose);
	float WalkClipSpeed = 290.f, JogClipSpeed = 580.f;
	FHeroReachPose Reach;
	FBikeRiderPose Bike;
	/** How far the hips go down in a full crouch, cm. */
	static constexpr float CrouchDrop = 34.f;

	/** The clothes around the torso, cm from the mesh's midline / front-back centre (SK_Hero: oversized overshirt). */
	static constexpr float ClothesHalfWidth = 30.f;
	static constexpr float ClothesHalfDepth = 20.f;
	/** Swings each arm out from the shoulder just enough that the hand stays outside the clothes. */
	void KeepHandsOutsideClothes(FCompactPose& Pose);
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

	/** Where the right hand goes (UHeroReachComponent sets it every frame; Weight 0 leaves the animation alone). */
	FHeroReachPose Reach;

	/** On a bicycle or leading one by the bars (UHeroBikeComponent every frame; Weight 0 leaves the body alone). */
	FBikeRiderPose Bike;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Idle;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> Walk;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> Jog;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> SwimIdle;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> SwimFwd;
	TArray<float> WalkStride, JogStride;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FHeroAnimProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete InProxy; }
};
