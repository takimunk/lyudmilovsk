#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudSunActivity.generated.h"

class UMaterialInstanceDynamic;
namespace LyudSunActivity { struct FState; }

/**
 * The red dwarf's activity (Private/Sky/SunActivityModel, docs/sun.md): active regions born and decaying over the
 * in-game days, turning with the star, each with its spot group and faculae; prominences standing on them and long
 * quiescent ones at high latitudes that grow, sway, sometimes erupt and lift off, and fade; flares now and then.
 * A pure function of the shown time, so saves and clock jumps agree. ALyudSky asks it every tick and it puts the star
 * on the sky material (regions, up to 16 prominences, the flare); the flare's share of the daylight comes back.
 *
 * Console: lyud.Sun.Activity (x births, 0 = quiet), lyud.Sun.TimeScale (the activity clock over the game's),
 * lyud.Sun.ForceFlare <peak> (a flare now), lyud.Sun.Debug 1 (the state on screen).
 */
UCLASS()
class LYUDMILOVSK_API ULyudSunActivity : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudSunActivity* Get(const UObject* WorldContext);

	/** Work out the star at the local time Local and put it on the sky material (nullptr: only work it out).
	 * NoonSpin = the spin the flare's longitude is drawn from (the sky material's SunSpots.x). Returns the flare's
	 * share of the daylight now, 0..1. */
	float Update(UMaterialInstanceDynamic* SkyMID, const FDateTime& Local, double RotationDays, float NoonSpin);

	/** A flare now at this peak 0..1, on the most visible strong region (lyud.Sun.ForceFlare does the same). */
	UFUNCTION(BlueprintCallable, Category = "Sky|Sun") void ForceFlare(float Peak);

	UFUNCTION(BlueprintPure, Category = "Sky|Sun") float GetFlareLight() const { return FlareLight; }
	UFUNCTION(BlueprintPure, Category = "Sky|Sun") int32 GetLimbProminences() const { return LimbProms; }
	UFUNCTION(BlueprintPure, Category = "Sky|Sun") FString GetDebugLine() const { return DebugLine; }

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
	TSharedPtr<LyudSunActivity::FState> State;
	double ForcedStartActivityHours = -1e9;
	float ForcedPeak = 0.f;
	float LastForceCVar = 0.f;
	bool bRequestForce = false;
	float RequestedPeak = 0.f;
	float FlareLight = 0.f;
	int32 LimbProms = 0;
	FString DebugLine;
};
