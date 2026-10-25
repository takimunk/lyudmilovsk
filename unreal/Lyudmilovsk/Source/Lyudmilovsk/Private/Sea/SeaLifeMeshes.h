#pragma once

#include "CoreMinimal.h"

class UStaticMesh;

/**
 * The seabed's meshes, made in code (cm, Z up, standing on the origin). Vertex colour carries what the
 * materials move: R = how far along the plant from its holdfast (sway) or along the fish from its head
 * (wriggle) or out to the jellyfish's rim (pulse); G = a per-part shade; B = 1 on special parts (the
 * wrack's air bladders, the jellyfish's four rings).
 */
namespace LyudSeaLife
{
	UStaticMesh* Eelgrass();
	UStaticMesh* Fucus();
	UStaticMesh* Furcellaria();
	UStaticMesh* Boulder();
	UStaticMesh* Mussels();
	UStaticMesh* Shells();
	UStaticMesh* Flounder();
	UStaticMesh* Jellyfish();
	UStaticMesh* Fish();
}
