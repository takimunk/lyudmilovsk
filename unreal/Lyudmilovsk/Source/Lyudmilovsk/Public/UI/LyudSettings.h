#pragma once

#include "CoreMinimal.h"

/**
 * The player's settings (the menu's Settings page): the language, the HUD, the sound volumes. Kept per machine, not per save,
 * in GameUserSettings.ini ([Lyudmilovsk.Settings]); applied once when the game starts. Every setting is also a console variable
 * (lyud.Language, lyud.Hud), so -ExecCmds and the console still override them for a run.
 */
namespace LyudSettings
{
	/** Applies what is stored (once per run; later calls do nothing). */
	void ApplySaved(UWorld* World);

	void SetLanguage(const FString& Lang);
	bool HudShown();
	void SetHudShown(bool bShow);

	/** The sound volumes 0..1 (ULyudSoundscapeSubsystem::SetVolumes), stored and handed to every new world's soundscape. */
	enum class EVolume : uint8 { Master, Ambience, Music, Effects, Count };
	float Volume(EVolume Which);
	void SetVolume(UWorld* World, EVolume Which, float Value);
	/** Gives World's soundscape the stored volumes (every level start). */
	void ApplyVolumes(UWorld* World);
}
