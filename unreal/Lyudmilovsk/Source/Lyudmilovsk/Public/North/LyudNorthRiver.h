#pragma once

#include "CoreMinimal.h"
#include "North/LyudNorth.h"
#include "LyudNorthRiver.generated.h"

class UAudioComponent;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * The North's river water (docs/north.md "River water (game side)"): one flat sheet along north.json river.water
 * (centre line samples ~15 m apart from the east edge to the mouth: map x, y, the water's surface z, the sheet's half
 * width), flat across each sample at its z, the joins smoothed (Catmull-Rom in plan, z and width linear), built once at
 * spawn as a few transient static meshes of ~300 m each (so the parts off screen are culled). UV: U across 0..1, V along
 * in metres divided by the local flow speed factor (fast stretches stretch and run the pattern faster under a constant
 * panner); vertex colour R = how fast the water is there 0..1 (foam), G = 1 at the very ends (fades the sheet in and out).
 * Material /Game/North/Materials/M_North_River (import_north.py: lit translucent, flowing along V, depth fade at the
 * banks), else the spring's M_North_Water, else an engine material. No collision, no shadow, out of Lumen and the
 * distance fields.
 *
 * Spawned by ULyudNorthSubsystem when north.json has "river", tagged North. It sleeps like any ALyudNorthThing, measured
 * from the water's edge (RangeDistanceCm): near (70 m) it plays one flow loop (/Game/Audio/North/S_north_river_01) from
 * the point of the centre line nearest the player, moved four times a second while awake; asleep it does not tick or
 * sound. The material's flow runs on the GPU clock alone (nothing set from the CPU).
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudNorthRiver : public ALyudNorthThing
{
	GENERATED_BODY()

public:
	ALyudNorthRiver();

	/** The centre line, world cm (X, Y, water surface Z) and W = the sheet's half width in cm; downstream order. Builds
	 *  the sheet (once: a second call does nothing). */
	void Setup(const TArray<FVector4>& InSamples);

	/** Samples given / the centre line's length (m) / the sound playing (tests, DebugReport). */
	int32 NumSamples() const { return Samples.Num(); }
	float LengthM() const { return TotalLengthCm / 100.f; }
	int32 NumParts() const { return Parts.Num(); }
	bool IsSounding() const { return Loop != nullptr; }

	/** The point of the centre line nearest a world location (2D), its water half width (cm) and how fast it runs there
	 *  (0..1); false when there is no river. */
	bool NearestPoint(const FVector& From, FVector& OutPoint, float& OutHalfCm, float& OutSpeed01) const;
	/** Within the water's sheet (2D) and no more than DepthCm under / 50 cm over its surface. */
	UFUNCTION(BlueprintCallable, Category = "North") bool IsInWater(const FVector& WorldLocation, float DepthCm = 150.f) const;

	virtual float RangeDistanceCm(const FVector& From) const override;
	virtual void SetAwake(bool bInAwake, bool bInNear) override;
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FRiverPoint
	{
		FVector Pos = FVector::ZeroVector;   // world cm, centre of the sheet at the water's surface
		FVector Side = FVector::ZeroVector;  // unit, to the right looking downstream (2D)
		float Half = 300.f;                  // cm
		float Speed = 1.f;                   // flow speed factor 0.6..1.8
		float AlongCm = 0.f;                 // centre line distance from the first point
	};

	void BuildPoints();
	void BuildParts();
	void StartLoop();
	void StopLoop();
	void UpdateLoop();

	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> Meshes;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Loop;
	/** The given samples (world cm, W half width cm). */
	TArray<FVector4> Samples;
	/** The smoothed line the sheet is built along. */
	TArray<FRiverPoint> Points;
	float TotalLengthCm = 0.f;
	/** The loop's level now (by how fast the water runs where it plays) and when it started (game seconds). */
	float LoopLevel = 0.f;
	double LoopStartTime = 0.0;
	bool bWarnedSound = false;
	bool bBuilt = false;
};
