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

	/** Is any festival's day today (its decorations up or its program on)? The weekly market counts. */
	UFUNCTION(BlueprintPure, Category = "Festival") bool IsFestivalToday() const;
	/** The most important festival on now (None if none). */
	UFUNCTION(BlueprintPure, Category = "Festival") FName ActiveFestival() const;
	/** Its running phase (None between phases). */
	UFUNCTION(BlueprintPure, Category = "Festival") FName ActivePhase() const;
	/** Every festival on now, most important first. */
	UFUNCTION(BlueprintPure, Category = "Festival") TArray<FLyudFestivalNow> ActiveFestivals() const { return Active; }
	/** Is today a public holiday (offices and schools closed)? */
	UFUNCTION(BlueprintPure, Category = "Festival") bool IsDayOff() const { return bDayOff; }
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
	/** Gatherings told to the people (id -> still on this update). */
	TSet<FName> Gatherings;
	/** Phases running (festival.phase) for the start/end events and the shows. */
	TSet<FName> RunningPhases;
	TMap<FName, FLyudVenue> Venues;
	/** Decorations up, by festival.venue. */
	UPROPERTY(Transient) TMap<FName, TObjectPtr<AActor>> Decor;
	/** Shows running, by festival.phase.show. */
	UPROPERTY(Transient) TMap<FName, TObjectPtr<AActor>> Shows;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UAudioComponent>> Music;

	void Update();
	void LoadFired();
	void CheckFirstSnow(const FDateTime& Now);
	/** The face point of a phase (cm), ZeroVector if none. */
	FVector FacePoint(const FLyudVenue& V, const FString& Face, const FDateTime& Now) const;
	void StartShow(FName Key, const FString& Show, const FLyudVenue& V, double EndsAt);
	void StartMusic(FName Key, const FLyudVenue& V);
	void Announce(const FString& Day, const FString& Id);
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
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UMaterialInterface>> Mats;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Glows;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> StageGlows;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Lights;
	TArray<float> GlowBase;
	float LightLevel = -1.f;
	int32 NumStageLights = 0;
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
	virtual void Tick(float DeltaTime) override;

private:
	struct FShell { int32 Comp = -1; float T = -1.f; float Rise = 1.6f; float Life = 2.8f; float Radius = 4000.f; FVector From, Burst; FLinearColor Color = FLinearColor::White; bool bBoom = false; };
	TArray<FShell> Shells;
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
	void Setup(const FVector& Beach, const FVector& SeaDir, double InEndsAt);
	virtual void Tick(float DeltaTime) override;

private:
	struct FLantern { float T = -1.f; FVector P, V; float Phase = 0.f; };
	TArray<FLantern> Lanterns;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Comps;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Glow;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Mesh;
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
