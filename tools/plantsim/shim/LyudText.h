#pragma once
#include "CoreMinimal.h"
// tools/plantsim: the string tables are not loaded here; a key reads as itself
namespace LyudText
{
	inline FText Get(const FString& Key) { return FText{ Key }; }
}
