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
class FLyudMeshKit;

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
	/** The gallery's frame: origin on its floor in its middle, +X toward the tower (Transport.Goto "vestibule"). */
	FTransform Vestibule;
	/** The vestibule connector (docs/transport.md): origin on the floor in the gallery's end door, +X toward the tower. */
	FTransform Connector;
	/** The tower's frame: origin on the ground floor in the middle of the lift shaft, X along the first level's line. */
	FTransform Core;
	/** The way out of the vestibule's street entrance (world, flat), zero when there is none. */
	FVector EntranceOut = FVector::ZeroVector;
	/** Per deck (as Decks): the frame the depot's storage rail runs along (origin past the turn wheel on the deck, X away). */
	TArray<FTransform> DepotAt;
	/** The vestibule's room: its middle at eye height, looking out to the gallery's side (identity: none). */
	FTransform Room;
	/** The tower's stops (cm over its ground: the ground, each deck) and the side of each stop's door (0..3). */
	TArray<float> Stops;
	TArray<int32> StopSides;
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
	/** The ropes near the viewer follow the network's span motion (sag under cabins, sway in the wind). */
	void UpdateRopes(const LyudTransport::FNetwork& Net, const FVector& Viewer);
	/** The boards' words (wait times) and the lifts, a few times a second. */
	void UpdateBoards(const LyudTransport::FNetwork& Net, const FVector& Viewer, float DeltaTime);

	TArray<FLyudBuiltStation> Built;

	/** Where a waiting person stands on station S's platform for line L (spot K of a few). */
	FTransform WaitSpot(int32 Station, int32 Line, int32 K) const;
	/** The call panel of station S nearest to P within R cm: its index into Built[S].Panels, -1 none. */
	int32 PanelNear(int32 Station, const FVector& P, float R) const;
	/** A test of station S's walkable parts: traces down at its landings, platforms, gallery and podium (log lines). */
	int32 Probe(int32 S) const;
	/** A named view for tests (Transport.Goto): lift, liftin, top, tower, pylon. */
	bool View(int32 S, const FString& Name, FTransform& Out) const;
	/** The station whose structure P is on or at the foot of (within its footprint), -1 none. */
	int32 StationAt(const FVector& P) const;

	UMaterialInterface* Mat(FName Name);
	/** A built kit (LyudMeshKit.h) as a mesh component at At (world; relative to Parent when given), its materials set. */
	UStaticMeshComponent* KitMesh(const FLyudMeshKit& K, const FTransform& At, FName Name, bool bMovable = false, USceneComponent* Parent = nullptr);
	/** Real glass and plaster on a kit mesh's TR_Glass / TR_Interior slots. */
	void KitMaterials(UStaticMeshComponent* C);

private:
	UPROPERTY(Transient) TObjectPtr<ULyudTransportSubsystem> Transport;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> Ism;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UMaterialInterface>> Mats;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Singles;
	TArray<FVector> StationCentres;
	TArray<float> StationRadius;
	UPROPERTY(Transient) TObjectPtr<UWidgetComponent> ClubBoard;
	/** Per line: the rope, one rod per loop segment, movable. */
	UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> RopeIsm;
	TArray<TArray<bool>> RopeMoved;
	FTransform ClubBoardAt;

public:
	/** The route board by the club's exit (where it stands), for the E prompt. */
	bool ClubBoardNear(const FVector& P, float R) const { return ClubBoard && FVector::Dist(ClubBoardAt.GetLocation() + FVector(0, 0, 150.f), P) < R; }
	/** The route diagram on a vestibule's wall near P: its station, -1 none. */
	int32 VestibuleBoardNear(const FVector& P, float R) const
	{
		for (int32 I = 0; I < VestBoards.Num(); I++)
		{
			if (FVector::Dist(VestBoards[I].GetLocation(), P) < R)
			{
				return VestBoardStation[I];
			}
		}
		return -1;
	}

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
	/** The parts (LyudStationBuild.cpp), into the station's kit in its tower frame (cm). */
	void BuildTower(FLyudMeshKit& K, const LyudTransport::FNetwork& Net, int32 S, const TArray<float>& Stops, const TArray<int32>& Sides, bool bOwnShell);
	/** A station's rope gear (LyudStationBuild.cpp): the posts and frames of its sheaves, its turn wheel. */
	void BuildRopeGear(FLyudMeshKit& K, const LyudTransport::FNetwork& Net, int32 S);
	/** The vestibule's tower kit (vestibules.json v2 "tower"): its modules stacked to the height, in the tower's frame. */
	bool StackTowerKit(int32 S, const FString& Style, float Height);
	void BuildDeck(FLyudMeshKit& K, const LyudTransport::FNetwork& Net, int32 S, int32 Level);
	void PlaceVestibule(const LyudTransport::FNetwork& Net, int32 S);
	/** Grass and forests scattered before the stations stood: again, with every station's footprint (and a margin) bare. */
	void ClearGround();
	/** Per station: boxes (world) the ground must be bare in (the vestibule and its terrace, the gallery, the island). */
	TArray<TPair<FTransform, FVector>> BareBoxes;
	UWidgetComponent* Widget(int32 S, const TSharedRef<class SWidget>& Slate, const FTransform& At, const FVector2D& Draw, float Scale, bool bOpaque);
	TArray<FTransform> VestBoards;
	TArray<int32> VestBoardStation;
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
	void Setup(ALyudCableway* Way, const TArray<float>& InStops, const TArray<int32>& InSides);
	void CallTo(int32 Stop);
	int32 StopNear(float LocalZ) const;
	bool IsMoving() const { return Target >= 0 && !FMath::IsNearlyEqual(Z, Stops[Target], 1.f); }
	UPROPERTY(VisibleAnywhere, Category = "Lift") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Lift") TObjectPtr<USceneComponent> Cab;
	UPROPERTY(VisibleAnywhere, Category = "Lift") TObjectPtr<UBoxComponent> Inside;

	TArray<float> Stops;
	/** Per stop: the side of the shaft its door is on (0..3, quarter turns of the actor's X). */
	TArray<int32> DoorSides;
	/** Two sliding steel leaves per stop (the landing doors), shut but where the cab stands still. */
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Leaves;
	TArray<float> Open;
	float Dwell = 0.f;
	float LogT = 0.f;
	/** The cab brought the player here: it stays until they step out. */
	bool bRode = false;
	float OutTime = 0.f;
	float Z = 0.f;
	int32 Target = -1;
	float Stand = 0.f;
	int32 LastStop = 0;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Hum;
};

/**
 * One cabin: the hi3d egg made a room in Blender (blender/transport/cabin.py): the hollow shell with glass windows and
 * a door opening, the plug door that steps out and slides along the side, the floor and two benches facing each other
 * (four seats, kit.json sockets), the hanger and grip plumb over the floor's centre; invisible boxes from kit.json keep
 * people inside. A small board over the door says where it goes, the number and the cabin's own name on both sides.
 * Driven by ULyudTransportSubsystem along its rope: the player inside rides it as a moving floor. An old kit without
 * the room gets the earlier shell round boxes.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudCabin : public AActor
{
	GENERATED_BODY()

public:
	ALyudCabin();

	void Setup(ALyudCableway* Way, int32 Index, int32 Number, const FText& Name, const FLinearColor& LineColor);
	/** Grip on the rope at G (world), heading Dir; Door 0..1; the board's text. */
	void Drive(const FVector& Grip, const FVector& Dir, float Door, float DeltaTime, float PitchDeg = 0.f, float RollDeg = 0.f);
	void SetBoard(const FText& Line1, const FText& Line2);
	/** The rope's height over the cabin's floor (kit.json cabin.grip; network.json rope_over_deck matches it). */
	static float GripOverFloor();
	/** Is P inside (the cabin's own volume)? */
	bool Contains(const FVector& P) const;
	/** Seat K's place (world) and facing for a seated person. */
	FTransform Seat(int32 K) const;
	/** Where the door is (world), its middle at floor level, on the hull's outside. */
	UFUNCTION(BlueprintPure, Category = "Cabin")
	FVector DoorPoint() const;

private:
	void SetupBoxes(ALyudCableway* Way, int32 InIndex, int32 Number, const FText& Name, const FLinearColor& LineColor);
public:

	UPROPERTY(VisibleAnywhere, Category = "Cabin") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Cabin") TObjectPtr<UStaticMeshComponent> Shell;
	UPROPERTY(VisibleAnywhere, Category = "Cabin") TObjectPtr<UStaticMeshComponent> DoorLeaf;
	UPROPERTY(VisibleAnywhere, Category = "Cabin") TObjectPtr<UBoxComponent> Inside;
	UPROPERTY(VisibleAnywhere, Category = "Cabin") TObjectPtr<UWidgetComponent> Board;
	UPROPERTY(VisibleAnywhere, Category = "Cabin") TObjectPtr<UPointLightComponent> Lamp;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidgetComponent>> Plates;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Cabin") TArray<TObjectPtr<UWidgetComponent>> Posters;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Hum;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> InsideParts;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Hanger;
	UPROPERTY(Transient) TObjectPtr<UBoxComponent> Gate;
	bool bRoom = false;                         // the hollow cabin from the kit (else the boxes)
	int32 Index = -1;
	/** 0 shut .. 1 open. */
	UPROPERTY(BlueprintReadOnly, Category = "Cabin") float Door = 0.f;
	FText BoardA, BoardB;
	bool bVisibleNear = true;

	static constexpr float Length = 380.f;      // cm, along the rope (the kit's egg is 3.9 x 2.2 m)
	static constexpr float Width = 210.f;
	static constexpr float Height = 230.f;
};
