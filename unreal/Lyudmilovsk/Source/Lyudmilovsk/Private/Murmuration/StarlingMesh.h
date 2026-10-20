#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UMaterialInterface;

/**
 * The starling in flight from Murmuration (tools/blender/starling.py): body rings, square tail and
 * lofted wings with a wrist, built in code. Unreal axes, centimetres: beak +Y, up +Z, wings along X,
 * origin at the centre of mass. Body ~21.5 cm beak to tail, wingspan 38 cm.
 *
 * Beat is the wing stroke, sin(phase) * amplitude in -1..1 (the original's flap vertex shader);
 * Amp is the stroke amplitude 0..1.
 */
namespace StarlingMesh
{
	UStaticMesh* Build(UObject* Outer, FName Name, float Beat, float Amp, UMaterialInterface* Material);
}
