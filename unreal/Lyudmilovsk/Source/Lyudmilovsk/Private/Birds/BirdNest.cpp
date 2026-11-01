// The birds' breeding year: nests, eggs, chicks, fledglings (BirdNest.h, docs/bird_nests.md).
#include "BirdNest.h"

#include "BirdCorvid.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace LyudBirds
{
	namespace
	{
		constexpr float Pi = 3.14159265f;

		float Clamp(float V, float A, float B) { return V < A ? A : (V > B ? B : V); }

		uint32_t Hash(uint32_t X)
		{
			X ^= X >> 16;
			X *= 0x7feb352du;
			X ^= X >> 15;
			X *= 0x846ca68bu;
			X ^= X >> 16;
			return X;
		}
		float Unit(uint32_t& S)
		{
			S = Hash(S + 0x9e3779b9u);
			return (float)(S & 0xffffff) / 16777216.f;
		}

		/** "Meadow 1, Beach .2" -> weights per habitat (as BirdEcology's). */
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

		bool IsTree(EHabitat H) { return H == EHabitat::Broadleaf || H == EHabitat::Conifer || H == EHabitat::Garden; }
		bool IsHidden(ENest K) { return K == ENest::Cavity || K == ENest::Eave || K == ENest::Burrow; }

		using N = ENest;
		using M = EMaterial;
		constexpr EWho Fe = EWho::Female, Bo = EWho::Both;

		struct FDef
		{
			const char* Id;
			ENest Nest;
			EMaterial Mat;
			const char* Where;
			float HMin, HMax, Radius;
			bool bColonial;
			float Spacing, ColonyR;
			int ColonySize, PerSite;
			int LayFrom, LayUntil, LayLast, Build, CMin, CMax;
			float Interval;
			int Incub, Nestling, Depend, Broods;
			EWho Builds, Sits;
			bool bPrecocial, bReuse;
			float Survival, FeedMin, MatRange, Defend, Sing;
		};

		// Days of the year: 1 Mar = 60, 1 Apr = 91, 1 May = 121, 1 Jun = 152, 1 Jul = 182, 1 Aug = 213. Laying dates for the
		// south-east Baltic (Kaliningrad/Lithuania atlases, BWP), clutches, incubation and nestling periods from BWP and Cramp,
		// nest survival per day from Mayfield-style figures (open cups of thrushes ~0.94-0.96, holes ~0.97-0.98, big birds ~0.99).
		// clang-format off
		const FDef Defs[] = {
			// id                nest          material      where                                            hmin hmax  r      col   space colR  csz per  layF layU last  bld cmin cmax ivl  inc  nst  dep  br  build sits  prec   reuse  surv   feed  mat   def  sing
			{ "Rook",            N::Platform,  M::Sticks,    "Broadleaf 1, Conifer .4",                       12, 25,  0.30f, true,  2.f,  45.f, 30, 6,   80, 100, 120,  14, 3, 5,  1.f, 17,  32,  30, 1,  Bo,   Fe,   false, true,  0.985f, 15.f, 150.f, 0.3f, 0.f },
			{ "HoodedCrow",      N::Platform,  M::Sticks,    "Conifer 1, Broadleaf .8, Tower .2",              6, 18,  0.30f, false, 300.f, 0.f,  1, 1,   92, 110, 140,  12, 4, 5,  1.f, 19,  30,  30, 1,  Bo,   Fe,   false, true,  0.985f, 15.f, 150.f, 0.6f, 0.f },
			{ "CarrionCrow",     N::Platform,  M::Sticks,    "Conifer 1, Broadleaf .8",                        6, 18,  0.30f, false, 300.f, 0.f,  1, 1,   92, 110, 140,  12, 4, 5,  1.f, 19,  30,  30, 1,  Bo,   Fe,   false, true,  0.985f, 15.f, 150.f, 0.6f, 0.f },
			{ "Jackdaw",         N::Cavity,    M::Twigs,     "Tower 1, Roof .8, Broadleaf .6",                 3, 12,  0.20f, true,  3.f,  80.f, 15, 4,  105, 120, 140,  10, 4, 5,  1.f, 18,  32,  28, 1,  Bo,   Fe,   false, true,  0.985f, 10.f, 100.f, 0.4f, 0.f },
			{ "Magpie",          N::Dome,      M::Sticks,    "Broadleaf 1, Garden 1, Conifer .4",              3, 10,  0.35f, false, 150.f, 0.f,  1, 1,   95, 115, 140,  21, 5, 7,  1.f, 18,  27,  30, 1,  Bo,   Fe,   false, true,  0.98f,  10.f, 100.f, 0.7f, 0.f },
			{ "Raven",           N::Platform,  M::Sticks,    "Cliff 1, Conifer .8, Tower .3",                 10, 25,  0.45f, false, 1500.f,0.f,  1, 1,   55,  75, 100,  14, 4, 6, 1.5f, 21,  40,  40, 1,  Bo,   Fe,   false, true,  0.99f,  20.f, 200.f, 0.5f, 0.f },
			{ "Jay",             N::Cup,       M::Twigs,     "Broadleaf 1, Conifer .8",                        2,  6,  0.14f, false, 200.f, 0.f,  1, 1,  120, 140, 165,   8, 4, 6,  1.f, 16,  21,  21, 1,  Bo,   Fe,   false, false, 0.97f,   8.f,  60.f, 0.3f, 0.5f },
			{ "Nutcracker",      N::Cup,       M::Twigs,     "Conifer 1",                                      4, 10,  0.15f, false, 300.f, 0.f,  1, 1,   75,  95, 110,  10, 3, 4,  1.f, 18,  23,  30, 1,  Bo,   Bo,   false, false, 0.98f,  10.f,  80.f, 0.2f, 0.3f },
			{ "FeralPigeon",     N::Platform,  M::Twigs,     "Roof 1, Tower 1, Cliff .5",                      0,  0,  0.12f, true,  1.5f, 40.f, 20, 8,   32, 300, 320,   4, 2, 2,  2.f, 18,  30,  10, 4,  Bo,   Bo,   false, true,  0.975f, 60.f,  60.f, 0.0f, 0.5f },
			{ "WoodPigeon",      N::Platform,  M::Twigs,     "Broadleaf 1, Conifer 1, Garden .8",              2, 10,  0.13f, false, 60.f,  0.f,  1, 1,  105, 135, 245,   4, 2, 2,  2.f, 17,  30,   7, 3,  Bo,   Bo,   false, false, 0.96f,  60.f,  50.f, 0.0f, 0.6f },
			{ "HerringGull",     N::Scrape,    M::Grass,     "Roof 1, Beach .6, Meadow .3",                    0,  0,  0.25f, true,  5.f, 120.f, 30, 8,  120, 140, 155,   6, 2, 3,  2.f, 30,   4,  50, 1,  Bo,   Bo,   true,  false, 0.98f,  40.f,  30.f, 1.0f, 0.f },
			{ "BlackHeadedGull", N::Mound,     M::Reed,      "Reed 1, Beach .3",                               0,  0,  0.20f, true,  2.f,  60.f, 40, 10, 110, 130, 150,   5, 2, 3, 1.5f, 24,   3,  35, 1,  Bo,   Bo,   true,  false, 0.97f,  30.f,  30.f, 1.0f, 0.f },
			{ "CommonGull",      N::Scrape,    M::Grass,     "Roof 1, Beach .8, Meadow .4",                    0,  0,  0.20f, true,  6.f, 100.f, 25, 8,  125, 145, 160,   5, 2, 3,  2.f, 25,   3,  40, 1,  Bo,   Bo,   true,  false, 0.97f,  40.f,  30.f, 1.0f, 0.f },
			{ "GreatTit",        N::Cavity,    M::Moss,      "Broadleaf 1, Garden 1, Conifer .5",            1.5f, 5,  0.07f, false, 60.f,  0.f,  1, 1,  110, 125, 175,   7, 7, 10, 1.f, 14,  19,  14, 2,  Fe,   Fe,   false, true,  0.97f,  1.5f, 40.f, 0.4f, 2.f },
			{ "BlueTit",         N::Cavity,    M::Moss,      "Broadleaf 1, Garden 1",                        1.5f, 5,  0.06f, false, 50.f,  0.f,  1, 1,  108, 122, 160,   7, 8, 11, 1.f, 14,  19,  14, 1,  Fe,   Fe,   false, true,  0.97f,  1.5f, 40.f, 0.4f, 2.f },
			{ "Goldfinch",       N::Cup,       M::Grass,     "Garden 1, Broadleaf .8",                         2,  8,  0.05f, false, 30.f,  0.f,  1, 1,  130, 160, 200,   6, 4, 6,  1.f, 12,  14,  14, 2,  Fe,   Fe,   false, false, 0.96f,   6.f, 30.f, 0.3f, 1.5f },
			{ "Chaffinch",       N::Cup,       M::Moss,      "Broadleaf 1, Conifer 1, Garden .8",              2,  8, 0.055f, false, 50.f,  0.f,  1, 1,  120, 140, 170,   6, 4, 5,  1.f, 12,  14,  14, 1,  Fe,   Fe,   false, false, 0.955f,  3.f, 30.f, 0.5f, 3.f },
			{ "Bullfinch",       N::Cup,       M::Twigs,     "Conifer 1, Garden .6, Broadleaf .5",             1,  4,  0.06f, false, 80.f,  0.f,  1, 1,  130, 150, 200,   6, 4, 5,  1.f, 13,  15,  15, 2,  Fe,   Fe,   false, false, 0.96f,   8.f, 30.f, 0.3f, 0.5f },
			{ "HouseSparrow",    N::Eave,      M::Grass,     "Roof 1, Garden .4",                              0,  0,  0.12f, true,  3.f,  40.f, 10, 3,  110, 125, 220,   5, 4, 5,  1.f, 12,  15,  10, 3,  Bo,   Bo,   false, true,  0.975f,  2.f, 30.f, 0.4f, 2.f },
			{ "TreeSparrow",     N::Cavity,    M::Grass,     "Garden 1, Broadleaf .8, Roof .3",              1.5f, 5,  0.07f, true,  4.f,  60.f, 10, 2,  115, 130, 200,   5, 5, 6,  1.f, 12,  15,  10, 2,  Bo,   Bo,   false, true,  0.975f,  2.f, 30.f, 0.3f, 1.5f },
			{ "Starling",        N::Cavity,    M::Grass,     "Garden 1, Roof .8, Broadleaf .8",                3,  6,  0.09f, true,  8.f, 120.f, 12, 2,  108, 122, 160,   6, 4, 6,  1.f, 12,  21,  14, 1,  Bo,   Bo,   false, true,  0.98f,  2.5f, 60.f, 0.4f, 3.f },
			{ "SongThrush",      N::Cup,       M::Mud,       "Conifer 1, Broadleaf .8, Garden .6",             1,  4,  0.08f, false, 70.f,  0.f,  1, 1,  100, 120, 190,   7, 4, 5,  1.f, 13,  13,  18, 2,  Fe,   Fe,   false, false, 0.95f,   5.f, 30.f, 0.5f, 3.f },
			{ "Fieldfare",       N::Cup,       M::Grass,     "Broadleaf 1, Conifer .8, Garden .5",             2,  8,  0.09f, true,  15.f,120.f,  8, 2,  118, 140, 175,   6, 5, 6,  1.f, 12,  13,  16, 1,  Fe,   Fe,   false, false, 0.95f,   5.f, 40.f, 1.0f, 1.f },
			{ "Blackbird",       N::Cup,       M::Grass,     "Garden 1, Broadleaf .8, Conifer .6",             1,  4,  0.09f, false, 50.f,  0.f,  1, 1,   95, 115, 200,   7, 3, 5,  1.f, 13,  13,  18, 3,  Fe,   Fe,   false, false, 0.94f,   5.f, 30.f, 0.6f, 3.f },
			{ "Robin",           N::Cup,       M::Moss,      "ForestFloor 1, Garden .8, Broadleaf .6",         0, 1.5f,0.06f, false, 60.f,  0.f,  1, 1,  110, 130, 190,   5, 4, 6,  1.f, 13,  14,  16, 2,  Fe,   Fe,   false, false, 0.95f,   4.f, 25.f, 0.5f, 3.f },
			{ "WhiteWagtail",    N::Eave,      M::Grass,     "Roof 1, Cliff .6, Street .4",                    0,  0,  0.07f, false, 80.f,  0.f,  1, 1,  120, 140, 190,   6, 5, 6,  1.f, 12,  14,  10, 2,  Bo,   Fe,   false, false, 0.96f,   3.f, 30.f, 0.6f, 1.5f },
			{ "CommonSwift",     N::Eave,      M::Feathers,  "Tower 1, Roof 1",                                0,  0,  0.06f, true,  2.f,  40.f, 15, 6,  145, 158, 170,   6, 2, 3,  2.f, 20,  42,   0, 1,  Bo,   Bo,   false, true,  0.985f, 30.f,  0.f, 0.0f, 1.f },
			{ "BarnSwallow",     N::MudCup,    M::Mud,       "Roof 1, Tower .3",                               0,  0,  0.07f, false, 6.f,   0.f,  1, 2,  135, 155, 205,   9, 4, 5,  1.f, 15,  20,   7, 2,  Bo,   Fe,   false, true,  0.975f,  2.f, 60.f, 0.6f, 2.f },
			{ "SandMartin",      N::Burrow,    M::None,      "Cliff 1",                                        0,  0,  0.04f, true,  0.4f, 30.f, 40, 30, 135, 150, 190,   9, 4, 5,  1.f, 14,  20,   7, 2,  Bo,   Bo,   false, true,  0.98f,   2.f,  0.f, 0.2f, 0.f },
			{ "Cormorant",       N::Platform,  M::Sticks,    "Conifer 1, Broadleaf 1, Cliff .5",               8, 20,  0.35f, true,  2.f,  60.f, 25, 6,   95, 115, 140,  10, 3, 4,  2.f, 29,  50,  30, 1,  Bo,   Bo,   false, true,  0.985f, 60.f, 150.f, 0.2f, 0.f },
			{ "MuteSwan",        N::Mound,     M::Reed,      "Reed 1",                                         0,  0,  0.90f, false, 600.f, 0.f,  1, 1,  100, 115, 130,  14, 5, 7,  2.f, 36,   1, 140, 1,  Bo,   Fe,   true,  true,  0.985f,  0.f, 15.f, 1.0f, 0.f },
			{ "Sparrowhawk",     N::Platform,  M::Twigs,     "Conifer 1, Broadleaf .4",                        6, 14,  0.30f, false, 800.f, 0.f,  1, 1,  120, 140, 155,  14, 4, 5,  2.f, 33,  28,  25, 1,  Fe,   Fe,   false, false, 0.985f, 60.f, 100.f, 0.5f, 0.f },
			{ "Peregrine",       N::Scrape,    M::None,      "Tower 1, Cliff 1",                               0,  0,  0.30f, false, 2000.f,0.f,  1, 1,   85, 105, 120,   0, 3, 4,  2.f, 31,  39,  40, 1,  Bo,   Bo,   false, true,  0.99f,  90.f,  0.f, 1.0f, 0.f },
			{ "WhiteTailedEagle",N::Platform,  M::Sticks,    "Conifer 1, Broadleaf .6",                       15, 25,  0.80f, false, 3000.f,0.f,  1, 1,   65,  85,  95,  30, 1, 2,  3.f, 38,  75,  40, 1,  Bo,   Bo,   false, true,  0.99f, 180.f, 300.f, 0.2f, 0.f },
			{ "GoldenEagle",     N::Platform,  M::Sticks,    "Cliff 1, Conifer .8",                           10, 20,  0.70f, false, 4000.f,0.f,  1, 1,   70,  90, 100,  30, 2, 2,  3.f, 43,  75,  60, 1,  Bo,   Fe,   false, true,  0.99f, 180.f, 300.f, 0.2f, 0.f },
			{ "CommonBuzzard",   N::Platform,  M::Sticks,    "Broadleaf 1, Conifer 1",                        10, 18,  0.40f, false, 1000.f,0.f,  1, 1,  100, 120, 135,  20, 2, 3,  3.f, 34,  50,  40, 1,  Bo,   Fe,   false, true,  0.985f, 90.f, 150.f, 0.4f, 0.f },
			{ "GreyHeron",       N::Platform,  M::Sticks,    "Conifer 1, Broadleaf 1",                        15, 25,  0.50f, true,  4.f,  80.f, 15, 4,   70, 100, 130,  10, 3, 5,  2.f, 26,  50,  20, 1,  Bo,   Bo,   false, true,  0.985f,120.f, 150.f, 0.2f, 0.f },
			// not here in the breeding season: Waxwing (the taiga), BlackBrowedAlbatross (the South Atlantic)
		};
		// clang-format on

		std::vector<FBreedKind> MakeTable()
		{
			std::vector<FBreedKind> Out;
			for (const FDef& D : Defs)
			{
				FBreedKind K;
				K.Id = D.Id;
				K.Nest = D.Nest;
				K.Material = D.Mat;
				K.Where = D.Where;
				K.HeightMin = D.HMin;
				K.HeightMax = D.HMax;
				K.NestRadius = D.Radius;
				K.bColonial = D.bColonial;
				K.Spacing = D.Spacing;
				K.ColonyRadius = D.ColonyR;
				K.ColonySize = D.ColonySize;
				K.PerSite = D.PerSite;
				K.LayFrom = D.LayFrom;
				K.LayUntil = D.LayUntil;
				K.LayLast = D.LayLast;
				K.BuildDays = D.Build;
				K.ClutchMin = D.CMin;
				K.ClutchMax = D.CMax;
				K.LayInterval = D.Interval;
				K.IncubDays = D.Incub;
				K.NestlingDays = D.Nestling;
				K.DependDays = D.Depend;
				K.Broods = D.Broods;
				K.Builds = D.Builds;
				K.Sits = D.Sits;
				K.bPrecocial = D.bPrecocial;
				K.bReuse = D.bReuse;
				K.Survival = D.Survival;
				K.FeedMinutes = D.FeedMin;
				K.MaterialRange = D.MatRange;
				K.Defend = D.Defend;
				K.SingRate = D.Sing;
				Out.push_back(K);
			}
			return Out;
		}

		/** Taste tables per row of the breeding table (parsed once). */
		struct FWhere { float W[NumHabitats]; };
		const std::vector<FWhere>& WhereTable()
		{
			static std::vector<FWhere> T = []
			{
				std::vector<FWhere> Out;
				for (const FBreedKind& K : BreedTable())
				{
					FWhere W;
					Prefs(W.W, K.Where);
					Out.push_back(W);
				}
				return Out;
			}();
			return T;
		}

		/** Habitats where a material is picked up. */
		bool MaterialIn(EMaterial Mat, EHabitat H)
		{
			switch (Mat)
			{
			case EMaterial::Sticks: return H == EHabitat::Broadleaf || H == EHabitat::Conifer || H == EHabitat::ForestFloor || H == EHabitat::Garden;
			case EMaterial::Twigs: return H == EHabitat::Broadleaf || H == EHabitat::Conifer || H == EHabitat::Garden || H == EHabitat::ForestFloor;
			case EMaterial::Grass: return H == EHabitat::Meadow || H == EHabitat::Garden || H == EHabitat::Street || H == EHabitat::Beach;
			case EMaterial::Moss: return H == EHabitat::ForestFloor || H == EHabitat::Garden || H == EHabitat::Meadow;
			case EMaterial::Mud: return H == EHabitat::Beach || H == EHabitat::Reed || H == EHabitat::Street || H == EHabitat::Meadow;
			case EMaterial::Reed: return H == EHabitat::Reed || H == EHabitat::Beach;
			case EMaterial::Feathers: return H == EHabitat::Meadow || H == EHabitat::Street || H == EHabitat::Beach;
			default: return false;
			}
		}

		bool Aerial(const FSpecies& S) { return S.Flight == EFlight::Aerial; }
	}

	const std::vector<FBreedKind>& BreedTable()
	{
		static std::vector<FBreedKind> T = MakeTable();
		return T;
	}

	const FBreedKind* BreedOf(const std::string& SpeciesId)
	{
		for (const FBreedKind& K : BreedTable())
		{
			if (SpeciesId == K.Id)
			{
				return &K;
			}
		}
		return nullptr;
	}

	const char* NestName(ENest K)
	{
		static const char* const Names[] = { "None", "Cup", "Dome", "Platform", "Cavity", "Eave", "MudCup", "Burrow", "Scrape", "Mound" };
		return (int)K < (int)ENest::Num ? Names[(int)K] : "?";
	}

	const char* MaterialName(EMaterial Mat)
	{
		static const char* const Names[] = { "None", "Sticks", "Twigs", "Grass", "Moss", "Mud", "Reed", "Feathers" };
		return (int)Mat < (int)EMaterial::Num ? Names[(int)Mat] : "?";
	}

	const char* StageName(EStage S)
	{
		static const char* const Names[] = { "None", "Old", "Build", "Eggs", "Chicks", "Fledged", "Failed" };
		return (int)S < (int)EStage::Num ? Names[(int)S] : "?";
	}

	const char* NestEventName(ENestEvent E)
	{
		static const char* const Names[] = { "BuildStart", "Material", "Lay", "Hatch", "Feed", "Fledge", "Fail", "Alarm", "Dive", "Song", "Beg" };
		return (int)E < (int)ENestEvent::Num ? Names[(int)E] : "?";
	}

	const char* DutyName(EDuty D)
	{
		static const char* const Names[] = { "None", "Sing", "Gather", "Carry", "Work", "Sit", "Forage", "Bring", "Feed", "Guard", "Dive" };
		return (int)D < (int)EDuty::Num ? Names[(int)D] : "?";
	}

	std::string FNesting::TableJson()
	{
		std::string Out = "{\n  \"species\": [\n";
		char Buf[512];
		const std::vector<FBreedKind>& T = BreedTable();
		for (size_t I = 0; I < T.size(); I++)
		{
			const FBreedKind& K = T[I];
			std::snprintf(Buf, sizeof(Buf),
				"    {\"id\": \"%s\", \"nest\": \"%s\", \"material\": \"%s\", \"where\": \"%s\", \"radius\": %.3f, \"heightMin\": %.1f, "
				"\"heightMax\": %.1f, \"colonial\": %s, \"clutchMax\": %d, \"precocial\": %s, \"reuse\": %s, \"incubDays\": %d, "
				"\"nestlingDays\": %d, \"dependDays\": %d}%s\n",
				K.Id, NestName(K.Nest), MaterialName(K.Material), K.Where, K.NestRadius, K.HeightMin, K.HeightMax,
				K.bColonial ? "true" : "false", K.ClutchMax, K.bPrecocial ? "true" : "false", K.bReuse ? "true" : "false", K.IncubDays,
				K.NestlingDays, K.DependDays, I + 1 < T.size() ? "," : "");
			Out += Buf;
		}
		Out += "  ]\n}\n";
		return Out;
	}

	// --- the plan of a nest's year ------------------------------------------------------------------

	const FBreedKind& FNesting::KindOf(const FNest& Nest) const { return BreedTable()[Nest.Kind]; }

	void FNesting::MakePlan(FNest& Nest, int InYear)
	{
		const FBreedKind& K = KindOf(Nest);
		Nest.PlanYear = InYear;
		Nest.Plan.clear();
		if ((Nest.Male < 0 && Nest.Female < 0) || InYear < Nest.FirstYear)
		{
			return;
		}
		uint32_t R = Hash(Nest.Seed * 2654435761u + (uint32_t)InYear * 40503u) | 1u;
		// a colony keeps to one calendar (its pairs lay within a week or so of each other)
		uint32_t RC = Hash((uint32_t)(Nest.Colony + 1) * 977u + (uint32_t)InYear * 131u + (uint32_t)Nest.Kind * 7u) | 1u;
		const float Span = (float)(K.LayUntil - K.LayFrom);
		float Lay = K.bColonial && Nest.Colony >= 0
			? K.LayFrom + Span * (0.2f + 0.6f * Unit(RC)) + (Unit(R) - 0.5f) * std::min(10.f, Span * 0.4f)
			: K.LayFrom + Span * Unit(R);
		bool bNew = !K.bReuse || InYear == Nest.FirstYear;
		int Brood = 0;
		for (int Tries = 0; Tries < 6 && Lay <= (float)K.LayLast; Tries++)
		{
			FAttempt A;
			const float Gap = 1.f + Unit(R) * 3.f;
			A.Build = Lay - Gap - (bNew ? (float)K.BuildDays : std::max(2.f, K.BuildDays / 3.f));
			A.Lay = Lay;
			A.Clutch = std::max(1, K.ClutchMin + (int)(Unit(R) * (K.ClutchMax - K.ClutchMin + 1)) - (Brood > 0 ? 1 : 0));
			A.Hatch = Lay + (A.Clutch - 1) * K.LayInterval + K.IncubDays;
			A.Fledge = A.Hatch + K.NestlingDays;
			A.Independent = A.Fledge + K.DependDays;
			// lost on some day between the first egg and fledging (Mayfield: one survival rate a day)
			const float U = std::max(1e-6f, Unit(R));
			const float Lasts = std::log(U) / std::log(std::max(0.5f, std::min(0.9999f, K.Survival)));
			if (Lasts < A.Fledge - A.Lay)
			{
				A.Fail = A.Lay + Lasts;
			}
			if (Nest.Deserted >= A.Lay && Nest.Deserted < std::min(A.Fail, A.Fledge))
			{
				A.Fail = Nest.Deserted;
			}
			int Young = 0;
			for (int C = 0; C < A.Clutch; C++)
			{
				Young += Unit(R) < 0.82f ? 1 : 0;
			}
			A.Fledglings = A.Fail < 1e8f ? 0 : std::max(1, Young);
			Nest.Plan.push_back(A);
			if (A.Fail < 1e8f)
			{
				// a repeat clutch, if the season allows: a few days to get over it, then (a new nest and) eggs again
				bNew = !K.bReuse;
				Lay = A.Fail + 3.f + Unit(R) * 4.f + (bNew ? (float)K.BuildDays : std::max(2.f, K.BuildDays / 3.f)) + 2.f;
				continue;
			}
			Brood++;
			if (Brood >= K.Broods || Unit(R) > (K.Id == std::string("GreatTit") ? 0.25f : 0.7f))
			{
				break;
			}
			// the next brood: thrushes and sparrows start laying again a week after the first young leave
			Lay = A.Fledge + 4.f + Unit(R) * 8.f;
			bNew = !K.bReuse;
		}
	}

	FNestState FNesting::StateOn(int NestId, int InYear, float Day)
	{
		FNest& Nest = Nests[NestId];
		const FBreedKind& K = KindOf(Nest);
		if (Nest.PlanYear != InYear)
		{
			MakePlan(Nest, InYear);
		}
		FNestState St;
		// before this year's first attempt: what is left of earlier years
		const bool bBuiltBefore = InYear > Nest.FirstYear;
		if (bBuiltBefore && (K.bReuse || Day < 110.f))
		{
			St.Stage = EStage::Old;
			St.Progress = K.bReuse ? 1.f : Clamp((110.f - Day) / 80.f, 0.3f, 1.f);   // last year's cup falls apart over the spring
		}
		for (int I = 0; I < (int)Nest.Plan.size(); I++)
		{
			const FAttempt& A = Nest.Plan[I];
			if (Day < A.Build)
			{
				break;
			}
			St = FNestState();
			St.Attempt = I;
			St.Progress = 1.f;
			if (Day >= A.Fail)
			{
				// robbed or given up: torn and empty for a while, then just an old nest
				St.Stage = Day < A.Fail + 12.f ? EStage::Failed : EStage::Old;
				St.Progress = A.Lay > A.Build ? 1.f : 0.5f;
				continue;                            // (a later attempt may have started)
			}
			if (Day < A.Lay)
			{
				St.Stage = EStage::Build;
				const bool bNew = !K.bReuse || (InYear == Nest.FirstYear && I == 0);
				const float T = Clamp((Day - A.Build) / std::max(0.5f, A.Lay - 1.f - A.Build), 0.f, 1.f);
				St.Progress = bNew ? T : 0.7f + 0.3f * T;
				St.bPairHere = true;
				continue;
			}
			if (Day < A.Hatch)
			{
				St.Stage = EStage::Eggs;
				St.Eggs = std::min(A.Clutch, 1 + (int)((Day - A.Lay) / std::max(0.5f, K.LayInterval)));
				St.bPairHere = true;
				continue;
			}
			if (Day < A.Fledge)
			{
				St.Stage = EStage::Chicks;
				St.Age = Day - A.Hatch;
				const float T = Clamp(St.Age / std::max(1.f, (float)K.NestlingDays), 0.f, 1.f);
				St.Chicks = std::max(A.Fledglings, (int)std::lround(A.Clutch + (A.Fledglings - A.Clutch) * T));
				St.bPairHere = true;
				continue;
			}
			if (Day < A.Independent)
			{
				St.Stage = EStage::Fledged;
				St.Chicks = A.Fledglings;
				St.Age = Day - A.Hatch;
				St.bPairHere = true;
				continue;
			}
			St.Stage = EStage::Old;
		}
		if (St.Stage == EStage::None && InYear == Nest.FirstYear && !Nest.Plan.empty() && Day > Nest.Plan.back().Independent)
		{
			St.Stage = EStage::Old;
		}
		if (St.Stage == EStage::Old && IsHidden(K.Nest) && K.Nest != ENest::Cavity)
		{
			St.Progress = 1.f;
		}
		return St;
	}

	float FNesting::Season(const FWorld& W, int Species) const
	{
		const FBreedKind* K = BreedOf(SpeciesTable()[Species].Id);
		if (!K)
		{
			return 0.f;
		}
		const float From = (float)(K->LayFrom - K->BuildDays - 20), Until = (float)(K->LayLast + K->IncubDays + K->NestlingDays);
		const float D = W.DayOfYear;
		return Clamp(std::min((D - From) / 10.f, (Until - D) / 10.f), 0.f, 1.f);
	}

	bool FNesting::DownyYoung(int Bird) const
	{
		const int NI = YoungOf(Bird);
		if (NI < 0)
		{
			return false;
		}
		const FNest& Nest = Nests[NI];
		const FBreedKind& K = KindOf(Nest);
		return K.bPrecocial && Nest.State.Age < (K.Id == std::string("MuteSwan") ? 70.f : 25.f);
	}

	float FNesting::BodyScale(const FWorld& W, int Bird) const
	{
		const int NI = YoungOf(Bird);
		if (NI < 0)
		{
			return 1.f;
		}
		const FNest& Nest = Nests[NI];
		const FBreedKind& K = KindOf(Nest);
		const float Age = Nest.State.Age;
		if (K.bPrecocial)
		{
			// downy young the size of a fist growing to nearly the parents' size as they fledge (swans by ~4 months, gulls ~6 weeks)
			const float Grow = K.Id == std::string("MuteSwan") ? 130.f : 40.f;
			return Clamp(0.3f + 0.7f * Age / Grow, 0.3f, 0.97f);
		}
		(void)W;
		return Clamp(0.85f + 0.15f * (Age - K.NestlingDays) / std::max(1.f, (float)K.DependDays), 0.85f, 1.f);
	}

	// --- setting up: sites, pairs, colonies ---------------------------------------------------------

	void FNesting::Setup(FWorld& W)
	{
		Year = W.Year;
		Nests.clear();
		Events.clear();
		Role.assign(W.Birds.size(), FRole());
		Pool.clear();
		ColonyAt.clear();
		SavedHome.assign(W.Groups.size(), V3());
		bHomeMoved.assign(W.Groups.size(), 0);
		for (size_t G = 0; G < W.Groups.size(); G++)
		{
			SavedHome[G] = W.Groups[G].Home;
		}
		const std::vector<FBreedKind>& T = BreedTable();
		const std::vector<FWhere>& Wh = WhereTable();
		std::vector<int> PerSiteCount(W.Sites.size(), 0);
		uint32_t R = W.Opt.Seed ^ 0x5eed0001u;
		struct FColony { int Kind; V3 At; int Pairs; };
		std::vector<FColony> Cols;

		for (int KI = 0; KI < (int)T.size(); KI++)
		{
			const FBreedKind& K = T[KI];
			const int Sp = FindSpecies(K.Id);
			if (Sp < 0 || SpeciesTable()[Sp].Population <= 0)
			{
				continue;
			}
			const FSpecies& S = SpeciesTable()[Sp];
			// the sites it nests on
			std::vector<int> Cand;
			for (int I = 0; I < (int)W.Sites.size(); I++)
			{
				const FSite& Site = W.Sites[I];
				if (!(Site.Flags & SiteFeeder) && Wh[KI].W[(int)Site.Kind] > 0.f)
				{
					Cand.push_back(I);
				}
			}
			if (Cand.empty())
			{
				continue;
			}
			// the pairs: male and female of a group, then the lone birds of neighbouring groups
			std::vector<std::pair<int, int>> Pairs;
			std::vector<int> Lone;
			for (const FGroup& G : W.Groups)
			{
				if (G.Species != Sp)
				{
					continue;
				}
				std::vector<int> Ms, Fs;
				for (int Id : G.Members)
				{
					(W.Birds[Id].bFemale ? Fs : Ms).push_back(Id);
				}
				const size_t NP = std::min(Ms.size(), Fs.size());
				for (size_t I = 0; I < NP; I++)
				{
					Pairs.push_back({ Ms[I], Fs[I] });
				}
				for (size_t I = NP; I < Ms.size(); I++)
				{
					Lone.push_back(Ms[I]);
				}
				for (size_t I = NP; I < Fs.size(); I++)
				{
					Lone.push_back(Fs[I]);
				}
			}
			for (size_t I = 0; I < Lone.size(); I++)
			{
				const int A = Lone[I];
				if (A < 0)
				{
					continue;
				}
				int Best = -1;
				float BestD = std::max(600.f, S.HomeRange * 2.f);
				for (size_t J = I + 1; J < Lone.size(); J++)
				{
					const int B = Lone[J];
					if (B < 0 || W.Birds[B].bFemale == W.Birds[A].bFemale)
					{
						continue;
					}
					const float D = (W.Groups[W.Birds[A].Group].Home - W.Groups[W.Birds[B].Group].Home).Len2D();
					if (D < BestD)
					{
						BestD = D;
						Best = (int)J;
					}
				}
				if (Best >= 0)
				{
					const int B = Lone[Best];
					Pairs.push_back(W.Birds[A].bFemale ? std::make_pair(B, A) : std::make_pair(A, B));
					Lone[Best] = -1;
				}
			}
			// a rare species with a single bird on the map (a peregrine, an eagle): its mate is out hunting beyond the map
			if (Pairs.empty() && S.Population <= 6)
			{
				for (int Id : Lone)
				{
					if (Id >= 0)
					{
						Pairs.push_back(W.Birds[Id].bFemale ? std::make_pair(-1, Id) : std::make_pair(Id, -1));
					}
				}
			}
			if (std::getenv("NEST_DEBUG"))
			{
				std::printf("nest setup %s: %d pairs, %d candidate sites\n", K.Id, (int)Pairs.size(), (int)Cand.size());
			}
			const size_t MaxNests = 160;
			if (Pairs.size() > MaxNests)
			{
				Pairs.resize(MaxNests);
			}
			std::vector<int> Mine;          // this species' nests so far
			for (const std::pair<int, int>& P : Pairs)
			{
				const FBird& Male = W.Birds[P.first >= 0 ? P.first : P.second];
				const V3 Home = W.Groups[Male.Group].Home;
				// its colony: the nearest of the species' with room, else a new one at the best site near home
				int Col = -1;
				V3 Anchor = Home;
				float Reach = std::max(30.f, S.HomeRange * 0.35f);
				if (K.bColonial)
				{
					float BestD = S.HomeRange * 0.9f;
					for (int C = 0; C < (int)Cols.size(); C++)
					{
						const float D = (Cols[C].At - Home).Len2D();
						if (Cols[C].Kind == KI && Cols[C].Pairs < K.ColonySize && D < BestD)
						{
							BestD = D;
							Col = C;
						}
					}
					if (Col < 0)
					{
						int BestS = -1;
						float BestScore = -1e9f;
						for (int I : Cand)
						{
							const float D = (W.Sites[I].Center() - Home).Len2D();
							if (D > S.HomeRange * 0.6f)
							{
								continue;
							}
							const float Sc = Wh[KI].W[(int)W.Sites[I].Kind] * (0.6f + 0.4f * Unit(R)) - D / S.HomeRange;
							if (Sc > BestScore)
							{
								BestScore = Sc;
								BestS = I;
							}
						}
						if (BestS < 0)
						{
							continue;
						}
						Col = (int)Cols.size();
						Cols.push_back({ KI, W.Sites[BestS].Center(), 0 });
					}
					Anchor = Cols[Col].At;
					Reach = std::max(K.ColonyRadius, 8.f);
				}
				// the site
				int Pick = -1;
				for (int Pass = 0; Pass < 2 && Pick < 0; Pass++, Reach *= 2.5f)
				{
					float BestScore = -1e9f;
					for (int I : Cand)
					{
						const FSite& Site = W.Sites[I];
						const float D = (Site.Center() - Anchor).Len2D();
						if (D > Reach || PerSiteCount[I] >= std::max(K.PerSite, 1) + 2)
						{
							continue;
						}
						int Here = 0;
						bool bClose = false;
						for (int NI : Mine)
						{
							if (Nests[NI].Site == I)
							{
								Here++;
							}
							if ((Nests[NI].Pos - Site.Center()).Len2D() < K.Spacing * 0.8f && Nests[NI].Site != I)
							{
								bClose = true;
							}
						}
						if (Here >= K.PerSite || bClose)
						{
							continue;
						}
						const float Sc = Wh[KI].W[(int)Site.Kind] * (0.5f + 0.5f * Unit(R)) - 0.3f * D / Reach;
						if (Sc > BestScore)
						{
							BestScore = Sc;
							Pick = I;
						}
					}
				}
				if (Pick < 0)
				{
					continue;            // no room: a pair that doesn't breed this year (a floater)
				}
				FNest Nest;
				Nest.Id = (int)Nests.size();
				Nest.Species = Sp;
				Nest.Kind = KI;
				Nest.Site = Pick;
				Nest.Colony = Col;
				Nest.Male = P.first;
				Nest.Female = P.second;
				Nest.Seed = Hash((uint32_t)Nest.Id * 7919u + (uint32_t)KI * 104729u + W.Opt.Seed) | 1u;
				uint32_t NR = Nest.Seed;
				Nest.FirstYear = K.bReuse ? Year - 1 - (int)(Unit(NR) * 4.f) : Year;
				const FSite& Site = W.Sites[Pick];
				const float Ground = W.GroundZ(Site.A.X, Site.A.Y);
				Nest.Yaw = Unit(NR) * 2.f * Pi;
				if (IsTree(Site.Kind))
				{
					// in a crown: platforms near the trunk, cups out on the side branches; holes and boxes on the trunk, below the crown
					const float H = K.HeightMin + (K.HeightMax - K.HeightMin) * Unit(NR);
					const float Out = K.Nest == ENest::Cavity ? 0.f : Site.Radius * (K.Nest == ENest::Platform ? 0.25f : 0.55f) * Unit(NR);
					Nest.Pos = Site.A + V3(std::cos(Nest.Yaw) * Out, std::sin(Nest.Yaw) * Out, 0.f);
					const float Low = Site.A.Z - Site.Radius * 0.9f, High = Site.A.Z + Site.Radius * 0.6f;
					Nest.Pos.Z = K.Nest == ENest::Cavity ? Clamp(Ground + H, Ground + 1.2f, std::max(Ground + 1.3f, Low))
						: Clamp(Ground + H, std::min(Low, High - 0.5f), High);
					if (K.Nest == ENest::Cavity)
					{
						Nest.Pos += V3(std::cos(Nest.Yaw), std::sin(Nest.Yaw), 0.f) * 0.15f;    // the box on the trunk's side
					}
				}
				else if (IsGroundHabitat(Site.Kind))
				{
					uint32_t SR = NR;
					Nest.Pos = W.SpotOn(Site, SR);
					Nest.Pos.Z = std::max(W.GroundZ(Nest.Pos.X, Nest.Pos.Y), Site.Kind == EHabitat::Reed ? W.MeanSeaZ : -1e6f);
					if (Site.Kind == EHabitat::Reed || Site.Kind == EHabitat::Beach)
					{
						Nest.Pos.Z = std::max(Nest.Pos.Z, Site.A.Z);
					}
				}
				else
				{
					// a roof, a tower, the cliff: along its edge; under it for the hole nesters (the game finds the wall)
					const float U = Unit(NR);
					Nest.Pos = Site.A + (Site.B - Site.A) * U;
					const V3 Along = (Site.B - Site.A).Flat();
					if (Along.Len2D() > 0.1f)
					{
						const V3 A = Along.Norm();
						Nest.Yaw = std::atan2(A.X, -A.Y) + (Unit(NR) < 0.5f ? Pi : 0.f);
					}
					if (K.Nest == ENest::Eave || K.Nest == ENest::MudCup || K.Nest == ENest::Cavity)
					{
						Nest.Pos.Z -= 0.35f;
					}
					else if (K.Nest == ENest::Burrow)
					{
						Nest.Pos.Z -= 0.5f + Unit(NR) * 2.5f;
					}
				}
				PerSiteCount[Pick]++;
				if (Col >= 0)
				{
					Cols[Col].Pairs++;
				}
				for (int Id : { P.first, P.second })
				{
					if (Id >= 0)
					{
						Role[Id].Nest = Nest.Id;
					}
				}
				Mine.push_back(Nest.Id);
				Nests.push_back(Nest);
			}
		}
		for (const FColony& C : Cols)
		{
			ColonyAt.push_back(C.At);
		}
		// the pool of fledglings: birds of the bird life that are away until a brood near the camera leaves its nest
		for (int I = 0; I < PoolSize && !W.Groups.empty(); I++)
		{
			FBird B;
			B.Id = (int)W.Birds.size();
			B.Species = W.Groups[0].Species;
			B.Group = 0;
			B.bGone = true;
			B.Rng = Hash((uint32_t)B.Id * 31337u) | 1u;
			B.Pos = B.Spot = W.Groups[0].Home + V3(0.f, 0.f, 0.f);      // (somewhere real while away)
			B.Pos.Z = B.Spot.Z = W.GroundZ(B.Pos.X, B.Pos.Y) + 1.f;
			W.Birds.push_back(B);
			Role.push_back(FRole());
			Pool.push_back(B.Id);
		}
		LastDay = -1;
		MinuteTimer = 0.f;
		for (FNest& Nest : Nests)
		{
			Update(W, Nest, W.DayOfYear, false);
		}
	}

	// --- stages and events --------------------------------------------------------------------------

	void FNesting::Emit(ENestEvent E, int Nest, int Bird, const V3& P)
	{
		Events.push_back({ E, Nest, Bird, P });
		Count[(int)E]++;
	}

	void FNesting::Update(FWorld& W, FNest& Nest, float Day, bool bEvents)
	{
		const FNestState Was = Nest.State;
		Nest.State = StateOn(Nest.Id, Year, Day);
		const FNestState& Now = Nest.State;
		if (!bEvents)
		{
			return;
		}
		const int Parent = Nest.Female >= 0 ? Nest.Female : Nest.Male;
		if (Now.Stage == EStage::Build && Was.Stage != EStage::Build)
		{
			Emit(ENestEvent::BuildStart, Nest.Id, Parent, Nest.Pos);
		}
		if (Now.Stage == EStage::Eggs && Now.Eggs > Was.Eggs)
		{
			Emit(ENestEvent::Lay, Nest.Id, Nest.Female, Nest.Pos);
		}
		if (Now.Stage == EStage::Chicks && Was.Stage != EStage::Chicks)
		{
			Emit(ENestEvent::Hatch, Nest.Id, Parent, Nest.Pos);
		}
		if (Now.Stage == EStage::Fledged && Was.Stage != EStage::Fledged)
		{
			Emit(ENestEvent::Fledge, Nest.Id, Parent, Nest.Pos);
		}
		if (Now.Stage == EStage::Failed && Was.Stage != EStage::Failed)
		{
			Emit(ENestEvent::Fail, Nest.Id, Parent, Nest.Pos);
		}
		if (Now.Stage != EStage::Fledged && !Nest.Young.empty())
		{
			ReleaseYoung(W, Nest);
		}
		if (!Now.bPairHere)
		{
			for (int Id : { Nest.Male, Nest.Female })
			{
				if (Id >= 0 && Role[Id].Duty != EDuty::None)
				{
					End(W, W.Birds[Id], true);
				}
			}
		}
	}

	void FNesting::Step(FWorld& W, float Dt)
	{
		// (Events are cleared by the caller after it read them, like FWorld::Events: the birds' moves before this emit too)
		if (Role.size() < W.Birds.size())
		{
			Role.resize(W.Birds.size());          // birds added later (AddBird, AddFlock) have no nest
		}
		const float Day = W.DayOfYear;
		MinuteTimer -= Dt;
		if (Year != W.Year)
		{
			Year = W.Year;               // a new year: new plans (and no events for the jump)
			LastDay = -1;
		}
		if (MinuteTimer <= 0.f || (int)Day != LastDay)
		{
			// the stages, once a game minute (each nest's plan is a few numbers; this is ~1500 comparisons)
			MinuteTimer = 60.f;
			const bool bEvents = LastDay >= 0;
			LastDay = (int)Day;
			for (FNest& Nest : Nests)
			{
				Update(W, Nest, Day, bEvents);
			}
			// the breeding pairs keep near their nests: the group's home moves to them for the season
			std::vector<V3> Sum(W.Groups.size(), V3());
			std::vector<int> Num(W.Groups.size(), 0);
			for (const FNest& Nest : Nests)
			{
				if (!Nest.State.bPairHere)
				{
					continue;
				}
				for (int Id : { Nest.Male, Nest.Female })
				{
					if (Id < 0)
					{
						continue;
					}
					const int G = W.Birds[Id].Group;
					if (G >= 0 && G < (int)Sum.size())
					{
						Sum[G] += Nest.Pos;
						Num[G]++;
					}
				}
			}
			for (size_t G = 0; G < W.Groups.size() && G < SavedHome.size(); G++)
			{
				if (Num[G] > 0)
				{
					const V3 At = Sum[G] * (1.f / (float)Num[G]);
					W.Groups[G].Home = SavedHome[G] + (At - SavedHome[G]) * 0.85f;
					bHomeMoved[G] = 1;
				}
				else if (bHomeMoved[G])
				{
					W.Groups[G].Home = SavedHome[G];
					bHomeMoved[G] = 0;
				}
			}
		}
		// fledglings near the camera come out of the pool; far ones go back to it (every few seconds)
		YoungTimer -= Dt;
		if (YoungTimer <= 0.f)
		{
			YoungTimer = 4.f;
			for (FNest& Nest : Nests)
			{
				if (Nest.State.Stage != EStage::Fledged)
				{
					continue;
				}
				const float D = (Nest.Pos - W.Camera).Len2D();
				if (Nest.Young.empty() && D < 320.f)
				{
					Fledge(W, Nest, Nest.State.Chicks);
				}
				else if (!Nest.Young.empty() && D > 600.f)
				{
					ReleaseYoung(W, Nest);
				}
			}
		}
		// the player near a nest with eggs or young
		GuardTimer -= Dt;
		if (GuardTimer <= 0.f && W.Player.bPresent)
		{
			const float Every = 0.5f;
			GuardTimer = Every;
			for (FNest& Nest : Nests)
			{
				const EStage St = Nest.State.Stage;
				if (St == EStage::Eggs || St == EStage::Chicks || St == EStage::Fledged || (St == EStage::Build && Nest.State.Progress > 0.3f))
				{
					Guard(W, Nest, Every);
				}
				else
				{
					Nest.Disturb = std::max(0.f, Nest.Disturb - Every * 0.2f);
				}
			}
		}
	}

	void FNesting::Guard(FWorld& W, FNest& Nest, float Dt)
	{
		const FBreedKind& K = KindOf(Nest);
		const FSpecies& S = SpeciesTable()[Nest.Species];
		const V3 P = W.Player.Feet;
		const float D = (P - Nest.Pos).Len2D();
		const float AlarmR = 8.f + 28.f * K.Defend + S.Length * 25.f;
		if (D > AlarmR * 1.5f)
		{
			Nest.Disturb = std::max(0.f, Nest.Disturb - Dt * 0.2f);
			return;
		}
		// standing over it: the pair keeps off while the eggs cool; long enough and they give it up
		const float TooClose = 4.f + S.Length * 12.f;
		if (D < TooClose && (Nest.State.Stage == EStage::Eggs || Nest.State.Stage == EStage::Build))
		{
			Nest.Disturb += Dt;
			if (Nest.Disturb > 240.f && Nest.Deserted < 0.f)
			{
				Nest.Deserted = W.DayOfYear;
				MakePlan(Nest, Year);
				Update(W, Nest, W.DayOfYear, true);
				return;
			}
		}
		else
		{
			Nest.Disturb = std::max(0.f, Nest.Disturb - Dt * 0.1f);
		}
		if (D > AlarmR || Nest.State.Stage == EStage::Build)
		{
			return;
		}
		for (int Id : { Nest.Male, Nest.Female })
		{
			if (Id < 0)
			{
				continue;
			}
			FBird& B = W.Birds[Id];
			FRole& R = Role[Id];
			if (B.bGone || B.Act == EAct::Claimed || B.Act == EAct::Approach || B.Act == EAct::Eat || B.Act == EAct::Sleep
				|| R.Duty == EDuty::Guard || R.Duty == EDuty::Dive || (B.Pos - Nest.Pos).Len() > 150.f)
			{
				continue;
			}
			if (R.Duty == EDuty::Sit && B.Act != EAct::Flee && D > S.FleeDistance * 0.35f)
			{
				continue;            // sitting tight
			}
			if (W.Corvids() && W.Corvids()->Busy(Id))
			{
				continue;
			}
			if (B.Act == EAct::Flee && B.ActTime < 3.f)
			{
				continue;
			}
			End(W, B, false);
			const bool bDive = K.Defend >= 0.8f && W.Rand(B.Rng) < 0.8f;
			Start(W, B, bDive ? EDuty::Dive : EDuty::Guard, 25.f + W.Rand(B.Rng) * 30.f);
			if (bDive)
			{
				R.Step = 0;
				W.FlyTo(B, -1, W.Player.Eye);
			}
			else
			{
				// a perch near the nest, between it and the player
				V3 Spot = Nest.Pos + V3(W.Rand(B.Rng) * 6.f - 3.f, W.Rand(B.Rng) * 6.f - 3.f, 0.f);
				const FSite& Site = W.Sites[Nest.Site];
				if (IsTree(Site.Kind))
				{
					Spot.Z = std::max(Spot.Z, Site.A.Z);
				}
				else
				{
					Spot.Z = std::max(Spot.Z, W.GroundZ(Spot.X, Spot.Y));
				}
				R.Goal = Spot;
				W.FlyTo(B, Nest.Site, Spot);
			}
			Emit(ENestEvent::Alarm, Nest.Id, Id, B.Pos);
		}
	}

	// --- duties -------------------------------------------------------------------------------------

	bool FNesting::Busy(int Bird) const
	{
		if (Bird < 0 || Bird >= (int)Role.size())
		{
			return false;
		}
		const FRole& R = Role[Bird];
		return R.Duty != EDuty::None || (R.YoungOf >= 0 && R.bTied);
	}

	float FNesting::DangerScale(const FBird& B) const
	{
		if (B.Id < 0 || B.Id >= (int)Role.size())
		{
			return 1.f;
		}
		const FRole& R = Role[B.Id];
		if (R.Duty == EDuty::Sit && R.Step == 1)
		{
			return 0.35f;            // on the eggs: it sits tight
		}
		if (R.Duty == EDuty::Guard || R.Duty == EDuty::Dive)
		{
			return 0.4f;
		}
		return 1.f;
	}

	bool FNesting::Daytime(const FWorld& W, const FSpecies& S) const
	{
		return W.Hour > W.Sunrise + S.WakeMinutes / 60.f + 0.1f && W.Hour < W.Sunset - 0.4f;
	}

	bool FNesting::Sitter(const FWorld& W, const FNest& Nest, const FBird& B) const
	{
		const FBreedKind& K = KindOf(Nest);
		const FNestState& St = Nest.State;
		const bool bBrood = St.Stage == EStage::Chicks && St.Age < std::max(1.f, K.NestlingDays * 0.35f);
		if (St.Stage != EStage::Eggs && !bBrood)
		{
			return false;
		}
		const FSpecies& S = SpeciesTable()[Nest.Species];
		const bool bNight = !Daytime(W, S);
		int Who = Nest.Female;
		const float Minutes = (float)(W.Time / 60.0) + (float)(Nest.Seed % 997);
		if (bNight)
		{
			Who = Nest.Female;
		}
		else if (K.Sits == EWho::Both)
		{
			Who = ((int)(Minutes / 120.f) & 1) ? Nest.Male : Nest.Female;
		}
		else
		{
			// she sits most of the time with short breaks to feed (and her mate feeds her)
			const float Cycle = S.Mass < 0.2f ? 45.f : 180.f;
			const float On = bBrood ? 0.55f : 0.8f;
			if (std::fmod(Minutes, Cycle) > Cycle * On)
			{
				Who = -1;
			}
		}
		if (Nest.Male < 0 || Nest.Female < 0)
		{
			Who = Who == -1 && Nest.Female >= 0 ? -1 : (Nest.Male < 0 ? Nest.Female : Nest.Male);   // a lone parent does it all
		}
		else if (Who >= 0 && W.Birds[Who].bGone)
		{
			Who = Mate(Nest, Who);
		}
		return Who == B.Id;
	}

	V3 FNesting::NestSeat(const FNest& Nest, bool bInside) const
	{
		const FBreedKind& K = KindOf(Nest);
		const V3 Fwd(std::cos(Nest.Yaw), std::sin(Nest.Yaw), 0.f);
		if (IsHidden(K.Nest))
		{
			// inside (the box hides it); feeding: at the entrance
			return bInside ? Nest.Pos - Fwd * 0.04f : Nest.Pos + Fwd * 0.05f;
		}
		return Nest.Pos + V3(0.f, 0.f, K.NestRadius * 0.35f);
	}

	void FNesting::Start(FWorld& W, FBird& B, EDuty D, float Len)
	{
		(void)W;
		FRole& R = Role[B.Id];
		R.Duty = D;
		R.Time = 0.f;
		R.Len = Len;
		R.Step = 0;
		DutyStarts[(int)D]++;
	}

	void FNesting::End(FWorld& W, FBird& B, bool bBackToGroup)
	{
		FRole& R = Role[B.Id];
		Drop(W, B);
		R.Duty = EDuty::None;
		R.Step = 0;
		if (B.bGone || B.Act == EAct::Claimed || B.Act == EAct::Flee)
		{
			return;
		}
		const FGroup& G = W.Groups[B.Group];
		if (bBackToGroup && G.Site >= 0 && W.Rand(B.Rng) < 0.5f)
		{
			W.FlyTo(B, G.Site, W.SpotFor(B, G.Site));
		}
		else if (!IsAirborne(B.Act))
		{
			W.SetAct(B, EAct::Perch, 1.f + W.Rand(B.Rng) * 3.f);
		}
		else if (B.Site < 0)
		{
			const int Nest = R.Nest >= 0 ? R.Nest : R.YoungOf;
			if (Nest >= 0)
			{
				W.FlyTo(B, Nests[Nest].Site, W.SpotFor(B, Nests[Nest].Site));
			}
		}
	}

	void FNesting::Carry(FWorld& W, FBird& B, EMaterial Mat)
	{
		FRole& R = Role[B.Id];
		R.Mat = Mat;
		R.Carry = -1;
		if ((Mat == EMaterial::Sticks || Mat == EMaterial::Twigs) && W.Corvids() && W.Corvids()->IsCorvid(B.Id))
		{
			R.Carry = W.Corvids()->GiveItem(W, B, EItem::Stick);      // a real stick (a neighbour may steal it)
		}
	}

	void FNesting::Drop(FWorld& W, FBird& B)
	{
		FRole& R = Role[B.Id];
		if (R.Carry >= 0 && W.Corvids())
		{
			W.Corvids()->UseItem(W, B, R.Carry);
		}
		R.Carry = -1;
		R.Mat = EMaterial::None;
	}

	EMaterial FNesting::Carrying(int Bird) const
	{
		if (Bird < 0 || Bird >= (int)Role.size() || Role[Bird].Carry >= 0)
		{
			return EMaterial::None;           // nothing, or a corvid's stick (the corvid items draw it)
		}
		return Role[Bird].Mat;
	}

	int FNesting::MaterialSite(FWorld& W, const FBird& B, const FNest& Nest, EMaterial Mat) const
	{
		const FBreedKind& K = KindOf(Nest);
		int Best = -1;
		float BestScore = -1e9f;
		uint32_t R = B.Rng ^ 0xa5a5u;
		const float Range = std::max(15.f, K.MaterialRange);
		// a few random looks are enough (sites are many): 64 draws among those in range
		for (int Try = 0, Found = 0; Try < 600 && Found < 64; Try++)
		{
			const int I = (int)(Unit(R) * (float)W.Sites.size()) % (int)W.Sites.size();
			const FSite& Site = W.Sites[I];
			if (!MaterialIn(Mat, Site.Kind) || (Site.Flags & SiteFeeder))
			{
				continue;
			}
			const float D = (Site.Center() - Nest.Pos).Len2D();
			if (D > Range * 2.f)
			{
				continue;
			}
			Found++;
			const float Sc = -D / Range + Unit(R) * 0.5f;
			if (Sc > BestScore)
			{
				BestScore = Sc;
				Best = I;
			}
		}
		return Best;
	}

	int FNesting::FoodSite(FWorld& W, const FBird& B, const FNest& Nest) const
	{
		const FSpecies& S = SpeciesTable()[B.Species];
		const float Range = Clamp(S.HomeRange * 0.3f, 40.f, 500.f);
		int Best = -1;
		float BestScore = -1e9f;
		uint32_t R = B.Rng ^ 0x5a5au;
		for (int Try = 0, Found = 0; Try < 800 && Found < 48; Try++)
		{
			const int I = (int)(Unit(R) * (float)W.Sites.size()) % (int)W.Sites.size();
			const FSite& Site = W.Sites[I];
			const float Taste = W.IsWinter() && S.bWinterForage ? S.WinterForage[(int)Site.Kind] : S.Forage[(int)Site.Kind];
			if (Taste <= 0.f || (Site.Flags & SiteFeeder) || (Site.Kind == EHabitat::Sea && !IsGroundHabitat(Site.Kind)))
			{
				continue;
			}
			const float D = (Site.Center() - Nest.Pos).Len2D();
			if (D > Range * 2.f)
			{
				continue;
			}
			Found++;
			const float Sc = Taste * (0.6f + 0.4f * Unit(R)) - D / Range;
			if (Sc > BestScore)
			{
				BestScore = Sc;
				Best = I;
			}
		}
		return Best >= 0 ? Best : W.Groups[B.Group].Site;
	}

	bool FNesting::ChooseDuty(FWorld& W, FBird& B, FNest& Nest)
	{
		FRole& R = Role[B.Id];
		const FBreedKind& K = KindOf(Nest);
		const FSpecies& S = SpeciesTable()[B.Species];
		const FGroup& G = W.Groups[B.Group];
		const FNestState& St = Nest.State;
		const bool bDay = Daytime(W, S);
		// the sitter goes to (or stays on) the nest, day and night
		if (Sitter(W, Nest, B))
		{
			Start(W, B, EDuty::Sit, 600.f);
			W.FlyTo(B, Nest.Site, NestSeat(Nest, true));
			return true;
		}
		if (!bDay || G.Plan == 2 || W.Time < R.Next || B.Alert > 0.5f || B.Act == EAct::Watch)
		{
			return false;
		}
		const bool bMale = !B.bFemale;
		switch (St.Stage)
		{
		case EStage::Build:
		{
			const bool bBuilder = K.Builds == EWho::Both || (K.Builds == EWho::Female) == B.bFemale;
			if (!bBuilder)
			{
				// he sings from his post (the territory), or sees her about
				if (K.SingRate > 0.f && W.Rand(B.Rng) < 0.5f)
				{
					break;
				}
				R.Next = W.Time + 60.0 + W.Rand(B.Rng) * 120.0;
				return false;
			}
			R.Next = W.Time + 10.0 + W.Rand(B.Rng) * 80.0;
			if (K.Material == EMaterial::None)
			{
				// digging a burrow, scraping a ledge: work at the nest itself
				Start(W, B, EDuty::Work, 120.f);
				R.Step = 2;
				W.FlyTo(B, Nest.Site, NestSeat(Nest, false));
				return true;
			}
			Start(W, B, EDuty::Gather, 240.f);
			if (Aerial(S))
			{
				// swifts catch their nest stuff on the wing (feathers, seeds of the wind)
				W.SetAct(B, EAct::Hawk, 20.f + W.Rand(B.Rng) * 40.f);
				R.Step = 1;
				return true;
			}
			const int Site = MaterialSite(W, B, Nest, K.Material);
			if (Site < 0)
			{
				R.Duty = EDuty::None;
				return false;
			}
			W.FlyTo(B, Site, W.SpotFor(B, Site));
			return true;
		}
		case EStage::Eggs:
			// the mate brings the sitter food now and then (finches, crows), or sings
			if (K.Sits == EWho::Female && bMale && K.FeedMinutes < 30.f && W.Rand(B.Rng) < 0.3f)
			{
				R.Next = W.Time + 300.0 + W.Rand(B.Rng) * 600.0;
				const int Sitting = Nest.Female;
				if (Sitting >= 0 && Role[Sitting].Duty == EDuty::Sit && Role[Sitting].Step == 1)
				{
					Start(W, B, EDuty::Bring, 120.f);
					R.Step = 2;
					W.FlyTo(B, Nest.Site, NestSeat(Nest, false) + V3(0.f, 0.f, 0.02f));
					return true;
				}
				return false;
			}
			break;
		case EStage::Chicks:
		case EStage::Fledged:
		{
			if (K.bPrecocial && St.Stage == EStage::Fledged)
			{
				// swans and gull chicks feed themselves: the parents keep near them, feeding and watching round about
				if (Nest.Young.empty() || (B.Pos - Nest.Pos).Len2D() < 35.f)
				{
					break;
				}
				R.Next = W.Time + 60.0 + W.Rand(B.Rng) * 120.0;
				Start(W, B, EDuty::Forage, 900.f);
				const V3 Near = Nest.Pos + V3(W.Rand(B.Rng) * 8.f - 4.f, W.Rand(B.Rng) * 8.f - 4.f, 0.f);
				W.FlyTo(B, Nest.Site, V3(Near.X, Near.Y, Nest.Pos.Z));
				return true;
			}
			if (K.FeedMinutes <= 0.f)
			{
				break;
			}
			if (St.Stage == EStage::Fledged && Nest.Young.empty())
			{
				break;                                // fed out of sight
			}
			// a feeding trip: food near home, back to the gaping mouths
			R.Next = W.Time + K.FeedMinutes * 60.0 * (0.15 + 0.2 * W.Rand(B.Rng));
			Start(W, B, EDuty::Forage, std::max(120.f, K.FeedMinutes * 120.f));
			if (Aerial(S) || S.Id == "BarnSwallow" || S.Id == "SandMartin")
			{
				W.SetAct(B, EAct::Hawk, K.FeedMinutes * 60.f * (0.4f + 0.4f * W.Rand(B.Rng)));
				R.Step = 1;
				return true;
			}
			const int Site = FoodSite(W, B, Nest);
			if (Site < 0)
			{
				R.Duty = EDuty::None;
				return false;
			}
			W.FlyTo(B, Site, W.SpotFor(B, Site));
			return true;
		}
		default:
			break;
		}
		// the song from a post near the nest (territorial males, from three weeks before building to the young)
		const float SongChance = W.Hour < W.Sunrise + 4.f ? 0.4f : (W.Hour > W.Sunset - 2.5f ? 0.3f : 0.08f);
		if (bMale && K.SingRate > 0.f && (St.bPairHere || Season(W, B.Species) > 0.5f) && W.Rand(B.Rng) < SongChance)
		{
			R.Next = W.Time + 120.0 + W.Rand(B.Rng) * 400.0;
			Start(W, B, EDuty::Sing, 90.f + W.Rand(B.Rng) * 180.f);
			const FSite& Site = W.Sites[Nest.Site];
			V3 Post = IsTree(Site.Kind) ? Site.A + V3(0.f, 0.f, Site.Radius * 0.8f) : Site.Center() + V3(0.f, 0.f, 0.2f);
			Post.X += W.Rand(B.Rng) * 4.f - 2.f;
			Post.Y += W.Rand(B.Rng) * 4.f - 2.f;
			W.FlyTo(B, Nest.Site, Post);
			return true;
		}
		R.Next = W.Time + 30.0 + W.Rand(B.Rng) * 90.0;
		return false;
	}

	bool FNesting::Think(FWorld& W, FBird& B, float Dt)
	{
		if (B.Id >= (int)Role.size())
		{
			Role.resize(W.Birds.size());
		}
		FRole& R = Role[B.Id];
		if (R.YoungOf >= 0)
		{
			return YoungThink(W, B, R, Dt);
		}
		if (R.Nest < 0)
		{
			return false;
		}
		FNest& Nest = Nests[R.Nest];
		if (B.Act == EAct::Claimed || B.Act == EAct::Approach || B.Act == EAct::Eat || B.Act == EAct::Murmur)
		{
			if (R.Duty != EDuty::None)
			{
				End(W, B, false);
			}
			return false;
		}
		if (R.Duty == EDuty::None)
		{
			if (!Nest.State.bPairHere && Season(W, B.Species) <= 0.f)
			{
				return false;
			}
			if (B.Act == EAct::Flee || (IsAirborne(B.Act) && B.Act != EAct::Fly) || (W.Corvids() && W.Corvids()->Busy(B.Id)))
			{
				return false;
			}
			return ChooseDuty(W, B, Nest);       // the flight it started runs on through BirdSim's Move; arrivals come back here
		}
		R.Time += Dt;
		const FBreedKind& K = KindOf(Nest);
		const FSpecies& S = SpeciesTable()[B.Species];
		if (R.Time > R.Len + 60.f || B.Act == EAct::Flee)
		{
			End(W, B, B.Act != EAct::Flee);
			return false;
		}
		switch (R.Duty)
		{
		case EDuty::Sit:
			if (R.Step == 1)
			{
				if (!Sitter(W, Nest, B))
				{
					End(W, B, true);
					return false;
				}
				B.Pos = B.Spot;
				B.Yaw = IsHidden(K.Nest) ? Nest.Yaw : B.Yaw;
				if (B.ActTime > B.ActLen)
				{
					const bool bNight = !Daytime(W, S);
					W.SetAct(B, bNight ? EAct::Sleep : (W.Rand(B.Rng) < 0.15f ? EAct::Preen : EAct::Perch), 20.f + W.Rand(B.Rng) * 40.f);
				}
				R.Len = R.Time + 600.f;       // as long as it is its turn
			}
			return true;
		case EDuty::Gather:
			if (R.Step == 1 && B.ActTime > B.ActLen)
			{
				// picked up a beakful: back to the nest
				Carry(W, B, K.Material);
				R.Step = 2;
				W.FlyTo(B, Nest.Site, NestSeat(Nest, false));
				Emit(ENestEvent::Material, Nest.Id, B.Id, B.Pos);
			}
			else if (R.Step == 3 && B.ActTime > B.ActLen)
			{
				Drop(W, B);
				End(W, B, false);
				return false;
			}
			return true;
		case EDuty::Work:
			if (R.Step == 3 && B.ActTime > B.ActLen)
			{
				End(W, B, false);
				return false;
			}
			return true;
		case EDuty::Forage:
			if (R.Step == 1 && B.ActTime > B.ActLen)
			{
				// a bill full of caterpillars: to the young (in the nest, or one of the fledglings)
				R.Step = 2;
				V3 To = NestSeat(Nest, false);
				int Site = Nest.Site;
				if (Nest.State.Stage == EStage::Fledged && !Nest.Young.empty())
				{
					const int Y = Nest.Young[(size_t)(W.Rand(B.Rng) * Nest.Young.size()) % Nest.Young.size()];
					R.Target = Y;
					To = W.Birds[Y].Pos + V3(std::cos(W.Birds[Y].Yaw), std::sin(W.Birds[Y].Yaw), 0.f) * (S.Length * 0.6f);
					Site = W.Birds[Y].Site;
				}
				W.FlyTo(B, Site, To);
			}
			else if (R.Step == 1 && !IsAirborne(B.Act) && B.Act != EAct::Peck && B.Act != EAct::Forage)
			{
				W.SetAct(B, EAct::Forage, 0.f);
				B.Spot = B.Pos + V3(W.Rand(B.Rng) * 2.f - 1.f, W.Rand(B.Rng) * 2.f - 1.f, 0.f);
			}
			else if (R.Step == 1 && B.Act == EAct::Peck && B.ActTime > B.ActLen && R.Time < R.Len * 0.5f)
			{
				// walking on between pecks while it looks for food
				W.SetAct(B, EAct::Forage, 0.f);
				B.Spot = B.Pos + V3(W.Rand(B.Rng) * 2.f - 1.f, W.Rand(B.Rng) * 2.f - 1.f, 0.f);
			}
			else if (R.Step == 3 && B.ActTime > B.ActLen)
			{
				End(W, B, false);
				return false;
			}
			return true;
		case EDuty::Bring:
			if (R.Step == 3 && B.ActTime > B.ActLen)
			{
				End(W, B, false);
				return false;
			}
			return true;
		case EDuty::Sing:
			if (R.Step == 1)
			{
				R.SongTimer -= Dt;
				if (R.SongTimer <= 0.f)
				{
					R.SongTimer = 60.f / std::max(0.2f, K.SingRate) * (0.6f + 0.8f * W.Rand(B.Rng));
					W.Emit(EEvent::Song, B);
					Emit(ENestEvent::Song, Nest.Id, B.Id, B.Pos);
				}
				if (B.ActTime > B.ActLen)
				{
					W.SetAct(B, EAct::Perch, 10.f + W.Rand(B.Rng) * 10.f);
				}
				if (R.Time > R.Len)
				{
					End(W, B, false);
					return false;
				}
			}
			return true;
		case EDuty::Guard:
		{
			const float D = (W.Player.Feet - Nest.Pos).Len2D();
			if (R.Time > R.Len || !W.Player.bPresent || D > 50.f + S.Length * 30.f)
			{
				End(W, B, false);
				return false;
			}
			if (R.Step == 1)
			{
				B.Look = W.Player.Eye;
				B.bLook = true;
				const V3 To = (W.Player.Feet - B.Pos).Flat();
				B.Yaw = std::atan2(To.Y, To.X);
				R.SongTimer -= Dt;
				if (R.SongTimer <= 0.f)
				{
					R.SongTimer = 1.5f + W.Rand(B.Rng) * 2.5f;
					W.Emit(EEvent::Call, B);
					Emit(ENestEvent::Alarm, Nest.Id, B.Id, B.Pos);
				}
				if (B.ActTime > B.ActLen)
				{
					W.SetAct(B, EAct::Watch, 3.f + W.Rand(B.Rng) * 4.f);
				}
			}
			return true;
		}
		case EDuty::Dive:
			if (R.Time > R.Len || !W.Player.bPresent || (W.Player.Feet - Nest.Pos).Len2D() > 60.f)
			{
				End(W, B, false);
				W.FlyTo(B, Nest.Site, NestSeat(Nest, false));
				return false;
			}
			if (!IsAirborne(B.Act))
			{
				R.Step = 0;
				W.FlyTo(B, -1, W.Player.Eye);
			}
			else if (B.Act == EAct::Fly)
			{
				B.Act = EAct::Mob;           // (low over the player's head: the flight keeps a mobbing bird's height, not the cruising one)
				B.Target = -1;
			}
			return true;
		default:
			return true;
		}
	}

	bool FNesting::Arrive(FWorld& W, FBird& B)
	{
		if (B.Id >= (int)Role.size())
		{
			return false;
		}
		FRole& R = Role[B.Id];
		if (R.YoungOf >= 0 && R.bTied)
		{
			W.SetAct(B, EAct::Perch, 3.f + W.Rand(B.Rng) * 5.f);
			return true;
		}
		if (R.Duty == EDuty::None || R.Nest < 0)
		{
			return false;
		}
		FNest& Nest = Nests[R.Nest];
		const FBreedKind& K = KindOf(Nest);
		switch (R.Duty)
		{
		case EDuty::Sit:
			R.Step = 1;
			B.Pos = B.Spot;
			B.Yaw = IsHidden(K.Nest) ? Nest.Yaw + Pi : W.Rand(B.Rng) * 2.f * Pi;
			W.SetAct(B, Daytime(W, SpeciesTable()[B.Species]) ? EAct::Perch : EAct::Sleep, 20.f + W.Rand(B.Rng) * 40.f);
			return true;
		case EDuty::Gather:
			if (R.Step == 0)
			{
				R.Step = 1;
				W.SetAct(B, EAct::Peck, 2.f + W.Rand(B.Rng) * 4.f);
				return true;
			}
			if (R.Step == 2)
			{
				R.Step = 3;
				B.Yaw = std::atan2(Nest.Pos.Y - B.Pos.Y, Nest.Pos.X - B.Pos.X);
				W.SetAct(B, EAct::Preen, 4.f + W.Rand(B.Rng) * 8.f);       // weaving it in
				return true;
			}
			return false;
		case EDuty::Work:
			R.Step = 3;
			W.SetAct(B, EAct::Peck, 20.f + W.Rand(B.Rng) * 40.f);
			Emit(ENestEvent::Material, Nest.Id, B.Id, B.Pos);
			return true;
		case EDuty::Forage:
			if (R.Step == 0)
			{
				R.Step = 1;
				W.SetAct(B, EAct::Peck, std::max(KindOf(Nest).bPrecocial ? 120.f : 3.f, KindOf(Nest).FeedMinutes * 60.f * (0.2f + 0.3f * W.Rand(B.Rng))));
				return true;
			}
			if (R.Step == 2)
			{
				R.Step = 3;
				const V3 To = R.Target >= 0 ? W.Birds[R.Target].Pos : Nest.Pos;
				B.Yaw = std::atan2(To.Y - B.Pos.Y, To.X - B.Pos.X);
				W.SetAct(B, EAct::Peck, 1.5f + W.Rand(B.Rng) * 3.f);
				Emit(ENestEvent::Feed, Nest.Id, R.Target >= 0 ? R.Target : B.Id, Nest.Pos);
				if (R.Target >= 0)
				{
					Role[R.Target].Hunger = 0.f;
				}
				R.Target = -1;
				return true;
			}
			return false;
		case EDuty::Bring:
			R.Step = 3;
			W.SetAct(B, EAct::Peck, 2.f + W.Rand(B.Rng) * 2.f);
			Emit(ENestEvent::Feed, Nest.Id, Nest.Female, Nest.Pos);
			return true;
		case EDuty::Sing:
			R.Step = 1;
			R.SongTimer = 1.f;
			W.SetAct(B, EAct::Perch, 10.f + W.Rand(B.Rng) * 10.f);
			return true;
		case EDuty::Guard:
			R.Step = 1;
			R.SongTimer = 0.5f;
			W.SetAct(B, EAct::Watch, 3.f);
			return true;
		default:
			return false;
		}
	}

	void FNesting::FlightGoal(FWorld& W, FBird& B, V3& Goal, float& Speed, bool& bLanding)
	{
		if (B.Id >= (int)Role.size())
		{
			return;
		}
		FRole& R = Role[B.Id];
		if (R.Duty != EDuty::Dive || !W.Player.bPresent)
		{
			return;
		}
		const FSpecies& S = SpeciesTable()[B.Species];
		bLanding = false;
		if (R.Step == 0)
		{
			// at the head, fast, pulling up just over it
			Goal = W.Player.Eye + V3(0.f, 0.f, 0.6f + S.Length);
			Speed = S.Sprint;
			if ((B.Pos - Goal).Len() < 2.5f)
			{
				R.Step = 1;
				const V3 Away = (B.Vel.Flat().Len2D() > 0.1f ? B.Vel.Flat().Norm() : V3(1.f, 0.f, 0.f));
				R.Goal = W.Player.Eye + Away * (12.f + W.Rand(B.Rng) * 8.f) + V3(0.f, 0.f, 8.f + W.Rand(B.Rng) * 6.f);
				W.Emit(EEvent::Call, B);
				Emit(ENestEvent::Dive, R.Nest, B.Id, B.Pos);
			}
		}
		else
		{
			Goal = R.Goal;
			Speed = S.Cruise;
			if ((B.Pos - Goal).Len() < 3.f)
			{
				R.Step = 0;
			}
		}
		B.Spot = Goal;
	}

	void FNesting::OnFlush(FWorld& W, FBird& B, const V3& From, int Cause)
	{
		(void)From;
		(void)Cause;
		if (B.Id >= (int)Role.size())
		{
			return;
		}
		FRole& R = Role[B.Id];
		if (R.Duty == EDuty::Dive)
		{
			return;              // it is the one attacking
		}
		if (R.Duty != EDuty::None)
		{
			R.Carry = -1;        // (a corvid's stick falls: the corvids drop what they hold)
			R.Mat = EMaterial::None;
			R.Duty = EDuty::None;
			R.Step = 0;
			R.Next = W.Time + 20.0 + W.Rand(B.Rng) * 40.0;
		}
	}

	// --- fledglings ---------------------------------------------------------------------------------

	void FNesting::Fledge(FWorld& W, FNest& Nest, int Num)
	{
		const FBreedKind& K = KindOf(Nest);
		const int Parent = Nest.Female >= 0 ? Nest.Female : Nest.Male;
		if (Parent < 0)
		{
			return;
		}
		const int GI = W.Birds[Parent].Group;
		FGroup& G = W.Groups[GI];
		uint32_t R = Hash(Nest.Seed + (uint32_t)LastDay * 17u) | 1u;
		for (int I = 0; I < Num && !Pool.empty(); I++)
		{
			const int Id = Pool.back();
			Pool.pop_back();
			FBird& B = W.Birds[Id];
			const uint32_t Seed = Hash((uint32_t)Id * 2654435761u + Nest.Seed) | 1u;
			B = FBird();
			B.Id = Id;
			B.Species = Nest.Species;
			B.Group = GI;
			B.Rng = Seed;
			B.bFemale = Unit(R) < 0.5f;
			B.Boldness = 0.8f + 0.3f * Unit(R);
			// out round the nest: on the twigs about it, on the ground below (thrushes leave before they fly well), on the water
			const FSite& Site = W.Sites[Nest.Site];
			const float A = Unit(R) * 2.f * Pi, Dd = K.bPrecocial ? 0.4f + Unit(R) * 1.2f : 0.6f + Unit(R) * 3.f;
			B.Pos = Nest.Pos + V3(std::cos(A) * Dd, std::sin(A) * Dd, 0.f);
			if (K.bPrecocial || IsGroundHabitat(Site.Kind))
			{
				B.Pos.Z = Site.Kind == EHabitat::Sea ? W.SeaZ : std::max(W.GroundZ(B.Pos.X, B.Pos.Y), Nest.Pos.Z - 0.3f);
			}
			else if (IsTree(Site.Kind))
			{
				B.Pos.Z = Nest.Pos.Z + (Unit(R) - 0.4f) * 1.5f;
			}
			else
			{
				B.Pos.Z = Nest.Pos.Z + 0.2f;      // on the roof edge by the hole
			}
			B.Spot = B.Pos;
			B.Site = Nest.Site;
			B.Yaw = Unit(R) * 2.f * Pi;
			B.Act = EAct::Perch;
			B.ActLen = 2.f + Unit(R) * 4.f;
			B.ThinkTimer = Unit(R);
			B.Tier = 1;
			G.Members.push_back(Id);
			Role[Id] = FRole();
			Role[Id].YoungOf = Nest.Id;
			Role[Id].bTied = true;
			Role[Id].Index = I;
			Nest.Young.push_back(Id);
		}
	}

	void FNesting::ReleaseYoung(FWorld& W, FNest& Nest)
	{
		for (int Id : Nest.Young)
		{
			FBird& B = W.Birds[Id];
			if (B.Act == EAct::Claimed)
			{
				W.Release(Id);
			}
			if (W.Approacher == Id)
			{
				W.Approacher = -1;
			}
			std::vector<int>& Mem = W.Groups[B.Group].Members;
			Mem.erase(std::remove(Mem.begin(), Mem.end(), Id), Mem.end());
			B.bGone = true;
			B.Act = EAct::Perch;
			B.Vel = V3();
			Role[Id] = FRole();
			Pool.push_back(Id);
		}
		Nest.Young.clear();
	}

	bool FNesting::YoungThink(FWorld& W, FBird& B, FRole& R, float Dt)
	{
		FNest& Nest = Nests[R.YoungOf];
		const FBreedKind& K = KindOf(Nest);
		const float Out = Nest.State.Age - K.NestlingDays;     // days since it left the nest
		R.Hunger += Dt / std::max(20.f, K.FeedMinutes * 60.f);
		if (B.Act == EAct::Flee || (IsAirborne(B.Act) && B.Act != EAct::Fly && B.Act != EAct::Land))
		{
			return false;
		}
		const FSpecies& S = SpeciesTable()[B.Species];
		if (K.bPrecocial)
		{
			// a cygnet or a gull chick keeps by its mother (and at night under her)
			R.bTied = true;
			const int Mum = Nest.Female >= 0 && !W.Birds[Nest.Female].bGone ? Nest.Female : Nest.Male;
			if (Mum < 0)
			{
				return false;
			}
			const FBird& P = W.Birds[Mum];
			const float A = P.Yaw + Pi + ((float)R.Index - 2.f) * 0.45f;
			const float Behind = 0.6f + 0.25f * (float)R.Index + S.Length * 0.4f;
			V3 Goal = P.Pos + V3(std::cos(A) * Behind, std::sin(A) * Behind, 0.f);
			if (IsAirborne(P.Act))
			{
				return true;             // the mother flew: it waits where it is (she comes back)
			}
			const bool bWater = P.Act == EAct::Swim || (P.Site >= 0 && W.Sites[P.Site].Kind == EHabitat::Sea);
			Goal.Z = bWater ? W.SeaZ : std::max(W.GroundZ(Goal.X, Goal.Y), P.Pos.Z - 0.5f);
			const float D = (Goal - B.Pos).Len2D();
			if (P.Act == EAct::Sleep)
			{
				if (D > 0.3f)
				{
					B.Pos = Goal;
				}
				W.SetAct(B, EAct::Sleep, 30.f);
				return true;
			}
			if (D > 0.5f)
			{
				B.Site = P.Site;
				B.Spot = Goal;
				if (bWater)
				{
					// paddles after her
					const V3 To = (Goal - B.Pos).Flat();
					B.Yaw = std::atan2(To.Y, To.X);
					B.Vel = To.Norm() * std::min(0.8f, D);
					W.SetAct(B, EAct::Swim, 1.f);
				}
				else
				{
					W.SetAct(B, EAct::Forage, 0.f);
				}
			}
			else if (B.ActTime > B.ActLen)
			{
				W.SetAct(B, bWater ? EAct::Swim : (W.Rand(B.Rng) < 0.5f ? EAct::Peck : EAct::Perch), 1.f + W.Rand(B.Rng) * 3.f);
				B.Vel = V3();
			}
			return true;
		}
		// the first half of the time out of the nest: sits about near it and begs; then follows the family
		R.bTied = Out < K.DependDays * 0.5f;
		if (!R.bTied)
		{
			return false;
		}
		const float FromNest = (B.Pos - Nest.Pos).Len();
		if (FromNest > 25.f && !IsAirborne(B.Act))
		{
			const V3 Back = Nest.Pos + V3(W.Rand(B.Rng) * 4.f - 2.f, W.Rand(B.Rng) * 4.f - 2.f, W.Rand(B.Rng) * 1.f);
			W.FlyTo(B, Nest.Site, Back);
			return true;
		}
		if (IsAirborne(B.Act))
		{
			return true;
		}
		// a parent near: begs, wings shivering, louder the hungrier
		bool bParentNear = false;
		for (int Id : { Nest.Male, Nest.Female })
		{
			if (Id >= 0 && !W.Birds[Id].bGone && (W.Birds[Id].Pos - B.Pos).Len() < 8.f)
			{
				bParentNear = true;
			}
		}
		R.SongTimer -= Dt;
		if (R.SongTimer <= 0.f && (bParentNear || R.Hunger > 1.f))
		{
			R.SongTimer = (bParentNear ? 0.8f : 6.f) + W.Rand(B.Rng) * 2.f;
			Emit(ENestEvent::Beg, Nest.Id, B.Id, B.Pos);
			W.Emit(EEvent::Call, B);
		}
		if (B.ActTime > B.ActLen)
		{
			W.SetAct(B, W.Rand(B.Rng) < 0.2f ? EAct::Preen : EAct::Perch, 3.f + W.Rand(B.Rng) * 6.f);
			if (W.Rand(B.Rng) < 0.1f)
			{
				// a short flutter to another twig near by
				const V3 Hop = Nest.Pos + V3(W.Rand(B.Rng) * 5.f - 2.5f, W.Rand(B.Rng) * 5.f - 2.5f, (W.Rand(B.Rng) - 0.3f) * 1.5f);
				W.FlyTo(B, Nest.Site, Hop);
			}
		}
		return true;
	}

	void FNesting::Resettle(FWorld& W)
	{
		for (FBird& B : W.Birds)
		{
			if (B.Id >= 0 && B.Id < (int)Role.size() && Role[B.Id].Duty != EDuty::None)
			{
				Role[B.Id].Duty = EDuty::None;
				Role[B.Id].Carry = -1;
				Role[B.Id].Mat = EMaterial::None;
			}
		}
		for (FNest& Nest : Nests)
		{
			if (!Nest.Young.empty())
			{
				ReleaseYoung(W, Nest);
			}
			Update(W, Nest, W.DayOfYear, false);
		}
		LastDay = (int)W.DayOfYear;
		MinuteTimer = 0.f;
		YoungTimer = 0.f;
	}

	void FNesting::Near(const V3& P, float R, std::vector<int>& Out) const
	{
		Out.clear();
		for (const FNest& Nest : Nests)
		{
			if ((Nest.Pos - P).Len() < R)
			{
				Out.push_back(Nest.Id);
			}
		}
	}

	std::string FNesting::Report(const FWorld& W) const
	{
		int Stages[(int)EStage::Num] = {};
		for (const FNest& Nest : Nests)
		{
			Stages[(int)Nest.State.Stage]++;
		}
		int Duties[(int)EDuty::Num] = {};
		for (const FRole& R : Role)
		{
			Duties[(int)R.Duty]++;
		}
		std::string Out = "nests " + std::to_string(Nests.size()) + ":";
		for (int S = 1; S < (int)EStage::Num; S++)
		{
			if (Stages[S])
			{
				Out += std::string(" ") + StageName((EStage)S) + " " + std::to_string(Stages[S]);
			}
		}
		Out += ", duties:";
		for (int D = 1; D < (int)EDuty::Num; D++)
		{
			if (Duties[D])
			{
				Out += std::string(" ") + DutyName((EDuty)D) + " " + std::to_string(Duties[D]);
			}
		}
		Out += ", young out " + std::to_string(YoungOut());
		(void)W;
		return Out;
	}
}
