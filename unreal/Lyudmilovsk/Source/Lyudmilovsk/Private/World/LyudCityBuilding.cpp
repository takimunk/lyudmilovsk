#include "World/LyudCityBuilding.h"

#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/Engine.h"
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
		From = FMath::Fmod(From + 48.f, 24.f);
		To = FMath::Fmod(To + 48.f, 24.f);
		return From <= To ? (Hour >= From && Hour < To) : (Hour >= From || Hour < To);
	}
}

// ------------------------------------------------------------------------------------------------ the building

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
	Tags.Add(CityTag);
}

void ALyudCityBuilding::BeginPlay()
{
	Super::BeginPlay();
	MakeLightTexture();
	const int32 NumLamps = FMath::Min(LampPositions.Num(), 96);
	for (int32 i = 0; i < NumLamps; ++i)
	{
		UPointLightComponent* P = NewObject<UPointLightComponent>(this);
		P->SetMobility(EComponentMobility::Movable);
		P->SetupAttachment(Shell);
		P->SetRelativeLocation(LampPositions[i]);
		P->SetIntensityUnits(ELightUnits::Lumens);
		P->SetIntensity(LampLumens.IsValidIndex(i) ? LampLumens[i] : 900.f);
		P->SetLightColor(LampColors.IsValidIndex(i) ? LampColors[i] : FLinearColor(1.f, 0.85f, 0.66f));
		P->SetAttenuationRadius(LampRadii.IsValidIndex(i) ? LampRadii[i] : 700.f);
		P->SetCastShadows(false);
		P->MaxDrawDistance = 4500.f;
		P->MaxDistanceFadeRange = 600.f;
		P->SetVisibility(false);
		P->RegisterComponent();
		LampComps.Add(P);
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
		Texels[S * 4 + 1] = uint8(FMath::Clamp(Hash01(Key, 11) * 255.f, 0.f, 255.f));
		Texels[S * 4 + 0] = uint8(FMath::Clamp(Hash01(Key, 23) * 255.f, 0.f, 255.f));
		Texels[S * 4 + 3] = 255;
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
	for (int32 i = 0; i < LampComps.Num(); ++i)
	{
		const uint8 K = LampKinds.IsValidIndex(i) ? LampKinds[i] : 0;
		if (K == Kind && LampComps[i])
		{
			LampComps[i]->SetVisibility(bOn);
		}
	}
}

void ALyudCityBuilding::SetSlot(int32 Slot, float Level)
{
	if (!Texels.IsValidIndex(Slot * 4 + 2))
	{
		return;
	}
	const uint8 V = uint8(FMath::Clamp(FMath::RoundToInt(Level * 255.f), 0, 255));
	if (Texels[Slot * 4 + 2] == V)
	{
		return;
	}
	Texels[Slot * 4 + 2] = V;
	DirtyMin = FMath::Min(DirtyMin, Slot / LightTexSize);
	DirtyMax = FMath::Max(DirtyMax, Slot / LightTexSize);
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
	bDriven = bInDriven;
	Accum = 1e9f;
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
			const bool bRoom = Hash01(B->WindowIds[R.Slot], 5) < 0.7f || Slots->Num() == 1;
			B->SetSlot(R.Slot, (bOn && bRoom) ? 1.f : 0.f);
			B->Flush();
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
	B->Flush();
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

float ULyudWindowLights::SeededFlat(const FString& FullFlat, float Hour, float SunAlt) const
{
	// a household's evening: home most days, lights on once it's dark, off at bedtime; a few up before dawn
	const bool bDark = SunAlt < 2.f;
	if (!bDark)
	{
		return 0.f;
	}
	if (Hash01(FullFlat, 1) > 0.82f)                       // nobody home tonight (or a dark empty flat)
	{
		return 0.f;
	}
	const float Bed = 21.5f + Hash01(FullFlat, 2) * 4.0f;   // 21:30 .. 01:30
	const float Home = 15.5f + Hash01(FullFlat, 3) * 4.0f;  // 15:30 .. 19:30
	if (InHours(Hour, Home, Bed))
	{
		return 1.f;
	}
	if (Hash01(FullFlat, 4) < 0.35f)                       // early risers
	{
		const float Up = 5.0f + Hash01(FullFlat, 6) * 2.5f;
		if (InHours(Hour, Up, Up + 1.25f))
		{
			return 1.f;
		}
	}
	return 0.f;
}

void ULyudWindowLights::Tick(float DeltaTime)
{
	Accum += DeltaTime;
	if (Accum < 1.f)
	{
		return;
	}
	Accum = 0.f;
	UpdateAll(false);
}

void ULyudWindowLights::UpdateAll(bool bForce)
{
	if (bIndexDirty)
	{
		Rebuild();
	}
	UWorld* World = GetWorld();
	float SunAlt = -10.f;
	if (World)
	{
		TActorIterator<ALyudSky> Sky(World);
		if (Sky)
		{
			SunAlt = Sky->SunAltitude();
		}
	}
	bNight = bNight ? SunAlt < 3.f : SunAlt < 0.5f;
	const FDateTime Now = UGameCalendar::NowIn(World);
	const float Hour = Now.GetHour() + Now.GetMinute() / 60.f;
	const int32 Mode = CVarCityLights.GetValueOnGameThread();
	for (const TWeakObjectPtr<ALyudCityBuilding>& W : Buildings)
	{
		ALyudCityBuilding* B = W.Get();
		if (!B)
		{
			continue;
		}
		const FString Plot = B->PlotId.ToString();
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
			else if (K == ELyudWindowKind::Stair)
			{
				L = bNight ? 0.8f : 0.f;
			}
			else if (K == ELyudWindowKind::Public)
			{
				L = bOpen ? (bNight ? 1.f : 0.35f) : 0.f;    // by day a shop's lamps still show a little
			}
			else
			{
				if (bDriven)
				{
					continue;                                // the life sim owns the flats: leave them as it set them
				}
				const FString Flat = B->WindowFlats.IsValidIndex(S) ? B->WindowFlats[S] : FString();
				const FString Full = Plot + TEXT("/") + Flat;
				const bool bRoom = Hash01(B->WindowIds[S], 5) < 0.7f;
				L = (SeededFlat(Full, Hour, SunAlt) > 0.f && bRoom) ? 1.f : 0.f;
			}
			B->SetSlot(S, L);
		}
		B->Flush();
		B->SetLamps(0, Mode == 2 ? false : (Mode == 1 || bOpen));
		B->SetLamps(1, Mode == 2 ? false : (Mode == 1 || bNight));
	}
	LastMode = Mode;
}

FString ULyudWindowLights::Report() const
{
	FString Out = FString::Printf(TEXT("CITY lights: %d buildings, night %d, driven %d\n"), Buildings.Num(), bNight ? 1 : 0, bDriven ? 1 : 0);
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
