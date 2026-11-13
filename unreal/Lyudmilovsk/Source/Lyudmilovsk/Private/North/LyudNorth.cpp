#include "North/LyudNorth.h"

#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "North/LyudCampfire.h"
#include "North/LyudFumarole.h"
#include "North/LyudNorthFx.h"
#include "North/NorthModel.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Weather/LyudWeather.h"
#include "Weather/LyudWind.h"

namespace
{
	/** The map's frame in Unreal (docs/world_map.md, scene01_layout.json outside.portal_face_y): Y = 790 + map Y * 100. */
	constexpr double NorthFaceY = 790.0;
	/** The North starts where the old map's forest ends (tools/north.py FOREST_END), map metres. */
	constexpr double NorthStartY = 2330.0;
	/** A place counts as reached within this many cm (2D). */
	constexpr double NorthReachCm = 3000.0;
	const FName NorthTag(TEXT("North"));

	FVector NorthMapToWorld(double X, double Y, double Z) { return FVector(X * 100.0, NorthFaceY + Y * 100.0, Z * 100.0); }

	ELyudFumaroleKind NorthKindOf(const FString& S)
	{
		if (S == TEXT("steam")) return ELyudFumaroleKind::Steam;
		if (S == TEXT("mudpot")) return ELyudFumaroleKind::MudPot;
		if (S == TEXT("spring")) return ELyudFumaroleKind::Spring;
		return ELyudFumaroleKind::Vent;
	}

	TAutoConsoleVariable<float> CVarNorthRange(TEXT("lyud.North.Range"), 300.f,
		TEXT("The North's things (fumaroles, the camp fire) wake within this many metres of the player and sleep beyond (+25 m)."),
		ECVF_Scalability);

	TAutoConsoleVariable<int32> CVarNorthWakeAll(TEXT("lyud.North.WakeAll"), 0,
		TEXT("1: every fumarole and the camp fire awake whatever the distance (tests, screenshots); 0: by distance."),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* V)
		{
			if (!GEngine) return;
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				if (UWorld* W = Ctx.World(); W && W->IsGameWorld())
				{
					ULyudNorthSubsystem::SetWakeAll(W, V->GetInt() != 0);
				}
			}
		}), ECVF_Default);

	TAutoConsoleVariable<int32> CVarNorth(TEXT("lyud.North"), 1,
		TEXT("The North (tag North, unreal/scripts/import_north.py): shown (1) or hidden with no collision, everything asleep (0)."),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* V)
		{
			if (!GEngine) return;
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				if (UWorld* W = Ctx.World(); W && W->IsGameWorld())
				{
					ULyudNorthSubsystem::SetShown(W, V->GetInt() != 0);
				}
			}
		}), ECVF_Default);

	FAutoConsoleCommandWithWorldAndArgs CmdNorthGoto(TEXT("lyud.North.Goto"),
		TEXT("lyud.North.Goto <place id>: puts the player on the ground at a place of Content/North/north.json (north_camp, north_spring, ...)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1 || !ULyudNorthSubsystem::GotoPlace(World, FName(*Args[0])))
			{
				FString Ids;
				if (const ULyudNorthSubsystem* N = ULyudNorthSubsystem::Get(World))
				{
					for (const FLyudNorthPlace& P : N->GetPlaces())
					{
						Ids += TEXT(" ") + P.Id.ToString();
					}
				}
				UE_LOG(LogTemp, Warning, TEXT("NORTH goto: no place %s; places:%s"), Args.Num() ? *Args[0] : TEXT("(none)"), *Ids);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdNorthFire(TEXT("lyud.North.Fire"),
		TEXT("lyud.North.Fire -1|0|1: the camp's fire by its schedule (-1), put out (0), burning (1)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ULyudNorthSubsystem::SetFire(World, Args.Num() ? FCString::Atoi(*Args[0]) : 1);
		}));

	FAutoConsoleCommandWithWorld CmdNorthReport(TEXT("lyud.North.Report"), TEXT("lyud.North.Report: the North's state (NORTH line)"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (const ULyudNorthSubsystem* N = ULyudNorthSubsystem::Get(World))
			{
				UE_LOG(LogTemp, Display, TEXT("NORTH %s"), *N->DebugReport());
			}
		}));
}

// ---------------------------------------------------------------------------------------------------------------------
// a thing that sleeps

ALyudNorthThing::ALyudNorthThing()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	Tags.Add(NorthTag);
}

void ALyudNorthThing::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(false);
	if (ULyudNorthSubsystem* N = ULyudNorthSubsystem::Get(this))
	{
		N->Register(this);
	}
}

void ALyudNorthThing::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULyudNorthSubsystem* N = ULyudNorthSubsystem::Get(this))
	{
		N->Unregister(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ALyudNorthThing::SetAwake(bool bInAwake, bool bInNear)
{
	bAwake = bInAwake;
	bNear = bInAwake && bInNear;
	SetActorTickEnabled(bAwake);
}

// ---------------------------------------------------------------------------------------------------------------------
// the manager

ULyudNorthSubsystem* ULyudNorthSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<ULyudNorthSubsystem>() : nullptr;
}

bool ULyudNorthSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool ULyudNorthSubsystem::IsInNorth(const FVector& WorldLocation)
{
	return WorldLocation.Y >= NorthFaceY + NorthStartY * 100.0;
}

void ULyudNorthSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	LoadData();
	bShown = CVarNorth.GetValueOnGameThread() != 0;
	bWakeAll = CVarNorthWakeAll.GetValueOnGameThread() != 0;
	SpawnPlumes();
	UpdateAir();
}

void ULyudNorthSubsystem::Deinitialize()
{
	Things.Reset();
	SteamFx = nullptr;
	Plumes.Reset();
	Super::Deinitialize();
}

void ULyudNorthSubsystem::LoadData()
{
	Fumaroles.Reset();
	Places.Reset();
	FarVolcanoes.Reset();
	FString Text;
	TSharedPtr<FJsonObject> Root;
	const FString Path = FPaths::ProjectContentDir() / TEXT("North/north.json");
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("NORTH no %s: the North has no places or fumaroles"), *Path);
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Root->TryGetArrayField(TEXT("places"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			FLyudNorthPlace& P = Places.AddDefaulted_GetRef();
			P.Id = FName(*O->GetStringField(TEXT("id")));
			P.NameKey = O->GetStringField(TEXT("name"));
			P.Location = NorthMapToWorld(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")), 0.0);
			P.Yaw = (float)O->GetNumberField(TEXT("yaw"));
		}
	}
	FVector Sum = FVector::ZeroVector;
	int32 InValley = 0;
	if (Root->TryGetArrayField(TEXT("fumaroles"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			FLyudFumaroleInfo& F = Fumaroles.AddDefaulted_GetRef();
			F.Id = FName(*O->GetStringField(TEXT("id")));
			F.Kind = NorthKindOf(O->GetStringField(TEXT("kind")));
			F.Size = (float)O->GetNumberField(TEXT("size"));
			F.Location = NorthMapToWorld(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")), 0.0);
			F.HotRadius = 100.f * LyudNorthModel::HotRadiusM((int)F.Kind, F.Size);
			if (F.Kind != ELyudFumaroleKind::Spring && InValley < 26)
			{
				Sum += F.Location;
				InValley++;
			}
		}
	}
	Valley = InValley ? Sum / InValley : NorthMapToWorld(-430.0, 3490.0, 0.0);
	if (Root->TryGetArrayField(TEXT("far_volcanoes"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			FFarVolcano& F = FarVolcanoes.AddDefaulted_GetRef();
			F.Id = FName(*O->GetStringField(TEXT("id")));
			F.CraterRadius = 100.f * (float)O->GetNumberField(TEXT("crater"));
			F.Top = NorthMapToWorld(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")),
				O->GetNumberField(TEXT("base")) + O->GetNumberField(TEXT("height"))) - FVector(0.0, 0.0, F.CraterRadius * 0.25f);
			F.Plume = (float)O->GetNumberField(TEXT("plume"));
		}
	}
	bLoaded = true;
	UE_LOG(LogTemp, Display, TEXT("NORTH data: %d places, %d fumaroles, %d far volcanoes"), Places.Num(), Fumaroles.Num(), FarVolcanoes.Num());
}

void ULyudNorthSubsystem::SpawnPlumes()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (const FFarVolcano& F : FarVolcanoes)
	{
		if (F.Plume <= 0.f)
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ALyudNorthPlume* P = World->SpawnActor<ALyudNorthPlume>(F.Top, FRotator::ZeroRotator, Params))
		{
			P->Tags.Add(NorthTag);
			P->Setup(F.Top, F.CraterRadius, F.Plume);
			P->SetActorHiddenInGame(!bShown);
			Plumes.Add(P);
		}
	}
}

void ULyudNorthSubsystem::Register(ALyudNorthThing* Thing)
{
	if (!Thing)
	{
		return;
	}
	Things.AddUnique(Thing);
	if (const ALyudFumarole* F = Cast<ALyudFumarole>(Thing))
	{
		for (FLyudFumaroleInfo& Info : Fumaroles)
		{
			if (Info.Id == F->FumaroleId)
			{
				Info.Location = F->GetActorLocation();
				break;
			}
		}
	}
	// wake it at once if the player is already here (a reload at the camp)
	RangeTimer = 1.f;
}

void ULyudNorthSubsystem::Unregister(ALyudNorthThing* Thing)
{
	Things.Remove(Thing);
}

ALyudNorthSteam* ULyudNorthSubsystem::Steam()
{
	if (!SteamFx)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// at the valley, never at the origin (the club): its puffs are placed in world space anyway
		SteamFx = GetWorld()->SpawnActor<ALyudNorthSteam>(Valley + FVector(0.0, 0.0, 5000.0), FRotator::ZeroRotator, Params);
		if (SteamFx)
		{
			SteamFx->Tags.Add(NorthTag);
			SteamFx->SetAir(WindCm, Density);
		}
	}
	return SteamFx;
}

bool ULyudNorthSubsystem::PlayerLocation(FVector& Out) const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return false;
	}
	// the possessed pawn: the hero, or the bird he flies
	if (const APawn* P = PC->GetPawn())
	{
		Out = P->GetActorLocation();
		return true;
	}
	if (PC->PlayerCameraManager)
	{
		Out = PC->PlayerCameraManager->GetCameraLocation();
		return true;
	}
	return false;
}

void ULyudNorthSubsystem::Tick(float DeltaTime)
{
	if (!bLoaded)
	{
		return;
	}
	RangeTimer += DeltaTime;
	AirTimer += DeltaTime;
	if (RangeTimer >= 0.25f)
	{
		RangeTimer = 0.f;
		UpdateRanges();
	}
	// the air matters only while something steams; the far plume is content with a slow update
	if (AirTimer >= (NumAwake() > 0 ? 0.5f : 5.f))
	{
		AirTimer = 0.f;
		UpdateAir();
	}
}

void ULyudNorthSubsystem::UpdateRanges()
{
	FVector P = FVector::ZeroVector;
	const bool bHave = PlayerLocation(P);
	LyudNorthModel::FRanges R;
	R.WakeM = FMath::Max(10.f, CVarNorthRange.GetValueOnGameThread());
	for (int32 I = Things.Num() - 1; I >= 0; --I)
	{
		ALyudNorthThing* T = Things[I].Get();
		if (!T)
		{
			Things.RemoveAtSwap(I);
			continue;
		}
		const float DistM = bHave ? float(FVector::Dist(P, T->GetActorLocation()) / 100.0) : 1e9f;
		const bool bA = bShown && (bWakeAll || LyudNorthModel::NextAwake(T->IsAwake(), DistM, R));
		const bool bN = bA && bHave && LyudNorthModel::NextNear(T->IsNear(), DistM, R);
		if (bA != T->IsAwake() || bN != T->IsNear())
		{
			T->SetAwake(bA, bN);
		}
	}
	// entering and leaving (20 m of margin at the line), the places reached
	const bool bInside = bHave && (bPlayerInside ? P.Y >= NorthFaceY + (NorthStartY - 20.0) * 100.0 : IsInNorth(P));
	if (bInside != bPlayerInside)
	{
		bPlayerInside = bInside;
		UE_LOG(LogTemp, Display, TEXT("NORTH player %s the North"), bInside ? TEXT("entered") : TEXT("left"));
		OnRegionChanged.Broadcast(bInside);
	}
	if (bPlayerInside)
	{
		for (const FLyudNorthPlace& Place : Places)
		{
			if (!Reached.Contains(Place.Id) && FVector::DistSquared2D(P, Place.Location) < NorthReachCm * NorthReachCm)
			{
				Reached.Add(Place.Id);
				UE_LOG(LogTemp, Display, TEXT("NORTH reached %s"), *Place.Id.ToString());
				OnPlaceReached.Broadcast(Place.Id);
			}
		}
	}
}

void ULyudNorthSubsystem::UpdateAir()
{
	FVector Wind = FVector::ZeroVector;
	FVector High = FVector::ZeroVector;
	if (const ULyudWindSubsystem* W = ULyudWindSubsystem::Get(this))
	{
		// m/s at a vent's height (with the gust over the valley); the far plume rides the stronger wind aloft
		Wind = W->GetWindAt(Valley, 5.f).Velocity * 100.0;
		High = W->GetMeanWind() * 160.0;
	}
	float NewDensity = 1.f;
	if (const ULyudWeatherSubsystem* Wx = ULyudWeatherSubsystem::Get(this))
	{
		const FLyudWeatherState S = Wx->GetState();
		NewDensity = LyudNorthModel::SteamDensity(S.TempC, S.Humidity);
	}
	WindCm = Wind;
	Density = NewDensity;
	if (SteamFx)
	{
		SteamFx->SetAir(WindCm, Density);
	}
	for (ALyudNorthPlume* P : Plumes)
	{
		if (P)
		{
			P->SetWind(High);
		}
	}
}

int32 ULyudNorthSubsystem::NumAwake() const
{
	int32 N = 0;
	for (const TWeakObjectPtr<ALyudNorthThing>& T : Things)
	{
		N += T.IsValid() && T->IsAwake();
	}
	return N;
}

bool ULyudNorthSubsystem::FindHotGround(const FVector& WorldLocation, FLyudFumaroleInfo& Out) const
{
	if (!IsInNorth(WorldLocation))
	{
		return false;
	}
	for (const FLyudFumaroleInfo& F : Fumaroles)
	{
		if (FVector::DistSquared2D(WorldLocation, F.Location) < F.HotRadius * F.HotRadius)
		{
			Out = F;
			return true;
		}
	}
	return false;
}

bool ULyudNorthSubsystem::IsHotGround(const FVector& WorldLocation) const
{
	FLyudFumaroleInfo Unused;
	return FindHotGround(WorldLocation, Unused);
}

bool ULyudNorthSubsystem::FindPlace(FName PlaceId, FLyudNorthPlace& Out) const
{
	for (const FLyudNorthPlace& P : Places)
	{
		if (P.Id == PlaceId)
		{
			Out = P;
			return true;
		}
	}
	return false;
}

FString ULyudNorthSubsystem::DebugReport() const
{
	int32 Fum = 0, FumAwake = 0, Near = 0, Fires = 0, Lit = 0;
	for (const TWeakObjectPtr<ALyudNorthThing>& T : Things)
	{
		if (const ALyudFumarole* F = Cast<ALyudFumarole>(T.Get()))
		{
			Fum++;
			FumAwake += F->IsAwake();
		}
		if (const ALyudCampfire* C = Cast<ALyudCampfire>(T.Get()))
		{
			Fires++;
			Lit += C->IsLit();
		}
		Near += T.IsValid() && T->IsNear();
	}
	FVector P = FVector::ZeroVector;
	const bool bHave = PlayerLocation(P);
	return FString::Printf(TEXT("fumaroles %d awake %d near %d fires %d lit %d puffs %d inside %d reached %d/%d shown %d wakeall %d wind %.1f m/s density %.2f player map (%.0f, %.0f)"),
		Fum, FumAwake, Near, Fires, Lit, SteamFx ? SteamFx->Alive() : 0, bPlayerInside ? 1 : 0, Reached.Num(), Places.Num(),
		bShown ? 1 : 0, bWakeAll ? 1 : 0, WindCm.Size() / 100.0, Density,
		bHave ? P.X / 100.0 : 0.0, bHave ? (P.Y - NorthFaceY) / 100.0 : 0.0);
}

// ---------------------------------------------------------------------------------------------------------------------
// actions

bool ULyudNorthSubsystem::GotoPlace(UWorld* World, FName PlaceId)
{
	const ULyudNorthSubsystem* N = Get(World);
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	FLyudNorthPlace Place;
	if (!N || !Pawn || !N->FindPlace(PlaceId, Place))
	{
		return false;
	}
	// dropped onto whatever is there (the landscape, a deck), from well above the North's highest ground
	FCollisionQueryParams Q(SCENE_QUERY_STAT(NorthGoto), false, Pawn);
	FHitResult Hit;
	const FVector From(Place.Location.X, Place.Location.Y, 40000.0), To(Place.Location.X, Place.Location.Y, -20000.0);
	const double Ground = World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Q) ? Hit.ImpactPoint.Z : 0.0;
	const ACharacter* C = Cast<ACharacter>(Pawn);
	const float Half = C ? C->GetDefaultHalfHeight() : 96.f;
	Pawn->SetActorLocation(FVector(Place.Location.X, Place.Location.Y, Ground + Half + 2.0), false, nullptr, ETeleportType::TeleportPhysics);
	Pawn->SetActorRotation(FRotator(0.f, Place.Yaw, 0.f));
	PC->SetControlRotation(FRotator(-5.f, Place.Yaw, 0.f));
	UE_LOG(LogTemp, Display, TEXT("NORTH goto %s at %s"), *PlaceId.ToString(), *Pawn->GetActorLocation().ToCompactString());
	return true;
}

void ULyudNorthSubsystem::SetShown(UWorld* World, bool bOn)
{
	ULyudNorthSubsystem* N = Get(World);
	if (!World || !N)
	{
		return;
	}
	N->bShown = bOn;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(NorthTag))
		{
			It->SetActorHiddenInGame(!bOn);
			It->SetActorEnableCollision(bOn);
		}
	}
	N->UpdateRanges();
	UE_LOG(LogTemp, Display, TEXT("NORTH shown %d"), bOn ? 1 : 0);
}

void ULyudNorthSubsystem::SetFire(UWorld* World, int32 Mode)
{
	if (!World)
	{
		return;
	}
	int32 N = 0;
	for (TActorIterator<ALyudCampfire> It(World); It; ++It)
	{
		It->SetForced(FMath::Clamp(Mode, -1, 1));
		N++;
	}
	UE_LOG(LogTemp, Display, TEXT("NORTH fire %d on %d campfires"), Mode, N);
}

void ULyudNorthSubsystem::SetWakeAll(UWorld* World, bool bAll)
{
	if (ULyudNorthSubsystem* N = Get(World))
	{
		N->bWakeAll = bAll;
		N->UpdateRanges();
	}
}
