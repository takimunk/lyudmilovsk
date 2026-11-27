// A talk with a townsperson (docs/town_talk.md "A talk"): ULyudFriends' page for it. Who may be asked and what about is
// the engine-free LyudTownChat (TownChat.*), their words are picked by ULyudTownTalkSubsystem (ChatBuild, ChatLine).
// Nothing here runs on a clock but the offer on the prompt line (LyudFriends.cpp TickOffer) and, while a talk is up, a
// look four times a second at whether they are still there.
#include "People/LyudFriends.h"

#include "Friends.h"
#include "PeopleSim.h"
#include "TownBarks.h"
#include "TownChat.h"
#include "City/SLyudCityPage.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "People/PeopleLife.h"
#include "People/TownTalk.h"
#include "UI/LyudQuests.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudFriendsTalk, Log, All);

namespace LyudFriendsTalkImpl
{
	/** The talk ends when they are this far from him (cm): he was moved, or they were. */
	constexpr double LeaveDist = 450.0;
	/** They stay where they are for this long after the last look (the life's seconds; let go when the talk ends). */
	constexpr float HoldSeconds = 120.f;
	constexpr float LookEvery = 0.25f;
	/** A quest's own lines for a resident shown before the talk: this many at most. */
	constexpr int32 MaxOpeners = 8;

	FString TStr(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
	std::string TStd(const FString& S)
	{
		const FTCHARToUTF8 Conv(*S);
		return std::string(Conv.Get(), (size_t)Conv.Length());
	}

	const TCHAR* RefuseName(int32 R)
	{
		switch ((LyudTownChat::ERefuse)R)
		{
		case LyudTownChat::ERefuse::None: return TEXT("free for a word");
		case LyudTownChat::ERefuse::Gone: return TEXT("nobody to ask (asleep, away, inside)");
		case LyudTownChat::ERefuse::Riding: return TEXT("riding");
		case LyudTownChat::ERefuse::Work: return TEXT("serving somebody");
		case LyudTownChat::ERefuse::Hurry: return TEXT("in a hurry");
		case LyudTownChat::ERefuse::Shy: return TEXT("a shy child");
		default: return TEXT("?");
		}
	}
}

void ULyudFriends::LoadTalk()
{
	bTalkLoaded = false;
	FString Text;
	const FString File = FPaths::ProjectContentDir() / TEXT("PeopleData/town_chat.json");
	if (!TalkData || !FFileHelper::LoadFileToString(Text, *File))
	{
		UE_LOG(LogLyudFriendsTalk, Warning, TEXT("Town chat: %s not found: nobody to talk to"), *File);
		return;
	}
	std::string Error;
	if (!TalkData->Load(LyudFriendsTalkImpl::TStd(Text), Error))
	{
		UE_LOG(LogLyudFriendsTalk, Warning, TEXT("Town chat: %s"), *LyudFriendsTalkImpl::TStr(Error));
		return;
	}
	bTalkLoaded = true;
	UE_LOG(LogLyudFriendsTalk, Display, TEXT("Town chat: %d questions back, %d names"), (int32)TalkData->Asks.size(), (int32)TalkData->Names.size());
}

int32 ULyudFriends::TalkRefusal(int32 PersonId) const
{
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LyudPeople::FWorld* S = Life ? Life->GetSim() : nullptr;
	if (!bTalkLoaded || !TalkData || !Model || !S || PersonId < 0 || PersonId >= (int32)S->People.size())
	{
		return (int32)LyudTownChat::ERefuse::Gone;
	}
	const LyudPeople::FPerson& P = S->People[(size_t)PersonId];
	return (int32)LyudTownChat::CanChat(*S, P, *TalkData, Life->GetMet(PersonId), Model->Level(P.Key));
}

bool ULyudFriends::TalkMayGift() const
{
	// a gift is for somebody he knows (they greeted him at least once), as it was on the prompt line
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	return Life && Model && TalkPerson >= 0 && (Life->GetMet(TalkPerson) > 0 || Model->Level(LyudFriendsTalkImpl::TStd(TalkKey)) >= 1);
}

bool ULyudFriends::TalkLine(const FString& Event, int32 Other, FText& Out)
{
	ULyudTownTalkSubsystem* Town = ULyudTownTalkSubsystem::Get(this);
	FString Key, Detail;
	if (!Town || !Town->ChatLine(TalkPerson, Event, Key, Detail))
	{
		return false;
	}
	// {0} the nickname they gave him ("neighbour" till they did), {1} what the town says he did, {2}..{7} the given name of
	// the one they speak of in the six cases (Russian; the other languages have the name as it is in all six)
	const FString Nick = NickOf(TalkPerson);
	const FString NameId = Other >= 0 ? LyudFriendsTalkImpl::TStr(LyudTownChat::NameId(LyudFriendsTalkImpl::TStd(KeyOf(Other)))) : FString();
	static const TCHAR* const Forms[] = { TEXT("nom"), TEXT("gen"), TEXT("dat"), TEXT("acc"), TEXT("ins"), TEXT("pre") };
	TArray<FStringFormatArg> Args;
	Args.Reserve(8);
	Args.Add(FStringFormatArg(Nick.IsEmpty() ? LyudText::Get(TEXT("friends.nick.neighbour")).ToString() : Nick));
	Args.Add(FStringFormatArg(Detail));
	for (const TCHAR* Form : Forms)
	{
		Args.Add(FStringFormatArg(NameId.IsEmpty() ? FString() : LyudText::Get(FString::Printf(TEXT("townchat.fn.%s.%s"), *NameId, Form)).ToString()));
	}
	Out = LyudText::Get(Key, Args);
	// the mouth moves while he reads it
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	FPersonInfo Info;
	if (Life && Life->GetPerson(TalkPerson, Info))
	{
		if (ANpcBody* Body = Cast<ANpcBody>(Info.Actor.Get()))
		{
			Body->Chatter(2.5f);
		}
	}
	return true;
}

bool ULyudFriends::OpenTalk(int32 PersonId, bool bForce)
{
	UWorld* World = GetWorld();
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	LyudPeople::FWorld* S = Life ? Life->GetSim() : nullptr;
	ULyudTownTalkSubsystem* Town = ULyudTownTalkSubsystem::Get(this);
	if (!World || !S || !Town || !bTalkLoaded || !TalkNow || Mode != EMode::None || PersonId < 0 || PersonId >= (int32)S->People.size())
	{
		return false;
	}
	const int32 Refusal = bForce ? 0 : TalkRefusal(PersonId);
	if (Refusal != (int32)LyudTownChat::ERefuse::None)
	{
		// not now: they say why over their head (serving somebody, late, a child who doesn't know him) and go on
		const char* Event = LyudTownChat::RefuseEvent((LyudTownChat::ERefuse)Refusal);
		if (Event && *Event && SayEvent(PersonId, FName(UTF8_TO_TCHAR(Event))))
		{
			Life->ReactManner(PersonId, TEXT("nod"), 2.f);
		}
		if (Refusal == (int32)LyudTownChat::ERefuse::Shy)
		{
			Life->NoteMet(PersonId);       // a child gets used to him: a try on two days and they answer
		}
		RefusedUntil = World->GetTimeSeconds() + 3.0;
		SetPrompt(-1);
		return false;
	}
	if (!Town->ChatBuild(PersonId, *TalkData, *TalkNow))
	{
		return false;
	}
	TalkPerson = PersonId;
	TalkKey = KeyOf(PersonId);
	bTalkForced = bForce;
	TalkSaid = FText::GetEmpty();
	if (!TalkLine(LyudFriendsTalkImpl::TStr(TalkNow->Hello), -1, TalkLineText) && TalkNow->Topics.empty())
	{
		TalkPerson = -1;
		TalkKey.Reset();
		return false;       // nothing to say at all (the book isn't read)
	}
	TalkAsked.Init(false, (int32)TalkNow->Topics.size());
	TalkSel = 0;
	TalkHeard = 0;
	bTalkAsked = false;
	TalkHold = 0.f;
	// a quest that waits for a talk with this very resident: its own lines first (line.<key>.<quest>.<stage>.<n>,
	// docs/quests.md), asked before the talk is reported as a quest that this talk begins counts as on its first stage
	TalkOpeners.Reset();
	TalkOpener = 0;
	if (const ULyudQuests* Quests = ULyudQuests::Get(this))
	{
		for (const FLyudQuestDef& Def : ULyudQuests::Defs())
		{
			const FName Stage = Quests->TalkStage(Def.Id, TalkKey);
			for (int32 N = 1; !Stage.IsNone() && TalkOpeners.Num() < LyudFriendsTalkImpl::MaxOpeners; ++N)
			{
				const FString Key = FString::Printf(TEXT("line.%s.%s.%s.%d"), *TalkKey, *Def.Id.ToString(), *Stage.ToString(), N);
				if (LyudText::Get(Key).ToString() == Key)
				{
					break;
				}
				TalkOpeners.Add(Key);
			}
		}
	}
	ULyudQuests::Notify(this, TEXT("talk"), TalkKey);
	Life->NoteMet(PersonId);
	S->HoldWalk(PersonId, LyudFriendsTalkImpl::HoldSeconds);
	Mode = EMode::Talk;
	GiftReturn = EMode::None;
	TalkStep = TalkOpeners.Num() > 0 ? ETalkStep::Opener : ETalkStep::Menu;
	SetPrompt(-1);
	OpenPage();
	if (!Page)
	{
		EndMode();
		return false;
	}
	ShowTalk();
	UE_LOG(LogLyudFriendsTalk, Display, TEXT("Town chat: a talk with %s (%d topics%s)"), *TalkKey, TalkAsked.Num(), TalkNow->Ask >= 0 ? TEXT(", a question back") : TEXT(""));
	return true;
}

void ULyudFriends::ShowTalk()
{
	if (!Page || !TalkNow)
	{
		return;
	}
	FString Body;
	FText Keys = LyudText::Get(TEXT("townchat.keys_next"));
	if (TalkStep == ETalkStep::Opener)
	{
		const FString Nick = NickOf(TalkPerson);
		Body = TalkOpeners.IsValidIndex(TalkOpener) ? LyudText::Get(TalkOpeners[TalkOpener],
			{ FStringFormatArg(Nick.IsEmpty() ? LyudText::Get(TEXT("friends.nick.neighbour")).ToString() : Nick) }).ToString() : FString();
	}
	else
	{
		if (!TalkSaid.IsEmpty())
		{
			Body = LyudText::Get(TEXT("townchat.said"), { FStringFormatArg(TalkSaid.ToString()) }).ToString() + TEXT("\n\n");
		}
		Body += TalkLineText.ToString();
	}
	if (TalkStep == ETalkStep::Menu)
	{
		// what is left to ask about, and a gift for somebody he knows
		TalkEntries.Reset();
		for (int32 I = 0; I < TalkAsked.Num(); ++I)
		{
			if (!TalkAsked[I])
			{
				TalkEntries.Add(I);
			}
		}
		if (TalkMayGift())
		{
			TalkEntries.Add(-1);
		}
		TalkSel = TalkEntries.Num() > 0 ? FMath::Clamp(TalkSel, 0, TalkEntries.Num() - 1) : 0;
		Body += TEXT("\n");
		for (int32 E = 0; E < TalkEntries.Num(); ++E)
		{
			const int32 Ix = TalkEntries[E];
			const FString Title = Ix < 0 ? LyudText::Get(TEXT("townchat.gift")).ToString()
				: LyudText::Get(LyudFriendsTalkImpl::TStr(TalkNow->Topics[(size_t)Ix].Title)).ToString();
			Body += FString::Printf(TEXT("\n%s%s"), E == TalkSel ? TEXT("> ") : TEXT("  "),
				*LyudText::Get(TEXT("townchat.entry"), { FStringFormatArg(E + 1), FStringFormatArg(Title) }).ToString());
		}
		Keys = LyudText::Get(TEXT("townchat.keys"));
	}
	else if (TalkStep == ETalkStep::Ask)
	{
		const LyudTownChat::FAsk* Ask = TalkNow->Ask >= 0 && TalkNow->Ask < (int)TalkData->Asks.size() ? &TalkData->Asks[(size_t)TalkNow->Ask] : nullptr;
		Body += TEXT("\n");
		for (int32 N = 1; Ask && N <= Ask->Answers; ++N)
		{
			const FString Answer = LyudText::Get(FString::Printf(TEXT("townchat.ask.%s.a%d"), *LyudFriendsTalkImpl::TStr(Ask->Id), N)).ToString();
			Body += TEXT("\n") + LyudText::Get(TEXT("townchat.entry"), { FStringFormatArg(N), FStringFormatArg(Answer) }).ToString();
		}
		Keys = LyudText::Get(TEXT("townchat.keys_ask"));
	}
	else if (TalkStep == ETalkStep::Bye)
	{
		Keys = LyudText::Get(TEXT("townchat.keys_bye"));
	}
	Page->SetPage(NameOf(TalkKey), FText::FromString(Body), Keys);
}

bool ULyudFriends::TalkKeep(float DeltaTime)
{
	TalkHold -= DeltaTime;
	if (TalkHold > 0.f)
	{
		return true;
	}
	TalkHold = LyudFriendsTalkImpl::LookEvery;
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	LyudPeople::FWorld* S = Life ? Life->GetSim() : nullptr;
	if (!S || TalkPerson < 0 || TalkPerson >= (int32)S->People.size())
	{
		return false;
	}
	S->HoldWalk(TalkPerson, LyudFriendsTalkImpl::HoldSeconds);
	if (bTalkForced)
	{
		return true;        // a developer's talk: with anybody, anywhere
	}
	// they went to sleep, indoors or away, their body isn't drawn any more, or he isn't beside them: the talk is over
	const APawn* Hero = UGameplayStatics::GetPlayerPawn(this, 0);
	FPersonInfo Info;
	if (!Hero || !LyudBarks::CanSpeak(*S, S->People[(size_t)TalkPerson]) || !Life->GetPerson(TalkPerson, Info) || !IsValid(Info.Actor.Get())
		|| Info.Actor->IsHidden() || FVector::Dist2D(Info.Actor->GetActorLocation(), Hero->GetActorLocation()) > LyudFriendsTalkImpl::LeaveDist)
	{
		return false;
	}
	if (ANpcBody* Body = Cast<ANpcBody>(Info.Actor.Get()))
	{
		Body->LookAtPoint(Hero->GetActorLocation() + FVector(0, 0, 60), LyudFriendsTalkImpl::LookEvery * 4.f);
	}
	return true;
}

void ULyudFriends::TalkKeys(APlayerController* PC)
{
	if (!PC || !TalkNow)
	{
		return;
	}
	const bool bEsc = PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::SpaceBar);
	const bool bUse = PC->WasInputKeyJustPressed(EKeys::E);
	static const FKey Digits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	switch (TalkStep)
	{
	case ETalkStep::Opener:
		if (bUse || bEsc)
		{
			if (++TalkOpener >= TalkOpeners.Num())
			{
				TalkStep = ETalkStep::Menu;
			}
			ShowTalk();
		}
		return;
	case ETalkStep::Answer:
		if (bUse || bEsc)
		{
			TalkNext();
		}
		return;
	case ETalkStep::Ask:
		if (bEsc)
		{
			TalkBye();
			return;
		}
		if (TalkNow->Ask >= 0 && TalkNow->Ask < (int)TalkData->Asks.size())
		{
			const int32 Answers = FMath::Min(TalkData->Asks[(size_t)TalkNow->Ask].Answers, 9);
			for (int32 N = 0; N < Answers; ++N)
			{
				if (PC->WasInputKeyJustPressed(Digits[N]))
				{
					TalkAnswer(N + 1);
					return;
				}
			}
		}
		return;
	case ETalkStep::Bye:
		if (bUse || bEsc)
		{
			EndMode();
		}
		return;
	default:
		break;
	}
	// the topics: a digit asks at once, A / D move the mark and E asks the marked one, Esc says goodbye
	if (bEsc)
	{
		TalkBye();
		return;
	}
	const int32 Count = TalkEntries.Num();
	if (Count == 0)
	{
		if (bUse)
		{
			TalkBye();
		}
		return;
	}
	if (PC->WasInputKeyJustPressed(EKeys::D) || PC->WasInputKeyJustPressed(EKeys::A))
	{
		TalkSel = (TalkSel + (PC->WasInputKeyJustPressed(EKeys::D) ? 1 : Count - 1)) % Count;
		ShowTalk();
		return;
	}
	if (bUse)
	{
		TalkChoose(TalkSel);
		return;
	}
	for (int32 I = 0; I < FMath::Min(Count, 9); ++I)
	{
		if (PC->WasInputKeyJustPressed(Digits[I]))
		{
			TalkChoose(I);
			return;
		}
	}
}

void ULyudFriends::TalkChoose(int32 Entry)
{
	if (!TalkEntries.IsValidIndex(Entry) || !TalkNow)
	{
		return;
	}
	const int32 Ix = TalkEntries[Entry];
	if (Ix < 0)
	{
		OpenGift(TalkPerson, TalkKey);      // (nothing to give: a word on the screen, the talk stays)
		return;
	}
	if (Ix >= (int32)TalkNow->Topics.size() || !TalkAsked.IsValidIndex(Ix))
	{
		return;
	}
	const LyudTownChat::FTopic& Topic = TalkNow->Topics[(size_t)Ix];
	TalkAsked[Ix] = true;
	FText Line;
	if (!TalkLine(LyudFriendsTalkImpl::TStr(Topic.Event), Topic.Other, Line))
	{
		ShowTalk();     // the moment for that line has passed (the hour turned): the topic is simply gone
		return;
	}
	TalkSaid = LyudText::Get(LyudFriendsTalkImpl::TStr(Topic.Title));
	TalkLineText = Line;
	TalkSel = 0;
	// the first thing heard in a talk is the talk: the bond grows a little, once a day (docs/friends.md)
	if (++TalkHeard == 1 && Model && !TalkKey.IsEmpty())
	{
		const std::string Key = LyudFriendsTalkImpl::TStd(TalkKey);
		const int32 Added = Model->Chatted(Key, Today());
		const LyudFriends::FBond* B = Model->Find(Key);
		if (Added > 0 && B && B->Chats == 1)
		{
			if (ULyudQuests* Quests = ULyudQuests::Get(this))
			{
				Quests->AddNote(TEXT("townchat.diary"), { NameOf(TalkKey).ToString() });
			}
		}
		Grew(TalkKey, Added);
	}
	// their question back comes after the answer has been read
	TalkStep = TalkNow->Ask >= 0 && !bTalkAsked && TalkHeard >= TalkNow->AskAfter ? ETalkStep::Answer : ETalkStep::Menu;
	ShowTalk();
}

void ULyudFriends::TalkNext()
{
	bTalkAsked = true;
	TalkStep = ETalkStep::Menu;
	if (TalkNow && TalkNow->Ask >= 0 && TalkNow->Ask < (int)TalkData->Asks.size())
	{
		FText Line;
		if (TalkLine(TEXT("tc_ask_") + LyudFriendsTalkImpl::TStr(TalkData->Asks[(size_t)TalkNow->Ask].Id), -1, Line))
		{
			TalkSaid = FText::GetEmpty();
			TalkLineText = Line;
			TalkStep = ETalkStep::Ask;
		}
	}
	ShowTalk();
}

void ULyudFriends::TalkAnswer(int32 Answer)
{
	if (!TalkNow || TalkNow->Ask < 0 || TalkNow->Ask >= (int)TalkData->Asks.size())
	{
		return;
	}
	const FString Id = LyudFriendsTalkImpl::TStr(TalkData->Asks[(size_t)TalkNow->Ask].Id);
	TalkSaid = LyudText::Get(FString::Printf(TEXT("townchat.ask.%s.a%d"), *Id, Answer));
	FText Line;
	TalkLineText = TalkLine(FString::Printf(TEXT("tc_reply_%s_%d"), *Id, Answer), -1, Line) ? Line : FText::GetEmpty();
	// he answered them: a little more (within the day's cap)
	if (Model && !TalkKey.IsEmpty())
	{
		Grew(TalkKey, Model->ChatAnswered(LyudFriendsTalkImpl::TStd(TalkKey), Today()));
	}
	TalkStep = ETalkStep::Menu;
	ShowTalk();
}

void ULyudFriends::TalkBye()
{
	FText Line;
	if (!TalkNow || !TalkLine(LyudFriendsTalkImpl::TStr(TalkNow->Bye), -1, Line))
	{
		EndMode();
		return;
	}
	TalkSaid = FText::GetEmpty();
	TalkLineText = Line;
	TalkStep = ETalkStep::Bye;
	if (UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this))
	{
		Life->ReactManner(TalkPerson, TalkHeard > 0 ? TEXT("wave") : TEXT("nod"), 2.f);
	}
	ShowTalk();
}

void ULyudFriends::EndTalk()
{
	// they go on with their day
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	if (LyudPeople::FWorld* S = Life ? Life->GetSim() : nullptr)
	{
		S->HoldWalk(TalkPerson, 0.f);
	}
	TalkPerson = -1;
	TalkKey.Reset();
	TalkOpeners.Reset();
	TalkEntries.Reset();
	TalkAsked.Reset();
	TalkSaid = FText::GetEmpty();
	TalkLineText = FText::GetEmpty();
	bTalkForced = false;
}

bool ULyudFriends::DevTalk(const FString& Key)
{
	int32 Id = -1;
	if (Key.IsEmpty())
	{
		const ULyudTownTalkSubsystem* Town = ULyudTownTalkSubsystem::Get(this);
		AActor* Actor = nullptr;
		Id = Town ? Town->NearestSpeaker(Actor) : -1;
	}
	else
	{
		Id = PersonOf(Key);
	}
	return Id >= 0 && OpenTalk(Id, true);
}

FString ULyudFriends::DevTalkReport()
{
	ULyudTownTalkSubsystem* Town = ULyudTownTalkSubsystem::Get(this);
	AActor* Actor = nullptr;
	const int32 Id = Town ? Town->NearestSpeaker(Actor) : -1;
	if (Id < 0 || !bTalkLoaded || !TalkData)
	{
		return TEXT("Town chat: nobody near who could speak");
	}
	FString Out = FString::Printf(TEXT("Town chat: %s (%s): %s"), *NameOf(KeyOf(Id)).ToString(), *KeyOf(Id), LyudFriendsTalkImpl::RefuseName(TalkRefusal(Id)));
	LyudTownChat::FTalk Would;
	if (Town->ChatBuild(Id, *TalkData, Would))
	{
		Out += FString::Printf(TEXT("\n  greeting %s, goodbye %s"), *LyudFriendsTalkImpl::TStr(Would.Hello), *LyudFriendsTalkImpl::TStr(Would.Bye));
		for (const LyudTownChat::FTopic& T : Would.Topics)
		{
			Out += FString::Printf(TEXT("\n  %s: %s -> %s%s"), *LyudFriendsTalkImpl::TStr(T.Id), *LyudText::Get(LyudFriendsTalkImpl::TStr(T.Title)).ToString(),
				*LyudFriendsTalkImpl::TStr(T.Event), T.Other >= 0 ? *(TEXT(" about ") + KeyOf(T.Other)) : TEXT(""));
		}
		if (Would.Ask >= 0 && Would.Ask < (int)TalkData->Asks.size())
		{
			Out += FString::Printf(TEXT("\n  asks back after %d: %s"), Would.AskAfter, *LyudFriendsTalkImpl::TStr(TalkData->Asks[(size_t)Would.Ask].Id));
		}
	}
	return Out;
}
