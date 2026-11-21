#pragma once

#include "CoreMinimal.h"
#include "North/LyudNorth.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudSouthUses.generated.h"

class AHeroCharacter;
class SLyudCityPage;

/**
 * A thing to do on the Aton coast (docs/south.md "Things to do", backlog item 39), spawned at runtime by ULyudSouthUses from
 * Content/South/south_uses.json (tools/south_uses.py):
 *  - "drink": the spring's trough or the alley's drinking fountain. [E] he bends down and drinks from his hands (a crouch for
 *    a moment, a line of what it tastes like); at the spring with an empty bottle in the pocket [E] fills it instead
 *    (empty_bottle -> water). Quests hear "use" with the spot's water ("spring", "fountain") and "get" water.
 *  - "read": a notice board, the stele, the belvedere's view. [E] shows its pages one at a time on the city's page widget
 *    (SLyudCityPage: the title, the lines, "E — next" / "E — close"); a step out of reach shuts it. Quests hear "read" with
 *    the spot's id (read_trail, read_reeds, read_stele, read_view).
 * It draws nothing. Asleep (ULyudNorthSubsystem's ranges) it does not tick; near, it polls E through
 * ANpcTalkDirector::UsePressed (never consumed). Of several spots in reach the nearest has the prompt.
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ALyudSouthUse : public ALyudNorthThing
{
	GENERATED_BODY()

public:
	ALyudSouthUse();

	/** "drink" or "read". */
	FName Kind;
	FString SpotId;
	/** drink: "spring" / "fountain"; it fills a bottle when bFill. */
	FString Water;
	bool bFill = false;
	/** read: the title's key and each page's lines' keys. */
	FString TitleKey;
	TArray<TArray<FString>> Pages;
	/** How near the hero's feet must be (in plan), cm. */
	float Reach = 120.f;

	virtual void SetAwake(bool bInAwake, bool bInNear) override;
	virtual void Tick(float DeltaTime) override;

	/** Does it now (the console's lyud.South.Use): drinks / fills, or opens or turns the page. */
	void Use(AHeroCharacter* Hero);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void SetPrompt(const TCHAR* Key);
	void Drink(AHeroCharacter* Hero);
	void ShowPage(int32 Index);
	void ClosePage();

	TSharedPtr<SLyudCityPage> Page;
	int32 PageIndex = -1;
	const TCHAR* Prompted = nullptr;
	/** The crouch of a drink: when he straightens up again (world seconds; 0 none). */
	double StandAt = 0.0;
	TWeakObjectPtr<AHeroCharacter> Drinker;
	int32 Sip = 0;
};

/**
 * Spawns the Aton coast's things to do (ALyudSouthUse) and the boats' seats (ALyudSouthSpot) from south_uses.json when the
 * level starts, where the coast is placed (any ALyudSouthSpot from import_south.py in the level): nothing to import in the
 * editor. Console: lyud.South.Use (uses the nearest spot of the file from anywhere: teleports there first), lyud.South.Uses
 * (lists them).
 */
UCLASS()
class LYUDMILOVSK_API ULyudSouthUses : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudSouthUses* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type T) const override { return T == EWorldType::Game || T == EWorldType::PIE; }

	const TArray<TWeakObjectPtr<ALyudSouthUse>>& GetUses() const { return Uses; }
	int32 NumSeats() const { return Seats; }

private:
	void Spawn(UWorld& World);

	TArray<TWeakObjectPtr<ALyudSouthUse>> Uses;
	int32 Seats = 0;
};
