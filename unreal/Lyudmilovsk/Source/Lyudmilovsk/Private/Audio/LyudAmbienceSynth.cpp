#include "LyudAmbienceSynth.h"

#include "Misc/ScopeLock.h"

ULyudAmbienceSynth::ULyudAmbienceSynth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 2;
	bAllowSpatialization = false;
}

ULyudAmbienceSynth::~ULyudAmbienceSynth() = default;

void ULyudAmbienceSynth::SetParams(const LyudAudio::FAmbienceParams& InParams)
{
	FScopeLock Guard(&Lock);
	Params = InParams;
}

void ULyudAmbienceSynth::Strike(float DistanceM, float Pan)
{
	SynthCommand([this, DistanceM, Pan]()
	{
		if (Bed)
		{
			Bed->Strike(DistanceM, Pan);
		}
	});
}

void ULyudAmbienceSynth::GetVoiceRms(float Out[LyudAudio::NumVoices]) const
{
	FScopeLock Guard(&Lock);
	FMemory::Memcpy(Out, Rms, sizeof(Rms));
}

bool ULyudAmbienceSynth::Init(int32& SampleRate)
{
	NumChannels = 2;
	Bed = MakePimpl<LyudAudio::FAmbience>();
	Bed->Init(float(SampleRate), 7);
	return true;
}

int32 ULyudAmbienceSynth::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	const int32 Frames = NumSamples / 2;
	Left.SetNumZeroed(Frames, EAllowShrinking::No);
	Right.SetNumZeroed(Frames, EAllowShrinking::No);
	FMemory::Memzero(Left.GetData(), Frames * sizeof(float));
	FMemory::Memzero(Right.GetData(), Frames * sizeof(float));
	LyudAudio::FAmbienceParams P;
	{
		FScopeLock Guard(&Lock);
		P = Params;
	}
	if (Bed)
	{
		Bed->Render(Left.GetData(), Right.GetData(), Frames, P);
		FScopeLock Guard(&Lock);
		FMemory::Memcpy(Rms, Bed->VoiceRms, sizeof(Rms));
	}
	for (int32 I = 0; I < Frames; I++)
	{
		OutAudio[2 * I] = Left[I];
		OutAudio[2 * I + 1] = Right[I];
	}
	return NumSamples;
}
