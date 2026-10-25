#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LyudQuests.generated.h"

/**
 * Quests and the journal (docs/ui.md).
 *
 * The quests are data: Content/UI/quests.json lists them, each a chain of stages, each stage a few objectives. An objective
 * waits for a game event (Event + Target, Need times): any system reports events with ULyudQuests::Notify(this, "talk", "owner").
 * Events the game sends today:
 *   talk <npc id>              a conversation with someone started (NpcDialogue)
 *   leave_computer             the hero got up from the club computer (AtonComputer)
 *   reach <place id>           the hero (or the bird he became) came within a place's radius (Content/UI/map.json)
 *   get <item id>              an item went into the pocket (ULyudInventory::Add; N = how many)
 *   use <item id>              one was used up: drunk, eaten, a handful of feed given (ULyudInventory::Remove)
 *   wear <item id>             something was put on or put in the hand
 *   bird                       the hero became a bird (any pawn that is not the hero)
 * A Target ending in '*' matches by prefix (drink_* is any soda). The words are keys (localization skill):
 *   quest.<id>.title, quest.<id>.desc, quest.<id>.<stage>.log (the journal entry when the stage begins),
 *   quest.<id>.<objective>.obj (the line in the journal and on the HUD), quest.<id>.done (the entry when the quest ends).
 * Progress, the journal entries, the places found and the tracked quest go into every save ("Quests").
 */

UENUM()
enum class ELyudQuestState : uint8
{
	Inactive,
	Active,
	Done,
};

/** One thing to do: Need events Event with Target. Place (a map.json place) puts a marker on the map. */
struct FLyudQuestObjective
{
	FName Id;
	FName Event;
	FString Target;
	int32 Need = 1;
	FName Place;
};

/** One step of a quest: all its objectives done, the next stage begins (or the quest ends after the last). */
struct FLyudQuestStage
{
	FName Id;
	TArray<FLyudQuestObjective> Objectives;
	/** Given when the stage is done. */
	TArray<TPair<FName, int32>> GiveItems;
	int32 GiveRubles = 0;
	TArray<FName> StartQuests;
};

struct FLyudQuestDef
{
	FName Id;
	/** main or side: the main quests are listed first and tracked by default. */
	bool bMain = false;
	/** Starts at a new game. */
	bool bAutoStart = false;
	/** Otherwise starts on this event (empty: only by another quest's StartQuests or Start()). */
	FName StartEvent;
	FString StartTarget;
	TArray<FLyudQuestStage> Stages;
};

USTRUCT()
struct FLyudQuestProgress
{
	GENERATED_BODY()

	UPROPERTY(SaveGame) FName Quest;
	UPROPERTY(SaveGame) ELyudQuestState State = ELyudQuestState::Inactive;
	UPROPERTY(SaveGame) int32 Stage = 0;
	/** Events counted so far, one per objective of the current stage. */
	UPROPERTY(SaveGame) TArray<int32> Counts;
	/** The calendar date it began and ended (UGameCalendar). */
	UPROPERTY(SaveGame) FDateTime Started;
	UPROPERTY(SaveGame) FDateTime Finished;
};

/** One line of the journal: Key's text with Args filled in (an arg starting with @ is a key itself, shown in the current language). */
USTRUCT()
struct FLyudJournalEntry
{
	GENERATED_BODY()

	UPROPERTY(SaveGame) FString Key;
	UPROPERTY(SaveGame) TArray<FString> Args;
	UPROPERTY(SaveGame) FDateTime When;
	/** The quest it belongs to (none: the hero's own note, a place found). */
	UPROPERTY(SaveGame) FName Quest;

	FText Text() const;
};

/** A place on the map (Content/UI/map.json, written by tools/ui_map.py from tools/world_map.py): metres in the map frame. */
struct FLyudMapPlace
{
	FName Id;
	FVector2D At = FVector2D::ZeroVector;
	/** Found when the hero comes this close (0: not found by walking). */
	float Radius = 0.f;
	/** town, nature, water, building, transport: how the map draws its name. */
	FName Kind;
	/** Known from the start (the club, the sea). */
	bool bKnown = false;
};

/** The map frame (docs/world_map.md): X west, Y north, metres from the club's portal face. */
struct FLyudMapFrame
{
	double XMin = -1300, XMax = 1300, YMin = -900, YMax = 2200;
	/** Unreal Y of the portal face (cm). */
	double PortalFaceY = 790.0;

	FVector2D FromWorld(const FVector& P) const { return FVector2D(P.X / 100.0, (P.Y - PortalFaceY) / 100.0); }
	FVector ToWorld(const FVector2D& M, double Z = 0.0) const { return FVector(M.X * 100.0, PortalFaceY + M.Y * 100.0, Z); }
};

UENUM()
enum class ELyudQuestNews : uint8
{
	Started,
	Stage,
	Done,
	Objective,
	Place,
};

UCLASS()
class LYUDMILOVSK_API ULyudQuests : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static ULyudQuests* Get(const UObject* WorldContext);

	/** Reports a game event to the quests (safe to call from anywhere, any time). */
	static void Notify(const UObject* WorldContext, FName Event, const FString& Target = FString(), int32 N = 1);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(SaveGame) TArray<FLyudQuestProgress> Progress;
	UPROPERTY(SaveGame) TArray<FLyudJournalEntry> Entries;
	UPROPERTY(SaveGame) TArray<FName> Found;
	UPROPERTY(SaveGame) FName Tracked;

	/** A new game: nothing done, the auto-start quests begun. */
	void Reset();

	void HandleEvent(FName Event, const FString& Target, int32 N);
	bool Start(FName Quest);
	/** Finishes the current stage of a quest at once (dev command Quest.Advance). */
	void Advance(FName Quest);

	/** The hero is at P (world cm): finds places and reports reach events. Called by the game screen every few frames. */
	void UpdatePosition(const UObject* WorldContext, const FVector& P);

	const FLyudQuestProgress* FindProgress(FName Quest) const;
	ELyudQuestState State(FName Quest) const;
	bool IsFound(FName Place) const { return Found.Contains(Place); }
	/** The quest the HUD shows: the tracked one if it is active, else the first active main quest, else any active. */
	FName ShownQuest() const;
	/** The current stage's objectives with their counts (empty when not active). */
	TArray<TPair<const FLyudQuestObjective*, int32>> CurrentObjectives(FName Quest) const;

	static const TArray<FLyudQuestDef>& Defs();
	static const FLyudQuestDef* FindDef(FName Quest);
	static const TArray<FLyudMapPlace>& Places();
	static const FLyudMapPlace* FindPlace(FName Place);
	static const FLyudMapFrame& Frame();
	/** Drops the parsed quests.json / map.json (re-read at every level start, like the strings). */
	static void ReloadData();

	static FText QuestTitle(FName Quest);
	static FText ObjectiveText(FName Quest, const FLyudQuestObjective& O, int32 Count);
	static FText PlaceName(FName Place);

	/** Something the HUD should tell: a quest begun, a stage reached, a quest done, an objective done, a place found. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FNews, ELyudQuestNews /*What*/, FName /*Quest or place*/);
	FNews OnNews;
	DECLARE_MULTICAST_DELEGATE(FChanged);
	FChanged OnChanged;

private:
	void BeginStage(FLyudQuestProgress& P, const FLyudQuestDef& Def);
	void FinishStage(FLyudQuestProgress& P, const FLyudQuestDef& Def);
	void AddEntry(const FString& Key, FName Quest, const TArray<FString>& Args = TArray<FString>());
	FDateTime Today() const;
	FLyudQuestProgress& ProgressOf(FName Quest);

	/** Events arriving while one is being handled (a reward that is an item) wait here. */
	TArray<TTuple<FName, FString, int32>> Queue;
	bool bHandling = false;
	/** Places the hero is in now (reach is reported on the way in). */
	TSet<FName> Inside;
};
