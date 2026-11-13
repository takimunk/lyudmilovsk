#include "World/LyudCityWindows.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Plants/GameCalendar.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sky/LyudSky.h"

static TAutoConsoleVariable<int32> CVarCityWindows(TEXT("lyud.CityWindows"), 1,
	TEXT("The town's lit windows in the terrace walls (ALyudCityWindows): 1 on, 0 not spawned. "
		 "lyud.CityWindows.All 1 lights every window (a look at them all), 0 back to normal."));
static TAutoConsoleVariable<int32> CVarCityWindowsAll(TEXT("lyud.CityWindows.All"), 0,
	TEXT("1: every town window lit (a check), 0: the schedule or the town's life decides."));

namespace
{
	// lamp colours behind the curtains: warm incandescent, a cooler LED, a TV's blue, the shops' strip lights
	struct FTint { FLinearColor Color; float Intensity; };
	const FTint Tints[] = {
		{ FLinearColor(1.f, 0.58f, 0.24f), 3.2f },
		{ FLinearColor(1.f, 0.82f, 0.6f), 2.6f },
		{ FLinearColor(0.38f, 0.5f, 1.f), 1.2f },
		{ FLinearColor(1.f, 0.92f, 0.78f), 4.5f },
		{ FLinearColor(1.f, 0.72f, 0.42f), 8.f },      // street lamps (warm LED)
	};
	constexpr int32 NumTints = UE_ARRAY_COUNT(Tints);

	uint32 HashName(const FString& S)
	{
		uint32 H = 2166136261u;
		for (TCHAR C : S)
		{
			H = (H ^ uint32(C)) * 16777619u;
		}
		return H;
	}

	float Frac(uint32 H, uint32 Salt)
	{
		uint32 X = H ^ (Salt * 0x9E3779B9u);
		X ^= X >> 16; X *= 0x7feb352du; X ^= X >> 15; X *= 0x846ca68bu; X ^= X >> 16;
		return float(X & 0xffffff) / float(0xffffff);
	}

	FVector V3(const TArray<TSharedPtr<FJsonValue>>& A) { return FVector(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber()); }
}

ALyudCityWindows::ALyudCityWindows()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

bool ALyudCityWindows::Build()
{
	FString Text;
	const FString Path = FPaths::ProjectContentDir() / TEXT("City/city_plan.json");
	TSharedPtr<FJsonObject> Doc;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Doc) || !Doc)
	{
		UE_LOG(LogTemp, Warning, TEXT("CITY windows: no %s"), *Path);
		return false;
	}
	auto AddWindows = [this](const TArray<TSharedPtr<FJsonValue>>& Owners, bool bShop)
	{
		for (const TSharedPtr<FJsonValue>& OV : Owners)
		{
			const TSharedPtr<FJsonObject> O = OV->AsObject();
			const TArray<TSharedPtr<FJsonValue>>* Ws = nullptr;
			if (!O || !O->TryGetArrayField(TEXT("windows"), Ws))
			{
				continue;
			}
			const double Yaw = O->GetNumberField(TEXT("yaw"));
			const FVector N(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)), 0.0);
			const FVector Along(-N.Y, N.X, 0.0);
			const FRotator Rot = FRotationMatrix::MakeFromZX(N, Along).Rotator();
			const uint32 FlatHash = HashName(O->GetStringField(TEXT("id")));
			for (const TSharedPtr<FJsonValue>& WV : *Ws)
			{
				const TSharedPtr<FJsonObject> W = WV->AsObject();
				const TArray<TSharedPtr<FJsonValue>>* Ue = nullptr;
				const TArray<TSharedPtr<FJsonValue>>* Size = nullptr;
				if (!W || !W->TryGetArrayField(TEXT("ue"), Ue) || Ue->Num() < 3 || !W->TryGetArrayField(TEXT("size"), Size) || Size->Num() < 2)
				{
					continue;
				}
				FWindow Win;
				Win.Id = FName(*W->GetStringField(TEXT("id")));
				Win.Hash = FlatHash;
				Win.bShop = bShop;
				Win.bUpper = W->GetIntegerField(TEXT("storey")) > 0;
				// the pane: 1 cm in front of the glass, the size of the opening less its frame
				const FVector At = V3(*Ue) + N * 1.0;
				const FVector Scale((*Size)[0]->AsNumber() - 0.08, (*Size)[1]->AsNumber() - 0.08, 1.0);
				Win.Pane = FTransform(Rot, At, Scale);
				// each flat keeps one lamp colour, now and then a window shows the TV
				const uint32 WH = HashName(Win.Id.ToString());
				Win.WinHash = WH;
				Win.Tint = bShop ? 3 : (Frac(WH, 7) < 0.15f ? 2 : (Frac(FlatHash, 3) < 0.7f ? 0 : 1));
				const int32 Index = Windows.Add(Win);
				ById.Add(Win.Id, Index);
				ByFlat.FindOrAdd(FName(*O->GetStringField(TEXT("id")))).Add(Index);
			}
		}
	};
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Doc->TryGetArrayField(TEXT("wall_dwellings"), Arr))
	{
		AddWindows(*Arr, false);
	}
	if (Doc->TryGetArrayField(TEXT("promenade_units"), Arr))
	{
		AddWindows(*Arr, true);
	}
	// the street lamps along the lanes at the foot of the walls: a glowing lens under each head, lit whenever it is dark
	if (Doc->TryGetArrayField(TEXT("lamps"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& LV : *Arr)
		{
			const TSharedPtr<FJsonObject> L = LV->AsObject();
			const TArray<TSharedPtr<FJsonValue>>* Head = nullptr;
			if (!L || !L->TryGetArrayField(TEXT("head_ue"), Head) || Head->Num() < 3)
			{
				continue;
			}
			const double Yaw = L->GetNumberField(TEXT("yaw"));
			const FVector Along(-FMath::Sin(FMath::DegreesToRadians(Yaw)), FMath::Cos(FMath::DegreesToRadians(Yaw)), 0.0);
			FWindow Lamp;
			Lamp.Id = FName(*L->GetStringField(TEXT("id")));
			Lamp.Hash = HashName(Lamp.Id.ToString());
			Lamp.bLamp = true;
			Lamp.Tint = 4;
			Lamp.Pane = FTransform(FRotationMatrix::MakeFromZX(FVector(0.0, 0.0, -1.0), Along).Rotator(),
				V3(*Head) - FVector(0.0, 0.0, 9.0), FVector(0.4, 0.55, 1.0));
			ById.Add(Lamp.Id, Windows.Add(Lamp));
		}
	}

	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Transport/Materials/M_TR_Glow.M_TR_Glow"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Base)
	{
		Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	for (int32 T = 0; T < NumTints; T++)
	{
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, *FString::Printf(TEXT("Panes%d"), T), RF_Transient | RF_DuplicateTransient);
		C->SetStaticMesh(Plane);
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		M->SetVectorParameterValue(TEXT("Color"), Tints[T].Color);
		M->SetScalarParameterValue(TEXT("Intensity"), Tints[T].Intensity);
		C->SetMaterial(0, M);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetAffectDistanceFieldLighting(false);
		C->bAffectDynamicIndirectLighting = false;
		C->bVisibleInRayTracing = false;
		C->SetMobility(EComponentMobility::Movable);
		C->SetupAttachment(RootComponent);
		C->RegisterComponent();
		Panes.Add(C);
		Mids.Add(M);
	}
	bDirty = true;
	Refresh(true);
	UE_LOG(LogTemp, Log, TEXT("CITY windows: %d windows from %s"), Windows.Num(), *Path);
	return Windows.Num() > 0;
}

bool ALyudCityWindows::ScheduleLit(const FWindow& W, const FDateTime& Now, float SunAltitude) const
{
	const float H = Now.GetHour() + Now.GetMinute() / 60.f;
	const bool bDark = SunAltitude < 2.f;
	const uint32 Day = uint32(Now.GetDayOfYear());
	if (W.bLamp)
	{
		return SunAltitude < 4.f;
	}
	if (W.bShop)
	{
		return bDark && H >= 9.f && H < 21.f + Frac(W.Hash, 11) * 1.5f;
	}
	if (!bDark)
	{
		return false;
	}
	// a flat: some are empty this evening (away, out late), the rest light up at dusk and go to bed between 21:30 and 01:30
	if (Frac(W.Hash, 100 + Day) < 0.12f)
	{
		return false;
	}
	const float Bed = 21.5f + Frac(W.Hash, 200 + Day) * 4.f;          // 21:30 .. 01:30
	const float Home = 16.5f + Frac(W.Hash, 300 + Day) * 3.f;         // 16:30 .. 19:30 (only matters once it is dark)
	const float Rise = 5.75f + Frac(W.Hash, 400 + Day) * 2.f;         // 05:45 .. 07:45
	bool bAwake;
	if (H >= 12.f)
	{
		bAwake = H >= Home && (Bed >= 24.f || H < Bed);
	}
	else
	{
		bAwake = (Bed > 24.f && H < Bed - 24.f) || (H >= Rise && H < 9.f);
	}
	if (!bAwake)
	{
		return false;
	}
	// not every room at once: the lower (kitchen) window most evenings, the upper rooms now and then
	const float P = W.bUpper ? 0.55f : 0.85f;
	return Frac(W.WinHash, Day * 7 + uint32(H)) < P;
}

void ALyudCityWindows::Refresh(bool bForceRebuild)
{
	const UWorld* World = GetWorld();
	// the sky's shown time (the calendar plus lyud.SkyHourOffset), so the windows and the sun agree
	FDateTime Now = UGameCalendar::NowIn(World);
	float SunAlt = -10.f;
	TActorIterator<ALyudSky> It(GetWorld());
	if (It)
	{
		Now = It->ShownTime();
		SunAlt = It->SunAltitude();
	}
	const bool bAll = CVarCityWindowsAll.GetValueOnGameThread() != 0;
	for (FWindow& W : Windows)
	{
		// a window the town's life has set stays as it set it; every other window (and every lamp) keeps the schedule
		const bool bLit = bAll || (W.External >= 0 ? W.External == 1 : ScheduleLit(W, Now, SunAlt));
		if (bLit != W.bLit)
		{
			W.bLit = bLit;
			W.bChanged = true;
			bDirty = true;
		}
	}
	if (bForceRebuild)
	{
		RebuildInstances();
	}
	else if (bDirty)
	{
		ApplyChanges();
	}
}

void ALyudCityWindows::ApplyChanges()
{
	// every window owns one instance for good (in its tint's mesh); unlit = scaled to nothing. Only the windows that
	// changed are touched. No MarkRenderStateDirty after: the instance update reaches the renderer by itself, and a
	// dirty render state would re-create the whole mesh's proxy on every change.
	bDirty = false;
	for (FWindow& W : Windows)
	{
		if (!W.bChanged || !Panes.IsValidIndex(W.Tint) || W.Instance < 0)
		{
			W.bChanged = false;
			continue;
		}
		W.bChanged = false;
		Panes[W.Tint]->UpdateInstanceTransform(W.Instance, W.bLit ? W.Pane : Hidden(W.Pane), true, false, true);
	}
}

FTransform ALyudCityWindows::Hidden(const FTransform& Pane)
{
	return FTransform(Pane.GetRotation(), Pane.GetLocation(), FVector(0.0001));
}

void ALyudCityWindows::RebuildInstances()
{
	// once, at the start: every window gets its instance (lit or hidden) in its tint's mesh
	bDirty = false;
	TArray<TArray<FTransform>> ByTint;
	ByTint.SetNum(Panes.Num());
	for (FWindow& W : Windows)
	{
		W.bChanged = false;
		if (Panes.IsValidIndex(W.Tint))
		{
			W.Instance = ByTint[W.Tint].Add(W.bLit ? W.Pane : Hidden(W.Pane));
		}
	}
	for (int32 T = 0; T < Panes.Num(); T++)
	{
		Panes[T]->ClearInstances();
		if (ByTint[T].Num())
		{
			Panes[T]->AddInstances(ByTint[T], false, true);
		}
	}
}

void ALyudCityWindows::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Accum += DeltaSeconds;
	if (Accum >= CheckSeconds)
	{
		Accum = 0.f;
		Refresh();
	}
}

void ALyudCityWindows::SetWindowLit(FName WindowId, bool bLit)
{
	if (const int32* I = ById.Find(WindowId))
	{
		Windows[*I].External = bLit ? 1 : 0;
		if (Windows[*I].bLit != bLit)
		{
			bDirty = true;               // applied at the next check (batched)
		}
	}
}

bool ALyudCityWindows::IsWindowLit(FName WindowId) const
{
	const int32* I = ById.Find(WindowId);
	return I && Windows[*I].bLit;
}

void ALyudCityWindows::UseSchedule()
{
	for (FWindow& W : Windows)
	{
		W.External = -1;
	}
	Refresh();
}

int32 ALyudCityWindows::NumLit() const
{
	int32 N = 0;
	for (const FWindow& W : Windows)
	{
		N += W.bLit ? 1 : 0;
	}
	return N;
}

bool ULyudCityWindowsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void ULyudCityWindowsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (CVarCityWindows.GetValueOnGameThread() == 0 || !InWorld.GetMapName().Contains(TEXT("Lvl_Scene01")))
	{
		return;
	}
	FActorSpawnParameters SP;
	SP.ObjectFlags |= RF_Transient;
	if (ALyudCityWindows* A = InWorld.SpawnActor<ALyudCityWindows>(SP))
	{
		A->Build();
		Actor = A;
	}
}

ULyudCityWindowsSubsystem* ULyudCityWindowsSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULyudCityWindowsSubsystem>() : nullptr;
}

void ULyudCityWindowsSubsystem::SetWindowLit(FName WindowId, bool bLit)
{
	if (ALyudCityWindows* A = Actor.Get())
	{
		A->SetWindowLit(WindowId, bLit);
	}
}

bool ULyudCityWindowsSubsystem::IsWindowLit(FName WindowId) const
{
	const ALyudCityWindows* A = Actor.Get();
	return A && A->IsWindowLit(WindowId);
}

void ULyudCityWindowsSubsystem::SetFlatLit(FName FlatId, bool bLit)
{
	if (ALyudCityWindows* A = Actor.Get())
	{
		A->SetFlatLit(FlatId, bLit);
	}
}

void ULyudCityWindowsSubsystem::SetDriven(bool bInDriven)
{
	if (!bInDriven)
	{
		if (ALyudCityWindows* A = Actor.Get())
		{
			A->UseSchedule();
		}
	}
}

void ALyudCityWindows::SetFlatLit(FName FlatId, bool bLit)
{
	if (const TArray<int32>* Is = ByFlat.Find(FlatId))
	{
		for (int32 I : *Is)
		{
			Windows[I].External = bLit ? 1 : 0;
		}
	}
}
