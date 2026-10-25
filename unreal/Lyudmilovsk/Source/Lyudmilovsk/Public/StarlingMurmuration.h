#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
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
	/** At dusk the flocks are pulled toward the roost from beyond this distance, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area") float RoostRadius = 120.f;

	/** Birds farther than this from the food in the hand don't notice it, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feeding") float NoticeRange = 40.f;
	/** Most birds that come down to the food at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feeding") int32 LureMax = 60;

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
	/**
	 * How gathered the birds are: -1 = from the time of day (scattered groups by day, one huge
	 * murmuration at sunset, with natural swings); 0..1 = fixed, 0 small groups, 1 one murmuration.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leading") float Gather = -1.f;

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
 * The birds live by the game clock (UGameCalendar): they leave inland at dawn, a few loaf along
 * the shore by day, the rest come back in the afternoon, murmurate at sunset and drop into the
 * roost at dusk. The roost is the land nearest the flock at dusk, found from the ground.
 *
 * Hand-feeding: the player holds out feed (F with bird feed in the hand: UBirdTameSubsystem calls SetFeeding and
 * SetFeedHand). Starlings within NoticeRange of
 * the hand come down onto the ground around the player, hop, peck and watch the hand, and leave
 * after a while; walking at them flushes them. Roosting birds flush too if the player walks up.
 *
 * Place the actor on the sea surface, in the middle of the area the flock should roam;
 * its rotation turns the area. The birds keep over water and well above land: the actor
 * samples the ground under and around the area with line traces when it starts.
 *
 * The starling is the low-poly model from Murmuration's tools/blender/starling.py, built in
 * code (no assets): nine wing-beat poses in flight and a perched one, standing.
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

	/** Latitude for the sun's times (sunrise, sunset, dusk), degrees north. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Murmuration|Day")
	float Latitude = 51.f;

	/** Clock time of the sun's highest point, hours. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Murmuration|Day")
	float SolarNoon = 12.f;

	/** >= 0: use this time of day (hours) instead of the game calendar (for tests). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Murmuration|Day")
	float ClockOverride = -1.f;

	/** Run the flock in the editor viewport too (not only in play). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Murmuration")
	bool bSimulateInEditor = false;

	UFUNCTION(BlueprintCallable, Category = "Murmuration")
	void Restart();

	/** Is the player holding out food? */
	UFUNCTION(BlueprintPure, Category = "Murmuration")
	bool IsFeeding() const { return bFeeding; }

	UFUNCTION(BlueprintCallable, Category = "Murmuration")
	void SetFeeding(bool bOn);

	/** Where the food is (the hero's open palm), world; until it is set the hand is guessed in front of the camera. */
	void SetFeedHand(const FVector& World) { FeedHand = World; bFeedHand = true; }

	/**
	 * Runs the flock headless for a while, logging it every LogEvery seconds (for tuning from
	 * scripts). StartHour >= 0 runs it on its own clock from that hour at HoursPerSecond.
	 */
	UFUNCTION(BlueprintCallable, Category = "Murmuration")
	void DebugRun(float Seconds, float StartHour = -1.f, float HoursPerSecond = 0.f, float LogEvery = 5.f);

	/**
	 * Headless: a person stands still at Feet (world X, Y, Z) holding out food for Seconds, then puts it
	 * away; from WalkAtBirdsAfter s on (>= 0) they walk at the birds on the ground for 4 s.
	 * NoticeRange >= 0 overrides Params.NoticeRange for the run.
	 */
	UFUNCTION(BlueprintCallable, Category = "Murmuration")
	void DebugFeed(float FeetX, float FeetY, float FeetZ, float Seconds, float WalkAtBirdsAfter = -1.f, float NoticeRange = -1.f);

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

	/** Birds standing on the ground or the roost. */
	UPROPERTY(VisibleAnywhere, Category = "Murmuration")
	TObjectPtr<UInstancedStaticMeshComponent> PerchedComponent;

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
	TArray<FTransform> PerchedTransforms;
	float ClockTimer = 0.f;
	float ViewerTimer = 0.f;
	bool bFeeding = false;
	FVector FeedHand = FVector::ZeroVector;
	bool bFeedHand = false;
	FVector LureFeet = FVector(1e30);
	float DebugHours = -1.f, DebugRate = 0.f;

	void EnsureMeshes();
	void UpdateClock(float DeltaSeconds, bool bForce);
	void UpdateViewer(bool bForce);
	void FeedViewer(const FVector& Feet, const FVector& Velocity, const FVector& Hand, bool bForce);
	TArray<FVector3f> MakeLureSpots(const FVector& Feet) const;
	float ProbeGround(float X, float Z) const;

	void SyncInstances(UInstancedStaticMeshComponent* ISM, TArray<FTransform>& Arr);
	void StartFlock();
	void SampleGround();
	void UpdateInstances(float Ahead);
	void LogStats() const;
};
