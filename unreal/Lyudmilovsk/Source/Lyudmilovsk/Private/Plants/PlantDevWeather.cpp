// The F7 panel's weather section: everything the weather engine, the rare sky, the winter and the star let a
// tester force, behind buttons. All of it goes through the same console variables and presets as the console
// (docs/weather.md), so whatever is set here can also be read or undone there; Auto clears them all.

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Plants/PlantDevTool.h"
#include "PlantDevWidgets.h"
#include "Sky/LyudSunActivity.h"
#include "TimerManager.h"
#include "Weather/LyudWeather.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SWrapBox.h"

using namespace PlantDevUI;

namespace
{
	float GetCVar(const TCHAR* Name)
	{
		const IConsoleVariable* C = IConsoleManager::Get().FindConsoleVariable(Name);
		return C ? C->GetFloat() : 0.f;
	}

	void SetCVar(const TCHAR* Name, float V)
	{
		if (IConsoleVariable* C = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			C->Set(V, ECVF_SetByConsole);
		}
	}

	const TCHAR* const Presets[] = {
		TEXT("clear"), TEXT("fair"), TEXT("overcast"), TEXT("drizzle"), TEXT("rain"), TEXT("downpour"), TEXT("storm"), TEXT("thunder"),
		TEXT("fog"), TEXT("seafog"), TEXT("lightsnow"), TEXT("snow"), TEXT("heavysnow"), TEXT("blizzard"), TEXT("freezingrain"), TEXT("frost"), TEXT("lenticular"),
		TEXT("aurora"), TEXT("halo"), TEXT("rainbow") };

	// ELyudWeatherRegime's order
	const TCHAR* const Regimes[] = {
		TEXT("High"), TEXT("WarmFront"), TEXT("WarmSector"), TEXT("ColdFront"), TEXT("Showery"), TEXT("Occluded"), TEXT("ArcticHigh"), TEXT("ThunderyLow") };

	// ELyudPrecipType's order
	const TCHAR* const Precips[] = {
		TEXT("none"), TEXT("drizzle"), TEXT("rain"), TEXT("showers"), TEXT("thunderstorm"), TEXT("hail"), TEXT("graupel"), TEXT("sleet"),
		TEXT("snow"), TEXT("snowshowers"), TEXT("freezingdrizzle"), TEXT("freezingrain") };

	/** A phenomenon forced on top of whatever the weather is: one console variable (and a companion) on or off. */
	struct FToggle
	{
		const TCHAR* Key;
		const TCHAR* CVar;
		float On, Off;
		const TCHAR* CVar2 = nullptr;
		float On2 = 0.f, Off2 = 0.f;

		bool IsOn() const
		{
			// a preset may have set the same variable: anything past halfway counts as on
			const float Mid = Off < 0.f ? On * 0.5f : 0.5f * (On + Off);
			return GetCVar(CVar) >= Mid;
		}
		void Set(bool bOn) const
		{
			SetCVar(CVar, bOn ? On : Off);
			if (CVar2)
			{
				SetCVar(CVar2, bOn ? On2 : Off2);
			}
		}
	};

	const FToggle Toggles[] = {
		{ TEXT("thunder"), TEXT("lyud.Weather.Thunder"), 0.8f, -1.f },
		{ TEXT("lenticular"), TEXT("lyud.Weather.Lenticular"), 1.f, -1.f },
		{ TEXT("fog"), TEXT("lyud.Weather.Fog"), 0.9f, -1.f },
		// a substorm that broke up ten minutes ago: the aurora at its liveliest
		{ TEXT("aurora"), TEXT("lyud.Rare.Aurora"), 1.f, -1.f, TEXT("lyud.Rare.AuroraOnset"), 10.f, -1e9f },
		{ TEXT("halo"), TEXT("lyud.Rare.Halo"), 1.f, -1.f },
		{ TEXT("rainbow"), TEXT("lyud.Rare.Rainbow"), 1.f, -1.f },
		{ TEXT("nlc"), TEXT("lyud.Rare.Noctilucent"), 1.f, -1.f },
		{ TEXT("flare"), TEXT("lyud.Rare.Flare"), 1.f, -1.f },
		{ TEXT("activestar"), TEXT("lyud.Sun.Activity"), 3.f, 1.f },
		{ TEXT("snowcover"), TEXT("lyud.Weather.SnowDepth"), 30.f, -1.f },
		{ TEXT("drifts"), TEXT("lyud.Winter.Drift"), 1.f, -1.f },
		{ TEXT("canopy"), TEXT("lyud.Winter.Canopy"), 1.f, -1.f },
		{ TEXT("seaice"), TEXT("lyud.Weather.SeaIce"), 30.f, -1.f },
		{ TEXT("lowwater"), TEXT("lyud.Winter.LowWaterNow"), 1.f, -1.f },
	};

	/** A value slider whose far left (the first 4%) hands the value back to the weather. */
	struct FValue
	{
		const TCHAR* Key;
		const TCHAR* CVar;
		float Min, Max, AutoBelow;
		float Scale; // shown = value x Scale (cover and fog as %)
		float (*Live)(const FLyudWeatherState&);
	};
	constexpr float AutoZone = 0.04f;

	const FValue Values[] = {
		{ TEXT("cloud"), TEXT("lyud.Weather.Cloud"), 0.f, 1.f, 0.f, 100.f, [](const FLyudWeatherState& S) { return S.CloudCover; } },
		{ TEXT("rain"), TEXT("lyud.Weather.Rain"), 0.f, 20.f, 0.f, 1.f, [](const FLyudWeatherState& S) { return S.PrecipRate; } },
		{ TEXT("fog"), TEXT("lyud.Weather.Fog"), 0.f, 1.f, 0.f, 100.f, [](const FLyudWeatherState& S) { return S.Fog; } },
		{ TEXT("temp"), TEXT("lyud.Weather.Temp"), -30.f, 35.f, -99.f, 1.f, [](const FLyudWeatherState& S) { return S.TempC; } },
		{ TEXT("wind"), TEXT("lyud.Wind.Speed"), 0.f, 30.f, 0.f, 1.f, [](const FLyudWeatherState& S) { return S.WindSpeed; } },
	};

	FText Number(float V)
	{
		FNumberFormattingOptions O;
		O.MaximumFractionalDigits = FMath::Abs(V) < 10.f ? 1 : 0;
		return FText::AsNumber(V, &O);
	}

	FText PresetName(const FString& Name)
	{
		if (Name.StartsWith(TEXT("regime:")))
		{
			return LyudText::Get(TEXT("ui.dev.wx.regime.") + Name.Mid(7).ToLower());
		}
		return Name == TEXT("values") ? LyudText::Get(TEXT("ui.dev.wx.mode_values")) : LyudText::Get(TEXT("ui.dev.wx.preset.") + Name);
	}

	bool AnyForced()
	{
		for (const FValue& V : Values)
		{
			if (GetCVar(V.CVar) >= V.AutoBelow) return true;
		}
		for (const FToggle& T : Toggles)
		{
			if (T.IsOn()) return true;
		}
		return false;
	}

	FText StateLine(UWorld* World)
	{
		const ULyudWeatherSubsystem* W = World ? ULyudWeatherSubsystem::Get(World) : nullptr;
		if (!W)
		{
			return FText::GetEmpty();
		}
		const FLyudWeatherState S = W->GetState();
		const int32 R = FMath::Clamp((int32)S.Regime, 0, (int32)UE_ARRAY_COUNT(Regimes) - 1);
		const int32 P = FMath::Clamp((int32)S.Precip, 0, (int32)UE_ARRAY_COUNT(Precips) - 1);
		return LyudText::Get(TEXT("ui.dev.wx.state"), {
			FStringFormatArg(LyudText::Get(TEXT("ui.dev.wx.regime.") + FString(Regimes[R]).ToLower()).ToString()),
			FStringFormatArg(LyudText::Get(TEXT("ui.dev.wx.precip.") + FString(Precips[P])).ToString()),
			FStringFormatArg(Number(S.TempC).ToString()),
			FStringFormatArg(FString::FromInt(FMath::RoundToInt(S.CloudCover * 100.f))),
			FStringFormatArg(Number(S.PrecipRate).ToString()),
			FStringFormatArg(FString::FromInt(FMath::RoundToInt(S.Fog * 100.f))),
			FStringFormatArg(Number(S.WindSpeed).ToString()),
			FStringFormatArg(FString::FromInt(FMath::RoundToInt(S.SnowDepthCm))),
			FStringFormatArg(FString::FromInt(FMath::RoundToInt(S.SeaIceCm))),
			FStringFormatArg(Number(S.Kp).ToString()) });
	}

	/** A row of buttons that wraps to the panel's width. */
	TSharedRef<SWrapBox> Wrap()
	{
		return SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f));
	}
}

namespace PlantDevWeather
{
	void Auto(UWorld* World)
	{
		if (ULyudWeatherSubsystem* W = World ? ULyudWeatherSubsystem::Get(World) : nullptr)
		{
			W->SetPreset(TEXT("off"));
		}
		for (const FValue& V : Values)
		{
			SetCVar(V.CVar, V.AutoBelow < -1.f ? -100.f : -1.f);
		}
		for (const FToggle& T : Toggles)
		{
			T.Set(false);
		}
		SetCVar(TEXT("lyud.Wind.From"), -1.f);
		SetCVar(TEXT("lyud.Weather.Snow"), -1.f);
		SetCVar(TEXT("lyud.Weather.Kp"), -1.f);
		SetCVar(TEXT("lyud.Rare.Kp"), -1.f);
	}

	FString Report(UWorld* World)
	{
		const ULyudWeatherSubsystem* W = World ? ULyudWeatherSubsystem::Get(World) : nullptr;
		FString Out = W ? W->DebugReport(false) : FString(TEXT("no weather"));
		Out += TEXT(" | on:");
		for (const FToggle& T : Toggles)
		{
			if (T.IsOn())
			{
				Out += TEXT(" ") + FString(T.Key);
			}
		}
		for (const FValue& V : Values)
		{
			const float C = GetCVar(V.CVar);
			if (C >= V.AutoBelow)
			{
				Out += FString::Printf(TEXT(" %s=%.2f"), V.Key, C);
			}
		}
		return Out;
	}

	TSharedRef<SWidget> Build(APlantDevTool* Tool)
	{
		TWeakObjectPtr<APlantDevTool> Self(Tool);
		auto World = [Self]() -> UWorld* { return Self.IsValid() ? Self->GetWorld() : nullptr; };
		auto Weather = [World]() -> ULyudWeatherSubsystem* { return World() ? ULyudWeatherSubsystem::Get(World()) : nullptr; };
		auto Choose = [Self, Weather](const FString& Name)
		{
			if (!Self.IsValid() || !Weather()) return;
			PlantDevWeather::Auto(Self->GetWorld());
			Weather()->SetPreset(Name);
			Self->WeatherChoice = Name;
		};

		TSharedRef<SWrapBox> PresetRow = Wrap();
		for (const TCHAR* Name : Presets)
		{
			const FString N(Name);
			PresetRow->AddSlot()[Button(LyudText::Get(TEXT("ui.dev.wx.preset.") + N), [Choose, N]() { Choose(N); },
				[Self, N]() { return Self.IsValid() && Self->WeatherChoice == N; }, 10)];
		}
		TSharedRef<SWrapBox> RegimeRow = Wrap();
		for (const TCHAR* Name : Regimes)
		{
			const FString N = FString(TEXT("regime:")) + Name;
			RegimeRow->AddSlot()[Button(LyudText::Get(TEXT("ui.dev.wx.regime.") + FString(Name).ToLower()), [Choose, N]() { Choose(N); },
				[Self, N]() { return Self.IsValid() && Self->WeatherChoice == N; }, 10)];
		}

		TSharedRef<SWrapBox> ToggleRow = Wrap();
		// two moments rather than states: a strike, and a flare starting on the star's disk
		ToggleRow->AddSlot()[Button(LyudText::Get(TEXT("ui.dev.wx.ph.lightning")), [Weather]()
			{ if (Weather()) Weather()->Strike(FMath::FRandRange(900.f, 3000.f)); }, nullptr, 10)];
		ToggleRow->AddSlot()[Button(LyudText::Get(TEXT("ui.dev.wx.ph.sunflare")), [World]()
			{ if (ULyudSunActivity* Sun = World() ? ULyudSunActivity::Get(World()) : nullptr) Sun->ForceFlare(1.f); }, nullptr, 10)];
		for (const FToggle& T : Toggles)
		{
			const FToggle* TP = &T;
			ToggleRow->AddSlot()
			[
				SNew(SCheckBox).IsFocusable(false)
				.IsChecked_Lambda([TP]() { return TP->IsOn() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([TP, Self](ECheckBoxState State)
				{
					TP->Set(State == ECheckBoxState::Checked);
					if (Self.IsValid() && Self->WeatherChoice.IsEmpty()) Self->WeatherChoice = TEXT("values");
				})
				[SNew(STextBlock).Font(Font(10)).ColorAndOpacity(FLinearColor::White).Text(LyudText::Get(TEXT("ui.dev.wx.ph.") + FString(T.Key)))]
			];
		}

		TSharedRef<SVerticalBox> ValueRows = SNew(SVerticalBox);
		for (const FValue& V : Values)
		{
			const FValue* VP = &V;
			ValueRows->AddSlot().AutoHeight().Padding(0, 2)
			[SliderRow(LyudText::Get(TEXT("ui.dev.wx.") + FString(V.Key)), 0.f, 1.f,
				[VP]()
				{
					const float C = GetCVar(VP->CVar);
					return C < VP->AutoBelow ? 0.f : AutoZone + (1.f - AutoZone) * FMath::Clamp((C - VP->Min) / (VP->Max - VP->Min), 0.f, 1.f);
				},
				[VP, Self](float T)
				{
					SetCVar(VP->CVar, T < AutoZone ? (VP->AutoBelow < -1.f ? -100.f : -1.f) : VP->Min + (T - AutoZone) / (1.f - AutoZone) * (VP->Max - VP->Min));
					if (Self.IsValid() && Self->WeatherChoice.IsEmpty()) Self->WeatherChoice = TEXT("values");
				},
				[VP, Weather]()
				{
					const float C = GetCVar(VP->CVar);
					if (C >= VP->AutoBelow)
					{
						return Number(C * VP->Scale);
					}
					return LyudText::Get(TEXT("ui.dev.wx.auto_value"), { FStringFormatArg(Weather() ? Number(VP->Live(Weather()->GetState()) * VP->Scale).ToString() : FString()) });
				})];
		}
		ValueRows->AddSlot().AutoHeight().Padding(0, 2)
		[SliderRow(LyudText::Get(TEXT("ui.dev.wx.blend")), 0.f, 120.f,
			[]() { return GetCVar(TEXT("lyud.Weather.PresetBlend")); },
			[](float V) { SetCVar(TEXT("lyud.Weather.PresetBlend"), FMath::RoundToFloat(V)); },
			[]() { return FText::AsNumber(FMath::RoundToInt(GetCVar(TEXT("lyud.Weather.PresetBlend")))); })];

		auto Sub = [](const TCHAR* Key)
		{
			return SNew(STextBlock).Text(LyudText::Get(Key)).Font(Font(10)).ColorAndOpacity(FLinearColor(0.75f, 0.8f, 0.9f));
		};

		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[SNew(STextBlock).Font(Font(11)).ColorAndOpacity(FLinearColor::White).Text_Lambda([Self]()
					{
						// a preset stays chosen until Auto; loose values (set here or in the console) while one is still forced
						const bool bPreset = Self.IsValid() && !Self->WeatherChoice.IsEmpty() && Self->WeatherChoice != TEXT("values");
						if (bPreset || AnyForced())
						{
							return LyudText::Get(TEXT("ui.dev.wx.mode_forced"), { FStringFormatArg(PresetName(bPreset ? Self->WeatherChoice : FString(TEXT("values"))).ToString()) });
						}
						return LyudText::Get(TEXT("ui.dev.wx.mode_auto"));
					})]
				+ SHorizontalBox::Slot().AutoWidth()
				[Button(LyudText::Get(TEXT("ui.dev.wx.auto")), [Self]()
					{
						if (!Self.IsValid()) return;
						PlantDevWeather::Auto(Self->GetWorld());
						Self->WeatherChoice.Reset();
					}, [Self]() { return Self.IsValid() && Self->WeatherChoice.IsEmpty(); })]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[SNew(STextBlock).AutoWrapText(true).Font(Font(10)).ColorAndOpacity(FLinearColor(0.85f, 0.9f, 0.8f)).Text_Lambda([World]() { return StateLine(World()); })]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 2)[Sub(TEXT("ui.dev.wx.presets"))]
			+ SVerticalBox::Slot().AutoHeight()[PresetRow]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 2)[Sub(TEXT("ui.dev.wx.regimes"))]
			+ SVerticalBox::Slot().AutoHeight()[RegimeRow]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 2)[Sub(TEXT("ui.dev.wx.phenomena"))]
			+ SVerticalBox::Slot().AutoHeight()[ToggleRow]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 2)[Sub(TEXT("ui.dev.wx.values"))]
			+ SVerticalBox::Slot().AutoHeight()[ValueRows]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
			[SNew(STextBlock).AutoWrapText(true).Font(Font(9)).ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f)).Text(LyudText::Get(TEXT("ui.dev.wx.note")))];
	}
}
