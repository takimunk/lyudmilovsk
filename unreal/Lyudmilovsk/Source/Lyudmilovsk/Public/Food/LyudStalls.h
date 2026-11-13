#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudStalls.generated.h"

class AStaticMeshActor;

/**
 * The street food stalls (Content/Food/stalls.json -> tools/people_map.py -> stalls_built.json): the Adyghe cheese tent by the
 * club's door and the boiled corn stall in the Academgorodok's square. Each is one static mesh /Game/Food/Stalls/<mesh>
 * (blender/food/build_stalls.py, the goods on display included), put on the ground at play start in worlds with the outdoors
 * (an ALyudSky); its seller sells over it while at work (PeopleData/counters.json, ULyudCounterTradeSubsystem). Spawned, not
 * saved in the level: a scene rebuild doesn't have to know them. Their signs are in the player's language.
 */
UCLASS()
class LYUDMILOVSK_API ULyudStallSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	/** Twice a second: the signs follow lyud.Language. */
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudStallSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return Stalls.Num() > 0; }

	const TArray<TWeakObjectPtr<AStaticMeshActor>>& GetStalls() const { return Stalls; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	TArray<TWeakObjectPtr<AStaticMeshActor>> Stalls;
	FString SignLanguage;
	float SignTimer = 0.f;
	/** Each FoodSign_<name> slot shows /Game/Food/Localized/<lang>/T_FoodSign_<name> (blender/food/food_atlas.py SIGNS). */
	void ApplySigns();
};
