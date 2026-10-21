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
	UStaticMesh* BuildWood(UObject* Outer, UStaticMesh* Mesh, const TArray<FPlantSkeleton::FTube>& Tubes, const FPlantBark& Bark, float Age = 0.f);

	/**
	 * A cluster of the species' leaves about the origin (a short twig along Z, blades facing up),
	 * real size, one-sided (the material is two-sided).
	 */
	UStaticMesh* BuildLeafCluster(UObject* Outer, const FPlantSpecies& Species);

	/** Bark colour (sRGB) and normal map tiles for the species' pattern; tileable both ways. */
	void BuildBarkTextures(UObject* Outer, const FPlantSpecies& Species, uint32 Seed, UTexture2D*& OutAlbedo, UTexture2D*& OutNormal);

	/** One leaf, centred on its blade: falling leaves and the litter under the tree. */
	UStaticMesh* BuildSingleLeaf(UObject* Outer, const FPlantSpecies& Species);
	/** The species' fruit, 100 units across like the engine sphere (an apple, or an acorn when it never blushes). */
	UStaticMesh* BuildFruit(UObject* Outer, const FPlantSpecies& Species);
	/** Noise (R) and lenticel dots (G) for the fruit skin. */
	UTexture2D* BuildFruitDetail(UObject* Outer);

	/** Veins and margin of the species' leaf blade, a greyscale multiplier on the leaf colour. */
	UTexture2D* BuildLeafDetail(UObject* Outer, const FPlantLeafShape& Leaf);
}
