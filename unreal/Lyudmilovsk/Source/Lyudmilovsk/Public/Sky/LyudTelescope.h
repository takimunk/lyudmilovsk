#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudTelescope.generated.h"

class ACameraActor;
class ALyudSky;
class SWidget;
class UTexture2D;
struct FLyudEyepieceView;

/** One thing the hero has seen in a telescope, and when (the game's date). */
USTRUCT()
struct FLyudSkySight
{
	GENERATED_BODY()

	/** LyudScope::SightId: moon_full, venus, jupiter_moons, saturn_ring... */
	UPROPERTY(SaveGame) FName Sight;
	UPROPERTY(SaveGame) FDateTime When;
};

/**
 * The hero's observation log (docs/telescopes.md): what he has seen through the town's telescopes. Saved with the game
 * ("SkyLog"). Every first sight writes a line in the diary, gives a stamp of the collection "sky" and tells the quests
 * ("observe <sight>"); Jupiter's moons are drawn anew every night they are looked at ("observe jupiter_night").
 */
UCLASS()
class LYUDMILOVSK_API ULyudSkyLog : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static ULyudSkyLog* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** A bit per LyudScope::ESight. */
	UPROPERTY(SaveGame) int32 SeenBits = 0;
	UPROPERTY(SaveGame) int32 JupiterNights = 0;
	UPROPERTY(SaveGame) int32 LastJupiterNight = -1000000;
	UPROPERTY(SaveGame) TArray<FLyudSkySight> Sights;

	bool HasSeen(FName Sight) const;
	int32 NumSeen() const;
	/** A new game: an empty log. */
	void Reset();
};

/**
 * Looking through a telescope (docs/telescopes.md). ULyudCityThings hands a thing over when its act's "screen" is
 * "scope" (a telescope with every eyepiece), "scope_coin" (a coin telescope: no x400) or "scope_sun" (a dark solar
 * filter on it: only the sun shows). While it is up:
 *
 *   - the view is the game's own, through a camera at the telescope with the eyepiece's narrow field: the real sky
 *     (ALyudSky: Europa with its phase, the red dwarf through a filter, the aurora, the clouds), the sea, the island,
 *     the town, the birds; round it the eyepiece's black. No second render: the bench of a look is the game's own frame;
 *   - what the sky material does not draw is drawn over it: the planets where they really stand (LyudScope, the plain
 *     C++ model in Private/Sky/TelescopeModel), Jupiter's moons, Saturn's ring, Venus' phase, the faint field stars;
 *   - the mouse moves the tube (slowly, with inertia; slower at high power), the wheel or A / D turn the focus (the
 *     camera's depth of field: a ship, a bird and the sky each want their own), W / S change the eyepiece, E or Esc
 *     step back;
 *   - a sharp steady look at something counts: the log (ULyudSkyLog), the diary, the stamp, the quests; a bird held
 *     sharp in the field is met as through the binoculars (ULyudBirdBook);
 *   - the sun without a filter whites the view out and the hero steps back.
 *
 * Console: lyud.Scope.Look [coin|sun] (a telescope where the hero stands, looking where he looks), lyud.Scope.Aim
 * <body|alt az>, lyud.Scope.Power <0..3>, lyud.Scope.Focus <0..1>, lyud.Scope.End, lyud.Scope.Log [clear|all],
 * lyud.Scope.Dof <0|1> (the depth of field), lyud.Scope.Debug 1 (the state on the log every second).
 */
UCLASS()
class LYUDMILOVSK_API ULyudTelescope : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudTelescope* Get(const UObject* WorldContext);

	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudTelescope, STATGROUP_Tickables); }
	/** Nothing runs unless somebody is looking. */
	virtual bool IsTickable() const override { return bLooking; }

	/** The hero puts his eye to the telescope at At (world, the middle of the instrument) that faces FacingYaw. Kind:
	 *  "scope", "scope_coin" or "scope_sun". Caption: a line shown under the eyepiece for the first seconds. */
	bool Begin(const FVector& At, float FacingYaw, const FString& Kind, const FText& Caption);
	void End();
	bool IsLooking() const { return bLooking; }

	/** Console and tests. */
	void AimAt(double AltDeg, double AzDeg);
	bool AimAtBody(const FString& Body);
	void SetEyepiece(int32 Index);
	void SetFocus(float Knob);
	FString Report() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	void UpdateCamera(float DeltaTime);
	void UpdateSkyView();
	void UpdateSights(float DeltaTime);
	void UpdateBirds(float DeltaTime);
	void UpdateTextures();
	void Say(const FText& Text, float Seconds);
	void Noted(int32 Sight, bool bFirst, bool bNewNight);
	ALyudSky* FindSky();
	FVector Direction(double AltDeg, double AzDeg) const;
	void MakeBrushes();

	bool bLooking = false;
	bool bFilter = false;
	int32 MaxEyepiece = 3;
	int32 EyepieceIndex = 0;
	float Knob = 0.4f;
	FVector Mount = FVector::ZeroVector;
	double AimAlt = 2.0, AimAz = 0.0, AimVAlt = 0.0, AimVAz = 0.0;
	/** The camera's horizontal field now and the one it eases toward (an eyepiece change is a short zoom). */
	float FovNow = 30.f;
	float FieldNow = 15.f;
	uint64 BeginFrame = 0;
	double BeganAt = 0.0;
	/** The sun came into the unfiltered field: the white-out's clock (>= 0 while it runs). */
	float WhiteOut = -1.f;
	bool bBlocked = false;
	float BlockedDistance = 0.f;
	float TraceClock = 0.f;
	float BirdClock = 0.f;
	int32 BirdHeld = -1;
	float BirdHeldFor = 0.f;
	float SayLeft = 0.f;
	float DebugClock = 0.f;
	float SavedStars = -1.f;
	/** The level's camera f-number and ISO (its manual exposure), so the eyepiece's f-number leaves the brightness alone. */
	float BaseFstop = 4.f;
	float BaseIso = 100.f;
	/** The look held on a sight (LyudScope::FWatch). */
	int32 WatchSight = -1;
	double WatchHeld = 0.0;
	bool bWatchCounted = false;
	/** The body the tube follows as the sky turns (-1: none). */
	int32 FollowBody = -1;
	double FollowAlt = 0.0, FollowAz = 0.0;
	uint32 PictureKeys[5] = { ~0u, ~0u, ~0u, ~0u, ~0u };

	UPROPERTY(Transient) TObjectPtr<ACameraActor> Camera;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> MaskTexture;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> DotTexture;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> Pictures[5];
	TWeakObjectPtr<ALyudSky> Sky;
	TWeakObjectPtr<APlayerController> LockedPC;
	TSharedPtr<SWidget> Widget;
	TSharedPtr<FLyudEyepieceView> View;
};
