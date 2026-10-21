#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/Interface.h"
#include "LyudSave.generated.h"

/**
 * Saving and loading the game in named slots (USaveGame + UGameplayStatics, so it works on every platform).
 *
 * What gets saved:
 *  - the game clock (ULyudSaveSubsystem::GetGameTimeSeconds), the level, the player's position and view;
 *  - every actor in the level that implements ILyudSaveable: its UPROPERTY(SaveGame) fields, keyed by actor name;
 *  - every object registered with RegisterSaveObject (subsystems, managers): its UPROPERTY(SaveGame) fields;
 *  - anything a system writes itself in OnBeforeSave (ULyudSaveGame::Records) and reads back in OnAfterLoad.
 * Loading reopens the saved level, then restores the above on the first tick after BeginPlay.
 */

USTRUCT()
struct FLyudSaveRecord
{
	GENERATED_BODY()

	/** UPROPERTY(SaveGame) fields as tagged properties: fields added or removed later load fine. */
	UPROPERTY()
	TArray<uint8> Data;
};

UINTERFACE(MinimalAPI, BlueprintType)
class ULyudSaveable : public UInterface
{
	GENERATED_BODY()
};

/** An actor (or registered object) whose UPROPERTY(SaveGame) fields go into the save. */
class LYUDMILOVSK_API ILyudSaveable
{
	GENERATED_BODY()

public:
	/** Just before the SaveGame fields are written: copy any runtime state into them. */
	UFUNCTION(BlueprintNativeEvent, Category = "Save")
	void OnGameSaving();
	virtual void OnGameSaving_Implementation() {}

	/** The SaveGame fields have just been restored: make the world match them (snap, no animation). */
	UFUNCTION(BlueprintNativeEvent, Category = "Save")
	void OnGameLoaded();
	virtual void OnGameLoaded_Implementation() {}
};

UCLASS()
class LYUDMILOVSK_API ULyudSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Version = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString Title;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FDateTime SavedAt;

	/** Long package name of the level, without the PIE prefix, e.g. /Game/Scene01/Lvl_Scene01. */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString Map;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	double GameTimeSeconds = 0.0;

	UPROPERTY()
	bool bHasPlayer = false;

	UPROPERTY()
	FTransform PlayerTransform;

	UPROPERTY()
	FRotator ControlRotation = FRotator::ZeroRotator;

	/** ILyudSaveable actors of the level, by actor name. */
	UPROPERTY()
	TMap<FName, FLyudSaveRecord> Actors;

	/** Objects registered with RegisterSaveObject and free-form records written in OnBeforeSave, by key. */
	UPROPERTY()
	TMap<FName, FLyudSaveRecord> Records;

	/** Writes Object's UPROPERTY(SaveGame) fields into Records[Key]. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	void WriteObject(FName Key, UObject* Object);

	/** Restores Object's UPROPERTY(SaveGame) fields from Records[Key]; false if the save has no such record. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool ReadObject(FName Key, UObject* Object) const;

	static void Store(UObject* Object, FLyudSaveRecord& Out);
	static void Restore(UObject* Object, const FLyudSaveRecord& In);
};

/** One slot as the menu lists it. */
USTRUCT(BlueprintType)
struct FLyudSlotInfo
{
	GENERATED_BODY()

	/** The UGameplayStatics slot name. */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString Slot;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString Title;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FDateTime SavedAt;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	double GameTimeSeconds = 0.0;
};

/** The list of slots, itself a save slot: not every platform can enumerate save files. */
UCLASS()
class LYUDMILOVSK_API ULyudSlotIndex : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FLyudSlotInfo> Slots;

	UPROPERTY()
	int32 NextId = 1;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FLyudSaveEvent, ULyudSaveGame* /*Save*/);

UCLASS()
class LYUDMILOVSK_API ULyudSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static ULyudSaveSubsystem* Get(const UObject* WorldContext);

	/** Seconds of play since the new game began: runs while the game is unpaused, saved and restored with the slot. */
	UFUNCTION(BlueprintPure, Category = "Save")
	double GetGameTimeSeconds() const;

	/** Slots, newest first. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	TArray<FLyudSlotInfo> GetSlots() const;

	/** Saves the current game. Empty Slot makes a new slot; an existing one is overwritten. Returns the slot, empty on failure. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	FString SaveToSlot(const FString& Title, const FString& Slot = TEXT(""));

	/** Reopens the slot's level and restores the slot into it. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool LoadSlot(const FString& Slot);

	UFUNCTION(BlueprintCallable, Category = "Save")
	void DeleteSlot(const FString& Slot);

	/** Reopens the current level fresh, with the clock at zero. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	void NewGame();

	/** Registers a non-actor object (a subsystem, a manager) whose UPROPERTY(SaveGame) fields are saved under Key. */
	void RegisterSaveObject(FName Key, UObject* Object);
	void UnregisterSaveObject(FName Key);

	/** Before a save is written; systems may add their own records to it. */
	FLyudSaveEvent OnBeforeSave;

	/** After a save has been restored into the world. */
	FLyudSaveEvent OnAfterLoad;

	/** Called by the menu subsystem: the world has begun play / is going away. */
	void NotifyWorldBeginPlay(UWorld& World);
	void NotifyWorldEnding(UWorld& World);

	/** The start-up main menu is shown once per run, not after every level reload. */
	bool bMainMenuShown = false;

	/** A level reload for a load or a new game is under way. */
	bool IsTravelling() const { return bTravelling; }

private:
	ULyudSlotIndex* LoadIndex() const;
	void SaveIndex(ULyudSlotIndex* Index) const;
	void ApplyPending(TWeakObjectPtr<UWorld> World);
	static FString MapOf(const UWorld* World);

	UPROPERTY()
	TObjectPtr<ULyudSaveGame> Pending;

	UPROPERTY()
	TMap<FName, TWeakObjectPtr<UObject>> SaveObjects;

	/** Game time accumulated before the current world, minus anything a load subtracted. */
	double ClockBase = 0.0;
	bool bPendingNewGame = false;
	bool bTravelling = false;
};
