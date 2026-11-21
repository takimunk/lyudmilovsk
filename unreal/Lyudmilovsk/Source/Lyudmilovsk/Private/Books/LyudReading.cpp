// Reading anywhere and the library's book club (docs/books.md "Reading anywhere", "The book club"). The rules are
// Private/Books/ReadingModel.h (tools/booksim/readingtest.cpp); the data Content/Reading/reading.json and the words
// Content/Localization/reading (tools/reading/build_reading.py).

#include "Books/LyudReading.h"

#include "Books/LyudBooks.h"
#include "Books/ReadingModel.h"
#include "City/LyudBenchSit.h"
#include "City/LyudCityThings.h"
#include "City/LyudPublicSpaces.h"
#include "Dev/LyudDev.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Festival/LyudFestival.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Letters/LyudLetters.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NpcDialogue.h"
#include "People/LyudFriends.h"
#include "People/PeopleLife.h"
#include "People/PeopleSim.h"
#include "People/TownTalk.h"
#include "Plants/GameCalendar.h"
#include "Sky/LyudSky.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "Weather/LyudRainFog.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudReading, Log, All);

namespace LyudReadingImpl
{
	FString Str(const std::string& S) { return UTF8_TO_TCHAR(S.c_str()); }
	std::string Std(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }
	FString WorkOf(const FString& Key)
	{
		FString Lang, Work;
		return Key.Split(TEXT("/"), &Lang, &Work) ? Work : Key;
	}
	FString TitleArg(const FString& Work) { return TEXT("@book.title.") + Work; }
	FString AuthorArg(const FString& Work) { return TEXT("@book.author.") + Work; }
	FText Title(const FString& Work) { return LyudText::Get(TEXT("book.title.") + Work); }
	FText Author(const FString& Work) { return LyudText::Get(TEXT("book.author.") + Work); }
	/** How near the hero must be to the club's table for its talk (cm). */
	constexpr float ClubNearCm = 1200.f;
	/** A line of the talk every this many real seconds while he listens. */
	constexpr float TalkEvery = 7.5f;
	/** The condition goes on this long before the club's hour: the members walk over first. */
	constexpr float GatherEarlyH = 0.33f;
}

ULyudReading::ULyudReading() = default;
ULyudReading::~ULyudReading() = default;

ULyudReading* ULyudReading::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudReading>() : nullptr;
}

void ULyudReading::Deinitialize()
{
	SetPrompt(0);
	if (bConditionOn)
	{
		if (UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this))
		{
			People->SetCondition(TEXT("book_club"), false);
		}
	}
	Super::Deinitialize();
}

void ULyudReading::LoadData()
{
	bLoaded = true;
	Club = MakePimpl<LyudReading::FClub>();
	FString Json;
	const FString Path = FPaths::ProjectContentDir() / TEXT("Reading/reading.json");
	std::string Error;
	if (!FFileHelper::LoadFileToString(Json, *Path) || !LyudReading::LoadClub(LyudReadingImpl::Std(Json), Club->D, Error))
	{
		UE_LOG(LogLyudReading, Warning, TEXT("Reading: %s: %s (python3 tools/reading/build_reading.py)"), *Path, *LyudReadingImpl::Str(Error));
		Club.Reset();
		return;
	}
	UE_LOG(LogLyudReading, Display, TEXT("Reading: the book club, %d members, %d books"), (int32)Club->D.Members.size(), (int32)Club->D.Books.size());
}

// ------------------------------------------------------------------------------------------------------------------- light

bool ULyudReading::CanReadHere(FText* Why, float* Bright, bool bWhileOpen) const
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	LyudReading::FLightIn In;
	if (const ULyudRainFogSubsystem* Rain = ULyudRainFogSubsystem::Get(World))
	{
		In.OpenSky = Rain->GetOpenSky();
		const FLyudRainFogWeather W = Rain->ReadWeather();
		In.RainMmH = W.RainRate;
		In.Snow = W.Snow;
	}
	else
	{
		In.OpenSky = 0.f;   // an indoor level (no sky, no rain): always light enough
	}
	if (World)
	{
		if (TActorIterator<ALyudSky> It(World); It)
		{
			In.SunAltDeg = It->SunAltitude();
		}
	}
	if (const ULyudGameScreen* Screen = ULyudGameScreen::Get(World))
	{
		In.bHeadlamp = Screen->IsHeadLampOn();
	}
	const FVector At = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	if (World)
	{
		if (TActorIterator<ALyudPublicSpaces> It(World); It)
		{
			const float Cm = It->NearestLitLampCm(At);
			if (Cm >= 0.f)
			{
				In.LampM = Cm / 100.f;
			}
		}
	}
	const LyudReading::FLight L = LyudReading::Judge(In);
	if (Bright)
	{
		*Bright = L.CanRead() ? FMath::Max(0.35f, L.Bright) : 0.35f;
	}
	if (Why)
	{
		*Why = L.Verdict == LyudReading::EVerdict::Rain ? LyudText::Get(bWhileOpen ? TEXT("reading.rain_close") : TEXT("reading.rain"))
			: L.Verdict == LyudReading::EVerdict::Dark ? LyudText::Get(bWhileOpen ? TEXT("reading.dusk_close") : TEXT("reading.dark"))
			: L.Verdict == LyudReading::EVerdict::Dim ? LyudText::Get(TEXT("reading.dim")) : FText::GetEmpty();
	}
	return L.CanRead();
}

// ------------------------------------------------------------------------------------------------------------------ prompts

void ULyudReading::SetPrompt(int32 What)
{
	if (What == PromptShown)
	{
		return;
	}
	PromptShown = What;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, What == 1 ? LyudText::Get(TEXT("reading.prompt.read")) : What == 2 ? LyudText::Get(TEXT("reading.club.join")) : FText::GetEmpty());
	}
}

void ULyudReading::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	if (!bLoaded)
	{
		LoadData();
	}
	APlayerController* PC = World->GetFirstPlayerController();
	AHeroCharacter* Hero = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	SlowClock -= DeltaTime;
	if (SlowClock <= 0.f)
	{
		SlowClock = 1.f;
		StepSlow();
	}
	// an open book outdoors: the dusk or the rain shuts it (asked every two seconds)
	ULyudBookScreen* Screen = ULyudBookScreen::Get(World);
	if (Screen && Screen->IsReading())
	{
		OpenCheckClock -= DeltaTime;
		if (OpenCheckClock <= 0.f)
		{
			OpenCheckClock = 2.f;
			FText Why;
			float Bright = 1.f;
			if (!CanReadHere(&Why, &Bright, true))
			{
				Screen->Close();
				if (ULyudGameScreen* G = ULyudGameScreen::Get(World))
				{
					G->Toast(Why);
				}
			}
			else
			{
				Screen->SetPageBright(Bright);
			}
		}
	}
	else
	{
		OpenCheckClock = 0.f;
	}
	if (!Hero || !PC || Hero->IsHidden() || UGameplayStatics::IsGamePaused(World))
	{
		SetPrompt(0);
		return;
	}
	TickClub(Hero, PC, DeltaTime);
	if (PromptShown != 2)
	{
		TickSeated(Hero, PC);
	}
}

void ULyudReading::TickSeated(AHeroCharacter* Hero, APlayerController* PC)
{
	UWorld* World = GetWorld();
	const ULyudBooks* Books = ULyudBooks::Get(World);
	const ULyudCityThings* Things = ULyudCityThings::Get(World);
	const ULyudBenchSit* Bench = ULyudBenchSit::Get(World);
	const ULyudBookScreen* Screen = ULyudBookScreen::Get(World);
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	const bool bSeated = (Hero->IsSeated() && !Hero->IsOnBike()) || (Things && Things->IsSittingOnThing());
	const bool bBusy = (UI && UI->IsOpen()) || (Screen && Screen->IsOpen()) || (Bench && Bench->IsOnBench()) || (Things && Things->HasModeUp());
	// the line is free (or ours): another seat's own prompt (the restaurant's menu, the train's ticket) comes first
	const UObject* Owner = UI ? UI->GetPromptOwner() : nullptr;
	const bool bLineFree = Owner == nullptr || Owner == this;
	if (!bSeated || bBusy || !Books || Books->Borrowed().Num() == 0 || !bLineFree)
	{
		SetPrompt(0);
		return;
	}
	SetPrompt(1);
	if (ANpcTalkDirector::UsePressed(PC))
	{
		SetPrompt(0);
		if (ULyudBookScreen* S = ULyudBookScreen::Get(World))
		{
			S->OpenReader(FString());
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------- the club

FString ULyudReading::ClubBook(const FDateTime& When) const
{
	return Club ? LyudReadingImpl::Str(Club->BookOf(When.GetMonth())) : FString();
}

bool ULyudReading::ClubSitsNow() const
{
	if (!Club)
	{
		return false;
	}
	if (ForceClub >= 0)
	{
		return ForceClub == 1;
	}
	UWorld* World = GetWorld();
	const FDateTime Now = UGameCalendar::NowIn(World);
	const ULyudFestivalSubsystem* Fest = ULyudFestivalSubsystem::Get(World);
	const float Hour = Now.GetHour() + Now.GetMinute() / 60.f;
	return Club->MeetsOn((int32)Now.GetDayOfWeek(), Fest && Fest->IsFestivalToday(), Fest && Fest->IsDayOff())
		&& Hour >= Club->D.From - LyudReadingImpl::GatherEarlyH && Hour < Club->D.To;
}

bool ULyudReading::HeroReadThisMonth(const FString& Work) const
{
	const ULyudBooks* Books = ULyudBooks::Get(GetWorld());
	return Books && Books->FinishedWorks().Contains(Work);
}

void ULyudReading::TickClub(AHeroCharacter* Hero, APlayerController* PC, float DeltaTime)
{
	UWorld* World = GetWorld();
	UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(World);
	const bool bSits = ClubSitsNow();
	if (People && (!bConditionSent || bSits != bConditionOn))
	{
		People->SetCondition(TEXT("book_club"), bSits);
		bConditionOn = bSits;
		bConditionSent = true;
	}
	if (!bSits || !People || !Club)
	{
		if (PromptShown == 2)
		{
			SetPrompt(0);
		}
		HeroLine = -1;
		bClubNear = false;
		ClubScanClock = 0.f;
		return;
	}
	// while he is away from the table (the whole evening, wherever he is in town) the members are looked up twice a second,
	// not every frame: the look-up below makes strings and searches the people by key
	if (!bClubNear)
	{
		ClubScanClock -= DeltaTime;
		if (ClubScanClock > 0.f)
		{
			return;
		}
		ClubScanClock = 0.5f;
	}
	const LyudPeople::FWorld* Sim = People->GetSim();
	const int32 ClubPlace = Sim ? Sim->FindPlace(Club->D.Place) : -1;
	const FString Work = ClubBook(UGameCalendar::NowIn(World));
	if (Work != TalkBook)
	{
		TalkBook = Work;
		Turn = 0;
	}
	// who of the circle sits there now, and is he near them
	TArray<TPair<int32, AActor*>, TInlineAllocator<8>> Seated;
	for (const LyudReading::FMember& M : Club->D.Members)
	{
		const int32 Id = People->FindPerson(LyudReadingImpl::Str(M.Key));
		if (!Sim || Id < 0 || Id >= (int32)Sim->People.size())
		{
			continue;
		}
		const LyudPeople::FPerson& P = Sim->People[(size_t)Id];
		AActor* Body = People->GetShownBody(Id);
		if (P.Place == ClubPlace && P.State == LyudPeople::EState::Doing && Body)
		{
			Seated.Add({ Id, Body });
		}
	}
	bool bNear = false;
	for (const TPair<int32, AActor*>& S : Seated)
	{
		bNear |= FVector::Dist(S.Value->GetActorLocation(), Hero->GetActorLocation()) < LyudReadingImpl::ClubNearCm;
	}
	bClubNear = Seated.Num() >= 2 && bNear;
	if (!bClubNear)
	{
		if (PromptShown == 2)
		{
			SetPrompt(0);
		}
		return;
	}
	// the talk: one line every few seconds, round the circle in its order (a member not there is skipped)
	ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(World);
	TalkClock -= DeltaTime;
	if (Talk && TalkClock <= 0.f && HeroLine < 0)
	{
		TalkClock = LyudReadingImpl::TalkEvery;
		for (int32 Try = 0; Try < Club->NumLines(); ++Try, ++Turn)
		{
			const LyudReading::FMember* M = Club->MemberOf(Club->SpeakerRole(Turn));
			const int32 Id = M ? People->FindPerson(LyudReadingImpl::Str(M->Key)) : -1;
			const TPair<int32, AActor*>* Who = Seated.FindByPredicate([Id](const TPair<int32, AActor*>& S) { return S.Key == Id; });
			if (Who)
			{
				Talk->SayLine(Who->Value, FString::Printf(TEXT("line.town.bookclub.%s.%d"), *Work, Club->LineNumber(Turn)), Id);
				++Turn;
				break;
			}
		}
	}
	// his own lines, one after the other, after he joined
	if (HeroLine >= 0)
	{
		HeroClock -= DeltaTime;
		if (HeroClock <= 0.f)
		{
			if (HeroLine < Club->D.HeroLines)
			{
				if (ULyudGameScreen* G = ULyudGameScreen::Get(World))
				{
					G->Toast(LyudText::Get(FString::Printf(TEXT("reading.club.hero.%s.%d"), *JoinedBook, HeroLine + 1)));
				}
				++HeroLine;
				HeroClock = 5.f;
			}
			else
			{
				HeroLine = -1;
				TalkClock = 2.f;
			}
		}
	}
	// he may join: the month's book read to its end, and not joined this month yet
	ULyudBooks* Books = ULyudBooks::Get(World);
	const FDateTime Now = UGameCalendar::NowIn(World);
	const FString Month = FString::Printf(TEXT("%04d-%02d"), Now.GetYear(), Now.GetMonth());
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	const ULyudBookScreen* Screen = ULyudBookScreen::Get(World);
	const bool bFree = !(UI && UI->IsOpen()) && !(Screen && Screen->IsOpen());
	if (Books && bFree && HeroLine < 0 && !Books->ClubMonths.Contains(Month) && HeroReadThisMonth(Work))
	{
		SetPrompt(2);
		if (ANpcTalkDirector::UsePressed(PC))
		{
			SetPrompt(0);
			Books->ClubMonths.Add(Month);
			JoinedBook = Work;
			HeroLine = 0;
			HeroClock = 1.5f;
			if (Talk && Seated.Num() > 0)
			{
				// the librarian (else whoever is there) asks him in
				const LyudReading::FMember* Lib = Club->MemberOf("librarian");
				const int32 LibId = Lib ? People->FindPerson(LyudReadingImpl::Str(Lib->Key)) : -1;
				const TPair<int32, AActor*>* Who = Seated.FindByPredicate([LibId](const TPair<int32, AActor*>& S) { return S.Key == LibId; });
				Talk->SayLine(Who ? Who->Value : Seated[0].Value, TEXT("line.town.bookclub_welcome"), Who ? LibId : Seated[0].Key);
			}
			if (ULyudCityThings* Things = ULyudCityThings::Get(World))
			{
				Things->GiveStamp(TEXT("book_club/") + Work);
			}
			if (ULyudQuests* Q = ULyudQuests::Get(World))
			{
				Q->AddNote(TEXT("reading.note.club"), { LyudReadingImpl::TitleArg(Work) });
			}
			ULyudQuests::Notify(World, TEXT("book_club"), Work);
			if (ULyudGameScreen* G = ULyudGameScreen::Get(World))
			{
				G->Toast(LyudText::Get(TEXT("reading.club.joined"), { FStringFormatArg(LyudReadingImpl::Title(Work).ToString()) }));
			}
			UE_LOG(LogLyudReading, Display, TEXT("Reading: the hero joined the club's talk about %s"), *Work);
		}
	}
	else if (PromptShown == 2)
	{
		SetPrompt(0);
	}
}

// ----------------------------------------------------------------------------------------------- once a second: the post

void ULyudReading::StepSlow()
{
	UWorld* World = GetWorld();
	ULyudBooks* Books = ULyudBooks::Get(World);
	ULyudLetters* Letters = ULyudLetters::Get(World);
	if (!Books || !Letters)
	{
		return;
	}
	const int32 Today = Books->Today();
	// a week-old loan: the librarian's postcard, once a loan
	for (const FString& Key : Books->Borrowed())
	{
		const int32 Since = Books->BorrowedOn(Key);
		const FString Mark = FString::Printf(TEXT("%s@%d"), *Key, Since);
		if (!LyudReading::IsOverdue(Since, Today) || Books->OverdueNoticed.Contains(Mark))
		{
			continue;
		}
		const FString Work = LyudReadingImpl::WorkOf(Key);
		if (Letters->PostTownLetter(TEXT("book_overdue_") + Mark, TEXT("reading.letter.from"), TEXT("reading.letter.overdue"),
			{ LyudReadingImpl::TitleArg(Work), FString::FromInt(Today - Since) }))
		{
			Books->OverdueNoticed.Add(Mark);
			UE_LOG(LogLyudReading, Display, TEXT("Reading: %s is overdue (%d days): the librarian writes"), *Key, Today - Since);
		}
	}
	// a friend he recommended a book to writes what they thought
	if (!Club)
	{
		return;
	}
	for (FLyudBookAdvice& A : Books->Advice)
	{
		if (A.bAnswered || Today < A.Day + Club->ReplyDelay(LyudReadingImpl::Std(A.Work), LyudReadingImpl::Std(A.Friend)))
		{
			continue;
		}
		const int32 N = Club->Opinion(LyudReadingImpl::Std(A.Work), LyudReadingImpl::Std(A.Friend));
		if (Letters->PostTownLetter(FString::Printf(TEXT("book_advice_%s_%s"), *A.Friend, *A.Work), A.Friend,
			FString::Printf(TEXT("reading.friend.opinion.%d"), N), { LyudReadingImpl::TitleArg(A.Work) }))
		{
			A.bAnswered = true;
		}
	}
}

// ------------------------------------------------------------------------------------------------ advice, the paper

FText ULyudReading::Advice() const
{
	ULyudBooks* Books = ULyudBooks::Get(GetWorld());
	if (!Books || !Club)
	{
		return FText::GetEmpty();
	}
	std::set<std::string> Available;
	for (const FLyudBookInfo& B : Books->Catalog())
	{
		Available.insert(LyudReadingImpl::Std(B.Work));
	}
	std::vector<std::string> Finished;
	for (const FString& W : Books->FinishedWorks())
	{
		Finished.push_back(LyudReadingImpl::Std(W));
	}
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	const FString Pick = LyudReadingImpl::Str(Club->Recommend(Finished, Available, Now.GetMonth()));
	if (Pick.IsEmpty())
	{
		return FText::GetEmpty();
	}
	// why: the book he liked that leads to it, else the club's month, else a first pick
	for (const std::string& F : Finished)
	{
		for (const auto& P : Club->D.Pairs)
		{
			if (P.first == F && LyudReadingImpl::Str(P.second) == Pick)
			{
				return LyudText::Get(TEXT("reading.desk.advice"), { FStringFormatArg(LyudReadingImpl::Title(Pick).ToString()),
					FStringFormatArg(LyudReadingImpl::Author(Pick).ToString()), FStringFormatArg(LyudReadingImpl::Title(LyudReadingImpl::Str(F)).ToString()) });
			}
		}
	}
	if (Pick == ClubBook(Now))
	{
		return LyudText::Get(TEXT("reading.desk.advice_month"), { FStringFormatArg(LyudReadingImpl::Title(Pick).ToString()),
			FStringFormatArg(LyudReadingImpl::Author(Pick).ToString()) });
	}
	return LyudText::Get(TEXT("reading.desk.advice_start"), { FStringFormatArg(LyudReadingImpl::Title(Pick).ToString()),
		FStringFormatArg(LyudReadingImpl::Author(Pick).ToString()) });
}

FText ULyudReading::Recommend(const FString& Work)
{
	UWorld* World = GetWorld();
	ULyudBooks* Books = ULyudBooks::Get(World);
	const ULyudFriends* Friends = ULyudFriends::Get(World);
	if (!Books || !Friends)
	{
		return FText::GetEmpty();
	}
	TArray<FString> Keys;
	Friends->FriendKeys(2, Keys);
	Keys.RemoveAll([](const FString& K) { return !K.StartsWith(TEXT("t_")); });
	if (Keys.Num() == 0)
	{
		return LyudText::Get(TEXT("reading.recommend.nobody"));
	}
	// the closest friend who is not still reading something of his
	for (const FString& K : Keys)
	{
		const bool bBusy = Books->Advice.ContainsByPredicate([&K, &Work](const FLyudBookAdvice& A) { return A.Friend == K && (!A.bAnswered || A.Work == Work); });
		if (bBusy)
		{
			continue;
		}
		FLyudBookAdvice A;
		A.Work = Work;
		A.Friend = K;
		A.Day = Books->Today();
		Books->Advice.Add(A);
		const FText Name = LyudText::Get(FString::Printf(TEXT("npc.%s.name"), *K));
		if (ULyudQuests* Q = ULyudQuests::Get(World))
		{
			Q->AddNote(TEXT("reading.note.recommend"), { LyudReadingImpl::TitleArg(Work), TEXT("@npc.") + K + TEXT(".name") });
		}
		return LyudText::Get(TEXT("reading.recommend.done"), { FStringFormatArg(LyudReadingImpl::Title(Work).ToString()), FStringFormatArg(Name.ToString()) });
	}
	return LyudText::Get(TEXT("reading.recommend.all"));
}

void ULyudReading::PaperLines(const FDateTime& Date, TArray<TPair<FString, TArray<FString>>>& Out) const
{
	Out.Reset();
	if (!Club)
	{
		return;
	}
	const FString Work = ClubBook(Date);
	Out.Emplace(TEXT("reading.paper.title"), TArray<FString>());
	Out.Emplace(TEXT("reading.paper.book"), TArray<FString>{ LyudReadingImpl::TitleArg(Work), LyudReadingImpl::AuthorArg(Work) });
	if (Club->MeetsOn((int32)Date.GetDayOfWeek(), false, false))
	{
		Out.Emplace(TEXT("reading.paper.tonight"), TArray<FString>());
	}
	if (Date.GetDay() > FDateTime::DaysInMonth(Date.GetYear(), Date.GetMonth()) - 7)
	{
		const FString Next = LyudReadingImpl::Str(Club->BookOf(Date.GetMonth() % 12 + 1));
		Out.Emplace(TEXT("reading.paper.next"), TArray<FString>{ LyudReadingImpl::TitleArg(Next), LyudReadingImpl::AuthorArg(Next) });
	}
}

// ------------------------------------------------------------------------------------------------------ console, dev

FString ULyudReading::Report() const
{
	const ULyudBooks* Books = ULyudBooks::Get(GetWorld());
	FText Why;
	float Bright = 0.f;
	const bool bCan = CanReadHere(&Why, &Bright);
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	return FString::Printf(TEXT("Reading: light %s (%.2f) %s; club book %s, sits now %d (forced %d), condition %d; quotes %d, pages %d, joined %d months, advice %d; desk: %s"),
		bCan ? TEXT("ok") : TEXT("no"), Bright, *Why.ToString(), *ClubBook(Now), ClubSitsNow() ? 1 : 0, ForceClub, bConditionOn ? 1 : 0,
		Books ? Books->GetQuotes().Num() : 0, Books ? Books->PagesRead() : 0, Books ? Books->ClubMonths.Num() : 0, Books ? Books->Advice.Num() : 0,
		*Advice().ToString());
}

void ULyudReading::DevFinishMonthBook()
{
	ULyudBooks* Books = ULyudBooks::Get(GetWorld());
	const FString Work = ClubBook(UGameCalendar::NowIn(GetWorld()));
	if (!Books || Work.IsEmpty())
	{
		return;
	}
	const FString Key = (Books->Find(LyudText::Language() + TEXT("/") + Work) ? LyudText::Language() : FString(TEXT("en"))) + TEXT("/") + Work;
	if (!Books->Find(Key))
	{
		// the texts are not fetched on this machine: mark the work read all the same
		Books->FinishedKeys.AddUnique(Key);
		return;
	}
	Books->MarkFinished(Key);
}

void ULyudReading::DevOverdue()
{
	if (ULyudBooks* Books = ULyudBooks::Get(GetWorld()))
	{
		for (const FString& K : Books->Borrowed())
		{
			Books->BorrowDays.Add(K, Books->Today() - LyudReading::LoanDays - 1);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GReadingCmd(TEXT("lyud.Reading"),
	TEXT("lyud.Reading report | club on|off|auto | recommend <work> | finish_month | overdue"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudReading* R = ULyudReading::Get(World);
		if (!R)
		{
			return;
		}
		const FString Cmd = Args.Num() > 0 ? Args[0].ToLower() : TEXT("report");
		if (Cmd == TEXT("club") && Args.Num() > 1)
		{
			R->ForceClub = Args[1] == TEXT("on") ? 1 : Args[1] == TEXT("off") ? 0 : -1;
		}
		else if (Cmd == TEXT("recommend") && Args.Num() > 1)
		{
			UE_LOG(LogLyudReading, Display, TEXT("%s"), *R->Recommend(Args[1]).ToString());
		}
		else if (Cmd == TEXT("finish_month"))
		{
			R->DevFinishMonthBook();
		}
		else if (Cmd == TEXT("overdue"))
		{
			R->DevOverdue();
		}
		UE_LOG(LogLyudReading, Display, TEXT("%s"), *R->Report());
	}));

static LyudDev::FAutoAction GDevReadingFinish(TEXT("reading.finish_month"), TEXT("books"), TEXT("dev.action.reading.finish_month"), [](UWorld* W)
{
	if (ULyudReading* R = ULyudReading::Get(W))
	{
		R->DevFinishMonthBook();
	}
});

static LyudDev::FAutoAction GDevReadingClub(TEXT("reading.club_now"), TEXT("books"), TEXT("dev.action.reading.club_now"), [](UWorld* W)
{
	// the coming Thursday 18:40 (the people's life is placed anew after a clock jump), at the library's lounge
	if (UGameCalendar* Cal = W ? W->GetSubsystem<UGameCalendar>() : nullptr)
	{
		const FDateTime Now = Cal->Now();
		int32 Days = (3 - (int32)Now.GetDayOfWeek() + 7) % 7;
		FDateTime At = FDateTime(Now.GetYear(), Now.GetMonth(), Now.GetDay(), 18, 40) + FTimespan::FromDays(Days);
		if (At < Now)
		{
			At += FTimespan::FromDays(7);
		}
		Cal->SetNow(At);
	}
	if (ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->GoToThing(TEXT("P-LIBRARY/lounge_dominoes"));
	}
});

static LyudDev::FAutoAction GDevReadingOverdue(TEXT("reading.overdue"), TEXT("books"), TEXT("dev.action.reading.overdue"), [](UWorld* W)
{
	if (ULyudReading* R = ULyudReading::Get(W))
	{
		R->DevOverdue();
	}
});

static LyudDev::FAutoAction GDevReadingRecommend(TEXT("reading.recommend"), TEXT("books"), TEXT("dev.action.reading.recommend"), [](UWorld* W)
{
	ULyudReading* R = ULyudReading::Get(W);
	ULyudBooks* Books = ULyudBooks::Get(W);
	if (!R || !Books)
	{
		return;
	}
	const TArray<FString> Works = Books->FinishedWorks();
	const FText Line = R->Recommend(Works.Num() > 0 ? Works[0] : FString(TEXT("little_princess")));
	if (ULyudGameScreen* G = ULyudGameScreen::Get(W))
	{
		G->Toast(Line);
	}
});
