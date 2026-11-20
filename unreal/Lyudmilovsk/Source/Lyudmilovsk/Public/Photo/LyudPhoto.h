#pragma once

#include "CoreMinimal.h"
#include "LyudSave.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "LyudPhoto.generated.h"

class APlayerController;
class AHeroCharacter;
class USceneCaptureComponent2D;
class UTexture2D;
class UTextureRenderTarget2D;
class SWidget;
namespace PhotoModel
{
	class FState;
	struct FShot;
	struct FDeveloped;
}

/**
 * The hero's film camera (docs/photo.md, backlog item 2). V raises it to the eye when the pocket holds a `camera`:
 * the view narrows to a 50 mm lens, bright frame lines show what the film will take, the mouse wheel turns the focus
 * ring, the left button fires the slow shutter, R winds the film back. A frame is a real picture of the view: one scene
 * capture into a small render target at the moment of the shot, made black-and-white, blurred by how sharp the frame
 * came out and written as a jpg under Saved/Photos/<game id>/. Nothing is captured and nothing ticks beyond a key poll
 * while the camera is down.
 *
 * The film's rules (36 frames, sharpness, the studio's next-morning prints, the hero's own darkroom, the album, the
 * photo goals) are plain C++ in Private/Photo/PhotoModel (tools/photosim). This subsystem is their hands in the world:
 * what is in the frame when the shutter falls (birds, the sunset, the aurora, a festival, a fresh catch, Maru, people,
 * the place), the studio's counter and the darkroom (city acts with "open": "photo_lab" / "photo_darkroom"), the album
 * (P), and what a development hands out: stamps, bird sightings, the quest event "photo <place>".
 *
 * Console: lyud.Photo.Raise 0|1, lyud.Photo.Shoot, lyud.Photo.Develop (every film, at once), lyud.Photo.Album, lyud.Photo.State.
 */
UCLASS()
class LYUDMILOVSK_API ULyudPhoto : public UTickableWorldSubsystem, public ILyudSaveable
{
	GENERATED_BODY()

public:
	ULyudPhoto();
	virtual ~ULyudPhoto() override;

	static ULyudPhoto* Get(const UObject* WorldContext);
	static const FName CameraId;
	static const FName FilmId;

	/** 0 down .. 1 at the eye. */
	float RaiseAmount() const { return Raise; }
	bool IsRaised() const { return bRaised; }
	void SetRaised(bool bUp);
	/** The focus ring's distance, metres (1e9: infinity). */
	float FocusDistance() const;
	int32 FramesLeft() const;
	bool HasFilm() const;
	/** The frame lines' size as a share of the viewport's height (width = 1.5 x that height). */
	static constexpr float FrameHeightShare = 0.72f;
	/** 1 just after the shutter, falling to 0: the viewfinder goes dark for the slow shutter. */
	float ShutterDark() const { return Dark; }

	/** Fires the shutter (the left button while raised): false with no film or no frame left. */
	bool Shoot();
	/** Winds the film back and takes it out. */
	bool Rewind();
	/** The studio's counter: prints that are ready come back, exposed films go in (StudioPrice each). */
	void UseStudio();
	/** The darkroom: starts the three baths when the pocket holds an exposed film. */
	void StartDarkroom();
	bool InDarkroom() const { return Bath >= 0; }
	/** The bath running now (0 developer, 1 stop, 2 fixer; -1 none), its seconds so far and its ideal. */
	int32 DarkroomBath() const { return Bath; }
	float DarkroomSeconds() const { return BathSeconds; }
	float DarkroomIdeal() const;
	void ToggleAlbum();
	bool IsAlbumOpen() const { return Album.IsValid(); }
	/** Developer mode: every film, in the camera or the pocket or at the studio, becomes prints now. */
	void DevelopAllNow();
	PhotoModel::FState& State();

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudPhoto, STATGROUP_Tickables); }
	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	void StartOver();
	double NowHours() const;
	FString PhotoDir() const;
	FString PhotoFile(int32 Roll, int32 Frame) const;
	/** What stands in the frame now, into the shot; the whole frame's sharpness. */
	void LookThrough(const AHeroCharacter& Hero, const FVector& Eye, const FRotator& Look, PhotoModel::FShot& Shot);
	/** The picture itself: one capture of the view, to a jpg. */
	void TakePicture(const FVector& Eye, const FRotator& Look, float Sharp, const FString& File);
	void HandOut(const PhotoModel::FDeveloped& Got);
	/** The quest event "photo <place>" for every place the album has a print of. */
	void TellPlaces();
	void Say(const TCHAR* Key, const TArray<FStringFormatArg>& Args = TArray<FStringFormatArg>());
	void ShowAlbumPage();
	void CloseAlbum();
	void FinishDarkroom();
	void LockHero(bool bLock);
	void EnsureOverlay();
	float CaptureFov(const APlayerController& PC) const;

	UPROPERTY(SaveGame)
	FString Saved;

	/** Names this game's folder of pictures (a save slot has no name of its own until it is written). */
	UPROPERTY(SaveGame)
	FString GameId;

	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> Target;

	/** The prints of the album page that is open. */
	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> PagePrints;

	TPimplPtr<PhotoModel::FState> Model;
	TSharedPtr<SWidget> Finder;
	TSharedPtr<SWidget> Album;
	bool bRaised = false;
	float Raise = 0.f;
	float NormalFov = 0.f;
	float Dark = 0.f;
	int32 FocusIndex = 0;
	int32 AlbumPage = 0;
	int32 Bath = -1;
	float BathSeconds = 0.f;
	float BathTimes[3] = { 0.f, 0.f, 0.f };
	bool bLocked = false;
};
