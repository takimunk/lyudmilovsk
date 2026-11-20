#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "Templates/PimplPtr.h"
#include "KiteHum.h"
#include <atomic>
#include "LyudKiteHumSynth.generated.h"

/** The kite line's hum (LyudKiteHum::FVoice) in mono, spatialised at the reel; ULyudKites sets how taut and how fast the air. */
UCLASS()
class ULyudKiteHumSynth : public USynthComponent
{
	GENERATED_BODY()

public:
	ULyudKiteHumSynth(const FObjectInitializer& ObjectInitializer);
	virtual ~ULyudKiteHumSynth() override;

	void SetHum(float InHum, float InAir, float InGain);
	float GetRms() const { return Rms.load(); }

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	TPimplPtr<LyudKiteHum::FVoice> Voice;
	std::atomic<float> Hum{ 0.f };
	std::atomic<float> Air{ 0.f };
	std::atomic<float> Gain{ 1.f };
	std::atomic<float> Rms{ 0.f };
};
