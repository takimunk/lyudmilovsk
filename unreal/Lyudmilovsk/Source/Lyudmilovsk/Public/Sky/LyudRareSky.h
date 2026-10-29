#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudRareSky.generated.h"

class ALyudSky;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/** What the rare sky shows now (for logs, tests and Blueprints). */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudRareSkyNow
{
	GENERATED_BODY()

	/** The geomagnetic Kp the aurora follows, and the aurora's brightness reaching the eye over a clear sky. */
	UPROPERTY(BlueprintReadOnly, Category = "Rare sky") float Kp = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Rare sky") float Aurora = 0.f;
	/** quiet, growth, breakup, recovery or none. */
	UPROPERTY(BlueprintReadOnly, Category = "Rare sky") FString AuroraPhase;
	/** Ground km toward the magnetic pole to the main arc (negative: it is south of us, a great storm). */
	UPROPERTY(BlueprintReadOnly, Category = "Rare sky") float AuroraEdgeKm = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Rare sky") float Halo = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Rare sky") float SunDogs = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Rare sky") float Rainbow = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Rare sky") float Noctilucent = 0.f;
	/** The red dwarf's flare now, 0..1. */
	UPROPERTY(BlueprintReadOnly, Category = "Rare sky") float Flare = 0.f;
	/** The lightning's flash now (0..~1). */
	UPROPERTY(BlueprintReadOnly, Category = "Rare sky") float Lightning = 0.f;
};

/**
 * The rare sky (docs/rare_sky.md): the aurora, halos and sun dogs, rainbows and moonbows, noctilucent clouds, the
 * lightning of a thunderstorm and the flares of our red dwarf. The weather engine (ULyudWeatherSubsystem) says how
 * likely each is; RareSkyModel (Private/Sky) turns that into what is in the sky this minute, as a function of the
 * calendar; this actor draws it:
 *   - on the star dome (ALyudSky's material): the aurora (curtains, rays, folds, the high red, pulsating patches),
 *     noctilucent clouds, the glow of a strike behind the clouds, a flare on the star's face;
 *   - on a sphere ~3 km round the camera (M_LyudSkyOptics): halos, sun dogs, the pillar, rainbows, worked out for a
 *     10-degree 3200 K star (and Europa by night);
 *   - on a sphere just short of a strike (M_LyudLightning): the bolt, with its leader, return strokes and branches,
 *     and a point light that flashes the land and the sea.
 * ULyudRareSkySubsystem spawns one in any game world that has an ALyudSky.
 *
 * Console: lyud.Rare.Kp / Aurora / Halo / Rainbow / Noctilucent force a value (negative: the weather's);
 * lyud.Rare.AuroraOnset <minutes> stages a substorm whose breakup was that long ago; lyud.Rare.Flare <0..1> a flare
 * at that strength; lyud.Rare.Strike [m] [cg 0/1] a strike; lyud.Rare.Debug 1 prints the state on screen.
 */
UCLASS()
class LYUDMILOVSK_API ALyudRareSky : public AActor
{
	GENERATED_BODY()

public:
	ALyudRareSky();

	/** The aurora's brightness on the dome per unit of the model's (tuning: the night exposure opens +3.5 EV). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rare sky|Look") float AuroraGain = 1.5f;
	/** How folded the curtains are (1 = the model's). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rare sky|Look") float AuroraFolding = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rare sky|Look") float NoctilucentGain = 0.45f;
	/** Halos and rainbows: brightness per unit of the light (sun or Europa) and how much of the light's real size blurs
	 * them (1 = physical: the 10-degree star makes them broad and pale; lower = sharper, Earth-like). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rare sky|Look") float OpticsGain = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rare sky|Look") float OpticsDiskScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rare sky|Look") float OpticsRadius = 300000.f;
	/** A strike's light: candela at the return stroke's peak (inverse square: ~2 lux on the ground 2 km off). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rare sky|Lightning") float StrikeCandela = 7.0e6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rare sky|Lightning") float BoltGain = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rare sky|Lightning") float SkyFlashGain = 0.12f;
	/** Share of strikes that reach the ground (the rest flash inside the cloud). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rare sky|Lightning") float CloudToGround = 0.4f;
	/** The cloud base over a strike, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rare sky|Lightning") float CloudBase = 150000.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rare sky") TObjectPtr<UStaticMeshComponent> OpticsDome;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rare sky") TObjectPtr<UStaticMeshComponent> BoltDome;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rare sky") TObjectPtr<UPointLightComponent> StrikeLight;

	UFUNCTION(BlueprintCallable, Category = "Rare sky") FLyudRareSkyNow GetNow() const { return Now; }
	/** Lightning now: a strike DistanceM from the camera (a random direction); bCloudToGround or a flash in the cloud. */
	UFUNCTION(BlueprintCallable, Category = "Rare sky") void StrikeAt(FVector Where, float DistanceM, bool bCloudToGround);
	UFUNCTION(BlueprintCallable, Category = "Rare sky") FString DebugReport() const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UFUNCTION() void OnLightning(FVector Location, float DistanceFromViewM);
	void UpdateLightning(float DeltaSeconds, const FVector& Camera);

	UPROPERTY(Transient) TObjectPtr<ALyudSky> Sky;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> OpticsMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BoltMID;
	FLyudRareSkyNow Now;

	struct FStrike
	{
		FVector Ground = FVector::ZeroVector;
		float Age = 0.f;
		float Seed = 0.f;
		bool bGround = true;
		float Strokes[5] = { 0.f, 0.f, 0.f, 0.f, 0.f };
		int32 NumStrokes = 1;
	};
	TArray<FStrike> Strikes;
	float SphereRadius = 1.f;
};

/** Puts an ALyudRareSky into every game world that has an ALyudSky. */
UCLASS()
class LYUDMILOVSK_API ULyudRareSkySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	virtual void OnWorldBeginPlay(UWorld& World) override;
};
