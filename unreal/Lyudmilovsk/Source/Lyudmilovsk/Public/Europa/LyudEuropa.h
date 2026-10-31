#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudSave.h"
#include "LyudEuropa.generated.h"

class ACharacter;
class APlayerController;
class SLyudEuropaSubtitle;
class UAudioComponent;
class UDirectionalLightComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UPostProcessComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * The hatch to Europa (docs/europa.md). An unknown round hatch on the sea bed some 430 m off the beach leads down a
 * shaft into a small airlock under the bed: a round room with a computer, a ladder up to the sea and a second ladder up
 * a shaft that comes out on Europa's ice, under a black sky with the red star and, overhead, the planet: ocean, clouds and
 * one small island, Lyudmilovsk. Nothing else. The hero says so; the music starts.
 *
 * The meshes are blender/europa/europa_kit.py's (Content/Europa/kit.json gives the measures), placed by these actors at
 * play start from Content/Europa/europa.json; nothing is in Lvl_Scene01 itself (it is built by a script).
 */

/** The airlock's water. */
UENUM(BlueprintType)
enum class ELyudAirlockState : uint8
{
	/** Full of sea water up to the hatch: the sea hatch opens, the Europa side doesn't. */
	Flooded,
	Draining,
	/** Empty: the Europa side opens, the sea hatch doesn't. */
	Dry,
	Filling,
};

/** A round hatch lid on a hinge (0 shut .. 1 open), with its sounds. */
USTRUCT()
struct FLyudHatchLid
{
	GENERATED_BODY()

	UPROPERTY(Transient) TObjectPtr<USceneComponent> Pivot;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Mesh;
	/** Degrees the pivot pitches at full open (+ swings the lid's far edge up). */
	float OpenPitch = 110.f;
	float Openness = 0.f;
	bool bOpen = false;

	/** Moves toward bOpen; true while moving. */
	bool Update(float DeltaTime, float Seconds);
	void Snap(bool bInOpen);
	bool IsShut() const { return !bOpen && Openness <= 0.001f; }
	bool IsFullyOpen() const { return bOpen && Openness >= 0.999f; }
};

/**
 * The airlock under the sea bed with its sea hatch on the bed (the actor's origin is that hatch's lid seat). Drained and
 * filled by its computer; saved (the water's state) by actor name.
 */
UCLASS()
class LYUDMILOVSK_API ALyudAirlock : public AActor, public ILyudSaveable
{
	GENERATED_BODY()

public:
	ALyudAirlock();

	/** Seconds to drain or fill. */
	UPROPERTY(EditAnywhere, Category = "Airlock") float CycleSeconds = 14.f;
	/** Seconds a lid takes to swing. */
	UPROPERTY(EditAnywhere, Category = "Airlock") float LidSeconds = 1.6f;

	UFUNCTION(BlueprintPure, Category = "Airlock") ELyudAirlockState GetState() const { return State; }
	/** The water's surface in the room, world Z. */
	UFUNCTION(BlueprintPure, Category = "Airlock") float GetWaterZ() const;
	/** 0 dry .. 1 full. */
	UFUNCTION(BlueprintPure, Category = "Airlock") float GetWater01() const { return Water01; }
	/** A kit point (cm, the hatch frame of kit.json) in the world (tests). */
	UFUNCTION(BlueprintPure, Category = "Airlock") FVector KitToWorld(const FVector& Local) const { return Kit(Local); }

	/** The computer's button: drains a flooded lock with the sea hatch shut, fills a dry one with the inner hatch shut.
	 *  False (and why, as a string key) if it can't now. */
	bool PressButton(FString& OutRefusal);

	FLyudHatchLid SeaLid;
	FLyudHatchLid InnerLid;
	/** The planet has been seen from Europa (the line is said once). */
	UPROPERTY(SaveGame) bool bPlanetSeen = false;
	void SetSeaLidOpen(bool bOpen);
	void SetInnerLidOpen(bool bOpen, bool bSnap = false);

	/** Positions in the kit's frame (cm, kit.json) to world. */
	FVector Kit(const FVector& Local) const { return GetActorTransform().TransformPosition(Local); }
	/** Is a world point inside the room or a shaft (under the bed)? */
	bool IsInside(const FVector& P) const;
	/** The console's screen, world. */
	FVector ScreenLocation() const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnGameLoaded_Implementation() override;

	UPROPERTY(VisibleAnywhere, Category = "Airlock") TObjectPtr<UStaticMeshComponent> Collar;
	UPROPERTY(VisibleAnywhere, Category = "Airlock") TObjectPtr<UStaticMeshComponent> Room;
	UPROPERTY(VisibleAnywhere, Category = "Airlock") TObjectPtr<UStaticMeshComponent> Console;
	UPROPERTY(VisibleAnywhere, Category = "Airlock") TObjectPtr<UStaticMeshComponent> Water;
	UPROPERTY(VisibleAnywhere, Category = "Airlock") TObjectPtr<UPostProcessComponent> Underwater;

private:
	UPROPERTY(SaveGame) ELyudAirlockState State = ELyudAirlockState::Flooded;
	UPROPERTY(SaveGame) float Water01 = 1.f;

	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Lights;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ScreenMID;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> CycleLoop;
	float ShownLevel = -1.f;
	int32 ShownMode = -1;
	void Build();
	void UpdateDryZone();
	void UpdateLook();
	void Sound(FName Pool, const FVector& Where, float Volume = 1.f) const;
};

/**
 * Europa: a disc of ice 4 km round the far hatch, curving away with the moon (ALyudEuropaSite's origin is that hatch's
 * lid seat), blocks of ice, the hatch and its shaft, and the planet over it. Far from the map (europa.json), hidden
 * unless the player is there.
 */
UCLASS()
class LYUDMILOVSK_API ALyudEuropaSite : public AActor
{
	GENERATED_BODY()

public:
	ALyudEuropaSite();

	FLyudHatchLid Lid;

	/** Unit vector to the planet's centre and its angular diameter, degrees. The planet stands PlanetElevationDeg up,
	 *  PlanetSunOffsetDeg round from straight away from the star (it follows the star slowly), so its face is lit. */
	FVector PlanetDir = FVector(0.3, 0.45, 0.84).GetSafeNormal();
	float PlanetElevationDeg = 35.f;
	float PlanetSunOffsetDeg = 15.f;
	float PlanetDistance = 6.0e6f;
	float PlanetDiameterDeg = 26.f;

	void Place(const FVector& InPlanetDir, float DistanceCm, float DiameterDeg);
	/** Shows or hides it all (it is drawn only while the player is on Europa). */
	void SetShown(bool bShown);
	bool IsShown() const { return bShown; }
	FVector Kit(const FVector& Local) const { return GetActorTransform().TransformPosition(Local); }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Europa") TObjectPtr<UStaticMeshComponent> Ground;
	UPROPERTY(VisibleAnywhere, Category = "Europa") TObjectPtr<UStaticMeshComponent> Blocks;
	UPROPERTY(VisibleAnywhere, Category = "Europa") TObjectPtr<UStaticMeshComponent> Collar;
	UPROPERTY(VisibleAnywhere, Category = "Europa") TObjectPtr<UStaticMeshComponent> Shaft;
	UPROPERTY(VisibleAnywhere, Category = "Europa") TObjectPtr<UStaticMeshComponent> Planet;
	/** The planet's light on the ice (a faint blue fill from overhead; channel 2: Europa only). */
	UPROPERTY(VisibleAnywhere, Category = "Europa") TObjectPtr<UDirectionalLightComponent> PlanetShine;
	UPROPERTY(VisibleAnywhere, Category = "Europa") TObjectPtr<UPointLightComponent> ShaftLight;

private:
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PlanetMID;
	bool bShown = true;
	float SunTimer = 0.f;
};

/**
 * Runs the hatch to Europa: spawns the airlock and the site at play start (Lvl_Scene01 with its sea and sky; lyud.Europa 0
 * leaves them out), lets E work the lids, ladders and the computer (polled, never consumed; one prompt at a time), climbs
 * the ladders as scripted moves (the sea and swimming let go of the body meanwhile; a jump between the two identical shafts
 * halfway up), and on Europa: the sky's space view, the soundscape's vacuum, 0.13 g, own breathing and steps, a fence at
 * 3.5 km, and the first look at the planet (the line line.hero.europa_planet.1, its subtitle, the music MU_europa_*).
 *
 * Console: lyud.Europa.GoHatch (by the sea hatch), lyud.Europa.GoRoom, lyud.Europa.GoEuropa, lyud.Europa.Use (= E),
 * lyud.Europa.Report; lyud.Europa.Fast 1 runs the climbs and the water five times faster (tests).
 */
UCLASS()
class LYUDMILOVSK_API ULyudEuropaSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudEuropaSubsystem* Get(const UObject* WorldContext);
	/** For Python: unreal.LyudEuropaSubsystem.get_europa(world). */
	UFUNCTION(BlueprintPure, Category = "Europa", meta = (WorldContext = "WorldContext"))
	static ULyudEuropaSubsystem* GetEuropa(const UObject* WorldContext) { return Get(WorldContext); }

	UFUNCTION(BlueprintPure, Category = "Europa") ALyudAirlock* GetAirlock() const { return Airlock; }
	UFUNCTION(BlueprintPure, Category = "Europa") ALyudEuropaSite* GetSite() const { return Site; }
	/** The player stands (or climbs) on Europa. */
	UFUNCTION(BlueprintPure, Category = "Europa") bool IsOnEuropa() const { return bOnEuropa; }
	/** A ladder climb is running. */
	UFUNCTION(BlueprintPure, Category = "Europa") bool IsClimbing() const { return Climb.Num() > 0; }
	/** The planet has been seen (the line said; saved). */
	UFUNCTION(BlueprintPure, Category = "Europa") bool HasSeenPlanet() const;
	/** What E would do now (open_sea, enter_sea, climb_sea, climb_europa, console, open_europa, enter_europa; empty: nothing). */
	UFUNCTION(BlueprintPure, Category = "Europa") FString CurrentAction() const { return Action; }
	/** The planet's direction from Europa's hatch (unit). */
	UFUNCTION(BlueprintPure, Category = "Europa") FVector PlanetDirection() const;
	/** The Europa music is playing. */
	UFUNCTION(BlueprintPure, Category = "Europa") bool IsMusicPlaying() const;

	/** E: do CurrentAction. */
	UFUNCTION(BlueprintCallable, Category = "Europa") void Use();
	/** Test teleports: beside the sea hatch (in the water over it), in the room by the console, on Europa by its hatch. */
	UFUNCTION(BlueprintCallable, Category = "Europa") void GoHatch();
	UFUNCTION(BlueprintCallable, Category = "Europa") void GoRoom();
	UFUNCTION(BlueprintCallable, Category = "Europa") void GoEuropa();
	UFUNCTION(BlueprintCallable, Category = "Europa") FString Report() const;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudEuropaSubsystem, STATGROUP_Tickables); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** One stretch of a scripted climb: the feet to Feet in Seconds (rung by rung if bRungs), or at once (bJump);
	 *  OnReach when there. */
	struct FKey
	{
		FVector Feet = FVector::ZeroVector;
		float Seconds = 0.f;
		bool bRungs = false;
		bool bJump = false;
		TFunction<void()> OnReach;
	};

	UPROPERTY(Transient) TObjectPtr<ALyudAirlock> Airlock;
	UPROPERTY(Transient) TObjectPtr<ALyudEuropaSite> Site;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Breath;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Music;

	bool TrySpawn();
	bool bSpawned = false;
	float SpawnWait = 0.f;
	FVector SiteOrigin = FVector::ZeroVector;
	float GravityScale = 0.13f;
	float SpaceRadius = 6.0e5f;
	float WalkRadius = 3.5e5f;

	// E and the prompt
	FString Action;
	FString ShownPrompt;
	void UpdateAction(ACharacter& Hero, APlayerController& PC);

	// the scripted climb
	TArray<FKey> Climb;
	int32 ClimbIndex = 0;
	float ClimbTime = 0.f;
	FVector ClimbFrom = FVector::ZeroVector;
	TFunction<void()> ClimbDone;
	float StepTimer = 0.f;
	void StartClimb(TArray<FKey>&& Keys, TFunction<void()> Done);
	void TickClimb(ACharacter& Hero, float DeltaTime);
	void EndClimb(ACharacter& Hero);
	FVector FeetOf(const ACharacter& Hero) const;
	void SetFeet(ACharacter& Hero, const FVector& Feet) const;
	void Rungs(TArray<FKey>& Keys, const FVector& From, const FVector& To) const;
	void ClimbDownFromSea();
	void ClimbUpToSea();
	void ClimbUpToEuropa();
	void ClimbDownFromEuropa();

	// Europa
	bool bOnEuropa = false;
	float SavedGravity = 1.f;
	float SavedAirControl = 0.05f;
	FString SavedRainFx;
	float StrideLeft = 0.5f;
	float LineIn = -1.f;
	float SubtitleFor = 0.f;
	float SubtitleAlpha = 0.f;
	TSharedPtr<SLyudEuropaSubtitle> SubtitleWidget;
	void SetOnEuropa(bool bOn);
	void TickEuropa(ACharacter& Hero, APlayerController& PC, float DeltaTime);
	void SayPlanetLine();
	void ShowSubtitle(bool bShow);
	float Speed() const;

	friend class SLyudEuropaSubtitle;
};
