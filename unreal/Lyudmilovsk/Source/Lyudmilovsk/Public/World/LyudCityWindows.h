#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudCityWindows.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * The lit windows of the town's terrace walls (docs/city_plan.md): the flats built into the retaining walls W2..W5 and
 * the shop units in W1 have their windows listed in Content/City/city_plan.json (tools/city_plan.py); the glass itself
 * is drawn dark by the wall meshes (tools/city_mesh.py). A lit window is an emissive pane just in front of the glass,
 * one instance in one of a few instanced meshes (warm lamp, cool lamp, TV flicker colour, shop light).
 *
 * Who decides which windows burn: whoever calls ULyudCityWindowsSubsystem::SetWindowLit (the town's life: a flat
 * lights up when its people are home and awake). Until someone does, every flat keeps a schedule of its own from a
 * hash of its id: lights on from dusk until a bedtime between 21:30 and 01:30, some again from 06:00 before dawn;
 * shops lit while open (9..21) in the dark.
 *
 * Perf (docs/perf checklist): nothing per frame. The state is checked every CheckSeconds of real time and the instance
 * buffers are rebuilt only when a window changed; the panes cast no shadows, have no collision and stay out of Lumen's
 * scene and the distance fields.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudCityWindows : public AActor
{
	GENERATED_BODY()

public:
	ALyudCityWindows();

	/** Reads city_plan.json; false if it is missing. */
	bool Build();

	virtual void Tick(float DeltaSeconds) override;

	/** External control: lights or darkens one window (city_plan.json window id). Turns the built-in schedule off. */
	void SetWindowLit(FName WindowId, bool bLit);
	bool IsWindowLit(FName WindowId) const;
	/** Back to the built-in schedule for all windows. */
	void UseSchedule();

	int32 NumWindows() const { return Windows.Num(); }
	int32 NumLit() const;

	/** Re-evaluate now (tests, the console). */
	void Refresh(bool bForceRebuild = false);

	UPROPERTY(EditAnywhere, Category = "City") float CheckSeconds = 2.f;

private:
	struct FWindow
	{
		FName Id;
		FTransform Pane;          // the plane's transform (unit 1 m plane scaled to the window)
		uint32 Hash = 0;
		bool bShop = false;
		bool bLamp = false;        // a street lamp's head (city_plan.json "lamps"): lit in the dark, not the life's
		bool bUpper = false;
		bool bLit = false;
		int8 Tint = 0;            // which instanced mesh when lit
		int8 External = -1;       // -1 = schedule, 0 / 1 = set from outside
	};
	TArray<FWindow> Windows;
	TMap<FName, int32> ById;
	bool bExternal = false;
	bool bDirty = true;
	float Accum = 0.f;

	UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Panes;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Mids;

	bool ScheduleLit(const FWindow& W, const FDateTime& Now, float SunAltitude) const;
	void RebuildInstances();
};

/** Spawns the town's window lights when the outdoor map starts (lyud.CityWindows 0: not at all). */
UCLASS()
class LYUDMILOVSK_API ULyudCityWindowsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	static ULyudCityWindowsSubsystem* Get(const UObject* WorldContext);

	/** The town's life calls this: a window by its city_plan.json id ("W3-D017-w2", "W1-U04-w1"). */
	UFUNCTION(BlueprintCallable, Category = "City") void SetWindowLit(FName WindowId, bool bLit);
	UFUNCTION(BlueprintPure, Category = "City") bool IsWindowLit(FName WindowId) const;
	/** Every window of a flat ("W3-D017") at once. */
	UFUNCTION(BlueprintCallable, Category = "City") void SetFlatLit(FName FlatId, bool bLit);

	ALyudCityWindows* GetActor() const { return Actor.Get(); }

private:
	TWeakObjectPtr<ALyudCityWindows> Actor;
};
