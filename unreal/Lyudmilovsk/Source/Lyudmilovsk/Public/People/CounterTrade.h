#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudInventory.h"
#include "CounterTrade.generated.h"

/** One person who sells over a counter while at work (Content/PeopleData/counters.json). */
struct FLyudCounter
{
	/** people.json key of the seller (girl_b: Olya, the librarian). */
	FString Person;
	/** The activity (activities.json) during which they sell: only then, and only once they are at their place ("doing"). */
	FString Activity;
	/** The shop's title key; {0} is the seller's name. */
	FString TitleKey = TEXT("ui.shop.title");
	/** How close the hero must be (cm) and look at them. */
	float Reach = 320.f;
	TArray<FLyudShopGood> Goods;
};

/**
 * The town's people who sell things at their work: the librarian at the library's desk (the bird guide, docs/bird_guide.md),
 * later the shop assistants. Data: Content/PeopleData/counters.json, a list of {person, activity, title, reach_cm, goods: [{item,
 * price, count}]}. When the seller is doing that activity at their place and the hero stands near and looks at them, the prompt
 * "T — trade" shows and T opens ULyudInventoryUI's shop with the goods. Reread at every level start.
 */
UCLASS()
class LYUDMILOVSK_API ULyudCounterTradeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudCounterTradeSubsystem* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudCounterTradeSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return Counters.Num() > 0; }

	const TArray<FLyudCounter>& GetCounters() const { return Counters; }
	/** Is this counter's seller at work now (for checks)? Where they are (cm). */
	bool IsOpen(int32 Counter, FVector* Where = nullptr) const;
	/** Opens a counter's shop whatever the distance (console: lyud.Trade.Open <person>). */
	bool OpenCounter(int32 Counter);

private:
	TArray<FLyudCounter> Counters;
	int32 Prompted = -1;

	void Load();
	void SetPrompted(int32 Counter);
};
