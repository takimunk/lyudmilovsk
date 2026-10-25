#include "UI/LyudSettings.h"

#include "HAL/IConsoleManager.h"
#include "Audio/LyudSoundscape.h"
#include "LyudText.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* Section = TEXT("Lyudmilovsk.Settings");

	void SetCVar(const TCHAR* Name, const FString& Value)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			V->Set(*Value, ECVF_SetByGameSetting);
		}
	}

	const TCHAR* VolumeKeys[] = { TEXT("MasterVolume"), TEXT("AmbienceVolume"), TEXT("MusicVolume"), TEXT("EffectsVolume") };
	// the soundscape's own defaults
	const float VolumeDefaults[] = { 1.f, 1.f, 0.45f, 1.f };

	void Store(const TCHAR* Key, const FString& Value)
	{
		GConfig->SetString(Section, Key, *Value, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

namespace LyudSettings
{
	void ApplySaved(UWorld*)
	{
		static bool bApplied = false;
		if (bApplied || !GConfig)
		{
			return;
		}
		bApplied = true;
		FString Value;
		if (GConfig->GetString(Section, TEXT("Language"), Value, GGameUserSettingsIni) && !Value.IsEmpty())
		{
			SetCVar(TEXT("lyud.Language"), Value);
		}
		if (GConfig->GetString(Section, TEXT("Hud"), Value, GGameUserSettingsIni) && !Value.IsEmpty())
		{
			SetCVar(TEXT("lyud.Hud"), Value);
		}
	}

	void SetLanguage(const FString& Lang)
	{
		SetCVar(TEXT("lyud.Language"), Lang);
		LyudText::Reload();
		Store(TEXT("Language"), Lang);
	}

	bool HudShown()
	{
		const IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Hud"));
		return !V || V->GetInt() != 0;
	}

	void SetHudShown(bool bShow)
	{
		SetCVar(TEXT("lyud.Hud"), bShow ? TEXT("1") : TEXT("0"));
		Store(TEXT("Hud"), bShow ? TEXT("1") : TEXT("0"));
	}

	float Volume(EVolume Which)
	{
		const int32 I = static_cast<int32>(Which);
		FString Value;
		if (GConfig && GConfig->GetString(Section, VolumeKeys[I], Value, GGameUserSettingsIni) && !Value.IsEmpty())
		{
			return FMath::Clamp(FCString::Atof(*Value), 0.f, 1.f);
		}
		return VolumeDefaults[I];
	}

	void SetVolume(UWorld* World, EVolume Which, float Value)
	{
		Store(VolumeKeys[static_cast<int32>(Which)], FString::SanitizeFloat(FMath::Clamp(Value, 0.f, 1.f)));
		ApplyVolumes(World);
	}

	void ApplyVolumes(UWorld* World)
	{
		if (ULyudSoundscapeSubsystem* Sound = World ? ULyudSoundscapeSubsystem::Get(World) : nullptr)
		{
			Sound->SetVolumes(Volume(EVolume::Master), Volume(EVolume::Ambience), Volume(EVolume::Music), Volume(EVolume::Effects));
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GLyudVolumeCmd(TEXT("lyud.Settings.Volume"), TEXT("lyud.Settings.Volume master|ambience|music|effects <0..1>: sets and stores a volume as the Settings page does."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		static const TCHAR* Names[] = { TEXT("master"), TEXT("ambience"), TEXT("music"), TEXT("effects") };
		for (int32 I = 0; Args.Num() > 1 && I < 4; ++I)
		{
			if (Args[0] == Names[I])
			{
				LyudSettings::SetVolume(World, static_cast<LyudSettings::EVolume>(I), FCString::Atof(*Args[1]));
				const ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(World);
				UE_LOG(LogTemp, Warning, TEXT("SETTINGSTEST %s %.2f -> soundscape %.2f %.2f %.2f %.2f"), Names[I], LyudSettings::Volume(static_cast<LyudSettings::EVolume>(I)),
					S ? S->MasterVolume : -1.f, S ? S->AmbienceVolume : -1.f, S ? S->MusicVolume : -1.f, S ? S->EffectsVolume : -1.f);
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GLyudLanguageCmd(TEXT("lyud.Settings.Language"), TEXT("lyud.Settings.Language ru|en|ja: switches and stores the language as the Settings page does."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
	{
		if (Args.Num())
		{
			LyudSettings::SetLanguage(Args[0]);
		}
	}));
