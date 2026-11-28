#pragma once

#include "CoreMinimal.h"
#include "LyudSave.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "Tickable.h"
#include "LyudMusic.generated.h"

class AActor;
class UAudioComponent;
class USoundBase;
class SWidget;
struct FCityActInfo;
namespace LyudMusic { class FModel; struct FFacts; }

/**
 * Records, the jukebox and the town radio «Волна Людмиловска» (docs/music.md). The rules live in plain C++
 * (Private/Music/MusicModel.h, checked offline by tools/musicsim/run.sh), the data in Content/City/music.json and the words in
 * Content/Localization/music (tools/music/music.py writes both).
 *
 * It listens to the town's things (ULyudCityThings::ActHook): E at a jukebox plays the record of the line it showed (5 roubles,
 * a stamp of the "records" collection); E at a record player puts on the next of the hero's own records (items record_*,
 * sold at the «Волна» records shop and the department store), or the house record when he has none; E at a radio switches it
 * on or off; E at the hits board of «Волна» posts a request on a postcard (one postcard used), read in the next evening's
 * request show. A record plays from its player in 3D, once to its end. The station is a pure function of the game clock (one
 * game second is one real second), so every radio plays the same programme at the same moment and a radio switched on comes
 * in at the right second of the record that is on: music hours, news (with the town paper's front page when there is one),
 * the weather and the tide, the request show at eight, the shipping forecast at eleven, the sign-off at one, off air till six.
 * The nearest radio that is on within a few metres plays it (one audio component moved to it); its words come as subtitles.
 * Shops', cafés' and kiosks' radios are on in the day by themselves. While a record or a radio is heard the ambient music
 * holds off (ULyudSoundscapeSubsystem::SetMusicHold).
 *
 * Nothing per frame but a distance test: the radio and the record are checked four times a second; one audio component each,
 * made once; the cues are loaded on first use and kept.
 * Console: lyud.Music.Radio (the air now to the log), lyud.Music.Program [day] (a day's programme to the log),
 * lyud.Music.GiveRecords, lyud.Music.Request (a request for tonight), lyud.Music.Stop. Developer mode: group "music".
 */
UCLASS()
class LYUDMILOVSK_API ULyudMusic : public UTickableWorldSubsystem, public ILyudSaveable
{
	GENERATED_BODY()

public:
	ULyudMusic();
	virtual ~ULyudMusic() override;

	static ULyudMusic* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudMusic, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bLoaded; }

	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;

	/** The air now as one line of text (the console, the log). */
	FString DescribeAir() const;
	/** A day's programme (0 = today) to the log, words in the current language. */
	void LogProgram(int32 DaysAhead) const;
	/** Developer mode: every record into the pocket. */
	void GiveAllRecords();
	/** Developer mode: a request as if posted yesterday, read at eight tonight. */
	void RequestTonight();
	/** Stops the record playing and turns every radio the hero switched on off. */
	void StopAll();

	/** For the subtitle widget: the caption heard now and its fade. */
	const FText& GetCaption() const { return Caption; }
	float GetCaptionAlpha() const { return CaptionAlpha; }

	UPROPERTY(SaveGame) TMap<FString, FString> Saved;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	struct FRadio
	{
		FString Id, Act;
		FVector At = FVector::ZeroVector;
	};

	bool LoadData();
	void BindCity();
	void StartOver();
	bool OnCityAct(const FCityActInfo& Info, FText& Line);
	long long Today() const;
	double SecondsOfDay() const;
	int32 Weekday() const;
	void RefreshFacts(bool bForce);
	USoundBase* Cue(int32 Track);
	USoundBase* Sound(const TCHAR* Path);
	UAudioComponent* MakeAudio(const TCHAR* Name);
	float MusicLevel() const;
	void PlayDisc(int32 Record, const FString& Thing, const FVector& At);
	void TickDisc(const FVector& Hero);
	void TickRadio(const FVector& Hero, float DeltaTime);
	const FRadio* NearestRadioOn(const FVector& Hero) const;
	FText Words(const FString& Key, const TArray<FString>& Args, const TArray<bool>& bKeys) const;
	FText NowOnAir() const;
	void TickRequest();
	void ShowSubtitle(bool bShow);
	/** His records: in the pocket, and for the player of his own room the ones standing on its shelves and nails too. */
	TArray<int32> OwnedRecords(bool bWithRoom = false) const;
	void Stamp(int32 Record);

	TPimplPtr<LyudMusic::FModel> Model;
	TPimplPtr<LyudMusic::FFacts> Facts;
	bool bLoaded = false;
	bool bCityBound = false;
	float Timer = 0.f;
	double FactsAt = -1e9;

	UPROPERTY(Transient) TObjectPtr<AActor> Holder;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Disc;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Radio;
	UPROPERTY(Transient) TMap<int32, TObjectPtr<USoundBase>> CueCache;
	UPROPERTY(Transient) TMap<FString, TObjectPtr<USoundBase>> SoundCache;

	// the record on a player or a jukebox
	int32 DiscRecord = -1;
	FString DiscThing;
	FVector DiscAt = FVector::ZeroVector;
	double DiscEnds = 0.0;      // world time seconds
	bool bDiscHeard = false;

	// the radios of the town (from the city's things once they are read) and the one playing now
	TArray<FRadio> Radios;
	bool bRadiosScanned = false;    // looked once (a town without radios isn't searched four times a second)
	FString RadioThing;
	int32 RadioTrack = -1;
	double RadioStart = -1.0;
	double RadioPlayedAt = 0.0, RadioOffsetAtPlay = 0.0;   // world time it was started, and from which second of the record
	float RadioVolume = 0.f;
	bool bRadioStatic = false;
	bool bRadioHeard = false;
	double LastSfxSec = -1.0;
	int64 HeardRequestDay = -1;

	// the caption on screen
	FText Caption;
	FString CaptionId;
	float CaptionAlpha = 0.f;
	float CaptionFor = 0.f;
	TSharedPtr<SWidget> SubtitleWidget;
	bool bMusicHeld = false;
};
