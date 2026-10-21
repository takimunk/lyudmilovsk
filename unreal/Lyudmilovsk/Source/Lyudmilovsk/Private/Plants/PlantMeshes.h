#pragma once

#include "CoreMinimal.h"
#include "MeshDescription.h"
#include "Plants/GrassSpecies.h"
#include "Plants/PlantModel.h"

class UStaticMesh;
class UTexture2D;

/** Everything a plant is drawn with, built in code from its species (cm, Z up). */
namespace PlantMeshes
{
	/**
	 * The tree's wood as one mesh: a tapered tube along every branch, its UVs in bark tiles (U around,
	 * V along). Rebuilds Mesh in place when given one. Vertex colour: R dark base, G young twig, B how
	 * much it sways. Species with bark Scars also get OutScarMask: the tree's own marks (chevrons over
	 * branches, shed-branch eyes, lenticels) in an atlas the mesh maps with UV channel 1.
	 */
	UStaticMesh* BuildWood(UObject* Outer, UStaticMesh* Mesh, const TArray<FPlantSkeleton::FTube>& Tubes, const FPlantBark& Bark, float Age = 0.f,
	                       uint32 Seed = 0, UTexture2D** OutScarMask = nullptr);

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

	/**
	 * A tuft of the grass's blades about the origin (cm, Z up), at the species' height: each blade a tapering,
	 * curved strip leaving the ground at Lean and arching over by Arch. Vertex colour R = how far up the
	 * blade (0 root .. 1 tip: sway and tint), G = a random per blade. Two-sided material.
	 */
	UStaticMesh* BuildGrassTuft(UObject* Outer, const FGrassSpecies& Grass);

	/** A whole tree frozen at one age and day as a static mesh with four LODs, for forests (slot 0 wood, slot 1 leaves). */
	struct FTreeBake
	{
		UStaticMesh* Mesh = nullptr;
		/** Bounding radius, m. */
		float Radius = 0.f;
		int32 Triangles[4] = {};
	};
	/**
	 * The tree AProceduralPlant would draw at (Age, Day), merged into one mesh: LOD0 every branch and leaf cluster,
	 * LOD1 limbs only and every 2nd cluster (larger), LOD2 every 5th, LOD3 trunk and every 14th. Switch distances
	 * are fractions of its size (FarLodDistances, m times Radius is not needed: pass the camera distances in
	 * LodDistances, m). The bark and leaf textures are not made here; the caller gives the materials.
	 */
	FTreeBake BuildTreeBake(UObject* Outer, const FPlantSpecies& Species, uint32 Seed, float Age, float Day, const FVector3f& LodDistances);

	/** The first, heavy half of BuildTreeBake: the four LODs as mesh descriptions. Touches no UObject: run it on any thread. */
	struct FTreePrep
	{
		TArray<FMeshDescription> Lods;
		float Radius = 0.f;
		int32 Triangles[4] = {};
		double Ms = 0.0;
	};
	FTreePrep PrepareTreeBake(const FPlantSpecies& Species, uint32 Seed, float Age, float Day);
	/** The second half, on the game thread: the static mesh from the descriptions. */
	FTreeBake FinishTreeBake(UObject* Outer, const FPlantSpecies& Species, uint32 Seed, FTreePrep&& Prep, const FVector3f& LodDistances);

	/** The alpha-masked texture of a foliage card (from LOD2 of a baked tree): a spray of the species' leaves, or its needles. */
	UTexture2D* BuildFoliageCard(UObject* Outer, const FPlantLeafShape& Leaf);

	/** Veins and margin of the species' leaf blade, a greyscale multiplier on the leaf colour. */
	UTexture2D* BuildLeafDetail(UObject* Outer, const FPlantLeafShape& Leaf);
}
