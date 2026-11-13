#pragma once

#include "CoreMinimal.h"
#include "LyudSave.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "Tickable.h"
#include "LyudCityThings.generated.h"

class AActor;
class ACameraActor;
class AHeroCharacter;
class ANpcTalkDirector;
class APlayerController;
class SLyudCityPage;
class USceneComponent;
struct FLyudCityLoad;
namespace LyudCity { class FModel; }

/**
 * Things to do inside the town's buildings (docs/city_interiors.md): sit on a chair, read at a shelf, look at a notice board,
 * buy at a counter, rest on a bed, play the piano, watch a film... Data: Content/City/interiors.json (tools/city/interiors.py),
 * the rules in the engine-free LyudCity::FModel (Private/City/CityThingsModel.h, checked by tools/citysim/run.sh).
 *
 * The file is read and parsed on a worker (things appear a moment after the level starts). Nothing is spawned per thing (the
 * furniture is part of the building meshes). A few times a second, and only while the hero is inside the box round all things,
 * the model picks the thing in view from a grid; a trace checks no wall is between. The prompt goes through
 * ULyudInventoryUI::SetPrompt (rebuilt only when the thing changes), E (polled, never consumed; not while the NPC talk has it)
 * runs the act: a page of text (SLyudCityPage, E turns it, walking away shuts it; lines may hold only by day, at night, on some
 * days), game minutes (a few minutes pass over half a second, longer stretches at once, with a fade when the act says so),
 * money, items, a shop (only while the staff is on shift; they greet the hero once a visit), sitting (AHeroCharacter::SitOn on
 * one pooled anchor), lying (the view from the pillow while the time passes), a screen show (a film's subtitles one after
 * another), a one-shot sound, quest events ("do"/act or the act's own; "visit"/plot at every use and on walking up to a
 * plot's things). Pages read, daily boards, per-day counts and uses are kept in the save ("CityThings").
 *
 * Console: lyud.City.Things, lyud.City.Do <thing id>, lyud.City.Things.Show 0|1, lyud.City.Weekday <mon..sun>.
 */
UCLASS()
class LYUDMILOVSK_API ULyudCityThings : public UTickableWorldSubsystem, public ILyudSaveable
{
	GENERATED_BODY()

public:
	ULyudCityThings();
	virtual ~ULyudCityThings() override;

	static ULyudCityThings* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudCityThings, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return NumThings() > 0 || Loading.IsValid(); }

	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;

	int32 NumThings() const;
	/** Runs a thing's act as if E was pressed at it (wherever the hero is); false for an unknown id. Logs what it did. */
	bool RunThingById(const FString& Id);
	/** Logs the count and the N things nearest the hero, with distances. */
	void LogNearest(int32 N) const;
	/** Runs the thing nearest the hero (developer mode). */
	bool RunNearest();
	/** Developer mode: today's limits start over. */
	void ResetDailyLimits();
	/** Debug labels of the things within 15 m (lyud.City.Things.Show). */
	void SetShowLabels(bool bShow) { bShowLabels = bShow; }
	bool AreLabelsShown() const { return bShowLabels; }

	/** Developer mode (interiors.open_all_hours): every act runs whatever the hour, the day and the staff. */
	static bool bOpenAllHours;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	/** The model's state as a flat map ("CityThings"): counts, pages, uses, plots visited. */
	UPROPERTY(SaveGame) TMap<FString, FString> Saved;

private:
	void StartLoad();
	/** The worker has parsed the file: the model takes it, the saved state (if a load came first) is applied. */
	void AdoptLoaded();
	void ApplySaved();
	void StartOver();
	/** Re-picks the thing in view (a few times a second). */
	int32 PickInView(const AHeroCharacter* Hero, const APlayerController* PC) const;
	/** The plot round the hero (a few times a second): a "visit" when it changes. */
	void TrackPlot(const AHeroCharacter* Hero);
	void SetPrompted(int32 Thing);
	void Run(int32 Thing, bool bFromConsole, bool bFromSeat = false);
	void ShowPage(const FText& Title, const FText& Body, bool bMore);
	void ClosePage();
	void Refuse(const FText& Why);
	bool Sit(int32 Thing, AHeroCharacter* Hero, bool bLie);
	/** Game minutes pass: a few (the people's life steps through them) over the next frames, more at once (it resettles). */
	void JumpMinutes(int32 Minutes, bool bBlockMove);
	void TickJump(float DeltaTime);
	void UnblockMove();
	void BeginLie(int32 Thing, bool bFade, bool bTimePasses);
	void ShowLieView(bool bCut);
	void EndLie(bool bStandUp);
	void TickLie(AHeroCharacter* Hero, APlayerController* PC, bool bTalk);
	void StartShow(int32 Thing, const FText& Title, const FText& Line, int32 Minutes, bool bSeated);
	void NextShowPart();
	void EndShow();
	void TickShow(AHeroCharacter* Hero, APlayerController* PC, bool bTalk);
	/** The staff of a counter say hello once a visit (town talk, "counter_hello"). */
	void Greet(int32 Thing);
	void DrawLabels() const;
	FVector ThingLocation(int32 Thing) const;

	TPimplPtr<LyudCity::FModel> Model;
	/** The worker's parse while it is under way. */
	TSharedPtr<FLyudCityLoad, ESPMode::ThreadSafe> Loading;

	/** The thing whose prompt shows (-1 none), and the one picked last (re-picked every PickEvery seconds). */
	int32 Prompted = -1;
	int32 Picked = -1;
	float PickClock = 0.f;
	/** Past the town box: nothing is picked until the hero comes back. */
	bool bOutside = true;
	/** "Not now" on the prompt line until then (world seconds). */
	double RefusedUntil = 0.0;
	/** The NPC talk: while it is engaged, E is its own. */
	TWeakObjectPtr<ANpcTalkDirector> TalkDirector;

	/** A thing of the plot the hero is in (-1 none), and the plot whose staff have said hello this visit. */
	int32 PlotThing = -1;
	FName GreetedPlot;

	/** The page shown: its thing, lines left to show this time ("E — next"), where the hero stood. */
	TSharedPtr<SLyudCityPage> Page;
	int32 PageThing = -1;
	int32 PageLeft = 0;
	FVector PageFrom = FVector::ZeroVector;
	FText PageTitle;
	/** The frame the page (or a show, or lying down) began: its own E must not end it. */
	uint64 PageFrame = 0;

	/** A fade around a jump of the clock: >0 minutes still to jump once the screen is black. */
	int32 FadeMinutes = 0;
	double FadeAt = 0.0;
	bool bFadingIn = false;

	/** Game seconds still to pass over the next frames, and how fast (game seconds a real second). */
	double JumpLeft = 0.0;
	double JumpRate = 0.0;
	/** The hero's moves are held while a few minutes pass (by this controller). */
	TWeakObjectPtr<APlayerController> MoveBlockedBy;

	/** The pooled anchor the hero sits on (one actor at the world origin; the seat's frame is given in world terms). */
	UPROPERTY(Transient) TObjectPtr<AActor> SeatAnchor;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> SeatRoot;
	int32 SeatedOn = -1;
	/** The screen a hero seated on SeatedOn may watch from there (-1 none). */
	int32 SeatScreen = -1;

	/** Lying on a bed: the view from the pillow (a pooled camera), the bed, when he gets up by himself (0: on a key). */
	UPROPERTY(Transient) TObjectPtr<ACameraActor> LieCamera;
	int32 LyingOn = -1;
	double LieUntil = 0.0;
	bool bLieAutoStand = false;
	bool bHeroHiddenForLie = false;
	FVector LieEye = FVector::ZeroVector;
	float LieYaw = 0.f;

	/** A screen show: its subtitles, the one shown (-1 none), when the next comes, the game minutes it lasts. */
	TArray<FText> ShowParts;
	int32 ShowAt = -1;
	double ShowNext = 0.0;
	int32 ShowMinutes = 0;
	int32 ShowThing = -1;
	FText ShowTitle;
	bool bShowSeated = false;

	bool bShowLabels = false;
	float LabelClock = 0.f;
};
