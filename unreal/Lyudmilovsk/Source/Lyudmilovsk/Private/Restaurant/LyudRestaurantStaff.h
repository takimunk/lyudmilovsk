#pragma once

#include "CoreMinimal.h"

class UObject;

/**
 * The restaurant's way to the town life's staff (docs/restaurant.md "The waiter"; the town life's docs/town_life.md, branch town-life):
 * UPeopleLifeSubsystem::StaffOnShift(PlaceId, Role), SetPresentedExternally(Id, bExternal) and GetDisplayName(Id). They are called by
 * name through reflection (all three are UFUNCTIONs there), so the restaurant compiles and runs without them: with no such function
 * there are no staff (the restaurant opens by its hours and draws its stand-in waiter) and nobody's body is taken over.
 */
namespace LyudRestaurantStaff
{
	/** Who works at PlaceId in Role now (person ids of the people's life); empty without the town life. */
	TArray<int32> OnShift(const UObject* WorldContext, FName PlaceId, FName Role);
	/** The restaurant draws Id's body now (the life hides its own); false without the town life. */
	bool SetPresentedExternally(const UObject* WorldContext, int32 Id, bool bExternal);
	/** Their name in the game's language; empty without the town life. */
	FText DisplayName(const UObject* WorldContext, int32 Id);
	/** The town life's staff calls exist in this build. */
	bool IsAvailable(const UObject* WorldContext);
}
