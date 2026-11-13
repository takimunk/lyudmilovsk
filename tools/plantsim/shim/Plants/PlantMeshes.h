#pragma once
// tools/plantsim: only what PlantModel.cpp and the herb builder call (the real file builds meshes; plantsim.cpp copies these)
#include "CoreMinimal.h"
#include "Plants/PlantModel.h"
namespace PlantMeshes
{
	int32 BlossomTriangles(const FPlantBlossom& Blossom);
}
