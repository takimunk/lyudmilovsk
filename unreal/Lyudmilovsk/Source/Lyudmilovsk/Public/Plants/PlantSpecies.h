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
	/**
	 * Needles (pine, spruce, juniper), never modelled one by one: a cluster is Cards flat cards, each a fan
	 * of PerCluster needles painted into an alpha-masked texture (M_PlantNeedle), Length long. Lobes = 2
	 * pairs the needles (a pine's fascicles), Lobes = 3 gives a juniper's whorls of three.
	 */
	Needle,
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
	/** Needle: flat cards in one cluster (a card is about 2 triangles however many needles it shows). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaf") int32 Cards = 3;
	/** Leaves in one cluster (one instance); needles: needles painted on each card. */
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
	/** White, peeling in thin horizontal strips, with black lenticel dashes (birch). */
	Birch,
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
	/** Plates pattern: share of plates already flaked off, showing the younger bark under them in UnderColour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bark") float FlakeChance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bark") FLinearColor UnderColour = FLinearColor(0.08f, 0.045f, 0.02f);
	/** Twigs thinner than TwigRadius (m) keep their young bark, TwigColour (birch: red-brown). 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bark") float TwigRadius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bark") FLinearColor TwigColour = FLinearColor(0.08f, 0.035f, 0.02f);
	/**
	 * Old trunks go rough and dark from the ground up (silver birch): DarkBaseRate metres a year, up to
	 * DarkBaseMax, deeply fissured between BaseFurrowColour and BaseRidgeColour. 0 = none.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bark") float DarkBaseRate = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bark") float DarkBaseMax = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bark") FLinearColor BaseRidgeColour = FLinearColor(0.06f, 0.06f, 0.055f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bark") FLinearColor BaseFurrowColour = FLinearColor(0.01f, 0.01f, 0.01f);
	/**
	 * Birch marks, unique to each tree (its seed): black chevrons over every branch and over the scars
	 * of shed ones, and large dark lenticels, drawn into a mask laid over the trunk and limbs. 0 = none.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bark") float Scars = 0.f;
};

UENUM(BlueprintType)
enum class EPlantHerbForm : uint8
{
	/** Not a herb: a tree or shrub of trunk and branches (the rest of FPlantSpecies). */
	None,
	/**
	 * Leaves from the base: a rosette (violet, aloe), a clump of fans of strap leaves (day lily, sansevieria) or
	 * long-stalked blades (aspidistra, monstera); flower scapes rise above it.
	 */
	Rosette,
	/** Stems from the base, branching, leaves along them and flowers at their tips (marigold, aster, geranium; a cane: dracaena). */
	Bush,
	/** Stems that leave over the pot's rim and hang (tradescantia, asparagus fern). */
	Trailing,
	/** A ribbed column, with arms (cactus). */
	Column,
};

UENUM(BlueprintType)
enum class EPlantFlowerForm : uint8
{
	None,
	/** A disc and a ring of rays (aster, cosmos). */
	Daisy,
	/** Six tepals flaring from a tube (day lily). */
	Trumpet,
	/** A ball of ruffled petals (marigold). */
	Pompon,
	/** Five round petals (begonia, violet, lemon blossom). */
	Five,
	/** A head of small five-petalled florets (geranium). */
	Umbel,
};

UENUM(BlueprintType)
enum class EPlantVariegation : uint8
{
	None,
	/** A pale band down the middle (chlorophytum). */
	CentralStripe,
	/** Pale edges (sansevieria 'Laurentii', dracaena marginata red edge). */
	Margin,
	/** Wavy dark bands across (sansevieria). */
	Bands,
	/** Pale blotches between the veins (dieffenbachia). */
	Blotches,
	/** Lengthwise stripes, silver on purple underneath (tradescantia). */
	Stripes,
};

UENUM(BlueprintType)
enum class EPlantPot : uint8
{
	/** The species' own (a house plant's: terracotta; outdoor plants: none). */
	Auto,
	None,
	Terracotta,
	/** Glazed, dark green. */
	Ceramic,
	/** Plain white plastic. */
	White,
};

/**
 * A herbaceous plant or house plant: what grows from the ground (or the pot) is generated from these
 * numbers, the leaf blades from the species' Leaf shape. Everything scales with its maturity, 0 (a seedling)
 * .. 1 (MatureYears old; an annual: grown up two months after its sowing day).
 */
USTRUCT(BlueprintType)
struct FPlantHerb
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb") EPlantHerbForm Form = EPlantHerbForm::None;
	/** Mature height (without flowers) and width, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb") float Height = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb") float Spread = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb") float MatureYears = 2.f;
	/**
	 * Hard bounds of an indoor plant, m (0 = derived: 1.3 x Height plus the flower scapes, 0.65 x Spread): whatever
	 * a seed or age would grow beyond them is pressed back, so a house plant fits the spot it was placed in.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb") float MaxHeight = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb") float MaxRadius = 0.f;
	/** Sown in spring and killed by the first frost (marigold, cosmos); its age counts within the year. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb") bool bAnnual = false;
	/** A shrub: its stems stand through the winter (only the leaves come and go). With CrownBox: a clipped hedge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb") bool bWoody = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb") float SowDay = 135.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb") float FrostDay = 290.f;

	// --- Leaves (blade outline: the species' Leaf)

	/** Rosette: leaves of a mature plant; Bush / Trailing: leaves per stem. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Leaves") int32 Leaves = 12;
	/** Rosette: fans (clumps of two-ranked strap leaves, as a day lily or sansevieria); 0 = one spiral rosette. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Leaves") int32 Fans = 0;
	/** Angle off the vertical a leaf leaves at, inner (young) and outer (old) leaves, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Leaves") FVector2D Rise = FVector2D(20.0, 60.0);
	/** How much a leaf bends down along its length, degrees from base to tip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Leaves") float Arch = 30.f;
	/** Twist along the leaf, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Leaves") float Twist = 0.f;
	/** 1: the Leaf outline's taper; below 1 flatter (strap leaves), above sharper. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Leaves") float ProfilePower = 1.f;
	/** Succulent: the blade's thickness as a fraction of its width (aloe); 0 = thin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Leaves") float Thickness = 0.f;
	/** Compound leaves: leaflet pairs along the rachis (palm frond, fern, cosmos); 0 = simple. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Leaves") int32 Leaflets = 0;
	/** Leaflet length / leaf length, and leaflet width / leaflet length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Leaves") FVector2D Leaflet = FVector2D(0.3, 0.12);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Leaves") EPlantVariegation Variegation = EPlantVariegation::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Leaves") FLinearColor VariegationColour = FLinearColor(0.75f, 0.75f, 0.55f);

	// --- Stems (Bush, Trailing)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Stems") int32 Stems = 5;
	/** Side branches per stem. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Stems") int32 Branches = 2;
	/** Lean of the stems off the vertical, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Stems") float Lean = 15.f;
	/** Base radius of a stem, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Stems") float StemRadius = 0.004f;
	/** The lower part of a stem without leaves, fraction (a dracaena's cane: 0.85). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Stems") float BareStem = 0.15f;
	/** Leaves in a whorl at each node (1 alternate, 2 opposite). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Stems") int32 PerNode = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Stems") FLinearColor StemColour = FLinearColor(0.08f, 0.14f, 0.03f);

	// --- Column (cactus)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Column") int32 Ribs = 8;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Column") int32 Arms = 0;

	// --- Flowers

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Flowers") EPlantFlowerForm Flower = EPlantFlowerForm::None;
	/** Flower diameter, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Flowers") float FlowerSize = 0.05f;
	/** Rosette: flower scapes (stalks) and their height above the leaves, m; Bush: flowers per stem. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Flowers") int32 Flowers = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Flowers") float ScapeHeight = 0.2f;
	/** Petals (rays of a daisy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Flowers") int32 Petals = 12;
	/** Petal colours: each flower takes one of them (by seed); and the centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Flowers") TArray<FLinearColor> FlowerColours;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Flowers") FLinearColor CentreColour = FLinearColor(0.6f, 0.4f, 0.02f);
	/** In bloom between these days of the year; BloomStart <= 0: all year (a house plant). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Flowers") float BloomStart = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herb|Flowers") float BloomEnd = 0.f;
};

/**
 * The rules one plant species grows by. Every tree of a species is the same function of
 * (seed, age, day of year); a new species is a new set of these numbers (PlantSpeciesLibrary),
 * not new code.
 *
 * Units: metres, years, days of the year (1 = 1 January).
 */
UENUM(BlueprintType)
enum class EPlantBlossomForm : uint8
{
	None,
	/** Pendulous tassels hanging from the twig tips, PerCluster to an instance (birch, alder, poplar, aspen, oak). */
	Catkin,
	/** Clusters of PerCluster small open flowers of Petals petals (apple, rowan, maple, sea buckthorn). */
	Blossom,
	/** Upright egg-shaped cones, PerCluster to an instance: pine and spruce flowers, a juniper's. */
	Cone,
};

/**
 * The flowering of a tree or shrub: pollen catkins, blossom, flowering cones. A pure function of the day of
 * the year (see PlantModel::Season): opens from StartDay, is full at PeakDay and has gone by EndDay, shifted by
 * the tree's own phenology jitter. Drawn as instances on the twigs near the camera; far away the crown and twigs
 * only take on its colour (FarTint).
 */
USTRUCT(BlueprintType)
struct FPlantBlossom
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blossom") EPlantBlossomForm Form = EPlantBlossomForm::None;
	/** Days of the year: first opening, in full bloom, and the last of it fallen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blossom") float StartDay = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blossom") float PeakDay = 115.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blossom") float EndDay = 135.f;
	/** Catkin and cone: length; blossom: one flower's diameter. m */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blossom") float Size = 0.05f;
	/** Catkin and cone: width / length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blossom") float Thickness = 0.1f;
	/** Catkins, cones or flowers in one instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blossom") int32 PerCluster = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blossom") int32 Petals = 5;
	/** Blossom: how wide a cluster spreads, in flower diameters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blossom") float Spread = 2.5f;
	/** How far above the twig it is carried, m (flowers and cones stand clear of the foliage). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blossom") float Lift = 0.f;
	/** Chance that a twig (a spur, on a tree that has them) bears any, and how many instances at most. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blossom") float Chance = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blossom") int32 PerTwig = 2;
	/** Trees flower from this age, years. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blossom") float MinAge = 5.f;
	/** Fresh colour (petals; the catkin's or cone's body), the flower's centre, and the tint it ages to (catkin and cone: its aged colour; blossom: a multiplier on the petals). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blossom") FLinearColor FreshColour = FLinearColor(0.9f, 0.8f, 0.8f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blossom") FLinearColor CentreColour = FLinearColor(0.7f, 0.5f, 0.05f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blossom") FLinearColor AgedColour = FLinearColor(0.5f, 0.4f, 0.3f);
	/** How much it colours the crown and twigs seen from afar (a baked tree), 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blossom") float FarTint = 0.4f;
};

USTRUCT(BlueprintType)
struct FPlantSpecies
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Species") FName Name;
	/** The level-0 crown from afar is a cone (spruce, pine) instead of an ellipsoid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Species") bool bConicalCrown = false;

	// --- Height: Chapman-Richards curve H(age) = MaxHeight * (1 - exp(-GrowthRate * age))^GrowthShape

	/** Height the species levels off at, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float MaxHeight = 28.f;
	/** 1/year; larger grows up sooner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float GrowthRate = 0.08f;
	/** Above 1 the first years are slow (a sprout), then it shoots up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float GrowthShape = 1.6f;
	/**
	 * The trunk (leader) stops at this fraction of MaxHeight and its top limbs take over: young trees
	 * have a central leader, old oaks and apples fork into a few big limbs.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float LeaderFraction = 1.f;
	/** Trunk base radius per metre of height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float TrunkRadiusPerHeight = 0.012f;
	/** Wood added to the trunk's radius every year on top of the height rule, m (an annual ring): old trees keep thickening after they stop growing up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float RingWidth = 0.f;
	/** Trunk radius never goes below this, m (a sprout's stem). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float MinRadius = 0.004f;
	/**
	 * Lifespan: each tree draws its own from its seed, log-normal about LifeMedian years (LifeSpread is
	 * the log-sd), never past LifeMax. Over its last DeclineYears the crown thins; dead, it stands as a
	 * bleached snag for SnagYears, then is gone.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Life") float LifeMedian = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Life") float LifeSpread = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Life") float LifeMax = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Life") float DeclineYears = 12.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Life") float SnagYears = 8.f;
	/** Branches on the trunk below CrownBase * height are shed (self-pruning). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float CrownBase = 0.12f;

	// --- Branches: order 1 grow off the trunk, order 2 off those. Each branch is a fixed curve that
	// only ever lengthens: L(t) = MaxLength * (1 - exp(-t / LengthTau)), t = years since it budded.

	/** Spacing of order-1 branches up the trunk, m (one per node, golden-angle phyllotaxis). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float TrunkNodeSpacing = 0.28f;
	/** Order-1 branches per node: above 1 they leave the trunk at one height, evenly around it, in a whorl (pine, spruce). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches", meta = (ClampMin = "1")) int32 WhorlSize = 1;
	/** Order-1 angle off the trunk, degrees (pyramidal poplar: steep, nearly along the trunk). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Branch1Angle = 26.f;
	/** Order-1 angle at the top of the trunk; below 0 the same as Branch1Angle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Branch1AngleTop = -1.f;
	/** Longest an order-1 branch gets, m; scaled by Branch1Profile along the trunk. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Branch1MaxLength = 6.f;
	/** Order-1 length cap at the bottom / middle / top of the mature trunk (crown silhouette). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") FVector Branch1Profile = FVector(0.55, 1.0, 0.35);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float LengthTau = 7.f;
	/** How hard shoots bend toward the sky, 1/m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Tropism = 0.06f;
	/** Random bend of shoots, radians per metre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Wobble = 0.12f;
	/**
	 * Sympodial kinks: every KinkSpacing metres the shoot's tip flowered or died and a side bud took
	 * over, turning it by up to Kink radians (apple, oak: crooked limbs). 0 = none.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Kink = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float KinkSpacing = 0.5f;
	/** Order-2 twig thickness, times the branch rule (birch twigs are hair-thin). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float TwigRadiusScale = 1.f;
	/** How far the tips of twigs swing in the wind, cm (and the leaves with them). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Sway = 8.f;
	/** Order-2 twigs hang: 0 they grow like the limbs, 1 they hang straight down (silver birch's weeping twigs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float TwigHang = 0.f;
	/** Long limbs sag under their own weight and fruit: downward pull growing with the distance out, 1/m^2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Droop = 0.f;
	/**
	 * The mature crown's width / MaxHeight; branches stop where they would leave that dome (a regular,
	 * rounded crown, as in open-grown oaks). 0 = no envelope.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float CrownWidth = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Branch2Spacing = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Branch2Angle = 38.f;
	/** Order-2 cap as a fraction of the parent's remaining length past the bud. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Branch2LengthRatio = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Branch2MaxLength = 1.6f;
	/** Order-2 twigs this far inside the parent's tip are shaded out and shed, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float Branch2ShadeDepth = 4.f;
	/**
	 * Spurs: short, slow shoots every SpurSpacing metres along order-1 and order-2 wood older than
	 * SpurAge years, each ending in a rosette of leaves and, on fruit trees, the flowers and fruit
	 * (apple). 0 = none.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float SpurSpacing = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float SpurLength = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float SpurAge = 2.f;
	/** Branch base radius = BranchRadiusBase + BranchRadiusPerLength * length, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float BranchRadiusBase = 0.003f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") float BranchRadiusPerLength = 0.008f;

	// --- Leaves

	/** Keeps its leaves all year (conifers, juniper): no bud break, no leaf fall; the autumn colours become its winter colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") bool bEvergreen = false;
	/** A house plant: indoors, no seasons (always in summer leaf), no wind, no snow, nothing falls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Species") bool bIndoor = false;
	/** A herb or house plant (Herb.Form != None): the herb generator draws it instead of trunk and branches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Species") FPlantHerb Herb;
	/** Its pot (house plants), diameter m; 0 = in the ground. An actor can choose another pot or none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Species") float PotDiameter = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Species") EPlantPot Pot = EPlantPot::Terracotta;
	/**
	 * Clipped to a box (a hedge): width along the row, depth, height, m; 0 = grows free. Branches stop at it,
	 * so shrubs planted in a row a little closer than their width make one wall.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branches") FVector CrownBox = FVector::ZeroVector;
	/** Drawn as its bake (UTreeBakeCache: one mesh shared by every plant of the same kind) at every distance: hedges, house trees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Species") bool bPreferBake = false;
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

	/** Its flowering: catkins, blossom or flowering cones in spring; Form None = none to see. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blossom") FPlantBlossom Blossom;

	// --- Fruit (apples, acorns): on order-2 twigs from FruitDay until FruitFallDay; none if FruitSize is 0.

	/** The inventory item a picked-up fruit becomes (ULyudInventory's table); None: it can't be picked up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") FName FruitItem;
	/** Fruit diameter, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") float FruitSize = 0.f;
	/** Fruit per bearing twig (rowan and sea buckthorn berries, a juniper's), spread along the twig. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") int32 FruitCluster = 1;
	/** Height / width of a fruit (a spruce cone is long). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") float FruitAspect = 0.92f;
	/** Chance that a twig bears one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") float FruitChance = 0.3f;
	/** Trees bear fruit from this age, years. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") float FruitingAge = 5.f;
	/** Fruit set (small and green), fully grown and coloured, and fallen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") float FruitSetDay = 160.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") float FruitRipeDay = 250.f;
	/**
	 * Fruit drops one by one over the 35 days up to FruitFallDay, then lies on the ground this long. A
	 * FruitFallDay past 366 keeps the fruit on the tree into the next year (berries, cones) until that day
	 * of the year; berry clusters (FruitCluster > 1) and fruit kept that long just go (eaten, withered)
	 * and are not simulated on the ground.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") float FruitFallDay = 310.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") float FruitRotDays = 25.f;
	/** Fruit skin: unripe ground colour, ripe ground colour, and the blush that develops over it (red on apples). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") FLinearColor FruitUnripeColour = FLinearColor(0.15f, 0.3f, 0.04f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") FLinearColor FruitGroundColour = FLinearColor(0.45f, 0.42f, 0.06f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") FLinearColor FruitColour = FLinearColor(0.5f, 0.04f, 0.02f);
	/** How much of a fruit's skin can blush (0 = never, acorns), and how striped the blush is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") float FruitBlush = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fruit") float FruitStripes = 0.f;

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
	/** Fallen leaves lie under the tree this long, days. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Season") float LitterDays = 50.f;

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
	/** The species' name in the player's language (plant.<name, lower case>.name in the string tables). */
	LYUDMILOVSK_API FText DisplayName(const FPlantSpecies& S);
}
