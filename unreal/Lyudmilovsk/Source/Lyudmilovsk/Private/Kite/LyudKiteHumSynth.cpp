#include "LyudKiteHumSynth.h"

ULyudKiteHumSynth::ULyudKiteHumSynth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	bAllowSpatialization = true;
}

ULyudKiteHumSynth::~ULyudKiteHumSynth() = default;

void ULyudKiteHumSynth::SetHum(float InHum, float InAir, float InGain)
{
	Hum.store(InHum);
	Air.store(InAir);
	Gain.store(InGain);
}

bool ULyudKiteHumSynth::Init(int32& SampleRate)
{
	NumChannels = 1;
	Voice = MakePimpl<LyudKiteHum::FVoice>();
	Voice->Init(float(SampleRate), 31);
	return true;
}

int32 ULyudKiteHumSynth::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	FMemory::Memzero(OutAudio, NumSamples * sizeof(float));
	if (Voice)
	{
		Voice->Render(OutAudio, NumSamples, Hum.load(), Air.load(), Gain.load());
		Rms.store(Voice->Rms);
	}
	return NumSamples;
}
