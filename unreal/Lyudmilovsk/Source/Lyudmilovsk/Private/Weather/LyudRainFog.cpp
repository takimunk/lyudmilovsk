#include "Weather/LyudRainFog.h"

#include "Components/DecalComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Rendering/Texture2DResource.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "Plants/ProceduralGrass.h"
#include "Sea/LyudSea.h"
#include "Sky/LyudSky.h"
#include "Weather/LyudWeather.h"
#include "Weather/LyudWind.h"
#include "Winter/LyudWinter.h"

namespace
{
	TAutoConsoleVariable<int32> CVarRainFx(TEXT("lyud.Rain.Fx"), 1, TEXT("1: rain drops, splashes and wet ground on screen; 0: off."));
	TAutoConsoleVariable<int32> CVarFogFx(TEXT("lyud.Fog.Fx"), 1, TEXT("1: the weather's haze and fog in the height fog; 0: leave the level's fog alone."));
	TAutoConsoleVariable<int32> CVarRainDebug(TEXT("lyud.Rain.Debug"), 0, TEXT("1: print the rain and fog effects' state on screen."));
	TAutoConsoleVariable<float> CVarFogTop(TEXT("lyud.Weather.FogTop"), -1.f, TEXT("Force the fog layer's depth, m (negative: the weather's)."));
	TAutoConsoleVariable<float> CVarWet(TEXT("lyud.Weather.Wet"), -1.f, TEXT("Force how wet the ground is 0..1 for the effects (negative: the weather's)."));
	TAutoConsoleVariable<float> CVarFogDensityScale(TEXT("lyud.Fog.Scale"), 1.f, TEXT("Scale on the fog's density (look tuning)."));

	const TCHAR* DropMaterialPath = TEXT("/Game/Weather/M_RainDrops.M_RainDrops");
	const TCHAR* SplashMaterialPath = TEXT("/Game/Weather/M_RainSplash.M_RainSplash");
	const TCHAR* WetMaterialPath = TEXT("/Game/Weather/M_WetGround.M_WetGround");
	const TCHAR* CollectionPath = TEXT("/Game/Weather/MPC_LyudWeather.MPC_LyudWeather");

	constexpr float NoRoof = -1.0e7f;

	float Smooth(float A, float B, float X)
	{
		return FMath::SmoothStep(FMath::Min(A, B), FMath::Max(A, B), X) * (A <= B ? 1.f : -1.f) + (A <= B ? 0.f : 1.f);
	}

	uint32 Hash(uint32 X)
	{
		X ^= X >> 16; X *= 0x7feb352dU; X ^= X >> 15; X *= 0x846ca68bU; X ^= X >> 16;
		return X;
	}

	float Rand01(uint32 I, uint32 Salt) { return float(Hash(I * 4u + Salt) & 0xFFFFFF) / float(0x1000000); }

	/** Quads that the vertex shader places: UV0 = corner, UV1 and UV2 = four random numbers per quad. */
	UStaticMesh* BuildQuads(UObject* Outer, FName Name, int32 Count, float Spread, uint32 Seed)
	{
		FMeshDescription MD;
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> VTan = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> VSign = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> VUV = Attr.GetVertexInstanceUVs();
		VUV.SetNumChannels(3);
		const FPolygonGroupID PG = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[PG] = Name;
		MD.ReserveNewVertices(Count * 4);
		MD.ReserveNewVertexInstances(Count * 4);
		MD.ReserveNewTriangles(Count * 2);
		static const FVector2f Corners[4] = { { 0.f, 0.f }, { 1.f, 0.f }, { 1.f, 1.f }, { 0.f, 1.f } };
		for (int32 Q = 0; Q < Count; ++Q)
		{
			const uint32 S = Seed * 7919u + uint32(Q);
			const FVector2f R1(Rand01(S, 0), Rand01(S, 1));
			const FVector2f R2(Rand01(S, 2), Rand01(S, 3));
			// rest positions spread over the box (only the bounds use them; the shader moves every vertex)
			const FVector3f Rest((R1.X - 0.5f) * Spread, (R1.Y - 0.5f) * Spread, (R2.X - 0.5f) * Spread * 0.5f);
			FVertexInstanceID VI[4];
			for (int32 C = 0; C < 4; ++C)
			{
				const FVertexID V = MD.CreateVertex();
				VPos[V] = Rest + FVector3f(Corners[C].X, Corners[C].Y, 0.f);
				VI[C] = MD.CreateVertexInstance(V);
				VNrm[VI[C]] = FVector3f::UpVector;
				VTan[VI[C]] = FVector3f::XAxisVector;
				VSign[VI[C]] = 1.f;
				VUV.Set(VI[C], 0, Corners[C]);
				VUV.Set(VI[C], 1, R1);
				VUV.Set(VI[C], 2, R2);
			}
			MD.CreateTriangle(PG, { VI[0], VI[2], VI[1] });
			MD.CreateTriangle(PG, { VI[0], VI[3], VI[2] });
		}
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, Name, RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, Name));
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Mesh->BuildFromMeshDescriptions({ &MD }, Params);
		return Mesh;
	}

	bool CameraOf(const UWorld* World, FVector& Out)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (PC && PC->PlayerCameraManager)
		{
			Out = PC->PlayerCameraManager->GetCameraLocation();
			return true;
		}
		return false;
	}
}

// ------------------------------------------------------------------------------------------------------------------
// the subsystem: spawning and the weather

ULyudRainFogSubsystem* ULyudRainFogSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudRainFogSubsystem>() : nullptr;
}

bool ULyudRainFogSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId ULyudRainFogSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudRainFogSubsystem, STATGROUP_Tickables);
}

FLyudRainFogWeather ULyudRainFogSubsystem::ReadWeather() const
{
	FLyudRainFogWeather W;
	const UWorld* World = GetWorld();
	const ULyudWeatherSubsystem* Weather = World ? World->GetSubsystem<ULyudWeatherSubsystem>() : nullptr;
	if (!Weather)
	{
		return W;
	}
	const FLyudWeatherState S = Weather->GetState();
	const ULyudWindSubsystem* Wind = World->GetSubsystem<ULyudWindSubsystem>();
	const FVector Mean = Wind ? Wind->GetMeanWind() : FVector::ZeroVector;
	W.Wind = FVector(Mean.X, Mean.Y, 0.0) * 100.0;
	W.Snow = S.SnowFraction;
	W.RainRate = S.PrecipRate * (1.f - S.SnowFraction);
	W.Cloud = S.CloudCover;
	W.Wet = S.Wet;
	// drizzle is its own kind (fine slow drops out of a low deck); a very light rain looks much like it
	const bool bDrizzle = S.Precip == ELyudPrecipType::Drizzle || S.Precip == ELyudPrecipType::FreezingDrizzle;
	W.Drizzle = bDrizzle ? 1.f : 1.f - Smooth(0.15f, 0.6f, S.PrecipRate);
	// the engine's visibility is the worst of the air, the rain and the fog: a fog lies as a layer on the sea or the
	// ground (deep for sea fog and frontal mist, a few metres for a radiation fog), with clearer air above it
	W.VisibilityM = S.VisibilityM;
	W.Fog = S.Fog;
	if (W.Fog > 0.01f)
	{
		W.FogVisibilityM = S.VisibilityM;
		W.VisibilityM = FMath::Min(25000.f / (1.f + 1.1f * W.RainRate), S.VisibilityM * 4.f + 400.f);
		const float DefaultTop = S.FogType == ELyudFogType::Radiation ? 15.f : S.FogType == ELyudFogType::Steam ? 12.f
			: S.FogType == ELyudFogType::Frontal ? 300.f : 80.f;   // advection, or a forced mist
		W.FogTopM = S.FogTopM > 0.f ? S.FogTopM : DefaultTop;
	}
	W.FogBaseM = 0.f;
	if (CVarWet.GetValueOnGameThread() >= 0.f)
	{
		W.Wet = FMath::Clamp(CVarWet.GetValueOnGameThread(), 0.f, 1.f);
	}
	const float ForceTop = CVarFogTop.GetValueOnGameThread();
	if (ForceTop >= 0.f)
	{
		W.FogTopM = ForceTop;
	}
	return W;
}

float ULyudRainFogSubsystem::GetOpenSky() const
{
	return Fx.IsValid() ? Fx->GetOpenSky() : 0.f;
}

void ULyudRainFogSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || Fx.IsValid() || bLooked)
	{
		return;
	}
	// only a world with the open sky gets weather on screen
	bLooked = true;
	TActorIterator<ALyudSky> It(World);
	if (!It)
	{
		return;
	}
	FActorSpawnParameters P;
	P.ObjectFlags |= RF_Transient;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Fx = World->SpawnActor<ALyudRainFx>(ALyudRainFx::StaticClass(), FTransform::Identity, P);
}

FString ULyudRainFogSubsystem::DebugReport() const
{
	const FLyudRainFogWeather W = ReadWeather();
	FString S = FString::Printf(TEXT("RAINFOG rain %.2f mm/h drizzle %.2f snow %.2f wet %.2f vis %.0f m fog %.2f (vis %.0f m, top %.0f m) wind %.1f m/s"),
		W.RainRate, W.Drizzle, W.Snow, W.Wet, W.VisibilityM, W.Fog, W.FogVisibilityM, W.FogTopM, W.Wind.Size2D() / 100.f);
	if (Fx.IsValid())
	{
		S += TEXT(" | ") + Fx->DebugReport();
	}
	return S;
}

// ------------------------------------------------------------------------------------------------------------------
// the effects

ALyudRainFx::ALyudRainFx()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetActorEnableCollision(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	auto MakeMesh = [this](FName Name)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(RootComponent);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->bReceivesDecals = false;
		C->SetGenerateOverlapEvents(false);
		C->bAffectDistanceFieldLighting = false;
		C->bAffectDynamicIndirectLighting = false;
		C->SetMobility(EComponentMobility::Movable);
		return C;
	};
	Drops = MakeMesh(TEXT("Drops"));
	Splashes = MakeMesh(TEXT("Splashes"));

	WetDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("WetDecal"));
	WetDecal->SetupAttachment(RootComponent);
	// projected straight down over the roof map (its X axis is the projection)
	WetDecal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	WetDecal->SortOrder = -10;
}

void ALyudRainFx::BeginPlay()
{
	Super::BeginPlay();
	BuildMeshes();
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, DropMaterialPath))
	{
		DropMID = UMaterialInstanceDynamic::Create(M, this);
		Drops->SetMaterial(0, DropMID);
	}
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, SplashMaterialPath))
	{
		SplashMID = UMaterialInstanceDynamic::Create(M, this);
		Splashes->SetMaterial(0, SplashMID);
	}
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, WetMaterialPath))
	{
		WetMID = UMaterialInstanceDynamic::Create(M, this);
		WetDecal->SetDecalMaterial(WetMID);
	}
	const float Half = RoofCells * RoofCellSize * 0.5f;
	WetDecal->DecalSize = FVector(6000.f, Half, Half);

	RoofCells = FMath::Clamp(RoofCells, 8, 256);
	Roof.Init(FVector2f(NoRoof, 0.f), RoofCells * RoofCells);
	Dirty.Init(1, RoofCells * RoofCells);
	DirtyLeft = Roof.Num();
	RoofTex = UTexture2D::CreateTransient(RoofCells, RoofCells, PF_G32R32F, TEXT("T_RainRoof"));
	RoofTex->Filter = TF_Nearest;
	RoofTex->SRGB = false;
	RoofTex->AddressX = TA_Clamp;
	RoofTex->AddressY = TA_Clamp;
	RoofTex->UpdateResource();
	for (UMaterialInstanceDynamic* MID : { DropMID.Get(), SplashMID.Get(), WetMID.Get() })
	{
		if (MID)
		{
			MID->SetTextureParameterValue(TEXT("Roof"), RoofTex);
		}
	}

	if (TActorIterator<ALyudSky> It(GetWorld()); It)
	{
		Sky = *It;
	}
	Collection = LoadObject<UMaterialParameterCollection>(nullptr, CollectionPath);
	for (TActorIterator<AProceduralGrass> It(GetWorld()); It; ++It)
	{
		Meadows.Add({ It->GetActorTransform(), It->HalfExtent });
	}
	if (const ALyudSea* Sea = ALyudSea::Get(this))
	{
		SeaZ = Sea->SeaLevel();
	}
	for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
	{
		if (UExponentialHeightFogComponent* C = It->GetComponent())
		{
			Fog = C;
			FogBase.Density = C->FogDensity;
			FogBase.Falloff = C->FogHeightFalloff;
			FogBase.Start = C->StartDistance;
			FogBase.Second = C->SecondFogData.FogDensity;
			FogBase.SecondFalloff = C->SecondFogData.FogHeightFalloff;
			FogBase.SecondHeight = C->SecondFogData.FogHeightOffset;
			FogBase.Inscatter = C->FogInscatteringLuminance;
			FogBase.ZOffset = float(C->GetComponentLocation().Z);
			bFogTaken = true;
			break;
		}
	}
	Drops->SetVisibility(false);
	Splashes->SetVisibility(false);
	WetDecal->SetVisibility(false);
}

void ALyudRainFx::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UExponentialHeightFogComponent* C = Fog.Get())
	{
		C->SetFogDensity(FogBase.Density);
		C->SetFogHeightFalloff(FogBase.Falloff);
		C->SetStartDistance(FogBase.Start);
		C->SetSecondFogDensity(FogBase.Second);
		C->SetSecondFogHeightFalloff(FogBase.SecondFalloff);
		C->SetSecondFogHeightOffset(FogBase.SecondHeight);
		C->SetFogInscatteringColor(FogBase.Inscatter);
	}
	Super::EndPlay(Reason);
}

void ALyudRainFx::BuildMeshes()
{
	const float Big = FMath::Max(DropBox.X, DropBox.Y);
	Drops->SetStaticMesh(BuildQuads(this, TEXT("RainDrops"), MaxDrops, Big, 1));
	Splashes->SetStaticMesh(BuildQuads(this, TEXT("RainSplashes"), MaxSplashes, SplashBox, 2));
	// the shader moves the quads anywhere in their box around the camera: keep the bounds over it
	Drops->SetBoundsScale(1.5f);
	Splashes->SetBoundsScale(4.f);
}

bool ALyudRainFx::RoofHeightAt(const FVector& Location, float& OutZ) const
{
	if (Origin.X == MAX_int32)
	{
		return false;
	}
	const int32 X = FMath::FloorToInt(Location.X / RoofCellSize) - Origin.X;
	const int32 Y = FMath::FloorToInt(Location.Y / RoofCellSize) - Origin.Y;
	if (X < 0 || Y < 0 || X >= RoofCells || Y >= RoofCells || Dirty[Y * RoofCells + X])
	{
		return false;
	}
	OutZ = Roof[Y * RoofCells + X].X;
	return true;
}

void ALyudRainFx::UpdateRoofMap(const FVector& Camera)
{
	const int32 N = RoofCells;
	const FIntPoint Want(FMath::FloorToInt(Camera.X / RoofCellSize) - N / 2, FMath::FloorToInt(Camera.Y / RoofCellSize) - N / 2);
	bool bChanged = false;
	if (Want != Origin)
	{
		// slide what is known; the new rows and columns are traced
		const bool bFresh = Origin.X == MAX_int32;
		const FIntPoint D = bFresh ? FIntPoint::ZeroValue : Want - Origin;
		TArray<FVector2f> Old = Roof;
		TArray<uint8> OldDirty = Dirty;
		const bool bFar = bFresh || FMath::Abs(D.X) >= N || FMath::Abs(D.Y) >= N;
		DirtyLeft = 0;
		for (int32 Y = 0; Y < N; ++Y)
		{
			for (int32 X = 0; X < N; ++X)
			{
				const int32 SX = X + D.X, SY = Y + D.Y, I = Y * N + X;
				if (!bFar && SX >= 0 && SY >= 0 && SX < N && SY < N)
				{
					Roof[I] = Old[SY * N + SX];
					Dirty[I] = OldDirty[SY * N + SX];
				}
				else
				{
					Roof[I] = FVector2f(NoRoof, 0.f);
					Dirty[I] = 1;
				}
				DirtyLeft += Dirty[I];
			}
		}
		Origin = Want;
		bChanged = true;
	}

	UWorld* World = GetWorld();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(LyudRainRoof), false);
	Q.AddIgnoredActor(this);
	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		Q.AddIgnoredActor(PC->GetPawn());
	}
	const float Top = float(Camera.Z) + 6000.f, Bottom = float(Camera.Z) - 12000.f;
	auto TraceCell = [&](int32 I)
	{
		const int32 X = I % N, Y = I / N;
		const FVector C((Origin.X + X + 0.5) * RoofCellSize, (Origin.Y + Y + 0.5) * RoofCellSize, Top);
		FVector2f R(NoRoof, 0.f);
		FHitResult Hit;
		FVector From = C;
		// people and animals do not keep the rain off the ground
		for (int32 Try = 0; Try < 4; ++Try)
		{
			if (!World->LineTraceSingleByChannel(Hit, From, FVector(C.X, C.Y, Bottom), ECC_Visibility, Q))
			{
				break;
			}
			if (Hit.GetActor() && Hit.GetActor()->IsA<APawn>())
			{
				From = Hit.ImpactPoint - FVector(0, 0, 5);
				continue;
			}
			R.X = float(Hit.ImpactPoint.Z);
			break;
		}
		if (SeaZ > NoRoof && R.X < SeaZ)
		{
			R = FVector2f(SeaZ, 1.f);
		}
		else
		{
			// rain soaks into a meadow: wet, but no standing water among the grass
			for (const TPair<FTransform, FVector2D>& M : Meadows)
			{
				const FVector L = M.Key.InverseTransformPosition(FVector(C.X, C.Y, R.X));
				if (FMath::Abs(L.X) < M.Value.X && FMath::Abs(L.Y) < M.Value.Y)
				{
					R.Y = 2.f;
					break;
				}
			}
		}
		bChanged |= Roof[I] != R || Dirty[I];
		Roof[I] = R;
		DirtyLeft -= Dirty[I];
		Dirty[I] = 0;
	};
	// a fresh map (spawn, a jump) is traced at once; then the new edges, then a slow refresh for things that move
	int32 Budget = DirtyLeft > N * N / 2 ? N * N : TracesPerFrame;
	for (int32 K = 0; K < N * N && Budget > 0 && DirtyLeft > 0; ++K)
	{
		const int32 I = (Cursor + K) % (N * N);
		if (Dirty[I])
		{
			TraceCell(I);
			--Budget;
		}
	}
	for (int32 K = 0; K < FMath::Min(Budget, TracesPerFrame / 4); ++K)
	{
		Cursor = (Cursor + 1) % (N * N);
		TraceCell(Cursor);
	}

	// open sky around the camera: the share of the nearby cells whose top is below the eye
	int32 Open = 0, Seen = 0;
	const int32 R = FMath::Max(2, FMath::RoundToInt(1200.f / RoofCellSize));
	const int32 CX = FMath::FloorToInt(Camera.X / RoofCellSize) - Origin.X, CY = FMath::FloorToInt(Camera.Y / RoofCellSize) - Origin.Y;
	for (int32 Y = FMath::Max(0, CY - R); Y <= FMath::Min(N - 1, CY + R); ++Y)
	{
		for (int32 X = FMath::Max(0, CX - R); X <= FMath::Min(N - 1, CX + R); ++X)
		{
			const int32 I = Y * N + X;
			if (!Dirty[I] && (X - CX) * (X - CX) + (Y - CY) * (Y - CY) <= R * R)
			{
				++Seen;
				Open += Roof[I].X < Camera.Z + 60.f ? 1 : 0;
			}
		}
	}
	OpenSkyRaw = Seen > 0 ? float(Open) / float(Seen) : 0.f;

	if (bChanged && RoofTex && RoofTex->GetResource())
	{
		const int32 Bytes = Roof.Num() * sizeof(FVector2f);
		uint8* Copy = static_cast<uint8*>(FMemory::Malloc(Bytes));
		FMemory::Memcpy(Copy, Roof.GetData(), Bytes);
		FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, N, N);
		RoofTex->UpdateTextureRegions(0, 1, Region, N * sizeof(FVector2f), sizeof(FVector2f), Copy,
			[](uint8* Data, const FUpdateTextureRegion2D* Regions)
			{
				FMemory::Free(Data);
				delete Regions;
			});
		bRoofUploaded = true;
	}
}

float ALyudRainFx::SkyLuminance(const FLyudRainFogWeather& W) const
{
	// the light of the sky the drops and the fog show: the sun's height and the cloud over it (scene units: the
	// sky's 40 lux sun under the club's fixed exposure), a moonlit floor at night
	float SunAlt = 20.f;
	float SunLux = 40.f;
	if (const ALyudSky* S = Sky.Get())
	{
		SunAlt = S->SunAltitude();
		SunLux = S->SunIlluminance;
	}
	const float Day = Smooth(-7.f, 8.f, SunAlt);
	const float Sin = FMath::Max(FMath::Sin(FMath::DegreesToRadians(FMath::Max(SunAlt, 0.f))), 0.15f);
	const float Diffuse = SunLux * Sin / PI * FMath::Lerp(0.55f, 0.22f, W.Cloud);
	return Diffuse * Day + 0.006f;
}

void ALyudRainFx::UpdateMaterials(const FLyudRainFogWeather& W, const FVector& Camera, float DeltaSeconds)
{
	const bool bOn = CVarRainFx.GetValueOnGameThread() > 0;
	// ease the visible amounts so a forced change or a shower's edge does not pop
	const float K = 1.f - FMath::Exp(-DeltaSeconds / 2.f);
	Smoothed[0] += ((bOn ? W.RainRate : 0.f) - Smoothed[0]) * K;
	Smoothed[1] += (W.Drizzle - Smoothed[1]) * K;
	Smoothed[2] += ((bOn ? W.Wet : 0.f) - Smoothed[2]) * K * 0.5f;
	// on snow the rain soaks in: no splashes, rings or puddles there (the winter's snow cover)
	const float SnowGround = ULyudWinterSubsystem::SnowOnGround(GetWorld(), 0.f);
	const float Rate = Smoothed[0], Drizzle = Smoothed[1], Wet = Smoothed[2] * (1.f - SnowGround);
	const float Light = SkyLuminance(W);

	const bool bDrops = bOn && Rate > 0.02f && OpenSky > 0.001f && DropMID && bRoofUploaded;
	Drops->SetVisibility(bDrops);
	Splashes->SetVisibility(bDrops && SplashMID && Drizzle < 0.9f && SnowGround < 0.5f);
	WetDecal->SetVisibility(bOn && Wet > 0.02f && WetMID && bRoofUploaded && OpenSky > 0.001f);

	const FLinearColor RoofP(Origin.X * RoofCellSize, Origin.Y * RoofCellSize, RoofCellSize, RoofCells);
	if (DropMID)
	{
		// drop speed: drizzle ~2 m/s, steady rain ~6, a downpour's big drops ~8
		const float Fall = FMath::Lerp(550.f + 15.f * FMath::Min(Rate, 20.f), 200.f, Drizzle);
		// how many of the streaks show: the drop count goes as the rate's ~0.6 power, drizzle is many tiny drops
		const float Density = FMath::Clamp(FMath::Lerp(0.04f + FMath::Pow(Rate / 14.f, 0.6f), 0.55f, Drizzle), 0.f, 1.f);
		DropMID->SetVectorParameterValue(TEXT("Box"), FLinearColor(DropBox.X, DropBox.Y, Fall, Density));
		DropMID->SetVectorParameterValue(TEXT("Wind"), FLinearColor(W.Wind.X, W.Wind.Y, FMath::Lerp(0.022f, 0.012f, Drizzle), FMath::Lerp(0.16f, 0.06f, Drizzle)));
		DropMID->SetVectorParameterValue(TEXT("RoofP"), RoofP);
		DropMID->SetScalarParameterValue(TEXT("Light"), Light);
		DropMID->SetScalarParameterValue(TEXT("Alpha"), FMath::Lerp(0.8f, 0.4f, Drizzle) * Smooth(0.02f, 0.3f, Rate));
	}
	if (SplashMID)
	{
		const float Density = FMath::Clamp(FMath::Pow(Rate / 12.f, 0.7f), 0.f, 1.f) * (1.f - Drizzle);
		SplashMID->SetVectorParameterValue(TEXT("Box"), FLinearColor(SplashBox, Density, 0.45f, 0.7f));
		SplashMID->SetVectorParameterValue(TEXT("RoofP"), RoofP);
		SplashMID->SetScalarParameterValue(TEXT("Light"), Light);
	}
	if (WetMID)
	{
		// puddles come after the ground is soaked; rings on them while it rains
		WetMID->SetVectorParameterValue(TEXT("Wet"), FLinearColor(Wet, Smooth(0.45f, 0.95f, Wet), FMath::Clamp(Rate / 8.f, 0.f, 1.f) * (1.f - 0.7f * Drizzle), 0.f));
		WetMID->SetVectorParameterValue(TEXT("RoofP"), RoofP);
	}
	// the sea draws its own rings (M_LyudSea reads Rain from the collection)
	if (UMaterialParameterCollection* MPC = Collection.Get())
	{
		if (MPC->GetVectorParameterByName(TEXT("Rain")))
		{
			UKismetMaterialLibrary::SetVectorParameterValue(this, MPC, TEXT("Rain"),
				FLinearColor(FMath::Clamp(Rate / 8.f, 0.f, 1.f) * (1.f - 0.6f * Drizzle), Drizzle, 0.f, 0.f));
		}
	}
}

void ALyudRainFx::UpdateFog(const FLyudRainFogWeather& W, float DeltaSeconds)
{
	UExponentialHeightFogComponent* C = Fog.Get();
	if (!C || !bFogTaken)
	{
		return;
	}
	const bool bOn = CVarFogFx.GetValueOnGameThread() > 0;
	// outdoors (open sky around) the weather's fog; in the club and the tunnels the level's own
	const float Target = bOn ? Smooth(0.03f, 0.25f, OpenSky) : 0.f;
	FogWeight += (Target - FogWeight) * (1.f - FMath::Exp(-DeltaSeconds / 1.2f));
	if (!bOn && FogWeight < 0.001f)
	{
		return;
	}
	const float Scale = CVarFogDensityScale.GetValueOnGameThread();
	const float Light = SkyLuminance(W);
	// Koschmieder: extinction 3.912 / visibility per metre; the height fog's density is per km of cm (x1000/100)
	// a blizzard's snow is a whiteout (the weather's visibility down to ~10 m): the haze goes that thick in snow
	const float HazeExt = 3.912f / FMath::Max(W.VisibilityM, W.Snow > 0.5f ? 10.f : 50.f);
	const float HazeDensity = HazeExt * 10.f * Scale;
	// the haze thins with height over ~600 m (falloff per 1000 cm: 1000 / scale height in cm)
	const float HazeFalloff = 1000.f / 60000.f;
	const float FogExt = W.Fog > 0.001f ? 3.912f / FMath::Max(W.FogVisibilityM, 20.f) : 0.f;
	const float FogDensity = FogExt * 10.f * Scale * Smooth(0.f, 0.15f, W.Fog);
	// the layer is about even up to its top: full density at half its depth, thinning fast above
	const float FogFalloff = 1000.f / FMath::Max(W.FogTopM * 100.f * 0.35f, 150.f);
	// the component's height is its own Z: both layers are put relative to the sea (or the valley's floor)
	const float Base = (SeaZ > NoRoof ? SeaZ : 0.f) + (W.FogBaseM + 0.5f * W.FogTopM) * 100.f;
	const float A = FogWeight;
	C->SetFogDensity(FMath::Lerp(FogBase.Density, HazeDensity, A));
	C->SetFogHeightFalloff(FMath::Lerp(FogBase.Falloff, HazeFalloff, A));
	C->SetStartDistance(FMath::Lerp(FogBase.Start, 0.f, A));
	C->SetSecondFogDensity(FMath::Lerp(FogBase.Second, FogDensity, A));
	C->SetSecondFogHeightFalloff(FMath::Lerp(FogBase.SecondFalloff, FogFalloff, A));
	C->SetSecondFogHeightOffset(FMath::Lerp(FogBase.SecondHeight, Base - FogBase.ZOffset, A));
	// the haze's colour: the sky's light, a little blue in the clean air, grey in the wet
	FLinearColor Tint = FMath::Lerp(FLinearColor(0.6f, 0.7f, 0.88f), FLinearColor(0.7f, 0.74f, 0.78f), FMath::Max(W.Cloud, W.Fog));
	// thick snow in the air scatters white
	Tint = FMath::Lerp(Tint, FLinearColor(0.86f, 0.88f, 0.9f), W.Snow * FMath::SmoothStep(400.f, 40.f, W.VisibilityM));
	C->SetFogInscatteringColor(FMath::Lerp(FogBase.Inscatter, Tint * Light, A));
	// not on the star dome (ALyudSky's, 200 km out): the atmosphere draws the sky's haze itself, reddened toward a low
	// sun; this flat haze over it veiled the setting disk into a pale ghost
	if (!FMath::IsNearlyEqual(C->FogCutoffDistance, 1.5e7f))
	{
		C->SetFogCutoffDistance(1.5e7f);
	}
}

void ALyudRainFx::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	FVector Camera;
	if (!World || !CameraOf(World, Camera))
	{
		return;
	}
	SetActorLocation(Camera);
	// the decal sits over the map's middle, its top well above the camera
	WetDecal->SetWorldLocation(FVector(Camera.X, Camera.Y, Camera.Z));
	UpdateRoofMap(Camera);
	OpenSky += (OpenSkyRaw - OpenSky) * (1.f - FMath::Exp(-DeltaSeconds / 0.5f));
	const ULyudRainFogSubsystem* Sub = ULyudRainFogSubsystem::Get(this);
	const FLyudRainFogWeather W = Sub ? Sub->ReadWeather() : FLyudRainFogWeather();
	UpdateMaterials(W, Camera, DeltaSeconds);
	UpdateFog(W, DeltaSeconds);
	if (CVarRainDebug.GetValueOnGameThread() > 0 && GEngine && Sub)
	{
		GEngine->AddOnScreenDebugMessage(int32(0x4A1F06), 0.f, FColor::Cyan, Sub->DebugReport());
	}
}

FString ALyudRainFx::DebugReport() const
{
	return FString::Printf(TEXT("open sky %.2f fog weight %.2f shown rain %.2f wet %.2f drops %d splashes %d decal %d roof dirty %d sea %.0f"),
		OpenSky, FogWeight, Smoothed[0], Smoothed[2], Drops && Drops->IsVisible() ? 1 : 0, Splashes && Splashes->IsVisible() ? 1 : 0,
		WetDecal && WetDecal->IsVisible() ? 1 : 0, DirtyLeft, SeaZ);
}
