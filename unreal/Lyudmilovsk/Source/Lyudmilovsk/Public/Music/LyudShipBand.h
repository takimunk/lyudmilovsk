#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "Tickable.h"
#include "LyudShipBand.generated.h"

class AActor;
class UAudioComponent;
class USoundWaveProcedural;
namespace LyudBand { class FPlayer; struct FBook; }

/**
 * The motor ship's quartet «Полуденный квартет» (docs/ship.md "The band"): piano, double bass, brushes and a reed, played
 * live by a synthesiser from lead sheets (Private/Music/ShipBandModel.h, Content/Music/band_tunes.txt; nothing recorded,
 * nothing to import, checked offline by tools/bandsim/run.sh).
 *
 * While the hero is aboard and one of the programme events concert1, captains_dinner, concert2 is on (ULyudCruise), the set of
 * that name plays from the bandstand in the music saloon: loud in the saloon, softer through the lobby and on the promenade
 * outside the saloon's windows, faint and muffled on the other decks. The captain's dinner is the soft trio, also relayed to the
 * dining saloon below. Where the set is, is a pure function of the voyage's clock: after a sleep, a clock jump or a load the band
 * is in the bar it would have reached. The title of a tune that begins while the hero is in the saloon is shown once.
 *
 * Console: lyud.Band 0|1, lyud.Band.Play [tune id|next|stop] [soft] (anywhere, not placed in the world), lyud.Band.Report.
 * Developer mode: band.play_next.
 */
UCLASS()
class LYUDMILOVSK_API ULyudShipBand : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudShipBand();
	virtual ~ULyudShipBand() override;

	static ULyudShipBand* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudShipBand, STATGROUP_Tickables); }

	/** The band is sounding (a set's tune, or a test). */
	bool IsPlaying() const;
	/** The tune sounding now: its id ("saints"), empty in a pause; and its title's string key ("band.tune.saints.title"). */
	FString CurrentTuneId() const;
	FString CurrentTuneKey() const;
	/** The set being played ("concert1", "captains_dinner", "concert2"); None outside them or when the hero is not aboard. */
	FName CurrentSet() const { return SetNow; }
	/** The number of the set's programme being played, from 0; -1 in a pause, an interval or a test. */
	int32 CurrentNumber() const { return SlotNow; }

	/** The tunes of band_tunes.txt in its order. */
	int32 NumTunes() const;
	FString TuneId(int32 Index) const;
	/** Plays a tune here and now, not placed in the world (a test); bSoft: the dinner trio. False: no such tune. */
	bool PlayTest(const FString& Id, bool bSoft = false);
	/** The tune after the last one tested. */
	void PlayNext();
	void StopTest();
	FString Report() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	TPimplPtr<LyudBand::FPlayer> Player;
	TPimplPtr<LyudBand::FBook> Book;
	void LoadBook();
	void LoadEventTimes();
	/** Voyage seconds (since Friday 18:00) at which each of the three sets begins: Content/Cruise/cruise.json "t0". */
	double SetStart[3] = { 10800.0, 90000.0, 95400.0 };

	UPROPERTY(Transient)
	TObjectPtr<USoundWaveProcedural> Wave;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Voice;
	UPROPERTY(Transient)
	TObjectPtr<AActor> Holder;
	TArray<int16> Pcm;
	TArray<float> Mix;
	bool EnsureVoice();
	void StartVoice(bool bInWorld, const FVector& At);
	void StopVoice();
	void Pump(float DeltaTime);

	/** A few times a second: which set, which tune, where the listener is. */
	void Decide();
	void Place(const class ULyudCruise& Cruise, const FVector& Hero, bool bDinner);
	float PollClock = 0.f;
	FName SetNow;
	int32 TuneNow = -1;
	int32 SlotNow = -1;
	uint32 SeedNow = 0;
	bool bTest = false;
	int32 TestTune = -1;
	bool bVoiceInWorld = false;
	bool bHeld = false;
	bool bInSaloon = false;
	/** The level and the muffling the listener's place asks for, and where they are now (smoothed every tick). */
	float WantGain = 1.f, WantLowPass = 20000.f;
	float Gain = 0.f, LowPass = 20000.f;
	float SilentFor = 0.f;
	/** The performance last announced (its seed): a title is shown once. */
	uint32 Announced = 0;
};
