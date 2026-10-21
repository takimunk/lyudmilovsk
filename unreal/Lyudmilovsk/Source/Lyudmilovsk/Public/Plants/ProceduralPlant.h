#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Plants/PlantModel.h"
#include "ProceduralPlant.generated.h"

class UCapsuleComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture2D;

/**
 * One plant, entirely described by (Species, Seed, its age at the calendar's start). Its shape at
 * any moment is a pure function of those and the in-game date (UGameCalendar), so nothing is saved
 * but these attributes and every machine grows the same tree.
 *
 * Near the camera it draws its branches and leaves (instanced); beyond SummaryDistance only the
 * level-0 summary: a trunk and a crown of the right height and leaf colour.
 */
UCLASS()
class LYUDMILOVSK_API AProceduralPlant : public AActor
{
	GENERATED_BODY()

public:
	AProceduralPlant();
	virtual ~AProceduralPlant() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plant", meta = (GetOptions = "GetSpeciesNames"))
	FName Species = TEXT("PyramidalPoplar");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plant") int32 Seed = 1;
	/** Its age on the calendar's start date, years; it was born that long before. 1 = a sprout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plant", meta = (ClampMin = "0")) float AgeAtStart = 1.f;

	/**
	 * Time-lapse: grow from AgeAtStart to DemoTargetAge over the first DemoSeconds of the game clock
	 * (so it pauses and loads with the game) while the leaves keep the calendar's date.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plant|Demo") bool bDemoGrowth = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plant|Demo", meta = (EditCondition = "bDemoGrowth")) float DemoSeconds = 120.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plant|Demo", meta = (EditCondition = "bDemoGrowth")) float DemoTargetAge = 30.f;
	/** Game-clock seconds before the time-lapse starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plant|Demo", meta = (EditCondition = "bDemoGrowth")) float DemoDelay = 3.f;

	/** Age shown in the editor viewport; below 0 shows AgeAtStart. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plant|Editor") float EditorPreviewAge = -1.f;
	/** Day of the year shown in the editor viewport; below 0 uses the calendar's start date. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plant|Editor") float EditorPreviewDay = -1.f;

	/** Beyond this camera distance only the level-0 summary is drawn, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plant|Detail") float SummaryDistance = 15000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plant|Detail") bool bForceSummary = false;
	/** Seconds between re-shapes of the leaves while it is visibly growing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plant|Detail") float UpdateInterval = 0.2f;
	/** Seconds between rebuilds of the wood mesh while it grows (the costlier part). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plant|Detail") float WoodUpdateInterval = 0.5f;
	/** Put the plant on the ground under it when play begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plant") bool bSnapToGround = true;

	UFUNCTION(BlueprintCallable, Category = "Plant") float GetAge() const;
	UFUNCTION(BlueprintCallable, Category = "Plant") FPlantSummary GetSummary() const;
	/** Shows the plant at an age and day of the year (for previews and tests); the next tick goes back to the calendar. */
	UFUNCTION(BlueprintCallable, Category = "Plant") void ShowAt(float Age, float DayOfYear, bool bSummary = false);
	/** Branch and leaf counts and sizes at an age, against the level-0 summary. */
	UFUNCTION(BlueprintCallable, Category = "Plant") FString DebugReport(float Age, float DayOfYear);
	UFUNCTION() TArray<FName> GetSpeciesNames() const;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	const FPlantSpecies& GetSpecies() const;
	float CurrentDay() const;
	void EnsureMeshes();
	void Apply(float Age, float Day, bool bSummary, bool bWood = true);
	bool WantsSummary() const;

	UPROPERTY(VisibleAnywhere, Category = "Plant") TObjectPtr<UStaticMeshComponent> Wood;
	UPROPERTY(VisibleAnywhere, Category = "Plant") TArray<TObjectPtr<UInstancedStaticMeshComponent>> LeafComponents;
	UPROPERTY(VisibleAnywhere, Category = "Plant") TObjectPtr<UInstancedStaticMeshComponent> FruitComponent;
	UPROPERTY(VisibleAnywhere, Category = "Plant") TObjectPtr<UStaticMeshComponent> SummaryTrunk;
	UPROPERTY(VisibleAnywhere, Category = "Plant") TObjectPtr<UStaticMeshComponent> SummaryCrown;
	UPROPERTY(VisibleAnywhere, Category = "Plant") TObjectPtr<UCapsuleComponent> TrunkCollision;

	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BaseMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BarkMaterialAsset;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> LeafMaterialAsset;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> WoodMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> LeafMesh;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> BarkAlbedo;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> BarkNormal;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> LeafDetail;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> WoodMaterial;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> LeafMaterials;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FruitMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CrownMaterial;

	FPlantSkeleton Skeleton;
	FName BuiltSpecies;
	int32 BuiltSeed = 0;
	float ShownAge = -1.f;
	float WoodAge = -1.f;
	float SinceWood = 0.f;
	float ShownDay = -1.f;
	bool bShownSummary = false;
	float SinceUpdate = 0.f;
};
