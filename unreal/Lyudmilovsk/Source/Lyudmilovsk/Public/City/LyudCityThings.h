#pragma once

#include "CoreMinimal.h"
#include "LyudSave.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "Tickable.h"
#include "LyudCityThings.generated.h"

class AActor;
class AHeroCharacter;
class APlayerController;
class SLyudCityPage;
class USceneComponent;
namespace LyudCity { class FModel; }

/**
 * Things to do inside the town's buildings (docs/city_interiors.md): sit on a chair, read at a shelf, look at a notice board,
 * buy at a counter, rest on a bed, play the piano... Data: Content/City/interiors.json (tools/city/interiors.py), the rules in
 * the engine-free LyudCity::FModel (Private/City/CityThingsModel.h, checked by tools/citysim/run.sh).
 *
 * Nothing is spawned per thing (the furniture is part of the building meshes). A few times a second, and only while the hero
 * is inside the box round all things, the model picks the thing in view from a grid; a trace checks no wall is between. The
 * prompt goes through ULyudInventoryUI::SetPrompt (rebuilt only when the thing changes), E (polled, never consumed) runs the
 * act: a page of text (SLyudCityPage, E turns it, walking away shuts it), game minutes (with a fade when the act says so),
 * money, items, a shop (ULyudInventoryUI::OpenShop with the act's stock, only while the staff is on shift), sitting
 * (AHeroCharacter::SitOn on one pooled anchor), a one-shot sound, quest events ("do"/act or the act's own, "visit"/plot the
 * first use in a plot each day). Pages read, per-day counts and uses are kept in the save ("CityThings").
 *
 * Console: lyud.City.Things (count and the nearest five), lyud.City.Do <thing id> (runs it from anywhere),
 * lyud.City.Things.Show 0|1 (labels of things within 15 m).
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
	virtual bool IsTickable() const override { return NumThings() > 0; }

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

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	/** The model's state as a flat map ("CityThings"): counts, pages, uses, plots visited. */
	UPROPERTY(SaveGame) TMap<FString, FString> Saved;

private:
	void Load();
	void StartOver();
	/** Re-picks the thing in view (a few times a second). */
	int32 PickInView(const AHeroCharacter* Hero, const APlayerController* PC) const;
	void SetPrompted(int32 Thing);
	void Run(int32 Thing, bool bFromConsole);
	void ShowPage(const FText& Title, const FText& Body, bool bMore);
	void ClosePage();
	void Refuse(const FText& Why);
	void Sit(int32 Thing, AHeroCharacter* Hero, bool bLie);
	void JumpMinutes(int32 Minutes);
	void DrawLabels() const;
	FVector ThingLocation(int32 Thing) const;

	TPimplPtr<LyudCity::FModel> Model;

	/** The thing whose prompt shows (-1 none), and the one picked last (re-picked every PickEvery seconds). */
	int32 Prompted = -1;
	int32 Picked = -1;
	float PickClock = 0.f;
	/** Past the town box: nothing is picked until the hero comes back. */
	bool bOutside = true;
	/** "Not now" on the prompt line until then (world seconds). */
	double RefusedUntil = 0.0;

	/** The page shown: its thing, lines left to show this time ("E — next"), where the hero stood. */
	TSharedPtr<SLyudCityPage> Page;
	int32 PageThing = -1;
	int32 PageLeft = 0;
	FVector PageFrom = FVector::ZeroVector;
	FText PageTitle;
	/** The frame the page opened (its own E must not turn it). */
	uint64 PageFrame = 0;

	/** A fade around a jump of the clock: >0 minutes still to jump once the screen is black. */
	int32 FadeMinutes = 0;
	double FadeAt = 0.0;
	bool bFadingIn = false;

	/** The pooled anchor the hero sits on (one actor at the world origin; the seat's frame is given in world terms). */
	UPROPERTY(Transient) TObjectPtr<AActor> SeatAnchor;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> SeatRoot;
	int32 SeatedOn = -1;

	bool bShowLabels = false;
	float LabelClock = 0.f;
};
