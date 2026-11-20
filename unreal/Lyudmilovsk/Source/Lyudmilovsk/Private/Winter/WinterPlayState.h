#pragma once

#include "WinterPlayModel.h"

#include <vector>

/** The model's objects (Private/Winter/WinterPlayModel.h), shared by LyudWinterPlay.cpp and LyudWinterPlayBuild.cpp
 *  (a header of its own: the Mac's adaptive build compiles a changed file outside the unity blob). */
struct FWinterPlayState
{
	LyudWinterPlay::FParams P;
	LyudWinterPlay::FIceState IceState;
	LyudWinterPlay::FIce Ice;
	LyudWinterPlay::FSkater Skater;
	LyudWinterPlay::FRoom Room;
	LyudWinterPlay::FBody Body;
	LyudWinterPlay::FFight Fight;
	LyudWinterPlay::FSaved Save;
	std::vector<LyudWinterPlay::FBall> Balls;
	std::vector<LyudWinterPlay::FCrack> Cracks;
	LyudWinterPlay::FRng Rng = LyudWinterPlay::FRng(4242u);
	bool bWasSwimming = false;
	double LastFactsHours = -1.0;
};
