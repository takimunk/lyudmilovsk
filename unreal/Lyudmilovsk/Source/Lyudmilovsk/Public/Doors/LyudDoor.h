#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudSave.h"
#include "LyudDoor.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPrimitiveComponent;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
class ALyudDoorBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLyudDoorEvent, ALyudDoorBase*, Door, AActor*, By);

/**
 * What every door in the game does, whatever it looks like (docs/doors.md):
 *  - it is open or closed (bOpen, saved through ILyudSaveable by actor name) and swings between the two at SwingSpeed;
 *  - the player opens and closes it with E while looking at it within InteractRange (ULyudDoorSubsystem polls E, never
 *    consumes it, and picks the one door the view ray meets first);
 *  - NPCs go through it: any pawn that is not the player's, or an actor registered with ULyudDoorSubsystem::RegisterAgent,
 *    that walks up to the doorway gets it opened and, if the door was shut, shut behind it once the doorway is clear.
 *    Movers that plan their own paths ask FindDoorOnSegment / RequestPassage / IsPassable / GetWaitPoint;
 *  - locked doors (bLocked) open for nobody until unlocked.
 * The leaves never shove anyone: they ignore pawns while they swing and block them only at rest, and a door does not
 * close on someone standing where its leaves sweep. Leaves never affect the navmesh, so a navmesh runs through doorways.
 *
 * Subclasses pose the leaves (ApplyOpenness) and say where the doorway is (local boxes): ALyudDoor is the standard
 * built-in-code door, ASwingDoor drives leaf actors placed in a level (the club's blast door, the hall's partition).
 */
UCLASS(Abstract)
class LYUDMILOVSK_API ALyudDoorBase : public AActor, public ILyudSaveable
{
	GENERATED_BODY()

public:
	ALyudDoorBase();

	/** Open when the level starts (a save overrides it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bStartOpen = false;

	/** Nobody opens a locked door; a locked open door can still be shut. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Door")
	bool bLocked = false;

	/** Swing speed, degrees per second of a 90 degree swing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float SwingSpeed = 160.f;

	/** The player may open and close it with E. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Use")
	bool bPlayerCanUse = true;

	/** How far from the eye E reaches the door, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Use")
	float InteractRange = 220.f;

	/** NPCs walking up to it get it opened. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Use")
	bool bNpcsCanUse = true;

	/** A door that was shut when an NPC opened it is shut again once the NPC has gone through and the doorway is clear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Use")
	bool bCloseBehindNpcs = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Sound")
	TObjectPtr<USoundBase> OpenSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Sound")
	TObjectPtr<USoundBase> CloseSound;
	/** Played when the leaves come to rest closed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Sound")
	TObjectPtr<USoundBase> ShutSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Sound")
	TObjectPtr<USoundBase> LockedSound;

	/** Starts to open / to close (By: who did it, may be null). */
	UPROPERTY(BlueprintAssignable, Category = "Door")
	FLyudDoorEvent OnOpening;
	UPROPERTY(BlueprintAssignable, Category = "Door")
	FLyudDoorEvent OnClosing;
	/** Came to rest fully open / fully closed. */
	UPROPERTY(BlueprintAssignable, Category = "Door")
	FLyudDoorEvent OnOpened;
	UPROPERTY(BlueprintAssignable, Category = "Door")
	FLyudDoorEvent OnClosed;

	/** Opens; false if it is locked (or already open). */
	UFUNCTION(BlueprintCallable, Category = "Door")
	bool Open(AActor* By = nullptr);

	/** Closes; it waits while someone stands where the leaves sweep. False if already closed. */
	UFUNCTION(BlueprintCallable, Category = "Door")
	bool Close(AActor* By = nullptr);

	/** What E does. */
	UFUNCTION(BlueprintCallable, Category = "Door")
	bool Toggle(AActor* By = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Door")
	void SetLocked(bool bInLocked);

	/** Open or opening (the state it is going to). */
	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsOpen() const { return bOpen; }

	/** 0 closed .. 1 open, where the leaves are now. */
	UFUNCTION(BlueprintPure, Category = "Door")
	float GetOpenness() const { return Openness; }

	/** The leaves are fully open: a character fits through now. */
	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsPassable() const { return bOpen && Openness >= 0.999f; }

	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsFullyClosed() const { return !bOpen && Openness <= 0.001f; }

	// ---- for NPCs and anything that plans its own path

	/** Agent wants to go through: opens the door (unless locked or bNpcsCanUse is off) and keeps it open until
	 *  ReleasePassage. True once it is passable; until then wait at GetWaitPoint. */
	UFUNCTION(BlueprintCallable, Category = "Door|Npc")
	bool RequestPassage(AActor* Agent);

	/** Agent is through (or gave up); the door shuts behind it if it was shut before (bCloseBehindNpcs). */
	UFUNCTION(BlueprintCallable, Category = "Door|Npc")
	void ReleasePassage(AActor* Agent);

	/** Can NPCs use it at all right now (not locked, bNpcsCanUse)? */
	UFUNCTION(BlueprintPure, Category = "Door|Npc")
	bool CanNpcsPass() const { return bNpcsCanUse && !bLocked; }

	/** Floor centre of the doorway, world. */
	UFUNCTION(BlueprintPure, Category = "Door|Npc")
	FVector GetPassageCenter() const;

	/** Horizontal unit vector through the doorway toward the side the leaves swing to. */
	UFUNCTION(BlueprintPure, Category = "Door|Npc")
	FVector GetPassageNormal() const;

	/** Clear width and height with the leaves open, cm. */
	UFUNCTION(BlueprintPure, Category = "Door|Npc")
	virtual float GetClearWidth() const;
	UFUNCTION(BlueprintPure, Category = "Door|Npc")
	virtual float GetClearHeight() const;

	/** Where an agent of Radius coming from From should stand while the door opens: in front of the doorway on From's
	 *  side, out of the leaves' sweep. */
	UFUNCTION(BlueprintPure, Category = "Door|Npc")
	FVector GetWaitPoint(const FVector& From, float Radius = 40.f) const;

	/** Does a straight move From -> To go through this doorway? OutAlpha: where along it the doorway starts (0..1). */
	UFUNCTION(BlueprintPure, Category = "Door|Npc")
	bool CrossesSegment(const FVector& From, const FVector& To, float& OutAlpha) const;

	/** Is a body of Radius at P in the doorway or where the leaves sweep? */
	bool IsInSweep(const FVector& P, float Radius) const;

	/** Can the player at Eye looking along Dir use it? OutDistance: how far along the view it is (smaller wins). */
	virtual bool PlayerCanUse(const APawn* Pawn, const FVector& Eye, const FVector& Dir, float& OutDistance) const;

	/** The hole in the wall the doorway fills, actor space: x through the wall, y across, z up from the floor. */
	virtual FBox GetDoorwayLocalBox() const PURE_VIRTUAL(ALyudDoorBase::GetDoorwayLocalBox, return FBox(ForceInit););
	/** Doorway plus where the leaves swing, actor space. */
	virtual FBox GetSweepLocalBox() const { return GetDoorwayLocalBox(); }

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnGameLoaded_Implementation() override;

protected:
	/** Pose the leaves for 0 (closed) .. 1 (open). */
	virtual void ApplyOpenness(float InOpenness) {}
	/** The leaves' colliders (to let pawns through while they swing). */
	virtual void GetLeafPrimitives(TArray<UPrimitiveComponent*>& Out) const {}

	/** Called once at BeginPlay before the start pose is applied (subclasses record the leaves' rest poses). */
	virtual void PrepareLeaves() {}

	bool SetOpen(bool bInOpen, AActor* By, bool bForPassage);
	void SetLeavesBlockPawns(bool bBlock);
	bool SomeoneInSweep() const;
	void Play(USoundBase* Sound) const;
	void UpdateTick();

	float Openness = 0.f;

private:
	UPROPERTY(SaveGame)
	bool bOpen = false;

	/** An NPC opened it from shut: shut it again behind them. */
	UPROPERTY(SaveGame)
	bool bOpenedForPassage = false;

	TArray<TWeakObjectPtr<AActor>> PassageHolders;
	bool bLeavesBlock = true;
	bool bWaitingToClose = false;
};

UENUM(BlueprintType)
enum class ELyudDoorSize : uint8
{
	/** One leaf, clear 110 x 215. */
	Standard,
	/** One leaf, clear 130 x 215 (halls, stairs, shops). */
	Wide,
	/** Two leaves, clear 150 x 230 (entrances). */
	Double,
	/** ClearWidth x ClearHeight, never below the character standard. */
	Custom,
	/** Fills HoleWidth x HoleHeight, a hole the building already has; flagged if that is below the standard. */
	FitHole,
};

UENUM(BlueprintType)
enum class ELyudDoorHinge : uint8
{
	/** The hinge on the left as you stand on the side the door swings to, facing the doorway. */
	Left,
	Right,
};

/**
 * The standard door: built in code from a few numbers, so every one is the same size class and works the same way.
 *
 * Actor frame: origin on the floor at the middle of the hole, in the middle of the wall; +X out of the wall toward the
 * side the leaves swing to; Y across the doorway; the wall is WallThickness thick. A lining (DoorLiningThickness) lines
 * the hole, a casing frames it on both faces; the leaf sits in the lining flush with the +X face and swings out to +X
 * by OpenAngle. With the leaf open its thickness stands across the opening, so the hole is
 *     clear width + 2 * lining + leaves * leaf thickness,   clear height + lining
 * (HoleForClear / ClearForHole; tools/character_standard.py door_hole() for the builders). The clear sizes never go below
 * MinDoorClearWidth x MinDoorClearHeight (CharacterStandard.h).
 */
UCLASS(Blueprintable)
class LYUDMILOVSK_API ALyudDoor : public ALyudDoorBase
{
	GENERATED_BODY()

public:
	ALyudDoor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Shape")
	ELyudDoorSize Size = ELyudDoorSize::Standard;

	/** Custom: clear width / height, cm (leaves open). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Shape", meta = (EditCondition = "Size == ELyudDoorSize::Custom"))
	float ClearWidth = 110.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Shape", meta = (EditCondition = "Size == ELyudDoorSize::Custom"))
	float ClearHeight = 215.f;
	/** Custom / FitHole: two leaves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Shape", meta = (EditCondition = "Size == ELyudDoorSize::Custom || Size == ELyudDoorSize::FitHole"))
	bool bDoubleLeaf = false;

	/** FitHole: the hole, cm. Otherwise the hole this door needs (set from the size). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Shape")
	float HoleWidth = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Shape")
	float HoleHeight = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Shape", meta = (ClampMin = "4"))
	float WallThickness = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Shape")
	ELyudDoorHinge Hinge = ELyudDoorHinge::Left;

	/** How far the leaves swing, degrees (at least 90, so the open leaf clears the opening). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Shape", meta = (ClampMin = "90", ClampMax = "175"))
	float OpenAngle = 95.f;

	/** Lining and casing (off when the building models its own frame round the hole). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Shape")
	bool bBuildFrame = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Shape")
	float CasingWidth = 7.f;

	/** The leaf's mesh, stretched to the leaf's box (x thickness, y width, z height); a box when empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Look")
	TObjectPtr<UStaticMesh> LeafMesh;

	/** Materials; empty = the engine's basic material in LeafColor / FrameColor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Look")
	TObjectPtr<UMaterialInterface> LeafMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Look")
	TObjectPtr<UMaterialInterface> FrameMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Look")
	TObjectPtr<UMaterialInterface> HandleMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Look")
	FLinearColor LeafColor = FLinearColor(0.36f, 0.33f, 0.27f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Look")
	FLinearColor FrameColor = FLinearColor(0.72f, 0.70f, 0.64f);

	/** Clear size this door has (leaves open), cm. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Shape")
	float ActualClearWidth = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Shape")
	float ActualClearHeight = 0.f;
	/** FitHole only: the hole is smaller than the character standard allows (the builder must widen it). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Shape")
	bool bBelowStandard = false;

	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<UInstancedStaticMeshComponent> Frame;
	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<USceneComponent> HingeA;
	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<USceneComponent> HingeB;
	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<UStaticMeshComponent> LeafA;
	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<UStaticMeshComponent> LeafB;
	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<UInstancedStaticMeshComponent> HandlesA;
	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<UInstancedStaticMeshComponent> HandlesB;

	/** The hole a door of this clear size needs, cm (x width, y height). */
	UFUNCTION(BlueprintPure, Category = "Door")
	static FVector2D HoleForClear(float InClearWidth, float InClearHeight, bool bTwoLeaves);
	/** The clear size a hole gives, cm. */
	UFUNCTION(BlueprintPure, Category = "Door")
	static FVector2D ClearForHole(float InHoleWidth, float InHoleHeight, bool bTwoLeaves);

	/** Re-lay the frame and leaves from the numbers above (OnConstruction does it; call after changing them at run time). */
	UFUNCTION(BlueprintCallable, Category = "Door")
	void Rebuild();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual float GetClearWidth() const override { return ActualClearWidth; }
	virtual float GetClearHeight() const override { return ActualClearHeight; }
	virtual FBox GetDoorwayLocalBox() const override;
	virtual FBox GetSweepLocalBox() const override;

protected:
	virtual void ApplyOpenness(float InOpenness) override;
	virtual void GetLeafPrimitives(TArray<UPrimitiveComponent*>& Out) const override;

private:
	int32 NumLeaves() const;
	/** The clear and hole sizes, the lining's inside and the leaf width from the numbers above. */
	void ComputeSizes();
	void ApplyLooks();

	/** Inner width / height of the lining. */
	float InnerW = 0.f;
	float InnerH = 0.f;
	float LeafW = 0.f;
};

/**
 * Every door of the world: E for the player, doors opening for NPCs, the path queries. Polls E (never consumes it: the
 * seat, the vending machine, the NPCs listen to it too) and gives it to the one door the view meets first.
 * Console: lyud.Door.List, lyud.Door.Toggle <name part>, lyud.Door.Lock <name part> <0|1>, Door.SelfTest.
 */
UCLASS()
class LYUDMILOVSK_API ULyudDoorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudDoorSubsystem* Get(const UObject* WorldContext);

	void RegisterDoor(ALyudDoorBase* Door);
	void UnregisterDoor(ALyudDoorBase* Door);
	const TArray<TWeakObjectPtr<ALyudDoorBase>>& GetDoors() const { return Doors; }

	/** A mover that is not a pawn (an animal, a crowd extra) that doors should open for; Radius: its body's. */
	UFUNCTION(BlueprintCallable, Category = "Door")
	void RegisterAgent(AActor* Agent, float Radius = 35.f);
	UFUNCTION(BlueprintCallable, Category = "Door")
	void UnregisterAgent(AActor* Agent);

	/** The first door a straight move From -> To goes through (null if none). */
	UFUNCTION(BlueprintCallable, Category = "Door")
	ALyudDoorBase* FindDoorOnSegment(const FVector& From, const FVector& To) const;

	/** The door the player would use with E now (null if none). */
	UFUNCTION(BlueprintCallable, Category = "Door")
	ALyudDoorBase* DoorInView() const;

	/** What E does: toggles DoorInView. True if a door took it. */
	UFUNCTION(BlueprintCallable, Category = "Door")
	bool PlayerInteract();

	/** Bodies that doors must not close on: the player and the agents (pawns and registered), with radii. */
	void GetBodies(TArray<TPair<FVector, float>>& Out) const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudDoorSubsystem, STATGROUP_Tickables); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	struct FAgent
	{
		TWeakObjectPtr<AActor> Actor;
		float Radius = 35.f;
		bool bRegistered = false;
		FVector LastPos = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		TArray<TWeakObjectPtr<ALyudDoorBase>> Holding;
	};

	void UpdateAgents(float DeltaTime);

	TArray<TWeakObjectPtr<ALyudDoorBase>> Doors;
	TArray<FAgent> Agents;
	float AgentClock = 0.f;
};
