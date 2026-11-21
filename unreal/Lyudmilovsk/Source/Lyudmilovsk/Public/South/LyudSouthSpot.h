#pragma once

#include "CoreMinimal.h"
#include "North/LyudNorth.h"
#include "LyudSouthSpot.generated.h"

class AHeroCharacter;

/**
 * A spot of the Aton coast the hero can use (docs/south.md "Game side"): a seat of a bench, of the lookout's rotunda or
 * of the bird hide (Kind "seat": [E] sits him down on AHeroCharacter::SitOn's bench pose, E again, a step or Space gets
 * him up), or a coin viewer (Kind "view": [E] holds the binoculars' view up through ULyudGameScreen::HoldViewer, worn or
 * not, so birds can be told through it; E again or a step away lets go).
 *
 * Placed by unreal/scripts/import_south.py from the meshes' manifests ("seats", "viewers"): the actor stands in the
 * seat's frame (origin on the floor under the cushion's front edge, X the way the sitter faces; a viewer: where the
 * looker stands, X the way he looks). It draws nothing. Asleep (ULyudNorthSubsystem's ranges) it does not tick; near, it
 * polls E through ANpcTalkDirector::UsePressed (never consumed). Of several spots in reach the nearest has the prompt.
 *
 * Console: lyud.South.Sit 1|0 (the hero sits on the nearest seat from anywhere / gets up; logs a SOUTH line).
 */
UCLASS()
class LYUDMILOVSK_API ALyudSouthSpot : public ALyudNorthThing
{
	GENERATED_BODY()

public:
	ALyudSouthSpot();

	/** "seat" or "view". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "South") FName Kind = TEXT("seat");
	/** The cushion over the frame's floor, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "South") float SeatHeight = 45.f;
	/** How near the hero's feet must be (in plan), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "South") float Reach = 110.f;

	/** The hero sits here or looks through this viewer. */
	bool IsInUse() const { return bUsing; }
	/** Sits the hero down here (a viewer: holds its view up) / lets him go. False when it cannot be. */
	bool Use(AHeroCharacter* Hero);
	void Leave();

	virtual void SetAwake(bool bInAwake, bool bInNear) override;
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void SetPrompt(int32 Mode);
	bool IsSeat() const { return Kind != TEXT("view"); }

	TWeakObjectPtr<AHeroCharacter> User;
	bool bUsing = false;
	int32 Prompted = 0;
};
