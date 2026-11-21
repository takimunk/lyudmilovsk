#pragma once

#include "CoreMinimal.h"
#include "LyudSave.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include <string>
#include <vector>
#include "LyudTownRumours.generated.h"

namespace LyudRumour { class FTown; struct FTies; }

/**
 * The talk of the town about the hero (docs/town_rumours.md, backlog item 22). Every game event (ULyudQuests::Notify) that
 * the rules of Content/PeopleData/rumours.json call news (a cup won, a regular beaten, a bird tamed, a first shift at a job,
 * a fine on the train, ...) becomes a rumour: the townsfolk close by know it at once, and it goes round along the town's
 * ties (household, workmates, friends: a far terrace hears it a day later) for a few game days, then is forgotten.
 *
 * Who hears it is worked out once when it happens (the engine-free LyudRumour::FTown, Private/People/TownRumours.h, tested by
 * tools/barksim/run.sh rumours); this subsystem only keeps the store, saved as "TownRumours", and answers:
 *  - ULyudTownTalkSubsystem: what a resident would bring up now (the bark sets' heard_of =), at most one remark each a day,
 *    each rumour once per resident, and a long pause between any two remarks in the town (RemarkReady / Remarked);
 *  - ULyudTownPaper: the day before's talk for the column «Говорят в городе»;
 *  - ULyudLetters: a P.S. for a townsperson's letter back;
 *  - ULyudCityThings: the staff's topic «Что говорят в городе?» (the kinds going round, their details).
 *
 * Console: lyud.Rumours.Status, lyud.Rumours.Add <kind> [detail key] (as if it happened now, here), lyud.Rumours.Clear.
 * Developer mode: rumour.cup, rumour.beat, rumour.status, rumour.clear (group "friends").
 */
UCLASS()
class LYUDMILOVSK_API ULyudTownRumours : public UWorldSubsystem, public ILyudSaveable
{
	GENERATED_BODY()

public:
	ULyudTownRumours();
	virtual ~ULyudTownRumours() override;

	static ULyudTownRumours* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;

	/** A game event (ULyudQuests::Notify calls it for every one): news by the rules becomes a rumour now, here. */
	static void OnGameEvent(const UObject* WorldContext, FName Event, const FString& Target);
	/** As OnGameEvent, by kind (the console, developer mode). False: not added (unknown kind, the same news going round). */
	bool AddNow(const FString& Kind, const FString& Detail);

	// --- the townsfolk's remarks (ULyudTownTalkSubsystem)

	/** May somebody remark on the hero now (the town's pause since the last remark is over)? */
	bool RemarkReady() const;
	/** The kinds Person would bring up now (empty: none, or they remarked on one today). Out is reused. */
	void KindsFor(int32 Person, TArray<FString>& Out) const;
	/** Person said a line about one of Kinds (the bark set's heard_of): the rumour it was about is told by them, the town's
	 *  pause starts again. Its detail in the game's language for the line's {1} (empty: none). */
	FString Remarked(int32 Person, const TArray<FString>& Kinds);

	// --- the paper, the letters, the staff

	/** The paper's column «Говорят в городе» on the morning of Day (days since 1970, the letters' calendar): up to two lines
	 *  about the day before's talk, each a key and its arguments ("@key": a key itself). */
	void PaperColumn(int32 Day, TArray<TPair<FString, TArray<FString>>>& Out) const;
	/** A P.S. for a townsperson's letter written on Day (the last days' talk): its key and arguments; false: none. */
	bool PostScript(int32 Day, FString& Key, TArray<FString>& Args) const;
	/** Days since 1970 of a date (the calendar PaperColumn and PostScript count in). */
	static int32 DayOf(const FDateTime& When);
	/** The kinds going round today (the staff's topic: every counter has heard of them by now). Stable between calls. */
	const std::vector<std::string>& KindsGoingRound();
	/** The detail of today's rumour of Kind in the game's language (empty: none). */
	FString DetailOf(const FString& Kind) const;

	/** What is going round, who knows it, who has remarked on it (lyud.Rumours.Status). */
	FString Status();
	void ClearAll();

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	/** The store as lines (LyudRumour::FTown::Save): the rumours with their witnesses and tellers by key ("TownRumours"). */
	UPROPERTY(SaveGame) TArray<FString> Saved;

private:
	bool LoadData();
	/** The town's ties from the people's life (built when first needed, again if the town's people changed). */
	bool EnsureTies();
	int32 Today(int32* Minute = nullptr) const;
	/** A rumour of Kind now, seen by whoever is near the hero (and Witness, or the resident Detail names): its index, -1 none. */
	int32 Add(int32 Kind, const std::string& Detail, int32 Witness);
	FString DetailText(const std::string& Detail) const;

	TPimplPtr<LyudRumour::FTown> Town;
	TPimplPtr<LyudRumour::FTies> Ties;
	bool bLoaded = false;
	bool bSavedPending = false;
	int32 TiesFor = -1;             // how many people the ties were built for
	float WitnessCm = 3000.f;
	float RemarkEvery = 150.f;
	double LastRemarkAt = -1e9;     // game seconds (the world's)
	std::vector<std::string> GoingRound;
	int32 GoingRoundDay = -1;
	size_t GoingRoundN = 0;
	mutable std::vector<int> Scratch;
};
