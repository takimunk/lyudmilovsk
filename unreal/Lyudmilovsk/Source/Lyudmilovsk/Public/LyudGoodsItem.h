#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeroReachComponent.h"
#include "LyudGoodsItem.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;
class UTexture;

/**
 * Any item from the item table that has a Look (ULyudInventory FLyudItemDef: a packet of chips, a coffee cup, a magazine, a
 * postcard...) as the vending machine pushes it out and the hand carries it. Built from the engine's shapes in the
 * item's size and tint, with the item's inventory icon as the picture on its front (the game-items rule: the icon is made
 * with the item). Origin at its middle, local Z up, local X out of its front. NotPlaceable.
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ALyudGoodsItem : public AActor
{
	GENERATED_BODY()

public:
	ALyudGoodsItem();

	/** Builds the look of item Id (a table entry with a Look). */
	void Init(FName InItem);

	FName ItemId() const { return Item; }
	FReachItem MakeReachItem() const;
	/** Half its height / half its depth (cm): where it rests on a tray. */
	float HalfHeight() const { return Half.Z; }
	float HalfDepth() const { return Half.X; }
	/** Lying on its back (the picture up), its top toward Along. */
	void LieDown(const FVector& Pos, const FVector& Along);

	/**
	 * A material instance of Base with "Color" (and "Label" when given), shared by every item of that look: goods and bottles that
	 * look alike draw with one material instead of one each. Kept for the run (rooted); never change it per item.
	 */
	static UMaterialInstanceDynamic* SharedMaterial(UMaterialInterface* Base, const FLinearColor& Color, UTexture* Label = nullptr);

	/** Shelf stock: a draw distance for tiny goods and no distance-field shadowing (not for what the hand holds). */
	static void SetShelfStock(AActor* Item);

private:
	UStaticMeshComponent* Part(UStaticMesh* Mesh, const FVector& Loc, const FVector& Size, const FLinearColor& Color);

	FName Item;
	FVector Half = FVector(3.f);
	bool bThin = false;
};
