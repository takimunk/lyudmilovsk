#include "BirdBio.h"

#include <cstring>

namespace BirdBio
{
	namespace
	{
		const FBio Table[] = {
			// id               stamina  dive  agility cruise swims  run-up
			{ "Rook",              240,   30,   1.0,     0,  false, 0 },
			{ "HoodedCrow",        240,   30,   1.0,     0,  false, 0 },
			{ "Jackdaw",           200,   30,   1.2,     0,  false, 0 },
			{ "Magpie",             50,   22,   1.1,     0,  false, 0 },
			{ "FeralPigeon",       300,   40,   1.1,     0,  false, 0 },
			{ "WoodPigeon",        300,   40,   1.0,     0,  false, 0 },
			{ "HerringGull",       600,   35,   0.9,     0,  true,  0 },
			{ "BlackHeadedGull",   500,   30,   1.2,     0,  true,  0 },
			{ "CommonGull",        500,   32,   1.0,     0,  true,  0 },
			{ "GreatTit",           35,   18,   1.3,     0,  false, 0 },
			{ "BlueTit",            30,   16,   1.4,     0,  false, 0 },
			{ "Goldfinch",          60,   18,   1.3,     0,  false, 0 },
			{ "Chaffinch",          60,   20,   1.2,     0,  false, 0 },
			{ "Bullfinch",          40,   16,   1.0,     0,  false, 0 },
			{ "HouseSparrow",       40,   18,   1.2,     0,  false, 0 },
			{ "TreeSparrow",        45,   18,   1.2,     0,  false, 0 },
			{ "Starling",          180,   30,   1.3,     0,  false, 0 },
			{ "SongThrush",        120,   25,   1.1,     0,  false, 0 },
			{ "Fieldfare",         150,   26,   1.0,     0,  false, 0 },
			{ "Blackbird",          60,   22,   1.1,     0,  false, 0 },
			{ "Robin",              45,   16,   1.2,     0,  false, 0 },
			{ "WhiteWagtail",       90,   18,   1.3,     0,  false, 0 },
			{ "Waxwing",           150,   25,   1.1,     0,  false, 0 },
			{ "CommonSwift",      1200,   45,   1.5,    20,  false, 0 },
			{ "BarnSwallow",       900,   30,   1.6,     0,  false, 0 },
			{ "SandMartin",        700,   25,   1.5,     0,  false, 0 },
			{ "Cormorant",         300,   30,   0.7,     0,  true,  1.2 },
			{ "Sparrowhawk",       120,   40,   1.6,     0,  false, 0 },
			{ "Peregrine",        1200,   90,   1.4,    25,  false, 0 },
			{ "MuteSwan",          600,   30,   0.5,    17,  true,  3.5 },
		};
		const FBio Fallback = { "", 150, 25, 1.0, 0, false, 0 };
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
