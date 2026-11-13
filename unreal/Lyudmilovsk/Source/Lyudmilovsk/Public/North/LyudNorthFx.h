#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyudNorthFx.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;
class UStaticMesh;

/**
 * The North's steam (and the camp fire's smoke): the cigarette smoke's way (ALyudSmokeFx, docs/smoking.md) at the scale of
 * a valley: a fixed pool of soft camera-facing puffs in one instanced mesh, moved on the CPU, nothing allocated while it
 * runs, only the live range of instances written each frame (no proxy rebuild), hidden and not ticking when the last puff
 * is gone. Puffs ride the wind (ULyudNorthSubsystem feeds it) more the older they are, so a vent's column leans downwind.
 * Material /Game/North/Materials/M_North_Steam (import_north.py: lit translucent, opacity = custom data 0, grey = data 2),
 * else the cigarette smoke's /Game/Smoking/M_Smoke. No shadows, out of Lumen and the distance fields.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudNorthSteam : public AActor
{
	GENERATED_BODY()

public:
	ALyudNorthSteam();

	static constexpr int32 Pool = 448;

	/** One puff. Where (world cm), Velocity (cm/s) slowed by Drag (1/s), growing Size0 -> Size1 cm over Life s at Alpha,
	 *  rising Rise cm/s^2, Grey 0 white steam .. 1 grey smoke; Drift 0..1 how much the wind takes it once it is old. */
	void Emit(const FVector& Where, const FVector& Velocity, float Size0, float Size1, float Alpha, float Life, float Rise,
		float Drag = 1.f, float Grey = 0.f, float Drift = 1.f);
	/** The wind (world cm/s) and the air's density (1 normal: alpha and life scale with it). */
	void SetAir(const FVector& InWindCm, float InDensity) { WindCm = InWindCm; Density = InDensity; }
	float GetDensity() const { return Density; }
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
		float Drag = 1.f;
		float Grey = 0.f;
		float Drift = 1.f;
		float Seed = 0.f;
		bool bHiddenWritten = true;
	};
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Puffs;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Card;
	TArray<FPuff> All;
	TArray<FTransform> Xf;
	FVector WindCm = FVector::ZeroVector;
	float Density = 1.f;
	int32 Next = 0;
	int32 Live = 0;
	bool bShown = false;
};

/**
 * A far volcano's steam plume (north.json far_volcanoes "plume"): a handful of huge soft sprites rising ~600 m from the
 * crater and bending downwind, 5..10 km away. Each sprite's place follows from the clock alone (no state), written four
 * times a second; unlit (/Game/North/Materials/M_North_Plume: the translucency lighting volume does not reach that far),
 * its colour set from the sun's height every few seconds. Spawned by ULyudNorthSubsystem, tagged North.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudNorthPlume : public AActor
{
	GENERATED_BODY()

public:
	ALyudNorthPlume();

	static constexpr int32 Count = 12;

	/** Crater top (world cm), its radius (cm), the plume's strength 0..1. */
	void Setup(const FVector& InTop, float InCraterRadius, float InStrength);
	void SetWind(const FVector& InWindCm) { WindCm = InWindCm; }

	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Sprites;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Card;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Mat;
	TArray<FTransform> Xf;
	FVector Top = FVector::ZeroVector;
	FVector WindCm = FVector::ZeroVector;
	FVector SmoothWind = FVector::ZeroVector;
	float CraterRadius = 20000.f;
	float Strength = 1.f;
	double Clock = 0.0;
	float ColourTimer = 0.f;
};

namespace LyudNorthFx
{
	/** A 1 cm square card facing +Z, centred, UV 0..1 (the puff shape; transient, never saved). */
	UStaticMesh* PuffCard(UObject* Outer, UMaterialInterface* Material, FName Name);
	/** A moving or effect part: no shadows, out of Lumen and the distance fields, no collision, no ray tracing. */
	void Lightweight(UPrimitiveComponent* C);
}
