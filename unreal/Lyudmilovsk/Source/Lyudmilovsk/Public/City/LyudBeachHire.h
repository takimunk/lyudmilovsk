#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "LyudBeachHire.generated.h"

class AActor;
class AHeroCharacter;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * The beach set hired at the promenade's rental unit (docs/city_interiors.md "The rental's beach set"): a deck chair and a
 * parasol the hero sets up on the sand himself.
 *
 * The hire is the town things' stay "beach_set" (the act rent_beach_set on the rental's deck chair on show; it holds for
 * the day). While it holds and he stands still on the beach (near the beach's own places of public_spaces.json, on the
 * sand's level): "E — set up the deck chair" puts the set a step in front of him, facing the way he looks. By the set:
 * "E — sit in the deck chair" (AHeroCharacter::SitOn; any step or Space gets him up). Twelve metres away from it the first
 * prompt comes back and moves the set to him. A while in it on a clear day between nine and seven is a sunbath: the quest
 * event "do sunbathe", a toast and a diary note, once a day. When the hire is over the attendant has taken the set back
 * (it goes; a toast if he is near). Nothing is saved: after a load the set is folded in his hands again while the hire holds.
 *
 * The meshes are the public spaces' kit's own lounger and umbrella (CityKit::BuildProp, /Game/City/M_CityProp), built once.
 * Steps aside for the town things' prompts, the benches and open screens. Console: lyud.Beach.Hire [days], lyud.Beach.Set,
 * lyud.Beach.Sit; developer mode: interiors.beach_set.
 */
UCLASS()
class LYUDMILOVSK_API ULyudBeachHire : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudBeachHire* Get(const UObject* WorldContext);

	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudBeachHire, STATGROUP_Tickables); }

	/** Is the set hired today (the stay "beach_set")? */
	bool IsHired() const;
	/** Is the set standing on the sand? */
	bool IsSetUp() const { return Set != nullptr; }
	bool IsInChair() const { return SeatedHero.IsValid(); }
	/** Is he on the beach (near one of its places, on the sand's level)? */
	bool IsOnBeach(const AHeroCharacter* Hero) const;
	/** Sets the set up a step in front of him (console, developer mode: anywhere, hired or not); false: no hero, or seated. */
	bool SetUpHere(AHeroCharacter* Hero);
	/** Sits him in the chair of the set that stands (console, developer mode). */
	bool SitInChair(AHeroCharacter* Hero);
	/** The set goes (the attendant took it back). */
	void TakeBack();

private:
	UPROPERTY(Transient)
	TObjectPtr<AActor> Set;
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> SetRoot;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ChairComp;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ParasolComp;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> ChairMesh;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> ParasolMesh;

	/** The beach's places (the slots of the "beach" spots), read once: where the sand is. */
	TArray<FVector> Sand;
	bool bSandRead = false;
	TWeakObjectPtr<AHeroCharacter> SeatedHero;
	/** What the prompt says now (0 none, 1 set up, 2 sit, 3 move here): set only when it changes. */
	int32 PromptShown = 0;
	int32 Offer = 0;
	double NextLook = 0.0;
	double StillSince = -1.0;
	/** Seated in the sun since (world seconds; < 0: not), and the game day the sunbath was last counted. */
	double SunSince = -1.0;
	int64 SunDay = -1;

	void ReadSand();
	void SetPrompt(int32 What);
	bool EnsureSet();
	bool IsSunny() const;
};
