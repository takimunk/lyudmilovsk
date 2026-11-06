#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "LyudSeabed.generated.h"

class ALyudSky;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;

/**
 * Life on the Baltic seabed around the camera, as it is off the Sambian coast (docs/underwater.md): rippled sand with shells;
 * eelgrass meadows (Zostera marina) on sand 1.5-7.5 m deep; the stones of the moraine reefs and the boulder and cobble
 * fields (the terrain mask's SeaStone, TerrainGen's SeabedDetail) carrying bladderwrack (Fucus) in the shallows with green
 * Cladophora tufts and sea lettuce in the first metres, the red Furcellaria deeper, a few oarweed kelps on the deepest reef
 * blocks, blue mussels in the crevices; moon jellyfish drifting with the waves. The weeds are the plant system's alga
 * species (PlantAlga.cpp: grown for the calendar's day, a few seeds of each), drawn in M_LyudSeaAlga, which leans them with
 * the current and sways them with the waves. The fish are ULyudUnderwaterSubsystem's.
 *
 * Everything is placed from a hash of the 8 m world tile it stands in (the same every time, nothing saved) within Radius of
 * the camera, and only while the camera is under or close over the water (the Baltic's water hides the rest).
 * lyud.Seabed 0 turns it off; lyud.Seabed.Radius sets the radius (m); it follows lyud.Sea.Quality.
 */
UCLASS(ClassGroup = (Lyudmilovsk), meta = (BlueprintSpawnableComponent))
class LYUDMILOVSK_API ULyudSeabedComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	ULyudSeabedComponent();

	/** Life is built out to this far from the camera, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seabed") float Radius = 4000.f;
	/** Side of the tiles life is placed by, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seabed") float TileSize = 800.f;
	/** Multiplies how much grows (0.5 sparse .. 2 lush). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seabed") float Density = 1.f;
	/** How many moon jellyfish drift around the camera (in their months). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seabed") int32 Jellyfish = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seabed|Look") TObjectPtr<UMaterialInterface> AlgaMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seabed|Look") TObjectPtr<UMaterialInterface> RockMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seabed|Look") TObjectPtr<UMaterialInterface> JellyMaterial;

	/** How many of each thing are placed now (for tests). */
	UFUNCTION(BlueprintCallable, Category = "Seabed") FString GetDebugReport() const;
	UFUNCTION(BlueprintCallable, Category = "Seabed") int32 GetInstanceCount() const;
	/**
	 * Where the nearest thing of a kind is, or From when there is none: 0 eelgrass, 1 bladderwrack, 2 Furcellaria, 3 boulder,
	 * 4 mussels, 5 shells, 6 Cladophora, 7 jellyfish, 8 sea lettuce, 9 oarweed, 10 reef rock, 11 cobbles.
	 */
	UFUNCTION(BlueprintCallable, Category = "Seabed") FVector FindNearest(int32 Kind, const FVector& From) const;

	/**
	 * The bed's habitat at a world point (cm) with Depth m of water at the lowest tide: sand, stones (moraine reefs, boulder
	 * and cobble fields: the terrain mask) and weed (eelgrass meadows on sand, wrack on the shallow stones), 0..1 each. The
	 * fish settle by it; the same function places the weeds and stones here.
	 */
	static void HabitatAt(double X, double Y, float Depth, float& Sand, float& Stone, float& Weed);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	enum EKind : int32 { Eelgrass, Bladderwrack, Furcellaria, Boulder, Mussels, Shells, Cladophora, Jelly, SeaLettuce, Oarweed, ReefRock, Cobbles, NumKinds };
	/** Seeds (plants) or shapes (rocks) of each kind, each its own instanced mesh. */
	static constexpr int32 Variants = 3;

	struct FSwimmer
	{
		FVector Pos = FVector::ZeroVector;
		float Phase = 0.f;
		float Scale = 1.f;
		float Seed = 0.f;
	};

	void EnsureComponents();
	/** Grows the plant kinds' meshes for the calendar's day (again when their state changes). */
	void UpdatePlants(bool bForce);
	void Rebuild(const FVector& Eye);
	void Clear();
	void UpdateJellies(const FVector& Eye, float Dt);
	float GroundZ(double X, double Y, bool& bOk) const;
	UInstancedStaticMeshComponent* Mesh(int32 Kind, int32 Variant) const { return Kinds.IsValidIndex(Kind * Variants + Variant) ? Kinds[Kind * Variants + Variant].Get() : nullptr; }

	UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Kinds;
	/** The plant meshes grown in code (freed when regrown). */
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> PlantMeshes;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> PlantMIDs;
	TArray<uint32> PlantKeys;
	float PlantClock = 0.f;
	FIntPoint BuiltTile = FIntPoint(INT32_MAX, INT32_MAX);
	int32 BuiltQuality = -2;
	float AppliedRadius = 0.f;
	bool bBuilt = false;
	TArray<FSwimmer> Jellies;
	bool bJelliesPlaced = false;
	double Clock = 0.0;
	/** Away from the water: hidden at once, freed only after a while (a wave or a pier's edge no longer rebuilds it). */
	bool bAwayHidden = false;
	float AwayTime = 0.f;
	/** The sky for the day (found once, not every frame). */
	TWeakObjectPtr<ALyudSky> SkyRef;
};
