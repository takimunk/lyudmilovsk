#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudCruiseScenery.generated.h"

class ABirdActor;
class ALyudCruiseSeascape;
class ALyudMotorShip;
class ALyudSea;
class ALyudSky;
class UAudioComponent;
class ULyudCruise;
class ULyudShipWake;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;
class USoundAttenuation;
class USoundBase;
struct FCityActInfo;
struct FLyudCruiseView;

/** The view's slow sway aboard the standing ship (ULyudCruiseScenery sets the angles; LyudShipSea::Sway, lyud.Cruise.Sway). */
UCLASS()
class LYUDMILOVSK_API ULyudShipSwayCamera : public UCameraModifier
{
	GENERATED_BODY()

public:
	float RollDeg = 0.f;
	float PitchDeg = 0.f;
	virtual bool ModifyCamera(float DeltaTime, struct FMinimalViewInfo& InOutPOV) override;
};

/**
 * Everything that makes the standing ship at the cruise stage feel under way (docs/ship.md), and the ship the town
 * sees. It reads only ULyudCruise's public face (View, StageTransform, RoadsTransform, GetShipPoint, IsEventOn,
 * OnCruiseEvent) and owns:
 *   - ALyudMotorShip: the visual ship on the roads, sailing out and home (spawned once the cruise runs);
 *   - ALyudCruiseSeascape: the islands and the home coast moved past the stage ship (while the hero is aboard);
 *   - ULyudShipWake at the stage ship and ALyudSea::SetSurfaceDrift: foam and waves streaming aft at her speed;
 *   - the hiding of what stands still in the world and would give the trick away (the far volcanoes and the far land
 *     of the North, the volcano's plume, the sea railway's track), restored when he goes ashore;
 *   - the gulls over the stern by day while under way (the train's gulls, docs/railway.md), FeedGulls for the stern
 *     rail's act, and the seabirds wheeling at Bird Island;
 *   - the ship's sounds: the diesel's hum from the engine room, the wash at the stem, the wind in the rigging, the
 *     cabins' creak, the bell every half hour of the day's watches, the horn (three long at the departure, one at
 *     the arrival and at every island), the anchor's chain; ULyudSoundscapeSubsystem::SetOpenSea (no beach breakers).
 *   - the sea at her hull (docs/ship.md "The sea at the hull"): while the camera is within 2.5 km of the stage the tide
 *     is held at the datum (ALyudSea::SetTideHold: she is a building standing at the datum) and the sea state capped
 *     (lyud.Cruise.MaxHs); a calm patch (SetCalmFootprint) and a dry zone keep the waves out of her lowest deck; the
 *     camera sways a little with the sea (lyud.Cruise.Sway); her navigation lights at night.
 * Console: lyud.Cruise.Scenery 0|1, lyud.Cruise.SceneryReport, lyud.Cruise.FeedGulls, lyud.Cruise.Sway, lyud.Cruise.MaxHs.
 */
UCLASS()
class LYUDMILOVSK_API ULyudCruiseScenery : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudCruiseScenery* Get(const UObject* WorldContext);

	/** Food thrown from the stern (the thing P-SHIP/stern_rail, act ship_gull_feed): the gulls come down and snatch it
	 *  out of the air for about ten seconds. Only the birds: the item, the stamp and the quest event are the act's. */
	void FeedGulls(const FVector& From);
	bool AreGullsFeeding() const { return FeedLeft > 0.f; }
	int32 NumGulls() const { return Gulls.Num(); }
	int32 NumColonyBirds() const { return Colony.Num(); }

	ALyudCruiseSeascape* GetSeascape() const { return Seascape; }
	ALyudMotorShip* GetMotorShip() const { return MotorShip; }
	ULyudShipWake* GetWake() const { return Wake; }
	FString Report() const;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bBegun; }

protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

private:
	bool bBegun = false;
	bool bMade = false;
	bool bAboard = false;
	bool bHooked = false;
	UPROPERTY(Transient) TObjectPtr<ALyudCruiseSeascape> Seascape;
	UPROPERTY(Transient) TObjectPtr<ALyudMotorShip> MotorShip;
	UPROPERTY(Transient) TObjectPtr<ULyudShipWake> Wake;
	TWeakObjectPtr<ALyudSea> Sea;
	TWeakObjectPtr<ALyudSky> Sky;
	/** What was hidden when he came aboard (and was not hidden before). */
	TArray<TWeakObjectPtr<AActor>> Hidden;

	/** The look for what came into the world after he came aboard (HideWorld again), and for the birds' assets. */
	float HideClock = 0.f;
	int32 HideRound = 0;
	float BirdClock = 0.f;
	bool bAskBirds = false;

	// --- the sea at the hull, the view's sway, the navigation lights
	TWeakObjectPtr<ALyudSea> PatchSea;
	bool bSeaHeld = false;
	bool bEyeSet = false;
	FVector LastEye = FVector::ZeroVector;
	float SwayGain = 0.f;
	float NavCode = -1.f;
	UPROPERTY(Transient) TObjectPtr<ULyudShipSwayCamera> SwayCam;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> NavLamps;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> NavMids;
	/** Every frame while the cruise runs: the hold, the cap, the patch, the sway, the lights. */
	void TickSeaAtStage(const ULyudCruise& Cruise, float DeltaTime);
	/** Lets the sea go (the hold, the cap, the patch, the dry zone) and stills the view. */
	void ReleaseSea();
	void MakeNavLamps();

	void Make();
	/** Hides what is shown of the world's far, standing things (the static mesh actors are the long list: not every time). */
	void HideWorld(bool bStaticMeshes);
	void ComeAboard();
	void GoAshore();
	void TickAboard(const FLyudCruiseView& View, float DeltaTime);
	void OnCruiseEvent(FName EventId);
	bool OnCityAct(const FCityActInfo& Info, FText& Line);

	// --- sounds
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Hum;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Wash;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Rigging;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Creak;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<USoundBase>> Sounds;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> DeckAtten;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> HornAtten;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> BirdAtten;
	void MakeSounds();
	void UpdateSounds(const FLyudCruiseView& View, const FTransform& Stage, float DeltaTime);
	void StopSounds();
	void PlayAtShip(FName Sound, FName Point, const FVector& FallbackShipCm, USoundAttenuation* Atten, float Volume = 1.f);
	/** The bell: strokes left of the half hour being struck, the time to the next one, the last half hour struck. */
	int32 BellsLeft = 0;
	int32 BellsStruck = 0;
	float BellClock = 0.f;
	int64 LastHalfHour = -1;
	uint8 LastPhase = 0;
	float SmoothSpeed = 0.f;

	// --- birds
	struct FGull
	{
		TWeakObjectPtr<ABirdActor> Bird;
		FVector Vel = FVector::ZeroVector;
		FVector Offset = FVector::ZeroVector;
		float Phase = 0.f;
		float NextCall = 5.f;
		bool bFlap = false;
	};
	TArray<FGull> Gulls;
	float GullAway = 0.f;
	float FeedLeft = 0.f;
	FVector FeedFrom = FVector::ZeroVector;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<USoundBase>> GullCalls;
	void UpdateGulls(const FLyudCruiseView& View, const FTransform& Stage, float DeltaTime);
	void DropGulls();
	/** The colony at Bird Island: birds on fixed rounds in front of the cliff (placed by the island's place each frame). */
	struct FColonyBird
	{
		TWeakObjectPtr<ABirdActor> Bird;
		float Radius = 8000.f, Height = 3000.f, Along = 0.f, Speed = 1100.f, Phase = 0.f, Squash = 0.5f;
	};
	TArray<FColonyBird> Colony;
	void UpdateColony(float DeltaTime);
	void DropColony();
	bool BirdsLoaded(bool bWithCormorant) const;
};
