#include "Plants/PlantHerb.h"

// The North's own plants (docs/north.md "Plants"): the Kamchatka-like volcano country above the stone birch, where the
// shrubs take over from the trees and the meadows flower late. Three shrubs drawn by the forests (AProceduralForest bakes,
// scattered by the thousand over open ground, so they are built cheap: few, long shoots and an early summary) and
// four herbs drawn by AProceduralGrass herb patches near the camera. Their flowering is set here, not in ApplyBlossom.
// Below them the river valley: two willows (forest bakes) and three herbs of the terraces and the wet gravel.
//
// The game calendar is the coast's (snow on the ground from late November to the March thaw); the North keeps it but
// everything here wakes three to five weeks later than on the coast, as it does under the late snow up there.

namespace
{
	FLinearColor C(float R, float G, float B) { return FLinearColor(R, G, B); }

	/**
	 * Pinus pumila, Siberian dwarf pine (кедровый стланик): the shrub that makes the belt above the stone birch. A dozen
	 * stems from one root, lying on the ground at their base and curving up at their ends into a dense, flat-topped thicket
	 * 1.5-3 m high and up to 6 m across. Five needles to a bundle, blue-green, crowded in brushes at the shoot tips; thin
	 * grey-brown bark. Purple male cones in early July; small violet cones, ripe and brown in the second September,
	 * taken by the nutcrackers before the snow.
	 */
	FPlantSpecies SiberianDwarfPine()
	{
		FPlantSpecies S;
		S.Name = TEXT("SiberianDwarfPine");
		S.bEvergreen = true;
		S.MaxHeight = 2.6f;
		S.GrowthRate = 0.05f;
		S.GrowthShape = 1.3f;
		// a short stool from which the stems leave nearly flat, two at a node
		S.LeaderFraction = 0.2f;
		S.TrunkRadiusPerHeight = 0.03f;
		S.RingWidth = 0.001f;
		S.MinRadius = 0.01f;
		S.CrownBase = 0.f;
		S.CrownWidth = 2.3f;
		S.TrunkNodeSpacing = 0.11f;
		S.WhorlSize = 2;
		S.Branch1Angle = 76.f;
		S.Branch1AngleTop = 58.f;
		S.Branch1MaxLength = 3.f;
		S.Branch1Profile = FVector(1.0, 0.95, 0.6);
		S.LengthTau = 12.f;
		// prostrate, then ascending
		S.Tropism = 0.32f;
		S.Wobble = 0.25f;
		S.Kink = 0.3f;
		S.KinkSpacing = 0.45f;
		S.Branch2Spacing = 0.2f;
		S.Branch2Angle = 55.f;
		S.Branch2MaxLength = 0.6f;
		S.Branch2LengthRatio = 0.35f;
		S.Branch2ShadeDepth = 1.3f;
		S.BranchRadiusBase = 0.004f;
		S.BranchRadiusPerLength = 0.01f;
		// brushes of needles only on the last two or three years' shoots
		S.LeafyTipLength = 0.8f;
		S.LeafSpacing = 0.08f;
		S.LeafClusterSize = 0.3f;
		S.Leaf.Form = EPlantLeafForm::Needle;
		S.Leaf.Length = 0.2f;
		S.Leaf.Width = 0.9f;
		S.Leaf.Lobes = 5;
		S.Leaf.PerCluster = 25;
		S.Leaf.Cards = 3;
		S.Bark.Pattern = EPlantBarkPattern::Plates;
		S.Bark.TileWidth = 0.08f;
		S.Bark.TileHeight = 0.16f;
		S.Bark.Depth = 0.8f;
		S.Bark.RidgeColour = C(0.13f, 0.1f, 0.08f);
		S.Bark.FurrowColour = C(0.05f, 0.04f, 0.03f);
		S.Bark.FlakeChance = 0.05f;
		S.Bark.UnderColour = C(0.2f, 0.09f, 0.05f);
		S.BudBreakDay = 150.f;
		S.FullLeafDay = 185.f;
		S.ColourStartDay = 280.f;
		S.ColourFullDay = 320.f;
		S.SpringLeafColour = C(0.12f, 0.22f, 0.08f);
		S.SummerLeafColour = C(0.04f, 0.1f, 0.075f);
		S.AutumnLeafColour = C(0.045f, 0.075f, 0.05f);
		S.AutumnLeafColourLate = C(0.045f, 0.075f, 0.05f);
		// cones in twos and threes at the shoot tips, gone (eaten) by November
		S.FruitSize = 0.04f;
		S.FruitAspect = 1.3f;
		S.FruitCluster = 3;
		S.FruitChance = 0.12f;
		S.FruitingAge = 20.f;
		S.FruitSetDay = 160.f;
		S.FruitRipeDay = 255.f;
		S.FruitFallDay = 300.f;
		S.FruitUnripeColour = C(0.12f, 0.05f, 0.1f);
		S.FruitGroundColour = C(0.14f, 0.07f, 0.035f);
		S.FruitColour = C(0.14f, 0.07f, 0.035f);
		S.FruitBlush = 0.f;
		FPlantBlossom& B = S.Blossom;
		B.Form = EPlantBlossomForm::Cone;
		B.StartDay = 172.f; B.PeakDay = 185.f; B.EndDay = 200.f;
		B.Size = 0.012f; B.Thickness = 0.5f; B.PerCluster = 6; B.Lift = 0.03f;
		B.Chance = 0.5f; B.PerTwig = 1; B.MinAge = 15.f;
		B.FreshColour = C(0.45f, 0.05f, 0.12f); B.AgedColour = C(0.2f, 0.12f, 0.04f);
		B.FarTint = 0.25f;
		S.LifeMedian = 200.f;
		S.LifeSpread = 0.25f;
		S.LifeMax = 260.f;
		S.DeclineYears = 20.f;
		S.SnagYears = 10.f;
		S.Sway = 3.f;
		S.WindBend = 0.03f;
		return S;
	}

	/**
	 * Alnus alnobetula subsp. fruticosa, Siberian dwarf alder (ольховник, ольха кустарниковая): thickets 2-3 m high in the
	 * gullies and on wet slopes where the snow lies long, many stems from the base, bent down there by the snow and curving
	 * up like sabres. Glossy, finely toothed oval leaves; yellow pollen catkins in June as the leaves unfold; small woody
	 * cones in clusters, brown from September and on the bare twigs all winter. The leaves fall dull green-yellow.
	 */
	FPlantSpecies DwarfAlder()
	{
		FPlantSpecies S;
		S.Name = TEXT("DwarfAlder");
		S.MaxHeight = 2.8f;
		S.GrowthRate = 0.12f;
		S.GrowthShape = 1.2f;
		S.LeaderFraction = 0.15f;
		S.TrunkRadiusPerHeight = 0.02f;
		S.RingWidth = 0.0015f;
		S.MinRadius = 0.006f;
		S.CrownBase = 0.f;
		S.CrownWidth = 1.4f;
		S.TrunkNodeSpacing = 0.1f;
		S.WhorlSize = 2;
		S.Branch1Angle = 42.f;
		S.Branch1AngleTop = 25.f;
		S.Branch1MaxLength = 3.f;
		S.Branch1Profile = FVector(1.0, 1.0, 0.8);
		S.LengthTau = 6.f;
		S.Tropism = 0.12f;
		S.Wobble = 0.25f;
		S.Kink = 0.2f;
		S.KinkSpacing = 0.5f;
		S.Droop = 0.04f;
		S.Branch2Spacing = 0.24f;
		S.Branch2Angle = 50.f;
		S.Branch2MaxLength = 0.9f;
		S.Branch2LengthRatio = 0.4f;
		S.Branch2ShadeDepth = 1.6f;
		S.BranchRadiusBase = 0.003f;
		S.BranchRadiusPerLength = 0.012f;
		S.LeafyTipLength = 1.2f;
		S.LeafSpacing = 0.1f;
		S.LeafClusterSize = 0.3f;
		S.Leaf.Length = 0.065f;
		S.Leaf.Width = 0.7f;
		S.Leaf.WidestAt = 0.45f;
		S.Leaf.Teeth = 16;
		S.Leaf.TeethDepth = 0.03f;
		S.Leaf.Petiole = 0.2f;
		S.Leaf.Fold = 0.1f;
		S.Leaf.PerCluster = 5;
		S.Bark.Pattern = EPlantBarkPattern::Smooth;
		S.Bark.TileWidth = 0.08f;
		S.Bark.TileHeight = 0.25f;
		S.Bark.Depth = 0.5f;
		S.Bark.RidgeColour = C(0.11f, 0.1f, 0.085f);
		S.Bark.FurrowColour = C(0.04f, 0.035f, 0.03f);
		S.BudBreakDay = 145.f;
		S.FullLeafDay = 168.f;
		S.ColourStartDay = 255.f;
		S.ColourFullDay = 280.f;
		S.LeafDropStartDay = 268.f;
		S.BareDay = 296.f;
		S.SpringLeafColour = C(0.14f, 0.28f, 0.05f);
		S.SummerLeafColour = C(0.035f, 0.1f, 0.02f);
		S.AutumnLeafColour = C(0.12f, 0.14f, 0.03f);
		S.AutumnLeafColourLate = C(0.25f, 0.22f, 0.05f);
		// the cones hang on into the next May
		S.FruitSize = 0.013f;
		S.FruitAspect = 1.4f;
		S.FruitCluster = 4;
		S.FruitChance = 0.35f;
		S.FruitingAge = 6.f;
		S.FruitSetDay = 170.f;
		S.FruitRipeDay = 255.f;
		S.FruitFallDay = 500.f;
		S.FruitUnripeColour = C(0.08f, 0.16f, 0.04f);
		S.FruitGroundColour = C(0.06f, 0.035f, 0.02f);
		S.FruitColour = C(0.06f, 0.035f, 0.02f);
		S.FruitBlush = 0.f;
		FPlantBlossom& B = S.Blossom;
		B.Form = EPlantBlossomForm::Catkin;
		B.StartDay = 150.f; B.PeakDay = 162.f; B.EndDay = 180.f;
		B.Size = 0.06f; B.Thickness = 0.12f; B.PerCluster = 2;
		B.Chance = 0.6f; B.PerTwig = 3; B.MinAge = 4.f;
		B.FreshColour = C(0.35f, 0.25f, 0.05f); B.AgedColour = C(0.12f, 0.07f, 0.03f);
		B.FarTint = 0.35f;
		S.LifeMedian = 110.f;
		S.LifeSpread = 0.2f;
		S.LifeMax = 160.f;
		S.DeclineYears = 10.f;
		S.SnagYears = 4.f;
		S.Sway = 5.f;
		S.WindBend = 0.08f;
		return S;
	}

	/**
	 * Rhododendron aureum, golden rhododendron (рододендрон золотистый): knee-high evergreen mats of creeping stems by
	 * the snowfields, leathery dark leaves in rosettes at the shoot ends; umbels of pale yellow bells open as the snow
	 * goes, in June and early July. In the cold the leaves roll up and go olive-brown.
	 */
	FPlantSpecies GoldenRhododendron()
	{
		FPlantSpecies S;
		S.Name = TEXT("GoldenRhododendron");
		S.bEvergreen = true;
		S.MaxHeight = 0.55f;
		S.GrowthRate = 0.1f;
		S.GrowthShape = 1.2f;
		S.LeaderFraction = 0.2f;
		S.TrunkRadiusPerHeight = 0.03f;
		S.MinRadius = 0.004f;
		S.CrownBase = 0.f;
		S.CrownWidth = 3.f;
		S.TrunkNodeSpacing = 0.025f;
		S.WhorlSize = 2;
		S.Branch1Angle = 78.f;
		S.Branch1AngleTop = 60.f;
		S.Branch1MaxLength = 0.9f;
		S.Branch1Profile = FVector(1.0, 1.0, 0.8);
		S.LengthTau = 6.f;
		S.Tropism = 0.5f;
		S.Wobble = 0.4f;
		S.Kink = 0.35f;
		S.KinkSpacing = 0.12f;
		S.Branch2Spacing = 0.08f;
		S.Branch2Angle = 45.f;
		S.Branch2MaxLength = 0.25f;
		S.Branch2LengthRatio = 0.5f;
		S.Branch2ShadeDepth = 0.4f;
		S.BranchRadiusBase = 0.002f;
		S.BranchRadiusPerLength = 0.01f;
		// leaves crowded at the shoot ends
		S.LeafyTipLength = 0.25f;
		S.LeafSpacing = 0.05f;
		S.LeafClusterSize = 0.1f;
		S.Leaf.Length = 0.06f;
		S.Leaf.Width = 0.42f;
		S.Leaf.WidestAt = 0.62f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.12f;
		S.Leaf.Fold = 0.15f;
		S.Leaf.PerCluster = 6;
		S.Bark.Pattern = EPlantBarkPattern::Smooth;
		S.Bark.TileWidth = 0.04f;
		S.Bark.TileHeight = 0.1f;
		S.Bark.Depth = 0.3f;
		S.Bark.RidgeColour = C(0.1f, 0.07f, 0.05f);
		S.Bark.FurrowColour = C(0.035f, 0.025f, 0.02f);
		S.BudBreakDay = 160.f;
		S.FullLeafDay = 190.f;
		S.ColourStartDay = 275.f;
		S.ColourFullDay = 315.f;
		S.SpringLeafColour = C(0.12f, 0.24f, 0.06f);
		S.SummerLeafColour = C(0.03f, 0.08f, 0.025f);
		S.AutumnLeafColour = C(0.05f, 0.06f, 0.025f);
		S.AutumnLeafColourLate = C(0.07f, 0.06f, 0.025f);
		FPlantBlossom& B = S.Blossom;
		B.Form = EPlantBlossomForm::Blossom;
		B.StartDay = 158.f; B.PeakDay = 172.f; B.EndDay = 192.f;
		B.Size = 0.035f; B.PerCluster = 6; B.Petals = 5; B.Spread = 1.6f; B.Lift = 0.04f;
		B.Chance = 0.85f; B.PerTwig = 1; B.MinAge = 4.f;
		B.FreshColour = C(0.9f, 0.8f, 0.42f); B.CentreColour = C(0.6f, 0.5f, 0.1f); B.AgedColour = C(0.6f, 0.5f, 0.35f);
		B.FarTint = 0.55f;
		S.LifeMedian = 110.f;
		S.LifeSpread = 0.2f;
		S.LifeMax = 160.f;
		S.DeclineYears = 10.f;
		S.SnagYears = 3.f;
		S.Sway = 1.5f;
		S.WindBend = 0.02f;
		return S;
	}

	/** A herb of the North's short summer: up in late May or June, turned in September, gone by mid-October. */
	void NorthPerennial(FPlantSpecies& S, float Up, float Turn)
	{
		S.BudBreakDay = Up;
		S.FullLeafDay = Up + 25.f;
		S.ColourStartDay = Turn - 22.f;
		S.ColourFullDay = Turn;
		S.LeafDropStartDay = Turn + 2.f;
		S.BareDay = Turn + 28.f;
		S.LitterDays = 10.f;
		S.PhenologyJitter = 6.f;
	}

	/**
	 * Chamaenerion angustifolium, fireweed (иван-чай): the colour of the volcanic ground, old burns and lava edges. Clumps
	 * of unbranched stems 1-1.5 m, narrow willow-like leaves all the way up, a spike of four-petalled magenta flowers opening
	 * from the bottom up in July and August; the leaves go crimson and orange in September and the stems die back.
	 */
	FPlantSpecies Fireweed()
	{
		FPlantSpecies S;
		S.Name = TEXT("Fireweed");
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Bush;
		H.Height = 1.3f;
		H.Spread = 0.5f;
		H.MatureYears = 2.f;
		H.Stems = 4;
		H.Branches = 0;
		H.Lean = 6.f;
		H.StemRadius = 0.004f;
		H.BareStem = 0.1f;
		H.Leaves = 14;
		H.Rise = FVector2D(40.0, 70.0);
		H.Arch = 20.f;
		H.StemColour = C(0.13f, 0.05f, 0.04f);
		// the spike: one at each tip and the rest on short stalks round the top of the stem
		H.Flower = EPlantFlowerForm::Five;
		H.FlowerSize = 0.03f;
		H.Flowers = 10;
		H.FlowerColours = { C(0.55f, 0.04f, 0.28f), C(0.62f, 0.07f, 0.36f), C(0.5f, 0.03f, 0.22f) };
		H.CentreColour = C(0.55f, 0.35f, 0.45f);
		H.BloomStart = 192.f;
		H.BloomEnd = 242.f;
		S.Leaf.Length = 0.12f;
		S.Leaf.Width = 0.16f;
		S.Leaf.WidestAt = 0.45f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.02f;
		S.Leaf.Fold = 0.15f;
		NorthPerennial(S, 150.f, 268.f);
		S.SpringLeafColour = C(0.1f, 0.18f, 0.05f);
		S.SummerLeafColour = C(0.05f, 0.11f, 0.03f);
		S.AutumnLeafColour = C(0.45f, 0.04f, 0.04f);
		S.AutumnLeafColourLate = C(0.5f, 0.18f, 0.03f);
		S.LifeMedian = 15.f;
		S.LifeMax = 30.f;
		return S;
	}

	/**
	 * Dryas octopetala, mountain avens (дриада восьмилепестная): low evergreen mats on cinder, gravel and old lava, small
	 * scalloped leaves, white eight-petalled flowers with a golden boss on short stalks in late June and July.
	 */
	FPlantSpecies MountainAvens()
	{
		FPlantSpecies S;
		S.Name = TEXT("MountainAvens");
		S.bEvergreen = true;
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Rosette;
		H.bWoody = true;
		H.Height = 0.03f;
		H.Spread = 0.3f;
		H.MatureYears = 6.f;
		H.Leaves = 24;
		H.Fans = 3;
		H.Rise = FVector2D(70.0, 86.0);
		H.Arch = 10.f;
		H.StemColour = C(0.08f, 0.05f, 0.03f);
		H.Flower = EPlantFlowerForm::Daisy;
		H.Petals = 8;
		H.FlowerSize = 0.035f;
		H.Flowers = 4;
		H.ScapeHeight = 0.07f;
		H.FlowerColours = { C(0.88f, 0.88f, 0.82f), C(0.85f, 0.86f, 0.8f) };
		H.CentreColour = C(0.7f, 0.5f, 0.05f);
		H.BloomStart = 168.f;
		H.BloomEnd = 205.f;
		S.Leaf.Length = 0.035f;
		S.Leaf.Width = 0.38f;
		S.Leaf.WidestAt = 0.4f;
		S.Leaf.Teeth = 5;
		S.Leaf.TeethDepth = 0.18f;
		S.Leaf.Petiole = 0.1f;
		S.Leaf.Fold = 0.1f;
		S.BudBreakDay = 150.f;
		S.FullLeafDay = 175.f;
		S.ColourStartDay = 270.f;
		S.ColourFullDay = 310.f;
		S.SpringLeafColour = C(0.1f, 0.18f, 0.05f);
		S.SummerLeafColour = C(0.04f, 0.08f, 0.03f);
		S.AutumnLeafColour = C(0.08f, 0.06f, 0.025f);
		S.AutumnLeafColourLate = C(0.08f, 0.06f, 0.025f);
		S.LifeMedian = 50.f;
		S.LifeMax = 100.f;
		return S;
	}

	/**
	 * Eriophorum vaginatum, hare's-tail cottongrass (пушица влагалищная): dense tussocks of thin arching leaves in wet
	 * hollows and by the melt runnels, white cotton heads on stiff stalks from late June into August; russet in the autumn.
	 */
	FPlantSpecies CottonGrass()
	{
		FPlantSpecies S;
		S.Name = TEXT("CottonGrass");
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Rosette;
		H.Height = 0.2f;
		H.Spread = 0.3f;
		H.MatureYears = 3.f;
		H.Leaves = 14;
		H.Rise = FVector2D(10.0, 40.0);
		H.Arch = 50.f;
		H.Twist = 30.f;
		H.ProfilePower = 0.5f;
		H.StemColour = C(0.08f, 0.12f, 0.04f);
		H.Flower = EPlantFlowerForm::Pompon;
		H.FlowerSize = 0.035f;
		H.Flowers = 3;
		H.ScapeHeight = 0.2f;
		H.FlowerColours = { C(0.92f, 0.92f, 0.9f), C(0.85f, 0.86f, 0.82f) };
		H.CentreColour = C(0.9f, 0.9f, 0.88f);
		H.BloomStart = 175.f;
		H.BloomEnd = 228.f;
		S.Leaf.Length = 0.3f;
		S.Leaf.Width = 0.03f;
		S.Leaf.WidestAt = 0.15f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.f;
		S.Leaf.Fold = 0.2f;
		NorthPerennial(S, 140.f, 265.f);
		S.SpringLeafColour = C(0.1f, 0.18f, 0.05f);
		S.SummerLeafColour = C(0.06f, 0.12f, 0.04f);
		S.AutumnLeafColour = C(0.35f, 0.2f, 0.06f);
		S.AutumnLeafColourLate = C(0.4f, 0.28f, 0.1f);
		S.LifeMedian = 20.f;
		S.LifeMax = 40.f;
		return S;
	}

	// ---- The river valley (docs/north.md "Plants", "River valley"): the braided gravel river, its bars, islands and
	// terraces, as on Kamchatka's Avacha. Willows on the gravel (forest bakes), the tall-herb meadow (шеломайник) on the
	// terraces and sedge at the wet edges (herb patches).

	/**
	 * Chosenia arbutifolia (Salix arbutifolia), Sakhalin willow (чозения толокнянколистная): the tree of Kamchatka's
	 * gravel bars, first on new gravel after a flood, in tall narrow groves along the channels. One straight grey trunk,
	 * 8-15 m, ascending limbs and a narrow, open crown; narrow pale blue-green leaves; the young shoots red under a bluish
	 * bloom, bright against the snow. Small hanging catkins in early June with the leaves; the leaves fall yellow early in
	 * the autumn. Fast-growing, short-lived for a tree (a century).
	 */
	FPlantSpecies SakhalinWillow()
	{
		FPlantSpecies S;
		S.Name = TEXT("SakhalinWillow");
		S.MaxHeight = 14.f;
		S.GrowthRate = 0.07f;
		S.GrowthShape = 1.3f;
		S.LeaderFraction = 0.72f;
		S.TrunkRadiusPerHeight = 0.014f;
		S.RingWidth = 0.002f;
		S.MinRadius = 0.006f;
		// the lower limbs shed early: a tall clean trunk, a narrow crown of steep limbs
		S.CrownBase = 0.3f;
		S.CrownWidth = 0.38f;
		S.TrunkNodeSpacing = 0.3f;
		S.Branch1Angle = 38.f;
		S.Branch1AngleTop = 22.f;
		S.Branch1MaxLength = 3.6f;
		S.Branch1Profile = FVector(0.45, 1.0, 0.5);
		S.LengthTau = 6.f;
		S.Tropism = 0.1f;
		S.Wobble = 0.22f;
		S.Kink = 0.2f;
		S.KinkSpacing = 0.6f;
		S.TwigHang = 0.15f;
		S.Branch2Spacing = 0.2f;
		S.Branch2Angle = 40.f;
		S.Branch2MaxLength = 1.1f;
		S.Branch2LengthRatio = 0.4f;
		S.Branch2ShadeDepth = 2.2f;
		S.TwigRadiusScale = 0.6f;
		S.BranchRadiusBase = 0.003f;
		S.BranchRadiusPerLength = 0.007f;
		S.LeafyTipLength = 2.4f;
		S.LeafSpacing = 0.09f;
		S.LeafClusterSize = 0.32f;
		// narrow lanceolate leaves, pale and glaucous
		S.Leaf.Length = 0.095f;
		S.Leaf.Width = 0.22f;
		S.Leaf.WidestAt = 0.4f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.08f;
		S.Leaf.Fold = 0.15f;
		S.Leaf.PerCluster = 7;
		S.Bark.Pattern = EPlantBarkPattern::Furrows;
		S.Bark.TileWidth = 0.12f;
		S.Bark.TileHeight = 0.5f;
		S.Bark.Depth = 0.6f;
		S.Bark.RidgeColour = C(0.22f, 0.2f, 0.17f);
		S.Bark.FurrowColour = C(0.08f, 0.07f, 0.06f);
		S.Bark.TwigRadius = 0.007f;
		S.Bark.TwigColour = C(0.24f, 0.07f, 0.05f);
		S.BudBreakDay = 148.f;
		S.FullLeafDay = 170.f;
		S.ColourStartDay = 248.f;
		S.ColourFullDay = 270.f;
		S.LeafDropStartDay = 260.f;
		S.BareDay = 290.f;
		S.SpringLeafColour = C(0.17f, 0.27f, 0.09f);
		S.SummerLeafColour = C(0.09f, 0.15f, 0.075f);
		S.AutumnLeafColour = C(0.4f, 0.36f, 0.06f);
		S.AutumnLeafColourLate = C(0.5f, 0.36f, 0.07f);
		FPlantBlossom& B = S.Blossom;
		B.Form = EPlantBlossomForm::Catkin;
		B.StartDay = 145.f; B.PeakDay = 156.f; B.EndDay = 170.f;
		B.Size = 0.035f; B.Thickness = 0.22f; B.PerCluster = 2;
		B.Chance = 0.35f; B.PerTwig = 2; B.MinAge = 8.f;
		B.FreshColour = C(0.5f, 0.45f, 0.14f); B.AgedColour = C(0.15f, 0.12f, 0.05f);
		B.FarTint = 0.25f;
		S.LifeMedian = 90.f;
		S.LifeSpread = 0.2f;
		S.LifeMax = 150.f;
		S.DeclineYears = 12.f;
		S.SnagYears = 8.f;
		S.Sway = 9.f;
		S.WindBend = 0.12f;
		return S;
	}

	/**
	 * Shrub willows of the gravel bars (тальник: Salix udensis, S. schwerinii and their kin): thickets 1.5-3 m of many
	 * straight wands from the root, red-brown and olive, narrow leaves dark above and silvery beneath. Silver pussy
	 * catkins on the bare wands in late May, the first colour of the valley's spring; the leaves go yellow-green late.
	 * The flood bends them down and they come up again.
	 */
	FPlantSpecies BarWillow()
	{
		FPlantSpecies S;
		S.Name = TEXT("BarWillow");
		S.MaxHeight = 2.8f;
		S.GrowthRate = 0.18f;
		S.GrowthShape = 1.1f;
		S.LeaderFraction = 0.12f;
		S.TrunkRadiusPerHeight = 0.014f;
		S.RingWidth = 0.001f;
		S.MinRadius = 0.006f;
		S.CrownBase = 0.f;
		S.CrownWidth = 1.f;
		// a stool of wands, two at a node, nearly upright
		S.TrunkNodeSpacing = 0.07f;
		S.WhorlSize = 2;
		S.Branch1Angle = 30.f;
		S.Branch1AngleTop = 18.f;
		S.Branch1MaxLength = 2.7f;
		S.Branch1Profile = FVector(1.0, 0.95, 0.7);
		S.LengthTau = 4.f;
		S.Tropism = 0.18f;
		S.Wobble = 0.18f;
		S.Kink = 0.1f;
		S.KinkSpacing = 0.6f;
		S.Branch2Spacing = 0.18f;
		S.Branch2Angle = 35.f;
		S.Branch2MaxLength = 0.6f;
		S.Branch2LengthRatio = 0.35f;
		S.Branch2ShadeDepth = 1.3f;
		S.BranchRadiusBase = 0.002f;
		S.BranchRadiusPerLength = 0.005f;
		S.LeafyTipLength = 2.2f;
		S.LeafSpacing = 0.09f;
		S.LeafClusterSize = 0.26f;
		S.Leaf.Length = 0.12f;
		S.Leaf.Width = 0.2f;
		S.Leaf.WidestAt = 0.45f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.08f;
		S.Leaf.Fold = 0.2f;
		S.Leaf.PerCluster = 5;
		S.Bark.Pattern = EPlantBarkPattern::Smooth;
		S.Bark.TileWidth = 0.05f;
		S.Bark.TileHeight = 0.2f;
		S.Bark.Depth = 0.3f;
		S.Bark.RidgeColour = C(0.17f, 0.12f, 0.08f);
		S.Bark.FurrowColour = C(0.06f, 0.04f, 0.03f);
		S.Bark.TwigRadius = 0.01f;
		S.Bark.TwigColour = C(0.28f, 0.09f, 0.04f);
		S.BudBreakDay = 142.f;
		S.FullLeafDay = 165.f;
		S.ColourStartDay = 255.f;
		S.ColourFullDay = 282.f;
		S.LeafDropStartDay = 268.f;
		S.BareDay = 298.f;
		S.SpringLeafColour = C(0.15f, 0.26f, 0.07f);
		S.SummerLeafColour = C(0.065f, 0.12f, 0.05f);
		S.AutumnLeafColour = C(0.22f, 0.22f, 0.05f);
		S.AutumnLeafColourLate = C(0.38f, 0.3f, 0.06f);
		// upright silver catkins on the bare wands, before the leaves
		FPlantBlossom& B = S.Blossom;
		B.Form = EPlantBlossomForm::Cone;
		B.StartDay = 128.f; B.PeakDay = 140.f; B.EndDay = 158.f;
		B.Size = 0.034f; B.Thickness = 0.5f; B.PerCluster = 2; B.Lift = 0.01f;
		B.Chance = 0.6f; B.PerTwig = 2; B.MinAge = 3.f;
		B.FreshColour = C(0.72f, 0.7f, 0.42f); B.AgedColour = C(0.3f, 0.3f, 0.15f);
		B.FarTint = 0.4f;
		// a clump outlives its wands: new ones come up from the root
		S.LifeMedian = 110.f;
		S.LifeSpread = 0.2f;
		S.LifeMax = 160.f;
		S.DeclineYears = 6.f;
		S.SnagYears = 3.f;
		S.Sway = 7.f;
		S.WindBend = 0.1f;
		return S;
	}

	/**
	 * Filipendula camtschatica, Kamchatka meadowsweet (шеломайник, лабазник камчатский): the giant of the tall-herb
	 * meadows on the river terraces, a wall of green 2-3 m high by late July. A clump of stout stems, each with huge
	 * palmate, lobed leaves held flat from knee height to the top, the lower ones the biggest; at the top of each stem a
	 * dense fluffy plume of tiny cream flowers in July and early August. It shoots up within a month of the snow going and
	 * is flattened brown by the first snow. Built cheap, as it stands by the thousand: stalkless leaf discs and a plume of
	 * crossed cards.
	 */
	FPlantSpecies KamchatkaMeadowsweet()
	{
		FPlantSpecies S;
		S.Name = TEXT("KamchatkaMeadowsweet");
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Bush;
		H.Height = 2.4f;
		H.Spread = 1.2f;
		H.MatureYears = 3.f;
		H.Stems = 4;
		H.Branches = 0;
		H.Lean = 15.f;
		// thin enough for the four-sided tube (the far LOD's budget)
		H.StemRadius = 0.0064f;
		H.BareStem = 0.18f;
		H.Leaves = 8;
		H.Rise = FVector2D(72.0, 86.0);
		H.Arch = 15.f;
		H.StemColour = C(0.08f, 0.1f, 0.03f);
		H.Flower = EPlantFlowerForm::Plume;
		H.FlowerSize = 0.2f;
		H.Flowers = 1;
		H.FlowerColours = { C(0.8f, 0.74f, 0.5f), C(0.76f, 0.7f, 0.46f) };
		H.CentreColour = C(0.7f, 0.62f, 0.35f);
		H.BloomStart = 192.f;
		H.BloomEnd = 228.f;
		// a disc on the stem (no stalk drawn: the leaf's own stalk is short against its size)
		S.Leaf.Form = EPlantLeafForm::Palmate;
		S.Leaf.Length = 0.62f;
		S.Leaf.Width = 1.f;
		S.Leaf.Lobes = 5;
		S.Leaf.LobeDepth = 0.3f;
		S.Leaf.Petiole = 0.f;
		S.Leaf.Fold = 0.2f;
		NorthPerennial(S, 145.f, 262.f);
		S.SpringLeafColour = C(0.11f, 0.2f, 0.05f);
		S.SummerLeafColour = C(0.05f, 0.11f, 0.03f);
		S.AutumnLeafColour = C(0.3f, 0.2f, 0.05f);
		S.AutumnLeafColourLate = C(0.22f, 0.13f, 0.06f);
		S.LifeMedian = 30.f;
		S.LifeMax = 60.f;
		return S;
	}

	/**
	 * Heracleum lanatum (H. dulce), cow parsnip (борщевик сладкий, шеломайник's companion): a waist-high mass of huge,
	 * broad three-lobed leaves on long stalks from the base, and in July one to three thick hollow stalks 1.5-2 m with
	 * broad flat white umbels on their tops (drawn while it flowers).
	 */
	FPlantSpecies CowParsnip()
	{
		FPlantSpecies S;
		S.Name = TEXT("CowParsnip");
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Rosette;
		H.Height = 0.85f;
		H.Spread = 1.f;
		H.MatureYears = 3.f;
		H.Leaves = 14;
		H.Rise = FVector2D(8.0, 38.0);
		H.Arch = 40.f;
		H.StemColour = C(0.12f, 0.13f, 0.05f);
		H.Flower = EPlantFlowerForm::Disc;
		H.FlowerSize = 0.24f;
		H.Flowers = 3;
		H.ScapeHeight = 0.95f;
		H.ScapeRadius = 0.012f;
		H.FlowerColours = { C(0.85f, 0.85f, 0.8f), C(0.8f, 0.8f, 0.74f) };
		H.CentreColour = C(0.7f, 0.72f, 0.6f);
		H.BloomStart = 180.f;
		H.BloomEnd = 218.f;
		// three broad lobes, on stalks longer than the blade
		S.Leaf.Form = EPlantLeafForm::Palmate;
		S.Leaf.Length = 0.55f;
		S.Leaf.Width = 1.2f;
		S.Leaf.Lobes = 3;
		S.Leaf.LobeDepth = 0.3f;
		S.Leaf.Petiole = 1.3f;
		S.Leaf.Fold = 0.15f;
		NorthPerennial(S, 140.f, 255.f);
		S.SpringLeafColour = C(0.12f, 0.21f, 0.05f);
		S.SummerLeafColour = C(0.055f, 0.12f, 0.03f);
		S.AutumnLeafColour = C(0.35f, 0.3f, 0.07f);
		S.AutumnLeafColourLate = C(0.3f, 0.18f, 0.06f);
		S.LifeMedian = 8.f;
		S.LifeMax = 15.f;
		return S;
	}

	/**
	 * Carex (C. rhynchophysa, C. aquatilis and kin), river sedge (осока): tussocks of long arching keeled leaves,
	 * blue-green, along the wet gravel, the side channels and the spring's runnels; straw-coloured in the autumn.
	 */
	FPlantSpecies RiverSedge()
	{
		FPlantSpecies S;
		S.Name = TEXT("RiverSedge");
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Rosette;
		H.Height = 0.5f;
		H.Spread = 0.4f;
		H.MatureYears = 3.f;
		H.Leaves = 16;
		H.Rise = FVector2D(5.0, 30.0);
		H.Arch = 40.f;
		H.Twist = 25.f;
		H.ProfilePower = 0.5f;
		H.StemColour = C(0.07f, 0.1f, 0.04f);
		H.Flower = EPlantFlowerForm::None;
		S.Leaf.Length = 0.7f;
		S.Leaf.Width = 0.025f;
		S.Leaf.WidestAt = 0.2f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.f;
		S.Leaf.Fold = 0.3f;
		NorthPerennial(S, 135.f, 270.f);
		S.SpringLeafColour = C(0.1f, 0.19f, 0.06f);
		S.SummerLeafColour = C(0.06f, 0.12f, 0.06f);
		S.AutumnLeafColour = C(0.3f, 0.24f, 0.09f);
		S.AutumnLeafColourLate = C(0.38f, 0.3f, 0.14f);
		S.LifeMedian = 20.f;
		S.LifeMax = 40.f;
		return S;
	}
}

TArray<FPlantSpecies> PlantHerbs::NorthSpecies()
{
	return { SiberianDwarfPine(), DwarfAlder(), GoldenRhododendron(), Fireweed(), MountainAvens(), CottonGrass(),
	         SakhalinWillow(), BarWillow(), KamchatkaMeadowsweet(), CowParsnip(), RiverSedge() };
}
