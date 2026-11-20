#include "World/LyudCityDress.h"

#include "Dev/LyudDev.h"

#include "../City/CityDressModel.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Festival/LyudFestival.h"
#include "HAL/IConsoleManager.h"
#include "Plants/GameCalendar.h"
#include "Weather/LyudWeather.h"
#include "World/LyudCityBuilding.h"

namespace
{
	TAutoConsoleVariable<int32> CVarCityDress(TEXT("lyud.City.Dress"), 1,
		TEXT("The town buildings' swappable dressing (docs/city_dress.md): 1 = by the date and the weather, 0 = all hidden, 2 = all shown."),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable*) { ULyudCityDress::MarkAllDirty(); }), ECVF_Default);

	FAutoConsoleCommandWithWorld CmdCityDressReport(TEXT("lyud.City.Dress.Report"),
		TEXT("The town buildings' dressing sets and which are shown now."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (ULyudCityDress* D = ULyudCityDress::Get(World))
			{
				UE_LOG(LogTemp, Display, TEXT("%s"), *D->Report());
			}
		}));

	/** Developer mode (F7, Actions): every set shown, or none, for checks and shots; pressed again: back to the day. */
	void ToggleDressMode(int32 Mode)
	{
		IConsoleVariable* V = CVarCityDress.AsVariable();
		V->Set(V->GetInt() == Mode ? 1 : Mode, ECVF_SetByConsole);
	}
	LyudDev::FAutoAction GDressAll(TEXT("interiors.dress_all"), TEXT("interiors"), TEXT("dev.action.interiors.dress_all"),
		[](UWorld*) { ToggleDressMode(2); }, [](UWorld*) { return CVarCityDress.GetValueOnGameThread() == 2; });
	LyudDev::FAutoAction GDressHide(TEXT("interiors.dress_hide"), TEXT("interiors"), TEXT("dev.action.interiors.dress_hide"),
		[](UWorld*) { ToggleDressMode(0); }, [](UWorld*) { return CVarCityDress.GetValueOnGameThread() == 0; });

	/** Every live subsystem (for the console's mode). Game thread only. */
	TArray<TWeakObjectPtr<ULyudCityDress>>& Live()
	{
		static TArray<TWeakObjectPtr<ULyudCityDress>> List;
		return List;
	}

	/** A model condition from the imported struct. */
	LyudCity::FDressWhen ToModel(const FLyudDressWhen& W)
	{
		LyudCity::FDressWhen C;
		C.WeatherAny = (uint8_t)(W.WeatherAny & 0xFF);
		C.WeatherNone = (uint8_t)(W.WeatherNone & 0xFF);
		C.Months = (uint16_t)(W.Months & 0xFFF);
		C.Weekdays = (uint8_t)(W.Weekdays & 0x7F);
		C.HourFrom = W.HourFrom;
		C.HourTo = W.HourTo;
		C.Festival = std::string(TCHAR_TO_UTF8(*W.Festival));
		C.Chance = W.Chance;
		C.PhasePeriod = W.PhasePeriod;
		C.PhaseFrom = W.PhaseFrom;
		C.PhaseTo = W.PhaseTo;
		C.Open = W.Open;
		return C;
	}

	/** The game's moment as the city things read it (LyudCityThings.cpp When): the date, the clock, the holidays, the weather. */
	LyudCity::FWhen Moment(const UWorld* World)
	{
		const FDateTime Now = UGameCalendar::NowIn(World);
		const UGameCalendar* Cal = World ? World->GetSubsystem<UGameCalendar>() : nullptr;
		const FDateTime Start = Cal ? Cal->StartDate : FDateTime(2026, 10, 20, 16, 0, 0);
		LyudCity::FWhen W;
		W.Day = (long long)(Now.GetDate().GetTicks() / ETimespan::TicksPerDay);
		W.Hour = (float)Now.GetHour() + (float)Now.GetMinute() / 60.f;
		W.Weekday = (int)Now.GetDayOfWeek();     // Monday = 0
		W.MonthDay = Now.GetMonth() * 100 + Now.GetDay();
		W.Since = FMath::Max<long long>(0, W.Day - (long long)(Start.GetDate().GetTicks() / ETimespan::TicksPerDay));
		if (const ULyudFestivalSubsystem* Fest = ULyudFestivalSubsystem::Get(World))
		{
			W.bFestival = Fest->IsFestivalToday();
			for (const FLyudFestivalNow& F : Fest->ActiveFestivals())
			{
				W.Festivals.push_back(std::string(TCHAR_TO_UTF8(*F.Id.ToString())));
			}
		}
		if (const ULyudWeatherSubsystem* Wx = ULyudWeatherSubsystem::Get(World))
		{
			const FLyudSkyWeather Sky = Wx->GetSky();
			W.Weather = LyudCity::WeatherBits(Sky.Cloud, Sky.Rain, Sky.Snow, Sky.Fog, Sky.TempC, Sky.Wet);
		}
		return W;
	}

	/** What decides the sets: the day, the clock's 10-minute step, the weather words, the holidays, the mode. */
	uint64 KeyOf(const LyudCity::FWhen& W, int32 Mode)
	{
		uint64 H = (uint64)W.Day * 1000003ull + (uint64)FMath::FloorToInt(W.Hour * 6.f);
		H = H * 131ull + W.Weather;
		H = H * 3ull + (W.bFestival ? 1u : 0u);
		for (const std::string& F : W.Festivals)
		{
			for (const char C : F)
			{
				H = H * 33ull + (unsigned char)C;
			}
		}
		return H * 7ull + (uint64)Mode;
	}

	/** Shows or hides each set of B for the moment W; returns how many are shown. */
	int32 ApplyTo(ALyudCityBuilding* B, int32 Mode, const LyudCity::FWhen& W)
	{
		const int32 N = B->NumDress();
		if (B->DressComps.Num() != N)
		{
			B->DressComps.SetNum(N);
		}
		const bool bOpen = LyudCity::InHours(W.Hour, B->OpenHour, B->CloseHour);
		const std::string Plot(TCHAR_TO_UTF8(*B->PlotId.ToString()));
		int32 Shown = 0;
		for (int32 I = 0; I < N; ++I)
		{
			bool bShow = Mode == 2;
			if (Mode == 1)
			{
				bShow = LyudCity::DressVisible(ToModel(B->DressWhen[I]), W, Plot, std::string(TCHAR_TO_UTF8(*B->DressIds[I].ToString())), bOpen);
			}
			UStaticMeshComponent* C = B->DressComps[I];
			if (bShow && !C && B->DressMeshes[I])
			{
				// made on first show: in the building's frame like Detail (the same glb origin), no collision, never moved
				C = NewObject<UStaticMeshComponent>(B, *FString::Printf(TEXT("Dress_%s"), *B->DressIds[I].ToString()), RF_Transient);
				C->SetMobility(EComponentMobility::Static);
				C->SetStaticMesh(B->DressMeshes[I]);
				C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				C->SetGenerateOverlapEvents(false);
				C->SetCanEverAffectNavigation(false);
				C->SetAffectDistanceFieldLighting(false);
				C->LightingChannels = B->Detail ? B->Detail->LightingChannels : C->LightingChannels;
				C->SetupAttachment(B->GetRootComponent());
				C->RegisterComponent();
				B->DressComps[I] = C;
			}
			if (C && C->IsVisible() != bShow)
			{
				C->SetVisibility(bShow);
			}
			Shown += (bShow && C) ? 1 : 0;
		}
		return Shown;
	}
}

ULyudCityDress* ULyudCityDress::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULyudCityDress>() : nullptr;
}

void ULyudCityDress::MarkAllDirty()
{
	for (const TWeakObjectPtr<ULyudCityDress>& D : Live())
	{
		if (D.IsValid())
		{
			D->MarkDirty();
		}
	}
}

void ULyudCityDress::Register(ALyudCityBuilding* B)
{
	if (!B)
	{
		return;
	}
	Buildings.AddUnique(B);
	Live().AddUnique(this);
	bDirty = true;
}

void ULyudCityDress::Unregister(ALyudCityBuilding* B)
{
	Buildings.Remove(B);
}

void ULyudCityDress::Deinitialize()
{
	Live().Remove(this);
	Buildings.Reset();
	Super::Deinitialize();
}

TStatId ULyudCityDress::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudCityDress, STATGROUP_Tickables);
}

void ULyudCityDress::Tick(float DeltaTime)
{
	Accum += DeltaTime;
	if (Accum < 2.f && !bDirty)
	{
		return;
	}
	Accum = 0.f;
	const int32 Mode = FMath::Clamp(CVarCityDress.GetValueOnGameThread(), 0, 2);
	const LyudCity::FWhen W = Moment(GetWorld());
	const uint64 Key = KeyOf(W, Mode);
	if (Key == LastKey && !bDirty)
	{
		return;
	}
	LastKey = Key;
	bDirty = false;
	Shown = 0;
	Buildings.RemoveAll([](const TWeakObjectPtr<ALyudCityBuilding>& B) { return !B.IsValid(); });
	for (const TWeakObjectPtr<ALyudCityBuilding>& B : Buildings)
	{
		Shown += ApplyTo(B.Get(), Mode, W);
	}
}

FString ULyudCityDress::Report() const
{
	FString Out = FString::Printf(TEXT("City dressing: %d buildings, %d sets shown, mode %d"), Buildings.Num(), Shown,
		CVarCityDress.GetValueOnGameThread());
	for (const TWeakObjectPtr<ALyudCityBuilding>& WB : Buildings)
	{
		const ALyudCityBuilding* B = WB.Get();
		if (!B)
		{
			continue;
		}
		Out += FString::Printf(TEXT("\n  %s (%s):"), *B->PlotId.ToString(), *B->TypeId);
		for (int32 I = 0; I < B->NumDress(); ++I)
		{
			const UStaticMeshComponent* C = B->DressComps.IsValidIndex(I) ? B->DressComps[I].Get() : nullptr;
			Out += FString::Printf(TEXT(" %s%s"), *B->DressIds[I].ToString(), (C && C->IsVisible()) ? TEXT("*") : TEXT(""));
		}
	}
	return Out;
}
