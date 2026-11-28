// Wind tell-tales: things that show the local wind to the eye before a kite is launched or a dinghy pushed off
// (docs/wind.md, "In the engine"). A striped windsock on a pole and a pennant streamer, built in code, swung by
// ULyudWindSubsystem::GetLocalWind at their own top: they point downwind, lift band by band as it freshens, sag in
// a lull, flutter in rough air, hang limp in a lee.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudWindsock.generated.h"

class UStaticMeshComponent;
class ULyudWindSubsystem;

// ScriptName: Python strips the E and the A, so the enum and ALyudWindTell would both be unreal.LyudWindTell
UENUM(BlueprintType, meta = (ScriptName = "LyudWindTellKind"))
enum class ELyudWindTell : uint8
{
	/** A sock of five red and white bands, 2.4 m long: each band that stands out is about 1.5 m/s (3 knots) of wind;
	 *  limp under 2 m/s, straight out by 8. */
	Windsock,
	/** A light ribbon pennant, 1.8 m: flies from a breath of air, straight by 5 m/s, whips in gusts. */
	Pennant,
};

/**
 * One tell-tale. Never ticks on its own: ULyudWindTells moves the ones near the camera. Its meshes and materials are
 * shared by every tell-tale (made once), only the transforms of its pieces change; nothing of it is in Lumen or in
 * the distance fields, nothing collides, nothing is saved with the level.
 *
 * It reads the wind at its own top, and takes its own motion off it: attached to something that moves (a mast head,
 * a boat's stern) it shows the apparent wind, as a real burgee does.
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ALyudWindTell : public AActor
{
	GENERATED_BODY()

public:
	ALyudWindTell();

	/** Puts it together (once). PoleHeightCm 0 = no pole: the cloth flies from the actor's own place (a mast head). */
	void Build(ELyudWindTell InKind, float PoleHeightCm, int32 Colours);
	/** One step: Wind may be null (then it hangs). bSample: ask the wind now (else it eases on toward the last answer). */
	void Step(const ULyudWindSubsystem* Wind, float Dt, bool bSample);

	UFUNCTION(BlueprintPure, Category = "Wind") ELyudWindTell GetKind() const { return Kind; }
	/** Where the cloth is tied on, world cm (the wind is asked here). */
	UFUNCTION(BlueprintPure, Category = "Wind") FVector GetTop() const;
	/** What it shows now: the speed it has filled to, m/s, and the compass bearing it points away from, degrees. */
	UFUNCTION(BlueprintPure, Category = "Wind") float GetShownSpeed() const { return ShownSpeed; }
	UFUNCTION(BlueprintPure, Category = "Wind") float GetShownFromDegrees() const;

private:
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Pole;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Pieces;

	ELyudWindTell Kind = ELyudWindTell::Windsock;
	float PoleHeight = 600.f;
	/** The way it points (downwind), radians in the world's XY, and how fast that swings. */
	float Yaw = 0.f, YawRate = 0.f;
	/** The last answer of the wind: its speed felt here (m/s), where it blows to, how rough it is. */
	float WindSpeed = 0.f, WindYaw = 0.f, WindRough = 0.f, WindRise = 0.f;
	/** The speed the cloth has filled to (it lags the wind a little). */
	float ShownSpeed = 0.f;
	float Flutter = 0.f;
	/** Where its top was when the wind was last asked, and how long ago: its own motion comes off the wind. */
	FVector LastTop = FVector::ZeroVector;
	float SinceSample = 0.f;
	bool bHasLastTop = false;
	bool bBuilt = false;
};

/**
 * The world's tell-tales. At the start of play it stands a windsock at each lifeguard tower's flag mast on the beach
 * (the town's things with the act "lg_flagstate") and one at the pier's head (the city plan's spot S-PIER-HEAD);
 * anything else puts up its own with Spawn. It moves the ones within 250 m of the camera every frame, the ones out
 * to 600 m four times a second, and hides the rest. lyud.Wind.Tells 0 hides them all.
 *
 *     // a pennant on a 4 m staff by the slipway, the third colourway:
 *     ALyudWindTell* T = ULyudWindTells::Spawn(this, FootOnTheGround, ELyudWindTell::Pennant, 400.f, 2);
 *     // a burgee at a dinghy's mast head: no pole, attach it and it shows the apparent wind
 *     ALyudWindTell* B = ULyudWindTells::Spawn(this, MastHead, ELyudWindTell::Pennant, 0.f, 1);
 *     B->AttachToComponent(Mast, FAttachmentTransformRules::KeepWorldTransform);
 *     ...
 *     ULyudWindTells::Remove(T);
 */
UCLASS()
class LYUDMILOVSK_API ULyudWindTells : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudWindTells* Get(const UObject* WorldContext);

	/**
	 * Stands a tell-tale with its foot at FootLocation (world cm). PoleHeightCm: the staff (0 = none, the cloth flies
	 * from FootLocation itself). Colours picks a colourway (windsock: 0 red and white, 1 orange and white; pennant: 0 red,
	 * 1 blue, 2 yellow, 3 white, 4 green). Transient: never saved. Null outside a game world.
	 */
	UFUNCTION(BlueprintCallable, Category = "Wind", meta = (WorldContext = "WorldContext"))
	static ALyudWindTell* Spawn(const UObject* WorldContext, const FVector& FootLocation, ELyudWindTell Kind = ELyudWindTell::Windsock,
		float PoleHeightCm = 600.f, int32 Colours = 0);
	/** Takes one down (also fine: just Destroy it). */
	UFUNCTION(BlueprintCallable, Category = "Wind")
	static void Remove(ALyudWindTell* Tell);

	/** How many stand now (for tests and the log). */
	UFUNCTION(BlueprintPure, Category = "Wind") int32 NumTells() const { return Tells.Num(); }
	/** Text for logs and tests: each tell-tale's place and what it shows. */
	UFUNCTION(BlueprintCallable, Category = "Wind") FString DebugReport() const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	/** The beach's and the pier's own, once the town's things are read. True when done (or given up). */
	bool PlaceWorldTells();
	/** The static world's top under a point (the sea is not ground); false if there is none within reach. */
	bool GroundUnder(const FVector& At, float UpCm, float DownCm, FVector& Out) const;

	UPROPERTY(Transient) TArray<TObjectPtr<ALyudWindTell>> Tells;
	float PlaceClock = 0.f, PlaceNext = 3.f;
	float FarClock = 0.f, SampleClock = 0.f;
	bool bPlaced = false;
	int32 ShownState = -1;
};
