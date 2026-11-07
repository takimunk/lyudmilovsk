#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BirdNests.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class FBirdPerches;
namespace LyudBirds { class FWorld; }

/** One nest as the game sees it (UBirdLifeSubsystem::GetNestsNear; docs/bird_nests.md). */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FBirdNestInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Birds") int32 Id = -1;
	UPROPERTY(BlueprintReadOnly, Category = "Birds") FName SpeciesId;
	/** The nest kind (Cup, Dome, Platform, Cavity, Eave, MudCup, Burrow, Scrape, Mound). */
	UPROPERTY(BlueprintReadOnly, Category = "Birds") FName Kind;
	/** None, Old, Build, Eggs, Chicks, Fledged, Failed. */
	UPROPERTY(BlueprintReadOnly, Category = "Birds") FName Stage;
	UPROPERTY(BlueprintReadOnly, Category = "Birds") FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Birds") float Yaw = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Birds") float Progress = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Birds") int32 Eggs = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Birds") int32 Chicks = 0;
	/** Days since the chicks hatched. */
	UPROPERTY(BlueprintReadOnly, Category = "Birds") float ChickAge = 0.f;
	/** The game has put it on a real branch, wall or the ground. */
	UPROPERTY(BlueprintReadOnly, Category = "Birds") bool bPlaced = false;
	UPROPERTY(BlueprintReadOnly, Category = "Birds") int32 Male = -1;
	UPROPERTY(BlueprintReadOnly, Category = "Birds") int32 Female = -1;
};

/**
 * Draws the birds' nests (docs/bird_nests.md): the nest itself at its stage (building, finished, old), the nest boxes, holes
 * and burrows, eggs and chicks in the nests near the camera, the downy young of swans and gulls following their mothers, and
 * the beakfuls of grass, moss or mud a builder carries (the corvid items draw the sticks). The meshes are the nest and chick
 * kits (blender/birds/nests, blender/birds/chicks -> /Game/Birds/Nests).
 *
 * Cost: a nest's instances are rewritten only when what is shown there changes (a stage, an egg, the draw range), checked
 * twice a second; begging chicks and the moving young are moved in place (no proxy rebuild) and only near the camera.
 * Nests are put on the real trees' branches, walls and ground once, when they first come within 160 m.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ABirdNestRenderer : public AActor
{
	GENERATED_BODY()

public:
	ABirdNestRenderer();
	virtual ~ABirdNestRenderer() override;

	void Draw(LyudBirds::FWorld& World, const FVector& Camera, float DeltaTime);
	/** A parent fed this nest's chicks now (they gape). */
	void OnFeed(int32 Nest);
	/** Puts a nest on real wood, a wall or the ground now (else it happens when it comes near). */
	void Place(LyudBirds::FWorld& World, int32 Nest);
	FString Describe() const;

	int32 InstancesLastUpdate = 0;
	int32 Rebuilds = 0;
	int32 Placed = 0;
	/** Placed on a tree's wood, a wall or cliff face, the ground; left where the simulation guessed (nothing found near). */
	int32 PlacedOn[4] = {};

private:
	struct FPiece
	{
		FName Mesh;
		FTransform Xf;
	};
	/** One mesh: its component, the transforms it shows (sorted by nest) and whether they changed. */
	struct FDrawSet
	{
		// weak: FDrawSet is not reflected; the actor's OwnedComponents keeps it, a destroyed one reads null
		TWeakObjectPtr<UInstancedStaticMeshComponent> ISM;
		TArray<FTransform> Xf;
		uint32 Hash = 0;
		bool bMover = false;
	};
	TMap<FName, FDrawSet> Sets;
	TSet<FName> Missing;
	TSharedPtr<FBirdPerches> Perches;
	float StaticTimer = 0.f;
	TMap<int32, double> LastFeed;
	FString Nearest;
	/** Per near chick nest: where its chicks sit (built with the static set), animated in place. */
	struct FBrood
	{
		int32 Nest = -1;
		FName Rest, Beg;
		int32 First = 0, Count = 0;      // instances in both sets
		TArray<FTransform> Base;
		float Phase = 0.f;
	};
	TArray<FBrood> Broods;

	FDrawSet* SetFor(FName Mesh, bool bMover);
	void Commit(FDrawSet& S, TArray<FTransform>& Want, bool bForce);
	void BuildStatic(LyudBirds::FWorld& World, const FVector& Camera);
	void AnimateBroods(LyudBirds::FWorld& World, const FVector& Camera, float DeltaTime);
	void DrawMovers(LyudBirds::FWorld& World, const FVector& Camera);
};
