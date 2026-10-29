// The starlings' evening murmuration, in plain C++ like the rest of the bird life (BirdSim.h).
//
// The starlings are ordinary birds of the bird life all day (groups feeding on fields, lawns and the beach, resting
// on wires and roofs). Before sunset their groups gather near the roost, fly to it and join one flock over it: the
// murmuration. While a bird is in it (EAct::Murmur) this flies it, with the flocking of Dima's Murmuration demo
// (StarDisplay, Hemelrijk & Hildenbrandt: topological neighbours, separation, alignment and cohesion scaled by
// centrality, speed alignment, banked flight with lift and roll, a few informed birds that know where the flock is
// going, spontaneous turns started at the rim, escape rolls copied neighbour to neighbour when the peregrine
// stoops). The flock wanders over the roost and the land round it and may go out over the sea along the coast,
// but never farther than FOptions::MurmurOffshore from the shore. From a few minutes after sunset the birds drop
// into the reedbed in waves (a bird follows its descending neighbours) and are handed back to the bird life on
// the last metres; they sleep there and leave at dawn group by group.
//
// Frame: Unreal world metres, Z up (as BirdSim).
#pragma once

#include "BirdSim.h"

#include <string>
#include <vector>

namespace LyudBirds
{
	class FMurmuration
	{
	public:
		/** Minutes from sunset: the groups fly to the roost and murmurate from this time; the descent starts at DescendMinutes. */
		static constexpr float StartMinutes = -40.f;
		static constexpr float DescendMinutes = 4.f;

		FMurmuration(const FWorld& W, int Species, int RoostSite);

		int Species() const { return SpeciesIdx; }
		bool IsRoostSite(int Site) const;
		/** A random site of the reedbed for a roosting bird. */
		int PickRoostSite(const FWorld& W, uint32_t& Rng) const;
		/** Where the flock is (or would be): above the roost before it has formed. */
		V3 Centre() const;
		V3 RoostCentre() const { return RoostC; }

		/** A bird arriving in the air joins the flock. */
		void Join(FWorld& W, int Id);
		/** A bird put straight into the flock (the clock jumped into the murmuration's time). */
		void Place(FWorld& W, int Id);
		/** Everybody out (a resettle); birds keep where they are. */
		void Clear();
		void Step(FWorld& W, float Dt);

		/** Seconds to extrapolate the drawn positions by (the flock moves in fixed steps). */
		float Ahead() const { return Acc; }
		int Count() const { return (int)M.size(); }
		/** Bank of a bird in the flock, radians (0 elsewhere): the renderer rolls the body. */
		float BankOf(int Id) const;
		/** A bird on the rim of the flock, for the falcon; -1 if none. */
		int RimBird(const FWorld& W, uint32_t& Rng) const;

		// for tests and logs
		float MaxOffshore = 0.f;        // metres over the sea from the shore, the farthest any bird went (reset by the caller)
		float MaxFromRoost = 0.f;       // metres, horizontally
		int Escapes = 0;                // escape rolls started (the falcon's attacks spread)
		int Landed = 0;                 // birds handed back into the reed
		double CpuMs = 0.0;             // time spent in Step, ms (the caller resets it)
		std::string Describe(const FWorld& W) const;

	private:
		struct FM
		{
			int Id = -1;
			float SpeedN = 0.f, AltN = 0.f, RollN = 0.f, Mood = 0.f, Rank = 0.f;
			float Bank = 0.f;
			float Refractory = 0.f, EscT = 0.f, EscDelay = 0.f;
			bool bEscape = false, bNextEscape = false, bDown = false;
			V3 Esc, NextEsc, Sf;
			int Site = -1;
			V3 Perch;
		};
		struct FImpulse { int Seed; V3 Dir; float T; float R; };

		int SpeciesIdx = -1;
		std::vector<int> RoostSites;
		V3 RoostC;
		float RoostR = 20.f;

		std::vector<FM> M;
		std::vector<int> SlotOf;        // bird id -> index in M (-1)
		std::vector<FImpulse> Impulses;
		V3 C, Vm;                       // the flock's centroid and mean velocity
		V3 Goal;
		float GoalT = 0.f, ImpulseT = 3.f;
		float Acc = 0.f;
		uint32_t Tick = 0, Rng = 0x2545F491u;
		bool bFormed = false;

		// spatial hash of the flock, rebuilt each step
		std::vector<int> BucketStart, Fill, Sorted, CellOf;
		std::vector<float> SX, SY, SZ;

		float Rnd();
		void Add(FWorld& W, int Id);
		void Remove(int Index);
		void NewGoal(const FWorld& W, float Hm);
		void Sub(FWorld& W, float H, float Hm);
		void BuildGrid(const FWorld& W);
		int Knn(const FWorld& W, int I, int K, float CosBlind, int* NIdx, float* ND2, unsigned char* NVis) const;
		void Solve(const FWorld& W, int I, float Rdt, float Hm, float* Out) const;
		void GoDown(FWorld& W, FM& F);
	};
}
