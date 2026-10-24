#include "Bench/LyudBenchmark.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "Kismet/GameplayStatics.h"
#include "LandscapeProxy.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Plants/GameCalendar.h"
#include "Plants/ProceduralForest.h"
#include "Plants/ProceduralPlant.h"
#include "Sky/LyudSky.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "RHIGlobals.h"
#include "RHIStats.h"
#include "DynamicRHI.h"
#include "RenderTimer.h"
#include "Scalability.h"
#include "UnrealClient.h"

static TAutoConsoleVariable<int32> CVarForestShadows(TEXT("lyud.Bench.ForestShadows"), -1,
	TEXT("Benchmark knob: forests cast shadows (1), not (0), as built (-1)."));
static TAutoConsoleVariable<float> CVarForestCull(TEXT("lyud.Bench.ForestCullM"), -1.f,
	TEXT("Benchmark knob: forest instances farther than this many metres are not drawn (-1: as built)."));
static TAutoConsoleVariable<int32> CVarForestHide(TEXT("lyud.Bench.ForestHide"), 0,
	TEXT("Benchmark knob: hide every forest (the cost of the rest of the scene)."));
static TAutoConsoleVariable<int32> CVarForestWPO(TEXT("lyud.Bench.ForestWPODisableM"), -1,
	TEXT("Benchmark knob: forest wind (world position offset) stops beyond this many metres (-1: as built)."));
static TAutoConsoleVariable<int32> CVarSkyFreeze(TEXT("lyud.Bench.SkyFreeze"), 0,
	TEXT("Benchmark knob: the sky stops moving the sun every frame (each move redraws the whole sun shadow)."));
static TAutoConsoleVariable<int32> CVarForestChannel0(TEXT("lyud.Bench.ForestChannel0"), 1,
	TEXT("Benchmark knob: forests on lighting channel 0 too (1, as built: the club's lights may shadow them) or on the outdoor channel 1 only (0)."));
static TAutoConsoleVariable<int32> CVarLiveHide(TEXT("lyud.Bench.LiveHide"), 0,
	TEXT("Benchmark knob: hide the live procedural plants (the full trees the forests swap in near the camera, and the hand-placed ones)."));
static TAutoConsoleVariable<int32> CVarForestMinLod(TEXT("lyud.Bench.ForestMinLod"), -1,
	TEXT("Benchmark knob: forest instances never draw finer than this LOD (-1: as built)."));

namespace
{
	const float SettleSeconds = 1.0f;
	const float WarmupSpeed = 3.f;
	const double BakeTimeout = 300.0;

	/** Unreal cm of a point of the outdoor map (tools/world_map.py): X west, Y out of the door, from the portal face. */
	FVector Map(double X, double Y, double Z)
	{
		const double PortalFaceY = 790.0;
		return FVector(X * 100.0, PortalFaceY + Y * 100.0, Z);
	}

	/** A JSON object written by hand (the module does not link the Json module). */
	struct FJson
	{
		TArray<FString> Fields;
		void Num(const TCHAR* K, double V) { Fields.Add(FString::Printf(TEXT("\"%s\": %.4g"), K, V)); }
		void Str(const TCHAR* K, const FString& V) { Fields.Add(FString::Printf(TEXT("\"%s\": \"%s\""), K, *V.ReplaceCharWithEscapedChar())); }
		void Bool(const TCHAR* K, bool V) { Fields.Add(FString::Printf(TEXT("\"%s\": %s"), K, V ? TEXT("true") : TEXT("false"))); }
		void Raw(const TCHAR* K, const FString& V) { Fields.Add(FString::Printf(TEXT("\"%s\": %s"), K, *V)); }
		void Obj(const TCHAR* K, const FJson& V) { Raw(K, V.ToString()); }
		FString ToString() const { return TEXT("{") + FString::Join(Fields, TEXT(", ")) + TEXT("}"); }
	};

	double Percentile(TArray<float> V, double P)
	{
		if (V.IsEmpty()) return 0.0;
		V.Sort();
		const double I = P * (V.Num() - 1);
		const int32 Lo = FMath::FloorToInt(I), Hi = FMath::Min(Lo + 1, V.Num() - 1);
		return FMath::Lerp((double)V[Lo], (double)V[Hi], I - Lo);
	}

	FJson Stats(const TArray<float>& V)
	{
		FJson O;
		double Sum = 0.0, Max = 0.0;
		for (const float X : V) { Sum += X; Max = FMath::Max(Max, (double)X); }
		O.Num(TEXT("mean"), V.Num() ? Sum / V.Num() : 0.0);
		O.Num(TEXT("median"), Percentile(V, 0.5));
		O.Num(TEXT("p95"), Percentile(V, 0.95));
		O.Num(TEXT("p99"), Percentile(V, 0.99));
		O.Num(TEXT("max"), Max);
		return O;
	}

	/** Every metric of a set of frames, keyed like the summary's columns. */
	template <typename TFrame>
	FJson Summarise(const TArray<TFrame>& Fs, double Seconds)
	{
		TArray<float> Frame, Game, Render, Rhi, Gpu, Draws, Prims, Mem;
		for (const TFrame& F : Fs)
		{
			Frame.Add(F.FrameMs); Game.Add(F.GameMs); Render.Add(F.RenderMs); Rhi.Add(F.RhiMs); Gpu.Add(F.GpuMs);
			Draws.Add((float)F.DrawCalls); Prims.Add((float)F.Prims); Mem.Add(F.MemMB);
		}
		FJson O;
		O.Num(TEXT("frames"), Fs.Num());
		O.Num(TEXT("seconds"), Seconds);
		O.Num(TEXT("fps_mean"), Seconds > 0.0 ? Fs.Num() / Seconds : 0.0);
		O.Obj(TEXT("frame_ms"), Stats(Frame));
		O.Obj(TEXT("game_ms"), Stats(Game));
		O.Obj(TEXT("render_ms"), Stats(Render));
		O.Obj(TEXT("rhi_ms"), Stats(Rhi));
		O.Obj(TEXT("gpu_ms"), Stats(Gpu));
		O.Obj(TEXT("draw_calls"), Stats(Draws));
		O.Obj(TEXT("triangles"), Stats(Prims));
		O.Obj(TEXT("memory_mb"), Stats(Mem));
		return O;
	}

	ULyudBenchmark* FindBench(UWorld* World)
	{
		return World ? World->GetSubsystem<ULyudBenchmark>() : nullptr;
	}
}

static FAutoConsoleCommandWithWorldAndArgs GLyudBenchCmd(TEXT("lyud.Bench"),
	TEXT("lyud.Bench [0,1,2,3]: run the benchmark route at these scalability levels (default all) and quit."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudBenchmark* B = FindBench(World);
		if (!B) return;
		TArray<int32> P;
		if (Args.Num())
		{
			TArray<FString> Parts;
			Args[0].ParseIntoArray(Parts, TEXT(","));
			for (const FString& S : Parts) P.Add(FCString::Atoi(*S));
		}
		B->Start(P);
	}));

bool ULyudBenchmark::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

TStatId ULyudBenchmark::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudBenchmark, STATGROUP_Tickables);
}

void ULyudBenchmark::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FString Value;
	const TCHAR* Cmd = FCommandLine::Get();
	if (!FParse::Param(Cmd, TEXT("LyudBench")) && !FParse::Value(Cmd, TEXT("LyudBench="), Value)) return;
	// the main menu would pause the world
	if (IConsoleVariable* Menu = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.MainMenuOnStart"))) Menu->Set(0, ECVF_SetByCommandline);
	TArray<int32> P;
	if (FParse::Value(Cmd, TEXT("LyudBenchPresets="), Value, false))
	{
		TArray<FString> Parts;
		Value.ParseIntoArray(Parts, TEXT(","));
		for (const FString& S : Parts) P.Add(FCString::Atoi(*S));
	}
	Start(P);
}

void ULyudBenchmark::Start(const TArray<int32>& InPresets)
{
	const TCHAR* Cmd = FCommandLine::Get();
	Presets = InPresets.Num() ? InPresets : TArray<int32>{ 0, 1, 2, 3 };
	FString Value;
	Speed = FParse::Value(Cmd, TEXT("LyudBenchSpeed="), Value) ? FMath::Max(0.1f, FCString::Atof(*Value)) : 1.f;
	bWarmupDone = FParse::Param(Cmd, TEXT("LyudBenchNoWarmup"));
	bCsv = FParse::Param(Cmd, TEXT("LyudBenchCsv"));
	bShots = FParse::Param(Cmd, TEXT("LyudBenchShots"));
	FParse::Value(Cmd, TEXT("LyudBenchLabel="), Label);
	// spaces travel as '+' (a command-line value with spaces does not survive every platform's launcher)
	FParse::Value(Cmd, TEXT("LyudBenchSetup="), Setup, false);
	Setup.ReplaceCharInline(TEXT('+'), TEXT(' '));
	if (!FParse::Value(Cmd, TEXT("LyudBenchOut="), OutPath))
	{
		OutPath = FPaths::ProjectSavedDir() / TEXT("Benchmark") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")) + TEXT(".json");
	}
	if (FParse::Value(Cmd, TEXT("LyudBenchTime="), Value)) FDateTime::Parse(Value, BenchTime);
	BuildRoute();
	if (FParse::Value(Cmd, TEXT("LyudBenchSegments="), Value, false))
	{
		TArray<FString> Keep;
		Value.ParseIntoArray(Keep, TEXT(","));
		Route.RemoveAll([&Keep](const FSegment& S) { return !Keep.Contains(S.Name); });
	}
	Passes.Reset();
	if (FParse::Value(Cmd, TEXT("LyudBenchPasses="), Value, false))
	{
		TArray<FString> Parts;
		Value.ParseIntoArray(Parts, TEXT("|"));
		for (const FString& Part : Parts)
		{
			FPass P;
			if (!Part.Split(TEXT("="), &P.Name, &P.Cmds)) P.Name = Part;
			P.Cmds.ReplaceCharInline(TEXT('+'), TEXT(' '));
			Passes.Add(P);
		}
	}
	if (Passes.IsEmpty()) Passes.Add(FPass());
	const int32 Rounds = FParse::Value(Cmd, TEXT("LyudBenchRounds="), Value) ? FMath::Max(1, FCString::Atoi(*Value)) : 1;
	Runs.Reset();
	for (const int32 Preset : Presets)
	{
		for (int32 R = 0; R < Rounds; R++)
		{
			for (int32 I = 0; I < Passes.Num(); I++) Runs.Add({ Preset, I });
		}
	}
	RunIdx = 0;
	Results.Reset();
	Phase = EPhase::WaitBake;
	PhaseStart = LastTick = FPlatformTime::Seconds();
	// the saved user settings would override -ResX/-ResY
	int32 ResX = 0, ResY = 0;
	if (FParse::Value(Cmd, TEXT("ResX="), ResX) && FParse::Value(Cmd, TEXT("ResY="), ResY)) Exec(FString::Printf(TEXT("r.SetRes %dx%dw"), ResX, ResY));
	StartLevels = Scalability::GetQualityLevels();
	Exec(TEXT("r.VSync 0"));
	Exec(TEXT("t.MaxFPS 0"));
	UE_LOG(LogTemp, Warning, TEXT("BENCH start: %d presets, %d passes, %d runs, %d segments, out %s"), Presets.Num(), Passes.Num(), Runs.Num(), Route.Num(), *OutPath);
}

void ULyudBenchmark::BuildRoute()
{
	// Lvl_Scene01: the hall (Y 0..790, the exit door at X -193 in its far wall), the corridor (walkway X -165..-50
	// toward -Y), then the outdoor map. Eye height 165 cm indoors.
	Route.Reset();
	auto Seg = [this](const TCHAR* Name, bool bGround, std::initializer_list<FKey> Keys)
	{
		FSegment S;
		S.Name = Name;
		S.bGround = bGround;
		S.Keys = Keys;
		Route.Add(MoveTemp(S));
	};
	Seg(TEXT("hall"), false, {
		{ FVector(-110, 60, 165), FRotator(-5, 75, 0), 0.f },
		{ FVector(40, 330, 165), FRotator(-5, 30, 0), 5.f },
		{ FVector(-150, 560, 165), FRotator(-3, 110, 0), 10.f } });
	Seg(TEXT("corridor"), false, {
		{ FVector(-110, -60, 165), FRotator(-3, -90, 0), 0.f },
		{ FVector(-110, -2250, 165), FRotator(-3, -90, 0), 15.f } });
	// out of the exit door and down the path
	Seg(TEXT("door"), false, {
		{ FVector(-193, 840, 165), FRotator(0, 90, 0), 0.f },
		{ FVector(-60, 1800, 170), FRotator(0, 85, 0), 7.f } });
	// the terrace in front of the door, looking back up at the upland and its forest
	Seg(TEXT("terrace"), true, {
		{ Map(0, 30, 170), FRotator(14, -90, 0), 0.f },
		{ Map(0, 30, 170), FRotator(10, -55, 0), 4.f },
		{ Map(0, 30, 170), FRotator(12, -125, 0), 8.f } });
	// a walk inside the old forest on the upland
	Seg(TEXT("forest"), true, {
		{ Map(-60, -90, 170), FRotator(0, -127, 0), 0.f },
		{ Map(-75, -110, 170), FRotator(2, -100, 0), 8.f },
		{ Map(-90, -130, 170), FRotator(0, -150, 0), 16.f } });
	// from over the upland's edge: the valley, the town and the far forests at once (the most instances in view)
	Seg(TEXT("overlook"), true, {
		{ Map(10, -45, 2500), FRotator(-8, 90, 0), 0.f },
		{ Map(10, -45, 2500), FRotator(-8, 45, 0), 5.f },
		{ Map(10, -45, 2500), FRotator(-8, 135, 0), 10.f } });
	// a bird's flight 25 m over the ground: over the upland's old forest, down the slope, across the town
	Seg(TEXT("flight"), true, {
		{ Map(-60, -120, 2500), FRotator(-12, 100, 0), 0.f },
		{ Map(-160, 250, 2500), FRotator(-15, 110, 0), 14.f },
		{ Map(-330, 900, 2500), FRotator(-20, 80, 0), 30.f } });
}

bool ULyudBenchmark::ForestsBaked(int32& OutTrees, int32& OutBaked) const
{
	OutTrees = OutBaked = 0;
	for (TActorIterator<AProceduralForest> It(GetWorld()); It; ++It)
	{
		OutTrees += It->NumTrees();
		OutBaked += It->NumBaked();
	}
	return OutBaked >= OutTrees;
}

void ULyudBenchmark::ApplyForestKnobs()
{
	// every knob also puts back what the forest built, so passes can follow one another in one process
	const int32 Shadows = CVarForestShadows.GetValueOnGameThread();
	const float CullM = CVarForestCull.GetValueOnGameThread();
	const bool bHide = CVarForestHide.GetValueOnGameThread() != 0;
	const int32 WpoM = CVarForestWPO.GetValueOnGameThread();
	const int32 MinLod = CVarForestMinLod.GetValueOnGameThread();
	int32 Comps = 0, Instances = 0;
	for (TActorIterator<AProceduralForest> It(GetWorld()); It; ++It)
	{
		It->SetActorHiddenInGame(bHide);
		TArray<UInstancedStaticMeshComponent*> Ism;
		It->GetComponents(Ism);
		for (UInstancedStaticMeshComponent* C : Ism)
		{
			Comps++;
			Instances += C->GetInstanceCount();
			C->SetCastShadow(Shadows != 0);
			C->SetLightingChannels(CVarForestChannel0.GetValueOnGameThread() != 0, true, false);
			C->SetCullDistances(0, CullM >= 0.f ? (int32)(CullM * 100.f) : (int32)It->CullDistance);
			C->SetWorldPositionOffsetDisableDistance(WpoM >= 0 ? WpoM * 100 : 0);
			C->bOverrideMinLOD = MinLod >= 0;
			C->MinLOD = FMath::Max(0, MinLod);
			C->MarkRenderStateDirty();
		}
	}
	for (TActorIterator<ALyudSky> It(GetWorld()); It; ++It) It->SetActorTickEnabled(CVarSkyFreeze.GetValueOnGameThread() == 0);
	UE_LOG(LogTemp, Warning, TEXT("BENCH forests: %d components, %d instances, shadows %d cull %.0f m wpo %d m minlod %d hide %d live hidden %d"),
		Comps, Instances, Shadows, CullM, WpoM, MinLod, bHide ? 1 : 0, CVarLiveHide.GetValueOnGameThread());
}

void ULyudBenchmark::ApplyPass(const FString& Cmds)
{
	TArray<FString> List;
	Cmds.ParseIntoArray(List, TEXT(";"));
	for (FString C : List)
	{
		C.TrimStartAndEndInline();
		FString Name, Arg;
		if (!C.Split(TEXT(" "), &Name, &Arg)) Name = C;
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(*Name)) Restore.Emplace(Name, V->GetString());
		Exec(C);
	}
}

void ULyudBenchmark::RestorePass()
{
	for (int32 I = Restore.Num() - 1; I >= 0; I--) Exec(Restore[I].Key + TEXT(" ") + Restore[I].Value);
	Restore.Reset();
}

void ULyudBenchmark::ApplyPreset(int32 Level)
{
	Scalability::FQualityLevels Q = Scalability::GetQualityLevels();
	Q.SetFromSingleQualityLevel(Level);
	Scalability::SetQualityLevels(Q, true);
	UE_LOG(LogTemp, Warning, TEXT("BENCH preset %d (resolution quality %.0f)"), Level, Q.ResolutionQuality);
}

void ULyudBenchmark::Exec(const FString& Cmd)
{
	if (GEngine) GEngine->Exec(GetWorld(), *Cmd);
}

double ULyudBenchmark::GroundZ(const FVector& At) const
{
	TArray<FHitResult> Hits;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(LyudBenchGround), true);
	GetWorld()->LineTraceMultiByChannel(Hits, FVector(At.X, At.Y, 60000.0), FVector(At.X, At.Y, -60000.0), ECC_Visibility, Q);
	for (const FHitResult& H : Hits)
	{
		if (Cast<ALandscapeProxy>(H.GetActor())) return H.ImpactPoint.Z;
	}
	return Hits.Num() ? Hits.Last().ImpactPoint.Z : 0.0;
}

void ULyudBenchmark::PlaceCamera(const FSegment& S, float T)
{
	if (!Camera || S.Keys.IsEmpty()) return;
	int32 I = 0;
	while (I < S.Keys.Num() - 2 && T > S.Keys[I + 1].T) I++;
	const FKey& A = S.Keys[I];
	const FKey& B = S.Keys[FMath::Min(I + 1, S.Keys.Num() - 1)];
	const float Alpha = B.T > A.T ? FMath::Clamp((T - A.T) / (B.T - A.T), 0.f, 1.f) : 0.f;
	FVector P = FMath::Lerp(A.Pos, B.Pos, Alpha);
	const FQuat R = FQuat::Slerp(A.Rot.Quaternion(), B.Rot.Quaternion(), Alpha);
	if (S.bGround)
	{
		const double Z = GroundZ(P);
		SmoothZ = bHaveZ ? FMath::Lerp(SmoothZ, Z, 0.15) : Z;
		bHaveZ = true;
		P.Z += SmoothZ;
	}
	Camera->SetActorLocationAndRotation(P, R);
	// the pawn goes along (the sky lights the player by an upward trace from the pawn's head)
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->SetActorLocation(P - FVector(0, 0, 60), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

void ULyudBenchmark::BeginSegment(int32 Index)
{
	SegIdx = Index;
	SegTime = 0.0;
	bHaveZ = false;
	Phase = bWarmupDone ? EPhase::Settle : EPhase::Warmup;
	PhaseStart = FPlatformTime::Seconds();
	PlaceCamera(Route[SegIdx], 0.f);
	if (bWarmupDone && bShots)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Benchmark") / FString::Printf(TEXT("route_p%d_%s.png"), Runs[RunIdx].Preset, *Route[SegIdx].Name), false, false);
	}
}

void ULyudBenchmark::NextSegment()
{
	if (SegIdx + 1 < Route.Num())
	{
		BeginSegment(SegIdx + 1);
		return;
	}
	if (!bWarmupDone)
	{
		bWarmupDone = true;
		UE_LOG(LogTemp, Warning, TEXT("BENCH warm-up done"));
		BeginRun(0);
		return;
	}
	FinishRun();
}

void ULyudBenchmark::BeginRun(int32 Index)
{
	RunIdx = Index;
	RestorePass();
	if (Index == 0 || Runs[Index].Preset != Runs[Index - 1].Preset) ApplyPreset(Runs[Index].Preset);
	const FPass& Pass = Passes[Runs[Index].Pass];
	ApplyPass(Pass.Cmds);
	ApplyForestKnobs();
	UE_LOG(LogTemp, Warning, TEXT("BENCH run %d / %d: preset %d, pass '%s' (%s)"), Index + 1, Runs.Num(), Runs[Index].Preset, *Pass.Name, *Pass.Cmds);
	Frames.Reset();
	Frames.SetNum(Route.Num());
	if (bCsv) Exec(TEXT("csvprofile start"));
	BeginSegment(0);
}

void ULyudBenchmark::FinishRun()
{
	if (bCsv) Exec(TEXT("csvprofile stop"));
	const int32 Level = Runs[RunIdx].Preset;
	FJson P;
	P.Num(TEXT("preset"), Level);
	static const TCHAR* Names[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Cinematic") };
	P.Str(TEXT("preset_name"), Names[FMath::Clamp(Level, 0, 4)]);
	P.Str(TEXT("pass"), Passes[Runs[RunIdx].Pass].Name);
	P.Str(TEXT("pass_setup"), Passes[Runs[RunIdx].Pass].Cmds);
	P.Num(TEXT("screen_percentage"), Scalability::GetQualityLevels().ResolutionQuality);
	TArray<FFrame> All;
	double AllSeconds = 0.0;
	FJson Segs;
	for (int32 I = 0; I < Route.Num(); I++)
	{
		double Sec = 0.0;
		for (const FFrame& F : Frames[I]) Sec += F.FrameMs / 1000.0;
		Segs.Obj(*Route[I].Name, Summarise(Frames[I], Sec));
		All.Append(Frames[I]);
		AllSeconds += Sec;
	}
	P.Obj(TEXT("overall"), Summarise(All, AllSeconds));
	P.Obj(TEXT("segments"), Segs);
	Results.Add(P.ToString());
	TArray<float> Ft, Gpu;
	for (const FFrame& F : All) { Ft.Add(F.FrameMs); Gpu.Add(F.GpuMs); }
	UE_LOG(LogTemp, Warning, TEXT("BENCH preset %d %s: %d frames, frame median %.2f p95 %.2f p99 %.2f ms, gpu median %.2f"), Level, *Passes[Runs[RunIdx].Pass].Name, All.Num(),
		Percentile(Ft, 0.5), Percentile(Ft, 0.95), Percentile(Ft, 0.99), Percentile(Gpu, 0.5));
	if (RunIdx + 1 < Runs.Num())
	{
		BeginRun(RunIdx + 1);
		return;
	}
	RestorePass();
	WriteSummary();
	Scalability::SetQualityLevels(StartLevels, true);
	Phase = EPhase::Done;
	Exec(TEXT("QUIT"));
}

void ULyudBenchmark::WriteSummary()
{
	FJson Root;
	Root.Str(TEXT("label"), Label);
	Root.Str(TEXT("date"), FDateTime::UtcNow().ToIso8601());
	Root.Str(TEXT("map"), GetWorld()->GetMapName());
	Root.Str(TEXT("game_time"), BenchTime.ToString(TEXT("%Y-%m-%d %H:%M")));
	FJson M;
	M.Str(TEXT("platform"), FPlatformProperties::IniPlatformName());
	M.Str(TEXT("os"), FPlatformMisc::GetOSVersion());
	M.Str(TEXT("device"), FPlatformMisc::GetDeviceMakeAndModel());
	M.Str(TEXT("cpu"), FPlatformMisc::GetCPUBrand().TrimStartAndEnd());
	M.Num(TEXT("cores"), FPlatformMisc::NumberOfCoresIncludingHyperthreads());
	M.Str(TEXT("gpu"), GRHIAdapterName);
	M.Str(TEXT("rhi"), GDynamicRHI ? GDynamicRHI->GetName() : TEXT("?"));
	M.Num(TEXT("ram_gb"), FPlatformMemory::GetConstants().TotalPhysicalGB);
	Root.Obj(TEXT("machine"), M);
	FIntPoint Res(0, 0);
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport) Res = GEngine->GameViewport->Viewport->GetSizeXY();
	Root.Str(TEXT("resolution"), FString::Printf(TEXT("%dx%d"), Res.X, Res.Y));
	Root.Str(TEXT("build"), LexToString(FApp::GetBuildConfiguration()));
	Root.Bool(TEXT("editor_game"), GIsEditor);
	Root.Str(TEXT("setup"), Setup);
	Root.Num(TEXT("trees"), NumTrees);
	Root.Num(TEXT("bake_seconds"), BakeSeconds);
	Root.Raw(TEXT("presets"), TEXT("[") + FString::Join(Results, TEXT(", ")) + TEXT("]"));
	const FString Text = Root.ToString();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutPath), true);
	FFileHelper::SaveStringToFile(Text, *OutPath);
	UE_LOG(LogTemp, Warning, TEXT("BENCH wrote %s"), *OutPath);
}

void ULyudBenchmark::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World) return;
	const double Now = FPlatformTime::Seconds();
	const double Dt = Now - LastTick;
	LastTick = Now;
	// the forests keep swapping live plants in as the camera moves, so this knob is applied every frame
	const bool bHideLive = CVarLiveHide.GetValueOnGameThread() != 0;
	if (Camera && (bHideLive || bLiveHidden))
	{
		for (TActorIterator<AProceduralPlant> It(World); It; ++It)
		{
			if (It->IsHidden() != bHideLive) It->SetActorHiddenInGame(bHideLive);
		}
		bLiveHidden = bHideLive;
	}
	switch (Phase)
	{
	case EPhase::WaitBake:
	{
		// the date and hour of the run: the calendar's start is moved so that now is BenchTime
		if (UGameCalendar* Cal = World->GetSubsystem<UGameCalendar>())
		{
			Cal->StartDate = BenchTime - FTimespan::FromSeconds(UGameCalendar::GameSeconds(World) * Cal->TimeScale);
		}
		int32 Trees = 0, Baked = 0;
		const bool bBaked = ForestsBaked(Trees, Baked);
		if ((bBaked && Now - PhaseStart > 3.0) || Now - PhaseStart > BakeTimeout)
		{
			BakeSeconds = Now - PhaseStart;
			NumTrees = Trees;
			UE_LOG(LogTemp, Warning, TEXT("BENCH forests baked: %d / %d trees in %.1f s"), Baked, Trees, BakeSeconds);
			FActorSpawnParameters Params;
			Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
			Camera->GetCameraComponent()->SetFieldOfView(90.f);
			Camera->GetCameraComponent()->bConstrainAspectRatio = false;
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				PC->SetViewTarget(Camera);
				if (APawn* Pawn = PC->GetPawn())
				{
					Pawn->SetActorHiddenInGame(true);
					Pawn->SetActorEnableCollision(false);
					if (ACharacter* Ch = Cast<ACharacter>(Pawn)) Ch->GetCharacterMovement()->DisableMovement();
				}
				PC->DisableInput(PC);
			}
			ApplyForestKnobs();
			TArray<FString> Cmds;
			Setup.ParseIntoArray(Cmds, TEXT(";"));
			for (const FString& C : Cmds) Exec(C.TrimStartAndEnd());
			if (bWarmupDone)
			{
				BeginRun(0);
			}
			else
			{
				ApplyPreset(Presets.Contains(3) ? 3 : Presets[0]);
				BeginSegment(0);
			}
		}
		else if (FMath::FloorToInt(Now - PhaseStart) % 10 == 0 && FMath::FloorToInt(Now - PhaseStart - Dt) % 10 != 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("BENCH baking forests: %d / %d"), Baked, Trees);
		}
		break;
	}
	case EPhase::Warmup:
	{
		SegTime += Dt * WarmupSpeed * Speed;
		PlaceCamera(Route[SegIdx], (float)SegTime);
		if (SegTime >= Route[SegIdx].Duration()) NextSegment();
		break;
	}
	case EPhase::Settle:
	{
		PlaceCamera(Route[SegIdx], 0.f);
		if (Now - PhaseStart >= SettleSeconds)
		{
			Phase = EPhase::Record;
			CSV_EVENT_GLOBAL(TEXT("%s"), *Route[SegIdx].Name);
		}
		break;
	}
	case EPhase::Record:
	{
		SegTime += Dt * Speed;
		PlaceCamera(Route[SegIdx], (float)SegTime);
		FFrame F;
		F.FrameMs = (float)(Dt * 1000.0);
		F.GameMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
		F.RenderMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
		F.RhiMs = FPlatformTime::ToMilliseconds(GRHIThreadTime);
		F.GpuMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
		F.DrawCalls = GNumDrawCallsRHI[0];
		F.Prims = GNumPrimitivesDrawnRHI[0];
		F.MemMB = (float)(FPlatformMemory::GetStats().UsedPhysical / (1024.0 * 1024.0));
		Frames[SegIdx].Add(F);
		if (SegTime >= Route[SegIdx].Duration()) NextSegment();
		break;
	}
	default:
		break;
	}
}
