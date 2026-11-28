// Poker at the ship's card table (docs/ship.md "Poker"): the seat, the keys, the money, the clock and the quests' events round
// the engine-free table of Private/Cruise/PokerModel.h; the panel is SLyudPokerPanel.

#include "Cruise/LyudShipPoker.h"

#include "City/LyudCityThings.h"
#include "Cruise/LyudCruise.h"
#include "Cruise/PokerModel.h"
#include "Cruise/SLyudPokerPanel.h"
#include "Dev/LyudDev.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "LyudGameMenu.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Plants/GameCalendar.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudPoker, Log, All);

namespace LyudPokerImpl
{
	// seconds the table waits after a thing happened, so it can be read
	constexpr float PaceMove = 0.85f;
	constexpr float PaceTell = 1.7f;
	constexpr float PaceDeal = 1.1f;
	constexpr float PaceWatch = 3.2f;
	constexpr float PaceShow = 1.3f;
	/** Game seconds a second the clock runs on after a hand (four minutes in well under a second, never in one frame). */
	constexpr double ClockRate = 400.0;
	constexpr double ClockMaxPerFrame = 30.0;
	constexpr int32 LogKept = 24;
	constexpr int32 HelpLines = 7;
	/** More than all the chips of a roubles table: a stake read from a save is never believed past it. */
	constexpr int32 MaxSavedStake = 1000;

	FString Str(const char* S) { return FString(UTF8_TO_TCHAR(S)); }

	FText Tx(const TCHAR* Key) { return LyudText::Get(Key); }
	FText Tx(const TCHAR* Key, const FString& A) { return LyudText::Get(Key, { FStringFormatArg(A) }); }
	FText Tx(const TCHAR* Key, const FString& A, const FString& B) { return LyudText::Get(Key, { FStringFormatArg(A), FStringFormatArg(B) }); }

	bool HandInPlay(const LyudPoker::FTable& T) { return T.Phase != LyudPoker::EPhase::Idle && T.Phase != LyudPoker::EPhase::Over; }
}

ULyudShipPoker::ULyudShipPoker() = default;
ULyudShipPoker::~ULyudShipPoker() = default;

ULyudShipPoker* ULyudShipPoker::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudShipPoker>() : nullptr;
}

// ------------------------------------------------------------------------------------------------------------- the life cycle

void ULyudShipPoker::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Table = MakePimpl<LyudPoker::FTable>();
	Record = MakePimpl<LyudPoker::FRecord>();
	Shown = MakePimpl<FLyudPokerView>();
	LogLines.Reserve(LyudPokerImpl::LogKept + 1);
	if (ULyudCityThings* Things = Collection.InitializeDependency<ULyudCityThings>())
	{
		OpenHandle = Things->OnOpen.AddUObject(this, &ULyudShipPoker::OnCityOpen);
	}
	AfterLoad();
}

void ULyudShipPoker::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("ShipPoker"), this);
		// before the SaveGame fields are written (ULyudSaveSubsystem::SaveToSlot): what lies on the table goes into the save
		SaveHandle = Saves->OnBeforeSave.AddWeakLambda(this, [this](ULyudSaveGame*) { BeforeSave(); });
		LoadHandle = Saves->OnAfterLoad.AddWeakLambda(this, [this](ULyudSaveGame* Save)
		{
			if (Save && !Save->Records.Contains(TEXT("ShipPoker")))
			{
				// a save from before the ship: nothing played yet
				Saved.Reset();
				SavedStake = 0;
			}
			AfterLoad();
		});
		NewGameHandle = Saves->OnNewGame.AddWeakLambda(this, [this]
		{
			Saved.Reset();
			SavedStake = 0;
			AfterLoad();
		});
	}
}

void ULyudShipPoker::Deinitialize()
{
	// the level is ending: his stack goes back to his pocket, nothing is told to systems that may be gone already
	Drop(true);
	if (UWorld* World = GetWorld())
	{
		if (ULyudCityThings* Things = World->GetSubsystem<ULyudCityThings>())
		{
			Things->OnOpen.Remove(OpenHandle);
		}
		if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(World))
		{
			Saves->OnBeforeSave.Remove(SaveHandle);
			Saves->OnAfterLoad.Remove(LoadHandle);
			Saves->OnNewGame.Remove(NewGameHandle);
			Saves->UnregisterSaveObject(TEXT("ShipPoker"));
		}
	}
	Super::Deinitialize();
}

void ULyudShipPoker::AfterLoad()
{
	// a sitting does not outlive a load: whatever was on the table is gone (the pocket is the saved one), the record is the
	// saved one
	Drop(false);
	if (!Record->FromString(std::string(TCHAR_TO_UTF8(*Saved))))
	{
		*Record = LyudPoker::FRecord();
	}
	// saved in the middle of a sitting: the buy-in had left the pocket, the stack was on the table. He is standing now
	const int32 Stake = FMath::Clamp(SavedStake, 0, LyudPokerImpl::MaxSavedStake);
	SavedStake = 0;
	if (Stake > 0)
	{
		if (ULyudInventory* Inv = ULyudInventory::Get(GetWorld()))
		{
			Inv->Earn(Stake);
			Record->Night.Net -= Stake;
			Store();
			UE_LOG(LogLyudPoker, Display, TEXT("Poker: %d roubles left on the table by the save are back in the pocket"), Stake);
		}
	}
}

void ULyudShipPoker::BeforeSave()
{
	const bool bOnTable = Stage == EStage::Play && !bSettled && bRoubles && Table->NumSeats() > 0;
	SavedStake = bOnTable ? FMath::Max(0, Table->Seats[0].Stack) : 0;
}

void ULyudShipPoker::Drop(bool bPay)
{
	if (Stage == EStage::Play)
	{
		if (bPay)
		{
			if (LyudPokerImpl::HandInPlay(*Table))
			{
				Table->HeroLeave();
			}
			Settle(true);
		}
		bSettled = true;
		bHandNoted = true;
		Stage = EStage::Done;
	}
	Close();
}

void ULyudShipPoker::Store()
{
	Saved = LyudPokerImpl::Str(Record->ToString().c_str());
}

void ULyudShipPoker::OnCityOpen(FName InScreen, int32)
{
	if (InScreen == TEXT("poker"))
	{
		Open(false);
	}
}

// ------------------------------------------------------------------------------------------------------------ what the game knows

void ULyudShipPoker::SetQuestMode(const FString& Name)
{
	Record->bSharp = Name == TEXT("sharp");
	if (Record->bSharp)
	{
		// the quest starts over: he may be caught again
		Record->bCaught = false;
	}
	if (Stage == EStage::Play)
	{
		Table->bSharp = SharpWanted();
		Table->bCaught = Record->bCaught;
	}
	Store();
	UE_LOG(LogLyudPoker, Display, TEXT("Poker: quest mode %s"), Record->bSharp ? TEXT("sharp") : TEXT("off"));
}

FString ULyudShipPoker::QuestMode() const { return SharpWanted() ? FString(TEXT("sharp")) : FString(); }

bool ULyudShipPoker::SharpWanted() const
{
	if (Record->bCaught)
	{
		return false;
	}
	if (Record->bSharp)
	{
		return true;
	}
	// from the quests' data, with no code: any quest standing on a stage called "watch_hands"
	static const FName WatchStage(TEXT("watch_hands"));
	if (const ULyudQuests* Quests = ULyudQuests::Get(this))
	{
		for (const FLyudQuestDef& Def : ULyudQuests::Defs())
		{
			if (Quests->StageId(Def.Id) == WatchStage)
			{
				return true;
			}
		}
	}
	return false;
}
bool ULyudShipPoker::IsSharpCaught() const { return Record->bCaught; }
int32 ULyudShipPoker::HandsPlayed() const { return Record->Hands; }
int32 ULyudShipPoker::PotsWon() const { return Record->PotsWon; }
int32 ULyudShipPoker::BiggestPot() const { return Record->BiggestPot; }

bool ULyudShipPoker::HasBeaten(const FString& ShipId) const
{
	for (int32 O = 0; O < LyudPoker::NumOpponents; ++O)
	{
		if (ShipId == LyudPokerImpl::Str(LyudPoker::OppId(O)))
		{
			return (Record->BeatenMask & (1 << O)) != 0;
		}
	}
	return false;
}

const FLyudPokerView& ULyudShipPoker::View() const { return *Shown; }

FString ULyudShipPoker::Report() const
{
	int32 Day = 0;
	const bool bOn = CardsOn(&Day);
	return FString::Printf(TEXT("Poker: %s; cards %s (day %d); hands %d, pots won %d, biggest pot %d, beaten mask %d, hands mask %d; quest %s%s; evening %lld net %d; record %s"),
		IsOpen() ? TEXT("open") : TEXT("closed"), bOn ? TEXT("on") : TEXT("off"), Day, Record->Hands, Record->PotsWon, Record->BiggestPot,
		Record->BeatenMask, Record->HandsMask, Record->bSharp ? TEXT("sharp") : TEXT("off"), Record->bCaught ? TEXT(" (caught)") : TEXT(""),
		Record->Night.Day, Record->Night.Net, *LyudPokerImpl::Str(Record->ToString().c_str()));
}

bool ULyudShipPoker::CardsOn(int32* OutDay) const
{
	const ULyudCruise* Cruise = ULyudCruise::Get(this);
	const bool bFirst = Cruise && Cruise->IsEventOn(TEXT("cards1"));
	const bool bSecond = Cruise && Cruise->IsEventOn(TEXT("cards2"));
	if (OutDay)
	{
		*OutDay = bSecond ? 2 : 1;
	}
	return bFirst || bSecond;
}

double ULyudShipPoker::HourNow(int64* OutDay) const
{
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	if (OutDay)
	{
		*OutDay = Now.GetDate().GetTicks() / ETimespan::TicksPerDay;
	}
	return (double)Now.GetHour() + (double)Now.GetMinute() / 60.0;
}

FText ULyudShipPoker::RegularName(int32 Opp, bool bFull) const
{
	const FString Id = LyudPokerImpl::Str(LyudPoker::OppId(Opp));
	if (bFull)
	{
		// the people's own table first (npc.ship_<id>.name), ours when it is not there (a missing key comes back as itself)
		const FString Key = TEXT("npc.") + Id + TEXT(".name");
		const FText Name = LyudText::Get(Key);
		if (!Name.IsEmpty() && Name.ToString() != Key)
		{
			return Name;
		}
	}
	return LyudText::Get(TEXT("poker.name.") + Id);
}

FString ULyudShipPoker::Amount(int32 Chips) const
{
	return LyudText::Get(bRoubles ? TEXT("poker.unit.roubles") : TEXT("poker.unit.matches"), { FStringFormatArg(Chips) }).ToString();
}

// ------------------------------------------------------------------------------------------------------------------ the screen

bool ULyudShipPoker::Open(bool bForce)
{
	UWorld* World = GetWorld();
	const ULyudGameScreen* GameScreen = ULyudGameScreen::Get(World);
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport || IsOpen() || (GameScreen && GameScreen->IsOpen()) || (UI && UI->IsOpen()))
	{
		return false;
	}
	int32 Day = 1;
	const bool bOn = CardsOn(&Day);
	if (!bOn && !bForce)
	{
		if (ULyudGameScreen* GS = ULyudGameScreen::Get(this))
		{
			GS->Toast(LyudText::Get(TEXT("poker.closed")));
		}
		return false;
	}
	bForced = !bOn;
	CruiseDay = Day;
	int64 DayNumber = 0;
	const double Hour = HourNow(&DayNumber);
	Evening = LyudPoker::FNight::EveningOf(DayNumber, Hour);
	Record->Night.Roll(Evening, LyudPoker::FStakes::Roubles());

	Stage = EStage::Choose;
	bSettled = true;
	bHandNoted = true;
	bHelp = false;
	bRoubles = false;
	Cursor = 0;
	Discard = 0;
	LogSeen = 0;
	StepWait = 0.f;
	LogLines.Reset();
	TellLine = FText::GetEmpty();
	SetBanner(FText::GetEmpty());
	Shown->Help.Reset();
	for (int32 I = 0; I < LyudPokerImpl::HelpLines; ++I)
	{
		Shown->Help.Add(LyudText::Get(FString::Printf(TEXT("poker.help.%d"), I)));
	}
	Shown->DealerWord = LyudText::Get(TEXT("poker.dealer"));

	Panel = SNew(SLyudPokerPanel).Owner(this);
	Viewport->AddViewportWidgetContent(Panel.ToSharedRef(), 66);
	OpenedFrame = GFrameCounter;
	Lock(true);
	if (UI)
	{
		UI->SetExternalScreen(this);
	}
	Refresh();
	UE_LOG(LogLyudPoker, Display, TEXT("Poker: open (day %d, %.2f h%s)"), CruiseDay, Hour, bForced ? TEXT(", forced") : TEXT(""));
	return true;
}

void ULyudShipPoker::Close()
{
	if (!Panel.IsValid())
	{
		return;
	}
	if (Stage == EStage::Play)
	{
		End(EWhy::Left);
	}
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport())
		{
			Viewport->RemoveViewportWidgetContent(Panel.ToSharedRef());
		}
		if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(World))
		{
			UI->SetExternalScreen(nullptr);
		}
	}
	Panel.Reset();
	Stage = EStage::Closed;
	Lock(false);
	UE_LOG(LogLyudPoker, Display, TEXT("Poker: closed"));
}

void ULyudShipPoker::Lock(bool bLock)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || bLocked == bLock)
	{
		return;
	}
	bLocked = bLock;
	PC->SetIgnoreLookInput(bLock);
	PC->SetIgnoreMoveInput(bLock);
	PC->bShowMouseCursor = false;
	if (bLock)
	{
		PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(true).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
	}
}

// -------------------------------------------------------------------------------------------------------------------- the tick

void ULyudShipPoker::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	TickClock(DeltaTime);
	if (!IsOpen())
	{
		return;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	if (!PC || (Menu && Menu->IsOpen()))
	{
		return;
	}
	const ULyudGameScreen* GameScreen = ULyudGameScreen::Get(World);
	if (GameScreen && GameScreen->IsOpen())
	{
		Close();
		return;
	}
	if (OpenedFrame != GFrameCounter)
	{
		Keys();
	}
	if (!IsOpen() || Stage != EStage::Play)
	{
		return;
	}
	if (Table->Phase == LyudPoker::EPhase::Over)
	{
		if (!bHandNoted)
		{
			HandOver();
		}
		return;
	}
	if (Table->WaitingHero())
	{
		return;
	}
	StepWait -= DeltaTime;
	if (StepWait <= 0.f)
	{
		Table->Step();
		StepWait = TakeEvents();
		Refresh();
	}
}

void ULyudShipPoker::TickClock(float DeltaTime)
{
	if (ClockLeft <= 0.0)
	{
		return;
	}
	UGameCalendar* Cal = GetWorld()->GetSubsystem<UGameCalendar>();
	if (!Cal)
	{
		ClockLeft = 0.0;
		return;
	}
	// a few minutes over the next frames (the people's life steps through them), as the city's acts pass theirs
	const double Add = FMath::Min(ClockLeft, FMath::Clamp(LyudPokerImpl::ClockRate * (double)DeltaTime, 1.0, LyudPokerImpl::ClockMaxPerFrame));
	Cal->SetNow(Cal->Now() + FTimespan::FromSeconds(Add));
	ClockLeft -= Add;
}

void ULyudShipPoker::Keys()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	auto Pressed = [PC](const FKey& K) { return PC->WasInputKeyJustPressed(K); };
	const bool bEsc = Pressed(EKeys::Escape) || Pressed(EKeys::Tab);
	if (Pressed(EKeys::H))
	{
		bHelp = !bHelp;
		Refresh();
		return;
	}
	if (bHelp && bEsc)
	{
		bHelp = false;
		Refresh();
		return;
	}
	if (Stage == EStage::Choose)
	{
		if (bEsc)
		{
			Close();
		}
		else if (Pressed(EKeys::One))
		{
			Sit(false);
		}
		else if (Pressed(EKeys::Two))
		{
			Sit(true);
		}
		return;
	}
	if (Stage == EStage::Done)
	{
		if (bEsc || Pressed(EKeys::E) || Pressed(EKeys::S) || Pressed(EKeys::Enter))
		{
			Close();
		}
		return;
	}
	if (Stage != EStage::Play)
	{
		return;
	}
	if (bEsc)
	{
		End(EWhy::Left);
		return;
	}
	const bool bGo = Pressed(EKeys::E) || Pressed(EKeys::Enter) || Pressed(EKeys::SpaceBar);
	if (Table->Phase == LyudPoker::EPhase::Over)
	{
		if (!bHandNoted)
		{
			return;
		}
		if (bGo)
		{
			NextHand();
		}
		else if (Pressed(EKeys::S))
		{
			End(EWhy::Left);
		}
		return;
	}
	if (Pressed(EKeys::Q))
	{
		Watch();
		return;
	}
	if (!Table->WaitingHero())
	{
		// the others are playing: E hurries them
		if (bGo)
		{
			StepWait = 0.f;
		}
		return;
	}
	if (Table->Phase == LyudPoker::EPhase::Draw)
	{
		if (Pressed(EKeys::A) || Pressed(EKeys::Left))
		{
			Cursor = (Cursor + 4) % 5;
		}
		else if (Pressed(EKeys::D) || Pressed(EKeys::Right))
		{
			Cursor = (Cursor + 1) % 5;
		}
		else if (Pressed(EKeys::W) || Pressed(EKeys::Up))
		{
			int32 Marked = 0;
			for (int32 I = 0; I < 5; ++I)
			{
				Marked += (Discard >> I) & 1;
			}
			if (Discard & (1 << Cursor))
			{
				Discard &= ~(1 << Cursor);
			}
			else if (Marked < 3)
			{
				Discard |= 1 << Cursor;
			}
		}
		else if (bGo)
		{
			Table->HeroDraw(Discard);
			Discard = 0;
			StepWait = TakeEvents();
		}
		else
		{
			return;
		}
		Refresh();
		return;
	}
	LyudPoker::EAct Act = LyudPoker::EAct::CheckCall;
	if (Pressed(EKeys::One) && Table->ToCall(0) > 0)
	{
		Act = LyudPoker::EAct::Fold;
	}
	else if (Pressed(EKeys::Three) && Table->CanRaise(0))
	{
		Act = LyudPoker::EAct::BetRaise;
	}
	else if (!Pressed(EKeys::Two))
	{
		return;
	}
	Table->HeroAct(Act);
	StepWait = TakeEvents();
	Refresh();
}

// ------------------------------------------------------------------------------------------------------------------ the sitting

namespace LyudPokerImpl
{
	/** Who is at the table now (a forced table, outside the evening's hours, is the one of half past ten). */
	std::vector<int> LineupNow(int32 Day, double Hour, bool bForced, bool bCaught)
	{
		return LyudPoker::Lineup(Day, bForced ? 22.5 : Hour, true, bCaught);
	}
}

void ULyudShipPoker::Sit(bool bInRoubles)
{
	const LyudPoker::FStakes Stakes = bInRoubles ? LyudPoker::FStakes::Roubles() : LyudPoker::FStakes::Matches();
	const std::vector<int> Who = LyudPokerImpl::LineupNow(CruiseDay, HourNow(), bForced, Record->bCaught);
	ULyudInventory* Inv = ULyudInventory::Get(GetWorld());
	int32 Able = 0;
	for (const int O : Who)
	{
		Able += (!bInRoubles || Record->Night.Purse[O] > 0) ? 1 : 0;
	}
	if (Able == 0)
	{
		SetBanner(LyudText::Get(TEXT("poker.nobody")), FText::GetEmpty(), true);
		Refresh();
		return;
	}
	if (bInRoubles)
	{
		if (!Record->Night.CanBuyIn(Stakes))
		{
			SetBanner(LyudText::Get(TEXT("poker.cap"), { FStringFormatArg(Stakes.LossCap) }), FText::GetEmpty(), true);
			Refresh();
			return;
		}
		if (!Inv || !Inv->Spend(Stakes.BuyIn))
		{
			SetBanner(LyudText::Get(TEXT("poker.no_money"), { FStringFormatArg(Stakes.BuyIn) }), FText::GetEmpty(), true);
			Refresh();
			return;
		}
		Record->Night.Net += Stakes.BuyIn;
	}
	bRoubles = bInRoubles;
	Table->bSharp = SharpWanted();
	Table->bCaught = Record->bCaught;
	Table->Sit(Stakes, Who, bInRoubles ? Record->Night.Purse : nullptr, Evening, CruiseDay, Record->Hands);
	bSettled = false;
	Stage = EStage::Play;
	LogLines.Reset();
	SetBanner(FText::GetEmpty());
	Store();
	ULyudQuests::Notify(this, TEXT("poker_sit"), bRoubles ? TEXT("roubles") : TEXT("matches"));
	UE_LOG(LogLyudPoker, Display, TEXT("Poker: sat down for %s with %d regulars"), bRoubles ? TEXT("roubles") : TEXT("matches"), Table->NumSeats() - 1);
	NextHand();
}

void ULyudShipPoker::NextHand()
{
	if (!bForced && !CardsOn())
	{
		End(EWhy::Clock);
		return;
	}
	const std::vector<int> Who = LyudPokerImpl::LineupNow(CruiseDay, HourNow(), bForced, Record->bCaught);
	Cursor = 0;
	Discard = 0;
	LogSeen = 0;
	TellLine = FText::GetEmpty();
	SetBanner(FText::GetEmpty());
	Table->bSharp = SharpWanted();
	Table->bCaught = Record->bCaught;
	if (!Table->StartHand(&Who, bRoubles ? Record->Night.Purse : nullptr))
	{
		// those whose hour came got up; nobody is left to play, or nothing to play with, or enough for one sitting
		TakeEvents();
		const LyudPoker::EEnd Ended = Table->Ended();
		End(Ended == LyudPoker::EEnd::Broke ? EWhy::Broke : (Ended == LyudPoker::EEnd::MaxHands ? EWhy::Hands : EWhy::Empty));
		return;
	}
	bHandNoted = false;
	TakeEvents();
	// in the quest's mode Альберт's deal is given time to be watched
	const bool bWatch = Table->bSharp && Table->Seats[(size_t)Table->Dealer].Opp == LyudPoker::Albert;
	StepWait = bWatch ? LyudPokerImpl::PaceWatch : LyudPokerImpl::PaceDeal;
	Refresh();
}

float ULyudShipPoker::TakeEvents()
{
	float Pause = LyudPokerImpl::PaceMove;
	int32 Wins = 0;
	for (; LogSeen < (int32)Table->Log.size(); ++LogSeen)
	{
		const LyudPoker::FEvent& E = Table->Log[(size_t)LogSeen];
		const bool bHero = E.Seat == 0;
		const int32 Opp = E.Seat > 0 && E.Seat < Table->NumSeats() ? Table->Seats[(size_t)E.Seat].Opp : -1;
		const FString Name = Opp >= 0 ? RegularName(Opp, false).ToString() : FString();
		const FString Chips = Amount(E.A);
		switch (E.Type)
		{
		case LyudPoker::EEv::Deal:
			AddLog(bHero ? LyudPokerImpl::Tx(TEXT("poker.log.you_deal")) : LyudPokerImpl::Tx(TEXT("poker.log.deal"), Name));
			break;
		case LyudPoker::EEv::Ante:
			AddLog(LyudPokerImpl::Tx(TEXT("poker.log.ante"), Chips));
			break;
		case LyudPoker::EEv::Check:
			AddLog(LyudPokerImpl::Tx(bHero ? TEXT("poker.act.you.check") : TEXT("poker.act.check"), Name));
			break;
		case LyudPoker::EEv::Bet:
			AddLog(LyudPokerImpl::Tx(bHero ? TEXT("poker.act.you.bet") : TEXT("poker.act.bet"), Name, Chips));
			break;
		case LyudPoker::EEv::Call:
			AddLog(LyudPokerImpl::Tx(bHero ? TEXT("poker.act.you.call") : TEXT("poker.act.call"), Name, Chips));
			break;
		case LyudPoker::EEv::Raise:
			AddLog(LyudPokerImpl::Tx(bHero ? TEXT("poker.act.you.raise") : TEXT("poker.act.raise"), Name, Chips));
			break;
		case LyudPoker::EEv::Fold:
			AddLog(LyudPokerImpl::Tx(bHero ? TEXT("poker.act.you.fold") : TEXT("poker.act.fold"), Name));
			break;
		case LyudPoker::EEv::Draw:
			if (E.A > 0)
			{
				AddLog(LyudPokerImpl::Tx(bHero ? TEXT("poker.act.you.draw") : TEXT("poker.act.draw"), Name, FString::FromInt(E.A)));
			}
			else
			{
				AddLog(LyudPokerImpl::Tx(bHero ? TEXT("poker.act.you.pat") : TEXT("poker.act.pat"), Name));
			}
			break;
		case LyudPoker::EEv::Tell:
			if (Opp >= 0)
			{
				TellLine = LyudText::Get(FString::Printf(TEXT("poker.tell.%s.%s.%d"), *LyudPokerImpl::Str(LyudPoker::OppId(Opp)), *LyudPokerImpl::Str(LyudPoker::TellId(E.A)), E.B));
				Pause = FMath::Max(Pause, LyudPokerImpl::PaceTell);
			}
			break;
		case LyudPoker::EEv::Show:
		{
			const FString Hand = LyudText::Get(TEXT("poker.hand.") + LyudPokerImpl::Str(LyudPoker::HandId(Table->Seats[(size_t)E.Seat].Value))).ToString();
			AddLog(bHero ? LyudPokerImpl::Tx(TEXT("poker.log.you_show"), Hand) : LyudPokerImpl::Tx(TEXT("poker.log.show"), Name, Hand));
			Pause = FMath::Max(Pause, LyudPokerImpl::PaceShow);
			break;
		}
		case LyudPoker::EEv::Win:
			AddLog(bHero ? LyudPokerImpl::Tx(TEXT("poker.log.you_win"), Chips) : LyudPokerImpl::Tx(TEXT("poker.log.win"), Name, Chips));
			if (++Wins == 2)
			{
				AddLog(LyudPokerImpl::Tx(TEXT("poker.log.split")));
			}
			break;
		case LyudPoker::EEv::Talk:
			// a word between hands: on the line of the tells (it has the panel's whole width)
			if (Opp >= 0)
			{
				TellLine = LyudPokerImpl::Tx(TEXT("poker.log.says"), Name,
					LyudText::Get(FString::Printf(TEXT("poker.talk.%s.%d"), *LyudPokerImpl::Str(LyudPoker::OppId(Opp)), E.B)).ToString());
			}
			break;
		case LyudPoker::EEv::Suspect:
			SetBanner(LyudText::Get(TEXT("poker.sharp.suspect")), LyudText::Get(TEXT("poker.sharp.suspect2")), true);
			ULyudQuests::Notify(this, TEXT("card_sharp_suspect"), LyudPokerImpl::Str(LyudPoker::OppId(LyudPoker::Albert)));
			break;
		case LyudPoker::EEv::Left:
			AddLog(LyudPokerImpl::Tx(TEXT("poker.log.left"), Name));
			break;
		case LyudPoker::EEv::Gone:
			AddLog(LyudPokerImpl::Tx(TEXT("poker.log.goes"), Name));
			break;
		case LyudPoker::EEv::Joined:
			AddLog(LyudPokerImpl::Tx(TEXT("poker.log.joins"), Name));
			break;
		case LyudPoker::EEv::Caught:
		case LyudPoker::EEv::Missed:
			break;   // said by Watch
		}
	}
	return Pause;
}

void ULyudShipPoker::HandOver()
{
	bHandNoted = true;
	const LyudPoker::FResult& R = Table->Result;
	if (!R.bVoid && R.bHeroIn)
	{
		Record->Note(R, bRoubles);
		ULyudCityThings* Things = ULyudCityThings::Get(this);
		ULyudQuests::Notify(this, TEXT("do"), TEXT("poker"));
		if (R.bHeroWon)
		{
			ULyudQuests::Notify(this, TEXT("win"), TEXT("poker"));
			if (R.bShowdown)
			{
				ULyudQuests::Notify(this, TEXT("win"), TEXT("poker_showdown"));
			}
			if (R.bBluffWin)
			{
				ULyudQuests::Notify(this, TEXT("win"), TEXT("poker_bluff"));
			}
			SetBanner(LyudPokerImpl::Tx(TEXT("poker.log.you_win"), Amount(Table->Seats[0].Won)));
		}
		for (int32 O = 0; O < LyudPoker::NumOpponents; ++O)
		{
			if (R.BeatenMask & (1 << O))
			{
				const FString Id = LyudPokerImpl::Str(LyudPoker::OppId(O));
				ULyudQuests::Notify(this, TEXT("poker_beat"), Id);
				if (Things)
				{
					Things->GiveStamp(TEXT("poker/") + Id);
				}
			}
		}
		if (R.bHeroShown)
		{
			const FString Hand = LyudPokerImpl::Str(LyudPoker::HandId(R.HeroValue));
			if (LyudPoker::CatOf(R.HeroValue) >= LyudPoker::OnePair)
			{
				ULyudQuests::Notify(this, TEXT("poker_hand"), Hand);
			}
			if (Things && LyudPoker::CatOf(R.HeroValue) >= LyudPoker::Straight)
			{
				Things->GiveStamp(TEXT("poker/") + Hand);
			}
		}
		ClockLeft += 60.0 * LyudPoker::MinutesPerHand;
	}
	Store();
	Refresh();
}

void ULyudShipPoker::Watch()
{
	if (!Table->bSharp)
	{
		return;   // nothing to look for outside the quest
	}
	if (Table->Phase == LyudPoker::EPhase::Dealing && Table->Dealer > 0 && Table->Seats[(size_t)Table->Dealer].Opp != LyudPoker::Albert)
	{
		// somebody else's deal: honest, and said so
		SetBanner(LyudPokerImpl::Tx(TEXT("poker.sharp.clean"), RegularName(Table->Seats[(size_t)Table->Dealer].Opp, false).ToString()));
		Refresh();
		return;
	}
	switch (Table->WatchHands())
	{
	case LyudPoker::EWatch::NotNow:
		SetBanner(LyudText::Get(TEXT("poker.sharp.not_now")));
		break;
	case LyudPoker::EWatch::Nothing:
		SetBanner(LyudText::Get(TEXT("poker.sharp.missed")));
		ULyudQuests::Notify(this, TEXT("card_sharp_missed"), LyudPokerImpl::Str(LyudPoker::OppId(LyudPoker::Albert)));
		break;
	case LyudPoker::EWatch::Caught:
		// the hand is void, he leaves the table and does not come back; the mode has done its work
		Record->bCaught = true;
		Record->bSharp = false;
		Table->bSharp = false;
		bHandNoted = true;
		SetBanner(LyudText::Get(TEXT("poker.sharp.caught")), LyudText::Get(TEXT("poker.sharp.caught2")));
		ULyudQuests::Notify(this, TEXT("card_sharp_caught"), LyudPokerImpl::Str(LyudPoker::OppId(LyudPoker::Albert)));
		if (ULyudCityThings* Things = ULyudCityThings::Get(this))
		{
			Things->GiveStamp(TEXT("poker/sharp"));
		}
		Store();
		UE_LOG(LogLyudPoker, Display, TEXT("Poker: the card sharp is caught"));
		break;
	}
	TakeEvents();
	Refresh();
}

void ULyudShipPoker::End(EWhy Why)
{
	if (Stage != EStage::Play)
	{
		return;
	}
	if (LyudPokerImpl::HandInPlay(*Table))
	{
		// getting up in the middle of a hand: his cards are folded, the others play it out
		Table->HeroLeave();
		TakeEvents();
	}
	if (!bHandNoted)
	{
		HandOver();
	}
	Settle();
	EndWhy = Why;
	Stage = EStage::Done;
	SetBanner(FText::GetEmpty());
	Refresh();
}

void ULyudShipPoker::Settle(bool bQuiet)
{
	if (bSettled)
	{
		return;
	}
	bSettled = true;
	const int32 Stack = Table->Seats[0].Stack;
	const int32 Net = Stack - Table->HeroBuyIn;
	if (bRoubles)
	{
		if (ULyudInventory* Inv = ULyudInventory::Get(GetWorld()))
		{
			if (Stack > 0)
			{
				Inv->Earn(Stack);
			}
		}
		Record->Night.Net -= Stack;
		Table->Purses(Record->Night.Purse);
	}
	Store();
	if (!bQuiet)
	{
		ULyudQuests::Notify(this, TEXT("poker_leave"), Net > 0 ? TEXT("up") : (Net < 0 ? TEXT("down") : TEXT("even")));
		if (Stack <= 0)
		{
			ULyudQuests::Notify(this, TEXT("poker_broke"), bRoubles ? TEXT("roubles") : TEXT("matches"));
		}
	}
	UE_LOG(LogLyudPoker, Display, TEXT("Poker: got up with %d of %d (%s); the evening's count %d"), Stack, Table->HeroBuyIn,
		bRoubles ? TEXT("roubles") : TEXT("matches"), Record->Night.Net);
}

void ULyudShipPoker::SetBanner(const FText& Line, const FText& Line2, bool bBad)
{
	Banner = Line;
	Banner2 = Line2;
	bBannerBad = bBad;
}

void ULyudShipPoker::AddLog(const FText& Line)
{
	if (LogLines.Num() >= LyudPokerImpl::LogKept)
	{
		LogLines.RemoveAt(0);
	}
	LogLines.Add(Line);
}

// ------------------------------------------------------------------------------------------------------- what the panel paints

void ULyudShipPoker::Refresh()
{
	FLyudPokerView& V = *Shown;
	++V.Serial;
	V.Stage = Stage == EStage::Play ? 1 : (Stage == EStage::Done ? 2 : 0);
	V.bHelp = bHelp;
	V.Tell = TellLine;
	V.Banner = Banner;
	V.Banner2 = Banner2;
	V.bBannerBad = bBannerBad;
	V.Keys2 = LyudText::Get(TEXT("poker.keys.common"));
	V.Lines.Reset();
	V.Seats.Reset();
	V.Cursor = -1;
	V.Discard = 0;
	if (Stage == EStage::Choose)
	{
		const LyudPoker::FStakes M = LyudPoker::FStakes::Matches();
		const LyudPoker::FStakes R = LyudPoker::FStakes::Roubles();
		V.Title = LyudText::Get(TEXT("poker.title"));
		V.Keys2 = FText::GetEmpty();
		V.Lines.Add(LyudText::Get(TEXT("poker.choose")));
		V.Lines.Add(LyudText::Get(TEXT("poker.choose.matches"), { FStringFormatArg(M.BuyIn) }));
		V.Lines.Add(LyudText::Get(TEXT("poker.choose.roubles"), { FStringFormatArg(R.BuyIn), FStringFormatArg(R.Ante), FStringFormatArg(R.Small),
			FStringFormatArg(R.Big) }));
		V.Lines.Add(LyudText::Get(TEXT("poker.choose.cap"), { FStringFormatArg(R.LossCap) }));
		// who is at the table tonight, and what is known of each
		for (const int O : LyudPokerImpl::LineupNow(CruiseDay, HourNow(), bForced, Record->bCaught))
		{
			V.Lines.Add(LyudPokerImpl::Tx(TEXT("poker.log.says"), RegularName(O, true).ToString(),
				LyudText::Get(TEXT("poker.style.") + LyudPokerImpl::Str(LyudPoker::OppId(O))).ToString()));
		}
		V.Keys = LyudText::Get(TEXT("poker.choose.keys"));
		return;
	}
	V.Title = LyudPokerImpl::Tx(TEXT("poker.title.table"), LyudText::Get(bRoubles ? TEXT("poker.table.roubles") : TEXT("poker.table.matches")).ToString());
	const LyudPoker::FSeat& Hero = Table->Seats[0];
	if (Stage == EStage::Done)
	{
		static const TCHAR* const Why[] = { TEXT("poker.end.left"), TEXT("poker.end.broke"), TEXT("poker.end.empty"), TEXT("poker.end.hands"), TEXT("poker.end.clock") };
		V.Lines.Add(LyudText::Get(Why[(int32)EndWhy]));
		const int32 Net = Hero.Stack - Table->HeroBuyIn;
		if (bRoubles)
		{
			V.Lines.Add(Net == 0 ? LyudText::Get(TEXT("poker.end.even"))
				: LyudText::Get(Net > 0 ? TEXT("poker.end.up") : TEXT("poker.end.down"), { FStringFormatArg(FMath::Abs(Net)) }));
		}
		else
		{
			V.Lines.Add(LyudText::Get(TEXT("poker.end.matches"), { FStringFormatArg(Table->HeroBuyIn), FStringFormatArg(Hero.Stack) }));
		}
		V.Keys = LyudText::Get(TEXT("poker.keys.over"));
		return;
	}

	const bool bOver = Table->Phase == LyudPoker::EPhase::Over;
	const bool bShown = bOver && Table->Result.bShowdown;
	V.HandNo = LyudText::Get(TEXT("poker.hand_no"), { FStringFormatArg(Table->HandBase + Table->HandNo) }).ToString();
	V.Pot = LyudPokerImpl::Tx(TEXT("poker.pot"), Amount(bOver ? Table->Result.Pot : Table->Pot())).ToString();
	V.Stack = LyudPokerImpl::Tx(TEXT("poker.stack"), Amount(Hero.Stack)).ToString();
	auto Fill = [this, bOver, bShown](FLyudPokerSeatView& Out, const LyudPoker::FSeat& S, int32 Index)
	{
		Out.Stack = Amount(S.Stack);
		Out.bDealer = Index == Table->Dealer;
		Out.bTurn = Index == Table->Turn;
		Out.bDim = !S.bIn || S.bFolded;
		Out.bWon = bOver && S.bIn && S.Won > S.Put;
		Out.NumCards = S.bIn ? 5 : 0;
		Out.bFaces = Index == 0 || (bShown && S.Live());
		for (int32 C = 0; C < 5; ++C)
		{
			Out.Cards[C] = S.Cards[C];
		}
		Out.Note.Reset();
		if (S.bOut)
		{
			Out.Note = LyudText::Get(TEXT("poker.mark.out")).ToString();
		}
		else if (S.bIn && S.bFolded)
		{
			Out.Note = LyudText::Get(TEXT("poker.mark.fold")).ToString();
		}
		else if (S.bIn && S.Stack == 0 && !bOver)
		{
			Out.Note = LyudText::Get(TEXT("poker.mark.allin")).ToString();
		}
		else if (S.bIn && S.Bet > 0)
		{
			Out.Note = LyudPokerImpl::Tx(TEXT("poker.bet"), Amount(S.Bet)).ToString();
		}
		else if (S.bIn && S.Drew >= 0)
		{
			Out.Note = LyudText::Get(TEXT("poker.mark.drew"), { FStringFormatArg(S.Drew) }).ToString();
		}
		Out.Hand = S.bIn && Out.bFaces ? LyudText::Get(TEXT("poker.hand.") + LyudPokerImpl::Str(LyudPoker::HandId(S.Value))) : FText::GetEmpty();
	};
	for (int32 I = 1; I < Table->NumSeats(); ++I)
	{
		const LyudPoker::FSeat& S = Table->Seats[(size_t)I];
		if (S.bOut && !S.bIn)
		{
			continue;   // got up before this hand
		}
		FLyudPokerSeatView& Seat = V.Seats.AddDefaulted_GetRef();
		Seat.Name = RegularName(S.Opp, true);
		Fill(Seat, S, I);
	}
	V.Hero.Name = LyudText::Get(TEXT("poker.you"));
	Fill(V.Hero, Hero, 0);
	const bool bMine = Table->WaitingHero();
	if (bMine && Table->Phase == LyudPoker::EPhase::Draw)
	{
		V.Cursor = Cursor;
		V.Discard = Discard;
		// the draw's instruction is a long line: on the banner's row (the whole width) when nothing else is said there
		if (V.Banner.IsEmpty())
		{
			V.Banner = LyudText::Get(TEXT("poker.log.your_draw"));
			V.bBannerBad = false;
		}
		V.Keys = LyudText::Get(TEXT("poker.keys.draw"));
	}
	else if (bMine)
	{
		V.Hero.Note = LyudText::Get(TEXT("poker.log.your_turn")).ToString();
		const int32 Need = Table->ToCall(0);
		V.Keys = Need > 0 ? LyudPokerImpl::Tx(TEXT("poker.keys.bet_call"), Amount(Need), Amount(Table->CurrentBet + Table->Unit()))
			: LyudPokerImpl::Tx(TEXT("poker.keys.bet"), Amount(Table->Unit()));
	}
	else if (bOver)
	{
		V.Keys = LyudText::Get(TEXT("poker.keys.next"));
	}
	else if (Table->Phase == LyudPoker::EPhase::Dealing && Table->bSharp)
	{
		V.Keys = LyudText::Get(TEXT("poker.keys.deal_watch"));
	}
	else
	{
		V.Keys = LyudText::Get(TEXT("poker.keys.wait"));
	}
	V.Log = LogLines;
}

// ---------------------------------------------------------------------------------------------- the console and developer mode

static FAutoConsoleCommandWithWorldAndArgs GPokerCmd(TEXT("lyud.Poker"),
	TEXT("lyud.Poker open | force | close | report: the ship's card table (docs/ship.md); force opens it whatever the hour"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudShipPoker* P = ULyudShipPoker::Get(World);
		if (!P)
		{
			return;
		}
		const FString Cmd = Args.Num() > 0 ? Args[0].ToLower() : FString(TEXT("report"));
		if (Cmd == TEXT("open") || Cmd == TEXT("force"))
		{
			UE_LOG(LogLyudPoker, Display, TEXT("Poker: %s: %s"), *Cmd, P->Open(Cmd == TEXT("force")) ? TEXT("yes") : TEXT("no"));
		}
		else if (Cmd == TEXT("close"))
		{
			P->Close();
		}
		else
		{
			UE_LOG(LogLyudPoker, Display, TEXT("%s"), *P->Report());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GPokerQuestCmd(TEXT("lyud.Poker.Quest"),
	TEXT("lyud.Poker.Quest sharp | off: the quest's marked-cards mode of the ship's poker (the sharp marks the hands he deals)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudShipPoker* P = ULyudShipPoker::Get(World))
		{
			P->SetQuestMode(Args.Num() > 0 ? Args[0].ToLower() : FString());
		}
	}));

static LyudDev::FAutoAction GDevPokerOpen(TEXT("poker.open"), TEXT("cruise"), TEXT("dev.action.poker.open"), [](UWorld* W)
{
	if (ULyudShipPoker* P = ULyudShipPoker::Get(W))
	{
		P->Close();
		P->Open(true);
	}
});

static LyudDev::FAutoAction GDevPokerSharp(TEXT("poker.sharp"), TEXT("cruise"), TEXT("dev.action.poker.sharp"),
	[](UWorld* W)
	{
		if (ULyudShipPoker* P = ULyudShipPoker::Get(W))
		{
			P->SetQuestMode(P->QuestMode().IsEmpty() ? TEXT("sharp") : TEXT("off"));
		}
	},
	[](UWorld* W)
	{
		const ULyudShipPoker* P = ULyudShipPoker::Get(W);
		return P && !P->QuestMode().IsEmpty();
	});
