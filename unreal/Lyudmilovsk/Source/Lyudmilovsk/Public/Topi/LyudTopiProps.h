#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HAL/IConsoleManager.h"
#include "LyudTopiProps.generated.h"

class ALyudTopi;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * What stands in Topi (docs/topi.md): five warning plates where the dunes begin, rag-tied stakes along the one path to the river, and at
 * its end the board «Урал» with the beach towels on the bank. Meshes SM_Topi_* from blender/topi/props.py, imported by
 * unreal/scripts/import_topi.py to /Game/Topi/Props (a missing mesh is skipped, logged once). Created by ALyudTopi as its "Props".
 *
 * Placed once, on the first tick after the owner has begun play, in map metres through ALyudTopi::ToWorld; the ground under everything
 * east of map X -1300 is Topi's own (ALyudTopi::SandZ); the warning plates stand on the map's landscape, whose chunks load later, so they
 * are traced down once a second for up to 30 s (SandZ if it never answers). Nothing ticks after that. The signs' pictures follow the player's
 * language (lyud.Language, T_Sign_<name> by LyudText::LoadTexture("Topi", ...)). lyud.Topi.Props logs what stands where.
 */
UCLASS()
class LYUDMILOVSK_API ULyudTopiProps : public UActorComponent
{
	GENERATED_BODY()

public:
	ULyudTopiProps();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** What was placed (the lyud.Topi.Props command logs it). */
	FString Report() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** A warning plate still waiting for the ground under it. */
	struct FPendingSign
	{
		double MapX = 0.0;
		double MapY = 0.0;
		float YawDeg = 0.f;
		float LeanDeg = 0.f;
		float LeanDirDeg = 0.f;
	};

	void PlaceAll(ALyudTopi* Topi);
	void PlaceStakes(ALyudTopi* Topi);
	void PlaceRiverBank(ALyudTopi* Topi);
	void PlaceSign(ALyudTopi* Topi, const FPendingSign& Sign, float GroundZ);
	/** World Z of the landscape under a map point (cm), or false while the ground there has not loaded. */
	bool TraceGround(ALyudTopi* Topi, double MapX, double MapY, float& OutZ) const;
	UStaticMeshComponent* Spawn(UStaticMesh* Mesh, const FVector& At, const FQuat& Rotation, bool bCollide);
	static FQuat Stand(float YawDeg, float LeanDeg, float LeanDirDeg);
	void ApplySignLanguage();
	void OnLanguageChanged(IConsoleVariable* Variable);
	/** Gives every TP_Sign* slot of a part its own material instance (the picture is set by ApplySignLanguage). */
	void MakeSignMaterials(UStaticMeshComponent* Part);

	UPROPERTY(Transient) TObjectPtr<UStaticMesh> WarnMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> UralMesh;
	/** The stake variants SM_Topi_Stake_A..D (or the single SM_Topi_Stake of older imports). */
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> StakeMeshes;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> TowelMeshes;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> PlacedParts;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> SignMIDs;

	/** The picture name of each of SignMIDs (T_Sign_<name>). */
	TArray<FString> SignNames;
	TArray<FPendingSign> Pending;
	FDelegateHandle LanguageHandle;
	double WaitedSeconds = 0.0;
	int32 NumSigns = 0;
	int32 NumStakes = 0;
	int32 NumTowels = 0;
	int32 NumTraced = 0;
	bool bPlaced = false;
	bool bUral = false;
	bool bMissingLogged = false;
};
