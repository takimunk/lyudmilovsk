#include "Plants/VineSpecies.h"

namespace
{
	/**
	 * Virginia creeper, Parthenocissus quinquefolia (девичий виноград пятилисточковый): the vine on old houses
	 * all along the Baltic coast. Five toothed leaflets on a long stalk; climbs by tendrils ending in adhesive
	 * discs, 1.5..3 m a summer, up to 15..20 m; leafs late (late May), turns crimson and scarlet from late
	 * September and is bare by early November, leaving blue-black berries on red stalks into the winter.
	 */
	FVineSpecies VirginiaCreeper()
	{
		FVineSpecies V;
		V.Name = TEXT("VirginiaCreeper");
		FPlantSpecies& S = V.Plant;
		S.Name = V.Name;
		// one leaflet: obovate, toothed above the middle, 5..12 cm
		S.Leaf.Form = EPlantLeafForm::Pinnate;
		S.Leaf.Length = 0.1f;
		S.Leaf.Width = 0.46f;
		S.Leaf.WidestAt = 0.62f;
		S.Leaf.Teeth = 7;
		S.Leaf.TeethDepth = 0.09f;
		S.Leaf.Fold = 0.1f;
		S.Leaf.Petiole = 0.05f;
		S.Leaf.PerCluster = 3;
		S.Bark.Pattern = EPlantBarkPattern::Smooth;
		S.Bark.TileWidth = 0.06f;
		S.Bark.TileHeight = 0.25f;
		S.Bark.Depth = 0.6f;
		S.Bark.RidgeColour = FLinearColor(0.11f, 0.085f, 0.065f);
		S.Bark.FurrowColour = FLinearColor(0.035f, 0.027f, 0.02f);
		S.Bark.TwigRadius = 0.006f;
		S.Bark.TwigColour = FLinearColor(0.13f, 0.045f, 0.025f);
		S.Sway = 3.f;
		// a dense mat: side shoots on side shoots, every year
		V.MaxOrder = 5;
		V.BranchSpacing = 0.38f;
		V.BranchChance = 0.85f;
		V.MaxShootLength = FVector(30.0, 10.0, 5.0);
		V.MinRadius = 0.0018f;
		V.RadiusPerSqrtLength = 0.0011f;
		V.RingWidth = 0.0004f;
		// blue-black berries on red stalks from September, kept into January
		S.FruitSize = 0.007f;
		S.FruitCluster = 9;
		S.FruitChance = 0.3f;
		S.FruitSetDay = 200.f;
		S.FruitRipeDay = 255.f;
		S.FruitFallDay = 366.f + 25.f;
		S.FruitUnripeColour = FLinearColor(0.08f, 0.16f, 0.03f);
		S.FruitColour = FLinearColor(0.012f, 0.014f, 0.035f);
		// late to leaf, early to colour
		S.BudBreakDay = 132.f;
		S.FullLeafDay = 158.f;
		S.ColourStartDay = 258.f;
		S.ColourFullDay = 286.f;
		S.LeafDropStartDay = 280.f;
		S.BareDay = 310.f;
		S.PhenologyJitter = 4.f;
		S.LitterDays = 30.f;
		S.SpringLeafColour = FLinearColor(0.16f, 0.2f, 0.035f); // bronzed young leaves
		S.SummerLeafColour = FLinearColor(0.035f, 0.11f, 0.022f);
		S.AutumnLeafColour = FLinearColor(0.38f, 0.012f, 0.018f); // crimson first
		S.AutumnLeafColourLate = FLinearColor(0.66f, 0.17f, 0.012f); // scarlet and orange last
		return V;
	}

	/**
	 * Boston ivy, Parthenocissus tricuspidata (девичий виноград триостренный): the vine that drapes old arches and
	 * gateways. Simple glossy three-lobed leaves, 10..20 cm, on long stalks that hold them out from the wall,
	 * overlapping like shingles into a flat close mat; climbs by short tendrils with adhesive discs, 1..2 m a summer
	 * (slower and closer than Virginia creeper). Bronze-red when young, wine-red, purple and orange from early
	 * October, the last leaves gone by mid-November; small blue-black berries.
	 */
	FVineSpecies BostonIvy()
	{
		FVineSpecies V = VirginiaCreeper();
		V.Name = TEXT("BostonIvy");
		FPlantSpecies& S = V.Plant;
		S.Name = V.Name;
		// one blade: three broad lobes, the middle one longest, shallow teeth
		S.Leaf.Form = EPlantLeafForm::Palmate;
		S.Leaf.Length = 0.1f;
		S.Leaf.Width = 1.05f;
		S.Leaf.Lobes = 3;
		S.Leaf.LobeDepth = 0.42f;
		S.Leaf.Teeth = 5;
		S.Leaf.TeethDepth = 0.06f;
		S.Leaf.Fold = 0.06f;
		S.Leaf.PerCluster = 3;
		V.Leaflets = 1;
		V.PetioleLength = 0.085f;
		V.LeafChance = 0.95f;
		S.Sway = 2.f;
		// a tighter mat: slower, more side shoots, closer spacing
		V.Speed = 1.7f;
		V.Spacing = 0.16f;
		V.BranchSpacing = 0.32f;
		V.BranchChance = 0.9f;
		V.Wobble = 0.5f;
		V.SideUpPull = 0.25f;
		V.MaxReach = 7.f;
		V.MaxHeight = 13.f;
		V.FreeLength = 0.55f;
		S.FruitChance = 0.18f;
		S.FruitSize = 0.006f;
		// colours a little later than the creeper and holds its leaves longer
		S.BudBreakDay = 128.f;
		S.FullLeafDay = 155.f;
		S.ColourStartDay = 266.f;
		S.ColourFullDay = 292.f;
		S.LeafDropStartDay = 292.f;
		S.BareDay = 322.f;
		S.SpringLeafColour = FLinearColor(0.2f, 0.12f, 0.03f);   // bronze-red young leaves
		S.SummerLeafColour = FLinearColor(0.03f, 0.095f, 0.02f); // dark glossy green
		S.AutumnLeafColour = FLinearColor(0.26f, 0.012f, 0.03f);  // wine-red and purple first
		S.AutumnLeafColourLate = FLinearColor(0.6f, 0.11f, 0.015f); // then scarlet and orange
		return V;
	}
}

const TArray<FVineSpecies>& VineSpeciesLibrary::All()
{
	static const TArray<FVineSpecies> Species = { VirginiaCreeper(), BostonIvy() };
	return Species;
}

const FVineSpecies* VineSpeciesLibrary::Find(FName Name)
{
	return All().FindByPredicate([Name](const FVineSpecies& S) { return S.Name == Name; });
}

TArray<FName> VineSpeciesLibrary::Names()
{
	TArray<FName> Out;
	for (const FVineSpecies& S : All()) Out.Add(S.Name);
	return Out;
}
