// The fish off the Sambian coast and how they live, in plain C++ (no Unreal types): tools/underwater/run.sh test runs
// it outside the engine. Metres, seconds, world X/Y/Z (Z up). ALyudSeaLife draws what it simulates.
#pragma once

#include <cmath>
#include <cstdint>
#include <unordered_map>
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
		/** Dives into the sand when chased (sand eels). */
		bool bBurrows;
	};

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
	};

	struct FClock
	{
		int Month = 10;
		/** 0 night .. 1 full day. */
		float Daylight = 1.f;
	};

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

	private:
		struct FGroup
		{
			int Species = 0;
			FV Centre, Vel, Goal;
			float Heading = 0.f;
			float Timer = 0.f;
			/** 0 calm .. 1 bolting; from where. */
			float Alarm = 0.f;
			FV From;
			int Members = 0;
			/** A hunter's prey group, its rest after a strike. */
			int Prey = -1;
			float Hunger = 0.f;
			bool bAlive = false;
		};

		void Settle(const FEnv& Env, const FV& Eye, const FClock& Clock);
		void Roam(const FEnv& Env, const FV& Eye, const FClock& Clock, float Dt);
		void SpawnGroup(int S, const FEnv& Env, const FV& Eye, const FClock& Clock);
		void StepGroup(int G, float Dt, const FEnv& Env, const FV& Eye);
		void StepFish(FFish& F, float Dt, const FEnv& Env, const FV& Eye);
		float Rand();
		float Season(int S, const FClock& Clock) const;

		std::vector<FFish> All;
		std::vector<FGroup> Groups;
		std::unordered_map<uint64_t, int> Cells;
		uint32_t Rng = 12345;
		double Time = 0.0;
		float SettleTimer = 0.f;
	};
}
