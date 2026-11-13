#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "LyudUnderwater.generated.h"

class ALyudSea;
class ALyudSky;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialParameterCollection;
class UStaticMesh;
class UStaticMeshComponent;
namespace LyudFish { class FSim; }
struct FLyudUnderwaterEnv;

/** Something round the diver was eaten: the predator's and the prey's species Ids (Cod, Herring, GreySeal...), where (cm). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FLyudFishEaten, FName, Predator, FName, Prey, FVector, Location);
/** A hunting moment round the diver: a strike, caught or missed (a pike's lunge, a cormorant's chase, a cod at a school). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FLyudFishHunt, FName, Predator, FName, Prey, FVector, Location, bool, bCaught);

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

	/** The species' instanced mesh, made on first use once its look is in memory (null until then: it loads in the background). */
	UInstancedStaticMeshComponent* SpeciesMesh(int32 Species);
	UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Fish;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Motes;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> MoteMesh;
	/** Species whose own mesh was missing (they swim as the old code-built stand-in). */
	TArray<FName> StandIns;
	/** The length each species' mesh was built at, m (its own: the species' length; the stand-in: 0.1). */
	TArray<float> MeshLength;
};

/**
 * The underwater world's life (docs/underwater.md): the currents (LyudSeaCurrent: the tidal stream along the coast and
 * over the flat, the wind's longshore current, eddies over the reefs, slower toward the bed), fed to the materials through
 * /Game/Sea/Underwater/MPC_LyudUnderwater so weed leans and motes drift with them; and the fish (LyudFish::FSim: the
 * Baltic's fish, crustaceans, mysids and grey seals in schools, shoals, hunters and bottom dwellers, by habitat, month and
 * light) round the eye, in the numbers the coastal food web (ULyudEcosystem) gives, eating what it says they eat.
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
	/** Takes the nearest fish of a species within Reach (cm) of a point out of the water round the eye (a hook, a net): false if none. */
	UFUNCTION(BlueprintCallable, Category = "Underwater") bool TakeFish(FName Species, const FVector& Location, float Reach = 100.f);

	/** A predator round the diver caught something (a cod a herring, a sculpin a shrimp, a seal a flounder). */
	UPROPERTY(BlueprintAssignable, Category = "Underwater") FLyudFishEaten OnFishEaten;
	/** Every strike seen, caught or missed (OnFishEaten has only the catches). */
	UPROPERTY(BlueprintAssignable, Category = "Underwater") FLyudFishHunt OnFishHunt;
	/** Meals seen since play began, "Predator<-Prey" -> count (lyud.SeaLife.Debug prints them). */
	TMap<FString, int32> MealsSeen;

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
	/** Away from the water the life is only hidden for a while (AwayTime), then freed: a pier's edge no longer rebuilds it. */
	float AwayTime = 0.f;
	/** The collection was last written far from the water (it is left alone there). */
	bool bCollectionFar = false;
	/** The sky (sun altitude, shown time), found once. */
	TWeakObjectPtr<ALyudSky> SkyRef;
	ALyudSky* FindSky();
	float DebugTimer = 0.f;
	float EcoSyncTimer = 0.f;
	bool bDietSet = false;
	/** Per species: the transforms and the shader's data written last (reused, no allocations per frame). */
	TArray<TArray<FTransform>> Xf;
	TArray<TArray<float>> Data;
};
