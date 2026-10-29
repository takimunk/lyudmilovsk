#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudSoundscape.generated.h"

class UAudioComponent;
class USoundAttenuation;
class USoundBase;
class ULyudAmbienceSynth;

/**
 * Everything the world sounds like around the player, and the music (docs/sound.md):
 *
 *  - the outdoor bed, synthesised live from the world's state (Private/Audio/AmbienceDSP.cpp): the wind at the ear
 *    (ULyudWindSubsystem, its gusts, from where it blows), the leaves of the trees around in this season and the pines,
 *    the surf of the nearest beach (ALyudSea's waves, how far the waterline is and in which direction), rain or snow and
 *    roofs overhead (ULyudWeatherSubsystem), crickets on warm summer nights, thunder after lightning; indoors it all
 *    comes muffled through the walls (ALyudSky's outdoors test), under water it nearly goes;
 *  - recorded-style sounds (audio/sounds.json, generated, /Game/Audio): the cats and dogs of UAnimalLifeSubsystem
 *    (bark, growl, whine, meow, purr, hiss where they are), a ship's horn out at sea, the foghorn in mist, the bell buoy,
 *    dogs barking across the valley at night, the town's far murmur, and in the computer club its room tone, people
 *    typing and clicking and old consoles bleeping;
 *  - music: ambient cues (/Game/Audio/Music/MU_<moods>_<n>) picked by the moment (club, dawn, day, dusk, night, rain,
 *    storm, snow, sea, flight), each played once and followed by a silence, cross-faded when the place changes a lot.
 *
 * It starts by itself in every game world. Volumes for the settings menu: SetVolumes. While the player is a bird
 * (ABirdPawn in the air or on the ground) the ground's wind bed is muted: the flight has its own (ULyudWindAudioComponent).
 *
 * Console: lyud.Sound 0 silences it all (A/B), lyud.Music 0 the music, lyud.Music.Next plays a cue now,
 * lyud.Music.Mood <name> forces the mood, lyud.Sound.Debug 1 prints the state on screen.
 */
UCLASS()
class LYUDMILOVSK_API ULyudSoundscapeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudSoundscapeSubsystem* Get(const UObject* WorldContext);
	/** For Blueprints and Python: unreal.LyudSoundscapeSubsystem.get_soundscape(world). */
	UFUNCTION(BlueprintPure, Category = "Sound", meta = (WorldContext = "WorldContext"))
	static ULyudSoundscapeSubsystem* GetSoundscape(const UObject* WorldContext) { return Get(WorldContext); }

	/** 0..1 each; the effective level of a sound is Master x its own. */
	UFUNCTION(BlueprintCallable, Category = "Sound")
	void SetVolumes(float Master, float Ambience, float Music, float Effects);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound") float MasterVolume = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound") float AmbienceVolume = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound") float MusicVolume = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound") float EffectsVolume = 1.f;

	/** The mood the music follows now (club, dawn, day, dusk, night, rain, storm, snow, sea, flight). */
	UFUNCTION(BlueprintCallable, Category = "Sound") FName GetMusicMood() const { return Mood; }
	/** The music cue playing (None in a silence). */
	UFUNCTION(BlueprintCallable, Category = "Sound") FName GetMusicCue() const { return CurrentCue; }
	/** 0 indoors (walls between the listener and the open air) .. 1 outdoors, smoothed: for sources outside to muffle. */
	UFUNCTION(BlueprintPure, Category = "Sound") float GetOutdoors() const { return Outdoors; }
	/** Starts a cue for the current mood now (cross-fading any playing one). */
	UFUNCTION(BlueprintCallable, Category = "Sound") void PlayNextCue();
	/** A one-shot of a pool (audio/sounds.json id) at Where, at the effects volume (the cable car, doors, lifts). */
	UFUNCTION(BlueprintCallable, Category = "Sound") void PlayEffect(FName Pool, const FVector& Where, float Volume = 1.f);
	/** A looping sound of a pool attached to Parent at the effects volume (a cabin's motor); null if the pool is empty.
	 *  The caller keeps it and stops it. */
	UAudioComponent* AttachLoop(FName Pool, USceneComponent* Parent, float Volume = 1.f);
	/** A spoken line (LyudText::LoadVoice) at Where, at the effects volume (the cable car's announcements). */
	UFUNCTION(BlueprintCallable, Category = "Sound") void PlayVoiceAt(USoundBase* Voice, const FVector& Where, float Volume = 1.f);
	/** Text for logs and tests: every input of the bed and the music's state. */
	UFUNCTION(BlueprintCallable, Category = "Sound") FString DebugReport() const;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FTreeCell { float Leafy = 0.f, Conifer = 0.f; };
	struct FPool { TArray<TObjectPtr<USoundBase>> Sounds; int32 Last = -1; };

	UPROPERTY(Transient) TObjectPtr<AActor> Holder;
	UPROPERTY(Transient) TObjectPtr<ULyudAmbienceSynth> Bed;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> MusicA;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> MusicB;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> RoomTone;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> TownLoop;
	UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> ClubLoops;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> NearAttenuation;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> FarAttenuation;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> RoomAttenuation;
	/** Every sound of /Game/Audio by pool name (S_dog_bark_big_01 -> dog_bark_big). */
	TMap<FName, FPool> Pools;
	/** Music cues and their moods. */
	struct FCue { TObjectPtr<USoundBase> Sound; FName Name; TArray<FName> Moods; float Seconds = 0.f; };
	TArray<FCue> Cues;
	UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> CueRefs;

	// what the bed hears, refreshed at a few Hz
	FVector Listener = FVector::ZeroVector;
	FVector ListenerRight = FVector::RightVector;
	float Outdoors = 0.f;
	float ParamTimer = 0.f, TreeTimer = 0.f, SurfTimer = 0.f, FarTimer = 1.f, ClubTimer = 0.f;
	float Leafy = 0.f, Conifer = 0.f, LeafSeason = 1.f, LeafDry = 0.f;
	bool bHaveShore = false;
	FVector Shore = FVector::ZeroVector;
	float SurfHeight = 0.f, SurfPeriod = 6.f;
	float Roof = 0.f;
	bool bBirdFlying = false;
	TMap<FIntPoint, FTreeCell> TreeGrid;
	bool bTreeGridBuilt = false;
	float LastWind = 0.f, LastGust = 0.f;
	double PlayTime = 0.0;
	TMap<FName, double> NextFar;
	int32 AnimalSoundsThisSecond = 0;
	double AnimalSecond = 0.0;

	// music
	FName Mood;
	FName CurrentCue;
	TArray<FName> RecentCues;
	bool bMusicOnA = true;
	double CueEnds = 0.0;
	double SilenceUntil = 8.0;
	double FadingOutUntil = 0.0;

	void Start(UWorld& World);
	void LoadSounds();
	void BuildTreeGrid();
	void UpdateListener();
	void UpdateTrees();
	void UpdateSurf();
	void UpdateBed();
	void UpdateFar(float DeltaTime);
	void UpdateClub();
	void UpdateMusic();
	FName ComputeMood() const;
	USoundBase* Pick(FName Pool);
	void PlayAt(FName Pool, const FVector& Where, float Volume, USoundAttenuation* Attenuation, float PitchJitter = 0.04f);
	float PanOf(const FVector& Where) const;
	float SunAltitude() const;
	float DayOfYear() const;

	UFUNCTION() void OnAnimalEvent(int32 AnimalId, FName Event);
	UFUNCTION() void OnLightning(FVector Location, float DistanceFromViewM);
};
