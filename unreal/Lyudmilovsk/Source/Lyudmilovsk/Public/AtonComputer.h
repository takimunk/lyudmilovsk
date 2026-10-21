#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "AtonComputer.generated.h"

class AHeroSeat;
class SAtonDesktop;
class ULevelSequencePlayer;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAtonComputerEvent, APlayerController*, Player);

/** The monitor surface: a world-space widget that takes the real mouse and keyboard. */
UCLASS(ClassGroup = AtonOS, meta = (BlueprintSpawnableComponent))
class LYUDMILOVSK_API UAtonScreenComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UAtonScreenComponent();
};

/**
 * The hero's computer running ATON OS. Place it at the centre of the hero's screen with +X pointing out
 * of the screen (toward the chair). It draws the OS on the screen all the time; when the sit-down cutscene
 * of the HeroSeat ends, the OS takes the mouse and keyboard.
 * If the level has no AtonComputer, one is spawned in front of the HeroSeat at BeginPlay (UAtonOSSubsystem).
 */
UCLASS()
class LYUDMILOVSK_API AAtonComputer : public AActor
{
	GENERATED_BODY()

public:
	AAtonComputer();

	/** Screen resolution of the OS, in widget pixels. */
	UPROPERTY(EditAnywhere, Category = "ATON OS")
	FIntPoint Resolution = FIntPoint(1024, 768);

	/** Width of the visible screen surface, cm (the hero's monitor: 42 x 31.5). */
	UPROPERTY(EditAnywhere, Category = "ATON OS")
	float ScreenWidth = 42.f;

	/** Emissive boost of the screen (the old screen image used 4). */
	UPROPERTY(EditAnywhere, Category = "ATON OS")
	float Brightness = 5.5f;

	/** The Chosen game, built and served locally by tools/chosen_local.sh; empty to skip it. */
	UPROPERTY(EditAnywhere, Category = "ATON OS")
	FString ChosenLocalURL = TEXT("http://localhost:8787/");

	/** Where Chosen is played when no local build is running. */
	UPROPERTY(EditAnywhere, Category = "ATON OS")
	FString ChosenURL = TEXT("https://chosen.stickies.fun/");

	/** The seat whose cutscene hands the player over to the OS; found in the level when empty. */
	UPROPERTY(EditAnywhere, Category = "ATON OS")
	TObjectPtr<AHeroSeat> Seat;

	/** The player is at the computer and the OS has the input. */
	UPROPERTY(BlueprintAssignable, Category = "ATON OS")
	FAtonComputerEvent OnActivated;

	/**
	 * The player chose "Встать из-за компьютера". Bind it to play the stand-up; the menu item only appears
	 * while something is bound. The OS has already given the input back to the game when this fires.
	 */
	UPROPERTY(BlueprintAssignable, Category = "ATON OS")
	FAtonComputerEvent OnLeave;

	/** Give the mouse and keyboard to the OS. */
	UFUNCTION(BlueprintCallable, Category = "ATON OS")
	void Activate(APlayerController* PC);

	/** Give the input back to the game. */
	UFUNCTION(BlueprintCallable, Category = "ATON OS")
	void Deactivate();

	UFUNCTION(BlueprintPure, Category = "ATON OS")
	bool IsActive() const { return bActive; }

	/** The OS start menu is open, so Esc belongs to the OS. */
	bool IsOSMenuOpen() const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "ATON OS")
	TObjectPtr<UAtonScreenComponent> Screen;

private:
	UFUNCTION()
	void OnSitFinished();

	void ApplyScreenSize();

	TSharedPtr<SAtonDesktop> Desktop;
	TWeakObjectPtr<ULevelSequencePlayer> WatchedPlayer;
	TWeakObjectPtr<APlayerController> ActivePC;
	bool bActive = false;
};

/** Spawns the AtonComputer in front of the HeroSeat when a level has a seat but no computer. */
UCLASS()
class LYUDMILOVSK_API UAtonOSSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& World) override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}
};
