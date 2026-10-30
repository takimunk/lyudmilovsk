#include "Winter/LyudWinter.h"

#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Volume.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Plants/GameCalendar.h"
#include "Plants/ProceduralForest.h"
#include "Plants/ProceduralGrass.h"
#include "Plants/ProceduralPlant.h"
#include "Plants/ProceduralVine.h"
#include "Railway/LyudRailway.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "Sea/LyudSea.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"
#include "SnowModel.h"
#include "TextureResource.h"
#include "Weather/LyudWeather.h"
#include "Weather/LyudWind.h"
#include "../Weather/WeatherModel.h"

namespace
{
	TAutoConsoleVariable<int32> CVarWinter(TEXT("lyud.Winter"), 1, TEXT("0: no snow on the ground and no sea ice (A/B benchmarks)."));
	TAutoConsoleVariable<float> CVarWinterSnow(TEXT("lyud.Winter.Snow"), -1.f, TEXT("Force the level snow depth, cm (negative: the weather's)."));
	TAutoConsoleVariable<float> CVarWinterSnowfall(TEXT("lyud.Winter.Snowfall"), -1.f, TEXT("Force the snowfall, cm/h (negative: the weather's)."));
	TAutoConsoleVariable<float> CVarWinterDrift(TEXT("lyud.Winter.Drift"), -1.f, TEXT("Force how far the drifts have built 0..1 (negative: the winter's blowing snow)."));
	TAutoConsoleVariable<float> CVarWinterDriftFrom(TEXT("lyud.Winter.DriftFrom"), -1.f, TEXT("Force the drifting wind's bearing, degrees from (negative: the winter's winds)."));
	TAutoConsoleVariable<float> CVarWinterIce(TEXT("lyud.Winter.Ice"), -1.f, TEXT("Force the sea ice cover 0..1 (negative: the weather's)."));
	TAutoConsoleVariable<float> CVarWinterIceCm(TEXT("lyud.Winter.IceCm"), -1.f, TEXT("Force the sea ice's thickness, cm (negative: the weather's)."));
	TAutoConsoleVariable<float> CVarWinterAge(TEXT("lyud.Winter.Age"), -1.f, TEXT("Force the snow's age, days since fresh snow (negative: the weather's)."));
	TAutoConsoleVariable<float> CVarWinterCanopy(TEXT("lyud.Winter.Canopy"), -1.f, TEXT("Force the snow on the branches 0..1 (negative: from the snowfall, wind and thaw)."));
	TAutoConsoleVariable<float> CVarWinterAutoClear(TEXT("lyud.Winter.AutoClearHours"), 5.f, TEXT("Hours after snow begins to lie that a path far from the camera is cleared by somebody unseen."));
	TAutoConsoleVariable<float> CVarWinterLowWater(TEXT("lyud.Winter.LowWater"), 70.f, TEXT("How far the sea goes down over the winter, cm (the island railway's rails 30 cm under high water stand dry)."));
	TAutoConsoleVariable<float> CVarWinterLowWaterForce(TEXT("lyud.Winter.LowWaterNow"), -1.f, TEXT("Force how far into the winter's low water 0..1 (negative: by the calendar)."));
	TAutoConsoleVariable<int32> CVarWinterDebug(TEXT("lyud.Winter.Debug"), 0, TEXT("1: print the winter on screen."));
	TAutoConsoleVariable<int32> CVarWinterTraceMs(TEXT("lyud.Winter.TraceMs"), 6, TEXT("Milliseconds a frame for reading the world for the snow when winter first comes."));

	FAutoConsoleCommandWithWorld WinterReportCommand(TEXT("lyud.Winter.Report"), TEXT("Logs the winter's state."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (ULyudWinterSubsystem* W = ULyudWinterSubsystem::Get(World))
			{
				UE_LOG(LogTemp, Warning, TEXT("%s"), *W->DebugReport());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs WinterSnowOnPathsCommand(TEXT("lyud.Winter.SnowOnPaths"),
		TEXT("Puts fresh snow on every path, cm (default 10): a night's snow for the shovellers (tests)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudWinterSubsystem* W = ULyudWinterSubsystem::Get(World))
			{
				W->SnowOnPaths(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 10.f);
			}
		}));

	const TCHAR* const WinterCollectionPath = TEXT("/Game/Winter/MPC_LyudWinter.MPC_LyudWinter");
	const TCHAR* const WinterDepthRTPath = TEXT("/Game/Winter/RT_SnowDepth.RT_SnowDepth");
	const TCHAR* const WinterCoverRTPath = TEXT("/Game/Winter/RT_SnowCover.RT_SnowCover");
	const TCHAR* const WinterIceRTPath = TEXT("/Game/Winter/RT_SeaIce.RT_SeaIce");
	const TCHAR* const WinterDecalMaterialPath = TEXT("/Game/Winter/M_SnowDecal.M_SnowDecal");
	const TCHAR* const WinterIceMaterialPath = TEXT("/Game/Winter/M_SeaIce.M_SeaIce");

	FLinearColor WinterVec(double A, double B, double C, double D)
	{
		return FLinearColor(float(A), float(B), float(C), float(D));
	}

	void WinterParallelRows(int N, const std::function<void(int)>& Body)
	{
		ParallelFor(N, [&Body](int32 I) { Body(I); });
	}

	bool WinterIsPlant(const AActor* A)
	{
		return A && (A->IsA<AProceduralPlant>() || A->IsA<AProceduralForest>() || A->IsA<AProceduralGrass>() || A->IsA<AProceduralVine>());
	}

	/** Down through everything at a point: the ground (the landscape or a LyudSeaBed) and the highest thing over it. */
	/** The railway's bank (LyudSeaBed pieces kilometres long): not a shore for the ice, not ground for the snow field. */
	TSet<const UPrimitiveComponent*> GWinterBank;

	void WinterTraceColumn(const UWorld* World, double X, double Y, double ZTop, double ZBottom, float& OutGround, float& OutTop)
	{
		OutGround = OutTop = LyudWinter::FCover::NoGround;
		TArray<FHitResult> Hits;
		FCollisionObjectQueryParams Objects(ECC_WorldStatic);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(LyudWinterCover), true);
		World->LineTraceMultiByObjectType(Hits, FVector(X, Y, ZTop), FVector(X, Y, ZBottom), Objects, Params);
		Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Distance < B.Distance; });
		for (const FHitResult& H : Hits)
		{
			const AActor* A = H.GetActor();
			const UPrimitiveComponent* C = H.GetComponent();
			if (!A || !C || A->IsA<AVolume>() || A->IsA<ALyudSeaIce>())
			{
				continue;
			}
			const bool bGround = A->IsA<ALandscapeProxy>() || (C->ComponentHasTag(TEXT("LyudSeaBed")) && !GWinterBank.Contains(C));
			if (OutTop == LyudWinter::FCover::NoGround && C->IsVisible() && !WinterIsPlant(A))
			{
				OutTop = float(H.ImpactPoint.Z);
			}
			if (bGround)
			{
				OutGround = float(H.ImpactPoint.Z);
				break;
			}
		}
		if (OutGround == LyudWinter::FCover::NoGround)
		{
			OutTop = LyudWinter::FCover::NoGround;
		}
		else if (OutTop == LyudWinter::FCover::NoGround || OutTop < OutGround)
		{
			OutTop = OutGround;
		}
	}

	/** Uploads Data (W x H floats, row-major) into a region of a float render target as halves. */
	void WinterUploadHalf(UTextureRenderTarget2D* RT, int32 DstX, int32 DstY, int32 W, int32 H, int32 Channels, TArray<FFloat16>&& Data)
	{
		if (!RT || W <= 0 || H <= 0)
		{
			return;
		}
		FTextureResource* Res = RT->GetResource();
		ENQUEUE_RENDER_COMMAND(LyudWinterUpload)([Res, DstX, DstY, W, H, Channels, Data = MoveTemp(Data)](FRHICommandListImmediate& RHICmdList)
		{
			if (!Res || !Res->TextureRHI)
			{
				return;
			}
			const FUpdateTextureRegion2D Region(DstX, DstY, 0, 0, W, H);
			RHICmdList.UpdateTexture2D(Res->TextureRHI, 0, Region, W * Channels * sizeof(FFloat16), (const uint8*)Data.GetData());
		});
	}
}

namespace LyudWinter
{
	enum class EPhase : uint8 { Idle, Tracing, Refining, Building, Ready };

	/** Everything the winter keeps (plain data + the async jobs' flags). */
	struct FWinterState
	{
		EPhase Phase = EPhase::Idle;
		FCover Cover;
		FDriftBasis Basis;
		FSnowField Field;
		FSnowPaths Paths;
		FIceMask Ice;
		FIceParams IceP;
		FSnowInputs In;
		FBox LandBox = FBox(ForceInit);
		float Datum = 0.f;
		double ZTop = 0.0, ZBottom = 0.0;
		int32 TraceRow = 0;
		int32 IceTraceRow = 0;
		int32 IceOutsideX0 = 0; // ice grid columns from here on lie off the landscape and are traced
		std::vector<int32> Refine; // field cells to trace finer
		int32 RefineAt = 0;
		std::vector<uint8_t> OpenFine;
		std::vector<float> IceBedTraced;
		TFuture<void> Job;
		std::atomic<bool> bJobDone { false };
		// the drift factor computed on a worker: swapped in when done
		std::vector<float> NewFactor;
		std::atomic<bool> bFactorBusy { false };
		std::atomic<bool> bFactorReady { false };
		float FactorT = -1.f;
		float FactorRose[8] = {};
		float ComposedLevel = -1.f;
		double LastComposeTime = -1e9;
		float PrevLevel = 0.f;
		double PathHours = -1.0;
		bool bPathsSettled = false;
		TFuture<void> IceJob;
		std::atomic<bool> bIceBusy { false };
		std::atomic<bool> bIceReady { false };
		std::vector<float> NextIceDist;
		float NextIceSeaZ = 0.f;
		bool bIceUploaded = false;
		float CanopySnow = 0.f;
		bool bCanopySeeded = false;
		float RoseHistory[8] = {};
		float TransportSum = 0.f;
		double LastWeatherHours = -1.0;
		FString Note;

		~FWinterState()
		{
			if (Job.IsValid())
			{
				Job.Wait();
			}
			if (IceJob.IsValid())
			{
				IceJob.Wait();
			}
		}
	};
}


// -------------------------------------------------------------------------------------------------------------- subsystem

ULyudWinterSubsystem::ULyudWinterSubsystem() = default;
ULyudWinterSubsystem::~ULyudWinterSubsystem() = default;

ULyudWinterSubsystem* ULyudWinterSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudWinterSubsystem>() : nullptr;
}

bool ULyudWinterSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId ULyudWinterSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudWinterSubsystem, STATGROUP_Tickables);
}

void ULyudWinterSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// only where there is an outdoor ground
	if (!TActorIterator<ALandscapeProxy>(&InWorld))
	{
		return;
	}
	State = MakePimpl<LyudWinter::FWinterState>();
	Collection = LoadObject<UMaterialParameterCollection>(nullptr, WinterCollectionPath);
	DepthRT = LoadObject<UTextureRenderTarget2D>(nullptr, WinterDepthRTPath);
	CoverRT = LoadObject<UTextureRenderTarget2D>(nullptr, WinterCoverRTPath);
	IceRT = LoadObject<UTextureRenderTarget2D>(nullptr, WinterIceRTPath);
	if (!Collection || !DepthRT || !CoverRT || !IceRT)
	{
		UE_LOG(LogTemp, Warning, TEXT("LyudWinter: /Game/Winter assets missing (unreal/scripts/make_winter_materials.py); no snow on the ground"));
	}
	bStarted = true;
	UpdateCollection();
}

void ULyudWinterSubsystem::Deinitialize()
{
	if (Sea.IsValid() && SeaWindScale >= 0.f)
	{
		Sea->WindScale = SeaWindScale;
	}
	State.Reset();
	bStarted = false;
	Super::Deinitialize();
}

bool ULyudWinterSubsystem::CameraLocation(FVector& Out) const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (PC && PC->PlayerCameraManager)
	{
		Out = PC->PlayerCameraManager->GetCameraLocation();
		return true;
	}
	return false;
}

float ULyudWinterSubsystem::SnowOnBranches(const UWorld* World, float Fallback)
{
	const ULyudWinterSubsystem* W = World ? World->GetSubsystem<ULyudWinterSubsystem>() : nullptr;
	return W && W->bStarted ? W->Now.CanopySnow : Fallback;
}

float ULyudWinterSubsystem::SnowOnGround(const UWorld* World, float Fallback)
{
	const ULyudWinterSubsystem* W = World ? World->GetSubsystem<ULyudWinterSubsystem>() : nullptr;
	return W && W->bStarted ? FMath::Clamp(W->Now.LevelCm / 8.f, 0.f, 1.f) : Fallback;
}

bool ULyudWinterSubsystem::IsReady() const
{
	return State.IsValid() && State->Phase == LyudWinter::EPhase::Ready;
}

// -------------------------------------------------------------------------------------------------------------- weather

void ULyudWinterSubsystem::ReadWeather(float DeltaTime)
{
	// what the weather engine says (its season: the snow pack, the sea ice) plus this winter's drifting winds, which
	// the snow's shape remembers: integrated from the weather's history (from 1 September) the first time, then as the
	// clock runs
	UWorld* World = GetWorld();
	LyudWinter::FWinterState& S = *State;
	const ULyudWeatherSubsystem* Weather = World->GetSubsystem<ULyudWeatherSubsystem>();
	if (!Weather)
	{
		return;
	}
	const LyudWeather::FWeatherState& W = Weather->GetModelState();
	const double Hours = W.Hours;
	auto Drift = [&S](const LyudWeather::FWeatherState& X, float DtH)
	{
		if (X.SnowDepthCm > 1.f && DtH > 0.f)
		{
			// fresh, dry snow drifts; old, crusted or wet snow hardly moves
			const float Loose = X.Melt > 0.2f || X.Glaze > 0.3f ? 0.08f : FMath::Clamp(1.f - X.SnowAgeDays / 10.f, 0.2f, 1.f);
			S.TransportSum += LyudWinter::AddToRose(S.RoseHistory, X.WindFromDeg, X.WindSpeed, Loose * DtH);
		}
		else if (X.SnowDepthCm <= 0.5f)
		{
			// a thaw took the snow: the next snow starts level again
			S.TransportSum = 0.f;
			FMemory::Memzero(S.RoseHistory);
		}
	};
	if (S.LastWeatherHours < 0.0 || Hours < S.LastWeatherHours - 1.0 || Hours > S.LastWeatherHours + 72.0)
	{
		// the history: this season from 1 September, every 3 hours
		S.TransportSum = 0.f;
		FMemory::Memzero(S.RoseHistory);
		const FDateTime WNow = Weather->WeatherTime();
		const int32 Year = WNow.GetMonth() >= 9 ? WNow.GetYear() : WNow.GetYear() - 1;
		const double Start = (FDateTime(Year, 9, 1) - FDateTime(2000, 1, 1)).GetTotalHours();
		LyudWeather::FChain& Chain = LyudWeather::SharedChain();
		LyudWeather::FSeason Season;
		for (double H = Start; H < Hours; H += 3.0)
		{
			Drift(LyudWeather::Full(H, Chain, Season), 3.f);
		}
		S.LastWeatherHours = Hours;
	}
	const float DtH = float(FMath::Clamp(Hours - S.LastWeatherHours, 0.0, 6.0));
	if (DtH >= 0.25f)
	{
		Drift(W, DtH);
		S.LastWeatherHours = Hours;
	}

	const float ForceSnow = CVarWinterSnow.GetValueOnGameThread();
	const float ForceFall = CVarWinterSnowfall.GetValueOnGameThread();
	Now.LevelCm = ForceSnow >= 0.f ? ForceSnow : W.SnowDepthCm;
	Now.SnowfallCmPerHour = ForceFall >= 0.f ? ForceFall : W.SnowfallCmPerHour;
	Now.Melt = W.Melt;
	const float ForceAge = CVarWinterAge.GetValueOnGameThread();
	Now.SnowAgeDays = ForceAge >= 0.f ? ForceAge : W.SnowAgeDays;

	const float ForceDrift = CVarWinterDrift.GetValueOnGameThread();
	const float ForceFrom = CVarWinterDriftFrom.GetValueOnGameThread();
	// a few days of a fresh wind over loose snow build the drifts fully (6000 = one day of 10 m/s and some)
	Now.Drift = ForceDrift >= 0.f ? FMath::Clamp(ForceDrift, 0.f, 1.f) : 1.f - FMath::Exp(-S.TransportSum / 6000.f);
	FMemory::Memzero(S.In.Rose);
	float RoseSum = 0.f;
	for (int32 D = 0; D < 8; D++)
	{
		RoseSum += S.RoseHistory[D];
	}
	if (ForceFrom >= 0.f || RoseSum <= 0.f)
	{
		LyudWinter::AddToRose(S.In.Rose, ForceFrom >= 0.f ? ForceFrom : W.WindFromDeg, 12.f, 1.f);
	}
	else
	{
		FMemory::Memcpy(S.In.Rose, S.RoseHistory, sizeof(S.In.Rose));
	}
	int32 Best = 0;
	for (int32 D = 1; D < 8; D++)
	{
		Best = S.In.Rose[D] > S.In.Rose[Best] ? D : Best;
	}
	Now.DriftFromDeg = 45.f * Best;

	// snow on the branches: fresh snow sticks, wind and thaw shake it off over a day or two
	const float CanopyDtH = FMath::Clamp(DeltaTime / 3600.f * FMath::Max(GetDefault<UGameCalendar>()->TimeScale, 1.f), 0.f, 6.f);
	S.CanopySnow = FMath::Clamp(S.CanopySnow + CanopyDtH * (0.35f * Now.SnowfallCmPerHour - 0.02f - 0.015f * FMath::Max(W.WindSpeed - 4.f, 0.f) - 0.6f * Now.Melt), 0.f, 1.f);
	if (!S.bCanopySeeded)
	{
		// a start mid-winter: what a day like this would have on the branches
		S.CanopySnow = Now.LevelCm > 2.f ? FMath::Clamp(1.f - Now.SnowAgeDays / 3.f, 0.f, 1.f) * (1.f - Now.Melt) : 0.f;
		S.bCanopySeeded = true;
	}
	if (Now.LevelCm <= 0.5f)
	{
		S.CanopySnow = 0.f;
	}
	const float ForceCanopy = CVarWinterCanopy.GetValueOnGameThread();
	Now.CanopySnow = ForceCanopy >= 0.f ? FMath::Clamp(ForceCanopy, 0.f, 1.f) : S.CanopySnow;

	const float ForceIce = CVarWinterIce.GetValueOnGameThread();
	const float ForceIceCm = CVarWinterIceCm.GetValueOnGameThread();
	Now.IceCover = ForceIce >= 0.f ? FMath::Clamp(ForceIce, 0.f, 1.f) : W.SeaIceCover;
	Now.IceCm = ForceIceCm >= 0.f ? ForceIceCm : (ForceIce >= 0.f && W.SeaIceCm < 1.f ? 15.f : W.SeaIceCm);

	S.In.LevelCm = Now.LevelCm;
	S.In.SnowfallCmPerHour = Now.SnowfallCmPerHour;
	S.In.Transport = Now.Drift;
	S.In.Density = W.SnowDensity;
	// leads and floes belong to the freeze's start and its break-up: from ~60% of the weather's cover the open sea is
	// shut (only the shore's strip stays open)
	S.IceP.Cover = FMath::Clamp((Now.IceCover - 0.15f) / 0.45f, 0.f, 1.f);
	S.IceP.ThicknessCm = Now.IceCm;
	if (CVarWinter.GetValueOnGameThread() == 0)
	{
		S.In.LevelCm = 0.f;
		S.IceP.Cover = 0.f;
		Now.CanopySnow = 0.f;
	}
}

// -------------------------------------------------------------------------------------------------------------- tick

void ULyudWinterSubsystem::Tick(float DeltaTime)
{
	if (!State.IsValid())
	{
		return;
	}
	LyudWinter::FWinterState& S = *State;
	if (!Sea.IsValid())
	{
		Sea = ALyudSea::Get(GetWorld());
	}
	ReadWeather(DeltaTime);
	LowWater();
	const bool bWanted = S.In.LevelCm > 0.2f || (S.IceP.Cover > 0.f && S.IceP.ThicknessCm > 0.5f);
	switch (S.Phase)
	{
	case LyudWinter::EPhase::Idle:
		if (bWanted && Collection && DepthRT)
		{
			StartBuild();
		}
		break;
	case LyudWinter::EPhase::Tracing:
	case LyudWinter::EPhase::Refining:
		StepCover();
		break;
	case LyudWinter::EPhase::Building:
		if (S.bJobDone)
		{
			FinishBuild();
		}
		break;
	case LyudWinter::EPhase::Ready:
		StepSnow(DeltaTime);
		break;
	}
	StepIce(DeltaTime);
	UpdateCollection();
	UpdateDecal();
	Wade(DeltaTime);
	StepNear(DeltaTime);
	StepFlakes(DeltaTime);
	SweepTimer -= DeltaTime;
	if (SweepTimer <= 0.f && S.Phase == LyudWinter::EPhase::Ready)
	{
		SweepTimer = 2.f;
		KeepOffDecals();
	}
	if (CVarWinterDebug.GetValueOnGameThread() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(int32(0x5170), 0.f, FColor::White, DebugReport());
	}
}

// -------------------------------------------------------------------------------------------------------------- building

void ULyudWinterSubsystem::StartBuild()
{
	LyudWinter::FWinterState& S = *State;
	UWorld* World = GetWorld();
	S.LandBox = FBox(ForceInit);
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		S.LandBox += It->GetComponentsBoundingBox(true);
	}
	if (!S.LandBox.IsValid)
	{
		return;
	}
	S.Datum = Sea.IsValid() ? float(Sea->GetActorLocation().Z) : float(S.LandBox.Min.Z);
	S.ZTop = S.LandBox.Max.Z + 6000.0;
	S.ZBottom = S.LandBox.Min.Z - 1000.0;
	const float Cell = 200.f;
	const int32 W = FMath::CeilToInt(S.LandBox.GetSize().X / Cell), H = FMath::CeilToInt(S.LandBox.GetSize().Y / Cell);
	S.Cover.Init(W, H, S.LandBox.Min.X + Cell * 0.5, S.LandBox.Min.Y + Cell * 0.5, Cell);
	// the ice's grid: the landscape and the sea out past the island (the small LyudSeaBed pieces; the bank is a lead along the line)
	double IceMaxX = S.LandBox.Max.X;
	GWinterBank.Reset();
	for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
	{
		if (It->GetWorld() == World && It->ComponentHasTag(TEXT("LyudSeaBed")) && It->IsRegistered())
		{
			const FBox B = It->Bounds.GetBox();
			if (B.GetSize().X < 200000.0 && B.GetSize().Y < 200000.0)
			{
				IceMaxX = FMath::Max(IceMaxX, B.Max.X + 15000.0);
			}
			else
			{
				GWinterBank.Add(*It);
			}
		}
	}
	const float IceCell = 300.f;
	const int32 IW = FMath::CeilToInt((IceMaxX - S.LandBox.Min.X) / IceCell), IH = FMath::CeilToInt(S.LandBox.GetSize().Y / IceCell);
	S.Ice.Init(IW, IH, S.LandBox.Min.X + IceCell * 0.5, S.LandBox.Min.Y + IceCell * 0.5, IceCell);
	S.IceOutsideX0 = FMath::Clamp(FMath::FloorToInt((S.LandBox.Max.X - S.LandBox.Min.X) / IceCell), 0, IW);
	S.IceBedTraced.assign(size_t(FMath::Max(IW - S.IceOutsideX0, 0)) * IH, LyudWinter::FCover::NoGround);
	S.TraceRow = 0;
	S.IceTraceRow = 0;
	S.Phase = LyudWinter::EPhase::Tracing;
	UE_LOG(LogTemp, Log, TEXT("LyudWinter: reading the world for the snow: %dx%d at 2 m, ice %dx%d at 3 m"), W, H, IW, IH);
}

void ULyudWinterSubsystem::StepCover()
{
	LyudWinter::FWinterState& S = *State;
	const UWorld* World = GetWorld();
	const double Budget = CVarWinterTraceMs.GetValueOnGameThread() * 0.001;
	const double Start = FPlatformTime::Seconds();
	if (S.Phase == LyudWinter::EPhase::Tracing)
	{
		while (FPlatformTime::Seconds() - Start < Budget)
		{
			if (S.TraceRow < S.Cover.H)
			{
				const int32 Rows = FMath::Min(16, S.Cover.H - S.TraceRow);
				const int32 Row0 = S.TraceRow;
				ParallelFor(Rows * S.Cover.W, [&](int32 K)
				{
					const int32 Y = Row0 + K / S.Cover.W, X = K % S.Cover.W;
					const int32 I = S.Cover.Index(X, Y);
					WinterTraceColumn(World, S.Cover.X0 + X * S.Cover.Cell, S.Cover.Y0 + Y * S.Cover.Cell, S.ZTop, S.ZBottom, S.Cover.Ground[I], S.Cover.Top[I]);
				});
				S.TraceRow += Rows;
				continue;
			}
			const int32 OW = S.Ice.W - S.IceOutsideX0;
			if (OW > 0 && S.IceTraceRow < S.Ice.H)
			{
				const int32 Rows = FMath::Min(32, S.Ice.H - S.IceTraceRow);
				const int32 Row0 = S.IceTraceRow;
				ParallelFor(Rows * OW, [&](int32 K)
				{
					const int32 Y = Row0 + K / OW, X = K % OW;
					float Top;
					WinterTraceColumn(World, S.Ice.X0 + (S.IceOutsideX0 + X) * S.Ice.Cell, S.Ice.Y0 + Y * S.Ice.Cell, S.ZTop, S.ZBottom,
						S.IceBedTraced[size_t(Y) * OW + X], Top);
				});
				S.IceTraceRow += Rows;
				continue;
			}
			// the coarse pass is done: what is roofed, the forests, lone posts dropped; then the roofs' edges at 1 m
			S.Cover.Classify(S.Datum);
			for (TActorIterator<AProceduralForest> It(World); It; ++It)
			{
				const FVector C = It->GetActorLocation();
				S.Cover.MarkForest(C.X - It->HalfExtent.X, C.Y - It->HalfExtent.Y, C.X + It->HalfExtent.X, C.Y + It->HalfExtent.Y);
			}
			S.Cover.CleanRoofs();
			S.Cover.ComputeUrban(WinterParallelRows);
			S.Field.Setup(S.Cover, 100.f);
			S.OpenFine = S.Field.Open;
			S.Refine.clear();
			for (int32 Y = 0; Y < S.Field.H; Y++)
			{
				for (int32 X = 0; X < S.Field.W; X++)
				{
					// a 1 m cell whose 2 m neighbours disagree about a roof
					const double WX = S.Field.X0 + X * S.Field.Cell, WY = S.Field.Y0 + Y * S.Field.Cell;
					const uint8 K = S.Cover.KindAt(WX, WY);
					bool bMixed = false;
					for (int32 D = 0; D < 4 && !bMixed; D++)
					{
						const uint8 N = S.Cover.KindAt(WX + (D == 0 ? 100.0 : D == 1 ? -100.0 : 0.0), WY + (D == 2 ? 100.0 : D == 3 ? -100.0 : 0.0));
						bMixed = (N == LyudWinter::CellRoof) != (K == LyudWinter::CellRoof) && N != LyudWinter::CellNone && K != LyudWinter::CellNone;
					}
					if (bMixed)
					{
						S.Refine.push_back(Y * S.Field.W + X);
					}
				}
			}
			S.RefineAt = 0;
			S.Phase = LyudWinter::EPhase::Refining;
			UE_LOG(LogTemp, Log, TEXT("LyudWinter: coarse read in, %d roof edge cells to read at 1 m"), int32(S.Refine.size()));
			return;
		}
		return;
	}
	// refining the roofs' edges
	while (FPlatformTime::Seconds() - Start < Budget && S.RefineAt < int32(S.Refine.size()))
	{
		const int32 N = FMath::Min(8192, int32(S.Refine.size()) - S.RefineAt);
		const int32 At = S.RefineAt;
		ParallelFor(N, [&](int32 K)
		{
			const int32 I = S.Refine[At + K];
			const int32 X = I % S.Field.W, Y = I / S.Field.W;
			float G, T;
			WinterTraceColumn(World, S.Field.X0 + X * S.Field.Cell, S.Field.Y0 + Y * S.Field.Cell, S.ZTop, S.ZBottom, G, T);
			S.OpenFine[I] = (G != LyudWinter::FCover::NoGround && G >= S.Datum + 30.f && T - G <= LyudWinter::RoofMinCm) ? 1 : 0;
		});
		S.RefineAt += N;
	}
	if (S.RefineAt < int32(S.Refine.size()))
	{
		return;
	}
	// the rest on a worker: the shelter of every cell, the paths, the ice's distances
	S.Phase = LyudWinter::EPhase::Building;
	S.bJobDone = false;
	TArray<uint8> AnimalGrid;
	FFileHelper::LoadFileToArray(AnimalGrid, *(FPaths::ProjectContentDir() / TEXT("AnimalsData/animal_world.bin")));
	FString PlacesJson;
	FFileHelper::LoadFileToString(PlacesJson, *(FPaths::ProjectContentDir() / TEXT("AnimalsData/animal_places.json")));
	// the doorsteps (the walk grid's places of kind 0) get a path to the nearest paved cell
	TArray<FVector2D> Doorsteps;
	TSharedPtr<FJsonObject> Doc;
	if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(PlacesJson), Doc) && Doc.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Places;
		if (Doc->TryGetArrayField(TEXT("places"), Places))
		{
			for (const TSharedPtr<FJsonValue>& V : *Places)
			{
				const TArray<TSharedPtr<FJsonValue>>& P = V->AsArray();
				if (P.Num() >= 3 && int32(P[0]->AsNumber()) == 0)
				{
					Doorsteps.Add(FVector2D(P[1]->AsNumber() * 100.0, P[2]->AsNumber() * 100.0));
				}
			}
		}
	}
	LyudWinter::FWinterState* SP = &S;
	S.Job = Async(EAsyncExecution::ThreadPool, [SP, AnimalGrid = MoveTemp(AnimalGrid), Doorsteps = MoveTemp(Doorsteps)]()
	{
		LyudWinter::FWinterState& S = *SP;
		const double T0 = FPlatformTime::Seconds();
		S.Field.Open = S.OpenFine;
		S.Basis.Build(S.Cover, WinterParallelRows);
		const double T1 = FPlatformTime::Seconds();
		// the paths: the walk grid's paved cells (2 m), read at 1 m where at least half paved, never under a roof
		std::vector<uint8_t> Mask(size_t(S.Field.W) * S.Field.H, 0);
		if (AnimalGrid.Num() > 28 && FMemory::Memcmp(AnimalGrid.GetData(), "AWG1", 4) == 0)
		{
			int32 NX, NY;
			float AX0, AY0, ACell;
			FMemory::Memcpy(&NX, AnimalGrid.GetData() + 4, 4);
			FMemory::Memcpy(&NY, AnimalGrid.GetData() + 8, 4);
			FMemory::Memcpy(&AX0, AnimalGrid.GetData() + 12, 4);
			FMemory::Memcpy(&AY0, AnimalGrid.GetData() + 16, 4);
			FMemory::Memcpy(&ACell, AnimalGrid.GetData() + 20, 4);
			const int64 KindsAt = 24 + int64(NX) * NY * 2;
			if (NX > 0 && NY > 0 && AnimalGrid.Num() >= KindsAt + int64(NX) * NY)
			{
				const uint8* Kinds = AnimalGrid.GetData() + KindsAt;
				auto Paved = [&](int32 IX, int32 IY) -> float
				{
					IX = FMath::Clamp(IX, 0, NX - 1);
					IY = FMath::Clamp(IY, 0, NY - 1);
					return (Kinds[IY * NX + IX] & 7) == 2 ? 1.f : 0.f;
				};
				ParallelFor(S.Field.H, [&](int32 Y)
				{
					for (int32 X = 0; X < S.Field.W; X++)
					{
						const size_t I = size_t(Y) * S.Field.W + X;
						if (!S.Field.Open[I])
						{
							continue;
						}
						const double U = ((S.Field.X0 + X * S.Field.Cell) * 0.01 - AX0) / ACell, V = ((S.Field.Y0 + Y * S.Field.Cell) * 0.01 - AY0) / ACell;
						if (U < 0.0 || V < 0.0 || U > NX - 1 || V > NY - 1)
						{
							continue;
						}
						const int32 IX = int32(U), IY = int32(V);
						const float FX = float(U - IX), FY = float(V - IY);
						const float P = FMath::Lerp(FMath::Lerp(Paved(IX, IY), Paved(IX + 1, IY), FX), FMath::Lerp(Paved(IX, IY + 1), Paved(IX + 1, IY + 1), FX), FY);
						Mask[I] = P >= 0.5f ? 1 : 0;
					}
				});
				// people clear walkways, not squares and streets: paved ground wider than ~8 m (the square, the empty town's
				// street grid) keeps its snow, with whatever narrow paving touches it; the rest rounded off (no 2 m steps)
				{
					const int32 FW = S.Field.W, FH = S.Field.H;
					std::vector<float> G(Mask.size());
					for (size_t I = 0; I < Mask.size(); I++)
					{
						G[I] = Mask[I] ? 1e20f : 0.f;
					}
					LyudWinter::DistanceTransform(G, FW, FH, WinterParallelRows);
					for (size_t I = 0; I < Mask.size(); I++)
					{
						G[I] = (Mask[I] && G[I] > 16.f) ? 0.f : 1e20f;   // the cores of wide paving (> 4 m from its edge)
					}
					LyudWinter::DistanceTransform(G, FW, FH, WinterParallelRows);
					for (size_t I = 0; I < Mask.size(); I++)
					{
						if (G[I] < 36.f)
						{
							Mask[I] = 0;
						}
					}
					for (int32 Pass = 0; Pass < 2; Pass++)
					{
						std::vector<uint8_t> M2 = Mask;
						ParallelFor(FH, [&](int32 Y)
						{
							for (int32 X = 0; X < FW; X++)
							{
								int32 N = 0, T = 0;
								for (int32 DY = -1; DY <= 1; DY++)
								{
									for (int32 DX = -1; DX <= 1; DX++)
									{
										const int32 NX = X + DX, NY = Y + DY;
										if (NX >= 0 && NY >= 0 && NX < FW && NY < FH)
										{
											N += M2[size_t(NY) * FW + NX];
											T++;
										}
									}
								}
								const size_t I = size_t(Y) * FW + X;
								Mask[I] = (N * 2 > T && S.Field.Open[I]) ? 1 : 0;
							}
						});
					}
				}
				// the doors: a 1.6 m path from every doorstep to the nearest cleared walkway within 150 m and to the
				// nearest other doorstep within 70 m (along a house's front), drawn only under the open sky
				const int32 FW = S.Field.W, FH = S.Field.H;
				std::vector<FVector2D> Kept;
				for (int32 Y = 0; Y < FH; Y += 2)
				{
					for (int32 X = 0; X < FW; X += 2)
					{
						if (Mask[size_t(Y) * FW + X])
						{
							Kept.emplace_back(S.Field.X0 + X * S.Field.Cell, S.Field.Y0 + Y * S.Field.Cell);
						}
					}
				}
				auto Line = [&](const FVector2D& A, const FVector2D& B)
				{
					const double Len = FVector2D::Distance(A, B);
					for (double T = 0.0; T <= Len; T += 50.0)
					{
						const FVector2D P = A + (B - A) * (Len > 0.0 ? T / Len : 0.0);
						for (int32 DY = -1; DY <= 1; DY++)
						{
							for (int32 DX = -1; DX <= 1; DX++)
							{
								const int32 X = FMath::RoundToInt((P.X - S.Field.X0) / S.Field.Cell) + DX, Y = FMath::RoundToInt((P.Y - S.Field.Y0) / S.Field.Cell) + DY;
								if (X >= 0 && Y >= 0 && X < FW && Y < FH && S.Field.Open[size_t(Y) * FW + X])
								{
									Mask[size_t(Y) * FW + X] = 1;
								}
							}
						}
					}
				};
				for (int32 I = 0; I < Doorsteps.Num(); I++)
				{
					const FVector2D& D = Doorsteps[I];
					double BestD = FMath::Square(15000.0);
					const FVector2D* Best = nullptr;
					for (const FVector2D& K : Kept)
					{
						const double DD = FVector2D::DistSquared(K, D);
						if (DD < BestD)
						{
							BestD = DD;
							Best = &K;
						}
					}
					if (Best)
					{
						Line(D, *Best);
					}
					BestD = FMath::Square(7000.0);
					int32 Next = -1;
					for (int32 J = 0; J < Doorsteps.Num(); J++)
					{
						const double DD = FVector2D::DistSquared(Doorsteps[J], D);
						if (J != I && DD > 1.0 && DD < BestD)
						{
							BestD = DD;
							Next = J;
						}
					}
					if (Next >= 0)
					{
						Line(D, Doorsteps[Next]);
					}
				}
			}
		}
		S.Paths.Build(S.Field.W, S.Field.H, S.Field.X0, S.Field.Y0, S.Field.Cell, Mask);
		const double T2 = FPlatformTime::Seconds();
		// the ice's bed: the landscape's from the cover, the rest traced
		const int32 OW = S.Ice.W - S.IceOutsideX0;
		for (int32 Y = 0; Y < S.Ice.H; Y++)
		{
			for (int32 X = 0; X < S.Ice.W; X++)
			{
				S.Ice.Bed[size_t(Y) * S.Ice.W + X] = X >= S.IceOutsideX0 && OW > 0 ? S.IceBedTraced[size_t(Y) * OW + (X - S.IceOutsideX0)]
				                                                                  : S.Cover.GroundAt(S.Ice.X0 + X * S.Ice.Cell, S.Ice.Y0 + Y * S.Ice.Cell);
			}
		}
		S.Note = FString::Printf(TEXT("shelter %.0f ms, paths %.0f ms (%d patches)"), (T1 - T0) * 1e3, (T2 - T1) * 1e3, int32(S.Paths.Patches.size()));
		S.bJobDone = true;
	});
}

void ULyudWinterSubsystem::FinishBuild()
{
	LyudWinter::FWinterState& S = *State;
	S.Job.Reset();
	if (DepthRT)
	{
		DepthRT->ResizeTarget(S.Field.W, S.Field.H);
	}
	if (CoverRT)
	{
		CoverRT->ResizeTarget(S.Cover.W, S.Cover.H);
	}
	if (IceRT)
	{
		IceRT->ResizeTarget(S.Ice.W, S.Ice.H);
	}
	UploadCover();
	S.FactorT = -1.f;
	S.ComposedLevel = -1.f;
	S.bPathsSettled = false;
	S.Phase = LyudWinter::EPhase::Ready;
	UE_LOG(LogTemp, Log, TEXT("LyudWinter: ready: field %dx%d, %s"), S.Field.W, S.Field.H, *S.Note);
}

// -------------------------------------------------------------------------------------------------------------- snow

void ULyudWinterSubsystem::StepSnow(float DeltaTime)
{
	LyudWinter::FWinterState& S = *State;
	UWorld* World = GetWorld();
	const double Real = World->GetRealTimeSeconds();

	// a new drift factor when the drifting winds or how far they got changed
	float RoseDiff = 0.f, RoseSum = 0.f;
	for (int32 D = 0; D < 8; D++)
	{
		RoseSum += S.In.Rose[D];
	}
	for (int32 D = 0; D < 8; D++)
	{
		RoseDiff += FMath::Abs((RoseSum > 0.f ? S.In.Rose[D] / RoseSum : 0.f) - S.FactorRose[D]);
	}
	if (!S.bFactorBusy && !S.bFactorReady && (S.FactorT < 0.f || FMath::Abs(S.In.Transport - S.FactorT) > 0.03f || RoseDiff > 0.08f))
	{
		S.bFactorBusy = true;
		S.FactorT = S.In.Transport;
		for (int32 D = 0; D < 8; D++)
		{
			S.FactorRose[D] = RoseSum > 0.f ? S.In.Rose[D] / RoseSum : 0.f;
		}
		LyudWinter::FWinterState* SP = &S;
		const LyudWinter::FSnowInputs In = S.In;
		S.Job = Async(EAsyncExecution::ThreadPool, [SP, In]()
		{
			LyudWinter::FSnowField::ComputeFactor(SP->Cover, SP->Basis, In, WinterParallelRows, SP->NewFactor);
			SP->bFactorReady = true;
			SP->bFactorBusy = false;
		});
	}
	bool bFull = false;
	if (S.bFactorReady)
	{
		S.Field.Factor.swap(S.NewFactor);
		S.bFactorReady = false;
		bFull = true;
	}
	if (S.Field.Factor.empty())
	{
		return;
	}

	// the paths: fresh snow on them, the unseen shovellers, a jump of the clock
	const double Hours = (UGameCalendar::NowIn(World) - FDateTime(2000, 1, 1)).GetTotalHours();
	FVector Cam = FVector::ZeroVector;
	CameraLocation(Cam);
	if (!S.bPathsSettled || S.PathHours < 0.0 || FMath::Abs(Hours - S.PathHours) > 3.0)
	{
		LyudWinter::SettlePaths(S.Paths, S.In, Hours);
		S.bPathsSettled = true;
		S.PathHours = Hours;
		S.PrevLevel = S.In.LevelCm;
		bFull = true;
	}
	TArray<int32> Changed;
	if (Hours - S.PathHours >= 1.0 / 60.0)
	{
		std::vector<int> Ch;
		LyudWinter::StepPaths(S.Paths, S.In, S.PrevLevel, Hours, float(Hours - S.PathHours), Cam.X, Cam.Y, CVarWinterAutoClear.GetValueOnGameThread(), 60.f, Ch);
		S.PathHours = Hours;
		S.PrevLevel = S.In.LevelCm;
		for (int C : Ch)
		{
			Changed.Add(C);
		}
	}
	if (FMath::Abs(S.In.LevelCm - S.ComposedLevel) > 0.3f && Real - S.LastComposeTime > 1.0)
	{
		bFull = true;
	}
	if (bFull || Changed.Num() > 24)
	{
		const double T0 = FPlatformTime::Seconds();
		S.Field.ComposeAll(S.Cover, S.In, &S.Paths, WinterParallelRows);
		UploadDepth(0, 0, S.Field.W - 1, S.Field.H - 1);
		S.ComposedLevel = S.In.LevelCm;
		S.LastComposeTime = Real;
		UE_LOG(LogTemp, Verbose, TEXT("LyudWinter: snow composed (level %.1f cm, drift %.2f) in %.1f ms"), S.In.LevelCm, S.In.Transport, (FPlatformTime::Seconds() - T0) * 1e3);
		return;
	}
	const int32 Pad = FMath::CeilToInt(LyudWinter::FSnowPaths::BermReachM * 100.f / S.Field.Cell) + 1;
	for (int32 Pk : Changed)
	{
		const LyudWinter::FPatch& P = S.Paths.Patches[Pk];
		S.Field.Compose(S.Cover, S.In, &S.Paths, P.MinX - Pad, P.MinY - Pad, P.MaxX + Pad, P.MaxY + Pad, LyudWinter::Serial);
		UploadDepth(P.MinX - Pad, P.MinY - Pad, P.MaxX + Pad, P.MaxY + Pad);
	}
}

void ULyudWinterSubsystem::UploadDepth(int32 X0, int32 Y0, int32 X1, int32 Y1)
{
	LyudWinter::FWinterState& S = *State;
	X0 = FMath::Max(X0, 0);
	Y0 = FMath::Max(Y0, 0);
	X1 = FMath::Min(X1, S.Field.W - 1);
	Y1 = FMath::Min(Y1, S.Field.H - 1);
	const int32 W = X1 - X0 + 1, H = Y1 - Y0 + 1;
	if (W <= 0 || H <= 0)
	{
		return;
	}
	TArray<FFloat16> Data;
	Data.SetNumUninitialized(W * H);
	ParallelFor(H, [&](int32 Row)
	{
		const float* Src = S.Field.Depth.data() + size_t(Y0 + Row) * S.Field.W + X0;
		FFloat16* Dst = Data.GetData() + Row * W;
		for (int32 X = 0; X < W; X++)
		{
			Dst[X] = FFloat16(Src[X]);
		}
	});
	WinterUploadHalf(DepthRT, X0, Y0, W, H, 1, MoveTemp(Data));
}

void ULyudWinterSubsystem::UploadCover()
{
	LyudWinter::FWinterState& S = *State;
	TArray<FFloat16> Data;
	Data.SetNumUninitialized(S.Cover.W * S.Cover.H * 2);
	for (int32 I = 0; I < S.Cover.W * S.Cover.H; I++)
	{
		const float G = S.Cover.Ground[I];
		const bool bNone = G == LyudWinter::FCover::NoGround;
		// R: the ground over the datum, m (10 m under it where there is none); G: the roof over it, m (0 = open sky)
		// what the snow lies on: a low surface over the landscape (a yard's paving, a platform) counts as the ground
		const float Base = (!bNone && S.Cover.Kind[I] != LyudWinter::CellRoof && S.Cover.Top[I] > G) ? S.Cover.Top[I] : G;
		Data[I * 2] = FFloat16(bNone ? -10.f : (Base - S.Datum) * 0.01f);
		Data[I * 2 + 1] = FFloat16(S.Cover.Kind[I] == LyudWinter::CellRoof ? (S.Cover.Top[I] - G) * 0.01f : 0.f);
	}
	WinterUploadHalf(CoverRT, 0, 0, S.Cover.W, S.Cover.H, 2, MoveTemp(Data));
}

void ULyudWinterSubsystem::UploadIce()
{
	LyudWinter::FWinterState& S = *State;
	TArray<FFloat16> Data;
	Data.SetNumUninitialized(S.Ice.W * S.Ice.H);
	for (int32 I = 0; I < S.Ice.W * S.Ice.H; I++)
	{
		Data[I] = FFloat16(FMath::Min(S.Ice.Dist[I], 60000.f));
	}
	WinterUploadHalf(IceRT, 0, 0, S.Ice.W, S.Ice.H, 1, MoveTemp(Data));
}

// -------------------------------------------------------------------------------------------------------------- ice

void ULyudWinterSubsystem::StepIce(float DeltaTime)
{
	LyudWinter::FWinterState& S = *State;
	UWorld* World = GetWorld();
	const bool bIce = S.Phase == LyudWinter::EPhase::Ready && S.IceP.Cover > 0.f && S.IceP.ThicknessCm > 0.5f && Sea.IsValid();
	if (Sea.IsValid())
	{
		// no fetch over the ice: the waves die down (in the open strip too)
		if (SeaWindScale < 0.f)
		{
			SeaWindScale = Sea->WindScale;
		}
		const float K = bIce ? S.IceP.Cover * FMath::Clamp(S.IceP.ThicknessCm / 4.f, 0.f, 1.f) : 0.f;
		Sea->WindScale = SeaWindScale * FMath::Lerp(1.f, 0.03f, K);
	}
	if (!bIce)
	{
		if (Ice)
		{
			Ice->SetActorHiddenInGame(true);
			Ice->SetActorEnableCollision(false);
		}
		return;
	}
	const float SeaZ = Sea->SeaLevel();
	S.IceP.SeaZ = SeaZ;
	// the railway's bank stands dry over the winter's low water: the ice comes right up to its slopes (no lead)
	S.IceP.RailX1 = S.IceP.RailX0 - 1.0;
	// the distances again when the tide has moved the shore
	if (!S.bIceBusy && !S.bIceReady && FMath::Abs(SeaZ - S.Ice.BuiltSeaZ) > 10.f)
	{
		S.bIceBusy = true;
		LyudWinter::FWinterState* SP = &S;
		const float ShallowM = S.IceP.ShallowM;
		if (S.IceJob.IsValid())
		{
			S.IceJob.Wait();
		}
		S.IceJob = Async(EAsyncExecution::ThreadPool, [SP, SeaZ, ShallowM]()
		{
			SP->Ice.ComputeDist(SeaZ, ShallowM, WinterParallelRows, SP->NextIceDist);
			SP->NextIceSeaZ = SeaZ;
			SP->bIceReady = true;
			SP->bIceBusy = false;
		});
	}
	if (S.bIceReady)
	{
		S.bIceReady = false;
		S.Ice.Dist.swap(S.NextIceDist);
		S.Ice.BuiltSeaZ = S.NextIceSeaZ;
		UploadIce();
		S.bIceUploaded = true;
	}
	if (!S.bIceUploaded)
	{
		return;
	}
	if (!Ice)
	{
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		Ice = World->SpawnActor<ALyudSeaIce>(ALyudSeaIce::StaticClass(), FTransform::Identity, P);
	}
	Ice->SetActorHiddenInGame(false);
	Ice->SetActorEnableCollision(true);
	FVector Cam = FVector::ZeroVector;
	CameraLocation(Cam);
	const LyudWinter::FIceParams IP = S.IceP;
	const LyudWinter::FIceMask& Mask = S.Ice;
	Ice->Drive(SeaZ + 0.1f * S.IceP.ThicknessCm + 12.f, S.IceP.ThicknessCm, Cam,
		[&Mask, &IP](double X, double Y) { return Mask.Walkable(X, Y, IP); }, false);
}

// -------------------------------------------------------------------------------------------------------------- look

void ULyudWinterSubsystem::LowWater()
{
	// the Baltic's winter low: the sea goes down from late November, stays down January-February, comes back by mid
	// April; smooth (a day of the year blend), so the shore and the ice edge walk out slowly
	if (!Sea.IsValid())
	{
		return;
	}
	const float Day = UGameCalendar::DayOfYear(UGameCalendar::NowIn(GetWorld()));
	auto Smooth = [](float A, float B, float V) { const float T = FMath::Clamp((V - A) / (B - A), 0.f, 1.f); return T * T * (3.f - 2.f * T); };
	// days counted from 1 July so the winter is one stretch: 20 Nov = 142, 5 Jan = 188, 1 Mar = 243, 10 Apr = 283
	const float D = FMath::Fmod(Day - 182.f + 365.f, 365.f);
	float W = Smooth(142.f, 188.f, D) * (1.f - Smooth(243.f, 283.f, D));
	const float Force = CVarWinterLowWaterForce.GetValueOnGameThread();
	if (Force >= 0.f)
	{
		W = FMath::Clamp(Force, 0.f, 1.f);
	}
	if (CVarWinter.GetValueOnGameThread() == 0)
	{
		W = 0.f;
	}
	Now.LowWaterCm = CVarWinterLowWater.GetValueOnGameThread() * W;
	if (FMath::Abs(Sea->GetLevelOffsetTarget() + Now.LowWaterCm) > 0.5f)
	{
		// the winter's low water eases itself over weeks: the sea follows it at once
		Sea->SetLevelOffset(-Now.LowWaterCm, 0.f);
	}
}

void ULyudWinterSubsystem::UpdateCollection()
{
	if (!Collection || !State.IsValid())
	{
		return;
	}
	UWorld* World = GetWorld();
	const LyudWinter::FWinterState& S = *State;
	auto Vec = [&](const TCHAR* Name, const FLinearColor& V) { UKismetMaterialLibrary::SetVectorParameterValue(World, Collection, FName(Name), V); };
	const bool bReady = S.Phase == LyudWinter::EPhase::Ready && !S.Field.Factor.empty();
	if (S.Field.W > 0)
	{
		const double MinX = S.Field.X0 - S.Field.Cell * 0.5, MinY = S.Field.Y0 - S.Field.Cell * 0.5;
		Vec(TEXT("FieldRect"), WinterVec(MinX, MinY, 1.0 / (S.Field.W * S.Field.Cell), 1.0 / (S.Field.H * S.Field.Cell)));
		Vec(TEXT("FieldTexel"), WinterVec(1.f / S.Field.W, 1.f / S.Field.H, S.Field.Cell, 0.f));
	}
	if (S.Cover.W > 0)
	{
		const double MinX = S.Cover.X0 - S.Cover.Cell * 0.5, MinY = S.Cover.Y0 - S.Cover.Cell * 0.5;
		Vec(TEXT("CoverRect"), WinterVec(MinX, MinY, 1.0 / (S.Cover.W * S.Cover.Cell), 1.0 / (S.Cover.H * S.Cover.Cell)));
		Vec(TEXT("CoverTexel"), WinterVec(1.f / S.Cover.W, 1.f / S.Cover.H, S.Cover.Cell, 0.f));
	}
	if (S.Ice.W > 0)
	{
		const double MinX = S.Ice.X0 - S.Ice.Cell * 0.5, MinY = S.Ice.Y0 - S.Ice.Cell * 0.5;
		Vec(TEXT("IceRect"), WinterVec(MinX, MinY, 1.0 / (S.Ice.W * S.Ice.Cell), 1.0 / (S.Ice.H * S.Ice.Cell)));
	}
	const float Fresh = FMath::Clamp(1.f - Now.SnowAgeDays / 14.f, 0.f, 1.f);
	Vec(TEXT("Snow"), WinterVec(bReady ? S.In.LevelCm * 0.01f : 0.f, Fresh, Now.Melt, Now.SnowAgeDays));
	Vec(TEXT("SnowB"), WinterVec(Now.CanopySnow, bReady ? 1.f : 0.f, S.Datum * 0.01f, Now.SnowfallCmPerHour));
	const bool bIce = Ice && !Ice->IsHidden();
	Vec(TEXT("Ice"), WinterVec(S.IceP.OpenStripM, S.IceP.StripNoiseM, bIce ? S.IceP.Cover : 0.f, S.IceP.ThicknessCm));
	Vec(TEXT("IceRail"), WinterVec(S.IceP.RailY, S.IceP.RailX0, S.IceP.RailX1, S.IceP.RailHalfWidthM));
	Vec(TEXT("IceDrift"), WinterVec(S.IceP.DriftX, S.IceP.DriftY, S.IceP.SeaZ, float(S.IceP.Seed)));
}

void ULyudWinterSubsystem::UpdateDecal()
{
	LyudWinter::FWinterState& S = *State;
	const bool bSnow = S.Phase == LyudWinter::EPhase::Ready && S.In.LevelCm > 0.2f && !S.Field.Factor.empty();
	if (!bSnow)
	{
		if (Decal)
		{
			Decal->SetVisibility(false);
		}
		return;
	}
	if (!Decal)
	{
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, WinterDecalMaterialPath);
		if (!M)
		{
			return;
		}
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		DecalActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, P);
		Decal = NewObject<UDecalComponent>(DecalActor, TEXT("SnowDecal"));
		Decal->SetDecalMaterial(M);
		Decal->SortOrder = -10;
		DecalActor->SetRootComponent(Decal);
		Decal->RegisterComponent();
		// down over the whole map and the sea out to the island (its X axis is the projection)
		const FBox B(FVector(S.LandBox.Min.X, S.LandBox.Min.Y, S.ZBottom), FVector(S.Ice.X0 + S.Ice.W * S.Ice.Cell, S.LandBox.Max.Y, S.ZTop));
		Decal->DecalSize = FVector(B.GetExtent().Z, B.GetExtent().Y, B.GetExtent().X);
		Decal->SetWorldLocationAndRotation(B.GetCenter(), FRotator(-90.f, 0.f, 0.f));
		Decal->SetFadeScreenSize(0.f);
	}
	// no point drawing it from deep under the ground (the club's corridor)
	FVector Cam;
	bool bUnder = false;
	if (CameraLocation(Cam))
	{
		const float G = S.Cover.GroundAt(Cam.X, Cam.Y);
		bUnder = G != LyudWinter::FCover::NoGround && Cam.Z < G - 400.f;
	}
	Decal->SetVisibility(!bUnder);
}

void ULyudWinterSubsystem::StepNear(float DeltaTime)
{
	// the fine snow round the player (footprints): only where there is snow to press
	const bool bSnow = IsReady() && State->In.LevelCm > 1.f && !State->Field.Factor.empty();
	FVector Cam;
	if (!bSnow || !CameraLocation(Cam))
	{
		if (NearSnow && !NearSnow->IsHidden())
		{
			NearSnow->SetActorHiddenInGame(true);
			if (Collection)
			{
				UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), Collection, TEXT("NearRect"), FLinearColor(0.f, 0.f, 0.f, 400.f));
			}
		}
		return;
	}
	if (!NearSnow)
	{
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		NearSnow = GetWorld()->SpawnActor<ALyudSnowNear>(ALyudSnowNear::StaticClass(), FTransform(Cam), P);
	}
	NearSnow->SetActorHiddenInGame(false);
	NearSnow->Drive(*this, Cam, DeltaTime);
}

void ULyudWinterSubsystem::StepFlakes(float DeltaTime)
{
	// falling snow: as thick as the weather's snowfall, not indoors or under the ground (the club, the tunnel)
	FVector Cam;
	if (!CameraLocation(Cam) || !State.IsValid())
	{
		return;
	}
	const LyudWinter::FWinterState& S = *State;
	const float Fall = CVarWinter.GetValueOnGameThread() ? Now.SnowfallCmPerHour : 0.f;
	float Density = Fall > 0.02f ? FMath::Clamp(0.06f + FMath::Pow(Fall / 2.5f, 0.6f), 0.f, 1.f) : 0.f;
	if (S.Cover.Valid())
	{
		const float G = S.Cover.GroundAt(Cam.X, Cam.Y);
		const int32 IX = FMath::RoundToInt((Cam.X - S.Cover.X0) / S.Cover.Cell), IY = FMath::RoundToInt((Cam.Y - S.Cover.Y0) / S.Cover.Cell);
		const bool bIn = IX >= 0 && IY >= 0 && IX < S.Cover.W && IY < S.Cover.H;
		const bool bRoofed = bIn && S.Cover.Kind[S.Cover.Index(IX, IY)] == LyudWinter::CellRoof && Cam.Z < S.Cover.Top[S.Cover.Index(IX, IY)];
		if ((G != LyudWinter::FCover::NoGround && Cam.Z < G - 300.f) || bRoofed)
		{
			Density = 0.f;
		}
	}
	if (!Flakes)
	{
		if (Density <= 0.f)
		{
			return;
		}
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		Flakes = GetWorld()->SpawnActor<ALyudSnowFx>(ALyudSnowFx::StaticClass(), FTransform(Cam), P);
	}
	const ULyudWindSubsystem* Wind = GetWorld()->GetSubsystem<ULyudWindSubsystem>();
	const FVector W = Wind ? Wind->GetMeanWind() * 100.0 : FVector::ZeroVector;
	Flakes->Drive(Cam, Density, FVector(W.X, W.Y, 0.0), DeltaTime);
}

void ULyudWinterSubsystem::KeepOffDecals()
{
	// people, animals and birds move: a world-space snow decal would slide over them
	const UWorld* World = GetWorld();
	for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
	{
		if (It->GetWorld() == World && It->bReceivesDecals)
		{
			It->SetReceivesDecals(false);
		}
	}
	// the plants carry their own snow (their materials read MPC_LyudWinter)
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (WinterIsPlant(*It))
		{
			It->ForEachComponent<UPrimitiveComponent>(false, [](UPrimitiveComponent* C)
			{
				if (C->bReceivesDecals)
				{
					C->SetReceivesDecals(false);
				}
			});
		}
	}
	if (Ice && Ice->Sheet && Ice->Sheet->bReceivesDecals)
	{
		Ice->Sheet->SetReceivesDecals(false);
	}
	if (Sea.IsValid() && Sea->Surface && Sea->Surface->bReceivesDecals)
	{
		Sea->Surface->SetReceivesDecals(false);
	}
}

void ULyudWinterSubsystem::Wade(float DeltaTime)
{
	// deep snow off the paths slows the player down (the landscape's collision is the ground under it)
	const UWorld* World = GetWorld();
	const APlayerController* PC = World->GetFirstPlayerController();
	ACharacter* Hero = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	UCharacterMovementComponent* Move = Hero ? Hero->GetCharacterMovement() : nullptr;
	if (!Move)
	{
		return;
	}
	const float DepthM = IsReady() ? State->Field.DepthAt(Hero->GetActorLocation().X, Hero->GetActorLocation().Y) : 0.f;
	const float Scale = Move->IsMovingOnGround() ? FMath::Lerp(1.f, 0.42f, FMath::Clamp((DepthM - 0.12f) / 0.8f, 0.f, 1.f)) : 1.f;
	const float Applied = WadeSpeed > 0.f ? WadeSpeed : 1.f;
	if (FMath::Abs(Scale - Applied) > 0.01f)
	{
		Move->MaxWalkSpeed = Move->MaxWalkSpeed / Applied * Scale;
		WadeSpeed = Scale;
	}
}

// -------------------------------------------------------------------------------------------------------------- queries

float ULyudWinterSubsystem::GetSnowDepthAt(const FVector& Location) const
{
	return IsReady() ? State->Field.DepthAt(Location.X, Location.Y) * 100.f : 0.f;
}

bool ULyudWinterSubsystem::GetSnowSurfaceAt(const FVector& Location, float& OutZ) const
{
	if (!IsReady())
	{
		return false;
	}
	const float G = State->Cover.GroundAt(Location.X, Location.Y);
	if (G == LyudWinter::FCover::NoGround)
	{
		return false;
	}
	OutZ = G + State->Field.DepthAt(Location.X, Location.Y) * 100.f;
	return true;
}

float ULyudWinterSubsystem::GetIceAt(const FVector& Location) const
{
	return IsReady() && State->bIceUploaded ? State->Ice.IceAt(Location.X, Location.Y, State->IceP) : 0.f;
}

bool ULyudWinterSubsystem::IsIceWalkable(const FVector& Location) const
{
	return IsReady() && State->bIceUploaded && State->Ice.Walkable(Location.X, Location.Y, State->IceP);
}

int32 ULyudWinterSubsystem::NumPatches() const
{
	return IsReady() ? int32(State->Paths.Patches.size()) : 0;
}

int32 ULyudWinterSubsystem::FindDirtyPatch(const FVector& Near, float RadiusCm, float MinCm) const
{
	if (!IsReady())
	{
		return -1;
	}
	int32 Best = -1;
	float BestScore = 0.f;
	const double R2 = double(RadiusCm) * RadiusCm;
	for (int32 I = 0; I < int32(State->Paths.Patches.size()); I++)
	{
		const LyudWinter::FPatch& P = State->Paths.Patches[I];
		const float Snow = P.FreshCm + P.FreshBefore;
		const double D2 = FMath::Square(P.X - Near.X) + FMath::Square(P.Y - Near.Y);
		if (P.Claimed >= 0 || P.Progress < 1.f || Snow < MinCm || D2 > R2 || P.Cells < 6)
		{
			continue;
		}
		// the most snow, nearest first
		const float Score = Snow / (1.f + float(FMath::Sqrt(D2)) * 0.002f);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = I;
		}
	}
	return Best;
}

void ULyudWinterSubsystem::SnowOnPaths(float Cm)
{
	if (!IsReady())
	{
		return;
	}
	LyudWinter::FWinterState& S = *State;
	const double Hours = (UGameCalendar::NowIn(GetWorld()) - FDateTime(2000, 1, 1)).GetTotalHours();
	if (!S.bPathsSettled)
	{
		LyudWinter::SettlePaths(S.Paths, S.In, Hours);
		S.bPathsSettled = true;
		S.PathHours = Hours;
		S.PrevLevel = S.In.LevelCm;
	}
	for (LyudWinter::FPatch& P : S.Paths.Patches)
	{
		if (P.Progress >= 1.f)
		{
			P.FreshCm = FMath::Min(Cm, S.In.LevelCm);
			P.SnowSinceHours = Hours;
		}
	}
	S.Field.ComposeAll(S.Cover, S.In, &S.Paths, WinterParallelRows);
	UploadDepth(0, 0, S.Field.W - 1, S.Field.H - 1);
}

void ULyudWinterSubsystem::FindDirtyPatches(const FVector& Near, float RadiusCm, int32 Max, float MinCm, TArray<int32>& Out) const
{
	Out.Reset();
	if (!IsReady())
	{
		return;
	}
	TArray<TPair<float, int32>> Found;
	const double R2 = double(RadiusCm) * RadiusCm;
	for (int32 I = 0; I < int32(State->Paths.Patches.size()); I++)
	{
		const LyudWinter::FPatch& P = State->Paths.Patches[I];
		const double D2 = FMath::Square(P.X - Near.X) + FMath::Square(P.Y - Near.Y);
		if (P.Claimed < 0 && P.Progress >= 1.f && P.FreshCm + P.FreshBefore >= MinCm && D2 <= R2 && P.Cells >= 6)
		{
			Found.Emplace(float(D2), I);
		}
	}
	Found.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	for (int32 I = 0; I < FMath::Min(Max, Found.Num()); I++)
	{
		Out.Add(Found[I].Value);
	}
}

float ULyudWinterSubsystem::GetPatchSnow(int32 Patch) const
{
	return IsReady() && State->Paths.Patches.size() > size_t(Patch) && Patch >= 0 ? State->Paths.Patches[Patch].FreshCm + State->Paths.Patches[Patch].FreshBefore : 0.f;
}

bool ULyudWinterSubsystem::GetPatchLine(int32 Patch, FVector& OutStart, FVector& OutEnd) const
{
	if (!IsReady() || Patch < 0 || size_t(Patch) >= State->Paths.Patches.size())
	{
		return false;
	}
	const LyudWinter::FWinterState& S = *State;
	const LyudWinter::FPatch& P = S.Paths.Patches[Patch];
	const double Cell = S.Field.Cell;
	FVector2D A, B;
	if (P.bAlongX)
	{
		A = FVector2D(S.Field.X0 + P.MinX * Cell, P.Y);
		B = FVector2D(S.Field.X0 + P.MaxX * Cell, P.Y);
	}
	else
	{
		A = FVector2D(P.X, S.Field.Y0 + P.MinY * Cell);
		B = FVector2D(P.X, S.Field.Y0 + P.MaxY * Cell);
	}
	const float GA = S.Cover.GroundAt(A.X, A.Y), GB = S.Cover.GroundAt(B.X, B.Y);
	OutStart = FVector(A, GA == LyudWinter::FCover::NoGround ? 0.f : GA);
	OutEnd = FVector(B, GB == LyudWinter::FCover::NoGround ? 0.f : GB);
	return true;
}

bool ULyudWinterSubsystem::ClaimPatch(int32 Patch, int32 Who)
{
	if (!IsReady() || Patch < 0 || size_t(Patch) >= State->Paths.Patches.size())
	{
		return false;
	}
	LyudWinter::FPatch& P = State->Paths.Patches[Patch];
	if (P.Claimed >= 0 && P.Claimed != Who)
	{
		return false;
	}
	P.Claimed = Who;
	return true;
}

void ULyudWinterSubsystem::ReleasePatch(int32 Patch, int32 Who)
{
	if (!IsReady() || Patch < 0 || size_t(Patch) >= State->Paths.Patches.size())
	{
		return;
	}
	LyudWinter::FPatch& P = State->Paths.Patches[Patch];
	if (P.Claimed == Who)
	{
		P.Claimed = -1;
		// left half done: the rest keeps its snow
		if (P.Progress < 1.f)
		{
			SetPatchProgress(Patch, 1.f);
		}
	}
}

void ULyudWinterSubsystem::SetPatchProgress(int32 Patch, float Progress)
{
	if (!IsReady() || Patch < 0 || size_t(Patch) >= State->Paths.Patches.size())
	{
		return;
	}
	LyudWinter::FWinterState& S = *State;
	LyudWinter::FPatch& P = S.Paths.Patches[Patch];
	Progress = FMath::Clamp(Progress, 0.f, 1.f);
	if (P.Progress >= 1.f && Progress < 1.f)
	{
		// a clearing starts: what lies there now is what the shovel sweeps
		P.FreshBefore = P.FreshCm;
		P.FreshCm = 0.f;
	}
	if (FMath::Abs(Progress - P.Progress) < 0.02f && Progress < 1.f)
	{
		return;
	}
	P.Progress = Progress;
	if (Progress >= 1.f)
	{
		P.RemovedCm += P.FreshBefore;
		P.FreshBefore = 0.f;
		P.SnowSinceHours = -1.0;
	}
	const int32 Pad = FMath::CeilToInt(LyudWinter::FSnowPaths::BermReachM * 100.f / S.Field.Cell) + 1;
	S.Field.Compose(S.Cover, S.In, &S.Paths, P.MinX - Pad, P.MinY - Pad, P.MaxX + Pad, P.MaxY + Pad, LyudWinter::Serial);
	UploadDepth(P.MinX - Pad, P.MinY - Pad, P.MaxX + Pad, P.MaxY + Pad);
}

FString ULyudWinterSubsystem::DebugReport() const
{
	if (!State.IsValid())
	{
		return TEXT("winter: off (no landscape)");
	}
	const LyudWinter::FWinterState& S = *State;
	static const TCHAR* Phases[] = { TEXT("idle"), TEXT("tracing"), TEXT("refining"), TEXT("building"), TEXT("ready") };
	int32 Dirty = 0, Claimed = 0;
	for (const LyudWinter::FPatch& P : S.Paths.Patches)
	{
		Dirty += (P.FreshCm + P.FreshBefore) >= 2.f;
		Claimed += P.Claimed >= 0;
	}
	FString R = FString::Printf(TEXT("winter %s: level %.1f cm, snowfall %.2f cm/h, drift %.2f from %.0f, age %.1f d, melt %.2f, canopy %.2f | patches %d (%d snowy, %d being cleared) | ice cover %.2f %.0f cm, sea z %.0f, solid tiles %d"),
		Phases[int32(S.Phase)], Now.LevelCm, Now.SnowfallCmPerHour, Now.Drift, Now.DriftFromDeg, Now.SnowAgeDays, Now.Melt, Now.CanopySnow,
		int32(S.Paths.Patches.size()), Dirty, Claimed, Now.IceCover, Now.IceCm, S.IceP.SeaZ, Ice ? Ice->NumSolid() : 0);
	if (S.Phase == LyudWinter::EPhase::Tracing)
	{
		R += FString::Printf(TEXT(" | reading row %d/%d, ice %d/%d"), S.TraceRow, S.Cover.H, S.IceTraceRow, S.Ice.H);
	}
	if (!S.Note.IsEmpty())
	{
		R += TEXT(" | ") + S.Note;
	}
	return R;
}

// -------------------------------------------------------------------------------------------------------------- ice actor

ALyudSeaIce::ALyudSeaIce()
{
	PrimaryActorTick.bCanEverTick = false;
	Sheet = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sheet"));
	RootComponent = Sheet;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane.Succeeded())
	{
		Sheet->SetStaticMesh(Plane.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(WinterIceMaterialPath);
	if (Mat.Succeeded())
	{
		Sheet->SetMaterial(0, Mat.Object);
	}
	Sheet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sheet->SetReceivesDecals(false);
	Sheet->SetCastShadow(false);
	// outdoors the sun and the moon light channel 1 only (ALyudSky)
	Sheet->LightingChannels.bChannel0 = true;
	Sheet->LightingChannels.bChannel1 = true;
	Sheet->bUseAsOccluder = false;
	Sheet->SetMobility(EComponentMobility::Movable);
	// 60 km across (the plane is 1 m), it follows the camera in X/Y
	Sheet->SetWorldScale3D(FVector(600.f * 100.f, 600.f * 100.f, 1.f));
}

void ALyudSeaIce::Drive(float SurfaceZ, float ThicknessCm, const FVector& Around, const TFunctionRef<bool(double X, double Y)>& Walkable, bool bForce)
{
	// the sheet follows the camera on a 1 km grid (its material maps by the world position, so the snap is invisible)
	const FVector Snap(FMath::RoundToDouble(Around.X / 100000.0) * 100000.0, FMath::RoundToDouble(Around.Y / 100000.0) * 100000.0, SurfaceZ);
	if (!GetActorLocation().Equals(Snap, 0.5))
	{
		SetActorLocation(Snap);
	}
	const bool bMoved = FVector::DistSquared2D(Around, LaidAt) > FMath::Square(TileCm * 0.5f) || FMath::Abs(SurfaceZ - LaidZ) > 2.f;
	if (!bMoved && !bForce)
	{
		return;
	}
	LaidAt = Around;
	LaidZ = SurfaceZ;
	const int32 R = FMath::CeilToInt(ReachCm / TileCm);
	const int32 Need = (2 * R + 1) * (2 * R + 1);
	while (Tiles.Num() < Need)
	{
		UBoxComponent* B = NewObject<UBoxComponent>(this);
		B->SetCollisionProfileName(TEXT("BlockAll"));
		B->SetCanEverAffectNavigation(false);
		B->SetHiddenInGame(true);
		B->SetMobility(EComponentMobility::Movable);
		B->RegisterComponent();
		Tiles.Add(B);
	}
	const double CX = FMath::RoundToDouble(Around.X / TileCm) * TileCm, CY = FMath::RoundToDouble(Around.Y / TileCm) * TileCm;
	const float HalfT = FMath::Max(ThicknessCm * 0.5f, 4.f);
	Solid = 0;
	int32 K = 0;
	for (int32 DY = -R; DY <= R; DY++)
	{
		for (int32 DX = -R; DX <= R; DX++, K++)
		{
			UBoxComponent* B = Tiles[K];
			const double X = CX + DX * TileCm, Y = CY + DY * TileCm;
			const double H = TileCm * 0.5;
			const bool bHold = FMath::Square(DX * TileCm) + FMath::Square(DY * TileCm) <= FMath::Square(ReachCm + TileCm) &&
			                   Walkable(X, Y) && Walkable(X - H, Y - H) && Walkable(X + H, Y - H) && Walkable(X - H, Y + H) && Walkable(X + H, Y + H);
			if (bHold)
			{
				B->SetBoxExtent(FVector(TileCm * 0.5f, TileCm * 0.5f, HalfT), false);
				B->SetWorldLocation(FVector(X, Y, SurfaceZ - HalfT));
				B->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				Solid++;
			}
			else
			{
				B->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}
}
