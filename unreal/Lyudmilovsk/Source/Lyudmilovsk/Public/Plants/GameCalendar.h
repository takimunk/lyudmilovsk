#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameCalendar.generated.h"

/**
 * The in-game date: StartDate when a new game begins, then TimeScale in-game seconds per second
 * of the game clock (ULyudSaveSubsystem: pauses with the game, saved with the slot), plus whatever
 * the date was moved by (developer mode: SetNow, SetTimeScale). Plants, the sky and sun, the
 * starlings and the grass all read it through NowIn. TimeScale and the move are saved ("Calendar").
 */
UCLASS()
class LYUDMILOVSK_API UGameCalendar : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Scene 01 happens on a late October afternoon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calendar") FDateTime StartDate = FDateTime(2026, 10, 20, 16, 0, 0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Calendar") float TimeScale = 1.f;
	/** In-game seconds the date was moved by (developer mode). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Calendar") double OffsetSeconds = 0.0;

	/** Moves the date and time to When; it runs on from there. */
	UFUNCTION(BlueprintCallable, Category = "Calendar") void SetNow(const FDateTime& When);
	/** In-game seconds per second of play from now on, keeping the current date. */
	UFUNCTION(BlueprintCallable, Category = "Calendar") void SetTimeScale(float Scale);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

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
