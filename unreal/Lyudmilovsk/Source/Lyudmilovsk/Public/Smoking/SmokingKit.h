#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SmokingKit.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * The smoking kit's measures, cm (the meshes are made by blender/props/smoking/build_smoking.py: keep the two in step) and the
 * places of the ten cigarettes in the pack. docs/smoking.md.
 */
namespace SmokingKit
{
	constexpr float CigR = 0.39f;
	constexpr float CigL = 8.4f;
	constexpr float Filter = 2.1f;
	constexpr float Tip = 0.4f;
	/** The paper that can burn (between the filter and the ember); a butt keeps Stub of it. */
	constexpr float Paper = CigL - Filter - Tip;
	constexpr float Stub = 0.5f;
	constexpr float PackW = 4.23f;
	constexpr float PackD = 1.79f;
	constexpr float PackH = 8.8f;
	/** The lid's hinge (the back top edge of the body's cut) in the pack's frame. */
	const FVector Hinge(-PackD / 2.f, 0.f, 6.8f);
	constexpr int32 PerPack = 10;
	/** The bottom of cigarette Index (0..9, in the order they are taken: the front row's middle first) in the pack's frame. */
	FVector Slot(int32 Index);
	/** The lighter: its height to the flame hole, and the body's sizes. */
	constexpr float LighterTop = 7.3f;
	const FVector LighterHalf(0.55f, 1.2f, 3.65f);
}

/** A cigarette pack: the box, its flip-top lid, the inner collar and the cigarettes still in it, the words on it in the game's language. */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ACigarettePack : public AActor
{
	GENERATED_BODY()

public:
	ACigarettePack();

	/** How many are left (the first 10 - Left of SmokingKit::Slot are gone). */
	void SetLeft(int32 Left);
	int32 GetLeft() const { return Left; }
	/** 0 shut .. 1 open. */
	void SetLid(float Open);
	/** The next one to be taken, pushed up out of the pack by Cm. */
	void SetRaised(float Cm);
	/** The next one's transform in the world (its mouth end, along its +X) as it stands now, raised or not. */
	FTransform NextCigarette() const;
	/** The next one's filter end in the pack's frame when raised by Cm. */
	static FVector NextMouthEnd(int32 Left, float Cm);

	virtual void BeginPlay() override;

private:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Lid;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Collar;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Cigs;
	int32 Left = SmokingKit::PerPack;
	float Raised = 0.f;
};

/**
 * One cigarette out of the pack: the filter, the paper (shorter as it burns) and the burning end (ember and ash, its glow a
 * material parameter, with a faint light while it glows). Origin at the mouth end, along +X.
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ACigaretteItem : public AActor
{
	GENERATED_BODY()

public:
	ACigaretteItem();

	/** 1 whole .. 0 burnt to the filter (Stub of paper left). */
	void SetLeft(float InLeft);
	float GetLeft() const { return Left; }
	/** 0 unlit or out .. ~0.3 smouldering .. 1 glowing on a drag. */
	void SetGlow(float Glow);
	/** The burning end (world). */
	FVector TipLocation() const;
	FVector Axis() const { return GetActorForwardVector(); }

	virtual void BeginPlay() override;

private:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> FilterMesh;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> PaperMesh;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> TipMesh;
	UPROPERTY() TObjectPtr<UPointLightComponent> Ember;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> TipMat;
	float Left = 1.f;
	float GlowNow = -1.f;
};

/** The disposable lighter, with its flame (a flickering teardrop and a little warm light) when lit. Origin at its bottom, up +Z. */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ALighterItem : public AActor
{
	GENERATED_BODY()

public:
	ALighterItem();

	void SetFlame(bool bOn);
	bool IsLit() const { return bLit; }
	/** The flame's top in the lighter's frame: what lights a cigarette. */
	static FVector FlamePoint() { return FVector(0.f, 0.f, SmokingKit::LighterTop + 1.9f); }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> BodyMesh;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> FlameMesh;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
	bool bLit = false;
	float Time = 0.f;
};

/** A butt on the ground (thrown away): smokes a little, then lies there until the pool of butts recycles it. */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ACigaretteButt : public AActor
{
	GENERATED_BODY()

public:
	ACigaretteButt();

	virtual void BeginPlay() override;

private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
};

/**
 * Cigarette smoke, the whole world's: a fixed pool of soft camera-facing puffs (one instanced mesh, nothing allocated while it
 * runs), moved here on the CPU and drawn translucent and lit. Emitters push puffs in (a thin stream from a burning tip, a breath
 * out of the mouth, a cough); when the last puff fades the mesh is hidden and nothing is updated.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudSmokeFx : public AActor
{
	GENERATED_BODY()

public:
	ALyudSmokeFx();

	static constexpr int32 Pool = 96;

	/** One puff at Where moving at Velocity (cm/s), growing from Size0 to Size1 cm over Life s at Alpha opacity, rising Rise cm/s. */
	void Emit(const FVector& Where, const FVector& Velocity, float Size0, float Size1, float Alpha, float Life, float Rise);
	/** A breath of smoke out of Where along Dir: Amount 0..1 (a short puff .. a long drag). */
	void Breath(const FVector& Where, const FVector& Dir, float Amount);
	int32 Alive() const { return Live; }

	virtual void Tick(float DeltaTime) override;

private:
	struct FPuff
	{
		FVector Pos = FVector::ZeroVector;
		FVector Vel = FVector::ZeroVector;
		float Age = 0.f;
		float Life = 0.f;
		float Size0 = 1.f;
		float Size1 = 1.f;
		float Alpha = 0.f;
		float Rise = 0.f;
		float Seed = 0.f;
		/** Gone, and its instance already written hidden: Tick leaves it alone till it is emitted again. */
		bool bHiddenWritten = true;
	};
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Puffs;
	TArray<FPuff> All;
	TArray<FTransform> Xf;
	int32 Next = 0;
	int32 Live = 0;
	bool bShown = false;
};
