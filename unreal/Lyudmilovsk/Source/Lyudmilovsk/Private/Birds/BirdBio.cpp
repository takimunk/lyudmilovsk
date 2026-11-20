#include "BirdBio.h"

#include <cstring>

namespace BirdBio
{
	namespace
	{
		// (the small birds' dive speeds are what a closed-wing body of their mass and frontal area falls at, less a margin:
		// a sparrow's terminal speed is about 30 m/s; nobody has clocked a blue tit's stoop)
		const FBio Table[] = {
			// id                  stamina  dive  agility cruise swims  run-up  sprint climb wind  glide  tuck  pull
			{ "Rook",                    240,    30,   1.0,     0, false,  0.0,     18,   2.0,   14,    0,  0.70, 0.0 },
			{ "HoodedCrow",              240,    30,   1.0,     0, false,  0.0,     18,   2.0,   15,    0,  0.70, 0.0 },
			{ "Jackdaw",                 200,    30,   1.2,     0, false,  0.0,     18,   2.2,   14,    0,  0.85, 4.5 },  // plays in a gale on the cliff
			{ "Magpie",                   50,    25,   1.1,     0, false,  0.0,     13,   1.6,   10,    0,  0.50, 0.0 },  // short broad wings, a poor flier in wind
			{ "FeralPigeon",             300,    40,   1.1,     0, false,  0.0,     24,   3.0,   13,    0,  0.85, 4.5 },  // racing pigeons hold ~20 m/s for hours
			{ "WoodPigeon",              300,    40,   1.0,     0, false,  0.0,     22,   2.8,   14,    0,  0.80, 0.0 },
			{ "HerringGull",             600,    35,   0.9,     0, true,   0.0,     18,   1.5,   24,   15,  0.70, 0.0 },
			{ "BlackHeadedGull",         500,    30,   1.2,     0, true,   0.0,     15,   1.8,   18,   13,  0.70, 0.0 },
			{ "CommonGull",              500,    32,   1.0,     0, true,   0.0,     16,   1.6,   20,   14,  0.70, 0.0 },
			{ "GreatTit",                 35,    22,   1.3,     0, false,  0.0,     12,   2.6,    8,    0,  0.90, 0.0 },
			{ "BlueTit",                  30,    20,   1.4,     0, false,  0.0,     11,   2.5,    7,    0,  0.90, 0.0 },
			{ "Goldfinch",                60,    23,   1.3,     0, false,  0.0,     14,   2.5,    8,    0,  0.90, 0.0 },
			{ "Chaffinch",                60,    25,   1.2,     0, false,  0.0,     15,   2.6,    9,    0,  0.90, 0.0 },
			{ "Bullfinch",                40,    21,   1.0,     0, false,  0.0,     12,   2.2,    7,    0,  0.90, 0.0 },
			{ "HouseSparrow",             40,    23,   1.2,     0, false,  0.0,     13,   2.6,    8,    0,  0.90, 0.0 },
			{ "TreeSparrow",              45,    23,   1.2,     0, false,  0.0,     13,   2.6,    8,    0,  0.90, 0.0 },
			{ "Starling",                180,    36,   1.3,     0, false,  0.0,     20,   2.8,   13,    0,  0.90, 4.5 },
			{ "SongThrush",              120,    30,   1.1,     0, false,  0.0,     16,   2.4,   10,    0,  0.80, 0.0 },
			{ "Fieldfare",               150,    31,   1.0,     0, false,  0.0,     17,   2.4,   12,    0,  0.80, 0.0 },
			{ "Blackbird",                60,    27,   1.1,     0, false,  0.0,     15,   2.4,   10,    0,  0.80, 0.0 },
			{ "Robin",                    45,    20,   1.2,     0, false,  0.0,     12,   2.2,    7,    0,  0.85, 0.0 },
			{ "WhiteWagtail",             90,    22,   1.3,     0, false,  0.0,     14,   2.4,    9,    0,  0.85, 0.0 },
			{ "Waxwing",                 150,    30,   1.1,     0, false,  0.0,     17,   2.4,   11,    0,  0.85, 0.0 },
			{ "CommonSwift",            1200,    45,   1.5,    20, false,  0.0,     31,   2.5,   18,   12,  0.90, 6.0 },  // screaming parties sprint at 31 m/s (Henningsson 2010)
			{ "BarnSwallow",             900,    35,   1.6,     0, false,  0.0,     18,   2.2,   12,   10,  0.85, 6.0 },
			{ "SandMartin",              700,    30,   1.5,     0, false,  0.0,     15,   2.0,   11,    9,  0.85, 5.5 },
			{ "Cormorant",               300,    30,   0.7,     0, true,   1.2,     20,   1.0,   18,    0,  0.45, 2.5 },  // heavy, fast and straight, a weak climber
			{ "Sparrowhawk",             120,    40,   1.6,     0, false,  0.0,     18,   3.0,   13,    0,  0.95, 6.0 },
			{ "Peregrine",              1200,    90,   1.4,    25, false,  0.0,     30,   4.0,   22,   10,  1.00, 8.0 },
			{ "MuteSwan",                600,    30,   0.5,    17, true,   3.5,     22,   1.0,   18,    0,  0.20, 2.2 },  // the heaviest flier here: climbs at 1 m/s at most
			{ "WhiteTailedEagle",        300,    35,   0.6,     0, false,  0.0,     17,   0.9,   22,   15,  0.60, 3.0 },  // a 'flying barn door': flapping is costly, it lives on rising air
			{ "GoldenEagle",             300,    67,   0.8,     0, false,  0.0,     20,   1.0,   22,   16,  0.95, 5.0 },  // stoops at 67 m/s (Tucker 1998)
			{ "BlackBrowedAlbatross",     90,    30,   0.5,     0, true,   2.5,     18,   0.5,   35,   22,  0.30, 2.5 },  // dynamic soaring: gales are its weather, calm leaves it sitting on the sea
			{ "CommonBuzzard",           180,    30,   0.8,     0, false,  0.0,     15,   1.2,   15,   13,  0.85, 4.0 },
			{ "Raven",                   600,    35,   1.1,     0, false,  0.0,     19,   2.0,   22,    9,  0.90, 4.0 },  // rolls and tumbles in a gale
			{ "GreyHeron",               900,    25,   0.6,     0, false,  0.0,     15,   1.3,   15,    9,  0.30, 2.4 },  // slow deep beats, neck drawn in
			{ "Jay",                      60,    24,   1.1,     0, false,  0.0,     13,   2.0,    9,    0,  0.60, 0.0 },  // rounded wings, an unhurried bounding flight
			{ "Nutcracker",              120,    26,   1.0,     0, false,  0.0,     14,   2.0,   11,    0,  0.70, 0.0 },  // carries nuts kilometres to its stores
			{ "CarrionCrow",             240,    30,   1.0,     0, false,  0.0,     18,   2.0,   15,    0,  0.70, 0.0 },
		};
		const FBio Fallback = { "", 150, 25, 1.0, 0, false, 0, 0, 2.0, 12, 0, 0.75, 0 };
	}

	const FBio& Get(const char* Id)
	{
		for (const FBio& B : Table)
		{
			if (Id && std::strcmp(B.Id, Id) == 0)
			{
				return B;
			}
		}
		return Fallback;
	}
}
