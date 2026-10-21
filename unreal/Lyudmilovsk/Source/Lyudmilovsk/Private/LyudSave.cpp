#include "LyudSave.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "TimerManager.h"

static const TCHAR* IndexSlot = TEXT("lyud_index");

void ULyudSaveGame::Store(UObject* Object, FLyudSaveRecord& Out)
{
	Out.Data.Reset();
	FMemoryWriter Writer(Out.Data, true);
	FObjectAndNameAsStringProxyArchive Ar(Writer, true);
	Ar.ArIsSaveGame = true;
	Ar.ArNoDelta = true;
	Object->Serialize(Ar);
}

void ULyudSaveGame::Restore(UObject* Object, const FLyudSaveRecord& In)
{
	FMemoryReader Reader(In.Data, true);
	FObjectAndNameAsStringProxyArchive Ar(Reader, true);
	Ar.ArIsSaveGame = true;
	Ar.ArNoDelta = true;
	Object->Serialize(Ar);
}

void ULyudSaveGame::WriteObject(FName Key, UObject* Object)
{
	if (Object)
	{
		Store(Object, Records.FindOrAdd(Key));
	}
}

bool ULyudSaveGame::ReadObject(FName Key, UObject* Object) const
{
	const FLyudSaveRecord* Record = Records.Find(Key);
	if (!Record || !Object)
	{
		return false;
	}
	Restore(Object, *Record);
	return true;
}

ULyudSaveSubsystem* ULyudSaveSubsystem::Get(const UObject* WorldContext)
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContext);
	return GI ? GI->GetSubsystem<ULyudSaveSubsystem>() : nullptr;
}

double ULyudSaveSubsystem::GetGameTimeSeconds() const
{
	const UWorld* World = GetGameInstance()->GetWorld();
	return ClockBase + (World ? World->GetTimeSeconds() : 0.0);
}

FString ULyudSaveSubsystem::MapOf(const UWorld* World)
{
	return UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
}

ULyudSlotIndex* ULyudSaveSubsystem::LoadIndex() const
{
	if (ULyudSlotIndex* Index = Cast<ULyudSlotIndex>(UGameplayStatics::LoadGameFromSlot(IndexSlot, 0)))
	{
		return Index;
	}
	return Cast<ULyudSlotIndex>(UGameplayStatics::CreateSaveGameObject(ULyudSlotIndex::StaticClass()));
}

void ULyudSaveSubsystem::SaveIndex(ULyudSlotIndex* Index) const
{
	UGameplayStatics::SaveGameToSlot(Index, IndexSlot, 0);
}

TArray<FLyudSlotInfo> ULyudSaveSubsystem::GetSlots() const
{
	TArray<FLyudSlotInfo> Slots = LoadIndex()->Slots;
	Slots.Sort([](const FLyudSlotInfo& A, const FLyudSlotInfo& B) { return A.SavedAt > B.SavedAt; });
	return Slots;
}

FString ULyudSaveSubsystem::SaveToSlot(const FString& Title, const FString& InSlot)
{
	UWorld* World = GetGameInstance()->GetWorld();
	if (!World || bTravelling)
	{
		return FString();
	}
	ULyudSlotIndex* Index = LoadIndex();
	FString Slot = InSlot;
	if (Slot.IsEmpty())
	{
		Slot = FString::Printf(TEXT("lyud_save_%03d"), Index->NextId++);
	}

	ULyudSaveGame* Save = Cast<ULyudSaveGame>(UGameplayStatics::CreateSaveGameObject(ULyudSaveGame::StaticClass()));
	Save->Title = Title;
	Save->SavedAt = FDateTime::Now();
	Save->Map = MapOf(World);
	Save->GameTimeSeconds = GetGameTimeSeconds();
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		Save->ControlRotation = PC->GetControlRotation();
		if (const APawn* Pawn = PC->GetPawn())
		{
			Save->bHasPlayer = true;
			Save->PlayerTransform = Pawn->GetActorTransform();
		}
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->Implements<ULyudSaveable>())
		{
			ILyudSaveable::Execute_OnGameSaving(*It);
			ULyudSaveGame::Store(*It, Save->Actors.FindOrAdd(It->GetFName()));
		}
	}
	for (const TPair<FName, TWeakObjectPtr<UObject>>& Pair : SaveObjects)
	{
		if (UObject* Object = Pair.Value.Get())
		{
			if (Object->Implements<ULyudSaveable>())
			{
				ILyudSaveable::Execute_OnGameSaving(Object);
			}
			Save->WriteObject(Pair.Key, Object);
		}
	}
	OnBeforeSave.Broadcast(Save);

	if (!UGameplayStatics::SaveGameToSlot(Save, Slot, 0))
	{
		UE_LOG(LogTemp, Warning, TEXT("Save: could not write slot %s"), *Slot);
		return FString();
	}
	FLyudSlotInfo* Info = Index->Slots.FindByPredicate([&](const FLyudSlotInfo& S) { return S.Slot == Slot; });
	if (!Info)
	{
		Info = &Index->Slots.AddDefaulted_GetRef();
		Info->Slot = Slot;
	}
	Info->Title = Title;
	Info->SavedAt = Save->SavedAt;
	Info->GameTimeSeconds = Save->GameTimeSeconds;
	SaveIndex(Index);
	return Slot;
}

bool ULyudSaveSubsystem::LoadSlot(const FString& Slot)
{
	ULyudSaveGame* Save = Cast<ULyudSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!Save || Save->Map.IsEmpty() || bTravelling)
	{
		return false;
	}
	Pending = Save;
	bPendingNewGame = false;
	bTravelling = true;
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*Save->Map));
	return true;
}

void ULyudSaveSubsystem::NewGame()
{
	UWorld* World = GetGameInstance()->GetWorld();
	if (!World || bTravelling)
	{
		return;
	}
	Pending = nullptr;
	bPendingNewGame = true;
	bTravelling = true;
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*MapOf(World)));
}

void ULyudSaveSubsystem::DeleteSlot(const FString& Slot)
{
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	ULyudSlotIndex* Index = LoadIndex();
	Index->Slots.RemoveAll([&](const FLyudSlotInfo& S) { return S.Slot == Slot; });
	SaveIndex(Index);
}

void ULyudSaveSubsystem::RegisterSaveObject(FName Key, UObject* Object)
{
	SaveObjects.Add(Key, Object);
}

void ULyudSaveSubsystem::UnregisterSaveObject(FName Key)
{
	SaveObjects.Remove(Key);
}

void ULyudSaveSubsystem::NotifyWorldEnding(UWorld& World)
{
	// The next level's clock starts from zero again.
	ClockBase += World.GetTimeSeconds();
}

void ULyudSaveSubsystem::NotifyWorldBeginPlay(UWorld& World)
{
	bTravelling = false;
	if (bPendingNewGame)
	{
		bPendingNewGame = false;
		ClockBase = -World.GetTimeSeconds();
	}
	if (Pending)
	{
		// After every actor's BeginPlay, so loaded state is not overwritten by start-up defaults.
		World.GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ULyudSaveSubsystem::ApplyPending, TWeakObjectPtr<UWorld>(&World)));
	}
}

void ULyudSaveSubsystem::ApplyPending(TWeakObjectPtr<UWorld> WeakWorld)
{
	UWorld* World = WeakWorld.Get();
	ULyudSaveGame* Save = Pending;
	Pending = nullptr;
	if (!World || !Save)
	{
		return;
	}
	ClockBase = Save->GameTimeSeconds - World->GetTimeSeconds();

	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		PC->SetControlRotation(Save->ControlRotation);
		if (APawn* Pawn = PC->GetPawn(); Pawn && Save->bHasPlayer)
		{
			Pawn->SetActorTransform(Save->PlayerTransform, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	int32 Restored = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (!It->Implements<ULyudSaveable>())
		{
			continue;
		}
		if (const FLyudSaveRecord* Record = Save->Actors.Find(It->GetFName()))
		{
			ULyudSaveGame::Restore(*It, *Record);
			ILyudSaveable::Execute_OnGameLoaded(*It);
			++Restored;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("Save: loaded \"%s\", %d of %d actors restored, game time %.0f s"), *Save->Title, Restored, Save->Actors.Num(), Save->GameTimeSeconds);
	for (const TPair<FName, TWeakObjectPtr<UObject>>& Pair : SaveObjects)
	{
		UObject* Object = Pair.Value.Get();
		if (Object && Save->ReadObject(Pair.Key, Object) && Object->Implements<ULyudSaveable>())
		{
			ILyudSaveable::Execute_OnGameLoaded(Object);
		}
	}
	OnAfterLoad.Broadcast(Save);
}

// Console: lyud.Saves lists the slots, lyud.Save [title] saves to a new slot, lyud.Load [n] loads the n-th newest (0 = latest).
static FAutoConsoleCommandWithWorldAndArgs GLyudSavesCmd(TEXT("lyud.Saves"), TEXT("List save slots, newest first."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(World))
		{
			const TArray<FLyudSlotInfo> Slots = Saves->GetSlots();
			for (int32 i = 0; i < Slots.Num(); ++i)
			{
				UE_LOG(LogTemp, Display, TEXT("Save %d: %s \"%s\" %s, game time %.0f s"), i, *Slots[i].Slot, *Slots[i].Title, *Slots[i].SavedAt.ToString(), Slots[i].GameTimeSeconds);
			}
			UE_LOG(LogTemp, Display, TEXT("Save: %d slots, game time now %.1f s"), Slots.Num(), Saves->GetGameTimeSeconds());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GLyudSaveCmd(TEXT("lyud.Save"), TEXT("lyud.Save [title]: save to a new slot."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(World))
		{
			const FString Title = Args.Num() > 0 ? FString::Join(Args, TEXT(" ")) : FDateTime::Now().ToString();
			UE_LOG(LogTemp, Display, TEXT("Save: wrote \"%s\" to %s"), *Title, *Saves->SaveToSlot(Title));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GLyudLoadCmd(TEXT("lyud.Load"), TEXT("lyud.Load [n]: load the n-th newest slot (0, the latest, by default)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(World))
		{
			const TArray<FLyudSlotInfo> Slots = Saves->GetSlots();
			const int32 N = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
			if (Slots.IsValidIndex(N))
			{
				Saves->LoadSlot(Slots[N].Slot);
			}
		}
	}));
