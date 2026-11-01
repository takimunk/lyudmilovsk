#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "LyudUnderwater.generated.h"

class ALyudSea;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialParameterCollection;
class UStaticMesh;
class UStaticMeshComponent;
namespace LyudFish { class FSim; }
struct FLyudUnderwaterEnv;

/**
 * What draws the underwater life the subsystem simulates: one instanced mesh per fish species (the Blender-built
 * /Game/Sea/Fish/SM_<Id>, swimming in M_LyudFish by per-instance data: phase, beat, bend) and the motes drifting in the
 * water round the eye (one static mesh of quads, placed and carried entirely in M_LyudSuspended). Spawned at play by
 * ULyudUnderwaterSubsystem; nothing of it is saved in a level.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudSeaLife : public AActor
{
	GENERATED_BODY()

public:
	ALyudSeaLife();

	/** The species' instanced mesh, made on first use (null when the species has no mesh and no stand-in). */
	UInstancedStaticMeshComponent* SpeciesMesh(int32 Species);
	UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Fish;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Motes;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> MoteMesh;
	/** Species whose own mesh was missing (they swim as the old code-built stand-in). */
	TArray<FName> StandIns;
};

/**
 * The underwater world's life (docs/underwater.md): the currents (LyudSeaCurrent: the tidal stream along the coast and
 * over the flat, the wind's longshore current, eddies over the reefs, slower toward the bed), fed to the materials through
 * /Game/Sea/Underwater/MPC_LyudUnderwater so weed leans and motes drift with them; and the fish (LyudFish::FSim: sixteen
 * Baltic species in schools, shoals, hunters and bottom dwellers, by habitat, month and light) round the eye.
 *
 * It only works while the eye is under the water or close over it; away from the sea everything is freed.
 * lyud.SeaLife 0 turns it off, lyud.SeaLife.Fish 0 only the fish, lyud.SeaLife.Density scales the fish,
 * lyud.SeaLife.Debug 1 prints the state every second, lyud.Current.Speed forces the current (m/s, along the coast).
 */
UCLASS()
class LYUDMILOVSK_API ULyudUnderwaterSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudUnderwaterSubsystem();
	virtual ~ULyudUnderwaterSubsystem() override;

	static ULyudUnderwaterSubsystem* Get(const UObject* WorldContext);
	UFUNCTION(BlueprintPure, Category = "Underwater", meta = (WorldContext = "WorldContext"))
	static ULyudUnderwaterSubsystem* GetUnderwater(const UObject* WorldContext) { return Get(WorldContext); }

	/** The water's motion at a point, cm/s: the mean current with its eddies (no wave orbits: ALyudSea::SampleSurface). */
	UFUNCTION(BlueprintCallable, Category = "Underwater") FVector CurrentAt(const FVector& Location) const;
	/** The mean current at a point, cm/s (what fish hold against and weed leans with). */
	UFUNCTION(BlueprintCallable, Category = "Underwater") FVector MeanCurrentAt(const FVector& Location) const;
	/** Is the life round the eye running (the eye under or close over the water)? */
	UFUNCTION(BlueprintCallable, Category = "Underwater") bool IsActive() const { return bActive; }
	/** Fish of a species (its Id: Herring, Cod, Flounder...) round the eye now, and where the nearest one is (From if none). */
	UFUNCTION(BlueprintCallable, Category = "Underwater") int32 GetFishCount(FName Species) const;
	UFUNCTION(BlueprintCallable, Category = "Underwater") FVector FindNearestFish(FName Species, const FVector& From) const;
	/** The species there are (Ids). */
	UFUNCTION(BlueprintCallable, Category = "Underwater") TArray<FName> GetSpecies() const;
	UFUNCTION(BlueprintCallable, Category = "Underwater") FString GetDebugReport() const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	friend struct FLyudUnderwaterEnv;
	void UpdateDrive(ALyudSea* Sea, float Dt);
	void UpdateCollection(ALyudSea* Sea, const FVector& Eye, bool bUnder, float Dt);
	void DrawFish(float Dt);
	void Release();
	FVector CurrentCm(const FVector& Location, bool bEddies) const;

	TWeakObjectPtr<ALyudSea> SeaRef;
	TWeakObjectPtr<ALyudSeaLife> Life;
	UPROPERTY(Transient) TObjectPtr<UMaterialParameterCollection> Collection;
	TPimplPtr<LyudFish::FSim> Sim;
	TPimplPtr<FLyudUnderwaterEnv> Env;
	/** The current's drive: the sea-state wind, the tide's rate (m/s) and springs, a clock. */
	float WindX = 0.f, WindY = 0.f, TideRate = 0.f, Spring = 0.5f;
	float LastTideCm = 0.f;
	bool bTideSeen = false;
	double Clock = 0.0;
	FVector Drift = FVector::ZeroVector;
	FVector EyeCurrent = FVector::ZeroVector;
	bool bActive = false;
	float DebugTimer = 0.f;
	/** Per species: the transforms and the shader's data written last (reused, no allocations per frame). */
	TArray<TArray<FTransform>> Xf;
	TArray<TArray<float>> Data;
};
