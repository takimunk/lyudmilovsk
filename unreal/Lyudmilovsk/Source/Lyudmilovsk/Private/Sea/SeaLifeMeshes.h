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
	/** Reef blocks of the moraine (Variant 0..2: a rounded erratic, a split slab, a heap of two), 1 m across. */
	UStaticMesh* ReefRock(int32 Variant);
	/** A patch of cobbles and pebbles, ~1.5 m across (Variant 0..2). */
	UStaticMesh* Cobbles(int32 Variant);
	UStaticMesh* Mussels();
	UStaticMesh* Shells();
	UStaticMesh* Flounder();
	UStaticMesh* Jellyfish();
	UStaticMesh* Fish();
	/** The length Fish() is built at, m (scale it by length / this). */
	constexpr float FishLength = 0.1f;
}
