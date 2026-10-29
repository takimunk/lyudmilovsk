#include "Railway/LyudRailway.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Plants/GameCalendar.h"
#include "Railway/LyudLevelCrossing.h"
#include "Railway/LyudRailTrack.h"
#include "Railway/LyudTrain.h"
#include "RailRun.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static TAutoConsoleVariable<int32> CVarRailway(TEXT("lyud.Railway"), 1,
	TEXT("The sea railway (ULyudRailwaySubsystem): 1 on in Lvl_Scene01, 0 off; 2 also in any other game level (tests)."));
static TAutoConsoleVariable<float> CVarRailwayOffset(TEXT("lyud.Railway.Clock"), 0.f,
	TEXT("Seconds added to the railway's timetable clock (tests: see the train come out without moving the calendar)."));

static FAutoConsoleCommandWithWorld CmdRailwayReport(TEXT("lyud.Railway.Report"), TEXT("Where the trains are."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (const ULyudRailwaySubsystem* R = ULyudRailwaySubsystem::Get(World))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s"), *R->Report());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdRailwayAt(TEXT("lyud.Railway.Next"),
	TEXT("lyud.Railway.Next <stop> [dir]: shifts the railway's clock so the train's doors open at that stop in 20 s (tests)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudRailwaySubsystem* R = ULyudRailwaySubsystem::Get(World);
		if (!R || !R->IsRunning() || Args.Num() < 1)
		{
			return;
		}
		const int32 Stop = R->FindStop(FName(*Args[0]));
		const int32 Dir = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 1;
		const double Now = R->NowSeconds();
		const double When = R->NextDoorsOpen(Stop, Dir >= 0 ? 1 : -1, Now);
		if (When >= 0.0)
		{
			R->SetClockOffset(R->GetClockOffset() + (When - Now) - 20.0);
			UE_LOG(LogTemp, Warning, TEXT("RAILWAY clock moved %.0f s: doors at %s in 20 s"), When - Now - 20.0, *Args[0]);
		}
	}));

ULyudRailwaySubsystem::ULyudRailwaySubsystem() = default;
ULyudRailwaySubsystem::~ULyudRailwaySubsystem() = default;

ULyudRailwaySubsystem* ULyudRailwaySubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULyudRailwaySubsystem>() : nullptr;
}

bool ULyudRailwaySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId ULyudRailwaySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudRailwaySubsystem, STATGROUP_Tickables);
}

void ULyudRailwaySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const int32 Mode = CVarRailway.GetValueOnGameThread();
	const bool bScene = InWorld.GetMapName().Contains(TEXT("Lvl_Scene01"));
	if (Mode == 0 || (!bScene && Mode < 2))
	{
		return;
	}
	bStarted = Start();
}

void ULyudRailwaySubsystem::Deinitialize()
{
	bStarted = false;
	Super::Deinitialize();
}

bool ULyudRailwaySubsystem::Start()
{
	UWorld* World = GetWorld();
	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Railway/railway_built.json"));
#if WITH_EDITOR
	// railway_built.json is made per checkout from its terrain (not committed): make it now if this one has none
	if (!FPaths::FileExists(Path))
	{
		const FString Script = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../../tools/railway.py")));
		int32 Code = -1;
		FString Out, Err;
		FPlatformProcess::ExecProcess(TEXT("/usr/bin/python3"), *FString::Printf(TEXT("\"%s\""), *Script), &Code, &Out, &Err);
		UE_LOG(LogTemp, Warning, TEXT("Railway: no railway_built.json in this checkout: ran tools/railway.py (exit %d)"), Code);
	}
#endif
	if (!FFileHelper::LoadFileToString(BuiltJson, *Path))
	{
		UE_LOG(LogTemp, Error, TEXT("Railway: OFF: no %s. Run python3 tools/railway.py (build_scene01.sh does) and play again."), *Path);
		return false;
	}
	LyudRail::FRunParams Params;
	std::string Err;
	if (!LyudRail::LoadParams(TCHAR_TO_UTF8(*BuiltJson), Params, Err))
	{
		UE_LOG(LogTemp, Error, TEXT("Railway: OFF: %s"), UTF8_TO_TCHAR(Err.c_str()));
		return false;
	}
	Run = MakePimpl<LyudRail::FRun>();
	Run->Build(Params);

	TSharedPtr<FJsonObject> J;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(BuiltJson), J) || !J.IsValid())
	{
		return false;
	}
	PortalFaceY = J->GetObjectField(TEXT("frame"))->GetNumberField(TEXT("portal_face_y"));
	const TSharedPtr<FJsonObject> Ln = J->GetObjectField(TEXT("line"));
	X0 = Ln->GetNumberField(TEXT("tunnel_start_x"));
	Step = Ln->GetNumberField(TEXT("step"));
	Length = Ln->GetNumberField(TEXT("length"));
	LineY = Ln->GetNumberField(TEXT("y"));
	Points.Reset();
	for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("points")))
	{
		const TSharedPtr<FJsonObject> P = V->AsObject();
		Points.Add({ P->GetNumberField(TEXT("s")), P->GetNumberField(TEXT("x")), P->GetNumberField(TEXT("y")), P->GetNumberField(TEXT("rail")),
			P->GetNumberField(TEXT("ground")) });
	}
	Stops.Reset();
	for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("stops")))
	{
		const TSharedPtr<FJsonObject> P = V->AsObject();
		FStopData S;
		S.Id = FName(*P->GetStringField(TEXT("id")));
		S.NameKey = P->GetStringField(TEXT("name"));
		S.S = P->GetNumberField(TEXT("s"));
		S.Side = int32(P->GetNumberField(TEXT("side")));
		const TArray<TSharedPtr<FJsonValue>>& Pl = P->GetArrayField(TEXT("platform_s"));
		S.P0 = Pl[0]->AsNumber();
		S.P1 = Pl[1]->AsNumber();
		const TArray<TSharedPtr<FJsonValue>>& En = P->GetArrayField(TEXT("entrance"));
		S.EntranceMap = FVector(En[0]->AsNumber(), En[1]->AsNumber(), En[2]->AsNumber());
		Stops.Add(S);
	}

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Track = World->SpawnActor<ALyudRailTrack>(ALyudRailTrack::StaticClass(), FTransform::Identity, SP);
	if (Track)
	{
		Track->Build(this, J);
	}
	Crossing = World->SpawnActor<ALyudLevelCrossing>(ALyudLevelCrossing::StaticClass(), FTransform::Identity, SP);
	if (Crossing)
	{
		Crossing->Setup(this, J);
	}
	Trains.Reset();
	LastInfo.Reset();
	for (int32 I = 0; I < Params.Trains; I++)
	{
		ALyudTrain* T = World->SpawnActor<ALyudTrain>(ALyudTrain::StaticClass(), FTransform::Identity, SP);
		if (T)
		{
			T->Setup(this, I);
			Trains.Add(T);
			LastInfo.AddDefaulted();
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Railway: %.1f km, %d stops, %d train(s), one way %.0f s, period %.0f s"), Length / 1000.0, Stops.Num(), Trains.Num(),
		Run->OneWaySeconds(), Run->Params().Period);
	return true;
}

double ULyudRailwaySubsystem::NowSeconds() const
{
	const UWorld* World = GetWorld();
	// seconds since the calendar's 1 January 2026 (whole days counted: the timetable keeps its minutes day after day)
	const FDateTime Now = World ? UGameCalendar::NowIn(World) : FDateTime(2026, 10, 20, 16);
	return (Now - FDateTime(2026, 1, 1)).GetTotalSeconds() + ClockOffset + CVarRailwayOffset.GetValueOnGameThread();
}

void ULyudRailwaySubsystem::StepTrain(int32 I, float DeltaTime)
{
	if (!bStarted || !Run.IsValid() || !LastInfo.IsValidIndex(I))
	{
		return;
	}
	const double Now = NowSeconds();
	const LyudRail::FTrainState St = Run->At(Now, I);
	FLyudTrainInfo Info;
	Info.S = St.S;
	Info.Speed = St.V;
	Info.Dir = St.Dir;
	Info.Stop = St.Stop;
	Info.bDoorsOpen = St.bDoorsOpen;
	Info.bVisible = St.bVisible;
	Info.bInService = St.bInService;
	const FLyudTrainInfo& Last = LastInfo[I];
	if (Info.Stop >= 0 && Last.Stop < 0)
	{
		OnTrainEvent.Broadcast(TEXT("Arrive"), I, Info.Stop);
	}
	if (Info.Stop < 0 && Last.Stop >= 0)
	{
		OnTrainEvent.Broadcast(TEXT("Depart"), I, Last.Stop);
	}
	if (Info.bDoorsOpen != Last.bDoorsOpen)
	{
		OnTrainEvent.Broadcast(Info.bDoorsOpen ? TEXT("DoorsOpen") : TEXT("DoorsClose"), I, Info.Stop >= 0 ? Info.Stop : Last.Stop);
	}
	if (Info.bVisible != Last.bVisible)
	{
		OnTrainEvent.Broadcast(Info.bVisible ? TEXT("Appear") : TEXT("Vanish"), I, -1);
	}
	LastInfo[I] = Info;
	if (ALyudTrain* T = Trains[I])
	{
		T->Place(Info, DeltaTime);
	}
}

void ULyudRailwaySubsystem::Tick(float DeltaTime)
{
	if (!bStarted || !Run.IsValid())
	{
		return;
	}
	// each train moves itself early in the frame (ALyudTrain::Tick, pre-physics, before the player's character, so
	// the character's movement base carries a rider the same frame); here only a train without its actor
	for (int32 I = 0; I < Trains.Num(); I++)
	{
		if (!Trains[I])
		{
			StepTrain(I, DeltaTime);
		}
	}
	const double Now = NowSeconds();
	if (Crossing)
	{
		Crossing->UpdateFrom(*Run, Now, DeltaTime);
	}
}

int32 ULyudRailwaySubsystem::NumStops() const
{
	return Stops.Num();
}

FName ULyudRailwaySubsystem::GetStopId(int32 Stop) const
{
	return Stops.IsValidIndex(Stop) ? Stops[Stop].Id : NAME_None;
}

int32 ULyudRailwaySubsystem::FindStop(FName Id) const
{
	return Stops.IndexOfByPredicate([Id](const FStopData& S) { return S.Id == Id; });
}

FString ULyudRailwaySubsystem::GetStopNameKey(int32 Stop) const
{
	return Stops.IsValidIndex(Stop) ? Stops[Stop].NameKey : FString();
}

FVector ULyudRailwaySubsystem::GetBoardingPoint(int32 Stop, int32 Slot) const
{
	if (!Stops.IsValidIndex(Stop))
	{
		return FVector::ZeroVector;
	}
	const FStopData& S = Stops[Stop];
	// spread along the platform's middle by the cars' doors (each car's two doors, 4.55 m either side of its middle)
	static const double DoorOffsets[] = { 8.45 + 4.55, 8.45 - 4.55, -8.45 + 4.55, -8.45 - 4.55 };
	const double Along = S.S + DoorOffsets[FMath::Abs(Slot) % 4] + ((FMath::Abs(Slot) / 4) % 3 - 1) * 0.9;
	return LineToWorld(Along, S.Side * (PlatformEdge + 0.9), PlatformOverRail);
}

FVector ULyudRailwaySubsystem::GetStopEntrance(int32 Stop) const
{
	if (!Stops.IsValidIndex(Stop))
	{
		return FVector::ZeroVector;
	}
	const FVector M = GetStopEntranceMap(Stop);
	return MapToWorld(M.X, M.Y, M.Z);
}

FVector ULyudRailwaySubsystem::GetStopEntranceMap(int32 Stop) const
{
	if (!Stops.IsValidIndex(Stop))
	{
		return FVector::ZeroVector;
	}
	return Stops[Stop].EntranceMap;
}

double ULyudRailwaySubsystem::NextDoorsOpen(int32 Stop, int32 Dir, double AfterSeconds) const
{
	return Run.IsValid() ? Run->NextDoorsOpen(Stop, Dir, AfterSeconds) : -1.0;
}

double ULyudRailwaySubsystem::RideSeconds(int32 StopA, int32 StopB, int32 Dir) const
{
	return Run.IsValid() ? Run->RideSeconds(StopA, StopB, Dir) : -1.0;
}

ALyudTrain* ULyudRailwaySubsystem::GetTrain(int32 Index) const
{
	return Trains.IsValidIndex(Index) ? Trains[Index].Get() : nullptr;
}

FVector ULyudRailwaySubsystem::MapToWorld(double X, double Y, double Z) const
{
	return FVector(X * 100.0, PortalFaceY + Y * 100.0, Z * 100.0);
}

double ULyudRailwaySubsystem::LineLength() const
{
	return Length;
}

double ULyudRailwaySubsystem::RailZ(double S) const
{
	if (!Points.Num())
	{
		return 0.0;
	}
	const double F = FMath::Clamp(S / Step, 0.0, double(Points.Num() - 1));
	const int32 I = FMath::Min(int32(F), Points.Num() - 2);
	const double T = F - I;
	return FMath::Lerp(Points[I].Rail, Points[I + 1].Rail, T) * 100.0;
}

double ULyudRailwaySubsystem::GroundZ(double S) const
{
	if (!Points.Num())
	{
		return 0.0;
	}
	const double F = FMath::Clamp(S / Step, 0.0, double(Points.Num() - 1));
	const int32 I = FMath::Min(int32(F), Points.Num() - 2);
	const double T = F - I;
	return FMath::Lerp(Points[I].Ground, Points[I + 1].Ground, T) * 100.0;
}

FVector ULyudRailwaySubsystem::LineDir(double S) const
{
	// the line runs straight along +X; its grade tilts the heading a little
	const double Dz = (RailZ(S + 2.0) - RailZ(S - 2.0)) / 400.0;
	return FVector(1.0, 0.0, Dz).GetSafeNormal();
}

FVector ULyudRailwaySubsystem::LineToWorld(double S, double Side, double Up) const
{
	// Side: metres to the left of the outbound way (+X): map +Y is north = world +Y
	const FVector W = MapToWorld(X0 + S, LineY + Side, 0.0);
	return FVector(W.X, W.Y, RailZ(S) + Up * 100.0);
}

FString ULyudRailwaySubsystem::Report() const
{
	if (!bStarted || !Run.IsValid())
	{
		return TEXT("RAILWAY off");
	}
	FString Out = FString::Printf(TEXT("RAILWAY clock %.0f (+%.0f), period %.0f s"), NowSeconds(), ClockOffset, Run->Params().Period);
	for (int32 I = 0; I < LastInfo.Num(); I++)
	{
		const FLyudTrainInfo& T = LastInfo[I];
		Out += FString::Printf(TEXT("\n  train %d: s %.1f m, %.1f m/s, dir %+d, stop %s, doors %s, %s%s"), I, T.S, T.Speed, T.Dir,
			T.Stop >= 0 ? *Stops[T.Stop].Id.ToString() : TEXT("-"), T.bDoorsOpen ? TEXT("open") : TEXT("shut"),
			T.bVisible ? TEXT("seen") : TEXT("hidden"), T.bInService ? TEXT("") : TEXT(", laid over"));
	}
	return Out;
}
