#pragma once

#include "CoreMinimal.h"

/**
 * The map's forest cover (tools/forest_cover.py -> Content/World/forest_cover.fcv): how much of a forest stays at a spot,
 * so the woods have natural edges, glades and thinning toward the tree line instead of the rectangles of the forest tiles.
 *
 * File: "FCV1", int32 nx, int32 ny, float32 x0, float32 y0 (map metres of cell 0,0's centre), float32 step (metres), then
 * nx * ny uint8, rows from y0 northward, each row from x0 toward map +X (west). Value / 255 = the share of trees kept.
 * Map frame (docs/world_map.md): world X cm = map X * 100, world Y cm = 790 + map Y * 100.
 *
 * Read once per process on first use (a few MB at most); no file = cover 1 everywhere (every tree kept). Outside the grid
 * the cover is 1. Game thread or any thread after the first call; the data never changes afterwards.
 */
namespace ForestCover
{
	/** 0..1 at a world location (cm), bilinear between cells. */
	LYUDMILOVSK_API float At(const FVector& WorldLocation);
	/** True when the file was found and read. */
	LYUDMILOVSK_API bool IsLoaded();
	/** Short state for logs: "forest_cover.fcv 640 x 1200 @ 8 m" or "no forest_cover.fcv". */
	LYUDMILOVSK_API FString Describe();
}
