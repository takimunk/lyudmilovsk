#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "North/LyudNorth.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudLighthouse.generated.h"

class AHeroCharacter;
class SLyudCityPage;
class UMaterialInstanceDynamic;
class USpotLightComponent;
class UStaticMeshComponent;

/**
 * The Mayak lighthouse's light (docs/lighthouse.md, backlog item 42): the Fresnel lens turning in the lantern (one turn in
 * 20 s, two bull's-eye panels: a white flash every 10 s), the beam as two cheap crossed-card cones (M_South_Beam, additive)
 * and ONE spot light along the brighter beam, out of Lumen (no indirect light, no shadows, no volumetric scattering). The
 * lantern's glass (the tower mesh's M_South_LanternGlass slot) glows warm and flashes when a panel faces the camera.
 *
 * The lamp burns from just before sunset to just after sunrise, and by day in thick fog; it warms up over 25 s and dies
 * down over 12 (LyudLighthouse::StepLevel). Fog dims the beam and shortens it (LyudLighthouse::BeamFor). Nothing is rebuilt
 * per frame: one scene component turns (the lens, the cards and the light ride on it), two material scalars change only
 * when they move, the card's length is its scale. By day the actor ticks once a second and draws nothing but the lens.
 *
 * Spawned by ULyudLighthouseSubsystem from Content/South/lighthouse.json (tools/lighthouse/lighthouse.py). Console:
 * lyud.Lighthouse.Force on|off|auto, lyud.Lighthouse.Vis <m>|-1 (the visibility it sees, for tests), lyud.Lighthouse.Report.
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ALyudLighthouse : public AActor
{
	GENERATED_BODY()

public:
	ALyudLighthouse();

	/** The lantern's glass of the placed tower mesh (found once, at the start). */
	void BindTower(UStaticMeshComponent* Tower);

	/** 0 out .. 1 burning full. */
	float GetLevel() const { return Level; }
	bool IsLampTime() const { return bLampTime; }
	float GetVisibilityM() const { return VisibilityM; }
	FString Report() const;

	/** -1 auto, 0 forced out, 1 forced on (console, dev). */
	static int32 Forced;
	/** >= 0: the visibility the light sees instead of the weather's (console, tests). */
	static float ForcedVisibilityM;

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	void SampleSky();
	void ShowParts(bool bShow);

	UPROPERTY() TObjectPtr<USceneComponent> Turn;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Lens;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Beam;
	UPROPERTY() TObjectPtr<USpotLightComponent> Spot;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GlassMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LensMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BeamMID;

	float Level = 0.f;
	bool bLampTime = false;
	bool bShown = false;
	float VisibilityM = 30000.f;
	float Rain = 0.f;
	double NextSample = 0.0;
	float SetGlass = -1.f, SetLens = -1.f, SetBeam = -1.f, SetLength = -1.f, SetSpot = -1.f;
	/** The sun's times today (hours, local clock) and the day they are for. */
	float Sunrise = 8.f, Sunset = 17.f;
	int32 SunDay = -1;
};

/**
 * A way up or down the lighthouse (docs/lighthouse.md "The gallery"): "up" at the tower's door (E: the door opens on the
 * stair, a short fade, he stands on the gallery by the hatch), "down" at the hatch on the gallery (the same back to the door
 * step). The tower is open 8:00..21:00; after the keeper's errand (quest lighthouse_paraffin done) at any hour. A locked door
 * says so. Prompt and E like the coast's seats (ANpcTalkDirector::UsePressed, never consumed); asleep far away.
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ALyudLighthouseStair : public ALyudNorthThing
{
	GENERATED_BODY()

public:
	ALyudLighthouseStair();

	bool bUp = true;
	/** Where his feet land and the way he then faces. */
	FVector To = FVector::ZeroVector;
	float ToYaw = 0.f;
	float Reach = 120.f;

	/** Is the tower open to the hero now. */
	bool IsOpen() const;
	/** Goes now (console, dev): false when locked (unless bForce). */
	bool Go(AHeroCharacter* Hero, bool bForce = false);

	virtual void SetAwake(bool bInAwake, bool bInNear) override;
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void SetPrompt(const TCHAR* Key);
	void Arrive();

	const TCHAR* Prompted = nullptr;
	TWeakObjectPtr<AHeroCharacter> Climber;
	double ArriveAt = 0.0;
};

/**
 * The keeper's weather log in its tin case on the weather screen's leg (docs/lighthouse.md "The log"): E opens it on the
 * city's page widget, today first (the readings at 08, 14 and 20 that are past, when the lamp was lit and put out, the
 * horn's hours, the keeper's remark), E turns back a day, seven days in all; a step away shuts it. The readings come from the
 * weather model (ULyudWeatherSubsystem::GetStateAt), the words from Content/Localization/lighthouse. Quests hear
 * `read lighthouse_log`.
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ALyudLighthouseLog : public ALyudNorthThing
{
	GENERATED_BODY()

public:
	ALyudLighthouseLog();

	float Reach = 120.f;
	/** Opens it or turns the page (console). */
	void Use();
	/** The page for a day back from today (0 today), as text: title and body (console, tests). */
	void PageText(int32 DaysBack, FText& Title, FText& Body) const;

	virtual void SetAwake(bool bInAwake, bool bInNear) override;
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void SetPrompt(const TCHAR* Key);
	void ShowPage(int32 Index);
	void ClosePage();

	TSharedPtr<SLyudCityPage> Page;
	int32 PageIndex = -1;
	const TCHAR* Prompted = nullptr;
};

/**
 * Spawns the lighthouse's light, its two ways up and down and the weather log from Content/South/lighthouse.json when the
 * level starts, where the Aton coast is placed (any ALyudSouthSpot from import_south.py), and binds the placed tower's
 * lantern glass. The soundscape asks it where the foghorn is (GetHornLocation). Console: lyud.Lighthouse.Climb up|down,
 * lyud.Lighthouse.Log [days back] (prints a page), lyud.Lighthouse.Goto base|gallery.
 */
UCLASS()
class LYUDMILOVSK_API ULyudLighthouseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudLighthouseSubsystem* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	ALyudLighthouse* GetLight() const { return Light.Get(); }
	ALyudLighthouseStair* GetStair(bool bUp) const { return (bUp ? Up : Down).Get(); }
	ALyudLighthouseLog* GetLog() const { return Log.Get(); }
	/** Where the foghorn sounds from (the lantern), false when there is no lighthouse in this level. */
	bool GetHornLocation(FVector& Out) const;

private:
	void Spawn(UWorld& World);

	TWeakObjectPtr<ALyudLighthouse> Light;
	TWeakObjectPtr<ALyudLighthouseStair> Up;
	TWeakObjectPtr<ALyudLighthouseStair> Down;
	TWeakObjectPtr<ALyudLighthouseLog> Log;
	FVector Horn = FVector::ZeroVector;
	bool bHorn = false;
};
