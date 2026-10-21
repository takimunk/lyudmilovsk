#pragma once

#include "CoreMinimal.h"
#include "Plants/PlantModel.h"

class UStaticMesh;
class UTexture2D;

/** Everything a plant is drawn with, built in code from its species (cm, Z up). */
namespace PlantMeshes
{
	/**
	 * The tree's wood as one mesh: a tapered tube along every branch, its UVs in bark tiles (U around,
	 * V along). Rebuilds Mesh in place when given one.
	 */
	UStaticMesh* BuildWood(UObject* Outer, UStaticMesh* Mesh, const TArray<FPlantSkeleton::FTube>& Tubes, const FPlantBark& Bark);

	/**
	 * A cluster of the species' leaves about the origin (a short twig along Z, blades facing up),
	 * real size, one-sided (the material is two-sided).
	 */
	UStaticMesh* BuildLeafCluster(UObject* Outer, const FPlantSpecies& Species);

	/** Bark colour (sRGB) and normal map tiles for the species' pattern; tileable both ways. */
	void BuildBarkTextures(UObject* Outer, const FPlantSpecies& Species, uint32 Seed, UTexture2D*& OutAlbedo, UTexture2D*& OutNormal);

	/** Veins and margin of the species' leaf blade, a greyscale multiplier on the leaf colour. */
	UTexture2D* BuildLeafDetail(UObject* Outer, const FPlantLeafShape& Leaf);
}
