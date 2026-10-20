#pragma once

#include "CoreMinimal.h"
#include "StarlingMurmuration.h"

/**
 * The flight simulation of Murmuration (src/sim/flock.worker.ts and src/sim/social.ts), ported
 * for one murmuration over the sea: StarDisplay social forces, banked flight with lift, individual
 * speed / altitude / roll, informed leaders steering for goals, formations, spontaneous turns, an
 * artistic flow field and a peregrine falcon.
 *
 * Frame: metres, three.js axes as in the original (Y up, X and Z horizontal), origin on the sea
 * surface in the middle of the roaming area. The actor maps it to Unreal.
 */
class FStarlingFlock
{
public:
	enum EState : uint8 { Flying = 0, Escaping = 1 };

	FStarlingFlock(int32 InCount, const FMurmurationParams& InParams, FVector2f InArea, uint32 Seed);

	FMurmurationParams P;
	/** Half size of the roaming area in X and Z, m. */
	FVector2f Area;

	/** Ground heights (sim Y, sea = 0) on a grid at (X0 + ix * Cell, Z0 + iz * Cell); outside it, the sea. */
	void SetGround(float InX0, float InZ0, float InCell, int32 InNx, int32 InNz, TArray<float>&& Heights);
	float GroundAt(float X, float Z) const;

	void SetPredator(bool bOn);
	bool IsPredatorOn() const { return bPredatorOn; }

	void Step(float Dt);
	/** Where the flock is heading, for logs. */
	FString Describe() const;

	int32 Count = 0;
	TArray<float> Px, Py, Pz, Vx, Vy, Vz, Bank, FlapPhase, FlapAmp;
	TArray<uint8> State;
	FVector3f PredPos = FVector3f::ZeroVector;
	FVector3f PredVel = FVector3f::ZeroVector;
	float SimTime = 0.f;

private:
	// per bird
	TArray<float> FlapFreq, Timer, Refractory, Ex, Ey, Ez, Sfx, Sfy, Sfz, EscDelay, NextEx, NextEy, NextEz;
	TArray<float> SpeedN, AltN, RollN, Mood, FlS, FlA, FlT;
	TArray<uint8> NextEscape;
	TArray<int32> TrailSeq;
	TArray<float> SocialOut;
	uint32 Tick = 0;

	// random
	uint32 Seed;
	float Rnd();
	float Gauss();
	float Normal();

	// ground
	float GX0 = 0.f, GZ0 = 0.f, GCell = 1.f, GroundTop = 0.f;
	int32 GNx = 0, GNz = 0;
	TArray<float> GroundH;
	bool IsLand(float X, float Z) const { return GroundAt(X, Z) > 0.5f; }

	// the flock (one group in the original's terms)
	enum class EForm : uint8 { Mass, Stream, Ball, Sheet };
	FVector3f GC = FVector3f::ZeroVector;     // centroid
	FVector2f GV = FVector2f::ZeroVector;     // mean horizontal velocity
	FVector3f Goal = FVector3f::ZeroVector;
	float GoalTime = 0.f;
	float Band = 0.f;                         // the flock's altitude, follows the goal at a rate it can hold
	int32 Lead = -1, Edge = -1, Tip = -1, LeadWait = 0, NFlock = 0;
	float LeadR = 3.f;
	EForm Form = EForm::Mass;
	FVector2f OrbitC = FVector2f::ZeroVector;
	float OrbitR = 0.f, OrbitW = 1.f;
	static constexpr int32 TRAIL = 64;
	static constexpr float TRAIL_DT = 0.35f;
	FVector3f Trail[TRAIL];
	int32 Head = 0, TrailStart = 0;
	float TrailT = 0.f;
	void ChooseGoal();
	void ChooseForm();
	void StepGroup(float Dt);

	// spontaneous turns
	struct FImpulse { int32 SeedBird; FVector3f Dir; float T; float R; int32 N; FVector3f At; };
	TArray<FImpulse> Impulses;
	void MaybeImpulse(float Dt);

	// flow field
	static constexpr int32 FX = 16, FY = 6, FZ = 16, FN = FX * FY * FZ, FC = 3;
	TArray<float> FlowFrom, FlowTo, FlowNow, FlowU, FlowRate;
	float FlowOx = 0.f, FlowOz = 0.f;
	float FlowVal();
	void StepFlow(float Dt);
	void SampleFlow(float X, float Y, float Z, float& S, float& A, float& T) const;

	// predator
	bool bPredatorOn = false;
	int32 PredMode = 0, PredTarget = -1;
	float PredT = 0.f;
	void StepPredator(float Dt);

	// social forces
	static constexpr float CELL = 1.5f;
	static constexpr int32 HBITS = 17, HSIZE = 1 << HBITS, HMASK = HSIZE - 1;
	static constexpr int32 MAXK = 16;
	static constexpr int32 OUT_STRIDE = 7;
	TArray<int32> CellOf, BucketStart, Fill, Sorted;
	TArray<float> SX, SY, SZ;
	static uint32 Hash(int32 X, int32 Y, int32 Z)
	{
		return ((uint32)X * 73856093u ^ (uint32)Y * 19349663u ^ (uint32)Z * 83492791u) & HMASK;
	}
	void BuildGrid();
	int32 Knn(int32 I, int32 K, float CosBlind, float MaxRange, int32* NIdx, float* ND2, uint8* NVis) const;
	void SolveBird(int32 I, float Rdt, const FVector3f& Centre, bool bHaveCentre, float* Out) const;
	void Social(float Dt);
};
