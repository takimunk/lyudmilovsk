#pragma once

#include "CoreMinimal.h"
#include "North/LyudNorth.h"
#include "North/NorthModel.h"
#include "LyudFumarole.generated.h"

class UAudioComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * One fumarole of north.json "fumaroles" (placed by unreal/scripts/import_north.py, one actor each; docs/north.md):
 *
 *  - Vent:   a crusted mound with a dark throat; awake, a hissing jet of steam 6..15 m high (by Size) leaning downwind;
 *  - Steam:  wisps rising off a few square metres of warm ground (the ground itself is the terrain's sulphur layer);
 *  - MudPot: a round pool of grey mud Size m across inside a dried rim; bubbles swell and plop, light steam;
 *  - Spring: a still hot pool (~2.6 m radius) of clear greenish water in a stone rim, thin steam.
 *
 * The rims and pools are built once in code (FLyudMeshKit: transient, never saved) and always drawn; steam, bubbles and
 * sound only while ULyudNorthSubsystem keeps it awake / near. The steam goes into the shared ALyudNorthSteam pool.
 */
UCLASS()
class LYUDMILOVSK_API ALyudFumarole : public ALyudNorthThing
{
	GENERATED_BODY()

public:
	ALyudFumarole();

	/** north.json id (F01..). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "North") FName FumaroleId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "North") ELyudFumaroleKind Kind = ELyudFumaroleKind::Vent;
	/** north.json size: a vent's strength (0.7..1.8), a patch's spread, a mud pot's diameter in m, the spring's 3.2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "North") float Size = 1.f;

	/** The pool's radius (mud pot, spring), cm; a vent's mound, a patch's spread. */
	float PoolRadius() const;
	/** The pool's surface (the spring's water, the mud) over the actor's origin, cm (built at BeginPlay). */
	float GetSurfaceZ() const { return SurfaceZ; }
	/** Puffs emitted since play began (tests). */
	int32 NumEmitted() const { return Emitted; }

	virtual void SetAwake(bool bInAwake, bool bInNear) override;
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Build();
	void EmitSteam(float DeltaTime);
	void StepBubbles(float DeltaTime);
	void StartLoop();
	void StopLoop();
	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, UMaterialInstanceDynamic* Mat, bool bMoving);
	UMaterialInstanceDynamic* MakeMat(const TCHAR* BasePath, const TCHAR* FallbackPath, const FLinearColor& Color, float Roughness);

	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> BubbleParts;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Mats;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Loop;
	TArray<LyudNorthModel::FBubble> Bubbles;
	float EmitDebt = 0.f;
	float SurfaceZ = 0.f;
	uint32 Rng = 1u;
	int32 Emitted = 0;
	bool bBuilt = false;
};
