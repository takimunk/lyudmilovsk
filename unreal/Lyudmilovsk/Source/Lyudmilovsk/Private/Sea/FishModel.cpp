#include "FishModel.h"

#include <algorithm>
#include <cstring>

namespace LyudFish
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

	// Baltic fish of the Sambian coast's shallow sea (brackish, ~7 psu), their numbers and ways. Behaviour notes:
	// herring and sprat in autumn come close inshore in big schools; sand eels school over clean sand and bury in it
	// for the winter (and when chased); sticklebacks shoal in the weed in summer and move out in autumn; perch shoal by
	// reefs and eelgrass, curious; cod hunt over the stones, more at night; sea trout cruise the coast, most in autumn;
	// garfish come in May to spawn on the weed and leave in September; round gobies (invasive, everywhere now) perch on
	// stones, sand gobies on the sand; eelpout and sculpin lurk among stones; lumpsuckers come to the shallow stones in
	// spring to spawn; flounders on sand, turbot on sand and gravel near stones; pipefish in the eelgrass in summer.
	const FSpecies Table[] = {
		// Id            Way           Len   Cruise Burst DMin  DMax  OBMin OBMax Sand Stone Weed Abund Gmin Gmax Months       Day   Night Shy  Prey  Waves Bend Burrow
		{ "Herring",     EWay::School,  0.20f, 0.45f, 1.6f, 3.0f, 30.f, 1.0f, 6.0f, 1.f, 0.7f, 0.6f, 0.05f, 40, 110, AllYear,  1.f,  0.6f, 2.0f, 0.f,  0.9f, 0.6f, false },
		{ "Sprat",       EWay::School,  0.11f, 0.35f, 1.2f, 4.0f, 30.f, 1.5f, 8.0f, 1.f, 0.6f, 0.5f, 0.04f, 60, 160, M(9, 5),  1.f,  0.6f, 1.8f, 0.f,  0.9f, 0.6f, false },
		{ "SandEel",     EWay::School,  0.16f, 0.35f, 1.4f, 1.5f, 15.f, 0.3f, 2.5f, 1.f, 0.f,  0.1f, 0.04f, 25, 70,  AllYear,  1.f,  0.f,  1.5f, 0.f,  1.6f, 0.9f, true  },
		{ "Stickleback", EWay::Shoal,   0.06f, 0.12f, 0.6f, 0.5f, 6.f,  0.1f, 1.2f, 0.4f, 0.4f, 1.f, 0.05f, 6,  25,  AllYear,  1.f,  0.3f, 0.8f, 0.f,  1.f,  0.7f, false },
		{ "Perch",       EWay::Shoal,   0.22f, 0.25f, 1.3f, 1.5f, 10.f, 0.3f, 2.5f, 0.3f, 0.9f, 0.9f, 0.025f, 5, 16,  AllYear,  1.f,  0.2f, 2.2f, 0.07f, 0.9f, 0.5f, false },
		{ "Cod",         EWay::Cruiser, 0.50f, 0.30f, 1.8f, 6.0f, 30.f, 0.3f, 2.0f, 0.4f, 1.f,  0.4f, 0.018f, 1, 3,   AllYear,  0.6f, 1.f,  3.0f, 0.22f, 0.9f, 0.5f, false },
		{ "SeaTrout",    EWay::Cruiser, 0.55f, 0.55f, 3.0f, 2.0f, 30.f, 1.0f, 5.0f, 0.8f, 0.6f, 0.6f, 0.008f, 1, 2,  AllYear,  1.f,  0.5f, 4.0f, 0.2f,  0.9f, 0.45f, false },
		{ "Garfish",     EWay::Surface, 0.60f, 0.6f,  3.0f, 2.0f, 30.f, -1.f, -1.f, 0.8f, 0.6f, 0.8f, 0.012f, 3, 9,  M(5, 9),  1.f,  0.3f, 3.0f, 0.f,  1.2f, 0.6f, false },
		{ "RoundGoby",   EWay::Hopper,  0.15f, 0.2f,  0.9f, 0.5f, 20.f, 0.f,  0.f,  0.1f, 1.f,  0.3f, 1.2f, 1,  1,   AllYear,  1.f,  0.4f, 0.9f, 0.f,  0.9f, 0.5f, false },
		{ "SandGoby",    EWay::Hopper,  0.065f, 0.1f, 0.7f, 0.3f, 15.f, 0.f,  0.f,  1.f,  0.f,  0.2f, 0.9f, 1,  1,   AllYear,  1.f,  0.4f, 0.6f, 0.f,  0.9f, 0.5f, false },
		{ "Eelpout",     EWay::Lurker,  0.28f, 0.08f, 0.5f, 1.0f, 30.f, 0.f,  0.f,  0.f,  1.f,  0.6f, 0.15f, 1, 1,   AllYear,  0.5f, 1.f,  0.7f, 0.f,  1.8f, 1.f,  false },
		{ "Sculpin",     EWay::Lurker,  0.22f, 0.1f,  0.9f, 1.0f, 30.f, 0.f,  0.f,  0.1f, 1.f,  0.7f, 0.12f, 1, 1,   AllYear,  0.8f, 1.f,  0.6f, 0.f,  0.8f, 0.5f, false },
		{ "Lumpsucker",  EWay::Lurker,  0.30f, 0.05f, 0.4f, 2.0f, 30.f, 0.f,  0.f,  0.f,  1.f,  0.4f, 0.03f, 1, 1,   M(2, 6),  1.f,  1.f,  0.5f, 0.f,  0.7f, 0.3f, false },
		{ "Flounder",    EWay::Flat,    0.30f, 0.1f,  1.2f, 0.5f, 30.f, 0.f,  0.f,  1.f,  0.1f, 0.3f, 0.25f, 1, 1,   AllYear,  0.8f, 1.f,  1.5f, 0.f,  0.5f, 0.3f, false },
		{ "Turbot",      EWay::Flat,    0.42f, 0.1f,  1.3f, 2.0f, 30.f, 0.f,  0.f,  0.8f, 0.6f, 0.2f, 0.04f, 1, 1,   AllYear,  0.8f, 1.f,  1.6f, 0.f,  0.5f, 0.3f, false },
		{ "Pipefish",    EWay::Hover,   0.22f, 0.03f, 0.2f, 1.0f, 8.f,  0.1f, 0.6f, 0.f,  0.f,  1.f,  0.35f, 1, 1,   M(5, 10), 1.f,  0.6f, 0.4f, 0.f,  1.f,  0.4f, false },
		// the second wave (2026-11): more fish, the bed's crustaceans, the mysids, the grey seal (docs/ecosystem.md).
		// Smelt school inshore from autumn to their spring spawning; twaite shad pass in May-July; whitefish come to the
		// coast for the winter; salmon in autumn on their way to the rivers; roach and vimba (the coastal brackish stock)
		// by weed and over sand; eels and rock gunnels hide among stones by day; straightnose pipefish in the wrack; the
		// fourhorn sculpin (a glacial relict) on the cold deep sand; black gobies in the eelgrass; ninespine sticklebacks
		// in the weed. Brown shrimp on the sand (buried by day), the isopod Saduria on the deeper sand, Harris mud crabs
		// (invasive) and amphipods among stones and weed; mysids drift in clouds low over the bed; grey seals hunt the
		// coast, come up to breathe every minute or two and come to look at a diver.
		// Id                     Way            Len    Cruise Burst DMin  DMax  OBMin OBMax Sand Stone Weed Abund  Gmin Gmax Months    Day   Night Shy   Prey   Waves Bend  Burrow
		{ "Smelt",                EWay::School,  0.18f, 0.40f, 1.5f, 2.0f, 20.f, 1.0f, 6.0f, 0.8f, 0.5f, 0.5f, 0.02f,  30, 90,  M(10, 5), 1.f,  0.5f, 1.8f, 0.05f, 0.9f, 0.6f, false },
		{ "TwaiteShad",           EWay::School,  0.38f, 0.60f, 2.2f, 3.0f, 30.f, 1.5f, 8.0f, 1.f,  0.6f, 0.5f, 0.006f, 8,  25,  M(5, 7),  1.f,  0.5f, 2.5f, 0.06f, 0.9f, 0.55f, false },
		{ "Whitefish",            EWay::Shoal,   0.40f, 0.35f, 1.8f, 2.0f, 25.f, 0.5f, 3.0f, 0.8f, 0.5f, 0.5f, 0.006f, 3,  10,  M(10, 5), 0.8f, 0.8f, 2.5f, 0.f,   0.9f, 0.5f, false },
		{ "Salmon",               EWay::Cruiser, 0.75f, 0.70f, 3.5f, 3.0f, 30.f, 1.5f, 6.0f, 0.7f, 0.6f, 0.5f, 0.003f, 1,  1,   M(9, 12), 1.f,  0.5f, 4.5f, 0.2f,  0.9f, 0.45f, false },
		{ "Roach",                EWay::Shoal,   0.22f, 0.25f, 1.3f, 0.5f, 8.f,  0.2f, 2.0f, 0.5f, 0.7f, 1.f,  0.03f,  6,  20,  AllYear,  1.f,  0.3f, 1.8f, 0.f,   0.9f, 0.55f, false },
		{ "Vimba",                EWay::Shoal,   0.30f, 0.30f, 1.4f, 3.0f, 20.f, 0.2f, 1.2f, 1.f,  0.4f, 0.3f, 0.01f,  4,  14,  M(4, 11), 0.7f, 1.f,  2.0f, 0.f,   0.9f, 0.5f, false },
		{ "Eel",                  EWay::Lurker,  0.65f, 0.25f, 1.0f, 1.0f, 25.f, 0.f,  0.f,  0.3f, 1.f,  0.8f, 0.02f,  1,  1,   AllYear,  0.2f, 1.f,  0.8f, 0.f,   2.2f, 1.f,  false },
		{ "RockGunnel",           EWay::Lurker,  0.20f, 0.10f, 0.6f, 0.5f, 15.f, 0.f,  0.f,  0.1f, 1.f,  0.9f, 0.1f,   1,  1,   AllYear,  0.6f, 0.8f, 0.4f, 0.f,   2.0f, 1.f,  false },
		{ "StraightPipefish",     EWay::Hover,   0.25f, 0.02f, 0.15f, 0.5f, 6.f, 0.1f, 0.5f, 0.f,  0.3f, 1.f,  0.25f,  1,  1,   M(5, 10), 1.f,  0.6f, 0.3f, 0.f,   1.f,  0.4f, false },
		{ "FourhornSculpin",      EWay::Lurker,  0.25f, 0.08f, 0.8f, 8.0f, 30.f, 0.f,  0.f,  0.7f, 0.4f, 0.f,  0.05f,  1,  1,   AllYear,  0.6f, 1.f,  0.6f, 0.f,   0.8f, 0.5f, false },
		{ "BlackGoby",            EWay::Hopper,  0.12f, 0.15f, 0.8f, 1.0f, 20.f, 0.f,  0.f,  0.6f, 0.3f, 1.f,  0.3f,   1,  1,   AllYear,  1.f,  0.4f, 0.7f, 0.f,   0.9f, 0.5f, false },
		{ "NinespineStickleback", EWay::Shoal,   0.065f, 0.10f, 0.5f, 0.6f, 5.f, 0.1f, 1.0f, 0.2f, 0.3f, 1.f,  0.03f,  4,  15,  AllYear,  1.f,  0.3f, 0.7f, 0.f,   1.f,  0.7f, false },
		{ "BrownShrimp",          EWay::Crawler, 0.06f, 0.05f, 0.6f, 0.3f, 15.f, 0.f,  0.f,  1.f,  0.f,  0.2f, 1.5f,   1,  1,   AllYear,  0.3f, 1.f,  0.5f, 0.f,   0.5f, 0.2f, true  },
		{ "Saduria",              EWay::Crawler, 0.07f, 0.03f, 0.2f, 5.0f, 30.f, 0.f,  0.f,  1.f,  0.2f, 0.f,  0.25f,  1,  1,   AllYear,  0.5f, 1.f,  0.3f, 0.f,   0.5f, 0.1f, false },
		{ "MudCrab",              EWay::Crawler, 0.025f, 0.03f, 0.15f, 0.5f, 10.f, 0.f, 0.f,  0.1f, 1.f,  0.4f, 0.6f,   1,  1,   AllYear,  0.4f, 1.f,  0.3f, 0.f,   0.5f, 0.05f, false },
		{ "Gammarus",             EWay::Crawler, 0.018f, 0.04f, 0.25f, 0.3f, 10.f, 0.f, 0.f,  0.1f, 0.8f, 1.f,  3.0f,   1,  1,   AllYear,  0.6f, 1.f,  0.2f, 0.f,   0.8f, 0.3f, false },
		{ "Mysid",                EWay::Swarm,   0.015f, 0.05f, 0.4f, 0.5f, 20.f, 0.1f, 1.0f, 0.8f, 0.6f, 0.6f, 0.03f,  80, 250, AllYear,  0.6f, 1.f,  0.4f, 0.f,   0.8f, 0.3f, false },
		{ "GreySeal",             EWay::Cruiser, 2.0f,  1.2f,  5.0f, 2.0f, 30.f, 0.5f, 8.0f, 0.7f, 0.8f, 0.4f, 0.0015f, 1, 1,   AllYear,  1.f,  1.f,  2.5f, 0.6f,  0.5f, 0.35f, false, true, true },
	};
	constexpr int NumTable = int(sizeof(Table) / sizeof(Table[0]));

	inline float Clamp(float V, float A, float B) { return V < A ? A : (V > B ? B : V); }
	inline float Smooth(float E0, float E1, float V)
	{
		const float T = Clamp((V - E0) / (E1 - E0), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}
	inline uint32_t Hash(uint32_t A, uint32_t B, uint32_t C)
	{
		uint32_t H = A * 0x8da6b343u ^ B * 0xd8163841u ^ C * 0xcb1ab31fu;
		H = (H ^ (H >> 15)) * 0x2c1b3c6du;
		H = (H ^ (H >> 12)) * 0x297a2d39u;
		return H ^ (H >> 15);
	}
	struct FCellRng
	{
		uint32_t S;
		float F() { S = S * 1664525u + 1013904223u; return float(S >> 8) / 16777216.f; }
	};
	inline float WrapPi(float A)
	{
		while (A > 3.14159265f) A -= 6.2831853f;
		while (A < -3.14159265f) A += 6.2831853f;
		return A;
	}

	constexpr float CellSize = 6.f;

	/** How well a spot suits a species, 0..1. */
	float Suits(const FSpecies& S, float Sand, float Stone, float Weed, float Depth)
	{
		if (Depth < S.DepthMin || Depth > S.DepthMax) return 0.f;
		const float Edge = Smooth(S.DepthMin, S.DepthMin + 1.f, Depth) * (1.f - Smooth(S.DepthMax - 2.f, S.DepthMax, Depth));
		return Clamp(std::max({ Sand * S.Sand, Stone * S.Stone, Weed * S.Weed }), 0.f, 1.f) * Edge;
	}

	/** Turns a fish toward a velocity (yaw/pitch eased at a turn rate), returns the yaw rate. */
	float Face(FFish& F, const FV& V, float Dt, float Rate)
	{
		if (V.Len() < 1e-3f) return 0.f;
		const float Yaw = std::atan2(V.Y, V.X);
		const float Pitch = Clamp(std::atan2(V.Z, std::max(V.Len2D(), 1e-3f)), -0.9f, 0.9f);
		const float DY = Clamp(WrapPi(Yaw - F.Yaw), -Rate * Dt, Rate * Dt);
		F.Yaw = WrapPi(F.Yaw + DY);
		F.Pitch += Clamp(Pitch - F.Pitch, -Rate * Dt, Rate * Dt);
		return Dt > 0.f ? DY / Dt : 0.f;
	}

	/** Advances the body wave for a speed through the water (a tail beat moves a fish ~0.7 of its length). */
	void Beat(FFish& F, float Speed, float Dt, float TurnRate)
	{
		const float Freq = Clamp(Speed / (0.7f * F.Len), 0.7f, 14.f);
		F.Phase = std::fmod(F.Phase + 6.2831853f * Freq * Dt, 6.2831853f * 64.f);
		F.Beat += (Clamp(0.25f + Speed / (F.Len * 4.f), 0.2f, 1.f) - F.Beat) * Clamp(Dt * 4.f, 0.f, 1.f);
		F.Bend += (Clamp(TurnRate * 0.35f, -1.f, 1.f) - F.Bend) * Clamp(Dt * 6.f, 0.f, 1.f);
	}
}

int NumSpecies() { return NumTable; }
float Suitability(int S, float Sand, float Stone, float Weed, float Depth) { return Suits(Table[S], Sand, Stone, Weed, Depth); }
float Activity(int S, const FClock& Clock)
{
	const FSpecies& Sp = Table[S];
	if (!(Sp.Months & (1u << ((Clock.Month - 1) % 12)))) return 0.f;
	return Sp.Day * Clock.Daylight + Sp.Night * (1.f - Clock.Daylight);
}
const FSpecies& Species(int Index) { return Table[Index]; }
int FindSpecies(const char* Id)
{
	for (int I = 0; I < NumTable; ++I)
		if (std::strcmp(Table[I].Id, Id) == 0) return I;
	return -1;
}

float FSim::Rand()
{
	Rng = Rng * 1664525u + 1013904223u;
	return float(Rng >> 8) / 16777216.f;
}

void FSim::Clear()
{
	All.clear();
	Groups.clear();
	Cells.clear();
}

int FSim::Count(int S) const
{
	int N = 0;
	for (const FFish& F : All) N += F.Species == S;
	return N;
}

bool FSim::Nearest(int S, const FV& From, FV& Out) const
{
	float Best = 1e30f;
	for (const FFish& F : All)
	{
		if (F.Species != S || F.bHidden) continue;
		const float D = (F.Pos - From).Len();
		if (D < Best) { Best = D; Out = F.Pos; }
	}
	return Best < 1e29f;
}

float FSim::Season(int S, const FClock& Clock) const
{
	const FSpecies& Sp = Table[S];
	if (!(Sp.Months & (1u << ((Clock.Month - 1) % 12)))) return 0.f;
	return Sp.Day * Clock.Daylight + Sp.Night * (1.f - Clock.Daylight);
}

void FSim::Settle(const FEnv& Env, const FV& Eye, const FClock& Clock)
{
	// bottom fish: each 6 m cell of the bed in reach has its own, by its habitat (the same fish every visit)
	const int CX = int(std::floor(Eye.X / CellSize)), CY = int(std::floor(Eye.Y / CellSize));
	const int Reach = int(std::ceil(Radius / CellSize));
	std::unordered_map<uint64_t, int> Keep;
	const float Water = Env.SurfaceZ();
	for (int Y = CY - Reach; Y <= CY + Reach; ++Y)
		for (int X = CX - Reach; X <= CX + Reach; ++X)
		{
			const float MX = (X + 0.5f) * CellSize - Eye.X, MY = (Y + 0.5f) * CellSize - Eye.Y;
			if (MX * MX + MY * MY > (Radius + CellSize) * (Radius + CellSize)) continue;
			const uint64_t Key = (uint64_t(uint32_t(X)) << 32 | uint32_t(Y)) | (1ull << 63);   // never 0 (0: a roamer)
			auto It = Cells.find(Key);
			if (It != Cells.end())
			{
				Keep[Key] = 1;
				continue;
			}
			Keep[Key] = 1;
			FCellRng R{ Hash(uint32_t(X), uint32_t(Y), Seed) };
			for (int S = 0; S < NumTable; ++S)
			{
				const FSpecies& Sp = Table[S];
				if (!IsSettled(Sp.Way)) continue;
				const float Here = Season(S, Clock) * Density;
				// try a few spots in the cell; each holds a fish with the chance its habitat gives
				const int Tries = int(std::ceil(Sp.Abundance * CellSize * CellSize / 100.f * 2.f));
				for (int T = 0; T < Tries; ++T)
				{
					const float PX = (X + R.F()) * CellSize, PY = (Y + R.F()) * CellSize;
					const float Roll = R.F(), Size = 0.75f + 0.5f * R.F(), Turn = R.F() * 6.2831853f;
					const uint32_t FishSeed = Hash(uint32_t(X), uint32_t(Y), uint32_t(S * 64 + T));
					float Bz;
					if (!Env.Bed(PX, PY, Bz)) continue;
					float Sand, Stone, Weed;
					Env.Habitat(PX, PY, Sand, Stone, Weed);
					const float P = Suits(Sp, Sand, Stone, Weed, Water - Bz) * Here * 0.5f * Env.Abundance(S, PX, PY);
					if (Roll >= P) continue;
					FFish F;
					F.Species = uint8_t(S);
					F.Len = Sp.Length * Size;
					F.Pos = F.Goal = FV(PX, PY, Bz);
					F.Yaw = Turn - 3.14159265f;
					F.Seed = FishSeed;
					F.Cell = Key;
					F.Timer = 2.f + float(FishSeed % 1000) * 0.01f;
					F.Phase = float(FishSeed % 628) * 0.01f;
					All.push_back(F);
				}
			}
		}
	// cells out of reach let their fish go
	for (auto It = Cells.begin(); It != Cells.end();)
		It = Keep.count(It->first) ? std::next(It) : Cells.erase(It);
	for (const auto& K : Keep) Cells[K.first] = 1;
	All.erase(std::remove_if(All.begin(), All.end(), [&](const FFish& F) { return F.Cell && !Cells.count(F.Cell); }), All.end());
}

void FSim::SpawnGroup(int S, const FEnv& Env, const FV& Eye, const FClock& Clock)
{
	const FSpecies& Sp = Table[S];
	const float Water = Env.SurfaceZ();
	for (int Try = 0; Try < 6; ++Try)
	{
		const float A = Rand() * 6.2831853f, D = SpawnMin + (Radius - SpawnMin) * Rand();
		const float X = Eye.X + std::cos(A) * D, Y = Eye.Y + std::sin(A) * D;
		float Bz;
		if (!Env.Bed(X, Y, Bz)) continue;
		const float Depth = Water - Bz;
		if (Depth < Sp.DepthMin || Depth > Sp.DepthMax) continue;
		float Sand, Stone, Weed;
		Env.Habitat(X, Y, Sand, Stone, Weed);
		if (Suits(Sp, Sand, Stone, Weed, Depth) < 0.2f * Rand()) continue;
		FGroup G;
		G.Species = S;
		G.bAlive = true;
		const float Over = Sp.OverBedMin < 0.f ? Depth - 0.5f : Sp.OverBedMin + (Sp.OverBedMax - Sp.OverBedMin) * Rand();
		G.Centre = FV(X, Y, std::min(Bz + Over, Water - 0.4f));
		// swims across the eye's area, not straight at it
		G.Heading = A + 3.14159265f + (Rand() - 0.5f) * 2.2f;
		G.Goal = G.Centre;
		G.Hunger = 10.f + 30.f * Rand();
		G.Breath = 30.f + 90.f * Rand();
		// a night school is looser and fewer stay together
		const int N = Sp.GroupMin + int((Sp.GroupMax - Sp.GroupMin) * Rand() * (0.5f + 0.5f * Clock.Daylight));
		const int Gi = int(Groups.size());
		Groups.push_back(G);
		for (int I = 0; I < std::max(N, 1); ++I)
		{
			FFish F;
			F.Species = uint8_t(S);
			F.Group = Gi;
			F.Len = Sp.Length * (0.82f + 0.36f * Rand());
			// a place in the formation: a school is longer along its way, flat-ish; a shoal looser
			// a mysid cloud is wide and flat over the bed
			const float Sz = Sp.Way == EWay::School ? 0.5f + std::sqrt(float(N)) * Sp.Length * 1.1f
				: Sp.Way == EWay::Swarm ? 0.6f + std::sqrt(float(N)) * 0.07f : 0.6f + std::sqrt(float(N)) * Sp.Length * 2.2f;
			F.Slot = FV((Rand() - 0.5f) * 2.f * Sz * 1.4f, (Rand() - 0.5f) * 2.f * Sz,
				(Rand() - 0.5f) * Sz * (Sp.Way == EWay::Surface ? 0.2f : Sp.Way == EWay::Swarm ? 0.3f : 0.7f));
			F.Pos = G.Centre + F.Slot;
			F.Yaw = G.Heading;
			F.Seed = Hash(uint32_t(Gi), uint32_t(I), uint32_t(Time * 10.0));
			F.Phase = Rand() * 6.28f;
			All.push_back(F);
		}
		Groups[Gi].Members = std::max(N, 1);
		return;
	}
}

void FSim::Roam(const FEnv& Env, const FV& Eye, const FClock& Clock, float Dt)
{
	// how many groups of each species belong round the eye: abundance over the reach, by the habitat under the eye
	float BedZ;
	const bool bSea = Env.Bed(Eye.X, Eye.Y, BedZ);
	float Sand = 0.f, Stone = 0.f, Weed = 0.f;
	if (bSea) Env.Habitat(Eye.X, Eye.Y, Sand, Stone, Weed);
	// the habitat round about is mixed: blur the eye's spot toward a little of everything
	Sand = 0.5f + 0.5f * Sand;
	Stone = 0.25f + 0.75f * Stone;
	Weed = 0.25f + 0.75f * Weed;
	const float Depth = bSea ? Env.SurfaceZ() - BedZ : 0.f;
	const float Area = 3.14159265f * Radius * Radius / 100.f;
	for (int S = 0; S < NumTable; ++S)
	{
		const FSpecies& Sp = Table[S];
		if (IsSettled(Sp.Way)) continue;
		int Have = 0;
		for (const FGroup& G : Groups) Have += G.bAlive && G.Species == S;
		const float Want = bSea ? Sp.Abundance * Area * Suits(Sp, Sand, Stone, Weed, std::max(Depth, Sp.DepthMin)) * Season(S, Clock) * Density
			* Env.Abundance(S, Eye.X, Eye.Y) : 0.f;
		// a new group comes in now and then while there are fewer than the habitat holds
		if (float(Have) < Want && Rand() < Dt * 0.5f * (Want - Have + 0.3f)) SpawnGroup(S, Env, Eye, Clock);
	}
	// groups that wandered off are gone
	for (int G = 0; G < int(Groups.size()); ++G)
	{
		FGroup& Gr = Groups[G];
		if (Gr.bAlive && (Gr.Centre - Eye).Len2D() > Radius * 1.3f)
		{
			Gr.bAlive = false;
		}
	}
	All.erase(std::remove_if(All.begin(), All.end(), [&](const FFish& F) { return F.Group >= 0 && !Groups[F.Group].bAlive; }), All.end());
	// compact the group list when it is mostly dead
	int Alive = 0;
	for (const FGroup& G : Groups) Alive += G.bAlive;
	if (Groups.size() > 16 && Alive * 2 < int(Groups.size()))
	{
		std::vector<int> Map(Groups.size(), -1);
		std::vector<FGroup> Kept;
		for (int G = 0; G < int(Groups.size()); ++G)
			if (Groups[G].bAlive) { Map[G] = int(Kept.size()); Kept.push_back(Groups[G]); }
		for (FGroup& G : Kept) G.Prey = G.Prey >= 0 ? Map[G.Prey] : -1;
		for (FFish& F : All) if (F.Group >= 0) F.Group = Map[F.Group];
		Groups.swap(Kept);
	}
}

void FSim::SetDiet(int Predator, int Prey, float Share)
{
	if (Predator < 0 || Prey < 0 || Predator >= NumTable || Prey >= NumTable) return;
	if (Diet.empty()) Diet.assign(NumTable * NumTable, 0.f);
	Diet[Predator * NumTable + Prey] = Share;
}

bool FSim::Eats(int Predator, int Prey) const
{
	if (Diet.empty()) return Predator != Prey && Table[Prey].Length <= Table[Predator].PreyLength;
	return Diet[Predator * NumTable + Prey] > 0.f;
}

FFish* FSim::FindSeed(uint32_t FishSeed)
{
	for (FFish& F : All)
		if (F.Seed == FishSeed && F.Group < 0 && !F.bEaten) return &F;
	return nullptr;
}

bool FSim::Take(int S, const FV& At, float Reach, FFish* Out)
{
	int Best = -1;
	float BestD = Reach;
	for (int I = 0; I < int(All.size()); ++I)
	{
		const FFish& F = All[I];
		if (F.Species != S || F.bHidden || F.bEaten) continue;
		const float D = (F.Pos - At).Len();
		if (D < BestD) { BestD = D; Best = I; }
	}
	if (Best < 0) return false;
	if (Out) *Out = All[Best];
	if (All[Best].Group >= 0) Groups[All[Best].Group].Members--;
	All.erase(All.begin() + Best);
	return true;
}

void FSim::Hunt(FGroup& G, int Gi, float Dt, FV& Want, float& Speed)
{
	const FSpecies& Sp = Table[G.Species];
	// after a while hungry, pick the nearest school it eats within 12 m and chase it; failing that, something on the bed
	G.Hunger -= Dt;
	if (G.Prey >= 0 && (G.Prey >= int(Groups.size()) || !Groups[G.Prey].bAlive)) G.Prey = -1;
	FFish* Bed = G.BedPrey ? FindSeed(G.BedPrey) : nullptr;
	if (G.BedPrey && (!Bed || Bed->bHidden)) { G.BedPrey = 0; Bed = nullptr; }
	if (G.Hunger <= 0.f && G.Prey < 0 && !Bed)
	{
		float Best = 12.f;
		for (int O = 0; O < int(Groups.size()); ++O)
		{
			const FGroup& P = Groups[O];
			if (!P.bAlive || O == Gi || P.Members <= 0 || Table[P.Species].Length > Sp.PreyLength || !Eats(G.Species, P.Species)) continue;
			const float D = (P.Centre - G.Centre).Len();
			if (D < Best) { Best = D; G.Prey = O; }
		}
		if (G.Prey < 0)
		{
			float BestBed = 10.f;
			for (FFish& F : All)
			{
				if (F.Group >= 0 || F.bHidden || F.bEaten || F.Len > Sp.PreyLength * 1.2f || !Eats(G.Species, F.Species)) continue;
				const float D = (F.Pos - G.Centre).Len();
				if (D < BestBed) { BestBed = D; Bed = &F; }
			}
			if (Bed) G.BedPrey = Bed->Seed;
		}
		if (G.Prey < 0 && !Bed) G.Hunger = 3.f + 4.f * Rand();   // nothing about: look again in a few seconds
	}
	const bool bSeal = Sp.bBreathes;
	if (G.Prey >= 0)
	{
		FGroup& P = Groups[G.Prey];
		const FV To = P.Centre - G.Centre;
		Want = To.Norm();
		Speed = Sp.Burst * 0.8f;
		if (To.Len() < 1.2f + Sp.Length * 0.5f)
		{
			// the strike: the school bursts away; now and then one is caught
			P.Alarm = 1.f;
			P.From = G.Centre;
			G.Prey = -1;
			G.Hunger = 20.f + 40.f * Rand();
			if (Rand() < (bSeal ? 0.6f : 0.35f))
			{
				FFish* Caught = nullptr;
				float Near = 1.5f + Sp.Length;
				for (FFish& F : All)
				{
					if (F.Group != int(&P - Groups.data()) || F.bHidden || F.bEaten) continue;
					const float D = (F.Pos - G.Centre).Len();
					if (D < Near) { Near = D; Caught = &F; }
				}
				if (Caught)
				{
					Caught->bEaten = true;
					P.Members--;
					Events.push_back({ G.Species, Caught->Species, Caught->Pos });
					G.Hunger = 40.f + 80.f * Rand();
				}
			}
		}
		else if (To.Len() > 16.f)
		{
			G.Prey = -1;
			G.Hunger = 8.f;
		}
	}
	else if (Bed)
	{
		// down onto the bed: a cod noses for a shrimp or a goby, a seal for a flounder
		const FV To = (Bed->Pos + FV(0.f, 0.f, 0.15f + Sp.Length * 0.1f)) - G.Centre;
		Want = To.Norm();
		Speed = std::min(Sp.Burst * 0.6f, std::max(Sp.Cruise, To.Len() * 1.5f));
		if (To.Len() < 0.5f + Sp.Length * 0.4f)
		{
			if (Rand() < 0.5f)
			{
				Bed->bEaten = true;
				Events.push_back({ G.Species, Bed->Species, Bed->Pos });
				G.Hunger = 30.f + 60.f * Rand();
			}
			else
			{
				// missed: it bolts (a shrimp into the sand)
				Bed->Timer = -1.f;
				Bed->State = 3;
				G.Hunger = 6.f + 10.f * Rand();
			}
			G.BedPrey = 0;
		}
		else if (To.Len() > 14.f) G.BedPrey = 0;
	}
	// a hunter on the hunt scares what it eats near it (cod scatter from a seal, a school from a cod)
	if (G.Hunger <= 0.f || G.Prey >= 0 || G.BedPrey)
		for (FGroup& P : Groups)
		{
			if (!P.bAlive || &P == &G || !Eats(G.Species, P.Species)) continue;
			const float D = (P.Centre - G.Centre).Len();
			if (D < 6.f) { P.Alarm = std::max(P.Alarm, 0.5f * (1.f - D / 6.f) + 0.2f); P.From = G.Centre; }
		}
}

void FSim::Ambush(FFish& F, const FEnv& Env)
{
	// a sit-and-wait snap at something small that comes within reach (a sculpin at a shrimp, a goby at an amphipod,
	// a perch at a goby, herring through a mysid cloud)
	if (F.bHidden || F.bEaten || F.Fed > 0.f) return;
	const FSpecies& Sp = Table[F.Species];
	const float Reach = 0.25f + F.Len * 1.5f;
	FFish* Best = nullptr;
	float BestD = Reach;
	for (FFish& O : All)
	{
		if (&O == &F || O.bHidden || O.bEaten || O.Len > F.Len * 0.6f || !Eats(F.Species, O.Species)) continue;
		const float D = (O.Pos - F.Pos).Len();
		if (D < BestD) { BestD = D; Best = &O; }
	}
	if (!Best) return;
	F.Fed = 8.f + 20.f * Rand();
	if (Rand() < 0.4f)
	{
		Best->bEaten = true;
		if (Best->Group >= 0) Groups[Best->Group].Members--;
		Events.push_back({ int(F.Species), int(Best->Species), Best->Pos });
		// the lunge: a settled fish jumps to where its prey was
		if (F.Group < 0 && Sp.Way != EWay::Hover)
		{
			float Bz;
			if (Env.Bed(Best->Pos.X, Best->Pos.Y, Bz)) { F.Goal = FV(Best->Pos.X, Best->Pos.Y, Bz); F.State = 2; }
		}
	}
	else if (Best->Group < 0) { Best->Timer = -1.f; Best->State = 3; }
	else Groups[Best->Group].Alarm = std::max(Groups[Best->Group].Alarm, 0.6f), Groups[Best->Group].From = F.Pos;
}

void FSim::StepGroup(int Gi, float Dt, const FEnv& Env, const FV& Eye)
{
	FGroup& G = Groups[Gi];
	const FSpecies& Sp = Table[G.Species];
	const float Water = Env.SurfaceZ();
	G.Timer -= Dt;
	G.Alarm = std::max(0.f, G.Alarm - Dt * 0.25f);
	// a diver close by alarms the group a little; a hunter's strike bolts it
	const float ToEye = (G.Centre - Eye).Len();
	if (ToEye < Sp.Shy * 2.f + 1.f && Sp.Way != EWay::Cruiser)
	{
		G.Alarm = std::max(G.Alarm, 0.4f * (1.f - ToEye / (Sp.Shy * 2.f + 1.f)));
		G.From = Eye;
	}
	float Speed = Sp.Cruise;
	FV Want(std::cos(G.Heading), std::sin(G.Heading), 0.f);
	if (Sp.Way == EWay::Cruiser)
	{
		Hunt(G, Gi, Dt, Want, Speed);
		const bool bHunting = G.Prey >= 0 || G.BedPrey != 0;
		if (Sp.bCurious && !bHunting && ToEye < 14.f && G.Look >= 0.f)
		{
			// a seal comes to look: in to a few metres, round the diver a while, then off about its business
			G.Look += Dt;
			if (G.Look > 40.f) G.Look = -90.f;
			const FV To = Eye - G.Centre;
			const FV Round = FV(-To.Y, To.X, 0.f).Norm() * (Gi % 2 ? 1.f : -1.f);
			const float Gap = To.Len() - (Sp.Shy + 1.5f);
			Want = (Round + To.Norm() * Clamp(Gap * 0.5f, -1.f, 1.f)).Norm();
			Speed = Sp.Cruise * (0.6f + 0.4f * Clamp(std::fabs(Gap) * 0.3f, 0.f, 1.f));
		}
		else if (G.Look < 0.f) G.Look = std::min(0.f, G.Look + Dt);
		// a hunter keeps its distance from a diver, a cod turns to look
		if (ToEye < Sp.Shy)
		{
			Want = (G.Centre - Eye).Norm();
			Speed = Sp.Cruise * 2.f;
		}
	}
	else if (Sp.Way == EWay::Swarm)
	{
		// a mysid cloud barely swims: it drifts with the water and holds its place over the bed
		Speed = Sp.Cruise * 0.4f;
	}
	if (G.Alarm > 0.f)
	{
		const FV Away = (G.Centre - G.From);
		Want = (Want * (1.f - G.Alarm) + Away.Norm() * (G.Alarm * 2.f)).Norm();
		Speed = Sp.Cruise + (Sp.Burst - Sp.Cruise) * G.Alarm;
	}
	// wander: the heading drifts; turn back from water too shallow or too deep for it
	if (G.Timer <= 0.f)
	{
		G.Timer = 3.f + 6.f * Rand();
		G.Goal = FV((Rand() - 0.5f) * 1.6f, 0.f, 0.f);
	}
	G.Heading += G.Goal.X * Dt * 0.35f;
	const FV Ahead = G.Centre + FV(std::cos(G.Heading), std::sin(G.Heading), 0.f) * 4.f;
	float Bz;
	const bool bAhead = Env.Bed(Ahead.X, Ahead.Y, Bz);
	if (!bAhead || Water - Bz < Sp.DepthMin || Water - Bz > Sp.DepthMax) G.Heading += Dt * 1.2f;
	// keep to its height over the bed (or just under the surface)
	float Here;
	if (Env.Bed(G.Centre.X, G.Centre.Y, Here))
	{
		const float Depth = Water - Here;
		float Lo = Here + std::max(Sp.OverBedMin, 0.2f), Hi = std::min(Here + Sp.OverBedMax, Water - 0.4f);
		if (Sp.OverBedMin < 0.f) { Lo = std::max(Water - 1.2f, Here + 0.3f); Hi = Water - 0.3f; }
		if (Depth < 1.f) Hi = Lo = Here + Depth * 0.5f;
		const float Z = Clamp(G.Centre.Z, Lo, std::max(Lo, Hi));
		Want.Z += Clamp((Z - G.Centre.Z) * 0.6f, -0.6f, 0.6f);
		if (G.Alarm <= 0.f && Sp.Way != EWay::Cruiser) Want.Z += std::sin(float(Time) * 0.13f + Gi) * 0.05f;
	}
	if (Sp.bBreathes)
	{
		// up for air every minute or two, a few breaths at the surface, then down again
		if (G.AtSurface > 0.f)
		{
			G.AtSurface -= Dt;
			Want.Z = Clamp((Water - 0.35f - G.Centre.Z) * 2.f, -0.5f, 0.5f);
			Speed = Sp.Cruise * 0.4f;
			if (G.AtSurface <= 0.f) G.Breath = 60.f + 120.f * Rand();
		}
		else if ((G.Breath -= Dt) <= 0.f)
		{
			Want = FV(Want.X * 0.4f, Want.Y * 0.4f, 1.f);
			Speed = Sp.Cruise;
			if (G.Centre.Z > Water - 0.7f) G.AtSurface = 4.f + 5.f * Rand();
		}
	}
	const FV Target = Want.Norm() * Speed + Env.Current(G.Centre) * (Sp.Way == EWay::Swarm ? 0.9f : 0.5f);
	G.Vel += (Target - G.Vel).Clamped(Dt * (0.6f + 2.5f * G.Alarm));
	G.Centre += G.Vel * Dt;
	G.Centre.Z = std::min(G.Centre.Z, Water - std::max(0.25f, Sp.Length * 0.15f));
	if (G.Vel.Len2D() > 0.05f) G.Heading += WrapPi(std::atan2(G.Vel.Y, G.Vel.X) - G.Heading) * Clamp(Dt * 0.8f, 0.f, 1.f);
}

void FSim::StepFish(FFish& F, float Dt, const FEnv& Env, const FV& Eye)
{
	const FSpecies& Sp = Table[F.Species];
	const float Water = Env.SurfaceZ();
	const FV Cur = Env.Current(F.Pos);
	if (F.Group >= 0)
	{
		const FGroup& G = Groups[F.Group];
		// sand eels dive into the sand when the school bolts over sand, and come out a while later
		if (Sp.bBurrows)
		{
			if (F.bHidden)
			{
				F.Timer -= Dt;
				if (F.Timer <= 0.f && G.Alarm < 0.2f) F.bHidden = false;
				else return;
			}
			else if (G.Alarm > 0.7f && Rand() < Dt * 3.f)
			{
				float Bz;
				if (Env.Bed(F.Pos.X, F.Pos.Y, Bz) && F.Pos.Z - Bz < 3.f)
				{
					F.bHidden = true;
					F.Timer = 20.f + 40.f * Rand();
					F.Pos.Z = Bz;
					return;
				}
			}
		}
		// keep the place in the formation (turned with the group, spread when alarmed or at night), dodge the diver
		const float Spread = 1.f + G.Alarm * 1.8f;
		const float C = std::cos(G.Heading), S = std::sin(G.Heading);
		const FV Slot(F.Slot.X * C - F.Slot.Y * S, F.Slot.X * S + F.Slot.Y * C, F.Slot.Z);
		FV Want = (G.Centre + Slot * Spread - F.Pos) * 1.4f + G.Vel;
		const FV FromEye = F.Pos - Eye;
		const float DE = FromEye.Len();
		const float Shy = Sp.Shy * (Sp.Way == EWay::Shoal ? 0.8f : 1.f);
		if (DE < Shy) Want += FromEye.Norm() * (Sp.Burst * (1.f - DE / Shy) * 2.f);
		if (G.Alarm > 0.5f) Want += (F.Pos - G.From).Norm() * (Sp.Burst * G.Alarm);
		// a little of each fish's own wander; a mysid jitters all the time
		Want += FV(std::sin(float(Time) * 0.7f + F.Seed % 97), std::cos(float(Time) * 0.53f + F.Seed % 89), 0.f) * (Sp.Cruise * 0.15f);
		if (Sp.Way == EWay::Swarm)
			Want += FV(std::sin(float(Time) * 3.1f + F.Seed % 53), std::cos(float(Time) * 2.7f + F.Seed % 41), std::sin(float(Time) * 2.3f + F.Seed % 37) * 0.5f) * (Sp.Cruise * 2.f);
		const FV Rel = (F.Vel - Cur * 0.5f);
		FV NewRel = Rel + (Want.Clamped(Sp.Burst) - Rel).Clamped(Dt * (2.f + 6.f * G.Alarm) * Sp.Burst);
		F.Vel = NewRel + Cur * 0.5f;
		F.Pos += F.Vel * Dt;
		float Bz;
		if (Env.Bed(F.Pos.X, F.Pos.Y, Bz)) F.Pos.Z = std::max(F.Pos.Z, Bz + F.Len * 0.4f);
		F.Pos.Z = std::min(F.Pos.Z, Water - 0.15f);
		const float Turn = Face(F, NewRel.Len() > 0.04f ? NewRel : FV(std::cos(G.Heading), std::sin(G.Heading), 0.f), Dt, 4.f + 6.f * G.Alarm);
		Beat(F, NewRel.Len(), Dt, Turn);
		F.Roll = Clamp(-Turn * 0.08f, -0.5f, 0.5f);
		return;
	}

	// bottom fish: rest, now and then a hop to a spot nearby, a dart away when the diver comes close
	float Bz = F.Pos.Z;
	Env.Bed(F.Pos.X, F.Pos.Y, Bz);
	const FV FromEye = F.Pos - Eye;
	const float DE = FromEye.Len();
	F.Timer -= Dt;
	enum { Rest = 0, Move = 1, Flee = 2, Startled = 3 };
	const bool bCrawler = Sp.Way == EWay::Crawler;
	if (F.bHidden)
	{
		// buried in the sand (a shrimp by day, or after a fright): out again a while later, when nothing is close
		F.Timer -= Dt;
		if (F.Timer <= 0.f && DE > Sp.Shy * 3.f) { F.bHidden = false; F.State = Rest; F.Timer = 2.f; }
		return;
	}
	if (F.State == Startled || (Sp.bBurrows && F.State != Flee && DE < Sp.Shy))
	{
		// a hunter missed it (or a diver is on it): a burrower digs in, the rest dart off
		if (Sp.bBurrows)
		{
			F.bHidden = true;
			F.Timer = 20.f + 40.f * Rand();
			F.State = Rest;
			F.Pos.Z = Bz;
			return;
		}
		const float A = Rand() * 6.2831853f;
		const float Dist = bCrawler ? 0.3f + 0.5f * Rand() : 1.f + 1.5f * Rand();
		float Tz;
		if (Env.Bed(F.Pos.X + std::cos(A) * Dist, F.Pos.Y + std::sin(A) * Dist, Tz))
		{
			F.Goal = FV(F.Pos.X + std::cos(A) * Dist, F.Pos.Y + std::sin(A) * Dist, Tz);
			F.State = Flee;
		}
		else F.State = Rest;
	}
	if (F.State != Flee && DE < Sp.Shy && Sp.Way != EWay::Hover)
	{
		// off, away from the diver and a little aside, for a few body lengths (a flatfish glides metres)
		const float A = std::atan2(FromEye.Y, FromEye.X) + (Rand() - 0.5f) * 1.2f;
		const float Dist = Sp.Way == EWay::Flat ? 2.5f + 2.5f * Rand() : Sp.Way == EWay::Lurker ? 0.6f + 0.6f * Rand()
			: bCrawler ? 0.3f + 0.4f * Rand() : 1.f + 1.f * Rand();
		const FV To(F.Pos.X + std::cos(A) * Dist, F.Pos.Y + std::sin(A) * Dist, 0.f);
		float Tz;
		if (Env.Bed(To.X, To.Y, Tz))
		{
			F.Goal = FV(To.X, To.Y, Tz);
			F.State = Flee;
		}
	}
	else if (F.State == Rest && F.Timer <= 0.f)
	{
		const float Reach = Sp.Way == EWay::Hopper ? 0.2f + 0.5f * Rand() : Sp.Way == EWay::Hover ? 0.3f * Rand()
			: bCrawler ? 0.08f + 0.35f * Rand() : 0.3f + 1.2f * Rand();
		const float A = Rand() * 6.2831853f;
		float Tz;
		const FV To(F.Pos.X + std::cos(A) * Reach, F.Pos.Y + std::sin(A) * Reach, 0.f);
		if (Sp.Way != EWay::Flat && Sp.Abundance > 0.f && Env.Bed(To.X, To.Y, Tz) && !(Sp.Way == EWay::Lurker && Sp.Cruise < 0.06f))
		{
			float Sand, Stone, Weed;
			Env.Habitat(To.X, To.Y, Sand, Stone, Weed);
			if (Suits(Sp, Sand, Stone, Weed, Water - Tz) > 0.1f)
			{
				F.Goal = FV(To.X, To.Y, Tz);
				F.State = Move;
			}
		}
		F.Timer = Sp.Way == EWay::Hopper ? 2.f + 6.f * Rand() : bCrawler ? 1.f + 4.f * Rand() : 10.f + 30.f * Rand();
	}
	const float Lift = Sp.Way == EWay::Hover ? 0.25f + 0.35f * float(F.Seed % 100) * 0.01f : Sp.Way == EWay::Flat ? 0.f : bCrawler ? F.Len * 0.12f : F.Len * 0.1f;
	if (F.State == Move || F.State == Flee)
	{
		const FV To = F.Goal - F.Pos;
		const float Left = To.Len2D();
		const float V = F.State == Flee ? Sp.Burst : bCrawler ? Sp.Cruise : std::max(Sp.Cruise, 0.12f);
		const float Step = std::min(V * Dt, Left);
		const FV Dir = FV(To.X, To.Y, 0.f).Norm();
		// over the bed in a low arc
		const float Arc = (bCrawler ? (F.State == Flee ? 0.06f : 0.f) : F.State == Flee ? (Sp.Way == EWay::Flat ? 0.35f : 0.15f) : 0.06f)
			* std::sin(3.14159f * Clamp(Left / 1.5f, 0.f, 1.f));
		F.Pos = FV(F.Pos.X + Dir.X * Step, F.Pos.Y + Dir.Y * Step, 0.f);
		float Nz = Bz;
		Env.Bed(F.Pos.X, F.Pos.Y, Nz);
		F.Pos.Z = Nz + Lift + Arc;
		const float Turn = Face(F, Dir, Dt, 8.f);
		F.Pitch = 0.f;
		Beat(F, V, Dt, Turn);
		F.Vel = Dir * V;
		if (Left < 0.02f)
		{
			F.State = Rest;
			F.Timer = Sp.Way == EWay::Flat ? 30.f + 60.f * Rand() : bCrawler ? 1.f + 3.f * Rand() : 2.f + 5.f * Rand();
		}
		return;
	}
	// at rest: face into the current a little at a time, fan the fins; the pipefish stands up in the eelgrass
	F.Pos.Z = Bz + Lift;
	F.Vel = FV();
	if (Sp.Way == EWay::Hover)
	{
		F.Pitch = 1.15f + 0.15f * std::sin(float(Time) * 0.4f + F.Seed % 31);
		F.Pos += Cur * (0.6f * std::sin(float(Time) * 0.9f + F.Seed % 17));
	}
	else
	{
		F.Pitch = 0.f;
		if (Cur.Len2D() > 0.03f && Sp.Way != EWay::Flat) Face(F, Cur * -1.f, Dt, 0.15f);
	}
	Beat(F, 0.f, Dt, 0.f);
	F.Beat = Sp.Way == EWay::Flat ? 0.05f : 0.15f;
}

void FSim::Step(float Dt, const FEnv& Env, const FV& Eye, const FClock& Clock)
{
	Dt = Clamp(Dt, 0.f, 0.1f);
	Time += Dt;
	Events.clear();
	++Frame;
	SettleTimer -= Dt;
	if (SettleTimer <= 0.f)
	{
		SettleTimer = 0.5f;
		Settle(Env, Eye, Clock);
	}
	Roam(Env, Eye, Clock, Dt);
	for (int G = 0; G < int(Groups.size()); ++G)
		if (Groups[G].bAlive) StepGroup(G, Dt, Env, Eye);
	for (FFish& F : All) StepFish(F, Dt, Env, Eye);
	// the small hunters snap at what comes by: each fish looks every 16th step (staggered by its seed)
	for (FFish& F : All)
	{
		F.Fed = std::max(0.f, F.Fed - Dt);
		if ((Frame + F.Seed) % 16u) continue;
		const EWay W = Table[F.Species].Way;
		if (W == EWay::Cruiser || W == EWay::Swarm || W == EWay::Surface) continue;
		if (Table[F.Species].Length < 0.05f) continue;
		Ambush(F, Env);
	}
	// what was eaten is gone
	if (!Events.empty())
		All.erase(std::remove_if(All.begin(), All.end(), [](const FFish& F) { return F.bEaten; }), All.end());
}
}
