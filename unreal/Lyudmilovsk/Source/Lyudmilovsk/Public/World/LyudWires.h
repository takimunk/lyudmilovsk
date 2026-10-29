#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudWires.generated.h"

class UMaterialInterface;
class UStaticMeshComponent;

/**
 * The Academgorodok's overhead power lines (docs/bird_life.md "Wires"): 1970s concrete poles with a steel crossarm and
 * four porcelain pin insulators, aluminium conductors sagging between them, struts on the end and corner poles, drops to
 * the houses' walls. Built in code from Content/BirdsData/wires.json (tools/academ_wires.py), the same file the bird
 * life's wire perches come from, so the birds sit exactly on the drawn wires.
 *
 * Spawned by ULyudWiresSubsystem in Lvl_Scene01 (not placed in the level). Tagged "Academ": lyud.Academ 0 hides it.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudWires : public AActor
{
	GENERATED_BODY()

public:
	ALyudWires();

	/** Reads the layout and builds the meshes; false if there is no wires.json. */
	bool Build();

	int32 NumPoles = 0;
	int32 NumWires = 0;

private:
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Poles;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Lines;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UMaterialInterface>> Mats;

	UMaterialInterface* Mat(FName Name);
};

/** Puts the power lines up when the outdoor map starts (lyud.Wires 0: not at all). */
UCLASS()
class LYUDMILOVSK_API ULyudWiresSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};
