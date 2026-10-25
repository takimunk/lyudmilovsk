#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "Templates/PimplPtr.h"
#include <atomic>
#include "LyudWindAudio.generated.h"

namespace LyudAudio { class FWindVoice; }

/**
 * The wind as heard at the head, synthesised (LyudAudio::FWindVoice in Private/Audio/AmbienceDSP.cpp): a low rumble and
 * a broad whoosh that rise with the speed and breathe with the gusts, aeolian whistling in edges and wires in a strong
 * wind, and the buffeting rush of the air past a moving head (a flying bird). Stereo, not spatialised: the ear facing
 * the wind hears more of it.
 *
 * Feed it every frame with SetWind: the wind's speed at the listener (ULyudWindSubsystem::GetWindAt, gusts included),
 * how gusty it is, where it comes from relative to where the listener faces, and the listener's own airspeed.
 * The ground's wind bed is ULyudSoundscapeSubsystem's (it uses the same voice); this component is for anything that
 * wants its own, like the bird's flight.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class LYUDMILOVSK_API ULyudWindAudioComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	ULyudWindAudioComponent(const FObjectInitializer& ObjectInitializer);
	virtual ~ULyudWindAudioComponent() override;

	/**
	 * WindSpeed m/s at the head; Gustiness 0 .. 0.5 (ULyudWindSubsystem::GetGustiness); YawToWindDeg = bearing the wind
	 * comes from relative to the facing (0 ahead, 90 from the right, -90 from the left); AirSpeed = the listener's own
	 * speed through the air, m/s (0 on foot). Values are smoothed inside, so calling it every frame is fine.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|Wind")
	void SetWind(float WindSpeed, float Gustiness, float YawToWindDeg, float AirSpeed);

	/** Overall level (1 = the soundscape's own mix). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Wind") float Gain = 1.f;

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	TPimplPtr<LyudAudio::FWindVoice> Voice;
	TArray<float> Left, Right;
	// written on the game thread, read on the audio thread
	std::atomic<float> Speed{ 0.f }, Gust{ 0.25f }, Pan{ 0.f }, Air{ 0.f };
};
