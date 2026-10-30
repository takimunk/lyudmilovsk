#pragma once

#include "CoreMinimal.h"
#include "Plants/PlantSpecies.h"

class UStaticMesh;
class UTexture2D;

/**
 * Herbs and house plants (FPlantSpecies::Herb): a rosette, a bush of stems, trailing stems or a ribbed column, with
 * their flowers, generated as one mesh of three sections (leaves, stems, flowers), and their pots. Like the trees,
 * the plant is a pure function of (species, seed, age, day): the same leaf is always the same leaf, it only grows.
 */
namespace PlantHerbs
{
	enum ESlot : int32 { Leaves = 0, Stems = 1, Flowers = 2, NumSlots = 3 };

	/** Vertices (cm) and triangles per section. */
	struct FHerbGeometry
	{
		TArray<FVector3f> Pos, Nrm, Tan;
		TArray<FVector2f> UV;
		TArray<FIntVector3> Tris[NumSlots];
		int32 Add(const FVector3f& P, const FVector3f& N, const FVector3f& T, const FVector2f& U)
		{
			Nrm.Add(N); Tan.Add(T); UV.Add(U);
			return Pos.Add(P);
		}
		int32 NumTriangles() const { return Tris[0].Num() + Tris[1].Num() + Tris[2].Num(); }
	};

	/** Where the plant is in its life and its year. */
	struct FState
	{
		/** 0 a seedling .. 1 grown. */
		float Maturity = 1.f;
		/** Above ground at all (an annual before sowing or after frost, a perennial in winter: false). */
		bool bAlive = true;
		/** Leaf size by the season (unfolding in spring) and the share of its leaves still on. */
		float LeafScale = 1.f;
		float Present = 1.f;
		/** Share of its flowers open. */
		float Bloom = 0.f;
		/** Killed by frost: 0 .. 1 browned. */
		float Wither = 0.f;
		/** The leaves' colour (season), when the texture does not carry it. */
		FLinearColor LeafColour = FLinearColor::Green;
		/** Changes whenever the mesh would. */
		uint32 Key = 0;
	};

	FState State(const FPlantSpecies& S, uint32 Seed, float Age, float Day);

	/** The plant at that state; OutHeight / OutRadius: its size, m. */
	void Build(const FPlantSpecies& S, uint32 Seed, const FState& St, FHerbGeometry& Out, float& OutHeight, float& OutRadius);

	/** A mesh of the three sections (rebuilt in place when given one). */
	UStaticMesh* BuildMesh(UObject* Outer, UStaticMesh* Mesh, FName Name, const FHerbGeometry& G);

	/** The leaves' texture: true colours (variegation, a cactus's areoles) when the species has a pattern, else null. */
	UTexture2D* BuildLeafTexture(UObject* Outer, const FPlantSpecies& S);
	/** True when BuildLeafTexture paints the colours (the material's colour then stays white). */
	bool LeafTextureCarriesColour(const FPlantSpecies& S);
	/** Four colour variants of the species' petals and their centre, side by side. */
	UTexture2D* BuildFlowerAtlas(UObject* Outer, const FPlantSpecies& S);

	/** A flower pot Diameter cm across at the rim: section 0 the pot, 1 the soil; the soil's top is at PotSoilZ. */
	UStaticMesh* BuildPot(UObject* Outer, float Diameter);
	float PotSoilZ(float Diameter);

	/** The presets of flowers, hedges and house plants (PlantSpeciesHerbs.cpp), in the species library after the trees. */
	TArray<FPlantSpecies> Species();
	/** The forest floor's dwarf shrubs, ferns and wood sorrel (PlantSpeciesFloor.cpp, drawn by AForestFloor). */
	TArray<FPlantSpecies> FloorSpecies();
}
