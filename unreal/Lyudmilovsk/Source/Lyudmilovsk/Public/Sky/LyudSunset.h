#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudSunset.generated.h"

class ALyudSky;
class USkyAtmosphereComponent;
namespace LyudSunset { struct FDayProfile; }

/** What the day's sunset does to ALyudSky's own settings (scales and tints; neutral by default). */
struct FLyudSunsetLook
{
	float Weight = 0.f;
	float MieScale = 1.f;
	float MieGAdd = 0.f;
	FLinearColor LightTint = FLinearColor::White;
	FLinearColor CloudTint = FLinearColor::White;
	/** The clouds' colour in the sky's own light (ULyudCloudSubsystem tints M_LyudClouds' albedo with it): the
	 * afterglow's hue once the sun is down, so backlit edges after sunset glow pink or orange instead of cream. */
	FLinearColor CloudAmbientTint = FLinearColor::White;
	float AureoleScale = 1.f;
	float DiskScale = 1.f;
	/** The photosphere's colour on top of the air's (the sky material's SunDiskTint): deep red in haze, golden when clean. */
	FLinearColor DiskTint = FLinearColor::White;
	float ExposureEV = 0.f;
};

/**
 * Varied sunsets and sunrises (Private/Sky/SunsetModel, docs/sun.md "Sunsets"): every evening and every morning its own
 * character (pale golden, orange, pink, red, deep crimson or a dull grey strip; ~5% spectacular ones), a pure function
 * of the shown date (so saves and clock jumps agree), made the most of or spoiled by the weather (clear, broken cloud,
 * overcast, washed air after rain, haze). It blends in only while the sun is within ~-9..11 degrees and is exactly
 * neutral outside that band.
 *
 * ALyudSky calls Update() from its UpdateSky() and folds the returned look into the settings it owns (Mie, the
 * sunlight's colour, the clouds' light, the aureole, the disk); this subsystem owns the atmosphere's Rayleigh scale,
 * Mie absorption, the haze's scale height, the ozone and the sky's luminance tint, and puts them back to their own
 * values once the sun leaves the band. Only changed values are set.
 *
 * Console: lyud.Sunset.Profile (-1 auto; 0 golden, 1 orange, 2 pink, 3 red, 4 crimson, 5 grey, 6 spectacular),
 * lyud.Sunset.Intensity (1; 0 = off), lyud.Sunset.Seed, lyud.Sunset.Clean / lyud.Sunset.Haze (-1 = from the weather),
 * lyud.Sunset.Debug 1 (the profile on screen).
 */
UCLASS()
class LYUDMILOVSK_API ULyudSunset : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudSunset* Get(const UObject* WorldContext);

	/** Work out the look for the local time Local and the sun at SunAltDeg, set the atmosphere settings this owns and
	 * return the rest for the sky to fold in. */
	const FLyudSunsetLook& Update(ALyudSky* Sky, const FDateTime& Local, float SunAltDeg);
	const FLyudSunsetLook& GetLook() const { return Look; }

	/** The profile in words (kind, hue, intensity, weather shares), for logs and tests. */
	UFUNCTION(BlueprintPure, Category = "Sky|Sunset") FString GetDebugLine() const { return DebugLine; }

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

private:
	void ApplyAtmosphere(USkyAtmosphereComponent* Atmosphere, float RayleighK, float MieAbsK, float MieHeightK, float OzoneK, const FLinearColor& SkyTint, bool bExact);

	FLyudSunsetLook Look;
	FString DebugLine;
	/** The atmosphere's own values (ALyudSky's class defaults), read the first time it is touched. */
	TWeakObjectPtr<USkyAtmosphereComponent> BaseOf;
	float BaseRayleigh = 0.f, BaseMieAbs = 0.f, BaseMieHeight = 0.f, BaseOzone = 0.f;
	FLinearColor BaseSkyLum = FLinearColor::White;
	bool bApplied = false;
	/** The profile's key (day, morning/evening, seed, forced kind) and its numbers, kept until it changes. */
	int64 ProfileKey = MIN_int64;
	TSharedPtr<LyudSunset::FDayProfile> Profile;
	double NextDebug = 0.0;
};
