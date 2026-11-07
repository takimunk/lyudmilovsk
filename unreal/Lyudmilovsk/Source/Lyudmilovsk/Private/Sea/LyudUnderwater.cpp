#include "Sea/LyudUnderwater.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FishModel.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "Plants/GameCalendar.h"
#include "Sea/LyudSea.h"
#include "Sea/LyudSeabed.h"
#include "SeaCurrent.h"
#include "SeaLifeMeshes.h"
#include "Sky/LyudSky.h"
#include "StaticMeshAttributes.h"
#include "Weather/LyudWind.h"

namespace
{
	TAutoConsoleVariable<int32> CVarSeaLife(TEXT("lyud.SeaLife"), 1, TEXT("0: no underwater life (fish, motes, the current's materials)."));
	TAutoConsoleVariable<int32> CVarSeaLifeFish(TEXT("lyud.SeaLife.Fish"), 1, TEXT("0: no fish."));
	TAutoConsoleVariable<float> CVarSeaLifeDensity(TEXT("lyud.SeaLife.Density"), 1.f, TEXT("Multiplies the fish's numbers (graphics levels: Effects)."), ECVF_Scalability);
	TAutoConsoleVariable<int32> CVarSeaLifeDebug(TEXT("lyud.SeaLife.Debug"), 0, TEXT("1: print the underwater life's state every second."));
	TAutoConsoleVariable<float> CVarCurrentSpeed(TEXT("lyud.Current.Speed"), -99.f, TEXT("Forces the current along the coast, m/s (+ north); -99 = the tide's and the wind's."));
	TAutoConsoleVariable<int32> CVarMotes(TEXT("lyud.SeaLife.Motes"), 1, TEXT("0: no motes drifting in the water (graphics levels: Effects)."), ECVF_Scalability);

	/** How each species' mesh swims in M_LyudFish (the generator's species are blender/fish/species/<Id>.py). */
	struct FLook
	{
		float Silver;
		float Amp;
	};
	FLook LookOf(const LyudFish::FSpecies& S)
	{
		const bool bSilver = S.Way == LyudFish::EWay::School || S.Way == LyudFish::EWay::Surface || FCString::Strcmp(ANSI_TO_TCHAR(S.Id), TEXT("SeaTrout")) == 0;
		return { bSilver ? 0.35f : 0.f, S.Bend > 0.8f ? 0.07f : 0.11f };
	}

	constexpr float MoteBox = 1400.f;
	constexpr int32 MoteCount = 1800;

	UStaticMesh* BuildMoteMesh(UObject* Outer)
	{
		// each mote a quad whose four corners sit on one random seed point in the box (the material spreads and wraps them)
		FMeshDescription MD;
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> VTan = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> VSign = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> VUV = Attr.GetVertexInstanceUVs();
		VUV.SetNumChannels(2);
		const FPolygonGroupID PG = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[PG] = TEXT("Motes");
		FRandomStream R(4242);
		static const FVector2f Corner[4] = { { -1.f, -1.f }, { 1.f, -1.f }, { 1.f, 1.f }, { -1.f, 1.f } };
		for (int32 M = 0; M < MoteCount; M++)
		{
			const FVector3f Seed(R.FRand() * MoteBox, R.FRand() * MoteBox, R.FRand() * MoteBox);
			const float Rnd = R.FRand();
			FVertexInstanceID VI[4];
			for (int32 K = 0; K < 4; K++)
			{
				const FVertexID V = MD.CreateVertex();
				VPos[V] = Seed;
				VI[K] = MD.CreateVertexInstance(V);
				VNrm[VI[K]] = FVector3f::UpVector;
				VTan[VI[K]] = FVector3f::XAxisVector;
				VSign[VI[K]] = 1.f;
				VUV.Set(VI[K], 0, Corner[K]);
				VUV.Set(VI[K], 1, FVector2f(Rnd, 0.f));
			}
			MD.CreateTriangle(PG, { VI[0], VI[1], VI[2] });
			MD.CreateTriangle(PG, { VI[0], VI[2], VI[3] });
		}
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, TEXT("SM_LyudMotes"), RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Motes")));
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Mesh->BuildFromMeshDescriptions({ &MD }, Params);
		return Mesh;
	}

	/**
	 * lyud.Underwater.View X Y Z Pitch Yaw [Fov]: look from a camera of its own (world cm, degrees), the hero left where it is
	 * (the sea would hold a swimming hero's eyes over the waves); "lyud.Underwater.View off" gives the view back. For the
	 * offscreen tests and screenshots.
	 */
	FAutoConsoleCommandWithWorldAndArgs CmdView(TEXT("lyud.Underwater.View"), TEXT("X Y Z Pitch Yaw [Fov] (cm, deg): look from a free camera; off: back to the hero."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			if (!PC) return;
			static TWeakObjectPtr<ACameraActor> Cam;
			if (Args.Num() < 5)
			{
				if (Cam.IsValid()) Cam->Destroy();
				if (PC->GetPawn()) PC->SetViewTarget(PC->GetPawn());
				return;
			}
			if (!Cam.IsValid() || Cam->GetWorld() != World)
			{
				FActorSpawnParameters P;
				P.ObjectFlags |= RF_Transient;
				Cam = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, P);
				if (!Cam.IsValid()) return;
				Cam->GetCameraComponent()->bConstrainAspectRatio = false;
			}
			Cam->SetActorLocationAndRotation(FVector(FCString::Atod(*Args[0]), FCString::Atod(*Args[1]), FCString::Atod(*Args[2])),
				FRotator(FCString::Atof(*Args[3]), FCString::Atof(*Args[4]), 0.f));
			Cam->GetCameraComponent()->SetFieldOfView(Args.Num() > 5 ? FCString::Atof(*Args[5]) : 80.f);
			if (PC->GetViewTarget() != Cam.Get()) PC->SetViewTarget(Cam.Get());
		}));

	template <typename T>
	T* LoadIfThere(const TCHAR* Path)
	{
		// never load just to see whether it exists (the perf checklist): ask the package first
		const FString Package = FPackageName::ObjectPathToPackageName(FString(Path));
		return FPackageName::DoesPackageExist(Package) ? LoadObject<T>(nullptr, Path) : nullptr;
	}
}

/** The water and bed the fish live in, from the sea, the ground (traced once per metre and kept) and the terrain mask. */
struct FLyudUnderwaterEnv : LyudFish::FEnv
{
	ULyudUnderwaterSubsystem* Owner = nullptr;
	TWeakObjectPtr<ALyudSea> Sea;
	float Level = 0.f;
	mutable TMap<FIntPoint, float> Beds;

	float Cell(int32 X, int32 Y) const
	{
		const FIntPoint K(X, Y);
		if (const float* Z = Beds.Find(K)) return *Z;
		bool bOk = false;
		const float Z = Sea.IsValid() ? Sea->GroundZ(X * 100.0, Y * 100.0, bOk) * 0.01f : 0.f;
		if (Beds.Num() > 40000) Beds.Reset();
		return Beds.Add(K, bOk ? Z : 1e6f);
	}
	virtual bool Bed(float X, float Y, float& OutZ) const override
	{
		const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
		const float FX = X - X0, FY = Y - Y0;
		const float A = Cell(X0, Y0), B = Cell(X0 + 1, Y0), C = Cell(X0, Y0 + 1), D = Cell(X0 + 1, Y0 + 1);
		if (FMath::Max(FMath::Max(A, B), FMath::Max(C, D)) > 1e5f) return false;
		OutZ = FMath::Lerp(FMath::Lerp(A, B, FX), FMath::Lerp(C, D, FX), FY);
		return OutZ < Level - 0.1f;
	}
	virtual float SurfaceZ() const override { return Level; }
	virtual void Habitat(float X, float Y, float& Sand, float& Stone, float& Weed) const override
	{
		float Bz;
		const float Depth = Bed(X, Y, Bz) ? Level - Bz : 0.f;
		ULyudSeabedComponent::HabitatAt(X * 100.0, Y * 100.0, Depth, Sand, Stone, Weed);
	}
	virtual LyudFish::FV Current(const LyudFish::FV& P) const override
	{
		const FVector U = Owner->CurrentCm(FVector(P.X, P.Y, P.Z) * 100.f, false) * 0.01f;
		return LyudFish::FV(U.X, U.Y, U.Z);
	}
};

ALyudSeaLife::ALyudSeaLife()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

UInstancedStaticMeshComponent* ALyudSeaLife::SpeciesMesh(int32 Species)
{
	if (Fish.Num() < LyudFish::NumSpecies()) Fish.SetNum(LyudFish::NumSpecies());
	if (Fish[Species]) return Fish[Species];
	const LyudFish::FSpecies& S = LyudFish::Species(Species);
	const FString Id = ANSI_TO_TCHAR(S.Id);
	UStaticMesh* Mesh = LoadIfThere<UStaticMesh>(*FString::Printf(TEXT("/Game/Sea/Fish/SM_%s.SM_%s"), *Id, *Id));
	UTexture2D* Skin = Mesh ? LoadIfThere<UTexture2D>(*FString::Printf(TEXT("/Game/Sea/Fish/T_%s_D.T_%s_D"), *Id, *Id)) : nullptr;
	UMaterialInterface* Body = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Sea/Underwater/M_LyudFish.M_LyudFish"));
	UMaterialInterface* Fin = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Sea/Underwater/M_LyudFishFin.M_LyudFishFin"));
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, *FString::Printf(TEXT("Fish_%s"), *Id), RF_Transient);
	C->SetupAttachment(GetRootComponent());
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCastShadow(false);
	// they move every frame: out of the Lumen scene and distance fields (the perf checklist)
	C->bAffectDynamicIndirectLighting = false;
	C->bAffectDistanceFieldLighting = false;
	C->SetCullDistances(0, 4500);
	C->SetNumCustomDataFloats(3);
	if (Mesh && Body)
	{
		C->SetStaticMesh(Mesh);
		const FLook L = LookOf(S);
		for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); Slot++)
		{
			const bool bFin = Mesh->GetStaticMaterials()[Slot].MaterialSlotName.ToString().Contains(TEXT("Fin"));
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(bFin && Fin ? Fin : Body, C);
			if (Skin) M->SetTextureParameterValue(TEXT("Skin"), Skin);
			M->SetScalarParameterValue(TEXT("BodyLength"), S.Length * 100.f);
			M->SetScalarParameterValue(TEXT("Waves"), S.Waves);
			M->SetScalarParameterValue(TEXT("Eel"), S.Bend > 0.8f ? 1.f : 0.f);
			M->SetScalarParameterValue(TEXT("Amp"), L.Amp);
			M->SetScalarParameterValue(TEXT("Flat"), S.Way == LyudFish::EWay::Flat ? 1.f : 0.f);
			M->SetScalarParameterValue(TEXT("Silver"), L.Silver);
			C->SetMaterial(Slot, M);
		}
	}
	else
	{
		// no model built for it yet: the old code-built fish (20 cm), so the behaviour still shows
		StandIns.AddUnique(FName(*Id));
		C->SetStaticMesh(LyudSeaLife::Fish());
		if (UMaterialInterface* Old = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Sea/M_SeaFish.M_SeaFish")))
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Old, C);
			M->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.5f, 0.52f, 0.5f));
			M->SetScalarParameterValue(TEXT("Wiggle"), 1.f);
			C->SetMaterial(0, M);
		}
	}
	C->RegisterComponent();
	Fish[Species] = C;
	return C;
}

ULyudUnderwaterSubsystem::ULyudUnderwaterSubsystem() = default;
ULyudUnderwaterSubsystem::~ULyudUnderwaterSubsystem() = default;

ULyudUnderwaterSubsystem* ULyudUnderwaterSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULyudUnderwaterSubsystem>() : nullptr;
}

bool ULyudUnderwaterSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId ULyudUnderwaterSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudUnderwaterSubsystem, STATGROUP_Tickables);
}

void ULyudUnderwaterSubsystem::Deinitialize()
{
	Release();
	Super::Deinitialize();
}

void ULyudUnderwaterSubsystem::Release()
{
	if (Sim) Sim->Clear();
	if (Env) Env->Beds.Empty();
	if (ALyudSeaLife* L = Life.Get())
	{
		L->Destroy();
	}
	Life.Reset();
	for (TArray<FTransform>& X : Xf) X.Empty();
	for (TArray<float>& D : Data) D.Empty();
}

void ULyudUnderwaterSubsystem::UpdateDrive(ALyudSea* Sea, float Dt)
{
	// the waves' wind (the last hours' mean) drives the longshore current; the tide's rate the tidal stream
	if (const ULyudWindSubsystem* Wind = ULyudWindSubsystem::Get(this))
	{
		const FVector W = Wind->GetSeaStateWind();
		// GetSeaStateWind: direction the wind blows toward times speed (m/s) in X/Y
		WindX = W.X;
		WindY = W.Y;
	}
	const float Tide = Sea->TideOffsetCm();
	if (bTideSeen && Dt > 0.f)
	{
		// eased: the tide's level is stepped by the sky's clock, its rate is smooth over minutes
		const float Rate = FMath::Clamp((Tide - LastTideCm) * 0.01f / Dt, -2e-3f, 2e-3f);
		TideRate += (Rate - TideRate) * FMath::Clamp(Dt / 20.f, 0.f, 1.f);
	}
	LastTideCm = Tide;
	bTideSeen = true;
	Spring = Sea->TideSpring();
}

FVector ULyudUnderwaterSubsystem::CurrentCm(const FVector& Location, bool bEddies) const
{
	const ALyudSea* Sea = SeaRef.Get();
	if (!Sea) return FVector::ZeroVector;
	LyudSeaCurrent::FDrive D;
	D.WindX = WindX;
	D.WindY = WindY;
	D.TideRate = TideRate;
	D.Spring = Spring;
	D.Time = Clock;
	LyudSeaCurrent::FPlace P;
	P.X = Location.X * 0.01;
	P.Y = Location.Y * 0.01;
	P.Depth = Sea->DepthAt(Location) * 0.01f;
	const float Level = Sea->SeaLevel();
	P.OverBed = FMath::Max(P.Depth - (Level - Location.Z) * 0.01f, 0.f);
	float Sand, Stone, Weed;
	ULyudSeabedComponent::HabitatAt(Location.X, Location.Y, P.Depth, Sand, Stone, Weed);
	P.Rough = Stone;
	const float Forced = CVarCurrentSpeed.GetValueOnGameThread();
	if (Forced > -98.f)
	{
		// a forced current along the coast: the tide's rate that gives it at mid water
		D.WindX = D.WindY = 0.f;
		D.TideRate = 3.2e-4f * Forced / 0.32f;
	}
	const LyudSeaCurrent::FVec3 U = bEddies ? LyudSeaCurrent::At(D, P) : LyudSeaCurrent::Mean(D, P);
	return FVector(U.X, U.Y, U.Z) * 100.f;
}

FVector ULyudUnderwaterSubsystem::CurrentAt(const FVector& Location) const { return CurrentCm(Location, true); }
FVector ULyudUnderwaterSubsystem::MeanCurrentAt(const FVector& Location) const { return CurrentCm(Location, false); }

void ULyudUnderwaterSubsystem::UpdateCollection(ALyudSea* Sea, const FVector& Eye, bool bUnder, float Dt)
{
	if (!Collection)
	{
		Collection = LoadIfThere<UMaterialParameterCollection>(TEXT("/Game/Sea/Underwater/MPC_LyudUnderwater.MPC_LyudUnderwater"));
		if (!Collection) return;
	}
	// the current at the eye's depth, a metre over the bed when the eye is high (where the weed is)
	const float Level = Sea->SeaLevel();
	const float Depth = Sea->DepthAt(Eye);
	FVector At = Eye;
	At.Z = FMath::Min(Eye.Z, Level - FMath::Max(Depth - 100.f, 20.f));
	EyeCurrent = MeanCurrentAt(At);
	float Sand, Stone, Weed;
	ULyudSeabedComponent::HabitatAt(Eye.X, Eye.Y, Depth * 0.01f, Sand, Stone, Weed);
	LyudSeaCurrent::FDrive D;
	D.WindX = WindX; D.WindY = WindY; D.TideRate = TideRate;
	LyudSeaCurrent::FPlace P;
	P.Depth = Depth * 0.01f;
	P.OverBed = 1.f;
	P.Rough = Stone;
	const float Turb = LyudSeaCurrent::Turbulence(D, P);
	// the water's drift (m), wrapped far beyond the motes' box so they never jump
	Drift += EyeCurrent * 0.01f * Dt;
	for (int32 K = 0; K < 3; K++) if (FMath::Abs(Drift[K]) > 1000.0) Drift[K] -= FMath::Sign(Drift[K]) * 1400.0;
	const float T = float(FMath::Fmod(Clock, 3600.0));
	UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("Current"), FLinearColor(EyeCurrent.X * 0.01f, EyeCurrent.Y * 0.01f, EyeCurrent.Z * 0.01f, Turb));
	UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("CurrentDrift"), FLinearColor(Drift.X, Drift.Y, Drift.Z, T));
	float Light = 1.f;
	if (TActorIterator<ALyudSky> It(GetWorld()); It)
	{
		Light = 0.03f + 0.97f * FMath::SmoothStep(-4.f, 12.f, It->SunAltitude());
	}
	// the light fades with depth too (the water takes it in a few metres)
	const float EyeDepthM = FMath::Max(Level - Eye.Z, 0.f) * 0.01f;
	UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("Eye"), FLinearColor(bUnder ? 1.f : 0.f, EyeDepthM, Level, Light * FMath::Exp(-EyeDepthM * 0.12f)));
}

void ULyudUnderwaterSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld()) return;
	ALyudSea* Sea = ALyudSea::Get(World);
	const APlayerController* PC = World->GetFirstPlayerController();
	if (!Sea || !PC || !PC->PlayerCameraManager || CVarSeaLife.GetValueOnGameThread() == 0)
	{
		if (bActive) Release();
		bActive = false;
		return;
	}
	SeaRef = Sea;
	Clock += DeltaTime;
	UpdateDrive(Sea, DeltaTime);
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const bool bUnder = Sea->IsCameraUnderwater();
	// near: under the water, or over water at least a metre deep within a few metres over the surface
	const float Level = Sea->SeaLevel();
	const bool bNear = bUnder || (Sea->DepthAt(Eye) > 100.f && Eye.Z < Level + 500.f);
	UpdateCollection(Sea, Eye, bUnder, DeltaTime);
	if (!bNear || !Sea->IsDepthReady())
	{
		if (bActive) Release();
		bActive = false;
		return;
	}
	bActive = true;
	ALyudSeaLife* L = Life.Get();
	if (!L)
	{
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		L = World->SpawnActor<ALyudSeaLife>(ALyudSeaLife::StaticClass(), FTransform::Identity, P);
		Life = L;
		if (!L) return;
	}
	// the motes round the eye (one mesh; the material places and carries them)
	if (!L->Motes)
	{
		L->MoteMesh = BuildMoteMesh(L);
		L->Motes = NewObject<UStaticMeshComponent>(L, TEXT("Motes"), RF_Transient);
		L->Motes->SetupAttachment(L->GetRootComponent());
		L->Motes->SetMobility(EComponentMobility::Movable);
		L->Motes->SetStaticMesh(L->MoteMesh);
		L->Motes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		L->Motes->SetCastShadow(false);
		L->Motes->bAffectDynamicIndirectLighting = false;
		L->Motes->bAffectDistanceFieldLighting = false;
		// the quads are moved to the eye in the material: never cull the mesh by its seed box
		L->Motes->SetBoundsScale(1.0e4f);
		if (UMaterialInterface* M = LoadIfThere<UMaterialInterface>(TEXT("/Game/Sea/Underwater/M_LyudSuspended.M_LyudSuspended")))
		{
			L->Motes->SetMaterial(0, M);
		}
		L->Motes->RegisterComponent();
	}
	L->Motes->SetVisibility(bUnder && CVarMotes.GetValueOnGameThread() != 0);

	// the fish
	if (!Sim)
	{
		Sim = MakePimpl<LyudFish::FSim>();
		Env = MakePimpl<FLyudUnderwaterEnv>();
		Env->Owner = this;
	}
	Env->Sea = Sea;
	Env->Level = Level * 0.01f;
	Sim->Density = FMath::Max(CVarSeaLifeDensity.GetValueOnGameThread(), 0.f);
	LyudFish::FClock FC;
	FDateTime Now = UGameCalendar::NowIn(World);
	float SunAlt = 30.f;
	if (TActorIterator<ALyudSky> It(World); It)
	{
		Now = It->ShownTime();
		SunAlt = It->SunAltitude();
	}
	FC.Month = Now.GetMonth();
	FC.Daylight = FMath::SmoothStep(-6.f, 6.f, SunAlt);
	if (CVarSeaLifeFish.GetValueOnGameThread() != 0)
	{
		Sim->Step(DeltaTime, *Env, LyudFish::FV(Eye.X, Eye.Y, Eye.Z) * 0.01f, FC);
	}
	else
	{
		Sim->Clear();
	}
	DrawFish(DeltaTime);

	if (CVarSeaLifeDebug.GetValueOnGameThread() != 0 && (DebugTimer -= DeltaTime) <= 0.f)
	{
		DebugTimer = 1.f;
		UE_LOG(LogTemp, Display, TEXT("LyudUnderwater: %s"), *GetDebugReport());
	}
}

void ULyudUnderwaterSubsystem::DrawFish(float Dt)
{
	ALyudSeaLife* L = Life.Get();
	if (!L || !Sim) return;
	const int32 NS = LyudFish::NumSpecies();
	Xf.SetNum(NS);
	Data.SetNum(NS);
	for (int32 S = 0; S < NS; S++)
	{
		Xf[S].Reset();
		Data[S].Reset();
	}
	for (const LyudFish::FFish& F : Sim->Fish())
	{
		if (F.bHidden) continue;
		const LyudFish::FSpecies& Sp = LyudFish::Species(F.Species);
		// the mesh is built at the species' length; a flatfish lies on the bed (its mesh is already flat)
		const FRotator R(FMath::RadiansToDegrees(F.Pitch), FMath::RadiansToDegrees(F.Yaw), FMath::RadiansToDegrees(F.Roll));
		Xf[F.Species].Add(FTransform(R, FVector(F.Pos.X, F.Pos.Y, F.Pos.Z) * 100.f, FVector(F.Len / Sp.Length)));
		Data[F.Species].Append({ FMath::Fmod(F.Phase, 2.f * PI * 16.f), F.Beat, F.Bend });
	}
	for (int32 S = 0; S < NS; S++)
	{
		UInstancedStaticMeshComponent* C = (Xf[S].Num() || (L->Fish.IsValidIndex(S) && L->Fish[S])) ? L->SpeciesMesh(S) : nullptr;
		if (!C) continue;
		if (C->GetInstanceCount() != Xf[S].Num())
		{
			// the number changed (a school came or went): rebuild this species' instances (rare)
			C->ClearInstances();
			if (Xf[S].Num()) C->AddInstances(Xf[S], false, true, false);
		}
		else if (Xf[S].Num())
		{
			// the usual frame: move the instances in place (never mark the render state dirty: the perf checklist)
			C->BatchUpdateInstancesTransforms(0, Xf[S], true, false);
		}
		if (Xf[S].Num()) C->SetCustomData(0, Xf[S].Num() - 1, Data[S], false);
	}
}

int32 ULyudUnderwaterSubsystem::GetFishCount(FName Species) const
{
	const int32 S = LyudFish::FindSpecies(TCHAR_TO_ANSI(*Species.ToString()));
	return Sim && S >= 0 ? Sim->Count(S) : 0;
}

FVector ULyudUnderwaterSubsystem::FindNearestFish(FName Species, const FVector& From) const
{
	const int32 S = LyudFish::FindSpecies(TCHAR_TO_ANSI(*Species.ToString()));
	LyudFish::FV Out;
	if (Sim && S >= 0 && Sim->Nearest(S, LyudFish::FV(From.X, From.Y, From.Z) * 0.01f, Out))
	{
		return FVector(Out.X, Out.Y, Out.Z) * 100.f;
	}
	return From;
}

TArray<FName> ULyudUnderwaterSubsystem::GetSpecies() const
{
	TArray<FName> Out;
	for (int32 S = 0; S < LyudFish::NumSpecies(); S++) Out.Add(FName(ANSI_TO_TCHAR(LyudFish::Species(S).Id)));
	return Out;
}

FString ULyudUnderwaterSubsystem::GetDebugReport() const
{
	FString Out = FString::Printf(TEXT("underwater: %s, current at the eye %.2f m/s toward %.0f deg (tide rate %.2f mm/min, sea wind %.1f %.1f m/s), drift %.1f %.1f m;"),
		bActive ? TEXT("active") : TEXT("idle"), EyeCurrent.Size2D() * 0.01f, FMath::RadiansToDegrees(FMath::Atan2(EyeCurrent.Y, EyeCurrent.X)),
		TideRate * 60000.f, WindX, WindY, Drift.X, Drift.Y);
	int32 Total = 0;
	if (Sim)
	{
		for (int32 S = 0; S < LyudFish::NumSpecies(); S++)
		{
			const int32 N = Sim->Count(S);
			Total += N;
			if (N) Out += FString::Printf(TEXT(" %s %d"), ANSI_TO_TCHAR(LyudFish::Species(S).Id), N);
		}
	}
	Out += FString::Printf(TEXT("; fish %d"), Total);
	if (const ALyudSeaLife* L = Life.Get())
	{
		if (L->StandIns.Num())
		{
			Out += TEXT("; stand-in meshes:");
			for (const FName& N : L->StandIns) Out += TEXT(" ") + N.ToString();
		}
	}
	return Out;
}
