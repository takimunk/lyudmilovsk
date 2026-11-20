#pragma once

#include "CoreMinimal.h"
#include "AtonComputer.h"
#include "LyudSave.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "AtonLab.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
namespace AtonLab { class FLabState; }

/**
 * A terminal of the lab network «ЛАБ-2» (docs/aton_lab.md): a desk with a beige tube monitor and a keyboard, built in
 * code, running ATON OS with the lab's programs (the session and its password, the old mail, «Ночная вахта», chess, the
 * weather station, the telescope's queue). No chair cutscene: E in front of it moves the view to the screen and gives
 * the OS the mouse and keyboard; «Встать» on the menu bar gives them back. Spawned by ULyudAtonLab from
 * Content/AtonOS/terminals.json; the actor's +X looks out of the screen, its origin is on the floor under the desk's
 * front edge.
 */
UCLASS()
class LYUDMILOVSK_API ALyudLabTerminal : public AAtonComputer
{
	GENERATED_BODY()

public:
	ALyudLabTerminal();

	/** The id of terminals.json. */
	UPROPERTY(VisibleAnywhere, Category = "ATON OS")
	FName TerminalId;

	/** Where the hero's eyes are taken to: in front of the screen. */
	FVector ViewPoint() const;
	/** The middle of the screen, world. */
	FVector ScreenPoint() const;

	/** The hero sits down to it (the view goes to the screen) and gets up again. */
	void Use(APlayerController* PC);
	void StopUsing();

	virtual void BeginPlay() override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "ATON OS")
	TObjectPtr<UStaticMeshComponent> Desk;

	UPROPERTY(VisibleAnywhere, Category = "ATON OS")
	TObjectPtr<UCameraComponent> Eye;

private:
	UFUNCTION()
	void OnLeft(APlayerController* Player);
};

/**
 * The lab network's state in a world: what the hero did on it (AtonLab::FLabState: logged in, letters read, games,
 * telescope slots), kept in the save as one line; the terminals of the level and their "E" prompt; the world's
 * services the programs ask for (the hour, the cloud over the telescope).
 *
 * Console: lyud.Aton.Profile club|lab (every ATON machine of the level becomes that kind: the lab's programs on the
 * club computer), lyud.Aton.Lab login|logout|reset|state, lyud.Aton.Use (sit down to the nearest terminal) / lyud.Aton.Leave.
 */
UCLASS()
class LYUDMILOVSK_API ULyudAtonLab : public UTickableWorldSubsystem, public ILyudSaveable
{
	GENERATED_BODY()

public:
	ULyudAtonLab();
	virtual ~ULyudAtonLab() override;

	static ULyudAtonLab* Get(const UObject* WorldContext);

	AtonLab::FLabState& State();
	/** Hours since the game's first midnight (the telescope queue's clock). */
	double NowHours() const;
	/** Cloud cover 0..1 at that hour (0 when the world has no weather). */
	float CloudAt(double Hours) const;
	/** The date a number of whole days after the game's first midnight. */
	FDateTime DayDate(int32 Days) const;
	/** Booked telescope slots that have passed become results. */
	void ResolveScope();
	/** Tells the quests: event "do", target "aton_<What>" (login, mail, chess_win, night_watch...). */
	void Report(const TCHAR* What);

	ALyudLabTerminal* NearestTerminal(const FVector& To) const;
	const TArray<TObjectPtr<ALyudLabTerminal>>& GetTerminals() const { return Terminals; }

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudAtonLab, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return Terminals.Num() > 0; }
	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	void SpawnTerminals(UWorld& World);
	void StartOver();
	void SetPrompt(bool bShow);

	UPROPERTY(SaveGame)
	FString Saved;

	UPROPERTY()
	TArray<TObjectPtr<ALyudLabTerminal>> Terminals;

	TPimplPtr<AtonLab::FLabState> Lab;
	TWeakObjectPtr<ALyudLabTerminal> Aimed;
	bool bPrompt = false;
	float ScopeTimer = 0.f;
};
