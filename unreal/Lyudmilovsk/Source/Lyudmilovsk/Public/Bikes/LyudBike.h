#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyudSave.h"
#include "Bikes/LyudBikeRider.h"
#include "LyudBike.generated.h"

class UAudioComponent;
class UBoxComponent;
class UMaterialInstanceDynamic;
class USkeletalMeshComponent;
class USoundAttenuation;
class UStaticMeshComponent;

/**
 * The bicycle's measures (Content/Bikes/bike.json, written by blender/bikes/build_bike.py with the meshes): Unreal cm,
 * X forward, Y to the bike's right, Z up, origin on the ground under the bottom bracket, the bars straight.
 */
struct LYUDMILOVSK_API FLyudBikeGeom
{
	FVector AxleF = FVector(59.5f, 0.f, 33.4f);
	FVector AxleR = FVector(-40.5f, 0.f, 33.4f);
	FVector BB = FVector(0.f, 0.f, 27.f);
	float WheelRadius = 33.4f;
	float CrankLength = 17.f;
	float PedalY[2] = { -15.5f, 15.5f };            // left, right
	FVector SteerPoint = FVector(52.f, 0.f, 62.f);
	FVector SteerDir = FVector(-0.292f, 0.f, 0.956f);
	FVector SaddleTop = FVector(-21.f, 0.f, 98.f);
	FVector Hoods[2] = { FVector(32.f, -21.f, 95.f), FVector(32.f, 21.f, 95.f) };
	FVector HoodAxis[2] = { FVector(1.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f) };
	float HoodRadius = 1.8f;
	FVector BarTops[2] = { FVector(25.f, -12.f, 96.f), FVector(25.f, 12.f, 96.f) };
	float BarRadius = 1.3f;
	FVector StandHinge = FVector(-8.f, -6.f, 25.f);
	FVector StandAxis = FVector(0.f, 1.f, 0.f);
	float StandUpDeg = 75.f;
	float ParkRoll = 7.f;
	/** Each part's pivot (where its mesh's origin is), by mesh name (SM_Bike_Steer, ...). */
	TMap<FName, FVector> Pivots;
	float ChainLinks = 108.f;
	FBox Bounds = FBox(FVector(-75.f, -25.f, 0.f), FVector(95.f, 25.f, 105.f));
	bool bFromFile = false;

	static const FLyudBikeGeom& Get();
	FVector Pivot(FName Part) const { const FVector* P = Pivots.Find(Part); return P ? *P : FVector::ZeroVector; }
};

/** What a body does with a bike (the choreography of ALyudBike::RiderPose). */
UENUM()
enum class ELyudBikeAct : uint8
{
	None,
	/** Beside it, the hands on the bars, walking it along (the bike on the walker's right). */
	Lead,
	/** Getting on from the left: the right leg over the saddle's back, onto the saddle, the feet onto the pedals. */
	Mount,
	Ride,
	/** Getting off: Mount backwards. */
	Dismount,
};

/** One body with a bike this frame. */
struct FLyudBikeRide
{
	ELyudBikeAct Act = ELyudBikeAct::None;
	/** 0..1 through a Mount or Dismount. */
	float Phase = 0.f;
	/** The body's size against the clips' (pelvis height / 93 cm). */
	float Scale = 1.f;
	/** 0..1 how far the body has come onto the bike (blends in and out). */
	float Weight = 1.f;
	/** Pedalling hard: out of the saddle a little, pulling on the bars. */
	bool bSprint = false;
	/** How much of the trunk's lean the neck and head take back to look ahead (the hero's own eyes see the way by the camera:
	 *  less, so his collar stays below them). */
	float HeadUp = 0.8f;
};

/**
 * A bicycle: a classic 1980s steel road racer (blender/bikes/build_bike.py, docs/bikes.md) made of moving parts: the
 * frame, the steering (fork, stem, bars), two wheels, the crankset and its two pedals that stay level as the cranks
 * turn, the chain, a kickstand. It stands where it was left (saved), leaning on its stand; whoever has it (the hero's
 * UHeroBikeComponent, a resident through ULyudBikeSubsystem) moves it with Place and Roll every frame, and asks it
 * where a body's hips, feet and hands go (RiderPose). Nothing here ticks while it stands.
 */
UCLASS(NotPlaceable, BlueprintType)
class LYUDMILOVSK_API ALyudBike : public AActor, public ILyudSaveable
{
	GENERATED_BODY()

public:
	ALyudBike();

	/** bikes.json id: "club", "academ", "Novikova1.s0", ... */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bike") FName BikeId;
	/** The frame's paint (sRGB). */
	UPROPERTY(EditAnywhere, Category = "Bike") FLinearColor Paint = FLinearColor(0.42f, 0.03f, 0.03f);

	/** Where it stands (ground under the bottom bracket, cm), its heading, and whether it leans on its stand there. */
	UPROPERTY(SaveGame) FVector ParkedAt = FVector::ZeroVector;
	UPROPERTY(SaveGame) float ParkedYaw = 0.f;
	/** Leaning against a wall instead (degrees toward its right, + or left -): entrances, the rack. 0 = on the stand. */
	UPROPERTY(SaveGame) float ParkedLean = 0.f;
	UPROPERTY(SaveGame) bool bMoved = false;

	/** Builds the parts (meshes from /Game/Bikes/Meshes) and stands it at At facing Yaw. */
	void Setup(FName Id, const FLinearColor& InPaint);
	/** Stands it where it is left: on its stand (Lean 0), or leaning that many degrees on a wall at its right (+) or left (-). */
	void Park(const FVector& Ground, float Yaw, float Lean = 0.f);
	/** Puts it on the ground at Ground (under the bottom bracket) heading Yaw, tipped by Pitch (nose up +) and Roll
	 *  (leaning to its right +), the stand folded by StandDown (1 down). Called every frame by whoever moves it. */
	void Place(const FVector& Ground, float Yaw, float Pitch, float Roll, float StandDown);
	/** Turns the wheels, the bars, the cranks (Pedal: they turn with the wheel's drive; else they stand or go to
	 *  CrankTo, deg, when CrankTo >= 0), the chain, and makes the sounds of Speed (cm/s) on its ground (bLoose: gravel,
	 *  earth, grass). */
	void Roll(float DeltaSeconds, float Speed, float SteerDeg, bool bPedal, float CrankTo, bool bLoose, bool bBraking);
	/** The bike stops making sounds (parked, hidden). */
	void Hush();

	/** Where a body's hips, feet and hands go this frame, in Mesh's component space. */
	void RiderPose(const USkeletalMeshComponent* Mesh, const FLyudBikeRide& Ride, FBikeRiderPose& Out) const;
	/** The world frame of the bike's ground point (under the bottom bracket, leaning with it). */
	FTransform GroundFrame() const;
	/** Where the rider's eyes would be on it (to look from, cm). */
	float GetCrank() const { return Crank; }
	float GetSpeed() const { return Speed; }
	float GetSteer() const { return Steer; }
	/** Someone has it (rides or leads it). */
	bool IsTaken() const { return Holder.IsValid(); }
	AActor* GetHolder() const { return Holder.Get(); }
	void SetHolder(AActor* InHolder) { Holder = InHolder; }

	/** The bike's own size, for checks (cm, local frame). */
	static const FLyudBikeGeom& Geom() { return FLyudBikeGeom::Get(); }

	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Bike") TObjectPtr<USceneComponent> Root;
	/** Leans and pitches about the ground point under the bottom bracket; all the parts hang on it. */
	UPROPERTY(VisibleAnywhere, Category = "Bike") TObjectPtr<USceneComponent> Lean;
	UPROPERTY(VisibleAnywhere, Category = "Bike") TObjectPtr<UStaticMeshComponent> Frame;
	UPROPERTY(VisibleAnywhere, Category = "Bike") TObjectPtr<UStaticMeshComponent> SteerPart;
	UPROPERTY(VisibleAnywhere, Category = "Bike") TObjectPtr<UStaticMeshComponent> WheelF;
	UPROPERTY(VisibleAnywhere, Category = "Bike") TObjectPtr<UStaticMeshComponent> WheelR;
	UPROPERTY(VisibleAnywhere, Category = "Bike") TObjectPtr<UStaticMeshComponent> CrankPart;
	UPROPERTY(VisibleAnywhere, Category = "Bike") TObjectPtr<UStaticMeshComponent> PedalL;
	UPROPERTY(VisibleAnywhere, Category = "Bike") TObjectPtr<UStaticMeshComponent> PedalR;
	UPROPERTY(VisibleAnywhere, Category = "Bike") TObjectPtr<UStaticMeshComponent> Chain;
	UPROPERTY(VisibleAnywhere, Category = "Bike") TObjectPtr<UStaticMeshComponent> Stand;
	/** What the view ray finds (E, R); blocks nothing. */
	UPROPERTY(VisibleAnywhere, Category = "Bike") TObjectPtr<UBoxComponent> Hit;

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PaintMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ChainMID;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> TyreSound;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> FreeSound;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> ChainSound;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> Atten;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> TyreAsphalt;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> TyreGravel;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> BrakeSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> StandSound;

private:
	TWeakObjectPtr<AActor> Holder;
	float Speed = 0.f;
	float Steer = 0.f;
	float Crank = 0.f;
	float WheelAngle[2] = { 0.f, 0.f };
	float ChainScroll = 0.f;
	float StandDownNow = 1.f;
	float FreeClock = 0.f;
	float BrakeCooldown = 0.f;
	bool bLooseNow = false;
	bool bBuilt = false;

	void PoseParts();
	void MakeSounds();
};
