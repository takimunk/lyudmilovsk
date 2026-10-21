#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyudSave.h"
#include "Plants/PlantModel.h"
#include "ProceduralPlant.generated.h"

class AFallenFruit;
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
class LYUDMILOVSK_API AProceduralPlant : public AActor, public ILyudSaveable
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
	/**
	 * Time-lapse of the calendar for this plant only: in-game days added per second of play (0 = the
	 * real calendar). 0.5 shows a month of leaf fall and dropping apples in a minute.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plant|Demo") float DemoDaysPerSecond = 0.f;
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
	/** How close the player must be to pick up a fallen fruit, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plant") float PickDistance = 250.f;
	/** Fruit picked up from under this tree, all time. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Plant") int32 FruitPicked = 0;
	/** Put the plant on the ground under it when play begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plant") bool bSnapToGround = true;

	UFUNCTION(BlueprintCallable, Category = "Plant") float GetAge() const;
	UFUNCTION(BlueprintCallable, Category = "Plant") FPlantSummary GetSummary() const;
	/** Shows the plant at an age and day of the year (for previews and tests); the next tick goes back to the calendar. */
	UFUNCTION(BlueprintCallable, Category = "Plant") void ShowAt(float Age, float DayOfYear, bool bSummary = false);
	/** Branch and leaf counts and sizes at an age, against the level-0 summary. */
	UFUNCTION(BlueprintCallable, Category = "Plant") FString DebugReport(float Age, float DayOfYear);
	UFUNCTION() TArray<FName> GetSpeciesNames() const;
	/** The calendar's date as this plant sees it (with DemoDaysPerSecond). */
	UFUNCTION(BlueprintCallable, Category = "Plant") FDateTime CurrentDate() const;
	UFUNCTION(BlueprintCallable, Category = "Plant") float CurrentDay() const;

	/** Developer mode: hold the plant at an age / day of the year (below 0: back to the calendar). */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Plant|Dev") float DevAge = -1.f;
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Plant|Dev") float DevDay = -1.f;
	/** Re-shape now (wood included), e.g. after changing species, seed or a developer override. */
	UFUNCTION(BlueprintCallable, Category = "Plant") void Refresh();
	/** One line of what is drawn now: size, leaves, litter, fruit. */
	UFUNCTION(BlueprintCallable, Category = "Plant") FString DescribeState() const;

	virtual void OnGameLoaded_Implementation() override;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	const FPlantSpecies& GetSpecies() const;
	void EnsureMeshes();
	void Apply(float Age, float Day, bool bSummary, bool bWood = true);
	bool WantsSummary() const;
	void SyncFallenFruit(const FPlantSkeleton::FInstances& Out);
	void UpdateLitter(const FPlantSkeleton::FInstances& Out, const FPlantSeason& Season);
	void SpawnFallingLeaves(const FPlantSkeleton::FInstances& Out);
	void TickFallingLeaves(float DeltaSeconds);
	/** Height of the ground under a point of the plant's space (cm, relative to the plant), from a grid of traces. */
	float GroundAt(float X, float Y);
	void ClearFallenFruit();
	void OnInteract();
	/** The fallen fruit of this tree the player looks at, within PickDistance; null if none. */
	AFallenFruit* AimedFruit() const;
	bool bPickPrompt = false;

	UPROPERTY(VisibleAnywhere, Category = "Plant") TObjectPtr<UStaticMeshComponent> Wood;
	UPROPERTY(VisibleAnywhere, Category = "Plant") TArray<TObjectPtr<UInstancedStaticMeshComponent>> LeafComponents;
	UPROPERTY(VisibleAnywhere, Category = "Plant") TObjectPtr<UInstancedStaticMeshComponent> FruitComponent;
	/** Fallen leaves on the ground, and leaves in the air, per colour bucket. */
	UPROPERTY(VisibleAnywhere, Category = "Plant") TArray<TObjectPtr<UInstancedStaticMeshComponent>> LitterComponents;
	UPROPERTY(VisibleAnywhere, Category = "Plant") TArray<TObjectPtr<UInstancedStaticMeshComponent>> FallingComponents;
	UPROPERTY(VisibleAnywhere, Category = "Plant") TObjectPtr<UStaticMeshComponent> SummaryTrunk;
	UPROPERTY(VisibleAnywhere, Category = "Plant") TObjectPtr<UStaticMeshComponent> SummaryCrown;
	UPROPERTY(VisibleAnywhere, Category = "Plant") TObjectPtr<UCapsuleComponent> TrunkCollision;

	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BaseMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BarkMaterialAsset;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> LeafMaterialAsset;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FruitMaterialAsset;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> SingleLeafMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> FruitMesh;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> FruitDetail;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> LitterMaterials;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> WoodMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> LeafMesh;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> BarkAlbedo;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> BarkNormal;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> ScarMask;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> LeafDetail;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> WoodMaterial;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> LeafMaterials;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FruitMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CrownMaterial;

	/** Keys (year * 100000 + twig) of fruit picked up, so it does not come back after a load. */
	UPROPERTY(SaveGame) TArray<int32> PickedFruit;
	TMap<int32, TWeakObjectPtr<AFallenFruit>> GroundFruit;
	/** Fruit drawn on the tree now (keys), with its transforms and skins (FruitDataFloats each). */
	TArray<int32> HangingKeys;
	TArray<FTransform> HangingXf;
	TArray<float> HangingData;
	/** Fruit whose day to drop has come but that still hangs, waiting its turn to fall. */
	struct FPendingFruit { int32 Key; FPlantSkeleton::FInstances::FFallen F; };
	TArray<FPendingFruit> PendingFruit;
	float FruitReleaseRate = 0.f, FruitReleaseAcc = 0.f;
	double LastFruitTime = -1.0;
	void DrawHangingFruit();
	void DropFruit(const FPendingFruit& P);

	struct FFallingLeaf
	{
		FVector P;
		FRotator Rot, Spin;
		FVector2D Target;
		float GroundZ = 0.f, Speed = 90.f, Phase = 0.f, Size = 1.f;
		int32 Bucket = 0;
	};
	TArray<FFallingLeaf> Falling;
	/** Leaves that let go at the last re-shape, released into the air one by one until the next. */
	TArray<FPlantSkeleton::FInstances::FDrop> PendingDrops;
	float ReleaseRate = 0.f, ReleaseAcc = 0.f;
	/** Leaves that have landed since the litter was last laid out (it includes them from then on). */
	TArray<FTransform> Landed[FPlantSeason::NumBuckets];
	double LastDropTime = -1.0;
	bool bLitterDirty = false;
	bool bLandedChanged = false;
	FPlantSeason LastSeason;
	float LastSeasonDay = -1000.f;
	int32 LastLeaves = 0, LastLitter = 0, LastFruit = 0;
	float LastHeight = 0.f, LastReach = 0.f;

	/** Ground heights around the trunk (cm, relative to the plant), GroundN x GroundN cells of GroundCell cm. */
	TArray<float> GroundGrid;
	int32 GroundN = 0;
	float GroundCell = 100.f;

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
