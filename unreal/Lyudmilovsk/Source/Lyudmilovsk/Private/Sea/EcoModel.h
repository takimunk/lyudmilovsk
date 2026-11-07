// The Baltic coast's food web under the water, in plain C++ (no Unreal types): tools/underwater/run.sh eco runs it outside
// the engine. Who eats whom, how much of each lives here, how the seasons move them; the fish round the diver (FishModel)
// come in the numbers this says. docs/ecosystem.md.
//
// The model is Ecopath with Ecosim's (Christensen & Walters 2004), cut to what the game needs: a mass-balanced base
// (biomass B, production P/B, consumption Q/B, a diet matrix; each group's other mortality M0 what is left of its
// production after everything that eats it), then daily steps of the foraging-arena equations (prey are only partly
// vulnerable to their predators at a time, which keeps the web stable), with the seasons on top: the light that drives
// the plankton's spring bloom and the summer's, the warmth that drives how much the cold-blooded eat, and the migrants
// that come in for some months and leave again (sprat, garfish, lumpsuckers, the winter's ducks...).
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace LyudEco
{
	enum class EKind : uint8_t
	{
		Producer,   // phytoplankton, the weed: grow by light
		Detritus,   // dead matter: fed by every death and the uneaten food
		Consumer,   // everything that eats
	};

	struct FDietItem
	{
		const char* Prey;
		float Share;
	};

	struct FNode
	{
		/** The node's id; a fish's is its LyudFish species Id (Herring, Cod...), so the two models meet by name. */
		const char* Id;
		EKind Kind;
		/** Base biomass, g wet weight per m2 of the coastal sea (0-20 m); production and consumption per year. */
		float B, PB, QB;
		/** Months it is here (bit 0 January); the rest of the year it is offshore or up a river (not eaten, not eating). */
		uint16_t Months;
		/** Warm-blooded (seals, birds): eats the same all year. */
		bool bWarm;
		/** What it eats (shares of its food, normalised). */
		std::vector<FDietItem> Diet;
	};

	int NumNodes();
	const FNode& Node(int Index);
	/** Index of a node by Id, or -1. */
	int Find(const char* Id);

	/** The seasons' drive by day of the year (0..365): the light for the plankton and weed, the water's warmth, both mean 1. */
	float LightFactor(float DayOfYear);
	float WarmthFactor(float DayOfYear);
	/** The blue-greens' drive (warm calm water, late July and August), mean 1. */
	float CyanoFactor(float DayOfYear);
	/** The day of the year a node's young of the year join it (its recruitment pulse), or -1. */
	float RecruitDay(int Node);

	class FModel
	{
	public:
		FModel();

		/** Back to the base (balanced) state, every migrant where the day of the year puts it. */
		void Reset(float DayOfYear);
		/** Steps the web through whole and part days from a day of the year on (wrapping at 365). */
		void Step(float DayOfYear, float Days);

		/** Biomass of a node now, g/m2 (here and away), and against its base (1 = as the base). */
		float Biomass(int Node) const { return B[Node]; }
		float Relative(int Node) const;
		/** How much of the node is in the coastal sea now (0..1: the migrants come and go). */
		float Here(int Node) const { return F[Node]; }
		/** The year's young still growing up out of sight (eggs, larvae), g/m2: they join the group round its RecruitDay. */
		float YoungToCome(int Node) const { return Young[Node]; }
		void SetBiomass(int Node, float Value);

		/** Base mass balance: how much of each node's production is eaten (EE, Ecopath's ecotrophic efficiency, must be <= 1). */
		float EE(int Node) const { return BaseEE[Node]; }
		/** The share of a predator's food that is a prey (0 if it never eats it). */
		float DietShare(int Predator, int Prey) const { return Dc[Predator * N + Prey]; }
		/** Food taken in the last step, g/m2 per day, prey by predator. */
		float Flow(int Predator, int Prey) const { return LastQ[Predator * N + Prey]; }

		/** Someone took fish out (the player's line, a net): grams from the whole coast (the model's area is AreaM2). */
		void Remove(int Node, float Grams);
		float AreaM2 = 3.0e6f;

		/**
		 * The local thinning round where fish were taken or eaten (the coast-wide numbers barely notice a few fish, the
		 * spot does): 40 m cells, a multiplier per species that heals as fish move back in (days; bottom fish slower).
		 */
		void TakeLocal(int Node, float X, float Y, float Fraction);
		float Local(int Node, float X, float Y) const;

		/** The state as (id, value) pairs: biomass against the base as "<id>@rel", the coastal fractions as "<id>@here", the local thinning as "<id>@x,y". */
		void Save(std::vector<std::string>& Keys, std::vector<float>& Values) const;
		/** Restores what it knows; unknown ids are skipped, missing ones keep the base. */
		void Load(const std::vector<std::string>& Keys, const std::vector<float>& Values);

		/** A line per node: biomass against base, here or away, what eats most of it. */
		std::string Report() const;

	private:
		void Balance();
		void SubStep(float DayOfYear, float Dt);

		int N = 0;
		std::vector<float> B, B0, F, M0, GE, BaseEE, Dc, A, V, LastQ, Pmax, Hp, RecDay, RecShare, Young;
		int IPhyto = -1, ICyano = -1;
		std::vector<float> ScratchC, ScratchGain, ScratchLoss;
		float DetExport = 0.f;
		struct FCell { std::vector<std::pair<int, float>> Taken; };
		std::unordered_map<uint64_t, FCell> Cells;
		float LocalHeal(int Node) const;
	};
}
