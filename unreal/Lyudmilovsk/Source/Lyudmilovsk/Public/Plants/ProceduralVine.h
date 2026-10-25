#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Plants/VineSpecies.h"
#include "ProceduralVine.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture2D;

/**
 * A climbing vine (Virginia creeper) rooted at the actor, at the foot of a wall its +X faces out of.
 *
 * It creeps over whatever static collision is there, a step at a time, the way the plant does: each
 * shoot clings to the surface (a trace down onto it), turns up a wall it runs into, goes round an outer
 * corner, slides along the edge of a window or door instead of crossing the hole, and where there is
 * nothing to hold (a roof edge) hangs free for a little. Glass is too smooth to hold. Shoots wander,
 * branch, and keep clear of wall already covered, so the plant spreads until the wall is full.
 *
 * Like AProceduralPlant it is a pure function of (Species, Seed, its age at the calendar's start) and
 * the in-game date, given the walls: shoots only lengthen, in the growing season (late May to August);
 * leaves follow the species' seasons (PlantModel::Season): bronze in spring, crimson and scarlet in
 * autumn, gone in winter, when only the stems and the berries are left.
 *
 * The growth is simulated in time order, only as far as the plant's age needs, a few milliseconds a
 * frame (TraceBudgetMs), so a newly loaded wall costs no hitch.
 */
UCLASS()
class LYUDMILOVSK_API AProceduralVine : public AActor
{
	GENERATED_BODY()

public:
	AProceduralVine();
	virtual ~AProceduralVine() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vine", meta = (GetOptions = "GetSpeciesNames"))
	FName Species = TEXT("VirginiaCreeper");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vine") int32 Seed = 1;
	/** Its age on the calendar's start date, years. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vine", meta = (ClampMin = "0")) float AgeAtStart = 12.f;

	/** Time-lapse: grow from AgeAtStart to DemoTargetAge over DemoSeconds of the game clock, the seasons kept. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vine|Demo") bool bDemoGrowth = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vine|Demo") float DemoSeconds = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vine|Demo") float DemoTargetAge = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vine|Demo") float DemoDelay = 3.f;

	/** Developer: hold the vine at an age / day of the year (below 0: back to the calendar). Console: Vine.ShowAt. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Vine|Dev") float DevAge = -1.f;
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Vine|Dev") float DevDay = -1.f;

	/** Milliseconds of growth simulation (wall traces) a frame at most. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vine|Detail") float TraceBudgetMs = 2.f;
	/** Leaves and berries are not drawn beyond this, cm; the stems beyond FarWood. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vine|Detail") float LeafCullDistance = 30000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vine|Detail") float FarWood = 15000.f;
	/** Most steps one plant may grow (a cap on its cost). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vine|Detail") int32 MaxNodes = 9000;

	UFUNCTION(BlueprintCallable, Category = "Vine") float GetAge() const;
	/** Years of growth it has had by now: whole seasons, plus how far into this one. */
	UFUNCTION(BlueprintCallable, Category = "Vine") float GetGrowth() const;
	UFUNCTION(BlueprintCallable, Category = "Vine") float CurrentDay() const;
	/** Re-grow from scratch (after moving it, changing species or seed). */
	UFUNCTION(BlueprintCallable, Category = "Vine") void Regrow();
	/** Grows the whole plant now, up to an age, ignoring the frame budget (tests, editor). */
	UFUNCTION(BlueprintCallable, Category = "Vine") void GrowNow(float Age);
	/** One line: age, growth, stems, leaves, berries, wall covered, simulation cost. */
	UFUNCTION(BlueprintCallable, Category = "Vine") FString DescribeState() const;
	UFUNCTION() TArray<FName> GetSpeciesNames() const;

	/** Years of growth at an age and day of the year: nothing grows from late August to late May. */
	static float GrowthAt(const FVineSpecies& S, float Age, float DayOfYear);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

private:
	struct FNode
	{
		/** On the surface (world cm) and the surface's normal; a free node hangs in the air. */
		FVector P;
		FVector N;
		/** Growth years when the shoot reached it. */
		float T = 0.f;
		int32 Parent = INDEX_NONE;
		int32 Shoot = 0;
		bool bFree = false;
	};
	struct FShoot
	{
		int32 Order = 0;
		int32 Parent = INDEX_NONE;
		/** The node it buds from (its parent's), then its own. */
		TArray<int32> Nodes;
	};
	struct FTip
	{
		int32 Shoot = 0;
		int32 Node = 0;
		FVector Dir;
		float T = 0.f;
		float Len = 0.f;
		float MaxLen = 0.f;
		float NextBranch = 0.f;
		float FreeLen = 0.f;
		/** How far it has run over level ground in a row (a ledge, a roof), m. */
		float FlatRun = 0.f;
		int32 Branches = 0;
		int32 Deflects = 0;
		bool bFree = false;
	};
	enum class EMove : uint8 { Ok, Opening, Edge };

	const FVineSpecies& GetSpecies() const;
	void ResetGrowth();
	void StartGrowth();
	/** Grows every shoot up to growth year G, or until the budget (seconds) runs out; true when done. */
	bool SimulateUntil(float G, double BudgetSeconds);
	void StepTip(FTip Tip);
	EMove MoveOnSurface(const FVector& P, const FVector& N, const FVector& D, float StepCm, FVector& OutP, FVector& OutN, FVector& OutD, bool& bOutWrapped) const;
	bool TraceWall(const FVector& From, const FVector& To, FHitResult& Hit) const;
	bool Crowded(const FVector& P, int32 Shoot, int32 ParentShoot) const;
	void Occupy(const FVector& P, int32 Shoot);
	static FIntVector CellOf(const FVector& P, float Cell) { return FIntVector(FMath::FloorToInt(P.X / Cell), FMath::FloorToInt(P.Y / Cell), FMath::FloorToInt(P.Z / Cell)); }

	void EnsureMeshes();
	/** Draws the plant at growth year G on day Day; wood only when bWood. */
	void Apply(float G, float Day, bool bWood);
	void CurrentTarget(float& OutAge, float& OutDay) const;
	/** Growth years to draw at (Age, Day): GrowthAt, or the age itself in a time-lapse. */
	float TargetGrowth(float Age, float Day) const;

	UPROPERTY(VisibleAnywhere, Category = "Vine") TObjectPtr<UStaticMeshComponent> Wood;
	UPROPERTY(VisibleAnywhere, Category = "Vine") TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> LeafComponents;
	UPROPERTY(VisibleAnywhere, Category = "Vine") TObjectPtr<UInstancedStaticMeshComponent> Berries;

	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BarkMaterialAsset;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> LeafMaterialAsset;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BaseMaterial;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> WoodMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> LeafMesh;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> BarkAlbedo;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> BarkNormal;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> LeafDetail;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> WoodMaterial;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> LeafMaterials;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BerryMaterial;
	/** Translucent surfaces found while growing (glass): never held. */
	mutable TSet<TWeakObjectPtr<const UPrimitiveComponent>> Slippery;

	TArray<FNode> Nodes;
	TArray<FShoot> Shoots;
	/** Growing tips, a min-heap on T. */
	TArray<FTip> Tips;
	/** Wall cells taken: cell -> shoot. */
	TMap<FIntVector, int32> Taken;
	float SimulatedTo = -1.f;
	FVector RootP = FVector::ZeroVector;
	FVector RootN = FVector::XAxisVector;
	FName BuiltSpecies;
	int32 BuiltSeed = 0;
	FTransform BuiltAt;
	double SimSeconds = 0.0;
	int32 SimTraces = 0;
	mutable int32 TraceCount = 0;
	int32 Wraps = 0, Slides = 0, Hangs = 0;

	float ShownG = -1.f, ShownDay = -1000.f, WoodG = -1.f;
	float SinceUpdate = 0.f;
	bool bShownByCVar = true;
	int32 LastLeaves = 0, LastBerries = 0, LastLiveNodes = 0;
	float LastArea = 0.f;
};
