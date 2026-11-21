#include "LyudMotion.h"

#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarAnimSmooth(TEXT("lyud.Anim.Smooth"), 1,
	TEXT("1: people, animals and birds follow their simulations smoothly (rounded corners, eased starts, stops and turns, steps ")
	TEXT("as long as the speed asks, clips that melt into each other). 0: as before, set straight onto the simulation (comparisons)."));

bool LyudMotion::Smooth()
{
	return CVarAnimSmooth.GetValueOnAnyThread() != 0;
}
