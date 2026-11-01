#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudSunGlare.generated.h"

class ALyudSky;
class SLyudSunGlareOverlay;
class UTexture2D;

/**
 * The sun as the eye meets it (docs/sun.md, "Glare"): blinding until the eye adapts, and the marks it leaves on the
 * retina. Wherever the player is (the coast, the sea, Europa: the same ALyudSky star), every frame it works out how
 * much of the star's light reaches the eye: whether the disk is on the screen and near the middle of the view, how
 * much of it the land, trees, buildings and the player's own world hide (a few line traces a few times a second), how
 * much the air, the haze and the clouds let through.
 *
 *  - Unadapted, the star is drawn far brighter than white (ALyudSky::SetGlare: the photosphere x lyud.Sun.Glare.Gain,
 *    stronger bloom) with a glow in the eye round it over everything in front; low over the horizon the air takes most
 *    of that and leaves it orange, pink, red.
 *  - Staring at it the hero squints, then (lyud.Sun.Glare.StareSeconds) blinks and says "ай" (line.hero.sun_ow.*, not
 *    more than once in lyud.Sun.Glare.OwEvery seconds); held on, the eye adapts over lyud.Sun.Glare.AdaptSeconds: the
 *    glare fades and the star shows as drawn, with its granulation, spots and prominences. A couple of seconds looking
 *    elsewhere (lyud.Sun.Glare.RecoverSeconds) and it is blinding again.
 *  - Afterimages: a low-resolution map of the retina in screen space (the eye moves with the view, so an afterimage
 *    stays where on the screen the sun was) takes the light of the disk: a short bright positive afterimage in the
 *    star's colour, and a bleached, slower one that shows as a dark blot in the complementary colours (violet in the
 *    middle, green-teal round it) once the eye moves on, fading over a few seconds, longer the longer one looked.
 *
 * Drawn as a Slate overlay under the HUD (no material, no render target): the retina map as a 192x108 texture uploaded
 * at most 30 times a second while an afterimage lasts, the eyelids and the glow as two small textures made once.
 * lyud.Sun.Glare 0 (or the Settings page) turns it all off: the star as drawn, no flashes, no blinks.
 */
UCLASS()
class LYUDMILOVSK_API ULyudSunGlare : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudSunGlare* Get(const UObject* WorldContext);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	/** Also while paused: the overlay is cleared under the menu. */
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** 0 fresh eye .. 1 adapted to the star. */
	UFUNCTION(BlueprintPure, Category = "Sun|Glare") float GetAdaptation() const { return Adapt; }
	/** How far into a stare the eye is (seconds of full sun in the middle of the view, falling off when it looks away). */
	UFUNCTION(BlueprintPure, Category = "Sun|Glare") float GetStare() const { return Stare; }
	/** The eyelids now, 0 open .. 1 shut (squint and blinks). */
	UFUNCTION(BlueprintPure, Category = "Sun|Glare") float GetLids() const { return Lids; }
	/** The strongest afterimage on the retina now (the bleach, 0..1). */
	UFUNCTION(BlueprintPure, Category = "Sun|Glare") float GetAfterimage() const { return MaxBleach; }
	/** The star's light reaching the eye now, 0..1 (on the screen, not hidden, through the air). */
	UFUNCTION(BlueprintPure, Category = "Sun|Glare") float GetSunInEye() const { return InEye; }
	/** The glare state in one line (tests, lyud.Sun.Glare.Debug). */
	UFUNCTION(BlueprintCallable, Category = "Sun|Glare") FString GetDebugLine() const;
	/** A fresh eye: no adaptation, no stare, no afterimages (lyud.Sun.Glare.Reset). */
	UFUNCTION(BlueprintCallable, Category = "Sun|Glare") void ResetEye();

	/** Blinded by a sudden light (stepping out of the dark into full sun, e.g. out of the hatch onto Europa): the view
	 *  whites out (Strength 0..1, 1 = nearly white) and the eye is fresh; the hero squints, says "ай" and blinks every
	 *  second or so, and each blink takes the whiteout down a notch (x lyud.Sun.Glare.FlashStep, 0.5: 3-5 blinks back
	 *  to normal). The sun's own glare goes on as usual afterwards. lyud.Sun.Glare 0 makes it do nothing. */
	UFUNCTION(BlueprintCallable, Category = "Sun|Glare") void BeginFlashBlind(float Strength = 1.f);
	/** BeginFlashBlind the first time only for Key (e.g. "EuropaHatch"), remembered in the save (ULyudSunGlareMemory);
	 *  true if it fired now. */
	UFUNCTION(BlueprintCallable, Category = "Sun|Glare") bool FlashBlindOnce(FName Key, float Strength = 1.f);
	/** The whiteout now 0..1. */
	UFUNCTION(BlueprintPure, Category = "Sun|Glare") float GetFlash() const { return Flash; }

	/** What the overlay draws (SLyudSunGlareOverlay reads these each paint). */
	struct FPaint
	{
		bool bRetina = false;
		FVector2f SunUV = FVector2f(0.5f, 0.5f);
		float SunRadiusUV = 0.f;
		float Halo = 0.f;
		float Veil = 0.f;
		/** The whiteout of a flash blind 0..1. */
		float Flash = 0.f;
		float Lids = 0.f;
		FLinearColor SunTint = FLinearColor::White;
		FLinearColor LidTint = FLinearColor::Black;
		FVector2f Wobble = FVector2f::ZeroVector;
	};
	const FPaint& GetPaint() const { return Paint; }
	UTexture2D* GetRetinaTexture() const { return RetinaTex; }
	UTexture2D* GetLidTexture(bool bUpper) const { return bUpper ? LidUpperTex : LidLowerTex; }
	UTexture2D* GetGlowTexture() const { return GlowTex; }

private:
	/** Where the sun is on the screen and how much of it the eye gets; false if there is no view or no sky. */
	bool Look(float DeltaTime);
	void UpdateEye(float DeltaTime);
	void UpdateRetina(float DeltaTime);
	void UploadRetina();
	void Ow();
	void ShowOverlay(bool bShow);
	void MakeTextures();

	TWeakObjectPtr<ALyudSky> Sky;
	TSharedPtr<SLyudSunGlareOverlay> Overlay;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> RetinaTex;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> LidUpperTex;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> LidLowerTex;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> GlowTex;

	/** The retina map: the fast positive afterimage and the slow bleach per cell, the star's light on it now. */
	TArray<float> Fast;
	TArray<float> Bleach;
	/** Upload buffers, used in turn (the render thread copies from one while the next is filled). */
	TArray<FColor> Upload[4];
	int32 UploadIndex = 0;
	double NextUpload = 0.0;
	bool bRetinaLive = false;
	float MaxBleach = 0.f;
	float MaxFast = 0.f;

	// the view and the star in it
	FVector2f SunUV = FVector2f(0.5f, 0.5f);
	float SunRadiusUV = 0.f;
	float Aspect = 16.f / 9.f;
	bool bSunOnScreen = false;
	/** Degrees from the middle of the view to the star's centre. */
	float GazeDeg = 180.f;
	/** The share of the disk the world leaves in sight (traced), eased. */
	float Visible = 1.f;
	float VisibleRaw = 1.f;
	double NextTrace = 0.0;
	/** The star's brightness for the eye 0..1 (air, haze, clouds), and all of it that reaches the eye now. */
	float Bright = 0.f;
	float InEye = 0.f;
	/** How much of it falls on the middle of the view (the stare). */
	float InGaze = 0.f;
	FLinearColor SunTint = FLinearColor::White;

	// the eye
	float Stare = 0.f;
	float Adapt = 0.f;
	bool bAdapting = false;
	float Lids = 0.f;
	float Squint = 0.f;
	/** Seconds into a blink (< 0: none). */
	float BlinkT = -1.f;
	float NextBlink = 0.f;
	double LastOw = -1e9;
	int32 OwIndex = 0;
	/** A small flinch of the head with the "ай": pitch put on and taken off again. */
	float FlinchT = -1.f;
	float FlinchDone = 0.f;
	float Time = 0.f;
	bool bLoggedVoice = false;
	/** A flash blind (BeginFlashBlind): the whiteout left and when the next blink comes. */
	float Flash = 0.f;
	float NextFlashBlink = 0.f;

	FPaint Paint;
};

/**
 * What the glare remembers across the game (saved as "SunGlare"): the one-time flash blinds already seen
 * (ULyudSunGlare::FlashBlindOnce). Back to a new game's at every level start; a load restores it right after.
 */
UCLASS()
class LYUDMILOVSK_API ULyudSunGlareMemory : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static ULyudSunGlareMemory* Get(const UObject* WorldContext);
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Sun|Glare") TArray<FName> FlashesSeen;
	/** The world it was last reset for (each level start resets once). */
	TWeakObjectPtr<UWorld> ResetFor;
};
