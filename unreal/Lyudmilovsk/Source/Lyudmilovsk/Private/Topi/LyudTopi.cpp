#include "Topi/LyudTopi.h"

#include "TopiModel.h"
#include "Topi/LyudTopiMind.h"
#include "Topi/LyudTopiProps.h"

#include "Async/Async.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Containers/Queue.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Plants/ProceduralGrass.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sky/LyudSky.h"

#include <atomic>

namespace
{
	TAutoConsoleVariable<int32> CVarTopi(TEXT("lyud.Topi"), 1, TEXT("0: no Topi (the dune land is hidden and builds nothing)."));
	TAutoConsoleVariable<float> CVarTopiNight(TEXT("lyud.Topi.Night"), -1.f, TEXT("Pin Topi's night 0..1 (1: the river Ural shows); negative: by the sun."));
	TAutoConsoleVariable<int32> CVarTopiDebug(TEXT("lyud.Topi.Debug"), 0, TEXT("1: print Topi's state on screen."));
	TAutoConsoleVariable<float> CVarTopiBudget(TEXT("lyud.Topi.BudgetMs"), 2.f, TEXT("Game-thread milliseconds a frame for turning built chunks of Topi's ground into meshes."));
	TAutoConsoleVariable<int32> CVarTopiWorkers(TEXT("lyud.Topi.Workers"), 3, TEXT("How many chunks of Topi's ground are built at once on worker threads."));
	TAutoConsoleVariable<float> CVarTopiHeave(TEXT("lyud.Topi.Heave"), 1.f, TEXT("Scale of the far dunes' slow heave (0: still)."));
	TAutoConsoleVariable<int32> CVarTopiGrass(TEXT("lyud.Topi.Grass"), 1, TEXT("0: no marram grass on Topi's dunes."));
	FAutoConsoleCommandWithWorld CmdTopiReport(TEXT("lyud.Topi.Report"), TEXT("Logs Topi's state (the lyud.Topi.Debug line) once: TOPIREPORT ..."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			const ALyudTopi* Topi = ALyudTopi::Get(World);
			UE_LOG(LogTemp, Display, TEXT("TOPIREPORT %s"), Topi ? *Topi->DebugReport() : TEXT("no Topi in this world"));
		}));

	const int32 TopiChunkCells = 64;
	const int32 TopiTopLevel = 8;          // 128 m * 2^8 = 32.8 km roots

	uint64 TopiChunkKey(int32 Level, int32 IX, int32 IY)
	{
		return (uint64(Level) << 56) | (uint64(uint32(IX + (1 << 24))) << 28) | uint64(uint32(IY + (1 << 24)));
	}

	/** One chunk's mesh, made on a worker thread. */
	struct FTopiChunkData
	{
		uint64 Key = 0;
		int32 Level = 0, IX = 0, IY = 0;
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FLinearColor> Colors;
		/** Mean marram density in the chunk's four quarters (x then y). */
		float Grass[4] = { 0.f, 0.f, 0.f, 0.f };
	};

	/** What the workers share with the actor: it outlives the actor while a build is in flight. */
	struct FTopiShared
	{
		LyudTopi::FModel Model;
		TQueue<TSharedPtr<FTopiChunkData>, EQueueMode::Mpsc> Done;
		std::atomic<int32> InFlight{ 0 };
		std::atomic<bool> bStop{ false };
	};

	TSharedPtr<FTopiChunkData> BuildTopiChunk(const LyudTopi::FModel& Model, int32 Level, int32 IX, int32 IY, double FinestM)
	{
		const double Size = FinestM * double(1 << Level);
		const double Cell = Size / TopiChunkCells;
		const double X0 = IX * Size, Y0 = IY * Size;
		// the far ground leaves out the dunes too small for its cells
		const double LodCell = Level == 0 ? 0.0 : Cell;
		const int32 N = TopiChunkCells + 1;         // vertices a side
		const int32 G = N + 2;                  // with one more ring for the normals

		TArray<LyudTopi::FSample> Samples;
		Samples.SetNumUninitialized(G * G);
		for (int32 J = 0; J < G; ++J)
		{
			for (int32 I = 0; I < G; ++I)
			{
				Model.Sample(X0 + (I - 1) * Cell, Y0 + (J - 1) * Cell, LodCell, Samples[J * G + I]);
			}
		}

		TSharedPtr<FTopiChunkData> Data = MakeShared<FTopiChunkData>();
		Data->Key = TopiChunkKey(Level, IX, IY);
		Data->Level = Level;
		Data->IX = IX;
		Data->IY = IY;
		const int32 SkirtVerts = 4 * N;
		Data->Vertices.Reserve(N * N + SkirtVerts);
		Data->Normals.Reserve(N * N + SkirtVerts);
		Data->Colors.Reserve(N * N + SkirtVerts);
		Data->Triangles.Reserve(TopiChunkCells * TopiChunkCells * 6 + 4 * TopiChunkCells * 12);

		double GrassSum[4] = { 0.0, 0.0, 0.0, 0.0 };
		for (int32 J = 0; J < N; ++J)
		{
			for (int32 I = 0; I < N; ++I)
			{
				const LyudTopi::FSample& S = Samples[(J + 1) * G + (I + 1)];
				const double HX = (Samples[(J + 1) * G + (I + 2)].Z - Samples[(J + 1) * G + I].Z) / (2.0 * Cell);
				const double HY = (Samples[(J + 2) * G + (I + 1)].Z - Samples[J * G + (I + 1)].Z) / (2.0 * Cell);
				Data->Vertices.Add(FVector(I * Cell * 100.0, J * Cell * 100.0, S.Z * 100.0));
				Data->Normals.Add(FVector(-HX, -HY, 1.0).GetSafeNormal());
				Data->Colors.Add(FLinearColor(float(S.Grass), float(S.River), 0.f, float(S.Region)));
				GrassSum[(I * 2 / N) + 2 * (J * 2 / N)] += S.Grass;
			}
		}
		for (int32 Q = 0; Q < 4; ++Q)
		{
			Data->Grass[Q] = float(GrassSum[Q] / (N * N / 4.0));
		}
		// (origin, +Y, +X) faces up
		for (int32 J = 0; J < TopiChunkCells; ++J)
		{
			for (int32 I = 0; I < TopiChunkCells; ++I)
			{
				const int32 A = J * N + I, B = A + 1, C = A + N, D = C + 1;
				Data->Triangles.Append({ A, C, B, B, C, D });
			}
		}
		// a skirt round the edge hides the cracks between chunks of different sizes
		const double Drop = FMath::Max(150.0, Size * 100.0 * 0.03);
		const auto Skirt = [&](int32 StartI, int32 StartJ, int32 StepI, int32 StepJ)
		{
			const int32 First = Data->Vertices.Num();
			for (int32 K = 0; K < N; ++K)
			{
				const int32 Src = (StartJ + K * StepJ) * N + (StartI + K * StepI);
				const FVector P = Data->Vertices[Src];
				Data->Vertices.Add(FVector(P.X, P.Y, P.Z - Drop));
				Data->Normals.Add(FVector(Data->Normals[Src]));
				Data->Colors.Add(FLinearColor(Data->Colors[Src]));
			}
			for (int32 K = 0; K < TopiChunkCells; ++K)
			{
				const int32 A = (StartJ + K * StepJ) * N + (StartI + K * StepI);
				const int32 B = (StartJ + (K + 1) * StepJ) * N + (StartI + (K + 1) * StepI);
				const int32 C = First + K, D = First + K + 1;
				// both ways round: whichever side the eye is on
				Data->Triangles.Append({ A, B, C, B, D, C, A, C, B, B, C, D });
			}
		};
		Skirt(0, 0, 1, 0);
		Skirt(0, N - 1, 1, 0);
		Skirt(0, 0, 0, 1);
		Skirt(N - 1, 0, 0, 1);
		return Data;
	}

	float TopiSmooth(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}
}

struct ALyudTopi::FState
{
	struct FChunk
	{
		int32 Level = 0, IX = 0, IY = 0;
		bool bBuilding = false;
		bool bShown = false;
		uint8 GrassNext = 0;            // the next quarter to grow grass on (4: done)
		uint32 Touched = 0;
		double ShownAt = 0.0;
		float Grass[4] = { 0.f, 0.f, 0.f, 0.f };
		TWeakObjectPtr<UProceduralMeshComponent> Mesh;
		TArray<TWeakObjectPtr<AProceduralGrass>> GrassPatches;
	};

	struct FRequest
	{
		uint64 Key;
		int32 Level, IX, IY;
		double Distance;
	};

	TSharedPtr<FTopiShared> Shared;
	TMap<uint64, FChunk> Chunks;
	TArray<uint64> Show;
	TSet<uint64> Shown;
	TArray<FRequest> Requests;
	TWeakObjectPtr<ALyudSky> Sky;
	FVector2D LastEye = FVector2D(1.0e12, 1.0e12);
	uint32 Round = 0;
	bool bDirty = true;
	bool bModelLoaded = false;
	bool bRiverBuilt = false;
	bool bRiverShown = false;
	FLinearColor SandLook = FLinearColor(-1.f, -1.f, -1.f, -1.f);
	FLinearColor RiverLook = FLinearColor(-1.f, -1.f, -1.f, -1.f);
	int32 ReadyChunks = 0;
};

ALyudTopi::ALyudTopi()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);
	Mind = CreateDefaultSubobject<ULyudTopiMind>(TEXT("Mind"));
	Props = CreateDefaultSubobject<ULyudTopiProps>(TEXT("Props"));
	State = MakePimpl<FState>();
	State->Shared = MakeShared<FTopiShared>();
}

ALyudTopi::~ALyudTopi()
{
}

ALyudTopi* ALyudTopi::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const ULyudTopiSubsystem* Sub = World ? World->GetSubsystem<ULyudTopiSubsystem>() : nullptr;
	return Sub ? Sub->GetTopi() : nullptr;
}

const LyudTopi::FModel& ALyudTopi::Model() const
{
	return State->Shared->Model;
}

float ALyudTopi::SandZ(const FVector& World) const
{
	const FVector2D M = ToMap(World);
	return float(Model().Height(M.X, M.Y) * 100.0);
}

float ALyudTopi::DepthInto(const FVector& World) const
{
	const FVector2D M = ToMap(World);
	return float(LyudTopi::FModel::Depth(M.X, M.Y));
}

float ALyudTopi::RiverDepthAt(const FVector& World) const
{
	if (NightNow < 0.5f)
	{
		return 0.f;
	}
	const FVector2D M = ToMap(World);
	return float(Model().RiverDepth(M.X, M.Y) * 100.0);
}

FVector ALyudTopi::RiverFlowAt(const FVector& World) const
{
	const FVector2D M = ToMap(World);
	const LyudTopi::FVec2 F = LyudTopi::FModel::RiverFlow(M.X, M.Y);
	return FVector(F.X * 100.0, F.Y * 100.0, 0.0);
}

float ALyudTopi::RiverSurfaceZ() const
{
	return float(LyudTopi::RiverZ * 100.0);
}

void ALyudTopi::BeginPlay()
{
	Super::BeginPlay();

	// the map's frame (the same file the M map and the quests read)
	FString Text;
	if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("UI/map.json"))))
	{
		TSharedPtr<FJsonObject> Json;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		const TSharedPtr<FJsonObject>* Frame = nullptr;
		if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid() && Json->TryGetObjectField(TEXT("frame"), Frame))
		{
			(*Frame)->TryGetNumberField(TEXT("portal_face_y"), PortalFaceY);
		}
	}

	TArray<uint8> Bytes;
	LyudTopi::FModel& M = State->Shared->Model;
	if (FFileHelper::LoadFileToArray(Bytes, *(FPaths::ProjectContentDir() / TEXT("Topi/topi_edge.tbl"))) && M.Edge.Read(Bytes.GetData(), size_t(Bytes.Num())))
	{
		M.Prepare();
		State->bModelLoaded = true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("LyudTopi: no Content/Topi/topi_edge.tbl (python3 tools/topi.py): Topi is not built"));
	}

	if (!SandMaterial)
	{
		SandMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Topi/M_TopiSand.M_TopiSand"));
	}
	if (!RiverMaterial)
	{
		RiverMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Topi/M_TopiRiver.M_TopiRiver"));
	}
	if (!GlowMaterial)
	{
		GlowMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Topi/M_TopiGlow.M_TopiGlow"));
	}
	if (!MotesMaterial)
	{
		MotesMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Topi/M_TopiMotes.M_TopiMotes"));
	}
	if (!SandMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("LyudTopi: no /Game/Topi/M_TopiSand (unreal/scripts/make_topi_materials.py): the sand has the default material"));
	}
	const auto MakeMID = [this](UMaterialInterface* Base) -> UMaterialInstanceDynamic*
	{
		return Base ? UMaterialInstanceDynamic::Create(Base->GetMaterial(), this) : nullptr;
	};
	SandMID = MakeMID(SandMaterial);
	RiverMID = MakeMID(RiverMaterial);
	GlowMID = MakeMID(GlowMaterial);
	MotesMID = MakeMID(MotesMaterial);
}

void ALyudTopi::EndPlay(const EEndPlayReason::Type Reason)
{
	State->Shared->bStop = true;
	for (TPair<uint64, FState::FChunk>& Pair : State->Chunks)
	{
		for (const TWeakObjectPtr<AProceduralGrass>& Patch : Pair.Value.GrassPatches)
		{
			if (Patch.IsValid())
			{
				Patch->Destroy();
			}
		}
	}
	State->Chunks.Reset();
	Super::EndPlay(Reason);
}

void ALyudTopi::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const bool bOff = CVarTopi.GetValueOnGameThread() == 0 || !State->bModelLoaded;
	if (bOff != bHidden)
	{
		bHidden = bOff;
		SetActorHiddenInGame(bOff);
		SetActorEnableCollision(!bOff);
	}
	if (bOff)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();

	FinishBuilt(double(CVarTopiBudget.GetValueOnGameThread()));
	UpdateChunks(Eye);
	if (!State->bRiverBuilt)
	{
		BuildRiver();
	}
	UpdateLook(DeltaSeconds, Eye);
	HoldHero();

	if (CVarTopiDebug.GetValueOnGameThread() != 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(uint64(GetUniqueID()), 0.f, FColor::Cyan, DebugReport());
	}
}

FString ALyudTopi::DebugReport() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const FVector Eye = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	const FVector2D M = ToMap(Eye);
	return FString::Printf(TEXT("TOPI map %.0f %.0f  into %.0f m  sand %.1f m  night %.2f  unrest %.2f  chunks %d shown / %d kept, %d building  path %.0f m  river %.0f m"),
		M.X, M.Y, LyudTopi::FModel::Depth(M.X, M.Y), Model().Height(M.X, M.Y), NightNow, UnrestShown,
		State->Shown.Num(), State->ReadyChunks, State->Shared->InFlight.load(),
		LyudTopi::FModel::PathDistance(M.X, M.Y), LyudTopi::FModel::RiverOffset(M.X, M.Y));
}

bool ALyudTopi::ResolveChunk(int32 Level, int32 IX, int32 IY, const FVector2D& EyeMap, TArray<uint64>& OutShow)
{
	const double Size = double(FinestChunkM) * double(1 << Level);
	const double X0 = IX * Size, Y0 = IY * Size;
	// nothing to draw: all west of the sand (Topi lies along the map's east edge and goes on east: toward -X)
	if (!LyudTopi::FModel::Near(X0, Y0, 0.0))
	{
		return true;
	}
	const double DX = FMath::Max3(X0 - EyeMap.X, 0.0, EyeMap.X - (X0 + Size));
	const double DY = FMath::Max3(Y0 - EyeMap.Y, 0.0, EyeMap.Y - (Y0 + Size));
	const double Distance = FMath::Sqrt(DX * DX + DY * DY);
	if (Distance > double(HorizonM))
	{
		return true;
	}

	const uint64 Key = TopiChunkKey(Level, IX, IY);
	if (Level > 0 && Distance < double(SplitDistance) * Size)
	{
		const int32 Mark = OutShow.Num();
		bool bAll = true;
		for (int32 Q = 0; Q < 4; ++Q)
		{
			bAll &= ResolveChunk(Level - 1, IX * 2 + (Q & 1), IY * 2 + (Q >> 1), EyeMap, OutShow);
		}
		if (bAll)
		{
			return true;
		}
		// not all four are there yet: this one stands in for them
		OutShow.SetNum(Mark, EAllowShrinking::No);
	}

	FState::FChunk& Chunk = State->Chunks.FindOrAdd(Key);
	Chunk.Level = Level;
	Chunk.IX = IX;
	Chunk.IY = IY;
	Chunk.Touched = State->Round;
	if (Chunk.Mesh.IsValid())
	{
		OutShow.Add(Key);
		return true;
	}
	if (!Chunk.bBuilding)
	{
		State->Requests.Add({ Key, Level, IX, IY, Distance - Size * 0.25 * Level });
	}
	// the eye has moved off and this chunk's four finer ones are still there: they stand in for it until it is built
	if (Level > 0)
	{
		const int32 Mark = OutShow.Num();
		bool bAll = true;
		for (int32 Q = 0; Q < 4 && bAll; ++Q)
		{
			const int32 CX = IX * 2 + (Q & 1), CY = IY * 2 + (Q >> 1);
			const double Half = Size * 0.5;
			if (!LyudTopi::FModel::Near(CX * Half, CY * Half, 0.0))
			{
				continue;
			}
			const uint64 ChildKey = TopiChunkKey(Level - 1, CX, CY);
			FState::FChunk* Child = State->Chunks.Find(ChildKey);
			if (Child && Child->Mesh.IsValid())
			{
				Child->Touched = State->Round;
				OutShow.Add(ChildKey);
			}
			else
			{
				bAll = false;
			}
		}
		if (bAll)
		{
			return true;
		}
		OutShow.SetNum(Mark, EAllowShrinking::No);
	}
	return false;
}

void ALyudTopi::UpdateChunks(const FVector& Eye)
{
	const FVector2D EyeMap = ToMap(Eye);
	// far from Topi with nothing changed there is nothing to do; near it, look again every few metres
	if (!State->bDirty && FVector2D::DistSquared(EyeMap, State->LastEye) < 16.0)
	{
		return;
	}
	State->LastEye = EyeMap;
	State->bDirty = false;
	++State->Round;

	State->Show.Reset();
	State->Requests.Reset();
	const double RootSize = double(FinestChunkM) * double(1 << TopiTopLevel);
	const int32 RX0 = FMath::FloorToInt32((EyeMap.X - HorizonM) / RootSize), RX1 = FMath::FloorToInt32((EyeMap.X + HorizonM) / RootSize);
	const int32 RY0 = FMath::FloorToInt32((EyeMap.Y - HorizonM) / RootSize), RY1 = FMath::FloorToInt32((EyeMap.Y + HorizonM) / RootSize);
	for (int32 RY = RY0; RY <= RY1; ++RY)
	{
		for (int32 RX = RX0; RX <= RX1; ++RX)
		{
			ResolveChunk(TopiTopLevel, RX, RY, EyeMap, State->Show);
		}
	}

	// show what was chosen, hide the rest
	TSet<uint64> Now;
	Now.Reserve(State->Show.Num());
	const double Time = GetWorld()->GetTimeSeconds();
	for (const uint64 Key : State->Show)
	{
		Now.Add(Key);
		FState::FChunk& Chunk = State->Chunks.FindChecked(Key);
		if (!Chunk.bShown && Chunk.Mesh.IsValid())
		{
			Chunk.bShown = true;
			Chunk.ShownAt = Time;
			Chunk.Mesh->SetVisibility(true);
			if (Chunk.Level == 0)
			{
				Chunk.Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			}
		}
	}
	for (const uint64 Key : State->Shown)
	{
		if (!Now.Contains(Key))
		{
			if (FState::FChunk* Chunk = State->Chunks.Find(Key))
			{
				Chunk->bShown = false;
				if (Chunk->Mesh.IsValid())
				{
					Chunk->Mesh->SetVisibility(false);
					Chunk->Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}
			}
		}
	}
	State->Shown = MoveTemp(Now);

	// build the nearest missing chunks first
	const int32 Free = CVarTopiWorkers.GetValueOnGameThread() - State->Shared->InFlight.load();
	if (Free > 0 && State->Requests.Num() > 0)
	{
		State->Requests.Sort([](const FState::FRequest& A, const FState::FRequest& B) { return A.Distance < B.Distance; });
		for (int32 R = 0; R < FMath::Min(Free, State->Requests.Num()); ++R)
		{
			const FState::FRequest Req = State->Requests[R];
			State->Chunks.FindChecked(Req.Key).bBuilding = true;
			++State->Shared->InFlight;
			const TSharedPtr<FTopiShared> Shared = State->Shared;
			const double Finest = double(FinestChunkM);
			Async(EAsyncExecution::ThreadPool, [Shared, Req, Finest]
			{
				if (!Shared->bStop)
				{
					Shared->Done.Enqueue(BuildTopiChunk(Shared->Model, Req.Level, Req.IX, Req.IY, Finest));
				}
				--Shared->InFlight;
			});
		}
	}
	if (State->Requests.Num() > 0)
	{
		State->bDirty = true;       // more to ask for when a worker is free
	}

	// free the meshes nobody has wanted for a while (the least recently wanted first)
	State->ReadyChunks = 0;
	TArray<TPair<uint32, uint64>> Idle;
	for (const TPair<uint64, FState::FChunk>& Pair : State->Chunks)
	{
		if (Pair.Value.Mesh.IsValid())
		{
			++State->ReadyChunks;
			if (!Pair.Value.bShown && Pair.Value.Touched + 2 < State->Round)
			{
				Idle.Add(TPair<uint32, uint64>(Pair.Value.Touched, Pair.Key));
			}
		}
		else if (!Pair.Value.bBuilding && Pair.Value.Touched + 2 < State->Round)
		{
			Idle.Add(TPair<uint32, uint64>(0u, Pair.Key));
		}
	}
	if (Idle.Num() > SpareChunks)
	{
		Idle.Sort([](const TPair<uint32, uint64>& A, const TPair<uint32, uint64>& B) { return A.Key < B.Key; });
		for (int32 K = 0; K < Idle.Num() - SpareChunks; ++K)
		{
			DropChunk(Idle[K].Value);
		}
	}
}

void ALyudTopi::DropChunk(uint64 Key)
{
	FState::FChunk* Chunk = State->Chunks.Find(Key);
	if (!Chunk)
	{
		return;
	}
	if (Chunk->Mesh.IsValid())
	{
		Chunk->Mesh->ClearAllMeshSections();
		Chunk->Mesh->DestroyComponent();
	}
	for (const TWeakObjectPtr<AProceduralGrass>& Patch : Chunk->GrassPatches)
	{
		if (Patch.IsValid())
		{
			Patch->Destroy();
		}
	}
	State->Shown.Remove(Key);
	State->Chunks.Remove(Key);
}

void ALyudTopi::FinishBuilt(double BudgetMs)
{
	const double Start = FPlatformTime::Seconds();
	TSharedPtr<FTopiChunkData> Data;
	while (State->Shared->Done.Dequeue(Data))
	{
		State->bDirty = true;
		FState::FChunk* Chunk = State->Chunks.Find(Data->Key);
		if (!Chunk)
		{
			continue;       // dropped while it was being built
		}
		Chunk->bBuilding = false;
		const double Size = double(FinestChunkM) * double(1 << Data->Level);
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
		Mesh->SetMobility(EComponentMobility::Static);
		Mesh->bUseAsyncCooking = true;
		Mesh->SetupAttachment(Root);
		Mesh->SetRelativeLocation(ToWorld(Data->IX * Size, Data->IY * Size, 0.0));
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->LightingChannels.bChannel0 = true;
		Mesh->LightingChannels.bChannel1 = true;
		// only the ground near the eye throws shadows; the far dunes' shading is in their normals
		Mesh->SetCastShadow(Data->Level <= 2);
		// the sand's material has a world position offset (the far heave): without this the shadow cache under every
		// near chunk is redrawn each frame. The heave is far and slow; shadows of the unheaved shape are right.
		Mesh->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Static;
		Mesh->bAffectDistanceFieldLighting = false;
		// the rain's wet patches and puddles are decals: the sand drains, it takes none
		Mesh->bReceivesDecals = false;
		Mesh->SetVisibility(false);
		// the far dunes heave in the material: leave room for it in the bounds
		Mesh->SetBoundsScale(Data->Level >= 2 ? 1.25f : 1.f);
		Mesh->RegisterComponent();
		Mesh->CreateMeshSection_LinearColor(0, Data->Vertices, Data->Triangles, Data->Normals, TArray<FVector2D>(), Data->Colors, TArray<FProcMeshTangent>(), Data->Level == 0, false);
		if (SandMID)
		{
			Mesh->SetMaterial(0, SandMID);
		}
		Chunk->Mesh = Mesh;
		for (int32 Q = 0; Q < 4; ++Q)
		{
			Chunk->Grass[Q] = Data->Grass[Q];
		}
		if ((FPlatformTime::Seconds() - Start) * 1000.0 > BudgetMs)
		{
			break;
		}
	}

	// marram on the finest chunks, once their collision has had time to cook (the tufts are traced onto it)
	if (bGrass && CVarTopiGrass.GetValueOnGameThread() != 0)
	{
		// one patch (one Scatter: traces and thousands of instances) a frame, only near the eye (the tufts are culled at
		// 90 m), and the far ones let go again
		const double Time = GetWorld()->GetTimeSeconds();
		const double Size = double(FinestChunkM);
		const double Reach = 220.0;
		for (const uint64 Key : State->Shown)
		{
			FState::FChunk& Chunk = State->Chunks.FindChecked(Key);
			if (Chunk.Level != 0)
			{
				continue;
			}
			const double DX = FMath::Max3(Chunk.IX * Size - State->LastEye.X, 0.0, State->LastEye.X - (Chunk.IX + 1) * Size);
			const double DY = FMath::Max3(Chunk.IY * Size - State->LastEye.Y, 0.0, State->LastEye.Y - (Chunk.IY + 1) * Size);
			const double D2 = DX * DX + DY * DY;
			if (Chunk.GrassNext < 4 && Chunk.bShown && Time - Chunk.ShownAt > 2.0 && D2 < Reach * Reach)
			{
				GrowGrass(Key, Chunk.GrassNext++);
				break;
			}
			if (Chunk.GrassPatches.Num() > 0 && D2 > 4.0 * Reach * Reach)
			{
				for (const TWeakObjectPtr<AProceduralGrass>& Patch : Chunk.GrassPatches)
				{
					if (Patch.IsValid())
					{
						Patch->Destroy();
					}
				}
				Chunk.GrassPatches.Reset();
				Chunk.GrassNext = 0;
				break;
			}
		}
	}
}

void ALyudTopi::GrowGrass(uint64 Key, int32 Q)
{
	FState::FChunk& Chunk = State->Chunks.FindChecked(Key);
	const double Size = double(FinestChunkM);
	{
		if (Chunk.Grass[Q] < 0.04f)
		{
			return;
		}
		const double CX = (Chunk.IX + 0.25 + 0.5 * (Q & 1)) * Size, CY = (Chunk.IY + 0.25 + 0.5 * (Q >> 1)) * Size;
		const FVector At = ToWorld(CX, CY, Model().Height(CX, CY));
		AProceduralGrass* Patch = GetWorld()->SpawnActorDeferred<AProceduralGrass>(AProceduralGrass::StaticClass(), FTransform(At), this, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Patch)
		{
			return;
		}
		Patch->Species = GrassSpecies;
		Patch->Seed = int32(Key & 0x7fffff) + Q;
		Patch->HalfExtent = FVector2D(Size * 25.0, Size * 25.0);
		Patch->DensityScale = FMath::Clamp(Chunk.Grass[Q] * 1.7f, 0.05f, 2.f);   // tussocks that read at eye level
		Patch->bUseTerrainMask = false;      // the mask covers only the map's own landscape
		Patch->MinSlopeNormalZ = 0.8f;       // none on the slip faces
		Patch->MinWorldZ = float((LyudTopi::RiverZ + 0.6) * 100.0);
		Patch->TraceUp = 6000.f;
		Patch->TraceDown = 6000.f;
		Patch->SetFlags(RF_Transient);
		Patch->FinishSpawning(FTransform(At));
		Chunk.GrassPatches.Add(Patch);
	}
}

void ALyudTopi::BuildRiver()
{
	State->bRiverBuilt = true;
	const LyudTopi::FModel& M = Model();
	const TArray<FProcMeshTangent> NoTangents;
	const LyudTopi::FVec2 Mid0 = LyudTopi::FModel::RiverPoint(0.0);
	const FVector Origin = ToWorld(Mid0.X, Mid0.Y, LyudTopi::RiverZ);
	// S metres downstream of where the path comes out: the middle of the stream, and the unit vector across it
	// that points back toward the town
	const auto Stream = [](double S, FVector2D& OutSide) -> FVector2D
	{
		const LyudTopi::FVec2 P = LyudTopi::FModel::RiverPoint(S);
		const LyudTopi::FVec2 D = LyudTopi::FModel::RiverDirection(S);
		OutSide = FVector2D(-D.Y, D.X);
		return FVector2D(P.X, P.Y);
	};
	const auto MakeMesh = [this, &Origin](const TCHAR* Name, UMaterialInstanceDynamic* MID, bool bFar) -> UProceduralMeshComponent*
	{
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, Name, RF_Transient);
		Mesh->SetMobility(EComponentMobility::Static);
		Mesh->SetupAttachment(Root);
		Mesh->SetRelativeLocation(Origin);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetCastShadow(false);
		Mesh->bAffectDistanceFieldLighting = false;
		Mesh->bVisibleInRayTracing = false;
		Mesh->SetVisibility(false);
		Mesh->SetBoundsScale(bFar ? 1.f : 1.5f);
		Mesh->RegisterComponent();
		if (MID)
		{
			Mesh->SetMaterial(0, MID);
		}
		return Mesh;
	};

	// what is drawn of the river (it has no end): 7 km either way of where the path comes out, fading at both ends.
	// The strips are laid from downstream up.
	const double Reach = 7000.0;
	const double Step = 16.0;
	const int32 Steps = int32(2.0 * Reach / Step);
	const auto Local = [&](double X, double Y, double Z) { return ToWorld(X, Y, Z) - Origin; };

	// the water: a smooth ribbon of light. UV0 = (metres along, -1..1 across), colour a = how much river
	{
		TArray<FVector> V;
		TArray<int32> T;
		TArray<FVector> N;
		TArray<FVector2D> UV;
		TArray<FLinearColor> C;
		const int32 Across = 5;
		const double Half = LyudTopi::RiverHalfWidth + 1.5;
		for (int32 S = 0; S <= Steps; ++S)
		{
			const double Down = Reach - S * Step;
			FVector2D Side;
			const FVector2D Mid = Stream(Down, Side);
			const float K = TopiSmooth(float(S) / 60.f) * (1.f - TopiSmooth(float(S - Steps + 60) / 60.f));
			for (int32 A = 0; A < Across; ++A)
			{
				const double W = -1.0 + 2.0 * A / (Across - 1);
				V.Add(Local(Mid.X + Side.X * W * Half, Mid.Y + Side.Y * W * Half, LyudTopi::RiverZ));
				N.Add(FVector::UpVector);
				UV.Add(FVector2D(-Down, W));
				C.Add(FLinearColor(0.f, 0.f, 0.f, K));
			}
		}
		for (int32 S = 0; S < Steps; ++S)
		{
			for (int32 A = 0; A + 1 < Across; ++A)
			{
				const int32 I0 = S * Across + A, I1 = I0 + 1, I2 = I0 + Across, I3 = I2 + 1;
				T.Append({ I0, I1, I2, I1, I3, I2 });
			}
		}
		River = MakeMesh(TEXT("River"), RiverMID, false);
		River->TranslucencySortPriority = 2;
		River->CreateMeshSection_LinearColor(0, V, T, N, UV, C, NoTangents, false, false);
	}

	// the glow over it, seen from far off: a curtain standing on the river. UV0 = (metres along, 0 foot .. 1 top)
	{
		TArray<FVector> V;
		TArray<int32> T;
		TArray<FVector> N;
		TArray<FVector2D> UV;
		TArray<FLinearColor> C;
		const double GStep = 64.0;
		const int32 GSteps = int32(2.0 * Reach / GStep);
		const double Heights[3] = { 0.0, 90.0, 520.0 };
		for (int32 S = 0; S <= GSteps; ++S)
		{
			const double Down = Reach - S * GStep;
			FVector2D Side;
			const FVector2D Mid = Stream(Down, Side);
			const float K = TopiSmooth(float(S) / 30.f) * (1.f - TopiSmooth(float(S - GSteps + 30) / 30.f));
			for (int32 H = 0; H < 3; ++H)
			{
				V.Add(Local(Mid.X, Mid.Y, LyudTopi::RiverZ + Heights[H]));
				N.Add(FVector(Side.X, Side.Y, 0.0));
				UV.Add(FVector2D(-Down, Heights[H] / Heights[2]));
				C.Add(FLinearColor(0.f, 0.f, 0.f, K));
			}
		}
		for (int32 S = 0; S < GSteps; ++S)
		{
			for (int32 H = 0; H < 2; ++H)
			{
				const int32 I0 = S * 3 + H, I1 = I0 + 1, I2 = I0 + 3, I3 = I2 + 1;
				T.Append({ I0, I1, I2, I1, I3, I2 });
			}
		}
		Glow = MakeMesh(TEXT("Glow"), GlowMID, true);
		Glow->TranslucencySortPriority = 1;
		Glow->CreateMeshSection_LinearColor(0, V, T, N, UV, C, NoTangents, false, false);
	}

	// motes of light: over the water near where the path comes out, and blown along the path from the river.
	// Every mote is a quad with all four corners at its place; the material opens it toward the eye and carries it
	// along. UV0 = corner (-1..1), colour = (speed, phase, size, 0: drifts with the wind / 1: rises from the water /
	// 0.5: streams on the water with the current, whose direction is in the normal)
	{
		TArray<FVector> V;
		TArray<int32> T;
		TArray<FVector> N;
		TArray<FVector2D> UV;
		TArray<FLinearColor> C;
		FRandomStream Rand(19570104);
		const auto Mote = [&](double X, double Y, double Z, float Kind, const FVector& Normal)
		{
			const int32 First = V.Num();
			const FVector P = Local(X, Y, Z);
			const FLinearColor Seed(Rand.FRand(), Rand.FRand(), Rand.FRand(), Kind);
			static const FVector2D Corners[4] = { FVector2D(-1.0, -1.0), FVector2D(1.0, -1.0), FVector2D(1.0, 1.0), FVector2D(-1.0, 1.0) };
			for (const FVector2D& Corner : Corners)
			{
				V.Add(P);
				N.Add(Normal);
				UV.Add(Corner);
				C.Add(Seed);
			}
			T.Append({ First, First + 1, First + 2, First, First + 2, First + 3 });
		};
		// over the river, 260 m either way of the path's end
		for (int32 K = 0; K < 2600; ++K)
		{
			FVector2D Side;
			const FVector2D Mid = Stream((Rand.FRand() * 2.0 - 1.0) * 260.0, Side);
			const double W = (Rand.FRand() * 2.0 - 1.0) * (LyudTopi::RiverHalfWidth + 6.0);
			Mote(Mid.X + Side.X * W, Mid.Y + Side.Y * W, LyudTopi::RiverZ + 0.05 + Rand.FRand() * Rand.FRand() * 3.5, 1.f, FVector::UpVector);
		}
		// the river made of points of light: tiny sparks streaming on the water with the current, 150 m either way of the
		// path's end (the material carries each downstream at the stream's speed and fades it in and out)
		for (int32 K = 0; K < 3500; ++K)
		{
			const double Down = (Rand.FRand() * 2.0 - 1.0) * 150.0;
			FVector2D Side;
			const FVector2D Mid = Stream(Down, Side);
			const LyudTopi::FVec2 D = LyudTopi::FModel::RiverDirection(Down);
			const double W = (Rand.FRand() * 2.0 - 1.0) * LyudTopi::RiverHalfWidth * 0.95;
			Mote(Mid.X + Side.X * W, Mid.Y + Side.Y * W, LyudTopi::RiverZ + 0.02 + Rand.FRand() * Rand.FRand() * 0.8, 0.5f, FVector(D.X, D.Y, 0.0));
		}
		// along the path: thin at its start, thick by the river
		const double Length = LyudTopi::FModel::PathLength();
		for (int32 K = 0; K < 1800; ++K)
		{
			const double Along = Length * (1.0 - FMath::Square(double(Rand.FRand())) * 0.92);
			const LyudTopi::FVec2 P = LyudTopi::FModel::PathPoint(Along);
			const double X = P.X + (Rand.FRand() * 2.0 - 1.0) * 9.0, Y = P.Y + (Rand.FRand() * 2.0 - 1.0) * 9.0;
			Mote(X, Y, M.Height(X, Y) + 0.25 + Rand.FRand() * 2.4, 0.f, FVector::UpVector);
		}
		Motes = MakeMesh(TEXT("Motes"), MotesMID, false);
		Motes->TranslucencySortPriority = 3;
		Motes->CreateMeshSection_LinearColor(0, V, T, N, UV, C, NoTangents, false, false);
	}

	// the river's light on the sand where the path comes out
	for (int32 L = 0; L < 3; ++L)
	{
		const LyudTopi::FVec2 At = LyudTopi::FModel::RiverPoint((L - 1) * 110.0);
		UPointLightComponent* Light = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetupAttachment(Root);
		Light->SetRelativeLocation(ToWorld(At.X, At.Y, LyudTopi::RiverZ + 6.0));
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(0.f);
		Light->SetLightColor(FLinearColor(0.35f, 1.f, 0.85f));
		Light->SetAttenuationRadius(16000.f);
		Light->SetSourceRadius(1500.f);
		Light->SetCastShadows(false);
		// a glow on the sand and on the hero, not a mirror image of a 15 m lamp in the wet bed
		Light->SetSpecularScale(0.f);
		Light->LightingChannels.bChannel0 = true;
		Light->LightingChannels.bChannel1 = true;
		Light->SetVisibility(false);
		Light->RegisterComponent();
		RiverLights.Add(Light);
	}
}

void ALyudTopi::UpdateLook(float DeltaSeconds, const FVector& Eye)
{
	// night by the sun: the river comes up through civil twilight
	const float Pinned = CVarTopiNight.GetValueOnGameThread();
	float Target = 0.f;
	if (Pinned >= 0.f)
	{
		Target = FMath::Clamp(Pinned, 0.f, 1.f);
	}
	else
	{
		if (!State->Sky.IsValid())
		{
			TActorIterator<ALyudSky> It(GetWorld());
			if (It)
			{
				State->Sky = *It;
			}
		}
		if (State->Sky.IsValid())
		{
			Target = TopiSmooth((-1.5f - State->Sky->SunAltitude()) / 5.5f);
		}
	}
	NightNow = Pinned >= 0.f ? Target : FMath::FInterpConstantTo(NightNow, Target, DeltaSeconds, 0.5f);
	UnrestShown = FMath::FInterpTo(UnrestShown, Unrest, DeltaSeconds, 1.5f);

	// parameters change only when the values do (a MID write is cheap, but not free)
	const FLinearColor Sand(UnrestShown, NightNow, Dissolve, CVarTopiHeave.GetValueOnGameThread());
	if (SandMID && !Sand.Equals(State->SandLook, 0.002f))
	{
		State->SandLook = Sand;
		SandMID->SetVectorParameterValue(TEXT("TopiLook"), Sand);
	}
	const FLinearColor Water(NightNow, Dissolve, UnrestShown, 0.f);
	if (!Water.Equals(State->RiverLook, 0.002f))
	{
		State->RiverLook = Water;
		for (UMaterialInstanceDynamic* MID : { RiverMID.Get(), GlowMID.Get(), MotesMID.Get() })
		{
			if (MID)
			{
				MID->SetVectorParameterValue(TEXT("TopiLook"), Water);
			}
		}
		const float Candelas = 1200.f * NightNow * (1.f + 5.f * Dissolve);
		for (UPointLightComponent* Light : RiverLights)
		{
			if (Light)
			{
				Light->SetIntensity(Candelas);
			}
		}
	}
	const bool bShow = NightNow > 0.01f;
	if (bShow != State->bRiverShown)
	{
		State->bRiverShown = bShow;
		for (UProceduralMeshComponent* Mesh : { River.Get(), Glow.Get(), Motes.Get() })
		{
			if (Mesh)
			{
				Mesh->SetVisibility(bShow);
			}
		}
		for (UPointLightComponent* Light : RiverLights)
		{
			if (Light)
			{
				Light->SetVisibility(bShow);
			}
		}
	}
}

void ALyudTopi::HoldHero()
{
	// the ground under the player may not be there yet (he was put here by a teleport, or walked faster than
	// the chunks cook): never let him fall through the sand
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	ACharacter* Hero = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!Hero || !Hero->GetCharacterMovement() || !Hero->GetCapsuleComponent())
	{
		return;
	}
	const FVector At = Hero->GetActorLocation();
	const FVector2D M = ToMap(At);
	if (LyudTopi::FModel::Region(M.X, M.Y) < 0.5)
	{
		return;
	}
	const double Sand = Model().Height(M.X, M.Y) * 100.0;
	const double Half = Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const double Feet = At.Z - Half;
	// over the map's own ground the sand may lie under it: only where he is under the sand
	if (Feet < Sand - 60.0 && Hero->GetCharacterMovement()->MovementMode != MOVE_Flying && Hero->GetCharacterMovement()->MovementMode != MOVE_None)
	{
		Hero->SetActorLocation(FVector(At.X, At.Y, Sand + Half + 3.0), false, nullptr, ETeleportType::TeleportPhysics);
		FVector Velocity = Hero->GetCharacterMovement()->Velocity;
		Velocity.Z = 0.0;
		Hero->GetCharacterMovement()->Velocity = Velocity;
	}
}

void ULyudTopiSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// spawned with the level's actors, before the first tick: the mind registers with the saves before a load's
	// records are applied (on the first frame's timers)
	if (InWorld.IsGameWorld() && !Topi.IsValid())
	{
		SpawnTopi(InWorld);
	}
}

void ULyudTopiSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || Topi.IsValid() || bLooked)
	{
		return;
	}
	// a fallback: the sky came after the world began play
	SpawnTopi(*World);
	bLooked = true;
}

void ULyudTopiSubsystem::SpawnTopi(UWorld& World)
{
	// only a world with the open sky has Topi
	TActorIterator<ALyudSky> It(&World);
	if (!It)
	{
		return;
	}
	bLooked = true;
	FActorSpawnParameters P;
	P.ObjectFlags |= RF_Transient;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Topi = World.SpawnActor<ALyudTopi>(ALyudTopi::StaticClass(), FTransform::Identity, P);
	UE_LOG(LogTemp, Log, TEXT("LyudTopi: the dune land is here"));
}
