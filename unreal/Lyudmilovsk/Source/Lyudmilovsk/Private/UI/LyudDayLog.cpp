#include "UI/LyudDayLog.h"

#include "DayLog.h"
#include "Dev/LyudDev.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Plants/GameCalendar.h"
#include "Weather/LyudWeather.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudDayLog, Log, All);

namespace LyudDayLogImpl
{
	FString Str(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
	std::string Std(const FString& S)
	{
		const FTCHARToUTF8 Conv(*S);
		return std::string(Conv.Get(), (size_t)Conv.Length());
	}

	/** Days since 1970-01-01 (the same numbers as the letters' and friends' calendars). */
	int32 DaysOf(const FDateTime& T)
	{
		return (int32)((T.GetDate() - FDateTime(1970, 1, 1)).GetTotalDays());
	}

	FDateTime DateOf(int32 Days)
	{
		return FDateTime(1970, 1, 1) + FTimespan::FromDays(Days);
	}

	const char* PrecipWord(ELyudPrecipType P)
	{
		switch (P)
		{
		case ELyudPrecipType::Drizzle:
		case ELyudPrecipType::FreezingDrizzle: return "drizzle";
		case ELyudPrecipType::Rain:
		case ELyudPrecipType::Showers:
		case ELyudPrecipType::FreezingRain:
		case ELyudPrecipType::Sleet: return "rain";
		case ELyudPrecipType::Thunderstorm:
		case ELyudPrecipType::Hail: return "thunder";
		case ELyudPrecipType::Snow:
		case ELyudPrecipType::SnowShowers:
		case ELyudPrecipType::Graupel: return "snow";
		default: return "";
		}
	}

	/** A line in the current language: a name with no words in it falls back to the topic's plain line. */
	FText LineText(const LyudDayLog::FLine& L)
	{
		TArray<FStringFormatArg> Args;
		for (const std::string& A : L.Args)
		{
			const FString S = Str(A);
			if (S.StartsWith(TEXT("@")))
			{
				const FString Key = S.Mid(1);
				const FText Name = LyudText::Get(Key);
				if (Name.ToString() == Key)
				{
					return L.Fallback.empty() ? FText::GetEmpty() : LyudText::Get(Str(L.Fallback));
				}
				Args.Add(Name.ToString());
			}
			else
			{
				Args.Add(S);
			}
		}
		return LyudText::Get(Str(L.Key), Args);
	}

	ULyudDayLog* FromWorld(UWorld* World)
	{
		return World ? ULyudDayLog::Get(World) : nullptr;
	}

	FAutoConsoleCommandWithWorld CmdReport(TEXT("lyud.DayLog.Report"), TEXT("The diary's day entries (docs/diary.md, Days) in the log."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudDayLog* L = FromWorld(W)) L->Report(); }));
	FAutoConsoleCommandWithWorldAndArgs CmdFill(TEXT("lyud.DayLog.Fill"), TEXT("lyud.DayLog.Fill [days=14]: made-up days before today, to look at the diary's «Дни» pages."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			if (ULyudDayLog* L = FromWorld(W)) L->Fill(Args.Num() ? FMath::Clamp(FCString::Atoi(*Args[0]), 1, 120) : 14);
		}));
	FAutoConsoleCommandWithWorld CmdForget(TEXT("lyud.DayLog.Forget"), TEXT("Forgets every day of the diary's «Дни»."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudDayLog* L = FromWorld(W)) L->Forget(); }));

	LyudDev::FAutoAction DevFill(TEXT("days.fill"), TEXT("days"), TEXT("dev.action.days.fill"), [](UWorld* W) { if (ULyudDayLog* L = FromWorld(W)) L->Fill(14); });
	LyudDev::FAutoAction DevForget(TEXT("days.forget"), TEXT("days"), TEXT("dev.action.days.forget"), [](UWorld* W) { if (ULyudDayLog* L = FromWorld(W)) L->Forget(); });
}


ULyudDayLog::ULyudDayLog() = default;
ULyudDayLog::~ULyudDayLog() = default;

ULyudDayLog* ULyudDayLog::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<ULyudDayLog>() : nullptr;
}

void ULyudDayLog::NoteEvent(const UObject* WorldContext, FName Event, const FString& Target, int32 N)
{
	if (ULyudDayLog* L = Get(WorldContext))
	{
		L->Note(Event, Target, N);
	}
}

void ULyudDayLog::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Model = MakePimpl<LyudDayLog::FModel>();
	FString Text;
	const FString File = FPaths::ProjectContentDir() / TEXT("UI/daylog.json");
	std::string Error;
	if (!FFileHelper::LoadFileToString(Text, *File))
	{
		UE_LOG(LogLyudDayLog, Warning, TEXT("DayLog: %s not found: the diary writes no days"), *File);
	}
	else if (!Model->Data.Load(LyudDayLogImpl::Std(Text), Error))
	{
		UE_LOG(LogLyudDayLog, Warning, TEXT("DayLog: %s"), *LyudDayLogImpl::Str(Error));
	}
	else
	{
		bLoaded = true;
	}
	if (ULyudSaveSubsystem* Saves = Collection.InitializeDependency<ULyudSaveSubsystem>())
	{
		Saves->RegisterSaveObject(TEXT("DayLog"), this);
		Saves->OnNewGame.AddWeakLambda(this, [this]() { Forget(); });
	}
	// the sky once an hour of game time is all it needs; looked at every few seconds
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ULyudDayLog::Tick), 4.f);
}

void ULyudDayLog::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Super::Deinitialize();
}

FDateTime ULyudDayLog::Now() const
{
	const UGameInstance* GI = GetGameInstance();
	return UGameCalendar::NowIn(GI ? GI->GetWorld() : nullptr);
}

bool ULyudDayLog::Tick(float)
{
	const UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (!bLoaded || !World || !World->IsGameWorld())
	{
		return true;
	}
	const ULyudWeatherSubsystem* W = ULyudWeatherSubsystem::Get(World);
	if (!W)
	{
		return true;
	}
	const FLyudWeatherState S = W->GetState();
	LyudDayLog::FSky Sky;
	Sky.Precip = LyudDayLogImpl::PrecipWord(S.Precip);
	Sky.Cloud = S.CloudCover;
	Sky.TempC = S.TempC;
	Sky.Fog = S.Fog;
	Sky.bRainbow = S.Rainbow > 0.3f;
	Sky.bAurora = S.Aurora > 0.3f;
	const FDateTime T = Now();
	Model->Sample(LyudDayLogImpl::DaysOf(T), T.GetHour(), Sky);
	return true;
}

void ULyudDayLog::Note(FName Event, const FString& Target, int32 N)
{
	if (bLoaded)
	{
		Model->Note(LyudDayLogImpl::DaysOf(Now()), LyudDayLogImpl::Std(Event.ToString()), LyudDayLogImpl::Std(Target), N);
	}
}

void ULyudDayLog::Entries(TArray<FEntry>& Out) const
{
	Out.Reset();
	if (!bLoaded)
	{
		return;
	}
	const FDateTime T = Now();
	const int32 Today = LyudDayLogImpl::DaysOf(T);
	for (const LyudDayLog::FEntry& E : Model->Write(Today, T.GetHour()))
	{
		FEntry& O = Out.AddDefaulted_GetRef();
		O.Day = LyudDayLogImpl::DateOf(E.Day);
		O.bToday = E.Day == Today;
		O.Doodle = LyudDayLogImpl::Str(E.Doodle);
		for (const LyudDayLog::FLine& L : E.Lines)
		{
			const FText Line = LyudDayLogImpl::LineText(L);
			if (!Line.IsEmpty())
			{
				O.Lines.Add(Line);
			}
		}
	}
}

void ULyudDayLog::Forget()
{
	if (Model)
	{
		Model->Reset();
	}
	Saved.Reset();
}

void ULyudDayLog::Fill(int32 Days)
{
	if (!bLoaded)
	{
		return;
	}
	// made-up days with real ids (their names resolve), the same every time
	static const char* Birds[] = { "GreatTit", "HerringGull", "Rook", "Starling", "BlueTit", "Robin", "Chaffinch" };
	static const char* Fish[] = { "fish_cod", "fish_herring", "fish_eelpout", "fish_turbot" };
	static const char* Places[] = { "academ", "alley", "beach", "cable_lower", "cable_upper" };
	static const char* Misc[][2] = { { "do", "job_cafe_scoops" }, { "win", "sp_dominoes" }, { "play_tune", "x" }, { "read_book", "x" },
		{ "read", "paper_issue" }, { "letter_sent", "x" }, { "do", "garden_pick" }, { "do", "girma_telescope" }, { "do", "un_chess" },
		{ "kite_flown", "" }, { "bird", "" }, { "film_watched", "chomon_hotel" }, { "talk", "belov_misha" }, { "eat", "apple" } };
	static const ELyudPrecipType Skies[] = { ELyudPrecipType::None, ELyudPrecipType::None, ELyudPrecipType::Drizzle, ELyudPrecipType::Rain,
		ELyudPrecipType::Snow, ELyudPrecipType::Thunderstorm };
	const int32 Today = LyudDayLogImpl::DaysOf(Now());
	FRandomStream R(Today);
	for (int32 D = Today - Days; D < Today; ++D)
	{
		const ELyudPrecipType P = Skies[R.RandRange(0, (int32)UE_ARRAY_COUNT(Skies) - 1)];
		const float Base = R.FRandRange(-5.f, 24.f), Cloud = R.FRand();
		for (int32 H = 9; H < 22; ++H)
		{
			LyudDayLog::FSky S;
			S.Precip = R.FRand() < 0.4f ? LyudDayLogImpl::PrecipWord(P) : "";
			S.Cloud = FMath::Clamp(Cloud + R.FRandRange(-0.2f, 0.2f), 0.f, 1.f);
			S.TempC = Base + R.FRandRange(-2.f, 2.f);
			Model->Sample(D, H, S);
		}
		const int32 N = R.RandRange(0, 7);
		for (int32 E = 0; E < N; ++E)
		{
			switch (R.RandRange(0, 4))
			{
			case 0: Model->Note(D, "bird_seen", Birds[R.RandRange(0, (int32)UE_ARRAY_COUNT(Birds) - 1)]); break;
			case 1: Model->Note(D, "fish_caught", Fish[R.RandRange(0, (int32)UE_ARRAY_COUNT(Fish) - 1)]); break;
			case 2: Model->Note(D, "reach", Places[R.RandRange(0, (int32)UE_ARRAY_COUNT(Places) - 1)]); break;
			default:
			{
				const int32 M = R.RandRange(0, (int32)UE_ARRAY_COUNT(Misc) - 1);
				Model->Note(D, Misc[M][0], Misc[M][1]);
				break;
			}
			}
		}
	}
	UE_LOG(LogLyudDayLog, Display, TEXT("DayLog: %d made-up days before today"), Days);
}

void ULyudDayLog::Report() const
{
	TArray<FEntry> All;
	Entries(All);
	UE_LOG(LogLyudDayLog, Display, TEXT("DayLog: %d entries"), All.Num());
	for (const FEntry& E : All)
	{
		UE_LOG(LogLyudDayLog, Display, TEXT("DAYLOG %s%s [%s]"), *E.Day.ToString(TEXT("%Y-%m-%d")), E.bToday ? TEXT(" (today)") : TEXT(""), *E.Doodle);
		for (const FText& L : E.Lines)
		{
			UE_LOG(LogLyudDayLog, Display, TEXT("DAYLOG    %s"), *L.ToString());
		}
	}
}

void ULyudDayLog::OnGameSaving_Implementation()
{
	if (!Model)
	{
		return;
	}
	std::map<std::string, std::string> Out;
	Model->Save(Out);
	Saved.Reset();
	for (const std::pair<const std::string, std::string>& KV : Out)
	{
		Saved.Add(LyudDayLogImpl::Str(KV.first), LyudDayLogImpl::Str(KV.second));
	}
}

void ULyudDayLog::OnGameLoaded_Implementation()
{
	if (!Model)
	{
		return;
	}
	std::map<std::string, std::string> In;
	for (const TPair<FString, FString>& KV : Saved)
	{
		In[LyudDayLogImpl::Std(KV.Key)] = LyudDayLogImpl::Std(KV.Value);
	}
	Model->LoadState(In);
}
