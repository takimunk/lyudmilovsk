#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UMaterialInterface;

/** The two meshes every plant is instanced from, built in code (cm, Z up). */
namespace PlantMeshes
{
	/** A 7-sided open tube from radius 1 at z = 0 to radius 0.9 at z = 1: one bark segment, scaled (r, r, length). */
	UStaticMesh* BuildBark(UObject* Outer, UMaterialInterface* Material);
	/**
	 * A cluster of deltoid poplar leaves about the origin, 1 unit across, double-sided, with normals
	 * pointing out of the cluster so the crown shades softly.
	 */
	UStaticMesh* BuildLeafCluster(UObject* Outer, UMaterialInterface* Material);
}
