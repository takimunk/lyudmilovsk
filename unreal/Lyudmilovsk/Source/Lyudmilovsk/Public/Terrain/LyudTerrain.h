#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LyudTerrain.generated.h"

class ALandscape;
class ULandscapeLayerInfoObject;
class UMaterialInterface;

/**
 * Where grass grows on the outdoor ground, baked by the terrain build (ULyudTerrainLibrary::BuildTerrain)
 * on a 2 m grid over the whole map: meadow grass (thin and absent on bare patches, tracks, rock and sand)
 * and dune grass (on the dry upper beach). Readable at run time, so AProceduralGrass can ask it per tuft.
 *
 * The build saves it as /Game/Scene01/Terrain/DA_TerrainMask (Get() finds it).
 */
UCLASS(BlueprintType)
class LYUDMILOVSK_API ULyudTerrainMask : public UDataAsset
{
	GENERATED_BODY()

public:
	/** World X/Y (cm) of cell (0, 0)'s centre, the cell size (cm) and the grid size. */
	UPROPERTY(VisibleAnywhere, Category = "Terrain") FVector2D Origin = FVector2D::ZeroVector;
	UPROPERTY(VisibleAnywhere, Category = "Terrain") float CellSize = 200.f;
	UPROPERTY(VisibleAnywhere, Category = "Terrain") int32 Width = 0;
	UPROPERTY(VisibleAnywhere, Category = "Terrain") int32 Height = 0;
	UPROPERTY() TArray<uint8> Grass;
	UPROPERTY() TArray<uint8> DuneGrass;
	UPROPERTY() TArray<uint8> Wetness;
	UPROPERTY() TArray<uint8> ForestFloor;

	/** 0..1, bilinear; 0 outside the map. */
	UFUNCTION(BlueprintPure, Category = "Terrain") float GetGrassDensity(const FVector& WorldLocation) const { return Sample(Grass, WorldLocation); }
	UFUNCTION(BlueprintPure, Category = "Terrain") float GetDuneGrassDensity(const FVector& WorldLocation) const { return Sample(DuneGrass, WorldLocation); }
	/** 0 dry .. 1 where water runs and stands (gullies, hollows, ruts). */
	UFUNCTION(BlueprintPure, Category = "Terrain") float GetWetness(const FVector& WorldLocation) const { return Sample(Wetness, WorldLocation); }
	/** 0..1 forest floor the ground cover grows on (AForestFloor): under the forest, off its tracks, rock and sand. */
	UFUNCTION(BlueprintPure, Category = "Terrain") float GetForestFloor(const FVector& WorldLocation) const { return Sample(ForestFloor, WorldLocation); }
	/** A mask built before the forest floor channel existed has none (AForestFloor then goes by the trees alone). */
	bool HasForestFloor() const { return ForestFloor.Num() == Width * Height && Width > 1; }

	/** The level's mask, or null if the terrain has not been built. */
	UFUNCTION(BlueprintPure, Category = "Terrain") static ULyudTerrainMask* Get();

private:
	float Sample(const TArray<uint8>& Data, const FVector& WorldLocation) const;
};

/** Builds the outdoor ground (tools/terrain.py writes the inputs, unreal/scripts/import_terrain.py drives this). */
UCLASS()
class LYUDMILOVSK_API ULyudTerrainLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	/** A landscape paint layer's info object, created in a new package at PackagePath (save it from the caller). */
	UFUNCTION(BlueprintCallable, Category = "Terrain|Editor", meta = (DevelopmentOnly))
	static ULandscapeLayerInfoObject* CreateLayerInfo(const FString& PackagePath, FName LayerName);

	/** Surface layer names in the generator's order (Grass, Meadow, Dirt, Mud, Sand, Rock, Forest). */
	UFUNCTION(BlueprintCallable, Category = "Terrain|Editor", meta = (DevelopmentOnly))
	static TArray<FName> GetLayerNames();

	/**
	 * Generates the ground from InputDir/terrain_in.* (noise, erosion, tracks, layers), writes
	 * InputDir/terrain_out.bin, spawns a landscape (1 m grid, vertex (0, 0) at Origin, cm) with the
	 * material, the layers (Layers in GetLayerNames' order) and the portal hole, and fills Mask.
	 * Returns the landscape (null on failure); Report says what was made.
	 */
	UFUNCTION(BlueprintCallable, Category = "Terrain|Editor", meta = (DevelopmentOnly, WorldContext = "WorldContextObject"))
	static ALandscape* BuildTerrain(UObject* WorldContextObject, const FString& InputDir, FVector Origin, UMaterialInterface* Material,
	                                const TArray<ULandscapeLayerInfoObject*>& Layers, ULyudTerrainMask* Mask, FString& Report);

	/** A new, empty mask asset at PackagePath (filled by BuildTerrain, saved by the caller). */
	UFUNCTION(BlueprintCallable, Category = "Terrain|Editor", meta = (DevelopmentOnly))
	static ULyudTerrainMask* CreateMask(const FString& PackagePath);
#endif
};
