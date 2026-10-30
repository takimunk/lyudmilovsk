#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudBirdGuide.generated.h"

class SWidget;

/** One live bird as the guide shows it (map metres, as ULyudQuests::Frame). */
struct FLyudGuideBird
{
	int32 Id = -1;
	FName Species;
	FVector2D At = FVector2D::ZeroVector;
	/** Map metres a second (the frame's axes). */
	FVector2D Velocity = FVector2D::ZeroVector;
	bool bFlying = false;
	bool bOnWater = false;
	bool bAsleep = false;
	/** The species' printed colour (its most colourful plumage tone) and size (body length, cm). */
	FLinearColor Color = FLinearColor::Black;
	float LengthCm = 20.f;
};

/**
 * The ornithological guide of Lyudmilovsk (docs/bird_guide.md): a book sold at the Academgorodok library's desk
 * (ULyudCounterTradeSubsystem, Content/PeopleData/counters.json). Its map shows the live birds, but only within
 * lyud.BirdGuide.Radius metres of the hero (they move, fly off, come back). Once it is in the pocket, the diary's
 * map gets the same birds as a layer that the map's sticky note switches on and off.
 *
 * This keeps what outlives the level and goes into the save ("BirdGuide"): whether the map's layer is on.
 */
UCLASS()
class LYUDMILOVSK_API ULyudBirdGuide : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static ULyudBirdGuide* Get(const UObject* WorldContext);
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The guide's item id. */
	static const FName ItemId;
	/** Does the hero have the guide? */
	static bool IsOwned(const UObject* WorldContext);
	/** How far round the hero the guide sees birds (map metres). */
	static float Radius();
	/** The live birds within Radius() of the hero, nearest first (empty without the bird life). */
	static TArray<FLyudGuideBird> BirdsNearHero(const UObject* WorldContext, FVector2D* HeroAt = nullptr);

	/** The diary map's bird layer (shown only while the guide is owned). */
	UPROPERTY(SaveGame)
	bool bMapLayer = true;

	bool ShowsOnMap(const UObject* WorldContext) const { return bMapLayer && IsOwned(WorldContext); }
};

/**
 * The guide open in front of the hero: its map page round him and the list of the species near (B, or "Open" on the
 * diary's Things page; B or Esc shuts it). Like a trader's screen it does not pause the game: walking and looking stop
 * and the birds go on moving on the page.
 */
UCLASS()
class LYUDMILOVSK_API ULyudBirdGuideScreen : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudBirdGuideScreen* Get(const UObject* WorldContext);

	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudBirdGuideScreen, STATGROUP_Tickables); }

	void Open();
	void Close();
	bool IsOpen() const { return Page.IsValid(); }

private:
	TSharedPtr<SWidget> Page;
	uint64 OpenedFrame = 0;
	void Lock(bool bLock);
};
