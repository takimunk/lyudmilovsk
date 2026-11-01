#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "Templates/PimplPtr.h"
#include "FountainDSP.h"
#include <atomic>
#include "LyudFountainSynth.generated.h"

/** Plays the fountain's water (LyudAudio::FFountainVoice) in mono, spatialised where it sits; ALyudFountain sets the flow. */
UCLASS()
class ULyudFountainSynth : public USynthComponent
{
	GENERATED_BODY()

public:
	ULyudFountainSynth(const FObjectInitializer& ObjectInitializer);
	virtual ~ULyudFountainSynth() override;

	/** 0 off .. 1 running (eased inside), and the level (the settings' volumes). */
	void SetFlow(float InFlow, float InGain);
	/** Loudness of the last block (tests). */
	float GetRms() const { return Rms.load(); }

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	TPimplPtr<LyudAudio::FFountainVoice> Voice;
	std::atomic<float> Flow{ 0.f };
	std::atomic<float> Gain{ 1.f };
	std::atomic<float> Rms{ 0.f };
};
