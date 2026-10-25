#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "BirdWindStreaks.generated.h"

class UInstancedStaticMeshComponent;

/**
 * The wind made visible while you are a bird: faint streaks of air drifting with it around the camera, long in
 * a strong wind, rising where the air rises (in front of the sea cliff, in a thermal) and streaming past when
 * you fly fast. One instanced mesh of thin cards; M_WindStreak (additive, unlit) reads the fade from custom data.
 */
UCLASS(ClassGroup = Birds)
class UBirdWindStreaks : public USceneComponent
{
	GENERATED_BODY()

public:
	UBirdWindStreaks();

	/** Each frame: the camera, the air's velocity there (m/s, wind plus rising air), Visible 0..1. */
	void UpdateStreaks(const FVector& CamLoc, const FVector& AirMs, float Visible, float Dt);

	static const FName MaterialPath;

protected:
	virtual void OnRegister() override;

private:
	struct FStreak
	{
		FVector Pos = FVector::ZeroVector;
		float Age = 0.f, Life = 1.f, Len = 1.f;
	};
	void Respawn(FStreak& S, const FVector& CamLoc, const FVector& AirMs, bool bAnyAge);

	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Cards;
	TArray<FStreak> Streaks;
	FRandomStream Rng;
};
