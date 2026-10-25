#pragma once

#include "CoreMinimal.h"
#include "GrassSpecies.generated.h"

UENUM(BlueprintType)
enum class EGrassHabitat : uint8
{
	/** Meadow and forest floor: follows the terrain's grass density, thinner in wet hollows. */
	Meadow,
	/** The dry upper beach and foredune: follows the terrain's dune grass density. */
	Dune,
	/** Gullies, hollows and ruts where water stands: follows the terrain's wetness. */
	Wet,
};

/**
 * The rules one kind of grass is built by: a tuft of curved blades (a mesh made in code), scattered by
 * AProceduralGrass. Like FPlantSpecies a new kind is a new set of these numbers (GrassSpeciesLibrary),
 * not new code.
 *
 * Units: metres, degrees, days of the year (1 = 1 January).
 */
USTRUCT(BlueprintType)
struct FGrassSpecies
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass") FName Name;

	/** Where it grows on the terrain (ULyudTerrainMask). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass") EGrassHabitat Habitat = EGrassHabitat::Meadow;

	/** Multiplies the patch's NearDistance and FarDistance (a low carpet is only worth drawing close by). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass") float CullScale = 1.f;

	// --- Tuft

	/** Blade length, m; each tuft is scaled by 1 +- HeightVariation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuft") float Height = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuft") float HeightVariation = 0.3f;
	/** Blade width at its base, m (it tapers to a point). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuft") float BladeWidth = 0.012f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuft", meta = (ClampMin = "3", ClampMax = "40")) int32 Blades = 11;
	/** Blades rise from a ring this wide, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuft") float TuftRadius = 0.07f;
	/** Angle off the vertical a blade leaves the ground at, degrees (stiff upright grass: small), and how far it arches over by its tip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuft") float Lean = 12.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuft") float Arch = 55.f;
	/** Tufts per square metre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuft") float Density = 5.f;
	/** How far the blade tips swing in the wind, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuft") float Sway = 7.f;
	/** Fraction of its height it stands at in the dead of winter (flattened by snow: low; reed and marram stay up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuft") float WinterHeight = 0.5f;

	// --- Colour (linear): green base and tip, and the straw it dries to

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor GreenBase = FLinearColor(0.02f, 0.06f, 0.012f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor GreenTip = FLinearColor(0.08f, 0.19f, 0.035f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor DryBase = FLinearColor(0.1f, 0.075f, 0.03f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor DryTip = FLinearColor(0.34f, 0.25f, 0.09f);

	// --- Season, day of the year: green-up in spring, and the turn to straw in autumn

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Season") float GreenUpStart = 95.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Season") float GreenUpFull = 135.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Season") float YellowStart = 245.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Season") float YellowFull = 300.f;
};

/** The built-in grasses. Add one: write its preset in GrassSpecies.cpp and list it in All(). */
namespace GrassSpeciesLibrary
{
	LYUDMILOVSK_API const TArray<FGrassSpecies>& All();
	/** nullptr when there is no such grass. */
	LYUDMILOVSK_API const FGrassSpecies* Find(FName Name);
	LYUDMILOVSK_API TArray<FName> Names();
	/** The grass's name in the player's language (grass.<name, lower case>.name in the string tables). */
	LYUDMILOVSK_API FText DisplayName(const FGrassSpecies& S);
}
