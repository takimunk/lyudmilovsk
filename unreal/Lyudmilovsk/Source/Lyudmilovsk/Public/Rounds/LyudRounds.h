#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "Tickable.h"
#include "LyudRounds.generated.h"

class AActor;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;
namespace LyudRounds { struct FRound; struct FData; }

/** A stop of the round under way, for the diary's map (SLyudMapView): where, and whether it is served yet. */
struct FLyudRoundMark
{
	FVector At = FVector::ZeroVector;
	bool bDone = false;
	bool bWell = false;
};

/**
 * The bicycle rounds (docs/rounds.md, backlog item 17): two small jobs ridden through the town's addresses.
 *
 * The paper round: from 6:00 to 7:30 the morning's bundle waits on kiosk 1's step (E takes it: the papers of the day's
 * terrace, T1..T4 in turn, plus a few spare). The subscribers' doors are on the diary's map; the left mouse button throws a
 * paper (on foot or riding, the throw carries the bicycle's speed) and where it lands is judged: on the step, near it, in the
 * road. Paid when the last paper is gone or the round ends (8:00 the deadline, a bonus for all in time and well placed); a good
 * round earns its terrace's stamp of the set "rounds".
 *
 * The post round: Tuesday to Saturday from 9:00 to 12:00 the old postman (the town's retired postman, a resident picked by
 * tools/rounds/rounds.py) gives the day's parcels at his door. At each flat E rings; whoever is in opens after a moment, says
 * a word by their manner and takes the parcel, nobody home and a notice goes in the box. Paid at the last flat.
 *
 * Data: Content/City/rounds.json (tools/rounds/rounds.py); rules: Private/Rounds/RoundsModel.h (tools/roundsim). Prompts go
 * through ULyudInventoryUI::SetPrompt, E through ANpcTalkDirector::UsePressed (never consumed). Saved ("Rounds"): the days the
 * rounds were done, the totals. Console lyud.Rounds paper|post|end|status; developer mode group "rounds".
 */
UCLASS()
class LYUDMILOVSK_API ULyudRounds : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudRounds();
	virtual ~ULyudRounds() override;

	static ULyudRounds* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudRounds, STATGROUP_Tickables); }

	/** Starts a round now wherever he is (bForce: whatever the hour or the day; the console and developer mode). */
	bool StartPaper(bool bForce);
	bool StartPost(bool bForce);
	/** Ends the round under way: paid for what was done. */
	void EndRound();
	bool IsActive() const;
	bool IsPaper() const;
	/** The stops of the round under way (empty when none), in their order. */
	void GetMarks(TArray<FLyudRoundMark>& Out) const;
	/** Where the round begins (kiosk 1's step; the postman's door). */
	FVector BundleAt() const;
	FVector PostmanDoorAt() const;
	FString Report() const;

	/** Day numbers (days since the game began) of the rounds done, and the totals. */
	UPROPERTY(SaveGame) int32 PaperDay = -1;
	UPROPERTY(SaveGame) int32 PostDay = -1;
	UPROPERTY(SaveGame) int32 PapersDelivered = 0;
	UPROPERTY(SaveGame) int32 ParcelsHanded = 0;
	UPROPERTY(SaveGame) int32 RoublesEarned = 0;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	TPimplPtr<LyudRounds::FData> Data;
	TPimplPtr<LyudRounds::FRound> Round;
	bool bLoaded = false;
	bool LoadData();
	int32 Today() const;
	double HourNow() const;
	/** HourNow on the clock of the day the round began (over 24 after midnight). */
	double RoundHour() const;
	int32 RoundTerrace = 0;
	int32 RoundDay = 0;

	// the papers in flight and on the steps: a few meshes on one actor, reused
	struct FFlight
	{
		FVector At = FVector::ZeroVector;
		FVector Vel = FVector::ZeroVector;
		FRotator Spin = FRotator::ZeroRotator;
		float Age = 0.f;
		bool bFlying = false;
		bool bLying = false;
	};
	static constexpr int32 MaxFlights = 6;
	FFlight Flights[MaxFlights];
	UPROPERTY(Transient) TObjectPtr<AActor> Holder;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> PaperMeshes;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> PaperMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PaperMat;
	int32 NextFlight = 0;
	void Throw();
	void TickFlights(float DeltaTime);
	void Land(int32 Index, const FVector& Where);
	bool EnsureHolder();

	// the doors: ringing and waiting
	int32 RingingEntry = -1;
	float RingWait = 0.f;
	int32 Prompted = -2;
	FText PromptText;
	void TickDoors();
	void Answer(int32 Entry);
	void SetPrompt(int32 What, const FText& Text);

	void Finish(bool bInTime);
	void ResetAll();
	FDelegateHandle NewGameHandle;
};
