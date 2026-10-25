#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "LyudTransport.generated.h"

namespace LyudTransport { class FNetwork; }
class ALyudCableway;
class ALyudCabin;
class SLyudTransitScreen;

/** A station as the maps and boards show it. */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudTransitStation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Transport") FName Id;
	/** transport.station.<id> in the strings. */
	UPROPERTY(BlueprintReadOnly, Category = "Transport") FString NameKey;
	/** The map's metres (X west, Y north). */
	UPROPERTY(BlueprintReadOnly, Category = "Transport") FVector2D MapAt = FVector2D::ZeroVector;
	/** Where it sits on the route diagram (grid units). */
	UPROPERTY(BlueprintReadOnly, Category = "Transport") FVector2D Schema = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Transport") TArray<FName> Lines;
	/** The ground door of its lift and stair (world cm), where people go in. */
	UPROPERTY(BlueprintReadOnly, Category = "Transport") FVector Entrance = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudTransitLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Transport") FName Id;
	UPROPERTY(BlueprintReadOnly, Category = "Transport") FString NameKey;
	UPROPERTY(BlueprintReadOnly, Category = "Transport") int32 Number = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Transport") FLinearColor Color = FLinearColor::White;
	/** Station indices, in order. */
	UPROPERTY(BlueprintReadOnly, Category = "Transport") TArray<int32> Stations;
	/** The rope from the first station to the last, map metres (for the map). */
	UPROPERTY(BlueprintReadOnly, Category = "Transport") TArray<FVector2D> Path;
	UPROPERTY(BlueprintReadOnly, Category = "Transport") int32 Depot = -1;
};

USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudTransitCabin
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Transport") FName Id;
	UPROPERTY(BlueprintReadOnly, Category = "Transport") int32 Number = 0;
	/** transport.cabin.<id> in the strings: the cabin's own name. */
	UPROPERTY(BlueprintReadOnly, Category = "Transport") FString NameKey;
	UPROPERTY(BlueprintReadOnly, Category = "Transport") int32 Line = -1;
	UPROPERTY(BlueprintReadOnly, Category = "Transport") FVector2D MapAt = FVector2D::ZeroVector;
	/** depot, moving, dwelling, idle. */
	UPROPERTY(BlueprintReadOnly, Category = "Transport") FName State;
	UPROPERTY(BlueprintReadOnly, Category = "Transport") int32 Riders = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Transport") int32 NextStation = -1;
	UPROPERTY(BlueprintReadOnly, Category = "Transport") int32 Destination = -1;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FLyudTransportEvent, FName, Event, int32, Cabin, int32, Station, int32, Who);

/**
 * The cable-car metro (docs/transport.md): seven stations, two lines, cabins that run when somebody needs them.
 * Starts by itself in a level that has the outdoor map (Lvl_Scene01): reads Content/Transport/network_built.json
 * (tools/transport.py made it from network.json and the terrain), builds the stations, pylons and ropes
 * (ALyudCableway) and the cabins (ALyudCabin), and runs the network (Private/Transport/CablewaySim.h).
 *
 * The player: at a station's call panel (E) the route diagram opens; pick where to go, a cabin is sent, the board
 * says how long to wait; walk in when its doors open, walk out at the stop. People of UPeopleLifeSubsystem ride
 * it too: their ride between the valley and the club's spur waits for a real cabin. The route board by the club's
 * exit (E) shows the diagram with the cabins live. lyud.Transport 0 turns it all off.
 */
UCLASS()
class LYUDMILOVSK_API ULyudTransportSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudTransportSubsystem();
	virtual ~ULyudTransportSubsystem() override;

	static ULyudTransportSubsystem* Get(const UObject* WorldContext);
	UFUNCTION(BlueprintPure, Category = "Transport", meta = (WorldContext = "WorldContext"))
	static ULyudTransportSubsystem* GetTransport(const UObject* WorldContext) { return Get(WorldContext); }

	UFUNCTION(BlueprintPure, Category = "Transport") bool IsRunning() const { return Net.IsValid(); }

	UFUNCTION(BlueprintCallable, Category = "Transport") TArray<FLyudTransitStation> GetStations() const;
	UFUNCTION(BlueprintCallable, Category = "Transport") TArray<FLyudTransitLine> GetLines() const;
	UFUNCTION(BlueprintCallable, Category = "Transport") TArray<FLyudTransitCabin> GetCabins() const;
	UFUNCTION(BlueprintPure, Category = "Transport") int32 FindStation(FName Id) const;
	UFUNCTION(BlueprintCallable, Category = "Transport") FText StationName(int32 Station) const;

	/** The player asks for a trip from station From to To (the call panel, the diagram). False if there is no way. */
	UFUNCTION(BlueprintCallable, Category = "Transport") bool CallCabin(int32 From, int32 To);
	/** Seconds till the next cabin of Line stops at Station (-1 none); bCalled: one is on its way (else: if called now). */
	UFUNCTION(BlueprintCallable, Category = "Transport") float WaitAt(int32 Station, int32 Line, bool& bCalled) const;
	/** The station the player stands at (its platform, lift or foot), -1 none. */
	UFUNCTION(BlueprintPure, Category = "Transport") int32 PlayerStation() const;
	/** The cabin the player is in, -1 none. */
	UFUNCTION(BlueprintPure, Category = "Transport") int32 PlayerCabin() const { return RidingCabin; }
	/** The player's trip: from, to (-1 none). */
	UFUNCTION(BlueprintPure, Category = "Transport") bool PlayerTrip(int32& From, int32& To) const;

	/** Opens the route diagram full screen; FromStation >= 0: the call panel of that station (pick where to go). */
	UFUNCTION(BlueprintCallable, Category = "Transport") void OpenDiagram(int32 FromStation = -1);
	UFUNCTION(BlueprintCallable, Category = "Transport") void CloseDiagram();
	UFUNCTION(BlueprintPure, Category = "Transport") bool IsDiagramOpen() const { return Screen.IsValid(); }

	/** Map metres <-> world cm (the outdoor map's frame). */
	FVector MapToWorld(float X, float Y, float Z) const;
	FVector2D WorldToMap(const FVector& P) const;

	/** People's rides (UPeopleLifeSubsystem): a person at world point From wants to get to the station nearest To.
	 *  True if a cabin will carry them (EndRide then comes from here); false: no network, they ride the old way. */
	bool BeginRide(int32 Person, const FVector& From, const FVector& To);
	/** Where to draw a rider now: false if not riding; bSeated in a cabin (Location is the seat), else waiting on a platform. */
	bool RiderPose(int32 Person, FVector& Location, float& Yaw, bool& bSeated) const;

	/** Launch, Arrive, Depart, Board, Alight, Done, Park. */
	UPROPERTY(BlueprintAssignable, Category = "Transport") FLyudTransportEvent OnTransportEvent;

	/** One line per cabin (Transport.Report). */
	UFUNCTION(BlueprintCallable, Category = "Transport") FString Report() const;
	/** Test helper: puts the player on From's platform and asks for To (Transport.Ride). */
	UFUNCTION(BlueprintCallable, Category = "Transport") bool TestRide(FName From, FName To);

	LyudTransport::FNetwork* GetNet() const { return Net.Get(); }
	ALyudCableway* GetCableway() const { return Cableway; }
	const TArray<TObjectPtr<ALyudCabin>>& GetCabinActors() const { return Cabins; }

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return Net.IsValid(); }

private:
	TPimplPtr<LyudTransport::FNetwork> Net;
	UPROPERTY(Transient) TObjectPtr<ALyudCableway> Cableway;
	UPROPERTY(Transient) TArray<TObjectPtr<ALyudCabin>> Cabins;
	TSharedPtr<SLyudTransitScreen> Screen;
	int32 ScreenStation = -1;
	int32 RidingCabin = -1;
	/** Person id -> rider index of their trip. */
	TMap<int32, int32> PeopleRides;
	float PromptCooldown = 0.f;

	bool Start(UWorld& World);
	void UpdatePlayer(float DeltaTime);
	void HandleEvents();
	void Announce(int32 Cabin, FName What, int32 Station);
	/** Moves the player aboard with their cabin from Before to After. */
	void Carry(const FTransform& Before, const FTransform& After, AActor* Cabin);
};
