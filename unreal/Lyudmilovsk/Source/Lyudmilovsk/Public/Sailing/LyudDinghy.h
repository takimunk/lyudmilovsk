// The yacht school's boats as actors (docs/yacht_school.md). ALyudDinghy is one sailing dinghy: it owns its
// LyudSail::FBoat (Private/Sailing/SailModel.h, plain C++), samples the wind and the sea for it, steps it, and draws
// it (hull, spars, sail, foils, crew, wake) and sounds it. WHO sails it is not its business: a helm (the hero's input
// or the school's LyudSail::FHelm) hands it controls each tick. ALyudSailLaunch is the coach's motor boat, ALyudSailMark
// a race mark or buoy. ULyudSailing (LyudSailing.h) owns them all.
//
// CONTRACT between the two engine parts (the boat part owns this file and LyudDinghy*.cpp / LyudSailHud.cpp; the
// school part owns LyudSailing.h and LyudSailing*.cpp). Signatures below are fixed; additions are fine.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/PimplPtr.h"
#include "LyudDinghy.generated.h"

class AHeroCharacter;
class UAudioComponent;
class USceneComponent;
class UStaticMeshComponent;
struct FLyudSailHudData;
namespace LyudSail { struct FBoat; struct FControls; struct FEnv; struct FHelm; struct FLaunch; }

/** How much of a boat is simulated and drawn. */
UENUM()
enum class ELyudDinghyLod : uint8
{
	/** Full model each frame, every part, crew bodies, sounds: the hero's boat and boats within ~60 m. */
	Near,
	/** Full model at a lower rate, LOD meshes, simple crew, no sounds but the loud ones: to ~250 m. */
	Mid,
	/** LyudSail::StepFar on the polar a few times a second, one merged far mesh: beyond. */
	Far,
	/** Not drawn or stepped at all (ashore in the park, laid up): the actor stays pooled. */
	Parked
};

/** What a boat shows about itself to the school and the HUD, refreshed by Advance. */
struct FLyudDinghyTelemetry
{
	FVector2D PosM = FVector2D::ZeroVector;   // world metres
	float YawRad = 0.f;
	float SpeedMs = 0.f;                      // through the water
	float HeelDeg = 0.f;
	float TrueWindAngleRad = 0.f;             // + = starboard tack
	float TrueWindMs = 0.f;
	float DepthM = 0.f;
	float Luff = 0.f;
	int32 State = 0;                          // LyudSail::EState
	uint32 Events = 0;                        // LyudSail::EEvent bits of the last Advance
	int32 Point = 0;                          // LyudSail::EPoint
	bool bPlaning = false;
};

UCLASS(NotPlaceable, NotBlueprintable)
class LYUDMILOVSK_API ALyudDinghy : public AActor
{
	GENERATED_BODY()

public:
	ALyudDinghy();
	virtual ~ALyudDinghy();

	/** Makes it a boat of this class (0 optimist, 1 cadet, 2 luch = LyudSail::EBoat), with its sail number and hull colour
	 *  (an index into the school's colourways). Safe to call again on a pooled actor. */
	void Setup(int32 BoatClass, int32 SailNumber, int32 Colours, uint32 Seed);
	int32 GetBoatClass() const { return BoatClass; }
	int32 GetSailNumber() const { return SailNumber; }

	/** The model itself (never null after Setup). The school reads and places it through LyudSail:: calls. */
	LyudSail::FBoat& Boat();
	const LyudSail::FBoat& Boat() const;

	/** Puts it on the water here (metres, yaw rad), sailing or in another LyudSail::EState; the drawn boat jumps there. */
	void PlaceAt(const FVector2D& PosM, float YawRad, int32 State, float SpeedMs = 0.f);
	/** Ashore on its trolley (or on the sand) at a world transform, rigged or with the sail down / under its winter cover. */
	void ParkAt(const FTransform& World, bool bRigged, bool bCovered);

	void SetLod(ELyudDinghyLod InLod);
	ELyudDinghyLod GetLod() const { return Lod; }

	/** One step of the boat: samples wind (ULyudWindSubsystem) and sea (ALyudSea) at the boat, steps the model with these
	 *  controls (Near/Mid), moves the drawn parts, plays its sounds. Called by ULyudSailing for every boat afloat, in the
	 *  subsystem's tick (the actor itself does not tick). */
	void Advance(const LyudSail::FControls& Controls, float DeltaTime);
	/** The far step: the helmsman's course and speed scale on the polar (LyudSail::StepFar). */
	void AdvanceFar(float CourseYawRad, float SpeedScale, float DeltaTime);
	/** Towed behind the launch (LyudSail::StepTowed). */
	void AdvanceTowed(const FVector2D& HitchM, float RopeM, float DeltaTime);
	/** The env it used last (wind toward m/s, depth, wave): the school hands the same to the helmsman. */
	const LyudSail::FEnv& Env() const;
	const FLyudDinghyTelemetry& Telemetry() const { return Tele; }

	/** Crew as drawn. Slot 0 = helm, 1 = crew (cadet). A townsperson: their people-life key (the boat shows that person's
	 *  body and looks, seated and animated by the boat's state); NAME_None empties the seat. */
	void SetCrew(int32 Slot, FName PersonKey, float AgeYears, bool bLifeJacket = true);
	FName GetCrew(int32 Slot) const;

	/** The hero takes the helm (seated through AHeroCharacter::SitOn, input captured by the boat's own handler: tiller
	 *  A/D, sheet W/S or wheel, hike Q/E or auto, board Z/X, right the boat Space when capsized, look around with the
	 *  mouse) or gives it up. While he has it, Advance ignores the Controls passed and uses his. bCheckReach: only if he
	 *  stands within reach of the boat. Leaving is allowed where the school says (CanLeave callback), else a word. */
	bool Board(AHeroCharacter* Hero, bool bCheckReach = true);
	bool Leave(bool bForce = false);
	bool IsHeroAboard() const { return bHeroAboard; }
	static ALyudDinghy* HeroBoat(const UObject* WorldContext);
	/** Asked before the hero steps out (true = he may here); set by the school. */
	TFunction<bool(const ALyudDinghy&)> CanLeave;
	/** The assists (easy mode): auto sheet, auto hike, no capsize. */
	void SetAssist(bool bAutoSheet, bool bAutoHike, bool bNoCapsize);
	/** A hint the tutor/HUD shows for a few seconds (a strings key, e.g. sail.tutor.ease); the HUD belongs to the boat part. */
	void ShowHint(FName Key, float Seconds = 4.f);
	/** Lesson / race lines on the hero's HUD, set by the school: title, objective lines, a countdown (seconds, <0 none),
	 *  the next mark's world position (zero = none) for the HUD's arrow. */
	void SetHudTask(const FText& Title, const TArray<FText>& Lines, float Countdown, const FVector2D& NextMarkM);

	// ---- additions of the boat part (not in the first contract) ----
	/** The tutor's own hints from LyudSail::Advice on the hero's HUD (on by default; each at most once in ~20 s). A lesson
	 *  that has its own words turns them off. */
	void SetTutorHints(bool bOn);
	/** The boat is shown ashore by ParkAt (on its trolley): SetLod(Parked) then keeps it drawn, still and unstepped. */
	bool IsOnTrolley() const { return bOnTrolley; }
	/** What the hero's HUD draws (LyudSailHud.h); never null after Setup. */
	const FLyudSailHudData* HudData() const;
	/** The scene component that heels and pitches (the hull's frame, boat origin at the waterline amidships). */
	USceneComponent* HullFrame() const;

	/** Fired (game thread) when the model raises events in Advance: (boat, LyudSail::EEvent bits). */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSailEvents, ALyudDinghy&, uint32);
	FOnSailEvents OnSailEvents;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	struct FImpl;
	TPimplPtr<FImpl> Impl;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient) TArray<TObjectPtr<UObject>> Kept;   // meshes, MIDs, components, audio kept alive
	/** The nine loops of LyudSail::Sound::ELoop while the boat is Near (null: not playing). */
	UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Loops;
	int32 BoatClass = 2;
	int32 SailNumber = 0;
	ELyudDinghyLod Lod = ELyudDinghyLod::Parked;
	bool bHeroAboard = false;
	bool bOnTrolley = false;
	FLyudDinghyTelemetry Tele;
	/** The hero is out of the boat (he stood up, or was stood up): the HUD, the view and the hooks go. */
	void HeroGone(bool bStandHim);
};

/** The coach's aluminium launch with its outboard: LyudSail::FLaunch drawn, with a driver and a flag staff. */
UCLASS(NotPlaceable, NotBlueprintable)
class LYUDMILOVSK_API ALyudSailLaunch : public AActor
{
	GENERATED_BODY()

public:
	ALyudSailLaunch();
	virtual ~ALyudSailLaunch();

	LyudSail::FLaunch& Launch();
	void PlaceAt(const FVector2D& PosM, float YawRad);
	/** Moored at the slip's foot / ashore, engine off. */
	void ParkAt(const FTransform& World);
	/** Drives toward a point at a cruise speed (LyudSail::LaunchDriveTo + StepLaunch), or holds (Cruise 0 = drift, engine
	 *  idling); moves the drawn boat, wake and engine sound. */
	void Advance(const FVector2D& TargetM, float CruiseMs, float RadiusM, float DeltaTime);
	void SetDriver(FName PersonKey);
	/** A flag on the staff by its id in sailing.json (flag_ap, flag_p, flag_x, flag_first_sub ...), NAME_None lowers it. */
	void SetFlag(FName FlagId);
	/** A signal from the launch: 0 whistle short, 1 whistle long, 2 recall (three blasts), 3 horn short, 4 horn long. */
	void Sound(int32 Signal);
	FVector2D PosM() const;
	float YawRad() const;
	/** The hitch a towed dinghy hangs on, metres. */
	FVector2D HitchM() const;

private:
	struct FImpl;
	TPimplPtr<FImpl> Impl;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient) TArray<TObjectPtr<UObject>> Kept;
	/** The outboard's four loops (LyudSail::Sound::MixOutboard) while the engine runs. */
	UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Loops;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};

/** A race mark, the pin buoy with its flag, a training buoy or the man-overboard buoy: rides the waves where put. */
UCLASS(NotPlaceable, NotBlueprintable)
class LYUDMILOVSK_API ALyudSailMark : public AActor
{
	GENERATED_BODY()

public:
	ALyudSailMark();
	/** Kind: 0 mark (orange cylinder), 1 pin with flag, 2 small training buoy, 3 man-overboard buoy. */
	void Setup(int32 Kind);
	void PlaceAt(const FVector2D& PosM);
	/** Out of the water and hidden (pooled). */
	void Stow();
	/** Rides the sea surface; called by the school for marks near the camera only (far ones sit at sea level). */
	void Advance(float DeltaTime);
	FVector2D PosM() const { return Pos; }

private:
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient) TArray<TObjectPtr<UObject>> Kept;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Body;
	FVector2D Pos = FVector2D::ZeroVector;
	int32 Kind = 0;
	int32 BuiltKind = -1;
	float Clock = 0.f;
	float LeanPitch = 0.f, LeanRoll = 0.f;
	bool bStowed = true;
};
