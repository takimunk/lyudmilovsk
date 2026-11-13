#include "World/LyudPrefetch.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProperties.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"

static TAutoConsoleVariable<int32> CVarPrefetch(TEXT("lyud.Prefetch"), 1,
	TEXT("1: people, animals and birds load their bodies in the background before they show (no hitches); 0: load them on the spot."));

bool ULyudPrefetch::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULyudPrefetch::Deinitialize()
{
	Held.Reset();
	Loading.Reset();
	Missing.Reset();
	Folders.Reset();
	Super::Deinitialize();
}

bool ULyudPrefetch::Ready(const UObject* WorldContext, const FString& ObjectPath)
{
	if (CVarPrefetch.GetValueOnGameThread() == 0)
	{
		return true;
	}
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	ULyudPrefetch* Self = World ? World->GetSubsystem<ULyudPrefetch>() : nullptr;
	if (!Self)
	{
		return true;
	}
	return Self->ReadyPackage(FName(FPackageName::ObjectPathToPackageName(ObjectPath)), ObjectPath);
}

bool ULyudPrefetch::ReadyPackage(FName Package, const FString& ObjectPath)
{
	if (Missing.Contains(Package))
	{
		return true;
	}
	if (Loading.Contains(Package))
	{
		return false;
	}
	if (FindObject<UObject>(nullptr, *ObjectPath))
	{
		return true;
	}
	Loading.Add(Package);
	TWeakObjectPtr<ULyudPrefetch> Weak(this);
	LoadPackageAsync(Package.ToString(), FLoadPackageAsyncDelegate::CreateLambda(
		[Weak, ObjectPath](const FName& Name, UPackage* Loaded, EAsyncLoadingResult::Type Result)
		{
			ULyudPrefetch* Self = Weak.Get();
			if (!Self)
			{
				return;
			}
			Self->Loading.Remove(Name);
			UObject* Asset = Loaded && Result == EAsyncLoadingResult::Succeeded ? FindObject<UObject>(nullptr, *ObjectPath) : nullptr;
			if (Asset)
			{
				Self->Held.Add(Asset);   // or the garbage collector takes it back before the body is made
			}
			else
			{
				Self->Missing.Add(Name);
			}
		}));
	return false;
}

void ULyudPrefetch::Release(const UObject* WorldContext, const FString& ObjectPath)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	ULyudPrefetch* Self = World ? World->GetSubsystem<ULyudPrefetch>() : nullptr;
	if (UObject* Asset = Self ? FindObject<UObject>(nullptr, *ObjectPath) : nullptr)
	{
		Self->Held.RemoveSwap(Asset, EAllowShrinking::No);
	}
}

bool ULyudPrefetch::FolderReady(const UObject* WorldContext, const FString& Folder, const TCHAR* Skip)
{
	if (CVarPrefetch.GetValueOnGameThread() == 0)
	{
		return true;
	}
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	ULyudPrefetch* Self = World ? World->GetSubsystem<ULyudPrefetch>() : nullptr;
	if (!Self)
	{
		return true;
	}
	const FString Key = Skip ? Folder + TEXT("|") + Skip : Folder;
	FFolder* Entry = Self->Folders.Find(Key);
	if (!Entry)
	{
		Entry = &Self->Folders.Add(Key);
		TArray<FAssetData> Assets;
		if (IAssetRegistry* Registry = IAssetRegistry::Get())
		{
			Registry->GetAssetsByPath(FName(Folder), Assets, true);
			// an uncooked -game (bench, soak, offscreen tests) may ask before the registry's background scan reached the
			// folder: scan it once, so the prefetch isn't silently empty there
			if (Assets.IsEmpty() && !GIsEditor && !FPlatformProperties::RequiresCookedData())
			{
				Registry->ScanPathsSynchronous({ Folder });
				Registry->GetAssetsByPath(FName(Folder), Assets, true);
			}
			UE_LOG(LogTemp, Log, TEXT("Prefetch: %s has %d assets"), *Folder, Assets.Num());
		}
		for (const FAssetData& A : Assets)
		{
			if (!Skip || !A.AssetName.ToString().Contains(Skip))
			{
				Entry->Paths.Add(A.GetObjectPathString());
				Entry->Packages.Add(A.PackageName);
			}
		}
	}
	if (Entry->bReady)
	{
		return true;
	}
	bool bAll = true;
	for (int32 I = 0; I < Entry->Paths.Num(); ++I)
	{
		bAll &= Self->ReadyPackage(Entry->Packages[I], Entry->Paths[I]);
	}
	if (bAll)
	{
		// hold what someone else had loaded too, so the answer stays true (a GC would take it otherwise)
		for (const FString& P : Entry->Paths)
		{
			if (UObject* Asset = FindObject<UObject>(nullptr, *P))
			{
				Self->Held.AddUnique(Asset);
			}
		}
		Entry->bReady = true;
	}
	return bAll;
}
