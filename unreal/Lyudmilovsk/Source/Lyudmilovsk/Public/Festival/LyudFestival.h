#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "LyudFestival.generated.h"

namespace LyudFest { class FCalendar; }
class UAudioComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class USoundBase;
class UStaticMeshComponent;

/** Where a festival happens: a spot of the city plan (Content/City/city_plan.json, Content/PeopleData/town_plan.json)
 *  or, while there is none of that kind, the nearest fitting place of the people's map. */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudVenue
{
	GENERATED_BODY()

	/** What was asked for (square, fair, beach, pier, promenade, park, sunrise_point, or a spot id). */
	UPROPERTY(BlueprintReadOnly, Category = "Festival") FName Name;
	/** What it was found as (city_plan:<id>, town_plan:<id>, place:<id>). */
	UPROPERTY(BlueprintReadOnly, Category = "Festival") FString Source;
	/** The middle of it on the ground, cm. */
	UPROPERTY(BlueprintReadOnly, Category = "Festival") FVector Center = FVector::ZeroVector;
	/** Which way it faces (the stage stands at its far side, watchers face that way), deg. */
	UPROPERTY(BlueprintReadOnly, Category = "Festival") float Yaw = 0.f;
	/** How big it is: decorations go round within this, cm. */
	UPROPERTY(BlueprintReadOnly, Category = "Festival") float Radius = 1400.f;
	/** Out to sea from here (unit, horizontal). */
	UPROPERTY(BlueprintReadOnly, Category = "Festival") FVector SeaDir = FVector(1.f, 0.f, 0.f);
	UPROPERTY(BlueprintReadOnly, Category = "Festival") bool bValid = false;
	/** The city plan's spot id (S-SQUARE), what the people's gatherings are told; empty if it isn't a plan's spot. */
	UPROPERTY(BlueprintReadOnly, Category = "Festival") FString SpotId;
	/** The plan has a stage of its own by this venue (public_spaces.py builds it): where, which way. */
	UPROPERTY(BlueprintReadOnly, Category = "Festival") bool bPlanStage = false;
	UPROPERTY(BlueprintReadOnly, Category = "Festival") FVector StageAt = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Festival") float StageYaw = 0.f;
	/** The plan has stalls of its own here (the fair's): no festival stalls added. */
	UPROPERTY(BlueprintReadOnly, Category = "Festival") bool bPlanStalls = false;
};

/** One festival on now, for the UI and other systems (the restaurant's festival menu). */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudFestivalNow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Festival") FName Id;
	/** The running part of its program (fair, concert, fireworks, ...); None between parts. */
	UPROPERTY(BlueprintReadOnly, Category = "Festival") FName Phase;
	UPROPERTY(BlueprintReadOnly, Category = "Festival") FName Venue;
	UPROPERTY(BlueprintReadOnly, Category = "Festival") FText Name;
	/** Hours since 00:00 of its first day. */
	UPROPERTY(BlueprintReadOnly, Category = "Festival") float Hours = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLyudFestivalEvent, FName, Festival, FName, Phase);

/**
 * The town's holidays (docs/festivals.md): Content/PeopleData/festivals.json read by LyudFest::FCalendar on the game's
 * calendar (UGameCalendar). On a festival's day it dresses its venues (bunting, lanterns, flags, a stage, fair stalls, a
 * New Year tree, wreaths, candles, birdhouses), runs its program's shows (fireworks over the sea, sky lanterns, a bonfire,
 * music and stage lights), tells the people's life where to gather (UPeopleLifeSubsystem::SetGathering, when it has it)
 * and closes the offices and schools on public holidays (SetDayOff). The first snow of a winter fires first_snow.
 * Saved: which triggers fired when. Console: lyud.Festival.List / Report / Go <id> [phase] / Fire <trigger>; lyud.Festivals 0 off.
 */
UCLASS()
class LYUDMILOVSK_API ULyudFestivalSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudFestivalSubsystem();
	virtual ~ULyudFestivalSubsystem() override;

	static ULyudFestivalSubsystem* Get(const UObject* WorldContext);

	/** Is a holiday on today (its day, or its night still running past midnight)? The weekly market does not count. */
	UFUNCTION(BlueprintPure, Category = "Festival") bool IsFestivalToday() const;
	/** The most important festival on now (None if none). */
	UFUNCTION(BlueprintPure, Category = "Festival") FName ActiveFestival() const;
	/** Its running phase (None between phases). */
	UFUNCTION(BlueprintPure, Category = "Festival") FName ActivePhase() const;
	/** Every festival on now, most important first. */
	UFUNCTION(BlueprintPure, Category = "Festival") TArray<FLyudFestivalNow> ActiveFestivals() const { return Active; }
	/** Is today a public holiday (offices and schools closed)? */
	UFUNCTION(BlueprintPure, Category = "Festival") bool IsDayOff() const { return bDayOff; }
	/** The restaurant's dishes for today's holidays (festivals.json "menu": item ids of Content/Restaurant/menu.json). */
	UFUNCTION(BlueprintPure, Category = "Festival") TArray<FName> FestivalDishes() const;
	/** What the fair's stalls sell today (festivals.json "foods"). */
	UFUNCTION(BlueprintPure, Category = "Festival") TArray<FName> FestivalFoods() const;
	/** Is this restaurant dish one of today's holidays' (the restaurant calls it by name)? */
	UFUNCTION(BlueprintPure, Category = "Festival") bool IsFestivalDish(FName ItemId) const;
	/** Till what hour (from today's 00:00, may pass 24) people stay up tonight for a festival; 0 their own bedtimes. */
	UFUNCTION(BlueprintPure, Category = "Festival") float StayUpUntil() const { return StayUp; }
	/** The festivals of the next Days days (date, id), for the diary and posters. */
	UFUNCTION(BlueprintCallable, Category = "Festival") TArray<FString> Upcoming(int32 Days = 30) const;
	/** The localized name of a festival. */
	UFUNCTION(BlueprintPure, Category = "Festival") static FText FestivalName(FName Id);
	/** Where a venue is (resolved once, then cached). */
	UFUNCTION(BlueprintCallable, Category = "Festival") FLyudVenue FindVenue(FName Name);

	/** Developer: moves the calendar to festival Id's day, at the start of Phase (or 1 hour before its first phase). */
	UFUNCTION(BlueprintCallable, Category = "Festival") bool GoTo(FName Id, FName Phase = NAME_None);
	/** A trigger festival happens (first_snow: the day after). */
	UFUNCTION(BlueprintCallable, Category = "Festival") void Fire(FName Trigger);
	UFUNCTION(BlueprintCallable, Category = "Festival") FString Report() const;

	/** A festival's phase started / ended. */
	UPROPERTY(BlueprintAssignable, Category = "Festival") FLyudFestivalEvent OnPhaseStart;
	UPROPERTY(BlueprintAssignable, Category = "Festival") FLyudFestivalEvent OnPhaseEnd;

	/** Saved: "trigger|YYYY-MM-DD" for every fired trigger. */
	UPROPERTY(SaveGame) TArray<FString> FiredTriggers;
	/** Saved: the days whose "today" note was shown (YYYY-MM-DD|id), so a reload doesn't repeat it. */
	UPROPERTY(SaveGame) TArray<FString> Announced;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bRunning; }

	LyudFest::FCalendar* GetCalendar() const { return Calendar.Get(); }

private:
	TPimplPtr<LyudFest::FCalendar> Calendar;
	bool bRunning = false;
	float Timer = 0.f;
	bool bDayOff = false;
	bool bDayOffSent = false;
	int32 LastDay = -1;
	TArray<FLyudFestivalNow> Active;
	TArray<FName> Dishes, Foods;
	float StayUp = 0.f;
	bool bStayUpSent = false;
	/** Gatherings told to the people (id -> still on this update). */
	TSet<FName> Gatherings;
	/** Phases running (festival.phase) for the start/end events and the shows. */
	TSet<FName> RunningPhases;
	TMap<FName, FLyudVenue> Venues;
	/** Venues not found, and when (real seconds): looked for again only after a while, not every update. */
	TMap<FName, double> VenueMissedAt;
	/** Whether the level has the plan's own stage / stalls (an actor tagged PlanStage / PlanStalls), looked for once. */
	bool bPlanTagsLooked = false, bPlanStageBuilt = false, bPlanStallsBuilt = false;
	/** The festival tunes (looked for once). */
	UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> MusicSounds;
	bool bMusicLooked = false;
	/** Decorations up, by festival.venue. */
	UPROPERTY(Transient) TMap<FName, TObjectPtr<AActor>> Decor;
	/** Shows running, by festival.phase.show. */
	UPROPERTY(Transient) TMap<FName, TObjectPtr<AActor>> Shows;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UAudioComponent>> Music;
	/** The calendar's seconds at the last update (0 before the first): a jump of the clock takes the old shows away. */
	double LastCal = 0.0;

	void Update();
	void LoadFired();
	void CheckFirstSnow(const FDateTime& Now);
	/** The face point of a phase (cm), ZeroVector if none. */
	FVector FacePoint(const FLyudVenue& V, const FString& Face, const FDateTime& Now) const;
	void StartShow(FName Key, const FString& Show, const FLyudVenue& V, double EndsAt);
	void StartMusic(FName Key, const FLyudVenue& V);
	/** A sound of a phase (drums, bells, the crowd) from /Game/Audio/Festival, looping by being started again. */
	void StartSound(FName Key, const TCHAR* Asset, const FVector& At, float Volume, float Size);
	/** Those sounds by asset name, looked for once (null: not imported). */
	UPROPERTY(Transient) TMap<FName, TObjectPtr<USoundBase>> LoopSounds;
	void Announce(const FString& Day, const FString& Id);
	/** The hero at the festival: prompts (light a sky lantern), what he watched and where he went (quest events). */
	void TickHero(float DeltaTime);
	/** Seconds the hero has spent watching each show now running (festival.phase.show). */
	TMap<FName, float> Watched;
	/** Quest events already sent this festival day ("day|event|target"). */
	TSet<FString> HeroEvents;
	/** Today's date key for the hero's events (set by Update, so the frame's code builds no strings), and the
	 *  venue visits / shows watched already sent today (by festival.phase / festival.phase.show). */
	FString HeroDay;
	TSet<TPair<FName, FName>> HeroVisited;
	TSet<FName> HeroWatched;
	int32 PromptKind = 0;
	TWeakObjectPtr<class ANpcTalkDirector> TalkDirector;
	bool HeroWished = false;
	int32 HeroLanternsTonight = 0;
	void HeroEvent(const FString& Day, FName Event, const FString& Target);
	/** The stand-in for the people's gatherings: orders free people to the venue (lyud.Festival.Stub). */
	void StubGather(class UPeopleLifeSubsystem* People, FName Key, const FLyudVenue& V, const FString& Mode, float Pull);
	/** Whether UPeopleLifeSubsystem::SetGathering exists in this build (known after the first call). */
	bool bHasGatherings = false;
	/** "festival.phase|person" asked once by the stand-in. */
	TSet<FString> StubOrdered;
	bool ResolveFromPlan(const FString& File, const FString& Want, FLyudVenue& Out) const;
	bool ResolveFromPeople(const FString& Want, FLyudVenue& Out) const;
	FVector Ground(const FVector& At) const;
	FVector SeaDirAt(const FVector& At) const;
	/** 0 by day .. 1 at night (for the lights). */
	float Night() const;
};

/** One venue's decorations for a festival: built once in code (FLyudMeshKit) from the decor kinds, lit at night. */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudFestivalDecor : public AActor
{
	GENERATED_BODY()

public:
	ALyudFestivalDecor();
	void Build(const FLyudVenue& Venue, const TArray<FString>& Kinds, uint32 Seed);
	/** Lights: 0 off (day) .. 1 full; Party: the stage lights' show on. */
	void SetLights(float Night, bool bParty);
	/** Where the wish bamboo stands (Tanabata), the hero ties a wish on it. */
	const TArray<FVector>& GetWishTrees() const { return WishTrees; }
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UMaterialInterface>> Mats;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Glows;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> StageGlows;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Lights;
	TArray<float> GlowBase;
	float LightLevel = -1.f;
	int32 NumStageLights = 0;
	TArray<FVector> WishTrees;
	bool bParty = false;
	float Clock = 0.f;
	UMaterialInterface* Mat(FName Name);
	FVector GroundAt(const FVector& P) const;
};

/** Fireworks over the sea: shells rise from the pier's end and burst into streaks of colour, with a flash and a
 *  boom that comes late by the distance; a finale at the end. Moving parts stay out of Lumen. */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudFireworks : public AActor
{
	GENERATED_BODY()

public:
	ALyudFireworks();
	/** From Launch (cm, on the water out to sea) till EndsAt (game seconds of the calendar). */
	void Setup(const FVector& Launch, const FVector& SeaDir, double InEndsAt);
	/** No more shells: the ones in the air burst and fade, then the actor goes. */
	void StopShow() { EndsAt = 0.0; }
	virtual void Tick(float DeltaTime) override;

private:
	struct FShell { int32 Comp = -1; float T = -1.f; float Rise = 1.6f; float Life = 2.8f; float Radius = 4000.f; FVector From, Burst; FLinearColor Color = FLinearColor::White; bool bBoom = false; };
	TArray<FShell> Shells;
	/** Booms still on their way (a shell far off has faded before its sound arrives): seconds to go, volume. */
	struct FBoom { float In = 0.f; float Vol = 1.f; };
	TArray<FBoom> PendingBooms;
	UPROPERTY(Transient) TObjectPtr<USoundBase> LaunchSound;
	UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> BoomSounds;
	UPROPERTY(Transient) TObjectPtr<USoundBase> CheerSound;
	UPROPERTY(Transient) TObjectPtr<class USoundAttenuation> LaunchAtt;
	UPROPERTY(Transient) TObjectPtr<class USoundAttenuation> CheerAtt;
	bool bCheered = false;
	void Boom(float Vol);
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Bursts;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Trails;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> BurstMats;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Flashes;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> BurstMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> TrailMesh;
	FVector Launch = FVector::ZeroVector, Sea = FVector(1.f, 0.f, 0.f);
	double EndsAt = 0.0;
	float NextShell = 0.f;
	uint32 Rng = 1;
	float Rand();
	void Fire(bool bFinale);
};

/** Sky lanterns: paper lanterns lit on the beach rising slowly over the sea, flickering, drifting with the wind. */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudSkyLanterns : public AActor
{
	GENERATED_BODY()

public:
	ALyudSkyLanterns();
	/** bOnWater: floating lanterns (toro nagashi, Loy Krathong) set on the sea at Beach (its Z the water's) drifting out. */
	void Setup(const FVector& Beach, const FVector& SeaDir, double InEndsAt, bool bOnWater = false);
	bool IsOnWater() const { return bWater; }
	/** One more lantern let go at At (the hero's own: a little brighter, it goes up first). False if all are in the air. */
	bool Release(const FVector& At, bool bHero);
	/** Whether a lantern is left to let go. */
	bool HasFree() const;
	/** No more lanterns let go by the town: the ones in the air fly on. */
	void StopShow() { EndsAt = 0.0; }
	FVector GetBeach() const { return Beach; }
	virtual void Tick(float DeltaTime) override;

private:
	struct FLantern { float T = -1.f; FVector P, V; float Phase = 0.f; };
	TArray<FLantern> Lanterns;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Comps;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Glow;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Mesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> WoodMat;
	TWeakObjectPtr<class ALyudSea> SeaActor;
	bool bWater = false;
	FVector Beach = FVector::ZeroVector, Sea = FVector(1.f, 0.f, 0.f);
	double EndsAt = 0.0;
	float NextRelease = 0.f;
	uint32 Rng = 7;
	float Rand();
};

/** A bonfire on the beach: a log pile, tongues of flame and a flickering light (Maslenitsa: a straw doll burns on it). */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudBonfire : public AActor
{
	GENERATED_BODY()

public:
	ALyudBonfire();
	void Setup(const FVector& At, bool bEffigy, double InEndsAt);
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Flames;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Effigy;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FlameMat;
	double StartsAt = 0.0, EndsAt = 0.0;
	float Clock = 0.f;
};

/** Shooting stars (the Perseids): now and then a short bright streak high across the sky, seen from wherever the hero is. */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudMeteors : public AActor
{
	GENERATED_BODY()

public:
	ALyudMeteors();
	void Setup(double InEndsAt);
	virtual void Tick(float DeltaTime) override;
	/** How many have fallen so far (the hero's wish: the first one he sees). */
	int32 NumFallen() const { return Fallen; }

private:
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Streak;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Glow;
	double EndsAt = 0.0;
	float Next = 3.f, T = -1.f, Life = 0.8f;
	FVector From, Dir;
	float Length = 0.f;
	int32 Fallen = 0;
	uint32 Rng = 99;
	float Rand();
};

/** Firecrackers (Lunar New Year, Diwali): long red strings of them on a pole going off in a rattle of flashes, sparks
 *  jumping on the ground, a flickering light and the rattle's sound, till the phase ends. */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudFirecrackers : public AActor
{
	GENERATED_BODY()

public:
	ALyudFirecrackers();
	void Setup(const FVector& At, const FVector& Facing, double InEndsAt);
	virtual void Tick(float DeltaTime) override;

private:
	struct FPop { float T = -1.f; float Life = 0.1f; };
	TArray<FPop> Pops;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Sparks;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> Sounds;
	UPROPERTY(Transient) TObjectPtr<class USoundAttenuation> Att;
	FVector Base = FVector::ZeroVector, Side = FVector(0.f, 1.f, 0.f);
	double EndsAt = 0.0;
	float Next = 0.f, Burn = 0.f;
	uint32 Rng = 11;
	float Rand();
};

/** Holi's colours: clouds of coloured powder thrown up over the crowd, billowing out and settling. */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudColorPowder : public AActor
{
	GENERATED_BODY()

public:
	ALyudColorPowder();
	void Setup(const FVector& At, float Radius, double InEndsAt);
	virtual void Tick(float DeltaTime) override;

private:
	struct FPuff { float T = -1.f; float Life = 2.5f; float Size = 200.f; FVector P, Drift; };
	TArray<FPuff> Puffs;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Comps;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Mats;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Mesh;
	FVector Center = FVector::ZeroVector;
	float Radius = 1200.f;
	double EndsAt = 0.0;
	float Next = 0.f;
	uint32 Rng = 13;
	float Rand();
};
