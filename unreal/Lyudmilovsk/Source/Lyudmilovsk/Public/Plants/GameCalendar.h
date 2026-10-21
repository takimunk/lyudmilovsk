#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameCalendar.generated.h"

/**
 * The in-game date: StartDate when a new game begins, then TimeScale in-game seconds per second
 * of the game clock (ULyudSaveSubsystem: pauses with the game, saved with the slot). Plants read
 * their age and season from it.
 */
UCLASS()
class LYUDMILOVSK_API UGameCalendar : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Scene 01 happens on a late October afternoon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calendar") FDateTime StartDate = FDateTime(2026, 10, 20, 16, 0, 0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calendar") float TimeScale = 1.f;

	UFUNCTION(BlueprintCallable, Category = "Calendar") FDateTime Now() const;
	/** 1 = 1 January 00:00; fractional. */
	UFUNCTION(BlueprintCallable, Category = "Calendar") float DayOfYear() const;

	static float DayOfYear(const FDateTime& Date);
	static double YearsBetween(const FDateTime& From, const FDateTime& To) { return (To - From).GetTotalDays() / 365.2425; }
	/** The saved game clock: seconds of play since the new game; 0 outside play. */
	static double GameSeconds(const UWorld* World);
	/** The calendar of a world, or a fixed one at the default StartDate when there is none. */
	static FDateTime NowIn(const UWorld* World);
};
