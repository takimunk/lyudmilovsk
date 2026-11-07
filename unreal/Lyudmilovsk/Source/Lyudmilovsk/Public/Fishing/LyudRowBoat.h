#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyudRowBoat.generated.h"

class AHeroCharacter;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * A wooden rowing boat moored at the pier (docs/fishing.md, Content/Fishing/fishing.json "boat"): E close by takes it (the
 * rent the first time from the mooring), the hero sits on the middle thwart facing the bow; W/S row, A/D turn, Space gets
 * out (at the pier's steps, or where the water is shallow enough to stand: not out at sea). It rides the sea's surface
 * (ALyudSea::SurfaceZ under its ends and sides: pitch and roll), goes only where there is water under it, and no further
 * than Range from the mooring. The rod works from it as from the shore (the journal writes "from the boat").
 * Built in code (a lofted clinker hull, thwarts, oars in their rowlocks); nothing collides, nothing goes into Lumen.
 * Console: lyud.Fish.Boat [board|leave|home].
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ALyudRowBoat : public AActor
{
	GENERATED_BODY()

public:
	ALyudRowBoat();

	static ALyudRowBoat* Get(const UObject* WorldContext);
	/** The hero is in it. */
	bool IsAboard() const { return bAboard; }
	static bool IsHeroAboard(const UObject* WorldContext);

	/** Gets in (paying the rent if it lies at the mooring and isn't paid today); false if too far or no money. */
	bool Board(AHeroCharacter* Hero, bool bCheckReach = true);
	/** Gets out if it may here (the pier, shallow water); false: not here. */
	bool Leave();
	/** Back to the mooring now (checks). */
	void Home();
	/** Where it is tied up and where the hero steps out onto the pier (the water's point found at spring low water). */
	void SetMooring(const FVector2D& InMooring, const FVector2D& InLanding);
	/** Into the save's fields and back (ULyudFishingState): where it lies, the hire, the hero in it. */
	void Store(class ULyudFishingState& State) const;
	void Restore(const class ULyudFishingState& State);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	void Build();
	UStaticMeshComponent* Piece(const TCHAR* Name, UStaticMesh* Mesh, USceneComponent* Parent, const FLinearColor& Tint);
	UMaterialInstanceDynamic* Paint(const FLinearColor& Tint);
	bool WaterFor(const FVector& Where) const;
	void Ride(float DeltaTime);
	void Oars(float DeltaTime);
	void Prompt(AHeroCharacter* Hero);
	/** A word on the prompt line for a moment (ui.boat.<What>). */
	void Say(FName What);
	bool NearMooring() const;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USceneComponent> Hull;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> OarPivot;
	UPROPERTY() TObjectPtr<UMaterialInterface> BasicMat;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Mids;
	TMap<FLinearColor, UMaterialInstanceDynamic*> Paints;

	TWeakObjectPtr<AHeroCharacter> Rower;
	bool bAboard = false;
	bool bLeaving = false;
	/** Rented today (the calendar's day). */
	int32 PaidDay = -1;
	FVector2D Input = FVector2D::ZeroVector;
	uint64 InputFrame = 0;
	float Speed = 0.f, Turn = 0.f, Yaw = 0.f, Phase = 0.f, Stroke = 0.f;
	float HintLeft = 0.f;
	FName Note;
	float NoteLeft = 0.f;
	FVector2D Pos = FVector2D::ZeroVector;
	FVector2D Mooring = FVector2D::ZeroVector, Landing = FVector2D::ZeroVector;
	/** Its hull's box swept along each move (the pier's piles, a boulder stop it). */
	bool Blocked(const FVector2D& From, const FVector2D& To, float AtYaw) const;
	uint64 PromptKey = 0;
};
