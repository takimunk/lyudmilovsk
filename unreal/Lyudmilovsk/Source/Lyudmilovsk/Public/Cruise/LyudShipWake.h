#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "LyudShipWake.generated.h"

class ALyudSea;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;

/**
 * The foam a ship makes going through the water (docs/ship.md), for a hull that really moves (ALyudMotorShip on the
 * roads) and for one that stands still while the sea is carried past it (the cruise stage): a V of spreading rings
 * from the stem, streaks along both sides, the screw's wash laid astern as a white road, spray over the bow in a head
 * sea. What counts is the hull's speed THROUGH the water (HullVelocity - WaterVelocity); every particle lies in the
 * water and goes with it (WaterVelocity), so at the stage the wash streams aft from a ship that goes nowhere. At
 * anchor nothing is made.
 *
 * The lace foam and the instance pools are ULyudTrainWake's (docs/railway.md): instanced meshes simulated here, a few
 * hundred at most, only near the camera, nothing sent while idle. The rates are LyudSeascape::WakeRates
 * (Private/Cruise/SeascapeMath.h, tools/seascapesim).
 */
UCLASS(ClassGroup = (Lyudmilovsk))
class LYUDMILOVSK_API ULyudShipWake : public USceneComponent
{
	GENERATED_BODY()

public:
	ULyudShipWake();

	/**
	 * Every frame from the owner. Hull: the ship frame (cm; x from the stern to the bow, y to starboard, z up from the
	 * waterline) to the world. HullVelocity: the hull's own velocity, cm/s (zero at the stage). WaterVelocity: the
	 * water's, cm/s (zero in the real sea; minus the ship's virtual velocity at the stage).
	 */
	void Feed(const FTransform& Hull, const FVector& HullVelocity, const FVector& WaterVelocity, float DeltaSeconds);

	/** 0 at anchor .. 1 at full speed, as of the last Feed (the sounds follow it). */
	float GetStrength() const { return Strength; }
	int32 NumFoam() const { return Foam.Num(); }
	int32 NumDroplets() const { return Drops.Num(); }

	/** The hull: length and half beam at the waterline, cm. */
	float LengthCm = 6200.f;
	float HalfBeamCm = 500.f;
	/** Sizes and rates of everything (the far, small ship: under 1). */
	float Amount = 1.f;
	/** Beyond this distance from the camera no particles are made (cm). */
	float NearCm = 25000.f;

private:
	struct FDrop { FVector P, V; float Life, Size; };
	/** Kind 0: a ring of foam spreading; 1: a streak along the side, Yaw its way; 2: a patch of the screw's wash. */
	struct FFoam { FVector P, V; float Age, Life, Size; uint8 Kind = 0; float Yaw = 0.f; };
	TArray<FDrop> Drops;
	TArray<FFoam> Foam;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> DropIsm;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> FoamIsm;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> TrailIsm;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> RingMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> PatchMesh;
	TWeakObjectPtr<ALyudSea> Sea;
	float Strength = 0.f;
	float BowDebt = 0.f, SideDebt = 0.f, SternDebt = 0.f, SprayDebt = 0.f;
	FRandomStream Rnd;
	// per-frame scratch kept between frames (Reset, not reallocated)
	TArray<FTransform> DropTs, FoamTs, TrailTs;
	/** How many instances of each ISM were shown last frame (the pool only grows; the rest are parked at zero scale). */
	int32 DropUsed = 0, FoamUsed = 0, TrailUsed = 0;
	void EnsureParts();
	/** Half the hull's breadth at X (cm from the stern) at the waterline. */
	float HalfBeamAt(float X) const;
};
