#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/PimplPtr.h"
#include "StarlingMurmuration.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMesh;
class FStarlingFlock;

/**
 * Flocking parameters, ported from Dima's Murmuration (src/sim/params.ts).
 * Values follow the StarDisplay model (Hildenbrandt, Carere & Hemelrijk 2010). Forces are in
 * newtons for a 0.08 kg bird, distances in metres, speeds in m/s.
 */
USTRUCT(BlueprintType)
struct FMurmurationParams
{
	GENERATED_BODY()

	/** Topological neighbours each bird tracks (Ballerini et al. 2008: 6-7). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Social") int32 Neighbours = 7;
	/** Separation weight, N. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Social") float Separation = 1.f;
	/** Alignment weight, N. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Social") float Alignment = 0.5f;
	/** Cohesion weight, N (scaled by centrality: birds at the rim pull in hard). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Social") float Cohesion = 1.f;
	/** Hard-sphere radius (body size), m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Social") float HardRadius = 0.2f;
	/** Width of the separation Gaussian, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Social") float SeparationSigma = 1.37f;
	/** Rear blind angle, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Social") float BlindAngle = 90.f;
	/** Max interaction range, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Social") float MaxRange = 25.f;
	/** A bird matches its neighbours' mean speed at this rate, 1/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Social") float SpeedAlign = 1.f;
	/** How often a bird re-reads its neighbours, s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Social") float ReactionTime = 0.05f;
	/** Rate (1/s) at which a bird copies an escaping neighbour's roll. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Social") float EscapeContagion = 40.f;

	/** Cruise speed, m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight") float CruiseSpeed = 10.f;
	/** Speed relaxation time, s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight") float SpeedTau = 1.f;
	/** Max bank angle, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight") float MaxBank = 60.f;
	/**
	 * Max lift in g; below 1/cos(MaxBank) steep turns cost height. StarDisplay uses 1.3; 1.5 keeps the
	 * flock up against the sky while it keeps turning inside a smaller area than Murmuration's bay.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight") float MaxLift = 1.5f;
	/** Roll rate, 1/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight") float RollRate = 6.f;
	/** Random steering noise, N. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight") float Noise = 0.01f;

	/** Preferred flight altitude above the sea, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area") float Altitude = 60.f;
	/** Vertical (altitude) attraction, N/m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area") float AltitudePull = 0.01f;
	/** Horizontal pull back into the roaming area, N. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area") float AreaPull = 0.02f;
	/** Lowest skimming height above the water, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area") float SkimHeight = 0.8f;
	/** Height the birds keep over land (the hill, people's heads), m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area") float LandClearance = 15.f;

	/** Spread of each bird's cruise speed, fraction of CruiseSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Individuality") float SpeedSpread = 0.07f;
	/** Spread of each bird's preferred altitude, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Individuality") float AltitudeSpread = 3.f;
	/** Spread of each bird's roll rate, fraction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Individuality") float RollSpread = 0.3f;
	/** Slow random drift of each bird's preferred speed, fraction of CruiseSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Individuality") float MoodSpeed = 0.05f;
	/** Correlation time of the mood drift, s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Individuality") float MoodTime = 8.f;

	/** Share of the flock that knows where it is heading (Couzin et al. 2005). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leading") float InformedFraction = 0.1f;
	/** Steering weight of an informed bird toward the goal, N. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leading") float LeaderWeight = 0.2f;
	/** Weak goal pull felt by every bird, fraction of LeaderWeight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leading") float FollowShare = 0.2f;
	/** Mean time between new goals, s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leading") float GoalInterval = 16.f;
	/** Turns started by a few birds at the flock's edge, per minute. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leading") float SpontaneousTurns = 4.f;
	/** How often the flock takes on formations (streams, balls, sheets): 0 never, 1 the default mix. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leading") float Formations = 1.f;
	/** How gathered the murmuration is, 0..1 (1 holds together better, shears less). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leading") float Gather = 0.8f;

	/** Large-scale flow field (artistic): 0 off, 1 up to +-15% speed, +-20 m altitude, +-0.8 m/s2 turning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flow") float Flow = 1.f;
	/** Length scale of the flow field, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flow") float FlowScale = 90.f;
	/** Time scale of the flow field, s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flow") float FlowTime = 12.f;
};

/**
 * A murmuration of European starlings, ported from Dima's Murmuration (three.js) to C++:
 * the StarDisplay flocking model with topological neighbours, banked flight with lift,
 * informed leaders, spontaneous turns, a flow field, formations and a peregrine falcon whose
 * attacks spread as escape waves.
 *
 * Place the actor on the sea surface, in the middle of the area the flock should roam;
 * its rotation turns the area. The birds keep over water and well above land: the actor
 * samples the ground under the area with line traces when it starts.
 *
 * The starling is the low-poly flight model from Murmuration's tools/blender/starling.py,
 * built in code (no assets), in a few wing-beat poses.
 */
UCLASS()
class LYUDMILOVSK_API AStarlingMurmuration : public AActor
{
	GENERATED_BODY()

public:
	AStarlingMurmuration();
	virtual ~AStarlingMurmuration() override;

	/** Number of starlings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Murmuration", meta = (ClampMin = "0", ClampMax = "20000"))
	int32 BirdCount = 2500;

	/** Half size of the roaming area around the actor (X, Y), m. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Murmuration")
	FVector2D AreaHalfSize = FVector2D(300.f, 180.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Murmuration")
	FMurmurationParams Params;

	/** A peregrine falcon hunts the flock now and then. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Murmuration|Peregrine")
	bool bPeregrine = true;

	/** Seconds between the falcon's visits (random within the range). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Murmuration|Peregrine")
	FVector2D PeregrineInterval = FVector2D(40.f, 110.f);

	/** How long the falcon hunts each visit, s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Murmuration|Peregrine")
	float PeregrineHunt = 30.f;

	/** Pixels a bird always covers, however far away (a real eye still sees a far flock as a dark cloud). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Murmuration|Look")
	float MinPixels = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Murmuration|Look")
	FLinearColor StarlingColor = FLinearColor(0.010f, 0.010f, 0.013f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Murmuration|Look")
	FLinearColor PeregrineColor = FLinearColor(0.06f, 0.065f, 0.075f);

	/** Run the flock in the editor viewport too (not only in play). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Murmuration")
	bool bSimulateInEditor = false;

	UFUNCTION(BlueprintCallable, Category = "Murmuration")
	void Restart();

	/** Runs the flock headless for a while, logging its extent every 5 s (for tuning from scripts). */
	UFUNCTION(BlueprintCallable, Category = "Murmuration")
	void DebugRun(float Seconds);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return bSimulateInEditor; }

private:
	static constexpr int32 NumPoses = 9;

	UPROPERTY(VisibleAnywhere, Category = "Murmuration")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> PoseComponents;

	UPROPERTY(VisibleAnywhere, Category = "Murmuration")
	TObjectPtr<UStaticMeshComponent> PeregrineComponent;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> PoseMeshes;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> StarlingMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PeregrineMaterial;

	TPimplPtr<FStarlingFlock> Flock;
	float Accumulator = 0.f;
	float PeregrineClock = 0.f;
	float PeregrineNext = 0.f;
	float PeregrineUntil = -1.f;
	TArray<TArray<FTransform>> PoseTransforms;

	void EnsureMeshes();
	void StartFlock();
	void SampleGround();
	void UpdateInstances(float Ahead);
	void LogStats() const;
};
