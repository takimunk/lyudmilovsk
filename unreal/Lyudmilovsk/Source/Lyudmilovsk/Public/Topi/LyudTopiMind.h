#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LyudTopiMind.generated.h"

class AHeroCharacter;
class ALyudSky;
class ALyudTopi;
class APlayerController;
class UAudioComponent;
class UPostProcessComponent;
class USoundBase;
class SLyudTopiSubtitle;

/**
 * The hero's side of Topi (docs/topi.md), a component of ALyudTopi ("Mind"):
 *
 *  - the pressure on his mind (LyudTopi::FModel::Pressure, smoothed): the far dunes grow restless (ALyudTopi::SetUnrest),
 *    the view bleaches, fringes and narrows (one unbound post process), he slows down (AHeroCharacter::SetSpeedScale);
 *  - when he cannot bear it he turns back by himself: input is taken, he turns toward FModel::WayOut, looks down and
 *    walks out for a few seconds, saying one of his lines;
 *  - at night, in the river Ural: the stream pushes him; in up to the chest he dives, dissolves in the light and wakes
 *    at the computer in the club the next morning (TimesDissolved is saved);
 *  - the music of the dunes and of the river, the heartbeat and the river's hum.
 *
 * Costs nothing while the hero is far from the sand. Console: lyud.Topi.TurnBack, lyud.Topi.Dissolve, lyud.Topi.Calm 1.
 */
UCLASS(ClassGroup = (Lyudmilovsk), meta = (BlueprintSpawnableComponent))
class LYUDMILOVSK_API ULyudTopiMind : public UActorComponent
{
	GENERATED_BODY()

public:
	ULyudTopiMind();

	/** How hard Topi presses on the hero now, 0..1 (smoothed). */
	UFUNCTION(BlueprintPure, Category = "Topi") float GetPressure() const { return Pressure; }
	UFUNCTION(BlueprintPure, Category = "Topi") bool IsTurningBack() const { return bTurning; }
	UFUNCTION(BlueprintPure, Category = "Topi") bool IsDissolving() const { return bDissolving; }
	/** Turns him back now (tests, lyud.Topi.TurnBack). */
	UFUNCTION(BlueprintCallable, Category = "Topi") void ForceTurnBack();
	/** Starts the river's sequence wherever he is (tests, lyud.Topi.Dissolve). */
	UFUNCTION(BlueprintCallable, Category = "Topi") void StartDissolve();

	/** How many times he has gone into the Ural and woken at the computer. */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Topi") int32 TimesDissolved = 0;

	/** The hero line on screen now (empty: none) and its opacity, for the subtitle widget. */
	const FString& GetSubtitleKey() const { return SubtitleKey; }
	float GetSubtitleAlpha() const { return SubtitleAlpha; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	ALyudTopi* Topi() const;
	AHeroCharacter* PlayerHero(APlayerController** OutPC) const;

	void UpdatePressure(float DeltaTime, AHeroCharacter& Hero, const FVector2D& Map, bool bNight);
	void UpdateTurn(float DeltaTime, AHeroCharacter& Hero, APlayerController& PC);
	void UpdateRiver(float DeltaTime, AHeroCharacter& Hero, bool bNight);
	void UpdateDissolve(float DeltaTime, AHeroCharacter& Hero, APlayerController& PC);
	void UpdateLook(float DeltaTime);
	void UpdateSound(float DeltaTime, AHeroCharacter& Hero, const FVector2D& Map, bool bNight);
	void UpdateSubtitle(float DeltaTime);

	void BeginTurn(AHeroCharacter& Hero, APlayerController& PC);
	void EndTurn();
	void BeginDissolve(AHeroCharacter& Hero, APlayerController& PC);
	void Wake(AHeroCharacter& Hero, APlayerController& PC);
	/** Puts back whatever a sequence took (input, fade, movement, collision); safe to call any time. */
	void AbortSequences();
	/** The hero has left: everything off, nothing more to do until he comes back. */
	void GoQuiet();

	void LockInput(APlayerController* PC, bool bLock);
	void Say(const FString& Key, const AHeroCharacter& Hero);
	void ShowSubtitle(bool bShow);
	void SetLevel(UAudioComponent* Music, float Level) const;
	void DropLoop(TObjectPtr<UAudioComponent>& Loop);

	UPROPERTY(Transient) TObjectPtr<UPostProcessComponent> Post;
	UPROPERTY(Transient) TObjectPtr<ALyudSky> Sky;
	UPROPERTY(Transient) TObjectPtr<USoundBase> TopiTrack;
	UPROPERTY(Transient) TObjectPtr<USoundBase> UralTrack;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> TopiMusic;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> UralMusic;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> HeartLoop;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> HumLoop;

	TWeakObjectPtr<AHeroCharacter> LastHero;
	TWeakObjectPtr<AHeroCharacter> SequenceHero;
	TWeakObjectPtr<APlayerController> LockedPC;
	TSharedPtr<SLyudTopiSubtitle> SubtitleWidget;

	bool bActive = false;
	bool bInputLocked = false;
	bool bTriedTracks = false;
	bool bMusicHeld = false;

	// the mind
	float Pressure = 0.f;
	float HighFor = 0.f;
	float Grace = 0.f;
	float Dissolve = 0.f;

	// turning back
	bool bTurning = false;
	float TurnTime = 0.f;
	float TurnStartYaw = 0.f;
	float TurnStartPitch = 0.f;
	float TurnYaw = 0.f;
	FVector TurnDir = FVector::ForwardVector;
	int32 TurnLines = 0;
	int32 LastTurnLine = 0;

	// the river
	bool bDissolving = false;
	bool bWoken = false;
	bool bWhiteStarted = false;
	bool bClearStarted = false;
	bool bWakeSaid = false;
	float DissolveTime = 0.f;
	float DiveStartPitch = 0.f;
	float SavedGravity = 1.f;

	// sound
	float TopiLevel = 0.f;
	float UralLevel = 0.f;
	float LoopRetry = 0.f;

	// the subtitle
	FString SubtitleKey;
	float SubtitleAlpha = 0.f;
	float SubtitleFor = 0.f;
};
