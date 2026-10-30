#pragma once

#include "CoreMinimal.h"

class UStaticMesh;

/**
 * The forest floor's dead and mineral things, built in code (cm, Z up, the origin where it meets the ground): stones
 * and boulders, sticks, fallen branches, logs, stumps, pine and spruce cones, curls of birch bark, moss cushions and
 * the roots round an old trunk. One section each, shaded by M_FloorDebris.
 *
 * Vertex colour: R = bare wood (broken ends, a stump's cut, the inside of a bark curl), G = a random per part,
 * B = how much it faces the sky before it is laid (moss and snow grow there), A = 1. UVs: U around and V along in
 * real metres (wood, cones), so a bark texture is never stretched; stones and moss are textured in object space.
 */
namespace ForestFloorMeshes
{
	struct FGeom
	{
		TArray<FVector3f> Pos, Nrm, Tan;
		TArray<FVector2f> UV;
		TArray<FVector4f> Col;
		TArray<FIntVector3> Tris;
		int32 Add(const FVector3f& P, const FVector3f& N, const FVector3f& T, const FVector2f& U, const FVector4f& C)
		{
			Nrm.Add(N); Tan.Add(T); UV.Add(U); Col.Add(C);
			return Pos.Add(P);
		}
		void Append(const FGeom& O);
		/** Highest point over the origin, cm (how deep snow must be to bury it). */
		float Top() const;
	};

	FGeom Stone(uint32 Seed, float Radius, float Flatness, int32 Detail);
	/** A dead stick or branch lying along X: a bent, tapering tube with Forks short side twigs, broken ends. */
	FGeom Stick(uint32 Seed, float Length, float Radius, int32 Forks, int32 Sides);
	/** A fallen trunk along X, half its radius sunk, with stubs of its limbs and jagged broken ends. */
	FGeom Log(uint32 Seed, float Length, float Radius);
	/** A stump with its root flare and a weathered, cracked top. */
	FGeom Stump(uint32 Seed, float Radius, float Height);
	/** A cone lying on its side along X: Length, max Radius; Open 0 closed (spruce) .. 1 scales spread (an old pine cone). */
	FGeom Cone(uint32 Seed, float Length, float Radius, float Open);
	/** A curl of birch bark, rolled up from both edges. */
	FGeom BarkCurl(uint32 Seed, float Size);
	/** A lumpy cushion of moss over a rotten stump or a stone. */
	FGeom Cushion(uint32 Seed, float Radius, float Height);
	/** Roots spreading from a trunk of radius TrunkR into the ground round the origin (the trunk stands on it). */
	FGeom Roots(uint32 Seed, float TrunkR, int32 Count);

	/**
	 * Checks a mesh for what keeps reaching the game (docs/forest_floor.md, "Checking"): degenerate triangles, UVs
	 * stretched more than 3x against the mesh's own texel density (where UVs are in metres), triangles of
	 * different parts lying in one plane on top of each other (z-fighting), NaNs. Returns a one-line report;
	 * OutProblems counts the faults (0 = clean).
	 */
	FString Lint(const FGeom& G, bool bCheckUVs, int32& OutProblems);

	/** A static mesh with one LOD per geometry (LOD screen sizes as given), never saved; collision from LOD 0 when asked. */
	UStaticMesh* Build(UObject* Outer, FName Name, const TArray<const FGeom*>& Lods, const TArray<float>& ScreenSizes, bool bCollision);
}
