#include "World/LyudCityDress.h"

#include "Dev/LyudDev.h"

#include "../City/CityDressModel.h"
#include "../People/PeopleSim.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Festival/LyudFestival.h"
#include "HAL/IConsoleManager.h"
#include "People/PeopleLife.h"
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
		C.VisitKeep = W.VisitActs.Num() > 0 ? W.VisitKeep : 0.f;
		return C;
	}

	/** The game's clock as hours since day 0 (FWhen Day * 24 + Hour): when a visit ended, how long ago. */
	double HoursOf(const LyudCity::FWhen& W)
	{
		return (double)W.Day * 24.0 + (double)W.Hour;
	}

	double NowHours(const UWorld* World)
	{
		const FDateTime Now = UGameCalendar::NowIn(World);
		return (double)(Now.GetDate().GetTicks() / ETimespan::TicksPerDay) * 24.0 + (double)Now.GetHour() + (double)Now.GetMinute() / 60.0;
	}

	/** A set's key in ULyudCityDress::ListenOf. */
	uint64 ListenKey(const ALyudCityBuilding* B, int32 I)
	{
		return ((uint64)B->GetUniqueID() << 16) | (uint64)(I & 0xFFFF);
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
	int32 ApplyTo(ALyudCityBuilding* B, int32 Mode, const LyudCity::FWhen& W, const ULyudCityDress& D)
	{
		const double Now = HoursOf(W);
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
				const FLyudDressWhen& When = B->DressWhen[I];
				const float Ago = When.VisitActs.Num() > 0 ? D.VisitAgo(B, I, Now) : -1.f;
				bShow = LyudCity::DressVisible(ToModel(When), W, Plot, std::string(TCHAR_TO_UTF8(*B->DressIds[I].ToString())), bOpen, Ago);
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
	if (!Buildings.Contains(B))
	{
		Buildings.Add(B);
		AddListens(B);
	}
	Live().AddUnique(this);
	bDirty = true;
}

void ULyudCityDress::Unregister(ALyudCityBuilding* B)
{
	Buildings.Remove(B);
	// its listens stay in the table with a dead building (skipped); a building that comes back gets new ones
	for (FVisitListen& V : Listens)
	{
		if (V.B.Get() == B)
		{
			ListenOf.Remove(ListenKey(B, V.Set));
			V.B.Reset();
		}
	}
}

void ULyudCityDress::AddListens(ALyudCityBuilding* B)
{
	const FString Plot = B->PlotId.ToString();
	for (int32 I = 0; I < B->NumDress(); ++I)
	{
		const FLyudDressWhen& W = B->DressWhen[I];
		if (W.VisitActs.Num() == 0 || W.VisitKeep <= 0.f)
		{
			continue;
		}
		FVisitListen V;
		V.B = B;
		V.Set = I;
		V.Plot = Plot;
		V.At = W.VisitAt;
		V.Who = W.VisitWho;
		const int32 L = Listens.Add(MoveTemp(V));
		for (const FName& A : W.VisitActs)
		{
			ListenByAct.Add(A, L);
		}
		ListenOf.Add(ListenKey(B, I), L);
	}
}

void ULyudCityDress::BindLife()
{
	if (bLifeBound || Listens.Num() == 0)
	{
		return;
	}
	if (UPeopleLifeSubsystem* L = UPeopleLifeSubsystem::Get(GetWorld()))
	{
		L->OnPersonEvent.AddUniqueDynamic(this, &ULyudCityDress::OnLifeEvent);
		Life = L;
		bLifeBound = true;
	}
}

void ULyudCityDress::OnLifeEvent(int32 PersonId, FName Event, int32 Other)
{
	static const FName NArrive(TEXT("Arrive"));
	static const FName NLeave(TEXT("Leave"));
	if (Listens.Num() == 0 || (Event != NArrive && Event != NLeave))
	{
		return;
	}
	const UPeopleLifeSubsystem* L = Life.Get();
	const LyudPeople::FWorld* S = L ? L->GetSim() : nullptr;
	if (!S || PersonId < 0 || PersonId >= (int32)S->People.size())
	{
		return;
	}
	const LyudPeople::FPerson& P = S->People[PersonId];
	auto ActName = [S](int32 A) { return A >= 0 && A < (int32)S->Activities.size() ? FName(UTF8_TO_TCHAR(S->Activities[A].Id.c_str())) : NAME_None; };
	if (Event == NArrive)
	{
		// where they are now, for when they leave (by then the life may have moved them on within the same step)
		if (ListenByAct.Contains(ActName(P.Activity)))
		{
			ArrivedAt.Add(PersonId, P.Place);
		}
		return;
	}
	TArray<int32, TInlineAllocator<8>> Hits;
	ListenByAct.MultiFind(ActName(P.PrevActivity), Hits);
	int32 PlaceI = P.Place;
	ArrivedAt.RemoveAndCopyValue(PersonId, PlaceI);
	if (Hits.Num() == 0 || PlaceI < 0 || PlaceI >= (int32)S->Places.size())
	{
		return;
	}
	const LyudPeople::FPlace& Pl = S->Places[PlaceI];
	const FString PlaceId(UTF8_TO_TCHAR(Pl.Id.c_str()));
	const FString Building(UTF8_TO_TCHAR(Pl.Building.c_str()));
	const FString Key(UTF8_TO_TCHAR(P.Key.c_str()));
	const FString Occupation(UTF8_TO_TCHAR(P.Occupation.c_str()));
	const double Now = NowHours(GetWorld());
	for (const int32 I : Hits)
	{
		FVisitListen& V = Listens[I];
		if (!V.B.IsValid())
		{
			continue;
		}
		const bool bHere = V.At.IsEmpty() ? (Building == V.Plot || PlaceId == V.Plot) : PlaceId == V.Plot + V.At;
		if (!bHere || (V.Who.Num() > 0 && !V.Who.Contains(Key) && !V.Who.Contains(Occupation)))
		{
			continue;
		}
		V.LastAt = Now;
		bDirty = true;
	}
}

float ULyudCityDress::VisitAgo(const ALyudCityBuilding* B, int32 I, double Now) const
{
	const int32* L = B ? ListenOf.Find(ListenKey(B, I)) : nullptr;
	if (!L || Listens[*L].LastAt < 0.0)
	{
		return -1.f;
	}
	return (float)FMath::Max(0.0, Now - Listens[*L].LastAt);
}

void ULyudCityDress::Deinitialize()
{
	if (UPeopleLifeSubsystem* L = Life.Get())
	{
		L->OnPersonEvent.RemoveDynamic(this, &ULyudCityDress::OnLifeEvent);
	}
	Live().Remove(this);
	Buildings.Reset();
	Listens.Reset();
	ListenByAct.Reset();
	ListenOf.Reset();
	ArrivedAt.Reset();
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
	BindLife();
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
		Shown += ApplyTo(B.Get(), Mode, W, *this);
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
