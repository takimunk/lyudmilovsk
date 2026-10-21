#pragma once

#include "CoreMinimal.h"
#include "PlantSpecies.generated.h"

UENUM(BlueprintType)
enum class EPlantLeafForm : uint8
{
	/** One midrib, the blade's half-width a function of the distance along it (poplar, apple, oak). */
	Pinnate,
	/** Lobes radiating from the petiole (maple). */
	Palmate,
};

/** A leaf blade's outline, built into the leaf mesh in code. */
USTRUCT(BlueprintType)
struct FPlantLeafShape
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaf") EPlantLeafForm Form = EPlantLeafForm::Pinnate;
	/** Blade length, base to tip, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaf") float Length = 0.09f;
	/** Widest width / length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaf") float Width = 0.95f;
	/** Where along the midrib it is widest, 0 base .. 1 tip (pinnate). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaf") float WidestAt = 0.3f;
	/** Pinnate: rounded lobes per side; palmate: number of lobes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaf") int32 Lobes = 0;
	/** Sinus depth between lobes, fraction of the width. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaf") float LobeDepth = 0.f;
	/** Marginal teeth per side, and their depth (fraction of the width). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaf") int32 Teeth = 12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaf") float TeethDepth = 0.03f;
	/** Leaf stalk, fraction of the blade length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaf") float Petiole = 0.6f;
	/** How far the halves fold up along the midrib, fraction of the width. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaf") float Fold = 0.12f;
	/** Leaves in one cluster (one instance). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaf") int32 PerCluster = 6;
};

UENUM(BlueprintType)
enum class EPlantBarkPattern : uint8
{
	/** Long ridges and deep furrows along the trunk (poplar). */
	Furrows,
	/** Furrows broken across into blocks (oak). */
	Blocks,
	/** Flaking plates (apple). */
	Plates,
	/** Smooth with fine ridges and lenticels (maple). */
	Smooth,
};

/** The bark texture, generated in code per species. */
USTRUCT(BlueprintType)
struct FPlantBark
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bark") EPlantBarkPattern Pattern = EPlantBarkPattern::Furrows;
	/** One texture tile across and along the trunk, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bark") float TileWidth = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bark") float TileHeight = 0.8f;
	/** Relief strength of the normal map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bark") float Depth = 1.f;
	/** Ridge tops and furrow bottoms (linear). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bark") FLinearColor RidgeColour = FLinearColor(0.16f, 0.15f, 0.13f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bark") FLinearColor FurrowColour = FLinearColor(0.035f, 0.03f, 0.025f);
};

/**
 * The rules one plant species grows by. Every tree of a species is the same function of
 * (seed, age, day of year); a new species is a new set of these numbers (PlantSpeciesLibrary),
 * not new code.
 *
 * Units: metres, years, days of the year (1 = 1 January).
 */
USTRUCT(BlueprintType)
struct FPlantSpecies
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Species") FName Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Species") FText DisplayName;

	// --- Height: Chapman-Richards curve H(age) = MaxHeight * (1 - exp(-GrowthRate * age))^GrowthShape

	/** Height the species levels off at, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float MaxHeight = 28.f;
	/** 1/year; larger grows up sooner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float GrowthRate = 0.08f;
	/** Above 1 the first years are slow (a sprout), then it shoots up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float GrowthShape = 1.6f;
	/** The trunk (leader) grows to this fraction of the height; the crown above is its branches (apple, oak). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float LeaderFraction = 1.f;
	/** Trunk base radius per metre of height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float TrunkRadiusPerHeight = 0.012f;
	/** Trunk radius never goes below this, m (a sprout's stem). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float MinRadius = 0.004f;
	/** Branches on the trunk below CrownBase * height are shed (self-pruning). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float CrownBase = 0.12f;

	// --- Branches: order 1 grow off the trunk, order 2 off those. Each branch is a fixed curve that
	// only ever lengthens: L(t) = MaxLength * (1 - exp(-t / LengthTau)), t = years since it budded.

	/** Spacing of order-1 branches up the trunk, m (one per node, golden-angle phyllotaxis). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float TrunkNodeSpacing = 0.28f;
	/** Order-1 angle off the trunk, degrees (pyramidal poplar: steep, nearly along the trunk). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Branch1Angle = 26.f;
	/** Longest an order-1 branch gets, m; scaled by Branch1Profile along the trunk. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Branch1MaxLength = 6.f;
	/** Order-1 length cap at the bottom / middle / top of the mature trunk (crown silhouette). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") FVector Branch1Profile = FVector(0.55, 1.0, 0.35);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float LengthTau = 7.f;
	/** How hard shoots bend toward the sky, 1/m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Tropism = 0.06f;
	/** Random bend of shoots, radians per metre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Wobble = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Branch2Spacing = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Branch2Angle = 38.f;
	/** Order-2 cap as a fraction of the parent's remaining length past the bud. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Branch2LengthRatio = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Branch2MaxLength = 1.6f;
	/** Order-2 twigs this far inside the parent's tip are shaded out and shed, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Branch2ShadeDepth = 4.f;
	/** Branch base radius = BranchRadiusBase + BranchRadiusPerLength * length, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float BranchRadiusBase = 0.003f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float BranchRadiusPerLength = 0.008f;

	// --- Leaves

	/**
	 * Leaves grow on every order-2 twig and on the last LeafyTipLength of the trunk and of order-1
	 * branches: twigs inside the crown are shed (Branch2ShadeDepth), so the leaves form its outer shell.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float LeafyTipLength = 3.5f;
	/** One leaf cluster per this much shoot, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float LeafSpacing = 0.11f;
	/** How far a cluster's leaves spread about its twig, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float LeafClusterSize = 0.42f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") FPlantLeafShape Leaf;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bark") FPlantBark Bark;

	// --- Fruit (apples, acorns): on order-2 twigs from FruitDay until FruitFallDay; none if FruitSize is 0.

	/** Fruit diameter, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") float FruitSize = 0.f;
	/** Chance that a twig bears one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") float FruitChance = 0.3f;
	/** Trees bear fruit from this age, years. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") float FruitingAge = 5.f;
	/** Fruit set (small and green), fully grown and coloured, and fallen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") float FruitSetDay = 160.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") float FruitRipeDay = 250.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") float FruitFallDay = 310.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") FLinearColor FruitUnripeColour = FLinearColor(0.15f, 0.3f, 0.04f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") FLinearColor FruitColour = FLinearColor(0.5f, 0.04f, 0.02f);

	// --- Phenology, day of the year. Each tree shifts these by up to +-PhenologyJitter days (its seed).

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Season") float BudBreakDay = 110.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Season") float FullLeafDay = 135.f;
	/** Leaves start to turn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Season") float ColourStartDay = 272.f;
	/** Leaves are fully autumn coloured. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Season") float ColourFullDay = 298.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Season") float LeafDropStartDay = 288.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Season") float BareDay = 318.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Season") float PhenologyJitter = 5.f;

	// --- Colours (linear)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor SpringLeafColour = FLinearColor(0.20f, 0.36f, 0.06f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor SummerLeafColour = FLinearColor(0.07f, 0.17f, 0.03f);
	/** Autumn colour of the first leaves to turn, and of the last (maple: red to yellow). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor AutumnLeafColour = FLinearColor(0.62f, 0.42f, 0.04f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor AutumnLeafColourLate = FLinearColor(0.62f, 0.42f, 0.04f);
};

/** The built-in species. Add a species: write its preset in PlantSpecies.cpp and list it in All(). */
namespace PlantSpeciesLibrary
{
	LYUDMILOVSK_API const TArray<FPlantSpecies>& All();
	/** nullptr when there is no such species. */
	LYUDMILOVSK_API const FPlantSpecies* Find(FName Name);
	LYUDMILOVSK_API TArray<FName> Names();
}
