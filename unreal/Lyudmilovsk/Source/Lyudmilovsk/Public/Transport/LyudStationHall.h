#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyudStationHall.generated.h"

class ALyudCableway;
class ANpcBody;
class UPointLightComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class FJsonObject;
class SLyudFlapBoard;
namespace LyudTransport { struct FHallMood; }

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
 * With the station life (docs/transport.md "Station life", ULyudTransportSubsystem::GetLife()): the staff by station_life.json's
 * rota (their own faces, the hall shut without an attendant), the riders waiting in it or walking out of it, a regular greeted
 * by the attendant, the waiting people's words (the wind, the stop, the hour).
 * The club's front door (docs/front_door.md, ULyudFrontDoor): the barista serves through the counter service (registered
 * with ULyudCounterService, the shop's counter "hall:<station>"), a line forms at the counter in the morning rush (spots
 * behind its customer place, found clear of the furniture; the hero standing in it holds his spot, the rest step up), a
 * regular's usual is E at the barista; a punch on the attendant's desk stamps the travel card; at Aton the attendant gives
 * the card, a split-flap board over the desk shows the next cabins (SLyudFlapBoard, a clack when its rows turn) and the lost
 * and found shelf stands by the desk. At dusk the lamps are seen from across the forecourt.
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
		uint8 Kind = 0;                     // 0 seat, 1 table seat, 2 counter, 3 rail, 4 window, 5 diagram, 6 read, 7 stand table,
		                                    // 8 a place in the counter's line behind the counter place
		int32 Who = -1;                     // index into People, -1 free
		int32 Line = -1;                    // its place in the counter's line (0 the counter place itself), -1 not in it
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
		int32 Run = -1;                     // the station life's run (ULyudTransportSubsystem::GetLife()->Today), -1: none
		int32 RunDay = -1;                  // the station life's Day that Run indexes (a new day makes Today again)
		int32 RunTrip = -1;                 // and that run's trip (the same index on another day is someone else)
		int32 Pid = -1;                     // their id in the town's life (shown here: the town doesn't draw them), -1
		int32 StaffIndex = -1;              // staff: their place in station_life.json's rota
		float TalkLeft = 0.f;               // the attendant turned to a regular for a word
		float BaseYaw = 0.f;
		uint8 BasePose = 0;
		double GoneSince = -1.0;            // their run left the hall at (the body goes once unseen)
		int32 Age = 35;
		float NextRoutine = 0.f;            // staff: the hall's clock of their next small change (docs/transport.md "Hall moods")
		// stepping up in the line, or off to a table with the cup (walking from WalkFrom to At)
		bool bWalking = false;
		FVector WalkFrom = FVector::ZeroVector;
		float WalkT = 0.f;
		uint8 WalkPose = 0;                 // the pose once there
		double FrontSince = -1.0;           // at the counter since (the barista serves them in a minute or so)
		bool bServed = false;
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
	bool bDutyAtSpawn = false;                // were the staff on duty when the people were last made (SpawnPeople)
	float ChurnTimer = 30.f;
	float Clock = 0.f;
	int32 Prompted = -1;                    // 0 barista (T; E the usual), 1 attendant (E), 2 the punch, 3 the lost and found shelf,
	                                        // 4 the bell on a shut hall's desk (E)
	FVector BellAt = FVector::ZeroVector;
	float BellQuiet = 0.f;
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
	/** The station life (docs/transport.md "Station life"): the riders now in this hall in and out, the staff by the rota,
	 *  the waiting people's words. */
	void SyncLife(const FVector& Cam, const FVector& Look);
	void TickBarks(float DeltaTime, const FVector& Cam);
	bool LifeOpen() const;
	FString LifeModel(const FString& Key, const FString& Mesh, int32& OutPid) const;
	void Unpresent(FPerson& P);
	/** The hour, the day, the weather and the buffet as the waiting people feel them (StationLife.h FHallMood). */
	LyudTransport::FHallMood Mood() const;
	/** A free place out of the player's sight for a person (Key, Age; walking out: bOut) by the mood, -1 none (the
	 *  counter's line behind the counter place fills by the line itself, never here). */
	int32 PickSlot(const FString& Key, int32 Age, bool bOut, const TFunctionRef<bool(const FVector&)>& Unseen, uint32 Salt) const;
	/** Their pose at slot K by the mood (Roll 0..1, theirs). */
	uint8 PoseAt(int32 K, int32 Age, float Roll) const;
	/** The barista and the attendant's small routines: working the counter, the logbook, a look at who comes in. */
	void TickStaff();
	float LifeTimer = 0.f;
	float BarkTimer = 20.f;
	TSet<int32> GreetedRegulars;
	int32 GreetedDay = -1;                  // the station life's day GreetedRegulars is of

	// --- the club's front door (docs/front_door.md)
	/** The counter's line: Slots indices, front (the counter place) first. */
	TArray<int32> LineSlots;
	FString CounterKey;
	FVector PunchAt = FVector::ZeroVector;
	FVector ShelfAt = FVector::ZeroVector;
	FVector BoardAt = FVector::ZeroVector;
	bool bFront = false;                    // this hall is the front door (Aton): the card, the board, the shelf
	bool bFrontBuilt = false;               // the line, the punch (and at Aton the board and the shelf) are placed
	UPROPERTY(Transient) TObjectPtr<UWidgetComponent> Board;
	TSharedPtr<SLyudFlapBoard> FlapBoard;
	int32 BoardFlips = 0;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Props;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> ShelfThings;
	int32 ShelfShown = -1;
	float FrontClock = 0.f;
	double NextUsualBark = 0.0;
	void BuildLine();
	void BuildFrontDoor();
	UStaticMeshComponent* PropBox(const FVector& At, const FRotator& R, const FVector& SizeCm, const FString& Paint, bool bCollide);
	bool Clear(const FVector& Floor, float Radius) const;
	bool HeroHolds(const FVector& At) const;
	void LineState(TArray<int32>& Out) const;
	void StepLine(const FVector& Cam, const FVector& Look);
	void TickWalks(float DeltaTime);
	void TickFront(float DeltaTime, const FVector& Cam, const FVector& Look);
	void ShowShelf();
	void StartWalk(FPerson& P, const FVector& To, float Yaw, uint8 Pose);
	int32 PersonIndexAt(int32 Slot) const;
};
