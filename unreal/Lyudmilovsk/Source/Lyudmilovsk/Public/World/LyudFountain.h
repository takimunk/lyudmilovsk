#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudFountain.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class ULyudFountainSynth;

/**
 * The water of the Academgorodok's fountain (docs/fountain.md): the white cast-stone two-tier fountain in the square
 * before Novikova 1 (its stone is part of the square's mesh). A jet rises from the fleur-de-lis and falls back as a
 * bell into the brim-full upper bowl, which spills all round in a veil breaking into strands; the veil lands in the
 * lower basin in a ring of foam and splashes, ripples running from it. It sounds (ULyudFountainSynth, procedural,
 * spatialised from the basin).
 *
 * It runs from 1 May until the first frost of the autumn (the weather engine's air temperature, the first hour under
 * -0.5 C since 1 September), then stands drained with the fallen leaves in it (snow lies on them in winter) until
 * May. Both come from the game calendar, so a save or a jump of the clock needs nothing of its own.
 *
 * Built in code from Content/Academgorodok/fountain.json (tools/academ_fountain.py); every moving thing moves in its
 * material, nothing is rebuilt or updated per frame. Spawned by ULyudFountainSubsystem in Lvl_Scene01, tagged "Academ".
 *
 * Console: lyud.Fountain 0 not at all; lyud.Fountain.Force -1 by the season, 0 drained, 1 running;
 *          lyud.Fountain.Debug 1 logs the state and the season's dates.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudFountain : public AActor
{
	GENERATED_BODY()

public:
	ALyudFountain();

	/** Builds the water from a layout (at, yaw, stone levels, flow_scale; null: the Academgorodok's fountain.json); false if none.
	 *  bAcadem: tagged Academ (lyud.Academ 0 hides it with the ensemble). */
	bool Build(TSharedPtr<class FJsonObject> Doc = nullptr, bool bAcadem = true);

	/** Whether the water runs now (by the season, or forced). */
	UFUNCTION(BlueprintCallable, Category = "Fountain") bool IsRunning() const { return bRunning; }
	/** The first air frost of the autumn of Year (the fountain is turned off then); FDateTime() if none by 1 December. */
	FDateTime FirstFrost(int32 Year);
	/** Whether the season has the fountain running at When (1 May .. the first frost). */
	bool InSeason(const FDateTime& When, bool& bKnown);
	/** The fountain's centre at its foot, world cm. */
	FVector GetFoot() const { return Foot; }
	/** One line for logs and tests. */
	UFUNCTION(BlueprintCallable, Category = "Fountain") FString DebugReport();
	/** The sound's loudness over its last block (0 when silent or stopped; tests). */
	UFUNCTION(BlueprintCallable, Category = "Fountain") float GetSoundLevel() const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> LowerWater;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> UpperWater;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Streams;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Drops;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Leaves;
	UPROPERTY(Transient) TObjectPtr<ULyudFountainSynth> Sound;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Mids;

	FVector Foot = FVector::ZeroVector;
	bool bRunning = false;
	bool bStateKnown = false;
	bool bSoundPlaying = false;
	float CheckTimer = 0.f;
	/** The sound's fade before it stops when the fountain is drained (s). */
	float SoundFadeLeft = 0.f;
	/** Advances the year's first-frost search (shared by all fountains) by up to Budget hours (0: to the end); true when done. */
	bool ScanFrost(int32 Year, int32 Budget);

	UStaticMeshComponent* MakePart(class UStaticMesh* Mesh, const TCHAR* Name, const TCHAR* MaterialPath, bool bShadow, float DrawCm,
		TFunctionRef<void(UMaterialInstanceDynamic*)> Setup);
	void SetRunning(bool bOn);
	void UpdateSound();
};

/** Puts the fountain's water in when the outdoor map starts (lyud.Fountain 0: not at all). */
UCLASS()
class LYUDMILOVSK_API ULyudFountainSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** The fountain of this world (null outside Lvl_Scene01). For Python: unreal.LyudFountainSubsystem. */
	UFUNCTION(BlueprintCallable, Category = "Fountain") ALyudFountain* GetFountain() const { return Fountain.Get(); }
	/** Every fountain of this world: the Academgorodok's and the town's (Sun Square). */
	UFUNCTION(BlueprintCallable, Category = "Fountain") TArray<ALyudFountain*> GetFountains() const
	{
		TArray<ALyudFountain*> Out;
		for (const TWeakObjectPtr<ALyudFountain>& F : All) if (F.IsValid()) Out.Add(F.Get());
		return Out;
	}

private:
	TWeakObjectPtr<ALyudFountain> Fountain;
	TArray<TWeakObjectPtr<ALyudFountain>> All;
};
