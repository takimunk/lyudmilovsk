#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * The air a flying bird is in: a steady sea wind with gusts (lyud.Bird.WindSpeed / lyud.Bird.WindFrom,
 * from the west-south-west by default), the wind rising where it meets rising ground (the sea cliff,
 * the upland's slopes, a roof) and sinking behind it, and thermals over sunlit land by day. Ground
 * heights come from traces against the world's static geometry, a few per second.
 */
class FBirdAir
{
public:
	struct FSample
	{
		FVector Wind = FVector::ZeroVector;   // m/s, horizontal
		float Updraft = 0.f;                  // m/s, rising air (ridge + thermal)
		float Ridge = 0.f, Thermal = 0.f;
		float GroundZ = 0.f;                  // cm, under the bird
	};

	void Init(UWorld* InWorld);
	/** At PosCm (world, cm); Dt advances the gusts and the trace cache. */
	FSample Sample(const FVector& PosCm, float Dt);

	/** The highest static surface under (X, Y), or the sea, cm. */
	float GroundZ(const FVector2D& XY) const;
	/** The highest static surface under (X, Y) even below the sea (the seabed), cm. */
	float SolidZ(const FVector2D& XY) const;
	/** The playable box (the landscape's extent), cm. */
	const FBox& Bounds() const { return WorldBounds; }

	/** The sea's datum, high water, cm (tools/world_map.py SEA_Z = -43 m); the water itself is WaterZ(). */
	static constexpr float SeaZ = -4300.f;
	/** The sea's still water now (with the tide; ALyudSea::SeaLevel), cm. */
	float WaterZ() const;

private:
	TWeakObjectPtr<UWorld> World;
	FBox WorldBounds = FBox(FVector(-200000, -200000, -10000), FVector(200000, 200000, 40000));
	double Clock = 0.0;
	float CacheAge = 1e9f;
	FVector CachePos = FVector::ZeroVector;
	float CacheGround = 0.f, CacheTop = 0.f, CacheSlope = 0.f;
};
