#include "Plants/GrassSpecies.h"

#include "LyudText.h"

namespace
{
	FLinearColor C(float R, float G, float B) { return FLinearColor(R, G, B); }

	/** Festuca rubra, red fescue (овсяница красная): the fine, tufted turf of the grey dunes and the clearings behind them. */
	FGrassSpecies RedFescue()
	{
		FGrassSpecies S;
		S.Name = TEXT("RedFescue");
		S.Height = 0.35f;
		S.BladeWidth = 0.004f;
		S.Blades = 14;
		S.TuftRadius = 0.05f;
		S.Lean = 15.f;
		S.Arch = 60.f;
		S.Density = 9.f;
		S.Sway = 6.f;
		S.WinterHeight = 0.5f;
		S.GreenBase = C(0.025f, 0.07f, 0.015f);
		S.GreenTip = C(0.1f, 0.2f, 0.04f);
		S.DryBase = C(0.12f, 0.09f, 0.035f);
		S.DryTip = C(0.4f, 0.3f, 0.1f);
		return S;
	}

	/**
	 * The short, dense turf under the taller grasses: what a bird low over the meadow sees as ground. Lay it
	 * over the same patches as the tall ones; it is drawn only within ~15 m. Stays green well into autumn.
	 */
	FGrassSpecies Turf()
	{
		FGrassSpecies S;
		S.Name = TEXT("Turf");
		S.Height = 0.16f;
		S.HeightVariation = 0.35f;
		S.BladeWidth = 0.004f;
		S.Blades = 18;
		S.TuftRadius = 0.065f;
		S.Lean = 35.f;
		S.Arch = 95.f;
		S.Density = 55.f;
		S.Sway = 2.f;
		S.WinterHeight = 0.6f;
		S.CullScale = 0.45f;
		S.GreenBase = C(0.025f, 0.07f, 0.015f);
		S.GreenTip = C(0.07f, 0.17f, 0.03f);
		S.DryBase = C(0.1f, 0.08f, 0.03f);
		S.DryTip = C(0.26f, 0.2f, 0.08f);
		S.GreenUpStart = 90.f;
		S.GreenUpFull = 120.f;
		S.YellowStart = 285.f;
		S.YellowFull = 335.f;
		return S;
	}

	/** Poa pratensis, meadow-grass (мятлик луговой): a soft green sward in the clearings, bleached to straw by October. */
	FGrassSpecies MeadowGrass()
	{
		FGrassSpecies S;
		S.Name = TEXT("MeadowGrass");
		S.Height = 0.5f;
		S.BladeWidth = 0.008f;
		S.Blades = 12;
		S.TuftRadius = 0.07f;
		S.Lean = 12.f;
		S.Arch = 55.f;
		S.Density = 6.f;
		S.Sway = 7.f;
		S.WinterHeight = 0.4f;
		S.GreenBase = C(0.03f, 0.075f, 0.015f);
		S.GreenTip = C(0.11f, 0.22f, 0.04f);
		S.DryBase = C(0.13f, 0.1f, 0.04f);
		S.DryTip = C(0.42f, 0.32f, 0.11f);
		S.YellowStart = 240.f;
		S.YellowFull = 292.f;
		return S;
	}

	/** Ammophila arenaria, marram grass (песчаный колосняк, пустынник): the stiff grey-green tussocks that build the foredune. */
	FGrassSpecies MarramGrass()
	{
		FGrassSpecies S;
		S.Name = TEXT("MarramGrass");
		S.Habitat = EGrassHabitat::Dune;
		S.Height = 0.8f;
		S.HeightVariation = 0.25f;
		S.BladeWidth = 0.007f;
		S.Blades = 12;
		S.TuftRadius = 0.1f;
		S.Lean = 8.f;
		S.Arch = 35.f;
		S.Density = 2.5f;
		S.Sway = 9.f;
		S.WinterHeight = 0.75f;
		S.GreenBase = C(0.03f, 0.06f, 0.03f);
		S.GreenTip = C(0.11f, 0.17f, 0.07f);
		S.DryBase = C(0.13f, 0.1f, 0.05f);
		S.DryTip = C(0.45f, 0.37f, 0.17f);
		S.GreenUpStart = 105.f;
		S.GreenUpFull = 140.f;
		S.YellowStart = 255.f;
		S.YellowFull = 305.f;
		return S;
	}

	/** Leymus arenarius, sea lyme grass (волоснец песчаный): broad, blue-grey, stiff leaves in big clumps on the open sand. */
	FGrassSpecies SeaLymeGrass()
	{
		FGrassSpecies S;
		S.Name = TEXT("SeaLymeGrass");
		S.Habitat = EGrassHabitat::Dune;
		S.Height = 1.0f;
		S.BladeWidth = 0.016f;
		S.Blades = 9;
		S.TuftRadius = 0.12f;
		S.Lean = 10.f;
		S.Arch = 40.f;
		S.Density = 1.8f;
		S.Sway = 10.f;
		S.WinterHeight = 0.7f;
		S.GreenBase = C(0.03f, 0.06f, 0.05f);
		S.GreenTip = C(0.1f, 0.17f, 0.13f);
		S.DryBase = C(0.14f, 0.11f, 0.06f);
		S.DryTip = C(0.5f, 0.42f, 0.2f);
		S.GreenUpStart = 105.f;
		S.GreenUpFull = 140.f;
		S.YellowStart = 250.f;
		S.YellowFull = 300.f;
		return S;
	}

	/** Phragmites australis, common reed (тростник): tall stands in the wet ground behind the dunes; the dead stems stand all winter. */
	FGrassSpecies CommonReed()
	{
		FGrassSpecies S;
		S.Name = TEXT("CommonReed");
		S.Habitat = EGrassHabitat::Wet;
		S.Height = 2.3f;
		S.HeightVariation = 0.2f;
		S.BladeWidth = 0.025f;
		S.Blades = 8;
		S.TuftRadius = 0.25f;
		S.Lean = 6.f;
		S.Arch = 45.f;
		S.Density = 1.2f;
		S.Sway = 22.f;
		S.WinterHeight = 0.85f;
		S.GreenBase = C(0.03f, 0.08f, 0.015f);
		S.GreenTip = C(0.1f, 0.2f, 0.04f);
		S.DryBase = C(0.17f, 0.12f, 0.05f);
		S.DryTip = C(0.5f, 0.38f, 0.14f);
		S.YellowStart = 240.f;
		S.YellowFull = 290.f;
		return S;
	}

	/** Carex arenaria, sand sedge (осока песчаная): low, wiry runners that knit loose sand. */
	FGrassSpecies SandSedge()
	{
		FGrassSpecies S;
		S.Name = TEXT("SandSedge");
		S.Habitat = EGrassHabitat::Wet;
		S.Height = 0.28f;
		S.BladeWidth = 0.005f;
		S.Blades = 10;
		S.TuftRadius = 0.06f;
		S.Lean = 25.f;
		S.Arch = 70.f;
		S.Density = 7.f;
		S.Sway = 5.f;
		S.WinterHeight = 0.6f;
		S.GreenBase = C(0.03f, 0.08f, 0.02f);
		S.GreenTip = C(0.12f, 0.22f, 0.05f);
		S.DryBase = C(0.13f, 0.1f, 0.04f);
		S.DryTip = C(0.38f, 0.29f, 0.1f);
		return S;
	}

	/** Deschampsia flexuosa, wavy hair-grass (луговик извилистый): fountains of hair-fine blades under the pines. */
	FGrassSpecies WavyHairGrass()
	{
		FGrassSpecies S;
		S.Name = TEXT("WavyHairGrass");
		S.Height = 0.45f;
		S.BladeWidth = 0.003f;
		S.Blades = 16;
		S.TuftRadius = 0.07f;
		S.Lean = 14.f;
		S.Arch = 75.f;
		S.Density = 6.f;
		S.Sway = 9.f;
		S.WinterHeight = 0.55f;
		S.GreenBase = C(0.025f, 0.065f, 0.02f);
		S.GreenTip = C(0.09f, 0.19f, 0.05f);
		S.DryBase = C(0.12f, 0.085f, 0.04f);
		S.DryTip = C(0.4f, 0.28f, 0.11f);
		return S;
	}

	/** An evergreen moss or lichen: no straw season, it stands the same height all year (snow covers it). */
	void Evergreen(FGrassSpecies& S)
	{
		S.GreenUpStart = -30.f;
		S.GreenUpFull = -20.f;
		S.YellowStart = 400.f;
		S.YellowFull = 420.f;
		S.WinterHeight = 1.f;
	}

	/** Polytrichum commune, common haircap moss (кукушкин лён): dark green bristly shoots in tight cushions, damp hollows of the forest floor. */
	FGrassSpecies HaircapMoss()
	{
		FGrassSpecies S;
		S.Name = TEXT("HaircapMoss");
		S.Height = 0.07f;
		S.HeightVariation = 0.35f;
		S.BladeWidth = 0.0028f;
		S.Blades = 22;
		S.TuftRadius = 0.045f;
		S.Lean = 10.f;
		S.Arch = 25.f;
		S.Density = 28.f;
		S.Sway = 0.4f;
		S.CullScale = 0.4f;
		S.GreenBase = C(0.012f, 0.03f, 0.008f);
		S.GreenTip = C(0.05f, 0.11f, 0.02f);
		S.DryBase = C(0.04f, 0.035f, 0.015f);
		S.DryTip = C(0.14f, 0.1f, 0.035f);
		Evergreen(S);
		return S;
	}

	/** Pleurozium schreberi, red-stemmed feather moss (плеуроциум): the loose yellow-green carpet of the pine and spruce woods. */
	FGrassSpecies FeatherMoss()
	{
		FGrassSpecies S;
		S.Name = TEXT("FeatherMoss");
		S.Height = 0.055f;
		S.HeightVariation = 0.3f;
		S.BladeWidth = 0.004f;
		S.Blades = 26;
		S.TuftRadius = 0.07f;
		S.Lean = 55.f;
		S.Arch = 105.f;
		S.Density = 34.f;
		S.Sway = 0.2f;
		S.CullScale = 0.35f;
		S.GreenBase = C(0.03f, 0.05f, 0.01f);
		S.GreenTip = C(0.13f, 0.2f, 0.035f);
		S.DryBase = C(0.06f, 0.05f, 0.02f);
		S.DryTip = C(0.2f, 0.17f, 0.06f);
		Evergreen(S);
		return S;
	}

	/** Cladonia, reindeer lichen (ягель): pale grey-green branched tufts in the driest, lightest pine woods and on the old dunes. */
	FGrassSpecies ReindeerLichen()
	{
		FGrassSpecies S;
		S.Name = TEXT("ReindeerLichen");
		S.Height = 0.06f;
		S.HeightVariation = 0.3f;
		S.BladeWidth = 0.006f;
		S.Blades = 20;
		S.TuftRadius = 0.06f;
		S.Lean = 40.f;
		S.Arch = 140.f;
		S.Density = 20.f;
		S.Sway = 0.f;
		S.CullScale = 0.35f;
		S.GreenBase = C(0.28f, 0.29f, 0.24f);
		S.GreenTip = C(0.55f, 0.57f, 0.5f);
		S.DryBase = C(0.3f, 0.29f, 0.24f);
		S.DryTip = C(0.6f, 0.58f, 0.5f);
		Evergreen(S);
		return S;
	}
}

const TArray<FGrassSpecies>& GrassSpeciesLibrary::All()
{
	static const TArray<FGrassSpecies> Species = { RedFescue(), MeadowGrass(), Turf(), MarramGrass(), SeaLymeGrass(), CommonReed(), SandSedge(), WavyHairGrass(), HaircapMoss(), FeatherMoss(), ReindeerLichen() };
	return Species;
}

const FGrassSpecies* GrassSpeciesLibrary::Find(FName Name)
{
	return All().FindByPredicate([Name](const FGrassSpecies& S) { return S.Name == Name; });
}

TArray<FName> GrassSpeciesLibrary::Names()
{
	TArray<FName> Out;
	for (const FGrassSpecies& S : All())
	{
		Out.Add(S.Name);
	}
	return Out;
}

FText GrassSpeciesLibrary::DisplayName(const FGrassSpecies& S)
{
	return LyudText::Get(FString::Printf(TEXT("grass.%s.name"), *S.Name.ToString().ToLower()));
}
