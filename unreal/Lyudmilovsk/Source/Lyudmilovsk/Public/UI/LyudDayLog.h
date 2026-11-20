#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "LyudSave.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/PimplPtr.h"
#include "LyudDayLog.generated.h"

namespace LyudDayLog { class FModel; }

/**
 * The diary writes itself (docs/diary.md, "Days"). Every game event (ULyudQuests::Notify passes each one here) that counts for
 * a topic of the day (Content/UI/daylog.json: a fish caught, a bird met, a film, a job, a festival...) and the weather of each
 * hour are kept as facts of the day; the diary's «Дни» pages show, for each day played, the short entry the hero writes about it
 * in the evening: a line about the weather, a few about what was best that day, a closing line, in his hand, with a doodle in
 * the margin. The words come from templates with slots and variants (tools/daylog/content.py), never the same sentence two days
 * running; the same facts always make the same entry. Today's entry appears from the evening (EveningHour, 18:00).
 * The rules are plain C++ (Private/UI/DayLog.*, tests tools/daylog/run.sh). Saved as "DayLog".
 *
 * Console: lyud.DayLog.Report (the entries in the log), lyud.DayLog.Fill [days] (made-up days before today, to look at the
 * pages), lyud.DayLog.Forget. Developer mode actions days.fill, days.forget.
 */
UCLASS()
class LYUDMILOVSK_API ULyudDayLog : public UGameInstanceSubsystem, public ILyudSaveable
{
	GENERATED_BODY()

public:
	ULyudDayLog();
	virtual ~ULyudDayLog() override;

	static ULyudDayLog* Get(const UObject* WorldContext);
	/** A game event (from ULyudQuests::Notify): counted if a topic wants it. */
	static void NoteEvent(const UObject* WorldContext, FName Event, const FString& Target, int32 N);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;

	/** One day's entry in the current language. */
	struct FEntry
	{
		FDateTime Day;
		bool bToday = false;
		TArray<FText> Lines;
		/** The margin doodle: "bird:<Species>", "item:<id>", "weather:<code>". */
		FString Doodle;
	};
	/** The entries of every day played, oldest first (today's from the evening). */
	void Entries(TArray<FEntry>& Out) const;

	void Note(FName Event, const FString& Target, int32 N);
	void Forget();
	/** Made-up days before today (dev, shots of the pages): Days days of birds, fish, people, weather... */
	void Fill(int32 Days);
	void Report() const;

	UPROPERTY(SaveGame) TMap<FString, FString> Saved;

private:
	bool Tick(float DeltaTime);
	FDateTime Now() const;

	TPimplPtr<LyudDayLog::FModel> Model;
	bool bLoaded = false;
	FTSTicker::FDelegateHandle TickHandle;
};
