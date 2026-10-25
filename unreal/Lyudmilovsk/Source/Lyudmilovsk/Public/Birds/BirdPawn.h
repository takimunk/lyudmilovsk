#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraTypes.h"
#include "GameFramework/Pawn.h"
#include "BirdPawn.generated.h"

class UBirdSelfBody;
class ABirdActor;
class UCameraComponent;
class USphereComponent;
class UInputAction;
struct FInputActionValue;
class FBirdAir;
class FHeadTracker;
class SBirdHud;
class FBirdSound;
class UBirdWindStreaks;
namespace BirdFlight { struct FParams; struct FState; }

/** What the player bird is doing. */
UENUM()
enum class EBirdMode : uint8
{
	Arriving,   // the camera is flying in from the hero's eyes
	Air,
	Ground,     // standing, hopping or walking on anything: ground, roof, rail, the hero's hand
	Leaving,    // the camera is flying back to the hero
	Swimming,   // a water bird sitting on the sea
};

/** Why a flight ended against the player's will (UBirdFlightSubsystem turns them back into the hero). */
UENUM()
enum class EBirdFate : uint8
{
	None,
	Crash,      // flew into something too fast
	Caught,     // a cat or a dog got it
	Drowned,    // a land bird in the sea
};

/**
 * Input as the player bird reads it each frame: the real keyboard and mouse, or a script
 * (UBirdFlightSubsystem's Bird.Script, for offscreen recordings) feeding the very same fields.
 */
struct FBirdControls
{
	bool Left = false, Right = false;     // mouse buttons: the two wings
	bool Space = false, Shift = false, Ctrl = false;
	bool FreeLook = false;                // C or the middle button
	bool Return = false;                  // R
	bool Peck = false, Call = false;      // E, F (pressed this frame)
	FVector2D Move = FVector2D::ZeroVector;   // WASD, -1..1 (X right, Y forward)
	float Roll = 0.f;                     // A / D in the air
	FVector2D Look = FVector2D::ZeroVector;   // degrees this frame (yaw, pitch)
};

/**
 * The bird you play: first person from its eye. The mouse is its head and the body steers after the
 * head; the two mouse buttons are its wings (docs/bird_flight.md). Flight physics in BirdFlightModel,
 * the air in FBirdAir, the body you see in UBirdSelfBody, AirPods in FHeadTracker. Spawned and
 * possessed by UBirdFlightSubsystem, which also brings the camera in from the hero and back.
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ABirdPawn : public APawn
{
	GENERATED_BODY()

public:
	ABirdPawn();
	virtual ~ABirdPawn() override;

	/** Before possession: the species, where, which way, and whether it stands (on a hand, a roof) or flies.
	 *  Outside is the bird's body as everyone else sees it (bird life's ABirdActor); null spawns one. */
	void Setup(FName InSpecies, const FVector& Location, float Yaw, const FVector& VelocityCm, bool bStanding, ABirdActor* Outside = nullptr);
	/** The body seen from outside (camera flights, chase camera, the world). */
	ABirdActor* GetOutsideBody() const { return OutsideBody.Get(); }

	/** The camera flies from From (the hero's view) into the bird's eye over Seconds. */
	void BeginArrival(const FMinimalViewInfo& From, float Seconds);
	/** The camera flies to To (the hero's eyes) and then calls OnLeft. */
	void BeginLeaving(const FMinimalViewInfo& To, TFunction<void()> OnLeft);

	/** Holding still in the air or on a surface without the camera flight (a loaded game). */
	void SkipArrival();

	FName GetSpecies() const { return Species; }
	EBirdMode GetMode() const { return Mode; }
	/** The bird's own view, whatever the camera flights are doing. */
	FMinimalViewInfo BirdView() const;
	FVector GetVelocityCm() const;
	/** Where the feet are standing, or the body's centre in the air. */
	FVector FeetLocation() const { return Mode == EBirdMode::Ground || Mode == EBirdMode::Swimming ? Feet : GetActorLocation(); }
	float GetAirspeed() const;
	/** 0..1 while R is held, 1 starts the way back. */
	float ReturnProgress() const { return ReturnHeld / ReturnHoldSeconds; }
	bool HasLandingSpot(FVector& Out) const { Out = LandingSpot; return bLandingSpot; }
	bool IsHeadTracked() const;
	FString DebugLine() const;
	UPrimitiveComponent* GetRootPrimitive() const;
	/** Seconds since the bird was possessed (the control hints fade out). */
	float TimeAsBird() const { return Age; }

	/** After the camera arrives on the palm, the bird flutters down onto the ground in front of the hero by
	 *  itself before the player takes over (set by the subsystem after a befriending). */
	bool bDescendFirst = false;
	bool IsDescending() const { return bAutoDescend; }

	/** Scripted input instead of the keyboard and mouse (offscreen recordings, tests). */
	bool bScripted = false;
	FBirdControls Script;
	/** A fake AirPods head for scripts and lyud.Bird.FakeHead. */
	void SetFakeHead(const FRotator& Head, bool bOn);

	/** Called when R has been held long enough; the subsystem starts the way back. */
	TFunction<void()> OnReturnRequested;
	/** Called once when the flight ends badly (a crash, a cat, the sea); the subsystem takes it from there. */
	TFunction<void(EBirdFate, const FVector&)> OnFate;
	/** Called on F: the subsystem finds a bird of the same species nearby to answer. */
	TFunction<void()> OnCall;

	/** 1 rested .. 0 spent. */
	float GetEnergy() const;
	bool CanSwim() const { return bSwims; }
	EBirdFate GetFate() const { return Fate; }
	/** A cat or a dog close and coming: the HUD warns. */
	bool IsInDanger() const { return DangerT > 0.f; }
	/** Pushed by another bird (a fight): a jolt, a step back, a little energy. */
	void TakePeck(const FVector& From);
	/** A line for the HUD for a few seconds (a localization key). */
	void ShowNote(const FString& Key, float Seconds = 3.f) { NoteKey = Key; NoteT = Seconds; }
	const FString& GetNote() const { return NoteKey; }
	float GetNoteTime() const { return NoteT; }

	static constexpr float ReturnHoldSeconds = 1.5f;

	virtual void Tick(float DeltaSeconds) override;
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Bird") TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere, Category = "Bird") TObjectPtr<UBirdSelfBody> Body;
	/** The head: the camera and the beak; turns with the gaze. */
	UPROPERTY(VisibleAnywhere, Category = "Bird") TObjectPtr<USceneComponent> Head;
	UPROPERTY(VisibleAnywhere, Category = "Bird") TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(Transient) TObjectPtr<ABirdActor> OutsideBody;
	UPROPERTY(VisibleAnywhere, Category = "Bird") TObjectPtr<UBirdWindStreaks> WindStreaks;
	UPROPERTY(Transient) TObjectPtr<class UAudioComponent> SoundOut;

private:
	void ReadControls(float Dt, FBirdControls& C);
	void OnLook(const FInputActionValue& Value);
	void StepAir(const FBirdControls& C, float Dt);
	void StepGround(const FBirdControls& C, float Dt);
	void StepSwim(const FBirdControls& C, float Dt);
	void EnterSwim(const FVector& At);
	float WaterZ(const FVector& At) const;
	bool IsWaterBelow(const FVector& At) const;
	void CheckDanger(float Dt);
	void SetFate(EBirdFate F);
	void TakeOff();
	void Land(const FVector& Point, const FVector& Normal);
	void FindLandingSpot(float Flare);
	void UpdateHead(const FBirdControls& C, float Dt);
	void UpdateBody(float Dt);
	void PlayCall();
	void UpdateOutsideBody(float Dt);
	float TrackedBlend() const;
	FMinimalViewInfo ChaseView() const;

	FName Species;
	EBirdMode Mode = EBirdMode::Air;
	TSharedPtr<BirdFlight::FParams> Params;
	TSharedPtr<BirdFlight::FState> Flight;
	TSharedPtr<FBirdAir> Air;
	TSharedPtr<FHeadTracker> Tracker;
	TSharedPtr<SBirdHud> Hud;
	TSharedPtr<FBirdSound> Sound;
	bool bSwims = false;
	float RunUp = 0.f;
	EBirdFate Fate = EBirdFate::None;
	float FateT = 0.f, DangerT = 0.f, DangerCheck = 0.f;
	FString NoteKey;
	float NoteT = 0.f;
	FVector SwimVel = FVector::ZeroVector;
	double PrevPhase = 0.0;
	bool bAutoDescend = false;

	// gaze and head
	FVector2D MouseLook = FVector2D::ZeroVector;
	FRotator FreeHead = FRotator::ZeroRotator;   // C / middle mouse
	FRotator TrackHead = FRotator::ZeroRotator;  // AirPods, eased in and out
	FRotator TrackZero = FRotator::ZeroRotator;  // the neck's centre (drifts to the head)
	float TrackWeight = 0.f;
	FRotator BodyRot = FRotator::ZeroRotator;
	float HeadBob = 0.f, PeckT = 0.f, Jolt = 0.f;

	// the wings from the buttons
	double PendingL = -1.0, PendingR = -1.0;
	float HeldL = 0.f, HeldR = 0.f;
	bool bPrevL = false, bPrevR = false, bPrevSpace = false;
	double AutoClock = 0.0;
	float SpaceHeld = 0.f, TakeoffAssist = 0.f, AssistClock = 0.f;
	float TuckL = 0.f, TuckR = 0.f, Flare = 0.f;

	// standing and hopping
	FVector Feet = FVector::ZeroVector;
	FVector HopVel = FVector::ZeroVector;
	bool bHopping = false, bAnchored = false;
	float StepPhase = 0.f, Fold = 0.f, LegsDown = 0.f;
	float Stun = 0.f, Ruffle = 0.f, StuckTime = 0.f;
	FVector LandingSpot = FVector::ZeroVector;
	bool bLandingSpot = false;
	float ReturnHeld = 0.f;
	float Age = 0.f;
	float Time = 0.f;
	FVector LastAir = FVector::ZeroVector;
	float LastUpdraft = 0.f;

	// the camera flights in and out
	FMinimalViewInfo TransFrom, TransTo;
	float TransT = 0.f, TransDur = 1.f;
	TFunction<void()> OnLeftCb;
	float SavedNearClip = -1.f;
	bool bOwnOutsideBody = false;
	bool bInsideView = true;
};
