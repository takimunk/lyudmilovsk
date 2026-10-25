#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "LyudSeabed.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/**
 * Life on the Baltic seabed around the camera, as it is off the Sambian coast: bare rippled sand with
 * shells in the surf zone; eelgrass meadows (Zostera marina) on sand from 1.5 m down; boulder fields
 * (glacial erratics) carrying bladderwrack (Fucus vesiculosus) in the shallows, red Furcellaria deeper
 * and clusters of blue mussels at their feet; flounders lying on the sand that dart off when the
 * camera comes close; a school of small silver fish (sprats and sticklebacks) wheeling about; and in
 * autumn moon jellyfish (Aurelia aurita) drifting with the waves.
 *
 * Everything is placed from a hash of the world tile it stands in, so it is the same every time and
 * needs nothing saved, and is only built within Radius of the camera (the Baltic's water hides the rest:
 * a few metres of visibility). Meshes are made in code (Private/Sea/SeaLifeMeshes.cpp); the weed and the
 * jellyfish sway in their materials with the sea's dominant wave (MPC_LyudWeather), the fish wriggle.
 *
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
	/** How many moon jellyfish and schooling fish swim around the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seabed") int32 Jellyfish = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seabed") int32 SchoolFish = 48;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seabed|Look") TObjectPtr<UMaterialInterface> WeedMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seabed|Look") TObjectPtr<UMaterialInterface> RockMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seabed|Look") TObjectPtr<UMaterialInterface> JellyMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seabed|Look") TObjectPtr<UMaterialInterface> FishMaterial;

	/** How many of each thing are placed now (for tests): eelgrass, fucus, furcellaria, boulders, mussels, shells, flounders, jellyfish, fish. */
	UFUNCTION(BlueprintCallable, Category = "Seabed") FString GetDebugReport() const;
	UFUNCTION(BlueprintCallable, Category = "Seabed") int32 GetInstanceCount() const;
	/** Where the nearest thing of a kind (0 eelgrass, 1 wrack, 2 red weed, 3 boulder, 4 mussels, 5 shells, 6 flounder, 7 jellyfish, 8 fish) is, or From when there is none. */
	UFUNCTION(BlueprintCallable, Category = "Seabed") FVector FindNearest(int32 Kind, const FVector& From) const;

	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	enum EKind : int32 { Eelgrass, Fucus, Furcellaria, Boulder, Mussels, Shells, Flounder, Jelly, Fish, NumKinds };

	struct FSwimmer
	{
		FVector Pos = FVector::ZeroVector;
		FVector Vel = FVector::ZeroVector;
		float Phase = 0.f;
		float Scale = 1.f;
		float Seed = 0.f;
	};

	struct FFlounder
	{
		FVector Rest = FVector::ZeroVector;
		FVector Pos = FVector::ZeroVector;
		float Yaw = 0.f;
		float Scale = 1.f;
		float Fleeing = 0.f;
		FVector Target = FVector::ZeroVector;
	};

	void EnsureComponents();
	void Rebuild(const FVector& Eye);
	void UpdateSwimmers(const FVector& Eye, float Dt);
	float GroundZ(double X, double Y, bool& bOk) const;

	UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Kinds;
	FIntPoint BuiltTile = FIntPoint(INT32_MAX, INT32_MAX);
	int32 BuiltQuality = -2;
	float AppliedRadius = 0.f;
	TArray<FSwimmer> Jellies;
	TArray<FSwimmer> School;
	FVector SchoolCenter = FVector::ZeroVector;
	FVector SchoolGoal = FVector::ZeroVector;
	float SchoolClock = 0.f;
	TArray<FFlounder> Flounders;
	bool bSwimmersPlaced = false;
	double Clock = 0.0;
};
