#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudCinema.generated.h"

namespace LyudFilm { struct FModel; }
class AActor;
class SLyudCityPage;
class UAudioComponent;
class UFileMediaSource;
class UMaterialInstanceDynamic;
class UMediaPlayer;
class UMediaTexture;
class UProceduralMeshComponent;

/**
 * The Rassvet's newsreel (docs/cinema.md): before a session of the hall's acts (cinema_week, cinema) the projectionist runs the
 * day's real films, public-domain shorts of 1895-1908 fetched by tools/films/fetch_films.py into Content/Films/<id>.mp4.
 *
 * ULyudCityThings asks Begin() when the hero sits down for a session; on yes it holds the session's own show until Release.
 * Meanwhile the hall's lamps go out (ULyudWindowLights), the film plays on a picture laid on the screen (a UMediaTexture on
 * M_FilmScreen), a beam with dust comes from the booth's porthole (M_FilmBeam), the projector whirrs, the lecturer's lines run
 * as subtitles (ru/en/ja, Content/Localization/films) and the hall laughs at its places. E, Esc or getting up leaves; the end
 * of the last film hands over to the session's feature. Films seen are saved ("Cinema") and counted in a toast.
 *
 * Data: Content/City/cinema.json (tools/films/films.py), model Private/City/CinemaModel.h (tools/cinemasim checks it). Missing
 * films are skipped; none at all: Begin says no and the session goes straight to its feature with a note. Without the two
 * materials (unreal/scripts/make_cinema_materials.py) the films still run, as subtitles over the hall, and the log says why.
 *
 * Console: lyud.Cinema.Status, lyud.Cinema.Play [film id] (the day's newsreel, or one film, at the Rassvet, no session),
 * lyud.Cinema.Stop. Dev action cinema.play.
 */
UCLASS()
class LYUDMILOVSK_API ULyudCinema : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudCinema();
	virtual ~ULyudCinema() override;

	static ULyudCinema* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudCinema, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return Running >= 0; }

	/**
	 * A session of Act at Plot starts for the hero (seated: on a hall seat). True: the newsreel runs now and the caller holds its
	 * show until ULyudCityThings::ReleaseHeldShow; false: no newsreel here (another act, no films fetched, already running).
	 */
	bool Begin(FName Plot, FName Act, bool bSeated);

	/** Runs the day's newsreel (or only FilmId) at the first cinema, with no session after it (the console, dev mode). */
	bool PlayLoose(FName FilmId);

	/** Ends the newsreel now (bFinished: as if it ran out, the session follows; else he left). */
	void Stop(bool bFinished);

	bool IsRunning() const { return Running >= 0; }
	/** The films, fetched or missing, seen, and today's programme (loads the data if it is not yet). */
	FString Status();

	/** The Rassvet's films seen by the hero (ids), saved. */
	UPROPERTY(SaveGame) TArray<FName> SeenFilms;

private:
	bool LoadModel();
	bool StartFilms(int32 Cinema, const TArray<int32>& Films, bool bSeated, bool bLoose);
	void BuildRig(int32 Cinema);
	void FreeRig();
	void StartCard();
	void StartFilm();
	void EndFilm(bool bWatched);
	void ShowCaption(const FText& Title, const FText& Body);
	void HideCaption();
	FString FilmPath(int32 Film) const;
	TArray<int32> TodaysFilms(int32 Cinema) const;

	TUniquePtr<LyudFilm::FModel> Model;
	bool bLoaded = false;

	// the run: the cinema, its films in order and where we are
	int32 Running = -1;
	TArray<int32> Queue;
	int32 QueueAt = -1;
	enum class EPhase : uint8 { Card, Opening, Playing } Phase = EPhase::Card;
	double PhaseStart = 0.0;
	double LastProgress = 0.0;
	float PlayedShare = 0.f;
	int32 Caption = -2;          // the lecturer's line shown (-1 none, -2 the card)
	bool bSeatedRun = false;
	bool bLooseRun = false;
	uint64 StartFrame = 0;
	FVector StartAt = FVector::ZeroVector;

	UPROPERTY(Transient) TObjectPtr<UMediaPlayer> Player;
	UPROPERTY(Transient) TObjectPtr<UMediaTexture> FilmTexture;
	UPROPERTY(Transient) TObjectPtr<UFileMediaSource> Source;
	UPROPERTY(Transient) TObjectPtr<AActor> Rig;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> ScreenMesh;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> BeamMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ScreenMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BeamMID;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Whirr;
	TSharedPtr<SLyudCityPage> Page;
};
