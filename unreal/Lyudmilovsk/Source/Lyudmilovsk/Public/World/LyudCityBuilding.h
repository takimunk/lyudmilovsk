#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudCityBuilding.generated.h"

class UStaticMeshComponent;
class UTexture2D;
class UMaterialInstanceDynamic;

/** What a window belongs to: a flat (lit by its people), the stair (lit from dusk to dawn) or a public room
 * (lit in the building's open hours). */
UENUM(BlueprintType)
enum class ELyudWindowKind : uint8 { Flat, Stair, Public };

/**
 * One town building (docs/city_buildings.md): the shell (Nanite, exact collision), its small details and its window
 * cards, placed on a plot of the town plan by unreal/scripts/import_city.py, which also fills the window table from
 * blender/export/city/<type>.json (slot order). Each window card's UV 1 points at one texel of LightTex (64 x 64);
 * ULyudWindowLights writes a texel when that window turns on or off, nothing per frame.
 */
UCLASS()
class LYUDMILOVSK_API ALyudCityBuilding : public AActor
{
	GENERATED_BODY()

public:
	ALyudCityBuilding();

	UPROPERTY(VisibleAnywhere, Category = "City") TObjectPtr<UStaticMeshComponent> Shell;
	UPROPERTY(VisibleAnywhere, Category = "City") TObjectPtr<UStaticMeshComponent> Detail;
	UPROPERTY(VisibleAnywhere, Category = "City") TObjectPtr<UStaticMeshComponent> Windows;

	/** The town plan's plot ("P-CAPSULE") and the building type ("capsule_tower"). */
	UPROPERTY(EditAnywhere, Category = "City") FName PlotId;
	UPROPERTY(EditAnywhere, Category = "City") FString TypeId;

	/** Per light slot (index = slot): the window id without the plot ("3-02/w1", "stair/w4", "pub/w2"), its kind,
	 * and its flat ("3-02"; empty for stair and public windows). Filled by import_city.py. */
	UPROPERTY(EditAnywhere, Category = "City") TArray<FString> WindowIds;
	UPROPERTY(EditAnywhere, Category = "City") TArray<ELyudWindowKind> WindowKinds;
	UPROPERTY(EditAnywhere, Category = "City") TArray<FString> WindowFlats;

	/** Real lamps inside the enterable rooms (B.lamp in blender/city), in the actor's local space (cm); kind 0 = on in the
	 * open hours, 1 = dusk to dawn. Point lights are made at BeginPlay (no shadows, drawn near only). */
	UPROPERTY(EditAnywhere, Category = "City") TArray<FVector> LampPositions;
	UPROPERTY(EditAnywhere, Category = "City") TArray<float> LampLumens;
	UPROPERTY(EditAnywhere, Category = "City") TArray<FLinearColor> LampColors;
	UPROPERTY(EditAnywhere, Category = "City") TArray<float> LampRadii;
	UPROPERTY(EditAnywhere, Category = "City") TArray<uint8> LampKinds;
	/** Turns the lamps of one kind on or off (only touches them on a change). */
	void SetLamps(uint8 Kind, bool bOn);

	/** Public rooms are lit from OpenHour to CloseHour (local game time). */
	UPROPERTY(EditAnywhere, Category = "City") float OpenHour = 8.f;
	UPROPERTY(EditAnywhere, Category = "City") float CloseHour = 21.f;

	/** Light level of a slot, 0..1 (quantised to a byte). Marks the texture for upload. */
	void SetSlot(int32 Slot, float Level);
	float GetSlot(int32 Slot) const { return Texels.IsValidIndex(Slot * 4) ? Texels[Slot * 4 + 2] / 255.f : 0.f; }
	/** Uploads the changed texels (once, by ULyudWindowLights after a batch of changes). */
	void Flush();
	int32 NumWindows() const { return WindowIds.Num(); }

	static constexpr int32 LightTexSize = 64;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void MakeLightTexture();

	UPROPERTY(Transient) TObjectPtr<UTexture2D> LightTex;
	UPROPERTY(Transient) TArray<TObjectPtr<class UPointLightComponent>> LampComps;
	int8 LampState[2] = {-1, -1};
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> WindowMID;
	/** BGRA texels: R = light (the shader's red channel is B in memory order: we write [2]), G = warmth, B = curtain. */
	TArray<uint8> Texels;
	int32 DirtyMin = INT32_MAX;
	int32 DirtyMax = -1;
};

/**
 * The town's lit windows. Who is home is the town life's business (UPeopleLifeSubsystem): it calls SetDriven(true)
 * and then SetFlatLit / SetWindowLit with full ids ("<plot>/<flat>" or "<plot>/<flat>/w<n>") whenever a light goes on
 * or off. Without it (lyud.People 0, other maps) a seeded evening pattern runs: flats light up at dusk and go dark
 * between 21:30 and 01:30, some early risers before dawn. Stair windows burn from dusk to dawn, public rooms in their
 * open hours. Works at most once a second and touches a building's texture only when one of its windows changed.
 *
 * Console: lyud.City 0|1 (hide/show every town building), lyud.City.Lights auto|on|off, lyud.City.Report.
 */
UCLASS()
class LYUDMILOVSK_API ULyudWindowLights : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudWindowLights* Get(const UObject* WorldContext);

	/** The life simulation says it decides the flats' lights from now on (false: back to the seeded pattern). */
	UFUNCTION(BlueprintCallable, Category = "City") void SetDriven(bool bInDriven);
	/** "<plot>/<flat>": every window of that flat. Returns false if no such flat. */
	UFUNCTION(BlueprintCallable, Category = "City") bool SetFlatLit(const FString& FlatId, bool bOn);
	/** "<plot>/<flat>/w<n>" (or "<plot>/stair/w<n>", "<plot>/pub/w<n>"). */
	UFUNCTION(BlueprintCallable, Category = "City") bool SetWindowLit(const FString& WindowId, bool bOn);
	UFUNCTION(BlueprintPure, Category = "City") bool IsWindowLit(const FString& WindowId) const;
	/** Every flat id in the town ("<plot>/<flat>"), for the life simulation's homes. */
	UFUNCTION(BlueprintCallable, Category = "City") TArray<FString> AllFlats() const;
	UFUNCTION(BlueprintPure, Category = "City") bool IsNight() const { return bNight; }

	void Register(ALyudCityBuilding* B);
	void Unregister(ALyudCityBuilding* B);
	FString Report() const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return Buildings.Num() > 0; }
	virtual bool DoesSupportWorldType(const EWorldType::Type T) const override { return T == EWorldType::Game || T == EWorldType::PIE; }

private:
	struct FSlotRef { TWeakObjectPtr<ALyudCityBuilding> B; int32 Slot = 0; };
	void Rebuild();
	void UpdateAll(bool bForce);
	float SeededFlat(const FString& FullFlat, float Hour, float SunAlt) const;

	TArray<TWeakObjectPtr<ALyudCityBuilding>> Buildings;
	TMap<FString, FSlotRef> WindowIndex;               // "<plot>/<id>" -> slot
	TMap<FString, TArray<FSlotRef>> FlatIndex;          // "<plot>/<flat>" -> slots
	bool bIndexDirty = true;
	bool bDriven = false;
	bool bNight = false;
	float Accum = 1e9f;
	int32 LastMode = -1;
	float LastHourBucket = -1.f;
};
