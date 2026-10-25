#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeroReachComponent.h"
#include "BirdFeedItem.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

/**
 * A paper bag of bird feed (sunflower seeds and millet) as the vending machine pushes it out and the hand carries it:
 * a kraft-paper block with the top rolled over twice, built from the engine's cube. Origin at its middle, local Z up,
 * local X out of its front. NotPlaceable: the vending machine and the hero's hand spawn it.
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ABirdFeedBag : public AActor
{
	GENERATED_BODY()

public:
	ABirdFeedBag();

	virtual void BeginPlay() override;

	/** How the hero's hand sees it. */
	FReachItem MakeReachItem() const;

	/** Lying on its side on a tray, front up. */
	void LieDown(const FVector& Pos, const FVector& Along);

	/** Half the bag's size, cm (width, depth, height). */
	static constexpr float HalfWidth = 4.5f;
	static constexpr float HalfDepth = 2.6f;
	static constexpr float HalfHeight = 7.f;

private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Fold;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Band;
};

/**
 * A handful of feed on the open palm: a low heap of dark striped sunflower seeds and pale millet, one instanced
 * mesh. Origin at the bottom middle of the heap (where it touches the palm), local Z out of the palm. It can shrink
 * as a bird pecks it (SetLeft).
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ASeedHandful : public AActor
{
	GENERATED_BODY()

public:
	ASeedHandful();

	virtual void BeginPlay() override;

	/** Share of the handful still there, 1 to 0: seeds go from the top of the heap down. */
	void SetLeft(float Left);

	/** The palm turned over: the heap comes off the hand and falls, scattering, and is gone in a second. */
	void Spill();
	virtual void Tick(float DeltaSeconds) override;

	/** Radius of the heap on the palm, cm. */
	static constexpr float Radius = 3.2f;

private:
	void Build();

	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Seeds;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Millet;
	int32 SeedCount = 0;
	int32 MilletCount = 0;
	bool bFalling = false;
	float FallTime = 0.f;
	FVector FallVelocity = FVector::ZeroVector;
};
