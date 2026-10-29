#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Railway/LyudRailway.h"
#include "LyudTrain.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UPointLightComponent;
class USpotLightComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;
class UInstancedStaticMeshComponent;
class UBoxComponent;
class ULyudTrainWake;
class USoundBase;
class UMaterialInterface;

/**
 * One train of the sea railway: two old cream-and-green cars of a Showa-era local line (blender/railway/train.py,
 * /Game/Railway/Meshes/rail_kit): round headlights, a pantograph on the wire, long benches along the walls, warm
 * lamps in the windows at night. ULyudRailwaySubsystem moves it by the timetable; it bends its two cars along the
 * line, opens its doors at the stops (both sides' leaves slide), sings its motor and clacks over the joints, pushes a
 * wake and spray off its wheels where the rails are under the water (ULyudTrainWake) and draws a few gulls after it
 * over the sea.
 *
 * Riders: the player walks in through an open door and is carried with the car (like the cable car's cabins); people
 * (UPeopleLifeSubsystem) are put on its seats: GetSeat gives a seat's world frame, origin on the floor under the
 * front edge of the seat, X the way the sitter faces (across the car: the benches run along the walls), Z up; the
 * seat's top is SeatHeight over the floor.
 */
UCLASS()
class LYUDMILOVSK_API ALyudTrain : public AActor
{
	GENERATED_BODY()

public:
	ALyudTrain();

	/** Builds the cars (from the kit, or plain boxes if it is not imported yet). */
	void Setup(ULyudRailwaySubsystem* InRailway, int32 InIndex);
	/** Puts it where the timetable says (called by the subsystem every frame). */
	void Place(const FLyudTrainInfo& Info, float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = "Train") int32 GetIndex() const { return Index; }
	UFUNCTION(BlueprintPure, Category = "Train") FLyudTrainInfo GetInfo() const { return Info; }

	UFUNCTION(BlueprintPure, Category = "Train") int32 NumSeats() const { return Seats.Num(); }
	/** A seat's world frame (see the class comment); false if there is no such seat. */
	UFUNCTION(BlueprintPure, Category = "Train") bool GetSeat(int32 Seat, FTransform& Out) const;
	/** Which car a seat is in (0 front outbound). */
	UFUNCTION(BlueprintPure, Category = "Train") int32 GetSeatCar(int32 Seat) const;
	/** Doorways (both sides): the sill's outside middle (world cm) and whether it faces the platform side. */
	UFUNCTION(BlueprintPure, Category = "Train") int32 NumDoors() const { return Doors.Num(); }
	UFUNCTION(BlueprintPure, Category = "Train") FVector GetDoorPoint(int32 Door) const;
	/** Is a world point inside one of the cars (for the player's ride)? */
	UFUNCTION(BlueprintPure, Category = "Train") bool IsInside(const FVector& Location) const;

	/** The water's answer (tests read its wetness and particles). */
	UFUNCTION(BlueprintPure, Category = "Train") ULyudTrainWake* GetWake() const { return Wake; }
	UFUNCTION(BlueprintPure, Category = "Train") int32 NumGulls() const { return Gulls.Num(); }

	/** cm over the floor. */
	static constexpr float SeatHeight = 42.f;

	virtual void Tick(float DeltaSeconds) override;

	struct FCar
	{
		TObjectPtr<USceneComponent> Root;
		double S = 0.0;    // its middle along the line
	};

private:
	void BuildCar(int32 C);
	void CarryPlayer(const TArray<FTransform>& Old);
	void UpdateLights();
	void UpdateDoors(float DeltaSeconds);
	void UpdateSound(float DeltaSeconds, const FLyudTrainInfo& Prev);
	void UpdateGulls(float DeltaSeconds);
	FTransform CarFrame(double S) const;

	UPROPERTY(Transient) TObjectPtr<ULyudRailwaySubsystem> Railway;
	int32 Index = 0;
	FLyudTrainInfo Info;
	TArray<FCar> Cars;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> CarRoots;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Bodies;
	struct FDoor { int32 Car = 0; double X = 0.0; int32 Side = 1; TWeakObjectPtr<UStaticMeshComponent> LeafA, LeafB; };
	TArray<FDoor> Doors;
	float DoorOpen = 0.f;          // 0 shut .. 1 open
	bool bDoorsInit = true;
	struct FSeat { int32 Car = 0; FTransform Local; };
	TArray<FSeat> Seats;
	struct FCarMats { TObjectPtr<UMaterialInstanceDynamic> Lamp, Head, Tail, Sign; };
	TArray<FCarMats> CarMats;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> KeepMats;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UMaterialInterface>> Mats;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<USoundBase>> Sounds;
	UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> GullSounds;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> CabinLights;
	UPROPERTY(Transient) TArray<TObjectPtr<USpotLightComponent>> HeadLights;
	UPROPERTY(Transient) TObjectPtr<class USoundAttenuation> RunAtten;
	UPROPERTY(Transient) TObjectPtr<class USoundAttenuation> JointAtten;
	UPROPERTY(Transient) TObjectPtr<class USoundAttenuation> HornAtten;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Motor;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Wheels;
	UPROPERTY(Transient) TObjectPtr<ULyudTrainWake> Wake;
	UPROPERTY(Transient) TArray<TObjectPtr<UBoxComponent>> Floors;
	struct FGull { TWeakObjectPtr<class ABirdActor> Bird; FVector Vel = FVector::ZeroVector; FVector Offset = FVector::ZeroVector; float Phase = 0.f; float NextCall = 5.f; bool bFlap = false; };
	TArray<FGull> Gulls;
	float GullAway = 0.f;
	TWeakObjectPtr<AActor> Sky;
	double LastS = 0.0;
	bool bWasVisible = true;
	TWeakObjectPtr<APawn> Rider;
	TWeakObjectPtr<APawn> TickOrderFor;    // the character made to tick after this train
public:
	/** Length of a car (m) and the gap between two (m). */
	double CarLength = 16.0;
	double CarGap = 0.9;
	int32 NumCars = 2;
};
