#include "Audio/LyudWindAudio.h"

#include "AmbienceDSP.h"

ULyudWindAudioComponent::ULyudWindAudioComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 2;
	bAllowSpatialization = false;
}

ULyudWindAudioComponent::~ULyudWindAudioComponent() = default;

void ULyudWindAudioComponent::SetWind(float WindSpeed, float Gustiness, float YawToWindDeg, float AirSpeed)
{
	Speed.store(FMath::Max(WindSpeed, 0.f));
	Gust.store(Gustiness);
	Pan.store(FMath::Sin(FMath::DegreesToRadians(YawToWindDeg)));
	Air.store(FMath::Max(AirSpeed, 0.f));
}

bool ULyudWindAudioComponent::Init(int32& SampleRate)
{
	NumChannels = 2;
	Voice = MakePimpl<LyudAudio::FWindVoice>();
	Voice->Init(float(SampleRate), uint32(GetUniqueID()));
	return true;
}

int32 ULyudWindAudioComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	const int32 Frames = NumSamples / 2;
	Left.SetNumZeroed(Frames, EAllowShrinking::No);
	Right.SetNumZeroed(Frames, EAllowShrinking::No);
	FMemory::Memzero(Left.GetData(), Frames * sizeof(float));
	FMemory::Memzero(Right.GetData(), Frames * sizeof(float));
	if (Voice)
	{
		Voice->Render(Left.GetData(), Right.GetData(), Frames, Speed.load(), Gust.load(), Pan.load(), Air.load(), Gain);
	}
	for (int32 I = 0; I < Frames; I++)
	{
		OutAudio[2 * I] = Left[I];
		OutAudio[2 * I + 1] = Right[I];
	}
	return NumSamples;
}
