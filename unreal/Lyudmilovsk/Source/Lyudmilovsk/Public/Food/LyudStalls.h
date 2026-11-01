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
 * saved in the level: a scene rebuild doesn't have to know them.
 */
UCLASS()
class LYUDMILOVSK_API ULyudStallSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	const TArray<TWeakObjectPtr<AStaticMeshActor>>& GetStalls() const { return Stalls; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	TArray<TWeakObjectPtr<AStaticMeshActor>> Stalls;
};
