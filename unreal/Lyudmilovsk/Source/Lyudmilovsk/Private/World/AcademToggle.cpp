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

	// a value set before the level loads (-dpcvars) applies once the map is there
	struct FAcademOnLoad
	{
		FAcademOnLoad()
		{
			FCoreUObjectDelegates::PostLoadMapWithWorld.AddLambda([](UWorld* World)
			{
				if (!IsOn())
				{
					ApplyToWorld(World, false);
				}
			});
		}
	} AcademOnLoad;
}
