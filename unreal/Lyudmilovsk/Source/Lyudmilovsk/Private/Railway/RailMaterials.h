// The sea railway's materials for what it builds in code (the bank, the rails, the platforms, the tunnel): the cable car's
// masters (M_TR_Paint: Color, Roughness, Metallic; M_TR_Surface: a texture tiled in metres; M_TR_Glow; M_TR_Glass)
// with the railway's own colours, one instance per name, kept by the Owner.
#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;

namespace LyudRailMat
{
	/** A material for Name: a palette paint ("ballast", "stone", "rail", "wood", ...), "tx_<surface>" a textured one
	 *  (concrete, steel, tread: the cable car's textures), "glow_warm" / "glow_red" / "glow_white" emissive, "glass". */
	UMaterialInterface* Get(UObject* Owner, FName Name, TMap<FName, TObjectPtr<UMaterialInterface>>& Cache);
	/** The kit's (rail_kit.glb) mesh SM_RL_<Name> or null. */
	class UStaticMesh* KitMesh(const TCHAR* Name);
	/** An emissive instance to drive (Color, Intensity), on the cable car's glow master. */
	UMaterialInstanceDynamic* Glow(UObject* Owner, const FLinearColor& Color, float Intensity);
}
