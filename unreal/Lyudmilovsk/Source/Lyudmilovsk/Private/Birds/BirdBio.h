// What a species can do in the air, from its biology (plain C++, shared with the headless tests): how long it
// can keep beating its wings, how fast it dives, how sharply it turns, whether it swims and how it gets off the
// water. A peregrine stoops at 90 m/s and flies all day; a blue tit manages half a minute of flapping before it
// must rest. Its flying qualities: the fastest it can fly level at full power (so the strongest headwind it can
// still make way against), how fast it can climb by flapping (its strength), the mean wind at which it stops
// flying in the open and keeps to shelter, and how flat it glides. A blue tit sprints at 11 m/s, climbs 2.5 m/s
// and hides from a 7 m/s wind; a herring gull hangs in a 20 m/s gale; a white-tailed eagle can barely climb by
// flapping (0.9 m/s) and lives on rising air. Sources: Alerstam et al. 2007 (cruising airspeeds of 138 species),
// Bruderer & Boldt 2001 (radar airspeeds), Pennycuick 2008 "Modelling the flying bird" (power curves, climb, glide
// ratios), Henningsson et al. 2010 (swift sprints), Tucker 1998 (falcon and eagle stoops), field guides.
#pragma once

namespace BirdBio
{
	struct FBio
	{
		const char* Id;
		double StaminaS;    // seconds of steady flapping from rested to spent
		double DiveMs;      // the fastest dive with the wings in, m/s
		double Agility;     // turn rate and oar kick, x the flight style's
		double CruiseMs;    // steady flight, m/s; 0: species.json's
		bool Swims;         // settles on water (and takes off from it)
		double RunUpS;      // seconds of pattering over water before it lifts off (swans)
		double SprintMs;    // the fastest level flight at full power, m/s: a headwind stronger than this blows it back
		double ClimbMs;     // the steepest climb by flapping, m/s up (the strength of the flight muscles)
		double WindLimitMs; // mean wind (10 m) above which it stays in shelter and flies only short hops, low
		double GlideRatio;  // metres forward per metre down at the best glide; 0: the flight style's
	};

	/** The bio of a species id; an unknown id gets a middling bird. */
	const FBio& Get(const char* Id);
}
