#include "Plants/PlantSpecies.h"

namespace
{
	FLinearColor C(float R, float G, float B) { return FLinearColor(R, G, B); }

	/** Populus nigra 'Italica', the pyramidal (Lombardy) poplar: a narrow column ~25 m tall, golden in late October. */
	FPlantSpecies PyramidalPoplar()
	{
		FPlantSpecies S; // the struct defaults are this species' growth and season
		S.Name = TEXT("PyramidalPoplar");
		S.DisplayName = NSLOCTEXT("Plants", "PyramidalPoplar", "Pyramidal poplar");
		// rhombic-deltoid blade, broadest near the base, on a long flat stalk that lets it flutter
		S.Leaf.Length = 0.085f;
		S.Leaf.Width = 1.0f;
		S.Leaf.WidestAt = 0.28f;
		S.Leaf.Teeth = 14;
		S.Leaf.Petiole = 0.6f;
		S.Leaf.PerCluster = 6;
		S.Bark.Pattern = EPlantBarkPattern::Furrows;
		S.Bark.TileWidth = 0.25f;
		S.Bark.TileHeight = 0.9f;
		S.Bark.Depth = 2.8f;
		S.Bark.RidgeColour = C(0.085f, 0.08f, 0.07f);
		S.Bark.FurrowColour = C(0.015f, 0.014f, 0.012f);
		return S;
	}

	/** Populus nigra, the black poplar: a broad, open crown on a thick, deeply fissured trunk. */
	FPlantSpecies BlackPoplar()
	{
		FPlantSpecies S;
		S.Name = TEXT("BlackPoplar");
		S.DisplayName = NSLOCTEXT("Plants", "BlackPoplar", "Black poplar");
		S.MaxHeight = 26.f;
		S.GrowthRate = 0.07f;
		S.TrunkRadiusPerHeight = 0.018f;
		S.CrownBase = 0.25f;
		S.TrunkNodeSpacing = 0.45f;
		S.Branch1Angle = 52.f;
		S.Branch1MaxLength = 10.f;
		S.Branch1Profile = FVector(0.7, 1.0, 0.45);
		S.LengthTau = 9.f;
		S.Tropism = 0.035f;
		S.Wobble = 0.18f;
		S.Branch2Angle = 45.f;
		S.Branch2MaxLength = 2.6f;
		S.Branch2LengthRatio = 0.4f;
		S.Leaf.Length = 0.095f;
		S.Leaf.Width = 1.05f;
		S.Leaf.WidestAt = 0.22f;
		S.Leaf.Teeth = 16;
		S.Leaf.Petiole = 0.7f;
		S.Bark.Pattern = EPlantBarkPattern::Furrows;
		S.Bark.TileWidth = 0.3f;
		S.Bark.TileHeight = 0.7f;
		S.Bark.Depth = 3.6f;
		S.Bark.RidgeColour = C(0.06f, 0.055f, 0.05f);
		S.Bark.FurrowColour = C(0.01f, 0.009f, 0.008f);
		S.AutumnLeafColour = S.AutumnLeafColourLate = C(0.55f, 0.40f, 0.05f);
		S.ColourStartDay = 276.f;
		S.BareDay = 322.f;
		return S;
	}

	/** Malus domestica, an orchard apple: a low, wide, gnarled crown; red and yellow apples into late October. */
	FPlantSpecies Apple()
	{
		FPlantSpecies S;
		S.Name = TEXT("Apple");
		S.DisplayName = NSLOCTEXT("Plants", "Apple", "Apple tree");
		S.MaxHeight = 9.f;
		S.GrowthRate = 0.15f;
		S.GrowthShape = 1.4f;
		S.LeaderFraction = 0.45f;
		S.TrunkRadiusPerHeight = 0.024f;
		S.CrownBase = 0.15f;
		S.TrunkNodeSpacing = 0.15f;
		S.Branch1Angle = 52.f;
		S.Branch1MaxLength = 4.6f;
		S.Branch1Profile = FVector(0.85, 1.0, 0.8);
		S.LengthTau = 4.f;
		S.Tropism = 0.03f;
		S.Wobble = 0.4f;
		S.Branch2Spacing = 0.15f;
		S.Branch2Angle = 50.f;
		S.Branch2MaxLength = 1.2f;
		S.Branch2LengthRatio = 0.45f;
		S.Branch2ShadeDepth = 3.5f;
		S.BranchRadiusPerLength = 0.013f;
		S.LeafyTipLength = 3.f;
		S.LeafSpacing = 0.07f;
		S.LeafClusterSize = 0.3f;
		// elliptic, finely serrate, short-stalked
		S.Leaf.Length = 0.08f;
		S.Leaf.Width = 0.6f;
		S.Leaf.WidestAt = 0.45f;
		S.Leaf.Teeth = 18;
		S.Leaf.Petiole = 0.3f;
		S.Leaf.Fold = 0.2f;
		S.Leaf.PerCluster = 6;
		S.Bark.Pattern = EPlantBarkPattern::Plates;
		S.Bark.TileWidth = 0.2f;
		S.Bark.TileHeight = 0.3f;
		S.Bark.Depth = 1.8f;
		S.Bark.RidgeColour = C(0.07f, 0.0575f, 0.045f);
		S.Bark.FurrowColour = C(0.02f, 0.016f, 0.0125f);
		S.BudBreakDay = 115.f;
		S.FullLeafDay = 140.f;
		S.ColourStartDay = 285.f;
		S.ColourFullDay = 312.f;
		S.LeafDropStartDay = 298.f;
		S.BareDay = 326.f;
		S.SummerLeafColour = C(0.06f, 0.15f, 0.03f);
		S.AutumnLeafColour = C(0.30f, 0.30f, 0.05f);
		S.AutumnLeafColourLate = C(0.45f, 0.32f, 0.05f);
		S.FruitSize = 0.075f;
		S.FruitChance = 0.6f;
		S.FruitingAge = 4.f;
		S.FruitSetDay = 160.f;
		S.FruitRipeDay = 245.f;
		S.FruitFallDay = 305.f;
		S.FruitColour = C(0.42f, 0.04f, 0.02f);
		return S;
	}

	/** Acer platanoides, the Norway maple: a dense round crown; leaves turn red, orange and yellow. */
	FPlantSpecies Maple()
	{
		FPlantSpecies S;
		S.Name = TEXT("Maple");
		S.DisplayName = NSLOCTEXT("Plants", "Maple", "Norway maple");
		S.MaxHeight = 26.f;
		S.GrowthRate = 0.07f;
		S.GrowthShape = 1.5f;
		S.LeaderFraction = 0.7f;
		S.TrunkRadiusPerHeight = 0.016f;
		S.CrownBase = 0.2f;
		S.TrunkNodeSpacing = 0.38f;
		S.Branch1Angle = 45.f;
		S.Branch1MaxLength = 7.5f;
		S.Branch1Profile = FVector(0.75, 1.0, 0.6);
		S.LengthTau = 9.f;
		S.Tropism = 0.05f;
		S.Wobble = 0.15f;
		S.Branch2Spacing = 0.22f;
		S.Branch2Angle = 45.f;
		S.Branch2MaxLength = 2.0f;
		S.Branch2LengthRatio = 0.4f;
		S.LeafyTipLength = 3.f;
		S.LeafSpacing = 0.09f;
		S.LeafClusterSize = 0.45f;
		// five pointed lobes on a long stalk
		S.Leaf.Form = EPlantLeafForm::Palmate;
		S.Leaf.Length = 0.12f;
		S.Leaf.Width = 1.15f;
		S.Leaf.Lobes = 5;
		S.Leaf.LobeDepth = 0.45f;
		S.Leaf.Petiole = 0.8f;
		S.Leaf.Fold = 0.1f;
		S.Leaf.PerCluster = 5;
		S.Bark.Pattern = EPlantBarkPattern::Furrows;
		S.Bark.TileWidth = 0.15f;
		S.Bark.TileHeight = 0.3f;
		S.Bark.Depth = 1.2f;
		S.Bark.RidgeColour = C(0.065f, 0.0625f, 0.0575f);
		S.Bark.FurrowColour = C(0.025f, 0.023f, 0.02f);
		S.BudBreakDay = 112.f;
		S.FullLeafDay = 135.f;
		S.ColourStartDay = 276.f;
		S.ColourFullDay = 300.f;
		S.LeafDropStartDay = 290.f;
		S.BareDay = 318.f;
		S.SummerLeafColour = C(0.05f, 0.14f, 0.025f);
		S.AutumnLeafColour = C(0.50f, 0.07f, 0.02f);
		S.AutumnLeafColourLate = C(0.66f, 0.45f, 0.03f);
		return S;
	}

	/** Quercus bicolor, the swamp white oak: crooked limbs, a broad crown, scalloped leaves held late, acorns. */
	FPlantSpecies SwampWhiteOak()
	{
		FPlantSpecies S;
		S.Name = TEXT("SwampWhiteOak");
		S.DisplayName = NSLOCTEXT("Plants", "SwampWhiteOak", "Swamp white oak");
		S.MaxHeight = 27.f;
		S.GrowthRate = 0.06f;
		S.GrowthShape = 1.5f;
		S.LeaderFraction = 0.6f;
		S.TrunkRadiusPerHeight = 0.022f;
		S.CrownBase = 0.22f;
		S.TrunkNodeSpacing = 0.42f;
		S.Branch1Angle = 60.f;
		S.Branch1MaxLength = 9.f;
		S.Branch1Profile = FVector(0.85, 1.0, 0.55);
		S.LengthTau = 11.f;
		S.Tropism = 0.025f;
		S.Wobble = 0.32f;
		S.Branch2Spacing = 0.2f;
		S.Branch2Angle = 50.f;
		S.Branch2MaxLength = 2.2f;
		S.Branch2LengthRatio = 0.4f;
		S.LeafyTipLength = 4.f;
		S.LeafSpacing = 0.08f;
		S.LeafClusterSize = 0.4f;
		// obovate, widest past the middle, with shallow rounded lobes
		S.Leaf.Length = 0.14f;
		S.Leaf.Width = 0.6f;
		S.Leaf.WidestAt = 0.68f;
		S.Leaf.Lobes = 6;
		S.Leaf.LobeDepth = 0.22f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.12f;
		S.Leaf.Fold = 0.1f;
		S.Leaf.PerCluster = 7;
		S.Bark.Pattern = EPlantBarkPattern::Blocks;
		S.Bark.TileWidth = 0.35f;
		S.Bark.TileHeight = 0.5f;
		S.Bark.Depth = 3.2f;
		S.Bark.RidgeColour = C(0.075f, 0.065f, 0.055f);
		S.Bark.FurrowColour = C(0.015f, 0.0125f, 0.01f);
		S.BudBreakDay = 122.f;
		S.FullLeafDay = 148.f;
		S.ColourStartDay = 285.f;
		S.ColourFullDay = 312.f;
		S.LeafDropStartDay = 300.f;
		S.BareDay = 338.f;
		S.SummerLeafColour = C(0.045f, 0.12f, 0.025f);
		S.AutumnLeafColour = C(0.42f, 0.17f, 0.04f);
		S.AutumnLeafColourLate = C(0.36f, 0.26f, 0.07f);
		S.FruitSize = 0.025f;
		S.FruitChance = 0.25f;
		S.FruitingAge = 12.f;
		S.FruitSetDay = 170.f;
		S.FruitRipeDay = 262.f;
		S.FruitFallDay = 300.f;
		S.FruitColour = C(0.17f, 0.09f, 0.03f);
		return S;
	}
}

const TArray<FPlantSpecies>& PlantSpeciesLibrary::All()
{
	static const TArray<FPlantSpecies> Species = { PyramidalPoplar(), BlackPoplar(), Apple(), Maple(), SwampWhiteOak() };
	return Species;
}

const FPlantSpecies* PlantSpeciesLibrary::Find(FName Name)
{
	return All().FindByPredicate([Name](const FPlantSpecies& S) { return S.Name == Name; });
}

TArray<FName> PlantSpeciesLibrary::Names()
{
	TArray<FName> Out;
	for (const FPlantSpecies& S : All())
	{
		Out.Add(S.Name);
	}
	return Out;
}
