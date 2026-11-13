#pragma once

// The F7 panel's tabs past the first (World: date, weather, trees): Teleport, Quests, Pocket, Birds, Actions (docs/dev_mode.md).

#include "CoreMinimal.h"

class APlantDevTool;
class SWidget;

namespace LyudDevPanel
{
	/** The row of tab buttons; it sets Tool->Tab. */
	TSharedRef<SWidget> TabBar(APlantDevTool* Tool);
	/** Every tab but the first, each shown while it is the chosen one. */
	TSharedRef<SWidget> Tabs(APlantDevTool* Tool);
}
