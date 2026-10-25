#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyudCableway.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UWidgetComponent;
class UBoxComponent;
class UPointLightComponent;
class UAudioComponent;
class ULyudTransportSubsystem;
namespace LyudTransport { class FNetwork; }

/** One station's parts the game needs again after building (platform frames, lift, call panel). */
USTRUCT()
struct FLyudBuiltStation
{
	GENERATED_BODY()

	/** Per level (line): the platform's frame: origin on the deck at the station's centre, X along the line. */
	TArray<FTransform> Decks;
	TArray<int32> DeckLines;
	/** The foot of the lift / stair (world cm), the way into the station. */
	FVector Entrance = FVector::ZeroVector;
	/** Where the call panel and the board stand (world), per level. */
	TArray<FTransform> Panels;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidgetComponent>> Boards;
	UPROPERTY(Transient) TObjectPtr<class ALyudStationLift> Lift;
	/** The door into the lift at the ground. */
	UPROPERTY(Transient) TObjectPtr<class ALyudDoor> Door;
	float BoardTimer = 0.f;
};

/**
 * Everything of the cable-car metro that stands still, built in code from the network (ULyudTransportSubsystem):
 * stations (a tower, the island platform between the two tracks under a crown roof, a glass lift and a square
 * spiral stair down to the ground; Aton right on its hill), pylons (line and corner ones, as tall as the ground
 * asks), the ropes, the depots. All heights come from network_built.json (tools/transport.py), so a changed
 * relief only needs that file made again. Meshes: /Game/Transport/Meshes (the hi3d models, optimized in Blender,
 * and simple kit parts); missing ones fall back to the engine's basic shapes.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudCableway : public AActor
{
	GENERATED_BODY()

public:
	ALyudCableway();

	void Build(ULyudTransportSubsystem* Owner, const LyudTransport::FNetwork& Net);
	/** The boards' words (wait times) and the lifts, a few times a second. */
	void UpdateBoards(const LyudTransport::FNetwork& Net, const FVector& Viewer, float DeltaTime);

	TArray<FLyudBuiltStation> Built;

	/** Where a waiting person stands on station S's platform for line L (spot K of a few). */
	FTransform WaitSpot(int32 Station, int32 Line, int32 K) const;
	/** The call panel of station S nearest to P within R cm: its index into Built[S].Panels, -1 none. */
	int32 PanelNear(int32 Station, const FVector& P, float R) const;
	/** The station whose structure P is on or at the foot of (within its footprint), -1 none. */
	int32 StationAt(const FVector& P) const;

	UMaterialInterface* Mat(FName Name);

private:
	UPROPERTY(Transient) TObjectPtr<ULyudTransportSubsystem> Transport;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> Ism;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UMaterialInterface>> Mats;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Singles;
	TArray<FVector> StationCentres;
	TArray<float> StationRadius;
	UPROPERTY(Transient) TObjectPtr<UWidgetComponent> ClubBoard;
	FTransform ClubBoardAt;

public:
	/** The route board by the club's exit (where it stands), for the E prompt. */
	bool ClubBoardNear(const FVector& P, float R) const { return ClubBoard && FVector::Dist(ClubBoardAt.GetLocation() + FVector(0, 0, 150.f), P) < R; }

private:

	/** An instance of a kit mesh (Kind: box, cyl, rope, step, ... or a /Game path) in material Mat. */
	void Add(FName Kind, FName MatName, const FTransform& T, bool bCollide = true);
	UInstancedStaticMeshComponent* IsmFor(FName Kind, FName MatName, bool bCollide);
	UStaticMeshComponent* Single(UStaticMesh* Mesh, const FTransform& T, bool bCollide, FName MatName = NAME_None);
	/** A box of size (cm) centred at C in frame F (local cm). */
	void Box(const FTransform& F, const FVector& C, const FVector& Size, FName MatName, bool bCollide = true, const FRotator& R = FRotator::ZeroRotator);
	void Cyl(const FTransform& F, const FVector& Bottom, float Radius, float Height, FName MatName, bool bCollide = true);
	/** A rod (cylinder) from A to B (world). */
	void Rod(const FVector& A, const FVector& B, float Radius, FName MatName, bool bCollide = false);

	void BuildStation(const LyudTransport::FNetwork& Net, int32 S);
	void BuildStair(const FTransform& Foot, float Height, int32 Station);
	void BuildPylons(const LyudTransport::FNetwork& Net, int32 L);
	void BuildRopes(const LyudTransport::FNetwork& Net, int32 L);
	void BuildDepot(const LyudTransport::FNetwork& Net, int32 L);
	void BuildClubBoard(const LyudTransport::FNetwork& Net);
	FVector W(float X, float Y, float Z) const;
};

/**
 * A station's glass lift: a cab between the ground and the decks. Standing in it for a moment sends it to the other
 * end (the top deck from the ground, the ground from a deck); E at a door calls it. The player rides it as a moving
 * floor, like the cabins.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudStationLift : public AActor
{
	GENERATED_BODY()

public:
	ALyudStationLift();
	virtual void Tick(float DeltaTime) override;

	/** Stops (cm over the actor's origin): the ground first. */
	void Setup(const TArray<float>& InStops, UMaterialInterface* Floor, UMaterialInterface* Glass, UMaterialInterface* Frame);
	void CallTo(int32 Stop);
	int32 StopNear(float LocalZ) const;
	bool IsMoving() const { return Target >= 0 && !FMath::IsNearlyEqual(Z, Stops[Target], 1.f); }
	UPROPERTY(VisibleAnywhere, Category = "Lift") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Lift") TObjectPtr<USceneComponent> Cab;
	UPROPERTY(VisibleAnywhere, Category = "Lift") TObjectPtr<UBoxComponent> Inside;

	TArray<float> Stops;
	/** Per stop: the side of the shaft its door is on (0..3, quarter turns of the actor's X). */
	TArray<int32> DoorSides;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Gates;
	float Z = 0.f;
	int32 Target = -1;
	float Stand = 0.f;
	int32 LastStop = 0;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Hum;
};

/**
 * One cabin: the hi3d shell (its door side cut open) round a simple inside: floor, two benches facing each other,
 * corner posts, a lamp, a small board over the door with where it goes, the number and the cabin's own name on both
 * sides. Driven by ULyudTransportSubsystem along its rope: the player inside rides it as a moving floor.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudCabin : public AActor
{
	GENERATED_BODY()

public:
	ALyudCabin();

	void Setup(ALyudCableway* Way, int32 Index, int32 Number, const FText& Name, const FLinearColor& LineColor);
	/** Grip on the rope at G (world), heading Dir; Door 0..1; the board's text. */
	void Drive(const FVector& Grip, const FVector& Dir, float Door, float DeltaTime);
	void SetBoard(const FText& Line1, const FText& Line2);
	/** Is P inside (the cabin's own volume)? */
	bool Contains(const FVector& P) const;
	/** Seat K's place (world) and facing for a seated person. */
	FTransform Seat(int32 K) const;
	/** Where the door is (world), its middle at floor level. */
	FVector DoorPoint() const;

	UPROPERTY(VisibleAnywhere, Category = "Cabin") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Cabin") TObjectPtr<UStaticMeshComponent> Shell;
	UPROPERTY(VisibleAnywhere, Category = "Cabin") TObjectPtr<UStaticMeshComponent> DoorLeaf;
	UPROPERTY(VisibleAnywhere, Category = "Cabin") TObjectPtr<UBoxComponent> Inside;
	UPROPERTY(VisibleAnywhere, Category = "Cabin") TObjectPtr<UWidgetComponent> Board;
	UPROPERTY(VisibleAnywhere, Category = "Cabin") TObjectPtr<UPointLightComponent> Lamp;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidgetComponent>> Plates;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Hum;
	int32 Index = -1;
	float Door = 0.f;
	FText BoardA, BoardB;
	bool bVisibleNear = true;

	static constexpr float Length = 380.f;      // cm, along the rope (the kit's egg is 3.9 x 2.2 m)
	static constexpr float Width = 210.f;
	static constexpr float Height = 230.f;
	static constexpr float Hanger = 390.f;      // rope over the floor (network.json rope_over_deck)
};
