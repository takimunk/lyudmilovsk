#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Bikes/LyudBikeRider.h"
#include "LyudLadder.generated.h"

class ACharacter;
class UInstancedStaticMeshComponent;
class UMaterialInterface;

/**
 * A ladder (docs/ladders.md): where its rungs are, for anybody climbing it. Put it on any actor whose mesh has a
 * ladder (the Europa shafts) or use ALyudLadder, which draws one. Its frame: the origin on the floor under the middle
 * of the rung line, +X out from the ladder toward whoever climbs it (they face -X), +Z up, the rungs along Y.
 *
 * bInteractive ladders are the player's to climb: look at one close by and E takes hold (ULyudLadderSubsystem), W and
 * S climb, Space lets go; at the top the climb steps off onto TopExit, at the bottom onto the floor. Scripted climbs
 * (the hatch to Europa) leave it false and drive ULyudLadderClimber themselves.
 */
UCLASS(ClassGroup = (Lyud), meta = (BlueprintSpawnableComponent))
class LYUDMILOVSK_API ULyudLadderComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	/** Rung to rung, cm, and the lowest rung over the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder") float RungSpacing = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder") float FirstRungZ = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder", meta = (ClampMin = "1")) int32 RungCount = 10;
	/** Half the rails' spacing and the rungs' radius, cm (the hands close round a rung this thick). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder") float HalfWidth = 22.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder") float RungRadius = 1.6f;
	/** How far out from the rung line the climber's hips hang, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder") float BodyStandoff = 26.f;
	/** The player climbs it with E (ULyudLadderSubsystem); off for ladders a script drives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder") bool bInteractive = true;
	/** Where an interactive climb steps off at the top (ladder frame, cm: the feet), e.g. onto a roof behind it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder") FVector TopExit = FVector(-45.f, 0.f, 0.f);
	/** Rungs a second an interactive climb goes (hand over hand). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder") float ClimbRungsPerSecond = 2.4f;
	/** The sound of a hand or a boot on a rung (ULyudSoundscapeSubsystem's pool; none = silent) and its volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder") FName GripSound = TEXT("ladder_step");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder") float GripVolume = 0.6f;

	/** A rung's height over the floor (any index: below 0 and past the top are where they would be), cm, ladder frame. */
	float RungZ(int32 Index) const { return FirstRungZ + Index * RungSpacing; }
	float TopRungZ() const { return RungZ(RungCount - 1); }
	/** A rung's middle, Across cm along it (+ toward the climber's right), world. */
	FVector RungWorld(int32 Index, float Across = 0.f) const;
	/** A point of the ladder's frame in the world. */
	FVector ToWorld(const FVector& Local) const { return GetComponentTransform().TransformPosition(Local); }
	FVector FromWorld(const FVector& World) const { return GetComponentTransform().InverseTransformPosition(World); }
	/** The way a climber faces (toward the ladder, level). */
	FVector Facing() const;

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
};

/** A limb of a climber: 0 left hand, 1 right hand, 2 left foot, 3 right foot. */
namespace LyudLadder
{
	/** Where each hand and foot is on a ladder with the feet FeetZ over its floor (cm, ladder frame), hand over hand: the
	 *  climb goes in cycles of two rungs (right hand, left foot, left hand, right foot), each limb moving two rungs
	 *  while the body rises by them; going down is the same played backwards (the feet lead). Out: per limb the rung
	 *  index it holds or is moving to/from, how far along that move it is (0 holding the lower, 1 the upper), and the
	 *  points (ladder frame) and the hands' closing (1 on the rung, 0 open in the air). */
	struct FLimbs
	{
		FVector At[4];
		float Close[4] = { 1.f, 1.f, 1.f, 1.f };
		/** Moving (between rungs) 0..1, 0 = settled. */
		float Moving[4] = { 0.f, 0.f, 0.f, 0.f };
		/** The rung each limb is on now (the nearer end of a move). */
		int32 Rung[4] = { 0, 0, 0, 0 };
		bool bOnFloor[2] = { false, false };
	};
	LYUDMILOVSK_API FLimbs Limbs(const ULyudLadderComponent& L, float FeetZ, float HandAboveFeet);
}

/**
 * The body on a ladder: put on a character (the hero) while it climbs. Something moves the feet along the ladder
 * (SetFeetZ: a script, or the player's W/S through ULyudLadderSubsystem); this places the character, holds its body
 * facing the ladder and every frame bends it onto the rungs: both hands closed round rungs (fingers stop on them),
 * hand over hand, the feet on the rungs below following, the hips out from the ladder (LyudBikeRider's whole-body
 * targets, the hero's animation applies them). A hand or a boot meeting a rung plays the ladder's grip sound.
 */
UCLASS(ClassGroup = (Lyud))
class LYUDMILOVSK_API ULyudLadderClimber : public UActorComponent
{
	GENERATED_BODY()

public:
	ULyudLadderClimber();

	/** The character's climber (made on first use). */
	static ULyudLadderClimber* Get(ACharacter* Who, bool bCreate = true);

	/** Onto a ladder (the body blends onto it over BlendSeconds; a jump between two alike ladders passes 0 and keeps
	 *  the hands and feet where they were on the other one). The caller has taken the character off its own movement. */
	void Attach(ULyudLadderComponent* Ladder, float FeetZ, float BlendSeconds = 0.35f);
	/** Off it: the body back to its own animation over BlendSeconds (the character stays where it is). */
	void Release(float BlendSeconds = 0.4f);
	/** Puts the feet FeetZ over the ladder's floor (cm, ladder frame) and the character with them. */
	void SetFeetZ(float FeetZ);
	float GetFeetZ() const { return FeetZ; }
	ULyudLadderComponent* GetLadder() const { return Ladder.Get(); }
	bool IsOnLadder() const { return Ladder.IsValid() && !bReleasing; }
	/** How much the body is on the ladder now, 0..1. */
	float GetWeight() const { return Weight; }

	/** The player climbing an interactive ladder: E took hold; W/S (Input -1..1) climb; Space lets go. */
	void GrabByPlayer(ULyudLadderComponent* Ladder);
	void SetClimbInput(float Input) { ClimbInput = FMath::Clamp(Input, -1.f, 1.f); }
	void LetGo();
	bool IsPlayerClimbing() const { return bPlayerDriven && IsOnLadder(); }

	/** The lower hand's rung over the feet, cm (the hero: by the chin). */
	UPROPERTY(EditAnywhere, Category = "Ladder") float HandAboveFeet = 128.f;
	/** The hips over the feet, cm (knees a little bent), and the trunk's lean toward the ladder, degrees. */
	UPROPERTY(EditAnywhere, Category = "Ladder") float HipsAboveFeet = 86.f;
	UPROPERTY(EditAnywhere, Category = "Ladder") float LeanDeg = 6.f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	TWeakObjectPtr<ULyudLadderComponent> Ladder;
	float FeetZ = 0.f;
	float Weight = 0.f;
	float BlendRate = 3.f;
	bool bReleasing = false;
	/** Each limb's rung last frame (a change that settles = it met a rung). */
	int32 LastRung[4] = { INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN };
	bool bWasMoving[4] = { false, false, false, false };

	// the player's climb
	bool bPlayerDriven = false;
	float ClimbInput = 0.f;
	/** Stepping off at the top or the bottom: from, to (world feet), time. */
	bool bStepping = false;
	FVector StepFrom = FVector::ZeroVector, StepTo = FVector::ZeroVector;
	float StepT = 0.f;
	void TickPlayer(float DeltaTime);
	void StepOff(const FVector& ToFeet);
	void EndPlayerClimb();

	void Place();
	void Pose(float DeltaTime);
};

/** Interactive ladders: the prompt and E for the one the player looks at (polled, never consumed). */
UCLASS()
class LYUDMILOVSK_API ULyudLadderSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudLadderSubsystem* Get(const UObject* WorldContext);
	void Register(ULyudLadderComponent* L) { Ladders.AddUnique(L); }
	void Unregister(ULyudLadderComponent* L) { Ladders.Remove(L); }
	/** The nearest interactive ladder. */
	ULyudLadderComponent* Nearest(const FVector& At) const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudLadderSubsystem, STATGROUP_Tickables); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TArray<TWeakObjectPtr<ULyudLadderComponent>> Ladders;
	TWeakObjectPtr<ULyudLadderComponent> Shown;
	bool bPrompt = false;
};

/** A free-standing ladder that draws itself (two rails and RungCount rungs, the engine's basic shapes, built once). */
UCLASS()
class LYUDMILOVSK_API ALyudLadder : public AActor
{
	GENERATED_BODY()

public:
	ALyudLadder();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder") TObjectPtr<ULyudLadderComponent> Ladder;
	UPROPERTY(VisibleAnywhere, Category = "Ladder") TObjectPtr<UInstancedStaticMeshComponent> Rails;
	UPROPERTY(VisibleAnywhere, Category = "Ladder") TObjectPtr<UInstancedStaticMeshComponent> Rungs;
	UPROPERTY(EditAnywhere, Category = "Ladder") TObjectPtr<UMaterialInterface> Material;

	virtual void OnConstruction(const FTransform& Transform) override;
};
