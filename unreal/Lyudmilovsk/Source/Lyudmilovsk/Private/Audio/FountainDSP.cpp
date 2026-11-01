#include "FountainDSP.h"

#include <algorithm>
#include <cmath>

namespace LyudAudio
{
	void FFountainVoice::Init(float SampleRate, uint32_t Seed)
	{
		Rate = SampleRate;
		R = FRng(Seed);
		Waver = FDrift(Seed + 51);
		Flutter = FDrift(Seed + 52);
		BurbleDrift = FDrift(Seed + 53);
		Wash.Set(1700.f, 0.45f, Rate);
		Hiss.Set(5200.f, 0.7f, Rate);
		Burble.Set(320.f, 0.9f, Rate);
		SFlow.Init(0.9f, Rate, 0.f);
		NumBubbles = NumTicks = 0;
	}

	void FFountainVoice::Render(float* Out, int Num, float Flow, float Gain)
	{
		double Energy = 0.0;
		// drops per second hitting the water: the veil and the jet's crown together
		const float BubbleRate = 260.f, TickRate = 420.f;
		for (int I = 0; I < Num; I++)
		{
			const float F = SFlow.Step(std::min(std::max(Flow, 0.f), 1.f));
			if (F < 1e-4f && NumBubbles == 0 && NumTicks == 0)
			{
				continue;
			}
			// the veil wavers (slow) and flutters (fast): the wash breathes with it
			const float W = 0.78f + 0.16f * Waver.Step(0.7f, Rate) + 0.06f * Flutter.Step(9.f, Rate);
			Wash.Process(Pink.Process(R.White()));
			Hiss.Process(R.White());
			Burble.Process(Brown.Process(R.White()) * 6.f);
			float V = F * W * (0.55f * Wash.Band + 0.12f * Hiss.Band)
				+ F * 0.22f * (0.7f + 0.3f * BurbleDrift.Step(2.3f, Rate)) * Burble.Band;

			// new bubbles and ticks, Poisson in time
			if (F > 0.01f && R.Uniform() < BubbleRate * F / Rate && NumBubbles < MaxBubbles)
			{
				FBubble& B = Bubbles[NumBubbles++];
				// radius log-uniform 0.9 .. 6 mm; bigger bubbles are rarer but louder
				const float Rad = 0.0009f * std::pow(6.6f, R.Uniform() * R.Uniform() * 1.4f);
				B.Freq = std::min(3.26f / Rad, 5200.f);
				B.Phase = 0.f;
				B.Amp = (0.02f + 0.05f * R.Uniform()) * std::sqrt(Rad / 0.003f);
				// rings for ~10-30 cycles
				const float Tau = (10.f + 20.f * R.Uniform()) / B.Freq;
				B.Decay = std::exp(-1.f / (Tau * Rate));
				// the pitch rises a few percent over its life as the bubble shrinks and leaves the surface
				B.Rise = std::exp(std::log(1.f + 0.06f * R.Uniform()) / (Tau * Rate * 3.f));
			}
			if (F > 0.01f && R.Uniform() < TickRate * F / Rate && NumTicks < MaxTicks)
			{
				FTick& T = Ticks[NumTicks++];
				T.F = FSvf();
				T.F.Set(1800.f + 5000.f * R.Uniform(), 1.2f, Rate);
				T.Amp = 0.05f + 0.1f * R.Uniform();
				T.Decay = std::exp(-1.f / ((0.0015f + 0.004f * R.Uniform()) * Rate));
			}
			for (int K = 0; K < NumBubbles;)
			{
				FBubble& B = Bubbles[K];
				B.Phase += B.Freq / Rate;
				B.Phase -= std::floor(B.Phase);
				B.Freq *= B.Rise;
				V += B.Amp * std::sin(2.f * Pi * B.Phase);
				B.Amp *= B.Decay;
				if (B.Amp < 1e-4f)
				{
					B = Bubbles[--NumBubbles];
					continue;
				}
				K++;
			}
			for (int K = 0; K < NumTicks;)
			{
				FTick& T = Ticks[K];
				T.F.Process(R.White());
				V += T.Amp * T.F.Band;
				T.Amp *= T.Decay;
				if (T.Amp < 1e-4f)
				{
					T = Ticks[--NumTicks];
					continue;
				}
				K++;
			}
			V *= Gain;
			Out[I] += V;
			Energy += double(V) * V;
		}
		Rms = Num > 0 ? float(std::sqrt(Energy / Num)) : 0.f;
	}
}
