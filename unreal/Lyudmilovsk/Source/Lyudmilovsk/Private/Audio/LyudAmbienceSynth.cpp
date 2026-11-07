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

void ULyudAmbienceSynth::Footstep(float Strength, float Pan)
{
	SynthCommand([this, Strength, Pan]()
	{
		if (Bed)
		{
			Bed->Footstep(Strength, Pan);
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
	Rate = SampleRate;
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
	// muted (lyud.Sound 0, vacuum, volume 0): the mix multiplies everything by Master, so after a second of
	// silence stop rendering at all; on unmute start a fresh bed, as at world start, so nothing stale (grains,
	// strikes, filter state frozen mid-way) plays out
	if (P.Master <= 0.f)
	{
		MutedFrames = FMath::Min(MutedFrames + Frames, 1 << 30);
	}
	else
	{
		if (bAsleep && Bed)
		{
			Bed = MakePimpl<LyudAudio::FAmbience>();
			Bed->Init(float(Rate), 7);
		}
		MutedFrames = 0;
		bAsleep = false;
	}
	if (Bed && MutedFrames > Rate)
	{
		if (!bAsleep)
		{
			bAsleep = true;
			FScopeLock Guard(&Lock);
			FMemory::Memzero(Rms, sizeof(Rms));
		}
	}
	else if (Bed)
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
