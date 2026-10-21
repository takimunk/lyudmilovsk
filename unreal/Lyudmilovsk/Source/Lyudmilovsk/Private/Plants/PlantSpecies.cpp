#include "Plants/PlantSpecies.h"

#include "LyudText.h"

namespace
{
	FLinearColor C(float R, float G, float B) { return FLinearColor(R, G, B); }

	/** Populus nigra 'Italica', the pyramidal (Lombardy) poplar: a narrow column ~25 m tall, golden in late October. */
	FPlantSpecies PyramidalPoplar()
	{
		FPlantSpecies S; // the struct defaults are this species' growth and season
		S.Name = TEXT("PyramidalPoplar");
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
		S.LifeMedian = 70.f;
		S.LifeSpread = 0.25f;
		S.LifeMax = 120.f;
		S.DeclineYears = 12.f;
		S.SnagYears = 8.f;
		return S;
	}

	/** Populus nigra, the black poplar: a broad, open crown on a thick, deeply fissured trunk. */
	FPlantSpecies BlackPoplar()
	{
		FPlantSpecies S;
		S.Name = TEXT("BlackPoplar");
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
		S.LifeMedian = 160.f;
		S.LifeSpread = 0.35f;
		S.LifeMax = 400.f;
		S.DeclineYears = 15.f;
		S.SnagYears = 10.f;
		return S;
	}

	/**
	 * Malus domestica, an old orchard apple: a short trunk forking into three or four scaffold limbs that
	 * arch outward, a low crown wider than tall. Shoots end in flower clusters and a side bud carries on
	 * (sympodial), so the limbs zigzag and sag under the fruit; the old wood is crowded with spurs, short
	 * shoots that carry the leaf rosettes and the apples. Grey bark flaking in small plates, rusty under.
	 * Apples ripen in September, yellow-green with a striped red blush, and drop through October.
	 */
	FPlantSpecies Apple()
	{
		FPlantSpecies S;
		S.Name = TEXT("Apple");
		S.MaxHeight = 7.f;
		S.GrowthRate = 0.15f;
		S.GrowthShape = 1.4f;
		S.LeaderFraction = 0.3f;
		S.TrunkRadiusPerHeight = 0.012f;
		S.CrownBase = 0.12f;
		S.TrunkNodeSpacing = 0.3f;
		S.Branch1Angle = 55.f;
		S.Branch1MaxLength = 5.f;
		S.Branch1Profile = FVector(1.0, 1.0, 0.9);
		S.LengthTau = 6.f;
		S.Tropism = 0.07f;
		S.Wobble = 0.15f;
		S.Branch2Spacing = 0.15f;
		S.Branch2Angle = 55.f;
		S.Branch2MaxLength = 1.6f;
		S.Branch2LengthRatio = 0.4f;
		S.Branch2ShadeDepth = 4.f;
		S.BranchRadiusPerLength = 0.012f;
		S.LeafyTipLength = 2.5f;
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
		S.Bark.TileWidth = 0.16f;
		S.Bark.TileHeight = 0.22f;
		S.Bark.Depth = 2.f;
		S.Bark.RidgeColour = C(0.075f, 0.068f, 0.06f);
		S.Bark.FurrowColour = C(0.02f, 0.017f, 0.014f);
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
		S.FruitChance = 0.15f;
		S.FruitingAge = 4.f;
		S.FruitSetDay = 160.f;
		S.FruitRipeDay = 245.f;
		S.FruitFallDay = 305.f;
		S.FruitColour = C(0.38f, 0.03f, 0.02f);
		S.RingWidth = 0.0018f;
		S.CrownWidth = 1.25f;
		S.Branch1AngleTop = 40.f;
		S.Kink = 0.3f;
		S.KinkSpacing = 0.45f;
		S.Droop = 0.02f;
		S.SpurSpacing = 0.06f;
		S.SpurLength = 0.08f;
		S.SpurAge = 2.f;
		S.Bark.FlakeChance = 0.12f;
		S.Bark.UnderColour = C(0.055f, 0.036f, 0.022f);
		S.FruitUnripeColour = C(0.12f, 0.28f, 0.04f);
		S.FruitGroundColour = C(0.45f, 0.42f, 0.08f);
		S.FruitBlush = 1.f;
		S.FruitStripes = 0.7f;
		S.LifeMedian = 70.f;
		S.LifeSpread = 0.3f;
		S.LifeMax = 150.f;
		S.DeclineYears = 10.f;
		S.SnagYears = 6.f;
		S.FruitItem = TEXT("apple");
		return S;
	}

	/** Acer platanoides, the Norway maple: a dense round crown; leaves turn red, orange and yellow. */
	FPlantSpecies Maple()
	{
		FPlantSpecies S;
		S.Name = TEXT("Maple");
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
		S.LifeMedian = 150.f;
		S.LifeSpread = 0.3f;
		S.LifeMax = 300.f;
		S.DeclineYears = 15.f;
		S.SnagYears = 10.f;
		return S;
	}

	/**
	 * Quercus bicolor, the swamp white oak, grown in the open: pyramidal while young, then the trunk stops at
	 * ~8 m and forks into massive crooked limbs under a broad, regular, rounded crown as wide as it is tall
	 * (15-21 m); the trunk keeps thickening for a century (to ~1 m across). Scalloped leaves held late, acorns.
	 */
	FPlantSpecies SwampWhiteOak()
	{
		FPlantSpecies S;
		S.Name = TEXT("SwampWhiteOak");
		S.MaxHeight = 21.f;
		S.GrowthRate = 0.035f;
		S.GrowthShape = 1.3f;
		S.LeaderFraction = 0.4f;
		S.TrunkRadiusPerHeight = 0.012f;
		S.CrownBase = 0.14f;
		S.TrunkNodeSpacing = 0.3f;
		S.Branch1Angle = 72.f;
		S.Branch1MaxLength = 16.f;
		S.Branch1Profile = FVector(1.0, 1.0, 0.9);
		S.LengthTau = 22.f;
		S.Tropism = 0.04f;
		S.Wobble = 0.12f;
		S.Branch2Spacing = 0.2f;
		S.Branch2Angle = 50.f;
		S.Branch2MaxLength = 4.5f;
		S.Branch2LengthRatio = 0.5f;
		S.LeafyTipLength = 6.f;
		S.LeafSpacing = 0.11f;
		S.LeafClusterSize = 0.45f;
		// obovate, widest past the middle, with shallow rounded lobes
		S.Leaf.Length = 0.14f;
		S.Leaf.Width = 0.6f;
		S.Leaf.WidestAt = 0.68f;
		S.Leaf.Lobes = 6;
		S.Leaf.LobeDepth = 0.22f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.12f;
		S.Leaf.Fold = 0.1f;
		S.Leaf.PerCluster = 5;
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
		S.FruitingAge = 20.f;
		S.FruitSetDay = 170.f;
		S.FruitRipeDay = 262.f;
		S.FruitFallDay = 300.f;
		S.FruitColour = C(0.17f, 0.09f, 0.03f);
		S.RingWidth = 0.0028f;
		S.CrownWidth = 1.05f;
		S.Branch1AngleTop = 45.f;
		S.Kink = 0.3f;
		S.KinkSpacing = 0.9f;
		S.Droop = 0.004f;
		S.Branch2ShadeDepth = 8.f;
		S.BranchRadiusPerLength = 0.009f;
		S.FruitUnripeColour = C(0.12f, 0.2f, 0.04f);
		S.FruitGroundColour = C(0.17f, 0.09f, 0.03f);
		S.LifeMedian = 250.f;
		S.LifeSpread = 0.3f;
		S.LifeMax = 400.f;
		S.DeclineYears = 25.f;
		S.SnagYears = 20.f;
		return S;
	}

	/** Birch leaf colours and calendar: early to turn, clear yellow by mid October, mostly down by early November. */
	void BirchSeason(FPlantSpecies& S)
	{
		S.BudBreakDay = 115.f;
		S.FullLeafDay = 138.f;
		S.ColourStartDay = 262.f;
		S.ColourFullDay = 285.f;
		S.LeafDropStartDay = 272.f;
		S.BareDay = 306.f;
		S.SpringLeafColour = C(0.22f, 0.4f, 0.06f);
		S.SummerLeafColour = C(0.07f, 0.17f, 0.03f);
		S.AutumnLeafColour = C(0.72f, 0.55f, 0.04f);
		S.AutumnLeafColourLate = C(0.6f, 0.47f, 0.06f);
	}

	/**
	 * Betula pendula, silver birch (берёза повислая): a pioneer that shoots up fast to 15-25 m on a
	 * straight trunk, an open oval crown of ascending limbs with long, thin twigs hanging from them.
	 * Chalk-white bark with black lenticels; old trunks go black and deeply fissured from the ground up.
	 * Small triangular, doubly toothed leaves on long stalks.
	 */
	FPlantSpecies SilverBirch()
	{
		FPlantSpecies S;
		S.Name = TEXT("SilverBirch");
		S.MaxHeight = 22.f;
		S.GrowthRate = 0.07f;
		S.GrowthShape = 1.4f;
		S.LeaderFraction = 0.85f;
		S.TrunkRadiusPerHeight = 0.011f;
		S.RingWidth = 0.0015f;
		S.CrownBase = 0.28f;
		S.CrownWidth = 0.55f;
		S.TrunkNodeSpacing = 0.35f;
		S.Branch1Angle = 50.f;
		S.Branch1AngleTop = 32.f;
		S.Branch1MaxLength = 6.f;
		S.Branch1Profile = FVector(0.8, 1.0, 0.5);
		S.LengthTau = 6.f;
		S.Tropism = 0.06f;
		S.Wobble = 0.15f;
		S.Kink = 0.15f;
		S.KinkSpacing = 0.6f;
		S.Droop = 0.01f;
		S.Branch2Spacing = 0.12f;
		S.Branch2Angle = 35.f;
		S.Branch2MaxLength = 3.f;
		S.Branch2LengthRatio = 0.9f;
		S.Branch2ShadeDepth = 6.f;
		S.TwigHang = 1.f;
		S.BranchRadiusPerLength = 0.007f;
		S.LeafyTipLength = 1.5f;
		S.LeafSpacing = 0.1f;
		S.LeafClusterSize = 0.12f;
		S.Leaf.Length = 0.055f;
		S.Leaf.Width = 0.8f;
		S.Leaf.WidestAt = 0.3f;
		S.Leaf.Teeth = 12; // doubly toothed: finer than the mesh draws, the veins carry it
		S.Leaf.TeethDepth = 0.06f;
		S.Leaf.Petiole = 0.45f;
		S.Leaf.PerCluster = 3;
		S.Bark.Pattern = EPlantBarkPattern::Birch;
		S.Bark.TileWidth = 0.25f;
		S.Bark.TileHeight = 0.5f;
		S.Bark.Depth = 0.6f;
		S.Bark.RidgeColour = C(0.62f, 0.62f, 0.58f);
		S.Bark.FurrowColour = C(0.015f, 0.015f, 0.015f);
		S.Bark.TwigRadius = 0.025f;
		S.Bark.TwigColour = C(0.07f, 0.03f, 0.018f);
		S.Bark.DarkBaseRate = 0.04f;
		S.Bark.DarkBaseMax = 2.f;
		S.Bark.BaseRidgeColour = C(0.08f, 0.08f, 0.075f);
		S.Bark.BaseFurrowColour = C(0.01f, 0.01f, 0.01f);
		BirchSeason(S);
		S.LifeMedian = 90.f;
		S.LifeSpread = 0.3f;
		S.LifeMax = 200.f;
		S.DeclineYears = 10.f;
		S.SnagYears = 6.f;
		S.TwigRadiusScale = 0.3f;
		S.Sway = 25.f;
		S.Bark.Scars = 1.f;
		return S;
	}

	/**
	 * Betula pubescens, downy birch (берёза пушистая), of wet ground and the north: smaller (to ~18 m),
	 * a denser, more upright crown, twigs that do not hang; duller white bark without the black
	 * fissured base; rounder, singly toothed leaves.
	 */
	FPlantSpecies DownyBirch()
	{
		FPlantSpecies S = SilverBirch();
		S.Name = TEXT("DownyBirch");
		S.MaxHeight = 18.f;
		S.GrowthRate = 0.06f;
		S.CrownWidth = 0.6f;
		S.Branch1Angle = 40.f;
		S.Branch1AngleTop = 28.f;
		S.Branch2Spacing = 0.14f;
		S.TwigHang = 0.15f;
		S.Tropism = 0.09f;
		S.Leaf.Length = 0.05f;
		S.Leaf.Width = 0.78f;
		S.Leaf.WidestAt = 0.42f;
		S.Leaf.Teeth = 10;
		S.Leaf.TeethDepth = 0.04f;
		S.Bark.RidgeColour = C(0.5f, 0.48f, 0.44f);
		S.Bark.TwigColour = C(0.05f, 0.025f, 0.018f);
		S.Bark.DarkBaseRate = 0.01f;
		S.Bark.DarkBaseMax = 0.5f;
		S.Bark.BaseRidgeColour = C(0.16f, 0.15f, 0.13f);
		S.Bark.BaseFurrowColour = C(0.04f, 0.035f, 0.03f);
		S.LifeMedian = 90.f;
		S.LifeSpread = 0.3f;
		S.LifeMax = 200.f;
		S.DeclineYears = 10.f;
		S.SnagYears = 6.f;
		S.Sway = 12.f;
		S.TwigRadiusScale = 0.5f;
		S.Branch2MaxLength = 1.8f;
		S.Branch2LengthRatio = 0.5f;
		return S;
	}

	/** A weeping silver birch (Betula pendula 'Tristis'/'Youngii'): limbs arching over and twigs curtaining to the ground. */
	FPlantSpecies WeepingBirch()
	{
		FPlantSpecies S = SilverBirch();
		S.Name = TEXT("WeepingBirch");
		S.MaxHeight = 14.f;
		S.LeaderFraction = 0.7f;
		S.CrownBase = 0.2f;
		S.CrownWidth = 0.85f;
		S.Branch1Angle = 70.f;
		S.Branch1AngleTop = 55.f;
		S.Tropism = 0.02f;
		S.Droop = 0.06f;
		S.TwigHang = 1.f;
		S.Branch2MaxLength = 4.f;
		S.Branch2LengthRatio = 1.f;
		S.LifeMedian = 70.f;
		S.LifeSpread = 0.25f;
		S.LifeMax = 120.f;
		S.DeclineYears = 8.f;
		S.SnagYears = 5.f;
		S.Sway = 35.f;
		return S;
	}

	/**
	 * Betula nana, dwarf birch (берёза карликовая, ерник) of the tundra and bogs: a sprawling shrub knee-high,
	 * many crooked stems from the base, red-brown bark, tiny round scalloped leaves turning red in September.
	 */
	FPlantSpecies DwarfBirch()
	{
		FPlantSpecies S;
		S.Name = TEXT("DwarfBirch");
		S.MaxHeight = 0.9f;
		S.GrowthRate = 0.15f;
		S.GrowthShape = 1.2f;
		S.LeaderFraction = 0.2f;
		S.TrunkRadiusPerHeight = 0.02f;
		S.MinRadius = 0.003f;
		S.CrownBase = 0.f;
		S.CrownWidth = 2.f;
		S.TrunkNodeSpacing = 0.025f;
		S.Branch1Angle = 75.f;
		S.Branch1AngleTop = 55.f;
		S.Branch1MaxLength = 0.8f;
		S.Branch1Profile = FVector(1.0, 1.0, 0.8);
		S.LengthTau = 5.f;
		S.Tropism = 0.4f;
		S.Wobble = 0.5f;
		S.Kink = 0.4f;
		S.KinkSpacing = 0.08f;
		S.Branch2Spacing = 0.05f;
		S.Branch2Angle = 50.f;
		S.Branch2MaxLength = 0.2f;
		S.Branch2LengthRatio = 0.5f;
		S.Branch2ShadeDepth = 0.4f;
		S.BranchRadiusBase = 0.002f;
		S.BranchRadiusPerLength = 0.01f;
		S.LeafyTipLength = 0.5f;
		S.LeafSpacing = 0.025f;
		S.LeafClusterSize = 0.06f;
		S.Leaf.Length = 0.015f;
		S.Leaf.Width = 1.1f;
		S.Leaf.WidestAt = 0.5f;
		S.Leaf.Lobes = 4;
		S.Leaf.LobeDepth = 0.15f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.2f;
		S.Leaf.PerCluster = 3;
		S.Bark.Pattern = EPlantBarkPattern::Smooth;
		S.Bark.TileWidth = 0.05f;
		S.Bark.TileHeight = 0.1f;
		S.Bark.Depth = 0.3f;
		S.Bark.RidgeColour = C(0.07f, 0.035f, 0.02f);
		S.Bark.FurrowColour = C(0.03f, 0.015f, 0.01f);
		S.BudBreakDay = 140.f;
		S.FullLeafDay = 160.f;
		S.ColourStartDay = 240.f;
		S.ColourFullDay = 265.f;
		S.LeafDropStartDay = 268.f;
		S.BareDay = 300.f;
		S.SummerLeafColour = C(0.04f, 0.1f, 0.02f);
		S.AutumnLeafColour = C(0.5f, 0.07f, 0.03f);
		S.AutumnLeafColourLate = C(0.6f, 0.25f, 0.04f);
		S.LifeMedian = 60.f;
		S.LifeSpread = 0.4f;
		S.LifeMax = 150.f;
		S.DeclineYears = 8.f;
		S.SnagYears = 3.f;
		S.Sway = 2.f;
		return S;
	}

	/**
	 * Betula ermanii, stone birch (берёза каменная) of Kamchatka and the Far East: a crooked, leaning trunk
	 * and a broad, low crown shaped by wind and snow; creamy, pinkish-grey bark peeling in strips.
	 */
	FPlantSpecies StoneBirch()
	{
		FPlantSpecies S = SilverBirch();
		S.Name = TEXT("StoneBirch");
		S.MaxHeight = 12.f;
		S.GrowthRate = 0.05f;
		S.LeaderFraction = 0.35f;
		S.TrunkRadiusPerHeight = 0.02f;
		S.RingWidth = 0.002f;
		S.CrownBase = 0.15f;
		S.CrownWidth = 1.f;
		S.TrunkNodeSpacing = 0.3f;
		S.Branch1Angle = 60.f;
		S.Branch1AngleTop = 45.f;
		S.Branch1MaxLength = 7.f;
		S.Wobble = 0.3f;
		S.Kink = 0.4f;
		S.KinkSpacing = 0.5f;
		S.TwigHang = 0.2f;
		S.Leaf.Length = 0.07f;
		S.Leaf.Width = 0.75f;
		S.Leaf.WidestAt = 0.4f;
		S.Bark.RidgeColour = C(0.5f, 0.42f, 0.38f);
		S.Bark.DarkBaseRate = 0.f;
		S.LifeMedian = 300.f;
		S.LifeSpread = 0.3f;
		S.LifeMax = 500.f;
		S.DeclineYears = 25.f;
		S.SnagYears = 15.f;
		S.Sway = 10.f;
		S.TwigRadiusScale = 0.5f;
		S.Branch2MaxLength = 1.8f;
		S.Branch2LengthRatio = 0.5f;
		return S;
	}

	// ---- The Baltic coast: dune pine, spruce, alder carr, rowan, sea buckthorn, juniper, aspen.

	/** Cone colours shared by the conifers: green in the first summer, brown once ripe. */
	void ConeColours(FPlantSpecies& S)
	{
		S.FruitUnripeColour = C(0.08f, 0.16f, 0.035f);
		S.FruitGroundColour = C(0.1f, 0.06f, 0.03f);
		S.FruitColour = C(0.1f, 0.06f, 0.03f);
		S.FruitBlush = 0.f;
	}

	/**
	 * Pinus sylvestris, Scots pine (сосна обыкновенная), the tree of the Baltic dunes. One straight trunk and
	 * a whorl of four branches each year; the lower limbs die in the shade and drop, so a stand is bare for
	 * the first half of its height and the crown goes flat-topped with age as the leader gives way to the top
	 * limbs. Bark orange and papery on the upper trunk, grey-brown and deeply plated toward the foot. Paired
	 * blue-green needles kept three years, so they grow only on the youngest shoots, in brush-like tufts at
	 * the ends of crooked limbs; new shoots light green in May, the needles bronze in winter. Cones hang on
	 * for a year.
	 */
	FPlantSpecies ScotsPine()
	{
		FPlantSpecies S;
		S.Name = TEXT("ScotsPine");
		S.bEvergreen = true;
		S.MaxHeight = 24.f;
		S.GrowthRate = 0.045f;
		S.GrowthShape = 1.4f;
		S.LeaderFraction = 0.82f;
		S.TrunkRadiusPerHeight = 0.006f;
		S.RingWidth = 0.0012f;
		S.CrownBase = 0.42f;
		S.TrunkNodeSpacing = 0.55f;
		S.WhorlSize = 4;
		S.Branch1Angle = 78.f;
		S.Branch1AngleTop = 40.f;
		S.Branch1MaxLength = 5.f;
		S.Branch1Profile = FVector(0.55, 1.0, 0.4);
		S.LengthTau = 10.f;
		S.Tropism = 0.06f;
		S.Wobble = 0.2f;
		S.Kink = 0.35f;
		S.KinkSpacing = 0.7f;
		S.CrownWidth = 0.55f;
		S.Branch2Spacing = 0.3f;
		S.Branch2Angle = 55.f;
		S.Branch2MaxLength = 1.3f;
		S.Branch2LengthRatio = 0.3f;
		S.Branch2ShadeDepth = 2.5f;
		S.BranchRadiusPerLength = 0.007f;
		S.LeafyTipLength = 1.8f;
		S.LeafSpacing = 0.14f;
		S.LeafClusterSize = 0.5f;
		S.Sway = 4.f;
		// two needles to a bundle, stiff and twisted, blue-green
		S.Leaf.Form = EPlantLeafForm::Needle;
		S.Leaf.Length = 0.4f;
		S.Leaf.Width = 0.9f;
		S.Leaf.Lobes = 2;
		S.Leaf.PerCluster = 20;
		S.Leaf.Cards = 3;
		S.Bark.Pattern = EPlantBarkPattern::Plates;
		S.Bark.TileWidth = 0.22f;
		S.Bark.TileHeight = 0.4f;
		S.Bark.Depth = 2.4f;
		S.Bark.RidgeColour = C(0.2f, 0.075f, 0.03f);
		S.Bark.FurrowColour = C(0.1f, 0.035f, 0.015f);
		S.Bark.FlakeChance = 0.22f;
		S.Bark.UnderColour = C(0.32f, 0.12f, 0.045f);
		S.Bark.DarkBaseRate = 0.2f;
		S.Bark.DarkBaseMax = 9.f;
		S.Bark.BaseRidgeColour = C(0.07f, 0.055f, 0.045f);
		S.Bark.BaseFurrowColour = C(0.012f, 0.01f, 0.008f);
		S.BudBreakDay = 118.f;
		S.FullLeafDay = 150.f;
		S.ColourStartDay = 285.f;
		S.ColourFullDay = 335.f;
		S.SpringLeafColour = C(0.17f, 0.28f, 0.05f);
		S.SummerLeafColour = C(0.055f, 0.13f, 0.065f);
		S.AutumnLeafColour = C(0.1f, 0.13f, 0.045f);
		S.AutumnLeafColourLate = C(0.1f, 0.13f, 0.045f);
		S.FruitSize = 0.045f;
		S.FruitAspect = 1.5f;
		S.FruitChance = 0.1f;
		S.FruitingAge = 15.f;
		S.FruitSetDay = 140.f;
		S.FruitRipeDay = 280.f;
		S.FruitFallDay = 385.f;
		ConeColours(S);
		S.LifeMedian = 200.f;
		S.LifeSpread = 0.3f;
		S.LifeMax = 400.f;
		S.DeclineYears = 25.f;
		S.SnagYears = 25.f;
		return S;
	}

	/**
	 * The same pine on the open dune: shaped by the wind and sand blast into a low, crooked tree, the trunk
	 * leaning and forking early into a few long limbs under a wide, flat crown.
	 */
	FPlantSpecies DunePine()
	{
		FPlantSpecies S = ScotsPine();
		S.Name = TEXT("DunePine");
		S.MaxHeight = 11.f;
		S.GrowthRate = 0.035f;
		S.LeaderFraction = 0.5f;
		S.TrunkRadiusPerHeight = 0.011f;
		S.RingWidth = 0.0022f;
		S.CrownBase = 0.18f;
		S.TrunkNodeSpacing = 0.45f;
		S.WhorlSize = 3;
		S.Branch1Angle = 68.f;
		S.Branch1AngleTop = 55.f;
		S.Branch1MaxLength = 6.f;
		S.Branch1Profile = FVector(0.9, 1.0, 0.8);
		S.Wobble = 0.34f;
		S.Kink = 0.6f;
		S.KinkSpacing = 0.5f;
		S.Tropism = 0.025f;
		S.CrownWidth = 1.4f;
		S.Bark.DarkBaseMax = 4.f;
		S.LifeMedian = 250.f;
		S.LifeMax = 450.f;
		return S;
	}

	/**
	 * Picea abies, Norway spruce (ель обыкновенная): a monopodial cone, one leader and a whorl of five
	 * branches a year, the limbs held out nearly flat and sagging, their tips turned up, hung with
	 * branchlets that drip straight down. Needles stay six or seven years, so every shoot is clothed to its
	 * base and the tree is a dark, dense wall to the ground. Scaly red-brown bark. Long cones hang at the top.
	 */
	FPlantSpecies NorwaySpruce()
	{
		FPlantSpecies S;
		S.Name = TEXT("NorwaySpruce");
		S.bEvergreen = true;
		S.bConicalCrown = true;
		S.MaxHeight = 26.f;
		S.GrowthRate = 0.04f;
		S.GrowthShape = 1.8f;
		S.LeaderFraction = 1.f;
		S.TrunkRadiusPerHeight = 0.0085f;
		S.RingWidth = 0.0012f;
		S.CrownBase = 0.08f;
		S.TrunkNodeSpacing = 0.5f;
		S.WhorlSize = 5;
		S.Branch1Angle = 84.f;
		S.Branch1AngleTop = 36.f;
		S.Branch1MaxLength = 3.8f;
		S.Branch1Profile = FVector(1.0, 0.62, 0.06);
		S.LengthTau = 9.f;
		S.Tropism = 0.07f;
		S.Droop = 0.02f;
		S.Wobble = 0.08f;
		S.Branch2Spacing = 0.18f;
		S.Branch2Angle = 70.f;
		S.Branch2MaxLength = 0.9f;
		S.Branch2LengthRatio = 0.3f;
		S.Branch2ShadeDepth = 6.f;
		S.TwigHang = 0.55f;
		S.TwigRadiusScale = 0.5f;
		S.LeafyTipLength = 6.f;
		S.LeafSpacing = 0.14f;
		S.LeafClusterSize = 0.5f;
		S.Sway = 3.f;
		S.Leaf.Form = EPlantLeafForm::Needle;
		S.Leaf.Length = 0.4f;
		S.Leaf.Width = 0.9f;
		S.Leaf.Lobes = 1;
		S.Leaf.PerCluster = 24;
		S.Leaf.Cards = 3;
		S.Bark.Pattern = EPlantBarkPattern::Plates;
		S.Bark.TileWidth = 0.18f;
		S.Bark.TileHeight = 0.26f;
		S.Bark.Depth = 2.2f;
		S.Bark.RidgeColour = C(0.07f, 0.03f, 0.016f);
		S.Bark.FurrowColour = C(0.04f, 0.02f, 0.012f);
		S.Bark.FlakeChance = 0.25f;
		S.Bark.UnderColour = C(0.25f, 0.1f, 0.05f);
		S.BudBreakDay = 125.f;
		S.FullLeafDay = 155.f;
		S.ColourStartDay = 290.f;
		S.ColourFullDay = 340.f;
		S.SpringLeafColour = C(0.14f, 0.28f, 0.05f);
		S.SummerLeafColour = C(0.03f, 0.09f, 0.045f);
		S.AutumnLeafColour = C(0.035f, 0.08f, 0.05f);
		S.AutumnLeafColourLate = C(0.035f, 0.08f, 0.05f);
		S.FruitSize = 0.05f;
		S.FruitAspect = 2.6f;
		S.FruitChance = 0.18f;
		S.FruitingAge = 25.f;
		S.FruitSetDay = 150.f;
		S.FruitRipeDay = 265.f;
		S.FruitFallDay = 395.f;
		ConeColours(S);
		S.LifeMedian = 250.f;
		S.LifeSpread = 0.3f;
		S.LifeMax = 400.f;
		S.DeclineYears = 25.f;
		S.SnagYears = 15.f;
		return S;
	}

	/**
	 * Alnus glutinosa, black alder (ольха чёрная) of the wet hollows behind the dunes: a fast, short-lived
	 * tree with a conical, later oval crown, and dark, blocky bark. Round leaves with a notched tip that
	 * stay dark green and fall green in November, without any autumn colour.
	 */
	FPlantSpecies BlackAlder()
	{
		FPlantSpecies S;
		S.Name = TEXT("BlackAlder");
		S.MaxHeight = 22.f;
		S.GrowthRate = 0.075f;
		S.GrowthShape = 1.4f;
		S.LeaderFraction = 0.7f;
		S.TrunkRadiusPerHeight = 0.011f;
		S.RingWidth = 0.002f;
		S.CrownBase = 0.25f;
		S.TrunkNodeSpacing = 0.4f;
		S.Branch1Angle = 55.f;
		S.Branch1AngleTop = 35.f;
		S.Branch1MaxLength = 5.5f;
		S.Branch1Profile = FVector(0.7, 1.0, 0.5);
		S.LengthTau = 8.f;
		S.Tropism = 0.05f;
		S.Wobble = 0.18f;
		S.Kink = 0.15f;
		S.KinkSpacing = 0.6f;
		S.Branch2Spacing = 0.2f;
		S.Branch2Angle = 50.f;
		S.Branch2MaxLength = 1.8f;
		S.Branch2LengthRatio = 0.4f;
		S.LeafyTipLength = 3.f;
		S.LeafSpacing = 0.1f;
		S.LeafClusterSize = 0.4f;
		S.Leaf.Length = 0.085f;
		S.Leaf.Width = 0.85f;
		S.Leaf.WidestAt = 0.62f;
		S.Leaf.Teeth = 14;
		S.Leaf.Petiole = 0.25f;
		S.Leaf.Fold = 0.1f;
		S.Leaf.PerCluster = 6;
		S.Bark.Pattern = EPlantBarkPattern::Blocks;
		S.Bark.TileWidth = 0.22f;
		S.Bark.TileHeight = 0.3f;
		S.Bark.Depth = 2.4f;
		S.Bark.RidgeColour = C(0.055f, 0.045f, 0.038f);
		S.Bark.FurrowColour = C(0.01f, 0.009f, 0.008f);
		S.BudBreakDay = 116.f;
		S.FullLeafDay = 140.f;
		S.ColourStartDay = 292.f;
		S.ColourFullDay = 315.f;
		S.LeafDropStartDay = 302.f;
		S.BareDay = 336.f;
		S.SpringLeafColour = C(0.16f, 0.3f, 0.05f);
		S.SummerLeafColour = C(0.03f, 0.09f, 0.02f);
		S.AutumnLeafColour = C(0.055f, 0.1f, 0.025f);
		S.AutumnLeafColourLate = C(0.1f, 0.12f, 0.03f);
		S.LifeMedian = 90.f;
		S.LifeSpread = 0.3f;
		S.LifeMax = 160.f;
		S.DeclineYears = 12.f;
		S.SnagYears = 10.f;
		return S;
	}

	/**
	 * Sorbus aucuparia, rowan (рябина): a slender tree of 8-12 m with smooth grey bark and a loose, oval crown.
	 * Compound leaves of 13 leaflets, red and orange in autumn. Flat heads of small berries ripen orange-red
	 * in September and hang into the new year, until the birds have had them.
	 */
	FPlantSpecies Rowan()
	{
		FPlantSpecies S;
		S.Name = TEXT("Rowan");
		S.MaxHeight = 11.f;
		S.GrowthRate = 0.1f;
		S.GrowthShape = 1.3f;
		S.LeaderFraction = 0.55f;
		S.TrunkRadiusPerHeight = 0.012f;
		S.RingWidth = 0.0015f;
		S.CrownBase = 0.2f;
		S.TrunkNodeSpacing = 0.3f;
		S.Branch1Angle = 50.f;
		S.Branch1MaxLength = 4.f;
		S.Branch1Profile = FVector(0.7, 1.0, 0.6);
		S.LengthTau = 7.f;
		S.Tropism = 0.05f;
		S.Wobble = 0.2f;
		S.Branch2Spacing = 0.18f;
		S.Branch2Angle = 55.f;
		S.Branch2MaxLength = 1.4f;
		S.Branch2LengthRatio = 0.4f;
		S.LeafyTipLength = 2.5f;
		S.LeafSpacing = 0.13f;
		S.LeafClusterSize = 0.5f;
		// a pinnate leaf as one blade: rounded leaflets down both sides of the rachis
		S.Leaf.Length = 0.18f;
		S.Leaf.Width = 0.42f;
		S.Leaf.WidestAt = 0.5f;
		S.Leaf.Lobes = 7;
		S.Leaf.LobeDepth = 0.85f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.15f;
		S.Leaf.Fold = 0.12f;
		S.Leaf.PerCluster = 4;
		S.Bark.Pattern = EPlantBarkPattern::Smooth;
		S.Bark.TileWidth = 0.15f;
		S.Bark.TileHeight = 0.4f;
		S.Bark.Depth = 0.6f;
		S.Bark.RidgeColour = C(0.13f, 0.11f, 0.095f);
		S.Bark.FurrowColour = C(0.05f, 0.04f, 0.035f);
		S.BudBreakDay = 112.f;
		S.FullLeafDay = 135.f;
		S.ColourStartDay = 268.f;
		S.ColourFullDay = 292.f;
		S.LeafDropStartDay = 285.f;
		S.BareDay = 315.f;
		S.SummerLeafColour = C(0.045f, 0.12f, 0.03f);
		S.AutumnLeafColour = C(0.5f, 0.1f, 0.02f);
		S.AutumnLeafColourLate = C(0.55f, 0.3f, 0.03f);
		S.FruitSize = 0.0095f;
		S.FruitCluster = 14;
		S.FruitChance = 0.45f;
		S.FruitingAge = 5.f;
		S.FruitSetDay = 150.f;
		S.FruitRipeDay = 240.f;
		S.FruitFallDay = 385.f;
		S.FruitUnripeColour = C(0.12f, 0.25f, 0.04f);
		S.FruitGroundColour = C(0.6f, 0.13f, 0.012f);
		S.FruitColour = C(0.6f, 0.13f, 0.012f);
		S.LifeMedian = 80.f;
		S.LifeSpread = 0.3f;
		S.LifeMax = 150.f;
		S.DeclineYears = 10.f;
		S.SnagYears = 8.f;
		return S;
	}

	/**
	 * Hippophae rhamnoides, sea buckthorn (облепиха крушиновая), the thorny thicket that holds the dunes
	 * together: a dense shrub of 3-5 m, twisted twigs ending in thorns, narrow silver-green leaves that go
	 * olive and drop without a show. Bright orange berries sit packed along the bare twigs through the winter.
	 */
	FPlantSpecies SeaBuckthorn()
	{
		FPlantSpecies S;
		S.Name = TEXT("SeaBuckthorn");
		S.MaxHeight = 4.5f;
		S.GrowthRate = 0.18f;
		S.GrowthShape = 1.2f;
		S.LeaderFraction = 0.4f;
		S.TrunkRadiusPerHeight = 0.015f;
		S.RingWidth = 0.002f;
		S.MinRadius = 0.01f;
		S.CrownBase = 0.03f;
		S.TrunkNodeSpacing = 0.15f;
		S.Branch1Angle = 48.f;
		S.Branch1MaxLength = 2.4f;
		S.Branch1Profile = FVector(0.9, 1.0, 0.8);
		S.LengthTau = 5.f;
		S.Tropism = 0.04f;
		S.Wobble = 0.3f;
		S.Kink = 0.4f;
		S.KinkSpacing = 0.35f;
		S.Branch2Spacing = 0.1f;
		S.Branch2Angle = 60.f;
		S.Branch2MaxLength = 0.9f;
		S.Branch2LengthRatio = 0.5f;
		S.Branch2ShadeDepth = 2.5f;
		S.BranchRadiusPerLength = 0.01f;
		S.LeafyTipLength = 3.f;
		S.LeafSpacing = 0.045f;
		S.LeafClusterSize = 0.12f;
		S.Sway = 6.f;
		S.Leaf.Length = 0.065f;
		S.Leaf.Width = 0.1f;
		S.Leaf.WidestAt = 0.5f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.05f;
		S.Leaf.Fold = 0.2f;
		S.Leaf.PerCluster = 8;
		S.Bark.Pattern = EPlantBarkPattern::Furrows;
		S.Bark.TileWidth = 0.1f;
		S.Bark.TileHeight = 0.3f;
		S.Bark.Depth = 1.4f;
		S.Bark.RidgeColour = C(0.075f, 0.062f, 0.05f);
		S.Bark.FurrowColour = C(0.015f, 0.012f, 0.01f);
		S.BudBreakDay = 105.f;
		S.FullLeafDay = 130.f;
		S.ColourStartDay = 285.f;
		S.ColourFullDay = 305.f;
		S.LeafDropStartDay = 295.f;
		S.BareDay = 335.f;
		S.SpringLeafColour = C(0.13f, 0.22f, 0.09f);
		S.SummerLeafColour = C(0.09f, 0.15f, 0.08f);
		S.AutumnLeafColour = C(0.18f, 0.19f, 0.08f);
		S.AutumnLeafColourLate = C(0.18f, 0.19f, 0.08f);
		S.FruitSize = 0.008f;
		S.FruitCluster = 6;
		S.FruitChance = 0.85f;
		S.FruitingAge = 4.f;
		S.FruitSetDay = 170.f;
		S.FruitRipeDay = 245.f;
		S.FruitFallDay = 420.f;
		S.FruitUnripeColour = C(0.2f, 0.3f, 0.05f);
		S.FruitGroundColour = C(0.9f, 0.22f, 0.008f);
		S.FruitColour = C(0.9f, 0.22f, 0.008f);
		S.LifeMedian = 80.f;
		S.LifeSpread = 0.3f;
		S.LifeMax = 160.f;
		S.DeclineYears = 12.f;
		S.SnagYears = 6.f;
		return S;
	}

	/**
	 * Juniperus communis, common juniper (можжевельник обыкновенный): a dense, dark column or shrub of 2-5 m
	 * in the dune heath. Prickly blue-green needles in whorls of three, kept all year; reddish bark that
	 * shreds in strips; berry-cones green the first year, blue-black the second.
	 */
	FPlantSpecies Juniper()
	{
		FPlantSpecies S;
		S.Name = TEXT("Juniper");
		S.bEvergreen = true;
		S.bConicalCrown = true;
		S.MaxHeight = 4.f;
		S.GrowthRate = 0.08f;
		S.GrowthShape = 1.3f;
		S.TrunkRadiusPerHeight = 0.025f;
		S.RingWidth = 0.002f;
		S.MinRadius = 0.01f;
		S.CrownBase = 0.03f;
		S.TrunkNodeSpacing = 0.15f;
		S.WhorlSize = 3;
		S.Branch1Angle = 35.f;
		S.Branch1AngleTop = 25.f;
		S.Branch1MaxLength = 1.4f;
		S.Branch1Profile = FVector(0.9, 1.0, 0.5);
		S.LengthTau = 6.f;
		S.Tropism = 0.06f;
		S.Wobble = 0.25f;
		S.Branch2Spacing = 0.1f;
		S.Branch2Angle = 45.f;
		S.Branch2MaxLength = 0.6f;
		S.Branch2LengthRatio = 0.45f;
		S.Branch2ShadeDepth = 3.f;
		S.BranchRadiusPerLength = 0.01f;
		S.LeafyTipLength = 3.f;
		S.LeafSpacing = 0.1f;
		S.LeafClusterSize = 0.2f;
		S.Sway = 2.f;
		S.Leaf.Form = EPlantLeafForm::Needle;
		S.Leaf.Length = 0.16f;
		S.Leaf.Width = 0.9f;
		S.Leaf.Lobes = 3;
		S.Leaf.PerCluster = 18;
		S.Leaf.Cards = 3;
		S.Bark.Pattern = EPlantBarkPattern::Furrows;
		S.Bark.TileWidth = 0.1f;
		S.Bark.TileHeight = 0.4f;
		S.Bark.Depth = 1.6f;
		S.Bark.RidgeColour = C(0.14f, 0.06f, 0.032f);
		S.Bark.FurrowColour = C(0.035f, 0.016f, 0.01f);
		S.BudBreakDay = 120.f;
		S.FullLeafDay = 150.f;
		S.ColourStartDay = 290.f;
		S.ColourFullDay = 330.f;
		S.SpringLeafColour = C(0.07f, 0.15f, 0.08f);
		S.SummerLeafColour = C(0.035f, 0.09f, 0.07f);
		S.AutumnLeafColour = C(0.035f, 0.06f, 0.04f);
		S.AutumnLeafColourLate = C(0.035f, 0.06f, 0.04f);
		S.FruitSize = 0.009f;
		S.FruitCluster = 3;
		S.FruitChance = 0.5f;
		S.FruitingAge = 6.f;
		S.FruitSetDay = 120.f;
		S.FruitRipeDay = 265.f;
		S.FruitFallDay = 420.f;
		S.FruitUnripeColour = C(0.05f, 0.15f, 0.04f);
		S.FruitGroundColour = C(0.03f, 0.035f, 0.07f);
		S.FruitColour = C(0.03f, 0.035f, 0.07f);
		S.LifeMedian = 120.f;
		S.LifeSpread = 0.3f;
		S.LifeMax = 300.f;
		S.DeclineYears = 15.f;
		S.SnagYears = 20.f;
		return S;
	}

	/**
	 * Populus tremula, aspen (осина): a straight, fast pioneer with smooth green-grey bark, black diamond
	 * scars and a light, rounded crown. Round leaves on long flattened stalks tremble in the faintest air;
	 * they turn clear yellow to red-orange.
	 */
	FPlantSpecies Aspen()
	{
		FPlantSpecies S;
		S.Name = TEXT("Aspen");
		S.MaxHeight = 24.f;
		S.GrowthRate = 0.085f;
		S.GrowthShape = 1.4f;
		S.LeaderFraction = 0.9f;
		S.TrunkRadiusPerHeight = 0.01f;
		S.RingWidth = 0.0018f;
		S.CrownBase = 0.35f;
		S.TrunkNodeSpacing = 0.4f;
		S.Branch1Angle = 48.f;
		S.Branch1AngleTop = 30.f;
		S.Branch1MaxLength = 4.5f;
		S.Branch1Profile = FVector(0.6, 1.0, 0.4);
		S.LengthTau = 8.f;
		S.Tropism = 0.06f;
		S.Wobble = 0.14f;
		S.Branch2Spacing = 0.22f;
		S.Branch2MaxLength = 1.7f;
		S.Branch2LengthRatio = 0.4f;
		S.LeafyTipLength = 3.f;
		S.LeafSpacing = 0.11f;
		S.LeafClusterSize = 0.4f;
		S.Sway = 9.f;
		S.Leaf.Length = 0.075f;
		S.Leaf.Width = 1.0f;
		S.Leaf.WidestAt = 0.5f;
		S.Leaf.Teeth = 12;
		S.Leaf.TeethDepth = 0.05f;
		S.Leaf.Petiole = 1.1f;
		S.Leaf.Fold = 0.05f;
		S.Leaf.PerCluster = 6;
		S.Bark.Pattern = EPlantBarkPattern::Smooth;
		S.Bark.TileWidth = 0.2f;
		S.Bark.TileHeight = 0.5f;
		S.Bark.Depth = 0.8f;
		S.Bark.RidgeColour = C(0.17f, 0.18f, 0.12f);
		S.Bark.FurrowColour = C(0.06f, 0.06f, 0.04f);
		S.Bark.DarkBaseRate = 0.25f;
		S.Bark.DarkBaseMax = 3.f;
		S.Bark.BaseRidgeColour = C(0.06f, 0.055f, 0.045f);
		S.Bark.BaseFurrowColour = C(0.012f, 0.011f, 0.01f);
		S.BudBreakDay = 114.f;
		S.FullLeafDay = 138.f;
		S.ColourStartDay = 270.f;
		S.ColourFullDay = 294.f;
		S.LeafDropStartDay = 282.f;
		S.BareDay = 312.f;
		S.SummerLeafColour = C(0.045f, 0.13f, 0.03f);
		S.AutumnLeafColour = C(0.6f, 0.3f, 0.03f);
		S.AutumnLeafColourLate = C(0.62f, 0.45f, 0.04f);
		S.LifeMedian = 90.f;
		S.LifeSpread = 0.3f;
		S.LifeMax = 200.f;
		S.DeclineYears = 12.f;
		S.SnagYears = 12.f;
		return S;
	}
}

const TArray<FPlantSpecies>& PlantSpeciesLibrary::All()
{
	static const TArray<FPlantSpecies> Species = { PyramidalPoplar(), BlackPoplar(), Apple(), Maple(), SwampWhiteOak(),
		SilverBirch(), DownyBirch(), WeepingBirch(), DwarfBirch(), StoneBirch(),
		ScotsPine(), DunePine(), NorwaySpruce(), BlackAlder(), Rowan(), SeaBuckthorn(), Juniper(), Aspen() };
	return Species;
}

const FPlantSpecies* PlantSpeciesLibrary::Find(FName Name)
{
	return All().FindByPredicate([Name](const FPlantSpecies& S) { return S.Name == Name; });
}

FText PlantSpeciesLibrary::DisplayName(const FPlantSpecies& S)
{
	return LyudText::Get(FString::Printf(TEXT("plant.%s.name"), *S.Name.ToString().ToLower()));
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
