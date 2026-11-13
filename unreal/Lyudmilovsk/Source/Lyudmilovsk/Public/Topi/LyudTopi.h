#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "LyudTopi.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class UPointLightComponent;
class ULyudTopiMind;
class ULyudTopiProps;
namespace LyudTopi { class FModel; }

/**
 * Topi: the dune land east of the town (docs/topi.md). The map's landscape ends at X -1300; from there on the
 * ground is this actor's: endless sand dunes from the plain C++ model (Private/Topi/TopiModel), drawn as a quadtree of
 * chunks round the camera (2 m cells underfoot, coarser and coarser out to the horizon ~40 km off; only the finest
 * chunks near the player collide). Chunks are built on worker threads and each is built once: nothing is rebuilt per
 * frame. Far dunes heave slowly in the material (M_TopiSand, world position offset past ~600 m), sand streams over the
 * surface with the wind.
 *
 * The river Ural lies in the dunes ~960 m in: a ribbon of light that shows only at night (Night()), with a curtain of
 * glow over it seen from far off. The hero's side of it all (the pressure on his mind, turning back, going into the
 * river and waking at the computer) is ULyudTopiMind; signs and towels are ULyudTopiProps.
 *
 * Spawned at play by ULyudTopiSubsystem in any world with the open sky. lyud.Topi 0 hides it (A/B benchmarks),
 * lyud.Topi.Night pins the night (0..1; -1 = by the sun), lyud.Topi.Debug 1 prints its state.
 */
UCLASS()
class LYUDMILOVSK_API ALyudTopi : public AActor
{
	GENERATED_BODY()

public:
	ALyudTopi();
	virtual ~ALyudTopi() override;

	/** The world's Topi, or null (no open sky, or before the first tick). */
	static ALyudTopi* Get(const UObject* WorldContext);

	/** The land's model (map metres: X west, Y north, Z up over the club's floor). */
	const LyudTopi::FModel& Model() const;
	/** World cm <-> map metres. */
	FVector2D ToMap(const FVector& World) const { return FVector2D(World.X / 100.0, (World.Y - PortalFaceY) / 100.0); }
	FVector ToWorld(double MapX, double MapY, double MapZ) const { return FVector(MapX * 100.0, PortalFaceY + MapY * 100.0, MapZ * 100.0); }

	/** World Z of Topi's sand under a point, cm (west of the sand's edge this is under the map's own ground). */
	UFUNCTION(BlueprintCallable, Category = "Topi") float SandZ(const FVector& World) const;
	/** Metres into Topi at a point (0 before it starts). */
	UFUNCTION(BlueprintCallable, Category = "Topi") float DepthInto(const FVector& World) const;

	/** 0 by day .. 1 in full night: how much of the river there is now. */
	UFUNCTION(BlueprintCallable, Category = "Topi") float Night() const { return NightNow; }
	/** Water depth of the river at a point now, cm (0: no water here, or it is day). */
	UFUNCTION(BlueprintCallable, Category = "Topi") float RiverDepthAt(const FVector& World) const;
	/** The river's stream at a point, cm/s (zero outside the water). */
	UFUNCTION(BlueprintCallable, Category = "Topi") FVector RiverFlowAt(const FVector& World) const;
	/** World Z of the river's surface, cm. */
	UFUNCTION(BlueprintCallable, Category = "Topi") float RiverSurfaceZ() const;

	/** The hero's state of mind 0..1 (ULyudTopiMind sets it): the far dunes grow restless with it. */
	UFUNCTION(BlueprintCallable, Category = "Topi") void SetUnrest(float Pressure) { Unrest = FMath::Clamp(Pressure, 0.f, 1.f); }
	/** How far the hero has dissolved in the river, 0..1 (ULyudTopiMind sets it): the river's light floods everything. */
	UFUNCTION(BlueprintCallable, Category = "Topi") void SetDissolve(float Amount) { Dissolve = FMath::Clamp(Amount, 0.f, 1.f); }

	/** Sand (unreal/scripts/make_topi_materials.py). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topi|Look") TObjectPtr<UMaterialInterface> SandMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topi|Look") TObjectPtr<UMaterialInterface> RiverMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topi|Look") TObjectPtr<UMaterialInterface> GlowMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topi|Look") TObjectPtr<UMaterialInterface> MotesMaterial;

	/** How far the ground is drawn, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topi|Detail") float HorizonM = 40000.f;
	/** A chunk splits into four when the camera is nearer than this many of its sizes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topi|Detail", meta = (ClampMin = "1.2", ClampMax = "4")) float SplitDistance = 1.9f;
	/** The finest chunk's size, m (64 cells a side: 128 m gives 2 m cells). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topi|Detail") float FinestChunkM = 128.f;
	/** Chunks kept after they leave the view before their meshes are freed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topi|Detail") int32 SpareChunks = 24;
	/** Marram grass on the fore dunes and round the edge (AProceduralGrass patches on the finest chunks). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topi|Detail") bool bGrass = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topi|Detail") FName GrassSpecies = TEXT("MarramGrass");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Topi") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Topi") TObjectPtr<UProceduralMeshComponent> River;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Topi") TObjectPtr<UProceduralMeshComponent> Glow;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Topi") TObjectPtr<UProceduralMeshComponent> Motes;
	/** The river's light on the sand and on the hero where the path comes out. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Topi") TArray<TObjectPtr<UPointLightComponent>> RiverLights;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Topi") TObjectPtr<ULyudTopiMind> Mind;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Topi") TObjectPtr<ULyudTopiProps> Props;

	virtual void Tick(float DeltaSeconds) override;
	FString DebugReport() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	struct FState;
	TPimplPtr<FState> State;

	void UpdateChunks(const FVector& Eye);
	bool ResolveChunk(int32 Level, int32 IX, int32 IY, const FVector2D& EyeMap, TArray<uint64>& OutShow);
	void FinishBuilt(double BudgetMs);
	void DropChunk(uint64 Key);
	void GrowGrass(uint64 Key);
	void BuildRiver();
	void UpdateLook(float DeltaSeconds, const FVector& Eye);
	void HoldHero();

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SandMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RiverMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> GlowMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> MotesMID;

	double PortalFaceY = 790.0;
	float NightNow = 0.f;
	float Unrest = 0.f;
	float UnrestShown = 0.f;
	float Dissolve = 0.f;
	bool bHidden = false;
};

/** Puts Topi into any game world that has the open sky. */
UCLASS()
class LYUDMILOVSK_API ULyudTopiSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudTopiSubsystem, STATGROUP_Tickables); }
	ALyudTopi* GetTopi() const { return Topi.Get(); }

private:
	TWeakObjectPtr<ALyudTopi> Topi;
	bool bLooked = false;
};
