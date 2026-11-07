#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudGraphics.generated.h"

/**
 * The graphics settings (the menu's Graphics page, docs/graphics.md): a preset (Auto, Low, Medium, High, Epic, Cinematic)
 * or each engine scalability group on its own, the render resolution, the frame-rate limit, vsync, the window, and the
 * power saving. Kept per machine in GameUserSettings.ini ([Lyudmilovsk.Graphics]); applied when the first game world of
 * a run begins (in the editor at every Play, and the editor's own settings are put back when Play stops).
 *
 * What each level does is Config/DefaultScalability.ini: the engine's groups plus the game's own lyud.* knobs (forests,
 * live plants, ...), so a level means the same thing to the menu, to `scalability N` in the console and to the benchmark.
 *
 * Auto (the default of a new install) picks the level from the machine: Apple silicon by its tier (Max and Ultra Epic,
 * Pro High and Epic from the M4 Pro, the plain chip Medium and High from the M5), other GPUs by the engine's quick hardware
 * benchmark, capped by the memory. The render resolution's Auto renders at the level's share of a 1080p window (Low 50%,
 * Medium 58%, High 67%, Epic 75%, Cinematic 100%) and lower in bigger windows (x sqrt(1080 / height): 4K Epic 53%), where
 * TSR upscales as well from fewer pixels; TSR's history shrinks the same way (Epic's 200% would be 4x a Retina window).
 * In the editor's Play the game's settings apply too and the editor's come back when Play stops. Power saving holds the
 * frame rate down while the game is in the background (lyud.Gfx.BackgroundFps) or under the menu (lyud.Gfx.MenuFps).
 * Offscreen, unattended and fixed-step runs keep their own frame rate and resolution; the benchmark gets the resolution
 * a player would.
 */
namespace LyudGraphics
{
	/** The quality groups of the page, each one engine scalability group (sg.<Group>Quality, levels 0..4). */
	enum class EGroup : uint8 { ViewDistance, Shadow, GlobalIllumination, Reflection, Foliage, Effects, PostProcess, Texture, Shading, AntiAliasing, Count };
	constexpr int32 NumGroups = static_cast<int32>(EGroup::Count);
	/** Low, Medium, High, Epic, Cinematic. */
	constexpr int32 NumLevels = 5;

	/** The group's name in strings and commands: "view_distance", "shadow", ... (ui.gfx.group.<name>). */
	const TCHAR* GroupName(EGroup Group);

	/** Applies what is stored (the first game world of a run, or of a Play in the editor). */
	void ApplySaved(UWorld* World);

	/** The preset: 0..4 when every group is on that level, -1 when they differ (custom). */
	int32 Preset();
	/** The levels are the machine's own pick (Auto), re-picked when the GPU changes. */
	bool IsAuto();
	void SetPreset(int32 Level);
	void SetAuto();
	int32 GroupLevel(EGroup Group);
	void SetGroupLevel(EGroup Group, int32 Level);

	/** The level Auto picks on this machine; OutWhy: the GPU and the memory it went by. */
	int32 DetectLevel(FString* OutGpu = nullptr, int32* OutMemoryGB = nullptr);

	/** Render resolution, percent of the window: 0 Auto. */
	int32 Resolution();
	void SetResolution(int32 Percent);
	/** The percent the game renders at now (Auto resolved for the window). */
	float ScreenPercentage();
	/** The game window's size in pixels (what the render is upscaled to). */
	FIntPoint OutputSize();
	/** The GPU and the memory Auto goes by, for the page. */
	FString MachineGpu();
	int32 MachineMemoryGB();

	/** The frame-rate limit, frames a second; 0 none. */
	int32 FrameLimit();
	void SetFrameLimit(int32 Fps);
	bool VSync();
	void SetVSync(bool bOn);
	/** The game's own window (not the editor's Play): fullscreen (borderless) or a window. */
	bool CanSetWindowMode();
	bool Fullscreen();
	void SetFullscreen(bool bOn);
	/** Fewer frames while the game is in the background or under the menu. */
	bool PowerSaving();
	void SetPowerSaving(bool bOn);

	/** The game menu is open over the paused world (drawn at lyud.Gfx.MenuFps with power saving on). */
	void SetMenuOpen(bool bOpen);

	/** The state in one line (lyud.Gfx, tests). */
	FString Describe();
}

class UInstancedStaticMeshComponent;

/**
 * Keeps the frame rate, the render resolution and TSR's history where the graphics settings want them while the window,
 * the menu and the focus change, and resends the instanced meshes when the view distance changes (a few cvar reads a
 * frame; it sets anything only when a value changes).
 */
UCLASS()
class LYUDMILOVSK_API ULyudGraphicsSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickable() const override { return bStarted; }

private:
	/** Queues the world's instanced meshes to go to the renderer again (their distances take r.ViewDistanceScale in only then). */
	void RefreshInstances();
	/** Resends the next queued instanced meshes, lyud.Gfx.RefreshInstances.PerFrame instances a frame. */
	void ResendQueued();

	bool bStarted = false;
	/** r.ViewDistanceScale as the world's instanced meshes last took it in; RefreshCountdown frames until they take the new one. */
	float SeenViewDistanceScale = -1.f;
	int32 RefreshCountdown = 0;
	/** The instanced meshes still to resend, from RefreshNext on; RefreshSent of them sent so far. */
	TArray<TWeakObjectPtr<UInstancedStaticMeshComponent>> RefreshQueue;
	int32 RefreshNext = 0;
	int32 RefreshSent = 0;
};
