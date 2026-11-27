// The town's everyday street life: the game side (Public/People/LyudStreetLife.h, docs/street_life.md).
#include "People/LyudStreetLife.h"

#include "StreetLifeModel.h"
#include "../City/CityKit.h"
#include "City/LyudCityThings.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Dev/LyudDev.h"
#include "Dom/JsonObject.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "People/PeopleLife.h"
#include "Plants/GameCalendar.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Weather/LyudWeather.h"

// (named, not anonymous: unity builds put files of this folder together)
namespace LyudStreetImpl
{
	/** Seconds between two looks at the clock and the weather. */
	constexpr float Every = 5.f;
	/** How far the street's things are drawn (cm). */
	constexpr float DrawCm = 12000.f;

	/** The city things' plots of the street (tools/town/street_life.py, extra_places.json) and the condition each needs. */
	struct FPlotWhen { const TCHAR* Plot; uint32 Bits; };
	const FPlotWhen PlotWhen[] = {
		{ TEXT("P-STREET-KVASS"), LyudStreet::Hot },
		{ TEXT("P-STREET-ICECART"), LyudStreet::Hot },
		{ TEXT("P-STREET-SUBBOTNIK"), LyudStreet::Subbotnik },
		{ TEXT("P-STREET-LEAVES"), LyudStreet::Raking | LyudStreet::LeafPiles },
		{ TEXT("P-STREET-DOMINO"), LyudStreet::Dominoes },
	};

	LyudStreet::FSky SkyOf(const FLyudWeatherState& S)
	{
		LyudStreet::FSky K;
		K.TempC = S.TempC;
		K.Rain = S.PrecipRate;
		K.Cloud = S.CloudCover;
		K.Snow = S.SnowFraction;
		return K;
	}

	/** Developer mode: one scene of the street on or off (a second condition along with it), whichever it is not now. */
	void StreetToggle(UWorld* W, const TCHAR* A, const TCHAR* B = nullptr)
	{
		if (ULyudStreetLife* S = ULyudStreetLife::Get(W))
		{
			const int32 V = S->IsOn(A) ? 0 : 1;
			S->Force(A, V);
			if (B)
			{
				S->Force(B, V);
			}
		}
	}

	bool StreetLit(UWorld* W, const TCHAR* A)
	{
		const ULyudStreetLife* S = ULyudStreetLife::Get(W);
		return S && S->IsOn(A);
	}
}

ALyudStreetThings::ALyudStreetThings()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

ULyudStreetLife* ULyudStreetLife::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudStreetLife>() : nullptr;
}

void ULyudStreetLife::Deinitialize()
{
	if (ALyudStreetThings* T = Things.Get())
	{
		T->Destroy();
	}
	Super::Deinitialize();
}

void ULyudStreetLife::Tick(float DeltaTime)
{
	Clock += DeltaTime;
	if (Clock < LyudStreetImpl::Every)
	{
		return;
	}
	Clock = 0.f;
	if (!bTriedBuild)
	{
		bTriedBuild = true;
		BuildThings();
	}
	Update();
}

void ULyudStreetLife::Update()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!bForcesInit)
	{
		FMemory::Memset(Forced, -1, sizeof(Forced));
		bForcesInit = true;
	}
	const FDateTime T = UGameCalendar::NowIn(World);
	const ULyudWeatherSubsystem* Wx = ULyudWeatherSubsystem::Get(World);
	const int32 Key = T.GetYear() * 10000 + T.GetMonth() * 100 + T.GetDay();
	const int32 Weekday = (static_cast<int32>(T.GetDayOfWeek()) + 7) % 7;      // EDayOfWeek: Monday = 0
	if (Key != PlanKey)
	{
		// the day's plan from the forecast, once a day (a handful of the weather's own forecasts)
		PlanKey = Key;
		const int32 Y = T.GetYear(), M = T.GetMonth(), D = T.GetDay();
		const LyudStreet::FDayPlan P = LyudStreet::PlanDay(Y, M, D, Weekday, [Wx, Y, M, D](float H) -> LyudStreet::FSky
		{
			if (!Wx)
			{
				return LyudStreet::FSky();
			}
			const int32 Hh = FMath::Clamp(FMath::FloorToInt(H), 0, 23);
			const int32 Mm = FMath::Clamp(FMath::RoundToInt((H - Hh) * 60.f), 0, 59);
			return LyudStreetImpl::SkyOf(Wx->GetStateAt(FDateTime(Y, M, D, Hh, Mm)));
		});
		bHotDay = P.bHotDay;
		bHang = P.bHang;
		TakeAt = P.TakeAt;
		HotMax = P.HotMax;
	}
	LyudStreet::FNow N;
	N.Year = T.GetYear();
	N.Month = T.GetMonth();
	N.Day = T.GetDay();
	N.Weekday = Weekday;
	N.Hour = T.GetHour() + T.GetMinute() / 60.f;
	if (Wx)
	{
		const FLyudSkyWeather Sky = Wx->GetSky();
		N.Sky.TempC = Sky.TempC;
		N.Sky.Rain = Sky.Rain;
		N.Sky.Cloud = Sky.Cloud;
		N.Sky.Snow = Sky.Snow;
	}
	LyudStreet::FDayPlan P;
	P.Key = PlanKey;
	P.bHotDay = bHotDay;
	P.bHang = bHang;
	P.TakeAt = TakeAt;
	uint32 Bits = LyudStreet::Conditions(N, P);
	for (int32 I = 0; I < LyudStreet::NumConds; I++)
	{
		if (Forced[I] >= 0)
		{
			Bits = Forced[I] ? (Bits | (1u << I)) : (Bits & ~(1u << I));
		}
	}
	Now = Bits;
	// the people's life: only the conditions that changed (it keeps them for a life that starts later)
	if (UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(World))
	{
		const uint32 Changed = bSentOnce ? (Bits ^ Sent) : 0xFFFFFFFFu;
		for (int32 I = 0; I < LyudStreet::NumConds; I++)
		{
			if (Changed & (1u << I))
			{
				People->SetCondition(FName(LyudStreet::CondName(I)), (Bits & (1u << I)) != 0);
			}
		}
		Sent = Bits;
		bSentOnce = true;
	}
	// the stalls' counters and the yard things the hero can use: away while their condition is off
	if (ULyudCityThings* City = World->GetSubsystem<ULyudCityThings>())
	{
		for (const LyudStreetImpl::FPlotWhen& W : LyudStreetImpl::PlotWhen)
		{
			const bool bOut = (Bits & W.Bits) != 0;
			if (City->IsPlotHidden(W.Plot) == bOut)
			{
				City->SetPlotHidden(W.Plot, !bOut);
			}
		}
	}
	ApplyThings(Bits);
}

void ULyudStreetLife::BuildThings()
{
	UWorld* World = GetWorld();
	// the street's things stand in the town of the main level only (as the public spaces: ULyudPublicSpotsSubsystem)
	if (!World || !World->GetMapName().Contains(TEXT("Lvl_Scene01")))
	{
		return;
	}
	FString Text;
	const FString Path = FPaths::ProjectContentDir() / TEXT("City") / TEXT("street_props.json");
	TSharedPtr<FJsonObject> Doc;
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Doc) || !Doc
		|| !Doc->TryGetArrayField(TEXT("items"), Items))
	{
		UE_LOG(LogTemp, Warning, TEXT("LyudStreetLife: no %s (python3 tools/town/street_life.py)"), *Path);
		return;
	}
	FActorSpawnParameters SP;
	SP.ObjectFlags |= RF_Transient;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ALyudStreetThings* T = World->SpawnActor<ALyudStreetThings>(ALyudStreetThings::StaticClass(), FTransform(), SP);
	if (!T)
	{
		return;
	}
	Things = T;
	PropMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/City/M_CityProp.M_CityProp"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	TMap<FString, int32> SetOf;
	int32 Num = 0;
	for (const TSharedPtr<FJsonValue>& V : *Items)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		const TArray<TSharedPtr<FJsonValue>>* At = nullptr;
		if (!O || !O->TryGetArrayField(TEXT("at"), At) || At->Num() != 3)
		{
			continue;
		}
		const FString Kind = O->GetStringField(TEXT("kind"));
		const FString When = O->GetStringField(TEXT("when"));
		const int32 Variant = O->HasField(TEXT("variant")) ? (int32)O->GetNumberField(TEXT("variant")) : 0;
		const int32 Cond = When.IsEmpty() ? -1 : LyudStreet::CondIndex(TCHAR_TO_UTF8(*When));
		const FString Key = FString::Printf(TEXT("%s_%d_%d"), *Kind, Variant, Cond);
		int32* Si = SetOf.Find(Key);
		if (!Si)
		{
			CityKit::FMesh M;
			CityKit::BuildProp(M, Kind, Variant, O);
			if (M.Idx.Num() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("LyudStreetLife: no kit mesh for '%s'"), *Kind);
				SetOf.Add(Key, INDEX_NONE);
				continue;
			}
			const bool bCollide = CityKit::PropCollides(Kind);
			UStaticMesh* Mesh = M.Build(T, *(TEXT("SM_Street_") + Key.Replace(TEXT("-"), TEXT("m"))), bCollide);
			T->Meshes.Add(Mesh);
			UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(T);
			C->SetMobility(EComponentMobility::Static);
			C->SetStaticMesh(Mesh);
			if (PropMaterial)
			{
				C->SetMaterial(0, PropMaterial);
			}
			C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
			C->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
			C->SetCanEverAffectNavigation(false);
			C->SetCastShadow(true);
			C->SetCachedMaxDrawDistance(LyudStreetImpl::DrawCm);
			C->LightingChannels.bChannel1 = true;     // as the town's own props
			// small things: no distance fields, out of Lumen's scene
			C->bAffectDistanceFieldLighting = false;
			C->bAffectDynamicIndirectLighting = false;
			C->SetupAttachment(T->Root);
			C->RegisterComponent();
			T->AddInstanceComponent(C);
			Si = &SetOf.Add(Key, T->Sets.Add(C));
			SetCond.Add(Cond);
			SetCollide.Add(bCollide);
		}
		if (*Si == INDEX_NONE)
		{
			continue;
		}
		// the ground under it (the data's z is the terrace's or the yard's level): one trace each, once
		FVector P((*At)[0]->AsNumber() * 100.0, (*At)[1]->AsNumber() * 100.0, (*At)[2]->AsNumber() * 100.0);
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(StreetThings), false, T);
		if (World->LineTraceSingleByObjectType(Hit, P + FVector(0, 0, 300), P - FVector(0, 0, 500), FCollisionObjectQueryParams(ECC_WorldStatic), Q))
		{
			P.Z = Hit.ImpactPoint.Z;
		}
		T->Sets[*Si]->AddInstance(FTransform(FRotator(0.f, O->GetNumberField(TEXT("yaw")), 0.f), P), true);
		Num++;
	}
	UE_LOG(LogTemp, Display, TEXT("LyudStreetLife: %d street things in %d sets"), Num, T->Sets.Num());
}

void ULyudStreetLife::ApplyThings(uint32 Bits)
{
	ALyudStreetThings* T = Things.Get();
	if (!T || (bShownOnce && Bits == ShownBits))
	{
		return;
	}
	for (int32 I = 0; I < T->Sets.Num() && I < SetCond.Num(); I++)
	{
		const int32 Cond = SetCond[I];
		if (Cond < 0 || !T->Sets[I])
		{
			continue;
		}
		const bool bOn = (Bits & (1u << Cond)) != 0;
		const bool bWas = (ShownBits & (1u << Cond)) != 0;
		if (bShownOnce && bOn == bWas)
		{
			continue;
		}
		T->Sets[I]->SetVisibility(bOn);
		// a hidden barrel blocks nobody
		T->Sets[I]->SetCollisionEnabled(bOn && SetCollide.IsValidIndex(I) && SetCollide[I] ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	ShownBits = Bits;
	bShownOnce = true;
}

bool ULyudStreetLife::IsOn(const FString& Cond) const
{
	const int32 I = LyudStreet::CondIndex(TCHAR_TO_UTF8(*Cond));
	return I >= 0 && (Now & (1u << I)) != 0;
}

bool ULyudStreetLife::Force(const FString& Cond, int32 Value)
{
	const int32 I = LyudStreet::CondIndex(TCHAR_TO_UTF8(*Cond));
	if (I < 0)
	{
		return false;
	}
	if (!bForcesInit)
	{
		FMemory::Memset(Forced, -1, sizeof(Forced));
		bForcesInit = true;
	}
	Forced[I] = (int8)FMath::Clamp(Value, -1, 1);
	Clock = 1e6f;       // at the next tick
	return true;
}

void ULyudStreetLife::ClearForces()
{
	FMemory::Memset(Forced, -1, sizeof(Forced));
	bForcesInit = true;
	Clock = 1e6f;
}

FString ULyudStreetLife::Report() const
{
	FString Out = FString::Printf(TEXT("STREET plan %d: hot day %d (%.1f C), washing %d (in at %.2f); things %d sets"), PlanKey, bHotDay ? 1 : 0, HotMax,
		bHang ? 1 : 0, TakeAt, Things.IsValid() ? Things->Sets.Num() : 0);
	for (int32 I = 0; I < LyudStreet::NumConds; I++)
	{
		Out += FString::Printf(TEXT("\n  %s: %s%s"), UTF8_TO_TCHAR(LyudStreet::CondName(I)), (Now & (1u << I)) ? TEXT("on") : TEXT("off"),
			bForcesInit && Forced[I] >= 0 ? TEXT(" (forced)") : TEXT(""));
	}
	return Out;
}

namespace
{
	FAutoConsoleCommandWithWorldAndArgs GStreetForce(TEXT("lyud.Street.Force"),
		TEXT("lyud.Street.Force <street_hot|street_sweep|laundry_hang|laundry_out|laundry_take|yard_grannies|yard_dominoes|leaf_raking|leaf_piles|subbotnik|trunks_white> <1|0|-1>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			ULyudStreetLife* S = ULyudStreetLife::Get(W);
			if (S && Args.Num() >= 1 && !S->Force(Args[0], Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 1))
			{
				UE_LOG(LogTemp, Warning, TEXT("lyud.Street.Force: no condition '%s'"), *Args[0]);
			}
		}));
	FAutoConsoleCommandWithWorld GStreetAuto(TEXT("lyud.Street.Auto"), TEXT("lyud.Street.Auto: every street condition back to the weather and the date"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudStreetLife* S = ULyudStreetLife::Get(W)) S->ClearForces(); }));
	FAutoConsoleCommandWithWorld GStreetReport(TEXT("lyud.Street.Report"), TEXT("lyud.Street.Report: the day's plan and the street's conditions (STREET ...)"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudStreetLife* S = ULyudStreetLife::Get(W)) UE_LOG(LogTemp, Display, TEXT("%s"), *S->Report()); }));
	FAutoConsoleCommandWithWorld GStreetReplan(TEXT("lyud.Street.Replan"), TEXT("lyud.Street.Replan: plan the day again from the forecast (after moving the clock or the weather)"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudStreetLife* S = ULyudStreetLife::Get(W)) S->Replan(); }));

	// developer mode (F7 > Actions, group "street"): each a toggle of one scene of the street, lit while it is on
	LyudDev::FAutoAction GDevStreetHot(TEXT("street.hot"), TEXT("street"), TEXT("dev.action.street.hot"),
		[](UWorld* W) { LyudStreetImpl::StreetToggle(W, TEXT("street_hot")); }, [](UWorld* W) { return LyudStreetImpl::StreetLit(W, TEXT("street_hot")); });
	LyudDev::FAutoAction GDevStreetSweep(TEXT("street.sweep"), TEXT("street"), TEXT("dev.action.street.sweep"),
		[](UWorld* W) { LyudStreetImpl::StreetToggle(W, TEXT("street_sweep")); }, [](UWorld* W) { return LyudStreetImpl::StreetLit(W, TEXT("street_sweep")); });
	LyudDev::FAutoAction GDevStreetLaundry(TEXT("street.laundry"), TEXT("street"), TEXT("dev.action.street.laundry"),
		[](UWorld* W) { LyudStreetImpl::StreetToggle(W, TEXT("laundry_out"), TEXT("laundry_hang")); }, [](UWorld* W) { return LyudStreetImpl::StreetLit(W, TEXT("laundry_out")); });
	LyudDev::FAutoAction GDevStreetTake(TEXT("street.laundry_take"), TEXT("street"), TEXT("dev.action.street.laundry_take"),
		[](UWorld* W) { LyudStreetImpl::StreetToggle(W, TEXT("laundry_take")); }, [](UWorld* W) { return LyudStreetImpl::StreetLit(W, TEXT("laundry_take")); });
	LyudDev::FAutoAction GDevStreetGrannies(TEXT("street.grannies"), TEXT("street"), TEXT("dev.action.street.grannies"),
		[](UWorld* W) { LyudStreetImpl::StreetToggle(W, TEXT("yard_grannies")); }, [](UWorld* W) { return LyudStreetImpl::StreetLit(W, TEXT("yard_grannies")); });
	LyudDev::FAutoAction GDevStreetDominoes(TEXT("street.dominoes"), TEXT("street"), TEXT("dev.action.street.dominoes"),
		[](UWorld* W) { LyudStreetImpl::StreetToggle(W, TEXT("yard_dominoes")); }, [](UWorld* W) { return LyudStreetImpl::StreetLit(W, TEXT("yard_dominoes")); });
	LyudDev::FAutoAction GDevStreetLeaves(TEXT("street.leaves"), TEXT("street"), TEXT("dev.action.street.leaves"),
		[](UWorld* W) { LyudStreetImpl::StreetToggle(W, TEXT("leaf_raking"), TEXT("leaf_piles")); }, [](UWorld* W) { return LyudStreetImpl::StreetLit(W, TEXT("leaf_piles")); });
	LyudDev::FAutoAction GDevStreetSubbotnik(TEXT("street.subbotnik"), TEXT("street"), TEXT("dev.action.street.subbotnik"),
		[](UWorld* W) { LyudStreetImpl::StreetToggle(W, TEXT("subbotnik"), TEXT("trunks_white")); }, [](UWorld* W) { return LyudStreetImpl::StreetLit(W, TEXT("subbotnik")); });
	LyudDev::FAutoAction GDevStreetAuto(TEXT("street.auto"), TEXT("street"), TEXT("dev.action.street.auto"),
		[](UWorld* W) { if (ULyudStreetLife* S = ULyudStreetLife::Get(W)) S->ClearForces(); });
	LyudDev::FAutoAction GDevStreetReport(TEXT("street.report"), TEXT("street"), TEXT("dev.action.street.report"),
		[](UWorld* W) { if (ULyudStreetLife* S = ULyudStreetLife::Get(W)) UE_LOG(LogTemp, Display, TEXT("%s"), *S->Report()); });
}
