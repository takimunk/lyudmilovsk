#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudFestivalTown.generated.h"

class AProceduralForest;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * The whole town dressed for a holiday (festivals.json decor kind "street_bunting", docs/festivals.md): strings of
 * pennants zigzag across every lane at the foot of the terrace walls, from each street lamp's head to the wall's parapet
 * and on to the next lamp, swag along the promenade's arcade and cross between neighbouring houses. Read from
 * Content/City/city_plan.json (lamps, walls, plots), built once in code in chunks along the coast (each culls on its
 * own); the pennants move in the vertex shader with the day's wind (M_FestivalCloth), nothing ticks.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudStreetBunting : public AActor
{
	GENERATED_BODY()

public:
	ALyudStreetBunting();
	/** Builds the town's strings (a few ms per chunk, all at once: called the evening before, out of sight). */
	void Build(uint32 Seed);
	int32 NumStrings() const { return Strings; }
	int32 NumPennants() const { return Pennants; }

private:
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> ClothMat;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RopeMat;
	int32 Strings = 0, Pennants = 0;
};

/**
 * Kites over the beach (the show "kites": the Big Sun day's afternoon, the kite day): a dozen diamonds, boxes and deltas
 * on their lines, standing downwind of the people who fly them, climbing in the gusts and sagging in a lull, their tails
 * streaming (M_FestivalCloth). Each kite is one movable component and a line, moved every frame near the camera and
 * twice a second far from it; no shadows, no collision, out of Lumen.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudKites : public AActor
{
	GENERATED_BODY()

public:
	ALyudKites();
	/** Flown from round At (the beach, cm) till EndsAt (calendar seconds); SeaDir: out to sea. */
	void Setup(const FVector& At, const FVector& InSeaDir, float Radius, double InEndsAt);
	/** No more flying: the kites come down and the actor goes. */
	void StopShow() { EndsAt = 0.0; }
	virtual void Tick(float DeltaTime) override;
	int32 NumKites() const { return Kites.Num(); }

private:
	struct FKite
	{
		FVector Hand = FVector::ZeroVector;     // where its flier stands
		float Line = 4000.f;                    // cm of line out
		float Phase = 0.f, Up = 0.f;            // how far up it is, 0 on the sand .. 1 flying
		FVector At = FVector::ZeroVector;
	};
	TArray<FKite> Kites;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Bodies;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Lines;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> Meshes;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> LineMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> ClothMat;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> LineMat;
	double EndsAt = 0.0;
	float Clock = 0.f;
	FVector SeaDir = FVector(1.0, 0.0, 0.0);
};

/**
 * Poplar fluff (docs/festivals.md "Poplar fluff"): from the end of May to the end of June the town's poplars shed their
 * down, and on a dry day with a little wind the air is full of it: thickest by the poplars, a few tufts anywhere in town,
 * none in the rain. One mesh of tufts lives in a 32 m box round the camera and drifts with the wind's own drift in its
 * material (M_PoplarFluff); this subsystem only works out how much there is (every half second) and keeps the box on the
 * camera. It also plants the town's own black poplars (Content/City/poplars.json, tools/town_poplars.py) through the
 * procedural plant system, as one forest of hand-placed trees.
 * Console: lyud.Fluff 0 off; lyud.Fluff.Amount 0..1 forces the amount (negative: the season's); lyud.Fluff.Report.
 */
UCLASS()
class LYUDMILOVSK_API ULyudPoplarFluffSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudPoplarFluffSubsystem* Get(const UObject* WorldContext);

	/** How much fluff is in the air round the camera now, 0..1. */
	UFUNCTION(BlueprintPure, Category = "Fluff") float GetAmount() const { return Amount; }
	/** The season alone, 0..1, for a day of the year (0 outside late May .. early July). */
	UFUNCTION(BlueprintPure, Category = "Fluff") static float SeasonAt(float DayOfYear);
	UFUNCTION(BlueprintCallable, Category = "Fluff") FString Report() const;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bRunning; }

private:
	bool bRunning = false;
	float Timer = 0.f;
	float Amount = 0.f, Target = 0.f, Season = 0.f, Near = 0.f;
	/** The poplars of every forest in the level (looked for a few seconds in, once their layout is done), cm. */
	TArray<FVector2D> Poplars;
	int32 PoplarLooks = 0;
	float PoplarTimer = 4.f;
	UPROPERTY(Transient) TObjectPtr<AActor> Holder;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Tufts;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> TuftMat;
	UPROPERTY(Transient) TObjectPtr<AProceduralForest> TownPoplars;
	bool bMeshTried = false;
	void PlantTownPoplars(UWorld& World);
	void FindPoplars();
	void MakeMesh();
	void UpdateAmount();
};
