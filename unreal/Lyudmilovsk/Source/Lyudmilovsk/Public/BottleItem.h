#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeroReachComponent.h"
#include "BottleItem.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * A 0.5 l glass bottle of Soviet-style fizzy drink: a lathe-turned glass body, the liquid inside (its level
 * can fall), a label with the drink's name (/Game/Hall/Localized/<lang>/T_Label_<flavor>) and a crown cap, built in code.
 * Origin at the middle of the bottle, local Z toward the neck, local X out of the label. NotPlaceable: the
 * vending machine and the inventory spawn it.
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ABottleItem : public AActor
{
	GENERATED_BODY()

public:
	ABottleItem();

	/** Flavor: baikal, dyushes, buratino, tarkhun; Level: how full it is (0..1). */
	void Init(FName InFlavor, float InLevel = 1.f);
	void SetLevel(float InLevel);
	float GetLevel() const { return Level; }

	/** How the hero's hand sees it. */
	FReachItem MakeReachItem() const;

	/** Lying on its side with the label up, neck toward Neck (a unit vector). */
	void LieDown(const FVector& Pos, const FVector& Neck);

	static constexpr float BottleHalfLength = 11.2f;
	static constexpr float BodyRadius = 3.2f;

private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Glass;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Liquid;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Label;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Cap;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LiquidMaterial;

	FName Flavor;
	float Level = 1.f;
};
