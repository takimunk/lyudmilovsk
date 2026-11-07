#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudFishBook.generated.h"

class SWidget;

/**
 * «Рыбы нашего берега», the angler's book (item fish_book, sold at the Gastronom with the tackle; docs/fishing.md): every fish one
 * can catch on this coast, a page each, with what it takes, its usual size and what the Gastronom pays, and in the margin the
 * hero's own catch of it from the angler's log (ULyudFishingState): how many, the biggest, when the first. N (or "Open" on the
 * diary's Things page) opens it, N or Esc shuts it; like the bird guide it does not pause the game.
 * Console: lyud.Fish.Book [close] (gives the book if missing).
 */
UCLASS()
class LYUDMILOVSK_API ULyudFishBookScreen : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudFishBookScreen* Get(const UObject* WorldContext);
	static const FName ItemId;
	static bool IsOwned(const UObject* WorldContext);

	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudFishBookScreen, STATGROUP_Tickables); }

	/** Opens it (at Species' page, or the last one read). */
	void Open(FName Species = NAME_None);
	void Close();
	bool IsOpen() const { return Page.IsValid(); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	TSharedPtr<SWidget> Page;
	uint64 OpenedFrame = 0;
	/** The page last read: -1 the journal, else a Catchable index. */
	int32 LastRead = -1;
	/** The fish pictures loaded in the background once the book is owned (checked once a second until they are). */
	bool bIconsReady = false;
	float IconsIn = 0.f;
	void Lock(bool bLock);
};
