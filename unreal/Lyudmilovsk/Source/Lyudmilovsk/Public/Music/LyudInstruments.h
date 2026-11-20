#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "Tickable.h"
#include "LyudInstruments.generated.h"

class SWidget;
class SLyudInstrument;
class UAudioComponent;
class USoundWaveProcedural;
namespace LyudInstr { class FSynth; struct FTuneBook; }
namespace LyudTunes { struct FTune; struct FRhythm; }

/**
 * The hero plays (docs/instruments.md): the town's pianos (E at the piano in the hall «Своды», the school's music room,
 * the sanatorium lobby, the clubs, the library) and his own guitar (bought at the records shop «Волна»; "Play" in his
 * pockets, best by a bonfire). A keyboard view at the foot of the screen: an octave and a half on the keys A S D F G H J K
 * L ; ' (white) and W E T Y U O P (black), Z / X an octave down / up, Shift the sustain pedal, 1-8 the guitar's chords.
 * Q: the tunes he knows, picked with W/S or their numbers; the tune then plays as he presses E (or Space) in time with
 * its notes as they come to the line (an easy rhythm game: a note let pass stays silent). Esc shuts it.
 *
 * Tunes: Content/Music/tunes.txt (Private/Music/TunesModel.h); two are known from the start, the rest from sheet music
 * found in the town (the acts listed in each tune's sheet =: the music room's blackboard, the hall's music stands, the
 * library's music shelf); learned tunes are saved ("Instruments"). The notes are synthesised as he plays
 * (Private/Music/InstrumentSynth.h, a USoundWaveProcedural): nothing recorded, nothing to import.
 *
 * People drawn near him notice (town talk hero_plays), the ones standing sway; a tune played well ends in applause and a
 * word (hero_played). By a lit bonfire (ALyudBonfire, ALyudCampfire) with the guitar the people round it sing along
 * (hero_sing_<tune>). Console: lyud.Instr piano|guitar|close|learn <id|all>|play <id>|auto <id>|list.
 */
UCLASS()
class LYUDMILOVSK_API ULyudInstruments : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudInstruments();
	virtual ~ULyudInstruments() override;

	static ULyudInstruments* Get(const UObject* WorldContext);
	static const FName GuitarItem;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudInstruments, STATGROUP_Tickables); }

	/** Sits him at a piano / takes out the guitar (false: a screen is open, or no guitar for bGuitar). */
	bool Open(bool bGuitar);
	void Close();
	bool IsOpen() const { return Screen.IsValid(); }
	bool IsGuitar() const { return bGuitarNow; }

	/** The tunes, in the file's order; their titles are music.tune.<id>. */
	int32 NumTunes() const;
	FString TuneId(int32 Index) const;
	bool Knows(const FString& Id) const { return Learned.Contains(Id); }
	/** He learns a tune (from its sheet music): false if he knew it or there is no such tune. */
	bool Learn(const FString& Id);
	/** Starts the rhythm game on a known tune of the instrument open now. */
	/** bAuto: every note struck on time by itself (the console's "auto": recordings, the bench). */
	bool StartTune(const FString& Id, bool bAuto = false);
	void StopTune();

	/** For the widget: what is sounding, the rhythm game's state. */
	bool IsKeyDown(int32 Midi) const;
	int32 BaseMidi() const { return Base; }
	bool IsSustain() const;
	const LyudTunes::FRhythm* Rhythm() const;
	double MusicClock() const;
	/** By index (no strings built, for the widget): the tune playing now (-1 none), the one just finished (-1 none), whether he
	 *  knows a tune and can play it on the instrument open now. */
	int32 PlayingTune() const { return GameTune; }
	int32 FinishedTuneIndex() const { return DoneIx; }
	bool CanPlay(int32 Index) const;
	/** The tunes he can play on the instrument open now (their indices), for the list. */
	void PlayableTunes(TArray<int32>& Out) const;
	bool IsListOpen() const { return bList; }
	int32 ListCursor() const { return Cursor; }
	/** The last judgement shown over the line (0 none, 1 perfect, 2 good, 3 miss) and when. */
	int32 LastHit() const { return HitShown; }
	double LastHitAt() const { return HitAt; }
	/** The tune just finished: its stars (-1: none to show) until the next one starts. */
	int32 FinishedStars() const { return DoneStars; }
	FString FinishedTune() const { return DoneTune; }
	bool IsByBonfire() const { return bByFire; }

	UPROPERTY(SaveGame)
	TArray<FString> Learned;
	/** How many times each tune was played to its end with a star or more (the journal, quests). */
	UPROPERTY(SaveGame)
	TMap<FString, int32> Played;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	TPimplPtr<LyudInstr::FSynth> Synth;
	TPimplPtr<LyudInstr::FTuneBook> Book;
	TPimplPtr<LyudTunes::FRhythm> Game;
	int32 GameTune = -1;
	bool bLoaded = false;
	void LoadTunes();
	int32 FindTune(const FString& Id) const;

	UPROPERTY(Transient)
	TObjectPtr<USoundWaveProcedural> Wave;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Voice;
	TArray<int16> Pcm;
	TArray<float> Mix;
	void Pump(float DeltaTime);

	TSharedPtr<SLyudInstrument> Screen;
	uint64 OpenedFrame = 0;
	bool bGuitarNow = false;
	bool bLocked = false;
	void Lock(bool bLock);
	int32 Base = 60;
	TSet<int32> Down;
	void Keys();
	void FreeKey(int32 Midi, bool bPressed);
	bool bList = false;
	TArray<int32> ListBuf;
	int32 Cursor = 0;
	int32 HitShown = 0;
	double HitAt = -10.0;
	int32 DoneStars = -1;
	FString DoneTune;
	int32 DoneIx = -1;
	/** Per tune index: he knows it (Learned, kept in step by RefreshKnown). */
	TArray<bool> KnownIx;
	void RefreshKnown();
	/** The guitar's chords under a tune: the next chord to strum, the notes held for their lengths. */
	int32 NextChord = 0;
	bool bAutoPlay = false;
	/** A stroke of E in the rhythm game (by the player, or by the auto player). */
	void Strike(double Now);
	TArray<TPair<double, int32>> NoteOffs;
	void TickGame(double Now);
	void FinishTune();

	// the people round him
	bool bByFire = false;
	float PeopleTimer = 0.f;
	float PlayingFor = 0.f;
	float SingTimer = 0.f;
	void TickPeople(float DeltaTime);
	void Applause(int32 Stars);
	void Sing();
	bool FindBonfire(FVector& Out) const;

	// the town: pianos open the keyboard, the sheet music teaches tunes
	FDelegateHandle OpenHandle;
	void OnCityOpen(FName InScreen, int32 Thing);
	bool OnCityAct(const struct FCityActInfo& Info, FText& Line);
	FDelegateHandle LoadHandle;
	FDelegateHandle NewGameHandle;
	void StartOver();
	void AfterLoad();
};
