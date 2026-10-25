#pragma once

#include "CoreMinimal.h"

class USoundWaveProcedural;

/**
 * The player bird's own sound, made as it plays (no recordings): the air rushing past (louder and brighter
 * with speed, gusty), and every downstroke of the wings, pitched and sized by the bird (a sparrow's whirr,
 * a pigeon's clatter and take-off clap, the mute swan's throbbing wing song). Mono PCM queued into a
 * USoundWaveProcedural a little ahead of time; the pawn plays it with an audio component.
 */
class FBirdSound
{
public:
	/** MassKg sets the pitch and weight of the beats; bClap: pigeons clap at take-off; bSinging: the swan's wings sing. */
	USoundWaveProcedural* Init(UObject* Outer, double MassKg, bool bClap, bool bSinging);

	/** Once per frame: Airspeed m/s through the air, Gust 0..2, Volume 0..1 (the camera flights fade it). */
	void Tick(float Dt, float Airspeed, float Gust, float Volume);
	/** A downstroke starts; Strength 0..1 (a spent bird beats weakly), bTakeoff for the clap. */
	void Beat(float Strength, bool bTakeoff);

private:
	void Generate(int32 Samples);

	USoundWaveProcedural* Wave = nullptr;
	static constexpr int32 Rate = 24000;
	double Mass = 0.1;
	bool bClaps = false, bSings = false;
	// state
	float Speed = 0.f, GustNow = 1.f, Vol = 0.f;
	float Lp1 = 0.f, Lp2 = 0.f, Rumble = 0.f, Bp1 = 0.f, Bp2 = 0.f;
	float BeatT = -1.f, BeatLen = 0.1f, BeatAmp = 0.f, ClapT = -1.f, SongPhase = 0.f;
	uint32 Seed = 12345u;
	double Clock = 0.0;
};
