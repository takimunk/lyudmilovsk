#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VinePlantings.generated.h"

class AProceduralVine;

/**
 * Plants the vines that belong to buildings when play begins: each planting names a house mesh and a
 * spot along one of its walls (in the house's own frame), so the vines follow the house wherever the
 * world build puts it and need no level edits. Every spawned vine is tagged like its house (so
 * lyud.Academ hides it with the house).
 *
 * Now: Virginia creeper at the two end bays of the courtyard (west) face of Novikova 1 in the
 * Academgorodok (the square's front is kept clear).
 */
UCLASS()
class LYUDMILOVSK_API UVinePlantings : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	UPROPERTY(Transient) TArray<TObjectPtr<AProceduralVine>> Vines;
};
