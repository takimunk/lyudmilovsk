// The fishing places (Content/Fishing/fishing.json, placed by Content/City/city_plan.json): the spots the journal names, the
// rowing boat's mooring at the pier, the kitchens that buy the catch. Read once. docs/fishing.md.
#pragma once

#include "CoreMinimal.h"

namespace LyudFishPlaces
{
	struct FSpot
	{
		FName Id;
		/** A stretch: within Radius (cm) of the segment A..B (world, flat). */
		FVector2D A = FVector2D::ZeroVector, B = FVector2D::ZeroVector;
		float Radius = 0.f;
	};

	struct FBoatDef
	{
		bool bValid = false;
		/** Where it lies moored (world, flat; the sea's height under it) and which way it points, deg; where the hero steps out. */
		FVector2D Mooring = FVector2D::ZeroVector;
		float Yaw = 0.f;
		FVector2D Landing = FVector2D::ZeroVector;
		int32 Fee = 30;
		/** How far from the mooring it may be rowed, cm; the water it needs under it, cm. */
		float Range = 60000.f;
		float MinDepth = 70.f;
	};

	struct FKitchen
	{
		FName Id;
		FVector At = FVector::ZeroVector;
		float Radius = 300.f;
		int32 Open = 11, Close = 23;
		int32 PerDay = 8;
	};

	struct FPlaces
	{
		TArray<FSpot> Spots;
		FBoatDef Boat;
		TArray<FKitchen> Kitchens;
	};

	const FPlaces& Get();
	/** The spot that holds a point (none: the open shore). */
	FName SpotAt(const FVector& Where);
}
