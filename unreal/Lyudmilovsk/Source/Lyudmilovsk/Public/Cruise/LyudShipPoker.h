#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "Tickable.h"
#include "LyudShipPoker.generated.h"

class SLyudPokerPanel;
struct FLyudPokerView;
namespace LyudPoker { struct FTable; struct FRecord; }

/**
 * Poker at the card table of the motor ship «Полдень» (docs/ship.md "Poker"): five-card draw against the table's regulars
 * (Альберт, Гиви, Вера Игнатьевна, Тарас, Блюм, Лев Маркович), each with his own way of playing and of giving himself away.
 * The rules, the regulars and their tells are the engine-free LyudPoker::FTable (Private/Cruise/PokerModel.h, checked by
 * tools/pokersim/run.sh); this subsystem seats the hero, takes the keys, moves the money and the clock and tells the quests.
 *
 * Opened by a city act with "open": "poker" (P-SHIP/card_table: ULyudCityThings seats the hero on a card chair first, then
 * broadcasts OnOpen) while the evening's cards are on (ULyudCruise::IsEventOn cards1 / cards2, 22:00-01:00). He chooses the
 * table: «на интерес» (matches, free) or «по маленькой» (roubles: the buy-in is taken with ULyudInventory::Spend when he sits
 * and his stack comes back with Earn when he gets up; never more than the cap lost in one evening).
 *
 * Keys (polled, never consumed): 1 fold, 2 check / call, 3 bet / raise; at the draw A / D move over the cards, W marks one
 * to change, E changes; E deals the next hand, S or Esc gets up (Esc in the middle of a hand folds it first); H the rules;
 * Q watches the dealer's hands (the quest's mode only). About four game minutes pass with every hand.
 *
 * Quests: ULyudQuests::Notify do/poker, win/poker, win/poker_showdown, win/poker_bluff, poker_beat/<ship_id>,
 * poker_hand/<hand>, poker_sit, poker_leave, poker_broke, card_sharp_suspect, card_sharp_caught, card_sharp_missed; stamps
 * poker/<ship_id>, poker/<hand>, poker/sharp (ULyudCityThings::GiveStamp). Saved as "ShipPoker".
 *
 * Console: lyud.Poker open | force | close | report, lyud.Poker.Quest sharp | off. Developer mode: group "cruise".
 */
UCLASS()
class LYUDMILOVSK_API ULyudShipPoker : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudShipPoker();
	virtual ~ULyudShipPoker() override;

	static ULyudShipPoker* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudShipPoker, STATGROUP_Tickables); }

	/** Sits the hero down to choose a table. False: another screen is up, or the table has not gathered (a word is said);
	 *  bForce: whatever the hour (developer mode, the console). */
	bool Open(bool bForce = false);
	/** Gets him up: a hand in play is folded, his stack is cashed. */
	void Close();
	bool IsOpen() const { return Panel.IsValid(); }

	/** The quests' switch: "sharp" turns the marked-cards mode on (Альберт marks the hands he deals; watching his hands
	 *  while he deals catches him: event card_sharp_caught), anything else turns it off. Saved. The mode is also on, with no
	 *  call at all, while any quest stands on a stage whose id is "watch_hands" (until he is caught). */
	void SetQuestMode(const FString& Name);
	FString QuestMode() const;
	bool IsSharpCaught() const;

	/** For the journal and the quests: hands played, pots taken, the biggest pot taken at the roubles table. */
	int32 HandsPlayed() const;
	int32 PotsWon() const;
	int32 BiggestPot() const;
	/** A regular ("ship_givi") was beaten at a showdown at least once. */
	bool HasBeaten(const FString& ShipId) const;
	FString Report() const;

	/** What the panel paints (rebuilt only when something happened). */
	const FLyudPokerView& View() const;

	/** The record as text (LyudPoker::FRecord::ToString). */
	UPROPERTY(SaveGame)
	FString Saved;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	enum class EStage : uint8 { Closed, Choose, Play, Done };
	enum class EWhy : uint8 { Left, Broke, Empty, Hands, Clock };

	TPimplPtr<LyudPoker::FTable> Table;
	TPimplPtr<LyudPoker::FRecord> Record;
	TPimplPtr<FLyudPokerView> Shown;
	TSharedPtr<SLyudPokerPanel> Panel;

	EStage Stage = EStage::Closed;
	bool bForced = false;
	bool bRoubles = false;
	bool bSettled = true;
	bool bHandNoted = true;
	bool bHelp = false;
	bool bLocked = false;
	uint64 OpenedFrame = 0;
	/** The cruise day of the evening (1 Friday, 2 Saturday) and the evening's day number. */
	int32 CruiseDay = 1;
	int64 Evening = 0;
	int32 Cursor = 0;
	int32 Discard = 0;
	int32 LogSeen = 0;
	float StepWait = 0.f;
	/** Game seconds still to pass (a few minutes a hand, over the next frames). */
	double ClockLeft = 0.0;
	TArray<FText> LogLines;
	FText TellLine;
	FText Banner;
	FText Banner2;
	bool bBannerBad = false;
	EWhy EndWhy = EWhy::Left;

	void Lock(bool bLock);
	void Keys();
	void Sit(bool bInRoubles);
	void NextHand();
	/** The model's new events become lines of the log, the tell, the banner; returns the pause they ask for. */
	float TakeEvents();
	void HandOver();
	void End(EWhy Why);
	/** His stack goes back to his pocket, the purses to the night's count; once a sitting. */
	void Settle();
	void Watch();
	void SetBanner(const FText& Line, const FText& Line2 = FText::GetEmpty(), bool bBad = false);
	void AddLog(const FText& Line);
	void Refresh();
	void Store();
	void TickClock(float DeltaTime);

	/** The marked-cards mode is asked for (SetQuestMode, or a quest standing on a stage called "watch_hands") and he is
	 *  not caught yet. */
	bool SharpWanted() const;
	bool CardsOn(int32* OutDay = nullptr) const;
	double HourNow(int64* OutDay = nullptr) const;
	FText RegularName(int32 Opp, bool bFull) const;
	FString Amount(int32 Chips) const;

	FDelegateHandle OpenHandle;
	void OnCityOpen(FName InScreen, int32 Thing);
	FDelegateHandle LoadHandle;
	FDelegateHandle NewGameHandle;
	void AfterLoad();
};
