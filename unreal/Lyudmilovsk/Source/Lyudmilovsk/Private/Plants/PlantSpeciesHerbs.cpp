#include "Plants/PlantHerb.h"

// Flowers for beds, hedge shrubs and house plants. Herbs are FPlantSpecies::Herb (PlantHerb.cpp); hedges and
// potted trees (weeping fig, lemon) are trees of the usual kind, a hedge clipped to its CrownBox.

namespace
{
	FLinearColor C(float R, float G, float B) { return FLinearColor(R, G, B); }

	/** A herbaceous perennial's year: leaves up in May, dying back after the first frosts. */
	void Perennial(FPlantSpecies& S, float Up, float Down)
	{
		S.BudBreakDay = Up;
		S.FullLeafDay = Up + 30.f;
		S.ColourStartDay = Down - 25.f;
		S.ColourFullDay = Down;
		S.LeafDropStartDay = Down - 10.f;
		S.BareDay = Down + 15.f;
		S.LitterDays = 10.f;
	}

	FPlantSpecies Indoor(const TCHAR* Name, EPlantHerbForm Form, float PotCm)
	{
		FPlantSpecies S;
		S.Name = Name;
		S.bIndoor = true;
		S.bEvergreen = true;
		S.Herb.Form = Form;
		S.Herb.MatureYears = 2.f;
		S.PotDiameter = PotCm / 100.f;
		S.Pot = EPlantPot::Terracotta;
		S.Sway = 0.f;
		S.LifeMedian = 40.f;
		S.LifeMax = 80.f;
		return S;
	}

	// --- flower beds

	/** Hemerocallis fulva, the tawny day lily (лилейник): fans of arching strap leaves, orange trumpets in July. */
	FPlantSpecies DayLily()
	{
		FPlantSpecies S;
		S.Name = TEXT("DayLily");
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Rosette;
		H.Height = 0.5f;
		H.Spread = 0.7f;
		H.MatureYears = 2.5f;
		H.Leaves = 40;
		H.Fans = 5;
		H.Rise = FVector2D(8.0, 40.0);
		H.Arch = 85.f;
		H.ProfilePower = 0.35f;
		H.Flower = EPlantFlowerForm::Trumpet;
		H.FlowerSize = 0.11f;
		H.Flowers = 6;
		H.ScapeHeight = 0.35f;
		H.FlowerColours = { C(0.85f, 0.22f, 0.03f), C(0.8f, 0.3f, 0.04f) };
		H.CentreColour = C(0.8f, 0.5f, 0.02f);
		H.BloomStart = 178.f;
		H.BloomEnd = 222.f;
		H.StemColour = C(0.07f, 0.13f, 0.03f);
		S.Leaf.Length = 0.6f;
		S.Leaf.Width = 0.045f;
		S.Leaf.WidestAt = 0.3f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.f;
		S.Leaf.Fold = 0.35f;
		Perennial(S, 100.f, 305.f);
		S.SpringLeafColour = C(0.12f, 0.26f, 0.05f);
		S.SummerLeafColour = C(0.06f, 0.15f, 0.03f);
		S.AutumnLeafColour = C(0.3f, 0.26f, 0.06f);
		S.AutumnLeafColourLate = C(0.22f, 0.13f, 0.05f);
		return S;
	}

	/** Tagetes patula, French marigold (бархатцы): an annual cushion of fern-like leaves and ruffled orange heads till the frost. */
	FPlantSpecies Marigold()
	{
		FPlantSpecies S;
		S.Name = TEXT("Marigold");
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Bush;
		H.bAnnual = true;
		H.SowDay = 128.f;
		H.FrostDay = 300.f;
		H.Height = 0.28f;
		H.Spread = 0.35f;
		H.Stems = 6;
		H.Branches = 3;
		H.Lean = 28.f;
		H.StemRadius = 0.003f;
		H.BareStem = 0.1f;
		H.Leaves = 5;
		H.PerNode = 1;
		H.Rise = FVector2D(45.0, 70.0);
		H.Arch = 30.f;
		H.Leaflets = 4;
		H.Leaflet = FVector2D(0.32, 0.28);
		H.Flower = EPlantFlowerForm::Pompon;
		H.FlowerSize = 0.045f;
		H.Flowers = 4;
		H.FlowerColours = { C(0.9f, 0.33f, 0.01f), C(0.95f, 0.62f, 0.02f), C(0.42f, 0.06f, 0.02f), C(0.9f, 0.45f, 0.01f) };
		H.CentreColour = C(0.6f, 0.25f, 0.01f);
		H.BloomStart = 165.f;
		H.BloomEnd = 400.f;
		H.StemColour = C(0.09f, 0.12f, 0.03f);
		S.Leaf.Length = 0.07f;
		S.Leaf.Width = 0.3f;
		S.Leaf.Petiole = 0.1f;
		S.Leaf.Teeth = 0;
		S.SummerLeafColour = C(0.035f, 0.09f, 0.02f);
		S.AutumnLeafColourLate = C(0.12f, 0.08f, 0.03f);
		return S;
	}

	/** Cosmos bipinnatus (космея): tall, airy, thread-leaved; pink, white and crimson daisies from July to the frost. */
	FPlantSpecies Cosmos()
	{
		FPlantSpecies S;
		S.Name = TEXT("Cosmos");
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Bush;
		H.bAnnual = true;
		H.SowDay = 130.f;
		H.FrostDay = 296.f;
		H.Height = 1.1f;
		H.Spread = 0.6f;
		H.Stems = 4;
		H.Branches = 4;
		H.Lean = 12.f;
		H.StemRadius = 0.005f;
		H.BareStem = 0.08f;
		H.Leaves = 12;
		H.PerNode = 2;
		H.Rise = FVector2D(40.0, 65.0);
		H.Arch = 25.f;
		H.Leaflets = 8;
		H.Leaflet = FVector2D(0.4, 0.1);
		H.Flower = EPlantFlowerForm::Daisy;
		H.FlowerSize = 0.075f;
		H.Petals = 8;
		H.Flowers = 7;
		H.FlowerColours = { C(0.8f, 0.25f, 0.45f), C(0.85f, 0.84f, 0.8f), C(0.55f, 0.04f, 0.25f), C(0.85f, 0.45f, 0.62f) };
		H.CentreColour = C(0.8f, 0.55f, 0.02f);
		H.BloomStart = 185.f;
		H.BloomEnd = 400.f;
		H.StemColour = C(0.09f, 0.16f, 0.04f);
		S.Leaf.Length = 0.1f;
		S.Leaf.Width = 0.05f;
		S.Leaf.Petiole = 0.1f;
		S.Leaf.Teeth = 0;
		S.SummerLeafColour = C(0.06f, 0.15f, 0.03f);
		S.AutumnLeafColourLate = C(0.12f, 0.08f, 0.03f);
		return S;
	}

	/** Symphyotrichum novi-belgii, Michaelmas daisy (астра новобельгийская, «сентябринка»): sprays of small lilac daisies in September and October. */
	FPlantSpecies Aster()
	{
		FPlantSpecies S;
		S.Name = TEXT("Aster");
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Bush;
		H.Height = 0.8f;
		H.Spread = 0.6f;
		H.MatureYears = 2.f;
		H.Stems = 8;
		H.Branches = 3;
		H.Lean = 14.f;
		H.StemRadius = 0.004f;
		H.BareStem = 0.12f;
		H.Leaves = 20;
		H.Rise = FVector2D(50.0, 75.0);
		H.Arch = 20.f;
		H.Flower = EPlantFlowerForm::Daisy;
		H.FlowerSize = 0.032f;
		H.Petals = 24;
		H.Flowers = 14;
		H.FlowerColours = { C(0.35f, 0.2f, 0.65f), C(0.22f, 0.1f, 0.55f), C(0.62f, 0.25f, 0.5f), C(0.4f, 0.28f, 0.7f) };
		H.CentreColour = C(0.85f, 0.6f, 0.03f);
		H.BloomStart = 245.f;
		H.BloomEnd = 305.f;
		H.StemColour = C(0.12f, 0.1f, 0.05f);
		S.Leaf.Length = 0.09f;
		S.Leaf.Width = 0.25f;
		S.Leaf.WidestAt = 0.45f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.f;
		Perennial(S, 110.f, 320.f);
		S.SummerLeafColour = C(0.05f, 0.12f, 0.03f);
		S.AutumnLeafColour = C(0.15f, 0.12f, 0.04f);
		S.AutumnLeafColourLate = C(0.12f, 0.07f, 0.03f);
		return S;
	}

	// --- hedges: shrubs clipped to a box; plant them in a row, about a box width apart

	/** Ligustrum vulgare, wild privet (бирючина): the clipped hedge of Soviet courtyards, small dark opposite leaves kept into winter. */
	FPlantSpecies Privet()
	{
		FPlantSpecies S;
		S.Name = TEXT("Privet");
		// many stems from the ground, leafy to the base, clipped to a box (width along the actor's X)
		S.CrownBox = FVector(0.9, 0.65, 1.3);
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Bush;
		H.bWoody = true;
		H.Height = 1.2f;
		// semi-evergreen on the Baltic: it keeps its leaves through the winter, darker
		S.bEvergreen = true;
		H.Spread = 0.9f;
		H.MatureYears = 3.f;
		H.Stems = 10;
		H.Branches = 3;
		H.Lean = 24.f;
		H.StemRadius = 0.005f;
		H.BareStem = 0.06f;
		H.Leaves = 8;
		H.PerNode = 2;
		H.Rise = FVector2D(50.0, 80.0);
		H.Arch = 15.f;
		H.StemColour = C(0.07f, 0.06f, 0.045f);
		S.Leaf.Length = 0.06f;
		S.Leaf.Width = 0.45f;
		S.Leaf.WidestAt = 0.45f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.08f;
		S.Leaf.Fold = 0.15f;
		S.BudBreakDay = 105.f;
		S.FullLeafDay = 130.f;
		S.ColourStartDay = 305.f;
		S.ColourFullDay = 340.f;
		S.LeafDropStartDay = 335.f;
		S.BareDay = 365.f;
		S.SpringLeafColour = C(0.1f, 0.22f, 0.05f);
		S.SummerLeafColour = C(0.04f, 0.1f, 0.025f);
		S.AutumnLeafColour = C(0.06f, 0.07f, 0.03f);
		S.AutumnLeafColourLate = C(0.12f, 0.06f, 0.06f);
		S.LifeMedian = 60.f;
		S.LifeMax = 100.f;
		return S;
	}

	/** A tree-form shrub to start a spiraea from (privet's old tree form). */
	FPlantSpecies ShrubBase()
	{
		FPlantSpecies S;
		S.MaxHeight = 1.7f;
		S.GrowthRate = 0.5f;
		S.GrowthShape = 1.2f;
		S.TrunkRadiusPerHeight = 0.012f;
		S.MinRadius = 0.004f;
		S.CrownBase = 0.f;
		S.TrunkNodeSpacing = 0.035f;
		S.Branch1Profile = FVector(1.0, 1.0, 0.9);
		S.LengthTau = 3.f;
		S.Wobble = 0.3f;
		S.KinkSpacing = 0.1f;
		S.Branch2Spacing = 0.035f;
		S.Branch2Angle = 50.f;
		S.Branch2MaxLength = 0.3f;
		S.Branch2LengthRatio = 0.6f;
		S.Branch2ShadeDepth = 0.35f;
		S.BranchRadiusBase = 0.002f;
		S.BranchRadiusPerLength = 0.008f;
		S.LeafyTipLength = 1.f;
		S.Leaf.Petiole = 0.1f;
		S.Leaf.Fold = 0.15f;
		S.Bark.Pattern = EPlantBarkPattern::Smooth;
		S.Bark.TileWidth = 0.05f;
		S.Bark.TileHeight = 0.1f;
		S.Bark.Depth = 0.3f;
		S.Bark.RidgeColour = C(0.07f, 0.065f, 0.055f);
		S.Bark.FurrowColour = C(0.03f, 0.028f, 0.024f);
		S.SpringLeafColour = C(0.1f, 0.22f, 0.05f);
		S.LifeMedian = 60.f;
		S.LifeMax = 100.f;
		return S;
	}

	/**
	 * Spiraea x vanhouttei (спирея Вангутта): arching branches covered in white umbels in late May; turns
	 * orange-red in October. Planted loose or as an unclipped hedge.
	 */
	FPlantSpecies Spiraea()
	{
		FPlantSpecies S = ShrubBase();
		S.Name = TEXT("Spiraea");
		S.CrownBox = FVector(1.4, 1.2, 1.7);
		S.LeaderFraction = 0.3f;
		S.Branch1Angle = 30.f;
		S.Branch1AngleTop = 25.f;
		S.Branch1MaxLength = 1.6f;
		S.Tropism = -0.05f;
		S.Droop = 0.12f;
		S.TwigHang = 0.4f;
		S.Kink = 0.1f;
		S.Leaf.Length = 0.045f;
		S.Leaf.Width = 0.7f;
		S.Leaf.WidestAt = 0.6f;
		S.Leaf.Lobes = 3;
		S.Leaf.LobeDepth = 0.15f;
		S.Leaf.Teeth = 6;
		S.Leaf.TeethDepth = 0.2f;
		S.SummerLeafColour = C(0.05f, 0.11f, 0.05f);
		S.AutumnLeafColour = C(0.45f, 0.15f, 0.03f);
		S.AutumnLeafColourLate = C(0.4f, 0.07f, 0.03f);
		S.BudBreakDay = 108.f;
		S.FullLeafDay = 132.f;
		S.ColourStartDay = 280.f;
		S.ColourFullDay = 300.f;
		S.LeafDropStartDay = 295.f;
		S.BareDay = 320.f;
		// its flower umbels drawn as the fruit: white balls along the arching shoots, from mid-May to mid-June
		S.FruitSize = 0.03f;
		S.FruitCluster = 4;
		S.FruitAspect = 0.6f;
		S.FruitChance = 0.7f;
		S.FruitingAge = 2.f;
		S.FruitSetDay = 132.f;
		S.FruitRipeDay = 140.f;
		S.FruitFallDay = 166.f;
		S.FruitRotDays = 0.f;
		S.FruitUnripeColour = C(0.75f, 0.78f, 0.7f);
		S.FruitGroundColour = C(0.85f, 0.85f, 0.8f);
		S.FruitColour = C(0.85f, 0.85f, 0.8f);
		S.Sway = 6.f;
		S.LeafSpacing = 0.022f;
		S.LeafClusterSize = 0.15f;
		S.Leaf.PerCluster = 10;
		return S;
	}

	// --- house plants

	/** Ficus elastica, the rubber plant (фикус каучуконосный): one stem of big leathery, glossy leaves. */
	FPlantSpecies RubberPlant()
	{
		FPlantSpecies S = Indoor(TEXT("RubberPlant"), EPlantHerbForm::Bush, 28.f);
		FPlantHerb& H = S.Herb;
		H.Height = 1.2f;
		H.Spread = 0.6f;
		H.MatureYears = 4.f;
		H.Stems = 1;
		H.Branches = 0;
		H.Lean = 6.f;
		H.StemRadius = 0.012f;
		H.BareStem = 0.12f;
		H.Leaves = 22;
		H.Rise = FVector2D(40.0, 80.0);
		H.Arch = 25.f;
		H.StemColour = C(0.07f, 0.08f, 0.04f);
		S.Leaf.Length = 0.28f;
		S.Leaf.Width = 0.45f;
		S.Leaf.WidestAt = 0.5f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.18f;
		S.Leaf.Fold = 0.12f;
		S.SummerLeafColour = C(0.015f, 0.05f, 0.015f);
		return S;
	}

	/** Ficus benjamina, weeping fig (фикус Бенджамина): a little tree of hanging twigs and small pointed leaves. */
	FPlantSpecies WeepingFig()
	{
		FPlantSpecies S;
		S.Name = TEXT("WeepingFig");
		S.bIndoor = true;
		S.bEvergreen = true;
		S.bPreferBake = true;
		S.PotDiameter = 0.3f;
		S.MaxHeight = 1.8f;
		S.GrowthRate = 0.3f;
		S.GrowthShape = 1.2f;
		S.LeaderFraction = 0.8f;
		S.TrunkRadiusPerHeight = 0.012f;
		S.MinRadius = 0.003f;
		S.CrownBase = 0.3f;
		S.CrownWidth = 0.6f;
		S.TrunkNodeSpacing = 0.06f;
		S.Branch1Angle = 55.f;
		S.Branch1AngleTop = 35.f;
		S.Branch1MaxLength = 0.6f;
		S.Branch1Profile = FVector(0.7, 1.0, 0.5);
		S.LengthTau = 3.f;
		S.Tropism = 0.f;
		S.Wobble = 0.25f;
		S.Droop = 0.05f;
		S.TwigHang = 0.6f;
		S.Branch2Spacing = 0.035f;
		S.Branch2Angle = 50.f;
		S.Branch2MaxLength = 0.25f;
		S.Branch2LengthRatio = 0.6f;
		S.Branch2ShadeDepth = 0.3f;
		S.BranchRadiusBase = 0.0015f;
		S.BranchRadiusPerLength = 0.008f;
		S.LeafyTipLength = 1.f;
		S.LeafSpacing = 0.018f;
		S.LeafClusterSize = 0.15f;
		S.Leaf.Length = 0.08f;
		S.Leaf.Width = 0.45f;
		S.Leaf.WidestAt = 0.4f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.15f;
		S.Leaf.Fold = 0.2f;
		S.Leaf.PerCluster = 10;
		S.Bark.Pattern = EPlantBarkPattern::Smooth;
		S.Bark.TileWidth = 0.05f;
		S.Bark.TileHeight = 0.1f;
		S.Bark.Depth = 0.2f;
		S.Bark.RidgeColour = C(0.12f, 0.11f, 0.09f);
		S.Bark.FurrowColour = C(0.06f, 0.055f, 0.045f);
		S.SummerLeafColour = C(0.04f, 0.1f, 0.025f);
		S.Sway = 0.f;
		S.LifeMedian = 40.f;
		S.LifeMax = 80.f;
		return S;
	}

	/** Citrus limon 'Pavlovsky' (павловский лимон), the classic windowsill lemon: dark glossy leaves, lemons all year. */
	FPlantSpecies Lemon()
	{
		FPlantSpecies S = WeepingFig();
		S.Name = TEXT("Lemon");
		S.bPreferBake = false;
		S.PotDiameter = 0.28f;
		S.MaxHeight = 1.3f;
		S.LeaderFraction = 0.5f;
		S.CrownBase = 0.25f;
		S.CrownWidth = 0.9f;
		S.Branch1Angle = 45.f;
		S.Branch1MaxLength = 0.55f;
		S.Kink = 0.3f;
		S.KinkSpacing = 0.12f;
		S.TwigHang = 0.f;
		S.Droop = 0.02f;
		S.LeafSpacing = 0.025f;
		S.LeafClusterSize = 0.16f;
		S.Leaf.Length = 0.1f;
		S.Leaf.Width = 0.5f;
		S.Leaf.WidestAt = 0.45f;
		S.Leaf.Petiole = 0.12f;
		S.Leaf.PerCluster = 7;
		S.Bark.RidgeColour = C(0.08f, 0.09f, 0.05f);
		S.SummerLeafColour = C(0.02f, 0.06f, 0.015f);
		S.FruitSize = 0.065f;
		S.FruitAspect = 1.3f;
		S.FruitChance = 0.3f;
		S.FruitingAge = 3.f;
		S.FruitSetDay = 1.f;
		S.FruitRipeDay = 2.f;
		S.FruitFallDay = 700.f; // never falls: lemons are picked, and on the tree they keep
		S.FruitColour = C(0.85f, 0.62f, 0.04f);
		S.FruitGroundColour = C(0.85f, 0.62f, 0.04f);
		S.FruitUnripeColour = C(0.2f, 0.35f, 0.05f);
		S.LeafyTipLength = 1.f;
		return S;
	}

	/** Dracaena fragrans 'Massangeana' (драцена): bare canes topped with rosettes of arching leaves striped yellow down the middle. */
	FPlantSpecies Dracaena()
	{
		FPlantSpecies S = Indoor(TEXT("Dracaena"), EPlantHerbForm::Bush, 30.f);
		FPlantHerb& H = S.Herb;
		H.Height = 1.3f;
		H.Spread = 0.7f;
		H.MatureYears = 4.f;
		H.Stems = 3;
		H.Branches = 0;
		H.Lean = 7.f;
		H.StemRadius = 0.022f;
		H.BareStem = 0.75f;
		H.Leaves = 30;
		H.Rise = FVector2D(10.0, 85.0);
		H.Arch = 70.f;
		H.ProfilePower = 0.6f;
		H.Variegation = EPlantVariegation::CentralStripe;
		H.VariegationColour = C(0.5f, 0.45f, 0.1f);
		H.StemColour = C(0.1f, 0.085f, 0.06f);
		S.Leaf.Length = 0.6f;
		S.Leaf.Width = 0.12f;
		S.Leaf.WidestAt = 0.5f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.f;
		S.Leaf.Fold = 0.15f;
		S.SummerLeafColour = C(0.03f, 0.09f, 0.025f);
		return S;
	}

	/** Monstera deliciosa (монстера): huge split leaves on long stalks. */
	FPlantSpecies Monstera()
	{
		FPlantSpecies S = Indoor(TEXT("Monstera"), EPlantHerbForm::Rosette, 34.f);
		FPlantHerb& H = S.Herb;
		H.Height = 0.9f;
		H.Spread = 1.0f;
		H.MatureYears = 4.f;
		H.Leaves = 9;
		H.Rise = FVector2D(15.0, 55.0);
		H.Arch = 75.f;
		H.StemColour = C(0.05f, 0.11f, 0.03f);
		S.Leaf.Length = 0.42f;
		S.Leaf.Width = 0.9f;
		S.Leaf.WidestAt = 0.42f;
		S.Leaf.Lobes = 6;
		S.Leaf.LobeDepth = 0.65f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 1.1f;
		S.Leaf.Fold = 0.1f;
		S.SummerLeafColour = C(0.02f, 0.07f, 0.02f);
		return S;
	}

	/** Sansevieria trifasciata, snake plant (сансевиерия, «тёщин язык»): stiff upright blades banded dark green. */
	FPlantSpecies SnakePlant()
	{
		FPlantSpecies S = Indoor(TEXT("SnakePlant"), EPlantHerbForm::Rosette, 20.f);
		FPlantHerb& H = S.Herb;
		H.Height = 0.7f;
		H.Spread = 0.3f;
		H.MatureYears = 3.f;
		H.Leaves = 16;
		H.Fans = 4;
		H.Rise = FVector2D(2.0, 16.0);
		H.Arch = 6.f;
		H.Twist = 40.f;
		H.ProfilePower = 0.45f;
		H.Thickness = 0.12f;
		H.Variegation = EPlantVariegation::Bands;
		S.Leaf.Length = 0.7f;
		S.Leaf.Width = 0.1f;
		S.Leaf.WidestAt = 0.55f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.f;
		S.Leaf.Fold = 0.35f;
		S.SummerLeafColour = C(0.06f, 0.13f, 0.05f);
		return S;
	}

	/** Aspidistra elatior, cast-iron plant (аспидистра): dark leaves on stalks straight from the soil; it survives anything. */
	FPlantSpecies Aspidistra()
	{
		FPlantSpecies S = Indoor(TEXT("Aspidistra"), EPlantHerbForm::Rosette, 24.f);
		FPlantHerb& H = S.Herb;
		H.Height = 0.6f;
		H.Spread = 0.6f;
		H.MatureYears = 3.f;
		H.Leaves = 14;
		H.Rise = FVector2D(8.0, 40.0);
		H.Arch = 50.f;
		H.StemColour = C(0.04f, 0.09f, 0.025f);
		S.Leaf.Length = 0.45f;
		S.Leaf.Width = 0.2f;
		S.Leaf.WidestAt = 0.45f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.5f;
		S.Leaf.Fold = 0.2f;
		S.SummerLeafColour = C(0.02f, 0.065f, 0.02f);
		return S;
	}

	/** Chlorophytum comosum, spider plant (хлорофитум): a fountain of narrow leaves with a cream stripe. */
	FPlantSpecies SpiderPlant()
	{
		FPlantSpecies S = Indoor(TEXT("SpiderPlant"), EPlantHerbForm::Rosette, 18.f);
		FPlantHerb& H = S.Herb;
		H.Height = 0.3f;
		H.Spread = 0.55f;
		H.MatureYears = 1.5f;
		H.Leaves = 30;
		H.Rise = FVector2D(15.0, 70.0);
		H.Arch = 110.f;
		H.ProfilePower = 0.5f;
		H.Variegation = EPlantVariegation::CentralStripe;
		H.VariegationColour = C(0.7f, 0.72f, 0.55f);
		S.Leaf.Length = 0.35f;
		S.Leaf.Width = 0.06f;
		S.Leaf.WidestAt = 0.35f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.f;
		S.Leaf.Fold = 0.25f;
		S.SummerLeafColour = C(0.08f, 0.2f, 0.04f);
		return S;
	}

	/** Tradescantia zebrina (традесканция): trailing stems of pointed leaves, purple-green with two silver stripes. */
	FPlantSpecies Tradescantia()
	{
		FPlantSpecies S = Indoor(TEXT("Tradescantia"), EPlantHerbForm::Trailing, 16.f);
		S.Pot = EPlantPot::White;
		FPlantHerb& H = S.Herb;
		H.Spread = 0.45f;
		H.Height = 0.15f;
		H.MatureYears = 1.f;
		H.Stems = 10;
		H.Leaves = 16;
		H.StemRadius = 0.0025f;
		H.Variegation = EPlantVariegation::Stripes;
		H.VariegationColour = C(0.4f, 0.45f, 0.42f);
		H.StemColour = C(0.16f, 0.06f, 0.09f);
		S.Leaf.Length = 0.065f;
		S.Leaf.Width = 0.45f;
		S.Leaf.WidestAt = 0.4f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.f;
		S.Leaf.Fold = 0.1f;
		S.SummerLeafColour = C(0.09f, 0.08f, 0.09f);
		return S;
	}

	/** Pelargonium zonale, geranium (герань): the windowsill classic, round scalloped leaves and scarlet heads all year. */
	FPlantSpecies Geranium()
	{
		FPlantSpecies S = Indoor(TEXT("Geranium"), EPlantHerbForm::Bush, 16.f);
		FPlantHerb& H = S.Herb;
		H.Height = 0.4f;
		H.Spread = 0.4f;
		H.MatureYears = 1.5f;
		H.Stems = 4;
		H.Branches = 1;
		H.Lean = 15.f;
		H.StemRadius = 0.007f;
		H.BareStem = 0.3f;
		H.Leaves = 6;
		H.Rise = FVector2D(50.0, 80.0);
		H.Arch = 15.f;
		H.Flower = EPlantFlowerForm::Umbel;
		H.FlowerSize = 0.1f;
		H.Flowers = 2;
		H.FlowerColours = { C(0.85f, 0.05f, 0.035f), C(0.8f, 0.08f, 0.05f), C(0.85f, 0.3f, 0.4f) };
		H.CentreColour = C(0.9f, 0.7f, 0.6f);
		H.StemColour = C(0.1f, 0.15f, 0.05f);
		S.Leaf.Form = EPlantLeafForm::Palmate;
		S.Leaf.Length = 0.085f;
		S.Leaf.Width = 1.0f;
		S.Leaf.Lobes = 7;
		S.Leaf.LobeDepth = 0.1f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 1.2f;
		S.Leaf.Fold = 0.25f;
		S.SummerLeafColour = C(0.05f, 0.12f, 0.03f);
		return S;
	}

	/** Begonia semperflorens, wax begonia (бегония вечноцветущая): glossy rounded leaves, small pink, red or white flowers all year. */
	FPlantSpecies Begonia()
	{
		FPlantSpecies S = Indoor(TEXT("Begonia"), EPlantHerbForm::Bush, 14.f);
		FPlantHerb& H = S.Herb;
		H.Height = 0.25f;
		H.Spread = 0.3f;
		H.MatureYears = 1.f;
		H.Stems = 6;
		H.Branches = 2;
		H.Lean = 20.f;
		H.StemRadius = 0.004f;
		H.BareStem = 0.15f;
		H.Leaves = 6;
		H.Rise = FVector2D(50.0, 80.0);
		H.Arch = 15.f;
		H.Flower = EPlantFlowerForm::Five;
		H.FlowerSize = 0.028f;
		H.Flowers = 5;
		H.FlowerColours = { C(0.85f, 0.3f, 0.45f), C(0.75f, 0.05f, 0.08f), C(0.85f, 0.83f, 0.8f) };
		H.CentreColour = C(0.9f, 0.7f, 0.05f);
		H.StemColour = C(0.18f, 0.08f, 0.06f);
		S.Leaf.Length = 0.06f;
		S.Leaf.Width = 0.85f;
		S.Leaf.WidestAt = 0.45f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.3f;
		S.Leaf.Fold = 0.1f;
		S.SummerLeafColour = C(0.04f, 0.1f, 0.02f);
		return S;
	}

	/** Saintpaulia, African violet (узамбарская фиалка): a flat rosette of furry round leaves, violet flowers in the middle. */
	FPlantSpecies AfricanViolet()
	{
		FPlantSpecies S = Indoor(TEXT("AfricanViolet"), EPlantHerbForm::Rosette, 10.f);
		S.Pot = EPlantPot::White;
		FPlantHerb& H = S.Herb;
		H.Height = 0.06f;
		H.Spread = 0.25f;
		H.MatureYears = 1.f;
		H.Leaves = 16;
		H.Rise = FVector2D(50.0, 85.0);
		H.Arch = 15.f;
		H.Flower = EPlantFlowerForm::Five;
		H.FlowerSize = 0.03f;
		H.Flowers = 6;
		H.ScapeHeight = 0.04f;
		H.FlowerColours = { C(0.3f, 0.07f, 0.55f), C(0.15f, 0.05f, 0.42f), C(0.7f, 0.2f, 0.45f) };
		H.CentreColour = C(0.9f, 0.75f, 0.05f);
		H.StemColour = C(0.08f, 0.1f, 0.05f);
		S.Leaf.Form = EPlantLeafForm::Palmate;
		S.Leaf.Length = 0.06f;
		S.Leaf.Width = 0.95f;
		S.Leaf.Lobes = 0;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.9f;
		S.Leaf.Fold = 0.2f;
		S.SummerLeafColour = C(0.03f, 0.075f, 0.03f);
		return S;
	}

	/** Howea forsteriana, kentia palm (кентия): arching fronds of drooping leaflets on long stalks. */
	FPlantSpecies KentiaPalm()
	{
		FPlantSpecies S = Indoor(TEXT("KentiaPalm"), EPlantHerbForm::Rosette, 36.f);
		S.Pot = EPlantPot::Ceramic;
		FPlantHerb& H = S.Herb;
		H.Height = 1.5f;
		H.Spread = 1.4f;
		H.MatureYears = 5.f;
		H.Leaves = 8;
		H.Rise = FVector2D(8.0, 40.0);
		H.Arch = 70.f;
		H.Leaflets = 16;
		H.Leaflet = FVector2D(0.35, 0.07);
		H.StemColour = C(0.05f, 0.1f, 0.03f);
		S.Leaf.Length = 1.0f;
		S.Leaf.Width = 0.5f;
		S.Leaf.Petiole = 0.45f;
		S.Leaf.Teeth = 0;
		S.SummerLeafColour = C(0.03f, 0.08f, 0.025f);
		return S;
	}

	/** Aloe vera (алоэ): a rosette of thick, grey-green succulent leaves. */
	FPlantSpecies Aloe()
	{
		FPlantSpecies S = Indoor(TEXT("Aloe"), EPlantHerbForm::Rosette, 16.f);
		FPlantHerb& H = S.Herb;
		H.Height = 0.3f;
		H.Spread = 0.4f;
		H.MatureYears = 3.f;
		H.Leaves = 16;
		H.Rise = FVector2D(15.0, 60.0);
		H.Arch = 25.f;
		H.Thickness = 0.35f;
		S.Leaf.Length = 0.3f;
		S.Leaf.Width = 0.18f;
		S.Leaf.WidestAt = 0.1f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.f;
		S.Leaf.Fold = -0.15f;
		S.SummerLeafColour = C(0.11f, 0.17f, 0.08f);
		return S;
	}

	/** A columnar cactus (кактус, цереус): a ribbed column with white-spined areoles, an arm or two when old. */
	FPlantSpecies Cactus()
	{
		FPlantSpecies S = Indoor(TEXT("Cactus"), EPlantHerbForm::Column, 12.f);
		FPlantHerb& H = S.Herb;
		H.Height = 0.32f;
		H.Spread = 0.11f;
		H.MatureYears = 6.f;
		H.Ribs = 9;
		H.Arms = 2;
		S.SummerLeafColour = C(0.05f, 0.12f, 0.045f);
		return S;
	}

	/** Dieffenbachia (диффенбахия): a thick stem of large leaves splashed cream between the veins. */
	FPlantSpecies Dieffenbachia()
	{
		FPlantSpecies S = Indoor(TEXT("Dieffenbachia"), EPlantHerbForm::Bush, 26.f);
		FPlantHerb& H = S.Herb;
		H.Height = 0.8f;
		H.Spread = 0.6f;
		H.MatureYears = 3.f;
		H.Stems = 2;
		H.Branches = 0;
		H.Lean = 8.f;
		H.StemRadius = 0.015f;
		H.BareStem = 0.3f;
		H.Leaves = 10;
		H.Rise = FVector2D(30.0, 65.0);
		H.Arch = 40.f;
		H.Variegation = EPlantVariegation::Blotches;
		H.VariegationColour = C(0.55f, 0.6f, 0.32f);
		H.StemColour = C(0.1f, 0.16f, 0.06f);
		S.Leaf.Length = 0.3f;
		S.Leaf.Width = 0.42f;
		S.Leaf.WidestAt = 0.45f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.6f;
		S.Leaf.Fold = 0.1f;
		S.SummerLeafColour = C(0.03f, 0.1f, 0.025f);
		return S;
	}

	/** Asparagus densiflorus 'Sprengeri' (аспарагус): arching, trailing stems furred with needle-like leaves. */
	FPlantSpecies AsparagusFern()
	{
		FPlantSpecies S = Indoor(TEXT("AsparagusFern"), EPlantHerbForm::Trailing, 18.f);
		FPlantHerb& H = S.Herb;
		H.Spread = 0.45f;
		H.Height = 0.2f;
		H.MatureYears = 2.f;
		H.Stems = 12;
		H.Leaves = 26;
		H.PerNode = 4;
		H.StemRadius = 0.002f;
		H.StemColour = C(0.06f, 0.13f, 0.03f);
		S.Leaf.Length = 0.035f;
		S.Leaf.Width = 0.2f;
		S.Leaf.WidestAt = 0.4f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.f;
		S.Leaf.Fold = 0.f;
		S.SummerLeafColour = C(0.06f, 0.15f, 0.03f);
		return S;
	}
}

TArray<FPlantSpecies> PlantHerbs::Species()
{
	return { DayLily(), Marigold(), Cosmos(), Aster(), Privet(), Spiraea(), RubberPlant(), WeepingFig(), Lemon(), Dracaena(), Monstera(),
		SnakePlant(), Aspidistra(), SpiderPlant(), Tradescantia(), Geranium(), Begonia(), AfricanViolet(), KentiaPalm(), Aloe(), Cactus(),
		Dieffenbachia(), AsparagusFern() };
}
