#include "BirdBio.h"

#include <cstring>

namespace BirdBio
{
	namespace
	{
		const FBio Table[] = {
			// id                  stamina  dive  agility cruise swims  run-up  sprint climb wind  glide
			{ "Rook",                    240,    30,   1.0,     0, false,  0.0,     18,   2.0,   14,    0 },
			{ "HoodedCrow",              240,    30,   1.0,     0, false,  0.0,     18,   2.0,   15,    0 },
			{ "Jackdaw",                 200,    30,   1.2,     0, false,  0.0,     18,   2.2,   14,    0 },  // plays in a gale on the cliff
			{ "Magpie",                   50,    22,   1.1,     0, false,  0.0,     13,   1.6,   10,    0 },  // short broad wings, a poor flier in wind
			{ "FeralPigeon",             300,    40,   1.1,     0, false,  0.0,     24,   3.0,   13,    0 },  // racing pigeons hold ~20 m/s for hours
			{ "WoodPigeon",              300,    40,   1.0,     0, false,  0.0,     22,   2.8,   14,    0 },
			{ "HerringGull",             600,    35,   0.9,     0, true,   0.0,     18,   1.5,   24,   15 },
			{ "BlackHeadedGull",         500,    30,   1.2,     0, true,   0.0,     15,   1.8,   18,   13 },
			{ "CommonGull",              500,    32,   1.0,     0, true,   0.0,     16,   1.6,   20,   14 },
			{ "GreatTit",                 35,    18,   1.3,     0, false,  0.0,     12,   2.6,    8,    0 },
			{ "BlueTit",                  30,    16,   1.4,     0, false,  0.0,     11,   2.5,    7,    0 },
			{ "Goldfinch",                60,    18,   1.3,     0, false,  0.0,     14,   2.5,    8,    0 },
			{ "Chaffinch",                60,    20,   1.2,     0, false,  0.0,     15,   2.6,    9,    0 },
			{ "Bullfinch",                40,    16,   1.0,     0, false,  0.0,     12,   2.2,    7,    0 },
			{ "HouseSparrow",             40,    18,   1.2,     0, false,  0.0,     13,   2.6,    8,    0 },
			{ "TreeSparrow",              45,    18,   1.2,     0, false,  0.0,     13,   2.6,    8,    0 },
			{ "Starling",                180,    30,   1.3,     0, false,  0.0,     20,   2.8,   13,    0 },
			{ "SongThrush",              120,    25,   1.1,     0, false,  0.0,     16,   2.4,   10,    0 },
			{ "Fieldfare",               150,    26,   1.0,     0, false,  0.0,     17,   2.4,   12,    0 },
			{ "Blackbird",                60,    22,   1.1,     0, false,  0.0,     15,   2.4,   10,    0 },
			{ "Robin",                    45,    16,   1.2,     0, false,  0.0,     12,   2.2,    7,    0 },
			{ "WhiteWagtail",             90,    18,   1.3,     0, false,  0.0,     14,   2.4,    9,    0 },
			{ "Waxwing",                 150,    25,   1.1,     0, false,  0.0,     17,   2.4,   11,    0 },
			{ "CommonSwift",            1200,    45,   1.5,    20, false,  0.0,     31,   2.5,   18,   12 },  // screaming parties sprint at 31 m/s (Henningsson 2010)
			{ "BarnSwallow",             900,    30,   1.6,     0, false,  0.0,     18,   2.2,   12,   10 },
			{ "SandMartin",              700,    25,   1.5,     0, false,  0.0,     15,   2.0,   11,    9 },
			{ "Cormorant",               300,    30,   0.7,     0, true,   1.2,     20,   1.0,   18,    0 },  // heavy, fast and straight, a weak climber
			{ "Sparrowhawk",             120,    40,   1.6,     0, false,  0.0,     18,   3.0,   13,    0 },
			{ "Peregrine",              1200,    90,   1.4,    25, false,  0.0,     30,   4.0,   22,   10 },
			{ "MuteSwan",                600,    30,   0.5,    17, true,   3.5,     22,   1.0,   18,    0 },  // the heaviest flier here: climbs at 1 m/s at most
			{ "WhiteTailedEagle",        300,    35,   0.6,     0, false,  0.0,     17,   0.9,   22,   15 },  // a 'flying barn door': flapping is costly, it lives on rising air
			{ "GoldenEagle",             300,    67,   0.8,     0, false,  0.0,     20,   1.0,   22,   16 },  // stoops at 67 m/s (Tucker 1998)
			{ "BlackBrowedAlbatross",     90,    30,   0.5,     0, true,   2.5,     18,   0.5,   35,   22 },  // dynamic soaring: gales are its weather, calm leaves it sitting on the sea
			{ "CommonBuzzard",           180,    30,   0.8,     0, false,  0.0,     15,   1.2,   15,   13 },
			{ "Raven",                   600,    35,   1.1,     0, false,  0.0,     19,   2.0,   22,    9 },  // rolls and tumbles in a gale
			{ "GreyHeron",               900,    25,   0.6,     0, false,  0.0,     15,   1.3,   15,    9 },  // slow deep beats, neck drawn in
		};
		const FBio Fallback = { "", 150, 25, 1.0, 0, false, 0, 0, 2.0, 12, 0 };
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
