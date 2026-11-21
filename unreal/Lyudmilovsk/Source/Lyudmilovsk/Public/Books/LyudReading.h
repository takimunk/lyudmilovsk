#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "Templates/PimplPtr.h"
#include "LyudReading.generated.h"

class AHeroCharacter;
class APlayerController;
namespace LyudReading { struct FClub; }

/**
 * Reading anywhere and the library's book club (docs/books.md "Reading anywhere", "The book club"; interactives backlog 47, 48).
 *
 * Reading: seated anywhere (a bench has its own prompt, ULyudBenchSit; any other seat: a café's chair, the train's window
 * seat, a cable car's bench, the rowing boat, the restaurant) with a library book on loan, "E — read" while no other prompt
 * holds the line. Every book opened is asked of the light first (LyudReading::Judge: under a roof always, outdoors by day,
 * dim in the dusk, at night only with the headlamp or a lit street lamp near, never in the rain) and again every two seconds
 * while it is open: dusk falling or rain starting shuts it with a line. Loans a week old bring the librarian's postcard
 * (ULyudLetters::PostTownLetter), once a loan.
 *
 * The club: Thursdays 18:30-20:00 in the library's lounge (Content/Reading/reading.json, tools/reading/build_reading.py),
 * never on a festival day or a public holiday. The people's life has its five members there by their duty while the
 * condition book_club is on (switched here, from 18:10). With the hero near (12 m) the members talk in turns about the
 * month's book (bubbles, line.town.bookclub.<work>.<n>); if he has read it to its end, "E — join the talk": his two lines,
 * the stamp book_club/<work>, a diary line. The librarian's advice (Advice) shows at the lending desk; the paper prints the
 * month's book (PaperLines); books recommended to friends (Recommend) are answered by letter a few days later.
 *
 * Console: lyud.Reading.Report, lyud.Reading.Light, lyud.Reading.Club [on|off|auto], lyud.Reading.Quote,
 * lyud.Reading.Recommend <work>. Developer mode: group "books".
 */
UCLASS()
class LYUDMILOVSK_API ULyudReading : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudReading();
	virtual ~ULyudReading() override;

	static ULyudReading* Get(const UObject* WorldContext);

	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudReading, STATGROUP_Tickables); }

	/** May he read here and now (Why: the line saying why not)? Bright: how bright the page is drawn (0.35..1). */
	bool CanReadHere(FText* Why = nullptr, float* Bright = nullptr, bool bWhileOpen = false) const;

	/** The club's month book (a work id) for a date, and whether the circle sits now. */
	FString ClubBook(const FDateTime& When) const;
	bool ClubSitsNow() const;
	/** The lending desk's line: the librarian's advice from what he has read. Empty when there is nothing to say. */
	FText Advice() const;
	/** Recommends a work he finished to his closest friend who has none from him yet; the line to show. */
	FText Recommend(const FString& Work);
	/** The paper's column for an issue of that date: (key, args) lines, "@key" args localised. */
	void PaperLines(const FDateTime& Date, TArray<TPair<FString, TArray<FString>>>& Out) const;

	// --- console and developer mode
	FString Report() const;
	/** -1 by the calendar, 0 off, 1 on (the club's condition and talk). */
	int32 ForceClub = -1;
	void DevFinishMonthBook();
	void DevOverdue();

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	void LoadData();
	void TickSeated(AHeroCharacter* Hero, APlayerController* PC);
	void TickClub(AHeroCharacter* Hero, APlayerController* PC, float DeltaTime);
	void StepSlow();
	void SetPrompt(int32 What);
	bool HeroReadThisMonth(const FString& Work) const;

	TPimplPtr<LyudReading::FClub> Club;
	bool bLoaded = false;
	float SlowClock = 0.f;
	float TalkClock = 0.f;
	float OpenCheckClock = 0.f;
	int32 Turn = 0;
	FString TalkBook;
	int32 PromptShown = 0;
	bool bConditionSent = false;
	bool bConditionOn = false;
	/** He was near the circle at the last look; while he is not, the circle is looked for only twice a second. */
	bool bClubNear = false;
	float ClubScanClock = 0.f;
	/** The hero's lines being said after he joined (index of the next; -1 none). */
	int32 HeroLine = -1;
	float HeroClock = 0.f;
	FString JoinedBook;
};
