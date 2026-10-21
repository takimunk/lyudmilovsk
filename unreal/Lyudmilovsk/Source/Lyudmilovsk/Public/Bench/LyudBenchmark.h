#pragma once

#include "CoreMinimal.h"
#include "Scalability.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudBenchmark.generated.h"

class ACameraActor;

/**
 * The repeatable benchmark (docs/benchmark.md): a fixed camera route through the hall, the corridor and out of
 * the door into the forest, at a fixed date and hour, once per scalability preset. It waits for the forests to
 * bake, flies the route once untimed to warm the shaders up, then flies it again per preset and records every
 * frame: frame time, game / render / RHI thread and GPU time, draw calls, triangles and memory. The summary
 * (median, p95, p99 per route segment and overall, plus the machine) goes to a JSON file and the game quits.
 *
 * Plain C++ so that it runs the same in a packaged game on every platform. Started by the command line:
 *   -LyudBench                      run on the level that opens
 *   -LyudBenchPresets=0,1,2,3       scalability levels to run (Low, Medium, High, Epic); default all four
 *   -LyudBenchTime=2026.10.20-16.00.00   game date and hour (default the scene's start)
 *   -LyudBenchOut=<file.json>       where the summary goes (default Saved/Benchmark/<time>.json)
 *   -LyudBenchLabel=<text>          free text stored in the summary (commit, variant)
 *   -LyudBenchSetup=cmd+1;cmd+2     console commands run after the forests are baked (a variant to compare); '+' for spaces
 *   -LyudBenchSegments=forest,overlook   only these segments of the route
 *   -LyudBenchPasses=base=|noshadow=lyud.Bench.ForestShadows+0   variants flown one after another in the same
 *                                   process ('|' between passes, name=commands, '+' for spaces): a fair A/B
 *                                   on a machine whose load drifts
 *   -LyudBenchRounds=<n>            fly the list of passes n times (default 1)
 *   -LyudBenchSpeed=<x>             route speed multiplier (1 by default)
 *   -LyudBenchNoWarmup              skip the warm-up lap
 *   -LyudBenchCsv                   also record a csvprofile per preset (Saved/Profiling/CSV)
 *   -LyudBenchShots                 a screenshot at the start of each segment (checks the route)
 * or from the console: lyud.Bench [presets].
 *
 * Dev knobs for comparing forest options without changing the forest code (lyud.Bench.Forest*), applied to the
 * forests' instanced components after the bake.
 */
UCLASS()
class LYUDMILOVSK_API ULyudBenchmark : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return Phase != EPhase::Idle; }

	/** Start a run now (presets: scalability levels 0..3). */
	void Start(const TArray<int32>& InPresets);

	/** One camera key of a segment: where, which way, at what second of the segment. */
	struct FKey
	{
		FVector Pos;
		FRotator Rot;
		float T = 0.f;
	};
	/** A piece of the route; bGround: Pos.Z is the eye height over the ground, not a world Z. */
	struct FSegment
	{
		FString Name;
		bool bGround = false;
		TArray<FKey> Keys;
		float Duration() const { return Keys.Num() ? Keys.Last().T : 0.f; }
	};

private:
	enum class EPhase : uint8 { Idle, WaitBake, Warmup, Settle, Record, Done };

	struct FFrame
	{
		float FrameMs, GameMs, RenderMs, RhiMs, GpuMs;
		int32 DrawCalls, Prims;
		float MemMB;
	};

	void BuildRoute();
	bool ForestsBaked(int32& OutTrees, int32& OutBaked) const;
	void ApplyForestKnobs();
	void ApplyPreset(int32 Level);
	void PlaceCamera(const FSegment& S, float T);
	double GroundZ(const FVector& At) const;
	void BeginSegment(int32 Index);
	void NextSegment();
	void BeginRun(int32 Index);
	void FinishRun();
	void ApplyPass(const FString& Cmds);
	void RestorePass();
	void WriteSummary();
	void Exec(const FString& Cmd);

	EPhase Phase = EPhase::Idle;
	TArray<FSegment> Route;
	TArray<int32> Presets;
	/** A variant: console commands applied for its runs and undone after. */
	struct FPass
	{
		FString Name;
		FString Cmds;
	};
	TArray<FPass> Passes;
	/** One flight of the route to record: a preset and a pass. */
	struct FRun
	{
		int32 Preset = 3;
		int32 Pass = 0;
	};
	TArray<FRun> Runs;
	int32 RunIdx = 0;
	/** Console variables a pass changed, with the values to put back. */
	TArray<TPair<FString, FString>> Restore;
	bool bLiveHidden = false;
	int32 SegIdx = 0;
	double PhaseStart = 0.0;
	double LastTick = 0.0;
	double SegTime = 0.f;
	float Speed = 1.f;
	bool bWarmupDone = false;
	bool bCsv = false;
	bool bShots = false;
	FString Label;
	FString OutPath;
	FString Setup;
	/** The scalability before the run, put back at the end (so the user settings are not left at the last preset). */
	Scalability::FQualityLevels StartLevels;
	FDateTime BenchTime = FDateTime(2026, 10, 20, 16, 0, 0);
	double SmoothZ = 0.0;
	bool bHaveZ = false;
	double BakeSeconds = 0.0;
	int32 NumTrees = 0;

	UPROPERTY() TObjectPtr<ACameraActor> Camera;

	/** Frames of the current preset, per segment. */
	TArray<TArray<FFrame>> Frames;
	/** The finished presets, each a JSON object. */
	TArray<FString> Results;
};
