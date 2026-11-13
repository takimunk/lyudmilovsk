#include "World/LyudCityBuilding.h"

#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "People/PeopleLife.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Plants/GameCalendar.h"
#include "Sky/LyudSky.h"
#include "TextureResource.h"

namespace
{
	const FName CityTag(TEXT("City"));

	void ApplyCityVisibility(bool bOn)
	{
		if (!GEngine)
		{
			return;
		}
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			UWorld* World = Ctx.World();
			if (!World || !World->IsGameWorld())
			{
				continue;
			}
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->ActorHasTag(CityTag))
				{
					It->SetActorHiddenInGame(!bOn);
					It->SetActorEnableCollision(bOn);
				}
			}
		}
	}

	TAutoConsoleVariable<int32> CVarCity(TEXT("lyud.City"), 1,
		TEXT("The town's buildings (tag City, unreal/scripts/import_city.py): shown (1) or hidden with no collision (0)."),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* V) { ApplyCityVisibility(V->GetInt() != 0); }), ECVF_Default);

	TAutoConsoleVariable<int32> CVarCityLights(TEXT("lyud.City.Lights"), 0,
		TEXT("Town windows: 0 = auto (the life simulation or the seeded evening), 1 = every window lit, 2 = all dark."), ECVF_Default);

	TAutoConsoleVariable<int32> CVarCityLampBudget(TEXT("lyud.City.LampBudget"), 24,
		TEXT("Town room lamps: at most this many point lights (no shadows), given to the lit lamps nearest the camera. 0 = none."),
		ECVF_Default);

	/** Lamps farther than this from the camera get no light (the pool fades them out over the last LampFade cm). */
	constexpr float LampReach = 4500.f;
	constexpr float LampFade = 600.f;
	/** A lamp that already has a pool light keeps it unless another is this much nearer (no blinking at the edge). */
	constexpr float LampKeep = 400.f;

	FAutoConsoleCommandWithWorld CmdCityReport(TEXT("lyud.City.Report"), TEXT("Town buildings and their lit windows."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (ULyudWindowLights* L = ULyudWindowLights::Get(World))
			{
				UE_LOG(LogTemp, Display, TEXT("%s"), *L->Report());
			}
		}));

	/** A stable 0..1 from a string and a salt. */
	float Hash01(const FString& S, uint32 Salt)
	{
		const uint32 H = HashCombine(GetTypeHash(S), Salt * 2654435761u);
		return float((H * 747796405u + 2891336453u) >> 8) / float(1 << 24);
	}

	/** Is Hour inside [From, To) on a 24 h clock (To may pass midnight)? */
	bool InHours(float Hour, float From, float To)
	{
		if (To - From >= 24.f)
		{
			return true;                                   // open round the clock (lighthouse, sanatorium)
		}
		From = FMath::Fmod(From + 48.f, 24.f);
		To = FMath::Fmod(To + 48.f, 24.f);
		return From <= To ? (Hour >= From && Hour < To) : (Hour >= From || Hour < To);
	}
}

// ------------------------------------------------------------------------------------------------ the building

/** Slot -> first byte of its texel. citykit.slot_uv puts slot row r at v = (r + 0.5) / 64 in Blender; the glTF export
 * writes 1 - v and Unreal keeps glTF UVs, so the card samples texture row 63 - r. */
static int32 TexelOf(int32 Slot)
{
	const int32 N = ALyudCityBuilding::LightTexSize;
	return ((N - 1 - Slot / N) * N + Slot % N) * 4;
}

ALyudCityBuilding::ALyudCityBuilding()
{
	PrimaryActorTick.bCanEverTick = false;
	Shell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shell"));
	RootComponent = Shell;
	Shell->SetMobility(EComponentMobility::Static);
	Detail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Detail"));
	Detail->SetupAttachment(Shell);
	Detail->SetMobility(EComponentMobility::Static);
	Detail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Windows = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Windows"));
	Windows->SetupAttachment(Shell);
	Windows->SetMobility(EComponentMobility::Static);
	Windows->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Windows->SetCastShadow(false);
	// the cards glow (lit rooms at night outside, the daylight pane M_City_WindowIn inside): out of Lumen's scene and
	// the distance fields, as ALyudCityWindows' panes, so their emission lights nothing and costs no GI
	Windows->SetAffectDistanceFieldLighting(false);
	Windows->bAffectDynamicIndirectLighting = false;
	Tags.Add(CityTag);
}

void ALyudCityBuilding::BeginPlay()
{
	Super::BeginPlay();
	MakeLightTexture();
	ClearOwned();
	Seeds.SetNum(WindowIds.Num());
	const FString Plot = PlotId.ToString();
	for (int32 S = 0; S < WindowIds.Num(); ++S)
	{
		FWindowSeed& D = Seeds[S];
		const FString Full = Plot + TEXT("/") + (WindowFlats.IsValidIndex(S) ? WindowFlats[S] : FString());
		D.Nobody = Hash01(Full, 1);
		D.Bed = 21.5f + Hash01(Full, 2) * 4.0f;            // 21:30 .. 01:30
		D.Home = 15.5f + Hash01(Full, 3) * 4.0f;           // 15:30 .. 19:30
		D.Early = Hash01(Full, 4);
		D.Up = 5.0f + Hash01(Full, 6) * 2.5f;
		D.bRoom = Hash01(WindowIds[S], 5) < 0.7f;          // not every room of a lit flat burns
	}
	if (ULyudWindowLights* L = ULyudWindowLights::Get(this))
	{
		L->Register(this);
	}
	if (CVarCity.GetValueOnGameThread() == 0)
	{
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
	}
}

void ALyudCityBuilding::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ULyudWindowLights* L = ULyudWindowLights::Get(this))
	{
		L->Unregister(this);
	}
	Super::EndPlay(Reason);
}

void ALyudCityBuilding::MakeLightTexture()
{
	if (!Windows || !Windows->GetStaticMesh() || WindowIds.Num() == 0)
	{
		return;
	}
	const int32 N = LightTexSize;
	Texels.SetNumZeroed(N * N * 4);
	for (int32 S = 0; S < WindowIds.Num() && S < N * N; ++S)
	{
		// per window, fixed: how warm its lamp is and how much curtain it has (the shader reads G and B)
		const FString Key = PlotId.ToString() + TEXT("/") + WindowIds[S];
		const int32 T = TexelOf(S);
		Texels[T + 1] = uint8(FMath::Clamp(Hash01(Key, 11) * 255.f, 0.f, 255.f));
		Texels[T + 0] = uint8(FMath::Clamp(Hash01(Key, 23) * 255.f, 0.f, 255.f));
		Texels[T + 3] = 255;
	}
	LightTex = UTexture2D::CreateTransient(N, N, PF_B8G8R8A8, FName(*FString::Printf(TEXT("T_CityLights_%s"), *PlotId.ToString())));
	if (!LightTex)
	{
		return;
	}
	LightTex->Filter = TF_Nearest;
	LightTex->SRGB = false;
	LightTex->CompressionSettings = TC_VectorDisplacementmap;
	LightTex->NeverStream = true;
#if WITH_EDITORONLY_DATA
	LightTex->MipGenSettings = TMGS_NoMipmaps;
#endif
	if (FTexturePlatformData* PD = LightTex->GetPlatformData())
	{
		void* Data = PD->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Data, Texels.GetData(), Texels.Num());
		PD->Mips[0].BulkData.Unlock();
	}
	LightTex->UpdateResource();
	for (int32 i = 0; i < Windows->GetNumMaterials(); ++i)
	{
		UMaterialInterface* Base = Windows->GetMaterial(i);
		if (!Base)
		{
			continue;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		MID->SetTextureParameterValue(TEXT("Lights"), LightTex);
		Windows->SetMaterial(i, MID);
		WindowMID = MID;
	}
}

void ALyudCityBuilding::SetLamps(uint8 Kind, bool bOn)
{
	if (Kind > 1 || LampState[Kind] == int8(bOn))
	{
		return;
	}
	LampState[Kind] = int8(bOn);
}

void ALyudCityBuilding::SetSlot(int32 Slot, float Level)
{
	if (Slot < 0 || Slot >= LightTexSize * LightTexSize || !Texels.IsValidIndex(TexelOf(Slot) + 2))
	{
		return;
	}
	const int32 T = TexelOf(Slot);
	const uint8 V = uint8(FMath::Clamp(FMath::RoundToInt(Level * 255.f), 0, 255));
	if (Texels[T + 2] == V)
	{
		return;
	}
	Texels[T + 2] = V;
	const int32 Row = T / (LightTexSize * 4);
	DirtyMin = FMath::Min(DirtyMin, Row);
	DirtyMax = FMath::Max(DirtyMax, Row);
}

float ALyudCityBuilding::GetSlot(int32 Slot) const
{
	if (Slot < 0 || Slot >= LightTexSize * LightTexSize)
	{
		return 0.f;
	}
	const int32 T = TexelOf(Slot);
	return Texels.IsValidIndex(T + 2) ? Texels[T + 2] / 255.f : 0.f;
}

void ALyudCityBuilding::Flush()
{
	if (DirtyMax < 0 || !LightTex || !LightTex->GetResource())
	{
		return;
	}
	// one region of whole rows: rows DirtyMin..DirtyMax; the copy of the rows outlives the render command
	const int32 Rows = DirtyMax - DirtyMin + 1;
	const int32 Pitch = LightTexSize * 4;
	uint8* Copy = new uint8[Rows * Pitch];
	FMemory::Memcpy(Copy, Texels.GetData() + DirtyMin * Pitch, Rows * Pitch);
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, DirtyMin, 0, 0, LightTexSize, Rows);
	LightTex->UpdateTextureRegions(0, 1, Region, Pitch, 4, Copy,
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			delete[] SrcData;
			delete Regions;
		});
	DirtyMin = INT32_MAX;
	DirtyMax = -1;
}

// ------------------------------------------------------------------------------------------------ the lights

ULyudWindowLights* ULyudWindowLights::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULyudWindowLights>() : nullptr;
}

TStatId ULyudWindowLights::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudWindowLights, STATGROUP_Tickables);
}

void ULyudWindowLights::Register(ALyudCityBuilding* B)
{
	Buildings.AddUnique(B);
	bIndexDirty = true;
	Accum = 1e9f;              // update on the next tick
}

void ULyudWindowLights::Unregister(ALyudCityBuilding* B)
{
	Buildings.Remove(B);
	bIndexDirty = true;
}

void ULyudWindowLights::Rebuild()
{
	WindowIndex.Reset();
	FlatIndex.Reset();
	Buildings.RemoveAll([](const TWeakObjectPtr<ALyudCityBuilding>& W) { return !W.IsValid(); });
	for (const TWeakObjectPtr<ALyudCityBuilding>& W : Buildings)
	{
		ALyudCityBuilding* B = W.Get();
		const FString Plot = B->PlotId.ToString();
		for (int32 S = 0; S < B->WindowIds.Num(); ++S)
		{
			FSlotRef Ref{W, S};
			WindowIndex.Add(Plot + TEXT("/") + B->WindowIds[S], Ref);
			if (B->WindowFlats.IsValidIndex(S) && !B->WindowFlats[S].IsEmpty())
			{
				FlatIndex.FindOrAdd(Plot + TEXT("/") + B->WindowFlats[S]).Add(Ref);
			}
		}
	}
	bIndexDirty = false;
}

void ULyudWindowLights::SetDriven(bool bInDriven)
{
	if (bDriven && !bInDriven)
	{
		for (const TWeakObjectPtr<ALyudCityBuilding>& W : Buildings)
		{
			if (ALyudCityBuilding* B = W.Get())
			{
				B->ClearOwned();                           // every window back to the seeded pattern
			}
		}
	}
	bDriven = bInDriven;
	Accum = 1e9f;
}

bool ULyudWindowLights::LifeAlive() const
{
	static IConsoleVariable* People = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.People"));
	if (People && People->GetInt() == 0)
	{
		return false;
	}
	const UWorld* World = GetWorld();
	return World && World->GetSubsystem<UPeopleLifeSubsystem>() != nullptr;
}

void ULyudWindowLights::FlushPending()
{
	for (const TWeakObjectPtr<ALyudCityBuilding>& W : Pending)
	{
		if (ALyudCityBuilding* B = W.Get())
		{
			B->Flush();
		}
	}
	Pending.Reset();
}

void ULyudWindowLights::Deinitialize()
{
	// the pool's actor and its lights are transient and go with the world
	Pending.Reset();
	LampPool.Reset();
	PoolLamp.Reset();
	PoolOwner.Reset();
	Super::Deinitialize();
}

bool ULyudWindowLights::SetFlatLit(const FString& FlatId, bool bOn)
{
	if (bIndexDirty)
	{
		Rebuild();
	}
	const TArray<FSlotRef>* Slots = FlatIndex.Find(FlatId);
	if (!Slots)
	{
		return false;
	}
	for (const FSlotRef& R : *Slots)
	{
		if (ALyudCityBuilding* B = R.B.Get())
		{
			// not every room of a lit flat burns: the seed leaves about a third of its windows dark
			const bool bRoom = Slots->Num() == 1 || (B->Seeds.IsValidIndex(R.Slot) && B->Seeds[R.Slot].bRoom);
			B->SetSlot(R.Slot, (bOn && bRoom) ? 1.f : 0.f);
			B->SetOwned(R.Slot, true);
			Pending.Add(R.B);                              // uploaded in a batch by Tick, not per call
		}
	}
	return true;
}

bool ULyudWindowLights::SetWindowLit(const FString& WindowId, bool bOn)
{
	if (bIndexDirty)
	{
		Rebuild();
	}
	const FSlotRef* R = WindowIndex.Find(WindowId);
	ALyudCityBuilding* B = R ? R->B.Get() : nullptr;
	if (!B)
	{
		return false;
	}
	B->SetSlot(R->Slot, bOn ? 1.f : 0.f);
	B->SetOwned(R->Slot, true);
	Pending.Add(R->B);
	return true;
}

bool ULyudWindowLights::IsWindowLit(const FString& WindowId) const
{
	const FSlotRef* R = WindowIndex.Find(WindowId);
	const ALyudCityBuilding* B = R ? R->B.Get() : nullptr;
	return B && B->GetSlot(R->Slot) > 0.f;
}

TArray<FString> ULyudWindowLights::AllFlats() const
{
	TArray<FString> Out;
	if (bIndexDirty)
	{
		const_cast<ULyudWindowLights*>(this)->Rebuild();
	}
	FlatIndex.GetKeys(Out);
	Out.Sort();
	return Out;
}

float ULyudWindowLights::SeededFlat(const ALyudCityBuilding::FWindowSeed& D, float Hour, float SunAlt)
{
	// a household's evening: home most days, lights on once it's dark, off at bedtime; a few up before dawn
	if (SunAlt >= 2.f || D.Nobody > 0.82f)               // daylight, or nobody home tonight (a dark empty flat)
	{
		return 0.f;
	}
	if (InHours(Hour, D.Home, D.Bed))
	{
		return 1.f;
	}
	return (D.Early < 0.35f && InHours(Hour, D.Up, D.Up + 1.25f)) ? 1.f : 0.f;   // early risers
}

void ULyudWindowLights::Tick(float DeltaTime)
{
	FlushAccum += DeltaTime;
	if (FlushAccum >= 0.25f && Pending.Num() > 0)
	{
		FlushAccum = 0.f;
		FlushPending();
	}
	Accum += DeltaTime;
	if (Accum >= 1.f)
	{
		Accum = 0.f;
		UpdateAll(false);
	}
	LampAccum += DeltaTime;
	if (LampAccum >= 0.5f)
	{
		LampAccum = 0.f;
		UpdateLamps();
	}
}

void ULyudWindowLights::UpdateLamps()
{
	UWorld* World = GetWorld();
	const int32 Budget = FMath::Clamp(CVarCityLampBudget.GetValueOnGameThread(), 0, 256);
	FVector Cam = FVector::ZeroVector;
	bool bCam = false;
	if (APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
	{
		if (PC->PlayerCameraManager)
		{
			Cam = PC->PlayerCameraManager->GetCameraLocation();
			bCam = true;
		}
	}
	// the lit lamps in reach, nearest first (buildings out of reach are skipped by their bounds)
	struct FCand { float D; ALyudCityBuilding* B; int32 Lamp; };   // D: distance, cm (held lamps less LampKeep)
	TArray<FCand, TInlineAllocator<128>> Cands;
	TSet<TPair<const ALyudCityBuilding*, int32>> Held;
	for (const FLampRef& R : PoolLamp)
	{
		if (R.Lamp >= 0 && R.B.IsValid())
		{
			Held.Add({R.B.Get(), R.Lamp});
		}
	}
	if (bCam && Budget > 0)
	{
		for (const TWeakObjectPtr<ALyudCityBuilding>& W : Buildings)
		{
			ALyudCityBuilding* B = W.Get();
			if (!B || B->IsHidden() || B->LampPositions.Num() == 0 || !B->Shell)
			{
				continue;
			}
			const FBoxSphereBounds& Bo = B->Shell->Bounds;
			if (FVector::DistSquared(Bo.Origin, Cam) > FMath::Square(LampReach + Bo.SphereRadius))
			{
				continue;
			}
			const FTransform& X = B->GetActorTransform();
			for (int32 i = 0; i < B->LampPositions.Num(); ++i)
			{
				if (!B->IsLampOn(i))
				{
					continue;
				}
				float D = FVector::Dist(X.TransformPosition(B->LampPositions[i]), Cam);
				if (D < LampReach)
				{
					if (Held.Contains({B, i}))
					{
						D = FMath::Max(0.f, D - LampKeep);   // hysteresis: held lamps rank as if nearer
					}
					Cands.Add({D, B, i});
				}
			}
		}
	}
	LampCandidates = Cands.Num();
	Cands.Sort([](const FCand& A, const FCand& C) { return A.D < C.D; });
	Cands.SetNum(FMath::Min(Cands.Num(), Budget), EAllowShrinking::No);

	// grow the pool lazily, up to what is needed now (never more than the budget)
	if (LampPool.Num() < Cands.Num() && World)
	{
		AActor* Owner = PoolOwner.Get();
		if (!Owner)
		{
			FActorSpawnParameters SP;
			SP.ObjectFlags |= RF_Transient;
			Owner = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SP);
			if (Owner)
			{
				USceneComponent* Root = NewObject<USceneComponent>(Owner, TEXT("Root"));
				Owner->SetRootComponent(Root);
				Root->RegisterComponent();
#if WITH_EDITOR
				Owner->SetActorLabel(TEXT("CityLampPool"));
#endif
			}
			PoolOwner = Owner;
		}
		while (Owner && LampPool.Num() < Cands.Num())
		{
			UPointLightComponent* P = NewObject<UPointLightComponent>(Owner);
			P->SetMobility(EComponentMobility::Movable);
			P->SetupAttachment(Owner->GetRootComponent());
			P->SetIntensityUnits(ELightUnits::Lumens);
			P->SetCastShadows(false);
			P->bAffectTranslucentLighting = false;
			P->MaxDrawDistance = LampReach;
			P->MaxDistanceFadeRange = LampFade;
			P->SetVisibility(false);
			P->RegisterComponent();
			LampPool.Add(P);
			PoolLamp.Add(FLampRef());
		}
	}

	// lights given a lamp last update come up to full (one intensity change each, then left alone)
	for (int32 p = 0; p < LampPool.Num(); ++p)
	{
		FLampRef& R = PoolLamp[p];
		const ALyudCityBuilding* RB = R.B.Get();
		if (R.Ramp > 0 && RB && LampPool[p])
		{
			LampPool[p]->SetIntensity(RB->LampLumens.IsValidIndex(R.Lamp) ? RB->LampLumens[R.Lamp] : 900.f);
			R.Ramp = 0;
		}
	}
	// keep the pool lights whose lamp is still chosen (no render update for them), give the free ones the rest
	TArray<bool, TInlineAllocator<128>> Taken;
	Taken.Init(false, Cands.Num());
	TArray<int32, TInlineAllocator<64>> Free;
	for (int32 p = 0; p < LampPool.Num(); ++p)
	{
		const FLampRef& R = PoolLamp[p];
		int32 Found = INDEX_NONE;
		if (R.Lamp >= 0)
		{
			const ALyudCityBuilding* RB = R.B.Get();
			for (int32 c = 0; c < Cands.Num(); ++c)
			{
				if (!Taken[c] && Cands[c].B == RB && Cands[c].Lamp == R.Lamp)
				{
					Found = c;
					break;
				}
			}
		}
		if (Found != INDEX_NONE)
		{
			Taken[Found] = true;
		}
		else
		{
			Free.Add(p);
		}
	}
	int32 f = 0;
	for (int32 c = 0; c < Cands.Num(); ++c)
	{
		if (Taken[c] || f >= Free.Num())
		{
			continue;
		}
		const int32 p = Free[f++];
		UPointLightComponent* P = LampPool[p];
		ALyudCityBuilding* B = Cands[c].B;
		const int32 i = Cands[c].Lamp;
		if (!P)
		{
			continue;
		}
		P->SetWorldLocation(B->GetActorTransform().TransformPosition(B->LampPositions[i]));
		P->SetIntensity(0.4f * (B->LampLumens.IsValidIndex(i) ? B->LampLumens[i] : 900.f));   // ramps up next update
		P->SetLightColor(B->LampColors.IsValidIndex(i) ? B->LampColors[i] : FLinearColor(1.f, 0.85f, 0.66f));
		P->SetAttenuationRadius(B->LampRadii.IsValidIndex(i) ? B->LampRadii[i] : 700.f);
		P->SetVisibility(true);
		PoolLamp[p] = FLampRef{B, i, 1};
	}
	for (; f < Free.Num(); ++f)
	{
		const int32 p = Free[f];
		if (PoolLamp[p].Lamp >= 0 && LampPool[p])
		{
			LampPool[p]->SetVisibility(false);
		}
		PoolLamp[p] = FLampRef();
	}
}

void ULyudWindowLights::UpdateAll(bool bForce)
{
	if (bIndexDirty)
	{
		Rebuild();
	}
	UWorld* World = GetWorld();
	float SunAlt = -10.f;
	FDateTime Now = UGameCalendar::NowIn(World);
	if (World)
	{
		if (!Sky.IsValid())
		{
			TActorIterator<ALyudSky> It(World);
			Sky = It ? *It : nullptr;
		}
		if (const ALyudSky* S = Sky.Get())
		{
			SunAlt = S->SunAltitude();
			Now = S->ShownTime();                          // the hour the sky shows, so lights and sun agree
		}
	}
	bNight = bNight ? SunAlt < 3.f : SunAlt < 0.5f;
	const float Hour = Now.GetHour() + Now.GetMinute() / 60.f;
	if (bDriven && !LifeAlive())
	{
		UE_LOG(LogTemp, Display, TEXT("City lights: the town life is gone, windows back to the seeded evening"));
		SetDriven(false);
	}
	const int32 Mode = CVarCityLights.GetValueOnGameThread();
	for (const TWeakObjectPtr<ALyudCityBuilding>& W : Buildings)
	{
		ALyudCityBuilding* B = W.Get();
		if (!B)
		{
			continue;
		}
		const bool bOpen = InHours(Hour, B->OpenHour, B->CloseHour);
		for (int32 S = 0; S < B->WindowIds.Num(); ++S)
		{
			const ELyudWindowKind K = B->WindowKinds.IsValidIndex(S) ? B->WindowKinds[S] : ELyudWindowKind::Flat;
			float L = 0.f;
			if (Mode == 1)
			{
				L = 1.f;
			}
			else if (Mode == 2)
			{
				L = 0.f;
			}
			else if (bDriven && B->IsOwned(S))
			{
				continue;                                    // the life sim set this window: leave it as it is
			}
			else if (K == ELyudWindowKind::Stair)
			{
				L = bNight ? 0.8f : 0.f;
			}
			else if (K == ELyudWindowKind::Public)
			{
				L = bOpen ? (bNight ? 1.f : 0.35f) : 0.f;    // by day a shop's lamps still show a little
			}
			else if (B->Seeds.IsValidIndex(S))
			{
				const ALyudCityBuilding::FWindowSeed& D = B->Seeds[S];
				L = (D.bRoom && SeededFlat(D, Hour, SunAlt) > 0.f) ? 1.f : 0.f;
			}
			B->SetSlot(S, L);
		}
		B->Flush();
		Pending.Remove(W);
		B->SetLamps(0, Mode == 2 ? false : (Mode == 1 || bOpen));
		B->SetLamps(1, Mode == 2 ? false : (Mode == 1 || bNight));
	}
	LastMode = Mode;
}

FString ULyudWindowLights::Report() const
{
	int32 PoolOn = 0;
	for (const FLampRef& R : PoolLamp)
	{
		PoolOn += R.Lamp >= 0 ? 1 : 0;
	}
	FString Out = FString::Printf(TEXT("CITY lights: %d buildings, night %d, driven %d; lamps: %d lit in reach, %d of %d pool lights on (budget %d)\n"),
		Buildings.Num(), bNight ? 1 : 0, bDriven ? 1 : 0, LampCandidates, PoolOn, LampPool.Num(), CVarCityLampBudget.GetValueOnGameThread());
	for (const TWeakObjectPtr<ALyudCityBuilding>& W : Buildings)
	{
		if (const ALyudCityBuilding* B = W.Get())
		{
			int32 Lit = 0;
			for (int32 S = 0; S < B->NumWindows(); ++S)
			{
				Lit += B->GetSlot(S) > 0.f ? 1 : 0;
			}
			Out += FString::Printf(TEXT("  %s %s: %d windows, %d lit\n"), *B->PlotId.ToString(), *B->TypeId, B->NumWindows(), Lit);
		}
	}
	return Out;
}
