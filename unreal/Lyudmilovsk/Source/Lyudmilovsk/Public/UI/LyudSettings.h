#pragma once

#include "CoreMinimal.h"

/**
 * The player's settings (the menu's Settings page): the language, the HUD, the sound volumes, how far off birds are drawn. Kept per machine, not per save,
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

	/** How far off wild birds are still drawn (lyud.Birds.DrawDistance): 0 near .. 3 very far; far birds are cheap silhouettes. */
	constexpr int32 NumBirdRanges = 4;
	int32 BirdRange();
	void SetBirdRange(int32 Range);

	/** The sound volumes 0..1 (ULyudSoundscapeSubsystem::SetVolumes), stored and handed to every new world's soundscape. */
	enum class EVolume : uint8 { Master, Ambience, Music, Effects, Count };
	float Volume(EVolume Which);
	void SetVolume(UWorld* World, EVolume Which, float Value);
	/** Gives World's soundscape the stored volumes (every level start). */
	void ApplyVolumes(UWorld* World);
}
