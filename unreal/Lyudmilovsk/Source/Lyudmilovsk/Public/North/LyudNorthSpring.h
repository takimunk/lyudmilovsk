#pragma once

#include "CoreMinimal.h"
#include "North/LyudNorth.h"
#include "LyudNorthSpring.generated.h"

class AHeroCharacter;
class ALyudFumarole;
class USceneComponent;

/** One seat of north.json "spring" "seats", in the world (Z comes from the water). */
struct FLyudNorthSpringSeatData
{
	/** World cm of the sitter's place in the pool. */
	FVector2D At = FVector2D::ZeroVector;
	/** Unreal degrees: where the sitter faces. */
	float Yaw = 0.f;
};

/** north.json "spring" as ULyudNorthSubsystem read it (world cm). */
struct FLyudNorthSpringData
{
	FVector Center = FVector::ZeroVector;
	float RadiusCm = 260.f;
	/** "surface_z" (map metres) when the data gives the water's height; else the spring fumarole's water, else a trace. */
	bool bHasSurfaceZ = false;
	double SurfaceZ = 0.0;
	TArray<FLyudNorthSpringSeatData> Seats;
};

/**
 * The hot spring one can sit in (docs/north.md "Hot spring (game side)"): spawned by ULyudNorthSubsystem from north.json
 * "spring" {x, y, r, seats [{x, y, yaw}]} (no seats in the data: four guessed round the pool, facing its middle). It draws
 * nothing itself (the pool and its steam are the Spring ALyudFumarole's, the deck is import_north.py's), sleeps with the
 * rest of the North and, while the hero is near, polls E (never consumed): "[E] Sit in the hot spring" within the pool's
 * reach takes the nearest free seat; the hero wades in (the seat's frame glides from his feet to the seat and down) and
 * sits on AHeroCharacter::SitOn's bench pose with the water about his chest (the cushion WaterOverSeat under the surface),
 * soft steam rising round his shoulders. E again, a step, or Space gets him out onto the rim.
 *
 * NPC hook: FindFreeSeat / Occupy / Release / GetSeatWorld (a seat's frame: origin on the pool's floor under the cushion,
 * X the way the sitter faces; the cushion SeatHeight over it). The hero takes his seat through the same Occupy.
 * Console: lyud.North.SitSpring 1|0 (the hero sits in the nearest free seat from anywhere / gets out; logs a NORTH spring line).
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ALyudNorthSpring : public ALyudNorthThing
{
	GENERATED_BODY()

public:
	ALyudNorthSpring();

	/** Spawns the spring's actor (tagged North) for the data (none when there is neither a seat nor a radius). */
	static ALyudNorthSpring* SpawnFor(UWorld* World, const FLyudNorthSpringData& Data);
	/** The world's spring (null when north.json has none). */
	UFUNCTION(BlueprintPure, Category = "North", meta = (WorldContext = "WorldContext"))
	static ALyudNorthSpring* FindSpring(const UObject* WorldContext);

	/** How deep the seat's cushion is under the water's surface, cm (chest-high water for a sitter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "North") float WaterOverSeat = 55.f;
	/** The cushion over the seat's floor, cm (the bench pose's seat height). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "North") float SeatHeight = 40.f;
	/** E works this far beyond the pool's rim, cm (flat). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "North") float Reach = 150.f;
	/** Seconds to wade from where E was pressed to the seat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "North") float WadeSeconds = 2.f;

	// ---- seats (the hero and, later, NPCs) ----
	UFUNCTION(BlueprintPure, Category = "North") int32 NumSeats() const { return Seats.Num(); }
	/** The free seat nearest Near (flat), -1 when all are taken. */
	UFUNCTION(BlueprintCallable, Category = "North") int32 FindFreeSeat(const FVector& Near) const;
	UFUNCTION(BlueprintPure, Category = "North") bool IsSeatFree(int32 Seat) const;
	/** Takes Seat for Who (true: it is Who's now, also when Who already had it). */
	UFUNCTION(BlueprintCallable, Category = "North") bool Occupy(int32 Seat, UObject* Who);
	/** Gives back whatever seat Who holds. */
	UFUNCTION(BlueprintCallable, Category = "North") void Release(UObject* Who);
	/** The seat's frame in the world: origin on the floor under the cushion, X the way the sitter faces. */
	UFUNCTION(BlueprintPure, Category = "North") FTransform GetSeatWorld(int32 Seat) const;
	/** The water's surface, world Z cm. */
	UFUNCTION(BlueprintPure, Category = "North") float GetWaterZ() const { return float(WaterZ); }
	UFUNCTION(BlueprintPure, Category = "North") float GetRadius() const { return RadiusCm; }

	// ---- the hero ----
	/** Sits the hero in Seat (-1: the free seat nearest him); bWade false puts him there at once. False when he cannot. */
	UFUNCTION(BlueprintCallable, Category = "North") bool SitHero(int32 Seat = -1, bool bWade = true);
	/** Gets the hero out onto the rim (if he sits here). */
	UFUNCTION(BlueprintCallable, Category = "North") void StandHero();
	UFUNCTION(BlueprintPure, Category = "North") bool IsHeroSeated() const;
	UFUNCTION(BlueprintPure, Category = "North") int32 GetHeroSeat() const { return HeroSeat; }
	/** The hero's eyes over the water's surface, cm (tests). */
	UFUNCTION(BlueprintPure, Category = "North") float HeroEyeAboveWater() const;
	/** A NORTH spring line for logs and tests. */
	UFUNCTION(BlueprintCallable, Category = "North") FString DebugReport() const;

	/** The console's lyud.North.SitSpring 1|0. */
	static void SetHeroSitting(UWorld* World, bool bSit);

	virtual void SetAwake(bool bInAwake, bool bInNear) override;
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ResolveWater();
	void UpdateHero(float DeltaTime);
	void SetPrompt(int32 Mode);
	void ReleaseHero();
	AHeroCharacter* Hero() const;
	FVector StandOffsetFor(int32 Seat) const;

	/** The seats' frames ride on it: it glides from the hero's feet to its rest while he wades in. */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Carrier;
	TWeakObjectPtr<ALyudFumarole> Pool;
	TArray<FLyudNorthSpringSeatData> Seats;
	TArray<TWeakObjectPtr<UObject>> Taken;
	FLyudNorthSpringData Data;
	TWeakObjectPtr<AHeroCharacter> SeatedHero;
	FVector WadeFrom = FVector::ZeroVector;
	double WaterZ = 0.0;
	float RadiusCm = 260.f;
	float WadeTime = -1.f;
	float SteamDebt = 0.f;
	uint32 SteamRng = 4243u;
	int32 HeroSeat = -1;
	/** 0 none, 1 sit, 2 get out. */
	int32 Prompted = 0;
	bool bWaterKnown = false;
};
