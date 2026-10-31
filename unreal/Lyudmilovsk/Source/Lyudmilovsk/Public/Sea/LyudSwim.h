#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LyudSwim.generated.h"

class ACharacter;
class ALyudSea;
class SLyudAirMeter;
class UAudioComponent;

/** Where the hero is in the water. */
UENUM(BlueprintType)
enum class ELyudSwimState : uint8
{
	/** On land, wading in the shallows, on the ice, seated, a bird. */
	Dry,
	/** Swimming at the surface, the head out (ALyudSea keeps the eyes over the waves). */
	Surface,
	/** Diving: the eyes under the water. */
	Under,
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FLyudSwimStateChanged, ELyudSwimState /*Old*/, ELyudSwimState /*New*/);
DECLARE_MULTICAST_DELEGATE(FLyudSwimEvent);

/**
 * The hero's breath across the whole game (saved as "Breath"): how many times he has nearly drowned, and whether he has
 * found out that he can breathe under the water here. Once he has, there is no air meter any more, ever.
 */
UCLASS()
class LYUDMILOVSK_API ULyudBreath : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static ULyudBreath* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Back to a new game's (the level began: a load restores after this). */
	void Reset();

	/** Times the air ran out under water and he came up choking. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Swim") int32 NearDrowns = 0;
	/** He coughed, found he was breathing, and the meter went for good: unlimited time under water. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Swim") bool bBreathFree = false;

	/** The world Reset ran for (each level start resets once, whoever asks first). */
	TWeakObjectPtr<UWorld> ResetFor;
};

/**
 * Swimming and diving for the hero (docs/swimming.md). Sits on AHeroCharacter; the sea (ALyudSea) does the water itself:
 * its physics volume makes the body swim, and at the surface it keeps the eyes over the waves. This adds going under.
 *
 *  - At the surface: WASD swims breaststroke along the look's heading, Space holds the head up. C or Ctrl, or swimming on
 *    while looking down steeply, dives (a breath, a splash); falling in fast goes under by itself.
 *  - Under: W swims where you look (up and down too), Space rises, C / Ctrl sinks, Shift swims slowly. The body is
 *    almost neutral and drifts up a little when still. Coming up to within a hand of the surface (rising) is surfacing.
 *    The tide moves the surface (ALyudSea::SeaLevel) and the bottom stays where it is; under the winter's sea ice
 *    there is no surface: the ice's underside is a ceiling and there is no air under it.
 *  - Air: BreathSeconds of it while the eyes are under (a little less swimming hard), back in a few breaths at the
 *    surface. The meter (bottom centre) shows while there is less than full; empty, it blinks red and the hero chokes,
 *    is pushed upward and has less control. Coming up after that is a near-drowning: he gasps and coughs.
 *  - The NearDrownsToRealise-th time the air runs out (or after RealiseAfterSeconds of choking where he can't come up,
 *    under the ice), he stops choking, coughs, notices he is breathing after all and says so (line.hero.swim_breath.*,
 *    voiced, subtitled), and the meter fades away for good (ULyudBreath). From then on the water has no time limit.
 *
 * For other systems (the underwater life, the hatch to Europa): ULyudSwimComponent::Get(World) is the player's; ask
 * GetState / IsSwimming / IsUnderwater / GetDepthCm / GetBottomDepthCm / IsUnderIce, and listen to OnStateChanged.
 *
 * Console: lyud.Swim 0 turns diving off (the old head-up-only swimming); lyud.Swim.Breath <s> the air; lyud.Swim.Debug 1
 * prints the state; lyud.Swim.Reset forgets the near-drownings (the meter comes back); lyud.Swim.Free gives the
 * realisation at once; lyud.Swim.Dive / lyud.Swim.Air <0..1> for tests.
 */
UCLASS(ClassGroup = (Lyudmilovsk), meta = (BlueprintSpawnableComponent))
class LYUDMILOVSK_API ULyudSwimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULyudSwimComponent();

	/** The player's (the hero's, if he is the pawn or hidden behind a bird), or null. */
	static ULyudSwimComponent* Get(const UObject* WorldContext);
	UFUNCTION(BlueprintPure, Category = "Swim", meta = (WorldContext = "WorldContext"))
	static ULyudSwimComponent* GetPlayerSwim(const UObject* WorldContext) { return Get(WorldContext); }

	UFUNCTION(BlueprintPure, Category = "Swim") ELyudSwimState GetState() const { return State; }
	/** In deep water, at the surface or under. */
	UFUNCTION(BlueprintPure, Category = "Swim") bool IsSwimming() const { return State != ELyudSwimState::Dry; }
	/** Diving: the eyes are under the water. */
	UFUNCTION(BlueprintPure, Category = "Swim") bool IsUnderwater() const { return State == ELyudSwimState::Under; }
	/** How far the eyes are under the water's surface (or the ice's underside), cm; 0 when not under. */
	UFUNCTION(BlueprintPure, Category = "Swim") float GetDepthCm() const { return EyeDepth; }
	/** How far it is from the eyes down to the bottom, cm (0 when dry). */
	UFUNCTION(BlueprintPure, Category = "Swim") float GetBottomDepthCm() const { return BottomBelow; }
	/** The eyes' world point (what the depth is measured at). */
	UFUNCTION(BlueprintPure, Category = "Swim") FVector GetEyeLocation() const { return Eye; }
	/** Under the winter's sea ice (no surface over the head). */
	UFUNCTION(BlueprintPure, Category = "Swim") bool IsUnderIce() const { return bUnderIce; }
	/** The air left 0..1 (1 at the surface; always 1 once the breath is free). */
	UFUNCTION(BlueprintPure, Category = "Swim") float GetAir() const { return Air; }
	/** The air has run out: choking. */
	UFUNCTION(BlueprintPure, Category = "Swim") bool IsOutOfAir() const { return bChoking; }
	/** He knows he can breathe under water: no meter, no limit. */
	UFUNCTION(BlueprintPure, Category = "Swim") bool IsBreathFree() const;
	/** The meter's opacity now 0..1 and whether it is in its red blink (the HUD). */
	float GetMeterAlpha() const { return MeterAlpha; }
	bool IsMeterRed() const { return bChoking; }
	/** The realisation line being spoken now (its key), or empty. */
	const FString& GetSubtitleKey() const { return SubtitleKey; }
	float GetSubtitleAlpha() const { return SubtitleAlpha; }
	/** Seconds the eyes have been under since the last breath. */
	float GetTimeUnder() const { return TimeUnder; }

	/** Goes under from the surface now (tests, a scripted dive); false if not at the surface. */
	UFUNCTION(BlueprintCallable, Category = "Swim") bool Dive();
	/** Sets the air left 0..1 (tests). */
	UFUNCTION(BlueprintCallable, Category = "Swim") void SetAir(float InAir);
	/** Up, down or along the look under water: the hero's move input calls this (true: used). */
	bool SwimMove(const FVector2D& Input);
	/** For the body: 0 dry .. 1 swimming, how fast (0..1 of the swim speed), the body's pitch (degrees, + head up). */
	float GetBodySwim() const { return BodySwim; }
	float GetBodyPitch() const { return BodyPitch; }

	FLyudSwimStateChanged OnStateChanged;
	/** The breath has just become free (the line starts). */
	FLyudSwimEvent OnBreathFree;

	/** Seconds of air under water. */
	UPROPERTY(EditAnywhere, Category = "Swim") float BreathSeconds = 40.f;
	/** Seconds from empty back to full at the surface. */
	UPROPERTY(EditAnywhere, Category = "Swim") float RefillSeconds = 6.f;
	/** Speeds under water, cm/s: normal, Shift. At the surface the sea's breaststroke (MaxSwimSpeed 160). */
	UPROPERTY(EditAnywhere, Category = "Swim") float DiveSpeed = 230.f;
	UPROPERTY(EditAnywhere, Category = "Swim") float DiveSlowSpeed = 110.f;
	UPROPERTY(EditAnywhere, Category = "Swim") float SurfaceSpeed = 160.f;
	/** Looking further down than this while swimming on at the surface dives, degrees. */
	UPROPERTY(EditAnywhere, Category = "Swim") float DiveLookDeg = 38.f;
	/** The eyes this close under the surface, rising, come up, cm. */
	UPROPERTY(EditAnywhere, Category = "Swim") float SurfaceWithinCm = 14.f;
	/** The near-drowning on which he realises (counting the first as 1). */
	UPROPERTY(EditAnywhere, Category = "Swim") int32 NearDrownsToRealise = 3;
	/** Choking this long where he can't come up (under the ice) realises at once, s; and choking this long on the
	 *  realising time before he notices, s. */
	UPROPERTY(EditAnywhere, Category = "Swim") float RealiseAfterSeconds = 12.f;
	UPROPERTY(EditAnywhere, Category = "Swim") float RealiseChokeSeconds = 4.f;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Swim") FString DebugReport() const;

private:
	void SetState(ELyudSwimState New);
	void UpdateWater(ACharacter& C, ALyudSea& Sea);
	void UpdateUnder(ACharacter& C, ALyudSea& Sea, float DeltaTime);
	void UpdateAir(ACharacter& C, float DeltaTime);
	void UpdateRealise(ACharacter& C, float DeltaTime);
	void UpdateLook(float DeltaTime);
	void StartRealise();
	void Surfaced(ACharacter& C);
	void Sound(FName Pool, float Volume = 1.f);
	void ShowHud(bool bShow);
	float IceUndersideZ(const FVector& At) const;

	ELyudSwimState State = ELyudSwimState::Dry;
	FVector Eye = FVector::ZeroVector;
	float EyeDepth = 0.f;
	float BottomBelow = 0.f;
	bool bUnderIce = false;
	bool bWasFalling = false;
	float FallSpeed = 0.f;

	float Air = 1.f;
	float TimeUnder = 0.f;
	bool bChoking = false;
	/** This choking is the one he realises on: no fight upward, he hangs there coughing. */
	bool bLastStraw = false;
	float ChokeTime = 0.f;
	float NextChoke = 0.f;
	float NextBubbles = 0.f;
	float SinceStroke = 0.f;
	float FullFor = 0.f;
	float MeterAlpha = 0.f;
	float StateTime = 0.f;

	/** The realisation: 0 none, then seconds since it began (coughing, the line, the meter fading). */
	bool bRealising = false;
	float RealiseTime = 0.f;
	bool bLineStarted = false;
	FString SubtitleKey;
	float SubtitleAlpha = 0.f;
	float SubtitleFor = 0.f;

	FVector2D MoveInput = FVector2D::ZeroVector;
	float MoveInputAge = 1.f;
	float BodySwim = 0.f;
	float BodyPitch = 0.f;

	UPROPERTY(Transient) TObjectPtr<UAudioComponent> UnderLoop;
	TSharedPtr<SLyudAirMeter> Hud;
};
