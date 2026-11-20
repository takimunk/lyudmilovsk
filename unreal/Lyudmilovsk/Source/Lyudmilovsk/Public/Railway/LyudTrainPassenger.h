#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "LyudTrainPassenger.generated.h"

namespace LyudTicket { struct FModel; }
class AActor;
class ANpcBody;
class SLyudCityPage;
class UMaterialInstanceDynamic;

/**
 * Riding the sea railway as a passenger (docs/railway.md "Riding as a passenger", backlog item 21):
 *  - a ticket box on each halt's platform (built in code from the public spaces' kit, M_CityProp): E buys a ticket
 *    (rail_ticket, the fare of Content/Railway/passenger.json);
 *  - the day's conductor (one of three, each with a manner and lines) walks down the car once the doors have shut, stops by
 *    the hero and checks: a ticket is punched (it becomes rail_ticket_punched), none earns a stern word or the fine
 *    (RailTicketModel.h decides);
 *  - the announcements in the car: the next stop on leaving, the doors, the halt on arriving, the end of the line.
 * Only for a hero inside a car; nothing ticks for anyone else. The conductor is a crowd body (ANpcBody, a shared look as the
 * townsfolk's); without the model the conductor is a voice in the subtitles.
 *
 * Console: lyud.Rail.Conductor (the conductor to the hero now), lyud.Rail.Passenger (state). Dev action rail.conductor.
 */
UCLASS()
class LYUDMILOVSK_API ULyudTrainPassenger : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudTrainPassenger();
	virtual ~ULyudTrainPassenger() override;

	static ULyudTrainPassenger* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudTrainPassenger, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bWorld; }

	/** Sends the conductor down the car to the hero now (he must be inside a car). */
	bool CallConductor();
	FString Report() const;

	/** Rides without a ticket so far (each one makes the next check stricter), and tickets punched. Saved. */
	UPROPERTY(SaveGame) int32 Offences = 0;
	UPROPERTY(SaveGame) int32 Punched = 0;

private:
	bool LoadModel();
	void BuildBoxes();
	void TickBoxes(class AHeroCharacter* Hero, class APlayerController* PC);
	void TickTrain(class AHeroCharacter* Hero, float DeltaTime);
	void TickConductor(class AHeroCharacter* Hero, float DeltaTime);
	void StartConductor();
	void EndConductor();
	void Say(const FText& Title, const FText& Line, float Seconds);
	void Unsay();
	FText StopName(int32 Stop) const;
	int32 NextStop(int32 Stop, int32 Dir) const;
	float AlongOf(const FVector& World) const;

	TPimplPtr<LyudTicket::FModel> Model;
	bool bLoaded = false;
	bool bWorld = false;
	bool bBoxesBuilt = false;
	double NextBoxTry = 0.0;

	struct FBox { FVector Front = FVector::ZeroVector; FVector Facing = FVector::ForwardVector; };
	TArray<FBox> Boxes;
	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> BoxActors;
	int32 PromptBox = -1;

	// the hero's ride: which train, its last state
	int32 RideTrain = -1;
	int32 LastStop = -2;
	bool bLastDoors = false;
	int32 LastDir = 1;
	bool bPunchedThisRide = false;
	bool bCheckedThisRide = false;
	double ConductorAt = -1.0;          // world seconds when the conductor sets off (-1 none due)

	// the conductor walking: offsets along the line from the train's reference point (cm)
	enum class EWalk : uint8 { None, Coming, Asking, Answer, Leaving } Walk = EWalk::None;
	float WalkD = 0.f, WalkFrom = 0.f, WalkTo = 0.f, WalkLo = 0.f, WalkHi = 0.f;
	bool bFromHi = true;
	double PhaseUntil = 0.0;
	int32 Conductor = -1;
	UPROPERTY(Transient) TObjectPtr<ANpcBody> Body;

	TSharedPtr<SLyudCityPage> Page;
	double PageUntil = 0.0;
};
