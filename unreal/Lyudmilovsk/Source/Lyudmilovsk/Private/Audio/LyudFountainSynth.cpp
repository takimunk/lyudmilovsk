#include "LyudFountainSynth.h"

ULyudFountainSynth::ULyudFountainSynth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	bAllowSpatialization = true;
}

ULyudFountainSynth::~ULyudFountainSynth() = default;

void ULyudFountainSynth::SetFlow(float InFlow, float InGain)
{
	Flow.store(InFlow);
	Gain.store(InGain);
}

bool ULyudFountainSynth::Init(int32& SampleRate)
{
	NumChannels = 1;
	Voice = MakePimpl<LyudAudio::FFountainVoice>();
	Voice->Init(float(SampleRate), 17);
	return true;
}

int32 ULyudFountainSynth::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	FMemory::Memzero(OutAudio, NumSamples * sizeof(float));
	if (Voice)
	{
		Voice->Render(OutAudio, NumSamples, Flow.load(), Gain.load());
		Rms.store(Voice->Rms);
	}
	return NumSamples;
}
