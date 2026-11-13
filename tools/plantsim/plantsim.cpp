// The plant model outside the engine (tools/plantsim/run.sh): what a species costs to draw. For trees and shrubs the
// triangles of the four forest-bake LODs, counted the way PlantMeshes::PrepareTreeBake builds them (wood tubes, leaf
// clusters, cards, blossom); for herbs the two LODs of an AProceduralGrass herb patch (PlantHerbs::Build of the species and
// of PlantHerbs::LiteSpecies). The counting below mirrors PlantMeshes.cpp (BuildWoodImpl's sides per radius,
// LeafClusterData, BuildBlade's simple blade, BlossomTriangles); keep it in step when those change.
#include "Plants/PlantHerb.h"
#include "Plants/PlantMeshes.h"
#include "Plants/PlantModel.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

int32 PlantMeshes::BlossomTriangles(const FPlantBlossom& B)
{
	const int32 N = FMath::Max(B.PerCluster, 1);
	switch (B.Form)
	{
	case EPlantBlossomForm::Catkin: return N * 50;
	case EPlantBlossomForm::Blossom: return N * (FMath::Max(B.Petals, 3) * 2 + 8 + 2);
	case EPlantBlossomForm::Cone: return N * 16;
	default: return 0;
	}
}

namespace
{
	/** Triangles of the wood of the tubes up to MaxOrder (PlantMeshes BuildWoodImpl: sides by the base radius). */
	int32 WoodTriangles(const TArray<FPlantSkeleton::FTube>& Tubes, int32 MaxOrder)
	{
		int32 Tris = 0;
		for (const FPlantSkeleton::FTube& T : Tubes)
		{
			if (T.Order > MaxOrder || T.P.Num() < 2) continue;
			const float R0 = T.R[0];
			const int32 Sides = R0 > 0.12f ? 14 : R0 > 0.04f ? 10 : R0 > 0.012f ? 6 : R0 > 0.005f ? 4 : 3;
			Tris += 2 * (T.P.Num() - 1) * Sides;
		}
		return Tris;
	}

	/** Triangles of one leaf cluster (PlantMeshes LeafClusterData; bSimple: BuildBlade's far blade). */
	int32 ClusterTriangles(const FPlantSpecies& S, bool bSimple)
	{
		const FPlantLeafShape& L = S.Leaf;
		if (L.Form == EPlantLeafForm::Needle) return 2 * FMath::Clamp(L.Cards, 1, 8);
		int32 Blade = 0;
		if (L.Form == EPlantLeafForm::Palmate)
		{
			Blade = bSimple ? 10 : 48;
		}
		else
		{
			const bool bTeeth = L.Teeth > 0 && L.Teeth <= 8;
			const int32 N = bSimple ? 2 : (L.Lobes > 0 ? L.Lobes * 2 + 4 : (bTeeth ? L.Teeth * 4 : 8));
			Blade = 4 * N;
		}
		return FMath::Max(L.PerCluster, 1) * (2 + Blade);
	}

	struct FBakeCost
	{
		int32 Tris[4] = {};
		int32 Tubes = 0, Clusters = 0, Blossoms = 0, Fruit = 0;
		float Height = 0.f, Reach = 0.f;
		bool bNaN = false;
	};

	/** The forest bake of (species, seed, age, day), counted as PlantMeshes::PrepareTreeBake builds it. */
	FBakeCost BakeCost(const FPlantSpecies& S, uint32 Seed, float Age, float Day)
	{
		FBakeCost C;
		FPlantSkeleton Skeleton;
		Skeleton.Build(S, Seed);
		FPlantSkeleton::FInstances Out;
		Skeleton.Evaluate(Age, PlantModel::Season(S, Seed, Day), Out);
		const bool bNeedle = S.Leaf.Form == EPlantLeafForm::Needle;
		struct FLod { int32 MaxOrder; int32 Stride; };
		const FLod Lods[4] = { { 3, bNeedle ? 1 : 2 }, { 1, bNeedle ? 3 : 6 }, { 1, 6 }, { 1, 20 } };
		int32 Clusters = 0;
		for (int32 K = 0; K < FPlantSeason::NumBuckets; K++) Clusters += Out.Leaves[K].Num();
		const int32 Bloom = S.Blossom.Form != EPlantBlossomForm::None ? PlantMeshes::BlossomTriangles(S.Blossom) : 0;
		for (int32 L = 0; L < 4; L++)
		{
			int32 Kept = 0;
			for (int32 I = 0; I < Clusters; I++) Kept += I % Lods[L].Stride == 0 ? 1 : 0;
			const int32 PerCluster = L >= 2 ? 6 : ClusterTriangles(S, !bNeedle);
			int32 Flowers = 0;
			if (L < 2) for (int32 B = 0; B < Out.Blossoms.Num(); B += (L == 0 ? 1 : 3)) Flowers++;
			C.Tris[L] = FMath::Max(1, WoodTriangles(Out.Tubes, Lods[L].MaxOrder) + Kept * PerCluster + Flowers * Bloom);
		}
		C.Tubes = Out.Tubes.Num();
		C.Clusters = Clusters;
		C.Blossoms = Out.Blossoms.Num();
		C.Fruit = Out.Fruit.Num();
		C.Height = Out.Height;
		C.Reach = Out.Reach;
		for (const FPlantSkeleton::FTube& T : Out.Tubes)
			for (const FVector3f& P : T.P) C.bNaN |= P.ContainsNaN();
		return C;
	}

	void HerbCost(const FPlantSpecies& S, uint32 Seed, float Age, float Day, int32& OutTris, int32& OutLite, float& OutHeight, float& OutRadius, bool& bNaN)
	{
		PlantHerbs::FHerbGeometry G0, G1;
		const PlantHerbs::FState St = PlantHerbs::State(S, Seed, Age, Day);
		PlantHerbs::Build(S, Seed, St, G0, OutHeight, OutRadius);
		const FPlantSpecies L = PlantHerbs::LiteSpecies(S);
		float H = 0.f, R = 0.f;
		PlantHerbs::Build(L, Seed, PlantHerbs::State(L, Seed, Age, Day), G1, H, R);
		OutTris = G0.NumTriangles();
		OutLite = G1.NumTriangles();
		bNaN = false;
		for (const FVector3f& P : G0.Pos) bNaN |= P.ContainsNaN();
	}

	const char* DayName(float Day)
	{
		return Day < 60.f ? "winter" : Day < 165.f ? "spring" : Day < 250.f ? "summer" : "autumn";
	}

	bool Report(const FPlantSpecies& S, bool bBrief, float OnlyAge, float OnlyDay)
	{
		bool bOk = true;
		const std::string N = *S.Name.ToString();
		if (S.Herb.Form == EPlantHerbForm::None && S.bIndoor)
		{
			// the potted trees grow in AProceduralPlant's pot (never baked): not modelled here
			std::printf("%-20s potted tree: skipped\n", N.c_str());
			return true;
		}
		if (S.Herb.Form == EPlantHerbForm::None)
		{
			// a forest's ages (north.py FOREST_MIX young/mature, and old) and the season's looks
			const float Ages[3] = { 8.f, 30.f, 80.f };
			const float Days[4] = { 30.f, 160.f, 200.f, 285.f };
			for (float Age0 : Ages)
			{
				const float Age = OnlyAge > 0.f ? OnlyAge : Age0;
				if (OnlyAge > 0.f && Age0 != Ages[0]) continue;
				for (float Day0 : Days)
				{
					const float Day = OnlyDay > 0.f ? OnlyDay : Day0;
					if (OnlyDay > 0.f && Day0 != Days[0]) continue;
					if (bBrief && Day != 200.f) continue;
					FBakeCost C1 = BakeCost(S, 1, Age, Day), C2 = BakeCost(S, 2, Age, Day);
					std::printf("%-20s tree age %4.0f %-6s (day %3.0f)  h %4.2f m reach %4.2f m  tubes %4d clusters %5d bloom %4d fruit %4d  LOD0-3 tris %6d %6d %5d %5d  (seed 2: LOD0 %6d)\n",
					            N.c_str(), Age, DayName(Day), Day, C1.Height, C1.Reach, C1.Tubes, C1.Clusters, C1.Blossoms, C1.Fruit,
					            C1.Tris[0], C1.Tris[1], C1.Tris[2], C1.Tris[3], C2.Tris[0]);
					if (C1.bNaN || C2.bNaN || C1.Height <= 0.f)
					{
						std::printf("  !! %s: %s\n", N.c_str(), C1.bNaN || C2.bNaN ? "NaN in the skeleton" : "nothing grows");
						bOk = false;
					}
				}
			}
		}
		else if (S.Herb.Form != EPlantHerbForm::Alga && !S.bIndoor)
		{
			const float Days[4] = { 30.f, 160.f, 210.f, 270.f };
			const float Age = OnlyAge > 0.f ? OnlyAge : 6.f;
			for (float Day0 : Days)
			{
				const float Day = OnlyDay > 0.f ? OnlyDay : Day0;
				if (OnlyDay > 0.f && Day0 != Days[0]) continue;
				if (bBrief && Day != 210.f) continue;
				int32 T0 = 0, T1 = 0;
				float H = 0.f, R = 0.f;
				bool bNaN = false;
				HerbCost(S, 7, Age, Day, T0, T1, H, R, bNaN);
				const PlantHerbs::FState St = PlantHerbs::State(S, 7, Age, Day);
				std::printf("%-20s herb age %.0f %-6s (day %3.0f)  alive %d bloom %.2f  h %4.2f m radius %4.2f m  LOD0 tris %5d  LOD1 (lite) %5d\n",
				            N.c_str(), Age, DayName(Day), Day, St.bAlive ? 1 : 0, St.Bloom, H, R, T0, T1);
				if (bNaN)
				{
					std::printf("  !! %s: NaN in the herb\n", N.c_str());
					bOk = false;
				}
			}
		}
		return bOk;
	}
}

int main(int Argc, char** Argv)
{
	// Name or Name:age:day (one age and day of the year only)
	std::vector<std::string> Names;
	std::vector<float> OnlyAges, OnlyDays;
	bool bAll = false;
	for (int I = 1; I < Argc; I++)
	{
		if (std::strcmp(Argv[I], "all") == 0) { bAll = true; continue; }
		std::string A = Argv[I];
		float Age = -1.f, Day = -1.f;
		const size_t C1 = A.find(':');
		if (C1 != std::string::npos)
		{
			std::sscanf(A.c_str() + C1 + 1, "%f:%f", &Age, &Day);
			A = A.substr(0, C1);
		}
		Names.push_back(A);
		OnlyAges.push_back(Age);
		OnlyDays.push_back(Day);
	}
	if (!bAll && Names.empty())
	{
		Names = { "SiberianDwarfPine", "DwarfAlder", "GoldenRhododendron", "Fireweed", "MountainAvens", "CottonGrass",
		          "DwarfBirch", "Juniper", "StoneBirch", "Bilberry", "Lingonberry" };
	}
	bool bOk = true;
	int32 Found = 0;
	for (const FPlantSpecies& S : PlantSpeciesLibrary::All())
	{
		if (bAll)
		{
			Found++;
			bOk &= Report(S, true, -1.f, -1.f);
			continue;
		}
		for (size_t K = 0; K < Names.size(); K++)
		{
			if (Names[K] != *S.Name.ToString()) continue;
			Found++;
			bOk &= Report(S, false, K < OnlyAges.size() ? OnlyAges[K] : -1.f, K < OnlyDays.size() ? OnlyDays[K] : -1.f);
		}
	}
	std::printf("%d species, %s\n", Found, bOk ? "all build" : "PROBLEMS");
	if (!bAll && Found != (int32)Names.size()) std::printf("(asked for %d: a name is not in the library)\n", (int)Names.size());
	return bOk ? 0 : 1;
}
