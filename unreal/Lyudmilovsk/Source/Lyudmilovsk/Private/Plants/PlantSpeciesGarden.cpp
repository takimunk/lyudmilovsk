#include "Plants/PlantHerb.h"

// The allotment's crops (docs/allotment.md, docs/plants_bloom.md "Garden crops"): six herbs drawn one to a bed cell
// (0.6 x 1.2 m) by an AProceduralPlant that the garden spawns and pins to a growth (AProceduralPlant::SetGrowth: the
// plant shows that growth, its flowers and fruit coming with it, not with the calendar). Placed on their own (a level,
// the dev tools) they live by the calendar like any other herb: the annuals from their sowing day to the first frost.
// Round fruit (tomato, strawberry, the radish's root) is EPlantFlowerForm::Berry.

namespace
{
	FLinearColor GardenC(float R, float G, float B) { return FLinearColor(R, G, B); }

	/** A crop sown (or planted out) in spring and gone with the frost; grown up two months after sowing. */
	void GardenAnnual(FPlantSpecies& S, float Sow, float Frost)
	{
		FPlantHerb& H = S.Herb;
		H.bAnnual = true;
		H.SowDay = Sow;
		H.FrostDay = Frost;
		// a season: the age a garden's growth maps onto (AProceduralPlant::SetGrowth)
		H.MatureYears = 0.25f;
		S.LifeMedian = 1.f;
		S.LifeMax = 2.f;
	}

	/** Raphanus sativus, garden radish (редис): a low rosette of rough lyre-shaped leaves, the red root's shoulder at its foot. */
	FPlantSpecies GardenRadish()
	{
		FPlantSpecies S;
		S.Name = TEXT("GardenRadish");
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Rosette;
		GardenAnnual(S, 115.f, 290.f);
		// the root is drawn as the "flower": one ball on a stalk 4 cm long, so after the plant is set 3 cm into the soil
		// the top of it shows
		H.Height = 0.f;
		H.Spread = 0.2f;
		H.Leaves = 9;
		H.Rise = FVector2D(25.0, 60.0);
		H.Arch = 30.f;
		H.StemColour = GardenC(0.18f, 0.12f, 0.08f);
		H.Flower = EPlantFlowerForm::Berry;
		H.Petals = 0;
		H.FlowerSize = 0.035f;
		H.Flowers = 1;
		H.ScapeHeight = 0.04f;
		H.ScapeRadius = 0.0015f;
		H.FlowerColours = { GardenC(0.6f, 0.03f, 0.06f), GardenC(0.7f, 0.06f, 0.1f) };
		H.CentreColour = GardenC(0.85f, 0.82f, 0.8f);
		H.BloomStart = 135.f;
		H.BloomEnd = 400.f;
		S.Leaf.Length = 0.17f;
		S.Leaf.Width = 0.45f;
		S.Leaf.WidestAt = 0.65f;
		S.Leaf.Lobes = 3;
		S.Leaf.LobeDepth = 0.35f;
		S.Leaf.Teeth = 6;
		S.Leaf.TeethDepth = 0.1f;
		S.Leaf.Petiole = 0.3f;
		S.Leaf.Fold = 0.15f;
		S.SpringLeafColour = GardenC(0.12f, 0.24f, 0.05f);
		S.SummerLeafColour = GardenC(0.06f, 0.15f, 0.03f);
		S.AutumnLeafColourLate = GardenC(0.2f, 0.14f, 0.05f);
		return S;
	}

	/** Anethum graveolens, dill (укроп): thin blue-green stems with thread-fine leaves, flat yellow-green umbels on top. */
	FPlantSpecies GardenDill()
	{
		FPlantSpecies S;
		S.Name = TEXT("GardenDill");
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Bush;
		GardenAnnual(S, 115.f, 285.f);
		H.Height = 0.8f;
		H.Spread = 0.35f;
		H.Stems = 2;
		H.Branches = 2;
		H.Lean = 6.f;
		H.StemRadius = 0.0035f;
		H.BareStem = 0.15f;
		H.Leaves = 5;
		H.PerNode = 1;
		H.Rise = FVector2D(35.0, 60.0);
		H.Arch = 30.f;
		H.Leaflets = 7;
		H.Leaflet = FVector2D(0.45, 0.06);
		H.StemColour = GardenC(0.12f, 0.2f, 0.09f);
		// an umbel at the tip of each stem and branch
		H.Flower = EPlantFlowerForm::Disc;
		H.FlowerSize = 0.12f;
		H.Flowers = 3;
		H.FlowerColours = { GardenC(0.42f, 0.46f, 0.06f), GardenC(0.38f, 0.43f, 0.05f) };
		H.CentreColour = GardenC(0.35f, 0.4f, 0.05f);
		H.BloomStart = 185.f;
		H.BloomEnd = 400.f;
		S.Leaf.Length = 0.16f;
		S.Leaf.Width = 0.06f;
		S.Leaf.Petiole = 0.15f;
		S.Leaf.Teeth = 0;
		S.SpringLeafColour = GardenC(0.12f, 0.22f, 0.08f);
		S.SummerLeafColour = GardenC(0.07f, 0.16f, 0.06f);
		S.AutumnLeafColourLate = GardenC(0.25f, 0.2f, 0.06f);
		return S;
	}

	/**
	 * Fragaria x ananassa, garden strawberry (клубника): a low rosette of three-lobed leaves on long stalks, small white
	 * flowers and red berries on short stalks among them. A perennial: it comes back each spring.
	 */
	FPlantSpecies GardenStrawberry()
	{
		FPlantSpecies S;
		S.Name = TEXT("GardenStrawberry");
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Rosette;
		H.Height = 0.1f;
		H.Spread = 0.3f;
		H.MatureYears = 1.f;
		H.Leaves = 14;
		H.Rise = FVector2D(20.0, 55.0);
		H.Arch = 25.f;
		H.StemColour = GardenC(0.14f, 0.16f, 0.06f);
		// four of ten still in flower, the rest berries
		H.Flower = EPlantFlowerForm::Berry;
		H.Petals = 4;
		H.FlowerSize = 0.025f;
		H.Flowers = 5;
		H.ScapeHeight = 0.02f;
		H.ScapeRadius = 0.0015f;
		H.FlowerColours = { GardenC(0.65f, 0.02f, 0.03f), GardenC(0.55f, 0.01f, 0.02f), GardenC(0.7f, 0.05f, 0.04f) };
		H.CentreColour = GardenC(0.9f, 0.9f, 0.85f);
		H.BloomStart = 150.f;
		H.BloomEnd = 220.f;
		// a round blade cut into three lobes (a trifoliate leaf at bed scale), on a stalk longer than itself
		S.Leaf.Form = EPlantLeafForm::Palmate;
		S.Leaf.Length = 0.075f;
		S.Leaf.Width = 1.f;
		S.Leaf.Lobes = 3;
		S.Leaf.LobeDepth = 0.35f;
		S.Leaf.Teeth = 0;
		S.Leaf.Petiole = 1.8f;
		S.Leaf.Fold = 0.2f;
		S.BudBreakDay = 95.f;
		S.FullLeafDay = 125.f;
		S.ColourStartDay = 285.f;
		S.ColourFullDay = 315.f;
		S.LeafDropStartDay = 305.f;
		S.BareDay = 335.f;
		S.LitterDays = 10.f;
		S.SpringLeafColour = GardenC(0.1f, 0.22f, 0.05f);
		S.SummerLeafColour = GardenC(0.05f, 0.13f, 0.03f);
		S.AutumnLeafColour = GardenC(0.4f, 0.06f, 0.03f);
		S.AutumnLeafColourLate = GardenC(0.25f, 0.08f, 0.04f);
		S.LifeMedian = 5.f;
		S.LifeMax = 8.f;
		return S;
	}

	/** Solanum tuberosum, potato (картофель): a bushy clump of stems with dark compound leaves, white or lilac five-petalled flowers. */
	FPlantSpecies GardenPotato()
	{
		FPlantSpecies S;
		S.Name = TEXT("GardenPotato");
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Bush;
		GardenAnnual(S, 125.f, 275.f);
		H.Height = 0.55f;
		H.Spread = 0.55f;
		H.Stems = 4;
		H.Branches = 2;
		H.Lean = 25.f;
		H.StemRadius = 0.006f;
		H.BareStem = 0.1f;
		H.Leaves = 6;
		H.PerNode = 1;
		H.Rise = FVector2D(40.0, 70.0);
		H.Arch = 25.f;
		H.Leaflets = 3;
		H.Leaflet = FVector2D(0.33, 0.55);
		H.StemColour = GardenC(0.1f, 0.16f, 0.05f);
		// a few flowers at the tips and in a cluster at the top of each stem
		H.Flower = EPlantFlowerForm::Five;
		H.FlowerSize = 0.028f;
		H.Flowers = 4;
		H.SprayFrom = 0.85f;
		H.FlowerColours = { GardenC(0.85f, 0.85f, 0.82f), GardenC(0.55f, 0.4f, 0.7f), GardenC(0.8f, 0.78f, 0.82f) };
		H.CentreColour = GardenC(0.85f, 0.7f, 0.05f);
		H.BloomStart = 175.f;
		H.BloomEnd = 225.f;
		S.Leaf.Length = 0.18f;
		S.Leaf.Width = 0.5f;
		S.Leaf.WidestAt = 0.4f;
		S.Leaf.Petiole = 0.15f;
		S.Leaf.Teeth = 0;
		S.SpringLeafColour = GardenC(0.08f, 0.18f, 0.04f);
		S.SummerLeafColour = GardenC(0.04f, 0.1f, 0.025f);
		S.AutumnLeafColourLate = GardenC(0.2f, 0.16f, 0.06f);
		return S;
	}

	/** Solanum lycopersicum, tomato (помидор): leggy bushy stems of compound leaves, small yellow flowers and round red fruit. */
	FPlantSpecies GardenTomato()
	{
		FPlantSpecies S;
		S.Name = TEXT("GardenTomato");
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Bush;
		GardenAnnual(S, 140.f, 280.f);
		H.Height = 1.0f;
		H.Spread = 0.6f;
		H.Stems = 2;
		H.Branches = 3;
		H.Lean = 14.f;
		H.StemRadius = 0.007f;
		H.BareStem = 0.12f;
		H.Leaves = 7;
		H.PerNode = 1;
		H.Rise = FVector2D(45.0, 75.0);
		H.Arch = 35.f;
		H.Leaflets = 3;
		H.Leaflet = FVector2D(0.3, 0.5);
		H.StemColour = GardenC(0.1f, 0.16f, 0.05f);
		// fruit at the stem and branch tips and on short stalks down the stem; three of ten still yellow flowers
		H.Flower = EPlantFlowerForm::Berry;
		H.Petals = 3;
		H.FlowerSize = 0.055f;
		H.Flowers = 8;
		H.SprayFrom = 0.3f;
		H.FlowerColours = { GardenC(0.65f, 0.04f, 0.02f), GardenC(0.7f, 0.08f, 0.02f), GardenC(0.6f, 0.03f, 0.02f), GardenC(0.75f, 0.18f, 0.02f) };
		H.CentreColour = GardenC(0.9f, 0.75f, 0.05f);
		H.BloomStart = 190.f;
		H.BloomEnd = 400.f;
		S.Leaf.Length = 0.22f;
		S.Leaf.Width = 0.5f;
		S.Leaf.WidestAt = 0.4f;
		S.Leaf.Petiole = 0.12f;
		S.Leaf.Teeth = 0;
		S.SpringLeafColour = GardenC(0.1f, 0.2f, 0.05f);
		S.SummerLeafColour = GardenC(0.05f, 0.12f, 0.03f);
		S.AutumnLeafColourLate = GardenC(0.2f, 0.15f, 0.05f);
		return S;
	}

	/** Helianthus annuus, sunflower (подсолнух): one thick stem, big rough heart-shaped leaves, a nodding head of yellow rays round a brown disc. */
	FPlantSpecies GardenSunflower()
	{
		FPlantSpecies S;
		S.Name = TEXT("GardenSunflower");
		FPlantHerb& H = S.Herb;
		H.Form = EPlantHerbForm::Bush;
		GardenAnnual(S, 130.f, 285.f);
		H.Height = 1.75f;
		H.Spread = 0.6f;
		H.Stems = 1;
		H.Branches = 0;
		H.Lean = 4.f;
		H.StemRadius = 0.016f;
		H.BareStem = 0.08f;
		H.Leaves = 12;
		H.PerNode = 1;
		H.Rise = FVector2D(50.0, 80.0);
		H.Arch = 45.f;
		H.ProfilePower = 0.8f;
		H.StemColour = GardenC(0.12f, 0.17f, 0.05f);
		// one head at the top (Daisy past 15 cm: a broad seed disc, nodding to the side)
		H.Flower = EPlantFlowerForm::Daisy;
		H.FlowerSize = 0.28f;
		H.Petals = 24;
		H.Flowers = 1;
		H.FlowerColours = { GardenC(0.95f, 0.62f, 0.02f), GardenC(0.92f, 0.55f, 0.01f) };
		H.CentreColour = GardenC(0.16f, 0.08f, 0.03f);
		H.BloomStart = 200.f;
		H.BloomEnd = 265.f;
		S.Leaf.Length = 0.32f;
		S.Leaf.Width = 0.8f;
		S.Leaf.WidestAt = 0.3f;
		S.Leaf.Teeth = 12;
		S.Leaf.TeethDepth = 0.02f;
		S.Leaf.Petiole = 0.45f;
		S.Leaf.Fold = 0.1f;
		S.SpringLeafColour = GardenC(0.1f, 0.2f, 0.05f);
		S.SummerLeafColour = GardenC(0.06f, 0.14f, 0.03f);
		S.AutumnLeafColourLate = GardenC(0.22f, 0.16f, 0.06f);
		return S;
	}
}

TArray<FPlantSpecies> PlantHerbs::GardenSpecies()
{
	return { GardenRadish(), GardenDill(), GardenStrawberry(), GardenPotato(), GardenTomato(), GardenSunflower() };
}
