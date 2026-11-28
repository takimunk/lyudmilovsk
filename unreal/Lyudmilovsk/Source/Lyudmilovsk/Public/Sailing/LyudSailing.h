// The yacht school as a part of the town (docs/yacht_school.md "Engine: the school"): ULyudSailing runs the school by the
// rules of Private/Sailing/SailSchoolModel.h on the data of Content/City/sailing.json, with the fleet's brain of
// Private/Sailing/SailFleetModel.h and the boats of LyudDinghy.h.
#pragma once

#include "CoreMinimal.h"
#include "LyudSave.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "Tickable.h"
#include "LyudSailing.generated.h"

class AActor;
class AHeroCharacter;
class ALyudDinghy;
class ALyudDoor;
class ALyudSailLaunch;
class ALyudSailMark;
class UAudioComponent;
class UInstancedStaticMeshComponent;
class ULyudSailingBook;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
struct FCityActInfo;
struct FLyudSailMorning;
struct FLyudSailNewsItem;
struct FLyudSailingState;
namespace LyudSail { struct FRaceCourse; struct FSchoolEvents; struct FSchoolV2; }

/**
 * The yacht school on the beach by the pier's root.
 *
 * It reads the calendar, the weather over the water and the tide every two seconds and asks the rules what is going on:
 * the season (laid up, fitting out, open, laying up), the session of the hour and its phase, whether the boats may go
 * afloat (decided once, at a session's start) and whether they must be recalled. From that it sets the people's
 * conditions (sail_season, sail_kids, sail_juniors, sail_adults, sail_race, sail_shore, sail_fitout, sail_prizes,
 * sail_watch), hoists the signal mast's flags, parks, rigs and covers the boats, and, while a session is on the water,
 * runs the fleet: each boat an ALyudDinghy sailed by a helmsman of the fleet's brain (LyudSail::FFleetBrain), the coach
 * in the launch, the marks of the drill or of the race. A regatta is run from the warning signal to the prize-giving and
 * written into the log; what the player does not see is resolved by numbers (LyudSail::SchoolResolveDay,
 * LyudSail::FleetResolveSession).
 *
 * The hero joins at the coach's desk, takes his lessons (LyudSail::FLessonRun shown on the boat's HUD), hires the Luch
 * number nine, enters the regattas; his boat is put afloat by the slip and he boards her with E. The log (ranks, miles,
 * races, stamps of the collection "sailing") is saved through ULyudSailingBook under the key "Sailing".
 *
 * The site (the boathouse, its things, the boat park, the slip's boards down to the water, the mast's flags) is built
 * once when the hero first comes within 400 m and hidden when he is far. Nothing is built or allocated per frame; boats
 * beyond 250 m are stepped on their polar a few times a second; with the hero farther than 1.5 km from the slip nothing
 * is stepped at all.
 *
 * Console: lyud.Sail 0|1, lyud.Sail.Off, lyud.Sail.Report, lyud.Sail.Season, lyud.Sail.Board, lyud.Sail.Lesson,
 * lyud.Sail.Race, lyud.Sail.Fleet, lyud.Sail.Capsize, lyud.Sail.Rank, lyud.Sail.Kit, lyud.Sail.Night. Developer mode:
 * group "sailing".
 */
UCLASS()
class LYUDMILOVSK_API ULyudSailing : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudSailing();
	virtual ~ULyudSailing() override;

	static ULyudSailing* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudSailing, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bLoaded; }

	/** The save (ULyudSailingBook calls these). Loading gives nothing to the pocket and tells no quest. */
	void WriteBook(ULyudSailingBook& Out) const;
	void ReadBook(const ULyudSailingBook& In);

	/** The pages of a board the game writes itself: "sail:notice" (the week's races and today's sessions) and
	 *  "sail:results" (the last race day's tables). False: not one of the school's. */
	bool BoardPages(const FString& Gen, TArray<FText>& Out) const;

	/** The news of a race day and the morning's facts (LyudSailNews.h; the paper and the radio ask through it). */
	void NewsOfDay(int32 DayNumber, TArray<FLyudSailNewsItem>& Out) const;
	bool MorningOf(int32 DayNumber, FLyudSailMorning& Out) const;

	/** What is going on, for the console and the log. */
	FString Report() const;

	// ---- developer mode and the console (LyudSailingDev.cpp)
	/** -1 the calendar's season, 0 laid up, 1 open (a summer day of the same weekday stands in for the date). */
	void DevSeason(int32 Mode);
	int32 DevSeasonNow() const;
	/** The hero aboard a boat of this class (0 optimist, 1 cadet, 2 luch) off the slip, now. */
	void DevBoard(int32 BoatClass);
	/** Starts a lesson by its id, or the next one ("next"); he is made a member if he is not. */
	bool DevLesson(const FString& Id);
	/** A race now for the school's fleet (kind: wl, triangle, kids; empty: by the classes afloat), the hero in it when he
	 *  is aboard. */
	void DevRace(const FString& CourseKind);
	/** A training session's fleet on the water now, at most N boats (0: everybody ashore). */
	void DevFleet(int32 N);
	/** The hero's boat (else the nearest one) goes over. */
	void DevCapsize();
	/** The hero's rank set by its id (none, yunga, helm3, helm2, helm1) with the lessons it stands for. */
	void DevRank(const FString& Id);
	/** The card, a life jacket (put on), gloves and a bailer into the pocket. */
	void GiveKit();
	/** The head coach's leave to sail after dusk. */
	void SetNightLeave(bool bOn);
	bool HasNightLeave() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	TPimplPtr<FLyudSailingState> M;
	bool bLoaded = false;
	bool bSwitchedOff = false;
	float FactsIn = 0.f, ConditionsIn = 0.f, SiteIn = 0.f;

	UPROPERTY(Transient) TObjectPtr<ULyudSailingBook> Book;
	/** The fleet's actors: a dinghy for every hull of the data (as FLyudSailingState::Hulls), the launch, the marks. */
	UPROPERTY(Transient) TArray<TObjectPtr<ALyudDinghy>> Dinghies;
	UPROPERTY(Transient) TObjectPtr<ALyudSailLaunch> LaunchBoat;
	UPROPERTY(Transient) TArray<TObjectPtr<ALyudSailMark>> MarkPool;
	/** The site: one actor that holds the boathouse and its things, the doors beside it. */
	UPROPERTY(Transient) TObjectPtr<AActor> SiteActor;
	UPROPERTY(Transient) TArray<TObjectPtr<UObject>> Kept;
	UPROPERTY(Transient) TArray<TObjectPtr<ALyudDoor>> SiteDoors;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> MastFlags;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> DoorLeaves;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> SiteLamps;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> SlipBoards;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> MastFrame;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> HalyardLoop;
	bool bSiteBuilt = false, bSiteShown = false, bFleetMade = false, bLampsOn = false, bLeavesOpen = false;
	TArray<FVector> LeafClosed, LeafOpen;
	FVector MastPoints[3] = { FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector };

	// ---- LyudSailing.cpp: the clock's facts, the people, the hero, the save
	bool LoadData();
	void StartOver();
	void SwitchOff();
	void RefreshFacts();
	void PushConditions();
	void DayTurned(long long PrevDay);
	bool OnCityAct(const FCityActInfo& Info, FText& Line);
	/** Everything the rules say happened to the hero: the stamps, the quest events, a new rank's word. */
	void Emit(const LyudSail::FSchoolEvents& Ev);
	void Say(const TCHAR* Key);
	void SayWith(const TCHAR* Key, const FString& Arg);
	bool HeroHasJacket() const;
	/** The hero's boat afloat by the slip, waiting for him (false: no fleet, no water). */
	bool PutHeroBoatOut(bool bHire);
	/** His outing is over (he stepped out by the slip, or the launch brought her in): the log, the boat back to the park. */
	void HeroAshore(bool bTowedIn);
	void TickHero(AHeroCharacter* Hero, float Dt);
	bool StartLesson(int32 Lesson);
	void EndLesson(bool bDone);
	void LessonObjectiveSet();
	void TickLesson(float Dt);
	void HeroHud();
	void ClearPrompt();
	ALyudDinghy* HeroDinghy() const;
	bool NightSailable() const;

	// ---- LyudSailingSite.cpp: the school on the beach
	void TickSite(const FVector& Eye, float Dt);
	void BuildSite();
	void BuildStandIn(const FTransform& House);
	void ShowSite(bool bShow);
	void LaySlip();
	void HoistSignals();
	void SiteHours();

	// ---- LyudSailingFleet.cpp: the boats, the helmsmen, the launch
	bool MakeFleet();
	void StowFleet();
	void ParkHulls(bool bForce);
	void TickFleet(const FVector& Eye, float Dt, bool bNear);
	void WaterControl(float Dt);
	void BeginWater(bool bRace);
	void EndWater();
	bool LaunchNextUnit();
	void FleetLod(const FVector& Eye);
	void FleetEvents();
	void UnitLanded(int32 Unit);
	void ResolveUnwatched();
	void LaunchOut(bool bOut);
	float GroundZ(const LyudSail::FSchoolV2& At) const;
	FTransform BeachTransform(const LyudSail::FSchoolV2& At) const;
	/** A mark of the pool put on the water (Kind as ALyudSailMark::Setup), or the pool's marks from First stowed. */
	void PlaceMark(int32 Index, int32 MarkKind, const LyudSail::FSchoolV2& At);
	void StowMarks(int32 First, int32 Last);
	int32 LayCourse(int32 First, const LyudSail::FRaceCourse& Course, bool bBoatEnd);

	// ---- LyudSailingRace.cpp: a regatta from the warning signal to the prizes
	void TickRace(float Dt);
	bool StartRace();
	void RaceClosed(bool bAbandoned);
	void RaceWatchers();
	void RaceHud();
};

/** The yacht school's record in the save (key "Sailing"): the hero's log and the engine's own memory, as text. */
UCLASS()
class LYUDMILOVSK_API ULyudSailingBook : public UObject, public ILyudSaveable
{
	GENERATED_BODY()

public:
	/** LyudSail::FSchoolLog::Save. */
	UPROPERTY(SaveGame) FString Journal;
	/** The hero's boat afloat and where, today's sessions as decided, what the members have learned, the coach's leave. */
	UPROPERTY(SaveGame) FString Memory;

	TWeakObjectPtr<ULyudSailing> Sailing;

	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;
};
