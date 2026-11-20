#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudSave.h"
#include "Templates/PimplPtr.h"
#include "LyudTownPaper.generated.h"

namespace LyudPaper { class FPaper; struct FIssue; struct FFacts; }

/**
 * The town paper «Людмиловская правда» (docs/town_paper.md). Every game day has an issue, set from the game's own news the
 * first time anyone asks for it (a newspaper is read, the archive is leafed through): the next day's weather and tides, the
 * festivals today and in the next two weeks, the fair's prices, the fish biting best, the residents' birthdays and
 * anniversaries, the cinema's programme, and what the hero did the day before (birds tamed or first seen, the biggest fish
 * and records, quests finished: read from the bird book, the angler's journal and the quest journal, all dated).
 * The rules live in plain C++ (Private/City/TownPaperModel.h), the data in Content/City/town_paper.json and the words in
 * Content/Localization/paper (tools/paper/paper.py writes both), so tools/papersim sets a year of issues offline.
 *
 * Issues are kept as keys and arguments (shown in the language of the moment) in the save, the last MaxKept of them: the
 * library's periodicals rack shows them as the archive, each with its read mark. Days nobody asked about are set when a later
 * day is asked (up to MaxBackfill back), so the archive has no holes.
 *
 * The reading view asks through ULyudCityThings: a text with "gen": "town_paper" (the daily paper: kiosks, café tables) or
 * "paper_archive" (the rack) shows these pages instead of its own.
 * Console: lyud.Paper.Print [days] (today's issue, or the next N days set ahead, to the log), lyud.Paper.Reset.
 */
UCLASS()
class LYUDMILOVSK_API ULyudTownPaper : public UWorldSubsystem, public ILyudSaveable
{
	GENERATED_BODY()

public:
	ULyudTownPaper();
	virtual ~ULyudTownPaper() override;

	static ULyudTownPaper* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;

	/** The pages a generated text shows now ("town_paper": today's issue; "paper_archive": the file of past issues). False
	 *  when it cannot be set (the data missing): the text's own pages are shown then. */
	bool Pages(const FString& Gen, TArray<FText>& Out);
	/** The reading view reached the last page of a generated text: today's issue is read (its mark in the archive). */
	void FinishedReading(const FString& Gen);

	/** Today's issue as plain text in the current language (the console, tests). */
	FString IssueText(int32 DaysAhead = 0);
	/** Forgets every kept issue and read mark (a new game, the console). */
	void Forget();

	/** How many issues the save keeps, and how far back a missing day is set when a later one is asked. */
	static constexpr int32 MaxKept = 40;
	static constexpr int32 MaxBackfill = 11;

	/** Kept issues, each packed by LyudPaper::FPaper::Pack (keys and arguments, oldest first), and the days read. */
	UPROPERTY(SaveGame) TArray<FString> SavedIssues;
	UPROPERTY(SaveGame) TArray<int32> ReadDays;

private:
	bool EnsureData();
	/** Days since the game began today. */
	int32 Today() const;
	/** The issue of a day (since the game began): kept, or set now (with the days before it that are missing). */
	const LyudPaper::FIssue* IssueOf(int32 Since);
	void BuildFacts(int32 Since, LyudPaper::FFacts& Out) const;

	TPimplPtr<LyudPaper::FPaper> Paper;
	bool bTriedLoad = false;
	/** The kept issues by day, unpacked. */
	TMap<int32, TSharedPtr<LyudPaper::FIssue>> Issues;
};
