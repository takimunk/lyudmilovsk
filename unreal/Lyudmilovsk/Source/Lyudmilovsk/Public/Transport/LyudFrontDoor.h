#pragma once

#include "CoreMinimal.h"
#include "LyudSave.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "Tickable.h"
#include "LyudFrontDoor.generated.h"

class AActor;
class AHeroCharacter;
class ALyudSky;
class UPointLightComponent;
class UStaticMeshComponent;
class UWidgetComponent;
namespace LyudFrontDoor { class FLostFound; struct FUsual; struct FTravelCard; }

/**
 * The Aton station as the club's front door (docs/front_door.md, backlog item 40). What the halls (ALyudStationHall) show
 * and the hero does there, kept with the save ("FrontDoor"):
 *  - the regulars' usual: what he bought at a hall's counter three times is "the usual" there; E at the barista buys it
 *    straight over the counter (the cup set down for his hand, the change counted, the barista's word);
 *  - the travel card (item travel_card): the Aton attendant gives it; the punch on every hall's desk stamps it, a stamp per
 *    station, the set «Все станции» of the collections (ULyudCityThings::GiveStamp "stations/<id>"), paid with a badge;
 *  - the lost and found at Aton: residents' things turn up on the shelf (0..2 a day, seeded), the attendant says whose; the
 *    hero carries one to its owner (E by them, in town or in a hall) for a word and a small present;
 *  - at dusk the club's sign over the forecourt lights up with the station's lamps (sun under 3 degrees).
 * The rules are LyudFrontDoor (Private/Transport/FrontDoor.*, tools/transportsim/run.sh board).
 * Console: lyud.FrontDoor status | card | stamps | lost | usual | dusk 0|1|-1. Developer mode: "frontdoor".
 */
UCLASS()
class LYUDMILOVSK_API ULyudFrontDoor : public UTickableWorldSubsystem, public ILyudSaveable
{
	GENERATED_BODY()

public:
	ULyudFrontDoor();
	virtual ~ULyudFrontDoor() override;

	static ULyudFrontDoor* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudFrontDoor, STATGROUP_Tickables); }

	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;

	// --- the halls ask
	/** The usual at a counter ("hall:<station>"): NAME_None till he has bought one thing there three times. */
	FName UsualAt(const FString& Counter, int32* OutTimes = nullptr) const;
	/** He buys his usual straight over the counter at Price: false if he can't pay. */
	bool BuyUsual(const FString& Counter, FName Item, int32 Price, AActor* ServerBody, int32 ServerPid);
	bool HasCard() const;
	/** The attendant (From) hands him the travel card. */
	void GiveCard(AActor* From, int32 FromPid);
	/** The punch at station St: a new stamp (false: no card, or this station's stamp is there). */
	bool Punch(int32 St, const FVector& Where);
	bool HasStamp(int32 St) const;
	/** The lost things on Aton's shelf now (their item ids, for the shelf's look). */
	void ShelfItems(TArray<FName>& Out) const;
	/** He takes the first thing from the shelf; the attendant (From) says whose it is. False: the shelf is empty. */
	bool TakeFromShelf(AActor* From, int32 FromPid);
	/** Dusk: the station's lamps and the club's sign are on (the sun under 3 degrees, or forced from the console). */
	bool IsDusk() const { return bDusk; }
	/** The station whose hall is the front door (Aton), -1 before the network is up. */
	int32 FrontStation() const;

	// --- console and developer mode
	FString Report() const;
	void DevGiveCard();
	void DevStampAll();
	void DevLost();
	void DevUsual();
	void DevDusk(int32 Mode) { DuskForced = Mode; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	/** The rules' state as strings ("FrontDoor"). */
	UPROPERTY(SaveGame) TMap<FString, FString> Saved;

private:
	bool EnsureOwners();
	int32 Today() const;
	void OnBought(FName Id, int32 Count);
	void TickReturn(AHeroCharacter* Hero);
	void TickDusk(float DeltaTime);
	void BuildSign();
	FText ItemName(FName Id) const;
	void Toast(const FText& Text) const;
	void Say(AActor* Body, const FString& Prefix, int32 Count, int32 Pid) const;

	TPimplPtr<LyudFrontDoor::FLostFound> Lost;
	TPimplPtr<LyudFrontDoor::FUsual> Usual;
	TPimplPtr<LyudFrontDoor::FTravelCard> Card;
	bool bOwners = false;
	bool bSavedPending = false;
	int32 Day = -1;
	float ReturnClock = 0.f;
	float DuskClock = 0.f;
	bool bDusk = false;
	int32 DuskForced = -1;
	/** The owner offered a thing now (-1 none): their person id, and which carried thing. */
	int32 OfferPid = -1;
	int32 OfferIndex = -1;
	TMap<FString, int32> PidCache;

	// the club's sign over the forecourt (built once near the club's route board)
	UPROPERTY(Transient) TObjectPtr<AActor> SignActor;
	UPROPERTY(Transient) TObjectPtr<UWidgetComponent> Sign;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> SignLights;
	bool bSignTried = false;
	TWeakObjectPtr<ALyudSky> Sky;
};
