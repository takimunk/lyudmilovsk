#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeroReachComponent.h"
#include "LyudSave.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "LyudFishing.generated.h"

namespace LyudFishing { class FSession; }
class UAudioComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;

/** One species in the angler's log: how many, the biggest. */
USTRUCT()
struct FLyudCatchRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame) FName Species;
	UPROPERTY(SaveGame) int32 Count = 0;
	UPROPERTY(SaveGame) float BiggestGrams = 0.f;
	UPROPERTY(SaveGame) float BiggestCm = 0.f;
	/** The calendar's day of the first one. */
	UPROPERTY(SaveGame) FDateTime First;
};

/** What of fishing goes into the save ("Fishing"): the bait the hero fishes with, the hook baited or bare, the catch log. */
UCLASS()
class LYUDMILOVSK_API ULyudFishingState : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static ULyudFishingState* Get(const UObject* WorldContext);
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	void Reset();

	/** The bait item on the rig (bait_worm, bait_maggot, lure_spoon; none: a bare hook) and whether one is on the hook now. */
	UPROPERTY(SaveGame) FName Bait;
	UPROPERTY(SaveGame) bool bBaited = false;
	UPROPERTY(SaveGame) TArray<FLyudCatchRecord> Log;
	UPROPERTY(SaveGame) int32 Total = 0;

	/** Grams of each species in the pocket now (what the catch weighed; a sale prices it by this). */
	UPROPERTY(SaveGame) TMap<FName, float> BagGrams;

	FLyudCatchRecord* Find(FName Species);
	/** The fish in the pocket, and what the Gastronom pays for them all (roubles). */
	int32 FishHeld(const UObject* WorldContext, int32* OutRoubles = nullptr) const;
	/** Sells every fish in the pocket: the roubles go in, the fish out; returns the roubles (0: nothing to sell). */
	int32 SellAll(const UObject* WorldContext);
	/** Writes a catch in the log; true if it is the first of its kind or the biggest yet. */
	bool Record(FName Species, float Grams, float Cm, const FDateTime& When);
};

/**
 * The fishing rod in the hand: a telescopic float rod with a spinning reel (blender/props/fishing; a stand-in from the engine's
 * shapes when the meshes are not imported), the line from its tip, the float or the spoon, and the fish on the hook. The
 * blank is a chain of segments that bend with the line's tension toward where it pulls. Origin at the grip (the reel seat),
 * +X along the rod to the tip, +Z away from the reel. Nothing on it collides or goes into Lumen (it moves every frame).
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ALyudFishingRod : public AActor
{
	GENERATED_BODY()

public:
	ALyudFishingRod();

	static constexpr int32 Segments = 8;
	static constexpr float Length = 270.f;
	static constexpr int32 LinePieces = 24;

	/** Bend the blank: Amount 0 straight .. ~1.4 a hard fight (radians at the tip), toward a world direction. */
	void SetBend(float Amount, const FVector& PullDir);
	/** The tip ring (world), after the bend. */
	FVector Tip() const;
	/** The line from the tip to End (world); Slack 0 taut .. 1 hanging. Under water it goes straight. */
	void SetLine(const FVector& End, float Slack, float WaterZ);
	void HideLine();
	/** The float (red top, white body) at Where (world), standing up or lying (0..1), or hidden. */
	void SetFloat(bool bShow, const FVector& Where = FVector::ZeroVector, float Lean = 0.f);
	/** The spoon at Where, turning as it is wound. */
	void SetSpoon(bool bShow, const FVector& Where = FVector::ZeroVector, const FVector& Along = FVector::ForwardVector, float Spin = 0.f);
	/** The fish on the hook (its own swimming mesh when imported: /Game/Sea/Fish/SM_<Id>), hanging head up or swimming, wriggling. */
	void SetFish(FName Species, float LengthM);
	void PlaceFish(bool bShow, const FVector& Where, const FRotator& Rot, float Wriggle, float DeltaTime);
	/** The reel's handle and spool turning (wound in: + ; line going out: -). */
	void TurnReel(float Revs);

	virtual void BeginPlay() override;

private:
	void BuildStandIns();
	UStaticMeshComponent* Part(const TCHAR* Name, UStaticMesh* Mesh, USceneComponent* Parent, const FLinearColor& Tint);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Butt;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Reel;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ReelHandle;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Joints;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Blank;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Line;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Float;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Spoon;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Fish;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Mids;

	TArray<FTransform> LineXf;
	FName FishSpecies;
	float FishPhase = 0.f;
	float ReelAngle = 0.f;
	bool bLineShown = false;
	bool bStandIn = false;
};

/**
 * Fishing (docs/fishing.md). The rod from the inventory ("Взять в руку" on fishing_rod) comes up in the right hand; a bait
 * taken in the hand while there is a rod goes on the rig (X changes it). Then, at the water:
 *   LMB hold, let go   the cast: the longer the swing, the further (a float to ~18 m, the spoon to ~30 m)
 *   float              wait; the float twitches (nibbles), then goes under: LMB strikes
 *   spoon              LMB held winds it in; a hit while winding hooks itself as often as not, else LMB strikes
 *   a fish on          LMB held winds (the rod bends, the line's tension climbs), let go and the clutch gives it line;
 *                      wind through a strong run and the line breaks, leave it slack too long and the hook comes out
 *   RMB                wind the line in;   Q   the rod away (wound in first)
 * What bites comes from the food web where the float lies (ULyudEcosystem::GetBiteWeights, the bait's liking), a fish caught
 * comes out of it (ReportCatch) and out of the water round the diver (ULyudUnderwaterSubsystem::TakeFish) and goes in the
 * pocket as fish_<species>; the log of catches is saved (ULyudFishingState).
 * Console: lyud.Fish.Give (rod and baits), lyud.Fish.State, lyud.Fish.Press 1|0, lyud.Fish.Click, lyud.Fish.Bite (a take now),
 * lyud.Fish.Hook <Species> (one on the line now), lyud.Fish.Log. Log lines "FISHING ..." for tests.
 */
UCLASS()
class LYUDMILOVSK_API ULyudFishing : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudFishing();
	virtual ~ULyudFishing() override;

	static ULyudFishing* Get(const UObject* WorldContext);
	static const FName RodId;

	/** The rod is in the hand: the mouse buttons and Q are the rod's. */
	static bool HasHand(const UObject* WorldContext);
	bool IsOut() const { return Rod.IsValid(); }
	/** The line is out (the float on the water, a fish on): the rod can't be put away yet. */
	bool IsLineOut() const;
	FString DebugState() const;

	/** The bait to fish with next (an item held; none: a bare hook). */
	void SetBait(FName Item);
	/** The next bait held after the current one (X). */
	void CycleBait();

	// checks without a mouse
	void Press(bool bDown) { bForced = bDown; }
	void Click() { bClickQueued = true; }
	void ForceBite();
	void ForceHook(FName Species);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudFishing, STATGROUP_Tickables); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	void TakeOut();
	void PutAway();
	void SyncBait();
	void UpdateWeights();
	void HandleEvents();
	void Fly(float DeltaTime);
	void Draw(float DeltaTime);
	void Caught();
	FTransform LookFrame() const;
	FHeroHoldPose PoseNow(float DeltaTime);
	void Sound(FName Pool, const FVector& Where, float Volume = 1.f);
	void Say(const FText& Text, float Seconds);
	void Prompt();
	FVector OnWater(const FVector2D& XY, float Below) const;
	bool IsWater(const FVector& Where) const;

	TPimplPtr<LyudFishing::FSession> Session;
	TWeakObjectPtr<ALyudFishingRod> Rod;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> ReelLoop;

	bool bAttached = false;
	bool bHidden = true;
	bool bForced = false;
	bool bClickQueued = false;
	bool bClickDown = false;
	bool bWaitUp = false;
	float HoldTime = 0.f;
	float WeightsIn = 0.f;
	float SayLeft = 0.f;
	FText Said;
	FText LastPrompt;
	/** Where the cast went from (world, at the water's height) and which way (unit, flat); the float or fish lies at Origin + Dir(Side) * Distance. */
	FVector Origin = FVector::ZeroVector;
	FVector Dir = FVector::ForwardVector;
	FVector FlyFrom = FVector::ZeroVector, FlyTo = FVector::ZeroVector, FlyPos = FVector::ZeroVector;
	float FlyTime = 0.f, FlyDur = 1.f;
	bool bFlying = false;
	/** The pose's moving parts: the strike's jerk, the cast's swing. */
	float Strike = 0.f;
	float Swing = 0.f;
	float LastDist = 0.f;
	float ReelClickIn = 0.f;
	/** After the catch: the fish shown hanging at the line's end, then into the pocket. */
	float ShowLeft = -1.f;
	float ShowGrams = 0.f, ShowCm = 0.f;
	FName ShowSpecies;
	float FishWriggle = 0.f;
	FVector EndPos = FVector::ZeroVector;
	FName ForcedHook;
	/** The hand's pose now (look frame), following the state's. */
	FVector PosePos = FVector(40.f, 20.f, -28.f);
	float PoseElev = 30.f, PoseYaw = -4.f;
};
