#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "Templates/Function.h"

class UWorld;

/**
 * Developer mode (docs/dev_mode.md, skill .claude/skills/dev-mode): the F7 panel's tabs and the Dev.* console commands.
 *
 * Every feature hooks itself in here so it can be tested in seconds, without playing up to it:
 *
 *  - Teleport points: as data, one file per feature, Content/Dev/teleports/<feature>.json (preferred, no code):
 *        {"teleports": [{"id": "north_hut", "group": "north", "name": "dev.tp.north_hut", "x": 12, "y": 2400, "yaw": 180}]}
 *    x, y are map metres (docs/world_map.md frame, like Content/UI/map.json) or "place": "<map.json id>"; "z" (world cm) is optional,
 *    without it the point is dropped onto whatever is solid below; "time": "HH:MM" sets the clock on arrival.
 *    Or in C++ (a point that moves: a station, an NPC, a bike):
 *        static LyudDev::FAutoTeleport GTp(TEXT("orni_now"), TEXT("ornithologist"), TEXT("dev.tp.orni_now"),
 *            [](UWorld* W, FTransform& At) { ...; return true; });
 *    The city's plots, squares and promenade shops (Content/City/city_plan.json) are listed on their own, groups city_*.
 *  - Actions (a button in the Actions tab and console Dev.Do <id>):
 *        static LyudDev::FAutoAction GErupt(TEXT("north.erupt"), TEXT("north"), TEXT("dev.action.north.erupt"),
 *            [](UWorld* W) { ... });
 *    A toggle gives a getter too (the button lights while it is on): FAutoAction(Id, Group, Name, Run, IsOn).
 *  - Quest stages: jumps come for free from Content/UI/quests.json. To make a jump land in a playable state, give the stage
 *    a "dev" block in quests.json ({"tp": "<teleport id>", "give": {"bird_feed": 5}, "time": "06:00", "exec": ["console cmd"]})
 *    or, for what needs code, a stage setup:
 *        static LyudDev::FAutoQuestSetup GSetup(TEXT("ornithologist"), TEXT("lure"), [](UWorld* W) { ... });
 *
 * Name is a string key (localization skill: ru, en, ja); a key missing from the tables shows the id. Registration is static
 * (like FAutoConsoleCommand), so it works from any .cpp with no init order to mind. Everything here is cheap: nothing ticks.
 */
namespace LyudDev
{
	using FRun = TFunction<void(UWorld*)>;
	using FIsOn = TFunction<bool(UWorld*)>;
	using FWhere = TFunction<bool(UWorld*, FTransform&)>;

	struct FAction
	{
		FName Id;
		FName Group;
		FString Name;
		FRun Run;
		FIsOn IsOn;
	};

	struct FTeleport
	{
		FName Id;
		FName Group;
		/** A string key; empty: the map place's name (Place) or the id. */
		FString Name;
		/** Text shown as is when there is no key (the city plan's own names, chosen by the language when read). */
		FString Label;
		/** Fixed points: map metres (or Place), Z world cm of the feet (unset: onto the ground). Where gives the feet too. */
		FName Place;
		FVector2D Map = FVector2D::ZeroVector;
		TOptional<double> Z;
		float Yaw = 0.f;
		float Pitch = 0.f;
		/** "HH:MM": the clock is set on arrival (empty: left alone). */
		FString Time;
		/** Points that move: worked out at the moment of the jump (false: not there now). */
		FWhere Where;
		/** Where it came from (a file name, "code", "map", "bookmark"), for the list and the logs. */
		FString Source;
	};

	/** Adds or replaces (same id) an action. */
	LYUDMILOVSK_API void AddAction(const FAction& Action);
	/** Adds or replaces (same id) a teleport point. */
	LYUDMILOVSK_API void AddTeleport(const FTeleport& Teleport);
	/** Code run after a jump to a quest stage (after the stage's "dev" block). */
	LYUDMILOVSK_API void AddQuestSetup(FName Quest, FName Stage, FRun Run);

	/** Every action, sorted by group then id. */
	LYUDMILOVSK_API TArray<const FAction*> Actions();
	LYUDMILOVSK_API const FAction* FindAction(FName Id);
	/** Every teleport point: map.json places, Content/Dev/teleports/<feature>.json, code, bookmarks; sorted by group then name. */
	LYUDMILOVSK_API TArray<FTeleport> Teleports();
	LYUDMILOVSK_API bool FindTeleport(FName Id, FTeleport& Out);

	/** Runs an action (false: no such id). Logs "DEV do <id>". */
	LYUDMILOVSK_API bool Do(UWorld* World, FName Id);
	/** Moves the player (hero, or the bird he is) to a point. Logs "DEV tp <id> at x y z". */
	LYUDMILOVSK_API bool Teleport(UWorld* World, const FTeleport& To);
	LYUDMILOVSK_API bool TeleportTo(UWorld* World, FName Id);
	/** To a world location (cm, Z where the feet go); bGround: dropped onto whatever is solid below X, Y. */
	LYUDMILOVSK_API bool TeleportWorld(UWorld* World, const FVector& At, float Yaw, bool bGround, float Pitch = 0.f);

	/** Makes Quest active at stage Stage (its index), as if the stages before were done, then sets the stage up ("dev" block, setups). */
	LYUDMILOVSK_API bool JumpToStage(UWorld* World, FName Quest, int32 Stage);
	/** Fires the real game event of one objective of the current stage, as many times as it needs (tests the wiring too). */
	LYUDMILOVSK_API bool CompleteObjective(UWorld* World, FName Quest, int32 Objective);

	/** Plays a quest to its end from where it is (starting it if needed): every objective of every stage by its real event, logging
	 *  "DEV play <quest> stage <id>" per stage and "DEV play <quest> done|STUCK". False if it did not finish (an objective whose event
	 *  does not count: a broken target, a missing item id). */
	LYUDMILOVSK_API bool PlayThrough(UWorld* World, FName Quest);

	/** The player's place as a line to paste: "Dev.TpAt <x> <y> <z> <yaw>" (world cm) plus map metres. Also copied to the clipboard. */
	LYUDMILOVSK_API FString Where(UWorld* World, bool bCopy);
	/** Saves the player's place as a bookmark (Saved/Dev/bookmarks.json, not in git); empty name: "bm_<n>". */
	LYUDMILOVSK_API FName AddBookmark(UWorld* World, const FString& Name);
	LYUDMILOVSK_API void RemoveBookmark(FName Id);

	/** The text for a registered name key (the id when the key is not in the tables). */
	LYUDMILOVSK_API FText NameOf(const FString& Key, FName Id);

	/** Re-reads Content/Dev/teleports/<feature>.json and the quests' dev blocks (done at every level start too). */
	LYUDMILOVSK_API void Reload();

	/** Whether developer mode is allowed in this build: always outside Shipping; in Shipping with -dev on the command line. */
	LYUDMILOVSK_API bool Enabled();

	/** Static registrars: put one at file scope in your feature's .cpp. */
	struct LYUDMILOVSK_API FAutoAction
	{
		FAutoAction(const TCHAR* Id, const TCHAR* Group, const TCHAR* NameKey, FRun Run, FIsOn IsOn = nullptr);
	};
	struct LYUDMILOVSK_API FAutoTeleport
	{
		FAutoTeleport(const TCHAR* Id, const TCHAR* Group, const TCHAR* NameKey, FWhere Where);
	};
	struct LYUDMILOVSK_API FAutoQuestSetup
	{
		FAutoQuestSetup(const TCHAR* Quest, const TCHAR* Stage, FRun Run);
	};
}
