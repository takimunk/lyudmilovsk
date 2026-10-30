#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HandReach.h"
#include "LyudDiaryBook.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTexture;

/**
 * The hero's diary as a thing in his hands (docs/diary.md): an A4 hardcover planner, oxblood leatherette over boards, cream
 * paper, index tabs on the fore-edge. Two halves hinged on the spine: the right one (back board and its pages) stays, the left
 * one (front board and its pages) swings round the spine, closed at 180 degrees, open flat at 0. The open spread shows a
 * picture of the game screen's pages (the diary screen draws them into a render target) so the 3D book and the screen
 * that takes over from it look the same.
 *
 * Its frame: X out of the open pages toward the reader, Y along the spread (to the reader's left: the left page is at +Y, the
 * right one at -Y), Z up the page; the origin on the spine, at the pages' surface, half way up. Centimetres. Built from code (no asset but the page material
 * /Game/UI/Diary/M_DiaryPage, with BasicShapeMaterial as the fallback); spawned by ULyudGameScreen while the diary is out.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudDiaryBook : public AActor
{
	GENERATED_BODY()

public:
	ALyudDiaryBook();

	static constexpr float PageW = 21.0f;       // A4
	static constexpr float PageH = 29.7f;
	static constexpr float Block = 0.85f;       // the pages of one half
	static constexpr float Board = 0.28f;
	static constexpr float Overhang = 0.3f;     // the boards past the pages
	static constexpr int32 NumTabs = 5;

	/** 0 open flat .. 180 shut. */
	void SetCover(float Degrees);
	float GetCover() const { return Cover; }
	/** The picture on the open pages (left page = left half of it); null shows blank paper. */
	void SetPages(UTexture* Spread);
	/** How bright the unlit pages are (1: as the screen draws them). */
	void SetPageBrightness(float B);
	/** Which index tabs stick out of which side: a section at or before the open spread lies under the left pages. */
	void SetTabs(int32 SectionsOnLeft);

	/** The four corners of the open spread's paper (top left, top right, bottom right, bottom left), world. */
	void SpreadCorners(FVector Out[4]) const;
	/** The book's solid shape as it is now (shut or open), for a hand to close on, world. */
	FReachShape Shape() const;
	/** Where a palm holds it from below, under the spine (world), and its frame (the fingers behind the book, the palm up). */
	void HoldFrame(FVector& Palm, FVector& Finger, FVector& Normal) const;

	/** Run each frame before the hero's hand is posed: the owner places the book there, so hand and book agree in the same frame. */
	TFunction<void(float)> OnTick;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> Spine;
	UPROPERTY()
	TObjectPtr<USceneComponent> LeftHinge;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> LeftPage;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> RightPage;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> TabsLeft;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> TabsRight;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PageMaterial;

	void Build();
	UStaticMeshComponent* AddPart(USceneComponent* Parent, const FName Name, UStaticMesh* Mesh, const FVector& At, UMaterialInterface* Material);

	float Cover = 180.f;
	bool bBuilt = false;
};
