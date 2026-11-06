#include "UI/LyudGraphics.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "RHIGlobals.h"
#include "Scalability.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"

static TAutoConsoleVariable<int32> CVarGfxMenuFps(
	TEXT("lyud.Gfx.MenuFps"), 30,
	TEXT("Frame-rate cap while the game menu is open over the paused world, with power saving on (0: none)."));

static TAutoConsoleVariable<int32> CVarGfxBackgroundFps(
	TEXT("lyud.Gfx.BackgroundFps"), 10,
	TEXT("Frame-rate cap while the game's window is in the background or minimized, with power saving on (0: none)."));

static TAutoConsoleVariable<int32> CVarGfxGovernor(
	TEXT("lyud.Gfx.Governor"), -1,
	TEXT("Menu and background frame-rate caps: 1 on, 0 off, -1 only in interactive runs (off with -RenderOffscreen, -unattended, -LyudBench)."));

static TAutoConsoleVariable<float> CVarGfxAutoRefHeight(
	TEXT("lyud.Gfx.AutoResolution.RefHeight"), 1080.f,
	TEXT("Auto render resolution: windows taller than this many pixels render at the level's share x sqrt(RefHeight / height)."));

static TAutoConsoleVariable<int32> CVarGfxAutoTSRHistory(
	TEXT("lyud.Gfx.AutoTSRHistory"), 1,
	TEXT("1: in windows taller than lyud.Gfx.AutoResolution.RefHeight TSR's history (r.TSR.History.ScreenPercentage, 200% at Epic and Cinematic) is the level's x sqrt(RefHeight / height), at least 100%, so a Retina window does not keep four times its own pixels. 0: the level's own."));

static TAutoConsoleVariable<int32> CVarGfxRefreshInstances(
	TEXT("lyud.Gfx.RefreshInstances"), 1,
	TEXT("1: when r.ViewDistanceScale changes (the View distance level) the world's instanced meshes go to the renderer again, since the engine bakes the scale into their draw and wind distances only then."));

// a named namespace: unity builds put neighbouring files (LyudSettings.cpp has its own Section, bApplied, ...) in one unit
namespace LyudGraphics::Local
{
	const TCHAR* Section = TEXT("Lyudmilovsk.Graphics");

	const TCHAR* GroupNames[NumGroups] = {
		TEXT("view_distance"), TEXT("shadow"), TEXT("gi"), TEXT("reflection"), TEXT("foliage"),
		TEXT("effects"), TEXT("post_process"), TEXT("texture"), TEXT("shading"), TEXT("anti_aliasing") };

	/** Auto render resolution at a 1080p window per level (Low .. Cinematic), percent. */
	const float AutoBase[NumLevels] = { 50.f, 58.f, 67.f, 75.f, 100.f };

	struct FState
	{
		bool bLoaded = false;
		/** The player's graphics are stored (else a new install: Auto, the engine's levels until detected). */
		bool bStored = false;
		bool bAuto = true;
		int32 Levels[NumGroups];
		int32 Resolution = 0;
		int32 FrameLimit = 60;
		bool bVSync = false;
		bool bPowerSaving = true;
	};

	/** The editor's own state before a Play applied the player's, put back when Play stops. */
	struct FEditorSnapshot
	{
		bool bValid = false;
		Scalability::FQualityLevels Levels;
		float MaxFps = 0.f;
		float ScreenPercentage = 100.f;
		float TSRHistory = 100.f;
		int32 VSync = 0;
	};

	FState GState;
	FEditorSnapshot GSnapshot;
	bool bApplied = false;
	bool bMenuOpen = false;
	bool bWorldIsPIE = false;
	float LastFpsCap = -1.f;
	float LastPercent = -1.f;
	/** What UpdateTSRHistory last set (-1: never, the scalability levels own the variable) and what it went by. */
	float LastHistory = -1.f;
	int32 LastHistoryLevel = -1;
	int32 LastHistoryHeight = -1;
	int32 LastHistoryAuto = -1;

	int32& LevelRef(Scalability::FQualityLevels& Q, EGroup G)
	{
		switch (G)
		{
		case EGroup::ViewDistance: return Q.ViewDistanceQuality;
		case EGroup::Shadow: return Q.ShadowQuality;
		case EGroup::GlobalIllumination: return Q.GlobalIlluminationQuality;
		case EGroup::Reflection: return Q.ReflectionQuality;
		case EGroup::Foliage: return Q.FoliageQuality;
		case EGroup::Effects: return Q.EffectsQuality;
		case EGroup::PostProcess: return Q.PostProcessQuality;
		case EGroup::Texture: return Q.TextureQuality;
		case EGroup::Shading: return Q.ShadingQuality;
		default: return Q.AntiAliasingQuality;
		}
	}

	FState& State()
	{
		FState& S = GState;
		if (S.bLoaded)
		{
			return S;
		}
		S.bLoaded = true;
		// until something is stored: the levels the engine started with (GameUserSettings.ini's [ScalabilityGroups], Epic by default)
		Scalability::FQualityLevels Q = Scalability::GetQualityLevels();
		for (int32 I = 0; I < NumGroups; ++I)
		{
			S.Levels[I] = FMath::Clamp(LevelRef(Q, static_cast<EGroup>(I)), 0, NumLevels - 1);
		}
		if (!GConfig)
		{
			return S;
		}
		FString Levels;
		if (GConfig->GetString(Section, TEXT("Levels"), Levels, GGameUserSettingsIni) && !Levels.IsEmpty())
		{
			TArray<FString> Parts;
			Levels.ParseIntoArray(Parts, TEXT(","));
			for (int32 I = 0; I < NumGroups && I < Parts.Num(); ++I)
			{
				S.Levels[I] = FMath::Clamp(FCString::Atoi(*Parts[I]), 0, NumLevels - 1);
			}
			S.bStored = true;
		}
		GConfig->GetBool(Section, TEXT("Auto"), S.bAuto, GGameUserSettingsIni);
		GConfig->GetInt(Section, TEXT("Resolution"), S.Resolution, GGameUserSettingsIni);
		GConfig->GetInt(Section, TEXT("FrameLimit"), S.FrameLimit, GGameUserSettingsIni);
		GConfig->GetBool(Section, TEXT("VSync"), S.bVSync, GGameUserSettingsIni);
		GConfig->GetBool(Section, TEXT("PowerSaving"), S.bPowerSaving, GGameUserSettingsIni);
		S.Resolution = S.Resolution <= 0 ? 0 : FMath::Clamp(S.Resolution, 30, 100);
		S.FrameLimit = FMath::Max(0, S.FrameLimit);
		return S;
	}

	void Store()
	{
		FState& S = State();
		S.bStored = true;
		if (!GConfig)
		{
			return;
		}
		TArray<FString> Parts;
		for (int32 I = 0; I < NumGroups; ++I)
		{
			Parts.Add(FString::FromInt(S.Levels[I]));
		}
		GConfig->SetString(Section, TEXT("Levels"), *FString::Join(Parts, TEXT(",")), GGameUserSettingsIni);
		GConfig->SetBool(Section, TEXT("Auto"), S.bAuto, GGameUserSettingsIni);
		GConfig->SetInt(Section, TEXT("Resolution"), S.Resolution, GGameUserSettingsIni);
		GConfig->SetInt(Section, TEXT("FrameLimit"), S.FrameLimit, GGameUserSettingsIni);
		GConfig->SetBool(Section, TEXT("VSync"), S.bVSync, GGameUserSettingsIni);
		GConfig->SetBool(Section, TEXT("PowerSaving"), S.bPowerSaving, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	IConsoleVariable* Find(const TCHAR* Name)
	{
		return IConsoleManager::Get().FindConsoleVariable(Name);
	}

	/** Whether a set with priority By would take (a value set from the console or the command line wins over the settings). */
	bool CanSet(const IConsoleVariable* V, EConsoleVariableFlags By)
	{
		return V && (static_cast<uint32>(V->GetFlags()) & ECVF_SetByMask) <= static_cast<uint32>(By);
	}

	void SetFloat(const TCHAR* Name, float Value, EConsoleVariableFlags By)
	{
		IConsoleVariable* V = Find(Name);
		if (CanSet(V, By) && !FMath::IsNearlyEqual(V->GetFloat(), Value, 0.001f))
		{
			V->Set(Value, By);
		}
	}

	/** A run someone watches: not offscreen, not unattended, not a benchmark or a fixed-step recording, not a commandlet. */
	bool Interactive()
	{
		static const bool bInteractive = [] {
			const TCHAR* Cmd = FCommandLine::Get();
			return FApp::CanEverRender() && !FApp::IsUnattended() && !IsRunningCommandlet() && !GIsAutomationTesting && !FApp::UseFixedTimeStep()
				&& !FParse::Param(Cmd, TEXT("RenderOffscreen")) && !FParse::Param(Cmd, TEXT("LyudBench"));
		}();
		return bInteractive;
	}

	/**
	 * The render resolution and TSR's history follow the settings in the player's runs and in the benchmark (it measures
	 * what a player gets); offscreen tests and recordings keep the project's (r.ScreenPercentage=75) and their own.
	 */
	bool ManagesResolution()
	{
		static const bool bBench = FParse::Param(FCommandLine::Get(), TEXT("LyudBench"));
		return Interactive() || bBench;
	}

	float ViewDistanceScale()
	{
		static IConsoleVariable* const V = Find(TEXT("r.ViewDistanceScale"));
		return V ? V->GetFloat() : 1.f;
	}

	bool GovernorOn()
	{
		const int32 Mode = CVarGfxGovernor.GetValueOnGameThread();
		return Mode > 0 || (Mode < 0 && Interactive());
	}

	UGameUserSettings* GameSettings()
	{
		// the editor's Play keeps the editor's own (its scalability is saved to EditorSettings.ini), and offscreen and
		// unattended runs leave GameUserSettings.ini alone: only the game's own window uses these
		return !GIsEditor && GEngine && Interactive() ? GEngine->GetGameUserSettings() : nullptr;
	}

	UGameViewportClient* GameViewport()
	{
		return GEngine ? GEngine->GameViewport : nullptr;
	}

	FIntPoint ViewSize()
	{
		UGameViewportClient* GV = GameViewport();
		return GV && GV->Viewport ? GV->Viewport->GetSizeXY() : FIntPoint(1920, 1080);
	}

	bool AppInBackground()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return false;
		}
		if (!FSlateApplication::Get().IsActive())
		{
			return true;
		}
		UGameViewportClient* GV = GameViewport();
		TSharedPtr<SWindow> Window = GV ? GV->GetWindow() : nullptr;
		return Window.IsValid() && Window->IsWindowMinimized();
	}

	float AutoPercent(int32 Level, int32 Height)
	{
		const int32 L = FMath::Clamp(Level, 0, NumLevels - 1);
		if (L == NumLevels - 1)
		{
			return 100.f;
		}
		const float Ref = FMath::Max(240.f, CVarGfxAutoRefHeight.GetValueOnGameThread());
		const float Scale = Height > Ref ? FMath::Sqrt(Ref / static_cast<float>(Height)) : 1.f;
		return FMath::Clamp(FMath::RoundToFloat(AutoBase[L] * Scale), L == 0 ? 40.f : 50.f, 100.f);
	}

	/** The render resolution the settings want for the window now. */
	float WantedPercent()
	{
		const FState& S = State();
		if (S.Resolution > 0)
		{
			return static_cast<float>(S.Resolution);
		}
		// Auto follows the anti-aliasing (TSR) level as it is now, so `scalability N` and the benchmark move it too
		static IConsoleVariable* AA = Find(TEXT("sg.AntiAliasingQuality"));
		return AutoPercent(AA ? AA->GetInt() : 3, ViewSize().Y);
	}

	void UpdateScreenPercentage(bool bForce)
	{
		if (!ManagesResolution())
		{
			return;
		}
		const float Want = WantedPercent();
		if (!bForce && FMath::IsNearlyEqual(Want, LastPercent))
		{
			return;
		}
		LastPercent = Want;
		SetFloat(TEXT("r.ScreenPercentage"), Want, ECVF_SetByCode);
	}

	/** r.TSR.History.ScreenPercentage of an anti-aliasing level (Config/DefaultScalability.ini over the engine's), 0 when it sets none. */
	float LevelTSRHistory(int32 Level)
	{
		const int32 L = FMath::Clamp(Level, 0, NumLevels - 1);
		// the top level's section is <Group>@Cine (Scalability.cpp, GetScalabilitySectionString)
		const FString Name = L == NumLevels - 1 ? FString(TEXT("AntiAliasingQuality@Cine")) : FString::Printf(TEXT("AntiAliasingQuality@%d"), L);
		FString Value;
		return GConfig && GConfig->GetString(*Name, TEXT("r.TSR.History.ScreenPercentage"), Value, GScalabilityIni) ? FCString::Atof(*Value) : 0.f;
	}

	/**
	 * TSR keeps its history at the output's size x r.TSR.History.ScreenPercentage: Epic's 200% is 4x the window's pixels, on
	 * a fullscreen Retina Mac (3024x1964) a 6048x3928 history. Bigger windows get less of it, as they get less render resolution.
	 * Only when the window or the level changes (the ini lookup builds strings).
	 */
	void UpdateTSRHistory(bool bForce)
	{
		if (!ManagesResolution())
		{
			return;
		}
		static IConsoleVariable* const AA = Find(TEXT("sg.AntiAliasingQuality"));
		const int32 Level = AA ? AA->GetInt() : 3;
		const int32 Height = ViewSize().Y;
		const int32 Auto = CVarGfxAutoTSRHistory.GetValueOnGameThread();
		if (!bForce && Level == LastHistoryLevel && Height == LastHistoryHeight && Auto == LastHistoryAuto)
		{
			return;
		}
		LastHistoryLevel = Level;
		LastHistoryHeight = Height;
		LastHistoryAuto = Auto;
		const float Base = LevelTSRHistory(Level);
		if (Base <= 0.f)
		{
			return;
		}
		const float Ref = FMath::Max(240.f, CVarGfxAutoRefHeight.GetValueOnGameThread());
		float Want = Base;
		if (Auto != 0 && Base > 100.f && Height > Ref)
		{
			Want = FMath::Max(100.f, FMath::RoundToFloat(Base * FMath::Sqrt(Ref / static_cast<float>(Height))));
		}
		// the level's own value is the scalability's to set; the game takes the variable over only to lower it, then keeps it
		if (Want != Base || LastHistory >= 0.f)
		{
			SetFloat(TEXT("r.TSR.History.ScreenPercentage"), Want, ECVF_SetByCode);
			LastHistory = Want;
		}
	}

	void UpdateFrameCap(bool bForce)
	{
		// the limit is the player's: offscreen, unattended and benchmark runs draw as fast as they can and keep their own t.MaxFPS
		if (!Interactive() && CVarGfxGovernor.GetValueOnGameThread() <= 0)
		{
			return;
		}
		const FState& S = State();
		float Want = static_cast<float>(S.FrameLimit);
		auto Cap = [&Want](int32 Fps) { if (Fps > 0) { Want = Want > 0.f ? FMath::Min(Want, static_cast<float>(Fps)) : static_cast<float>(Fps); } };
		if (S.bPowerSaving && GovernorOn())
		{
			if (AppInBackground())
			{
				Cap(CVarGfxBackgroundFps.GetValueOnGameThread());
			}
			else if (bMenuOpen)
			{
				Cap(CVarGfxMenuFps.GetValueOnGameThread());
			}
		}
		if (!bForce && Want == LastFpsCap)
		{
			return;
		}
		LastFpsCap = Want;
		// as UEngine::SetMaxFPS sets it; t.MaxFPS from the console or -ExecCmds (the benchmark's 0) stays
		SetFloat(TEXT("t.MaxFPS"), Want, ECVF_SetByCode);
	}

	void ApplyVSync()
	{
		const FState& S = State();
		if (IConsoleVariable* V = Find(TEXT("r.VSync")); CanSet(V, ECVF_SetByGameSetting) && V->GetInt() != (S.bVSync ? 1 : 0))
		{
			V->Set(S.bVSync ? 1 : 0, ECVF_SetByGameSetting);
		}
		if (UGameUserSettings* GS = GameSettings())
		{
			GS->SetVSyncEnabled(S.bVSync);
			GS->SetFrameRateLimit(static_cast<float>(S.FrameLimit));
			GS->SaveSettings();
		}
	}

	void ApplyLevels()
	{
		const FState& S = State();
		Scalability::FQualityLevels Q = Scalability::GetQualityLevels();
		for (int32 I = 0; I < NumGroups; ++I)
		{
			LevelRef(Q, static_cast<EGroup>(I)) = S.Levels[I];
		}
		Scalability::SetQualityLevels(Q);
		if (UGameUserSettings* GS = GameSettings())
		{
			// the game's own settings object keeps the same levels, so nothing that applies it later puts the old ones back
			GS->SetViewDistanceQuality(S.Levels[static_cast<int32>(EGroup::ViewDistance)]);
			GS->SetShadowQuality(S.Levels[static_cast<int32>(EGroup::Shadow)]);
			GS->SetGlobalIlluminationQuality(S.Levels[static_cast<int32>(EGroup::GlobalIllumination)]);
			GS->SetReflectionQuality(S.Levels[static_cast<int32>(EGroup::Reflection)]);
			GS->SetFoliageQuality(S.Levels[static_cast<int32>(EGroup::Foliage)]);
			GS->SetVisualEffectQuality(S.Levels[static_cast<int32>(EGroup::Effects)]);
			GS->SetPostProcessingQuality(S.Levels[static_cast<int32>(EGroup::PostProcess)]);
			GS->SetTextureQuality(S.Levels[static_cast<int32>(EGroup::Texture)]);
			GS->SetShadingQuality(S.Levels[static_cast<int32>(EGroup::Shading)]);
			GS->SetAntiAliasingQuality(S.Levels[static_cast<int32>(EGroup::AntiAliasing)]);
			GS->SaveSettings();
		}
		UpdateScreenPercentage(true);
		UpdateTSRHistory(true);
	}

	void SetAllLevels(int32 Level)
	{
		FState& S = State();
		for (int32 I = 0; I < NumGroups; ++I)
		{
			S.Levels[I] = FMath::Clamp(Level, 0, NumLevels - 1);
		}
	}

	void TakeEditorSnapshot()
	{
		GSnapshot.bValid = true;
		GSnapshot.Levels = Scalability::GetQualityLevels();
		const IConsoleVariable* Fps = Find(TEXT("t.MaxFPS"));
		const IConsoleVariable* Sp = Find(TEXT("r.ScreenPercentage"));
		const IConsoleVariable* VSync = Find(TEXT("r.VSync"));
		const IConsoleVariable* History = Find(TEXT("r.TSR.History.ScreenPercentage"));
		GSnapshot.MaxFps = Fps ? Fps->GetFloat() : 0.f;
		GSnapshot.ScreenPercentage = Sp ? Sp->GetFloat() : 100.f;
		GSnapshot.TSRHistory = History ? History->GetFloat() : 100.f;
		GSnapshot.VSync = VSync ? VSync->GetInt() : 0;
		// No need to turn r.Editor.Viewport.OverridePIEScreenPercentage off: a positive r.ScreenPercentage wins over the editor
		// viewports' settings in Play too (LegacyScreenPercentageDriver.cpp, PullRunTimeRenderingSettings).
	}

	void RestoreEditorSnapshot()
	{
		if (!GSnapshot.bValid)
		{
			return;
		}
		GSnapshot.bValid = false;
		Scalability::SetQualityLevels(GSnapshot.Levels);
		SetFloat(TEXT("t.MaxFPS"), GSnapshot.MaxFps, ECVF_SetByCode);
		SetFloat(TEXT("r.ScreenPercentage"), GSnapshot.ScreenPercentage, ECVF_SetByCode);
		if (LastHistory >= 0.f)
		{
			SetFloat(TEXT("r.TSR.History.ScreenPercentage"), GSnapshot.TSRHistory, ECVF_SetByCode);
		}
		if (IConsoleVariable* V = Find(TEXT("r.VSync")); CanSet(V, ECVF_SetByGameSetting))
		{
			V->Set(GSnapshot.VSync, ECVF_SetByGameSetting);
		}
	}

	/** "Apple M5 Max" -> 5 (0 when not an M chip). */
	int32 AppleGeneration(const FString& Gpu)
	{
		for (int32 I = 0; I + 1 < Gpu.Len(); ++I)
		{
			if (Gpu[I] == TEXT('M') && FChar::IsDigit(Gpu[I + 1]) && (I == 0 || Gpu[I - 1] == TEXT(' ')))
			{
				return FCString::Atoi(*Gpu.Mid(I + 1, 2));
			}
		}
		return 0;
	}
}

namespace LyudGraphics
{
	using namespace Local;

	const TCHAR* GroupName(EGroup Group)
	{
		return GroupNames[FMath::Clamp(static_cast<int32>(Group), 0, NumGroups - 1)];
	}

	int32 DetectLevel(FString* OutGpu, int32* OutMemoryGB)
	{
		const FString Gpu = GRHIAdapterName.TrimStartAndEnd();
		const int32 MemoryGB = static_cast<int32>(FPlatformMemory::GetConstants().TotalPhysicalGB);
		if (OutGpu) { *OutGpu = Gpu; }
		if (OutMemoryGB) { *OutMemoryGB = MemoryGB; }
		int32 Level = 3;
#if PLATFORM_IOS || PLATFORM_ANDROID
		// phones and tablets: the device profile has already chosen
		Scalability::FQualityLevels Q = Scalability::GetQualityLevels();
		Level = NumLevels - 1;
		for (int32 I = 0; I < NumGroups; ++I)
		{
			Level = FMath::Min(Level, LevelRef(Q, static_cast<EGroup>(I)));
		}
#else
		const int32 Gen = AppleGeneration(Gpu);
		if (Gpu.StartsWith(TEXT("Apple")) && Gen > 0)
		{
			// Apple silicon by its tier: Max and Ultra carry Epic (the M5 Max ~20-25 ms on the route at 1080p), a Pro
			// High (Epic from the M4 Pro on), the plain chip Medium (High from the M5 on)
			const bool bMax = Gpu.Contains(TEXT("Max")) || Gpu.Contains(TEXT("Ultra"));
			const bool bPro = Gpu.Contains(TEXT("Pro"));
			Level = bMax ? 3 : bPro ? (Gen >= 4 ? 3 : 2) : (Gen >= 5 ? 2 : 1);
		}
		else if (FApp::CanEverRender())
		{
			// anything else: the engine's quick CPU and GPU test (the thresholds of [ScalabilitySettings])
			const Scalability::FQualityLevels Q = Scalability::BenchmarkQualityLevels();
			Level = FMath::Min3(Q.ShadowQuality, Q.GlobalIlluminationQuality, Q.PostProcessQuality);
		}
#endif
		// the world holds ~13-20 GB (the forests' baked trees): less memory, lighter levels
		if (MemoryGB > 0 && MemoryGB < 16)
		{
			Level = FMath::Min(Level, 1);
		}
		else if (MemoryGB > 0 && MemoryGB < 24)
		{
			Level = FMath::Min(Level, 2);
		}
		return FMath::Clamp(Level, 0, 3);
	}

	void ApplySaved(UWorld* World)
	{
		if (bApplied)
		{
			return;
		}
		bApplied = true;
		bWorldIsPIE = World && World->WorldType == EWorldType::PIE;
		FState& S = State();
		if (bWorldIsPIE)
		{
			TakeEditorSnapshot();
		}
		else if (S.bAuto && Interactive() && GConfig)
		{
			// a new install, or the settings came to another machine: pick the level for this one
			FString Last;
			GConfig->GetString(Section, TEXT("DetectedGpu"), Last, GGameUserSettingsIni);
			if (!S.bStored || Last != GRHIAdapterName.TrimStartAndEnd())
			{
				FString Gpu;
				int32 Memory = 0;
				const int32 Level = DetectLevel(&Gpu, &Memory);
				SetAllLevels(Level);
				GConfig->SetString(Section, TEXT("DetectedGpu"), *Gpu, GGameUserSettingsIni);
				Store();
				UE_LOG(LogTemp, Log, TEXT("Graphics: Auto picked level %d for %s, %d GB"), Level, *Gpu, Memory);
			}
		}
		// the editor's Play without settings of its own keeps the editor's levels
		if (S.bStored || !bWorldIsPIE)
		{
			ApplyLevels();
		}
		ApplyVSync();
		UpdateScreenPercentage(true);
		UpdateTSRHistory(true);
		UpdateFrameCap(true);
	}

	int32 Preset()
	{
		const FState& S = State();
		for (int32 I = 1; I < NumGroups; ++I)
		{
			if (S.Levels[I] != S.Levels[0])
			{
				return -1;
			}
		}
		return S.Levels[0];
	}

	bool IsAuto()
	{
		return State().bAuto;
	}

	void SetPreset(int32 Level)
	{
		FState& S = State();
		S.bAuto = false;
		SetAllLevels(Level);
		Store();
		ApplyLevels();
	}

	void SetAuto()
	{
		FState& S = State();
		S.bAuto = true;
		FString Gpu;
		SetAllLevels(DetectLevel(&Gpu));
		if (GConfig)
		{
			GConfig->SetString(Section, TEXT("DetectedGpu"), *Gpu, GGameUserSettingsIni);
		}
		Store();
		ApplyLevels();
	}

	int32 GroupLevel(EGroup Group)
	{
		return State().Levels[FMath::Clamp(static_cast<int32>(Group), 0, NumGroups - 1)];
	}

	void SetGroupLevel(EGroup Group, int32 Level)
	{
		FState& S = State();
		S.bAuto = false;
		S.Levels[FMath::Clamp(static_cast<int32>(Group), 0, NumGroups - 1)] = FMath::Clamp(Level, 0, NumLevels - 1);
		Store();
		ApplyLevels();
	}

	int32 Resolution()
	{
		return State().Resolution;
	}

	void SetResolution(int32 Percent)
	{
		FState& S = State();
		S.Resolution = Percent <= 0 ? 0 : FMath::Clamp(Percent, 30, 100);
		Store();
		UpdateScreenPercentage(true);
	}

	float ScreenPercentage()
	{
		const IConsoleVariable* Sp = Find(TEXT("r.ScreenPercentage"));
		return Sp ? Sp->GetFloat() : 100.f;
	}

	FIntPoint OutputSize()
	{
		return ViewSize();
	}

	FString MachineGpu()
	{
		return GRHIAdapterName.TrimStartAndEnd();
	}

	int32 MachineMemoryGB()
	{
		return static_cast<int32>(FPlatformMemory::GetConstants().TotalPhysicalGB);
	}

	int32 FrameLimit()
	{
		return State().FrameLimit;
	}

	void SetFrameLimit(int32 Fps)
	{
		State().FrameLimit = FMath::Max(0, Fps);
		Store();
		ApplyVSync();
		UpdateFrameCap(true);
	}

	bool VSync()
	{
		return State().bVSync;
	}

	void SetVSync(bool bOn)
	{
		State().bVSync = bOn;
		Store();
		ApplyVSync();
	}

	bool CanSetWindowMode()
	{
#if PLATFORM_DESKTOP
		return !GIsEditor && GameSettings() != nullptr;
#else
		return false;
#endif
	}

	bool Fullscreen()
	{
		const UGameUserSettings* GS = GameSettings();
		return GS && GS->GetFullscreenMode() != EWindowMode::Windowed;
	}

	void SetFullscreen(bool bOn)
	{
		UGameUserSettings* GS = GameSettings();
		if (!GS || !CanSetWindowMode())
		{
			return;
		}
		const FIntPoint Desktop = GS->GetDesktopResolution();
		GS->SetFullscreenMode(bOn ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
		// a window three quarters of the screen, fullscreen at the screen's own size
		GS->SetScreenResolution(bOn ? Desktop : FIntPoint(Desktop.X * 3 / 4, Desktop.Y * 3 / 4));
		GS->ApplyResolutionSettings(false);
		GS->SaveSettings();
	}

	bool PowerSaving()
	{
		return State().bPowerSaving;
	}

	void SetPowerSaving(bool bOn)
	{
		State().bPowerSaving = bOn;
		Store();
		UpdateFrameCap(true);
	}

	void SetMenuOpen(bool bOpen)
	{
		bMenuOpen = bOpen;
		UpdateFrameCap(false);
	}

	FString Describe()
	{
		const FState& S = State();
		TArray<FString> Levels;
		for (int32 I = 0; I < NumGroups; ++I)
		{
			Levels.Add(FString::Printf(TEXT("%s=%d"), GroupNames[I], S.Levels[I]));
		}
		FString Gpu;
		int32 Memory = 0;
#if PLATFORM_IOS || PLATFORM_ANDROID
		const int32 Detected = DetectLevel(&Gpu, &Memory);
#else
		// (the engine's GPU test is not run for a status line)
		const int32 Detected = GRHIAdapterName.StartsWith(TEXT("Apple")) ? DetectLevel(&Gpu, &Memory) : -1;
#endif
		const FIntPoint Size = ViewSize();
		const IConsoleVariable* Fps = Find(TEXT("t.MaxFPS"));
		const IConsoleVariable* History = Find(TEXT("r.TSR.History.ScreenPercentage"));
		return FString::Printf(TEXT("preset %d%s [%s] stored %d | render %.0f%% (%s) of %dx%d, tsr history %.0f%%, view distance x%.2f | fps cap %.0f (limit %d, menu %d, background %d, power saving %d, governor %d) vsync %d | detect %d (%s, %d GB)"),
			Preset(), S.bAuto ? TEXT(" auto") : TEXT(""), *FString::Join(Levels, TEXT(" ")), S.bStored ? 1 : 0,
			ScreenPercentage(), S.Resolution > 0 ? *FString::Printf(TEXT("%d%%"), S.Resolution) : TEXT("auto"), Size.X, Size.Y,
			History ? History->GetFloat() : 0.f, ViewDistanceScale(),
			Fps ? Fps->GetFloat() : 0.f, S.FrameLimit, bMenuOpen ? 1 : 0, AppInBackground() ? 1 : 0, S.bPowerSaving ? 1 : 0, GovernorOn() ? 1 : 0,
			S.bVSync ? 1 : 0, Detected, *Gpu, Memory);
	}
}

bool ULyudGraphicsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId ULyudGraphicsSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudGraphicsSubsystem, STATGROUP_Tickables);
}

void ULyudGraphicsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!FApp::CanEverRender() || IsRunningDedicatedServer())
	{
		return;
	}
	// the instanced meshes already in the level took the view distance in as it was before the settings
	SeenViewDistanceScale = LyudGraphics::Local::ViewDistanceScale();
	LyudGraphics::ApplySaved(&InWorld);
	bStarted = true;
}

void ULyudGraphicsSubsystem::Deinitialize()
{
	using namespace LyudGraphics::Local;
	if (bStarted && bWorldIsPIE)
	{
		// Play stopped (or travels to another level, which applies them again): the editor gets its own settings back
		RestoreEditorSnapshot();
		bApplied = false;
		bMenuOpen = false;
		LastFpsCap = -1.f;
		LastPercent = -1.f;
		LastHistoryLevel = -1;
		LastHistoryHeight = -1;
	}
	bStarted = false;
	Super::Deinitialize();
}

void ULyudGraphicsSubsystem::Tick(float DeltaTime)
{
	// (UTickableWorldSubsystem::Tick only checks that the subsystem is initialized)
	using namespace LyudGraphics::Local;
	// the window may be resized or the levels changed from the console: the resolution follows; the focus moves the frame cap
	UpdateScreenPercentage(false);
	UpdateTSRHistory(false);
	UpdateFrameCap(false);
	const float Scale = ViewDistanceScale();
	if (Scale != SeenViewDistanceScale)
	{
		SeenViewDistanceScale = Scale;
		// two frames on, when the engine's scalability sink has cached the new scale for the renderer
		RefreshCountdown = 2;
	}
	if (RefreshCountdown > 0 && --RefreshCountdown == 0 && CVarGfxRefreshInstances.GetValueOnGameThread() != 0)
	{
		RefreshInstances();
	}
}

void ULyudGraphicsSubsystem::RefreshInstances()
{
	// instanced meshes (forests, grass, the forest floor, the seabed, birds) carry their draw and wind distances x
	// r.ViewDistanceScale into the GPU scene when they are sent to the renderer, and a change of the scale alone resends
	// nothing (UnrealEngine.cpp, ScalabilityCVarsSinkCallback): once per change of the View distance level, never per frame
	const UWorld* World = GetWorld();
	int32 Count = 0;
	for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
	{
		if (It->GetWorld() == World && It->IsRegistered())
		{
			It->MarkRenderStateDirty();
			++Count;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Graphics: view distance x%.2f, %d instanced meshes resent to the renderer"), SeenViewDistanceScale, Count);
}

static FAutoConsoleCommand GLyudGfxCmd(TEXT("lyud.Gfx"), TEXT("lyud.Gfx: prints the graphics settings and what they resolve to now."),
	FConsoleCommandDelegate::CreateLambda([]
	{
		UE_LOG(LogTemp, Warning, TEXT("GFXTEST %s"), *LyudGraphics::Describe());
	}));

static FAutoConsoleCommandWithArgsAndOutputDevice GLyudGfxPresetCmd(TEXT("lyud.Gfx.Preset"),
	TEXT("lyud.Gfx.Preset auto|0..4: sets and stores the graphics preset as the Graphics page does (0 Low .. 4 Cinematic)."),
	FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, FOutputDevice&)
	{
		if (Args.Num() && Args[0] == TEXT("auto"))
		{
			LyudGraphics::SetAuto();
		}
		else if (Args.Num())
		{
			LyudGraphics::SetPreset(FCString::Atoi(*Args[0]));
		}
		UE_LOG(LogTemp, Warning, TEXT("GFXTEST %s"), *LyudGraphics::Describe());
	}));

static FAutoConsoleCommandWithArgsAndOutputDevice GLyudGfxGroupCmd(TEXT("lyud.Gfx.Group"),
	TEXT("lyud.Gfx.Group <view_distance|shadow|gi|reflection|foliage|effects|post_process|texture|shading|anti_aliasing> <0..4>: one group as the Graphics page sets it."),
	FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, FOutputDevice&)
	{
		for (int32 I = 0; Args.Num() > 1 && I < LyudGraphics::NumGroups; ++I)
		{
			const LyudGraphics::EGroup G = static_cast<LyudGraphics::EGroup>(I);
			if (Args[0] == LyudGraphics::GroupName(G))
			{
				LyudGraphics::SetGroupLevel(G, FCString::Atoi(*Args[1]));
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("GFXTEST %s"), *LyudGraphics::Describe());
	}));

static FAutoConsoleCommandWithArgsAndOutputDevice GLyudGfxResolutionCmd(TEXT("lyud.Gfx.Resolution"),
	TEXT("lyud.Gfx.Resolution <0 auto | 30..100>: the render resolution as the Graphics page sets it."),
	FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, FOutputDevice&)
	{
		if (Args.Num())
		{
			LyudGraphics::SetResolution(FCString::Atoi(*Args[0]));
		}
		UE_LOG(LogTemp, Warning, TEXT("GFXTEST %s"), *LyudGraphics::Describe());
	}));

static FAutoConsoleCommandWithArgsAndOutputDevice GLyudGfxFrameLimitCmd(TEXT("lyud.Gfx.FrameLimit"),
	TEXT("lyud.Gfx.FrameLimit <fps, 0 none>: the frame-rate limit as the Graphics page sets it."),
	FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, FOutputDevice&)
	{
		if (Args.Num())
		{
			LyudGraphics::SetFrameLimit(FCString::Atoi(*Args[0]));
		}
		UE_LOG(LogTemp, Warning, TEXT("GFXTEST %s"), *LyudGraphics::Describe());
	}));
