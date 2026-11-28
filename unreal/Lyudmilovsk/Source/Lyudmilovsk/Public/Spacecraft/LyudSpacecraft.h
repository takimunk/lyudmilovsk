#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudSave.h"
#include "LyudSpacecraft.generated.h"

class ACharacter;
class APlayerController;
class SLyudFlightSubtitle;
class UAudioComponent;
class ULyudLadderComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USoundBase;
class UStaticMeshComponent;

// The flight model is engine-free and lives in Private/Spacecraft/FlightModel.h: only its names cross this header.
namespace LyudFlight
{
	struct FState;
}

/**
 * The flight home from Europa in the descent craft «Ласточка» (docs/spacecraft.md, tools/spacecraft/ENGINE_NOTES.md).
 *
 *  - ALyudSpacecraft: the cabin (the descent capsule) and everything in it. Declared here, IMPLEMENTED in
 *    Private/Spacecraft/LyudSpacecraftCabin.cpp by the cabin programmer.
 *  - ALyudSpacecraftStage: the landing stage that stays on Europa, with its ladder (Private/Spacecraft/LyudSpaceflight.cpp).
 *  - ULyudSpaceflightSubsystem: the flight's director: spawns both, boarding, E, the model, the sky round the cabin, the
 *    sounds, the swap into the real world, the splash, egress, the save (Private/Spacecraft/LyudSpaceflight.cpp).
 *
 * Frames. The CABIN frame is the actor's own (Content/Spacecraft/kit.json): origin = the middle of the cabin's floor,
 * +X forward (the panel, the big porthole), the hatch at -X, +Z up, +Y the seated hero's right. The flight model's cabin
 * frame is the same with Y mirrored: model (X, Y, Z) = Unreal local (X, -Y, Z).
 */

/** What the outside does to the cabin's shell and glass (all 0..1 unless said). */
struct FLyudCraftExterior
{
	/** The plasma round the hull and past the portholes, and its colour temperature in kelvin (1800..7000). */
	float Plasma = 0.f;
	float PlasmaKelvin = 3000.f;
	/** The heat shield's glow, 0..1 (FState::I.Hull01). */
	float Heat = 0.f;
	/** Soot burnt onto the glass (stays till the craft is back on Europa). */
	float Scorch = 0.f;
	/** Frost on the glass's rim (on Europa and in the coast's shade). */
	float Frost = 0.f;
	/** Sea spray running off the glass (1 at the splash, drying over a minute). */
	float Wet = 0.f;
	/** The cloud deck's white through the glass: 1 = nothing else is seen (the model's Whiteout). */
	float Fog = 0.f;
	/** The star's direction in the CABIN frame (Unreal local axes), unit: the glass's glints and the shell's lit side. */
	FVector SunLocal = FVector::UpVector;
	/** The flight second (the materials' own clock, so a save or lyud.Flight.Fast keeps the flames in step). */
	float Time = 0.f;
};

/**
 * The cabin of «Ласточка».
 *
 * ============================== OWNED BY THE CABIN PROGRAMMER ==============================
 * Everything in this class is implemented in Private/Spacecraft/LyudSpacecraftCabin.cpp. The cabin programmer may add
 * private members, components and helper methods freely. The PUBLIC methods below are what the director calls
 * (tools/spacecraft/ENGINE_NOTES.md "For the cabin programmer" says when and in what frame): do not change their
 * signatures without a line under "Interface changes" there.
 *
 * The cabin never decides anything: no flight logic, no checklist, no input, no prompt, no flight sounds (the director
 * plays every sound of SOUND_NOTES.md; the cabin only tells it where things are). It builds itself from kit.json and
 * the SM_SC_* meshes, draws what ApplyState says, animates what PlayActionAnim says, and answers "what is the hero
 * looking at".
 * ============================================================================================
 */
UCLASS()
class LYUDMILOVSK_API ALyudSpacecraft : public AActor
{
	GENERATED_BODY()

public:
	ALyudSpacecraft();

	/** Builds the cabin (kit.json, meshes, MIDs, lamps, collision). Called once by the director right after the spawn;
	 *  a second call does nothing. Everything starts hidden (SetShownInside/Outside(false)), cold and dark. */
	void Build();
	bool IsBuilt() const { return bBuilt; }

	/** Everything to draw this frame: needles, digits, lamps, globe, ball, CRT, bar, switches' positions, the fans, the
	 *  cabin lamps (State.I.bCabinLight and bPowered). Called every tick while the cabin is shown (60 Hz or the frame
	 *  rate); the cabin damps needles and throttles what it likes with DeltaSeconds (real seconds). Must not allocate. */
	void ApplyState(const LyudFlight::FState& State, float DeltaSeconds);

	/**
	 * What E would do for a hero whose eye is at Eye (world) looking along LookDir (world, unit): the action id of the
	 * nearest interaction point within reach and within the look cone, or empty. Pure geometry: the cabin does NOT check
	 * whether the action is allowed now (the director filters by its own state: seated, standing, phase).
	 * Ids: hatch, wheel, sit, stand (looking at the floor/standing room from the couch is not needed: the director
	 * offers `stand` itself), buckle, sw_power, sw_air, sw_gyro, sw_seal, sw_arm, cover, launch, sw_light, sw_crt,
	 * sw_radio, port_big, port_left, port_right, player, needle, thermos, book, nap (the couch's headrest/blanket),
	 * chute_cover, chute_handle. OutWhere (optional): the point's world location (sounds are played there).
	 */
	FString ActionAt(const FVector& Eye, const FVector& LookDir, FVector* OutWhere = nullptr) const;

	/** The world location of an action's point (same ids as ActionAt, plus: speaker, panel, couch, fan_0, fan_1,
	 *  hull_0..hull_3 (random hull points for creaks), jets). Unknown id: the cabin's middle. */
	FVector ActionPoint(FName Action) const;

	/**
	 * Animates a moving part to a logical state the DIRECTOR owns:
	 *  hatch (bOn = open; swings in ~1.5 s), wheel (bOn = sealed: turns_to_open turns in ~0.8 s), cover (bOn = up),
	 *  launch (bOn: the button goes in and comes back by itself), chute_cover (bOn = up), chute_handle (a tug and back),
	 *  buckle (bOn = the harness is over the hero's couch), book (bOn = open in front of the seated hero), thermos
	 *  (bOn = out of the net for a sip, back by itself), sw_light, sw_crt, sw_radio (momentary clicks).
	 * The five checklist toggles are NOT animated through this: their positions come in ApplyState (State.Switches).
	 * bSnap: no animation (a load, a dev jump).
	 */
	void PlayActionAnim(FName Action, bool bOn, bool bSnap = false);
	/** A part started by PlayActionAnim is still moving (hatch, wheel). */
	bool IsAnimating(FName Action) const;

	// ---- places (all WORLD space, read from kit.json through the actor's transform)
	/** The seated hero's eye, X along the couch's look direction (kit "seat".eyes / look_direction). */
	FTransform SeatEye() const;
	/** The hero's pelvis on his couch and the couch's facing (kit "seat".pelvis, couch_yaw): where his body is put. */
	FTransform SeatPelvis() const;
	/** The camera leaning to a porthole: big, left, right (kit "portholes".<id>.look camera/direction). */
	FTransform PortholeView(FName Which) const;
	/** Feet on the platform/ladder top outside the hatch (kit "stand".outside_hatch). */
	FVector HatchExit() const;
	/** Feet just inside the hatch (kit "stand".inside_hatch). */
	FVector HatchInside() const;
	/** Feet in the standing room in the middle (kit "stand".centre). */
	FVector StandPoint() const;
	/** The waterline's height in the cabin frame, cm (kit "afloat".waterline_z): the director floats the actor so that
	 *  this plane lies on the sea. */
	float WaterlineZ() const;
	/** A world point is inside the cabin's pressure shell. */
	bool IsInside(const FVector& World) const;

	// ---- the outside
	/** The fire shell, the heat shield's glow, the glass (scorch, frost, wet, fog). Every tick while shown; set MID
	 *  parameters only when a value changed. */
	void SetExterior(const FLyudCraftExterior& Exterior);
	/** The parachute: Stage 0 none (meshes hidden), 1 drogue, 2 main; Inflate 0..1 of that stage (main: 0.35 reefed);
	 *  SwingDeg = (pitch, roll) of the CANOPY relative to the cabin's own up, degrees (the director tilts the cabin
	 *  actor itself by the model's swing: this is the extra lean of the lines, usually a fraction of it); YawDeg the
	 *  lines' twist. */
	void SetChute(int32 Stage, float Inflate, FVector2D SwingDeg, float YawDeg);
	/** The soft-landing jets' flames under the shield, 0..1 (885..888 s). */
	void SetJets(float Jets01);

	// ---- the record player's visuals
	/** Cell = the sleeve/label atlas cell of the record on the platter (records.json "cell"; -1: empty platter);
	 *  bSpinning; Arm01 0 = on its rest, 1 = on the record's first groove .. 2 = at the label (the side's end). */
	void SetRecord(int32 Cell, bool bSpinning, float Arm01);

	// ---- lamps and showing
	/** The orange flicker of the plasma inside the cabin is the director's own light; this is only the three cabin
	 *  lamps as a whole on top of ApplyState (false forces them off: a nap). */
	void SetCabinLight(bool bOn);
	/** The interior set (inner shell, panel, couches, props, cabin lamps): shown only while the hero is in or at the
	 *  hatch. The outside set (outer shell, shield, fire shell, chute): shown while the craft is seen from outside OR
	 *  through its own portholes (the fire shell). Hidden sets do not tick and cast nothing. */
	void SetShownInside(bool bShown);
	void SetShownOutside(bool bShown);
	bool IsShownInside() const { return bShownInside; }
	bool IsShownOutside() const { return bShownOutside; }

	/** Frees what Build made that is heavy (MIDs stay; meshes are let go for the GC) once the craft is gone from the
	 *  sea and the hero is not on Europa. Build() makes it again. Optional: may be a no-op. */
	void Unbuild();

	virtual void Tick(float DeltaSeconds) override;

protected:
	// The cabin programmer's members go here and below (components as UPROPERTY(Transient); MIDs created once in
	// Build and parented to the base materials; nothing per frame while hidden).
	bool bBuilt = false;
	bool bShownInside = false;
	bool bShownOutside = false;

	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> InsideMeshes;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> OutsideMeshes;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Mids;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> CabinLamps;

	/** Opaque to the director: the cabin's own runtime data (kit measures, needles' damped values, animation clocks). */
	struct FCabinData;
	FCabinData* Data = nullptr;

public:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};

/**
 * The landing stage of «Ласточка» on Europa's ice: truss, four legs with foot pads, engine bell, the ladder up to the
 * hatch's platform. The actor's origin is on the ice under the stage's axis (kit.json "stage" frame), +X the same as
 * the cabin's (the big porthole toward the planet's azimuth); the cabin stands at kit "stage".cabin_seat in it.
 * Owned by the director (LyudSpaceflight.cpp).
 */
UCLASS()
class LYUDMILOVSK_API ALyudSpacecraftStage : public AActor
{
	GENERATED_BODY()

public:
	ALyudSpacecraftStage();

	void Build();
	void SetShown(bool bShown);
	bool IsShown() const { return bShown; }
	/** Where the cabin's origin stands on the stage (world). */
	FTransform CabinSeat() const;
	/** Feet on the ice at the ladder's foot, and on the platform at its top (world). */
	FVector LadderFoot() const;
	FVector LadderTop() const;

	UPROPERTY(VisibleAnywhere, Category = "Spacecraft") TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, Category = "Spacecraft") TObjectPtr<ULyudLadderComponent> Ladder;

private:
	bool bBuilt = false;
	bool bShown = true;
	FVector CabinSeatLocal = FVector(0.f, 0.f, 275.f);
	FVector FootLocal = FVector(-300.f, 0.f, 0.f);
	FVector TopLocal = FVector(-205.f, 0.f, 295.f);
};

/**
 * The view in the cabin (the director sets the fields each tick; nothing is allocated): the eye put on the couch's eye
 * point or leaned to a porthole, the shake, the sink under load and the field of view's squeeze.
 */
UCLASS()
class LYUDMILOVSK_API ULyudFlightCamera : public UCameraModifier
{
	GENERATED_BODY()

public:
	/** 0: the hero's own eye; 1: EyeAt (world). */
	float EyeAlpha = 0.f;
	FVector EyeAt = FVector::ZeroVector;
	/** Added on top (world cm; degrees; degrees of field of view). */
	FVector Offset = FVector::ZeroVector;
	FRotator Shake = FRotator::ZeroRotator;
	float FovAdd = 0.f;
	bool bOn = false;
	virtual bool ModifyCamera(float DeltaTime, struct FMinimalViewInfo& InOutPOV) override;
};

/**
 * The flight's director (docs/spacecraft.md): spawns the stage and the cabin on Europa at play start (worlds where the
 * Europa subsystem has its site; lyud.Flight 0 leaves them out), lets E work the ladder, the hatch, the couch, the
 * checklist, the comforts and the record player (polled, never consumed; one prompt), runs LyudFlight::FFlight every
 * tick while the hero is near the craft, in it, or the capsule is afloat, and draws what it says: the sky round the cabin
 * (the planet, Europa and the stars as impostors turned by the model), the sounds, the voice and the hero's lines, the
 * shake, the entry's light, the swap into the real world at the whiteout's peak, the descent under the canopy to the
 * splash point of Content/Spacecraft/spacecraft.json, the capsule afloat on the real sea, the swim out. Saved at any
 * moment (the model's own string + where the hero is).
 *
 * Console: lyud.Flight.Use (= E), .Report, .GoCraft, .GoCabin, .Launch (does the checklist and presses the button),
 * .Phase <id>, .Fast <x>, .Reset, .Record [n], .Look <big|left|right|back>, .SwimOut.
 */
UCLASS()
class LYUDMILOVSK_API ULyudSpaceflightSubsystem : public UTickableWorldSubsystem, public ILyudSaveable
{
	GENERATED_BODY()

public:
	static ULyudSpaceflightSubsystem* Get(const UObject* WorldContext);
	/** For Python: unreal.LyudSpaceflightSubsystem.get_spaceflight(world). */
	UFUNCTION(BlueprintPure, Category = "Spacecraft", meta = (WorldContext = "WorldContext"))
	static ULyudSpaceflightSubsystem* GetSpaceflight(const UObject* WorldContext) { return Get(WorldContext); }

	UFUNCTION(BlueprintPure, Category = "Spacecraft") ALyudSpacecraft* GetCabin() const { return Cabin; }
	UFUNCTION(BlueprintPure, Category = "Spacecraft") ALyudSpacecraftStage* GetStage() const { return Stage; }
	/** The model's phase id (parked, prelaunch, ascent, coast, approach, entry, chute, splash, afloat). */
	UFUNCTION(BlueprintPure, Category = "Spacecraft") FString PhaseId() const;
	/** The flight second (0 before the launch, 900 afloat). */
	UFUNCTION(BlueprintPure, Category = "Spacecraft") float FlightSeconds() const;
	/** The hero is inside the cabin (seated or standing). */
	UFUNCTION(BlueprintPure, Category = "Spacecraft") bool IsHeroAboard() const { return bAboard; }
	UFUNCTION(BlueprintPure, Category = "Spacecraft") bool IsHeroSeated() const { return bSeated; }
	/** Launched and not yet afloat. */
	UFUNCTION(BlueprintPure, Category = "Spacecraft") bool IsFlying() const;
	/** The cabin hangs or floats in the real world (after the whiteout's peak). */
	UFUNCTION(BlueprintPure, Category = "Spacecraft") bool IsInRealWorld() const { return bRealWorld; }
	/** The capsule is on the real sea. */
	UFUNCTION(BlueprintPure, Category = "Spacecraft") bool IsCapsuleAfloat() const;
	/** What E would do now (an action id; empty: nothing). */
	UFUNCTION(BlueprintPure, Category = "Spacecraft") FString CurrentAction() const { return Action; }
	/** The splash point on the real sea (world, on the mean sea level). */
	UFUNCTION(BlueprintPure, Category = "Spacecraft") FVector SplashPoint() const { return SplashAt; }
	/** A record is turning under the needle. */
	UFUNCTION(BlueprintPure, Category = "Spacecraft") bool IsRecordPlaying() const;
	UFUNCTION(BlueprintPure, Category = "Spacecraft") int32 FlightsFlown() const;

	/** E: do CurrentAction. */
	UFUNCTION(BlueprintCallable, Category = "Spacecraft") void Use();
	/** Does one action by id whatever the hero looks at (tests, dev): the same ids as CurrentAction. */
	UFUNCTION(BlueprintCallable, Category = "Spacecraft") void Do(const FString& ActionId);
	/** Test teleports: on the ice by the ladder; seated in the cabin on Europa (hatch shut and sealed, buckled). */
	UFUNCTION(BlueprintCallable, Category = "Spacecraft") void GoCraft();
	UFUNCTION(BlueprintCallable, Category = "Spacecraft") void GoCabin();
	/** Seats the hero if needed, runs the checklist and presses ПУСК. */
	UFUNCTION(BlueprintCallable, Category = "Spacecraft") void LaunchNow();
	/** Jumps to a phase's start (the model's DevJump): parked resets; afloat = 899 s. */
	UFUNCTION(BlueprintCallable, Category = "Spacecraft") void JumpToPhase(const FString& Phase);
	/** Back on Europa, cold; the hero stays where he is unless he was aboard in flight (then: on the ice by the ladder). */
	UFUNCTION(BlueprintCallable, Category = "Spacecraft") void ResetFlight();
	/** Leans to a porthole (big, left, right) or back to the seat (back/empty). */
	UFUNCTION(BlueprintCallable, Category = "Spacecraft") void LookThrough(const FString& Which);
	UFUNCTION(BlueprintCallable, Category = "Spacecraft") FString Report() const;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudSpaceflightSubsystem, STATGROUP_Tickables); }
	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	// ---- saved
	/** LyudFlight::FFlight::Save(). */
	UPROPERTY(SaveGame) FString SavedFlight;
	/** 0 not aboard, 1 aboard standing, 2 seated, 3 seated and buckled. */
	UPROPERTY(SaveGame) int32 SavedAboard = 0;
	UPROPERTY(SaveGame) bool bSavedHatchOpen = false;
	UPROPERTY(SaveGame) bool bSavedWheelSealed = false;
	UPROPERTY(SaveGame) bool bLandedToastShown = false;
	/** The cabin's place afloat (world cm; the drift's origin) and its heading. */
	UPROPERTY(SaveGame) FVector SavedAfloatAt = FVector::ZeroVector;
	UPROPERTY(SaveGame) float SavedAfloatYaw = 0.f;

	// ---- actors and components (made once, kept for the GC)
	UPROPERTY(Transient) TObjectPtr<ALyudSpacecraft> Cabin;
	UPROPERTY(Transient) TObjectPtr<ALyudSpacecraftStage> Stage;
	/** The sky round the cabin in flight: its root, the planet, its air, Europa astern, the stars. */
	UPROPERTY(Transient) TObjectPtr<AActor> SkyRig;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> PlanetMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> AirMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> EuropaMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> StarMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PlanetMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> AirMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> EuropaMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> StarMID;
	/** The plasma's flicker inside the cabin. */
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> FireLight;
	/** The panel's own glow on the hero's knees and the couches (the instrument materials are unlit). */
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> PanelLight;
	/** The loops, by slot (ELoop in the .cpp), and the record's music. */
	UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Loops;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> RecordMusic;
	UPROPERTY(Transient) TObjectPtr<USoundBase> RecordSound;
	UPROPERTY(Transient) TObjectPtr<ULyudFlightCamera> Camera;

	// ---- spawn and site data (spacecraft.json)
	bool TrySpawn();
	bool bSpawned = false;
	float SpawnWait = 0.f;
	FVector SplashAt = FVector::ZeroVector;
	float SplashHeadingDeg = 0.f;
	FVector FlightSpot = FVector::ZeroVector;
	FRotator PadRotation = FRotator::ZeroRotator;

	// ---- the hero in the craft
	bool bAboard = false;
	bool bSeated = false;
	bool bBuckled = false;
	bool bHatchOpen = false;
	bool bWheelSealed = false;
	bool bCoverUp = false;
	bool bChuteCoverUp = false;
	bool bBookOpen = false;
	bool bRealWorld = false;
	bool bActive = false;
	FString Action;
	FString ShownPrompt;
	FVector ActionWhere = FVector::ZeroVector;

	/** The model (LyudFlight::FFlight) and the director's runtime state; made at the spawn, freed in Deinitialize. */
	struct FImpl;
	FImpl* Impl = nullptr;

	friend class SLyudFlightSubtitle;
};
