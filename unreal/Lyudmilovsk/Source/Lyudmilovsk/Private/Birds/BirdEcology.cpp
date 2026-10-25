// The species' ecology and temper: where each one feeds, rests and sleeps on a Baltic coast in late
// October, how many there are, how they fly, how close they let a person come, who gives way to whom.
// Flight distances follow Fernández-Juricic et al. and Blumstein (2006) for a person walking straight at
// the bird; town birds are tamer than field birds. Sizes and flight styles match the species thread's
// table (Content/BirdsData/species.json).
#include "BirdSim.h"

#include <cstdlib>
#include <cstring>

namespace LyudBirds
{
	namespace
	{
		/** "Meadow 1, Beach .2" -> weights per habitat. */
		void Prefs(float (&Out)[NumHabitats], const char* Spec)
		{
			for (float& V : Out)
			{
				V = 0.f;
			}
			const char* P = Spec;
			while (*P)
			{
				while (*P == ' ' || *P == ',')
				{
					P++;
				}
				if (!*P)
				{
					break;
				}
				const char* NameEnd = P;
				while (*NameEnd && *NameEnd != ' ')
				{
					NameEnd++;
				}
				const std::string Name(P, NameEnd - P);
				char* End = nullptr;
				const float W = std::strtof(NameEnd, &End);
				for (int H = 0; H < NumHabitats; H++)
				{
					if (Name == HabitatName((EHabitat)H))
					{
						Out[H] = W;
					}
				}
				P = End && End != NameEnd ? End : NameEnd;
			}
		}

		struct FDef
		{
			const char* Id;
			float Length, Mass;
			int Population, FlockMin, FlockMax;
			float HomeRange;
			const char* Forage;
			const char* Perch;
			const char* Roost;
			bool bCommunal;
			EFlight Flight;
			float Cruise, Clearance;
			bool bHops;
			float Gait, FleeDistance, SightRange, Tameness, Curiosity;
			int Dominance;
			bool bKlepto, bMobs, bPredator;
			float PreyValue, Wake, Roost_, MiddayRest, CallRate, DawnSong;
			uint32_t Color;
			bool bMigrant;
		};

		// clang-format off
		const FDef Defs[] = {
			// Id             len   kg     pop min max  range  forage                                       perch                                         roost                     comm  flight              cruise clear hops gait  flee sight tame curio dom klep mobs pred prey  wake roost rest calls dawn  colour   migr
			{ "Rook",         0.46f, 0.45f, 60, 8, 24,  1500, "Meadow 1, Beach .2, Street .15",              "Broadleaf 1, Conifer .5, Wire .3, Roof .2",    "Broadleaf 1, Conifer .6", true,  EFlight::Flap,      11.f, 22.f, false, 0.9f, 30.f, 90.f, 0.15f, 0.2f, 70, false, true,  false, 0.f,  -25.f, 5.f,  0.3f, 3.f,  1.f,  0x18181c, false },
			{ "HoodedCrow",   0.47f, 0.55f, 24, 1, 3,   900,  "Beach 1, Meadow .8, Street .6, Sea .1",       "Conifer .8, Broadleaf .8, Roof .5, Tower .3, Wire .3", "Conifer 1",   true,  EFlight::Flap,      11.f, 16.f, false, 1.0f, 28.f, 100.f,0.15f, 0.4f, 85, true,  true,  false, 0.f,  -20.f, 0.f,  0.3f, 1.5f, 0.5f, 0x8a8a8a, false },
			{ "Jackdaw",      0.34f, 0.23f, 50, 6, 20,  900,  "Meadow 1, Street .5, Beach .2",               "Roof 1, Tower 1, Broadleaf .5",               "Tower 1, Roof .6, Broadleaf .5", true, EFlight::Flap,   11.f, 16.f, false, 1.0f, 15.f, 70.f, 0.3f, 0.4f, 55, true,  true,  false, 0.f,  -20.f, 10.f, 0.3f, 4.f,  1.f,  0x2a2a30, false },
			{ "Magpie",       0.45f, 0.22f, 14, 1, 3,   500,  "Meadow .8, Street .6, Garden .5",             "Garden 1, Broadleaf .8, Roof .4",             "Garden 1, Broadleaf 1",  false, EFlight::Flap,       8.f, 10.f, true,  0.8f, 18.f, 70.f, 0.25f, 0.6f, 65, true,  true,  false, 0.f,  -15.f, -5.f, 0.4f, 1.5f, 0.f,  0x101014, false },
			{ "FeralPigeon",  0.33f, 0.33f, 90, 8, 30,  450,  "Street 1, Beach .1, Meadow .1",               "Roof 1, Tower 1, Wire .2",                    "Tower 1, Roof .8",       false, EFlight::Flap,      14.f, 10.f, false, 0.7f,  3.f, 40.f, 0.8f, 0.5f, 40, false, false, false, 0.6f, 0.f,   -5.f, 0.5f, 1.f,  0.f,  0x6e7480, false },
			{ "WoodPigeon",   0.41f, 0.5f,  30, 3, 15,  900,  "Meadow 1, Garden .3, ForestFloor .3",         "Conifer 1, Broadleaf 1, Wire .3",             "Conifer 1",              false, EFlight::Flap,      15.f, 16.f, false, 0.7f, 25.f, 80.f, 0.2f, 0.1f, 45, false, false, false, 0.4f, -10.f, -10.f,0.5f, 0.5f, 0.5f, 0x7c808c, false },
			{ "HerringGull",  0.60f, 1.0f,  50, 2, 12,  2600, "Sea 1, Beach 1, Street .4, Meadow .2",        "Roof .8, Beach 1, Sea 1, Tower .3, Cliff .3", "Sea 1, Beach .6",        true,  EFlight::FlapGlide, 11.f, 14.f, false, 0.9f, 20.f, 120.f,0.3f, 0.6f, 80, true,  true,  false, 0.f,  -30.f, 20.f, 0.5f, 2.f,  0.5f, 0xd8dadf, false },
			{ "BlackHeadedGull",0.38f,0.28f,60, 5, 25,  2000, "Meadow 1, Beach 1, Sea .8, Street .5",        "Beach 1, Sea 1, Roof .5, Wire .3",            "Sea 1",                  true,  EFlight::FlapGlide, 10.f, 12.f, false, 0.9f, 15.f, 100.f,0.35f,0.5f, 50, true,  true,  false, 0.1f, -30.f, 15.f, 0.4f, 3.f,  0.5f, 0xe4e6ea, false },
			{ "CommonGull",   0.42f, 0.4f,  30, 3, 15,  2000, "Meadow 1, Beach .8, Sea .8",                  "Beach 1, Sea 1, Roof .4",                     "Sea 1",                  true,  EFlight::FlapGlide, 10.f, 12.f, false, 0.9f, 18.f, 100.f,0.3f, 0.4f, 60, true,  true,  false, 0.f,  -30.f, 15.f, 0.4f, 2.f,  0.5f, 0xdcdee4, false },
			{ "Cormorant",    0.85f, 2.2f,  12, 2, 8,   2600, "Sea 1",                                       "Beach .8, Cliff .3",                          "Beach 1, Sea .5",        true,  EFlight::Flap,      15.f, 10.f, false, 0.5f, 45.f, 120.f,0.05f,0.1f, 75, false, false, false, 0.f,  -10.f, 0.f,  0.6f, 0.2f, 0.f,  0x141414, false },
			{ "GreatTit",     0.14f, 0.018f,50, 2, 8,   280,  "Garden 1, Broadleaf 1, Conifer .6, ForestFloor .3, Reed .2", "Garden 1, Broadleaf 1, Conifer .6", "Broadleaf 1, Garden 1", false, EFlight::Bounding, 8.f, 6.f, true,  0.6f,  6.f, 40.f, 0.8f, 0.8f, 22, false, true,  false, 0.8f, -15.f, -20.f,0.2f, 6.f,  4.f,  0x6b8a3a, false },
			{ "BlueTit",      0.12f, 0.011f,40, 2, 6,   250,  "Garden 1, Broadleaf 1, Reed .4, Conifer .3",  "Garden 1, Broadleaf 1, Reed .3",              "Broadleaf 1, Garden 1",  false, EFlight::Bounding,  8.f,  6.f, true,  0.6f,  5.f, 35.f, 0.7f, 0.7f, 12, false, true,  false, 0.9f, -15.f, -20.f,0.2f, 6.f,  3.f,  0x4f7fb8, false },
			{ "Goldfinch",    0.13f, 0.016f,40, 6, 20,  700,  "Meadow 1, Garden .5, Reed .3",                "Broadleaf 1, Garden 1, Wire .6",              "Conifer 1, Broadleaf .5",false, EFlight::Bounding,  9.f,  6.f, true,  0.5f,  8.f, 40.f, 0.4f, 0.4f, 10, false, false, false, 0.8f, -5.f,  -20.f,0.3f, 8.f,  2.f,  0xb08a50, false },
			{ "Chaffinch",    0.15f, 0.024f,40, 3, 15,  400,  "ForestFloor 1, Meadow .5, Garden .5, Street .2", "Broadleaf 1, Conifer 1",                  "Conifer 1",              false, EFlight::Bounding,  9.f,  6.f, true,  0.6f,  8.f, 40.f, 0.5f, 0.4f, 18, false, false, false, 0.8f, -15.f, -20.f,0.3f, 3.f,  4.f,  0x9a6a52, false },
			{ "Bullfinch",    0.16f, 0.03f, 20, 2, 8,   400,  "Garden 1, Broadleaf 1, Meadow .3",            "Garden 1, Broadleaf 1, Conifer .5",           "Conifer 1, Garden .5",   false, EFlight::Bounding,  9.f,  6.f, true,  0.5f,  8.f, 40.f, 0.5f, 0.3f, 20, false, false, false, 0.8f, -5.f,  -20.f,0.4f, 1.5f, 0.f,  0xd8484a, false },
			{ "Waxwing",      0.19f, 0.06f, 30, 10, 30, 1200, "Garden 1, Broadleaf .8",                      "Broadleaf 1, Garden 1, Wire .5",              "Conifer 1",              false, EFlight::Flap,      12.f,  8.f, true,  0.5f,  6.f, 40.f, 0.6f, 0.3f, 24, false, false, false, 0.6f, 0.f,   -20.f,0.4f, 6.f,  0.f,  0xb89082, false },
			{ "WhiteWagtail", 0.18f, 0.022f,16, 1, 3,   500,  "Beach 1, Street .8, Meadow .6, Reed .4",      "Roof 1, Wire .8",                             "Reed 1, Broadleaf .5",   false, EFlight::Bounding,  9.f,  4.f, false, 1.2f,  8.f, 40.f, 0.4f, 0.4f, 16, false, true,  false, 0.7f, 0.f,   -10.f,0.2f, 3.f,  0.f,  0x8e939a, true  },
			{ "HouseSparrow", 0.15f, 0.03f, 90, 8, 30,  160,  "Street 1, Garden .8, Meadow .3",              "Garden 1, Roof .8, Wire .3",                  "Roof 1, Garden .6",      false, EFlight::Bounding,  9.f,  5.f, true,  0.6f,  5.f, 35.f, 0.6f, 0.5f, 15, false, false, false, 0.9f, -10.f, -20.f,0.4f, 10.f, 3.f,  0x8a6e50, false },
			{ "TreeSparrow",  0.14f, 0.022f,40, 6, 20,  350,  "Meadow 1, Garden .6, Reed .3",                "Broadleaf 1, Garden 1, Wire .5",              "Garden 1, Broadleaf .5", false, EFlight::Bounding,  9.f,  5.f, true,  0.6f,  7.f, 40.f, 0.4f, 0.4f, 14, false, false, false, 0.9f, -10.f, -20.f,0.4f, 8.f,  2.f,  0x8c6a4c, false },
			{ "Starling",     0.21f, 0.08f, 40, 10, 30, 900,  "Meadow 1, Street .4, Beach .4",               "Wire 1, Roof .8, Broadleaf .6",               "Reed 1, Broadleaf .5",   true,  EFlight::Flap,      12.f, 10.f, false, 1.1f, 12.f, 60.f, 0.35f,0.4f, 30, false, true,  false, 0.6f, -15.f, -5.f, 0.3f, 6.f,  4.f,  0x1c1e24, false },
			{ "SongThrush",   0.22f, 0.07f, 20, 1, 1,   300,  "ForestFloor 1, Garden .6",                    "Broadleaf 1, Conifer .8",                     "Conifer 1",              false, EFlight::Flap,      10.f,  6.f, true,  1.0f, 12.f, 50.f, 0.3f, 0.3f, 28, false, false, false, 0.6f, -40.f, 0.f,  0.4f, 0.5f, 3.f,  0x8a7058, false },
			{ "Fieldfare",    0.26f, 0.1f,  60, 10, 40, 1000, "Meadow 1, Garden .6, Broadleaf .5",           "Broadleaf 1, Conifer .5",                     "Conifer 1",              false, EFlight::Flap,      11.f, 12.f, true,  1.0f, 22.f, 70.f, 0.15f,0.2f, 38, false, true,  false, 0.5f, -10.f, -10.f,0.3f, 5.f,  1.f,  0x7a5a40, false },
			{ "Blackbird",    0.25f, 0.1f,  30, 1, 2,   250,  "Garden 1, ForestFloor 1, Meadow .3",          "Garden 1, Broadleaf .8, Roof .4",             "Conifer 1, Garden 1",    false, EFlight::Flap,       9.f,  6.f, true,  1.2f, 10.f, 50.f, 0.4f, 0.4f, 35, false, true,  false, 0.5f, -40.f, 15.f, 0.3f, 0.7f, 3.f,  0x101010, false },
			{ "Robin",        0.14f, 0.018f,24, 1, 1,   120,  "ForestFloor 1, Garden 1",                     "Broadleaf 1, Garden 1, Conifer .6",           "Broadleaf 1, Conifer 1", false, EFlight::Flap,       8.f,  4.f, true,  0.8f,  4.f, 30.f, 1.0f, 1.0f, 20, false, true,  false, 0.8f, -45.f, 20.f, 0.3f, 1.5f, 4.f,  0xc0703a, false },
			{ "CommonSwift",  0.17f, 0.04f, 40, 5, 20,  1600, "Street 1, Meadow 1, Sea .6, Cliff .5",        "Tower 1, Cliff .6",                           "Tower 1, Cliff 1, Roof .4", false, EFlight::Aerial, 18.f, 30.f, false, 0.f,  10.f, 80.f, 0.f,  0.f,  30, false, false, false, 0.3f, 10.f,  -15.f,0.f,  5.f,  0.f,  0x2a2622, true  },
			{ "BarnSwallow",  0.18f, 0.02f, 40, 4, 20,  1200, "Meadow 1, Reed 1, Sea .6, Beach .5",          "Wire 1, Roof .6",                             "Reed 1",                 true,  EFlight::Aerial,    10.f,  4.f, false, 0.f,   8.f, 50.f, 0.2f, 0.3f, 25, false, true,  false, 0.4f, 0.f,   -10.f,0.f,  6.f,  2.f,  0x1a2440, true  },
			{ "SandMartin",   0.12f, 0.014f,40, 10, 30, 1200, "Sea 1, Beach 1, Reed .8, Meadow .4",          "Wire 1, Cliff 1",                             "Cliff 1, Reed .6",       true,  EFlight::Aerial,    10.f,  4.f, false, 0.f,   8.f, 40.f, 0.1f, 0.2f, 15, false, true,  false, 0.5f, 0.f,   -10.f,0.f,  6.f,  0.f,  0x8a7a66, true  },
			{ "Sparrowhawk",  0.33f, 0.2f,   2, 1, 1,   1400, "Garden 1, Street .6, Broadleaf .5, Meadow .3","Conifer 1, Broadleaf 1",                      "Conifer 1",              false, EFlight::FlapGlide, 12.f, 14.f, false, 0.6f, 35.f, 150.f,0.f,  0.1f, 90, false, false, true,  0.f,  0.f,   -10.f,0.4f, 0.1f, 0.f,  0x6a6660, false },
			{ "Peregrine",    0.42f, 0.8f,   1, 1, 1,   2600, "Street 1, Beach .6, Sea .4",                  "Tower 1, Cliff 1",                            "Tower 1, Cliff 1",       false, EFlight::Soar,      16.f, 40.f, false, 0.5f, 50.f, 300.f,0.f,  0.1f, 100,false, false, true,  0.f,  0.f,   -10.f,0.6f, 0.1f, 0.f,  0x50545c, false },
		};
		// clang-format on

		std::vector<FSpecies> MakeTable()
		{
			std::vector<FSpecies> Out;
			for (const FDef& D : Defs)
			{
				FSpecies S;
				S.Id = D.Id;
				S.Length = D.Length;
				S.Mass = D.Mass;
				S.Population = D.Population;
				S.FlockMin = D.FlockMin;
				S.FlockMax = D.FlockMax;
				S.HomeRange = D.HomeRange;
				Prefs(S.Forage, D.Forage);
				Prefs(S.Perch, D.Perch);
				Prefs(S.Roost, D.Roost);
				S.bCommunalRoost = D.bCommunal;
				S.Flight = D.Flight;
				S.Cruise = D.Cruise;
				S.FlightClearance = D.Clearance;
				S.HawkAlt = D.Id == std::string("CommonSwift") ? 35.f : 4.f;
				S.bHops = D.bHops;
				S.Gait = D.Gait;
				S.FleeDistance = D.FleeDistance;
				S.SightRange = D.SightRange;
				S.Fov = S.bPredator ? 220.f : 300.f;
				S.Tameness = D.Tameness;
				S.Curiosity = D.Curiosity;
				S.Dominance = D.Dominance;
				S.bKlepto = D.bKlepto;
				S.bMobs = D.bMobs;
				S.bPredator = D.bPredator;
				S.Fov = D.bPredator ? 220.f : 300.f;
				S.PreyValue = D.PreyValue;
				S.WakeMinutes = D.Wake;
				S.RoostMinutes = D.Roost_;
				S.MiddayRest = D.MiddayRest;
				S.CallRate = D.CallRate;
				S.DawnSong = D.DawnSong;
				S.Color = D.Color;
				S.bMigrant = D.bMigrant;
				Out.push_back(S);
			}
			return Out;
		}
	}

	std::vector<FSpecies>& MutableSpeciesTable()
	{
		static std::vector<FSpecies> Table = MakeTable();
		return Table;
	}

	const std::vector<FSpecies>& SpeciesTable()
	{
		return MutableSpeciesTable();
	}

	int FindSpecies(const std::string& Id)
	{
		const std::vector<FSpecies>& T = SpeciesTable();
		for (int I = 0; I < (int)T.size(); I++)
		{
			if (T[I].Id == Id)
			{
				return I;
			}
		}
		return -1;
	}
}
