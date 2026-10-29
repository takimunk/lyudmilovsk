#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sky/Ephemeris.h"
#include "LyudSky.generated.h"

class UDirectionalLightComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPostProcessComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UStaticMeshComponent;
class UVolumetricCloudComponent;

/**
 * The sky over the outdoor world: the sun and the moon where they really stand over the Baltic coast at
 * the game's date and hour (UGameCalendar, ephemeris in LyudEphemeris), a physical atmosphere (blue day,
 * red sunset, dark night, haze over the distance), volumetric clouds, a sky light captured from all of it
 * in real time, and a star dome.
 *
 * Indoors stays separate: the sun and the moon light only lighting channel 1, which outdoor meshes add to
 * the default channel 0, so the club's own lamps keep it lit as before. The player's pawn gets channel 1
 * only while it stands under the open sky (nothing but outdoor meshes above its head), so the hero is
 * sunlit outside and not inside.
 *
 * Outside, the eye adapts: as the light fails after sunset the exposure opens up by up to NightExposureEV
 * stops over the club's fixed one, eased in over EyeAdaptSeconds as the player walks out (and back as
 * they walk in), so dusk and moonlit nights read instead of going black.
 *
 * The sun is a huge red dwarf (SunAngularDiameterDeg across, a ~3200 K star: limb darkening, boiling
 * granulation, drifting sunspots, faculae, a corona and now and then a prominence) and the moon is Europa
 * (MoonAngularDiameterDeg, lit per pixel from the true sun direction, so its phases are right, with a faint
 * ashen light on its dark side). Both are drawn by the sky material (import_sky.py), not the atmosphere's
 * light disks, with brightness capped so they read rather than blind. Nights are lit by Europa as its phase says (a bright ice moon,
 * aquamarine soft-shadowed light, the same phase law as its disk) and never quite black (a faint airglow, NightSkyGlow).
 *
 * In the editor the sky shows EditorPreviewTime; in play it follows the calendar. The console variable
 * lyud.SkyHourOffset shifts either by some hours (look at a sunset or a night without waiting for it).
 */
UCLASS()
class LYUDMILOVSK_API ALyudSky : public AActor
{
	GENERATED_BODY()

public:
	ALyudSky();

	/** Where the world is on Earth (default: the Sambian coast west of Zelenogradsk). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Place") double LatitudeDeg = 54.96;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Place") double LongitudeDeg = 20.40;
	/** The calendar's clock is local time; Kaliningrad is UTC+2 all year. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Place") double UtcOffsetHours = 2.0;
	/** Compass yaw of the world: 0 = north is world +Y (out of the club's door), east is world -X. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Place") double NorthYawDeg = 0.0;

	/** The local date and hour the sky shows outside play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Time") FDateTime EditorPreviewTime = FDateTime(2026, 10, 20, 16, 0, 0);
	/** Shift the shown hour, for looking at the sky at another time without touching the calendar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Time") float HourOffset = 0.f;

	/** Sunlight above the atmosphere, in the scene's lux (the club runs a fixed dark-room exposure); the atmosphere dims and reddens it near the horizon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Light") float SunIlluminance = 40.f;
	/** The daylight's colour: the red dwarf's ~3200 K, paled toward white so the day stays pleasant (the disk itself is drawn deeper orange). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Light") FLinearColor SunColor = FLinearColor(1.f, 0.83f, 0.68f);
	/** Full moon light, scene lux: a huge bright ice moon, far brighter than real moonlight, so nights read as moonlit.
	 * Scaled by Europa's phase law (the disk's own brightness integrated, see MoonOppositionSurge) and by the air's
	 * transmittance toward it, the same as the sky material dims the disk with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Light") float FullMoonIlluminance = 1.1f;
	/** Europa's light: sea-wave aquamarine (teal-green), the night's own colour here, neither a cold blue filter nor
	 * the ice's cream. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Light") FLinearColor MoonColor = FLinearColor(0.55f, 0.85f, 0.8f);
	/** The sky light's tint at full night (the moonlit and airglow ambient in the same aquamarine; white by day). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Light") FLinearColor NightAmbientTint = FLinearColor(0.72f, 0.95f, 0.9f);
	/** Europa's phase law on top of Lommel-Seeliger (which the disk is shaded with per pixel): an opposition surge
	 * (1 + B0 / (1 + tan(a/2) / h), the shadow hiding of a porous ice regolith: the full moon flares up) and the
	 * shadows of a rough surface (exp(-k a)). a = the phase angle (sun-Europa-viewer). With these a quarter moon is
	 * ~0.12 of the full one, a crescent ~0.02. The disk and the light take the same number. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Light") float MoonOppositionSurge = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Light") float MoonOppositionWidth = 0.06f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Light") float MoonRoughness = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Light") float SkyLightIntensity = 1.f;
	/** The faint glow of the night sky itself (airglow), sky luminance at full dark: keeps a moonless night dark blue, not black. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Light") FLinearColor NightSkyGlow = FLinearColor(0.012f, 0.03f, 0.056f);
	/** Star dome brightness at full dark (the day sky outshines it on its own). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Light") float StarBrightness = 1.f;

	/** The red dwarf's apparent size, degrees across (the Sun from Earth: 0.53). Also the sunlight's source angle: soft shadows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look", meta = (ClampMin = "0.1", ClampMax = "40")) float SunAngularDiameterDeg = 10.f;
	/** Europa's apparent size, degrees across. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look", meta = (ClampMin = "0.1", ClampMax = "40")) float MoonAngularDiameterDeg = 7.f;
	/** The sun disk's luminance at the disk centre per lux of SunIlluminance, before the atmosphere dims it: a bright disk whose spots read at the outdoor exposure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") float SunDiskGain = 0.14f;
	/** The sunspots' umbra brightness (of the disk's: small spots and pores only a little darker), the granulation's contrast (1 = as drawn) and the faculae's strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") float SunSpotUmbra = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") float SunGranulation = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") float SunFaculae = 0.35f;
	/** Europa's luminance (full sunlit ice) per lux of SunIlluminance, and how much brighter it is drawn by night (a bright
	 * icy disk at the night exposure) and by day (so it shows against the blue). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") float MoonDiskGain = 0.012f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") float MoonNightBoost = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") float MoonDayBoost = 12.f;
	/** Europa's own faint halo by night. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") float MoonNightHalo = 0.003f;
	/** Days the star takes to turn once (its spots drift across the disk with it); new spots each in-game day. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") float SunRotationDays = 6.f;
	/** The sunlight's specular strength: the drawn disk is far dimmer than a physical sun of SunIlluminance, so its reflections (the sea's glitter path) are toned down to match. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") float SunSpecularScale = 0.012f;
	/** Bloom outdoors (the club's level sets its own): low, so the big bright disk is not smeared into a blob. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") float OutdoorBloom = 0.2f;
	/** Bloom outdoors at full night, and the atmosphere's haze (Mie) then as a share of the day's: the haze lit by a 7-degree
	 * moon at a night exposure would wrap Europa in a big glow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") float NightBloom = 0.05f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") float NightMieShare = 0.15f;
	/** How much of the moonlight the clouds scatter (of the engine's): moonlit clouds round a 7-degree moon at the night exposure would glow into a halo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") float MoonCloudScatter = 0.45f;
	/** The clouds' density over the cloud material's own (thicker clouds hide the low red sun instead of veiling it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") float CloudDensityScale = 8.f;
	/** Clouds letting through less than this of the light behind them are drawn opaque (the cloud component's
	 * StopTracingTransmittanceThreshold, 0.005 by default): a thick cloud hides the sun's disk, its
	 * limb and corona entirely instead of showing their outline through it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") float CloudOpaqueBelow = 0.25f;
	/** The clouds' ray-march samples over the engine's (view and shadow): the thick clouds go grainy with too few.
	 * The view needs 2 with CloudDensityScale 8: at 1 the dense clouds' fringes speckle (the noise in their
	 * transmittance, with or without the opaque cut-off); the shadow samples don't show it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") float CloudViewSamples = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") float CloudShadowSamples = 1.f;

	/** Unlit is-sky material on the dome: atmosphere luminance, sun and moon disks, stars (unreal/scripts/import_sky.py). Without one the atmosphere draws the sky itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") TObjectPtr<UMaterialInterface> SkyMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Look") TObjectPtr<UMaterialInterface> CloudMaterial;

	/** How many stops the exposure opens outdoors at full night over the club's own (a day sky adds none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Exposure") float NightExposureEV = 3.5f;
	/** The eye opens further on a darker night: up to this many stops more as the moonlight fails (a quarter of a
	 * stop per stop of moonlight lost), so a quarter-moon night is dimmer than a full one but readable, and a moonless
	 * one dark blue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Exposure") float MoonlessExposureEV = 1.f;
	/** Seconds the eye takes to adapt walking out of or into the club. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Exposure") float EyeAdaptSeconds = 1.5f;

	/** Switch the player pawn's lighting channel 1 by whether it stands under the open sky. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Player") bool bLightPlayerOutdoors = true;
	/** How far above the pawn a roof is looked for, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Player") float RoofSearchHeight = 5000.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky") TObjectPtr<UDirectionalLightComponent> SunLight;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky") TObjectPtr<UDirectionalLightComponent> MoonLight;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky") TObjectPtr<USkyAtmosphereComponent> Atmosphere;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky") TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky") TObjectPtr<UVolumetricCloudComponent> Clouds;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky") TObjectPtr<UStaticMeshComponent> Dome;
	/** The outdoor exposure, blended in by how far the player is outside. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky") TObjectPtr<UPostProcessComponent> OutdoorExposure;

	/** The local date and hour the sky shows now. */
	UFUNCTION(BlueprintCallable, Category = "Sky") FDateTime ShownTime() const;
	UFUNCTION(BlueprintCallable, Category = "Sky") float SunAltitude() const { return SunPos.AltitudeDeg; }
	UFUNCTION(BlueprintCallable, Category = "Sky") float SunAzimuth() const { return SunPos.AzimuthDeg; }
	UFUNCTION(BlueprintCallable, Category = "Sky") bool IsPlayerOutdoors() const { return bPlayerOutdoors; }
	UFUNCTION(BlueprintCallable, Category = "Sky") FString GetDebugReport() const { return DebugReport(); }

	/** Move the sun and moon to the shown time. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Sky") void UpdateSky();
	/** Text for logs and tests: time, sun and moon altitude/azimuth, phase. */
	FString DebugReport() const;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

private:
	/** Local east, north, up to world. */
	FVector WorldDirection(const FVector& Enu) const;
	void UpdatePlayerChannels(float DeltaSeconds);
	void UpdateExposure(float DeltaSeconds);

	/** The dome's material instance: sun, moon, pole directions, disk sizes and brightness each update. */
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SkyMID;
	void UpdateSkyMaterial();
	/** The clouds' instance (CloudDensityScale on the material's "Cloud_GlobalDensity"). */
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CloudMID;
	float CloudBaseDensity = -1.f;
	void MakeInstances();
	void UpdateClouds();

	FSkyPosition SunPos;
	FSkyPosition MoonPos;
	/** Europa's lit fraction and phase angle (rad) from the sun and moon directions the sky material is given. */
	double MoonPhase = 0.0;
	float MoonPhaseAngle = 0.f;
	/** The phase law's factor over Lommel-Seeliger (the sky material's MoonLook.z) and the moonlight now over the
	 * full moon's at the zenith (phase law, air, altitude). */
	float MoonLawFactor = 1.f;
	float MoonLight01 = 0.f;
	/** The ephemeris' own (geocentric) lit fraction, for the report. */
	double MoonPhaseEphemeris = 0.0;
	bool bPlayerOutdoors = false;
	float OutdoorsVote = 0.f;
	float LastShift = 0.f;
	/** The level's own exposure bias (the club's post process volume), read at BeginPlay. */
	float BaseExposureBias = 0.f;
	float OutdoorWeight = 0.f;
	TWeakObjectPtr<APawn> LitPawn;
};
