// lyud.Academ 0/1: shows or hides the Academgorodok (every actor tagged "Academ", placed by
// unreal/scripts/import_academgorodok.py) so the benchmark can compare with and without it in one
// process: -LyudBenchPasses=on=lyud.Academ+1|off=lyud.Academ+0

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectGlobals.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "Components/LightComponent.h"
#include "Sky/LyudSky.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "LyudText.h"

namespace
{
	const FName AcademTag(TEXT("Academ"));

	// Academ actors that were ticking when lyud.Academ 0 hid them
	TSet<TWeakObjectPtr<AActor>> TickingBeforeHide;

	void ApplyToWorld(UWorld* World, bool bOn)
	{
		if (!World || !World->IsGameWorld())
		{
			return;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(AcademTag))
			{
				It->SetActorHiddenInGame(!bOn);
				It->SetActorEnableCollision(bOn);
				if (!bOn)
				{
					// remember who was ticking, so showing again does not wake actors that keep their tick off
					if (It->IsActorTickEnabled())
					{
						TickingBeforeHide.Add(TWeakObjectPtr<AActor>(*It));
					}
					It->SetActorTickEnabled(false);
				}
				else if (TickingBeforeHide.Remove(TWeakObjectPtr<AActor>(*It)) > 0)
				{
					It->SetActorTickEnabled(true);
				}
			}
		}
		if (bOn)
		{
			// drop entries of actors gone or of other worlds no longer loaded
			for (auto It = TickingBeforeHide.CreateIterator(); It; ++It)
			{
				if (!It->IsValid())
				{
					It.RemoveCurrent();
				}
			}
		}
	}

	bool IsOn();

	void ApplyAll()
	{
		if (!GEngine)
		{
			return;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			ApplyToWorld(Context.World(), IsOn());
		}
	}

	TAutoConsoleVariable<int32> CVarAcadem(TEXT("lyud.Academ"), 1,
		TEXT("The Academgorodok before the town: shown (1) or hidden with no collision (0)."),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable*) { ApplyAll(); }), ECVF_Default);

	bool IsOn()
	{
		return CVarAcadem.GetValueOnGameThread() != 0;
	}

	// The flats' and the landings' lamps (tag "AcademLamp", import_academgorodok.py): from dusk to dawn those
	// tagged "AcademLit" burn (about a quarter of the rooms and every landing); by day only the lamps near
	// the player (the room they walk in), so from outside the windows of empty rooms stay dark.
	const FName LampTag(TEXT("AcademLamp"));
	const FName LitTag(TEXT("AcademLit"));
	const float NearLampCm = 650.f;
	bool bNight = false;

	// Signs: a material slot named AC_Sign_<name> on an Academ mesh shows the picture
	// /Game/Academgorodok/Localized/<lang>/T_Sign_<name> of the player's language (tools/make_academ_signs.py).
	FString SignLanguage;

	void ApplySigns(UWorld* World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->ActorHasTag(AcademTag))
			{
				continue;
			}
			TInlineComponentArray<UStaticMeshComponent*> Meshes(*It);
			for (UStaticMeshComponent* C : Meshes)
			{
				const TArray<FName> Slots = C->GetMaterialSlotNames();
				for (const FName& Slot : Slots)
				{
					const FString S = Slot.ToString();
					if (!S.StartsWith(TEXT("AC_Sign_")))
					{
						continue;
					}
					UTexture2D* Tex = LyudText::LoadTexture(TEXT("Academgorodok"), TEXT("T_Sign_") + S.Mid(8));
					const int32 Index = C->GetMaterialIndex(Slot);
					UMaterialInterface* Base = C->GetMaterial(Index);
					if (!Tex || !Base)
					{
						continue;
					}
					UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Base);
					if (!MID)
					{
						MID = UMaterialInstanceDynamic::Create(Base, C);
						C->SetMaterial(Index, MID);
					}
					MID->SetTextureParameterValue(TEXT("Image"), Tex);
				}
			}
		}
	}

	// the lamps and the sky of a world, found once rather than walking every actor twice a second
	struct FLamp
	{
		TWeakObjectPtr<AActor> Actor;
		TArray<TWeakObjectPtr<ULightComponent>> Lights;
		bool bLit = false;
	};

	struct FLampCache
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<ALyudSky> Sky;
		TArray<FLamp> Lamps;
		double BuiltAt = -1.0;
		bool bStale = false;          // a level came into a world since: its lamps may be new
	};

	TArray<FLampCache> LampCaches;

	FLampCache& LampCacheFor(UWorld* World)
	{
		LampCaches.RemoveAll([](const FLampCache& C) { return !C.World.IsValid(); });
		FLampCache* Cache = LampCaches.FindByPredicate([World](const FLampCache& C) { return C.World.Get() == World; });
		if (!Cache)
		{
			Cache = &LampCaches.AddDefaulted_GetRef();
			Cache->World = World;
		}
		// found again at most once a minute, and only when a level was streamed in since or no lamp was found yet (the
		// sky: every 5 s till it is there); otherwise the lamps are the level's own and stay, so there is no walk over
		// every actor of the world each minute
		const double Now = FPlatformTime::Seconds();
		if (Cache->BuiltAt < 0.0 || (!Cache->Sky.IsValid() && Now - Cache->BuiltAt > 5.0)
			|| ((Cache->bStale || Cache->Lamps.Num() == 0) && Now - Cache->BuiltAt > 60.0))
		{
			Cache->BuiltAt = Now;
			Cache->bStale = false;
			Cache->Lamps.Reset();
			Cache->Sky.Reset();
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (!Cache->Sky.IsValid())
				{
					if (ALyudSky* Sky = Cast<ALyudSky>(*It))
					{
						Cache->Sky = Sky;
					}
				}
				if (!It->ActorHasTag(LampTag))
				{
					continue;
				}
				FLamp& Lamp = Cache->Lamps.AddDefaulted_GetRef();
				Lamp.Actor = *It;
				Lamp.bLit = It->ActorHasTag(LitTag);
				TInlineComponentArray<ULightComponent*> Lights(*It);
				for (ULightComponent* L : Lights)
				{
					Lamp.Lights.Add(L);
				}
			}
		}
		return *Cache;
	}

	bool LampTick(float)
	{
		if (!GEngine)
		{
			return true;
		}
		// hooked here, not at static initialisation: the engine's delegate must exist first
		static bool bLevelHook = false;
		if (!bLevelHook)
		{
			bLevelHook = true;
			FWorldDelegates::LevelAddedToWorld.AddLambda([](ULevel*, UWorld*)
			{
				for (FLampCache& Stale : LampCaches)
				{
					Stale.bStale = true;
				}
			});
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || !World->IsGameWorld())
			{
				continue;
			}
			if (SignLanguage != LyudText::Language())          // first tick on a map, or lyud.Language changed
			{
				SignLanguage = LyudText::Language();
				ApplySigns(World);
			}
			FLampCache& Cache = LampCacheFor(World);
			if (ALyudSky* Sky = Cache.Sky.Get())
			{
				const float Alt = Sky->SunAltitude();
				bNight = bNight ? Alt < 3.f : Alt < 0.5f;     // a little hysteresis round sunset
			}
			const APlayerController* PC = World->GetFirstPlayerController();
			const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			const FVector Eye = Pawn ? Pawn->GetActorLocation() : FVector(1e12);
			for (const FLamp& Lamp : Cache.Lamps)
			{
				const AActor* A = Lamp.Actor.Get();
				if (!A)
				{
					continue;
				}
				const bool bWant = IsOn() && ((bNight && Lamp.bLit) ||
					FVector::DistSquared(A->GetActorLocation(), Eye) < NearLampCm * NearLampCm);
				for (const TWeakObjectPtr<ULightComponent>& Weak : Lamp.Lights)
				{
					ULightComponent* L = Weak.Get();
					if (L && L->IsVisible() != bWant)
					{
						L->SetVisibility(bWant);
					}
				}
			}
		}
		return true;
	}

	// a value set before the level loads (-dpcvars) applies once the map is there
	struct FAcademOnLoad
	{
		FAcademOnLoad()
		{
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&LampTick), 0.5f);
			FCoreUObjectDelegates::PostLoadMapWithWorld.AddLambda([](UWorld* World)
			{
				SignLanguage.Reset();                    // the new map's signs get their language on the next tick
				LampCaches.Reset();                      // and its lamps are found again
				if (!IsOn())
				{
					ApplyToWorld(World, false);
				}
			});
		}
	} AcademOnLoad;
}
