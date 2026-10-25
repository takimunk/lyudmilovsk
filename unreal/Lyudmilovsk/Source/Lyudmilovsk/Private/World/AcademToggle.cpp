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
				It->SetActorTickEnabled(bOn);
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

	bool LampTick(float)
	{
		if (!GEngine)
		{
			return true;
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
			TActorIterator<ALyudSky> Sky(World);
			if (Sky)
			{
				const float Alt = Sky->SunAltitude();
				bNight = bNight ? Alt < 3.f : Alt < 0.5f;     // a little hysteresis round sunset
			}
			const APlayerController* PC = World->GetFirstPlayerController();
			const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			const FVector Eye = Pawn ? Pawn->GetActorLocation() : FVector(1e12);
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (!It->ActorHasTag(LampTag))
				{
					continue;
				}
				const bool bWant = IsOn() && ((bNight && It->ActorHasTag(LitTag)) ||
					FVector::DistSquared(It->GetActorLocation(), Eye) < NearLampCm * NearLampCm);
				TInlineComponentArray<ULightComponent*> Lights(*It);
				for (ULightComponent* L : Lights)
				{
					if (L->IsVisible() != bWant)
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
				if (!IsOn())
				{
					ApplyToWorld(World, false);
				}
			});
		}
	} AcademOnLoad;
}
