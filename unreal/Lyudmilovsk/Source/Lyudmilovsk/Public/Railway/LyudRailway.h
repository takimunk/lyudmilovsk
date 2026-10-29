#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "LyudRailway.generated.h"

namespace LyudRail { class FRun; }
class ALyudTrain;
class ALyudRailTrack;
class ALyudLevelCrossing;

/** Where a train is (LyudRail::FTrainState for Blueprints and Python). */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudTrainInfo
{
	GENERATED_BODY()

	/** Metres along the line from the tunnel's dark end (its middle). */
	UPROPERTY(BlueprintReadOnly, Category = "Railway") float S = 0.f;
	/** m/s, + outbound (to sea), - inbound. */
	UPROPERTY(BlueprintReadOnly, Category = "Railway") float Speed = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Railway") int32 Dir = 1;
	/** The stop it stands at (index), -1 while moving. */
	UPROPERTY(BlueprintReadOnly, Category = "Railway") int32 Stop = -1;
	UPROPERTY(BlueprintReadOnly, Category = "Railway") bool bDoorsOpen = false;
	UPROPERTY(BlueprintReadOnly, Category = "Railway") bool bVisible = false;
	UPROPERTY(BlueprintReadOnly, Category = "Railway") bool bInService = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FLyudTrainEvent, FName, Event, int32, Train, int32, Stop);

/**
 * The sea railway (docs/railway.md): a single track from a tunnel in the sea cliff south of the club, over the beach
 * path's level crossing, past the beach halt and out along a low bank in the sea (the rails a hand's breadth under the
 * water at high tide) to the island halt 2.3 km off, and on to the horizon. One train of two cars runs it by the
 * timetable (Private/Railway/RailRun.h) on the game calendar: out of the tunnel every hour, into the haze, and back.
 *
 * Starts by itself in a level with the outdoor map (Lvl_Scene01; lyud.Railway.AnyMap 1 elsewhere): reads
 * Content/Railway/railway_built.json (tools/railway.py), builds the track (ALyudRailTrack), the crossing
 * (ALyudLevelCrossing) and the trains (ALyudTrain). lyud.Railway 0 turns it all off.
 *
 * People (UPeopleLifeSubsystem) ride it: they wait at a halt's boarding point until a train's doors open
 * (NextDoorsOpen), sit on its seats (ALyudTrain::GetSeat) for the ride and get off at the other halt.
 */
UCLASS()
class LYUDMILOVSK_API ULyudRailwaySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudRailwaySubsystem();
	virtual ~ULyudRailwaySubsystem() override;

	static ULyudRailwaySubsystem* Get(const UObject* WorldContext);
	/** For Blueprints and Python: unreal.LyudRailwaySubsystem.get_railway(world). */
	UFUNCTION(BlueprintPure, Category = "Railway", meta = (WorldContext = "WorldContext"))
	static ULyudRailwaySubsystem* GetRailway(const UObject* WorldContext) { return Get(WorldContext); }

	UFUNCTION(BlueprintPure, Category = "Railway") bool IsRunning() const { return bStarted; }

	/** The timetable clock: seconds of the game calendar (whole days counted), what the timetable runs on. */
	UFUNCTION(BlueprintPure, Category = "Railway") double NowSeconds() const;
	/** The timetable (null before it starts). */
	const LyudRail::FRun* GetRun() const { return Run.Get(); }

	UFUNCTION(BlueprintPure, Category = "Railway") int32 NumStops() const;
	UFUNCTION(BlueprintPure, Category = "Railway") FName GetStopId(int32 Stop) const;
	UFUNCTION(BlueprintPure, Category = "Railway") int32 FindStop(FName Id) const;
	/** rail.stop.<id> in the strings. */
	UFUNCTION(BlueprintPure, Category = "Railway") FString GetStopNameKey(int32 Stop) const;
	/** Where to wait on a halt's platform for the doors (world cm, on the deck), Slot spreads waiting people along it. */
	UFUNCTION(BlueprintPure, Category = "Railway") FVector GetBoardingPoint(int32 Stop, int32 Slot = 0) const;
	/** The foot of a halt's stair on the ground (world cm): where someone walking there arrives. */
	UFUNCTION(BlueprintPure, Category = "Railway") FVector GetStopEntrance(int32 Stop) const;
	/** The same in map metres (X west, Y north, Z up), for the people's map (tools/people_map.py reads the json instead). */
	UFUNCTION(BlueprintPure, Category = "Railway") FVector GetStopEntranceMap(int32 Stop) const;

	/** The next moment (NowSeconds' clock) a train's doors open at Stop heading Dir (+1 out to sea, -1 back); -1 never. */
	UFUNCTION(BlueprintPure, Category = "Railway") double NextDoorsOpen(int32 Stop, int32 Dir, double AfterSeconds) const;
	/** Seconds from the doors opening at A to their opening at B (B further along Dir). */
	UFUNCTION(BlueprintPure, Category = "Railway") double RideSeconds(int32 StopA, int32 StopB, int32 Dir) const;

	UFUNCTION(BlueprintPure, Category = "Railway") int32 NumTrains() const { return Trains.Num(); }
	UFUNCTION(BlueprintPure, Category = "Railway") ALyudTrain* GetTrain(int32 Index = 0) const;
	UFUNCTION(BlueprintPure, Category = "Railway") ALyudLevelCrossing* GetCrossing() const { return Crossing; }
	UFUNCTION(BlueprintPure, Category = "Railway") ALyudRailTrack* GetTrack() const { return Track; }

	/** A point of the line: S metres along it, Side metres to its left (outbound), Up metres over the rail head; world cm. */
	UFUNCTION(BlueprintPure, Category = "Railway") FVector LineToWorld(double S, double Side = 0.0, double Up = 0.0) const;
	/** The line's heading at S (outbound, unit, world). */
	FVector LineDir(double S) const;
	/** Rail head Z at S (world cm) and the ground/sea bed under it. */
	double RailZ(double S) const;
	double GroundZ(double S) const;
	/** Map metres -> world cm, and back. */
	FVector MapToWorld(double X, double Y, double Z) const;
	UFUNCTION(BlueprintPure, Category = "Railway") FVector WorldToMap(const FVector& World) const { return FVector(World.X / 100.0, (World.Y - PortalFaceY) / 100.0, World.Z / 100.0); }
	double LineLength() const;

	/** Depart, Arrive, DoorsOpen, DoorsClose, Appear (out of the tunnel's dark or the haze), Vanish. */
	UPROPERTY(BlueprintAssignable, Category = "Railway") FLyudTrainEvent OnTrainEvent;

	/** One line per train: where, how fast, next stop (console lyud.Railway.Report). */
	UFUNCTION(BlueprintCallable, Category = "Railway") FString Report() const;
	/** Tests: pushes the timetable's clock by Seconds (only the railway's; the calendar is untouched). */
	UFUNCTION(BlueprintCallable, Category = "Railway") void SetClockOffset(double Seconds) { ClockOffset = Seconds; }
	UFUNCTION(BlueprintPure, Category = "Railway") double GetClockOffset() const { return ClockOffset; }

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bStarted; }

private:
	bool Start();
	TPimplPtr<LyudRail::FRun> Run;
	bool bStarted = false;
	double ClockOffset = 0.0;
	double PortalFaceY = 0.0;          // world cm of the map's Y = 0
	struct FPoint { double S, X, Y, Rail, Ground; };
	TArray<FPoint> Points;
	double X0 = 0.0, Step = 4.0, Length = 0.0, LineY = 0.0;
	struct FStopData { FName Id; FString NameKey; double S = 0.0; int32 Side = -1; double P0 = 0.0, P1 = 0.0; FVector EntranceMap = FVector::ZeroVector; };
	TArray<FStopData> Stops;
	UPROPERTY(Transient) TArray<TObjectPtr<ALyudTrain>> Trains;
	UPROPERTY(Transient) TObjectPtr<ALyudRailTrack> Track;
	UPROPERTY(Transient) TObjectPtr<ALyudLevelCrossing> Crossing;
	TArray<FLyudTrainInfo> LastInfo;
	friend class ALyudTrain;
	friend class ALyudRailTrack;
	friend class ALyudLevelCrossing;
public:
	/** The built data (railway_built.json) as JSON text, for the actors that build from it. */
	FString BuiltJson;
	/** Platform deck height over the rail head, m; the cars' floor is at FloorOverRail. */
	static constexpr double PlatformOverRail = 1.10;
	static constexpr double FloorOverRail = 1.15;
	/** The platform's edge from the track's centreline, m, and its width. */
	static constexpr double PlatformEdge = 1.55;
	static constexpr double PlatformWidth = 3.2;
};
