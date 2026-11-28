#pragma once

#include "CoreMinimal.h"
#include "LyudSave.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "Tickable.h"
#include "LyudFriends.generated.h"

class AActor;
class AHeroCharacter;
class ALyudGoodsItem;
class ANpcBody;
class APlayerController;
class SLyudCityPage;
namespace LyudFriends { class FModel; struct FWho; struct FHost; }
namespace LyudTownChat { struct FData; struct FTalk; }

/**
 * Friends among the residents (docs/friends.md). Every named resident the hero comes to know has a bond with him that grows
 * with greetings, lines of theirs he hears, quests done for them, gifts, his postcards, an hour on a bench, tea at their home
 * and their family's outing. Nothing is shown as a number: a friend waves first from across the street, calls him by the
 * nickname they gave him, comes to sit with him when he sits on a bench and tells him about their day; the ten open-home hosts
 * invite him to tea by letter (he sits at their table: three things to talk about, the tea served, a gift if he brought one);
 * a close friend's family invites him to their weekend outing. The diary's «Люди» pages say what he knows of each.
 *
 * The rules are the engine-free LyudFriends::FModel (Private/People/Friends.*, tests in tools/mannersim/run.sh friends); the
 * data is Content/PeopleData/friends.json and the words Content/Localization/friends (tools/friends/build_friends.py); the
 * town-talk lines (bond = friendly | friend | close, on = friend_*) are tools/barks/lines/13_friends.txt. Saved as "Friends".
 * Gifts and the tea use SLyudCityPage (keys polled, never consumed: 1-9, A/D, E, Esc).
 *
 * A talk with a townsperson (docs/town_talk.md "A talk"): E on a resident who is free for a word opens the same page with
 * their greeting, two to four things to ask about, now and then a question back and a goodbye; a gift is one of the page's
 * entries. Who may be asked and what about is the engine-free LyudTownChat (Private/People/TownChat.*, tests in
 * tools/townchatsim/run.sh); their words are the town-talk sets with on = tc_* (tools/barks/lines/18_townchat.txt), his are
 * Content/Localization/townchat (tools/townchat/build_town_chat.py). The game side is LyudFriendsTalk.cpp. A talk reports
 * the quests' event "talk <resident key>" (docs/quests.md).
 *
 * Console: lyud.Friends.Report, lyud.Friends.Befriend [key] (the nearest townsperson or key: a close friend),
 * lyud.Friends.Tea <host> (an invitation for now), lyud.Friends.Invites (every invitation due now), lyud.Friends.Talk [key]
 * (a talk with the nearest townsperson or key, whatever they are doing), lyud.Friends.TalkReport. Developer mode: "friends".
 */
UCLASS()
class LYUDMILOVSK_API ULyudFriends : public UTickableWorldSubsystem, public ILyudSaveable
{
	GENERATED_BODY()

public:
	ULyudFriends();
	virtual ~ULyudFriends() override;

	static ULyudFriends* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudFriends, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bLoaded; }

	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;

	// --- town talk (ULyudTownTalkSubsystem asks; nothing of it is drawn here)

	/** friendly, friend, close for the sets' bond = ("" a stranger or barely known). */
	const char* BondWord(int32 PersonId) const;
	/** The nickname somebody gave him, in the game's language (empty: none yet). */
	FString NickOf(int32 PersonId) const;
	/** They greeted him / he heard a line of theirs near him: the bond grows a little (once a day / a few a day). */
	void OnGreeted(int32 PersonId);
	void OnHeard(int32 PersonId);
	/** A postcard of his reached them (ULyudLetters: the addressee's id, a person's key or a correspondent's). */
	void OnCardDelivered(const FString& To);

	/** A gift or a talk is offered on the prompt line now (others who use E step aside), a chooser, a talk or the tea has
	 *  the keys. Somebody at work behind a counter is offered softly: the counter's own E comes first. */
	bool IsOffering() const { return OfferedPerson >= 0 && !bOfferSoft; }
	bool HasModeUp() const { return Mode != EMode::None; }

	// --- the diary's «Люди» (SLyudDiary)
	struct FDiaryFriend
	{
		FText Name;
		FText Level;
		TArray<FText> Facts;
	};
	/** Everybody of level 1 and up, the closest first. */
	void DiaryFriends(TArray<FDiaryFriend>& Out) const;
	/** The keys of everybody of MinLevel and up (friends.level.<n>), the closest first (the book club's advice). */
	void FriendKeys(int32 MinLevel, TArray<FString>& Out) const;

	// --- console and developer mode
	FString Report() const;
	bool DevBefriend(const FString& Key);
	bool DevBefriendNearest();
	bool DevTeaNow(const FString& HostId);
	/** Opens the talk with a townsperson (Key empty: the nearest one), whatever they are doing. */
	bool DevTalk(const FString& Key);
	/** What the nearest townsperson would say now: whether they'd talk, the greeting, the topics, the question back. */
	FString DevTalkReport();
	void DevInvites();
	/** Where to stand for a host's tea: in front of the chair at their table (false: no such host). */
	bool TeaStand(const FString& HostId, FTransform& Out) const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	/** The model's state as a flat map ("Friends"). */
	UPROPERTY(SaveGame) TMap<FString, FString> Saved;

private:
	enum class EMode : uint8 { None, Gift, Tea, Talk };
	/** A talk's page: quest lines first (if a quest has some for them), the topics, an answer before their question, their
	 *  question, the goodbye. */
	enum class ETalkStep : uint8 { Opener, Menu, Answer, Ask, Bye };

	void Load();
	int32 Today(float* Hour = nullptr, int32* Weekday = nullptr) const;
	FString KeyOf(int32 PersonId) const;
	int32 PersonOf(const FString& Key) const;
	FText NameOf(const FString& Key) const;
	bool WhoOf(const FString& Key, LyudFriends::FWho& Out) const;
	void Grew(const FString& Key, int32 Added);

	void StepInvites();
	void StepQuests();
	void TickNear(AHeroCharacter* Hero);
	void TickBench(AHeroCharacter* Hero);
	void TickOuting(AHeroCharacter* Hero);
	/** A holiday's gathering: a friend near him there waves and they keep the holiday together (once a day each). */
	void TickFestival(AHeroCharacter* Hero);
	void TickVisits(AHeroCharacter* Hero);
	void TickTeaChair(AHeroCharacter* Hero);
	void TickOffer(AHeroCharacter* Hero, APlayerController* PC);
	void TickMode(APlayerController* PC, float DeltaTime);
	void SetPrompt(int32 PersonId, bool bTalk = false, bool bSoft = false);

	void OpenGift(int32 PersonId, const FString& Key);
	void ShowGift();
	void GiveChosen(int32 Entry);
	void OpenTea(const LyudFriends::FHost& Host);
	void ShowTea();
	void EndTea();
	void PlaceTeaThings(const LyudFriends::FHost& Host);
	void ClearTeaThings();
	void DriveHostBody(float DeltaTime);
	// a talk with a townsperson (LyudFriendsTalk.cpp)
	void LoadTalk();
	/** May PersonId be talked to now: 0 yes, else LyudTownChat::ERefuse. */
	int32 TalkRefusal(int32 PersonId) const;
	bool OpenTalk(int32 PersonId, bool bForce);
	void ShowTalk();
	/** The talk goes on: they stay where they are and look at him; false when they are gone or he walked away. */
	bool TalkKeep(float DeltaTime);
	void TalkKeys(APlayerController* PC);
	void TalkChoose(int32 Entry);
	void TalkAnswer(int32 Answer);
	void TalkNext();
	void TalkBye();
	void EndTalk();
	/** Their line for a moment of the talk, with the names put in; false if they have none. */
	bool TalkLine(const FString& Event, int32 Other, FText& Out);
	bool TalkMayGift() const;
	void OpenPage();
	void ClosePage();
	void EndMode();
	bool SayEvent(int32 PersonId, FName Event);
	void Toast(const FText& Text) const;

	TPimplPtr<LyudFriends::FModel> Model;
	bool bLoaded = false;
	bool bSavedPending = false;

	float StepClock = 0.f;
	float NearClock = 0.f;
	float OfferClock = 0.f;
	float BenchClock = 0.f;
	/** Seated on a city thing: the chair is matched to the hosts' twice a second (strings are built for it), not every frame. */
	float TeaChairClock = 0.f;
	/** Person ids by key, looked up once (the life's ids don't change in a session). */
	mutable TMap<FString, int32> PersonIds;
	/** Who waved first today / mentioned his card (keys; refreshed each day). */
	TSet<FString> WavedToday;
	TSet<FString> CardSaid;
	int32 NearDay = -1;
	/** The friend on the bench beside him (-1 none), when they sat down, how many lines since. */
	int32 BenchFriend = -1;
	bool bBenchSat = false;
	int32 BenchLines = 0;
	float BenchNextLine = 0.f;
	/** The townsperson a gift or a talk is offered to (-1 none); a talk (a townsperson) or a gift (the story's people);
	 *  softly (see IsOffering). */
	int32 OfferedPerson = -1;
	bool bOfferTalk = false;
	bool bOfferSoft = false;
	double RefusedUntil = 0.0;

	// the mode up
	EMode Mode = EMode::None;
	TSharedPtr<SLyudCityPage> Page;
	uint64 ModeFrame = 0;
	TWeakObjectPtr<APlayerController> MoveBlockedBy;
	/** Gift: to whom (a person id, -1 a host at tea), their bond key, the items offered. */
	int32 GiftPerson = -1;
	FString GiftKey;
	TArray<FName> GiftItems;
	int32 GiftPage = 0;
	EMode GiftReturn = EMode::None;
	/** A talk: the data, the talk built when it opened, with whom, what is on the page. */
	TPimplPtr<LyudTownChat::FData> TalkData;
	TPimplPtr<LyudTownChat::FTalk> TalkNow;
	bool bTalkLoaded = false;
	int32 TalkPerson = -1;
	FString TalkKey;
	ETalkStep TalkStep = ETalkStep::Menu;
	FText TalkSaid;
	FText TalkLineText;
	TArray<bool> TalkAsked;
	/** The page's entries: a topic's index, -1 the gift. */
	TArray<int32> TalkEntries;
	int32 TalkSel = 0;
	int32 TalkHeard = 0;
	bool bTalkAsked = false;
	bool bTalkForced = false;
	TArray<FString> TalkOpeners;
	int32 TalkOpener = 0;
	float TalkHold = 0.f;
	/** Tea: the host, the topics asked, the answer shown, whether anything was said. */
	FString TeaHost;
	TArray<bool> TeaAsked;
	int32 TeaShown = -1;
	FText TeaLine;
	bool bTeaBye = false;
	int32 TeaPerson = -1;
	FString TeaNotNowShown;
	/** The chair whose tea was opened while he still sits on it: not opened again until he gets up (a tea ended without a
	 *  topic leaves the invitation due, and an open inventory ends the tea every frame). */
	FString TeaClosedSeat;
	UPROPERTY(Transient) TArray<TObjectPtr<ALyudGoodsItem>> TeaThings;
	UPROPERTY(Transient) TObjectPtr<ANpcBody> HostBody;
};
