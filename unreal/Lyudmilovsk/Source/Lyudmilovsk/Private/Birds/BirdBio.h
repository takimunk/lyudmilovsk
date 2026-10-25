// What a species can do in the air, from its biology (plain C++, shared with the headless tests): how long it
// can keep beating its wings, how fast it dives, how sharply it turns, whether it swims and how it gets off the
// water. A peregrine stoops at 90 m/s and flies all day; a blue tit manages half a minute of flapping before it
// must rest. Sources: Alerstam et al. 2007 (flight speeds), Pennycuick's flight-range estimates, field guides.
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
	};

	/** The bio of a species id; an unknown id gets a middling bird. */
	const FBio& Get(const char* Id);
}
