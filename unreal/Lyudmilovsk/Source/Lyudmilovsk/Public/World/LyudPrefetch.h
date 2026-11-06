#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudPrefetch.generated.h"

/**
 * Loads assets in the background before the game needs them. A LoadObject of an asset not yet in memory stops the
 * game thread until its package is read (FlushAsyncLoading): people, cats, dogs and bird species coming into view did
 * that dozens of times in the first minutes of play, each a visible hitch. Callers ask Ready() each frame and spawn
 * the body only once it says yes; the load meanwhile runs off the game thread. What was loaded is held until the
 * world ends (it would be collected before it is used otherwise). lyud.Prefetch 0 always answers yes (the old
 * synchronous loads, A/B).
 */
UCLASS()
class LYUDMILOVSK_API ULyudPrefetch : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

	/** True when the object (full path, /Game/X/SK_X.SK_X) is in memory or doesn't exist; otherwise starts loading its
	 *  package in the background (once) and returns false. */
	static bool Ready(const UObject* WorldContext, const FString& ObjectPath);
	/** Ready() for every asset in a content folder and its subfolders whose name doesn't contain Skip. */
	static bool FolderReady(const UObject* WorldContext, const FString& Folder, const TCHAR* Skip = nullptr);

	int32 NumLoading() const { return Loading.Num(); }

private:
	bool ReadyPackage(FName Package, const FString& ObjectPath);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> Held;

	TSet<FName> Loading;
	TSet<FName> Missing;
	struct FFolder
	{
		TArray<FString> Paths;     // object paths, from the asset registry
		TArray<FName> Packages;    // their packages, made once
		bool bReady = false;       // everything was there once: held since, so no more checks
	};
	TMap<FString, FFolder> Folders;
};
