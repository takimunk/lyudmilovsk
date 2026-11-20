#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyudStationHall.generated.h"

class ALyudCableway;
class ANpcBody;
class UPointLightComponent;
class UStaticMeshComponent;
class FJsonObject;

/**
 * A cable-car station's hall, v3 (docs/transport.md "Halls v3"; Dima 2026-11-20: the halls were crude, dark and empty, a cafe
 * with nobody to buy from). Content/Transport/halls.json (blender/transport/halls.py) says, per vestibule style in the station
 * frame: the dressing (a town-kit building, /Game/City/Buildings/tr_hall__<style>: the furniture and its hidden collision
 * cores), the lamps, the staff's places (the barista behind the buffet's counter, the station attendant at her desk), the seats
 * and standing spots for waiting passengers, the counter. This actor places all of it at the station and keeps it alive only
 * near the player:
 *  - the lamps (warm, shadowless) are on while the player is within LightsOn of the hall;
 *  - the people (ANpcBody, the town's models) exist only within PeopleOn: the barista and the attendant 6:00..24:00, and a few
 *    passengers by the hour (morning and evening most), on the seats (some reading or eating at the cafe's tables) and at the
 *    railing, the windows, the diagram; now and then one leaves and another comes, never in the player's sight;
 *  - T at the barista opens the buffet (lobby.json stock "buffet"); E at the attendant opens the route diagram; both greet
 *    the player once a visit (line.transport.barista|attendant.*, over their heads).
 * Console: Transport.Hall [station] (report), lyud.Transport.HallPeople 0/1.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudStationHall : public AActor
{
	GENERATED_BODY()

public:
	ALyudStationHall();
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** The hall's record of halls.json for this style (null: the style has none). */
	static TSharedPtr<FJsonObject> HallData(const FString& Style);
	/** Is the hall's dressing imported (its detail mesh loads)? Then the old furniture (kiosk, bench, planters, lamps) stays out. */
	static bool HasDressing(const FString& Style);

	/** Builds the hall at station S in frame C (the vestibule's station frame, world). */
	void Setup(ALyudCableway* Way, int32 InStation, const FString& InStyle, const FTransform& C, const TSharedPtr<FJsonObject>& Hall);

	/** Report for the console and tests. */
	FString Report() const;

	int32 Station = -1;
	FString Style;

private:
	struct FSlot
	{
		FVector At = FVector::ZeroVector;   // world, the floor under the person (seated: under the seat)
		float Yaw = 0.f;                    // world yaw the person faces
		float SeatHeight = 0.f;             // cm, 0 standing
		uint8 Kind = 0;                     // 0 seat, 1 table seat, 2 counter, 3 rail, 4 window, 5 diagram, 6 read, 7 stand table
		int32 Who = -1;                     // index into People, -1 free
	};
	struct FPerson
	{
		TWeakObjectPtr<ANpcBody> Body;
		int32 Slot = -1;                    // passengers: their slot; staff: -1
		uint8 Pose = 0;                     // EPersonPose as a number
		FVector At = FVector::ZeroVector;
		float Yaw = 0.f;
		float SeatHeight = 0.f;
		FName Role;                         // barista, attendant, passenger
		FString Model;
		bool bGreeted = false;
	};

	UPROPERTY(Transient) TObjectPtr<ALyudCableway> Way;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Detail;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Cores;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Lights;
	UPROPERTY(Transient) TArray<TObjectPtr<ANpcBody>> Bodies;

	FTransform Frame;
	FVector Middle = FVector::ZeroVector;
	TArray<FSlot> Slots;
	TArray<FPerson> People;
	TArray<FPerson> StaffPlan;              // barista, attendant: where they stand (not spawned)
	FVector CounterAt = FVector::ZeroVector;
	FString Shop = TEXT("buffet");
	bool bLightsOn = false;
	bool bPeopleOn = false;
	float ChurnTimer = 30.f;
	float Clock = 0.f;
	int32 Prompted = -1;                    // 0 barista (T), 1 attendant (E)
	uint32 Seed = 0;

	void SetLights(bool bOn);
	void SpawnPeople();
	void ClearPeople();
	bool SpawnPerson(FPerson& P);
	void Churn(const FVector& Cam, const FVector& Look);
	int32 WantPassengers() const;
	bool StaffOnDuty() const;
	void TickPrompt(float DeltaTime);
	FString PickModel(int32 Salt) const;
};
