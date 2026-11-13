// The fish off the Sambian coast and how they live, in plain C++ (no Unreal types): tools/underwater/run.sh test runs
// it outside the engine. Metres, seconds, world X/Y/Z (Z up). ALyudSeaLife draws what it simulates.
#pragma once

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace LyudFish
{
	struct FV
	{
		float X = 0.f, Y = 0.f, Z = 0.f;
		FV() = default;
		FV(float A, float B, float C) : X(A), Y(B), Z(C) {}
		FV operator+(const FV& O) const { return FV(X + O.X, Y + O.Y, Z + O.Z); }
		FV operator-(const FV& O) const { return FV(X - O.X, Y - O.Y, Z - O.Z); }
		FV operator*(float S) const { return FV(X * S, Y * S, Z * S); }
		FV& operator+=(const FV& O) { X += O.X; Y += O.Y; Z += O.Z; return *this; }
		float Dot(const FV& O) const { return X * O.X + Y * O.Y + Z * O.Z; }
		float Len() const { return std::sqrt(X * X + Y * Y + Z * Z); }
		float Len2D() const { return std::sqrt(X * X + Y * Y); }
		FV Norm() const { const float L = Len(); return L > 1e-6f ? *this * (1.f / L) : FV(1.f, 0.f, 0.f); }
		FV Clamped(float M) const { const float L = Len(); return L > M ? *this * (M / L) : *this; }
	};

	/** How a species lives. */
	enum class EWay : uint8_t
	{
		School,     // a tight school in mid water (herring, sprat; sand eels over sand, which dive into it when chased)
		Shoal,      // a loose shoal near cover, curious (perch, sticklebacks)
		Cruiser,    // a lone hunter cruising over the bed or in open water (cod, sea trout); chases schools
		Surface,    // small groups just under the surface (garfish)
		Hopper,     // sits on the bed facing the current, hops a little, darts off (gobies)
		Lurker,     // lies still among stones and weed, rarely moves (sculpin, eelpout, lumpsucker)
		Flat,       // lies flat on the sand half buried, glides off in a puff when you come close (flounder, turbot)
		Hover,      // hangs head up among the eelgrass, swaying with it (pipefish)
		Crawler,    // walks the bed in short runs, stops, feels about (shrimp, the isopod Saduria, mud crabs, amphipods)
		Swarm,      // a loose drifting cloud low over the bed, each one jittering (mysids)
	};

	struct FSpecies
	{
		const char* Id;
		EWay Way;
		/** Total length, m (each fish 0.75..1.25 of it), cruise and burst speed, m/s. */
		float Length, Cruise, Burst;
		/** Still-water depth it lives in, m; height over the bed it keeps (min, max), m (-1: just under the surface). */
		float DepthMin, DepthMax, OverBedMin, OverBedMax;
		/** Liking for sand, stones and weed, 0..1 each. */
		float Sand, Stone, Weed;
		/** Groups (schools, shoals, hunters) or single fish (bottom fish) per 100 m2 of the best habitat. */
		float Abundance;
		int GroupMin, GroupMax;
		/** Months it is here (bit 0 January), and how active by day and by night, 0..1. */
		uint16_t Months;
		float Day, Night;
		/** Comes no closer to a diver than this, m; a hunter of fish up to PreyLength m. */
		float Shy;
		float PreyLength;
		/** The body's swimming wave: waves along the body and how much of it bends (eel-like: more, from the head). */
		float Waves, Bend;
		/** Dives into the sand when chased (sand eels, shrimp). */
		bool bBurrows;
		/** Breathes air: goes up to the surface every minute or two (grey seals). */
		bool bBreathes = false;
		/** Comes to look at a diver instead of keeping off (grey seals). */
		bool bCurious = false;
		/** An air breather's dive and its time at the surface, s (0: a seal's 1-3 minutes under, 4-9 s up). */
		float Dive = 0.f, Rest = 0.f;
		/** A bird: floats on the surface between dives (cormorants after fish, eiders down to the mussels). */
		bool bBird = false;
		/** Months it spawns (bit 0 January): schools and shoals go low over the weed, guards stay by their eggs. */
		uint16_t Spawn = 0;
		/** Guards its eggs while spawning: stays put, faces a diver (lumpsucker males, round gobies). */
		bool bGuards = false;
		/** Builds a nest while spawning: the males settle over it in the weed (three-spined sticklebacks). */
		bool bNests = false;
		/** An ambusher's reach, body lengths (a pike lunges far), and its rest height over the bed, m (a pike in the weed). */
		float Lunge = 1.5f, Hang = 0.f;
	};

	/** Ways that live on the bed in their own place (settled per cell), not roaming in groups. */
	inline bool IsSettled(EWay W) { return W == EWay::Hopper || W == EWay::Lurker || W == EWay::Flat || W == EWay::Hover || W == EWay::Crawler; }

	int NumSpecies();
	const FSpecies& Species(int Index);
	/** Index of a species by Id, or -1. */
	int FindSpecies(const char* Id);

	/** The water and bed around the fish (ALyudSeaLife answers from the sea, the ground and the terrain mask). */
	struct FEnv
	{
		virtual ~FEnv() = default;
		/** The bed's height, m; false where there is no sea (land, the bare flat). */
		virtual bool Bed(float X, float Y, float& OutZ) const = 0;
		/** The still water's level now, m. */
		virtual float SurfaceZ() const = 0;
		/** Sand, stones and weed (eelgrass, wrack) on the bed here, 0..1 each. */
		virtual void Habitat(float X, float Y, float& Sand, float& Stone, float& Weed) const = 0;
		/** The water's mean motion, m/s. */
		virtual FV Current(const FV& P) const = 0;
		/** How many of a species there are here against the usual (the food web's numbers, a spot fished out), 0.. */
		virtual float Abundance(int Species, float X, float Y) const { return 1.f; }
	};

	/** A hunting moment seen: who struck at whom, where, and whether it was caught (a miss: the prey got away). */
	struct FEaten
	{
		int Predator = 0, Prey = 0;
		FV Pos;
		bool bCaught = true;
	};

	struct FClock
	{
		int Month = 10;
		/** 0 night .. 1 full day. */
		float Daylight = 1.f;
	};

	/** How well a spot suits a species, 0..1 (its depth range and its liking for sand, stones, weed). */
	float Suitability(int Species, float Sand, float Stone, float Weed, float Depth);
	/** How active a species is now, 0..1 (here this month, by day or by night). */
	float Activity(int Species, const FClock& Clock);
	/** Spawning this month. */
	bool Spawning(int Species, int Month);
	/** The share of the young of the year among them this month (some months after spawning: Jul-Oct for the herring). */
	float YoungShare(int Species, int Month);

	/** One fish as drawn: where, which way, how its body moves. */
	struct FFish
	{
		FV Pos, Vel;
		/** Facing: yaw and pitch (radians), roll (radians: a turning fish banks a little). */
		float Yaw = 0.f, Pitch = 0.f, Roll = 0.f;
		float Len = 0.2f;
		/** The swimming wave's phase (radians, wrapped), its amplitude 0..1, and the body's bend into a turn (-1..1). */
		float Phase = 0.f, Beat = 0.2f, Bend = 0.f;
		uint8_t Species = 0;
		uint8_t State = 0;
		/** Hidden in the sand (not drawn). */
		bool bHidden = false;
		/** Eaten this step (gone at the step's end). */
		bool bEaten = false;
		/** A young of the year (smaller), a nest keeper. */
		bool bYoung = false, bNest = false;
		/** Seconds until it is hungry again after a catch. */
		float Fed = 0.f;
		float Timer = 0.f;
		int Group = -1;
		FV Slot, Goal;
		uint32_t Seed = 1;
		uint64_t Cell = 0;
	};

	/**
	 * Fish around an eye: bottom fish are settled per 6 m cell of the bed by its habitat (the same fish in the same
	 * place every visit), schools, shoals and hunters roam in from beyond sight and leave again. Schools keep their
	 * formation, open round a diver and flee hunters (sand eels dive into the sand), hunters chase schools, bottom fish
	 * dart off when you come close, slow fish hold against the current. What is here follows the month and the light.
	 */
	class FSim
	{
	public:
		/** Fish live within this of the eye, m; roamers come in from beyond SpawnMin (out of sight in the Baltic). */
		float Radius = 24.f;
		float SpawnMin = 13.f;
		/** How many bed cells new to the reach are filled with their bottom fish in one step (the rest on the next steps):
		 *  the game sets a few, so coming to the water does not ask the bed everywhere in one frame. */
		int NewCellsPerSettle = 1 << 30;
		/** Multiplies every species' numbers. */
		float Density = 1.f;
		uint32_t Seed = 7;

		void Step(float Dt, const FEnv& Env, const FV& Eye, const FClock& Clock);
		void Clear();
		const std::vector<FFish>& Fish() const { return All; }
		/** How many of a species are around now (drawn or hidden). */
		int Count(int Species) const;
		/** The nearest fish of a species to a point, or false. */
		bool Nearest(int Species, const FV& From, FV& Out) const;

		/** Who eats whom: the share of a predator's food a prey is (from the food web). Without it a hunter takes any fish small enough. */
		void SetDiet(int Predator, int Prey, float Share);
		bool Eats(int Predator, int Prey) const;
		/** The hunting moments of the last step, caught and missed. */
		const std::vector<FEaten>& Eaten() const { return Events; }
		/** Takes the nearest drawn fish of a species within Reach of a point (a hook, a net): false if none. */
		bool Take(int Species, const FV& At, float Reach, FFish* Out = nullptr);

	private:
		struct FGroup
		{
			int Species = 0;
			FV Centre, Vel, Goal;
			/** The water's drift at the centre this step (its members swim in it). */
			FV Cur;
			float Heading = 0.f;
			float Timer = 0.f;
			/** 0 calm .. 1 bolting; from where. */
			float Alarm = 0.f;
			FV From;
			int Members = 0;
			/** A hunter's prey group, its rest after a strike. */
			int Prey = -1;
			float Hunger = 0.f;
			/** A hunter's prey on the bed (a settled fish or crawler, by its seed), 0 none. */
			uint32_t BedPrey = 0;
			/** An air breather's time left under, s, and its time at the surface. */
			float Breath = 0.f, AtSurface = 0.f;
			/** A curious one's interest in the diver, s. */
			float Look = 0.f;
			bool bAlive = false;
			bool bYoung = false;
		};

		void Settle(const FEnv& Env, const FV& Eye, const FClock& Clock);
		void Roam(const FEnv& Env, const FV& Eye, const FClock& Clock, float Dt);
		void SpawnGroup(int S, const FEnv& Env, const FV& Eye, const FClock& Clock);
		void StepGroup(int G, float Dt, const FEnv& Env, const FV& Eye);
		void StepFish(FFish& F, float Dt, const FEnv& Env, const FV& Eye);
		void Ambush(FFish& F, const FEnv& Env);
		void Hunt(FGroup& G, int Gi, float Dt, FV& Want, float& Speed);
		FFish* FindSeed(uint32_t FishSeed);
		float Rand();
		float Season(int S, const FClock& Clock) const;

		std::vector<FFish> All;
		std::vector<FGroup> Groups;
		std::unordered_map<uint64_t, int> Cells;
		std::vector<float> Diet;
		/** Prey candidates by grid cell (key, index into All), sorted; rebuilt each step for the ambush checks. */
		std::vector<std::pair<uint64_t, uint32_t>> PreyGrid;
		static constexpr float PreyCell = 1.f;
		std::vector<FEaten> Events;
		uint32_t Rng = 12345;
		uint32_t Frame = 0;
		double Time = 0.0;
		float SettleTimer = 0.f;
		/** The clock of this step: 0 night .. 1 day, and the month. */
		float Light = 1.f;
		int Month = 10;
	};
}
