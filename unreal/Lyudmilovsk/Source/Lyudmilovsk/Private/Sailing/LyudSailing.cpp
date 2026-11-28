// The yacht school's subsystem: its life, the clock's facts every two seconds, the people's conditions, the hero's path
// (joining, lessons, hire, the regattas, the launch that fetches him), the stamps and quest events, the save.
// docs/yacht_school.md "Engine: the school".
#include "Sailing/LyudSailing.h"

#include "LyudSailHud.h"
#include "LyudSailingState.h"

#include "Async/Async.h"
#include "Audio/LyudSoundscape.h"
#include "City/LyudCityThings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Festival/LyudFestival.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NpcDialogue.h"
#include "People/PeopleLife.h"
#include "Plants/GameCalendar.h"
#include "Sailing/LyudDinghy.h"
#include "Sailing/LyudSailNews.h"
#include "Sea/LyudSea.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "Weather/LyudWeather.h"
#include "Weather/LyudWind.h"

#include <sstream>

DEFINE_LOG_CATEGORY_STATIC(LogLyudSailing, Log, All);

using namespace LyudSail;

namespace LyudSailingMain
{
	TAutoConsoleVariable<int32> CVarSail(TEXT("lyud.Sail"), 1, TEXT("The yacht school (docs/yacht_school.md): 0 off (the boats stowed, nothing ticks)."));

	/** The sea's own depth for the rules and the helmsmen, metres (the data's profile when there is no sea). */
	double SeaDepth(void* User, double X, double Y)
	{
		const FLyudSailingState* S = static_cast<const FLyudSailingState*>(User);
		if (const ALyudSea* Sea = S->Sea.Get())
		{
			return Sea->DepthAt(FVector(X * 100.0, Y * 100.0, 0.0)) * 0.01;
		}
		return SchoolDepthAt(S->P, FSchoolV2(X, Y), S->W.TideM);
	}

	/** The date the rules are asked about: the calendar's, or, with the season forced, a day of the same weekday in July
	 *  (open) or in January (laid up). */
	FSchoolDate DateFor(const FSchoolParams& P, const FSchoolDate& Real, int32 Forced)
	{
		if (Forced < 0 || (Forced == 1) == SeasonOpen(P, Real))
		{
			return Real;
		}
		FSchoolDate D(Real.Y, Forced == 1 ? 7 : 1, 8);
		const int Wd = SchoolWeekday(Real);
		for (int I = 0; I < 7 && SchoolWeekday(D) != Wd; I++)
		{
			D = SchoolAddDays(D, 1);
		}
		return D;
	}

	FString Clock(double Hour)
	{
		const int32 Min = FMath::Clamp(FMath::RoundToInt32(Hour * 60.0), 0, 24 * 60 - 1);
		return FString::Printf(TEXT("%02d:%02d"), Min / 60, Min % 60);
	}

	/** The collection's pieces by their full ids, each written out (tools/city/interiors.py reads this file for them). */
	const TCHAR* StampId(const std::string& Piece)
	{
		struct FId { const char* Piece; const TCHAR* Id; };
		static const FId Ids[] = {
			{ "joined", TEXT("sailing/joined") },
			{ "first_sail", TEXT("sailing/first_sail") },
			{ "capsize", TEXT("sailing/capsize") },
			{ "certificate", TEXT("sailing/certificate") },
			{ "helm2", TEXT("sailing/helm2") },
			{ "helm1", TEXT("sailing/helm1") },
			{ "first_race", TEXT("sailing/first_race") },
			{ "race_win", TEXT("sailing/race_win") },
			{ "opening", TEXT("sailing/opening") },
			{ "closing", TEXT("sailing/closing") },
			{ "cup_lyudmilovsk", TEXT("sailing/cup_lyudmilovsk") },
			{ "miles_10", TEXT("sailing/miles_10") },
			{ "miles_100", TEXT("sailing/miles_100") },
			{ "recall", TEXT("sailing/recall") },
		};
		for (const FId& It : Ids)
		{
			if (Piece == It.Piece)
			{
				return It.Id;
			}
		}
		return nullptr;
	}

	FSchoolWeather WeatherOf(const FLyudWeatherState& St, double TideM, bool bDaylight)
	{
		FSchoolWeather W;
		W.Wind = St.WindSpeed;
		W.Gust = St.WindSpeed * (1.0 + St.WindGustiness);
		W.VisibilityM = St.VisibilityM;
		W.TideM = TideM;
		W.WaterC = St.SeaTempC;
		W.bThunder = St.Thunder > 0.3f;
		W.bDaylight = bDaylight;
		W.bSeaIce = St.SeaIceCover > 0.3f;
		return W;
	}

	FName ReasonName(ESchoolReason R)
	{
		switch (R)
		{
		case ESchoolReason::Calm: return TEXT("calm");
		case ESchoolReason::Wind: return TEXT("wind");
		case ESchoolReason::Gusts: return TEXT("gust");
		case ESchoolReason::Thunder: return TEXT("thunder");
		case ESchoolReason::Fog: return TEXT("fog");
		case ESchoolReason::Dark: return TEXT("dark");
		case ESchoolReason::Ice: return TEXT("ice");
		case ESchoolReason::LowWater: return TEXT("low_water");
		default: return NAME_None;
		}
	}
}

// ------------------------------------------------------------------ life

ULyudSailing::ULyudSailing() = default;
ULyudSailing::~ULyudSailing() = default;

ULyudSailing* ULyudSailing::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudSailing>() : nullptr;
}

void ULyudSailing::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	M = MakePimpl<FLyudSailingState>();
	bLoaded = LoadData();
	if (!bLoaded)
	{
		return;
	}
	M->Brain.Init(M->P, 0x5A11B0A7u);
	M->Brain.DepthFn = &LyudSailingMain::SeaDepth;
	M->Brain.DepthUser = M.Get();
	// the polars take a moment to build: on a worker, long before the first boat needs them
	M->PolarJob = Async(EAsyncExecution::ThreadPool, []() { LyudSail::WarmPolars(); });
	Book = NewObject<ULyudSailingBook>(this);
	Book->Sailing = this;
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("Sailing"), Book);
		Saves->OnNewGame.AddUObject(this, &ULyudSailing::StartOver);
	}
	if (ULyudCityThings* Things = ULyudCityThings::Get(&InWorld))
	{
		Things->AddActHook(ULyudCityThings::FActHook::CreateUObject(this, &ULyudSailing::OnCityAct));
	}
	LyudSetSailNewsProviders(
		[](const UObject* Ctx, int32 DayNumber, TArray<FLyudSailNewsItem>& Out)
		{
			if (const ULyudSailing* S = ULyudSailing::Get(Ctx))
			{
				S->NewsOfDay(DayNumber, Out);
			}
		},
		[](const UObject* Ctx, int32 DayNumber, FLyudSailMorning& Out)
		{
			const ULyudSailing* S = ULyudSailing::Get(Ctx);
			return S && S->MorningOf(DayNumber, Out);
		});
}

bool ULyudSailing::LoadData()
{
	const FString Path = FPaths::ProjectContentDir() / TEXT("City/sailing.json");
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogLyudSailing, Display, TEXT("Sailing: no %s: no yacht school"), *Path);
		return false;
	}
	std::string Error;
	if (!M->P.Load(LyudSailingImpl::Std(Text), Error))
	{
		UE_LOG(LogLyudSailing, Warning, TEXT("Sailing: %s: %s"), *Path, *LyudSailingImpl::Str(Error));
		return false;
	}
	return true;
}

void ULyudSailing::StartOver()
{
	if (!M || !bLoaded)
	{
		return;
	}
	if (ALyudDinghy* Boat = HeroDinghy())
	{
		Boat->Leave(true);
	}
	M->Journal = FSchoolLog();
	M->Hero = FLyudSailHero();
	M->Learned.clear();
	M->States.clear();
	M->Decisions.clear();
	M->Race = FLyudSailRaceRun();
	M->ResolvedDay = -1;
	if (M->HeroHull >= 0)
	{
		M->Hulls[(size_t)M->HeroHull].bAfloat = false;
		M->Hulls[(size_t)M->HeroHull].Shown = -1;
	}
	if (!M->Brain.Units.empty())
	{
		M->Brain.Units[0].Stage = EFleetStage::Ashore;
	}
	ClearPrompt();
}

void ULyudSailing::Deinitialize()
{
	LyudSetSailNewsProviders(FLyudSailNewsProvider(), FLyudSailMorningProvider());
	if (UWorld* World = GetWorld())
	{
		if (ULyudCityThings* Things = ULyudCityThings::Get(World))
		{
			Things->RemoveActHooks(this);
		}
	}
	ClearPrompt();
	if (HalyardLoop)
	{
		ULyudSoundscapeSubsystem::ReleaseLoop(HalyardLoop, 0.f);
		HalyardLoop = nullptr;
	}
	Super::Deinitialize();
}

void ULyudSailing::SwitchOff()
{
	if (ALyudDinghy* Boat = HeroDinghy())
	{
		Boat->Leave(true);
	}
	M->Hero.bBoatOut = false;
	M->Hero.bLesson = false;
	StowFleet();
	ShowSite(false);
	ClearPrompt();
	if (UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this))
	{
		for (const TCHAR* C : { TEXT("sail_season"), TEXT("sail_kids"), TEXT("sail_juniors"), TEXT("sail_adults"), TEXT("sail_race"), TEXT("sail_shore"),
			TEXT("sail_fitout"), TEXT("sail_prizes"), TEXT("sail_watch") })
		{
			People->SetCondition(FName(C), false);
		}
		People->ClearGathering(TEXT("sail_watch"));
	}
}

void ULyudSailing::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!M || !bLoaded)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (LyudSailingMain::CVarSail.GetValueOnGameThread() == 0)
	{
		if (!bSwitchedOff)
		{
			bSwitchedOff = true;
			SwitchOff();
		}
		return;
	}
	if (bSwitchedOff)
	{
		bSwitchedOff = false;
		FactsIn = 0.f;
		ConditionsIn = 0.f;
	}
	const float Dt = FMath::Min(DeltaTime, 0.1f);
	FactsIn -= DeltaTime;
	if (FactsIn <= 0.f)
	{
		FactsIn = 2.f;
		RefreshFacts();
	}
	if (!M->bFacts)
	{
		return;
	}
	ConditionsIn -= DeltaTime;
	if (ConditionsIn <= 0.f)
	{
		ConditionsIn = 10.f;
		PushConditions();
	}
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	const FVector Eye = Hero ? Hero->GetActorLocation() : FVector(1.0e9, 1.0e9, 0.0);
	TickSite(Eye, Dt);
	// the water is the school's while the hero is within a mile of the slip (or out in his boat, wherever that takes him)
	const FVector Slip = LyudSailingImpl::CmOf(M->P.LaunchOrigin, Eye.Z);
	const bool bNear = FVector::DistSquared2D(Eye, Slip) < FMath::Square(M->bWaterWatched ? 165000.0 : 150000.0) || M->Hero.bBoatOut;
	TickFleet(Eye, Dt, bNear);
	TickHero(Hero, Dt);
}

// ------------------------------------------------------------------ the clock's facts

void ULyudSailing::RefreshFacts()
{
	UWorld* World = GetWorld();
	FLyudSailingState& S = *M;
	const FSchoolParams& P = S.P;
	const FDateTime NowT = UGameCalendar::NowIn(World);
	const FSchoolDate Date = LyudSailingMain::DateFor(P, FSchoolDate(NowT.GetYear(), NowT.GetMonth(), NowT.GetDay()), S.ForcedSeason);
	const long long DayN = SchoolDayNumber(Date);
	S.Hour = NowT.GetTimeOfDay().GetTotalHours();
	const long long PrevDay = S.Day;
	S.Date = Date;
	S.Day = DayN;
	S.Season = SeasonStateOn(P, Date);
	const ULyudFestivalSubsystem* Fest = ULyudFestivalSubsystem::Get(this);
	S.bDayOff = Fest && Fest->IsDayOff();
	S.Sea = ALyudSea::Get(this);

	// the weather over the water where the session of the hour sails (else over the race area)
	const FSchoolDaylight Light = SchoolDaylight(P, Date);
	const bool bLight = S.Hour >= Light.Sunrise - 0.1 && S.Hour <= Light.Sunset + 0.1;
	const ALyudSea* Sea = S.Sea.Get();
	const double TideM = Sea ? Sea->TideOffsetCm() * 0.01 : 0.0;
	if (const ULyudWeatherSubsystem* Weather = ULyudWeatherSubsystem::Get(this))
	{
		S.W = LyudSailingMain::WeatherOf(Weather->GetState(), TideM, bLight);
	}
	else
	{
		S.W = FSchoolWeather();
		S.W.TideM = TideM;
		S.W.bDaylight = bLight;
	}
	if (const ULyudWindSubsystem* Wind = ULyudWindSubsystem::Get(this))
	{
		const FSchoolArea* Area = P.Area(S.Now.bOn && !S.Now.Area.empty() ? S.Now.Area : std::string("race"));
		const FSchoolV2 Mid = Area ? SchoolCentroid(*Area) : S.Brain.CorridorEnd;
		const FLyudWindSample Smp = Wind->GetWindAt(LyudSailingImpl::CmOf(Mid, Sea ? Sea->SeaLevel() : 0.f), 10.f, true);
		const double Mean = Smp.Gust > 0.2f ? Smp.Speed / Smp.Gust : Wind->GetMeanSpeed();
		S.W.Wind = Mean;
		S.W.Gust = Mean * (1.0 + Wind->GetGustiness());
		S.WindFromDeg = Wind->GetFromDegrees();
		S.WindTo = SchoolWindTo(S.WindFromDeg);
	}
	S.Lp = SchoolLaunchPointSampled(P, [&S](const FSchoolV2& At) { return LyudSailingMain::SeaDepth(&S, At.X, At.Y); });
	S.Brain.SetWeather(S.WindTo, S.W.Wind, S.W.TideM, S.W.WaterC);

	// a new day: yesterday's racing is closed, the mending day mends, today's decisions are still to make
	if (PrevDay != DayN)
	{
		for (size_t I = S.States.size(); I-- > 0;)
		{
			if (S.States[I].Key / 16 != DayN)
			{
				S.States.erase(S.States.begin() + (std::ptrdiff_t)I);
			}
		}
		S.Decisions.clear();
		if (PrevDay >= 0)
		{
			DayTurned(PrevDay);
		}
		SchoolMend(P, S.Journal, Date);
	}
	if (S.HeroInDay >= 0)
	{
		S.Race.bHeroIn = S.Race.bHeroIn || S.HeroInDay == DayN;
		S.HeroInDay = -1;
	}

	// the session of the hour; its go / no-go is decided once, when it begins
	S.Now = SchoolSessionAt(P, Date, S.Hour, S.bDayOff, &S.States, &S.W);
	if (S.Now.bOn)
	{
		bool bKnown = false;
		for (const FSchoolSessionState& St : S.States)
		{
			bKnown = bKnown || St.Key == S.Now.Key;
		}
		const bool bWaterKind = S.Now.Session.bAfloat && S.Now.Session.Classes != 0;
		if (!bKnown && bWaterKind)
		{
			const FSchoolGo Go = SchoolDecide(P, Date, S.W, S.Now.Session.Classes);
			FSchoolSessionState St;
			St.Key = S.Now.Key;
			St.Go = Go.Go;
			St.Reason = Go.Reason;
			S.States.push_back(St);
			S.Decisions[St.Key] = Go;
			S.Now = SchoolSessionAt(P, Date, S.Hour, S.bDayOff, &S.States, &S.W);
			UE_LOG(LogLyudSailing, Display, TEXT("SAIL session %s %s: %s (%s), wind %.1f gust %.1f"), *LyudSailingImpl::Str(S.Now.Group), *LyudSailingImpl::Str(S.Now.Kind),
				Go.Go == ESchoolGo::Sail ? TEXT("afloat") : TEXT("ashore"), *LyudSailingImpl::Str(SchoolReasonId(Go.Reason)), S.W.Wind, S.W.Gust);
		}
		else if (bKnown && bWaterKind && S.Decisions.find(S.Now.Key) == S.Decisions.end())
		{
			// a loaded game: the decision itself is not saved, only what it came to
			S.Decisions[S.Now.Key] = SchoolDecide(P, Date, S.W, S.Now.Session.Classes);
		}
		// afloat: must they come in?
		for (FSchoolSessionState& St : S.States)
		{
			if (St.Key != S.Now.Key || St.Go != ESchoolGo::Sail || St.RecallHour >= 0.0 || !S.Now.bAfloat)
			{
				continue;
			}
			const auto It = S.Decisions.find(St.Key);
			if (It == S.Decisions.end())
			{
				continue;
			}
			const ESchoolReason Why = SchoolMustRecall(P, S.W, It->second);
			if (Why != ESchoolReason::None)
			{
				St.RecallHour = S.Hour;
				St.Reason = Why;
				S.Now = SchoolSessionAt(P, Date, S.Hour, S.bDayOff, &S.States, &S.W);
				UE_LOG(LogLyudSailing, Display, TEXT("SAIL recall: %s"), *LyudSailingImpl::Str(SchoolReasonId(Why)));
			}
		}
	}

	// a race day's end when it was not all sailed in front of the player
	const FRaceDay Rd = RaceDayOn(P, Date);
	if (Rd.Event >= 0 && S.ResolvedDay != DayN && S.WaterKey == 0)
	{
		double Last = -1.0;
		for (const FTimetableSession& Plan : SchoolSessionsOn(P, Date, S.bDayOff))
		{
			if (Plan.Kind == "race")
			{
				Last = std::max(Last, Plan.bAfloat ? Plan.WaterTo : Plan.To);
			}
		}
		if (Last >= 0.0 && S.Hour >= Last)
		{
			S.ResolvedDay = DayN;
			FSchoolEvents Ev;
			SchoolResolveDay(P, S.Journal, Date, S.W, S.WindTo, S.bDayOff, &Ev);
			Emit(Ev);
		}
	}
	const FSchoolSignal Sig = SchoolSignalAt(P, Date, S.Hour, S.W, S.bDayOff, &S.States);
	std::string Joined = Sig.State;
	for (const std::string& F : Sig.Flags)
	{
		Joined += ' ';
		Joined += F;
	}
	S.SignalState = Joined;
	S.bFacts = true;
}

void ULyudSailing::DayTurned(long long PrevDay)
{
	FLyudSailingState& S = *M;
	const FSchoolDate Prev = SchoolDateOf(PrevDay);
	if (RaceDayOn(S.P, Prev).Event >= 0 && S.ResolvedDay != PrevDay)
	{
		// the hero slept through it, or was away: by the numbers, with the weather of this morning standing in
		S.ResolvedDay = PrevDay;
		FSchoolWeather Wr = S.W;
		Wr.bDaylight = true;
		FSchoolEvents Ev;
		SchoolResolveDay(S.P, S.Journal, Prev, Wr, S.WindTo, false, &Ev);
		Emit(Ev);
	}
	S.Race = FLyudSailRaceRun();
	S.Hero.bLeftElsewhere = S.Hero.bLeftElsewhere && S.Hero.bBoatOut;
}

void ULyudSailing::PushConditions()
{
	FLyudSailingState& S = *M;
	UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	if (!People)
	{
		return;
	}
	const FSchoolConditions C = SchoolConditionsAt(S.P, S.Date, S.Hour, S.bDayOff, S.W.bSeaIce, &S.States);
	People->SetCondition(TEXT("sail_season"), C.bSeason);
	People->SetCondition(TEXT("sail_kids"), C.bKids);
	People->SetCondition(TEXT("sail_juniors"), C.bJuniors);
	People->SetCondition(TEXT("sail_adults"), C.bAdults);
	People->SetCondition(TEXT("sail_race"), C.bRace);
	People->SetCondition(TEXT("sail_shore"), C.bShore);
	People->SetCondition(TEXT("sail_fitout"), C.bFitOut);
	People->SetCondition(TEXT("sail_prizes"), S.Now.bOn && S.Now.Phase == "prizes");
	People->SetCondition(TEXT("sail_watch"), SchoolSpectatorPull(S.P, S.Date, S.Hour, S.bDayOff) > 0.0 && C.bRace && !C.bShore);
	RaceWatchers();
}

// ------------------------------------------------------------------ what happened to the hero

void ULyudSailing::Say(const TCHAR* Key)
{
	if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
	{
		Screen->Toast(LyudText::Get(Key));
	}
}

void ULyudSailing::SayWith(const TCHAR* Key, const FString& Arg)
{
	if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
	{
		Screen->Toast(LyudText::Get(Key, { FStringFormatArg(Arg) }));
	}
}

void ULyudSailing::Emit(const FSchoolEvents& Ev)
{
	if (Ev.Stamps.empty() && Ev.Quest.empty() && Ev.NewRank < 0)
	{
		return;
	}
	if (ULyudCityThings* Things = ULyudCityThings::Get(this))
	{
		for (const std::string& Piece : Ev.Stamps)
		{
			if (const TCHAR* Id = LyudSailingMain::StampId(Piece))
			{
				Things->GiveStamp(Id);
			}
			else
			{
				UE_LOG(LogLyudSailing, Warning, TEXT("Sailing: stamp %s is not one of the collection's"), *LyudSailingImpl::Str(Piece));
			}
		}
	}
	for (const std::string& Q : Ev.Quest)
	{
		// "sail <what>": the event and its target
		const FString Target = LyudSailingImpl::Str(Q.rfind("sail ", 0) == 0 ? Q.substr(5) : Q);
		ULyudQuests::Notify(this, TEXT("sail"), Target);
	}
	if (Ev.NewRank > 0 && Ev.NewRank < (int)M->P.Ranks.size())
	{
		const FString RankKey = FString(TEXT("sail.rank.")) + LyudSailingImpl::Str(M->P.Ranks[(size_t)Ev.NewRank].Id);
		SayWith(TEXT("sail.rank.new"), LyudText::Get(RankKey).ToString());
	}
}

bool ULyudSailing::HeroHasJacket() const
{
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	return Inv && Inv->Count(TEXT("life_jacket")) > 0 && Inv->IsWorn(TEXT("life_jacket"));
}

bool ULyudSailing::NightSailable() const
{
	const FLyudSailingState& S = *M;
	const int Helm2 = S.P.Rank("helm2");
	const bool bLeave = S.Hero.bNightLeave || (Helm2 > 0 && S.Journal.Rank >= Helm2);
	return bLeave && SeasonOpen(S.P, S.Date, S.W.bSeaIce) && !S.W.bThunder && S.W.VisibilityM > 4000.0 && S.W.Wind >= 1.0 && S.W.Wind <= 6.0
		&& S.W.Gust <= 8.5 && S.Lp.bOk;
}

bool ULyudSailing::OnCityAct(const FCityActInfo& Info, FText& Line)
{
	if (!M || !bLoaded || !M->bFacts || bSwitchedOff)
	{
		return false;
	}
	FString Act = Info.Act;
	int32 AtSign = INDEX_NONE;
	if (Act.FindChar(TEXT('@'), AtSign))
	{
		Act.LeftInline(AtSign);
	}
	if (!Act.StartsWith(TEXT("sail_")))
	{
		return false;
	}
	FLyudSailingState& S = *M;
	const FSchoolParams& P = S.P;
	ULyudInventory* Inv = ULyudInventory::Get(this);
	const int32 Money = Inv ? Inv->Rubles : 0;
	auto Reason = [&S](ESchoolReason R)
	{
		const FString Key = FString(TEXT("sail.reason.")) + LyudSailingImpl::Str(SchoolReasonId(R));
		return LyudText::Get(Key, { FStringFormatArg(FString::Printf(TEXT("%.0f"), R == ESchoolReason::Gusts ? S.W.Gust : S.W.Wind)) });
	};
	if (Act == TEXT("sail_join"))
	{
		if (S.Journal.bMember)
		{
			Line = LyudText::Get(TEXT("sail.join.already"));
		}
		else if (!SchoolCanJoin(P, S.Journal, Money) || !Inv || !Inv->Spend(P.JoinFee))
		{
			Line = LyudText::Get(TEXT("sail.join.no_money"), { FStringFormatArg(P.JoinFee) });
		}
		else
		{
			const FSchoolEvents Ev = SchoolJoin(P, S.Journal, S.Date);
			Inv->Add(LyudSailingImpl::Name(P.CardItem));
			Line = LyudText::Get(TEXT("sail.join.ok"));
			Emit(Ev);
		}
		return true;
	}
	if (Act == TEXT("sail_jacket"))
	{
		if (!Inv)
		{
			return false;
		}
		if (Inv->Count(TEXT("life_jacket")) > 0)
		{
			if (S.Hero.bAboard)
			{
				Line = LyudText::Get(TEXT("sail.jacket.need"));
				return true;
			}
			Inv->Remove(TEXT("life_jacket"));
			Line = LyudText::Get(TEXT("sail.jacket.returned"));
		}
		else
		{
			Inv->Add(TEXT("life_jacket"));
			Inv->Wear(TEXT("life_jacket"));
			Line = LyudText::Get(TEXT("sail.jacket.taken"));
		}
		return true;
	}
	if (Act == TEXT("sail_lesson"))
	{
		if (S.Hero.bLesson)
		{
			// asked again: the lesson's own word for where he is
			const FString Id = LyudSailingImpl::Str(P.Lessons[(size_t)S.Hero.Lesson.Lesson].Id);
			Line = LyudText::Get(FString::Printf(TEXT("sail.lesson.%s.hint%d"), *Id, S.Hero.Lesson.Active + 1));
			return true;
		}
		const int Next = SchoolNextLesson(P, S.Journal);
		const int Lesson = Next >= 0 ? Next : (int)P.Lessons.size() - 1;
		const FLessonGate Gate = LessonAvailable(P, S.Journal, Lesson, S.Date, S.Hour, S.W, S.bDayOff);
		if (!Gate.bOk && !(Gate.Why == "done_before" && Lesson >= 0))
		{
			Line = Gate.Why == "weather" && Gate.Reason != ESchoolReason::None ? Reason(Gate.Reason)
				: LyudText::Get(FString(TEXT("sail.lesson.gate.")) + LyudSailingImpl::Str(Gate.Why.empty() ? std::string("weather") : Gate.Why));
			return true;
		}
		if (P.Lessons[(size_t)Lesson].bAfloat && !HeroHasJacket())
		{
			Line = LyudText::Get(TEXT("sail.jacket.need"));
			return true;
		}
		if (P.Lessons[(size_t)Lesson].bAfloat && S.Hero.bLeftElsewhere)
		{
			Line = LyudText::Get(TEXT("sail.hire.left_elsewhere"));
			return true;
		}
		if (!StartLesson(Lesson))
		{
			Line = LyudText::Get(TEXT("sail.lesson.gate.weather"));
			return true;
		}
		Line = LyudText::Get(FString::Printf(TEXT("sail.lesson.%s.intro"), *LyudSailingImpl::Str(P.Lessons[(size_t)Lesson].Id)));
		return true;
	}
	if (Act == TEXT("sail_hire"))
	{
		ESchoolReason Why = ESchoolReason::None;
		if (S.Hero.bBoatOut && !S.Hero.bLeftElsewhere)
		{
			Line = LyudText::Get(TEXT("sail.hire.ok"));
		}
		else if (!SchoolCanHire(P, S.Journal))
		{
			Line = LyudText::Get(TEXT("sail.hire.no_certificate"));
		}
		else if (S.Hero.bLeftElsewhere)
		{
			Line = LyudText::Get(TEXT("sail.hire.left_elsewhere"));
		}
		else if (!SchoolHireOpen(P, S.Date, S.Hour, S.W, S.bDayOff, &Why) && !(Why == ESchoolReason::Dark && NightSailable()))
		{
			Line = Why == ESchoolReason::None || Why == ESchoolReason::DayOff ? LyudText::Get(TEXT("sail.hire.closed")) : Reason(Why);
		}
		else if (!HeroHasJacket())
		{
			Line = LyudText::Get(TEXT("sail.jacket.need"));
		}
		else if (Money < P.HireFee || !Inv)
		{
			Line = LyudText::Get(TEXT("sail.hire.no_money"), { FStringFormatArg(P.HireFee) });
		}
		else if (!PutHeroBoatOut(true))
		{
			Line = LyudText::Get(TEXT("sail.hire.closed"));
		}
		else
		{
			Inv->Spend(P.HireFee);
			S.Hero.bNightOut = !S.W.bDaylight || Why == ESchoolReason::Dark;
			Emit(SchoolHire(P, S.Journal, S.Date));
			Line = LyudText::Get(TEXT("sail.hire.ok"));
		}
		return true;
	}
	if (Act == TEXT("sail_enter"))
	{
		const FRaceDay Rd = RaceDayOn(P, S.Date);
		const bool bToday = Rd.Event >= 0 && P.Events[(size_t)Rd.Event].Classes[P.HeroClass] && S.Race.Stage != 3 && S.ResolvedDay != S.Day
			&& SeasonOpen(P, S.Date, S.W.bSeaIce);
		if (!bToday)
		{
			Line = LyudText::Get(TEXT("sail.race.not_today"));
		}
		else if (S.Race.bHeroIn)
		{
			Line = LyudText::Get(TEXT("sail.race.entered"));
			if (!S.Hero.bBoatOut)
			{
				PutHeroBoatOut(false);
			}
		}
		else if (!SchoolCanHire(P, S.Journal))
		{
			Line = LyudText::Get(TEXT("sail.hire.no_certificate"));
		}
		else if (!SchoolCanEnter(P, S.Journal, Rd.Event, Money) || !Inv)
		{
			Line = LyudText::Get(TEXT("sail.race.no_money"), { FStringFormatArg(P.Events[(size_t)Rd.Event].EntryFee) });
		}
		else if (!HeroHasJacket())
		{
			Line = LyudText::Get(TEXT("sail.jacket.need"));
		}
		else if (S.Hero.bLeftElsewhere || (!S.Hero.bBoatOut && !PutHeroBoatOut(false)))
		{
			Line = LyudText::Get(S.Hero.bLeftElsewhere ? TEXT("sail.hire.left_elsewhere") : TEXT("sail.hire.closed"));
		}
		else
		{
			Inv->Spend(P.Events[(size_t)Rd.Event].EntryFee);
			S.Race.bHeroIn = true;
			Line = LyudText::Get(TEXT("sail.race.entered"));
		}
		return true;
	}
	if (Act == TEXT("sail_knot") || Act == TEXT("sail_parts") || Act == TEXT("sail_rig"))
	{
		if (S.Hero.bLesson && !P.Lessons[(size_t)S.Hero.Lesson.Lesson].bAfloat)
		{
			const int Was = S.Hero.Lesson.Active;
			const std::vector<ELessonEvent> Ev = S.Hero.Lesson.Notify(P, LyudSailingImpl::Std(Act.RightChop(5)));
			for (const ELessonEvent E : Ev)
			{
				if (E == ELessonEvent::LessonDone)
				{
					EndLesson(true);
				}
				else if (E == ELessonEvent::ObjectiveDone && !S.Hero.Lesson.bComplete)
				{
					const FString Id = LyudSailingImpl::Str(P.Lessons[(size_t)S.Hero.Lesson.Lesson].Id);
					Say(*FString::Printf(TEXT("sail.lesson.%s.hint%d"), *Id, Was + 2));
				}
			}
		}
		return false;
	}
	if (Act == TEXT("sail_bell"))
	{
		if (ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this))
		{
			Scape->PlayEffect(TEXT("sail_bell_single"), Info.At, 1.f);
		}
		return false;
	}
	return false;
}

// ------------------------------------------------------------------ the hero's boat

ALyudDinghy* ULyudSailing::HeroDinghy() const
{
	return M && M->HeroHull >= 0 && Dinghies.IsValidIndex(M->HeroHull) ? Dinghies[M->HeroHull].Get() : nullptr;
}

void ULyudSailing::ClearPrompt()
{
	if (M && M->Hero.bPrompt)
	{
		M->Hero.bPrompt = false;
		if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
		{
			UI->SetPrompt(this, FText());
		}
	}
}

bool ULyudSailing::PutHeroBoatOut(bool bHire)
{
	FLyudSailingState& S = *M;
	if (!MakeFleet() || !S.Lp.bOk || S.HeroHull < 0)
	{
		return false;
	}
	ALyudDinghy* Boat = HeroDinghy();
	if (!Boat)
	{
		return false;
	}
	if (S.Hero.bBoatOut)
	{
		return true;
	}
	// held by her painter in thigh-deep water off the trolleys' stop, head to wind
	FSchoolV2 At = S.Lp.At + S.P.LaunchDir * 4.0 + S.P.LaunchDir.Left() * -5.0;
	for (double D = 0.0; D <= 60.0; D += 1.0)
	{
		const FSchoolV2 Try = S.Lp.At + S.P.LaunchDir * D + S.P.LaunchDir.Left() * -5.0;
		if (LyudSailingMain::SeaDepth(&S, Try.X, Try.Y) >= 0.75)
		{
			At = Try;
			break;
		}
	}
	Boat->SetActorHiddenInGame(false);
	Boat->PlaceAt(LyudSailingImpl::V2(At), float(WindFromYaw(S.WindTo.X, S.WindTo.Y)), int32(EState::Moored), 0.f);
	Boat->SetLod(ELyudDinghyLod::Near);
	Boat->SetAssist(false, false, false);
	Boat->SetTutorHints(true);
	Boat->CanLeave = [](const ALyudDinghy& D)
	{
		const FLyudDinghyTelemetry& T = D.Telemetry();
		return T.DepthM < 1.25f && T.SpeedMs < 1.5f;
	};
	FLyudSailHull& Hull = S.Hulls[(size_t)S.HeroHull];
	Hull.bAfloat = true;
	Hull.Shown = -1;
	const bool bLeave = S.Hero.bNightLeave, bLesson = S.Hero.bLesson, bLeft = S.Hero.bLeftElsewhere;
	const FLessonRun Run = S.Hero.Lesson;
	S.Hero = FLyudSailHero();
	S.Hero.bNightLeave = bLeave;
	S.Hero.bLesson = bLesson;
	S.Hero.bLeftElsewhere = bLeft;
	S.Hero.Lesson = Run;
	S.Hero.bBoatOut = true;
	S.Hero.bHired = bHire;
	if (!S.Brain.Units.empty())
	{
		FFleetUnit& U = S.Brain.Units[0];
		U.Stage = EFleetStage::Ashore;
		U.Metres = 0.0;
		U.AfloatS = 0.0;
		U.Capsizes = 0;
		U.Beach = S.Lp.At + S.P.LaunchDir.Left() * -5.0;
	}
	UE_LOG(LogLyudSailing, Display, TEXT("SAIL hero's boat afloat at %.0f, %.0f (%s)"), At.X, At.Y, bHire ? TEXT("hire") : TEXT("lesson or race"));
	return true;
}

void ULyudSailing::HeroAshore(bool bTowedIn)
{
	FLyudSailingState& S = *M;
	if (!S.Hero.bBoatOut)
	{
		return;
	}
	ALyudDinghy* Boat = HeroDinghy();
	if (Boat)
	{
		Boat->Leave(true);
		Boat->SetHudTask(FText(), TArray<FText>(), -1.f, FVector2D::ZeroVector);
	}
	FSchoolEvents Ev;
	if (!S.Brain.Units.empty())
	{
		FFleetUnit& U = S.Brain.Units[0];
		Ev = SchoolAddSailing(S.P, S.Journal, U.Metres, U.AfloatS / 3600.0, U.Capsizes);
		U.Stage = EFleetStage::Ashore;
		U.Metres = 0.0;
		U.AfloatS = 0.0;
		U.Capsizes = 0;
		if (S.Brain.ForceTow == 0)
		{
			S.Brain.ForceTow = -1;
		}
	}
	if (S.Hero.bRecallSaid && !bTowedIn && !S.Hero.bTowAsked)
	{
		Ev.Add(SchoolNoteRecall(S.P, S.Journal));
	}
	const bool bWasAboardOnTow = bTowedIn && S.Hero.bTowed && !S.Hero.bLeftElsewhere;
	if (S.Hero.bLesson && S.P.Lessons[(size_t)S.Hero.Lesson.Lesson].bAfloat)
	{
		EndLesson(false);
	}
	FLyudSailHull& Hull = S.Hulls[(size_t)S.HeroHull];
	Hull.bAfloat = false;
	Hull.Shown = -1;
	const bool bLeave = S.Hero.bNightLeave, bLesson = S.Hero.bLesson, bLeft = S.Hero.bLeftElsewhere && !bTowedIn;
	const FLessonRun Run = S.Hero.Lesson;
	S.Hero = FLyudSailHero();
	S.Hero.bNightLeave = bLeave;
	S.Hero.bLesson = bLesson;
	S.Hero.bLeftElsewhere = bLeft;
	S.Hero.Lesson = Run;
	ClearPrompt();
	ParkHulls(false);
	Emit(Ev);
	if (bWasAboardOnTow)
	{
		Say(TEXT("sail.recall.tow"));
		ULyudQuests::Notify(this, TEXT("sail"), TEXT("towed"));
	}
	else if (bTowedIn)
	{
		Say(TEXT("sail.hire.towed"));
	}
}

void ULyudSailing::TickHero(AHeroCharacter* Hero, float Dt)
{
	FLyudSailingState& S = *M;
	FLyudSailHero& H = S.Hero;
	// a loaded game with the boat out: she is put back where she was and he in her
	if (H.bRestore && bFleetMade)
	{
		H.bRestore = false;
		const FSchoolV2 At = H.RestoreAt;
		const double Yaw = H.RestoreYaw;
		const bool bIn = H.bRestoreAboard, bHire = H.bHired, bLeft = H.bLeftElsewhere, bNight = H.bNightOut;
		H.bBoatOut = false;
		if (PutHeroBoatOut(bHire))
		{
			ALyudDinghy* Boat = HeroDinghy();
			Boat->PlaceAt(LyudSailingImpl::V2(At), float(Yaw), int32(bIn ? EState::Sailing : EState::Moored), 0.f);
			Boat->SetLod(ELyudDinghyLod::Near);
			H.bLeftElsewhere = bLeft;
			H.bNightOut = bNight;
			if (bIn && Hero)
			{
				Boat->Board(Hero, false);
			}
		}
	}
	ALyudDinghy* Boat = HeroDinghy();
	if (!Boat || !H.bBoatOut || S.Brain.Units.empty())
	{
		ClearPrompt();
		return;
	}
	FFleetUnit& U = S.Brain.Units[0];
	const bool bAboard = Boat->IsHeroAboard();
	const FLyudDinghyTelemetry& T = Boat->Telemetry();
	const FSchoolV2 At(T.PosM.X, T.PosM.Y);
	const bool bTowedNow = U.Stage == EFleetStage::Towed;
	if (bAboard != H.bAboard)
	{
		H.bAboard = bAboard;
		if (bAboard)
		{
			if (Boat->Boat().State == EState::Moored)
			{
				Boat->Boat().State = EState::Sailing;
			}
			if (U.Stage == EFleetStage::Ashore)
			{
				U.Stage = EFleetStage::Work;
			}
			H.bLeftElsewhere = false;
			if (S.Brain.ForceTow == 0 && !H.bTowAsked)
			{
				S.Brain.ForceTow = -1;
			}
		}
		else if (!bTowedNow)
		{
			// he stepped out: by the slip the outing is over, anywhere else she is left and the launch must fetch her
			if ((At - S.Lp.At).Len() < 70.0 || Boat->Boat().State == EState::Ashore)
			{
				HeroAshore(false);
				return;
			}
			Boat->Boat().State = EState::Moored;
			H.bLeftElsewhere = true;
			S.Brain.ForceTow = 0;
			H.bTowAsked = true;
			LaunchOut(true);
		}
	}
	H.bTowed = H.bTowed || bTowedNow;
	if (!bAboard)
	{
		// E by the boat puts him aboard
		const APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
		const bool bReach = Hero && PC && !bTowedNow && !Hero->IsSeated() && FVector::DistSquared2D(Hero->GetActorLocation(), Boat->GetActorLocation()) < FMath::Square(330.0);
		ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
		if (bReach && UI && !UI->IsOpen())
		{
			H.bPrompt = true;
			UI->SetPrompt(this, LyudText::Get(TEXT("ui.sail.board")));
			if (ANpcTalkDirector::UsePressed(PC))
			{
				if (!HeroHasJacket())
				{
					Say(TEXT("sail.jacket.need"));
				}
				else if (Boat->Board(Hero, true))
				{
					ClearPrompt();
				}
			}
		}
		else
		{
			ClearPrompt();
		}
		return;
	}
	ClearPrompt();
	H.AfloatS += Dt;

	// the recall, the hire's hours: home, or the launch comes
	bool bHome = S.Brain.bRecall && S.WaterKey != 0;
	if (H.bHired && !H.bNightOut)
	{
		const FSchoolDaylight Light = SchoolDaylight(S.P, S.Date);
		bHome = bHome || S.Hour > Light.Sunset - 0.25 || S.W.bThunder || S.W.Wind > S.P.Wind[S.P.HeroClass][1] + S.P.RecallMargin;
	}
	else if (H.bNightOut)
	{
		bHome = bHome || S.W.bThunder || S.W.Wind > 8.0 || S.Hour > 23.5 || (S.Hour > 3.0 && S.Hour < 4.0);
	}
	for (const FSchoolSessionState& St : S.States)
	{
		bHome = bHome || (St.RecallHour >= 0.0 && S.Hour - St.RecallHour < 0.75);
	}
	const double FromHome = (At - S.Brain.Home).Len();
	if (bHome && !bTowedNow)
	{
		if (!H.bRecallSaid)
		{
			H.bRecallSaid = true;
			Say(TEXT("sail.recall.call"));
		}
		if (FromHome > 70.0)
		{
			H.RecallS += Dt;
		}
		if (H.RecallS > 300.0 && !H.bTowAsked)
		{
			H.bTowAsked = true;
			S.Brain.ForceTow = 0;
			LaunchOut(true);
			Say(TEXT("sail.area.launch"));
		}
	}
	// the school's water has edges
	const bool bInside = SchoolInsideWater(S.P, At) || (At - S.Lp.At).Len() < 160.0;
	H.OutsideS = bInside ? 0.0 : H.OutsideS + Dt;
	if (bInside)
	{
		H.bAreaWarned = false;
	}
	else if (H.OutsideS > 6.0 && !H.bAreaWarned)
	{
		H.bAreaWarned = true;
		Say(TEXT("sail.area.warn"));
	}
	else if (H.OutsideS > 75.0 && !H.bTowAsked)
	{
		H.bTowAsked = true;
		S.Brain.ForceTow = 0;
		LaunchOut(true);
		Say(TEXT("sail.area.launch"));
	}
	// a night sail: three minutes out past the corridor
	if (H.bNightOut && !H.bNightSaid && (At - S.P.LaunchOrigin).Dot(S.P.LaunchDir) > 115.0 && !bTowedNow)
	{
		H.NightS += Dt;
		if (H.NightS >= 180.0)
		{
			H.bNightSaid = true;
			ULyudQuests::Notify(this, TEXT("sail"), TEXT("night"));
		}
	}
	if (H.bLesson && S.P.Lessons[(size_t)H.Lesson.Lesson].bAfloat && !bTowedNow)
	{
		TickLesson(Dt);
	}
	H.HudIn -= Dt;
	if (H.HudIn <= 0.0)
	{
		H.HudIn = 0.5;
		HeroHud();
	}
}

// ------------------------------------------------------------------ lessons

bool ULyudSailing::StartLesson(int32 Lesson)
{
	FLyudSailingState& S = *M;
	if (Lesson < 0 || Lesson >= (int32)S.P.Lessons.size())
	{
		return false;
	}
	const FLessonDef& Def = S.P.Lessons[(size_t)Lesson];
	if (Def.bAfloat && !S.Hero.bBoatOut && !PutHeroBoatOut(false))
	{
		return false;
	}
	S.Hero.bLesson = true;
	S.Hero.Lesson.Begin(S.P, Lesson);
	S.Hero.LessonObjectiveShown = -1;
	S.Hero.LessonRaceT = -1e9;
	if (ALyudDinghy* Boat = Def.bAfloat ? HeroDinghy() : nullptr)
	{
		// the first times out the boat forgives; later she does not
		const bool bFirst = Def.Id == "first_sail", bEarly = bFirst || Def.Id == "tacking" || Def.Id == "points";
		Boat->SetAssist(bFirst, bEarly, bFirst);
		Boat->SetTutorHints(Def.Id != "racing");
	}
	UE_LOG(LogLyudSailing, Display, TEXT("SAIL lesson %s begins"), *LyudSailingImpl::Str(Def.Id));
	return true;
}

void ULyudSailing::EndLesson(bool bDone)
{
	FLyudSailingState& S = *M;
	if (!S.Hero.bLesson)
	{
		return;
	}
	const int32 Lesson = S.Hero.Lesson.Lesson;
	const FString Id = LyudSailingImpl::Str(S.P.Lessons[(size_t)Lesson].Id);
	S.Hero.bLesson = false;
	S.Hero.bLessonCourse = false;
	StowMarks(16, 24);
	if (S.WaterKey == 0 && S.Brain.Mode == EFleetMode::Drill)
	{
		S.Brain.Mode = EFleetMode::Idle;
	}
	if (ALyudDinghy* Boat = HeroDinghy())
	{
		Boat->SetAssist(false, false, false);
		Boat->SetTutorHints(true);
		Boat->SetHudTask(FText(), TArray<FText>(), -1.f, FVector2D::ZeroVector);
	}
	Say(*FString::Printf(TEXT("sail.lesson.%s.%s"), *Id, bDone ? TEXT("done") : TEXT("fail")));
	if (bDone)
	{
		Emit(SchoolCompleteLesson(S.P, S.Journal, Lesson));
	}
}

void ULyudSailing::LessonObjectiveSet()
{
	FLyudSailingState& S = *M;
	FLyudSailHero& H = S.Hero;
	const FLessonDef& Def = S.P.Lessons[(size_t)H.Lesson.Lesson];
	const FLessonObjective* O = H.Lesson.Objective(S.P);
	H.LessonObjectiveShown = H.Lesson.Active;
	H.bHasTarget = false;
	if (!O)
	{
		return;
	}
	const FSchoolArea* Area = S.P.Area(Def.Area.empty() ? std::string("race") : Def.Area);
	const FSchoolV2 Mid = Area ? SchoolCentroid(*Area) : S.Brain.CorridorEnd;
	auto Inside = [Area](const FSchoolV2& V) { return Area ? SchoolNearestInside(*Area, V, 25.0) : V; };
	const FString Id = LyudSailingImpl::Str(Def.Id);
	if (ALyudDinghy* Boat = HeroDinghy())
	{
		Boat->ShowHint(FName(*FString::Printf(TEXT("sail.lesson.%s.hint%d"), *Id, H.Lesson.Active + 1)), 8.f);
	}
	if (O->Kind == "follow")
	{
		// the coach leads in the launch (when no session has it)
		LaunchOut(true);
		if (S.WaterKey == 0)
		{
			S.Brain.SetDrill("follow", Area ? Area->Id : std::string("kids"));
		}
	}
	else if (S.WaterKey == 0 && S.Brain.Mode == EFleetMode::Drill)
	{
		S.Brain.Mode = EFleetMode::Idle;
	}
	if (O->Kind == "course" || O->Kind == "start")
	{
		if (!H.bLessonCourse)
		{
			// one course for the start and the lap after it
			std::string CourseId = O->Course;
			int Laps = O->Laps;
			for (size_t K = (size_t)H.Lesson.Active; CourseId.empty() && K < Def.Objectives.size(); K++)
			{
				CourseId = Def.Objectives[K].Course;
				Laps = Def.Objectives[K].Laps;
			}
			const ERaceCourse Kind = CourseId.empty() ? ERaceCourse::StartLine : RaceCourseKind(CourseId);
			H.LessonCourse = RaceSetCourse(S.P, Kind, S.P.HeroClass, S.WindTo, S.W.Wind, 2, RaceSeed(S.Date, 31, H.Lesson.Lesson, S.P.HeroClass), S.W.TideM,
				Area ? Area->Id : std::string(), Laps);
			H.bLessonCourse = H.LessonCourse.bOk;
			if (H.bLessonCourse)
			{
				StowMarks(16 + LayCourse(16, H.LessonCourse, true), 24);
			}
		}
		if (H.bLessonCourse)
		{
			H.Lesson.SetCourse(H.LessonCourse, O->Kind == "start");
			H.LessonRaceT = O->Kind == "start" ? -90.0 : 0.0;
		}
		else
		{
			// no room for it in today's water: the coach lets it pass
			std::vector<ELessonEvent> Ev;
			H.Lesson.Advance(S.P, Ev);
		}
	}
	else if (O->Kind == "stop" || O->Kind == "pickup")
	{
		H.Target = Inside(Mid + S.WindTo * -40.0);
		H.bHasTarget = true;
		PlaceMark(22, O->Kind == "pickup" ? 3 : 2, H.Target);
	}
	else if (O->Kind == "land")
	{
		H.Target = S.Lp.At + S.P.LaunchDir * 6.0;
		H.bHasTarget = true;
		StowMarks(22, 23);
	}
	else
	{
		StowMarks(22, 23);
	}
}

void ULyudSailing::TickLesson(float Dt)
{
	FLyudSailingState& S = *M;
	FLyudSailHero& H = S.Hero;
	ALyudDinghy* Boat = HeroDinghy();
	if (!Boat || H.Lesson.bComplete)
	{
		return;
	}
	if (H.LessonObjectiveShown != H.Lesson.Active)
	{
		LessonObjectiveSet();
		if (H.Lesson.bComplete)
		{
			EndLesson(true);
			return;
		}
	}
	const FLessonObjective* O = H.Lesson.Objective(S.P);
	const FBoat& B = Boat->Boat();
	const FLyudDinghyTelemetry& Tele = Boat->Telemetry();
	FLessonTelemetry T;
	T.Dt = Dt;
	T.Pos = FSchoolV2(B.X, B.Y);
	T.HeadingDeg = FMath::RadiansToDegrees(B.Yaw);
	T.Speed = B.VX * std::cos(B.Yaw) + B.VY * std::sin(B.Yaw);
	T.TwaDeg = FMath::RadiansToDegrees(Tele.TrueWindAngleRad);
	T.bCapsized = B.State == EState::Capsized || B.State == EState::Turtled;
	if (LaunchBoat && S.bLaunchOut)
	{
		T.bHasLeader = true;
		const FVector2D Lp = LaunchBoat->PosM();
		T.Leader = FSchoolV2(Lp.X, Lp.Y);
	}
	T.bHasTarget = H.bHasTarget;
	T.Target = H.Target;
	if (O && (O->Kind == "start" || O->Kind == "course"))
	{
		H.LessonRaceT += Dt;
		if (O->Kind == "start" && H.LessonRaceT - Dt < 0.0 && H.LessonRaceT >= 0.0)
		{
			if (LaunchBoat)
			{
				LaunchBoat->Sound(3);
			}
			Say(TEXT("sail.race.started"));
		}
	}
	T.RaceT = H.LessonRaceT;
	const int Was = H.Lesson.Active;
	const std::vector<ELessonEvent> Ev = H.Lesson.Step(S.P, T);
	for (const ELessonEvent E : Ev)
	{
		switch (E)
		{
		case ELessonEvent::LessonDone:
			EndLesson(true);
			return;
		case ELessonEvent::ObjectiveDone:
			if (LaunchBoat && S.bLaunchOut)
			{
				LaunchBoat->Sound(0);
			}
			break;
		case ELessonEvent::NextTarget:
		{
			// the coach throws the buoy somewhere else
			const FLessonDef& Def = S.P.Lessons[(size_t)H.Lesson.Lesson];
			const FSchoolArea* Area = S.P.Area(Def.Area.empty() ? std::string("race") : Def.Area);
			const FSchoolV2 Side = S.WindTo.Left() * (((int)H.Lesson.Progress[(size_t)Was] % 2) ? 55.0 : -55.0);
			const FSchoolV2 Next = T.Pos + Side + S.WindTo * -25.0;
			H.Target = Area ? SchoolNearestInside(*Area, Next, 25.0) : Next;
			PlaceMark(22, 3, H.Target);
			break;
		}
		case ELessonEvent::BadStart:
			Say(TEXT("sail.race.ocs"));
			H.Lesson.RestartStart();
			H.LessonRaceT = -60.0;
			break;
		default:
			break;
		}
	}
}

void ULyudSailing::HeroHud()
{
	FLyudSailingState& S = *M;
	FLyudSailHero& H = S.Hero;
	ALyudDinghy* Boat = HeroDinghy();
	if (!Boat || !Boat->IsHeroAboard())
	{
		return;
	}
	if (H.bLesson && S.P.Lessons[(size_t)H.Lesson.Lesson].bAfloat)
	{
		const FLessonDef& Def = S.P.Lessons[(size_t)H.Lesson.Lesson];
		const FString Id = LyudSailingImpl::Str(Def.Id);
		const FLessonObjective* O = H.Lesson.Objective(S.P);
		H.HudLines.Reset();
		FVector2D Mark = FVector2D::ZeroVector;
		float Countdown = -1.f;
		if (O)
		{
			const double Want = FLessonRun::Wanted(*O);
			const double Have = H.Lesson.Active < (int)H.Lesson.Progress.size() ? H.Lesson.Progress[(size_t)H.Lesson.Active] : 0.0;
			H.HudLines.Add(LyudText::Get(FString::Printf(TEXT("sail.lesson.%s.obj%d"), *Id, H.Lesson.Active + 1),
				{ FStringFormatArg(FMath::FloorToInt32(Have)), FStringFormatArg(FMath::RoundToInt32(Want)) }));
			if (H.bHasTarget)
			{
				Mark = LyudSailingImpl::V2(H.Target);
			}
			else if ((O->Kind == "course" || O->Kind == "start") && H.bLessonCourse)
			{
				Mark = LyudSailingImpl::V2(H.LessonCourse.Waypoint(H.Lesson.Tracker.bStarted ? H.Lesson.Tracker.Next : -1));
				if (O->Kind == "start" && H.LessonRaceT < 0.0)
				{
					Countdown = float(-H.LessonRaceT);
				}
			}
			else if (O->Kind == "follow" && LaunchBoat && S.bLaunchOut)
			{
				Mark = LaunchBoat->PosM();
			}
		}
		Boat->SetHudTask(LyudText::Get(FString::Printf(TEXT("sail.lesson.%s.title"), *Id)), H.HudLines, Countdown, Mark);
		return;
	}
	if (S.Race.bHeroIn && S.bWaterRace && S.Race.Stage != 0)
	{
		RaceHud();
		return;
	}
	if (H.bRecallSaid || H.bTowed)
	{
		H.HudLines.Reset();
		H.HudLines.Add(LyudText::Get(TEXT("sail.recall.call")));
		Boat->SetHudTask(FText(), H.HudLines, -1.f, LyudSailingImpl::V2(S.Brain.Home));
		return;
	}
	Boat->SetHudTask(FText(), TArray<FText>(), -1.f, FVector2D::ZeroVector);
}

// ------------------------------------------------------------------ the boards, the news, the report

bool ULyudSailing::BoardPages(const FString& Gen, TArray<FText>& Out) const
{
	if (!M || !bLoaded || !Gen.StartsWith(TEXT("sail:")))
	{
		return false;
	}
	const FLyudSailingState& S = *M;
	const FSchoolParams& P = S.P;
	FString Page;
	auto EventName = [](const std::string& Id) { return LyudText::Get(FString(TEXT("paper.sail.event.")) + LyudSailingImpl::Str(Id)).ToString(); };
	if (Gen == TEXT("sail:notice"))
	{
		Page = LyudText::Get(TEXT("sail.board.notice.title")).ToString() + TEXT("\n");
		int32 Lines = 0;
		const FSchoolDate From = S.bFacts ? S.Date : FSchoolDate();
		for (int I = 0; I < 7 && S.bFacts; I++)
		{
			const FSchoolDate D = SchoolAddDays(From, I);
			for (const FTimetableSession& Plan : SchoolSessionsOn(P, D, I == 0 && S.bDayOff))
			{
				if (Plan.Kind != "race")
				{
					continue;
				}
				const FString When = FString::Printf(TEXT("%02d.%02d"), D.D, D.M);
				Page += TEXT("\n") + LyudText::Get(TEXT("sail.board.notice.line"), { FStringFormatArg(When), FStringFormatArg(EventName(Plan.Event)),
					FStringFormatArg(LyudSailingMain::Clock(Plan.bAfloat ? Plan.WaterFrom : Plan.From)) }).ToString();
				Lines++;
			}
		}
		if (Lines == 0)
		{
			Page += TEXT("\n") + LyudText::Get(TEXT("sail.board.notice.none")).ToString();
		}
	}
	else if (Gen == TEXT("sail:results"))
	{
		Page = LyudText::Get(TEXT("sail.board.results.title")).ToString() + TEXT("\n");
		const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
		int32 Lines = 0;
		for (const FSchoolDayTable& Tab : S.Journal.Days)
		{
			if (Tab.Points.empty())
			{
				continue;
			}
			Page += TEXT("\n") + EventName(Tab.Event) + TEXT(", ") + LyudText::Get(FString(TEXT("paper.sail.class.")) + LyudSailingImpl::Str(SchoolBoatId(Tab.Class))).ToString();
			int32 Place = 0;
			for (const std::pair<std::string, double>& Row : Tab.Points)
			{
				Place++;
				FString Who = LyudSailingImpl::Str(Row.first);
				if (Row.first == "hero")
				{
					Who = LyudSailHud::Word(TEXT("sail.board.hero"), TEXT("Me")).ToString();
				}
				else if (People)
				{
					for (const FSchoolSailor& Sailor : P.Sailors)
					{
						if (Sailor.Slot == Row.first)
						{
							const int32 Id = People->FindPerson(LyudSailingImpl::Str(Sailor.Person));
							if (Id >= 0)
							{
								Who = People->GetDisplayName(Id).ToString();
							}
							break;
						}
					}
				}
				Page += TEXT("\n") + LyudText::Get(TEXT("sail.board.results.line"), { FStringFormatArg(Place), FStringFormatArg(Who),
					FStringFormatArg(FString::SanitizeFloat(Row.second, 0)) }).ToString();
				Lines++;
			}
			Page += TEXT("\n");
		}
		if (Lines == 0)
		{
			Page += TEXT("\n") + LyudText::Get(TEXT("sail.board.results.none")).ToString();
		}
	}
	else
	{
		return false;
	}
	Out.Add(FText::FromString(Page));
	return true;
}

void ULyudSailing::NewsOfDay(int32 DayNumber, TArray<FLyudSailNewsItem>& Out) const
{
	if (!M || !bLoaded)
	{
		return;
	}
	const FLyudSailingState& S = *M;
	const FSchoolDate Date = SchoolDateOf(DayNumber);
	const FRaceDay Rd = RaceDayOn(S.P, Date);
	for (const FSchoolNewsItem& N : SchoolNewsOfDay(S.P, S.Journal, Date))
	{
		if (N.bSeries)
		{
			continue;
		}
		FLyudSailNewsItem Item;
		Item.Event = LyudSailingImpl::Name(N.Event);
		Item.Class = FName(UTF8_TO_TCHAR(SchoolBoatId(N.Class)));
		for (const std::string* Slot : { &N.First, &N.Second, &N.Third })
		{
			if (Slot->empty())
			{
				continue;
			}
			FName Key = NAME_None;
			if (*Slot == "hero")
			{
				Key = TEXT("hero");
			}
			for (const FSchoolSailor& Sailor : S.P.Sailors)
			{
				if (Sailor.Slot == *Slot && !Sailor.Person.empty())
				{
					Key = LyudSailingImpl::Name(Sailor.Person);
				}
			}
			Item.Podium.Add(Key);
		}
		const int32 C = FMath::Clamp(N.Class, 0, SchoolBoats - 1);
		const bool bSeen = S.NewsDay == DayNumber;
		Item.WindMs = float(N.Wind);
		Item.Boats = bSeen && S.NewsBoats[C] > 0 ? S.NewsBoats[C] : (int32)RaceEntries(S.P, N.Class).size();
		Item.Capsizes = bSeen ? S.NewsCapsizes[C] : 0;
		Item.bRecall = bSeen && S.bNewsRecall;
		Item.Day = Rd.Day;
		Item.HeroPlace = N.HeroPlace;
		Item.bAbandoned = N.bAbandoned;
		Out.Add(Item);
	}
}

bool ULyudSailing::MorningOf(int32 DayNumber, FLyudSailMorning& Out) const
{
	if (!M || !bLoaded)
	{
		return false;
	}
	const FLyudSailingState& S = *M;
	const FSchoolParams& P = S.P;
	const FSchoolDate Real = SchoolDateOf(DayNumber);
	const FSchoolDate Date = LyudSailingMain::DateFor(P, Real, S.ForcedSeason);
	const bool bDayOff = S.bFacts && S.Day == SchoolDayNumber(Date) && S.bDayOff;
	Out = FLyudSailMorning();
	Out.Season = int32(SeasonStateOn(P, Date));
	const ULyudWeatherSubsystem* Weather = ULyudWeatherSubsystem::Get(this);
	if (!Weather)
	{
		return true;
	}
	const FSchoolDaylight Light = SchoolDaylight(P, Date);
	std::vector<std::pair<double, FSchoolWeather>> Hours;
	float FirstFrom = 0.f, LastFrom = 0.f, FirstWind = 0.f, LastWind = 0.f, MaxWind = 0.f;
	for (int H = 9; H <= 20; H++)
	{
		const FLyudWeatherState St = Weather->GetStateAt(FDateTime(Real.Y, Real.M, Real.D, H, 0, 0));
		Hours.push_back(std::make_pair((double)H, LyudSailingMain::WeatherOf(St, 0.0, H >= Light.Sunrise && H <= Light.Sunset)));
		if (H == 9)
		{
			FirstFrom = St.WindFromDeg;
			FirstWind = St.WindSpeed;
		}
		if (H == 13)
		{
			Out.FromDeg = St.WindFromDeg;
		}
		LastFrom = St.WindFromDeg;
		LastWind = St.WindSpeed;
		MaxWind = FMath::Max(MaxWind, St.WindSpeed);
	}
	const FSchoolForecast F = SchoolForecastForSailors(P, Date, Hours, bDayOff);
	Out.WindMs = float(F.Wind);
	Out.GustMs = float(F.Gust);
	if (Out.Season != int32(ESeasonState::Open))
	{
		Out.Call = 0;
	}
	else if (F.Go == ESchoolGo::Sail)
	{
		Out.Call = F.bRace ? 3 : (F.bReef ? 2 : 1);
	}
	else
	{
		Out.Call = F.Go == ESchoolGo::Shore ? 4 : (F.Go == ESchoolGo::Storm ? 5 : 6);
		Out.Reason = LyudSailingMain::ReasonName(F.Reason);
	}
	if (!F.Event.empty())
	{
		Out.Event = LyudSailingImpl::Name(F.Event);
		for (const FTimetableSession& Plan : SchoolSessionsOn(P, Date, bDayOff))
		{
			if (Plan.Kind == "race")
			{
				Out.EventTime = LyudSailingMain::Clock(Plan.bAfloat ? Plan.WaterFrom : Plan.From);
				break;
			}
		}
	}
	const float Turn = FMath::FindDeltaAngleDegrees(FirstFrom, LastFrom);
	Out.Shift = LastWind < 0.6f * FirstWind ? 3 : (LastWind > 1.5f * FirstWind ? 4 : (Turn > 25.f ? 1 : (Turn < -25.f ? 2 : 0)));
	Out.bRecallRisk = F.Go == ESchoolGo::Sail && MaxWind > P.Wind[SchoolOptimist][1] + P.RecallMargin;
	if (F.BestHour >= 0.0)
	{
		Out.BestHour = LyudSailingMain::Clock(F.BestHour);
	}
	return true;
}

FString ULyudSailing::Report() const
{
	if (!M || !bLoaded)
	{
		return TEXT("Sailing: no data");
	}
	const FLyudSailingState& S = *M;
	static const TCHAR* const Seasons[] = { TEXT("laid up"), TEXT("fitting out"), TEXT("open"), TEXT("laying up") };
	int32 Afloat = 0;
	for (const FLyudSailHull& H : S.Hulls)
	{
		Afloat += H.bAfloat ? 1 : 0;
	}
	FString Out = FString::Printf(TEXT("Sailing %04d-%02d-%02d %s: season %s%s; wind %.1f gust %.1f from %.0f, tide %.2f m, water %.0f C, launch point %s %.0f m out; "),
		S.Date.Y, S.Date.M, S.Date.D, *LyudSailingMain::Clock(S.Hour), Seasons[FMath::Clamp(int32(S.Season), 0, 3)], S.ForcedSeason >= 0 ? TEXT(" (forced)") : TEXT(""),
		S.W.Wind, S.W.Gust, S.WindFromDeg, S.W.TideM, S.W.WaterC, S.Lp.bOk ? TEXT("ok") : TEXT("none"), S.Lp.D);
	if (S.Now.bOn)
	{
		Out += FString::Printf(TEXT("session %s %s, phase %s%s, drill %s, area %s; "), *LyudSailingImpl::Str(S.Now.Group), *LyudSailingImpl::Str(S.Now.Kind),
			*LyudSailingImpl::Str(S.Now.Phase), S.Now.bAfloat ? TEXT(" (afloat)") : TEXT(""), *LyudSailingImpl::Str(S.Now.Drill), *LyudSailingImpl::Str(S.Now.Area));
	}
	else
	{
		Out += TEXT("no session; ");
	}
	Out += FString::Printf(TEXT("mast [%s]; fleet %s, %d afloat (%d units), water key %lld%s, race stage %d (%d of %d sailed%s); "), *LyudSailingImpl::Str(S.SignalState),
		bFleetMade ? TEXT("made") : TEXT("not made"), Afloat, (int32)S.Brain.Units.size(), S.WaterKey, S.bWaterWatched ? TEXT(" watched") : TEXT(""), S.Race.Stage, S.Race.Sailed,
		S.Race.Wanted, S.Race.bHeroIn ? TEXT(", hero in") : TEXT(""));
	const FSchoolLog& J = S.Journal;
	Out += FString::Printf(TEXT("hero: %s, rank %s, lessons %x, %.1f miles, %.1f h, %d races (%d wins), capsizes %d, wear %.2f%s%s%s"), J.bMember ? TEXT("member") : TEXT("not a member"),
		*LyudSailingImpl::Str(J.Rank >= 0 && J.Rank < (int)S.P.Ranks.size() ? S.P.Ranks[(size_t)J.Rank].Id : std::string("?")), J.LessonsDone, J.Miles(S.P), J.HoursAfloat, J.Races, J.Wins,
		J.Capsizes, J.Wear, S.Hero.bBoatOut ? TEXT(", boat out") : TEXT(""), S.Hero.bLesson ? TEXT(", in a lesson") : TEXT(""), S.Hero.bNightLeave ? TEXT(", night leave") : TEXT(""));
	return Out;
}

// ------------------------------------------------------------------ the save

void ULyudSailing::WriteBook(ULyudSailingBook& Out) const
{
	if (!M || !bLoaded)
	{
		return;
	}
	const FLyudSailingState& S = *M;
	Out.Journal = LyudSailingImpl::Str(S.Journal.Save());
	std::ostringstream O;
	O.precision(9);
	const ALyudDinghy* Boat = HeroDinghy();
	const bool bOut = S.Hero.bBoatOut && Boat != nullptr;
	O << "m1 " << S.ResolvedDay << ' ' << (S.Hero.bNightLeave ? 1 : 0) << ' ' << S.ForcedSeason << ' ' << (bOut ? 1 : 0) << ' ' << (bOut ? Boat->Boat().X : 0.0) << ' '
		<< (bOut ? Boat->Boat().Y : 0.0) << ' ' << (bOut ? Boat->Boat().Yaw : 0.0) << ' ' << (bOut && Boat->IsHeroAboard() ? 1 : 0) << ' ' << (S.Hero.bHired ? 1 : 0) << ' '
		<< (S.Hero.bLeftElsewhere ? 1 : 0) << ' ' << (S.Hero.bNightOut ? 1 : 0) << ' ' << (S.Race.bHeroIn ? S.Day : -1);
	O << ' ' << S.States.size();
	for (const FSchoolSessionState& St : S.States)
	{
		O << ' ' << St.Key << ' ' << (int)St.Go << ' ' << (int)St.Reason << ' ' << St.RecallHour;
	}
	O << ' ' << S.Learned.size();
	for (const std::pair<const std::string, double>& It : S.Learned)
	{
		O << ' ' << It.first << ' ' << It.second;
	}
	Out.Memory = LyudSailingImpl::Str(O.str());
}

void ULyudSailing::ReadBook(const ULyudSailingBook& In)
{
	if (!M || !bLoaded)
	{
		return;
	}
	FLyudSailingState& S = *M;
	if (ALyudDinghy* Boat = HeroDinghy())
	{
		Boat->Leave(true);
	}
	if (S.HeroHull >= 0)
	{
		S.Hulls[(size_t)S.HeroHull].bAfloat = false;
		S.Hulls[(size_t)S.HeroHull].Shown = -1;
	}
	S.Journal = FSchoolLog();
	if (!In.Journal.IsEmpty() && !S.Journal.Load(LyudSailingImpl::Std(In.Journal)))
	{
		UE_LOG(LogLyudSailing, Warning, TEXT("Sailing: the saved log could not be read: a fresh one"));
		S.Journal = FSchoolLog();
	}
	S.Hero = FLyudSailHero();
	S.States.clear();
	S.Decisions.clear();
	S.Learned.clear();
	S.Race = FLyudSailRaceRun();
	S.ResolvedDay = -1;
	S.WaterKey = 0;
	S.Day = -1;
	FactsIn = 0.f;
	std::istringstream I(LyudSailingImpl::Std(In.Memory));
	std::string V;
	if (!(I >> V) || V != "m1")
	{
		return;
	}
	int Leave = 0, Forced = -1, Out = 0, Aboard = 0, Hired = 0, Left = 0, Night = 0;
	long long HeroInDay = -1;
	double X = 0.0, Y = 0.0, Yaw = 0.0;
	size_t N = 0;
	if (!(I >> S.ResolvedDay >> Leave >> Forced >> Out >> X >> Y >> Yaw >> Aboard >> Hired >> Left >> Night >> HeroInDay >> N))
	{
		return;
	}
	S.Hero.bNightLeave = Leave != 0;
	S.ForcedSeason = Forced;
	S.Hero.bHired = Hired != 0;
	S.Hero.bLeftElsewhere = Left != 0;
	S.Hero.bNightOut = Night != 0;
	S.Hero.bRestore = Out != 0;
	S.Hero.bRestoreAboard = Aboard != 0;
	S.Hero.RestoreAt = FSchoolV2(X, Y);
	S.Hero.RestoreYaw = Yaw;
	S.HeroInDay = HeroInDay;
	for (size_t K = 0; K < N && K < 64; K++)
	{
		FSchoolSessionState St;
		int Go = 0, Why = 0;
		if (!(I >> St.Key >> Go >> Why >> St.RecallHour))
		{
			return;
		}
		St.Go = ESchoolGo(FMath::Clamp(Go, 0, (int)ESchoolGo::Sail));
		St.Reason = ESchoolReason(FMath::Clamp(Why, 0, (int)ESchoolReason::DayOff));
		S.States.push_back(St);
	}
	if (!(I >> N))
	{
		return;
	}
	for (size_t K = 0; K < N && K < 256; K++)
	{
		std::string Slot;
		double Val = 0.0;
		if (!(I >> Slot >> Val))
		{
			return;
		}
		S.Learned[Slot] = SchoolClamp(Val, 0.0, 0.12);
	}
}

void ULyudSailingBook::OnGameSaving_Implementation()
{
	if (const ULyudSailing* S = Sailing.Get())
	{
		S->WriteBook(*this);
	}
}

void ULyudSailingBook::OnGameLoaded_Implementation()
{
	if (ULyudSailing* S = Sailing.Get())
	{
		S->ReadBook(*this);
	}
}
