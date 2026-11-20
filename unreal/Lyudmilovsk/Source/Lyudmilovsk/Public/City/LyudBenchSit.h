#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "LyudBenchSit.generated.h"

class AActor;
class AHeroCharacter;
class USceneComponent;

/**
 * The hero sits down on the town's public benches (the promenade, the park, the squares: the "bench" spots of
 * Content/City/public_spaces.json, the same seats the townsfolk use) and, seated, reads a library book (docs/books.md).
 *
 * Standing near a free bench seat in front of him: "E — sit on the bench". Seated: "E — read" when he has a library book
 * on loan (ULyudBookScreen::OpenReader), any step or Space gets him up (AHeroCharacter). The seat is reserved for him
 * in ULyudPublicSpotsSubsystem while he sits, so nobody sits on his lap; the people beside him notice (hero_sits).
 * Steps aside for the city things' prompts and the NPC talk. Console: lyud.Bench.Sit (the nearest bench seat within 30 m).
 */
UCLASS()
class LYUDMILOVSK_API ULyudBenchSit : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudBenchSit* Get(const UObject* WorldContext);

	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudBenchSit, STATGROUP_Tickables); }

	/** Sits him on the nearest free bench seat within MaxCm of him (console, developer mode). */
	bool SitNearest(float MaxCm);
	bool IsOnBench() const { return !SpotId.IsNone(); }

private:
	struct FSeat
	{
		FName Spot;
		int32 Index = 0;
		FVector At = FVector::ZeroVector;  // the cushion's middle, cm
		float Yaw = 0.f;
		float Height = 45.f;
	};
	TArray<FSeat> Seats;
	bool bSeatsRead = false;
	/** The seat in front of him now (index into Seats), -1: none. */
	int32 Offered = -1;
	double NextSearch = 0.0;
	/** Where he sits: the spot reserved, its seat. */
	FName SpotId;
	int32 SeatIndex = -1;
	TWeakObjectPtr<AHeroCharacter> SeatedHero;
	UPROPERTY(Transient)
	TObjectPtr<AActor> Anchor;
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> AnchorRoot;
	/** What the prompt says now (0 none, 1 sit, 2 read): the prompt is set only when it changes. */
	int32 PromptShown = 0;

	void ReadSeats();
	int32 FindSeat(const AHeroCharacter* Hero, const FVector& ViewDir, float MaxCm, bool bInFront) const;
	bool SitOn(int32 Seat, AHeroCharacter* Hero);
	void Release();
	void SetPrompt(int32 What);
};
