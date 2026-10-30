#include "Plants/PlantHerb.h"

// The undergrowth of the Baltic coast's pine, spruce and birch woods (Zelenogradsk, Svetlogorsk): dwarf shrubs, ferns and
// wood sorrel, drawn by AForestFloor (docs/forest_floor.md). Herbs of the usual kind (FPlantSpecies::Herb); berries are
// their "flowers" (a tiny pompon in the berry's colour, out from ripening to when they are eaten or fall).

namespace
{
	FLinearColor C(float R, float G, float B) { return FLinearColor(R, G, B); }

	/** A deciduous dwarf shrub's year: leaves out in May, turned in September, gone in early November. */
	void Deciduous(FPlantSpecies& S, float Up, float Turn, float Bare)
	{
		S.BudBreakDay = Up;
		S.FullLeafDay = Up + 25.f;
		S.ColourStartDay = Turn - 20.f;
		S.ColourFullDay = Turn;
		S.LeafDropStartDay = Turn;
		S.BareDay = Bare;
		S.LitterDays = 10.f;
		S.PhenologyJitter = 6.f;
	}

	/**
	 * Vaccinium myrtillus, bilberry (черника): knee-high carpets of green, angular stems with small finely toothed leaves;
	 * blue-black berries in July and August; the leaves turn wine red in September and are gone by November, leaving
	 * the green twigs through the winter.
	 */
	FPlantSpecies Bilberry()
	{
		FPlantSpecies S;
		S.Name = TEXT("Bilberry");
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Bush;
		H.bWoody = true;
		H.Height = 0.3f;
		H.Spread = 0.35f;
		H.MatureYears = 4.f;
		H.Stems = 6;
		H.Branches = 3;
		H.Lean = 20.f;
		H.StemRadius = 0.0022f;
		H.BareStem = 0.25f;
		H.Leaves = 7;
		H.Rise = FVector2D(45.0, 70.0);
		H.Arch = 12.f;
		H.StemColour = C(0.06f, 0.13f, 0.035f);
		H.Flower = EPlantFlowerForm::Pompon;
		H.FlowerSize = 0.009f;
		H.Flowers = 2;
		H.FlowerColours = { C(0.03f, 0.035f, 0.09f), C(0.04f, 0.03f, 0.07f) };
		H.CentreColour = C(0.05f, 0.05f, 0.1f);
		H.BloomStart = 192.f;
		H.BloomEnd = 238.f;
		S.Leaf.Length = 0.03f;
		S.Leaf.Width = 0.6f;
		S.Leaf.WidestAt = 0.45f;
		S.Leaf.Teeth = 6;
		S.Leaf.TeethDepth = 0.04f;
		S.Leaf.Petiole = 0.05f;
		S.Leaf.Fold = 0.1f;
		// in the Baltic autumn the wine-red leaves hang on into November
		Deciduous(S, 118.f, 285.f, 322.f);
		S.SpringLeafColour = C(0.14f, 0.28f, 0.06f);
		S.SummerLeafColour = C(0.06f, 0.15f, 0.03f);
		S.AutumnLeafColour = C(0.55f, 0.09f, 0.05f);
		S.AutumnLeafColourLate = C(0.42f, 0.05f, 0.06f);
		S.LifeMedian = 30.f;
		S.LifeMax = 60.f;
		return S;
	}

	/**
	 * Vaccinium vitis-idaea, lingonberry (брусника): a low evergreen of the dry pine woods, small leathery, glossy leaves
	 * with rolled edges; clusters of red berries from late August that stay into the winter; bronze in the cold.
	 */
	FPlantSpecies Lingonberry()
	{
		FPlantSpecies S;
		S.Name = TEXT("Lingonberry");
		S.bEvergreen = true;
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Bush;
		H.bWoody = true;
		H.Height = 0.16f;
		H.Spread = 0.25f;
		H.MatureYears = 4.f;
		H.Stems = 5;
		H.Branches = 1;
		H.Lean = 22.f;
		H.StemRadius = 0.0018f;
		H.BareStem = 0.2f;
		H.Leaves = 9;
		H.Rise = FVector2D(50.0, 80.0);
		H.Arch = 4.f;
		H.StemColour = C(0.09f, 0.07f, 0.04f);
		H.Flower = EPlantFlowerForm::Pompon;
		H.FlowerSize = 0.01f;
		H.Flowers = 2;
		H.FlowerColours = { C(0.5f, 0.025f, 0.02f), C(0.42f, 0.02f, 0.025f), C(0.55f, 0.05f, 0.03f) };
		H.CentreColour = C(0.35f, 0.02f, 0.02f);
		H.BloomStart = 228.f;
		H.BloomEnd = 335.f;
		S.Leaf.Length = 0.02f;
		S.Leaf.Width = 0.55f;
		S.Leaf.WidestAt = 0.55f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.03f;
		S.Leaf.Fold = 0.22f;
		S.ColourStartDay = 300.f;
		S.ColourFullDay = 345.f;
		S.SpringLeafColour = C(0.1f, 0.2f, 0.05f);
		S.SummerLeafColour = C(0.025f, 0.075f, 0.02f);
		S.AutumnLeafColour = C(0.04f, 0.065f, 0.02f);
		S.AutumnLeafColourLate = C(0.08f, 0.04f, 0.03f);
		S.LifeMedian = 30.f;
		S.LifeMax = 60.f;
		return S;
	}

	/**
	 * Calluna vulgaris, heather (вереск): wiry, much-branched brown stems clothed in tiny scale leaves; pink-mauve spikes
	 * in August, rusty seed heads through the winter. Dry, light pine woods and the dunes.
	 */
	FPlantSpecies Heather()
	{
		FPlantSpecies S;
		S.Name = TEXT("Heather");
		S.bEvergreen = true;
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Bush;
		H.bWoody = true;
		H.Height = 0.36f;
		H.Spread = 0.4f;
		H.MatureYears = 5.f;
		H.Stems = 8;
		H.Branches = 3;
		H.Lean = 25.f;
		H.StemRadius = 0.0025f;
		H.BareStem = 0.15f;
		H.Leaves = 14;
		H.PerNode = 2;
		H.Rise = FVector2D(25.0, 45.0);
		H.Arch = 0.f;
		H.StemColour = C(0.07f, 0.045f, 0.03f);
		H.Flower = EPlantFlowerForm::Five;
		H.FlowerSize = 0.007f;
		H.Petals = 4;
		H.Flowers = 9;
		H.FlowerColours = { C(0.42f, 0.13f, 0.32f), C(0.5f, 0.2f, 0.4f), C(0.36f, 0.1f, 0.3f) };
		H.CentreColour = C(0.3f, 0.08f, 0.2f);
		H.BloomStart = 205.f;
		H.BloomEnd = 270.f;
		S.Leaf.Length = 0.012f;
		S.Leaf.Width = 0.3f;
		S.Leaf.WidestAt = 0.4f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.f;
		S.Leaf.Fold = 0.3f;
		S.ColourStartDay = 285.f;
		S.ColourFullDay = 330.f;
		S.SpringLeafColour = C(0.08f, 0.15f, 0.04f);
		S.SummerLeafColour = C(0.04f, 0.09f, 0.025f);
		S.AutumnLeafColour = C(0.06f, 0.07f, 0.03f);
		S.AutumnLeafColourLate = C(0.1f, 0.05f, 0.035f);
		S.LifeMedian = 25.f;
		S.LifeMax = 40.f;
		return S;
	}

	/**
	 * Pteridium aquilinum, bracken (орляк): single tall fronds from a creeping rhizome, in colonies in the lighter woods;
	 * unrolls in late May, rusty brown from early October and broken down by the first snow.
	 */
	FPlantSpecies Bracken()
	{
		FPlantSpecies S;
		S.Name = TEXT("Bracken");
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Rosette;
		H.Height = 0.9f;
		H.Spread = 0.9f;
		H.MatureYears = 2.f;
		H.Leaves = 3;
		H.Rise = FVector2D(20.0, 35.0);
		H.Arch = 55.f;
		H.Leaflets = 11;
		H.Leaflet = FVector2D(0.42, 0.28);
		H.StemColour = C(0.1f, 0.12f, 0.04f);
		S.Leaf.Length = 0.75f;
		S.Leaf.Width = 0.5f;
		S.Leaf.WidestAt = 0.35f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.6f;
		S.Leaf.Fold = 0.05f;
		S.BudBreakDay = 140.f;
		S.FullLeafDay = 165.f;
		S.ColourStartDay = 255.f;
		S.ColourFullDay = 282.f;
		S.LeafDropStartDay = 300.f;
		S.BareDay = 335.f;
		S.PhenologyJitter = 7.f;
		S.SpringLeafColour = C(0.14f, 0.26f, 0.05f);
		S.SummerLeafColour = C(0.06f, 0.15f, 0.03f);
		S.AutumnLeafColour = C(0.36f, 0.2f, 0.05f);
		S.AutumnLeafColourLate = C(0.22f, 0.1f, 0.04f);
		return S;
	}

	/**
	 * Dryopteris dilatata, broad buckler fern (щитовник широкий): a shuttlecock of arching, finely cut fronds in the damp
	 * spruce woods; green well into the autumn, browning after the frosts.
	 */
	FPlantSpecies BucklerFern()
	{
		FPlantSpecies S;
		S.Name = TEXT("BucklerFern");
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Rosette;
		H.Height = 0.6f;
		H.Spread = 0.9f;
		H.MatureYears = 3.f;
		H.Leaves = 8;
		H.Rise = FVector2D(30.0, 55.0);
		H.Arch = 60.f;
		H.Leaflets = 14;
		H.Leaflet = FVector2D(0.34, 0.32);
		H.StemColour = C(0.09f, 0.08f, 0.035f);
		S.Leaf.Length = 0.62f;
		S.Leaf.Width = 0.4f;
		S.Leaf.WidestAt = 0.35f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 0.3f;
		S.Leaf.Fold = 0.05f;
		S.BudBreakDay = 125.f;
		S.FullLeafDay = 150.f;
		S.ColourStartDay = 295.f;
		S.ColourFullDay = 322.f;
		S.LeafDropStartDay = 318.f;
		S.BareDay = 350.f;
		S.PhenologyJitter = 7.f;
		S.SpringLeafColour = C(0.14f, 0.28f, 0.06f);
		S.SummerLeafColour = C(0.05f, 0.14f, 0.03f);
		S.AutumnLeafColour = C(0.18f, 0.16f, 0.04f);
		S.AutumnLeafColourLate = C(0.2f, 0.1f, 0.04f);
		return S;
	}

	/**
	 * Oxalis acetosella, wood sorrel (кислица): carpets of bright trefoil leaves on long stalks under the spruce, folding
	 * at night; small white flowers in May; the leaves last into the winter.
	 */
	FPlantSpecies WoodSorrel()
	{
		FPlantSpecies S;
		S.Name = TEXT("WoodSorrel");
		S.bEvergreen = true;
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Rosette;
		H.Height = 0.08f;
		H.Spread = 0.14f;
		H.MatureYears = 1.5f;
		H.Leaves = 7;
		H.Rise = FVector2D(35.0, 70.0);
		H.Arch = 25.f;
		H.StemColour = C(0.12f, 0.1f, 0.05f);
		H.Flower = EPlantFlowerForm::Five;
		H.FlowerSize = 0.012f;
		H.Flowers = 2;
		H.ScapeHeight = 0.02f;
		H.FlowerColours = { C(0.75f, 0.72f, 0.7f), C(0.7f, 0.62f, 0.66f) };
		H.CentreColour = C(0.6f, 0.5f, 0.1f);
		H.BloomStart = 120.f;
		H.BloomEnd = 150.f;
		S.Leaf.Form = EPlantLeafForm::Palmate;
		S.Leaf.Length = 0.022f;
		S.Leaf.Lobes = 3;
		S.Leaf.LobeDepth = 0.55f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 2.6f;
		S.Leaf.Fold = 0.2f;
		S.ColourStartDay = 310.f;
		S.ColourFullDay = 350.f;
		S.SpringLeafColour = C(0.14f, 0.3f, 0.06f);
		S.SummerLeafColour = C(0.08f, 0.2f, 0.04f);
		S.AutumnLeafColour = C(0.07f, 0.15f, 0.035f);
		S.AutumnLeafColourLate = C(0.1f, 0.1f, 0.04f);
		return S;
	}
}

TArray<FPlantSpecies> PlantHerbs::FloorSpecies()
{
	return { Bilberry(), Lingonberry(), Heather(), Bracken(), BucklerFern(), WoodSorrel() };
}
