#include "Music/LyudMusic.h"

#include "MusicModel.h"

#include "Audio/LyudSoundscape.h"
#include "City/LyudCityThings.h"
#include "Components/AudioComponent.h"
#include "Dev/LyudDev.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Festival/LyudFestival.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Plants/GameCalendar.h"
#include "Sea/Tide.h"
#include "Sound/SoundBase.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "UI/LyudUIStyle.h"
#include "Weather/LyudWeather.h"
#include "Winter/LyudWinterPlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#if __has_include("City/LyudTownPaper.h")
#include "City/LyudTownPaper.h"
#define LYUD_MUSIC_HAS_PAPER 1
#else
#define LYUD_MUSIC_HAS_PAPER 0
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLyudMusic, Log, All);

namespace LyudMusicImpl
{
	FString Str(const std::string& S) { return UTF8_TO_TCHAR(S.c_str()); }
	std::string Std(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }

	/** How far a record on a player or a jukebox is heard (cm), and the radio's own reach is the data's range_m. */
	constexpr float DiscHeardCm = 1100.f;
	/** A radio or a record on another storey is not ours. */
	constexpr float FloorCm = 350.f;
	/** Four looks a second. */
	constexpr float TickSeconds = 0.25f;

	TArray<FStringFormatArg> Args(const std::vector<LyudMusic::FArg>& In)
	{
		TArray<FStringFormatArg> Out;
		for (const LyudMusic::FArg& A : In)
		{
			Out.Add(FStringFormatArg(A.bKey ? LyudText::Get(Str(A.S)).ToString() : Str(A.S)));
		}
		return Out;
	}

	FText CaptionText(const LyudMusic::FCaption& C)
	{
		return LyudText::Get(Str(C.Key), Args(C.Args));
	}
}

// ------------------------------------------------------------------ the subtitle

class SLyudRadioSubtitle : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudRadioSubtitle) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ULyudMusic>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		using namespace LyudUI;
		TWeakObjectPtr<ULyudMusic> W = InArgs._Owner;
		SetVisibility(EVisibility::HitTestInvisible);
		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0, 0, 0, 96))
			[
				SNew(SBox).WidthOverride(780.f)
				[
					SNew(STextBlock).Font(Font(16)).Justification(ETextJustify::Center).AutoWrapText(true)
					.ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.75f))
					.Text_Lambda([W]() { return W.IsValid() ? W->GetCaption() : FText::GetEmpty(); })
					.ColorAndOpacity_Lambda([W]() { return FSlateColor(FLinearColor(0.86f, 0.9f, 0.82f, W.IsValid() ? W->GetCaptionAlpha() : 0.f)); })
				]
			]
		];
	}
};

// ------------------------------------------------------------------ life

ULyudMusic::ULyudMusic() = default;
ULyudMusic::~ULyudMusic() = default;

ULyudMusic* ULyudMusic::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudMusic>() : nullptr;
}

void ULyudMusic::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Model = MakePimpl<LyudMusic::FModel>();
	Facts = MakePimpl<LyudMusic::FFacts>();
	bLoaded = LoadData();
	if (!bLoaded)
	{
		return;
	}
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("Music"), this);
		Saves->OnNewGame.AddUObject(this, &ULyudMusic::StartOver);
	}
	BindCity();
}

bool ULyudMusic::LoadData()
{
	const FString Path = FPaths::ProjectContentDir() / TEXT("City/music.json");
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogLyudMusic, Display, TEXT("Music: no %s: no records or radio"), *Path);
		return false;
	}
	const FTCHARToUTF8 Utf8(*Text);
	std::string Error;
	if (!Model->Load(std::string(Utf8.Get(), Utf8.Length()), Error))
	{
		UE_LOG(LogLyudMusic, Warning, TEXT("Music: %s: %s"), *Path, *LyudMusicImpl::Str(Error));
		return false;
	}
	UE_LOG(LogLyudMusic, Display, TEXT("Music: %d tracks, %d records, %d segments"), (int32)Model->Tracks.size(), (int32)Model->Records.size(),
		(int32)Model->Schedule.size());
	return true;
}

void ULyudMusic::BindCity()
{
	ULyudCityThings* City = ULyudCityThings::Get(this);
	if (!City || bCityBound)
	{
		return;
	}
	City->ActHook.BindUObject(this, &ULyudMusic::OnCityAct);
	bCityBound = true;
}

void ULyudMusic::Deinitialize()
{
	if (bCityBound)
	{
		if (ULyudCityThings* City = ULyudCityThings::Get(this))
		{
			City->ActHook.Unbind();
		}
		bCityBound = false;
	}
	ShowSubtitle(false);
	if (ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this))
	{
		Scape->SetMusicHold(this, false);
	}
	if (Holder)
	{
		Holder->Destroy();
		Holder = nullptr;
	}
	Disc = nullptr;
	Radio = nullptr;
	Super::Deinitialize();
}

void ULyudMusic::StartOver()
{
	if (Model)
	{
		Model->ResetState();
	}
	Saved.Reset();
	StopAll();
}

void ULyudMusic::OnGameSaving_Implementation()
{
	if (!Model || !bLoaded)
	{
		return;
	}
	Saved.Reset();
	std::map<std::string, std::string> Out;
	Model->SaveState(Out);
	for (const auto& KV : Out)
	{
		Saved.Add(LyudMusicImpl::Str(KV.first), LyudMusicImpl::Str(KV.second));
	}
}

void ULyudMusic::OnGameLoaded_Implementation()
{
	if (!Model || !bLoaded)
	{
		return;
	}
	std::map<std::string, std::string> In;
	for (const TPair<FString, FString>& KV : Saved)
	{
		In[LyudMusicImpl::Std(KV.Key)] = LyudMusicImpl::Std(KV.Value);
	}
	Model->LoadState(In);
	// a record that was on before the load is not on any more
	if (Disc)
	{
		Disc->Stop();
	}
	DiscRecord = -1;
	DiscThing.Reset();
}

// ------------------------------------------------------------------ the clock

long long ULyudMusic::Today() const
{
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	return (long long)(Now.GetDate().GetTicks() / ETimespan::TicksPerDay);
}

double ULyudMusic::SecondsOfDay() const
{
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	return (double)(Now.GetTicks() - Now.GetDate().GetTicks()) / (double)ETimespan::TicksPerSecond;
}

int32 ULyudMusic::Weekday() const
{
	return (int32)UGameCalendar::NowIn(GetWorld()).GetDayOfWeek();     // Monday = 0
}

void ULyudMusic::RefreshFacts(bool bForce)
{
	UWorld* World = GetWorld();
	const double T = World ? World->GetTimeSeconds() : 0.0;
	if (!bForce && T - FactsAt < 20.0)
	{
		return;
	}
	FactsAt = T;
	LyudMusic::FFacts& F = *Facts;
	if (const ULyudWeatherSubsystem* Wx = ULyudWeatherSubsystem::Get(this))
	{
		const FLyudSkyWeather Sky = Wx->GetSky();
		const FLyudWeatherState State = Wx->GetState();
		F.Cloud = Sky.Cloud;
		F.Rain = Sky.Rain;
		F.Snow = Sky.Snow;
		F.Fog = Sky.Fog;
		F.Thunder = Sky.Thunder;
		F.TempC = Sky.TempC;
		F.WindSpeed = State.WindSpeed;
		F.WindFromDeg = State.WindFromDeg;
	}
	const FDateTime Now = UGameCalendar::NowIn(World);
	const int32 Month = Now.GetMonth();
	F.Season = (Month == 12 || Month <= 2) ? 0 : (Month <= 5 ? 1 : (Month <= 8 ? 2 : 3));
	// winter: the rink's state, the walruses' Sunday swim on Saturday's broadcasts, New Year's Day (docs/winter_play.md)
	F.Rink = 0;
	F.RinkCm = 0.f;
	F.Walrus = 0;
	if (const ULyudWinterPlay* Play = ULyudWinterPlay::Get(this))
	{
		F.Rink = Play->RinkReport(F.RinkCm);
		F.RinkCm = FMath::RoundToFloat(F.RinkCm);
		if (Month == 1 && Now.GetDay() == 1)
		{
			F.Walrus = 2;
		}
		else if ((Month == 12 || Month <= 3) && Now.GetDayOfWeek() == EDayOfWeek::Saturday && Now.GetHour() >= 12)
		{
			F.Walrus = 1;
		}
	}
	// the tide (local time is UTC+2, like the people's sun times): rising or falling now, and when it turns
	const double Longitude = 20.40;
	const FDateTime Utc = Now - FTimespan::FromHours(2.0);
	const LyudTide::FState Tide = LyudTide::At(Utc, Longitude);
	F.bTideRising = Tide.bRising;
	F.TideTime.clear();
	double Prev = Tide.LevelM;
	for (int32 K = 1; K <= 14 * 10; ++K)
	{
		const double L = LyudTide::At(Utc + FTimespan::FromMinutes(6.0 * K), Longitude).LevelM;
		if ((Tide.bRising && L < Prev) || (!Tide.bRising && L > Prev))
		{
			const FDateTime Turn = Now + FTimespan::FromMinutes(6.0 * (K - 1));
			char Buf[16];
			std::snprintf(Buf, sizeof Buf, "%02d:%02d", Turn.GetHour(), Turn.GetMinute());
			F.TideTime = Buf;
			break;
		}
		Prev = L;
	}
	// a holiday today: the station's holiday broadcast (docs/festivals.md "The new toys")
	F.Festival.clear();
	if (const ULyudFestivalSubsystem* Fest = ULyudFestivalSubsystem::Get(this))
	{
		const FName FestId = Fest->IsFestivalToday() ? Fest->ActiveFestival() : NAME_None;
		if (!FestId.IsNone())
		{
			F.Festival = "festival." + LyudMusicImpl::Std(FestId.ToString()) + ".name";
		}
	}
	F.PaperHeadline.clear();
#if LYUD_MUSIC_HAS_PAPER
	// the morning news reads the day's front page: its longest line, cut short
	if (ULyudTownPaper* Paper = ULyudTownPaper::Get(this))
	{
		TArray<FText> Pages;
		if (Paper->Pages(TEXT("town_paper"), Pages) && Pages.Num() > 0)
		{
			TArray<FString> Lines;
			Pages[0].ToString().ParseIntoArrayLines(Lines);
			FString Best;
			for (const FString& L : Lines)
			{
				if (L.Len() > Best.Len())
				{
					Best = L;
				}
			}
			if (Best.Len() > 180)
			{
				Best = Best.Left(177) + TEXT("...");
			}
			F.PaperHeadline = LyudMusicImpl::Std(Best);
		}
	}
#endif
}

// ------------------------------------------------------------------ sounds

USoundBase* ULyudMusic::Cue(int32 Track)
{
	if (!Model || Track < 0 || Track >= (int32)Model->Tracks.size())
	{
		return nullptr;
	}
	if (const TObjectPtr<USoundBase>* Found = CueCache.Find(Track))
	{
		return *Found;
	}
	const FString Name = TEXT("MU_") + LyudMusicImpl::Str(Model->Tracks[(size_t)Track].Cue);
	USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/Music/%s.%s"), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	CueCache.Add(Track, S);
	return S;
}

USoundBase* ULyudMusic::Sound(const TCHAR* Path)
{
	if (const TObjectPtr<USoundBase>* Found = SoundCache.Find(Path))
	{
		return *Found;
	}
	USoundBase* S = LoadObject<USoundBase>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	SoundCache.Add(Path, S);
	return S;
}

UAudioComponent* ULyudMusic::MakeAudio(const TCHAR* Name)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	if (!Holder)
	{
		FActorSpawnParameters SP;
		SP.ObjectFlags |= RF_Transient;
		Holder = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SP);
		if (!Holder)
		{
			return nullptr;
		}
	}
	UAudioComponent* A = NewObject<UAudioComponent>(Holder, Name);
	A->bAutoActivate = false;
	A->bAutoDestroy = false;
	A->bIsUISound = false;
	A->bOverrideAttenuation = true;
	A->AttenuationOverrides.bAttenuate = true;
	A->AttenuationOverrides.bSpatialize = true;
	A->AttenuationOverrides.AttenuationShapeExtents = FVector(250.f, 0.f, 0.f);
	A->AttenuationOverrides.FalloffDistance = 900.f;
	A->RegisterComponent();
	return A;
}

float ULyudMusic::MusicLevel() const
{
	const ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this);
	// a record is louder than the ambient cues (which sit at 0.45 of the music volume by default)
	return Scape ? Scape->MasterVolume * FMath::Clamp(Scape->MusicVolume * 1.8f, 0.2f, 1.f) : 0.8f;
}

// ------------------------------------------------------------------ the city's things

TArray<int32> ULyudMusic::OwnedRecords() const
{
	TArray<int32> Out;
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Inv || !Model)
	{
		return Out;
	}
	for (int32 I = 0; I < (int32)Model->Records.size(); ++I)
	{
		if (Inv->Count(FName(*LyudMusicImpl::Str(Model->Records[(size_t)I].Id))) > 0)
		{
			Out.Add(I);
		}
	}
	return Out;
}

void ULyudMusic::Stamp(int32 Record)
{
	if (!Model || Record < 0 || Record >= (int32)Model->Records.size())
	{
		return;
	}
	Model->LastRecord = Record;
	if (ULyudCityThings* City = ULyudCityThings::Get(this))
	{
		City->GiveStamp(TEXT("records/") + LyudMusicImpl::Str(Model->Tracks[(size_t)Model->Records[(size_t)Record].Track].Cue));
	}
	ULyudQuests::Notify(this, TEXT("play"), LyudMusicImpl::Str(Model->Records[(size_t)Record].Id));
}

FText ULyudMusic::Words(const FString& Key, const TArray<FString>& InArgs, const TArray<bool>& bKeys) const
{
	TArray<FStringFormatArg> A;
	for (int32 I = 0; I < InArgs.Num(); ++I)
	{
		A.Add(FStringFormatArg(bKeys.IsValidIndex(I) && bKeys[I] ? LyudText::Get(InArgs[I]).ToString() : InArgs[I]));
	}
	return LyudText::Get(Key, A);
}

bool ULyudMusic::OnCityAct(const FCityActInfo& Info, FText& Line)
{
	if (!Model || !bLoaded)
	{
		return false;
	}
	using namespace LyudMusicImpl;
	const std::string Act = Std(Info.Act);
	const LyudMusic::ERole Role = Model->RoleOf(Act);
	if (Role == LyudMusic::ERole::None)
	{
		return false;
	}
	const bool bReplace = Model->Replaces(Act);
	const FText Own = Line;
	auto Under = [&](const FText& Ours)
	{
		// the act's own words first, ours under them (a resident's radio keeps its story); the generic acts only ours
		Line = (bReplace || Own.IsEmpty()) ? Ours : FText::FromString(Own.ToString() + TEXT("\n\n") + Ours.ToString());
		return true;
	};
	auto TitleOf = [&](int32 Record) { return Str(Model->Tracks[(size_t)Model->Records[(size_t)Record].Track].Title); };
	auto ArtistOf = [&](int32 Record) { return Str(Model->Tracks[(size_t)Model->Records[(size_t)Record].Track].Artist); };
	const long long Day = Today();
	const int32 Minute = (int32)(SecondsOfDay() / 60.0);

	switch (Role)
	{
	case LyudMusic::ERole::Jukebox:
	{
		int32 Rec = Model->RecordOfLine(Std(Info.Line));
		const bool bOwnLine = Rec >= 0;
		if (Rec < 0)
		{
			// a jukebox with words of its own: its records in turn
			std::vector<int> All;
			for (int I = 0; I < (int)Model->Records.size(); ++I)
			{
				All.push_back(I);
			}
			Rec = Model->NextOwned(Std(Info.Thing), All);
		}
		if (Rec < 0)
		{
			return false;
		}
		PlayDisc(Rec, Info.Thing, Info.At);
		Stamp(Rec);
		(void)bOwnLine;
		Line = FText::FromString(Own.ToString() + TEXT("\n\n") + Words(TEXT("music.jukebox.now"), { TitleOf(Rec), ArtistOf(Rec) }, { true, true }).ToString());
		return true;
	}
	case LyudMusic::ERole::Player:
	{
		const TArray<int32> Owned = OwnedRecords();
		std::vector<int> OwnedV(Owned.GetData(), Owned.GetData() + Owned.Num());
		const int32 Rec = Model->NextOwned(Std(Info.Thing), OwnedV);
		if (Rec >= 0)
		{
			PlayDisc(Rec, Info.Thing, Info.At);
			Stamp(Rec);
			FString S = Words(TEXT("music.player.play"), { TitleOf(Rec), ArtistOf(Rec) }, { true, true }).ToString() + TEXT("\n\n")
				+ LyudText::Get(Str(Model->Records[(size_t)Rec].Note)).ToString();
			if (Owned.Num() > 1)
			{
				S += TEXT("\n\n") + Words(TEXT("music.player.more"), { FString::FromInt(Owned.IndexOfByKey(Rec) + 1), FString::FromInt(Owned.Num()) }, {}).ToString();
			}
			Line = FText::FromString(S);
			return true;
		}
		const int32 House = Model->HouseRecord(Std(Info.Thing));
		if (House < 0)
		{
			return false;
		}
		PlayDisc(House, Info.Thing, Info.At);
		Model->LastRecord = House;
		Line = FText::FromString((Own.IsEmpty() ? FString() : Own.ToString() + TEXT("\n\n"))
			+ Words(TEXT("music.player.none"), { TitleOf(House), ArtistOf(House) }, { true, true }).ToString());
		return true;
	}
	case LyudMusic::ERole::Radio:
	{
		const bool bOn = Model->ToggleRadio(Std(Info.Thing), Act, Day, Minute);
		ULyudQuests::Notify(this, TEXT("radio"), bOn ? TEXT("on") : TEXT("off"));
		if (!bOn && RadioThing == Info.Thing && Radio)
		{
			Radio->FadeOut(0.4f, 0.f);
			RadioThing.Reset();
			RadioTrack = -1;
		}
		Timer = TickSeconds;    // look again at once
		if (!bOn)
		{
			return Under(LyudText::Get(TEXT("music.radio.off")));
		}
		RefreshFacts(true);
		return Under(Words(TEXT("music.radio.on"), { Str(Model->StationName), NowOnAir().ToString() }, { true, false }));
	}
	case LyudMusic::ERole::Request:
	{
		if (Model->Request.Valid())
		{
			return Under(LyudText::Get(TEXT("music.req.already")));
		}
		ULyudInventory* Inv = ULyudInventory::Get(this);
		if (!Inv || Inv->Count(TEXT("postcard")) <= 0)
		{
			return Under(LyudText::Get(TEXT("music.req.need_card")));
		}
		if (!Model->PostRequest(Day, Std(Info.Thing)))
		{
			return false;
		}
		Inv->Remove(TEXT("postcard"));
		ULyudQuests::Notify(this, TEXT("radio"), TEXT("request"));
		const FString Ded = Model->Dedications.empty() ? FString() : Str(Model->Dedications[(size_t)Model->Request.Dedication % Model->Dedications.size()]);
		return Under(Words(TEXT("music.req.posted"), { Str(Model->Tracks[(size_t)Model->Request.Track].Title), Ded }, { true, true }));
	}
	default:
		return false;
	}
}

// ------------------------------------------------------------------ the record on a player

void ULyudMusic::PlayDisc(int32 Record, const FString& Thing, const FVector& At)
{
	if (!Model || Record < 0 || Record >= (int32)Model->Records.size())
	{
		return;
	}
	if (!Disc)
	{
		Disc = MakeAudio(TEXT("MusicDisc"));
	}
	const int32 Track = Model->Records[(size_t)Record].Track;
	USoundBase* S = Cue(Track);
	DiscRecord = Record;
	DiscThing = Thing;
	DiscAt = At;
	DiscEnds = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + Model->Tracks[(size_t)Track].Seconds;
	if (!Disc || !S)
	{
		UE_LOG(LogLyudMusic, Display, TEXT("Music: record %s at %s (no sound loaded)"), *LyudMusicImpl::Str(Model->Records[(size_t)Record].Id), *Thing);
		return;
	}
	Disc->Stop();
	Disc->SetWorldLocation(At + FVector(0.f, 0.f, 40.f));
	Disc->SetSound(S);
	Disc->SetVolumeMultiplier(MusicLevel());
	Disc->Play(0.f);
	UE_LOG(LogLyudMusic, Display, TEXT("Music: record %s on %s"), *LyudMusicImpl::Str(Model->Records[(size_t)Record].Id), *Thing);
}

void ULyudMusic::TickDisc(const FVector& Hero)
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (DiscRecord >= 0 && Now >= DiscEnds)
	{
		DiscRecord = -1;
		DiscThing.Reset();
	}
	bDiscHeard = DiscRecord >= 0 && FVector::Dist(Hero, DiscAt) < LyudMusicImpl::DiscHeardCm && FMath::Abs(Hero.Z - DiscAt.Z) < LyudMusicImpl::FloorCm;
	if (Disc && Disc->IsPlaying())
	{
		Disc->SetVolumeMultiplier(MusicLevel());
	}
}

// ------------------------------------------------------------------ the radio

const ULyudMusic::FRadio* ULyudMusic::NearestRadioOn(const FVector& Hero) const
{
	const long long Day = Today();
	const int32 Minute = (int32)(SecondsOfDay() / 60.0);
	const float Range = (float)Model->RangeM * 100.f;
	const FRadio* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	for (const FRadio& R : Radios)
	{
		// the one playing keeps a little more reach, so standing at the edge does not flip it
		const float Reach = R.Id == RadioThing ? Range * 1.3f : Range;
		const float D = FVector::Dist(Hero, R.At);
		if (D < Reach && D < BestD && FMath::Abs(Hero.Z - R.At.Z) < LyudMusicImpl::FloorCm
			&& Model->IsRadioOn(LyudMusicImpl::Std(R.Id), LyudMusicImpl::Std(R.Act), Day, Minute))
		{
			Best = &R;
			BestD = D;
		}
	}
	return Best;
}

FText ULyudMusic::NowOnAir() const
{
	if (!Model)
	{
		return FText::GetEmpty();
	}
	LyudMusic::FAir Air;
	Model->AirAt(Today(), Weekday(), SecondsOfDay(), *Facts, Air);
	using namespace LyudMusicImpl;
	const FString Show = Air.Show.empty() ? FString() : LyudText::Get(Str(Air.Show)).ToString();
	switch (Air.Kind)
	{
	case LyudMusic::EKind::Music:
		if (Air.Play)
		{
			return LyudText::Get(TEXT("music.radio.now.music"), { FStringFormatArg(Show),
				FStringFormatArg(LyudText::Get(Str(Model->Tracks[(size_t)Air.Play->Track].Title)).ToString()),
				FStringFormatArg(LyudText::Get(Str(Model->Tracks[(size_t)Air.Play->Track].Artist)).ToString()) });
		}
		return LyudText::Get(TEXT("music.radio.now.music"), { FStringFormatArg(Show), FStringFormatArg(FString()), FStringFormatArg(FString()) });
	case LyudMusic::EKind::Requests:
		return LyudText::Get(TEXT("music.radio.now.requests"), { FStringFormatArg(Show),
			FStringFormatArg(Air.Play ? LyudText::Get(Str(Model->Tracks[(size_t)Air.Play->Track].Title)).ToString() : FString()) });
	case LyudMusic::EKind::OffAir:
		return LyudText::Get(TEXT("music.radio.now.off_air"));
	default:
		return LyudText::Get(TEXT("music.radio.now.") + Str(LyudMusic::KindName(Air.Kind)));
	}
}

void ULyudMusic::TickRadio(const FVector& Hero, float DeltaTime)
{
	using namespace LyudMusicImpl;
	const FRadio* R = NearestRadioOn(Hero);
	// a record heard nearby wins over the radio
	if (!R || bDiscHeard)
	{
		// once, as it goes quiet: asked again four times a second while the fade runs (it still IsPlaying), each FadeOut
		// started the fade and its stop timer over, so the radio never stopped and kept its voice, nearly silent
		if (Radio && !RadioThing.IsEmpty())
		{
			Radio->FadeOut(0.6f, 0.f);
		}
		RadioThing.Reset();
		RadioTrack = -1;
		bRadioStatic = false;
		bRadioHeard = false;
		return;
	}
	if (!Radio)
	{
		Radio = MakeAudio(TEXT("MusicRadio"));
		if (!Radio)
		{
			return;
		}
	}
	const bool bMoved = R->Id != RadioThing;
	if (bMoved)
	{
		RadioThing = R->Id;
		Radio->SetWorldLocation(R->At + FVector(0.f, 0.f, 30.f));
	}
	bRadioHeard = true;
	RefreshFacts(false);
	const double Sec = SecondsOfDay();
	LyudMusic::FAir Air;
	Model->AirAt(Today(), Weekday(), Sec, *Facts, Air);
	const float Level = MusicLevel();
	if (Air.Kind == LyudMusic::EKind::OffAir || !Air.Play)
	{
		// between stations: the hiss when it has been made (audio/sfx/radio), else quiet
		USoundBase* Static = Air.Kind == LyudMusic::EKind::OffAir ? Sound(TEXT("/Game/Audio/Radio/S_radio_static_01.S_radio_static_01")) : nullptr;
		if (Static && (!bRadioStatic || bMoved || !Radio->IsPlaying()))
		{
			Radio->SetSound(Static);
			Radio->SetVolumeMultiplier(Level * 0.25f);
			Radio->Play(0.f);
			bRadioStatic = true;
		}
		else if (!Static && Radio->IsPlaying() && RadioTrack >= 0)
		{
			Radio->FadeOut(0.5f, 0.f);
		}
		RadioTrack = -1;
	}
	else
	{
		const LyudMusic::FPlay& P = *Air.Play;
		const double Offset = Sec - P.Start + P.From;
		// a jump of the clock (a sleep, a sit with the minutes passing) puts the record out of step: start it again where it is
		const double WorldNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		const bool bOutOfStep = RadioTrack == P.Track && FMath::Abs(RadioOffsetAtPlay + (WorldNow - RadioPlayedAt) - Offset) > 3.0;
		if (bMoved || bRadioStatic || RadioTrack != P.Track || RadioStart != P.Start || !Radio->IsPlaying() || bOutOfStep)
		{
			if (USoundBase* S = Cue(P.Track))
			{
				Radio->SetSound(S);
				Radio->SetVolumeMultiplier(Level * P.Volume);
				Radio->Play((float)FMath::Max(0.0, Offset));
			}
			RadioPlayedAt = WorldNow;
			RadioOffsetAtPlay = Offset;
			RadioTrack = P.Track;
			RadioStart = P.Start;
			bRadioStatic = false;
		}
		else
		{
			Radio->SetVolumeMultiplier(Level * P.Volume);
		}
	}
	// the chime and the time pips, once as their moment passes
	if (LastSfxSec >= 0.0 && Sec > LastSfxSec && Sec - LastSfxSec < 5.0)
	{
		for (const LyudMusic::FSfx& X : Air.Timeline.Sfx)
		{
			if (X.At > LastSfxSec && X.At <= Sec)
			{
				if (ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this))
				{
					Scape->PlayEffect(FName(*Str(X.Sound)), R->At, 0.8f);
				}
			}
		}
	}
	LastSfxSec = Sec;
	// the words on screen
	if (Air.Cap)
	{
		const FString Id = Str(Air.Cap->Key) + FString::Printf(TEXT("@%.0f"), Air.Cap->Start);
		if (Id != CaptionId)
		{
			CaptionId = Id;
			Caption = CaptionText(*Air.Cap);
			ShowSubtitle(true);
		}
		CaptionFor = (float)FMath::Max(0.5, Air.Cap->Start + Air.Cap->Dur - Sec);
		if (Air.Cap->bHeroRequest && HeardRequestDay != (int64)Today())
		{
			HeardRequestDay = (int64)Today();
			Model->RequestsHeard++;
			if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
			{
				Screen->Toast(LyudText::Get(TEXT("music.req.heard")), true);
			}
			ULyudQuests::Notify(this, TEXT("radio"), TEXT("request_heard"));
		}
	}
	(void)DeltaTime;
}

void ULyudMusic::TickRequest()
{
	if (!Model->Request.Valid())
	{
		return;
	}
	// the request show of the day after the card was posted is over: the card is used up (heard or not)
	const long long Day = Today();
	int ShowEnd = 24 * 60;
	for (size_t I = 0; I < Model->Schedule.size(); ++I)
	{
		if (Model->Schedule[I].Kind == LyudMusic::EKind::Requests)
		{
			double S, E;
			Model->SegmentSpan((int)I, Weekday(), false, S, E);
			ShowEnd = (int)(E / 60.0);
			break;
		}
	}
	if ((Day > Model->Request.PostedDay && (int32)(SecondsOfDay() / 60.0) >= ShowEnd) || Day > Model->Request.PostedDay + 1)
	{
		Model->ConsumeRequest(Day);
	}
}

void ULyudMusic::Tick(float DeltaTime)
{
	if (!bLoaded || !Model)
	{
		return;
	}
	// the caption fades on every frame (cheap); the rest four times a second
	CaptionFor -= DeltaTime;
	CaptionAlpha = FMath::FInterpConstantTo(CaptionAlpha, CaptionFor > 0.f && bRadioHeard ? 1.f : 0.f, DeltaTime, 3.f);
	if (CaptionAlpha <= 0.f && SubtitleWidget && CaptionFor <= 0.f)
	{
		ShowSubtitle(false);
		CaptionId.Reset();
	}
	Timer += DeltaTime;
	if (Timer < LyudMusicImpl::TickSeconds)
	{
		return;
	}
	const float Step = Timer;
	Timer = 0.f;
	if (!bCityBound)
	{
		BindCity();
	}
	ULyudCityThings* City = ULyudCityThings::Get(this);
	if (!bRadiosScanned && City && City->NumThings() > 0)
	{
		// the town's radios, once its things are read
		bRadiosScanned = true;
		TArray<FCityThingRef> Found;
		City->GetThingsByAct([this](const FString& Act) { return Model->RoleOf(LyudMusicImpl::Std(Act)) == LyudMusic::ERole::Radio; }, Found);
		for (const FCityThingRef& T : Found)
		{
			FRadio R;
			R.Id = T.Id;
			R.Act = City->ActOfThing(T.Id);
			R.At = T.Location;
			Radios.Add(R);
		}
		UE_LOG(LogLyudMusic, Display, TEXT("Music: %d radios in town"), Radios.Num());
	}
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Pawn)
	{
		return;
	}
	const FVector Hero = Pawn->GetActorLocation();
	TickDisc(Hero);
	TickRadio(Hero, Step);
	TickRequest();
	const bool bHold = bDiscHeard || bRadioHeard;
	if (bHold != bMusicHeld)
	{
		if (ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this))
		{
			Scape->SetMusicHold(this, bHold);
		}
		bMusicHeld = bHold;
	}
}

void ULyudMusic::ShowSubtitle(bool bShow)
{
	if (!GEngine || !GEngine->GameViewport)
	{
		SubtitleWidget.Reset();
		return;
	}
	if (bShow && !SubtitleWidget)
	{
		SubtitleWidget = SNew(SLyudRadioSubtitle).Owner(this);
		GEngine->GameViewport->AddViewportWidgetContent(SubtitleWidget.ToSharedRef(), 57);
	}
	else if (!bShow && SubtitleWidget)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(SubtitleWidget.ToSharedRef());
		SubtitleWidget.Reset();
	}
}

// ------------------------------------------------------------------ console and developer mode

FString ULyudMusic::DescribeAir() const
{
	if (!Model || !bLoaded)
	{
		return TEXT("Music: no data");
	}
	LyudMusic::FAir Air;
	Model->AirAt(Today(), Weekday(), SecondsOfDay(), *Facts, Air);
	using namespace LyudMusicImpl;
	return FString::Printf(TEXT("Music: %s %s track=%s cap=%s | radio=%s heard=%d | record=%s heard=%d | request=%s"),
		*Str(LyudMusic::KindName(Air.Kind)), *Str(Air.Show), Air.Play ? *Str(Model->Tracks[(size_t)Air.Play->Track].Cue) : TEXT("-"),
		Air.Cap ? *CaptionText(*Air.Cap).ToString() : TEXT("-"), RadioThing.IsEmpty() ? TEXT("-") : *RadioThing, bRadioHeard ? 1 : 0,
		DiscRecord >= 0 ? *Str(Model->Records[(size_t)DiscRecord].Id) : TEXT("-"), bDiscHeard ? 1 : 0,
		Model->Request.Valid() ? *Str(Model->Tracks[(size_t)Model->Request.Track].Cue) : TEXT("-"));
}

void ULyudMusic::LogProgram(int32 DaysAhead) const
{
	if (!Model || !bLoaded)
	{
		return;
	}
	using namespace LyudMusicImpl;
	const long long Day = Today() + DaysAhead;
	const int32 Wd = (Weekday() + DaysAhead) % 7;
	int LastSeg = -99;
	FString LastCap;
	LyudMusic::FAir Air;
	for (double S = 0.0; S < 86400.0; S += 2.0)
	{
		Model->AirAt(Day, Wd, S, *Facts, Air);
		if (Air.Segment != LastSeg)
		{
			UE_LOG(LogLyudMusic, Display, TEXT("%s [%s] %s"), *Str(LyudMusic::FModel::Clock(S)), *Str(LyudMusic::KindName(Air.Kind)),
				Air.Show.empty() ? TEXT("") : *LyudText::Get(Str(Air.Show)).ToString());
			LastSeg = Air.Segment;
		}
		if (Air.Cap)
		{
			const FString C = CaptionText(*Air.Cap).ToString();
			if (C != LastCap)
			{
				UE_LOG(LogLyudMusic, Display, TEXT("   %s %s"), *Str(LyudMusic::FModel::Clock(S)), *C);
				LastCap = C;
			}
		}
	}
}

void ULyudMusic::GiveAllRecords()
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Inv || !Model)
	{
		return;
	}
	for (const LyudMusic::FRecord& R : Model->Records)
	{
		const FName Id(*LyudMusicImpl::Str(R.Id));
		if (Inv->Count(Id) <= 0)
		{
			Inv->Add(Id);
		}
	}
}

void ULyudMusic::RequestTonight()
{
	if (!Model)
	{
		return;
	}
	Model->Request = LyudMusic::FRequest();
	const long long Day = Today();
	if (Model->PostRequest(Day - 1, "P-WAVE/records_hits"))
	{
		UE_LOG(LogLyudMusic, Display, TEXT("Music: a request for tonight: %s"), *LyudMusicImpl::Str(Model->Tracks[(size_t)Model->Request.Track].Cue));
	}
	HeardRequestDay = -1;
}

void ULyudMusic::StopAll()
{
	if (Disc)
	{
		Disc->Stop();
	}
	if (Radio)
	{
		Radio->Stop();
	}
	DiscRecord = -1;
	DiscThing.Reset();
	RadioThing.Reset();
	RadioTrack = -1;
	if (Model)
	{
		Model->RadiosOn.clear();
	}
}

namespace
{
	ULyudMusic* MusicOf(UWorld* World) { return World ? World->GetSubsystem<ULyudMusic>() : nullptr; }

	FAutoConsoleCommandWithWorldAndArgs GMusicRadioCmd(TEXT("lyud.Music.Radio"), TEXT("The town radio's air now, to the log (docs/music.md)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* W)
		{
			if (ULyudMusic* M = MusicOf(W)) { UE_LOG(LogLyudMusic, Display, TEXT("%s"), *M->DescribeAir()); }
		}));
	FAutoConsoleCommandWithWorldAndArgs GMusicProgramCmd(TEXT("lyud.Music.Program"), TEXT("lyud.Music.Program [days ahead]: a day of the radio to the log."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			if (ULyudMusic* M = MusicOf(W)) { M->LogProgram(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0); }
		}));
	FAutoConsoleCommandWithWorldAndArgs GMusicGiveCmd(TEXT("lyud.Music.GiveRecords"), TEXT("Every record into the pocket."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* W) { if (ULyudMusic* M = MusicOf(W)) { M->GiveAllRecords(); } }));
	FAutoConsoleCommandWithWorldAndArgs GMusicRequestCmd(TEXT("lyud.Music.Request"), TEXT("A request as if posted yesterday: read at eight tonight."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* W) { if (ULyudMusic* M = MusicOf(W)) { M->RequestTonight(); } }));
	FAutoConsoleCommandWithWorldAndArgs GMusicStopCmd(TEXT("lyud.Music.Stop"), TEXT("Stops the record and the radios the hero switched on."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* W) { if (ULyudMusic* M = MusicOf(W)) { M->StopAll(); } }));

	LyudDev::FAutoAction GDevGive(TEXT("music.give_records"), TEXT("music"), TEXT("dev.action.music.give_records"),
		[](UWorld* W) { if (ULyudMusic* M = MusicOf(W)) { M->GiveAllRecords(); } });
	LyudDev::FAutoAction GDevProgram(TEXT("music.program"), TEXT("music"), TEXT("dev.action.music.program"),
		[](UWorld* W) { if (ULyudMusic* M = MusicOf(W)) { M->LogProgram(0); UE_LOG(LogLyudMusic, Display, TEXT("%s"), *M->DescribeAir()); } });
	LyudDev::FAutoAction GDevRequest(TEXT("music.request"), TEXT("music"), TEXT("dev.action.music.request"),
		[](UWorld* W) { if (ULyudMusic* M = MusicOf(W)) { M->RequestTonight(); } });
	LyudDev::FAutoAction GDevStop(TEXT("music.stop"), TEXT("music"), TEXT("dev.action.music.stop"),
		[](UWorld* W) { if (ULyudMusic* M = MusicOf(W)) { M->StopAll(); } });
}
