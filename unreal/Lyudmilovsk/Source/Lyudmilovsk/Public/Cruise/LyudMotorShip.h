#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyudMotorShip.generated.h"

class ALyudSea;
class ALyudSky;
class ULyudShipWake;
class UMaterialInstanceDynamic;
class USoundAttenuation;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * The motor ship «Полдень» as the town sees her (docs/ship.md): at anchor on the roads between voyages, swinging
 * slowly head to wind on her cable and lifting with the tide; sailing out on Friday at 18:00 and home on Sunday
 * (where ULyudCruise::View() says she is on the chart, which is the world out there), with a wake while she moves,
 * her horn heard in the town, an anchor light and running lights at night. Never boarded: the cruise is played aboard
 * the furnished ship at the cruise stage, and while the hero is aboard this one is hidden. Laid up out of season.
 *
 * She wears the stage ship's own meshes (the city building of the plot P-SHIP, type motor_ship: shell, details, window
 * cards with their material), exterior only, no collision, movable and out of Lumen and the distance fields. Until
 * that building is in the level a plain stand-in hull is shown.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudMotorShip : public AActor
{
	GENERATED_BODY()

public:
	ALyudMotorShip();
	virtual void Tick(float DeltaSeconds) override;

	/** Blasts of the horn from where she lies (1: one long; 3: three long, the departure). */
	void Horn(int32 Blasts);
	bool IsShown() const { return bShown; }
	FString Report() const;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> StandIn;
	UPROPERTY(Transient) TObjectPtr<ULyudShipWake> Wake;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Lamps;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> LampMids;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> HornAtten;
	UPROPERTY(Transient) TObjectPtr<USoundBase> HornOne;
	UPROPERTY(Transient) TObjectPtr<USoundBase> HornThree;
	TWeakObjectPtr<ALyudSea> Sea;
	TWeakObjectPtr<ALyudSky> Sky;
	bool bDressed = false;
	bool bStandIn = false;
	bool bShown = false;
	float DressClock = 0.f;
	float SwingYaw = 0.f;
	bool bSwingSet = false;
	float LampGlow = -1.f;
	float LengthCm = 6200.f;

	/** The stage ship's meshes (or the stand-in) on this actor; true once the real ones are on. */
	bool Dress();
	void MakeStandIn();
	void MakeLamps();
	void Show(bool bOn);
};
