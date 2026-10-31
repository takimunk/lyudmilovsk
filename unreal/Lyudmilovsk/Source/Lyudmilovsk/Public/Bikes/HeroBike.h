#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Bikes/LyudBike.h"
#include "HeroBike.generated.h"

class AHeroCharacter;
class ALyudBike;
class ALyudSea;

/**
 * The hero and a bicycle (docs/bikes.md). On foot, looking at a bike within reach: E gets on it, R takes it by the
 * bars and walks it along beside you (R again puts it on its stand). Riding: W pedals, S brakes, A/D steer, Shift
 * pedals hard; the bike leans into turns by its speed, its wheels and cranks turn with the road, the camera stays
 * at the hero's eyes over the bars (his own hands on the bar tops, knees coming up as he pedals), E gets off (it
 * stops first). The bike stays where it was left (ALyudBike saves itself). Polled keys, nothing consumed.
 *
 * ULyudBikeSubsystem gives the player's pawn this component; AHeroCharacter calls HeroTick from its Tick (after its
 * own animation state, before the body's animation runs) and hands it the move input while it rides.
 */
UCLASS()
class LYUDMILOVSK_API UHeroBikeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeroBikeComponent();

	static UHeroBikeComponent* Get(const AActor* Hero);

	/** On it: getting on, riding, getting off (the hero's own walking and turning are off). */
	bool IsOnBike() const { return Act == ELyudBikeAct::Mount || Act == ELyudBikeAct::Ride || Act == ELyudBikeAct::Dismount; }
	/** Walking it along by the bars. */
	bool IsLeading() const { return Act == ELyudBikeAct::Lead; }
	ALyudBike* GetBike() const { return Bike.Get(); }
	float GetSpeed() const { return Speed; }

	/** The move input while on the bike (x right, y forward), every frame it is held. */
	void SetMoveInput(const FVector2D& V) { MoveInput = V; bMoveInput = true; }
	/** Everything for this frame: keys, the ride, where the hero and the bike are, the body's targets. */
	void HeroTick(float DeltaSeconds);
	/** The eye point to look from (smooths the step onto and off the bike). */
	FVector SmoothEye(const FVector& Eye, float DeltaSeconds);

	/** Tests and the console: get on, off, lead, let go of a bike (the nearest when null). */
	bool Mount(ALyudBike* Which);
	void Dismount();
	bool Lead(ALyudBike* Which);
	void LetGo();
	/** Tests: ride with this input for a while (throttle -1..1, steer -1..1) as if the keys were held. */
	void SetAutopilot(float Throttle, float SteerIn, float Seconds) { AutoThrottle = Throttle; AutoSteer = SteerIn; AutoUntil = Seconds; }

private:
	AHeroCharacter* Hero() const;
	ALyudBike* BikeInView() const;
	void UpdateKeys();
	void Ride(float DeltaSeconds);
	void Follow(float DeltaSeconds);
	void PlaceHero();
	void Release(bool bStandBeside);
	void FeedBody();
	/** Ground under a point (cm), whether it is loose (earth, grass) and whether anything is there at all. */
	bool GroundAt(const FVector& At, float& OutZ, bool& bLoose) const;

	TWeakObjectPtr<ALyudBike> Bike;
	UPROPERTY(Transient) TObjectPtr<ALyudSea> Sea;
	bool bSeaLooked = false;
	ELyudBikeAct Act = ELyudBikeAct::None;
	float Phase = 0.f;
	float Weight = 0.f;
	FVector2D MoveInput = FVector2D::ZeroVector;
	bool bMoveInput = false;
	// the bike on the ground: under its bottom bracket, heading, pitch, lean, bars, speed (cm/s)
	FVector Ground = FVector::ZeroVector;
	float Yaw = 0.f;
	float Pitch = 0.f;
	float LeanDeg = 0.f;
	float StartRoll = 0.f;
	float Steer = 0.f;
	float Speed = 0.f;
	float StandDown = 1.f;
	float Still = 0.f;
	float FootDown = 0.f;
	bool bPedal = false;
	bool bBrake = false;
	bool bSprint = false;
	bool bLoose = false;
	bool bWantOff = false;
	float CoastTime = 0.f;
	// the eye
	FVector LastEye = FVector::ZeroVector;
	float EyeEase = 0.f;
	bool bEyeValid = false;
	// tests
	float AutoThrottle = 0.f, AutoSteer = 0.f, AutoUntil = 0.f;
	TEnumAsByte<ECollisionEnabled::Type> HeroCollision = ECollisionEnabled::QueryAndPhysics;
	FTransform MeshRelative;
};
