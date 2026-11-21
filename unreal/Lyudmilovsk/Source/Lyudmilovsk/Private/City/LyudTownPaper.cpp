#include "City/LyudTownPaper.h"

#include "City/LyudCityThings.h"

#include "TownPaperModel.h"
#include "Birds/BirdBook.h"
#include "Dev/LyudDev.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Festival/LyudFestival.h"
#include "Fishing/LyudFishing.h"
#include "People/LyudTownRumours.h"
#include "HAL/IConsoleManager.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Plants/GameCalendar.h"
#include "Sea/LyudEcosystem.h"
#include "Sea/Tide.h"
#include "Sky/LyudSky.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "Weather/LyudWeather.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudPaper, Log, All);

namespace LyudTownPaperImpl
{
	FString Str(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
	std::string Std(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }

	LyudFest::FDate ToDate(const FDateTime& T) { LyudFest::FDate D; D.Y = T.GetYear(); D.M = T.GetMonth(); D.D = T.GetDay(); return D; }
	bool SameDay(const FDateTime& T, const LyudFest::FDate& D) { return T.GetYear() == D.Y && T.GetMonth() == D.M && T.GetDay() == D.D; }

	FDateTime StartOf(const UWorld* World)
	{
		const UGameCalendar* Cal = World ? World->GetSubsystem<UGameCalendar>() : nullptr;
		return (Cal ? Cal->StartDate : FDateTime(2026, 10, 20, 16, 0, 0)).GetDate();
	}

	/** One line in the language of the moment: its key with the arguments ("@key" a key itself). */
	FText LineText(const LyudPaper::FLine& L)
	{
		TArray<FStringFormatArg> Args;
		for (const std::string& A : L.Args)
		{
			Args.Add(FStringFormatArg(!A.empty() && A[0] == '@' ? LyudText::Get(Str(A.substr(1))).ToString() : Str(A)));
		}
		return LyudText::Get(Str(L.Key), Args);
	}

	FText PageText(const LyudPaper::FPage& Page)
	{
		FString Out;
		for (const LyudPaper::FLine& L : Page)
		{
			if (!Out.IsEmpty()) Out += TEXT("\n\n");
			Out += LineText(L).ToString();
		}
		return FText::FromString(Out);
	}

	/** The fish anglers talk about (LyudF ids, as ULyudEcosystem knows them). */
	const TCHAR* const BiteFish[] = { TEXT("Herring"), TEXT("Perch"), TEXT("Cod"), TEXT("SeaTrout"), TEXT("Garfish"), TEXT("Flounder"),
		TEXT("Turbot"), TEXT("Smelt"), TEXT("Whitefish"), TEXT("Salmon"), TEXT("Roach"), TEXT("Vimba"), TEXT("Eel"), TEXT("RoundGoby") };
}


ULyudTownPaper::ULyudTownPaper() = default;
ULyudTownPaper::~ULyudTownPaper() = default;

ULyudTownPaper* ULyudTownPaper::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudTownPaper>() : nullptr;
}

bool ULyudTownPaper::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULyudTownPaper::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("TownPaper"), this);
		Saves->OnNewGame.AddUObject(this, &ULyudTownPaper::Forget);
	}
}

void ULyudTownPaper::Deinitialize()
{
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(GetWorld()))
	{
		Saves->UnregisterSaveObject(TEXT("TownPaper"));
		Saves->OnNewGame.RemoveAll(this);
	}
	Issues.Reset();
	Super::Deinitialize();
}

void ULyudTownPaper::OnGameSaving_Implementation()
{
	// SavedIssues and ReadDays are kept current as issues are set and read
}

void ULyudTownPaper::OnGameLoaded_Implementation()
{
	Issues.Reset();
	for (const FString& S : SavedIssues)
	{
		TSharedPtr<LyudPaper::FIssue> I = MakeShared<LyudPaper::FIssue>();
		if (LyudPaper::FPaper::Unpack(LyudTownPaperImpl::Std(S), *I))
		{
			Issues.Add(I->Since, I);
		}
	}
	UE_LOG(LogLyudPaper, Display, TEXT("TownPaper: %d issues kept, %d read"), Issues.Num(), ReadDays.Num());
}

void ULyudTownPaper::Forget()
{
	Issues.Reset();
	SavedIssues.Reset();
	ReadDays.Reset();
}

bool ULyudTownPaper::EnsureData()
{
	if (Paper.IsValid())
	{
		return true;
	}
	if (bTriedLoad)
	{
		return false;
	}
	bTriedLoad = true;
	FString Json, Fest;
	const FString Dir = FPaths::ProjectContentDir();
	if (!FFileHelper::LoadFileToString(Json, *(Dir / TEXT("City/town_paper.json"))))
	{
		UE_LOG(LogLyudPaper, Warning, TEXT("TownPaper: Content/City/town_paper.json is missing (tools/paper/paper.py writes it)"));
		return false;
	}
	TPimplPtr<LyudPaper::FPaper> P = MakePimpl<LyudPaper::FPaper>();
	if (!P->Load(LyudTownPaperImpl::Std(Json)))
	{
		UE_LOG(LogLyudPaper, Warning, TEXT("TownPaper: %s"), *LyudTownPaperImpl::Str(P->LastError));
		return false;
	}
	if (!FFileHelper::LoadFileToString(Fest, *(Dir / TEXT("PeopleData/festivals.json"))) || !P->LoadCalendar(LyudTownPaperImpl::Std(Fest)))
	{
		UE_LOG(LogLyudPaper, Warning, TEXT("TownPaper: no festival calendar (%s): the paper prints none"), *LyudTownPaperImpl::Str(P->LastError));
	}
	Paper = MoveTemp(P);
	return true;
}

int32 ULyudTownPaper::Today() const
{
	const UWorld* World = GetWorld();
	const FDateTime Now = UGameCalendar::NowIn(World);
	return FMath::Max(0, (int32)((Now.GetDate() - LyudTownPaperImpl::StartOf(World)).GetDays()));
}

void ULyudTownPaper::BuildFacts(int32 Since, LyudPaper::FFacts& Out) const
{
	UWorld* World = GetWorld();
	const FDateTime Day = LyudTownPaperImpl::StartOf(World) + FTimespan::FromDays(Since);
	Out.Since = Since;
	Out.Date = LyudTownPaperImpl::ToDate(Day);
	const FDateTime Tomorrow = Day + FTimespan::FromDays(1);
	const LyudFest::FDate Yesterday = LyudTownPaperImpl::ToDate(Day - FTimespan::FromDays(1));

	// --- tomorrow's weather: six looks through the day
	if (const ULyudWeatherSubsystem* Wx = ULyudWeatherSubsystem::Get(World))
	{
		LyudPaper::FWeather& W = Out.Tomorrow;
		W.bKnown = true;
		float TMin = 99.f, TMax = -99.f, Wind = -1.f, Cloud = 0.f;
		int32 NThunder = 0, NSnow = 0, NSleet = 0, NRain = 0, NShowers = 0, NDrizzle = 0, NFog = 0;
		for (const int32 H : { 6, 9, 12, 15, 18, 21 })
		{
			const FLyudWeatherState S = Wx->GetStateAt(Tomorrow + FTimespan::FromHours(H));
			TMin = FMath::Min(TMin, S.TempC);
			TMax = FMath::Max(TMax, S.TempC);
			if (S.WindSpeed > Wind)
			{
				Wind = S.WindSpeed;
				W.WindFromDeg = FMath::RoundToInt(S.WindFromDeg);
			}
			Cloud += S.CloudCover / 6.f;
			const bool bFalling = S.PrecipRate > 0.05f;
			switch (S.Precip)
			{
			case ELyudPrecipType::Thunderstorm: case ELyudPrecipType::Hail: NThunder++; break;
			case ELyudPrecipType::Snow: case ELyudPrecipType::SnowShowers: case ELyudPrecipType::Graupel: NSnow += bFalling ? 1 : 0; break;
			case ELyudPrecipType::Sleet: case ELyudPrecipType::FreezingRain: case ELyudPrecipType::FreezingDrizzle: NSleet += bFalling ? 1 : 0; break;
			case ELyudPrecipType::Rain: NRain += bFalling ? 1 : 0; break;
			case ELyudPrecipType::Showers: NShowers += bFalling ? 1 : 0; break;
			case ELyudPrecipType::Drizzle: NDrizzle += bFalling ? 1 : 0; break;
			default: break;
			}
			NThunder += S.Thunder > 0.5f ? 1 : 0;
			NFog += S.VisibilityM < 1000.f ? 1 : 0;
			if (H == 12)
			{
				W.SeaC = FMath::RoundToInt(S.SeaTempC);
			}
		}
		W.TMin = FMath::RoundToInt(TMin);
		W.TMax = FMath::RoundToInt(TMax);
		W.WindMs = FMath::RoundToInt(FMath::Max(0.f, Wind));
		W.Sky = NThunder > 0 ? "thunder" : NSnow >= 2 ? "snow" : NSleet >= 2 ? "sleet" : NRain >= 2 ? "rain" : NShowers >= 2 ? "showers"
			: NDrizzle >= 2 ? "drizzle" : NFog >= 2 ? "fog" : Cloud < 0.15f ? "clear" : Cloud < 0.4f ? "fair" : Cloud < 0.75f ? "cloudy" : "overcast";
		const FLyudWeatherState Night = Wx->GetStateAt(Day + FTimespan::FromHours(23));
		W.bAurora = Night.Aurora > 0.3f && Night.CloudCover < 0.6f;
		// the first snow: snow tomorrow and none fired this winter (the festival calendar's first_snow trigger)
		if (W.Sky == "snow")
		{
			bool bSeen = false;
			if (const ULyudFestivalSubsystem* Fest = ULyudFestivalSubsystem::Get(World))
			{
				for (const FString& F : Fest->FiredTriggers)
				{
					FString Trigger, When;
					FDateTime At;
					if (F.Split(TEXT("|"), &Trigger, &When) && Trigger == TEXT("first_snow") && FDateTime::ParseIso8601(*When, At))
					{
						const double Ago = (Day - At).GetTotalDays();
						bSeen |= Ago >= 0.0 && Ago < 150.0;
					}
				}
			}
			W.bFirstSnow = !bSeen;
		}
	}

	// --- tomorrow's tides: the turns of the water, every 6 minutes from midnight
	double Lon = 20.40, Utc = 2.0;
	if (TActorIterator<ALyudSky> It(World); It)
	{
		Lon = It->LongitudeDeg;
		Utc = It->UtcOffsetHours;
	}
	{
		constexpr int32 Step = 6;
		double Prev2 = 0.0, Prev1 = 0.0;
		for (int32 M = -2 * Step; M <= 24 * 60; M += Step)
		{
			const double L = LyudTide::At(Tomorrow + FTimespan::FromMinutes(M) - FTimespan::FromHours(Utc), Lon).LevelM;
			if (M >= 0 + Step && M - Step < 24 * 60)
			{
				const bool bHigh = Prev1 > Prev2 && Prev1 >= L;
				const bool bLow = Prev1 < Prev2 && Prev1 <= L;
				if (bHigh || bLow)
				{
					LyudPaper::FTide T;
					T.Minute = M - Step;
					T.bHigh = bHigh;
					T.LevelCm = (int)FMath::RoundToInt(Prev1 * 100.0);
					Out.Tides.push_back(T);
				}
			}
			Prev2 = Prev1;
			Prev1 = L;
		}
	}

	// --- what the hero did yesterday (every source is dated)
	if (const ULyudBirdBook* Book = ULyudBirdBook::Get(World))
	{
		TSet<FName> Done;
		for (const FLyudBirdFriend& F : Book->Friends)
		{
			if (LyudTownPaperImpl::SameDay(F.Tamed, Yesterday) && !Done.Contains(F.Species))
			{
				Done.Add(F.Species);
				Out.Deeds.push_back({ "tamed", LyudTownPaperImpl::Std(F.Species.ToString()), 0 });
			}
		}
		for (const FLyudBirdSeen& S : Book->Seen)
		{
			if (LyudTownPaperImpl::SameDay(S.When, Yesterday))
			{
				const std::string Id = LyudTownPaperImpl::Std(S.Species.ToString());
				const bool bRare = Paper.IsValid() && std::find(Paper->RareBirds.begin(), Paper->RareBirds.end(), Id) != Paper->RareBirds.end();
				Out.Deeds.push_back({ bRare ? "rare" : "seen", Id, 0 });
			}
		}
	}
	if (ULyudFishingState* Fishing = ULyudFishingState::Get(World))
	{
		const FLyudCatchEntry* Best = nullptr;
		for (const FLyudCatchEntry& E : Fishing->Journal)
		{
			if (LyudTownPaperImpl::SameDay(E.When, Yesterday) && (!Best || E.Grams > Best->Grams))
			{
				Best = &E;
			}
		}
		if (Best)
		{
			const FLyudCatchRecord* R = Fishing->Find(Best->Species);
			const bool bRecord = R && R->Count >= 3 && FMath::Abs(R->BiggestGrams - Best->Grams) < 0.5f;
			Out.Deeds.push_back({ bRecord ? "record" : "catch", LyudTownPaperImpl::Std(Best->Species.ToString()), FMath::RoundToInt(Best->Grams) });
		}
	}
	if (const ULyudQuests* Quests = ULyudQuests::Get(World))
	{
		for (const FLyudJournalEntry& E : Quests->Entries)
		{
			if (!E.Quest.IsNone() && E.Key.EndsWith(TEXT(".done")) && LyudTownPaperImpl::SameDay(E.When, Yesterday))
			{
				Out.Deeds.push_back({ "quest", LyudTownPaperImpl::Std(E.Quest.ToString()), 0 });
			}
		}
	}

	// --- the fish biting best (only known for today: the web's numbers now)
	if (Since == Today())
	{
		if (const ULyudEcosystem* Eco = ULyudEcosystem::Get(World))
		{
			float Best = 1.15f;
			for (const TCHAR* F : LyudTownPaperImpl::BiteFish)
			{
				const float A = Eco->GetAbundance(FName(F));
				if (A > Best)
				{
					Best = A;
					Out.BiteFish = LyudTownPaperImpl::Std(F);
				}
			}
		}
	}

	// --- the clubs' tournaments: yesterday's results, today's to come (ULyudCityThings, CityCupsModel.h)
	if (const ULyudCityThings* Things = ULyudCityThings::Get(World))
	{
		for (int32 Back = 1; Back >= 0; --Back)
		{
			TArray<FLyudCupNews> News;
			Things->GetCupNews(Since - Back, News);
			for (const FLyudCupNews& N : News)
			{
				if (Back == 1 && !N.bOver)
				{
					continue;
				}
				LyudPaper::FCup C;
				C.Title = LyudTownPaperImpl::Std(N.Title);
				C.Venue = LyudTownPaperImpl::Std(N.Venue);
				C.Winner = LyudTownPaperImpl::Std(N.Winner);
				C.RunnerUp = LyudTownPaperImpl::Std(N.RunnerUp);
				C.StartMinute = N.StartMinute;
				C.HeroReached = N.HeroReached;
				C.bOver = Back == 1;
				(Back == 1 ? Out.CupsYesterday : Out.CupsToday).push_back(MoveTemp(C));
			}
		}
	}

	// --- the talk of the town about the hero yesterday (ULyudTownRumours: the column «Говорят в городе»)
	if (const ULyudTownRumours* Rumours = ULyudTownRumours::Get(World))
	{
		TArray<TPair<FString, TArray<FString>>> Lines;
		Rumours->PaperColumn(ULyudTownRumours::DayOf(Day), Lines);
		for (const TPair<FString, TArray<FString>>& L : Lines)
		{
			LyudPaper::FLine Line;
			Line.Key = LyudTownPaperImpl::Std(L.Key);
			for (const FString& A : L.Value)
			{
				Line.Args.push_back(LyudTownPaperImpl::Std(A));
			}
			Out.Talk.push_back(MoveTemp(Line));
		}
	}

	if (const TSharedPtr<LyudPaper::FIssue>* Prev = Issues.Find(Since - 1))
	{
		Out.PrevFront = (*Prev)->Front;
	}
}

const LyudPaper::FIssue* ULyudTownPaper::IssueOf(int32 Since)
{
	if (const TSharedPtr<LyudPaper::FIssue>* Kept = Issues.Find(Since))
	{
		return Kept->Get();
	}
	if (!EnsureData())
	{
		return nullptr;
	}
	// the missing days before it first (up to MaxBackfill back), so the archive has no holes and no front repeats
	int32 From = Since;
	while (From > 0 && Since - From < MaxBackfill && !Issues.Contains(From - 1))
	{
		--From;
	}
	for (int32 D = From; D <= Since; ++D)
	{
		if (Issues.Contains(D))
		{
			continue;
		}
		LyudPaper::FFacts Facts;
		BuildFacts(D, Facts);
		TSharedPtr<LyudPaper::FIssue> I = MakeShared<LyudPaper::FIssue>(Paper->Build(Facts));
		Issues.Add(D, I);
		UE_LOG(LogLyudPaper, Display, TEXT("TownPaper: issue %d for day %d set (front %s, %d deeds)"), I->Number, D, *LyudTownPaperImpl::Str(I->Front), (int32)Facts.Deeds.size());
	}
	// keep the newest MaxKept, packed for the save
	TArray<int32> Days;
	Issues.GetKeys(Days);
	Days.Sort();
	while (Days.Num() > MaxKept)
	{
		Issues.Remove(Days[0]);
		Days.RemoveAt(0);
	}
	SavedIssues.Reset(Days.Num());
	for (const int32 D : Days)
	{
		SavedIssues.Add(LyudTownPaperImpl::Str(LyudPaper::FPaper::Pack(*Issues[D])));
	}
	const TSharedPtr<LyudPaper::FIssue>* Now = Issues.Find(Since);
	return Now ? Now->Get() : nullptr;
}

bool ULyudTownPaper::Pages(const FString& Gen, TArray<FText>& Out)
{
	Out.Reset();
	const int32 Day = Today();
	if (Gen == TEXT("town_paper"))
	{
		const LyudPaper::FIssue* I = IssueOf(Day);
		if (!I)
		{
			return false;
		}
		for (const LyudPaper::FPage& P : I->Pages)
		{
			Out.Add(LyudTownPaperImpl::PageText(P));
		}
		return Out.Num() > 0;
	}
	if (Gen == TEXT("paper_archive"))
	{
		if (!IssueOf(Day))
		{
			return false;
		}
		TArray<int32> Days;
		Issues.GetKeys(Days);
		Days.Sort([](int32 A, int32 B) { return A > B; });
		int32 NRead = 0;
		for (const int32 D : Days)
		{
			NRead += ReadDays.Contains(D) ? 1 : 0;
		}
		Out.Add(LyudText::Get(TEXT("paper.archive.head"), { FStringFormatArg(Days.Num()), FStringFormatArg(NRead) }));
		for (const int32 D : Days)
		{
			if (Out.Num() >= 12)
			{
				break;
			}
			const LyudPaper::FIssue& I = *Issues[D];
			LyudPaper::FPage P;
			P.push_back(LyudPaper::FPaper::Masthead(I));
			if (const LyudPaper::FLine* Front = LyudPaper::FPaper::FrontLine(I))
			{
				P.push_back(*Front);
			}
			P.push_back({ ReadDays.Contains(D) ? "paper.archive.read" : "paper.archive.unread", {} });
			Out.Add(LyudTownPaperImpl::PageText(P));
		}
		return true;
	}
	return false;
}

void ULyudTownPaper::FinishedReading(const FString& Gen)
{
	if (Gen != TEXT("town_paper"))
	{
		return;
	}
	const int32 Day = Today();
	if (ReadDays.Contains(Day) || !Issues.Contains(Day))
	{
		return;
	}
	ReadDays.Add(Day);
	TArray<int32> Kept;
	Issues.GetKeys(Kept);
	ReadDays.RemoveAll([&Kept](int32 D) { return !Kept.Contains(D); });
	if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
	{
		Screen->Toast(LyudText::Get(TEXT("paper.read"), { FStringFormatArg(ReadDays.Num()), FStringFormatArg(Issues.Num()) }));
	}
	ULyudQuests::Notify(this, TEXT("read"), TEXT("paper_issue"));
	UE_LOG(LogLyudPaper, Display, TEXT("TownPaper: issue of day %d read (%d of %d kept)"), Day, ReadDays.Num(), Issues.Num());
}

FString ULyudTownPaper::IssueText(int32 DaysAhead)
{
	const LyudPaper::FIssue* I = IssueOf(Today() + FMath::Max(0, DaysAhead));
	if (!I)
	{
		return TEXT("(no paper: Content/City/town_paper.json missing)");
	}
	FString Out;
	for (const LyudPaper::FPage& P : I->Pages)
	{
		Out += TEXT("---\n") + LyudTownPaperImpl::PageText(P).ToString() + TEXT("\n");
	}
	return Out;
}

// --- console and developer mode

static FAutoConsoleCommandWithWorldAndArgs GPaperPrint(TEXT("lyud.Paper.Print"),
	TEXT("lyud.Paper.Print [days]: today's town paper to the log, or that many days of issues from today (set ahead)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudTownPaper* P = ULyudTownPaper::Get(World))
		{
			const int32 N = Args.Num() > 0 ? FMath::Clamp(FCString::Atoi(*Args[0]), 1, 30) : 1;
			for (int32 D = 0; D < N; ++D)
			{
				UE_LOG(LogLyudPaper, Display, TEXT("TownPaper day +%d:\n%s"), D, *P->IssueText(D));
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GPaperReset(TEXT("lyud.Paper.Reset"), TEXT("lyud.Paper.Reset: forgets every kept issue and read mark"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (ULyudTownPaper* P = ULyudTownPaper::Get(World))
		{
			P->Forget();
		}
	}));

static LyudDev::FAutoAction GPaperDevPrint(TEXT("paper.print"), TEXT("interiors"), TEXT("dev.action.paper.print"), [](UWorld* W)
{
	if (ULyudTownPaper* P = ULyudTownPaper::Get(W))
	{
		for (int32 D = 0; D < 7; ++D)
		{
			UE_LOG(LogLyudPaper, Display, TEXT("TownPaper day +%d:\n%s"), D, *P->IssueText(D));
		}
	}
});

static LyudDev::FAutoAction GPaperDevReset(TEXT("paper.reset"), TEXT("interiors"), TEXT("dev.action.paper.reset"), [](UWorld* W)
{
	if (ULyudTownPaper* P = ULyudTownPaper::Get(W))
	{
		P->Forget();
	}
});
