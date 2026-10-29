#include "AmbienceDSP.h"

#include <algorithm>
#include <cmath>

namespace LyudAudio
{
	namespace
	{
		// own names: the unity build may put this file next to AmbienceDSP.cpp's anonymous helpers
		float WClamp(float V, float A, float B) { return std::min(std::max(V, A), B); }
		float WSmooth(float A, float B, float X)
		{
			const float T = WClamp((X - A) / (B - A), 0.f, 1.f);
			return T * T * (3.f - 2.f * T);
		}
		constexpr int WBlock = 32;
		/** The fog signal's minute: two blasts of 2.5 s, 5 s apart (start, length) in seconds. */
		constexpr float SignalPeriod = 60.f;
		constexpr float Blasts[2][2] = { { 0.f, 2.5f }, { 5.f, 2.5f } };
		/** Footstep kinds. */
		enum EStep { DryEarth = 0, WetEarth, Puddle, Crunch, Fluff, Squeak, Slush, IceStep };
	}

	void FWeatherVoices::Init(float SampleRate, uint32_t Seed)
	{
		Rate = SampleRate;
		R = FRng(Seed * 13 + 5);
		Pings.reserve(256);
		StepPool.resize(3);
		Chirps.resize(12);
		Eaves.resize(6);
		for (size_t I = 0; I < Eaves.size(); I++)
		{
			FEave& E = Eaves[I];
			// each drip point has its own beat: a slow eave and a quick icicle; into a puddle, onto a tin sill or snow
			E.Period = 0.45f + 1.8f * R.Uniform();
			E.Clock = R.Uniform() * E.Period;
			E.Pan = R.White() * 0.85f;
			E.Gain = 0.4f + 0.6f * R.Uniform();
			E.Kind = int(I % 3);
		}
		const float BlockRate = SampleRate / WBlock;
		SBliz.Init(2.f, BlockRate);
		SHush.Init(6.f, BlockRate);
		SDrip.Init(5.f, BlockRate);
		SMelt.Init(5.f, BlockRate);
		SGlaze.Init(3.f, BlockRate);
		SIce.Init(4.f, BlockRate);
		SSignal.Init(6.f, BlockRate);
		SGrind.Init(3.f, BlockRate);
		BlizLowL.Set(900.f, 0.6f, Rate); BlizLowR.Set(950.f, 0.6f, Rate);
		HushL.Set(2200.f, 0.6f, Rate); HushR.Set(2200.f, 0.6f, Rate);
		BrashF.Set(5200.f, 2.f, Rate);
		Echo.assign(size_t(SampleRate * 2.f), 0.f);
		EchoAt = 0;
	}

	void FWeatherVoices::AddPing(int Kind, float Amp, float Pan, float Freq, float DecayS, float Q, float Glide, int Wait, bool bNear, int Voice)
	{
		if (Pings.size() >= 220)
		{
			return;
		}
		FPing G;
		G.Kind = Kind;
		G.Wait = Wait + int(R.Uniform() * WBlock);
		G.Amp = Amp;
		G.Pan = Pan;
		G.Freq = Freq;
		G.Glide = Glide;
		G.bNear = bNear;
		G.Voice = Voice;
		G.Decay = std::exp(-1.f / std::max(DecayS * Rate, 1.f));
		if (Kind == 1)
		{
			G.F.Set(Freq, Q, Rate);
		}
		Pings.push_back(G);
	}

	void FWeatherVoices::RenderPings(float* L, float* R2, int Num, bool bNear, double Energy[NumVoices])
	{
		for (size_t K = 0; K < Pings.size();)
		{
			FPing& G = Pings[K];
			if (G.bNear != bNear)
			{
				K++;
				continue;
			}
			double E = 0.0;
			bool bDead = false;
			for (int I = 0; I < Num; I++)
			{
				if (G.Wait > 0)
				{
					G.Wait--;
					continue;
				}
				float V;
				if (G.Kind == 0)
				{
					// a bubble's pitch rises as it shrinks (Glide > 1); glass and ice ring steady
					G.Phase += G.Freq / Rate;
					G.Phase -= std::floor(G.Phase);
					G.Freq *= G.Glide;
					V = G.Amp * std::sin(2.f * Pi * G.Phase);
				}
				else
				{
					G.F.Process(R.White());
					V = G.Amp * G.F.Band;
				}
				float A = 0.f, B = 0.f;
				PanTo(V, G.Pan, 1.f, A, B);
				L[I] += A;
				R2[I] += B;
				E += double(A) * A + double(B) * B;
				G.Amp *= G.Decay;
				if (G.Amp < 1e-5f)
				{
					bDead = true;
					break;
				}
			}
			Energy[G.Voice] += E;
			if (bDead)
			{
				G = Pings.back();
				Pings.pop_back();
				continue;
			}
			K++;
		}
	}

	void FWeatherVoices::Spawn(const FAmbienceParams& P, int Num)
	{
		// expected count of events in this buffer at PerSec, drawn so a fraction carries over as a chance
		auto Count = [this, Num](float PerSec)
		{
			const float Expected = std::max(PerSec, 0.f) * Num / Rate;
			int N = int(Expected);
			if (R.Uniform() < Expected - N)
			{
				N++;
			}
			return N;
		};
		auto At = [this, Num]() { return int(R.Uniform() * std::max(Num - WBlock, 1)); };
		const float Wind = std::max(P.WindSpeed, 0.f);
		const float Snow = WClamp(P.Snow, 0.f, 1.f);
		const float Water = std::max(P.Rain, 0.f) * (1.f - 0.92f * Snow);
		const float Out = 1.f - WClamp(P.Enclosure, 0.f, 1.f);
		const float Roof = WClamp(P.RainRoof, 0.f, 1.f);

		// hail: hard knocks, lower and louder on a roof, a few big stones among many pellets
		const float Hail = WClamp(P.Hail, 0.f, 1.f) * std::min(std::max(P.Rain, 0.f) / 2.f + 0.3f, 1.5f);
		for (int K = Count(Hail * 260.f); K > 0; K--)
		{
			const float Big = std::pow(R.Uniform(), 4.f);
			if (R.Uniform() < 0.4f + 0.5f * Roof)
			{
				AddPing(1, 0.05f * (0.3f + 1.6f * Big), R.White() * 0.7f, 700.f + 900.f * R.Uniform(), 0.02f + 0.03f * Big, 5.f, 1.f, At(), false, Rain);
			}
			else
			{
				AddPing(1, 0.035f * (0.25f + 1.5f * Big), R.White(), 2200.f + 3000.f * R.Uniform(), 0.008f + 0.012f * Big, 7.f, 1.f, At(), false, Rain);
			}
		}

		// an umbrella: every drop on the taut cloth a little "pock" right over the head (~66 drops/s/m2 per mm/h of
		// 2 mm drops); fine drizzle barely taps; snow only pats; and the trickle off the rim around
		const float Um = WClamp(P.Umbrella, 0.f, 1.f) * Out;
		if (Um > 0.01f)
		{
			const float Drops = std::min(66.f * Water * (1.f - 0.85f * WClamp(P.Drizzle, 0.f, 1.f)), 420.f) + 8.f * std::max(P.Rain, 0.f) * Snow;
			for (int K = Count(Um * Drops); K > 0; K--)
			{
				const float Big = std::pow(R.Uniform(), 3.f);
				const float Soft = Snow > 0.5f ? 0.25f : 1.f;
				AddPing(1, Soft * 0.16f * (0.35f + 1.4f * Big), R.White() * 0.45f, 380.f + 320.f * R.Uniform() + 200.f * (1.f - Big), 0.018f + 0.02f * Big, 2.5f, 1.f, At(), true, Rain);
				if (Snow < 0.5f && R.Uniform() < 0.5f)
				{
					AddPing(1, 0.012f * (0.4f + Big), R.White() * 0.45f, 3500.f + 2500.f * R.Uniform(), 0.003f, 3.f, 1.f, At(), true, Rain);
				}
			}
			for (int K = Count(Um * std::min(Water * 6.f, 18.f)); K > 0; K--)
			{
				const float Side = R.Uniform() < 0.5f ? -1.f : 1.f;
				AddPing(0, 0.02f * (0.4f + R.Uniform()), Side * (0.55f + 0.35f * R.Uniform()), 900.f + 1300.f * R.Uniform(), 0.015f + 0.02f * R.Uniform(), 0.f, 1.00004f, At(), true, Rain);
			}
		}

		// rain on the window glass when indoors: sharp little ticks, more when the wind drives it
		const float Win = WClamp(P.Windows, 0.f, 1.f) * WClamp(P.Enclosure, 0.f, 1.f);
		if (Win > 0.01f && Water > 0.05f)
		{
			for (int K = Count(Win * std::min(25.f * Water, 140.f) * (1.f + Wind / 10.f)); K > 0; K--)
			{
				AddPing(1, 0.01f * (0.3f + 1.5f * std::pow(R.Uniform(), 4.f)), R.White() * 0.8f, 2600.f + 2800.f * R.Uniform(), 0.006f + 0.006f * R.Uniform(), 10.f, 1.f, At(), true, Rain);
			}
		}

		// drips off the trees (after rain, in fog): a patter onto leaves, now and then a plip into a puddle
		const float Drip = SDrip.Y;
		for (int K = Count(Drip * 22.f); K > 0; K--)
		{
			const float Big = std::pow(R.Uniform(), 3.f);
			if (R.Uniform() < 0.35f)
			{
				AddPing(0, 0.04f * (0.3f + Big), R.White() * 0.9f, 1100.f + 1600.f * R.Uniform(), 0.02f + 0.02f * R.Uniform(), 0.f, 1.00003f, At(), false, Drips);
			}
			else
			{
				AddPing(1, 0.055f * (0.3f + Big), R.White() * 0.95f, 1500.f + 2500.f * R.Uniform(), 0.01f + 0.01f * R.Uniform(), 2.f, 1.f, At(), false, Drips);
			}
		}

		// a thaw: the eaves and icicles each drip on their own beat, quicker the faster it melts
		const float Melt = SMelt.Y;
		const int Active = int(std::ceil(Melt * Eaves.size()));
		for (int I = 0; I < Active; I++)
		{
			FEave& E = Eaves[size_t(I)];
			E.Clock += Num / Rate * (0.5f + Melt);
			if (E.Clock < E.Period)
			{
				continue;
			}
			// the beat wanders a little: drops gather at their own pace
			E.Clock -= E.Period * (0.85f + 0.3f * R.Uniform());
			const float Fade = WClamp(Melt * Eaves.size() - I, 0.f, 1.f) * E.Gain;
			if (E.Kind == 0)
			{
				AddPing(0, 0.05f * Fade, E.Pan, 700.f + 500.f * R.Uniform(), 0.03f + 0.02f * R.Uniform(), 0.f, 1.00005f, At(), false, Drips);
			}
			else if (E.Kind == 1)
			{
				AddPing(1, 0.04f * Fade, E.Pan, 2400.f + 600.f * R.Uniform(), 0.05f, 18.f, 1.f, At(), false, Drips);
			}
			else
			{
				AddPing(1, 0.03f * Fade, E.Pan, 500.f + 300.f * R.Uniform(), 0.015f, 1.5f, 1.f, At(), false, Drips);
			}
		}

		// iced twigs knocking in the wind: little clusters of glassy clinks
		const float Glaze = SGlaze.Y * WSmooth(1.5f, 9.f, Wind);
		for (int K = Count(Glaze * (2.f + 0.5f * Wind * Wind)); K > 0; K--)
		{
			const float Pan = R.White() * 0.9f;
			const int Wait = At();
			const int N = 1 + int(R.Uniform() * 4.f);
			for (int C = 0; C < N; C++)
			{
				AddPing(0, 0.012f * (0.4f + R.Uniform()), Pan + 0.1f * R.White(), 3200.f + 4200.f * R.Uniform(), 0.03f + 0.05f * R.Uniform(), 0.f, 1.f, Wait + int(Rate * 0.03f * C * (0.5f + R.Uniform())), false, Drips);
			}
		}

		// sea ice: singing chirps (thin ice sings most), booms (thick ice cracking as the air cools)
		const float Ice = SIce.Y;
		if (Ice > 0.01f)
		{
			const float Cooling = std::max(-P.IceTempRate, 0.f);
			const float Thin = WSmooth(30.f, 5.f, P.IceCm);
			const float PerMin = Ice * (2.f + 18.f * std::min(Cooling, 2.f)) * (0.4f + 0.6f * Thin);
			for (int K = Count(PerMin / 60.f); K > 0; K--)
			{
				bool bBoom = R.Uniform() < 0.25f + 0.5f * (1.f - Thin);
				const float Dist = std::max(P.IceDistance, 5.f) + 300.f * R.Uniform() * R.Uniform();
				const float Pan = WClamp(P.IcePan + 0.6f * R.White(), -1.f, 1.f);
				// farther cracks: the dispersion stretches the chirp longer and the air takes its top
				const float Spread = 0.15f + Dist / 900.f;
				// a boom rings on as chirps running off along the sheet; a chirp comes again round the floes ("pew - pew")
				int Left = 1 + int(R.Uniform() * 3.f) + (bBoom ? 1 : 0);
				float Wait = R.Uniform() * Num / Rate;
				for (FChirp& C : Chirps)
				{
					if (C.bLive)
					{
						continue;
					}
					if (Left-- <= 0)
					{
						break;
					}
					C = FChirp();
					C.bLive = true;
					C.Kind = bBoom ? 1 : 0;
					C.Wait = Wait;
					C.Pan = Pan;
					C.Amp = (bBoom ? 0.25f : 0.09f) / (1.f + Dist / 120.f) * (0.5f + R.Uniform()) * (bBoom || Wait < 0.05f ? 1.f : 0.55f);
					C.F0 = bBoom ? 60.f + 50.f * R.Uniform() : 2500.f + 3500.f * R.Uniform();
					C.F1 = bBoom ? 35.f : 180.f + 250.f * R.Uniform();
					C.Dur = bBoom ? 0.9f + 0.6f * R.Uniform() : std::min(Spread * (0.8f + 0.5f * R.Uniform()), 1.2f);
					C.F.Set(std::min(9000.f / (1.f + Dist / 400.f) + 600.f, 9000.f), 0.7f, Rate);
					Wait += bBoom ? 0.15f + 0.2f * R.Uniform() : 0.12f + 0.25f * R.Uniform();
					bBoom = false;
				}
			}
		}
	}

	void FWeatherVoices::RenderOutside(float* L, float* R2, int Num, const FAmbienceParams& P, double Energy[NumVoices])
	{
		const float BlockRate = Rate / WBlock;
		const float Wind = std::max(P.WindSpeed, 0.f);
		const float Snow = WClamp(P.Snow, 0.f, 1.f);
		const float SnowFall = std::max(P.Rain, 0.f) * Snow;
		const float Fresh = WSmooth(1.f, 10.f, P.SnowDepthCm) * WSmooth(260.f, 120.f, P.SnowDensity);

		// the slow things first, once per buffer
		SDrip.Step(WClamp(P.Drip, 0.f, 1.f));
		SMelt.Step(WClamp(P.Melt, 0.f, 1.f) * WSmooth(0.5f, 3.f, P.SnowDepthCm + 4.f * P.Glaze));
		SGlaze.Step(WClamp(P.Glaze, 0.f, 1.f));
		SIce.Step(WClamp(P.SeaIce, 0.f, 1.f) * WSmooth(1.f, 4.f, P.IceCm));
		Spawn(P, Num);

		// blizzard: snow driven by the wind hisses on everything; in a gale fresh snow is lifted off the ground too
		const float Falling = std::min(SnowFall / 1.5f, 1.2f) * WSmooth(4.f, 14.f, Wind);
		const float Lifted = WSmooth(7.f, 15.f, Wind) * Fresh * 0.7f;
		float Bliz = SBliz.Y;
		double EBliz = 0.0;
		// sea ice grinding: floes knocking and creaking in a swell when the sea is only partly frozen; brash clinks
		const float Cover = WClamp(P.SeaIce, 0.f, 1.f);
		const float GrindTarget = Cover * (1.f - Cover) * 4.f * WSmooth(0.03f, 0.4f, P.IceSwell + 0.02f * Wind) / (1.f + std::max(P.IceDistance, 0.f) / 80.f);
		float Grind = SGrind.Y, Swell = 0.f, StickHz = 20.f;
		double EIce = 0.0;
		// the fog signal
		const float SigDist = std::max(P.FogSignalDistance, 50.f);
		const float SigLoud = 0.6f / (1.f + std::max(SigDist, 300.f) / 700.f);
		float Sig = SSignal.Y;
		double ESig = 0.0;
		for (int I = 0; I < Num; I++)
		{
			if ((I & (WBlock - 1)) == 0)
			{
				Bliz = SBliz.Step(std::max(Falling, Lifted) + 0.04f * std::min(SnowFall, 1.f));
				const float Gust = 0.55f + 0.45f * BlizGust.Step(0.35f, BlockRate) + 0.15f * BlizFlutter.Step(6.f, BlockRate);
				Bliz *= WClamp(Gust, 0.1f, 1.4f);
				BlizL.Set(4200.f + 220.f * Wind, 0.5f, Rate);
				BlizR.Set(4500.f + 210.f * Wind, 0.5f, Rate);
				Grind = SGrind.Step(GrindTarget);
				Swell = 0.5f + 0.5f * GrindSwell.Step(0.25f, BlockRate);
				StickHz = 14.f + 22.f * (0.5f + 0.5f * GrindRate.Step(0.6f, BlockRate));
				GrindF.Set(280.f + 260.f * Swell, 3.5f, Rate);
				Sig = SSignal.Step(WClamp(P.FogSignal, 0.f, 1.f));
				SigF.Set(std::min(2600.f / (1.f + SigDist / 1500.f) + 260.f, 3000.f), 0.6f, Rate);
			}
			float OutL = 0.f, OutR = 0.f;
			if (Bliz > 1e-4f)
			{
				const float WL = R.White(), WR = R.White();
				BlizL.Process(WL); BlizR.Process(WR);
				BlizLowL.Process(WL); BlizLowR.Process(WR);
				const float A = 0.14f * Bliz * (BlizL.Band + 0.25f * BlizLowL.Band);
				const float B = 0.14f * Bliz * (BlizR.Band + 0.25f * BlizLowR.Band);
				OutL += A * P.Gain[LyudAudio::Snow];
				OutR += B * P.Gain[LyudAudio::Snow];
				EBliz += double(A) * A + double(B) * B;
			}
			if (Grind > 1e-4f)
			{
				// stick-slip: a train of little jolts whose rate wanders, swelling with the waves, through the floe's body
				GrindPhase += StickHz / Rate;
				float Jolt = 0.f;
				if (GrindPhase >= 1.f)
				{
					GrindPhase -= 1.f;
					Jolt = (0.5f + R.Uniform()) * (R.Uniform() < 0.8f ? 1.f : 0.f);
				}
				GrindF.Process(Jolt * 4.f + 0.15f * R.White());
				BrashF.Process(R.White());
				const float V = Grind * Swell * Swell * (0.4f * GrindF.Band + 0.03f * BrashF.Band * Swell);
				float A = 0.f, B = 0.f;
				PanTo(V, P.IcePan, 0.7f, A, B);
				OutL += A * P.Gain[Ice];
				OutR += B * P.Gain[Ice];
				EIce += double(A) * A + double(B) * B;
			}
			// the diaphone's two blasts a minute: a deep reed tone that ends in the grunt (pitch dropping as the air runs out)
			SigClock += 1.f / Rate;
			if (SigClock >= SignalPeriod)
			{
				SigClock -= SignalPeriod;
			}
			float Tone = 0.f;
			if (Sig > 1e-3f)
			{
				for (const auto& Bl : Blasts)
				{
					const float T = SigClock - Bl[0];
					if (T < 0.f || T > Bl[1] + 0.6f)
					{
						continue;
					}
					const float Body = WSmooth(0.f, 0.25f, T) * (T < Bl[1] ? 1.f : 0.f);
					const float Grunt = T >= Bl[1] ? std::exp(-(T - Bl[1]) / 0.18f) : 0.f;
					const float F = T < Bl[1] ? 180.f * (0.97f + 0.03f * WSmooth(0.f, 0.4f, T)) : 180.f * (1.f - 0.45f * std::min((T - Bl[1]) / 0.5f, 1.f));
					SigPhase += F / Rate;
					SigPhase -= std::floor(SigPhase);
					// a reed's buzz: a saw rounded off a little
					const float Saw = 2.f * SigPhase - 1.f;
					Tone = (Body + 0.9f * Grunt) * (Saw - Saw * Saw * Saw / 3.f);
				}
			}
			SigF.Process(Tone);
			// the coast throws it back twice
			const int Size = int(Echo.size());
			const int D1 = int(Rate * 0.7f), D2 = int(Rate * 1.6f);
			const float Back = 0.35f * Echo[size_t((EchoAt - D1 + Size) % Size)] + 0.2f * Echo[size_t((EchoAt - D2 + Size) % Size)];
			const float Direct = Sig * SigLoud * SigF.Low;
			Echo[size_t(EchoAt)] = Direct;
			EchoAt = (EchoAt + 1) % Size;
			if (std::fabs(Direct) + std::fabs(Back) > 1e-6f)
			{
				float A = 0.f, B = 0.f;
				PanTo(Direct, P.FogSignalPan, 0.8f, A, B);
				PanTo(Back, -0.3f * P.FogSignalPan, 0.4f, A, B);
				OutL += A * P.Gain[Signal];
				OutR += B * P.Gain[Signal];
				ESig += double(A) * A + double(B) * B;
			}
			L[I] += OutL;
			R2[I] += OutR;
		}
		Energy[LyudAudio::Snow] += EBliz;
		Energy[Ice] += EIce;
		Energy[Signal] += ESig;

		// the chirps and booms of the sea ice
		for (FChirp& C : Chirps)
		{
			if (!C.bLive)
			{
				continue;
			}
			double E = 0.0;
			for (int I = 0; I < Num; I++)
			{
				if (C.Wait > 0.f)
				{
					C.Wait -= 1.f / Rate;
					continue;
				}
				const float X = C.T / C.Dur;
				// an exponential sweep down from F0 to F1: the high frequencies of the flexural wave come first
				const float F = C.F0 * std::pow(C.F1 / C.F0, std::min(X, 1.f));
				C.Phase += F / Rate;
				C.Phase -= std::floor(C.Phase);
				float V;
				if (C.Kind == 1)
				{
					const float Env = std::min(C.T / 0.01f, 1.f) * std::exp(-C.T / (0.35f * C.Dur));
					V = C.Amp * Env * (std::sin(2.f * Pi * C.Phase) + 0.6f * std::exp(-C.T / 0.04f) * R.White());
				}
				else
				{
					const float Env = std::min(C.T / 0.004f, 1.f) * std::exp(-3.f * X);
					V = C.Amp * Env * std::sin(2.f * Pi * C.Phase);
				}
				C.F.Process(V);
				float A = 0.f, B = 0.f;
				PanTo(C.F.Low * P.Gain[Ice], C.Pan, 0.8f, A, B);
				L[I] += A;
				R2[I] += B;
				E += double(A) * A + double(B) * B;
				C.T += 1.f / Rate;
				if (C.T > C.Dur * 1.6f)
				{
					C.bLive = false;
					break;
				}
			}
			Energy[Ice] += E;
		}
		RenderPings(L, R2, Num, false, Energy);
		(void)Snow;
	}

	void FWeatherVoices::Hush(float* L, float* R2, int Num, const FAmbienceParams& P)
	{
		// fresh soft snow absorbs the highs (and the echoes) of everything around; fog dulls the far things
		const float Fresh = WSmooth(1.f, 12.f, P.SnowDepthCm) * WSmooth(300.f, 110.f, P.SnowDensity);
		const float Target = std::max(0.8f * Fresh, 0.5f * WClamp(P.Fog, 0.f, 1.f));
		float M = SHush.Y;
		for (int I = 0; I < Num; I++)
		{
			if ((I & (WBlock - 1)) == 0)
			{
				M = SHush.Step(Target);
			}
			if (M < 1e-3f)
			{
				continue;
			}
			HushL.Process(L[I]);
			HushR.Process(R2[I]);
			const float G = 1.f - 0.3f * M;
			L[I] = G * (L[I] - 0.65f * M * HushL.High);
			R2[I] = G * (R2[I] - 0.65f * M * HushR.High);
		}
	}

	void FWeatherVoices::Footstep(float Strength, float Pan)
	{
		for (FStep& S : StepPool)
		{
			if (!S.bLive)
			{
				S = FStep();
				S.bLive = true;
				S.Kind = -1;    // decided by the ground at the next render
				S.Strength = WClamp(Strength, 0.2f, 2.f);
				S.Pan = Pan;
				return;
			}
		}
	}

	void FWeatherVoices::SpawnStepGrains(int Kind, float Strength, float Pan)
	{
		if (Kind == Puddle || Kind == Slush)
		{
			// water thrown up: bubbles and plips around the foot
			const int N = (Kind == Puddle ? 5 : 3) + int(R.Uniform() * 4.f);
			for (int K = 0; K < N; K++)
			{
				AddPing(0, 0.025f * Strength * (0.3f + R.Uniform()), Pan + 0.3f * R.White(), (Kind == Puddle ? 700.f : 450.f) + 1600.f * R.Uniform(), 0.015f + 0.025f * R.Uniform(), 0.f, 1.00006f, int(Rate * (0.02f + 0.18f * R.Uniform())), true, LyudAudio::Steps);
			}
		}
	}

	void FWeatherVoices::RenderNear(float* L, float* R2, int Num, const FAmbienceParams& P, double Energy[NumVoices])
	{
		for (FStep& S : StepPool)
		{
			if (!S.bLive)
			{
				continue;
			}
			if (S.Kind < 0)
			{
				// what the foot lands on
				float Snow = P.UnderfootSnowCm >= 0.f ? P.UnderfootSnowCm : P.SnowDepthCm;
				if (Snow < 1.5f && P.SnowDepthCm >= 1.5f && P.TempC < 0.5f)
				{
					// a cleared path in a snowy winter: the trodden crust left on it still crunches, thinner
					Snow = 1.5f;
					S.Strength *= 0.6f;
				}
				if (P.OnIce > 0.5f || (P.Glaze > 0.5f && Snow < 1.5f))
				{
					S.Kind = IceStep;
				}
				else if (Snow >= 1.5f)
				{
					if (P.Melt > 0.3f || P.TempC > 0.5f || P.Rain * (1.f - P.Snow) > 0.5f)
					{
						S.Kind = Slush;
					}
					else if (P.TempC < -8.f)
					{
						S.Kind = Squeak;
					}
					else
					{
						S.Kind = P.SnowDensity < 110.f && Snow > 5.f ? Fluff : Crunch;
					}
				}
				else if (P.Puddles > R.Uniform())
				{
					S.Kind = Puddle;
				}
				else
				{
					S.Kind = P.Wet > 0.35f ? WetEarth : DryEarth;
				}
				switch (S.Kind)
				{
				case Crunch: S.Len = 0.32f; S.Crunch.Set(1700.f + 600.f * R.Uniform(), 0.9f, Rate); break;
				case Fluff: S.Len = 0.3f; S.Crunch.Set(900.f + 300.f * R.Uniform(), 0.7f, Rate); break;
				case Squeak: S.Len = 0.34f; S.Crunch.Set(2000.f + 500.f * R.Uniform(), 1.f, Rate);
					// cold snow's squeak: the grains' friction rings at a couple of steady pitches
					S.Sq1.Set(1050.f + 250.f * R.Uniform(), 22.f, Rate); S.Sq2.Set(2150.f + 400.f * R.Uniform(), 26.f, Rate); break;
				case Slush: S.Len = 0.35f; S.Crunch.Set(650.f + 200.f * R.Uniform(), 0.8f, Rate); break;
				case Puddle: S.Len = 0.3f; S.Crunch.Set(1300.f + 500.f * R.Uniform(), 0.7f, Rate); break;
				case IceStep: S.Len = 0.18f; S.Crunch.Set(3200.f + 900.f * R.Uniform(), 8.f, Rate); break;
				case WetEarth: S.Len = 0.24f; S.Crunch.Set(700.f + 200.f * R.Uniform(), 0.7f, Rate); break;
				default: S.Len = 0.2f; S.Crunch.Set(1100.f + 400.f * R.Uniform(), 0.7f, Rate); break;
				}
				// the thud of the weight: low on soft snow and mud, higher on hard ground
				S.Body.Set(S.Kind == Fluff || S.Kind == Slush ? 160.f : (S.Kind == IceStep ? 600.f : 260.f), 0.8f, Rate);
				S.Hi.Set(4500.f, 0.6f, Rate);
				SpawnStepGrains(S.Kind, S.Strength, S.Pan);
			}
			double E = 0.0;
			for (int I = 0; I < Num; I++)
			{
				const float T = S.T;
				// the heel lands, then the foot rolls onto the toe
				const float Heel = T < 0.012f ? T / 0.012f : std::exp(-(T - 0.012f) / (0.22f * S.Len));
				const float Toe = T < 0.11f ? 0.f : (T < 0.13f ? (T - 0.11f) / 0.02f : std::exp(-(T - 0.13f) / (0.3f * S.Len)));
				const float Env = Heel + 0.6f * Toe;
				float V = 0.f;
				switch (S.Kind)
				{
				case Crunch:
				case Fluff:
				case Squeak:
				{
					// the snow's crystals break one after another: a dense crackle of tiny impulses
					const float PerSec = (S.Kind == Fluff ? 900.f : 2600.f) * Env;
					const float Imp = R.Uniform() < PerSec / Rate ? R.White() * (0.4f + R.Uniform()) : 0.f;
					S.Crunch.Process(Imp * 6.f);
					S.Body.Process(Imp * 4.f + 0.05f * R.White() * Env);
					V = 0.09f * S.Crunch.Band + 0.12f * Env * S.Body.Low;
					if (S.Kind == Fluff)
					{
						V = 1.4f * V + 0.1f * Env * S.Body.Band;
					}
					if (S.Kind == Squeak)
					{
						S.Sq1.Process(Imp * 3.f);
						S.Sq2.Process(Imp * 3.f);
						V += 0.035f * Env * (S.Sq1.Band + 0.7f * S.Sq2.Band);
					}
					break;
				}
				case Slush:
				case Puddle:
				{
					// water squeezed and thrown: soft broad noise, darker for slush
					S.Crunch.Process(R.White());
					S.Body.Process(R.White());
					V = Env * (0.07f * S.Crunch.Band + 0.08f * S.Body.Low);
					break;
				}
				case IceStep:
				{
					// a hard click and the sole's little scrape on the ice
					const float Click = T < 0.002f ? R.White() : 0.f;
					S.Crunch.Process(Click * 8.f);
					S.Hi.Process(R.White());
					V = 0.014f * S.Crunch.Band + 0.06f * Toe * S.Hi.High + 0.05f * Heel * S.Hi.High;
					break;
				}
				default:
				{
					// earth: a dull pad, a little grit when dry, a soft smack when wet
					S.Crunch.Process(R.White());
					S.Body.Process(R.White());
					V = Env * ((S.Kind == WetEarth ? 0.13f : 0.16f) * S.Crunch.Band + 0.25f * S.Body.Low);
					break;
				}
				}
				V *= S.Strength * P.Gain[LyudAudio::Steps];
				float A = 0.f, B = 0.f;
				PanTo(V, S.Pan, 0.5f, A, B);
				L[I] += A;
				R2[I] += B;
				E += double(A) * A + double(B) * B;
				S.T += 1.f / Rate;
				if (S.T > S.Len * 2.2f)
				{
					S.bLive = false;
					break;
				}
			}
			Energy[LyudAudio::Steps] += E;
		}
		RenderPings(L, R2, Num, true, Energy);
	}
}
