#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "Templates/PimplPtr.h"
#include "AmbienceDSP.h"
#include "LyudAmbienceSynth.generated.h"

/** Plays the procedural outdoor bed (LyudAudio::FAmbience) in stereo; ULyudSoundscapeSubsystem sets its parameters. */
UCLASS()
class ULyudAmbienceSynth : public USynthComponent
{
	GENERATED_BODY()

public:
	ULyudAmbienceSynth(const FObjectInitializer& ObjectInitializer);
	virtual ~ULyudAmbienceSynth() override;

	/** The world's state for the next audio blocks (smoothed inside). */
	void SetParams(const LyudAudio::FAmbienceParams& InParams);
	/** Lightning DistanceM away at Pan: the thunder arrives after the sound's travel time. */
	void Strike(float DistanceM, float Pan);
	/** Each voice's loudness over the last block (tests, lyud.Sound.Debug). */
	void GetVoiceRms(float Out[LyudAudio::NumVoices]) const;

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	TPimplPtr<LyudAudio::FAmbience> Bed;
	mutable FCriticalSection Lock;
	LyudAudio::FAmbienceParams Params;
	float Rms[LyudAudio::NumVoices] = {};
	TArray<float> Left, Right;
};
