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
class AHeroCharacter;
class APawn;
class APlayerController;
class UAudioComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
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

/** One fish in the pocket and what it weighed (the kitchen picks by each fish's own size). */
USTRUCT()
struct FLyudBagFish
{
	GENERATED_BODY()

	UPROPERTY(SaveGame) FName Species;
	UPROPERTY(SaveGame) float Grams = 0.f;
};

/** One fish in the angler's journal: when, what, how big, where and with what. */
USTRUCT()
struct FLyudCatchEntry
{
	GENERATED_BODY()

	UPROPERTY(SaveGame) FDateTime When;
	UPROPERTY(SaveGame) FName Species;
	UPROPERTY(SaveGame) float Grams = 0.f;
	UPROPERTY(SaveGame) float Cm = 0.f;
	/** The spot (Content/Fishing/fishing.json: pier_head, pier, promenade, beach), boat, or none: the shore. */
	UPROPERTY(SaveGame) FName Spot;
	UPROPERTY(SaveGame) FName Bait;
	UPROPERTY(SaveGame) FName Rod;
};

/** What of fishing goes into the save ("Fishing"): the bait the hero fishes with, the hook baited or bare, the catch log. */
UCLASS()
class LYUDMILOVSK_API ULyudFishingState : public UGameInstanceSubsystem, public ILyudSaveable
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
	/** Fish the diner's cook has fried for the hero. */
	UPROPERTY(SaveGame) int32 Cooked = 0;

	/** The plate of fried fish the diner serves (Content/Food/food.json). */
	static const FName DinerFishId;

	/** Grams of each species in the pocket now (what the catch weighed; a sale prices it by this). */
	UPROPERTY(SaveGame) TMap<FName, float> BagGrams;
	/** The same fish one by one (a fish without an entry, given or found, counts at its share of BagGrams). */
	UPROPERTY(SaveGame) TArray<FLyudBagFish> BagFish;

	/** The rowing boat (ALyudRowBoat): where it lies, which way, the day it was hired, the hero in it. Written on saving. */
	UPROPERTY(SaveGame) bool bBoatSaved = false;
	UPROPERTY(SaveGame) FVector BoatPos = FVector::ZeroVector;
	UPROPERTY(SaveGame) float BoatYaw = 0.f;
	UPROPERTY(SaveGame) int32 BoatPaidDay = -1;
	UPROPERTY(SaveGame) bool bBoatAboard = false;
	/** Its mooring and landing as found (a load spawns it at once there, not after the sea's depth map is read). */
	UPROPERTY(SaveGame) FVector BoatMooring = FVector::ZeroVector;
	UPROPERTY(SaveGame) FVector BoatLanding = FVector::ZeroVector;

	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;
	/** A caught fish into the pocket's weights. */
	void Pocket(FName Species, float Grams);

	/** The angler's journal: the last JournalMax fish, oldest first. */
	static constexpr int32 JournalMax = 60;
	UPROPERTY(SaveGame) TArray<FLyudCatchEntry> Journal;
	/** The rod last fished with (fishing_rod, rod_spinning, rod_sea): a bait taken in the hand brings it out. */
	UPROPERTY(SaveGame) FName Rod;
	/** The kitchens' buying: the calendar day it counts, fish each took that day, all they paid. */
	UPROPERTY(SaveGame) int32 KitchenDay = -1;
	UPROPERTY(SaveGame) TMap<FName, int32> KitchenTaken;
	UPROPERTY(SaveGame) int32 KitchenRoubles = 0;
	UPROPERTY(SaveGame) int32 KitchenSold = 0;

	FLyudCatchRecord* Find(FName Species);
	void Note(const FLyudCatchEntry& Entry);
	/** What a kitchen would buy now (the prized kinds of a good size, its day's want left: PerDay fish): how many, for how much. */
	int32 KitchenOffer(const UObject* WorldContext, FName Kitchen, int32 PerDay, int32* OutRoubles = nullptr) const;
	/** Sells it those; the roubles (0: nothing it wants). */
	int32 SellToKitchen(const UObject* WorldContext, FName Kitchen, int32 PerDay);
	/** The fish in the pocket the Gastronom buys, and what it pays for them all (roubles). */
	int32 FishHeld(const UObject* WorldContext, int32* OutRoubles = nullptr) const;
	/** Sells every fish in the pocket: the roubles go in, the fish out; returns the roubles (0: nothing to sell). */
	int32 SellAll(const UObject* WorldContext);
	/** The fish item in the pocket that weighs the most each (none: no fish). */
	FName BiggestHeld(const UObject* WorldContext, float* OutGrams = nullptr) const;
	/** One fish of Item out of the pocket (and its share of the grams); the grams it weighed, 0 if there was none. */
	float TakeOne(const UObject* WorldContext, FName Item);
	/** One fish of Catchable C out of the pocket: the one weighing Grams (>= 0), the biggest (-2), or one not listed (-1); its grams. */
	float TakeFish(const UObject* WorldContext, int32 C, float Grams);
	/** The diner's cook fries one fish of Item for Fee roubles: paid, the fish goes, a plate of fried fish (diner_fish) is
	 *  brought to the counter a few seconds later. False if there is no such fish or not the money. */
	bool Cook(const UObject* WorldContext, FName Item, int32 Fee);
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
	/** The bottom rig's lead (the sea rod: no float). */
	void SetSinker(bool bShow, const FVector& Where = FVector::ZeroVector);
	/** The rod's look for a LyudFishing::FindTackle index (0 the float rod, 1 the spinning rod, 2 the sea rod): the blank's colour and thickness. */
	void SetTier(int32 Tackle);
	/** Starts loading Species' swimming look in the background (ULyudPrefetch); true once it is in memory (or doesn't exist). */
	bool PrefetchFish(FName Species);
	/** The fish on the hook (its own swimming mesh when imported: /Game/Sea/Fish/SM_<Id>), hanging head up or swimming, wriggling.
	 *  Never loads: until its look is in memory the stand-in shows, swapped for the real one when it arrives. */
	void SetFish(FName Species, float LengthM);
	void PlaceFish(bool bShow, const FVector& Where, const FRotator& Rot, float Wriggle, float DeltaTime);
	/** The reel's handle and spool turning (wound in: + ; line going out: -). */
	void TurnReel(float Revs);
	/** Put away: everything hidden and straight (the rod is kept for the next time, not rebuilt). */
	void Park();

	virtual void BeginPlay() override;

private:
	void BuildStandIns();
	UStaticMeshComponent* Part(const TCHAR* Name, UStaticMesh* Mesh, USceneComponent* Parent, const FLinearColor& Tint);
	/** One plain-coloured material per colour, shared by the parts of that colour. */
	UMaterialInstanceDynamic* Paint(const FLinearColor& Tint);
	/** Puts Species' own mesh and materials on the hooked fish if they are in memory; false if not (the stand-in stays). */
	bool ApplyFishLook(FName Species);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Butt;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Reel;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ReelHandle;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Joints;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Blank;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Line;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Float;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Spoon;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Sinker;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Fish;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Mids;
	UPROPERTY() TObjectPtr<UMaterialInterface> BasicMat;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> StandInMid;
	/** Each species' materials made once (rooted in Mids), by species. */
	TMap<FName, TArray<UMaterialInstanceDynamic*>> FishMids;
	TMap<FLinearColor, UMaterialInstanceDynamic*> Paints;

	TArray<FTransform> LineXf;
	FName FishSpecies;
	float FishLength = 0.f;
	/** The species whose look is still loading (the stand-in shows meanwhile). */
	FName PendingLook;
	float PendingCheck = 0.f;
	float FishPhase = 0.f;
	float ReelAngle = 0.f;
	bool bLineShown = false;
	bool bStandIn = false;
	int32 Tier = 0;
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
	/** The basic float rod (the shop's others: rod_spinning, rod_sea). */
	static const FName RodId;
	/** Any of the rods. */
	static bool IsRod(FName Item);
	/** A line on how the fish are biting now and why (the dusk, the falling glass, a gale), for the angler's book. */
	static FText Forecast(const UObject* WorldContext);

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
	/** The rod a bait taken in the hand brings out: the one last used if it is still owned, else the best owned; none: no rod. */
	FName RodToUse() const;
	/** The kitchens that buy fish (fishing.json): the prompt and Y close by. */
	void Kitchens(float DeltaTime, APawn* Hero, APlayerController* PC);
	/** The boat at the pier, put on the water once the sea is there. */
	void LaunchBoat();
	void PutAway();
	void SyncBait();
	void UpdateWeights();
	/** Handles this step's events from index From on (Fly adds Landed / Ashore after the first pass). */
	void HandleEvents(int32 From = 0);
	/** The catch's showing in the hand ends (the fish went into the pocket at the catch). */
	void Bag();
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
	/** The rod in the hand now (one of the rods' items). */
	FName RodItem;
	float KitchenIn = 0.f;
	int32 KitchenNear = -1;
	uint64 KitchenKey = 0;
	bool bBoatTried = false;
	/** The food in each fish's reach from the web (Catchable index), refreshed now and then. */
	TArray<float> HungerOf;
	float HungerIn = 0.f;
	TWeakObjectPtr<ALyudFishingRod> Rod;
	/** The rod put away: hidden, kept for the next take-out (building one is ~20 meshes). */
	TWeakObjectPtr<ALyudFishingRod> Parked;
	/** Whose hand the rod is in (the hero stops being the player's pawn while a bird is flown: the rod is put away then,
	 *  and his hand let go). */
	TWeakObjectPtr<AHeroCharacter> HeldBy;
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
	/** What the prompt was built from (state, bait, tension step, the last words said): rebuilt only when it changes. */
	uint64 PromptKey = ~0ull;
	uint32 SayCount = 0;
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
	/** Where the fish was hooked (the web's cell it came from, the diver's fish it was), and the ground the cast fell on. */
	FVector HookPos = FVector::ZeroVector;
	float FeetZ = 0.f, DryZ = 0.f;
	bool bDryCast = false;
	/** Where the winding of an empty line began (distance, m; height, cm). */
	float WindFrom = 0.f, WindFromZ = 0.f;
	bool bWasWinding = false;
	/** "No rod" stays on the line until it runs out. */
	float NeedRodLeft = 0.f;
	/** The hand's pose now (look frame), following the state's. */
	FVector PosePos = FVector(40.f, 20.f, -28.f);
	float PoseElev = 30.f, PoseYaw = -4.f;
};
