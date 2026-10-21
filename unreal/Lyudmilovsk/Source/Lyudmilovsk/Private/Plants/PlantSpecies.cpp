#include "Plants/PlantSpecies.h"

namespace
{
	/** Populus nigra 'Italica', the pyramidal (Lombardy) poplar: a narrow column ~25 m tall, golden in late October. */
	FPlantSpecies PyramidalPoplar()
	{
		FPlantSpecies S;
		S.Name = TEXT("PyramidalPoplar");
		S.DisplayName = NSLOCTEXT("Plants", "PyramidalPoplar", "Pyramidal poplar");
		return S; // the struct defaults are this species
	}

	/** Populus nigra, the black poplar: a broad, open crown on a thick trunk. */
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
		S.BarkColour = FLinearColor(0.03f, 0.028f, 0.024f);
		S.AutumnLeafColour = FLinearColor(0.55f, 0.40f, 0.05f);
		S.ColourStartDay = 276.f;
		S.BareDay = 322.f;
		return S;
	}
}

const TArray<FPlantSpecies>& PlantSpeciesLibrary::All()
{
	static const TArray<FPlantSpecies> Species = { PyramidalPoplar(), BlackPoplar() };
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
