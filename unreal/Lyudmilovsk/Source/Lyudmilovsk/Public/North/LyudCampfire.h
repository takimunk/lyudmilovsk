#pragma once

#include "CoreMinimal.h"
#include "North/LyudNorth.h"
#include "LyudCampfire.generated.h"

class ALyudSky;
class UAudioComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * The campsite's fire in its stone ring (north.json camp "camp_fire"; the ring itself is SM_North_Fire_ring): a few logs,
 * tongues of flame flickering by their own clocks (the festival bonfire's way, ALyudBonfire), a warm flickering point light
 * without shadows, embers rising, grey smoke into the shared steam pool, a crackle loop. It burns from dusk (the sun under
 * LightSunAltitude) until OutHour after midnight; the hero lights it or puts it out with E (polled, never consumed) and his
 * choice holds until the schedule itself changes. Sleeps with the rest of the North when the player is far.
 */
UCLASS()
class LYUDMILOVSK_API ALyudCampfire : public ALyudNorthThing
{
	GENERATED_BODY()

public:
	ALyudCampfire();

	/** Lit when the sun goes under this many degrees in the evening. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "North") float LightSunAltitude = 2.f;
	/** Put out at this local hour after midnight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "North") float OutHour = 1.5f;
	/** The light at full fire, candelas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "North") float LightCandelas = 900.f;

	UFUNCTION(BlueprintPure, Category = "North") bool IsLit() const { return bLit; }
	/** 0 out .. 1 burning (it takes a few seconds to catch and to die down). */
	UFUNCTION(BlueprintPure, Category = "North") float GetBurn() const { return Burn; }
	/** The hero's E: lights it when out, puts it out when lit. */
	UFUNCTION(BlueprintCallable, Category = "North") void Toggle();
	/** -1 by the schedule (and the hero), 0 forced out, 1 forced burning (lyud.North.Fire). */
	UFUNCTION(BlueprintCallable, Category = "North") void SetForced(int32 Mode);

	virtual void SetAwake(bool bInAwake, bool bInNear) override;
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FEmber
	{
		FVector Pos = FVector::ZeroVector;
		FVector Vel = FVector::ZeroVector;
		float Age = 0.f;
		float Life = -1.f;
	};

	void Build();
	bool Scheduled();
	void UpdatePrompt();
	void ShowFire(bool bOn);
	void SetPrompt(bool bOn);

	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Flames;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> EmberParts;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Logs;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FlameMat;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> EmberMat;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CharMat;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Crackle;
	TWeakObjectPtr<ALyudSky> Sky;
	TArray<FEmber> Embers;
	float Burn = 0.f;
	float Clock = 0.f;
	float ScheduleTimer = 0.f;
	float SmokeDebt = 0.f;
	uint32 Rng = 77u;
	int32 Forced = -1;
	bool bLit = false;
	bool bScheduled = false;
	bool bHeroChose = false;
	bool bHeroLit = false;
	bool bPrompt = false;
	bool bPromptLit = false;
	bool bFireShown = false;
	bool bBuilt = false;
};
