#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyudLevelCrossing.generated.h"

namespace LyudRail { class FRun; }
class FJsonObject;
class ULyudRailwaySubsystem;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UAudioComponent;
class UPointLightComponent;
class UMaterialInterface;

/**
 * The level crossing where the beach path crosses the line by the tunnel's mouth (docs/railway.md), a Japanese one:
 * a post either side with the yellow crossbuck, two red lamps that blink in turn, the bell (the "kan-kan" of every
 * anime summer), barriers that come down across the path, and a plate "Caution: train" in the player's language
 * (rail.sign.caution). It starts warning WarnSeconds before the train's front reaches it (the timetable knows when),
 * drops the barriers a few seconds later, and lifts them when the train's tail has cleared it.
 *
 * The decking between and over the rails is rubber panels; ramps lead up from the sand on both sides.
 */
UCLASS()
class LYUDMILOVSK_API ALyudLevelCrossing : public AActor
{
	GENERATED_BODY()

public:
	ALyudLevelCrossing();
	void Setup(ULyudRailwaySubsystem* InRailway, const TSharedPtr<FJsonObject>& Built);
	void UpdateFrom(const LyudRail::FRun& Run, double Now, float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = "Crossing") bool IsWarning() const { return bWarning; }
	/** 0 up .. 1 down. */
	UFUNCTION(BlueprintPure, Category = "Crossing") float GetBarrier() const { return Barrier; }
	/** Tests: warn regardless of the train (for Seconds). */
	UFUNCTION(BlueprintCallable, Category = "Crossing") void TestWarn(float Seconds) { TestUntil = Seconds; }

	/** Seconds of warning before the train's front arrives (railway.json crossing.warn_seconds). */
	float WarnSeconds = 25.f;

private:
	UMaterialInterface* Mat(FName Name);
	UPROPERTY(Transient) TObjectPtr<ULyudRailwaySubsystem> Railway;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Arms;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> LampsA;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> LampsB;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Glows;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Bell;
	UPROPERTY(Transient) TObjectPtr<class USoundAttenuation> BellAtten;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UMaterialInterface>> Mats;
	TArray<FQuat> ArmUp, ArmDown;
	double S = 0.0;              // where the crossing is along the line
	double HalfTrain = 17.0;     // the train's half length, m
	bool bWarning = false;
	float WarnTime = 0.f;
	float Barrier = 0.f;
	float TestUntil = 0.f;
};
