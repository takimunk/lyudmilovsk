#include "BirdSound.h"

#include "Sound/SoundWaveProcedural.h"

namespace
{
	float Noise(uint32& S)
	{
		S = S * 1664525u + 1013904223u;
		return ((S >> 9) & 0x7fff) / 16383.5f - 1.f;
	}
}

USoundWaveProcedural* FBirdSound::Init(UObject* Outer, double MassKg, bool bClap, bool bSinging)
{
	Mass = FMath::Max(MassKg, 0.005);
	bClaps = bClap;
	bSings = bSinging;
	Wave = NewObject<USoundWaveProcedural>(Outer);
	Wave->SetSampleRate(Rate);
	Wave->NumChannels = 1;
	Wave->Duration = INDEFINITELY_LOOPING_DURATION;
	Wave->SoundGroup = SOUNDGROUP_Default;
	Wave->bLooping = false;
	Wave->bCanProcessAsync = true;
	return Wave;
}

void FBirdSound::Beat(float Strength, bool bTakeoff)
{
	// a small bird's downstroke is short and high, a big one's long and low
	BeatLen = FMath::Clamp(0.05f * (float)FMath::Pow(Mass / 0.03, 0.3), 0.04f, 0.35f);
	BeatAmp = FMath::Clamp(0.12f + 0.5f * (float)FMath::Pow(Mass / 0.3, 0.35), 0.1f, 0.9f) * FMath::Clamp(Strength, 0.2f, 1.f);
	BeatT = 0.f;
	if (bTakeoff && bClaps)
	{
		ClapT = 0.f;
	}
}

void FBirdSound::Tick(float Dt, float Airspeed, float Gust, float Volume)
{
	if (!Wave)
	{
		return;
	}
	Speed = Airspeed;
	GustNow = Gust;
	Vol = Volume;
	// keep about 80 ms queued ahead of the device
	const int32 Have = Wave->GetAvailableAudioByteCount() / (int32)sizeof(int16);
	const int32 Want = (int32)(Rate * 0.08f) - Have;
	if (Want > 0)
	{
		Generate(FMath::Min(Want + (int32)(Dt * Rate), Rate / 4));
	}
}

void FBirdSound::Generate(int32 Samples)
{
	TArray<int16> Pcm;
	Pcm.SetNumUninitialized(Samples);
	const float Dt = 1.f / Rate;
	// the rush of air: noise through a low-pass that opens with speed, and a rumble under it
	const float V = FMath::Max(Speed, 0.f);
	const float WindAmp = bAirRush ? FMath::Clamp(FMath::Square(V / 22.f), 0.f, 1.f) * 0.55f + 0.02f : 0.f;
	const float Fc = FMath::Min(250.f + 140.f * V, 5000.f);
	const float A1 = 1.f - FMath::Exp(-2.f * PI * Fc * Dt);
	const float ARumble = 1.f - FMath::Exp(-2.f * PI * 90.f * Dt);
	// the beats: band noise around a pitch that falls with the bird's size
	const float F0 = FMath::Clamp(900.f * (float)FMath::Pow(0.03 / Mass, 0.3), 200.f, 2200.f);
	const float Q = 0.35f;
	const float Fb = 2.f * FMath::Sin(PI * F0 * Dt);
	for (int32 I = 0; I < Samples; I++)
	{
		Clock += Dt;
		const float Gusting = 0.8f + 0.2f * FMath::Sin((float)Clock * 1.7f) * GustNow;
		const float N = Noise(Seed);
		Lp1 += A1 * (N - Lp1);
		Lp2 += A1 * (Lp1 - Lp2);
		Rumble += ARumble * (N - Rumble);
		float Out = (Lp2 * 1.6f + Rumble * 2.5f * FMath::Clamp(V / 20.f, 0.f, 1.f)) * WindAmp * Gusting;
		if (BeatT >= 0.f)
		{
			// state-variable band-pass, enveloped: a quick swell and a fall
			const float X = BeatT / BeatLen;
			const float Env = X < 0.2f ? X / 0.2f : FMath::Max(0.f, 1.f - (X - 0.2f) / 0.8f);
			const float Hp = N - Bp2 - Q * Bp1;
			Bp1 += Fb * Hp;
			Bp2 += Fb * Bp1;
			Out += Bp1 * BeatAmp * Env * 1.4f;
			if (bSings)
			{
				// the mute swan's wings: a throbbing whistle on every stroke
				SongPhase += 2.f * PI * (1150.f + 120.f * Env) * Dt;
				Out += FMath::Sin(SongPhase) * 0.25f * Env * BeatAmp;
			}
			BeatT += Dt;
			if (BeatT > BeatLen)
			{
				BeatT = -1.f;
			}
		}
		if (ClapT >= 0.f)
		{
			// the pigeon's take-off clap: wings struck together over the back
			const float Env = FMath::Exp(-ClapT * 180.f);
			Out += N * Env * 0.9f;
			ClapT += Dt;
			if (ClapT > 0.05f)
			{
				ClapT = -1.f;
			}
		}
		Pcm[I] = (int16)FMath::Clamp(Out * Vol * 26000.f, -32767.f, 32767.f);
	}
	Wave->QueueAudio((const uint8*)Pcm.GetData(), Samples * sizeof(int16));
}
