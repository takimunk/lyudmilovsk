// The town's everyday street life (docs/street_life.md): the game side of LyudStreet (Private/People/StreetLifeModel.h).
// Works out the street's conditions from the calendar and the weather every few seconds and hands the ones that changed
// to the people's life (UPeopleLifeSubsystem::SetCondition: the vendors' duties, the residents' kvass, the laundresses,
// the grandmothers, the dominoes, the yard keeper's leaves, the subbotnik, the promenade's summer evenings: docs/promenade.md), shows the street's things while their
// condition holds (the kvass barrel, the ice-cream cart, the washing on the lines, the leaf piles, the whitewashed trunks:
// CityKit meshes from Content/City/street_props.json, built once) and hides the city things of the stalls that are not out
// (ULyudCityThings::SetPlotHidden). Nothing per frame, nothing saved: it all follows from the date and the weather.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudStreetLife.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/** The street's things: one instanced set per kind, variant and condition, static, shown and hidden as a whole. */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudStreetThings : public AActor
{
	GENERATED_BODY()

public:
	ALyudStreetThings();

	UPROPERTY(Transient) TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Sets;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> Meshes;
};

UCLASS()
class LYUDMILOVSK_API ULyudStreetLife : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudStreetLife* Get(const UObject* WorldContext);

	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudStreetLife, STATGROUP_Tickables); }

	/** Is a condition (street_hot, laundry_out, subbotnik ...: LyudStreet::CondName) on now? */
	bool IsOn(const FString& Cond) const;
	/** Developer mode: hold a condition on (1) or off (0), or back to the weather and the date (-1). False for an unknown name. */
	bool Force(const FString& Cond, int32 Value);
	/** Every condition back to the weather and the date. */
	void ClearForces();
	/** The day's plan and the conditions (console lyud.Street.Report). */
	FString Report() const;
	/** Re-plan the day now (after the clock or the weather was moved by hand). */
	void Replan() { PlanKey = -1; Clock = 1e6f; }
	/** Developer mode: move the calendar on to the next MonthDay (same time of day), then plan that day. False for a bad date. */
	bool MoveToDate(int32 Month, int32 Day);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	void Update();
	void BuildThings();
	void ApplyThings(uint32 Bits);

	float Clock = 1e6f;
	int32 PlanKey = -1;
	bool bHotDay = false, bHang = false;
	/** The square's evenings (docs/promenade.md): tonight's dance and this Sunday's band not called off by the forecast. */
	bool bDanceEve = false, bBandDay = false;
	float TakeAt = 18.f, HotMax = 0.f;
	/** The conditions handed out last (all off before the first). */
	uint32 Sent = 0;
	bool bSentOnce = false;
	/** Developer forces: -1 none, 0 off, 1 on, by LyudStreet's index. */
	int8 Forced[16];
	bool bForcesInit = false;
	uint32 Now = 0;

	bool bTriedBuild = false;
	TWeakObjectPtr<ALyudStreetThings> Things;
	/** Each set's condition index (-1: always shown), parallel to Things->Sets. */
	TArray<int32> SetCond;
	/** Whether each set blocks the player while shown. */
	TArray<bool> SetCollide;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> PropMaterial;
	uint32 ShownBits = 0;
	bool bShownOnce = false;
};
