#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "LyudSave.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "Tickable.h"
#include "LyudWinterPlay.generated.h"

class AActor;
class AHeroCharacter;
class ALyudLadder;
class SWidget;
class UAudioComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
struct FWinterPlayState;

/** The view tipping over when the hero falls on the ice and coming back up as he gets up (ULyudWinterPlay sets Phase). */
UCLASS()
class LYUDMILOVSK_API ULyudIceFallCamera : public UCameraModifier
{
	GENERATED_BODY()

public:
	/** Seconds since the fall (-1: none), how long the fall and the getting up take, and which way he tips (+1 / -1). */
	float Phase = -1.f;
	float FallS = 1.5f;
	float GetupS = 1.1f;
	float Side = 1.f;
	virtual bool ModifyCamera(float DeltaTime, struct FMinimalViewInfo& InOutPOV) override;
};

/**
 * Winter play (docs/winter_play.md, interactives backlog item 18). The rules are plain C++ (Private/Winter/WinterPlayModel.h,
 * checked offline by tools/winterplaysim/run.sh), the places and numbers in Content/City/winter_play.json and the words in
 * Content/Localization/winter_play (tools/winter_play/winter_play.py writes both).
 *
 * The rink: the park pond's ice (ALyudPublicSpaces draws it on the calendar) gets a thickness from the weather's freezing
 * degree-days and cracks that fewer and narrower as it thickens. On the ice in skates (an item worn on the feet, sold at the
 * department store's sports counter) the hero glides: a stroke at a time while a move key is held, a long glide when let go,
 * turns held by the edge's grip, S into a hockey stop; a crack crossed fast or a turn too sharp throws him (he goes down
 * and gets up). In boots the ice is slippery. In skates on land he totters. The model's velocity drives the character's
 * movement (its friction and braking off while he is on the ice; put back when he leaves).
 *
 * The banya by the pier's root: a log banya built in code (a dressing room, a steam room with a stove and two shelves),
 * heated on its days (Wed, Fri, Sat, Sun 14-22; New Year's Eve and Day by day). E at the door pays and seats him on the
 * upper shelf; E pours a ladle on the stones (the room's heat and steam rise), G swishes the venik (one a visit); his heat
 * rises by the felt heat (the felt hat takes some off); Space takes him out. Hot, then cooled through by lying in the snow
 * (crouched in snow by the banya) or by the cold sea within a couple of minutes: a round; three rounds, the full banya. Cold
 * water builds a chill once the banya's heat is spent; past it the walruses haul him onto the deck. The walruses' ice hole
 * off the pier: one tile of the sea ice left open (ULyudWinterSubsystem::SetIceHole) with a rim and a frame, and a ladder
 * down from the deck (ALyudLadder) that is there all year. The club swims on Sunday mornings in season, everybody on New
 * Year's morning (the people's life: conditions rink, rink_busy, walrus, banya; UPeopleLifeSubsystem::SetCondition).
 *
 * Snow that packs (deep enough, near freezing): G with no feed in the hand scoops and throws a snowball (a flight under
 * gravity; it bursts on what it hits). Children at play near by join in and throw back for a couple of minutes, aiming
 * with a lead. Crouch + E on snow rolls a ball that grows as he pushes it; E leaves it, or lifts it onto a bigger one;
 * three are a snowman, then E gives it a carrot nose, coal eyes and twig arms. A thaw slumps them; they are saved.
 *
 * Stamps of the collection "winter" (ULyudCityThings::GiveStamp); quest events "winter <what>". The town radio reads the
 * rink's state with the weather (ULyudMusic asks RinkReport).
 *
 * Perf: nothing is rebuilt per frame: the buildings, the cracks and the snowmen are built once (again only when they
 * change); the flying snowballs and the rolled ball are a few pooled components moved while they fly; moving ones are out of
 * Lumen and the distance fields. Away from the pond, the banya and the snow nothing runs but a 4 Hz distance check.
 * Console: lyud.WinterPlay.Report, lyud.WinterPlay.Ice -1|cm (force the rink's thickness), lyud.WinterPlay.Kit (skates,
 * venik, hat, towel, carrot, coal). Developer mode: group "winter".
 */
UCLASS()
class LYUDMILOVSK_API ULyudWinterPlay : public UTickableWorldSubsystem, public ILyudSaveable
{
	GENERATED_BODY()

public:
	ULyudWinterPlay();
	virtual ~ULyudWinterPlay() override;

	static ULyudWinterPlay* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudWinterPlay, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bLoaded; }

	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;

	/** The rink for the radio: 0 no ice (or summer), 1 open (Cm thick), 2 thin (Cm), 3 slush on it. */
	int32 RinkReport(float& OutCm) const;
	/** One line of what is going on (the console, the log). */
	FString Report() const;
	/** Developer mode: the winter kit into the pocket. */
	void GiveKit();
	/** Developer mode: the rink's thickness forced (cm; negative: the weather's). */
	void ForceIce(float Cm) { ForcedIceCm = Cm; IceTimer = 0.f; }
	/** Developer mode: a snowball fight with the children near (or the nearest ones in town brought to the pond's edge). */
	void StartFightNow();
	/** Developer mode: the hero hot from the steam (to try the snow and the hole). */
	void HeatUp();

	/** For the HUD. */
	float GetHeat() const;
	float GetCold() const;
	float GetSteam() const;
	int32 GetRounds() const;
	bool IsInSteamRoom() const { return bInSteam; }
	bool IsColdShown() const;
	float GetSplat() const { return Splat; }
	FText GetScoreLine() const;

	UPROPERTY(SaveGame) FString Saved;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	bool LoadData();
	void StartOver();
	void Build();
	void BuildBanya();
	void BuildHole();
	void BuildCracks();
	void BuildBallMeshes();
	UStaticMeshComponent* MakePart(UStaticMesh* Mesh, const FString& Name, bool bCollide, const FTransform& Xf, float CullCm, bool bMoving = false);
	USoundBase* Sound(const TCHAR* Path);
	void PlaySfx(FName Pool, const FVector& At, float Volume = 1.f);
	void Say(const FString& Key, const TArray<FString>& Args = TArray<FString>());
	void SayOne(const FString& Prefix, int32 Count);
	void Stamp(const FString& Piece);
	void Event(const FString& What);
	void SetPrompt(const FText& Text);

	// the clock and the weather, every few seconds
	void RefreshFacts(bool bForce);
	void SendConditions();

	// the rink and skating
	void TickRink(AHeroCharacter* Hero, float Dt);
	void EnterIce(AHeroCharacter* Hero);
	void LeaveIce(AHeroCharacter* Hero);
	void ShowBlades(AHeroCharacter* Hero, bool bShow);
	bool HeroOnPondIce(const AHeroCharacter* Hero) const;

	// the banya and the cold water
	void TickBanya(AHeroCharacter* Hero, float Dt);
	void EnterBanya(AHeroCharacter* Hero);
	void ExitBanya(bool bFaint);
	void TickBody(AHeroCharacter* Hero, float Dt);
	void TickHole(float Dt);
	bool BanyaOpenNow() const;
	FVector BanyaPoint(const double* Local) const;

	// snowballs, the fight, snowmen
	void TickSnow(AHeroCharacter* Hero, float Dt);
	void ThrowFromHero(AHeroCharacter* Hero);
	void Throw(const FVector& From, const FVector& Vel, int32 Owner);
	void TickBalls(AHeroCharacter* Hero, float Dt);
	void Burst(const FVector& At);
	void TickFight(AHeroCharacter* Hero, float Dt);
	void TickSnowman(AHeroCharacter* Hero, float Dt);
	void ShowSnowman(int32 Index);
	void HideSnowmen();
	float PacksAt(const FVector& Feet) const;

	// the HUD
	void ShowHud(bool bShow);

	/** The model's objects (Private/Winter/WinterPlayModel.h): the params, the ice, the skater, the room, the body, the fight, the
	 *  snowballs in flight, the cracks, the saved state. */
	TPimplPtr<struct FWinterPlayState> M;
	bool bLoaded = false;
	bool bBuilt = false;

	// facts
	float Timer = 0.f, IceTimer = 0.f, CondTimer = 0.f, HourTimer = 0.f;
	int32 Month = 1, Day = 1, Year = 2026, Weekday = 0;
	double Hour = 12.0;
	float AirC = 5.f, SeaC = 6.f, Melt = 0.f, Fdd = 0.f, SnowCm = 0.f;
	float ForcedIceCm = -1.f;
	int32 CracksWinter = -1, CracksCount = -1;
	double WorldTime = 0.0;

	// skating
	bool bOnIce = false;
	bool bMoveSaved = false;
	float SavedFriction = 8.f, SavedBraking = 2048.f, SavedAccel = 2048.f, SavedBrakingFactor = 2.f;
	bool bLandScaled = false;
	bool bBladesShown = false;
	bool bWasDown = false;
	float GlideVolume = 0.f;
	FVector LastFeet = FVector::ZeroVector;
	bool bLastFeet = false;
	float SkatedM = 0.f;
	bool bFirstGlide = false, bLongGlide = false;

	// the banya
	bool bInSteam = false;
	bool bVenikThisVisit = false;
	float VenikT = 0.f;
	float HotSayT = 0.f;
	bool bNearDoor = false;
	bool bSnowSaid = false;
	bool bHoleOpen = false;
	bool bCoalGiven = false;
	int32 CoalDay = -1;

	// snow
	float ScoopT = -1.f;
	float ThrowCooldown = 0.f;
	float Splat = 0.f;
	int32 Rolling = -1;            // the snowman whose next ball is being rolled (-1: none)
	float RollR = 0.f;
	FVector RollAt = FVector::ZeroVector;
	FRotator RollSpin = FRotator::ZeroRotator;
	bool bSpeedHeld = false;
	FText FightLine;
	float FightLineT = 0.f;

	// a fade's second half: what happens when the screen is black (1 into the banya, 2 out of it, 3 hauled onto the deck)
	int32 Pending = 0;
	float PendingT = 0.f;
	bool bExitAsked = false;
	bool bFaint = false;

	// prompts
	FText PromptShown;
	bool bPrompt = false;

	UPROPERTY(Transient) TObjectPtr<AActor> Holder;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> PropMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> GlowMid;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> Meshes;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> CrackPart;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> HoleParts;
	UPROPERTY(Transient) TObjectPtr<ALyudLadder> Ladder;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> ShelfSeat;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Lights;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> BladeL;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> BladeR;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> BallMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> SnowSphere;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> NoseMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> EyeMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> ArmMesh;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> BallPool;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> PuffPool;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> RollPart;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> ManParts;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Glide;
	UPROPERTY(Transient) TObjectPtr<ULyudIceFallCamera> FallCam;
	UPROPERTY(Transient) TMap<FString, TObjectPtr<USoundBase>> SoundCache;

	struct FPuff { int32 Part = -1; FVector Vel = FVector::ZeroVector; float Age = 0.f; };
	TArray<FPuff> Puffs;
	/** The snowmen's parts: per snowman, the first part index and how many (rebuilt only when one changes). */
	TArray<TPair<int32, int32>> ManSlices;
	TArray<int32> Kids;            // people ids of the town's children (ages from the params), read once
	bool bKidsRead = false;
	TSharedPtr<SWidget> HudWidget;
};
