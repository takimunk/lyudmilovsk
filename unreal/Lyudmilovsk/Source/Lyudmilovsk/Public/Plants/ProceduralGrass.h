#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Plants/GrassSpecies.h"
#include "ProceduralGrass.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;

/**
 * A rectangle of grass: tufts of curved blades scattered over the ground inside it, as instances of one
 * small mesh built in code from the species (FGrassSpecies). Nothing is saved but these attributes; the
 * tufts' places come from a hash of the world grid cell they stand in, so neighbouring patches line up
 * without a seam and every machine grows the same meadow.
 *
 * Cheap by construction: a tuft is ~80 triangles, no shadows, no collision, hierarchical instances so
 * the view frustum and distance cull whole clusters. Two rings: every tuft out to NearDistance, and
 * beyond it a quarter of them, drawn larger, out to FarDistance. Colour (green to straw), snow and the
 * winter height follow the calendar (UGameCalendar). Wind is in the material.
 *
 * Size a patch for ~20 000 tufts (60 m x 60 m at 5 per m2); tile bigger meadows with several.
 */
UCLASS()
class LYUDMILOVSK_API AProceduralGrass : public AActor
{
	GENERATED_BODY()

public:
	AProceduralGrass();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass", meta = (GetOptions = "GetSpeciesNames"))
	FName Species = TEXT("RedFescue");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass") int32 Seed = 1;
	/** Half the patch's width and depth, cm, about the actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass", meta = (ClampMin = "100")) FVector2D HalfExtent = FVector2D(2000.0, 2000.0);
	/** Multiplies the species' density (a thin edge, a lush hollow). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass", meta = (ClampMin = "0", ClampMax = "4")) float DensityScale = 1.f;
	/** Tufts are put on the ground found by traces down from above the actor; none where nothing is hit or it is steeper than this (surface normal Z). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass") bool bSnapToGround = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass", meta = (ClampMin = "0", ClampMax = "1")) float MinSlopeNormalZ = 0.7f;
	/** Only where the ground lies between these world heights, cm (sand below the dune line, forest floor above it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass") float MinWorldZ = -100000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass") float MaxWorldZ = 100000.f;
	/** Ground that belongs to an actor with this tag (a path, a beach, a yard) stays bare. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass") FName BareGroundTag = TEXT("NoGrass");
	/** How far above and below the actor the ground is searched, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass") float TraceUp = 3000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass") float TraceDown = 6000.f;

	/** Every tuft is drawn out to this distance from the camera, cm; beyond, a sparse ring of larger tufts out to FarDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass|Detail") float NearDistance = 3500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass|Detail") float FarDistance = 9000.f;
	/** Every Nth tuft is in the far ring, drawn FarScale times as large. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass|Detail", meta = (ClampMin = "2")) int32 FarEvery = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass|Detail") float FarScale = 1.6f;
	/** Show the patch in the editor viewport (off for big levels: it costs a few hundred ms per change). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass|Detail") bool bPreviewInEditor = true;

	/** Day of the year shown in the editor viewport; below 0 uses the calendar's start date. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass|Editor") float EditorPreviewDay = -1.f;
	/** Developer mode: hold the season at a day of the year (below 0: back to the calendar). */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Grass|Dev") float DevDay = -1.f;

	/** Throws the tufts away and lays them out again (after changing species, seed, size or the ground). */
	UFUNCTION(BlueprintCallable, Category = "Grass") void Rebuild();
	UFUNCTION(BlueprintCallable, Category = "Grass") int32 NumTufts() const { return Tufts; }
	UFUNCTION(BlueprintCallable, Category = "Grass") float CurrentDay() const;
	/** One line: species, tufts, triangles. */
	UFUNCTION(BlueprintCallable, Category = "Grass") FString DescribeState() const;
	UFUNCTION() TArray<FName> GetSpeciesNames() const;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	const FGrassSpecies& GetSpecies() const;
	void EnsureAssets();
	void Scatter();
	void ApplySeason(float Day);

	UPROPERTY(VisibleAnywhere, Category = "Grass") TObjectPtr<UInstancedStaticMeshComponent> NearTufts;
	UPROPERTY(VisibleAnywhere, Category = "Grass") TObjectPtr<UInstancedStaticMeshComponent> FarTufts;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> TuftMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> MaterialAsset;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Material;

	FName BuiltSpecies;
	int32 Tufts = 0;
	int32 TuftTriangles = 0;
	float ShownDay = -1000.f;
};
