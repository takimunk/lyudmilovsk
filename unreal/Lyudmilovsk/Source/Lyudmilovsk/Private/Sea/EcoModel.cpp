#include "EcoModel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace LyudEco
{
namespace
{
	constexpr uint16_t AllYear = 0xFFF;
	constexpr uint16_t M(int A, int B)   // months A..B (1-based, wrapping)
	{
		uint16_t R = 0;
		for (int I = A;; I = I % 12 + 1)
		{
			R |= uint16_t(1u << (I - 1));
			if (I == B) break;
		}
		return R;
	}

	// The coastal Baltic off the Sambian peninsula (brackish ~7 psu, 0-20 m): biomass g/m2 wet weight, P/B and Q/B per
	// year. Rough, but in the ranges the Baltic Ecopath models give (Gulf of Riga, Puck Bay, the coastal Lithuanian and
	// Gdansk models: Tomczak et al. 2009, Sandberg et al. 2000, Harvey et al. 2003), turned to a shallow coastal box.
	// Diets: the stomach studies' broad picture: herring and sprat eat zooplankton (herring also mysids), cod on the coast
	// eat the isopod Saduria, shrimp, gobies and clupeids, round gobies crush mussels, flounders eat clams and mussels,
	// lumpsuckers eat jellyfish, eiders and long-tailed ducks winter here on the mussel beds, grey seals take herring, cod
	// and flatfish, cormorants gobies, perch and herring.
	const FNode Table[] = {
		// Id                  Kind               B       PB      QB     Months     Warm   Diet
		{ "Phyto",             EKind::Producer,   6.0f,   180.f,  0.f,   AllYear,   false, {} },
		{ "Cyano",             EKind::Producer,   1.5f,   60.f,   0.f,   AllYear,   false, {} },
		{ "Weed",              EKind::Producer,   60.f,   4.0f,   0.f,   AllYear,   false, {} },
		{ "Detritus",          EKind::Detritus,   100.f,  0.f,    0.f,   AllYear,   false, {} },
		{ "Zoo",               EKind::Consumer,   8.0f,   30.f,   80.f,  AllYear,   false, { { "Phyto", 0.66f }, { "Cyano", 0.04f }, { "Detritus", 0.3f } } },
		{ "Jelly",             EKind::Consumer,   3.0f,   10.f,   30.f,  M(6, 11),  false, { { "Zoo", 1.f } } },
		{ "Mysid",             EKind::Consumer,   3.5f,   8.f,    40.f,  AllYear,   false, { { "Phyto", 0.3f }, { "Zoo", 0.4f }, { "Detritus", 0.3f } } },
		{ "Gammarus",          EKind::Consumer,   4.0f,   6.f,    30.f,  AllYear,   false, { { "Weed", 0.4f }, { "Detritus", 0.6f } } },
		{ "Worms",             EKind::Consumer,   12.f,   2.5f,   15.f,  AllYear,   false, { { "Detritus", 1.f } } },
		{ "Macoma",            EKind::Consumer,   30.f,   1.2f,   8.f,   AllYear,   false, { { "Detritus", 0.7f }, { "Phyto", 0.3f } } },
		{ "Mussel",            EKind::Consumer,   60.f,   1.0f,   6.f,   AllYear,   false, { { "Phyto", 0.75f }, { "Cyano", 0.05f }, { "Detritus", 0.2f } } },
		{ "BrownShrimp",       EKind::Consumer,   1.5f,   4.f,    18.f,  AllYear,   false, { { "Worms", 0.3f }, { "Mysid", 0.2f }, { "Gammarus", 0.2f }, { "Macoma", 0.1f }, { "Detritus", 0.2f } } },
		{ "Saduria",           EKind::Consumer,   2.0f,   1.2f,   8.f,   AllYear,   false, { { "Macoma", 0.4f }, { "Worms", 0.3f }, { "Detritus", 0.3f } } },
		{ "Prawn",             EKind::Consumer,   0.8f,   3.f,    15.f,  AllYear,   false, { { "Gammarus", 0.12f }, { "Detritus", 0.43f }, { "Zoo", 0.15f }, { "Mysid", 0.1f }, { "Weed", 0.1f }, { "Worms", 0.1f } } },
		{ "MittenCrab",        EKind::Consumer,   0.05f,  0.6f,   4.f,   AllYear,   false, { { "Mussel", 0.2f }, { "Macoma", 0.2f }, { "Worms", 0.2f }, { "Gammarus", 0.1f }, { "Detritus", 0.3f } } },
		{ "MudCrab",           EKind::Consumer,   0.5f,   1.5f,   8.f,   AllYear,   false, { { "Mussel", 0.3f }, { "Detritus", 0.4f }, { "Gammarus", 0.3f } } },
		{ "Herring",           EKind::Consumer,   4.0f,   0.8f,   6.f,   AllYear,   false, { { "Zoo", 0.6f }, { "Mysid", 0.35f }, { "Gammarus", 0.05f } } },
		{ "Sprat",             EKind::Consumer,   3.0f,   1.0f,   8.f,   M(9, 5),   false, { { "Zoo", 0.95f }, { "Mysid", 0.05f } } },
		{ "SandEel",           EKind::Consumer,   1.2f,   1.2f,   9.f,   AllYear,   false, { { "Zoo", 0.9f }, { "Worms", 0.1f } } },
		{ "Stickleback",       EKind::Consumer,   0.9f,   2.f,    15.f,  AllYear,   false, { { "Zoo", 0.6f }, { "Gammarus", 0.2f }, { "Mysid", 0.2f } } },
		{ "NinespineStickleback", EKind::Consumer, 0.2f,  2.f,    15.f,  AllYear,   false, { { "Zoo", 0.6f }, { "Gammarus", 0.3f }, { "Worms", 0.1f } } },
		{ "Smelt",             EKind::Consumer,   0.8f,   1.0f,   6.f,   M(10, 5),  false, { { "Mysid", 0.4f }, { "Zoo", 0.3f }, { "Sprat", 0.1f }, { "Stickleback", 0.1f }, { "SandEel", 0.1f } } },
		{ "TwaiteShad",        EKind::Consumer,   0.15f,  0.6f,   5.f,   M(5, 7),   false, { { "Mysid", 0.4f }, { "Zoo", 0.3f }, { "Sprat", 0.2f }, { "Herring", 0.1f } } },
		{ "Whitefish",         EKind::Consumer,   0.3f,   0.5f,   4.f,   M(10, 5),  false, { { "Macoma", 0.3f }, { "Gammarus", 0.3f }, { "Mysid", 0.2f }, { "Worms", 0.2f } } },
		{ "Salmon",            EKind::Consumer,   0.08f,  0.5f,   5.f,   M(9, 12),  false, { { "Herring", 0.5f }, { "Sprat", 0.5f } } },
		{ "SeaTrout",          EKind::Consumer,   0.12f,  0.6f,   5.f,   AllYear,   false, { { "Herring", 0.3f }, { "Sprat", 0.25f }, { "Stickleback", 0.15f }, { "SandEel", 0.1f }, { "Gammarus", 0.1f }, { "RoundGoby", 0.1f } } },
		{ "Garfish",           EKind::Consumer,   0.15f,  0.7f,   6.f,   M(5, 9),   false, { { "Zoo", 0.3f }, { "Sprat", 0.2f }, { "Herring", 0.2f }, { "Stickleback", 0.2f }, { "SandEel", 0.1f } } },
		{ "Perch",             EKind::Consumer,   0.8f,   0.6f,   4.5f,  AllYear,   false, { { "RoundGoby", 0.3f }, { "Gammarus", 0.2f }, { "Stickleback", 0.15f }, { "BrownShrimp", 0.1f }, { "Mysid", 0.1f }, { "Smelt", 0.05f }, { "Roach", 0.05f }, { "SandGoby", 0.05f }, { "Prawn", 0.05f } } },
		{ "Roach",             EKind::Consumer,   1.0f,   0.6f,   5.f,   AllYear,   false, { { "Mussel", 0.35f }, { "Weed", 0.2f }, { "Gammarus", 0.2f }, { "Detritus", 0.15f }, { "Zoo", 0.1f } } },
		{ "Vimba",             EKind::Consumer,   0.3f,   0.5f,   4.f,   M(4, 11),  false, { { "Worms", 0.4f }, { "Macoma", 0.3f }, { "Gammarus", 0.2f }, { "Mussel", 0.1f } } },
		{ "Cod",               EKind::Consumer,   1.2f,   0.7f,   3.5f,  AllYear,   false, { { "Saduria", 0.15f }, { "Herring", 0.15f }, { "Sprat", 0.15f }, { "RoundGoby", 0.1f }, { "BrownShrimp", 0.1f }, { "Mysid", 0.05f }, { "Worms", 0.05f }, { "Macoma", 0.05f }, { "SandEel", 0.05f }, { "Flounder", 0.05f }, { "Eelpout", 0.03f }, { "Cod", 0.02f }, { "Gammarus", 0.05f }, { "Prawn", 0.03f } } },
		{ "Pike",              EKind::Consumer,   0.15f,  0.5f,   3.5f,  AllYear,   false, { { "Roach", 0.25f }, { "Perch", 0.15f }, { "Herring", 0.15f }, { "Stickleback", 0.15f }, { "RoundGoby", 0.15f }, { "NinespineStickleback", 0.05f }, { "Smelt", 0.05f }, { "Pike", 0.05f } } },
		{ "Eel",               EKind::Consumer,   0.1f,   0.2f,   3.f,   AllYear,   false, { { "BrownShrimp", 0.2f }, { "RoundGoby", 0.2f }, { "MudCrab", 0.1f }, { "Worms", 0.2f }, { "Gammarus", 0.1f }, { "Saduria", 0.1f }, { "Sculpin", 0.1f }, { "Prawn", 0.05f }, { "MittenCrab", 0.05f } } },
		{ "RoundGoby",         EKind::Consumer,   3.0f,   1.1f,   7.f,   AllYear,   false, { { "Mussel", 0.6f }, { "Macoma", 0.1f }, { "Gammarus", 0.15f }, { "Worms", 0.1f }, { "BrownShrimp", 0.05f } } },
		{ "SandGoby",          EKind::Consumer,   0.6f,   2.f,    12.f,  AllYear,   false, { { "Zoo", 0.2f }, { "Mysid", 0.2f }, { "Gammarus", 0.3f }, { "Worms", 0.2f }, { "BrownShrimp", 0.1f } } },
		{ "BlackGoby",         EKind::Consumer,   0.15f,  1.2f,   8.f,   AllYear,   false, { { "Gammarus", 0.4f }, { "Worms", 0.3f }, { "BrownShrimp", 0.15f }, { "Prawn", 0.05f }, { "Mysid", 0.1f } } },
		{ "RockGunnel",        EKind::Consumer,   0.1f,   1.0f,   6.f,   AllYear,   false, { { "Gammarus", 0.5f }, { "Worms", 0.3f }, { "Mysid", 0.2f } } },
		{ "Eelpout",           EKind::Consumer,   0.4f,   0.8f,   4.f,   AllYear,   false, { { "Gammarus", 0.3f }, { "Worms", 0.3f }, { "BrownShrimp", 0.2f }, { "Saduria", 0.1f }, { "Mussel", 0.1f } } },
		{ "Sculpin",           EKind::Consumer,   0.25f,  0.6f,   4.f,   AllYear,   false, { { "BrownShrimp", 0.3f }, { "Gammarus", 0.3f }, { "Saduria", 0.1f }, { "RoundGoby", 0.1f }, { "SandGoby", 0.1f }, { "Mysid", 0.1f }, { "Prawn", 0.1f } } },
		{ "FourhornSculpin",   EKind::Consumer,   0.2f,   0.5f,   3.5f,  AllYear,   false, { { "Saduria", 0.6f }, { "Macoma", 0.1f }, { "Gammarus", 0.1f }, { "Worms", 0.1f }, { "Mysid", 0.1f } } },
		{ "Lumpsucker",        EKind::Consumer,   0.1f,   0.4f,   4.f,   M(2, 6),   false, { { "Jelly", 0.4f }, { "Mysid", 0.3f }, { "Zoo", 0.2f }, { "Gammarus", 0.1f } } },
		{ "Flounder",          EKind::Consumer,   1.5f,   0.6f,   4.f,   AllYear,   false, { { "Macoma", 0.35f }, { "Mussel", 0.2f }, { "Worms", 0.2f }, { "BrownShrimp", 0.1f }, { "Gammarus", 0.1f }, { "Saduria", 0.05f } } },
		{ "Turbot",            EKind::Consumer,   0.2f,   0.5f,   4.f,   AllYear,   false, { { "SandEel", 0.25f }, { "Herring", 0.1f }, { "Sprat", 0.1f }, { "RoundGoby", 0.2f }, { "SandGoby", 0.15f }, { "BrownShrimp", 0.1f }, { "Mysid", 0.1f } } },
		{ "Pipefish",          EKind::Consumer,   0.05f,  1.f,    10.f,  M(5, 10),  false, { { "Zoo", 0.5f }, { "Mysid", 0.4f }, { "Gammarus", 0.1f } } },
		{ "StraightPipefish",  EKind::Consumer,   0.04f,  1.f,    10.f,  M(5, 10),  false, { { "Zoo", 0.6f }, { "Mysid", 0.3f }, { "Gammarus", 0.1f } } },
		{ "GreySeal",          EKind::Consumer,   0.06f,  0.08f,  13.f,  AllYear,   true,  { { "Herring", 0.3f }, { "Cod", 0.2f }, { "Flounder", 0.15f }, { "Sprat", 0.1f }, { "RoundGoby", 0.05f }, { "Eelpout", 0.05f }, { "Salmon", 0.03f }, { "SeaTrout", 0.03f }, { "Whitefish", 0.04f }, { "Smelt", 0.05f } } },
		{ "Porpoise",          EKind::Consumer,   0.004f, 0.1f,   25.f,  AllYear,   true,  { { "Herring", 0.35f }, { "Sprat", 0.3f }, { "Cod", 0.1f }, { "RoundGoby", 0.1f }, { "SandEel", 0.1f }, { "Smelt", 0.05f } } },
		{ "Cormorant",         EKind::Consumer,   0.02f,  0.2f,   70.f,  AllYear,   true,  { { "RoundGoby", 0.3f }, { "Perch", 0.1f }, { "Herring", 0.15f }, { "Sprat", 0.1f }, { "SandEel", 0.1f }, { "Stickleback", 0.1f }, { "Eelpout", 0.05f }, { "Flounder", 0.05f }, { "Smelt", 0.05f } } },
		{ "Eider",             EKind::Consumer,   0.05f,  0.2f,   60.f,  M(10, 4),  true,  { { "Mussel", 0.7f }, { "Macoma", 0.1f }, { "Gammarus", 0.1f }, { "BrownShrimp", 0.05f }, { "RoundGoby", 0.05f } } },
	};
	constexpr int NumTable = int(sizeof(Table) / sizeof(Table[0]));

	/** Vulnerability (Ecosim's K): 2 = mixed control; a little more top-down for the fish on the plankton. */
	constexpr float Vulnerability = 2.f;
	/** The share of food a consumer does not take in (to the detritus). */
	constexpr float Unassimilated = 0.2f;
	/** Days a migrant takes to come in or go. */
	constexpr float MigrationDays = 12.f;
	/** Per year: how fast the open sea's stock of a migrant pulls the part away back toward the base. */
	constexpr float OffshoreRelax = 1.f;

	inline float Clamp(float V, float A, float B) { return V < A ? A : (V > B ? B : V); }
	float Wrap(float D) { D = std::fmod(D, 365.f); return D < 0.f ? D + 365.f : D; }

	// the growth drive of the phytoplankton by day of the year (relative, normalised to a mean of 1 below): the diatoms
	// and dinoflagellates with the light and the nutrients (winter dark, the spring bloom in April, a thin summer when
	// the nutrients are spent, a small autumn bloom); the blue-greens (Nodularia, Aphanizomenon) only in the warm,
	// calm water of late July and August, the summer's bloom: barely eaten, they mostly sink to the detritus
	const float LightKeys[][2] = { { 0, 0.15f }, { 60, 0.25f }, { 90, 1.2f }, { 105, 3.4f }, { 125, 1.6f }, { 150, 0.7f },
		{ 185, 0.7f }, { 245, 0.8f }, { 270, 1.2f }, { 300, 0.5f }, { 335, 0.2f }, { 365, 0.15f } };
	const float CyanoKeys[][2] = { { 0, 0.03f }, { 150, 0.06f }, { 185, 0.4f }, { 205, 2.6f }, { 222, 4.2f }, { 240, 2.4f },
		{ 260, 0.5f }, { 290, 0.08f }, { 365, 0.03f } };

	template <int NK>
	float Keyed(const float (&Keys)[NK][2], float D)
	{
		for (int K = 1; K < NK; ++K)
			if (D <= Keys[K][0])
			{
				const float T = (D - Keys[K - 1][0]) / (Keys[K][0] - Keys[K - 1][0]);
				return Keys[K - 1][1] + (Keys[K][1] - Keys[K - 1][1]) * T;
			}
		return Keys[NK - 1][1];
	}
	float RawLight(float D) { return Keyed(LightKeys, D); }
	float RawCyano(float D) { return Keyed(CyanoKeys, D); }

	// the young of the year: the share of a group's yearly production that comes as a pulse (the day the young join it,
	// grown from the spawning months before: herring spawn on the weed in April-May, the young school inshore in July)
	struct FRecruit { const char* Id; float Day; float Share; };
	const FRecruit Recruits[] = {
		{ "Herring", 195.f, 0.5f }, { "Sprat", 210.f, 0.5f }, { "SandEel", 180.f, 0.4f }, { "Stickleback", 200.f, 0.5f },
		{ "NinespineStickleback", 200.f, 0.5f }, { "Smelt", 165.f, 0.4f }, { "Perch", 200.f, 0.5f }, { "Roach", 190.f, 0.5f },
		{ "Pike", 160.f, 0.5f }, { "Cod", 240.f, 0.4f }, { "RoundGoby", 215.f, 0.5f }, { "SandGoby", 200.f, 0.5f },
		{ "BlackGoby", 205.f, 0.4f }, { "Flounder", 210.f, 0.4f }, { "Turbot", 230.f, 0.4f }, { "Eelpout", 50.f, 0.5f },
		{ "BrownShrimp", 200.f, 0.4f }, { "Prawn", 210.f, 0.4f }, { "Gammarus", 160.f, 0.3f }, { "Mysid", 170.f, 0.3f },
		{ "Saduria", 100.f, 0.3f }, { "MudCrab", 215.f, 0.4f },
	};
	/** The young join over this many days either side of their day. */
	constexpr float RecruitDays = 30.f;
	/** Days from a node's recruitment day (-182.5..182.5). */
	float FromDay(float D, float Centre) { return Wrap(D - Centre + 182.5f) - 182.5f; }

	/** The water's temperature, deg C: ~1 in February, ~18 in mid August. */
	float WaterTemp(float D) { return 9.5f - 8.5f * std::cos(6.2831853f * (D - 40.f) / 365.f); }
	float RawWarmth(float D) { return std::pow(2.2f, (WaterTemp(D) - 9.5f) / 10.f); }

	float Mean(float (*Fn)(float))
	{
		double S = 0.0;
		for (int D = 0; D < 365; ++D) S += Fn(D + 0.5f);
		return float(S / 365.0);
	}

	/** In the coastal sea by the month of a day of the year (0..1, ramped over the month's edges). */
	float Presence(uint16_t Months, float D)
	{
		if (Months == AllYear) return 1.f;
		static const int Start[13] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };
		int Mo = 0;
		while (Mo < 11 && D >= Start[Mo + 1]) ++Mo;
		return (Months >> Mo) & 1u ? 1.f : 0.f;
	}

	uint64_t CellKey(float X, float Y)
	{
		const int32_t CX = int32_t(std::floor(X / 40.f)), CY = int32_t(std::floor(Y / 40.f));
		return uint64_t(uint32_t(CX)) << 32 | uint32_t(CY);
	}
}

int NumNodes() { return NumTable; }
const FNode& Node(int Index) { return Table[Index]; }
int Find(const char* Id)
{
	for (int I = 0; I < NumTable; ++I)
		if (std::strcmp(Table[I].Id, Id) == 0) return I;
	return -1;
}

float LightFactor(float D)
{
	static const float Norm = Mean(RawLight);
	return RawLight(Wrap(D)) / Norm;
}

float CyanoFactor(float D)
{
	static const float Norm = Mean(RawCyano);
	return RawCyano(Wrap(D)) / Norm;
}

float RecruitDay(int Node)
{
	for (const FRecruit& R : Recruits)
		if (std::strcmp(R.Id, Table[Node].Id) == 0) return R.Day;
	return -1.f;
}

float WarmthFactor(float D)
{
	static const float Norm = Mean(RawWarmth);
	return RawWarmth(Wrap(D)) / Norm;
}

FModel::FModel()
{
	N = NumTable;
	Balance();
	Reset(290.f);
}

void FModel::Balance()
{
	B0.assign(N, 0.f);
	M0.assign(N, 0.f);
	GE.assign(N, 0.f);
	BaseEE.assign(N, 0.f);
	Dc.assign(N * N, 0.f);
	A.assign(N * N, 0.f);
	V.assign(N * N, 0.f);
	LastQ.assign(N * N, 0.f);
	Pmax.assign(N, 0.f);
	Hp.assign(N, 0.f);
	RecDay.assign(N, -1.f);
	RecShare.assign(N, 0.f);
	for (const FRecruit& R : Recruits)
		if (const int I = Find(R.Id); I >= 0) { RecDay[I] = R.Day; RecShare[I] = R.Share; }
	IPhyto = Find("Phyto");
	ICyano = Find("Cyano");
	for (int J = 0; J < N; ++J)
	{
		const FNode& Nd = Table[J];
		B0[J] = Nd.B;
		float Sum = 0.f;
		for (const FDietItem& D : Nd.Diet) Sum += D.Share;
		for (const FDietItem& D : Nd.Diet)
		{
			const int I = Find(D.Prey);
			if (I >= 0 && Sum > 0.f) Dc[J * N + I] += D.Share / Sum;
		}
		if (Nd.Kind == EKind::Consumer) GE[J] = Nd.PB / Nd.QB;
	}
	// predation on each node at the base, its ecotrophic efficiency and other mortality
	std::vector<float> Eaten(N, 0.f);
	float Uneaten = 0.f;
	for (int J = 0; J < N; ++J)
	{
		if (Table[J].Kind != EKind::Consumer) continue;
		const float QJ = Table[J].QB * B0[J];
		Uneaten += Unassimilated * QJ;
		for (int I = 0; I < N; ++I) Eaten[I] += Dc[J * N + I] * QJ;
	}
	float DeadIn = Uneaten;
	for (int I = 0; I < N; ++I)
	{
		const FNode& Nd = Table[I];
		if (Nd.Kind == EKind::Detritus) continue;
		const float P = Nd.PB * B0[I];
		BaseEE[I] = P > 0.f ? Eaten[I] / P : 0.f;
		M0[I] = std::max(0.f, Nd.PB * (1.f - BaseEE[I]));
		DeadIn += M0[I] * B0[I];
		if (Nd.Kind == EKind::Producer)
		{
			// P(B) = Pmax B / (1 + B/B0): the base's production at the base, at most twice the rate when thin
			Pmax[I] = 2.f * Nd.PB;
			Hp[I] = 1.f / B0[I];
		}
	}
	// the detritus: what is not eaten is buried or carried off
	for (int I = 0; I < N; ++I)
		if (Table[I].Kind == EKind::Detritus)
		{
			BaseEE[I] = DeadIn > 0.f ? Eaten[I] / DeadIn : 0.f;
			DetExport = std::max(0.f, DeadIn - Eaten[I]) / B0[I];
		}
	// the foraging arenas: v = K Q0 / Bprey, a = 2 K Q0 / (Bprey Bpred (K - 1)), so the base's flows come out at the base
	for (int J = 0; J < N; ++J)
	{
		if (Table[J].Kind != EKind::Consumer) continue;
		for (int I = 0; I < N; ++I)
		{
			const float Q0 = Dc[J * N + I] * Table[J].QB * B0[J];
			if (Q0 <= 0.f) continue;
			V[J * N + I] = Vulnerability * Q0 / B0[I];
			A[J * N + I] = 2.f * Vulnerability * Q0 / (B0[I] * B0[J] * (Vulnerability - 1.f));
		}
	}
}

void FModel::Reset(float DayOfYear)
{
	B = B0;
	Young.assign(N, 0.f);
	F.assign(N, 1.f);
	for (int I = 0; I < N; ++I) F[I] = Presence(Table[I].Months, Wrap(DayOfYear));
	Cells.clear();
}

float FModel::Relative(int Node) const { return B0[Node] > 0.f ? B[Node] / B0[Node] : 1.f; }

void FModel::SetBiomass(int Node, float Value)
{
	if (Node < 0 || Node >= N || !std::isfinite(Value)) return;
	B[Node] = Clamp(Value, B0[Node] * 1e-3f, B0[Node] * 20.f);
}

void FModel::Remove(int Node, float Grams)
{
	if (Node < 0 || Node >= N || !std::isfinite(Grams) || Grams <= 0.f) return;
	B[Node] = std::max(B[Node] - Grams / AreaM2, B0[Node] * 1e-3f);
}

float FModel::LocalHeal(int Node) const
{
	// days to fill back in: the fish that stay put (gobies, sculpins, flatfish, eelpout) slowly, the roamers fast
	static const char* Slow[] = { "RoundGoby", "SandGoby", "BlackGoby", "Eelpout", "Sculpin", "FourhornSculpin", "Lumpsucker",
		"Flounder", "Turbot", "Pipefish", "StraightPipefish", "RockGunnel", "Eel", "BrownShrimp", "Saduria", "MudCrab", "Prawn",
		"MittenCrab", "Pike" };
	for (const char* S : Slow)
		if (std::strcmp(Table[Node].Id, S) == 0) return 20.f;
	return 3.f;
}

void FModel::TakeLocal(int Node, float X, float Y, float Fraction)
{
	if (Node < 0 || Node >= N || !std::isfinite(Fraction) || !std::isfinite(X) || !std::isfinite(Y)) return;
	FCell& C = Cells[CellKey(X, Y)];
	for (auto& T : C.Taken)
		if (T.first == Node)
		{
			T.second *= Clamp(1.f - Fraction, 0.f, 1.f);
			return;
		}
	C.Taken.push_back({ Node, Clamp(1.f - Fraction, 0.f, 1.f) });
}

float FModel::Local(int Node, float X, float Y) const
{
	auto It = Cells.find(CellKey(X, Y));
	if (It == Cells.end()) return 1.f;
	for (const auto& T : It->second.Taken)
		if (T.first == Node) return T.second;
	return 1.f;
}

void FModel::SubStep(float D, float DtDays)
{
	const float Dt = DtDays / 365.f;
	const float Light = LightFactor(D), Cyano = CyanoFactor(D), Warm = WarmthFactor(D);
	// the migrants come and go
	const float Ease = Clamp(DtDays / MigrationDays, 0.f, 1.f);
	for (int I = 0; I < N; ++I) F[I] += (Presence(Table[I].Months, D) - F[I]) * Ease;
	// scratch kept between steps (no allocations while it runs)
	std::vector<float>& C = ScratchC;
	std::vector<float>& Gain = ScratchGain;
	std::vector<float>& Loss = ScratchLoss;
	C.assign(N, 0.f);
	Gain.assign(N, 0.f);
	Loss.assign(N, 0.f);
	for (int I = 0; I < N; ++I) C[I] = B[I] * F[I];
	float DeadIn = 0.f;
	for (int J = 0; J < N; ++J)
	{
		if (Table[J].Kind != EKind::Consumer || C[J] <= 0.f) continue;
		const float W = Table[J].bWarm ? 1.f : Warm;
		float QJ = 0.f;
		for (int I = 0; I < N; ++I)
		{
			const int K = J * N + I;
			if (A[K] <= 0.f) continue;
			// the foraging arena: only the vulnerable part of the prey can be taken
			const float Q = W * A[K] * V[K] * C[I] * C[J] / (2.f * V[K] + A[K] * C[J]);
			LastQ[K] = Q / 365.f;
			Loss[I] += Q;
			QJ += Q;
		}
		// the young of the year: part of the growth goes to the eggs and larvae (out of sight, not eaten here), and they
		// join the group over the weeks round its recruitment day; the year's production is the same, only later
		const float Grown = GE[J] * QJ;
		if (RecShare[J] > 0.f)
		{
			Young[J] += Dt * RecShare[J] * Grown;
			const float Off = FromDay(D, RecDay[J]);
			const float Join = std::fabs(Off) <= RecruitDays ? Young[J] * Clamp(DtDays / (RecruitDays - Off + DtDays), 0.f, 1.f) : 0.f;
			Young[J] -= Join;
			Gain[J] += (1.f - RecShare[J]) * Grown + Join / Dt;
		}
		else Gain[J] += Grown;
		DeadIn += Unassimilated * QJ;
	}
	for (int I = 0; I < N; ++I)
	{
		const FNode& Nd = Table[I];
		if (Nd.Kind == EKind::Producer)
		{
			const float Drive = I == IPhyto ? Light : I == ICyano ? Cyano : 0.4f + 0.6f * Warm;
			Gain[I] += Drive * Pmax[I] * C[I] / (1.f + Hp[I] * C[I]);
		}
		if (Nd.Kind != EKind::Detritus)
		{
			Loss[I] += M0[I] * C[I];
			DeadIn += M0[I] * C[I];
		}
	}
	for (int I = 0; I < N; ++I)
		if (Table[I].Kind == EKind::Detritus)
		{
			Gain[I] += DeadIn;
			Loss[I] += DetExport * B[I];
		}
	// the part away offshore (or up a river) belongs to the open Baltic's stock: it drifts back toward the base in a year
	for (int I = 0; I < N; ++I)
		if (F[I] < 1.f) Gain[I] += (1.f - F[I]) * (B0[I] - B[I]) * OffshoreRelax;
	// semi-implicit: the losses scale with the biomass, so nothing goes below zero however long the step
	for (int I = 0; I < N; ++I)
	{
		const float Old = B[I];
		float New = Old > 1e-9f ? (Old + Dt * Gain[I]) / (1.f + Dt * Loss[I] / Old) : Old + Dt * Gain[I];
		if (!std::isfinite(New)) New = B0[I];
		B[I] = Clamp(New, B0[I] * 1e-3f, B0[I] * 20.f);
	}
	// the local thinning heals
	for (auto It = Cells.begin(); It != Cells.end();)
	{
		auto& T = It->second.Taken;
		for (auto& E : T) E.second += (1.f - E.second) * Clamp(DtDays / LocalHeal(E.first), 0.f, 1.f);
		T.erase(std::remove_if(T.begin(), T.end(), [](const std::pair<int, float>& E) { return E.second > 0.99f; }), T.end());
		It = T.empty() ? Cells.erase(It) : std::next(It);
	}
}

void FModel::Step(float DayOfYear, float Days)
{
	// quarter-day steps (the plankton turn over in days)
	float D = DayOfYear;
	while (Days > 1e-6f)
	{
		const float H = std::min(Days, 0.25f);
		SubStep(Wrap(D), H);
		D += H;
		Days -= H;
	}
}

void FModel::Save(std::vector<std::string>& Keys, std::vector<float>& Values) const
{
	Keys.clear();
	Values.clear();
	for (int I = 0; I < N; ++I)
	{
		// against the base, so a retuned table keeps the saved state's meaning
		Keys.push_back(std::string(Table[I].Id) + "@rel");
		Values.push_back(B[I] / B0[I]);
		Keys.push_back(std::string(Table[I].Id) + "@here");
		Values.push_back(F[I]);
		if (RecShare[I] > 0.f)
		{
			Keys.push_back(std::string(Table[I].Id) + "@young");
			Values.push_back(Young[I] / B0[I]);
		}
	}
	for (const auto& C : Cells)
		for (const auto& T : C.second.Taken)
		{
			char Buf[96];
			std::snprintf(Buf, sizeof(Buf), "%s@%d,%d", Table[T.first].Id, int32_t(uint32_t(C.first >> 32)), int32_t(uint32_t(C.first)));
			Keys.push_back(Buf);
			Values.push_back(T.second);
		}
}

void FModel::Load(const std::vector<std::string>& Keys, const std::vector<float>& Values, float DayOfYear)
{
	Cells.clear();
	std::vector<char> HasYoung(N, 0);
	for (size_t K = 0; K < Keys.size() && K < Values.size(); ++K)
	{
		const std::string& Key = Keys[K];
		const size_t At = Key.find('@');
		std::string Id = Key.substr(0, At);
		if (Id == "FishBirds") Id = "Cormorant";   // early saves' names
		else if (Id == "SeaDucks") Id = "Eider";
		const int I = Find(Id.c_str());
		if (I < 0 || !std::isfinite(Values[K])) continue;
		if (At == std::string::npos) B[I] = Clamp(Values[K], B0[I] * 1e-3f, B0[I] * 20.f);   // early saves: g/m2
		else if (Key.compare(At, std::string::npos, "@rel") == 0) B[I] = Clamp(Values[K] * B0[I], B0[I] * 1e-3f, B0[I] * 20.f);
		else if (Key.compare(At, std::string::npos, "@here") == 0) F[I] = Clamp(Values[K], 0.f, 1.f);
		else if (Key.compare(At, std::string::npos, "@young") == 0) { Young[I] = Clamp(Values[K], 0.f, 20.f) * B0[I]; HasYoung[I] = 1; }
		else
		{
			const char* P = Key.c_str() + At + 1;
			char* End = nullptr;
			const long CX = std::strtol(P, &End, 10);
			if (End && *End == ',')
			{
				const long CY = std::strtol(End + 1, nullptr, 10);
				Cells[uint64_t(uint32_t(int32_t(CX))) << 32 | uint32_t(int32_t(CY))].Taken.push_back({ I, Clamp(Values[K], 0.f, 1.f) });
			}
		}
	}
	// an older save (before the young were kept): the pool its share of the production would have grown since the
	// group's last recruitment (none mid-recruitment: that part has joined)
	if (DayOfYear >= 0.f)
		for (int I = 0; I < N; ++I)
			if (RecShare[I] > 0.f && !HasYoung[I])
			{
				const float Since = Wrap(DayOfYear - RecDay[I] - RecruitDays);
				const float Off = FromDay(DayOfYear, RecDay[I]);
				const float Grown = std::fabs(Off) <= RecruitDays ? (RecruitDays - Off) / (2.f * RecruitDays) * (365.f - 2.f * RecruitDays) : Since;
				Young[I] = RecShare[I] * Table[I].PB * B[I] * F[I] * Grown / 365.f;
			}
}

std::string FModel::Report() const
{
	std::string Out;
	char Buf[256];
	for (int I = 0; I < N; ++I)
	{
		int Top = -1;
		float TopQ = 0.f;
		for (int J = 0; J < N; ++J)
			if (LastQ[J * N + I] > TopQ) { TopQ = LastQ[J * N + I]; Top = J; }
		std::snprintf(Buf, sizeof(Buf), "%-20s %8.3f g/m2 %5.0f%% of base, %3.0f%% here%s%s\n", Table[I].Id, B[I], 100.f * Relative(I),
			100.f * F[I], Top >= 0 ? ", eaten most by " : "", Top >= 0 ? Table[Top].Id : "");
		Out += Buf;
	}
	return Out;
}
}
