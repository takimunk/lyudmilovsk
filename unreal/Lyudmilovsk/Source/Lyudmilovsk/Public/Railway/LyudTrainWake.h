#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "LyudTrainWake.generated.h"

class ALyudSea;
class UInstancedStaticMeshComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;

/**
 * The water's answer to a train running through it (docs/railway.md): where the sea stands over the rails (the bank's
 * crest under the surface: ALyudSea::SampleSurface at each wheel, so the tide and the waves decide, never a fixed
 * flag) every wheel throws spray to both sides, which falls back as foam; the bank's flanks push a V of foam that
 * drifts off and fades behind the train; a hiss of water follows the front bogie. Out of the water (low tide, the
 * beach, the island) nothing.
 *
 * Particles are instanced meshes (droplets, foam patches) simulated here, a few hundred at most, only near the camera.
 */
UCLASS(ClassGroup = (Lyudmilovsk))
class LYUDMILOVSK_API ULyudTrainWake : public USceneComponent
{
	GENERATED_BODY()

public:
	ULyudTrainWake();

	/** Every frame from the train: its wheels' rail-head points (world cm), its velocity (cm/s). */
	void Feed(const TArray<FVector>& Wheels, const FVector& Velocity, float DeltaSeconds);

	/** 0..1 how much of the train ran through water in the last frame (tests). */
	UFUNCTION(BlueprintPure, Category = "Train") float GetWetness() const { return Wet; }
	UFUNCTION(BlueprintPure, Category = "Train") int32 NumDroplets() const { return Drops.Num(); }
	UFUNCTION(BlueprintPure, Category = "Train") int32 NumFoam() const { return Foam.Num(); }

	/** Beyond this distance from the camera no particles are made (cm). */
	float NearCm = 25000.f;

private:
	struct FDrop { FVector P, V; float Life, Size; };
	struct FFoam { FVector P, V; float Age, Life, Size; };
	TArray<FDrop> Drops;
	TArray<FFoam> Foam;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> DropIsm;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> FoamIsm;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Hiss;
	TWeakObjectPtr<ALyudSea> Sea;
	float Wet = 0.f;
	float EmitDebt = 0.f, FoamDebt = 0.f;
	FRandomStream Rnd;
	void EnsureParts();
};
