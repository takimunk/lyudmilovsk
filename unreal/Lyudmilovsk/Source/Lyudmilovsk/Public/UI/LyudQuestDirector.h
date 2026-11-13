#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudQuestDirector.generated.h"

class ALyudGoodsItem;
class UAudioComponent;

/**
 * The staging the quests need in the world (docs/quests.md), read from Content/UI/quest_scenes.json, so a new quest is
 * data and words, not code:
 *
 *  - "pickups": an item lying somewhere while a quest is on certain stages (the bird guide on the library's desk). The hero
 *    looks at it from near by, E takes it (the hand reaches for it: UHeroReachComponent::Take) and it is in the pocket.
 *    Taken once ("pickup.<id>" in ULyudQuests::Flags); not put out while the hero already has one.
 *  - "listeners": people who notice a bird that calls beside them. The hero, as a bird, comes down within reach of one
 *    and calls (F): the quests get "bird_call <Species>.<Body>" and the person says the lines of the first reaction that
 *    fits (the species, a quest's stage, called from the air), over their head and aloud, looking at the bird.
 *  - "remarks": somebody says a line or two of their own accord when the hero comes near while a quest is on certain stages
 *    (the grocer about the rowan he came for). Once each ("remark.<id>" in ULyudQuests::Flags); never while a talk is on.
 *  - "feeder_visit <Species>" is reported when a bird comes down onto one of the hero's feeders while he is within 60 m.
 *
 * Say() is for any scene: somebody says lines one after another over their head (ULyudTownTalkSubsystem's bubbles), each
 * with its voice (LyudText::LoadVoice) and their lips moving.
 *
 * E is polled and never consumed. Console: Quest.Scenes (what is staged now), Quest.Say <person key> <line key>...
 */
UCLASS()
class LYUDMILOVSK_API ULyudQuestDirector : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudQuestDirector* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudQuestDirector, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bStarted; }

	/**
	 * PersonKey (people.json) says Keys (line.<speaker>.<scene>.<n>, with or without "line.") one after another, looking at
	 * LookAt if it is given. False if they are not to be seen now or already speaking for the director.
	 */
	bool Say(const FString& PersonKey, const TArray<FString>& Keys, const FVector* LookAt = nullptr);

	/** What is staged now, one line each (console Quest.Scenes). */
	FString Report() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	struct FPickup
	{
		FName Id;
		FName Item;
		FName Quest;
		TArray<FName> Stages;
		/** Where it lies (world cm): dropped from DropCm above onto whatever is under it. */
		FVector At = FVector::ZeroVector;
		float DropCm = 150.f;
		float Yaw = 0.f;
		TWeakObjectPtr<ALyudGoodsItem> Actor;
		bool bTaking = false;
	};

	struct FReaction
	{
		/** None: any species. */
		FName Species;
		/** None: whatever the quests are at. */
		FName Quest;
		FName Stage;
		/** Called on the wing near by (true), or sitting within reach (false). */
		bool bAir = false;
		TArray<FString> Lines;
	};

	struct FListener
	{
		FString Person;
		float WithinCm = 1400.f;
		float AirWithinCm = 4500.f;
		TArray<FReaction> Reactions;
	};

	struct FRemark
	{
		FName Id;
		FString Person;
		FName Quest;
		TArray<FName> Stages;
		float WithinCm = 600.f;
		TArray<FString> Lines;
	};

	struct FSpeech
	{
		int32 Person = -1;
		TArray<FString> Keys;
		int32 Next = 0;
		double NextAt = 0.0;
		bool bLook = false;
		FVector LookAt = FVector::ZeroVector;
		TWeakObjectPtr<UAudioComponent> Voice;
	};

	void Load();
	void HandleBirdCall(FName Species, const FVector& At, bool bInAir);
	void TickPickups(class APlayerController* PC, bool bSlow);
	void TickFeeders(const class APawn* Pawn);
	void TickRemarks(const class APawn* Pawn);
	void TickSpeech();
	void DropPickup(FPickup& P);
	AActor* PersonActor(int32 PersonId) const;

	TArray<FPickup> Pickups;
	TArray<FListener> Listeners;
	TArray<FRemark> Remarks;
	TArray<FSpeech> Speeches;
	/** Birds on the hero's feeders at the last look (bird life ids): a visit is told once, when a bird comes down. */
	TSet<int32> OnTrays;
	bool bStarted = false;
	bool bPrompt = false;
	/** The pickup the prompt is written for (not rewritten every frame). */
	FName PromptFor;
	/** E is the talk's while the hero faces somebody or is talking. */
	TWeakObjectPtr<class ANpcTalkDirector> TalkDirector;
	float SlowTimer = 0.f;
	FDelegateHandle CallHandle;
};
