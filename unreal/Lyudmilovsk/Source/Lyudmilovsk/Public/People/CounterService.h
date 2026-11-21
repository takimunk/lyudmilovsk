#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "CounterService.generated.h"

class AHeroCharacter;

/**
 * Counters with real service (docs/counters.md, backlog item 27). Buying at a counter is a little scene: the one on shift
 * turns to the hero and nods (the hello itself is ULyudCityThings::Greet's, or ours when a shop opens without it: by the
 * hour, by whether they know him, by friendship; tools/barks/lines/09_counters.txt), watches him while he chooses, puts
 * what he bought on the counter between them (ULyudFoodSubsystem asks HandoverPoint; the hand reaches for it) and says so;
 * a toast counts the money: the note he paid with and the change (LyudCounter, Private/People/CounterModel.h).
 *
 * The lines: the people's life keeps a queue at every counter (tools/town/counters.py, FWorld::StepQueues). The hero standing
 * at a queue's spot holds it (FWorld::SetHeroAt, told here four times a second), so the townsfolk line up behind him and
 * never step in front; when he joins a line behind others a toast says how many are ahead. Near him the one on shift calls
 * "next" when the front changes and a newcomer asks who is last.
 *
 * Bodies drawn outside the people's life (the station halls' barista, docs/front_door.md) are registered here
 * (RegisterBody), so they serve and are found like the town's own; whoever opens a shop may name its counter first
 * (OpenedAt: "hall:aton"), and the counter's purchases are told to whoever listens (ULyudFrontDoor: the regulars' usual).
 *
 * Console: lyud.Counter status | crowd [n]. Developer mode: "counters" (crowd, status).
 */
UCLASS()
class LYUDMILOVSK_API ULyudCounterService : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudCounterService* Get(const UObject* WorldContext);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudCounterService, STATGROUP_Tickables); }

	/** Where the Index-th thing he just bought is put down: on the counter between the one serving him and him (world cm).
	 *  False when nobody serves him now (the shop shut over 3 s ago, or nobody on shift stands near): hand it over as before. */
	bool HandoverPoint(int32 Index, FVector& Out) const;
	/** The same for a given server (a direct sale: the usual), whether or not a shop is open. */
	bool HandoverPointFrom(const AActor* ServerBody, int32 Index, FVector& Out) const;
	/** Somebody greeted him with this body (ULyudCityThings::Greet, a hall's barista): no second hello when the shop opens. */
	void NoteGreeted(const AActor* Body);

	/** A body drawn outside the people's life (PersonId: who they are in it, -1 nobody); bServer: they serve at a counter. */
	void RegisterBody(AActor* Body, int32 PersonId, bool bServer);
	void UnregisterBody(AActor* Body);
	/** Whoever is drawn for PersonId now: the town's body or a registered one (null: not drawn). */
	AActor* BodyOf(int32 PersonId) const;
	/** The next shop that opens is at this counter, served by this body (the station halls; the town's shops find theirs). */
	void OpenedAt(const FString& Counter, AActor* ServerBody, int32 ServerPid);
	/** The counter of the shop open now ("" none or unnamed). */
	const FString& OpenCounter() const { return Counter; }
	/** The toast that counts the money: Spent out of a purse of Purse roubles. */
	void AnnounceChange(int32 Spent, int32 Purse) const;

	/** The lines near the hero, for the log. */
	FString Status() const;
	/** Sends up to Count townsfolk to the line nearest the hero (within 40 m): how many went. */
	int32 Crowd(int32 Count);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	struct FExtra
	{
		TWeakObjectPtr<AActor> Body;
		int32 Pid = -1;
		bool bServer = false;
	};

	void TickHold(AHeroCharacter* Hero);
	void TickShop(AHeroCharacter* Hero, float DeltaTime);
	void TickLines(AHeroCharacter* Hero);
	/** The nearest body on a staff duty (or a registered server) within MaxCm of Near: its id (-1: a registered one with no
	 *  id, or none) and the body (null: none). */
	int32 FindServer(const FVector& Near, float MaxCm, AActor*& OutBody) const;
	AActor* ShownBody(int32 PersonId) const;
	double Now() const;

	float HoldClock = 0.f;
	float LookClock = 0.f;
	float LineClock = 0.f;
	bool bHeldOn = false;

	// the scene at the counter
	bool bShopWas = false;
	int32 Server = -1;
	TWeakObjectPtr<AActor> ServerBody;
	int32 RublesAtOpen = 0;
	double ClosedAt = -100.0;
	TWeakObjectPtr<const AActor> GreetedBody;
	double GreetedAt = -1000.0;
	FString Counter;
	FString PendingCounter;
	TWeakObjectPtr<AActor> PendingBody;
	int32 PendingPid = -1;
	double PendingAt = -100.0;
	TArray<FExtra> Extras;

	// the lines: who stood at each spot last look (flattened by LineStart), the front's person per line
	TArray<int32> LineStart;
	TArray<int32> LastAt;
	TArray<int32> LastFront;
	int32 HeroLine = -1;
	double NextNextBark = 0.0;
	double NextJoinBark = 0.0;
};
