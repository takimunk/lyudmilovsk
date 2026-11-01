#pragma once

#include "CoreMinimal.h"
#include "Food/LyudHandUse.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudSmoking.generated.h"

class ACigaretteButt;
class ACigaretteItem;
class ACigarettePack;
class ALighterItem;
class ALyudSmokeFx;

/** What of smoking outlives the level and goes into the save ("Smoking"): the started pack lying on the club's terrace. The hero's
 *  own packs are inventory items ("cigarettes", one per pack; the open one's share left in ULyudInventory::Started). */
UCLASS()
class LYUDMILOVSK_API ULyudSmokingState : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static ULyudSmokingState* Get(const UObject* WorldContext);
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	void Reset() { bTerraceTaken = false; }

	UPROPERTY(SaveGame) bool bTerraceTaken = false;
	/** How many the terrace's pack still has. */
	static constexpr int32 TerraceLeft = 7;
};

/**
 * Cigarettes (docs/smoking.md). "Взять в руку" on the pack (or Inventory.Equip cigarettes) takes one out: the pack comes up, the
 * lid opens, a cigarette is shaken up and taken with the lips, the pack goes back, the hand takes the cigarette from the lips.
 * Then, with the cigarette between the fingers (ULyudHandUse carries it):
 *   unlit  LMB      to the lips; it stays there while the hand fetches the lighter, lights it, puts the lighter away and takes
 *                   the cigarette back (no lighter: "nothing to light it with")
 *          RMB / Q  back into the pack
 *   lit    click    to the lips and down;  hold: a drag (the ember brightens, the paper burns down, breath out after);
 *                   a drag held too long makes the hero cough (his voice, smoke, the hand down)
 *          RMB / Q  put out and flicked away: a butt on the ground
 * Smoke: a thin stream from the burning tip, a breath out of the mouth after a drag (ALyudSmokeFx's fixed pool).
 * One started pack with a lighter lies on the club's terrace (E takes them).
 * Console: lyud.Smoke.Give, lyud.Smoke.Take, lyud.Smoke.Out, lyud.Smoke.State; the button: lyud.Hand.Press 1|0, lyud.Hand.Click.
 * Log lines "SMOKE ..." for tests.
 */
UCLASS()
class LYUDMILOVSK_API ULyudSmoking : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudSmoking* Get(const UObject* WorldContext);
	static const FName PackId;
	static const FName LighterId;

	/** A cigarette is in the hand or the lips, or one is being taken out or lit: the right mouse button is the cigarette's. */
	static bool HasHand(const UObject* WorldContext);

	/** Cigarettes left in the hero's open pack (0 with no pack). */
	int32 PackLeft() const;
	/** Starts taking one out (as the inventory's "take in hand" does); false if there is none or the hand is busy. */
	bool TakeOut();
	/** RMB: put out and away (lit) or back into the pack (unlit). */
	void PutOut();
	bool IsLit() const { return bLit; }
	FString DebugState() const;
	/** Takes the terrace's pack next tick as E would, from anywhere (checks). */
	void ForcePick() { bForcePick = true; }

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudSmoking, STATGROUP_Tickables); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	enum class EStep : uint8
	{
		None,
		PackUp,        // the pack in the hand, the lid opening, a cigarette shaken up, then to the lips
		PackDown,      // the lips have it; the pack goes back to the pocket
		FromMouth,     // the hand comes up and takes the cigarette from the lips
		Held,          // between the fingers (ULyudHandUse: clicks and drags)
		ToMouthToLight,// left in the lips, the hand goes down
		LighterUp,     // the lighter comes up to the tip
		Lighting,      // the flame at the tip
		LighterDown,   // the lighter goes back
	};

	void Go(EStep Next);
	FLyudHandThing CigaretteThing(bool bFromMouth);
	void HoldCigarette(bool bFromMouth);
	void HoldPack();
	void HoldLighter();
	/** The cigarette in the lips: where the hand's cigarette is when it is at the lips (look frame), and placing it there. */
	FTransform MouthInLook() const;
	FTransform LookFrame() const;
	void PlaceMouthCig(float DeltaTime);
	void Smoke(float DeltaTime);
	void Cough();
	void Exhale(float Amount);
	void ThrowButt(const FTransform& From, bool bLitEnd);
	void TickButts(float DeltaTime);
	void TakeOneFromPack();
	void ReturnOneToPack();
	void Sound(FName Pool, float Volume = 1.f);
	void Say(const TCHAR* Key, float Seconds);
	ALyudSmokeFx* Fx();
	void SpawnTerrace();
	void TickTerrace();

	EStep Step = EStep::None;
	float StepTime = 0.f;
	bool bLit = false;
	/** The cigarette's own part left (1 whole .. 0 at the filter); ULyudHandUse's Left follows it while it is in the hand. */
	float Left = 1.f;
	float Glow = 0.f;
	float DragTime = 0.f;
	float ExhaleIn = -1.f;
	float ExhaleAmount = 0.f;
	float StreamTimer = 0.f;
	float CoughTime = -1.f;
	float SayLeft = 0.f;
	bool bWasAtLips = false;
	bool bDrag = false;
	/** Our own moves put a thing away (the pack after the lips took the cigarette): its OnPutAway is ours, not the player's. */
	bool bInternal = false;
	bool bAtLipsEvent = false;
	FTransform LastCigXf;
	/** From where the lips took it (the pack) to where it rests in them, over a moment. */
	FTransform MouthFrom;
	float MouthBlend = 1.f;

	TWeakObjectPtr<ACigarettePack> Pack;
	TWeakObjectPtr<ACigaretteItem> Cig;
	TWeakObjectPtr<ACigaretteItem> MouthCig;
	TWeakObjectPtr<ALighterItem> Lighter;
	TWeakObjectPtr<ALyudSmokeFx> SmokeFx;

	struct FFlying
	{
		TWeakObjectPtr<ACigaretteButt> Butt;
		FVector Pos, Vel;
		FRotator Spin;
		float Smoke = 0.f;
		bool bLanded = false;
		float Age = 0.f;
	};
	TArray<FFlying> Butts;

	TWeakObjectPtr<ACigarettePack> TerracePack;
	TWeakObjectPtr<ALighterItem> TerraceLighter;
	bool bTerracePrompt = false;
	bool bForcePick = false;
};
