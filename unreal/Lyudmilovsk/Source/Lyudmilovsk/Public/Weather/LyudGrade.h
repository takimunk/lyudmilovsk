#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudGrade.generated.h"

class ALyudSky;
class UPostProcessComponent;

/**
 * The outdoor colour of the season, the weather and the hour (docs/weather.md, "Colour"): so faint that a player never
 * sees it change. In winter dusks and nights the shadows and middle tones lean a little to deep blue while the bright
 * tones (the lamps' sodium orange) keep their warmth and a touch more saturation; a summer day is a little lighter and
 * airier with the faintest ultramarine in the shade; a low summer sun warms the highlights; grey and wet weather
 * mutes the colour, fog flattens the contrast. It also offers the sky a haze factor (GetSkyHazeScale: humidity, mist,
 * rain) for ALyudSky to multiply into its own Mie scattering. Indoors (ALyudSky says the player is under a roof) it
 * eases out entirely.
 *
 * It is an unbound post-process component on a transient actor (priority 1001, over ALyudSky's exposure) and writes
 * only colour grading, never the exposure. Every input is continuous and the result eases, so nothing steps.
 *
 * Console: lyud.Grade 0 turns it off (A/B), lyud.Grade.Strength scales it (1 = as designed), lyud.Grade.Debug 1 prints
 * the grade's inputs and outputs.
 */
UCLASS()
class LYUDMILOVSK_API ULyudGradeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

	/** The weather's factor on the sky's haze (Mie scattering) for ALyudSky, which owns the atmosphere: 1 = as it is. */
	UFUNCTION(BlueprintPure, Category = "Weather", meta = (WorldContext = "WorldContext"))
	static float GetSkyHazeScale(const UObject* WorldContext);

	/** Text for logs and tests. */
	UFUNCTION(BlueprintCallable, Category = "Weather") FString DebugReport() const;

private:
	void EnsureActor();

	UPROPERTY(Transient) TObjectPtr<AActor> GradeActor;
	UPROPERTY(Transient) TObjectPtr<UPostProcessComponent> Grade;
	TWeakObjectPtr<ALyudSky> Sky;
	float Outdoor = 0.f;
	float HazeScale = -1.f;
	// what the grade's inputs were last tick (debug)
	float WinterNight = 0.f, SummerDay = 0.f, Golden = 0.f, Grey = 0.f, Haze = 0.f;
	double NextLook = 0.0;
};
