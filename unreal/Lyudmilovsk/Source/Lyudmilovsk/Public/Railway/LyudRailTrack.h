#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyudRailTrack.generated.h"

class FJsonObject;
class FLyudMeshKit;
class ULyudRailwaySubsystem;
class UStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UPointLightComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * The sea railway's line, built in code from railway_built.json (docs/railway.md): the tunnel's mouth in the cliff (a
 * concrete headwall and hood over the bore, the bore going dark), the low bank of ballast on armour stones out to sea
 * and on to the horizon, the rails on wooden sleepers, the catenary (masts every span, the contact wire and its
 * messenger sagging between them), the two halts (a timber deck on piles, a ramp to the beach, a shelter, the
 * nameboard in the player's language, lamps lit at dusk) and the island.
 *
 * The bank near the coast and the island carry the tag LyudSeaBed on their components, so the sea's depth map counts
 * them as bed (the waves shrink and break over the bank instead of rolling through it). Their crest and decks
 * collide: one can walk out along the rails.
 */
UCLASS()
class LYUDMILOVSK_API ALyudRailTrack : public AActor
{
	GENERATED_BODY()

public:
	ALyudRailTrack();
	void Build(ULyudRailwaySubsystem* InRailway, const TSharedPtr<FJsonObject>& Built);
	virtual void Tick(float DeltaSeconds) override;

	/** The bank's chunks (for tests): one mesh per this many metres. */
	static constexpr double ChunkM = 400.0;
	int32 NumChunks() const { return Chunks.Num(); }

private:
	void BuildLine(const TSharedPtr<FJsonObject>& Built);
	void BuildTunnel(const TSharedPtr<FJsonObject>& Built);
	void BuildHalt(const TSharedPtr<FJsonObject>& Stop);
	void BuildIsland(const TSharedPtr<FJsonObject>& Built);
	UStaticMeshComponent* AddKit(const FLyudMeshKit& Kit, FName Name, const FVector& At, bool bSeaBed, bool bCollide);
	UStaticMeshComponent* AddPiece(const TCHAR* Kit, const FTransform& At, bool bCollide = true);
	UMaterialInterface* Mat(FName Name);

	UPROPERTY(Transient) TObjectPtr<ULyudRailwaySubsystem> Railway;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Chunks;
	UPROPERTY(Transient) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Sleepers;
	UPROPERTY(Transient) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Masts;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Lamps;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> LampGlows;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UMaterialInterface>> Mats;
	TWeakObjectPtr<AActor> Sky;
	float LampOn = -1.f;
	float CheckIn = 0.f;
};
