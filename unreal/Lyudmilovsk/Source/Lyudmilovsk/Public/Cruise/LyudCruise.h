#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "LyudCruise.generated.h"

class SLyudCityPage;
class ULyudSaveGame;
struct FCityActInfo;
namespace LyudCruise { struct FHero; }

/** What the motor ship «Полдень» is doing (LyudCruise::EPhase of Private/Cruise/CruiseModel.h, the same order). */
UENUM()
enum class ELyudCruisePhase : uint8
{
	/** Out of season (16 November .. 14 April): laid up, not shown. */
	Laid,
	/** At anchor on the roads off the town between voyages. */
	Port,
	/** Friday 16:00-18:00: the launch runs from the pier head. */
	Boarding,
	Underway,
	/** Round an island (Bird Island, the Far Light). */
	Circling,
	/** The night anchorage in Long Island's bay. */
	Anchored,
	/** Sunday 18:00-20:00, back on the roads: the launch takes people ashore. */
	Landing,
};

/** The voyage now. A plain struct, refreshed every tick (ULyudCruise::View). */
struct FLyudCruiseView
{
	ELyudCruisePhase Phase = ELyudCruisePhase::Laid;
	bool bHeroAboard = false;
	/** 1..3 (Friday, Saturday, Sunday) from the boarding to the end of the landing; 0 in port. */
	int32 Day = 0;
	/** The ship on the chart, metres (the map's frame carried out to sea: X west, Y north). */
	FVector2D ChartPosM = FVector2D::ZeroVector;
	/** The chart heading, atan2(dY, dX) of the bow, in (-pi, pi]: an angle, compare it as one (it wraps). */
	float HeadingRad = 0.f;
	/** Through the water, m/s; 0 at anchor. */
	float SpeedMS = 0.f;
	/** rad/s, + to starboard (the heading, which is the Unreal yaw at the stage's frame, growing); smoothed over 40 s, never a step. */
	float TurnRateRad = 0.f;
	/** Since Friday 18:00 of the voyage of this week; < 0 before it sails. */
	double VoyageSeconds = 0.0;
};

/**
 * The motor ship's two-day cruise (docs/ship.md). The voyage is a pure function of the game calendar (the engine-free
 * LyudCruise::FModel, Private/Cruise/CruiseModel.h, data Content/Cruise/cruise.json from tools/ship/cruise.py, checked by
 * tools/cruisesim/run.sh): the season, the weekly voyage, where the ship is on the chart, what the programme says. The hero
 * goes through it aboard a furnished ship that stands still at the stage far out at sea (the city plot P-SHIP: every thing
 * aboard is a city thing); ALyudCruiseSeascape moves the islands past by View().
 *
 * This subsystem polls the calendar (a clock jump needs no notice), sets the city's flags (cruise_*: ULyudCityThings::SetFlag)
 * and the minutes of the acts that wait for the next event ("until": "cruise_event"), fires OnCruiseEvent, says the ship's
 * radio announcements as subtitles, takes the hero aboard when he comes through the launch gate with a ticket, puts him
 * ashore when the voyage is over (also after a load or a long sleep) and fishes him out of the sea. Saved as "Cruise".
 *
 * Console: lyud.Cruise 0|1|2, lyud.Cruise.Report, lyud.Cruise.Clock <seconds>, lyud.Cruise.Board, lyud.Cruise.Next,
 * lyud.Cruise.Jump <event id>, lyud.Cruise.Ashore, lyud.Cruise.BridgePass. Developer mode: group "cruise".
 */
UCLASS()
class LYUDMILOVSK_API ULyudCruise : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudCruise();
	virtual ~ULyudCruise() override;

	static ULyudCruise* Get(const UObject* WorldContext);

	// --- the contract of docs/ship.md (W5 the seascape, W6 the band, W7 poker, W10 people read these)

	const FLyudCruiseView& View() const { return Seen; }
	/** Is a programme event on now ("concert1", "isl_birds", ...)? False for an unknown id and while the cruise is off. */
	bool IsEventOn(FName EventId) const;
	/** Seconds to the next programme event's start (of this voyage, else the next voyage's boarding); < 0: none (off). */
	double SecondsToNextEvent(FName* OutId = nullptr) const;
	/** Ship frame (cm; x to the bow from the stern, y to starboard, z up from the waterline) -> world, at the stage. */
	FTransform StageTransform() const { return Stage; }
	/** The same for the visual ship at anchor on the roads (it does not move with the tide: z is the sea datum). */
	FTransform RoadsTransform() const { return Roads; }
	/** A named point in world cm at the stage: a point of cruise.json "points" ("bow", "funnel_top", "aft_deck", ...) or a thing
	 *  of the plot P-SHIP by its id ("bandstand", "stern_rail", "card_table", "gangway", ...). False: unknown. */
	bool GetShipPoint(FName Name, FVector& OutWorld) const;
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCruiseEvent, FName /*event id: started*/);
	FOnCruiseEvent OnCruiseEvent;

	// --- more of the same kind

	/** The cruise runs in this world (the data was read). Everything above is safe to call when it does not. */
	bool IsRunning() const { return bStarted; }
	bool IsHeroAboard() const { return bAboard; }
	static bool IsHeroAboard(const UObject* WorldContext);
	/** The hero's class aboard: "hold", "std", "lux"; None when he is not aboard. */
	FName HeroClass() const;
	/** His cabin's number (15, 7, 11); 0 when he is not aboard. */
	int32 HeroCabin() const { return bAboard ? HeroCabinNo : 0; }
	/** Voyages he sailed to their end (saved). */
	int32 CruisesDone() const { return CruisesSailed; }
	/** World cm -> ship frame cm at the stage, and is that point inside the hull's outline (any deck)? */
	FVector ToShipFrame(const FVector& World) const { return Stage.InverseTransformPosition(World); }
	bool IsInsideHull(const FVector& World, float MarginCm = 0.f) const;
	/** The ship's radio (or anybody aboard) says a line as a subtitle for Seconds; a later line replaces it. */
	void Say(const FText& Speaker, const FText& Line, float Seconds = 7.f);
	/** Seconds since 1 January 2026 00:00 of the game calendar (+ lyud.Cruise.Clock): what the model is asked with. */
	double NowSeconds() const;
	FString Report() const;

	// --- developer mode and the console

	/** A ticket in the pocket, the clock at the next boarding's Friday 17:30, the hero at the pier's launch gate. */
	void DevBoardNow();
	/** The clock on to five seconds before the next programme event; false: none. */
	bool DevNextEvent();
	/** The clock to five seconds before that event of the voyage on now (else of the next one); false: no such event. */
	bool DevJumpTo(FName EventId);
	/** Ashore at once (as at the voyage's end). */
	void DevAshore();
	/** The landing at the Far Light: the clock at Saturday 16:40 of the voyage, the hero on the rock's jetty. */
	void DevLanding();

	// --- the landing at the Far Light (docs/ship.md): the real rock is the city building of plot P-FARLIGHT

	/** Is the real rock shown (and solid) now? Then the seascape hides its far-view copy of the lighthouse island: the two
	 *  stand in the same place while the ship lies at the landing stop. */
	bool IsRealRockShown() const;
	/** Is the hero walking the rock (within 170 m of its chart point, not aboard)? */
	bool IsHeroOnRock() const { return bHeroOnRock; }
	/** The rock's chart point in the world (cm, at the sea's datum): the stage ship's midship point + the model's offset. */
	FVector RockWorld() const;
	/** The wheelhouse opens to him for today (the city's stay "cruise_bridge": the bridge tour, the captain's invitation). */
	void GiveBridgePass();

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bStarted; }

protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// the save record "Cruise"
	UPROPERTY(SaveGame) bool bAboard = false;
	/** 0 hold, 1 std, 2 lux. */
	UPROPERTY(SaveGame) int32 HeroClassIndex = 1;
	UPROPERTY(SaveGame) int32 HeroCabinNo = 0;
	/** The voyage he boarded (LyudCruise::FModel::VoyageAt). */
	UPROPERTY(SaveGame) int64 HeroVoyage = -1;
	UPROPERTY(SaveGame) int32 CruisesSailed = 0;

private:
	/** The model, the flags as sent, the queue of announcements, a move under way (LyudCruise.cpp). */
	struct FWork;

	bool Start();
	/** The model's answer for now into Seen; the events that began since the last look are fired and announced. */
	void Step(double Now);
	void FireEvent(int32 Index, bool bAnnounce);
	/** A few times a second: the boarding, the end of the voyage, the sea, the flags, the wait minutes. */
	void Poll(float DeltaTime);
	void PushFlags(double Now, bool bAll);
	void Board(double Now);
	/** His voyage is over in the books: the key handed in, the stays ended, the quests told. */
	void FinishAshore(double Now);
	/** A fade that takes the hero to a thing (or, when the city cannot, to a fallback place); Kind: 1 ashore, 2 overboard, 3 fetched from
	 *  the Far Light's rock to the gangway, 4 out of the water onto the rock's jetty. */
	void BeginMove(int32 Kind, const TCHAR* Thing, const FVector& Fallback, float FallbackYaw, bool bFallbackGround, const TCHAR* SpeakerKey, const TCHAR* LineKey);
	void TickMove();
	void TickSubtitle();
	void Unsay();
	LyudCruise::FHero HeroState() const;
	void TakeHeroState(const LyudCruise::FHero& H);
	void SetClock(double Seconds);
	void OnSaveLoaded(ULyudSaveGame* Save);
	bool OnCityAct(const FCityActInfo& Info, FText& Line);

	TPimplPtr<FWork> Work;
	bool bStarted = false;
	FLyudCruiseView Seen;
	FTransform Stage = FTransform::Identity;
	FTransform Roads = FTransform::Identity;
	/** cruise.json "points", world cm at the stage; the things of the plot P-SHIP by their ids (refreshed when the city's data comes). */
	TMap<FName, FVector> Points;
	TMap<FName, FVector> ThingPoints;
	/** The hull's outline for IsInsideHull: (x, half beam) pairs, ship frame cm, x growing. */
	TArray<FVector2D> HullPlan;
	float HullLengthCm = 6200.f;
	float HullHalfBeamCm = 500.f;
	/** Where the hero is put ashore when the city has no launch gate (world cm, dropped onto the ground; Unreal degrees). */
	FVector PierFallback = FVector::ZeroVector;
	float PierFallbackYaw = 180.f;
	/** The Far Light's jetty (world cm, the capsule's middle) when the city has no jetty gate; whether the hero is on the rock. */
	FVector JettyFallback = FVector::ZeroVector;
	float JettyFallbackYaw = 0.f;
	bool bHeroOnRock = false;

	float PollClock = 0.f;
	TSharedPtr<SLyudCityPage> Page;
	double PageUntil = 0.0;
};
