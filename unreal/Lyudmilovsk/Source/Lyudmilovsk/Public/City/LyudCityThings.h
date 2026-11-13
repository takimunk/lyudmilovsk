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
class SLyudCityBook;
class SLyudCityGame;
class SLyudCityPage;
class USceneComponent;
struct FLyudCityLoad;
namespace LyudCity { class FModel; struct FGameState; struct FPageTurn; }

/** A thing of the town's buildings as the townsfolk see it (ULyudCityThings::FindFreeThing, GetThingsOfPlot). */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FCityThingRef
{
	GENERATED_BODY()

	/** "<plot>/<thing id>" (P-CAFE/chair_3): what ClaimThing and ReleaseThing take. */
	UPROPERTY(BlueprintReadOnly, Category = "City") FString Id;
	/** seat, bed, shelf, board, counter, machine, play, sport, craft, screen, take, door. */
	UPROPERTY(BlueprintReadOnly, Category = "City") FName Kind;
	UPROPERTY(BlueprintReadOnly, Category = "City") FString Plot;
	/** The point the thing is looked at by (its middle; a seat's cushion), world cm. */
	UPROPERTY(BlueprintReadOnly, Category = "City") FVector Location = FVector::ZeroVector;
	/** The way it faces (a seat: the way its sitter looks), Unreal degrees. */
	UPROPERTY(BlueprintReadOnly, Category = "City") float Yaw = 0.f;
	/** A seat's or bed's surface over its own floor, cm (0 for other kinds). */
	UPROPERTY(BlueprintReadOnly, Category = "City") float SeatHeightCm = 0.f;
	/** Somebody holds it now (a townsperson's claim, or the hero sits or lies on it). */
	UPROPERTY(BlueprintReadOnly, Category = "City") bool bHeld = false;
};

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
 * Lines may hold only on a holiday (ULyudFestivalSubsystem) or in some weather (ULyudWeatherSubsystem::GetSky); a thing of a
 * shut plot may say something of its own (the chairs on the tables); acts, lines and things may give stamps of collections
 * (films seen, records heard, kiosks visited: a note on the HUD, "3 of 12"), saved with the rest.
 *
 * The townsfolk (docs/city_interiors.md "API for townsfolk"): FindFreeThing, ClaimThing / ReleaseThing (a weak holder, let go
 * when it dies; the hero can't sit on a held seat), GetThingsOfPlot.
 *
 * Mini-games (docs/city_interiors.md "Mini-games"): an act with a "game" (darts, a cue shot, free throws: aim; table tennis,
 * badminton: rally; chess, draughts, dominoes: a duel of three moves against a named regular) opens a panel (SLyudCityGame, made
 * once and kept) instead of its page; the rules and the dice are the model's, the keys are polled (E; 1/2/3 for a duel) and
 * never consumed; walking off, standing up or any screen ends it without a result. The result line, a stamp (the regular
 * beaten), a prize and the act's minutes come at the end. Clock lines show the game's time ({time}); a thing with "switch" turns
 * the room's lamps on and off for the hero's visit (ULyudWindowLights::SwitchLampsNear).
 *
 * Sitting and lying (docs/city_interiors.md "Sitting and lying"): a seat is a real pose (AHeroCharacter::SitOn, the floor
 * traced and checked against the data, a free spot found to stand up to, the view held in a cone round the seat's facing); a
 * bed is the view from the pillow (the hero has no lying pose): a slow settle toward the room's window, and on a "sleep" bed an
 * hour chooser (A/D, E) that sleeps the clock on to it. Never stuck: a seat whose anchor went, a load while seated, a page
 * or the chooser open when he gets up: all let go.
 *
 * The reading view (docs/city_interiors.md "The reading page"): an act with "read" opens a text of the data's "texts" in
 * SLyudCityBook (made once, kept collapsed): E/D/right next, A/left back, Esc/W/S/Space close; the page each text was left on,
 * the texts read to the end and where are saved; game minutes pass per page; the last page gives the text's stamp.
 *
 * Console: lyud.City.Things, lyud.City.Do <thing id>, lyud.City.Things.Show 0|1, lyud.City.Weekday <mon..sun>,
 * lyud.City.Collections [grant|clear <set|*>], lyud.City.Game <thing id> | press [1-3] | win | quit | regulars,
 * lyud.City.Sit [thing id], lyud.City.Lie [thing id], lyud.City.Sleep <hour>, lyud.City.Read <text id> | next | prev | close |
 * list | all | clear.
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

	// --- collections

	/** Logs every collection ("films: 3 of 12"); with bPage also shows them on the page (closed by E or walking off). */
	void ShowCollections(bool bPage);
	/** Developer mode: "grant" or "clear" a set ("*": all); false for an unknown verb or set. */
	bool EditCollections(const FString& Verb, const FString& Set);
	/** Is every collection complete (the developer toggle's light)? */
	bool AreCollectionsComplete() const;

	// --- mini-games

	/** Starts the game of a thing's act at once (console, developer mode: no hours, money or limit asked); false: no such
	 *  thing or its act has no game. */
	bool StartGameAt(const FString& ThingId);
	/** Developer mode: the game of the type ("aim", "rally", "duel") on the thing nearest the hero. */
	bool StartNearestGame(const FString& Type);
	/** A key of the game being played: E (Choice -1) or a duel's move 0..2 (console: lyud.City.Game press). */
	void PressGame(int32 Choice);
	/** Developer mode: the game being played is won at once. */
	void WinGame();
	/** Ends the game being played without a result. */
	void QuitGame();
	bool IsPlayingGame() const;
	/** Logs the regulars (name, style, games, where); with bPage also on the page. */
	void ShowRegulars(bool bPage);

	// --- sitting, lying, sleeping

	/** Seats the hero on a thing (bLie: lies on it; a "sleep" bed's act opens the hour chooser), wherever he is; empty Id: the
	 *  nearest free seat (bed) to him. Stands him up first when seated. False: no such thing, not a seat (bed), or held. */
	bool SitOnThing(const FString& Id, bool bLie);
	/** Lying on a bed: sleeps until Hour (0..23) at once (the chooser's E); false when not lying on a bed of ours. */
	bool SleepUntil(int32 Hour);
	bool IsSittingOnThing() const { return SeatedOn >= 0; }
	bool IsLyingOnThing() const { return LyingOn >= 0; }
	bool IsChoosingSleep() const { return SleepHour >= 0; }

	// --- the reading view

	/** Opens a text of the data by id wherever the hero is (developer mode, console); false: no such text. */
	bool OpenText(const FString& TextId);
	/** Runs the nearest thing whose act reads (developer mode). */
	bool OpenNearestText();
	/** Turns the open text's page (+1 next, -1 back; past the last page: closed). */
	void TurnBook(int32 Delta);
	void CloseBook();
	bool IsReading() const { return BookText >= 0; }
	/** Logs the texts read (with where); with bPage also shows them on the page. */
	void ShowReadList(bool bPage);
	/** Developer mode: every text read (their stamps given) or none (and the pages left on forgotten). */
	void SetAllRead(bool bRead);
	bool AreAllRead() const;

	// --- API for townsfolk (docs/city_interiors.md): the people's life asks for seats and things to use

	/** The nearest thing of Kind (None: any) within RadiusCm of Near, on about Near's floor (2.5 m), that nobody holds and the
	 *  hero is not on; false none. Reads the model's 4 m grid round Near only. */
	bool FindFreeThing(FName Kind, const FVector& Near, float RadiusCm, FCityThingRef& Out) const;
	/** Who takes the thing (a townsperson sits down): true when it was free or already theirs. Never one the hero sits or lies
	 *  on. The hold goes when Who is destroyed (checked a few times a second) or calls ReleaseThing. */
	bool ClaimThing(const FString& Id, const UObject* Who);
	/** Who lets the thing go (nothing when somebody else holds it). */
	void ReleaseThing(const FString& Id, const UObject* Who);
	/** The things of a plot of Kind (None: all), appended to Out. A table built at load: no pass over the town. */
	void GetThingsOfPlot(const FString& PlotId, FName Kind, TArray<FCityThingRef>& Out) const;

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
	/** A note on the HUD for every stamp got for the first time ("Films at «Рассвет»: 3 of 12"). */
	void AnnounceStamps(const TArray<FString>& Stamps);
	void FillRef(int32 Thing, FCityThingRef& Out) const;
	/** The holds of townsfolk whose holder is gone are let go (a few times a second). */
	void DropDeadHolds();
	/** A line's words: the key's text with {time} filled in (the wall clocks). */
	FText LineText(const FString& Key) const;
	/** A thing with "switch": the lamps of its room on or off for the hero's visit (all back when he leaves the plot). */
	void FlipSwitch(int32 Thing);
	/** A short word on the prompt line for a moment (the switch's "lights on"), like a refusal but not one. */
	void Note(const FText& What);

	// --- mini-games
	void BeginGame(int32 Thing, int32 Minutes, bool bSeated);
	void TickGame(AHeroCharacter* Hero, APlayerController* PC, bool bTalk, float DeltaTime);
	/** The panel's texts (only when a beat, a round or the score changed) and its bar (every frame). */
	void UpdateGamePanel(bool bTexts);
	/** The game is over: the result page, the stamp, the prize, the minutes. */
	void FinishGame();
	/** The panel away and the state cleared (bFinished false: walked off, no result). */
	void EndGame(bool bFinished);

	// --- sitting and lying
	/** Where the hero stands up to from a seat, in its frame (cm): in front, beside, behind, the first spot his capsule fits on
	 *  about the seat's floor; in front when none does. */
	FVector StandOffsetFor(const FTransform& Frame, double FloorZ, const AHeroCharacter* Hero, bool bLie) const;
	/** The seated hero's view kept within Half degrees of Yaw (APlayerCameraManager's yaw limits), and let go. */
	void SetLookCone(APlayerController* PC, float Yaw, float Half);
	void ClearLookCone();
	/** He is off the seat but the seat never let him go (its anchor went): walking and colliding again. */
	void RecoverFromSeat(AHeroCharacter* Hero);
	/** The seat's own keys while a text or the chooser is up (moves and Space are the view's; they stand him up otherwise). */
	void HookSeatKeys(AHeroCharacter* Hero);
	void UnhookSeatKeys();
	/** The nearest seat (bLie: bed) to the hero that nobody holds; -1 none. Developer mode: one pass over the things. */
	int32 NearestSeat(bool bLie) const;
	/** Everything the hero was in the middle of (a text, the chooser, a game, a show, lying, a seat of ours) let go: a load. */
	void ResetHeroState();

	// --- sleep: the hour chooser on the page while he lies on a "sleep" bed
	void BeginSleepChoice(int32 Thing, int32 Act, const FString& LineKey, const FText& Title);
	void ShowSleepChoice();
	void TickSleepChoice(AHeroCharacter* Hero, APlayerController* PC, bool bTalk);
	/** bSleep: the fade and the clock on to the hour, then up; else he gets up at once. */
	void EndSleepChoice(bool bSleep);

	// --- the reading view
	void OpenBook(int32 Text, int32 Thing, bool bSeated);
	void ShowBookPage();
	/** After a turn (or the opening): the stamps, and "read to the end" with the quest event "read"/<text id>. */
	void BookTurned(const LyudCity::FPageTurn& Turn);
	void TickBook(AHeroCharacter* Hero, APlayerController* PC, bool bTalk);
	/** "Read — 2 of 6" and per place the titles read there (Read list); empty when there are no texts. */
	FString ReadListBody() const;
	/** A plot's name for the player (its city.plot.<id>.name, else its teleport name, else the id). */
	FString PlotName(const FString& Plot) const;

	TPimplPtr<LyudCity::FModel> Model;
	/** The worker's parse while it is under way. */
	TSharedPtr<FLyudCityLoad, ESPMode::ThreadSafe> Loading;

	/** The thing whose prompt shows (-1 none) and the act it offers (its closed act while its plot is shut), and the one picked
	 *  last (re-picked every PickEvery seconds). */
	int32 Prompted = -1;
	int32 PromptedAct = -1;
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

	/** The townsfolk's holds: thing -> the holder (weak) and its key in the model. */
	struct FHold
	{
		TWeakObjectPtr<const UObject> Who;
		uint64 Key = 0;
	};
	TMap<int32, FHold> Holds;

	/** The game being played (Def < 0: none; made once and reused), its panel (made once, kept collapsed between games), the act's
	 *  minutes that pass at its end, where the hero stood, whether he sat down for it, the frame it began (its own E must not
	 *  count), the beat and round the panel shows. */
	TPimplPtr<LyudCity::FGameState> Playing;
	TSharedPtr<SLyudCityGame> GamePanel;
	bool bGamePanelAdded = false;
	int32 GameMinutes = 0;
	FVector GameFrom = FVector::ZeroVector;
	bool bGameSeated = false;
	uint64 GameFrame = 0;
	int32 GameBeatShown = -1;
	FText GameTitle;
	FText GameOpponent;
	/** Plots whose lamps the hero switched (cleared when he walks into another plot). */
	FName SwitchedPlot;

	/** The seated view's yaw limits are ours (set by this controller) until let go. */
	TWeakObjectPtr<APlayerController> ConeBy;
	/** The seat's keys are ours (HookSeatKeys) on this hero. They never change the hero's hooks from inside a hook: they ask,
	 *  and the tick stands him up (bStandAsked) or shuts the text (bCloseBookAsked). */
	TWeakObjectPtr<AHeroCharacter> HookedHero;
	bool bStandAsked = false;
	bool bCloseBookAsked = false;

	/** The sleep chooser: the hour chosen (-1: no chooser), the bed, the line shown on waking, the act's title; bSleeping: E
	 *  was pressed and the fade, the clock and getting up are under way. */
	int32 SleepHour = -1;
	int32 SleepThing = -1;
	FString SleepLine;
	FText SleepTitle;
	bool bSleeping = false;

	/** The reading view (made once, collapsed while no text is open): the text (-1 none), its thing (-1: the console), the
	 *  pages now (line keys), the page shown, where the hero stood, the frame it opened (its own E must not turn it), the plot it
	 *  is read in, whether he sits for it, whether the moves are held for it (by this controller). */
	TSharedPtr<SLyudCityBook> Book;
	bool bBookAdded = false;
	int32 BookText = -1;
	int32 BookThing = -1;
	TArray<FString> BookPages;
	int32 BookPage = 0;
	FVector BookFrom = FVector::ZeroVector;
	uint64 BookFrame = 0;
	FString BookPlot;
	bool bBookSeated = false;
	TWeakObjectPtr<APlayerController> BookBlockedBy;
};
