#pragma once

#include "CoreMinimal.h"
#include "StarlingMurmuration.h"

/**
 * The flight simulation of Murmuration (src/sim/flock.worker.ts and src/sim/social.ts), ported:
 * StarDisplay social forces, banked flight with lift, individual speed / altitude / roll, flocks
 * with informed leaders, formations, merges and splits, spontaneous turns, an artistic flow field,
 * a peregrine falcon, the day (dawn exodus inland, small groups along the shore, the afternoon
 * return, the murmuration, the descent to the roost, the night), and hand-feeding: birds near the
 * person come down onto the ground around them, hop, peck and watch the hand, and flush when the
 * person moves at them.
 *
 * Frame: metres, three.js axes as in the original (Y up, X and Z horizontal), origin on the sea
 * surface in the middle of the roaming area. The actor maps it to Unreal. The world comes in as a
 * ground heightfield (sea = 0, land > 0.5 m) and a precise ground probe for landing spots.
 */
class FStarlingFlock
{
public:
	enum EState : uint8
	{
		Flying = 0,
		/** Dropping into the roost at dusk. */
		Descending = 1,
		/** On the roost at night (wings folded). */
		Perched = 2,
		/** Rolling away from the falcon. */
		Escaping = 3,
		/** Flying in to a spot on the ground near the person feeding. */
		Visiting = 4,
		/** On the ground near the person: stands, hops, pecks, watches the hand. */
		Grounded = 5,
		/** Far away inland (daytime foraging): not simulated, not drawn. */
		Away = 6,
	};
	enum class EActivity : uint8 { Roosting, Exodus, Foraging, Returning, Gathering, Murmuration, Settling };

	FStarlingFlock(int32 InCount, const FMurmurationParams& InParams, FVector2f InArea, uint32 Seed);

	FMurmurationParams P;
	/** Half size of the roaming area in X and Z, m. */
	FVector2f Area;

	/** Ground heights (sim Y, sea = 0) on a grid at (X0 + ix * Cell, Z0 + iz * Cell); outside it, the sea. */
	void SetGround(float InX0, float InZ0, float InCell, int32 InNx, int32 InNz, TArray<float>&& Heights);
	float GroundAt(float X, float Z) const;
	/** Exact ground height at (x, z) (a line trace), for landing spots; falls back to the heightfield. */
	TFunction<float(float X, float Z)> GroundProbe;

	/**
	 * The local time, hours 0-24, and the sun's times for the day. The first call puts every bird
	 * where it should be at that hour; later calls follow the clock, and a jump (a loaded game, a
	 * changed calendar) puts them there again.
	 */
	void SetClock(float Hours, float Sunrise, float Sunset, float Dusk);

	void SetPredator(bool bOn);
	bool IsPredatorOn() const { return bPredatorOn; }

	/**
	 * The person: feet position and walking velocity (sim frame), and whether they hold out food
	 * at Hand. Spots are landing spots on the ground around them (ground height included). Only
	 * birds within the params' NoticeRange of the hand notice it.
	 */
	void SetViewer(const FVector3f& Feet, const FVector3f& Velocity);
	void SetLure(bool bOn, const FVector3f& Hand, TArray<FVector3f>&& Spots);
	bool IsLureOn() const { return Lure.bOn; }

	void Step(float Dt);

	/** Where the flocks are and what they do, for logs. */
	FString Describe() const;
	EActivity GetActivity() const { return Act; }
	static const TCHAR* ActivityName(EActivity A);

	int32 Count = 0;
	TArray<float> Px, Py, Pz, Vx, Vy, Vz, Bank, FlapPhase, FlapAmp, Fold;
	TArray<uint8> State;
	FVector3f PredPos = FVector3f::ZeroVector;
	FVector3f PredVel = FVector3f::ZeroVector;
	float SimTime = 0.f;
	/** The roost: where the birds sleep, chosen at dusk as the land nearest the flock. */
	FVector3f Roost = FVector3f::ZeroVector;
	bool bHaveRoost = false;

private:
	// --- per bird
	TArray<float> FlapFreq, Timer, Refractory, Ex, Ey, Ez, Sfx, Sfy, Sfz, EscDelay, NextEx, NextEy, NextEz;
	TArray<float> SpeedN, AltN, RollN, Mood, FlS, FlA, FlT;
	TArray<uint8> NextEscape, Grp, FinalApp, Relocated;
	TArray<int32> TrailSeq, PerchOf, SpotOf;
	TArray<float> Dwell, ActT, HopT, HopX, HopY, HopZ, PeckT, HopCool, LeftBeach, Disturb, DisturbAt, AlarmT;
	TArray<float> SocialOut;
	uint32 Tick = 0;

	// --- random
	uint32 Seed;
	float Rnd();
	float Gauss();
	float Normal();
	/** Fixed per-bird ranks in [0, 1) (golden-ratio sequences): who stays, who leaves first. */
	static float RankA(int32 I) { return FMath::Frac(I * 0.6180339887498949f); }
	static float RankB(int32 I) { return FMath::Frac(I * 0.7548776662466927f + 0.31f); }

	// --- the world
	float GX0 = 0.f, GZ0 = 0.f, GCell = 1.f, GroundTop = 0.f;
	int32 GNx = 0, GNz = 0;
	TArray<float> GroundH;
	/** Horizontal direction from the area's centre to the land (inland), and points over the water along the shore. */
	FVector2f Inland = FVector2f(0.f, -1.f);
	TArray<FVector2f> ShorePts;
	/** Points on the land along the shore (grass where a shore group may come over low). */
	TArray<FVector2f> CoastLandPts;
	bool IsLand(float X, float Z) const { return GroundAt(X, Z) > 0.5f; }
	float Probe(float X, float Z) const { return GroundProbe ? GroundProbe(X, Z) : GroundAt(X, Z); }
	/** The land point nearest (x, z) on the heightfield; false if there is no land. */
	bool NearestLand(float X, float Z, FVector2f& Out) const;
	void ChooseRoost();
	TArray<FVector3f> Perches;
	int32 PerchCursor = 0;
	void PerchPos(int32 I, FVector3f& Out) const;
	void StartDescent(int32 I);
	void Settle(int32 I, const FVector3f& At);
	void Launch(int32 I, int32 G);

	// --- flocks (groups in the original's terms)
	enum class EMode : uint8 { Free, Local, Shore, Out, In };
	enum class EForm : uint8 { Mass, Stream, Ball, Sheet, Funnel };
	static constexpr int32 GMAX = 24;
	static constexpr int32 TRAIL = 64;
	static constexpr float TRAIL_DT = 0.35f;
	struct FGroup
	{
		EMode Mode = EMode::Free;
		int32 N = 0;
		FVector3f C = FVector3f::ZeroVector;
		FVector2f V = FVector2f::ZeroVector;
		FVector3f Goal = FVector3f::ZeroVector;
		float GoalT = 0.f, Band = 0.f, R = 3.f, Born = 0.f;
		int32 Lead = -1, Edge = -1, Tip = -1, LeadWait = 0;
		EForm Form = EForm::Mass;
		FVector2f OrbitC = FVector2f::ZeroVector;
		float OrbitR = 0.f, OrbitW = 1.f;
		FVector3f Trail[TRAIL];
		int32 Head = 0, TrailStart = 0;
		float TrailT = 0.f;
		// per step
		FVector3f SumP = FVector3f::ZeroVector;
		FVector2f SumV = FVector2f::ZeroVector;
		int32 Cnt = 0, In = 0;
		float EdgeS = 0.f, TipS = 0.f;
		int32 NewEdge = -1, NewTip = -1;
		FVector3f LeadPos = FVector3f::ZeroVector;
		float LeadR2 = -1.f, Dx = 0.f, Dz = 0.f, Pull = 0.f, Pk = 1.f, Pv = 0.5f, AltM = 1.f, Pr = 1.f;
	};
	FGroup Groups[GMAX];
	int32 AllocGroup(EMode Mode, const FVector3f& C);
	int32 NearestGroup(const FVector3f& At);
	int32 LargestGroup(EMode A, EMode B) const;
	int32 LargestGroup(EMode A) const { return LargestGroup(A, A); }
	void ChooseGoal(int32 G);
	void ChooseForm(int32 G);
	void SetForm(int32 G, EForm F);
	void StepGroups(float Dt);

	// --- spontaneous turns
	struct FImpulse { int32 SeedBird; int32 G; FVector3f Dir; float T; float R; int32 N; FVector3f At; };
	TArray<FImpulse> Impulses;
	void MaybeImpulse(float Dt, int32 FlyingN);

	// --- flow field
	static constexpr int32 FX = 16, FY = 6, FZ = 16, FN = FX * FY * FZ, FC = 3;
	TArray<float> FlowFrom, FlowTo, FlowNow, FlowU, FlowRate;
	float FlowOx = 0.f, FlowOz = 0.f;
	float FlowVal();
	void StepFlow(float Dt);
	void SampleFlow(float X, float Y, float Z, float& S, float& A, float& T) const;

	// --- the day
	enum class EPhase : uint8 { Murmur, Roost, Wake };
	EPhase Phase = EPhase::Murmur;
	EActivity Act = EActivity::Murmuration;
	bool bClockOn = false;
	float Hours = 16.f, HRate = 0.f, LastClockSim = 0.f, LastClockH = 0.f;
	float Sunrise = 7.4f, Sunset = 16.1f, Dusk = 16.9f;
	float GatherNow = 0.8f, GWander = 0.f, GWanderV = 0.f, RegimeT = 3.f, ArriveT = 0.f;
	static constexpr int32 WAVES = 6;
	int32 WaveGroup[WAVES];
	bool bShoreGroupsMade = false;
	int32 PerchedLast = 0, AwayLast = 0, AirLast = 0;
	float ExStart() const { return Sunrise - 0.4f; }
	float ExEnd() const { return Sunrise + 0.25f; }
	float DayEnd() const { return Sunset - 1.6f; }
	float GatherStart() const { return Sunset - 0.75f; }
	float MurmStart() const { return Sunset - 0.45f; }
	float SettleStart() const { return Sunset + 0.2f; }
	float NightStart() const { return Dusk + 0.4f; }
	EActivity ActivityAt(float H) const;
	float ShoreShare() const;
	int32 WaveOf(int32 I) const;
	float WaveStart(int32 W) const { return ExStart() + 0.05f + W * 0.05f; }
	float PresentShare(float H) const;
	void AutoGather(float& Base, float& Swing) const;
	void EnterActivity(EActivity A);
	enum class EWant : uint8 { Perched, Away, Present };
	EWant WantAt(int32 I, float H) const;
	void Resync();
	void PlaceGroups(const TArray<int32>& Birds, int32 K, EMode Mode);
	void FlyAt(int32 I, const FVector3f& At, const FVector3f& Vel, int32 G);
	void MakeAway(int32 I);
	int32 TargetGroups(int32 Local, float Gather) const;
	void StepRegime(float Dt);
	void StepDay(float Dt);
	void Arrive(const TArray<int32>& Birds);
	FVector2f CoastPoint(float Side);

	// --- the person and hand-feeding
	struct FLure
	{
		bool bOn = false;
		FVector3f Hand = FVector3f::ZeroVector;
		TArray<FVector3f> Spots;
		TArray<int32> Taken;
		float T = 0.f, QT = 0.f;
		TArray<int32> Queue;
	} Lure;
	FVector3f ViewerPos = FVector3f(1e9f);
	FVector3f ViewerVel = FVector3f::ZeroVector;
	float ViewerAt = -1e9f;
	float RecentFlush = 0.f;
	TArray<int32> GroundList;
	void FreeSpot(int32 I);
	int32 PickSpot(float X, float Z, float Within = 1e9f);
	int32 FarSpot(int32 I, float MinD, float Within);
	int32 Visitors() const;
	void Recruit();
	void StepLure(float Dt);
	float FidOf(int32 I, float D, float DXV, float DZV) const;
	void StepViewer(float Dt);
	void TakeOff(int32 I, int32 G, float FromX, float FromZ);
	void StepGrounded(int32 I, float Dt);
	void SeekStep(int32 I, const FVector3f& T, float DL, float Dt, float VMax, float Social, bool bHoldHeight = true);
	void FloorClamp(int32 I, float Min);

	// --- predator
	bool bPredatorOn = false;
	int32 PredMode = 0, PredTarget = -1;
	float PredT = 0.f;
	void StepPredator(float Dt);

	// --- social forces
	static constexpr float CELL = 1.5f;
	static constexpr int32 HBITS = 17, HSIZE = 1 << HBITS, HMASK = HSIZE - 1;
	static constexpr int32 MAXK = 16;
	static constexpr int32 OUT_STRIDE = 8;
	TArray<int32> CellOf, BucketStart, Fill, Sorted;
	TArray<float> SX, SY, SZ;
	static uint32 Hash(int32 X, int32 Y, int32 Z)
	{
		return ((uint32)X * 73856093u ^ (uint32)Y * 19349663u ^ (uint32)Z * 83492791u) & HMASK;
	}
	static bool InFlock(uint8 S) { return S == Flying || S == Escaping || S == Descending; }
	bool IsFlyingBird(int32 J) const { return State[J] == Flying || State[J] == Escaping; }
	void BuildGrid();
	int32 Knn(int32 I, int32 K, float CosBlind, float MaxRange, int32* NIdx, float* ND2, uint8* NVis) const;
	void SolveBird(int32 I, float Rdt, const FVector3f& Centre, bool bHaveCentre, float* Out) const;
	void Social(float Dt);
};
