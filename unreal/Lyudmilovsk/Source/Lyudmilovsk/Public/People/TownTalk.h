#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "TownTalk.generated.h"

class AActor;
class SWidget;
class UGameViewportClient;
class USkeletalMeshComponent;
struct FTownTalkState;
struct FTownTalkView;

/**
 * What the townsfolk say on their own (docs/town_talk.md): a short line over the head of somebody near the player, in
 * the game's language, picked by who they are and what they do (Content/Dialogue/town_barks.txt: the sets and their
 * moments; the words in Content/Localization/barks/strings_<lang>.json). People greet the player walking past (once a
 * day each), say a line now and then by their job, their age, where they are, the hour, the sky and the holiday, and two
 * of them side by side or talking (the people's life's chat pairs) now and then have a short exchange. The restaurant's
 * waiter and cook speak at the moments of the player's visit (sat down, an order carried from the pass, a dish served,
 * the bill, leaving), read off ALyudRestaurant by name so neither needs the other to compile, or told by SayEvent.
 *
 * The picking is plain C++ (Private/People/TownBarks.cpp; tools/barksim runs it on the town's people). Only the
 * townsfolk speak (lyud.Barks.Residents 1: the story's people too); only people drawn near the camera and in view; one
 * line on one's own every ~9 s at most (lyud.Barks.Every), one exchange at a time, four lines on screen at most. The
 * speaker's mouth moves while the line is up (ANpcBody::Chatter). Lines aren't voiced yet. lyud.Barks 0 turns it off.
 */
UCLASS()
class LYUDMILOVSK_API ULyudTownTalkSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudTownTalkSubsystem();
	virtual ~ULyudTownTalkSubsystem() override;

	static ULyudTownTalkSubsystem* Get(const UObject* WorldContext);

	/**
	 * Somebody says the line of a moment (a set with on = <Event> in town_barks.txt) over Speaker's head now: rest_sit,
	 * rest_carry, rest_serve, rest_bill, rest_bye. PersonId: who they are in the people's life (their name over the line,
	 * the sets' conditions on them; -1 unknown), Role: their job for the sets' job = (waiter, cook). False if no line fits.
	 */
	UFUNCTION(BlueprintCallable, Category = "Town talk")
	bool SayEvent(AActor* Speaker, FName Event, int32 PersonId = -1, FName Role = NAME_None);

	/** Shows one line (its string key: line.town.<scene>.<n>) over an actor's head (tests, scripts). */
	UFUNCTION(BlueprintCallable, Category = "Town talk")
	bool SayLine(AActor* Speaker, const FString& Key, int32 PersonId = -1);

	/** Re-reads town_barks.txt (it is read when the level starts). */
	UFUNCTION(BlueprintCallable, Category = "Town talk")
	bool Reload();

	/** What is on screen and how often each set was said (console lyud.Barks.Report). */
	UFUNCTION(BlueprintCallable, Category = "Town talk")
	FString Report() const;

	/** The townsperson nearest the player who could speak now (-1 none): console tests. */
	int32 NearestSpeaker(AActor*& OutActor) const;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bStarted; }

private:
	TPimplPtr<FTownTalkState> State;
	bool bStarted = false;
	bool bLoaded = false;
	/** Play seconds since the level started (the cooldowns' clock). */
	float Clock = 0.f;

	/** A line up over somebody's head. */
	struct FBubble
	{
		bool bActive = false;
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<USkeletalMeshComponent> Mesh;
		int32 Person = -1;
		FString Key;
		FText Name;
		TArray<FText> Rows;
		float Age = 0.f;
		float Life = 3.f;
		float Alpha = 0.f;
		/** Seen from the camera (traced ten times a second). */
		bool bSeen = true;
		float SeenCheck = 0.f;
		/** The person's own body from the life (not the restaurant's waiter): the line goes if it comes out as somebody else. */
		bool bOwnBody = false;
	};
	static constexpr int32 MaxBubbles = 4;
	FBubble Bubbles[MaxBubbles];

	/** The exchange going on (one at a time): the keys in the book, who says the next one, when. */
	struct FExchange
	{
		bool bActive = false;
		int32 Person[2] = { -1, -1 };
		TWeakObjectPtr<AActor> Actor[2];
		int32 Set = -1;
		int32 Talk = -1;
		int32 Next = 0;
		float NextAt = 0.f;
		int32 First = 0;
	};
	FExchange Exchange;

	/** Per person: the game day they last greeted the player (-1 never), the play second they last spoke. */
	TArray<int32> GreetedDay;
	TArray<float> LastSpoke;
	float ScanTimer = 0.f;
	float NextBarkAt = 6.f;
	float NextGreetAt = 0.f;
	/** No word about the player running past, standing too close or the lamp before this (the manners' notice events). */
	float NextNoticeAt = 0.f;
	/** The life's events: somebody noticed the player (hero_runs, hero_close, hero_lamp: docs/town_manners.md). */
	UFUNCTION()
	void OnLifeEvent(int32 PersonId, FName Event, int32 Other);
	int32 Today = 0;

	// the restaurant's moments, read off ALyudRestaurant by its properties' names (no link to its code)
	TWeakObjectPtr<AActor> Restaurant;
	float RestaurantLook = 0.f;
	float RestaurantPoll = 0.f;
	bool bRestPushed = false;
	bool bHeroWasSeated = false;
	bool bAteThisVisit = false;
	int32 LastHeroDishes = 0;
	int32 LastOnTray = 0;
	float SitLineAt = -1.f;
	float WaiterCarryAt = -1.f;
	float LastServeLine = -100.f;
	TWeakObjectPtr<AActor> BillAnnounced;

	// the holiday on now (ULyudFestivalSubsystem by name), the sky
	float FestivalLook = 0.f;
	TWeakObjectPtr<AActor> Sky;

	// the lines over the heads
	TSharedPtr<FTownTalkView> View;
	TSharedPtr<SWidget> Overlay;
	TWeakObjectPtr<UGameViewportClient> Viewport;

	void Start(UWorld& InWorld);
	void Stop();
	/** The moment for the sets: the game's clock, the sky, the weather (the holiday is looked up in Tick). */
	void UpdateNow();
	/** Twice a second: greetings, a line on one's own, an exchange starting. */
	void Scan();
	/** Two people side by side or talking, in view, who have an exchange that fits them: starts it. */
	bool StartExchange(const FVector& Cam, const FVector& Look);
	void TickExchange();
	void TickRestaurant(float DeltaTime);
	/** The waiter or cook on shift at the restaurant (the life's duties), -1 none. */
	int32 StaffPerson(FName Role) const;
	/** A moment's line said by Speaker (SayEvent, and the restaurant's moments read off it). */
	bool SayMoment(AActor* Speaker, FName Event, int32 PersonId, FName Role);
	/** Every frame: where the lines are on screen, how strong (fading in and out, with the distance, behind walls). */
	void UpdateView(float DeltaTime);
	/** Puts a line up over Actor's head; false if its words are missing or there's no room. */
	bool Show(AActor* Actor, int32 Person, const FString& Key);
	/** Whose actor shows a person now (the life's body, a club actor), null if not drawn. */
	AActor* ActorOf(int32 Person) const;
	/** Actor still draws Person (a townsperson's body goes back to the pool and may come out as somebody else). */
	bool Still(const AActor* Actor, int32 Person) const;
	bool Busy(int32 Person) const;
	bool CanSee(const FVector& Cam, const FVector& Head, const AActor* Actor) const;
	FVector HeadOf(const FBubble& B) const;
	void CountSaid(int32 Set);
	void ClearAll();
};
